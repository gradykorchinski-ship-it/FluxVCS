#include "flux/util/hash.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>

TEST(HashTest, SHA1) {
  std::string data = "The quick brown fox jumps over the lazy dog";
  std::vector<uint8_t> bytes(data.begin(), data.end());
  // Known SHA1 for this string: 2fd4e1c67a2d28fced849ee1bb76e7391b93eb12

  auto hash = flux::Hash::compute(flux::HashAlgorithm::SHA1, bytes);

  EXPECT_EQ(hash.size(), 20);
  EXPECT_EQ(flux::Hash::to_hex(hash),
            "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");
}

TEST(HashTest, SHA256) {
  std::string data = "The quick brown fox jumps over the lazy dog";
  std::vector<uint8_t> bytes(data.begin(), data.end());
  // Known SHA256:
  // d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592

  auto hash = flux::Hash::compute(flux::HashAlgorithm::SHA256, bytes);

  EXPECT_EQ(hash.size(), 32);
  EXPECT_EQ(flux::Hash::to_hex(hash),
            "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
}

TEST(HashTest, SHA3_256) {
  std::string data = "The quick brown fox jumps over the lazy dog";
  std::vector<uint8_t> bytes(data.begin(), data.end());
  // Known SHA3-256:
  // 69070dda01975c8c120c3aada1b282394e7f032fa9cf32f4cb2259a0897dfc04

  auto hash = flux::Hash::compute(flux::HashAlgorithm::SHA3_256, bytes);

  EXPECT_EQ(hash.size(), 32);
  EXPECT_EQ(flux::Hash::to_hex(hash),
            "69070dda01975c8c120c3aada1b282394e7f032fa9cf32f4cb2259a0897dfc04");
}

TEST(HashTest, HexRoundTrip) {
  std::vector<uint8_t> original = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xFF};
  std::string hex = flux::Hash::to_hex(original);

  EXPECT_EQ(hex, "deadbeef00ff");

  auto result = flux::Hash::from_hex(hex);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, original);
}
