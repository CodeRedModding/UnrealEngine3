/*=============================================================================
	UnModelLight.cpp: Unreal model lighting.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnTextureLayout.h"
#include "UnModelLight.h"


//
//	Static vars
//

/** The new BSP elements that are made during lighting, and will be applied to the components when all lighting is done */
TMap<UModelComponent*, TIndirectArray<FModelElement> > UModelComponent::TempBSPElements;

//
//	Definitions.
//

#define SHADOWMAP_MAX_WIDTH			1024
#define SHADOWMAP_MAX_HEIGHT		1024

#define SHADOWMAP_TEXTURE_WIDTH		512
#define SHADOWMAP_TEXTURE_HEIGHT	512

#if (_MSC_VER || PLATFORM_MACOSX) && !CONSOLE && !UE3_LEAN_AND_MEAN
	/** Whether to allow cropping of unmapped borders in lightmaps and shadowmaps. Controlled by BaseLightmass.ini setting. */
	extern UBOOL GAllowLightmapCropping;
#endif

/** Sorts the BSP surfaces by descending static lighting texture size. */
struct FBSPSurfaceDescendingTextureSizeSort
{
	static inline INT Compare( const FBSPSurfaceStaticLighting* A, const FBSPSurfaceStaticLighting* B	)
	{
		return B->SizeX * B->SizeY - A->SizeX * A->SizeY;
	}
};

/**
 * Checks whether a sphere intersects a BSP node.
 * @param	Model - The BSP tree containing the node.
 * @param	NodeIndex - The index of the node in Model.
 * @param	Point - The origin of the sphere.
 * @param	Radius - The radius of the sphere.
 * @return	True if the sphere intersects the BSP node.
 */
static UBOOL SphereOnNode(UModel* Model,UINT NodeIndex,FVector Point,FLOAT Radius)
{
	FBspNode&	Node = Model->Nodes(NodeIndex);
	FBspSurf&	Surf = Model->Surfs(Node.iSurf);

	for(UINT VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
	{
		// Create plane perpendicular to both this side and the polygon's normal.
		FVector	Edge = Model->Points(Model->Verts(Node.iVertPool + VertexIndex).pVertex) - Model->Points(Model->Verts(Node.iVertPool + ((VertexIndex + Node.NumVertices - 1) % Node.NumVertices)).pVertex),
				EdgeNormal = Edge ^ (FVector)Surf.Plane;
		FLOAT	VertexDot = Node.Plane.PlaneDot(Model->Points(Model->Verts(Node.iVertPool + VertexIndex).pVertex));

		// Ignore degenerate edges.
		if(Edge.SizeSquared() < 2.0f*2.0f)
			continue;

		// If point is not behind all the planes created by this polys edges, it's outside the poly.
		if(FPointPlaneDist(Point,Model->Points(Model->Verts(Node.iVertPool + VertexIndex).pVertex),EdgeNormal.SafeNormal()) > Radius)
			return 0;
	}

	return 1;
}

FBSPSurfaceStaticLighting::FBSPSurfaceStaticLighting(
	const FNodeGroup* InNodeGroup,
	UModel* InModel,
	UModelComponent* InComponent,
	UBOOL bForceDirectLightmap
	):
	FStaticLightingTextureMapping(
		this, 
		InModel, 
		InNodeGroup->SizeX,
		InNodeGroup->SizeY,
		1,
		bForceDirectLightMap
	),
	FStaticLightingMesh(
		InNodeGroup->TriangleVertexIndices.Num() / 3,
		InNodeGroup->TriangleVertexIndices.Num() / 3,
		InNodeGroup->Vertices.Num(),
		InNodeGroup->Vertices.Num(),
		0,
		TRUE,
		FALSE,
		FALSE,
		InNodeGroup->RelevantLights,
		InComponent,
		InNodeGroup->BoundingBox, 
		InModel->LightingGuid
		),
	NodeGroup(InNodeGroup),
	bComplete(FALSE),
	LightMapData(NULL),
	QuantizedData(NULL),
	Model(InModel)
{}

FBSPSurfaceStaticLighting::~FBSPSurfaceStaticLighting()
{
	// Free the surface's static lighting data.
	ResetStaticLightingData();
}

void FBSPSurfaceStaticLighting::GetTriangle(INT TriangleIndex,FStaticLightingVertex& OutV0,FStaticLightingVertex& OutV1,FStaticLightingVertex& OutV2) const
{
	OutV0 = NodeGroup->Vertices(NodeGroup->TriangleVertexIndices(TriangleIndex * 3 + 0));
	OutV1 = NodeGroup->Vertices(NodeGroup->TriangleVertexIndices(TriangleIndex * 3 + 1));
	OutV2 = NodeGroup->Vertices(NodeGroup->TriangleVertexIndices(TriangleIndex * 3 + 2));
}

void FBSPSurfaceStaticLighting::GetTriangleIndices(INT TriangleIndex,INT& OutI0,INT& OutI1,INT& OutI2) const
{
	OutI0 = NodeGroup->TriangleVertexIndices(TriangleIndex * 3 + 0);
	OutI1 = NodeGroup->TriangleVertexIndices(TriangleIndex * 3 + 1);
	OutI2 = NodeGroup->TriangleVertexIndices(TriangleIndex * 3 + 2);
}

FLightRayIntersection FBSPSurfaceStaticLighting::IntersectLightRay(const FVector& Start,const FVector& End,UBOOL bFindNearestIntersection) const
{
	FCheckResult Result(1.0f);

	for(INT TriangleIndex = 0;TriangleIndex < NodeGroup->TriangleVertexIndices.Num();TriangleIndex += 3)
	{
		const INT I0 = NodeGroup->TriangleVertexIndices(TriangleIndex + 0);
		const INT I1 = NodeGroup->TriangleVertexIndices(TriangleIndex + 1);
		const INT I2 = NodeGroup->TriangleVertexIndices(TriangleIndex + 2);

		const FVector& V0 = NodeGroup->Vertices(I0).WorldPosition;
		const FVector& V1 = NodeGroup->Vertices(I1).WorldPosition;
		const FVector& V2 = NodeGroup->Vertices(I2).WorldPosition;

		if(LineCheckWithTriangle(Result,V2,V1,V0,Start,End,End - Start))
		{
			// Setup a vertex to represent the intersection.
			FStaticLightingVertex IntersectionVertex;
			IntersectionVertex.WorldPosition = Start + (End - Start) * Result.Time;
			IntersectionVertex.WorldTangentZ = Result.Normal;
			return FLightRayIntersection(TRUE,IntersectionVertex);
		}
	}

	return FLightRayIntersection(FALSE,FStaticLightingVertex());
}

void FBSPSurfaceStaticLighting::Apply(FLightMapData2D* InLightMapData,const TMap<ULightComponent*,FShadowMapData2D*>& InShadowMapData, FQuantizedLightmapData* InQuantizedData)
{
	if(bComplete)
	{
		// Free the surface's old static lighting data.
		ResetStaticLightingData();
	}
	else
	{
		// Update the number of surfaces with incomplete static lighting.
		Model->NumIncompleteNodeGroups--;
	}

	// Save the static lighting until all of the component's static lighting has been built.
	LightMapData = InLightMapData;
	ShadowMapData = InShadowMapData;
	QuantizedData = InQuantizedData;
	bComplete = TRUE;

	// If all the surfaces have complete static lighting, apply the component's static lighting.
	if(Model->NumIncompleteNodeGroups == 0)
	{
		Model->ApplyStaticLighting();
	}
}

#if WITH_EDITOR
UBOOL FBSPSurfaceStaticLighting::DebugThisMapping() const
{
	extern FSelectedLightmapSample GCurrentSelectedLightmapSample;
	const UBOOL bDebug = GCurrentSelectedLightmapSample.Component 
		&& GCurrentSelectedLightmapSample.NodeIndex >= 0
		&& NodeGroup->Nodes.ContainsItem(GCurrentSelectedLightmapSample.NodeIndex)
		// Only allow debugging if the lightmap resolution hasn't changed
		&& GCurrentSelectedLightmapSample.MappingSizeX == SizeX 
		&& GCurrentSelectedLightmapSample.MappingSizeY == SizeY;

	return bDebug;
}
#endif //WITH_EDITOR

void FBSPSurfaceStaticLighting::ResetStaticLightingData()
{
	// Free the light-map data.
	if(LightMapData)
	{
		delete LightMapData;
		LightMapData = NULL;
	}

	// Free the shadow-map data.
	for(TMap<ULightComponent*,FShadowMapData2D*>::TConstIterator ShadowMapDataIt(ShadowMapData);ShadowMapDataIt;++ShadowMapDataIt)
	{
		delete ShadowMapDataIt.Value();
	}
	ShadowMapData.Empty();
}

/**
 * Create a new temp FModelElement element for the component, which will be applied
 * when lighting is all done.
 *
 * @param Component The UModelComponent to make the temp element in
 */
FModelElement* UModelComponent::CreateNewTempElement(UModelComponent* Component)
{
	// make the array if needed
	TIndirectArray<FModelElement>* TempElements = TempBSPElements.Find(Component);
	if (TempElements == NULL)
	{
		TempElements = &UModelComponent::TempBSPElements.Set(Component, TIndirectArray<FModelElement>());
	}

	// make it in the temp array
	FModelElement* Element = new(*TempElements) FModelElement(Component, NULL);
	return Element;
}

/**
 * Apply all the elements that we were putting into the TempBSPElements map to
 * the Elements arrays in all components
 *
 * @param bLightingWasSuccessful If TRUE, the lighting should be applied, otherwise, the temp lighting should be cleaned up
 */
void UModelComponent::ApplyTempElements(UBOOL bLightingWasSuccessful)
{
	if (bLightingWasSuccessful)
	{
		TArray<UModel*> UpdatedModels;
		TArray<UModelComponent*> UpdatedComponents;

		// apply the temporary lighting elements to the real data
		for (TMap<UModelComponent*, TIndirectArray<FModelElement> >::TIterator It(TempBSPElements); It; ++It)
		{
			UModelComponent* Component = It.Key();
			TIndirectArray<FModelElement>& TempElements = It.Value();

			// replace the current elements with the ones in the temp array
			Component->Elements = TempElements;

			// make sure the element index for the nodes are correct
			for (INT ElementIndex = 0; ElementIndex < Component->Elements.Num(); ElementIndex++)
			{
				FModelElement& Element = Component->Elements(ElementIndex);
				for (INT NodeIndex = 0; NodeIndex < Element.Nodes.Num(); NodeIndex++)
				{
					FBspNode& Node = Component->Model->Nodes(Element.Nodes(NodeIndex));
					Node.ComponentElementIndex = ElementIndex;
				}
			}
			// cache the model/component for updating below
			UpdatedModels.AddUniqueItem(Component->Model);
			UpdatedComponents.AddUniqueItem(Component);
		}

		// Detach all of the components that are being modified (they will be reattached at the end of the scope)
		TIndirectArray<FPrimitiveSceneAttachmentContext> ComponentContexts;
		for (INT ComponentIndex = 0; ComponentIndex < UpdatedComponents.Num(); ComponentIndex++)
		{
			UModelComponent* Component = UpdatedComponents(ComponentIndex);

			new(ComponentContexts) FPrimitiveSceneAttachmentContext(Component);
		}

		// Release all index buffers since they will be modified by BuildRenderData()
		for (INT ModelIndex = 0; ModelIndex < UpdatedModels.Num(); ModelIndex++)
		{
			UModel* Model = UpdatedModels(ModelIndex);
			for(TMap<UMaterialInterface*,TScopedPointer<FRawIndexBuffer16or32> >::TIterator IndexBufferIt(Model->MaterialIndexBuffers); IndexBufferIt; ++IndexBufferIt)
			{
				BeginReleaseResource(IndexBufferIt.Value());
			}
		}

		// Block until the index buffers have been released
		FlushRenderingCommands();

		// Rebuild rendering data for each modified component
		for (INT ComponentIndex = 0; ComponentIndex < UpdatedComponents.Num(); ComponentIndex++)
		{
			UModelComponent* Component = UpdatedComponents(ComponentIndex);

			// Build the render data for the new elements.
			Component->BuildRenderData();
		}

		// Initialize all models' index buffers.
		for (INT ModelIndex = 0; ModelIndex < UpdatedModels.Num(); ModelIndex++)
		{
			UModel* Model = UpdatedModels(ModelIndex);
			for(TMap<UMaterialInterface*,TScopedPointer<FRawIndexBuffer16or32> >::TIterator IndexBufferIt(Model->MaterialIndexBuffers); IndexBufferIt; ++IndexBufferIt)
			{
				BeginInitResource(IndexBufferIt.Value());
			}

			// Mark the model's package as dirty.
			Model->MarkPackageDirty();
		}

		// After this line, the elements in the ComponentContexts array will be destructed, causing components to reattach.
	}

	// the temp lighting is no longer of any use, so clear it out
	TempBSPElements.Empty();
}



void UModelComponent::GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo,const TArray<ULightComponent*>& InRelevantLights,const FLightingBuildOptions& Options)
{
	check(0);
}

/**
 * Checks if a lightmap texel is mapped or not.
 *
 * @param MappingData	Array of lightmap texels
 * @param X				X-coordinate for the texel to check
 * @param Y				Y-coordinate for the texel to check
 * @param Pitch			Number of texels per row
 * @return				TRUE if the texel is mapped
 */
FORCEINLINE UBOOL IsTexelMapped( const FLightMapData2D& MappingData, INT X, INT Y, INT Pitch )
{
	const FLightSample& Sample = MappingData(X, Y);
	return Sample.bIsMapped;
}

#if !CONSOLE && !UE3_LEAN_AND_MEAN
/** A group of BSP surfaces which have the same static lighting relevance. */
class FSurfaceStaticLightingGroup
{
public:

	/** Information about a grouped surface. */
	struct FSurfaceInfo
	{
		FBSPSurfaceStaticLighting* SurfaceStaticLighting;
		UINT BaseX;
		UINT BaseY;
	};

	/** The surfaces in the group. */
	TArray<FSurfaceInfo> Surfaces;

	/** The shadow-mapped lights affecting the group. */
	TArray<ULightComponent*> ShadowMappedLights;

	/** The signed distance field shadow-mapped lights affecting the group. */
	TArray<ULightComponent*> SignedDistanceFieldShadowMappedLights;

	/** The approximate brightness of the group's light-map. */
	const INT BrightnessGroup;

	/** The layout of the group's static lighting texture. */
	FTextureLayout TextureLayout;

	/**
	 * Minimal initialization constructor.
	 */
	FSurfaceStaticLightingGroup(UINT InSizeX,UINT InSizeY,INT InBrightnessGroup):
		BrightnessGroup(InBrightnessGroup),
		TextureLayout(1,1,InSizeX,InSizeY)
	{}

	/**
	 * Attempts to add a surface to the group.  It may fail if the surface doesn't match the group or won't fit in the group's texture.
	 * @param SurfaceStaticLighting - The static lighting for the surface to add.
	 * @return TRUE if the surface was successfully added.
	 */
	UBOOL AddSurface(FBSPSurfaceStaticLighting* SurfaceStaticLighting)
	{
		// Check that the surface's relevant shadow-mapped lights match the group.
		if(ShadowMappedLights.Num() + SignedDistanceFieldShadowMappedLights.Num() != SurfaceStaticLighting->ShadowMapData.Num())
		{
			return FALSE;
		}
		UBOOL bShadowMappedLightsMatch = TRUE;
		for(INT LightIndex = 0;LightIndex < ShadowMappedLights.Num();LightIndex++)
		{
			if(SurfaceStaticLighting->ShadowMapData.Find(ShadowMappedLights(LightIndex)) == NULL)
			{
				bShadowMappedLightsMatch = FALSE;
				break;
			}
		}
		for(INT LightIndex = 0;LightIndex < SignedDistanceFieldShadowMappedLights.Num();LightIndex++)
		{
			if(SurfaceStaticLighting->ShadowMapData.Find(SignedDistanceFieldShadowMappedLights(LightIndex)) == NULL)
			{
				bShadowMappedLightsMatch = FALSE;
				break;
			}
		}
		if(!bShadowMappedLightsMatch)
		{
			return FALSE;
		}

 		if ( GAllowLightmapCropping && SurfaceStaticLighting->LightMapData )
		{
			CropUnmappedTexels( *SurfaceStaticLighting->LightMapData, SurfaceStaticLighting->SizeX, SurfaceStaticLighting->SizeY, SurfaceStaticLighting->MappedRect );
		}
 		else if ( GAllowLightmapCropping && SurfaceStaticLighting->QuantizedData )
		{
			CropUnmappedTexels( SurfaceStaticLighting->QuantizedData->Data, SurfaceStaticLighting->SizeX, SurfaceStaticLighting->SizeY, SurfaceStaticLighting->MappedRect );
		}
		else
		{
			SurfaceStaticLighting->MappedRect = FIntRect( 0, 0, SurfaceStaticLighting->SizeX, SurfaceStaticLighting->SizeY );
		}

		// Attempt to add the surface to the group's texture.
		UINT PaddedSurfaceBaseX = 0;
		UINT PaddedSurfaceBaseY = 0;
		if(TextureLayout.AddElement(PaddedSurfaceBaseX,PaddedSurfaceBaseY,SurfaceStaticLighting->MappedRect.Width(),SurfaceStaticLighting->MappedRect.Height()))
		{
			// The surface fits in the group's texture, add it to the group's surface list.
			FSurfaceInfo* SurfaceInfo = new(Surfaces) FSurfaceInfo;
			SurfaceInfo->SurfaceStaticLighting = SurfaceStaticLighting;

			SurfaceInfo->BaseX = PaddedSurfaceBaseX;
			SurfaceInfo->BaseY = PaddedSurfaceBaseY;

			return TRUE;
		}
		else
		{
			// The surface didn't fit in the group's texture, return failure.
			return FALSE;
		}
	}

	/**
	 * Converts the passed in max light brightness to a light classification group index.
	 * @param	MaxLightMapBrightness	Brightness to convert
	 * @return	Brightness classification
	 */
	static INT ConvertMaxLightBrightnessToClassification( FLOAT MaxLightMapBrightness )
	{
		if(MaxLightMapBrightness > DELTA)
		{
			const FLOAT BrightnessScale = GWorld->GetWorldInfo()->bMinimizeBSPSections ? .25f : 1.0f;
			return appRoundUpToPowerOfTwo(appCeil(MaxLightMapBrightness * BrightnessScale));
		}
		else
		{
			return 0;
		}
	}
};
#endif //!CONSOLE && !UE3_LEAN_AND_MEAN

void UModelComponent::InvalidateLightingCache()
{
	UBOOL bHasStaticLightingData = FALSE;
	for(INT ElementIndex = 0; ElementIndex < Elements.Num() && !bHasStaticLightingData; ElementIndex++)
	{
		FModelElement& Element = Elements(ElementIndex);
		bHasStaticLightingData = bHasStaticLightingData || Element.ShadowMaps.Num() > 0;
		bHasStaticLightingData = bHasStaticLightingData || Element.IrrelevantLights.Num() > 0;
		bHasStaticLightingData = bHasStaticLightingData || Element.LightMap != NULL;
	}
	if(bHasStaticLightingData)
	{
		// Save the model state for transactions.
		Modify();

		// Mark lighting as requiring a rebuilt.
		MarkLightingRequiringRebuild();

		FComponentReattachContext ReattachContext(this);
		Super::InvalidateLightingCache();

		// Clear all statically cached shadowing data.
		for(INT ElementIndex = 0;ElementIndex < Elements.Num();ElementIndex++)
		{
			FModelElement& Element = Elements(ElementIndex);
			Element.ShadowMaps.Empty();
			Element.IrrelevantLights.Empty();
			Element.LightMap = NULL;
		}
	}
}

/** Calculates the lightmap resolution to be used by the given surface. */
void UModelComponent::GetSurfaceLightMapResolution( INT SurfaceIndex, INT QualityScale, INT& Width, INT& Height, FMatrix& WorldToMap, TArray<INT>* GatheredNodes ) const
{
	FBspSurf& Surf = Model->Surfs(SurfaceIndex);

	// Find a plane parallel to the surface.
	FVector MapX;
	FVector MapY;
	Surf.Plane.FindBestAxisVectors(MapX,MapY);

	// Find the surface's nodes and the part of the plane they map to.
	UBOOL bFoundNode = FALSE;
	FVector2D MinUV(WORLD_MAX,WORLD_MAX);
	FVector2D MaxUV(-WORLD_MAX,-WORLD_MAX);

	// if the nodes weren't already gathered, then find the ones in this component
	for(INT NodeIndex = 0; NodeIndex < (GatheredNodes ? GatheredNodes->Num() : Nodes.Num()); NodeIndex++)
	{
		FBspNode& Node = Model->Nodes(GatheredNodes ? (*GatheredNodes)(NodeIndex) : Nodes(NodeIndex));

		// if they are already gathered, don't check the surface index
		if (GatheredNodes || Node.iSurf == SurfaceIndex)
		{
			// Compute the bounds of the node's vertices on the surface plane.
			for(UINT VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
			{
				bFoundNode = TRUE;

				FVector	Position = Model->Points(Model->Verts(Node.iVertPool + VertexIndex).pVertex);
				FLOAT	X = MapX | Position,
					Y = MapY | Position;
				MinUV.X = Min(X,MinUV.X);
				MinUV.Y = Min(Y,MinUV.Y);
				MaxUV.X = Max(X,MaxUV.X);
				MaxUV.Y = Max(Y,MaxUV.Y);
			}
		}
	}

	if (bFoundNode)
	{
		FLOAT Scale = Surf.ShadowMapScale * QualityScale;
		MinUV.X = (FLOAT)appFloor(MinUV.X / Scale) * Scale;
		MinUV.Y = (FLOAT)appFloor(MinUV.Y / Scale) * Scale;
		MaxUV.X = (FLOAT)appCeil(MaxUV.X / Scale) * Scale;
		MaxUV.Y = (FLOAT)appCeil(MaxUV.Y / Scale) * Scale;

		Width = Clamp(appCeil((MaxUV.X - MinUV.X) / (Surf.ShadowMapScale * QualityScale)),4,SHADOWMAP_MAX_WIDTH);
		Height = Clamp(appCeil((MaxUV.Y - MinUV.Y) / (Surf.ShadowMapScale * QualityScale)),4,SHADOWMAP_MAX_HEIGHT);
		WorldToMap = FMatrix(
			FPlane(MapX.X / (MaxUV.X - MinUV.X),	MapY.X / (MaxUV.Y - MinUV.Y),	Surf.Plane.X,	0),
			FPlane(MapX.Y / (MaxUV.X - MinUV.X),	MapY.Y / (MaxUV.Y - MinUV.Y),	Surf.Plane.Y,	0),
			FPlane(MapX.Z / (MaxUV.X - MinUV.X),	MapY.Z / (MaxUV.Y - MinUV.Y),	Surf.Plane.Z,	0),
			FPlane(-MinUV.X / (MaxUV.X - MinUV.X),	-MinUV.Y / (MaxUV.Y - MinUV.Y),	-Surf.Plane.W,	1)
			);
	}
	else
	{
		Width = 0;
		Height = 0;
		WorldToMap = FMatrix::Identity;
	}
}

/**
 * Returns the lightmap resolution used for this primivite instnace in the case of it supporting texture light/ shadow maps.
 * 0 if not supported or no static shadowing.
 *
 * @param	Width	[out]	Width of light/shadow map
 * @param	Height	[out]	Height of light/shadow map
 *
 * @return	UBOOL			TRUE if LightMap values are padded, FALSE if not
 */
UBOOL UModelComponent::GetLightMapResolution( INT& Width, INT& Height ) const
{
	INT LightMapArea = 0;
	for(INT SurfaceIndex = 0;SurfaceIndex < Model->Surfs.Num();SurfaceIndex++)
	{
		INT SizeX;
		INT SizeY;
		FMatrix WorldToMap;
		GetSurfaceLightMapResolution(SurfaceIndex, 1, SizeX, SizeY, WorldToMap);
		LightMapArea += SizeX * SizeY;
	}

	Width = appTrunc( appSqrt( LightMapArea ) );
	Height = Width;
	return FALSE;
}

/**
 *	Returns the static lightmap resolution used for this primitive.
 *	0 if not supported or no static shadowing.
 *
 * @return	INT		The StaticLightmapResolution for the component
 */
INT UModelComponent::GetStaticLightMapResolution() const
{
	INT Width;
	INT Height;
	GetLightMapResolution(Width, Height);

	return Max<INT>(Width, Height);
}

/**
 * Returns the light and shadow map memory for this primite in its out variables.
 *
 * Shadow map memory usage is per light whereof lightmap data is independent of number of lights, assuming at least one.
 *
 * @param [out] LightMapMemoryUsage		Memory usage in bytes for light map (either texel or vertex) data
 * @param [out]	ShadowMapMemoryUsage	Memory usage in bytes for shadow map (either texel or vertex) data
 */
void UModelComponent::GetLightAndShadowMapMemoryUsage( INT& LightMapMemoryUsage, INT& ShadowMapMemoryUsage ) const
{
	INT LightMapWidth	= 0;
	INT LightMapHeight	= 0;
	GetLightMapResolution( LightMapWidth, LightMapHeight );
	
	// Stored in texture.
	const FLOAT MIP_FACTOR = 1.33f;
	ShadowMapMemoryUsage	= appTrunc( MIP_FACTOR * LightMapWidth * LightMapHeight ); // G8
	const UINT NumLightMapCoefficients = GSystemSettings.bAllowDirectionalLightMaps ? NUM_DIRECTIONAL_LIGHTMAP_COEF : NUM_SIMPLE_LIGHTMAP_COEF;
	LightMapMemoryUsage		= appTrunc( NumLightMapCoefficients * MIP_FACTOR * LightMapWidth * LightMapHeight / 2 ); // DXT1
	return;
}



/**
 * Groups all nodes in the model into NodeGroups (cached in the NodeGroups object)
 *
 * @param Level The level for this model
 * @param Lights The possible lights that will be cached in the NodeGroups
 */
void UModel::GroupAllNodes(const ULevel* Level, const TArray<ULightComponent*>& Lights)
{
#if !CONSOLE && !UE3_LEAN_AND_MEAN
	// cache the level
	LightingLevel = Level;

	// gather all the lights for each component
	TMap<INT, TArray<ULightComponent*> > ComponentRelevantLights;
	for (INT ComponentIndex = 0; ComponentIndex < Level->ModelComponents.Num(); ComponentIndex++)
	{
		// create a list of lights for the component
		TArray<ULightComponent*>* RelevantLights = &ComponentRelevantLights.Set(ComponentIndex, TArray<ULightComponent*>());

		// Find the lights relevant to the component, and add them to the list of lights for this component
		for (INT LightIndex = 0; LightIndex < Lights.Num(); LightIndex++)
		{
			ULightComponent* Light = Lights(LightIndex);
			// Only add enabled lights and lights that can potentially be enabled at runtime (toggleable)
			if ((Light->bEnabled || !Light->UseDirectLightMap) && Light->AffectsPrimitive(Level->ModelComponents(ComponentIndex), TRUE))
			{
				RelevantLights->AddItem(Light);
			}
		}
	}

	// make sure the NodeGroups is empty
	for (TMap<INT, FNodeGroup*>::TIterator It(NodeGroups); It; ++It)
	{
		delete It.Value();
	}
	NodeGroups.Empty();

	// caches the nodegroups used by each node
	TArray<FNodeGroup*> ParentNodes;
	ParentNodes.Empty(Nodes.Num());
	ParentNodes.AddZeroed(Nodes.Num());

	// We request this value potentially many times, at what appears to be
	// a high cost (even though the routine is trivial), so cache it first
	const INT ModelComponentCount = Level->ModelComponents.Num();
	TArray<UBOOL> HasStaticShadowingCache(ModelComponentCount);
	for (INT ComponentIndex = 0; ComponentIndex < ModelComponentCount; ComponentIndex++)
	{
		HasStaticShadowingCache(ComponentIndex) = Level->ModelComponents(ComponentIndex)->HasStaticShadowing();
	}

	// N^2 node loop to find nodes that are coplanar and same lightmap resolution, then 
	// for each matching one, an N^2 vertex loop looking for shared vertices
	for (INT NodeIndex1 = 0; NodeIndex1 < Nodes.Num(); NodeIndex1++)
	{
		FBspNode& Node1 = Nodes(NodeIndex1);
		if (Node1.NumVertices == 0)
		{
			continue;
		}
		FBspSurf& Surf1 = Surfs(Node1.iSurf);
		UModelComponent* Comp1 = Level->ModelComponents(Node1.ComponentIndex);

		// if this node's component has no shadowing, never use this node (as seen in UModelComponent::GetStaticLightingInfo)
		if (!HasStaticShadowingCache(Node1.ComponentIndex))
		{
			continue;
		}

		FLightmassPrimitiveSettings Surf1LightmassSettings = LightmassSettings(Surf1.iLightmassIndex);

		// look through rest of nodes looking for "child" nodes
		for (INT NodeIndex2 = NodeIndex1 + 1; NodeIndex2 < Nodes.Num(); NodeIndex2++)
		{
			// if I've already been parented, I don't need to reparent
			if (ParentNodes(NodeIndex1) != NULL && ParentNodes(NodeIndex2) != NULL && ParentNodes(NodeIndex1) == ParentNodes(NodeIndex2))
			{
				continue;
			}

			FBspNode& Node2 = Nodes(NodeIndex2);
			if (Node2.NumVertices == 0)
			{
				continue;
			}
			FBspSurf& Surf2 = Surfs(Node2.iSurf);
			UModelComponent* Comp2 = Level->ModelComponents(Node2.ComponentIndex);

			// if this node's component has no shadowing, never use this node (as seen in UModelComponent::GetStaticLightingInfo)
			if (!HasStaticShadowingCache(Node2.ComponentIndex))
			{
				continue;
			}

			// if the components don't have matching bAcceptLights, they will not be grouped
 			if (Comp1->bAcceptsLights != Comp2->bAcceptsLights)
 			{
 				continue;
 			}

			FLightmassPrimitiveSettings Surf2LightmassSettings = LightmassSettings(Surf2.iLightmassIndex);

			// variable to see check if the 2 nodes are conodes
			UBOOL bNodesAreConodes = FALSE;

			// if we have a tolerance, then join based on coplanar adjacency
			if (GLightmassDebugOptions.bGatherBSPSurfacesAcrossComponents)
			{
				// are these two nodes conodes?
				if (Surf1.ShadowMapScale == Surf2.ShadowMapScale)
				{
					if (Comp1->bForceDirectLightMap == Comp2->bForceDirectLightMap)
					{
						if (Surf1LightmassSettings == Surf2LightmassSettings)	// Do they have the same Lightmass settings?
						{
							if (/*(Node1.iSurf == Node2.iSurf) ||*/ 
								Surf1.Plane.Equals(Surf2.Plane, GLightmassDebugOptions.CoplanarTolerance)
								)
							{
								// they are coplanar, have the same lightmap res and Lightmass settings, 
								// now we need to check for adjacency which we check for by looking for a shared vertex

								UBOOL bFoundMatchingVert = FALSE;
								FVert* VertPool1 = &Verts(Node1.iVertPool);
								for (INT A=0; A < Node1.NumVertices && !bNodesAreConodes; A++)
								{
									FVert* VertPool2 = &Verts(Node2.iVertPool);
									for (INT B=0; B < Node2.NumVertices && !bNodesAreConodes; B++)
									{
										// if they share a vertex location, they are adjacent (this won't detect adjacency via T-joints)
										if (VertPool1->pVertex == VertPool2->pVertex)
										{
											bNodesAreConodes = TRUE;
										}
										VertPool2++;
									}
									VertPool1++;
								}
							}
						}
					}
				}
			}
			// if coplanar tolerance is < 0, then we join nodes together based on being in the same ModelComponent
			// and from the same surface
			else
			{
				if (Node1.iSurf == Node2.iSurf && Node1.ComponentIndex == Node2.ComponentIndex)
				{
					bNodesAreConodes = TRUE;
				}
			}


			// are Node1 and Node2 conodes - if so, join into a group
			if (bNodesAreConodes)
			{
				// okay, these two nodes are conodes, so we need to stick them together into some pot of nodes
				// look to see if either one are already in a group
				FNodeGroup* NodeGroup = NULL;
				// if both are already in different groups, we need to combine the groups
				if (ParentNodes(NodeIndex1) != NULL && ParentNodes(NodeIndex2) != NULL)
				{
					NodeGroup = ParentNodes(NodeIndex1);

					// merge 2 into 1
					FNodeGroup* NodeGroup2 = ParentNodes(NodeIndex2);
					for (INT NodeIndex = 0; NodeIndex < NodeGroup2->Nodes.Num(); NodeIndex++)
					{
						NodeGroup->Nodes.AddItem(NodeGroup2->Nodes(NodeIndex));
					}
					for (INT LightIndex = 0; LightIndex < NodeGroup2->RelevantLights.Num(); LightIndex++)
					{
						NodeGroup->RelevantLights.AddUniqueItem(NodeGroup2->RelevantLights(LightIndex));
					}

					// replace all the users of NodeGroup2 with NodeGroup
					for (INT GroupIndex = 0; GroupIndex < ParentNodes.Num(); GroupIndex++)
					{
						if (ParentNodes(GroupIndex) == NodeGroup2)
						{
							ParentNodes(GroupIndex) = NodeGroup;
						}
					}

					// the key for the nodegroup is the 0th node (could just be a set now)
					NodeGroups.Remove(NodeGroup2->Nodes(0));

					// free the now useless nodegroup
					delete NodeGroup2;
				}
				else if (ParentNodes(NodeIndex1) != NULL)
				{
					NodeGroup = ParentNodes(NodeIndex1);
				}
				else if (ParentNodes(NodeIndex2) != NULL)
				{
					NodeGroup = ParentNodes(NodeIndex2);
				}
				// otherwise, make a new group and put them both in it
				else
				{
					NodeGroup = NodeGroups.Set(NodeIndex1, new FNodeGroup());

					// cache the force direct lightmap from the a node's component (they are all the same, as per conode condition)
					NodeGroup->bForceDirectLightmap = Level->ModelComponents(Nodes(NodeIndex1).ComponentIndex)->bForceDirectLightMap;
				}

				// apply both these nodes to the NodeGroup
				for (INT WhichNode = 0; WhichNode < 2; WhichNode++)
				{
					// operator on each node in this loop
					INT NodeIndex = WhichNode ? NodeIndex2 : NodeIndex1;

					// track what group the node went into
					ParentNodes(NodeIndex) = NodeGroup;

					// is this node already not yet in the group
					if (NodeGroup->Nodes.FindItemIndex(NodeIndex) == INDEX_NONE)
					{
						// add it to the group
						NodeGroup->Nodes.AddItem(NodeIndex);

						// add the relevant lights to the nodegroup
						TArray<ULightComponent*>* RelevantLights = ComponentRelevantLights.Find(Nodes(NodeIndex).ComponentIndex);
						check(RelevantLights);
						for (INT LightIndex = 0; LightIndex < RelevantLights->Num(); LightIndex++)
						{
							NodeGroup->RelevantLights.AddUniqueItem((*RelevantLights)(LightIndex));
						}
					}
				}
			}
		}
	}

	// make a node group for any ungrouped nodes (entries would only be made above if conodes were found)
	for (INT NodeIndex = 0; NodeIndex < ParentNodes.Num(); NodeIndex++)
	{
		if (ParentNodes(NodeIndex) != NULL)
		{
			continue;
		}

		FNodeGroup* NodeGroup = NodeGroups.Set(NodeIndex, new FNodeGroup());

		// cache the force direct lightmap from the a node's component (they are all the same, as per conode condition)
		NodeGroup->bForceDirectLightmap = Level->ModelComponents(Nodes(NodeIndex).ComponentIndex)->bForceDirectLightMap;

		// is this node already not yet in the group
		if (NodeGroup->Nodes.FindItemIndex(NodeIndex) == INDEX_NONE)
		{
			// add it to the group
			NodeGroup->Nodes.AddItem(NodeIndex);

			// add the relevant lights to the nodegroup
			TArray<ULightComponent*>* RelevantLights = ComponentRelevantLights.Find(Nodes(NodeIndex).ComponentIndex);
			check(RelevantLights);
			for (INT LightIndex = 0; LightIndex < RelevantLights->Num(); LightIndex++)
			{
				NodeGroup->RelevantLights.AddUniqueItem((*RelevantLights)(LightIndex));
			}
		}
	}
#endif	//#if !CONSOLE && !UE3_LEAN_AND_MEAN
}

/**
 * Applies all of the finished lighting cached in the NodeGroups 
 */
void UModel::ApplyStaticLighting()
{
#if !CONSOLE && !UE3_LEAN_AND_MEAN
	// if the first surface has quantized data, that means that all should, and so we can use all quantized data
	UBOOL bUseQuantizedData = CachedMappings(0)->QuantizedData != NULL;

	// Group surfaces based on their static lighting relevance.
	TArray<FSurfaceStaticLightingGroup> SurfaceGroups;
	for (INT MappingIndex = 0; MappingIndex < CachedMappings.Num(); MappingIndex++)
	{
		FBSPSurfaceStaticLighting* SurfaceStaticLighting = CachedMappings(MappingIndex);

		// Calculate the surface light-map's maximum brightness 
		FLOAT MaxLightMapBrightness = 0.0f;

		if (bUseQuantizedData)
		{
			// the Scale in the quantized data is already the max brightness for that coefficient for all quantized values
			for(INT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
			{
				for(INT ColorIndex = 0;ColorIndex < 3;ColorIndex++)
				{
					MaxLightMapBrightness = Max(MaxLightMapBrightness,SurfaceStaticLighting->QuantizedData->Scale[CoefficientIndex][ColorIndex]);
				}
			}
		}
		else
		{
			// look over each non-quantized value, finding the max
			for(INT Y = 0;Y < SurfaceStaticLighting->SizeY;Y++)
			{
				for(INT X = 0;X < SurfaceStaticLighting->SizeX;X++)
				{
					const FLightSample& LightMapSample = (*SurfaceStaticLighting->LightMapData)(X,Y);
					if(LightMapSample.bIsMapped)
					{
						for(INT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
						{
							for(INT ColorIndex = 0;ColorIndex < 3;ColorIndex++)
							{
								MaxLightMapBrightness = Max(MaxLightMapBrightness,LightMapSample.Coefficients[CoefficientIndex][ColorIndex]);
							}
						}
					}
				}
			}
		}

		// Calculate the light-map's brightness group.
		INT BrightnessGroup = FSurfaceStaticLightingGroup::ConvertMaxLightBrightnessToClassification(MaxLightMapBrightness);

		// Find an existing surface group with the same static lighting relevance.
		FSurfaceStaticLightingGroup* Group = NULL;
		for(INT GroupIndex = 0;GroupIndex < SurfaceGroups.Num();GroupIndex++)
		{
			FSurfaceStaticLightingGroup& ExistingGroup = SurfaceGroups(GroupIndex);

			// Check that the surface's light-map brightness approximately matches the group.
			// This improves light-map precision by not merging dim light-maps with bright light-maps.
			if(BrightnessGroup == ExistingGroup.BrightnessGroup)
			{
				// Attempt to add the surface to the group.
				if(ExistingGroup.AddSurface(SurfaceStaticLighting))
				{
					Group = &ExistingGroup;
					break;
				}
			}
		}

		// If the surface didn't fit in any existing group, create a new group.
		if(!Group)
		{
			// If the surface is larger than the standard group texture size, create a special group with the texture the same size as the surface.
			UINT TextureSizeX = SHADOWMAP_TEXTURE_WIDTH;
			UINT TextureSizeY = SHADOWMAP_TEXTURE_HEIGHT;
			if(SurfaceStaticLighting->SizeX > SHADOWMAP_TEXTURE_WIDTH || SurfaceStaticLighting->SizeY > SHADOWMAP_TEXTURE_HEIGHT)
			{
				TextureSizeX = (SurfaceStaticLighting->SizeX + 3) & ~3;
				TextureSizeY = (SurfaceStaticLighting->SizeY + 3) & ~3;
			}

			// Create the new group.
			Group = ::new(SurfaceGroups) FSurfaceStaticLightingGroup(TextureSizeX,TextureSizeY,BrightnessGroup);

			// Initialize the group's light lists from the surface.
			for(TMap<ULightComponent*,FShadowMapData2D*>::TConstIterator ShadowMapDataIt(SurfaceStaticLighting->ShadowMapData);ShadowMapDataIt;++ShadowMapDataIt)
			{
				switch (ShadowMapDataIt.Value()->GetType())
				{
					case FShadowMapData2D::SHADOW_FACTOR_DATA:
					case FShadowMapData2D::SHADOW_FACTOR_DATA_QUANTIZED:
						Group->ShadowMappedLights.AddItem(ShadowMapDataIt.Key());
					break;
					case FShadowMapData2D::SHADOW_SIGNED_DISTANCE_FIELD_DATA:
					case FShadowMapData2D::SHADOW_SIGNED_DISTANCE_FIELD_DATA_QUANTIZED:
						Group->SignedDistanceFieldShadowMappedLights.AddItem(ShadowMapDataIt.Key());
					break;
				}
			}

			// Add the surface to the new group.
			verify(Group->AddSurface(SurfaceStaticLighting));
		}
	}

	for (INT ComponentIndex = 0; ComponentIndex < LightingLevel->ModelComponents.Num(); ComponentIndex++)
	{
		UModelComponent* Component = LightingLevel->ModelComponents(ComponentIndex);
		FLOAT AccumulatedPreviewEnvironmentShadowFactors = 0.0f;
		INT NumPreviewEnvironmentShadowFactors = 0;
		for(INT GroupIndex = 0;GroupIndex < SurfaceGroups.Num();GroupIndex++)
		{
			const FSurfaceStaticLightingGroup& SurfaceGroup = SurfaceGroups(GroupIndex);
			for(INT SurfaceIndex = 0;SurfaceIndex < SurfaceGroup.Surfaces.Num();SurfaceIndex++)
			{
				const FSurfaceStaticLightingGroup::FSurfaceInfo& SurfaceInfo = SurfaceGroup.Surfaces(SurfaceIndex);
				const FBSPSurfaceStaticLighting* SurfaceStaticLighting = SurfaceInfo.SurfaceStaticLighting;

				if (SurfaceStaticLighting->QuantizedData)
				{
					UBOOL bSurfaceAffectsComponent = FALSE;
					// gather all the components that contributed to this mapping
					for(INT NodeIndex = 0;NodeIndex < SurfaceStaticLighting->NodeGroup->Nodes.Num();NodeIndex++)
					{
						const FBspNode& Node = Nodes(SurfaceStaticLighting->NodeGroup->Nodes(NodeIndex));
						if (Node.ComponentIndex == ComponentIndex)
						{
							bSurfaceAffectsComponent = TRUE;
							break;
						}
					}
					if (bSurfaceAffectsComponent)
					{
						AccumulatedPreviewEnvironmentShadowFactors += Square(SurfaceStaticLighting->QuantizedData->PreviewEnvironmentShadowing / 255.0f);
						NumPreviewEnvironmentShadowFactors++;
					}
				}
			}
		}
		const FLOAT AverageShadowFactor = NumPreviewEnvironmentShadowFactors ? AccumulatedPreviewEnvironmentShadowFactors / NumPreviewEnvironmentShadowFactors : .5f;
		Component->PreviewEnvironmentShadowing = (BYTE)Clamp<INT>(appTrunc(appSqrt(AverageShadowFactor) * 255.0f), 0, 255);
	}

	// Create an element for each surface group.
	for(INT GroupIndex = 0;GroupIndex < SurfaceGroups.Num();GroupIndex++)
	{
		const FSurfaceStaticLightingGroup& SurfaceGroup = SurfaceGroups(GroupIndex);
		const UINT GroupSizeX = SurfaceGroup.TextureLayout.GetSizeX();
		const UINT GroupSizeY = SurfaceGroup.TextureLayout.GetSizeY();

		FLightMapData2D* GroupLightMapData = NULL;
		FQuantizedLightmapData* GroupQuantizedData = NULL;

		// build a group quantized data
		if (bUseQuantizedData)
		{
			// initialize new quantized data for the entire group
			GroupQuantizedData = new FQuantizedLightmapData;
			GroupQuantizedData->SizeX = GroupSizeX;
			GroupQuantizedData->SizeY = GroupSizeY;
			GroupQuantizedData->Data.Empty(GroupSizeX * GroupSizeY);
			GroupQuantizedData->Data.AddZeroed(GroupSizeX * GroupSizeY);

			// calculate the new scale for all of the surfaces
			appMemzero(GroupQuantizedData->Scale, sizeof(GroupQuantizedData->Scale));
			for(INT SurfaceIndex = 0;SurfaceIndex < SurfaceGroup.Surfaces.Num();SurfaceIndex++)
			{
				const FBSPSurfaceStaticLighting* SurfaceStaticLighting = SurfaceGroup.Surfaces(SurfaceIndex).SurfaceStaticLighting;

				for(INT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
				{
					for(INT ColorIndex = 0;ColorIndex < 3;ColorIndex++)
					{
						GroupQuantizedData->Scale[CoefficientIndex][ColorIndex] = 
							Max(GroupQuantizedData->Scale[CoefficientIndex][ColorIndex],
							SurfaceStaticLighting->QuantizedData->Scale[CoefficientIndex][ColorIndex]);
					}
				}
			}

			// calculate the inverse scale
			FLOAT InvCoefficientScale[NUM_STORED_LIGHTMAP_COEF][3];
			for (INT CoefficientIndex = 0; CoefficientIndex < NUM_STORED_LIGHTMAP_COEF; CoefficientIndex++)
			{
				for (INT ColorIndex = 0; ColorIndex < 3; ColorIndex++)
				{
					InvCoefficientScale[CoefficientIndex][ColorIndex] = 1.0f / Max<FLOAT>(GroupQuantizedData->Scale[CoefficientIndex][ColorIndex], DELTA);
				}
			}


			// now gather all surfaces together, requantizing using the new Scale above
			for(INT SurfaceIndex = 0;SurfaceIndex < SurfaceGroup.Surfaces.Num();SurfaceIndex++)
			{
				const FSurfaceStaticLightingGroup::FSurfaceInfo& SurfaceInfo = SurfaceGroup.Surfaces(SurfaceIndex);
				const FBSPSurfaceStaticLighting* SurfaceStaticLighting = SurfaceInfo.SurfaceStaticLighting;

				// Copy the surface's light-map into the merged group light-map.
				for(INT Y = SurfaceStaticLighting->MappedRect.Min.Y; Y < SurfaceStaticLighting->MappedRect.Max.Y; Y++)
				{
					for(INT X = SurfaceStaticLighting->MappedRect.Min.X; X < SurfaceStaticLighting->MappedRect.Max.X; X++)
					{
						// get source from input, dest from the rectangular offset in the group
						FLightMapCoefficients& SourceSample = SurfaceStaticLighting->QuantizedData->Data(Y * SurfaceStaticLighting->SizeX + X);
						FLightMapCoefficients& DestSample = GroupQuantizedData->Data((SurfaceInfo.BaseY + Y - SurfaceStaticLighting->MappedRect.Min.Y) * GroupSizeX + (SurfaceInfo.BaseX + X - SurfaceStaticLighting->MappedRect.Min.X));

						// coverage doesn't change
						DestSample.Coverage = SourceSample.Coverage;

						// go over each coefficient and dequantize and requantize with new Scale
						for(INT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
						{
							for(INT ColorIndex = 0;ColorIndex < 3;ColorIndex++)
							{
								// dequantize it
								FLOAT Dequantized = (FLOAT)SourceSample.Coefficients[CoefficientIndex][ColorIndex] / 255.0f;
								Dequantized = appPow(Dequantized, 2.2f) * SurfaceStaticLighting->QuantizedData->Scale[CoefficientIndex][ColorIndex];

								// requantize it
								DestSample.Coefficients[CoefficientIndex][ColorIndex] = (BYTE)Clamp<INT>(
									appTrunc(
										appPow(
											Dequantized * InvCoefficientScale[CoefficientIndex][ColorIndex],
											1.0f / 2.2f
											) * 255.0f
										),
									0,
									255);
							}
						}
					}
				}

				// the QuantizedData is expected that AllocateLightMap would take ownership, but since its using a group one, we need to free it
				delete SurfaceStaticLighting->QuantizedData;
			}

		}
		// build a group lightmap data
		else
		{
			// Create merged light-map data.
			GroupLightMapData = new FLightMapData2D(GroupSizeX,GroupSizeY);
			for(INT SurfaceIndex = 0;SurfaceIndex < SurfaceGroup.Surfaces.Num();SurfaceIndex++)
			{
				const FSurfaceStaticLightingGroup::FSurfaceInfo& SurfaceInfo = SurfaceGroup.Surfaces(SurfaceIndex);
				const FBSPSurfaceStaticLighting* SurfaceStaticLighting = SurfaceInfo.SurfaceStaticLighting;

				// Merge the surface's light-mapped light list into the group's light-mapped light list.
				for(INT LightIndex = 0;LightIndex < SurfaceStaticLighting->LightMapData->LightGuids.Num();LightIndex++)
				{
					GroupLightMapData->LightGuids.AddUniqueItem(SurfaceStaticLighting->LightMapData->LightGuids(LightIndex));
				}

				// Copy the surface's light-map into the merged group light-map.
				for(INT Y = SurfaceStaticLighting->MappedRect.Min.Y; Y < SurfaceStaticLighting->MappedRect.Max.Y; Y++)
				{
					for(INT X = SurfaceStaticLighting->MappedRect.Min.X; X < SurfaceStaticLighting->MappedRect.Max.X; X++)
					{
						const FLightSample& SourceSample = (*SurfaceStaticLighting->LightMapData)(X,Y);
						FLightSample& DestSample = (*GroupLightMapData)(SurfaceInfo.BaseX + X - SurfaceStaticLighting->MappedRect.Min.X,SurfaceInfo.BaseY + Y - SurfaceStaticLighting->MappedRect.Min.Y);
						DestSample = SourceSample;
					}
				}
			}
		}

		// Calculate the bounds for the lightmap group.
		FBox GroupBox(0);
		for(INT SurfaceIndex = 0;SurfaceIndex < SurfaceGroup.Surfaces.Num();SurfaceIndex++)
		{
			const FSurfaceStaticLightingGroup::FSurfaceInfo& SurfaceInfo = SurfaceGroup.Surfaces(SurfaceIndex);
			const FBSPSurfaceStaticLighting* SurfaceStaticLighting = SurfaceInfo.SurfaceStaticLighting;
			GroupBox += SurfaceStaticLighting->BoundingBox;
		}
		FBoxSphereBounds GroupLightmapBounds( GroupBox );

		// create the grouped together lightmap, which is used by all elements.
		ELightMapPaddingType PaddingType = GAllowLightmapPadding ? LMPT_PrePadding : LMPT_NoPadding;

		FLightMap2D* LightMap = NULL;
		const UBOOL bHasNonZeroData = GroupLightMapData != NULL || GroupQuantizedData != NULL && GroupQuantizedData->HasNonZeroData();
		if (bHasNonZeroData)
		{
			LightMap = FLightMap2D::AllocateLightMap(this, GroupLightMapData, GroupQuantizedData, NULL, GroupLightmapBounds, PaddingType, LMF_None);
		}

		// Free the merged raw light-map data.
		delete GroupLightMapData;
		GroupLightMapData = NULL;

		// Allocate merged shadow-map data.
		TMap<ULightComponent*,FShadowMapData2D*> GroupShadowMapData;
		for(INT LightIndex = 0;LightIndex < SurfaceGroup.ShadowMappedLights.Num();LightIndex++)
		{
			GroupShadowMapData.Set(
				SurfaceGroup.ShadowMappedLights(LightIndex),
				new FQuantizedShadowFactorData2D(GroupSizeX,GroupSizeY)
				);
		}
		for(INT LightIndex = 0;LightIndex < SurfaceGroup.SignedDistanceFieldShadowMappedLights.Num();LightIndex++)
		{
			GroupShadowMapData.Set(
				SurfaceGroup.SignedDistanceFieldShadowMappedLights(LightIndex),
				new FQuantizedShadowSignedDistanceFieldData2D(GroupSizeX,GroupSizeY)
				);
		}

		// Merge surface shadow-maps into the group shadow-maps.
		for(INT SurfaceIndex = 0;SurfaceIndex < SurfaceGroup.Surfaces.Num();SurfaceIndex++)
		{
			const FSurfaceStaticLightingGroup::FSurfaceInfo& SurfaceInfo = SurfaceGroup.Surfaces(SurfaceIndex);
			const FBSPSurfaceStaticLighting* SurfaceStaticLighting = SurfaceInfo.SurfaceStaticLighting;

			for(TMap<ULightComponent*,FShadowMapData2D*>::TConstIterator ShadowMapDataIt(SurfaceStaticLighting->ShadowMapData);ShadowMapDataIt;++ShadowMapDataIt)
			{
				FShadowMapData2D* GroupShadowMap = GroupShadowMapData.FindRef(ShadowMapDataIt.Key());
				check(GroupShadowMap);

				FShadowMapData2D* SurfaceShadowMap = ShadowMapDataIt.Value();
				const FShadowMapData2D::ShadowMapDataType SurfaceShadowMapType = SurfaceShadowMap->GetType();
				switch (SurfaceShadowMapType)
				{
					case FShadowMapData2D::SHADOW_FACTOR_DATA:
					case FShadowMapData2D::SHADOW_FACTOR_DATA_QUANTIZED:
					{
						check(GroupShadowMap->GetType() == FShadowMapData2D::SHADOW_FACTOR_DATA_QUANTIZED);
						FQuantizedShadowFactorData2D* GroupShadowFactorData = (FQuantizedShadowFactorData2D*)GroupShadowMap;

						// If the data is already quantized, this will just copy the data
						TArray<FQuantizedShadowSample> QuantizedData;
						SurfaceShadowMap->Quantize( QuantizedData );

						// Copy the surface's shadow-map into the merged group shadow-map.
						for(INT Y = SurfaceStaticLighting->MappedRect.Min.Y; Y < SurfaceStaticLighting->MappedRect.Max.Y; Y++)
						{
							for(INT X = SurfaceStaticLighting->MappedRect.Min.X; X < SurfaceStaticLighting->MappedRect.Max.X; X++)
							{
								const FQuantizedShadowSample& SourceSample = QuantizedData(Y * SurfaceStaticLighting->SizeX + X);
								FQuantizedShadowSample& DestSample = (*GroupShadowFactorData)(SurfaceInfo.BaseX + X - SurfaceStaticLighting->MappedRect.Min.X, SurfaceInfo.BaseY + Y - SurfaceStaticLighting->MappedRect.Min.Y);
								DestSample = SourceSample;
							}
						}
					}
					break;

					case FShadowMapData2D::SHADOW_SIGNED_DISTANCE_FIELD_DATA:
					case FShadowMapData2D::SHADOW_SIGNED_DISTANCE_FIELD_DATA_QUANTIZED:
					{
						check(GroupShadowMap->GetType() == FShadowMapData2D::SHADOW_SIGNED_DISTANCE_FIELD_DATA_QUANTIZED);
						FQuantizedShadowSignedDistanceFieldData2D* GroupShadowFactorData = (FQuantizedShadowSignedDistanceFieldData2D*)GroupShadowMap;

						// If the data is already quantized, this will just copy the data
						TArray<FQuantizedSignedDistanceFieldShadowSample> QuantizedData;
						SurfaceShadowMap->Quantize( QuantizedData );

						// Copy the surface's shadow-map into the merged group shadow-map.
						for(INT Y = SurfaceStaticLighting->MappedRect.Min.Y; Y < SurfaceStaticLighting->MappedRect.Max.Y; Y++)
						{
							for(INT X = SurfaceStaticLighting->MappedRect.Min.X; X < SurfaceStaticLighting->MappedRect.Max.X; X++)
							{
								const FQuantizedSignedDistanceFieldShadowSample& SourceSample = QuantizedData(Y * SurfaceStaticLighting->SizeX + X);
								FQuantizedSignedDistanceFieldShadowSample& DestSample = (*GroupShadowFactorData)(SurfaceInfo.BaseX + X - SurfaceStaticLighting->MappedRect.Min.X, SurfaceInfo.BaseY + Y - SurfaceStaticLighting->MappedRect.Min.Y);
								DestSample = SourceSample;
							}
						}
					}
					break;
				}
			}
		}

// 		//@TODO:  Calculate the bounds for the shadowmap.
// 		FBox GroupBox(0);
// 		for(INT SurfaceIndex = 0;SurfaceIndex < SurfaceGroup.Surfaces.Num();SurfaceIndex++)
// 		{
// 			const FSurfaceStaticLightingGroup::FSurfaceInfo& SurfaceInfo = SurfaceGroup.Surfaces(SurfaceIndex);
// 			const FBSPSurfaceStaticLighting* SurfaceStaticLighting = SurfaceInfo.SurfaceStaticLighting;
// 			GroupBox += SurfaceStaticLighting->BoundingBox;
// 		}
// 		FBoxSphereBounds GroupLightmapBounds( GroupBox );

		// Create the shadow-maps, which is used by all elements.
		TArray<UShadowMap2D*> ShadowMaps;
		for(TMap<ULightComponent*,FShadowMapData2D*>::TConstIterator ShadowMapDataIt(GroupShadowMapData);ShadowMapDataIt;++ShadowMapDataIt)
		{
			ShadowMaps.AddItem(new(this) UShadowMap2D(
				this,
				*ShadowMapDataIt.Value(),
				ShadowMapDataIt.Key()->LightGuid,
				NULL,
				Bounds,
				PaddingType,
				SMF_None
				));
		}

		// Apply the surface's static lighting mapping to its vertices.
		for(INT SurfaceIndex = 0;SurfaceIndex < SurfaceGroup.Surfaces.Num();SurfaceIndex++)
		{
			const FSurfaceStaticLightingGroup::FSurfaceInfo& SurfaceInfo = SurfaceGroup.Surfaces(SurfaceIndex);
			const FBSPSurfaceStaticLighting* SurfaceStaticLighting = SurfaceInfo.SurfaceStaticLighting;

			for(INT NodeIndex = 0;NodeIndex < SurfaceStaticLighting->NodeGroup->Nodes.Num();NodeIndex++)
			{
				const FBspNode& Node = Nodes(SurfaceStaticLighting->NodeGroup->Nodes(NodeIndex));
				for(INT VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
				{
					FVert& Vert = Verts(Node.iVertPool + VertexIndex);
					const FVector& WorldPosition = Points(Vert.pVertex);
					const FVector4 StaticLightingTextureCoordinate = SurfaceStaticLighting->NodeGroup->WorldToMap.TransformFVector(WorldPosition);

					UINT PaddedSizeX = SurfaceStaticLighting->SizeX;
					UINT PaddedSizeY = SurfaceStaticLighting->SizeY;
					UINT BaseX = SurfaceInfo.BaseX - SurfaceStaticLighting->MappedRect.Min.X;
					UINT BaseY = SurfaceInfo.BaseY - SurfaceStaticLighting->MappedRect.Min.Y;
					if (GLightmassDebugOptions.bPadMappings && GAllowLightmapPadding)
					{
						if ((PaddedSizeX - 2 > 0) && ((PaddedSizeY - 2) > 0))
						{
							PaddedSizeX -= 2;
							PaddedSizeY -= 2;
							BaseX += 1;
							BaseY += 1;
						}
					}

					Vert.ShadowTexCoord.X = (BaseX + StaticLightingTextureCoordinate.X * PaddedSizeX) / (FLOAT)GroupSizeX;
					Vert.ShadowTexCoord.Y = (BaseY + StaticLightingTextureCoordinate.Y * PaddedSizeY) / (FLOAT)GroupSizeY;
				}
			}
		}

		// we need to go back to the source components and use this lightmap
		TArray<UModelComponent*> Components;
		for(INT SurfaceIndex = 0;SurfaceIndex < SurfaceGroup.Surfaces.Num();SurfaceIndex++)
		{
			const FSurfaceStaticLightingGroup::FSurfaceInfo& SurfaceInfo = SurfaceGroup.Surfaces(SurfaceIndex);
			const FBSPSurfaceStaticLighting* SurfaceStaticLighting = SurfaceInfo.SurfaceStaticLighting;

			// gather all the components that contributed to this mapping
			for(INT NodeIndex = 0;NodeIndex < SurfaceStaticLighting->NodeGroup->Nodes.Num();NodeIndex++)
			{
				const FBspNode& Node = Nodes(SurfaceStaticLighting->NodeGroup->Nodes(NodeIndex));
				Components.AddUniqueItem(LightingLevel->ModelComponents(Node.ComponentIndex));
			}
		}

		// use this lightmap in all of the components that contributed to it
		for (INT ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++)
		{
			UModelComponent* Component = Components(ComponentIndex);

			// Create an element for the surface group.
			FModelElement* Element = UModelComponent::CreateNewTempElement(Component);
			
			// Create the element's light-map and shadow-map.
			Element->LightMap = LightMap;
			Element->ShadowMaps = ShadowMaps;

 			TSet<FGuid> TempIrrelevantLights;
			for(INT SurfaceIndex = 0;SurfaceIndex < SurfaceGroup.Surfaces.Num();SurfaceIndex++)
			{
				const FSurfaceStaticLightingGroup::FSurfaceInfo& SurfaceInfo = SurfaceGroup.Surfaces(SurfaceIndex);
				const FBSPSurfaceStaticLighting* SurfaceStaticLighting = SurfaceInfo.SurfaceStaticLighting;

				// Build the list of the element's statically irrelevant lights.
				for(INT LightIndex = 0;LightIndex < SurfaceStaticLighting->NodeGroup->RelevantLights.Num();LightIndex++)
				{
					ULightComponent* Light = SurfaceStaticLighting->NodeGroup->RelevantLights(LightIndex);

					// Check if the light is stored in the light-map.
					const UBOOL bIsInLightMap = Element->LightMap && Element->LightMap->LightGuids.ContainsItem(Light->LightmapGuid);
					if (!bIsInLightMap)
					{
						// Check if the light is stored in the shadow-map.
						const UBOOL bIsInShadowMap = GroupShadowMapData.Find(Light) != NULL;

						// Add the light to the statically irrelevant light list if it is in the potentially relevant light list, but didn't contribute to the light-map or a shadow-map.
						if (!bIsInShadowMap)
						{
							TempIrrelevantLights.Add(Light->LightGuid);
						}
					}
				}

				// Add the surfaces' nodes to the element.
				for(INT NodeIndex = 0;NodeIndex < SurfaceStaticLighting->NodeGroup->Nodes.Num();NodeIndex++)
				{
					const INT ModelNodeIndex = SurfaceStaticLighting->NodeGroup->Nodes(NodeIndex);
					// Only add nodes from the node group that belong to this component
					if (Nodes(ModelNodeIndex).ComponentIndex == Component->ComponentIndex)
					{
						Element->Nodes.AddItem(SurfaceStaticLighting->NodeGroup->Nodes(NodeIndex));
					}
				}
			}

			// Move the data from the set into the array
 			for(TSet<FGuid>::TIterator It(TempIrrelevantLights); It; ++It)
 			{
 				Element->IrrelevantLights.AddItem(*It);
 			}
		}

		for(TMap<ULightComponent*,FShadowMapData2D*>::TConstIterator ShadowMapDataIt(GroupShadowMapData);ShadowMapDataIt;++ShadowMapDataIt)
		{
			// Free the merged raw shadow-map data.
			delete ShadowMapDataIt.Value();
		}
	}

	// Free the surfaces' static lighting data.
	for(INT SurfaceIndex = 0;SurfaceIndex < CachedMappings.Num();SurfaceIndex++)
	{
		CachedMappings(SurfaceIndex)->ResetStaticLightingData();
	}
	CachedMappings.Empty();

	// clear the node groups
	for (TMap<INT, FNodeGroup*>::TIterator It(NodeGroups); It; ++It)
	{
		delete It.Value();
	}
	NodeGroups.Empty();

	// Invalidate the model's vertex buffer.
	InvalidSurfaces = TRUE;
#endif	//#if !CONSOLE && !UE3_LEAN_AND_MEAN
}
