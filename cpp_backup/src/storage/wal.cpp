#include "flux/storage/wal.hpp"
#include "flux/util/filesystem.hpp"
#include <fstream>
#include <fmt/format.h>

namespace flux {

WAL::WAL(Path wal_dir) : wal_dir_(std::move(wal_dir)) {
}

Result<uint64_t> WAL::begin_transaction() {
    uint64_t txn_id = next_txn_id_++;
    
    // Create transaction directory
    Path txn_path = get_transaction_path(txn_id);
    auto result = Filesystem::create_directories(txn_path);
    if (!result) {
        return flux::unexpected(result.error());
    }
    
    // Write transaction header
    std::string header = fmt::format("txn:{}\nstatus:active\n", txn_id);
    Bytes data(header.begin(), header.end());
    
    auto write_result = Filesystem::write_file(txn_path / "header", data);
    if (!write_result) {
        return flux::unexpected(write_result.error());
    }
    
    return txn_id;
}

Result<void> WAL::log_operation(uint64_t txn_id, const std::string& operation, const Bytes& data) {
    Path txn_path = get_transaction_path(txn_id);
    
    if (!Filesystem::exists(txn_path)) {
        return flux::unexpected(fmt::format("Transaction {} not found", txn_id));
    }
    
    // Append operation to log
    Path log_path = txn_path / "operations";
    
    // Format: <operation_len><operation><data_len><data>
    Bytes log_entry;
    
    uint32_t op_len = operation.size();
    log_entry.insert(log_entry.end(),
        reinterpret_cast<uint8_t*>(&op_len),
        reinterpret_cast<uint8_t*>(&op_len) + sizeof(op_len));
    log_entry.insert(log_entry.end(), operation.begin(), operation.end());
    
    uint64_t data_len = data.size();
    log_entry.insert(log_entry.end(),
        reinterpret_cast<uint8_t*>(&data_len),
        reinterpret_cast<uint8_t*>(&data_len) + sizeof(data_len));
    log_entry.insert(log_entry.end(), data.begin(), data.end());
    
    // Append to file (not atomic, but that's okay for WAL)
    std::ofstream file(log_path, std::ios::binary | std::ios::app);
    if (!file) {
        return flux::unexpected(fmt::format("Failed to open WAL log: {}", log_path.string()));
    }
    
    file.write(reinterpret_cast<const char*>(log_entry.data()), log_entry.size());
    
    return {};
}

Result<void> WAL::commit_transaction(uint64_t txn_id) {
    Path txn_path = get_transaction_path(txn_id);
    
    if (!Filesystem::exists(txn_path)) {
        return flux::unexpected(fmt::format("Transaction {} not found", txn_id));
    }
    
    // Mark as committed
    std::string header = fmt::format("txn:{}\nstatus:committed\n", txn_id);
    Bytes data(header.begin(), header.end());
    
    auto result = Filesystem::write_file(txn_path / "header", data);
    if (!result) {
        return result;
    }
    
    // Clean up transaction directory
    return Filesystem::remove(txn_path);
}

Result<void> WAL::rollback_transaction(uint64_t txn_id) {
    Path txn_path = get_transaction_path(txn_id);
    
    if (!Filesystem::exists(txn_path)) {
        return {};  // Already rolled back
    }
    
    // Simply remove transaction directory
    return Filesystem::remove(txn_path);
}

Result<void> WAL::recover() {
    // Check for incomplete transactions
    if (!Filesystem::exists(wal_dir_)) {
        return {};
    }
    
    auto entries_result = Filesystem::list_directory(wal_dir_);
    if (!entries_result) {
        return flux::unexpected(entries_result.error());
    }
    
    // Roll back any active transactions
    for (const auto& entry : *entries_result) {
        if (Filesystem::is_directory(entry)) {
            // This is a transaction directory
            // For now, just remove it (rollback)
            // In a full implementation, we'd replay or rollback based on status
            auto result = Filesystem::remove(entry);
            if (!result) {
                return result;
            }
        }
    }
    
    return {};
}

Path WAL::get_transaction_path(uint64_t txn_id) const {
    return wal_dir_ / fmt::format("txn_{:016x}", txn_id);
}

} // namespace flux
