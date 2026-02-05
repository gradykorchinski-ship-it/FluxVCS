#pragma once

#include "flux/core/types.hpp"
#include <span>

namespace flux {

/**
 * Zlib utilities for Git pack decompression
 */
class ZlibUtil {
public:
    /**
     * Decompress a raw deflate stream (zlib format used in Git packs)
     * @param data Compressed data
     * @param expected_size Expected decompressed size (optimization)
     * @return Decompressed bytes or error
     */
    static Result<Bytes> decompress(std::span<const uint8_t> data, size_t expected_size = 0);

    /**
     * Decompress part of a stream and return how many bytes were consumed
     * Useful for stream-based parsing of pack files
     */
    struct DecompressResult {
        Bytes decompressed;
        size_t consumed;
    };
    static Result<DecompressResult> decompress_partial(std::span<const uint8_t> data, size_t expected_size = 0);

    /**
     * Compress data using zlib deflate
     * @param data Data to compress
     * @param level Compression level (default: Z_DEFAULT_COMPRESSION)
     * @return Compressed bytes or error
     */
    static Result<Bytes> compress(std::span<const uint8_t> data, int level = -1);
};

} // namespace flux
