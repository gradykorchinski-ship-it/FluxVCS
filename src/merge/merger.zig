const std = @import("std");

pub const Merger = struct {
    pub fn merge(allocator: std.mem.Allocator, base: []const u8, ours: []const u8, theirs: []const u8) ![]u8 {
        _ = allocator;
        _ = base;
        _ = ours;
        _ = theirs;
        return error.NotImplemented;
    }
};
