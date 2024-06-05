from nodeitems_utils import register_node_categories, unregister_node_categories, NodeItem
from bpy.utils import register_class, unregister_class

from . import moonray_bl_nodes_sockets

from .moonray_bl_nodes_base import MoonRayShadingNode, MoonRayShaderNodeCategory, MoonRayWorldNodeCategory, MoonRayLightNodeCategory, MoonRayCompNodeCategory
from .moonray_bl_nodes_sockets import classes as socket_classes, register_sockets
from .nodes.light import classes as light_classes
from .nodes.shader import classes as shader_classes
from .nodes.world import classes as world_classes
from .nodes.comp import classes as comp_classes


__MOONRAY_NODES_ALREADY_REGISTERED__ = False

register_sockets()

classes = socket_classes + shader_classes + light_classes + world_classes + comp_classes

__MOONRAY_NODE_CATEGORIES__ = [
    MoonRayShaderNodeCategory("MOONRAYSHADERS", "MoonRay", items=[
        NodeItem("MoonRay_ShaderOutputNode"),
    ]),
    MoonRayWorldNodeCategory("MOONRAYWOLRDS", "MoonRay", items=[
        NodeItem("MoonRay_WorldOutputNode")
    ]),
    MoonRayLightNodeCategory("MOONRAYLIGHTS", "MoonRay", items=[
        NodeItem("MoonRay_LightOutputNode"),
        NodeItem("MoonRay_EnvLightNode")
    ]),
    MoonRayCompNodeCategory("MOONRAYCOMP", "MoonRay", items=[   
        NodeItem("MoonRay_CompToonDisplayFilterNode"),
    ]),
]

def register():
    global __MOONRAY_NODES_ALREADY_REGISTERED__

    for cls in classes:
        register_class(cls)

    if not __MOONRAY_NODES_ALREADY_REGISTERED__:
        try:
            register_node_categories('MOONRAYSHADERNODES', __MOONRAY_NODE_CATEGORIES__)  
        except:
            unregister_node_categories('MOONRAYSHADERNODES')
            register_node_categories('MOONRAYSHADERNODES', __MOONRAY_NODE_CATEGORIES__)  
            __MOONRAY_NODES_ALREADY_REGISTERED__ = True  


def unregister():
    for cls in classes:
        unregister_class(cls)
    if __MOONRAY_NODES_ALREADY_REGISTERED__:
        unregister_node_categories('MOONRAYSHADERNODES')
    