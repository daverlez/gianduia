import os
import mathutils
import bpy
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

        elif obj_eval.type == 'VOLUME':
            volume_data = obj_eval.original.data

            if not volume_data.filepath:
                continue

            prim_node = ET.SubElement(root, "primitive")

            bbox = [mathutils.Vector(b) for b in obj_eval.bound_box]
            min_pt = mathutils.Vector((min([v.x for v in bbox]), min([v.y for v in bbox]), min([v.z for v in bbox])))
            max_pt = mathutils.Vector((max([v.x for v in bbox]), max([v.y for v in bbox]), max([v.z for v in bbox])))

            extents = (max_pt - min_pt) / 2.0
            extents *= 1.05

            local_center = (max_pt + min_pt) / 2.0
            translation_mat = mathutils.Matrix.Translation(local_center)
            world_matrix = inst.matrix_world @ translation_mat
            data_extractors.export_transform(prim_node, world_matrix, name="toWorld")

            shape_node = ET.SubElement(prim_node, "shape", type="cube")
            ET.SubElement(shape_node, "vector", name="extents", value=f"{extents.x:.6f} {extents.y:.6f} {extents.z:.6f}")

            mat_node = ET.SubElement(prim_node, "material", type="index")

            volumes_dir_name = "volumes"
            volumes_dir_path = os.path.join(base_dir, volumes_dir_name)
            if not os.path.exists(volumes_dir_path):
                os.makedirs(volumes_dir_path)

            vdb_abs_path = bpy.path.abspath(volume_data.filepath)
            filename = os.path.basename(vdb_abs_path)
            dest_path = os.path.join(volumes_dir_path, filename)

            import shutil
            if os.path.exists(vdb_abs_path):
                if not os.path.exists(dest_path) or not os.path.samefile(vdb_abs_path, dest_path):
                    shutil.copy2(vdb_abs_path, dest_path)

            med_node = ET.SubElement(mat_node, "medium", type="heterogeneous", name="inside")
            ET.SubElement(med_node, "string", name="filename", value=f"{volumes_dir_name}/{filename}")

            grid_name = "density"
            if volume_data.grids:
                grid_name = volume_data.grids[0].name
            ET.SubElement(med_node, "string", name="gridName", value=grid_name)

            world_to_local_matrix = inst.matrix_world.inverted()
            data_extractors.export_transform(med_node, world_to_local_matrix, name="toLocal")

            material = obj_eval.active_material
            if material and material.use_nodes:
                nodes = material.node_tree.nodes
                output_node = next((n for n in nodes if n.type == 'OUTPUT_MATERIAL'), None)
                if output_node and output_node.inputs['Volume'].is_linked:
                    vol_node = output_node.inputs['Volume'].links[0].from_node

                    if vol_node.type == 'PRINCIPLED_VOLUME':
                        density = vol_node.inputs['Density'].default_value * 10.0
                        color = vol_node.inputs['Color'].default_value[:3]
                        anisotropy = vol_node.inputs['Anisotropy'].default_value

                        bb_intensity = 0.0
                        if 'Blackbody Intensity' in vol_node.inputs:
                            bb_intensity = vol_node.inputs['Blackbody Intensity'].default_value
                            temperature = vol_node.inputs['Temperature'].default_value

                        ET.SubElement(med_node, "float", name="densityScale", value=f"{density:.4f}")

                        if abs(anisotropy) > 1e-4:
                            ET.SubElement(med_node, "float", name="g", value=f"{anisotropy:.4f}")

                        if bb_intensity > 0.0:
                            ET.SubElement(med_node, "color", name="sigma_s", value="0.1000 0.1000 0.1000")
                            ET.SubElement(med_node, "color", name="sigma_a", value="0.9000 0.9000 0.9000")

                            ET.SubElement(med_node, "string", name="temperatureGrid", value="temperature") # o 'flame' se necessario
                            ET.SubElement(med_node, "float", name="temperatureScale", value=f"{temperature/2.0:.1f}")
                            ET.SubElement(med_node, "float", name="temperatureOffset", value="800.0")
                            ET.SubElement(med_node, "float", name="emissionScale", value=f"{bb_intensity * 2.0:.4f}")
                        else:
                            sig_s = [c for c in color]
                            sig_a = [max(1.0 - c, 0.0) for c in color]
                            ET.SubElement(med_node, "color", name="sigma_s", value=f"{sig_s[0]:.4f} {sig_s[1]:.4f} {sig_s[2]:.4f}")
                            ET.SubElement(med_node, "color", name="sigma_a", value=f"{sig_a[0]:.4f} {sig_a[1]:.4f} {sig_a[2]:.4f}")

    xml_string = ET.tostring(root, encoding="utf-8")
    parsed_xml = minidom.parseString(xml_string)
    pretty_xml = parsed_xml.toprettyxml(indent="    ")

    with open(filepath, "w") as f:
        f.write("\n".join(pretty_xml.split("\n")[1:]))

    return {'FINISHED'}