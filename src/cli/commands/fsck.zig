const std = @import("std");

pub fn cmdFsck(allocator: std.mem.Allocator) !void {
    _ = allocator;
    try std.io.getStdOut().writer().print("Checking repository integrity...\n", .{});
}
