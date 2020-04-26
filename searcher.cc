#include "searcher.h"

#include <assert.h>

#include <algorithm>
#include <chrono>
#include <vector>

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

} // namespace

std::vector<index_t> FindShortestPath(const GraphReader &graph, index_t start, index_t finish, SearchStats *stats) {
    return stats == nullptr ?
            FindShortestPathImpl(graph, start, finish, DummyStatsCollector()) :
            FindShortestPathImpl(graph, start, finish, RealStatsCollector(*stats));
}
