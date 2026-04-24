bl_info = {
    "name": "Gianduia Renderer Exporter",
    "author": "Davide Verlezza",
    "version": (1, 0, 0),
    "blender": (5, 1, 0),
    "location": "File > Export > Gianduia Scene (.xml)",
    "description": "Export the scene to Gianduia format",
    "warning": "",
    "category": "Import-Export",
}

import bpy
from bpy_extras.io_utils import ExportHelper
from bpy.props import StringProperty, BoolProperty
from . import exporter

class ExportGianduia(bpy.types.Operator, ExportHelper):
    bl_idname = "export_scene.gianduia"
    bl_label = "Export Gianduia"

    filename_ext = ".xml"
    filter_glob: StringProperty(
        default="*.xml",
        options={'HIDDEN'},
        maxlen=255,
    )

    export_meshes: BoolProperty(
        name="Export Meshes",
        description="Export meshes in .obj format, under mesh/ subfolder.",
        default=True,
    )

    def execute(self, context):
        return exporter.export_scene(context, self.filepath, self.export_meshes)

def menu_func_export(self, context):
    self.layout.operator(ExportGianduia.bl_idname, text="Gianduia Scene (.xml)")

def register():
    bpy.utils.register_class(ExportGianduia)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)

def unregister():
    bpy.utils.unregister_class(ExportGianduia)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)

if __name__ == "__main__":
    register()