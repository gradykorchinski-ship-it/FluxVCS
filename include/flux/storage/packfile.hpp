#pragma once

#include "flux/core/types.hpp"
#include "flux/core/object_id.hpp"
#include <vector>
#include <map>

namespace flux {

// Forward declaration
class ObjectStore;

/**
 * Pack file for efficient object storage
 * Stores multiple objects in a single compressed file
 */
class PackFile {
public:
    struct PackedObject {
        ObjectId id;
        ObjectType type;
        uint64_t offset;      // Offset in pack file
        uint64_t size;        // Compressed size
        uint64_t original_size; // Uncompressed size
    };
    
    explicit PackFile(Path pack_path);
    
    // Create pack file from loose objects
    static Result<PackFile> create_from_objects(
        const Path& pack_dir,
        const std::vector<ObjectId>& objects,
        ObjectStore& store
    );
    
    // Read object from pack
    Result<Bytes> read_object(const ObjectId& id);
    
    // Check if pack contains object
    bool contains(const ObjectId& id) const;
    
    // Get pack statistics
    struct Stats {
        size_t object_count;
        size_t total_compressed_size;
        size_t total_original_size;
        double compression_ratio;
    };
    
    Stats get_stats() const;
    
    // Save/load pack index
    Result<void> save_index();
    Result<void> load_index();
    
    const Path& path() const { return pack_path_; }
    const std::vector<PackedObject>& objects() const { return objects_; }
    
private:
    Path pack_path_;
    Path index_path_;
    std::vector<PackedObject> objects_;
    std::map<ObjectId, size_t> object_index_;  // id -> index in objects_
    
    Result<void> write_pack_file(const std::vector<std::pair<ObjectId, Bytes>>& data);
};

/**
 * Manages pack files in repository
 */
class PackManager {
public:
    explicit PackManager(Path pack_dir);
    
    // Pack loose objects into pack file
    Result<void> pack_loose_objects(ObjectStore& store, size_t min_objects = 100);
    
    // Read object from any pack file
    Result<Bytes> read_object(const ObjectId& id);
    
    // Check if object exists in packs
    bool contains(const ObjectId& id) const;
    
    // Load all pack files
    Result<void> load_packs();
    
    // Get total statistics
    struct TotalStats {
        size_t pack_count;
        size_t total_objects;
        size_t total_compressed_size;
        size_t total_original_size;
    };
    
    TotalStats get_stats() const;
    
private:
    Path pack_dir_;
    std::vector<PackFile> packs_;
};

} // namespace flux
