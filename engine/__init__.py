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
        settings = bpy.context.scene.moonray

        result = {
            'volumeRaymarchingStepSize': 2.0,
            'volumeRaymarchingStepSizeLighting': 3.0,


            "sceneVariable:min_frame": 0.0,
            "sceneVariable:max_frame": 1.0,

            "sceneVariable:frame": 0.0,

            "sceneVariable:dicing_camera": "/freeCamera/camera",

            "sceneVariable:exr_header_attributes": 0.0,
            "sceneVariable:res": 0.0,
            "sceneVariable:aperture_window": 0.0,
            "sceneVariable:region_window": 0.0,
            "sceneVariable:sub_viewport": 0.0,

            "sceneVariable:fps": 0.0,
            "sceneVariable:scene_scale": 1.0,
            "sceneVariable:sampling_mode": 0.0,
            "sceneVariable:min_adaptive_samples": 0.0,
            "sceneVariable:max_adaptive_samples": 0.0,

            "sceneVariable:target_adaptive_error": 0.0,

            "sceneVariable:light_sampling_mode": 0.0,
            "sceneVariable:light_sampling_quality": 0.0,


            "sceneVariable:pixel_samples": 1,
            "sceneVariable:light_samples": 1,
            "sceneVariable:bsdf_samples": 1,
            "sceneVariable:bssrdf_samples": 1,

            "sceneVariable:max_depth": 1,
            "sceneVariable:max_diffuse_depth": 1,
            "sceneVariable:max_glossy_depth": 1,
            "sceneVariable:max_mirror_depth": 1,
            "sceneVariable:max_volume_depth": 1,
            "sceneVariable:max_presence_depth": 1,
            "sceneVariable:max_hair_depth": 1,

            "sceneVariable:disable_optimized_hair_sampling": 1,
            "sceneVariable:max_subsurface_per_path": 1,
            "sceneVariable:transparency_threshold": 1,
            "sceneVariable:presence_threshold": 1,
            "sceneVariable:lock_frame_noise": 1,
            "sceneVariable:volume_quality": 1,
            "sceneVariable:volume_shadow_quality": 1,

            "sceneVariable:volume_illumination_samples": 1,

            "sceneVariable:volume_opacity_threshold": 1,
            "sceneVariable:volume_overlap_mode": 1,
            "sceneVariable:volume_attenuation_factor": 1,

            "sceneVariable:volume_contribution_factor": 1,
            "sceneVariable:volume_phase_attenuation_factor": 1,

            "sceneVariable:path_guide_enable": 1,

            "sceneVariable:sample_clamping_value": 1,
            "sceneVariable:sample_clamping_depth": 1,

            "sceneVariable:roughness_clamping_factor": 1,
            "sceneVariable:texture_blur": 1,

            "sceneVariable:pixel_filter_width": 1,
            "sceneVariable:pixel_filter": 1,

            "sceneVariable:deep_format": 1,
            "sceneVariable:deep_curvature_tolerance": 1,

            "sceneVariable:deep_z_tolerance": 1,

            "sceneVariable:deep_vol_compression_res": 1,

            "sceneVariable:deep_id_attribute_name": 1,

            "sceneVariable:texture_cache_size": 1,

            "sceneVariable:crypto_uv_attribute_name": 1,
            "sceneVariable:texture_file_handles": 1,

            "sceneVariable:fast_geometry_update": 1,
            "sceneVariable:checkpoint_active": 0,
            "sceneVariable:checkpoint_interval": 100,
            "sceneVariable:checkpoint_quality_steps": 1,
            "sceneVariable:checkpoint_time_cap": 1,
            "sceneVariable:checkpoint_sample_cap": 1,

            "sceneVariable:checkpoint_overwrite": 1,
            "sceneVariable:checkpoint_mode": 1,           

            "sceneVariable:max_depth": 1, 

            "sceneVariable:checkpoint_mode": 1,
            "sceneVariable:checkpoint_start_sample": 1,
            "sceneVariable:checkpoint_bg_write": 1,
            "sceneVariable:checkpoint_post_script": 1,
            "sceneVariable:checkpoint_total_files": 1,
            "sceneVariable:checkpoint_max_bgcache": 1,
            "sceneVariable:checkpoint_max_snapshot_overhead": 1,
            "sceneVariable:checkpoint_snapshot_interval": 1,
            "sceneVariable:resumable_output": 1,
            "sceneVariable:resume_render": 1,
            "sceneVariable:on_resume_script": 1,
            "sceneVariable:enable_dof": 1,
            "sceneVariable:enable_max_geometry_resolution": 1,
            "sceneVariable:max_geometry_resolution": 1,
            "sceneVariable:enable_displacement": 1,
            "sceneVariable:enable_subsurface_scattering": 1,
            "sceneVariable:enable_shadowing": 1,
            "sceneVariable:enable_presence_shadows": 1,
            "sceneVariable:lights_visible_in_camera": 1,
            "sceneVariable:propagate_visibility_bounce_type": 1,
            "sceneVariable:shadow_terminator_fix": 1,
            "sceneVariable:machine_id": 1,
            "sceneVariable:num_machines": 1,
            "sceneVariable:task_distribution_type": 1,
            "sceneVariable:batch_tile_order": 1,
            "sceneVariable:progressive_tile_order": 1,
            "sceneVariable:checkpoint_tile_order": 1,
            "sceneVariable:output_file": 1,
            "sceneVariable:tmp_dir": 1,
            "sceneVariable:two_stage_output": 1,
            "sceneVariable:log_debug": 1,
            "sceneVariable:log_info": 1,
            "sceneVariable:fatal_color": 1,
            "sceneVariable:stats_file": 1,
            "sceneVariable:athena_debug": 1,
            "sceneVariable:debug_pixel": 1,
            "sceneVariable:debug_rays_file": 1,
            "sceneVariable:debug_rays_primary_range": 1,
            "sceneVariable:debug_rays_depth_range": 1,
            "sceneVariable:debug_console": 1,
            "sceneVariable:validate_geometry": 1,
            "sceneVariable:cryptomatte_multi_presence": 1



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