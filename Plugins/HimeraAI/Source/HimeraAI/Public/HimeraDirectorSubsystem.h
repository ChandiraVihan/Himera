#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HimeraAgentTypes.h"
#include "HimeraDirectorSubsystem.generated.h"

/**
 * HIM-7 Exposure Director. A world subsystem, so there is exactly one per world and no enemy
 * can accidentally create its own. Get it with "Get World Subsystem" in Blueprint.
 *
 * Evaluate() returns the chosen encounter as ActionName ("RadioWhisper", "VisualDistortion",
 * "HollowWave"). Command is TriggerExposureEvent, or NoAction with status NoSafeEvent when the
 * Director chose Quiet. A returned HollowWave is not a spawn: call ReportWaveSpawned() when the
 * wave actually spawns, and ReportHollowRemoved() as each Hollow dies/despawns.
 */
UCLASS()
class HIMERAAI_API UHimeraDirectorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Himera|Director")
    FHimeraAgentDecision Evaluate(float Health01, int32 Ammo, float Exposure01, float Stress01, bool bSafeZone);

    UFUNCTION(BlueprintCallable, Category = "Himera|Director")
    void ReportWaveSpawned(int32 HollowCount);

    UFUNCTION(BlueprintCallable, Category = "Himera|Director")
    void ReportHollowRemoved();

    UFUNCTION(BlueprintPure, Category = "Himera|Director")
    int32 GetActiveHollows() const { return ActiveHollows; }

private:
    int32 ActiveHollows = 0;
    double LastWaveTime = -1.0; // measured by the Director itself, from world time
    int64 NextDecisionId = 1;
};
