# RT-64 Telescope — source deliverables

## Current generation status

**GENERATED HERE:** `RT64_Build.py`, `RT64_Validation.py`, `UNREAL_IMPORT_GUIDE.md`.

**NOT GENERATED HERE:** `.blend`, `.fbx`, actual preview PNGs. Blender is unavailable in this runtime. Running the build script in Blender will attempt to generate these in the specified `output` directory. This code has been checked for Python syntax but has **not** been executed in Blender, so Blender-version compatibility, the actual rig/export and visual quality still require verification.

## Quick command

```bash
blender -b --factory-startup --python RT64_Build.py -- --output ./output
blender -b ./output/RT64_Telescope.blend --python RT64_Validation.py
```

After export, verify the rig and 6400 cm reflector width inside Unreal. See `UNREAL_IMPORT_GUIDE.md`.

## Dimensions

64 m main dish, 23.68 m focal length, 10.81 m dish depth, 6 m secondary, ~23 m elevation pivot; 40-second 30 FPS tracking animation.
