#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace himera {
// A small symbolic planning world, separate from Unreal actors and components.
enum class Fact : std::uint32_t {
    KnowsTarget = 1u << 0,
    UnderFire = 1u << 1,
    InCover = 1u << 2,
    HasAmmo = 1u << 3,
    Aimed = 1u << 4,
    TargetSuppressed = 1u << 5,
    HeardSound = 1u << 6,
    SoundInvestigated = 1u << 7,
    Patrolled = 1u << 8,
    LowHealth = 1u << 9,
    Retreated = 1u << 10
};
using Facts = std::uint32_t;
constexpr Facts bit(Fact f) { return static_cast<Facts>(f); }
constexpr bool has(Facts state, Fact f) { return (state & bit(f)) != 0; }

struct GoapAction {
    std::string name;
    Facts requiredTrue = 0;
    Facts requiredFalse = 0;
    Facts add = 0;
    Facts remove = 0;
    double baseCost = 1.0;
};
struct Plan {
    std::vector<std::string> steps;
    double cost = 0;
    int expandedStates = 0;
    bool achieved = false;
};
class SecurityGoapAgent {
public:
    SecurityGoapAgent();
    Plan plan(Facts initial) const;
    Plan planTo(Facts initial, Fact goal) const;
    Facts apply(Facts state, const std::string& actionName) const;
private:
    std::vector<GoapAction> actions_;
    static double actionCost(const GoapAction& action, Facts state);
};
} // namespace himera
