const std = @import("std");

pub const HttpTransport = struct {
    pub fn get(allocator: std.mem.Allocator, url: []const u8) ![]u8 {
        _ = allocator;
        _ = url;
        return error.NotImplemented;
    }
};
