#pragma once

#include "flux/core/types.hpp"
#include <string>
#include <chrono>

namespace flux {

struct Credential {
    std::string token;
    std::chrono::system_clock::time_point expires_at;
};

class AuthManager {
public:
    static AuthManager& instance();

    // Save token with expiration (days from now)
    Result<void> save_token(const std::string& token, int expires_in_days = 30);
    
    // Save token with specific expiration date
    Result<void> save_token_with_date(const std::string& token, std::chrono::system_clock::time_point expires_at);

    // Update expiration from GitHub header (YYYY-MM-DD HH:MM:SS UTC)
    Result<void> update_expiration_from_header(const std::string& header_val);

    // Get the stored token if it exists and is not expired
    Result<std::string> get_token();

    // Check if token is expired
    bool is_expired() const;
    
    // Get expiration date as string
    std::string get_expiration_string() const;

    // Clear saved token
    Result<void> clear();

private:
    AuthManager();
    Path get_credentials_path() const;
    
    std::optional<Credential> current_cred_;
    Result<void> load();
    Result<void> save();
};

} // namespace flux
