#include "flux/storage/ref_store.hpp"
#include "flux/util/filesystem.hpp"
#include <fmt/format.h>
#include <fstream>

namespace flux {

RefStore::RefStore(Path refs_dir) : refs_dir_(std::move(refs_dir)) {
}

Result<ObjectId> RefStore::read(const std::string& name) {
    Path ref_path = get_ref_path(name);
    
    auto data_result = Filesystem::read_file(ref_path);
    if (!data_result) {
        return flux::unexpected(data_result.error());
    }
    
    // Parse object ID from file content
    std::string_view content(reinterpret_cast<const char*>(data_result->data()), data_result->size());
    
    // Trim whitespace
    while (!content.empty() && std::isspace(content.back())) {
        content.remove_suffix(1);
    }
    
    return ObjectId::from_hex(content);
}

Result<void> RefStore::write(const std::string& name, const ObjectId& id) {
    Path ref_path = get_ref_path(name);
    
    // Create parent directories
    auto dir_result = Filesystem::create_directories(ref_path.parent_path());
    if (!dir_result) {
        return dir_result;
    }
    
    // Write object ID
    std::string content = id.to_hex() + "\n";
    Bytes data(content.begin(), content.end());
    
    return Filesystem::write_file(ref_path, data);
}

Result<void> RefStore::remove(const std::string& name) {
    return Filesystem::remove(get_ref_path(name));
}

bool RefStore::exists(const std::string& name) {
    return Filesystem::exists(get_ref_path(name));
}

Result<std::vector<std::string>> RefStore::list() {
    std::vector<std::string> refs;
    
    // List refs/heads/
    Path heads_dir = refs_dir_ / "heads";
    if (Filesystem::exists(heads_dir)) {
        auto heads_result = Filesystem::list_directory(heads_dir);
        if (!heads_result) {
            return flux::unexpected(heads_result.error());
        }
        
        for (const auto& path : *heads_result) {
            refs.push_back("refs/heads/" + path.filename().string());
        }
    }
    
    // List refs/tags/
    Path tags_dir = refs_dir_ / "tags";
    if (Filesystem::exists(tags_dir)) {
        auto tags_result = Filesystem::list_directory(tags_dir);
        if (!tags_result) {
            return flux::unexpected(tags_result.error());
        }
        
        for (const auto& path : *tags_result) {
            refs.push_back("refs/tags/" + path.filename().string());
        }
    }
    
    return refs;
}

Result<std::string> RefStore::read_symbolic(const std::string& name) {
    Path ref_path = refs_dir_.parent_path() / name;  // e.g., .flux/HEAD
    
    auto data_result = Filesystem::read_file(ref_path);
    if (!data_result) {
        return flux::unexpected(data_result.error());
    }
    
    std::string_view content(reinterpret_cast<const char*>(data_result->data()), data_result->size());
    
    // Format: "ref: refs/heads/main\n"
    if (!content.starts_with("ref: ")) {
        return flux::unexpected("Not a symbolic reference");
    }
    
    content.remove_prefix(5);  // Remove "ref: "
    
    // Trim whitespace
    while (!content.empty() && std::isspace(content.back())) {
        content.remove_suffix(1);
    }
    
    return std::string(content);
}

Result<void> RefStore::write_symbolic(const std::string& name, const std::string& target) {
    Path ref_path = refs_dir_.parent_path() / name;
    
    std::string content = fmt::format("ref: {}\n", target);
    Bytes data(content.begin(), content.end());
    
    return Filesystem::write_file(ref_path, data);
}

Path RefStore::get_ref_path(const std::string& name) const {
    // name is like "refs/heads/main"
    return refs_dir_.parent_path() / name;
}

} // namespace flux
