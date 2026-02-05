const std = @import("std");

/// Filesystem utilities for FluxVCS
pub const Filesystem = struct {
    /// Create directory and all parent directories if they don't exist
    pub fn createDirAll(path: []const u8) !void {
        std.fs.cwd().makePath(path) catch |err| {
            return switch (err) {
                error.PathAlreadyExists => {}, // Ignore if already exists
                else => err,
            };
        };
    }

    /// Check if path exists
    pub fn exists(path: []const u8) bool {
        std.fs.cwd().access(path, .{}) catch return false;
        return true;
    }

    /// Check if path is a directory
    pub fn isDirectory(path: []const u8) bool {
        const stat = std.fs.cwd().statFile(path) catch return false;
        return stat.kind == .directory;
    }

    /// Check if path is a file
    pub fn isFile(path: []const u8) bool {
        const stat = std.fs.cwd().statFile(path) catch return false;
        return stat.kind == .file;
    }

    /// Read entire file into memory
    pub fn readFile(allocator: std.mem.Allocator, path: []const u8) ![]u8 {
        const file = try std.fs.cwd().openFile(path, .{});
        defer file.close();

        const stat = try file.stat();
        const size = stat.size;

        const buffer = try allocator.alloc(u8, size);
        errdefer allocator.free(buffer);

        const bytes_read = try file.readAll(buffer);
        if (bytes_read != size) {
            return error.IOError;
        }

        return buffer;
    }

    /// Write data to file, creating parent directories if needed
    pub fn writeFile(path: []const u8, data: []const u8) !void {
        // Create parent directory if needed
        if (std.fs.path.dirname(path)) |dir| {
            try createDirAll(dir);
        }

        const file = try std.fs.cwd().createFile(path, .{});
        defer file.close();

        try file.writeAll(data);
    }

    /// Remove file or directory
    pub fn remove(path: []const u8) !void {
        if (isDirectory(path)) {
            try std.fs.cwd().deleteTree(path);
        } else {
            try std.fs.cwd().deleteFile(path);
        }
    }

    /// Join path components
    pub fn joinPath(allocator: std.mem.Allocator, parts: []const []const u8) ![]u8 {
        return std.fs.path.join(allocator, parts);
    }

    /// Get absolute path
    pub fn absolutePath(allocator: std.mem.Allocator, path: []const u8) ![]u8 {
        return std.fs.cwd().realpathAlloc(allocator, path);
    }

    /// List directory entries
    pub fn listDir(allocator: std.mem.Allocator, path: []const u8) ![][]const u8 {
        var dir = try std.fs.cwd().openDir(path, .{ .iterate = true });
        defer dir.close();

        var entries = std.ArrayList([]const u8).init(allocator);
        errdefer {
            for (entries.items) |entry| {
                allocator.free(entry);
            }
            entries.deinit();
        }

        var iterator = dir.iterate();
        while (try iterator.next()) |entry| {
            const name = try allocator.dupe(u8, entry.name);
            try entries.append(name);
        }

        return entries.toOwnedSlice();
    }

    /// Copy file
    pub fn copyFile(src: []const u8, dst: []const u8) !void {
        try std.fs.cwd().copyFile(src, std.fs.cwd(), dst, .{});
    }

    /// Get file size
    pub fn fileSize(path: []const u8) !u64 {
        const file = try std.fs.cwd().openFile(path, .{});
        defer file.close();

        const stat = try file.stat();
        return stat.size;
    }
};

test "filesystem operations" {
    const allocator = std.testing.allocator;

    const test_dir = "zig_test_tmp";
    const test_file = "zig_test_tmp/test.txt";

    defer {
        Filesystem.remove(test_dir) catch {};
    }

    // Create directory
    try Filesystem.createDirAll(test_dir);
    try std.testing.expect(Filesystem.exists(test_dir));
    try std.testing.expect(Filesystem.isDirectory(test_dir));

    // Write and read file
    const content = "Hello, Zig!";
    try Filesystem.writeFile(test_file, content);
    try std.testing.expect(Filesystem.exists(test_file));
    try std.testing.expect(Filesystem.isFile(test_file));

    const read_content = try Filesystem.readFile(allocator, test_file);
    defer allocator.free(read_content);
    try std.testing.expectEqualStrings(content, read_content);

    // File size
    const size = try Filesystem.fileSize(test_file);
    try std.testing.expectEqual(@as(u64, content.len), size);
}
