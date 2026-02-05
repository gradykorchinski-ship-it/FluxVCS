#include <gtest/gtest.h>
#include "flux/util/chunker.hpp"

TEST(ChunkerTest, BasicChunking) {
    // Create test data
    std::vector<uint8_t> data(1024 * 1024);  // 1 MB
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i % 256);
    }
    
    flux::ContentChunker chunker;
    auto chunks = chunker.chunk(data);
    
    // Should have at least one chunk
    EXPECT_GT(chunks.size(), 0);
    
    // Verify chunks cover all data
    size_t total_size = 0;
    for (const auto& [offset, size] : chunks) {
        total_size += size;
        EXPECT_GE(size, flux::ContentChunker::MIN_CHUNK_SIZE);
        EXPECT_LE(size, flux::ContentChunker::MAX_CHUNK_SIZE);
    }
    
    EXPECT_EQ(total_size, data.size());
}

TEST(ChunkerTest, EmptyData) {
    std::vector<uint8_t> data;
    
    flux::ContentChunker chunker;
    auto chunks = chunker.chunk(data);
    
    EXPECT_EQ(chunks.size(), 0);
}

TEST(ChunkerTest, SmallData) {
    std::vector<uint8_t> data(1024);  // 1 KB
    
    flux::ContentChunker chunker;
    auto chunks = chunker.chunk(data);
    
    // Small data should be a single chunk
    EXPECT_EQ(chunks.size(), 1);
    EXPECT_EQ(chunks[0].second, 1024);
}
