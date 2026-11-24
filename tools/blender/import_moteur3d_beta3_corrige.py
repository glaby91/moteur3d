"""Import M3D1314 v5 scene binary in Blender 5.x; preserve source triangles, UV and normals.
Usage: blender --python import_moteur3d_beta3_blender.py -- /path/to/scene_pripyat_central.bin
Or run in Scripting; defaults to scene_pripyat_central.bin beside this script.
"""
import bpy, struct, math, os, sys, mmap  # type: ignore[reportMissingImports]
from mathutils import Vector  # type: ignore[reportMissingImports]
from collections import defaultdict, Counter
from enum import IntEnum
from typing import Any, Literal, overload

raw: memoryview
pos = 0

script_path = globals().get('__file__', '')
ROOT = os.path.dirname(os.path.abspath(script_path)) if script_path and script_path.lower().endswith('.py') else os.path.dirname(bpy.data.filepath)
# Default location matching the user's extracted Beta 3 folder.
DEFAULT_BIN = r'C:\Users\charles\Desktop\Moteur\beta3\scene_pripyat_central.bin'
args = sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
BIN = args[0] if args else (os.path.join(ROOT, 'scene_pripyat_central.bin') if os.path.isfile(os.path.join(ROOT, 'scene_pripyat_central.bin')) else DEFAULT_BIN)
if not os.path.isfile(BIN):
    raise FileNotFoundError('Fichier scène Beta 3 introuvable: '+BIN+' (copiez scene_pripyat_central.bin à côté du script)')
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


class PrimitiveKind(IntEnum):
    TRIANGLE = 0
    BOX = 1
    ROUNDED_BOX = 2
    CAPSULE = 3
    CYLINDER = 4
    SPHERE = 5


@overload
def read(fmt: Literal['I']) -> int: ...
@overload
def read(fmt: Literal['IIIiIIII', 'BBH', 'fff', '24f', '8f', '7f', '13f', '6f']) -> tuple[Any, ...]: ...
def read(fmt: str) -> int | tuple[Any, ...]:
    global pos
    result=struct.unpack_from('<'+fmt,raw,pos);pos+=struct.calcsize('<'+fmt)
    return result[0] if len(result)==1 else result

def read_header():
    global pos
    magic = bytes(raw[:8])
    pos = 8
    if magic.rstrip(b'\x00') != b'M3D1314':
        raise ValueError('Format scène inconnu: ' + str(magic))
    version, nobj, nbvh, _, norder, nmat, _, texsize = read('IIIiIIII')
    if version != 5:
        raise ValueError('Version de scène non supportée: ' + str(version))
    return nobj, nbvh, norder, nmat, texsize


def read_materials(nmat):
    global pos
    materials = []
    for _ in range(nmat):
        name = bytes(raw[pos:pos + 24]).split(b'\0')[0].decode('utf8', 'replace')
        pos += 24
        reflect, transmission, ior = read('BBH')
        materials.append((name, reflect, transmission, ior))
    return materials

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


def import_triangle(mat, *_):
    v = read('24f')
    tri(mat, [v[0:3], v[3:6], v[6:9]],
        [v[18:20], v[20:22], v[22:24]],
        [v[9:12], v[12:15], v[15:18]])


def import_box(mat, lo, hi):
    cube(mat, lo, hi)


def import_rounded_box(mat, lo, hi):
    read('7f')
    cube(mat, lo, hi)


def import_capsule(mat, *_):
    v = read('8f')
    a, b = v[:3], v[3:6]
    cylinder(mat, a, b, v[6])
    uv_sphere(mat, a, (v[6],) * 3, 6, 10)
    uv_sphere(mat, b, (v[6],) * 3, 6, 10)


def import_cylinder(mat, *_):
    v = read('13f')
    cylinder(mat, v[:3], v[3:6], v[6], v[7])


def import_sphere(mat, *_):
    v = read('6f')
    uv_sphere(mat, v[:3], v[3:6])


PRIMITIVE_IMPORTERS = {
    PrimitiveKind.TRIANGLE: import_triangle,
    PrimitiveKind.BOX: import_box,
    PrimitiveKind.ROUNDED_BOX: import_rounded_box,
    PrimitiveKind.CAPSULE: import_capsule,
    PrimitiveKind.CYLINDER: import_cylinder,
    PrimitiveKind.SPHERE: import_sphere,
}


def read_geometry(nobj):
    global meshdata, counts
    meshdata = defaultdict(lambda: {'verts': [], 'faces': [], 'uv': [], 'normals': []})
    counts = Counter()
    for _ in range(nobj):
        kind, mat, _ = read('BBH')
        lo, hi = read('fff'), read('fff')
        counts[kind] += 1
        try:
            primitive = PrimitiveKind(kind)
        except ValueError as exc:
            raise ValueError('Unknown primitive type ' + str(kind)) from exc
        PRIMITIVE_IMPORTERS[primitive](mat, lo, hi)
    print('Primitive distribution:', dict(counts))


def read_textures(materials, nbvh, norder):
    global pos
    import numpy as np

    pos += nbvh * 40 + norder * 4
    images = []
    for name, *_ in materials:
        levels = read('I')
        image = None
        for level in range(levels):
            size = read('I')
            n = size * size
            if level == 0:
                # Native color is packed 0xRRGGBB uint32, use NumPy for fast conversion.
                pixels = np.frombuffer(raw, dtype='<u4', count=n, offset=pos)
                rgba = np.empty((n, 4), dtype=np.float32)
                rgba[:, 0] = ((pixels >> 16) & 255) / 255
                rgba[:, 1] = ((pixels >> 8) & 255) / 255
                rgba[:, 2] = (pixels & 255) / 255
                rgba[:, 3] = 1
                image = bpy.data.images.new('M3D_' + name, width=size, height=size, alpha=True)
                image.pixels.foreach_set(rgba.ravel())
                image.pack()
            pos += n * 4
        images.append(image)

    normal_images, rough_images = [], []
    for name, *_ in materials:
        for channel, result in (("normal", normal_images), ("roughness", rough_images)):
            levels = read('I')
            image = None
            for level in range(levels):
                size = read('I')
                n = size * size
                if level == 0 and ENABLE_PBR_NORMAL_ROUGHNESS:
                    packed = np.frombuffer(raw, dtype='<u2', count=n, offset=pos)
                    rgba = np.empty((n, 4), dtype=np.float32)
                    rgba[:, 0] = ((packed >> 11) & 31) / 31
                    rgba[:, 1] = ((packed >> 5) & 63) / 63
                    rgba[:, 2] = (packed & 31) / 31
                    rgba[:, 3] = 1
                    image = bpy.data.images.new(
                        'M3D_' + name + '_' + channel, width=size, height=size, alpha=True
                    )
                    image.pixels.foreach_set(rgba.ravel())
                    image.colorspace_settings.name = 'Non-Color'
                    image.pack()
                pos += n * 2
            result.append(image)

    lights = [read('8f') for _ in range(read('I'))]
    if pos != len(raw):
        print('WARNING: unparsed bytes:', len(raw) - pos)
    return images, normal_images, rough_images, lights

def create_objects(materials, images, normal_images, rough_images):
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    collection = bpy.data.collections.new('ENERGETIK_BETA3')
    bpy.context.scene.collection.children.link(collection)
    for mid, data in meshdata.items():
        name = materials[mid][0]
        mesh = bpy.data.meshes.new('mesh_' + name)
        mesh.from_pydata(data['verts'], [], data['faces'])
        mesh.update()
        uv = mesh.uv_layers.new(name='Source UV')
        for i, polygon in enumerate(uv.data):
            polygon.uv = data['uv'][i]
        obj = bpy.data.objects.new('material_' + name, mesh)
        collection.objects.link(obj)
        material = bpy.data.materials.new('M3D_' + name)
        material.use_nodes = True
        nodes, links = material.node_tree.nodes, material.node_tree.links
        bsdf = next(node for node in nodes if node.type == 'BSDF_PRINCIPLED')
        if images[mid]:
            node = nodes.new('ShaderNodeTexImage')
            node.image = images[mid]
            links.new(node.outputs['Color'], bsdf.inputs['Base Color'])
        if ENABLE_PBR_NORMAL_ROUGHNESS and rough_images[mid]:
            node = nodes.new('ShaderNodeTexImage')
            node.image = rough_images[mid]
            links.new(node.outputs['Color'], bsdf.inputs['Roughness'])
        if ENABLE_PBR_NORMAL_ROUGHNESS and normal_images[mid]:
            node = nodes.new('ShaderNodeTexImage')
            node.image = normal_images[mid]
            normal_map = nodes.new('ShaderNodeNormalMap')
            links.new(node.outputs['Color'], normal_map.inputs['Color'])
            links.new(normal_map.outputs['Normal'], bsdf.inputs['Normal'])
        if DIAGNOSTIC_NEUTRAL_MATERIALS:
            bsdf.inputs['Base Color'].default_value = (0.65, 0.65, 0.65, 1.0)
            for link in list(bsdf.inputs['Base Color'].links):
                links.remove(link)
        bsdf.inputs['Metallic'].default_value = 0.0
        bsdf.inputs['Roughness'].default_value = 0.85
        mesh.materials.append(material)

        normals = data['normals']
        if normals and len(normals) == len(mesh.loops) and all(v is not None for v in normals):
            try:
                mesh.normals_split_custom_set(normals)
            except (AttributeError, RuntimeError, ValueError) as exc:
                print('INFO: normals unavailable for', name, ':', exc)
    return collection


def setup_lighting(collection):
    world = bpy.context.scene.world or bpy.data.worlds.new('M3D Daylight World')
    bpy.context.scene.world = world
    world.use_nodes = True
    background = world.node_tree.nodes.get('Background')
    if background:
        background.inputs['Color'].default_value = (0.55, 0.68, 0.85, 1.0)
        background.inputs['Strength'].default_value = 0.65
    sun = bpy.data.lights.new('M3D Daylight Sun', 'SUN')
    sun.energy = 3.0
    sun.angle = math.radians(6)
    sun_object = bpy.data.objects.new('M3D Daylight Sun', sun)
    collection.objects.link(sun_object)
    sun_object.rotation_euler = (math.radians(28), math.radians(-25), math.radians(35))


def camera(collection, name, location, target, lens=32):
    data=bpy.data.cameras.new(name)
    ob=bpy.data.objects.new(name,data)
    collection.objects.link(ob)
    ob.location=to_blender(location)
    target=Vector(to_blender(target))
    ob.rotation_euler=(target-ob.location).to_track_quat('-Z','Y').to_euler()
    data.lens=lens
    data.clip_end=5000
    return ob


def setup_scene(collection):
    # Original M3D camera positions are Z-up. Transform source positions once.
    cameras = [
        camera(collection, 'CAM_facade', (0, -70, 15), (0, -25, 8), 28),
        camera(collection, 'CAM_foyer', (8, 5, -22), (8, 2, -30), 30),
        camera(collection, 'CAM_auditorium', (-32.5, 3, -26), (-32.5, 2.15, -33.4), 45),
    ]
    scene = bpy.context.scene
    scene.camera = cameras[0]
    scene.render.engine = 'BLENDER_EEVEE'
    scene.render.resolution_x = 1920
    scene.render.resolution_y = 1080
    scene.render.resolution_percentage = RENDER_PERCENT
    scene.render.image_settings.file_format = 'PNG'
    scene.render.film_transparent = False
    scene.view_settings.view_transform = 'AgX'
    try:
        scene.render.image_settings.color_mode = 'RGBA'
    except (AttributeError, ValueError):
        pass
    for obj in bpy.context.selected_objects:
        obj.select_set(False)
    cameras[0].select_set(True)
    bpy.context.view_layer.objects.active = cameras[0]
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == 'VIEW_3D':
                area.spaces.active.region_3d.view_perspective = 'CAMERA'
                area.spaces.active.shading.type = 'MATERIAL'
    return cameras


def report_import(nobj, nmat, nlights):
    print('INFO: axes', SOURCE_UP_AXIS, 'PBR enabled', ENABLE_PBR_NORMAL_ROUGHNESS,
          'neutral materials', DIAGNOSTIC_NEUTRAL_MATERIALS)
    output = os.path.join(os.path.dirname(BIN), 'Energetik_Beta3_IMPORT_CORRIGE.blend')
    if SAVE_PROJECT:
        bpy.ops.wm.save_as_mainfile(filepath=output)
        print('Saved diagnostic project:', output)
    else:
        print('Import complete; use File > Save As to save a copy.')
    print('Objects', nobj, 'materials', nmat, 'lights in M3D file', nlights)
    print('PBR normal/roughness enabled:', ENABLE_PBR_NORMAL_ROUGHNESS)
    print('WARNING: source primitives 1-5 are approximated by the original importer.')
    print('IMPORTANT: this script has been syntax-checked, but not run against the 1.6 GB binary in Blender.')


def main():
    global raw, pos
    if not os.path.isfile(BIN):
        raise FileNotFoundError(
            'Fichier scène Beta 3 introuvable: ' + BIN +
            ' (copiez scene_pripyat_central.bin à côté du script)'
        )
    with open(BIN, 'rb') as bin_handle:
        with mmap.mmap(bin_handle.fileno(), 0, access=mmap.ACCESS_READ) as mapped:
            raw = memoryview(mapped)
            try:
                pos = 0
                nobj, nbvh, norder, nmat, texsize = read_header()
                materials = read_materials(nmat)
                print('M3D Beta3:', nobj, 'objects,', nmat, 'materials, texture size', texsize)
                read_geometry(nobj)
                images, normal_images, rough_images, lights = read_textures(materials, nbvh, norder)
                collection = create_objects(materials, images, normal_images, rough_images)
                setup_lighting(collection)
                setup_scene(collection)
                report_import(nobj, nmat, len(lights))
            finally:
                raw.release()


if __name__ == '__main__':
    main()