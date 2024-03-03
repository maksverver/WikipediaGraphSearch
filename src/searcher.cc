#include "wikipath/searcher.h"

#include <assert.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <ranges>
#include <vector>

namespace wikipath {
namespace {

class DummyStatsCollector {
public:
    void VertexReached() {}
    void VertexExpanded() {}
    void EdgeExpanded() {}
};

class RealStatsCollector {
public:
    RealStatsCollector(SearchStats &stats)
            : stats(stats), start_time(std::chrono::steady_clock::now()) {}

    ~RealStatsCollector() {
        auto duration = std::chrono::steady_clock::now() - start_time;
        stats = {
            .vertices_reached = vertices_reached,
            .vertices_expanded = vertices_expanded,
            .edges_expanded = edges_expanded,
            .time_taken_ms = duration / std::chrono::milliseconds(1),
        };
    }

    void VertexReached() { ++vertices_reached; }
    void VertexExpanded() { ++vertices_expanded; }
    void EdgeExpanded() { ++edges_expanded; }

    // Not copyable or assignable.
    RealStatsCollector(const RealStatsCollector&) = delete;
    RealStatsCollector &operator=(const RealStatsCollector&) = delete;

private:
    SearchStats &stats;
    int64_t vertices_reached = 0;
    int64_t vertices_expanded = 0;
    int64_t edges_expanded = 0;
    std::chrono::time_point<std::chrono::steady_clock> start_time;
};

template<class StatsCollectorT>
std::vector<index_t> FindShortestPathImpl(const GraphReader &graph, index_t start, index_t finish, StatsCollectorT stats_collector) {
    const index_t size = graph.VertexCount();
    assert(~size > size);
    assert(start < size);
    assert(finish < size);

    if (start == finish) {
        stats_collector.VertexReached();
        return {start};
    }

    // For each vertex, visited[v] can be:
    //
    //  0 if vertex is unvisited
    //  1 < v < size: if vertex was reachable via a forward edge from `v`
    //  1 < ~v < size: if vertex was reachable via a backward edge from `v`
    std::vector<index_t> visited(size, 0);

    // Reconstructs the path from start to finish, assuming there is an edge
    // (i, j), a forward path from `start` to `i`, and a backward path from
    // `j` to `finish`.
    auto ReconstructPath = [start, finish, &visited](index_t i, index_t j) {
        std::vector<index_t> path;
        while (i != start) {
            path.push_back(i);
            i = visited[i];
        }
        path.push_back(start);
        std::reverse(path.begin(), path.end());
        while (j != finish) {
            path.push_back(j);
            j = ~visited[j];
        }
        path.push_back(finish);
        return path;
    };

    std::vector<index_t> forward_fringe;
    std::vector<index_t> backward_fringe;
    visited[start] = start;
    visited[finish] = ~finish;
    forward_fringe.push_back(start);
    backward_fringe.push_back(finish);
    stats_collector.VertexReached();
    stats_collector.VertexReached();
    while (!forward_fringe.empty() && !backward_fringe.empty()) {
        if (forward_fringe.size() <= backward_fringe.size()) {
            // Expand forward fringe.
            std::vector<index_t> new_fringe;
            for (index_t i : forward_fringe) {
                stats_collector.VertexExpanded();
                for (index_t j : graph.ForwardEdges(i)) {
                    stats_collector.EdgeExpanded();
                    if (visited[j] == 0) {
                        stats_collector.VertexReached();
                        visited[j] = i;
                        new_fringe.push_back(j);
                    } else if (~visited[j] < size) {
                        return ReconstructPath(i, j);  // path found!
                    } else {
                        assert(visited[j] < size);
                    }
                }
            }
            forward_fringe.swap(new_fringe);
        } else {
            // Expand backward fringe.
            std::vector<index_t> new_fringe;
            for (index_t j : backward_fringe) {
                stats_collector.VertexExpanded();
                for (index_t i : graph.BackwardEdges(j)) {
                    stats_collector.EdgeExpanded();
                    if (visited[i] == 0) {
                        stats_collector.VertexReached();
                        visited[i] = ~j;
                        new_fringe.push_back(i);
                    } else if (visited[i] < size) {
                        return ReconstructPath(i, j);  // path found!
                    } else {
                        assert(~visited[i] < size);
                    }
                }
            }
            backward_fringe.swap(new_fringe);
        }
    }
    return {};  // no path found!
}

template<class StatsCollectorT, class DistT = uint8_t>
std::optional<std::vector<std::pair<index_t, index_t>>>
FindShortestPathDagImpl(const GraphReader &graph, index_t start, index_t finish, StatsCollectorT stats_collector) {
    // List of all edges that occur on a shortest path from `start` to `finish`.
    std::vector<std::pair<index_t, index_t>> edges;

    if (start == finish) {
        stats_collector.VertexReached();
        return edges;
    }

    // We use an 8-bit integer to represent distances, with start at 1 and
    // finish at 255. This means we can only the shortest paths that consist of
    // at most 254 edges. It's trivial to raise this restriction by chosing a
    // bigger integer type here, but in real-world graphs it does not seem
    // necessary because most paths are relatively short (think: less than 20).
    using dist_t = std::uint_least8_t;

    // If dist[v] == 0, distance to v is not known.
    // If dist[v] >= 1, then dist[v] - 1 is the distance from start to finish.
    std::vector<dist_t> dist(graph.VertexCount(), 0);

    // List of vertices that occur on a shortest path, which must have their
    // predecessors/successors added to the DAG.
    std::vector<index_t> propagate_forward, propagate_backward;

    // marked[v] is true iff. v == start or v == finish or v is an element of
    // propagate_forward or propagate_backward.
    std::vector<bool> marked(graph.VertexCount());
    marked[start] = true;
    marked[finish] = true;

    // Bidirectional search to find distances and initial edges.
    {
        std::vector<index_t> forward_fringe;
        std::vector<index_t> backward_fringe;
        dist_t forward_dist = 1;
        dist_t backward_dist = std::numeric_limits<dist_t>::max();
        dist[start]  = forward_dist;
        dist[finish] = backward_dist;
        forward_fringe.push_back(start);
        backward_fringe.push_back(finish);
        stats_collector.VertexReached();
        stats_collector.VertexReached();
        while (propagate_forward.empty() && propagate_backward.empty()) {
            if (backward_dist - forward_dist < 2) {
                // This happens when the path length is greater than fits in dist_t.
                // This shouldn't happen in real-world graphs, where the maximum
                // path length is relatively small (under the maximum of 254).
                std::cerr << "WARNING: path length too great!\n";
                return {};
            }
            if (forward_fringe.empty() || backward_fringe.empty()) {
                // No path exists.
                return {};
            }
            if (forward_fringe.size() <= backward_fringe.size()) {
                // Expand forward fringe.
                ++forward_dist;
                std::vector<index_t> new_fringe;
                for (index_t v : forward_fringe) {
                    stats_collector.VertexExpanded();
                    assert(dist[v] == forward_dist - 1);
                    for (index_t w : graph.ForwardEdges(v)) {
                        stats_collector.EdgeExpanded();
                        if (dist[w] == 0) {
                            // Vertex w is an unvisted successor of v.
                            stats_collector.VertexReached();
                            dist[w] = forward_dist;
                            if (propagate_backward.empty()) new_fringe.push_back(w);
                        } else if (forward_dist < dist[w]) {
                            // There is a minimum path containing edge v->w.
                            edges.push_back({v, w});
                            if (!marked[v]) {
                                marked[v] = true;
                                propagate_backward.push_back(v);
                            }
                            if (!marked[w]) {
                                marked[w] = true;
                                propagate_forward.push_back(w);
                            }
                        } else {
                            assert(dist[w] <= forward_dist);
                        }
                    }
                }
                forward_fringe.swap(new_fringe);
            } else {
                // Expand backward fringe.
                --backward_dist;
                std::vector<index_t> new_fringe;
                for (index_t w : backward_fringe) {
                    assert(dist[w] == backward_dist + 1);
                    stats_collector.VertexExpanded();
                    for (index_t v : graph.BackwardEdges(w)) {
                        stats_collector.EdgeExpanded();
                        if (dist[v] == 0) {
                            // Vertex v is an unvisted predecessor of w.
                            stats_collector.VertexReached();
                            dist[v] = backward_dist;
                            if (propagate_forward.empty()) new_fringe.push_back(v);
                        } else if (dist[v] < backward_dist) {
                            // There is a minimum path containing edge v->w.
                            if (!marked[w]) {
                                marked[w] = true;
                                propagate_forward.push_back(w);
                            }
                            if (!marked[v]) {
                                marked[v] = true;
                                propagate_backward.push_back(v);
                            }
                            edges.push_back({v, w});
                        } else {
                            assert(dist[v] >= backward_dist);
                        }
                    }
                }
                backward_fringe.swap(new_fringe);
            }
        }
    }

    for (size_t i = 0; i < propagate_backward.size(); ++i) {
        index_t w = propagate_backward[i];
        for (index_t v : graph.BackwardEdges(w)) {
            if (dist[v] + 1 == dist[w]) {
                edges.push_back({v, w});
                if (!marked[v]) {
                    marked[v] = true;
                    propagate_backward.push_back(v);
                }
            }
        }
    }

    for (size_t i = 0; i < propagate_forward.size(); ++i) {
        index_t v = propagate_forward[i];
        for (index_t w : graph.ForwardEdges(v)) {
            if (dist[v] + 1 == dist[w]) {
                edges.push_back({v, w});
                if (!marked[w]) {
                    marked[w] = true;
                    propagate_forward.push_back(w);
                }
            }
        }
    }

    std::ranges::sort(edges);

    return edges;
}

} // namespace

std::vector<index_t> FindShortestPath(const GraphReader &graph, index_t start, index_t finish, SearchStats *stats) {
    return stats == nullptr ?
            FindShortestPathImpl(graph, start, finish, DummyStatsCollector()) :
            FindShortestPathImpl(graph, start, finish, RealStatsCollector(*stats));
}

std::optional<std::vector<std::pair<index_t, index_t>>>
FindShortestPathDag(const GraphReader &graph, index_t start, index_t finish, SearchStats *stats) {
    return stats == nullptr ?
            FindShortestPathDagImpl(graph, start, finish, DummyStatsCollector()) :
            FindShortestPathDagImpl(graph, start, finish, RealStatsCollector(*stats));
}

}  // namespace wikipath
