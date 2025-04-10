import bpy
from bpy.props import *
from bpy.types import *

class MoonRayUserData(bpy.types.PropertyGroup):
    name: StringProperty()
    type: EnumProperty(
        items=[
            ("BOOL", "Boolean", ""),
            ("COLOR", "Color", ""),
            ("FLOAT", "Float", ""),
            ("INTEGER", "Integer", ""),
            ("MAT4F", "Mat4f", ""),
            ("STRING", "String", ""),
            ("VEC2F", "Vec2f", ""),
            ("VEC3F", "Vec3f", ""),
        ],
        default="FLOAT"
    )
    value_bool: BoolProperty(default=False)
    value_color: FloatVectorProperty(subtype='COLOR', default=(1.0, 1.0, 1.0))
    value_float: FloatProperty(default=0.0)
    value_int: IntProperty(default=0)
    value_mat4f: FloatVectorProperty(size=16, default=[0.0]*16)
    value_string: StringProperty(default="")
    value_vec2f: FloatVectorProperty(size=2, default=(0.0, 0.0))
    value_vec3f: FloatVectorProperty(size=3, default=(0.0, 0.0, 0.0))

classes = [MoonRayUserData]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)