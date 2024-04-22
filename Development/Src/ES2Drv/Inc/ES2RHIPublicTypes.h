/*=============================================================================
	ES2RHITypes.h: OpenGL ES 2.0 RHI type declarations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __ES2RHIPUBLICTYPES_H__
#define __ES2RHIPUBLICTYPES_H__

#if WITH_ES2_RHI

// If we're compiling with a static RHI then the base class for all ES2 resources will be
// the TES2RHIResource class that was declared in StaticRHI.h, otherwise we'll use the
// TDynamicRHIResource class from DynamicRHI.h
#if USE_STATIC_RHI
	#define ES2_BASE_RESOURCE( Type ) TES2RHIResource< Type >
#else
	#define ES2_BASE_RESOURCE( Type ) TDynamicRHIResource< Type >
#endif


template< ERHIResourceTypes ResourceType >
struct RHIRefPlaceholder : public FRefCountedObject, public ES2_BASE_RESOURCE( ResourceType )
{
};

#if IPHONE && WITH_IOS_5
	class FES2OcclusionQuery : public FRefCountedObject, public ES2_BASE_RESOURCE( RRT_OcclusionQuery )
	{
	public:

		/** The query resource. */
		GLuint Resource;

		/** The cached query result. */
		GLuint Result;

		/** TRUE if the query's result is cached. */
		UBOOL bResultIsCached : 1;

		/** Initialization constructor. */
		FES2OcclusionQuery()
		:	Resource(0)
		,	bResultIsCached(FALSE)
		{}
		FES2OcclusionQuery(GLuint InResource)
		:	Resource(InResource)
		,	bResultIsCached(FALSE)
		{}

		~FES2OcclusionQuery()
		{
			if (Resource)
			{
				glDeleteQueriesEXT(1, &Resource);
			}
		}
	};
#else
	typedef RHIRefPlaceholder< RRT_OcclusionQuery > FES2OcclusionQuery;
#endif

typedef RHIRefPlaceholder< RRT_SharedMemoryResource > FES2SharedMemoryResource;


/**
 * ES2 viewport implementation
 */
class FES2Viewport
	: public FRefCountedObject, public ES2_BASE_RESOURCE( RRT_Viewport )
{

public:

	/** FES2Viewport constructor */
	FES2Viewport( void* InWindowHandle, UINT InSizeX, UINT InSizeY, UBOOL bInIsFullscreen );

	/** FES2Viewport destructor */
	virtual ~FES2Viewport();

	// Accessors.
	UINT GetSizeX() const { return SizeX; }
	UINT GetSizeY() const { return SizeY; }
	UBOOL IsFullscreen() const { return bIsFullscreen; }

	/** A per platform pointer, that the platform can use for MakeCurrent, SwapBuffers, etc */
	void* PlatformData;

	/** If the platform needs the back buffer to use an existing back buffer GL name, set it here, otherwise 0 will be used (default backbuffer, usually) */
	GLuint BackBufferName;

	/** If the platform needs the back buffer to use an existing MSAA back buffer GL name, set it here, otherwise 0 will be used (default backbuffer, usually) */
	GLuint MSAABackBufferName;

	/** Width of viewport */
	UINT SizeX;

	/** Heigh of viewport */
	UINT SizeY;

	/** True if this will be a full screen viewport */
	UBOOL bIsFullscreen;

	/** The back buffer for this viewport */
	FSurfaceRHIRef ViewportBackBuffer;
	
	/** The (optional) default back buffer for this viewport */
	FSurfaceRHIRef ViewportDepthBuffer;

	/** The default viewport depth texture. Resolve target for ViewportDepthBuffer. */
	FTexture2DRHIRef ViewportDepthBufferTexture;

#if IPHONE || ANDROID
	/** The MSAA enabled back buffer for this viewport */
	FSurfaceRHIRef ViewportBackBufferMSAA;

	/** The (optional) MSAA default back buffer for this viewport */
	FSurfaceRHIRef ViewportDepthBufferMSAA;
#endif

};


// Shader parameter caching because GLSL "combined shaders = program" model sucks.
struct FShaderParameters : public FRefCountedObject
{
	FShaderParameters(GLenum TypeOfShader) : ShaderType(TypeOfShader), Version( 0 ) 
	{
	}
	const GLenum ShaderType;
	INT Version;
	TMap< FString, TArray<BYTE> > Parameters;
};

struct FES2VertexShader : public FShaderParameters, public ES2_BASE_RESOURCE( RRT_VertexShader )
{
	FES2VertexShader() : FShaderParameters( GL_VERTEX_SHADER )
	{
	}
};

struct FES2PixelShader : public FShaderParameters, public ES2_BASE_RESOURCE( RRT_PixelShader )
{
	FES2PixelShader() : FShaderParameters( GL_FRAGMENT_SHADER )
	{
	}
};



// Depth State
struct FES2DepthState : public FRefCountedObject, public FDepthStateInitializerRHI, public ES2_BASE_RESOURCE( RRT_DepthState )
{
};

// Sampler state
struct FES2SamplerState : public FRefCountedObject, public FSamplerStateInitializerRHI, public ES2_BASE_RESOURCE( RRT_SamplerState )
{
	FES2SamplerState( const FSamplerStateInitializerRHI& Initializer )
	:	FSamplerStateInitializerRHI(Initializer)
	{
	}
};

// Rasterizer state
struct FES2RasterizerState : public FRefCountedObject, public FRasterizerStateInitializerRHI, public ES2_BASE_RESOURCE( RRT_RasterizerState )
{
};

// Stencil state
struct FES2StencilState : public FRefCountedObject, public FStencilStateInitializerRHI, public ES2_BASE_RESOURCE( RRT_StencilState )
{
};

// Blend state
struct FES2BlendState : public FRefCountedObject, public FBlendStateInitializerRHI, public ES2_BASE_RESOURCE( RRT_BlendState )
{
};

/**
 * Basic vertex declaration
 */
struct FES2VertexDeclaration : public FRefCountedObject, public ES2_BASE_RESOURCE( RRT_VertexDeclaration )
{
	FES2VertexDeclaration(const FVertexDeclarationElementList& InElements, FName InDeclName)
	: VertexElements(InElements)
	, DeclName(InDeclName)
	{
		
	}
	
	FVertexDeclarationElementList VertexElements;
	FName DeclName;
};



// Index/vertex/pixel buffer.
class FES2Buffer : public FRefCountedObject
{
public:
	FES2Buffer(const GLenum InBufferType, const GLuint InBufferName, const GLsizeiptr InBufferSize, const UBOOL InIsDynamic, const UBOOL InIsSmallUpdate)
	: BufferType(InBufferType)
	, BufferName(InBufferName)
	, BufferSize(InBufferSize)
	, bIsDynamic(InIsDynamic)
    , bIsSmallUpdate(InIsSmallUpdate)
	, bLockedReadOnly(FALSE)
	, LockedSize(0)
	, LockedOffset(0)
	, LockBuffer( NULL )
	{
        //XXX - mapbuffer check
        if (bIsSmallUpdate)
        {
            LockBuffer = appMalloc(BufferSize);
        }
	}
	virtual ~FES2Buffer();
	const GLenum GetBufferType() const { return BufferType; }
	const GLuint GetBufferName() const { return BufferName; }
	const GLsizeiptr GetBufferSize() const { return BufferSize; }
	const UBOOL IsDynamic() const { return bIsDynamic; }
	const GLsizeiptr GetLockedOffset() const { return LockedOffset; }
	const GLsizeiptr GetLockedSize() const { return LockedSize; }
	const UBOOL IsLockedReadOnly() const { return bLockedReadOnly; }
	const UBOOL IsLocked() const { return (LockedSize != 0); }
	void SetLockedRange(const GLsizeiptr Offset, const GLsizeiptr Size, const UBOOL ReadOnly) { LockedOffset = Offset; LockedSize = Size; bLockedReadOnly = ReadOnly; }
	void Bind();
	BYTE *Lock(const UINT Offset, const UINT Size, const UBOOL bReadOnly, const UBOOL bDiscard);
	void Unlock();
	
protected:
	const GLenum BufferType;
	GLuint BufferName;
	const GLsizeiptr BufferSize; // The size of the buffer.
	const UBOOL bIsDynamic; // If the buffer is dynamic (discarded on locks).
    const UBOOL bIsSmallUpdate; // Retain entire buffer
	GLsizeiptr LockedSize;
	GLsizeiptr LockedOffset;
	UBOOL bLockedReadOnly;

	GLuint LockSize;
	GLuint LockOffset;
	void* LockBuffer;
};

// Index buffer.
class FES2IndexBuffer : public FES2Buffer, public ES2_BASE_RESOURCE( RRT_IndexBuffer )
{
public:
	FES2IndexBuffer(const GLuint InBufferName, const GLsizeiptr InBufferSize, const UINT InStride, const UBOOL InIsDynamic, const UBOOL InIsSmallUpdate)
	: FES2Buffer(GL_ELEMENT_ARRAY_BUFFER, InBufferName, InBufferSize, InIsDynamic, InIsSmallUpdate)
	, Stride(InStride)
	{
	}
	const UINT GetStride() const { return Stride; }
	
protected:
	const UINT Stride; // Size of one element.
};


// Vertex buffer.
class FES2VertexBuffer : public FES2Buffer, public ES2_BASE_RESOURCE( RRT_VertexBuffer )
{
public:
	FES2VertexBuffer(const GLuint InBufferName, const GLsizeiptr InBufferSize, const UBOOL InIsDynamic, const UBOOL InIsSmallUpdate)
	: FES2Buffer(GL_ARRAY_BUFFER, InBufferName, InBufferSize, InIsDynamic, InIsSmallUpdate)
	{
	}
};


class FES2BoundShaderState : public FRefCountedObject, public ES2_BASE_RESOURCE( RRT_BoundShaderState )
{
public:
	FES2BoundShaderState( FVertexDeclarationRHIRef InVertexDeclaration, FVertexShaderRHIRef InVertexShader, FPixelShaderRHIRef InPixelShader, EMobileGlobalShaderType InMobileGlobalShaderType )
	: VertexDeclaration(InVertexDeclaration)
	, VertexShader(InVertexShader)
	, PixelShader(InPixelShader)
	, VertexShaderParametersVersion(0)
	, FragmentShaderParametersVersion(0)
	, MobileGlobalShaderType(InMobileGlobalShaderType)
	{
	}
	
	FVertexDeclarationRHIRef VertexDeclaration;
	FVertexShaderRHIRef VertexShader;
	FPixelShaderRHIRef PixelShader;
	INT VertexShaderParametersVersion;
	INT FragmentShaderParametersVersion;
	EMobileGlobalShaderType MobileGlobalShaderType;
};


/**
 * An outstanding ES2 texture lock
 */
struct FES2OutstandingTextureLock
{
	/** Mip level index that was locked */
	INT LockedMipIndex;

	/** The scratch memory buffer for the current lock */
	void* LockBuffer;


	/** Constructor */
	FES2OutstandingTextureLock()
		: LockedMipIndex( 0 ),
		  LockBuffer( NULL )
	{
	}
};



class FES2BaseTexture : public FRefCountedObject
{
public:
	FES2BaseTexture(const GLenum InTextureType, const GLint InFaceCount, const GLuint InTextureName, const EPixelFormat InFormat, const GLint InWidth, const GLint InHeight, const GLint InMipCount, const UBOOL InIsSRGB, const ESamplerFilter DefaultFilter, GLenum InAddress, void* Mip0=NULL, GLuint BulkDataSize=0);
		
	virtual ~FES2BaseTexture();
	const GLenum GetTextureType() const		{ return TextureType; }
	const GLuint GetTextureName() const		{ return TextureName; }
	const GLint GetMipCount() const			{ return MipCount; }
	const GLint GetWidth() const			{ return Width; }
	const GLint GetHeight() const			{ return Height; }
	const EPixelFormat GetFormat() const	{ return Format; }
	const ESamplerFilter GetFilter() const	{ return Filter; }
    GLenum GetAddressS() const              { return AddressS; }
    GLenum GetAddressT() const              { return AddressT; }
	void SetFilter(ESamplerFilter InFilter)	{ Filter = InFilter; }
    void SetAddressS(GLenum InAddress)      { AddressS = InAddress; }
    void SetAddressT(GLenum InAddress)      { AddressT = InAddress; }
	void* Lock(UINT MipIndex);
	void Unlock(UINT MipIndex, INT FaceIndex=-1);
	void Bind();

	/**
	 * Swaps the OpenGL texture name with the other texture.
	 * Used by RHICopyToResolveTarget() to ping-pong between two resolve textures.
	 */
	void SwapTextureName( FES2BaseTexture* OtherTexture )
	{
		GLuint OtherTextureName = OtherTexture->TextureName;
		OtherTexture->TextureName = this->TextureName;
		this->TextureName = OtherTextureName;
	}
		
		
protected:
	const GLenum TextureType;
	GLuint TextureName;
	const GLint MipCount;
	const GLint Width;
	const GLint Height;
	const EPixelFormat Format;		// this is the Unreal format, not the GL one.
	ESamplerFilter Filter;			// Unreal filter type
    GLenum AddressS, AddressT;
	const UBOOL bIsSRGB;
#if USE_DETAILED_IPHONE_MEM_TRACKING
	UINT Size;
#endif

	/** Currently outstanding locks on this texture */
	TArray< FES2OutstandingTextureLock > Locks;
};


class FES2Texture : public FES2BaseTexture, public ES2_BASE_RESOURCE( RRT_Texture )
{
public:
	FES2Texture(const GLuint InTextureName, const EPixelFormat InFormat, const GLint InWidth, const GLint InHeight, const GLint InMipCount, const UBOOL InIsSRGB, const ESamplerFilter DefaultFilter, GLenum InAddress)
	: FES2BaseTexture((const GLenum) (GL_TEXTURE_2D), 1, InTextureName, InFormat, InWidth, InHeight, InMipCount, InIsSRGB, DefaultFilter, InAddress) 
	{
	}
};




class FES2Texture2D : public FES2BaseTexture, public ES2_BASE_RESOURCE( RRT_Texture2D )
{
public:
	FES2Texture2D(const GLuint InTextureName, const EPixelFormat InFormat, const GLint InWidth, const GLint InHeight, const GLint InMipCount, DWORD Flags, const UBOOL InIsSRGB, const ESamplerFilter DefaultFilter, GLenum InAddress, void* Mip0=NULL, GLuint BulkDataSize=0)
	: FES2BaseTexture((const GLenum) (GL_TEXTURE_2D), 1, InTextureName, InFormat, InWidth, InHeight, InMipCount, InIsSRGB, DefaultFilter, InAddress, Mip0, BulkDataSize) 
	,	CreateFlags(Flags)
	{
#if USE_DETAILED_IPHONE_MEM_TRACKING
		Size = GetMemorySize();
#endif
		INC_TRACKED_OPEN_GL_TEXTURE_MEM(Size);
	}

	DWORD	GetCreateFlags() const	{ return CreateFlags; }
	DWORD	GetMemorySize() const
	{
		DWORD NumBlocks = Align(Width,GPixelFormats[Format].BlockSizeX) / GPixelFormats[Format].BlockSizeX * Align(Height,GPixelFormats[Format].BlockSizeY) / GPixelFormats[Format].BlockSizeY;
		return NumBlocks * GPixelFormats[Format].BlockBytes;
	}

protected:
	/** The ETextureCreateFlags that were specified at creation time. */
	DWORD	CreateFlags;
};


typedef FES2Texture2D FES2SharedTexture2D;

typedef FES2Texture2D FES2Texture3D;

class FES2Texture2DArray : public FES2BaseTexture, public ES2_BASE_RESOURCE( RRT_Texture2DArray )
{
	FES2Texture2DArray(const GLuint InTextureName, const EPixelFormat InFormat, const GLint InWidth, const GLint InHeight, const GLint InMipCount, const UBOOL InIsSRGB, const ESamplerFilter DefaultFilter, GLenum InAddress)
		: FES2BaseTexture((const GLenum) (GL_TEXTURE_2D), 1, InTextureName, InFormat, InWidth, InHeight, InMipCount, InIsSRGB, DefaultFilter, InAddress) 
	{
	}
};

typedef FES2Texture2DArray FES2SharedTexture2DArray;

class FES2TextureCube : public FES2BaseTexture, public ES2_BASE_RESOURCE( RRT_TextureCube )
{
public:
	FES2TextureCube(const GLuint InTextureName, const EPixelFormat InFormat, const GLint InSize, const GLint InMipCount, const UBOOL InIsSRGB, const ESamplerFilter DefaultFilter, GLenum InAddress)
	: FES2BaseTexture((const GLenum) (GL_TEXTURE_CUBE_MAP), 6, InTextureName, InFormat, InSize, InSize, InMipCount, InIsSRGB, DefaultFilter, InAddress) 
	{
	}
};




struct FES2Surface : public FRefCountedObject, public ES2_BASE_RESOURCE( RRT_Surface )
{
	/**
	 * Constructor for a placeholder surface with a simple width and height - nothing will be created for this surface
	 */
	FES2Surface(INT InWidth, INT InHeight);

	/**
	 * Constructor for a surface with no backing texture, so a new renderbuffer will be created
	 */
	FES2Surface(INT InWidth, INT InHeight, EPixelFormat Format, INT Samples);

	/**
	 * Constructor for a surface with an existing renderbuffer (ie the OS backbuffer)
	 */
	FES2Surface(INT InWidth, INT InHeight, GLuint ExistingRenderBuffer);

	/**
	 * Constructor for a surface that has a backing texture already made
	 */
	FES2Surface(FTexture2DRHIRef InResolveTexture, FTexture2DRHIRef InResolveTexture2, DWORD SurfCreateFlags );

	/**
	 * Constructor for a surface with a backing cubemap face already made
	 */
	FES2Surface(FTextureCubeRHIRef InResolveTextureCube, ECubeFace InResolveFace);

	/**
	 * Common destructor
	 */
	~FES2Surface();

	/**
	 * @return TRUE if this surface is a simple, unbacked placeholder surface
	 */
	UBOOL IsAPlaceholderSurface() const
	{
		return bPlaceholderSurface;
	}

	/**
	 * @return TRUE if this surface uses a render buffer instead of a ResolveTexture
	 */
	UBOOL HasValidRenderBuffer() const
	{
		return BackingRenderBuffer != 0xFFFFFFFF;
	}

	/**
	 * @return TRUE if this surface uses a render buffer instead of a ResolveTexture
	 */
	UBOOL HasSeparateStencilBuffer() const
	{
		return bUsingSeparateStencilBuffer;
	}

	FTexture2DRHIRef GetResolveTexture() const
	{
		return ResolveTexture;
	}

	FTexture2DRHIRef GetRenderTargetTexture() const
	{
		return RenderTargetTexture;
	}

	/** Swaps between the two resolve targets, if it was created with two dedicated buffers. */
	void		SwapResolveTarget();

	INT			GetWidth() const				{ return Width; }
	INT			GetHeight() const				{ return Height; }
	INT			GetUniqueID() const				{ return UniqueID + CurrentResolveTextureIndex; }
	GLuint		GetBackingRenderBuffer() const	{ return BackingRenderBuffer; }
	GLuint		GetBackingStencilBuffer() const { return bUsingSeparateStencilBuffer ? BackingStencilBuffer : BackingRenderBuffer; }

private:
	INT			Width;
	INT			Height;
//	FString		Usage;
//	BYTE		Format;
	UBOOL		bPlaceholderSurface;
	UBOOL		bBackingRenderBufferOwner;
	UBOOL		bUsingSeparateStencilBuffer;
	GLuint		BackingRenderBuffer;
	GLuint		BackingStencilBuffer;
	ECubeFace	ResolveTextureCubeFace;
	WORD		UniqueID;

	FTexture2DRHIRef	ResolveTexture;
	FTexture2DRHIRef	RenderTargetTexture;
	FTextureCubeRHIRef	ResolveTextureCube;
	INT					CurrentResolveTextureIndex;
	
	static WORD	NextUniqueID;
};

// No HS/DS/GS support in ES2, but need this to properly conform to RHI
typedef FRefCountedObject FES2HullShader;
typedef FRefCountedObject FES2DomainShader;
typedef FRefCountedObject FES2GeometryShader;
typedef FRefCountedObject FES2ComputeShader;

// Undefine temporary macros
#undef ES2_BASE_RESOURCE


#endif // WITH_ES2_RHI

#endif // __ES2RHIPUBLICTYPES_H__

