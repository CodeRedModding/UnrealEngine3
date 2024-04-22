/*=============================================================================
	OpenGLRenderTarget.h: OpenGL render target RHI definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * A OpenGL surface.
 */
class FOpenGLSurface :
	public FRefCountedObject,
	public TDynamicRHIResource<RRT_Surface>
{
public:

	GLuint Resource;
	GLuint ResolveTargetResource;
	GLenum Type;
	GLenum Attachment;
	BYTE Format;
	UINT SizeX;
	UINT SizeY;
	UINT MultiSampleCount;

	/** Initialization constructor. */
	FOpenGLSurface(
		class FOpenGLDynamicRHI* InOpenGLRHI,
		GLuint InResource,
		GLuint InResolveTargetResource,
		GLenum InType,
		GLenum InAttachment,
		BYTE InFormat,
		UINT InSizeX,
		UINT InSizeY,
		UINT InMultiSampleCount
		)
		:	OpenGLRHI(InOpenGLRHI)
		,	Resource(InResource)
		,	ResolveTargetResource(InResolveTargetResource)
		,	Type(InType)
		,	Attachment(InAttachment)
		,	Format(InFormat)
		,	SizeX(InSizeX)
		,	SizeY(InSizeY)
		,	MultiSampleCount(InMultiSampleCount)
	{}

	virtual ~FOpenGLSurface();

private:

	FOpenGLDynamicRHI* OpenGLRHI;
};

extern GLuint GetOpenGLFramebuffer(FSurfaceRHIParamRef RenderTargetRHI, FSurfaceRHIParamRef DepthStencilTargetRHI);
