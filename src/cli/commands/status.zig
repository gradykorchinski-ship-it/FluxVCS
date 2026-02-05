const std = @import("std");

pub fn cmdStatus(allocator: std.mem.Allocator) !void {
    _ = allocator;
    try std.io.getStdOut().writer().print("On branch main\nNothing to commit\n", .{});
}
