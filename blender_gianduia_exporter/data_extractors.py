import os
import math
import shutil
import struct
import bmesh
import bpy
import mathutils
import xml.etree.ElementTree as ET


# XML HELPER FUNCTIONS

def add_param(parent, tag, name=None, value=None, **kwargs):
    attrs = {}
    if name is not None: attrs["name"] = name
    if value is not None: attrs["value"] = value
    attrs.update(kwargs)
    return ET.SubElement(parent, tag, **attrs)

def add_float(parent, name, value):
    return add_param(parent, "float", name, f"{value:.4f}")

def add_string(parent, name, value):
    return add_param(parent, "string", name, value)

def add_color(parent, name, r, g, b):
    return add_param(parent, "color", name, f"{r:.4f} {g:.4f} {b:.4f}")

def add_integer(parent, name, value):
    return add_param(parent, "integer", name, str(value))

def copy_asset(filepath, target_dir):
    """Copia un file nella cartella di destinazione se non esiste o è diverso."""
    if not os.path.exists(target_dir):
        os.makedirs(target_dir)
    filename = os.path.basename(filepath)
    dest_path = os.path.join(target_dir, filename)
    if os.path.exists(filepath):
        if not os.path.exists(dest_path) or not os.path.samefile(filepath, dest_path):
            shutil.copy2(filepath, dest_path)
    return filename


# EXTRACTION LOGIC

def export_transform(parent_node, matrix, name="toWorld"):
    trans_node = ET.SubElement(parent_node, "transform", name=name)
    loc, rot_quat, scale = matrix.decompose()

    if any(abs(s - 1.0) > 1e-4 for s in scale):
        ET.SubElement(trans_node, "scale", x=f"{scale.x:.6f}", y=f"{scale.y:.6f}", z=f"{scale.z:.6f}")

    axis, angle_rad = rot_quat.to_axis_angle()
    angle_deg = math.degrees(angle_rad)

    if abs(angle_deg) > 1e-4:
        axis_str = f"{axis.x:.6f} {axis.y:.6f} {axis.z:.6f}"
        ET.SubElement(trans_node, "rotate", axis=axis_str, angle=f"{angle_deg:.6f}")

    if any(abs(l) > 1e-4 for l in loc):
        ET.SubElement(trans_node, "translate", x=f"{loc.x:.6f}", y=f"{loc.y:.6f}", z=f"{loc.z:.6f}")


def get_effective_fov_y(scene, cam_data):
    res_x = scene.render.resolution_x * scene.render.pixel_aspect_x
    res_y = scene.render.resolution_y * scene.render.pixel_aspect_y
    render_aspect = res_x / res_y

    fit = cam_data.sensor_fit
    if fit == 'AUTO':
        fit = 'HORIZONTAL' if render_aspect >= 1.0 else 'VERTICAL'

    if fit == 'HORIZONTAL':
        fov_y_rad = 2.0 * math.atan(math.tan(cam_data.angle / 2.0) / render_aspect)
        return math.degrees(fov_y_rad)
    return math.degrees(cam_data.angle if fit == 'VERTICAL' else cam_data.angle_y)


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
    forward = (matrix.to_3x3() @ mathutils.Vector((0.0, 0.0, -1.0))).normalized()
    up = (matrix.to_3x3() @ mathutils.Vector((0.0, 1.0, 0.0))).normalized()
    target = origin + forward

    ET.SubElement(trans_node, "lookat",
                  origin=f"{origin.x:.6f}, {origin.y:.6f}, {origin.z:.6f}",
                  target=f"{target.x:.6f}, {target.y:.6f}, {target.z:.6f}",
                  up=f"{up.x:.6f}, {up.y:.6f}, {up.z:.6f}")

    add_float(cam_node, "fov", get_effective_fov_y(scene, cam_data))
    add_integer(cam_node, "width", scene.render.resolution_x)
    add_integer(cam_node, "height", scene.render.resolution_y)

    ET.SubElement(cam_node, "filter", type="box")
    add_param(cam_node, "boolean", "rolling_shutter", "false")
    add_float(cam_node, "scan_duration", 0.9)
    add_float(cam_node, "exposure_duration", 0.1)


def export_render_settings(root_node, scene):
    spp = scene.cycles.samples if hasattr(scene, 'cycles') else 2056
    sampler_node = ET.SubElement(root_node, "sampler", type="independent")
    add_integer(sampler_node, "spp", spp)
    ET.SubElement(root_node, "integrator", type="path")


def write_local_obj(mesh_data, filepath):
    bm = bmesh.new()
    bm.from_mesh(mesh_data)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bm.normal_update()

    with open(filepath, 'w') as f:
        f.write(f"# Exported for Gianduia Renderer\no {mesh_data.name}\n")

        for v in bm.verts:
            f.write(f"v {v.co.x:.6f} {v.co.y:.6f} {v.co.z:.6f}\n")

        for face in bm.faces:
            for loop in face.loops:
                n = loop.vert.normal if face.smooth else face.normal
                f.write(f"vn {n.x:.6f} {n.y:.6f} {n.z:.6f}\n")

        n_idx = 1
        for face in bm.faces:
            verts_str = [f"{loop.vert.index + 1}//{n_idx + i}" for i, loop in enumerate(face.loops)]
            f.write(f"f {' '.join(verts_str)}\n")
            n_idx += len(face.loops)

    bm.free()


def export_world(root_node, scene, export_dir):
    if not scene.world or not scene.world.use_nodes:
        return

    bg_node = scene.world.node_tree.nodes.get("Background")
    if not bg_node: return

    color_input = bg_node.inputs['Color']
    if color_input.is_linked:
        env_node = color_input.links[0].from_node
        if env_node.type == 'TEX_ENVIRONMENT' and env_node.image:
            img_path = bpy.path.abspath(env_node.image.filepath)

            if not img_path.lower().endswith('.exr'):
                print(f"ATTENZIONE: Gianduia accetta solo .exr. Trovato: {img_path}")

            filename = copy_asset(img_path, export_dir)

            env_xml = ET.SubElement(root_node, "emitter", type="environment")
            add_string(env_xml, "filename", filename)
            add_float(env_xml, "strength", bg_node.inputs['Strength'].default_value)


def write_area_light_obj(light_data, filepath):
    bm = bmesh.new()
    sx, sy = light_data.size / 2.0, light_data.size_y / 2.0 if light_data.shape in {'RECTANGLE', 'ELLIPSE'} else light_data.size / 2.0

    v1, v2, v3, v4 = [bm.verts.new(co) for co in [(-sx, -sy, 0), (sx, -sy, 0), (sx, sy, 0), (-sx, sy, 0)]]
    face = bm.faces.new((v4, v3, v2, v1))
    bmesh.ops.recalc_face_normals(bm, faces=[face])

    with open(filepath, 'w') as f:
        f.write("# Area Light Mesh for Gianduia\no AreaLight\n")
        for v in bm.verts: f.write(f"v {v.co.x:.6f} {v.co.y:.6f} {v.co.z:.6f}\n")
        for v in bm.verts: f.write(f"vn {v.normal.x:.6f} {v.normal.y:.6f} {v.normal.z:.6f}\n")
        f.write("f 1//1 2//2 3//3 4//4\n")

    bm.free()


def get_emission_data(material):
    if not material or not material.use_nodes: return None

    output_node = next((n for n in material.node_tree.nodes if n.type == 'OUTPUT_MATERIAL'), None)
    if not output_node or not output_node.inputs['Surface'].is_linked: return None

    surface_node = output_node.inputs['Surface'].links[0].from_node
    color, strength = (0.0, 0.0, 0.0), 0.0

    if surface_node.type == 'EMISSION':
        color = surface_node.inputs['Color'].default_value[:3]
        strength = surface_node.inputs['Strength'].default_value
    elif surface_node.type == 'BSDF_PRINCIPLED':
        color_socket = surface_node.inputs.get('Emission Color') or surface_node.inputs.get('Emission')
        if color_socket: color = color_socket.default_value[:3]
        strength = surface_node.inputs['Emission Strength'].default_value

    if strength > 0.0 and sum(color) > 0.0:
        return [c * strength for c in color]
    return None


def get_light_data(light):
    color, strength = light.color[:3], 1.0
    if light.use_nodes and light.node_tree:
        emission_node = next((n for n in light.node_tree.nodes if n.type == 'EMISSION'), None)
        if emission_node:
            color = emission_node.inputs['Color'].default_value[:3]
            strength = emission_node.inputs['Strength'].default_value
    return color, strength


def export_texture_or_value(parent_node, input_socket, prop_name, export_dir, textures_dir_name="textures"):
    if input_socket.is_linked:
        from_node = input_socket.links[0].from_node
        if from_node.type == 'NORMAL_MAP':
            if not from_node.inputs['Color'].is_linked: return
            from_node = from_node.inputs['Color'].links[0].from_node

        if from_node.type == 'TEX_IMAGE' and from_node.image:
            img = from_node.image
            filename = copy_asset(bpy.path.abspath(img.filepath), os.path.join(export_dir, textures_dir_name))

            tex_node = ET.SubElement(parent_node, "texture", type="image", name=prop_name)
            add_string(tex_node, "filename", f"{textures_dir_name}/{filename}")
            add_string(tex_node, "color_space", "sRGB" if img.colorspace_settings.name == 'sRGB' else "linear")
            add_string(tex_node, "interpolation_mode", "closest" if from_node.interpolation == 'Closest' else "linear")
            add_string(tex_node, "repeat_mode", "clamp" if from_node.extension == 'EXTEND' else "repeat")
            return

    val = input_socket.default_value
    if isinstance(val, float):
        add_float(parent_node, prop_name, val)
    elif len(val) >= 3:
        add_color(parent_node, prop_name, val[0], val[1], val[2])


def export_volume_medium(parent_node, volume_node, name="inside"):
    med_node = ET.SubElement(parent_node, "medium", type="homogeneous", name=name)
    color = volume_node.inputs['Color'].default_value[:3]
    density = volume_node.inputs['Density'].default_value
    g = volume_node.inputs['Anisotropy'].default_value

    sig_s = [c * density for c in color]
    add_color(med_node, "sigma_s", *sig_s)

    if volume_node.type == 'BSDF_PRINCIPLED_VOLUME':
        sig_a = [(1.0 - c) * density for c in color]
        add_color(med_node, "sigma_a", *sig_a)
    elif volume_node.type == 'VOLUME_SCATTER':
        add_color(med_node, "sigma_a", 0.0, 0.0, 0.0)

    if abs(g) > 1e-4: add_float(med_node, "g", g)


def export_material(prim_node, material, export_dir):
    def fallback():
        mat_node = ET.SubElement(prim_node, "material", type="matte")
        add_color(mat_node, "albedo", 0.72, 0.72, 0.72)

    if not material or not material.use_nodes:
        return fallback()

    output_node = next((n for n in material.node_tree.nodes if n.type == 'OUTPUT_MATERIAL'), None)
    if not output_node: return fallback()

    surface_linked = output_node.inputs.get('Surface') and output_node.inputs['Surface'].is_linked
    volume_linked = output_node.inputs.get('Volume') and output_node.inputs['Volume'].is_linked

    if not surface_linked and not volume_linked: return fallback()

    mat_node = None
    if surface_linked:
        surface_node = output_node.inputs['Surface'].links[0].from_node

        if surface_node.type == 'BSDF_PRINCIPLED':
            sss_weight = surface_node.inputs.get('Subsurface Weight', surface_node.inputs.get('Subsurface'))
            sss_weight_val = sss_weight.default_value if sss_weight else 0.0

            if sss_weight_val > 0.01:
                mat_node = ET.SubElement(prim_node, "material", type="glass")
                add_float(mat_node, "roughness", 0.3)
                add_float(mat_node, "eta", 1.4)

                base_color = surface_node.inputs['Base Color'].default_value[:3]
                sss_scale = surface_node.inputs['Subsurface Scale'].default_value
                sss_radius = surface_node.inputs['Subsurface Radius'].default_value[:3]

                med_node = ET.SubElement(mat_node, "medium", type="homogeneous", name="inside")
                sig_s, sig_a = [0,0,0], [0,0,0]

                for i in range(3):
                    sigma_t = 1.0 / max(sss_radius[i] * sss_scale, 0.0001)
                    sig_s[i] = base_color[i] * sigma_t
                    sig_a[i] = max(sigma_t - sig_s[i], 0.0)

                add_color(med_node, "sigma_s", *sig_s)
                add_color(med_node, "sigma_a", *sig_a)
            else:
                mat_node = ET.SubElement(prim_node, "material", type="matte")
                export_texture_or_value(mat_node, surface_node.inputs['Base Color'], "albedo", export_dir)
                export_texture_or_value(mat_node, surface_node.inputs['Normal'], "normal", export_dir)

        elif surface_node.type == 'BSDF_DIFFUSE':
            mat_node = ET.SubElement(prim_node, "material", type="matte")
            export_texture_or_value(mat_node, surface_node.inputs['Color'], "albedo", export_dir)
            export_texture_or_value(mat_node, surface_node.inputs['Normal'], "normal", export_dir)

        elif surface_node.type == 'BSDF_GLASS':
            mat_node = ET.SubElement(prim_node, "material", type="glass")
            export_texture_or_value(mat_node, surface_node.inputs['Color'], "T", export_dir)
            export_texture_or_value(mat_node, surface_node.inputs['Roughness'], "roughness", export_dir)
            export_texture_or_value(mat_node, surface_node.inputs['IOR'], "eta", export_dir)
            export_texture_or_value(mat_node, surface_node.inputs['Normal'], "normal", export_dir)

        elif surface_node.type == 'BSDF_GLOSSY':
            mat_node = ET.SubElement(prim_node, "material", type="conductor")
            export_texture_or_value(mat_node, surface_node.inputs['Color'], "R", export_dir)
            export_texture_or_value(mat_node, surface_node.inputs['Roughness'], "roughness", export_dir)
            export_texture_or_value(mat_node, surface_node.inputs['Normal'], "normal", export_dir)

    if volume_linked:
        volume_node = output_node.inputs['Volume'].links[0].from_node
        if mat_node is None:
            mat_node = ET.SubElement(prim_node, "material", type="index")
        export_volume_medium(mat_node, volume_node, "inside")


def write_local_hair_binary(obj_eval, filepath):
    segments = []

    if obj_eval.type == 'CURVES':
        curves_data = obj_eval.data
        points = curves_data.points
        radius_attr = curves_data.attributes.get('radius')
        default_radius = 0.005

        for curve in curves_data.curves:
            start, length = curve.first_point_index, curve.points_length
            if length < 2: continue

            for i in range(length - 1):
                idx0, idx1 = start + i, start + i + 1
                p0, p3 = points[idx0].position, points[idx1].position
                p1, p2 = p0.lerp(p3, 1.0/3.0), p0.lerp(p3, 2.0/3.0)

                w0 = (radius_attr.data[idx0].value * 2.0) if radius_attr else default_radius
                w1 = (radius_attr.data[idx1].value * 2.0) if radius_attr else default_radius
                segments.append((p0, p1, p2, p3, w0, w1))

    elif obj_eval.type == 'CURVE':
        default_radius = obj_eval.data.bevel_depth if obj_eval.data.bevel_depth > 0 else 0.01

        for spline in obj_eval.data.splines:
            if spline.type == 'BEZIER':
                bps = spline.bezier_points
                for i in range(len(bps) - 1):
                    segments.append((
                        bps[i].co, bps[i].handle_right, bps[i+1].handle_left, bps[i+1].co,
                        bps[i].radius * default_radius * 2.0, bps[i+1].radius * default_radius * 2.0
                    ))
            elif spline.type == 'POLY':
                pts = spline.points
                for i in range(len(pts) - 1):
                    p0, p3 = pts[i].co.xyz, pts[i+1].co.xyz
                    segments.append((
                        p0, p0.lerp(p3, 1.0/3.0), p0.lerp(p3, 2.0/3.0), p3,
                        pts[i].radius * default_radius * 2.0, pts[i+1].radius * default_radius * 2.0
                    ))

    if not segments: return False

    with open(filepath, 'wb') as f:
        f.write(b'HAIR')
        f.write(struct.pack('<I', len(segments)))
        for seg in segments:
            f.write(struct.pack('<14f',
                                seg[0].x, seg[0].y, seg[0].z, seg[1].x, seg[1].y, seg[1].z,
                                seg[2].x, seg[2].y, seg[2].z, seg[3].x, seg[3].y, seg[3].z,
                                seg[4], seg[5]
                                ))

    return True