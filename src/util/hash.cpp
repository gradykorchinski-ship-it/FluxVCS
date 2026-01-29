#include "flux/util/hash.hpp"
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <fmt/format.h>
#include <stdexcept>

namespace flux {

Bytes Hash::compute(HashAlgorithm algo, std::span<const uint8_t> data) {
    switch (algo) {
        case HashAlgorithm::SHA1:
            return compute_sha1(data);
        case HashAlgorithm::SHA256:
            return compute_sha256(data);
        case HashAlgorithm::BLAKE3:
        case HashAlgorithm::SHA3_256:
            // Not yet implemented
            return compute_sha256(data);
    }
    return compute_sha256(data);
}

Bytes Hash::compute_sha1(std::span<const uint8_t> data) {
    Bytes hash(SHA_DIGEST_LENGTH);
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, data.data(), data.size());
    SHA1_Final(hash.data(), &ctx);
    return hash;
}

Bytes Hash::compute_sha256(std::span<const uint8_t> data) {
    Bytes hash(SHA256_DIGEST_LENGTH);
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data.data(), data.size());
    SHA256_Final(hash.data(), &ctx);
    return hash;
}

size_t Hash::hash_size(HashAlgorithm algo) {
    switch (algo) {
        case HashAlgorithm::SHA1: return 20;  // Git compatibility
        case HashAlgorithm::SHA256: return 32;
        case HashAlgorithm::BLAKE3: return 32;
        case HashAlgorithm::SHA3_256: return 32;
    }
    return 32;  // Default
}

std::string Hash::to_hex(std::span<const uint8_t> data) {
    std::string result;
    result.reserve(data.size() * 2);
    
    for (uint8_t byte : data) {
        result += fmt::format("{:02x}", byte);
    }
    
    return result;
}

Result<Bytes> Hash::from_hex(std::string_view hex) {
    if (hex.size() % 2 != 0) {
        return flux::unexpected("Hex string must have even length");
    }
    
    Bytes result;
    result.reserve(hex.size() / 2);
    
    for (size_t i = 0; i < hex.size(); i += 2) {
        char high = hex[i];
        char low = hex[i + 1];
        
        auto hex_to_int = [](char c) -> std::optional<uint8_t> {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return std::nullopt;
        };
        
        auto h = hex_to_int(high);
        auto l = hex_to_int(low);
        
        if (!h || !l) {
            return flux::unexpected(fmt::format("Invalid hex character at position {}", i));
        }
        
        result.push_back((*h << 4) | *l);
    }
    
    return result;
}

} // namespace flux
