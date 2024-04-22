/*=============================================================================
LandscapeMobileRender.cpp: Mobile rendering for Landscape
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnTerrain.h"
#include "LandscapeRender.h"
#include "LandscapeRenderMobile.h"
#include "LocalVertexFactoryShaderParms.h"

#define LANDSCAPE_LOD_DISTANCE_FACTOR 2.f

FLandscapeComponentSceneProxyMobile::FLandscapeComponentSceneProxyMobile(ULandscapeComponent* InComponent)
:	FPrimitiveSceneProxy(InComponent)
,	MaxLOD(appCeilLogTwo(InComponent->SubsectionSizeQuads+1)-1)
,	StaticLodBias(Min<INT>(InComponent->GetLandscapeProxy()->MobileLODBias, MaxLOD))
#if WITH_MOBILE_RHI
,	RuntimeLodBias(GSystemSettings.MobileLandscapeLodBias)
#else
,	RuntimeLodBias(0)
#endif
,	EffectiveLodBias(StaticLodBias+RuntimeLodBias)
,	ComponentSizeQuads(InComponent->ComponentSizeQuads)
,	NumSubsections(InComponent->NumSubsections)
,	SubsectionSizeQuads(InComponent->SubsectionSizeQuads)
,	SubsectionSizeVerts(InComponent->SubsectionSizeQuads+1)
,	SectionBaseX(InComponent->SectionBaseX)
,	SectionBaseY(InComponent->SectionBaseY)
,	StaticLightingResolution(InComponent->GetLandscapeProxy()->StaticLightingResolution)
,	VertexFactory(this)
,	ComponentLightInfo(new FLandscapeLCI(InComponent))
,	PlatformData(InComponent->PlatformData)
,	bNeedToDestroyPlatformData(FALSE)
{
	check( EffectiveLodBias <= MaxLOD );

	UMaterialInterface* MaterialInterface = InComponent->GetLandscapeProxy()->LandscapeMaterial;
	if( MaterialInterface == NULL )
	{
		MaterialInterface = GEngine->DefaultMaterial;
	}

#if !MOBILE && WITH_EDITOR
	// If we haven't cooked, we need to generate the mobile render data now.
	if( PlatformData == NULL )
	{
		INT PlatformDataSize;
		if( GIsGame )
		{
			// When running with the mobile previewer, make sure the usage is correct, and if not, use DefaultMateial
			UMaterial* ParentUMaterial = MaterialInterface->GetMaterial();
			if( !ParentUMaterial || !ParentUMaterial->bUsedWithMobileLandscape )
			{
				MaterialInterface = GEngine->DefaultMaterial;
			}		
		}
		InComponent->GeneratePlatformData( UE3::PLATFORM_Mobile, PlatformData, PlatformDataSize, WeightTexture );
		WeightTexture->UpdateResource();
		WeightTexture->AddToRoot();
		bNeedToDestroyPlatformData = TRUE;
	}
	else
#endif
	{
		WeightTexture = InComponent->WeightmapTextures(0);
	}
	
	MaterialViewRelevance = MaterialInterface->GetViewRelevance();
	MaterialRenderProxy = new FLandscapeMobileMaterialRenderProxy(MaterialInterface->GetRenderProxy(FALSE), this);

	check(PlatformData);

	// LOD
	LODDistance = appSqrt(2.f * Square((FLOAT)SubsectionSizeQuads)) * LANDSCAPE_LOD_DISTANCE_FACTOR;
	DistDiff = -appSqrt(2.f * Square(0.5f*(FLOAT)SubsectionSizeQuads));

	// Set Lightmap ScaleBias
	INT PatchExpandCountX = 1;
	INT PatchExpandCountY = 1;
	INT DesiredSize = 1;
	FLOAT LightMapRatio = ::GetTerrainExpandPatchCount(StaticLightingResolution, PatchExpandCountX, PatchExpandCountY, ComponentSizeQuads, (NumSubsections * (SubsectionSizeQuads+1)), DesiredSize);
	// Make sure they're the same
	FLOAT LightmapScaleX = LightMapRatio / (FLOAT)( ComponentSizeQuads + 2 * PatchExpandCountX + 1 );
	FLOAT LightmapScaleY = LightMapRatio / (FLOAT)( ComponentSizeQuads + 2 * PatchExpandCountY + 1 );
	FLOAT ExtendFactorX = (FLOAT)(ComponentSizeQuads) * LightmapScaleX;
	FLOAT ExtendFactorY = (FLOAT)(ComponentSizeQuads) * LightmapScaleY;

	LightmapScaleBias = FVector4(
		LightmapScaleX,
		LightmapScaleY,
		PatchExpandCountX * LightmapScaleY,
		PatchExpandCountX * LightmapScaleX);
}

FLandscapeComponentSceneProxyMobile::~FLandscapeComponentSceneProxyMobile()
{
	delete VertexBuffer;
	VertexBuffer = NULL;
	
	for( INT i=0;i<IndexBuffers.Num();i++ )
	{
		IndexBuffers(i)->Release();
	}
	IndexBuffers.Empty();

	VertexFactory.ReleaseResource();

	delete ComponentLightInfo;
	ComponentLightInfo = NULL;

#if !MOBILE	
	if( bNeedToDestroyPlatformData )
	{
		WeightTexture->RemoveFromRoot();	
	}
#endif

	delete MaterialRenderProxy;
	MaterialRenderProxy = NULL;
}

UBOOL FLandscapeComponentSceneProxyMobile::CreateRenderThreadResources()
{
	INT VertexBufferSize = 0;
	INT SkipVertexBufferSize = 0;
	INT StartVertexIndex = 0;
	INT Mip = 0;
	INT MipSubsectionSizeVerts = SubsectionSizeQuads+1;
	while( MipSubsectionSizeVerts > 1 )
	{
		INT MipComponentSizeQuads = (MipSubsectionSizeVerts-1) * NumSubsections;
		INT MipComponentSizeVerts = MipComponentSizeQuads + 1;
		
		// Only create index buffers for LODs we're going to use.
		if( Mip >= EffectiveLodBias )
		{
			INT NextMipComponentSizeQuads = ((MipSubsectionSizeVerts>>1)-1) * NumSubsections;
			FLOAT CurrentMipToBase	= (FLOAT)ComponentSizeQuads / (FLOAT)MipComponentSizeQuads;
			
			if( Mip < MaxLOD )
			{
				FLOAT NextMipToBase	= (FLOAT)ComponentSizeQuads / (FLOAT)NextMipComponentSizeQuads;
				new(BatchParameters) FLandscapeMobileBatchParams(this, Mip-EffectiveLodBias, FVector4(CurrentMipToBase,NextMipToBase,1.f,0.f));
			}
			else
			{
				new(BatchParameters) FLandscapeMobileBatchParams(this, Mip-EffectiveLodBias, FVector4(CurrentMipToBase,0.f,0.f,0.f));
			}

			FLandscapeIndexBufferMobile* IndexBuffer = FLandscapeIndexBufferMobile::GetLandscapeIndexBufferMobile(MipSubsectionSizeVerts-1, NumSubsections, StartVertexIndex);
			IndexBuffer->AddRef();
			IndexBuffers.AddItem(IndexBuffer);

			StartVertexIndex += Square(MipComponentSizeVerts);
			VertexBufferSize += Square(MipComponentSizeVerts) * sizeof(FLandscapeMobileVertex);
		}
#if !MOBILE
		else
		if( Mip >= StaticLodBias )
		{
			// We need to discard any PlatformData mips we don't need due to RuntimeLodBias.
			// Don't need to do this on MOBILE as the data is discarded in ULandscapeComponent::Serialize()
			SkipVertexBufferSize += Square(MipComponentSizeVerts) * sizeof(FLandscapeMobileVertex);
		}
#endif

		Mip++;
		MipSubsectionSizeVerts >>= 1;
	}

	// Copy platform data into vertex buffer
	VertexBuffer = new FLandscapeVertexBufferMobile(PlatformData, VertexBufferSize, SkipVertexBufferSize);

	FLocalVertexFactory::DataType Data;
	Data.PositionComponent = FVertexStreamComponent(
		VertexBuffer,
		STRUCT_OFFSET(FLandscapeMobileVertex,Position),
		sizeof(FLandscapeMobileVertex),
		VET_UByte4
		);
	Data.TangentBasisComponents[0] = FVertexStreamComponent(
		VertexBuffer,
		STRUCT_OFFSET(FLandscapeMobileVertex,NextMipPosition),
		sizeof(FLandscapeMobileVertex),
		VET_UByte4
		);
	Data.TangentBasisComponents[1] = FVertexStreamComponent(
		VertexBuffer,
		STRUCT_OFFSET(FLandscapeMobileVertex,Normal),
		sizeof(FLandscapeMobileVertex),
		VET_PackedNormal
		);

	VertexFactory.SetData(Data);
	VertexFactory.InitResource();

#if MOBILE
	ULandscapeComponent* Component = Cast<ULandscapeComponent>(PrimitiveSceneInfo->Component);
	Component->PlatformData = NULL;
	Component->PlatformDataSize = 0;
	{
#else
	if( bNeedToDestroyPlatformData )
	{
#endif
		appFree(PlatformData);
		PlatformData = NULL;
	}

	return TRUE;
}

void FLandscapeVertexFactoryMobile::GetStaticBatchElementVisibility( const class FSceneView& View, const struct FMeshBatch* Batch, TArray<INT>& BatchesToRender ) const
{
	SCOPE_CYCLE_COUNTER(STAT_LandscapeStaticDrawLODTime);
	SceneProxy->GetStaticBatchElementVisibility( View, Batch, BatchesToRender );
}

INT GForceLandscapeLOD = -1;

void FLandscapeComponentSceneProxyMobile::GetStaticBatchElementVisibility( const class FSceneView& View, const struct FMeshBatch* Batch, TArray<INT>& BatchesToRender ) const
{
	INC_DWORD_STAT(STAT_LandscapeComponents);

	FVector CameraLocalPos3D = WorldToLocal.TransformFVector(View.ViewOrigin); 
	FVector2D CameraLocalPos(CameraLocalPos3D.X, CameraLocalPos3D.Y);
	FVector2D ComponentPosition(0.5f * (FLOAT)SubsectionSizeQuads, 0.5f * (FLOAT)SubsectionSizeQuads);

	for( INT SubY=0;SubY<NumSubsections;SubY++)
	{
		for( INT SubX=0;SubX<NumSubsections;SubX++)
		{
			if( IndexBuffers.IsValidIndex(GForceLandscapeLOD) )
			{
				BatchesToRender.AddItem( GForceLandscapeLOD * Square(NumSubsections) + SubY*NumSubsections + SubX );
			}
			else
			{
				FVector2D LocalTranslate(SubX * SubsectionSizeQuads,SubY * SubsectionSizeQuads);
				FVector2D CurrentCameraLocalPos = CameraLocalPos - LocalTranslate;
				FLOAT ComponentDistance = FVector2D(CurrentCameraLocalPos-ComponentPosition).Size() + DistDiff;
				INT SubsectionLOD = Clamp<INT>( appFloor( ComponentDistance / LODDistance ), 0, MaxLOD - EffectiveLodBias );
				BatchesToRender.AddItem( SubsectionLOD * Square(NumSubsections) + SubY * NumSubsections + SubX );
			}
		}
	}		
}

void FLandscapeComponentSceneProxyMobile::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	FMeshBatch Mesh;
	Mesh.LCI = ComponentLightInfo; 
	Mesh.ReverseCulling = LocalToWorldDeterminant < 0.0f ? TRUE : FALSE;
	Mesh.CastShadow = TRUE;
	Mesh.Type = PT_TriangleList;
	Mesh.DepthPriorityGroup = SDPG_World;
	Mesh.MaterialRenderProxy = MaterialRenderProxy;
	Mesh.VertexFactory = &VertexFactory;
	Mesh.Elements.Empty();

	INT StartVertexIndex = 0;

	for( INT LodIndex=EffectiveLodBias;LodIndex<=MaxLOD;LodIndex++ )
	{
		INT MipSubsectionSizeVerts = (SubsectionSizeQuads+1) >> LodIndex;
		INT MipComponentSizeVerts = (MipSubsectionSizeVerts-1) * NumSubsections + 1;
		INT PrimitivesPerSubsection = Square(MipSubsectionSizeVerts-1) * 2;

		for( INT SubY=0;SubY<NumSubsections;SubY++)
		{
			for( INT SubX=0;SubX<NumSubsections;SubX++)
			{
				FMeshBatchElement* BatchElement = &Mesh.Elements(Mesh.Elements.AddZeroed());
				BatchElement->LocalToWorld = LocalToWorld;
				BatchElement->WorldToLocal = WorldToLocal;
				BatchElement->MinVertexIndex = StartVertexIndex;
				BatchElement->IndexBuffer = IndexBuffers(LodIndex-EffectiveLodBias);
				BatchElement->FirstIndex = (SubX + SubY * NumSubsections) * PrimitivesPerSubsection * 3;
				BatchElement->NumPrimitives = PrimitivesPerSubsection;
				BatchElement->MaxVertexIndex = StartVertexIndex + Square(MipComponentSizeVerts);
				BatchElement->ElementUserData = &BatchParameters(LodIndex-EffectiveLodBias);
			}
		}

		StartVertexIndex += Square(MipComponentSizeVerts);
	}

	PDI->DrawMesh(Mesh,0,FLT_MAX);
}


#if WITH_EDITOR
void FLandscapeComponentSceneProxyMobile::DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
{
	// Calculate which elements to render based on LOD
	TArray<INT> BatchesToRender;
	GetStaticBatchElementVisibility( *View, &PrimitiveSceneInfo->StaticMeshes(0), BatchesToRender );

	// Make local copy of static draw list
	FMeshBatch Mesh = PrimitiveSceneInfo->StaticMeshes(0);

	// Clear out existing elements and reserve enough space for those we'll render.
	Mesh.Elements.Empty(Mesh.Elements.Num() - BatchesToRender.Num());

	// Copy only the elements to render.
	for( INT Idx=0;Idx<BatchesToRender.Num();Idx++ )
	{
		Mesh.Elements.AddItem( PrimitiveSceneInfo->StaticMeshes(0).Elements(BatchesToRender(Idx)) );
	}

	DrawRichMesh(PDI, Mesh, FLinearColor::White, FLinearColor::White, FLinearColor::White, PrimitiveSceneInfo, IsSelected());
}
#endif

FPrimitiveViewRelevance FLandscapeComponentSceneProxyMobile::GetViewRelevance(const FSceneView* View)
{
	FPrimitiveViewRelevance Result;
	const EShowFlags ShowFlags = View->Family->ShowFlags;
	if((ShowFlags & SHOW_Terrain) && IsShown(View))
	{
		Result.SetDPG(GetDepthPriorityGroup(View),TRUE);
	
#if WITH_EDITOR
		if( IsRichView(View) )
		{
			Result.bDynamicRelevance = TRUE;
		}
		else
#endif
		{
			Result.bStaticRelevance = TRUE;
		}
		MaterialViewRelevance.SetPrimitiveViewRelevance(Result);
	}
	return Result;
}

void FLandscapeComponentSceneProxyMobile::GetLightRelevance(const FLightSceneInfo* LightSceneInfo, UBOOL& bDynamic, UBOOL& bRelevant, UBOOL& bLightMapped) const
{
	const ELightInteractionType InteractionType = ComponentLightInfo->GetInteraction(LightSceneInfo).GetType();

	// Attach the light to the primitive's static meshes.
	bDynamic = TRUE;
	bRelevant = FALSE;
	bLightMapped = TRUE;

	if (ComponentLightInfo)
	{
		ELightInteractionType InteractionType = ComponentLightInfo->GetInteraction(LightSceneInfo).GetType();
		if(InteractionType != LIT_CachedIrrelevant)
		{
			bRelevant = TRUE;
		}
		if(InteractionType != LIT_CachedLightMap && InteractionType != LIT_CachedIrrelevant)
		{
			bLightMapped = FALSE;
		}
		if(InteractionType != LIT_Uncached)
		{
			bDynamic = FALSE;
		}
	}
	else
	{
		bRelevant = TRUE;
		bLightMapped = FALSE;
	}
}

void FLandscapeComponentSceneProxyMobile::OnTransformChanged()
{
	WorldToLocal = LocalToWorld.Inverse();
}

/** 
* Initialize the RHI for this rendering resource 
*/
void FLandscapeVertexBufferMobile::InitRHI()
{
	// create a static vertex buffer
	VertexBufferRHI = RHICreateVertexBuffer(DataSize, NULL, RUF_Static);
	void* VertexData = RHILockVertexBuffer(VertexBufferRHI, 0, DataSize,FALSE);
	// Copy stored platform data
	appMemcpy(VertexData, (BYTE*)Data + SkipDataSize, DataSize);
	RHIUnlockVertexBuffer(VertexBufferRHI);
}

/* Map storing shared index buffers of the appropriate size */
TMap<QWORD, FLandscapeIndexBufferMobile*> FLandscapeIndexBufferMobile::SharedIndexBufferMap;

QWORD FLandscapeIndexBufferMobile::GetKey(INT SubsectionSizeQuads, INT NumSubsections, INT StartVertexIndex)
{
	return ((QWORD)StartVertexIndex << 32) | ((QWORD)SubsectionSizeQuads << 16) | (QWORD)NumSubsections;
}

FLandscapeIndexBufferMobile* FLandscapeIndexBufferMobile::GetLandscapeIndexBufferMobile(INT SubsectionSizeQuads, INT NumSubsections, INT StartVertexIndex)
{
	QWORD Key = GetKey(SubsectionSizeQuads, NumSubsections, StartVertexIndex);
	FLandscapeIndexBufferMobile* IndexBuffer = SharedIndexBufferMap.FindRef(Key);
	if( IndexBuffer == NULL )
	{
		IndexBuffer = new FLandscapeIndexBufferMobile(Key, SubsectionSizeQuads, NumSubsections, StartVertexIndex);
	}
	return IndexBuffer;
}

FLandscapeIndexBufferMobile::FLandscapeIndexBufferMobile(QWORD Key, INT SubsectionSizeQuads, INT NumSubsections, INT StartVertexIndex)
:	CachedKey(Key)
{
	INT VBSizeVertices = SubsectionSizeQuads * NumSubsections + 1;

	TArray<WORD> NewIndices;
	NewIndices.Empty(Square(SubsectionSizeQuads * NumSubsections) * 6);

	for( INT SubY=0;SubY<NumSubsections;SubY++)
	{
		for( INT SubX=0;SubX<NumSubsections;SubX++)
		{
			INT BaseY = SubY * SubsectionSizeQuads;
			for( INT y=0;y<SubsectionSizeQuads;y++ )
			{
				INT BaseX = SubX * SubsectionSizeQuads;
				for( INT x=0;x<SubsectionSizeQuads;x++ )
				{
					NewIndices.AddItem( (BaseX+x+0) + (BaseY+y+0) * VBSizeVertices + StartVertexIndex );
					NewIndices.AddItem( (BaseX+x+1) + (BaseY+y+1) * VBSizeVertices + StartVertexIndex );
					NewIndices.AddItem( (BaseX+x+1) + (BaseY+y+0) * VBSizeVertices + StartVertexIndex );
					NewIndices.AddItem( (BaseX+x+0) + (BaseY+y+0) * VBSizeVertices + StartVertexIndex );
					NewIndices.AddItem( (BaseX+x+0) + (BaseY+y+1) * VBSizeVertices + StartVertexIndex );
					NewIndices.AddItem( (BaseX+x+1) + (BaseY+y+1) * VBSizeVertices + StartVertexIndex );
				}
			}
		}
	}
	Indices = NewIndices;

	InitResource();

	// Add to shared map
	SharedIndexBufferMap.Set(CachedKey, this);
}

FLandscapeIndexBufferMobile::~FLandscapeIndexBufferMobile()
{
	ReleaseResource();
	SharedIndexBufferMap.Remove(CachedKey);
}


/** Shader parameters for use with FLandscapeVertexFactoryMobile */
class FLandscapeVertexFactoryMobileVertexShaderParameters : public FLocalVertexFactoryShaderParameters
{
public:
	/**
	* Bind shader constants by name
	* @param	ParameterMap - mapping of named shader constants to indices
	*/
	virtual void Bind(const FShaderParameterMap& ParameterMap)
	{
		FLocalVertexFactoryShaderParameters::Bind(ParameterMap);
		LightmapScaleBiasParameter.Bind(ParameterMap,TEXT("LightmapScaleBias"),TRUE);
		LayerUVScaleBiasParameter.Bind(ParameterMap,TEXT("LayerUVScaleBias"),TRUE);
		LodValuesParameter.Bind(ParameterMap,TEXT("LodValues"),TRUE);
		LodDistancesParameter.Bind(ParameterMap,TEXT("LodDistancesValues"),TRUE);	
	}

	/**
	* Serialize shader params to an archive
	* @param	Ar - archive to serialize to
	*/
	virtual void Serialize(FArchive& Ar)
	{
		FLocalVertexFactoryShaderParameters::Serialize(Ar);
		Ar << LightmapScaleBiasParameter;
		Ar << LayerUVScaleBiasParameter;
		Ar << LodValuesParameter;
		Ar << LodDistancesParameter;

		// set parameter names for mobile
		LightmapScaleBiasParameter.SetShaderParamName(TEXT("LightmapScaleBias"));
		LayerUVScaleBiasParameter.SetShaderParamName(TEXT("LayerUVScaleBias"));
		LodValuesParameter.SetShaderParamName(TEXT("LodValues"));
		LodDistancesParameter.SetShaderParamName(TEXT("LodDistancesValues"));
	}

	/**
	* Set any shader data specific to this vertex factory
	*/
	virtual void SetMesh(FShader* VertexShader, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View) const
	{
		FLocalVertexFactoryShaderParameters::SetMesh(VertexShader,Mesh,BatchElementIndex,View);

		SCOPE_CYCLE_COUNTER(STAT_LandscapeVFDrawTime);
		INC_DWORD_STAT(STAT_LandscapeDrawCalls);
		INC_DWORD_STAT_BY(STAT_LandscapeTriangles,Mesh.Elements(BatchElementIndex).NumPrimitives);

		const FLandscapeMobileBatchParams* BatchParams = (const FLandscapeMobileBatchParams*)Mesh.Elements(BatchElementIndex).ElementUserData;
		const FLandscapeComponentSceneProxyMobile* SceneProxy = BatchParams->SceneProxy;

		if( LightmapScaleBiasParameter.IsBound() )
		{
			SetVertexShaderValue(VertexShader->GetVertexShader(),LightmapScaleBiasParameter,SceneProxy->LightmapScaleBias);
		}

		if( LayerUVScaleBiasParameter.IsBound() )
		{
			FVector4 LayerUVScaleBias(SceneProxy->SectionBaseX, SceneProxy->SectionBaseY, 1.f/(FLOAT)SceneProxy->ComponentSizeQuads, 1.f/(FLOAT)SceneProxy->ComponentSizeQuads);
			SetVertexShaderValue(VertexShader->GetVertexShader(),LayerUVScaleBiasParameter,LayerUVScaleBias);
		}

		if( LodValuesParameter.IsBound() )
		{
			SetVertexShaderValue(VertexShader->GetVertexShader(), LodValuesParameter, BatchParams->LodParameters);
		}

		if( LodDistancesParameter.IsBound() )
		{
			FVector CameraLocalPos3D = SceneProxy->WorldToLocal.TransformFVector(View.ViewOrigin);
			FVector2D CameraLocalPos(CameraLocalPos3D.X, CameraLocalPos3D.Y);

			FLOAT DistParam1 = ((FLOAT)BatchParams->BiasedLodIndex+0.5f) * SceneProxy->LODDistance;
			FLOAT DistParam2 = ((FLOAT)BatchParams->BiasedLodIndex+1.f) * SceneProxy->LODDistance;

			FVector4 LodDistancesValues(
				CameraLocalPos.X, 
				CameraLocalPos.Y,
				DistParam1,
				1.f / (DistParam2 - DistParam1));
			SetVertexShaderValue(VertexShader->GetVertexShader(), LodDistancesParameter, LodDistancesValues);
		}
	}

private:
	FShaderParameter LightmapScaleBiasParameter;
	FShaderParameter LayerUVScaleBiasParameter;
	FShaderParameter LodValuesParameter;
	FShaderParameter LodDistancesParameter;
};

FVertexFactoryShaderParameters* FLandscapeVertexFactoryMobile::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FLandscapeVertexFactoryMobileVertexShaderParameters() : NULL;
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FLandscapeVertexFactoryMobile,"LandscapeVertexFactoryMobilePreview",TRUE,TRUE,TRUE,TRUE,TRUE, VER_VERTEX_FACTORY_LOCALTOWORLD_FLIP,0);

UBOOL FLandscapeMobileMaterialRenderProxy::GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const
{
	static FName MobileMaskTexture(TEXT("MobileMaskTexture"));
	if( ParameterName == MobileMaskTexture )
	{
		*OutValue = SceneProxy->WeightTexture->Resource;
		return TRUE;
	}
	return Parent->GetTextureValue(ParameterName, OutValue, Context);
}

#if WITH_MOBILE_RHI

FTexture* FLandscapeMobileMaterialRenderProxy::GetMobileTexture(const INT MobileTextureUnit) const
{ 
	switch( MobileTextureUnit )
	{
	case LandscapeLayer0_MobileTexture:
		return Parent->GetMobileTexture(Base_MobileTexture); 
	case LandscapeLayer1_MobileTexture:
		return Parent->GetMobileTexture(Detail_MobileTexture); 
	case LandscapeLayer2_MobileTexture:
		return Parent->GetMobileTexture(Detail_MobileTexture2); 
	case LandscapeLayer3_MobileTexture:
		return Parent->GetMobileTexture(Detail_MobileTexture3); 
	case Mask_MobileTexture:
		return SceneProxy->WeightTexture->Resource;
	case Lightmap_MobileTexture:
		return Parent->GetMobileTexture(Lightmap_MobileTexture); 
	case Normal_MobileTexture:
		return Parent->GetMobileTexture(Normal_MobileTexture); 
	}
	return NULL;
}

#endif
