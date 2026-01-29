#pragma once

#include "flux/core/types.hpp"
#include <span>

namespace flux {

/**
 * Hash utilities for computing object IDs
 */
class Hash {
public:
    // Compute hash using specified algorithm
    static Bytes compute(HashAlgorithm algo, std::span<const uint8_t> data);
    
    // Get hash size for algorithm
    static size_t hash_size(HashAlgorithm algo);
    
    // Convert bytes to hex string
    static std::string to_hex(std::span<const uint8_t> data);
    
    // Convert hex string to bytes
    static Result<Bytes> from_hex(std::string_view hex);

    static Bytes compute_sha1(std::span<const uint8_t> data);
    
private:
    static Bytes compute_sha256(std::span<const uint8_t> data);
};

} // namespace flux
