#include "entry_codec.hpp"
#include <sstream>

sha256::Digest hex_to_digest(const std::string& hex) {
    sha256::Digest d{};
    for (size_t i = 0; i < 32 && i * 2 + 1 < hex.size(); ++i) {
        d[i] = static_cast<uint8_t>(std::stoi(hex.substr(i * 2, 2), nullptr, 16));
    }
    return d;
}

std::string clock_to_wire(const VectorClock& c) {
    std::ostringstream oss;
    bool first = true;
    for (const auto& [node, val] : c.entries()) {
        if (!first) oss << ",";
        oss << node << ":" << val;
        first = false;
    }
    return oss.str();
}

VectorClock clock_from_wire(const std::string& s) {
    VectorClock c;
    if (s.empty()) return c;
    std::stringstream ss(s);
    std::string pair;
    while (std::getline(ss, pair, ',')) {
        auto colon = pair.find(':');
        if (colon == std::string::npos) continue;
        std::string node = pair.substr(0, colon);
        uint64_t val = std::stoull(pair.substr(colon + 1));
        c.set(node, val);
    }
    return c;
}

std::string entry_to_line(const FileEntry& e) {
    std::ostringstream oss;
    oss << e.path << "\t"
        << (e.deleted ? "1" : "0") << "\t"
        << e.size << "\t"
        << sha256::to_hex(e.merkle_root) << "\t"
        << clock_to_wire(e.clock) << "\t"
        << e.last_writer << "\t";
    for (size_t i = 0; i < e.chunk_hashes.size(); ++i) {
        if (i) oss << ",";
        oss << sha256::to_hex(e.chunk_hashes[i]);
    }
    return oss.str();
}

FileEntry line_to_entry(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ls(line);
    std::string field;
    while (std::getline(ls, field, '\t')) fields.push_back(field);
    while (fields.size() < 7) fields.push_back("");

    FileEntry e;
    e.path = fields[0];
    e.deleted = (fields[1] == "1");
    e.size = fields[2].empty() ? 0 : std::stoull(fields[2]);
    e.merkle_root = hex_to_digest(fields[3]);
    e.clock = clock_from_wire(fields[4]);
    e.last_writer = fields[5];

    if (!fields[6].empty()) {
        std::stringstream hs(fields[6]);
        std::string h;
        while (std::getline(hs, h, ',')) {
            if (!h.empty()) e.chunk_hashes.push_back(hex_to_digest(h));
        }
    }
    return e;
}
