#pragma once

#include "flux/core/types.hpp"
#include <span>

namespace flux {

/**
 * ContentChunker - Splits content into variable-sized chunks
 * 
 * Uses Rabin fingerprinting (rolling hash) for content-defined chunking.
 * This ensures that small changes don't shift all chunk boundaries.
 */
class ContentChunker {
public:
    static constexpr size_t AVG_CHUNK_SIZE = 64 * 1024;      // 64 KB
    static constexpr size_t MIN_CHUNK_SIZE = 16 * 1024;      // 16 KB
    static constexpr size_t MAX_CHUNK_SIZE = 256 * 1024;     // 256 KB
    
    ContentChunker() = default;
    
    /**
     * Split data into chunks using rolling hash
     * Returns vector of (offset, size) pairs
     */
    std::vector<std::pair<size_t, size_t>> chunk(std::span<const uint8_t> data);
    
private:
    static constexpr uint64_t POLYNOMIAL = 0x3DA3358B4DC173;
    static constexpr uint64_t WINDOW_SIZE = 64;
    
    uint64_t rolling_hash(std::span<const uint8_t> window);
    bool is_chunk_boundary(uint64_t hash);
};

} // namespace flux
