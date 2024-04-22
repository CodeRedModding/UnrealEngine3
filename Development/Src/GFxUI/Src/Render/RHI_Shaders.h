#ifndef INC_SF_Render_RHI_Shaders_H
#define INC_SF_Render_RHI_Shaders_H

#if WITH_GFx

namespace Scaleform
{
namespace Render
{
namespace RHI
{

struct Resource
{
    enum Flags
    {
        Uniform_Builtin  = 1,
		Uniform_TexScale = 2,
	};
    enum ResourceType
    {
        SR_srctex,
        SR_tex,
        SR_tex0,
        SR_tex1,
        SR_tex_a,
        SR_tex_u,
        SR_tex_v,
        SR_tex_y,
        SR_Count,

        SR_TotalSize = 990
    };
};

struct Uniform
{
    enum Flags
    {
        Uniform_Builtin  = 1,
		Uniform_TexScale = 2,
	};
    enum UniformType
    {
        SU_DFShadowColor,
        SU_DFShadowEnable,
        SU_DFShadowOffset,
        SU_DFSize,
        SU_InverseGamma,
        SU_cxadd,
        SU_cxmul,
        SU_fsize,
        SU_mvp,
        SU_offset,
        SU_scolor,
        SU_scolor2,
        SU_srctexscale,
        SU_texgen,
        SU_texscale,
        SU_vfmuniforms,
        SU_vfuniforms,
        SU_Count,

        SU_TotalSize = 990
    };
    static const unsigned char UniformFlags[];
};

enum ShaderFlags
{
    Shader_Batched  = 1,
	Shader_User     = 2,
};
enum ShaderStages
{
    Shader_Vertex   = 1,
    Shader_Fragment = 3,
};
struct UniformVar
{
	signed char   Location;
	short         ShadowOffset;
	unsigned char ElementSize;
    short         Size;
    unsigned char ElementCount;
	unsigned char BatchSize;
};

struct BatchVar
{
	char          Array;
	signed char   Offset;
	unsigned char Size;
};

struct VertexShaderDesc
{
    enum ShaderType
    {
        VS_None = 0,
        VS_start_base,
        VS_VVatc = 1,
        VS_VCxformVatc,
        VS_VPosition3dVatc,
        VS_VPosition3dCxformVatc,
        VS_VBatchVatc = 9,
        VS_VBatchCxformVatc,
        VS_VBatchPosition3dVatc,
        VS_VBatchPosition3dCxformVatc,
        VS_VTexTGVacolorVafactor = 17,
        VS_VTexTGCxformVacolorVafactor,
        VS_VTexTGPosition3dVacolorVafactor,
        VS_VTexTGPosition3dCxformVacolorVafactor,
        VS_VTexTGBatchVacolorVafactor = 25,
        VS_VTexTGBatchCxformVacolorVafactor,
        VS_VTexTGBatchPosition3dVacolorVafactor,
        VS_VTexTGBatchPosition3dCxformVacolorVafactor,
        VS_VYUVVafactor = 33,
        VS_VYUVCxformVafactor,
        VS_VYUVPosition3dVafactor,
        VS_VYUVPosition3dCxformVafactor,
        VS_VYUVYUVAVafactor,
        VS_VYUVYUVACxformVafactor,
        VS_VYUVPosition3dYUVAVafactor,
        VS_VYUVPosition3dYUVACxformVafactor,
        VS_VYUVBatchVafactor,
        VS_VYUVBatchCxformVafactor,
        VS_VYUVBatchPosition3dVafactor,
        VS_VYUVBatchPosition3dCxformVafactor,
        VS_VYUVBatchYUVAVafactor,
        VS_VYUVBatchYUVACxformVafactor,
        VS_VYUVBatchPosition3dYUVAVafactor,
        VS_VYUVBatchPosition3dYUVACxformVafactor,
        VS_VTextDFA,
        VS_VTextDFAPosition3d = 51,
        VS_VTextDFABatch = 57,
        VS_VTextDFABatchPosition3d = 59,
        VS_VTexTG = 65,
        VS_VTexTGCxform,
        VS_VTexTGPosition3d,
        VS_VTexTGPosition3dCxform,
        VS_VTexTGBatch = 73,
        VS_VTexTGBatchCxform,
        VS_VTexTGBatchPosition3d,
        VS_VTexTGBatchPosition3dCxform,
        VS_VTexTGTexTGVafactor = 81,
        VS_VTexTGTexTGCxformVafactor,
        VS_VTexTGTexTGPosition3dVafactor,
        VS_VTexTGTexTGPosition3dCxformVafactor,
        VS_VTexTGTexTGBatchVafactor = 89,
        VS_VTexTGTexTGBatchCxformVafactor,
        VS_VTexTGTexTGBatchPosition3dVafactor,
        VS_VTexTGTexTGBatchPosition3dCxformVafactor,
        VS_VText = 97,
        VS_VTextPosition3d = 99,
        VS_VTextBatch = 105,
        VS_VTextBatchPosition3d = 107,
        VS_VYUV = 113,
        VS_VYUVCxform,
        VS_VYUVPosition3d,
        VS_VYUVPosition3dCxform,
        VS_VYUVYUVA,
        VS_VYUVYUVACxform,
        VS_VYUVPosition3dYUVA,
        VS_VYUVPosition3dYUVACxform,
        VS_VYUVBatch,
        VS_VYUVBatchCxform,
        VS_VYUVBatchPosition3d,
        VS_VYUVBatchPosition3dCxform,
        VS_VYUVBatchYUVA,
        VS_VYUVBatchYUVACxform,
        VS_VYUVBatchPosition3dYUVA,
        VS_VYUVBatchPosition3dYUVACxform,
        VS_VTexTGVafactor,
        VS_VTexTGCxformVafactor,
        VS_VTexTGPosition3dVafactor,
        VS_VTexTGPosition3dCxformVafactor,
        VS_VTexTGBatchVafactor = 137,
        VS_VTexTGBatchCxformVafactor,
        VS_VTexTGBatchPosition3dVafactor,
        VS_VTexTGBatchPosition3dCxformVafactor,
        VS_V = 145,
        VS_VPosition3d = 147,
        VS_VBatch = 153,
        VS_VBatchPosition3d = 155,
        VS_VVacolorVafactor = 161,
        VS_VCxformVacolorVafactor,
        VS_VPosition3dVacolorVafactor,
        VS_VPosition3dCxformVacolorVafactor,
        VS_VBatchVacolorVafactor = 169,
        VS_VBatchCxformVacolorVafactor,
        VS_VBatchPosition3dVacolorVafactor,
        VS_VBatchPosition3dCxformVacolorVafactor,
        VS_VVacolor = 177,
        VS_VCxformVacolor,
        VS_VPosition3dVacolor,
        VS_VPosition3dCxformVacolor,
        VS_VBatchVacolor = 185,
        VS_VBatchCxformVacolor,
        VS_VBatchPosition3dVacolor,
        VS_VBatchPosition3dCxformVacolor,
        VS_end_base = 188,
        VS_start_shadows,
        VS_end_shadows = 188,
        VS_start_blurs,
        VS_VBox1 = 189,
        VS_VBox2,
        VS_end_blurs = 190,
        VS_start_cmatrix,
        VS_end_cmatrix = 190,
        VS_Count,

        VS_base_Cxform               = 0x00000001,
        VS_base_Position3d           = 0x00000002,
        VS_base_YUVA                 = 0x00000004,
        VS_base_Batch                = 0x00000008,
        VS_blurs_Box2                 = 0x00000001,
    };


    struct VertexAttrDesc
    {
        const char* Name;
        unsigned    Attr;
    };
    unsigned       Flags;
    char           NumAttribs;
    VertexAttrDesc Attributes[5];
    const char*    name;
    UniformVar     Resources[Resource::SR_Count];
    BatchVar       BatchResources[Resource::SR_Count];
    UniformVar     Uniforms[Uniform::SU_Count];
    BatchVar       BatchUniforms[Uniform::SU_Count];

    enum
    {
        MaxAttributes = 5,
        MaxTexParams  = 4,
    };

    static const VertexShaderDesc* Descs[VS_Count];

    static struct VertexShader* GetShader( ShaderType Type );
};

typedef VertexShaderDesc::ShaderType VertexShaderType;
struct FragShaderDesc
{
    enum ShaderType
    {
        FS_None = 0,
        FS_start_base,
        FS_FTexTGVertex = 1,
        FS_FTexTGVertexCxform,
        FS_FTexTGVertexBatch = 5,
        FS_FTexTGVertexBatchCxform,
        FS_FTexTGVertexMul = 9,
        FS_FTexTGVertexCxformMul,
        FS_FTexTGVertexBatchMul = 13,
        FS_FTexTGVertexBatchCxformMul,
        FS_FVertexVertexEAlpha = 17,
        FS_FVertexVertexCxformEAlpha,
        FS_FVertexVertexBatchEAlpha = 21,
        FS_FVertexVertexBatchCxformEAlpha,
        FS_FVertexVertexEAlphaMul = 25,
        FS_FVertexVertexCxformEAlphaMul,
        FS_FVertexVertexBatchEAlphaMul = 29,
        FS_FVertexVertexBatchCxformEAlphaMul,
        FS_FTexTGVertexEAlpha = 33,
        FS_FTexTGVertexCxformEAlpha,
        FS_FTexTGVertexBatchEAlpha = 37,
        FS_FTexTGVertexBatchCxformEAlpha,
        FS_FTexTGVertexEAlphaMul = 41,
        FS_FTexTGVertexCxformEAlphaMul,
        FS_FTexTGVertexBatchEAlphaMul = 45,
        FS_FTexTGVertexBatchCxformEAlphaMul,
        FS_FVertexVertex = 49,
        FS_FVertexVertexCxform,
        FS_FVertexVertexBatch = 53,
        FS_FVertexVertexBatchCxform,
        FS_FVertexVertexMul = 57,
        FS_FVertexVertexCxformMul,
        FS_FVertexVertexBatchMul = 61,
        FS_FVertexVertexBatchCxformMul,
        FS_FTexTGTexTGEAlpha = 65,
        FS_FTexTGTexTGCxformEAlpha,
        FS_FTexTGTexTGBatchEAlpha = 69,
        FS_FTexTGTexTGBatchCxformEAlpha,
        FS_FTexTGTexTGEAlphaMul = 73,
        FS_FTexTGTexTGCxformEAlphaMul,
        FS_FTexTGTexTGBatchEAlphaMul = 77,
        FS_FTexTGTexTGBatchCxformEAlphaMul,
        FS_FYUVEAlpha = 81,
        FS_FYUVCxformEAlpha,
        FS_FYUVYUVAEAlpha,
        FS_FYUVYUVACxformEAlpha,
        FS_FYUVBatchEAlpha,
        FS_FYUVBatchCxformEAlpha,
        FS_FYUVBatchYUVAEAlpha,
        FS_FYUVBatchYUVACxformEAlpha,
        FS_FYUVEAlphaMul,
        FS_FYUVCxformEAlphaMul,
        FS_FYUVYUVAEAlphaMul,
        FS_FYUVYUVACxformEAlphaMul,
        FS_FYUVBatchEAlphaMul,
        FS_FYUVBatchCxformEAlphaMul,
        FS_FYUVBatchYUVAEAlphaMul,
        FS_FYUVBatchYUVACxformEAlphaMul,
        FS_FYUV,
        FS_FYUVCxform,
        FS_FYUVYUVA,
        FS_FYUVYUVACxform,
        FS_FYUVBatch,
        FS_FYUVBatchCxform,
        FS_FYUVBatchYUVA,
        FS_FYUVBatchYUVACxform,
        FS_FYUVMul,
        FS_FYUVCxformMul,
        FS_FYUVYUVAMul,
        FS_FYUVYUVACxformMul,
        FS_FYUVBatchMul,
        FS_FYUVBatchCxformMul,
        FS_FYUVBatchYUVAMul,
        FS_FYUVBatchYUVACxformMul,
        FS_FText,
        FS_FTextBatch = 117,
        FS_FTextMul = 121,
        FS_FTextBatchMul = 125,
        FS_FTexTGTexTG = 129,
        FS_FTexTGTexTGCxform,
        FS_FTexTGTexTGBatch = 133,
        FS_FTexTGTexTGBatchCxform,
        FS_FTexTGTexTGMul = 137,
        FS_FTexTGTexTGCxformMul,
        FS_FTexTGTexTGBatchMul = 141,
        FS_FTexTGTexTGBatchCxformMul,
        FS_FVertex = 145,
        FS_FVertexCxform,
        FS_FVertexBatch = 149,
        FS_FVertexBatchCxform,
        FS_FVertexMul = 153,
        FS_FVertexCxformMul,
        FS_FVertexBatchMul = 157,
        FS_FVertexBatchCxformMul,
        FS_FTexTG = 161,
        FS_FTexTGCxform,
        FS_FTexTGBatch = 165,
        FS_FTexTGBatchCxform,
        FS_FTexTGMul = 169,
        FS_FTexTGCxformMul,
        FS_FTexTGBatchMul = 173,
        FS_FTexTGBatchCxformMul,
        FS_FVertexEAlpha = 177,
        FS_FVertexCxformEAlpha,
        FS_FVertexBatchEAlpha = 181,
        FS_FVertexBatchCxformEAlpha,
        FS_FVertexEAlphaMul = 185,
        FS_FVertexCxformEAlphaMul,
        FS_FVertexBatchEAlphaMul = 189,
        FS_FVertexBatchCxformEAlphaMul,
        FS_FTextColor = 193,
        FS_FTextColorCxform,
        FS_FTextColorBatch = 197,
        FS_FTextColorBatchCxform,
        FS_FTextColorMul = 201,
        FS_FTextColorCxformMul,
        FS_FTextColorBatchMul = 205,
        FS_FTextColorBatchCxformMul,
        FS_FTexTGEAlpha = 209,
        FS_FTexTGCxformEAlpha,
        FS_FTexTGBatchEAlpha = 213,
        FS_FTexTGBatchCxformEAlpha,
        FS_FTexTGEAlphaMul = 217,
        FS_FTexTGCxformEAlphaMul,
        FS_FTexTGBatchEAlphaMul = 221,
        FS_FTexTGBatchCxformEAlphaMul,
        FS_FTextDFA = 225,
        FS_FTextDFABatch = 229,
        FS_FTextDFAMul = 233,
        FS_FTextDFABatchMul = 237,
        FS_FSolid = 241,
        FS_FSolidBatch = 245,
        FS_end_base = 245,
        FS_start_shadows,
        FS_FBox2InnerShadow = 246,
        FS_FBox2InnerShadowHighlight,
        FS_FBox2InnerShadowKnockout,
        FS_FBox2InnerShadowKnockoutHighlight,
        FS_FBox2InnerShadowMul,
        FS_FBox2InnerShadowMulHighlight,
        FS_FBox2InnerShadowKnockoutMul,
        FS_FBox2InnerShadowKnockoutMulHighlight,
        FS_FBox2Shadow,
        FS_FBox2ShadowHighlight,
        FS_FBox2ShadowKnockout,
        FS_FBox2ShadowKnockoutHighlight,
        FS_FBox2ShadowMul,
        FS_FBox2ShadowMulHighlight,
        FS_FBox2ShadowKnockoutMul,
        FS_FBox2ShadowKnockoutMulHighlight,
        FS_FBox2Shadowonly,
        FS_FBox2ShadowonlyHighlight,
        FS_FBox2ShadowonlyMul = 266,
        FS_FBox2ShadowonlyMulHighlight,
        FS_end_shadows = 267,
        FS_start_blurs,
        FS_FBox1Blur = 268,
        FS_FBox2Blur,
        FS_FBox1BlurMul,
        FS_FBox2BlurMul,
        FS_end_blurs = 271,
        FS_start_cmatrix,
        FS_FTexTGCMatrixAc = 272,
        FS_FTexTGCMatrixAcMul,
        FS_end_cmatrix = 273,
        FS_Count,

        FS_base_Cxform               = 0x00000001,
        FS_base_YUVA                 = 0x00000002,
        FS_base_Batch                = 0x00000004,
        FS_base_Mul                  = 0x00000008,
        FS_shadows_Highlight            = 0x00000001,
        FS_shadows_Knockout             = 0x00000002,
        FS_shadows_Mul                  = 0x00000004,
        FS_blurs_Box2                 = 0x00000001,
        FS_blurs_Mul                  = 0x00000002,
        FS_cmatrix_Mul                  = 0x00000001,
    };


    unsigned    Flags;
    signed char TexParams[4];
    const char* name;
    UniformVar  Resources[Resource::SR_Count];
    BatchVar    BatchResources[Resource::SR_Count];
    UniformVar  Uniforms[Uniform::SU_Count];
    BatchVar    BatchUniforms[Uniform::SU_Count];

    enum
    {
        MaxAttributes = 5,
        MaxTexParams  = 4,
    };

    static const FragShaderDesc* Descs[FS_Count];
    static VertexShaderType VShaderForFShader[FragShaderDesc::FS_Count];
    static ShaderType FindStaticShader(const char* name, unsigned flags);

    static struct FragShader* GetShader( ShaderType Type );
};

typedef FragShaderDesc::ShaderType FragShaderType;

}
}
}

#endif

#endif
