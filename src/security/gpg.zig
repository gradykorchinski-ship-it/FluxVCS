const std = @import("std");

/// GPG - GPG signing and verification (placeholder)
pub const GPG = struct {
    pub fn sign(allocator: std.mem.Allocator, data: []const u8) ![]u8 {
        _ = allocator;
        _ = data;
        return error.NotImplemented;
    }

    pub fn verify(signature: []const u8, data: []const u8) !bool {
        _ = signature;
        _ = data;
        return error.NotImplemented;
    }
};
