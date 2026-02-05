const std = @import("std");
const types = @import("../core/types.zig");
const object_id_module = @import("../core/object_id.zig");
const filesystem = @import("../util/filesystem.zig");
const compression = @import("../util/compression.zig");

const ObjectId = object_id_module.ObjectId;
const ObjectType = types.ObjectType;
const HashAlgorithm = types.HashAlgorithm;
const Filesystem = filesystem.Filesystem;
const Compression = compression.Compression;

/// ObjectStore - Content-addressed object storage
pub const ObjectStore = struct {
    objects_dir: []const u8,
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator, objects_dir: []const u8) ObjectStore {
        return .{
            .allocator = allocator,
            .objects_dir = objects_dir,
        };
    }

    /// Write object to store
    pub fn write(self: *ObjectStore, obj_type: ObjectType, data: []const u8, algo: HashAlgorithm) !ObjectId {
        // Compute object ID
        const id = try ObjectId.compute(self.allocator, algo, data, obj_type);
        errdefer id.deinit();

        // Check if already exists
        if (try self.exists(&id)) {
            return id;
        }

        // Write as loose object
        try self.writeLoose(&id, obj_type, data);
        return id;
    }

    /// Read object from store
    pub fn read(self: *ObjectStore, id: *const ObjectId) ![]u8 {
        // Try loose objects
        return self.readLoose(id) catch |err| {
            if (err == error.FileNotFound) {
                // Could try pack files here
                return error.ObjectNotFound;
            }
            return err;
        };
    }

    /// Check if object exists
    pub fn exists(self: *ObjectStore, id: *const ObjectId) !bool {
        const path = try self.getLoosePath(id);
        defer self.allocator.free(path);
        return Filesystem.exists(path);
    }

    /// Get object type
    pub fn getType(self: *ObjectStore, id: *const ObjectId) !ObjectType {
        const path = try self.getLoosePath(id);
        defer self.allocator.free(path);

        const data = try Filesystem.readFile(self.allocator, path);
        defer self.allocator.free(data);

        if (data.len == 0) return error.InvalidObject;

        return @enumFromInt(data[0]);
    }

    /// Remove object
    pub fn remove(self: *ObjectStore, id: *const ObjectId) !void {
        const path = try self.getLoosePath(id);
        defer self.allocator.free(path);
        try Filesystem.remove(path);
    }

    /// Get loose object path
    fn getLoosePath(self: *ObjectStore, id: *const ObjectId) ![]u8 {
        const hex = try id.toHex(self.allocator);
        defer self.allocator.free(hex);

        // Extract algorithm and hash
        const colon_pos = std.mem.indexOf(u8, hex, ":") orelse hex.len;
        const algo_prefix = hex[0..colon_pos];
        const hash_hex = if (colon_pos < hex.len) hex[colon_pos + 1 ..] else hex;

        if (hash_hex.len < 2) {
            return try std.fs.path.join(self.allocator, &[_][]const u8{ self.objects_dir, "loose", "invalid" });
        }

        // Create path: objects/loose/algo/ab/cdef...
        const parts = [_][]const u8{
            self.objects_dir,
            "loose",
            algo_prefix,
            hash_hex[0..2],
            hash_hex[2..],
        };

        return try std.fs.path.join(self.allocator, &parts);
    }

    /// Write loose object
    fn writeLoose(self: *ObjectStore, id: *const ObjectId, obj_type: ObjectType, data: []const u8) !void {
        const obj_path = try self.getLoosePath(id);
        defer self.allocator.free(obj_path);

        // Format: <type_byte><compressed_data>
        var obj_data = std.ArrayList(u8).init(self.allocator);
        defer obj_data.deinit();

        try obj_data.append(@intFromEnum(obj_type));

        // Compress data
        const compressed = try Compression.compress(self.allocator, data, 3);
        defer self.allocator.free(compressed);

        try obj_data.appendSlice(compressed);

        // Write atomically
        try Filesystem.writeFile(obj_path, obj_data.items);
    }

    /// Read loose object
    fn readLoose(self: *ObjectStore, id: *const ObjectId) ![]u8 {
        const obj_path = try self.getLoosePath(id);
        defer self.allocator.free(obj_path);

        const data = try Filesystem.readFile(self.allocator, obj_path);
        errdefer self.allocator.free(data);

        if (data.len == 0) return error.InvalidObject;

        // Skip type byte and decompress
        const compressed_data = data[1..];

        const decompressed = try Compression.decompress(self.allocator, compressed_data);
        self.allocator.free(data);

        return decompressed;
    }
};
