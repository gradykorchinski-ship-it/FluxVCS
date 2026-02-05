#include "flux/core/commit.hpp"
#include "flux/util/hash.hpp"
#include <fmt/format.h>
#include <cstring>
#include <sstream>

namespace flux {

Signature::Signature(std::string name, std::string email)
    : name(std::move(name)), 
      email(std::move(email)),
      timestamp(std::chrono::system_clock::now()),
      timezone_offset(0) {
}

std::string Signature::format() const {
    auto time_t_val = std::chrono::system_clock::to_time_t(timestamp);
    return fmt::format("{} <{}> {} {:+03d}{:02d}",
        name, email, time_t_val,
        timezone_offset / 60, std::abs(timezone_offset % 60));
}

Result<Signature> Signature::parse(std::string_view str) {
    // Format: "Name <email> timestamp +offset"
    auto email_start = str.find('<');
    auto email_end = str.find('>');
    
    if (email_start == std::string_view::npos || email_end == std::string_view::npos) {
        return flux::unexpected("Invalid signature format");
    }
    
    Signature sig;
    sig.name = std::string(str.substr(0, email_start - 1));
    sig.email = std::string(str.substr(email_start + 1, email_end - email_start - 1));
    
    // Parse timestamp and timezone (simplified)
    auto remaining = str.substr(email_end + 2);
    std::istringstream iss{std::string{remaining}};
    
    std::time_t time_val;
    iss >> time_val;
    sig.timestamp = std::chrono::system_clock::from_time_t(time_val);
    
    // Parse timezone offset (optional)
    int tz_hours = 0, tz_mins = 0;
    char sign;
    if (iss >> sign >> tz_hours >> tz_mins) {
        sig.timezone_offset = (sign == '-' ? -1 : 1) * (tz_hours * 60 + tz_mins);
    }
    
    return sig;
}

ObjectId Commit::compute_id(HashAlgorithm algo) const {
    Bytes serialized = serialize();
    return ObjectId::compute(algo, serialized, ObjectType::Commit);
}

Bytes Commit::serialize() const {
    // Format: tree <tree_id>\nparent <parent1_id>\nparent <parent2_id>\n...
    //         author <author>\ncommitter <committer>\n\n<message>
    std::string result;
    
    result += fmt::format("tree {}\n", tree_.to_hex());
    
    for (const auto& parent : parents_) {
        result += fmt::format("parent {}\n", parent.to_hex());
    }
    
    result += fmt::format("author {}\n", author_.format());
    result += fmt::format("committer {}\n", committer_.format());
    result += "\n";
    result += message_;
    
    return Bytes(result.begin(), result.end());
}

Bytes Commit::serialize_git() const {
    std::string result;
    
    // Git format uses raw 40-char hex for SHA-1
    result += fmt::format("tree {}\n", Hash::to_hex(tree_.hash()));
    
    for (const auto& parent : parents_) {
        result += fmt::format("parent {}\n", Hash::to_hex(parent.hash()));
    }
    
    result += fmt::format("author {}\n", author_.format());
    result += fmt::format("committer {}\n", committer_.format());
    result += "\n";
    result += message_;
    if (result.empty() || result.back() != '\n') {
        result += '\n';
    }
    
    return Bytes(result.begin(), result.end());
}

Result<Commit> Commit::deserialize(std::span<const uint8_t> data) {
    std::string_view content(reinterpret_cast<const char*>(data.data()), data.size());
    
    Commit commit;
    size_t pos = 0;
    
    // Parse headers
    while (pos < content.size()) {
        auto line_end = content.find('\n', pos);
        if (line_end == std::string_view::npos) {
            break;
        }
        
        auto line = content.substr(pos, line_end - pos);
        pos = line_end + 1;
        
        if (line.empty()) {
            // Empty line marks end of headers
            break;
        }
        
        auto space_pos = line.find(' ');
        if (space_pos == std::string_view::npos) {
            return flux::unexpected("Invalid commit format");
        }
        
        auto key = line.substr(0, space_pos);
        auto value = line.substr(space_pos + 1);
        
        if (key == "tree") {
            auto tree_result = ObjectId::from_hex(value);
            if (!tree_result) {
                return flux::unexpected(tree_result.error());
            }
            commit.tree_ = *tree_result;
        } else if (key == "parent") {
            auto parent_result = ObjectId::from_hex(value);
            if (!parent_result) {
                return flux::unexpected(parent_result.error());
            }
            commit.parents_.push_back(*parent_result);
        } else if (key == "author") {
            auto author_result = Signature::parse(value);
            if (!author_result) {
                return flux::unexpected(author_result.error());
            }
            commit.author_ = *author_result;
        } else if (key == "committer") {
            auto committer_result = Signature::parse(value);
            if (!committer_result) {
                return flux::unexpected(committer_result.error());
            }
            commit.committer_ = *committer_result;
        }
    }
    
    // Rest is message
    commit.message_ = std::string(content.substr(pos));
    
    return commit;
}

} // namespace flux
