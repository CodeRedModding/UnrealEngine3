/*=============================================================================
	OpenGLViewport.h: OpenGL viewport RHI definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

class FOpenGLViewport : public FRefCountedObject, public TDynamicRHIResource<RRT_Viewport>
{
public:

	FOpenGLViewport(class FOpenGLDynamicRHI* InOpenGLRHI,void* InWindowHandle,UINT InSizeX,UINT InSizeY,UBOOL bInIsFullscreen);
	~FOpenGLViewport();

	void InitRenderThreadContext();
	void DestroyRenderThreadContext();

	void Resize(UINT InSizeX,UINT InSizeY,UBOOL bInIsFullscreen);

	void SwapBuffers();

	// Accessors.
	UINT GetSizeX() const { return SizeX; }
	UINT GetSizeY() const { return SizeY; }
	FOpenGLSurface *GetBackBuffer() const { return BackBuffer; }
	FOpenGLTexture2D *GetBackBufferTexture() const { return BackBufferTexture; }

	FOpenGLSurface *CreateBackBufferSurface(FOpenGLDynamicRHI *RHI);

	UBOOL IsFullscreen( void ) const { return bIsFullscreen; }

private:

	FOpenGLDynamicRHI* OpenGLRHI;
#if _WINDOWS
	HWND WindowHandle;
	HDC GLContext;
	HGLRC GLRenderContext;
	HGLRC RenderThreadGLContext;
#elif PLATFORM_MACOSX
	void *OpenGLView;
	void *FullscreenOpenGLView;
	void *RenderThreadGLContext;
#endif
	UINT SizeX;
	UINT SizeY;
	UBOOL bIsFullscreen;
	UBOOL bIsValid;
	TRefCountPtr<FOpenGLSurface> BackBuffer;
#if !OPENGL_USE_BLIT_FOR_BACK_BUFFER
	TRefCountPtr<FOpenGLTexture2D> BackBufferTexture;
#endif
};
