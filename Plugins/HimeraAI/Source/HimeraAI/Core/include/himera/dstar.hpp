#pragma once
#include "himera/grid.hpp"
#include <cstdint>
#include <limits>
#include <queue>
#include <vector>

namespace himera {
class DStarLiteAgent {
public:
    DStarLiteAgent(Grid map, Point start, Point goal);
    void updateCell(Point p, bool blocked, double risk = 0);
    void moveStart(Point p);
    std::vector<Point> findPath();
    int lastExpansionCount() const { return lastExpansions_; }
    const Grid& grid() const { return grid_; }
private:
    struct Key { double first; double second; };
    struct Entry { Key key; int id; std::uint64_t version; };
    struct Greater { bool operator()(const Entry& a, const Entry& b) const; };
    static constexpr double INF = std::numeric_limits<double>::infinity();
    Grid grid_;
    Point start_, goal_;
    double km_ = 0;
    std::vector<double> g_, rhs_;
    std::vector<std::uint64_t> version_;
    std::vector<bool> inQueue_;
    std::priority_queue<Entry, std::vector<Entry>, Greater> queue_;
    int lastExpansions_ = 0;
    static bool less(Key a, Key b);
    Key calculateKey(int id) const;
    Key topKey();
    void push(int id);
    void discardInvalidTop();
    double edge(int from, int to) const;
    void updateVertex(int id);
    void computeShortestPath();
};
} // namespace himera
