/*=============================================================================
	OpenGLResources.h: OpenGL resource RHI definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "BoundShaderStateCache.h"

enum OpenGLAttributeIndex
{
	GLAttr_Position			= 0,
	GLAttr_Tangent			= 1,
	GLAttr_Color0			= 2,
	GLAttr_Color1			= 3,
	GLAttr_Binormal			= 4,
	GLAttr_Normal			= 5,
	GLAttr_Weights			= 6,
	GLAttr_Bones			= 7,
	GLAttr_TexCoord0		= 8,
	GLAttr_TexCoord1		= 9,
	GLAttr_TexCoord2		= 10,
	GLAttr_TexCoord3		= 11,
	GLAttr_TexCoord4		= 12,
	GLAttr_TexCoord5		= 13,
	GLAttr_TexCoord6		= 14,
	GLAttr_TexCoord7		= 15
};

struct OpenGLVertexElement
{
	GLenum Type;
	GLuint StreamIndex;
	GLuint Usage;
	GLuint Offset;
	GLuint Size;
	GLboolean bNormalized;
};

/** This represents a vertex declaration that hasn't been combined with a specific shader to create a bound shader. */
class FOpenGLVertexDeclaration : public FRefCountedObject, public TDynamicRHIResource<RRT_VertexDeclaration>
{
public:

	/** Elements of the vertex declaration. */
	TArray<OpenGLVertexElement> VertexElements;

	/** Initialization constructor. */
	FOpenGLVertexDeclaration(const FVertexDeclarationElementList& InElements);
};

class FOpenGLBoundShaderState;

template <ERHIResourceTypes ResourceTypeEnum, GLenum Type>
class TOpenGLShader : public FRefCountedObject, public TDynamicRHIResource<ResourceTypeEnum>
{
public:

	GLuint Resource;
	DWORD CodeCrc;

	TOpenGLShader(const TArray<BYTE>& InCode)
		:	Resource(0)
		,	Code(InCode)
	{
		CodeCrc = appMemCrc(&Code(0), Code.Num());
	}

	~TOpenGLShader()
	{
		if (Resource)
		{
			glDeleteShader(Resource);
		}
	}

	UBOOL Compile();

private:

	TArray<BYTE> Code;
};

typedef TOpenGLShader<RRT_VertexShader, GL_VERTEX_SHADER> FOpenGLVertexShader;
typedef TOpenGLShader<RRT_PixelShader, GL_FRAGMENT_SHADER> FOpenGLPixelShader;

/**
 * Combined shader state and vertex definition for rendering geometry. 
 * Each unique instance consists of a vertex decl, vertex shader, and pixel shader.
 */
class FOpenGLBoundShaderState :
	public FRefCountedObject,
	public TDynamicRHIResource<RRT_BoundShaderState>
{
	friend class FOpenGLDynamicRHI;

public:

	FCachedBoundShaderStateLink CacheLink;

	GLuint Resource;
	TRefCountPtr<FOpenGLVertexDeclaration> VertexDeclaration;
	TRefCountPtr<FOpenGLVertexShader> VertexShader;
	TRefCountPtr<FOpenGLPixelShader> PixelShader;

	/** Initialization constructor. */
	FOpenGLBoundShaderState(
		class FOpenGLDynamicRHI* InOpenGLRHI,
		FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
		DWORD* InStreamStrides,
		FVertexShaderRHIParamRef InVertexShaderRHI,
		FPixelShaderRHIParamRef InPixelShaderRHI
		);

	~FOpenGLBoundShaderState()
	{
		if (Resource)
		{
			glDeleteProgram(Resource);
		}
	}

	void Bind();

	enum UniformArrayType
	{
		UniformArray_VLocal = 0,
		UniformArray_VGlobal = 1,
		UniformArray_VBones = 2,
		UniformArray_VBool = 3,
		UniformArray_PLocal = 4,
		UniformArray_PGlobal = 5,
		UniformArray_PBool = 6
	};

	void UpdateUniforms(INT ArrayNum, void *Data, UINT Size);

private:

	FOpenGLDynamicRHI* OpenGLRHI;

	struct FUniformArray
	{
		GLint Location;
		UINT CacheSize;
		void *Cache;
		
		FUniformArray()
		:	Location(-1)
		,	CacheSize(0)
		,	Cache(NULL)
		{
		}

		~FUniformArray()
		{
			if (Cache)
			{
				appFree(Cache);
			}
		}
	};

	FUniformArray UniformArrays[7];

	void Setup(DWORD* InStreamStrides);
	void SetupUniformArray(const GLchar *Name);
};

extern void OnVertexBufferDeletion( GLuint VertexBufferResource );

extern void CachedBindArrayBuffer( GLuint Buffer );
extern void CachedBindElementArrayBuffer( GLuint Buffer );
extern void CachedBindPixelUnpackBuffer( GLuint Buffer );
extern void CachedBindUniformBuffer( GLuint Buffer );

typedef void (*BufferBindFunction)( GLuint Buffer );

template <ERHIResourceTypes ResourceTypeEnum, GLenum Type, BufferBindFunction BufBind>
class TOpenGLBuffer : public FRefCountedObject, public TDynamicRHIResource<ResourceTypeEnum>
{
public:

	GLuint Resource;

	TOpenGLBuffer(DWORD InSize, UBOOL bInIsDynamic, const void *InData = NULL):
		Resource(0),
		Size(InSize),
		bIsDynamic(bInIsDynamic),
		bIsLocked(FALSE),
		bIsLockReadOnly(FALSE)
	{
		glGenBuffers(1, &Resource);
		Bind();
		CreateBuffer( InData );
	}

	virtual ~TOpenGLBuffer()
	{
		if (Resource != 0)
		{
			if( Type == GL_ARRAY_BUFFER )
			{
				OnVertexBufferDeletion( Resource );	// We need to make sure devices invalidate their vertex array caches
			}
			glDeleteBuffers(1, &Resource);
		}
	}

	void Bind()
	{
		BufBind(Resource);
	}

	BYTE *Lock(DWORD InOffset, DWORD InSize, UBOOL bReadOnly, UBOOL bDiscard)
	{
		Bind();

#if GL_ARB_map_buffer_range
		GLenum Access = bDiscard ? (GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_WRITE_BIT) : (bReadOnly ? GL_MAP_READ_BIT : (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT));
		BYTE *Data = (BYTE *)glMapBufferRange(Type, InOffset, InSize, Access);
		InOffset = 0;
#else
		if (bDiscard && (InSize == Size))
		{
			CreateBuffer();
		}

		GLenum Access = bDiscard ? GL_WRITE_ONLY : (bReadOnly ? GL_READ_ONLY : GL_READ_WRITE);
		BYTE *Data = (BYTE *)glMapBuffer(Type, Access);
		LockOffset = InOffset;
		LockSize = InSize;
#endif
		check(Data != NULL);
		bIsLocked = TRUE;
		bIsLockReadOnly = bReadOnly;
#if GL_ARB_map_buffer_range
		return Data;
#else
		return Data + InOffset;
#endif
	}

	void Unlock()
	{
		if (bIsLocked)
		{
			Bind();
#if !GL_ARB_map_buffer_range
			if (!bIsLockReadOnly)
			{
				glFlushMappedBufferRangeAPPLE(Type, LockOffset, LockSize);
			}
#endif
			glUnmapBuffer(Type);
			bIsLocked = FALSE;
		}
	}

	void Update(void *InData, UINT InOffset, UINT InSize, UBOOL bDiscard)
	{
		Bind();
		if (bDiscard)
		{
//			CreateBuffer();
		}
		glBufferSubData(Type, InOffset, InSize, InData);
	}

	DWORD GetSize() const { return Size; }
	UBOOL IsDynamic() const { return bIsDynamic; }
	UBOOL IsLocked() const { return bIsLocked; }
	UBOOL IsLockReadOnly() const { return bIsLockReadOnly; }

private:

	void CreateBuffer( const void* InData = NULL )
	{
		glBufferData(Type, Size, InData, bIsDynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
#if !GL_ARB_map_buffer_range
		glBufferParameteriAPPLE(Type, GL_BUFFER_FLUSHING_UNMAP_APPLE, GL_FALSE);
#endif
	}

	DWORD Size;
#if !GL_ARB_map_buffer_range
	DWORD LockOffset;
	DWORD LockSize;
#endif
	const BITFIELD bIsDynamic : 1;
	BITFIELD bIsLocked : 1;
	BITFIELD bIsLockReadOnly : 1;
};

typedef TOpenGLBuffer<RRT_VertexBuffer, GL_ARRAY_BUFFER, CachedBindArrayBuffer> FOpenGLVertexBuffer;
typedef TOpenGLBuffer<RRT_None, GL_PIXEL_UNPACK_BUFFER_ARB, CachedBindPixelUnpackBuffer> FOpenGLPixelBuffer;
typedef TOpenGLBuffer<RRT_None, GL_UNIFORM_BUFFER_EXT, CachedBindUniformBuffer> FOpenGLUniformBuffer;

class FOpenGLIndexBuffer : public TOpenGLBuffer<RRT_IndexBuffer, GL_ELEMENT_ARRAY_BUFFER, CachedBindElementArrayBuffer>
{
public:

	UBOOL bIs32Bit;

	FOpenGLIndexBuffer(DWORD InSize, UBOOL bInIsDynamic, const void *InData = NULL, UBOOL bInIs32Bit = FALSE)
		: TOpenGLBuffer<RRT_IndexBuffer, GL_ELEMENT_ARRAY_BUFFER, CachedBindElementArrayBuffer>(InSize, bInIsDynamic, InData), bIs32Bit(bInIs32Bit)
	{
	}
};

class FOpenGLSurface;

// Textures.
template<ERHIResourceTypes ResourceTypeEnum>
class TOpenGLTexture : public FRefCountedObject, public TDynamicRHIResource<ResourceTypeEnum>
{
public:

	/** The OpenGL texture resource. */
	GLuint Resource;

	/** The OpenGL texture target. */
	GLenum Target;

	GLenum InternalFormat;
	GLenum Type;

	/** The width of the texture. */
	const UINT SizeX;

	/** The height of texture. */
	const UINT SizeY;

	/** The number of mip-maps in the texture. */
	const UINT NumMips;

	/** Index of the largest mip-map in the texture */
	UINT BaseLevel;

	/** The texture's format. */
	EPixelFormat Format;

	/** Whether the texture is a cube-map. */
	const BITFIELD bCubemap : 1;

	/** Whether the texture is dynamic. */
	const BITFIELD bDynamic : 1;

	TRefCountPtr<FOpenGLSurface> ResolveTarget;
	
	/** Initialization constructor. */
	TOpenGLTexture(
		class FOpenGLDynamicRHI* InOpenGLRHI,
		GLuint InResource,
		GLenum InTarget,
		GLenum InInternalFormat,
		GLenum InType,
		UINT InSizeX,
		UINT InSizeY,
		UINT InNumMips,
		EPixelFormat InFormat,
		UBOOL bInCubemap,
		UBOOL bInDynamic
		)
		: OpenGLRHI(InOpenGLRHI)
		, Resource(InResource)
		, Target(InTarget)
		, InternalFormat(InInternalFormat)
		, Type(InType)
		, SizeX(InSizeX)
		, SizeY(InSizeY)
		, NumMips(InNumMips)
		, BaseLevel(0)
		, Format(InFormat)
		, MemorySize( 0 )
		, bCubemap(bInCubemap)
		, bDynamic(bInDynamic)
	{
		PixelBuffers.AddZeroed(NumMips * (bCubemap ? 6 : 1));
	}

	virtual ~TOpenGLTexture();

	/**
	 * Locks one of the texture's mip-maps.
	 * @return A pointer to the specified texture data.
	 */
	void* Lock(UINT MipIndex,UINT ArrayIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride);

	/** Unlocks a previously locked mip-map. */
	void Unlock(UINT MipIndex,UINT ArrayIndex);

	INT GetMemorySize() const
	{
		return MemorySize;
	}

	void SetMemorySize( INT InMemorySize )
	{
		MemorySize = InMemorySize;
	}

	UBOOL NeedsToChangeSamplerState(const FOpenGLSamplerState *NewState, GLenum NewMinFilter, UINT NewBaseLevel)
	{
		return ( (SamplerState.AddressU != NewState->AddressU) ||
				 (SamplerState.AddressV != NewState->AddressV) ||
				 (SamplerState.AddressW != NewState->AddressW) ||
				 (SamplerState.MagFilter != NewState->MagFilter) ||
				 (SamplerState.MinFilter != NewMinFilter) ||
				 (SamplerState.MaxAnisotropy != NewState->MaxAnisotropy) ||
				 (BaseLevel != NewBaseLevel) );
	}

	void SetSamplerState(const FOpenGLSamplerState *NewState, GLenum NewMinFilter, UINT NewBaseLevel)
	{
		// Assumes the texture is bound
		if (SamplerState.AddressU != NewState->AddressU)
		{
			glTexParameteri(Target, GL_TEXTURE_WRAP_S, NewState->AddressU);
			SamplerState.AddressU = NewState->AddressU;
		}
		if (SamplerState.AddressV != NewState->AddressV)
		{
			glTexParameteri(Target, GL_TEXTURE_WRAP_T, NewState->AddressV);
			SamplerState.AddressV = NewState->AddressV;
		}
		if (SamplerState.AddressW != NewState->AddressW)
		{
			glTexParameteri(Target, GL_TEXTURE_WRAP_R, NewState->AddressW);
			SamplerState.AddressW = NewState->AddressW;
		}
		if (SamplerState.MagFilter != NewState->MagFilter)
		{
			glTexParameteri(Target, GL_TEXTURE_MAG_FILTER, NewState->MagFilter);
			SamplerState.MagFilter = NewState->MagFilter;
		}
		if (SamplerState.MinFilter != NewMinFilter)
		{
			glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, NewMinFilter);
			SamplerState.MinFilter = NewMinFilter;
		}
		if (SamplerState.MaxAnisotropy != NewState->MaxAnisotropy)
		{
			glTexParameteri(Target, GL_TEXTURE_MAX_ANISOTROPY_EXT, NewState->MaxAnisotropy);
			SamplerState.MaxAnisotropy = NewState->MaxAnisotropy;
		}
		if (BaseLevel != NewBaseLevel)
		{
			glTexParameteri(Target, GL_TEXTURE_BASE_LEVEL, NewBaseLevel);
			BaseLevel = NewBaseLevel;
		}
	}

private:

	FOpenGLDynamicRHI* OpenGLRHI;
	INT MemorySize;
	TArray< TRefCountPtr<FOpenGLPixelBuffer> > PixelBuffers;
	FOpenGLSamplerState SamplerState;
};

typedef TOpenGLTexture<RRT_Texture>			FOpenGLTexture;
typedef TOpenGLTexture<RRT_Texture2D>		FOpenGLTexture2D;
typedef TOpenGLTexture<RRT_Texture2DArray>	FOpenGLTexture2DArray;
typedef TOpenGLTexture<RRT_Texture3D>		FOpenGLTexture3D;
typedef TOpenGLTexture<RRT_TextureCube>		FOpenGLTextureCube;
typedef TOpenGLTexture<RRT_SharedTexture2D>	FOpenGLSharedTexture2D;
typedef TOpenGLTexture<RRT_SharedTexture2DArray>	FOpenGLSharedTexture2DArray;

class FOpenGLOcclusionQuery : public FRefCountedObject, public TDynamicRHIResource<RRT_OcclusionQuery>
{
public:

	/** The query resource. */
	GLuint Resource;

	/** The cached query result. */
	GLuint Result;

	/** TRUE if the query's result is cached. */
	UBOOL bResultIsCached : 1;

	/** Initialization constructor. */
	FOpenGLOcclusionQuery(GLuint InResource):
	Resource(InResource),
		bResultIsCached(FALSE)
	{}

	~FOpenGLOcclusionQuery()
	{
		if (Resource)
		{
			glDeleteQueries(1, &Resource);
		}
	}
};

typedef FRefCountedObject FOpenGLHullShader;
typedef FRefCountedObject FOpenGLDomainShader;
typedef FRefCountedObject FOpenGLSharedMemoryResource;
typedef FRefCountedObject FOpenGLGeometryShader;
typedef FRefCountedObject FOpenGLComputeShader;

template <typename FTextureType>
void OpenGLTextureDeleted( FTextureType& Texture )
{
}
void OpenGLTextureDeleted( FOpenGLTexture2D& Texture );
void OpenGLTextureAllocated( FOpenGLTexture2D& Texture );
