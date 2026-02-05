#include "flux/net/git_pack.hpp"
#include "flux/core/repository.hpp"
#include "flux/storage/object_store.hpp"
#include "flux/util/zlib_util.hpp"
#include <fmt/format.h>
#include <arpa/inet.h>
#include <map>
#include <unordered_map>

namespace flux {

GitPackUnpacker::GitPackUnpacker(Repository& repo) : repo_(repo) {}

Result<size_t> GitPackUnpacker::unpack(std::span<const uint8_t> data) {
    if (data.size() < 12) return flux::unexpected("Pack file too short");
    if (std::string_view(reinterpret_cast<const char*>(data.data()), 4) != "PACK")
        return flux::unexpected("Invalid pack file magic");

    uint32_t version = ntohl(*reinterpret_cast<const uint32_t*>(data.data() + 4));
    uint32_t object_count = ntohl(*reinterpret_cast<const uint32_t*>(data.data() + 8));

    if (version != 2 && version != 3)
        return flux::unexpected(fmt::format("Unsupported pack version: {}", version));

    fmt::print("Unpacking {} objects from version {} pack\n", object_count, version);

    std::vector<RawObject> raw_objects;
    raw_objects.reserve(object_count);

    size_t pos = 12;
    for (uint32_t i = 0; i < object_count; ++i) {
        auto obj_result = read_object(data, pos);
        if (!obj_result) return flux::unexpected(obj_result.error());
        raw_objects.push_back(std::move(*obj_result));
    }

    return resolve_and_store(raw_objects);
}

Result<GitPackUnpacker::RawObject> GitPackUnpacker::read_object(std::span<const uint8_t> data, size_t& pos) {
    if (pos >= data.size()) return flux::unexpected("Unexpected end of pack data");
    size_t start_pos = pos;
    uint8_t byte = data[pos++];
    uint8_t type = (byte >> 4) & 0x07;
    size_t size = byte & 0x0F;
    size_t shift = 4;
    while (byte & 0x80) {
        byte = data[pos++];
        size |= (static_cast<size_t>(byte & 0x7F) << shift);
        shift += 7;
    }

    RawObject obj;
    obj.offset = start_pos;
    obj.type = type;

    if (type == 6) { // OFS_DELTA
        uint8_t b = data[pos++];
        uint64_t offset_val = b & 0x7F;
        while (b & 0x80) {
            offset_val += 1;
            b = data[pos++];
            offset_val = (offset_val << 7) | (b & 0x7F);
        }
        obj.base_offset = offset_val;
    } else if (type == 7) { // REF_DELTA
        Bytes hash(20);
        std::copy(data.data() + pos, data.data() + pos + 20, hash.begin());
        obj.base_id = ObjectId::from_hash(HashAlgorithm::SHA1, std::move(hash));
        pos += 20;
    }

    auto decompress_result = ZlibUtil::decompress_partial(data.subspan(pos), size);
    if (!decompress_result) return flux::unexpected(decompress_result.error());
    obj.data = std::move(decompress_result->decompressed);
    pos += decompress_result->consumed;
    return obj;
}

Result<size_t> GitPackUnpacker::resolve_and_store(std::vector<RawObject>& objects) {
    std::unordered_map<uint64_t, std::pair<ObjectType, Bytes>> cache;
    std::unordered_map<ObjectId, std::pair<ObjectType, Bytes>> id_cache;
    size_t resolved_count = 0;
    std::vector<bool> resolved(objects.size(), false);

    bool progress = true;
    while (progress && resolved_count < objects.size()) {
        progress = false;
        for (size_t i = 0; i < objects.size(); ++i) {
            if (resolved[i]) continue;
            auto& obj = objects[i];
            Bytes final_data;
            ObjectType final_type;
            bool can_resolve = false;

            if (obj.type >= 1 && obj.type <= 4) {
                final_data = std::move(obj.data);
                if (obj.type == 1) final_type = ObjectType::Commit;
                else if (obj.type == 2) final_type = ObjectType::Tree;
                else if (obj.type == 3) final_type = ObjectType::Blob;
                else final_type = ObjectType::Tag;
                can_resolve = true;
            } else if (obj.type == 6) {
                uint64_t base_offset = obj.offset - *obj.base_offset;
                auto it = cache.find(base_offset);
                if (it != cache.end()) {
                    auto res = apply_delta(it->second.second, obj.data);
                    if (!res) return flux::unexpected(res.error());
                    final_data = std::move(*res);
                    final_type = it->second.first;
                    can_resolve = true;
                }
            } else if (obj.type == 7) {
                auto it = id_cache.find(*obj.base_id);
                if (it != id_cache.end()) {
                    auto res = apply_delta(it->second.second, obj.data);
                    if (!res) return flux::unexpected(res.error());
                    final_data = std::move(*res);
                    final_type = it->second.first;
                    can_resolve = true;
                } else {
                    auto store_res = repo_.object_store().read(*obj.base_id);
                    if (store_res) {
                        auto type_res = repo_.object_store().get_type(*obj.base_id);
                        if (type_res) {
                            auto res = apply_delta(*store_res, obj.data);
                            if (!res) return flux::unexpected(res.error());
                            final_data = std::move(*res);
                            final_type = *type_res;
                            can_resolve = true;
                        }
                    }
                }
            }

            if (can_resolve) {
                auto id_res = repo_.object_store().write(final_type, final_data, HashAlgorithm::SHA1);
                if (!id_res) return flux::unexpected(id_res.error());
                cache[obj.offset] = {final_type, final_data};
                id_cache[*id_res] = {final_type, std::move(final_data)};
                resolved[i] = true;
                resolved_count++;
                progress = true;
            }
        }
    }

    if (resolved_count < objects.size())
        return flux::unexpected(fmt::format("Resolved only {}/{} objects", resolved_count, objects.size()));

    return resolved_count;
}

Result<Bytes> GitPackUnpacker::apply_delta(std::span<const uint8_t> base, std::span<const uint8_t> delta) {
    size_t pos = 0;
    auto read_varint = [&](size_t& p) {
        size_t val = 0, shift = 0;
        uint8_t b;
        do { b = delta[p++]; val |= (static_cast<size_t>(b & 0x7F) << shift); shift += 7; } while (b & 0x80);
        return val;
    };
    if (delta.size() < 2) return flux::unexpected("Delta too short");
    size_t base_size = read_varint(pos);
    size_t target_size = read_varint(pos);
    if (base.size() != base_size) return flux::unexpected("Delta base size mismatch");

    Bytes result; result.reserve(target_size);
    while (pos < delta.size()) {
        uint8_t cmd = delta[pos++];
        if (cmd & 0x80) {
            size_t off = 0, sz = 0;
            if (cmd & 0x01) off |= static_cast<size_t>(delta[pos++]);
            if (cmd & 0x02) off |= static_cast<size_t>(delta[pos++]) << 8;
            if (cmd & 0x04) off |= static_cast<size_t>(delta[pos++]) << 16;
            if (cmd & 0x08) off |= static_cast<size_t>(delta[pos++]) << 24;
            if (cmd & 0x10) sz |= static_cast<size_t>(delta[pos++]);
            if (cmd & 0x20) sz |= static_cast<size_t>(delta[pos++]) << 8;
            if (cmd & 0x40) sz |= static_cast<size_t>(delta[pos++]) << 16;
            if (sz == 0) sz = 0x10000;
            if (off + sz > base.size()) return flux::unexpected("Delta copy out of bounds");
            result.insert(result.end(), base.begin() + off, base.begin() + off + sz);
        } else if (cmd != 0) {
            if (pos + cmd > delta.size()) return flux::unexpected("Delta insert out of bounds");
            result.insert(result.end(), delta.begin() + pos, delta.begin() + pos + cmd);
            pos += cmd;
        } else return flux::unexpected("Invalid delta opcode 0");
    }
    if (result.size() != target_size) return flux::unexpected("Delta size mismatch");
    return result;
}

} // namespace flux
