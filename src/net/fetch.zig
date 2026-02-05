const std = @import("std");

pub const Fetch = struct {
    pub fn fetch(allocator: std.mem.Allocator, remote: []const u8) !void {
        _ = allocator;
        _ = remote;
    }
};
