# Trooper (D* Lite): how it works and how it was tested

Plain-language notes for the viva. Code: `Plugins/HimeraAI/Source/HimeraAI/Core/{include/himera/dstar.hpp,src/dstar.cpp}`
(vendored from the AI repo, not changed here). Tests: `Tools/tests/`.

## 1. The idea in one paragraph

A normal A* plans a route once. If a door closes, it throws the plan away and starts again.
D* Lite keeps its working notes and, when the map changes, only fixes the parts of those notes
that the change made wrong. It searches **backwards, from the goal to the start**, so the notes
stay useful while the Trooper walks forward.

## 2. The terms

| Term | Meaning | In the code |
|---|---|---|
| `g(s)` | The best known cost from cell `s` to the goal. Starts at infinity | `g_[i]` |
| `rhs(s)` | A one-step look-ahead: the cheapest of (cost of stepping to a neighbour + that neighbour's `g`). The goal has `rhs = 0` | `rhs_[i]`, set in `updateVertex` |
| Consistent cell | `g(s) = rhs(s)`: the notes for this cell are correct | `updateVertex` only queues cells where they differ |
| Over-consistent | `g(s) > rhs(s)`: a cheaper route was found; lower `g` | `g_[u] = rhs_[u]` |
| Under-consistent | `g(s) < rhs(s)`: the old cost is too optimistic (something got blocked); set `g` to infinity and recompute | `g_[u] = INF` |
| Priority key | A pair of numbers that decides which cell to fix first: `[ min(g, rhs) + h(start, s) + km , min(g, rhs) ]`. Smaller is first; the second number breaks ties | `calculateKey` |
| `h` (heuristic) | Manhattan distance from the **current start** to the cell | `manhattan(start_, p)` |
| `km` | A running offset that grows by the distance the start has moved. It keeps old keys comparable after the Trooper moves, so the queue need not be rebuilt | `km_ += manhattan(old, new)` in `moveStart` |

Cost rule in this project: entering a walkable cell costs `1 + risk(cell)`. A blocked cell costs
infinity. A hazard is therefore a cost, not a wall.

## 3. Why the heuristic is allowed (admissible)

Every step costs at least 1 and moves one cell in a 4-neighbour grid, so the real cost between two
cells is never less than their Manhattan distance. The heuristic never over-estimates, which is
what keeps the result optimal. Hazards only add cost, so the claim still holds.

## 4. What happens when something changes

1. `updateCell(cell, blocked, risk)` changes the cell and calls `updateVertex` on it and its
   four neighbours (their look-ahead may now be wrong). Cells whose `g` and `rhs` now differ go
   into the queue.
2. `findPath()` runs `computeShortestPath()`. It keeps taking the smallest key and fixing cells
   until the start cell is consistent and nothing in the queue could improve it.
3. The path is read forward from the start by always stepping to the neighbour with the lowest
   `step cost + g`.
4. `moveStart(p)` adds the distance moved to `km` and sets the new start. No re-planning of the
   whole map is needed.
5. If the **goal** changes, the stored values are no longer valid. The Unreal adapter builds a
   new planner (`SetGoal`). This is a limit, not a trick: D* Lite repairs changes for a fixed goal.

## 5. How it was tested

Run (no Unreal needed):

```
cd Tools/tests
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

| Test | What it checks |
|---|---|
| Baseline | Open map: valid path, cost 8, same cost as A* |
| Closed alternative route | Closing door A moves the route to door B; reopening works |
| Both routes blocked | Empty path (`Unreachable`), route returns when a door reopens |
| Rapid repeated door changes | 500 random door toggles; after every change D* cost equals a fresh A* cost |
| Hazard cost up and down | Hazard is avoided when a cheaper route exists; cost adds up; a hazard is not a wall |
| Moving start | The pawn walks the route while a door closes; each step matches fresh A* |
| Unreachable destination | Sealed goal, goal blocked later, goal outside grid (see note below) |
| Same-cost ties | Valid path, same answer every run, exactly one of two equal routes |
| LV_AI_Test layout | The 12 x 8 level map in `LV_AI_TEST_SETUP.md`: cost 11 open, 13 with the door closed, 13 / 12 with the spill at risk 5 / 1, no route with both corridors closed |

All 51 checks pass. The `VendoredCoreDriftCheck` test is **skipped** unless the environment
variable `HIMERA_AI_REPO` points to a checkout of the AI repo (then it runs
`Tools/sync_core.py --check`; verified once against commit `d55704d`).

Note for the Unreal adapter: a goal that is inside a wall makes the core's constructor throw
(`Start and goal must be walkable`). `UHimeraAITrooperComponent::SetGoal` catches it and returns
`InvalidInput`. A goal that becomes blocked **after** the planner exists returns an empty path,
which the component reports as `Unreachable`.

## 6. D* Lite vs fresh A*

`astar_compare` runs both on the same 200 random 40 x 40 grids per scenario (20% walls, fixed
seeds). "D* time" includes the map updates; the first plan is measured separately, because D*
needs it before any repair. Raw numbers: `Tools/tests/results.csv`.

| Scenario | Runs reachable | D* valid / cost = A* | D* expanded (mean) | A* expanded (mean) | D* us mean (sd) | A* us mean (sd) |
|---|---|---|---|---|---|---|
| First plan | 200 | 200 / 200 | 833 | 465 | 556 (144) | 69 (47) |
| 1 route cell blocked | 196 | 196 / 196 | 28 | 467 | 34 (84) | 74 (26) |
| 10 random cell changes | 199 | 199 / 199 | 33 | 467 | 55 (105) | 87 (30) |
| Start moved 5 steps | 200 | 200 / 200 | 0 | 435 | 10 (8) | 79 (32) |

(Numbers from one run on a Linux machine, `-O2`. Timings change from run to run.)

### What these numbers do and do not show

- **Correctness:** on all 800 runs D* Lite and A* agreed on whether a route exists, and every D*
  path was valid with exactly the A* cost.
- **First plan:** D* Lite expanded more cells and took longer than A*. It does extra work up front
  so that later repairs are cheap.
- **After a change:** D* Lite expanded far fewer cells (about 30 against about 465).
- **Time:** the time per update is small for both (tens of microseconds on a 40 x 40 grid). The
  spread (sd) is large, in one row larger than the mean, so the time comparison is not
  conclusive. Fewer expansions is **not** proof of less runtime, and this document does not claim
  that D* Lite is faster.
- **Start moved:** the 0 expansions means the earlier plan already covered the new start; it is
  not a general speed result.
- **Limits:** one grid size, one wall density, one machine, no Unreal. Planning time for the real
  level should be measured inside Unreal with the 4-agent playtest (see `INTEGRATION_TEST_PLAN.md`).
