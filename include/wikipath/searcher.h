#ifndef WIKIPATH_SEARCHER_H_INCLUDED
#define WIKIPATH_SEARCHER_H_INCLUDED

#include "common.h"
#include "graph-reader.h"

#include <optional>
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

// Finds a single shortest path from `start` to `finish` using bidirectional
// breadth-first search.
//
// Returns the path as vector indices, including start and finish, or an
// empty vector if no path exists.
//
// If `stats` is not null, search statistics are written to *stats.
std::vector<index_t> FindShortestPath(
    const GraphReader &graph, index_t start, index_t finish, SearchStats *stats);

// Finds all shortest paths from `start` to `finish` using bidirectional
// breadth-first search, and returns the result as a DAG, represented as a
// sorted list of (source, destination) pairs where `start` is one of the
// sources and `finish` is one of the destinations.
//
// If no path is found, an empty optional is returned instead. Note that the
// optional contains an empty vector only if `start` == `finish`.
//
// Every path through the DAG from `start` to `finish` has the same length, so
// the DAG consists of layers corresponding with distances from the start/to the
// finish, and only edges between consecutive layers are possible.
//
// If `stats` is not null, search statistics are written to *stats.
std::optional<std::vector<std::pair<index_t, index_t>>>
FindShortestPathDag(
    const GraphReader &graph, index_t start, index_t finish, SearchStats *stats);

}  // namespace wikipath

// std::hash<> implementation. Only needed for the Python bindings.
template<> struct std::hash<wikipath::SearchStats> {
  size_t operator()(const wikipath::SearchStats& stats) const noexcept {
    return stats.Hash();
  }
};

#endif  // ndef WIKIPATH_SEARCHER_H_INCLUDED
