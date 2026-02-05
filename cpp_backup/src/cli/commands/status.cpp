#include "flux/core/repository.hpp"
#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <iostream>

int cmd_status(int argc, char** argv) {
    CLI::App app{"Show working tree status"};
    
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
    
    // Get current branch
    auto head_result = repo.head();
    if (head_result) {
        std::string branch = *head_result;
        if (branch.starts_with("refs/heads/")) {
            branch = branch.substr(11);
        }
        fmt::print("On branch {}\n", branch);
    }
    
    // Get all status
    auto status_result = repo.index().get_all_status();
    if (!status_result) {
        fmt::print(stderr, "Error: {}\n", status_result.error());
        return 1;
    }
    
    if (status_result->empty()) {
        fmt::print("\nNothing to commit, working tree clean\n");
        return 0;
    }
    
    // Show staged files
    fmt::print("\nChanges to be committed:\n");
    fmt::print("  (use \"flux reset <file>...\" to unstage)\n\n");
    
    for (const auto& status : *status_result) {
        if (status.staged) {
            fmt::print("\t{}: {}\n", "new file", status.path.string());
        }
    }
    
    return 0;
}
