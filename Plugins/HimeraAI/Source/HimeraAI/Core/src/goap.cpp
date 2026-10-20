#include "himera/goap.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>
#include <unordered_map>

namespace himera {
SecurityGoapAgent::SecurityGoapAgent() : actions_({
    {"TakeCover", bit(Fact::UnderFire), bit(Fact::InCover), bit(Fact::InCover), bit(Fact::UnderFire), 1.0},
    {"Reload", 0, bit(Fact::HasAmmo), bit(Fact::HasAmmo), 0, 2.0},
    {"AcquireLineOfSight", bit(Fact::KnowsTarget), bit(Fact::Aimed), bit(Fact::Aimed), 0, 1.0},
    {"SuppressTarget", bit(Fact::KnowsTarget) | bit(Fact::HasAmmo) | bit(Fact::Aimed),
        bit(Fact::TargetSuppressed), bit(Fact::TargetSuppressed), bit(Fact::HasAmmo), 2.0},
    {"InvestigateSound", bit(Fact::HeardSound), bit(Fact::SoundInvestigated),
        bit(Fact::SoundInvestigated), bit(Fact::HeardSound), 1.5},
    {"PatrolSector", 0, bit(Fact::Patrolled), bit(Fact::Patrolled), 0, 1.0},
    {"Retreat", bit(Fact::LowHealth), bit(Fact::Retreated), bit(Fact::Retreated) | bit(Fact::InCover), bit(Fact::UnderFire), 0.8}
}) {}

double SecurityGoapAgent::actionCost(const GoapAction& a, Facts s) {
    // Being exposed makes these actions dangerous; planning can find a safer order.
    if (has(s, Fact::UnderFire) && !has(s, Fact::InCover)) {
        if (a.name == "Reload") return a.baseCost + 8.0;
        if (a.name == "AcquireLineOfSight") return a.baseCost + 6.0;
        if (a.name == "SuppressTarget") return a.baseCost + 10.0;
    }
    return a.baseCost;
}

Plan SecurityGoapAgent::plan(Facts initial) const {
    if (has(initial, Fact::LowHealth)) return planTo(initial, Fact::Retreated);
    if (has(initial, Fact::KnowsTarget)) return planTo(initial, Fact::TargetSuppressed);
    if (has(initial, Fact::HeardSound)) return planTo(initial, Fact::SoundInvestigated);
    return planTo(initial, Fact::Patrolled);
}

Facts SecurityGoapAgent::apply(Facts state, const std::string& name) const {
    for (const auto& a : actions_) {
        if (a.name != name) continue;
        if ((state & a.requiredTrue) != a.requiredTrue || (state & a.requiredFalse) != 0)
            throw std::logic_error("Action preconditions not met: " + name);
        return (state | a.add) & ~a.remove;
    }
    throw std::invalid_argument("Unknown GOAP action: " + name);
}

Plan SecurityGoapAgent::planTo(Facts initial, Fact goal) const {
    struct QueueNode { double cost; Facts state; bool operator>(const QueueNode& other) const {return cost > other.cost;} };
    std::priority_queue<QueueNode, std::vector<QueueNode>, std::greater<>> frontier;
    std::unordered_map<Facts, double> best;
    struct Parent { Facts previous; std::string action; };
    std::unordered_map<Facts, Parent> previous;
    frontier.push({0, initial});
    best[initial] = 0;
    Plan result;
    while (!frontier.empty()) {
        auto [cost, state] = frontier.top(); frontier.pop();
        if (cost > best[state] + 1e-9) continue;
        ++result.expandedStates;
        if (has(state, goal)) {
            result.achieved = true;
            result.cost = cost;
            for (Facts at = state; at != initial; at = previous.at(at).previous)
                result.steps.push_back(previous.at(at).action);
            std::reverse(result.steps.begin(), result.steps.end());
            return result;
        }
        for (const auto& a : actions_) {
            if ((state & a.requiredTrue) != a.requiredTrue || (state & a.requiredFalse) != 0) continue;
            Facts next = (state | a.add) & ~a.remove;
            double nextCost = cost + actionCost(a, state);
            if (!best.contains(next) || nextCost + 1e-9 < best[next]) {
                best[next] = nextCost;
                previous[next] = {state, a.name};
                frontier.push({nextCost, next});
            }
        }
    }
    return result;
}
} // namespace himera
