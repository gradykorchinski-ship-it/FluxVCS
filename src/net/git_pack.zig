const std = @import("std");

pub const GitPack = struct {
    pub fn parse(allocator: std.mem.Allocator, data: []const u8) !void {
        _ = allocator;
        _ = data;
    }
};
