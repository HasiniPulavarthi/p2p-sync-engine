#include "merkle_tree.hpp"
#include "chunker.hpp"
#include <cassert>
#include <iostream>

static Chunk make_chunk(const std::string& s, size_t offset) {
    Chunk c;
    c.offset = offset;
    c.data.assign(s.begin(), s.end());
    c.hash = sha256::hash(c.data);
    return c;
}

int main() {
    std::vector<Chunk> chunks = {
        make_chunk("alpha", 0),
        make_chunk("bravo", 5),
        make_chunk("charlie", 10),
        make_chunk("delta", 17),
        make_chunk("echo", 22),
    };

    auto tree_a = MerkleTree::build(chunks);
    auto tree_b = MerkleTree::build(chunks);

    // 1. Identical chunk lists produce identical roots.
    assert(tree_a.root() == tree_b.root());
    std::cout << "[ok] identical content -> identical Merkle root\n";

    // 2. Changing one chunk changes the root, and diff_leaf_indices finds
    //    exactly that one leaf without needing to compare every chunk.
    auto chunks_c = chunks;
    chunks_c[2] = make_chunk("CHARLIE-EDITED", 10);
    auto tree_c = MerkleTree::build(chunks_c);

    assert(!(tree_a.root() == tree_c.root()));
    auto diffs = MerkleTree::diff_leaf_indices(tree_a, tree_c);
    assert(diffs.size() == 1);
    assert(diffs[0] == 2);
    std::cout << "[ok] single-chunk edit isolated to exactly one leaf index\n";

    // 3. Two independent edits are both found.
    auto chunks_d = chunks;
    chunks_d[0] = make_chunk("ALPHA-EDITED", 0);
    chunks_d[4] = make_chunk("ECHO-EDITED", 22);
    auto tree_d = MerkleTree::build(chunks_d);
    auto diffs2 = MerkleTree::diff_leaf_indices(tree_a, tree_d);
    assert(diffs2.size() == 2);
    std::cout << "[ok] multiple edits isolated to their respective leaves\n";

    // 4. Mismatched leaf counts are reported as "can't align" (empty diff),
    //    signalling the caller to fall back to a full transfer.
    auto chunks_e = chunks;
    chunks_e.push_back(make_chunk("foxtrot", 27));
    auto tree_e = MerkleTree::build(chunks_e);
    auto diffs3 = MerkleTree::diff_leaf_indices(tree_a, tree_e);
    assert(diffs3.empty());
    std::cout << "[ok] leaf-count mismatch correctly reported as unalignable\n";

    std::cout << "All Merkle tree tests passed.\n";
    return 0;
}
