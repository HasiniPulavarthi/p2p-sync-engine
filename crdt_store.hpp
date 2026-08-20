#pragma once
#include <map>
#include <string>
#include <vector>
#include <optional>
#include "file_entry.hpp"

enum class MergeOutcome { APPLIED_REMOTE, KEPT_LOCAL, NO_CHANGE, CONFLICT_FORKED };

struct MergeResult {
    MergeOutcome outcome;
    std::string path;              // path that was merged
    std::string conflict_path;     // populated only when outcome == CONFLICT_FORKED
};

// The CRDT itself: a state-based, observed-remove map from file path to
// FileEntry, where entries are ordered by vector clock rather than wall
// clock time. Two replicas that have each merged the same set of updates
// (in ANY order — that's the "conflict-free" part) converge to identical
// state. This gives strong eventual consistency across nodes that may be
// offline for arbitrary lengths of time.
//
// Merge rule per path:
//   - remote clock happened-before local clock  -> keep local, drop remote
//   - remote clock happened-after local clock   -> adopt remote (incl. tombstones)
//   - clocks equal                              -> no-op (already synced)
//   - clocks concurrent                         -> genuine conflict: since we
//        can't semantically merge arbitrary binary file content, we keep
//        BOTH versions (matching Dropbox/Syncthing's "conflicted copy"
//        convention) by forking the incoming version to a new path. This
//        keeps the merge function total and deterministic -- no data is
//        ever silently lost.
class CrdtStore {
public:
    explicit CrdtStore(std::string node_id);

    // Local mutation: bump this node's clock component and record the entry.
    void put_local(const std::string& path,
                    std::vector<sha256::Digest> chunk_hashes,
                    const sha256::Digest& merkle_root,
                    uint64_t size);
    void delete_local(const std::string& path);

    // Remote mutation: merge an entry learned from a peer during sync.
    MergeResult merge_remote(const FileEntry& remote);

    // Directly installs an entry with no clock bump and no merge logic.
    // Used only to restore state that was already ours (on-disk journal
    // replay at startup) -- never for data learned from a peer.
    void load_entry(const FileEntry& e);

    std::optional<FileEntry> get(const std::string& path) const;
    std::vector<FileEntry> all_entries() const;
    const std::string& node_id() const { return node_id_; }

private:
    std::string make_conflict_path(const std::string& path, const std::string& other_node) const;

    std::string node_id_;
    std::map<std::string, FileEntry> entries_;
};
