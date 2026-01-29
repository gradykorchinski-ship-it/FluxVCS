#include "flux/net/http_transport.hpp"
#include <curl/curl.h>
#include <sstream>
#include <regex>

namespace flux {

// libcurl implementation details
struct HttpTransport::Impl {
    CURL* curl = nullptr;
    std::string username;
    std::string password;
    std::string token;
    std::string last_github_token_expiry;
    ProgressCallback progress_callback;
    
    Impl() {
        curl = curl_easy_init();
    }
    
    ~Impl() {
        if (curl) {
            curl_easy_cleanup(curl);
        }
    }
    
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t total_size = size * nmemb;
        std::string* str = static_cast<std::string*>(userp);
        str->append(static_cast<char*>(contents), total_size);
        return total_size;
    }
    
    static size_t header_callback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t total_size = size * nmemb;
        std::string header(static_cast<char*>(contents), total_size);
        Impl* impl = static_cast<Impl*>(userp);
        
        // Look for GitHub token expiration header
        const std::string target = "github-authentication-token-expiration:";
        if (header.size() > target.size()) {
            std::string header_lower = header;
            std::transform(header_lower.begin(), header_lower.end(), header_lower.begin(), ::tolower);
            if (header_lower.starts_with(target)) {
                std::string val = header.substr(target.size());
                // Trim whitespace and newline
                val.erase(0, val.find_first_not_of(" \t\r\n"));
                val.erase(val.find_last_not_of(" \t\r\n") + 1);
                impl->last_github_token_expiry = val;
            }
        }
        
        return total_size;
    }

    static int progress_callback_wrapper(
        void* clientp,
        curl_off_t dltotal,
        curl_off_t dlnow,
        curl_off_t /*ultotal*/,
        curl_off_t /*ulnow*/
    ) {
        auto* callback = static_cast<ProgressCallback*>(clientp);
        if (callback && *callback) {
            (*callback)(dlnow, dltotal);
        }
        return 0;
    }
};

HttpTransport::HttpTransport()
    : impl_(std::make_unique<Impl>()) {}

HttpTransport::~HttpTransport() = default;

void HttpTransport::set_credentials(const std::string& username, const std::string& password) {
    impl_->username = username;
    impl_->password = password;
}

void HttpTransport::set_token(const std::string& token) {
    impl_->token = token;
}

void HttpTransport::set_progress_callback(ProgressCallback callback) {
    impl_->progress_callback = std::move(callback);
}

std::string HttpTransport::get_last_github_token_expiry() const {
    return impl_->last_github_token_expiry;
}

Result<std::string> HttpTransport::get(const std::string& url) {
    if (!impl_->curl) {
        return flux::unexpected("CURL not initialized");
    }
    
    std::string response;
    
    curl_easy_reset(impl_->curl);
    curl_easy_setopt(impl_->curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(impl_->curl, CURLOPT_WRITEFUNCTION, Impl::write_callback);
    curl_easy_setopt(impl_->curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(impl_->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(impl_->curl, CURLOPT_USERAGENT, "FluxVCS/1.0");
    curl_easy_setopt(impl_->curl, CURLOPT_HEADERFUNCTION, Impl::header_callback);
    curl_easy_setopt(impl_->curl, CURLOPT_HEADERDATA, impl_.get());
    
    // Authentication
    if (!impl_->token.empty()) {
        curl_easy_setopt(impl_->curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        curl_easy_setopt(impl_->curl, CURLOPT_USERNAME, "git"); // GitHub uses PAT as password
        curl_easy_setopt(impl_->curl, CURLOPT_PASSWORD, impl_->token.c_str());
    } else if (!impl_->username.empty()) {
        curl_easy_setopt(impl_->curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        curl_easy_setopt(impl_->curl, CURLOPT_USERNAME, impl_->username.c_str());
        curl_easy_setopt(impl_->curl, CURLOPT_PASSWORD, impl_->password.c_str());
    }
    
    // Progress callback
    if (impl_->progress_callback) {
        curl_easy_setopt(impl_->curl, CURLOPT_XFERINFOFUNCTION, Impl::progress_callback_wrapper);
        curl_easy_setopt(impl_->curl, CURLOPT_XFERINFODATA, &impl_->progress_callback);
        curl_easy_setopt(impl_->curl, CURLOPT_NOPROGRESS, 0L);
    }
    
    CURLcode res = curl_easy_perform(impl_->curl);
    
    if (res != CURLE_OK) {
        return flux::unexpected(std::string("HTTP GET failed: ") + curl_easy_strerror(res));
    }
    
    long http_code = 0;
    curl_easy_getinfo(impl_->curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    if (http_code >= 400) {
        return flux::unexpected("HTTP error " + std::to_string(http_code));
    }
    
    return response;
}

Result<std::string> HttpTransport::post(
    const std::string& url,
    const std::string& content_type,
    const std::string& data
) {
    if (!impl_->curl) {
        return flux::unexpected("CURL not initialized");
    }
    
    std::string response;
    
    curl_easy_reset(impl_->curl);
    curl_easy_setopt(impl_->curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(impl_->curl, CURLOPT_POST, 1L);
    curl_easy_setopt(impl_->curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(impl_->curl, CURLOPT_POSTFIELDSIZE, data.size());
    curl_easy_setopt(impl_->curl, CURLOPT_WRITEFUNCTION, Impl::write_callback);
    curl_easy_setopt(impl_->curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(impl_->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(impl_->curl, CURLOPT_USERAGENT, "FluxVCS/1.0");
    curl_easy_setopt(impl_->curl, CURLOPT_HEADERFUNCTION, Impl::header_callback);
    curl_easy_setopt(impl_->curl, CURLOPT_HEADERDATA, impl_.get());
    
    // Set content type
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Content-Type: " + content_type).c_str());
    
    // Authentication
    if (!impl_->token.empty()) {
        curl_easy_setopt(impl_->curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        curl_easy_setopt(impl_->curl, CURLOPT_USERNAME, "git");
        curl_easy_setopt(impl_->curl, CURLOPT_PASSWORD, impl_->token.c_str());
    } else if (!impl_->username.empty()) {
        curl_easy_setopt(impl_->curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        curl_easy_setopt(impl_->curl, CURLOPT_USERNAME, impl_->username.c_str());
        curl_easy_setopt(impl_->curl, CURLOPT_PASSWORD, impl_->password.c_str());
    }
    
    curl_easy_setopt(impl_->curl, CURLOPT_HTTPHEADER, headers);
    
    // Progress callback
    if (impl_->progress_callback) {
        curl_easy_setopt(impl_->curl, CURLOPT_XFERINFOFUNCTION, Impl::progress_callback_wrapper);
        curl_easy_setopt(impl_->curl, CURLOPT_XFERINFODATA, &impl_->progress_callback);
        curl_easy_setopt(impl_->curl, CURLOPT_NOPROGRESS, 0L);
    }
    
    CURLcode res = curl_easy_perform(impl_->curl);
    
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        return flux::unexpected(std::string("HTTP POST failed: ") + curl_easy_strerror(res));
    }
    
    long http_code = 0;
    curl_easy_getinfo(impl_->curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    if (http_code >= 400) {
        return flux::unexpected("HTTP error " + std::to_string(http_code));
    }
    
    return response;
}

Result<std::string> HttpTransport::git_upload_pack_discovery(const std::string& repo_url) {
    auto url_result = GitUrl::parse(repo_url);
    if (!url_result) {
        return flux::unexpected(url_result.error());
    }
    
    if (!url_result->username.empty() && impl_->username.empty() && impl_->token.empty()) {
        set_credentials(url_result->username, url_result->password);
    }
    
    std::string url = url_result->get_info_refs_url("git-upload-pack");
    return get(url);
}

Result<std::string> HttpTransport::git_upload_pack(const std::string& repo_url, const std::string& request) {
    auto url_result = GitUrl::parse(repo_url);
    if (!url_result) {
        return flux::unexpected(url_result.error());
    }
    
    if (!url_result->username.empty() && impl_->username.empty() && impl_->token.empty()) {
        set_credentials(url_result->username, url_result->password);
    }
    
    std::string url = url_result->get_service_url("git-upload-pack");
    return post(url, "application/x-git-upload-pack-request", request);
}

Result<std::string> HttpTransport::git_receive_pack_discovery(const std::string& repo_url) {
    auto url_result = GitUrl::parse(repo_url);
    if (!url_result) {
        return flux::unexpected(url_result.error());
    }
    
    if (!url_result->username.empty() && impl_->username.empty() && impl_->token.empty()) {
        set_credentials(url_result->username, url_result->password);
    }
    
    std::string url = url_result->get_info_refs_url("git-receive-pack");
    return get(url);
}

Result<std::string> HttpTransport::git_receive_pack(const std::string& repo_url, const std::string& request) {
    auto url_result = GitUrl::parse(repo_url);
    if (!url_result) {
        return flux::unexpected(url_result.error());
    }
    
    if (!url_result->username.empty() && impl_->username.empty() && impl_->token.empty()) {
        set_credentials(url_result->username, url_result->password);
    }
    
    std::string url = url_result->get_service_url("git-receive-pack");
    return post(url, "application/x-git-receive-pack-request", request);
}

// GitUrl implementation

Result<GitUrl> GitUrl::parse(const std::string& url) {
    GitUrl result;
    
    // Regex for URL parsing
    std::regex url_regex(
        R"(^(https?|ssh|git|file)://(?:([^:@]+)(?::([^@]+))?@)?([^:/]+)(?::(\d+))?(/.*)?$)"
    );
    
    std::smatch match;
    if (std::regex_match(url, match, url_regex)) {
        std::string protocol_str = match[1];
        if (protocol_str == "http") result.protocol = Protocol::HTTP;
        else if (protocol_str == "https") result.protocol = Protocol::HTTPS;
        else if (protocol_str == "ssh") result.protocol = Protocol::SSH;
        else if (protocol_str == "git") result.protocol = Protocol::GIT;
        else if (protocol_str == "file") result.protocol = Protocol::FILE;
        
        result.username = match[2];
        result.password = match[3];
        result.host = match[4];
        
        if (match[5].matched) {
            result.port = std::stoi(match[5]);
        } else {
            // Default ports
            if (result.protocol == Protocol::HTTP) result.port = 80;
            else if (result.protocol == Protocol::HTTPS) result.port = 443;
            else if (result.protocol == Protocol::SSH) result.port = 22;
            else if (result.protocol == Protocol::GIT) result.port = 9418;
        }
        
        result.path = match[6].matched ? std::string(match[6]) : "/";
        
        return result;
    }
    
    // Try SSH shorthand: user@host:path
    std::regex ssh_regex(R"(^(?:([^@]+)@)?([^:]+):(.+)$)");
    if (std::regex_match(url, match, ssh_regex)) {
        result.protocol = Protocol::SSH;
        result.username = match[1].matched ? std::string(match[1]) : "git";
        result.host = match[2];
        result.port = 22;
        result.path = "/" + std::string(match[3]);
        return result;
    }
    
    return flux::unexpected("Invalid Git URL format");
}

std::string GitUrl::to_string() const {
    std::ostringstream oss;
    
    switch (protocol) {
        case Protocol::HTTP: oss << "http://"; break;
        case Protocol::HTTPS: oss << "https://"; break;
        case Protocol::SSH: oss << "ssh://"; break;
        case Protocol::GIT: oss << "git://"; break;
        case Protocol::FILE: oss << "file://"; break;
    }
    
    if (!username.empty()) {
        oss << username;
        if (!password.empty()) {
            oss << ":" << password;
        }
        oss << "@";
    }
    
    oss << host;
    
    // Add port if non-default
    bool add_port = false;
    if (protocol == Protocol::HTTP && port != 80) add_port = true;
    if (protocol == Protocol::HTTPS && port != 443) add_port = true;
    if (protocol == Protocol::SSH && port != 22) add_port = true;
    if (protocol == Protocol::GIT && port != 9418) add_port = true;
    
    if (add_port) {
        oss << ":" << port;
    }
    
    oss << path;
    
    return oss.str();
}

std::string GitUrl::get_info_refs_url(const std::string& service) const {
    std::ostringstream oss;
    
    switch (protocol) {
        case Protocol::HTTP: oss << "http://"; break;
        case Protocol::HTTPS: oss << "https://"; break;
        default:
            return "";  // Only HTTP/HTTPS supported for smart protocol
    }
    
    oss << host;
    
    if ((protocol == Protocol::HTTP && port != 80) ||
        (protocol == Protocol::HTTPS && port != 443)) {
        oss << ":" << port;
    }
    
    oss << path;
    if (!path.empty() && path.back() != '/') {
        oss << "/";
    }
    oss << "info/refs?service=" << service;
    
    return oss.str();
}

std::string GitUrl::get_service_url(const std::string& service) const {
    std::ostringstream oss;
    
    switch (protocol) {
        case Protocol::HTTP: oss << "http://"; break;
        case Protocol::HTTPS: oss << "https://"; break;
        default:
            return "";
    }
    
    oss << host;
    
    if ((protocol == Protocol::HTTP && port != 80) ||
        (protocol == Protocol::HTTPS && port != 443)) {
        oss << ":" << port;
    }
    
    oss << path;
    if (!path.empty() && path.back() != '/') {
        oss << "/";
    }
    oss << service;
    
    return oss.str();
}

} // namespace flux
