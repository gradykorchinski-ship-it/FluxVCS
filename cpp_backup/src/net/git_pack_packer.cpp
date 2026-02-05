#include "flux/net/git_pack.hpp"
#include "flux/core/repository.hpp"
#include "flux/core/blob.hpp"
#include "flux/core/tree.hpp"
#include "flux/core/commit.hpp"
#include "flux/storage/object_store.hpp"
#include "flux/util/zlib_util.hpp"
#include "flux/util/hash.hpp"
#include <fmt/format.h>
#include <arpa/inet.h>
#include <iostream>

namespace flux {

GitPackPacker::GitPackPacker(Repository& repo) : repo_(repo) {}

Result<Bytes> GitPackPacker::create_pack(const std::vector<ObjectId>& object_ids) {
    Bytes pack_data;
    
    // 1. Pack Header: 'PACK', version 2, num_objects
    pack_data.push_back('P');
    pack_data.push_back('A');
    pack_data.push_back('C');
    pack_data.push_back('K');
    
    uint32_t version = htonl(2);
    uint8_t* version_ptr = reinterpret_cast<uint8_t*>(&version);
    pack_data.insert(pack_data.end(), version_ptr, version_ptr + 4);
    
    uint32_t num_objs = htonl(static_cast<uint32_t>(object_ids.size()));
    uint8_t* num_ptr = reinterpret_cast<uint8_t*>(&num_objs);
    pack_data.insert(pack_data.end(), num_ptr, num_ptr + 4);
    
    // 2. Objects
    for (const auto& id : object_ids) {
        auto type_result = repo_.objects().get_type(id);
        if (!type_result) return flux::unexpected(type_result.error());
        
        auto raw_data_result = repo_.objects().read(id);
        if (!raw_data_result) return flux::unexpected(raw_data_result.error());
        
        Bytes git_binary;
        uint8_t git_type = 0;
        
        if (repo_.hash_algorithm() == HashAlgorithm::SHA1) {
            // For SHA1 (Git compatibility), objects are stored in Git format (or raw content for blobs).
            // We can just pass them through to avoid serialization round-trip issues (hash mismatch).
            git_binary = *raw_data_result;
            switch (*type_result) {
                case ObjectType::Commit: git_type = 1; break;
                case ObjectType::Tree: git_type = 2; break;
                case ObjectType::Blob: git_type = 3; break;
                case ObjectType::Tag: git_type = 4; break;
            }
        } else {
            // FluxVCS custom format -> Git format conversion
            switch (*type_result) {
                case ObjectType::Commit: {
                    git_type = 1;
                    auto commit = Commit::deserialize(*raw_data_result);
                    if (!commit) return flux::unexpected(commit.error());
                    git_binary = commit->serialize_git();
                    break;
                }
                case ObjectType::Tree: {
                    git_type = 2;
                    auto tree = Tree::deserialize(*raw_data_result);
                    if (!tree) return flux::unexpected(tree.error());
                    git_binary = tree->serialize_git();
                    break;
                }
                case ObjectType::Blob: {
                    git_type = 3;
                    auto blob_res = Blob::deserialize(*raw_data_result);
                    if (!blob_res) return flux::unexpected(blob_res.error());
                    
                    // Reassemble chunks
                    for (const auto& chunk : blob_res->chunks()) {
                        if (!chunk.data.empty()) {
                            git_binary.insert(git_binary.end(), chunk.data.begin(), chunk.data.end());
                        } else {
                            auto chunk_data = repo_.objects().read(chunk.id);
                            if (!chunk_data) return flux::unexpected(chunk_data.error());
                            git_binary.insert(git_binary.end(), chunk_data->begin(), chunk_data->end());
                        }
                    }
                    break;
                }
                case ObjectType::Tag:
                    git_type = 4;
                    git_binary = *raw_data_result;
                    break;
            }
        }
        
        // Encode object header
        auto header = encode_object_header(git_type, git_binary.size());
        pack_data.insert(pack_data.end(), header.begin(), header.end());
        
        // Compress data
        auto compressed = ZlibUtil::compress(git_binary);
        if (!compressed) return flux::unexpected(compressed.error());
        pack_data.insert(pack_data.end(), compressed->begin(), compressed->end());
    }
    
    // 3. Footer: SHA-1 checksum of everything before
    Bytes checksum = Hash::compute_sha1(pack_data);
    pack_data.insert(pack_data.end(), checksum.begin(), checksum.end());
    
    return pack_data;
}

std::vector<uint8_t> GitPackPacker::encode_object_header(uint8_t type, uint64_t size) {
    std::vector<uint8_t> header;
    
    // First byte: MSB (more?), 3-bit type, 4-bit size LSB
    uint8_t byte = (type << 4) | (size & 0x0F);
    size >>= 4;
    
    if (size > 0) {
        byte |= 0x80;
    }
    header.push_back(byte);
    
    // Subsequent bytes: MSB, 7-bit size
    while (size > 0) {
        byte = size & 0x7F;
        size >>= 7;
        if (size > 0) {
            byte |= 0x80;
        }
        header.push_back(byte);
    }
    
    return header;
}

} // namespace flux
