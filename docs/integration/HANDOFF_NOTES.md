# Handoff notes

Short notes for the two teammates whose work touches the AI integration. Everything here is a
**proposal to agree on**, because their Blueprints are not in the repo yet and none of this has
been tried in Unreal. Details are in `BLUEPRINT_WIRING.md` and `OBSERVATION_RESULT_CONTRACT.md`.

## World Builder: `BP_K47_WeatherController`

(Not in the repo at the time of writing. Names below are proposed.)

What the AI side provides: the Exposure Director picks an encounter (`RadioWhisper`,
`VisualDistortion`, `HollowWave`) from the player's health, ammo, exposure, stress and safe zone,
and never during a safe zone or low health.

What we ask the weather controller to offer:
1. A Blueprint event such as `OnExposureEvent(EncounterName : String)` that the level Blueprint
   calls when the Director returns `TriggerExposureEvent`. The weather controller decides the look
   (fog, wind, light, audio); the Director does not control weather values itself.
2. A way to return to normal (`ClearExposureEvent`) when the Director returns `NoSafeEvent`/Quiet.
3. No dependency on the Director class: the controller only receives a name, so it keeps working
   when the AI plugin is missing.

Also useful from the World Builder:
- The forest graybox needs a NavMesh Bounds Volume. The Hollow uses a belief grid
  (`InitializeHollow`), so we will need grid size, cell size and which areas are blocked for the
  forest later. Keep the production maps unchanged until `LV_AI_Test` works.

## Core Developer: lab door

(The `core-developer` branch has an "interactive lab door" Blender model. No door Blueprint exists
in the repo yet.)

The door must do these in the **same event** when it opens or closes:
1. Toggle blocking collision, with **Can Ever Affect Navigation = true**. Project Settings,
   Navigation Mesh, **Runtime Generation = Dynamic**.
2. Call `OnDoorStateChanged(DoorId, bOpen)` on every Trooper component, and
   `ReportCellBlocked(cell, NOT bOpen)` on every Hollow brain.
3. Ask each Trooper to `Replan(Pawn Location)`.

Needed from the door:
- A unique integer `DoorId` and the grid cell it sits in (the Trooper component's `DoorCells`
  map holds `DoorId -> cell`; see `LV_AI_TEST_SETUP.md`).
- Collision on the door frame so a closed door is a real obstacle: the planner cannot make the
  Trooper walk through a shut door by itself.
- Do not change the player's Blueprints (`BP_FirstPersonCharacter`, `ABP_FirstPersonCharacter`,
  weapon fire/reload) without agreeing the new API first.

## Open points to agree with the team

- Who owns `LV_AI_Test` and its `.umap`, and whether One File Per Actor is on.
- The name and `DoorId` convention for doors.
- Who adds the `.uproject` entry for `HimeraAI` and builds the plugin (needs a PC with Unreal).
- Whether the weapon owner accepts the gunshot hook in `BLUEPRINT_WIRING.md` section 3.
