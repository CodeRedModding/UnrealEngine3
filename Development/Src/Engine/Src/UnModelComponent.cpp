/*=============================================================================
	UnModelComponent.cpp: Model component implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UModelComponent);

FModelElement::FModelElement(UModelComponent* InComponent,UMaterialInterface* InMaterial):
	Component(InComponent),
	Material(InMaterial)
{}

/**
 * Serializer.
 */
FArchive& operator<<(FArchive& Ar,FModelElement& Element)
{
	Ar << Element.LightMap;
	Ar << *(UObject**)&Element.Component << *(UObject**)&Element.Material << Element.Nodes;
	Ar << Element.ShadowMaps;		
	Ar << Element.IrrelevantLights;
	return Ar;
}

UModelComponent::UModelComponent(UModel* InModel,INT InZoneIndex,WORD InComponentIndex,DWORD MaskedSurfaceFlags,DWORD InLightingChannels,const TArray<WORD>& InNodes):
	Model(InModel),
	ZoneIndex(InZoneIndex),
	ComponentIndex(InComponentIndex),
	Nodes(InNodes)
{
	// Model components are transacted.
	SetFlags( RF_Transactional );

	// Propagate model surface flags to component.
	bForceDirectLightMap	= (MaskedSurfaceFlags & PF_ForceLightMap)			? TRUE : FALSE;
	bAcceptsLights			= (MaskedSurfaceFlags & PF_AcceptsLights)			? TRUE : FALSE;
	bAcceptsDynamicLights	= (MaskedSurfaceFlags & PF_AcceptsDynamicLights)	? TRUE : FALSE;

	// Set the model component's lighting channels.
	LightingChannels.Bitfield = InLightingChannels;

	GenerateElements(TRUE);
}

/**
 * Callback used to allow object register its direct object references that are not already covered by
 * the token stream.
 *
 * @param ObjectArray	array to add referenced objects to via AddReferencedObject
 */
void UModelComponent::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	Super::AddReferencedObjects( ObjectArray );
	for( INT ElementIndex=0; ElementIndex<Elements.Num(); ElementIndex++ )
	{
		const FModelElement& Element = Elements(ElementIndex);
		AddReferencedObject( ObjectArray, Element.Component );
		AddReferencedObject( ObjectArray, Element.Material );
		for( INT ShadowMapIndex=0; ShadowMapIndex<Element.ShadowMaps.Num(); ShadowMapIndex++ )
		{
			AddReferencedObject( ObjectArray, Element.ShadowMaps(ShadowMapIndex) );
		}
		if(Element.LightMap != NULL)
		{
			Element.LightMap->AddReferencedObjects(ObjectArray);
		}
	}
}

void UModelComponent::CommitSurfaces()
{
	TArray<INT> InvalidElements;

    // Find nodes that are from surfaces which have been invalidated.
	TMap<WORD,FModelElement*> InvalidNodes;
	for(INT ElementIndex = 0;ElementIndex < Elements.Num();ElementIndex++)
	{
		FModelElement& Element = Elements(ElementIndex);
		TArray<WORD> NewNodes;
		for(INT NodeIndex = 0;NodeIndex < Element.Nodes.Num();NodeIndex++)
		{
			FBspNode& Node = Model->Nodes(Element.Nodes(NodeIndex));
			FBspSurf& Surf = Model->Surfs(Node.iSurf);
			if(Surf.Material != Element.Material)
			{
				// This node's material changed, remove it from the element and put it on the invalid node list.
				InvalidNodes.Set(Element.Nodes(NodeIndex),&Element);

				// Mark the node's original element as being invalid.
				InvalidElements.AddUniqueItem(ElementIndex);
			}
			else
			{
				NewNodes.AddItem(Element.Nodes(NodeIndex));
			}
		}
		Element.Nodes = NewNodes;
	}

	// Reassign the invalid nodes to appropriate mesh elements.
	for(TMap<WORD,FModelElement*>::TConstIterator It(InvalidNodes);It;++It)
	{
		FBspNode& Node = Model->Nodes(It.Key());
		FBspSurf& Surf = Model->Surfs(Node.iSurf);
		FModelElement* OldElement = It.Value();

		// Find an element which has the same material and lights as the invalid node.
		FModelElement* NewElement = NULL;
		for(INT ElementIndex = 0;ElementIndex < Elements.Num();ElementIndex++)
		{
			FModelElement& Element = Elements(ElementIndex);
			if(Element.Material != Surf.Material)
				continue;
			if(Element.ShadowMaps != OldElement->ShadowMaps)
				continue;
			if(Element.LightMap != OldElement->LightMap)
				continue;
			if(Element.IrrelevantLights != OldElement->IrrelevantLights)
				continue;
			// This element's material and lights match the node.
			NewElement = &Element;
		}

		// If no matching element was found, create a new element.
		if(!NewElement)
		{
			NewElement = new(Elements) FModelElement(this,Surf.Material);
			NewElement->ShadowMaps = OldElement->ShadowMaps;
			NewElement->LightMap = OldElement->LightMap;
			NewElement->IrrelevantLights = OldElement->IrrelevantLights;
		}

		NewElement->Nodes.AddItem(It.Key());
		InvalidElements.AddUniqueItem(Elements.Num() - 1);
	}

	// Rebuild the render data for the elements which have changed.
	BuildRenderData();

	ShrinkElements();
}

void UModelComponent::ShrinkElements()
{
	// Find elements which have no nodes remaining.
	for(INT ElementIndex = 0;ElementIndex < Elements.Num();ElementIndex++)
	{
		FModelElement& Element = Elements(ElementIndex);
		if(!Element.Nodes.Num())
		{
			// This element contains no nodes, remove it.
			Elements.Remove(ElementIndex--);
		}
	}
}

void UModelComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << Model;
	Ar << ZoneIndex << Elements;
	Ar << ComponentIndex << Nodes;
}

void UModelComponent::PostLoad()
{
	Super::PostLoad();

	// Fix for old model components which weren't created with transactional flag.
	SetFlags( RF_Transactional );

	// BuildRenderData relies on the model having been post-loaded, so we ensure this by calling ConditionalPostLoad.
	check(Model);
	Model->ConditionalPostLoad();

	if (!GIsUCC)
	{
		BuildRenderData();
	}
}

void UModelComponent::PostEditUndo()
{
	// Rebuild the component's render data after applying a transaction to it.
	if(GWorld)
	{
		GWorld->InvalidateModelSurface(TRUE);
		GWorld->CommitModelSurfaces();
	}
	else
	{
		BuildRenderData();
	}
	Super::PostEditUndo();
}

/**
* @return		Sum of the size of textures referenced by this material.
*/
INT UModelComponent::GetResourceSize()
{
	if (GExclusiveResourceSizeMode)
	{
		return 0;
	}
	else
	{
		FArchiveCountMem CountBytesSize(this);
		INT ResourceSize = CountBytesSize.GetNum();
		return ResourceSize;
	}
}

/** 
 * Retrieves the materials used in this component 
 * 
 * @param OutMaterials	The list of used materials.
 */
void UModelComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const
{
	for( INT ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex )
	{
		const FModelElement& Element = Elements( ElementIndex );
		if( Element.Material )
		{
			OutMaterials.AddItem( Element.Material );
		}
	}
}

/**
 * Selects all surfaces that are part of this model component.
 */
void UModelComponent::SelectAllSurfaces()
{
	check(Model);
	for( INT NodeIndex=0; NodeIndex<Nodes.Num(); NodeIndex++ )
	{
		FBspNode& Node = Model->Nodes(Nodes(NodeIndex));
		FBspSurf& Surf = Model->Surfs(Node.iSurf);
		Model->ModifySurf( Node.iSurf, 0 );
		Surf.PolyFlags |= PF_Selected;
	}
}

void UModelComponent::GetStaticTriangles(FPrimitiveTriangleDefinitionInterface* PTDI) const
{
	for(INT IndirectNodeIndex = 0;IndirectNodeIndex < Nodes.Num();IndirectNodeIndex++)
	{
		const INT NodeIndex = Nodes(IndirectNodeIndex);
		const FBspNode& Node = Model->Nodes(NodeIndex);
		const FBspSurf& Surf = Model->Surfs(Node.iSurf);

		// Set up the node's vertices.
		TArray<FPrimitiveTriangleVertex> Vertices;
		Vertices.Empty(Node.NumVertices);
		for(INT VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
		{
			const FVert& Vert = Model->Verts(Node.iVertPool + VertexIndex);
			FPrimitiveTriangleVertex* Vertex = new(Vertices) FPrimitiveTriangleVertex();

			Vertex->WorldPosition = Model->Points(Vert.pVertex);

			// Use the texture coordinates and normal to create an orthonormal tangent basis.
			Vertex->WorldTangentX = Model->Vectors(Surf.vTextureU);
			Vertex->WorldTangentY = Model->Vectors(Surf.vTextureV);
			Vertex->WorldTangentZ = Model->Vectors(Surf.vNormal);
			CreateOrthonormalBasis(Vertex->WorldTangentX,Vertex->WorldTangentY,Vertex->WorldTangentZ);
		}

		// Triangulate the BSP node.
		for(INT LeadingVertexIndex = 2;LeadingVertexIndex < Vertices.Num();LeadingVertexIndex++)
		{
			PTDI->DefineTriangle(
				Vertices(0),
				Vertices(LeadingVertexIndex - 1),
				Vertices(LeadingVertexIndex)
				);
		}
	}
}

void UModelComponent::GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const
{
	if( Model )
	{
		// Build a map from surface indices to node indices for nodes in this component.
		TMultiMap<INT,INT> SurfToNodeMap;
		for(INT NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
		{
			const FBspNode& Node = Model->Nodes(Nodes(NodeIndex));
			SurfToNodeMap.Add(Node.iSurf,Nodes(NodeIndex));
		}

		TArray<INT> SurfaceNodes;
		TArray<FVector> SurfaceVertices;
		for(INT SurfaceIndex = 0;SurfaceIndex < Model->Surfs.Num();SurfaceIndex++)
		{
			// Check if the surface has nodes in this component.
			SurfaceNodes.Reset();
			SurfToNodeMap.MultiFind(SurfaceIndex,SurfaceNodes);
			if(SurfaceNodes.Num())
			{
				const FBspSurf& Surf = Model->Surfs(SurfaceIndex);

				// Compute a bounding sphere for the surface's nodes.
				SurfaceVertices.Reset();
				for(INT NodeIndex = 0;NodeIndex < SurfaceNodes.Num();NodeIndex++)
				{
					const FBspNode& Node = Model->Nodes(SurfaceNodes(NodeIndex));
					for(INT VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
					{
						const FVector WorldVertex = LocalToWorld.TransformFVector(Model->Points(Model->Verts(Node.iVertPool + VertexIndex).pVertex));
						SurfaceVertices.AddItem(WorldVertex);
					}
				}
				const FSphere SurfaceBoundingSphere(&SurfaceVertices(0),SurfaceVertices.Num());

				// Compute the surface's texture scaling factor.
				// WRH - 2007/08/31 - This 128 value should be made a global to be used by all BspTexel calculations
				const FLOAT BspTexelsPerNormalizedTexel = 128.0f; // Bsp texel coordinates are in 0..128 range
				const FLOAT WorldUnitsPerBspTexel = Max( Model->Vectors(Surf.vTextureU).Size(), Model->Vectors(Surf.vTextureV).Size() );
				const FLOAT TexelFactor = BspTexelsPerNormalizedTexel / WorldUnitsPerBspTexel;

				// Determine the material applied to the surface.
				UMaterialInterface* Material = Surf.Material;
				if(!Material)
				{
					Material = GEngine->DefaultMaterial;
				}

				// Enumerate the textures used by the surface's material.
				TArray<UTexture*> Textures;
				
				Material->GetUsedTextures(Textures, MSQ_UNSPECIFIED, TRUE);

				// Add each texture to the output with the appropriate parameters.
				for(INT TextureIndex = 0;TextureIndex < Textures.Num();TextureIndex++)
				{
					FStreamingTexturePrimitiveInfo& StreamingTexture = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
					StreamingTexture.Bounds = SurfaceBoundingSphere;
					StreamingTexture.TexelFactor = TexelFactor;
					StreamingTexture.Texture = Textures(TextureIndex);
				}
			}
		}
	}
}

/**
 *	Generate the Elements array.
 *
 *	@param	bBuildRenderData	If TRUE, build render data after generating the elements.
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not.
 */
UBOOL UModelComponent::GenerateElements(UBOOL bBuildRenderData)
{
	Elements.Empty();
	if (GIsEditor == FALSE)
	{
		TMap<UMaterialInterface*,FModelElement*> MaterialToElementMap;

		// Find the BSP nodes which are in this component's zone.
		for(INT NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
		{
			FBspNode& Node = Model->Nodes(Nodes(NodeIndex));
			FBspSurf& Surf = Model->Surfs(Node.iSurf);

			// Find an element with the same material as this node.
			FModelElement* Element = MaterialToElementMap.FindRef(Surf.Material);
			if(!Element)
			{
				// If there's no matching element, create a new element.
				Element = MaterialToElementMap.Set(Surf.Material,new(Elements) FModelElement(this,Surf.Material));
			}

			// Add the node to the element.
			Element->Nodes.AddItem(Nodes(NodeIndex));
		}
	}
	else
	{
		typedef TMap<UMaterialInterface*,FModelElement*> TMaterialElementMap;
		typedef TMap<DWORD,TMaterialElementMap*> TResolutionToMaterialElementMap;
		TMap<FNodeGroup*,TResolutionToMaterialElementMap*> NodeGroupToResolutionToMaterialElementMap;

		// Find the BSP nodes which are in this component's zone.
		for (INT NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
		{
			FBspNode& Node = Model->Nodes(Nodes(NodeIndex));
			FBspSurf& Surf = Model->Surfs(Node.iSurf);

			// Find the node group it belong to
			FNodeGroup* FoundNodeGroup = NULL;
			// find the NodeGroup that this node went into, and get all of its node
			for (TMap<INT, FNodeGroup*>::TIterator It(Model->NodeGroups); It && (FoundNodeGroup == NULL); ++It)
			{
				FNodeGroup* NodeGroup = It.Value();
				for (INT InnerNodeIdx = 0; InnerNodeIdx < NodeGroup->Nodes.Num(); InnerNodeIdx++)
				{
					if (NodeGroup->Nodes(InnerNodeIdx) == Nodes(NodeIndex))
					{
						FoundNodeGroup = NodeGroup;
						break;
					}
				}
			}

			TResolutionToMaterialElementMap* ResToMaterialElementMap = NodeGroupToResolutionToMaterialElementMap.FindRef(FoundNodeGroup);
			if (!ResToMaterialElementMap)
			{
				TResolutionToMaterialElementMap* TempResToMaterialElementMap = new TResolutionToMaterialElementMap();
				NodeGroupToResolutionToMaterialElementMap.Set(FoundNodeGroup, TempResToMaterialElementMap);
				ResToMaterialElementMap = NodeGroupToResolutionToMaterialElementMap.FindRef(FoundNodeGroup);
				check(ResToMaterialElementMap);
			}

			// Find an element with the same material as this node.
			DWORD ResValue = appTrunc(Surf.ShadowMapScale);
			TMap<UMaterialInterface*,FModelElement*>* MaterialElementMap = ResToMaterialElementMap->FindRef(ResValue);
			if (!MaterialElementMap)
			{
				TMap<UMaterialInterface*,FModelElement*>* TempMap = new TMap<UMaterialInterface*,FModelElement*>();
				ResToMaterialElementMap->Set(ResValue, TempMap);
				MaterialElementMap = ResToMaterialElementMap->FindRef(ResValue);
				check(MaterialElementMap);
			}

			FModelElement* Element = MaterialElementMap->FindRef(Surf.Material);
			if (!Element)
			{
				// If there's no matching element, create a new element.
				Element = MaterialElementMap->Set(Surf.Material,new(Elements) FModelElement(this,Surf.Material));
			}
			check(Element);

			// Add the node to the element.
			Element->Nodes.AddItem(Nodes(NodeIndex));
		}
	}

	if (bBuildRenderData == TRUE)
	{
		BuildRenderData();
	}

	return TRUE;
}

/**
 *  Retrieve various actor metrics depending on the provided type.  All of
 *  these will total the values for this component.
 *
 *  @param MetricsType The type of metric to calculate.
 *
 *  METRICS_VERTS    - Get the number of vertices.
 *  METRICS_TRIS     - Get the number of triangles.
 *  METRICS_SECTIONS - Get the number of sections.
 *
 *  @return INT The total of the given type for this actor.
 */
INT UModelComponent::GetActorMetrics(EActorMetricsType MetricsType)
{
	if(GetModel() != NULL)
	{
		if(MetricsType == METRICS_VERTS)
		{
			return GetModel()->NumUniqueVertices;
		}
		else if(MetricsType == METRICS_TRIS)
		{
			UModel* Model(GetModel());
			INT TotalCount(0);

			for(INT PolyIndex(0); PolyIndex < Model->Polys->Element.Num(); ++PolyIndex)
			{
				TotalCount += Model->Polys->Element(PolyIndex).Vertices.Num()-2;
			}

			return TotalCount;
		}
	}

	return 0;
}
