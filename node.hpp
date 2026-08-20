#pragma once
#include <string>
#include <vector>
#include "sync_engine.hpp"
#include "network.hpp"

// A Node is one participant in the P2P mesh: it owns a local SyncEngine
// (chunk store + CRDT metadata store) and knows how to add/remove files
// and sync with a peer over TCP. Nodes are symmetric -- there is no
// client/server distinction beyond who initiates the TCP connection.
class Node {
public:
    Node(std::string node_id, std::string data_dir);

    // Reads a real file from `source_path` on disk and ingests it into the
    // sync engine under the logical name `logical_path`.
    void add_file(const std::string& logical_path, const std::string& source_path);
    void remove_file(const std::string& logical_path);
    void list_files() const;

    // Acts as the TCP connector.
    SyncStats sync_with(const std::string& host, int port);
    // Acts as the TCP acceptor (blocks until one peer connects).
    SyncStats listen_and_sync(int port);

    SyncEngine& engine() { return engine_; }

private:
    SyncEngine engine_;
};
