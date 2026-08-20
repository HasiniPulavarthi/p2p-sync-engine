#pragma once
#include <string>
#include "file_entry.hpp"

// One FileEntry <-> one tab-separated text line. Used both for the
// on-disk metadata journal (so a node remembers its CRDT state across
// process restarts) and for the MANIFEST wire message (so two nodes can
// exchange that same state over the network) -- same format, two uses.
sha256::Digest hex_to_digest(const std::string& hex);
std::string clock_to_wire(const VectorClock& c);
VectorClock clock_from_wire(const std::string& s);

std::string entry_to_line(const FileEntry& e);
FileEntry line_to_entry(const std::string& line);
