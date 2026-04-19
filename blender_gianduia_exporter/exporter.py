import os
import xml.etree.ElementTree as ET
from xml.dom import minidom
from . import data_extractors

def export_scene(context, filepath, export_meshes):
    base_dir = os.path.dirname(filepath)
    meshes_dir_name = "meshes"
    meshes_dir_path = os.path.join(base_dir, meshes_dir_name)

    if export_meshes and not os.path.exists(meshes_dir_path):
        os.makedirs(meshes_dir_path)

    root = ET.Element("scene")

    data_extractors.export_render_settings(root, context.scene)
    data_extractors.export_camera(root, context.scene, context.evaluated_depsgraph_get())

    data_extractors.export_world(root, context.scene, base_dir)

    depsgraph = context.evaluated_depsgraph_get()
    exported_meshes = set()

    for inst in depsgraph.object_instances:
        obj_eval = inst.object

        if obj_eval.type == 'MESH':
            prim_node = ET.SubElement(root, "primitive")
            data_extractors.export_transform(prim_node, inst.matrix_world)

            mesh_name = obj_eval.original.data.name

            if mesh_name not in exported_meshes:
                shape_node = ET.SubElement(prim_node, "shape", type="mesh", id=mesh_name)
                rel_obj_path = f"{meshes_dir_name}/{mesh_name}.obj"
                ET.SubElement(shape_node, "string", name="filename", value=rel_obj_path)

                if export_meshes:
                    abs_obj_path = os.path.join(base_dir, rel_obj_path)
                    data_extractors.write_local_obj(obj_eval.data, abs_obj_path)

                exported_meshes.add(mesh_name)
            else:
                ET.SubElement(prim_node, "ref", id=mesh_name)

            data_extractors.export_material(prim_node, obj_eval.active_material, base_dir)
            radiance = data_extractors.get_emission_data(obj_eval.active_material)
            if radiance:
                emitter_node = ET.SubElement(prim_node, "emitter", type="area")
                ET.SubElement(emitter_node, "color", name="radiance", value=f"{radiance[0]:.4f} {radiance[1]:.4f} {radiance[2]:.4f}")

        elif obj_eval.type == 'LIGHT':
            light = obj_eval.data

            color, strength = data_extractors.get_light_data(light)

            power_r = color[0] * strength * light.energy
            power_g = color[1] * strength * light.energy
            power_b = color[2] * strength * light.energy

            if light.type == 'POINT':
                emitter_node = ET.SubElement(root, "emitter", type="point")

                pos = inst.matrix_world.translation
                ET.SubElement(emitter_node, "point", name="position", x=f"{pos.x:.6f}", y=f"{pos.y:.6f}", z=f"{pos.z:.6f}")
                ET.SubElement(emitter_node, "color", name="power", value=f"{power_r:.4f} {power_g:.4f} {power_b:.4f}")

            elif light.type == 'AREA':
                prim_node = ET.SubElement(root, "primitive")
                data_extractors.export_transform(prim_node, inst.matrix_world)

                mesh_name = f"LightMesh_{obj_eval.original.name}"

                shape_node = ET.SubElement(prim_node, "shape", type="mesh", id=mesh_name)
                rel_obj_path = f"{meshes_dir_name}/{mesh_name}.obj"
                ET.SubElement(shape_node, "string", name="filename", value=rel_obj_path)

                if export_meshes:
                    abs_obj_path = os.path.join(base_dir, rel_obj_path)
                    data_extractors.write_area_light_obj(light, abs_obj_path)

                mat_node = ET.SubElement(prim_node, "material", type="matte")
                ET.SubElement(mat_node, "color", name="albedo", value="0 0 0")

                emitter_node = ET.SubElement(prim_node, "emitter", type="area")
                ET.SubElement(emitter_node, "color", name="radiance", value=f"{power.r:.4f} {power.g:.4f} {power.b:.4f}")

    xml_string = ET.tostring(root, encoding="utf-8")
    parsed_xml = minidom.parseString(xml_string)
    pretty_xml = parsed_xml.toprettyxml(indent="    ")

    with open(filepath, "w") as f:
        f.write("\n".join(pretty_xml.split("\n")[1:]))

    return {'FINISHED'}