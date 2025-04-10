import bpy
from bpy.props import *


# Light Sets

class MoonRayLightSetItem(bpy.types.PropertyGroup):
    name: StringProperty(name="Name")

class MoonRayLightSets(bpy.types.PropertyGroup):
    items: CollectionProperty(type=MoonRayLightSetItem)
    index: IntProperty(name="Index", default=0)

# Light Filter Sets

class MoonRayLightFilterSetItem(bpy.types.PropertyGroup):
    name: StringProperty(name="Name")

class MoonRayLightFilterSets(bpy.types.PropertyGroup):
    items: CollectionProperty(type=MoonRayLightSetItem)
    index: IntProperty(name="Index", default=0)

# Shadow Sets

class MoonRayShadowSetItem(bpy.types.PropertyGroup):
    name: StringProperty(name="Name")

class MoonRayShadowSets(bpy.types.PropertyGroup):
    items: CollectionProperty(type=MoonRayShadowSetItem)
    index: IntProperty(name="Index", default=0)

# Shadow Receiver Sets

class MoonRayShadowReceiverSetItem(bpy.types.PropertyGroup):
    name: StringProperty(name="Name")

class MoonRayShadowReceiverSets(bpy.types.PropertyGroup):
    items: CollectionProperty(type=MoonRayShadowReceiverSetItem)
    index: IntProperty(name="Index", default=0)

# Trace Sets

class MoonRayTraceSetItem(bpy.types.PropertyGroup):
    name: StringProperty(name="Name")

class MoonRayTraceSets(bpy.types.PropertyGroup):
    items: CollectionProperty(type=MoonRayShadowReceiverSetItem)
    index: IntProperty(name="Index", default=0)

classes = [MoonRayLightSetItem, MoonRayLightSets, MoonRayLightFilterSetItem, MoonRayLightFilterSets, MoonRayShadowSetItem, MoonRayShadowSets, MoonRayShadowReceiverSetItem, MoonRayShadowReceiverSets, MoonRayTraceSetItem, MoonRayTraceSets]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)