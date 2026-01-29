#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include "flux/net/http_transport.hpp"
#include "flux/net/fetch.hpp"
#include "flux/core/repository.hpp"
#include "flux/core/auth.hpp"
#include "flux/core/blob.hpp"
#include "flux/core/tree.hpp"
#include "flux/core/commit.hpp"
#include "flux/net/push.hpp"
#include "flux/util/filesystem.hpp"
#include <iostream>

// Forward declarations
int cmd_init(int argc, char** argv);
int cmd_add(int argc, char** argv);
int cmd_commit(int argc, char** argv);
int cmd_branch(int argc, char** argv);
int cmd_log(int argc, char** argv);
int cmd_status(int argc, char** argv);
int cmd_fsck(int argc, char** argv);
int cmd_snapshot(int argc, char** argv);
int cmd_merge(int argc, char** argv);
int cmd_diff(int argc, char** argv);
int cmd_gc(int argc, char** argv);
int cmd_pack(int argc, char** argv);
int cmd_sign(int argc, char** argv);
int cmd_verify(int argc, char** argv);
int cmd_gpg(int argc, char** argv);
int cmd_clone(int argc, char** argv);
int cmd_remote(int argc, char** argv);
int cmd_fetch(int argc, char** argv);
int cmd_pull(int argc, char** argv);
int cmd_push(int argc, char** argv);
int cmd_checkout(int argc, char** argv);
int cmd_config(int argc, char** argv);
// int cmd_auth(int argc, char** argv); // Removed delegation

int main(int argc, char** argv) {
    CLI::App app{"FluxVCS - A modern distributed version control system"};
    app.require_subcommand(1);
    
    // Init command
    auto init_cmd = app.add_subcommand("init", "Initialize a new repository");
    static std::string init_path = ".";
    static std::string init_hash = "sha256";
    init_cmd->add_option("path", init_path, "Path to initialize repository");
    init_cmd->add_option("--hash", init_hash, "Hash algorithm (sha1, sha256)")
        ->check(CLI::IsMember({"sha1", "sha256"}));
    init_cmd->allow_extras();
    init_cmd->callback([argc, argv]() { std::exit(cmd_init(argc - 1, argv + 1)); });
    
    // Add command
    auto add_cmd = app.add_subcommand("add", "Add files to staging area");
    add_cmd->allow_extras();
    add_cmd->callback([argc, argv]() { std::exit(cmd_add(argc - 1, argv + 1)); });
    
    // Commit command
    auto commit_cmd = app.add_subcommand("commit", "Create a new commit");
    static std::string commit_msg;
    commit_cmd->add_option("-m,--message", commit_msg, "Commit message")->required();
    commit_cmd->allow_extras();
    commit_cmd->callback([argc, argv]() { std::exit(cmd_commit(argc - 1, argv + 1)); });
    
    // Branch command
    auto branch_cmd = app.add_subcommand("branch", "List, create, or delete branches");
    branch_cmd->allow_extras();
    branch_cmd->callback([argc, argv]() { std::exit(cmd_branch(argc - 1, argv + 1)); });
    
    // Log command
    auto log_cmd = app.add_subcommand("log", "Show commit history");
    log_cmd->allow_extras();
    log_cmd->callback([argc, argv]() { std::exit(cmd_log(argc - 1, argv + 1)); });
    
    // Checkout command
    auto checkout_cmd = app.add_subcommand("checkout", "Checkout a branch or commit");
    checkout_cmd->allow_extras();
    checkout_cmd->callback([argc, argv]() { std::exit(cmd_checkout(argc - 1, argv + 1)); });
    
    // Status command
    auto status_cmd = app.add_subcommand("status", "Show working tree status");
    status_cmd->allow_extras();
    status_cmd->callback([argc, argv]() { std::exit(cmd_status(argc - 1, argv + 1)); });
    
    // Fsck command
    auto fsck_cmd = app.add_subcommand("fsck", "Check repository integrity");
    fsck_cmd->allow_extras();
    fsck_cmd->callback([argc, argv]() { std::exit(cmd_fsck(argc - 1, argv + 1)); });
    
    // Snapshot command
    auto snapshot_cmd = app.add_subcommand("snapshot", "Manage repository snapshots");
    snapshot_cmd->allow_extras();
    snapshot_cmd->callback([argc, argv]() { std::exit(cmd_snapshot(argc - 1, argv + 1)); });
    
    // Merge command
    auto merge_cmd = app.add_subcommand("merge", "Merge branches");
    merge_cmd->allow_extras();
    merge_cmd->callback([argc, argv]() { std::exit(cmd_merge(argc - 1, argv + 1)); });
    
    // Diff command
    auto diff_cmd = app.add_subcommand("diff", "Show differences");
    diff_cmd->allow_extras();
    diff_cmd->callback([argc, argv]() { std::exit(cmd_diff(argc - 1, argv + 1)); });
    
    // GC command
    auto gc_cmd = app.add_subcommand("gc", "Garbage collection");
    gc_cmd->allow_extras();
    gc_cmd->callback([argc, argv]() { std::exit(cmd_gc(argc - 1, argv + 1)); });
    
    // Pack command
    auto pack_cmd = app.add_subcommand("pack", "Manage pack files");
    pack_cmd->allow_extras();
    pack_cmd->callback([argc, argv]() { std::exit(cmd_pack(argc - 1, argv + 1)); });
    
    // Sign command
    auto sign_cmd = app.add_subcommand("sign", "Sign commits");
    sign_cmd->allow_extras();
    sign_cmd->callback([argc, argv]() { std::exit(cmd_sign(argc - 1, argv + 1)); });
    
    // Verify command
    auto verify_cmd = app.add_subcommand("verify", "Verify signatures");
    verify_cmd->allow_extras();
    verify_cmd->callback([argc, argv]() { std::exit(cmd_verify(argc - 1, argv + 1)); });
    
    // GPG command
    auto gpg_cmd = app.add_subcommand("gpg", "GPG key management");
    gpg_cmd->allow_extras();
    gpg_cmd->callback([argc, argv]() { std::exit(cmd_gpg(argc - 1, argv + 1)); });
    
    // Clone command
    auto clone_cmd = app.add_subcommand("clone", "Clone a repository");
    clone_cmd->allow_extras();
    std::string clone_url;
    std::string clone_dir;
    std::string clone_branch;
    std::string clone_token;
    std::string clone_hash = "sha256";
    clone_cmd->add_option("url", clone_url, "Repository URL")->required();
    clone_cmd->add_option("directory", clone_dir, "Target directory");
    clone_cmd->add_option("-b,--branch", clone_branch, "Branch to checkout");
    clone_cmd->add_option("-t,--token", clone_token, "GitHub Personal Access Token");
    clone_cmd->add_option("--hash", clone_hash, "Hash algorithm (sha1, sha256)")
        ->check(CLI::IsMember({"sha1", "sha256"}));
    clone_cmd->callback([&]() {
        // Parse URL to get directory name if not specified
        std::string directory = clone_dir;
        if (directory.empty()) {
            auto url_result = flux::GitUrl::parse(clone_url);
            if (!url_result) {
                fmt::print(stderr, "Error: {}\n", url_result.error());
                std::exit(1);
            }
            
            // Extract repo name from path
            std::string path = url_result->path;
            auto last_slash = path.find_last_of('/');
            if (last_slash != std::string::npos) {
                directory = path.substr(last_slash + 1);
            } else {
                directory = path;
            }
            
            // Remove .git suffix
            if (directory.ends_with(".git")) {
                directory = directory.substr(0, directory.size() - 4);
            }
        }
        
        fmt::print("Cloning into '{}'...\n", directory);
        
        // Initialize repository
        flux::HashAlgorithm algo = (clone_hash == "sha1") ? 
            flux::HashAlgorithm::SHA1 : flux::HashAlgorithm::SHA256;
        auto init_result = flux::Repository::init(directory, algo);
        if (!init_result) {
            fmt::print(stderr, "Error: {}\n", init_result.error());
            std::exit(1);
        }
        
        auto& repo = **init_result;
        
        // Fetch from remote
        flux::Fetcher fetcher(repo);
        
        // Try to get token from env or auth manager if not provided
        if (clone_token.empty()) {
            const char* env_token = std::getenv("FLUX_TOKEN");
            if (env_token) {
                clone_token = env_token;
            } else {
                auto auth_token = flux::AuthManager::instance().get_token();
                if (auth_token) {
                    clone_token = *auth_token;
                    fmt::print("Using saved token (expires on {})\n", flux::AuthManager::instance().get_expiration_string());
                } else if (flux::AuthManager::instance().is_expired()) {
                    fmt::print(stderr, "Warning: Saved token is expired! (expired on {})\n", 
                               flux::AuthManager::instance().get_expiration_string());
                }
            }
        }
        
        if (!clone_token.empty()) {
            fetcher.set_token(clone_token);
        }
        
        auto fetch_result = fetcher.fetch_all(clone_url);
        
        // After any successful network op, try to update stored expiration
        // (Moved to Fetcher::fetch_all implementation)

        if (!fetch_result) {
            fmt::print(stderr, "Error fetching: {}\n", fetch_result.error());
            fmt::print("Repository initialized but fetch failed\n");
            std::exit(1);
        }
        
        // Checkout
        fmt::print("Checking out files...\n");
        flux::Checkout checkout(repo);
        
        flux::Result<void> checkout_result = flux::unexpected("No branch to checkout");
        if (!clone_branch.empty()) {
            checkout_result = checkout.checkout_branch(clone_branch);
        } else {
            // Try common default branches
            checkout_result = checkout.checkout_branch("main");
            if (!checkout_result) {
                checkout_result = checkout.checkout_branch("master");
            }
        }
        
        if (!checkout_result) {
            fmt::print(stderr, "Warning: Could not checkout default branch: {}\n", 
                      checkout_result.error());
        }
        
        fmt::print("Done!\n");
    });
    
    // Remote command  
    auto remote_cmd = app.add_subcommand("remote", "Manage remotes");
    remote_cmd->allow_extras();
    remote_cmd->callback([argc, argv]() { std::exit(cmd_remote(argc, argv)); });

    // Auth command
    auto auth_cmd = app.add_subcommand("auth", "Manage authentication");
    auth_cmd->require_subcommand(1);
    
    // Auth login
    auto auth_login_cmd = auth_cmd->add_subcommand("login", "Save a GitHub Personal Access Token");
    static std::string login_token;
    static int expires_days = 30;
    static std::string expires_date;
    auth_login_cmd->add_option("token", login_token, "GitHub Personal Access Token")->required();
    auth_login_cmd->add_option("-d,--days", expires_days, "Days until expiration (default: 30)");
    auth_login_cmd->add_option("-e,--expires", expires_date, "Expiration date (YYYY-MM-DD)");
    auth_login_cmd->callback([&]() {
        auto& auth = flux::AuthManager::instance();
        flux::Result<void> res;
        
        if (!expires_date.empty()) {
            std::tm tm = {};
            std::istringstream ss(expires_date);
            ss >> std::get_time(&tm, "%Y-%m-%d");
            if (ss.fail()) {
                fmt::print(stderr, "Error: Invalid date format. Use YYYY-MM-DD\n");
                std::exit(1);
            }
            auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
            res = auth.save_token_with_date(login_token, tp);
        } else {
            // No date/days provided - try to fetch from GitHub automatically
            res = auth.save_token(login_token, 30); // Save as 30 days initially
            if (res) {
                fmt::print("Verifying token with GitHub and fetching expiration...\n");
                flux::HttpTransport transport;
                transport.set_token(login_token);
                // Simple request to GitHub API to get the header
                auto probe = transport.get("https://api.github.com/user");
                auto header = transport.get_last_github_token_expiry();
                if (!header.empty()) {
                    auth.update_expiration_from_header(header);
                    fmt::print("Verified! GitHub reported real expiration.\n");
                } else if (!probe) {
                    fmt::print(stderr, "Warning: Could not verify token with GitHub: {}\n", probe.error());
                } else {
                    fmt::print("Verified, but GitHub didn't provide expiration header (token might not expire).\n");
                }
            }
        }

        if (res) {
            fmt::print("Successfully logged in! Token expires on {}\n", auth.get_expiration_string());
        } else {
            fmt::print(stderr, "Error: {}\n", res.error());
            std::exit(1);
        }
    });

    // Auth status
    auto auth_status_cmd = auth_cmd->add_subcommand("status", "Check authentication status");
    auth_status_cmd->callback([]() {
        auto& auth = flux::AuthManager::instance();
        auto token_res = auth.get_token();
        
        if (token_res) {
            std::string token_val = *token_res;
            std::string masked = token_val.substr(0, 4) + "..." + token_val.substr(token_val.size() - 4);
            fmt::print("Logged in with token: {}\n", masked);
            fmt::print("Expires on: {}\n", auth.get_expiration_string());
            if (auth.is_expired()) {
                fmt::print(stderr, "WARNING: Token is expired!\n");
            }
        } else {
            fmt::print("Not logged in. Use 'flux auth login' to authenticate.\n");
            if (auth.is_expired()) {
                auto exp = auth.get_expiration_string();
                if (exp != "None") {
                    fmt::print(stderr, "Previous token expired on {}\n", exp);
                }
            }
        }
    });

    // Auth logout
    auto auth_logout_cmd = auth_cmd->add_subcommand("logout", "Clear saved credentials");
    auth_logout_cmd->callback([]() {
        auto res = flux::AuthManager::instance().clear();
        if (res) {
            fmt::print("Successfully logged out.\n");
        } else {
            fmt::print(stderr, "Error: {}\n", res.error());
            std::exit(1);
        }
    });
    
    // Fetch command
    auto fetch_cmd = app.add_subcommand("fetch", "Fetch from remote");
    std::string fetch_token;
    fetch_cmd->add_option("-t,--token", fetch_token, "GitHub Personal Access Token");
    fetch_cmd->callback([argc, argv]() { std::exit(cmd_fetch(argc, argv)); });
    
    // Pull command
    auto pull_cmd = app.add_subcommand("pull", "Pull from remote");
    std::string pull_token;
    pull_cmd->add_option("-t,--token", pull_token, "GitHub Personal Access Token");
    pull_cmd->callback([argc, argv]() { std::exit(cmd_pull(argc, argv)); });
    
    // Push command
    auto push_cmd = app.add_subcommand("push", "Push to remote");
    static std::string push_url;
    static std::string push_branch;
    static std::string push_token_val;
    push_cmd->add_option("url", push_url, "Remote URL");
    push_cmd->add_option("branch", push_branch, "Local branch to push (default: main)");
    push_cmd->add_option("-t,--token", push_token_val, "GitHub Personal Access Token");
    push_cmd->callback([&]() {
        // Open repository
        auto repo_res = flux::Repository::open(".");
        if (!repo_res) {
            fmt::print(stderr, "Error: {}\n", repo_res.error());
            std::exit(1);
        }
        auto& repo = **repo_res;
        
        std::string branch = push_branch.empty() ? "main" : push_branch;
        
        // Find remote URL if not provided
        std::string url = push_url;
        if (url.empty()) {
            // Default to 'origin' if it exists. 
            // For now, let's just make it required if no origin is configured.
            fmt::print(stderr, "Error: Remote URL is required.\n");
            std::exit(1);
        }

        flux::Pusher pusher(repo);
        
        // Handle token
        if (push_token_val.empty()) {
            const char* env_token = std::getenv("FLUX_TOKEN");
            if (env_token) {
                push_token_val = env_token;
            } else {
                auto auth_token = flux::AuthManager::instance().get_token();
                if (auth_token) {
                    push_token_val = *auth_token;
                    fmt::print("Using saved token (expires on {})\n", flux::AuthManager::instance().get_expiration_string());
                }
            }
        }
        
        if (!push_token_val.empty()) {
            pusher.set_token(push_token_val);
        }

        auto result = pusher.push(url, branch);
        if (!result) {
            fmt::print(stderr, "Error pushing: {}\n", result.error());
            std::exit(1);
        }
        
        fmt::print("Push successful!\n");
    });
    // Config command
    auto config_cmd = app.add_subcommand("config", "Get and set repository configuration options");
    config_cmd->allow_extras();
    config_cmd->callback([argc, argv]() { std::exit(cmd_config(argc - 1, argv + 1)); });

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }
    
    return 0;
}
