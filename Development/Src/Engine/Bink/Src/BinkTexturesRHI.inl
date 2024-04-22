#if USE_BINK_CODEC

typedef struct POS_TC_VERTEX
{
  F32 sx, sy, sz, rhw;  // Screen coordinates
  F32 tu, tv;           // Texture coordinates
} POS_TC_VERTEX;

//
// matrix to convert yuv to rgb
// not const - we change the final value to reflect global alpha
//

static float Gyuvtorgb[] =
{
   1.164123535f,  1.595794678f,  0.0f,         -0.87065506f,
   1.164123535f, -0.813476563f, -0.391448975f,  0.529705048f,
   1.164123535f,  0.0f,          2.017822266f, -1.081668854f,
   1.0f, 0.0f, 0.0f, 0.0f
};

//
// Bink shader definitions.
//

class FBinkYCrCbToRGBNoPixelAlphaPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FBinkYCrCbToRGBNoPixelAlphaPixelShader,Global);
public:

	FShaderResourceParameter tex0Parameter;
	FShaderResourceParameter tex1Parameter;
	FShaderResourceParameter tex2Parameter;
	FShaderParameter torParameter;
	FShaderParameter togParameter;
	FShaderParameter tobParameter;
	FShaderParameter constsParameter;

	/** Default constructor. */
	FBinkYCrCbToRGBNoPixelAlphaPixelShader() : FGlobalShader() {}

	/** Initialization constructor. */
	FBinkYCrCbToRGBNoPixelAlphaPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		tex0Parameter.Bind(Initializer.ParameterMap,TEXT("tex0"));
		tex1Parameter.Bind(Initializer.ParameterMap,TEXT("tex1"));
		tex2Parameter.Bind(Initializer.ParameterMap,TEXT("tex2"));
		torParameter.Bind(Initializer.ParameterMap,TEXT("tor"));
		togParameter.Bind(Initializer.ParameterMap,TEXT("tog"));
		tobParameter.Bind(Initializer.ParameterMap,TEXT("tob"));
		constsParameter.Bind(Initializer.ParameterMap,TEXT("consts"));
	}

	// FShader INTerface.
	virtual UBOOL Serialize(FArchive& Ar)
	{
		Ar << tex0Parameter << tex1Parameter << tex2Parameter << torParameter << togParameter << tobParameter << constsParameter;
		return FGlobalShader::Serialize(Ar);
	}
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}
};

class FBinkYCrCbAToRGBAPixelShader : public FBinkYCrCbToRGBNoPixelAlphaPixelShader
{
	DECLARE_SHADER_TYPE(FBinkYCrCbAToRGBAPixelShader,Global);
public:

	FShaderResourceParameter tex3Parameter;

	/** Default constructor. */
	FBinkYCrCbAToRGBAPixelShader() : FBinkYCrCbToRGBNoPixelAlphaPixelShader() {}

	/** Initialization constructor. */
	FBinkYCrCbAToRGBAPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FBinkYCrCbToRGBNoPixelAlphaPixelShader(Initializer)
	{
		tex3Parameter.Bind(Initializer.ParameterMap,TEXT("tex3"));
	}

	// FShader INTerface.
	virtual UBOOL Serialize(FArchive& Ar)
	{
		Ar << tex3Parameter;
		return FBinkYCrCbToRGBNoPixelAlphaPixelShader::Serialize(Ar);
	}
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}
};

class FBinkVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FBinkVertexShader,Global);
public:

	FBinkVertexShader() : FGlobalShader() {}

	FBinkVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{}

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}
};

IMPLEMENT_SHADER_TYPE(,FBinkYCrCbToRGBNoPixelAlphaPixelShader,TEXT("BinkShaders"),TEXT("YCrCbToRGBNoPixelAlpha"),SF_Pixel,VER_BINK_SHADER_SERIALIZATION_CHANGE,0);
IMPLEMENT_SHADER_TYPE(,FBinkYCrCbAToRGBAPixelShader,TEXT("BinkShaders"),TEXT("YCrCbAToRGBA"),SF_Pixel,VER_BINK_SHADER_SERIALIZATION_CHANGE,0);
IMPLEMENT_SHADER_TYPE(,FBinkVertexShader,TEXT("BinkShaders"),TEXT("VertexShaderMain"),SF_Vertex,VER_BINK_SHADER_SERIALIZATION_CHANGE,0);

/**
 * The Bink vertex declaration resource type.
 */
class FBinkVertexDeclaration : public FRenderResource
{
public:

	FVertexDeclarationRHIRef VertexDeclarationRHI;

	/** Destructor. */
	virtual ~FBinkVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		Elements.AddItem(FVertexElement(0,STRUCT_OFFSET(POS_TC_VERTEX,sx),VET_Float4,VEU_Position,0));
		Elements.AddItem(FVertexElement(0,STRUCT_OFFSET(POS_TC_VERTEX,tu),VET_Float2,VEU_TextureCoordinate,0));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

static TGlobalResource<FBinkVertexDeclaration> GBinkVertexDeclaration;

//############################################################################
//##                                                                        ##
//## Free the textures that we allocated.                                   ##
//##                                                                        ##
//############################################################################

void Free_Bink_textures( FBinkTextureSet * set_textures )
{
	set_textures->ReleaseResource();
}

void FBinkTextureSet::ReleaseDynamicRHI()
{
	BINKFRAMETEXTURES * abt[] = { &textures[ 0 ], &textures[ 1 ] };

	// Free the texture memory and then the textures directly
	for ( INT i = 0; i < ARRAY_COUNT(abt); ++i )
	{
		abt[ i ]->Ytexture.Empty();
		abt[ i ]->cRtexture.Empty();
		abt[ i ]->cBtexture.Empty();
		abt[ i ]->Atexture.Empty();
	}

	Ytexture.SafeRelease();
	cRtexture.SafeRelease();
	cBtexture.SafeRelease();
	Atexture.SafeRelease();
	bTriggerMovieRefresh = TRUE;
}


//############################################################################
//##                                                                        ##
//## Create 2 sets of textures for Bink to decompress INTo...               ##
//## Also does some basic sampler and render state init                     ##
//##                                                                        ##
//############################################################################

S32 Create_Bink_textures( FBinkTextureSet * set_textures )
{
	set_textures->InitResource();
	return 1;
}

void FBinkTextureSet::InitDynamicRHI()
{
	BINKFRAMETEXTURES * abt[] = { &textures[ 0 ], &textures[ 1 ] };
	BINKFRAMEBUFFERS* bb = &bink_buffers;

	//
	// Create system memory buffers for the frames being decoded.
	//

	for ( INT i = 0; i < ARRAY_COUNT(abt); ++i )
	{
		// Create Y plane
		if ( bb->Frames[ 0 ].YPlane.Allocate )
		{
			abt[i]->Ytexture.Empty(bb->YABufferWidth * bb->YABufferHeight);
			abt[i]->Ytexture.AddZeroed(bb->YABufferWidth * bb->YABufferHeight);
			bb->Frames[i].YPlane.Buffer = &abt[i]->Ytexture(0);
			bb->Frames[i].YPlane.BufferPitch = bb->YABufferWidth;
		}

		// Create cR plane
		if ( bb->Frames[ 0 ].cRPlane.Allocate )
		{
			abt[i]->cRtexture.Empty(bb->cRcBBufferWidth * bb->cRcBBufferHeight);
			abt[i]->cRtexture.AddZeroed(bb->cRcBBufferWidth * bb->cRcBBufferHeight);
			bb->Frames[i].cRPlane.Buffer = &abt[i]->cRtexture(0);
			bb->Frames[i].cRPlane.BufferPitch = bb->cRcBBufferWidth;
		}

		// Create cB plane
		if ( bb->Frames[ 0 ].cBPlane.Allocate )
		{
			abt[i]->cBtexture.Empty(bb->cRcBBufferWidth * bb->cRcBBufferHeight);
			abt[i]->cBtexture.AddZeroed(bb->cRcBBufferWidth * bb->cRcBBufferHeight);
			bb->Frames[i].cBPlane.Buffer = &abt[i]->cBtexture(0);
			bb->Frames[i].cBPlane.BufferPitch = bb->cRcBBufferWidth;
		}

		// Create alpha plane
		if ( bb->Frames[ 0 ].APlane.Allocate )
		{
			abt[i]->Atexture.Empty(bb->YABufferWidth * bb->YABufferHeight);
			abt[i]->Atexture.AddZeroed(bb->YABufferWidth * bb->YABufferHeight);
			bb->Frames[i].APlane.Buffer = &abt[i]->Atexture(0);
			bb->Frames[i].APlane.BufferPitch = bb->YABufferWidth;
		}
	}

	//
	// Create our output draw texture (this should be in video card memory)
	//

	// Create Y plane
	if ( bb->Frames[ 0 ].YPlane.Allocate )
	{
		Ytexture = RHICreateTexture2D( bb->YABufferWidth, bb->YABufferHeight, PF_G8, 1, TexCreate_Dynamic|TexCreate_NoTiling, NULL);
	}

	// Create cR plane
	if ( bb->Frames[ 0 ].cRPlane.Allocate )
	{
		cRtexture = RHICreateTexture2D( bb->cRcBBufferWidth, bb->cRcBBufferHeight, PF_G8, 1, TexCreate_Dynamic|TexCreate_NoTiling, NULL );
	}

	// Create cB plane
	if ( bb->Frames[ 0 ].cBPlane.Allocate )
	{
		cBtexture = RHICreateTexture2D( bb->cRcBBufferWidth, bb->cRcBBufferHeight, PF_G8, 1, TexCreate_Dynamic|TexCreate_NoTiling, NULL );
	}

	// Create alpha plane
	if ( bb->Frames[ 0 ].APlane.Allocate )
	{
		Atexture = RHICreateTexture2D( bb->YABufferWidth, bb->YABufferHeight, PF_G8, 1, TexCreate_Dynamic|TexCreate_NoTiling, NULL );
	}
}


//############################################################################
//##                                                                        ##
//## Lock Bink textures for use by D3D.                                     ##
//##                                                                        ##
//############################################################################

void Lock_Bink_textures( FBinkTextureSet * set_textures )
{

}

//############################################################################
//##                                                                        ##
//## Unlock Bink textures for use by D3D.                                   ##
//##                                                                        ##
//############################################################################

#if XBOX
#include <ppcintrinsics.h>

static void store_cache( void * ptr, U32 size )
{
	U8 * p, * end;

	p = (U8*) ( ( (UINTa) ptr ) & ~127 );
	end = (U8*) ( ( ( (UINTa) ptr ) + size + 127 ) & ~127 );

	while ( p < end )
	{
		__dcbst( 0, p );
		p += 128;
	}
}
#endif

static void CopyBufferToTexture(FTexture2DRHIRef Texture,const TArray<BYTE>& Buffer,const UINT Width,const UINT Height, UBOOL bFixupCache)
{
	UINT TextureStride = 0;
	BYTE* const TextureData = (BYTE*)RHILockTexture2D(Texture,0,TRUE,TextureStride,FALSE);

#if XBOX
	if( bFixupCache )
	{
		store_cache(TextureData,Height*TextureStride);
	}
#endif

	// Copy the texture data from the staging area into the output texture.
	for(UINT Y = 0;Y < Height;Y++)
	{
		appMemcpy(TextureData + Y * TextureStride,&Buffer(Y * Width),Width);
	}

	RHIUnlockTexture2D(Texture,0,FALSE);
}

void Unlock_Bink_textures( FBinkTextureSet * set_textures, HBINK Bink )
{
	INT CurrentFrame = set_textures->bink_buffers.FrameNum;
	INT NumRects = BinkGetRects( Bink, BINKSURFACEFAST );

	if ( NumRects > 0 )
	{
		CopyBufferToTexture(
			set_textures->Ytexture,
			set_textures->textures[CurrentFrame].Ytexture,
			set_textures->bink_buffers.YABufferWidth,
			set_textures->bink_buffers.YABufferHeight,
			FALSE
			);
		
		CopyBufferToTexture(
			set_textures->cRtexture,
			set_textures->textures[CurrentFrame].cRtexture,
			set_textures->bink_buffers.cRcBBufferWidth,
			set_textures->bink_buffers.cRcBBufferHeight,
			FALSE
			);
		
		CopyBufferToTexture(
			set_textures->cBtexture,
			set_textures->textures[CurrentFrame].cBtexture,
			set_textures->bink_buffers.cRcBBufferWidth,
			set_textures->bink_buffers.cRcBBufferHeight,
			FALSE
			);

		if(IsValidRef(set_textures->Atexture))
		{
			CopyBufferToTexture(
				set_textures->Atexture,
				set_textures->textures[CurrentFrame].Atexture,
				set_textures->bink_buffers.YABufferWidth,
				set_textures->bink_buffers.YABufferHeight,
				TRUE
				);
		}
	}
}

//############################################################################
//##                                                                        ##
//## Draw our textures onto the screen with our vertex and pixel shaders.   ##
//##                                                                        ##
//############################################################################

void Draw_Bink_textures( FBinkTextureSet * set_textures,
                         U32 width,
                         U32 height,
                         F32 x_offset,
                         F32 y_offset,
                         F32 x_scale,
                         F32 y_scale,
                         F32 alpha_level,
                         UBOOL is_premultiplied_alpha,
						 UBOOL bIsFullscreen)
{
#if DWTRIOVIZSDK	
	// trioviz uses alpha for depth, not blending, so disble alpha if needed
	UBOOL bOverrideAlpha = !is_premultiplied_alpha;//Fullscreen = is_premultiplied_alpha == FALSE
	if (bOverrideAlpha && IsValidRef(set_textures->Atexture))
	{
		alpha_level = 1.0f;
	}
#endif

	TShaderMapRef<FBinkVertexShader> VertexShader(GetGlobalShaderMap());

	RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());

	
	BINKFRAMETEXTURES * bt_src = &set_textures->textures[ set_textures->bink_buffers.FrameNum ];

	FSamplerStateRHIRef SamplerStateRHI = TStaticSamplerState<SF_Bilinear>::GetRHI();
	FBinkYCrCbToRGBNoPixelAlphaPixelShader* BasePixelShader;
	if(IsValidRef(set_textures->Atexture))
	{
		TShaderMapRef<FBinkYCrCbAToRGBAPixelShader> PixelShader(GetGlobalShaderMap());
		SetTextureParameter(PixelShader->GetPixelShader(),PixelShader->tex3Parameter,SamplerStateRHI,set_textures->Atexture);
		BasePixelShader = *PixelShader;
	}
	else
	{
		TShaderMapRef<FBinkYCrCbToRGBNoPixelAlphaPixelShader> PixelShader(GetGlobalShaderMap());
		BasePixelShader = *PixelShader;
	}

	DWORD StreamStrides[MaxVertexElementCount];
	appMemzero(StreamStrides,sizeof(StreamStrides));
	StreamStrides[0] = sizeof(POS_TC_VERTEX);

	RHISetBoundShaderState(
		RHICreateBoundShaderState(GBinkVertexDeclaration.VertexDeclarationRHI,StreamStrides,VertexShader->GetVertexShader(),BasePixelShader->GetPixelShader(),EGST_None)
		);

	SetTextureParameter(BasePixelShader->GetPixelShader(),BasePixelShader->tex0Parameter,SamplerStateRHI,set_textures->Ytexture);
	SetTextureParameter(BasePixelShader->GetPixelShader(),BasePixelShader->tex1Parameter,SamplerStateRHI,set_textures->cRtexture);
	SetTextureParameter(BasePixelShader->GetPixelShader(),BasePixelShader->tex2Parameter,SamplerStateRHI,set_textures->cBtexture);

	//
	// upload the YUV to RGB matrix
	//

	Gyuvtorgb[15] = is_premultiplied_alpha ? alpha_level : 1.0;
	SetPixelShaderValue(BasePixelShader->GetPixelShader(),BasePixelShader->torParameter,*(FVector4*)&Gyuvtorgb[0],0);
	SetPixelShaderValue(BasePixelShader->GetPixelShader(),BasePixelShader->togParameter,*(FVector4*)&Gyuvtorgb[4],0);
	SetPixelShaderValue(BasePixelShader->GetPixelShader(),BasePixelShader->tobParameter,*(FVector4*)&Gyuvtorgb[8],0);
	SetPixelShaderValue(BasePixelShader->GetPixelShader(),BasePixelShader->constsParameter,*(FVector4*)&Gyuvtorgb[12],0);

#if DWTRIOVIZSDK	
	// disable blending when overriding alpha
	if (bOverrideAlpha && IsValidRef(set_textures->Atexture))
	{
		RHISetBlendState(TStaticBlendState<>::GetRHI());
	}
	else
#endif
	// for render-to-texture movies, we don't need to blend here, as the texture/material will do the 
	// normal UE3 blending; we just need to pass alpha through
	if (!bIsFullscreen || alpha_level >= 0.999f || !IsValidRef(set_textures->Atexture))
	{
		RHISetBlendState(TStaticBlendState<>::GetRHI());
	}
	else
	{
		if(alpha_level < 0.999f )
		{
			if (is_premultiplied_alpha)
			{
				RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_Zero>::GetRHI());
			}
			else
			{
				RHISetBlendState(TStaticBlendState<BO_Add,BF_SourceAlpha,BF_Zero>::GetRHI());		
			}		
		}
	}

	//
	// Setup up the vertices.
	//

	POS_TC_VERTEX vertices[ 4 ];

	vertices[ 0 ].sx = x_offset;
	vertices[ 0 ].sy = y_offset;
	vertices[ 0 ].sz = 0.0f;
	vertices[ 0 ].rhw = 1.0f;
	vertices[ 0 ].tu = 0.0f;
	vertices[ 0 ].tv = 0.0f;
	vertices[ 1 ] = vertices[ 0 ];
	vertices[ 1 ].sx = x_offset + ( ( (F32)(S32) width ) * x_scale );
	vertices[ 1 ].tu = 1.0f;
	vertices[ 2 ] = vertices[0];
	vertices[ 2 ].sy = y_offset + ( ( (F32)(S32) height ) * y_scale );
	vertices[ 2 ].tv = 1.0f;
	vertices[ 3 ] = vertices[ 1 ];
	vertices[ 3 ].sy = vertices[ 2 ].sy;
	vertices[ 3 ].tv = 1.0f;

	RHIDrawPrimitiveUP(PT_TriangleStrip,2,vertices,sizeof(vertices[0]));
}

#endif //USE_BINK_CODEC
