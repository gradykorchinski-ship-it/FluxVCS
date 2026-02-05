#include "flux/core/repository.hpp"
#include "flux/core/commit.hpp"
#include "flux/core/tree.hpp"
#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <iostream>
#include <map>

int cmd_commit(int argc, char** argv) {
    CLI::App app{"Create a new commit"};
    
    std::string message;
    app.add_option("-m,--message", message, "Commit message")->required();
    
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
    
    // Get staged files
    auto staged_result = repo.index().get_staged_files();
    if (!staged_result) {
        fmt::print(stderr, "Error: {}\n", staged_result.error());
        return 1;
    }
    
    if (staged_result->empty()) {
        fmt::print(stderr, "Error: Nothing to commit\n");
        fmt::print(stderr, "Suggestion: Use 'flux add <files>' to stage changes\n");
        return 1;
    }
    
    // Create tree from staged files
    std::vector<flux::TreeEntry> entries;
    std::map<std::string, flux::ObjectId> entry_map;
    
    // 1. Start with parent tree entries
    auto head_result = repo.head();
    if (head_result) {
        auto head_ref = repo.refs().read(*head_result);
        if (head_ref) {
            auto commit_data = repo.objects().read(*head_ref);
            if (commit_data) {
                auto parent_commit = flux::Commit::deserialize(*commit_data);
                if (parent_commit) {
                    auto tree_data = repo.objects().read(parent_commit->tree());
                    if (tree_data) {
                        auto parent_tree = flux::Tree::deserialize(*tree_data);
                        if (parent_tree) {
                            for (const auto& entry : parent_tree->entries()) {
                                entry_map[entry.name] = entry.id;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // 2. Overlay staged changes
    for (const auto& [path, id] : *staged_result) {
        entry_map[path.filename().string()] = id;
    }
    
    // 3. Collect all entries
    for (const auto& [name, id] : entry_map) {
        entries.emplace_back(
            name,
            flux::FileMode::Regular,
            id
        );
    }
    
    flux::Tree tree(std::move(entries));
    
    // Write tree to object store
    flux::Bytes tree_data = (repo.hash_algorithm() == flux::HashAlgorithm::SHA1) ? tree.serialize_git() : tree.serialize();
    auto tree_id_result = repo.objects().write(
        flux::ObjectType::Tree,
        tree_data,
        repo.hash_algorithm()
    );
    
    if (!tree_id_result) {
        fmt::print(stderr, "Error writing tree: {}\n", tree_id_result.error());
        return 1;
    }
    
    // Create commit
    flux::Commit commit;
    commit.set_tree(*tree_id_result);
    commit.set_message(message);
    
    // Set author and committer
    flux::Signature sig(repo.user_name(), repo.user_email());
    commit.set_author(sig);
    commit.set_committer(sig);
    
    // Get current HEAD to set as parent
    if (head_result) {
        auto head_ref_result = repo.refs().read(*head_result);
        if (head_ref_result) {
            commit.add_parent(*head_ref_result);
        }
    }
    
    // Write commit to object store
    flux::Bytes commit_data = (repo.hash_algorithm() == flux::HashAlgorithm::SHA1) ? commit.serialize_git() : commit.serialize();
    auto commit_id_result = repo.objects().write(
        flux::ObjectType::Commit,
        commit_data,
        repo.hash_algorithm()
    );
    
    if (!commit_id_result) {
        fmt::print(stderr, "Error writing commit: {}\n", commit_id_result.error());
        return 1;
    }
    
    // Update current branch reference
    if (head_result) {
        auto update_result = repo.refs().write(*head_result, *commit_id_result);
        if (!update_result) {
            fmt::print(stderr, "Error updating branch: {}\n", update_result.error());
            return 1;
        }
    }
    
    // Clear staging area
    auto clear_result = repo.index().clear_staging();
    if (!clear_result) {
        fmt::print(stderr, "Warning: Failed to clear staging area: {}\n", clear_result.error());
    }
    
    fmt::print("[{}] {}\n", commit_id_result->to_hex().substr(0, 12), message);
    
    return 0;
}
