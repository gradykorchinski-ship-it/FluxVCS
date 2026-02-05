const std = @import("std");

pub fn cmdNetwork(allocator: std.mem.Allocator, command: []const u8) !void {
    _ = allocator;
    try std.io.getStdOut().writer().print("Network command: {s}\n", .{command});
}
