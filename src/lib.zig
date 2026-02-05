// Flux VCS - Main library entry point
const std = @import("std");

// Core modules
pub const types = @import("core/types.zig");
pub const ObjectId = @import("core/object_id.zig").ObjectId;
pub const Blob = @import("core/blob.zig").Blob;
pub const Tree = @import("core/tree.zig").Tree;
pub const Commit = @import("core/commit.zig").Commit;
pub const Repository = @import("core/repository.zig").Repository;

// Util modules
pub const Hash = @import("util/hash.zig").Hash;
pub const ContentChunker = @import("util/chunker.zig").ContentChunker;
pub const filesystem = @import("util/filesystem.zig");
pub const compression = @import("util/compression.zig");
pub const zlib_util = @import("util/zlib_util.zig");

// Storage modules
pub const ObjectStore = @import("storage/object_store.zig").ObjectStore;
pub const RefStore = @import("storage/ref_store.zig").RefStore;
pub const WAL = @import("storage/wal.zig").WAL;

// Diff and formatting modules
pub const DiffEngine = @import("diff/diff_engine.zig").DiffEngine;
pub const unified_diff = @import("diff/unified_diff.zig");
pub const color = @import("format/color.zig");
pub const pretty_print = @import("format/pretty_print.zig");

// Include all tests
test {
    std.testing.refAllDecls(@This());
    _ = @import("core/types.zig");
    _ = @import("core/object_id.zig");
    _ = @import("core/blob.zig");
    _ = @import("core/tree.zig");
    _ = @import("core/commit.zig");
    _ = @import("diff/diff_engine.zig");
    _ = @import("diff/unified_diff.zig");
    _ = @import("format/color.zig");
}
