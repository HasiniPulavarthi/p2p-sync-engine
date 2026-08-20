#include "file_store.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

FileStore::FileStore(std::string root_dir) : root_(std::move(root_dir)) {
    fs::create_directories(root_ + "/chunks");
}

std::string FileStore::chunk_path(const sha256::Digest& hash) const {
    return root_ + "/chunks/" + sha256::to_hex(hash) + ".chunk";
}

void FileStore::put_chunk(const Chunk& c) const {
    std::string p = chunk_path(c.hash);
    if (fs::exists(p)) return; // content-addressed: already have it, dedup
    std::ofstream out(p, std::ios::binary);
    out.write(reinterpret_cast<const char*>(c.data.data()),
              static_cast<std::streamsize>(c.data.size()));
}

bool FileStore::has_chunk(const sha256::Digest& hash) const {
    return fs::exists(chunk_path(hash));
}

std::vector<uint8_t> FileStore::get_chunk(const sha256::Digest& hash) const {
    std::ifstream in(chunk_path(hash), std::ios::binary);
    if (!in) throw std::runtime_error("missing chunk: " + sha256::to_hex(hash));
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
}

std::vector<uint8_t> FileStore::reassemble(const std::vector<sha256::Digest>& hashes) const {
    std::vector<uint8_t> out;
    for (const auto& h : hashes) {
        auto bytes = get_chunk(h);
        out.insert(out.end(), bytes.begin(), bytes.end());
    }
    return out;
}

void FileStore::materialize(const std::string& materialized_dir,
                             const std::string& relative_path,
                             const std::vector<sha256::Digest>& hashes) const {
    fs::path full = fs::path(materialized_dir) / relative_path;
    fs::create_directories(full.parent_path());
    auto bytes = reassemble(hashes);
    std::ofstream out(full, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}
