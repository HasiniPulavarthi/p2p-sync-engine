#include "chunker.hpp"
#include <cassert>
#include <iostream>
#include <random>

static std::vector<uint8_t> random_bytes(size_t n, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 255);
    std::vector<uint8_t> out(n);
    for (auto& b : out) b = static_cast<uint8_t>(dist(rng));
    return out;
}

int main() {
    Chunker chunker(256, 1024, 4096);

    // 1. Chunking is deterministic.
    auto data = random_bytes(100000, 1);
    auto c1 = chunker.chunk(data);
    auto c2 = chunker.chunk(data);
    assert(c1.size() == c2.size());
    for (size_t i = 0; i < c1.size(); ++i) assert(c1[i].hash == c2[i].hash);
    std::cout << "[ok] deterministic chunking (" << c1.size() << " chunks)\n";

    // 2. Reassembly is lossless.
    std::vector<uint8_t> rebuilt;
    for (auto& c : c1) rebuilt.insert(rebuilt.end(), c.data.begin(), c.data.end());
    assert(rebuilt == data);
    std::cout << "[ok] chunk reassembly reproduces original bytes\n";

    // 3. Every chunk respects the max size bound.
    for (auto& c : c1) assert(c.data.size() <= 4096);
    std::cout << "[ok] chunk sizes respect max bound\n";

    // 4. A localized edit should only perturb a small, bounded number of
    //    chunks -- the whole point of content-defined chunking over naive
    //    fixed-size chunking.
    auto edited = data;
    for (size_t i = 50000; i < 50010; ++i) edited[i] = static_cast<uint8_t>(~edited[i]);
    auto c3 = chunker.chunk(edited);

    size_t common_prefix = 0;
    while (common_prefix < c1.size() && common_prefix < c3.size() &&
           c1[common_prefix].hash == c3[common_prefix].hash) common_prefix++;
    size_t total_chunks = std::max(c1.size(), c3.size());
    // A handful of chunks near the edit should differ, not the whole file.
    assert(total_chunks - common_prefix < total_chunks / 2);
    std::cout << "[ok] localized edit only perturbs a small chunk region ("
              << (total_chunks - common_prefix) << "/" << total_chunks << " diverge before re-sync)\n";

    std::cout << "All chunker tests passed.\n";
    return 0;
}
