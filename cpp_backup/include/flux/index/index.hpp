#pragma once

#include "flux/core/types.hpp"
#include "flux/core/object_id.hpp"
#include <sqlite3.h>
#include <memory>

namespace flux {

/**
 * FileStatus - Status of a file in the working tree
 */
struct FileStatus {
    Path path;
    bool modified = false;
    bool staged = false;
    bool untracked = false;
    bool deleted = false;
};

/**
 * Index - SQLite-based metadata index for fast operations
 * 
 * Tracks working tree and staging area state.
 */
class Index {
public:
    explicit Index(Path index_path);
    ~Index();
    
    // Initialize database schema
    Result<void> init();
    
    // Update working tree entry
    Result<void> update_working_tree(const Path& path, const ObjectId& id, 
                                     FileMode mode, uint64_t mtime, uint64_t size);
    
    // Update staging area entry
    Result<void> stage_file(const Path& path, const ObjectId& id, FileMode mode);
    
    // Remove from staging area
    Result<void> unstage_file(const Path& path);
    
    // Clear staging area
    Result<void> clear_staging();
    
    // Get file status
    Result<FileStatus> get_status(const Path& path);
    
    // Get all modified files
    Result<std::vector<FileStatus>> get_all_status();
    
    // Get staged files
    Result<std::vector<std::pair<Path, ObjectId>>> get_staged_files();
    
private:
    Result<void> execute(const std::string& sql);
    
    Path index_path_;
    sqlite3* db_ = nullptr;
};

} // namespace flux
