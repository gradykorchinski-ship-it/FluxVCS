#pragma once

#include "flux/core/types.hpp"
#include "flux/core/object_id.hpp"
#include <string>
#include <vector>
#include <chrono>

namespace flux {

/**
 * Signature - Author or committer information
 */
struct Signature {
    std::string name;
    std::string email;
    std::chrono::system_clock::time_point timestamp;
    int timezone_offset = 0;  // Minutes from UTC
    
    Signature() = default;
    Signature(std::string name, std::string email);
    
    std::string format() const;
    static Result<Signature> parse(std::string_view str);
};

/**
 * Commit - Represents a snapshot in history
 */
class Commit {
public:
    Commit() = default;
    
    // Getters
    const ObjectId& tree() const { return tree_; }
    const std::vector<ObjectId>& parents() const { return parents_; }
    const Signature& author() const { return author_; }
    const Signature& committer() const { return committer_; }
    const std::string& message() const { return message_; }
    
    // Setters
    void set_tree(ObjectId tree) { tree_ = std::move(tree); }
    void add_parent(ObjectId parent) { parents_.push_back(std::move(parent)); }
    void set_author(Signature author) { author_ = std::move(author); }
    void set_committer(Signature committer) { committer_ = std::move(committer); }
    void set_message(std::string message) { message_ = std::move(message); }
    
    // Compute commit's object ID
    ObjectId compute_id(HashAlgorithm algo) const;
    
    // Serialize to FluxVCS format
    Bytes serialize() const;
    
    // Serialize to Git format
    Bytes serialize_git() const;
    
    // Deserialize commit
    static Result<Commit> deserialize(std::span<const uint8_t> data);
    
    // Check if this is a merge commit
    bool is_merge() const { return parents_.size() > 1; }
    
    // Check if this is the root commit
    bool is_root() const { return parents_.empty(); }
    
private:
    ObjectId tree_;                    // Root tree
    std::vector<ObjectId> parents_;    // Parent commits
    Signature author_;                 // Original author
    Signature committer_;              // Person who committed
    std::string message_;              // Commit message
};

} // namespace flux
