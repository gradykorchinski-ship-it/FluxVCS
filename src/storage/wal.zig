const std = @import("std");
const filesystem = @import("../util/filesystem.zig");

const Filesystem = filesystem.Filesystem;

/// WAL - Write-Ahead Log for transactional operations
pub const WAL = struct {
    wal_dir: []const u8,
    next_txn_id: u64,
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator, wal_dir: []const u8) WAL {
        return .{
            .allocator = allocator,
            .wal_dir = wal_dir,
            .next_txn_id = 1,
        };
    }

    /// Begin transaction
    pub fn beginTransaction(self: *WAL) !u64 {
        const txn_id = self.next_txn_id;
        self.next_txn_id += 1;

        const txn_path = try self.getTransactionPath(txn_id);
        defer self.allocator.free(txn_path);

        // Create empty transaction file
        try Filesystem.writeFile(txn_path, "");

        return txn_id;
    }

    /// Log operation
    pub fn logOperation(self: *WAL, txn_id: u64, operation: []const u8, data: []const u8) !void {
        const txn_path = try self.getTransactionPath(txn_id);
        defer self.allocator.free(txn_path);

        // Append operation to transaction log
        var log_data = std.ArrayList(u8).init(self.allocator);
        defer log_data.deinit();

        try log_data.writer().print("{s}\n", .{operation});
        try log_data.appendSlice(data);
        try log_data.append('\n');

        // Append to file
        const file = try std.fs.cwd().openFile(txn_path, .{ .mode = .write_only });
        defer file.close();

        try file.seekFromEnd(0);
        try file.writeAll(log_data.items);
    }

    /// Commit transaction
    pub fn commitTransaction(self: *WAL, txn_id: u64) !void {
        const txn_path = try self.getTransactionPath(txn_id);
        defer self.allocator.free(txn_path);

        // Remove transaction log (operation completed)
        try Filesystem.remove(txn_path);
    }

    /// Rollback transaction
    pub fn rollbackTransaction(self: *WAL, txn_id: u64) !void {
        const txn_path = try self.getTransactionPath(txn_id);
        defer self.allocator.free(txn_path);

        // Remove transaction log
        Filesystem.remove(txn_path) catch {};
    }

    /// Recover from crash (placeholder - full implementation would replay logs)
    pub fn recover(self: *WAL) !void {
        _ = self;
        // TODO: Implement recovery by replaying uncommitted transactions
    }

    fn getTransactionPath(self: *WAL, txn_id: u64) ![]u8 {
        const filename = try std.fmt.allocPrint(self.allocator, "txn_{d}.log", .{txn_id});
        defer self.allocator.free(filename);

        const parts = [_][]const u8{ self.wal_dir, filename };
        return try std.fs.path.join(self.allocator, &parts);
    }
};
