const std = @import("std");

pub fn cmdAdd(allocator: std.mem.Allocator, files: []const []const u8) !void {
    _ = allocator;
    for (files) |file| {
        try std.io.getStdOut().writer().print("Adding: {s}\n", .{file});
    }
}
