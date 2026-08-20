#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "sync_engine.hpp"

// Minimal length-framed TCP transport: every message is
//   [4-byte big-endian length][1 type byte][payload]
// Two message types matter for the sync protocol:
//   MANIFEST      - text payload listing this node's FileEntry metadata
//   CHUNK_REQUEST - text payload: newline-separated hex chunk hashes wanted
//   CHUNK_DATA    - binary payload: repeated [32-byte hash][4-byte
//                   length][raw chunk bytes], used to answer a request
class TcpSocket {
public:
    TcpSocket() = default;
    explicit TcpSocket(int fd) : fd_(fd) {}
    ~TcpSocket();

    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;
    TcpSocket(TcpSocket&& other) noexcept;
    TcpSocket& operator=(TcpSocket&& other) noexcept;

    static TcpSocket connect_to(const std::string& host, int port);
    // Listens, accepts exactly one connection, and returns the accepted socket.
    static TcpSocket accept_once(int port);

    void send_frame(uint8_t type, const std::vector<uint8_t>& payload) const;
    // Returns false on clean peer disconnect.
    bool recv_frame(uint8_t& type, std::vector<uint8_t>& payload) const;

    bool valid() const { return fd_ >= 0; }

private:
    int fd_ = -1;
};

enum FrameType : uint8_t {
    FRAME_MANIFEST = 1,
    FRAME_CHUNK_REQUEST = 2,
    FRAME_CHUNK_DATA = 3,
    FRAME_DONE = 4,
};

struct SyncStats {
    int entries_sent = 0;
    int entries_applied_remote = 0;
    int entries_kept_local = 0;
    int entries_no_change = 0;
    int conflicts_forked = 0;
    int chunks_fetched = 0;
};

// Runs a full two-way sync over an already-connected socket. Both the
// initiator (the side that called connect) and the responder (the side
// that called accept) run the same symmetric exchange, so either role can
// call this -- the protocol doesn't distinguish "client" and "server"
// beyond who opened the TCP connection.
SyncStats run_sync_session(TcpSocket& sock, SyncEngine& engine);

std::vector<uint8_t> serialize_manifest(const SyncEngine& engine);
std::vector<FileEntry> parse_manifest(const std::vector<uint8_t>& payload);
