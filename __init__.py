#  MoonRay for Blender is a render engine addon which allows users to utilise Dreamwork's MoonRay Production Renderer inside of Blender.
#  Copyright 2024 Christopher Hosken
# 
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.

#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.

import bpy
from bpy.types import HydraRenderEngine
from bpy.utils import register_class, unregister_class

bl_info = {
    "name" : "MoonRay For Blender",
    "author" : "Christopher Hosken",
    "version" : (0, 0, 1),
    "blender" : (4, 1, 0),
    "description" : "Dreamworks' MoonRay Production Renderer integration",
    "warning" : "This Addon is currently under development",
    "support": "COMMUNITY",
    "doc_url": "https://github.com/Christopher-Hosken/moonray_for_blender/wiki",
    "tracker_url": "https://github.com/Christopher-Hosken/moonray_for_blender/issues",
    "category" : "Render"
}

class MoonRayRender(HydraRenderEngine):
    bl_idname = "MOONRAY"
    bl_label = "MoonRay"
    bl_info = "Dreamworks' MoonRay Production Renderer integration"

    bl_use_preview = True
    bl_use_gpu_context = True
    bl_use_materialx = True

    bl_delegate_id = "HdMoonrayPlugin"

    def __init__(self):
        pass

    def __del__(self):
        pass

    def render(self, depsgraph):
        pass

    def view_update(self, context, depsgraph):
        pass

    def view_draw(self, context, depsgraph):
        pass

from .preferences import classes as preferences_classes
from . import ui
from . import nodes

classes = [MoonRayRender] + preferences_classes

def register():
    for cls in classes:
        register_class(cls)
    ui.register()
    nodes.register()

def unregister():
    for cls in classes:
        unregister_class(cls)
    ui.unregister()
    nodes.unregister()
