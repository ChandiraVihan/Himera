#pragma once

#include "CoreMinimal.h"
#include "HimeraAgentTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogHimeraAI, Log, All);

/** What the executor (Blueprint / AI Controller) should do. See docs/integration/OBSERVATION_RESULT_CONTRACT.md */
UENUM(BlueprintType)
enum class EHimeraCommand : uint8
{
    NoAction,
    Patrol,
    Investigate,
    MoveToCover,
    Attack,
    Reload,
    Retreat,
    Search,
    TriggerExposureEvent,
    FollowRoute
};

UENUM(BlueprintType)
enum class EHimeraStatus : uint8
{
    Ok,
    Unreachable,
    InvalidInput,
    TargetDestroyed,
    PlannerTimedOut,
    NoSafeEvent
};

/** Result the executor reports back for a decision. */
UENUM(BlueprintType)
enum class EHimeraActionResult : uint8
{
    Succeeded,
    Failed,
    Cancelled,
    Blocked
};

UENUM(BlueprintType)
enum class EHimeraAgentRole : uint8
{
    Security,
    Hollow
};

/** Authored planning grid. Cells are waypoint nodes, not floor tiles. Units: centimetres. */
USTRUCT(BlueprintType)
struct FHimeraGridSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera", meta = (ClampMin = "1"))
    int32 Width = 12;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera", meta = (ClampMin = "1"))
    int32 Height = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera", meta = (ClampMin = "1.0"))
    float CellSizeCm = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera")
    FVector OriginCm = FVector::ZeroVector;

    bool IsInside(const FIntPoint& Cell) const
    {
        return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Width && Cell.Y < Height;
    }

    /** Returns false when the world position is outside the grid. Z is ignored. */
    bool WorldToCell(const FVector& World, FIntPoint& OutCell) const
    {
        OutCell.X = FMath::FloorToInt((World.X - OriginCm.X) / CellSizeCm);
        OutCell.Y = FMath::FloorToInt((World.Y - OriginCm.Y) / CellSizeCm);
        return IsInside(OutCell);
    }

    /** Centre of the cell; Z is taken from the origin (project onto the NavMesh before use). */
    FVector CellToWorld(const FIntPoint& Cell) const
    {
        return FVector(OriginCm.X + (Cell.X + 0.5) * CellSizeCm,
                       OriginCm.Y + (Cell.Y + 0.5) * CellSizeCm,
                       OriginCm.Z);
    }
};

/** Facts about the world for one NPC. Producers and bounds are listed in the contract document. */
USTRUCT(BlueprintType)
struct FHimeraAgentObservation
{
    GENERATED_BODY()

    /** 0..1 (Health / MaxHealth). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Health01 = 1.f;

    /** Rounds available, >= 0. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera", meta = (ClampMin = "0"))
    int32 Ammo = 0;

    /** True while the target is remembered by AI Perception (seen within the memory window). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera")
    bool bPlayerKnown = false;

    /** Took damage within the last ~3 seconds. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera")
    bool bUnderFire = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera")
    bool bInCover = false;

    /** Extra "heard something" flag from the producer; the component also tracks ReportGunshot itself. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera")
    bool bHeardSound = false;

    /** Pawn location, cm. Needed by the Hollow role. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera")
    FVector SelfLocation = FVector::ZeroVector;

    /** Search destinations of other Hollows, cm. Hollow role only. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera")
    TArray<FVector> AllyTargets;

    /** UWorld::GetTimeSeconds(). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera", meta = (ClampMin = "0.0"))
    double TimeSeconds = 0.0;
};

/** What an agent wants the executor to do. A decision is not an outcome. */
USTRUCT(BlueprintType)
struct FHimeraAgentDecision
{
    GENERATED_BODY()

    /** Echo this in ReportActionResult. Unchanged decisions keep their id. */
    UPROPERTY(BlueprintReadOnly, Category = "Himera")
    int64 DecisionId = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Himera")
    EHimeraCommand Command = EHimeraCommand::NoAction;

    UPROPERTY(BlueprintReadOnly, Category = "Himera")
    EHimeraStatus Status = EHimeraStatus::Ok;

    /** Target for movement-type commands, cm. Zero otherwise. */
    UPROPERTY(BlueprintReadOnly, Category = "Himera")
    FVector Destination = FVector::ZeroVector;

    /** FollowRoute only: cell centres in cm, start -> goal. */
    UPROPERTY(BlueprintReadOnly, Category = "Himera")
    TArray<FVector> Waypoints;

    /** False when this is the same command/destination as the previous decision: do not restart it. */
    UPROPERTY(BlueprintReadOnly, Category = "Himera")
    bool bChanged = true;

    UPROPERTY(BlueprintReadOnly, Category = "Himera")
    int32 GraphRevision = 0;

    /** GOAP plan cost, D* Lite path cost, or Director utility. */
    UPROPERTY(BlueprintReadOnly, Category = "Himera")
    float PlanCost = 0.f;

    /** Raw planner step or encounter name, for debug overlays. */
    UPROPERTY(BlueprintReadOnly, Category = "Himera")
    FString ActionName;

    UPROPERTY(BlueprintReadOnly, Category = "Himera")
    FString Message;
};
