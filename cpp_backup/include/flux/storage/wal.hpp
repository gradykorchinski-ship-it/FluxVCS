#pragma once

#include "flux/core/types.hpp"
#include <vector>
#include <string>

namespace flux {

/**
 * WAL - Write-Ahead Log for transactional operations
 * 
 * Ensures atomicity and crash recovery for repository operations.
 */
class WAL {
public:
    explicit WAL(Path wal_dir);
    
    // Begin transaction
    Result<uint64_t> begin_transaction();
    
    // Log operation
    Result<void> log_operation(uint64_t txn_id, const std::string& operation, const Bytes& data);
    
    // Commit transaction
    Result<void> commit_transaction(uint64_t txn_id);
    
    // Rollback transaction
    Result<void> rollback_transaction(uint64_t txn_id);
    
    // Recover from crash (replay uncommitted transactions)
    Result<void> recover();
    
private:
    Path get_transaction_path(uint64_t txn_id) const;
    
    Path wal_dir_;
    uint64_t next_txn_id_ = 1;
};

} // namespace flux
