const std = @import("std");

pub const SimpleParser = struct {
    pub fn parse(allocator: std.mem.Allocator, source: []const u8) !void {
        _ = allocator;
        _ = source;
    }
};
