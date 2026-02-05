const std = @import("std");

pub const SemanticDiff = struct {
    pub fn diff(allocator: std.mem.Allocator, old: []const u8, new: []const u8) !void {
        _ = allocator;
        _ = old;
        _ = new;
    }
};
