"""Validate a generated RT64 blend in Blender: blender -b RT64_Telescope.blend --python RT64_Validation.py"""
import bpy, math, sys
from mathutils import Vector
errors=[]; warnings=[]
def check(ok,message):
 print(('PASS' if ok else 'FAIL')+' '+message)
 if not ok:errors.append(message)
mesh=bpy.data.objects.get('SK_RT64_Telescope');rig=bpy.data.objects.get('SK_RT64_Armature')
check(mesh is not None and mesh.type=='MESH','Joined telescope geometry present')
check(rig is not None and rig.type=='ARMATURE','Armature present')
if rig:
 names={'ROOT','AZIMUTH_PIVOT','ELEVATION_PIVOT'}
 check(names.issubset(rig.data.bones.keys()),'Required bones present')
 if names.issubset(rig.data.bones.keys()):
  check(rig.data.bones['AZIMUTH_PIVOT'].parent.name=='ROOT','Azimuth parent is ROOT')
  check(rig.data.bones['ELEVATION_PIVOT'].parent.name=='AZIMUTH_PIVOT','Elevation parent is azimuth')
  check(abs(rig.data.bones['ELEVATION_PIVOT'].head_local.z-23)<.001,'Elevation pivot at 23 m')
if mesh:
 check(all(mesh.vertex_groups.get(n) for n in ('ROOT','AZIMUTH_PIVOT','ELEVATION_PIVOT')),'Rigid vertex groups exist')
 check(any(m.type=='ARMATURE' and m.object==rig for m in mesh.modifiers),'Armature modifier points to rig')
 check(len(mesh.data.polygons)>10000,'Mesh contains detailed geometry')
 check(len(mesh.data.uv_layers)>0,'UV map exists')
 # directly sample known reflector vertex coordinates from build: nearest 192 vertices at rim by name after join.
 # Evaluate nominal rim envelope: reflected rim orientation does not align to world XY.
 try:
  verts=[v.co for v in mesh.data.vertices]
  xs=[v.x for v in verts]
  extent=max(xs)-min(xs)
  check(abs(extent-64)<1.0,f'Overall lateral width near 64 m: {extent:.2f} m')
 except Exception as exc:check(False,'Geometry extent inspection: '+str(exc))
for a in ('RT64_Tracking_Loop','RT64_Azimuth_Test'):
 check(bpy.data.actions.get(a) is not None,f'{a} action present')
check(bpy.context.scene.render.fps==30,'30 FPS scene')
check(bpy.context.scene.frame_end==1201,'1201-frame timeline')
if rig and bpy.data.actions.get('RT64_Tracking_Loop'):
 original=rig.animation_data.action if rig.animation_data else None
 rig.animation_data.action=bpy.data.actions['RT64_Tracking_Loop']
 poses=[]
 for frame in (1,301,601,901,1201):
  bpy.context.scene.frame_set(frame)
  poses.append((tuple(rig.pose.bones['AZIMUTH_PIVOT'].rotation_euler),tuple(rig.pose.bones['ELEVATION_PIVOT'].rotation_euler)))
 check(poses[0]==poses[-1],'Tracking loop endpoints identical')
 check(poses[0]!=poses[1] and poses[1]!=poses[3],'Tracking pose actually changes')
 bpy.context.scene.frame_set(1)
 if original:rig.animation_data.action=original
print('RESULT:',len(errors),'failures;',len(warnings),'warnings')
if errors: sys.exit(1)
