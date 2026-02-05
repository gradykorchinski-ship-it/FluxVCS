#include "flux/net/fetch.hpp"
#include "flux/core/repository.hpp"
#include "flux/core/commit.hpp"
#include "flux/core/tree.hpp"
#include "flux/core/blob.hpp"
#include "flux/storage/object_store.hpp"
#include "flux/storage/ref_store.hpp"
#include "flux/net/git_pack.hpp"
#include "flux/core/auth.hpp" // Added this include based on the instruction's implied change
#include <fmt/format.h>
#include <fstream>

namespace flux {

Fetcher::Fetcher(Repository& repo) : repo_(repo) {}

void Fetcher::set_credentials(const std::string& username, const std::string& password) {
    username_ = username;
    password_ = password;
}

void Fetcher::set_token(const std::string& token) {
    token_ = token;
}

Result<std::vector<GitRef>> Fetcher::list_remote_refs(const std::string& remote_url) {
    HttpTransport transport;
    if (!token_.empty()) {
        transport.set_token(token_);
    } else if (!username_.empty()) {
        transport.set_credentials(username_, password_);
    }
    
    auto response = transport.git_upload_pack_discovery(remote_url);
    
    // Update expiration if seen
    auto expiry = transport.get_last_github_token_expiry();
    if (!expiry.empty()) {
        AuthManager::instance().update_expiration_from_header(expiry);
    }
    
    if (!response) return flux::unexpected(response.error());
    
    auto discovery = GitProtocol::parse_ref_discovery(*response);
    if (!discovery) return flux::unexpected(discovery.error());
    
    return discovery->refs;
}

Result<void> Fetcher::fetch_all(const std::string& remote_url) {
    fmt::print("Discovering remote refs...\n");
    auto refs_result = list_remote_refs(remote_url);
    if (!refs_result) return flux::unexpected(refs_result.error());
    
    auto& refs = *refs_result;
    if (refs.empty()) return flux::unexpected("No refs found on remote");
    
    std::vector<ObjectId> wants;
    ObjectId remote_head_oid;
    for (const auto& ref : refs) {
        if (ref.name == "HEAD") remote_head_oid = ref.oid;
        if (ref.name.find("^{}") == std::string::npos) {
            wants.push_back(ref.oid);
        }
    }
    
    if (wants.empty()) return flux::unexpected("No objects to fetch");
    
    auto receive_result = receive_pack(remote_url, wants, {});
    if (!receive_result) return receive_result;

    fmt::print("Updating refs...\n");
    return update_refs(refs);
}

Result<void> Fetcher::fetch_refs(const std::string& remote_url, const std::vector<std::string>& /*ref_patterns*/) {
    return fetch_all(remote_url);
}

Result<void> Fetcher::receive_pack(const std::string& remote_url, const std::vector<ObjectId>& wants, const std::vector<ObjectId>& haves) {
    HttpTransport transport;
    if (!token_.empty()) {
        transport.set_token(token_);
    } else if (!username_.empty()) {
        transport.set_credentials(username_, password_);
    }
    
    GitProtocol::UploadPackRequest request;
    request.wants = wants;
    request.haves = haves;
    request.done = true;
    request.capabilities.side_band_64k = true;
    request.capabilities.ofs_delta = true;
    request.capabilities.agent = "FluxVCS/1.0";
    
    fmt::print("Receiving pack file...\n");
    
    auto response = transport.git_upload_pack(remote_url, GitProtocol::encode_upload_pack_request(request));
    
    // Update expiration if seen
    auto expiry = transport.get_last_github_token_expiry();
    if (!expiry.empty()) {
        AuthManager::instance().update_expiration_from_header(expiry);
    }
    
    if (!response) return flux::unexpected(response.error());
    
    std::vector<uint8_t> pack_data;
    size_t pos = 0;
    while (pos < response->size()) {
        auto pkt_result = PktLine::parse(response->substr(pos));
        if (!pkt_result) break;
        auto [pkt, consumed] = *pkt_result;
        pos += consumed;
        if (pkt.empty()) break;
        
        if (pkt.find("NAK\n") != std::string::npos) continue;

        if (!pkt.empty() && pkt[0] >= 1 && pkt[0] <= 3) {
            auto sb_result = SideBand::parse(pkt);
            if (sb_result && sb_result->channel == SideBand::Channel::PACK_DATA) {
                pack_data.insert(pack_data.end(), sb_result->data.begin(), sb_result->data.end());
            }
        } else {
             if (pkt.size() >= 4 && std::string_view(reinterpret_cast<const char*>(pkt.data()), 4) == "PACK") {
                 pack_data.insert(pack_data.end(), pkt.begin(), pkt.end());
             }
        }
    }
    
    if (pack_data.empty()) return flux::unexpected("No pack data received");
    return unpack_objects(pack_data);
}

Result<void> Fetcher::unpack_objects(std::span<const uint8_t> pack_data) {
    GitPackUnpacker unpacker(repo_);
    auto result = unpacker.unpack(pack_data);
    if (!result) return flux::unexpected(result.error());
    fmt::print("Unpacked {} objects\n", *result);
    return {};
}

Result<void> Fetcher::update_refs(const std::vector<GitRef>& remote_refs) {
    auto& ref_store = repo_.ref_store();
    ObjectId head_oid;
    std::string head_branch;

    for (const auto& ref : remote_refs) {
        if (ref.name == "HEAD") head_oid = ref.oid;
        if (ref.name.find("^{}") != std::string::npos) continue;
        
        std::string local_ref = ref.name.starts_with("refs/heads/") ? 
            "refs/remotes/origin/" + ref.name.substr(11) : ref.name;
        ref_store.write(local_ref, ref.oid);
    }

    // Identify and setup default branch
    if (head_oid.is_valid()) {
        for (const auto& ref : remote_refs) {
            if (ref.name.starts_with("refs/heads/") && ref.oid == head_oid) {
                head_branch = ref.name;
                break;
            }
        }
        if (head_branch.empty()) head_branch = "refs/heads/master";
        
        // Create local branch and set HEAD
        ref_store.write(head_branch, head_oid);
        ref_store.write("HEAD", head_oid); // For now, direct OID. Should be symbolic later.
        fmt::print("Set default branch: {}\n", head_branch);
    }
    return {};
}

Checkout::Checkout(Repository& repo) : repo_(repo) {}

Result<void> Checkout::checkout_head() {
    auto head_result = repo_.ref_store().read("HEAD");
    if (!head_result) return flux::unexpected(head_result.error());
    return checkout_commit(*head_result);
}

Result<void> Checkout::checkout_branch(const std::string& branch_name) {
    std::string ref_name = branch_name.starts_with("refs/") ? branch_name : "refs/heads/" + branch_name;
    auto commit_result = repo_.ref_store().read(ref_name);
    if (!commit_result) return flux::unexpected(commit_result.error());
    repo_.ref_store().write_symbolic("HEAD", ref_name);
    return checkout_commit(*commit_result);
}

Result<void> Checkout::checkout_commit(const ObjectId& commit_id) {
    auto commit_data = repo_.object_store().read(commit_id);
    if (!commit_data) return flux::unexpected(commit_data.error());
    auto commit_result = Commit::deserialize(*commit_data);
    if (!commit_result) return flux::unexpected(commit_result.error());
    return materialize_tree(commit_result->tree(), repo_.workdir());
}

Result<void> Checkout::materialize_tree(const ObjectId& tree_id, const Path& base_path) {
    auto tree_data = repo_.object_store().read(tree_id);
    if (!tree_data) return flux::unexpected(tree_data.error());
    auto tree_result = Tree::deserialize(*tree_data);
    if (!tree_result) return flux::unexpected(tree_result.error());
    
    std::filesystem::create_directories(base_path);
    for (const auto& entry : tree_result->entries()) {
        Path entry_path = base_path / entry.name;
        if (entry.is_directory()) {
            materialize_tree(entry.id, entry_path);
        } else {
            auto blob_data = repo_.object_store().read(entry.id);
            if (!blob_data) continue;
            
            std::ofstream file(entry_path, std::ios::binary);
            if (entry.id.algorithm() == HashAlgorithm::SHA1) {
                // Git blob is raw data
                file.write(reinterpret_cast<const char*>(blob_data->data()), blob_data->size());
            } else {
                auto blob_result = Blob::deserialize(*blob_data);
                if (!blob_result) continue;
                auto content = blob_result->reconstruct();
                file.write(reinterpret_cast<const char*>(content.data()), content.size());
            }
            std::filesystem::permissions(entry_path, static_cast<std::filesystem::perms>(entry.mode));
        }
    }
    return {};
}

} // namespace flux
