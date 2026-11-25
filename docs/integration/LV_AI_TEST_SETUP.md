# LV_AI_Test: graybox level setup

Steps and numbers to build the small AI test level in Unreal. **Not built or run yet**: someone with
the editor has to do it. The route numbers below are checked by the `testLevelLayout` test in
`Tools/tests/trooper_tests.cpp` (they come from the real D* Lite core, not from guesses).

Rules: make a **new** level `Content/Maps/LV_AI_Test` (or similar). Do not change
`Lvl_FirstPerson.umap` or the production forest/lab maps. Use One File Per Actor if the team has
it on, and agree who owns the `.umap`.

## 1. Grid numbers (used by the Trooper and Hollow components)

| Setting | Value |
|---|---|
| Width x Height | 12 x 8 cells |
| Cell size | 200 cm (so the level floor is 2400 x 1600 cm) |
| Origin (`OriginCm`) | (0, 0, 0) = corner of cell (0,0) |
| Cell centre in world cm | X = (cx + 0.5) x 200, Y = (cy + 0.5) x 200 |

## 2. Layout (cell coordinates)

```
 y\x  0 1 2 3 4 5 6 7 8 9 10 11
  0   # # # # # # # # # # #  #
  1   # . . . . D H . . . .  #     top corridor: D = door (5,1), H = spill (6,1)
  2   # . . . # # # # . . .  #
  3   # . S . # # # # . . G  #     S = start (2,3), G = goal area (9,3)
  4   # . . . # # # # . . .  #
  5   # . . . # # # # . . .  #
  6   # . . . . . . . . . .  #     bottom corridor (long route), no door
  7   # # # # # # # # # # #  #
```
`#` = wall (blocked). Room A = x 1-3, y 1-6. Room B = x 8-10, y 1-6. The block x 4-7, y 2-5 is solid.
The two routes between the rooms are the top corridor (y = 1) and the bottom corridor (y = 6).

**WallCells** to enter in the Trooper component (52 cells):
- x = 0 and x = 11 for y = 0..7 (16 cells)
- y = 0 and y = 7 for x = 1..10 (20 cells)
- x = 4..7 for y = 2..5 (16 cells)

| Component setting | Value |
|---|---|
| `DoorCells` | `{ 1 : (5,1) }` |
| `HazardCells` | `{ 1 : (6,1) }` |
| `ClosedDoorsAtStart` | empty (door starts open) |
| Hazard risk when active | 5.0 (extra cells of travel) |

World positions of the key cells: door (5,1) = (1100, 300); spill (6,1) = (1300, 300); Room A
start (2,3) = (500, 700); goal (9,3) = (1900, 700).

## 3. Build steps in the editor

1. New empty level, save as `LV_AI_Test`.
2. **Floor:** a cube or plane covering X 0 to 2400, Y 0 to 1600, Z = 0, with collision.
3. **Walls:** cubes 300 cm high that fill the wall cells above (use one stretched cube per solid
   run, for example one cube for the 4 x 4 block). The border walls close the level.
4. **Door:** a Blueprint actor in the top corridor at cell (5,1) with a mesh, collision, a
   `DoorId` variable = 1, and **Can Ever Affect Navigation = true** (see the wiring guide).
5. **Spill:** a box trigger volume at cell (6,1) (200 x 200 cm) plus a visible decal or plane, with
   `HazardId` = 1 and a toggle that switches it on and off.
6. **NavMesh:** one `Nav Mesh Bounds Volume` centred at (1200, 800, 100) with size at least
   2600 x 1800 x 400 cm. Press **P** in the viewport: all corridors and both rooms should be green.
   In Project Settings, Navigation Mesh, set **Runtime Generation = Dynamic** (needed for the door).
7. **Player:** a Player Start in Room A at about cell (1,6) = (300, 1300), using the existing
   `BP_FirstPersonCharacter` game mode. Do not edit the player Blueprints.
8. **Placeholder NPCs** (cubes or the mannequin are fine), each with a brain or Trooper component:

| NPC | Component | Start cell | World cm |
|---|---|---|---|
| Trooper | `UHimeraAITrooperComponent` | (2,3) | (500, 700) |
| Security | `UHimeraAgentBrainComponent`, Role = Security | (9,5) | (1900, 1100) |
| Hollow | `UHimeraAgentBrainComponent`, Role = Hollow | (9,2) | (1900, 500) |

   Call `InitializeHollow` on the Hollow with the same grid and `WallCells`.
9. **Director:** nothing to place. `UHimeraDirectorSubsystem` is created by Unreal for the world.
   Call it from one actor, for example the level Blueprint.
10. **Debug output:** on every decision, `Print String` the fields `Command`, `ActionName`,
    `PlanCost`, `Status` and `Message` (key = the NPC name) so they show on screen and in the log.

## 4. Expected results for the acceptance scenarios

| # | Do this | Expect |
|---|---|---|
| 1 | Fire a gunshot behind a wall | Security `Investigate` and Hollow `Search`; neither gets `bPlayerKnown` |
| 2 | Let Security see the player | `Attack` steps (`AcquireLineOfSight`, then `SuppressTarget`); ammo 0 gives `Reload`; health below 0.30 gives `Retreat` |
| 3 | Trooper at start (2,3), goal (9,3), then close the door | Route cost 11 through the top corridor changes to **13** through the bottom corridor; the pawn walks it with navigation, no teleport |
| 3b | Also close a second door in the bottom corridor (optional, `DoorId` 2 at (5,6)) | `Unreachable`; the pawn holds still |
| 4 | Door open, spill on (risk 5) | Route flips to the bottom corridor, cost 13. With risk 1 it stays on top at cost 12. The spill cell stays walkable |
| 5 | Player health low, or inside a safe zone | Director never returns `HollowWave`; `VisualDistortion` / `RadioWhisper` can still happen |
| 6 | Run all four for 5+ minutes | No crash, no frozen AI, one Director, no big frame spikes |

Fill in the result column of `INTEGRATION_TEST_PLAN.md` only for what was really run.
