// D* Lite vs fresh A* on identical synthetic scenarios (40 x 40 grids, random walls).
// Reports: path validity, cost agreement, expanded nodes, planning microseconds, spread.
// Usage: astar_compare [results.csv]
// Exit code is non-zero only if D* Lite is ever wrong (invalid path / different cost / different
// reachability). Speed numbers are reported, never asserted.
#include "astar.hpp"
#include "himera/dstar.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using himera::DStarLiteAgent;
using himera::Grid;
using himera::Point;
using namespace trooper_test;
using Clock = std::chrono::steady_clock;

static const int W = 40, H = 40, RUNS = 200;
static const Point START{0, 0}, GOAL{W - 1, H - 1};

static double us(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::micro>(b - a).count();
}
struct Stats {
    std::vector<double> v;
    void add(double x) { v.push_back(x); }
    double mean() const { double s = 0; for (double x : v) s += x; return v.empty() ? 0 : s / v.size(); }
    double sd() const {
        if (v.size() < 2) return 0;
        double m = mean(), s = 0;
        for (double x : v) s += (x - m) * (x - m);
        return std::sqrt(s / (v.size() - 1));
    }
};
struct Row {
    std::string name;
    int runs = 0, reachable = 0, valid = 0, costEqual = 0, reachDisagree = 0;
    Stats dExp, aExp, dUs, aUs;
};

static Grid randomMap(std::mt19937& rng) {
    for (;;) {
        Grid g(W, H);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                if (rng() % 100 < 20 && !(Point{x, y} == START) && !(Point{x, y} == GOAL)) g.setBlocked({x, y}, true);
        if (!astar(g, START, GOAL).path.empty()) return g;
    }
}

static void compare(Row& row, const Grid& mapNow, DStarLiteAgent& d, Point start, double dMicros, int dExpanded,
                    const std::vector<Point>& dPath, const AStarResult& a, double aMicros) {
    ++row.runs;
    if (a.path.empty() != dPath.empty()) { ++row.reachDisagree; return; }
    if (a.path.empty()) return;  // both say unreachable
    ++row.reachable;
    double cost = validatePath(d.grid(), dPath, start, GOAL);
    (void)mapNow;
    if (cost >= 0) ++row.valid;
    if (cost >= 0 && std::fabs(cost - a.cost) < 1e-9) ++row.costEqual;
    row.dExp.add(dExpanded); row.aExp.add(a.expanded);
    row.dUs.add(dMicros); row.aUs.add(aMicros);
}

// kind: 0 = first plan, 1 = one blocked cell on the route, 2 = 10 random changes, 3 = start moves 5 steps
static Row run(const std::string& name, int kind) {
    Row row; row.name = name;
    std::mt19937 rng(2026 + kind);
    for (int r = 0; r < RUNS; ++r) {
        Grid g = randomMap(rng);
        DStarLiteAgent d(g, START, GOAL);
        Point start = START;
        std::vector<Point> dPath;
        double dMicros = 0; int dExp = 0;
        if (kind == 0) {
            auto t0 = Clock::now(); dPath = d.findPath(); auto t1 = Clock::now();
            dMicros = us(t0, t1); dExp = d.lastExpansionCount();
        } else {
            auto base = d.findPath();           // initial plan: not timed here (see kind 0)
            if (kind == 1) {
                Point c = base[1 + rng() % (base.size() - 2)];
                g.setBlocked(c, true);
                auto t0 = Clock::now(); d.updateCell(c, true); dPath = d.findPath(); auto t1 = Clock::now();
                dMicros = us(t0, t1);
            } else if (kind == 2) {
                std::vector<std::tuple<Point, bool, double>> ch;
                for (int i = 0; i < 10; ++i) {
                    Point c{int(rng() % W), int(rng() % H)};
                    if (c == START || c == GOAL) continue;
                    bool block = rng() % 2;
                    double risk = block ? 0.0 : double(rng() % 4);
                    g.setBlocked(c, block); g.setRisk(c, risk);
                    ch.emplace_back(c, block, risk);
                }
                auto t0 = Clock::now();
                for (auto& [c, b, k] : ch) d.updateCell(c, b, k);
                dPath = d.findPath(); auto t1 = Clock::now();
                dMicros = us(t0, t1);
            } else {
                start = base[std::min<size_t>(5, base.size() - 2)];
                auto t0 = Clock::now(); d.moveStart(start); dPath = d.findPath(); auto t1 = Clock::now();
                dMicros = us(t0, t1);
            }
            dExp = d.lastExpansionCount();
        }
        auto t0 = Clock::now(); AStarResult a = astar(g, start, GOAL); auto t1 = Clock::now();
        compare(row, g, d, start, dMicros, dExp, dPath, a, us(t0, t1));
    }
    return row;
}

int main(int argc, char** argv) {
    std::vector<Row> rows;
    rows.push_back(run("first plan (D* from scratch vs A*)", 0));
    rows.push_back(run("1 route cell blocked", 1));
    rows.push_back(run("10 random cell changes", 2));
    rows.push_back(run("start moved 5 steps", 3));

    std::printf("%d runs per scenario, %dx%d grid, 20%% random walls, fixed seeds\n\n", RUNS, W, H);
    std::printf("%-36s %5s %6s %6s %9s %9s %9s %9s %9s %9s\n", "scenario", "reach", "valid", "equal",
                "D*exp", "A*exp", "D*us", "D*sd", "A*us", "A*sd");
    bool bad = false;
    std::ofstream csv(argc > 1 ? argv[1] : "results.csv");
    csv << "scenario,runs,reachable,path_valid,cost_equal_to_astar,reachability_disagree,"
           "dstar_expanded_mean,astar_expanded_mean,dstar_us_mean,dstar_us_sd,astar_us_mean,astar_us_sd\n";
    for (auto& r : rows) {
        std::printf("%-36s %5d %6d %6d %9.1f %9.1f %9.1f %9.1f %9.1f %9.1f\n", r.name.c_str(), r.reachable, r.valid,
                    r.costEqual, r.dExp.mean(), r.aExp.mean(), r.dUs.mean(), r.dUs.sd(), r.aUs.mean(), r.aUs.sd());
        csv << '"' << r.name << "\"," << r.runs << ',' << r.reachable << ',' << r.valid << ',' << r.costEqual << ','
            << r.reachDisagree << ',' << r.dExp.mean() << ',' << r.aExp.mean() << ',' << r.dUs.mean() << ','
            << r.dUs.sd() << ',' << r.aUs.mean() << ',' << r.aUs.sd() << '\n';
        if (r.valid != r.reachable || r.costEqual != r.reachable || r.reachDisagree != 0) bad = true;
    }
    std::puts(bad ? "\nFAIL: D* Lite disagreed with A*" : "\nD* Lite agreed with A* on every run (validity, cost, reachability).");
    std::puts("Timing is wall-clock on this machine and varies between runs. Fewer expansions is not the same as less time.");
    return bad ? 1 : 0;
}
