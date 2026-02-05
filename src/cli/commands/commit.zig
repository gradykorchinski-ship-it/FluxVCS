const std = @import("std");

pub fn cmdCommit(allocator: std.mem.Allocator, message: []const u8) !void {
    _ = allocator;
    try std.io.getStdOut().writer().print("Creating commit: {s}\n", .{message});
}
