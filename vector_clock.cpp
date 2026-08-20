#include "vector_clock.hpp"
#include <sstream>
#include <algorithm>

void VectorClock::increment(const std::string& node_id) {
    counters_[node_id] += 1;
}

void VectorClock::merge(const VectorClock& other) {
    for (const auto& [node, val] : other.counters_) {
        auto it = counters_.find(node);
        if (it == counters_.end() || it->second < val) {
            counters_[node] = val;
        }
    }
}

uint64_t VectorClock::get(const std::string& node_id) const {
    auto it = counters_.find(node_id);
    return it == counters_.end() ? 0 : it->second;
}

void VectorClock::set(const std::string& node_id, uint64_t value) {
    counters_[node_id] = value;
}

ClockOrder VectorClock::compare(const VectorClock& a, const VectorClock& b) {
    bool a_less = false; // a has some component strictly less than b
    bool b_less = false; // b has some component strictly less than a

    // Union of node ids appearing in either clock.
    std::map<std::string, bool> seen;
    for (const auto& [n, _] : a.counters_) seen[n] = true;
    for (const auto& [n, _] : b.counters_) seen[n] = true;

    for (const auto& [n, _] : seen) {
        uint64_t av = a.get(n), bv = b.get(n);
        if (av < bv) a_less = true;
        if (bv < av) b_less = true;
    }

    if (!a_less && !b_less) return ClockOrder::EQUAL;
    if (a_less && !b_less) return ClockOrder::BEFORE;   // a happened-before b
    if (b_less && !a_less) return ClockOrder::AFTER;    // a happened-after b
    return ClockOrder::CONCURRENT;                      // neither dominates -> true conflict
}

std::string VectorClock::to_string() const {
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& [n, v] : counters_) {
        if (!first) oss << ", ";
        oss << n << ":" << v;
        first = false;
    }
    oss << "}";
    return oss.str();
}
