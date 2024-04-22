/*=============================================================================
	DecalRenderData.cpp: Utility classes for rendering decals.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineDecalClasses.h"
#include "ScenePrivate.h"

IMPLEMENT_CLASS(UDecalMaterial);

/**
 * Helper function for determining when to render opaque and transparent decals
 * 
 * @param bSurfaceOpaqueRelevance - TRUE if the surface upon which the decals are placed has opaque materials attached
 * @param bSurfaceTranslucentRelevance - TRUE if the surface upon which the decals are placed has non-opaque materials attached
 * @param bTransparentRenderPass - TRUE if the rendering pipeline is in the translucency phase
 * @param bDrawOpaqueDecals - Return TRUE if opaque decals should be rendered now
 * @param bDrawTransparentDecals - Return TRUE if transparent decals should be rendered now
 */
void GetDrawDecalFilters(const UBOOL bSurfaceOpaqueRelevance, const UBOOL bSurfaceTranslucentRelevance, const UBOOL bTransparentRenderPass, UBOOL& bDrawOpaqueDecals, UBOOL& bDrawTransparentDecals)
{
	if (bTransparentRenderPass)
	{
		//during the transparent pass, the only decals remaining on transparent decals (on transparent surfaces)
		bDrawOpaqueDecals = FALSE;
		bDrawTransparentDecals = TRUE;
	}
	else
	{
		bDrawOpaqueDecals = TRUE;
		//if parts of the surface will be done in the translucent pass, do not render transparent decals now.
		bDrawTransparentDecals = !bSurfaceTranslucentRelevance;

	}
}


/*-----------------------------------------------------------------------------
   FDecalVertex
-----------------------------------------------------------------------------*/

/**
 * FDecalVertex serializer.
 */
FArchive& operator<<(FArchive& Ar, FDecalVertex& DecalVertex)
{
	Ar << DecalVertex.Position;
	if( Ar.Ver() < VER_DECAL_VERTEX_FACTORY_VER1 )
	{
		Ar << DecalVertex.TangentX;
		Ar << DecalVertex.TangentZ;
		// removed UVs
		FVector2D LegacyProjectedUVs;
		Ar << LegacyProjectedUVs;
	}
	else if( Ar.Ver() < VER_DECAL_VERTEX_FACTORY_VER2 )
	{
		DecalVertex.TangentX = FVector(1,0,0);
		Ar << DecalVertex.TangentZ;
	}
	else
	{
		Ar << DecalVertex.TangentX;
		Ar << DecalVertex.TangentZ;
	}
	
	Ar << DecalVertex.LightMapCoordinate;
	if( Ar.Ver() < VER_DECAL_REMOVED_2X2_NORMAL_TRANSFORM )
	{
		// removed UVs for 2x2 normal transform
		FVector2D LegacyNormalTransform[2];
		Ar << LegacyNormalTransform[0];
		Ar << LegacyNormalTransform[1];
	}
	return Ar;
}

/*-----------------------------------------------------------------------------
   FDecalVertexBuffer
-----------------------------------------------------------------------------*/

FDecalVertexBuffer::FDecalVertexBuffer(FDecalRenderData* InDecalRenderData)
:	DecalRenderData( InDecalRenderData )
,	NumVertices(0)
{
	
}

/**
 * FRenderResource interface for FDecalVertexBuffer.
 * Has to be defined after FDecalRenderData has been properly declared.
 */
void FDecalVertexBuffer::InitRHI()
{
	NumVertices = DecalRenderData->Vertices.Num();
	if( NumVertices > 0 )
	{
		const UINT Size = NumVertices * sizeof(FDecalVertex);
		VertexBufferRHI = RHICreateVertexBuffer( Size, NULL, RUF_Static );
		void* DestBuf = RHILockVertexBuffer( VertexBufferRHI, 0, Size, FALSE );
		appMemcpy( DestBuf, DecalRenderData->Vertices.GetData(), Size );		
		RHIUnlockVertexBuffer( VertexBufferRHI );
	}
}

/*-----------------------------------------------------------------------------
	FDecalLocalSpaceInfo
-----------------------------------------------------------------------------*/
FDecalLocalSpaceInfo::FDecalLocalSpaceInfo(
	const FDecalState* InDecal, 
	const FMatrix& InReceiverLocalToWorld, 
	const FMatrix& InReceiverWorldToLocal)
	:	Decal(InDecal)
{
	check(Decal);

	// world to projected texture space
	TextureTransform = InReceiverLocalToWorld * Decal->WorldTexCoordMtx;

	// transform decal hit location to local space
	LocalLocation = InReceiverWorldToLocal.TransformFVector( Decal->HitLocation );

	// transform decal tangents to local space
	// used for generating projected tangent basis for vertices	
	LocalTangent = InReceiverWorldToLocal.TransformNormal( Decal->HitTangent ).SafeNormal();	
	LocalBinormal = InReceiverWorldToLocal.TransformNormal( Decal->HitBinormal ).SafeNormal();
	// account for flipped decal frame as well as local transform
	LocalNormal = (LocalTangent ^ LocalBinormal) * (InReceiverWorldToLocal.Determinant() * (Decal->bFlipBackfaceDirection ? -1.f : 1.f));
}

/*-----------------------------------------------------------------------------
	FDecalLocalSpaceInfoClip
-----------------------------------------------------------------------------*/

FDecalLocalSpaceInfoClip::FDecalLocalSpaceInfoClip(
	const FDecalState* InDecal, 
	const FMatrix& InReceiverLocalToWorld, 
	const FMatrix& InReceiverWorldToLocal)
	:	FDecalLocalSpaceInfo(InDecal,InReceiverLocalToWorld,InReceiverWorldToLocal)
{
	// transform world space hit location to decal texture space
	// used for generating vertex UVs
	TextureHitLocation		= Decal->WorldTexCoordMtx.TransformFVector( Decal->HitLocation );
	
	// transform decal frustum planes to local space
	// used for clipping of vertices
	for ( INT PlaneIndex = 0 ; PlaneIndex < Decal->Planes.Num() ; ++PlaneIndex )
	{
		Convex.Planes.AddItem( Decal->Planes(PlaneIndex).TransformBy( InReceiverWorldToLocal ) );
	}	

	// transform decal orientation vector to local space
	// used for culling of backfacing tris
	LocalLookVector = InReceiverWorldToLocal.TransformNormal( Decal->OrientationVector ).SafeNormal();
}

/**
 * Computes decal texture coordinates from the the specified world-space position.
 *
 * @param		InPos			World-space position.
 * @param		OutTexCoords	[out] Decal texture coordinates.
 */
void FDecalLocalSpaceInfoClip::ComputeTextureCoordinates(const FVector& InPos, FVector2D& OutTexCoords) const
{
	const FVector OutPos = TextureTransform.TransformFVector( InPos ) - TextureHitLocation;
	OutTexCoords.X = (-OutPos.X+0.5f+Decal->OffsetX);
	OutTexCoords.Y = (-OutPos.Y+0.5f+Decal->OffsetY);
}

/*-----------------------------------------------------------------------------
   FDecalRenderData
-----------------------------------------------------------------------------*/

/**
 * Prepares resources for deletion.
 * This is only called by the rendering thread.
 */
FDecalRenderData::~FDecalRenderData()
{
	DEC_DWORD_STAT_BY( STAT_DecalVertexMemory, NumVerticesInitialized * sizeof(FDecalVertex) );
	DEC_DWORD_STAT_BY( STAT_DecalIndexMemory, NumIndicesInitialized * sizeof(WORD) );

	// Prepare resources for deletion.
	// ReleaseResources_RenderingThread() will assert we're in the rendering thread.
	ReleaseResources_RenderingThread();

	delete DecalVertexFactory;
	DecalVertexFactory = NULL;

	// Delete receiver resource structs.  These will assert if any resources are still initialized.
	for ( INT Index = 0 ; Index < ReceiverResources.Num() ; ++Index )
	{
		FReceiverResource* Resource = ReceiverResources(Index);
		delete Resource;
	}
}

/**
 * Initializes resources.
 * This is only called by the game thread, when a receiver is attached to a decal.
 */
void FDecalRenderData::InitResources_GameThread()
{
	check(IsInGameThread()); 
	if( NumTriangles > 0 )
	{
		if( ReceiverVertexFactory )
		{
			check(!DecalVertexFactory);
			DecalVertexFactory = ReceiverVertexFactory->CreateDecalVertexFactory();
			if( DecalVertexFactory )
			{
				DecalVertexFactory->SetDecalMinMaxBlend(DecalBlendRange);
				BeginInitResource( DecalVertexFactory->CastToFVertexFactory() );
			}
		}
		else if ( bUsesVertexResources )
		{
			NumVerticesInitialized = Vertices.Num();
			INC_DWORD_STAT_BY( STAT_DecalVertexMemory, NumVerticesInitialized * sizeof(FDecalVertex) );

			BeginInitResource( &DecalVertexBuffer );

			// FLocalDecalVertexFactory always used for decal receiver with its own generated vertex data 
			check(!DecalVertexFactory);
			DecalVertexFactory = new FLocalDecalVertexFactory();
			DecalVertexFactory->SetDecalMinMaxBlend(DecalBlendRange);

			// Initialize the decal's vertex factory.
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
				InitVertexFactory,
				FLocalDecalVertexFactory*,VertexFactory,(FLocalDecalVertexFactory*)DecalVertexFactory,
				FDecalVertexBuffer*,DecalVertexBuffer,&DecalVertexBuffer, 
				{
					FLocalDecalVertexFactory::DataType VertexFactoryData;

					VertexFactoryData.PositionComponent			= STRUCTMEMBER_VERTEXSTREAMCOMPONENT( DecalVertexBuffer, FDecalVertex, Position, VET_Float3 );
					VertexFactoryData.TangentBasisComponents[0]	= STRUCTMEMBER_VERTEXSTREAMCOMPONENT( DecalVertexBuffer, FDecalVertex, TangentX, VET_PackedNormal );
					VertexFactoryData.TangentBasisComponents[1]	= STRUCTMEMBER_VERTEXSTREAMCOMPONENT( DecalVertexBuffer, FDecalVertex, TangentZ, VET_PackedNormal );

					// Texture coordinates.
					VertexFactoryData.TextureCoordinates.Empty();
					VertexFactoryData.ShadowMapCoordinateComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT( DecalVertexBuffer, FDecalVertex, LightMapCoordinate, VET_Float2 );

					VertexFactory->SetData(VertexFactoryData);
				});
			BeginInitResource( DecalVertexFactory->CastToFVertexFactory() );
		}
		if ( bUsesIndexResources )
		{
			NumIndicesInitialized = IndexBuffer.Indices.Num();
			INC_DWORD_STAT_BY( STAT_DecalIndexMemory, NumIndicesInitialized * sizeof(WORD) );

			BeginInitResource( &IndexBuffer );
		}
	}

	if (LightMap1D != NULL)
	{
		LightMap1D->InitResources();
	}
	for (INT ShadowMap1DIdx=0; ShadowMap1DIdx < ShadowMap1D.Num(); ShadowMap1DIdx++)
	{
		if (ShadowMap1D(ShadowMap1DIdx) != NULL)
		{
			BeginInitResource(ShadowMap1D(ShadowMap1DIdx));
		}
	}

	// free decal render data memory after RHI resources have been initialized
	if( GIsGame && !GIsEditor &&
		(Vertices.Num() > 0 || IndexBuffer.Indices.Num() > 0 ) )
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FreeDecalRenderData,
			FDecalRenderData*,DecalRenderData,this,
		{
			DecalRenderData->Vertices.Empty();
			DecalRenderData->IndexBuffer.Indices.Empty();
		});
	}
}

/**
 * Prepares resources for deletion.
 * This is only called by the rendering thread.
 */
void FDecalRenderData::ReleaseResources_RenderingThread()
{
	check(IsInRenderingThread());
	if( NumTriangles > 0 )
	{
		if( bUsesVertexResources )
		{
			DecalVertexBuffer.ReleaseResource();			
		}
		if( bUsesIndexResources )
		{
			IndexBuffer.ReleaseResource();
		}
	}
	if( DecalVertexFactory )
	{
		DecalVertexFactory->CastToFVertexFactory()->ReleaseResource();				
	}
	// Release any receiver-specific resources.
	for ( INT Index = 0 ; Index < ReceiverResources.Num() ; ++Index )
	{
		ReceiverResources(Index)->Release_RenderingThread();
	}
}

/**
* Render clipped decal vertices and their tangents 
*/
void FDecalRenderData::DebugDraw(FPrimitiveDrawInterface* PDI, const FDecalState& DecalState, const FMatrix& LocalToWorld, INT DPGIdx) const
{
	const FLOAT TangentDrawScale = 100.0f;
	const FLOAT PointDrawScale = 5.0f;
	const FColor White(255, 255, 255);
	const FColor Red(255,0,0);
	const FColor Green(0,255,0);
	const FColor Blue(0,0,255);

	for( INT Idx=0; Idx < Vertices.Num(); Idx++ )
	{
		const FDecalVertex& DecalVertex = Vertices(Idx);
		FVector DecalPosition = LocalToWorld.TransformFVector(DecalVertex.Position);
		FVector DecalNormal = LocalToWorld.TransformNormal(DecalVertex.TangentZ).SafeNormal();
		FVector DecalTangent = DecalNormal ^ (DecalNormal ^ DecalState.HitTangent).SafeNormal();
		FVector DecalBinormal = DecalNormal ^ (DecalNormal ^ DecalState.HitBinormal).SafeNormal();

		PDI->DrawPoint(
			DecalPosition,
			White,
			PointDrawScale,
			DPGIdx
			);

		PDI->DrawLine( 
			DecalPosition, 
			DecalPosition + (DecalTangent*TangentDrawScale), 
			Red,
			DPGIdx );

		PDI->DrawLine( 
			DecalPosition, 
			DecalPosition + (DecalBinormal*TangentDrawScale), 
			Green,
			DPGIdx );

		PDI->DrawLine( 
			DecalPosition, 
			DecalPosition + (DecalNormal*TangentDrawScale), 
			Blue,
			DPGIdx );
	}
}

/** 
* @return memory usage in bytes for the render data for a given decal
*/
INT FDecalRenderData::GetMemoryUsage()
{
	INT MemoryCount=0;
	
	FArchiveCountMem ArCountMem(NULL);
	ArCountMem << Vertices;
	ArCountMem << IndexBuffer;
	ArCountMem << LightMap1D;
	ArCountMem << ShadowMap1D;
	ArCountMem << RigidVertices;
	ArCountMem << SoftVertices;
	ArCountMem << SampleRemapping;
	MemoryCount += ArCountMem.GetMax();

	// Account for memory that's actually in RHI buffers
	if (Vertices.Num() == 0)
	{
		MemoryCount += NumVerticesInitialized * sizeof(FDecalVertex);
	}
	if (IndexBuffer.Indices.Num() == 0)
	{
		MemoryCount += NumIndicesInitialized * sizeof(WORD);
	}

	for( INT ReceiverIdx=0; ReceiverIdx < ReceiverResources.Num(); ReceiverIdx++ )
	{
		FReceiverResource* ReceiverResource = ReceiverResources(ReceiverIdx);
		if( ReceiverResource )
		{
			MemoryCount += ReceiverResource->GetMemoryUsage();
		}
	}

	return MemoryCount;
}

/**
 * FStaticReceiverData serializer.
 */
FArchive& operator<<(FArchive& Ar, FStaticReceiverData& Tgt)
{
	Ar << Tgt.Component;
	Tgt.Vertices.BulkSerialize(Ar);
	Tgt.Indices.BulkSerialize(Ar);
	Ar << Tgt.NumTriangles;
	Ar << Tgt.LightMap1D;
	if (Ar.Ver() >= VER_DECAL_SHADOWMAPS)
	{
		Ar << Tgt.ShadowMap1D;
	}
	if (Ar.Ver() >= VER_DECAL_SERIALIZE_BSP_ELEMENT)
	{
		Ar << Tgt.Data;
	}
	if( Ar.Ver() >= VER_STATIC_DECAL_INSTANCE_INDEX )
	{
		Ar << Tgt.InstanceIndex;
	}
	return Ar;
}

/*-----------------------------------------------------------------------------
	FDecalMaterialInstance
-----------------------------------------------------------------------------*/

struct FDecalMaterialInstance : FMaterialRenderProxy
{
public:
	FMaterialRenderProxy* ParentInstance;

	FDecalMaterialInstance(FMaterialRenderProxy* InParentInstance, FLOAT InDepthBias, FLOAT InSlopeScaleDepthBias)
		:	ParentInstance( InParentInstance )
	{
	}

	// FMaterialRenderProxy interface.  Simply forward calls on to the parent
	virtual UBOOL GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	{
		return ParentInstance->GetVectorValue( ParameterName, OutValue, Context );
	}
	virtual UBOOL GetScalarValue(const FName ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const
	{
		return ParentInstance->GetScalarValue( ParameterName, OutValue, Context );
	}
	virtual UBOOL GetTextureValue(const FName ParameterName, const FTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		return ParentInstance->GetTextureValue( ParameterName, OutValue, Context );
	}
};

/*-----------------------------------------------------------------------------
	FDecalMaterial
-----------------------------------------------------------------------------*/

namespace {

struct FDecalMaterial : public FMaterialResource
{
public:
	FDecalMaterial(UMaterial* InMaterial)
		:	FMaterialResource(InMaterial)
	{}

	virtual UBOOL IsDecalMaterial() const
	{
		return TRUE;
	}

	virtual UBOOL IsUsedWithDecals() const
	{
		return TRUE;
	}

	/**
	 * Should the shader for this material with the given platform, shader type and vertex 
	 * factory type combination be compiled.
	 *
	 * @param	Platform			The platform currently being compiled for.
	 * @param	ShaderType			Which shader is being compiled.
	 * @param	VertexFactory		Which vertex factory is being compiled (can be NULL).
	 *
	 * @return						TRUE if the shader should be compiled.
	 */
	virtual UBOOL ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const
	{
		// Don't compile decal materials for HitProxy shaders.

#if 0
		// AJS: I disabled this to solve a crash when a decal material is applied directly to a mesh.
		// Is that the right fix?
		if ( appStristr(ShaderType->GetName(), TEXT("HitProxy")) )
		{
			return FALSE;
		}
#endif
	
		return TRUE;
	}

	/** Entry point for compiling a specific material property.  This must call SetMaterialProperty. */
	INT CompileProperty(EMaterialProperty Property,FMaterialCompiler* Compiler) const
	{
		// If the property is not active, don't compile it
		if (!IsActiveMaterialProperty(Material, Property))
		{
			return INDEX_NONE;
		}
		
		const EShaderFrequency ShaderFrequency = GetMaterialPropertyShaderFrequency(Property);
		Compiler->SetMaterialProperty(Property);
		INT SelectionColorIndex = INDEX_NONE;
		if (ShaderFrequency == SF_Pixel)
		{
			SelectionColorIndex = Compiler->Mul(Compiler->ComponentMask(Compiler->VectorParameter(NAME_SelectionColor,FLinearColor::Black),1,1,1,0), Compiler->PerInstanceSelectionMask());
		}

		switch(Property)
		{
		case MP_EmissiveColor: 
			return Compiler->Add(Compiler->ForceCast(Material->EmissiveColor.Compile(Compiler,FColor(0,0,0)),MCT_Float3),SelectionColorIndex);
		case MP_Opacity: return Material->Opacity.Compile(Compiler,1.0f);
		case MP_OpacityMask: return Material->OpacityMask.Compile(Compiler,1.0f);
		case MP_Distortion: return Material->Distortion.Compile(Compiler,FVector2D(0,0));
		case MP_TwoSidedLightingMask: return Compiler->Mul(Compiler->ForceCast(Material->TwoSidedLightingMask.Compile(Compiler,0.0f),MCT_Float),Material->TwoSidedLightingColor.Compile(Compiler,FColor(255,255,255)));
		case MP_DiffuseColor: 
			return Compiler->Mul(Compiler->ForceCast(Material->DiffuseColor.Compile(Compiler,FColor(128,128,128)),MCT_Float3),Compiler->Sub(Compiler->Constant(1.0f),SelectionColorIndex));
		case MP_DiffusePower: return Material->DiffusePower.Compile(Compiler,1.0f);
		case MP_SpecularColor: return Material->SpecularColor.Compile(Compiler,FColor(0,0,0));
		case MP_SpecularPower: return Material->SpecularPower.Compile(Compiler,15.0f);
		case MP_Normal: return Material->Normal.Compile(Compiler,FVector(0,0,1));
		case MP_CustomLighting: return Material->CustomLighting.Compile(Compiler,FColor(0,0,0));
		case MP_CustomLightingDiffuse: return Material->CustomSkylightDiffuse.Compile(Compiler,FColor(0,0,0));
		case MP_AnisotropicDirection: return Material->AnisotropicDirection.Compile(Compiler,FVector(0,1,0));
		default:
			return INDEX_NONE;
		};
	}
};

} // namespace

/*-----------------------------------------------------------------------------
	UDecalMaterial
-----------------------------------------------------------------------------*/

FMaterialResource* UDecalMaterial::AllocateResource()
{
	return new FDecalMaterial(this);
}

void UDecalMaterial::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// make sure usage flag is set for decals
	CheckMaterialUsage(MATUSAGE_Decals);
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UDecalMaterial::PreSave()
{
	Super::PreSave();
}

void UDecalMaterial::PostLoad()
{
	Super::PostLoad();
}

void UDecalMaterial::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// force recompile materials based on decal version
	if( Ar.Ver() < VER_MIN_COMPILEDMATERIAL_DECAL || Ar.LicenseeVer() < LICENSEE_VER_MIN_COMPILEDMATERIAL_DECAL )
	{
		for( INT Idx=0; Idx < MSQ_MAX; Idx++ )
		{
			if( MaterialResources[Idx] != NULL )
			{
				MaterialResources[Idx]->bValidCompilationOutput = FALSE;
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	FDecalPoly
-----------------------------------------------------------------------------*/

static FORCEINLINE void Copy(FDecalPoly* Dst, const FDecalPoly* Src, INT Index)
{
	new(Dst->Vertices) FVector( Src->Vertices(Index) );
	new(Dst->ShadowTexCoords) FVector2D( Src->ShadowTexCoords(Index) );
	Dst->Indices.AddItem( Src->Indices(Index) );
}


// Split a poly and keep only the front half.
// @return		Number of vertices, 0 if clipped away.
INT FDecalPoly::Split( const FVector &Normal, const FVector &Base )
{
	// Split it.
	static FDecalPoly Front;
	Front.Init();
	switch( SplitWithPlaneFast( FPlane(Base,Normal), &Front ))
	{
	case SP_Back:
		return 0;
	case SP_Split:
		*this = Front;
		return Vertices.Num();
	default:
		return Vertices.Num();
	}
}

/**
 * Split with plane quickly for in-game geometry operations.
 * Results are always valid. May return sliver polys.
 */
INT FDecalPoly::SplitWithPlaneFast(const FPlane& Plane, FDecalPoly* FrontPoly) const
{
	enum EPlaneClassification
	{
		V_FRONT=0,
		V_BACK=1
	};
	EPlaneClassification Status,PrevStatus;
	check(Vertices.Num());
	EPlaneClassification* VertStatus = (EPlaneClassification*) appAlloca( sizeof(EPlaneClassification) * Vertices.Num() );
	INT Front=0,Back=0;

	EPlaneClassification* StatusPtr = &VertStatus[0];
	check(StatusPtr);
	for( INT i=0; i<Vertices.Num(); i++ )
	{
		const FLOAT Dist = Plane.PlaneDot(Vertices(i));
		if( Dist >= 0.f )
		{
			*StatusPtr++ = V_FRONT;
			if( Dist > +THRESH_SPLIT_POLY_WITH_PLANE )
			{
				Front=1;
			}
		}
		else
		{
			*StatusPtr++ = V_BACK;
			if( Dist < -THRESH_SPLIT_POLY_WITH_PLANE )
			{
				Back=1;
			}
		}
	}
	ESplitType Result;
	if( !Front )
	{
		if( Back ) Result = SP_Back;
		else       Result = SP_Coplanar;
	}
	else if( !Back )
	{
		Result = SP_Front;
	}
	else
	{
		// Split.
		if( FrontPoly )
		{
			const FVector	*V		= &Vertices(0);
			const FVector	*W		= &Vertices(Vertices.Num()-1);
			const FVector2D *STCV	= &ShadowTexCoords(0);
			const FVector2D *STCW	= &ShadowTexCoords(ShadowTexCoords.Num()-1);
			PrevStatus				= VertStatus[Vertices.Num()-1];
			StatusPtr				= &VertStatus[0];

			for( INT i=0; i<Vertices.Num(); i++ )
			{
				Status = *StatusPtr++;
				if( Status != PrevStatus )
				{
					// Crossing.
					const FVector& P1 = *W;
					const FVector& P2 = *V;
					const FVector P2MP1(P2-P1);
					const FLOAT t = ((Plane.W - (P1|Plane))/(P2MP1|Plane));
					const FVector Intersection = P1 + (P2MP1*t);

					// ShadowTexCoord
					const FVector2D& STC1 = *STCW;
					const FVector2D& STC2 = *STCV;
					const FVector2D STC2MSTC1(STC2-STC1);
					const FVector2D NewShadowTexCoord = STC1 + (STC2MSTC1*t);

					new(FrontPoly->Vertices) FVector(Intersection);
					new(FrontPoly->ShadowTexCoords) FVector2D(NewShadowTexCoord);
					FrontPoly->Indices.AddItem( Indices(i) );

					if( PrevStatus == V_BACK )
					{			
						Copy(FrontPoly, this, i);
					}
				}
				else if( Status==V_FRONT )
				{
					Copy(FrontPoly, this, i);
				}

				PrevStatus = Status;
				W          = V++;
				STCW       = STCV++;
			}
			FrontPoly->FaceNormal	= FaceNormal;
		}
		Result = SP_Split;
	}

	return Result;
}

/*-----------------------------------------------------------------------------
FDecalState
-----------------------------------------------------------------------------*/


/**
 * Transforms the decal frustum vertices by the specified matrix.
 */
void FDecalState::TransformFrustumVerts(const FMatrix& FrustumVertexTransform)
{
	// Transform decal frustum verts by the specified matrix.
	for ( INT Index = 0 ; Index < 8 ; ++Index )
	{
		FrustumVerts[Index] = FrustumVertexTransform.TransformFVector( FrustumVerts[Index] );
	}
}
void FDecalState::TransformFrustumVerts(const FBoneAtom& FrustumVertexTransform)
{
	// Transform decal frustum verts by the specified matrix.
	for ( INT Index = 0 ; Index < 8 ; ++Index )
	{
		FrustumVerts[Index] = FrustumVertexTransform.TransformFVector( FrustumVerts[Index] );
	}
}

/**
* Update AttachmentLocalToWorld and anything dependent on it
*/
void FDecalState::UpdateAttachmentLocalToWorld(const FMatrix& InAttachmentLocalToWorld)
{
	AttachmentLocalToWorld = InAttachmentLocalToWorld;
	FMatrix AttachmentLocalToWorldInv = AttachmentLocalToWorld.Inverse();

	// transform far plane distance with inverse of attachment local to world transform
	FVector FarVec = HitNormal * FarPlaneDistance;
	FarVec = AttachmentLocalToWorldInv.TransformNormal(FarVec);
	FarPlaneDistance = FarVec.Size() * (FarPlaneDistance > 0.f ? 1.f : -1.f);
	// transform near plane distance with inverse of attachment local to world transform
	FVector NearVec = HitNormal * NearPlaneDistance;
	NearVec = AttachmentLocalToWorldInv.TransformNormal(NearVec);
	NearPlaneDistance = NearVec.Size() * (NearPlaneDistance > 0.f ? 1.f : -1.f);
}

/** 
* @return memory used by this struct 
*/
DWORD FDecalState::GetMemoryFootprint()
{
 	FArchiveCountMem ArCountMem(NULL);
 	ArCountMem << Planes;
 	ArCountMem << HitNodeIndices;
 	return ArCountMem.GetMax() + sizeof(*this);
}

/**
 * Computes an axis-aligned bounding box of the decal frustum vertices projected to the screen.
 *
 * @param		SceneView				Scene projection
 * @param		OutMin					[out] Min vertex of the screen AABB.
 * @param		OutMax					[out] Max vertex of the screen AABB.
 * @param		FrustumVertexTransform	A transform applied to the frustum verts before screen projection.
 * @return								FALSE if the AABB has zero area, TRUE otherwise.
 */
UBOOL FDecalState::QuadToClippedScreenSpaceAABB(const FSceneView* SceneView, FVector2D& OutMin, FVector2D& OutMax, const FMatrix& FrustumVertexTransform) const
{
	#define GEnableScissorTestForDecals TRUE
	#define GShowFullyVisible           TRUE
	#define GShowPartiallyVisible       TRUE

	const FVector2D MinView(SceneView->ClipX, SceneView->ClipY);
	const FVector2D MaxView(SceneView->ClipX + SceneView->SizeX, SceneView->ClipY + SceneView->SizeY);

	if ( !GEnableScissorTestForDecals )
	{
		OutMin = MinView;
		OutMax = MaxView;
		return TRUE;
	}

	enum {
		Front        = 0x01,
		Back         = 0x02,
		FrontAndBack = Front | Back,
	};

	FVector4 ScreenPoints[8];
	INT Sides[8];
	INT TotalSide = 0;

	// Transform decal frustum verts to screen space and classify sidedness w.r.t the near clipping plane.
	for ( INT Index = 0 ; Index < 8 ; ++Index )
	{
		ScreenPoints[Index] = SceneView->WorldToScreen( FrustumVertexTransform.TransformFVector(FrustumVerts[Index]) );
		Sides[Index] = (ScreenPoints[Index].W > SceneView->NearClippingDistance) ? Front : Back;
		TotalSide |= Sides[Index];
	}

	// Return if all verts project behind the near clipping plane.
	if ( TotalSide == Back )
	{
		return FALSE;
	}

	FVector2D MinCorner(FLT_MAX, FLT_MAX);
	FVector2D MaxCorner(-FLT_MAX, -FLT_MAX);

	const FLOAT HalfWidth  = SceneView->SizeX * 0.5f;
	const FLOAT HalfHeight = SceneView->SizeY * 0.5f;

	const FLOAT X = SceneView->ClipX + HalfWidth;
	const FLOAT Y = SceneView->ClipY + HalfHeight;

	// If all verts project in front of the near clipping plane . . .
	if ( GShowFullyVisible && (TotalSide == Front) )
	{
		for ( INT Index = 0 ; Index < 8 ; ++Index )
		{
			const FLOAT InvW = 1.0f / ScreenPoints[Index].W;
			const FVector2D Pixel(
			X + ScreenPoints[Index].X * InvW * HalfWidth,
			Y - ScreenPoints[Index].Y * InvW * HalfHeight );

			MinCorner.X = Min(Pixel.X, MinCorner.X);
			MinCorner.Y = Min(Pixel.Y, MinCorner.Y);
			MaxCorner.X = Max(Pixel.X, MaxCorner.X);
			MaxCorner.Y = Max(Pixel.Y, MaxCorner.Y);
		}
	}
	else
	// If some verts verts project in front and some project in back . . .
	if ( GShowPartiallyVisible && (TotalSide == FrontAndBack) )
	{
		// decal frustum verts (near)
		// 0 1
		// 3 2

		// decal frustum verts (far)
		// 4 5
		// 7 6

		for ( INT Index = 0 ; Index < 8 ; ++Index )
		{
			// 8 individual points
			if ( Sides[Index] == Front )
			{
				// project from clip space to pixel
				const FLOAT InvW = 1.0f / ScreenPoints[Index].W;
				const FVector2D Pixel(
					X + ScreenPoints[Index].X * InvW * HalfWidth,
					Y - ScreenPoints[Index].Y * InvW * HalfHeight );

				// get min,max screen bounds
				MinCorner.X = Min(Pixel.X, MinCorner.X);
				MinCorner.Y = Min(Pixel.Y, MinCorner.Y);
				MaxCorner.X = Max(Pixel.X, MaxCorner.X);
				MaxCorner.Y = Max(Pixel.Y, MaxCorner.Y);
			}

			const INT NextIndex = (Index & ~3) | ((Index + 1) & 3);

			// 8 edges normal to the projection direction
			if ( (Sides[Index] | Sides[NextIndex]) == FrontAndBack )
			{
				// find intersection point on the near plane for the two points
				const FLOAT T = (SceneView->NearClippingDistance -
					ScreenPoints[Index].W) / (ScreenPoints[NextIndex].W -
					ScreenPoints[Index].W);
				const FVector2D ClipPoint(
					ScreenPoints[Index].X * (1 - T) + ScreenPoints[NextIndex].X * T,
					ScreenPoints[Index].Y * (1 - T) + ScreenPoints[NextIndex].Y * T );

				// project from clip space to pixel
				const FLOAT InvW = 1.0f / SceneView->NearClippingDistance;
				const FVector2D Pixel(
					X + ClipPoint.X * InvW * HalfWidth,
					Y - ClipPoint.Y * InvW * HalfHeight );

				// get min,max screen bounds
				MinCorner.X = Min(Pixel.X, MinCorner.X);
				MinCorner.Y = Min(Pixel.Y, MinCorner.Y);
				MaxCorner.X = Max(Pixel.X, MaxCorner.X);
				MaxCorner.Y = Max(Pixel.Y, MaxCorner.Y);
			}
		}

		for ( INT Index = 0 ; Index < 4 ; ++Index )
		{
			const INT NextIndex = (Index + 4) & 7;

			// 4 remaining edges
			if ( (Sides[Index] | Sides[NextIndex]) == FrontAndBack )
			{
				// find intersection point on the near plane for the two points
				const FLOAT T = (SceneView->NearClippingDistance - ScreenPoints[Index].W) / (ScreenPoints[NextIndex].W - ScreenPoints[Index].W);
				const FVector2D ClipPoint(
					ScreenPoints[Index].X * (1 - T) + ScreenPoints[NextIndex].X * T,
					ScreenPoints[Index].Y * (1 - T) + ScreenPoints[NextIndex].Y * T );

				// project from clip space to pixel
				const FLOAT InvW = 1.0f / SceneView->NearClippingDistance;
				const FVector2D Pixel(
					X + ClipPoint.X * InvW * HalfWidth,
					Y - ClipPoint.Y * InvW * HalfHeight );

				// get min,max screen bounds
				MinCorner.X = Min(Pixel.X, MinCorner.X);
				MinCorner.Y = Min(Pixel.Y, MinCorner.Y);
				MaxCorner.X = Max(Pixel.X, MaxCorner.X);
				MaxCorner.Y = Max(Pixel.Y, MaxCorner.Y);
			}
		}
	}
	else if( !GShowPartiallyVisible )
	{
		// fullscreen
		OutMin = MinView;
		OutMax = MaxView;
		return TRUE;	
	}

	OutMin.X = Clamp( MinCorner.X, MinView.X, MaxView.X );
	OutMin.Y = Clamp( MinCorner.Y, MinView.Y, MaxView.Y );

	OutMax.X = Clamp( MaxCorner.X, MinView.X, MaxView.X );
	OutMax.Y = Clamp( MaxCorner.Y, MinView.Y, MaxView.Y );

	return (OutMin.X < OutMax.X) && (OutMin.Y < OutMax.Y);
}

/*-----------------------------------------------------------------------------
	FDecalInteraction
-----------------------------------------------------------------------------*/

/** 
* Default Constructor 
*/
FDecalInteraction::FDecalInteraction()
:	Decal(NULL)
,	RenderData(NULL)
,	DecalStaticMesh(NULL)
{}

/** 
* Constructor 
* @param InDecal - decal component for this interaction
* @param InRenderData - render data generated for this interaction
*/
FDecalInteraction::FDecalInteraction(UDecalComponent* InDecal, FDecalRenderData* InRenderData)
:	Decal(InDecal)
,	RenderData(InRenderData)
,	DecalStaticMesh(NULL)
{}

/** 
* Copy Constructor  
*/
FDecalInteraction::FDecalInteraction(const FDecalInteraction& Copy)
:	Decal(NULL)
,	RenderData(NULL)
,	DecalStaticMesh(NULL)
{
	SafeCopy(Copy);
}

/**
* Assignment
*/
FDecalInteraction& FDecalInteraction::operator=(const FDecalInteraction& Other)
{
	SafeCopy(Other);
	return *this;
}

/**
* Destructor
*/
FDecalInteraction::~FDecalInteraction()
{
	delete DecalStaticMesh;
}

/**
* Copies all the decal members except and deletes the existing DecalStaticMesh
* @param Copy - other decal interaction to copy from
*/
void FDecalInteraction::SafeCopy(const FDecalInteraction& Copy)
{
	Decal = Copy.Decal;
	RenderData = Copy.RenderData;
	DecalState = Copy.DecalState;

	delete DecalStaticMesh;
	DecalStaticMesh = NULL;
}

/** 
* Generate the static mesh using the render proxy of the receiver the decal is attaching to 
* Updates DecalStaticMesh and also adds it to the list of DecalSTaticMeshes in FScene
* @param PrimitiveSceneInfo - primitive info for the receiving mesh
*/
void FDecalInteraction::CreateDecalStaticMesh(FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	check(IsInRenderingThread());

	/**
	* An implementation of FStaticPrimitiveDrawInterface that stores the drawn elements for the rendering thread to use.
	*/
	class FBatchingSPDI : public FStaticPrimitiveDrawInterface
	{
	public:

		// Constructor.
		FBatchingSPDI(FDecalInteraction* InDecalInteraction, FPrimitiveSceneInfo* InPrimitiveSceneInfo)
			:	DecalInteraction(InDecalInteraction)
			,	PrimitiveSceneInfo(InPrimitiveSceneInfo)
		  {}

		  // FStaticPrimitiveDrawInterface.
		  virtual void SetHitProxy(HHitProxy* HitProxy) 
		  {}

		  virtual void DrawMesh(
			  const FMeshBatch& Mesh,
			  FLOAT MinDrawDistance,
			  FLOAT MaxDrawDistance
			  )
		  {
			  if( Mesh.GetNumPrimitives() &&
				  DecalInteraction->DecalStaticMesh == NULL)
			  {
				  // lit translucent decals should go through un-batched mesh element rendering path for now
				  const FMaterial* Material = Mesh.MaterialRenderProxy != NULL ? Mesh.MaterialRenderProxy->GetMaterial() : NULL;
				  const UBOOL bIsLitTranslucent = Material != NULL && Material->GetLightingModel() != MLM_Unlit && Mesh.IsTranslucent();
				  if (!bIsLitTranslucent)
				  {
					  DecalInteraction->DecalStaticMesh = new FStaticMesh(
						  PrimitiveSceneInfo,
						  Mesh,
						  Square(Max(0.0f,MinDrawDistance)),
						  Square(Max(0.0f,MaxDrawDistance)),
						  FHitProxyId()
						  );
					  check(DecalInteraction->DecalStaticMesh->IsDecal());
					  // If the default material is being used on the decal, switch over to the default decal material
					  //@todo - figure out how this is happening and fix it at the source, since UDecalComponent::CaptureDecalState is supposed to enforce this
					  if (Mesh.MaterialRenderProxy == GEngine->DefaultMaterial->GetRenderProxy(0))
					  {
						  DecalInteraction->DecalStaticMesh->MaterialRenderProxy = GEngine->DefaultDecalMaterial->GetRenderProxy(0);
					  }
				  }
			  }

		  }

	private:
		FDecalInteraction* DecalInteraction;
		FPrimitiveSceneInfo* PrimitiveSceneInfo;
	};

	if( PrimitiveSceneInfo && 
		PrimitiveSceneInfo->Proxy )
	{
		delete DecalStaticMesh;
		DecalStaticMesh = NULL;
		// Cache the decal's static mesh elements.
		FBatchingSPDI BatchingSPDI(this,PrimitiveSceneInfo);
		PrimitiveSceneInfo->Proxy->DrawStaticDecalElements(&BatchingSPDI,*this);

		if( DecalStaticMesh )
		{
			// Add the static mesh to the scene's static mesh list.
			FScene* Scene = PrimitiveSceneInfo->Scene;
			check(Scene);
			FSparseArrayAllocationInfo SceneArrayAllocation = Scene->DecalStaticMeshes.Add();
			Scene->DecalStaticMeshes(SceneArrayAllocation.Index) = DecalStaticMesh;
			DecalStaticMesh->Id = SceneArrayAllocation.Index;

			// Add the static mesh to the appropriate draw lists.
			DecalStaticMesh->AddToDrawLists(Scene);
		}
	}
}
