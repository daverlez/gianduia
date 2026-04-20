import os
import math
import shutil
import struct
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

def export_texture_or_value(parent_node, input_socket, prop_name, export_dir, textures_dir_name="textures"):
    if input_socket.is_linked:
        from_node = input_socket.links[0].from_node

        if from_node.type == 'NORMAL_MAP':
            if from_node.inputs['Color'].is_linked:
                from_node = from_node.inputs['Color'].links[0].from_node
            else:
                return

        if from_node.type == 'TEX_IMAGE' and from_node.image:
            img = from_node.image
            img_path = bpy.path.abspath(img.filepath)

            textures_dir_path = os.path.join(export_dir, textures_dir_name)
            if not os.path.exists(textures_dir_path):
                os.makedirs(textures_dir_path)

            filename = os.path.basename(img_path)
            dest_path = os.path.join(textures_dir_path, filename)

            if os.path.exists(img_path):
                if not os.path.exists(dest_path) or not os.path.samefile(img_path, dest_path):
                    shutil.copy2(img_path, dest_path)

            rel_path = f"{textures_dir_name}/{filename}"

            tex_node = ET.SubElement(parent_node, "texture", type="image", name=prop_name)
            ET.SubElement(tex_node, "string", name="filename", value=rel_path)

            if img.colorspace_settings.name == 'sRGB':
                ET.SubElement(tex_node, "string", name="color_space", value="sRGB")
            else:
                ET.SubElement(tex_node, "string", name="color_space", value="linear")

            if from_node.interpolation == 'Closest':
                ET.SubElement(tex_node, "string", name="interpolation_mode", value="closest")
            else:
                ET.SubElement(tex_node, "string", name="interpolation_mode", value="linear")

            if from_node.extension == 'EXTEND':
                ET.SubElement(tex_node, "string", name="repeat_mode", value="clamp")
            else:
                ET.SubElement(tex_node, "string", name="repeat_mode", value="repeat")

            return

    val = input_socket.default_value
    if isinstance(val, float):
        ET.SubElement(parent_node, "float", name=prop_name, value=f"{val:.4f}")
    elif len(val) >= 3:
        ET.SubElement(parent_node, "color", name=prop_name, value=f"{val[0]:.4f} {val[1]:.4f} {val[2]:.4f}")

def export_volume_medium(parent_node, volume_node, name="inside"):
    med_node = ET.SubElement(parent_node, "medium", type="homogeneous", name=name)

    if volume_node.type == 'BSDF_PRINCIPLED_VOLUME':
        color = volume_node.inputs['Color'].default_value[:3]
        density = volume_node.inputs['Density'].default_value
        g = volume_node.inputs['Anisotropy'].default_value

        sig_s = [c * density for c in color]
        sig_a = [(1.0 - c) * density for c in color]

        ET.SubElement(med_node, "color", name="sigma_s", value=f"{sig_s[0]:.4f} {sig_s[1]:.4f} {sig_s[2]:.4f}")
        ET.SubElement(med_node, "color", name="sigma_a", value=f"{sig_a[0]:.4f} {sig_a[1]:.4f} {sig_a[2]:.4f}")
        if abs(g) > 1e-4:
            ET.SubElement(med_node, "float", name="g", value=f"{g:.4f}")

    elif volume_node.type == 'VOLUME_SCATTER':
        color = volume_node.inputs['Color'].default_value[:3]
        density = volume_node.inputs['Density'].default_value
        g = volume_node.inputs['Anisotropy'].default_value

        sig_s = [c * density for c in color]

        ET.SubElement(med_node, "color", name="sigma_s", value=f"{sig_s[0]:.4f} {sig_s[1]:.4f} {sig_s[2]:.4f}")
        ET.SubElement(med_node, "color", name="sigma_a", value="0.0000 0.0000 0.0000")
        if abs(g) > 1e-4:
            ET.SubElement(med_node, "float", name="g", value=f"{g:.4f}")

def export_material(prim_node, material, export_dir):
    fallback_mat = lambda: ET.SubElement(prim_node, "material", type="matte")

    if not material or not material.use_nodes:
        mat_node = fallback_mat()
        ET.SubElement(mat_node, "color", name="albedo", value="0.72 0.72 0.72")
        return

    nodes = material.node_tree.nodes
    output_node = next((n for n in nodes if n.type == 'OUTPUT_MATERIAL'), None)
    if not output_node:
        return

    surface_socket = output_node.inputs.get('Surface')
    volume_socket = output_node.inputs.get('Volume')

    surface_linked = surface_socket and surface_socket.is_linked
    volume_linked = volume_socket and volume_socket.is_linked

    if not surface_linked and not volume_linked:
        mat_node = fallback_mat()
        ET.SubElement(mat_node, "color", name="albedo", value="0.72 0.72 0.72")
        return

    mat_node = None

    if surface_linked:
        surface_node = surface_socket.links[0].from_node

        if surface_node.type == 'BSDF_PRINCIPLED':
            sss_weight = 0.0
            if 'Subsurface Weight' in surface_node.inputs:
                sss_weight = surface_node.inputs['Subsurface Weight'].default_value
            elif 'Subsurface' in surface_node.inputs:
                sss_weight = surface_node.inputs['Subsurface'].default_value

            if sss_weight > 0.01:
                mat_node = ET.SubElement(prim_node, "material", type="glass")
                ET.SubElement(mat_node, "float", name="roughness", value="0.3")
                ET.SubElement(mat_node, "float", name="eta", value="1.4")

                base_color = surface_node.inputs['Base Color'].default_value[:3]
                sss_scale = surface_node.inputs['Subsurface Scale'].default_value
                sss_radius = surface_node.inputs['Subsurface Radius'].default_value[:3]

                med_node = ET.SubElement(mat_node, "medium", type="homogeneous", name="inside")

                sig_s, sig_a = [0,0,0], [0,0,0]
                for i in range(3):
                    mfp = sss_radius[i] * sss_scale
                    sigma_t = 1.0 / max(mfp, 0.0001)
                    sig_s[i] = base_color[i] * sigma_t
                    sig_a[i] = max(sigma_t - sig_s[i], 0.0)

                ET.SubElement(med_node, "color", name="sigma_s", value=f"{sig_s[0]:.4f} {sig_s[1]:.4f} {sig_s[2]:.4f}")
                ET.SubElement(med_node, "color", name="sigma_a", value=f"{sig_a[0]:.4f} {sig_a[1]:.4f} {sig_a[2]:.4f}")
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
        volume_node = volume_socket.links[0].from_node

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
            start = curve.first_point_index
            length = curve.points_length
            if length < 2:
                continue

            for i in range(length - 1):
                idx0, idx1 = start + i, start + i + 1

                p0 = points[idx0].position
                p3 = points[idx1].position

                p1 = p0.lerp(p3, 1.0 / 3.0)
                p2 = p0.lerp(p3, 2.0 / 3.0)

                w0 = (radius_attr.data[idx0].value * 2.0) if radius_attr else default_radius
                w1 = (radius_attr.data[idx1].value * 2.0) if radius_attr else default_radius

                segments.append((p0, p1, p2, p3, w0, w1))

    elif obj_eval.type == 'CURVE':
        default_radius = obj_eval.data.bevel_depth if obj_eval.data.bevel_depth > 0 else 0.01

        for spline in obj_eval.data.splines:
            if spline.type == 'BEZIER':
                bps = spline.bezier_points
                for i in range(len(bps) - 1):
                    p0, p1 = bps[i].co, bps[i].handle_right
                    p2, p3 = bps[i+1].handle_left, bps[i+1].co

                    w0 = bps[i].radius * default_radius * 2.0
                    w1 = bps[i+1].radius * default_radius * 2.0

                    segments.append((p0, p1, p2, p3, w0, w1))

            elif spline.type == 'POLY':
                pts = spline.points
                for i in range(len(pts) - 1):
                    p0, p3 = pts[i].co.xyz, pts[i+1].co.xyz
                    p1, p2 = p0.lerp(p3, 1.0 / 3.0), p0.lerp(p3, 2.0 / 3.0)

                    w0 = pts[i].radius * default_radius * 2.0
                    w1 = pts[i+1].radius * default_radius * 2.0

                    segments.append((p0, p1, p2, p3, w0, w1))

    if not segments:
        return False

    with open(filepath, 'wb') as f:
        f.write(b'HAIR')
        f.write(struct.pack('<I', len(segments)))

        for seg in segments:
            f.write(struct.pack('<14f',
                                seg[0].x, seg[0].y, seg[0].z,
                                seg[1].x, seg[1].y, seg[1].z,
                                seg[2].x, seg[2].y, seg[2].z,
                                seg[3].x, seg[3].y, seg[3].z,
                                seg[4], seg[5]
                                ))

    return True