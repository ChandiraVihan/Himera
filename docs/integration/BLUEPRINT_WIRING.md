# Blueprint wiring guide

How to connect the game to the `HimeraAI` plugin. It follows `OBSERVATION_RESULT_CONTRACT.md`.

**Read this first:** the Blueprint graphs (`.uasset` files) were not opened while writing this
guide, so it does not describe existing nodes or pin layouts. Node names below are standard Unreal
nodes and the plugin's own functions. Check names and pins in the editor and fix this file where
they differ. Existing Blueprints named in the repo: `BP_FirstPersonCharacter`,
`BP_FirstPersonPlayerController`, `BP_FirstPersonGameMode`, `BP_FirstPersonCameraManager`, input
action `IA_Shoot`, `IA_Reload`. **Do not edit these** until the new API is agreed with their owner.

The plugin never moves a pawn or fires a weapon. Blueprint does, then reports back.

## 1. Per-NPC setup

1. Give each NPC a `UHimeraAgentBrainComponent` (Security or Hollow) or `UHimeraAITrooperComponent`.
2. Brain: set `Role`. Hollow only: call `InitializeHollow(Grid, WallCells)` in `BeginPlay`.
3. Trooper: set `Grid`, `WallCells`, `DoorCells`, `HazardCells` (values in `LV_AI_TEST_SETUP.md`),
   then call `SetGoal(StartWorld, GoalWorld)`.
4. In the AI Controller `Event BeginPlay`, **Set Timer by Event** every **0.2 s**, looping, that runs
   the decision loop in section 7. Do not use Event Tick.

## 2. Sight: AI Perception -> `ReportPlayerSeen` / `ReportPlayerLost`

1. On the AI Controller add `AIPerception` with an **AI Sight** config (detect enemies/neutrals as
   needed, set sight radius, lose-sight radius and max age).
2. Bind **On Target Perception Updated** (inputs: `Actor`, `Stimulus`).
3. Branch on `Stimulus > Was Successfully Sensed` and check the sense is Sight
   (`Get Sense Class for Stimulus`).
   - True: `Brain > ReportPlayerSeen(Location = Stimulus > Stimulus Location, TimeSeconds = Get Game Time in Seconds)`.
   - False: `Brain > ReportPlayerLost(LastKnownLocation = Stimulus > Stimulus Location, TimeSeconds = Get Game Time in Seconds)`.
4. Keep `bPlayerKnown` in your observation true only while Perception still remembers the target
   (use `Get Known Perceived Actors` or the Max Age setting).

Do not read the player's live location to fill the observation. That would be omniscience.

## 3. Sound: weapon noise -> `ReportGunshot`

Needs agreement with the owner of the weapon / `BP_FirstPersonCharacter`. Proposed way that does
not change the existing events: add a **new** small Blueprint component or interface, called from
the weapon's fire event (the place `IA_Shoot` leads to), that does:

1. `Get All Actors with Tag` (tag `HimeraNPC`) or `Get All Actors of Class`.
2. For each NPC: `Get Component by Class (UHimeraAgentBrainComponent)`, and if valid call
   `ReportGunshot(Location = muzzle/impact location, Confidence = 0.9, MaxRangeCm = 3000)`.
   The component ignores shots farther than `MaxRangeCm` and returns false.
3. Optional: also call **Report Noise Event** (Loudness 1.0, Max Range 3000) so normal AI Hearing
   can be used later.

A gunshot behind a wall is allowed to reach the AI. It only gives a position to investigate, never
"player visible".

## 4. Doors -> `OnDoorStateChanged` and the real navigation

On the door Blueprint (name to be agreed with the Core Developer):

1. Variables: `DoorId` (int, unique), `bOpen` (bool), a mesh with collision.
2. Mesh setting **Can Ever Affect Navigation = true**, and Project Settings, Navigation Mesh,
   **Runtime Generation = Dynamic**.
3. In the Open / Close events, in **the same event**:
   - toggle collision (open: no blocking collision; closed: blocking),
   - for every Trooper component in the level: `OnDoorStateChanged(DoorId, bOpen)`,
   - for every Hollow brain: `ReportCellBlocked(DoorCell, NOT bOpen)`,
   - then ask each Trooper for a new route: `Replan(Pawn Location)`.
4. Press **P** after closing a door to confirm the NavMesh really changed.

If only the plugin is updated the Trooper walks into a shut door; if only Unreal is updated its plan
is wrong. Do both.

## 5. Spill / hazard -> `OnHazardStateChanged`

1. Hazard Blueprint with `HazardId` (int) and a toggle.
2. On toggle: for every Trooper component call `OnHazardStateChanged(HazardId, bActive, Risk = 5.0)`,
   then `Replan(Pawn Location)`.
3. Optional: set a NavArea with higher cost in the volume so Unreal's own pathing agrees. The cell
   stays walkable.

## 6. Director

1. From one actor (the level Blueprint is fine), every **1 s**: `Get World Subsystem` ->
   `HimeraDirectorSubsystem` -> `Evaluate(Health01, Ammo, Exposure01, Stress01, bSafeZone)`.
   Fill the inputs from the player's real state (not from NPCs).
2. Switch on the result:
   - `Status = NoSafeEvent` -> do nothing.
   - `Command = TriggerExposureEvent` -> switch on `ActionName`: `RadioWhisper` (audio),
     `VisualDistortion` (post-process, for example the Bodycam materials), `HollowWave` (spawn Hollows).
3. After Hollows really spawn: `ReportWaveSpawned(count)`. When one dies or despawns:
   `ReportHollowRemoved()`. The Director counts the wave cooldown from world time itself.
4. Weather events: the World Builder's weather controller (see `HANDOFF_NOTES.md`) listens for
   the encounter name; the Director never talks to it directly.

## 7. Action executor loop (per NPC, runs on the timer)

```
Decision = Brain.Decide(Observation)          (Trooper: Replan(Pawn Location))
if Decision.Status != Ok:
      Unreachable      -> stop and hold, or Patrol; do NOT teleport
      InvalidInput     -> Print String the Message, fix the wiring
      TargetDestroyed  -> forget the target
      PlannerTimedOut  -> keep the current action
      NoSafeEvent      -> do nothing
elif Decision.bChanged == false: do nothing (keep the current action)
else:  Stop the current action, then switch on Decision.Command:
      Patrol / Investigate / Search -> AI MoveTo (Destination)
      FollowRoute   -> AI MoveTo (Waypoints[1])   (next point only; re-plan when it finishes)
      MoveToCover   -> AI MoveTo cover point (EQS or authored cover actors)
      Attack        -> turn/aim; fire only when ActionName == SuppressTarget
      Reload        -> play reload montage
      Retreat       -> AI MoveTo away from the threat (authored safe point)
      NoAction      -> idle
When the action ends:  Brain.ReportActionResult(Decision.DecisionId, Succeeded/Failed/Cancelled/Blocked)
                       (use the Trooper's ReportActionResult for routes), then run the loop again.
```
A stale `DecisionId` is ignored by the plugin, so late callbacks are safe. On every decision
`Print String` the fields `Command`, `ActionName`, `PlanCost`, `Status`, `Message` for the debug display.

## 8. Checklist

- [ ] Timer set (0.2 s), no Tick.
- [ ] Sight and sound reported separately.
- [ ] Door and spill events update the plugin **and** Unreal navigation.
- [ ] Executor consumes the decision and reports the result.
- [ ] Only one place calls the Director.
