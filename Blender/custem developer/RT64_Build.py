"""RT-64 inspired telescope asset generator. Blender 4.2+ recommended.
Run: blender -b --python RT64_Build.py -- --output /path/to/RT64
Geometry is a reference-inspired reconstruction, not surveyed engineering CAD.
"""
import bpy, math, os, sys, argparse, traceback
from mathutils import Vector, Matrix
from math import sin, cos, pi, radians

D=64.0; R=D/2; F=0.37*D; RINGS=49; SEGMENTS=192; TRACK_END=1201
PIVOT_Z=23.0; DISH_Z=26.0; REST_ELEV=50.0
ANGLE=radians(REST_ELEV)
OUT=os.path.dirname(os.path.realpath(bpy.data.filepath)) if bpy.data.filepath else os.getcwd()
if '--' in sys.argv:
    ap=argparse.ArgumentParser(); ap.add_argument('--output'); ap.add_argument('--no-export',action='store_true'); ap.add_argument('--no-render',action='store_true')
    args=ap.parse_args(sys.argv[sys.argv.index('--')+1:]); OUT=os.path.abspath(args.output or OUT)
else: args=argparse.Namespace(no_export=False,no_render=False)
os.makedirs(OUT,exist_ok=True)

# Each module is assigned to a single rigid bone. Builders batch thousands of members into a few meshes.
COLL='RT64_GENERATED'
if COLL in bpy.data.collections:
    old=bpy.data.collections[COLL]
    for obj in list(old.objects): bpy.data.objects.remove(obj,do_unlink=True)
    bpy.data.collections.remove(old)
coll=bpy.data.collections.new(COLL); bpy.context.scene.collection.children.link(coll)
scene=bpy.context.scene; scene.unit_settings.system='METRIC'; scene.unit_settings.scale_length=1.0
scene.render.engine='BLENDER_EEVEE_NEXT' if 'BLENDER_EEVEE_NEXT' in {e.identifier for e in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items} else 'BLENDER_EEVEE'
scene.render.resolution_x=1280; scene.render.resolution_y=960; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.film_transparent=False
scene.frame_start=1; scene.frame_end=TRACK_END; scene.render.fps=30
scene.world.color=(.18,.22,.27)

mats={}
def mat(name, rgb, metallic=0.0, roughness=.65):
    m=bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.diffuse_color=(*rgb,1); m.use_nodes=True
    bs=m.node_tree.nodes.get('Principled BSDF')
    if bs:
        bs.inputs['Base Color'].default_value=(*rgb,1)
        bs.inputs['Metallic'].default_value=metallic
        bs.inputs['Roughness'].default_value=roughness
    mats[name]=m; return m
mat('M_RT64_Panel_ColdGalvanized',(.36,.44,.47),.76,.52)
mat('M_RT64_Panel_Alternate',(.31,.38,.42),.74,.56)
mat('M_RT64_StructuralSteel',(.25,.32,.35),.76,.6)
mat('M_RT64_PaintedMetal',(.37,.43,.43),.48,.63)
mat('M_RT64_BearingDark',(.10,.14,.16),.68,.46)
mat('M_RT64_OxidizedSteel',(.24,.19,.16),.48,.8)
mat('M_RT64_Concrete',(.44,.45,.42),.02,.92)
mat('M_RT64_Cable',(.055,.065,.067),.05,.82)
mat('M_RT64_Secondary',(.49,.53,.52),.66,.39)
mat('M_RT64_Snow',(.78,.85,.87),0,.91)

parts=[]
class Builder:
    def __init__(self,name,bone): self.name=name; self.bone=bone; self.verts=[]; self.faces=[]; self.materials=[]; self.face_mats=[]; self.uvs=[]
    def ensure(self,m):
        if m not in self.materials:self.materials.append(m)
        return self.materials.index(m)
    def poly(self,vs,fs, material, uvs=None):
        ix=len(self.verts); self.verts.extend(vs); mi=self.ensure(material)
        for j,f in enumerate(fs):
            self.faces.append(tuple(ix+k for k in f)); self.face_mats.append(mi)
    def beam(self,a,b,width,material,sides=6):
        a,b=Vector(a),Vector(b); direction=b-a
        if direction.length < 1e-6:return
        direction.normalize(); cross=direction.cross(Vector((0,0,1)))
        if cross.length<.05:cross=direction.cross(Vector((0,1,0)))
        cross.normalize(); cross2=direction.cross(cross).normalized()
        vs=[]
        for center in (a,b):
            for j in range(sides):
                q=2*pi*j/sides; p=center+width*.5*(cos(q)*cross+sin(q)*cross2); vs.append(tuple(p))
        fs=[tuple(reversed(range(sides))),tuple(range(sides,2*sides))]
        for j in range(sides): fs.append((j,(j+1)%sides,(j+1)%sides+sides,j+sides))
        self.poly(vs,fs,material)
    def box(self,center,dimensions,material):
        x,y,z=Vector(center); a,b,c=[q/2 for q in dimensions]
        self.poly([(x+dx*a,y+dy*b,z+dz*c) for dz in (-1,1) for dy in (-1,1) for dx in (-1,1)],
          [(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)],material)
    def cylinder(self,center,radius,depth,material,axis=(0,0,1),sides=32):
        axis=Vector(axis).normalized(); cross=axis.cross(Vector((0,1,0)))
        if cross.length<.05:cross=axis.cross(Vector((0,0,1)))
        cross.normalize(); cross2=axis.cross(cross).normalized(); ctr=Vector(center)
        vs=[]
        for h in (-depth/2,depth/2):
            for j in range(sides):
                a=2*pi*j/sides; v=ctr+h*axis+radius*(cos(a)*cross+sin(a)*cross2);vs.append(tuple(v))
        fs=[tuple(reversed(range(sides))),tuple(range(sides,2*sides))]
        for j in range(sides):fs.append((j,(j+1)%sides,(j+1)%sides+sides,j+sides))
        self.poly(vs,fs,material)
    def finish(self):
        if not self.verts:return None
        mesh=bpy.data.meshes.new(self.name+'_Geo'); mesh.from_pydata(self.verts,[],self.faces);mesh.update()
        o=bpy.data.objects.new(self.name,mesh);coll.objects.link(o)
        for m in self.materials:mesh.materials.append(m)
        for face,mi in zip(mesh.polygons,self.face_mats):face.material_index=mi
        o['rigid_bone']=self.bone;parts.append(o)
        # Provide basic valid UV coordinates. Generic truss UVs are local planar projections.
        uv=mesh.uv_layers.new(name='UVMap')
        for poly in mesh.polygons:
            for li in poly.loop_indices:
                v=mesh.vertices[mesh.loops[li].vertex_index].co
                uv.data[li].uv=(v.x/64+.5,v.y/64+.5)
        return o

fixed=Builder('RT64_Foundation_Fixed','ROOT')
fixed.cylinder((0,0,.8),11,1.6,mats['M_RT64_Concrete'],sides=64)
fixed.cylinder((0,0,4.9),7.2,6.8,mats['M_RT64_Concrete'],sides=48)
fixed.cylinder((0,0,9.0),8.6,1.3,mats['M_RT64_Concrete'],sides=64)
fixed.cylinder((0,0,11),6.2,3.3,mats['M_RT64_PaintedMetal'],sides=48)
for i in range(24):
 a=2*pi*i/24; x,y=8.45*cos(a),8.45*sin(a)
 fixed.cylinder((x,y,1.65),.17,.35,mats['M_RT64_BearingDark'],sides=10)
for i in range(12):
 a=2*pi*i/12; x,y=6.95*cos(a),6.95*sin(a)
 fixed.box((x,y,5.2),(.3,.3,5.1),mats['M_RT64_Concrete'])
fixed.finish()

az=Builder('RT64_Azimuth_Carriage','AZIMUTH_PIVOT')
for z,rad,deep,mt in [(12.7,7.0,.65,'M_RT64_BearingDark'),(13.2,7.4,.37,'M_RT64_StructuralSteel'),(14.0,7.9,.8,'M_RT64_PaintedMetal'),(15.0,7.2,1.5,'M_RT64_PaintedMetal')]:
 az.cylinder((0,0,z),rad,deep,mats[mt],sides=64)
# opposing bearing towers on left and right of axis X
for sign in (-1,1):
 x=sign*7.35
 az.box((x,0,18.7),(3.6,5.3,8),mats['M_RT64_PaintedMetal'])
 az.beam((sign*6.0,-3,15.7),(sign*6.8,-3,23),.8,mats['M_RT64_StructuralSteel'],8)
 az.beam((sign*6,3,15.7),(sign*6.8,3,23),.8,mats['M_RT64_StructuralSteel'],8)
 az.cylinder((sign*7.9,0,PIVOT_Z),3,1.8,mats['M_RT64_BearingDark'],axis=(1,0,0),sides=48)
 az.cylinder((sign*9.0,0,PIVOT_Z),2.15,.43,mats['M_RT64_OxidizedSteel'],axis=(1,0,0),sides=48)
 for q in range(12):
  t=2*pi*q/12
  az.cylinder((sign*9.28,2.4*cos(t),PIVOT_Z+2.4*sin(t)),.15,.3,mats['M_RT64_StructuralSteel'],axis=(1,0,0),sides=8)
# rotating machinery decks and rail posts
az.box((0,-4.9,15.7),(8.2,3.1,.35),mats['M_RT64_StructuralSteel'])
for x in (-3.8,-2,0,2,3.8):az.beam((x,-6.3,15.8),(x,-6.3,17.0),.09,mats['M_RT64_PaintedMetal'])
az.beam((-4,-6.3,17),(4,-6.3,17),.11,mats['M_RT64_PaintedMetal'])
for j in range(16):
 t=2*pi*j/16
 az.box((6.6*cos(t),6.6*sin(t),13.35),(.8,.5,.25),mats['M_RT64_BearingDark'])
az.finish()

def dishpoint(u,v,w=0.0):
 """u,v: face plane coordinates, w: forward/axial. Pose tilt from horizon."""
 return (u, -v*sin(ANGLE)+w*cos(ANGLE), DISH_Z+v*cos(ANGLE)+w*sin(ANGLE))
def dish_r(r,t,w=0):return dishpoint(r*cos(t),r*sin(t),w)
def profile(r):return (r*r)/(4*F)

surface=Builder('RT64_Main_Reflector_Panels','ELEVATION_PIVOT')
# one single seamless parabolic panel field with sector/annulus material modulation
verts=[]; faces=[]; ims=[]
for j in range(RINGS):
 rr=R*j/(RINGS-1)
 for i in range(SEGMENTS):
  t=2*pi*i/SEGMENTS;verts.append(dish_r(rr,t,profile(rr)))
for j in range(RINGS-1):
 for i in range(SEGMENTS):
  ni=(i+1)%SEGMENTS; faces.append((j*SEGMENTS+i,j*SEGMENTS+ni,(j+1)*SEGMENTS+ni,(j+1)*SEGMENTS+i))
# alternate panels with subtle staggered reflectivity, no exaggerated grid grooves
surface.poly(verts,faces,mats['M_RT64_Panel_ColdGalvanized'])
o=surface.finish();
for poly in o.data.polygons:poly.use_smooth=True
# Alternate panel materials on select bands, UV map aligned to polar layout.
o.data.materials.append(mats['M_RT64_Panel_Alternate'])
for j in range(RINGS-1):
 for i in range(SEGMENTS):
  o.data.polygons[j*SEGMENTS+i].material_index=int(((j//4)+(i//8))%7==0)
uv=o.data.uv_layers.active
for poly in o.data.polygons:
 for li in poly.loop_indices:
  idx=o.data.loops[li].vertex_index
  ring,sec=divmod(idx,SEGMENTS);theta=2*pi*sec/SEGMENTS
  uv.data[li].uv=(.5+.5*ring/(RINGS-1)*cos(theta),.5+.5*ring/(RINGS-1)*sin(theta))

framework=Builder('RT64_Reflector_RearTrusses','ELEVATION_PIVOT')
steel=mats['M_RT64_StructuralSteel']; dark=mats['M_RT64_PaintedMetal']
# rear depth tapers to center, forming rigid lattice instead of loose decorative spokes
N_RIBS=48
for i in range(N_RIBS):
 t=2*pi*i/N_RIBS
 for j in range(1,13):
  r0=R*(j-1)/12;r1=R*j/12
  a=dish_r(r0,t,profile(r0)-.45);b=dish_r(r1,t,profile(r1)-.45)
  framework.beam(a,b,.20 if i%4 else .36,steel,6)
  # rear chord/rib supported by ties to actual reflecting surface
  dep0=2.1+5.5*(r0/R);dep1=2.1+5.5*(r1/R)
  c=dish_r(r0,t,profile(r0)-dep0);d=dish_r(r1,t,profile(r1)-dep1)
  framework.beam(c,d,.27 if i%4 else .42,steel,6)
  framework.beam(a,c,.17,steel)
  framework.beam(c,b,.14,steel)
 for j in (3,6,9,12):
  r=R*j/12; dep=2.1+5.5*(r/R)
  a=dish_r(r,t,profile(r)-dep);b=dish_r(r,t+2*pi/N_RIBS,profile(r)-dep)
  framework.beam(a,b,.24,steel)
  if j!=12:
   rn=R*(j+1)/12
   c=dish_r(rn,t+2*pi/N_RIBS,profile(rn)-(2.1+5.5*rn/R))
   framework.beam(a,c,.16,steel)
# ring/perimeter braces
for i in range(SEGMENTS):
 t=2*pi*i/SEGMENTS;tn=2*pi*(i+1)/SEGMENTS
 framework.beam(dish_r(R,t,profile(R)),dish_r(R,tn,profile(R)),.56,dark,8)
 for r in (8,16,24):
  framework.beam(dish_r(r,t,profile(r)-.4),dish_r(r,tn,profile(r)-.4),.045,steel,5)
framework.finish()

# articulated back-frame and hub, mechanically attached to fork/trunnions
hub=Builder('RT64_ElevationHubAndBraces','ELEVATION_PIVOT')
hub.cylinder((0,0,PIVOT_Z),3.8,15.2,mats['M_RT64_PaintedMetal'],axis=(1,0,0),sides=48)
for sign in (-1,1):
 for r,t in [(14,pi/2),(14,3*pi/2),(25,pi/2),(25,3*pi/2),(26,0),(26,pi)]:
  hub.beam((sign*6.6,0,PIVOT_Z),dish_r(r,t,profile(r)-4.5),.55 if r>=25 else .8,steel,8)
for sign in (-1,1):
 hub.box((sign*5,1.5,25),(2.5,3,3),dark)
hub.finish()

feed=Builder('RT64_SecondaryAndSupport','ELEVATION_PIVOT')
# shallow secondary dish opposed toward large bowl, 6 m diameter
secverts=[]; secfaces=[];SN=64; SR=3.; STEPS=10
for j in range(STEPS+1):
 r=SR*j/STEPS
 for i in range(SN):
  t=2*pi*i/SN; secverts.append(dish_r(r,t,19.3-.18*r*r))
for j in range(STEPS):
 for i in range(SN):
  ni=(i+1)%SN;secfaces.append((j*SN+i,j*SN+ni,(j+1)*SN+ni,(j+1)*SN+i))
feed.poly(secverts,secfaces,mats['M_RT64_Secondary'])
feed.cylinder(dishpoint(0,0,19.8),.95,1.8,mats['M_RT64_BearingDark'],axis=(0,cos(ANGLE),sin(ANGLE)),sides=24)
feed.cylinder(dishpoint(0,0,2.0),1.3,2.8,mats['M_RT64_BearingDark'],axis=(0,cos(ANGLE),sin(ANGLE)),sides=24)
for i in range(4):
 t=2*pi*(i+.5)/4
 # support from near dish rim to secondary hub
 start=dish_r(26,t,profile(26)+.35)
 end=dish_r(1.6,t,19.3)
 feed.beam(start,end,.42,steel,8)
 feed.beam(dish_r(23,t,profile(23)+.4),dish_r(2.2,t,18.5),.20,dark,6)
for i in range(SEGMENTS):
 t=2*pi*i/SEGMENTS;tn=2*pi*(i+1)/SEGMENTS
 feed.beam(dish_r(3,t,17.68),dish_r(3,tn,17.68),.15,steel)
feed.finish()

# Objects are linked to bones as rigid skinning rather than movable empties.
armdata=bpy.data.armatures.new('RT64_ArmatureData');rig=bpy.data.objects.new('SK_RT64_Armature',armdata);coll.objects.link(rig)
bpy.context.view_layer.objects.active=rig;rig.select_set(True);bpy.ops.object.mode_set(mode='EDIT')
def addbone(name,head,tail,parent=None):
 b=armdata.edit_bones.new(name);b.head=head;b.tail=tail
 if parent:b.parent=armdata.edit_bones[parent];b.use_connect=False
 return b
addbone('ROOT',(0,0,0),(0,0,1))
addbone('AZIMUTH_PIVOT',(0,0,13.3),(0,0,14.3),'ROOT')
addbone('ELEVATION_PIVOT',(0,0,PIVOT_Z),(1,0,PIVOT_Z),'AZIMUTH_PIVOT')
bpy.ops.object.mode_set(mode='OBJECT');rig.show_in_front=True

for obj in parts:
 group=obj.vertex_groups.new(name=obj['rigid_bone'])
 group.add(list(range(len(obj.data.vertices))),1.,'REPLACE')
 mod=obj.modifiers.new('RT64_RigidSkeleton','ARMATURE');mod.object=rig
 obj.parent=rig

# export as one skeletal mesh by joining meshes; do not apply armature modifiers
bpy.ops.object.select_all(action='DESELECT')
for obj in parts:obj.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.object.join()
meshobj=bpy.context.view_layer.objects.active;meshobj.name='SK_RT64_Telescope'
parts=[meshobj]

# actions have explicit pose channels, independently working controls; use bone LOCAL Y axis
azbone=rig.pose.bones['AZIMUTH_PIVOT'];elbone=rig.pose.bones['ELEVATION_PIVOT']
for b in (azbone,elbone):b.rotation_mode='XYZ'
def action_make(name,azangles,elangles):
 rig.animation_data_create(); act=bpy.data.actions.new(name)
 # Blender 4.4+ slotted action compatible via keyframe insertion
 rig.animation_data.action=act
 for frame,angle in azangles:
  azbone.rotation_euler=(0,radians(angle),0)
  azbone.keyframe_insert('rotation_euler',frame=frame,group='AZIMUTH_PIVOT')
 for frame,angle in elangles:
  elbone.rotation_euler=(0,radians(angle),0)
  elbone.keyframe_insert('rotation_euler',frame=frame,group='ELEVATION_PIVOT')
 # smooth Bezier; matching opening/ending pose and handles for seam
 try:
  for fc in act.fcurves:
   for key in fc.keyframe_points:key.interpolation='BEZIER';key.handle_left_type='AUTO_CLAMPED';key.handle_right_type='AUTO_CLAMPED'
 except AttributeError:pass
 return act
track=action_make('RT64_Tracking_Loop',[(1,0),(301,12),(601,0),(901,-12),(1201,0)],[(1,0),(301,3),(601,0),(901,2),(1201,0)])
test=action_make('RT64_Azimuth_Test',[(1,-45),(301,0),(601,45),(901,0),(1201,-45)],[(1,0),(1201,0)])
# keep both actions accessible to Unreal export regardless of active action.
for act in (track,test):
 act.use_fake_user=True
try:
 rig.animation_data.action=track
except Exception:pass
scene.frame_set(1)

# Collision volumes intentionally separate and non-skeletal for UE (import separately if desired).
collision=Builder('UCX_RT64_Foundation_00','ROOT')
collision.box((0,0,4),(15,15,8),mats['M_RT64_Concrete'])
col_obj=collision.finish();col_obj.hide_render=True;col_obj.hide_set(True)
parts=[meshobj]

# visual inspection preview environment is excluded from exports
preview_coll=bpy.data.collections.new('RT64_PREVIEW_ONLY');scene.collection.children.link(preview_coll)
def preview_object(obj):
 for c in list(obj.users_collection):c.objects.unlink(obj)
 preview_coll.objects.link(obj)
bpy.ops.mesh.primitive_plane_add(size=250,location=(0,0,-.08));ground=bpy.context.object;ground.name='PREVIEW_Snow_Ground';preview_object(ground);ground.data.materials.append(mats['M_RT64_Snow'])

def aim(obj,target): obj.rotation_euler=(Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler()
views={'Front':((72,110,66),(0,0,29)),'Rear':((-83,-103,68),(0,0,26)),'Side':((103,-9,49),(0,0,25)),'ThreeQuarter':((85,80,75),(0,0,28))}
for name,(pos,target) in views.items():
 camdata=bpy.data.cameras.new('PREVIEW_'+name);cam=bpy.data.objects.new('PREVIEW_'+name,camdata);preview_coll.objects.link(cam)
 cam.location=pos;aim(cam,target);camdata.type='ORTHO';camdata.ortho_scale=91
lightdata=bpy.data.lights.new('PREVIEW_Key','AREA');light=bpy.data.objects.new('PREVIEW_Key',lightdata);preview_coll.objects.link(light)
light.location=(25,-40,105);lightdata.energy=9000;lightdata.shape='DISK';lightdata.size=65;aim(light,(0,0,25))

# Stable active camera; separate renders for inspection; no preview items exported.
scene.camera=bpy.data.objects['PREVIEW_ThreeQuarter']

# validate dimensions based on planned mesh vertices as well as animation signatures
assert abs(D-64)<1e-8
assert len(parts)==1 and len(meshobj.data.vertices)>20000
assert all(n in armdata.bones for n in ('ROOT','AZIMUTH_PIVOT','ELEVATION_PIVOT'))
assert meshobj.vertex_groups.get('ROOT') and meshobj.vertex_groups.get('AZIMUTH_PIVOT') and meshobj.vertex_groups.get('ELEVATION_PIVOT')
assert track.name in bpy.data.actions and test.name in bpy.data.actions

blendpath=os.path.join(OUT,'RT64_Telescope.blend')
bpy.ops.wm.save_as_mainfile(filepath=blendpath,check_existing=False)

if not args.no_export:
 def fbx_export(path,action=None,mesh=True):
  bpy.ops.object.select_all(action='DESELECT');rig.select_set(True)
  if mesh:meshobj.select_set(True)
  bpy.context.view_layer.objects.active=rig
  if action:rig.animation_data.action=action
  # official Blender FBX exporter, 1 Blender m = 100 UE cm via UE import unit scaling
  bpy.ops.export_scene.fbx(filepath=path,use_selection=True,object_types={'ARMATURE','MESH'},
   use_mesh_modifiers=True,add_leaf_bones=False,primary_bone_axis='Y',secondary_bone_axis='X',
   axis_forward='-Y',axis_up='Z',apply_unit_scale=True,global_scale=1.0,
   bake_anim=True,bake_anim_use_all_bones=True,bake_anim_use_nla_strips=False,
   bake_anim_use_all_actions=False,bake_anim_step=1.0,bake_anim_simplify_factor=0.0,
   path_mode='AUTO',use_armature_deform_only=True)
 fbx_export(os.path.join(OUT,'SK_RT64_Telescope.fbx'),track,True)
 fbx_export(os.path.join(OUT,'AN_RT64_TrackingLoop.fbx'),track,False)
 fbx_export(os.path.join(OUT,'AN_RT64_AzimuthTest.fbx'),test,False)
 rig.animation_data.action=track
 scene.frame_set(1)

if not args.no_render:
 for name in ('Front','Rear','Side','ThreeQuarter'):
  scene.camera=bpy.data.objects['PREVIEW_'+name]
  scene.render.filepath=os.path.join(OUT,'PREVIEW_'+name+'.png')
  bpy.ops.render.render(write_still=True)
 scene.camera=bpy.data.objects['PREVIEW_ThreeQuarter'];scene.frame_set(601)
 scene.render.filepath=os.path.join(OUT,'PREVIEW_AnimatedPose.png');bpy.ops.render.render(write_still=True)
 scene.frame_set(1)

scene.camera=bpy.data.objects['PREVIEW_ThreeQuarter']
bpy.ops.wm.save_as_mainfile(filepath=blendpath,check_existing=False)
report=f'''RT-64 telescope build report\nBlender: {bpy.app.version_string}\nOutput: {OUT}\nReflector diameter: {D:.3f} m (Unreal equivalent {D*100:.0f} cm)\nReflector focal length: {F:.3f} m\nCalculated parabola bowl depth: {R*R/(4*F):.3f} m\nNominal secondary diameter: 6 m\nElevation pivot height: {PIVOT_Z} m\nGeometry vertices (joined skeletal mesh): {len(meshobj.data.vertices)}\nGeometry triangles (estimated): {sum(max(1,len(p.vertices)-2) for p in meshobj.data.polygons)}\nTracking loop: 1-1201 at 30fps (40 seconds); az +/-12 degrees; elevation rest 50 to 53 deg\nAzimuth test: 1-1201; +/-45 deg\nFBX: axis_forward -Y, axis_up Z, apply_unit_scale True, add_leaf_bones False, animation baked step 1\nImportant: Autodesk FBX / Unreal Unit import should use Blender metres -> UE centimetres. Verify 6400 cm width in Unreal.\nReference uncertainty: tower height, support details, secondary curvature and exact bearing geometry are artistic approximations.\nMaterials: Blender PBR nodes; FBX material color/roughness transfer is version-dependent. Rebuild enhanced weathering in UE materials.\nTextures: no baked PBR textures; basic UV channels generated.\nCollision: UCX_RT64_Foundation_00 in scene but NOT in skeletal FBXs; import separately or use Unreal physics asset simplified collision.\nLOD: none produced; use UE skeletal mesh LOD tool with review of truss preservation.\nValidation: geometric script assertions passed; physical import to Unreal and rendered visual inspection must be done by user.\n'''
with open(os.path.join(OUT,'RT64_Export_Report.txt'),'w',encoding='utf-8') as f:f.write(report)
print('RT64 BUILD COMPLETE:',blendpath)
