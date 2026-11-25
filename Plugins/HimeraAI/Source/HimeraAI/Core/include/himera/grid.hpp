#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace himera {
struct Point {
    int x = 0;
    int y = 0;
    bool operator==(const Point&) const = default;
};

inline int manhattan(Point a, Point b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

class Grid {
public:
    Grid(int width, int height) : width_(width), height_(height), blocked_(width * height, false), risk_(width * height, 0.0) {
        if (width <= 0 || height <= 0) throw std::invalid_argument("Grid dimensions must be positive");
    }
    int width() const { return width_; }
    int height() const { return height_; }
    int count() const { return width_ * height_; }
    bool inside(Point p) const { return p.x >= 0 && p.y >= 0 && p.x < width_ && p.y < height_; }
    int id(Point p) const {
        if (!inside(p)) throw std::out_of_range("Grid point outside boundary");
        return p.y * width_ + p.x;
    }
    Point point(int id) const {
        if (id < 0 || id >= count()) throw std::out_of_range("Invalid grid cell");
        return {id % width_, id / width_};
    }
    bool blocked(Point p) const { return blocked_.at(id(p)); }
    bool walkable(Point p) const { return inside(p) && !blocked_[id(p)]; }
    void setBlocked(Point p, bool value) { blocked_.at(id(p)) = value; }
    double risk(Point p) const { return risk_.at(id(p)); }
    void setRisk(Point p, double value) {
        if (!std::isfinite(value) || value < 0) throw std::invalid_argument("Risk must be nonnegative and finite");
        risk_.at(id(p)) = value;
    }
    // For graph updates we need geometrical neighbors, including blocked ones.
    std::vector<Point> neighbors(Point p, bool walkableOnly = true) const {
        std::vector<Point> result;
        for (Point delta : {Point{0, -1}, Point{1, 0}, Point{0, 1}, Point{-1, 0}}) {
            Point q{p.x + delta.x, p.y + delta.y};
            if (inside(q) && (!walkableOnly || walkable(q))) result.push_back(q);
        }
        return result;
    }
private:
    int width_, height_;
    std::vector<bool> blocked_;
    std::vector<double> risk_;
};
} // namespace himera
