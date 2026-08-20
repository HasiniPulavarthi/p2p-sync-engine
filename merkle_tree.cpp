#include "merkle_tree.hpp"

std::unique_ptr<MerkleNode> MerkleTree::build_recursive(
    const std::vector<Chunk>& chunks, size_t lo, size_t hi) {

    auto node = std::make_unique<MerkleNode>();
    if (hi - lo == 1) {
        node->hash = chunks[lo].hash;
        node->leaf_index = static_cast<int>(lo);
        return node;
    }
    size_t mid = lo + (hi - lo) / 2;
    node->left = build_recursive(chunks, lo, mid);
    node->right = build_recursive(chunks, mid, hi);
    node->hash = sha256::combine(node->left->hash, node->right->hash);
    return node;
}

MerkleTree MerkleTree::build(const std::vector<Chunk>& chunks) {
    MerkleTree tree;
    if (chunks.empty()) return tree;
    tree.root_ = build_recursive(chunks, 0, chunks.size());
    tree.leaf_count_ = chunks.size();
    return tree;
}

void MerkleTree::collect_diffs(const MerkleNode* a, const MerkleNode* b, std::vector<int>& out) {
    if (a == nullptr || b == nullptr) return;
    if (a->hash == b->hash) return; // identical subtree, nothing changed beneath it

    if (a->is_leaf() && b->is_leaf()) {
        out.push_back(a->leaf_index);
        return;
    }
    // Structural mismatch (one side is a leaf, the other isn't) shouldn't
    // happen when leaf counts match, but guard defensively.
    if (a->is_leaf() || b->is_leaf()) {
        out.push_back(a->is_leaf() ? a->leaf_index : b->leaf_index);
        return;
    }
    collect_diffs(a->left.get(), b->left.get(), out);
    collect_diffs(a->right.get(), b->right.get(), out);
}

std::vector<int> MerkleTree::diff_leaf_indices(const MerkleTree& a, const MerkleTree& b) {
    std::vector<int> out;
    if (a.empty() || b.empty() || a.leaf_count() != b.leaf_count()) {
        // Can't align leaves directly (e.g. file grew/shrank chunk count) —
        // caller should treat this as "everything differs".
        return out;
    }
    collect_diffs(a.root_.get(), b.root_.get(), out);
    return out;
}
