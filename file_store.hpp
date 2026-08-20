#pragma once
#include <string>
#include <vector>
#include "sha256.hpp"
#include "chunker.hpp"

// Content-addressed local storage for chunk bytes: each chunk is written
// once to disk, named by the hex of its own hash. Because the name IS the
// hash, two nodes that end up with the same chunk (e.g. an unchanged
// region shared across file versions, or identical content in two
// different files) store it only once and never need to re-transfer it if
// they already have it -- this is the same dedup property Dropbox's block
// storage and restic's repository format rely on.
class FileStore {
public:
    explicit FileStore(std::string root_dir);

    void put_chunk(const Chunk& c) const;
    bool has_chunk(const sha256::Digest& hash) const;
    std::vector<uint8_t> get_chunk(const sha256::Digest& hash) const;

    // Reassembles a full file's bytes from an ordered list of chunk hashes.
    // Throws std::runtime_error if any chunk is missing locally.
    std::vector<uint8_t> reassemble(const std::vector<sha256::Digest>& hashes) const;

    // Writes reassembled bytes out to a real file under `materialized_dir`.
    void materialize(const std::string& materialized_dir,
                      const std::string& relative_path,
                      const std::vector<sha256::Digest>& hashes) const;

    std::string chunk_path(const sha256::Digest& hash) const;

private:
    std::string root_;
};
