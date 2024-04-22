/*=============================================================================
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "UnrealEd.h"
#include "Factories.h"

#include "EnginePrivate.h"
#include "ExportMeshUtils.h"

#if WITH_FBX
#include "UnFbxExporter.h"
#endif

#if _WINDOWS
#include "../../D3D9Drv/Inc/D3D9Drv.h"
#endif

namespace ExportMeshUtils
{
	/*
	*Writes out all StaticMeshes passed in (and all LoDs) for light map editing
	* @param InAllMeshes - All Static Mesh objects that need their LOD's lightmaps to be exported
	* @param InDirectory - The directory to attempt the export to
	* @return If any errors occured during export
	*/
	UBOOL ExportAllLightmapModels (TArray<UStaticMesh*>& InAllMeshes, const FString& InDirectory, UBOOL IsFBX)
	{
		UBOOL bAnyErrorOccured = FALSE;
		for (INT MeshIndex = 0; MeshIndex < InAllMeshes.Num(); ++MeshIndex)
		{
			UStaticMesh* CurrentMesh = InAllMeshes(MeshIndex);
			check (CurrentMesh);

			if (IsWithin(CurrentMesh->LightMapCoordinateIndex, 0, (INT)MAX_TEXCOORDS))
			{
				for (INT LODIndex = 0; LODIndex < CurrentMesh->LODModels.Num(); ++LODIndex)
				{	
#if WITH_FBX
					if (IsFBX)
					{
						FFilename Filename = InDirectory + TEXT("\\") + CurrentMesh->GetName() + FString::Printf( TEXT("_UVs_LOD_%d.FBX"), LODIndex);

						bAnyErrorOccured |= ExportSingleLightmapModelFBX(CurrentMesh, Filename, LODIndex);
					}
					else
#endif
					{
						FFilename Filename = InDirectory + TEXT("\\") + CurrentMesh->GetName() + FString::Printf( TEXT("_UVs_LOD_%d.OBJ"), LODIndex);
						UBOOL bDisableBackup = TRUE;
						FOutputDevice* Buffer = new FOutputDeviceFile(*Filename, bDisableBackup);
						if (Buffer)
						{
							Buffer->SetSuppressEventTag(TRUE);
							Buffer->SetAutoEmitLineTerminator(FALSE);

							bAnyErrorOccured |= ExportSingleLightmapModel (CurrentMesh, Buffer, LODIndex);

							Buffer->TearDown();
							//GFileManager->Move(ExportParams.Filename, *TempFile, 1, 1);  //if we were to rename the file

						} 
						else 
						{
							GWarn->Log(*(LocalizeUnrealEd("StaticMeshEditor_UnableToLoadFile") + FString::Printf( TEXT("(%s)"), *Filename )));
							bAnyErrorOccured = TRUE;
						}
					}
				}
			}
			else
			{
				bAnyErrorOccured |= TRUE;
				GWarn->Logf( LocalizeSecure(LocalizeUnrealEd("StaticMeshEditor_InvalidLightMapCoordinateIndex"), *CurrentMesh->GetName(), CurrentMesh->LightMapCoordinateIndex));
			}
		}
		return bAnyErrorOccured;
	}


	/*
	*Reads in all StaticMeshes passed in (and all LoDs) for light map editing
	* @param InAllMeshes - All Static Mesh objects that need their LOD's lightmaps to be imported
	* @param InDirectory - The directory to attempt the import from
	* @return If any errors occured during import
	*/
	UBOOL ImportAllLightmapModels (TArray<UStaticMesh*>& InAllMeshes, const FString& InDirectory, UBOOL IsFBX)
	{
		UBOOL bAnyErrorOccured = FALSE;
		for (INT MeshIndex = 0; MeshIndex < InAllMeshes.Num(); ++MeshIndex)
		{
			UStaticMesh* CurrentMesh = InAllMeshes(MeshIndex);
			check (CurrentMesh);

			if (IsWithin(CurrentMesh->LightMapCoordinateIndex, 0, (INT)MAX_TEXCOORDS))
			{
				for (INT LODIndex = 0; LODIndex < CurrentMesh->LODModels.Num(); ++LODIndex)
				{
#if WITH_FBX
					if (IsFBX)
					{
						FFilename Filename = InDirectory + TEXT("\\") + CurrentMesh->GetName() + FString::Printf( TEXT("_UVs_LOD_%d.FBX"), LODIndex);

						bAnyErrorOccured |= ImportSingleLightmapModelFBX(CurrentMesh, Filename, LODIndex);						
					}
					else
#endif
					{
#if WITH_ACTORX
						FFilename Filename = InDirectory + TEXT("\\") + CurrentMesh->GetName() + FString::Printf( TEXT("_UVs_LOD_%d.ASE"), LODIndex);

						FString Data;
						if( appLoadFileToString( Data, *Filename ) )
						{
							bAnyErrorOccured |= ImportSingleLightmapModelASE(CurrentMesh, Data, LODIndex);
						}
						else 
						{
							GWarn->Log(*(LocalizeUnrealEd("StaticMeshEditor_UnableToLoadFile") + FString::Printf( TEXT("(%s)"), *Filename )));
							bAnyErrorOccured = TRUE;
						}
#else
						appMsgf( AMT_OK, *LocalizeUnrealEd("Error_ActorXDeprecated") );
#endif
					}
				}
			}
			else
			{
				bAnyErrorOccured |= TRUE;
				GWarn->Logf( LocalizeSecure(LocalizeUnrealEd("StaticMeshEditor_InvalidLightMapCoordinateIndex"), *CurrentMesh->GetName(), CurrentMesh->LightMapCoordinateIndex));
			}
		}
		return bAnyErrorOccured;
	}


	/*
	*Writes out a single LOD of a StaticMesh(LODIndex) for light map editing
	* @param InStaticMesh - The mesh to be exported
	* @param InOutputDevice - The output device to write the data to.  Current usage is a FOutputDeviceFile
	* @param InLODIndex - The LOD of the model we want exported
	* @return If any errors occured during export
	*/
	UBOOL ExportSingleLightmapModel (UStaticMesh* InStaticMesh, FOutputDevice* InOutputDevice, const INT InLODIndex)
	{
		UBOOL bAnyErrorOccured = FALSE;

		check(InStaticMesh);
		check(InLODIndex < InStaticMesh->LODModels.Num());
		check(InOutputDevice);

		INT LightMapChannelIndex = InStaticMesh->LightMapCoordinateIndex;

		//OBJ File Header
		InOutputDevice->Log( TEXT("# UnrealEd Lightmap OBJ exporter\r\n") );


		// Collect all the data about the mesh
		FStaticMeshRenderData* MeshRenderData = InStaticMesh->GetLODForExport(InLODIndex);
		if (MeshRenderData == NULL)
		{
			return FALSE;
		}
		const FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*) MeshRenderData->RawTriangles.Lock(LOCK_READ_ONLY);

		//inform the user on export
		if (MeshRenderData->RawTriangles.GetElementCount())
		{
			const INT LightMapChannelIndex = InStaticMesh->LightMapCoordinateIndex;
			const INT MaxUVS = RawTriangleData[0].NumUVs;

			//if no light map has been authored yet, OR the light map is out of bounds of the current NumUVS
			if ((LightMapChannelIndex==0) || (LightMapChannelIndex >= MaxUVS)) 
			{
				GWarn->Logf( LocalizeSecure(LocalizeUnrealEd("StaticMeshEditor_InvalidLightMapCoordinateIndexOnExport"), *InStaticMesh->GetName(), LightMapChannelIndex));
				bAnyErrorOccured = TRUE;
			}
		} 
		else 
		{
			GWarn->Logf( LocalizeSecure(LocalizeUnrealEd("StaticMeshEditor_NoSourceDataForExport"), *InStaticMesh->GetName()));
			bAnyErrorOccured = TRUE;
		}

		TArray<FVector> Verts;				// The verts in the mesh
		TArray<FVector2D> UVs;			// Lightmap UVs from channel 1

		// Collect all the data about the mesh
		for( INT tri = 0 ; tri < MeshRenderData->RawTriangles.GetElementCount() ; tri++ )
		{
			// Vertices
			Verts.AddItem( RawTriangleData[tri].Vertices[0] );
			Verts.AddItem( RawTriangleData[tri].Vertices[1] );
			Verts.AddItem( RawTriangleData[tri].Vertices[2] );

			// UVs from channel 1 (lightmap coords)
			UVs.AddItem( RawTriangleData[tri].UVs[0][LightMapChannelIndex] );
			UVs.AddItem( RawTriangleData[tri].UVs[1][LightMapChannelIndex] );
			UVs.AddItem( RawTriangleData[tri].UVs[2][LightMapChannelIndex] );
		}
		MeshRenderData->RawTriangles.Unlock();

		// Write out the vertex data
		InOutputDevice->Log( TEXT("\r\n") );
		for( INT v = 0 ; v < Verts.Num() ; ++v )
		{
			// Transform to Lightwave's coordinate system
			InOutputDevice->Logf( TEXT("v %f %f %f\r\n"), Verts(v).X, Verts(v).Z, Verts(v).Y );
		}
		InOutputDevice->Logf( TEXT("# %d vertices\r\n"), Verts.Num() );

		// Write out the lightmap UV data 
		InOutputDevice->Log( TEXT("\r\n") );
		for( INT uv = 0 ; uv < UVs.Num() ; ++uv )
		{
			// Invert the y-coordinate (Lightwave has their bitmaps upside-down from us).
			InOutputDevice->Logf( TEXT("vt %f %f\r\n"), UVs(uv).X, 1.0f - UVs(uv).Y);
		}
		InOutputDevice->Logf( TEXT("# %d uvs\r\n"), UVs.Num() );

		// Write object header
		InOutputDevice->Log( TEXT("\r\n") );
		InOutputDevice->Log( TEXT("g UnrealEdObject\r\n") );
		InOutputDevice->Log( TEXT("\r\n") );

		//NOTE - JB - Do not save out the smoothing groups separately.  If you do, the vertices get swizzled on import.
		//This is because the vertices are exported in RawTriangle Ordering, so to match again on import the face list has to be perfectly sequential.
		InOutputDevice->Logf( TEXT("s %i\r\n"), 0);		//only smoothing group
		for( INT tri = 0 ; tri < MeshRenderData->RawTriangles.GetElementCount() ; tri++ )
		{
			INT VertexIndex = 1 + tri*3;
			InOutputDevice->Logf(TEXT("f %d/%d %d/%d %d/%d\r\n"), VertexIndex, VertexIndex, VertexIndex+1, VertexIndex+1, VertexIndex+2, VertexIndex+2 );
		}

		// Write out footer
		InOutputDevice->Log( TEXT("\r\n") );
		InOutputDevice->Log( TEXT("g\r\n") );

		return bAnyErrorOccured;	//no errors occured at this point
	};

	/**
	 * Writes out a single LOD of a StaticMesh(LODIndex) for light map editing
	 * @param InCurrentStaticMesh - The mesh to be exported
	 * @param InFilename - The name of the file to write the data to.
	 * @param InLODIndex - The LOD of the model we want exported
	 * @return If any errors occured during export
	 */
	UBOOL ExportSingleLightmapModelFBX (UStaticMesh* InStaticMesh, FFilename& InFilename, const INT InLODIndex)
	{
		UBOOL bAnyErrorOccured = FALSE;

		check(InStaticMesh);
		check(InLODIndex < InStaticMesh->LODModels.Num());

		INT LightMapChannelIndex = InStaticMesh->LightMapCoordinateIndex;
#if WITH_FBX
		UnFbx::CFbxExporter* Exporter = UnFbx::CFbxExporter::GetInstance();
		Exporter->CreateDocument();
		Exporter->ExportStaticMeshLightMap(InStaticMesh, InLODIndex, LightMapChannelIndex);
		Exporter->WriteToFile(*InFilename);
#endif

		return bAnyErrorOccured;
	}

	static UBOOL ExtractLightmapUVsFromMesh( UStaticMesh* InDestStaticMesh, UStaticMesh* MeshToExtractFrom, INT InLODIndex )
	{
		UBOOL bAnyErrorOccured = FALSE;
		// Extract data to LOD
		if(MeshToExtractFrom)
		{
			// Detach all instances of the static mesh from the scene while we're merging the LOD data.
			FStaticMeshComponentReattachContext	ComponentReattachContext(InDestStaticMesh);

			//open the source data
			check (MeshToExtractFrom->LODModels.Num() == 1);		//make sure there is only one LOD in the incoming mesh
			FStaticMeshRenderData &SrcMeshRenderData = MeshToExtractFrom->LODModels(0);
			FStaticMeshRenderData &DstMeshRenderData = InDestStaticMesh->LODModels(InLODIndex);

			//we need to ensure there are a valid number of source trianagles to destination.  
			//In some cases, an exported lightmap uv set may contain fewer polygons than our active mesh, but shouldn't contain more.
			UBOOL bValidTriangleCount = SrcMeshRenderData.RawTriangles.GetElementCount() <= DstMeshRenderData.RawTriangles.GetElementCount();
			if (bValidTriangleCount && SrcMeshRenderData.RawTriangles.GetElementCount())
			{
				// Open the Destination Data
				DstMeshRenderData.ReleaseResources();
				DstMeshRenderData.VertexBuffer.CleanUp();	//only invalidate the texture coordinate vertex buffer

				//lock all data needed
				const FStaticMeshTriangle* SrcRawTriangleData = (FStaticMeshTriangle*) SrcMeshRenderData.RawTriangles.Lock(LOCK_READ_ONLY);
				FStaticMeshTriangle* DstRawTriangleData = (FStaticMeshTriangle*) DstMeshRenderData.RawTriangles.Lock(LOCK_READ_WRITE);

				UBOOL bVerticesCompletelyMatch = TRUE;
				//before stomping texture data, let's make sure the vertex data still matches up
				for( INT tri = 0 ; tri < SrcMeshRenderData.RawTriangles.GetElementCount() ; tri++ )
				{
					for (int SubTriVertIndex = 0; SubTriVertIndex < 3; ++SubTriVertIndex)
					{
						//*4.0f is used in other "the same" operations
						UBOOL bArePointsTheSame = DstRawTriangleData[tri].Vertices[SubTriVertIndex].Equals(SrcRawTriangleData[tri].Vertices[SubTriVertIndex], THRESH_POINTS_ARE_SAME*4.0f);
						if(!bArePointsTheSame)
						{
							bVerticesCompletelyMatch = FALSE;
							break;
						}
					}
				}

				UBOOL ErrorOccured = !bValidTriangleCount;
				if (bValidTriangleCount)
				{
					TArray<INT> TriangleMapping;

					// If the raw triangles don't match, it's possible that they are just out of order, in which case
					// we want to attempt to create a mapping between the two sets of triangles
					if (!bVerticesCompletelyMatch)
					{
						for (INT SrcTriIdx = 0; SrcTriIdx < SrcMeshRenderData.RawTriangles.GetElementCount(); ++SrcTriIdx)
						{
							UBOOL MatchFound = FALSE;
							for (INT DstTriIdx = 0; DstTriIdx < DstMeshRenderData.RawTriangles.GetElementCount(); ++DstTriIdx)
							{
								if (TriangleMapping.ContainsItem(SrcTriIdx))
								{
									continue;
								}

								if (DstRawTriangleData[DstTriIdx].Vertices[0].Equals(SrcRawTriangleData[SrcTriIdx].Vertices[0], THRESH_POINTS_ARE_SAME*4.0f) &&
									DstRawTriangleData[DstTriIdx].Vertices[1].Equals(SrcRawTriangleData[SrcTriIdx].Vertices[1], THRESH_POINTS_ARE_SAME*4.0f) &&
									DstRawTriangleData[DstTriIdx].Vertices[2].Equals(SrcRawTriangleData[SrcTriIdx].Vertices[2], THRESH_POINTS_ARE_SAME*4.0f))
								{
									MatchFound = TRUE;
									TriangleMapping.AddItem(SrcTriIdx);
									break;
								}
							}
							// Couldn't find a pair of matching triangles
							if (!MatchFound)
							{
								break;
							}
						}
						// We successfully created a complete mapping between the two sets of triangles
						if (TriangleMapping.Num() == DstMeshRenderData.RawTriangles.GetElementCount())
						{
							GWarn->Logf( LocalizeSecure(LocalizeUnrealEd("StaticMeshEditor_LightmapImportRemappedTriangleIndices"), *InDestStaticMesh->GetName()));
						}
						// We have mapped all of our source data, but there may exist some unmapped poly's in the dest mesh
						else if (TriangleMapping.Num() == SrcMeshRenderData.RawTriangles.GetElementCount())
						{
							//check if we want to import even if we have potentially bad UV maps.
							INT Result = appMsgf( AMT_YesNo, LocalizeSecure(LocalizeUnrealEd("StaticMeshEditor_LightmapImportFewerTriangles"), *InDestStaticMesh->GetName()));
							if (Result)
							{
								GWarn->Logf( LocalizeSecure(LocalizeUnrealEd("StaticMeshEditor_LightmapImportRemappedTriangleIndices"), *InDestStaticMesh->GetName()));
							}
							else
							{
								ErrorOccured = TRUE;
							}
						}
						// Our poly's could not be successfully mapped
						else
						{
							ErrorOccured = TRUE;
						}
					}

					// if the two sets of raw triangles match or we've successfully created a mapping between them
					if (bVerticesCompletelyMatch || !ErrorOccured)
					{
						//copy back over the texture coordinates to the right channel
						INT SrcLightMapChannelIndex = MeshToExtractFrom->LightMapCoordinateIndex;
						INT DstLightMapChannelIndex = InDestStaticMesh->LightMapCoordinateIndex;
						INT SrcMaxUVS = SrcRawTriangleData[0].NumUVs;
						INT DstMaxUVS = DstRawTriangleData[0].NumUVs;

						//if the light map is out of bounds of the current NumUVS
						if (SrcLightMapChannelIndex >= SrcMaxUVS)
						{
							SrcLightMapChannelIndex = 0;
							GWarn->Logf( *LocalizeUnrealEd("StaticMeshEditor_LightmapImportUsingDefualtLightmapCoordinate"));
						}

						//if no light map has been authored yet, OR the light map is out of bounds of the current NumUVS
						if ((DstLightMapChannelIndex==0) || (DstLightMapChannelIndex >= DstMaxUVS))
						{
							InDestStaticMesh->LightMapCoordinateIndex = DstMaxUVS;
							DstLightMapChannelIndex = DstMaxUVS;
							DstMaxUVS++;
							check(DstMaxUVS <= MAX_TEXCOORDS);	//there just isn't memory for folks to jam that many uvs in here.  Find out what's really going on.
							GWarn->Logf( LocalizeSecure(LocalizeUnrealEd("StaticMeshEditor_LightmapImportNewLightmapCoordinate"), *InDestStaticMesh->GetName(), DstLightMapChannelIndex));
						}

						for (INT TriangleIndex = 0; TriangleIndex < SrcMeshRenderData.RawTriangles.GetElementCount(); ++TriangleIndex)
						{
							INT SrcTriangleIndex = bVerticesCompletelyMatch? TriangleIndex: TriangleMapping(TriangleIndex);
							DstRawTriangleData[TriangleIndex].UVs[0][DstLightMapChannelIndex] = SrcRawTriangleData[SrcTriangleIndex].UVs[0][SrcLightMapChannelIndex];
							DstRawTriangleData[TriangleIndex].UVs[1][DstLightMapChannelIndex] = SrcRawTriangleData[SrcTriangleIndex].UVs[1][SrcLightMapChannelIndex];
							DstRawTriangleData[TriangleIndex].UVs[2][DstLightMapChannelIndex] = SrcRawTriangleData[SrcTriangleIndex].UVs[2][SrcLightMapChannelIndex];
							DstRawTriangleData[TriangleIndex].NumUVs = DstMaxUVS;		//ensure the max uvs is still correct
						}		
					}
				}

				if (ErrorOccured)
				{
					bAnyErrorOccured |= TRUE;
					GWarn->Logf( LocalizeSecure(LocalizeUnrealEd("StaticMeshEditor_LightmapImportVertexMismatch"), *InDestStaticMesh->GetName(), InLODIndex));
				}
				
				//release data
				SrcMeshRenderData.RawTriangles.Unlock();
				DstMeshRenderData.RawTriangles.Unlock();

				// Rebuild the static mesh.
				InDestStaticMesh->Build();	
				InDestStaticMesh->MarkPackageDirty();
			} 
			else
			{
				bAnyErrorOccured |= TRUE;
				GWarn->Logf( LocalizeSecure(LocalizeUnrealEd("StaticMeshEditor_LightmapImportTriangleMismatch"), *InDestStaticMesh->GetName(), InLODIndex));
			}

		}

		return bAnyErrorOccured;
	}

	/**
	 * Reads in a temp mesh who's texture coordinates will REPLACE that of the active static mesh's LightMapCoordinates
	 * @param InStaticMesh - The mesh to be inported
	 * @param InData- A string that contains the files contents
	 * @param InLODIndex - The LOD of the model we want imported
	 * @return If any errors occured during import
	 */
	UBOOL ImportSingleLightmapModelASE(UStaticMesh* InStaticMesh, FString& InData, const INT InLODIndex)
	{
		UBOOL bAnyErrorOccured = FALSE;

		check(InStaticMesh);
		check(InLODIndex < InStaticMesh->LODModels.Num());	//the select which LOD dialog should have stopped invalid values

		const TCHAR* Ptr = *InData;

		debugf(TEXT("LOD %d loading (0x%p)"),InLODIndex,(PTRINT)&InLODIndex);

		// Use the StaticMeshFactory to load this StaticMesh into a temporary StaticMesh.
		UStaticMeshFactory* StaticMeshFact = new UStaticMeshFactory();
		UStaticMesh* TempStaticMesh = (UStaticMesh*)StaticMeshFact->FactoryCreateText( 
			UStaticMesh::StaticClass(), UObject::GetTransientPackage(), NAME_None, 0, NULL, TEXT("ASE"), Ptr, Ptr+InData.Len(), GWarn );

		if( TempStaticMesh )
		{
			bAnyErrorOccured |= ExtractLightmapUVsFromMesh( InStaticMesh, TempStaticMesh, InLODIndex );
		}
		else
		{
			bAnyErrorOccured |= TRUE;
			GWarn->Logf( LocalizeSecure(LocalizeUnrealEd("StaticMeshEditor_LightmapImportFileReadFailure"), *InStaticMesh->GetName(), InLODIndex));
		}
		return bAnyErrorOccured;
	}

	UBOOL ImportSingleLightmapModelFBX(UStaticMesh* InStaticMesh, FString& Filename, const INT InLODIndex )
	{
		UBOOL bAnyErrorOccured = FALSE;

		check(InStaticMesh);
		check(InLODIndex < InStaticMesh->LODModels.Num());	//the select which LOD dialog should have stopped invalid values

		debugf(TEXT("LOD %d loading (0x%p)"),InLODIndex,(PTRINT)&InLODIndex);

		UFbxFactory* FbxFactory = new UFbxFactory();
		// import static meshes only
		FbxFactory->ImportUI->MeshTypeToImport = FBXIT_StaticMesh;
		const BYTE* FakeBuffer = 0;
		UFactory::CurrentFilename = Filename;
		UStaticMesh* TempStaticMesh = (UStaticMesh*)FbxFactory->FactoryCreateBinary( UStaticMesh::StaticClass(), UObject::GetTransientPackage(), NAME_None, 0, NULL, TEXT("FBX"), FakeBuffer, NULL, GWarn );

		if( TempStaticMesh )
		{
			bAnyErrorOccured |= ExtractLightmapUVsFromMesh( InStaticMesh, TempStaticMesh, InLODIndex );
		}
		else
		{
			bAnyErrorOccured |= TRUE;
			GWarn->Logf( LocalizeSecure(LocalizeUnrealEd("StaticMeshEditor_LightmapImportFileReadFailure"), *InStaticMesh->GetName(), InLODIndex));
		}
		return bAnyErrorOccured;
	}

}  //end namespace MeshUtils
