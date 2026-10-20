#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HimeraAgentTypes.h"
#include "HimeraAITrooperComponent.generated.h"

struct FHimeraTrooperImpl;

/**
 * Containment Trooper route planner (D* Lite). Plans on the authored grid (see
 * docs/integration/NAVIGATION_MAPPING.md) and returns waypoints; the AI Controller walks them
 * with normal Unreal navigation. Door/hazard updates must also be applied to the real
 * collision / NavMesh by the Door and Hazard Blueprints.
 */
UCLASS(ClassGroup = (Himera), meta = (BlueprintSpawnableComponent))
class HIMERAAI_API UHimeraAITrooperComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHimeraAITrooperComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera")
    FHimeraGridSettings Grid;

    /** Walls and other permanently blocked cells. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera")
    TArray<FIntPoint> WallCells;

    /** DoorId -> cell. Doors start open unless listed in ClosedDoorsAtStart. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera")
    TMap<int32, FIntPoint> DoorCells;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera")
    TArray<int32> ClosedDoorsAtStart;

    /** HazardId -> cell. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera")
    TMap<int32, FIntPoint> HazardCells;

    /** A planning update slower than this is reported as PlannerTimedOut. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera", meta = (ClampMin = "0.1"))
    float PlannerBudgetMs = 4.f;

    /** Build (or rebuild) the planner for a goal. Rebuilding is required when the goal moves. */
    UFUNCTION(BlueprintCallable, Category = "Himera")
    FHimeraAgentDecision SetGoal(const FVector& StartWorld, const FVector& GoalWorld);

    /** Door Blueprint calls this in the same event that toggles collision/NavMesh. */
    UFUNCTION(BlueprintCallable, Category = "Himera|Events")
    bool OnDoorStateChanged(int32 DoorId, bool bOpen);

    /** Hazard volume calls this. Risk is in "extra cells of travel" (default 5). */
    UFUNCTION(BlueprintCallable, Category = "Himera|Events")
    bool OnHazardStateChanged(int32 HazardId, bool bActive, float Risk = 5.f);

    /** Replan from the pawn's current position. Cheap after small changes (incremental). */
    UFUNCTION(BlueprintCallable, Category = "Himera")
    FHimeraAgentDecision Replan(const FVector& StartWorld);

    /** Executor result. Stale ids are ignored. Failed/Blocked: call Replan() next. */
    UFUNCTION(BlueprintCallable, Category = "Himera|Events")
    bool ReportActionResult(int64 DecisionId, EHimeraActionResult Result);

    UFUNCTION(BlueprintPure, Category = "Himera")
    int32 GetGraphRevision() const { return GraphRevision; }

private:
    bool BuildPlanner(const FIntPoint& StartCell, const FIntPoint& GoalCell, FString& OutError);
    FHimeraAgentDecision Plan(const FIntPoint& StartCell);

    TSharedPtr<FHimeraTrooperImpl> Impl;

    TSet<int32> ClosedDoors;
    TMap<int32, float> ActiveHazards;
    FIntPoint GoalCell = FIntPoint(-1, -1);
    bool bHasGoal = false;
    bool bDoorsInitialized = false;

    int32 GraphRevision = 0;
    int64 NextDecisionId = 1;
    int64 ActiveDecisionId = 0;
    TArray<FVector> LastWaypoints;

    float CellRisk(const FIntPoint& Cell) const;
    bool IsCellBlocked(const FIntPoint& Cell) const;
};
