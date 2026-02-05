// Core library exports
pub const types = @import("types.zig");
pub const object_id = @import("object_id.zig");
pub const blob = @import("blob.zig");
pub const tree = @import("tree.zig");
pub const commit = @import("commit.zig");

// Util module exports for cross-directory imports
pub const util_hash = @import("../util/hash.zig");
pub const util_chunker = @import("../util/chunker.zig");
pub const util_filesystem = @import("../util/filesystem.zig");
pub const util_compression = @import("../util/compression.zig");
pub const util_zlib = @import("../util/zlib_util.zig");

// Re-export commonly used types
pub const HashAlgorithm = types.HashAlgorithm;
pub const ObjectType = types.ObjectType;
pub const FileMode = types.FileMode;
pub const FluxError = types.FluxError;
pub const FluxErrorSet = types.FluxErrorSet;

pub const ObjectId = object_id.ObjectId;
pub const Blob = blob.Blob;
pub const Tree = tree.Tree;
pub const TreeEntry = tree.TreeEntry;
pub const Commit = commit.Commit;
pub const Signature = commit.Signature;
