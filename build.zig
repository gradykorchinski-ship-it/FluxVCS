const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Create modules
    const lib_module = b.createModule(.{
        .root_source_file = b.path("src/lib.zig"),
        .target = target,
        .optimize = optimize,
    });

    const exe_module = b.createModule(.{
        .root_source_file = b.path("src/cli/main.zig"),
        .target = target,
        .optimize = optimize,
    });

    const test_module = b.createModule(.{
        .root_source_file = b.path("tests/test_main.zig"),
        .target = target,
        .optimize = optimize,
    });

    // Static library
    const lib = b.addLibrary(.{
        .name = "flux_core",
        .root_module = lib_module,
    });

    lib.linkLibC();
    lib.linkSystemLibrary("sqlite3");
    lib.linkSystemLibrary("ssl");
    lib.linkSystemLibrary("crypto");
    lib.linkSystemLibrary("zstd");
    lib.linkSystemLibrary("curl");
    lib.linkSystemLibrary("z");

    b.installArtifact(lib);

    // Executable
    const exe = b.addExecutable(.{
        .name = "flux",
        .root_module = exe_module,
    });

    exe.linkLibrary(lib);
    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);

    // Unit tests
    const unit_tests = b.addTest(.{
        .root_module = test_module,
    });

    unit_tests.linkLibrary(lib);

    const run_unit_tests = b.addRunArtifact(unit_tests);

    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&run_unit_tests.step);
}
