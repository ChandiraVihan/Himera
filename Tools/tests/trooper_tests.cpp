// Trooper (D* Lite) scenario tests. Plain C++, no Unreal, no test framework.
// Test map (7 x 5): a wall at x = 3 with two gaps, "door A" at (3,1) and "door B" at (3,3).
// Start (0,2), goal (6,2). Both routes cost the same (8 steps).
#include "astar.hpp"
#include "himera/dstar.hpp"
#include <cmath>
#include <cstdio>
#include <random>
#include <set>

using himera::DStarLiteAgent;
using himera::Grid;
using himera::Point;
using namespace trooper_test;

static int failures = 0;
static int checks = 0;
#define CHECK(cond, msg) do { ++checks; if (!(cond)) { ++failures; std::printf("  FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__); } } while (0)

static const Point START{0, 2}, GOAL{6, 2}, DOOR_A{3, 1}, DOOR_B{3, 3};

static Grid makeMap() {
    Grid g(7, 5);
    for (int y = 0; y < 5; ++y)
        if (y != 1 && y != 3) g.setBlocked({3, y}, true);
    return g;
}
static bool usesCell(const std::vector<Point>& p, Point c) {
    for (Point q : p) if (q == c) return true;
    return false;
}
static bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

static void testBaseline() {
    std::puts("baseline: open map, both routes");
    Grid g = makeMap();
    DStarLiteAgent d(g, START, GOAL);
    auto path = d.findPath();
    double cost = validatePath(d.grid(), path, START, GOAL);
    CHECK(cost >= 0, "D* path is valid");
    CHECK(near(cost, 8.0), "cost is 8 steps");
    CHECK(near(cost, astar(g, START, GOAL).cost), "same cost as A*");
}

static void testClosedAlternativeRoute() {
    std::puts("closed alternative route: close door A, route must use door B");
    DStarLiteAgent d(makeMap(), START, GOAL);
    d.findPath();
    d.updateCell(DOOR_A, true);
    auto path = d.findPath();
    CHECK(validatePath(d.grid(), path, START, GOAL) >= 0, "path still valid");
    CHECK(!usesCell(path, DOOR_A), "path avoids the closed door");
    CHECK(usesCell(path, DOOR_B), "path goes through the other door");
    d.updateCell(DOOR_A, false);
    CHECK(validatePath(d.grid(), d.findPath(), START, GOAL) >= 0, "path valid after reopening");
}

static void testBothRoutesBlocked() {
    std::puts("both routes blocked: must report no path, not a fake one");
    DStarLiteAgent d(makeMap(), START, GOAL);
    d.findPath();
    d.updateCell(DOOR_A, true);
    d.updateCell(DOOR_B, true);
    CHECK(d.findPath().empty(), "no path when both doors are closed");
    d.updateCell(DOOR_B, false);
    auto path = d.findPath();
    CHECK(!path.empty() && usesCell(path, DOOR_B), "route returns when one door reopens");
}

static void testRapidRepeatedDoorChanges() {
    std::puts("rapid repeated door changes: D* must match fresh A* after every change");
    Grid reference = makeMap();
    DStarLiteAgent d(reference, START, GOAL);
    d.findPath();
    std::mt19937 rng(12345);
    int mismatches = 0;
    for (int i = 0; i < 500; ++i) {
        Point door = (rng() & 1) ? DOOR_A : DOOR_B;
        bool close = rng() & 1;
        reference.setBlocked(door, close);
        d.updateCell(door, close);
        auto fresh = astar(reference, START, GOAL);
        auto path = d.findPath();
        if (fresh.path.empty()) { if (!path.empty()) ++mismatches; continue; }
        double cost = validatePath(d.grid(), path, START, GOAL);
        if (cost < 0 || !near(cost, fresh.cost)) ++mismatches;
    }
    CHECK(mismatches == 0, "500 random door toggles, D* cost always equals A* cost");
}

static void testHazardCost() {
    std::puts("hazard cost up and down");
    Grid reference = makeMap();
    DStarLiteAgent d(reference, START, GOAL);
    d.findPath();
    // Put a hazard on door A. The planner should prefer door B (cost 8) over A (8 + risk).
    d.updateCell(DOOR_A, false, 5.0);
    auto path = d.findPath();
    CHECK(near(validatePath(d.grid(), path, START, GOAL), 8.0), "detour keeps cost 8");
    CHECK(!usesCell(path, DOOR_A), "avoids the hazard when a cheaper route exists");
    // Put a hazard on door B as well: now both cost more, and the cheaper one wins.
    d.updateCell(DOOR_B, false, 2.0);
    path = d.findPath();
    CHECK(near(validatePath(d.grid(), path, START, GOAL), 10.0), "both hazardous: cost 8 + 2");
    CHECK(usesCell(path, DOOR_B), "picks the lower hazard (2 < 5)");
    // Hazard cleared: back to cost 8.
    d.updateCell(DOOR_A, false, 0.0);
    d.updateCell(DOOR_B, false, 0.0);
    CHECK(near(validatePath(d.grid(), d.findPath(), START, GOAL), 8.0), "cost returns to 8");
    // A hazard cell stays walkable: close door A, make B hazardous, route must still go through B.
    d.updateCell(DOOR_A, true, 0.0);
    d.updateCell(DOOR_B, false, 50.0);
    path = d.findPath();
    CHECK(!path.empty() && usesCell(path, DOOR_B), "hazard is a cost, not a wall");
    CHECK(near(validatePath(d.grid(), path, START, GOAL), 58.0), "cost includes the hazard");
}

static void testMovingStart() {
    std::puts("moving start position along the route (door closes mid-walk)");
    Grid reference = makeMap();
    DStarLiteAgent d(reference, START, GOAL);
    auto first = d.findPath();
    Point usedDoor = usesCell(first, DOOR_A) ? DOOR_A : DOOR_B;
    Point at = START;
    int step = 0;
    while (!(at == GOAL) && step < 40) {
        if (step == 1) { reference.setBlocked(usedDoor, true); d.updateCell(usedDoor, true); }
        auto fresh = astar(reference, at, GOAL);
        auto now = d.findPath();
        double cost = validatePath(d.grid(), now, at, GOAL);
        CHECK(cost >= 0, "path valid from the current position");
        CHECK(near(cost, fresh.cost), "cost equals fresh A* from the current position");
        if (cost < 0 || now.size() < 2) break;
        at = now[1];          // the pawn walks one cell along the planned route
        d.moveStart(at);
        ++step;
    }
    CHECK(at == GOAL, "pawn reaches the goal after the detour");
    CHECK(!usesCell(d.findPath(), usedDoor), "closed door was never used");
}

static void testUnreachableDestination() {
    std::puts("unreachable destination");
    {
        bool threw = false;  // the core refuses to build a planner for a goal inside a wall
        try { DStarLiteAgent d(makeMap(), START, {3, 0}); } catch (const std::exception&) { threw = true; }
        CHECK(threw, "goal inside a wall is rejected at construction (adapter must catch it)");
    }
    {
        DStarLiteAgent d(makeMap(), START, GOAL);
        d.findPath();
        d.updateCell(GOAL, true);  // goal becomes blocked later
        CHECK(d.findPath().empty(), "goal that becomes blocked gives no path");
    }
    {
        Grid g = makeMap();
        for (Point p : {Point{5, 2}, Point{6, 1}, Point{6, 3}}) g.setBlocked(p, true);  // seal (6,2)
        DStarLiteAgent d(g, START, GOAL);
        CHECK(d.findPath().empty(), "sealed goal cell gives no path");
    }
    {
        bool threw = false;
        try { DStarLiteAgent d(makeMap(), START, {99, 99}); d.findPath(); } catch (const std::exception&) { threw = true; }
        CHECK(threw, "goal outside the grid raises an exception (caught by the Unreal adapter)");
    }
}

static void testSameCostTies() {
    std::puts("same-cost ties");
    auto run = [] {
        DStarLiteAgent d(makeMap(), START, GOAL);
        return d.findPath();
    };
    auto a = run(), b = run();
    CHECK(validatePath(makeMap(), a, START, GOAL) >= 0, "tie path is valid");
    CHECK(a == b, "same input always gives the same path (deterministic)");
    std::set<std::pair<int,int>> seen;
    CHECK(usesCell(a, DOOR_A) != usesCell(a, DOOR_B), "uses exactly one of the two equal routes");
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    testBaseline();
    testClosedAlternativeRoute();
    testBothRoutesBlocked();
    testRapidRepeatedDoorChanges();
    testHazardCost();
    testMovingStart();
    testUnreachableDestination();
    testSameCostTies();
    std::printf("\n%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
