#pragma once
#include <string>
#include <vector>
#include "sha256.hpp"
#include "vector_clock.hpp"

// Metadata CRDT payload for one file path. The actual chunk *bytes* live
// content-addressed in FileStore, keyed by hash — FileEntry only carries
// the ordered list of chunk hashes plus enough causality/version info to
// merge safely without coordination.
struct FileEntry {
    std::string path;
    std::vector<sha256::Digest> chunk_hashes; // ordered -> defines file content
    sha256::Digest merkle_root{};
    VectorClock clock;
    std::string last_writer;   // node id that made the most recent local edit
    bool deleted = false;      // tombstone for OR-Set-style delete handling
    uint64_t size = 0;

    bool has_content() const { return !deleted && !chunk_hashes.empty(); }
};
