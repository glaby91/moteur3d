"""Import M3D1314 v5 scene binary in Blender 5.x; preserve source triangles, UV and normals.
Usage: blender --python import_moteur3d_beta3_blender.py -- /path/to/scene_pripyat_central.bin
Or run in Scripting; defaults to scene_pripyat_central.bin beside this script.
"""
import bpy, struct, math, os, sys, mmap
from mathutils import Vector
from collections import defaultdict, Counter
from array import array

script_path = globals().get('__file__', '')
ROOT = os.path.dirname(os.path.abspath(script_path)) if script_path and script_path.lower().endswith('.py') else os.path.dirname(bpy.data.filepath)
# Default location matching the user's extracted Beta 3 folder.
DEFAULT_BIN = r'C:\Users\charles\Desktop\Moteur\beta3\scene_pripyat_central.bin'
args = sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
BIN = args[0] if args else (os.path.join(ROOT, 'scene_pripyat_central.bin') if os.path.isfile(os.path.join(ROOT, 'scene_pripyat_central.bin')) else DEFAULT_BIN)
if not os.path.isfile(BIN):
    raise FileNotFoundError('Fichier scène Beta 3 introuvable: '+BIN+' (copiez scene_pripyat_central.bin à côté du script)')
# Memory-map the large scene instead of copying 1.6 GB into Python RAM.
_bin_handle = open(BIN, 'rb')
_mapped = mmap.mmap(_bin_handle.fileno(), 0, access=mmap.ACCESS_READ)
raw = memoryview(_mapped)
pos = 0

# Safe default: preserve geometry and albedo, disable unverified PBR channels.
# Set True only after validating RGB565 normal/roughness data.
ENABLE_PBR_NORMAL_ROUGHNESS = False
# The current M3D scene appears Z-up; do not rotate it without verification.
# Use "Y_UP" only if your source scene truly uses Y as its vertical axis.
SOURCE_UP_AXIS = "Z_UP"
# Display-safe material mode for diagnosing geometry and illumination.
DIAGNOSTIC_NEUTRAL_MATERIALS = False
# Render preview quality; increase after verifying geometry and lighting.
RENDER_PERCENT = 60
# Set True to write a separate .blend file after import.
SAVE_PROJECT = False

def read(fmt):
    global pos
    result=struct.unpack_from('<'+fmt,raw,pos);pos+=struct.calcsize('<'+fmt)
    return result[0] if len(result)==1 else result

magic=bytes(raw[:8]);pos=8
if magic.rstrip(b'\x00')!=b'M3D1314': raise ValueError('Format scène inconnu: '+str(magic))
version,nobj,nbvh,root,norder,nmat,ntex,texsize=read('IIIiIIII')
if version!=5: raise ValueError('Version de scène non supportée: '+str(version))
materials=[]
for i in range(nmat):
    name=bytes(raw[pos:pos+24]).split(b'\0')[0].decode('utf8','replace');pos+=24
    reflect,transmission,ior=read('BBH');materials.append((name,reflect,transmission,ior))
print('M3D Beta3:',nobj,'objects,',nmat,'materials, texture size',texsize)

# Material-batched mesh: triangle primitives retain exact source coordinates, UV and normals.
meshdata=defaultdict(lambda: {'verts':[],'faces':[],'uv':[],'normals':[]})
counts=Counter()

def to_blender(p):
    x,y,z=p
    return (x,-z,y) if SOURCE_UP_AXIS == "Y_UP" else (x,y,z)

def to_blender_normal(p):
    return to_blender(p)

def tri(mat,points,uvs=None,normals=None):
    d=meshdata[mat];idx=len(d['verts']);d['verts'].extend(to_blender(p) for p in points)
    d['faces'].append((idx,idx+1,idx+2))
    d['uv'].extend(uvs if uvs is not None else [(0,0),(1,0),(0,1)])
    if normals: d['normals'].extend(to_blender_normal(p) for p in normals)
    else: d['normals'].extend([None]*3)

def quad(mat,p,uv=None):
    uv=uv or [(0,0),(1,0),(1,1),(0,1)]
    tri(mat,[p[0],p[1],p[2]],[uv[0],uv[1],uv[2]])
    tri(mat,[p[0],p[2],p[3]],[uv[0],uv[2],uv[3]])

def cube(mat,lo,hi):
    x,y,z=lo;X,Y,Z=hi
    corners=[(x,y,z),(X,y,z),(X,Y,z),(x,Y,z),(x,y,Z),(X,y,Z),(X,Y,Z),(x,Y,Z)]
    for ids in [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]:quad(mat,[corners[i] for i in ids])

def uv_sphere(mat,center,radii,lat=10,lon=14):
    cx,cy,cz=center;rx,ry,rz=radii
    def pt(i,j):
        a=math.pi*i/lat;b=2*math.pi*j/lon
        return (cx+rx*math.sin(a)*math.cos(b),cy+ry*math.cos(a),cz+rz*math.sin(a)*math.sin(b))
    for i in range(lat):
        for j in range(lon):
            p=[pt(i,j),pt(i,j+1),pt(i+1,j+1),pt(i+1,j)]
            if i==0:tri(mat,[p[0],p[2],p[3]])
            elif i==lat-1:tri(mat,[p[0],p[1],p[2]])
            else:quad(mat,p)

def cylinder(mat,a,b,r0,r1=None,sides=12):
    r1=r0 if r1 is None else r1
    va=Vector(a);vb=Vector(b);direction=vb-va
    if direction.length<1e-6:return
    direction.normalize();basis=direction.cross(Vector((0,0,1)))
    if basis.length<1e-5:basis=direction.cross(Vector((0,1,0)))
    basis.normalize();other=direction.cross(basis).normalized()
    for j in range(sides):
        t=2*math.pi*j/sides;u=2*math.pi*(j+1)/sides
        def ring(center,r,ang):return tuple(center+r*(math.cos(ang)*basis+math.sin(ang)*other))
        quad(mat,[ring(va,r0,t),ring(va,r0,u),ring(vb,r1,u),ring(vb,r1,t)])

for k in range(nobj):
    kind,mat,_=read('BBH');lo=read('fff');hi=read('fff');counts[kind]+=1
    if kind==0:
        v=read('24f');tri(mat,[v[0:3],v[3:6],v[6:9]],[v[18:20],v[20:22],v[22:24]],[v[9:12],v[12:15],v[15:18]])
    elif kind==1:cube(mat,lo,hi)
    elif kind==2:
        r,cx,cy,cz,sx,sy,sz=read('7f');cube(mat,lo,hi) # rounded-box bounds, exact bounding shape; corner radius retained in report
    elif kind==3:
        v=read('8f');a=v[:3];b=v[3:6];cylinder(mat,a,b,v[6]);uv_sphere(mat,a,(v[6],)*3,6,10);uv_sphere(mat,b,(v[6],)*3,6,10)
    elif kind==4:
        v=read('13f');cylinder(mat,v[:3],v[3:6],v[6],v[7])
    elif kind==5:
        v=read('6f');uv_sphere(mat,v[:3],v[3:6])
    else:raise ValueError('Unknown primitive type '+str(kind))
print('Primitive distribution:',dict(counts))
# Skip BVH, order; decode the original embedded textures.
pos+=nbvh*40+norder*4
images=[]
for mid,(name,*_) in enumerate(materials):
    levels=read('I');image=None
    for level in range(levels):
        size=read('I');n=size*size
        if level==0:
            # Native color is packed 0xRRGGBB uint32, use NumPy for fast conversion.
            import numpy as np
            pixels=np.frombuffer(raw,dtype='<u4',count=n,offset=pos)
            rgba=np.empty((n,4),dtype=np.float32)
            rgba[:,0]=((pixels>>16)&255)/255;rgba[:,1]=((pixels>>8)&255)/255;rgba[:,2]=(pixels&255)/255;rgba[:,3]=1
            image=bpy.data.images.new('M3D_'+name,width=size,height=size,alpha=True)
            image.pixels.foreach_set(rgba.ravel());image.pack()
        pos+=n*4
    images.append(image)
normal_images=[];rough_images=[]
for mid,(name,*_) in enumerate(materials):
    for channel,result in (("normal",normal_images),("roughness",rough_images)):
        levels=read('I');image=None
        for level in range(levels):
            size=read('I');n=size*size
            if level==0 and ENABLE_PBR_NORMAL_ROUGHNESS:
                import numpy as np
                packed=np.frombuffer(raw,dtype='<u2',count=n,offset=pos)
                rgba=np.empty((n,4),dtype=np.float32)
                rgba[:,0]=((packed>>11)&31)/31;rgba[:,1]=((packed>>5)&63)/63;rgba[:,2]=(packed&31)/31;rgba[:,3]=1
                image=bpy.data.images.new('M3D_'+name+'_'+channel,width=size,height=size,alpha=True)
                image.pixels.foreach_set(rgba.ravel());image.colorspace_settings.name='Non-Color';image.pack()
            pos+=n*2
        result.append(image)
nlights=read('I');lights=[read('8f') for _ in range(nlights)]
if pos!=len(raw):print('WARNING: unparsed bytes:',len(raw)-pos)

# Clear scene and make grouped meshes.
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
collection=bpy.data.collections.new('ENERGETIK_BETA3');bpy.context.scene.collection.children.link(collection)
for mid,d in meshdata.items():
    name=materials[mid][0];mesh=bpy.data.meshes.new('mesh_'+name);mesh.from_pydata(d['verts'],[],d['faces']);mesh.update()
    uv=mesh.uv_layers.new(name='Source UV')
    for i,p in enumerate(uv.data):p.uv=d['uv'][i]
    obj=bpy.data.objects.new('material_'+name,mesh);collection.objects.link(obj)
    mat=bpy.data.materials.new('M3D_'+name);mat.use_nodes=True
    nodes=mat.node_tree.nodes;links=mat.node_tree.links
    bsdf=next(n for n in nodes if n.type=='BSDF_PRINCIPLED')
    if images[mid]:
        node=nodes.new('ShaderNodeTexImage');node.image=images[mid];links.new(node.outputs['Color'],bsdf.inputs['Base Color'])
    if ENABLE_PBR_NORMAL_ROUGHNESS and rough_images[mid]:
        node=nodes.new('ShaderNodeTexImage');node.image=rough_images[mid];links.new(node.outputs['Color'],bsdf.inputs['Roughness'])
    if ENABLE_PBR_NORMAL_ROUGHNESS and normal_images[mid]:
        node=nodes.new('ShaderNodeTexImage');node.image=normal_images[mid]
        norm=nodes.new('ShaderNodeNormalMap');links.new(node.outputs['Color'],norm.inputs['Color']);links.new(norm.outputs['Normal'],bsdf.inputs['Normal'])
    if DIAGNOSTIC_NEUTRAL_MATERIALS:
        # Disconnect texture color temporarily without destroying original images.
        bsdf.inputs['Base Color'].default_value=(0.65,0.65,0.65,1.0)
        for link in list(bsdf.inputs['Base Color'].links): links.remove(link)
    bsdf.inputs['Metallic'].default_value=0.0
    bsdf.inputs['Roughness'].default_value=0.85
    mesh.materials.append(mat)
    # Preserve explicit normals for source triangles when Blender supports it.
    normals = d['normals']
    if normals and len(normals) == len(mesh.loops) and all(v is not None for v in normals):
        try:
            mesh.normals_split_custom_set(normals)
        except (AttributeError, RuntimeError, ValueError) as exc:
            print('INFO: normals unavailable for', name, ':', exc)

# Reproducible daytime lighting. Never use an enormous experimental area light.
world=bpy.context.scene.world or bpy.data.worlds.new('M3D Daylight World')
bpy.context.scene.world=world
world.use_nodes=True
bg=world.node_tree.nodes.get('Background')
if bg:
    bg.inputs['Color'].default_value=(0.55,0.68,0.85,1.0)
    bg.inputs['Strength'].default_value=0.65
sun=bpy.data.lights.new('M3D Daylight Sun','SUN')
sun.energy=3.0
sun.angle=math.radians(6)
ob=bpy.data.objects.new('M3D Daylight Sun',sun)
collection.objects.link(ob)
ob.rotation_euler=(math.radians(28),math.radians(-25),math.radians(35))

def camera(name,location,target,lens=32):
    data=bpy.data.cameras.new(name)
    ob=bpy.data.objects.new(name,data)
    collection.objects.link(ob)
    ob.location=to_blender(location)
    target=Vector(to_blender(target))
    ob.rotation_euler=(target-ob.location).to_track_quat('-Z','Y').to_euler()
    data.lens=lens
    data.clip_end=5000
    return ob

# Original M3D camera positions are Z-up. Source-space locations are transformed
# exactly once, together with the imported geometry.
cams=[
    camera('CAM_facade',(0,-70,15),(0,-25,8),28),
    camera('CAM_foyer',(8,5,-22),(8,2,-30),30),
    camera('CAM_auditorium',(-32.5,3,-26),(-32.5,2.15,-33.4),45),
]
sc=bpy.context.scene
sc.camera=cams[0]
sc.render.engine='BLENDER_EEVEE'
sc.render.resolution_x=1920
sc.render.resolution_y=1080
sc.render.resolution_percentage=RENDER_PERCENT
sc.render.image_settings.file_format='PNG'
sc.render.film_transparent=False
sc.view_settings.view_transform='AgX'
# Disable motion blur and expensive viewport temporal effects for diagnostics.
try: sc.render.image_settings.color_mode='RGBA'
except (AttributeError,ValueError): pass
# Material preview in viewport, with the camera selected for immediate inspection.
for obj in bpy.context.selected_objects: obj.select_set(False)
cams[0].select_set(True)
bpy.context.view_layer.objects.active=cams[0]
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            area.spaces.active.region_3d.view_perspective='CAMERA'
            area.spaces.active.shading.type='MATERIAL'
print('INFO: axes',SOURCE_UP_AXIS,'PBR enabled',ENABLE_PBR_NORMAL_ROUGHNESS,
      'neutral materials',DIAGNOSTIC_NEUTRAL_MATERIALS)
# Never overwrite the existing Beta 3 file automatically.
output=os.path.join(os.path.dirname(BIN),'Energetik_Beta3_IMPORT_CORRIGE.blend')
if SAVE_PROJECT:
    bpy.ops.wm.save_as_mainfile(filepath=output)
    print('Saved diagnostic project:', output)
else:
    print('Import complete; use File > Save As to save a copy.')
print('Objects',nobj,'materials',nmat,'lights in M3D file',nlights)
print('PBR normal/roughness enabled:', ENABLE_PBR_NORMAL_ROUGHNESS)
print('WARNING: source primitives 1-5 are approximated by the original importer.')
print('IMPORTANT: this script has been syntax-checked, but not run against the 1.6 GB binary in Blender.')