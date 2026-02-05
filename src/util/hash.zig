const std = @import("std");
const types = @import("../core/types.zig");
const HashAlgorithm = types.HashAlgorithm;
const Bytes = types.Bytes;

// C bindings for OpenSSL (for SHA1 Git compatibility)
const c = @cImport({
    @cInclude("openssl/evp.h");
    @cInclude("openssl/sha.h");
});

/// Hash utilities for computing object IDs
pub const Hash = struct {
    /// Compute hash using specified algorithm
    pub fn compute(allocator: std.mem.Allocator, algo: HashAlgorithm, data: []const u8) ![]u8 {
        return switch (algo) {
            .sha1 => computeSha1(allocator, data),
            .sha256 => computeSha256(allocator, data),
            .sha3_256 => computeSha3_256(allocator, data),
            .blake3 => computeSha256(allocator, data), // Fallback to SHA256 for now
        };
    }

    /// Get hash size for algorithm
    pub fn hashSize(algo: HashAlgorithm) usize {
        return switch (algo) {
            .sha1 => 20,
            .sha256 => 32,
            .sha3_256 => 32,
            .blake3 => 32,
        };
    }

    /// Convert bytes to hex string
    pub fn toHex(allocator: std.mem.Allocator, data: []const u8) ![]u8 {
        const hex_chars = "0123456789abcdef";
        var result = try allocator.alloc(u8, data.len * 2);

        for (data, 0..) |byte, i| {
            result[i * 2] = hex_chars[byte >> 4];
            result[i * 2 + 1] = hex_chars[byte & 0x0F];
        }

        return result;
    }

    /// Convert hex string to bytes
    pub fn fromHex(allocator: std.mem.Allocator, hex: []const u8) ![]u8 {
        if (hex.len % 2 != 0) {
            return error.InvalidHash;
        }

        var result = try allocator.alloc(u8, hex.len / 2);
        errdefer allocator.free(result);

        for (0..result.len) |i| {
            const high = try hexCharToValue(hex[i * 2]);
            const low = try hexCharToValue(hex[i * 2 + 1]);
            result[i] = (high << 4) | low;
        }

        return result;
    }

    /// Compute SHA-1 hash (using OpenSSL for Git compatibility)
    pub fn computeSha1(allocator: std.mem.Allocator, data: []const u8) ![]u8 {
        const hash = try allocator.alloc(u8, 20);
        errdefer allocator.free(hash);

        const ctx = c.EVP_MD_CTX_new() orelse return error.OutOfMemory;
        defer c.EVP_MD_CTX_free(ctx);

        const sha1_type = c.EVP_sha1();
        if (c.EVP_DigestInit_ex(ctx, sha1_type, null) != 1) {
            return error.InvalidHash;
        }

        if (c.EVP_DigestUpdate(ctx, data.ptr, data.len) != 1) {
            return error.InvalidHash;
        }

        var md_len: c_uint = 20;
        if (c.EVP_DigestFinal_ex(ctx, hash.ptr, &md_len) != 1) {
            return error.InvalidHash;
        }

        return hash;
    }

    /// Compute SHA-256 hash (using Zig stdlib)
    fn computeSha256(allocator: std.mem.Allocator, data: []const u8) ![]u8 {
        var hash = try allocator.alloc(u8, 32);
        std.crypto.hash.sha2.Sha256.hash(data, hash[0..32], .{});
        return hash;
    }

    /// Compute SHA3-256 hash (using OpenSSL)
    fn computeSha3_256(allocator: std.mem.Allocator, data: []const u8) ![]u8 {
        const hash = try allocator.alloc(u8, 32);
        errdefer allocator.free(hash);

        const ctx = c.EVP_MD_CTX_new() orelse return error.OutOfMemory;
        defer c.EVP_MD_CTX_free(ctx);

        const sha3_type = c.EVP_sha3_256();
        if (c.EVP_DigestInit_ex(ctx, sha3_type, null) != 1) {
            return error.InvalidHash;
        }

        if (c.EVP_DigestUpdate(ctx, data.ptr, data.len) != 1) {
            return error.InvalidHash;
        }

        var md_len: c_uint = 32;
        if (c.EVP_DigestFinal_ex(ctx, hash.ptr, &md_len) != 1) {
            return error.InvalidHash;
        }

        return hash;
    }

    fn hexCharToValue(char: u8) !u8 {
        return switch (char) {
            '0'...'9' => char - '0',
            'a'...'f' => char - 'a' + 10,
            'A'...'F' => char - 'A' + 10,
            else => error.InvalidHash,
        };
    }
};

test "SHA-256 hash" {
    const allocator = std.testing.allocator;

    const data = "Hello, World!";
    const hash = try Hash.compute(allocator, .sha256, data);
    defer allocator.free(hash);

    try std.testing.expectEqual(@as(usize, 32), hash.len);
}

test "hex encoding/decoding" {
    const allocator = std.testing.allocator;

    const original = [_]u8{ 0xDE, 0xAD, 0xBE, 0xEF };
    const hex = try Hash.toHex(allocator, &original);
    defer allocator.free(hex);

    try std.testing.expectEqualStrings("deadbeef", hex);

    const decoded = try Hash.fromHex(allocator, hex);
    defer allocator.free(decoded);

    try std.testing.expectEqualSlices(u8, &original, decoded);
}

test "hash size" {
    try std.testing.expectEqual(@as(usize, 20), Hash.hashSize(.sha1));
    try std.testing.expectEqual(@as(usize, 32), Hash.hashSize(.sha256));
}
