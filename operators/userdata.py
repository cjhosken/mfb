import bpy
from bpy.props import *

from ..props.userdata import MoonRayUserData

class MOONRAY_OT_AddUserData(bpy.types.Operator):
    bl_idname = "moonray.add_userdata"
    bl_label = "Add User Data"
    bl_description = "Add a custom userdata attribute"

    attribute: PointerProperty(type=MoonRayUserData)
    
    def execute(self, context):
        user_data = context.object.moonray.user_data

        new_attr = user_data.add()
        new_attr.name = self.attribute.name
        new_attr.type = self.attribute.type

        # Set value based on type
        if self.attribute.type == 'BOOL':
            new_attr.value_bool = self.attribute.value_bool
        elif self.attribute.type == 'COLOR':
            new_attr.value_color = self.attribute.value_color
        elif self.attribute.type == 'FLOAT':
            new_attr.value_float = self.attribute.value_float
        elif self.attribute.type == 'INTEGER':
            new_attr.value_int = self.attribute.value_int
        elif self.attribute.type == 'MAT4F':
            new_attr.value_mat4f = self.attribute.value_mat4f
        elif self.attribute.type == 'STRING':
            new_attr.value_string = self.attribute.value_string
        elif self.attribute.type == 'VEC2F':
            new_attr.value_vec2f = self.attribute.value_vec2f
        elif self.attribute.type == 'VEC3F':
            new_attr.value_vec3f = self.attribute.value_vec3f

        return {'FINISHED'}

    def invoke(self, context, event):
        self.attribute.name = "default"
        wm = context.window_manager
        return wm.invoke_props_dialog(self)

    def draw(self, context):
        layout = self.layout
        layout.prop(self.attribute, "name")
        layout.prop(self.attribute, "type")
        
        # Additional properties for value types
        if self.attribute.type == 'BOOL':
            layout.prop(self.attribute, "value_bool")
        elif self.attribute.type == 'COLOR':
            layout.prop(self.attribute, "value_color")
        elif self.attribute.type == 'FLOAT':
            layout.prop(self.attribute, "value_float")
        elif self.attribute.type == 'INTEGER':
            layout.prop(self.attribute, "value_int")
        elif self.attribute.type == 'MAT4F':
            layout.prop(self.attribute, "value_mat4f")
        elif self.attribute.type == 'STRING':
            layout.prop(self.attribute, "value_string")
        elif self.attribute.type == 'VEC2F':
            layout.prop(self.attribute, "value_vec2f")
        elif self.attribute.type == 'VEC3F':
            layout.prop(self.attribute, "value_vec3f")


class MOONRAY_OT_RemoveUserData(bpy.types.Operator):
    bl_idname = "moonray.remove_userdata"
    bl_label = "Remove Custom Attribute"
    bl_description = "Remove a custom attribute"

    index: IntProperty()

    def execute(self, context):
        obj = context.object
        user_data = obj.moonray.user_data
        user_data.remove(self.index)
        return {'FINISHED'}

classes = [MOONRAY_OT_AddUserData, MOONRAY_OT_RemoveUserData]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
