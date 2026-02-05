const std = @import("std");

/// Zlib compression/decompression (Git object compatibility)
pub const ZlibUtil = struct {
    /// Compress data using zlib (deflate)
    pub fn compress(allocator: std.mem.Allocator, data: []const u8) ![]u8 {
        var compressed = std.ArrayList(u8).init(allocator);
        errdefer compressed.deinit();

        var compressor = try std.compress.zlib.compressor(compressed.writer(), .{});
        try compressor.writer().writeAll(data);
        try compressor.finish();

        return compressed.toOwnedSlice();
    }

    /// Decompress zlib data (inflate)
    pub fn decompress(allocator: std.mem.Allocator, data: []const u8) ![]u8 {
        var decompressed = std.ArrayList(u8).init(allocator);
        errdefer decompressed.deinit();

        var stream = std.io.fixedBufferStream(data);
        var decompressor = try std.compress.zlib.decompressor(stream.reader());

        try decompressor.reader().readAllArrayList(&decompressed, std.math.maxInt(usize));

        return decompressed.toOwnedSlice();
    }
};

test "zlib compress and decompress" {
    const allocator = std.testing.allocator;

    const original = "This is test data for zlib compression!";

    const compressed = try ZlibUtil.compress(allocator, original);
    defer allocator.free(compressed);

    const decompressed = try ZlibUtil.decompress(allocator, compressed);
    defer allocator.free(decompressed);

    try std.testing.expectEqualStrings(original, decompressed);
}
