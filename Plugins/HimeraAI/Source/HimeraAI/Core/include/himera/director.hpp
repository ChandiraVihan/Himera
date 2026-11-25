#pragma once
#include <string>
#include <vector>
namespace himera {
enum class Encounter { Quiet, RadioWhisper, VisualDistortion, HollowWave };
std::string toString(Encounter event);
struct DirectorState {
    double health01 = 1.0;
    int ammo = 30;
    double exposure01 = 0.0;
    double stress01 = 0.0;
    int activeHollows = 0;
    double secondsSinceWave = 100.0;
    bool safeZone = false;
};
struct EncounterScore {
    Encounter encounter;
    double utility;
    bool allowed;
    std::string reason;
};
struct DirectorDecision {
    Encounter chosen;
    std::vector<EncounterScore> scores;
};
class ExposureDirectorAgent {
public:
    DirectorDecision choose(const DirectorState& state) const;
};
} // namespace himera
