const std = @import("std");

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var args = try std.process.argsWithAllocator(allocator);
    defer args.deinit();

    // Skip program name
    _ = args.skip();

    const command = args.next() orelse {
        try printUsage();
        return;
    };

    const stdout = std.fs.File.stdout();
    const stderr = std.fs.File.stderr();

    if (std.mem.eql(u8, command, "version")) {
        try stdout.writeAll("FluxVCS 0.1.1 (Zig)\n");
    } else if (std.mem.eql(u8, command, "help")) {
        try printUsage();
    } else {
        try stderr.writeAll("Unknown command: ");
        try stderr.writeAll(command);
        try stderr.writeAll("\n");
        try printUsage();
        std.process.exit(1);
    }
}

fn printUsage() !void {
    const usage =
        \\FluxVCS - A modern distributed version control system
        \\
        \\Usage: flux <command> [<args>]
        \\
        \\Commands:
        \\  version    Show version information
        \\  help       Show this help message
        \\  
        \\  Basic commands:
        \\  init       Initialize a new repository
        \\  add        Add files to staging area
        \\  commit     Create a new commit
        \\  status     Show working tree status
        \\  log        Show commit history
        \\  
        \\  Branching and merging:
        \\  branch     List, create, or delete branches
        \\  checkout   Switch branches or restore files
        \\  merge      Merge branches
        \\  
        \\  Inspection:
        \\  diff       Show changes between commits
        \\  show       Show commit information
        \\  
        \\  Tagging:
        \\  tag        Create, list, or delete tags
        \\  
        \\  Undo changes:
        \\  reset      Reset current HEAD to specified state
        \\  
        \\  Advanced:
        \\  fsck       Verify repository integrity
        \\  gc         Garbage collect loose objects
        \\  gpg        GPG signature management
        \\  config     Get and set repository options
        \\  network    Network operations
        \\
        \\Note: Full CLI implementation in progress
        \\
    ;
    const stdout = std.fs.File.stdout();
    try stdout.writeAll(usage);
}
