#include "flux/core/repository.hpp"
#include "flux/net/fetch.hpp"
#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <iostream>

int cmd_checkout(int argc, char** argv) {
    CLI::App app{"Checkout a branch or commit"};
    
    std::string target;
    app.add_option("target", target, "Branch or commit to checkout")->required();
    
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }
    
    auto repo_result = flux::Repository::open(".");
    if (!repo_result) {
        fmt::print(stderr, "Error: {}\n", repo_result.error());
        return 1;
    }
    
    auto& repo = **repo_result;
    flux::Checkout checkout(repo);
    
    // Try as branch first
    auto res = checkout.checkout_branch(target);
    if (!res) {
        // Try as OID
        auto id_res = flux::ObjectId::from_hex(target);
        if (id_res) {
            res = checkout.checkout_commit(*id_res);
        }
    }
    
    if (!res) {
        fmt::print(stderr, "Error checking out '{}': {}\n", target, res.error());
        return 1;
    }
    
    fmt::print("Checked out '{}'\n", target);
    return 0;
}
