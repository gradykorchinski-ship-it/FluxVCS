#include <gtest/gtest.h>
#include "flux/core/object_id.hpp"
#include "flux/util/hash.hpp"

TEST(ObjectIdTest, ComputeAndSerialize) {
    std::string data = "Hello, FluxVCS!";
    std::vector<uint8_t> bytes(data.begin(), data.end());
    
    auto id = flux::ObjectId::compute(flux::HashAlgorithm::SHA256, bytes);
    
    EXPECT_TRUE(id.is_valid());
    EXPECT_EQ(id.algorithm(), flux::HashAlgorithm::SHA256);
    EXPECT_EQ(id.size(), 32);  // SHA-256 is 32 bytes
    
    // Test serialization
    std::string hex = id.to_hex();
    EXPECT_TRUE(hex.starts_with("sha256:"));
    
    // Test deserialization
    auto id2_result = flux::ObjectId::from_hex(hex);
    ASSERT_TRUE(id2_result.has_value());
    EXPECT_EQ(id, *id2_result);
}

TEST(ObjectIdTest, Comparison) {
    std::string data1 = "test1";
    std::string data2 = "test2";
    
    std::vector<uint8_t> bytes1(data1.begin(), data1.end());
    std::vector<uint8_t> bytes2(data2.begin(), data2.end());
    
    auto id1 = flux::ObjectId::compute(flux::HashAlgorithm::SHA256, bytes1);
    auto id2 = flux::ObjectId::compute(flux::HashAlgorithm::SHA256, bytes2);
    auto id3 = flux::ObjectId::compute(flux::HashAlgorithm::SHA256, bytes1);
    
    EXPECT_NE(id1, id2);
    EXPECT_EQ(id1, id3);
}
