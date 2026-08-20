#pragma once
#include <cstdint>
#include <cstddef>

// Gear-hash style rolling hash used for Content-Defined Chunking (CDC).
// Unlike fixed-size chunking, a rolling hash lets us pick chunk boundaries
// based on *content*, so inserting a byte near the start of a file only
// shifts one chunk boundary instead of re-chunking the entire file.
// This is the same core trick used by rsync, LBFS, and restic.
class RollingHash {
public:
    RollingHash();

    // Feed one byte in and return the updated 64-bit hash value.
    uint64_t roll(uint8_t byte);

    void reset();

private:
    uint64_t hash_;
    static const uint64_t GEAR_TABLE[256];
};
