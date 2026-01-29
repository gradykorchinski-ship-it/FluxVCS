#include "flux/net/push.hpp"
#include "flux/core/repository.hpp"
#include "flux/core/commit.hpp"
#include "flux/core/tree.hpp"
#include "flux/storage/object_store.hpp"
#include "flux/storage/ref_store.hpp"
#include "flux/net/http_transport.hpp"
#include "flux/net/git_protocol.hpp"
#include "flux/net/git_pack.hpp"
#include "flux/core/auth.hpp"
#include "flux/util/hash.hpp" // Added based on usage and context
#include <fmt/format.h>
#include <set>
#include <queue>
#include <iostream>

namespace flux {

Pusher::Pusher(Repository& repo) : repo_(repo) {}

void Pusher::set_token(const std::string& token) {
    token_ = token;
}

Result<void> Pusher::push(const std::string& remote_url, const std::string& branch_name) {
    HttpTransport transport;
    if (!token_.empty()) {
        transport.set_token(token_);
    }

    fmt::print("Discovering remote refs...\n");
    auto discovery_res = transport.git_receive_pack_discovery(remote_url);
    if (!discovery_res) return flux::unexpected(discovery_res.error());

    auto discovery = GitProtocol::parse_ref_discovery(*discovery_res);
    if (!discovery) return flux::unexpected(discovery.error());

    // Update expiration if seen
    auto expiry = transport.get_last_github_token_expiry();
    if (!expiry.empty()) {
        AuthManager::instance().update_expiration_from_header(expiry);
    }

    // Find the remote OID for the target branch
    ObjectId remote_oid(repo_.hash_algorithm(), Bytes(Hash::hash_size(repo_.hash_algorithm()), 0)); // Zero-OID for new branch
    std::string full_ref = branch_name.starts_with("refs/heads/") ? branch_name : "refs/heads/" + branch_name;
    
    for (const auto& ref : discovery->refs) {
        if (ref.name == full_ref) {
            remote_oid = ref.oid;
            break;
        }
    }

    // Find local OID
    auto local_oid_res = repo_.ref_store().read(full_ref);
    if (!local_oid_res) {
        // Try without refs/heads/
        local_oid_res = repo_.ref_store().read(branch_name);
        if (!local_oid_res) return flux::unexpected("Local branch not found: " + branch_name);
    }
    ObjectId local_oid = *local_oid_res;

    if (local_oid == remote_oid) {
        fmt::print("Everything up-to-date\n");
        return {};
    }

    fmt::print("Calculating delta...\n");
    auto objects_res = collect_objects(local_oid, remote_oid);
    if (!objects_res) return flux::unexpected(objects_res.error());
    
    fmt::print("Committing {} objects to pack...\n", objects_res->size());
    GitPackPacker packer(repo_);
    auto pack_res = packer.create_pack(*objects_res);
    if (!pack_res) return flux::unexpected(pack_res.error());

    fmt::print("Pushing to {}...\n", remote_url);
    GitProtocol::ReceivePackRequest request;
    request.commands.push_back({remote_oid, local_oid, full_ref});
    request.capabilities = GitCapabilities();
    request.capabilities.report_status = true;
    request.capabilities.ofs_delta = true; // Support offsets in pack if server does
    request.pack_data = *pack_res;

    auto response_res = transport.git_receive_pack(remote_url, GitProtocol::encode_receive_pack_request(request));
    if (!response_res) return flux::unexpected(response_res.error());

    auto response = GitProtocol::parse_receive_pack_response(*response_res);
    if (!response) return flux::unexpected(response.error());

    if (response->unpack_status != "ok") {
        return flux::unexpected("Remote failed to unpack: " + response->unpack_status);
    }

    for (const auto& update : response->updates) {
        if (!update.success) {
            fmt::print(stderr, "Rejected {}: {}\n", update.ref_name, update.message);
            return flux::unexpected("Push rejected");
        }
        fmt::print("Updated {} ({} -> {})\n", update.ref_name, 
                  Hash::to_hex(remote_oid.hash()).substr(0, 8), 
                  Hash::to_hex(local_oid.hash()).substr(0, 8));
    }

    return {};
}

Result<std::vector<ObjectId>> Pusher::collect_objects(const ObjectId& local_oid, const ObjectId& remote_oid) {
    std::vector<ObjectId> result;
    std::set<ObjectId> seen;
    std::queue<ObjectId> queue;

    queue.push(local_oid);
    while (!queue.empty()) {
        ObjectId current = queue.front();
        queue.pop();

        if (seen.count(current) || current == remote_oid || !current.is_valid()) continue;
        seen.insert(current);

        auto data = repo_.objects().read(current);
        if (!data) continue;

        result.push_back(current);

        // Check if it's a commit to find tree/parents
        auto type = repo_.objects().get_type(current);
        if (type && *type == ObjectType::Commit) {
            auto commit = Commit::deserialize(*data);
            if (commit) {
                add_tree_objects(commit->tree(), result, seen);
                for (const auto& parent : commit->parents()) {
                    queue.push(parent);
                }
            }
        }
    }

    return result;
}

void Pusher::add_tree_objects(const ObjectId& tree_id, std::vector<ObjectId>& objects, std::set<ObjectId>& seen) {
    if (seen.count(tree_id)) return;
    seen.insert(tree_id);

    auto data = repo_.objects().read(tree_id);
    if (!data) return;

    objects.push_back(tree_id);

    auto tree = Tree::deserialize(*data);
    if (!tree) return;

    for (const auto& entry : tree->entries()) {
        if (seen.count(entry.id)) continue;
        
        if (entry.is_directory()) {
            add_tree_objects(entry.id, objects, seen);
        } else {
            seen.insert(entry.id);
            objects.push_back(entry.id);
        }
    }
}

} // namespace flux
