const std = @import("std");

pub const GitProtocol = struct {
    pub fn connect(allocator: std.mem.Allocator, url: []const u8) !void {
        _ = allocator;
        _ = url;
    }
};
