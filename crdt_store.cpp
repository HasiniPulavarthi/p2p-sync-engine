#include "crdt_store.hpp"

CrdtStore::CrdtStore(std::string node_id) : node_id_(std::move(node_id)) {}

void CrdtStore::put_local(const std::string& path,
                           std::vector<sha256::Digest> chunk_hashes,
                           const sha256::Digest& merkle_root,
                           uint64_t size) {
    FileEntry entry;
    auto it = entries_.find(path);
    if (it != entries_.end()) entry.clock = it->second.clock;

    entry.path = path;
    entry.chunk_hashes = std::move(chunk_hashes);
    entry.merkle_root = merkle_root;
    entry.size = size;
    entry.deleted = false;
    entry.last_writer = node_id_;
    entry.clock.increment(node_id_);

    entries_[path] = std::move(entry);
}

void CrdtStore::delete_local(const std::string& path) {
    auto it = entries_.find(path);
    FileEntry entry;
    if (it != entries_.end()) entry = it->second;
    entry.path = path;
    entry.deleted = true;
    entry.chunk_hashes.clear();
    entry.last_writer = node_id_;
    entry.clock.increment(node_id_);
    entries_[path] = std::move(entry);
}

std::string CrdtStore::make_conflict_path(const std::string& path, const std::string& other_node) const {
    // e.g. "notes.txt" -> "notes.conflict-nodeB.txt", mirroring the
    // human-visible conflict-copy convention used by Dropbox/Syncthing so
    // a user can see and manually reconcile forked versions.
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return path + ".conflict-" + other_node;
    }
    return path.substr(0, dot) + ".conflict-" + other_node + path.substr(dot);
}

MergeResult CrdtStore::merge_remote(const FileEntry& remote) {
    MergeResult result;
    result.path = remote.path;

    auto it = entries_.find(remote.path);
    if (it == entries_.end()) {
        // We have never seen this path -> trivially adopt it.
        entries_[remote.path] = remote;
        result.outcome = MergeOutcome::APPLIED_REMOTE;
        return result;
    }

    FileEntry& local = it->second;
    ClockOrder order = VectorClock::compare(local.clock, remote.clock);

    switch (order) {
        case ClockOrder::EQUAL:
            result.outcome = MergeOutcome::NO_CHANGE;
            return result;

        case ClockOrder::BEFORE: {
            // Local causally precedes remote -> remote strictly dominates.
            local = remote;
            result.outcome = MergeOutcome::APPLIED_REMOTE;
            return result;
        }

        case ClockOrder::AFTER:
            // Local already dominates remote -> nothing to do.
            result.outcome = MergeOutcome::KEPT_LOCAL;
            return result;

        case ClockOrder::CONCURRENT: {
            // Real conflict: both sides edited independently while
            // disconnected. Resolve deterministically and losslessly by
            // keeping local in place and forking remote to a conflict
            // path. Both replicas that run this merge (regardless of
            // which direction sync happened) reach the same two entries,
            // which is what makes this conflict-free in the CRDT sense.
            std::string conflict_path = make_conflict_path(remote.path, remote.last_writer);
            FileEntry forked = remote;
            forked.path = conflict_path;
            entries_[conflict_path] = std::move(forked);

            // Local's clock absorbs remote's counters so future comparisons
            // against this path correctly treat the conflict as observed.
            local.clock.merge(remote.clock);

            result.outcome = MergeOutcome::CONFLICT_FORKED;
            result.conflict_path = conflict_path;
            return result;
        }
    }
    result.outcome = MergeOutcome::NO_CHANGE;
    return result;
}

void CrdtStore::load_entry(const FileEntry& e) {
    entries_[e.path] = e;
}

std::optional<FileEntry> CrdtStore::get(const std::string& path) const {
    auto it = entries_.find(path);
    if (it == entries_.end()) return std::nullopt;
    return it->second;
}

std::vector<FileEntry> CrdtStore::all_entries() const {
    std::vector<FileEntry> out;
    out.reserve(entries_.size());
    for (const auto& [_, e] : entries_) out.push_back(e);
    return out;
}
