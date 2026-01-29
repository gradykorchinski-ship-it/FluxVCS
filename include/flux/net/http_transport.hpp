#pragma once

#include "flux/core/types.hpp"
#include <string>
#include <map>
#include <functional>

namespace flux {

/**
 * HTTP transport for Git protocol
 */
class HttpTransport {
public:
    HttpTransport();
    ~HttpTransport();
    
    // Progress callback: (current, total) -> void
    using ProgressCallback = std::function<void(size_t, size_t)>;
    
    // Set authentication
    void set_credentials(const std::string& username, const std::string& password);
    void set_token(const std::string& token);
    
    // Set progress callback
    void set_progress_callback(ProgressCallback callback);
    
    // Get last seen GitHub token expiration (if any)
    std::string get_last_github_token_expiry() const;
    
    // HTTP GET request
    Result<std::string> get(const std::string& url);
    
    // HTTP POST request
    Result<std::string> post(
        const std::string& url,
        const std::string& content_type,
        const std::string& data
    );
    
    // Git-specific requests
    Result<std::string> git_upload_pack_discovery(const std::string& repo_url);
    Result<std::string> git_upload_pack(const std::string& repo_url, const std::string& request);
    
    Result<std::string> git_receive_pack_discovery(const std::string& repo_url);
    Result<std::string> git_receive_pack(const std::string& repo_url, const std::string& request);
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * Parse Git repository URL
 */
struct GitUrl {
    enum class Protocol {
        HTTP,
        HTTPS,
        SSH,
        FILE,
        GIT
    };
    
    Protocol protocol;
    std::string host;
    uint16_t port = 0;
    std::string path;
    std::string username;
    std::string password;
    
    static Result<GitUrl> parse(const std::string& url);
    std::string to_string() const;
    
    // Get HTTP URL for smart protocol
    std::string get_info_refs_url(const std::string& service) const;
    std::string get_service_url(const std::string& service) const;
};

} // namespace flux
