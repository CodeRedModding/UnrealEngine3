/*=============================================================================
	ES2RHI.h: OpenGL ES 2.0 RHI definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/


#ifndef __ES2RHI_H__
#define __ES2RHI_H__

#if _WINDOWS
	#include "OpenGLWindowsLoader.h"

	// @TODO: move these into the loader code in a clean way
	#define GL_DEPTH_STENCIL_OES			0x84F9
	#define GL_UNSIGNED_INT_24_8_OES		0x84FA
	#define GL_WRITE_ONLY_OES				0x88B9
	#define GL_DEPTH24_STENCIL8_OES			0x88F0

	#include <GL/wglext.h>
	#include <WinGDI.h>
	#define WITH_WGL 1
#elif IPHONE
	#include <OpenGLES/ES2/gl.h>
	#include <OpenGLES/ES2/glext.h>
#elif ANDROID || FLASH
    #if ANDROID
        #define GL_GLEXT_PROTOTYPES
    #endif
	#include <GLES2/gl2.h>
	#include <GLES2/gl2ext.h>
	#include <GLES2/gl2platform.h>
#endif

#include "ES2RHIPublicTypes.h"

#if WITH_ES2_RHI

// Platform specific globals
extern INT GScreenWidth;
extern INT GScreenHeight;

/** Allow platforms to override the GL name for the on-screen render buffer (i.e. iPhone which makes one for a EAGLView) */ 

#if IPHONE || ANDROID || FLASH
	/** Controls for allowing and enabling MSAA */
	extern UBOOL GMSAAAllowed;
	extern UBOOL GMSAAEnabled;
	extern UBOOL GMSAAToggleRequest;
#endif

/** 
 * Whether we're currently rendering a depth only pass. 
 * This should be replaced with a pass specific shader once the ES2 RHI supports RHISetBoundShaderState properly instead of keying off of vertex declaration.
 */
extern UBOOL GMobileRenderingDepthOnly;

/** 
 * Whether we're currently rendering a shadow (linear) depth to a shadow buffer. 
 * This should be replaced with a pass specific shader once the ES2 RHI supports RHISetBoundShaderState properly instead of keying off of vertex declaration.
 */
extern UBOOL GMobileRenderingShadowDepth;

/** 
 * Whether we're currently rendering a forward shadow projection.
 * This should be replaced with a pass specific shader once the ES2 RHI supports RHISetBoundShaderState properly instead of keying off of vertex declaration.
 */
extern UBOOL GMobileRenderingForwardShadowProjections;

extern UBOOL GES2MapBuffer;

/** 
 *	Some devices (i.e. Adreno 205) crash if shaders use the discard instruction
 */
extern UBOOL GMobileAllowShaderdiscard;

/*
 *	NVIDIA's nonlinear-depth extension for 16-bit depth only devices
 */
extern UBOOL GSupports16BitNonLinearDepth;

#if ANDROID
	// The total device memory in bytes
	extern UINT GAndroidDeviceMemory;
#endif

// Name of GPU Vendor and Renderer returned by OpenGL
extern FString GGraphicsVendor;
extern FString GGraphicsRenderer;

/**
 * Checks the OpenGL extensions and sets up global variables according to what the device supports,
 * e.g. GTextureFormatSupport and GSupportsDepthTextures.
 */
void CheckOpenGLExtensions();

/**
 * A control for profiling draw calls after shader compiles
 */
STAT(extern UBOOL GES2TimeNextDrawCall);

// public #defines that control formats and rendering passes

// On console we'll fully disallow 32-bit index buffers for ES2.  Otherwise, these index buffers will simply
// be converted at load time
#if CONSOLE
	// 16-bit indices only
	#define DISALLOW_32BIT_INDICES 1
#endif


/**
 * Container struct for information about GL specific texture formats based on UE3 formats
 */
struct FES2PixelFormat
{
	GLenum InternalFormat;	// Parameter 3 to glTexImage2D()
	GLenum Format;			// Parameter 7 to glTexImage2D()
	GLenum Type;			// Parameter 8 to glTexImage2D()
	UBOOL bIsCompressed;	// use glCompressedTexImage2D() instead of glTexImage2D()?

	void Setup( GLenum InInternalFormat, GLenum InFormat, GLenum InType, UBOOL bInIsCompressed )
	{
		InternalFormat = InInternalFormat;
		Format = InFormat;
		Type = InType;
		bIsCompressed = bInIsCompressed;
	}
};

/** Mirror the unreal pixel types to extended GL format information. */
extern FES2PixelFormat GES2PixelFormats[];

/**
 * Engine stats
 */
enum EES2Stats
{
	STAT_DrawCalls = STAT_ES2FirstStat,
	STAT_DrawCallsUP,
	STAT_ES2InitShaderCacheTime,
	STAT_ES2InitShaderCacheCompileTime,
	STAT_ES2InitShaderCacheDrawCallTime,
	STAT_ES2ShaderCacheVSAvoided,
	STAT_ES2ShaderCachePSAvoided,
	STAT_ES2ShaderCacheProgramsAvoided,
	STAT_ES2ShaderCacheCompileTime,
	STAT_ES2ShaderCache1stDrawTime,
	STAT_ES2VertexProgramCompileTime,
	STAT_ES2PixelProgramCompileTime,
	STAT_ES2ProgramLinkTime,
	STAT_PrimitivesDrawn,
	STAT_PrimitivesDrawnUP,
	STAT_NumInvalidMeshes,
	STAT_ShaderProgramCount,
	STAT_ShaderProgramCountPP,
	STAT_ShaderProgramChanges,
	STAT_ShaderUniformUpdates,
	STAT_BaseTextureBinds,
	STAT_DetailTextureBinds,
	STAT_Detail2TextureBinds,
	STAT_Detail3TextureBinds,
	STAT_LightmapTextureBinds,
	STAT_NormalTextureBinds,
	STAT_EnvironmentTextureBinds,
	STAT_MaskTextureBinds,
	STAT_EmissiveTextureBinds,
};


/** This is a macro that casts a dynamically bound RHI reference to the appropriate type. */
#define DYNAMIC_CAST_ES2RESOURCE(Type,Name) \
	FES2##Type* Name = (FES2##Type*)Name##RHI;

/** Casts a generic RHIRef resource to an ES2-specific resource */
#define ES2CAST( Type, Var ) ((Type*)&( *Var ))


#if USE_STATIC_RHI

/** Extends the static RHI with overrides for reference counting */
class FES2RHIExt
	: public FES2RHI
{
public:

	/** ES2RHI destructor, called before shut down */
	virtual ~FES2RHIExt();

	// Reference counting API for the different resource types.
	#define IMPLEMENT_DYNAMICRHI_REFCOUNTING_FORTYPE(Type,ParentType) \
		virtual void AddResourceRef(TES2RHIResource<RRT_##Type>* ReferenceRHI) \
		{ \
			DYNAMIC_CAST_ES2RESOURCE(Type,Reference); \
			Reference->AddRef(); \
		} \
		virtual void RemoveResourceRef(TES2RHIResource<RRT_##Type>* ReferenceRHI) \
		{ \
			DYNAMIC_CAST_ES2RESOURCE(Type,Reference); \
			Reference->Release(); \
		} \
		virtual DWORD GetRefCount(TES2RHIResource<RRT_##Type>* ReferenceRHI) \
		{ \
			DYNAMIC_CAST_ES2RESOURCE(Type,Reference); \
			Reference->AddRef(); \
			return Reference->Release(); \
		}

	ENUM_RHI_RESOURCE_TYPES(IMPLEMENT_DYNAMICRHI_REFCOUNTING_FORTYPE);

	#undef IMPLEMENT_DYNAMICRHI_REFCOUNTING_FORTYPE

};

#endif



#if USE_DYNAMIC_RHI

/** The interface which is implemented by the dynamically bound RHI. */
class FES2RHI
	: public FDynamicRHI
{
public:

	/** ES2RHI destructor, called before shut down */
	virtual ~FES2RHI();

	// The RHI methods are defined as virtual functions in URenderHardwareInterface.
	#define DEFINE_RHIMETHOD(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) virtual Type Name ParameterTypesAndNames
	#include "RHIMethods.h"
	#undef DEFINE_RHIMETHOD

	// Reference counting API for the different resource types.
	#define IMPLEMENT_DYNAMICRHI_REFCOUNTING_FORTYPE(Type,ParentType) \
		virtual void AddResourceRef(TDynamicRHIResource<RRT_##Type>* ReferenceRHI) \
		{ \
			DYNAMIC_CAST_ES2RESOURCE(Type,Reference); \
			Reference->AddRef(); \
		} \
		virtual void RemoveResourceRef(TDynamicRHIResource<RRT_##Type>* ReferenceRHI) \
		{ \
			DYNAMIC_CAST_ES2RESOURCE(Type,Reference); \
			Reference->Release(); \
		} \
		virtual DWORD GetRefCount(TDynamicRHIResource<RRT_##Type>* ReferenceRHI) \
		{ \
			DYNAMIC_CAST_ES2RESOURCE(Type,Reference); \
			Reference->AddRef(); \
			return Reference->Release(); \
		}

	ENUM_RHI_RESOURCE_TYPES(IMPLEMENT_DYNAMICRHI_REFCOUNTING_FORTYPE);

	#undef IMPLEMENT_DYNAMICRHI_REFCOUNTING_FORTYPE

};

#endif

#endif

#endif // __ES2RHI_H__

