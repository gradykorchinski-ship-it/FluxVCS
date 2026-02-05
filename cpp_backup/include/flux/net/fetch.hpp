#pragma once

#include "flux/core/types.hpp"
#include "flux/core/object_id.hpp"
#include "flux/net/git_protocol.hpp"
#include "flux/net/http_transport.hpp"
#include <string>
#include <vector>

namespace flux {

// Forward declarations
class Repository;
class ObjectStore;

/**
 * Fetch objects from remote repository
 */
class Fetcher {
public:
    explicit Fetcher(Repository& repo);
    
    // Fetch all refs from remote
    Result<void> fetch_all(const std::string& remote_url);
    
    // Fetch specific refs
    Result<void> fetch_refs(
        const std::string& remote_url,
        const std::vector<std::string>& ref_patterns
    );
    
    // Get list of remote refs
    Result<std::vector<GitRef>> list_remote_refs(const std::string& remote_url);
    
    // Set authentication
    void set_credentials(const std::string& username, const std::string& password);
    void set_token(const std::string& token);
    
private:
    Repository& repo_;
    std::string username_;
    std::string password_;
    std::string token_;
    
    // Receive and unpack pack file
    Result<void> receive_pack(
        const std::string& remote_url,
        const std::vector<ObjectId>& wants,
        const std::vector<ObjectId>& haves
    );
    
    // Unpack pack file data
    Result<void> unpack_objects(std::span<const uint8_t> pack_data);
    
    // Update refs after fetch
    Result<void> update_refs(const std::vector<GitRef>& remote_refs);
};

/**
 * Checkout working tree from repository
 */
class Checkout {
public:
    explicit Checkout(Repository& repo);
    
    // Checkout a specific commit
    Result<void> checkout_commit(const ObjectId& commit_id);
    
    // Checkout a branch
    Result<void> checkout_branch(const std::string& branch_name);
    
    // Checkout HEAD
    Result<void> checkout_head();
    
private:
    Repository& repo_;
    
    // Materialize tree to working directory
    Result<void> materialize_tree(const ObjectId& tree_id, const Path& base_path);
};

} // namespace flux
