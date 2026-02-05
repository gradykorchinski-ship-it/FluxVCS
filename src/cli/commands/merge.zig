const std = @import("std");

pub fn cmdMerge(allocator: std.mem.Allocator, branch: []const u8) !void {
    _ = allocator;
    try std.io.getStdOut().writer().print("Merging: {s}\n", .{branch});
}
