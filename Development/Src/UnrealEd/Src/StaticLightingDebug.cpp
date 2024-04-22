/*=============================================================================
	StaticLightingDebug.cpp: Code for debugging static lighting
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "StaticLightingPrivate.h"
#include "EngineFluidClasses.h"

/** The color to set selected texels to */
FColor GTexelSelectionColor(255, 50, 0);

/** Information about the texel that is selected */
FSelectedLightmapSample GCurrentSelectedLightmapSample;

/** Information about the last static lighting build */
FDebugLightingOutput GDebugStaticLightingInfo;

#if !CONSOLE

/** Helper function that writes a texel into the given texture. */
static void WriteTexel(UTexture2D* Texture, INT X, INT Y, FColor NewColor, FColor& OriginalColor)
{
	if (X >= 0 && X < Texture->SizeX && Y >= 0 && Y < Texture->SizeY)
	{
		check(X >= 0 && X < Texture->SizeX);
		check(Y >= 0 && Y < Texture->SizeY);
		// Only supporting uncompressed textures for now
		if (Texture->Format == PF_A8R8G8B8)
		{
			// Release the texture's resources and block until the rendering thread is done accessing it
			Texture->ReleaseResource();
			FTexture2DMipMap& BaseMip = Texture->Mips(0);
			FColor* Data = (FColor*)BaseMip.Data.Lock( LOCK_READ_WRITE );
			FColor& SelectedTexel = Data[Y * Texture->SizeX + X];
			// Save the original color
			OriginalColor = SelectedTexel;
			// Write the new color
			SelectedTexel = NewColor;
			BaseMip.Data.Unlock();
			// Re-initialize the textures render resources
			Texture->UpdateResource();
		}
		else
		{
			debugf(TEXT("Texel selection coloring failed because the lightmap is not PF_A8R8G8B8!"));
		}
	}
}

/** Helper function that writes a color into the given vertex lightmap. */
static void WriteVertex(FLightMap1D* Lightmap, INT VertexIndex, FColor NewColor, FColor& OriginalColor)
{
	check(Lightmap);
	check(VertexIndex >= 0 && VertexIndex < Lightmap->NumSamples());
	// Release the vertex lightmap and block until the rendering thread is done accessing it
	BeginReleaseResource(Lightmap);
	FlushRenderingCommands();
	if (Lightmap->AllowsDirectionalLightmaps())
	{
		// Write the color for directional lightmaps
		FQuantizedDirectionalLightSample* Sample = (FQuantizedDirectionalLightSample*)Lightmap->GetDirectionalSamples().Lock(LOCK_READ_WRITE);
		for (INT CoefIndex = 0; CoefIndex < NUM_DIRECTIONAL_LIGHTMAP_COEF; CoefIndex++)
		{
			//@todo - save all coefficients of the original color
			OriginalColor = Sample[VertexIndex].Coefficients[CoefIndex];
			Sample[VertexIndex].Coefficients[CoefIndex] = NewColor;
		}
		Lightmap->GetDirectionalSamples().Unlock();
	}
	else
	{
		// Write the color for simple lightmaps
		FQuantizedSimpleLightSample* Sample = (FQuantizedSimpleLightSample*)Lightmap->GetSimpleSamples().Lock(LOCK_READ_WRITE);
		for (INT CoefIndex = 0; CoefIndex < NUM_SIMPLE_LIGHTMAP_COEF; CoefIndex++)
		{
			OriginalColor = Sample[VertexIndex].Coefficients[CoefIndex];
			Sample[VertexIndex].Coefficients[CoefIndex] = NewColor;
		}
		Lightmap->GetSimpleSamples().Unlock();
	}
	// Reinitialize the vertex lightmap
	Lightmap->InitResources();
}

static void RestoreSelectedLightmapSample()
{
	// Restore the last selected sample if it is still valid
	if (IsValidRef(GCurrentSelectedLightmapSample.Lightmap))
	{
		FColor DummyColor;
		FLightMap2D* OldLightmap2D = GCurrentSelectedLightmapSample.Lightmap->GetLightMap2D();
		FLightMap1D* OldLightmap1D = GCurrentSelectedLightmapSample.Lightmap->GetLightMap1D();
		if (OldLightmap2D)
		{
			if (OldLightmap2D->AllowsDirectionalLightmaps())
			{
				for (INT TextureIndex = 0; TextureIndex < NUM_DIRECTIONAL_LIGHTMAP_COEF; TextureIndex++)
				{
					UTexture2D* OldDirectionalLightmap = OldLightmap2D->GetTexture(TextureIndex);
					WriteTexel(OldDirectionalLightmap, GCurrentSelectedLightmapSample.LightmapX, GCurrentSelectedLightmapSample.LightmapY, GCurrentSelectedLightmapSample.OriginalColor, DummyColor);
				}
			}
			else
			{
				UTexture2D* OldSimpleLightmap = OldLightmap2D->GetTexture(SIMPLE_LIGHTMAP_COEF_INDEX);
				WriteTexel(OldSimpleLightmap, GCurrentSelectedLightmapSample.LightmapX, GCurrentSelectedLightmapSample.LightmapY, GCurrentSelectedLightmapSample.OriginalColor, DummyColor);
			}
		}
		else
		{
			WriteVertex(OldLightmap1D, GCurrentSelectedLightmapSample.VertexIndex, GCurrentSelectedLightmapSample.OriginalColor, DummyColor);
		}
	}
}

static UBOOL UpdateSelectedTexel(
	UPrimitiveComponent* Component, 
	INT NodeIndex, 
	FLightMapRef& Lightmap, 
	const FVector& Position, 
	FVector2D InterpolatedUV, 
	INT LocalX, INT LocalY,
	INT LightmapSizeX, INT LightmapSizeY)
{
	if (Component == GCurrentSelectedLightmapSample.Component
		&& NodeIndex == GCurrentSelectedLightmapSample.NodeIndex
		&& LocalX == GCurrentSelectedLightmapSample.LocalX
		&& LocalY == GCurrentSelectedLightmapSample.LocalY)
	{
		return FALSE;
	}
	else
	{
		// Store information about the selected texel
		FSelectedLightmapSample NewSelectedTexel(Component, NodeIndex, Lightmap, Position, LocalX, LocalY, LightmapSizeX, LightmapSizeY);

		if (IsValidRef(Lightmap))
		{
			FLightMap2D* Lightmap2D = Lightmap->GetLightMap2D();
			check(Lightmap2D);
			const FVector2D CoordinateScale = Lightmap2D->GetCoordinateScale();
			const FVector2D CoordinateBias = Lightmap2D->GetCoordinateBias();
			// Calculate lightmap atlas UV's for the selected point
			FVector2D LightmapUV = InterpolatedUV * CoordinateScale + CoordinateBias;

			if (Lightmap2D->AllowsDirectionalLightmaps())
			{
				for (INT TextureIndex = 0; TextureIndex < NUM_DIRECTIONAL_LIGHTMAP_COEF; TextureIndex++)
				{
					UTexture2D* CurrentLightmap = Lightmap2D->GetTexture(TextureIndex);
					// UV's in the lightmap atlas
					NewSelectedTexel.LightmapX = appTrunc(LightmapUV.X * CurrentLightmap->SizeX);
					NewSelectedTexel.LightmapY = appTrunc(LightmapUV.Y * CurrentLightmap->SizeY);
					// Write the selection color to the selected lightmap texel
					WriteTexel(CurrentLightmap, NewSelectedTexel.LightmapX, NewSelectedTexel.LightmapY, GTexelSelectionColor, NewSelectedTexel.OriginalColor);
				}
			}
			else
			{
				UTexture2D* SimpleLightmap = Lightmap2D->GetTexture(SIMPLE_LIGHTMAP_COEF_INDEX);
				// UV's in the lightmap atlas
				NewSelectedTexel.LightmapX = appTrunc(LightmapUV.X * SimpleLightmap->SizeX);
				NewSelectedTexel.LightmapY = appTrunc(LightmapUV.Y * SimpleLightmap->SizeY);
				// Write the selection color to the selected lightmap texel
				WriteTexel(SimpleLightmap, NewSelectedTexel.LightmapX, NewSelectedTexel.LightmapY, GTexelSelectionColor, NewSelectedTexel.OriginalColor);
			}

			GCurrentSelectedLightmapSample = NewSelectedTexel;
			return TRUE;
		}
		else
		{
			debugf(TEXT("Texel selection failed because the lightmap is an invalid reference!"));
			return FALSE;
		}
	}
}

static UBOOL UpdateSelectedVertex(
	UPrimitiveComponent* Component, 
	FLightMapRef& Lightmap, 
	const FVector& Position, 
	INT VertexIndex)
{
	if (Component == GCurrentSelectedLightmapSample.Component
		&& VertexIndex == GCurrentSelectedLightmapSample.VertexIndex)
	{
		return FALSE;
	}
	else
	{
		// Store information about the selected vertex
		FSelectedLightmapSample NewSelectedVertex(Component, Lightmap, Position, VertexIndex);
		if (IsValidRef(Lightmap))
		{
			FLightMap1D* Lightmap1D = Lightmap->GetLightMap1D();
			check(Lightmap1D);
			WriteVertex(Lightmap1D, VertexIndex, GTexelSelectionColor, NewSelectedVertex.OriginalColor);
			GCurrentSelectedLightmapSample = NewSelectedVertex;
			return TRUE;
		}
		else
		{
			debugf(TEXT("Vertex selection failed because the lightmap is an invalid reference!"));
			return FALSE;
		}
	}
}

static UBOOL GetBarycentricWeights(
	const FVector& Position0,
	const FVector& Position1,
	const FVector& Position2,
	FVector InterpolatePosition,
	FLOAT Tolerance,
	FVector& BarycentricWeights
	)
{
	BarycentricWeights = FVector(0,0,0);
	FVector TriangleNormal = (Position0 - Position1) ^ (Position2 - Position0);
	FLOAT ParallelogramArea = TriangleNormal.Size();
	FVector UnitTriangleNormal = TriangleNormal / ParallelogramArea;
	FLOAT PlaneDistance = UnitTriangleNormal | (InterpolatePosition - Position0);

	// Only continue if the position to interpolate to is in the plane of the triangle (within some error)
	if (Abs(PlaneDistance) < Tolerance)
	{
		// Move the position to interpolate to into the plane of the triangle along the normal, 
		// Otherwise there will be error in our barycentric coordinates
		InterpolatePosition -= UnitTriangleNormal * PlaneDistance;

		FVector NormalU = (InterpolatePosition - Position1) ^ (Position2 - InterpolatePosition);
		// Signed area, if negative then InterpolatePosition is not in the triangle
		FLOAT ParallelogramAreaU = NormalU.Size() * appFloatSelect(NormalU | TriangleNormal, 1.0f, -1.0f);
		FLOAT BaryCentricU = ParallelogramAreaU / ParallelogramArea;

		FVector NormalV = (InterpolatePosition - Position2) ^ (Position0 - InterpolatePosition);
		FLOAT ParallelogramAreaV = NormalV.Size() * appFloatSelect(NormalV | TriangleNormal, 1.0f, -1.0f);
		FLOAT BaryCentricV = ParallelogramAreaV / ParallelogramArea;

		FLOAT BaryCentricW = 1.0f - BaryCentricU - BaryCentricV;
		if (BaryCentricU > -Tolerance && BaryCentricV > -Tolerance && BaryCentricW > -Tolerance)
		{
			BarycentricWeights = FVector(BaryCentricU, BaryCentricV, BaryCentricW);
			return TRUE;
		}
	}
	return FALSE;
}

/** Updates GCurrentSelectedLightmapSample given a selected actor's components and the location of the click. */
void SetDebugLightmapSample(TArrayNoInit<UActorComponent*>* Components, UModel* Model, INT iSurf, FVector ClickLocation)
{
	UStaticMeshComponent* SMComponent = NULL;
	UFluidSurfaceComponent* FluidComponent = NULL;
	if (Components)
	{
		// Find the first supported component
		for (INT ComponentIndex = 0; ComponentIndex < Components->Num() && !(SMComponent || FluidComponent); ComponentIndex++)
		{
			SMComponent = Cast<UStaticMeshComponent>((*Components)(ComponentIndex));
			if (SMComponent && (!SMComponent->StaticMesh || SMComponent->LODData.Num() == 0))
			{
				SMComponent = NULL;
			}
			FluidComponent = Cast<UFluidSurfaceComponent>((*Components)(ComponentIndex));
		}
	}
	 
	UBOOL bFoundLightmapSample = FALSE;
	// Only static mesh components, fluid components and BSP handled for now
	if (SMComponent)
	{
		UStaticMesh* StaticMesh = SMComponent->StaticMesh;
		check(StaticMesh);
		check(StaticMesh->LODModels.Num());
		// Only supporting LOD0
		const INT LODIndex = 0;
		FStaticMeshRenderData& LODModel = StaticMesh->LODModels(LODIndex);
		const UBOOL bHasStaticShadowing = SMComponent->HasStaticShadowing();
		if (bHasStaticShadowing)
		{
			UBOOL bUseTextureMap = FALSE;
			INT LightmapSizeX = 0;
			INT LightmapSizeY = 0;
			SMComponent->GetLightMapResolution(LightmapSizeX, LightmapSizeY);

			if (LightmapSizeX > 0 && LightmapSizeY > 0 
				&& StaticMesh->LightMapCoordinateIndex >= 0 
				&& (UINT)StaticMesh->LightMapCoordinateIndex < LODModel.VertexBuffer.GetNumTexCoords()
				)
			{
				bUseTextureMap = TRUE;
			}
			else
			{
				bUseTextureMap = FALSE;
			}

			// Search through the static mesh's triangles for the one that was hit (since we can't get triangle index from a line check)
			for(INT TriangleIndex = 0; TriangleIndex < LODModel.IndexBuffer.Indices.Num(); TriangleIndex += 3)
			{
				UINT Index0 = LODModel.IndexBuffer.Indices(TriangleIndex);
				UINT Index1 = LODModel.IndexBuffer.Indices(TriangleIndex + 1);
				UINT Index2 = LODModel.IndexBuffer.Indices(TriangleIndex + 2);

				// Transform positions to world space
				FVector Position0 = SMComponent->LocalToWorld.TransformFVector(LODModel.PositionVertexBuffer.VertexPosition(Index0));
				FVector Position1 = SMComponent->LocalToWorld.TransformFVector(LODModel.PositionVertexBuffer.VertexPosition(Index1));
				FVector Position2 = SMComponent->LocalToWorld.TransformFVector(LODModel.PositionVertexBuffer.VertexPosition(Index2));

				FVector BaryCentricWeights;
				// Continue if click location is in the triangle and get its barycentric weights
				if (GetBarycentricWeights(Position0, Position1, Position2, ClickLocation, .001f, BaryCentricWeights))
				{
					RestoreSelectedLightmapSample();

					if (bUseTextureMap)
					{
						// Fetch lightmap UV's
						FVector2D LightmapUV0 = LODModel.VertexBuffer.GetVertexUV(Index0, StaticMesh->LightMapCoordinateIndex);
						FVector2D LightmapUV1 = LODModel.VertexBuffer.GetVertexUV(Index1, StaticMesh->LightMapCoordinateIndex);
						FVector2D LightmapUV2 = LODModel.VertexBuffer.GetVertexUV(Index2, StaticMesh->LightMapCoordinateIndex);
						// Interpolate lightmap UV's to the click location
						FVector2D InterpolatedUV = LightmapUV0 * BaryCentricWeights.X + LightmapUV1 * BaryCentricWeights.Y + LightmapUV2 * BaryCentricWeights.Z;

						INT PaddedSizeX = LightmapSizeX;
						INT PaddedSizeY = LightmapSizeY;
						if (GLightmassDebugOptions.bPadMappings && GAllowLightmapPadding && LightmapSizeX - 2 > 0 && LightmapSizeY - 2 > 0)
						{
							PaddedSizeX -= 2;
							PaddedSizeY -= 2;
						}

						const INT LocalX = appTrunc(InterpolatedUV.X * PaddedSizeX);
						const INT LocalY = appTrunc(InterpolatedUV.Y * PaddedSizeY);
						if (LocalX < 0 || LocalX >= PaddedSizeX
							|| LocalY < 0 || LocalY >= PaddedSizeY)
						{
							debugf(TEXT("Texel selection failed because the lightmap UV's wrap!"));
						}
						else
						{
							bFoundLightmapSample = UpdateSelectedTexel(SMComponent, -1, SMComponent->LODData(LODIndex).LightMap, ClickLocation, InterpolatedUV, LocalX, LocalY, LightmapSizeX, LightmapSizeY);
						}
					}
					else
					{
						// Find the vertex index that the click location was closest to
						INT MostInfluentialVertexIndex = Index0;
						if (BaryCentricWeights.Y > BaryCentricWeights.X)
						{
							MostInfluentialVertexIndex = Index1;
							if (BaryCentricWeights.Z > BaryCentricWeights.Y)
							{
								MostInfluentialVertexIndex = Index2;
							}
						}
						else if (BaryCentricWeights.Z > BaryCentricWeights.X)
						{
							MostInfluentialVertexIndex = Index2;
						}
						
						bFoundLightmapSample = UpdateSelectedVertex(SMComponent, SMComponent->LODData(LODIndex).LightMap, ClickLocation, MostInfluentialVertexIndex);
					}
					break;
				}
			}
		}
	}
	else if (FluidComponent)
	{
		INT LightmapSizeX = 0;
		INT LightmapSizeY = 0;
		FluidComponent->GetLightMapResolution(LightmapSizeX, LightmapSizeY);
		if (LightmapSizeX > 0 && LightmapSizeY > 0)
		{
			RestoreSelectedLightmapSample();
			// Can only debug the center texel for now
			//@todo - handle padding and arbitrary texel selection
			INT LocalX = LightmapSizeX / 2;
			INT LocalY = LightmapSizeY / 2;
			FVector2D InterpolatedUV(LocalX / (FLOAT)LightmapSizeX, LocalY / (FLOAT)LightmapSizeY);
			bFoundLightmapSample = UpdateSelectedTexel(FluidComponent, -1, FluidComponent->LightMap, ClickLocation, InterpolatedUV, LocalX, LocalY, LightmapSizeX, LightmapSizeY);
		}
	}
	else if (Model)
	{
		for (INT ModelIndex = 0; ModelIndex < GWorld->CurrentLevel->ModelComponents.Num(); ModelIndex++)
		{
			UModelComponent* CurrentComponent = GWorld->CurrentLevel->ModelComponents(ModelIndex);
			INT LightmapSizeX = 0;
			INT LightmapSizeY = 0;
			CurrentComponent->GetLightMapResolution(LightmapSizeX, LightmapSizeY);
			if (LightmapSizeX > 0 && LightmapSizeY > 0)
			{
				for (INT ElementIndex = 0; ElementIndex < CurrentComponent->GetElements().Num(); ElementIndex++)
				{
					FModelElement& Element = CurrentComponent->GetElements()(ElementIndex);
					TScopedPointer<FRawIndexBuffer16or32>* IndexBufferRef = Model->MaterialIndexBuffers.Find(Element.Material);
					check(IndexBufferRef);
					for(UINT TriangleIndex = Element.FirstIndex; TriangleIndex < Element.FirstIndex + Element.NumTriangles * 3; TriangleIndex += 3)
					{
						UINT Index0 = (*IndexBufferRef)->Indices(TriangleIndex);
						UINT Index1 = (*IndexBufferRef)->Indices(TriangleIndex + 1);
						UINT Index2 = (*IndexBufferRef)->Indices(TriangleIndex + 2);

						FModelVertex* ModelVertices = (FModelVertex*)Model->VertexBuffer.Vertices.GetData();
						FVector Position0 = ModelVertices[Index0].Position;
						FVector Position1 = ModelVertices[Index1].Position;
						FVector Position2 = ModelVertices[Index2].Position;

						FVector BaryCentricWeights;
						// Continue if click location is in the triangle and get its barycentric weights
						if (GetBarycentricWeights(Position0, Position1, Position2, ClickLocation, .001f, BaryCentricWeights))
						{
							RestoreSelectedLightmapSample();

							// Fetch lightmap UV's
							FVector2D LightmapUV0 = ModelVertices[Index0].ShadowTexCoord;
							FVector2D LightmapUV1 = ModelVertices[Index1].ShadowTexCoord;
							FVector2D LightmapUV2 = ModelVertices[Index2].ShadowTexCoord;
							// Interpolate lightmap UV's to the click location
							FVector2D InterpolatedUV = LightmapUV0 * BaryCentricWeights.X + LightmapUV1 * BaryCentricWeights.Y + LightmapUV2 * BaryCentricWeights.Z;

							// Find the node index belonging to the selected triangle
							const UModel* CurrentModel = CurrentComponent->GetModel();
							INT SelectedNodeIndex = INDEX_NONE;
							for (INT ElementNodeIndex = 0; ElementNodeIndex < Element.Nodes.Num(); ElementNodeIndex++)
							{
								const FBspNode& CurrentNode = CurrentModel->Nodes(Element.Nodes(ElementNodeIndex));
								if ((INT)Index0 >= CurrentNode.iVertexIndex && (INT)Index0 < CurrentNode.iVertexIndex + CurrentNode.NumVertices)
								{
									SelectedNodeIndex = Element.Nodes(ElementNodeIndex);
								}
							}
							check(SelectedNodeIndex >= 0);

							TArray<ULightComponent*> DummyLights;

							// fill out the model's NodeGroups (not the mapping part of it, but the nodes part)
							Model->GroupAllNodes(GWorld->CurrentLevel, DummyLights);

							// Find the FGatheredSurface that the selected node got put into during the last lighting rebuild
							TArray<INT> GatheredNodes;

							// find the NodeGroup that this node went into, and get all of its node
							for (TMap<INT, FNodeGroup*>::TIterator It(Model->NodeGroups); It && GatheredNodes.Num() == 0; ++It)
							{
								FNodeGroup* NodeGroup = It.Value();
								for (INT NodeIndex = 0; NodeIndex < NodeGroup->Nodes.Num(); NodeIndex++)
								{
									if (NodeGroup->Nodes(NodeIndex) == SelectedNodeIndex)
									{
										GatheredNodes = NodeGroup->Nodes;
										break;
									}
								}
							}
							check(GatheredNodes.Num() > 0);

							// use the surface of the selected node, it will have to suffice for the GetSurfaceLightMapResolution() call
							INT SelectedGatheredSurfIndex = Model->Nodes(SelectedNodeIndex).iSurf;

							// Get the lightmap resolution used by the FGatheredSurface containing the selected node
							FMatrix WorldToMap;
							CurrentComponent->GetSurfaceLightMapResolution(SelectedGatheredSurfIndex, 1, LightmapSizeX, LightmapSizeY, WorldToMap, &GatheredNodes);

							INT PaddedSizeX = LightmapSizeX;
							INT PaddedSizeY = LightmapSizeY;
							if (GLightmassDebugOptions.bPadMappings && GAllowLightmapPadding && LightmapSizeX - 2 > 0 && LightmapSizeY - 2 > 0)
							{
								PaddedSizeX -= 2;
								PaddedSizeY -= 2;
							}
							check(LightmapSizeX > 0 && LightmapSizeY > 0);

							// Apply the transform to the intersection position to find the local texel coordinates
							const FVector4 StaticLightingTextureCoordinate = WorldToMap.TransformFVector(ClickLocation);
							const INT LocalX = appTrunc(StaticLightingTextureCoordinate.X * PaddedSizeX);
							const INT LocalY = appTrunc(StaticLightingTextureCoordinate.Y * PaddedSizeY);
							check(LocalX >= 0 && LocalX < PaddedSizeX && LocalY >= 0 && LocalY < PaddedSizeY);

							bFoundLightmapSample = UpdateSelectedTexel(
								CurrentComponent, 
								SelectedNodeIndex, 
								Element.LightMap, 
								ClickLocation, 
								InterpolatedUV, 
								LocalX, LocalY,
								LightmapSizeX, LightmapSizeY);

							if (!bFoundLightmapSample)
							{
								RestoreSelectedLightmapSample();
								GCurrentSelectedLightmapSample = FSelectedLightmapSample();
							}
							return;
						}
					}
				}
			}
		}
	}

	if (!bFoundLightmapSample)
	{
		RestoreSelectedLightmapSample();
		GCurrentSelectedLightmapSample = FSelectedLightmapSample();
	}
}

/** Renders debug elements for visualizing static lighting info */
void DrawStaticLightingDebugInfo(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
	if (GDebugStaticLightingInfo.bValid)
	{
		for (INT VertexIndex = 0; VertexIndex < GDebugStaticLightingInfo.Vertices.Num(); VertexIndex++)
		{
			const FDebugStaticLightingVertex& CurrentVertex = GDebugStaticLightingInfo.Vertices(VertexIndex);
			FColor NormalColor(250,250,50);
			if (GDebugStaticLightingInfo.SelectedVertexIndices.ContainsItem(VertexIndex))
			{
				NormalColor = FColor(150, 250, 250);
				for (INT CornerIndex = 0; CornerIndex < NumTexelCorners; CornerIndex++)
				{
					if (GDebugStaticLightingInfo.bCornerValid[CornerIndex])
					{
						PDI->DrawPoint(GDebugStaticLightingInfo.TexelCorners[CornerIndex] + CurrentVertex.VertexNormal * .2f, FLinearColor(0, 1, 1), 4.0f, SDPG_World);
					}
				}
				PDI->DrawPoint(CurrentVertex.VertexPosition, NormalColor, 4.0f, SDPG_World);
				DrawWireSphere(PDI, CurrentVertex.VertexPosition, NormalColor, GDebugStaticLightingInfo.SampleRadius, 36, SDPG_World);
			}
			PDI->DrawLine(CurrentVertex.VertexPosition, CurrentVertex.VertexPosition + CurrentVertex.VertexNormal * 10, NormalColor, SDPG_World);
		}

		for (INT RayIndex = 0; RayIndex < GDebugStaticLightingInfo.ShadowRays.Num(); RayIndex++)
		{
			const FDebugStaticLightingRay& CurrentRay = GDebugStaticLightingInfo.ShadowRays(RayIndex);
			PDI->DrawLine(CurrentRay.Start, CurrentRay.End, CurrentRay.bHit ? FColor(255,0,0) : FColor(0,255,0), SDPG_World);
		}
		
		for (INT RayIndex = 0; RayIndex < GDebugStaticLightingInfo.PathRays.Num(); RayIndex++)
		{
			const FDebugStaticLightingRay& CurrentRay = GDebugStaticLightingInfo.PathRays(RayIndex);
			const FColor RayColor = CurrentRay.bHit ? (CurrentRay.bPositive ? FColor(255,255,150) : FColor(150,150,150)) : FColor(50,50,255);
			PDI->DrawLine(CurrentRay.Start, CurrentRay.End, RayColor, SDPG_World);
		}

		for (INT RecordIndex = 0; RecordIndex < GDebugStaticLightingInfo.CacheRecords.Num(); RecordIndex++)
		{
			const FDebugLightingCacheRecord& CurrentRecord = GDebugStaticLightingInfo.CacheRecords(RecordIndex);
			if (CurrentRecord.bNearSelectedTexel)
			{
				DrawWireSphere(PDI, CurrentRecord.Vertex.VertexPosition + CurrentRecord.Vertex.VertexNormal * .1f, CurrentRecord.bAffectsSelectedTexel ? FColor(50, 255, 100) : FColor(100, 100, 100), CurrentRecord.Radius, 36, SDPG_World);
				PDI->DrawLine(CurrentRecord.Vertex.VertexPosition, CurrentRecord.Vertex.VertexPosition + CurrentRecord.Vertex.VertexNormal * 60, CurrentRecord.bAffectsSelectedTexel ? FColor(50, 255, 100) : FColor(100, 100, 100), SDPG_World);
			}
			PDI->DrawPoint(CurrentRecord.Vertex.VertexPosition + CurrentRecord.Vertex.VertexNormal * .1f, FLinearColor(.5, 1, .5), 2.0f, SDPG_World);
		}

		for (INT PhotonIndex = 0; PhotonIndex < GDebugStaticLightingInfo.DirectPhotons.Num(); PhotonIndex++)
		{
			const FDebugPhoton& CurrentPhoton = GDebugStaticLightingInfo.DirectPhotons(PhotonIndex);
			PDI->DrawLine(CurrentPhoton.Position, CurrentPhoton.Position + CurrentPhoton.Direction * 50, FColor(200, 200, 100), SDPG_World);
		}

		for (INT PhotonIndex = 0; PhotonIndex < GDebugStaticLightingInfo.IndirectPhotons.Num(); PhotonIndex++)
		{
			const FDebugPhoton& CurrentPhoton = GDebugStaticLightingInfo.IndirectPhotons(PhotonIndex);
			PDI->DrawLine(CurrentPhoton.Position, CurrentPhoton.Position + CurrentPhoton.Direction, FColor(200, 100, 100), SDPG_World);
		}

		for (INT PhotonIndex = 0; PhotonIndex < GDebugStaticLightingInfo.IrradiancePhotons.Num(); PhotonIndex++)
		{
			const FDebugPhoton& CurrentPhoton = GDebugStaticLightingInfo.IrradiancePhotons(PhotonIndex);
			PDI->DrawLine(CurrentPhoton.Position, CurrentPhoton.Position + CurrentPhoton.Direction * 50, FColor(150, 100, 250), SDPG_World);
		}

		for (INT PhotonIndex = 0; PhotonIndex < GDebugStaticLightingInfo.GatheredCausticPhotons.Num(); PhotonIndex++)
		{
			const FDebugPhoton& CurrentPhoton = GDebugStaticLightingInfo.GatheredCausticPhotons(PhotonIndex);
			PDI->DrawLine(CurrentPhoton.Position, CurrentPhoton.Position + CurrentPhoton.Direction * 50, FColor(100, 100, 200), SDPG_World);
			PDI->DrawPoint(CurrentPhoton.Position + CurrentPhoton.Direction * .1f, FLinearColor(.5, 1, .5), 4.0f, SDPG_World);
		}

		for (INT PhotonIndex = 0; PhotonIndex < GDebugStaticLightingInfo.GatheredPhotons.Num(); PhotonIndex++)
		{
			const FDebugPhoton& CurrentPhoton = GDebugStaticLightingInfo.GatheredPhotons(PhotonIndex);
			PDI->DrawLine(CurrentPhoton.Position, CurrentPhoton.Position + CurrentPhoton.Normal * 50, FColor(100, 100, 100), SDPG_World);
			PDI->DrawLine(CurrentPhoton.Position, CurrentPhoton.Position + CurrentPhoton.Direction * 50, FColor(50, 255, 100), SDPG_World);
			PDI->DrawPoint(CurrentPhoton.Position + CurrentPhoton.Direction * .1f, FLinearColor(.5, 1, .5), 4.0f, SDPG_World);
		}

		for (INT PhotonIndex = 0; PhotonIndex < GDebugStaticLightingInfo.GatheredImportancePhotons.Num(); PhotonIndex++)
		{
			const FDebugPhoton& CurrentPhoton = GDebugStaticLightingInfo.GatheredImportancePhotons(PhotonIndex);
			PDI->DrawLine(CurrentPhoton.Position, CurrentPhoton.Position + CurrentPhoton.Normal * 50, FColor(100, 100, 100), SDPG_World);
			PDI->DrawLine(CurrentPhoton.Position, CurrentPhoton.Position + CurrentPhoton.Direction * 50, FColor(200, 100, 100), SDPG_World);
			PDI->DrawPoint(CurrentPhoton.Position + CurrentPhoton.Direction * .1f, FLinearColor(.5, 1, .5), 4.0f, SDPG_World);
		}
		const FColor NodeColor(150, 170, 180);
		for (INT NodeIndex = 0; NodeIndex < GDebugStaticLightingInfo.GatheredPhotonNodes.Num(); NodeIndex++)
		{
			const FDebugOctreeNode& CurrentNode = GDebugStaticLightingInfo.GatheredPhotonNodes(NodeIndex);
			PDI->DrawLine(CurrentNode.Center + FVector(CurrentNode.Extent.X, CurrentNode.Extent.Y, CurrentNode.Extent.Z), CurrentNode.Center + FVector(-CurrentNode.Extent.X, CurrentNode.Extent.Y, CurrentNode.Extent.Z), NodeColor, SDPG_World);
			PDI->DrawLine(CurrentNode.Center + FVector(CurrentNode.Extent.X, CurrentNode.Extent.Y, CurrentNode.Extent.Z), CurrentNode.Center + FVector(CurrentNode.Extent.X, -CurrentNode.Extent.Y, CurrentNode.Extent.Z), NodeColor, SDPG_World);
			PDI->DrawLine(CurrentNode.Center + FVector(CurrentNode.Extent.X, CurrentNode.Extent.Y, CurrentNode.Extent.Z), CurrentNode.Center + FVector(CurrentNode.Extent.X, CurrentNode.Extent.Y, -CurrentNode.Extent.Z), NodeColor, SDPG_World);
			
			PDI->DrawLine(CurrentNode.Center - FVector(CurrentNode.Extent.X, CurrentNode.Extent.Y, CurrentNode.Extent.Z), CurrentNode.Center - FVector(-CurrentNode.Extent.X, CurrentNode.Extent.Y, CurrentNode.Extent.Z), NodeColor, SDPG_World);
			PDI->DrawLine(CurrentNode.Center - FVector(CurrentNode.Extent.X, CurrentNode.Extent.Y, CurrentNode.Extent.Z), CurrentNode.Center - FVector(CurrentNode.Extent.X, -CurrentNode.Extent.Y, CurrentNode.Extent.Z), NodeColor, SDPG_World);
			PDI->DrawLine(CurrentNode.Center - FVector(CurrentNode.Extent.X, CurrentNode.Extent.Y, CurrentNode.Extent.Z), CurrentNode.Center - FVector(CurrentNode.Extent.X, CurrentNode.Extent.Y, -CurrentNode.Extent.Z), NodeColor, SDPG_World);
			
			PDI->DrawLine(CurrentNode.Center + FVector(CurrentNode.Extent.X, -CurrentNode.Extent.Y, CurrentNode.Extent.Z), CurrentNode.Center + FVector(CurrentNode.Extent.X, -CurrentNode.Extent.Y, -CurrentNode.Extent.Z), NodeColor, SDPG_World);
			PDI->DrawLine(CurrentNode.Center + FVector(CurrentNode.Extent.X, -CurrentNode.Extent.Y, CurrentNode.Extent.Z), CurrentNode.Center + FVector(-CurrentNode.Extent.X, -CurrentNode.Extent.Y, CurrentNode.Extent.Z), NodeColor, SDPG_World);

			PDI->DrawLine(CurrentNode.Center + FVector(-CurrentNode.Extent.X, CurrentNode.Extent.Y, CurrentNode.Extent.Z), CurrentNode.Center + FVector(-CurrentNode.Extent.X, -CurrentNode.Extent.Y, CurrentNode.Extent.Z), NodeColor, SDPG_World);
			PDI->DrawLine(CurrentNode.Center + FVector(-CurrentNode.Extent.X, CurrentNode.Extent.Y, CurrentNode.Extent.Z), CurrentNode.Center + FVector(-CurrentNode.Extent.X, CurrentNode.Extent.Y, -CurrentNode.Extent.Z), NodeColor, SDPG_World);

			PDI->DrawLine(CurrentNode.Center + FVector(CurrentNode.Extent.X, CurrentNode.Extent.Y, -CurrentNode.Extent.Z), CurrentNode.Center + FVector(CurrentNode.Extent.X, -CurrentNode.Extent.Y, -CurrentNode.Extent.Z), NodeColor, SDPG_World);
			PDI->DrawLine(CurrentNode.Center + FVector(CurrentNode.Extent.X, CurrentNode.Extent.Y, -CurrentNode.Extent.Z), CurrentNode.Center + FVector(-CurrentNode.Extent.X, CurrentNode.Extent.Y, -CurrentNode.Extent.Z), NodeColor, SDPG_World);
		}

		if (GDebugStaticLightingInfo.bDirectPhotonValid)
		{
			const FDebugPhoton& DirectPhoton = GDebugStaticLightingInfo.GatheredDirectPhoton;
			PDI->DrawLine(DirectPhoton.Position, DirectPhoton.Position + DirectPhoton.Direction * 60, FColor(255, 255, 100), SDPG_World);
			PDI->DrawPoint(DirectPhoton.Position + DirectPhoton.Direction * .1f, FLinearColor(1, 1, .5), 4.0f, SDPG_World);
		}

		for (INT RayIndex = 0; RayIndex < GDebugStaticLightingInfo.IndirectPhotonPaths.Num(); RayIndex++)
		{
			const FDebugStaticLightingRay& CurrentRay = GDebugStaticLightingInfo.IndirectPhotonPaths(RayIndex);
			PDI->DrawLine(CurrentRay.Start, CurrentRay.End, FColor(255,255,255), SDPG_World);
		}

		for (INT SampleIndex = 0; SampleIndex < GDebugStaticLightingInfo.VolumeLightingSamples.Num(); SampleIndex++)
		{
			const FDebugVolumeLightingSample& CurrentSample = GDebugStaticLightingInfo.VolumeLightingSamples(SampleIndex);
			PDI->DrawPoint(CurrentSample.Position, CurrentSample.AverageIncidentRadiance * GEngine->LightingOnlyBrightness, 12.0f, SDPG_World);
		}

		for (INT RayIndex = 0; RayIndex < GDebugStaticLightingInfo.PrecomputedVisibilityRays.Num(); RayIndex++)
		{
			const FDebugStaticLightingRay& CurrentRay = GDebugStaticLightingInfo.PrecomputedVisibilityRays(RayIndex);
			const FColor RayColor = CurrentRay.bHit ? (CurrentRay.bPositive ? FColor(255,255,150) : FColor(150,150,150)) : FColor(50,50,255);
			PDI->DrawLine(CurrentRay.Start, CurrentRay.End, RayColor, SDPG_World);
		}
	}
#endif
}

/** Renders debug elements for visualizing static lighting info */
void DrawStaticLightingDebugInfo(const FSceneView* View, FCanvas* Canvas)
{
#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
	if (GDebugStaticLightingInfo.bValid)
	{
		for (INT RecordIndex = 0; RecordIndex < GDebugStaticLightingInfo.CacheRecords.Num(); RecordIndex++)
		{
			const FDebugLightingCacheRecord& CurrentRecord = GDebugStaticLightingInfo.CacheRecords(RecordIndex);
			if (CurrentRecord.bNearSelectedTexel)
			{
				FVector2D PixelLocation;
				if(View->ScreenToPixel(View->WorldToScreen(CurrentRecord.Vertex.VertexPosition),PixelLocation))
				{
					const FColor TagColor = CurrentRecord.bAffectsSelectedTexel ? FColor(50,160,200) : FColor(120,120,120);
					DrawShadowedString(Canvas,PixelLocation.X,PixelLocation.Y, *appItoa(CurrentRecord.RecordId), GEngine->SmallFont, TagColor);
				}
			}
		}

		for (INT PhotonIndex = 0; PhotonIndex < GDebugStaticLightingInfo.GatheredImportancePhotons.Num(); PhotonIndex++)
		{
			const FDebugPhoton& CurrentPhoton = GDebugStaticLightingInfo.GatheredImportancePhotons(PhotonIndex);
			FVector2D PixelLocation;
			if(View->ScreenToPixel(View->WorldToScreen(CurrentPhoton.Position),PixelLocation))
			{
				const FColor TagColor = FColor(120,120,120);
				DrawShadowedString(Canvas,PixelLocation.X,PixelLocation.Y, *appItoa(CurrentPhoton.Id), GEngine->SmallFont, TagColor);
			}
		}

		for (INT RayIndex = 0; RayIndex < GDebugStaticLightingInfo.PathRays.Num(); RayIndex++)
		{
			const FDebugStaticLightingRay& CurrentRay = GDebugStaticLightingInfo.PathRays(RayIndex);
			if (CurrentRay.bHit && CurrentRay.bPositive)
			{
				FVector2D PixelLocation;
				if(View->ScreenToPixel(View->WorldToScreen(CurrentRay.End),PixelLocation))
				{
					const FColor TagColor = FColor(180,180,120);
					DrawShadowedString(Canvas,PixelLocation.X,PixelLocation.Y, *appItoa(RayIndex), GEngine->SmallFont, TagColor);
				}
			}
		}
	}
#endif
}

#endif	//#if !CONSOLE