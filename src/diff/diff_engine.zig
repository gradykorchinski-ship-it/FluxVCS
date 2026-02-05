const std = @import("std");

/// Line in diff output
pub const DiffLine = struct {
    content: []const u8,
    old_line: ?usize, // null if added
    new_line: ?usize, // null if deleted

    pub fn isAddition(self: DiffLine) bool {
        return self.old_line == null;
    }

    pub fn isDeletion(self: DiffLine) bool {
        return self.new_line == null;
    }

    pub fn isContext(self: DiffLine) bool {
        return self.old_line != null and self.new_line != null;
    }
};

/// Hunk of changes in a diff
pub const DiffHunk = struct {
    old_start: usize,
    old_lines: usize,
    new_start: usize,
    new_lines: usize,
    lines: []DiffLine,
    allocator: std.mem.Allocator,

    pub fn deinit(self: *DiffHunk) void {
        for (self.lines) |line| {
            self.allocator.free(line.content);
        }
        self.allocator.free(self.lines);
    }
};

/// Complete diff between two files
pub const FileDiff = struct {
    old_path: []const u8,
    new_path: []const u8,
    hunks: []DiffHunk,
    allocator: std.mem.Allocator,

    pub fn deinit(self: *FileDiff) void {
        self.allocator.free(self.old_path);
        self.allocator.free(self.new_path);
        for (self.hunks) |*hunk| {
            hunk.deinit();
        }
        self.allocator.free(self.hunks);
    }
};

/// Myers' diff algorithm - computes shortest edit script
pub const DiffEngine = struct {
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator) DiffEngine {
        return .{ .allocator = allocator };
    }

    /// Compute diff between two strings line-by-line
    pub fn diffLines(self: DiffEngine, old: []const u8, new: []const u8) ![]DiffLine {
        var old_lines = std.ArrayList([]const u8).init(self.allocator);
        defer old_lines.deinit();

        var new_lines = std.ArrayList([]const u8).init(self.allocator);
        defer new_lines.deinit();

        // Split into lines
        var old_iter = std.mem.split(u8, old, "\n");
        while (old_iter.next()) |line| {
            try old_lines.append(line);
        }

        var new_iter = std.mem.split(u8, new, "\n");
        while (new_iter.next()) |line| {
            try new_lines.append(line);
        }

        return self.diffLineArrays(old_lines.items, new_lines.items);
    }

    /// Simplified LCS-based diff (can be optimized to full Myers' algorithm later)
    fn diffLineArrays(self: DiffEngine, old_lines: []const []const u8, new_lines: []const []const u8) ![]DiffLine {
        var result = std.ArrayList(DiffLine).init(self.allocator);
        errdefer result.deinit();

        // Simple implementation: mark all old as deleted, all new as added
        // TODO: Implement proper LCS/Myers algorithm for better diffs
        var old_idx: usize = 0;
        var new_idx: usize = 0;

        while (old_idx < old_lines.len or new_idx < new_lines.len) {
            if (old_idx < old_lines.len and new_idx < new_lines.len) {
                // Check if lines match
                if (std.mem.eql(u8, old_lines[old_idx], new_lines[new_idx])) {
                    // Context line
                    const content = try self.allocator.dupe(u8, old_lines[old_idx]);
                    try result.append(.{
                        .content = content,
                        .old_line = old_idx,
                        .new_line = new_idx,
                    });
                    old_idx += 1;
                    new_idx += 1;
                } else {
                    // Deletion
                    const content = try self.allocator.dupe(u8, old_lines[old_idx]);
                    try result.append(.{
                        .content = content,
                        .old_line = old_idx,
                        .new_line = null,
                    });
                    old_idx += 1;
                }
            } else if (old_idx < old_lines.len) {
                // Remaining deletions
                const content = try self.allocator.dupe(u8, old_lines[old_idx]);
                try result.append(.{
                    .content = content,
                    .old_line = old_idx,
                    .new_line = null,
                });
                old_idx += 1;
            } else {
                // Remaining additions
                const content = try self.allocator.dupe(u8, new_lines[new_idx]);
                try result.append(.{
                    .content = content,
                    .old_line = null,
                    .new_line = new_idx,
                });
                new_idx += 1;
            }
        }

        return result.toOwnedSlice();
    }

    /// Create a FileDiff from two file contents
    pub fn diffFiles(self: DiffEngine, old_path: []const u8, new_path: []const u8, old_content: []const u8, new_content: []const u8) !FileDiff {
        const diff_lines = try self.diffLines(old_content, new_content);

        // Group into hunks (for now, one big hunk)
        var hunks = try self.allocator.alloc(DiffHunk, 1);
        hunks[0] = .{
            .old_start = 1,
            .old_lines = std.mem.count(u8, old_content, "\n") + 1,
            .new_start = 1,
            .new_lines = std.mem.count(u8, new_content, "\n") + 1,
            .lines = diff_lines,
            .allocator = self.allocator,
        };

        return FileDiff{
            .old_path = try self.allocator.dupe(u8, old_path),
            .new_path = try self.allocator.dupe(u8, new_path),
            .hunks = hunks,
            .allocator = self.allocator,
        };
    }
};

test "diff simple lines" {
    const allocator = std.testing.allocator;
    var engine = DiffEngine.init(allocator);

    const old = "line1\nline2\nline3";
    const new = "line1\nline2 modified\nline3";

    const diff_lines = try engine.diffLines(old, new);
    defer {
        for (diff_lines) |line| {
            allocator.free(line.content);
        }
        allocator.free(diff_lines);
    }

    try std.testing.expect(diff_lines.len > 0);
}
