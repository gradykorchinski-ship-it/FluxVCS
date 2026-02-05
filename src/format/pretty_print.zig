const std = @import("std");
const color = @import("color.zig");

/// Line change type for diff output
pub const ChangeType = enum {
    context, // Unchanged line
    addition, // Added line
    deletion, // Removed line
    header, // Diff header
};

/// Pretty print a diff line with colors
pub fn printDiffLine(writer: anytype, line: []const u8, change_type: ChangeType) !void {
    switch (change_type) {
        .addition => {
            try color.writeColored(writer, "+", .green);
            try color.writeColored(writer, line, .green);
        },
        .deletion => {
            try color.writeColored(writer, "-", .red);
            try color.writeColored(writer, line, .red);
        },
        .context => {
            try writer.writeAll(" ");
            try writer.writeAll(line);
        },
        .header => {
            try color.writeColored(writer, line, .cyan);
        },
    }
    try writer.writeAll("\n");
}

/// Print a commit header with formatting
pub fn printCommitHeader(writer: anytype, commit_id: []const u8, author: []const u8, date: []const u8, message: []const u8) !void {
    // Commit ID in yellow
    try color.writeColored(writer, "commit ", .yellow);
    try color.writeColored(writer, commit_id, .yellow);
    try writer.writeAll("\n");

    // Author
    try writer.writeAll("Author: ");
    try writer.writeAll(author);
    try writer.writeAll("\n");

    // Date
    try writer.writeAll("Date:   ");
    try writer.writeAll(date);
    try writer.writeAll("\n\n");

    // Message (indented)
    try writer.writeAll("    ");
    try writer.writeAll(message);
    try writer.writeAll("\n\n");
}

/// Print a file header for diff output
pub fn printFileHeader(writer: anytype, old_path: []const u8, new_path: []const u8) !void {
    try color.writeColored(writer, "--- ", .bold);
    try writer.writeAll(old_path);
    try writer.writeAll("\n");

    try color.writeColored(writer, "+++ ", .bold);
    try writer.writeAll(new_path);
    try writer.writeAll("\n");
}

/// Print a hunk header
pub fn printHunkHeader(writer: anytype, old_start: usize, old_count: usize, new_start: usize, new_count: usize) !void {
    const header = try std.fmt.allocPrint(
        std.heap.page_allocator,
        "@@ -{d},{d} +{d},{d} @@",
        .{ old_start, old_count, new_start, new_count },
    );
    defer std.heap.page_allocator.free(header);

    try color.writeColored(writer, header, .cyan);
    try writer.writeAll("\n");
}

test "print diff line" {
    var buffer = std.ArrayList(u8).init(std.testing.allocator);
    defer buffer.deinit();

    try printDiffLine(buffer.writer(), "test line", .addition);
    try std.testing.expect(buffer.items.len > 0);
}
