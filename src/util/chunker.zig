const std = @import("std");

/// Content-defined chunking for efficient storage of large files
pub const ContentChunker = struct {
    const WINDOW_SIZE: usize = 64;
    const MIN_CHUNK_SIZE: usize = 2 * 1024; // 2 KB
    const MAX_CHUNK_SIZE: usize = 8 * 1024 * 1024; // 8 MB
    const POLYNOMIAL: u64 = 0x3DA3358B4DC173;

    /// Chunk represents a slice of data (offset, length)
    pub const Chunk = struct {
        offset: usize,
        length: usize,
    };

    /// Chunker state for incremental chunking
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator) ContentChunker {
        return .{ .allocator = allocator };
    }

    /// Chunk data into content-defined chunks
    pub fn chunk(self: *ContentChunker, data: []const u8) ![]Chunk {
        var chunks = std.ArrayList(Chunk).init(self.allocator);
        errdefer chunks.deinit();

        if (data.len == 0) {
            return chunks.toOwnedSlice();
        }

        var offset: usize = 0;

        while (offset < data.len) {
            const chunk_start = offset;
            var chunk_size: usize = 0;

            // Scan for chunk boundary
            while (offset < data.len and chunk_size < MAX_CHUNK_SIZE) {
                chunk_size += 1;
                offset += 1;

                // Only check for boundaries after minimum chunk size
                if (chunk_size >= MIN_CHUNK_SIZE and offset >= WINDOW_SIZE) {
                    // Get rolling window
                    const window_start = offset - WINDOW_SIZE;
                    const window_data = data[window_start..offset];

                    const hash = rollingHash(window_data);

                    if (isChunkBoundary(hash)) {
                        break;
                    }
                }
            }

            try chunks.append(.{
                .offset = chunk_start,
                .length = chunk_size,
            });
        }

        return chunks.toOwnedSlice();
    }

    /// Compute rolling hash of window
    fn rollingHash(window: []const u8) u64 {
        var hash: u64 = 0;

        for (window) |byte| {
            hash = @mulWithOverflow(hash, POLYNOMIAL)[0] +% byte;
        }

        return hash;
    }

    /// Check if hash indicates chunk boundary
    fn isChunkBoundary(hash: u64) bool {
        // Check if lower bits match pattern (creates ~64KB average chunks)
        const mask: u64 = (1 << 16) - 1; // 16 bits = ~64KB average
        return (hash & mask) == 0;
    }
};

test "content chunking" {
    const allocator = std.testing.allocator;

    var chunker = ContentChunker.init(allocator);

    // Create test data
    const data = try allocator.alloc(u8, 1024 * 1024); // 1 MB
    defer allocator.free(data);

    // Fill with some pattern
    for (data, 0..) |*byte, i| {
        byte.* = @intCast(i % 256);
    }

    const chunks = try chunker.chunk(data);
    defer allocator.free(chunks);

    // Should have at least one chunk
    try std.testing.expect(chunks.len > 0);

    // Verify chunks cover all data
    var total_size: usize = 0;
    for (chunks) |c| {
        total_size += c.length;
    }
    try std.testing.expectEqual(data.len, total_size);
}
