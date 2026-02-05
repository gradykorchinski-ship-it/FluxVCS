const std = @import("std");

// C bindings for zstd
const c = @cImport({
    @cInclude("zstd.h");
});

/// Compression utilities using zstd
pub const Compression = struct {
    /// Compress data using zstd
    pub fn compress(allocator: std.mem.Allocator, data: []const u8, level: i32) ![]u8 {
        const bound = c.ZSTD_compressBound(data.len);
        var compressed = try allocator.alloc(u8, bound);
        errdefer allocator.free(compressed);

        const size = c.ZSTD_compress(
            compressed.ptr,
            compressed.len,
            data.ptr,
            data.len,
            level,
        );

        if (c.ZSTD_isError(size) != 0) {
            return error.CompressionError;
        }

        // Resize to actual compressed size
        compressed = try allocator.realloc(compressed, size);
        return compressed;
    }

    /// Decompress data using zstd
    pub fn decompress(allocator: std.mem.Allocator, data: []const u8) ![]u8 {
        // Get decompressed size
        const decompressed_size = c.ZSTD_getFrameContentSize(data.ptr, data.len);

        if (decompressed_size == c.ZSTD_CONTENTSIZE_ERROR or
            decompressed_size == c.ZSTD_CONTENTSIZE_UNKNOWN)
        {
            return error.DecompressionError;
        }

        const decompressed = try allocator.alloc(u8, decompressed_size);
        errdefer allocator.free(decompressed);

        const size = c.ZSTD_decompress(
            decompressed.ptr,
            decompressed.len,
            data.ptr,
            data.len,
        );

        if (c.ZSTD_isError(size) != 0) {
            return error.DecompressionError;
        }

        return decompressed;
    }

    /// Get compression bound (maximum compressed size)
    pub fn getCompressBound(size: usize) usize {
        return c.ZSTD_compressBound(size);
    }
};

test "compress and decompress" {
    const allocator = std.testing.allocator;

    const original = "Hello, this is a test string for compression!";

    const compressed = try Compression.compress(allocator, original, 3);
    defer allocator.free(compressed);

    // Compressed should be smaller (or at least different size)
    try std.testing.expect(compressed.len <= Compression.getCompressBound(original.len));

    const decompressed = try Compression.decompress(allocator, compressed);
    defer allocator.free(decompressed);

    try std.testing.expectEqualStrings(original, decompressed);
}
