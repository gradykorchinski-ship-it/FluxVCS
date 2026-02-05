const std = @import("std");
const types = @import("types.zig");
const object_id_module = @import("object_id.zig");

const ObjectId = object_id_module.ObjectId;
const HashAlgorithm = types.HashAlgorithm;

/// Signature - Author or committer information
pub const Signature = struct {
    name: []const u8,
    email: []const u8,
    timestamp: i64, // Unix timestamp
    timezone_offset: i32, // Minutes from UTC
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator, name: []const u8, email: []const u8) !Signature {
        const name_copy = try allocator.dupe(u8, name);
        errdefer allocator.free(name_copy);
        const email_copy = try allocator.dupe(u8, email);

        return .{
            .allocator = allocator,
            .name = name_copy,
            .email = email_copy,
            .timestamp = std.time.timestamp(),
            .timezone_offset = 0,
        };
    }

    pub fn deinit(self: *Signature) void {
        self.allocator.free(self.name);
        self.allocator.free(self.email);
    }

    pub fn format(self: Signature, allocator: std.mem.Allocator) ![]u8 {
        // Format: Name <email> timestamp +0000
        const tz_sign: u8 = if (self.timezone_offset >= 0) '+' else '-';
        const tz_abs = @abs(self.timezone_offset);
        const tz_hours = @divTrunc(tz_abs, 60);
        const tz_mins = @mod(tz_abs, 60);

        return std.fmt.allocPrint(
            allocator,
            "{s} <{s}> {d} {c}{d:0>2}{d:0>2}",
            .{ self.name, self.email, self.timestamp, tz_sign, tz_hours, tz_mins },
        );
    }
};

/// Commit - Represents a snapshot in history
pub const Commit = struct {
    tree: ?ObjectId,
    parents: []ObjectId,
    author: Signature,
    committer: Signature,
    message: []const u8,
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator) !Commit {
        const empty_name = try allocator.dupe(u8, "");
        errdefer allocator.free(empty_name);
        const empty_email = try allocator.dupe(u8, "");

        return .{
            .allocator = allocator,
            .tree = null,
            .parents = &[_]ObjectId{},
            .author = .{
                .allocator = allocator,
                .name = empty_name,
                .email = empty_email,
                .timestamp = 0,
                .timezone_offset = 0,
            },
            .committer = .{
                .allocator = allocator,
                .name = try allocator.dupe(u8, ""),
                .email = try allocator.dupe(u8, ""),
                .timestamp = 0,
                .timezone_offset = 0,
            },
            .message = try allocator.dupe(u8, ""),
        };
    }

    pub fn deinit(self: *Commit) void {
        if (self.tree) |*tree| {
            tree.deinit();
        }
        for (self.parents) |*parent| {
            parent.deinit();
        }
        self.allocator.free(self.parents);
        self.author.deinit();
        self.committer.deinit();
        self.allocator.free(self.message);
    }

    pub fn setTree(self: *Commit, tree: ObjectId) void {
        if (self.tree) |*old_tree| {
            old_tree.deinit();
        }
        self.tree = tree;
    }

    pub fn addParent(self: *Commit, parent: ObjectId) !void {
        const old = self.parents;
        self.parents = try self.allocator.alloc(ObjectId, old.len + 1);

        if (old.len > 0) {
            @memcpy(self.parents[0..old.len], old);
            self.allocator.free(old);
        }

        self.parents[old.len] = parent;
    }

    pub fn setMessage(self: *Commit, message: []const u8) !void {
        self.allocator.free(self.message);
        self.message = try self.allocator.dupe(u8, message);
    }

    /// Compute commit's object ID
    pub fn computeId(self: Commit, algo: HashAlgorithm) !ObjectId {
        const serialized = try self.serialize();
        defer self.allocator.free(serialized);

        return try ObjectId.compute(self.allocator, algo, serialized, .commit);
    }

    /// Serialize to FluxVCS format (Git-compatible)
    pub fn serialize(self: Commit) ![]u8 {
        var result = std.ArrayList(u8).init(self.allocator);
        errdefer result.deinit();

        // Tree
        if (self.tree) |tree| {
            const tree_hex = try tree.toHex(self.allocator);
            defer self.allocator.free(tree_hex);

            try result.appendSlice("tree ");
            try result.appendSlice(tree_hex);
            try result.append('\n');
        }

        // Parents
        for (self.parents) |parent| {
            const parent_hex = try parent.toHex(self.allocator);
            defer self.allocator.free(parent_hex);

            try result.appendSlice("parent ");
            try result.appendSlice(parent_hex);
            try result.append('\n');
        }

        // Author
        const author_str = try self.author.format(self.allocator);
        defer self.allocator.free(author_str);
        try result.appendSlice("author ");
        try result.appendSlice(author_str);
        try result.append('\n');

        // Committer
        const committer_str = try self.committer.format(self.allocator);
        defer self.allocator.free(committer_str);
        try result.appendSlice("committer ");
        try result.appendSlice(committer_str);
        try result.append('\n');

        // Message
        try result.append('\n');
        try result.appendSlice(self.message);

        return result.toOwnedSlice();
    }

    pub fn isMerge(self: Commit) bool {
        return self.parents.len > 1;
    }

    pub fn isRoot(self: Commit) bool {
        return self.parents.len == 0;
    }
};

test "commit creation and serialization" {
    const allocator = std.testing.allocator;

    var commit = try Commit.init(allocator);
    defer commit.deinit();

    const tree_id = try ObjectId.compute(allocator, .sha256, "tree_data", null);
    commit.setTree(tree_id);

    commit.author = try Signature.init(allocator, "Test Author", "author@example.com");
    commit.committer = try Signature.init(allocator, "Test Committer", "committer@example.com");
    try commit.setMessage("Test commit message");

    try std.testing.expect(!commit.isMerge());
    try std.testing.expect(commit.isRoot());

    const serialized = try commit.serialize();
    defer allocator.free(serialized);

    try std.testing.expect(serialized.len > 0);
    try std.testing.expect(std.mem.indexOf(u8, serialized, "tree ") != null);
    try std.testing.expect(std.mem.indexOf(u8, serialized, "Test commit message") != null);
}
