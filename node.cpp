#include "node.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

Node::Node(std::string node_id, std::string data_dir)
    : engine_(std::move(node_id), std::move(data_dir)) {}

void Node::add_file(const std::string& logical_path, const std::string& source_path) {
    std::ifstream in(source_path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open source file: " + source_path);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
    engine_.ingest_file(logical_path, bytes);
}

void Node::remove_file(const std::string& logical_path) {
    engine_.delete_file(logical_path);
}

void Node::list_files() const {
    for (const auto& e : engine_.store().all_entries()) {
        if (e.deleted) {
            std::cout << "  [deleted]  " << e.path << "  clock=" << e.clock.to_string() << "\n";
        } else {
            std::cout << "  " << e.path << "  (" << e.size << " bytes, "
                       << e.chunk_hashes.size() << " chunks)  clock="
                       << e.clock.to_string() << "  last_writer=" << e.last_writer << "\n";
        }
    }
}

SyncStats Node::sync_with(const std::string& host, int port) {
    TcpSocket sock = TcpSocket::connect_to(host, port);
    return run_sync_session(sock, engine_);
}

SyncStats Node::listen_and_sync(int port) {
    std::cout << "[" << engine_.node_id() << "] listening on port " << port << " ...\n";
    TcpSocket sock = TcpSocket::accept_once(port);
    std::cout << "[" << engine_.node_id() << "] peer connected, syncing...\n";
    return run_sync_session(sock, engine_);
}
