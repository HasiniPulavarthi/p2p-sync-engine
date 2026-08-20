#include "network.hpp"
#include "entry_codec.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <set>

// --------------------------------------------------------------------
// TcpSocket
// --------------------------------------------------------------------

TcpSocket::~TcpSocket() {
    if (fd_ >= 0) ::close(fd_);
}

TcpSocket::TcpSocket(TcpSocket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) ::close(fd_);
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

TcpSocket TcpSocket::connect_to(const std::string& host, int port) {
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    std::string port_str = std::to_string(port);

    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0 || res == nullptr) {
        throw std::runtime_error("DNS/address resolution failed for " + host);
    }
    int fd = -1;
    for (auto* p = res; p != nullptr; p = p->ai_next) {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        ::close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) throw std::runtime_error("failed to connect to " + host + ":" + port_str);
    return TcpSocket(fd);
}

TcpSocket TcpSocket::accept_once(int port) {
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) throw std::runtime_error("socket() failed");

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(listen_fd);
        throw std::runtime_error("bind() failed on port " + std::to_string(port));
    }
    if (::listen(listen_fd, 1) < 0) {
        ::close(listen_fd);
        throw std::runtime_error("listen() failed");
    }

    sockaddr_in peer{};
    socklen_t peer_len = sizeof(peer);
    int conn_fd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&peer), &peer_len);
    ::close(listen_fd); // demo server: accept exactly one peer per invocation
    if (conn_fd < 0) throw std::runtime_error("accept() failed");
    return TcpSocket(conn_fd);
}

static void send_all(int fd, const uint8_t* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(fd, data + sent, len - sent, 0);
        if (n <= 0) throw std::runtime_error("send() failed / peer closed connection");
        sent += static_cast<size_t>(n);
    }
}

static bool recv_all(int fd, uint8_t* data, size_t len) {
    size_t got = 0;
    while (got < len) {
        ssize_t n = ::recv(fd, data + got, len - got, 0);
        if (n == 0) return false; // clean disconnect
        if (n < 0) throw std::runtime_error("recv() failed");
        got += static_cast<size_t>(n);
    }
    return true;
}

void TcpSocket::send_frame(uint8_t type, const std::vector<uint8_t>& payload) const {
    uint32_t len = static_cast<uint32_t>(payload.size());
    uint8_t header[5];
    header[0] = static_cast<uint8_t>((len >> 24) & 0xff);
    header[1] = static_cast<uint8_t>((len >> 16) & 0xff);
    header[2] = static_cast<uint8_t>((len >> 8) & 0xff);
    header[3] = static_cast<uint8_t>(len & 0xff);
    header[4] = type;
    send_all(fd_, header, sizeof(header));
    if (!payload.empty()) send_all(fd_, payload.data(), payload.size());
}

bool TcpSocket::recv_frame(uint8_t& type, std::vector<uint8_t>& payload) const {
    uint8_t header[5];
    if (!recv_all(fd_, header, sizeof(header))) return false;
    uint32_t len = (uint32_t(header[0]) << 24) | (uint32_t(header[1]) << 16) |
                   (uint32_t(header[2]) << 8) | uint32_t(header[3]);
    type = header[4];
    payload.assign(len, 0);
    if (len > 0 && !recv_all(fd_, payload.data(), len)) return false;
    return true;
}

// --------------------------------------------------------------------
// Manifest (metadata) serialization -- plain text, one entry per line,
// using the same codec as the on-disk persistence journal (entry_codec.*)
// so "what we'd save to disk" and "what we send over the wire" never
// drift out of sync with each other.
// --------------------------------------------------------------------

std::vector<uint8_t> serialize_manifest(const SyncEngine& engine) {
    std::ostringstream oss;
    oss << engine.node_id() << "\n";
    for (const auto& e : engine.store().all_entries()) {
        oss << entry_to_line(e) << "\n";
    }
    std::string s = oss.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}

std::vector<FileEntry> parse_manifest(const std::vector<uint8_t>& payload) {
    std::string text(payload.begin(), payload.end());
    std::istringstream iss(text);
    std::string line;
    std::getline(iss, line); // remote node id (currently unused by the caller)

    std::vector<FileEntry> out;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        out.push_back(line_to_entry(line));
    }
    return out;
}

// --------------------------------------------------------------------
// Sync session: exchange manifests, merge via CRDT, fetch missing chunks.
// --------------------------------------------------------------------

static std::vector<uint8_t> serialize_hash_list(const std::vector<sha256::Digest>& hashes) {
    std::ostringstream oss;
    for (size_t i = 0; i < hashes.size(); ++i) {
        if (i) oss << "\n";
        oss << sha256::to_hex(hashes[i]);
    }
    std::string s = oss.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}

static std::vector<sha256::Digest> parse_hash_list(const std::vector<uint8_t>& payload) {
    std::string text(payload.begin(), payload.end());
    std::istringstream iss(text);
    std::string line;
    std::vector<sha256::Digest> out;
    while (std::getline(iss, line)) {
        if (!line.empty()) out.push_back(hex_to_digest(line));
    }
    return out;
}

SyncStats run_sync_session(TcpSocket& sock, SyncEngine& engine) {
    SyncStats stats;

    // 1. Exchange manifests.
    auto local_manifest = serialize_manifest(engine);
    stats.entries_sent = static_cast<int>(engine.store().all_entries().size());
    sock.send_frame(FRAME_MANIFEST, local_manifest);

    uint8_t type;
    std::vector<uint8_t> payload;
    if (!sock.recv_frame(type, payload) || type != FRAME_MANIFEST) {
        throw std::runtime_error("expected MANIFEST frame from peer");
    }
    auto remote_entries = parse_manifest(payload);

    // 2. Merge each remote entry into our CRDT store; collect chunk hashes
    //    we don't yet have locally for anything we ended up adopting.
    std::set<std::string> needed_hex;
    std::vector<std::string> materialize_paths;

    for (const auto& re : remote_entries) {
        MergeResult r = engine.store().merge_remote(re);
        switch (r.outcome) {
            case MergeOutcome::APPLIED_REMOTE:
                stats.entries_applied_remote++;
                for (const auto& h : re.chunk_hashes) {
                    if (!engine.blobs().has_chunk(h)) needed_hex.insert(sha256::to_hex(h));
                }
                materialize_paths.push_back(re.path);
                break;
            case MergeOutcome::CONFLICT_FORKED: {
                stats.conflicts_forked++;
                auto forked = engine.store().get(r.conflict_path);
                if (forked) {
                    for (const auto& h : forked->chunk_hashes) {
                        if (!engine.blobs().has_chunk(h)) needed_hex.insert(sha256::to_hex(h));
                    }
                    materialize_paths.push_back(r.conflict_path);
                }
                break;
            }
            case MergeOutcome::KEPT_LOCAL:
                stats.entries_kept_local++;
                break;
            case MergeOutcome::NO_CHANGE:
                stats.entries_no_change++;
                break;
        }
    }

    // 3. Request any chunks we're missing for the entries we just adopted.
    std::vector<sha256::Digest> needed;
    needed.reserve(needed_hex.size());
    for (const auto& h : needed_hex) needed.push_back(hex_to_digest(h));
    sock.send_frame(FRAME_CHUNK_REQUEST, serialize_hash_list(needed));

    // 4. Peer's incoming chunk request for what THEY are missing from US.
    if (!sock.recv_frame(type, payload) || type != FRAME_CHUNK_REQUEST) {
        throw std::runtime_error("expected CHUNK_REQUEST frame from peer");
    }
    auto their_needed = parse_hash_list(payload);

    std::vector<uint8_t> chunk_data_payload;
    for (const auto& h : their_needed) {
        if (!engine.blobs().has_chunk(h)) continue; // shouldn't happen; skip defensively
        auto bytes = engine.blobs().get_chunk(h);
        chunk_data_payload.insert(chunk_data_payload.end(), h.begin(), h.end());
        uint32_t len = static_cast<uint32_t>(bytes.size());
        uint8_t len_bytes[4] = {
            uint8_t((len >> 24) & 0xff), uint8_t((len >> 16) & 0xff),
            uint8_t((len >> 8) & 0xff), uint8_t(len & 0xff)
        };
        chunk_data_payload.insert(chunk_data_payload.end(), len_bytes, len_bytes + 4);
        chunk_data_payload.insert(chunk_data_payload.end(), bytes.begin(), bytes.end());
    }
    sock.send_frame(FRAME_CHUNK_DATA, chunk_data_payload);

    // 5. Receive the chunks we asked for and store them.
    if (!sock.recv_frame(type, payload) || type != FRAME_CHUNK_DATA) {
        throw std::runtime_error("expected CHUNK_DATA frame from peer");
    }
    size_t pos = 0;
    while (pos + 36 <= payload.size()) {
        sha256::Digest h{};
        std::copy(payload.begin() + static_cast<long>(pos),
                  payload.begin() + static_cast<long>(pos) + 32, h.begin());
        pos += 32;
        uint32_t len = (uint32_t(payload[pos]) << 24) | (uint32_t(payload[pos+1]) << 16) |
                       (uint32_t(payload[pos+2]) << 8) | uint32_t(payload[pos+3]);
        pos += 4;
        Chunk c;
        c.hash = h;
        c.offset = 0;
        c.data.assign(payload.begin() + static_cast<long>(pos),
                       payload.begin() + static_cast<long>(pos) + len);
        pos += len;
        engine.blobs().put_chunk(c);
        stats.chunks_fetched++;
    }

    // 6. Now that chunk bytes are local, materialize any adopted/forked
    //    files and persist the merged CRDT state so it survives restarts.
    for (const auto& p : materialize_paths) engine.materialize(p);
    engine.save_journal();

    sock.send_frame(FRAME_DONE, {});
    if (!sock.recv_frame(type, payload) || type != FRAME_DONE) {
        // Non-fatal: peer may have already closed after sending DONE.
    }

    return stats;
}
