const std = @import("std");

pub fn execute(allocator: std.mem.Allocator, args: []const []const u8) !void {
    _ = allocator;

    const stdout = std.fs.File.stdout();
    const writer = stdout.writer();

    if (args.len == 0) {
        try writer.writeAll("Usage: flux reset [--soft|--mixed|--hard] <commit>\n");
        return;
    }

    var mode: enum { soft, mixed, hard } = .mixed;
    var commit_id: ?[]const u8 = null;

    for (args) |arg| {
        if (std.mem.eql(u8, arg, "--soft")) {
            mode = .soft;
        } else if (std.mem.eql(u8, arg, "--mixed")) {
            mode = .mixed;
        } else if (std.mem.eql(u8, arg, "--hard")) {
            mode = .hard;
        } else {
            commit_id = arg;
        }
    }

    if (commit_id) |id| {
        switch (mode) {
            .soft => try writer.print("Would soft reset to: {s} (keep changes staged)\n", .{id}),
            .mixed => try writer.print("Would mixed reset to: {s} (unstage changes)\n", .{id}),
            .hard => try writer.print("Would hard reset to: {s} (discard all changes)\n", .{id}),
        }
        // TODO: Implement actual reset logic
    } else {
        try writer.writeAll("Error: No commit specified\n");
    }
}
