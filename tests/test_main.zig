// Test entry point - runs all unit tests
const std = @import("std");

// No explicit tests here - the lib module has all tests embedded
// Since we link to the lib in build.zig, all tests will run
test {
    std.testing.refAllDecls(@This());
}
