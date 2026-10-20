 # World Builder Performance Notes

## K47 exterior graybox

Keep the source scene lightweight while layout is changing. Prefer simple
blockout meshes, apply transforms before export, and remove hidden geometry that
does not contribute to the exterior read.

Before promoting the graybox into Unreal, check:

- Collision is generated from simplified shapes rather than render geometry.
- Repeated environment pieces can use instancing.
- Large surfaces have sensible material and lightmap boundaries.
- The playable route is tested in the first-person camera at target scale.
