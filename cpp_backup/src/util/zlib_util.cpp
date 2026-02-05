#include "flux/util/zlib_util.hpp"
#include <zlib.h>
#include <fmt/format.h>

namespace flux {

Result<Bytes> ZlibUtil::decompress(std::span<const uint8_t> data, size_t expected_size) {
    auto result = decompress_partial(data, expected_size);
    if (!result) return flux::unexpected(result.error());
    return std::move(result->decompressed);
}

Result<ZlibUtil::DecompressResult> ZlibUtil::decompress_partial(std::span<const uint8_t> data, size_t expected_size) {
    if (data.empty()) {
        return flux::unexpected("No data to decompress");
    }

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = static_cast<uInt>(data.size());
    strm.next_in = const_cast<Bytef*>(data.data());

    if (inflateInit(&strm) != Z_OK) {
        return flux::unexpected("Failed to initialize zlib inflate");
    }

    DecompressResult result;
    if (expected_size > 0) {
        result.decompressed.resize(expected_size);
    } else {
        result.decompressed.resize(std::min(data.size() * 2, size_t(16384)));
    }

    strm.avail_out = static_cast<uInt>(result.decompressed.size());
    strm.next_out = result.decompressed.data();

    int ret;
    while (true) {
        ret = inflate(&strm, Z_NO_FLUSH);
        
        if (ret == Z_STREAM_END) break;
        if (ret == Z_OK) {
            // Need more space
            size_t current_size = result.decompressed.size();
            result.decompressed.resize(current_size * 2);
            strm.next_out = result.decompressed.data() + current_size;
            strm.avail_out = static_cast<uInt>(current_size);
        } else {
            inflateEnd(&strm);
            return flux::unexpected(fmt::format("Zlib inflate failed: {} (code {})", 
                                  strm.msg ? strm.msg : "unknown error", ret));
        }
    }

    // Shrink to actual size
    result.decompressed.resize(strm.total_out);
    result.consumed = strm.total_in;
    
    inflateEnd(&strm);
    
    return DecompressResult{std::move(result.decompressed), data.size() - strm.avail_in};
}

Result<Bytes> ZlibUtil::compress(std::span<const uint8_t> data, int level) {
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    if (deflateInit(&strm, level) != Z_OK) {
        return flux::unexpected("Failed to initialize zlib deflate");
    }

    Bytes result;
    result.resize(compressBound(data.size()));

    strm.next_in = const_cast<uint8_t*>(data.data());
    strm.avail_in = static_cast<uInt>(data.size());
    strm.next_out = result.data();
    strm.avail_out = static_cast<uInt>(result.size());

    int ret = deflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        deflateEnd(&strm);
        return flux::unexpected("Zlib compression failed");
    }

    result.resize(strm.total_out);
    deflateEnd(&strm);

    return result;
}

} // namespace flux
