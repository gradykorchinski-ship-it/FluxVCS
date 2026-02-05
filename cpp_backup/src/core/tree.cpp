#include "flux/core/tree.hpp"
#include "flux/util/hash.hpp"
#include <algorithm>
#include <fmt/format.h>
#include <cstring>

namespace flux {

Tree::Tree(std::vector<TreeEntry> entries) : entries_(std::move(entries)) {
    sort_entries();
}

void Tree::add_entry(TreeEntry entry) {
    entries_.push_back(std::move(entry));
    sort_entries();
}

void Tree::sort_entries() {
    std::sort(entries_.begin(), entries_.end(),
        [](const TreeEntry& a, const TreeEntry& b) {
            return a.name < b.name;
        });
}

const TreeEntry* Tree::find(const std::string& name) const {
    auto it = std::find_if(entries_.begin(), entries_.end(),
        [&name](const TreeEntry& entry) {
            return entry.name == name;
        });
    
    return it != entries_.end() ? &(*it) : nullptr;
}

ObjectId Tree::compute_id(HashAlgorithm algo) const {
    Bytes serialized = serialize();
    return ObjectId::compute(algo, serialized, ObjectType::Tree);
}

Bytes Tree::serialize() const {
    Bytes result;
    uint32_t num_entries = entries_.size();
    result.insert(result.end(),
        reinterpret_cast<const uint8_t*>(&num_entries),
        reinterpret_cast<const uint8_t*>(&num_entries) + sizeof(num_entries));
    
    for (const auto& entry : entries_) {
        uint32_t name_len = entry.name.size();
        result.insert(result.end(),
            reinterpret_cast<const uint8_t*>(&name_len),
            reinterpret_cast<const uint8_t*>(&name_len) + sizeof(name_len));
        result.insert(result.end(), entry.name.begin(), entry.name.end());
        
        uint32_t mode = static_cast<uint32_t>(entry.mode);
        result.insert(result.end(),
            reinterpret_cast<const uint8_t*>(&mode),
            reinterpret_cast<const uint8_t*>(&mode) + sizeof(mode));
        
        std::string id_hex = entry.id.to_hex();
        uint32_t id_len = id_hex.size();
        result.insert(result.end(),
            reinterpret_cast<const uint8_t*>(&id_len),
            reinterpret_cast<const uint8_t*>(&id_len) + sizeof(id_len));
        result.insert(result.end(), id_hex.begin(), id_hex.end());
    }
    return result;
}

Bytes Tree::serialize_git() const {
    Bytes result;
    for (const auto& entry : entries_) {
        // Mode in octal string
        uint32_t mode_val = (entry.mode == FileMode::Directory) ? 0040000 : 0100644;
        std::string mode_str = fmt::format("{:o}", mode_val);
        
        result.insert(result.end(), mode_str.begin(), mode_str.end());
        result.push_back(' ');
        result.insert(result.end(), entry.name.begin(), entry.name.end());
        result.push_back('\0');
        
        // Raw 20-byte hash
        Bytes hash = entry.id.hash();
        result.insert(result.end(), hash.begin(), hash.end());
    }
    return result;
}

Result<Tree> Tree::deserialize(std::span<const uint8_t> data) {
    if (data.empty()) return Tree(std::vector<TreeEntry>{});

    // Detect Git tree format: starts with mode digits followed by space
    bool is_git_tree = false;
    size_t first_space = 0;
    while (first_space < data.size() && data[first_space] != ' ') first_space++;
    if (first_space > 0 && first_space < 10) {
        is_git_tree = true;
        for (size_t i = 0; i < first_space; ++i) {
            if (!std::isdigit(data[i])) {
                is_git_tree = false;
                break;
            }
        }
    }

    if (is_git_tree) {
        std::vector<TreeEntry> entries;
        size_t pos = 0;
        while (pos < data.size()) {
            size_t space = pos;
            while (space < data.size() && data[space] != ' ') space++;
            if (space >= data.size()) break;

            std::string mode_str(reinterpret_cast<const char*>(data.data() + pos), space - pos);
            uint32_t mode_int = 0;
            try {
                mode_int = std::stoul(mode_str, nullptr, 8);
            } catch (...) { break; }
            
            FileMode mode = (mode_int == 0040000) ? FileMode::Directory : FileMode::Regular;

            size_t null_byte = space + 1;
            while (null_byte < data.size() && data[null_byte] != '\0') null_byte++;
            if (null_byte >= data.size()) break;

            std::string name(reinterpret_cast<const char*>(data.data() + space + 1), null_byte - (space + 1));
            pos = null_byte + 1;

            if (pos + 20 > data.size()) break;
            Bytes hash(20);
            std::copy(data.data() + pos, data.data() + pos + 20, hash.begin());
            entries.emplace_back(std::move(name), mode, ObjectId(HashAlgorithm::SHA1, std::move(hash)));
            pos += 20;
        }
        return Tree(std::move(entries));
    }

    if (data.size() < sizeof(uint32_t)) return flux::unexpected("Invalid tree data: too short");
    
    size_t offset = 0;
    uint32_t num_entries;
    std::memcpy(&num_entries, data.data() + offset, sizeof(num_entries));
    offset += sizeof(num_entries);
    
    std::vector<TreeEntry> entries;
    entries.reserve(num_entries);
    
    for (uint32_t i = 0; i < num_entries; ++i) {
        if (offset + sizeof(uint32_t) > data.size()) return flux::unexpected("Truncated tree data");
        uint32_t name_len;
        std::memcpy(&name_len, data.data() + offset, sizeof(name_len));
        offset += sizeof(name_len);
        
        if (offset + name_len > data.size()) return flux::unexpected("Truncated name");
        std::string name(reinterpret_cast<const char*>(data.data() + offset), name_len);
        offset += name_len;
        
        if (offset + sizeof(uint32_t) > data.size()) return flux::unexpected("Truncated mode");
        uint32_t mode_val;
        std::memcpy(&mode_val, data.data() + offset, sizeof(mode_val));
        offset += sizeof(mode_val);
        
        if (offset + sizeof(uint32_t) > data.size()) return flux::unexpected("Truncated ID length");
        uint32_t id_len;
        std::memcpy(&id_len, data.data() + offset, sizeof(id_len));
        offset += sizeof(id_len);
        
        if (offset + id_len > data.size()) return flux::unexpected("Truncated ID");
        std::string_view id_hex(reinterpret_cast<const char*>(data.data() + offset), id_len);
        auto id_result = ObjectId::from_hex(id_hex);
        if (!id_result) return flux::unexpected(id_result.error());
        offset += id_len;
        
        entries.emplace_back(std::move(name), static_cast<FileMode>(mode_val), *id_result);
    }
    return Tree(std::move(entries));
}

} // namespace flux
