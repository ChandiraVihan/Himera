# RT64 — Blender and Unreal Engine Quick Start

## Generate files locally

Install **Blender 4.2 or later** from blender.org. On macOS Blender may be at `/Applications/Blender.app/Contents/MacOS/Blender`; on Windows `blender.exe` may be under `C:\Program Files\Blender Foundation\Blender <version>\`.

Place `RT64_Build.py` and `RT64_Validation.py` in the same `RT64` directory. From a terminal in that folder:

```bash
blender -b --factory-startup --python RT64_Build.py -- --output ./output
blender -b ./output/RT64_Telescope.blend --python RT64_Validation.py
```

For a quicker first test, omit high-resolution preview renders:

```bash
blender -b --factory-startup --python RT64_Build.py -- --output ./output --no-render
```

The build uses Blender's builtin FBX exporter; no addons are required. If your Blender version rejects an FBX setting, check `bpy.ops.export_scene.fbx.get_rna_type().properties.keys()` and adjust to match the installed version.

## Inspect Blender result

Open `RT64_Telescope.blend` and check Front, Rear, Side and ThreeQuarter cameras. Scrub frames 1, 301, 601, 901 and 1201. Check that the building/foundation does not move, while the upper mount and dish articulate on orthogonal axes. Choose the `RT64_Azimuth_Test` action to view the larger yaw sweep. The `UCX_RT64_Foundation_00` simplified collision is retained within the blend and hidden. The preview-only environment is excluded from FBX exports.

## Import to Unreal Engine 5

1. Import `SK_RT64_Telescope.fbx` with **Skeletal Mesh** enabled. Create a new skeleton named `SKEL_RT64_Telescope`. Verify that root, azimuth and elevation bones are present. Avoid importing spurious preview mesh, cameras or lights.
2. Verify size. The **64 m reflector should be about 6400 Unreal centimetres across**. Blender builds in metres and FBX/Unreal version combinations may expose a unit conversion mismatch. Inspect bounds and sample dimensions before using any correction; do not blindly multiply by 100.
3. Import `AN_RT64_TrackingLoop.fbx` as Animation Only, choosing `SKEL_RT64_Telescope`. Import `AN_RT64_AzimuthTest.fbx` using the SAME skeleton.
4. Open the Skeletal Mesh Editor; inspect at frames 1, 301, 601, 901, 1201. Confirm the foundation remains stationary and that the dish remains rigid with no detached secondary supports.
5. In your level, add a Skeletal Mesh Actor with `SK_RT64_Telescope`; set Animation Mode to **Use Animation Asset**, choose `AN_RT64_TrackingLoop`, and enable **Looping** and **Playing**. Alternatively, use a looping state in an Animation Blueprint.
6. For collision, create a Physics Asset with a small number of simple boxes/capsules on the stationary base and broad dish structure. **Do not** use triangle-mesh complex collision for continuously animated sections. For a landmark with no player climbing, collision can be reduced to the base.
7. Setup Unreal PBR instances per `M_RT64_*` slots. The script makes base-color, roughness and metalness materials with UVs, **not** fully baked texture maps. Add tiling steel noise and mask-based directional corrosion in Unreal if required.
8. Generate and inspect Unreal skeletal mesh LODs or author manual LODs. Preserve truss silhouettes at long distance. Use a moderate screen-size setting for animated skeletal landmark performance.

## Known limitations

- This is a photo-inspired *reconstruction*, not a surveyed blueprint of RT-64/TNA-1500. Exact bearing dimensions, secondary geometry and truss arrangement require engineering documentation for historical precision.
- Preview material wear is restrained, vertex/material based; production texture baking is **not included**.
- No tested Unreal import can be claimed until you run the FBX files through your specific Unreal version.
- No built-in on-disk low-poly LOD files; Unreal-generated LODs are recommended as a follow-on.
- Animations demonstrate articulation, not scientifically calibrated telescope pointing.
- Project scripts never include preview cameras/lighting in skeletal FBX exports.
