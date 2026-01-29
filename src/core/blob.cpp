#include "flux/core/blob.hpp"
#include "flux/util/chunker.hpp"
#include "flux/util/hash.hpp"
#include <fmt/format.h>
#include <cstring>

namespace flux {

Blob::Blob(std::vector<Chunk> chunks) : chunks_(std::move(chunks)) {
}

Blob Blob::from_data(std::span<const uint8_t> data, HashAlgorithm algo) {
    if (algo == HashAlgorithm::SHA1) {
        // For Git compatibility, we don't chunk SHA1 blobs.
        // The blob ID will be the hash of the file content.
        ObjectId content_id = ObjectId::compute(algo, data, ObjectType::Blob);
        std::vector<Chunk> chunks;
        chunks.emplace_back(content_id, Bytes(data.begin(), data.end()));
        return Blob(std::move(chunks));
    }

    ContentChunker chunker;
    auto chunk_boundaries = chunker.chunk(data);
    
    std::vector<Chunk> chunks;
    chunks.reserve(chunk_boundaries.size());
    
    for (const auto& [offset, size] : chunk_boundaries) {
        std::span<const uint8_t> chunk_data(data.data() + offset, size);
        ObjectId chunk_id = ObjectId::compute(algo, chunk_data, ObjectType::Blob);
        
        chunks.emplace_back(chunk_id, Bytes(chunk_data.begin(), chunk_data.end()));
    }
    
    return Blob(std::move(chunks));
}

size_t Blob::total_size() const {
    size_t total = 0;
    for (const auto& chunk : chunks_) {
        total += chunk.size;
    }
    return total;
}

ObjectId Blob::compute_id(HashAlgorithm algo) const {
    if (algo == HashAlgorithm::SHA1 && chunks_.size() == 1) {
        return chunks_[0].id;
    }
    // Serialize chunk list and hash it (for internal chunked storage)
    Bytes serialized = serialize();
    return ObjectId::compute(algo, serialized, ObjectType::Blob);
}

Bytes Blob::serialize() const {
    // Format: <num_chunks><chunk1_id><chunk1_size><chunk2_id><chunk2_size>...
    Bytes result;
    
    // Write number of chunks (4 bytes)
    uint32_t num_chunks = chunks_.size();
    result.insert(result.end(), 
        reinterpret_cast<uint8_t*>(&num_chunks),
        reinterpret_cast<uint8_t*>(&num_chunks) + sizeof(num_chunks));
    
    for (const auto& chunk : chunks_) {
        // Write chunk ID hex string
        std::string id_hex = chunk.id.to_hex();
        uint32_t id_len = id_hex.size();
        
        result.insert(result.end(),
            reinterpret_cast<uint8_t*>(&id_len),
            reinterpret_cast<uint8_t*>(&id_len) + sizeof(id_len));
        
        result.insert(result.end(), id_hex.begin(), id_hex.end());
        
        // Write chunk size (8 bytes)
        uint64_t size = chunk.size;
        result.insert(result.end(),
            reinterpret_cast<uint8_t*>(&size),
            reinterpret_cast<uint8_t*>(&size) + sizeof(size));
    }
    
    return result;
}

Result<Blob> Blob::deserialize(std::span<const uint8_t> data) {
    // Try to parse as FluxVCS metadata (chunk list)
    // Minimal valid metadata is: 4 (num_chunks) + 4 (id_len) + 40 (sha1 hex) + 8 (size) = 56 bytes.
    if (data.size() >= 56) {
        size_t offset = 0;
        uint32_t num_chunks;
        std::memcpy(&num_chunks, data.data() + offset, sizeof(num_chunks));
        
        // Very basic heuristic: if num_chunks is reasonable and size could fit
        if (num_chunks > 0 && num_chunks < 1000000) {
            size_t expected_min_size = 4 + num_chunks * (4 + 40 + 8);
            if (data.size() >= expected_min_size) {
                // Looks like metadata, try to parse
                offset += sizeof(num_chunks);
                std::vector<Chunk> chunks;
                chunks.reserve(num_chunks);
                
                bool success = true;
                for (uint32_t i = 0; i < num_chunks; ++i) {
                    if (offset + 4 > data.size()) { success = false; break; }
                    uint32_t id_len;
                    std::memcpy(&id_len, data.data() + offset, sizeof(id_len));
                    offset += 4;
                    
                    if (offset + id_len + 8 > data.size()) { success = false; break; }
                    std::string_view id_hex(reinterpret_cast<const char*>(data.data() + offset), id_len);
                    auto id_result = ObjectId::from_hex(id_hex);
                    if (!id_result) { success = false; break; }
                    offset += id_len;
                    
                    uint64_t size;
                    std::memcpy(&size, data.data() + offset, sizeof(size));
                    offset += 8;
                    
                    chunks.emplace_back(*id_result, size);
                }
                
                if (success) {
                    return Blob(std::move(chunks));
                }
            }
        }
    }
    
    // Fallback: treat as raw content (single chunk)
    // Compute the content ID if possible, though it's often not needed for reconstruction
    std::vector<Chunk> chunks;
    chunks.emplace_back(ObjectId(), Bytes(data.begin(), data.end()));
    return Blob(std::move(chunks));
}

Bytes Blob::reconstruct() const {
    Bytes result;
    result.reserve(total_size());
    
    for (const auto& chunk : chunks_) {
        result.insert(result.end(), chunk.data.begin(), chunk.data.end());
    }
    
    return result;
}

} // namespace flux
