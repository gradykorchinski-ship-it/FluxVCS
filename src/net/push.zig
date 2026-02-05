const std = @import("std");

pub const Push = struct {
    pub fn push(allocator: std.mem.Allocator, remote: []const u8) !void {
        _ = allocator;
        _ = remote;
    }
};
