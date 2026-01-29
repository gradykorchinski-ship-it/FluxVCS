#pragma once

#include "flux/core/types.hpp"
#include "flux/core/object_id.hpp"
#include "flux/storage/object_store.hpp"
#include "flux/storage/ref_store.hpp"
#include "flux/storage/wal.hpp"
#include "flux/index/index.hpp"
#include <memory>
#include <vector>

namespace flux {

/**
 * Repository - Main interface to a FluxVCS repository
 */
class Repository {
public:
    // Open existing repository
    static Result<std::unique_ptr<Repository>> open(const Path& path);
    
    // Initialize new repository
    static Result<std::unique_ptr<Repository>> init(const Path& path, HashAlgorithm algo = HashAlgorithm::SHA256);
    
    // Get repository root path
    const Path& path() const { return repo_path_; }
    
    // Get .flux directory path
    Path flux_dir() const { return repo_path_ / ".flux"; }
    
    // Get object store
    ObjectStore& objects() { return *object_store_; }
    const ObjectStore& objects() const { return *object_store_; }
    
    // Get reference store
    RefStore& refs() { return *ref_store_; }
    const RefStore& refs() const { return *ref_store_; }
    
    // Accessors
    const Path& workdir() const { return repo_path_; }
    Path gitdir() const { return repo_path_ / ".flux"; }
    ObjectStore& object_store() { return *object_store_; }
    RefStore& ref_store() { return *ref_store_; }
    Index& index() { return *index_; }
    
    // Get current hash algorithm
    HashAlgorithm hash_algorithm() const { return hash_algorithm_; }
    
    // Get HEAD reference
    Result<std::string> head() const;
    
    // Set HEAD reference
    Result<void> set_head(const std::string& ref);
    
    // Find merge base (common ancestor) of two commits
    Result<ObjectId> find_merge_base(const ObjectId& left, const ObjectId& right);
    
    // Perform 3-way tree merge
    Result<ObjectId> merge_trees(const ObjectId& base, const ObjectId& ours, const ObjectId& theirs);
    
    // User information
    const std::string& user_name() const { return user_name_; }
    const std::string& user_email() const { return user_email_; }
    void set_user_info(std::string name, std::string email);
    
private:
    Repository(Path repo_path, HashAlgorithm algo);
    
    Result<void> create_structure();
    Result<void> load_config();
    Result<void> save_config();
    
    Path repo_path_;
    HashAlgorithm hash_algorithm_;
    
    std::unique_ptr<ObjectStore> object_store_;
    std::unique_ptr<RefStore> ref_store_;
    std::unique_ptr<Index> index_;
    
    std::string user_name_ = "FluxVCS User";
    std::string user_email_ = "user@example.com";
};

} // namespace flux
