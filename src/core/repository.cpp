#include "flux/core/repository.hpp"
#include "flux/core/tree.hpp"
#include "flux/storage/object_store.hpp"
#include "flux/storage/ref_store.hpp"
#include "flux/storage/wal.hpp"
#include "flux/index/index.hpp"
#include "flux/util/filesystem.hpp"
#include "flux/core/commit.hpp"
#include <fmt/format.h>
#include <fstream>
#include <set>
#include <queue>
#include <algorithm>

namespace flux {

Repository::Repository(Path repo_path, HashAlgorithm algo)
    : repo_path_(std::move(repo_path)), hash_algorithm_(algo) {
}

Result<std::unique_ptr<Repository>> Repository::open(const Path& path) {
    // Find repository root
    Path current = std::filesystem::absolute(path);
    
    while (true) {
        Path flux_dir = current / ".flux";
        if (Filesystem::exists(flux_dir) && Filesystem::is_directory(flux_dir)) {
            // Found repository
            auto repo = std::unique_ptr<Repository>(new Repository(current, HashAlgorithm::SHA256));
            
            // Load config
            auto config_result = repo->load_config();
            if (!config_result) {
                return flux::unexpected(config_result.error());
            }
            
            // Initialize components
            repo->object_store_ = std::make_unique<ObjectStore>(flux_dir / "objects");
            repo->ref_store_ = std::make_unique<RefStore>(flux_dir / "refs");
            repo->index_ = std::make_unique<Index>(flux_dir / "index.db");
            
            // Initialize index
            auto index_result = repo->index_->init();
            if (!index_result) {
                return flux::unexpected(index_result.error());
            }
            
            return repo;
        }
        
        // Move to parent directory
        Path parent = current.parent_path();
        if (parent == current) {
            // Reached filesystem root
            return flux::unexpected("Not a flux repository (or any parent up to mount point)");
        }
        current = parent;
    }
}

Result<std::unique_ptr<Repository>> Repository::init(const Path& path, HashAlgorithm algo) {
    Path abs_path = std::filesystem::absolute(path);
    
    // Check if already a repository
    if (Filesystem::exists(abs_path / ".flux")) {
        return flux::unexpected(fmt::format("Repository already exists: {}", abs_path.string()));
    }
    
    auto repo = std::unique_ptr<Repository>(new Repository(abs_path, algo));
    
    // Create directory structure
    auto create_result = repo->create_structure();
    if (!create_result) {
        return flux::unexpected(create_result.error());
    }
    
    // Save config
    auto config_result = repo->save_config();
    if (!config_result) {
        return flux::unexpected(config_result.error());
    }
    
    // Initialize components
    Path flux_dir = abs_path / ".flux";
    repo->object_store_ = std::make_unique<ObjectStore>(flux_dir / "objects");
    repo->ref_store_ = std::make_unique<RefStore>(flux_dir / "refs");
    repo->index_ = std::make_unique<Index>(flux_dir / "index.db");
    
    // Initialize index
    auto index_result = repo->index_->init();
    if (!index_result) {
        return flux::unexpected(index_result.error());
    }
    
    // Set HEAD to refs/heads/main
    auto head_result = repo->ref_store_->write_symbolic("HEAD", "refs/heads/main");
    if (!head_result) {
        return flux::unexpected(head_result.error());
    }
    
    return repo;
}

Result<void> Repository::create_structure() {
    Path flux_dir = repo_path_ / ".flux";
    
    // Create directories
    std::vector<Path> dirs = {
        flux_dir,
        flux_dir / "objects",
        flux_dir / "objects" / "loose",
        flux_dir / "objects" / "packs",
        flux_dir / "refs",
        flux_dir / "refs" / "heads",
        flux_dir / "refs" / "tags",
        flux_dir / "refs" / "remotes",
        flux_dir / "wal",
        flux_dir / "snapshots"
    };
    
    for (const auto& dir : dirs) {
        auto result = Filesystem::create_directories(dir);
        if (!result) {
            return result;
        }
    }
    
    // Create description file
    std::string description = "Unnamed repository; edit this file to name the repository.\n";
    Bytes desc_data(description.begin(), description.end());
    auto desc_result = Filesystem::write_file(flux_dir / "description", desc_data);
    if (!desc_result) {
        return desc_result;
    }
    
    return {};
}

Result<void> Repository::load_config() {
    Path config_path = flux_dir() / "config";
    
    if (!Filesystem::exists(config_path)) {
        return flux::unexpected("Config file not found");
    }
    
    auto data_result = Filesystem::read_file(config_path);
    if (!data_result) {
        return flux::unexpected(data_result.error());
    }
    
    std::string_view content(reinterpret_cast<const char*>(data_result->data()), data_result->size());
    
    // Parse config (simplified)
    // Format: key=value
    size_t pos = 0;
    while (pos < content.size()) {
        auto line_end = content.find('\n', pos);
        if (line_end == std::string_view::npos) {
            line_end = content.size();
        }
        
        auto line = content.substr(pos, line_end - pos);
        pos = line_end + 1;
        
        auto eq_pos = line.find('=');
        if (eq_pos != std::string_view::npos) {
            auto key = line.substr(0, eq_pos);
            auto value = line.substr(eq_pos + 1);
            
            if (key == "hash_algorithm") {
                auto algo = parse_hash_algorithm(value);
                if (algo) {
                    hash_algorithm_ = *algo;
                }
            } else if (key == "user.name") {
                user_name_ = std::string(value);
            } else if (key == "user.email") {
                user_email_ = std::string(value);
            } else if (key.starts_with("remote.")) {
                auto rest = key.substr(7);
                auto dot_pos = rest.find(".url");
                if (dot_pos != std::string_view::npos) {
                    auto name = rest.substr(0, dot_pos);
                    remotes_[std::string(name)] = std::string(value);
                }
            }
        }
    }
    
    return {};
}

Result<void> Repository::save_config() {
    Path config_path = flux_dir() / "config";
    
    std::string config = fmt::format(
        "# FluxVCS repository configuration\n"
        "hash_algorithm={}\n"
        "user.name={}\n"
        "user.email={}\n"
        "version=1\n",
        to_string(hash_algorithm_),
        user_name_,
        user_email_
    );
    
    for (const auto& [name, url] : remotes_) {
        config += fmt::format("remote.{}.url={}\n", name, url);
    }
    
    Bytes data(config.begin(), config.end());
    return Filesystem::write_file(config_path, data);
}

Result<std::string> Repository::head() const {
    return ref_store_->read_symbolic("HEAD");
}

Result<void> Repository::set_head(const std::string& ref) {
    return ref_store_->write_symbolic("HEAD", ref);
}

Result<ObjectId> Repository::find_merge_base(const ObjectId& left, const ObjectId& right) {
    if (left == right) return left;
    
    std::set<ObjectId> visited_left;
    std::queue<ObjectId> queue_left;
    queue_left.push(left);
    visited_left.insert(left);
    
    // Build set of all ancestors of left
    while (!queue_left.empty()) {
        ObjectId current = queue_left.front();
        queue_left.pop();
        
        auto obj_res = object_store_->read(current);
        if (!obj_res) continue;
        
        auto commit_res = Commit::deserialize(*obj_res);
        if (!commit_res) continue;
        
        for (const auto& parent : commit_res->parents()) {
            if (visited_left.find(parent) == visited_left.end()) {
                visited_left.insert(parent);
                queue_left.push(parent);
            }
        }
    }
    
    // Find first ancestor of right that is also in visited_left (BFS)
    std::set<ObjectId> visited_right;
    std::queue<ObjectId> queue_right;
    queue_right.push(right);
    visited_right.insert(right);
    
    while (!queue_right.empty()) {
        ObjectId current = queue_right.front();
        queue_right.pop();
        
        if (visited_left.find(current) != visited_left.end()) {
            return current; // Found common ancestor
        }
        
        auto obj_res = object_store_->read(current);
        if (!obj_res) continue;
        
        auto commit_res = Commit::deserialize(*obj_res);
        if (!commit_res) continue;
        
        for (const auto& parent : commit_res->parents()) {
            if (visited_right.find(parent) == visited_right.end()) {
                visited_right.insert(parent);
                queue_right.push(parent);
            }
        }
    }
    
    return flux::unexpected("No common ancestor found");
}

Result<ObjectId> Repository::merge_trees(const ObjectId& base_id, const ObjectId& ours_id, const ObjectId& theirs_id) {
    if (ours_id == theirs_id) return ours_id;
    if (base_id == ours_id) return theirs_id;
    if (base_id == theirs_id) return ours_id;
    
    // Read trees
    auto base_data = object_store_->read(base_id);
    auto ours_data = object_store_->read(ours_id);
    auto theirs_data = object_store_->read(theirs_id);
    
    if (!base_data || !ours_data || !theirs_data) {
        return flux::unexpected("Failed to read trees for merge");
    }
    
    auto base_tree = Tree::deserialize(*base_data);
    auto ours_tree = Tree::deserialize(*ours_data);
    auto theirs_tree = Tree::deserialize(*theirs_data);
    
    if (!base_tree || !ours_tree || !theirs_tree) {
        return flux::unexpected("Failed to deserialize trees for merge");
    }
    
    // Map of entries by name
    std::map<std::string, const TreeEntry*> base_map, ours_map, theirs_map;
    std::set<std::string> all_names;
    
    for (const auto& entry : base_tree->entries()) { base_map[entry.name] = &entry; all_names.insert(entry.name); }
    for (const auto& entry : ours_tree->entries()) { ours_map[entry.name] = &entry; all_names.insert(entry.name); }
    for (const auto& entry : theirs_tree->entries()) { theirs_map[entry.name] = &entry; all_names.insert(entry.name); }
    
    std::vector<TreeEntry> merged_entries;
    
    for (const auto& name : all_names) {
        const TreeEntry* b = base_map.count(name) ? base_map[name] : nullptr;
        const TreeEntry* o = ours_map.count(name) ? ours_map[name] : nullptr;
        const TreeEntry* t = theirs_map.count(name) ? theirs_map[name] : nullptr;
        
        // 3-way merge logic
        bool o_changed = (!b && o) || (b && o && (b->id != o->id || b->mode != o->mode)) || (b && !o);
        bool t_changed = (!b && t) || (b && t && (b->id != t->id || b->mode != t->mode)) || (b && !t);
        
        if (!o_changed && !t_changed) {
            // Unchanged in both
            if (b) merged_entries.emplace_back(*b);
        } else if (o_changed && !t_changed) {
            // Changed in ours only
            if (o) merged_entries.emplace_back(*o);
        } else if (!o_changed && t_changed) {
            // Changed in theirs only
            if (t) merged_entries.emplace_back(*t);
        } else {
            // Both changed - check if they changed to the same thing
            if (o && t && o->id == t->id && o->mode == t->mode) {
                merged_entries.emplace_back(*o);
            } else if (o && t && o->mode == FileMode::Directory && t->mode == FileMode::Directory) {
                // Recursive merge for directories
                ObjectId b_id = (b && b->mode == FileMode::Directory) ? b->id : ObjectId();
                auto sub_res = merge_trees(b_id, o->id, t->id);
                if (!sub_res) return sub_res;
                merged_entries.emplace_back(name, FileMode::Directory, *sub_res);
            } else {
                // True conflict
                return flux::unexpected(fmt::format("Conflict detected in: {}", name));
            }
        }
    }
    
    Tree merged_tree(std::move(merged_entries));
    auto serialized = (hash_algorithm_ == HashAlgorithm::SHA1) ? merged_tree.serialize_git() : merged_tree.serialize();
    return object_store_->write(ObjectType::Tree, serialized, hash_algorithm_);
}

} // namespace flux

void flux::Repository::set_user_info(std::string name, std::string email) {
    user_name_ = std::move(name);
    user_email_ = std::move(email);
    save_config();
}

void flux::Repository::add_remote(const std::string& name, const std::string& url) {
    remotes_[name] = url;
    save_config();
}

void flux::Repository::remove_remote(const std::string& name) {
    remotes_.erase(name);
    save_config();
}

std::string flux::Repository::get_remote_url(const std::string& name) const {
    auto it = remotes_.find(name);
    if (it != remotes_.end()) {
        return it->second;
    }
    return "";
}
