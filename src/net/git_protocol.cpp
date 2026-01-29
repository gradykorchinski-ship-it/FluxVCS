#include "flux/net/git_protocol.hpp"
#include <fmt/format.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "flux/util/hash.hpp"

namespace flux {

// PktLine implementation

Result<std::pair<std::string, size_t>> PktLine::parse(std::string_view buffer) {
    if (buffer.size() < 4) {
        return flux::unexpected("Buffer too small for pkt-line");
    }
    
    // Parse 4-byte hex length
    std::string len_str(buffer.substr(0, 4));
    size_t length;
    try {
        length = std::stoul(len_str, nullptr, 16);
    } catch (...) {
        return flux::unexpected("Invalid pkt-line length");
    }
    
    // Special packets
    if (length == 0) {
        return std::make_pair(std::string(), size_t(4));  // Flush packet
    }
    if (length == 1) {
        return std::make_pair(std::string("\x01"), size_t(4));  // Delim packet
    }
    
    if (length < 4 || length > MAX_LENGTH) {
        return flux::unexpected("Invalid pkt-line length value");
    }
    
    if (buffer.size() < length) {
        return flux::unexpected("Incomplete pkt-line");
    }
    
    // Extract data (excluding 4-byte length prefix)
    std::string data(buffer.substr(4, length - 4));
    
    return std::make_pair(data, length);
}

std::string PktLine::encode(std::string_view data) {
    if (data.empty()) {
        return flush();
    }
    
    size_t length = data.size() + 4;
    if (length > MAX_LENGTH) {
        length = MAX_LENGTH;
        data = data.substr(0, MAX_LENGTH - 4);
    }
    
    std::ostringstream oss;
    oss << std::hex << std::setw(4) << std::setfill('0') << length;
    oss << data;
    
    return oss.str();
}

std::string PktLine::flush() {
    return "0000";
}

std::string PktLine::delim() {
    return "0001";
}

bool PktLine::is_flush(std::string_view pkt) {
    return pkt.size() >= 4 && pkt.substr(0, 4) == "0000";
}

bool PktLine::is_delim(std::string_view pkt) {
    return pkt.size() >= 4 && pkt.substr(0, 4) == "0001";
}

// GitCapabilities implementation

GitCapabilities GitCapabilities::parse(const std::vector<std::string>& caps) {
    GitCapabilities result;
    
    for (const auto& cap : caps) {
        if (cap == "multi_ack") result.multi_ack = true;
        else if (cap == "multi_ack_detailed") result.multi_ack_detailed = true;
        else if (cap == "side-band") result.side_band = true;
        else if (cap == "side-band-64k") result.side_band_64k = true;
        else if (cap == "ofs-delta") result.ofs_delta = true;
        else if (cap == "thin-pack") result.thin_pack = true;
        else if (cap == "no-progress") result.no_progress = true;
        else if (cap == "include-tag") result.include_tag = true;
        else if (cap == "report-status") result.report_status = true;
        else if (cap == "delete-refs") result.delete_refs = true;
        else if (cap == "quiet") result.quiet = true;
        else if (cap == "atomic") result.atomic = true;
        else if (cap == "push-options") result.push_options = true;
        else if (cap.starts_with("agent=")) {
            result.agent = cap.substr(6);
        }
    }
    
    return result;
}

std::vector<std::string> GitCapabilities::encode() const {
    std::vector<std::string> result;
    
    if (multi_ack) result.push_back("multi_ack");
    if (multi_ack_detailed) result.push_back("multi_ack_detailed");
    if (side_band) result.push_back("side-band");
    if (side_band_64k) result.push_back("side-band-64k");
    if (ofs_delta) result.push_back("ofs-delta");
    if (thin_pack) result.push_back("thin-pack");
    if (no_progress) result.push_back("no-progress");
    if (include_tag) result.push_back("include-tag");
    if (report_status) result.push_back("report-status");
    if (delete_refs) result.push_back("delete-refs");
    if (quiet) result.push_back("quiet");
    if (atomic) result.push_back("atomic");
    if (push_options) result.push_back("push-options");
    
    if (!agent.empty()) {
        result.push_back("agent=" + agent);
    }
    
    return result;
}

// GitProtocol implementation

Result<GitProtocol::RefDiscovery> GitProtocol::parse_ref_discovery(std::string_view data) {
    RefDiscovery discovery;
    size_t pos = 0;
    bool first_ref = true;
    int line_count = 0;
    int ref_count = 0;
    
    fmt::print("[DEBUG] Starting ref discovery parse, data size: {}\n", data.size());
    
    while (pos < data.size()) {
        auto parse_result = PktLine::parse(data.substr(pos));
        if (!parse_result) {
            fmt::print("[DEBUG] PktLine parse failed at pos {}: {}\n", pos, parse_result.error());
            break;
        }
        
        auto [line, consumed] = *parse_result;
        pos += consumed;
        line_count++;
        
        if (line.empty()) {
            fmt::print("[DEBUG] Line {}: flush packet\n", line_count);
            continue;  // Flush packet
        }
        
        if (line_count <= 3) {
            fmt::print("[DEBUG] Line {}: [{}] (len={})\n", line_count, 
                      line.size() > 80 ? line.substr(0, 80) + "..." : line, line.size());
        }
        
        // Service line
        if (line.starts_with("# service=")) {
            discovery.service = line.substr(10);
            fmt::print("[DEBUG] Found service: {}\n", discovery.service);
            // Next should be flush
            auto flush_result = PktLine::parse(data.substr(pos));
            if (flush_result) {
                pos += flush_result->second;
            }
            continue;
        }
        
        // Parse ref line: <oid> <name>\0<capabilities>
        size_t space_pos = line.find(' ');
        if (space_pos == std::string::npos || space_pos != 40) {
            if (line_count <= 5) {
                fmt::print("[DEBUG] Skipping line (space_pos={}): {}\n", 
                          space_pos, line.substr(0, std::min(line.size(), size_t(60))));
            }
            continue;  // Invalid ref line
        }
        
        std::string oid_str = line.substr(0, 40);
        std::string rest = line.substr(41);  // Everything after "<oid> "
        
        if (line_count <= 3) {
            fmt::print("[DEBUG] OID: {}, rest length: {}, first 20 chars of rest: [{}]\n", 
                      oid_str.substr(0, 8), rest.size(),
                      rest.substr(0, std::min(rest.size(), size_t(20))));
            // Hex dump first few bytes
            fmt::print("[DEBUG] Rest hex: ");
            for (size_t i = 0; i < std::min(rest.size(), size_t(30)); i++) {
                fmt::print("{:02x} ", static_cast<unsigned char>(rest[i]));
            }
            fmt::print("\n");
        }
        
        // Find null terminator for capabilities
        size_t null_pos = rest.find('\0');
        std::string ref_name;
        
        if (null_pos != std::string::npos) {
            ref_name = rest.substr(0, null_pos);
            if (first_ref) {
                // Parse capabilities
                std::string caps_str = rest.substr(null_pos + 1);
                fmt::print("[DEBUG] First ref name: [{}], capabilities: {}\n", 
                          ref_name, caps_str.substr(0, std::min(caps_str.size(), size_t(100))));
                std::vector<std::string> caps;
                size_t start = 0;
                while (start < caps_str.size()) {
                    size_t end = caps_str.find(' ', start);
                    if (end == std::string::npos) {
                        if (start < caps_str.size()) {
                            caps.push_back(caps_str.substr(start));
                        }
                        break;
                    }
                    caps.push_back(caps_str.substr(start, end - start));
                    start = end + 1;
                }
                discovery.capabilities = GitCapabilities::parse(caps);
                first_ref = false;
            }
        } else {
            ref_name = rest;
        }
        
        // Strip trailing newline from ref_name
        while (!ref_name.empty() && (ref_name.back() == '\n' || ref_name.back() == '\r')) {
            ref_name.pop_back();
        }
        
        // Parse OID - Git uses SHA-1, prepend algorithm prefix
        if (line_count <= 3) {
            fmt::print("[DEBUG] Attempting to parse OID: [{}] (len={})\n", oid_str, oid_str.size());
        }
        std::string full_oid = "sha1:" + oid_str;
        auto oid_result = ObjectId::from_hex(full_oid);
        if (!oid_result) {
            if (line_count <= 5) {
                fmt::print("[DEBUG] Failed to parse OID: {} - error: {}\n", full_oid, oid_result.error());
            }
            continue;
        }
        
        ref_count++;
        if (ref_count <= 5) {
            fmt::print("[DEBUG] Found ref: {} -> {}\n", ref_name, oid_str.substr(0, 8));
        }
        
        discovery.refs.push_back(GitRef{ref_name, *oid_result, false, {}});
    }
    
    fmt::print("[DEBUG] Parse complete: {} lines, {} refs\n", line_count, ref_count);
    
    return discovery;
}

std::string GitProtocol::encode_upload_pack_request(const UploadPackRequest& req) {
    std::ostringstream oss;
    
    // Encode wants
    bool first = true;
    for (const auto& want : req.wants) {
        // Git wire protocol expects raw 40-char hex, NOT FluxVCS 'algo:hash' format
        std::string line = "want " + Hash::to_hex(want.hash());
        
        if (first) {
            // Add capabilities to first want
            auto caps = req.capabilities.encode();
            if (!caps.empty()) {
                line += " ";
                for (size_t i = 0; i < caps.size(); ++i) {
                    if (i > 0) line += " ";
                    line += caps[i];
                }
            }
            first = false;
        }
        
        line += "\n";
        oss << PktLine::encode(line);
    }
    
    oss << PktLine::flush();
    
    // Encode haves
    for (const auto& have : req.haves) {
        std::string line = "have " + Hash::to_hex(have.hash()) + "\n";
        oss << PktLine::encode(line);
    }
    
    if (req.done) {
        oss << PktLine::encode("done\n");
    }
    
    oss << PktLine::flush();
    
    return oss.str();
}

std::string GitProtocol::encode_receive_pack_request(const ReceivePackRequest& req) {
    std::ostringstream oss;
    
    // Encode commands
    bool first = true;
    for (const auto& cmd : req.commands) {
        // Git expects raw hex (without algo: prefix)
        std::string old_hex = Hash::to_hex(cmd.old_oid.hash());
        std::string new_hex = Hash::to_hex(cmd.new_oid.hash());
        
        std::string line = old_hex + " " + new_hex + " " + cmd.ref_name;
        
        if (first) {
            // Add capabilities to first command after a null byte
            auto caps = req.capabilities.encode();
            if (!caps.empty()) {
                line += '\0';
                for (size_t i = 0; i < caps.size(); ++i) {
                    if (i > 0) line += " ";
                    line += caps[i];
                }
            }
            first = false;
        }
        
        oss << PktLine::encode(line + '\n');
    }
    
    oss << PktLine::flush();
    
    // Append pack data
    oss.write(reinterpret_cast<const char*>(req.pack_data.data()), req.pack_data.size());
    
    return oss.str();
}

Result<GitProtocol::ReceivePackResponse> GitProtocol::parse_receive_pack_response(std::string_view data) {
    ReceivePackResponse response;
    
    size_t pos = 0;
    
    while (pos < data.size()) {
        auto parse_result = PktLine::parse(data.substr(pos));
        if (!parse_result) {
            break;
        }
        
        std::string line = parse_result->first;
        pos += parse_result->second;
        
        if (line.empty()) {
            break;
        }
        
        // Handle side-band (channel 1: data, 2: progress, 3: fatal error)
        if (!line.empty() && (line[0] == '\x01' || line[0] == '\x02' || line[0] == '\x03')) {
            char channel = line[0];
            std::string payload = line.substr(1);
            
            if (channel == '\x02' || channel == '\x03') {
                fmt::print(stderr, "Remote: {}", payload);
                continue;
            }
            
            // If it's channel 1, it might contain more pkt-lines (like report-status)
            // Recursively parse the payload if it looks like a pkt-line
            if (payload.size() >= 4 && std::isxdigit(payload[0]) && std::isxdigit(payload[1])) {
                auto inner_res = parse_receive_pack_response(payload);
                if (inner_res) {
                    if (response.unpack_status.empty()) response.unpack_status = inner_res->unpack_status;
                    response.updates.insert(response.updates.end(), inner_res->updates.begin(), inner_res->updates.end());
                }
                continue;
            }
            line = payload;
        }
        
        // Parse unpack status
        if (line.starts_with("unpack ")) {
            response.unpack_status = line.substr(7);
            if (!response.unpack_status.empty() && response.unpack_status.back() == '\n') {
                response.unpack_status.pop_back();
            }
            continue;
        }
        
        // Parse ref updates: ok <ref> or ng <ref> <reason>
        if (line.starts_with("ok ") || line.starts_with("ng ")) {
            ReceivePackResponse::RefUpdate update;
            update.success = line.starts_with("ok ");
            
            std::string rest = line.substr(3);
            if (rest.back() == '\n') rest.pop_back();
            
            auto space_pos = rest.find(' ');
            
            if (space_pos != std::string::npos) {
                update.ref_name = rest.substr(0, space_pos);
                update.message = rest.substr(space_pos + 1);
            } else {
                update.ref_name = rest;
            }
            response.updates.push_back(update);
        }
    }
    
    return response;
}

// SideBand implementation

Result<SideBand::Message> SideBand::parse(std::string_view pkt) {
    if (pkt.empty()) {
        return flux::unexpected("Empty side-band packet");
    }
    
    uint8_t channel_byte = static_cast<uint8_t>(pkt[0]);
    Channel channel;
    
    switch (channel_byte) {
        case 1: channel = Channel::PACK_DATA; break;
        case 2: channel = Channel::PROGRESS; break;
        case 3: channel = Channel::ERROR; break;
        default:
            return flux::unexpected("Invalid side-band channel");
    }
    
    Message msg;
    msg.channel = channel;
    msg.data = Bytes(pkt.begin() + 1, pkt.end());
    
    return msg;
}

std::string SideBand::encode(Channel channel, std::string_view data) {
    std::string result;
    result += static_cast<char>(channel);
    result += data;
    return result;
}

} // namespace flux
