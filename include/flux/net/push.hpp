#pragma once

#include "flux/core/types.hpp"
#include "flux/core/object_id.hpp"
#include <string>
#include <vector>
#include <set>

namespace flux {

class Repository;

/**
 * Pusher - Handles sending local changes to a remote repository
 */
class Pusher {
public:
    explicit Pusher(Repository& repo);
    
    /**
     * Push a local branch to a remote
     * @param remote_url Remote Git URL
     * @param branch_name Local branch name (e.g., "main")
     * @return Result of the push operation
     */
    Result<void> push(const std::string& remote_url, const std::string& branch_name);
    
    // Set authentication token
    void set_token(const std::string& token);

private:
    Repository& repo_;
    std::string token_;
    
    // Traverses history to find all objects that need to be sent
    Result<std::vector<ObjectId>> collect_objects(const ObjectId& local_oid, const ObjectId& remote_oid);
    
    // Add all objects from a tree recursively
    void add_tree_objects(const ObjectId& tree_id, std::vector<ObjectId>& objects, std::set<ObjectId>& seen);
};

} // namespace flux
