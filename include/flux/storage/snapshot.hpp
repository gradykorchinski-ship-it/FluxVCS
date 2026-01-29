#pragma once

#include "flux/core/types.hpp"
#include "flux/core/object_id.hpp"
#include <chrono>
#include <map>

namespace flux {

// Forward declaration
class Repository;

/**
 * Repository snapshot for rollback and recovery
 */
class Snapshot {
public:
    struct RefSnapshot {
        std::string name;
        ObjectId target;
        bool is_symbolic;
        std::string symbolic_target;
    };
    
    Snapshot(uint64_t id, std::chrono::system_clock::time_point timestamp);
    
    // Create snapshot of current repository state
    static Result<Snapshot> create(Repository& repo, const std::string& description = "");
    
    // Restore repository to this snapshot
    Result<void> restore(Repository& repo) const;
    
    // Save snapshot to disk
    Result<void> save(const Path& snapshot_dir) const;
    
    // Load snapshot from disk
    static Result<Snapshot> load(const Path& snapshot_path);
    
    // Prune old snapshots
    static Result<std::vector<uint64_t>> prune_old(
        const Path& snapshot_dir,
        std::chrono::hours max_age
    );
    
    // List all snapshots
    static Result<std::vector<Snapshot>> list(const Path& snapshot_dir);
    
    // Getters
    uint64_t id() const { return id_; }
    std::chrono::system_clock::time_point timestamp() const { return timestamp_; }
    const std::string& description() const { return description_; }
    const std::map<std::string, RefSnapshot>& refs() const { return refs_; }
    const ObjectId& head_commit() const { return head_commit_; }
    
private:
    uint64_t id_;
    std::chrono::system_clock::time_point timestamp_;
    std::string description_;
    ObjectId head_commit_;
    std::map<std::string, RefSnapshot> refs_;  // All refs at snapshot time
    
    // Serialization
    Bytes serialize() const;
    static Result<Snapshot> deserialize(std::span<const uint8_t> data);
};

} // namespace flux
