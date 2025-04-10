import bpy
from bpy.props import *

from .props.sets import MoonRayLightSets, MoonRayLightFilterSets, MoonRayShadowSets, MoonRayShadowReceiverSets, MoonRayTraceSets
from .props.attributes import (
    MoonRayAttributes_Caching, 
    MoonRayAttributes_CameraAndLayer, 
    MoonRayAttributes_Checkpoint, 
    MoonRayAttributes_Debug, 
    MoonRayAttributes_DeepImages, 
    MoonRayAttributes_Driver, 
    MoonRayAttributes_Filtering, 
    MoonRayAttributes_FirefliesRemoval, 
    MoonRayAttributes_Frame,  
    MoonRayAttributes_GlobalToggles, 
    MoonRayAttributes_ImageSize, 
    MoonRayAttributes_Logging, 
    MoonRayAttributes_MetaData, 
    MoonRayAttributes_MotionAndScale, 
    MoonRayAttributes_PathGuide, 
    MoonRayAttributes_ResumeRender, 
    MoonRayAttributes_Sampling, 
    MoonRayAttributes_Volumes, 
    MoonRayAttributes_General
)

from .props.render_output import MoonRayAttributes_RenderOutput
from .props import MoonRayAttributes_Mfb

from .props.userdata import MoonRayUserData

class MoonRaySceneProperties(bpy.types.PropertyGroup):

    mfb: PointerProperty(type=MoonRayAttributes_Mfb)

    caching : PointerProperty(type=MoonRayAttributes_Caching)
    camera_and_layer: PointerProperty(type=MoonRayAttributes_CameraAndLayer)
    checkpoint: PointerProperty(type=MoonRayAttributes_Checkpoint)
    debug: PointerProperty(type=MoonRayAttributes_Debug)
    deep_images: PointerProperty(type=MoonRayAttributes_DeepImages)
    driver: PointerProperty(type=MoonRayAttributes_Driver)
    filtering: PointerProperty(type=MoonRayAttributes_Filtering)
    fireflies_removal: PointerProperty(type=MoonRayAttributes_FirefliesRemoval)
    frame: PointerProperty(type=MoonRayAttributes_Frame)
    global_toggles: PointerProperty(type=MoonRayAttributes_GlobalToggles)
    image_size: PointerProperty(type=MoonRayAttributes_ImageSize)
    logging: PointerProperty(type=MoonRayAttributes_Logging)
    metadata: PointerProperty(type=MoonRayAttributes_MetaData)
    motion_and_scale: PointerProperty(type=MoonRayAttributes_MotionAndScale)
    path_guide: PointerProperty(type=MoonRayAttributes_PathGuide)
    resume_render: PointerProperty(type=MoonRayAttributes_ResumeRender)
    sampling: PointerProperty(type=MoonRayAttributes_Sampling)
    volumes: PointerProperty(type=MoonRayAttributes_Volumes)
    general: PointerProperty(type=MoonRayAttributes_General)


    output:PointerProperty(type=MoonRayAttributes_RenderOutput)

    light_sets: PointerProperty(type=MoonRayLightSets)
    lightfilter_sets: PointerProperty(type=MoonRayLightFilterSets)
    shadow_sets: PointerProperty(type=MoonRayShadowSets)
    shadowreceiver_sets: PointerProperty(type=MoonRayShadowReceiverSets)
    trace_sets: PointerProperty(type=MoonRayTraceSets)

class MoonRayLightProperties(bpy.types.PropertyGroup):
    visible : BoolProperty(name="Visible")
    motion_blur : BoolProperty(name="Motion Blur")
    texture : StringProperty(name="Texture", subtype="FILE_PATH")

    type : EnumProperty(
        name="Light Type",
        description="Choose a light type",
        items=[
            ('CYLINDER', 'Cylinder', ''),
            ('DISK', 'Disk', ''),
            ('DISTANT', 'Distant', ''),
            ('ENV', 'Environment', ''),
            ('RECT', 'Rect', ''),
            ('SPHERE', 'Sphere', ''),
            ('SPOT', 'Spot', '')
        ]
    )

    lightfilter_set: StringProperty(name="Light Filter Set")
    light_set_input: StringProperty(name="Light Sets")
    shadow_set_input: StringProperty(name="Shadow Sets")

class MoonRayObjectProperties(bpy.types.PropertyGroup):
    is_light : BoolProperty(name="Is Light Source")

    light_set: StringProperty(name="Light Set")
    shadow_set: StringProperty(name="Shadow Set")
    shadowreceiver_set: StringProperty(name="Shadow Receiver Set")
    trace_set_input: StringProperty(name="Trace Set")
    shadowreceiver_set_input: StringProperty(name="Shadow Receiver Set")

    user_data : CollectionProperty(type=MoonRayUserData)

classes = [MoonRaySceneProperties, MoonRayLightProperties, MoonRayObjectProperties]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.Scene.moonray = PointerProperty(type=MoonRaySceneProperties)
    bpy.types.Light.moonray = PointerProperty(type=MoonRayLightProperties)
    bpy.types.Object.moonray = PointerProperty(type=MoonRayObjectProperties)

def unregister():
    del bpy.types.Scene.moonray
    del bpy.types.Object.moonray
    del bpy.types.Light.moonray

    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)