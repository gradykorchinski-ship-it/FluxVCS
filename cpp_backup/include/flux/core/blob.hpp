#pragma once

#include "flux/core/types.hpp"
#include "flux/core/object_id.hpp"
#include <vector>

namespace flux {

/**
 * Chunk - A content-defined piece of a file
 */
struct Chunk {
    ObjectId id;           // Hash of chunk data
    size_t size;           // Size in bytes
    Bytes data;            // Actual content (may be empty if not loaded)
    
    Chunk(ObjectId id, size_t size) : id(std::move(id)), size(size) {}
    Chunk(ObjectId id, Bytes data) 
        : id(std::move(id)), size(data.size()), data(std::move(data)) {}
};

/**
 * Blob - Represents file content
 * 
 * Unlike Git, FluxVCS stores files as a sequence of chunks
 * for better deduplication and efficient binary handling.
 */
class Blob {
public:
    Blob() = default;
    explicit Blob(std::vector<Chunk> chunks);
    
    // Create blob from file content
    static Blob from_data(std::span<const uint8_t> data, HashAlgorithm algo);
    
    // Get chunks
    const std::vector<Chunk>& chunks() const { return chunks_; }
    
    // Total size
    size_t total_size() const;
    
    // Compute blob's object ID (hash of chunk list)
    ObjectId compute_id(HashAlgorithm algo) const;
    
    // Serialize blob metadata (chunk list)
    Bytes serialize() const;
    
    // Deserialize blob metadata
    static Result<Blob> deserialize(std::span<const uint8_t> data);
    
    // Reconstruct full content from chunks
    Bytes reconstruct() const;
    
private:
    std::vector<Chunk> chunks_;
};

} // namespace flux
