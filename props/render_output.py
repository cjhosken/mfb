import bpy
from bpy.props import *

from .deep import MoonRayDeepIDAttribute
from .meta import MoonRayEXRHeaderAttribute
from .attributes import MoonRayAttributes_CameraAndLayer

class MoonRayAttributes_RenderOutput(bpy.types.PropertyGroup):
    active : BoolProperty(name="Active", description="true enables, false disables render output", default=True)

    camera : PointerProperty(
        name="Camera",
        description="Camera to use for this output. If not specified, defaults to the primary camera",
        type=bpy.types.Object,
        poll=lambda self, obj: obj.type == 'CAMERA'
    )

    channel_format : EnumProperty(
        name="Channel Format",
        description="The pixel encoding (bit depth and type) of the output channel",
        items=[
            ("0", "Float", ""),
            ("1", "Half", "")
        ],
        default="1"
    )

    channel_name : StringProperty(
        name="Channel Name",
        description="Name of the output channel. In the case of an empty channel name a sensible default name is chosen",
        default="beauty"
    )

    channel_suffix_mode : EnumProperty(
        name="Channel Suffix Mode",
        description="When processing multi-channel outputs, how should channel names be suffixed?",
        items=[
            ("0", "Auto", "A best guess suffix is chosen based on the type of output"),
            ("1", "RGB", ".R, .G, .B"),
            ("2", "XYZ", ".X, .Y, .Z"),
            ("3", "UVW", ".U, .V, .W")
        ],
        default="1"
    )

    checkpoint_file_name : StringProperty(
        name="Checkpoint File Name",
        description="Name of checkpoint output file",
        default="checkpoint.exr"
    )

    checkpoint_multi_version_file_name : StringProperty(
        name="Checkpoint Multi Version File Name",
        description="Name of checkpoint output file under checkpoint file overwrite=off condition",
        default=""
    )

    compression : EnumProperty(
        name="Compression",
        description="Compression used for file (or file part in the multi-part case). All render outputs that target the same image must specify the same compression",
        items=[
            ("0", "None", ""),
            ("1", "Zip", ""),
            ("2", "RLE", ""),
            ("3", "Zips", ""),
            ("4", "PIZ", ""),
            ("5", "Pxr24", ""),
            ("6", "B44", ""),
            ("7", "B44A", ""),
            ("8", "DWAA", ""),
            ("9", "DWAB", "")
        ],
        default="1"
    )

    cryptomatte_depth : IntProperty(name="Cryptomatte Depth", description="", default=6)
    cryptomatte_enable_refract : BoolProperty(name="Cryptomatte Enable Refract", description="Enable refractive cryptomatte channels. Doubles the number of cryptomatte channels", default=True)
    cryptomatte_output_beauty : BoolProperty(name="Cryptomatte Output Beauty", description="Whether to output beauty data per cryptomatte id", default=False)
    cryptomatte_output_normals: BoolProperty(name="Cryptomatte Output Normals", description="Whether to output shading normal data per cryptomatte id", default=False)
    cryptomatte_output_p0 : BoolProperty(name="Cryptomatte Output p0", description="Whether to output p0 data per cryptomatte id", default=False)
    cryptomatte_output_positions : BoolProperty(name="Cryptomatte Output Positions", description="Whether to output position data per cryptomatte id", default=False)
    cryptomatte_output_refn : BoolProperty(name="Cryptomatte Output RefN", description="Whether to output refn data per cryptomatte id", default=False)
    cryptomatte_output_refp : BoolProperty(name="Cryptomatte Outpiut RefP", description="Whether to output refp data per cryptomatte id", default=False)
    cryptomatte_output_uv : BoolProperty(name="Cryptomatte Output UV", description="Whether to output uv data per cryptomatte id", default=False)
    cryptomatte_support_resume_render : BoolProperty(name="Cryptomatte Support Resume Render", description="Whether to add additional cryptomatte layers to support checkpoint/resume rendering", default=False)
    denoise : BoolProperty(name="Enable Denoising", description="Run optix denoiser before writing to disk", default=False)

    denoiser_input: EnumProperty(
        name="Denoiser Input",
        description="How to use this output as a denoiser input",
        items=[
            ("0", "Not an Input", ""),
            ("1", "As Albedo", ""),
            ("2", "As Normal", "")
        ],
        default="0"
    )

    display_filter : PointerProperty(
        name="Display Filter",
        description="If 'result' is 'display filter', this attribute refers to a display filter object which is used to compute the output pixel values.",
        type=MoonRayEXRHeaderAttribute
    )

    exr_dwa_compression_level : FloatProperty(
        name="EXR DWA Compression Level",
        description="Compression level used for file with dwaa or dwab compression. All render outputs that target the same image must specify the same compression level",
        default=85.0
    )

    exr_header_attributes : CollectionProperty(name="EXR Header Attributes", description="Metadata that is passed directly to the exr header. Format: {'name', 'type', 'value'}", type=MoonRayEXRHeaderAttribute)

    file_name: StringProperty(name="File Name", description="Name of destination file", subtype="FILE_PATH", default="scene.exr")
    file_part : StringProperty(name="File Part", description="Name of sub-image if using a multi-part exr file", default="")
    lpe : StringProperty(
        name="LPE",
        description="""This attribute specifies a light path expression to output. For details on light path expression syntax see:
  https://github.com/imageworks/OpenShadingLanguage/wiki/OSL-Light-Path-Expressions
 Labels on scattering events are constructed from two parts: [ML.]LL Where:
  <ML> is the label attribute value of the material (if non-empty)
  <LL> is the lobe label assigned in the shader by the shader writer
 Labels on light events are set from the label attribute of the light.
 Additionally, a small set of pre-defined expressions are available:
  'caustic' : CD[S]+[<L.>O]
  'diffuse' : CD[<L.>O]
  'emission' : CO
  'glossy' : CG[<L.>O]
  'mirror' : CS[<L.>O]
  'reflection' : C<RS>[DSG]+[<L.>O]
  'translucent' : C<TD>[DSG]+[<L.>O]
  'transmission' : C<TS>[DSG]+[<L.>O]""",
        default=""
    )
    
    material_aov : StringProperty(
        name="Material Aov",
        description="""If 'result' is 'material aov', this attribute specifies a material aov expression to output. The expression format is:
 [('<GL>')+\.][('<ML>')+\.][('<LL>')+\.][(SS|R|T|D|G|M)+\.][fresnel\.]<property>. Where:
  <GL> is a label associated with the geometry
  <ML> is a label associated with the material
  <LL> is a lobe label
  R means reflection side lobe
  T means transmission side lobe
  D means diffuse lobe category
  G means glossy lobe category
  M means mirror lobe category
  SS means sub-surface component of the material
  fresnel means to select the lobe's or sub-surface's fresnel
  <property> can be one of:
   'albedo' (bsdf lobe | subsurface) (RGB),
   'color' (bsdf lobe | subsurface | fresnel) (RGB),
   'depth' (state variable) (FLOAT),
   'dPds' (state variable) (VEC3F),
   'dPdt' (state variable) (VEC3F),
   'dSdx' (state variable) (FLOAT),
   'dSdy' (state variable) (FLOAT),
   'dTdx' (state variable) (FLOAT),
   'dTdy' (state variable) (FLOAT),
   'emission' (bsdf) (RGB),
   'factor' (fresnel) (FLOAT),
   'float:<attr>' (primitive attribute) (FLOAT),
   'matte' (bsdf lobe | subsurface) (FLOAT),
   'motionvec' (state variable) (VEC2F),
   'N' (state variable) (VEC3F),
   'Ng' (state variable) (VEC3F),
   'normal' (bsdf lobe | subsurface) (VEC3F),
   'P' (state variable) (VEC3F),
   'pbr_validity' (bsdf lobe | subsurface) (RGB),
   'radius' (subsurface) (RGB),
   'rgb:<attr>' (primitive attribute) (RGB),
   'roughness' (bsdf lobe) (fresnel) (VEC2F),
   'St' (state variable) (VEC2F),
   'vec2:<attr>' (primitive attribute) (VEC2F),
   'vec3:<attr>' (primitive attribute) (VEC3F),
   'Wp' (state variable) (VEC3F)
 Examples:
  albedo : Albedo of all rendered materials
  R.albedo : Total reflection albedo
  'spec'.MG.roughness : Roughness of all mirror and glossy lobes that have the 'spec' label""",
        default=""
    )
    
    math_filter : EnumProperty(
        name="Math Filter",
        description="the math filter over the pixel",
        items=[
            ("0", "Average", ""),
            ("1", "Sum", ""),
            ("2", "Min", ""),
            ("3", "Max", ""),
            ("4", "Force_Consistent_Sampling", "Average of the first 'min_adaptive_samples'"),
            ("5", "Closest", "Use sample with minimum z-depth")
        ],
        default="5"
    )

    output_type : StringProperty(
        name="Output Type",
        description="Specifies the type of output. Defaults to 'flat', meaning a flat exr file. 'deep' will output a deep exr file",
        default="flat"
    )

    primitive_attribute : StringProperty(
        name="Primtive Attribute",
        description="If 'result' is 'primitive attribute', this attribute specifies the particular primitive attribute to output. Default channel name is based on primitive attribute name and type",
        default=""
    )

    primitive_attribute_type : EnumProperty(
        name="Primitive Attribute Type",
        description="This attribute specifies the type of the attribute named with the 'primitive attribute' setting. This is required to uniquely specify the primitive attribute",
        items=[
            ("0", "Float", ""),
            ("1", "Vec2f", ""),
            ("2", "Vec3f", ""),
            ("3", "RGB", "")
        ],
        default="0"
    )

    result : EnumProperty(
        name="Result",
        description="The result to output",
        items=[
            ("0", "Beauty", "Full render (R, G, B)"),
            ("1", "Alpha", "Full render alpha channel (A)"),
            ("2", "Depth", "Z distance from camera (Z)"),
            ("3", "State Variable", "Built-in state variable"),
            ("4", "Primitive Attribute", "Procedural provided attributes"),
            ("5", "Time Per Pixel", "Time per pixel heat map metric"),
            ("6", "Wireframe", "Render as wireframe"),
            ("7", "Material Aov", "Aovs provided via material expressions"),
            ("8", "Light Aov", "Aovs provided via light path expressions"),
            ("9", "Visiblity Aov", "Fraction of light samples that hit light source"),
            ("11", "Weight", "weight"),
            ("12", "Beauty Aux", "RenderBuffer auxiliary sample data for adaptive sampling"),
            ("13", "Cryptomatte", "Cryptomatte"),
            ("14", "Alpha Aux", "Alpha auxiliary sample data for adaptive sampling"),
            ("15", "Display Filter", "Output results from a display filter")
        ],
        default="0"
    )

    resume_file_name: StringProperty(
        name="Resume File Name",
        description="Name of input file for resume render start condition",
        default=""
    )

    state_variable : EnumProperty(
        name="State Variable",
        description="If 'result' is 'state variable', this attribute specifies the particular state variable result",
        items=[
            ("0", "P", "Position (P.X, P.Y, P.Z)"),
            ("1", "Ng", "Geometric normal (Ng.X, Ng.Y, Ng.Z)"),
            ("2", "N", "Normal (N.X, N.Y, N.Z)"),
            ("3", "St", "Texture coordinates (St.X, St.Y)"),
            ("4", "dPds", "Derivative of P w.r.t S (dPds.X, dPds.Y, dPds.Z)"),
            ("5", "dPdt", "Derivative of P w.r.t T (dPdt.X, dPdt.Y, dPdt.Z)"),
            ("6", "dSdx", "S derivative w.r.t. x (dSdx)"),
            ("7", "dSdy", "S derivative w.r.t. y (dSdy)"),
            ("8", "dTdx", "T derivative w.r.t. x (dTdx)"),
            ("9", "dTdy", "T derivative w.r.t. y (dTdy)"),
            ("10", "Wp", "World position (Wp.X, Wp.Y, Wp.Z)"),
            ("11", "Depth", "Z distance from camera (Z)"),
            ("12", "MotionVec", "2D motion vector")
        ],
        default="2"
    )

    visibility_aov : StringProperty(
        name="Visibility Aov",
        description="If 'result' is 'visibility aov', this attribute specifies a light path expression that defines the set of all paths used to compute the visibility ratio.",
        default="C[<T.><RS>]*[<R[DG]><TD>][LO]"
    )


classes = [
    MoonRayAttributes_RenderOutput
]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)