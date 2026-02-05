const std = @import("std");
const types = @import("types.zig");
const lib = @import("lib.zig");
const hash_module = lib.util_hash;

const Hash = hash_module.Hash;
const HashAlgorithm = types.HashAlgorithm;
const ObjectType = types.ObjectType;
const FluxErrorSet = types.FluxErrorSet;

/// ObjectId - Algorithm-agile content identifier
/// Unlike Git's SHA-1 based object IDs, FluxVCS supports multiple hash algorithms
pub const ObjectId = struct {
    algorithm: HashAlgorithm,
    hash: []u8,
    allocator: std.mem.Allocator,

    /// Initialize ObjectId from pre-computed hash
    pub fn init(allocator: std.mem.Allocator, algorithm: HashAlgorithm, hash: []u8) ObjectId {
        return .{
            .algorithm = algorithm,
            .hash = hash,
            .allocator = allocator,
        };
    }

    /// Deinitialize and free hash memory
    pub fn deinit(self: *ObjectId) void {
        self.allocator.free(self.hash);
    }

    /// Compute object ID from data
    pub fn compute(allocator: std.mem.Allocator, algorithm: HashAlgorithm, data: []const u8, obj_type: ?ObjectType) !ObjectId {
        if (algorithm == .sha1 and obj_type != null) {
            // Git-style hashing: "type size\0data"
            const type_str = obj_type.?.toString();
            const header = try std.fmt.allocPrint(allocator, "{s} {d}", .{ type_str, data.len });
            defer allocator.free(header);

            var hashing_data = std.ArrayList(u8).init(allocator);
            defer hashing_data.deinit();

            try hashing_data.appendSlice(header);
            try hashing_data.append(0); // null byte
            try hashing_data.appendSlice(data);

            const computed_hash = try Hash.compute(allocator, algorithm, hashing_data.items);
            return ObjectId.init(allocator, algorithm, computed_hash);
        }

        const computed_hash = try Hash.compute(allocator, algorithm, data);
        return ObjectId.init(allocator, algorithm, computed_hash);
    }

    /// Parse from hex string (format: "algo:hash" or raw hex)
    pub fn fromHex(allocator: std.mem.Allocator, hex: []const u8) !ObjectId {
        // Look for colon separator
        if (std.mem.indexOf(u8, hex, ":")) |colon_pos| {
            const algo_str = hex[0..colon_pos];
            const hash_str = hex[colon_pos + 1 ..];

            const algo = HashAlgorithm.parse(algo_str) orelse return error.InvalidHash;
            const parsed_hash = try Hash.fromHex(allocator, hash_str);

            return ObjectId.init(allocator, algo, parsed_hash);
        }

        // Support raw hex for Git compatibility
        if (hex.len == 40) {
            // SHA-1
            const parsed_hash = try Hash.fromHex(allocator, hex);
            return ObjectId.init(allocator, .sha1, parsed_hash);
        } else if (hex.len == 64) {
            // SHA-256
            const parsed_hash = try Hash.fromHex(allocator, hex);
            return ObjectId.init(allocator, .sha256, parsed_hash);
        }

        return error.InvalidHash;
    }

    /// Serialize to hex string
    pub fn toHex(self: ObjectId, allocator: std.mem.Allocator) ![]u8 {
        const hash_hex = try Hash.toHex(allocator, self.hash);
        defer allocator.free(hash_hex);

        return std.fmt.allocPrint(allocator, "{s}:{s}", .{ self.algorithm.toString(), hash_hex });
    }

    /// Get size of hash
    pub fn size(self: ObjectId) usize {
        return self.hash.len;
    }

    /// Check if valid
    pub fn isValid(self: ObjectId) bool {
        return self.hash.len > 0;
    }

    /// Clone the ObjectId
    pub fn clone(self: ObjectId, allocator: std.mem.Allocator) !ObjectId {
        const hash_copy = try allocator.dupe(u8, self.hash);
        return ObjectId.init(allocator, self.algorithm, hash_copy);
    }

    /// Compare two ObjectIds
    pub fn eql(self: ObjectId, other: ObjectId) bool {
        if (self.algorithm != other.algorithm) return false;
        return std.mem.eql(u8, self.hash, other.hash);
    }

    /// Hash code for use in hash maps
    pub fn hashCode(self: ObjectId) u64 {
        var h: u64 = @intFromEnum(self.algorithm);
        const len = @min(self.hash.len, 8);
        for (0..len) |i| {
            h ^= @as(u64, self.hash[i]) << @intCast(i * 8);
        }
        return h;
    }
};

test "ObjectId compute and hex conversion" {
    const allocator = std.testing.allocator;

    const data = "Hello, FluxVCS!";
    var obj_id = try ObjectId.compute(allocator, .sha256, data, null);
    defer obj_id.deinit();

    try std.testing.expect(obj_id.isValid());
    try std.testing.expectEqual(@as(usize, 32), obj_id.size());

    const hex = try obj_id.toHex(allocator);
    defer allocator.free(hex);

    var parsed = try ObjectId.fromHex(allocator, hex);
    defer parsed.deinit();

    try std.testing.expect(obj_id.eql(parsed));
}

test "ObjectId Git-style hashing" {
    const allocator = std.testing.allocator;

    const data = "test content";
    var obj_id = try ObjectId.compute(allocator, .sha1, data, .blob);
    defer obj_id.deinit();

    try std.testing.expectEqual(HashAlgorithm.sha1, obj_id.algorithm);
    try std.testing.expectEqual(@as(usize, 20), obj_id.size());
}
