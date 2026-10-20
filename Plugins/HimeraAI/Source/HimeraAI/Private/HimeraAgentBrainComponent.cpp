#include "HimeraAgentBrainComponent.h"

#include "himera/goap.hpp"
#include "himera/grid.hpp"
#include "himera/hollow.hpp"

#include <exception>
#include <memory>
#include <string>

// Per-NPC planner state. Defined here so that no engine or game module sees core headers.
struct FHimeraBrainImpl
{
    himera::SecurityGoapAgent Goap;
    std::unique_ptr<himera::HollowBeliefAgent> Hollow;
};

namespace
{
    FString ToFString(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

    FHimeraAgentDecision MakeFailure(EHimeraStatus Status, const FString& Message)
    {
        FHimeraAgentDecision D;
        D.Command = EHimeraCommand::NoAction;
        D.Status = Status;
        D.Message = Message;
        return D;
    }

    EHimeraCommand CommandForStep(const std::string& Step)
    {
        if (Step == "TakeCover") return EHimeraCommand::MoveToCover;
        if (Step == "Reload") return EHimeraCommand::Reload;
        if (Step == "AcquireLineOfSight") return EHimeraCommand::Attack; // move/aim only
        if (Step == "SuppressTarget") return EHimeraCommand::Attack;
        if (Step == "InvestigateSound") return EHimeraCommand::Investigate;
        if (Step == "PatrolSector") return EHimeraCommand::Patrol;
        if (Step == "Retreat") return EHimeraCommand::Retreat;
        return EHimeraCommand::NoAction;
    }
}

UHimeraAgentBrainComponent::UHimeraAgentBrainComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // decisions come from a timer / events, not Tick
}

void UHimeraAgentBrainComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!Impl.IsValid())
    {
        Impl = MakeShared<FHimeraBrainImpl>();
    }
}

bool UHimeraAgentBrainComponent::InitializeHollow(const FHimeraGridSettings& GridSettings, const TArray<FIntPoint>& BlockedCells)
{
    if (!Impl.IsValid())
    {
        Impl = MakeShared<FHimeraBrainImpl>();
    }
    try
    {
        himera::Grid G(GridSettings.Width, GridSettings.Height);
        for (const FIntPoint& Cell : BlockedCells)
        {
            if (!GridSettings.IsInside(Cell))
            {
                UE_LOG(LogHimeraAI, Warning, TEXT("InitializeHollow: blocked cell (%d,%d) outside grid, ignored"), Cell.X, Cell.Y);
                continue;
            }
            G.setBlocked({Cell.X, Cell.Y}, true);
        }
        Impl->Hollow = std::make_unique<himera::HollowBeliefAgent>(std::move(G));
        Grid = GridSettings;
        LastPredictTime = -1.0;
        return true;
    }
    catch (const std::exception& E)
    {
        UE_LOG(LogHimeraAI, Error, TEXT("InitializeHollow failed: %s"), *ToFString(E.what()));
        Impl->Hollow.reset();
        return false;
    }
}

void UHimeraAgentBrainComponent::ReportPlayerSeen(const FVector& Location, double /*TimeSeconds*/)
{
    LastPlayerLocation = Location;
    bHasLastPlayerLocation = true;

    if (Role == EHimeraAgentRole::Hollow && Impl.IsValid() && Impl->Hollow)
    {
        FIntPoint Cell;
        if (!Grid.WorldToCell(Location, Cell))
        {
            return;
        }
        try
        {
            Impl->Hollow->seePlayer({Cell.X, Cell.Y});
        }
        catch (const std::exception& E)
        {
            UE_LOG(LogHimeraAI, Warning, TEXT("ReportPlayerSeen ignored: %s"), *ToFString(E.what()));
        }
    }
}

void UHimeraAgentBrainComponent::ReportPlayerLost(const FVector& LastKnownLocation, double /*TimeSeconds*/)
{
    // Keep only the last known position. The live position is never revealed.
    LastPlayerLocation = LastKnownLocation;
    bHasLastPlayerLocation = true;
}

bool UHimeraAgentBrainComponent::ReportGunshot(const FVector& Location, float Confidence, float MaxRangeCm)
{
    if (Confidence < 0.f || Confidence > 1.f || MaxRangeCm <= 0.f)
    {
        UE_LOG(LogHimeraAI, Warning, TEXT("ReportGunshot: invalid confidence/range"));
        return false;
    }
    const AActor* Owner = GetOwner();
    if (Owner && FVector::Dist(Owner->GetActorLocation(), Location) > MaxRangeCm)
    {
        return false; // too far to hear
    }

    LastHeardLocation = Location;
    bSoundPending = true;

    if (Role == EHimeraAgentRole::Hollow && Impl.IsValid() && Impl->Hollow)
    {
        FIntPoint Cell;
        if (Grid.WorldToCell(Location, Cell))
        {
            try
            {
                Impl->Hollow->hear({Cell.X, Cell.Y}, HearingSigmaCells, Confidence);
            }
            catch (const std::exception& E)
            {
                UE_LOG(LogHimeraAI, Warning, TEXT("ReportGunshot (belief) ignored: %s"), *ToFString(E.what()));
                return false;
            }
        }
    }
    return true;
}

void UHimeraAgentBrainComponent::ReportVisibleEmpty(const TArray<FIntPoint>& Cells)
{
    if (!Impl.IsValid() || !Impl->Hollow)
    {
        return;
    }
    std::vector<himera::Point> Visible;
    Visible.reserve(Cells.Num());
    for (const FIntPoint& C : Cells)
    {
        if (Grid.IsInside(C))
        {
            Visible.push_back({C.X, C.Y});
        }
    }
    try
    {
        Impl->Hollow->observeEmpty(Visible);
    }
    catch (const std::exception& E)
    {
        UE_LOG(LogHimeraAI, Warning, TEXT("ReportVisibleEmpty ignored: %s"), *ToFString(E.what()));
    }
}

void UHimeraAgentBrainComponent::ReportCellBlocked(const FIntPoint& Cell, bool bBlocked)
{
    if (!Impl.IsValid() || !Impl->Hollow || !Grid.IsInside(Cell))
    {
        return;
    }
    try
    {
        Impl->Hollow->setBlocked({Cell.X, Cell.Y}, bBlocked);
    }
    catch (const std::exception& E)
    {
        UE_LOG(LogHimeraAI, Warning, TEXT("ReportCellBlocked ignored: %s"), *ToFString(E.what()));
    }
}

bool UHimeraAgentBrainComponent::ReportActionResult(int64 DecisionId, EHimeraActionResult Result)
{
    if (DecisionId != ActiveDecisionId || DecisionId == 0)
    {
        return false; // stale: a newer decision replaced this one
    }

    if (Result == EHimeraActionResult::Succeeded)
    {
        namespace H = himera;
        if (ActiveActionName == TEXT("AcquireLineOfSight")) ConfirmedFacts |= H::bit(H::Fact::Aimed);
        else if (ActiveActionName == TEXT("SuppressTarget")) ConfirmedFacts |= H::bit(H::Fact::TargetSuppressed);
        else if (ActiveActionName == TEXT("Retreat")) ConfirmedFacts |= H::bit(H::Fact::Retreated);
        else if (ActiveActionName == TEXT("InvestigateSound")) bSoundPending = false;
    }

    // Any result ends the active decision; the next Decide() is "changed" even if identical.
    ActiveDecisionId = 0;
    ActiveActionName.Empty();
    LastDecision = FHimeraAgentDecision();
    return true;
}

void UHimeraAgentBrainComponent::CancelActiveDecision()
{
    ActiveDecisionId = 0;
    ActiveActionName.Empty();
    LastDecision = FHimeraAgentDecision();
}

FHimeraAgentDecision UHimeraAgentBrainComponent::Decide(const FHimeraAgentObservation& Obs)
{
    if (!Impl.IsValid())
    {
        Impl = MakeShared<FHimeraBrainImpl>();
    }
    if (Obs.Health01 < 0.f || Obs.Health01 > 1.f || Obs.Ammo < 0 || Obs.TimeSeconds < 0.0)
    {
        return Finish(MakeFailure(EHimeraStatus::InvalidInput, TEXT("Health01 must be 0..1, Ammo >= 0, TimeSeconds >= 0")));
    }

    try
    {
        return Finish(Role == EHimeraAgentRole::Hollow ? DecideHollow(Obs) : DecideSecurity(Obs));
    }
    catch (const std::exception& E)
    {
        return Finish(MakeFailure(EHimeraStatus::InvalidInput, ToFString(E.what())));
    }
}

FHimeraAgentDecision UHimeraAgentBrainComponent::DecideSecurity(const FHimeraAgentObservation& Obs)
{
    namespace H = himera;

    const bool bLowHealth = Obs.Health01 < LowHealthThreshold;

    // Confirmed facts expire when their cause goes away.
    if (!Obs.bPlayerKnown)
    {
        ConfirmedFacts &= ~(H::bit(H::Fact::Aimed) | H::bit(H::Fact::TargetSuppressed));
    }
    if (!bLowHealth)
    {
        ConfirmedFacts &= ~H::bit(H::Fact::Retreated);
    }

    H::Facts Facts = ConfirmedFacts;
    if (Obs.bPlayerKnown) Facts |= H::bit(H::Fact::KnowsTarget);
    if (Obs.bUnderFire) Facts |= H::bit(H::Fact::UnderFire);
    if (Obs.bInCover) Facts |= H::bit(H::Fact::InCover);
    if (Obs.Ammo > 0) Facts |= H::bit(H::Fact::HasAmmo);
    if (Obs.bHeardSound || bSoundPending) Facts |= H::bit(H::Fact::HeardSound);
    if (bLowHealth) Facts |= H::bit(H::Fact::LowHealth);

    const H::Plan Plan = Impl->Goap.plan(Facts);

    FHimeraAgentDecision D;
    D.PlanCost = static_cast<float>(Plan.cost);
    if (!Plan.achieved)
    {
        D.Message = TEXT("No GOAP plan reaches the goal from the current facts");
        return D; // NoAction
    }
    if (Plan.steps.empty())
    {
        D.Message = TEXT("Goal already satisfied");
        return D; // NoAction
    }

    const std::string& Step = Plan.steps.front();
    D.Command = CommandForStep(Step);
    D.ActionName = ToFString(Step);
    D.Message = FString::Printf(TEXT("Plan of %d step(s), cost %.1f, %d states expanded"),
                                static_cast<int32>(Plan.steps.size()), Plan.cost, Plan.expandedStates);

    if (D.Command == EHimeraCommand::Investigate)
    {
        D.Destination = LastHeardLocation;
    }
    else if (D.Command == EHimeraCommand::Attack)
    {
        if (!bHasLastPlayerLocation)
        {
            return MakeFailure(EHimeraStatus::TargetDestroyed, TEXT("Attack planned but no player location is known"));
        }
        D.Destination = LastPlayerLocation;
    }
    return D;
}

FHimeraAgentDecision UHimeraAgentBrainComponent::DecideHollow(const FHimeraAgentObservation& Obs)
{
    if (!Impl->Hollow)
    {
        return MakeFailure(EHimeraStatus::InvalidInput, TEXT("Hollow belief map not initialised: call InitializeHollow"));
    }

    // Diffuse belief on its own slower timer, not on every decision.
    if (LastPredictTime < 0.0 || Obs.TimeSeconds - LastPredictTime >= BeliefPredictIntervalSeconds)
    {
        Impl->Hollow->predict();
        LastPredictTime = Obs.TimeSeconds;
    }

    FIntPoint SelfCell;
    if (!Grid.WorldToCell(Obs.SelfLocation, SelfCell))
    {
        return MakeFailure(EHimeraStatus::InvalidInput, TEXT("SelfLocation is outside the Hollow grid"));
    }

    std::vector<himera::Point> Allies;
    for (const FVector& A : Obs.AllyTargets)
    {
        FIntPoint C;
        if (Grid.WorldToCell(A, C))
        {
            Allies.push_back({C.X, C.Y});
        }
    }

    const himera::Point Chosen = Impl->Hollow->selectSearchCell({SelfCell.X, SelfCell.Y}, Allies);

    FHimeraAgentDecision D;
    D.ActionName = TEXT("SelectSearchCell");
    D.PlanCost = static_cast<float>(Impl->Hollow->probability(Chosen));
    if (Chosen.x == SelfCell.X && Chosen.y == SelfCell.Y)
    {
        D.Message = TEXT("Already at the best search cell");
        return D;
    }
    D.Command = EHimeraCommand::Search;
    D.Destination = Grid.CellToWorld(FIntPoint(Chosen.x, Chosen.y));
    D.Message = FString::Printf(TEXT("Search cell (%d,%d), belief %.3f"), Chosen.x, Chosen.y, D.PlanCost);
    return D;
}

FHimeraAgentDecision UHimeraAgentBrainComponent::Finish(FHimeraAgentDecision D)
{
    const bool bSame = D.Status == EHimeraStatus::Ok && LastDecision.Status == EHimeraStatus::Ok &&
                       LastDecision.Command == D.Command &&
                       FVector::Dist(LastDecision.Destination, D.Destination) <= UnchangedToleranceCm;

    if (bSame && (ActiveDecisionId != 0 || D.Command == EHimeraCommand::NoAction))
    {
        D.DecisionId = ActiveDecisionId; // keep the in-flight id valid (0 for NoAction)
        D.bChanged = false;
        return D;
    }

    D.bChanged = true;
    if (D.Status == EHimeraStatus::Ok && D.Command != EHimeraCommand::NoAction)
    {
        D.DecisionId = NextDecisionId++;
        ActiveDecisionId = D.DecisionId;
        ActiveActionName = D.ActionName;
    }
    else
    {
        D.DecisionId = 0;
        ActiveDecisionId = 0;
        ActiveActionName.Empty();
    }
    LastDecision = D;
    return D;
}
