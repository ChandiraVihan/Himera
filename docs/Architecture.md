 # World Builder Integration

## K47 exterior graybox

The Blender scene is the source blockout for the K47 exterior environment. Keep
the `.blend` file as the editable source, then export approved geometry into
the Unreal content tree as a separate integration step.

The graybox should support three checks before it becomes level content:

1. Exterior scale matches the first-person character and camera.
2. Major routes remain readable from the intended player approach.
3. Imported geometry receives collision and material assignments in Unreal.

The source scene does not replace the playable map in
`Content/FirstPerson/Lvl_FirstPerson.umap`; it feeds future environment work.
