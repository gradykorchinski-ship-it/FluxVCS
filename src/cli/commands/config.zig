const std = @import("std");

pub fn cmdConfig(allocator: std.mem.Allocator, key: ?[]const u8, value: ?[]const u8) !void {
    _ = allocator;
    if (key) |k| {
        if (value) |v| {
            try std.io.getStdOut().writer().print("Setting {s} = {s}\n", .{ k, v });
        } else {
            try std.io.getStdOut().writer().print("Getting {s}\n", .{k});
        }
    } else {
        try std.io.getStdOut().writer().print("Listing configuration...\n", .{});
    }
}
