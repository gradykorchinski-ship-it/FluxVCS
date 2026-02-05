#include "flux/storage/packfile.hpp"
#include "flux/storage/object_store.hpp"
#include "flux/util/compression.hpp"
#include "flux/util/filesystem.hpp"
#include <fmt/format.h>
#include <fstream>
#include <sstream>

namespace flux {

PackFile::PackFile(Path pack_path)
    : pack_path_(std::move(pack_path))
    , index_path_(pack_path_.string() + ".idx") {}

Result<PackFile> PackFile::create_from_objects(
    const Path& pack_dir,
    const std::vector<ObjectId>& objects,
    ObjectStore& store
) {
    // Create pack file name based on timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    Path pack_path = pack_dir / fmt::format("pack-{:016x}.pack", timestamp);
    PackFile pack(pack_path);
    
    // Read all objects and prepare data
    std::vector<std::pair<ObjectId, Bytes>> object_data;
    for (const auto& id : objects) {
        auto data_result = store.read(id);
        if (data_result) {
            object_data.emplace_back(id, *data_result);
        }
    }
    
    // Write pack file
    auto write_result = pack.write_pack_file(object_data);
    if (!write_result) {
        return flux::unexpected(write_result.error());
    }
    
    // Save index
    auto index_result = pack.save_index();
    if (!index_result) {
        return flux::unexpected(index_result.error());
    }
    
    return pack;
}

Result<Bytes> PackFile::read_object(const ObjectId& id) {
    auto it = object_index_.find(id);
    if (it == object_index_.end()) {
        return flux::unexpected("Object not found in pack");
    }
    
    const auto& obj = objects_[it->second];
    
    // Read from pack file
    std::ifstream file(pack_path_, std::ios::binary);
    if (!file) {
        return flux::unexpected("Failed to open pack file");
    }
    
    file.seekg(obj.offset);
    Bytes compressed(obj.size);
    file.read(reinterpret_cast<char*>(compressed.data()), obj.size);
    
    if (!file) {
        return flux::unexpected("Failed to read from pack file");
    }
    
    // Decompress
    return Compression::decompress(compressed);
}

bool PackFile::contains(const ObjectId& id) const {
    return object_index_.count(id) > 0;
}

PackFile::Stats PackFile::get_stats() const {
    Stats stats{};
    stats.object_count = objects_.size();
    stats.total_compressed_size = 0;
    stats.total_original_size = 0;
    
    for (const auto& obj : objects_) {
        stats.total_compressed_size += obj.size;
        stats.total_original_size += obj.original_size;
    }
    
    if (stats.total_original_size > 0) {
        stats.compression_ratio = static_cast<double>(stats.total_compressed_size) / 
                                 stats.total_original_size;
    } else {
        stats.compression_ratio = 1.0;
    }
    
    return stats;
}

Result<void> PackFile::save_index() {
    std::ofstream file(index_path_, std::ios::binary);
    if (!file) {
        return flux::unexpected("Failed to create index file");
    }
    
    // Write header
    uint32_t magic = 0x50414358;  // "PACX"
    uint32_t version = 1;
    uint32_t count = objects_.size();
    
    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));
    
    // Write objects
    for (const auto& obj : objects_) {
        // Write object ID
        auto hash = obj.id.hash();
        file.write(reinterpret_cast<const char*>(hash.data()), hash.size());
        
        // Write metadata
        uint8_t type = static_cast<uint8_t>(obj.type);
        file.write(reinterpret_cast<const char*>(&type), sizeof(type));
        file.write(reinterpret_cast<const char*>(&obj.offset), sizeof(obj.offset));
        file.write(reinterpret_cast<const char*>(&obj.size), sizeof(obj.size));
        file.write(reinterpret_cast<const char*>(&obj.original_size), sizeof(obj.original_size));
    }
    
    return {};
}

Result<void> PackFile::load_index() {
    std::ifstream file(index_path_, std::ios::binary);
    if (!file) {
        return flux::unexpected("Failed to open index file");
    }
    
    // Read header
    uint32_t magic, version, count;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    file.read(reinterpret_cast<char*>(&count), sizeof(count));
    
    if (magic != 0x50414358) {
        return flux::unexpected("Invalid pack index magic");
    }
    
    objects_.clear();
    object_index_.clear();
    
    // Read objects
    for (uint32_t i = 0; i < count; ++i) {
        PackedObject obj;
        
        // Read object ID (assume SHA256 for now)
        Bytes hash(32);
        file.read(reinterpret_cast<char*>(hash.data()), 32);
        obj.id = ObjectId::from_hash(HashAlgorithm::SHA256, hash);
        
        // Read metadata
        uint8_t type;
        file.read(reinterpret_cast<char*>(&type), sizeof(type));
        obj.type = static_cast<ObjectType>(type);
        file.read(reinterpret_cast<char*>(&obj.offset), sizeof(obj.offset));
        file.read(reinterpret_cast<char*>(&obj.size), sizeof(obj.size));
        file.read(reinterpret_cast<char*>(&obj.original_size), sizeof(obj.original_size));
        
        object_index_[obj.id] = objects_.size();
        objects_.push_back(obj);
    }
    
    return {};
}

Result<void> PackFile::write_pack_file(const std::vector<std::pair<ObjectId, Bytes>>& data) {
    std::ofstream file(pack_path_, std::ios::binary);
    if (!file) {
        return flux::unexpected("Failed to create pack file");
    }
    
    // Write header
    uint32_t magic = 0x5041434B;  // "PACK"
    uint32_t version = 1;
    uint32_t count = data.size();
    
    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));
    
    uint64_t offset = sizeof(magic) + sizeof(version) + sizeof(count);
    
    // Write objects
    for (const auto& [id, obj_data] : data) {
        // Compress data
        auto compressed_result = Compression::compress(obj_data);
        if (!compressed_result) {
            return flux::unexpected(compressed_result.error());
        }
        
        const auto& compressed = *compressed_result;
        
        // Write compressed data
        file.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
        
        // Record in index
        PackedObject obj;
        obj.id = id;
        obj.type = ObjectType::Blob;  // Simplified - would need to get actual type
        obj.offset = offset;
        obj.size = compressed.size();
        obj.original_size = obj_data.size();
        
        object_index_[id] = objects_.size();
        objects_.push_back(obj);
        
        offset += compressed.size();
    }
    
    return {};
}

// PackManager implementation

PackManager::PackManager(Path pack_dir)
    : pack_dir_(std::move(pack_dir)) {}

Result<void> PackManager::pack_loose_objects(ObjectStore& /*store*/, size_t /*min_objects*/) {
    // This would scan for loose objects and pack them
    // Simplified implementation
    return {};
}

Result<Bytes> PackManager::read_object(const ObjectId& id) {
    for (auto& pack : packs_) {
        if (pack.contains(id)) {
            return pack.read_object(id);
        }
    }
    
    return flux::unexpected("Object not found in any pack");
}

bool PackManager::contains(const ObjectId& id) const {
    for (const auto& pack : packs_) {
        if (pack.contains(id)) {
            return true;
        }
    }
    return false;
}

Result<void> PackManager::load_packs() {
    if (!Filesystem::exists(pack_dir_)) {
        return {};
    }
    
    auto entries_result = Filesystem::list_directory(pack_dir_);
    if (!entries_result) {
        return flux::unexpected(entries_result.error());
    }
    
    for (const auto& entry : *entries_result) {
        if (entry.extension() == ".pack") {
            PackFile pack(entry);
            auto load_result = pack.load_index();
            if (load_result) {
                packs_.push_back(std::move(pack));
            }
        }
    }
    
    return {};
}

PackManager::TotalStats PackManager::get_stats() const {
    TotalStats stats{};
    stats.pack_count = packs_.size();
    stats.total_objects = 0;
    stats.total_compressed_size = 0;
    stats.total_original_size = 0;
    
    for (const auto& pack : packs_) {
        auto pack_stats = pack.get_stats();
        stats.total_objects += pack_stats.object_count;
        stats.total_compressed_size += pack_stats.total_compressed_size;
        stats.total_original_size += pack_stats.total_original_size;
    }
    
    return stats;
}

} // namespace flux
