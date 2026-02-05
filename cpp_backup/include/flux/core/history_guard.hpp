#pragma once

#include "flux/core/types.hpp"
#include "flux/core/object_id.hpp"
#include <set>

namespace flux {

// Forward declaration
class Repository;

/**
 * Tracks and protects published (public) history
 */
class HistoryGuard {
public:
    explicit HistoryGuard(Repository& repo);
    
    // Mark a commit as published (pushed to remote)
    Result<void> mark_published(const ObjectId& commit_id);
    
    // Check if a commit is published
    bool is_published(const ObjectId& commit_id) const;
    
    // Check if rewriting history is safe
    Result<bool> can_rewrite(const ObjectId& commit_id) const;
    
    // Get all published commits
    const std::set<ObjectId>& published_commits() const { return published_; }
    
    // Save published commit list
    Result<void> save();
    
    // Load published commit list
    Result<void> load();
    
private:
    Repository& repo_;
    std::set<ObjectId> published_;
    Path published_file_;
};

} // namespace flux
