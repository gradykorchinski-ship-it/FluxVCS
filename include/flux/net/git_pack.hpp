#pragma once

#include "flux/core/types.hpp"
#include "flux/core/object_id.hpp"
#include <vector>
#include <map>
#include <span>

namespace flux {

class Repository;
class ObjectStore;

/**
 * GitPackUnpacker - Unpacks Git binary pack files
 */
class GitPackUnpacker {
public:
    explicit GitPackUnpacker(Repository& repo);

    /**
     * Unpack a Git pack file into the repository's object store
     * @param data Raw pack file bytes
     * @return Number of objects unpacked or error
     */
    Result<size_t> unpack(std::span<const uint8_t> data);

private:
    Repository& repo_;
    
    struct RawObject {
        uint64_t offset;
        uint8_t type;
        Bytes data;
        std::optional<ObjectId> base_id; // For REF_DELTA
        std::optional<uint64_t> base_offset; // For OFS_DELTA
    };

    Result<RawObject> read_object(std::span<const uint8_t> data, size_t& pos);
    
    // Resolve deltas and write objects to store
    Result<size_t> resolve_and_store(std::vector<RawObject>& objects);
    
    // helper for delta resolution
    Result<Bytes> apply_delta(std::span<const uint8_t> base, std::span<const uint8_t> delta);
};

/**
 * GitPackPacker - Creates Git binary pack files
 */
class GitPackPacker {
public:
    explicit GitPackPacker(Repository& repo);

    /**
     * Create a Git-compatible pack file from a list of objects
     * @param object_ids List of objects to include
     * @return Raw pack file bytes
     */
    Result<Bytes> create_pack(const std::vector<ObjectId>& object_ids);

private:
    Repository& repo_;
    
    // Encodes an object header (type + size) in Git format
    std::vector<uint8_t> encode_object_header(uint8_t type, uint64_t size);
};

} // namespace flux
