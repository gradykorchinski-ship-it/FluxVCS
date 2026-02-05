const std = @import("std");

pub fn cmdGpg(allocator: std.mem.Allocator, action: []const u8) !void {
    _ = allocator;
    try std.io.getStdOut().writer().print("GPG {s}\n", .{action});
}
