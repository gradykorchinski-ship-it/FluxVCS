const std = @import("std");

pub fn cmdCheckout(allocator: std.mem.Allocator, target: []const u8) !void {
    _ = allocator;
    try std.io.getStdOut().writer().print("Checking out: {s}\n", .{target});
}
