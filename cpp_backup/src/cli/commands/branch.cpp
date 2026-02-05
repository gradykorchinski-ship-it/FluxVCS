#include "flux/core/repository.hpp"
#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <iostream>

int cmd_branch(int argc, char** argv) {
    CLI::App app{"List, create, or delete branches"};
    
    std::string branch_name;
    app.add_option("name", branch_name, "Branch name to create");
    
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
    
    if (branch_name.empty()) {
        // List branches
        auto refs_result = repo.refs().list();
        if (!refs_result) {
            fmt::print(stderr, "Error: {}\n", refs_result.error());
            return 1;
        }
        
        auto head_result = repo.head();
        std::string current_branch;
        if (head_result) {
            current_branch = *head_result;
        }
        
        for (const auto& ref : *refs_result) {
            if (ref.starts_with("refs/heads/")) {
                std::string name = ref.substr(11);  // Remove "refs/heads/"
                if (ref == current_branch) {
                    fmt::print("* {}\n", name);
                } else {
                    fmt::print("  {}\n", name);
                }
            }
        }
    } else {
        // Create branch
        std::string ref_name = "refs/heads/" + branch_name;
        
        // Get current HEAD commit
        auto head_result = repo.head();
        if (!head_result) {
            fmt::print(stderr, "Error: No commits yet\n");
            return 1;
        }
        
        auto commit_result = repo.refs().read(*head_result);
        if (!commit_result) {
            fmt::print(stderr, "Error: {}\n", commit_result.error());
            return 1;
        }
        
        // Create new branch pointing to same commit
        auto create_result = repo.refs().write(ref_name, *commit_result);
        if (!create_result) {
            fmt::print(stderr, "Error: {}\n", create_result.error());
            return 1;
        }
        
        fmt::print("Created branch '{}'\n", branch_name);
    }
    
    return 0;
}
