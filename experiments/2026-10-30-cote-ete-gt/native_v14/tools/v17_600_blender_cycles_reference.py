"""Render the packaged V17.600 cupola geometry in Blender Cycles for comparison.

Run with:
  blender -b -t 8 --python v17_600_blender_cycles_reference.py -- \
    --scene-root /path/to/extracted/benchmark-civic \
    --output /path/to/blender-reference.png --samples 64
"""
import argparse
import bpy
import json
import math
import sys
from pathlib import Path
from mathutils import Matrix, Vector


def arguments():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scene-root", required=True,
                        help="Extracted benchmark-civic directory from the V17.600 scene package")
    parser.add_argument("--output", default="V17.600_Blender_Cycles_reference.png")
    parser.add_argument("--samples", type=int, default=64)
    parser.add_argument("--glass-thickness", type=float, default=0.03,
                        help="Physical thickness in metres used by Cycles (engine uses thin-sheet transmission)")
    return parser.parse_args(argv)


args = arguments()
root = Path(args.scene_root)
meshdata = json.loads((root / "geometry/v523-pool-cupola.json").read_text(encoding="utf-8"))["meshes"]
matdata = {m["id"]: m for m in json.loads((root / "materials/v442-corrections.json").read_text(encoding="utf-8"))["materials"]}

# Rebuild a clean deterministic scene.
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = max(1, args.samples)
scene.cycles.use_denoising = False
scene.cycles.max_bounces = 12
scene.cycles.transmission_bounces = 8
scene.cycles.transparent_max_bounces = 8
scene.render.resolution_x = 1000
scene.render.resolution_y = 680
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.film_transparent = False
scene.view_settings.view_transform = "Standard"
scene.view_settings.look = "Medium High Contrast"
scene.view_settings.exposure = 0
scene.view_settings.gamma = 1
scene.render.filepath = str(Path(args.output).resolve())


def principled(name, color, roughness=0.5, metallic=0.0, transmission=0.0, ior=1.5):
    material = bpy.data.materials.new(name)
    material.diffuse_color = (*color, 1)
    material.use_nodes = True
    shader = material.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = (*color, 1)
    shader.inputs["Roughness"].default_value = roughness
    shader.inputs["Metallic"].default_value = metallic
    shader.inputs["Transmission Weight"].default_value = transmission
    shader.inputs["IOR"].default_value = ior
    return material


glass_spec = matdata["v600_tinted_glass"]
glass = principled("V17.600 tinted glass | Cycles dielectric", glass_spec["baseColor"],
                   glass_spec["roughness"], glass_spec["metallic"],
                   glass_spec["transmission"], glass_spec["ior"])
frame = principled("Dark structural steel", (0.11, 0.12, 0.12), 0.58, 0.58)
steel = principled("Brushed inox", (0.53, 0.57, 0.58), 0.28, 0.82)

triangles = 0
for item in meshdata:
    ident = item["id"]
    if not item.get("faces") or ident.startswith("v600_cupola_guide"):
        continue
    material = glass if ident.startswith("v600_cupola_leaf_") else (steel if ident.endswith("roof_curb") else frame)
    mesh = bpy.data.meshes.new(ident)
    mesh.from_pydata(item["vertices"], [], item["faces"])
    mesh.update()
    obj = bpy.data.objects.new(ident, mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material)
    triangles += len(item["faces"])
    if ident.startswith("v600_cupola_leaf_") and args.glass_thickness > 0:
        # Cycles gets a closed dielectric pane; CSE's authored mesh remains a thin sheet.
        modifier = obj.modifiers.new("Physical glazing thickness", "SOLIDIFY")
        modifier.thickness = args.glass_thickness
        modifier.offset = -1.0
        modifier.use_rim = True

# Capture camera from the V17.600 overlay: world up is +Y. Build its basis explicitly
# to match the CSE camera ray equations (avoids Blender track-quaternion roll ambiguity).
width, height = 1000, 680
fov = math.radians(70.0)
yaw = math.radians(306.7)
pitch = math.radians(36.5)
eye = Vector((0.0, 2.245, -13.0))
forward = Vector((math.sin(yaw) * math.cos(pitch), math.sin(pitch),
                  -math.cos(yaw) * math.cos(pitch))).normalized()
right = forward.cross(Vector((0, 1, 0))).normalized()
up = right.cross(forward).normalized()
camera_data = bpy.data.cameras.new("V17.600 capture camera")
camera = bpy.data.objects.new("V17.600 capture camera", camera_data)
bpy.context.collection.objects.link(camera)
camera.location = eye
camera.rotation_euler = Matrix(((right.x, up.x, -forward.x),
                                (right.y, up.y, -forward.y),
                                (right.z, up.z, -forward.z))).to_euler()
camera_data.lens = 36
camera_data.sensor_width = 36
camera_data.lens_unit = "FOV"
camera_data.angle = fov
scene.camera = camera

world = bpy.data.worlds.new("Clear blue comparison sky")
scene.world = world
world.use_nodes = True
background = world.node_tree.nodes.get("Background")
background.inputs["Color"].default_value = (0.31, 0.51, 0.69, 1)
background.inputs["Strength"].default_value = 0.72

# Small distant emitter tests if the glass introduces an unintended split/warp.
tan_half = math.tan(fov / 2)
aspect = width / height
px, py = 520, 210
rx = (2 * (px + 0.5) / width - 1) * tan_half
ry = (1 - 2 * (py + 0.5) / height) * tan_half / aspect
direction = (forward + right * rx + up * ry).normalized()
center = eye + direction * 90
bpy.ops.mesh.primitive_circle_add(vertices=48, radius=0.40, fill_type="NGON", location=center)
disk = bpy.context.object
disk.name = "Distant luminous disk for transmission check"
disk.rotation_euler = (-direction).to_track_quat("Z", "Y").to_euler()
emission = bpy.data.materials.new("Warm white distant emitter")
emission.use_nodes = True
nodes = emission.node_tree
nodes.nodes.clear()
out = nodes.nodes.new("ShaderNodeOutputMaterial")
light_shader = nodes.nodes.new("ShaderNodeEmission")
light_shader.inputs["Color"].default_value = (1.0, 0.91, 0.68, 1)
light_shader.inputs["Strength"].default_value = 12
nodes.links.new(light_shader.outputs[0], out.inputs["Surface"])
disk.data.materials.append(emission)

area_data = bpy.data.lights.new("Large soft comparison light", "AREA")
area = bpy.data.objects.new("Large soft comparison light", area_data)
bpy.context.collection.objects.link(area)
area.location = (0, 45, -30)
area_data.energy = 15000
area_data.shape = "DISK"
area_data.size = 10
area.rotation_euler = (Vector((0, 20, -30)) - area.location).to_track_quat("-Z", "Y").to_euler()

scene.render.threads_mode = "FIXED"
scene.render.threads = 0
bpy.ops.render.render(write_still=True)
print(f"REFERENCE_RENDER {scene.render.filepath} samples={scene.cycles.samples} "
      f"ior={glass_spec['ior']} transmission={glass_spec['transmission']} "
      f"glass_thickness_m={args.glass_thickness} triangles={triangles}")
