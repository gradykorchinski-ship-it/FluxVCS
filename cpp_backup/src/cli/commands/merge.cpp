#include "flux/core/repository.hpp"
#include "flux/core/commit.hpp"
#include "flux/merge/semantic.hpp"
#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <iostream>
#include <fstream>

// Forward declarations
std::shared_ptr<flux::LanguageParser> create_cpp_parser();
std::string change_type_to_string(flux::SemanticDiff::ChangeType type);

int cmd_merge(int argc, char** argv) {
    CLI::App app{"Merge branches"};
    
    std::string branch_name;
    app.add_option("branch", branch_name, "Branch to merge");
    
    bool abort = false;
    app.add_flag("--abort", abort, "Abort current merge");
    
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }
    
    if (abort) {
        fmt::print("Merge aborted\n");
        return 0;
    }
    
    if (branch_name.empty()) {
        fmt::print(stderr, "Error: branch name required\n");
        return 1;
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
    if (!head_result) {
        fmt::print(stderr, "Error: {}\n", head_result.error());
        return 1;
    }
    
    auto current_ref = *head_result;
    auto current_commit_result = repo.refs().read(current_ref);
    if (!current_commit_result) {
        fmt::print(stderr, "Error: {}\n", current_commit_result.error());
        return 1;
    }
    
    // Get branch to merge
    std::string merge_ref = "refs/heads/" + branch_name;
    auto merge_commit_result = repo.refs().read(merge_ref);
    if (!merge_commit_result) {
        fmt::print(stderr, "Error: Branch '{}' not found\n", branch_name);
        return 1;
    }
    
    // For now, just report what would be merged
    fmt::print("Merging branch '{}' into current branch\n", branch_name);
    
    // Find common ancestor (merge base)
    auto base_id_res = repo.find_merge_base(*current_commit_result, *merge_commit_result);
    if (!base_id_res) {
        fmt::print(stderr, "Error: {}\n", base_id_res.error());
        return 1;
    }
    
    fmt::print("Merge base: {}\n", base_id_res->to_hex());
    
    // Check for fast-forward
    if (*base_id_res == *current_commit_result) {
        fmt::print("Fast-forward possible. Updating {} to {}\n", current_ref, merge_commit_result->to_hex());
        auto res = repo.refs().write(current_ref, *merge_commit_result);
        if (!res) {
            fmt::print(stderr, "Error updating ref: {}\n", res.error());
            return 1;
        }
        fmt::print("Fast-forward successful.\n");
        return 0;
    }
    
    if (*base_id_res == *merge_commit_result) {
        fmt::print("Already up-to-date.\n");
        return 0;
    }
    
    // Get trees for 3-way merge
    auto get_tree_id = [&](const flux::ObjectId& commit_id) -> flux::Result<flux::ObjectId> {
        auto data = repo.objects().read(commit_id);
        if (!data) return flux::unexpected(data.error());
        auto commit = flux::Commit::deserialize(*data);
        if (!commit) return flux::unexpected(commit.error());
        return commit->tree();
    };
    
    auto base_tree = get_tree_id(*base_id_res);
    auto ours_tree = get_tree_id(*current_commit_result);
    auto theirs_tree = get_tree_id(*merge_commit_result);
    
    if (!base_tree || !ours_tree || !theirs_tree) {
        fmt::print(stderr, "Error reading trees\n");
        return 1;
    }
    
    // Perform 3-way tree merge
    auto merged_tree_res = repo.merge_trees(*base_tree, *ours_tree, *theirs_tree);
    if (!merged_tree_res) {
        fmt::print(stderr, "Merge failed: {}\n", merged_tree_res.error());
        return 1;
    }
    
    // Create merge commit
    flux::Commit merge_commit;
    merge_commit.set_tree(*merged_tree_res);
    merge_commit.add_parent(*current_commit_result);
    merge_commit.add_parent(*merge_commit_result);
    merge_commit.set_message(fmt::format("Merge branch '{}'", branch_name));
    
    flux::Signature sig("User", "user@example.com");
    merge_commit.set_author(sig);
    merge_commit.set_committer(sig);
    
    auto commit_data = (repo.hash_algorithm() == flux::HashAlgorithm::SHA1) ? merge_commit.serialize_git() : merge_commit.serialize();
    auto commit_id = repo.objects().write(flux::ObjectType::Commit, commit_data, repo.hash_algorithm());
    
    if (!commit_id) {
        fmt::print(stderr, "Error creating merge commit: {}\n", commit_id.error());
        return 1;
    }
    
    // Update reference
    auto update_res = repo.refs().write(current_ref, *commit_id);
    if (!update_res) {
        fmt::print(stderr, "Error updating ref: {}\n", update_res.error());
        return 1;
    }
    
    fmt::print("Merge successful! New commit: {}\n", commit_id->to_hex());
    
    return 0;
}

int cmd_diff(int argc, char** argv) {
    CLI::App app{"Show differences"};
    
    bool semantic = false;
    app.add_flag("--semantic", semantic, "Show semantic diff");
    
    std::string commit_id;
    app.add_option("commit", commit_id, "Commit to diff against");
    
    std::string file_path;
    app.add_option("file", file_path, "File to diff");
    
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }
    
    if (semantic && !file_path.empty()) {
        // Demonstrate semantic diff on a file
        std::ifstream file(file_path);
        if (!file) {
            fmt::print(stderr, "Error: Cannot open file '{}'\n", file_path);
            return 1;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        // For demonstration, diff against itself (no changes)
        auto parser = create_cpp_parser();
        flux::SemanticDiffer differ(parser);
        
        auto diff_result = differ.diff(content, content);
        if (!diff_result) {
            fmt::print(stderr, "Error: {}\n", diff_result.error());
            return 1;
        }
        
        if (diff_result->changes.empty()) {
            fmt::print("No semantic changes detected\n");
        } else {
            fmt::print("Semantic changes:\n");
            for (const auto& change : diff_result->changes) {
                fmt::print("  {} - {}\n", 
                    change_type_to_string(change.type),
                    change.description);
            }
        }
        
        return 0;
    }
    
    fmt::print("Standard diff not yet implemented\n");
    fmt::print("Use --semantic <file> to see semantic diff\n");
    
    return 0;
}

std::string change_type_to_string(flux::SemanticDiff::ChangeType type) {
    using CT = flux::SemanticDiff::ChangeType;
    switch (type) {
        case CT::ADDED: return "ADDED";
        case CT::REMOVED: return "REMOVED";
        case CT::MODIFIED: return "MODIFIED";
        case CT::MOVED: return "MOVED";
        case CT::RENAMED: return "RENAMED";
        default: return "UNKNOWN";
    }
}
