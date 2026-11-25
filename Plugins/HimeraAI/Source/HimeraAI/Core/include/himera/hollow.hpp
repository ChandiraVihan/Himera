#pragma once
#include "himera/grid.hpp"
#include <vector>

namespace himera {
class HollowBeliefAgent {
public:
    explicit HollowBeliefAgent(Grid grid);
    // Player may move between observations; probability mass diffuses to walkable neighbors.
    void predict(double moveChance = 0.3);
    // A gunshot is more likely to have come from points near its perceived origin.
    void hear(Point perceivedOrigin, double sigma = 1.5, double confidence = 0.9);
    void seePlayer(Point player);
    // Reconcile a dynamic obstacle without discarding previous observations.
    void setBlocked(Point cell, bool blocked);
    // Visually inspected locations without a player become less probable.
    void observeEmpty(const std::vector<Point>& visibleCells, double remainingLikelihood = 0.02);
    Point selectSearchCell(Point self, const std::vector<Point>& allyTargets = {}) const;
    double probability(Point p) const { return belief_.at(grid_.id(p)); }
    double totalProbability() const;
    const Grid& grid() const { return grid_; }
private:
    Grid grid_;
    std::vector<double> belief_;
    void normalize();
};
} // namespace himera
