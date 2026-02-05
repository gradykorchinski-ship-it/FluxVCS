#include "flux/core/repository.hpp"
#include "flux/security/gpg.hpp"
#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <iostream>

int cmd_sign(int argc, char** argv) {
    CLI::App app{"Sign commits with GPG"};
    
    std::string commit_id;
    app.add_option("commit", commit_id, "Commit to sign")->required();
    
    std::string key_id;
    app.add_option("-k,--key", key_id, "GPG key ID to use");
    
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
    flux::GPGSigner signer;
    
    // Get default key if not specified
    if (key_id.empty()) {
        auto key_result = signer.get_default_key();
        if (!key_result) {
            fmt::print(stderr, "Error: {}\n", key_result.error());
            return 1;
        }
        key_id = *key_result;
    }
    
    // Load commit
    auto oid_result = flux::ObjectId::from_hex(commit_id);
    if (!oid_result) {
        fmt::print(stderr, "Error: Invalid commit ID\n");
        return 1;
    }
    
    auto commit_data = repo.objects().read(*oid_result);
    if (!commit_data) {
        fmt::print(stderr, "Error: Commit not found\n");
        return 1;
    }
    
    auto commit_result = flux::Commit::deserialize(*commit_data);
    if (!commit_result) {
        fmt::print(stderr, "Error: {}\n", commit_result.error());
        return 1;
    }
    
    // Sign commit
    auto signed_result = flux::SignedCommit::create(*commit_result, signer, key_id);
    if (!signed_result) {
        fmt::print(stderr, "Error: {}\n", signed_result.error());
        return 1;
    }
    
    fmt::print("✓ Commit signed with key {}\n", key_id);
    
    return 0;
}

int cmd_verify(int argc, char** argv) {
    CLI::App app{"Verify GPG signatures"};
    
    std::string commit_id;
    app.add_option("commit", commit_id, "Commit to verify");
    
    bool verify_all = false;
    app.add_flag("--all", verify_all, "Verify all commits");
    
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
    flux::GPGSigner signer;
    
    if (verify_all) {
        fmt::print("Verifying all commits...\n");
        fmt::print("Note: Full verification not yet implemented\n");
        return 0;
    }
    
    if (commit_id.empty()) {
        fmt::print(stderr, "Error: commit ID required\n");
        return 1;
    }
    
    // Load and verify commit
    auto oid_result = flux::ObjectId::from_hex(commit_id);
    if (!oid_result) {
        fmt::print(stderr, "Error: Invalid commit ID\n");
        return 1;
    }
    
    auto commit_data = repo.objects().read(*oid_result);
    if (!commit_data) {
        fmt::print(stderr, "Error: Commit not found\n");
        return 1;
    }
    
    // Try to deserialize as signed commit
    auto signed_result = flux::SignedCommit::deserialize(*commit_data);
    if (!signed_result) {
        fmt::print("Commit is not signed\n");
        return 1;
    }
    
    // Verify signature
    std::string signer_key;
    auto verify_result = signed_result->verify(signer, &signer_key);
    if (!verify_result) {
        fmt::print(stderr, "Error: {}\n", verify_result.error());
        return 1;
    }
    
    if (*verify_result) {
        fmt::print("✓ Good signature from {}\n", signer_key);
    } else {
        fmt::print("✗ Bad signature\n");
        return 1;
    }
    
    return 0;
}

int cmd_gpg(int argc, char** argv) {
    CLI::App app{"GPG key management"};
    
    std::string action;
    app.add_option("action", action, "Action: list, show")->required();
    
    std::string key_id;
    app.add_option("key", key_id, "Key ID");
    
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }
    
    flux::GPGSigner signer;
    
    if (action == "list") {
        auto keys_result = signer.list_keys();
        if (!keys_result) {
            fmt::print(stderr, "Error: {}\n", keys_result.error());
            return 1;
        }
        
        if (keys_result->empty()) {
            fmt::print("No GPG keys found\n");
        } else {
            fmt::print("GPG Keys:\n");
            for (const auto& key : *keys_result) {
                fmt::print("  {} - {}\n", key.key_id, key.user_id);
                fmt::print("    Fingerprint: {}\n", key.fingerprint);
                fmt::print("    Can sign: {}, Can encrypt: {}\n", 
                    key.can_sign ? "yes" : "no",
                    key.can_encrypt ? "yes" : "no");
            }
        }
        
    } else if (action == "show") {
        if (key_id.empty()) {
            fmt::print(stderr, "Error: key ID required\n");
            return 1;
        }
        
        auto key_result = signer.get_key(key_id);
        if (!key_result) {
            fmt::print(stderr, "Error: {}\n", key_result.error());
            return 1;
        }
        
        fmt::print("Key: {}\n", key_result->key_id);
        fmt::print("User: {}\n", key_result->user_id);
        fmt::print("Fingerprint: {}\n", key_result->fingerprint);
        
    } else {
        fmt::print(stderr, "Error: unknown action '{}'\n", action);
        fmt::print("Valid actions: list, show\n");
        return 1;
    }
    
    return 0;
}
