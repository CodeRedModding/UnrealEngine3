/*=============================================================================
LandscapeRender.cpp: New terrain rendering
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineDecalClasses.h"
#include "UnTerrain.h"
#include "LandscapeRender.h"
#include "LandscapeEdit.h"
#include "ScenePrivate.h"
#include "LevelUtils.h"

#define LANDSCAPE_LOD_DISTANCE_FACTOR 2.f
#define LANDSCAPE_MAX_COMPONENT_SIZE 255

/**
	This debug variable is toggled by the 'landscapebatching' console command.
*/
UBOOL GUseLandscapeBatching = TRUE;
/**
	This debug variable is toggled by the 'landscapestaticdraw' console command.
*/
UBOOL GUseLandscapeStatic = TRUE;

#if WITH_EDITOR
ELandscapeViewMode::Type GLandscapeViewMode = ELandscapeViewMode::Normal;
INT GLandscapeEditRenderMode = ELandscapeEditRenderMode::None;
INT GLandscapePreviewMeshRenderMode = 0;
UMaterial* GLayerDebugColorMaterial = NULL;
UMaterialInstanceConstant* GSelectionColorMaterial = NULL;
UMaterialInstanceConstant* GSelectionRegionMaterial = NULL;
UMaterialInstanceConstant* GMaskRegionMaterial = NULL;

void FLandscapeEditToolRenderData::UpdateDebugColorMaterial()
{
	if (!LandscapeComponent)
	{
		return;
	}

	// Debug Color Rendering Material....
	DebugChannelR = INDEX_NONE, DebugChannelG = INDEX_NONE, DebugChannelB = INDEX_NONE;
	LandscapeComponent->GetLayerDebugColorKey(DebugChannelR, DebugChannelG, DebugChannelB);

	if (!GLayerDebugColorMaterial)
	{
		GLayerDebugColorMaterial = LoadObject<UMaterial>(NULL, TEXT("EditorLandscapeResources.LayerVisMaterial"), NULL, LOAD_None, NULL);
	}
}

void FLandscapeEditToolRenderData::UpdateSelectionMaterial(INT InSelectedType)
{
	if (!LandscapeComponent)
	{
		return;
	}

	if (!GSelectionColorMaterial)
	{
		GSelectionColorMaterial = LoadObject<UMaterialInstanceConstant>(NULL, TEXT("EditorLandscapeResources.SelectBrushMaterial_Selected"), NULL, LOAD_None, NULL);
	}

	if (!GSelectionRegionMaterial)
	{
		GSelectionRegionMaterial = LoadObject<UMaterialInstanceConstant>(NULL, TEXT("EditorLandscapeResources.SelectBrushMaterial_SelectedRegion"), NULL, LOAD_None, NULL);
	}

	// Check selection
	if (SelectedType != InSelectedType && (SelectedType & ST_REGION) && !(InSelectedType & ST_REGION) )
	{
		// Clear Select textures...
		if (DataTexture)
		{
			FLandscapeEditDataInterface LandscapeEdit(LandscapeComponent->GetLandscapeInfo());
			LandscapeEdit.ZeroTexture(DataTexture);
		}
	}
	SelectedType = InSelectedType;
}

#endif

//
//	FLandscapeDecalInteraction
//
/** An association between a decal and a terrain component. */
class FLandscapeDecalInteraction : public FReceiverResource
{
public:
	FLandscapeDecalInteraction()
		: DecalComponent( NULL )
		, DecalIndexBuffers(NULL)
	{
	}
	FLandscapeDecalInteraction(const UDecalComponent* InDecalComponent, const ULandscapeComponent* LandscapeComponent);
	virtual ~FLandscapeDecalInteraction()
	{
		if (DecalIndexBuffers)
		{
			delete DecalIndexBuffers;
		}
	}

	void InitResources_RenderingThread();
	
	// FReceiverResource Interface
	virtual void OnRelease_RenderingThread()
	{
		if ( DecalIndexBuffers )
		{
			delete DecalIndexBuffers;
			DecalIndexBuffers = NULL;
		}
	}
	/** 
	* @return memory usage in bytes for the receiver resource data for a given decal
	*/
	virtual INT GetMemoryUsage()
	{
		INT MemoryCount=0;
		if( DecalIndexBuffers )
		{
			MemoryCount += DecalIndexBuffers->GetTotalIndexNum() * sizeof(WORD);
		}
		return MemoryCount;
	}

	FORCEINLINE FLandscapeDecalIndexBuffers* GetDecalIndexBuffers()
	{
		return DecalIndexBuffers;
	}

private:
	/** The decal component associated with this interaction. */
	const UDecalComponent* DecalComponent;
	/** The index buffer associated with this decal. */
	FLandscapeDecalIndexBuffers* DecalIndexBuffers;
	/** Min/Max patch indices spanned by this decal. */
	INT MinPatchX[LANDSCAPE_MAX_SUBSECTION_NUM*LANDSCAPE_MAX_SUBSECTION_NUM];
	INT MinPatchY[LANDSCAPE_MAX_SUBSECTION_NUM*LANDSCAPE_MAX_SUBSECTION_NUM];
	INT MaxPatchX[LANDSCAPE_MAX_SUBSECTION_NUM*LANDSCAPE_MAX_SUBSECTION_NUM];
	INT MaxPatchY[LANDSCAPE_MAX_SUBSECTION_NUM*LANDSCAPE_MAX_SUBSECTION_NUM];
	INT SubSectionSize;
	INT NumSubsections;
};

FLandscapeDecalInteraction::FLandscapeDecalInteraction(const UDecalComponent* InDecalComponent,
												   const ULandscapeComponent* LandscapeComponent)
												   : DecalComponent( InDecalComponent ),
												   DecalIndexBuffers(NULL)
{
	const FMatrix& LandscapeWorldToLocal = LandscapeComponent->GetLandscapeProxy()->WorldToLocal();

	// Transform the decal frustum verts into local space and compute mins/maxs.
	FVector FrustumVerts[8];
	DecalComponent->GenerateDecalFrustumVerts( FrustumVerts );

	// Compute mins/maxs of local space frustum verts.
	FrustumVerts[0] = LandscapeWorldToLocal.TransformFVector( FrustumVerts[0] );
	FLOAT MinX = FrustumVerts[0].X;
	FLOAT MinY = FrustumVerts[0].Y;
	FLOAT MaxX = FrustumVerts[0].X;
	FLOAT MaxY = FrustumVerts[0].Y;
	FLOAT MinZ = FrustumVerts[0].Z;
	FLOAT MaxZ = FrustumVerts[0].Z;
	for ( INT Index = 1 ; Index < 8 ; ++Index )
	{
		FrustumVerts[Index] = LandscapeWorldToLocal.TransformFVector( FrustumVerts[Index] );
		MinX = Min( MinX, FrustumVerts[Index].X );
		MinY = Min( MinY, FrustumVerts[Index].Y );
		MinZ = Min( MinZ, FrustumVerts[Index].Z );
		MaxX = Max( MaxX, FrustumVerts[Index].X );
		MaxY = Max( MaxY, FrustumVerts[Index].Y );
		MaxZ = Max( MaxZ, FrustumVerts[Index].Z );
	}

	SubSectionSize = LandscapeComponent->SubsectionSizeQuads+1;
	NumSubsections = LandscapeComponent->NumSubsections;

	// Compute min/max patch indices for each subsections
	// TODO: Vector Field consideration....
	// Currently only Bounding box check
	for (INT SubY = 0; SubY < NumSubsections; ++SubY)
	{
		for (INT SubX = 0; SubX < NumSubsections; ++SubX)
		{
			MinPatchX[SubY*NumSubsections + SubX] = Clamp<INT>(appFloor(MinX) - (LandscapeComponent->SectionBaseX+SubX*LandscapeComponent->SubsectionSizeQuads), 0, LandscapeComponent->SubsectionSizeQuads );
			MinPatchY[SubY*NumSubsections + SubX] = Clamp<INT>(appFloor(MinY) - (LandscapeComponent->SectionBaseY+SubY*LandscapeComponent->SubsectionSizeQuads), 0, LandscapeComponent->SubsectionSizeQuads );
			MaxPatchX[SubY*NumSubsections + SubX] = Clamp<INT>(appCeil(MaxX) - (LandscapeComponent->SectionBaseX+SubX*LandscapeComponent->SubsectionSizeQuads), 0, LandscapeComponent->SubsectionSizeQuads );
			MaxPatchY[SubY*NumSubsections + SubX] = Clamp<INT>(appCeil(MaxY) - (LandscapeComponent->SectionBaseY+SubY*LandscapeComponent->SubsectionSizeQuads), 0, LandscapeComponent->SubsectionSizeQuads );
		}
	}
}

void FLandscapeDecalInteraction::InitResources_RenderingThread()
{
	// Create the tessellation index buffers
	DecalIndexBuffers = new FLandscapeDecalIndexBuffers(MinPatchX, MinPatchY, MaxPatchX, MaxPatchY, NumSubsections, SubSectionSize);
	DecalIndexBuffers->InitResources();

	// Mark that the receiver resource is initialized.
	SetInitialized();
}

//
// FLandscapeComponentSceneProxy
//
FLandscapeVertexBuffer* FLandscapeComponentSceneProxy::SharedVertexBuffer = NULL;
FLandscapeIndexBuffer** FLandscapeComponentSceneProxy::SharedIndexBuffers = NULL;
FLandscapeVertexFactory* FLandscapeComponentSceneProxy::SharedVertexFactory = NULL;

#if WITH_EDITOR
void ULandscapeComponent::Attach()
{
	Super::Attach();
	// update all decals when attaching terrain
	if( GIsEditor && !GIsPlayInEditorWorld )
	{
		if( bAcceptsDynamicDecals || bAcceptsStaticDecals )
		{
			GEngine->IssueDecalUpdateRequest();
		}
	}
}
#endif

// Decal stuffs
void ULandscapeComponent::GenerateDecalRenderData(FDecalState* Decal, TArray< FDecalRenderData* >& OutDecalRenderDatas) const
{
	SCOPE_CYCLE_COUNTER(STAT_DecalTerrainAttachTime);

	OutDecalRenderDatas.Reset();

	// Do nothing if the specified decal doesn't project on static meshes.
	if ( !Decal->bProjectOnTerrain )
	{
		return;
	}

	// scissor rect based on frustum assumes local space verts (see FMeshDrawingPolicy::SetMeshRenderState)
	FMatrix WorldToLocal = LocalToWorld.Inverse();
	Decal->TransformFrustumVerts( WorldToLocal );
	// no clipping occurs for terrain. Rely on screen space scissor rect culling instead
	Decal->bUseSoftwareClip = FALSE;

	if (!FLandscapeComponentSceneProxy::SharedVertexFactory)
	{
		// To wait for SharedVertexFactory initialize in Rendering Thread
		FlushRenderingCommands();
	}

	// create the new decal render data using the vertex factory from the terrain object
	FDecalRenderData* DecalRenderData = new FDecalRenderData( NULL, FALSE, FALSE, FLandscapeComponentSceneProxy::SharedVertexFactory );

	// always need at least one triangle
	if ( DecalRenderData )
	{
		DecalRenderData->NumTriangles = 1;
		DecalRenderData->DecalBlendRange = Decal->DecalComponent->CalcDecalDotProductBlendRange();

		OutDecalRenderDatas.AddItem( DecalRenderData );
	}
}

IMPLEMENT_COMPARE_CONSTPOINTER( FDecalInteraction, LandscapeRender,
{
	return (A->DecalState.SortOrder <= B->DecalState.SortOrder) ? -1 : 1;
} );

FLandscapeComponentSceneProxy::FLandscapeComponentSceneProxy(ULandscapeComponent* InComponent, FLandscapeEditToolRenderData* InEditToolRenderData)
:	FPrimitiveSceneProxy(InComponent)
,	MaxLOD(appCeilLogTwo(InComponent->SubsectionSizeQuads+1)-1)
,	ComponentSizeQuads(InComponent->ComponentSizeQuads)
,	NumSubsections(InComponent->NumSubsections)
,	SubsectionSizeQuads(InComponent->SubsectionSizeQuads)
,	SubsectionSizeVerts(InComponent->SubsectionSizeQuads+1)
,	SectionBaseX(InComponent->SectionBaseX)
,	SectionBaseY(InComponent->SectionBaseY)
,	DrawScaleXY(InComponent->GetLandscapeProxy()->DrawScale * InComponent->GetLandscapeProxy()->DrawScale3D.X)
,	ActorOrigin(InComponent->GetLandscapeProxy()->Location)
,	StaticLightingResolution(InComponent->GetLandscapeProxy()->StaticLightingResolution)
,	WeightmapScaleBias(InComponent->WeightmapScaleBias)
,	WeightmapSubsectionOffset(InComponent->WeightmapSubsectionOffset)
,	HeightmapTexture(InComponent->HeightmapTexture)
,	HeightmapScaleBias(InComponent->HeightmapScaleBias)
,	HeightmapSubsectionOffsetU((FLOAT)(InComponent->SubsectionSizeQuads+1) / (FLOAT)InComponent->HeightmapTexture->SizeX)
,	HeightmapSubsectionOffsetV((FLOAT)(InComponent->SubsectionSizeQuads+1) / (FLOAT)InComponent->HeightmapTexture->SizeY)
,	VertexFactory(NULL)
,	VertexBuffer(NULL)
,	IndexBuffers(NULL)
,	MaterialInterface(InComponent->MaterialInstance)
,	EditToolRenderData(InEditToolRenderData)
,	ComponentLightInfo(NULL)
,	LevelColor(1.f, 1.f, 1.f)
#if PS3
,	PlatformData(InComponent->PlatformData)
#else
,	ForcedLOD(InComponent->ForcedLOD)
,	LODBias(InComponent->LODBias)
#endif
{
	// 0 - 1 - 2        - - 0 - -
	// |       |        |       |
	// 3   P   4   or   1   P   2
	// |       |        |       |
	// 5 - 6 - 7        - - 3 - -

#if !PS3
	NeighborPosition[0].Set(0.5f * (FLOAT)SubsectionSizeQuads, -0.5f * (FLOAT)SubsectionSizeQuads);
	NeighborPosition[1].Set(-0.5f * (FLOAT)SubsectionSizeQuads, 0.5f * (FLOAT)SubsectionSizeQuads);
	NeighborPosition[2].Set(1.5f * (FLOAT)SubsectionSizeQuads, 0.5f * (FLOAT)SubsectionSizeQuads);
	NeighborPosition[3].Set(0.5f * (FLOAT)SubsectionSizeQuads, 1.5f * (FLOAT)SubsectionSizeQuads);

	ForcedNeighborLOD[0] = InComponent->NeighborLOD[1];
	ForcedNeighborLOD[1] = InComponent->NeighborLOD[3];
	ForcedNeighborLOD[2] = InComponent->NeighborLOD[4];
	ForcedNeighborLOD[3] = InComponent->NeighborLOD[6];

	NeighborLODBias[0] = InComponent->NeighborLODBias[1];
	NeighborLODBias[1] = InComponent->NeighborLODBias[3];
	NeighborLODBias[2] = InComponent->NeighborLODBias[4];
	NeighborLODBias[3] = InComponent->NeighborLODBias[6];
#endif // !PS3

	if( InComponent->GetLandscapeProxy()->MaxLODLevel >= 0 )
	{
		MaxLOD = Min<INT>(MaxLOD, InComponent->GetLandscapeProxy()->MaxLODLevel);
	}

	LODDistance = appSqrt(2.f * Square((FLOAT)SubsectionSizeQuads)) * LANDSCAPE_LOD_DISTANCE_FACTOR / InComponent->GetLandscapeProxy()->LODDistanceFactor;
	DistDiff = -appSqrt(2.f * Square(0.5f*(FLOAT)SubsectionSizeQuads));
	LODDistanceFactor = InComponent->GetLandscapeProxy()->LODDistanceFactor * 0.33f;

	// Set Lightmap ScaleBias
	INT PatchExpandCountX = 1;
	INT PatchExpandCountY = 1;
	INT DesiredSize = 1;
	FLOAT LightMapRatio = ::GetTerrainExpandPatchCount(StaticLightingResolution, PatchExpandCountX, PatchExpandCountY, ComponentSizeQuads, (NumSubsections * (SubsectionSizeQuads+1)), DesiredSize);
	// Make sure they're the same
	PatchExpandCount = PatchExpandCountX;
	FLOAT LightmapScaleX = LightMapRatio / (FLOAT)( ComponentSizeQuads + 2 * PatchExpandCountX + 1 );
	FLOAT LightmapScaleY = LightMapRatio / (FLOAT)( ComponentSizeQuads + 2 * PatchExpandCountY + 1 );
	FLOAT ExtendFactorX = (FLOAT)(ComponentSizeQuads) * LightmapScaleX;
	FLOAT ExtendFactorY = (FLOAT)(ComponentSizeQuads) * LightmapScaleY;

	// Caching static parameters...
	SubsectionParams.Empty(Square(NumSubsections));

	for( INT SubY=0;SubY<NumSubsections;SubY++ )
	{
		for( INT SubX=0;SubX<NumSubsections;SubX++ )
		{
			new (SubsectionParams) FLandscapeSubsectionParams;
			FMatrix SubsectionLocalToWorld = FTranslationMatrix(FVector(SubX * SubsectionSizeQuads,SubY * SubsectionSizeQuads,0)) * InComponent->LocalToWorld;
			FMatrix SubsectionWorldToLocal = SubsectionLocalToWorld.Inverse();
			FLandscapeSubsectionParams& SubsectionParam = SubsectionParams(NumSubsections * SubY + SubX);
			SubsectionParam.LocalToWorld = SubsectionLocalToWorld;
			SubsectionParam.WorldToLocal = SubsectionWorldToLocal;

			SubsectionParam.WorldToLocalNoScaling = SubsectionWorldToLocal;
			SubsectionParam.WorldToLocalNoScaling.RemoveScaling();
			SubsectionParam.HeightmapUVScaleBias = HeightmapScaleBias;
			SubsectionParam.HeightmapUVScaleBias.Z += HeightmapSubsectionOffsetU * (FLOAT)SubX;
			SubsectionParam.HeightmapUVScaleBias.W += HeightmapSubsectionOffsetV * (FLOAT)SubY; 
			SubsectionParam.WeightmapUVScaleBias = WeightmapScaleBias;
			SubsectionParam.WeightmapUVScaleBias.Z += WeightmapSubsectionOffset * (FLOAT)SubX;
			SubsectionParam.WeightmapUVScaleBias.W += WeightmapSubsectionOffset * (FLOAT)SubY; 

			SubsectionParam.LandscapeLightmapScaleBias = FVector4(
				LightmapScaleX,
				LightmapScaleY,
				(FLOAT)(SubY) / (FLOAT)(NumSubsections) * ExtendFactorY + PatchExpandCount * LightmapScaleY,
				(FLOAT)(SubX) / (FLOAT)(NumSubsections) * ExtendFactorX + PatchExpandCount * LightmapScaleX);
			SubsectionParam.SubsectionSizeVertsLayerUVPan = FVector4(
				SubsectionSizeVerts,
				1.f / (FLOAT)SubsectionSizeQuads,
				SectionBaseX + (FLOAT)(SubX * SubsectionSizeQuads),
				SectionBaseY + (FLOAT)(SubY * SubsectionSizeQuads)
				);

			SubsectionParam.LocalToWorldNoScaling = SubsectionLocalToWorld;
			SubsectionParam.LocalToWorldNoScaling.RemoveScaling();
		}
	}

	ComponentLightInfo = new FLandscapeLCI(InComponent);
	check(ComponentLightInfo);

	// Check material usage
	if( MaterialInterface == NULL || !MaterialInterface->CheckMaterialUsage(MATUSAGE_Landscape) )
	{
		MaterialInterface = GEngine->DefaultMaterial;
	}

	MaterialViewRelevance = MaterialInterface->GetViewRelevance();

#if !FINAL_RELEASE
	if( GIsEditor )
	{
		ALandscapeProxy* Proxy = InComponent->GetLandscapeProxy();
		// Try to find a color for level coloration.
		if ( Proxy )
		{
			ULevel* Level = Proxy->GetLevel();
			ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel( Level );
			if ( LevelStreaming )
			{
				LevelColor = LevelStreaming->DrawColor;
			}
		}
	}
#endif
}

void FLandscapeComponentSceneProxy::OnTransformChanged()
{
	for( INT SubY=0;SubY<NumSubsections;SubY++ )
	{
		for( INT SubX=0;SubX<NumSubsections;SubX++ )
		{
			FMatrix SubsectionLocalToWorld = FTranslationMatrix(FVector(SubX * SubsectionSizeQuads,SubY * SubsectionSizeQuads,0)) * LocalToWorld;
			FMatrix SubsectionWorldToLocal = SubsectionLocalToWorld.Inverse();
			FLandscapeSubsectionParams& SubsectionParam = SubsectionParams(NumSubsections * SubY + SubX);
			SubsectionParam.LocalToWorld = SubsectionLocalToWorld;
			SubsectionParam.WorldToLocal = SubsectionWorldToLocal;

			SubsectionParam.WorldToLocalNoScaling = SubsectionWorldToLocal;
			SubsectionParam.WorldToLocalNoScaling.RemoveScaling();
			SubsectionParam.LocalToWorldNoScaling = SubsectionLocalToWorld;
			SubsectionParam.LocalToWorldNoScaling.RemoveScaling();
		}
	}

}

FLandscapeComponentSceneProxy::~FLandscapeComponentSceneProxy()
{
	if( VertexFactory )
	{
		check( SharedVertexFactory == VertexFactory );
		if (SharedVertexFactory->Release() == 0)
		{
			SharedVertexFactory = NULL;
		}
		VertexFactory = NULL;
	}

	if( VertexBuffer )
	{
		check( SharedVertexBuffer == VertexBuffer );
		if( SharedVertexBuffer->Release() == 0 )
		{
			SharedVertexBuffer = NULL;
		}
		VertexBuffer = NULL;
	}

	if( IndexBuffers )
	{
		check( SharedIndexBuffers == IndexBuffers );
		UBOOL bCanDeleteArray = TRUE;
		for( INT i=0;i<LANDSCAPE_LOD_LEVELS;i++ )
		{
			if( SharedIndexBuffers[i]->Release() == 0 )
			{
				SharedIndexBuffers[i] = NULL;
			}
			else
			{
				bCanDeleteArray = FALSE;
			}
		}
		if( bCanDeleteArray )
		{
			delete[] SharedIndexBuffers;
			SharedIndexBuffers = NULL;
		}
		IndexBuffers = NULL;
	}

	delete ComponentLightInfo;
	ComponentLightInfo = NULL;
}

UBOOL FLandscapeComponentSceneProxy::CreateRenderThreadResources()
{
	check(HeightmapTexture != NULL);

	if( SharedVertexBuffer == NULL )
	{
		SharedVertexBuffer = new FLandscapeVertexBuffer(LANDSCAPE_MAX_COMPONENT_SIZE+1);
	}

	if( SharedIndexBuffers == NULL )
	{
		SharedIndexBuffers = new FLandscapeIndexBuffer*[LANDSCAPE_LOD_LEVELS];
		for( INT i=0;i<LANDSCAPE_LOD_LEVELS;i++ )
		{
			SharedIndexBuffers[i] = new FLandscapeIndexBuffer(((LANDSCAPE_MAX_COMPONENT_SIZE+1) >> i)-1, LANDSCAPE_MAX_COMPONENT_SIZE+1);
		}
	}
	for( INT i=0;i<LANDSCAPE_LOD_LEVELS;i++ )
	{
		SharedIndexBuffers[i]->AddRef();
	}
	IndexBuffers = SharedIndexBuffers;

	SharedVertexBuffer->AddRef();
	VertexBuffer = SharedVertexBuffer;

	if (SharedVertexFactory == NULL)
	{
		SharedVertexFactory = new FLandscapeVertexFactory();
		SharedVertexFactory->Data.PositionComponent = FVertexStreamComponent(VertexBuffer, 0, sizeof(FLandscapeVertexBuffer::FLandscapeVertex), VET_Float2);
#if PS3
		SharedVertexFactory->Data.PS3HeightComponent = FVertexStreamComponent(&FLandscapeVertexFactory::GPS3LandscapeHeightVertexBuffer, 0, 4, VET_UByte4);
#endif
		SharedVertexFactory->InitResource();
	}
	SharedVertexFactory->AddRef();
	VertexFactory = SharedVertexFactory;

	return TRUE;
}

void FLandscapeComponentSceneProxy::AddDecalInteraction_RenderingThread(const FDecalInteraction& DecalInteraction)
{
	//checkSlow( IsInRenderingThread() );
	FPrimitiveSceneProxy::AddDecalInteraction_RenderingThread( DecalInteraction );

	INT DecalType = (DecalInteraction.DecalStaticMesh != NULL) ? STATIC_DECALS : DYNAMIC_DECALS;

	FDecalInteraction& NewInteraction = *Decals[DecalType](Decals[DecalType].Num()-1);

#if LOOKING_FOR_PERF_ISSUES
	const DOUBLE StartTime = appSeconds();
#endif // LOOKING_FOR_PERF_ISSUES || SHOW_SLOW_ADD_DECAL_INTERACTIONS

	FLandscapeDecalInteraction* Decal = new FLandscapeDecalInteraction(DecalInteraction.Decal, ComponentLightInfo->GetLandscapeComponent());
	Decal->InitResources_RenderingThread();

	DecalInteraction.RenderData->ReceiverResources.AddItem( Decal );

#if LOOKING_FOR_PERF_ISSUES
	const DOUBLE TimeSpent = (appSeconds() - StartTime) * 1000;
	if( TimeSpent > 0.5f )
	{
		warnf( NAME_DevDecals, TEXT("AddDecal to terrain took: %f"), TimeSpent );
	}
#endif // LOOKING_FOR_PERF_ISSUES || SHOW_SLOW_ADD_DECAL_INTERACTIONS
}

/**
 * Removes a decal interaction from the primitive.  This is called in the rendering thread by RemoveDecalInteraction_GameThread.
 */
void FLandscapeComponentSceneProxy::RemoveDecalInteraction_RenderingThread(UDecalComponent* DecalComponent)
{
	FPrimitiveSceneProxy::RemoveDecalInteraction_RenderingThread( DecalComponent );

#if 0
	// Find the decal interaction representing the given decal component, and remove it from the interaction list.
	const INT DecalLightCacheIndex = FindDecalLightCacheIndex( DecalComponent );
	if ( DecalLightCacheIndex != INDEX_NONE )
	{
		DecalLightCaches.Remove( DecalLightCacheIndex );
	}
#endif
}

/** @return True if the primitive has decals with static relevance which should be rendered in the given view. */
UBOOL FLandscapeComponentSceneProxy::HasRelevantStaticDecals(const FSceneView* View) const
{
	// Landscape render Static Decal as dynamic decal, due to 
	return FALSE;
}

/** @return True if the primitive has decals with dynamic relevance which should be rendered in the given view. */
UBOOL FLandscapeComponentSceneProxy::HasRelevantDynamicDecals(const FSceneView* View) const
{
	if( View->Family->ShowFlags & SHOW_Decals &&
		GSystemSettings.bAllowUnbatchedDecals )
	{
		return (Decals[STATIC_DECALS].Num() + Decals[DYNAMIC_DECALS].Num())> 0;
	}
	return FALSE;
}

FPrimitiveViewRelevance FLandscapeComponentSceneProxy::GetViewRelevance(const FSceneView* View)
{
	FPrimitiveViewRelevance Result;
	const EShowFlags ShowFlags = View->Family->ShowFlags;
	if(ShowFlags & SHOW_Terrain)
	{
		if (IsShown(View))
		{
			Result.SetDPG(GetDepthPriorityGroup(View),TRUE);
			Result.bDecalStaticRelevance = HasRelevantStaticDecals(View);
			Result.bDecalDynamicRelevance = HasRelevantDynamicDecals(View);

			MaterialViewRelevance.SetPrimitiveViewRelevance(Result);

#if WITH_EDITOR
			if( EditToolRenderData && (EditToolRenderData->ToolMaterial || EditToolRenderData->SelectedType || EditToolRenderData->GizmoMaterial ) )
			{
				MaterialViewRelevance.bTranslucency = TRUE;
			}
			else if ( GLandscapeEditRenderMode & ELandscapeEditRenderMode::Mask )
			{
				MaterialViewRelevance.bTranslucency = TRUE;
			}
#endif
			if(
#if PS3
				TRUE
#elif FINAL_RELEASE
				FALSE
#else
				IsRichView(View) || 
				(View->Family->ShowFlags & (SHOW_Bounds|SHOW_Collision))
				|| !GUseLandscapeBatching
				|| !GUseLandscapeStatic
	#if WITH_EDITOR
				// only check these in the editor
				|| IsSelected() 	
				|| (GIsEditor && ((GLandscapeEditRenderMode & ELandscapeEditRenderMode::Mask) || (GLandscapeViewMode > ELandscapeViewMode::EditLayer)) )
	#endif
#endif // PS3
				)
			{
				Result.bDynamicRelevance = TRUE;
			}
			else
			{
				Result.bStaticRelevance = TRUE;
			}
		}
		if (IsShadowCast(View))
		{
			Result.bShadowRelevance = TRUE;
		}
		Result.bDecalStaticRelevance = HasRelevantStaticDecals(View);
		Result.bDecalDynamicRelevance = HasRelevantDynamicDecals(View);
	}
	return Result;
}

/**
*	Determines the relevance of this primitive's elements to the given light.
*	@param	LightSceneInfo			The light to determine relevance for
*	@param	bDynamic (output)		The light is dynamic for this primitive
*	@param	bRelevant (output)		The light is relevant for this primitive
*	@param	bLightMapped (output)	The light is light mapped for this primitive
*/
void FLandscapeComponentSceneProxy::GetLightRelevance(const FLightSceneInfo* LightSceneInfo, UBOOL& bDynamic, UBOOL& bRelevant, UBOOL& bLightMapped) const
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

#if WITH_EDITOR
namespace DebugColorMask
{
	const FLinearColor Masks[5] = 
	{
		FLinearColor(1.f,0.f,0.f,0.f),
		FLinearColor(0.f,1.f,0.f,0.f),
		FLinearColor(0.f,0.f,1.f,0.f),
		FLinearColor(0.f,0.f,0.f,1.f),
		FLinearColor(0.f,0.f,0.f,0.f)
	};
};
#endif

/** 
* Draw the scene proxy as a dynamic element
*
* @param	PDI - draw interface to render to
* @param	View - current view
*/

#if !PS3
void FLandscapeComponentSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	INT FirstLOD = (ForcedLOD >= 0) ? Min<INT>(ForcedLOD, MaxLOD) : Max<INT>(LODBias, 0);
	INT LastLOD  = (ForcedLOD >= 0) ? FirstLOD : Min<INT>(MaxLOD, MaxLOD+LODBias);

	StaticBatchParamArray.Empty((1+LastLOD-FirstLOD) * Square(NumSubsections));

	FMeshBatch MeshBatch;
	MeshBatch.Elements.Empty(Square(NumSubsections));

	MeshBatch.VertexFactory = VertexFactory;
	MeshBatch.MaterialRenderProxy = MaterialInterface->GetRenderProxy(FALSE);
	MeshBatch.LCI = ComponentLightInfo; 
	MeshBatch.ReverseCulling = LocalToWorldDeterminant < 0.0f ? TRUE : FALSE;
	MeshBatch.CastShadow = TRUE;
	MeshBatch.Type = PT_TriangleList;
	MeshBatch.DepthPriorityGroup = SDPG_World;

	for( INT LOD = FirstLOD; LOD <= LastLOD; LOD++ )
	{
		for( INT SubY=0;SubY<NumSubsections;SubY++ )
		{
			for( INT SubX=0;SubX<NumSubsections;SubX++ )
			{
				FMeshBatchElement* BatchElement = new(MeshBatch.Elements) FMeshBatchElement;
				FLandscapeBatchElementParams* BatchElementParams = new(StaticBatchParamArray) FLandscapeBatchElementParams;
				BatchElement->ElementUserData = BatchElementParams;

				INT SubSectionIdx = SubX + SubY*NumSubsections;
				BatchElement->LocalToWorld = SubsectionParams(NumSubsections * SubY + SubX).LocalToWorld;
				BatchElement->WorldToLocal = SubsectionParams(NumSubsections * SubY + SubX).WorldToLocal;

				BatchElementParams->SubsectionParam = &SubsectionParams(NumSubsections * SubY + SubX);

				BatchElementParams->SceneProxy = this;
				BatchElementParams->SubX = SubX;
				BatchElementParams->SubY = SubY;
				BatchElementParams->CurrentLOD = LOD;

				BatchElement->IndexBuffer = IndexBuffers[appCeilLogTwo((LANDSCAPE_MAX_COMPONENT_SIZE+1) / (SubsectionSizeVerts >> LOD))];
				BatchElement->NumPrimitives = Square(((SubsectionSizeVerts >> LOD) - 1)) * 2;
				BatchElement->FirstIndex = 0;
				BatchElement->MinVertexIndex = 0;
				BatchElement->MaxVertexIndex = (LANDSCAPE_MAX_COMPONENT_SIZE+1) * (SubsectionSizeVerts >> LOD) - 1;
			}
		}
	}

	PDI->DrawMesh(MeshBatch,0,FLT_MAX);				
}

void FLandscapeComponentSceneProxy::GetStaticBatchElementVisibility( const class FSceneView& View, const struct FMeshBatch* Batch, TArray<INT>& BatchesToRender ) const
{
	SCOPE_CYCLE_COUNTER(STAT_LandscapeStaticDrawLODTime);
	if( ForcedLOD >= 0 )
	{
		for( INT BatchElementIndex=0;BatchElementIndex < Batch->Elements.Num(); BatchElementIndex++ )
		{
			BatchesToRender.AddItem(BatchElementIndex);
			INC_DWORD_STAT(STAT_LandscapeDrawCalls);
			INC_DWORD_STAT_BY(STAT_LandscapeTriangles,Batch->Elements(BatchElementIndex).NumPrimitives);
		}
	}
	else
	{
		// camera position in local heightmap space
		FVector CameraLocalPos3D = SubsectionParams(0).WorldToLocal.TransformFVector(View.ViewOrigin); 
		FVector2D CameraLocalPos(CameraLocalPos3D.X, CameraLocalPos3D.Y);

		for( INT SubY=0;SubY<NumSubsections;SubY++ )
		{
			for( INT SubX=0;SubX<NumSubsections;SubX++ )
			{
				INT TempLOD = CalcLODForSubsectionNoForced(SubX, SubY, CameraLocalPos);			
				INT BatchElementIndex = TempLOD*Square(NumSubsections) + SubY*NumSubsections + SubX - Max<INT>(LODBias, 0);
				BatchesToRender.AddItem(BatchElementIndex);
				INC_DWORD_STAT(STAT_LandscapeDrawCalls);
				INC_DWORD_STAT_BY(STAT_LandscapeTriangles,Batch->Elements(BatchElementIndex).NumPrimitives);
			}
		}
	}

	INC_DWORD_STAT(STAT_LandscapeComponents);
}
void FLandscapeVertexFactory::GetStaticBatchElementVisibility( const class FSceneView& View, const struct FMeshBatch* Batch, TArray<INT>& BatchesToRender ) const
{
	const FLandscapeComponentSceneProxy* SceneProxy = ((FLandscapeBatchElementParams*)Batch->Elements(0).ElementUserData)->SceneProxy;
	SceneProxy->GetStaticBatchElementVisibility( View, Batch, BatchesToRender );
}

INT FLandscapeComponentSceneProxy::CalcLODForSubsectionNoForced(INT SubX, INT SubY, const FVector2D& CameraLocalPos) const
{
	FVector2D ComponentPosition(0.5f * (FLOAT)SubsectionSizeQuads, 0.5f * (FLOAT)SubsectionSizeQuads);
	FVector2D CurrentCameraLocalPos = CameraLocalPos - FVector2D(SubX * SubsectionSizeQuads,SubY * SubsectionSizeQuads);
	FLOAT ComponentDistance = FVector2D(CurrentCameraLocalPos-ComponentPosition).Size() + DistDiff;
	FLOAT fLOD = Clamp<FLOAT>( ComponentDistance / LODDistance, Max<INT>(LODBias, 0), Min<INT>(MaxLOD, MaxLOD+LODBias) );
	return appFloor( fLOD );
}

INT FLandscapeComponentSceneProxy::CalcLODForSubsection(INT SubX, INT SubY, const FVector2D& CameraLocalPos) const
{
	if( ForcedLOD >= 0 )
	{
		return ForcedLOD;
	}
	else
	{		
		return CalcLODForSubsectionNoForced(SubX, SubY, CameraLocalPos);
	}
}

void FLandscapeComponentSceneProxy::CalcLODParamsForSubsection(const class FSceneView& View, INT SubX, INT SubY, FLOAT& OutfLOD, FVector4& OutNeighborLODs) const
{
	FVector CameraLocalPos3D = SubsectionParams(0).WorldToLocal.TransformFVector(View.ViewOrigin); 
	FVector2D CameraLocalPos(CameraLocalPos3D.X, CameraLocalPos3D.Y);

	FVector2D ComponentPosition(0.5f * (FLOAT)SubsectionSizeQuads, 0.5f * (FLOAT)SubsectionSizeQuads);
	FVector2D CurrentCameraLocalPos = CameraLocalPos - FVector2D(SubX * SubsectionSizeQuads,SubY * SubsectionSizeQuads);
	FLOAT ComponentDistance = FVector2D(CurrentCameraLocalPos-ComponentPosition).Size() + DistDiff;

	INT FirstLOD = Max<INT>(LODBias, 0);
	INT LastLOD = Min<INT>(MaxLOD, MaxLOD+LODBias);
	if (ForcedLOD >= 0)
	{
		OutfLOD = ForcedLOD;
	}
	else
	{
		OutfLOD = Clamp<FLOAT>( ComponentDistance / LODDistance, FirstLOD, LastLOD );
	}

	for (INT Idx = 0; Idx < LANDSCAPE_NEIGHBOR_NUM; ++Idx)
	{
		FLOAT ComponentDistance = FVector2D(CurrentCameraLocalPos-NeighborPosition[Idx]).Size() + DistDiff;
		if (NumSubsections > 1 
			&& ((SubX == 0 && Idx == 2) 
			|| (SubX == NumSubsections-1 && Idx == 1) 
			|| (SubY == 0 && Idx == 3) 
			|| (SubY == NumSubsections-1 && Idx == 0)) )
		{
			OutNeighborLODs[Idx] = ForcedLOD >= 0 ? ForcedLOD : Clamp<FLOAT>( ComponentDistance / LODDistance, FirstLOD, LastLOD );
		}
		else
		{
			OutNeighborLODs[Idx] = ForcedNeighborLOD[Idx] != 255 ? ForcedNeighborLOD[Idx] : Clamp<FLOAT>( ComponentDistance / LODDistance, Max<INT>(NeighborLODBias[Idx]-128, 0), Min<INT>(MaxLOD, MaxLOD+NeighborLODBias[Idx]-128) );
		}

		OutNeighborLODs[Idx] = Max<FLOAT>(OutfLOD, OutNeighborLODs[Idx]);
	}
}
#endif

/** 
* Draw the scene proxy as a dynamic element
*
* @param	PDI - draw interface to render to
* @param	View - current view
* @param	DPGIndex - current depth priority 
* @param	Flags - optional set of flags from EDrawDynamicElementFlags
*/

void FLandscapeComponentSceneProxy::DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
{
#if _WINDOWS || XBOX || PS3 || PLATFORM_MACOSX
	if( (GRHIShaderPlatform!=SP_PCD3D_SM3 && GRHIShaderPlatform!=SP_PCD3D_SM4 && GRHIShaderPlatform!=SP_PCD3D_SM5 && GRHIShaderPlatform!=SP_PCOGL && GRHIShaderPlatform!=SP_XBOXD3D && GRHIShaderPlatform!=SP_PS3) || GUsingMobileRHI || GEmulateMobileRendering )
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_LandscapeDynamicDrawTime);

	INT NumPasses=0;
	INT NumTriangles=0;
	INT NumDrawCalls=0;

	// Determine the DPG the primitive should be drawn in for this view.
	if ((GetDepthPriorityGroup(View) == DPGIndex) && ((View->Family->ShowFlags & SHOW_Terrain) != 0))
	{
		FVector CameraLocalPos3D = SubsectionParams(0).WorldToLocal.TransformFVector(View->ViewOrigin); 
		FVector2D CameraLocalPos(CameraLocalPos3D.X, CameraLocalPos3D.Y);

#if PS3
		FVector2D ComponentPosition(0.5f * (FLOAT)SubsectionSizeQuads, 0.5f * (FLOAT)SubsectionSizeQuads);
#endif

		FMeshBatch Mesh;
		Mesh.LCI = ComponentLightInfo; 
		Mesh.ReverseCulling = LocalToWorldDeterminant < 0.0f ? TRUE : FALSE;
		Mesh.CastShadow = TRUE;
		Mesh.Type = PT_TriangleList;
		Mesh.VertexFactory = VertexFactory;
		Mesh.DepthPriorityGroup = (ESceneDepthPriorityGroup)DPGIndex;

		TArray<FLandscapeBatchElementParams> BatchParamArray;
		BatchParamArray.Empty(NumSubsections*NumSubsections);

#if !PS3
		INT minLOD = Max<INT>(LODBias, 0);
		INT maxLOD = Min<INT>(MaxLOD, MaxLOD+LODBias);
#endif

		for( INT SubY=0;SubY<NumSubsections;SubY++ )
		{
			for( INT SubX=0;SubX<NumSubsections;SubX++ )
			{
				FMeshBatchElement* BatchElement;
				INT SubSectionIdx = NumSubsections * SubY + SubX;
#if PS3
				BatchElement = &Mesh.Elements(0);
#else
				if (SubX==0 && SubY==0)
				{
					BatchElement = &Mesh.Elements(0);
				}
				else
				{
					BatchElement = new(Mesh.Elements) FMeshBatchElement;
				}
#endif

				BatchElement->LocalToWorld = SubsectionParams(SubSectionIdx).LocalToWorld;
				BatchElement->WorldToLocal = SubsectionParams(SubSectionIdx).WorldToLocal;

				FLandscapeBatchElementParams* BatchParams = new(BatchParamArray) FLandscapeBatchElementParams;
				BatchElement->ElementUserData = BatchParams;

				BatchParams->SceneProxy = this;
				BatchParams->SubX = SubX;
				BatchParams->SubY = SubY;

				BatchParams->SubsectionParam = &SubsectionParams(SubSectionIdx);
#if PS3
				FVector2D LocalTranslate(SubX * SubsectionSizeQuads,SubY * SubsectionSizeQuads);

				FVector2D CurrentCameraLocalPos = CameraLocalPos - LocalTranslate;
				FLOAT ComponentDistance = FVector2D(CurrentCameraLocalPos-ComponentPosition).Size() + DistDiff;
				BatchParams->CurrentLOD = Clamp<INT>( appFloor( ComponentDistance / LODDistance ), 0, MaxLOD );
#else
				BatchParams->CurrentLOD = CalcLODForSubsection(SubX, SubY, CameraLocalPos);
#endif

				BatchElement->IndexBuffer = IndexBuffers[appCeilLogTwo((LANDSCAPE_MAX_COMPONENT_SIZE+1) / (SubsectionSizeVerts >> BatchParams->CurrentLOD))];
				BatchElement->NumPrimitives = Square(((SubsectionSizeVerts >> BatchParams->CurrentLOD) - 1)) * 2;

				BatchElement->FirstIndex = 0;
				BatchElement->MinVertexIndex = 0;
				BatchElement->MaxVertexIndex = (LANDSCAPE_MAX_COMPONENT_SIZE+1) * (SubsectionSizeVerts >> BatchParams->CurrentLOD) - 1;

#if USE_PS3_RHI		// No batching for PS3
				// Create the height stream from the texture data
				extern FVertexBufferRHIRef PS3GenerateLandscapeHeightStream( const FVector4& LodDistancesValues, INT CurrentLOD, INT SubsectionSizeQuads, INT NumSubsections, INT SubsectionX, INT SubsectionY, void* PS3Data );

				// Setup parameters for LOD
				if( BatchParams->CurrentLOD < MaxLOD )
				{
					LodDistancesValues.X = CameraLocalPos.X - LocalTranslate.X;
					LodDistancesValues.Y = CameraLocalPos.Y - LocalTranslate.Y;
					LodDistancesValues.Z = ((FLOAT)BatchParams->CurrentLOD+0.5f) * LODDistance;
					LodDistancesValues.W = ((FLOAT)BatchParams->CurrentLOD+1.f) * LODDistance;
				}
				else
				{
					// makes the LOD always negative, so there is no morphing.
					LodDistancesValues.X = 0.f;
					LodDistancesValues.Y = 0.f;
					LodDistancesValues.Z = -1.f;
					LodDistancesValues.W = -2.f;
				}

				// Assign it to the stream
				FLandscapeVertexFactory::GPS3LandscapeHeightVertexBuffer.VertexBufferRHI = PS3GenerateLandscapeHeightStream( LodDistancesValues, BatchParams->CurrentLOD, SubsectionSizeQuads, NumSubsections, SubX, SubY, PlatformData );

				Mesh.MaterialRenderProxy = MaterialInterface->GetRenderProxy(FALSE);
				NumPasses += PDI->DrawMesh(Mesh);
				NumTriangles += Mesh.GetNumPrimitives();
				NumDrawCalls += Mesh.Elements.Num();

				// Have RSX notify that it's finished with the vertex stream
				extern void PS3FinishedWithLandscapeHeightStream();
				PS3FinishedWithLandscapeHeightStream();
#endif
			}
		}

		FLinearColor WireColors[7];
		WireColors[0] = FLinearColor(1,1,1,1);
		WireColors[1] = FLinearColor(1,0,0,1);
		WireColors[2] = FLinearColor(0,1,0,1);
		WireColors[3] = FLinearColor(0,0,1,1);
		WireColors[4] = FLinearColor(1,1,0,1);
		WireColors[5] = FLinearColor(1,0,1,1);
		WireColors[6] = FLinearColor(0,1,1,1);

#if !USE_PS3_RHI

#define LANDSCAPE_DRAW(x) \
		if ( !GUseLandscapeBatching ) \
		{ \
			for( INT i=0;i<Mesh.Elements.Num();i++ ) \
			{ \
				FMeshBatch TempMesh = Mesh; \
				TempMesh.Elements.Empty(1); \
				TempMesh.Elements.AddItem(Mesh.Elements(i)); \
				if (x) \
					NumPasses += DrawRichMesh(PDI, TempMesh, WireColors[((FLandscapeBatchElementParams*)TempMesh.Elements(0).ElementUserData)->CurrentLOD], LevelColor, FLinearColor(1.0f,1.0f,1.0f), PrimitiveSceneInfo, IsSelected()); \
				else \
					NumPasses += PDI->DrawMesh(Mesh); \
				NumTriangles += Mesh.Elements(i).NumPrimitives; \
				NumDrawCalls++; \
			} \
		} \
		else \
		{ \
			if (x) \
				NumPasses += DrawRichMesh(PDI, Mesh, FLinearColor(1,1,1,1), LevelColor, FLinearColor(1.0f,1.0f,1.0f), PrimitiveSceneInfo, IsSelected()); \
			else \
				NumPasses += PDI->DrawMesh(Mesh); \
			NumTriangles += Mesh.GetNumPrimitives(); \
			NumDrawCalls += Mesh.Elements.Num(); \
		}

#if WITH_EDITOR
		if ( GLandscapeViewMode == ELandscapeViewMode::DebugLayer && GLayerDebugColorMaterial && EditToolRenderData && EditToolRenderData->LandscapeComponent )
		{
			const FLandscapeDebugMaterialRenderProxy DebugColorMaterialInstance(GLayerDebugColorMaterial->GetRenderProxy(FALSE), 
				(EditToolRenderData->DebugChannelR >= 0 ? EditToolRenderData->LandscapeComponent->WeightmapTextures(EditToolRenderData->DebugChannelR/4) : NULL),
				(EditToolRenderData->DebugChannelG >= 0 ? EditToolRenderData->LandscapeComponent->WeightmapTextures(EditToolRenderData->DebugChannelG/4) : NULL),
				(EditToolRenderData->DebugChannelB >= 0 ? EditToolRenderData->LandscapeComponent->WeightmapTextures(EditToolRenderData->DebugChannelB/4) : NULL),	
				(EditToolRenderData->DebugChannelR >= 0 ? DebugColorMask::Masks[EditToolRenderData->DebugChannelR%4] : DebugColorMask::Masks[4]),
				(EditToolRenderData->DebugChannelG >= 0 ? DebugColorMask::Masks[EditToolRenderData->DebugChannelG%4] : DebugColorMask::Masks[4]),
				(EditToolRenderData->DebugChannelB >= 0 ? DebugColorMask::Masks[EditToolRenderData->DebugChannelB%4] : DebugColorMask::Masks[4])
				);

			Mesh.MaterialRenderProxy = &DebugColorMaterialInstance;
			LANDSCAPE_DRAW(TRUE);
		}
		else if ( GLandscapeViewMode == ELandscapeViewMode::LayerDensity && EditToolRenderData && EditToolRenderData->LandscapeComponent )
		{
			INT ColorIndex = Min<INT>(EditToolRenderData->LandscapeComponent->WeightmapLayerAllocations.Num(), GEngine->ShaderComplexityColors.Num());
			const FColoredMaterialRenderProxy LayerDensityMaterialInstance(GEngine->LevelColorationUnlitMaterial->GetRenderProxy(FALSE), ColorIndex ? GEngine->ShaderComplexityColors(ColorIndex-1) : FLinearColor::Black);
			Mesh.MaterialRenderProxy = &LayerDensityMaterialInstance;
			LANDSCAPE_DRAW(TRUE);
		}
		else if ( GLandscapeViewMode == ELandscapeViewMode::LOD )
		{
			for( INT i=0;i<Mesh.Elements.Num();i++ )
			{
				FMeshBatch TempMesh = Mesh;
				TempMesh.Elements.Empty(1);
				TempMesh.Elements.AddItem(Mesh.Elements(i));
				INT ColorIndex = ((FLandscapeBatchElementParams*)Mesh.Elements(i).ElementUserData)->CurrentLOD;
				FLinearColor Color = EditToolRenderData->LandscapeComponent->ForcedLOD >= 0 ? WireColors[ColorIndex] : WireColors[ColorIndex]*0.2f;
				const FColoredMaterialRenderProxy LODMaterialInstance(GEngine->LevelColorationUnlitMaterial->GetRenderProxy(FALSE), Color);
				TempMesh.MaterialRenderProxy = &LODMaterialInstance;
				NumPasses += DrawRichMesh(PDI, TempMesh, Color, LevelColor, FLinearColor(1.0f,1.0f,1.0f), PrimitiveSceneInfo, IsSelected());
				NumTriangles += Mesh.Elements(i).NumPrimitives;
				NumDrawCalls++;
			}
		}
		else
#endif // WITH_EDITOR
		// Regular Landscape rendering
		{
			Mesh.MaterialRenderProxy = MaterialInterface->GetRenderProxy(FALSE);
			LANDSCAPE_DRAW(TRUE);
		}

#if WITH_EDITOR
		if ( EditToolRenderData && EditToolRenderData->SelectedType )
		{
			if ((GLandscapeEditRenderMode & ELandscapeEditRenderMode::SelectRegion) && (EditToolRenderData->SelectedType & FLandscapeEditToolRenderData::ST_REGION)
				&& !(GLandscapeEditRenderMode & ELandscapeEditRenderMode::Mask))
			{
				const FLandscapeSelectMaterialRenderProxy SelectMaterialInstance(GSelectionRegionMaterial->GetRenderProxy(FALSE), EditToolRenderData->DataTexture);
				Mesh.MaterialRenderProxy = &SelectMaterialInstance;
				LANDSCAPE_DRAW(FALSE);
			}
			if ((GLandscapeEditRenderMode & ELandscapeEditRenderMode::SelectComponent) && (EditToolRenderData->SelectedType & FLandscapeEditToolRenderData::ST_COMPONENT))
			{
				Mesh.MaterialRenderProxy = GSelectionColorMaterial->GetRenderProxy(0);
				LANDSCAPE_DRAW(FALSE);
			}
		}

		// Render Mask... 
		if ( GLandscapeEditRenderMode & ELandscapeEditRenderMode::Mask )
		{
			if (EditToolRenderData->SelectedType)
			{
				if (EditToolRenderData->SelectedType & FLandscapeEditToolRenderData::ST_REGION)
				{
					const FLandscapeMaskMaterialRenderProxy MaskMaterialInstance(GMaskRegionMaterial->GetRenderProxy(FALSE), EditToolRenderData->DataTexture, (GLandscapeEditRenderMode & ELandscapeEditRenderMode::InvertedMask) );
					Mesh.MaterialRenderProxy = &MaskMaterialInstance;
					LANDSCAPE_DRAW(FALSE);
				}
			}
			else if (!(GLandscapeEditRenderMode & ELandscapeEditRenderMode::InvertedMask))
			{
				const FLandscapeMaskMaterialRenderProxy MaskMaterialInstance(GMaskRegionMaterial->GetRenderProxy(FALSE), NULL, (GLandscapeEditRenderMode & ELandscapeEditRenderMode::InvertedMask) );
				Mesh.MaterialRenderProxy = &MaskMaterialInstance;
				LANDSCAPE_DRAW(FALSE);
			}
		}

		// Render tool
		if( EditToolRenderData )
		{
			if (EditToolRenderData->ToolMaterial)
			{
				Mesh.MaterialRenderProxy = EditToolRenderData->ToolMaterial->GetRenderProxy(0);
				PDI->DrawMesh(Mesh);
			}

			if (EditToolRenderData->GizmoMaterial && GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo)
			{
				Mesh.MaterialRenderProxy = EditToolRenderData->GizmoMaterial->GetRenderProxy(0);
				PDI->DrawMesh(Mesh);
			}
		}
#endif

#endif // !USE_PS3_RHI

		if (View->Family->ShowFlags & SHOW_TerrainPatches)
		{
			DrawWireBox(PDI, PrimitiveSceneInfo->Bounds.GetBox(), FColor(255, 255, 0), DPGIndex);
		}
	}

	INC_DWORD_STAT_BY(STAT_LandscapeComponents, NumPasses);
	INC_DWORD_STAT_BY(STAT_LandscapeDrawCalls, NumDrawCalls);
	INC_DWORD_STAT_BY(STAT_LandscapeTriangles, NumTriangles * NumPasses);
#endif
}

/**
* Draws the primitive's static decal elements.  This is called from the game thread whenever this primitive is attached
* as a receiver for a decal.
*
* The static elements will only be rendered if GetViewRelevance declares both static and decal relevance.
* Called in the game thread.
*
* @param PDI - The interface which receives the primitive elements.
*/
void FLandscapeComponentSceneProxy::DrawStaticDecalElements(FStaticPrimitiveDrawInterface* PDI,const FDecalInteraction& DecalInteraction)
{
	return; // Do nothing for Landscape, because all the render should be dynamic
}

/**
* Draws the primitive's dynamic decal elements.  This is called from the rendering thread for each frame of each view.
* The dynamic elements will only be rendered if GetViewRelevance declares dynamic relevance.
* Called in the rendering thread.
*
* @param	PDI						The interface which receives the primitive elements.
* @param	View					The view which is being rendered.
* @param	InDepthPriorityGroup	The DPG which is being rendered.
* @param	bDynamicLightingPass	TRUE if drawing dynamic lights, FALSE if drawing static lights.
* @param	bDrawOpaqueDecals		TRUE if we want to draw opaque decals
* @param	bDrawTransparentDecals	TRUE if we want to draw transparent decals
* @param	bTranslucentReceiverPass	TRUE during the decal pass for translucent receivers, FALSE for opaque receivers.
*/
void FLandscapeComponentSceneProxy::DrawDynamicDecalElements(
	FPrimitiveDrawInterface* PDI,
	const FSceneView* View,
	UINT InDepthPriorityGroup,
	UBOOL bDynamicLightingPass,
	UBOOL bDrawOpaqueDecals,
	UBOOL bDrawTransparentDecals,
	UBOOL bTranslucentReceiverPass
	)
{
#if _WINDOWS || XBOX || PS3 || PLATFORM_MACOSX
	if( (GRHIShaderPlatform!=SP_PCD3D_SM3 && GRHIShaderPlatform!=SP_PCD3D_SM4 && GRHIShaderPlatform!=SP_PCD3D_SM5 && GRHIShaderPlatform!=SP_PCOGL && GRHIShaderPlatform!=SP_XBOXD3D && GRHIShaderPlatform!=SP_PS3) || GUsingMobileRHI || GEmulateMobileRendering )
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_DecalRenderDynamicTerrainTime);

#if !FINAL_RELEASE
	const UBOOL bRichView = IsRichView(View);
#else
	const UBOOL bRichView = FALSE;
#endif
	// only render decals that haven't been added to a static batch
	INT StartDecalType = !bRichView ? DYNAMIC_DECALS : STATIC_DECALS;

	// Determine the DPG the primitive should be drawn in for this view.
	if (GetViewRelevance(View).GetDPG(InDepthPriorityGroup) == TRUE && (View->Family->ShowFlags & SHOW_Terrain) && (View->Family->ShowFlags & SHOW_Decals) )
	{
		// Compute the set of decals in this DPG.
		FMemMark MemStackMark(GRenderingThreadMemStack);
		TArray<FDecalInteraction*,TMemStackAllocator<GRenderingThreadMemStack> > DPGDecals;
		for (INT DecalType = StartDecalType; DecalType < NUM_DECAL_TYPES; ++DecalType)
		{
			for ( INT DecalIndex = 0 ; DecalIndex < Decals[DecalType].Num() ; ++DecalIndex )
			{
				FDecalInteraction* Interaction = Decals[DecalType](DecalIndex);
				if( // only render decals that haven't been added to a static batch
					(!Interaction->DecalStaticMesh || bRichView) &&
					// match current DPG
					InDepthPriorityGroup == Interaction->DecalState.DepthPriorityGroup &&
					// Render all decals during the opaque pass, as the translucent pass is rejected completely above
					//((Interaction->DecalState.MaterialViewRelevance.bTranslucency && bTranslucentReceiverPass) || !bTranslucentReceiverPass) &&
					((Interaction->DecalState.MaterialViewRelevance.bTranslucency && bDrawTransparentDecals) || (Interaction->DecalState.MaterialViewRelevance.bOpaque && bDrawOpaqueDecals)) &&
					// only render lit decals during dynamic lighting pass
					((Interaction->DecalState.MaterialViewRelevance.bLit && bDynamicLightingPass) || !bDynamicLightingPass) )
				{
					DPGDecals.AddItem( Interaction );
				}
			}
		}
		// Sort decals for the translucent receiver pass
		if( bTranslucentReceiverPass )
		{
			Sort<USE_COMPARE_CONSTPOINTER(FDecalInteraction,LandscapeRender)>( DPGDecals.GetTypedData(), DPGDecals.Num() );
		}

		for ( INT DecalIndex = 0 ; DecalIndex < DPGDecals.Num() ; ++DecalIndex )
		{
			FDecalInteraction* Decal	= DPGDecals(DecalIndex);
			FDecalState& DecalState	= Decal->DecalState;
			FVector FrustumVerts[8];
			UBOOL bFrustumInit = FALSE;
			FDecalRenderData* RenderData = Decal->RenderData;

			const FIndexBuffer* DecalIndexBuffer = NULL;

			FVector2D MinCorner;
			FVector2D MaxCorner;
			if ( DecalState.QuadToClippedScreenSpaceAABB( View, MinCorner, MaxCorner, LocalToWorld ) )
			{
				// Don't override the light's scissor rect ?
				// Set the decal scissor rect.
				RHISetScissorRect( TRUE, appTrunc(MinCorner.X), appTrunc(MinCorner.Y), appTrunc(MaxCorner.X), appTrunc(MaxCorner.Y) );

				if( RenderData && RenderData->ReceiverResources.Num() > 0 && RenderData->DecalVertexFactory)
				{			
					FLandscapeDecalInteraction* DecalResource = static_cast<FLandscapeDecalInteraction*>(RenderData->ReceiverResources(0));

					FVector CameraLocalPos3D = SubsectionParams(0).WorldToLocal.TransformFVector(View->ViewOrigin); 
					FVector2D CameraLocalPos(CameraLocalPos3D.X, CameraLocalPos3D.Y);

#if PS3
					FVector2D ComponentPosition(0.5f * (FLOAT)SubsectionSizeQuads, 0.5f * (FLOAT)SubsectionSizeQuads);
#endif

					FMeshBatch Mesh;
					Mesh.ReverseCulling = LocalToWorldDeterminant < 0.0f ? TRUE : FALSE;
					Mesh.CastShadow = TRUE;
					Mesh.VertexFactory = RenderData->DecalVertexFactory->CastToFVertexFactory();
					Mesh.MaterialRenderProxy = DecalState.DecalMaterial->GetRenderProxy(0);

					// Terrain decals are much less susceptible to z-fighting because of how we draw them.
					// So bias the DepthBias to push the decal closer to the object. Allows us have a much more conservative
					// setting for everything else.
					Mesh.DepthBias = DecalState.DepthBias * 0.1f;
					Mesh.SlopeScaleDepthBias = DecalState.SlopeScaleDepthBias;
					Mesh.Type = PT_TriangleList;
					Mesh.DepthPriorityGroup = InDepthPriorityGroup;
					Mesh.bIsDecal = TRUE;
					
					if( DecalState.bDecalMaterialHasStaticLightingUsage )
					{
						if( bTranslucentReceiverPass )
						{
							Mesh.LCI = RenderData->LCI;
						}
						else
						{
							Mesh.LCI = ComponentLightInfo;
						}
					}
					else
					{
						Mesh.LCI = NULL;
					}

					FLandscapeBatchElementParams BatchParams;

					// Update CurrentLOD
					for( INT SubY=0;SubY<NumSubsections;SubY++ )
					{
						for( INT SubX=0;SubX<NumSubsections;SubX++ )
						{
							// No batching for Landscape Decal
							FMeshBatchElement* BatchElement = &Mesh.Elements(0);
							INT SubSectionIdx = NumSubsections * SubY + SubX;

#if PS3
							FVector2D LocalTranslate(SubX * SubsectionSizeQuads,SubY * SubsectionSizeQuads);

							FVector2D CurrentCameraLocalPos = CameraLocalPos - LocalTranslate;
							FLOAT ComponentDistance = FVector2D(CurrentCameraLocalPos-ComponentPosition).Size() + DistDiff;
							BatchParams.CurrentLOD = Clamp<INT>( appFloor( ComponentDistance / LODDistance ), 0, MaxLOD );
#else
							BatchParams.CurrentLOD = CalcLODForSubsection(SubX, SubY, CameraLocalPos);
#endif

							INT LODIndex = appCeilLogTwo((LANDSCAPE_MAX_COMPONENT_SIZE+1) / (SubsectionSizeVerts >> BatchParams.CurrentLOD));
							FLandscapeDecalIndexBuffers* DecalIndexBuffers = DecalResource->GetDecalIndexBuffers();
							DecalIndexBuffer = DecalIndexBuffers->LODIndexBuffers[LODIndex];

							const INT TriCount = DecalIndexBuffer ? ((FLandscapeSubRegionIndexBuffer*)DecalIndexBuffer)->GetTriCount(SubSectionIdx) : 0;

							if( TriCount > 0 )
							{
								//((FLandscapeDecalVertexFactory*)RenderData->DecalVertexFactory)->SetSceneProxy(this);

								BatchElement->LocalToWorld = SubsectionParams(SubSectionIdx).LocalToWorld;
								BatchElement->WorldToLocal = SubsectionParams(SubSectionIdx).WorldToLocal;

								BatchElement->ElementUserData = &BatchParams;

								BatchParams.SceneProxy = this;
								BatchParams.SubX = SubX;
								BatchParams.SubY = SubY;

								BatchParams.SubsectionParam = &SubsectionParams(SubSectionIdx);

								//BatchElementParams->HeightmapTexture = HeightmapTexture;

								BatchElement->IndexBuffer = DecalIndexBuffer;

								// This makes the decal render using a scissor rect (for performance reasons).
								Mesh.DecalState = &DecalState;

								BatchElement->FirstIndex = DecalIndexBuffers->GetStartIndex(SubSectionIdx, LODIndex);
								BatchElement->MinVertexIndex = 0;
								BatchElement->MaxVertexIndex = (LANDSCAPE_MAX_COMPONENT_SIZE+1) * (SubsectionSizeVerts >> BatchParams.CurrentLOD) - 1;
								BatchElement->NumPrimitives = TriCount;

								RenderData->DecalVertexFactory->SetDecalMatrix( DecalState.WorldTexCoordMtx );
								RenderData->DecalVertexFactory->SetDecalLocation( DecalState.HitLocation );
								RenderData->DecalVertexFactory->SetDecalOffset( FVector2D(DecalState.OffsetX, DecalState.OffsetY) );
								FVector V1 = DecalState.HitBinormal;
								FVector V2 = DecalState.HitTangent;
								FVector V3 = DecalState.HitNormal;
								V1 = BatchElement->WorldToLocal.TransformNormal( V1 ).SafeNormal();
								V2 = BatchElement->WorldToLocal.TransformNormal( V2 ).SafeNormal();
								V3 = BatchElement->WorldToLocal.TransformNormal( V3 ).SafeNormal();
								RenderData->DecalVertexFactory->SetDecalLocalBinormal( V1 );
								RenderData->DecalVertexFactory->SetDecalLocalTangent( V2 );
								RenderData->DecalVertexFactory->SetDecalLocalNormal( V3 );
								RenderData->DecalVertexFactory->SetDecalMinMaxBlend(RenderData->DecalBlendRange);

								static const FLinearColor WireColor(0.5f,1.0f,0.5f);
								const INT NumPasses = DrawRichMesh(PDI,Mesh, WireColor, LevelColor, FLinearColor(1.0f,1.0f,1.0f), PrimitiveSceneInfo, FALSE);

								INC_DWORD_STAT_BY(STAT_DecalTriangles,Mesh.GetNumPrimitives()*NumPasses);
								INC_DWORD_STAT(STAT_DecalDrawCalls);
							}
						}
					}
				}

				// Restore the scissor rect.
				RHISetScissorRect( FALSE, 0, 0, 0, 0 );
			}
		}
	}
#endif
}

//
// FLandscapeVertexBuffer
//

/** 
* Initialize the RHI for this rendering resource 
*/
void FLandscapeVertexBuffer::InitRHI()
{
	// create a static vertex buffer
	VertexBufferRHI = RHICreateVertexBuffer(Square(SizeVerts) * sizeof(FLandscapeVertex), NULL, RUF_Static);
	FLandscapeVertex* Vertex = (FLandscapeVertex*)RHILockVertexBuffer(VertexBufferRHI, 0, Square(SizeVerts) * sizeof(FLandscapeVertex),FALSE);

	for( INT y=0;y<SizeVerts;y++ )
	{
		for( INT x=0;x<SizeVerts;x++ )
		{
			Vertex->VertexX = x;
			Vertex->VertexY = y;
			Vertex++;
		}
	}

	RHIUnlockVertexBuffer(VertexBufferRHI);
}

//
// FLandscapeVertexBuffer
//
FLandscapeIndexBuffer::FLandscapeIndexBuffer(INT SizeQuads, INT VBSizeVertices)
{
	TArray<WORD> NewIndices;
	NewIndices.Empty(SizeQuads*SizeQuads*6);
	for( INT y=0;y<SizeQuads;y++ )
	{
		for( INT x=0;x<SizeQuads;x++ )
		{
			NewIndices.AddItem( (x+0) + (y+0) * VBSizeVertices );
			NewIndices.AddItem( (x+1) + (y+1) * VBSizeVertices );
			NewIndices.AddItem( (x+1) + (y+0) * VBSizeVertices );
			NewIndices.AddItem( (x+0) + (y+0) * VBSizeVertices );
			NewIndices.AddItem( (x+0) + (y+1) * VBSizeVertices );
			NewIndices.AddItem( (x+1) + (y+1) * VBSizeVertices );
		}
	}
	Indices = NewIndices;

	InitResource();
}

//
// FLandscapeSubRegionIndexBuffer
//
FLandscapeSubRegionIndexBuffer::FLandscapeSubRegionIndexBuffer()
: NumSubIndex(0)
{
}

void FLandscapeSubRegionIndexBuffer::AddSubsection(TArray<WORD>& NewIndices, INT MinX, INT MinY, INT MaxX, INT MaxY, INT VBSizeVertices)
{
	INT SizeX = MaxX - MinX;
	INT SizeY = MaxY - MinY;

	INT NumTris = SizeX*SizeY*2;
	if( NumTris > 0 )
	{
		WORD* IndexPointer = &NewIndices(NewIndices.Add(NumTris * 3));
		for( INT y=MinY;y<MaxY;y++ )
		{
			for( INT x=MinX;x<MaxX;x++ )
			{
				*IndexPointer++ = (x+0) + (y+0) * VBSizeVertices;
				*IndexPointer++ = (x+1) + (y+1) * VBSizeVertices;
				*IndexPointer++ = (x+1) + (y+0) * VBSizeVertices;
				*IndexPointer++ = (x+0) + (y+0) * VBSizeVertices;
				*IndexPointer++ = (x+0) + (y+1) * VBSizeVertices;
				*IndexPointer++ = (x+1) + (y+1) * VBSizeVertices;
			}
		}
	}
	TriCount[NumSubIndex] = NumTris;
	NumSubIndex++;
}

void FLandscapeSubRegionIndexBuffer::Finalize(TArray<WORD>& NewIndices)
{
	Indices = NewIndices;
}

//
// FLandscapeDecalIndexBuffers
//
FLandscapeDecalIndexBuffers::FLandscapeDecalIndexBuffers(INT MinX[], INT MinY[], INT MaxX[], INT MaxY[], INT NumSubsections, INT SubsectionSize)
:	 NumTotalIndex(0)
{
	NumSubIndex = Square(NumSubsections);
	INT MinLOD = appCeilLogTwo((LANDSCAPE_MAX_COMPONENT_SIZE+1) / SubsectionSize);
	for (INT LODIdx = 0; LODIdx < LANDSCAPE_LOD_LEVELS; ++LODIdx)
	{
		INT LODShift = Max(0, LODIdx - MinLOD);

		LODIndexBuffers[LODIdx] = new FLandscapeSubRegionIndexBuffer;
		TArray<WORD> NewIndices;

		for (INT SubY = 0; SubY < NumSubsections; ++SubY)
		{
			for (INT SubX = 0; SubX < NumSubsections; ++SubX)
			{
				INT SubIdx = SubY*NumSubsections + SubX;

				StartIndex[SubIdx][LODIdx] = NewIndices.Num();
				LODIndexBuffers[LODIdx]->AddSubsection(NewIndices, MinX[SubIdx]>>LODShift, MinY[SubIdx]>>LODShift, MaxX[SubIdx]>>LODShift, MaxY[SubIdx]>>LODShift, (LANDSCAPE_MAX_COMPONENT_SIZE+1) );
			}
		}

		LODIndexBuffers[LODIdx]->Finalize(NewIndices);

		NumTotalIndex += NewIndices.Num();
	}
}

void FLandscapeDecalIndexBuffers::InitResources()
{
	for (INT i = 0; i < LANDSCAPE_LOD_LEVELS; ++i)
	{
		LODIndexBuffers[i]->InitResource();
	}
}

FLandscapeDecalIndexBuffers::~FLandscapeDecalIndexBuffers()
{
	for (INT i = 0; i < LANDSCAPE_LOD_LEVELS; ++i)
	{
		delete LODIndexBuffers[i];
	}
}

//
// FLandscapeVertexFactoryShaderParameters
//

/** VTF landscape vertex factory */

/** Shader parameters for use with FLandscapeVertexFactory */
class FLandscapeVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:
	/**
	* Bind shader constants by name
	* @param	ParameterMap - mapping of named shader constants to indices
	*/
	virtual void Bind(const FShaderParameterMap& ParameterMap)
	{
		LocalToWorldParameter.Bind(ParameterMap,TEXT("LocalToWorld"));
		WorldToLocalNoScalingParameter.Bind(ParameterMap,TEXT("WorldToLocalNoScaling"),TRUE);
		HeightmapUVScaleBiasParameter.Bind(ParameterMap,TEXT("HeightmapUVScaleBias"),TRUE);
		WeightmapUVScaleBiasParameter.Bind(ParameterMap,TEXT("WeightmapUVScaleBias"),TRUE);
		HeightmapTextureParameter.Bind(ParameterMap,TEXT("HeightmapTexture"),TRUE);
		LodValuesParameter.Bind(ParameterMap,TEXT("LodValues"),TRUE);
		LodDistancesValuesParameter.Bind(ParameterMap,TEXT("LodDistancesValues"),TRUE);
		SubsectionSizeVertsLayerUVPanParameter.Bind(ParameterMap,TEXT("SubsectionSizeVertsLayerUVPan"),TRUE);
		LodBiasParameter.Bind(ParameterMap,TEXT("LodBias"),TRUE);
		LightmapScaleBiasParameter.Bind(ParameterMap,TEXT("LandscapeLightmapScaleBias"),TRUE);
	}

	/**
	* Serialize shader params to an archive
	* @param	Ar - archive to serialize to
	*/
	virtual void Serialize(FArchive& Ar)
	{
		Ar << LocalToWorldParameter;
		Ar << WorldToLocalNoScalingParameter;
		Ar << HeightmapUVScaleBiasParameter;
		Ar << WeightmapUVScaleBiasParameter;
		Ar << HeightmapTextureParameter;
		Ar << LodValuesParameter;
		Ar << LodDistancesValuesParameter;
		Ar << SubsectionSizeVertsLayerUVPanParameter;
		Ar << LodBiasParameter;
		Ar << LightmapScaleBiasParameter;
	}

	/**
	* Set any shader data specific to this vertex factory
	*/
	virtual void Set(FShader* VertexShader,const FVertexFactory* VertexFactory,const FSceneView& View) const
	{
	}

	/**
	* 
	*/
	virtual void SetMesh(FShader* VertexShader, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View) const
	{
		SCOPE_CYCLE_COUNTER(STAT_LandscapeVFDrawTime);
		const FMeshBatchElement& BatchElement = Mesh.Elements(BatchElementIndex);
		const FLandscapeBatchElementParams* BatchElementParams = (FLandscapeBatchElementParams*)BatchElement.ElementUserData;
		const FLandscapeSubsectionParams* SubsectionParam = BatchElementParams ? BatchElementParams->SubsectionParam : NULL;
		check(SubsectionParam);

		SetVertexShaderValue(
			VertexShader->GetVertexShader(),
			LocalToWorldParameter,
			BatchElement.LocalToWorld.ConcatTranslation(View.PreViewTranslation)
			);

		// WorldToLocal is used by the vertex factory to transform the light vectors into tangent space.
		// We need to remove scaling for correct lighting with non-uniform scaling
		SetVertexShaderValue(VertexShader->GetVertexShader(), WorldToLocalNoScalingParameter, SubsectionParam->WorldToLocalNoScaling);

		const FLandscapeComponentSceneProxy* SceneProxy = BatchElementParams->SceneProxy;
		if( HeightmapTextureParameter.IsBound() )
		{
			SetVertexShaderTextureParameter(
				VertexShader->GetVertexShader(),
				HeightmapTextureParameter,
				SceneProxy->HeightmapTexture->Resource->TextureRHI);
		}

		if( LodBiasParameter.IsBound() )
		{	
			FVector4 LodBias(
				SceneProxy->LODDistanceFactor,
				1.f / ( 1.f - SceneProxy->LODDistanceFactor ),
				SceneProxy->HeightmapTexture->Mips.Num() - Min(SceneProxy->HeightmapTexture->ResidentMips, SceneProxy->HeightmapTexture->RequestedMips),
				0.f // Reserved
				);
			SetVertexShaderValue(VertexShader->GetVertexShader(), LodBiasParameter, LodBias);
		}

		// Cached values
		if (HeightmapUVScaleBiasParameter.IsBound())
		{
			SetVertexShaderValue(VertexShader->GetVertexShader(),HeightmapUVScaleBiasParameter, SubsectionParam->HeightmapUVScaleBias);
		}

		if (WeightmapUVScaleBiasParameter.IsBound())
		{
			SetVertexShaderValue(VertexShader->GetVertexShader(),WeightmapUVScaleBiasParameter, SubsectionParam->WeightmapUVScaleBias);
		}

		if (LightmapScaleBiasParameter.IsBound())
		{
			SetVertexShaderValue(VertexShader->GetVertexShader(),LightmapScaleBiasParameter, SubsectionParam->LandscapeLightmapScaleBias);
		}

		if( SubsectionSizeVertsLayerUVPanParameter.IsBound() )
		{
			SetVertexShaderValue(VertexShader->GetVertexShader(), SubsectionSizeVertsLayerUVPanParameter, SubsectionParam->SubsectionSizeVertsLayerUVPan);
		}

#if !PS3
		// Calculate LOD params
		FLOAT fCurrentLOD;
		FVector4 CurrentNeighborLODs;
		SceneProxy->CalcLODParamsForSubsection(View, BatchElementParams->SubX, BatchElementParams->SubY, fCurrentLOD, CurrentNeighborLODs);
#endif

		if( LodDistancesValuesParameter.IsBound() )
		{
#if PS3
			SetVertexShaderValue(VertexShader->GetVertexShader(), LodDistancesValuesParameter, SceneProxy->LodDistancesValues);
#else
			SetVertexShaderValue(VertexShader->GetVertexShader(), LodDistancesValuesParameter, CurrentNeighborLODs);
#endif
		}

		if( LodValuesParameter.IsBound() )
		{
#if PS3
			FVector4 LodValues;
			LodValues.X = (FLOAT)BatchElementParams->CurrentLOD;
			// convert current LOD coordinates into highest LOD coordinates
			LodValues.Y = (FLOAT)SceneProxy->SubsectionSizeQuads / (FLOAT)(((SceneProxy->SubsectionSizeVerts) >> BatchElementParams->CurrentLOD)-1);

			if( BatchElementParams->CurrentLOD < SceneProxy->MaxLOD )
			{
			    // convert highest LOD coordinates into next LOD coordinates.
			    LodValues.Z = (FLOAT)(((SceneProxy->SubsectionSizeVerts) >> (BatchElementParams->CurrentLOD+1))-1) / (FLOAT)SceneProxy->SubsectionSizeQuads;
    
			    // convert next LOD coordinates into highest LOD coordinates.
			    LodValues.W = 1.f / LodValues.Z;
			}
			else
			{
				LodValues.Z = 1.f;
				LodValues.W = 1.f;
			}
#else
			FVector4 LodValues(
				fCurrentLOD,
				// convert current LOD coordinates into highest LOD coordinates
				(FLOAT)SceneProxy->SubsectionSizeQuads / (FLOAT)(((SceneProxy->SubsectionSizeVerts) >> BatchElementParams->CurrentLOD)-1),
				(FLOAT)((SceneProxy->SubsectionSizeVerts >> BatchElementParams->CurrentLOD) - 1),
				1.f/(FLOAT)((SceneProxy->SubsectionSizeVerts >> BatchElementParams->CurrentLOD) - 1) );
#endif

			SetVertexShaderValue(VertexShader->GetVertexShader(), LodValuesParameter, LodValues);
		}
	}

private:
	INT	TessellationLevel;
	FShaderParameter LocalToWorldParameter;
	FShaderParameter WorldToLocalNoScalingParameter;
	FShaderParameter LightmapScaleBiasParameter;
	FShaderParameter HeightmapUVScaleBiasParameter;
	FShaderParameter WeightmapUVScaleBiasParameter;
	FShaderParameter LodValuesParameter;
	FShaderParameter LodDistancesValuesParameter;
	FShaderParameter SubsectionSizeVertsLayerUVPanParameter;
	FShaderParameter LodBiasParameter;
	FShaderResourceParameter HeightmapTextureParameter;
};

/** Pixel shader parameters for use with FLandscapeVertexFactory */
class FLandscapeVertexFactoryPixelShaderParameters : public FVertexFactoryShaderParameters
{
public:
	/**
	* Bind shader constants by name
	* @param	ParameterMap - mapping of named shader constants to indices
	*/
	virtual void Bind(const FShaderParameterMap& ParameterMap)
	{
		HeightmapTextureParameter.Bind(ParameterMap,TEXT("HeightmapTexture"),TRUE);
		LocalToWorldNoScalingParameter.Bind(ParameterMap,TEXT("LocalToWorldNoScaling"),TRUE);
	}

	/**
	* Serialize shader params to an archive
	* @param	Ar - archive to serialize to
	*/
	virtual void Serialize(FArchive& Ar)
	{
		Ar	<< HeightmapTextureParameter
			<< LocalToWorldNoScalingParameter;
	}

	/**
	* Set any shader data specific to this vertex factory
	*/
	virtual void Set(FShader* PixelShader,const FVertexFactory* VertexFactory,const FSceneView& View) const
	{
	}

	/**
	* 
	*/
	virtual void SetMesh(FShader* PixelShader, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View) const
	{
		const FMeshBatchElement& BatchElement = Mesh.Elements(BatchElementIndex);
		const FLandscapeBatchElementParams* BatchElementParams = (FLandscapeBatchElementParams*)BatchElement.ElementUserData;

		if( HeightmapTextureParameter.IsBound() )
		{
			SetTextureParameter(
				PixelShader->GetPixelShader(),
				HeightmapTextureParameter,
				BatchElementParams->SceneProxy->HeightmapTexture->Resource);
		}

		if( LocalToWorldNoScalingParameter.IsBound() )
		{
			const FLandscapeSubsectionParams* SubsectionParam = BatchElementParams ? BatchElementParams->SubsectionParam : NULL;
			// This is used to calculate the TangentToWorld matrix. The other vertex factories call CalcTangentToWorld() in
			// MaterialTemplate.usf, that silently normalizes the TangentToWorld basis. This removes scaling that would otherwise
			// be present. We need to do the same for Landscape, but we do it once per component on the CPU rather than per pixel.
			if (SubsectionParam)
			{
				SetPixelShaderValue(PixelShader->GetPixelShader(), LocalToWorldNoScalingParameter, SubsectionParam->LocalToWorldNoScaling);
			}
		}
	}

private:
	FShaderParameter HeightmapUVScaleBiasParameter;
	FShaderResourceParameter HeightmapTextureParameter;
	FShaderParameter LocalToWorldNoScalingParameter;
};

//
// FLandscapeVertexFactory
//

void FLandscapeVertexFactory::InitRHI()
{
	// list of declaration items
	FVertexDeclarationElementList Elements;

	// position decls
	Elements.AddItem(AccessStreamComponent(Data.PositionComponent, VEU_Position));
#if PS3
	Elements.AddItem(AccessStreamComponent(Data.PS3HeightComponent, VEU_BlendWeight));
#endif

	// create the actual device decls
	InitDeclaration(Elements,FVertexFactory::DataType(),FALSE,FALSE);
}

#if PS3
// Updated by PS3 RHI code with the appropriate vertex buffer memory
FPS3LandscapeHeightVertexBuffer FLandscapeVertexFactory::GPS3LandscapeHeightVertexBuffer;
#endif

FVertexFactoryShaderParameters* FLandscapeVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	switch( ShaderFrequency )
	{
	case SF_Vertex:
		return new FLandscapeVertexFactoryShaderParameters();
		break;
	case SF_Pixel:
		return new FLandscapeVertexFactoryPixelShaderParameters();
		break;
	default:
		return NULL;
	}
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FLandscapeVertexFactory, "LandscapeVertexFactory", TRUE, TRUE, TRUE, FALSE, TRUE, VER_LANDSCAPEVERTEXFACTORY_ADD_XYOFFSET_PARAMS, 0);

// Decal VertexFactory

class FLandscapeDecalVertexFactoryShaderParameters : public FLandscapeVertexFactoryShaderParameters
{
public:
	typedef FLandscapeVertexFactoryShaderParameters Super;

	/**
	 * Bind shader constants by name
	 * @param	ParameterMap - mapping of named shader constants to indices
	 */
	virtual void Bind(const FShaderParameterMap& ParameterMap)
	{
		Super::Bind( ParameterMap );
		DecalMatrixParameter.Bind( ParameterMap, TEXT("DecalMatrix"), TRUE );
		DecalLocationParameter.Bind( ParameterMap, TEXT("DecalLocation"), TRUE );
		DecalOffsetParameter.Bind( ParameterMap, TEXT("DecalOffset"), TRUE );
		DecalLocalBinormal.Bind( ParameterMap, TEXT("DecalLocalBinormal"), TRUE );
		DecalLocalTangent.Bind( ParameterMap, TEXT("DecalLocalTangent"), TRUE );
		DecalLocalNormalParameter.Bind( ParameterMap, TEXT("DecalLocalNormal"), TRUE );
		//DecalBlendIntervalParameter.Bind( ParameterMap, TEXT("DecalBlendInterval"), TRUE );
	}

	/**
	* Serialize shader params to an archive
	* @param	Ar - archive to serialize to
	*/
	virtual void Serialize(FArchive& Ar)
	{
		Super::Serialize( Ar );
		Ar << DecalMatrixParameter;
		Ar << DecalLocationParameter;
		Ar << DecalOffsetParameter;
		Ar << DecalLocalBinormal;
		Ar << DecalLocalTangent;
		Ar << DecalLocalNormalParameter;
	}

	/**
	 * Set any shader data specific to this vertex factory
	 */
	virtual void Set(FShader* VertexShader,const FVertexFactory* VertexFactory,const FSceneView& View) const
	{
		Super::Set( VertexShader, VertexFactory, View );

		if (VertexFactory && VertexFactory != FLandscapeComponentSceneProxy::SharedVertexFactory)
		{
			FLandscapeDecalVertexFactory* DecalVF = (FLandscapeDecalVertexFactory*)VertexFactory;
			SetVertexShaderValue(  VertexShader->GetVertexShader(), DecalMatrixParameter, DecalVF->GetDecalMatrix() );
			SetVertexShaderValue(  VertexShader->GetVertexShader(), DecalLocationParameter, DecalVF->GetDecalLocation() + View.PreViewTranslation  );
			SetVertexShaderValue(  VertexShader->GetVertexShader(), DecalOffsetParameter, DecalVF->GetDecalOffset() );
			SetVertexShaderValue(  VertexShader->GetVertexShader(), DecalLocalBinormal, DecalVF->GetDecalLocalBinormal() );
			SetVertexShaderValue(  VertexShader->GetVertexShader(), DecalLocalTangent, DecalVF->GetDecalLocalTangent() );
			SetVertexShaderValue(  VertexShader->GetVertexShader(), DecalLocalNormalParameter, DecalVF->GetDecalLocalNormal() );	
		}
	}

private:
	FShaderParameter DecalMatrixParameter;	
	FShaderParameter DecalLocationParameter;
	FShaderParameter DecalOffsetParameter;
	FShaderParameter DecalLocalBinormal;
	FShaderParameter DecalLocalTangent;
	FShaderParameter DecalLocalNormalParameter;
};

/** Pixel shader parameters for use with FLandscapeVertexFactory */
class FLandscapeDecalVertexFactoryPixelShaderParameters : public FLandscapeVertexFactoryPixelShaderParameters
{
public:

	typedef FLandscapeVertexFactoryPixelShaderParameters Super;
	/**
	* Bind shader constants by name
	* @param	ParameterMap - mapping of named shader constants to indices
	*/
	virtual void Bind(const FShaderParameterMap& ParameterMap)
	{
		Super::Bind(ParameterMap);
		DecalLocalNormalParameter.Bind( ParameterMap, TEXT("DecalLocalNormal"), TRUE );
		DecalLocalBinormalParameter.Bind( ParameterMap, TEXT("DecalLocalBinormal"), TRUE );
		DecalLocalTangentParameter.Bind( ParameterMap, TEXT("DecalLocalTangent"), TRUE );
		DecalBlendIntervalParameter.Bind( ParameterMap, TEXT("DecalBlendInterval"), TRUE );
	}

	/**
	* Serialize shader params to an archive
	* @param	Ar - archive to serialize to
	*/
	virtual void Serialize(FArchive& Ar)
	{
		Super::Serialize(Ar);
		Ar << DecalLocalNormalParameter;
		Ar << DecalLocalBinormalParameter;
		Ar << DecalLocalTangentParameter;
		Ar << DecalBlendIntervalParameter;
	}

	/**
	* Set any shader data specific to this vertex factory
	*/
	virtual void Set(FShader* PixelShader,const FVertexFactory* VertexFactory,const FSceneView& View) const
	{
	}

	/**
	* 
	*/
	virtual void SetMesh(FShader* PixelShader, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View) const
	{
		Super::SetMesh( PixelShader, Mesh, BatchElementIndex, View );
		if (Mesh.VertexFactory && Mesh.VertexFactory != FLandscapeComponentSceneProxy::SharedVertexFactory)
		{
			FLandscapeDecalVertexFactory* DecalVF = (FLandscapeDecalVertexFactory*)Mesh.VertexFactory;
			SetPixelShaderValues<FVector>( PixelShader->GetPixelShader(), DecalLocalNormalParameter, &DecalVF->GetDecalLocalNormal(), 1 );
			SetPixelShaderValues<FVector>( PixelShader->GetPixelShader(), DecalLocalBinormalParameter, &DecalVF->GetDecalLocalBinormal(), 1 );
			SetPixelShaderValues<FVector>( PixelShader->GetPixelShader(), DecalLocalTangentParameter, &DecalVF->GetDecalLocalTangent(), 1 );
			SetPixelShaderValues<FVector2D>( PixelShader->GetPixelShader(), DecalBlendIntervalParameter, &DecalVF->GetDecalMinMaxBlend(), 1 );
		}
	}

private:
	FShaderParameter DecalLocalNormalParameter;
	FShaderParameter DecalLocalBinormalParameter;
	FShaderParameter DecalLocalTangentParameter;
	FShaderParameter DecalBlendIntervalParameter;
};

/**
* Copy the data from another vertex factory
* @param Other - factory to copy from
*/
void FLandscapeVertexFactory::Copy(const FLandscapeVertexFactory& Other)
{
	//SetSceneProxy(Other.GetSceneProxy());
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FLandscapeVertexFactoryCopyData,
		FLandscapeVertexFactory*,VertexFactory,this,
		const DataType*,DataCopy,&Other.Data,
	{
		VertexFactory->Data = *DataCopy;
	});	
	BeginUpdateResourceRHI(this);
}

/**
* Vertex factory interface for creating a corresponding decal vertex factory
* Copies the data from this existing vertex factory.
*
* @return new allocated decal vertex factory
*/
FDecalVertexFactoryBase* FLandscapeVertexFactory::CreateDecalVertexFactory() const
{
	FLandscapeDecalVertexFactory* DecalFactory = new FLandscapeDecalVertexFactory();
	DecalFactory->Copy(*this);
	return DecalFactory;
}

UBOOL FLandscapeDecalVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
#if MOBILE
	return FALSE;
#endif
	return (Material->IsUsedWithDecals() || AllowDebugViewmodes(Platform) && Material->IsSpecialEngineMaterial() || Material->IsDecalMaterial() ) &&
		(Platform==SP_PCD3D_SM3 || Platform==SP_PCD3D_SM4 || Platform==SP_PCD3D_SM5 || Platform==SP_PCOGL || Platform==SP_XBOXD3D || Platform==SP_PS3);
}

FVertexFactoryShaderParameters* FLandscapeDecalVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	switch( ShaderFrequency )
	{
	case SF_Vertex:
		return new FLandscapeDecalVertexFactoryShaderParameters();
		break;
	case SF_Pixel:
		return new FLandscapeDecalVertexFactoryPixelShaderParameters();
		break;
	default:
		return NULL;
	}
}

/** bind terrain decal vertex factory to its shader file and its shader parameters */
IMPLEMENT_VERTEX_FACTORY_TYPE(FLandscapeDecalVertexFactory, "LandscapeVertexFactory", TRUE, TRUE, TRUE, FALSE, TRUE, VER_LANDSCAPEDECALVERTEXFACTORY, 0);


/** FLandscapeMICResource 
  * version of FMaterialResource that only caches shaders necessary for landscape rendering 
  */
class FLandscapeMICResource : public FMaterialResource
{
	UBOOL LayerThumbnail;
	INT DataWeightmapIndex;
	INT DataWeightmapSize;
public:
	FLandscapeMICResource(UMaterial* InMaterial, UBOOL InLayerThumbnail, INT InDataWeightmapIndex, INT InDataWeightmapSize)
	:	FMaterialResource(InMaterial)
	,	LayerThumbnail(InLayerThumbnail)
	,	DataWeightmapIndex(InDataWeightmapIndex)
	,	DataWeightmapSize(InDataWeightmapSize)
	{
	}

	/** Entry point for compiling a specific material property.  This must call SetMaterialProperty. */
	virtual INT CompileProperty(EMaterialProperty Property,FMaterialCompiler* Compiler) const
	{
		// If the property is not active, don't compile it
		if (!IsActiveMaterialProperty(Material, Property))
		{
			if (DataWeightmapIndex == INDEX_NONE || DataWeightmapSize <= 0 || Property != MP_OpacityMask)
			{
				return INDEX_NONE;
			}
		}

		// Should be same as FMaterialResource::CompileProperty, other than MP_OpacityMask...
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
		case MP_OpacityMask: 
			if (DataWeightmapIndex != INDEX_NONE && DataWeightmapSize > 0)
			{
				return Compiler->Sub( Compiler->Constant(1.0f), 
						Compiler->Dot(
							Compiler->TextureSample(
								Compiler->TextureParameter(FName(*FString::Printf(TEXT("Weightmap%d"), DataWeightmapIndex)), GEngine->WeightMapPlaceholderTexture), 
									Compiler->Add(
										Compiler->Mul( 
											Compiler->Floor( 
												Compiler->Mul(Compiler->Add( Compiler->TextureCoordinate(1, FALSE, FALSE), Compiler->Constant(-0.5f/DataWeightmapSize) ), Compiler->Constant((FLOAT)DataWeightmapSize) ) ),
										Compiler->Constant(1.f/(FLOAT)DataWeightmapSize) ),
									Compiler->Constant(0.5f/DataWeightmapSize) ) ),
							Compiler->VectorParameter(FName(*FString::Printf(TEXT("LayerMask_%s"), *ALandscape::DataWeightmapName.ToString())), FLinearColor::Black) ) );
			}
			return Compiler->Constant(1.0f);
		case MP_Distortion: return Material->Distortion.Compile(Compiler,FVector2D(0,0));
		case MP_TwoSidedLightingMask: return Compiler->Mul(Compiler->ForceCast(Material->TwoSidedLightingMask.Compile(Compiler,0.0f),MCT_Float),Material->TwoSidedLightingColor.Compile(Compiler,FColor(255,255,255)));
		case MP_DiffuseColor: 
			return Compiler->Mul(Compiler->ForceCast(Material->DiffuseColor.Compile(Compiler,FColor(0,0,0)),MCT_Float3),Compiler->Sub(Compiler->Constant(1.0f),SelectionColorIndex));
		case MP_DiffusePower:
			return Material->DiffusePower.Compile(Compiler,1.0f);
		case MP_SpecularColor: return Material->SpecularColor.Compile(Compiler,FColor(0,0,0));
		case MP_SpecularPower: return Material->SpecularPower.Compile(Compiler,15.0f);
		case MP_Normal: return Material->Normal.Compile(Compiler,FVector(0,0,1));
		case MP_CustomLighting: return Material->CustomLighting.Compile(Compiler,FColor(0,0,0));
		case MP_CustomLightingDiffuse: return Material->CustomSkylightDiffuse.Compile(Compiler,FColor(0,0,0));
		case MP_AnisotropicDirection: return Material->AnisotropicDirection.Compile(Compiler,FVector(0,1,0));
		case MP_WorldPositionOffset: return Material->WorldPositionOffset.Compile(Compiler,FVector(0,0,0));
		case MP_WorldDisplacement: return Material->WorldDisplacement.Compile(Compiler,FVector(0,0,0));
		case MP_TessellationMultiplier: return Material->TessellationMultiplier.Compile(Compiler,1.0f);
		case MP_SubsurfaceInscatteringColor: return Material->SubsurfaceInscatteringColor.Compile(Compiler,FColor(255,255,255));
		case MP_SubsurfaceAbsorptionColor: return Material->SubsurfaceAbsorptionColor.Compile(Compiler,FColor(230,200,200));
		case MP_SubsurfaceScatteringRadius: return Material->SubsurfaceScatteringRadius.Compile(Compiler,0.0f);
		default:
			return INDEX_NONE;
		};
	}

	/**
	 * Should the shader for this material with the given platform, shader type and vertex 
	 * factory type combination be compiled
	 *
	 * @param Platform		The platform currently being compiled for
	 * @param ShaderType	Which shader is being compiled
	 * @param VertexFactory	Which vertex factory is being compiled (can be NULL)
	 *
	 * @return TRUE if the shader should be compiled
	 */
	virtual UBOOL ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const
	{
		if( !LayerThumbnail && VertexFactoryType == FindVertexFactoryType(FName(TEXT("FLandscapeVertexFactory"), FNAME_Find)))
		{
			return TRUE;
		}

		// These are only needed for rendering thumbnails
		if(VertexFactoryType == FindVertexFactoryType(FName(TEXT("FLocalVertexFactory"), FNAME_Find)))
		{
			if(appStristr(ShaderType->GetName(), TEXT("BasePassVertexShaderFNoLightMapPolicyFNoDensityPolicy")) ||
				appStristr(ShaderType->GetName(), TEXT("BasePassHullShaderFNoLightMapPolicyFNoDensityPolicy")) ||
				appStristr(ShaderType->GetName(), TEXT("BasePassDomainShaderFNoLightMapPolicyFNoDensityPolicy")))
			{
				return TRUE;
			}
			else if(appStristr(ShaderType->GetName(), TEXT("BasePassPixelShaderFNoLightMapPolicy")))
			{
				return TRUE;
			}
			else if (appStristr(ShaderType->GetName(), TEXT("TLight")))
			{
				if (appStristr(ShaderType->GetName(), TEXT("FDirectionalLightPolicyFShadowTexturePolicy")) ||
					appStristr(ShaderType->GetName(), TEXT("FDirectionalLightPolicyFNoStaticShadowingPolicy")))
				{
					return TRUE;
				}
			}
			else 
			if( MaterialModifiesMeshPosition() && 
				(appStristr(ShaderType->GetName(), TEXT("TDepthOnlyVertexShader<0>")) ||
				 appStristr(ShaderType->GetName(), TEXT("FDepthOnlyHullShader")) ||
				 appStristr(ShaderType->GetName(), TEXT("FDepthOnlyDomainShader"))) )
			{
				return TRUE;
			}

		}

		return FALSE;
	}
};

/** ULandscapeMaterialInstanceConstant */

/**
* Custom version of AllocateResource to minimize the shaders we need to generate 
* @return	The allocated resource
*/
FMaterialResource* ULandscapeMaterialInstanceConstant::AllocateResource()
{
	return new FLandscapeMICResource(GetMaterial(), bIsLayerThumbnail, DataWeightmapIndex, DataWeightmapSize);
}

// Streaming Texture info
void ULandscapeComponent::GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const
{
	ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(GetOuter());
	FSphere BoundingSphere = Bounds.GetSphere();
	const FLOAT TexelFactor = 0.75f * Proxy->StreamingDistanceMultiplier * ComponentSizeQuads * Proxy->DrawScale * Proxy->DrawScale3D.X;

	// Normal usage...
	// Enumerate the textures used by the material.
	TArray<UTexture*> Textures;
	MaterialInstance->GetUsedTextures(Textures, MSQ_UNSPECIFIED, TRUE);

	// Add each texture to the output with the appropriate parameters.
	// TODO: Take into account which UVIndex is being used.
	for(INT TextureIndex = 0;TextureIndex < Textures.Num();TextureIndex++)
	{
		FStreamingTexturePrimitiveInfo& StreamingTexture = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
		StreamingTexture.Bounds = BoundingSphere;
		StreamingTexture.TexelFactor = TexelFactor;
		StreamingTexture.Texture = Textures(TextureIndex);
	}

	// Enumerate the material's textures.
	if (MaterialInstance)
	{
		UMaterial* Material = MaterialInstance->GetMaterial();
		if (Material)
		{
			INT NumExpressions = Material->Expressions.Num();
			for(INT ExpressionIndex = 0;ExpressionIndex < NumExpressions; ExpressionIndex++)
			{
				UMaterialExpression* Expression = Material->Expressions(ExpressionIndex);
				UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>(Expression);

				// TODO: This is only works for direct Coordinate Texture Sample cases
				if(TextureSample && TextureSample->Coordinates.Expression)
				{
					UMaterialExpressionTextureCoordinate* TextureCoordinate =
						Cast<UMaterialExpressionTextureCoordinate>( TextureSample->Coordinates.Expression );

					UMaterialExpressionTerrainLayerCoords* TerrainTextureCoordinate =
						Cast<UMaterialExpressionTerrainLayerCoords>( TextureSample->Coordinates.Expression );

					//FStreamingTexturePrimitiveInfo& StreamingTexture = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
					//StreamingTexture.Bounds = BoundingSphere;
					//StreamingTexture.Texture = TextureSample->Texture;
					//StreamingTexture.TexelFactor = TexelFactor;

					if (TextureCoordinate || TerrainTextureCoordinate)
					{
						for (INT i = 0; i < OutStreamingTextures.Num(); ++i)
						{
							FStreamingTexturePrimitiveInfo& StreamingTexture = OutStreamingTextures(i);
							if (StreamingTexture.Texture == TextureSample->Texture)
							{
								if ( TextureCoordinate )
								{
									StreamingTexture.TexelFactor = TexelFactor * Max(TextureCoordinate->UTiling, TextureCoordinate->VTiling);
								}
								else //if ( TerrainTextureCoordinate )
								{
									StreamingTexture.TexelFactor = TexelFactor * TerrainTextureCoordinate->MappingScale;
								}
								break;
							}
						}
					}
				}
			}
		}
	}

	// Weightmap
	for(INT TextureIndex = 0;TextureIndex < WeightmapTextures.Num();TextureIndex++)
	{
		FStreamingTexturePrimitiveInfo& StreamingWeightmap = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
		StreamingWeightmap.Bounds = BoundingSphere;
		StreamingWeightmap.TexelFactor = TexelFactor;
		StreamingWeightmap.Texture = WeightmapTextures(TextureIndex);
	}

	// Heightmap
	FStreamingTexturePrimitiveInfo& StreamingHeightmap = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
	StreamingHeightmap.Bounds = BoundingSphere;
	//StreamingHeightmap.Bounds.W += appSqrt(2.f * Square(0.5f*(FLOAT)SubsectionSizeQuads));
	StreamingHeightmap.TexelFactor = ForcedLOD >= 0 ? -13+ForcedLOD : TexelFactor; // Minus Value indicate ForcedLOD, 13 for 8k texture
		//2.f * Square((FLOAT)SubsectionSizeQuads * NumSubsections * Proxy->DrawScale * Proxy->DrawScale3D.X * LANDSCAPE_LOD_DISTANCE_FACTOR); 
	StreamingHeightmap.Texture = HeightmapTexture;

#if WITH_EDITOR
	if (GIsEditor && EditToolRenderData && EditToolRenderData->DataTexture)
	{
		FStreamingTexturePrimitiveInfo& StreamingDatamap = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
		StreamingDatamap.Bounds = BoundingSphere;
		StreamingDatamap.TexelFactor = TexelFactor;
		StreamingDatamap.Texture = EditToolRenderData->DataTexture;
	}
#endif
}

#if !PS3
void ALandscapeProxy::ChangeLODDistanceFactor(FLOAT InLODDistanceFactor)
{
	LODDistanceFactor = Clamp<FLOAT>(InLODDistanceFactor, 0.1f, 3.f);

	if (LandscapeComponents.Num())
	{
		INT CompNum = LandscapeComponents.Num();
		FLandscapeComponentSceneProxy** Proxies = new FLandscapeComponentSceneProxy*[CompNum];
		for (INT Idx = 0; Idx < CompNum; ++Idx)
		{
			Proxies[Idx] = (FLandscapeComponentSceneProxy*)(LandscapeComponents(Idx)->SceneInfo->Proxy);
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			LandscapeChangeLODDistanceFactorCommand,
			FLandscapeComponentSceneProxy**, Proxies, Proxies,
			INT, CompNum, CompNum,
			FVector2D, InLODDistanceFactors, FVector2D(appSqrt(2.f * Square((FLOAT)SubsectionSizeQuads)) * LANDSCAPE_LOD_DISTANCE_FACTOR / LODDistanceFactor, LODDistanceFactor * 0.33f),
		{
			for (INT Idx = 0; Idx < CompNum; ++Idx)
			{
				Proxies[Idx]->ChangeLODDistanceFactor_RenderThread(InLODDistanceFactors);
			}
			delete[] Proxies;
		}
		);
	}
};

void FLandscapeComponentSceneProxy::ChangeLODDistanceFactor_RenderThread(FVector2D InLODDistanceFactors)
{
	LODDistance = InLODDistanceFactors.X;
	LODDistanceFactor = InLODDistanceFactors.Y;
}
#endif