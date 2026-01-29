#pragma once

#include "flux/core/types.hpp"
#include <span>

namespace flux {

/**
 * Filesystem utilities
 */
class Filesystem {
public:
    // Read entire file into memory
    static Result<Bytes> read_file(const Path& path);
    
    // Write data to file atomically
    static Result<void> write_file(const Path& path, std::span<const uint8_t> data);
    
    // Create directory and all parents
    static Result<void> create_directories(const Path& path);
    
    // Check if path exists
    static bool exists(const Path& path);
    
    // Check if path is directory
    static bool is_directory(const Path& path);
    
    // Get file size
    static Result<size_t> file_size(const Path& path);
    
    // Get file modification time
    static Result<std::chrono::system_clock::time_point> last_write_time(const Path& path);
    
    // List directory contents
    static Result<std::vector<Path>> list_directory(const Path& path);
    
    // Remove file or directory
    static Result<void> remove(const Path& path);
    
    // Rename/move file
    static Result<void> rename(const Path& from, const Path& to);
};

} // namespace flux
