#pragma once
#include <string>
#include <vector>
#include "chunker.hpp"
#include "merkle_tree.hpp"
#include "crdt_store.hpp"
#include "file_store.hpp"

// Ties the three algorithmic pieces together for a single local node:
//   1. Chunker            -> content-defined chunks of a file's bytes
//   2. MerkleTree          -> cheap "did this file actually change, and
//                             which chunks specifically" detection
//   3. CrdtStore           -> conflict-free metadata versioning across nodes
//
// SyncEngine is the layer NetworkSession and the CLI talk to; it never
// touches sockets itself.
class SyncEngine {
public:
    SyncEngine(std::string node_id, std::string data_dir);

    // Ingests local file bytes: chunk, hash, diff against the previous
    // local version (if any) purely for reporting, store new chunks, and
    // record the new version in the CRDT store.
    FileEntry ingest_file(const std::string& path, const std::vector<uint8_t>& bytes);

    void delete_file(const std::string& path);

    CrdtStore& store() { return store_; }
    const CrdtStore& store() const { return store_; }
    FileStore& blobs() { return blobs_; }
    const FileStore& blobs() const { return blobs_; }
    const std::string& node_id() const { return node_id_; }
    const std::string& materialized_dir() const { return materialized_dir_; }

    // Writes the current (post-merge) content of `path` out to disk under
    // the materialized directory, e.g. after adopting a remote version.
    void materialize(const std::string& path);

    // Persists the current CRDT metadata state to disk so it survives
    // process restarts. Called automatically after every local mutation;
    // also called explicitly by the network layer after a sync session
    // merges in remote entries directly via store().merge_remote().
    void save_journal() const;

private:
    void load_journal();
    std::string journal_path() const { return data_dir_ + "/store.meta"; }

    std::string node_id_;
    std::string data_dir_;
    std::string materialized_dir_;
    Chunker chunker_;
    FileStore blobs_;
    CrdtStore store_;
};
