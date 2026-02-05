const std = @import("std");

pub const GitPackPacker = struct {
    pub fn pack(allocator: std.mem.Allocator, objects: []const []const u8) ![]u8 {
        _ = allocator;
        _ = objects;
        return error.NotImplemented;
    }
};
