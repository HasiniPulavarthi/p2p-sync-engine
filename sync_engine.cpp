#include "sync_engine.hpp"
#include "entry_codec.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

SyncEngine::SyncEngine(std::string node_id, std::string data_dir)
    : node_id_(std::move(node_id)),
      data_dir_(std::move(data_dir)),
      materialized_dir_(data_dir_ + "/files"),
      blobs_(data_dir_ + "/blobstore"),
      store_(node_id_) {
    std::filesystem::create_directories(data_dir_);
    load_journal();
}

void SyncEngine::load_journal() {
    std::ifstream in(journal_path());
    if (!in) return; // first run for this node -- nothing to restore yet
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        store_.load_entry(line_to_entry(line));
    }
}

void SyncEngine::save_journal() const {
    std::ofstream out(journal_path(), std::ios::trunc);
    for (const auto& e : store_.all_entries()) {
        out << entry_to_line(e) << "\n";
    }
}

FileEntry SyncEngine::ingest_file(const std::string& path, const std::vector<uint8_t>& bytes) {
    auto chunks = chunker_.chunk(bytes);
    MerkleTree new_tree = MerkleTree::build(chunks);

    // Report how many chunks actually changed vs. the previous local
    // version, purely to demonstrate the point of content-defined
    // chunking + Merkle diffing: small edits should touch few chunks.
    auto prev = store_.get(path);
    if (prev && prev->has_content()) {
        std::vector<Chunk> prev_placeholder; // we only have hashes, not bytes, for old chunks
        // Build a hash-only comparison since we don't retain old raw chunks
        // here; leaf-for-leaf hash comparison still tells us what changed
        // when chunk counts match.
        if (prev->chunk_hashes.size() == chunks.size()) {
            size_t changed = 0;
            for (size_t i = 0; i < chunks.size(); ++i) {
                if (!(prev->chunk_hashes[i] == chunks[i].hash)) changed++;
            }
            std::cout << "[" << node_id_ << "] " << path << ": "
                      << changed << "/" << chunks.size()
                      << " chunks changed vs. previous version\n";
        } else {
            std::cout << "[" << node_id_ << "] " << path << ": chunk count changed ("
                      << prev->chunk_hashes.size() << " -> " << chunks.size()
                      << "), treating as full re-chunk\n";
        }
    } else {
        std::cout << "[" << node_id_ << "] " << path << ": new file, "
                  << chunks.size() << " chunk(s)\n";
    }

    std::vector<sha256::Digest> hashes;
    hashes.reserve(chunks.size());
    uint64_t total_size = 0;
    for (const auto& c : chunks) {
        blobs_.put_chunk(c);
        hashes.push_back(c.hash);
        total_size += c.data.size();
    }

    store_.put_local(path, hashes, new_tree.empty() ? sha256::Digest{} : new_tree.root(), total_size);
    materialize(path);
    save_journal();
    return *store_.get(path);
}

void SyncEngine::delete_file(const std::string& path) {
    store_.delete_local(path);
    save_journal();
}

void SyncEngine::materialize(const std::string& path) {
    auto entry = store_.get(path);
    if (!entry || !entry->has_content()) return;
    blobs_.materialize(materialized_dir_, path, entry->chunk_hashes);
}
