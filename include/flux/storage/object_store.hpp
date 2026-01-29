#pragma once

#include "flux/core/types.hpp"
#include "flux/core/object_id.hpp"

namespace flux {

/**
 * ObjectStore - Content-addressed object storage
 * 
 * Stores objects (blobs, trees, commits) in .flux/objects/
 * Uses loose objects for recent writes and pack files for efficiency.
 */
class ObjectStore {
public:
    explicit ObjectStore(Path objects_dir);
    
    // Write object to store
    Result<ObjectId> write(ObjectType type, std::span<const uint8_t> data, HashAlgorithm algo);
    
    // Read object from store
    Result<Bytes> read(const ObjectId& id);
    
    // Check if object exists
    bool exists(const ObjectId& id);
    
    // Get object type
    Result<ObjectType> get_type(const ObjectId& id);
    
    // Delete object (dangerous, used for cleanup)
    Result<void> remove(const ObjectId& id);
    
private:
    Path get_object_path(const ObjectId& id) const;
    Path get_loose_path(const ObjectId& id) const;
    
    Result<void> write_loose(const ObjectId& id, ObjectType type, std::span<const uint8_t> data);
    Result<Bytes> read_loose(const ObjectId& id);
    
    Path objects_dir_;
};

} // namespace flux
