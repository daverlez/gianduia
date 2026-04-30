import os
import shutil
import xml.etree.ElementTree as ET
from xml.dom import minidom

class GianduiaToMitsuba:
    def __init__(self, input_file, output_file):
        self.input_file = input_file
        self.output_file = output_file
        self.input_dir = os.path.dirname(os.path.abspath(input_file))
        self.output_dir = os.path.dirname(os.path.abspath(output_file))

        self.gnd_tree = ET.parse(self.input_file)
        self.gnd_root = self.gnd_tree.getroot()

        self.mi_root = ET.Element("scene", version="3.0.0")

    def copy_asset(self, relative_path):
        src_path = os.path.join(self.input_dir, relative_path)
        dst_path = os.path.join(self.output_dir, relative_path)

        if os.path.exists(src_path):
            os.makedirs(os.path.dirname(dst_path), exist_ok=True)
            if not os.path.exists(dst_path) or not os.path.samefile(src_path, dst_path):
                shutil.copy2(src_path, dst_path)
        else:
            print(f"Warning: asset not found - {src_path}")

        return relative_path

    def add_param(self, parent, tag, name, value):
        return ET.SubElement(parent, tag, name=name, value=str(value))

    def copy_transform(self, gnd_transform, parent_node, target_name="to_world"):
        if gnd_transform is None:
            return

        mi_trans = ET.SubElement(parent_node, "transform", name=target_name)
        for child in gnd_transform:
            if child.tag == "rotate" and "axis" in child.attrib:
                axis_vals = child.attrib["axis"].split()
                if len(axis_vals) == 3:
                    ET.SubElement(mi_trans, "rotate",
                                  x=axis_vals[0],
                                  y=axis_vals[1],
                                  z=axis_vals[2],
                                  angle=child.attrib.get("angle", "0"))
                else:
                    ET.SubElement(mi_trans, child.tag, **child.attrib)
            else:
                ET.SubElement(mi_trans, child.tag, **child.attrib)

    def convert(self):
        print(f"==Gianduia2Mitsuba== \nConverting scene:\n  {self.input_file}\n  =>  {self.output_file}\n")

        self.convert_integrator()
        self.convert_camera_and_sampler()
        self.convert_world()
        self.convert_primitives()

        self.save_xml()
        print("Conversion completed successfully!")

    def convert_integrator(self):
        gnd_int = self.gnd_root.find("integrator")
        if gnd_int is not None:
            int_type = gnd_int.get("type")
            if int_type in ["path", "volpath"]:
                ET.SubElement(self.mi_root, "integrator", type=int_type)
            else:
                ET.SubElement(self.mi_root, "integrator", type="path")
        else:
            ET.SubElement(self.mi_root, "integrator", type="path")

    def convert_camera_and_sampler(self):
        gnd_cam = self.gnd_root.find("camera")
        gnd_sampler = self.gnd_root.find("sampler")

        if gnd_cam is None: return

        gnd_cam_type = gnd_cam.get("type")
        cam_type = "thinlens" if gnd_cam_type == "thinlens" else "perspective"

        mi_sensor = ET.SubElement(self.mi_root, "sensor", type=cam_type)

        gnd_trans = gnd_cam.find("transform")
        self.copy_transform(gnd_trans, mi_sensor, "to_world")

        fov_node = gnd_cam.find("float[@name='fov']")
        if fov_node is not None:
            self.add_param(mi_sensor, "float", "fov", fov_node.get("value"))

        if cam_type == "thinlens":
            lens_radius_node = gnd_cam.find("float[@name='lens_radius']")
            if lens_radius_node is not None:
                self.add_param(mi_sensor, "float", "aperture_radius", lens_radius_node.get("value"))

            focal_dist_node = gnd_cam.find("float[@name='focal_distance']")
            if focal_dist_node is not None:
                self.add_param(mi_sensor, "float", "focus_distance", focal_dist_node.get("value"))

        width_node = gnd_cam.find("integer[@name='width']")
        height_node = gnd_cam.find("integer[@name='height']")

        if width_node is not None and height_node is not None:
            mi_film = ET.SubElement(mi_sensor, "film", type="hdrfilm")
            self.add_param(mi_film, "integer", "width", width_node.get("value"))
            self.add_param(mi_film, "integer", "height", height_node.get("value"))

        if gnd_sampler is not None:
            spp_node = gnd_sampler.find("integer[@name='spp']")
            if spp_node is not None:
                mi_samp = ET.SubElement(mi_sensor, "sampler", type="independent")
                self.add_param(mi_samp, "integer", "sample_count", spp_node.get("value"))

    def convert_world(self):
        for gnd_emitter in self.gnd_root.findall("emitter"):
            if gnd_emitter.get("type") == "environment":
                mi_emitter = ET.SubElement(self.mi_root, "emitter", type="envmap")

                filename_node = gnd_emitter.find("string[@name='filename']")
                if filename_node is not None:
                    new_path = self.copy_asset(filename_node.get("value"))
                    self.add_param(mi_emitter, "string", "filename", new_path)

                strength_node = gnd_emitter.find("float[@name='strength']")
                if strength_node is not None:
                    self.add_param(mi_emitter, "float", "scale", strength_node.get("value"))

    def convert_primitives(self):
        shape_definitions = {}

        for gnd_prim in self.gnd_root.findall("primitive"):
            gnd_shape = gnd_prim.find("shape")
            if gnd_shape is not None:
                shape_id = gnd_shape.get("id")
                if shape_id:
                    shape_definitions[shape_id] = gnd_shape

        for gnd_prim in self.gnd_root.findall("primitive"):
            gnd_shape = gnd_prim.find("shape")
            gnd_ref = gnd_prim.find("ref")

            if gnd_ref is not None:
                ref_id = gnd_ref.get("id")
                gnd_shape = shape_definitions.get(ref_id)
                if gnd_shape is None:
                    print(f"Warning: reference not found for id {ref_id}")
                    continue

            if gnd_shape is None:
                continue

            if gnd_shape.get("type") == "mesh":
                mi_shape = ET.SubElement(self.mi_root, "shape", type="obj")

                filename_node = gnd_shape.find("string[@name='filename']")
                if filename_node is not None:
                    new_path = self.copy_asset(filename_node.get("value"))
                    self.add_param(mi_shape, "string", "filename", new_path)

                gnd_trans = gnd_prim.find("transform")
                self.copy_transform(gnd_trans, mi_shape, "to_world")

                gnd_mat = gnd_prim.find("material")
                self.convert_material(gnd_mat, mi_shape)

                gnd_emitter = gnd_prim.find("emitter")
                if gnd_emitter is not None and gnd_emitter.get("type") == "area":
                    mi_emitter = ET.SubElement(mi_shape, "emitter", type="area")
                    rad_node = gnd_emitter.find("color[@name='radiance']")
                    if rad_node is not None:
                        ET.SubElement(mi_emitter, "rgb", name="radiance", value=rad_node.get("value"))

    def map_property(self, gnd_parent, mi_parent, gnd_name, mi_name, default=None, square_float=False):
        tex_node = gnd_parent.find(f"texture[@name='{gnd_name}']")
        if tex_node is not None:
            filename_node = tex_node.find("string[@name='filename']")
            if filename_node is not None:
                mi_tex = ET.SubElement(mi_parent, "texture", type="bitmap", name=mi_name)
                new_path = self.copy_asset(filename_node.get("value"))
                self.add_param(mi_tex, "string", "filename", new_path)

                if tex_node.get("type") == "image_float":
                    self.add_param(mi_tex, "boolean", "raw", "true")
            return

        col_node = gnd_parent.find(f"color[@name='{gnd_name}']")
        if col_node is not None:
            ET.SubElement(mi_parent, "rgb", name=mi_name, value=col_node.get("value"))
            return

        flt_node = gnd_parent.find(f"float[@name='{gnd_name}']")
        if flt_node is not None:
            val = float(flt_node.get("value"))
            if square_float:
                val = val * val
            self.add_param(mi_parent, "float", mi_name, f"{val:.6f}")
            return

        if default is not None:
            val = default
            if square_float and isinstance(default, (int, float)):
                val = val * val

            if isinstance(default, (int, float)):
                self.add_param(mi_parent, "float", mi_name, f"{val:.6f}")
            else:
                ET.SubElement(mi_parent, "rgb", name=mi_name, value=str(default))
            return

    def convert_material(self, gnd_mat, mi_shape_node):
        if gnd_mat is None:
            return

        mat_type = gnd_mat.get("type")
        normal_tex = gnd_mat.find("texture[@name='normal']")

        if normal_tex is not None:
            outer_bsdf = ET.SubElement(mi_shape_node, "bsdf", type="normalmap")

            filename_node = normal_tex.find("string[@name='filename']")
            if filename_node is not None:
                new_path = self.copy_asset(filename_node.get("value"))
                nmap_tex = ET.SubElement(outer_bsdf, "texture", type="bitmap", name="normalmap")
                self.add_param(nmap_tex, "string", "filename", new_path)
                self.add_param(nmap_tex, "boolean", "raw", "true")

            mi_bsdf = ET.SubElement(outer_bsdf, "bsdf")
        else:
            mi_bsdf = ET.SubElement(mi_shape_node, "bsdf")

        if mat_type == "matte":
            mi_bsdf.set("type", "diffuse")
            self.map_property(gnd_mat, mi_bsdf, "albedo", "reflectance")

        elif mat_type == "disney":
            mi_bsdf.set("type", "principled")
            self.map_property(gnd_mat, mi_bsdf, "baseColor", "base_color")
            self.map_property(gnd_mat, mi_bsdf, "metallic", "metallic")
            self.map_property(gnd_mat, mi_bsdf, "roughness", "roughness") # Il principled di Mitsuba prende la roughness e si occupa lui di farci il quadrato all'interno
            self.map_property(gnd_mat, mi_bsdf, "specular", "specular")
            self.map_property(gnd_mat, mi_bsdf, "specularTransmission", "spec_trans")
            self.map_property(gnd_mat, mi_bsdf, "specularTint", "spec_tint")
            self.map_property(gnd_mat, mi_bsdf, "anisotropic", "anisotropic")
            self.map_property(gnd_mat, mi_bsdf, "sheen", "sheen")
            self.map_property(gnd_mat, mi_bsdf, "sheenTint", "sheen_tint")
            self.map_property(gnd_mat, mi_bsdf, "clearcoat", "clearcoat")
            self.map_property(gnd_mat, mi_bsdf, "clearcoatGloss", "clearcoat_gloss")
            self.map_property(gnd_mat, mi_bsdf, "eta", "eta")

        elif mat_type == "glass":
            mi_bsdf.set("type", "roughdielectric")
            self.add_param(mi_bsdf, "string", "distribution", "ggx")
            self.map_property(gnd_mat, mi_bsdf, "T", "specular_transmittance")
            self.map_property(gnd_mat, mi_bsdf, "R", "specular_reflectance")
            self.map_property(gnd_mat, mi_bsdf, "eta", "int_ior", default=1.5)
            self.map_property(gnd_mat, mi_bsdf, "roughness", "alpha", square_float=True)

        elif mat_type == "conductor":
            mi_bsdf.set("type", "roughconductor")
            self.add_param(mi_bsdf, "string", "distribution", "ggx")
            self.map_property(gnd_mat, mi_bsdf, "R", "specular_reflectance")
            self.map_property(gnd_mat, mi_bsdf, "eta", "eta")
            self.map_property(gnd_mat, mi_bsdf, "k", "k")
            self.map_property(gnd_mat, mi_bsdf, "roughness", "alpha", square_float=True)

    def save_xml(self):
        xml_string = ET.tostring(self.mi_root, encoding="utf-8")
        pretty_xml = minidom.parseString(xml_string).toprettyxml(indent="    ")

        os.makedirs(self.output_dir, exist_ok=True)
        with open(self.output_file, "w") as f:
            lines = pretty_xml.split("\n")
            f.write("\n".join(line for line in lines if line.strip()))


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Convertitore da Gianduia a Mitsuba 3")
    parser.add_argument("input", help="Percorso del file .xml di Gianduia")
    parser.add_argument("output", help="Percorso del file .xml di Mitsuba da generare")

    args = parser.parse_args()

    converter = GianduiaToMitsuba(args.input, args.output)
    converter.convert()