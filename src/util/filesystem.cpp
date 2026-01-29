#include "flux/util/filesystem.hpp"
#include <fstream>
#include <fmt/format.h>

namespace flux {

Result<Bytes> Filesystem::read_file(const Path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    
    if (!file) {
        return flux::unexpected(fmt::format("Failed to open file: {}", path.string()));
    }
    
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    Bytes data(size);
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        return flux::unexpected(fmt::format("Failed to read file: {}", path.string()));
    }
    
    return data;
}

Result<void> Filesystem::write_file(const Path& path, std::span<const uint8_t> data) {
    // Write to temporary file first for atomicity
    Path temp_path = path;
    temp_path += ".tmp";
    
    std::ofstream file(temp_path, std::ios::binary);
    if (!file) {
        return flux::unexpected(fmt::format("Failed to create file: {}", temp_path.string()));
    }
    
    if (!file.write(reinterpret_cast<const char*>(data.data()), data.size())) {
        return flux::unexpected(fmt::format("Failed to write file: {}", temp_path.string()));
    }
    
    file.close();
    
    // Atomic rename
    std::error_code ec;
    std::filesystem::rename(temp_path, path, ec);
    
    if (ec) {
        return flux::unexpected(fmt::format("Failed to rename file: {}", ec.message()));
    }
    
    return {};
}

Result<void> Filesystem::create_directories(const Path& path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    
    if (ec) {
        return flux::unexpected(fmt::format("Failed to create directories: {}", ec.message()));
    }
    
    return {};
}

bool Filesystem::exists(const Path& path) {
    return std::filesystem::exists(path);
}

bool Filesystem::is_directory(const Path& path) {
    return std::filesystem::is_directory(path);
}

Result<size_t> Filesystem::file_size(const Path& path) {
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    
    if (ec) {
        return flux::unexpected(fmt::format("Failed to get file size: {}", ec.message()));
    }
    
    return size;
}

Result<std::chrono::system_clock::time_point> Filesystem::last_write_time(const Path& path) {
    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(path, ec);
    
    if (ec) {
        return flux::unexpected(fmt::format("Failed to get last write time: {}", ec.message()));
    }
    
    // Convert file_time to system_clock time
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
    );
    
    return sctp;
}

Result<std::vector<Path>> Filesystem::list_directory(const Path& path) {
    std::error_code ec;
    std::vector<Path> result;
    
    for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
        if (ec) {
            return flux::unexpected(fmt::format("Failed to list directory: {}", ec.message()));
        }
        result.push_back(entry.path());
    }
    
    return result;
}

Result<void> Filesystem::remove(const Path& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    
    if (ec) {
        return flux::unexpected(fmt::format("Failed to remove: {}", ec.message()));
    }
    
    return {};
}

Result<void> Filesystem::rename(const Path& from, const Path& to) {
    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    
    if (ec) {
        return flux::unexpected(fmt::format("Failed to rename: {}", ec.message()));
    }
    
    return {};
}

} // namespace flux
