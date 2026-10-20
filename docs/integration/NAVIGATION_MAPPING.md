# Navigation Mapping (D* Lite graph <-> Unreal navigation)

Unreal NavMesh and D* Lite are different systems. The Trooper's planner decides **which route**
on a small authored graph. Unreal navigation then moves the pawn physically between the returned
world-space waypoints. Neither replaces the other.

## 1. Graph representation (phase 1)

The existing core planner (`DStarLiteAgent`) works on a `Grid` of cells. For the first
integration, each cell is an **authored waypoint node**, not a physical floor tile:

- `LV_AI_Test` gets a coarse grid (start with about 12 x 8), origin at a `HimeraGridOrigin` actor.
- Cell size is a level setting, default **200 cm**.
- Rooms, hallway and the two alternative routes are drawn as walkable cells. Walls are blocked.
- A door is **one cell** (or the cell pair across the doorway) with a known `DoorId`.
- A hazard is one or more cells with a known `HazardId`.

A true room/door/waypoint graph with arbitrary edges is a later extension of the planner (part of
the Trooper's own work in the AI repo). The mapping below is written so that only the adapter
changes when that happens.

## 2. Coordinate conversion

```text
Cell = ( floor((World.X - Origin.X) / CellSize),
         floor((World.Y - Origin.Y) / CellSize) )

WorldOfCell = Origin + ( (Cell.X + 0.5) * CellSize, (Cell.Y + 0.5) * CellSize, 0 )
```

- Z is ignored by the grid. The executor projects each waypoint onto the NavMesh before use
  (`UNavigationSystemV1::ProjectPointToNavigation`). If projection fails the waypoint is
  reported as `Unreachable`.
- A world position outside the grid returns `InvalidInput`.
- A position inside a blocked cell is `Unreachable` for goals; for the start it is `InvalidInput`.

## 3. Cost semantics

From the core: moving into a walkable cell costs `1.0 + risk(cell)`; a blocked cell costs
infinity. So:

| State | Planner | Unreal |
|---|---|---|
| Door open | cell walkable, risk 0 | door collision off / NavModifier removed |
| Door closed | `updateCell(blocked=true)` | collision on; NavMesh updated (dynamic nav modifier / nav link disabled) |
| Hazard active | `updateCell(blocked=false, risk=R)` | still walkable; optional NavArea cost raised |
| Hazard off | `updateCell(risk=0)` | NavArea back to default |

`R` is in "extra cells of travel": `R = 5` means the hazard cell is as bad as 5 additional steps.
The default hazard risk is **5.0**, tuned with the test plan.

The heuristic used by the planner is Manhattan distance, which is admissible for unit-cost 4-connected
movement because every step costs at least 1.

## 4. Door and hazard update flow

```text
Door BP: OnOpened/OnClosed
   -> Trooper component: OnDoorStateChanged(DoorId, bOpen)
        -> look up cell for DoorId
        -> GraphRevision++
        -> DStarLiteAgent::updateCell(cell, blocked = !bOpen, risk = 0)
        -> mark active decision stale, replan (event-driven)
   -> Door BP (same event): toggle collision + update NavMesh
```

Both sides must happen in the same event. If only the planner is updated the Trooper walks into a
shut door; if only Unreal is updated the plan is wrong. The Door Blueprint owner (Core Developer)
calls one function on the Trooper component.

## 5. Route following

1. Component returns `FollowRoute` with `Waypoints` (cell centres in cm, start -> goal).
2. Executor calls AI MoveTo for the **next** waypoint only (not the whole route), so a graph
   change mid-route can redirect it.
3. On `MoveFinished(Succeeded)` the component calls `moveStart(cell of pawn)` and replans from the
   current position. This keeps D* Lite's `km` offset correct.
4. On `MoveFinished(Blocked/Failed)` it replans immediately. If still no path -> `Unreachable`.

## 6. Moving the goal

D* Lite repairs the graph for a **fixed goal**. If the goal moves, the core's cost values are no
longer valid, so the adapter rebuilds the planner (`ResetPlanner`) instead of pretending it is
incremental. Goal changes should therefore be rare (player re-sighted at a new room), not every
tick.

## 7. Known limits

- Grid cell size limits path precision; waypoints are cell centres, not exact geometry.
- The planner has no notion of agent radius; the NavMesh handles that during MoveTo.
- Both-routes-blocked yields an empty path -> `Unreachable`, not a teleport.
