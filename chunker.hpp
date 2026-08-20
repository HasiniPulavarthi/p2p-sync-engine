#pragma once
#include <cstdint>
#include <vector>
#include "sha256.hpp"

// One content-defined chunk of a file.
struct Chunk {
    size_t offset;              // byte offset within the file
    std::vector<uint8_t> data;  // raw chunk bytes
    sha256::Digest hash;        // content hash of `data`
};

// Splits a byte buffer into variable-length, content-defined chunks so that
// local edits only change the chunks touching the edit, not everything
// after it (the classic problem with naive fixed-size chunking). Chunk
// boundaries are picked with a rolling gear hash, min/avg/max bound the
// chunk size so pathological inputs can't produce a 1-byte or unbounded
// chunk.
class Chunker {
public:
    explicit Chunker(size_t min_size = 2 * 1024,
                      size_t avg_size = 8 * 1024,
                      size_t max_size = 64 * 1024);

    std::vector<Chunk> chunk(const std::vector<uint8_t>& data) const;

private:
    static Chunk make_chunk(const std::vector<uint8_t>& data, size_t start, size_t end);

    size_t min_size_;
    size_t max_size_;
    size_t mask_bits_;
    uint64_t mask_;
};
