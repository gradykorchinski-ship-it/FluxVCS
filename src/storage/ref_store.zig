const std = @import("std");
const types = @import("../core/types.zig");
const object_id_module = @import("../core/object_id.zig");
const filesystem = @import("../util/filesystem.zig");

const ObjectId = object_id_module.ObjectId;
const Filesystem = filesystem.Filesystem;

/// RefStore - Manages references (branches, tags, HEAD)
pub const RefStore = struct {
    refs_dir: []const u8,
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator, refs_dir: []const u8) RefStore {
        return .{
            .allocator = allocator,
            .refs_dir = refs_dir,
        };
    }

    /// Read reference
    pub fn read(self: *RefStore, name: []const u8) !ObjectId {
        const ref_path = try self.getRefPath(name);
        defer self.allocator.free(ref_path);

        const data = try Filesystem.readFile(self.allocator, ref_path);
        defer self.allocator.free(data);

        // Trim whitespace
        const content = std.mem.trim(u8, data, &std.ascii.whitespace);

        return try ObjectId.fromHex(self.allocator, content);
    }

    /// Write reference
    pub fn write(self: *RefStore, name: []const u8, id: *const ObjectId) !void {
        const ref_path = try self.getRefPath(name);
        defer self.allocator.free(ref_path);

        const content = try id.toHex(self.allocator);
        defer self.allocator.free(content);

        var data = std.ArrayList(u8).init(self.allocator);
        defer data.deinit();

        try data.appendSlice(content);
        try data.append('\n');

        try Filesystem.writeFile(ref_path, data.items);
    }

    /// Remove reference
    pub fn remove(self: *RefStore, name: []const u8) !void {
        const ref_path = try self.getRefPath(name);
        defer self.allocator.free(ref_path);
        try Filesystem.remove(ref_path);
    }

    /// Check if reference exists
    pub fn exists(self: *RefStore, name: []const u8) !bool {
        const ref_path = try self.getRefPath(name);
        defer self.allocator.free(ref_path);
        return Filesystem.exists(ref_path);
    }

    /// List all references
    pub fn list(self: *RefStore) ![][]const u8 {
        var refs = std.ArrayList([]const u8).init(self.allocator);
        errdefer {
            for (refs.items) |ref| self.allocator.free(ref);
            refs.deinit();
        }

        // List refs/heads/
        const heads_dir = try std.fs.path.join(self.allocator, &[_][]const u8{ self.refs_dir, "heads" });
        defer self.allocator.free(heads_dir);

        if (Filesystem.exists(heads_dir)) {
            const entries = try Filesystem.listDir(self.allocator, heads_dir);
            defer {
                for (entries) |entry| self.allocator.free(entry);
                self.allocator.free(entries);
            }

            for (entries) |entry| {
                const ref_name = try std.fmt.allocPrint(self.allocator, "refs/heads/{s}", .{entry});
                try refs.append(ref_name);
            }
        }

        // List refs/tags/
        const tags_dir = try std.fs.path.join(self.allocator, &[_][]const u8{ self.refs_dir, "tags" });
        defer self.allocator.free(tags_dir);

        if (Filesystem.exists(tags_dir)) {
            const entries = try Filesystem.listDir(self.allocator, tags_dir);
            defer {
                for (entries) |entry| self.allocator.free(entry);
                self.allocator.free(entries);
            }

            for (entries) |entry| {
                const ref_name = try std.fmt.allocPrint(self.allocator, "refs/tags/{s}", .{entry});
                try refs.append(ref_name);
            }
        }

        return refs.toOwnedSlice();
    }

    /// Read symbolic reference (e.g., HEAD)
    pub fn readSymbolic(self: *RefStore, name: []const u8) ![]u8 {
        const parts = [_][]const u8{ self.refs_dir, "..", name };
        const ref_path = try std.fs.path.join(self.allocator, &parts);
        defer self.allocator.free(ref_path);

        const data = try Filesystem.readFile(self.allocator, ref_path);
        defer self.allocator.free(data);

        var content = std.mem.trim(u8, data, &std.ascii.whitespace);

        // Format: "ref: refs/heads/main"
        if (!std.mem.startsWith(u8, content, "ref: ")) {
            return error.InvalidReference;
        }

        return try self.allocator.dupe(u8, content[5..]);
    }

    /// Write symbolic reference
    pub fn writeSymbolic(self: *RefStore, name: []const u8, target: []const u8) !void {
        const parts = [_][]const u8{ self.refs_dir, "..", name };
        const ref_path = try std.fs.path.join(self.allocator, &parts);
        defer self.allocator.free(ref_path);

        const content = try std.fmt.allocPrint(self.allocator, "ref: {s}\n", .{target});
        defer self.allocator.free(content);

        try Filesystem.writeFile(ref_path, content);
    }

    fn getRefPath(self: *RefStore, name: []const u8) ![]u8 {
        const parts = [_][]const u8{ self.refs_dir, "..", name };
        return try std.fs.path.join(self.allocator, &parts);
    }
};
