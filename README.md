# P2P File Sync Engine with CRDT Conflict Resolution

A peer-to-peer file synchronization engine, in C++17, built around
CRDT-based metadata versioning across multiple nodes that may be
disconnected from each other for arbitrary lengths of time. When nodes
reconnect, changes merge without a central coordinator, and genuine
conflicts — edits made concurrently, while offline — are detected and
resolved losslessly instead of silently dropped.

This is the same core problem Dropbox and Syncthing solve internally, and
it's the part naive implementations skip: a "last edit wins" sync engine
either loses data on concurrent edits, or requires a central server to
serialize every write. Neither is true peer-to-peer conflict resolution.

## Architecture

Three algorithmic layers, each solving a distinct problem, composed by a
`SyncEngine` that never touches sockets, and a `network` layer that never
touches file-content logic:

```
 chunker.*        Content-defined chunking (gear rolling hash).
                   Splits file bytes into variable-length chunks so a
                   localized edit only changes the chunks touching it,
                   not everything after it (unlike fixed-size chunking).

 merkle_tree.*     Builds a Merkle tree over a file's chunk hashes.
                   Two files are identical iff their roots match (O(1)
                   check); when they differ, walking down only the
                   mismatched subtrees finds the *changed* chunks in
                   O(log n + changed) instead of diffing every chunk.

 vector_clock.*    Per-node logical clocks. Determines, for any two
                   versions of a file, whether one strictly happened
                   before the other (safe to auto-merge) or whether they
                   are *concurrent* (a genuine conflict — this is why
                   wall-clock "last write wins" is wrong for
                   disconnected peers: clocks drift and cannot express
                   causality, but vector clocks can).

 crdt_store.*      A state-based, observed-remove CRDT map from file path
                   to FileEntry (chunk hashes + vector clock + tombstone
                   flag). merge_remote() is total, deterministic, and
                   commutative: any two replicas that have observed the
                   same set of updates converge to identical state
                   regardless of merge order (strong eventual
                   consistency). Concurrent edits are never dropped —
                   they fork into a ".conflict-<node>" copy, mirroring
                   Dropbox/Syncthing's conflicted-copy convention.

 file_store.*      Content-addressed local blob storage: each chunk is
                   written once, named by its own hash, so identical
                   content (shared regions across versions, or across
                   different files) is never stored or transferred twice.

 sync_engine.*      Wires the above together for one local node: ingest a
                   file -> chunk -> hash -> diff-report -> store chunks
                   -> record a new CRDT version -> materialize to disk.
                   Also owns an on-disk metadata journal (store.meta) so
                   a node's CRDT state survives process restarts.

 network.*         Length-framed TCP protocol. Two peers exchange their
                   full CRDT manifests, merge remote entries locally,
                   request only the chunk bytes they're missing, and
                   materialize any newly adopted or forked files. Both
                   sides run the identical symmetric exchange — there is
                   no client/server distinction beyond who dialed.

 node.*            Public API: add_file, remove_file, list_files,
                   sync_with(host, port), listen_and_sync(port).

 main.cpp           CLI front-end (see Usage below).
```

## Design notes worth highlighting in an interview

- **Why content-defined chunking, not fixed-size blocks**: fixed-size
  chunking means inserting one byte at the start of a file shifts every
  chunk boundary after it, so a diff against the old version looks like
  the whole file changed. The gear rolling hash picks boundaries based on
  local content, so edits stay local. This is the same idea behind
  rsync's rolling checksum and restic/borg's chunking.
- **Why a Merkle tree on top of that**: chunk-by-chunk hash comparison is
  O(chunks). Comparing two roots is O(1); walking only the differing
  subtrees is O(log n + changed chunks). This is exactly why Git can tell
  two trees are identical without reading every blob, and why Dropbox's
  block-level sync doesn't re-hash unchanged regions.
- **Why vector clocks instead of timestamps**: wall-clock time across
  independently-clocked, occasionally-offline machines is not a reliable
  way to order events — clock skew can make an *older* edit look newer.
  Vector clocks capture actual causal history, so `BEFORE`/`AFTER` are
  provably correct, and `CONCURRENT` is only ever reported for edits that
  really did happen independently.
- **Why fork-on-conflict instead of a 3-way merge**: for structured text
  formats a real content merge is possible; for arbitrary binary files it
  generally isn't. The engine is conservative and total: it never
  guesses, it always preserves both versions, and users reconcile
  conflict copies manually — the same trade-off Dropbox and Syncthing
  make.
- **Known simplification**: the sync protocol sends full manifests and
  waits on ordered `send`/`recv` pairs rather than interleaving chunk
  transfer asynchronously. For very large repositories, a production
  version would stream chunk requests/responses concurrently to avoid
  socket-buffer-bound head-of-line blocking — noted here rather than
  hidden.

## Building

Requires a C++17 compiler and POSIX sockets (Linux/macOS). No external
dependencies.

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
ctest --output-on-failure   # runs the chunker, Merkle tree, and CRDT test suites
```

(If `cmake` isn't available, every source file compiles standalone with
plain `g++ -std=c++17`; see `build_manual.sh` for the equivalent flags.)

## Usage

Every node has its own `--id` and local `--data` directory:

```bash
# Node A ingests a file
./syncnode --id A --data ./dataA add notes.txt ./notes.txt

# Node A waits for a peer to connect
./syncnode --id A --data ./dataA listen --port 9101

# In another terminal: node B connects to A and syncs
./syncnode --id B --data ./dataB sync --host 127.0.0.1 --port 9101

# Inspect what a node currently knows
./syncnode --id B --data ./dataB list
```

### Demonstrating conflict resolution

```bash
# 1. Sync a base version to both nodes (as above).
# 2. Simulate going offline: edit the SAME logical file differently on each side.
./syncnode --id A --data ./dataA add notes.txt ./edited-by-A.txt
./syncnode --id B --data ./dataB add notes.txt ./edited-by-B.txt

# 3. Reconnect.
./syncnode --id A --data ./dataA listen --port 9102
./syncnode --id B --data ./dataB sync --host 127.0.0.1 --port 9102
```

Both nodes end up with `notes.txt` (their own edit, unchanged) plus
`notes.conflict-<other node>.txt` holding the other side's edit — both
byte-identical to what was actually written, verified in this project's
own test run: no data was lost or overwritten.

## Layout

```
CMakeLists.txt
include/            all public headers
src/                implementation + main.cpp CLI entry point
tests/              standalone assert-based test executables (no framework)
```
