#include "HimeraDirectorSubsystem.h"

#include "Engine/World.h"
#include "himera/director.hpp"

#include <exception>

FHimeraAgentDecision UHimeraDirectorSubsystem::Evaluate(float Health01, int32 Ammo, float Exposure01, float Stress01, bool bSafeZone)
{
    FHimeraAgentDecision D;
    const UWorld* World = GetWorld();
    const double Now = World ? World->GetTimeSeconds() : 0.0;

    himera::DirectorState State;
    State.health01 = Health01;
    State.ammo = Ammo;
    State.exposure01 = Exposure01;
    State.stress01 = Stress01;
    State.activeHollows = ActiveHollows;
    State.safeZone = bSafeZone;
    State.secondsSinceWave = LastWaveTime < 0.0 ? 100.0 : static_cast<double>(Now - LastWaveTime);

    try
    {
        const himera::ExposureDirectorAgent Agent;
        const himera::DirectorDecision Result = Agent.choose(State);

        D.ActionName = FString(UTF8_TO_TCHAR(himera::toString(Result.chosen).c_str()));
        for (const himera::EncounterScore& S : Result.scores)
        {
            if (S.encounter == Result.chosen)
            {
                D.PlanCost = static_cast<float>(S.utility);
                D.Message = FString(UTF8_TO_TCHAR(S.reason.c_str()));
            }
        }
        if (Result.chosen == himera::Encounter::Quiet)
        {
            D.Command = EHimeraCommand::NoAction;
            D.Status = EHimeraStatus::NoSafeEvent;
        }
        else
        {
            D.Command = EHimeraCommand::TriggerExposureEvent;
            D.DecisionId = NextDecisionId++;
        }
    }
    catch (const std::exception& E)
    {
        D.Command = EHimeraCommand::NoAction;
        D.Status = EHimeraStatus::InvalidInput;
        D.Message = FString(UTF8_TO_TCHAR(E.what()));
    }
    return D;
}

void UHimeraDirectorSubsystem::ReportWaveSpawned(int32 HollowCount)
{
    ActiveHollows += FMath::Max(0, HollowCount);
    const UWorld* World = GetWorld();
    LastWaveTime = World ? World->GetTimeSeconds() : 0.0;
}

void UHimeraDirectorSubsystem::ReportHollowRemoved()
{
    ActiveHollows = FMath::Max(0, ActiveHollows - 1);
}
