// Fresh A* baseline for comparison with D* Lite. Test code only, not part of the core.
// Uses exactly the same cost rule as the core: entering a walkable cell q costs 1 + risk(q).
#pragma once
#include "himera/grid.hpp"
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <vector>

namespace trooper_test {

struct AStarResult {
    std::vector<himera::Point> path;  // empty when unreachable
    double cost = 0;
    int expanded = 0;
};

inline AStarResult astar(const himera::Grid& grid, himera::Point start, himera::Point goal) {
    using himera::Point;
    AStarResult result;
    if (!grid.walkable(start) || !grid.walkable(goal)) return result;
    const double INF = std::numeric_limits<double>::infinity();
    std::vector<double> g(grid.count(), INF);
    std::vector<int> parent(grid.count(), -1);
    std::vector<bool> closed(grid.count(), false);
    struct Node { double f; int id; };
    auto cmp = [](const Node& a, const Node& b) { return a.f > b.f; };
    std::priority_queue<Node, std::vector<Node>, decltype(cmp)> open(cmp);
    const int s = grid.id(start), t = grid.id(goal);
    g[s] = 0;
    open.push({double(himera::manhattan(start, goal)), s});
    while (!open.empty()) {
        Node n = open.top(); open.pop();
        if (closed[n.id]) continue;
        closed[n.id] = true;
        ++result.expanded;
        if (n.id == t) break;
        Point at = grid.point(n.id);
        for (Point q : grid.neighbors(at)) {
            const int qi = grid.id(q);
            const double next = g[n.id] + 1.0 + grid.risk(q);
            if (next < g[qi]) {
                g[qi] = next;
                parent[qi] = n.id;
                open.push({next + himera::manhattan(q, goal), qi});
            }
        }
    }
    if (!std::isfinite(g[t])) return result;
    result.cost = g[t];
    for (int at = t; at != -1; at = parent[at]) result.path.push_back(grid.point(at));
    std::reverse(result.path.begin(), result.path.end());
    return result;
}

// A path is valid when it starts at start, ends at goal, every step moves to a walkable
// 4-neighbour. Returns the path cost, or a negative number when invalid.
inline double validatePath(const himera::Grid& grid, const std::vector<himera::Point>& path,
                           himera::Point start, himera::Point goal) {
    if (path.empty() || !(path.front() == start) || !(path.back() == goal)) return -1;
    double cost = 0;
    for (size_t i = 1; i < path.size(); ++i) {
        if (himera::manhattan(path[i - 1], path[i]) != 1 || !grid.walkable(path[i])) return -1;
        cost += 1.0 + grid.risk(path[i]);
    }
    return cost;
}

}  // namespace trooper_test
