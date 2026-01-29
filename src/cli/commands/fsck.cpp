#include "flux/core/repository.hpp"
#include "flux/storage/snapshot.hpp"
#include "flux/verify/integrity.hpp"
#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <iostream>

int cmd_fsck(int argc, char** argv) {
    CLI::App app{"Check repository integrity"};
    
    bool verbose = false;
    app.add_flag("-v,--verbose", verbose, "Verbose output");
    
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
    flux::IntegrityChecker checker(repo);
    
    fmt::print("Checking repository integrity...\n");
    
    // Run all checks
    auto issues_result = checker.check_all();
    if (!issues_result) {
        fmt::print(stderr, "Error running integrity check: {}\n", issues_result.error());
        return 1;
    }
    
    if (issues_result->empty()) {
        fmt::print("✓ No issues found\n");
        
        if (verbose) {
            auto stats_result = checker.get_stats();
            if (stats_result) {
                fmt::print("\nStatistics:\n");
                fmt::print("  Total objects: {}\n", stats_result->total_objects);
                fmt::print("  Commits: {}\n", stats_result->total_commits);
                fmt::print("  Trees: {}\n", stats_result->total_trees);
                fmt::print("  Blobs: {}\n", stats_result->total_blobs);
                fmt::print("  References: {}\n", stats_result->total_refs);
            }
        }
        
        return 0;
    }
    
    // Report issues
    fmt::print("✗ Found {} issue(s):\n\n", issues_result->size());
    
    for (const auto& issue : *issues_result) {
        fmt::print("Issue: {}\n", issue.message);
        if (!issue.suggestion.empty()) {
            fmt::print("  Suggestion: {}\n", issue.suggestion);
        }
        if (issue.object_id) {
            fmt::print("  Object: {}\n", issue.object_id->to_hex());
        }
        if (issue.ref_name) {
            fmt::print("  Reference: {}\n", *issue.ref_name);
        }
        fmt::print("\n");
    }
    
    return 1;
}

int cmd_snapshot(int argc, char** argv) {
    CLI::App app{"Manage repository snapshots"};
    
    std::string subcommand;
    app.add_option("command", subcommand, "Subcommand: create, list, restore")
        ->required()
        ->check(CLI::IsMember({"create", "list", "restore"}));
    
    std::string description;
    app.add_option("-m,--message", description, "Snapshot description");
    
    uint64_t snapshot_id = 0;
    app.add_option("--id", snapshot_id, "Snapshot ID (for restore)");
    
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
    
    if (subcommand == "create") {
        auto snapshot_result = flux::Snapshot::create(repo, description);
        if (!snapshot_result) {
            fmt::print(stderr, "Error creating snapshot: {}\n", snapshot_result.error());
            return 1;
        }
        
        fmt::print("Created snapshot {}\n", snapshot_result->id());
        return 0;
    }
    
    if (subcommand == "list") {
        auto snapshots_result = flux::Snapshot::list(repo.flux_dir() / "snapshots");
        if (!snapshots_result) {
            fmt::print(stderr, "Error listing snapshots: {}\n", snapshots_result.error());
            return 1;
        }
        
        if (snapshots_result->empty()) {
            fmt::print("No snapshots found\n");
            return 0;
        }
        
        fmt::print("Snapshots:\n");
        for (const auto& snapshot : *snapshots_result) {
            auto time_t_val = std::chrono::system_clock::to_time_t(snapshot.timestamp());
            fmt::print("  {:016x} - {} - {}\n", 
                snapshot.id(),
                std::ctime(&time_t_val),
                snapshot.description());
        }
        
        return 0;
    }
    
    if (subcommand == "restore") {
        if (snapshot_id == 0) {
            fmt::print(stderr, "Error: --id required for restore\n");
            return 1;
        }
        
        // Find and load snapshot
        auto snapshots_result = flux::Snapshot::list(repo.flux_dir() / "snapshots");
        if (!snapshots_result) {
            fmt::print(stderr, "Error: {}\n", snapshots_result.error());
            return 1;
        }
        
        flux::Snapshot* target_snapshot = nullptr;
        for (auto& snapshot : *snapshots_result) {
            if (snapshot.id() == snapshot_id) {
                target_snapshot = &snapshot;
                break;
            }
        }
        
        if (!target_snapshot) {
            fmt::print(stderr, "Error: Snapshot {:016x} not found\n", snapshot_id);
            return 1;
        }
        
        auto restore_result = target_snapshot->restore(repo);
        if (!restore_result) {
            fmt::print(stderr, "Error restoring snapshot: {}\n", restore_result.error());
            return 1;
        }
        
        fmt::print("Restored snapshot {:016x}\n", snapshot_id);
        return 0;
    }
    
    return 0;
}
