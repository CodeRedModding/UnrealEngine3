/*=============================================================================
	RHI.h: Render Hardware Interface definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __RHI_H__
#define __RHI_H__

#include "UnBuild.h"

/*
Platform independent RHI definitions.
The use of non-member functions to operate on resource references allows two different approaches to RHI implementation:
- Resource references are to RHI data structures, which directly contain the platform resource data.
- Resource references are to the platform's HAL representation of the resource.  This eliminates a layer of indirection for platforms such as
	D3D where the RHI doesn't directly store resource data.
*/

//
// RHI globals.
//

/** True if the render hardware has been initialized. */
extern UBOOL GIsRHIInitialized;

//
// RHI capabilities.
//

/** Maximum number of miplevels in a texture. */
enum { MAX_TEXTURE_MIP_COUNT = 14 };

/** The maximum number of vertex elements which can be used by a vertex declaration. */
enum { MaxVertexElementCount = 16 };

/** The alignment in bytes between elements of array shader parameters. */
enum { ShaderArrayElementAlignBytes = 16 };

/** The maximum number of mip-maps that a texture can contain. 	*/
extern	INT		GMaxTextureMipCount;
/** The minimum number of mip-maps that always remain resident.	*/
extern 	INT		GMinTextureResidentMipCount;

/** TRUE if PF_DepthStencil textures can be created and sampled. */
extern UBOOL GSupportsDepthTextures;

/** Features supported by the platform. */
struct FPlatformFeatures
{
	/** Constructor for the hardware features object. Sets up all default values. */
	FPlatformFeatures();

	/** Maximum level of texture anisotropic filtering that the platform supports. 1 if it's not supported. */
	INT MaxTextureAnisotropy;

	/** Primarily for OpenGL ES 2.0 platforms. Telling the driver to discard the rendertarget (FBO) let's it avoid having to
	    save out all color/depth/stencil buffers to memory when switching away to another rendertarget, or avoid having to
		reload them all from memory when switching back to a previously used rendertarget.
		See also: http://www.khronos.org/registry/gles/extensions/EXT/EXT_discard_framebuffer.txt
	*/
	UBOOL bSupportsRendertargetDiscard;
};

/** Features supported by the platform. */
extern FPlatformFeatures	GPlatformFeatures;

/** 
* TRUE if PF_DepthStencil textures can be created and sampled to obtain PCF values. 
* This is different from GSupportsDepthTextures in three ways:
*	-results of sampling are PCF values, not depths
*	-a color target must be bound with the depth stencil texture even if never written to or read from,
*		due to API restrictions
*	-a dedicated resolve surface may or may not be necessary
*/
extern UBOOL GSupportsHardwarePCF;

/**
* TRUE if the our renderer, the driver and the hardware supports the feature.
*/
extern UBOOL GSupportsVertexTextureFetch;

/** TRUE if D24 textures can be created and sampled, retrieving 4 neighboring texels in a single lookup. */
extern UBOOL GSupportsFetch4;

/**
* TRUE if floating point filtering is supported
*/
extern UBOOL GSupportsFPFiltering;

/** TRUE if PF_G8 render targets are supported */
extern UBOOL GSupportsRenderTargetFormat_PF_G8;

/** Can we handle quad primitives? */
extern UBOOL GSupportsQuads;

/** Are we using an inverted depth buffer? Viewport MinZ/MaxZ reversed */
extern UBOOL GUsesInvertedZ;

/** The offset from the upper left corner of a pixel to the position which is used to sample textures for fragments of that pixel. */
extern FLOAT GPixelCenterOffset;

/** The maximum size to allow for the shadow depth buffer in the X dimension.  This must be larger or equal to GMaxPerObjectShadowDepthBufferSizeY. */
extern INT GMaxPerObjectShadowDepthBufferSizeX;
/** The maximum size to allow for the shadow depth buffer in the Y dimension. */
extern INT GMaxPerObjectShadowDepthBufferSizeY;

/** The maximum size to allow for the whole scene shadow depth buffer. */
extern INT GMaxWholeSceneDominantShadowDepthBufferSize;

/** Bias exponent used to apply to shader color output when rendering to the scene color surface */
extern INT GCurrentColorExpBias;

/** TRUE if we are running with the NULL RHI */
extern UBOOL GUsingNullRHI;

/** TRUE if we are running with the ES2 RHI */
extern UBOOL GUsingES2RHI;

/** TRUE if we are running with a mobile RHI */
extern UBOOL GUsingMobileRHI;

/** Whether to use the post-process code path on mobile. */
extern UBOOL GMobileAllowPostProcess;

/** Whether the current mobile renderer is a tiled architecture. */
extern UBOOL GMobileTiledRenderer;

/** Whether the current mobile implementation uses a packed depth stencil format. */
extern UBOOL GMobileUsePackedDepthStencil;

/** Whether the current mobile device supports shader discard. */
extern UBOOL GMobileAllowShaderDiscard;

/** Whether the current mobile device supports shader bump offset. */
extern UBOOL GMobileDeviceAllowBumpOffset;

/** Whether the current mobile device can make reliable framebuffer status checks */ 
extern UBOOL GMobileAllowFramebufferStatusCheck;

/**
 *	The size to check against for Draw*UP call vertex counts.
 *	If greater than this value, the draw call will not occur.
 */
extern INT GDrawUPVertexCheckCount;
/**
 *	The size to check against for Draw*UP call index counts.
 *	If greater than this value, the draw call will not occur.
 */
extern INT GDrawUPIndexCheckCount;

/** TRUE if the rendering hardware supports vertex instancing. */
extern UBOOL GSupportsVertexInstancing;

/** TRUE if the rendering hardware can emulate vertex instancing. */
extern UBOOL GSupportsEmulatedVertexInstancing;

/** If FALSE code needs to patch up vertex declaration. */
extern UBOOL GVertexElementsCanShareStreamOffset;

/** TRUE for each VET that is supported. One-to-one mapping with EVertexElementType */
extern class FVertexElementTypeSupportInfo GVertexElementTypeSupport;

/** MSAA level that the engine is tweaked to best run at. */
extern INT GOptimalMSAALevel;

/** When greater than one, indicates that SLI rendering is enabled */
#if !CONSOLE
#define WITH_SLI (1)
extern INT GNumActiveGPUsForRendering;
#else
#define WITH_SLI (0)
#define GNumActiveGPUsForRendering (1)
#endif

/** Whether the next frame should profile the GPU. */
extern UBOOL GProfilingGPU;

/** Whether we are profiling GPU hitches. */
extern UBOOL GProfilingGPUHitches;

/** Bit mask for what texture formats a platform supports. */
enum ETextureFormatSupport
{
	TEXSUPPORT_DXT		= 0x01,
	TEXSUPPORT_PVRTC	= 0x02,
	TEXSUPPORT_ATITC	= 0x04,
	TEXSUPPORT_ETC		= 0x08,
};

/** Bit flags from ETextureFormatSupport, specifying what texture formats a platform supports. */
extern DWORD GTextureFormatSupport;

// if we can reload ALL rhi resources (really only working fully on android)
extern UBOOL GAllowFullRHIReset;

//
// Common RHI definitions.
//

enum ESamplerFilter
{
	SF_Point,
	SF_Bilinear,
	SF_Trilinear,
	SF_AnisotropicPoint,
	SF_AnisotropicLinear,
};

enum ESamplerAddressMode
{
	AM_Wrap,
	AM_Clamp,
	AM_Mirror,
	/** Not supported on all platforms */
	AM_Border
};

enum ESamplerMipMapLODBias
{
	MIPBIAS_None,
	MIPBIAS_HigherResolution_1 = -1,
	MIPBIAS_HigherResolution_2 = -2,
	MIPBIAS_HigherResolution_3 = -3,
	MIPBIAS_HigherResolution_4 = -4,
	MIPBIAS_HigherResolution_5 = -5,
	MIPBIAS_HigherResolution_6 = -6,
	MIPBIAS_HigherResolution_7 = -7,
	MIPBIAS_HigherResolution_8 = -8,
	MIPBIAS_HigherResolution_9 = -9,
	MIPBIAS_HigherResolution_10 = -10,
	MIPBIAS_HigherResolution_11 = -11,
	MIPBIAS_HigherResolution_12 = -12,
	MIPBIAS_HigherResolution_13 = -13,
	MIPBIAS_LowerResolution_1 = 1,
	MIPBIAS_LowerResolution_2 = 2,
	MIPBIAS_LowerResolution_3 = 3,
	MIPBIAS_LowerResolution_4 = 4,
	MIPBIAS_LowerResolution_5 = 5,
	MIPBIAS_LowerResolution_6 = 6,
	MIPBIAS_LowerResolution_7 = 7,
	MIPBIAS_LowerResolution_8 = 8,
	MIPBIAS_LowerResolution_9 = 9,
	MIPBIAS_LowerResolution_10 = 10,
	MIPBIAS_LowerResolution_11 = 11,
	MIPBIAS_LowerResolution_12 = 12,
	MIPBIAS_LowerResolution_13 = 13,
	MIPBIAS_Get4=100
};

enum ESamplerCompareFunction
{
	SCF_Never,
	SCF_Less
};

enum ERasterizerFillMode
{
	FM_Point,
	FM_Wireframe,
	FM_Solid
};

enum ERasterizerCullMode
{
	CM_None,
	CM_CW,
	CM_CCW
};

enum EColorWriteMask
{
	CW_RED		= 0x01,
	CW_GREEN	= 0x02,
	CW_BLUE		= 0x04,
	CW_ALPHA	= 0x08,

	CW_RGB		= 0x07,
	CW_RGBA		= 0x0f,
};

enum ECompareFunction
{
	CF_Less,
	CF_LessEqual,
	CF_Greater,
	CF_GreaterEqual,
	CF_Equal,
	CF_NotEqual,
	CF_Never,
	CF_Always
};

enum EStencilOp
{
	SO_Keep,
	SO_Zero,
	SO_Replace,
	SO_SaturatedIncrement,
	SO_SaturatedDecrement,
	SO_Invert,
	SO_Increment,
	SO_Decrement
};

enum EBlendOperation
{
	BO_Add,
	BO_Subtract,
	BO_Min,
	BO_Max,
    BO_ReverseSubtract,
};

enum EBlendFactor
{
	BF_Zero,
	BF_One,
	BF_SourceColor,
	BF_InverseSourceColor,
	BF_SourceAlpha,
	BF_InverseSourceAlpha,
	BF_DestAlpha,
	BF_InverseDestAlpha,
	BF_DestColor,
	BF_InverseDestColor,
	// Only implemented for ES2
	BF_ConstantBlendColor
};

enum EVertexElementType
{
	VET_None,
	VET_Float1,
	VET_Float2,
	VET_Float3,
	VET_Float4,
	VET_PackedNormal,	// FPackedNormal
	VET_UByte4,
	VET_UByte4N,
	VET_Color,
	VET_Short2,
	VET_Short2N,		// 16 bit word normalized to (value/32767.0,value/32767.0,0,0,1)
	VET_Half2,			// 16 bit float using 1 bit sign, 5 bit exponent, 10 bit mantissa 
	VET_Pos3N,			// Normalized, 3D signed 11 11 10 format expanded to (value/1023.0, value/1023.0, value/511.0, 1). This is only valid in Xbox360 and PS3
	VET_MAX
};

enum EVertexElementUsage
{
	VEU_Position,
	VEU_TextureCoordinate,
	VEU_BlendWeight,
	VEU_BlendIndices,
	VEU_Normal,
	VEU_Tangent,
	VEU_Binormal,
	VEU_Color
};

enum ECubeFace
{
	CubeFace_PosX=0,
	CubeFace_NegX,
	CubeFace_PosY,
	CubeFace_NegY,
	CubeFace_PosZ,
	CubeFace_NegZ,
	CubeFace_MAX
};

/** limited to 8 types in FReadSurfaceDataFlags */
enum ERangeCompressionMode
{
	// 0 .. 1
	RCM_UNorm,
	// -1 .. 1
	RCM_SNorm,
	// 0 .. 1 unless there are smaller values than 0 or bigger values than 1, then the range is extended to the minimum or the maximum of the values
	RCM_MinMaxNorm,
	// minimum .. maximum (each channel independent)
	RCM_MinMax,
};

/** Bitfield types of render buffers, used by RHIDiscardRenderBuffer(). */
enum ERenderBufferType
{
	RBT_Color	= 0x01,
	RBT_Depth	= 0x02,
	RBT_Stencil = 0x04,
};

/** to customize the RHIReadSurfaceData() output */
class FReadSurfaceDataFlags
{
public:
	// @param InCompressionMode defines the value input range that is mapped to output range
	// @param InCubeFace defined which cubemap side is used, only required for cubemap content, then it needs to be a valid side
	FReadSurfaceDataFlags(ERangeCompressionMode InCompressionMode = RCM_UNorm, ECubeFace InCubeFace = CubeFace_MAX) 
		:CubeFace(InCubeFace), CompressionMode(InCompressionMode), bLinearToGamma(TRUE), MaxDepthRange(16000.0f)
	{
	}

	ECubeFace GetCubeFace() const
	{
		checkSlow(CubeFace <= CubeFace_NegZ);
		return CubeFace;
	}

	ERangeCompressionMode GetCompressionMode() const
	{
		return CompressionMode;
	}

	void SetLinearToGamma(UBOOL Value)
	{
		bLinearToGamma = Value;
	}

	UBOOL GetLinearToGamma() const
	{
		return bLinearToGamma;
	}

	void SetMaxDepthRange(FLOAT Value)
	{
		MaxDepthRange = Value;
	}

	FLOAT ComputeNormalizedDepth(FLOAT DeviceZ) const
	{
		return abs(ConvertFromDeviceZ(DeviceZ) / MaxDepthRange);
	}

private:

	// @return SceneDepth
	FLOAT ConvertFromDeviceZ(float DeviceZ) const
	{
		DeviceZ = Min(DeviceZ, 1 - Z_PRECISION);

		// for depth to linear conversion
		const FVector2D InvDeviceZToWorldZ(0.1f, 0.1f);

		return 1.0f / (DeviceZ * InvDeviceZToWorldZ.X - InvDeviceZToWorldZ.Y);
	}

	ECubeFace CubeFace;
	ERangeCompressionMode CompressionMode;
	UBOOL bLinearToGamma;
	FLOAT MaxDepthRange;
};

/** Info for supporting the vertex element types */
class FVertexElementTypeSupportInfo
{
public:
	FVertexElementTypeSupportInfo() { for(INT i=0; i<VET_MAX; i++) ElementCaps[i]=TRUE; }
	FORCEINLINE UBOOL IsSupported(EVertexElementType ElementType) { return ElementCaps[ElementType]; }
	FORCEINLINE void SetSupported(EVertexElementType ElementType,UBOOL bIsSupported) { ElementCaps[ElementType]=bIsSupported; }
private:
	/** cap bit set for each VET. One-to-one mapping based on EVertexElementType */
	UBOOL ElementCaps[VET_MAX];
};

struct FVertexElement
{
	BYTE StreamIndex;
	BYTE Offset;
	BYTE Type;
	BYTE Usage;
	BYTE UsageIndex;
	UBOOL bUseInstanceIndex;
	UINT NumVerticesPerInstance;

	FVertexElement() {}
	FVertexElement(BYTE InStreamIndex,BYTE InOffset,BYTE InType,BYTE InUsage,BYTE InUsageIndex,UBOOL bInUseInstanceIndex = FALSE,UINT InNumVerticesPerInstance = 0):
		StreamIndex(InStreamIndex),
		Offset(InOffset),
		Type(InType),
		Usage(InUsage),
		UsageIndex(InUsageIndex),
		bUseInstanceIndex(bInUseInstanceIndex),
		NumVerticesPerInstance(InNumVerticesPerInstance)
	{}
	/**
	* Suppress the compiler generated assignment operator so that padding won't be copied.
	* This is necessary to get expected results for code that zeros, assigns and then CRC's the whole struct.
	*/
	void operator=(const FVertexElement& Other)
	{
		StreamIndex = Other.StreamIndex;
		Offset = Other.Offset;
		Type = Other.Type;
		Usage = Other.Usage;
		UsageIndex = Other.UsageIndex;
		bUseInstanceIndex = Other.bUseInstanceIndex;
		NumVerticesPerInstance = Other.NumVerticesPerInstance;
	}
};

typedef TPreallocatedArray<FVertexElement,MaxVertexElementCount> FVertexDeclarationElementList;

struct FSamplerStateInitializerRHI
{
	FSamplerStateInitializerRHI(
		ESamplerFilter InFilter,
		ESamplerAddressMode InAddressU=AM_Wrap,
		ESamplerAddressMode InAddressV=AM_Wrap,
		ESamplerAddressMode InAddressW=AM_Wrap,
		ESamplerMipMapLODBias InMipBias=MIPBIAS_None,
		INT InMaxAnisotropy=0,
		DWORD InBorderColor=0,
		/** Only supported in D3D11 */
		ESamplerCompareFunction InSamplerComparisonFunction=SCF_Never
		)
	:	Filter(InFilter)
	,	AddressU(InAddressU)
	,	AddressV(InAddressV)
	,	AddressW(InAddressW)
	,	MipBias(InMipBias)
	,	MaxAnisotropy(InMaxAnisotropy)
	,	BorderColor(InBorderColor)
	,	SamplerComparisonFunction(InSamplerComparisonFunction)
	{
	}
	ESamplerFilter Filter;
	ESamplerAddressMode AddressU;
	ESamplerAddressMode AddressV;
	ESamplerAddressMode AddressW;
	ESamplerMipMapLODBias MipBias;
	// Note: setting to a different value than GSystemSettings.MaxAnisotropy is only supported in D3D11
	INT MaxAnisotropy;
	DWORD BorderColor;
	ESamplerCompareFunction SamplerComparisonFunction;
};
struct FRasterizerStateInitializerRHI
{
	ERasterizerFillMode FillMode;
	ERasterizerCullMode CullMode;
	FLOAT DepthBias;
	FLOAT SlopeScaleDepthBias;
	UBOOL bAllowMSAA;
};
struct FDepthStateInitializerRHI
{
	UBOOL bEnableDepthWrite;
	ECompareFunction DepthTest;
};

struct FStencilStateInitializerRHI
{
	FStencilStateInitializerRHI(
		UBOOL bInEnableFrontFaceStencil = FALSE,
		ECompareFunction InFrontFaceStencilTest = CF_Always,
		EStencilOp InFrontFaceStencilFailStencilOp = SO_Keep,
		EStencilOp InFrontFaceDepthFailStencilOp = SO_Keep,
		EStencilOp InFrontFacePassStencilOp = SO_Keep,
		UBOOL bInEnableBackFaceStencil = FALSE,
		ECompareFunction InBackFaceStencilTest = CF_Always,
		EStencilOp InBackFaceStencilFailStencilOp = SO_Keep,
		EStencilOp InBackFaceDepthFailStencilOp = SO_Keep,
		EStencilOp InBackFacePassStencilOp = SO_Keep,
		DWORD InStencilReadMask = 0xFFFFFFFF,
		DWORD InStencilWriteMask = 0xFFFFFFFF,
		DWORD InStencilRef = 0) :
		bEnableFrontFaceStencil(bInEnableFrontFaceStencil),
		FrontFaceStencilTest(InFrontFaceStencilTest),
		FrontFaceStencilFailStencilOp(InFrontFaceStencilFailStencilOp),
		FrontFaceDepthFailStencilOp(InFrontFaceDepthFailStencilOp),
		FrontFacePassStencilOp(InFrontFacePassStencilOp),
		bEnableBackFaceStencil(bInEnableBackFaceStencil),
		BackFaceStencilTest(InBackFaceStencilTest),
		BackFaceStencilFailStencilOp(InBackFaceStencilFailStencilOp),
		BackFaceDepthFailStencilOp(InBackFaceDepthFailStencilOp),
		BackFacePassStencilOp(InBackFacePassStencilOp),
		StencilReadMask(InStencilReadMask),
		StencilWriteMask(InStencilWriteMask),
		StencilRef(InStencilRef)
	{
	}

	UBOOL bEnableFrontFaceStencil;
	ECompareFunction FrontFaceStencilTest;
	EStencilOp FrontFaceStencilFailStencilOp;
	EStencilOp FrontFaceDepthFailStencilOp;
	EStencilOp FrontFacePassStencilOp;
	UBOOL bEnableBackFaceStencil;
	ECompareFunction BackFaceStencilTest;
	EStencilOp BackFaceStencilFailStencilOp;
	EStencilOp BackFaceDepthFailStencilOp;
	EStencilOp BackFacePassStencilOp;
	DWORD StencilReadMask;
	DWORD StencilWriteMask;
	DWORD StencilRef;
};
struct FBlendStateInitializerRHI
{
	FBlendStateInitializerRHI()
	{}

	FBlendStateInitializerRHI(
		EBlendOperation InColorBlendOperation,
		EBlendFactor InColorSourceBlendFactor,
		EBlendFactor InColorDestBlendFactor,
		EBlendOperation InAlphaBlendOperation,
		EBlendFactor InAlphaSourceBlendFactor,
		EBlendFactor InAlphaDestBlendFactor,
		ECompareFunction InAlphaTest,
		BYTE InAlphaRef,
		FLinearColor InConstantBlendColor = FLinearColor::Black)
		:
		ColorBlendOperation(InColorBlendOperation),
		ColorSourceBlendFactor(InColorSourceBlendFactor),
		ColorDestBlendFactor(InColorDestBlendFactor),
		AlphaBlendOperation(InAlphaBlendOperation),
		AlphaSourceBlendFactor(InAlphaSourceBlendFactor),
		AlphaDestBlendFactor(InAlphaDestBlendFactor),
		AlphaTest(InAlphaTest),
		AlphaRef(InAlphaRef),
		ConstantBlendColor(InConstantBlendColor)
	{}

	EBlendOperation ColorBlendOperation;
	EBlendFactor ColorSourceBlendFactor;
	EBlendFactor ColorDestBlendFactor;
	EBlendOperation AlphaBlendOperation;
	EBlendFactor AlphaSourceBlendFactor;
	EBlendFactor AlphaDestBlendFactor;
	ECompareFunction AlphaTest;
	BYTE AlphaRef;
	// Only implemented for ES2
	FLinearColor ConstantBlendColor;
};


class TMRTStaticBlendState
{
public:
	TMRTStaticBlendState(
		EBlendOperation InColorBlendOp = BO_Add,
		EBlendFactor InColorSrcBlend = BF_One,
		EBlendFactor InColorDestBlend = BF_Zero,
		EBlendOperation InAlphaBlendOp = BO_Add,
		EBlendFactor InAlphaSrcBlend = BF_One,
		EBlendFactor InAlphaDestBlend = BF_Zero)
		:ColorBlendOp(InColorBlendOp), 
		ColorSrcBlend(InColorSrcBlend),
		ColorDestBlend(InColorDestBlend),
		AlphaBlendOp(InAlphaBlendOp),
		AlphaSrcBlend(InAlphaSrcBlend),
		AlphaDestBlend(InAlphaDestBlend)
	{
	}

	EBlendOperation ColorBlendOp;
	EBlendFactor ColorSrcBlend;
	EBlendFactor ColorDestBlend;
	EBlendOperation AlphaBlendOp;
	EBlendFactor AlphaSrcBlend;
	EBlendFactor AlphaDestBlend;
};

class FMRTBlendStateInitializerRHI
{
public:
	FMRTBlendStateInitializerRHI(
		const TMRTStaticBlendState& a = TMRTStaticBlendState(),
		const TMRTStaticBlendState& b = TMRTStaticBlendState(),
		const TMRTStaticBlendState& c = TMRTStaticBlendState(),
		const TMRTStaticBlendState& d = TMRTStaticBlendState(),
		const TMRTStaticBlendState& e = TMRTStaticBlendState(),
		const TMRTStaticBlendState& f = TMRTStaticBlendState(),
		const TMRTStaticBlendState& g = TMRTStaticBlendState(),
		const TMRTStaticBlendState& h = TMRTStaticBlendState())
		:AlphaToCoverageEnable(FALSE) 
	{
		MRTBlendState[0] = a;
		MRTBlendState[1] = b;
		MRTBlendState[2] = c;
		MRTBlendState[3] = d;
		MRTBlendState[4] = e;
		MRTBlendState[5] = f;
		MRTBlendState[6] = g;
		MRTBlendState[7] = h;
	}

	// [MRT id]
	TMRTStaticBlendState	MRTBlendState[8];
	//
	UBOOL					AlphaToCoverageEnable;
};

enum EPrimitiveType
{
	PT_TriangleList,
	PT_TriangleStrip,
	PT_LineList,
	PT_QuadList,
	PT_TessellatedQuadPatchXbox,
	PT_PointSprite,

#if WITH_D3D11_TESSELLATION
	PT_1_ControlPointPatchList,
	PT_2_ControlPointPatchList,
	PT_3_ControlPointPatchList,
	PT_4_ControlPointPatchList,
	PT_5_ControlPointPatchList,
	PT_6_ControlPointPatchList,
	PT_7_ControlPointPatchList,
	PT_8_ControlPointPatchList,
	PT_9_ControlPointPatchList,
	PT_10_ControlPointPatchList,
	PT_11_ControlPointPatchList,
	PT_12_ControlPointPatchList,
	PT_13_ControlPointPatchList,
	PT_14_ControlPointPatchList,
	PT_15_ControlPointPatchList,
	PT_16_ControlPointPatchList,
	PT_17_ControlPointPatchList,
	PT_18_ControlPointPatchList,
	PT_19_ControlPointPatchList,
	PT_20_ControlPointPatchList,
	PT_21_ControlPointPatchList,
	PT_22_ControlPointPatchList,
	PT_23_ControlPointPatchList,
	PT_24_ControlPointPatchList,
	PT_25_ControlPointPatchList,
	PT_26_ControlPointPatchList,
	PT_27_ControlPointPatchList,
	PT_28_ControlPointPatchList,
	PT_29_ControlPointPatchList,
	PT_30_ControlPointPatchList,
	PT_31_ControlPointPatchList,
	PT_32_ControlPointPatchList,
#endif // #if WITH_D3D11_TESSELLATION

	PT_Num,

#if WITH_D3D11_TESSELLATION
	PT_NumBits = 6
#else // #if WITH_D3D11_TESSELLATION
	PT_NumBits = 3
#endif // #if WITH_D3D11_TESSELLATION
};

checkAtCompileTime( PT_Num <= (1 << 8), EPrimitiveType_DoesntFitInAByte );
checkAtCompileTime( PT_Num <= (1 << PT_NumBits), PT_NumBits_TooSmall );

enum EParticleEmitterType
{
	PET_None = 0,			// Not a particle emitter
	PET_Sprite = 1,			// Sprite particle emitter
	PET_SubUV = 2,			// SubUV particle emitter
	PET_Mesh = 3,			// Mesh emitter
	PET_PointSprite = 4,	// Point sprites (mobile only)
	PET_PresuppliedMemory = 5, // Particle where the render data is ready to go, and it's triple buffered, so no memcpy is needed

	PET_NumBits = 3
};

// Pixel shader constants that are reserved by the Engine.
enum EPixelShaderRegisters
{
	PSR_ColorBiasFactor = 0,			// Factor applied to the color output from the pixelshader
	PSR_ScreenPositionScaleBias = 1,	// Converts projection-space XY coordinates to texture-space UV coordinates
	PSR_MinZ_MaxZ_Ratio = 2,			// Converts device Z values to clip-space W values
	PSR_NvStereoEnabled = 3,			// Whether stereo is enabled on the current rendering device.
	PSR_DiffuseOverride = 4,			// Overrides GetMaterialDiffuseColor for visualization
	PSR_SpecularOverride = 5,			// Overrides GetMaterialSpecularColor for visualization
	PSR_ViewOrigin = 6,					// World space position of the view's origin (camera position)
	PSR_ScreenAndTexelSize = 7,			// Size of the screen in pixels (1280, 720), and 1/BufferSize (size of a texel in the render targets)
#if WITH_REALD
	PSR_RealDCoefficients1      = 8,            // Depth Buffer Allocation vars 1
#endif
	PSR_MaxPixelShaderRegister
};

// Vertex shader constants that are reserved by the Engine.
enum EVertexShaderRegister
{
	VSR_ViewProjMatrix = 0,		// View-projection matrix, transforming from World space to Projection space
	VSR_ViewOrigin = 4,			// World space position of the view's origin (camera position)
	VSR_PreViewTranslation = 5,
#if WITH_REALD
	VSR_RealDCoefficients1      = 6,    // Depth Buffer Allocation vars 1
	VSR_MaxVertexShaderRegister = 7
#else
	VSR_MaxVertexShaderRegister = 6
#endif
};

/**
 *	Resource usage flags - for vertex and index buffers.
 */
enum EResourceUsageFlags
{
	// Mutually exclusive flags
	RUF_Static		= 0x01,		// The resource will be created, filled, and never repacked.
	RUF_Dynamic		= 0x02,		// The resource will be repacked in-frequently.
	RUF_Volatile	= 0x04,		// The resource will be repacked EVERY frame.
    RUF_SmallUpdate = 0x08,     // During map only part of the buffer will be updated.

	// The following flags can be OR:d in
	RUF_WriteOnly	= 0x80,		// The resource data will never be read from.

	// Helper bit-masks
	RUF_AnyDynamic	= (RUF_Dynamic|RUF_Volatile),
};

/**
 *	Tessellation mode, to be used with RHISetTessellationMode.
 */
enum ETessellationMode
{
	TESS_Discrete = 0,
	TESS_Continuous = 1,
	TESS_PerEdge = 2,
};

/**
 *	Screen Resolution
 */
struct FScreenResolutionRHI
{
	DWORD	Width;
	DWORD	Height;
	DWORD	RefreshRate;
};

/**
 *	Viewport bounds structure to set multiple view ports for the geometry shader
 *  (needs to be 1:1 to the D3D11 structure)
 */
struct FViewPortBounds
{
	FLOAT	TopLeftX;
	FLOAT	TopLeftY;
	FLOAT	Width;
	FLOAT	Height;
	FLOAT	MinDepth;
	FLOAT	MaxDepth;

	FViewPortBounds(FLOAT InTopLeftX, FLOAT InTopLeftY, FLOAT InWidth, FLOAT InHeight, FLOAT InMinDepth = 0.0f, FLOAT InMaxDepth = 1.0f)
		:TopLeftX(InTopLeftX), TopLeftY(InTopLeftY), Width(InWidth), Height(InHeight), MinDepth(InMinDepth), MaxDepth(InMaxDepth)
	{
	}
};


typedef TArray<FScreenResolutionRHI>	FScreenResolutionArray;

#define ENUM_RHI_RESOURCE_TYPES(EnumerationMacro) \
	EnumerationMacro(SamplerState,None) \
	EnumerationMacro(RasterizerState,None) \
	EnumerationMacro(DepthState,None) \
	EnumerationMacro(StencilState,None) \
	EnumerationMacro(BlendState,None) \
	EnumerationMacro(VertexDeclaration,None) \
	EnumerationMacro(VertexShader,None) \
	EnumerationMacro(HullShader,None) \
	EnumerationMacro(DomainShader,None) \
	EnumerationMacro(PixelShader,None) \
	EnumerationMacro(GeometryShader,None) \
	EnumerationMacro(ComputeShader,None) \
	EnumerationMacro(BoundShaderState,None) \
	EnumerationMacro(IndexBuffer,None) \
	EnumerationMacro(VertexBuffer,None) \
	EnumerationMacro(Surface,None) \
	EnumerationMacro(Texture,None) \
	EnumerationMacro(Texture2D,Texture) \
	EnumerationMacro(Texture2DArray,Texture) \
	EnumerationMacro(Texture3D,Texture) \
	EnumerationMacro(TextureCube,Texture) \
	EnumerationMacro(SharedTexture2D,Texture2D) \
	EnumerationMacro(SharedTexture2DArray,Texture2DArray) \
	EnumerationMacro(SharedMemoryResource,None) \
	EnumerationMacro(OcclusionQuery,None) \
	EnumerationMacro(Viewport,None)

/** An enumeration of the different RHI reference types. */
enum ERHIResourceTypes
{
	RRT_None,

#define DECLARE_RESOURCETYPE_ENUM(Type,ParentType) RRT_##Type,
	ENUM_RHI_RESOURCE_TYPES(DECLARE_RESOURCETYPE_ENUM)
#undef DECLARE_RESOURCETYPE_ENUM
};

/** Flags used for texture creation */
enum ETextureCreateFlags
{
	// Texture is encoded in sRGB gamma space
	TexCreate_SRGB					= 1<<0,
	// Texture can be used as a resolve target (normally not stored in the texture pool)
	TexCreate_ResolveTargetable		= 1<<1,
	// Texture is a depth stencil format that can be sampled
	TexCreate_DepthStencil			= 1<<2,
	// Texture will be created without a packed miptail
	TexCreate_NoMipTail				= 1<<3,
	// Texture will be created with an un-tiled format
	TexCreate_NoTiling				= 1<<4,
	// Texture that for a resolve target will only be written to/resolved once
	TexCreate_WriteOnce				= 1<<5,
	// Texture that may be updated every frame
	TexCreate_Dynamic				= 1<<6,
	// Texture that didn't go through the offline cooker (normally not stored in the texture pool)
	TexCreate_Uncooked				= 1<<7,
	// Allow silent texture creation failure
	TexCreate_AllowFailure			= 1<<8,
	// Disable automatic defragmentation if the initial texture memory allocation fails.
	TexCreate_DisableAutoDefrag		= 1<<9,
	// Create the texture with automatic -1..1 biasing
	TexCreate_BiasNormalMap			= 1<<10,
	// Create the texture with the flag that allows mip generation later, only applicable to D3D11
	TexCreate_GenerateMipCapable	= 1<<11,
	// A resolve textures that can be presented to screen
	TexCreate_Presentable			= 1<<12,
	// Texture is used as a resolvetarget for a multisampled surface. (Required for multisampled depth textures)
	TexCreate_Multisample			= 1<<13,
	// Texture should disable any filtering (NGP only, and is hopefully temporary)
	TexCreate_PointFilterNGP		= 1<<14,
	// This is a targetable resolve texture for a TargetSurfCreate_HighPerf, so should be fast to read if possible
	TexCreate_HighPerf				= 1<<15,
	// Texture has been created with an explicit address.
	TexCreate_ExplicitAddress		= 1<<16,
};

/** Flags used for targetable surface creation */
enum ETargetableSurfaceCreateFlags
{
	TargetSurfCreate_None			= 0,
	// Without this the surface may simply be an alias for the texture's memory. Note that you must still resolve the surface.
    TargetSurfCreate_Dedicated		= 1<<0,
	// Surface must support being read from by RHIReadSurfaceData.
	TargetSurfCreate_Readable		= 1<<1,
	// Surface will be only written to one time, in case that helps the platform
	TargetSurfCreate_WriteOnce		= 1<<2,
	// Surface will be created as multisampled.  This implies TargetSurfCreate_Dedicated, unless multisampling is disabled.
	TargetSurfCreate_Multisample	= 1<<3,
	// Create the surface with the flag that allows mip generation later, only applicable to D3D11
	TargetSurfCreate_GenerateMipCapable	= 1<<4,
	// A render target that can be presented to screen
	TargetSurfCreate_Presentable	= 1<<5,
	// UnorderedAccessView (DX11 only)
	TargetSurfCreate_UAV			= 1<<6,
	// This surface needs to be as high performance as possible
	TargetSurfCreate_HighPerf		= 1<< 7,
	// SCALEFORM+ES2 - Tells the RHI to create a depth buffer that matches the back buffer (MSAA settings)
	TargetSurfCreate_DepthBufferToMatchBackBuffer = 1<< 8,
};

/**
 * GPU memory type
 *
 * Note that allocations from GPUMem_TexturePool may fail due to OOM, but it will try a full defragmentation pass if needed.
 * When allocated from the gamethread, the user can call GStreamingManager->StreamOutTextureData() to make room.
 */
enum EGPUMemoryType
{
	GPUMem_System = 0,
	GPUMem_TexturePool,
	GPUMem_MAX
};

/**
 * Settings that determines whether SceneDepth are looked up normally, or specially for projected shadow passes.
 * PS3 reads SceneDepth straight from the depth buffer during shadow passes.
 */
enum ESceneDepthUsage
{
	SceneDepthUsage_Normal = 0,
	SceneDepthUsage_ProjectedShadows,
};

// Forward-declaration.
struct FResolveParams;

/**
 * Async texture reallocation status, returned by RHIGetReallocateTexture2DStatus().
 */
enum ETextureReallocationStatus
{
	TexRealloc_Succeeded = 0,
	TexRealloc_Failed,
	TexRealloc_InProgress,
};

//
// Platform-specific RHI types.
//	
#if !CONSOLE || USE_NULL_RHI
	// Use dynamically bound RHIs on PCs and when using the null RHI.
	#define USE_DYNAMIC_RHI 1
	#define WITH_ES2_RHI USE_DYNAMIC_ES2_RHI
	#include "DynamicRHI.h"
#elif XBOX
	// Use the statically bound XeD3D RHI on Xenon
	// #define a wrapper define that will make XeD3D files still compile even when using Null RHI
	#define USE_XeD3D_RHI 1
	#include "XeD3DDrv.h"
#elif NGP
	// Use statically bound PS3 RHI
	#define USE_GXM_RHI 1
	#include "GXMRHI.h"
#elif PS3
	// Use statically bound PS3 RHI
    #define USE_PS3_RHI 1
	#include "PS3RHI.h"
#elif USE_STATIC_ES2_RHI
	// Use statically bound OpenGL ES 2.0 RHI
	#define USE_STATIC_RHI 1
	#define STATIC_RHI_NAME ES2
	#define WITH_ES2_RHI 1
	#include "StaticRHI.h"
	#include "ES2RHI.h"
#elif WIIU
	// statically bound GX2 RHI
	#define USE_GX2_RHI 1
	#include "GX2RHI.h"
#else
	// Fall back to the null RHI
	#undef USE_NULL_RHI
	#define USE_NULL_RHI 1
	#define USE_DYNAMIC_RHI 1
	#include "DynamicRHI.h"
#endif

// do we have support for some mobile RHI compiled in?
#define WITH_MOBILE_RHI (WITH_ES2_RHI || USE_GXM_RHI)

// Vertex/index buffers and textures are in directly accessible memory
#if PS3 || XBOX || NGP || WIIU
#define RHI_UNIFIED_MEMORY 1
#else
#define RHI_UNIFIED_MEMORY 0
#endif

struct FResolveRect
{
	INT X1;
	INT Y1;
	INT X2;
	INT Y2;
	// e.g. for a a full 256 x 256 area starting at (0, 0) it would be 
	// the values would be 0, 0, 256, 256
	FResolveRect(INT InX1=-1, INT InY1=-1, INT InX2=-1, INT InY2=-1)
	:	X1(InX1)
	,	Y1(InY1)
	,	X2(InX2)
	,	Y2(InY2)
	{}
};

struct FResolveParams
{
	/** used to specify face when resolving to a cube map texture */
	ECubeFace CubeFace;
	/** resolve RECT bounded by [X1,Y1]..[X2,Y2]. Or -1 for fullscreen */
	FResolveRect Rect;
	/** Texture to resolve to. If NULL, it will resolve to the texture associated with the rendertarget. */
	FTexture2DRHIParamRef ResolveTarget;

#if XBOX
	/** Texture to resolve to. If NULL, it will resolve to the texture associated with the rendertarget. */
	FTexture2DArrayRHIParamRef ArrayTarget;

	FResolveParams(FTexture2DArrayRHIParamRef InTarget, UINT InSlice, const FResolveRect& InRect = FResolveRect())
		:	CubeFace((ECubeFace)InSlice)
		,	Rect(InRect)
		,	ResolveTarget(0)
		,	ArrayTarget(InTarget)
	{}
#endif // XBOX

	/** constructor */
	FResolveParams(const FResolveRect& InRect = FResolveRect(), ECubeFace InCubeFace=CubeFace_PosX, FTexture2DRHIParamRef InResolveTarget=NULL)
		:	CubeFace(InCubeFace)
		,	Rect(InRect)
		,	ResolveTarget(InResolveTarget)
#if XBOX
		,	ArrayTarget(0)
#endif // XBOX
	{}

	/** Constructor. */
	FResolveParams( FTexture2DRHIParamRef InResolveTarget )
		:	CubeFace(CubeFace_PosX)
		,	ResolveTarget(InResolveTarget)
	{}
};

/** specifies a texture and region to copy */
struct FCopyTextureRegion2D
{
	/** source texture to lock and copy from */
	FTexture2DRHIParamRef SrcTexture;
	/** Actual source texture (not RHI ref) */
	void* SrcTextureObject;
	/** horizontal texel offset for copy region */
	INT OffsetX;
	/** vertical texel offset for copy region */
	INT OffsetY;
	/** horizontal texel offset for copy region */
	INT DestOffsetX;
	/** vertical texel offset for copy region */
	INT DestOffsetY;
	/** horizontal texel size for copy region */
	INT SizeX;
	/** vertical texel size for copy region */
	INT SizeY;	
	/** Starting mip index. This is treated as the base level (default is 0) */
	INT FirstMipIdx;
	/** constructor */
	FCopyTextureRegion2D(FTexture2DRHIParamRef InSrcTextureRef,void* InSrcTextureObject, INT InOffsetX=-1,INT InOffsetY=-1,INT InSizeX=-1,INT InSizeY=-1,INT InFirstMipIdx=0,INT InDestOffsetX=-1,INT InDestOffsetY=-1)
		:	SrcTexture(InSrcTextureRef)
		,	SrcTextureObject (InSrcTextureObject)
		,	OffsetX(InOffsetX)
		,	OffsetY(InOffsetY)
		,	DestOffsetX(InDestOffsetX)
		,	DestOffsetY(InDestOffsetY)
		,	SizeX(InSizeX)
		,	SizeY(InSizeY)
		,	FirstMipIdx(InFirstMipIdx)
	{}
};

/** specifies an update region for a texture */
struct FUpdateTextureRegion2D
{
	/** offset in texture */
	INT DestX;
	INT DestY;
	
	/** offset in source image data */
	INT SrcX;
	INT SrcY;
	
	/** size of region to copy */
	INT Width;
	INT Height;

	FUpdateTextureRegion2D()
	{}

	FUpdateTextureRegion2D(INT InDestX, INT InDestY, INT InSrcX, INT InSrcY, INT InWidth, INT InHeight)
	:	DestX(InDestX)
	,	DestY(InDestY)
	,	SrcX(InSrcX)
	,	SrcY(InSrcY)
	,	Width(InWidth)
	,	Height(InHeight)
	{}
};

/** The parameters for 4 layers of height-fog. */
struct FHeightFogParams
{
	TStaticArray<FLOAT,4> FogMinHeight;
	TStaticArray<FLOAT,4> FogMaxHeight;
	TStaticArray<FLOAT,4> FogDistanceScale;
	TStaticArray<FLOAT,4> FogExtinctionDistance;
	TStaticArray<FLinearColor,4> FogInScattering;
	TStaticArray<FLOAT,4> FogStartDistance;
};


// Define the statically bound RHI methods with the RHI name prefix.
#define DEFINE_RHIMETHOD(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) extern Type RHI##Name ParameterTypesAndNames
#include "RHIMethods.h"
#undef DEFINE_RHIMETHOD

/** Initializes the RHI. */
extern void RHIInit( UBOOL bIsEditor );

/** Shuts down the RHI. */
extern void RHIExit();

/**
 * The scene rendering stats.
 */
enum ESceneRenderingStats
{
	STAT_BeginOcclusionTestsTime = STAT_SceneRenderingFirstStat,
	STAT_OcclusionResultTime,
	STAT_InitViewsTime,
	STAT_DynamicShadowSetupTime,
	STAT_TotalGPUFrameTime,
	STAT_TotalSceneRenderingTime,
	STAT_SceneCaptureRenderingTime,
	STAT_SceneCaptureInitViewsTime,
	STAT_SceneCaptureBasePassTime,
	STAT_SceneCaptureMiscTime,
	STAT_DepthDrawTime,
	STAT_BasePassDrawTime,
	STAT_LightingDrawTime,
	STAT_ProjectedShadowDrawTime,
	STAT_TranslucencyDrawTime,
	STAT_VelocityDrawTime,
	STAT_DynamicPrimitiveDrawTime,
	STAT_StaticDrawListDrawTime,
	STAT_SoftMaskedDrawTime,
	STAT_UnaccountedSceneRenderingTime,
	STAT_UnlitDecalDrawTime,
	STAT_PostProcessDrawTime,
	STAT_SceneLights,
	STAT_MeshDrawCalls,
	STAT_DynamicPathMeshDrawCalls,
	STAT_StaticDrawListMeshDrawCalls,
	STAT_PresentTime,
};

enum EInitViewsStats
{
	STAT_InitViewsTime2 = STAT_InitViewsFirstStat,
	STAT_InitDynamicShadowsTime,
	STAT_GatherShadowPrimitivesTime,
	STAT_PerformViewFrustumCullingTime,
	STAT_OcclusionResultTime2,
	STAT_ProcessVisibleTime,
	STAT_DecompressPrecomputedOcclusion,
	STAT_ProcessedPrimitives,
	STAT_CulledPrimitives,
	STAT_OccludedPrimitives,
	STAT_StaticallyOccludedPrimitives,
	STAT_LODDroppedPrimitives,
	STAT_MinDrawDroppedPrimitives,
	STAT_MaxDrawDroppedPrimitives,
	STAT_OcclusionQueries,
	STAT_VisibleStaticMeshElements,
	STAT_VisibleDynamicPrimitives,
};

enum EShadowRenderingStats
{
	STAT_ShadowRendering = STAT_ShadowRenderingFirstStat,
	STAT_RenderPerObjectShadowProjectionsTime,
	STAT_RenderPerObjectShadowDepthsTime,
	STAT_RenderWholeSceneShadowProjectionsTime,
	STAT_RenderWholeSceneShadowDepthsTime,
	STAT_RenderModulatedShadowsTime,
	STAT_RenderNormalShadowsTime,
	STAT_PerObjectShadows,
	STAT_PreShadows,
	STAT_TranslucentPreShadows,
	STAT_CachedPreShadows,
	STAT_WholeSceneShadows,
	STAT_ShadowCastingDominantLights,
};

/** The scene update stats. */
enum
{
	STAT_AddScenePrimitiveRenderThreadTime = STAT_SceneUpdateFirstStat,
	STAT_AddSceneLightTime,
	STAT_RemoveScenePrimitiveTime,
	STAT_RemoveSceneLightTime,
	STAT_UpdateSceneLightTime,
	STAT_UpdatePrimitiveTransformRenderThreadTime,
	STAT_GPUSkinUpdateRTTime,
	STAT_ParticleUpdateRTTime,
	STAT_InfluenceWeightsUpdateRTTime,
	STAT_TotalRTUpdateTime,
};

/** Memory stats for tracking virtual allocations used by the renderer to represent the scene. */
enum ESceneMemoryStats
{
	STAT_StaticDrawListMemory = STAT_SceneMemoryFirstStat,
	STAT_PrimitiveInfoMemory,
	STAT_RenderingSceneMemory,
	STAT_ViewStateMemory,
	STAT_RenderingMemStackMemory,
	STAT_LightInteractionMemory
};

/**
 * The scene rendering stats.
 */
enum ELightsStats
{
	STAT_NumLightShafts = STAT_LightsFirstStat,
	STAT_NumSceneLights,
};

/**
 * The tracked primitimve rendering mode (debugging only)
 */
enum ETrackedPrimitiveMode
{
	TPM_Isolate,
	TPM_To,
	TPM_From,

	TPM_Count
};

// the following helper macros allow to safely convert shader types without much code clutter
#define GETSAFERHISHADER_PIXEL(Shader) (Shader ? Shader->GetPixelShader() : NULL)
#define GETSAFERHISHADER_VERTEX(Shader) (Shader ? Shader->GetVertexShader() : NULL)
#define GETSAFERHISHADER_HULL(Shader) (Shader ? Shader->GetHullShader() : NULL)
#define GETSAFERHISHADER_DOMAIN(Shader) (Shader ? Shader->GetDomainShader() : NULL)

#if WITH_D3D11_TESSELLATION
	#define GETSAFERHISHADER_GEOMETRY(Shader) (Shader ? Shader->GetGeometryShader() : NULL)
	#define GETSAFERHISHADER_COMPUTE(Shader) (Shader ? Shader->GetComputeShader() : NULL)
#else // WITH_D3D11_TESSELLATION
	#define GETSAFERHISHADER_GEOMETRY(Shader) (0)
	#define GETSAFERHISHADER_COMPUTE(Shader) (0)
#endif // WITH_D3D11_TESSELLATION


#endif // !__RHI_H__

