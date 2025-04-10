import bpy
from bpy.props import *

from . import sets
from . import userdata
from . import deep
from . import meta
from . import attributes
from . import render_output


class MoonRayAttributes_Mfb(bpy.types.PropertyGroup):

    hdmoonray_path : StringProperty(
        name="HdMoonRay Path",
        description="Path to the hydra plugin for MoonRay",
        subtype="FILE_PATH",
        default=""
    )
    
    execution_mode : EnumProperty(
        name="Execution Mode",
        description="MoonRay runs in four different execution modes",
        items=[
            ("0", "Scalar", """
Scalar mode processes one ray at a time. The rendering is distributed across multiple CPU cores, but MoonRay does not attempt to use the multiple SIMD lanes within the CPU cores for additional parallelism. It also does not batch rays together for improved memory access coherency.
Hence, it can be considered a “classical” path tracing algorithm
"""),
            ("1", "Vector", """
Vector mode achieves higher performance than scalar mode with two strategies:

Batch rays and shading operations together for improved memory access coherency.
Process multiple rays and shading calculations in parallel by using the multiple SIMD lanes within the multiple CPU cores.
The ray/shading batching is implemented as a “wave-front” path tracer, where rays and shading operations are batched and sorted into queues. When these queues fill up, they are processed/emptied as one batch of work. This makes better use of the CPU’s caches than scalar mode’s “single-ray” operation, which results in a more random memory access pattern.

SIMD calculation is implemented in special vectorized code. On typical CPUs, there are eight “lanes”, so up to eight rays or shading operations can be processed at once per CPU core.

Vector mode is designed to generate identical images as scalar mode, however due to architectural differences there are a few unsupported features:

1. Physically-correct overlapping dielectrics
2. Path guiding
3. Variance buffers
4. Volume rendering with deep file output
If the scene uses one of these unsupported features, a warning message will be logged and the scene will be rendered without the feature.

MoonRay’s vector mode is described in detail in the paper “Vectorized Production Path Tracing”, available from ACM at: https://dl.acm.org/doi/10.1145/3105762.3105768
"""),

            ("2", "XPU", """
MoonRay’s XPU mode uses a NVIDIA CUDA/OptiX-capable GPU to accelerate ray-scene intersection queries. Hence, it is not a complete GPU implementation of MoonRay, but rather uses the GPU as a heterogeneous coprocessor that offloads work from the CPU.

XPU mode is designed to pixel-match MoonRay’s vector mode output. It utilizes the vector mode infrastructure, hence it inherits the same performance benefits and feature limitations of vector mode.

XPU mode has the following additional unsupported features:

1. Round bezier curves
2. Round curves with more than 2 motion samples
3. Meshes with more than 2 motion samples
If the scene uses one of these unsupported features, a warning message will be logged and the scene will be rendered without the feature.

XPU mode may also fall back to CPU vector mode if there is insufficient GPU memory for the scene, or if there is a problem initializing the GPU
"""),

            ("3", "Auto", """
Auto mode will first try to render in xpu mode. If the scene uses a feature that is unsupported in xpu mode, MoonRay will fall back to vector mode. Then, if the scene uses a feature that is unsupported in vector mode, MoonRay will fall back to scalar mode. This prioritizes features over performance
"""),

        ],
        default="3"
    )


classes = [MoonRayAttributes_Mfb]


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    meta.register()
    deep.register()
    attributes.register()
    render_output.register()
    userdata.register()
    sets.register()

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)

    userdata.unregister()
    sets.unregister()
    render_output.unregister()
    attributes.unregister()
    deep.unregister()
    meta.unregister()