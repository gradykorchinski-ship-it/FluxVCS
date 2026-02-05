#include "flux/core/repository.hpp"
#include "flux/core/commit.hpp"
#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <iostream>
#include <vector>

int cmd_log(int argc, char** argv) {
    CLI::App app{"Show commit history"};
    
    bool oneline = false;
    app.add_flag("--oneline", oneline, "Show condensed output");
    
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
    
    // Get HEAD
    auto head_result = repo.head();
    if (!head_result) {
        fmt::print(stderr, "Error: {}\n", head_result.error());
        return 1;
    }
    
    // Get current commit
    auto commit_id_result = repo.refs().read(*head_result);
    if (!commit_id_result) {
        fmt::print("No commits yet\n");
        return 0;
    }
    
    // Traverse commit history
    flux::ObjectId current_id = *commit_id_result;
    
    while (true) {
        // Read commit object
        auto commit_data_result = repo.objects().read(current_id);
        if (!commit_data_result) {
            break;
        }
        
        auto commit_result = flux::Commit::deserialize(*commit_data_result);
        if (!commit_result) {
            break;
        }
        
        auto& commit = *commit_result;
        
        if (oneline) {
            // Condensed format
            std::string short_id = current_id.to_hex().substr(0, 12);
            std::string short_msg = commit.message();
            if (auto pos = short_msg.find('\n'); pos != std::string::npos) {
                short_msg = short_msg.substr(0, pos);
            }
            fmt::print("{} {}\n", short_id, short_msg);
        } else {
            // Full format
            fmt::print("commit {}\n", current_id.to_hex());
            fmt::print("Author: {}\n", commit.author().format());
            fmt::print("Date:   {}\n", commit.author().format());
            fmt::print("\n    {}\n\n", commit.message());
        }
        
        // Move to parent
        if (commit.parents().empty()) {
            break;
        }
        
        current_id = commit.parents()[0];
    }
    
    return 0;
}
