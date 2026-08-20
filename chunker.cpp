#include "rolling_hash.hpp"
#include "chunker.hpp"
#include <algorithm>

const uint64_t RollingHash::GEAR_TABLE[256] = {
#include "chunker_gear_table.inc"
};

RollingHash::RollingHash() : hash_(0) {}

void RollingHash::reset() { hash_ = 0; }

uint64_t RollingHash::roll(uint8_t byte) {
    // Gear hash: shift left, add table-looked-up value. Cheap (one shift,
    // one add, one table lookup per byte) which matters because this runs
    // once per byte of every file we chunk.
    hash_ = (hash_ << 1) + GEAR_TABLE[byte];
    return hash_;
}

// ---------------------------------------------------------------------
// Chunker
// ---------------------------------------------------------------------

Chunker::Chunker(size_t min_size, size_t avg_size, size_t max_size)
    : min_size_(min_size), max_size_(max_size) {
    // Choose a bitmask so that P(boundary) ~= 1/avg_size, matching the
    // classic FastCDC / restic approach: a boundary is declared when the
    // low `mask_bits_` bits of the rolling hash are all zero.
    mask_bits_ = 0;
    size_t v = avg_size;
    while (v > 1) { v >>= 1; mask_bits_++; }
    mask_ = (mask_bits_ >= 64) ? ~0ULL : ((1ULL << mask_bits_) - 1);
}

std::vector<Chunk> Chunker::chunk(const std::vector<uint8_t>& data) const {
    std::vector<Chunk> chunks;
    if (data.empty()) return chunks;

    RollingHash rh;
    size_t start = 0;
    size_t i = 0;
    size_t n = data.size();

    while (i < n) {
        size_t window = 0;
        uint64_t h = 0;
        rh.reset();

        for (size_t j = start; j < n; ++j) {
            h = rh.roll(data[j]);
            window = j - start + 1;

            bool at_min = window >= min_size_;
            bool at_max = window >= max_size_;
            bool boundary = at_min && ((h & mask_) == 0);

            if (boundary || at_max) {
                size_t end = j + 1; // exclusive
                chunks.push_back(make_chunk(data, start, end));
                start = end;
                i = end;
                goto next_chunk;
            }
        }
        // Reached end of data without hitting a boundary: final chunk.
        chunks.push_back(make_chunk(data, start, n));
        i = n;
    next_chunk:
        continue;
    }
    return chunks;
}

Chunk Chunker::make_chunk(const std::vector<uint8_t>& data, size_t start, size_t end) {
    Chunk c;
    c.offset = start;
    c.data.assign(data.begin() + static_cast<long>(start),
                   data.begin() + static_cast<long>(end));
    c.hash = sha256::hash(c.data);
    return c;
}
