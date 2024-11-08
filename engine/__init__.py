import bpy, os

class MoonRayRenderEngine(bpy.types.HydraRenderEngine):
    bl_idname = "MOONRAY"
    bl_label = "MoonRay"
    bl_info = "Dreamworks' MoonRay Production Renderer integration"

    bl_use_preview = True
    bl_use_gpu_context = True
    bl_use_materialx = True

    bl_delegate_id = "HdMoonrayRendererPlugin"

    @classmethod
    def register(cls):
        import pxr.Plug
        rel = os.path.join(os.path.expanduser("~"), ".mfb","installs","openmoonray")
        
        os.environ["REL"] = rel
        
        # Set other environment variables using the expanded `REL` path
        os.environ["RDL2_DSO_PATH"] = os.path.join(rel, "rdl2dso.proxy") + ":" + os.path.join(rel, "rdl2dso")
        os.environ['ARRAS_SESSION_PATH'] = os.path.join(rel, "sessions")
        os.environ["MOONRAY_CLASS_PATH"] = os.path.join(rel, "shader_json")

        # Update PATH and PXR_PLUGINPATH_NAME by expanding the current environment variables
        os.environ["PATH"] = os.path.join(rel, "bin") + ":" + os.environ.get("PATH", "")
        
        os.environ["PXR_PLUGINPATH_NAME"] = os.path.join(rel, "plugin", "usd") + ":" + os.environ.get("PXR_PLUGINPATH_NAME", "")
        
        #os.environ["HDMOONRAY_DEBUG_MODE"] = "1"
        #os.environ["HDMOONRAY_DEBUG"] = "1"
        #os.environ["HDMOONRAY_INFO"] = "1"
        #os.environ["HDMOONRAY_DISABLE"]="0"
        #os.environ["HDMOONRAY_RDLA_OUTPUT"]="temp"

        pxr.Plug.Registry().RegisterPlugins([os.path.join(rel, "plugin", "pxr")])


    def get_render_settings(self, engine_type):
        moonray = bpy.context.scene.moonray

        result = {
            'volumeRaymarchingStepSize': 2.0,
            'volumeRaymarchingStepSizeLighting': 3.0,

            "houdini:interactive": False,


            "sceneVariable:min_frame": 0.0,
            "sceneVariable:max_frame": 0.0,

            "sceneVariable:frame": 0.0,

            "sceneVariable:dicing_camera": None,
            "sceneVariable_dicing_camera": None,

            "sceneVariable:exr_header_attributes": None, # {"name", "type". "value"}
            "sceneVariable_exr_header_attributes": None, # {"name", "type". "value"}
            "sceneVariable:res": 1.0,

            "sceneVariable:aperture_window": {0, 0, 1280, 720},
            "sceneVariable_aperture_window": {0, 0, 1280, 720},

            "sceneVariable:region_window": {},
            "sceneVariable_region_window": {},

            "sceneVariable:sub_viewport": {0, 0},
            "sceneVariable_sub_viewport": {0, 0},

            "sceneVariable:fps": 24.0,
            "sceneVariable:scene_scale": 0.01,
            "sceneVariable:sampling_mode": 0, # (0:uniform (default), 2:adaptive)
            "sceneVariable:min_adaptive_samples": 16,
            "sceneVariable:max_adaptive_samples": 4096,

            "sceneVariable:target_adaptive_error": 10.0,

            "sceneVariable:light_sampling_mode": 0, # (0:uniform (default), 1:adaptive)
            "sceneVariable:light_sampling_quality": 0.5,

            "sceneVariable:russian_roulette_threshold": 0.0375,


            "sceneVariable:pixel_samples": 8,
            "sceneVariable:light_samples": 2,
            "sceneVariable:bsdf_samples": 2,
            "sceneVariable:bssrdf_samples": 2,

            "sceneVariable:max_depth": 5,
            "sceneVariable:max_diffuse_depth": 2,
            "sceneVariable:max_glossy_depth": 2,
            "sceneVariable:max_mirror_depth": 3,
            "sceneVariable:max_volume_depth": 1,
            "sceneVariable:max_presence_depth": 16,
            "sceneVariable:max_hair_depth": 5,

            "sceneVariable:disable_optimized_hair_sampling": False,
            "sceneVariable:max_subsurface_per_path": 1,
            "sceneVariable:transparency_threshold": 1.0,
            "sceneVariable:presence_threshold": 0.999,
            "sceneVariable:lock_frame_noise": False,
            "sceneVariable:volume_quality": 0.5,
            "sceneVariable:volume_shadow_quality": 1.0,

            "sceneVariable:volume_illumination_samples": 4,

            "sceneVariable:volume_opacity_threshold": 0.995,
            "sceneVariable:volume_overlap_mode": 0, # (0:sum (default), 1: max, 2: rnd)
            "sceneVariable:volume_attenuation_factor": 0.65,

            "sceneVariable:volume_contribution_factor": 0.65,
            "sceneVariable:volume_phase_attenuation_factor": 0.5,

            "sceneVariable:path_guide_enable": False,

            "sceneVariable:sample_clamping_value": 10.0,
            "sceneVariable:sample_clamping_depth": 1,

            "sceneVariable:roughness_clamping_factor": 0.0,
            "sceneVariable:texture_blur": 0.0,

            "sceneVariable:pixel_filter_width": 3.0,
            "sceneVariable:pixel_filter": 1, # (0: box, 1: cubic b-spline (default), 2: quadratic b-spline)

            "sceneVariable:deep_format": 1, # (0: openexr2.0, 1: opencx2.0 (default))
            "sceneVariable:deep_curvature_tolerance": 45.0,

            "sceneVariable:deep_z_tolerance": 2.0,

            "sceneVariable:deep_vol_compression_res": 10,

            "sceneVariable:deep_id_attribute_names": {},
            "sceneVariable_deep_id_attribute_names": {},

            "sceneVariable:texture_cache_size": 4000,

            "sceneVariable:crypto_uv_attribute_name": "",
            "sceneVariable:texture_file_handles": 24000,

            "sceneVariable:fast_geometry_update": False,
            
            "sceneVariable:checkpoint_active": False,
            "sceneVariable:checkpoint_interval": 15.0,
            "sceneVariable:checkpoint_quality_steps": 2,
            "sceneVariable:checkpoint_time_cap": 0.0,
            "sceneVariable:checkpoint_sample_cap": 0,
            "sceneVariable:checkpoint_overwrite": True,         
            "sceneVariable:checkpoint_mode": 0, # (0: time (default), 1: quality)
            "sceneVariable:checkpoint_start_sample": 1,
            "sceneVariable:checkpoint_bg_write": True,
            "sceneVariable:checkpoint_post_script": "",
            "sceneVariable:checkpoint_total_files": 0,
            "sceneVariable:checkpoint_max_bgcache": 2,
            "sceneVariable:checkpoint_max_snapshot_overhead": 0.0,

            "sceneVariable:checkpoint_snapshot_interval": 0.0,
            "sceneVariable:resumable_output": False,
            "sceneVariable:resume_render": False,
            "sceneVariable:on_resume_script": "",
            "sceneVariable:enable_dof": True,
            "sceneVariable:enable_max_geometry_resolution": False,
            "sceneVariable:max_geometry_resolution": 2147483647,
            "sceneVariable:enable_displacement": True,
            "sceneVariable:enable_subsurface_scattering": True,
            "sceneVariable:enable_shadowing": True,
            "sceneVariable:enable_presence_shadows": False,
            "sceneVariable:lights_visible_in_camera": 1,
            "sceneVariable:propagate_visibility_bounce_type": False,
            "sceneVariable:shadow_terminator_fix": 0, # (0: off (default), 1: on, 2: on (sine compensation alternative), 3: on (GGX compensation alternative), 4: on (Cosine compensation alternative))
            "sceneVariable:machine_id": -1,
            "sceneVariable:num_machines": -1,
            "sceneVariable:task_distribution_type": 1, # (0: non-overlapped tile, 1: multiplex pixel (default))
            "sceneVariable:batch_tile_order": 4, # (0:top,1:bottom,2:left,3:right,4:morton(default),5:random,6:spiralsquare,7:spiralrect,8:moronshiftflip)
            "sceneVariable:progressive_tile_order": 4, # (0:top,1:bottom,2:left,3:right,4:morton(default),5:random,6:spiralsquare,7:spiralrect,8:moronshiftflip)
            "sceneVariable:checkpoint_tile_order": 4, # (0:top,1:bottom,2:left,3:right,4:morton(default),5:random,6:spiralsquare,7:spiralrect,8:moronshiftflip)
            "sceneVariable:output_file": "",
            "sceneVariable:tmp_dir": "",
            "sceneVariable:two_stage_output": True,
            "sceneVariable:log_debug": False,
            "sceneVariable:log_info": False,

            "sceneVariable:fatal_color": [1, 0, 1],
            "sceneVariable_fatal_color": [1, 0, 1],

            "sceneVariable:stats_file": "",
            "sceneVariable:athena_debug": False,

            "sceneVariable:debug_pixel": {},
            "sceneVariable_debug_pixel": {},

            "sceneVariable:debug_rays_file": "",

            "sceneVariable:debug_rays_primary_range": {},
            "sceneVariable_debug_rays_primary_range": {},

            "sceneVariable:debug_rays_depth_range": {},
            "sceneVariable_debug_rays_depth_range": {},

            "sceneVariable:debug_console": -1,
            "sceneVariable:validate_geometry": False,
            "sceneVariable:cryptomatte_multi_presence": False



            }


        #
        # There's also these controls for the objects, but im not so sure how to transfer that data over.
        #
        # moonray:visible_in_camera
        # moonray:visible_shadow
        # moonray:visible_diffuse_reflection
        # moonray:visible_diffuse_transmission
        # moonray:visible_glossy_reflection
        # moonray:visible_glossy_transmission
        # moonray:visible_mirror_reflection
        # moonray:visible_mirror_transmission
        # moonray:visible_volume
        # moonray:side_type

        return result
    
    def update_render_passes(self, scene, render_layer):
        if render_layer.use_pass_z:
            self.register_pass(scene, render_layer, 'Depth', 1, 'Z', 'VALUE')

    def update(self, data, depsgraph):
        super().update(data, depsgraph)


def register():
    bpy.utils.register_class(MoonRayRenderEngine)

def unregister():
    bpy.utils.unregister_class(MoonRayRenderEngine)