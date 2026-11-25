# HIMERA AI Integration — Architecture

Owner: Systems Engineer / Architect. Status: **design (Package A)**. Nothing here is claimed as
working in Unreal until it has been tested in the editor.

Baseline AI core: [HIMERA-Autonomous-AI](https://github.com/ChandiraVihan/HIMERA-Autonomous-AI),
branch `feature/v0.4-ai-evaluation` (commit `d55704d`).

## 1. Layers and ownership

```text
                    UNREAL WORLD (UE 5.8)
   BP_FirstPersonCharacter / Level / AI Perception / Doors / Hazards
                            |
                  observations and events
                            v
   +------------------------------------------------------+
   |  INTEGRATION LAYER   (Plugins/HimeraAI, UE C++)      |
   |  FHimeraAgentObservation   UHimeraAgentBrainComponent |
   |  UHimeraAITrooperComponent UHimeraDirectorSubsystem   |
   |  - converts FVector (cm) <-> graph cell IDs           |
   |  - validates and clamps inputs                        |
   |  - owns one stateful planner per NPC                  |
   +------------------------------------------------------+
                            |  plain C++ calls (no UE types)
                            v
   +------------------------------------------------------+
   |  HIMERA ALGORITHM CORE   (Source/HimeraAI/Core)      |
   |  GOAP | Hollow belief | D* Lite | Exposure Director  |
   +------------------------------------------------------+
                            |
                  FHimeraAgentDecision
                            v
   +------------------------------------------------------+
   |  ACTION EXECUTOR   (Blueprint / AI Controller)       |
   |  AI MoveTo, aim, fire, reload, montage, spawn, FX    |
   +------------------------------------------------------+
                            |
          MoveFinished / AttackFinished / ... events
                            |
                            +--> back into the Integration Layer
```

| Layer | Owns | Must NOT |
|---|---|---|
| Algorithm core | Decision logic, planner state, costs | Include any Unreal header, hold an Actor pointer |
| Integration layer | Unit conversion, validation, per-NPC state, staleness checks | Move pawns, fire weapons, play animations |
| Action executor (BP) | Movement, combat, animation, spawning, completion reports | Decide *what* to do |
| Level / gameplay | Doors, hazards, perception, weapon noise | Talk to the core directly |

The core stays buildable with the standalone CMake project, so the AI team can keep running
`ctest` without Unreal. The Unreal module compiles the same sources with UnrealBuildTool.

## 2. Key rules

1. **No global brain.** Each NPC owns its own brain component. Only the Director is shared, and
   it is a `UWorldSubsystem`, so there is exactly one per world.
2. **A decision is not an outcome.** `Attack` does not mean a bullet was fired. The executor must
   report `AttackFinished` / `MoveFinished` with a result.
3. **No omniscience.** The brain only sees what the sensors reported. Losing sight does not
   reveal the live player position; the last known position and its timestamp are kept.
4. **Planner graph and Unreal navigation change together.** Closing a door updates the D* Lite
   graph *and* blocks the NavMesh/collision. Planner correctness alone is not enough.
5. **Fail loudly, not silently.** Bad input returns `InvalidInput` and is logged; the core's
   exceptions never cross into Blueprint (see §5).

## 3. Units and conventions

| Quantity | Unit / range | Notes |
|---|---|---|
| Time | seconds, `double`, `UWorld::GetTimeSeconds()` | Never frames. Simulation time, not real time |
| World position | `FVector`, **centimetres** (UE default) | Only used outside the core |
| Graph position | `int32` cell X/Y (core `Point`) | Converted by the trooper/hollow adapters; see NAVIGATION_MAPPING.md |
| Health | `0.0 – 1.0` (`health01`) | Executor normalises `Health / MaxHealth` |
| Ammo | integer `>= 0` | Rounds in magazine + reserve as agreed by weapon owner |
| Exposure, stress | `0.0 – 1.0` | Owned by the Director / level controller |
| Sight / hearing | positions + timestamp + confidence `0–1` | Sight from AIPerception; hearing from weapon noise events |
| Actor lifetime | `TWeakObjectPtr` for any actor kept across ticks | A destroyed target yields `TargetDestroyed` |

## 4. Decision timing

- Tactical brains (Security, Hollow, Trooper): start at **5 Hz** (every 0.2 s) from a
  timer (Blueprint *Set Timer by Event*), not `Tick`. The components have ticking turned off. Allowed range while tuning: 5–10 Hz.
- Event-driven replanning on `DoorStateChanged`, `HazardStateChanged`, `PlayerSeen`,
  `PlayerLost`, `GunshotHeard`, `MoveFinished` and `AttackFinished`.
- Director: evaluated every **1 s**, and on `ExposureChanged`.
- Hollow `predict()` runs on the Hollow's own timer (default 1 Hz), not every decision.
- These rates are starting points. They get measured in the 4-agent playtest (INTEGRATION_TEST_PLAN.md
  scenario 6) and adjusted from the numbers, not from assumption.

## 5. Plan cancellation and stale decisions

Every decision carries a monotonically increasing `DecisionId` and the `GraphRevision` it was
computed against.

- A decision is **stale** if the executor reports a result for an older `DecisionId`, the graph
  revision changed (door/hazard), or the target actor is gone. Stale results are ignored.
- A new decision for the same NPC **cancels** the previous one: the executor must stop the old
  `MoveTo` before starting the new one.
- The component keeps the last decision and compares: if the new `Command` and `Destination`
  (within 50 cm) match, the result is flagged `bChanged = false` so the executor does not restart
  the same move every tick.
- Command completion (`MoveFinished` / `AttackFinished`) clears the active decision and
  triggers an immediate replan.
- Core exceptions (`std::invalid_argument`, `std::out_of_range`, `std::runtime_error`,
  `std::logic_error`) are caught in the integration layer and turned into `InvalidInput`
  or `Unreachable` with a message. They never reach Blueprint. This needs
  `bEnableExceptions = true` in `HimeraAI.Build.cs`.
- Planner work is bounded: grids are small authored graphs, and a time budget check marks a
  slow update as `PlannerTimedOut` instead of freezing the game thread.

## 6. Who talks to whom

| Producer | Event | Consumer |
|---|---|---|
| AIPerception (sight) | `PlayerSeen`, `PlayerLost` | Security brain, Hollow brain |
| Weapon Blueprint | `GunshotHeard` | Security brain, Hollow brain |
| Door Blueprint | `DoorStateChanged` | Trooper (and Hollow `setBlocked`) + NavMesh |
| Hazard volume | `HazardStateChanged` | Trooper (risk cost) |
| Health/exposure | `ExposureChanged` | Director subsystem |
| AI Controller | `MoveFinished`, `AttackFinished` | Brain / Trooper component |

Exact field names, types and bounds are in `OBSERVATION_RESULT_CONTRACT.md`.

## 7. Source-of-truth for the core

The core is **vendored** into `Plugins/HimeraAI/Source/HimeraAI/Core/`. The file
`Core/VENDORED_FROM.txt` records the AI-repo commit, and `Tools/sync_core.py` copies from a local
AI-repo checkout and can `--check` for drift. The AI repo remains the single source of truth;
nobody edits `Core/` by hand.

## 8. Not in scope here

Forest/lab level art (World Builder), character animation (Core Developer), and the player's
existing Blueprints, which are left untouched.
