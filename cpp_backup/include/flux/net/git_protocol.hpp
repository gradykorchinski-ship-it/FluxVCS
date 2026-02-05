#pragma once

#include "flux/core/types.hpp"
#include "flux/core/object_id.hpp"
#include <string>
#include <vector>
#include <map>

namespace flux {

/**
 * Git pkt-line protocol
 * Format: 4-byte hex length + data
 */
class PktLine {
public:
    static constexpr size_t MAX_LENGTH = 65520;
    
    // Parse pkt-line from buffer
    static Result<std::pair<std::string, size_t>> parse(std::string_view buffer);
    
    // Encode data as pkt-line
    static std::string encode(std::string_view data);
    
    // Special packets
    static std::string flush();  // 0000
    static std::string delim();  // 0001
    
    // Check if packet is flush/delim
    static bool is_flush(std::string_view pkt);
    static bool is_delim(std::string_view pkt);
};

/**
 * Git reference
 */
struct GitRef {
    std::string name;
    ObjectId oid;
    bool is_peeled = false;
    ObjectId peeled_oid;
};

/**
 * Git capabilities
 */
struct GitCapabilities {
    bool multi_ack = false;
    bool multi_ack_detailed = false;
    bool side_band = false;
    bool side_band_64k = false;
    bool ofs_delta = false;
    bool thin_pack = false;
    bool no_progress = false;
    bool include_tag = false;
    bool report_status = false;
    bool delete_refs = false;
    bool quiet = false;
    bool atomic = false;
    bool push_options = false;
    
    std::string agent;
    
    // Parse from capability string
    static GitCapabilities parse(const std::vector<std::string>& caps);
    
    // Encode to capability strings
    std::vector<std::string> encode() const;
};

/**
 * Git protocol handler
 */
class GitProtocol {
public:
    // Discover references from remote
    struct RefDiscovery {
        std::vector<GitRef> refs;
        GitCapabilities capabilities;
        std::string service;
    };
    
    static Result<RefDiscovery> parse_ref_discovery(std::string_view data);
    
    // Create upload-pack request (for fetch)
    struct UploadPackRequest {
        std::vector<ObjectId> wants;
        std::vector<ObjectId> haves;
        GitCapabilities capabilities;
        bool done = false;
    };
    
    static std::string encode_upload_pack_request(const UploadPackRequest& req);
    
    // Create receive-pack request (for push)
    struct ReceivePackRequest {
        struct Command {
            ObjectId old_oid;
            ObjectId new_oid;
            std::string ref_name;
        };
        
        std::vector<Command> commands;
        GitCapabilities capabilities;
        Bytes pack_data;
    };
    
    static std::string encode_receive_pack_request(const ReceivePackRequest& req);
    
    // Parse receive-pack response
    struct ReceivePackResponse {
        struct RefUpdate {
            std::string ref_name;
            bool success;
            std::string message;
        };
        
        std::vector<RefUpdate> updates;
        std::string unpack_status;
    };
    
    static Result<ReceivePackResponse> parse_receive_pack_response(std::string_view data);
};

/**
 * Side-band protocol (for progress/errors during pack transfer)
 */
class SideBand {
public:
    enum class Channel {
        PACK_DATA = 1,
        PROGRESS = 2,
        ERROR = 3
    };
    
    struct Message {
        Channel channel;
        Bytes data;
    };
    
    static Result<Message> parse(std::string_view pkt);
    static std::string encode(Channel channel, std::string_view data);
};

} // namespace flux
