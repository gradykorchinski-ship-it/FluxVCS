const std = @import("std");
const pretty_print = @import("../../format/pretty_print.zig");

pub fn execute(allocator: std.mem.Allocator, args: []const []const u8) !void {
    _ = allocator;

    const stdout = std.fs.File.stdout();
    const writer = stdout.writer();

    if (args.len == 0) {
        try writer.writeAll("Usage: flux show <commit-id>\n");
        return;
    }

    const commit_id = args[0];

    // TODO: Load commit from object store
    // For now, show example

    try pretty_print.printCommitHeader(
        writer,
        commit_id,
        "Author Name <author@example.com>",
        "Mon Feb 5 16:00:00 2026 -0600",
        "Example commit message",
    );

    try writer.writeAll("    (commit details and diff would appear here)\n");
}
