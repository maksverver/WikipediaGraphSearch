#ifndef WIKIPATH_SEARCHER_H_INCLUDED
#define WIKIPATH_SEARCHER_H_INCLUDED

#include "common.h"
#include "graph-reader.h"

#include <utility>
#include <vector>

namespace wikipath {

struct SearchStats {
    int64_t vertices_reached = 0;
    int64_t vertices_expanded = 0;
    int64_t edges_expanded = 0;
    int64_t time_taken_ms = 0;

    bool operator==(const SearchStats&) const = default;

    // Hash code. Only used by Python bindings.
    size_t Hash() const noexcept {
        size_t hash = 0;
        hash = 8191*hash + vertices_reached;
        hash = 8191*hash + vertices_expanded;
        hash = 8191*hash + edges_expanded;
        hash = 8191*hash + time_taken_ms;
        return hash;
    }
};

// Finds a shortest path from `start` to `finish` using bidirectional
// breadth-first search.
//
// Returns the path as vector indices, including start and finish, or an
// empty vector if no path exists.
//
// If `stats` is not null, statistics are written to *stats.
std::vector<index_t> FindShortestPath(const GraphReader &graph, index_t start, index_t finish, SearchStats *stats);

}  // namespace wikipath

// std::hash<> implementation. Only needed for the Python bindings.
template<> struct std::hash<wikipath::SearchStats> {
  size_t operator()(const wikipath::SearchStats& stats) const noexcept {
    return stats.Hash();
  }
};

#endif  // ndef WIKIPATH_SEARCHER_H_INCLUDED
