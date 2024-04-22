/*=============================================================================
	OpenGLDrv.h: Public OpenGL RHI definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_OPENGLDRV
#define _INC_OPENGLDRV

#if _WINDOWS
#include "OpenGLWindowsLoader.h"
#elif PLATFORM_MACOSX
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#endif

#define OPENGL_USE_BINDABLE_UNIFORMS 0
#define OPENGL_USE_BLIT_FOR_BACK_BUFFER 0

/** This is a macro that casts a dynamically bound RHI reference to the appropriate OpenGL type. */
#define DYNAMIC_CAST_OPENGLRESOURCE(Type,Name) \
	FOpenGL##Type* Name = (FOpenGL##Type*)Name##RHI;

// OpenGL RHI public headers.
#include "OpenGLUtil.h"
#include "OpenGLState.h"
#include "OpenGLResources.h"
#include "OpenGLConstantBuffer.h"
#include "OpenGLRenderTarget.h"
#include "OpenGLViewport.h"

#define FOpenGLCachedAttr_Invalid (void*)0xFFFFFFFF
#define FOpenGLCachedAttr_SingleVertex (void*)0xFFFFFFFE

/** The interface which is implemented by the dynamically bound RHI. */
class FOpenGLDynamicRHI : public FDynamicRHI
{
public:

	friend class FOpenGLViewport;

	/** Initialization constructor. */
	FOpenGLDynamicRHI();

	/** Destructor */
	~FOpenGLDynamicRHI();

	void Init();

	// The RHI methods are defined as virtual functions in URenderHardwareInterface.
	#define DEFINE_RHIMETHOD(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) virtual Type Name ParameterTypesAndNames
	#include "RHIMethods.h"
	#undef DEFINE_RHIMETHOD

	// Reference counting API for the different resource types.
	#define IMPLEMENT_DYNAMICRHI_REFCOUNTING_FORTYPE(Type,ParentType) \
		virtual void AddResourceRef(TDynamicRHIResource<RRT_##Type>* ReferenceRHI) \
		{ \
			DYNAMIC_CAST_OPENGLRESOURCE(Type,Reference); \
			Reference->AddRef(); \
		} \
		virtual void RemoveResourceRef(TDynamicRHIResource<RRT_##Type>* ReferenceRHI) \
		{ \
			DYNAMIC_CAST_OPENGLRESOURCE(Type,Reference); \
			Reference->Release(); \
		} \
		virtual DWORD GetRefCount(TDynamicRHIResource<RRT_##Type>* ReferenceRHI) \
		{ \
			DYNAMIC_CAST_OPENGLRESOURCE(Type,Reference); \
			Reference->AddRef(); \
			return Reference->Release(); \
		}

	ENUM_RHI_RESOURCE_TYPES(IMPLEMENT_DYNAMICRHI_REFCOUNTING_FORTYPE);

	#undef IMPLEMENT_DYNAMICRHI_REFCOUNTING_FORTYPE

	void Cleanup();

	void PurgeFramebufferFromCaches(GLuint Framebuffer);
	void OnVertexBufferDeletion(GLuint VertexBufferResource);
	void InvalidateTextureResourceInCache(GLuint Resource);

	FOpenGLStateCache CachedState;
	void CachedSetActiveTexture( GLenum SamplerIndex );

private:

	static const UINT NumBytesPerShaderRegister = sizeof(FLOAT) * 4;

	FOpenGLTexture2D* CreateOpenGLTexture(UINT SizeX,UINT SizeY,UBOOL CubeTexture,BYTE Format,UINT NumMips,DWORD Flags);
	FOpenGLSurface *CreateSurface(UINT SizeX,UINT SizeY,BYTE Format,GLuint Texture,DWORD Flags,GLenum Target);

	/** needs to be called before each draw call */
	void CommitNonComputeShaderConstants();

	/** needs to be called before each dispatch call */
	void CommitComputeShaderConstants();

	void EnableVertexElement(const OpenGLVertexElement &VertexElement, GLsizei Stride, void *Pointer, GLuint Buffer);
	void SetupVertexArrays( UINT BaseVertexIndex );
	void SetupVertexArraysWithData( const void* VertexData, UINT VertexDataStride );
	void BindPendingFramebuffer();

	void UpdateScissorRectOnRenderTargetHeightChange();
	void InternalSetViewport(UINT MinX,UINT MinY,FLOAT MinZ,UINT MaxX,UINT MaxY,FLOAT MaxZ, UBOOL ForceVieportChange);

	/** Initializes the constant buffers.  Called once at RHI initialization time. */
	void InitConstantBuffers();

	TRefCountPtr<FOpenGLViewport> DrawingViewport;
	TRefCountPtr<FOpenGLViewport> DefaultViewport;
	GLuint PendingFramebuffer;
	UBOOL bPendingFramebufferHasRenderTarget;

	struct FOpenGLCachedAttr
	{
		UBOOL Enabled;
		GLuint Buffer;
		GLuint Usage;
		GLuint Size;
		GLenum Type;
		GLboolean bNormalized;
		GLsizei Stride;
		void* Pointer;

		FOpenGLCachedAttr() : Enabled(FALSE), Pointer(FOpenGLCachedAttr_Invalid) {}
	};

	struct FOpenGLStream
	{
		FOpenGLStream()
			:	Stride(0)
            ,   Offset(0)
			,	Frequency(0)
		{}
		FOpenGLVertexBuffer *VertexBuffer;
		UINT Stride;
        UINT Offset;
		UINT Frequency;
	};

	enum { NumVertexStreams = 16 };

	UINT PendingNumInstances;
	FOpenGLStream PendingStreams[NumVertexStreams];
	FOpenGLCachedAttr CachedVertexAttrs[NumVertexStreams];

	TRefCountPtr<FOpenGLBoundShaderState> PendingBoundShaderState;

	TArray<TRefCountPtr<FOpenGLConstantBuffer> > VSConstantBuffers;
	TArray<TRefCountPtr<FOpenGLConstantBuffer> > PSConstantBuffers;

	// Information about pending BeginDraw[Indexed]PrimitiveUP calls.
	UBOOL PendingBegunDrawPrimitiveUP;
	TArray<BYTE> PendingDrawPrimitiveUPVertexData;
	UINT PendingNumVertices;
	UINT PendingVertexDataStride;
	TArray<BYTE> PendingDrawPrimitiveUPIndexData;
	UINT PendingPrimitiveType;
	UINT PendingNumPrimitives;
	UINT PendingMinVertexIndex;
	UINT PendingIndexDataStride;

	/** When a new shader is set, we discard all old constants set for the previous shader. */
	UBOOL bDiscardSharedConstants;
};

extern TArray<FOpenGLDynamicRHI*> FOpenGLDevicesList;

#endif
