#pragma once
#include <map>
#include <string>
#include <cstdint>

enum class ClockOrder { EQUAL, BEFORE, AFTER, CONCURRENT };

// Standard vector clock: one counter per node. Lets us tell, for any two
// versions of the same file, whether one strictly happened-before the
// other (safe to auto-resolve: newer wins) or whether they were made
// *concurrently* on disconnected nodes (a true conflict that needs
// CRDT-style merge or a conflict copy — this is the crux of the whole
// project: naive "last write wins by timestamp" is wrong here because
// wall-clock time across disconnected machines isn't trustworthy, but
// causal order via vector clocks is).
class VectorClock {
public:
    void increment(const std::string& node_id);
    void merge(const VectorClock& other); // component-wise max, used after sync

    uint64_t get(const std::string& node_id) const;
    void set(const std::string& node_id, uint64_t value);

    static ClockOrder compare(const VectorClock& a, const VectorClock& b);

    const std::map<std::string, uint64_t>& entries() const { return counters_; }

    std::string to_string() const;

private:
    std::map<std::string, uint64_t> counters_;
};
