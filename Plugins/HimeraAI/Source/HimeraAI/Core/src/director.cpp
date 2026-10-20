#include "himera/director.hpp"
#include <algorithm>
#include <stdexcept>
namespace himera {
std::string toString(Encounter e) {
    switch(e) {
        case Encounter::Quiet: return "Quiet";
        case Encounter::RadioWhisper: return "RadioWhisper";
        case Encounter::VisualDistortion: return "VisualDistortion";
        case Encounter::HollowWave: return "HollowWave";
    }
    return "Unknown";
}
DirectorDecision ExposureDirectorAgent::choose(const DirectorState& s) const {
    if (s.health01 < 0 || s.health01 > 1 || s.exposure01 < 0 || s.exposure01 > 1 ||
        s.stress01 < 0 || s.stress01 > 1 || s.ammo < 0 || s.activeHollows < 0 || s.secondsSinceWave < 0)
        throw std::invalid_argument("Invalid director state");
    bool waveAllowed = !s.safeZone && s.health01 >= 0.35 && s.ammo >= 5 &&
        s.activeHollows < 4 && s.secondsSinceWave >= 25 && s.exposure01 >= 0.4;
    bool distortionAllowed = !s.safeZone && s.exposure01 >= 0.15;
    DirectorDecision result{Encounter::Quiet, {
        {Encounter::Quiet, 0.24 + 0.25 * (1.0 - s.exposure01), true, "Recovery pacing"},
        {Encounter::RadioWhisper, 0.35 + 0.25 * s.stress01 + 0.10 * s.exposure01, true, "Subtle tension"},
        {Encounter::VisualDistortion, 0.20 + 0.65 * s.exposure01, distortionAllowed,
            distortionAllowed ? "Exposure threshold met" : "Needs exposure and non-safe zone"},
        {Encounter::HollowWave, 0.35 + 0.45 * s.exposure01 + 0.15 * s.health01 + std::min(s.ammo, 30) / 30.0 * 0.08,
            waveAllowed, waveAllowed ? "Safe to spawn" : "Health/ammo/cooldown/zone/enemy-cap constraint"}
    }};
    const EncounterScore* best = &result.scores.front();
    for (const auto& score : result.scores)
        if (score.allowed && score.utility > best->utility) best = &score;
    result.chosen = best->encounter;
    return result;
}
} // namespace himera
