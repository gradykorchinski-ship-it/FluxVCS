const std = @import("std");
const types = @import("types.zig");
const object_store_module = @import("../storage/object_store.zig");
const ref_store_module = @import("../storage/ref_store.zig");

const HashAlgorithm = types.HashAlgorithm;
const ObjectStore = object_store_module.ObjectStore;
const RefStore = ref_store_module.RefStore;

/// Repository - Main repository class
pub const Repository = struct {
    repo_path: []const u8,
    hash_algorithm: HashAlgorithm,
    object_store: ObjectStore,
    ref_store: RefStore,
    user_name: []const u8,
    user_email: []const u8,
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator, repo_path: []const u8, algo: HashAlgorithm) !Repository {
        const flux_dir = try std.fs.path.join(allocator, &[_][]const u8{ repo_path, ".flux" });
        defer allocator.free(flux_dir);

        const objects_dir = try std.fs.path.join(allocator, &[_][]const u8{ flux_dir, "objects" });
        errdefer allocator.free(objects_dir);
        const refs_dir = try std.fs.path.join(allocator, &[_][]const u8{ flux_dir, "refs" });
        errdefer allocator.free(refs_dir);

        return .{
            .allocator = allocator,
            .repo_path = try allocator.dupe(u8, repo_path),
            .hash_algorithm = algo,
            .object_store = ObjectStore.init(allocator, objects_dir),
            .ref_store = RefStore.init(allocator, refs_dir),
            .user_name = try allocator.dupe(u8, ""),
            .user_email = try allocator.dupe(u8, ""),
        };
    }

    pub fn deinit(self: *Repository) void {
        self.allocator.free(self.repo_path);
        self.allocator.free(self.object_store.objects_dir);
        self.allocator.free(self.ref_store.refs_dir);
        self.allocator.free(self.user_name);
        self.allocator.free(self.user_email);
    }

    /// Initialize new repository
    pub fn initRepo(allocator: std.mem.Allocator, path: []const u8, algo: HashAlgorithm) !*Repository {
        // Create directory structure
        const flux_dir = try std.fs.path.join(allocator, &[_][]const u8{ path, ".flux" });
        defer allocator.free(flux_dir);

        try std.fs.cwd().makePath(flux_dir);
        try std.fs.cwd().makePath(try std.fs.path.join(allocator, &[_][]const u8{ flux_dir, "objects" }));
        try std.fs.cwd().makePath(try std.fs.path.join(allocator, &[_][]const u8{ flux_dir, "refs", "heads" }));
        try std.fs.cwd().makePath(try std.fs.path.join(allocator, &[_][]const u8{ flux_dir, "refs", "tags" }));

        const repo = try allocator.create(Repository);
        repo.* = try Repository.init(allocator, path, algo);

        // Set initial HEAD
        try repo.ref_store.writeSymbolic("HEAD", "refs/heads/main");

        return repo;
    }

    /// Open existing repository
    pub fn open(allocator: std.mem.Allocator, path: []const u8) !*Repository {
        const repo = try allocator.create(Repository);
        repo.* = try Repository.init(allocator, path, .sha256);
        return repo;
    }

    pub fn setUserInfo(self: *Repository, name: []const u8, email: []const u8) !void {
        self.allocator.free(self.user_name);
        self.allocator.free(self.user_email);
        self.user_name = try self.allocator.dupe(u8, name);
        self.user_email = try self.allocator.dupe(u8, email);
    }
};
