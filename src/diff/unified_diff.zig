const std = @import("std");
const diff_engine = @import("diff_engine.zig");
const DiffLine = diff_engine.DiffLine;
const DiffHunk = diff_engine.DiffHunk;
const FileDiff = diff_engine.FileDiff;

/// Generate unified diff format output
pub fn formatUnifiedDiff(allocator: std.mem.Allocator, file_diff: FileDiff, context_lines: usize) ![]u8 {
    _ = context_lines; // TODO: Use for context in hunks
    var output = std.ArrayList(u8).init(allocator);
    errdefer output.deinit();

    const writer = output.writer();

    // File headers
    try writer.print("--- {s}\n", .{file_diff.old_path});
    try writer.print("+++ {s}\n", .{file_diff.new_path});

    // Hunks
    for (file_diff.hunks) |hunk| {
        // Hunk header
        try writer.print("@@ -{d},{d} +{d},{d} @@\n", .{
            hunk.old_start,
            hunk.old_lines,
            hunk.new_start,
            hunk.new_lines,
        });

        // Lines
        for (hunk.lines) |line| {
            if (line.isAddition()) {
                try writer.print("+{s}\n", .{line.content});
            } else if (line.isDeletion()) {
                try writer.print("-{s}\n", .{line.content});
            } else {
                try writer.print(" {s}\n", .{line.content});
            }
        }
    }

    return output.toOwnedSlice();
}

/// Parse a unified diff
pub fn parseUnifiedDiff(allocator: std.mem.Allocator, diff_text: []const u8) !FileDiff {
    _ = allocator;
    _ = diff_text;
    // TODO: Implement unified diff parsing
    return error.NotImplemented;
}

test "format unified diff" {
    const allocator = std.testing.allocator;
    var engine = diff_engine.DiffEngine.init(allocator);

    const old_content = "line1\nline2\nline3";
    const new_content = "line1\nline2 modified\nline3\nline4";

    var file_diff = try engine.diffFiles("old.txt", "new.txt", old_content, new_content);
    defer file_diff.deinit();

    const output = try formatUnifiedDiff(allocator, file_diff, 3);
    defer allocator.free(output);

    try std.testing.expect(output.len > 0);
    try std.testing.expect(std.mem.indexOf(u8, output, "---") != null);
    try std.testing.expect(std.mem.indexOf(u8, output, "+++") != null);
}
