#include "flux/core/object_id.hpp"
#include "flux/util/hash.hpp"
#include <fmt/format.h>

namespace flux {

ObjectId::ObjectId(HashAlgorithm algo, Bytes hash)
    : algorithm_(algo), hash_(std::move(hash)) {
}

ObjectId ObjectId::compute(HashAlgorithm algo, std::span<const uint8_t> data, std::optional<ObjectType> type) {
    if (algo == HashAlgorithm::SHA1 && type) {
        // Git-style hashing: "type size\0data"
        std::string header = fmt::format("{} {}", to_string(*type), data.size());
        Bytes hashing_data;
        hashing_data.reserve(header.size() + 1 + data.size());
        hashing_data.insert(hashing_data.end(), header.begin(), header.end());
        hashing_data.push_back('\0');
        hashing_data.insert(hashing_data.end(), data.begin(), data.end());
        
        return ObjectId(algo, Hash::compute(algo, hashing_data));
    }
    return ObjectId(algo, Hash::compute(algo, data));
}

Result<ObjectId> ObjectId::from_hex(std::string_view hex) {
    auto colon_pos = hex.find(':');
    
    if (colon_pos == std::string_view::npos) {
        // Support raw hex for Git/compatibility
        if (hex.size() == 40) {
            auto hash_result = Hash::from_hex(hex);
            if (!hash_result) return flux::unexpected(hash_result.error());
            return ObjectId(HashAlgorithm::SHA1, std::move(*hash_result));
        } else if (hex.size() == 64) {
             auto hash_result = Hash::from_hex(hex);
            if (!hash_result) return flux::unexpected(hash_result.error());
            return ObjectId(HashAlgorithm::SHA256, std::move(*hash_result));
        }
        return flux::unexpected("Invalid object ID format, expected 'algo:hash'");
    }
    
    auto algo_str = hex.substr(0, colon_pos);
    auto hash_str = hex.substr(colon_pos + 1);
    
    auto algo = parse_hash_algorithm(algo_str);
    if (!algo) {
        return flux::unexpected(fmt::format("Unknown hash algorithm: {}", algo_str));
    }
    
    auto hash_result = Hash::from_hex(hash_str);
    if (!hash_result) {
        return flux::unexpected(hash_result.error());
    }
    
    return ObjectId(*algo, std::move(*hash_result));
}

std::string ObjectId::to_hex() const {
    return fmt::format("{}:{}", to_string(algorithm_), Hash::to_hex(hash_));
}

size_t ObjectId::hash_code() const {
    size_t h = static_cast<size_t>(algorithm_);
    for (size_t i = 0; i < std::min(hash_.size(), size_t(8)); ++i) {
        h ^= static_cast<size_t>(hash_[i]) << (i * 8);
    }
    return h;
}

} // namespace flux
