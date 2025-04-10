import bpy
from bpy.props import *

from .deep import MoonRayDeepIDAttribute
from .meta import MoonRayEXRHeaderAttribute

#https://docs.openmoonray.org/user-reference/scene-objects/scene-variables/SceneVariables/

class MoonRayAttributes_Caching(bpy.types.PropertyGroup):
    fast_geometry_update : BoolProperty(name="Fast Geometry Update", description="If this flag is off, the tessellation related data for subdivision surface will be deleted after tessellation is done. This is to save memory for single frame rendering. Otherwise, that data will be kept in memory to support re-tessellation after geometry are updated", default=False)
    
    texture_cache_size : IntProperty(name="Texture Cache Size", description="Specifies the maximum size of the texture cache in megabytes. This value can significantly impact rendering speed, where larger values often improve rendering speed", default=4000)
    texture_file_handles : IntProperty(name="Texture fIle Handles", description="Specifies the maximum number of simultaneous open texture file handles", default=24000)


class MoonRayAttributes_CameraAndLayer(bpy.types.PropertyGroup):
    # We can access this in Blender a different way
    camera : PointerProperty(
        name="Camera",
        description="This specifies the camera object used for rendering. If no camera is specified in the scene variables, MoonRay will render using the first camera object encountered",
        type=bpy.types.Object,
        poll=lambda self, obj: obj.type == 'CAMERA'
    )

    dicing_camera : PointerProperty(
        name="Dicing Camera",
        description="This attribute specifies a camera to use for adaptive geometry tessellation. The rendering camera is used if no camera is specified",
        type=bpy.types.Object,
        poll=lambda self, obj: obj.type == 'CAMERA'
    )

    #this could be set to the main collection. Or we just ignore it.
    layer : PointerProperty(
        name="Layer",
        description="This specifies the layer object used for rendering. If no layer is specified in the scene variables, MoonRay will rendering using the first layer object encountered",
        type=bpy.types.Collection
    )
    

class MoonRayAttributes_Checkpoint(bpy.types.PropertyGroup):
    checkpoint_active : BoolProperty(name="Checkpoint Active", description="Enables or disables checkpoint file writing", default=False)
    checkpoint_bg_write : BoolProperty(name="Checkpoint Background Write", description="When set to true, checkpoint file writes occur in a background thread that runs concurrently with the MCRT threads. Otherwise, all MCRT threads must wait while the checkpoint file is written", default=True)
    checkpoint_interval : FloatProperty(name="Checkpoint Interval", description="Specifies the time interval, in minutes, between checkpoint file writes. The interval must be equal to or greater than 0.1 minutes", default=15.0)
    checkpoint_max_bgcache : IntProperty(name="Checkpoint Max Background Cache", description="Specifies the maximum number of queued checkpoint images the checkpoint-writing background thread can handle. The value of checkpoint_max_bgcache must be greater than or equal to 1. If the number of queued checkpoint images exceeds this limit, MCRT threads will be temporarily suspended while background images are written to make room in the queue. A larger value can support background writing even with short checkpoint intervals, but it may require more memory. A value of 2 is recommended for most cases", default=2)

    checkpoint_max_snapshot_overhead: FloatProperty(name="Checkpoint Max Snapshot Overhead", description="Specifies the maximum fraction of the snapshot overhead threshold for an extra snapshot action in the event of an unexpected interruption by SIGINT. The value is expressed as a fraction. If the value is set to zero or a negative number, no extra snapshot action will be executed, and no checkpoint file will be generated if SIGINT is received", default=2.0)
    checkpoint_mode : EnumProperty(
        name = "Checkpoint Mode",
        description = "Allows you to choose whether checkpoint images are written based on time elapsed or on quality reached",
        items = [
            ("0", "Time", "Images are written based on time elapsed"),
            ("1", "Quality", "Images are written based on quality reached")
        ],
        default = "0"
    )

    checkpoint_overwrite : BoolProperty(name="Checkpoint Overwrite", description="When set to true, the last checkpoint file will be overwritten when writing out the new checkpoint file. If set to false, the checkpoint filename will be appended with the total number of samples, which will result in the retention of all checkpoint files", default=True)
    checkpoint_post_script : StringProperty(name="Checkpoint Post Script", description="Specifies the filename of a Lua script that will be executed after every checkpoint file is written. The script will run concurrently with the ongoing MCRT threads. For more information, refer to the documentation for MoonRay-provided Lua variables accessible within the script", subtype="FILE_PATH", default="")
    checkpoint_quality_steps : IntProperty(name="Checkpoint Quality Steps", description="Specifies the number of quality steps, which refers to the internal sampling iteration count between checkpoint file writes. The value must be equal to or greater than 1. In the case of uniform sampling, this number of steps is equivalent to the pixel sampling steps for each pixel. For example, if you set quality steps to 2, a checkpoint file will be created every time each pixel's sample count exceeds 2, 4, 6, 8, 10, and so on. In the case of adaptive sampling, this number of steps is equivalent to the internal adaptive sampling iteration steps. A recommended number falls within the range of 1 to 3. For example, if you set the value to 2, a checkpoint file will be created after finishing every 2 adaptive sampling passes. A larger value will conduct more rendering passes before writing a file", default=2)
    checkpoint_sample_cap : IntProperty(name="Checkpoint Sample Cap", description="Causes the render to finish based on the total pixel sample count. For example, if the value is 1024, the render will end after the next checkpoint write when each pixel exceeds 1024 samples. If the value is set to 0, the sample cap feature is disabled", default=0)
    checkpoint_snapshot_interval : FloatProperty(name="Checkpoint Snapshot Interval", description="Specifies the time interval, in minutes, allowed for a snapshot when a SIGINT is encountered. If the value is 0 or negative, the checkpoint_max_snapshot_overhead parameter is used instead", default=0.0)
    checkpoint_start_sample : IntProperty(name="Checkpoint Start Sample", description="Specifies the samples per pixel (SPP). A checkpoint file is created when all pixels' SPP are greater than or equal to this number. A checkpoint file is created once this criterion is met", default=1)
    checkpoint_time_cap : FloatProperty(name="Checkpoint Time Cap", description="Determines when the render will finish based on the total render process time in minutes. If the value is exceeded, the render will finish after the next checkpoint write. If the value is set to 0, the time cap feature is disabled", default=0.0)
    checkpoint_total_files : IntProperty(name="Checkpoint Total Files", description="This variable specifies the total number of checkpoint files for the quality-based checkpoint mode. It serves as a substitute parameter for checkpoint_quality_steps. If the value is set to 0 (the default), the interval at which checkpoints are generated is controlled by the checkpoint_quality_steps variable. If the value is set to 1 or higher, the renderer will attempt to automatically generate a user-defined number of checkpoint files based on this value. This option takes into account the checkpoint_start_sample variable.\n\nIn some cases, the renderer may be unable to create the requested number of checkpoint_total_files due to limitations in the internal implementation or because the user has specified a value greater than 1 for the checkpoint_start_sample variable. However, in these cases, the renderer will attempt to generate the closest possible number of checkpoint files to the user-defined value", default=0)

class MoonRayAttributes_Debug(bpy.types.PropertyGroup):
    debug_console : IntProperty(name="Debug Console", description="Specifies the port number for the debug console. When the debug console functionalities are enabled, you can use a telnet connection to send commands and control rendering behavior for debugging purposes.\n- A value of -1 disables all debug console functionality.\n- A positive value specifies a specific port number.\n- If you set the port number to 0, the kernel will find an available port for you and display the port number to stderr", default=-1)
    debug_pixel :  IntVectorProperty(name="Debug Pixel", description="Allows for rendering a single pixel and is typically used for debugging. The value given specifies the 2D pixel coordinate expressed from the bottom-left of the frame-viewport", size=2, default=(0, 0))
    
    validate_geometry : BoolProperty(name="Validate Geometry", description="Checks geometry for bad data", default=False)

class MoonRayAttributes_DeepImages(bpy.types.PropertyGroup):
    deep_curvature_tolerance : FloatProperty(name="Deep Curvature Tolerance", description="Maximum curvature (in degrees) of the deep surface within a pixel before it is split", default=45.0)
    deep_format = EnumProperty(
        name="Deep Format",
        description="Deep image format",
        items=[
            ("0", "OpenEXR 2.0", "vanilla OpenEXR deep"),
            ("1", "OpenDCX 2.0", "DCX abuffer mask encoding")
        ],
        default="1"
    )

    deep_id_attribute_names: CollectionProperty(name="Deep ID Attribute Names", description="Names of primitive attributes containing deep IDs", type=MoonRayDeepIDAttribute)
    deep_vol_compression_res: IntProperty(name="Deep Volume Comrpession Resolution", description="Volume opacity compression resolution. Lower values gives higher compression", default=10)
    deep_z_tolerance: FloatProperty(name="Deep Z Tolerance", description="Maximum range of the deep surface's Z values within a pixel before it is split", default=2.0)


class MoonRayAttributes_Driver(bpy.types.PropertyGroup):
    machine_id : IntProperty(name="Machine ID", description="Used only in arras moonray context, automatically set by arras and indicates the MCRT computation ID in the current session", default=-1)
    num_machines : IntProperty(name="Number of Machines", description="Used only in arras moonray context, automatically set by arras and indicates total number of MCRT computations active in the current session", default=-1)

    output_file : StringProperty(name="Output File", description="This specifies the output path for the beauty image (RGBA). This is independent of the AOV RenderOutputs, which can also write a beauty image", subtype="FILE_PATH", default="scene.exr")

    task_distribution_type : EnumProperty(
        name="Task Distribution Type",
        description="Used only in arras moonray context, defines the task distribution method to the MCRT computation. Multi-plex pixel is the default and preferred method. Non-overlapped tile is experimental and only used for debugging/development purposes",
        items = [
            ("0", "Non-Overlapped Tile", "Experimental. Only use for debugging/development purposes"),
            ("1", "Multiplex Pixel", "Preferred method")
        ],
        default="1"
    )

    tmp_dir : StringProperty(name="Temporary Folder", description="Define temporary directory name for temporary file generation. Use $TMPDIR environment variable value if this variable is empty.If $TMPDIR is also empty, use /tmp", subtype="FILE_PATH", default="/tmp")

class MoonRayAttributes_Filtering(bpy.types.PropertyGroup):
    pixel_filter: EnumProperty(
        name="Pixel Filter",
        description="The type of filter used for filter importance sampling. A box filter with a width of 1 is analogous to disabling pixel filtering",
        items = [
            ("0", "Box", ""),
            ("1", "Cubic B-Spline", ""),
            ("2", "Quadratic B-Spline", "")
        ],
        default="1"
    )

    pixel_filter_width : FloatProperty(name="Pixel Filter Width", description="The overall extents, in pixels, of the pixel filter. Larger values will result in softer images", default=3.0)
    texture_blur : FloatProperty(name="Texture Blur", description="Adjusts the amount of texture filtering", default=0.0)

class MoonRayAttributes_FirefliesRemoval(bpy.types.PropertyGroup):
    roughness_clamping_factor : FloatProperty(name="Roughness Clamping Factor", description="Clamp material roughness along paths. A value of 1 clamps values to the maximum roughness encountered, while lower values temper the clamping value. 0 disables the effect. Using this technique reduces fireflies from indirect caustics but is biased", default=0.0)
    sample_clamping_depth : IntProperty(name="Sample Clamping Depth", description="Clamp sample values only after the given non-specular ray depth", default=1)
    sample_clamping_value : FloatProperty(name="Sample Clamping Value", description="Clamp sample radiance values to this maximum value (the feature is disabled if the value is 0.0). Using this technique reduces fireflies, but is biased", default=10.0)

class MoonRayAttributes_Frame(bpy.types.PropertyGroup):
    # Can obtain these values through blender, but kept the attributes just in case
    frame: FloatProperty(name="Frame", description="Used to provide unique samples per frame, and for selecting the frame for scenes with animated data", default=0.0)
    max_frame : FloatProperty(name="Max Frame", description="Used to provide unique samples per frame", default=0.0)
    min_frame : FloatProperty(name="Min Frame", description="Used to provide unique samples per frame", default=0.0)

class MoonRayAttributes_GlobalToggles(bpy.types.PropertyGroup):
    cryptomatte_multi_presence : BoolProperty(name="Enable Cryptomatte Multi Presence", description="Determines whether to record presence bounces as separate cryptomatte samples", default=False)

    enable_displacement : BoolProperty(name="Enable Displacement", description="Enables or disables geometry displacement", default=True)
    enable_dof : BoolProperty(name="Enable Depth Of Field (DOF)", description="Enables or disables camera depth-of-field (DOF)", default = True)
    enable_max_geometry_resolution : BoolProperty(name="Enable Max Geometry Resolution", description="Specifies whether the max_geometry_resolution limit is in effect", default=False)
    enable_motion_blur : BoolProperty(name="Enable Motion Blur", description="Enables or disables motion blur", default=True)
    enable_presence_shadows : BoolProperty(name="Enable Presence Shadows", description="Whether or not to respect a material's 'presence' value for shadow rays. Performance may improve when disabled, but all materials are treated as fully present", default=False)
    enable_shadowing : BoolProperty(name="Enable Shadowing", description="Enables or disables shadowing through occlusion rays", default=True)
    enable_subsurface_scattering : BoolProperty(name="Enable Subsurface Scattering", description="Enables or disables sub-surface scattering", default=True)

    lights_visible_in_camera: BoolProperty(name="Show Lights in Camera", description="Globally enables or disables lights being visible in camera. Each light has its own setting which may override this value", default=False)
    max_geometry_resolution: IntProperty(name="Max Geometry Resolution", description="Specifies a global limit to geometry resolution. Geometry procedurals should respect this limit", default=2147483647)
    propagate_visibility_bounce_type: BoolProperty(name="Propagate Visibility Bounce Type", description="Turns on/off propagation for ray visibility masks", default=False)

    shadow_terminator_fix : EnumProperty(
        name = "Shadow Terminator Fix",
        description="Attempt to soften hard shadow terminator boundaries due to shading/geometric normal deviations. 'ON uses a custom terminator softening method. Cosine Compensation' is Chiang's 2019 SIGGRAPH technique. 'GGX' is Estevez's raytracing gems technique. 'Sine Compensation' is a sine based modification of Chiang's method. Different scenes may work better with different techniques. The recommendation is to start with the custom compensation ON, then sine compensation technique, then GGX, then cosine",
        items=[
            ("0", "Off", ""),
            ("1", "On", ""),
            ("2", "On (Sine Compoensation Alternative)", ""),
            ("3", "On (GGX Compensation Alternative)", ""),
            ("4", "On (Cosine Compensation Alternative)", ""),
        ],
        default="0"
    )

class MoonRayAttributes_ImageSize(bpy.types.PropertyGroup):
    aperture_window : IntVectorProperty(name="Aperture Window", description="The window of the camera aperture. Overrides image_width and image_height. Ordered as xmin, ymin, xmax, and ymax, with origin at the bottom-left", size=4, default=(0, 0, 0, 0))

    # Can be obtained through blender
    image_height: IntProperty(name="Image Height", description="The desired height of the output image(s), in pixels", default=1080)
    image_width: IntProperty(name="Image Width", description="The desired width of the output image(s), in pixels", default=1920)

    region_window : IntVectorProperty(name="Region Window", description="Window that is rendered. Overrides image width / height (and overrides aperture window override). Order: xmin ymin xmax ymax, with origin at left bottom.", size=4, default=(0, 0, 0, 0))
    res : FloatProperty(name="Resolution", description="Final divisor for the overall image dimensions. A quick way to reduce or increase the size of the render. A value of 2 halves the size of the rendered image(s). A value of 0.5 doubles it", default=1.0)
    sub_viewport : IntVectorProperty(name="Sub Viewport", description="Subviewport of region window. Coordinate (0,0) maps to left, bottom of region window", size=2, default=(0, 0))

class MoonRayAttributes_Logging(bpy.types.PropertyGroup):
    athena_debug : BoolProperty(name="Use Athena Debug", description="[DreamWorks Animation internal] Enables or disables sending logging results to the Athena debugging database instead of the production database", default=False)
    fatal_color: FloatVectorProperty(name="Fatal Color", description="The color to use for materials or map shaders that are unable to execute shading, usually due to incomplete initialization", subtype="COLOR", default=(1, 0, 1))
    log_debug: BoolProperty(name="Log Debug", description="Determines whether debugging-level messages are logged", default=False)
    log_info: BoolProperty(name="Log Info", description="Determines whether information-level messages are logged", default=False)

    stats_file: StringProperty(name="Stats File", description="The filename to write the rendering statistics to in CSV format", subtype="FILE_PATH", default="")

class MoonRayAttributes_MetaData(bpy.types.PropertyGroup):
    exr_header_attributes : CollectionProperty(name="EXR Header Attributes", description="Metadata that is passed directly to the exr header. Format: {'name', 'type', 'value'}", type=MoonRayEXRHeaderAttribute)

class MoonRayAttributes_MotionAndScale(bpy.types.PropertyGroup):
    motion_steps: FloatVectorProperty(name="Motion Steps", description="Frame-relative time offsets for motion sampling", size=2, default=(0, 0))

    # Can use the blender scene scale
    scene_scale : FloatProperty(name="Scene Scale", description="(in meters): one unit in world space = 'scene scale' meters", default=0.01)

class MoonRayAttributes_PathGuide(bpy.types.PropertyGroup):
    path_guide_enable : BoolProperty(name="Path Guiding", description="Turn on path guiding to handle difficult light transport problems (e.g. caustics) at the cost of increased memory", default=False)

class MoonRayAttributes_ResumeRender(bpy.types.PropertyGroup):
    on_resume_script : StringProperty(name="On Resume Script", description="When using resumable rendering, the Lua script named here is executed after the render prep stage. In addition, MoonRay sets some Lua global variables the script can access. This functionality is disabled when the script name is empty or when not using resumable rendering. Please refer to the checkpoint/resume documentation for more details", subtype="FILE_PATH", default="")
    resumable_output : BoolProperty(name="Resumable Output", description="Make aov output as resumable for resume render", default=False)
    resume_render : BoolProperty(name="Resume Render", description="Resuming render process", default=False)

class MoonRayAttributes_Sampling(bpy.types.PropertyGroup):
    bsdf_samples : IntProperty(name="BSDF Samples", description="The square root of the number of samples taken for BSDF lobe evaluations on the primary intersection. The number of samples taken per material depends on the BSDF sampler strategy and the number of lobes that comprise the material", default=2)
    bssrdf_samples : IntProperty(name="BSSRDF Samples", description="The square root of the number of samples taken to evaluate BSSRDF (subsurface scattering) contributions on the primary intersection", default=2)
    disable_optimized_hair_sampling : BoolProperty(name="Disable Optimized Hair Sampling", description="Forces all hair materials to sample each hair BSDF lobe independently. This will enable the LPE label syntax for 'hair R', 'hair TT', 'hair TRT' and 'hair TRRT ' but will result in slower rendering", default=False)
    light_samples : IntProperty(name="Light Samples", description="The square root of the number of samples taken for each light on the primary intersection", default=2)
    lock_frame_noise: BoolProperty(name="Lock Frame Noise", description="By default, the random number generators are seeded by considering the frame number. However, if lock_frame_noise is true, the same seed values are used for each frame, which is typically undesirable", default=False)
    max_depth: IntProperty(name="Max Depth", description="The maximum ray depth (number of 'bounces') for diffuse|glossy|mirror event types. This can be thought of as the global depth limit. Reducing this can improve performance at the cost of biasing the rendered image", default=5)
    max_diffuse_depth: IntProperty(name="Max Diffuse Depth", description="The maximum ray depth (number of 'bounces') for diffuse event types. Reducing this can improve performance at the cost of biasing the rendered image. Note that this limit is also governed by the global 'max depth' attribute", default=2)
    max_glossy_depth: IntProperty(name="Max Glossy Depth", description="The maximum ray depth (number of 'bounces') for glossy event types. Reducing this can improve performance at the cost of biasing the rendered image. Note that this limit is also governed by the global 'max depth' attribute.", default=2)
    max_hair_depth: IntProperty(name="Max Hair Depth", description="The maximum ray depth (number of 'bounces') for hair material types. This limit may need to be increased to allow for more hair-to-hair interactions, especially for blonde/white hair or fur. Reducing this can improve performance at the cost of biasing the rendered image.", default=5)
    max_mirror_depth: IntProperty(name="Max Mirror Depth", description="The maximum ray depth (number of 'bounces') for mirror event types. Reducing this can improve performance at the cost of biasing the rendered image. Note that this limit is also governed by the global 'max depth' attribute", default=3)
    max_presence_depth: IntProperty(name="Max Presence Depth", description="The maximum ray depth (number of 'bounces') for presence event types. The material's 'presence' attribute is ignored after this depth has been reached and the surface is treated as fully present. Reducing this can improve performance at the cost of biasing the rendered image.", default=16)
    max_subsurface_per_path: IntProperty(name="Max Subsurface Depth", description="The maximum ray depth (number of 'bounces') to allow subsurface scattering. For ray depths beyond this limit Lambertian diffuse is used to approximate subsurface scattering", default=1)
    pixel_samples: IntProperty(name="Pixel Samples", description="The square root of the number of primary samples taken for each pixel in uniform sampling mode. For example, a value of 4 will result in 4*4 = 16 uniform pixel samples", default=8)
    presence_threshold: FloatProperty(name="Presence Threshold", description="The presence threshold defines the point at which the accumulated presence can be considered opaque, skipping the generation of presence continuation rays", default=0.999)
    russian_roulette_threshold: FloatProperty(name="Russian Roulette Threshold", description="The Russian roulette threshold specifies the point at which point Russian roulette is evaluated for direct light sampling and BSDF continuation. The unit is luminance of the radiance", default=0.0375)
    transparency_threshold: FloatProperty(name="Transparency Threshold", description="The transparency threshold defines the point at which the accumulated opacity can be considered opaque, skipping the generation of new transparency rays", default=1.0)

class MoonRayAttributes_Volumes(bpy.types.PropertyGroup):
    max_volume_depth: IntProperty(name="Max Volume Depth", description="The maximum ray depth (number of 'bounces') for volume event types. Volumes are ignored after this depth has been reached. Reducing this can improve performance at the cost of biasing the rendered image", default=1)
    volume_attenuation_factor: FloatProperty(name="Volume Attenuation Factor", description="Controls how volume attenuation gets exponentially scaled down when rendering multiple scattering volumes. Dialing down the value generally results in more translucent look. This variable is only effective when 'max volume depth' is greater than 1", default=0.65)
    volume_contribution_factor: FloatProperty(name="Volume Contribution Factor", description="Controls how scattering contribution gets exponentially scaled down when rendering multiple scattering volumes. Dialing down the value generally results in a darker volume scattering look. This variable is only effective when 'max volume depth' is greater than 1", default=0.65)
    volume_illumination_samples: IntProperty(name="Volume Illumination Samples", description="Sample number along the ray when computing volume scattering radiance towards the eye. Set to 0 to turn off volume lighting completely", default=4)
    volume_opacity_threshold: FloatProperty(name="Volume Opacity Threshold", description="As a ray travels through volumes, it will accumulate opacity. When the value exceeds the volume opacity threshold, the renderer will stop further volume integration along this ray", default=0.995)
    volume_overlap_mode : EnumProperty(
        name="Volume Overlap Mode",
        description="Selects how to handle contributions from overlapping volumes",
        items=[
            ("0", "Sum", "Add contributions from all volumes"),
            ("1", "Max", "Only consider maximum volume based on extinction"),
            ("2", "RND", "Randomly choose one value weighted by extinction")
        ],
        default="0"
    )

    volume_phase_attenuation_factor: FloatProperty(name="Volume Phase Attenuation Factor", description="Controls how phase function (anisotropy) gets exponentially scaled down when rendering multiple scattering volumes. This variable is only effective when 'max volume depth' is greater than 1", default=0.5)
    volume_quality : FloatProperty(name="Volume Quality", description="Controls the overall quality of volume rendering. The higher number gives better volume shape detail and more accurate scattering integration result", default=0.5)
    volume_shadow_quality: FloatProperty(name="Volume Shadow Quality", description="Controls the quality of volume shadow (transmittance). The higher number gives more accurate volume shadow", default=1.0)

class MoonRayAttributes_General(bpy.types.PropertyGroup):
    batch_tile_order : EnumProperty(
        name="Batch Tile Order",
        description="Specifies the order in which tiles (as areas of 8x8 pixels) are prioritized for batch rendering, which determines which areas of the image are rendered first. The ordering is not guaranteed: the strict sequence of tile starting and completion for any pass is nondeterministic due to thread scheduling",
        items=[
            ("0", "Top", ""),
            ("1", "Bottom", ""),
            ("2", "Left", ""),
            ("3", "Right", ""),
            ("4", "Morton", ""),
            ("5", "Random", ""),
            ("6", "Spiral Square", ""),
            ("7", "Spiral Rect", ""),
            ("8", "Morton Shiftflip", "")
        ],
        default="4"
    )

    checkpoint_tile_order : EnumProperty(
        name="Checkpoint Tile Order",
        description="Specifies the order in which tiles (as areas of 8x8 pixels) are prioritized for checkpoint rendering, which determines which areas of the image are rendered first. The ordering is not guaranteed: the strict sequence of tile starting and completion for any pass is nondeterministic due to thread scheduling",
        items=[
            ("0", "Top", ""),
            ("1", "Bottom", ""),
            ("2", "Left", ""),
            ("3", "Right", ""),
            ("4", "Morton", ""),
            ("5", "Random", ""),
            ("6", "Spiral Square", ""),
            ("7", "Spiral Rect", ""),
            ("8", "Morton Shiftflip", "")
        ],
        default="4"
    )

    crypto_uv_attribute_name: StringProperty(name="Cryptomatte UV Attribute Name", description="Names of primitive attribute containing crypto UVs", default="")

    # can be obtained from blender
    fps: FloatProperty(name="FPS", description="(Frames per second) Affects motion blur", default=24.0)
    
    light_sampling_mode : EnumProperty(
        name="Light Sampling Mode",
        description="Controls which light sampling scheme to use: uniform or adaptive",
        items=[
            ("0", "Uniform", ""),
            ("1", "Adaptive", "")
        ],
        default="0"
    )

    light_samping_quality : FloatProperty(
        name="Light Sampling Quality", description="When the light sampling mode is 'adaptive', this attribute controls how many lights are sampled per light sample, where 0.0 is low quality (1 light sampled per light sample) and 1.0 is high quality (all lights sampled per light sample). Any value in between will cause adaptive light sampling to kick into effect, meaning that it will choose a higher or lower number of lights depending on what that particular point needs. A number closer to 0.0 will cause it to sample a lower number of lights on average, and vice versa", 
        default=0.5
    )

    max_adaptive_samples: IntProperty(
        name="Max Adaptive Samples", description="When adaptive sampling is turned on, this represents the max number of samples we can throw at a pixel. It's best to err on the high side since adaptive sampling will cull out samples where they're not needed based on the target adaptive error, in which case we should rarely hit the max samples value", 
        default=4096
    )

    min_adaptive_samples: IntProperty(
        name="Min Adaptive Samples", description="This is the minimum number of samples taken per pixel before enabling adaptive sampling. A larger number of samples may prevent the adaptive sampler from prematurely identifying an area as converged but may incur a longer running time", 
        default=16
    )

    progressive_tile_order: EnumProperty(
        name="Progressive Tile Order",
        description="Specifies the order in which tiles (as areas of 8x8 pixels) are prioritized for progressive rendering, which determines which areas of the image are rendered first. The ordering is not guaranteed: the strict sequence of tile starting and completion for any pass is nondeterministic due to thread scheduling",
        items=[
            ("0", "Top", ""),
            ("1", "Bottom", ""),
            ("2", "Left", ""),
            ("3", "Right", ""),
            ("4", "Morton", ""),
            ("5", "Random", ""),
            ("6", "Spiral Square", ""),
            ("7", "Spiral Rect", ""),
            ("8", "Morton Shiftflip", "")
        ],
        default="4"
    )

    sampling_mode: EnumProperty(
        name="Sampling Mode",
        description="Controls which sampling scheme to use: uniform or adaptive",
        items=[
            ("0", "Uniform", ""),
            ("2", "Adaptive", "")
        ],
        default="0"
    )

    target_adaptive_error: FloatProperty(
        name="Target Adaptive Error", description="When adaptive sampling is turned on, this represents the desired quality of the output images. Lower values will give higher quality but take longer to render. Higher values will give lower quality but render quicker", 
        default=10.0
    )

    two_stage_output: BoolProperty(name="Two Stage Output", description="Specifies whether to use a two-stage writing process for images. In two-stage writing, the image is first written to a temporary location and then moved to the final location. This approach significantly reduces the risk of output data corruption due to an unexpected render process termination.\nThe directory where the temporary files are stored is defined by the 'tmp_dir' scene variable.", default=True)

classes = [
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
]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)