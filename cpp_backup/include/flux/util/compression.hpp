#pragma once

#include "flux/core/types.hpp"
#include <span>

namespace flux {

/**
 * Compression utilities using zstd
 */
class Compression {
public:
    // Compression level (1-22, higher = better compression but slower)
    static constexpr int DEFAULT_LEVEL = 3;
    
    // Compress data
    static Result<Bytes> compress(std::span<const uint8_t> data, int level = DEFAULT_LEVEL);
    
    // Decompress data
    static Result<Bytes> decompress(std::span<const uint8_t> data);
    
    // Get compressed size estimate
    static size_t compress_bound(size_t size);
};

} // namespace flux
