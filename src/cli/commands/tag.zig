const std = @import("std");

pub fn execute(allocator: std.mem.Allocator, args: []const []const u8) !void {
    _ = allocator;

    const stdout = std.fs.File.stdout();
    const writer = stdout.writer();

    if (args.len == 0) {
        // List tags
        try writer.writeAll("Tags:\n");
        try writer.writeAll("  (no tags yet)\n");
        return;
    }

    const tag_name = args[0];

    // Check for flags
    var is_annotated = false;
    var message: ?[]const u8 = null;
    var delete_tag = false;

    var i: usize = 1;
    while (i < args.len) : (i += 1) {
        if (std.mem.eql(u8, args[i], "-a")) {
            is_annotated = true;
        } else if (std.mem.eql(u8, args[i], "-m") and i + 1 < args.len) {
            message = args[i + 1];
            i += 1;
        } else if (std.mem.eql(u8, args[i], "-d")) {
            delete_tag = true;
        }
    }

    if (delete_tag) {
        try writer.print("Would delete tag: {s}\n", .{tag_name});
        // TODO: Implement tag deletion
    } else if (is_annotated) {
        const msg = message orelse "Annotated tag";
        try writer.print("Would create annotated tag '{s}' with message: {s}\n", .{ tag_name, msg });
        // TODO: Implement annotated tag creation
    } else {
        try writer.print("Would create lightweight tag: {s}\n", .{tag_name});
        // TODO: Implement lightweight tag creation
    }
}
