const std = @import("std");
const diff_engine = @import("../../diff/diff_engine.zig");
const unified_diff = @import("../../diff/unified_diff.zig");
const pretty_print = @import("../../format/pretty_print.zig");

pub fn execute(allocator: std.mem.Allocator, args: []const []const u8) !void {
    _ = args; // TODO: Parse arguments

    const stdout = std.fs.File.stdout();
    const writer = stdout.writer();

    // TODO: Get working directory changes from index
    // For now, show example diff

    const old_content = "line 1\nline 2\nline 3\n";
    const new_content = "line 1\nline 2 modified\nline 3\nline 4\n";

    var engine = diff_engine.DiffEngine.init(allocator);
    var file_diff = try engine.diffFiles("file.txt", "file.txt", old_content, new_content);
    defer file_diff.deinit();

    // Print file header
    try pretty_print.printFileHeader(writer, file_diff.old_path, file_diff.new_path);

    // Print hunks
    for (file_diff.hunks) |hunk| {
        try pretty_print.printHunkHeader(writer, hunk.old_start, hunk.old_lines, hunk.new_start, hunk.new_lines);

        for (hunk.lines) |line| {
            if (line.isAddition()) {
                try pretty_print.printDiffLine(writer, line.content, .addition);
            } else if (line.isDeletion()) {
                try pretty_print.printDiffLine(writer, line.content, .deletion);
            } else {
                try pretty_print.printDiffLine(writer, line.content, .context);
            }
        }
    }
}
