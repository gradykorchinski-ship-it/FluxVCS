#include "flux/util/hash.hpp"
#include <array>
#include <fmt/format.h>
#include <openssl/evp.h>
#include <stdexcept>

namespace flux {

namespace {
Bytes compute_evp(const EVP_MD *type, std::span<const uint8_t> data) {
  if (!type) {
    throw std::runtime_error("Unknown hash algorithm");
  }

  unsigned int md_len = EVP_MD_size(type);
  Bytes hash(md_len);

  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (!ctx) {
    throw std::runtime_error("Failed to create EVP_MD_CTX");
  }

  if (EVP_DigestInit_ex(ctx, type, nullptr) != 1 ||
      EVP_DigestUpdate(ctx, data.data(), data.size()) != 1 ||
      EVP_DigestFinal_ex(ctx, hash.data(), &md_len) != 1) {
    EVP_MD_CTX_free(ctx);
    throw std::runtime_error("OpenSSL hashing failed");
  }

  EVP_MD_CTX_free(ctx);
  return hash;
}
} // namespace

Bytes Hash::compute(HashAlgorithm algo, std::span<const uint8_t> data) {
  switch (algo) {
  case HashAlgorithm::SHA1:
    return compute_sha1(data);
  case HashAlgorithm::SHA256:
    return compute_sha256(data);
  case HashAlgorithm::SHA3_256:
    return compute_sha3_256(data);
  case HashAlgorithm::BLAKE3:
    // BLAKE3 not standard in OpenSSL yet, fallback to SHA256 or throw?
    // Existing code fell back to SHA256. Let's keep fallback but maybe warn?
    // For now, consistent with previous behavior:
    return compute_sha256(data);
  }
  return compute_sha256(data);
}

Bytes Hash::compute_sha1(std::span<const uint8_t> data) {
  return compute_evp(EVP_sha1(), data);
}

Bytes Hash::compute_sha256(std::span<const uint8_t> data) {
  return compute_evp(EVP_sha256(), data);
}

Bytes Hash::compute_sha3_256(std::span<const uint8_t> data) {
  return compute_evp(EVP_sha3_256(), data);
}

size_t Hash::hash_size(HashAlgorithm algo) {
  switch (algo) {
  case HashAlgorithm::SHA1:
    return 20;
  case HashAlgorithm::SHA256:
    return 32;
  case HashAlgorithm::SHA3_256:
    return 32;
  case HashAlgorithm::BLAKE3:
    return 32;
  }
  return 32; // Default
}

std::string Hash::to_hex(std::span<const uint8_t> data) {
  static constexpr char hex_digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(data.size() * 2);

  for (uint8_t byte : data) {
    result.push_back(hex_digits[byte >> 4]);
    result.push_back(hex_digits[byte & 0x0F]);
  }

  return result;
}

Result<Bytes> Hash::from_hex(std::string_view hex) {
  if (hex.size() % 2 != 0) {
    return flux::unexpected("Hex string must have even length");
  }

  Bytes result;
  result.reserve(hex.size() / 2);

  static constexpr int8_t lookup[] = {
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  -1, -1, -1, -1, -1, -1,
      -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

  for (size_t i = 0; i < hex.size(); i += 2) {
    char high = hex[i];
    char low = hex[i + 1];

    if (static_cast<unsigned char>(high) > 127 ||
        static_cast<unsigned char>(low) > 127 ||
        lookup[static_cast<unsigned char>(high)] == -1 ||
        lookup[static_cast<unsigned char>(low)] == -1) {
      return flux::unexpected(
          fmt::format("Invalid hex character at position {}", i));
    }

    result.push_back((lookup[static_cast<unsigned char>(high)] << 4) |
                     lookup[static_cast<unsigned char>(low)]);
  }

  return result;
}

} // namespace flux
