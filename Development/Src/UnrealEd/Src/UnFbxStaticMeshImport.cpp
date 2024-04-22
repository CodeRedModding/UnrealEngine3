/*
* Copyright 2009 Autodesk, Inc.  All Rights Reserved.
*
* Permission to use, copy, modify, and distribute this software in object
* code form for any purpose and without fee is hereby granted, provided
* that the above copyright notice appears in all copies and that both
* that copyright notice and the limited warranty and restricted rights
* notice below appear in all supporting documentation.
*
* AUTODESK PROVIDES THIS PROGRAM "AS IS" AND WITH ALL FAULTS.
* AUTODESK SPECIFICALLY DISCLAIMS ANY AND ALL WARRANTIES, WHETHER EXPRESS
* OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTY
* OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE OR NON-INFRINGEMENT
* OF THIRD PARTY RIGHTS.  AUTODESK DOES NOT WARRANT THAT THE OPERATION
* OF THE PROGRAM WILL BE UNINTERRUPTED OR ERROR FREE.
*
* In no event shall Autodesk, Inc. be liable for any direct, indirect,
* incidental, special, exemplary, or consequential damages (including,
* but not limited to, procurement of substitute goods or services;
* loss of use, data, or profits; or business interruption) however caused
* and on any theory of liability, whether in contract, strict liability,
* or tort (including negligence or otherwise) arising in any way out
* of such code.
*
* This software is provided to the U.S. Government with the same rights
* and restrictions as described herein.
*/

/*=============================================================================
	Static mesh creation from FBX data.
	Largely based on UnStaticMeshEdit.cpp
=============================================================================*/

#include "UnrealEd.h"

#if WITH_FBX
#include "Factories.h"
#include "Engine.h"
#include "UnTextureLayout.h"
#include "UnFracturedStaticMesh.h"
#include "EnginePhysicsClasses.h"
#include "BSPOps.h"
#include "EngineMaterialClasses.h"
#include "EngineInterpolationClasses.h"
#include "UnLinkedObjDrawUtils.h"

#include "UnFbxImporter.h"

using namespace UnFbx;

struct ExistingStaticMeshData;
extern ExistingStaticMeshData* SaveExistingStaticMeshData(UStaticMesh* ExistingMesh);
extern void RestoreExistingMeshData(struct ExistingStaticMeshData* ExistingMeshDataPtr, UStaticMesh* NewMesh);

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
UObject* UnFbx::CFbxImporter::ImportStaticMesh(UObject* InParent, FbxNode* Node, const FName& Name, EObjectFlags Flags, UStaticMesh* InStaticMesh, int LODIndex)
{
	TArray<FbxNode*> MeshNodeArray;
	
	if ( !Node->GetMesh())
	{
		return NULL;
	}
	
	MeshNodeArray.AddItem(Node);
	return ImportStaticMeshAsSingle(InParent, MeshNodeArray, Name, Flags, InStaticMesh, LODIndex);
}

UBOOL UnFbx::CFbxImporter::BuildStaticMeshFromGeometry(FbxMesh* Mesh, UStaticMesh* StaticMesh, int LODIndex, TMap<FVector, FColor>* ExistingVertexColorData)
{
	FbxNode* Node = Mesh->GetNode();

    //remove the bad polygons before getting any data from mesh
    Mesh->RemoveBadPolygons();

    //Get the base layer of the mesh
    FbxLayer* BaseLayer = Mesh->GetLayer(0);
    if (BaseLayer == NULL)
    {
        warnf(NAME_Error,TEXT("There is no geometry information in mesh"),ANSI_TO_TCHAR(Mesh->GetName()));
        return FALSE;
    }

	//
	//	store the UVs in arrays for fast access in the later looping of triangles 
	//
	// mapping from UVSets to Fbx LayerElementUV
	// Fbx UVSets may be duplicated, remove the duplicated UVSets in the mapping 
	INT LayerCount = Mesh->GetLayerCount();
	TArray<FString> UVSets;
	UVSets.Empty();
	if (LayerCount > 0)
	{
		INT UVLayerIndex;
		for (UVLayerIndex = 0; UVLayerIndex<LayerCount; UVLayerIndex++)
		{
			FbxLayer* lLayer = Mesh->GetLayer(UVLayerIndex);
			int UVSetCount = lLayer->GetUVSetCount();
			if(UVSetCount)
			{
				FbxArray<FbxLayerElementUV const*> EleUVs = lLayer->GetUVSets();
				for (int UVIndex = 0; UVIndex<UVSetCount; UVIndex++)
				{
					FbxLayerElementUV const* ElementUV = EleUVs[UVIndex];
					if (ElementUV)
					{
						const char* UVSetName = ElementUV->GetName();
						FString LocalUVSetName = ANSI_TO_TCHAR(UVSetName);

						UVSets.AddUniqueItem(LocalUVSetName);
					}
				}
			}
		}
	}


	// If the the UV sets are named using the following format (UVChannel_X; where X ranges from 1 to 4)
	// we will re-order them based on these names.  Any UV sets that do not follow this naming convention
	// will be slotted into available spaces.
	if( UVSets.Num() > 0 )
	{
		for(INT ChannelNumIdx = 0; ChannelNumIdx < 4; ChannelNumIdx++)
		{
			FString ChannelName = FString::Printf( TEXT("UVChannel_%d"), ChannelNumIdx+1 );
			INT SetIdx = UVSets.FindItemIndex( ChannelName );

			// If the specially formatted UVSet name appears in the list and it is in the wrong spot,
			// we will swap it into the correct spot.
			if( SetIdx != INDEX_NONE && SetIdx != ChannelNumIdx )
			{
				// If we are going to swap to a position that is outside the bounds of the
				// array, then we pad out to that spot with empty data.
				for(INT ArrSize = UVSets.Num(); ArrSize < ChannelNumIdx+1; ArrSize++)
				{
					UVSets.AddItem ( FString(TEXT("")) );
				}
				//Swap the entry into the appropriate spot.
				UVSets.SwapItems( SetIdx, ChannelNumIdx );
			}
		}
	}


	// See if any of our UV set entry names match LightMapUV.
	for(int UVSetIdx = 0; UVSetIdx < UVSets.Num(); UVSetIdx++)
	{
		if( UVSets(UVSetIdx) == TEXT("LightMapUV"))
		{
			StaticMesh->LightMapCoordinateIndex = UVSetIdx;
		}
	}

	//
	// create materials
	//
	INT MaterialCount = 0;
	INT MaterialIndex;
	TArray<UMaterialInterface*> Materials;
	Materials.Empty();
	if ( ImportOptions->bImportMaterials )
	{
		CreateNodeMaterials(Node,Materials,UVSets);
	}
	else if ( ImportOptions->bImportTextures )
	{
		ImportTexturesFromNode(Node);
	}
	
	MaterialCount = Node->GetMaterialCount();
	
	// Used later to offset the material indices on the raw triangle data
	INT MaterialIndexOffset = StaticMesh->LODModels(LODIndex).Elements.Num();

	INT NewMaterialIndex = StaticMesh->LODModels(LODIndex).Elements.Num();
	for (MaterialIndex=0; MaterialIndex<MaterialCount; MaterialIndex++,NewMaterialIndex++)
	{
		FbxSurfaceMaterial *FbxMaterial = Node->GetMaterial(MaterialIndex);
		FString MaterialFullName = ANSI_TO_TCHAR(MakeName(FbxMaterial->GetName()));
		
		if ( !ImportOptions->bImportMaterials )
		{
			UMaterialInterface* UnrealMaterial = FindObject<UMaterialInterface>(Parent,*MaterialFullName);
			Materials.AddItem(UnrealMaterial);
		}
		
		new(StaticMesh->LODModels(LODIndex).Elements) FStaticMeshElement(NULL,NewMaterialIndex);
		StaticMesh->LODModels(LODIndex).Elements(NewMaterialIndex).Name = MaterialFullName;
		StaticMesh->LODModels(LODIndex).Elements(NewMaterialIndex).Material = Materials(MaterialIndex);
		// Enable per poly collision if we do it globally, and this is the first lod index
		StaticMesh->LODModels(LODIndex).Elements(NewMaterialIndex).EnableCollision = GBuildStaticMeshCollision && LODIndex == 0 && ImportOptions->bRemoveDegenerates;
		StaticMesh->LODInfo(LODIndex).Elements.Add();
		StaticMesh->LODInfo(LODIndex).Elements(NewMaterialIndex).Material = Materials(MaterialIndex);
		StaticMesh->LODInfo(LODIndex).Elements(NewMaterialIndex).bEnableShadowCasting = TRUE;
		StaticMesh->LODInfo(LODIndex).Elements(NewMaterialIndex).bSelected = FALSE;
		StaticMesh->LODInfo(LODIndex).Elements(NewMaterialIndex).bEnableCollision = GBuildStaticMeshCollision && LODIndex == 0 && ImportOptions->bRemoveDegenerates;
	}

	if ( MaterialCount == 0 )
	{
		UPackage* EngineMaterialPackage = UObject::FindPackage(NULL,TEXT("EngineMaterials"));
		UMaterial* DefaultMaterial = FindObject<UMaterial>(EngineMaterialPackage,TEXT("DefaultMaterial"));
		if (DefaultMaterial)
		{
			const INT NewMaterialIndex = StaticMesh->LODModels(LODIndex).Elements.Num();
			new(StaticMesh->LODModels(LODIndex).Elements)FStaticMeshElement(NULL,NewMaterialIndex);

			StaticMesh->LODModels(LODIndex).Elements(NewMaterialIndex).Material = DefaultMaterial;
			// Enable per poly collision if we do it globally, and this is the first lod index
			StaticMesh->LODModels(LODIndex).Elements(NewMaterialIndex).EnableCollision = GBuildStaticMeshCollision && LODIndex == 0 && ImportOptions->bRemoveDegenerates;
			StaticMesh->LODInfo(LODIndex).Elements.Add();
			StaticMesh->LODInfo(LODIndex).Elements(NewMaterialIndex).Material = DefaultMaterial;
			StaticMesh->LODInfo(LODIndex).Elements(NewMaterialIndex).bEnableShadowCasting = TRUE;
			StaticMesh->LODInfo(LODIndex).Elements(NewMaterialIndex).bSelected = FALSE;
			StaticMesh->LODInfo(LODIndex).Elements(NewMaterialIndex).bEnableCollision = GBuildStaticMeshCollision && LODIndex == 0 && ImportOptions->bRemoveDegenerates;
		}
	}

	//
	// Convert data format to unreal-compatible
	//

	// Must do this before triangulating the mesh due to an FBX bug in TriangulateMeshAdvance
	INT LayerSmoothingCount = Mesh->GetLayerCount(FbxLayerElement::eSMOOTHING);
	for(INT i = 0; i < LayerSmoothingCount; i++)
	{
		GeometryConverter->ComputePolygonSmoothingFromEdgeSmoothing (Mesh, i);
	}

	UBOOL bDestroyMesh = FALSE;
	if (!Mesh->IsTriangleMesh())
	{
		warnf(NAME_Log,TEXT("Triangulating static mesh %s"), ANSI_TO_TCHAR(Node->GetName()));
		bool bSuccess;
		Mesh = GeometryConverter->TriangulateMeshAdvance(Mesh, bSuccess); // not in place ! the old mesh is still there
		if (Mesh == NULL)
		{
			warnf(NAME_Error,TEXT("Unable to triangulate mesh"));
			return FALSE; // not clean, missing some dealloc
		}
		// this gets deleted at the end of the import
		bDestroyMesh = TRUE;
	}
	
	// renew the base layer
	BaseLayer = Mesh->GetLayer(0);

	//
	//	get the "material index" layer.  Do this AFTER the triangulation step as that may reorder material indices
	//
	FbxLayerElementMaterial* LayerElementMaterial = BaseLayer->GetMaterials();
	FbxLayerElement::EMappingMode MaterialMappingMode = LayerElementMaterial ? 
		LayerElementMaterial->GetMappingMode() : FbxLayerElement::eBY_POLYGON;

	//
	//	store the UVs in arrays for fast access in the later looping of triangles 
	//
	INT UniqueUVCount = UVSets.Num();
	FbxLayerElementUV** LayerElementUV = NULL;
	FbxLayerElement::EReferenceMode* UVReferenceMode = NULL;
	FbxLayerElement::EMappingMode* UVMappingMode = NULL;
	if (UniqueUVCount > 0)
	{
		LayerElementUV = new FbxLayerElementUV*[UniqueUVCount];
		UVReferenceMode = new FbxLayerElement::EReferenceMode[UniqueUVCount];
		UVMappingMode = new FbxLayerElement::EMappingMode[UniqueUVCount];
	}
	LayerCount = Mesh->GetLayerCount();
	for (INT UVIndex = 0; UVIndex < UniqueUVCount; UVIndex++)
	{
		UBOOL bFoundUV = FALSE;
		LayerElementUV[UVIndex] = NULL;
		for (INT UVLayerIndex = 0; !bFoundUV &&UVLayerIndex<LayerCount; UVLayerIndex++)
		{
			FbxLayer* lLayer = Mesh->GetLayer(UVLayerIndex);
			int UVSetCount = lLayer->GetUVSetCount();
			if(UVSetCount)
			{
				FbxArray<FbxLayerElementUV const*> EleUVs = lLayer->GetUVSets();
				for (int FbxUVIndex = 0; FbxUVIndex<UVSetCount; FbxUVIndex++)
				{
					FbxLayerElementUV const* ElementUV = EleUVs[FbxUVIndex];
					if (ElementUV)
					{
						const char* UVSetName = ElementUV->GetName();
						FString LocalUVSetName = ANSI_TO_TCHAR(UVSetName);
						if (LocalUVSetName == UVSets(UVIndex))
						{
							LayerElementUV[UVIndex] = const_cast<FbxLayerElementUV*>(ElementUV);
							UVReferenceMode[UVIndex] = LayerElementUV[FbxUVIndex]->GetReferenceMode();
							UVMappingMode[UVIndex] = LayerElementUV[FbxUVIndex]->GetMappingMode();
							break;
						}
					}
				}
			}
		}
	}


    //
    // get the smoothing group layer
    //
    UBOOL bSmoothingAvailable = FALSE;

    FbxLayerElementSmoothing const* SmoothingInfo = BaseLayer->GetSmoothing();
    FbxLayerElement::EReferenceMode SmoothingReferenceMode(FbxLayerElement::eDIRECT);
    FbxLayerElement::EMappingMode SmoothingMappingMode(FbxLayerElement::eBY_EDGE);
    if (SmoothingInfo)
    {
        if( SmoothingInfo->GetMappingMode() == FbxLayerElement::eBY_EDGE )
        {
            if (!GeometryConverter->ComputePolygonSmoothingFromEdgeSmoothing(Mesh))
            {
                warnf(NAME_Warning,TEXT("Unable to fully convert the smoothing groups for mesh %s"),ANSI_TO_TCHAR(Mesh->GetName()));
                bSmoothingAvailable = FALSE;
            }
        }

		if( SmoothingInfo->GetMappingMode() == FbxLayerElement::eBY_POLYGON )
		{
			bSmoothingAvailable = TRUE;
		}


        SmoothingReferenceMode = SmoothingInfo->GetReferenceMode();
        SmoothingMappingMode = SmoothingInfo->GetMappingMode();
    }

	if(!StaticMesh->LODModels(LODIndex).Elements.Num())
	{
		const INT NewMaterialIndex = StaticMesh->LODModels(LODIndex).Elements.Num();
		new(StaticMesh->LODModels(LODIndex).Elements) FStaticMeshElement(NULL,NewMaterialIndex);
	}

	//
	// get the first vertex color layer
	//
	FbxLayerElementVertexColor* LayerElementVertexColor = BaseLayer->GetVertexColors();
	FbxLayerElement::EReferenceMode VertexColorReferenceMode(FbxLayerElement::eDIRECT);
	FbxLayerElement::EMappingMode VertexColorMappingMode(FbxLayerElement::eBY_CONTROL_POINT);
	if (LayerElementVertexColor)
	{
		VertexColorReferenceMode = LayerElementVertexColor->GetReferenceMode();
		VertexColorMappingMode = LayerElementVertexColor->GetMappingMode();
	}

	//
	// get the first normal layer
	//
	FbxLayerElementNormal* LayerElementNormal = BaseLayer->GetNormals();
    FbxLayerElementTangent* LayerElementTangent = BaseLayer->GetTangents();
    FbxLayerElementBinormal* LayerElementBinormal = BaseLayer->GetBinormals();

    //whether there is normal, tangent and binormal data in this mesh
    UBOOL bHasNTBInformation = LayerElementNormal && LayerElementTangent && LayerElementBinormal;

	FbxLayerElement::EReferenceMode NormalReferenceMode(FbxLayerElement::eDIRECT);
    FbxLayerElement::EMappingMode NormalMappingMode(FbxLayerElement::eBY_CONTROL_POINT);
    if (LayerElementNormal)
    {
        NormalReferenceMode = LayerElementNormal->GetReferenceMode();
        NormalMappingMode = LayerElementNormal->GetMappingMode();
    }

    FbxLayerElement::EReferenceMode TangentReferenceMode(FbxLayerElement::eDIRECT);
    FbxLayerElement::EMappingMode TangentMappingMode(FbxLayerElement::eBY_CONTROL_POINT);
    if (LayerElementTangent)
    {
        TangentReferenceMode = LayerElementTangent->GetReferenceMode();
        TangentMappingMode = LayerElementTangent->GetMappingMode();
    }

	//
	// build collision
	//
	if (ImportCollisionModels(StaticMesh, new FbxString(Node->GetName())))
	{
		INT LODModelIndex;
		for(LODModelIndex=0; LODModelIndex<StaticMesh->LODModels(LODIndex).Elements.Num(); LODModelIndex++)
		{
			StaticMesh->LODModels(LODIndex).Elements(LODModelIndex).EnableCollision = TRUE;
		}
	}

	//
	// build un-mesh triangles
	//

    // Construct the matrices for the conversion from right handed to left handed system
    FbxAMatrix TotalMatrix;
    FbxAMatrix TotalMatrixForNormal;
    TotalMatrix = ComputeTotalMatrix(Node);
    TotalMatrixForNormal = TotalMatrix.Inverse();
    TotalMatrixForNormal = TotalMatrixForNormal.Transpose();

	INT ExistingTris = StaticMesh->LODModels(LODIndex).RawTriangles.GetElementCount();
	INT TriangleCount = Mesh->GetPolygonCount();
	StaticMesh->LODModels(LODIndex).RawTriangles.Lock(LOCK_READ_WRITE);
	FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*)StaticMesh->LODModels(LODIndex).RawTriangles.Realloc( ExistingTris + TriangleCount );

	UBOOL OddNegativeScale = IsOddNegativeScale(TotalMatrix);
	RawTriangleData += ExistingTris;
	INT TriangleIndex;
	for( TriangleIndex = 0 ; TriangleIndex < TriangleCount ; TriangleIndex++ )
	{
		FStaticMeshTriangle*	Triangle = (RawTriangleData++);

        Triangle->bOverrideTangentBasis = bHasNTBInformation && (ImportOptions->bOverrideTangents);
		Triangle->bExplicitNormals = ImportOptions->bExplicitNormals;

        //
        // default vertex colors
        //
        Triangle->Colors[0] = FColor(255,255,255,255); // default value
        Triangle->Colors[1] = FColor(255,255,255,255); // default value
        Triangle->Colors[2] = FColor(255,255,255,255); // default value


		INT VertexIndex;
		for ( VertexIndex=0; VertexIndex<3; VertexIndex++)
		{
			// If there are odd number negative scale, invert the vertex order for triangles
			INT UnrealVertexIndex = OddNegativeScale ? 2 - VertexIndex : VertexIndex;

            int ControlPointIndex = Mesh->GetPolygonVertex(TriangleIndex, VertexIndex);
			FbxVector4 FbxPosition = Mesh->GetControlPoints()[ControlPointIndex];
			FbxVector4 FinalPosition = TotalMatrix.MultT(FbxPosition);
            Triangle->Vertices[UnrealVertexIndex] = Converter.ConvertPos(FinalPosition);

            //
            // normals, tangents and binormals
            //
            if( Triangle->bOverrideTangentBasis || Triangle->bExplicitNormals )
            {
                INT TmpIndex = TriangleIndex*3 + VertexIndex;
                //normals may have different reference and mapping mode than tangents and binormals
                INT NormalMapIndex = (NormalMappingMode == FbxLayerElement::eBY_CONTROL_POINT) ? 
ControlPointIndex : TmpIndex;
                INT NormalValueIndex = (NormalReferenceMode == FbxLayerElement::eDIRECT) ? 
                    NormalMapIndex : LayerElementNormal->GetIndexArray().GetAt(NormalMapIndex);

				if( Triangle->bOverrideTangentBasis )
               	{
					//tangents and binormals share the same reference, mapping mode and index array
                	INT TangentMapIndex = TmpIndex;

               		FbxVector4 TempValue = LayerElementTangent->GetDirectArray().GetAt(TangentMapIndex);
                	Triangle->TangentX[ UnrealVertexIndex ] = Converter.ConvertDir(TempValue);

                	TempValue = LayerElementBinormal->GetDirectArray().GetAt(TangentMapIndex);
                	Triangle->TangentY[ UnrealVertexIndex ] = -Converter.ConvertDir(TempValue);
				}
				else
				{
					Triangle->TangentX[ UnrealVertexIndex ] = FVector( 0.0f, 0.0f, 0.0f );
                    Triangle->TangentY[ UnrealVertexIndex ] = FVector( 0.0f, 0.0f, 0.0f );
				}

				FbxVector4 TempValue = LayerElementNormal->GetDirectArray().GetAt(NormalValueIndex);
              	TempValue = TotalMatrixForNormal.MultT(TempValue);
                Triangle->TangentZ[ UnrealVertexIndex ] = Converter.ConvertDir(TempValue);
            }
            else
            {
                INT NormalIndex;
                for( NormalIndex = 0; NormalIndex < 3; ++NormalIndex )
                {
                    Triangle->TangentX[ NormalIndex ] = FVector( 0.0f, 0.0f, 0.0f );
                    Triangle->TangentY[ NormalIndex ] = FVector( 0.0f, 0.0f, 0.0f );
                    Triangle->TangentZ[ NormalIndex ] = FVector( 0.0f, 0.0f, 0.0f );
                }
            }

            //
            // vertex colors
            //
            if (LayerElementVertexColor && !ExistingVertexColorData)
            {
                INT VertexColorMappingIndex = (VertexColorMappingMode == FbxLayerElement::eBY_CONTROL_POINT) ? 
                    Mesh->GetPolygonVertex(TriangleIndex,VertexIndex) : (TriangleIndex*3+VertexIndex);

                INT VectorColorIndex = (VertexColorReferenceMode == FbxLayerElement::eDIRECT) ? 
                    VertexColorMappingIndex : LayerElementVertexColor->GetIndexArray().GetAt(VertexColorMappingIndex);

                FbxColor VertexColor = LayerElementVertexColor->GetDirectArray().GetAt(VectorColorIndex);

                Triangle->Colors[UnrealVertexIndex] = FColor(	BYTE(255.f*VertexColor.mRed),
                                                        BYTE(255.f*VertexColor.mGreen),
                                                        BYTE(255.f*VertexColor.mBlue),
                                                        BYTE(255.f*VertexColor.mAlpha));
            }
			else if (ExistingVertexColorData)
			{
				// try to match this triangles current vertex with one that existed in the previous mesh.
				// This is a find in a tmap which uses a fast hash table lookup.
				FColor* PaintedColor = ExistingVertexColorData->Find( Triangle->Vertices[VertexIndex] );
				if( PaintedColor )
				{
					// A matching color for this vertex was found
					Triangle->Colors[VertexIndex] = *PaintedColor;
				}
			}
		}

		//
		// smoothing mask
		//
		if ( ! bSmoothingAvailable)
		{
			Triangle->SmoothingMask = 0;
		}
		else
		{
			Triangle->SmoothingMask = 0; // default
			if (SmoothingInfo)
			{
				if (SmoothingMappingMode == FbxLayerElement::eBY_POLYGON)
				{
                    int lSmoothingIndex = (SmoothingReferenceMode == FbxLayerElement::eDIRECT) ? TriangleIndex : SmoothingInfo->GetIndexArray().GetAt(TriangleIndex);
					Triangle->SmoothingMask = SmoothingInfo->GetDirectArray().GetAt(lSmoothingIndex);
				}
				else
				{
					warnf(NAME_Warning,TEXT("Unsupported Smoothing group mapping mode on mesh %s"),ANSI_TO_TCHAR(Mesh->GetName()));
				}
			}
		}

		//
		// uvs
		//
		// In FBX file, the same UV may be saved multiple times, i.e., there may be same UV in LayerElementUV
		// So we don't import the duplicate UVs
		Triangle->NumUVs = Min(UniqueUVCount, 8);
		INT UVLayerIndex;
		for (UVLayerIndex = 0; UVLayerIndex<UniqueUVCount; UVLayerIndex++)
		{
			if (LayerElementUV[UVLayerIndex] != NULL) 
			{
				INT VertexIndex;
				for (VertexIndex=0;VertexIndex<3;VertexIndex++)
				{
					// If there are odd number negative scale, invert the vertex order for triangles
					INT UnrealVertexIndex = OddNegativeScale ? 2 - VertexIndex : VertexIndex;

                    int lControlPointIndex = Mesh->GetPolygonVertex(TriangleIndex, VertexIndex);
                    int UVMapIndex = (UVMappingMode[UVLayerIndex] == FbxLayerElement::eBY_CONTROL_POINT) ? 
lControlPointIndex : TriangleIndex*3+VertexIndex;
                    INT UVIndex = (UVReferenceMode[UVLayerIndex] == FbxLayerElement::eDIRECT) ? 
                        UVMapIndex : LayerElementUV[UVLayerIndex]->GetIndexArray().GetAt(UVMapIndex);
					FbxVector2	UVVector = LayerElementUV[UVLayerIndex]->GetDirectArray().GetAt(UVIndex);

					Triangle->UVs[UnrealVertexIndex][UVLayerIndex].X = static_cast<float>(UVVector[0]);
					Triangle->UVs[UnrealVertexIndex][UVLayerIndex].Y = 1.f-static_cast<float>(UVVector[1]);   //flip the Y of UVs for DirectX
				}
			}
		}

		if (Triangle->NumUVs == 0) // backup, need at least one channel
		{
			Triangle->UVs[0][0].X = 0;
			Triangle->UVs[0][0].Y = 0;

			Triangle->UVs[1][0].X = 0;
			Triangle->UVs[1][0].Y = 0;

			Triangle->UVs[2][0].X = 0;
			Triangle->UVs[2][0].Y = 0;

			Triangle->NumUVs = 1;
		}

		//
		// material index
		//
		Triangle->MaterialIndex = 0; // default value
		if (MaterialCount>0)
		{
			if (LayerElementMaterial)
			{
				switch(MaterialMappingMode)
				{
					// material index is stored in the IndexArray, not the DirectArray (which is irrelevant with 2009.1)
				case FbxLayerElement::eALL_SAME:
					{	
						Triangle->MaterialIndex = LayerElementMaterial->GetIndexArray().GetAt(0) + MaterialIndexOffset;
					}
					break;
				case FbxLayerElement::eBY_POLYGON:
					{	
						Triangle->MaterialIndex = LayerElementMaterial->GetIndexArray().GetAt(TriangleIndex) + MaterialIndexOffset;
					}
					break;
				}

				if (Triangle->MaterialIndex >= MaterialCount + MaterialIndexOffset || Triangle->MaterialIndex < 0)
				{
					warnf(NAME_Warning,TEXT("Face material index inconsistency - forcing to 0"));
					Triangle->MaterialIndex = 0;
				}
			}
		}

		//
		// fragments - TODO
		//
		Triangle->FragmentIndex = 0;
	}

	//
	// complete build
	//
	StaticMesh->LODModels(LODIndex).RawTriangles.Unlock();
	
	//
	// clean up
	//
	if (UniqueUVCount > 0)
	{
		delete[] LayerElementUV;
		delete[] UVReferenceMode;
		delete[] UVMappingMode;
	}

	if (bDestroyMesh)
	{
		Mesh->Destroy(true);
	}

	return TRUE;
}

UObject* UnFbx::CFbxImporter::ReimportStaticMesh(UStaticMesh* Mesh)
{
	char MeshName[1024];
	appStrcpy(MeshName,1024,TCHAR_TO_ANSI(*Mesh->GetName()));
	TArray<FbxNode*> FbxMeshArray;
	FbxNode* Node = NULL;
	UObject* NewMesh = NULL;
	
	// get meshes in Fbx file
	//the function also fill the collision models, so we can update collision models correctly
	FillFbxMeshArray(Scene->GetRootNode(), FbxMeshArray, this);
	
	// if there is only one mesh, use it without name checking 
	// (because the "Used As Full Name" option enables users name the Unreal mesh by themselves
	if (FbxMeshArray.Num() == 1)
	{
		Node = FbxMeshArray(0);
	}
	else
	{
		// find the Fbx mesh node that the Unreal Mesh matches according to name
		INT MeshIndex;
		for ( MeshIndex = 0; MeshIndex < FbxMeshArray.Num(); MeshIndex++ )
		{
			const char* FbxMeshName = FbxMeshArray(MeshIndex)->GetName();
			// The name of Unreal mesh may have a prefix, so we match from end
			UINT i = 0;
			char* MeshPtr = MeshName + strlen(MeshName) - 1;
			if (strlen(FbxMeshName) <= strlen(MeshName))
			{
				const char* FbxMeshPtr = FbxMeshName + strlen(FbxMeshName) - 1;
				while (i < strlen(FbxMeshName))
				{
					if (*MeshPtr != *FbxMeshPtr)
					{
						break;
					}
					else
					{
						i++;
						MeshPtr--;
						FbxMeshPtr--;
					}
				}
			}

			if (i == strlen(FbxMeshName)) // matched
			{
				// check further
				if ( strlen(FbxMeshName) == strlen(MeshName) ||  // the name of Unreal mesh is full match
					*MeshPtr == '_')							 // or the name of Unreal mesh has a prefix
				{
					Node = FbxMeshArray(MeshIndex);
					break;
				}
			}
		}
	}
	
	if (Node)
	{
		FbxNode* Parent = Node->GetParent();
		// set import options, how about others?
		ImportOptions->bImportMaterials = FALSE;
		ImportOptions->bImportTextures = FALSE;
		
		// if the Fbx mesh is a part of LODGroup, update LOD
		if (Parent && Parent->GetNodeAttribute() && Parent->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGROUP)
		{
			NewMesh = ImportStaticMesh(Mesh->GetOuter(), Parent->GetChild(0), *Mesh->GetName(), RF_Public|RF_Standalone, Mesh, 0);
			if (NewMesh)
			{
				// import LOD meshes
				for (INT LODIndex = 1; LODIndex < Parent->GetChildCount(); LODIndex++)
				{
					ImportStaticMesh(Mesh->GetOuter(), Parent->GetChild(LODIndex), *Mesh->GetName(), RF_Public|RF_Standalone, Mesh, LODIndex);
				}
			}
		}
		else
		{
			NewMesh = ImportStaticMesh(Mesh->GetOuter(), Node, *Mesh->GetName(), RF_Public|RF_Standalone, Mesh, 0);
		}
	}
	else
	{
		// no FBX mesh match, maybe the Unreal mesh is imported from multiple FBX mesh (enable option "Import As Single")
		if (FbxMeshArray.Num() > 0)
		{
			NewMesh = ImportStaticMeshAsSingle(Mesh->GetOuter(), FbxMeshArray, *Mesh->GetName(), RF_Public|RF_Standalone, Mesh, 0);
		}
		else // no mesh found in the FBX file
		{
			warnf(NAME_Log,TEXT("No FBX mesh found when reimport Unreal mesh %s. The FBX file is crashed."), *Mesh->GetName());
		}
	}

	return NewMesh;
}

UObject* UnFbx::CFbxImporter::ImportStaticMeshAsSingle(UObject* InParent, TArray<FbxNode*>& MeshNodeArray, const FName& Name, EObjectFlags Flags, UStaticMesh* InStaticMesh, int LODIndex)
{
	UBOOL bBuildStatus = TRUE;
	struct ExistingStaticMeshData* ExistMeshDataPtr = NULL;

	// Make sure rendering is done - so we are not changing data being used by collision drawing.
	FlushRenderingCommands();

	if (MeshNodeArray.Num() == 0)
	{
		return NULL;
	}
	
	// Ensure that the vert indicies can fit into a 16-bit integer
	INT NumVerts = 0;
	INT MeshIndex;
	for (MeshIndex = 0; MeshIndex < MeshNodeArray.Num(); MeshIndex++ )
	{
		FbxNode* Node = MeshNodeArray(MeshIndex);
		FbxMesh* FbxMesh = Node->GetMesh();

		if (FbxMesh)
		{
			NumVerts += FbxMesh->GetControlPointsCount();
			if (NumVerts > 65535)
			{
				appMsgf(AMT_OK, *LocalizeUnrealEd("StaticMeshImport_TooManyVertices_FBX"));
				return NULL;
			}

			// If not combining meshes, reset the vert count between meshes
			if (!ImportOptions->bCombineToSingle)
			{
				NumVerts = 0;
			}
		}
	}

	Parent = InParent;
	
	// warning for missing smoothing group info
	CheckSmoothingInfo(MeshNodeArray(0)->GetMesh());

	// create empty mesh
	UStaticMesh*	StaticMesh = NULL;

	UStaticMesh* ExistingMesh = NULL;
	// A mapping of vertex positions to their color in the existing static mesh
	TMap<FVector, FColor>		ExistingVertexColorData;
	// If we should get vertex colors. Defaults to the checkbox value available to the user in the import property window
	UBOOL bGetVertexColors = ImportOptions->bReplaceVertexColors;

	if( InStaticMesh == NULL || LODIndex == 0 )
	{
		ExistingMesh = FindObject<UStaticMesh>( InParent, *Name.ToString() );
		
	}
	if (ExistingMesh)
	{
		// What LOD to get vertex colors from.  
		// Currently mesh painting only allows for painting on the first lod.
		const UINT PaintingMeshLODIndex = 0;

		const FStaticMeshRenderData& ExistingRenderData = ExistingMesh->LODModels( PaintingMeshLODIndex );

		// Nothing to copy if there are no colors stored.
		if( ExistingRenderData.ColorVertexBuffer.GetNumVertices() > 0 )
		{
			// Build a mapping of vertex positions to vertex colors.  Using a TMap will allow for fast lookups so we can match new static mesh vertices with existing colors 
			const FPositionVertexBuffer& ExistingPositionVertexBuffer = ExistingRenderData.PositionVertexBuffer;
			for( UINT PosIdx = 0; PosIdx < ExistingPositionVertexBuffer.GetNumVertices(); ++PosIdx )
			{
				const FVector& VertexPos = ExistingPositionVertexBuffer.VertexPosition( PosIdx );

				// Make sure this vertex doesnt already have an assigned color.  
				// If the static mesh had shadow volume verts, the position buffer holds duplicate vertices
				if( ExistingVertexColorData.Find( VertexPos ) == NULL )
				{
					ExistingVertexColorData.Set( VertexPos, ExistingRenderData.ColorVertexBuffer.VertexColor( PosIdx ) );
				}
			}
		}
		else
		{
			// If there were no vertex colors, automatically take vertex colors from the file.
			bGetVertexColors = TRUE;
		}

		// Free any RHI resources for existing mesh before we re-create in place.
		ExistingMesh->PreEditChange(NULL);
		ExistMeshDataPtr = SaveExistingStaticMeshData(ExistingMesh);
	}
	else
	{
		// Vertex colors should be copied always if there is no existing static mesh.
		bGetVertexColors = TRUE;
	}
	
	if( InStaticMesh != NULL && LODIndex > 0 )
	{
		StaticMesh = InStaticMesh;
	}
	else
	{
		StaticMesh = new(InParent,Name,Flags|RF_Public) UStaticMesh;
	}


	if(StaticMesh->LODModels.Num() < LODIndex+1)
	{
		// Add one LOD 
		new(StaticMesh->LODModels) FStaticMeshRenderData();
		StaticMesh->LODInfo.AddItem(FStaticMeshLODInfo());
		
		if (StaticMesh->LODModels.Num() < LODIndex+1)
		{
			LODIndex = StaticMesh->LODModels.Num() - 1;
		}
	}
	
	StaticMesh->SourceFilePath = GFileManager->ConvertToRelativePath(*UFactory::CurrentFilename);

	FFileManager::FTimeStamp Timestamp;
	if (GFileManager->GetTimestamp( *UFactory::CurrentFilename, Timestamp ))
	{
		FFileManager::FTimeStamp::TimestampToFString(Timestamp, /*out*/ StaticMesh->SourceFileTimestamp);
	}
	
	// make sure it has a new lighting guid
	StaticMesh->LightingGuid = appCreateGuid();

	// Set it to use textured lightmaps. Note that Build Lighting will do the error-checking (texcoordindex exists for all LODs, etc).
	StaticMesh->LightMapResolution = 32;
	StaticMesh->LightMapCoordinateIndex = 1;

	for (MeshIndex = 0; MeshIndex < MeshNodeArray.Num(); MeshIndex++ )
	{
		FbxNode* Node = MeshNodeArray(MeshIndex);

		if (Node->GetMesh())
		{
			if (!BuildStaticMeshFromGeometry(Node->GetMesh(), StaticMesh, LODIndex, bGetVertexColors? NULL: &ExistingVertexColorData))
			{
				bBuildStatus = FALSE;
				break;
			}
		}
	}

	if (bBuildStatus)
	{
		// Compress the materials array by removing any duplicates.
		for(INT k=0;k<StaticMesh->LODModels.Num();k++)
		{
			TArray<FStaticMeshElement> SaveElements = StaticMesh->LODModels(k).Elements;
			StaticMesh->LODModels(k).Elements.Empty();

			for( INT x = 0 ; x < SaveElements.Num() ; ++x )
			{
				// See if this material is already in the list.  If so, loop through all the raw triangles
				// and change the material index to point to the one already in the list.

				INT newidx = INDEX_NONE;
				for( INT y = 0 ; y < StaticMesh->LODModels(k).Elements.Num() ; ++y )
				{
					if( StaticMesh->LODModels(k).Elements(y).Name == SaveElements(x).Name )
					{
						newidx = y;
						break;
					}
				}

				if( newidx == INDEX_NONE )
				{
					newidx = StaticMesh->LODModels(k).Elements.AddItem( SaveElements(x) );
					StaticMesh->LODModels(k).Elements(newidx).MaterialIndex = newidx;
				}

				FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*) StaticMesh->LODModels(k).RawTriangles.Lock(LOCK_READ_WRITE);
				for( INT t = 0 ; t < StaticMesh->LODModels(k).RawTriangles.GetElementCount() ; ++t )
				{
					if( RawTriangleData[t].MaterialIndex == x )
					{
						RawTriangleData[t].MaterialIndex = newidx;
					}
				}
				StaticMesh->LODModels(k).RawTriangles.Unlock();
			}
		}

		SetMaterialSkinXXOrder(StaticMesh);

		if (ExistMeshDataPtr)
		{
			RestoreExistingMeshData(ExistMeshDataPtr, StaticMesh);
		}

		StaticMesh->bRemoveDegenerates = ImportOptions->bRemoveDegenerates;
		StaticMesh->Build(FALSE, FALSE);
		
		// Warn about bad light map UVs if we have any
		{
			TArray< FString > MissingUVSets;
			TArray< FString > BadUVSets;
			TArray< FString > ValidUVSets;
			UStaticMesh::CheckLightMapUVs( StaticMesh, MissingUVSets, BadUVSets, ValidUVSets );

			// NOTE: We don't care about missing UV sets here, just bad ones!
// 			if( BadUVSets.Num() > 0 )
// 			{
// 				appMsgf( AMT_OK, LocalizeSecure( LocalizeUnrealEd( "Error_NewStaticMeshHasBadLightMapUVSet_F" ), *Name.ToString() ) );
// 			}
		}

 		// Warn about any bad mesh elements
 		if (UStaticMesh::RemoveZeroTriangleElements(StaticMesh, FALSE) == TRUE)
		{
			// Build it again...
			StaticMesh->Build(FALSE, TRUE);
		}
	}
	else
	{
		StaticMesh = NULL;
	}

	return StaticMesh;
}


void UnFbx::CFbxImporter::SetMaterialSkinXXOrder(UStaticMesh* StaticMesh)
{
	for(INT k=0;k<StaticMesh->LODModels.Num();k++)
	{
		TArray<INT> SkinIndex;
		TArray<INT> MaterialIndexMapping;

		INT MaterialCount = StaticMesh->LODModels(k).Elements.Num();
		SkinIndex.Add(MaterialCount);
		//MaterialIndexMapping(MaterialCount);
		for(INT MaterialIndex=0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			// get skin index
			UBOOL Found = FALSE;
			INT MatNameLen = appStrlen(*StaticMesh->LODModels(k).Elements(MaterialIndex).Name) + 1;
			char* MatName = new char[MatNameLen];
			appStrcpyANSI(MatName, MatNameLen, TCHAR_TO_ANSI(*StaticMesh->LODModels(k).Elements(MaterialIndex).Name));
			if (strlen(MatName) > 6)
			{
				const char* SkinXX = MatName + strlen(MatName) - 6;
				if (toupper(*SkinXX) == 'S' && toupper(*(SkinXX+1)) == 'K' && toupper(*(SkinXX+2)) == 'I' && toupper(*(SkinXX+3)) == 'N')
				{
					if (isdigit(*(SkinXX+4)) && isdigit(*(SkinXX+5)))
					{
						Found = TRUE;

						INT TmpIndex = (*(SkinXX+4) - 0x30) * 10 + (*(SkinXX+5) - 0x30);
						SkinIndex(MaterialIndex) = TmpIndex;
						
						// remove the 'skinXX' suffix from the material name
						INT MatNameLen = appStrlen(*StaticMesh->LODModels(k).Elements(MaterialIndex).Name);
						StaticMesh->LODModels(k).Elements(MaterialIndex).Name = StaticMesh->LODModels(k).Elements(MaterialIndex).Name.Left(MatNameLen - 7);
					}
				}
			}
			if (!Found)
			{
				SkinIndex(MaterialIndex) = 0;
			}

			INT i;
			for ( i=0; i<MaterialIndex; i++)
			{
				if ( SkinIndex(MaterialIndexMapping(i)) > SkinIndex(MaterialIndex) )
				{
					MaterialIndexMapping.InsertItem(MaterialIndex, i);
					break;
				}
			}

			if ( i == MaterialIndex )
			{
				MaterialIndexMapping.AddItem(MaterialIndex);
			}
			delete [] MatName;
		}

		// check if reorder is needed
		UBOOL bNeedReorder = FALSE;
		for(INT MaterialIndex=0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			if (MaterialIndexMapping(MaterialIndex) != MaterialIndex)
			{
				bNeedReorder = TRUE;
				break;
			}
		}

		if (bNeedReorder)
		{
			// re-order the materials
			TArray<FStaticMeshElement> SaveElements = StaticMesh->LODModels(k).Elements;
			StaticMesh->LODModels(k).Elements.Empty();

			for(INT MaterialIndex=0; MaterialIndex < MaterialCount; ++MaterialIndex)
			{
				StaticMesh->LODModels(k).Elements.AddItem(SaveElements(MaterialIndexMapping(MaterialIndex)));
			}

			FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*) StaticMesh->LODModels(k).RawTriangles.Lock(LOCK_READ_WRITE);
			for( INT t = 0 ; t < StaticMesh->LODModels(k).RawTriangles.GetElementCount() ; ++t )
			{
				RawTriangleData[t].MaterialIndex = MaterialIndexMapping(RawTriangleData[t].MaterialIndex);
			}
			StaticMesh->LODModels(k).RawTriangles.Unlock();
		}
	}
}


UBOOL UnFbx::CFbxImporter::FillCollisionModelList(FbxNode* Node)
{
	FbxString* NodeName = new FbxString(Node->GetName());
	if ( NodeName->Find("UCX") == 0 || NodeName->Find("MCDCX") == 0 ||
		 NodeName->Find("UBX") == 0 || NodeName->Find("USP") == 0 )
	{
		// Get name of static mesh that the collision model connect to
		UINT StartIndex = NodeName->Find('_') + 1;
		INT TmpEndIndex = NodeName->Find('_', StartIndex);
		INT EndIndex = TmpEndIndex;
		// Find the last '_' (underscore)
		while (TmpEndIndex >= 0)
		{
			EndIndex = TmpEndIndex;
			TmpEndIndex = NodeName->Find('_', EndIndex+1);
		}
		
		const INT NumMeshNames = 2;
		FbxString MeshName[NumMeshNames];
		if ( EndIndex >= 0 )
		{
			// all characters between the first '_' and the last '_' are the FBX mesh name
			// convert the name to upper because we are case insensitive
			MeshName[0] = NodeName->Mid(StartIndex, EndIndex - StartIndex).Upper();
			
			// also add a version of the mesh name that includes what follows the last '_'
			// in case that's not a suffix but, instead, is part of the mesh name
			if (StartIndex < (INT)NodeName->GetLen())
			{            
				MeshName[1] = NodeName->Mid(StartIndex).Upper();
			}
		}
		else if (StartIndex < (INT)NodeName->GetLen())
		{            
			MeshName[0] = NodeName->Mid(StartIndex).Upper();
		}

		for (INT NameIdx = 0; NameIdx < NumMeshNames; ++NameIdx)
		{
			if ((INT)MeshName[NameIdx].GetLen() > 0)
			{
				FbxMap<FbxString, FbxArray<FbxNode* >* >::RecordType const *Models = CollisionModels.Find(MeshName[NameIdx]);
				FbxArray<FbxNode* >* Record;
				if ( !Models )
				{
					Record = new FbxArray<FbxNode*>();
					CollisionModels.Insert(MeshName[NameIdx], Record);
				}
				else
				{
					Record = Models->GetValue();
				}
				Record->Add(Node);
			}
		}

		return TRUE;
	}

	return FALSE;
}

extern void AddConvexGeomFromVertices( const TArray<FVector>& Verts, FKAggregateGeom* AggGeom, const TCHAR* ObjName );
extern void AddSphereGeomFromVerts( const TArray<FVector>& Verts, FKAggregateGeom* AggGeom, const TCHAR* ObjName );
extern void AddBoxGeomFromTris( const TArray<FPoly>& Tris, FKAggregateGeom* AggGeom, const TCHAR* ObjName );
extern void DecomposeUCXMesh( const TArray<FVector>& CollisionVertices, const TArray<INT>& CollisionFaceIdx, FKAggregateGeom& AggGeom );

UBOOL UnFbx::CFbxImporter::ImportCollisionModels(UStaticMesh* StaticMesh, FbxString* NodeName)
{
	// find collision models
	UBOOL bRemoveEmptyKey = FALSE;
	FbxString EmptyKey;

	// convert the name to upper because we are case insensitive
	FbxMap<FbxString, FbxArray<FbxNode* >* >::RecordType const *Record = CollisionModels.Find(NodeName->Upper());
	if ( !Record )
	{
		// compatible with old collision name format
		// if CollisionModels has only one entry and the key is ""
		if ( CollisionModels.GetSize() == 1 )
		{
			Record = CollisionModels.Find( EmptyKey );
		}
		if ( !Record ) 
		{
			return FALSE;
		}
		else
		{
			bRemoveEmptyKey = TRUE;
		}
	}

	FbxArray<FbxNode*>* Models = Record->GetValue();
	if( !StaticMesh->BodySetup )
	{
		StaticMesh->BodySetup = ConstructObject<URB_BodySetup>(URB_BodySetup::StaticClass(), StaticMesh);
	}    

	TArray<FVector>	CollisionVertices;
	TArray<INT>		CollisionFaceIdx;

	// construct collision model
	for (INT i=0; i<Models->GetCount(); i++)
	{
		FbxNode* Node = Models->GetAt(i);
		FbxMesh* FbxMesh = Node->GetMesh();

		FbxMesh->RemoveBadPolygons();
		UBOOL bDestroyMesh = FALSE;

		// Must do this before triangulating the mesh due to an FBX bug in TriangulateMeshAdvance
		INT LayerSmoothingCount = FbxMesh->GetLayerCount(FbxLayerElement::eSMOOTHING);
		for(INT i = 0; i < LayerSmoothingCount; i++)
		{
			GeometryConverter->ComputePolygonSmoothingFromEdgeSmoothing (FbxMesh, i);
		}

		if (!FbxMesh->IsTriangleMesh())
		{
			FString NodeName = ANSI_TO_TCHAR(MakeName(Node->GetName()));
			warnf(NAME_Log,TEXT("Triangulating mesh %s for collision model"), *NodeName);
			bool bSuccess;
			FbxMesh = GeometryConverter->TriangulateMeshAdvance(FbxMesh, bSuccess); // not in place ! the old mesh is still there
			if (FbxMesh == NULL)
			{
				warnf(NAME_Error,TEXT("Unable to triangulate mesh"));
				return FALSE; // not clean, missing some dealloc
			}
			// this gets deleted at the end of the import
			bDestroyMesh = TRUE;
		}

		INT ControlPointsIndex;
		INT ControlPointsCount = FbxMesh->GetControlPointsCount();
		FbxVector4* ControlPoints = FbxMesh->GetControlPoints();
		FbxAMatrix Matrix = ComputeTotalMatrix(Node);

		for ( ControlPointsIndex = 0; ControlPointsIndex < ControlPointsCount; ControlPointsIndex++ )
		{
			new(CollisionVertices)FVector( Converter.ConvertPos(Matrix.MultT(ControlPoints[ControlPointsIndex])) );
		}

		TArray<FPoly> CollisionTriangles;

		INT TriangleCount = FbxMesh->GetPolygonCount();
		INT TriangleIndex;
		for ( TriangleIndex = 0 ; TriangleIndex < TriangleCount ; TriangleIndex++ )
		{
			new(CollisionFaceIdx)INT(FbxMesh->GetPolygonVertex(TriangleIndex,0));
			new(CollisionFaceIdx)INT(FbxMesh->GetPolygonVertex(TriangleIndex,1));
			new(CollisionFaceIdx)INT(FbxMesh->GetPolygonVertex(TriangleIndex,2));
		}

		FKAggregateGeom& AggGeo = StaticMesh->BodySetup->AggGeom;

		// Construct geometry object
		FbxString *ModelName = new FbxString(Node->GetName());
		if ( ModelName->Find("UCX") == 0 || ModelName->Find("MCDCX") == 0 )
		{
			if( !ImportOptions->bOneConvexHullPerUCX )
			{
				DecomposeUCXMesh( CollisionVertices, CollisionFaceIdx, AggGeo );
			}
			else
			{
				// Make triangles
				for(INT x = 0;x < CollisionFaceIdx.Num();x += 3)
				{
					FPoly*	Poly = new( CollisionTriangles ) FPoly();

					Poly->Init();

					new(Poly->Vertices) FVector( CollisionVertices(CollisionFaceIdx(x + 2)) );
					new(Poly->Vertices) FVector( CollisionVertices(CollisionFaceIdx(x + 1)) );
					new(Poly->Vertices) FVector( CollisionVertices(CollisionFaceIdx(x + 0)) );
					Poly->iLink = x / 3;

					Poly->CalcNormal(1);
				}

				// This function cooks the given data, so we cannot test for duplicates based on the position data
				// before we call it
				AddConvexGeomFromVertices( CollisionVertices, &AggGeo, (const TCHAR*)Node->GetName() );

				// Now test the late element in the AggGeo list and remove it if its a duplicate
				if(AggGeo.ConvexElems.Num() > 1)
				{
					FKConvexElem& NewElem = AggGeo.ConvexElems.Last();

					for(INT ElementIndex = 0; ElementIndex < AggGeo.ConvexElems.Num()-1; ++ElementIndex)
					{
						FKConvexElem& CurrentElem = AggGeo.ConvexElems(ElementIndex);

						if(CurrentElem.VertexData.Num() == NewElem.VertexData.Num())
						{
							UBOOL bFoundDifference = FALSE;
							for(INT i = 0; i < NewElem.VertexData.Num(); ++i)
							{
								if(CurrentElem.VertexData(i) != NewElem.VertexData(i))
								{
									bFoundDifference = TRUE;
									break;
								}
							}

							if(!bFoundDifference)
							{
								// The new collision geo is a duplicate, delete it
								AggGeo.ConvexElems.Remove(AggGeo.ConvexElems.Num()-1);
								break;
							}
						}
					}
				}
			}
		}
		else if ( ModelName->Find("UBX") == 0 )
		{
			FKAggregateGeom& AggGeo = StaticMesh->BodySetup->AggGeom;

			TArray<FPoly> CollisionTriangles;

			// Make triangles
			for(INT x = 0;x < CollisionFaceIdx.Num();x += 3)
			{
				FPoly*	Poly = new( CollisionTriangles ) FPoly();

				Poly->Init();

				new(Poly->Vertices) FVector( CollisionVertices(CollisionFaceIdx(x + 2)) );
				new(Poly->Vertices) FVector( CollisionVertices(CollisionFaceIdx(x + 1)) );
				new(Poly->Vertices) FVector( CollisionVertices(CollisionFaceIdx(x + 0)) );
				Poly->iLink = x / 3;

				Poly->CalcNormal(1);
			}

			AddBoxGeomFromTris( CollisionTriangles, &AggGeo, (const TCHAR*)Node->GetName() );

			// Now test the late element in the AggGeo list and remove it if its a duplicate
			if(AggGeo.BoxElems.Num() > 1)
			{
				FKBoxElem& NewElem = AggGeo.BoxElems.Last();

				for(INT ElementIndex = 0; ElementIndex < AggGeo.BoxElems.Num()-1; ++ElementIndex)
				{
					FKBoxElem& CurrentElem = AggGeo.BoxElems(ElementIndex);

					if(	CurrentElem.TM == NewElem.TM &&
						CurrentElem.X == NewElem.X &&
						CurrentElem.Y == NewElem.Y &&
						CurrentElem.Z == NewElem.Z )
					{
						// The new element is a duplicate, remove it
						AggGeo.BoxElems.Remove(AggGeo.BoxElems.Num()-1);
						break;
					}
				}
			}
		}
		else if ( ModelName->Find("USP") == 0 )
		{
			FKAggregateGeom& AggGeo = StaticMesh->BodySetup->AggGeom;

			AddSphereGeomFromVerts( CollisionVertices, &AggGeo, (const TCHAR*)Node->GetName() );

			// Now test the late element in the AggGeo list and remove it if its a duplicate
			if(AggGeo.SphereElems.Num() > 1)
			{
				FKSphereElem& NewElem = AggGeo.SphereElems.Last();

				for(INT ElementIndex = 0; ElementIndex < AggGeo.SphereElems.Num()-1; ++ElementIndex)
				{
					FKSphereElem& CurrentElem = AggGeo.SphereElems(ElementIndex);

					if(	CurrentElem.TM == NewElem.TM &&
						CurrentElem.Radius == NewElem.Radius )
					{
						// The new element is a duplicate, remove it
						AggGeo.SphereElems.Remove(AggGeo.SphereElems.Num()-1);
						break;
					}
				}
			}
		}

		// Clear any cached rigid-body collision shapes for this body setup.
		StaticMesh->BodySetup->ClearShapeCache();

		if (bDestroyMesh)
		{
			FbxMesh->Destroy();
		}

		// Remove the empty key because we only use the model once for the first mesh
		if (bRemoveEmptyKey)
		{
			CollisionModels.Remove(EmptyKey);
		}

		CollisionVertices.Empty();
		CollisionFaceIdx.Empty();
	}
		
	return TRUE;
}

#endif // WITH_FBX
