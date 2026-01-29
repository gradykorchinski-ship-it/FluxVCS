#pragma once

#include "flux/core/types.hpp"
#include "flux/core/object_id.hpp"
#include <string>
#include <vector>
#include <map>

namespace flux {

/**
 * TreeEntry - Single entry in a tree (file or subdirectory)
 */
struct TreeEntry {
    std::string name;      // Filename or directory name
    FileMode mode;         // File mode
    ObjectId id;           // Object ID (blob or tree)
    
    TreeEntry(std::string name, FileMode mode, ObjectId id)
        : name(std::move(name)), mode(mode), id(std::move(id)) {}
    
    bool is_directory() const { return mode == FileMode::Directory; }
    bool is_executable() const { return mode == FileMode::Executable; }
    bool is_symlink() const { return mode == FileMode::Symlink; }
};

/**
 * Tree - Represents a directory
 * 
 * Contains entries for files and subdirectories.
 * Entries are sorted by name for deterministic hashing.
 */
class Tree {
public:
    Tree() = default;
    explicit Tree(std::vector<TreeEntry> entries);
    
    // Add entry (maintains sorted order)
    void add_entry(TreeEntry entry);
    
    // Get entries
    const std::vector<TreeEntry>& entries() const { return entries_; }
    
    // Find entry by name
    const TreeEntry* find(const std::string& name) const;
    
    // Compute tree's object ID
    ObjectId compute_id(HashAlgorithm algo) const;
    
    // Serialize to FluxVCS format
    Bytes serialize() const;
    
    // Serialize to Git binary tree format
    Bytes serialize_git() const;
    
    // Deserialize tree
    static Result<Tree> deserialize(std::span<const uint8_t> data);
    
private:
    std::vector<TreeEntry> entries_;  // Sorted by name
    
    void sort_entries();
};

} // namespace flux
