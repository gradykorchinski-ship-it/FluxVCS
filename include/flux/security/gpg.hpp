#pragma once

#include "flux/core/types.hpp"
#include "flux/core/commit.hpp"
#include <vector>
#include <memory>

namespace flux {

// Forward declarations
class Repository;

/**
 * GPG key information
 */
struct GPGKey {
    std::string key_id;
    std::string fingerprint;
    std::string user_id;
    bool can_sign;
    bool can_encrypt;
};

/**
 * GPG signer for commits
 */
class GPGSigner {
public:
    GPGSigner();
    ~GPGSigner();
    
    // Sign data with specified key
    Result<std::string> sign(std::string_view data, const std::string& key_id);
    
    // Verify signature
    Result<bool> verify(
        std::string_view data,
        std::string_view signature,
        std::string* signer_key = nullptr
    );
    
    // List available keys
    Result<std::vector<GPGKey>> list_keys();
    
    // Get specific key
    Result<GPGKey> get_key(const std::string& key_id);
    
    // Get default signing key
    Result<std::string> get_default_key();
    
private:
    bool initialized_ = false;
};

/**
 * Signed commit
 */
class SignedCommit {
public:
    SignedCommit(const Commit& commit, std::string signature);
    
    // Create signed commit
    static Result<SignedCommit> create(
        const Commit& commit,
        GPGSigner& signer,
        const std::string& key_id
    );
    
    // Verify signature
    Result<bool> verify(GPGSigner& signer, std::string* signer_key = nullptr) const;
    
    // Getters
    const Commit& commit() const { return commit_; }
    const std::string& signature() const { return signature_; }
    
    // Serialization
    Bytes serialize() const;
    static Result<SignedCommit> deserialize(std::span<const uint8_t> data);
    
private:
    Commit commit_;
    std::string signature_;
};

/**
 * Trust policy for signature verification
 */
class TrustPolicy {
public:
    enum class Level {
        UNKNOWN,
        UNTRUSTED,
        MARGINAL,
        FULL,
        ULTIMATE
    };
    
    virtual Level get_trust_level(const std::string& key_id) const = 0;
    virtual bool require_signature() const = 0;
    virtual ~TrustPolicy() = default;
};

/**
 * Security audit results
 */
struct SecurityAudit {
    struct Issue {
        enum class Severity {
            INFO,
            WARNING,
            ERROR,
            CRITICAL
        };
        
        Severity severity;
        std::string message;
        std::optional<ObjectId> commit_id;
    };
    
    std::vector<Issue> issues;
    size_t unsigned_commits;
    size_t invalid_signatures;
    size_t untrusted_signatures;
};

/**
 * Security auditor
 */
class SecurityAuditor {
public:
    explicit SecurityAuditor(Repository& repo);
    
    Result<SecurityAudit> audit();
    Result<SecurityAudit> audit_commit_range(
        const ObjectId& from,
        const ObjectId& to
    );
    
private:
    Repository& repo_;
};

} // namespace flux
