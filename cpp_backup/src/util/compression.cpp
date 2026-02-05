#include "flux/util/compression.hpp"
#include <zstd.h>
#include <fmt/format.h>

namespace flux {

Result<Bytes> Compression::compress(std::span<const uint8_t> data, int level) {
    size_t bound = ZSTD_compressBound(data.size());
    Bytes compressed(bound);
    
    size_t compressed_size = ZSTD_compress(
        compressed.data(), compressed.size(),
        data.data(), data.size(),
        level
    );
    
    if (ZSTD_isError(compressed_size)) {
        return flux::unexpected(fmt::format("Compression failed: {}", 
            ZSTD_getErrorName(compressed_size)));
    }
    
    compressed.resize(compressed_size);
    return compressed;
}

Result<Bytes> Compression::decompress(std::span<const uint8_t> data) {
    // Get decompressed size
    unsigned long long decompressed_size = ZSTD_getFrameContentSize(data.data(), data.size());
    
    if (decompressed_size == ZSTD_CONTENTSIZE_ERROR) {
        return flux::unexpected("Not compressed by zstd");
    }
    
    if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        return flux::unexpected("Decompressed size unknown");
    }
    
    Bytes decompressed(decompressed_size);
    
    size_t result = ZSTD_decompress(
        decompressed.data(), decompressed.size(),
        data.data(), data.size()
    );
    
    if (ZSTD_isError(result)) {
        return flux::unexpected(fmt::format("Decompression failed: {}", 
            ZSTD_getErrorName(result)));
    }
    
    return decompressed;
}

size_t Compression::compress_bound(size_t size) {
    return ZSTD_compressBound(size);
}

} // namespace flux
