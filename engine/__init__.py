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
        
        os.environ["HDMOONRAY_DEBUG_MODE"] = "1"
        os.environ["HDMOONRAY_DEBUG"] = "1"
        os.environ["HDMOONRAY_INFO"] = "1"
        os.environ["HDMOONRAY_DISABLE"]="0"
        os.environ["HDMOONRAY_RDLA_OUTPUT"]="temp"

        pxr.Plug.Registry().RegisterPlugins([os.path.join(rel, "plugin", "pxr")])


    def get_render_settings(self, engine_type):
        settings = bpy.context.scene.moonray

        result = {
            'aovToken:Combined': "color",
            'aovToken:Depth': "depth"
            }

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