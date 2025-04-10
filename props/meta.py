import bpy
from bpy.props import *

class MoonRayEXRHeaderAttribute(bpy.types.PropertyGroup):
    name: StringProperty(name="Name", description="Metadata name")
    type: EnumProperty(name="Type", 
        description="Allowed types for exr headers:\n* box2i\n* box2f\n* chromaticities\n* double\n* float\n* int\n* m33f\n* m44f\n* string\n* v2i\n* v2f\n* v3i\n* v3f", 
        items = [
            ("BOX2I", "box2i", ""),
            ("BOX2F", "box2f", ""),
            ("CHROMATICITIES", "chromaticities", ""),
            ("DOUBLE", "double", ""),
            ("FLOAT", "float", ""),
            ("INT", "int", ""),
            ("M33F", "m33f", ""),
            ("M44F", "m44f", ""),
            ("STRING", "string", ""),
            ("V2I", "v2i", ""),
            ("V2F", "v2f", ""),
            ("V3I", "v3i", ""),
            ("V3F", "v2f", "")
        ],
        default="STRING"
    )
    value: StringProperty(name="Value", description="Metadata value", default="")

classes = [MoonRayEXRHeaderAttribute]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)