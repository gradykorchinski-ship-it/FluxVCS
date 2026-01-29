#pragma once

#include "flux/core/types.hpp"
#include "flux/core/object_id.hpp"
#include <vector>
#include <functional>

namespace flux {

/**
 * Network protocol packet types
 */
enum class PacketType : uint8_t {
    CAPABILITY_ADVERTISEMENT = 1,
    WANT_OBJECT = 2,
    HAVE_OBJECT = 3,
    OBJECT_DATA = 4,
    REF_UPDATE = 5,
    DONE = 6,
    ERROR = 7
};

/**
 * Protocol capabilities
 */
struct Capabilities {
    bool supports_delta_compression = false;
    bool supports_thin_pack = false;
    bool supports_ofs_delta = false;
    std::vector<std::string> hash_algorithms;
};

/**
 * Network connection interface
 */
class Connection {
public:
    virtual ~Connection() = default;
    
    virtual Result<void> send_packet(PacketType type, std::span<const uint8_t> data) = 0;
    virtual Result<std::pair<PacketType, Bytes>> receive_packet() = 0;
    virtual Result<void> close() = 0;
};

/**
 * Protocol handler
 */
class ProtocolHandler {
public:
    Result<Capabilities> negotiate_capabilities(Connection& conn);
    
    Result<std::vector<ObjectId>> compute_delta(
        const std::vector<ObjectId>& have,
        const std::vector<ObjectId>& want
    );
    
    Result<void> send_objects(
        Connection& conn,
        const std::vector<ObjectId>& objects
    );
    
    Result<std::vector<ObjectId>> receive_objects(Connection& conn);
};

/**
 * Remote repository
 */
class Remote {
public:
    Remote(std::string name, std::string url);
    
    const std::string& name() const { return name_; }
    const std::string& url() const { return url_; }
    
    Result<std::map<std::string, ObjectId>> list_refs();
    Result<void> fetch(const std::vector<std::string>& refs);
    Result<void> push(const std::string& local_ref, const std::string& remote_ref);
    
private:
    std::string name_;
    std::string url_;
};

} // namespace flux
