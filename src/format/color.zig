const std = @import("std");

/// ANSI color codes for terminal output
pub const Color = enum {
    reset,
    bold,
    dim,
    red,
    green,
    yellow,
    blue,
    magenta,
    cyan,
    white,

    pub fn code(self: Color) []const u8 {
        return switch (self) {
            .reset => "\x1b[0m",
            .bold => "\x1b[1m",
            .dim => "\x1b[2m",
            .red => "\x1b[31m",
            .green => "\x1b[32m",
            .yellow => "\x1b[33m",
            .blue => "\x1b[34m",
            .magenta => "\x1b[35m",
            .cyan => "\x1b[36m",
            .white => "\x1b[37m",
        };
    }
};

/// Check if terminal supports colors
pub fn supportsColor() bool {
    const term = std.posix.getenv("TERM") orelse return false;
    if (std.mem.eql(u8, term, "dumb")) return false;
    return true;
}

/// Colorize text if terminal supports it
pub fn colorize(allocator: std.mem.Allocator, text: []const u8, color: Color) ![]u8 {
    if (!supportsColor()) {
        return allocator.dupe(u8, text);
    }
    return std.fmt.allocPrint(allocator, "{s}{s}{s}", .{ color.code(), text, Color.reset.code() });
}

/// Write colored text to writer
pub fn writeColored(writer: anytype, text: []const u8, color: Color) !void {
    if (supportsColor()) {
        try writer.writeAll(color.code());
        try writer.writeAll(text);
        try writer.writeAll(Color.reset.code());
    } else {
        try writer.writeAll(text);
    }
}

test "color codes" {
    try std.testing.expectEqualStrings("\x1b[31m", Color.red.code());
    try std.testing.expectEqualStrings("\x1b[0m", Color.reset.code());
}

test "colorize text" {
    const allocator = std.testing.allocator;

    // Note: This will only work if TERM is set
    const colored = try colorize(allocator, "test", .red);
    defer allocator.free(colored);

    // Result depends on terminal support
    try std.testing.expect(colored.len > 0);
}
