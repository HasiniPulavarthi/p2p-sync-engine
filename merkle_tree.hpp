#pragma once
#include <vector>
#include <memory>
#include "sha256.hpp"
#include "chunker.hpp"

// A Merkle tree built over a file's ordered chunk hashes.
//
// Why: comparing two files chunk-by-chunk is O(chunks). Comparing two
// Merkle roots is O(1) — if the roots match, the files are identical, full
// stop, no transfer needed. If they differ, walking down only the subtrees
// whose hashes differ finds the *changed* chunks in O(log n + changed
// chunks) instead of re-diffing everything. This is exactly how Dropbox's
// block-level sync and Git's tree objects avoid re-hashing whole trees on
// small changes.
struct MerkleNode {
    sha256::Digest hash;
    std::unique_ptr<MerkleNode> left;
    std::unique_ptr<MerkleNode> right;
    int leaf_index = -1; // >= 0 for leaves: index into the chunk list
    bool is_leaf() const { return leaf_index >= 0; }
};

class MerkleTree {
public:
    MerkleTree() = default;

    // Builds the tree from a list of chunks (in file order).
    static MerkleTree build(const std::vector<Chunk>& chunks);

    const sha256::Digest& root() const { return root_->hash; }
    bool empty() const { return root_ == nullptr; }
    size_t leaf_count() const { return leaf_count_; }

    // Returns the leaf indices whose hashes differ between `a` and `b`.
    // Requires both trees to have been built over chunk lists of the
    // same length (variable-length CDC chunking means this holds only
    // when comparing versions of files chunked the same way; the sync
    // engine falls back to a full transfer when leaf counts diverge).
    static std::vector<int> diff_leaf_indices(const MerkleTree& a, const MerkleTree& b);

private:
    static std::unique_ptr<MerkleNode> build_recursive(
        const std::vector<Chunk>& chunks, size_t lo, size_t hi);

    static void collect_diffs(const MerkleNode* a, const MerkleNode* b, std::vector<int>& out);

    std::unique_ptr<MerkleNode> root_;
    size_t leaf_count_ = 0;
};
