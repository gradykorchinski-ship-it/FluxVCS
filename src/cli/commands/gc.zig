const std = @import("std");

pub fn cmdGc(allocator: std.mem.Allocator) !void {
    _ = allocator;
    try std.io.getStdOut().writer().print("Running garbage collection...\n", .{});
}
