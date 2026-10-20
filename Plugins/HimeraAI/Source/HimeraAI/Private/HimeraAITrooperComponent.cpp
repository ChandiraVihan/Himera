#include "HimeraAITrooperComponent.h"

#include "himera/dstar.hpp"
#include "himera/grid.hpp"

#include <exception>
#include <memory>
#include <string>

struct FHimeraTrooperImpl
{
    std::unique_ptr<himera::DStarLiteAgent> Agent;
};

namespace
{
    FString ToFString(const char* S) { return FString(UTF8_TO_TCHAR(S)); }

    FHimeraAgentDecision MakeFailure(EHimeraStatus Status, const FString& Message, int32 Revision)
    {
        FHimeraAgentDecision D;
        D.Status = Status;
        D.Command = EHimeraCommand::NoAction;
        D.Message = Message;
        D.GraphRevision = Revision;
        return D;
    }
}

UHimeraAITrooperComponent::UHimeraAITrooperComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

float UHimeraAITrooperComponent::CellRisk(const FIntPoint& Cell) const
{
    float Risk = 0.f;
    for (const TPair<int32, float>& H : ActiveHazards)
    {
        const FIntPoint* HazardCell = HazardCells.Find(H.Key);
        if (HazardCell && *HazardCell == Cell)
        {
            Risk = FMath::Max(Risk, H.Value);
        }
    }
    return Risk;
}

bool UHimeraAITrooperComponent::IsCellBlocked(const FIntPoint& Cell) const
{
    if (WallCells.Contains(Cell))
    {
        return true;
    }
    for (const int32 DoorId : ClosedDoors)
    {
        const FIntPoint* DoorCell = DoorCells.Find(DoorId);
        if (DoorCell && *DoorCell == Cell)
        {
            return true;
        }
    }
    return false;
}

bool UHimeraAITrooperComponent::BuildPlanner(const FIntPoint& StartCell, const FIntPoint& InGoalCell, FString& OutError)
{
    try
    {
        himera::Grid G(Grid.Width, Grid.Height);
        for (int32 Y = 0; Y < Grid.Height; ++Y)
        {
            for (int32 X = 0; X < Grid.Width; ++X)
            {
                const FIntPoint Cell(X, Y);
                if (IsCellBlocked(Cell))
                {
                    G.setBlocked({X, Y}, true);
                }
                const float Risk = CellRisk(Cell);
                if (Risk > 0.f)
                {
                    G.setRisk({X, Y}, Risk);
                }
            }
        }
        if (!Impl.IsValid())
        {
            Impl = MakeShared<FHimeraTrooperImpl>();
        }
        Impl->Agent = std::make_unique<himera::DStarLiteAgent>(std::move(G),
            himera::Point{StartCell.X, StartCell.Y}, himera::Point{InGoalCell.X, InGoalCell.Y});
        return true;
    }
    catch (const std::exception& E)
    {
        OutError = ToFString(E.what());
        if (Impl.IsValid())
        {
            Impl->Agent.reset();
        }
        return false;
    }
}

FHimeraAgentDecision UHimeraAITrooperComponent::SetGoal(const FVector& StartWorld, const FVector& GoalWorld)
{
    FIntPoint StartCell, NewGoal;
    if (!Grid.WorldToCell(StartWorld, StartCell) || !Grid.WorldToCell(GoalWorld, NewGoal))
    {
        return MakeFailure(EHimeraStatus::InvalidInput, TEXT("Start or goal is outside the planning grid"), GraphRevision);
    }

    if (!bDoorsInitialized)
    {
        // First use: apply the authored initial door state.
        ClosedDoors.Reset();
        for (const int32 DoorId : ClosedDoorsAtStart)
        {
            ClosedDoors.Add(DoorId);
        }
        bDoorsInitialized = true;
    }

    FString Error;
    ++GraphRevision;
    if (!BuildPlanner(StartCell, NewGoal, Error))
    {
        bHasGoal = false;
        return MakeFailure(EHimeraStatus::InvalidInput, Error, GraphRevision);
    }
    GoalCell = NewGoal;
    bHasGoal = true;
    ActiveDecisionId = 0;
    return Plan(StartCell);
}

bool UHimeraAITrooperComponent::OnDoorStateChanged(int32 DoorId, bool bOpen)
{
    const FIntPoint* Cell = DoorCells.Find(DoorId);
    if (!Cell)
    {
        UE_LOG(LogHimeraAI, Warning, TEXT("OnDoorStateChanged: unknown DoorId %d"), DoorId);
        return false;
    }
    if (bOpen) { ClosedDoors.Remove(DoorId); } else { ClosedDoors.Add(DoorId); }
    ++GraphRevision;
    ActiveDecisionId = 0; // the active route is stale

    if (Impl.IsValid() && Impl->Agent)
    {
        try
        {
            Impl->Agent->updateCell({Cell->X, Cell->Y}, IsCellBlocked(*Cell), CellRisk(*Cell));
        }
        catch (const std::exception& E)
        {
            UE_LOG(LogHimeraAI, Error, TEXT("OnDoorStateChanged: %s"), *ToFString(E.what()));
            return false;
        }
    }
    return true;
}

bool UHimeraAITrooperComponent::OnHazardStateChanged(int32 HazardId, bool bActive, float Risk)
{
    const FIntPoint* Cell = HazardCells.Find(HazardId);
    if (!Cell || Risk < 0.f || !FMath::IsFinite(Risk))
    {
        UE_LOG(LogHimeraAI, Warning, TEXT("OnHazardStateChanged: unknown HazardId %d or invalid risk"), HazardId);
        return false;
    }
    if (bActive) { ActiveHazards.Add(HazardId, Risk); } else { ActiveHazards.Remove(HazardId); }
    ++GraphRevision;
    ActiveDecisionId = 0;

    if (Impl.IsValid() && Impl->Agent)
    {
        try
        {
            Impl->Agent->updateCell({Cell->X, Cell->Y}, IsCellBlocked(*Cell), CellRisk(*Cell));
        }
        catch (const std::exception& E)
        {
            UE_LOG(LogHimeraAI, Error, TEXT("OnHazardStateChanged: %s"), *ToFString(E.what()));
            return false;
        }
    }
    return true;
}

FHimeraAgentDecision UHimeraAITrooperComponent::Replan(const FVector& StartWorld)
{
    if (!bHasGoal || !Impl.IsValid() || !Impl->Agent)
    {
        return MakeFailure(EHimeraStatus::InvalidInput, TEXT("No goal set: call SetGoal first"), GraphRevision);
    }
    FIntPoint StartCell;
    if (!Grid.WorldToCell(StartWorld, StartCell))
    {
        return MakeFailure(EHimeraStatus::InvalidInput, TEXT("Start is outside the planning grid"), GraphRevision);
    }
    return Plan(StartCell);
}

FHimeraAgentDecision UHimeraAITrooperComponent::Plan(const FIntPoint& StartCell)
{
    try
    {
        const double T0 = FPlatformTime::Seconds();
        Impl->Agent->moveStart({StartCell.X, StartCell.Y});
        const std::vector<himera::Point> Path = Impl->Agent->findPath();
        const double ElapsedMs = (FPlatformTime::Seconds() - T0) * 1000.0;

        if (ElapsedMs > PlannerBudgetMs)
        {
            UE_LOG(LogHimeraAI, Warning, TEXT("D* Lite update took %.2f ms (budget %.2f ms)"), ElapsedMs, PlannerBudgetMs);
            return MakeFailure(EHimeraStatus::PlannerTimedOut,
                FString::Printf(TEXT("Planner took %.2f ms, budget %.2f ms; keep the previous route"), ElapsedMs, PlannerBudgetMs),
                GraphRevision);
        }
        if (Path.empty())
        {
            ActiveDecisionId = 0;
            LastWaypoints.Reset();
            return MakeFailure(EHimeraStatus::Unreachable, TEXT("No route to the goal (blocked or goal unwalkable)"), GraphRevision);
        }

        FHimeraAgentDecision D;
        D.GraphRevision = GraphRevision;
        D.ActionName = TEXT("D*Lite");

        float Cost = 0.f;
        TArray<FVector> Waypoints;
        Waypoints.Reserve(static_cast<int32>(Path.size()));
        for (size_t I = 0; I < Path.size(); ++I)
        {
            const FIntPoint Cell(Path[I].x, Path[I].y);
            Waypoints.Add(Grid.CellToWorld(Cell));
            if (I > 0)
            {
                Cost += 1.f + CellRisk(Cell);
            }
        }
        D.PlanCost = Cost;
        D.Message = FString::Printf(TEXT("Route of %d cells, cost %.1f, %d nodes expanded, %.3f ms"),
            static_cast<int32>(Path.size()), Cost, Impl->Agent->lastExpansionCount(), ElapsedMs);

        if (Path.size() == 1)
        {
            D.Command = EHimeraCommand::NoAction; // already at the goal
            D.Message += TEXT(" (already at goal)");
            ActiveDecisionId = 0;
            return D;
        }

        D.Command = EHimeraCommand::FollowRoute;
        D.Destination = Waypoints.Last();
        D.Waypoints = Waypoints;

        D.bChanged = (Waypoints != LastWaypoints) || ActiveDecisionId == 0;
        LastWaypoints = Waypoints;
        if (D.bChanged)
        {
            ActiveDecisionId = NextDecisionId++;
        }
        D.DecisionId = ActiveDecisionId;
        return D;
    }
    catch (const std::exception& E)
    {
        return MakeFailure(EHimeraStatus::InvalidInput, ToFString(E.what()), GraphRevision);
    }
}

bool UHimeraAITrooperComponent::ReportActionResult(int64 DecisionId, EHimeraActionResult /*Result*/)
{
    if (DecisionId == 0 || DecisionId != ActiveDecisionId)
    {
        return false; // stale: graph changed or a newer route replaced it
    }
    ActiveDecisionId = 0;
    LastWaypoints.Reset(); // next Replan() is a fresh, "changed" decision
    return true;
}
