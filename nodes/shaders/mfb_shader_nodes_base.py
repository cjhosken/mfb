import bpy
from ..mfb_nodes import MoonRayShaderNode

class MoonRayShaderNode_Base(MoonRayShaderNode):
    bl_idname = 'MoonRayShaderNode_Base'
    bl_label = 'MoonRay Node'

    def init(self, context):
        pass

    def update(self):
        pass


classes = [MoonRayShaderNode_Base]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)