#include "flux/storage/snapshot.hpp"
#include "flux/core/repository.hpp"
#include "flux/storage/ref_store.hpp"
#include "flux/util/filesystem.hpp"
#include <fmt/format.h>
#include <sstream>

namespace flux {

Snapshot::Snapshot(uint64_t id, std::chrono::system_clock::time_point timestamp)
    : id_(id), timestamp_(timestamp) {}

Result<Snapshot> Snapshot::create(Repository& repo, const std::string& description) {
    auto now = std::chrono::system_clock::now();
    auto id = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    Snapshot snapshot(id, now);
    snapshot.description_ = description;
    
    // Capture HEAD commit
    auto head_result = repo.head();
    if (!head_result) {
        return flux::unexpected("Failed to get HEAD");
    }
    
    auto head_ref = *head_result;
    auto head_commit_result = repo.refs().read(head_ref);
    if (head_commit_result) {
        snapshot.head_commit_ = *head_commit_result;
    }
    
    // Capture all references
    auto refs_result = repo.refs().list();
    if (!refs_result) {
        return flux::unexpected(refs_result.error());
    }
    
    for (const auto& ref_name : *refs_result) {
        RefSnapshot ref_snap;
        ref_snap.name = ref_name;
        
        // Check if symbolic
        auto symbolic_result = repo.refs().read_symbolic(ref_name);
        if (symbolic_result) {
            ref_snap.is_symbolic = true;
            ref_snap.symbolic_target = *symbolic_result;
        } else {
            ref_snap.is_symbolic = false;
            auto target_result = repo.refs().read(ref_name);
            if (target_result) {
                ref_snap.target = *target_result;
            }
        }
        
        snapshot.refs_[ref_name] = ref_snap;
    }
    
    // Save snapshot
    auto save_result = snapshot.save(repo.flux_dir() / "snapshots");
    if (!save_result) {
        return flux::unexpected(save_result.error());
    }
    
    return snapshot;
}

Result<void> Snapshot::restore(Repository& repo) const {
    // Restore all references
    for (const auto& [name, ref_snap] : refs_) {
        if (ref_snap.is_symbolic) {
            auto result = repo.refs().write_symbolic(name, ref_snap.symbolic_target);
            if (!result) {
                return result;
            }
        } else {
            auto result = repo.refs().write(name, ref_snap.target);
            if (!result) {
                return result;
            }
        }
    }
    
    return {};
}

Result<void> Snapshot::save(const Path& snapshot_dir) const {
    auto dir_result = Filesystem::create_directories(snapshot_dir);
    if (!dir_result) {
        return dir_result;
    }
    
    Path snapshot_file = snapshot_dir / fmt::format("snapshot_{:016x}", id_);
    Bytes data = serialize();
    
    return Filesystem::write_file(snapshot_file, data);
}

Result<Snapshot> Snapshot::load(const Path& snapshot_path) {
    auto data_result = Filesystem::read_file(snapshot_path);
    if (!data_result) {
        return flux::unexpected(data_result.error());
    }
    
    return deserialize(*data_result);
}

Result<std::vector<uint64_t>> Snapshot::prune_old(const Path& snapshot_dir, std::chrono::hours max_age) {
    if (!Filesystem::exists(snapshot_dir)) {
        return std::vector<uint64_t>{};
    }
    
    auto entries_result = Filesystem::list_directory(snapshot_dir);
    if (!entries_result) {
        return flux::unexpected(entries_result.error());
    }
    
    auto now = std::chrono::system_clock::now();
    std::vector<uint64_t> pruned;
    
    for (const auto& entry : *entries_result) {
        if (!entry.filename().string().starts_with("snapshot_")) {
            continue;
        }
        
        auto snapshot_result = load(entry);
        if (!snapshot_result) {
            continue;
        }
        
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - snapshot_result->timestamp_);
        if (age > max_age) {
            Filesystem::remove(entry);
            pruned.push_back(snapshot_result->id_);
        }
    }
    
    return pruned;
}

Result<std::vector<Snapshot>> Snapshot::list(const Path& snapshot_dir) {
    if (!Filesystem::exists(snapshot_dir)) {
        return std::vector<Snapshot>{};
    }
    
    auto entries_result = Filesystem::list_directory(snapshot_dir);
    if (!entries_result) {
        return flux::unexpected(entries_result.error());
    }
    
    std::vector<Snapshot> snapshots;
    for (const auto& entry : *entries_result) {
        if (!entry.filename().string().starts_with("snapshot_")) {
            continue;
        }
        
        auto snapshot_result = load(entry);
        if (snapshot_result) {
            snapshots.push_back(*snapshot_result);
        }
    }
    
    // Sort by timestamp (newest first)
    std::sort(snapshots.begin(), snapshots.end(), [](const Snapshot& a, const Snapshot& b) {
        return a.timestamp_ > b.timestamp_;
    });
    
    return snapshots;
}

Bytes Snapshot::serialize() const {
    std::ostringstream oss;
    
    // Header
    oss << "snapshot " << id_ << "\n";
    oss << "timestamp " << std::chrono::system_clock::to_time_t(timestamp_) << "\n";
    oss << "description " << description_ << "\n";
    
    if (head_commit_.is_valid()) {
        oss << "head " << head_commit_.to_hex() << "\n";
    }
    
    // References
    for (const auto& [name, ref] : refs_) {
        if (ref.is_symbolic) {
            oss << "symref " << name << " " << ref.symbolic_target << "\n";
        } else {
            oss << "ref " << name << " " << ref.target.to_hex() << "\n";
        }
    }
    
    std::string str = oss.str();
    return Bytes(str.begin(), str.end());
}

Result<Snapshot> Snapshot::deserialize(std::span<const uint8_t> data) {
    std::string_view content(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream iss{std::string{content}};
    
    uint64_t id = 0;
    std::time_t timestamp = 0;
    std::string description;
    ObjectId head_commit;
    std::map<std::string, RefSnapshot> refs;
    
    std::string line;
    while (std::getline(iss, line)) {
        std::istringstream line_stream{line};
        std::string key;
        line_stream >> key;
        
        if (key == "snapshot") {
            line_stream >> id;
        } else if (key == "timestamp") {
            line_stream >> timestamp;
        } else if (key == "description") {
            std::getline(line_stream, description);
            if (!description.empty() && description[0] == ' ') {
                description = description.substr(1);
            }
        } else if (key == "head") {
            std::string hex;
            line_stream >> hex;
            auto id_result = ObjectId::from_hex(hex);
            if (id_result) {
                head_commit = *id_result;
            }
        } else if (key == "ref") {
            std::string name, hex;
            line_stream >> name >> hex;
            auto id_result = ObjectId::from_hex(hex);
            if (id_result) {
                RefSnapshot ref;
                ref.name = name;
                ref.is_symbolic = false;
                ref.target = *id_result;
                refs[name] = ref;
            }
        } else if (key == "symref") {
            std::string name, target;
            line_stream >> name >> target;
            RefSnapshot ref;
            ref.name = name;
            ref.is_symbolic = true;
            ref.symbolic_target = target;
            refs[name] = ref;
        }
    }
    
    Snapshot snapshot(id, std::chrono::system_clock::from_time_t(timestamp));
    snapshot.description_ = description;
    snapshot.head_commit_ = head_commit;
    snapshot.refs_ = std::move(refs);
    
    return snapshot;
}

} // namespace flux
