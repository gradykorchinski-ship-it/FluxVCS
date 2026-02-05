const std = @import("std");
const types = @import("types.zig");
const object_id_module = @import("object_id.zig");

const ObjectId = object_id_module.ObjectId;
const HashAlgorithm = types.HashAlgorithm;
const FileMode = types.FileMode;

/// TreeEntry - Single entry in a tree (file or subdirectory)
pub const TreeEntry = struct {
    name: []const u8,
    mode: FileMode,
    id: ObjectId,
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator, name: []const u8, mode: FileMode, id: ObjectId) !TreeEntry {
        const name_copy = try allocator.dupe(u8, name);
        return .{
            .allocator = allocator,
            .name = name_copy,
            .mode = mode,
            .id = id,
        };
    }

    pub fn deinit(self: *TreeEntry) void {
        self.allocator.free(self.name);
        self.id.deinit();
    }

    pub fn isDirectory(self: TreeEntry) bool {
        return self.mode == .directory;
    }

    pub fn isExecutable(self: TreeEntry) bool {
        return self.mode == .executable;
    }

    pub fn isSymlink(self: TreeEntry) bool {
        return self.mode == .symlink;
    }
};

/// Tree - Represents a directory
pub const Tree = struct {
    entries: []TreeEntry,
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator) Tree {
        return .{
            .allocator = allocator,
            .entries = &[_]TreeEntry{},
        };
    }

    pub fn initWithEntries(allocator: std.mem.Allocator, entries: []TreeEntry) !Tree {
        var tree = Tree{ .allocator = allocator, .entries = entries };
        try tree.sortEntries();
        return tree;
    }

    pub fn deinit(self: *Tree) void {
        for (self.entries) |*entry| {
            entry.deinit();
        }
        self.allocator.free(self.entries);
    }

    /// Add entry (maintains sorted order)
    pub fn addEntry(self: *Tree, entry: TreeEntry) !void {
        const old = self.entries;
        self.entries = try self.allocator.alloc(TreeEntry, old.len + 1);

        if (old.len > 0) {
            @memcpy(self.entries[0..old.len], old);
            self.allocator.free(old);
        }

        self.entries[old.len] = entry;
        try self.sortEntries();
    }

    /// Find entry by name
    pub fn find(self: Tree, name: []const u8) ?*const TreeEntry {
        for (self.entries) |*entry| {
            if (std.mem.eql(u8, entry.name, name)) {
                return entry;
            }
        }
        return null;
    }

    /// Compute tree's object ID
    pub fn computeId(self: Tree, algo: HashAlgorithm) !ObjectId {
        const serialized = try self.serialize();
        defer self.allocator.free(serialized);

        return try ObjectId.compute(self.allocator, algo, serialized, .tree);
    }

    /// Serialize to FluxVCS format
    pub fn serialize(self: Tree) ![]u8 {
        var result = std.ArrayList(u8).init(self.allocator);
        errdefer result.deinit();

        for (self.entries) |entry| {
            // Format: mode name\0id_hex\n
            const mode_str = entry.mode.toString();
            try result.appendSlice(mode_str);
            try result.append(' ');
            try result.appendSlice(entry.name);
            try result.append(0);

            const id_hex = try entry.id.toHex(self.allocator);
            defer self.allocator.free(id_hex);
            try result.appendSlice(id_hex);
            try result.append('\n');
        }

        return result.toOwnedSlice();
    }

    /// Sort entries by name
    fn sortEntries(self: *Tree) !void {
        const Context = struct {
            pub fn lessThan(_: void, a: TreeEntry, b: TreeEntry) bool {
                return std.mem.order(u8, a.name, b.name) == .lt;
            }
        };
        std.mem.sort(TreeEntry, self.entries, {}, Context.lessThan);
    }
};

test "tree operations" {
    const allocator = std.testing.allocator;

    var tree = Tree.init(allocator);
    defer tree.deinit();

    // Create some test object IDs
    const id1 = try ObjectId.compute(allocator, .sha256, "test1", null);
    const id2 = try ObjectId.compute(allocator, .sha256, "test2", null);

    const entry1 = try TreeEntry.init(allocator, "file1.txt", .regular, id1);
    const entry2 = try TreeEntry.init(allocator, "file2.txt", .executable, id2);

    try tree.addEntry(entry1);
    try tree.addEntry(entry2);

    try std.testing.expectEqual(@as(usize, 2), tree.entries.len);

    const found = tree.find("file1.txt");
    try std.testing.expect(found != null);
    try std.testing.expectEqual(FileMode.regular, found.?.mode);
}
