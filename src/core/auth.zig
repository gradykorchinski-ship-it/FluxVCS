const std = @import("std");

/// Auth - Authentication utilities (placeholder)
pub const Auth = struct {
    pub fn hashPassword(allocator: std.mem.Allocator, password: []const u8) ![]u8 {
        _ = allocator;
        _ = password;
        return error.NotImplemented;
    }
};
