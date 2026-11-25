#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HimeraAgentTypes.h"
#include "HimeraAgentBrainComponent.generated.h"

struct FHimeraBrainImpl;

/**
 * One stateful brain per NPC (Security = GOAP, Hollow = belief search).
 * Add one component to each NPC. Nothing here is shared between NPCs.
 *
 * The owner calls Decide() from a timer (Set Timer by Event, 5-10 Hz) and after each event.
 * The component never moves the pawn: the executor does that and reports back with
 * ReportActionResult().
 */
UCLASS(ClassGroup = (Himera), meta = (BlueprintSpawnableComponent))
class HIMERAAI_API UHimeraAgentBrainComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHimeraAgentBrainComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera")
    EHimeraAgentRole Role = EHimeraAgentRole::Security;

    /** Health01 below this sets the GOAP LowHealth fact. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LowHealthThreshold = 0.30f;

    /** Same command and destination within this distance counts as "unchanged". */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera", meta = (ClampMin = "0.0"))
    float UnchangedToleranceCm = 50.f;

    /** Hollow: seconds between belief diffusion steps. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera|Hollow", meta = (ClampMin = "0.1"))
    float BeliefPredictIntervalSeconds = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Himera|Hollow", meta = (ClampMin = "0.1"))
    float HearingSigmaCells = 1.5f;

    /** Hollow role only: build the belief map. Call once at BeginPlay. */
    UFUNCTION(BlueprintCallable, Category = "Himera")
    bool InitializeHollow(const FHimeraGridSettings& GridSettings, const TArray<FIntPoint>& BlockedCells);

    // ---- Events (gameplay -> AI) -------------------------------------------------------

    /** Sight. Sets the last known player location. Hollow: the belief collapses onto that cell. */
    UFUNCTION(BlueprintCallable, Category = "Himera|Events")
    void ReportPlayerSeen(const FVector& Location, double TimeSeconds);

    /** Sight lost. Keeps the last known location; does not reveal the live position. */
    UFUNCTION(BlueprintCallable, Category = "Himera|Events")
    void ReportPlayerLost(const FVector& LastKnownLocation, double TimeSeconds);

    /** Sound only. Never makes the player "visible". Returns false if ignored (out of range / invalid). */
    UFUNCTION(BlueprintCallable, Category = "Himera|Events")
    bool ReportGunshot(const FVector& Location, float Confidence = 0.9f, float MaxRangeCm = 3000.f);

    /** Hollow: cells inspected and found empty. */
    UFUNCTION(BlueprintCallable, Category = "Himera|Events")
    void ReportVisibleEmpty(const TArray<FIntPoint>& Cells);

    /** Hollow: a door or hazard changed walkability of a cell. */
    UFUNCTION(BlueprintCallable, Category = "Himera|Events")
    void ReportCellBlocked(const FIntPoint& Cell, bool bBlocked);

    /** Executor result for a decision. Stale ids are ignored (returns false). Call Decide() afterwards. */
    UFUNCTION(BlueprintCallable, Category = "Himera|Events")
    bool ReportActionResult(int64 DecisionId, EHimeraActionResult Result);

    // ---- Decision ----------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Himera")
    FHimeraAgentDecision Decide(const FHimeraAgentObservation& Observation);

    /** Drop the active decision, e.g. when the target is destroyed. */
    UFUNCTION(BlueprintCallable, Category = "Himera")
    void CancelActiveDecision();

    UFUNCTION(BlueprintPure, Category = "Himera")
    int64 GetActiveDecisionId() const { return ActiveDecisionId; }

protected:
    virtual void BeginPlay() override;

private:
    FHimeraAgentDecision DecideSecurity(const FHimeraAgentObservation& Obs);
    FHimeraAgentDecision DecideHollow(const FHimeraAgentObservation& Obs);
    FHimeraAgentDecision Finish(FHimeraAgentDecision Decision);

    TSharedPtr<FHimeraBrainImpl> Impl;

    FHimeraGridSettings Grid;
    FHimeraAgentDecision LastDecision;
    int64 NextDecisionId = 1;
    int64 ActiveDecisionId = 0;

    FVector LastPlayerLocation = FVector::ZeroVector;
    bool bHasLastPlayerLocation = false;
    FVector LastHeardLocation = FVector::ZeroVector;
    bool bSoundPending = false;

    /** GOAP facts the executor has confirmed (Aimed, TargetSuppressed, Retreated). */
    uint32 ConfirmedFacts = 0;
    FString ActiveActionName;

    double LastPredictTime = -1.0;
};
