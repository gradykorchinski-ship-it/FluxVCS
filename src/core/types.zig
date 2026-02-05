const std = @import("std");

/// Common type aliases
pub const Bytes = []u8;
pub const Path = []const u8;

/// Hash algorithm enumeration
pub const HashAlgorithm = enum {
    sha1, // Git compatibility
    sha256, // Default for FluxVCS
    blake3, // Future
    sha3_256, // Future

    pub fn toString(self: HashAlgorithm) []const u8 {
        return switch (self) {
            .sha1 => "SHA1",
            .sha256 => "SHA256",
            .blake3 => "BLAKE3",
            .sha3_256 => "SHA3-256",
        };
    }

    pub fn parse(str: []const u8) ?HashAlgorithm {
        if (std.mem.eql(u8, str, "SHA1")) return .sha1;
        if (std.mem.eql(u8, str, "SHA256")) return .sha256;
        if (std.mem.eql(u8, str, "BLAKE3")) return .blake3;
        if (std.mem.eql(u8, str, "SHA3-256")) return .sha3_256;
        return null;
    }
};

/// Object types in the VCS
pub const ObjectType = enum(u8) {
    blob = 1,
    tree = 2,
    commit = 3,
    tag = 4,

    pub fn toString(self: ObjectType) []const u8 {
        return switch (self) {
            .blob => "blob",
            .tree => "tree",
            .commit => "commit",
            .tag => "tag",
        };
    }

    pub fn parse(str: []const u8) ?ObjectType {
        if (std.mem.eql(u8, str, "blob")) return .blob;
        if (std.mem.eql(u8, str, "tree")) return .tree;
        if (std.mem.eql(u8, str, "commit")) return .commit;
        if (std.mem.eql(u8, str, "tag")) return .tag;
        return null;
    }
};

/// File mode (similar to Git)
pub const FileMode = enum(u32) {
    regular = 0o100644, // Regular file
    executable = 0o100755, // Executable file
    symlink = 0o120000, // Symbolic link
    directory = 0o040000, // Directory (tree)

    pub fn toString(self: FileMode) []const u8 {
        return switch (self) {
            .regular => "100644",
            .executable => "100755",
            .symlink => "120000",
            .directory => "040000",
        };
    }

    pub fn toInt(self: FileMode) u32 {
        return @intFromEnum(self);
    }
};

/// Error type with message, suggestion, and optional recovery command
pub const FluxError = struct {
    message: []const u8,
    suggestion: ?[]const u8 = null,
    recovery_command: ?[]const u8 = null,

    pub fn init(message: []const u8) FluxError {
        return .{ .message = message };
    }

    pub fn withSuggestion(message: []const u8, suggestion: []const u8) FluxError {
        return .{ .message = message, .suggestion = suggestion };
    }

    pub fn withRecovery(message: []const u8, suggestion: []const u8, recovery: []const u8) FluxError {
        return .{
            .message = message,
            .suggestion = suggestion,
            .recovery_command = recovery,
        };
    }

    pub fn format(self: FluxError, allocator: std.mem.Allocator) ![]u8 {
        var result = std.ArrayList(u8).init(allocator);
        errdefer result.deinit();

        try result.appendSlice("Error: ");
        try result.appendSlice(self.message);

        if (self.suggestion) |sug| {
            try result.appendSlice("\nSuggestion: ");
            try result.appendSlice(sug);
        }

        if (self.recovery_command) |rec| {
            try result.appendSlice("\nRun: ");
            try result.appendSlice(rec);
        }

        return result.toOwnedSlice();
    }
};

/// Custom error set for Flux operations
pub const FluxErrorSet = error{
    InvalidHash,
    InvalidObjectType,
    ObjectNotFound,
    RepositoryNotFound,
    InvalidRepository,
    IOError,
    CompressionError,
    DecompressionError,
    DatabaseError,
    NetworkError,
    InvalidReference,
    MergeConflict,
    AuthenticationFailed,
    InvalidConfiguration,
    OutOfMemory,
};

/// Result type helper - use Zig's built-in error unions
/// In Zig, we use `!T` for results that can error, or `?T` for optional values
/// Example: `FluxErrorSet!ObjectId` instead of C++ `Result<ObjectId>`
pub fn Result(comptime T: type) type {
    return FluxErrorSet!T;
}

test "HashAlgorithm string conversion" {
    try std.testing.expectEqualStrings("SHA256", HashAlgorithm.sha256.toString());
    try std.testing.expectEqual(HashAlgorithm.sha1, HashAlgorithm.parse("SHA1").?);
    try std.testing.expectEqual(@as(?HashAlgorithm, null), HashAlgorithm.parse("INVALID"));
}

test "ObjectType string conversion" {
    try std.testing.expectEqualStrings("commit", ObjectType.commit.toString());
    try std.testing.expectEqual(ObjectType.blob, ObjectType.parse("blob").?);
}

test "FileMode values" {
    try std.testing.expectEqual(@as(u32, 0o100644), FileMode.regular.toInt());
    try std.testing.expectEqual(@as(u32, 0o100755), FileMode.executable.toInt());
}
