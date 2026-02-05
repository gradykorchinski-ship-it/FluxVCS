const std = @import("std");

// C bindings for SQLite3
const c = @cImport({
    @cInclude("sqlite3.h");
});

/// Index - SQLite-based metadata index
pub const Index = struct {
    db_path: []const u8,
    db: ?*c.sqlite3,
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator, db_path: []const u8) Index {
        return .{
            .allocator = allocator,
            .db_path = db_path,
            .db = null,
        };
    }

    pub fn open(self: *Index) !void {
        const path_z = try self.allocator.dupeZ(u8, self.db_path);
        defer self.allocator.free(path_z);

        if (c.sqlite3_open(path_z.ptr, &self.db) != c.SQLITE_OK) {
            return error.DatabaseError;
        }

        // Create tables
        const create_sql =
            \\CREATE TABLE IF NOT EXISTS files (
            \\  path TEXT PRIMARY KEY,
            \\  object_id TEXT,
            \\  size INTEGER,
            \\  mtime INTEGER
            \\);
        ;

        if (c.sqlite3_exec(self.db, create_sql, null, null, null) != c.SQLITE_OK) {
            return error.DatabaseError;
        }
    }

    pub fn close(self: *Index) void {
        if (self.db) |db| {
            _ = c.sqlite3_close(db);
            self.db = null;
        }
    }

    pub fn addFile(self: *Index, path: []const u8, object_id: []const u8) !void {
        _ = self;
        _ = path;
        _ = object_id;
        // TODO: Implement SQL INSERT
    }
};
