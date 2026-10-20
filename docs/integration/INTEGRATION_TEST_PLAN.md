# Integration Test Plan

Level: `LV_AI_Test` (graybox, separate from the production forest/lab map).
Status: **plan only**. Nothing below has been run in Unreal yet; fill in the result column when it is.

## 1. Level checklist

- [ ] Floor, two rooms, a hallway, two alternative routes, one door, one hazard area.
- [ ] `Nav Mesh Bounds Volume` covering all paths (press **P** to check).
- [ ] Player Start using `BP_FirstPersonCharacter`.
- [ ] Placeholder meshes: Security, Hollow, Trooper.
- [ ] One `HimeraDirector` (world subsystem; nothing to place) and one `HimeraGridOrigin` actor.
- [ ] Debug overlay or log rows per agent: goal, observations, plan cost / belief / path cost / utility, action, outcome.

## 2. Standalone regression (must stay green)

| # | Check | Command | Expected |
|---|---|---|---|
| R1 | Core builds without Unreal | `cmake -B build && cmake --build build` | success |
| R2 | Original tests pass | `ctest --test-dir build --output-on-failure` | all pass |
| R3 | Vendored core matches AI repo | `python Tools/sync_core.py --check <ai-repo>` | no drift |

## 3. Acceptance scenarios

| # | Scenario | Expected | Result |
|---|---|---|---|
| 1 | Fire gunshot behind a wall | Security and Hollow receive a sound observation; neither gets `bPlayerKnown`. Security `Investigate`, Hollow belief shifts | |
| 2 | Security sees player | GOAP plans cover/aim/attack; replans on ammo 0 (`Reload`) and health < 0.30 (`Retreat`) | |
| 3 | Close door while Trooper approaches | Route changes to the other corridor; pawn walks via NavMesh; no teleporting | |
| 3b | Close both routes | `Unreachable`; pawn holds, no crash | |
| 4 | Turn hazard on | Path cost rises; safe route preferred while hazard cell stays walkable; clearing it restores the short route | |
| 5 | Low player health / safe zone | Director returns no `HollowWave`; `VisualDistortion`/`RadioWhisper` dispatch still works | |
| 6 | All four agents for 5+ minutes | No crash, no frozen AI, exactly one Director, no severe frame spikes | |

## 4. Unit-level checks for the Unreal layer

- Cell conversion round-trips: `WorldOfCell(Cell(p))` is within half a cell of `p`.
- Out-of-grid position -> `InvalidInput`, no exception leaks.
- Decision after the same inputs twice -> second has `bChanged = false`.
- Stale `DecisionId` result is ignored.
- Destroyed target actor -> `TargetDestroyed`.
- Two NPCs with brain components do not share state (change one, the other is unaffected).
- Two calls to `GetDirector` in one world return the same object.

## 5. Measurements for the viva

Record per agent over the 5-minute run: decision rate (Hz), time per decision (microseconds,
mean and worst), frame time with all four active, and number of stale/cancelled decisions. For
D* Lite vs fresh A* on the same synthetic scenarios (done in the AI repo): path validity, cost,
expanded nodes, planning microseconds and variance. Fewer expansions are reported as fewer
expansions, not as "faster".

## 6. Evidence to attach to each checkpoint

Screenshot or log of the scenario, the build log for the plugin, and the test output.
