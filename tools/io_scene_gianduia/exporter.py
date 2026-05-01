import math
import os
import mathutils
import bpy
import xml.etree.ElementTree as ET
from xml.dom import minidom
from . import data_extractors

def handle_mesh(root, inst, obj_eval, base_dir, meshes_dir_name, export_meshes, exported_meshes):
    prim_node = ET.SubElement(root, "primitive")
    data_extractors.export_transform(prim_node, inst.matrix_world)

    mesh_name = obj_eval.original.data.name
    if mesh_name not in exported_meshes:
        shape_node = ET.SubElement(prim_node, "shape", type="mesh", id=mesh_name)
        rel_obj_path = f"{meshes_dir_name}/{mesh_name}.obj"
        data_extractors.add_string(shape_node, "filename", rel_obj_path)

        if export_meshes:
            data_extractors.write_local_obj(obj_eval.data, os.path.join(base_dir, rel_obj_path))
        exported_meshes.add(mesh_name)
    else:
        ET.SubElement(prim_node, "ref", id=mesh_name)

    data_extractors.export_material(prim_node, obj_eval.active_material, base_dir)

    radiance = data_extractors.get_emission_data(obj_eval.active_material)
    if radiance:
        emitter_node = ET.SubElement(prim_node, "emitter", type="area")
        data_extractors.add_color(emitter_node, "radiance", *radiance)

def handle_curves(root, inst, obj_eval, base_dir, meshes_dir_name, export_meshes, exported_meshes):
    prim_node = ET.SubElement(root, "primitive")
    data_extractors.export_transform(prim_node, inst.matrix_world)

    curve_name = obj_eval.original.data.name
    if curve_name not in exported_meshes:
        shape_node = ET.SubElement(prim_node, "shape", type="curve_array", id=curve_name)
        rel_obj_path = f"{meshes_dir_name}/{curve_name}.hair"
        data_extractors.add_string(shape_node, "filename", rel_obj_path)

        if export_meshes:
            success = data_extractors.write_local_hair_binary(obj_eval, os.path.join(base_dir, rel_obj_path))
            if not success:
                root.remove(prim_node)
                return
        exported_meshes.add(curve_name)
    else:
        ET.SubElement(prim_node, "ref", id=curve_name)

    material = None

    if getattr(obj_eval, 'material_slots', None):
        material = next((slot.material for slot in obj_eval.material_slots if slot.material), None)

    if not material and getattr(obj_eval.original.data, 'materials', None):
        material = next((m for m in obj_eval.original.data.materials if m), None)

    data_extractors.export_material(prim_node, material, base_dir)

def handle_light(root, inst, obj_eval, base_dir, meshes_dir_name, export_meshes):
    light = obj_eval.data
    color, strength = data_extractors.get_light_data(light)

    power_r = color[0] * strength * light.energy
    power_g = color[1] * strength * light.energy
    power_b = color[2] * strength * light.energy

    if light.type == 'POINT':
        emitter_node = ET.SubElement(root, "emitter", type="point")
        pos = inst.matrix_world.translation
        ET.SubElement(emitter_node, "point", name="position", x=f"{pos.x:.6f}", y=f"{pos.y:.6f}", z=f"{pos.z:.6f}")
        data_extractors.add_color(emitter_node, "power", power_r, power_g, power_b)

    elif light.type == 'AREA':
        prim_node = ET.SubElement(root, "primitive")
        data_extractors.export_transform(prim_node, inst.matrix_world)

        mesh_name = f"LightMesh_{obj_eval.original.name}"
        shape_node = ET.SubElement(prim_node, "shape", type="mesh", id=mesh_name)
        rel_obj_path = f"{meshes_dir_name}/{mesh_name}.obj"
        data_extractors.add_string(shape_node, "filename", rel_obj_path)

        if export_meshes:
            data_extractors.write_area_light_obj(light, os.path.join(base_dir, rel_obj_path))

        mat_node = ET.SubElement(prim_node, "material", type="matte")
        data_extractors.add_color(mat_node, "albedo", 0, 0, 0)

        emitter_node = ET.SubElement(prim_node, "emitter", type="area")
        data_extractors.add_color(emitter_node, "radiance", power_r, power_g, power_b)

    elif light.type == 'SPOT':
        emitter_node = ET.SubElement(root, "emitter", type="spot")
        pos = inst.matrix_world.translation

        local_dir = mathutils.Vector((0.0, 0.0, -1.0))
        world_dir = (inst.matrix_world.to_3x3() @ local_dir).normalized()

        ET.SubElement(emitter_node, "point", name="position", x=f"{pos.x:.6f}", y=f"{pos.y:.6f}", z=f"{pos.z:.6f}")
        ET.SubElement(emitter_node, "vector", name="direction", x=f"{world_dir.x:.6f}", y=f"{world_dir.y:.6f}", z=f"{world_dir.z:.6f}")

        data_extractors.add_color(emitter_node, "power", power_r, power_g, power_b)

        spot_size_deg = math.degrees(light.spot_size)
        data_extractors.add_float(emitter_node, "spot_size", spot_size_deg)
        data_extractors.add_float(emitter_node, "blend", light.spot_blend)

def handle_volume(root, inst, obj_eval, base_dir):
    volume_data = obj_eval.original.data
    if not volume_data.filepath: return

    prim_node = ET.SubElement(root, "primitive")

    bbox = [mathutils.Vector(b) for b in obj_eval.bound_box]
    min_pt = mathutils.Vector((min([v.x for v in bbox]), min([v.y for v in bbox]), min([v.z for v in bbox])))
    max_pt = mathutils.Vector((max([v.x for v in bbox]), max([v.y for v in bbox]), max([v.z for v in bbox])))

    extents = ((max_pt - min_pt) / 2.0) * 1.05
    local_center = (max_pt + min_pt) / 2.0

    world_matrix = inst.matrix_world @ mathutils.Matrix.Translation(local_center)
    data_extractors.export_transform(prim_node, world_matrix, name="toWorld")

    shape_node = ET.SubElement(prim_node, "shape", type="cube")
    ET.SubElement(shape_node, "vector", name="extents", value=f"{extents.x:.6f} {extents.y:.6f} {extents.z:.6f}")

    mat_node = ET.SubElement(prim_node, "material", type="index")

    vdb_abs_path = bpy.path.abspath(volume_data.filepath)
    filename = data_extractors.copy_asset(vdb_abs_path, os.path.join(base_dir, "volumes"))

    med_node = ET.SubElement(mat_node, "medium", type="heterogeneous", name="inside")
    data_extractors.add_string(med_node, "filename", f"volumes/{filename}")
    data_extractors.add_string(med_node, "gridName", volume_data.grids[0].name if volume_data.grids else "density")

    data_extractors.export_transform(med_node, inst.matrix_world.inverted(), name="toLocal")

    material = obj_eval.active_material
    if not material or not material.use_nodes: return

    output_node = next((n for n in material.node_tree.nodes if n.type == 'OUTPUT_MATERIAL'), None)
    if output_node and output_node.inputs['Volume'].is_linked:
        vol_node = output_node.inputs['Volume'].links[0].from_node

        if vol_node.type == 'PRINCIPLED_VOLUME':
            data_extractors.add_float(med_node, "densityScale", vol_node.inputs['Density'].default_value * 10.0)

            anisotropy = vol_node.inputs['Anisotropy'].default_value
            if abs(anisotropy) > 1e-4: data_extractors.add_float(med_node, "g", anisotropy)

            bb_intensity = vol_node.inputs.get('Blackbody Intensity')
            if bb_intensity and bb_intensity.default_value > 0.0:
                data_extractors.add_color(med_node, "sigma_s", 0.1, 0.1, 0.1)
                data_extractors.add_color(med_node, "sigma_a", 0.9, 0.9, 0.9)
                data_extractors.add_string(med_node, "temperatureGrid", "temperature")
                data_extractors.add_float(med_node, "temperatureScale", vol_node.inputs['Temperature'].default_value / 2.0)
                data_extractors.add_float(med_node, "temperatureOffset", 800.0)
                data_extractors.add_float(med_node, "emissionScale", bb_intensity.default_value * 2.0)
            else:
                color = vol_node.inputs['Color'].default_value[:3]
                data_extractors.add_color(med_node, "sigma_s", *color)
                data_extractors.add_color(med_node, "sigma_a", *[max(1.0 - c, 0.0) for c in color])

def export_scene(context, filepath, export_meshes):
    base_dir = os.path.dirname(filepath)
    meshes_dir_name = "meshes"

    if export_meshes:
        os.makedirs(os.path.join(base_dir, meshes_dir_name), exist_ok=True)

    root = ET.Element("scene")
    data_extractors.export_render_settings(root, context.scene)
    data_extractors.export_camera(root, context.scene, context.evaluated_depsgraph_get())
    data_extractors.export_world(root, context.scene, base_dir)

    depsgraph = context.evaluated_depsgraph_get()
    exported_meshes = set()

    for inst in depsgraph.object_instances:
        obj_eval = inst.object

        if obj_eval.type == 'MESH':
            handle_mesh(root, inst, obj_eval, base_dir, meshes_dir_name, export_meshes, exported_meshes)
        elif obj_eval.type in {'CURVES', 'CURVE'}:
            handle_curves(root, inst, obj_eval, base_dir, meshes_dir_name, export_meshes, exported_meshes)
        elif obj_eval.type == 'LIGHT':
            handle_light(root, inst, obj_eval, base_dir, meshes_dir_name, export_meshes)
        elif obj_eval.type == 'VOLUME':
            handle_volume(root, inst, obj_eval, base_dir)

    xml_string = ET.tostring(root, encoding="utf-8")
    pretty_xml = minidom.parseString(xml_string).toprettyxml(indent="    ")

    with open(filepath, "w") as f:
        f.write("\n".join(pretty_xml.split("\n")[1:]))

    return {'FINISHED'}