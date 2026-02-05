#include "flux/core/repository.hpp"
#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <iostream>

int cmd_config(int argc, char** argv) {
    CLI::App app{"Get and set repository configuration options"};
    
    std::string key;
    std::string value;
    app.add_option("key", key, "Configuration key (e.g., user.name)")->required();
    app.add_option("value", value, "New value for the key");
    
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
    
    if (argc == 2) {
        // GET mode
        if (key == "user.name") {
            fmt::print("{}\n", repo.user_name());
        } else if (key == "user.email") {
            fmt::print("{}\n", repo.user_email());
        } else if (key == "hash_algorithm") {
            fmt::print("{}\n", (repo.hash_algorithm() == flux::HashAlgorithm::SHA1 ? "sha1" : "sha256"));
        } else {
            fmt::print(stderr, "Error: Unknown configuration key '{}'\n", key);
            return 1;
        }
    } else {
        // SET mode
        if (key == "user.name") {
            repo.set_user_info(value, repo.user_email());
            fmt::print("Set user.name to '{}'\n", value);
        } else if (key == "user.email") {
            repo.set_user_info(repo.user_name(), value);
            fmt::print("Set user.email to '{}'\n", value);
        } else {
            fmt::print(stderr, "Error: Unknown or read-only configuration key '{}'\n", key);
            return 1;
        }
    }
    
    return 0;
}
