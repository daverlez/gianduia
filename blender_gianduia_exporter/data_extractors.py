import os
import math
import shutil
import bmesh
import bpy
import mathutils
import xml.etree.ElementTree as ET

def export_transform(parent_node, matrix, name="toWorld"):
    trans_node = ET.SubElement(parent_node, "transform", name=name)
    loc, rot_quat, scale = matrix.decompose()

    if abs(scale.x - 1.0) > 1e-4 or abs(scale.y - 1.0) > 1e-4 or abs(scale.z - 1.0) > 1e-4:
        ET.SubElement(trans_node, "scale", x=f"{scale.x:.6f}", y=f"{scale.y:.6f}", z=f"{scale.z:.6f}")

    axis, angle_rad = rot_quat.to_axis_angle()
    angle_deg = math.degrees(angle_rad)

    if abs(angle_deg) > 1e-4:
        axis_str = f"{axis.x:.6f} {axis.y:.6f} {axis.z:.6f}"
        ET.SubElement(trans_node, "rotate", axis=axis_str, angle=f"{angle_deg:.6f}")

    # 3. Traslazione
    if abs(loc.x) > 1e-4 or abs(loc.y) > 1e-4 or abs(loc.z) > 1e-4:
        ET.SubElement(trans_node, "translate", x=f"{loc.x:.6f}", y=f"{loc.y:.6f}", z=f"{loc.z:.6f}")


def get_effective_fov_y(scene, cam_data):
    res_x = scene.render.resolution_x * scene.render.pixel_aspect_x
    res_y = scene.render.resolution_y * scene.render.pixel_aspect_y
    render_aspect = res_x / res_y

    fit = cam_data.sensor_fit
    if fit == 'AUTO':
        fit = 'HORIZONTAL' if render_aspect >= 1.0 else 'VERTICAL'

    if fit == 'HORIZONTAL':
        fov_x = cam_data.angle
        fov_y_rad = 2.0 * math.atan(math.tan(fov_x / 2.0) / render_aspect)
        return math.degrees(fov_y_rad)
    elif fit == 'VERTICAL':
        return math.degrees(cam_data.angle)

    return math.degrees(cam_data.angle_y)

def export_camera(root_node, scene, depsgraph):
    camera_obj = scene.camera
    if not camera_obj:
        return

    camera_eval = depsgraph.objects.get(camera_obj.name, camera_obj)
    cam_data = camera_eval.data

    cam_node = ET.SubElement(root_node, "camera", type="thinlens")
    trans_node = ET.SubElement(cam_node, "transform", name="toWorld")

    matrix = camera_eval.matrix_world.normalized()

    origin = matrix.translation

    forward = matrix.to_3x3() @ mathutils.Vector((0.0, 0.0, -1.0))
    forward.normalize()
    target = origin + forward

    up = matrix.to_3x3() @ mathutils.Vector((0.0, 1.0, 0.0))
    up.normalize()

    origin_str = f"{origin.x:.6f}, {origin.y:.6f}, {origin.z:.6f}"
    target_str = f"{target.x:.6f}, {target.y:.6f}, {target.z:.6f}"
    up_str = f"{up.x:.6f}, {up.y:.6f}, {up.z:.6f}"

    ET.SubElement(trans_node, "lookat", origin=origin_str, target=target_str, up=up_str)

    fov_deg = get_effective_fov_y(scene, cam_data)
    ET.SubElement(cam_node, "float", name="fov", value=f"{fov_deg:.2f}")

    res_x = scene.render.resolution_x
    res_y = scene.render.resolution_y
    ET.SubElement(cam_node, "integer", name="width", value=str(res_x))
    ET.SubElement(cam_node, "integer", name="height", value=str(res_y))

    ET.SubElement(cam_node, "filter", type="box")
    ET.SubElement(cam_node, "boolean", name="rolling_shutter", value="false")
    ET.SubElement(cam_node, "float", name="scan_duration", value="0.9")
    ET.SubElement(cam_node, "float", name="exposure_duration", value="0.1")

def export_render_settings(root_node, scene):
    spp = 2056
    if hasattr(scene, 'cycles'):
        spp = scene.cycles.samples

    sampler_node = ET.SubElement(root_node, "sampler", type="independent") #
    ET.SubElement(sampler_node, "integer", name="spp", value=str(spp)) #

    ET.SubElement(root_node, "integrator", type="path") #


def write_local_obj(mesh_data, filepath):
    bm = bmesh.new()
    bm.from_mesh(mesh_data)

    bmesh.ops.triangulate(bm, faces=bm.faces[:])

    bm.normal_update()

    with open(filepath, 'w') as f:
        f.write(f"# Exported for Gianduia Renderer\n")
        f.write(f"o {mesh_data.name}\n")

        for v in bm.verts:
            f.write(f"v {v.co.x:.6f} {v.co.y:.6f} {v.co.z:.6f}\n")

        for face in bm.faces:
            for loop in face.loops:
                n = loop.vert.normal if face.smooth else face.normal
                f.write(f"vn {n.x:.6f} {n.y:.6f} {n.z:.6f}\n")

        n_idx = 1
        for face in bm.faces:
            verts_str = []
            for loop in face.loops:
                verts_str.append(f"{loop.vert.index + 1}//{n_idx}")
                n_idx += 1
            f.write(f"f {' '.join(verts_str)}\n")

    bm.free()

def export_world(root_node, scene, export_dir):
    world = scene.world
    if not world or not world.use_nodes:
        return

    bg_node = world.node_tree.nodes.get("Background")
    if not bg_node:
        return

    strength = bg_node.inputs['Strength'].default_value

    color_input = bg_node.inputs['Color']
    if color_input.is_linked:
        env_node = color_input.links[0].from_node
        if env_node.type == 'TEX_ENVIRONMENT' and env_node.image:
            img_path = bpy.path.abspath(env_node.image.filepath)

            if os.path.exists(img_path):
                if not img_path.lower().endswith('.exr'):
                    print(f"ATTENZIONE: Gianduia accetta solo .exr. Trovato: {img_path}")

                filename = os.path.basename(img_path)
                dest_path = os.path.join(export_dir, filename)

                if not os.path.exists(dest_path) or not os.path.samefile(img_path, dest_path):
                    shutil.copy2(img_path, dest_path)

                env_xml = ET.SubElement(root_node, "emitter", type="environment")
                ET.SubElement(env_xml, "string", name="filename", value=filename)
                ET.SubElement(env_xml, "float", name="strength", value=f"{strength:.4f}")

def write_area_light_obj(light_data, filepath):
    bm = bmesh.new()

    sx = light_data.size / 2.0
    sy = light_data.size_y / 2.0 if light_data.shape in {'RECTANGLE', 'ELLIPSE'} else sx

    v1 = bm.verts.new((-sx, -sy, 0))
    v2 = bm.verts.new((sx, -sy, 0))
    v3 = bm.verts.new((sx, sy, 0))
    v4 = bm.verts.new((-sx, sy, 0))

    face = bm.faces.new((v4, v3, v2, v1))
    bmesh.ops.recalc_face_normals(bm, faces=[face])

    with open(filepath, 'w') as f:
        f.write("# Area Light Mesh for Gianduia\n")
        f.write("o AreaLight\n")
        for v in bm.verts:
            f.write(f"v {v.co.x:.6f} {v.co.y:.6f} {v.co.z:.6f}\n")
        for v in bm.verts:
            f.write(f"vn {v.normal.x:.6f} {v.normal.y:.6f} {v.normal.z:.6f}\n")
        f.write("f 1//1 2//2 3//3 4//4\n")

    bm.free()

def get_emission_data(material):
    if not material or not material.use_nodes:
        return None

    nodes = material.node_tree.nodes

    output_node = next((n for n in nodes if n.type == 'OUTPUT_MATERIAL'), None)
    if not output_node or not output_node.inputs['Surface'].is_linked:
        return None

    surface_node = output_node.inputs['Surface'].links[0].from_node

    color = (0.0, 0.0, 0.0)
    strength = 0.0

    if surface_node.type == 'EMISSION':
        color = surface_node.inputs['Color'].default_value[:3]
        strength = surface_node.inputs['Strength'].default_value

    elif surface_node.type == 'BSDF_PRINCIPLED':
        if 'Emission Color' in surface_node.inputs:
            color = surface_node.inputs['Emission Color'].default_value[:3]
        elif 'Emission' in surface_node.inputs:
            color = surface_node.inputs['Emission'].default_value[:3]

        strength = surface_node.inputs['Emission Strength'].default_value

    if strength > 0.0 and sum(color) > 0.0:
        return (color[0] * strength, color[1] * strength, color[2] * strength)

    return None

def get_light_data(light):
    color = light.color[:3]
    strength = 1.0

    if light.use_nodes and light.node_tree:
        emission_node = next((n for n in light.node_tree.nodes if n.type == 'EMISSION'), None)

        if emission_node:
            color = emission_node.inputs['Color'].default_value[:3]
            strength = emission_node.inputs['Strength'].default_value

    return color, strength