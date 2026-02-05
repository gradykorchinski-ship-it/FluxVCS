#pragma once

#include "flux/core/types.hpp"
#include <span>
#include <compare>

namespace flux {

/**
 * ObjectId - Algorithm-agile content identifier
 * 
 * Unlike Git's SHA-1 based object IDs, FluxVCS supports multiple
 * hash algorithms and can migrate between them.
 */
class ObjectId {
public:
    // Constructors
    ObjectId() = default;
    ObjectId(HashAlgorithm algo, Bytes hash);
    
    // Compute object ID from data
    static ObjectId compute(HashAlgorithm algo, std::span<const uint8_t> data, std::optional<ObjectType> type = std::nullopt);
    
    // Create from raw hash bytes
    static ObjectId from_hash(HashAlgorithm algo, Bytes hash) {
        return ObjectId(algo, std::move(hash));
    }
    
    // Parse from hex string (format: "algo:hash")
    static Result<ObjectId> from_hex(std::string_view hex);
    
    // Serialize to hex string
    std::string to_hex() const;
    
    // Accessors
    HashAlgorithm algorithm() const { return algorithm_; }
    const Bytes& hash() const { return hash_; }
    size_t size() const { return hash_.size(); }
    bool is_valid() const { return !hash_.empty(); }
    
    // Comparison operators
    auto operator<=>(const ObjectId& other) const = default;
    
    // Hash for use in unordered containers
    size_t hash_code() const;
    
private:
    HashAlgorithm algorithm_ = HashAlgorithm::SHA256;
    Bytes hash_;
};

} // namespace flux

// Hash specialization for std::unordered_map
namespace std {
    template<>
    struct hash<flux::ObjectId> {
        size_t operator()(const flux::ObjectId& id) const {
            return id.hash_code();
        }
    };
}
