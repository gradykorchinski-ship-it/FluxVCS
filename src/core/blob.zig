const std = @import("std");
const types = @import("types.zig");
const object_id_module = @import("object_id.zig");
const lib = @import("lib.zig");
const chunker_module = lib.util_chunker;

const ObjectId = object_id_module.ObjectId;
const HashAlgorithm = types.HashAlgorithm;
const ObjectType = types.ObjectType;
const ContentChunker = chunker_module.ContentChunker;

/// Chunk - A content-defined piece of a file
pub const Chunk = struct {
    id: ObjectId,
    size: usize,
    data: []u8, // May be empty if not loaded

    pub fn deinit(self: *Chunk) void {
        if (self.data.len > 0) {
            self.id.allocator.free(self.data);
        }
        self.id.deinit();
    }
};

/// Blob - Represents file content as a sequence of chunks
pub const Blob = struct {
    chunks: []Chunk,
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator, chunks: []Chunk) Blob {
        return .{ .allocator = allocator, .chunks = chunks };
    }

    pub fn deinit(self: *Blob) void {
        for (self.chunks) |*chunk| {
            chunk.deinit();
        }
        self.allocator.free(self.chunks);
    }

    /// Create blob from file content
    pub fn fromData(allocator: std.mem.Allocator, data: []const u8, algo: HashAlgorithm) !Blob {
        if (algo == .sha1) {
            // Git compatibility - don't chunk SHA-1 blobs
            const content_id = try ObjectId.compute(allocator, algo, data, .blob);
            const data_copy = try allocator.dupe(u8, data);

            var chunks = try allocator.alloc(Chunk, 1);
            chunks[0] = .{
                .id = content_id,
                .size = data.len,
                .data = data_copy,
            };
            return Blob.init(allocator, chunks);
        }

        // Content-defined chunking
        var chunker = ContentChunker.init(allocator);
        const chunk_boundaries = try chunker.chunk(data);
        defer allocator.free(chunk_boundaries);

        var chunks = try allocator.alloc(Chunk, chunk_boundaries.len);
        errdefer {
            for (chunks) |*chunk| chunk.deinit();
            allocator.free(chunks);
        }

        for (chunk_boundaries, 0..) |boundary, i| {
            const chunk_data = data[boundary.offset .. boundary.offset + boundary.length];
            const chunk_id = try ObjectId.compute(allocator, algo, chunk_data, .blob);
            const chunk_data_copy = try allocator.dupe(u8, chunk_data);

            chunks[i] = .{
                .id = chunk_id,
                .size = boundary.length,
                .data = chunk_data_copy,
            };
        }

        return Blob.init(allocator, chunks);
    }

    /// Get total size
    pub fn totalSize(self: Blob) usize {
        var total: usize = 0;
        for (self.chunks) |chunk| {
            total += chunk.size;
        }
        return total;
    }

    /// Compute blob's object ID
    pub fn computeId(self: Blob, algo: HashAlgorithm) !ObjectId {
        if (algo == .sha1 and self.chunks.len == 1) {
            return try self.chunks[0].id.clone(self.allocator);
        }

        // Serialize chunk list and hash it
        const serialized = try self.serialize();
        defer self.allocator.free(serialized);

        return try ObjectId.compute(self.allocator, algo, serialized, .blob);
    }

    /// Serialize to FluxVCS format
    pub fn serialize(self: Blob) ![]u8 {
        var result = std.ArrayList(u8).init(self.allocator);
        errdefer result.deinit();

        // Write number of chunks (4 bytes)
        const num_chunks: u32 = @intCast(self.chunks.len);
        try result.writer().writeInt(u32, num_chunks, .little);

        for (self.chunks) |chunk| {
            // Write chunk ID hex string
            const id_hex = try chunk.id.toHex(self.allocator);
            defer self.allocator.free(id_hex);

            const id_len: u32 = @intCast(id_hex.len);
            try result.writer().writeInt(u32, id_len, .little);
            try result.appendSlice(id_hex);

            // Write chunk size
            try result.writer().writeInt(u64, chunk.size, .little);
        }

        return result.toOwnedSlice();
    }

    /// Reconstruct full content from chunks
    pub fn reconstruct(self: Blob) ![]u8 {
        var result = try self.allocator.alloc(u8, self.totalSize());
        var offset: usize = 0;

        for (self.chunks) |chunk| {
            @memcpy(result[offset .. offset + chunk.data.len], chunk.data);
            offset += chunk.data.len;
        }

        return result;
    }
};

test "blob from data and reconstruction" {
    const allocator = std.testing.allocator;

    const data = "Hello, this is test file content!";
    var blob = try Blob.fromData(allocator, data, .sha256);
    defer blob.deinit();

    try std.testing.expect(blob.chunks.len > 0);
    try std.testing.expectEqual(data.len, blob.totalSize());

    const reconstructed = try blob.reconstruct();
    defer allocator.free(reconstructed);

    try std.testing.expectEqualStrings(data, reconstructed);
}
