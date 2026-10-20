#include "himera/hollow.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
#include <stdexcept>

namespace himera {
HollowBeliefAgent::HollowBeliefAgent(Grid grid) : grid_(std::move(grid)), belief_(grid_.count(), 0.0) {
    int open = 0;
    for (int i = 0; i < grid_.count(); ++i) if (grid_.walkable(grid_.point(i))) ++open;
    if (open == 0) throw std::invalid_argument("Hollow map needs a walkable cell");
    for (int i = 0; i < grid_.count(); ++i) if (grid_.walkable(grid_.point(i))) belief_[i] = 1.0 / open;
}
void HollowBeliefAgent::normalize() {
    double sum = std::accumulate(belief_.begin(), belief_.end(), 0.0);
    if (!(sum > 0)) throw std::runtime_error("Belief collapsed: contradictory observation");
    for (double& p : belief_) p /= sum;
}
double HollowBeliefAgent::totalProbability() const {
    return std::accumulate(belief_.begin(), belief_.end(), 0.0);
}
void HollowBeliefAgent::predict(double moveChance) {
    if (moveChance < 0 || moveChance > 1 || !std::isfinite(moveChance))
        throw std::invalid_argument("moveChance must be in [0, 1]");
    std::vector<double> next(belief_.size(), 0.0);
    for (int i = 0; i < grid_.count(); ++i) {
        Point p = grid_.point(i);
        if (!grid_.walkable(p)) continue;
        auto near = grid_.neighbors(p);
        next[i] += belief_[i] * (near.empty() ? 1 : (1 - moveChance));
        if (!near.empty()) for (Point q : near) next[grid_.id(q)] += belief_[i] * moveChance / near.size();
    }
    belief_ = std::move(next);
    normalize();
}
void HollowBeliefAgent::hear(Point origin, double sigma, double confidence) {
    if (!grid_.inside(origin) || sigma <= 0 || !std::isfinite(sigma) || confidence < 0 || confidence > 1)
        throw std::invalid_argument("Invalid hearing observation");
    for (int i = 0; i < grid_.count(); ++i) {
        Point p = grid_.point(i);
        if (!grid_.walkable(p)) continue;
        double distance2 = double((p.x-origin.x)*(p.x-origin.x) + (p.y-origin.y)*(p.y-origin.y));
        double likelihood = (1 - confidence) + confidence * std::exp(-distance2 / (2 * sigma * sigma));
        belief_[i] *= likelihood;
    }
    normalize();
}
void HollowBeliefAgent::setBlocked(Point cell, bool blocked) {
    if (!grid_.inside(cell)) throw std::out_of_range("Changed cell outside map");
    bool wasBlocked = grid_.blocked(cell);
    if (wasBlocked == blocked) return;
    const int id = grid_.id(cell);
    double lost = blocked ? belief_[id] : 0.0;
    grid_.setBlocked(cell, blocked);
    belief_[id] = 0.0;
    if (blocked && lost > 0.0) {
        // Spread eliminated probability across remaining walkable cells, rather
        // than resetting the entire posterior to a uniform prior.
        double remaining = std::accumulate(belief_.begin(), belief_.end(), 0.0);
        if (remaining > 1e-12) {
            for (double& v : belief_) v += v / remaining * lost;
        } else {
            int count = 0;
            for (int i = 0; i < grid_.count(); ++i) if (grid_.walkable(grid_.point(i))) ++count;
            if (count == 0) throw std::runtime_error("No walkable cells after topology change");
            for (int i = 0; i < grid_.count(); ++i)
                if (grid_.walkable(grid_.point(i))) belief_[i] = 1.0 / count;
        }
    }
    normalize();
}
void HollowBeliefAgent::seePlayer(Point player) {
    if (!grid_.walkable(player)) throw std::invalid_argument("Observed player in blocked cell");
    std::fill(belief_.begin(), belief_.end(), 0.0);
    belief_[grid_.id(player)] = 1.0;
}
void HollowBeliefAgent::observeEmpty(const std::vector<Point>& visibleCells, double remainingLikelihood) {
    if (!std::isfinite(remainingLikelihood) || remainingLikelihood <= 0 || remainingLikelihood > 1)
        throw std::invalid_argument("remainingLikelihood must be in (0, 1]");
    for (Point p : visibleCells) if (grid_.walkable(p)) belief_[grid_.id(p)] *= remainingLikelihood;
    normalize();
}
Point HollowBeliefAgent::selectSearchCell(Point self, const std::vector<Point>& allyTargets) const {
    if (!grid_.walkable(self)) throw std::invalid_argument("Hollow position must be walkable");
    Point chosen = self;
    double best = -std::numeric_limits<double>::infinity();
    std::vector<int> distance(grid_.count(), -1);
    std::queue<Point> bfs;
    distance[grid_.id(self)] = 0;
    bfs.push(self);
    while (!bfs.empty()) {
        Point at = bfs.front(); bfs.pop();
        for (Point next : grid_.neighbors(at)) {
            const int id = grid_.id(next);
            if (distance[id] >= 0) continue;
            distance[id] = distance[grid_.id(at)] + 1;
            bfs.push(next);
        }
    }
    for (int i = 0; i < grid_.count(); ++i) {
        Point p = grid_.point(i);
        if (!grid_.walkable(p) || distance[i] < 0) continue;
        // Posterior belief, actual walkable travel distance and risk.
        double score = belief_[i] / (1.0 + 0.08 * distance[i]) / (1.0 + grid_.risk(p));
        for (Point ally : allyTargets)
            if (manhattan(p, ally) <= 1) score *= 0.05;
        if (score > best) { best = score; chosen = p; }
    }
    return chosen;
}
} // namespace himera
