/*=============================================================================
	UnRenderUtils.cpp: Rendering utility classes.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"

/** Global realtime clock for the rendering thread. */
FTimer GRenderingRealtimeClock;

/** Whether to pause the global realtime clock for the rendering thread. */
UBOOL GPauseRenderingRealtimeClock = FALSE;

/** Global vertex color view mode setting when SHOW_VertexColors show flag is set */
EVertexColorViewMode::Type GVertexColorViewMode = EVertexColorViewMode::Color;


/** X=127.5, Y=127.5, Z=1/127.5f, W=-1.0 */
const VectorRegister GVectorPackingConstants = MakeVectorRegister( 127.5f, 127.5f, 1.0f/127.5f, -1.0f );

/** Zero Normal **/
FPackedNormal FPackedNormal::ZeroNormal(127, 127, 127, 127);
/**
 * operator FVector
 */
FPackedNormal::operator FVector() const
{
	VectorRegister VectorToUnpack = GetVectorRegister();
	// Write to FVector and return it.
	FVector UnpackedVector;
	VectorStoreFloat3( VectorToUnpack, &UnpackedVector );
	return UnpackedVector;
}

/**
 * operator VectorRegister
 */
VectorRegister FPackedNormal::GetVectorRegister() const
{
	// Rescale [0..255] range to [-1..1]
#if XBOX
	// make sure to swap order of packed bytes on Xe
	VectorRegister VectorToUnpack		= VectorLoadByte4Reverse( this );
#else
	VectorRegister VectorToUnpack		= VectorLoadByte4( this );
#endif
	VectorToUnpack						= VectorMultiplyAdd( VectorToUnpack, VectorReplicate(GVectorPackingConstants,2), VectorReplicate(GVectorPackingConstants,3) );
	VectorResetFloatRegisters();
	// Return unpacked vector register.
	return VectorToUnpack;
}

//
// FPackedNormal serializer
//
FArchive& operator<<(FArchive& Ar,FPackedNormal& N)
{
#if !CONSOLE
	if ( (GCookingTarget & (UE3::PLATFORM_PS3 | UE3::PLATFORM_WiiU)) && Ar.IsSaving() && Ar.ForceByteSwapping() )
	{
		// PS3 GPU data uses Intel byte order but the FArchive is byte-swapping. Serialize it with explicit byte order.
		Ar << N.Vector.X << N.Vector.Y << N.Vector.Z << N.Vector.W;
	}
	else
#endif
	{
		Ar << N.Vector.Packed;
	}
	return Ar;
}

//
//	Pixel format information.
//

// NOTE: If you add a new basic texture format (ie a format that could be cooked - currently PF_A32B32G32R32F through
// PF_UYVY) you MUST also update XeTools.cpp and PS3Tools.cpp to match up!
FPixelFormatInfo	GPixelFormats[PF_MAX] =
{
	// Name						BlockSizeX	BlockSizeY	BlockSizeZ	BlockBytes	NumComponents	PlatformFormat	Flags			Supported		UnrealFormat

	{ TEXT("unknown"),			0,			0,			0,			0,			0,				0,				0,				0,				PF_Unknown			},
	{ TEXT("A32B32G32R32F"),	1,			1,			1,			16,			4,				0,				0,				1,				PF_A32B32G32R32F	},
	{ TEXT("A8R8G8B8"),			1,			1,			1,			4,			4,				0,				0,				1,				PF_A8R8G8B8			},
	{ TEXT("G8"),				1,			1,			1,			1,			1,				0,				0,				1,				PF_G8				},
	{ TEXT("G16"),				1,			1,			1,			2,			1,				0,				0,				1,				PF_G16				},
	{ TEXT("DXT1"),				4,			4,			1,			8,			3,				0,				0,				1,				PF_DXT1				},
	{ TEXT("DXT3"),				4,			4,			1,			16,			4,				0,				0,				1,				PF_DXT3				},
	{ TEXT("DXT5"),				4,			4,			1,			16,			4,				0,				0,				1,				PF_DXT5				},
	{ TEXT("UYVY"),				2,			1,			1,			4,			4,				0,				0,				0,				PF_UYVY				},
	{ TEXT("FloatRGB"),			1,			1,			1,			0,			3,				0,				0,				0,				PF_FloatRGB			},
	{ TEXT("FloatRGBA"),		1,			1,			1,			0,			4,				0,				0,				0,				PF_FloatRGBA		},
	{ TEXT("DepthStencil"),		1,			1,			1,			0,			1,				0,				0,				0,				PF_DepthStencil		},
	{ TEXT("ShadowDepth"),		1,			1,			1,			4,			1,				0,				0,				0,				PF_ShadowDepth		},
	{ TEXT("FilteredShadowDepth"),1,		1,			1,			4,			1,				0,				0,				0,				PF_FilteredShadowDepth },
	{ TEXT("R32F"),				1,			1,			1,			4,			1,				0,				0,				1,				PF_R32F				},
	{ TEXT("G16R16"),			1,			1,			1,			4,			2,				0,				0,				1,				PF_G16R16			},
	{ TEXT("G16R16F"),			1,			1,			1,			4,			2,				0,				0,				1,				PF_G16R16F			},
	{ TEXT("G16R16F_FILTER"),	1,			1,			1,			4,			2,				0,				0,				1,				PF_G16R16F_FILTER	},
	{ TEXT("G32R32F"),			1,			1,			1,			8,			2,				0,				0,				1,				PF_G32R32F			},
	{ TEXT("A2B10G10R10"),      1,          1,          1,          4,          4,              0,              0,              1,				PF_A2B10G10R10		},
	{ TEXT("A16B16G16R16"),		1,			1,			1,			8,			4,				0,				0,				1,				PF_A16B16G16R16		},
	{ TEXT("D24"),				1,			1,			1,			4,			1,				0,				0,				1,				PF_D24				},
	{ TEXT("PF_R16F"),			1,			1,			1,			2,			1,				0,				0,				1,				PF_R16F				},
	{ TEXT("PF_R16F_FILTER"),	1,			1,			1,			2,			1,				0,				0,				1,				PF_R16F_FILTER		},
	{ TEXT("BC5"),				4,			4,			1,			16,			2,				0,				0,				1,				PF_BC5				},
	{ TEXT("V8U8"),				1,			1,			1,			2,			2,				0,				0,				1,				PF_V8U8				},
	{ TEXT("A1"),				1,			1,			1,			1,			1,				0,				0,				0,				PF_A1				},
	{ TEXT("FloatR11G11B10"),	1,			1,			1,			0,			3,				0,				0,				0,				PF_FloatR11G11B10	},
	{ TEXT("PF_A4R4G4B4"),		1,			1,			1,			2,			4,				0,				0,				0,				PF_A4R4G4B4			},
	{ TEXT("PF_R5G6B5"),		1,			1,			1,			2,			3,				0,				0,				0,				PF_R5G6B5			}
};

void ValidatePixelFormats()
{
	for (INT X = 0; X < ARRAY_COUNT(GPixelFormats); ++X)
	{
		// Make sure GPixelFormats has an entry for every unreal format
		check(X == GPixelFormats[X].UnrealFormat);
	}
}

//
//	CalculateImageBytes
//

SIZE_T CalculateImageBytes(DWORD SizeX,DWORD SizeY,DWORD SizeZ,BYTE Format)
{
	if ( Format == PF_A1 )
	{
		// The number of bytes needed to store all 1 bit pixels in a line is the width of the image divided by the number of bits in a byte
		DWORD BytesPerLine = SizeX / 8;
		// The number of actual bytes in a 1 bit image is the bytes per line of pixels times the number of lines
		return sizeof(BYTE) * BytesPerLine * SizeY;
	}
	else if( SizeZ > 0 )
	{
		return (SizeX / GPixelFormats[Format].BlockSizeX) * (SizeY / GPixelFormats[Format].BlockSizeY) * (SizeZ / GPixelFormats[Format].BlockSizeZ) * GPixelFormats[Format].BlockBytes;
	}
	else
	{	
		return (SizeX / GPixelFormats[Format].BlockSizeX) * (SizeY / GPixelFormats[Format].BlockSizeY) * GPixelFormats[Format].BlockBytes;
	}
}

//
// FWhiteTexture implementation
//

/**
 * A solid-colored 1x1 texture.
 */
template <INT R, INT G, INT B, INT A>
class FColoredTexture : public FTextureResource
{
public:
	// FResource interface.
	virtual void InitRHI()
	{
		// Create the texture RHI.  		
		FTexture2DRHIRef Texture2D = RHICreateTexture2D(1,1,PF_A8R8G8B8,1,TexCreate_Uncooked,NULL);
		TextureRHI = Texture2D;

		// Write the contents of the texture.
		UINT DestStride;
		FColor* DestBuffer = (FColor*)RHILockTexture2D(Texture2D,0,TRUE,DestStride,FALSE);
		*DestBuffer = FColor(R, G, B, A);
		RHIUnlockTexture2D(Texture2D,0,FALSE);

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point,AM_Wrap,AM_Wrap,AM_Wrap);
		SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
	}

	/** Returns the width of the texture in pixels. */
	virtual UINT GetSizeX() const
	{
		return 1;
	}

	/** Returns the height of the texture in pixels. */
	virtual UINT GetSizeY() const
	{
		return 1;
	}
};

FTexture* GWhiteTexture = new TGlobalResource<FColoredTexture<255,255,255,255> >;
FTexture* GBlackTexture = new TGlobalResource<FColoredTexture<0,0,0,255> >;

class FBlackArrayTexture : public FTextureResource
{
public:
	// FResource interface.
	virtual void InitRHI()
	{
#if PLATFORM_SUPPORTS_D3D10_PLUS
		if(GRHIShaderPlatform == SP_PCD3D_SM5)
		{
			// Create the texture RHI.  		
			FTexture2DArrayRHIRef TextureArray = RHICreateTexture2DArray(1,1,1,PF_A8R8G8B8,1,TexCreate_Uncooked,NULL);
			TextureRHI = TextureArray;

			UINT DestStride;
			FColor* DestBuffer = (FColor*)RHILockTexture2DArray(TextureArray, 0, 0, TRUE, DestStride, FALSE);
			*DestBuffer = FColor(0, 0, 0, 0);
			RHIUnlockTexture2DArray(TextureArray, 0, 0, FALSE);

			// Create the sampler state RHI resource.
			FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point,AM_Wrap,AM_Wrap,AM_Wrap);
			SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
		}
#endif
	}

	/** Returns the width of the texture in pixels. */
	virtual UINT GetSizeX() const
	{
		return 1;
	}

	/** Returns the height of the texture in pixels. */
	virtual UINT GetSizeY() const
	{
		return 1;
	}
};

FTexture* GBlackArrayTexture = new TGlobalResource<FBlackArrayTexture>;

//
// FMipColorTexture implementation
//

/**
 * A texture that has a different solid color in each mip-level
 */
class FMipColorTexture : public FTextureResource
{
public:
	enum
	{
		NumMips = 12
	};
	static const FColor MipColors[NumMips];

	// FResource interface.
	virtual void InitRHI()
	{
		// Create the texture RHI.
		INT TextureSize = 1 << (NumMips - 1);
		FTexture2DRHIRef Texture2D = RHICreateTexture2D(TextureSize,TextureSize,PF_A8R8G8B8,NumMips,TexCreate_Uncooked,NULL);
		TextureRHI = Texture2D;

		// Write the contents of the texture.
		UINT DestStride;
		INT Size = TextureSize;
		for ( INT MipIndex=0; MipIndex < NumMips; ++MipIndex )
		{
			FColor* DestBuffer = (FColor*) RHILockTexture2D(Texture2D,MipIndex,TRUE,DestStride,FALSE);
			for ( INT Y=0; Y < Size; ++Y )
			{
				for ( INT X=0; X < Size; ++X )
				{
					DestBuffer[X] = MipColors[NumMips - 1 - MipIndex];
				}
				DestBuffer += DestStride / sizeof(FColor);
			}
			RHIUnlockTexture2D(Texture2D,MipIndex,FALSE);
			Size >>= 1;
		}

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point,AM_Wrap,AM_Wrap,AM_Wrap);
		SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
	}

	/** Returns the width of the texture in pixels. */
	virtual UINT GetSizeX() const
	{
		INT TextureSize = 1 << (NumMips - 1);
		return TextureSize;
	}

	/** Returns the height of the texture in pixels. */
	virtual UINT GetSizeY() const
	{
		INT TextureSize = 1 << (NumMips - 1);
		return TextureSize;
	}
};

const FColor FMipColorTexture::MipColors[NumMips] =
{
	FColor(  80,  80,  80, 0 ),		// Mip  0: 1x1			(dark grey)
	FColor( 200, 200, 200, 0 ),		// Mip  1: 2x2			(light grey)
	FColor( 200, 200,   0, 0 ),		// Mip  2: 4x4			(medium yellow)
	FColor( 255, 255,   0, 0 ),		// Mip  3: 8x8			(yellow)
	FColor( 160, 255,  40, 0 ),		// Mip  4: 16x16		(light green)
	FColor(   0, 255,   0, 0 ),		// Mip  5: 32x32		(green)
	FColor(   0, 255, 200, 0 ),		// Mip  6: 64x64		(cyan)
	FColor(   0, 170, 170, 0 ),		// Mip  7: 128x128		(light blue)
	FColor(  60,  60, 255, 0 ),		// Mip  8: 256x256		(dark blue)
	FColor( 255,   0, 255, 0 ),		// Mip  9: 512x512		(pink)
	FColor( 255,   0,   0, 0 ),		// Mip 10: 1024x1024	(red)
	FColor( 255, 130,   0, 0 ),		// Mip 11: 2048x2048	(orange)
};

/** A global texture that has a different solid color in each mip-level. */
FTexture* GMipColorTexture = new FMipColorTexture;
/** Number of mip-levels in 'GMipColorTexture' */
INT GMipColorTextureMipLevels = FMipColorTexture::NumMips;

//
// FWhiteTextureCube implementation
//

/**
 * A solid white cube texture.
 */
class FWhiteTextureCube : public FTextureResource
{
public:

	// FResource interface.
	virtual void InitRHI()
	{
		// Create the texture RHI.  		
#if XBOX
		// make it ResolveTargetable so it is created on Xenon using the D3D functions instead of XG; otherwise
		// it shows up as a 1x1 black texture
		DWORD CreationFlags=TexCreate_ResolveTargetable;
#else
		DWORD CreationFlags=0;
#endif
		FTextureCubeRHIRef TextureCube = RHICreateTextureCube(1,PF_A8R8G8B8,1,CreationFlags,NULL);
		TextureRHI = TextureCube;

		// Write the contents of the texture.
		for(UINT FaceIndex = 0;FaceIndex < 6;FaceIndex++)
		{
			UINT DestStride;
			FColor* DestBuffer = (FColor*)RHILockTextureCubeFace(TextureCube,FaceIndex,0,TRUE,DestStride,FALSE);
			*DestBuffer = FColor(255,255,255);
			RHIUnlockTextureCubeFace(TextureCube,FaceIndex,0,FALSE);
		}

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point,AM_Wrap,AM_Wrap,AM_Wrap);
		SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
	}

	/** Returns the width of the texture in pixels. */
	virtual UINT GetSizeX() const
	{
		return 1;
	}

	/** Returns the height of the texture in pixels. */
	virtual UINT GetSizeY() const
	{
		return 1;
	}
};

FTexture* GWhiteTextureCube = new TGlobalResource<FWhiteTextureCube>;

//
// Primitive drawing utility functions.
//

void DrawBox(FPrimitiveDrawInterface* PDI,const FMatrix& BoxToWorld,const FVector& Radii,const FMaterialRenderProxy* MaterialRenderProxy,BYTE DepthPriorityGroup)
{
	// Calculate verts for a face pointing down Z
	FVector Positions[4] =
	{
		FVector(-1, -1, +1),
		FVector(-1, +1, +1),
		FVector(+1, +1, +1),
		FVector(+1, -1, +1)
	};
	FVector2D UVs[4] =
	{
		FVector2D(0,0),
		FVector2D(0,1),
		FVector2D(1,1),
		FVector2D(1,0),
	};

	// Then rotate this face 6 times
	FRotator FaceRotations[6];
	FaceRotations[0] = FRotator(0,		0,	0);
	FaceRotations[1] = FRotator(16384,	0,	0);
	FaceRotations[2] = FRotator(-16384,	0,  0);
	FaceRotations[3] = FRotator(0,		0,	16384);
	FaceRotations[4] = FRotator(0,		0,	-16384);
	FaceRotations[5] = FRotator(32768,	0,	0);

	FDynamicMeshBuilder MeshBuilder;

	for(INT f=0; f<6; f++)
	{
		FMatrix FaceTransform = FRotationMatrix(FaceRotations[f]);

		INT VertexIndices[4];
		for(INT VertexIndex = 0;VertexIndex < 4;VertexIndex++)
		{
			VertexIndices[VertexIndex] = MeshBuilder.AddVertex(
				FaceTransform.TransformFVector( Positions[VertexIndex] ),
				UVs[VertexIndex],
				FaceTransform.TransformNormal(FVector(1,0,0)),
				FaceTransform.TransformNormal(FVector(0,1,0)),
				FaceTransform.TransformNormal(FVector(0,0,1)),
				FColor(255,255,255)
				);
		}

		MeshBuilder.AddTriangle(VertexIndices[0],VertexIndices[1],VertexIndices[2]);
		MeshBuilder.AddTriangle(VertexIndices[0],VertexIndices[2],VertexIndices[3]);
	}

	MeshBuilder.Draw(PDI,FScaleMatrix(Radii) * BoxToWorld,MaterialRenderProxy,DepthPriorityGroup,0.f);
}



void DrawSphere(FPrimitiveDrawInterface* PDI,const FVector& Center,const FVector& Radii,INT NumSides,INT NumRings,const FMaterialRenderProxy* MaterialRenderProxy,BYTE DepthPriority,UBOOL bDisableBackfaceCulling)
{
	// Use a mesh builder to draw the sphere.
	FDynamicMeshBuilder MeshBuilder;
	{
		// The first/last arc are on top of each other.
		INT NumVerts = (NumSides+1) * (NumRings+1);
		FDynamicMeshVertex* Verts = (FDynamicMeshVertex*)appMalloc( NumVerts * sizeof(FDynamicMeshVertex) );

		// Calculate verts for one arc
		FDynamicMeshVertex* ArcVerts = (FDynamicMeshVertex*)appMalloc( (NumRings+1) * sizeof(FDynamicMeshVertex) );

		for(INT i=0; i<NumRings+1; i++)
		{
			FDynamicMeshVertex* ArcVert = &ArcVerts[i];

			FLOAT angle = ((FLOAT)i/NumRings) * PI;

			// Note- unit sphere, so position always has mag of one. We can just use it for normal!			
			ArcVert->Position.X = 0.0f;
			ArcVert->Position.Y = appSin(angle);
			ArcVert->Position.Z = appCos(angle);

			ArcVert->SetTangents(
				FVector(1,0,0),
				FVector(0.0f,-ArcVert->Position.Z,ArcVert->Position.Y),
				ArcVert->Position
				);

			ArcVert->TextureCoordinate.X = 0.0f;
			ArcVert->TextureCoordinate.Y = ((FLOAT)i/NumRings);
		}

		// Then rotate this arc NumSides+1 times.
		for(INT s=0; s<NumSides+1; s++)
		{
			FRotator ArcRotator(0, appTrunc(65536.f * ((FLOAT)s/NumSides)), 0);
			FRotationMatrix ArcRot( ArcRotator );
			FLOAT XTexCoord = ((FLOAT)s/NumSides);

			for(INT v=0; v<NumRings+1; v++)
			{
				INT VIx = (NumRings+1)*s + v;

				Verts[VIx].Position = ArcRot.TransformFVector( ArcVerts[v].Position );
				
				Verts[VIx].SetTangents(
					ArcRot.TransformNormal( ArcVerts[v].TangentX ),
					ArcRot.TransformNormal( ArcVerts[v].GetTangentY() ),
					ArcRot.TransformNormal( ArcVerts[v].TangentZ )
					);

				Verts[VIx].TextureCoordinate.X = XTexCoord;
				Verts[VIx].TextureCoordinate.Y = ArcVerts[v].TextureCoordinate.Y;
			}
		}

		// Add all of the vertices we generated to the mesh builder.
		for(INT VertIdx=0; VertIdx < NumVerts; VertIdx++)
		{
			MeshBuilder.AddVertex(Verts[VertIdx]);
		}
		
		// Add all of the triangles we generated to the mesh builder.
		for(INT s=0; s<NumSides; s++)
		{
			INT a0start = (s+0) * (NumRings+1);
			INT a1start = (s+1) * (NumRings+1);

			for(INT r=0; r<NumRings; r++)
			{
				MeshBuilder.AddTriangle(a0start + r + 0, a1start + r + 0, a0start + r + 1);
				MeshBuilder.AddTriangle(a1start + r + 0, a1start + r + 1, a0start + r + 1);
			}
		}

		// Free our local copy of verts and arc verts
		appFree(Verts);
		appFree(ArcVerts);
	}
	MeshBuilder.Draw(PDI, FScaleMatrix( Radii ) * FTranslationMatrix( Center ), MaterialRenderProxy, DepthPriority,0.f,bDisableBackfaceCulling);
}

void DrawCone(FPrimitiveDrawInterface* PDI,const FMatrix& ConeToWorld, FLOAT Angle1, FLOAT Angle2, INT NumSides, UBOOL bDrawSideLines, const FColor& SideLineColor, const FMaterialRenderProxy* MaterialRenderProxy, BYTE DepthPriority)
{
	FLOAT ang1 = Clamp<FLOAT>(Angle1, 0.01f, (FLOAT)PI - 0.01f);
	FLOAT ang2 = Clamp<FLOAT>(Angle2, 0.01f, (FLOAT)PI - 0.01f);

	FLOAT sinX_2 = appSin(0.5f * ang1);
	FLOAT sinY_2 = appSin(0.5f * ang2);

	FLOAT sinSqX_2 = sinX_2 * sinX_2;
	FLOAT sinSqY_2 = sinY_2 * sinY_2;

	FLOAT tanX_2 = appTan(0.5f * ang1);
	FLOAT tanY_2 = appTan(0.5f * ang2);

	TArray<FVector> ConeVerts(NumSides);

	for(INT i = 0; i < NumSides; i++)
	{
		FLOAT Fraction = (FLOAT)i/(FLOAT)(NumSides);
		FLOAT thi = 2.f*PI*Fraction;
		FLOAT phi = appAtan2(appSin(thi)*sinY_2, appCos(thi)*sinX_2);
		FLOAT sinPhi = appSin(phi);
		FLOAT cosPhi = appCos(phi);
		FLOAT sinSqPhi = sinPhi*sinPhi;
		FLOAT cosSqPhi = cosPhi*cosPhi;

		FLOAT rSq, r, Sqr, alpha, beta;

		rSq = sinSqX_2*sinSqY_2/(sinSqX_2*sinSqPhi + sinSqY_2*cosSqPhi);
		r = appSqrt(rSq);
		Sqr = appSqrt(1-rSq);
		alpha = r*cosPhi;
		beta  = r*sinPhi;

		ConeVerts(i).X = (1-2*rSq);
		ConeVerts(i).Y = 2*Sqr*alpha;
		ConeVerts(i).Z = 2*Sqr*beta;
	}

	FDynamicMeshBuilder MeshBuilder;
	{
		for(INT i=0; i < NumSides; i++)
		{
			FDynamicMeshVertex V0, V1, V2;

			FVector TriTangentZ = ConeVerts((i+1)%NumSides) ^ ConeVerts( i ); // aka triangle normal
			FVector TriTangentY = ConeVerts(i);
			FVector TriTangentX = TriTangentZ ^ TriTangentY;

			V0.Position = FVector(0);
			V0.TextureCoordinate.X = 0.0f;
			V0.TextureCoordinate.Y = (FLOAT)i/NumSides;
			V0.SetTangents(TriTangentX,TriTangentY,TriTangentZ);

			V1.Position = ConeVerts(i);
			V1.TextureCoordinate.X = 1.0f;
			V1.TextureCoordinate.Y = (FLOAT)i/NumSides;
			V1.SetTangents(TriTangentX,TriTangentY,TriTangentZ);

			V2.Position = ConeVerts((i+1)%NumSides);
			V2.TextureCoordinate.X = 1.0f;
			V2.TextureCoordinate.Y = (FLOAT)((i+1)%NumSides)/NumSides;
			V2.SetTangents(TriTangentX,TriTangentY,TriTangentZ);

			const INT VertexStart = MeshBuilder.AddVertex(V0);
			MeshBuilder.AddVertex(V1);
			MeshBuilder.AddVertex(V2);

			MeshBuilder.AddTriangle(VertexStart,VertexStart+1,VertexStart+2);
		}
	}
	MeshBuilder.Draw(PDI, ConeToWorld, MaterialRenderProxy, DepthPriority,0.f);


	if(bDrawSideLines)
	{
		// Draw lines down major directions
		for(INT i=0; i<4; i++)
		{
			PDI->DrawLine( ConeToWorld.GetOrigin(), ConeToWorld.TransformFVector( ConeVerts( (i*NumSides/4)%NumSides ) ), SideLineColor, DepthPriority );
		}
	}
}


void DrawCylinder(FPrimitiveDrawInterface* PDI,const FVector& Base, const FVector& XAxis, const FVector& YAxis, const FVector& ZAxis,
	FLOAT Radius, FLOAT HalfHeight, INT Sides, const FMaterialRenderProxy* MaterialRenderProxy, BYTE DepthPriority)
{
	const FLOAT	AngleDelta = 2.0f * PI / Sides;
	FVector	LastVertex = Base + XAxis * Radius;

	FVector2D TC = FVector2D(0.0f, 0.0f);
	FLOAT TCStep = 1.0f / Sides;

	FVector TopOffset = HalfHeight * ZAxis;

	FDynamicMeshBuilder MeshBuilder;


	//Compute vertices for base circle.
	for(INT SideIndex = 0;SideIndex < Sides;SideIndex++)
	{
		const FVector Vertex = Base + (XAxis * appCos(AngleDelta * (SideIndex + 1)) + YAxis * appSin(AngleDelta * (SideIndex + 1))) * Radius;
		FVector Normal = Vertex - Base;
		Normal.Normalize();

		FDynamicMeshVertex MeshVertex;

		MeshVertex.Position = Vertex - TopOffset;
		MeshVertex.TextureCoordinate = TC;

		MeshVertex.SetTangents(
			-ZAxis,
			(-ZAxis) ^ Normal,
			Normal
			);

		MeshBuilder.AddVertex(MeshVertex); //Add bottom vertex

		LastVertex = Vertex;
		TC.X += TCStep;
	}

	LastVertex = Base + XAxis * Radius;
	TC = FVector2D(0.0f, 1.0f);

	//Compute vertices for the top circle
	for(INT SideIndex = 0;SideIndex < Sides;SideIndex++)
	{
		const FVector Vertex = Base + (XAxis * appCos(AngleDelta * (SideIndex + 1)) + YAxis * appSin(AngleDelta * (SideIndex + 1))) * Radius;
		FVector Normal = Vertex - Base;
		Normal.Normalize();

		FDynamicMeshVertex MeshVertex;

		MeshVertex.Position = Vertex + TopOffset;
		MeshVertex.TextureCoordinate = TC;

		MeshVertex.SetTangents(
			-ZAxis,
			(-ZAxis) ^ Normal,
			Normal
			);

		MeshBuilder.AddVertex(MeshVertex); //Add top vertex

		LastVertex = Vertex;
		TC.X += TCStep;
	}
	
	//Add top/bottom triangles, in the style of a fan.
	//Note if we wanted nice rendering of the caps then we need to duplicate the vertices and modify
	//texture/tangent coordinates.
	for(INT SideIndex = 1; SideIndex < Sides; SideIndex++)
	{
		INT V0 = 0;
		INT V1 = SideIndex;
		INT V2 = (SideIndex + 1) % Sides;

		MeshBuilder.AddTriangle(V0, V1, V2); //bottom
		MeshBuilder.AddTriangle(Sides + V2, Sides + V1 , Sides + V0); //top
	}

	//Add sides.

	for(INT SideIndex = 0; SideIndex < Sides; SideIndex++)
	{
		INT V0 = SideIndex;
		INT V1 = (SideIndex + 1) % Sides;
		INT V2 = V0 + Sides;
		INT V3 = V1 + Sides;

		MeshBuilder.AddTriangle(V0, V2, V1);
		MeshBuilder.AddTriangle(V2, V3, V1);
	}

	MeshBuilder.Draw(PDI, FMatrix::Identity, MaterialRenderProxy, DepthPriority,0.f);
}

/**
 * Draws a circle using triangles.
 *
 * @param	PDI						Draw interface.
 * @param	Base					Center of the circle.
 * @param	XAxis					X alignment axis to draw along.
 * @param	YAxis					Y alignment axis to draw along.
 * @param	Color					Color of the circle.
 * @param	Radius					Radius of the circle.
 * @param	NumSides				Numbers of sides that the circle has.
 * @param	MaterialRenderProxy		Material to use for render 
 * @param	DepthPriority			Depth priority for the circle.
 */
void DrawDisc(class FPrimitiveDrawInterface* PDI,const FVector& Base,const FVector& XAxis,const FVector& YAxis,FColor Color,FLOAT Radius,INT NumSides,const FMaterialRenderProxy* MaterialRenderProxy, BYTE DepthPriority)
{
	check (NumSides >= 3);

	const FLOAT	AngleDelta = 2.0f * PI / NumSides;

	FVector2D TC = FVector2D(0.0f, 0.0f);
	FLOAT TCStep = 1.0f / NumSides;
	
	FVector ZAxis = (XAxis) ^ YAxis;

	FDynamicMeshBuilder MeshBuilder;

	//Compute vertices for base circle.
	for(INT SideIndex = 0;SideIndex < NumSides;SideIndex++)
	{
		const FVector Vertex = Base + (XAxis * appCos(AngleDelta * (SideIndex)) + YAxis * appSin(AngleDelta * (SideIndex))) * Radius;
		FVector Normal = Vertex - Base;
		Normal.Normalize();

		FDynamicMeshVertex MeshVertex;
		MeshVertex.Position = Vertex;
		MeshVertex.Color = Color;
		MeshVertex.TextureCoordinate = TC;
		MeshVertex.TextureCoordinate.X += TCStep * SideIndex;

		MeshVertex.SetTangents(
			-ZAxis,
			(-ZAxis) ^ Normal,
			Normal
			);

		MeshBuilder.AddVertex(MeshVertex); //Add bottom vertex
	}
	
	//Add top/bottom triangles, in the style of a fan.
	for(INT SideIndex = 0; SideIndex < NumSides-1; SideIndex++)
	{
		INT V0 = 0;
		INT V1 = SideIndex;
		INT V2 = (SideIndex + 1);

		MeshBuilder.AddTriangle(V0, V1, V2);
		MeshBuilder.AddTriangle(V0, V2, V1);
	}

	MeshBuilder.Draw(PDI, FMatrix::Identity, MaterialRenderProxy, DepthPriority,0.f);
}

/**
 * Draws a flat arrow with an outline.
 *
 * @param	PDI						Draw interface.
 * @param	Base					Base of the arrow.
 * @param	XAxis					X alignment axis to draw along.
 * @param	YAxis					Y alignment axis to draw along.
 * @param	Color					Color of the circle.
 * @param	Length					Length of the arrow, from base to tip.
 * @param	Width					Width of the base of the arrow, head of the arrow will be 2x.
 * @param	MaterialRenderProxy		Material to use for render 
 * @param	DepthPriority			Depth priority for the circle.
 */

/*
x-axis is from point 0 to point 2
y-axis is from point 0 to point 1
        6
        /\
       /  \
      /    \
     4_2  3_5
       |  |
       0__1
*/


void DrawFlatArrow(class FPrimitiveDrawInterface* PDI,const FVector& Base,const FVector& XAxis,const FVector& YAxis,FColor Color,FLOAT Length,INT Width, const FMaterialRenderProxy* MaterialRenderProxy, BYTE DepthPriority)
{
	FLOAT DistanceFromBaseToHead = Length/3.0f;
	FLOAT DistanceFromBaseToTip = DistanceFromBaseToHead*2.0f;
	FLOAT WidthOfBase = Width;
	FLOAT WidthOfHead = 2*Width;

	FVector ArrowPoints[7];
	//base points
	ArrowPoints[0] = Base - YAxis*(WidthOfBase*.5f);
	ArrowPoints[1] = Base + YAxis*(WidthOfBase*.5f);
	//inner head
	ArrowPoints[2] = ArrowPoints[0] + XAxis*DistanceFromBaseToHead;
	ArrowPoints[3] = ArrowPoints[1] + XAxis*DistanceFromBaseToHead;
	//outer head
	ArrowPoints[4] = ArrowPoints[2] - YAxis*(WidthOfBase*.5f);
	ArrowPoints[5] = ArrowPoints[3] + YAxis*(WidthOfBase*.5f);
	//tip
	ArrowPoints[6] = Base + XAxis*Length;

	//Draw lines
	{
		//base
		PDI->DrawLine(ArrowPoints[0], ArrowPoints[1], Color, DepthPriority);
		//base sides
		PDI->DrawLine(ArrowPoints[0], ArrowPoints[2], Color, DepthPriority);
		PDI->DrawLine(ArrowPoints[1], ArrowPoints[3], Color, DepthPriority);
		//head base
		PDI->DrawLine(ArrowPoints[2], ArrowPoints[4], Color, DepthPriority);
		PDI->DrawLine(ArrowPoints[3], ArrowPoints[5], Color, DepthPriority);
		//head sides
		PDI->DrawLine(ArrowPoints[4], ArrowPoints[6], Color, DepthPriority);
		PDI->DrawLine(ArrowPoints[5], ArrowPoints[6], Color, DepthPriority);

	}

	FDynamicMeshBuilder MeshBuilder;

	//Compute vertices for base circle.
	for(INT i = 0; i< 7; ++i)
	{
		FDynamicMeshVertex MeshVertex;
		MeshVertex.Position = ArrowPoints[i];
		MeshVertex.Color = Color;
		MeshVertex.TextureCoordinate = FVector2D(0.0f, 0.0f);;
		MeshVertex.SetTangents(XAxis^YAxis, YAxis, XAxis);
		MeshBuilder.AddVertex(MeshVertex); //Add bottom vertex
	}
	
	//Add triangles / double sided
	{
		MeshBuilder.AddTriangle(0, 2, 1); //base
		MeshBuilder.AddTriangle(0, 1, 2); //base
		MeshBuilder.AddTriangle(1, 2, 3); //base
		MeshBuilder.AddTriangle(1, 3, 2); //base
		MeshBuilder.AddTriangle(4, 5, 6); //head
		MeshBuilder.AddTriangle(4, 6, 5); //head
	}

	MeshBuilder.Draw(PDI, FMatrix::Identity, MaterialRenderProxy, DepthPriority, 0.f);
}

// Line drawing utility functions.

/**
 * Draws a wireframe box.
 *
 * @param	PDI				Draw interface.
 * @param	Box				The FBox to use for drawing.
 * @param	Color			Color of the box.
 * @param	DepthPriority	Depth priority for the circle.
 */
void DrawWireBox(FPrimitiveDrawInterface* PDI,const FBox& Box,FColor Color,BYTE DepthPriority)
{
	FVector	B[2],P,Q;
	int i,j;

	B[0]=Box.Min;
	B[1]=Box.Max;

	for( i=0; i<2; i++ ) for( j=0; j<2; j++ )
	{
		P.X=B[i].X; Q.X=B[i].X;
		P.Y=B[j].Y; Q.Y=B[j].Y;
		P.Z=B[0].Z; Q.Z=B[1].Z;
		PDI->DrawLine(P,Q,Color,DepthPriority);

		P.Y=B[i].Y; Q.Y=B[i].Y;
		P.Z=B[j].Z; Q.Z=B[j].Z;
		P.X=B[0].X; Q.X=B[1].X;
		PDI->DrawLine(P,Q,Color,DepthPriority);

		P.Z=B[i].Z; Q.Z=B[i].Z;
		P.X=B[j].X; Q.X=B[j].X;
		P.Y=B[0].Y; Q.Y=B[1].Y;
		PDI->DrawLine(P,Q,Color,DepthPriority);
	}
}

/**
 * Draws a circle using lines.
 *
 * @param	PDI				Draw interface.
 * @param	Base			Center of the circle.
 * @param	X				X alignment axis to draw along.
 * @param	Y				Y alignment axis to draw along.
 * @param	Z				Z alignment axis to draw along.
 * @param	Color			Color of the circle.
 * @param	Radius			Radius of the circle.
 * @param	NumSides		Numbers of sides that the circle has.
 * @param	DepthPriority	Depth priority for the circle.
 */
void DrawCircle(FPrimitiveDrawInterface* PDI,const FVector& Base,const FVector& X,const FVector& Y,FColor Color,FLOAT Radius,INT NumSides,BYTE DepthPriority)
{
	const FLOAT	AngleDelta = 2.0f * PI / NumSides;
	FVector	LastVertex = Base + X * Radius;

	for(INT SideIndex = 0;SideIndex < NumSides;SideIndex++)
	{
		const FVector Vertex = Base + (X * appCos(AngleDelta * (SideIndex + 1)) + Y * appSin(AngleDelta * (SideIndex + 1))) * Radius;
		PDI->DrawLine(LastVertex,Vertex,Color,DepthPriority);
		LastVertex = Vertex;
	}
}

/**
 * Draws a sphere using circles.
 *
 * @param	PDI				Draw interface.
 * @param	Base			Center of the sphere.
 * @param	Color			Color of the sphere.
 * @param	Radius			Radius of the sphere.
 * @param	NumSides		Numbers of sides that the circle has.
 * @param	DepthPriority	Depth priority for the circle.
 */
void DrawWireSphere(class FPrimitiveDrawInterface* PDI, const FVector& Base, FColor Color, FLOAT Radius, INT NumSides, BYTE DepthPriority)
{
  DrawCircle(PDI, Base, FVector(1,0,0), FVector(0,1,0), Color, Radius, NumSides, DepthPriority);
  DrawCircle(PDI, Base, FVector(1,0,0), FVector(0,0,1), Color, Radius, NumSides, DepthPriority);
  DrawCircle(PDI, Base, FVector(0,1,0), FVector(0,0,1), Color, Radius, NumSides, DepthPriority);
}

/**
 * Draws a wireframe cylinder.
 *
 * @param	PDI				Draw interface.
 * @param	Base			Center pointer of the base of the cylinder.
 * @param	X				X alignment axis to draw along.
 * @param	Y				Y alignment axis to draw along.
 * @param	Z				Z alignment axis to draw along.
 * @param	Color			Color of the cylinder.
 * @param	Radius			Radius of the cylinder.
 * @param	HalfHeight		Half of the height of the cylinder.
 * @param	NumSides		Numbers of sides that the cylinder has.
 * @param	DepthPriority	Depth priority for the cylinder.
 */
void DrawWireCylinder(FPrimitiveDrawInterface* PDI,const FVector& Base,const FVector& X,const FVector& Y,const FVector& Z,FColor Color,FLOAT Radius,FLOAT HalfHeight,INT NumSides,BYTE DepthPriority)
{
	const FLOAT	AngleDelta = 2.0f * PI / NumSides;
	FVector	LastVertex = Base + X * Radius;

	for(INT SideIndex = 0;SideIndex < NumSides;SideIndex++)
	{
		const FVector Vertex = Base + (X * appCos(AngleDelta * (SideIndex + 1)) + Y * appSin(AngleDelta * (SideIndex + 1))) * Radius;

		PDI->DrawLine(LastVertex - Z * HalfHeight,Vertex - Z * HalfHeight,Color,DepthPriority);
		PDI->DrawLine(LastVertex + Z * HalfHeight,Vertex + Z * HalfHeight,Color,DepthPriority);
		PDI->DrawLine(LastVertex - Z * HalfHeight,LastVertex + Z * HalfHeight,Color,DepthPriority);

		LastVertex = Vertex;
	}
}


/**
 * Draws a wireframe chopped cone(cylinder with independant top and bottom radius).
 *
 * @param	PDI				Draw interface.
 * @param	Base			Center pointer of the base of the cone.
 * @param	X				X alignment axis to draw along.
 * @param	Y				Y alignment axis to draw along.
 * @param	Z				Z alignment axis to draw along.
 * @param	Color			Color of the cone.
 * @param	Radius			Radius of the cone at the bottom.
 * @param	TopRadius		Radius of the cone at the top.
 * @param	HalfHeight		Half of the height of the cone.
 * @param	NumSides		Numbers of sides that the cone has.
 * @param	DepthPriority	Depth priority for the cone.
 */
void DrawWireChoppedCone(FPrimitiveDrawInterface* PDI,const FVector& Base,const FVector& X,const FVector& Y,const FVector& Z,FColor Color,FLOAT Radius, FLOAT TopRadius,FLOAT HalfHeight,INT NumSides,BYTE DepthPriority)
{
	const FLOAT	AngleDelta = 2.0f * PI / NumSides;
	FVector	LastVertex = Base + X * Radius;
	FVector LastTopVertex = Base + X * TopRadius;

	for(INT SideIndex = 0;SideIndex < NumSides;SideIndex++)
	{
		const FVector Vertex = Base + (X * appCos(AngleDelta * (SideIndex + 1)) + Y * appSin(AngleDelta * (SideIndex + 1))) * Radius;
		const FVector TopVertex = Base + (X * appCos(AngleDelta * (SideIndex + 1)) + Y * appSin(AngleDelta * (SideIndex + 1))) * TopRadius;	

		PDI->DrawLine(LastVertex - Z * HalfHeight,Vertex - Z * HalfHeight,Color,DepthPriority);
		PDI->DrawLine(LastTopVertex + Z * HalfHeight,TopVertex + Z * HalfHeight,Color,DepthPriority);
		PDI->DrawLine(LastVertex - Z * HalfHeight,LastTopVertex + Z * HalfHeight,Color,DepthPriority);

		LastVertex = Vertex;
		LastTopVertex = TopVertex;
	}
}

/**
 * Draws a wireframe cone
 *
 * @param	PDI				Draw interface.
 * @param	Transform		Generic transform to apply (ex. a local-to-world transform).
 * @param	ConeRadius		Radius of the cone.
 * @param	ConeAngle		Angle of the cone.
 * @param	ConeSides		Numbers of sides that the cone has.
 * @param	Color			Color of the cone.
 * @param	DepthPriority	Depth priority for the cone.
 * @param	Verts			Out param, the positions of the verts at the cone's base.
 */
void DrawWireCone(FPrimitiveDrawInterface* PDI, const FMatrix& Transform, FLOAT ConeRadius, FLOAT ConeAngle, INT ConeSides, FColor Color, BYTE DepthPriority, TArray<FVector>& Verts)
{
	static const FLOAT TwoPI = 2.0f * PI;
	static const FLOAT ToRads = PI / 180.0f;
	static const FLOAT MaxAngle = 89.0f * ToRads + 0.001f;
	const FLOAT ClampedConeAngle = Clamp(ConeAngle * ToRads, 0.001f, MaxAngle);
	const FLOAT SinClampedConeAngle = appSin( ClampedConeAngle );
	const FLOAT CosClampedConeAngle = appCos( ClampedConeAngle );
	const FVector ConeDirection(1,0,0);
	const FVector ConeUpVector(0,1,0);
	const FVector ConeLeftVector(0,0,1);

	Verts.Add( ConeSides );

	for ( INT i = 0 ; i < Verts.Num() ; ++i )
	{
		const FLOAT Theta = static_cast<FLOAT>( (TwoPI * i) / Verts.Num() );
		Verts(i) = (ConeDirection * (ConeRadius * CosClampedConeAngle)) +
			((SinClampedConeAngle * ConeRadius * appCos( Theta )) * ConeUpVector) +
			((SinClampedConeAngle * ConeRadius * appSin( Theta )) * ConeLeftVector);
	}

	// Transform to world space.
	for ( INT i = 0 ; i < Verts.Num() ; ++i )
	{
		Verts(i) = Transform.TransformFVector( Verts(i) );
	}

	// Draw spokes.
	for ( INT i = 0 ; i < Verts.Num(); ++i )
	{
		PDI->DrawLine( Transform.GetOrigin(), Verts(i), Color, DepthPriority );
	}

	// Draw rim.
	for ( INT i = 0 ; i < Verts.Num()-1 ; ++i )
	{
		PDI->DrawLine( Verts(i), Verts(i+1), Color, DepthPriority );
	}
	PDI->DrawLine( Verts(Verts.Num()-1), Verts(0), Color, DepthPriority );
}

/**
 * Draws an oriented box.
 *
 * @param	PDI				Draw interface.
 * @param	Base			Center point of the box.
 * @param	X				X alignment axis to draw along.
 * @param	Y				Y alignment axis to draw along.
 * @param	Z				Z alignment axis to draw along.
 * @param	Color			Color of the box.
 * @param	Extent			Vector with the half-sizes of the box.
 * @param	DepthPriority	Depth priority for the cone.
 */

void DrawOrientedWireBox(FPrimitiveDrawInterface* PDI,const FVector& Base,const FVector& X,const FVector& Y,const FVector& Z, FVector Extent, FColor Color,BYTE DepthPriority)
{
	FVector	B[2],P,Q;
	int i,j;

	FMatrix m(X, Y, Z, Base);
	B[0] = -Extent;
	B[1] = Extent;

	for( i=0; i<2; i++ ) for( j=0; j<2; j++ )
	{
		P.X=B[i].X; Q.X=B[i].X;
		P.Y=B[j].Y; Q.Y=B[j].Y;
		P.Z=B[0].Z; Q.Z=B[1].Z;
		P = m.TransformFVector(P); Q = m.TransformFVector(Q);
		PDI->DrawLine(P,Q,Color,DepthPriority);

		P.Y=B[i].Y; Q.Y=B[i].Y;
		P.Z=B[j].Z; Q.Z=B[j].Z;
		P.X=B[0].X; Q.X=B[1].X;
		P = m.TransformFVector(P); Q = m.TransformFVector(Q);
		PDI->DrawLine(P,Q,Color,DepthPriority);

		P.Z=B[i].Z; Q.Z=B[i].Z;
		P.X=B[j].X; Q.X=B[j].X;
		P.Y=B[0].Y; Q.Y=B[1].Y;
		P = m.TransformFVector(P); Q = m.TransformFVector(Q);
		PDI->DrawLine(P,Q,Color,DepthPriority);
	}
}


/**
 * Draws a directional arrow.
 *
 * @param	PDI				Draw interface.
 * @param	ArrowToWorld	Transform matrix for the arrow.
 * @param	InColor			Color of the arrow.
 * @param	Length			Length of the arrow
 * @param	ArrowSize		Size of the arrow head.
 * @param	DepthPriority	Depth priority for the arrow.
 */
void DrawDirectionalArrow(FPrimitiveDrawInterface* PDI,const FMatrix& ArrowToWorld,FColor InColor,FLOAT Length,FLOAT ArrowSize,BYTE DepthPriority)
{
	PDI->DrawLine(ArrowToWorld.TransformFVector(FVector(Length,0,0)),ArrowToWorld.TransformFVector(FVector(0,0,0)),InColor,DepthPriority);
	PDI->DrawLine(ArrowToWorld.TransformFVector(FVector(Length,0,0)),ArrowToWorld.TransformFVector(FVector(Length-ArrowSize,+ArrowSize,+ArrowSize)),InColor,DepthPriority);
	PDI->DrawLine(ArrowToWorld.TransformFVector(FVector(Length,0,0)),ArrowToWorld.TransformFVector(FVector(Length-ArrowSize,+ArrowSize,-ArrowSize)),InColor,DepthPriority);
	PDI->DrawLine(ArrowToWorld.TransformFVector(FVector(Length,0,0)),ArrowToWorld.TransformFVector(FVector(Length-ArrowSize,-ArrowSize,+ArrowSize)),InColor,DepthPriority);
	PDI->DrawLine(ArrowToWorld.TransformFVector(FVector(Length,0,0)),ArrowToWorld.TransformFVector(FVector(Length-ArrowSize,-ArrowSize,-ArrowSize)),InColor,DepthPriority);
}

/**
 * Draws a axis-aligned 3 line star.
 *
 * @param	PDI				Draw interface.
 * @param	Position		Position of the star.
 * @param	Size			Size of the star
 * @param	InColor			Color of the arrow.
 * @param	DepthPriority	Depth priority for the star.
 */
void DrawWireStar(FPrimitiveDrawInterface* PDI,const FVector& Position, FLOAT Size, FColor Color,BYTE DepthPriority)
{
	PDI->DrawLine(Position + Size * FVector(1,0,0), Position - Size * FVector(1,0,0), Color, DepthPriority);
	PDI->DrawLine(Position + Size * FVector(0,1,0), Position - Size * FVector(0,1,0), Color, DepthPriority);
	PDI->DrawLine(Position + Size * FVector(0,0,1), Position - Size * FVector(0,0,1), Color, DepthPriority);
}

/**
 * Draws a dashed line.
 *
 * @param	PDI				Draw interface.
 * @param	Start			Start position of the line.
 * @param	End				End position of the line.
 * @param	Color			Color of the arrow.
 * @param	DashSize		Size of each of the dashes that makes up the line.
 * @param	DepthPriority	Depth priority for the line.
 */
void DrawDashedLine(FPrimitiveDrawInterface* PDI,const FVector& Start, const FVector& End, FColor Color, FLOAT DashSize,BYTE DepthPriority)
{
	FVector LineDir = End - Start;
	FLOAT LineLeft = (End - Start).Size();
	LineDir /= LineLeft;

	while(LineLeft > 0.f)
	{
		const FVector DrawStart = End - ( LineLeft * LineDir );
		const FVector DrawEnd = DrawStart + ( Min<FLOAT>(DashSize, LineLeft) * LineDir );

		PDI->DrawLine(DrawStart, DrawEnd, Color, DepthPriority);

		LineLeft -= 2*DashSize;
	}
}

/**
 * Draws a wireframe diamond.
 *
 * @param	PDI				Draw interface.
 * @param	DiamondMatrix	Transform Matrix for the diamond.
 * @param	Size			Size of the diamond.
 * @param	InColor			Color of the diamond.
 * @param	DepthPriority	Depth priority for the diamond.
 */
void DrawWireDiamond(FPrimitiveDrawInterface* PDI,const FMatrix& DiamondMatrix, FLOAT Size, const FColor& InColor,BYTE DepthPriority)
{
	const FVector TopPoint = DiamondMatrix.TransformFVector( FVector(0,0,1) * Size );
	const FVector BottomPoint = DiamondMatrix.TransformFVector( FVector(0,0,-1) * Size );

	const FLOAT OneOverRootTwo = appSqrt(0.5f);

	FVector SquarePoints[4];
	SquarePoints[0] = DiamondMatrix.TransformFVector( FVector(1,1,0) * Size * OneOverRootTwo );
	SquarePoints[1] = DiamondMatrix.TransformFVector( FVector(1,-1,0) * Size * OneOverRootTwo );
	SquarePoints[2] = DiamondMatrix.TransformFVector( FVector(-1,-1,0) * Size * OneOverRootTwo );
	SquarePoints[3] = DiamondMatrix.TransformFVector( FVector(-1,1,0) * Size * OneOverRootTwo );

	PDI->DrawLine(TopPoint, SquarePoints[0], InColor, DepthPriority);
	PDI->DrawLine(TopPoint, SquarePoints[1], InColor, DepthPriority);
	PDI->DrawLine(TopPoint, SquarePoints[2], InColor, DepthPriority);
	PDI->DrawLine(TopPoint, SquarePoints[3], InColor, DepthPriority);

	PDI->DrawLine(BottomPoint, SquarePoints[0], InColor, DepthPriority);
	PDI->DrawLine(BottomPoint, SquarePoints[1], InColor, DepthPriority);
	PDI->DrawLine(BottomPoint, SquarePoints[2], InColor, DepthPriority);
	PDI->DrawLine(BottomPoint, SquarePoints[3], InColor, DepthPriority);

	PDI->DrawLine(SquarePoints[0], SquarePoints[1], InColor, DepthPriority);
	PDI->DrawLine(SquarePoints[1], SquarePoints[2], InColor, DepthPriority);
	PDI->DrawLine(SquarePoints[2], SquarePoints[3], InColor, DepthPriority);
	PDI->DrawLine(SquarePoints[3], SquarePoints[0], InColor, DepthPriority);
}

const WORD GCubeIndices[12*3] =
{
	0, 2, 3,
	0, 3, 1,
	4, 5, 7,
	4, 7, 6,
	0, 1, 5,
	0, 5, 4,
	2, 6, 7,
	2, 7, 3,
	0, 4, 6,
	0, 6, 2,
	1, 3, 7,
	1, 7, 5,
};

FLinearColor GetSelectionColor(const FLinearColor& BaseColor,UBOOL bSelected,UBOOL bHovered)
{
	const FLOAT SelectionFactor = bSelected ? 1.0f : ( bHovered ? 0.65f : 0.5f );

	// Apply the selection factor in SRGB space, to match legacy behavior.
	return FLinearColor(
		appPow(appPow(BaseColor.R,1.0f / 2.2f) * SelectionFactor,2.2f),
		appPow(appPow(BaseColor.G,1.0f / 2.2f) * SelectionFactor,2.2f),
		appPow(appPow(BaseColor.B,1.0f / 2.2f) * SelectionFactor,2.2f),
		BaseColor.A
		);
}

FLinearColor ConditionalAdjustForMobileEmulation(const FSceneView* View, const FLinearColor& Color)
{
#if !CONSOLE
	if( GEmulateMobileRendering && !GUseGammaCorrectionForMobileEmulation )
	{
		// If mobile emulation is enabled, compensate for gamma being disabled
		FLOAT InvDisplayGamma = 1.0f / View->Family->RenderTarget->GetDisplayGamma();
		return FLinearColor(
			appPow(Color.R, InvDisplayGamma),
			appPow(Color.G, InvDisplayGamma),
			appPow(Color.B, InvDisplayGamma),
			Color.A
		);
	}
#endif
	return Color;
}

UBOOL IsRichView(const FSceneView* View)
{
	// Flags which make the view rich when absent.
	const static EShowFlags NonRichShowFlags =
		SHOW_Materials |
		SHOW_LOD |
		SHOW_Lighting;
	if(NonRichShowFlags != (View->Family->ShowFlags & NonRichShowFlags))
	{
		return TRUE;
	}

	// Flags which make the view rich when present.
	const static EShowFlags RichShowFlags =
		SHOW_Wireframe |
		SHOW_LevelColoration |
		SHOW_BSPSplit |
		SHOW_LightComplexity |
		SHOW_ShaderComplexity |
		SHOW_PropertyColoration |
		SHOW_MeshEdges | 
		SHOW_LightInfluences |
		SHOW_TextureDensity |
		SHOW_LightMapDensity |
		SHOW_VertexColors;

	return (View->Family->ShowFlags & RichShowFlags) != 0;
}

/** Utility for returning if a view is using one of the collision views. */
UBOOL IsCollisionView(const FSceneView* View)
{
	if(View->Family->ShowFlags & SHOW_Collision_Any)
	{
		return TRUE;
	}

	return FALSE;
}

/**
 * Draws a mesh, modifying the material which is used depending on the view's show flags.
 * Meshes with materials irrelevant to the pass which the mesh is being drawn for may be entirely ignored.
 *
 * @param PDI - The primitive draw interface to draw the mesh on.
 * @param Mesh - The mesh to draw.
 * @param WireframeColor - The color which is used when rendering the mesh with SHOW_Wireframe.
 * @param LevelColor - The color which is used when rendering the mesh with SHOW_LevelColoration.
 * @param PropertyColor - The color to use when rendering the mesh with SHOW_PropertyColoration.
 * @param PrimitiveInfo - The FScene information about the UPrimitiveComponent.
 * @param bSelected - True if the primitive is selected.
 * @param ExtraDrawFlags - optional flags to override the view family show flags when rendering
 * @return Number of passes rendered for the mesh
 */
INT DrawRichMesh(
	FPrimitiveDrawInterface* PDI,
	const FMeshBatch& Mesh,
	const FLinearColor& WireframeColor,
	const FLinearColor& LevelColor,
	const FLinearColor& PropertyColor,
	FPrimitiveSceneInfo *PrimitiveInfo,
	UBOOL bSelected,
	const EShowFlags& ExtraDrawFlags
	)
{
	UBOOL PassesRendered=0;

#if FINAL_RELEASE
	// Draw the mesh unmodified.
	PassesRendered = PDI->DrawMesh( Mesh );
#else

	// If debug viewmodes are not allowed, skip all of the debug viewmode handling
	if (!AllowDebugViewmodes())
	{
		return PDI->DrawMesh( Mesh );
	}

	const EShowFlags ShowFlags = PDI->View->Family->ShowFlags | ExtraDrawFlags;

	if(ShowFlags & SHOW_Wireframe)
	{
		// In wireframe mode, draw the edges of the mesh with the specified wireframe color, or
		// with the level or property color if level or property coloration is enabled.
		FLinearColor BaseColor( WireframeColor );
		if ( ShowFlags & SHOW_PropertyColoration )
		{
			BaseColor = PropertyColor;
		}
		else if ( ShowFlags & SHOW_LevelColoration )
		{
			BaseColor = LevelColor;
		}

		const FMaterial* Material = Mesh.MaterialRenderProxy->GetMaterial();
		if( Material->GetD3D11TessellationMode() != MTM_NoTessellation ||
			( Material->UsesMaterialVertexPositionOffset() && Material->GetBlendMode() != BLEND_Masked ) )
		{
			// If the material is mesh-modifying, we cannot rely on substitution
			const FOverrideSelectionColorMaterialRenderProxy WireframeMaterialInstance(
				Mesh.MaterialRenderProxy,
				ConditionalAdjustForMobileEmulation(PDI->View, GetSelectionColor( BaseColor, bSelected, Mesh.MaterialRenderProxy->IsHovered()))
				);

			FMeshBatch ModifiedMesh = Mesh;
			ModifiedMesh.bWireframe = TRUE;
			ModifiedMesh.MaterialRenderProxy = &WireframeMaterialInstance;
			PassesRendered = PDI->DrawMesh(ModifiedMesh);
		}
		else
		{
			const FColoredMaterialRenderProxy WireframeMaterialInstance(
				GEngine->WireframeMaterial->GetRenderProxy(Mesh.MaterialRenderProxy->IsSelected(), Mesh.MaterialRenderProxy->IsHovered()),
				ConditionalAdjustForMobileEmulation(PDI->View, GetSelectionColor( BaseColor, bSelected, Mesh.MaterialRenderProxy->IsHovered()))
				);
			FMeshBatch ModifiedMesh = Mesh;
			ModifiedMesh.bWireframe = TRUE;
			ModifiedMesh.MaterialRenderProxy = &WireframeMaterialInstance;
			PassesRendered = PDI->DrawMesh(ModifiedMesh);
		}
	}
	else if(ShowFlags & SHOW_LightComplexity)
	{
		// Don't render unlit translucency when in 'light complexity' viewmode.
		if (!Mesh.IsTranslucent() || Mesh.MaterialRenderProxy->GetMaterial()->GetLightingModel() != MLM_Unlit)
		{
			// Count the number of lights interacting with this primitive.
			INT NumLights = 0;
			FLightPrimitiveInteraction *LightList = PrimitiveInfo->LightList;
			while ( LightList )
			{
				const FLightSceneInfo* LightSceneInfo = LightList->GetLight();

				// Don't count sky lights, since they're "free".
				if ( LightSceneInfo->LightType != LightType_Sky )
				{
					// Determine the interaction type between the mesh and the light.
					FLightInteraction LightInteraction = FLightInteraction::Uncached();
					if(Mesh.LCI)
					{
						LightInteraction = Mesh.LCI->GetInteraction(LightSceneInfo);
					}

					// Don't count light-mapped or irrelevant lights.
					if(LightInteraction.GetType() != LIT_CachedIrrelevant && LightInteraction.GetType() != LIT_CachedLightMap)
					{
						NumLights++;
					}
				}
				LightList = LightList->GetNextLight();
			}

			// Get a colored material to represent the number of lights.
			// Some component types (BSP) have multiple FLightCacheInterface's per component, so make sure the whole component represents the number of dominant lights affecting
			NumLights = Min( Max<INT>(NumLights, PrimitiveInfo->NumAffectingDominantLights), GEngine->LightComplexityColors.Num() - 1 );
			// Color primitives with multiple affecting dominant lights red
			FColor Color = PrimitiveInfo->NumAffectingDominantLights > 1 ? FColor(255,0,0) : GEngine->LightComplexityColors(NumLights);

			FColoredMaterialRenderProxy LightComplexityMaterialInstance(
				GEngine->LevelColorationUnlitMaterial->GetRenderProxy(Mesh.MaterialRenderProxy->IsSelected(), Mesh.MaterialRenderProxy->IsHovered()),
				ConditionalAdjustForMobileEmulation(PDI->View, Color) );

			// Draw the mesh colored by light complexity.
			FMeshBatch ModifiedMesh = Mesh;
			ModifiedMesh.MaterialRenderProxy = &LightComplexityMaterialInstance;
			PassesRendered = PDI->DrawMesh(ModifiedMesh);
		}
	}
	else if(!(ShowFlags & SHOW_Materials))
	{
		// Don't render unlit translucency when in 'lighting only' viewmode.
		if (Mesh.MaterialRenderProxy->GetMaterial()->GetLightingModel() != MLM_Unlit)
		{
			// When materials aren't shown, apply the same basic material to all meshes.
			FMeshBatch ModifiedMesh = Mesh;

			UBOOL bTextureMapped = FALSE;
			FVector2D LMResolution;

			if ((ShowFlags & SHOW_LightMapDensity) &&
				ModifiedMesh.LCI &&
				(ModifiedMesh.LCI->GetLightMapInteraction().GetType() == LMIT_Texture) &&
				ModifiedMesh.LCI->GetLightMapInteraction().GetTexture(0))
			{
				LMResolution.X = Mesh.LCI->GetLightMapInteraction().GetTexture(0)->SizeX;
				LMResolution.Y = Mesh.LCI->GetLightMapInteraction().GetTexture(0)->SizeY;
				bTextureMapped = TRUE;
			}

			if (bTextureMapped == FALSE)
			{
				FMaterialRenderProxy* RenderProxy = GEngine->LevelColorationLitMaterial->GetRenderProxy(Mesh.MaterialRenderProxy->IsSelected(),Mesh.MaterialRenderProxy->IsHovered());
				const FColoredMaterialRenderProxy LightingOnlyMaterialInstance(
					RenderProxy,
					ConditionalAdjustForMobileEmulation(PDI->View, GEngine->LightingOnlyBrightness)
					);

				ModifiedMesh.MaterialRenderProxy = &LightingOnlyMaterialInstance;
				PassesRendered = PDI->DrawMesh(ModifiedMesh);
			}
			else
			{
				FMaterialRenderProxy* RenderProxy = GEngine->LightingTexelDensityMaterial->GetRenderProxy(Mesh.MaterialRenderProxy->IsSelected(),Mesh.MaterialRenderProxy->IsHovered());
				const FLightingDensityMaterialRenderProxy LightingDensityMaterialInstance(
					RenderProxy,
					GEngine->LightingOnlyBrightness,
					LMResolution
					);

				ModifiedMesh.MaterialRenderProxy = &LightingDensityMaterialInstance;
				PassesRendered = PDI->DrawMesh(ModifiedMesh);
			}
		}
	}
	else
	{	
		if(ShowFlags & SHOW_PropertyColoration)
		{
			// In property coloration mode, override the mesh's material with a color that was chosen based on property value.
			const UMaterial* PropertyColorationMaterial = (ShowFlags & SHOW_ViewMode_Lit) ? GEngine->LevelColorationLitMaterial : GEngine->LevelColorationUnlitMaterial;

			const FColoredMaterialRenderProxy PropertyColorationMaterialInstance(
				PropertyColorationMaterial->GetRenderProxy(Mesh.MaterialRenderProxy->IsSelected(), Mesh.MaterialRenderProxy->IsHovered()),
				ConditionalAdjustForMobileEmulation(PDI->View, GetSelectionColor(PropertyColor,bSelected,Mesh.MaterialRenderProxy->IsHovered()))
				);
			FMeshBatch ModifiedMesh = Mesh;
			ModifiedMesh.MaterialRenderProxy = &PropertyColorationMaterialInstance;
			PassesRendered = PDI->DrawMesh(ModifiedMesh);
		}
		else if ( ShowFlags & SHOW_LevelColoration )
		{
			const UMaterial* LevelColorationMaterial = (ShowFlags & SHOW_ViewMode_Lit) ? GEngine->LevelColorationLitMaterial : GEngine->LevelColorationUnlitMaterial;
			// Draw the mesh with level coloration.
			const FColoredMaterialRenderProxy LevelColorationMaterialInstance(
				LevelColorationMaterial->GetRenderProxy(Mesh.MaterialRenderProxy->IsSelected(), Mesh.MaterialRenderProxy->IsHovered()),
				ConditionalAdjustForMobileEmulation(PDI->View, GetSelectionColor(LevelColor,bSelected,Mesh.MaterialRenderProxy->IsHovered()))
				);
			FMeshBatch ModifiedMesh = Mesh;
			ModifiedMesh.MaterialRenderProxy = &LevelColorationMaterialInstance;
			PassesRendered = PDI->DrawMesh(ModifiedMesh);
		}
		else if (	(ShowFlags & SHOW_BSPSplit) 
				&&	PrimitiveInfo->Component 
				&&	PrimitiveInfo->Component->IsA(UModelComponent::StaticClass()) )
		{
			// Determine unique color for model component.
			FLinearColor BSPSplitColor;
			FRandomStream RandomStream( PrimitiveInfo->Component->GetIndex() );
			BSPSplitColor.R = RandomStream.GetFraction();
			BSPSplitColor.G = RandomStream.GetFraction();
			BSPSplitColor.B = RandomStream.GetFraction();
			BSPSplitColor.A = 1.0f;

			// Piggy back on the level coloration material.
			const UMaterial* BSPSplitMaterial = (ShowFlags & SHOW_ViewMode_Lit) ? GEngine->LevelColorationLitMaterial : GEngine->LevelColorationUnlitMaterial;
			
			// Draw BSP mesh with unique color for each model component.
			const FColoredMaterialRenderProxy BSPSplitMaterialInstance(
				BSPSplitMaterial->GetRenderProxy(Mesh.MaterialRenderProxy->IsSelected(), Mesh.MaterialRenderProxy->IsHovered()),
				ConditionalAdjustForMobileEmulation(PDI->View, GetSelectionColor(BSPSplitColor,bSelected,Mesh.MaterialRenderProxy->IsHovered()))
				);
			FMeshBatch ModifiedMesh = Mesh;
			ModifiedMesh.MaterialRenderProxy = &BSPSplitMaterialInstance;
			PassesRendered = PDI->DrawMesh(ModifiedMesh);
		}
		else if( ShowFlags & SHOW_Collision_Any)
		{
			const UMaterial* LevelColorationMaterial = (ShowFlags & SHOW_ViewMode_Lit) ? GEngine->ShadedLevelColorationLitMaterial : GEngine->ShadedLevelColorationUnlitMaterial;
			const FColoredMaterialRenderProxy CollisionMaterialInstance(
				LevelColorationMaterial->GetRenderProxy(bSelected),
				ConditionalAdjustForMobileEmulation(PDI->View, GetSelectionColor(LevelColor,bSelected,Mesh.MaterialRenderProxy->IsHovered()))
				);
			FMeshBatch ModifiedMesh = Mesh;
			ModifiedMesh.MaterialRenderProxy = &CollisionMaterialInstance;
			PassesRendered = PDI->DrawMesh(ModifiedMesh);
		}
		else 
		{
			// Draw the mesh unmodified.
			PassesRendered = PDI->DrawMesh( Mesh );
		}

        //Draw a wireframe overlay last, if requested
		if(ShowFlags & SHOW_MeshEdges)
		{
			// Draw the mesh's edges in blue, on top of the base geometry.
			if(Mesh.MaterialRenderProxy->GetMaterial()->MaterialModifiesMeshPosition())
			{
				const FSceneView* View = PDI->View;

				// Suppress diffuse/specular
				RHISetViewParametersWithOverrides(*View, View->TranslatedViewProjectionMatrix, FVector4(0.f, 0.f, 0.f, 0.f), FVector4(0.f, 0.f, 0.f, 0.f));

				// If the material is mesh-modifying, we cannot rely on substitution
				const FOverrideSelectionColorMaterialRenderProxy WireframeMaterialInstance(
					Mesh.MaterialRenderProxy,
					ConditionalAdjustForMobileEmulation(View, WireframeColor)
					);

				FMeshBatch ModifiedMesh = Mesh;
				ModifiedMesh.bWireframe = TRUE;
				ModifiedMesh.DepthBias = Mesh.DepthBias - 0.00004f;
				ModifiedMesh.MaterialRenderProxy = &WireframeMaterialInstance;
				PassesRendered = PDI->DrawMesh(ModifiedMesh);

				// Restore diffuse/specular
				RHISetViewParameters(*View);
			}
			else
			{
				FColoredMaterialRenderProxy WireframeMaterialInstance(
					GEngine->WireframeMaterial->GetRenderProxy(Mesh.MaterialRenderProxy->IsSelected(), Mesh.MaterialRenderProxy->IsHovered()),
					ConditionalAdjustForMobileEmulation(PDI->View, WireframeColor)
					);
				FMeshBatch ModifiedMesh = Mesh;
				ModifiedMesh.bWireframe = TRUE;
				ModifiedMesh.DepthBias = Mesh.DepthBias - 0.00004f;
				ModifiedMesh.MaterialRenderProxy = &WireframeMaterialInstance;
				PassesRendered = PDI->DrawMesh(ModifiedMesh);
			}
		}
	}

#if USE_MASSIVE_LOD
	// If we are disabling the LOD, then we will be drawing both parents and children, so then drawing lines connecting them makes sense
	if (!(ShowFlags & SHOW_LOD) && bSelected && PrimitiveInfo->ReplacementPrimitiveMapKey)
	{
		FPathToCompact* Path = FPrimitiveSceneInfo::PrimitiveToCompactMap.Find(PrimitiveInfo->ReplacementPrimitiveMapKey);
		if (Path)
		{
			FPrimitiveSceneInfoCompact& CompactInfo = Path->GetCompact(PrimitiveInfo->Scene->PrimitiveOctree);
			for( INT BatchElementIndex=0;BatchElementIndex < Mesh.Elements.Num();BatchElementIndex++ )
			{
				PDI->DrawLine( Mesh.Elements(BatchElementIndex).LocalToWorld.GetOrigin(), CompactInfo.Bounds.Origin, FColor(255, 255, 0), SDPG_World );
			}
		}
	}
#endif

#endif

	return PassesRendered;
}

/** Returns the current display gamma. */
FLOAT GetDisplayGamma(void)
{
	return GEngine->Client->DisplayGamma;
}

/** Sets the current display gamma. */
void SetDisplayGamma(FLOAT Gamma)
{
	GEngine->Client->DisplayGamma = Gamma;
}

/** Saves the current display gamma. */
void SaveDisplayGamma()
{
	GEngine->Client->SaveConfig();
}


/*=============================================================================
	FMipBiasFade class
=============================================================================*/

/** Global mip fading settings, indexed by EMipFadeSettings. */
FMipFadeSettings GMipFadeSettings[MipFade_NumSettings] =
{ 
	FMipFadeSettings(0.3f, 0.1f),	// MipFade_Normal
	FMipFadeSettings(2.0f, 1.0f)	// MipFade_Slow
};

/** Whether to enable mip-level fading or not: +1.0f if enabled, -1.0f if disabled. */
FLOAT GEnableMipLevelFading = 1.0f;

/** How "old" a texture must be to be considered a "new texture", in seconds. */
FLOAT GMipLevelFadingAgeThreshold = 0.5f;

/**
 *	Sets up a new interpolation target for the mip-bias.
 *	@param ActualMipCount	Number of mip-levels currently in memory
 *	@param TargetMipCount	Number of mip-levels we're changing to
 *	@param LastRenderTime	Timestamp when it was last rendered (GCurrentTime time space)
 *	@param FadeSetting		Which fade speed settings to use
 */
void FMipBiasFade::SetNewMipCount( FLOAT ActualMipCount, FLOAT TargetMipCount, DOUBLE LastRenderTime, EMipFadeSettings FadeSetting )
{
	check( ActualMipCount >=0 && TargetMipCount <= ActualMipCount );

	FLOAT TimeSinceLastRendered = FLOAT(GCurrentTime - LastRenderTime);

	// Is this a new texture or is this not in-game?
	if ( TotalMipCount == 0 || GIsGame == FALSE || TimeSinceLastRendered >= GMipLevelFadingAgeThreshold || GEnableMipLevelFading < 0.0f )
	{
		// No fading.
		TotalMipCount = ActualMipCount;
		MipCountDelta = 0.0f;
		MipCountFadingRate = 0.0f;
		StartTime = GRenderingRealtimeClock.GetCurrentTime();
		BiasOffset = 0.0f;
		return;
	}

	// Calculate the mipcount we're interpolating towards.
	FLOAT CurrentTargetMipCount = TotalMipCount - BiasOffset + MipCountDelta;

	// Is there no change?
	if ( appIsNearlyEqual(TotalMipCount, ActualMipCount) && appIsNearlyEqual(TargetMipCount, CurrentTargetMipCount) )
	{
		return;
	}

	// Calculate the mip-count at our current interpolation point.
	FLOAT CurrentInterpolatedMipCount = TotalMipCount - CalcMipBias();

	// Clamp it against the available mip-levels.
	CurrentInterpolatedMipCount = Clamp<FLOAT>(CurrentInterpolatedMipCount, 0, ActualMipCount);

	// Set up a new interpolation from CurrentInterpolatedMipCount to TargetMipCount.
	StartTime = GRenderingRealtimeClock.GetCurrentTime();
	TotalMipCount = ActualMipCount;
	MipCountDelta = TargetMipCount - CurrentInterpolatedMipCount;

	// Don't fade if we're already at the target mip-count.
	if ( appIsNearlyZero(MipCountDelta) )
	{
		MipCountDelta = 0.0f;
		BiasOffset = 0.0f;
		MipCountFadingRate = 0.0f;
	}
	else
	{
		BiasOffset = TotalMipCount - CurrentInterpolatedMipCount;
		if ( MipCountDelta > 0.0f )
		{
			MipCountFadingRate = 1.0f / (GMipFadeSettings[FadeSetting].FadeInSpeed * MipCountDelta);
		}
		else
		{
			MipCountFadingRate = -1.0f / (GMipFadeSettings[FadeSetting].FadeOutSpeed * MipCountDelta);
		}
	}
}

/** Emits draw events for a given FMeshBatch and the FPrimitiveSceneInfo corresponding to that mesh element. */
void EmitMeshDrawEvents(const FPrimitiveSceneInfo* PrimitiveSceneInfo, const FMeshBatch& Mesh)
{
#if !FINAL_RELEASE
	extern UBOOL GShowMaterialDrawEvents;
	if ( GShowMaterialDrawEvents )
	{
		// Only show material name at the top level
		// Note: this is the parent's material name, not the material instance
		SCOPED_DRAW_EVENT(MaterialEvent)(DEC_SCENE_ITEMS, *Mesh.MaterialRenderProxy->GetMaterial()->GetFriendlyName());
		if (PrimitiveSceneInfo)
		{
			// Show Actor, level and resource name inside the material name
			// These are separate draw events since some platforms only allow 32 character event names (xenon)
			{
				SCOPED_CONDITIONAL_DRAW_EVENT(LevelEvent,PrimitiveSceneInfo->LevelName != NAME_None)(DEC_SCENE_ITEMS, PrimitiveSceneInfo->LevelName.IsValid() ? *PrimitiveSceneInfo->LevelName.ToString() : TEXT(""));
			}
			{
				SCOPED_CONDITIONAL_DRAW_EVENT(OwnerEvent,PrimitiveSceneInfo->Owner != NULL)(DEC_SCENE_ITEMS, *PrimitiveSceneInfo->Owner->GetName());
			}
			SCOPED_CONDITIONAL_DRAW_EVENT(ResourceEvent,PrimitiveSceneInfo->Proxy->ResourceName != NAME_None)(DEC_SCENE_ITEMS, PrimitiveSceneInfo->Proxy->ResourceName.IsValid() ? *PrimitiveSceneInfo->Proxy->ResourceName.ToString() : TEXT(""));
		}
	}
#endif
}

/*
 	3 XYZ packed in 4 bytes. (11:11:10 for X:Y:Z)
*/

/**
*	operator FVector - unpacked to -1 to 1
*/
FPackedPosition::operator FVector() const
{

	return FVector(Vector.X/1023.f, Vector.Y/1023.f, Vector.Z/511.f);
}

/**
* operator VectorRegister
*/
VectorRegister FPackedPosition::GetVectorRegister() const
{
	FVector UnpackedVect = *this;

	VectorRegister VectorToUnpack = VectorLoadFloat3_W0(&UnpackedVect);

	return VectorToUnpack;
}

/**
* Pack this vector(-1 to 1 for XYZ) to 4 bytes XYZ(11:11:10)
*/
void FPackedPosition::Set( const FVector& InVector )
{
	check (Abs<FLOAT>(InVector.X) <= 1.f && Abs<FLOAT>(InVector.Y) <= 1.f &&  Abs<FLOAT>(InVector.Z) <= 1.f);
	
#if CONSOLE
	// This should not happen in Console - this should happen during Cooking in PC
	check (FALSE);
#else
	// Too confusing to use .5f - wanted to use the last bit!
	// Change to int for easier read
	Vector.X = Clamp<INT>(appTrunc(InVector.X * 1023.0f),-1023,1023);
	Vector.Y = Clamp<INT>(appTrunc(InVector.Y * 1023.0f),-1023,1023);
	Vector.Z = Clamp<INT>(appTrunc(InVector.Z * 511.0f),-511,511);
#endif
}

/**
* operator << serialize
*/
FArchive& operator<<(FArchive& Ar,FPackedPosition& N)
{
	// Save N.Packed
	return Ar << N.Packed;
}

/**
 * Calculates the amount of memory used for a texture.
 *
 * @param SizeX		Number of horizontal texels (for the base mip-level)
 * @param SizeY		Number of vertical texels (for the base mip-level)
 * @param Format	Texture format
 * @param MipCount	Number of mip-levels (including the base mip-level)
 */
INT CalcTextureSize( UINT SizeX, UINT SizeY, EPixelFormat Format, UINT MipCount )
{
	INT Size = 0;
	for ( UINT MipIndex=0; MipIndex < MipCount; ++MipIndex )
	{
		DWORD MipSizeX = Max<DWORD>(SizeX >> MipIndex, GPixelFormats[Format].BlockSizeX);
		DWORD Pitch = (MipSizeX / GPixelFormats[Format].BlockSizeX) * GPixelFormats[Format].BlockBytes;

		DWORD MipSizeY = Max<DWORD>(SizeY >> MipIndex, GPixelFormats[Format].BlockSizeY);
		DWORD NumRows = MipSizeY / GPixelFormats[Format].BlockSizeY;

		Size += NumRows * Pitch;
	}
	return Size;
}

/**
 * Calculates the amount of unused memory at the end of an Xbox packed mip tail. Can save 4-12 KB.
 *
 * @param SizeX				Width of the texture
 * @param SizeY				Height of the texture
 * @param Format			Texture format
 * @param NumMips			Number of mips, including the top mip
 * @param bHasPackedMipTail	Whether the texture has a packed mip-tail
 * @return					Amount of texture memory that is unused at the end of packed mip tail, in bytes
 */
UINT XeCalcUnusedMipTailSize( UINT SizeX, UINT SizeY, EPixelFormat Format, UINT NumMips, UBOOL bHasPackedMipTail )
{
	UINT UnusedMipTailSize = 0;
#if XBOX_OPTIMIZE_TEXTURE_SIZES
	// do we qualify to remove the 4K/12K wasted space after the packed mip tail?
	UBOOL bIsOptimizableFormat = bHasPackedMipTail && (Format == PF_DXT1 || Format == PF_DXT5);
	// this may be overly restrictive, but for memory savings, the edge cases aren't important
	// we suppose non-square textures, but only 1:1, 2:1 and 1:2
	if (bIsOptimizableFormat && NumMips >= 5 && 
		SizeX >= 16 && SizeY >= 16 && 
		SizeX <= SizeY * 2 &&
		SizeX >= SizeY / 2 &&
		(SizeX & (SizeX - 1)) == 0 &&	// power of two
		(SizeY & (SizeY - 1)) == 0)		// power of two
	{
		UBOOL bIsDXT5 = (Format == PF_DXT5);
		UBOOL bIsSquare = (SizeX == SizeY);
		// 2:1 or 1:2 DXT5 textures only has 8K of waste, not 12.
		UnusedMipTailSize = 1024 * (bIsDXT5 ? (bIsSquare ? 12 : 8) : 4);
	}
#endif
	return UnusedMipTailSize;
}
