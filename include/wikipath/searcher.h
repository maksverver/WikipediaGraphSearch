#ifndef WIKIPATH_SEARCHER_H_INCLUDED
#define WIKIPATH_SEARCHER_H_INCLUDED

#include "common.h"
#include "graph-reader.h"

#include <vector>

namespace wikipath {

struct SearchStats {
    int64_t vertices_reached = 0;
    int64_t vertices_expanded = 0;
    int64_t edges_expanded = 0;
    int64_t time_taken_ms = 0;
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

#endif  // ndef WIKIPATH_SEARCHER_H_INCLUDED
