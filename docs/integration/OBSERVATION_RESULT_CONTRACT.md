# Observation / Result Contract

This is the contract between gameplay (Blueprints) and the HimeraAI plugin. A teammate should be
able to build the Blueprint action executor from this file alone. Names match the C++ types in
`Plugins/HimeraAI/Source/HimeraAI/Public/HimeraAgentTypes.h`.

Conventions: time = seconds (`double`), positions = `FVector` in cm, health/exposure/stress/
confidence = `0.0–1.0`. "Default" is used when the producer leaves a field unset.

## 1. Events (gameplay -> AI)

| Event | Producer | Fields (type, bounds) | Consumer |
|---|---|---|---|
| `PlayerSeen` | AIPerception sight | `Location` FVector; `Time` double >= 0 | Security, Hollow |
| `PlayerLost` | AIPerception sight | `LastKnownLocation` FVector; `Time` double | Security, Hollow |
| `GunshotHeard` | Weapon Blueprint noise | `Location` FVector; `Confidence` 0–1 (default 0.9); `MaxRange` cm > 0 (default 3000) | Security, Hollow |
| `DoorStateChanged` | Door Blueprint | `DoorId` int >= 0; `bOpen` bool; `GraphRevision` int | Trooper, Hollow |
| `HazardStateChanged` | Hazard volume | `HazardId` int >= 0; `bActive` bool; `Risk` >= 0 finite (default 0) | Trooper |
| `ExposureChanged` | Director/level | `Exposure01` 0–1 | Director |
| `MoveFinished` | AI Controller | `DecisionId` int64; `Result` enum | Brain, Trooper |
| `AttackFinished` | AI Controller | `DecisionId` int64; `Result` enum | Brain |

Sight and sound are **separate** events. `GunshotHeard` never sets "player visible".
`PlayerLost` keeps the last-known position and time; it does not reveal the live position.

## 2. Observations (state the brain needs)

### 2.1 Security (GOAP) — `FHimeraAgentObservation`

| Field | Type | Bounds / default | Producer | Consumer / GOAP fact |
|---|---|---|---|---|
| `Health01` | float | 0–1, default 1 | NPC health component | `LowHealth` when `< 0.30` |
| `Ammo` | int32 | >= 0, default 0 | NPC weapon state | `HasAmmo` when `> 0` |
| `bPlayerKnown` | bool | default false | Perception memory (seen within memory window) | `KnowsTarget` |
| `bUnderFire` | bool | default false | Damage event in last 3 s | `UnderFire` |
| `bInCover` | bool | default false | Executor (cover query) | `InCover` |
| `bHeardSound` | bool | default false | Un-investigated `GunshotHeard` | `HeardSound` |
| `Self`/`Target` positions | FVector | cm | Pawn / perception | Converted to cells only for Hollow/Trooper |
| `Time` | double | >= 0 | `GetTimeSeconds()` | Staleness checks |

The facts `Aimed`, `TargetSuppressed`, `SoundInvestigated`, `Patrolled`, `Retreated` are **planner
outputs**. They are never taken from observation; the executor reports results through
`AttackFinished` / `MoveFinished`.

### 2.2 Hollow (belief) — extra fields

| Field | Type | Bounds / default | Producer | Consumer |
|---|---|---|---|---|
| `SelfLocation` | FVector | on NavMesh | Pawn | `selectSearchCell(self)` |
| `Heard` | list of `GunshotHeard` | sigma default 1.5 cells | Weapon noise | `hear()` |
| `VisibleEmptyCells` | int32 cell list | in-grid | Perception sweep | `observeEmpty()` |
| `AllyTargets` | FVector list | cm | Other Hollows' current destinations | Search de-duplication |

### 2.3 Trooper (D* Lite) — see NAVIGATION_MAPPING.md

| Field | Type | Bounds / default | Producer | Consumer |
|---|---|---|---|---|
| `Start` | FVector -> cell | in-grid, walkable | Pawn | `moveStart()` |
| `Goal` | FVector -> cell | in-grid, walkable | Level / Hollow target | Planner goal |
| `Door` updates | `DoorStateChanged` | | Door BP | `updateCell(blocked)` |
| `Hazard` updates | `HazardStateChanged` | risk >= 0 | Hazard volume | `updateCell(risk)` |

### 2.4 Director — maps to `DirectorState`

| Field | Type | Bounds / default | Producer |
|---|---|---|---|
| `Health01` | float | 0–1, default 1 | Player health component |
| `Ammo` | int32 | >= 0, default 30 | Player weapon |
| `Exposure01` | float | 0–1, default 0 | Level controller |
| `Stress01` | float | 0–1, default 0 | Level controller |
| `ActiveHollows` | int32 | >= 0 | Director counts its own spawns |
| `SecondsSinceWave` | float | >= 0, default 100 | **Director measures real elapsed time itself** |
| `bSafeZone` | bool | default false | Trigger volume |

Out-of-range values are rejected (`InvalidInput`), not silently clamped, so wiring mistakes show up.

## 3. Output: `FHimeraAgentDecision`

| Field | Type | Meaning |
|---|---|---|
| `DecisionId` | int64 | Increases per agent. Executor echoes it in `*Finished` events |
| `Command` | `EHimeraCommand` | What to do |
| `Status` | `EHimeraStatus` | `Ok` or a failure state |
| `Destination` | FVector (cm) | Target for movement commands; zero otherwise |
| `Waypoints` | TArray<FVector> | Route for `FollowRoute`, world cm, start -> goal |
| `bChanged` | bool | False if same command/destination as the last decision |
| `GraphRevision` | int32 | Graph version the decision was computed on |
| `PlanCost` | float | GOAP plan cost / D* path cost / Director utility |
| `ActionName` | FString | Raw planner step (e.g. `AcquireLineOfSight`) for debug |
| `Message` | FString | Human-readable reason or error |

### Commands (`EHimeraCommand`)

`NoAction`, `Patrol`, `Investigate`, `MoveToCover`, `Attack`, `Reload`, `Retreat`, `Search`,
`TriggerExposureEvent`, `FollowRoute`.

GOAP step -> command: `TakeCover`->`MoveToCover`, `Reload`->`Reload`,
`AcquireLineOfSight`->`Attack` (move/aim, do not fire yet), `SuppressTarget`->`Attack`,
`InvestigateSound`->`Investigate`, `PatrolSector`->`Patrol`, `Retreat`->`Retreat`.
Hollow `selectSearchCell` -> `Search`. D* path -> `FollowRoute`. Director choice ->
`TriggerExposureEvent` (the chosen `Encounter` is in `ActionName`: `RadioWhisper`,
`VisualDistortion`, `HollowWave`; `Quiet` -> `NoAction`).

### Status / failure states (`EHimeraStatus`)

| Status | When | Executor should |
|---|---|---|
| `Ok` | Normal decision | Execute |
| `Unreachable` | No path / goal blocked / search cell unreachable | Hold position or fall back to `Patrol`; do not teleport |
| `InvalidInput` | Bounds violated, cell outside grid, bad state | Log, fix the wiring; command is `NoAction` |
| `TargetDestroyed` | Target actor no longer valid | Drop the target, wait for new perception |
| `PlannerTimedOut` | Update exceeded time budget | Keep previous decision; retry next tick |
| `NoSafeEvent` | Director chose `Quiet` (nothing to dispatch); `Command` is `NoAction` | Do nothing |

### Result codes reported by the executor (`EHimeraActionResult`)

`Succeeded`, `Failed`, `Cancelled`, `Blocked`. A `Failed` or `Blocked` move triggers an immediate
replan; `Cancelled` does not.

## 4. Blueprint executor checklist

1. Call the brain's decision function (timer, or on an event).
2. If `bChanged` is false, do nothing.
3. If `Status != Ok`, handle per the table above.
4. Cancel the current action, start the new one for `Command`.
5. On completion, call `ReportActionResult(DecisionId, Result)`.
