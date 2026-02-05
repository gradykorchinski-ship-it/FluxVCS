#include "flux/core/auth.hpp"
#include "flux/util/filesystem.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <fmt/chrono.h>

namespace flux {

AuthManager& AuthManager::instance() {
    static AuthManager instance;
    return instance;
}

AuthManager::AuthManager() {
    load();
}

Path AuthManager::get_credentials_path() const {
    const char* home = std::getenv("HOME");
    if (!home) return ".flux/credentials";
    return Path(home) / ".flux" / "credentials";
}

Result<void> AuthManager::load() {
    Path path = get_credentials_path();
    if (!Filesystem::exists(path)) return {};

    auto data_result = Filesystem::read_file(path);
    if (!data_result) return flux::unexpected(data_result.error());

    std::string content(reinterpret_cast<const char*>(data_result->data()), data_result->size());
    std::istringstream iss(content);
    std::string line;
    
    Credential cred;
    bool has_token = false;
    bool has_expiry = false;

    while (std::getline(iss, line)) {
        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);

        if (key == "token") {
            cred.token = value;
            has_token = true;
        } else if (key == "expires_at") {
            std::time_t t = std::stoll(value);
            cred.expires_at = std::chrono::system_clock::from_time_t(t);
            has_expiry = true;
        }
    }

    if (has_token && has_expiry) {
        current_cred_ = cred;
    }

    return {};
}

Result<void> AuthManager::save() {
    if (!current_cred_) {
        Path path = get_credentials_path();
        if (Filesystem::exists(path)) {
            std::filesystem::remove(path);
        }
        return {};
    }

    Path path = get_credentials_path();
    Path dir = path.parent_path();
    if (!Filesystem::exists(dir)) {
        Filesystem::create_directories(dir);
    }

    std::ostringstream oss;
    oss << "token=" << current_cred_->token << "\n";
    oss << "expires_at=" << std::chrono::system_clock::to_time_t(current_cred_->expires_at) << "\n";

    std::string content = oss.str();
    Bytes data(content.begin(), content.end());
    return Filesystem::write_file(path, data);
}

Result<void> AuthManager::save_token(const std::string& token, int expires_in_days) {
    auto now = std::chrono::system_clock::now();
    auto expiry = now + std::chrono::hours(24 * expires_in_days);
    return save_token_with_date(token, expiry);
}

Result<void> AuthManager::save_token_with_date(const std::string& token, std::chrono::system_clock::time_point expires_at) {
    current_cred_ = {token, expires_at};
    return save();
}

Result<void> AuthManager::update_expiration_from_header(const std::string& header_val) {
    if (!current_cred_) return flux::unexpected("No token to update");

    std::tm tm = {};
    std::istringstream ss(header_val);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    
    if (ss.fail()) return flux::unexpected("Failed to parse GitHub expiration header");

    // Convert UTC tm to time_point
    auto t = timegm(&tm);
    current_cred_->expires_at = std::chrono::system_clock::from_time_t(t);
    
    return save();
}

Result<std::string> AuthManager::get_token() {
    if (!current_cred_) return flux::unexpected("No token saved. Use 'flux auth set' to login.");
    
    if (is_expired()) {
        return flux::unexpected(fmt::format("Token expired on {}. Please login again.", get_expiration_string()));
    }

    return current_cred_->token;
}

bool AuthManager::is_expired() const {
    if (!current_cred_) return true;
    return std::chrono::system_clock::now() > current_cred_->expires_at;
}

std::string AuthManager::get_expiration_string() const {
    if (!current_cred_) return "None";
    auto t = std::chrono::system_clock::to_time_t(current_cred_->expires_at);
    return fmt::format("{:%Y-%m-%d %H:%M:%S}", fmt::localtime(t));
}

Result<void> AuthManager::clear() {
    current_cred_ = std::nullopt;
    return save();
}

} // namespace flux
