const std = @import("std");

pub fn cmdLog(allocator: std.mem.Allocator) !void {
    _ = allocator;
    try std.io.getStdOut().writer().print("Showing commit history...\n", .{});
}
