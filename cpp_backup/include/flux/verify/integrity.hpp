#pragma once

#include "flux/core/types.hpp"
#include "flux/core/object_id.hpp"
#include <vector>
#include <set>

namespace flux {

// Forward declarations
class Repository;
class ObjectStore;
class RefStore;

/**
 * Repository integrity verification
 */
class IntegrityChecker {
public:
    enum class IssueType {
        MISSING_OBJECT,
        CORRUPT_OBJECT,
        INVALID_REFERENCE,
        DANGLING_REFERENCE,
        UNREACHABLE_OBJECT,
        CYCLE_IN_COMMIT_GRAPH,
        INVALID_TREE_ENTRY,
        INVALID_PARENT_LINK
    };
    
    struct Issue {
        IssueType type;
        std::string message;
        std::string suggestion;
        std::optional<ObjectId> object_id;
        std::optional<std::string> ref_name;
    };
    
    explicit IntegrityChecker(Repository& repo);
    
    // Run full integrity check
    Result<std::vector<Issue>> check_all();
    
    // Individual checks
    Result<std::vector<Issue>> check_objects();
    Result<std::vector<Issue>> check_references();
    Result<std::vector<Issue>> check_commit_graph();
    Result<std::vector<Issue>> check_reachability();
    
    // Repair operations
    Result<void> repair_references();
    Result<void> prune_unreachable();
    
    // Statistics
    struct Stats {
        size_t total_objects;
        size_t total_commits;
        size_t total_trees;
        size_t total_blobs;
        size_t total_refs;
        size_t unreachable_objects;
    };
    
    Result<Stats> get_stats();
    
private:
    Repository& repo_;
    
    // Helper methods
    Result<std::set<ObjectId>> collect_reachable_objects();
    Result<bool> verify_object(const ObjectId& id);
    Result<bool> has_cycle(const ObjectId& commit_id, std::set<ObjectId>& visited, std::set<ObjectId>& recursion_stack);
};

} // namespace flux
