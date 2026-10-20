#include "himera/dstar.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace himera {
namespace { constexpr double EPS = 1e-9; }
bool DStarLiteAgent::less(Key a, Key b) {
    return a.first < b.first - EPS || (std::abs(a.first - b.first) <= EPS && a.second < b.second - EPS);
}
bool DStarLiteAgent::Greater::operator()(const Entry& a, const Entry& b) const {
    if (a.key.first != b.key.first) return a.key.first > b.key.first;
    if (a.key.second != b.key.second) return a.key.second > b.key.second;
    return a.id > b.id;
}
DStarLiteAgent::DStarLiteAgent(Grid map, Point start, Point goal)
    : grid_(std::move(map)), start_(start), goal_(goal),
      g_(grid_.count(), INF), rhs_(grid_.count(), INF),
      version_(grid_.count(), 0), inQueue_(grid_.count(), false) {
    if (!grid_.walkable(start_) || !grid_.walkable(goal_))
        throw std::invalid_argument("Start and goal must be walkable");
    rhs_[grid_.id(goal_)] = 0;
    push(grid_.id(goal_));
}
DStarLiteAgent::Key DStarLiteAgent::calculateKey(int i) const {
    double v = std::min(g_[i], rhs_[i]);
    return {v + manhattan(start_, grid_.point(i)) + km_, v};
}
void DStarLiteAgent::push(int i) {
    ++version_[i];
    inQueue_[i] = true;
    queue_.push({calculateKey(i), i, version_[i]});
}
void DStarLiteAgent::discardInvalidTop() {
    while (!queue_.empty() && (!inQueue_[queue_.top().id] ||
             queue_.top().version != version_[queue_.top().id])) queue_.pop();
}
DStarLiteAgent::Key DStarLiteAgent::topKey() {
    discardInvalidTop();
    return queue_.empty() ? Key{INF, INF} : queue_.top().key;
}
double DStarLiteAgent::edge(int a, int b) const {
    Point p = grid_.point(a), q = grid_.point(b);
    if (manhattan(p, q) != 1 || !grid_.walkable(p) || !grid_.walkable(q)) return INF;
    return 1.0 + grid_.risk(q);
}
void DStarLiteAgent::updateVertex(int i) {
    if (i != grid_.id(goal_)) {
        rhs_[i] = INF;
        for (Point neighbor : grid_.neighbors(grid_.point(i), false)) {
            int j = grid_.id(neighbor);
            rhs_[i] = std::min(rhs_[i], edge(i, j) + g_[j]);
        }
    }
    if (inQueue_[i]) { inQueue_[i] = false; ++version_[i]; }
    if (std::abs(g_[i] - rhs_[i]) > EPS && !(std::isinf(g_[i]) && std::isinf(rhs_[i]))) push(i);
}
void DStarLiteAgent::computeShortestPath() {
    lastExpansions_ = 0;
    int startId = grid_.id(start_);
    const int guard = std::max(10000, grid_.count() * grid_.count() * 16);
    while (less(topKey(), calculateKey(startId)) || std::abs(rhs_[startId] - g_[startId]) > EPS) {
        if (++lastExpansions_ > guard) throw std::runtime_error("D* Lite iteration guard reached");
        discardInvalidTop();
        if (queue_.empty()) break;
        Entry current = queue_.top(); queue_.pop();
        int u = current.id;
        inQueue_[u] = false;
        Key updated = calculateKey(u);
        if (less(current.key, updated)) {
            push(u);
        } else if (g_[u] > rhs_[u]) {
            g_[u] = rhs_[u];
            for (Point p : grid_.neighbors(grid_.point(u), false)) updateVertex(grid_.id(p));
        } else {
            g_[u] = INF;
            updateVertex(u);
            for (Point p : grid_.neighbors(grid_.point(u), false)) updateVertex(grid_.id(p));
        }
    }
}
void DStarLiteAgent::updateCell(Point p, bool blocked, double risk) {
    grid_.setRisk(p, risk);
    grid_.setBlocked(p, blocked);
    updateVertex(grid_.id(p));
    for (Point neighbor : grid_.neighbors(p, false)) updateVertex(grid_.id(neighbor));
}
void DStarLiteAgent::moveStart(Point p) {
    if (!grid_.walkable(p)) throw std::invalid_argument("New start must be walkable");
    km_ += manhattan(start_, p);
    start_ = p;
}
std::vector<Point> DStarLiteAgent::findPath() {
    if (!grid_.walkable(start_) || !grid_.walkable(goal_)) return {};
    computeShortestPath();
    if (!std::isfinite(g_[grid_.id(start_)])) return {};
    std::vector<Point> path{start_};
    Point at = start_;
    for (int steps = 0; steps < grid_.count(); ++steps) {
        if (at == goal_) return path;
        Point next = at;
        double best = INF;
        for (Point p : grid_.neighbors(at)) {
            double value = edge(grid_.id(at), grid_.id(p)) + g_[grid_.id(p)];
            if (value < best - EPS) { best = value; next = p; }
        }
        if (!std::isfinite(best) || next == at) return {};
        path.push_back(next);
        at = next;
    }
    throw std::runtime_error("Path extraction encountered a cycle");
}
} // namespace himera
