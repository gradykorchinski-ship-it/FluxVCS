#include "flux/core/types.hpp"
#include <fmt/format.h>

namespace flux {

std::string Error::format() const {
    std::string result = fmt::format("Error: {}", message);
    
    if (!suggestion.empty()) {
        result += fmt::format("\nSuggestion: {}", suggestion);
    }
    
    if (recovery_command) {
        result += fmt::format("\nTry: flux {}", *recovery_command);
    }
    
    return result;
}

std::string to_string(HashAlgorithm algo) {
    switch (algo) {
        case HashAlgorithm::SHA1: return "sha1";
        case HashAlgorithm::SHA256: return "sha256";
        case HashAlgorithm::BLAKE3: return "blake3";
        case HashAlgorithm::SHA3_256: return "sha3-256";
        default: return "unknown";
    }
}

std::string_view to_string(ObjectType type) {
    switch (type) {
        case ObjectType::Blob: return "blob";
        case ObjectType::Tree: return "tree";
        case ObjectType::Commit: return "commit";
        case ObjectType::Tag: return "tag";
    }
    return "unknown";
}

std::string_view to_string(FileMode mode) {
    switch (mode) {
        case FileMode::Regular: return "100644";
        case FileMode::Executable: return "100755";
        case FileMode::Symlink: return "120000";
        case FileMode::Directory: return "040000";
    }
    return "unknown";
}

std::optional<HashAlgorithm> parse_hash_algorithm(std::string_view str) {
    if (str == "sha1") return HashAlgorithm::SHA1;
    if (str == "sha256") return HashAlgorithm::SHA256;
    if (str == "blake3") return HashAlgorithm::BLAKE3;
    if (str == "sha3-256") return HashAlgorithm::SHA3_256;
    return std::nullopt;
}

std::optional<ObjectType> parse_object_type(std::string_view str) {
    if (str == "blob") return ObjectType::Blob;
    if (str == "tree") return ObjectType::Tree;
    if (str == "commit") return ObjectType::Commit;
    if (str == "tag") return ObjectType::Tag;
    return std::nullopt;
}

} // namespace flux
