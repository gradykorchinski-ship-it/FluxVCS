const std = @import("std");

pub fn cmdBranch(allocator: std.mem.Allocator, name: ?[]const u8) !void {
    _ = allocator;
    if (name) |n| {
        try std.io.getStdOut().writer().print("Creating branch: {s}\n", .{n});
    } else {
        try std.io.getStdOut().writer().print("Listing branches...\n", .{});
    }
}
