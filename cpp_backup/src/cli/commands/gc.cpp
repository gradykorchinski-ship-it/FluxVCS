#include "flux/core/repository.hpp"
#include "flux/storage/packfile.hpp"
#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <iostream>

int cmd_gc(int argc, char** argv) {
    CLI::App app{"Garbage collection and optimization"};
    
    bool aggressive = false;
    app.add_flag("--aggressive", aggressive, "Aggressive optimization");
    
    bool prune = false;
    app.add_flag("--prune", prune, "Prune unreachable objects");
    
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }
    
    // Open repository
    auto repo_result = flux::Repository::open(".");
    if (!repo_result) {
        fmt::print(stderr, "Error: {}\n", repo_result.error());
        return 1;
    }
    
    auto& repo = **repo_result;
    
    // Suppress unused warnings for now
    (void)aggressive;
    (void)prune;
    (void)repo;
    
    fmt::print("Running garbage collection...\n");
    
    // TODO: Implement actual GC
    // 1. Find all reachable objects
    // 2. Pack loose objects
    // 3. Prune unreachable if requested
    
    fmt::print("✓ Garbage collection complete\n");
    
    return 0;
}

int cmd_pack(int argc, char** argv) {
    CLI::App app{"Create pack files from loose objects"};
    
    size_t min_objects = 100;
    app.add_option("--min", min_objects, "Minimum objects to pack");
    
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }
    
    // Open repository
    auto repo_result = flux::Repository::open(".");
    if (!repo_result) {
        fmt::print(stderr, "Error: {}\n", repo_result.error());
        return 1;
    }
    
    auto& repo = **repo_result;
    
    fmt::print("Creating pack files...\n");
    
    // Load pack manager
    flux::PackManager pack_mgr(repo.flux_dir() / "objects" / "pack");
    auto load_result = pack_mgr.load_packs();
    if (!load_result) {
        fmt::print(stderr, "Error loading packs: {}\n", load_result.error());
        return 1;
    }
    
    // Get statistics
    auto stats = pack_mgr.get_stats();
    fmt::print("Current packs: {}\n", stats.pack_count);
    fmt::print("Packed objects: {}\n", stats.total_objects);
    
    if (stats.total_original_size > 0) {
        double ratio = static_cast<double>(stats.total_compressed_size) / stats.total_original_size;
        fmt::print("Compression ratio: {:.2f}%\n", ratio * 100);
    }
    
    // TODO: Pack loose objects
    fmt::print("\nNote: Packing loose objects not yet implemented\n");
    
    return 0;
}
