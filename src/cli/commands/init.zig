const std = @import("std");
const repository = @import("../../core/repository.zig");

pub fn cmdInit(allocator: std.mem.Allocator, path: []const u8) !void {
    var repo = try repository.Repository.initRepo(allocator, path, .sha256);
    defer {
        repo.deinit();
        allocator.destroy(repo);
    }

    try std.io.getStdOut().writer().print("Initialized empty FluxVCS repository in {s}/.flux/\n", .{path});
}
