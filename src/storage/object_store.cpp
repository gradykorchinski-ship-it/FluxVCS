#include "flux/storage/object_store.hpp"
#include "flux/util/filesystem.hpp"
#include "flux/util/compression.hpp"
#include <fmt/format.h>
#include <fstream>

namespace flux {

ObjectStore::ObjectStore(Path objects_dir) 
    : objects_dir_(std::move(objects_dir))
    , pack_manager_(objects_dir_ / "pack") {
    // Attempt to load existing packs, ignore errors in constructor
    (void)pack_manager_.load_packs();
}

Result<ObjectId> ObjectStore::write(ObjectType type, std::span<const uint8_t> data, HashAlgorithm algo) {
    // Compute object ID
    ObjectId id = ObjectId::compute(algo, data, type);
    
    // Check if already exists
    if (exists(id)) {
        return id;
    }
    
    // Write as loose object
    auto result = write_loose(id, type, data);
    if (!result) {
        return flux::unexpected(result.error());
    }
    
    return id;
}

Result<Bytes> ObjectStore::read(const ObjectId& id) {
    // Try loose objects first
    auto loose_result = read_loose(id);
    if (loose_result) {
        return loose_result;
    }
    
    // Try pack files
    if (pack_manager_.contains(id)) {
        return pack_manager_.read_object(id);
    }
    
    return flux::unexpected(fmt::format("Object not found: {}", id.to_hex()));
}

bool ObjectStore::exists(const ObjectId& id) {
    return Filesystem::exists(get_loose_path(id));
}

Result<ObjectType> ObjectStore::get_type(const ObjectId& id) {
    Path obj_path = get_loose_path(id);
    std::ifstream file(obj_path, std::ios::binary);
    if (!file) {
        return flux::unexpected(fmt::format("Object not found: {}", id.to_hex()));
    }
    
    char type_byte;
    if (!file.get(type_byte)) {
        return flux::unexpected("Failed to read type byte");
    }
    
    return static_cast<ObjectType>(static_cast<uint8_t>(type_byte));
}

Result<void> ObjectStore::remove(const ObjectId& id) {
    return Filesystem::remove(get_loose_path(id));
}

Path ObjectStore::get_loose_path(const ObjectId& id) const {
    // Use first 2 hex chars as subdirectory for better filesystem performance
    std::string hex = id.to_hex();
    
    // Extract algorithm prefix and hash
    auto colon_pos = hex.find(':');
    std::string algo_prefix = hex.substr(0, colon_pos);
    std::string hash_hex = (colon_pos != std::string::npos) ? hex.substr(colon_pos + 1) : hex;
    
    if (hash_hex.length() < 2) {
        return objects_dir_ / "loose" / "invalid";
    }
    
    // Create path: objects/loose/algo/ab/cdef...
    return objects_dir_ / "loose" / algo_prefix / hash_hex.substr(0, 2) / hash_hex.substr(2);
}

Result<void> ObjectStore::write_loose(const ObjectId& id, ObjectType type, std::span<const uint8_t> data) {
    Path obj_path = get_loose_path(id);
    
    // Create parent directories
    auto dir_result = Filesystem::create_directories(obj_path.parent_path());
    if (!dir_result) {
        return dir_result;
    }
    
    // Format: <type_byte><compressed_data>
    Bytes obj_data;
    obj_data.push_back(static_cast<uint8_t>(type));
    
    // Compress data
    auto compressed_result = Compression::compress(data);
    if (!compressed_result) {
        return flux::unexpected(compressed_result.error());
    }
    
    obj_data.insert(obj_data.end(), compressed_result->begin(), compressed_result->end());
    
    // Write atomically
    return Filesystem::write_file(obj_path, obj_data);
}

Result<Bytes> ObjectStore::read_loose(const ObjectId& id) {
    Path obj_path = get_loose_path(id);
    
    auto data_result = Filesystem::read_file(obj_path);
    if (!data_result) {
        return data_result;
    }
    
    if (data_result->empty()) {
        return flux::unexpected("Invalid object: empty file");
    }
    
    // Skip type byte
    std::span<const uint8_t> compressed_data(data_result->data() + 1, data_result->size() - 1);
    
    // Decompress
    return Compression::decompress(compressed_data);
}

} // namespace flux
