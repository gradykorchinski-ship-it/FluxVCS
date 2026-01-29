#include "flux/index/index.hpp"
#include "flux/util/filesystem.hpp"
#include <fmt/format.h>

namespace flux {

Index::Index(Path index_path) : index_path_(std::move(index_path)) {
}

Index::~Index() {
    if (db_) {
        sqlite3_close(db_);
    }
}

Result<void> Index::init() {
    // Open database
    int rc = sqlite3_open(index_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        return flux::unexpected(fmt::format("Failed to open index database: {}", 
            sqlite3_errmsg(db_)));
    }
    
    // Create tables
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS working_tree (
            path TEXT PRIMARY KEY,
            object_id TEXT NOT NULL,
            mode INTEGER NOT NULL,
            mtime INTEGER NOT NULL,
            size INTEGER NOT NULL
        );
        
        CREATE TABLE IF NOT EXISTS staging_area (
            path TEXT PRIMARY KEY,
            object_id TEXT NOT NULL,
            mode INTEGER NOT NULL
        );
        
        CREATE INDEX IF NOT EXISTS idx_working_tree_mtime ON working_tree(mtime);
    )";
    
    return execute(schema);
}

Result<void> Index::update_working_tree(const Path& path, const ObjectId& id, 
                                        FileMode mode, uint64_t mtime, uint64_t size) {
    const char* sql = R"(
        INSERT OR REPLACE INTO working_tree (path, object_id, mode, mtime, size)
        VALUES (?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return flux::unexpected(fmt::format("Failed to prepare statement: {}", 
            sqlite3_errmsg(db_)));
    }
    
    std::string path_str = path.string();
    std::string id_str = id.to_hex();
    
    sqlite3_bind_text(stmt, 1, path_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, id_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, static_cast<int>(mode));
    sqlite3_bind_int64(stmt, 4, mtime);
    sqlite3_bind_int64(stmt, 5, size);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        return flux::unexpected(fmt::format("Failed to update working tree: {}", 
            sqlite3_errmsg(db_)));
    }
    
    return {};
}

Result<void> Index::stage_file(const Path& path, const ObjectId& id, FileMode mode) {
    const char* sql = R"(
        INSERT OR REPLACE INTO staging_area (path, object_id, mode)
        VALUES (?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return flux::unexpected(fmt::format("Failed to prepare statement: {}", 
            sqlite3_errmsg(db_)));
    }
    
    std::string path_str = path.string();
    std::string id_str = id.to_hex();
    
    sqlite3_bind_text(stmt, 1, path_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, id_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, static_cast<int>(mode));
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        return flux::unexpected(fmt::format("Failed to stage file: {}", 
            sqlite3_errmsg(db_)));
    }
    
    return {};
}

Result<void> Index::unstage_file(const Path& path) {
    const char* sql = "DELETE FROM staging_area WHERE path = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return flux::unexpected(fmt::format("Failed to prepare statement: {}", 
            sqlite3_errmsg(db_)));
    }
    
    std::string path_str = path.string();
    sqlite3_bind_text(stmt, 1, path_str.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        return flux::unexpected(fmt::format("Failed to unstage file: {}", 
            sqlite3_errmsg(db_)));
    }
    
    return {};
}

Result<void> Index::clear_staging() {
    return execute("DELETE FROM staging_area");
}

Result<FileStatus> Index::get_status(const Path& path) {
    // Simplified implementation
    FileStatus status;
    status.path = path;
    
    // Check if staged
    const char* staged_sql = "SELECT 1 FROM staging_area WHERE path = ?";
    sqlite3_stmt* stmt;
    
    std::string path_str = path.string();
    
    if (sqlite3_prepare_v2(db_, staged_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, path_str.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            status.staged = true;
        }
        sqlite3_finalize(stmt);
    }
    
    return status;
}

Result<std::vector<FileStatus>> Index::get_all_status() {
    std::vector<FileStatus> result;
    
    // Get all staged files
    const char* sql = "SELECT path FROM staging_area";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return flux::unexpected(fmt::format("Failed to query staging area: {}", 
            sqlite3_errmsg(db_)));
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* path_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        FileStatus status;
        status.path = path_str;
        status.staged = true;
        result.push_back(status);
    }
    
    sqlite3_finalize(stmt);
    
    return result;
}

Result<std::vector<std::pair<Path, ObjectId>>> Index::get_staged_files() {
    std::vector<std::pair<Path, ObjectId>> result;
    
    const char* sql = "SELECT path, object_id FROM staging_area";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return flux::unexpected(fmt::format("Failed to query staging area: {}", 
            sqlite3_errmsg(db_)));
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* path_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* id_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        
        auto id_result = ObjectId::from_hex(id_str);
        if (!id_result) {
            sqlite3_finalize(stmt);
            return flux::unexpected(id_result.error());
        }
        
        result.emplace_back(Path(path_str), *id_result);
    }
    
    sqlite3_finalize(stmt);
    
    return result;
}

Result<void> Index::execute(const std::string& sql) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        if (err_msg) {
            sqlite3_free(err_msg);
        }
        return flux::unexpected(fmt::format("SQL error: {}", error));
    }
    
    return {};
}

} // namespace flux
