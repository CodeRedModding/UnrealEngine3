/*=============================================================================
	UnSkeletalRender.h: Definitions and inline code for rendering SkeletalMeshComponet
	Copyright 2004 Epic Games, Inc. All Rights Reserved.
=============================================================================*/


#define STATIC_SKIN_VB			1
#define USE_SSE_SKINNING		__HAS_SSE__
#define USE_ALTIVEC_SKINNING	__HAS_ALTIVEC__


//
//	FFinalSkinVertex
//

struct FFinalSkinVertex
{
	FVector			Position;
	FPackedNormal	TangentX,
					TangentY,
					TangentZ;
	FLOAT			U,
					V;
};

//
//	FFinalSkinVertexBuffer
//

struct FFinalSkinVertexBuffer: FVertexBuffer
{
	USkeletalMeshComponent*	SkeletalMeshComponent;
	INT						LOD;

	// Constructor.

	FFinalSkinVertexBuffer(USkeletalMeshComponent* InSkeletalMeshComponent,INT InLOD):
		SkeletalMeshComponent(InSkeletalMeshComponent),
		LOD(InLOD)
	{
		Stride = sizeof(FFinalSkinVertex);
		Size = Stride * (SkeletalMeshComponent->SkeletalMesh->LODModels(LOD).RigidVertices.Num() + SkeletalMeshComponent->SkeletalMesh->LODModels(LOD).SoftVertices.Num());
		Dynamic = !STATIC_SKIN_VB;
	}

	// FVertexBuffer interface.

	virtual void GetData(void* Buffer);
};

//
//	FSkinShadowVertex
//

struct FSkinShadowVertex
{
	FVector	Position;
	FLOAT	Extrusion;

	// Constructors.

	FSkinShadowVertex() {}
	FSkinShadowVertex(FVector InPosition,FLOAT InExtrusion): Position(InPosition), Extrusion(InExtrusion) {}
};

//
//	FSkinShadowVertexBuffer
//

struct FSkinShadowVertexBuffer: FVertexBuffer
{
	USkeletalMeshComponent*	SkeletalMeshComponent;
	INT						LOD;

	// Constructor.

	FSkinShadowVertexBuffer(USkeletalMeshComponent* InSkeletalMeshComponent,INT InLOD):
		SkeletalMeshComponent(InSkeletalMeshComponent),
		LOD(InLOD)
	{
		Stride = sizeof(FSkinShadowVertex);
		Size = Stride * (SkeletalMeshComponent->SkeletalMesh->LODModels(LOD).RigidVertices.Num() + SkeletalMeshComponent->SkeletalMesh->LODModels(LOD).SoftVertices.Num()) * 2;
		Dynamic = !STATIC_SKIN_VB;
	}

	// FVertexBuffer interface.

	virtual void GetData(void* Buffer);
};

//
//	FSkeletalMeshObject
//
#if USE_SSE_SKINNING
static const FLOAT			INV_255							= 1.f / 255.f;
static const F32vec4		SSE_PACK_127_5					= F32vec4( 0.f, 127.5f, 127.5f, 127.5f );
static const F32vec4		SSE_UNPACK_INV_127_5			= F32vec4( 0.f, 1.f / 127.5f, 1.f / 127.5f, 1.f / 127.5f );
static const F32vec4		SSE_UNPACK_1					= F32vec4( 0.f, 1.f, 1.f, 1.f );
#elif USE_ALTIVEC_SKINNING
static const FLOAT			INV_255							= 1.f / 255.f;
static const __vector4_c	ALTIVEC_0						= __vector4_c( 0.f, 0.f, 0.f, 0.f );
static const __vector4_c	ALTIVEC_PACK_127_5				= __vector4_c( 127.5f, 127.5f, 127.5f, 0.f );
static const __vector4_c	ALTIVEC_UNPACK_INV_127_5		= __vector4_c( 1.f / 127.5f, 1.f / 127.5f, 1.f / 127.5f, 0.f );
static const __vector4_c	ALTIVEC_UNPACK_MINUS_1			= __vector4_c( -1.f, -1.f, -1.f, 0.f );
static const __vector4_c	ALTIVEC_PERMUTATION_MASK_UNPACK	= __vector4_c( (DWORD) 0xFFFFFF0F, 0xFFFFFF0E, 0xFFFFFF0D, 0xFFFFFF0C );
static const __vector4_c	ALTIVEC_PERMUTATION_MASK_PACK	= __vector4_c( 0, 0, 0, (DWORD) 0x0F0E0D0C );
#endif

struct FSkeletalMeshObject
{
	struct FSkeletalMeshObjectLOD
	{
		FFinalSkinVertexBuffer		VertexBuffer;
		FSkinShadowVertexBuffer		ShadowVertexBuffer;
		FLocalVertexFactory			VertexFactory;
		FLocalShadowVertexFactory	ShadowVertexFactory;

		// Constructor.

		FSkeletalMeshObjectLOD(USkeletalMeshComponent* SkeletalMeshComponent,INT LOD):
			VertexBuffer(SkeletalMeshComponent,LOD),
			ShadowVertexBuffer(SkeletalMeshComponent,LOD)
		{
			VertexFactory.Dynamic = 1;
			VertexFactory.DynamicVertexBuffer = &VertexBuffer;
			VertexFactory.PositionComponent = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,Position),VCT_Float3);
			VertexFactory.TangentBasisComponents[0] = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,TangentX),VCT_PackedNormal);
			VertexFactory.TangentBasisComponents[1] = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,TangentY),VCT_PackedNormal);
			VertexFactory.TangentBasisComponents[2] = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,TangentZ),VCT_PackedNormal);
			VertexFactory.TextureCoordinates.AddItem(FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,U),VCT_Float2));
			GResourceManager->UpdateResource(&VertexFactory);
			GResourceManager->UpdateResource(&VertexBuffer);

			ShadowVertexFactory.Dynamic = 1;
			ShadowVertexFactory.DynamicVertexBuffer = &ShadowVertexBuffer;
			ShadowVertexFactory.PositionComponent = FVertexStreamComponent(&ShadowVertexBuffer,STRUCT_OFFSET(FSkinShadowVertex,Position),VCT_Float3);
			ShadowVertexFactory.ExtrusionComponent = FVertexStreamComponent(&ShadowVertexBuffer,STRUCT_OFFSET(FSkinShadowVertex,Extrusion),VCT_Float1);
			GResourceManager->UpdateResource(&ShadowVertexFactory);
			GResourceManager->UpdateResource(&ShadowVertexBuffer);
		}
	};

	USkeletalMeshComponent*			SkeletalMeshComponent;
	TArray<FSkeletalMeshObjectLOD>	LODs;
	TArray<FFinalSkinVertex>		CachedFinalVertices;
	INT								CachedVertexLOD;

	// Constructor.

	FSkeletalMeshObject(USkeletalMeshComponent* InSkeletalMeshComponent):
		SkeletalMeshComponent(InSkeletalMeshComponent),
		CachedVertexLOD(INDEX_NONE)
	{
		for(UINT LODIndex = 0;LODIndex < (UINT)SkeletalMeshComponent->SkeletalMesh->LODModels.Num();LODIndex++)
			new(LODs) FSkeletalMeshObjectLOD(SkeletalMeshComponent,LODIndex);
	}

	// UpdateTransforms

	void UpdateTransforms()
	{
		for(UINT LODIndex = 0;LODIndex < (UINT)LODs.Num();LODIndex++)
		{
			LODs(LODIndex).VertexFactory.LocalToWorld = SkeletalMeshComponent->LocalToWorld;
			LODs(LODIndex).VertexFactory.WorldToLocal = SkeletalMeshComponent->LocalToWorld.Inverse();
			GResourceManager->UpdateResource(&LODs(LODIndex).VertexFactory);
			GResourceManager->UpdateResource(&LODs(LODIndex).VertexBuffer);

			LODs(LODIndex).ShadowVertexFactory.LocalToWorld = SkeletalMeshComponent->LocalToWorld;
			GResourceManager->UpdateResource(&LODs(LODIndex).ShadowVertexFactory);
			GResourceManager->UpdateResource(&LODs(LODIndex).ShadowVertexBuffer);
		}

		CachedVertexLOD = INDEX_NONE;

	}

// Please ensure that SSE and plain C++ codepath always match as Ryan will kill 
// me otherwise. 	

#if USE_SSE_SKINNING
	// Pack/ Unpack operate on __m64 which is not supported on AMD64 so those will 
	// need adjustment.

	static FORCEINLINE void TransformNormals( const FMatrix& Matrix, FPlane* Normals )
	{
		// FMatrix is 16 byte aligned.
		F32vec4 M00 = _mm_load_ps( &Matrix.M[0][0] );
		F32vec4 M10 = _mm_load_ps( &Matrix.M[1][0] );
		F32vec4 M20 = _mm_load_ps( &Matrix.M[2][0] );
		F32vec4 M30 = _mm_load_ps( &Matrix.M[3][0] );
		{
			// Splatter/ smear x/y/z components across respective vectors.
			F32vec4 N_xxxx = _mm_load_ps1( &Normals[0].X );
			F32vec4 N_yyyy = _mm_load_ps1( &Normals[0].Y );
			F32vec4 N_zzzz = _mm_load_ps1( &Normals[0].Z );
			// Normals[i] = Matrix * Normals[i], w == 1
			_mm_store_ps( &Normals[0].X, _mm_add_ps( _mm_add_ps( _mm_add_ps( _mm_mul_ps( M00, N_xxxx ),_mm_mul_ps( M10, N_yyyy ) ), _mm_mul_ps( M20, N_zzzz ) ), M30 ) );
		}
		{
			// Splatter/ smear x/y/z components across respective vectors.
			F32vec4 N_xxxx = _mm_load_ps1( &Normals[1].X );
			F32vec4 N_yyyy = _mm_load_ps1( &Normals[1].Y );
			F32vec4 N_zzzz = _mm_load_ps1( &Normals[1].Z );
			// Normals[i] = Matrix * Normals[i], w == 0
			_mm_store_ps( &Normals[1].X, _mm_add_ps( _mm_add_ps( _mm_mul_ps( M00, N_xxxx ),_mm_mul_ps( M10, N_yyyy ) ), _mm_mul_ps( M20, N_zzzz ) ) );
		}
		{
			// Splatter/ smear x/y/z components across respective vectors.
			F32vec4 N_xxxx = _mm_load_ps1( &Normals[2].X );
			F32vec4 N_yyyy = _mm_load_ps1( &Normals[2].Y );
			F32vec4 N_zzzz = _mm_load_ps1( &Normals[2].Z );
			// Normals[i] = Matrix * Normals[i], w == 0
			_mm_store_ps( &Normals[2].X, _mm_add_ps( _mm_add_ps( _mm_mul_ps( M00, N_xxxx ),_mm_mul_ps( M10, N_yyyy ) ), _mm_mul_ps( M20, N_zzzz ) ) );
		}
		{
			// Splatter/ smear x/y/z components across respective vectors.
			F32vec4 N_xxxx = _mm_load_ps1( &Normals[3].X );
			F32vec4 N_yyyy = _mm_load_ps1( &Normals[3].Y );
			F32vec4 N_zzzz = _mm_load_ps1( &Normals[3].Z );
			// Normals[i] = Matrix * Normals[i], w == 0
			_mm_store_ps( &Normals[3].X, _mm_add_ps( _mm_add_ps( _mm_mul_ps( M00, N_xxxx ),_mm_mul_ps( M10, N_yyyy ) ), _mm_mul_ps( M20, N_zzzz ) ) );
		}
	}

	static FORCEINLINE void TransformNormalsAddTimesWeight( const FMatrix& Matrix, FPlane* SrcNormals, F32vec4* DstNormals, FLOAT Weight )
	{
		// FMatrix is 16 byte aligned.
		F32vec4 M00 = _mm_load_ps( &Matrix.M[0][0] );
		F32vec4 M10 = _mm_load_ps( &Matrix.M[1][0] );
		F32vec4 M20 = _mm_load_ps( &Matrix.M[2][0] );
		F32vec4 M30 = _mm_load_ps( &Matrix.M[3][0] );
		{
			// Splatter/ smear x/y/z components across respective vectors.
			F32vec4 N_xxxx = _mm_load_ps1( &SrcNormals[0].X );
			F32vec4 N_yyyy = _mm_load_ps1( &SrcNormals[0].Y );
			F32vec4 N_zzzz = _mm_load_ps1( &SrcNormals[0].Z );
			// DstNormals[0] += Weight * Matrix * SrcNormals[i], w == 1
			_mm_store_ps( (FLOAT*) &DstNormals[0], 
				_mm_add_ps( DstNormals[0], _mm_mul_ps( _mm_load_ps1( &Weight ), _mm_add_ps( _mm_add_ps( _mm_add_ps( _mm_mul_ps( M00, N_xxxx ),_mm_mul_ps( M10, N_yyyy ) ), _mm_mul_ps( M20, N_zzzz ) ), M30 ) ) ) );
		}
		{
			// Splatter/ smear x/y/z components across respective vectors.
			F32vec4 N_xxxx = _mm_load_ps1( &SrcNormals[1].X );
			F32vec4 N_yyyy = _mm_load_ps1( &SrcNormals[1].Y );
			F32vec4 N_zzzz = _mm_load_ps1( &SrcNormals[1].Z );
			// DstNormals[1] += Weight * Matrix * SrcNormals[i], w == 0
			_mm_store_ps( (FLOAT*) &DstNormals[1],
				_mm_add_ps( DstNormals[1], _mm_mul_ps( _mm_load_ps1( &Weight ), _mm_add_ps( _mm_add_ps( _mm_mul_ps( M00, N_xxxx ), _mm_mul_ps( M10, N_yyyy ) ), _mm_mul_ps( M20, N_zzzz ) ) ) ) );
		}
		{
			// Splatter/ smear x/y/z components across respective vectors.
			F32vec4 N_xxxx = _mm_load_ps1( &SrcNormals[2].X );
			F32vec4 N_yyyy = _mm_load_ps1( &SrcNormals[2].Y );
			F32vec4 N_zzzz = _mm_load_ps1( &SrcNormals[2].Z );
			// DstNormals[2] += Weight * Matrix * SrcNormals[i], w == 0
			_mm_store_ps( (FLOAT*) &DstNormals[2],
				_mm_add_ps( DstNormals[2], _mm_mul_ps( _mm_load_ps1( &Weight ), _mm_add_ps( _mm_add_ps( _mm_mul_ps( M00, N_xxxx ), _mm_mul_ps( M10, N_yyyy ) ), _mm_mul_ps( M20, N_zzzz ) ) ) ) );
		}
		{
			// Splatter/ smear x/y/z components across respective vectors.
			F32vec4 N_xxxx = _mm_load_ps1( &SrcNormals[3].X );
			F32vec4 N_yyyy = _mm_load_ps1( &SrcNormals[3].Y );
			F32vec4 N_zzzz = _mm_load_ps1( &SrcNormals[3].Z );
			// DstNormals[3] += Weight * Matrix * SrcNormals[i], w == 0
			_mm_store_ps( (FLOAT*) &DstNormals[3],
				_mm_add_ps( DstNormals[3], _mm_mul_ps( _mm_load_ps1( &Weight ), _mm_add_ps( _mm_add_ps( _mm_mul_ps( M00, N_xxxx ), _mm_mul_ps( M10, N_yyyy ) ), _mm_mul_ps( M20, N_zzzz ) ) ) ) );
		}			
	}

	static FORCEINLINE F32vec4 Unpack( const FPackedNormal& Normal )
	{
		return	_mm_sub_ps(																			// translate into -1 .. 1 range
					_mm_mul_ps(																		// scale into 0 .. 2 range
						_mm_cvtpu8_ps( *((__m64*)&Normal.Vector.Packed) ),							// convert unsigned bytes to floats (Normal has sufficient padding)
						SSE_UNPACK_INV_127_5),
					SSE_UNPACK_1											
				);
	}

	static FORCEINLINE FPackedNormal Pack( const F32vec4& Normal )
	{
		return	FPackedNormal
				(
					_mm_packs_pu16(																	// convert to unsigned bytes with saturation
						_mm_cvtps_pi16(																// convert to signed shorts
							_mm_add_ps( _mm_mul_ps( Normal, SSE_PACK_127_5 ), SSE_PACK_127_5 )),	// scale and translate into 0 .. 255 range (saturation occurs later)
							_mm_setzero_si64()									
					).m64_u32[0]
				);															
	}

#elif USE_ALTIVEC_SKINNING

	static FORCEINLINE void TransformNormals( const FMatrix& Matrix, __vector4* Normals )
	{
		__vector4 M00	= __lvx( &Matrix.M[0][0], 0 );
		__vector4 M10	= __lvx( &Matrix.M[1][0], 0 );
		__vector4 M20	= __lvx( &Matrix.M[2][0], 0 );
		__vector4 M30	= __lvx( &Matrix.M[3][0], 0 );
		{
			// Splatter/ smear x/y/z components across respective vectors.
			__vector4 N0_xxxx	=	__vspltw( Normals[0], 0 );
			__vector4 N0_yyyy	=	__vspltw( Normals[0], 1 );
			__vector4 N0_zzzz	=	__vspltw( Normals[0], 2 );

			// Normals[i] = Matrix * Normals[i], w == 1
			Normals[0]			=	__vaddfp(
										__vmaddfp(M00, N0_xxxx,__vmaddfp(M10, N0_yyyy, ALTIVEC_0)),
										// Just add the matrix, since w is assumed to be 1.0
										__vmaddfp(M20, N0_zzzz, M30));
		}
		{
			// Splatter/ smear x/y/z components across respective vectors.
			__vector4 N1_xxxx	=	__vspltw( Normals[1], 0 );
			__vector4 N1_yyyy	=	__vspltw( Normals[1], 1 );
			__vector4 N1_zzzz	=	__vspltw( Normals[1], 2 );

			// Normals[i] = Matrix * Normals[i], w == 0
			Normals[1]			=	__vaddfp(
										__vmaddfp(M00, N1_xxxx,__vmaddfp(M10, N1_yyyy, ALTIVEC_0)),
										// Just ignore M30, since w is assumed to be 0.0
										__vmaddfp(M20, N1_zzzz, ALTIVEC_0));
		}
		{
			// Splatter/ smear x/y/z components across respective vectors.
			__vector4 N2_xxxx	=	__vspltw( Normals[2], 0 );
			__vector4 N2_yyyy	=	__vspltw( Normals[2], 1 );
			__vector4 N2_zzzz	=	__vspltw( Normals[2], 2 );

			// Normals[i] = Matrix * Normals[i], w == 0
			Normals[2]			=	__vaddfp(
										__vmaddfp(M00, N2_xxxx,__vmaddfp(M10, N2_yyyy, ALTIVEC_0)),
										// Just ignore M30, since w is assumed to be 0.0
										__vmaddfp(M20, N2_zzzz, ALTIVEC_0));
		}
		{
			// Splatter/ smear x/y/z components across respective vectors.
			__vector4 N3_xxxx	=	__vspltw( Normals[3], 0 );
			__vector4 N3_yyyy	=	__vspltw( Normals[3], 1 );
			__vector4 N3_zzzz	=	__vspltw( Normals[3], 2 );

			// Normals[i] = Matrix * Normals[i], w == 0
			Normals[3]			=	__vaddfp(
										__vmaddfp(M00, N3_xxxx,__vmaddfp(M10, N3_yyyy, ALTIVEC_0)),
										// Just ignore M30, since w is assumed to be 0.0
										__vmaddfp(M20, N3_zzzz, ALTIVEC_0));
		}
	}

	static FORCEINLINE void TransformNormalsAddTimesWeight( const FMatrix& Matrix, __vector4* SrcNormals, __vector4* DstNormals, FLOAT InWeight )
	{
		__vector4 M00	= __lvx( &Matrix.M[0][0], 0 );
		__vector4 M10	= __lvx( &Matrix.M[1][0], 0 );
		__vector4 M20	= __lvx( &Matrix.M[2][0], 0 );
		__vector4 M30	= __lvx( &Matrix.M[3][0], 0 );
		__vector4 Weight;
		Weight.x		= InWeight;
		Weight			= __vspltw( Weight, 0 );
		{
			// Splatter/ smear x/y/z components across respective vectors.
			__vector4 N0_xxxx	=	__vspltw( SrcNormals[0], 0 );
			__vector4 N0_yyyy	=	__vspltw( SrcNormals[0], 1 );
			__vector4 N0_zzzz	=	__vspltw( SrcNormals[0], 2 );

			// DstNormals[1] += Weight * Matrix * SrcNormals[i], w == 1
			DstNormals[0]		=	__vmaddfp(
										__vaddfp(
											__vmaddfp(M00, N0_xxxx,__vmaddfp(M10, N0_yyyy, ALTIVEC_0)),
											// Just add the matrix, since w is assumed to be 1.0
											__vmaddfp(M20, N0_zzzz, M30)),
										Weight, DstNormals[0]);
		}
		{
			// Splatter/ smear x/y/z components across respective vectors.
			__vector4 N1_xxxx	=	__vspltw( SrcNormals[1], 0 );
			__vector4 N1_yyyy	=	__vspltw( SrcNormals[1], 1 );
			__vector4 N1_zzzz	=	__vspltw( SrcNormals[1], 2 );

			// DstNormals[1] += Weight * Matrix * SrcNormals[i], w == 0
			DstNormals[1]		=	__vmaddfp(
										__vaddfp(
											__vmaddfp(M00, N1_xxxx,__vmaddfp(M10, N1_yyyy, ALTIVEC_0)),
											// Just ignore M30, since w is assumed to be 0.0
											__vmaddfp(M20, N1_zzzz, ALTIVEC_0)),
										Weight, DstNormals[1]);
		}
		{
			// Splatter/ smear x/y/z components across respective vectors.
			__vector4 N2_xxxx	=	__vspltw( SrcNormals[2], 0 );
			__vector4 N2_yyyy	=	__vspltw( SrcNormals[2], 1 );
			__vector4 N2_zzzz	=	__vspltw( SrcNormals[2], 2 );

			// DstNormals[1] += Weight * Matrix * SrcNormals[i], w == 0
			DstNormals[2]		=	__vmaddfp(
										__vaddfp(
											__vmaddfp(M00, N2_xxxx,__vmaddfp(M10, N2_yyyy, ALTIVEC_0)),
											// Just ignore M30, since w is assumed to be 0.0
											__vmaddfp(M20, N2_zzzz, ALTIVEC_0)),
										Weight, DstNormals[2]);
		}
		{
			// Splatter/ smear x/y/z components across respective vectors.
			__vector4 N3_xxxx	= __vspltw( SrcNormals[3], 0 );
			__vector4 N3_yyyy	= __vspltw( SrcNormals[3], 1 );
			__vector4 N3_zzzz	= __vspltw( SrcNormals[3], 2 );

			// DstNormals[1] += Weight * Matrix * SrcNormals[i], w == 0
			DstNormals[3]		=	__vmaddfp(
										__vaddfp(
											__vmaddfp(M00, N3_xxxx,__vmaddfp(M10, N3_yyyy, ALTIVEC_0)),
											// Just ignore M30, since w is assumed to be 0.0
											__vmaddfp(M20, N3_zzzz, ALTIVEC_0)),
										Weight, DstNormals[3]);
		}
	}

	static FORCEINLINE __vector4 Unpack( const __vector4& PackedNormal )
	{
		return	__vmaddfp(
					__vcfux( __vperm( PackedNormal, ALTIVEC_0, ALTIVEC_PERMUTATION_MASK_UNPACK ), 0 ),	// unpack and convert unsigned bytes to float
					ALTIVEC_UNPACK_INV_127_5,															// scale into 0 .. 2 range
					ALTIVEC_UNPACK_MINUS_1																// translate into -1 .. 1 range
					);
	}

	static FORCEINLINE FPackedNormal Pack( const __vector4& Normal )
	{
		return	FPackedNormal(
					__vperm(
						__vpkuhus( ALTIVEC_0, __vpkuwus( ALTIVEC_0,										// convert unsigned dword -> unsiged short -> unsigned byte with saturation in last step
							__vctuxs( __vmaddfp( Normal, ALTIVEC_PACK_127_5, ALTIVEC_PACK_127_5 ), 0))),// scale and translate into 0.255 range and convert to unsigned word
						ALTIVEC_PERMUTATION_MASK_PACK, ALTIVEC_PERMUTATION_MASK_PACK
					).u[3]
				);
	}

#endif


	// CacheVertices

	void CacheVertices(INT LODIndex)
	{
		FCycleCounterSection	CycleCounter(GEngineStats.SkeletalSkinningTime);

		FStaticLODModel&	LOD = SkeletalMeshComponent->SkeletalMesh->LODModels(LODIndex);
		FMatrix*			ReferenceToLocal = new FMatrix[LOD.ActiveBoneIndices.Num()];

		for(UINT BoneIndex = 0;BoneIndex < (UINT)LOD.ActiveBoneIndices.Num();BoneIndex++)
			ReferenceToLocal[BoneIndex] = SkeletalMeshComponent->SkeletalMesh->RefBasesInvMatrix(LOD.ActiveBoneIndices(BoneIndex)) *
				SkeletalMeshComponent->SpaceBases(LOD.ActiveBoneIndices(BoneIndex));

		SkeletalMeshComponent->MeshObject->CachedFinalVertices.Empty(LOD.RigidVertices.Num() + LOD.SoftVertices.Num());
		SkeletalMeshComponent->MeshObject->CachedFinalVertices.Add(LOD.RigidVertices.Num() + LOD.SoftVertices.Num());

		FFinalSkinVertex*	DestVertex = &CachedFinalVertices(0);

#if USE_SSE_SKINNING
		//
		//	SSE software skinning codepath (uses MMX for data packing).
		//

		DWORD SSE_StatusRegister = _mm_getcsr();
		_mm_setcsr( SSE_StatusRegister | _MM_ROUND_TOWARD_ZERO );

		FRigidSkinVertex*	SrcRigidVertex = &LOD.RigidVertices(0);
		for(UINT VertexIndex = 0;VertexIndex < (UINT)LOD.RigidVertices.Num();VertexIndex++,SrcRigidVertex++,DestVertex++)
		{
			FPlane	Position = FPlane( SrcRigidVertex->Position, 1.f );
			F32vec4 Normals[4];

			Normals[0] = _mm_loadu_ps( &Position.X );
			Normals[1] = Unpack( SrcRigidVertex->TangentX );
			Normals[2] = Unpack( SrcRigidVertex->TangentY );
			Normals[3] = Unpack( SrcRigidVertex->TangentZ );

			TransformNormals( ReferenceToLocal[SrcRigidVertex->Bone], (FPlane*) Normals );

			_mm_storeu_ps( &DestVertex->Position.X, Normals[0] );

			DestVertex->TangentX = Pack( Normals[1] );
			DestVertex->TangentY = Pack( Normals[2] );
			DestVertex->TangentZ = Pack( Normals[3] );
            _mm_empty();
			DestVertex->U = SrcRigidVertex->U;
			DestVertex->V = SrcRigidVertex->V;
		}

		FSoftSkinVertex*	SrcSoftVertex = &LOD.SoftVertices(0);
		for(UINT VertexIndex = 0;VertexIndex < (UINT)LOD.SoftVertices.Num();VertexIndex++,SrcSoftVertex++,DestVertex++)
		{
			FPlane	Position = FPlane( SrcSoftVertex->Position, 1.f );
			F32vec4 SrcNormals[4], DstNormals[4];

			SrcNormals[0] = _mm_loadu_ps( &Position.X );
			SrcNormals[1] = Unpack( SrcSoftVertex->TangentX );
			SrcNormals[2] = Unpack( SrcSoftVertex->TangentY );
			SrcNormals[3] = Unpack( SrcSoftVertex->TangentZ );
			_mm_empty(); // calculation of weight relies on EMMS issued here.

			DstNormals[0] = _mm_setzero_ps();
			DstNormals[1] = _mm_setzero_ps();
			DstNormals[2] = _mm_setzero_ps();
			DstNormals[3] = _mm_setzero_ps();

			for( UINT InfluenceIndex=0; InfluenceIndex<4; InfluenceIndex++ )
			{
                _mm_empty();

				FLOAT Weight = SrcSoftVertex->InfluenceWeights[InfluenceIndex] * INV_255; // relies on EMMS above.
				TransformNormalsAddTimesWeight( ReferenceToLocal[SrcSoftVertex->InfluenceBones[InfluenceIndex]], (FPlane*) SrcNormals, DstNormals, Weight );
			}
			
			_mm_storeu_ps( &DestVertex->Position.X, DstNormals[0] );
			DestVertex->TangentX = Pack( DstNormals[1] );
			DestVertex->TangentY = Pack( DstNormals[2] );
			DestVertex->TangentZ = Pack( DstNormals[3] );

            _mm_empty();

			DestVertex->U = SrcSoftVertex->U;
			DestVertex->V = SrcSoftVertex->V;
		}

		_mm_empty();
		_mm_setcsr( SSE_StatusRegister );

#elif USE_ALTIVEC_SKINNING
		//
		//	Altivec software skinning codepath.
		//

		FRigidSkinVertex*	SrcRigidVertex = &LOD.RigidVertices(0);
		for(UINT VertexIndex = 0;VertexIndex < (UINT)LOD.RigidVertices.Num();VertexIndex++,SrcRigidVertex++,DestVertex++)
		{
			__vector4	Normals[4];

			Normals[0].x		= SrcRigidVertex->Position.X;
			Normals[0].y		= SrcRigidVertex->Position.Y;
			Normals[0].z		= SrcRigidVertex->Position.Z;
			Normals[0].w		= 1.f;

			Normals[1].u[3]		= SrcRigidVertex->TangentX.Vector.Packed;
			Normals[1]			= Unpack( Normals[1] );

			Normals[2].u[3]		= SrcRigidVertex->TangentY.Vector.Packed;
			Normals[2]			= Unpack( Normals[2] );

			Normals[3].u[3]		= SrcRigidVertex->TangentZ.Vector.Packed;
			Normals[3]			= Unpack( Normals[3] );

			TransformNormals( ReferenceToLocal[SrcRigidVertex->Bone], Normals );

			__stvx( Normals[0], &DestVertex->Position, 0 );
			DestVertex->TangentX = Pack( Normals[1] );
			DestVertex->TangentY = Pack( Normals[2] );
			DestVertex->TangentZ = Pack( Normals[3] );
			DestVertex->U = SrcRigidVertex->U;
			DestVertex->V = SrcRigidVertex->V;
		}
		
		FSoftSkinVertex* SrcSoftVertex = &LOD.SoftVertices(0);
		for(UINT VertexIndex = 0;VertexIndex < (UINT)LOD.SoftVertices.Num();VertexIndex++,SrcSoftVertex++,DestVertex++)
		{
			__vector4	SrcNormals[4], 
						DstNormals[4];

			SrcNormals[0].x		= SrcSoftVertex->Position.X;
			SrcNormals[0].y		= SrcSoftVertex->Position.Y;
			SrcNormals[0].z		= SrcSoftVertex->Position.Z;
			SrcNormals[0].w		= 1.f;

			SrcNormals[1].u[3]	= SrcSoftVertex->TangentX.Vector.Packed;
			SrcNormals[1]		= Unpack( SrcNormals[1] );

			SrcNormals[2].u[3]	= SrcSoftVertex->TangentY.Vector.Packed;
			SrcNormals[2]		= Unpack( SrcNormals[2] );

			SrcNormals[3].u[3]	= SrcSoftVertex->TangentZ.Vector.Packed;
			SrcNormals[3]		= Unpack( SrcNormals[3] );
			
			DstNormals[0]		= ALTIVEC_0;
			DstNormals[1]		= ALTIVEC_0;
			DstNormals[2]		= ALTIVEC_0;
			DstNormals[3]		= ALTIVEC_0;

			for( UINT InfluenceIndex=0; InfluenceIndex<4; InfluenceIndex++ )
			{
				FLOAT Weight = SrcSoftVertex->InfluenceWeights[InfluenceIndex] * INV_255;
				TransformNormalsAddTimesWeight( ReferenceToLocal[SrcSoftVertex->InfluenceBones[InfluenceIndex]], SrcNormals, DstNormals, Weight );
			}

			__stvx( DstNormals[0], &DestVertex->Position, 0 );
			DestVertex->TangentX = Pack( DstNormals[1] );
			DestVertex->TangentY = Pack( DstNormals[2] );
			DestVertex->TangentZ = Pack( DstNormals[3] );
			DestVertex->U = SrcSoftVertex->U;
			DestVertex->V = SrcSoftVertex->V;
		}

#else
		//
		//	Plain C++ software skinning codepath.
		//

		FRigidSkinVertex*	SrcRigidVertex = &LOD.RigidVertices(0);
		for(UINT VertexIndex = 0;VertexIndex < (UINT)LOD.RigidVertices.Num();VertexIndex++,SrcRigidVertex++,DestVertex++)
		{
			DestVertex->Position = ReferenceToLocal[SrcRigidVertex->Bone].TransformFVector(SrcRigidVertex->Position);
			DestVertex->TangentX = ReferenceToLocal[SrcRigidVertex->Bone].TransformNormal(SrcRigidVertex->TangentX);
			DestVertex->TangentY = ReferenceToLocal[SrcRigidVertex->Bone].TransformNormal(SrcRigidVertex->TangentY);
			DestVertex->TangentZ = ReferenceToLocal[SrcRigidVertex->Bone].TransformNormal(SrcRigidVertex->TangentZ);
			DestVertex->U = SrcRigidVertex->U;
			DestVertex->V = SrcRigidVertex->V;
		}

		FSoftSkinVertex*	SrcSoftVertex = &LOD.SoftVertices(0);
		for(UINT VertexIndex = 0;VertexIndex < (UINT)LOD.SoftVertices.Num();VertexIndex++,SrcSoftVertex++,DestVertex++)
		{
			FVector	SrcTangentX(SrcSoftVertex->TangentX),
					SrcTangentY(SrcSoftVertex->TangentY),
					SrcTangentZ(SrcSoftVertex->TangentZ);

			FVector	Position(0,0,0),
					TangentX(0,0,0),
					TangentY(0,0,0),
					TangentZ(0,0,0);

			for(UINT InfluenceIndex = 0;InfluenceIndex < 4;InfluenceIndex++)
			{
				const BYTE&		BoneIndex = SrcSoftVertex->InfluenceBones[InfluenceIndex];
				const FLOAT&	Weight = (FLOAT)SrcSoftVertex->InfluenceWeights[InfluenceIndex] / 255.0f;

				Position += ReferenceToLocal[BoneIndex].TransformFVector(SrcSoftVertex->Position) * Weight;
				TangentX += ReferenceToLocal[BoneIndex].TransformNormal(SrcTangentX) * Weight;
				TangentY += ReferenceToLocal[BoneIndex].TransformNormal(SrcTangentY) * Weight;
				TangentZ += ReferenceToLocal[BoneIndex].TransformNormal(SrcTangentZ) * Weight;
			}

			DestVertex->Position = Position;
			DestVertex->TangentX = TangentX;
			DestVertex->TangentY = TangentY;
			DestVertex->TangentZ = TangentZ;
			DestVertex->U = SrcSoftVertex->U;
			DestVertex->V = SrcSoftVertex->V;
		}

#endif

		GEngineStats.SkeletalSkinVertices.Value += LOD.RigidVertices.Num();
		GEngineStats.SkeletalSkinVertices.Value += LOD.SoftVertices.Num();

		delete [] ReferenceToLocal;

		CachedVertexLOD = LODIndex;

	}
};