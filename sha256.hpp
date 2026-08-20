#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Minimal, dependency-free SHA-256. Used everywhere we need a stable,
// collision-resistant content hash: chunk hashes, Merkle tree nodes, and
// CRDT entry identifiers all derive from this.
namespace sha256 {

using Digest = std::array<uint8_t, 32>;

Digest hash(const uint8_t* data, size_t len);
Digest hash(const std::vector<uint8_t>& data);
Digest hash(const std::string& s);

// Combine two digests into one (used to build interior Merkle-tree nodes):
// parent = SHA256(left || right)
Digest combine(const Digest& left, const Digest& right);

std::string to_hex(const Digest& d);

} // namespace sha256
