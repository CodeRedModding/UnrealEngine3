/*=============================================================================
	OpenGLRenderTarget.cpp: OpenGL render target implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "OpenGLDrvPrivate.h"
#include "ScreenRendering.h"

// gDEBugger is currently very buggy. For example, it cannot show render buffers correctly and doesn't
// know what combined depth/stencil is. This define makes OpenGL render directly to textures and disables
// stencil. It results in broken post process effects, but allows to debug the rendering in gDEBugger.
//#define GDEBUGGER_MODE

/**
* Key used to map a set of unique render/depth stencil target combinations to
* a framebuffer resource
*/
class FOpenGLFramebufferKey
{
public:

	FOpenGLFramebufferKey(
		FSurfaceRHIParamRef InRenderTarget,
		FSurfaceRHIParamRef InDepthStencilTarget
		)
		:	RenderTarget(InRenderTarget)
		,	DepthStencilTarget(InDepthStencilTarget)
	{
	}

	/**
	* Equality is based on render and depth stencil targets 
	* @param Other - instance to compare against
	* @return TRUE if equal
	*/
	friend UBOOL operator ==(const FOpenGLFramebufferKey& A,const FOpenGLFramebufferKey& B)
	{
		return	A.RenderTarget == B.RenderTarget && A.DepthStencilTarget == B.DepthStencilTarget;
	}

	/**
	* Get the hash for this type. 
	* @param Key - struct to hash
	* @return DWORD hash based on type
	*/
	friend DWORD GetTypeHash(const FOpenGLFramebufferKey &Key)
	{
		return GetTypeHash(Key.RenderTarget) ^ GetTypeHash(Key.DepthStencilTarget);
	}

	const FSurfaceRHIParamRef GetRenderTarget( void ) const { return RenderTarget; }
	const FSurfaceRHIParamRef GetDepthStencilTarget( void ) const { return DepthStencilTarget; }

private:

	FSurfaceRHIParamRef RenderTarget;
	FSurfaceRHIParamRef DepthStencilTarget;
};

typedef TMap<FOpenGLFramebufferKey,GLuint> FOpenGLFramebufferCache;

/** Lazily initialized framebuffer cache singleton. */
static FOpenGLFramebufferCache& GetOpenGLFramebufferCache()
{
	static FOpenGLFramebufferCache OpenGLFramebufferCache;
	return OpenGLFramebufferCache;
}

GLuint GetOpenGLFramebuffer(FSurfaceRHIParamRef RenderTargetRHI, FSurfaceRHIParamRef DepthStencilTargetRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Surface,RenderTarget);
	DYNAMIC_CAST_OPENGLRESOURCE(Surface,DepthStencilTarget);

	GLuint Framebuffer = GetOpenGLFramebufferCache().FindRef(FOpenGLFramebufferKey(RenderTargetRHI, DepthStencilTargetRHI));

	if (!Framebuffer && (RenderTarget || DepthStencilTarget))
	{
		glGenFramebuffers(1, &Framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, Framebuffer);
		
		if (RenderTarget)
		{
			if (RenderTarget->Type == GL_TEXTURE_2D)
			{
				glFramebufferTexture2D(GL_FRAMEBUFFER, RenderTarget->Attachment, RenderTarget->Type, RenderTarget->Resource, 0);
			}
			else
			{
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, RenderTarget->Attachment, GL_RENDERBUFFER, RenderTarget->Resource);
			}
		}

		if (DepthStencilTarget)
		{
			if (DepthStencilTarget->Type == GL_TEXTURE_2D)
			{
				glFramebufferTexture2D(GL_FRAMEBUFFER, DepthStencilTarget->Attachment, DepthStencilTarget->Type, DepthStencilTarget->Resource, 0);
			}
			else
			{
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, DepthStencilTarget->Attachment, GL_RENDERBUFFER, DepthStencilTarget->Resource);
			}
		}

		glReadBuffer(RenderTarget ? GL_COLOR_ATTACHMENT0 : GL_NONE);
		glDrawBuffer(RenderTarget ? GL_COLOR_ATTACHMENT0 : GL_NONE);

#if _DEBUG
		GLenum CompleteResult = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (CompleteResult != GL_FRAMEBUFFER_COMPLETE)
		{
			appErrorf(TEXT("Framebbuffer not complete. Status = 0x%x"), CompleteResult);
			return NULL;
		}
#endif

		GetOpenGLFramebufferCache().Set(FOpenGLFramebufferKey(RenderTargetRHI, DepthStencilTargetRHI), Framebuffer);
	}

	return Framebuffer;
}

void ReleaseOpenGLFramebuffersForSurface(FOpenGLDynamicRHI* Device, FSurfaceRHIParamRef SurfaceRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Surface,Surface);

	for (FOpenGLFramebufferCache::TIterator It(GetOpenGLFramebufferCache()); It; ++It)
	{
		FOpenGLFramebufferKey Key = It.Key();
		FSurfaceRHIParamRef RenderTargetRHI = Key.GetRenderTarget();
		FSurfaceRHIParamRef DepthStencilTargetRHI = Key.GetDepthStencilTarget();
		DYNAMIC_CAST_OPENGLRESOURCE(Surface,RenderTarget);
		DYNAMIC_CAST_OPENGLRESOURCE(Surface,DepthStencilTarget);
		if( ( RenderTarget && RenderTarget->Type == Surface->Type && RenderTarget->Resource == Surface->Resource )
		   || ( DepthStencilTarget && DepthStencilTarget->Type == Surface->Type && DepthStencilTarget->Resource == Surface->Resource ) )
		{
			GLuint FramebufferToDelete = It.Value();
			Device->PurgeFramebufferFromCaches( FramebufferToDelete );
			glDeleteFramebuffers( 1, &FramebufferToDelete );
			It.RemoveCurrent();
		}
	}
}

void FOpenGLDynamicRHI::PurgeFramebufferFromCaches( GLuint Framebuffer )
{
	if( Framebuffer == PendingFramebuffer )
	{
		PendingFramebuffer = 0;	// screen, so it isn't invalid
	}

	if( Framebuffer == CachedState.Framebuffer )
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDrawBuffer(GL_BACK);
		glReadBuffer(GL_BACK);
		CachedState.Framebuffer = 0;	// screen, so it isn't invalid
	}
}

/**
* Copies the contents of the given surface to its resolve target texture.
* @param SourceSurface - surface with a resolve texture to copy to
* @param bKeepOriginalSurface - TRUE if the original surface will still be used after this function so must remain valid
* @param ResolveParams - optional resolve params
*/
void FOpenGLDynamicRHI::CopyToResolveTarget(FSurfaceRHIParamRef SourceSurfaceRHI, UBOOL bKeepOriginalSurface, const FResolveParams& ResolveParams)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Surface,SourceSurface);

	if (ResolveParams.ResolveTarget || SourceSurface->ResolveTargetResource)
	{
		const UBOOL bIsColorBuffer = SourceSurface->Attachment == GL_COLOR_ATTACHMENT0;

		GLuint TargetFramebuffer = ResolveParams.ResolveTarget
									? GetOpenGLFramebuffer(((FOpenGLTexture2D *)ResolveParams.ResolveTarget)->ResolveTarget, NULL)
									: SourceSurface->ResolveTargetResource;
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, TargetFramebuffer);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);

		GLuint SourceFramebuffer = bIsColorBuffer ? GetOpenGLFramebuffer(SourceSurfaceRHI, NULL) : GetOpenGLFramebuffer(NULL, SourceSurfaceRHI);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, SourceFramebuffer);
		glReadBuffer(SourceSurface->Attachment);

		GLbitfield Mask = GL_NONE;
		if (bIsColorBuffer)
		{
			Mask = GL_COLOR_BUFFER_BIT;
		}
		else if (SourceSurface->Attachment == GL_DEPTH_ATTACHMENT)
		{
			Mask = GL_DEPTH_BUFFER_BIT;
		}
		else
		{
			Mask = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
		}

		// this flips the buffer vertically
		glBlitFramebuffer(
			0, 0, SourceSurface->SizeX, SourceSurface->SizeY,
			0, SourceSurface->SizeY, SourceSurface->SizeX, 0,
			Mask,
			GL_NEAREST);
		CheckOpenGLErrors();

		CachedState.Framebuffer = (GLuint)-1;
	}
}

void FOpenGLDynamicRHI::CopyFromResolveTargetFast(FSurfaceRHIParamRef DestSurface)
{
	// these need to be referenced in order for the FScreenVertexShader/FScreenPixelShader types to not be compiled out on PC
	TShaderMapRef<FScreenVertexShader> ScreenVertexShader(GetGlobalShaderMap());
	TShaderMapRef<FScreenPixelShader> ScreenPixelShader(GetGlobalShaderMap());
}

void FOpenGLDynamicRHI::CopyFromResolveTargetRectFast(FSurfaceRHIParamRef DestSurface,FLOAT X1,FLOAT Y1,FLOAT X2,FLOAT Y2)
{
	// these need to be referenced in order for the FScreenVertexShader/FScreenPixelShader types to not be compiled out on PC
	TShaderMapRef<FScreenVertexShader> ScreenVertexShader(GetGlobalShaderMap());
	TShaderMapRef<FScreenPixelShader> ScreenPixelShader(GetGlobalShaderMap());
}

void FOpenGLDynamicRHI::CopyFromResolveTarget(FSurfaceRHIParamRef DestSurface)
{
	// these need to be referenced in order for the FScreenVertexShader/FScreenPixelShader types to not be compiled out on PC
	TShaderMapRef<FScreenVertexShader> ScreenVertexShader(GetGlobalShaderMap());
	TShaderMapRef<FScreenPixelShader> ScreenPixelShader(GetGlobalShaderMap());
}

/**
 *	Returns the resolve target of a surface.
 *	@param SurfaceRHI	- Surface from which to get the resolve target
 *	@return				- Resolve target texture associated with the surface
 */
FTexture2DRHIRef FOpenGLDynamicRHI::GetResolveTarget( FSurfaceRHIParamRef SurfaceRHI )
{
	// It seems it's only used on PS3
	appErrorf(TEXT("RHIGetResolveTarget not supported in OpenGL driver"));
	return NULL;
}

void FOpenGLDynamicRHI::DiscardRenderBuffer( DWORD RenderBufferTypes )
{
	// Not supported on this platform at this time.
}

FOpenGLSurface::~FOpenGLSurface()
{
	ReleaseOpenGLFramebuffersForSurface( OpenGLRHI, this );

	if (ResolveTargetResource != 0)
	{
		OpenGLRHI->PurgeFramebufferFromCaches( ResolveTargetResource );
		glDeleteFramebuffers(1, &ResolveTargetResource);
	}

	if (Resource != 0 && Type == GL_RENDERBUFFER )
	{
		glDeleteRenderbuffers(1, &Resource);	// texture is deleted in its own object
	}
}

FOpenGLSurface *FOpenGLDynamicRHI::CreateSurface(UINT SizeX,UINT SizeY,BYTE Format,GLuint Texture,DWORD Flags,GLenum Target)
{
#ifdef GDEBUGGER_MODE
	if (Format == PF_DepthStencil)
	{
		Format = PF_D24;
	}
#endif
	const UBOOL bDepthStencilFormat = (Format == PF_DepthStencil);
	const UBOOL bDepthFormat = (Format == PF_ShadowDepth|| Format == PF_FilteredShadowDepth || Format == PF_D24) || bDepthStencilFormat;
	const GLenum Attachment = bDepthFormat ? (bDepthStencilFormat ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT) : GL_COLOR_ATTACHMENT0;

	// Determine whether to use MSAA for this surface.
	const UINT MaxMultiSamples = GSystemSettings.MaxMultiSamples;
	UINT MultiSampleCount = 1;
	if(MaxMultiSamples > 0 && (Flags&TargetSurfCreate_Multisample))
	{
		GLint GLMaxSamples = 0;

		glGetIntegerv(GL_MAX_SAMPLES, &GLMaxSamples);
		MultiSampleCount = Min<UINT>(MaxMultiSamples, GLMaxSamples);

		// MSAA surfaces can't be shared with a texture.
		Flags |= TargetSurfCreate_Dedicated;
	}

	GLuint Resource = 0;
	GLenum Type = GL_NONE;

#ifdef GDEBUGGER_MODE
	if (Texture)
#else
	if (Texture && !(Flags & TargetSurfCreate_Dedicated))
#endif
	{
		Resource = Texture;
		Type = Target;
	}
	else
	{
		glGenRenderbuffers(1, &Resource);
		glBindRenderbuffer(GL_RENDERBUFFER, Resource);

		GLenum InternalFormat, DataType;
		FindInternalFormatAndType(Format, InternalFormat, DataType, FALSE);

		if (MultiSampleCount > 1)
		{
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, MultiSampleCount, InternalFormat, SizeX, SizeY);
		}
		else
		{
			glRenderbufferStorage(GL_RENDERBUFFER, InternalFormat, SizeX, SizeY);
		}

		glBindRenderbuffer(GL_RENDERBUFFER, 0);

		Type = GL_RENDERBUFFER;

		CheckOpenGLErrors();
	}

	GLuint ResolveTargetFramebuffer = 0;
#ifndef GDEBUGGER_MODE
	if (Texture && (Flags & TargetSurfCreate_Dedicated))
	{
		glGenFramebuffers(1, &ResolveTargetFramebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, ResolveTargetFramebuffer);

		glFramebufferTexture2D(GL_FRAMEBUFFER, Attachment, Target, Texture, 0);

#if _DEBUG
		GLenum CompleteResult = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (CompleteResult != GL_FRAMEBUFFER_COMPLETE)
		{
			appErrorf(TEXT("Framebbuffer not complete. Status = 0x%x"), CompleteResult);
			return NULL;
		}
#endif

		CachedState.Framebuffer = (GLuint)-1;
	}
#endif
	return new FOpenGLSurface(this, Resource, ResolveTargetFramebuffer, Type, Attachment, Format, SizeX, SizeY, MultiSampleCount);
}

/**
* Creates a RHI surface that can be bound as a render target.
* Note that a surface cannot be created which is both resolvable AND readable.
* @param SizeX - The width of the surface to create.
* @param SizeY - The height of the surface to create.
* @param Format - The surface format to create.
* @param ResolveTargetTexture - The 2d texture which the surface will be resolved to.  It must have been allocated with bResolveTargetable=TRUE
* @param Flags - Surface creation flags
* @param UsageStr - Text describing usage for this surface
* @return The surface that was created.
*/
FSurfaceRHIRef FOpenGLDynamicRHI::CreateTargetableSurface(
	UINT SizeX,
	UINT SizeY,
	BYTE Format,
	FTexture2DRHIParamRef ResolveTargetTextureRHI,
	DWORD Flags,
	const TCHAR* UsageStr
	)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Texture2D,ResolveTargetTexture);

	if (ResolveTargetTexture)
	{
		Flags |= TargetSurfCreate_Dedicated;
	}

	return CreateSurface(SizeX, SizeY, Format, ResolveTargetTexture ? ResolveTargetTexture->Resource : 0, Flags, GL_TEXTURE_2D);
}

/**
* Creates a RHI surface that can be bound as a render target and can resolve w/ a cube texture
* Note that a surface cannot be created which is both resolvable AND readable.
* @param SizeX - The width of the surface to create.
* @param Format - The surface format to create.
* @param ResolveTargetTexture - The cube texture which the surface will be resolved to.  It must have been allocated with bResolveTargetable=TRUE
* @param CubeFace - face from resolve texture to use as surface
* @param Flags - Surface creation flags
* @param UsageStr - Text describing usage for this surface
* @return The surface that was created.
*/
FSurfaceRHIRef FOpenGLDynamicRHI::CreateTargetableCubeSurface(
	UINT SizeX,
	BYTE Format,
	FTextureCubeRHIParamRef ResolveTargetTextureRHI,
	ECubeFace CubeFace,
	DWORD Flags,
	const TCHAR* UsageStr
	)
{
	DYNAMIC_CAST_OPENGLRESOURCE(TextureCube,ResolveTargetTexture);

	return CreateSurface(SizeX, SizeX, Format, ResolveTargetTexture ? ResolveTargetTexture->Resource : 0, Flags, GetOpenGLCubeFace(CubeFace));
}

void FOpenGLDynamicRHI::ReadSurfaceData(FSurfaceRHIParamRef SurfaceRHI,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<BYTE>& OutData, FReadSurfaceDataFlags InFlags)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Surface,Surface);

	GLuint FramebufferToDelete = 0;
	GLuint RenderbufferToDelete = 0;

	GLenum InternalFormat, Type;
	FindInternalFormatAndType(Surface->Format, InternalFormat, Type, FALSE);

	const UBOOL bDepthStencilFormat = (Surface->Format == PF_DepthStencil);
	const UBOOL bDepthFormat = (Surface->Format == PF_ShadowDepth|| Surface->Format == PF_FilteredShadowDepth || Surface->Format == PF_D24) || bDepthStencilFormat;
	const GLenum Attachment = bDepthFormat ? (bDepthStencilFormat ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT) : GL_COLOR_ATTACHMENT0;

	GLuint SourceFramebuffer = Surface->Resource;
	if (Surface->MultiSampleCount > 1)
	{
		// OpenGL doesn't allow to read pixels from multisample framebuffers, we need a single sample copy
		if (Surface->ResolveTargetResource)
		{
			SourceFramebuffer = Surface->ResolveTargetResource;
		}
		else
		{
			glGenFramebuffers(1, &FramebufferToDelete);
			glBindFramebuffer(GL_FRAMEBUFFER, FramebufferToDelete);

			GLuint Renderbuffer = 0;
			glGenRenderbuffers(1, &RenderbufferToDelete);
			glBindRenderbuffer(GL_RENDERBUFFER, RenderbufferToDelete);
			glRenderbufferStorage(GL_RENDERBUFFER, InternalFormat, Surface->SizeX, Surface->SizeY);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);

			glFramebufferRenderbuffer(GL_FRAMEBUFFER, Attachment, GL_RENDERBUFFER, RenderbufferToDelete);
#if _DEBUG
			GLenum CompleteResult = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (CompleteResult != GL_FRAMEBUFFER_COMPLETE)
			{
				appErrorf(TEXT("Framebbuffer not complete. Status = 0x%x"), CompleteResult);
			}
#endif
			glBindFramebuffer(GL_READ_FRAMEBUFFER, Surface->Resource);
			glBlitFramebuffer(
				0, 0, Surface->SizeX, Surface->SizeY,
				0, 0, Surface->SizeX, Surface->SizeY,
				(bDepthFormat ? (bDepthStencilFormat ? (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT) : GL_DEPTH_BUFFER_BIT) : GL_COLOR_BUFFER_BIT),
				GL_NEAREST);
			CheckOpenGLErrors();

			SourceFramebuffer = FramebufferToDelete;
		}
	}

	UINT SizeX = MaxX - MinX + 1;
	UINT SizeY = MaxY - MinY + 1;

	OutData.Empty( SizeX * SizeY * sizeof(FColor) );
	BYTE* TargetBuffer = (BYTE*)&OutData(OutData.Add(SizeX * SizeY * sizeof(FColor)));

	glBindFramebuffer(GL_READ_FRAMEBUFFER, SourceFramebuffer);
	glReadBuffer( (!bDepthFormat && !bDepthStencilFormat && !SourceFramebuffer) ? GL_BACK : Attachment);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	check(!bDepthFormat && !bDepthStencilFormat);	// if those are here, you need to read pixels differently.
	check(Surface->Format == PF_A16B16G16R16);		// otherwise this needs testing, and probably changes
	glReadPixels(MinX, MinY, SizeX, SizeY, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, TargetBuffer );

	UINT SizeOfLine = SizeX * 4 * sizeof(BYTE);
	BYTE* LineOfBytes = (BYTE*)malloc( SizeOfLine );
	for( UINT Line = 0; Line < SizeY/2; ++Line )
	{
		BYTE* Line1 = TargetBuffer + Line * SizeOfLine;
		BYTE* Line2 = TargetBuffer + (SizeY-1-Line) * SizeOfLine;
		memcpy( LineOfBytes, Line1, SizeOfLine );
		memcpy( Line1, Line2, SizeOfLine );
		memcpy( Line2, LineOfBytes, SizeOfLine );
	}

	CheckOpenGLErrors();
	glPixelStorei(GL_PACK_ALIGNMENT, 4);

	if( FramebufferToDelete )
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers( 1, &FramebufferToDelete );
	}

	if( RenderbufferToDelete )
	{
		glDeleteRenderbuffers( 1, &RenderbufferToDelete );
	}

	CheckOpenGLErrors();

	CachedState.Framebuffer = (GLuint)-1;
}

void FOpenGLDynamicRHI::ReadSurfaceDataMSAA(FSurfaceRHIParamRef SurfaceRHI,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags)
{
}

void FOpenGLDynamicRHI::ReadSurfaceFloatData(FSurfaceRHIParamRef SurfaceRHI,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<FFloat16Color>& OutData,ECubeFace CubeFace)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Surface,Surface);
	check(0);
}

void FOpenGLDynamicRHI::BindPendingFramebuffer()
{
	if (CachedState.Framebuffer != PendingFramebuffer)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, PendingFramebuffer);

		if (PendingFramebuffer)
		{
			glReadBuffer(bPendingFramebufferHasRenderTarget ? GL_COLOR_ATTACHMENT0 : GL_NONE);
			glDrawBuffer(bPendingFramebufferHasRenderTarget ? GL_COLOR_ATTACHMENT0 : GL_NONE);
		}
		else
		{
			glReadBuffer(GL_BACK);
			glDrawBuffer(GL_BACK);
		}

		CachedState.Framebuffer = PendingFramebuffer;
	}
}

/**
 *	Copies the contents of the back buffer to specified texture.
 *	@param ResolveParams Required resolve params
 */
void FOpenGLDynamicRHI::CopyFrontBufferToTexture( const FResolveParams& ResolveParams )
{
	// Not supported
}

#if !RHI_UNIFIED_MEMORY && !USE_NULL_RHI
void FOpenGLDynamicRHI::GetTargetSurfaceSize(FSurfaceRHIParamRef SurfaceRHI, UINT& OutSizeX, UINT& OutSizeY)
{
    if ( SurfaceRHI )
    {
        DYNAMIC_CAST_OPENGLRESOURCE(Surface,Surface);
        OutSizeX = Surface->SizeX;
        OutSizeY = Surface->SizeY;
    }
    else
    {
        OutSizeX = 0;
        OutSizeY = 0;
    }
}
#endif
