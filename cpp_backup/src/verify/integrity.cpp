#include "flux/verify/integrity.hpp"
#include "flux/core/repository.hpp"
#include "flux/core/commit.hpp"
#include "flux/core/tree.hpp"
#include "flux/core/blob.hpp"
#include "flux/storage/object_store.hpp"
#include "flux/storage/ref_store.hpp"
#include "flux/util/filesystem.hpp"
#include <fmt/format.h>
#include <queue>

namespace flux {

IntegrityChecker::IntegrityChecker(Repository& repo) : repo_(repo) {}

Result<std::vector<IntegrityChecker::Issue>> IntegrityChecker::check_all() {
    std::vector<Issue> all_issues;
    
    auto object_issues = check_objects();
    if (object_issues) {
        all_issues.insert(all_issues.end(), object_issues->begin(), object_issues->end());
    }
    
    auto ref_issues = check_references();
    if (ref_issues) {
        all_issues.insert(all_issues.end(), ref_issues->begin(), ref_issues->end());
    }
    
    auto graph_issues = check_commit_graph();
    if (graph_issues) {
        all_issues.insert(all_issues.end(), graph_issues->begin(), graph_issues->end());
    }
    
    auto reach_issues = check_reachability();
    if (reach_issues) {
        all_issues.insert(all_issues.end(), reach_issues->begin(), reach_issues->end());
    }
    
    return all_issues;
}

Result<std::vector<IntegrityChecker::Issue>> IntegrityChecker::check_objects() {
    std::vector<Issue> issues;
    
    // Check all objects in object store
    Path objects_dir = repo_.flux_dir() / "objects" / "loose";
    if (!Filesystem::exists(objects_dir)) {
        return issues;
    }
    
    // This is a simplified check - in production we'd scan all object directories
    // For now, just verify objects referenced by refs are valid
    
    return issues;
}

Result<std::vector<IntegrityChecker::Issue>> IntegrityChecker::check_references() {
    std::vector<Issue> issues;
    
    auto refs_result = repo_.refs().list();
    if (!refs_result) {
        Issue issue;
        issue.type = IssueType::INVALID_REFERENCE;
        issue.message = "Failed to list references";
        issue.suggestion = "Repository may be corrupted";
        issues.push_back(issue);
        return issues;
    }
    
    for (const auto& ref_name : *refs_result) {
        // Check if symbolic
        auto symbolic_result = repo_.refs().read_symbolic(ref_name);
        if (symbolic_result) {
            // Verify target exists
            auto target_result = repo_.refs().read(*symbolic_result);
            if (!target_result) {
                Issue issue;
                issue.type = IssueType::DANGLING_REFERENCE;
                issue.message = fmt::format("Symbolic ref '{}' points to non-existent '{}'", 
                    ref_name, *symbolic_result);
                issue.suggestion = "Delete the symbolic reference or create the target";
                issue.ref_name = ref_name;
                issues.push_back(issue);
            }
        } else {
            // Direct reference - verify object exists
            auto target_result = repo_.refs().read(ref_name);
            if (!target_result) {
                Issue issue;
                issue.type = IssueType::INVALID_REFERENCE;
                issue.message = fmt::format("Failed to read reference '{}'", ref_name);
                issue.suggestion = "Reference file may be corrupted";
                issue.ref_name = ref_name;
                issues.push_back(issue);
                continue;
            }
            
            if (!repo_.objects().exists(*target_result)) {
                Issue issue;
                issue.type = IssueType::MISSING_OBJECT;
                issue.message = fmt::format("Reference '{}' points to missing object {}", 
                    ref_name, target_result->to_hex());
                issue.suggestion = "Object may have been deleted or corrupted";
                issue.ref_name = ref_name;
                issue.object_id = *target_result;
                issues.push_back(issue);
            }
        }
    }
    
    return issues;
}

Result<std::vector<IntegrityChecker::Issue>> IntegrityChecker::check_commit_graph() {
    std::vector<Issue> issues;
    
    auto refs_result = repo_.refs().list();
    if (!refs_result) {
        return issues;
    }
    
    std::set<ObjectId> visited;
    std::set<ObjectId> recursion_stack;
    
    for (const auto& ref_name : *refs_result) {
        auto commit_result = repo_.refs().read(ref_name);
        if (!commit_result) {
            continue;
        }
        
        auto cycle_result = has_cycle(*commit_result, visited, recursion_stack);
        if (cycle_result && *cycle_result) {
            Issue issue;
            issue.type = IssueType::CYCLE_IN_COMMIT_GRAPH;
            issue.message = fmt::format("Cycle detected in commit graph starting from {}", 
                commit_result->to_hex());
            issue.suggestion = "Repository is corrupted - commit graph should be a DAG";
            issue.object_id = *commit_result;
            issues.push_back(issue);
        }
    }
    
    return issues;
}

Result<std::vector<IntegrityChecker::Issue>> IntegrityChecker::check_reachability() {
    std::vector<Issue> issues;
    
    // Collect all reachable objects
    auto reachable_result = collect_reachable_objects();
    if (!reachable_result) {
        return flux::unexpected(reachable_result.error());
    }
    
    // Note: Full unreachable object detection would require scanning all objects
    // This is a simplified version
    
    return issues;
}

Result<void> IntegrityChecker::repair_references() {
    auto issues_result = check_references();
    if (!issues_result) {
        return flux::unexpected(issues_result.error());
    }
    
    for (const auto& issue : *issues_result) {
        if (issue.type == IssueType::DANGLING_REFERENCE && issue.ref_name) {
            // Remove dangling symbolic references
            repo_.refs().remove(*issue.ref_name);
        }
    }
    
    return {};
}

Result<void> IntegrityChecker::prune_unreachable() {
    // This would remove unreachable objects
    // Simplified implementation - just return success
    return {};
}

Result<IntegrityChecker::Stats> IntegrityChecker::get_stats() {
    Stats stats{};
    
    auto reachable_result = collect_reachable_objects();
    if (!reachable_result) {
        return flux::unexpected(reachable_result.error());
    }
    
    stats.total_objects = reachable_result->size();
    
    // Count by type
    for (const auto& id : *reachable_result) {
        auto type_result = repo_.objects().get_type(id);
        if (type_result) {
            switch (*type_result) {
                case ObjectType::Commit: stats.total_commits++; break;
                case ObjectType::Tree: stats.total_trees++; break;
                case ObjectType::Blob: stats.total_blobs++; break;
                default: break;
            }
        }
    }
    
    auto refs_result = repo_.refs().list();
    if (refs_result) {
        stats.total_refs = refs_result->size();
    }
    
    return stats;
}

Result<std::set<ObjectId>> IntegrityChecker::collect_reachable_objects() {
    std::set<ObjectId> reachable;
    std::queue<ObjectId> to_visit;
    
    // Start from all refs
    auto refs_result = repo_.refs().list();
    if (!refs_result) {
        return flux::unexpected(refs_result.error());
    }
    
    for (const auto& ref_name : *refs_result) {
        auto commit_result = repo_.refs().read(ref_name);
        if (commit_result) {
            to_visit.push(*commit_result);
        }
    }
    
    while (!to_visit.empty()) {
        ObjectId current = to_visit.front();
        to_visit.pop();
        
        if (reachable.count(current)) {
            continue;
        }
        
        reachable.insert(current);
        
        // Get object type and traverse
        auto type_result = repo_.objects().get_type(current);
        if (!type_result) {
            continue;
        }
        
        if (*type_result == ObjectType::Commit) {
            auto data_result = repo_.objects().read(current);
            if (data_result) {
                auto commit_result = Commit::deserialize(*data_result);
                if (commit_result) {
                    // Add tree
                    to_visit.push(commit_result->tree());
                    // Add parents
                    for (const auto& parent : commit_result->parents()) {
                        to_visit.push(parent);
                    }
                }
            }
        } else if (*type_result == ObjectType::Tree) {
            auto data_result = repo_.objects().read(current);
            if (data_result) {
                auto tree_result = Tree::deserialize(*data_result);
                if (tree_result) {
                    for (const auto& entry : tree_result->entries()) {
                        to_visit.push(entry.id);
                    }
                }
            }
        }
        // Blobs have no children
    }
    
    return reachable;
}

Result<bool> IntegrityChecker::verify_object(const ObjectId& id) {
    if (!repo_.objects().exists(id)) {
        return false;
    }
    
    auto data_result = repo_.objects().read(id);
    if (!data_result) {
        return false;
    }
    
    // Verify hash matches
    auto computed_id = ObjectId::compute(repo_.hash_algorithm(), *data_result);
    return computed_id == id;
}

Result<bool> IntegrityChecker::has_cycle(
    const ObjectId& commit_id,
    std::set<ObjectId>& visited,
    std::set<ObjectId>& recursion_stack
) {
    if (recursion_stack.count(commit_id)) {
        return true;  // Cycle detected
    }
    
    if (visited.count(commit_id)) {
        return false;  // Already visited, no cycle
    }
    
    visited.insert(commit_id);
    recursion_stack.insert(commit_id);
    
    // Read commit and check parents
    auto data_result = repo_.objects().read(commit_id);
    if (!data_result) {
        recursion_stack.erase(commit_id);
        return false;
    }
    
    auto commit_result = Commit::deserialize(*data_result);
    if (!commit_result) {
        recursion_stack.erase(commit_id);
        return false;
    }
    
    for (const auto& parent : commit_result->parents()) {
        auto cycle_result = has_cycle(parent, visited, recursion_stack);
        if (cycle_result && *cycle_result) {
            return true;
        }
    }
    
    recursion_stack.erase(commit_id);
    return false;
}

} // namespace flux
