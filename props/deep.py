import bpy
from bpy.props import *

class MoonRayDeepIDAttribute(bpy.types.PropertyGroup):
    name: StringProperty(name="Deep ID Attribute Name")

classes = [MoonRayDeepIDAttribute]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)