/*=============================================================================
	UnShadow.cpp: Bsp light mesh illumination builder code
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"

FSwarmDebugOptions GSwarmDebugOptions;

#include "LightingBuildOptions.h"
#include "StaticLightingPrivate.h"
#include "Database.h"
#include "Sorting.h"
#include "UnModelLight.h"
#include "PrecomputedLightVolume.h"
#include "EngineMeshClasses.h"
#include "EngineProcBuildingClasses.h"
#include "LevelUtils.h"

// Don't compile the static lighting system on consoles.
#if !CONSOLE

#if WITH_MANAGED_CODE
#include "Lightmass.h"
#endif

/** The number of hardware threads to not use for building static lighting. */
#define NUM_STATIC_LIGHTING_UNUSED_THREADS 0

UBOOL GbLogAddingMappings = FALSE;

/** Counts the number of lightmap textures generated each lighting build. */
extern INT GLightmapCounter;

/** Whether to allow lighting builds to generate streaming lightmaps. */
extern UBOOL GAllowStreamingLightmaps;

// NOTE: We're only counting the top-level mip-map for the following variables.
/** Total number of texels allocated for all lightmap textures. */
extern QWORD GNumLightmapTotalTexels;
/** Total number of texels used if the texture was non-power-of-two. */
extern QWORD GNumLightmapTotalTexelsNonPow2;
/** Number of lightmap textures generated. */
extern INT GNumLightmapTextures;
/** Total number of mapped texels. */
extern QWORD GNumLightmapMappedTexels;
/** Total number of unmapped texels. */
extern QWORD GNumLightmapUnmappedTexels;
/** Total number of texels allocated for all shadowmap textures. */
extern QWORD GNumShadowmapTotalTexels;
/** Number of shadowmap textures generated. */
extern INT GNumShadowmapTextures;
/** Total number of mapped texels. */
extern QWORD GNumShadowmapMappedTexels;
/** Total number of unmapped texels. */
extern QWORD GNumShadowmapUnmappedTexels;
/** Whether to allow cropping of unmapped borders in lightmaps and shadowmaps. Controlled by BaseEngine.ini setting. */
extern UBOOL GAllowLightmapCropping;
/** Total lightmap texture memory size (in bytes), including GLightmapTotalStreamingSize. */
extern QWORD GLightmapTotalSize;
/** Total lightmap texture memory size on an Xbox 360 (in bytes). */
extern QWORD GLightmapTotalSize360;
/** Total memory size for streaming lightmaps (in bytes). */
extern QWORD GLightmapTotalStreamingSize;
/** Total shadowmap texture memory size (in bytes). */
extern QWORD GShadowmapTotalSize;
/** Total shadowmap texture memory size on an Xbox 360 (in bytes). */
extern QWORD GShadowmapTotalSize360;
/** Total texture memory size for streaming shadowmaps. */
extern QWORD GShadowmapTotalStreamingSize;
/** Largest boundingsphere radius to use when packing lightmaps into a texture atlas. */
extern FLOAT GMaxLightmapRadius;


IMPLEMENT_COMPARE_CONSTREF(FStaticLightingMappingSortHelper,UnShadow,{ return A.NumTexels < B.NumTexels ? 1 : -1; });

FStaticLightingSystem::FStaticLightingSystem(const FLightingBuildOptions& InOptions):
	Options(InOptions), 
	bBuildCanceled(FALSE),
	DeterministicIndex(0),
	NextVisibilityId(0),
	CompleteVertexMappingList(*this),
	CompleteTextureMappingList(*this),
	bUseCustomImportanceVolume(FALSE)
{
	const DOUBLE StartTime = appSeconds();
	const DOUBLE StartupStartTime = appSeconds();

	// Clear out the last builds results
	GWarn->LightingBuild_Clear();
	GWarn->LightingBuildInfo_Clear();

	GLightmapCounter = 0;
	GNumLightmapTotalTexels = 0;
	GNumLightmapTotalTexelsNonPow2 = 0;
	GNumLightmapTextures = 0;
	GNumLightmapMappedTexels = 0;
	GNumLightmapUnmappedTexels = 0;
	GNumShadowmapTotalTexels = 0;
	GNumShadowmapTextures = 0;
	GNumShadowmapMappedTexels = 0;
	GNumShadowmapUnmappedTexels = 0;
	GLightmapTotalSize = 0;
	GLightmapTotalSize360 = 0;
	GLightmapTotalStreamingSize = 0;
	GShadowmapTotalSize = 0;
	GShadowmapTotalSize360 = 0;
	GShadowmapTotalStreamingSize = 0;

	for( TObjectIterator<UPrimitiveComponent> It ; It ; ++It )
	{
		UPrimitiveComponent* Component = *It;
		Component->VisibilityId = INDEX_NONE;
	}

	FString SkippedLevels;
	for ( INT LevelIndex=0; LevelIndex < GWorld->Levels.Num(); LevelIndex++ )
	{
		ULevel* Level = GWorld->Levels(LevelIndex);
		Level->LightmapTotalSize = 0.0f;
		Level->ShadowmapTotalSize = 0.0f;
		ULevelStreaming* LevelStreaming = NULL;
		if ( GWorld->PersistentLevel != Level )
		{
			LevelStreaming = FLevelUtils::FindStreamingLevel( Level );
		}
		if (!Options.ShouldBuildLightingForLevel(Level))
		{
			SkippedLevels += FString(TEXT(", ")) + (LevelStreaming ? LevelStreaming->PackageName.ToString() : Level->GetName());
		}
	}

	AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();
	for( INT LevelIndex = 0 ; LevelIndex < WorldInfo->StreamingLevels.Num() ; ++LevelIndex )
	{
		ULevelStreaming* CurStreamingLevel = WorldInfo->StreamingLevels( LevelIndex );
		if (CurStreamingLevel && CurStreamingLevel->LoadedLevel && !CurStreamingLevel->bShouldBeVisibleInEditor)
		{
			if (SkippedLevels.Len() > 0)
			{
				SkippedLevels += FString(TEXT(", ")) + CurStreamingLevel->PackageName.ToString();
			}
			else
			{
				SkippedLevels += CurStreamingLevel->PackageName.ToString();
			}
		}
	}

	if (SkippedLevels.Len() > 0)
	{
		// Warn when some levels are not visible and therefore will not be built, because that indicates that only a partial build will be done,
		// Lighting will still be unbuilt for some areas when playing through the level.
		const FString HiddenLevelsWarning = FString::Printf( LocalizeSecure( LocalizeUnrealEd("Warning_WarningHiddenLevelsBeforeRebuild"), *SkippedLevels ) );
		WxSuppressableWarningDialog WarnAboutHiddenLevels( *HiddenLevelsWarning, *LocalizeUnrealEd("Warning_WarningHiddenLevelsBeforeRebuildTitle"), "WarnOnHiddenLevelsBeforeRebuild" );
		WarnAboutHiddenLevels.ShowModal();
	}

	UINT YesNoToAll = ART_No;

	// Begin the static lighting progress bar.
	GWarn->BeginSlowTask( TEXT("Building static lighting"), FALSE /*Options.bUseLightmass*/ );

	const UBOOL bForceNoPrecomputedLighting = GWorld->GetWorldInfo()->bForceNoPrecomputedLighting;
	UBOOL bRebuildDirtyGeometryForLighting = TRUE;
	GConfig->GetFloat( TEXT("TextureStreaming"), TEXT("MaxLightmapRadius"), GMaxLightmapRadius, GEngineIni );
	GConfig->GetBool( TEXT("TextureStreaming"), TEXT("AllowStreamingLightmaps"), GAllowStreamingLightmaps, GEngineIni );
#if WITH_MANAGED_CODE
	appCheckIniForOutdatedness( GLightmassIni, GDefaultLightmassIni, FALSE, YesNoToAll, TRUE );
	verify(GConfig->GetBool(TEXT("DevOptions.StaticLighting"), TEXT("bAllowMultiThreadedStaticLighting"), bAllowMultiThreadedStaticLighting, GLightmassIni));
	verify(GConfig->GetBool(TEXT("DevOptions.StaticLighting"), TEXT("bUseConservativeTexelRasterization"), bAllowBilinearTexelRasterization, GLightmassIni));
	verify(GConfig->GetBool(TEXT("DevOptions.StaticLighting"), TEXT("bAllowLightmapCompression"), GAllowLightmapCompression, GLightmassIni));
	verify(GConfig->GetBool(TEXT("DevOptions.StaticLighting"), TEXT("bAllowEagerLightmapEncode"), GAllowEagerLightmapEncode, GLightmassIni));
	verify(GConfig->GetBool(TEXT("DevOptions.StaticLighting"), TEXT("bUseBilinearFilterLightmaps"), GUseBilinearLightmaps, GLightmassIni));
	verify(GConfig->GetBool(TEXT("DevOptions.StaticLighting"), TEXT("bAllowCropping"), GAllowLightmapCropping, GLightmassIni));
	verify(GConfig->GetBool(TEXT("DevOptions.StaticLighting"), TEXT("bRebuildDirtyGeometryForLighting"), bRebuildDirtyGeometryForLighting, GLightmassIni));
	verify(GConfig->GetBool(TEXT("DevOptions.StaticLighting"), TEXT("bRepackLightAndShadowMapTextures"), GRepackLightAndShadowMapTextures, GLightmassIni));
	
	// Do they want deterministic lighting?
	debugf(TEXT("Deterministic lighting is %s"), GLightmassDebugOptions.bUseDeterministicLighting ? TEXT("ENABLED") : TEXT("DISABLED"));
	if (GLightmassDebugOptions.bUseDeterministicLighting)
	{
		debugf(TEXT("\tLighting will be slower to complete..."));
	}

	// Do they want to use Lightmass or old-style UE3 lighting?
	if ( Options.bUseLightmass )
	{
		GAllowLightmapPadding = TRUE;
		appMemzero(&CustomImportanceBoundingBox, sizeof(FBox));
		appMemzero(&LightingMeshBounds, sizeof(FBox));

		GLightingBuildQuality = Options.QualityLevel;
		switch(Options.QualityLevel)
		{
			case Quality_Preview:
				GLightmapEncodeQualityLevel = 0; // nvtt::Quality_Fastest
				break;

			case Quality_Medium:
			case Quality_High:
			case Quality_Production:
			default:
				GLightmapEncodeQualityLevel = 2; // nvtt::Quality_Production
				break;
		}
	}
	else
#endif //#if WITH_MANAGED_CODE
	{
		GAllowLightmapPadding = FALSE;
		GLightmapEncodeQualityLevel = 2; // nvtt::Quality_Production
	}

	LightmassStatistics.StartupTime += appSeconds() - StartupStartTime;
	const DOUBLE CollectStartTime = appSeconds();

	// Prepare lights for rebuild.
	const DOUBLE PrepareLightsStartTime = appSeconds();

	if (!Options.bOnlyBuildVisibility)
	{
		// Delete all AGeneratedMeshAreaLight's, since new ones will be created after the build with updated properties.
		USelection* EditorSelection = GEditor->GetSelectedActors();
		for(TObjectIterator<AGeneratedMeshAreaLight> LightIt;LightIt;++LightIt)
		{
			if (EditorSelection)
			{
				EditorSelection->Deselect(*LightIt);
			}
			GWorld->DestroyActor(*LightIt);
		}

		for(TObjectIterator<ULightComponent> LightIt;LightIt;++LightIt)
		{
			ULightComponent* const Light = *LightIt;
			const UBOOL bLightIsInWorld = Light->GetOwner() && GWorld->ContainsActor(Light->GetOwner());
			if(bLightIsInWorld && (Light->HasStaticShadowing() || Light->HasStaticLighting()))
			{
				// Make sure the light GUIDs and volumes are up-to-date.
				Light->ValidateLightGUIDs();

				// Add the light to the system's list of lights in the world.
				Lights.AddItem(Light);
			}
		}
	}

	LightmassStatistics.PrepareLightsTime += appSeconds() - PrepareLightsStartTime;
	const DOUBLE GatherLightingInfoStartTime = appSeconds();

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
	// Clear reference to the selected lightmap
	GCurrentSelectedLightmapSample.Lightmap = NULL;
	GDebugStaticLightingInfo = FDebugLightingOutput();
#endif

	UINT ActorsInvalidated = 0;
	UINT ActorsToInvalidate = 0;
	for( INT LevelIndex=0; LevelIndex<GWorld->Levels.Num(); LevelIndex++ )
	{
		ActorsToInvalidate += GWorld->Levels(LevelIndex)->Actors.Num();
	}

	GWarn->StatusUpdatef( ActorsInvalidated, ActorsToInvalidate, TEXT("Gathering scene and invalidating lighting...") );

	UBOOL bObjectsToBuildLightingForFound = FALSE;
	// Gather static lighting info from actor components.
	for( INT LevelIndex=0; LevelIndex<GWorld->Levels.Num(); LevelIndex++ )
	{
		UBOOL bMarkLevelDirty = FALSE;
		ULevel* Level = GWorld->Levels(LevelIndex);

		// If the geometry is dirty and we're allowed to automatically clean it up, do so
		if (Level->bGeometryDirtyForLighting)
		{
			warnf( NAME_Log, TEXT("WARNING: Lighting build detected that geometry needs to be rebuilt to avoid incorrect lighting (due to modifying a lighting property).") );
			if (bRebuildDirtyGeometryForLighting)
			{
				// This will go ahead and clean up lighting on all dirty levels (not just this one)
				warnf( NAME_Log, TEXT("WARNING: Lighting build automatically rebuilding geometry.") );
				GUnrealEd->Exec( TEXT("MAP REBUILD ALLDIRTYFORLIGHTING") );
			}
		}

		const UBOOL bBuildLightingForLevel = Options.ShouldBuildLightingForLevel( Level );
		UModel* Model = Level->Model;

		if (bBuildLightingForLevel)
		{
			if (Level->PrecomputedLightVolume && !Options.bOnlyBuildVisibility)
			{
				Level->PrecomputedLightVolume->InvalidateLightingCache();
			}
			if (Level == GWorld->PersistentLevel)
			{
				Level->PrecomputedVisibilityHandler.Invalidate(GWorld->Scene);
				Level->PrecomputedVolumeDistanceField.Invalidate(GWorld->Scene);
			}
		}

		// Gather static lighting info from BSP.
		UBOOL bBuildBSPLighting = 
			bBuildLightingForLevel &&
			Options.bBuildBSP;

		TArray<FNodeGroup*> NodeGroupsToBuild;
		TArray<UModelComponent*> SelectedModelComponents;
		if (bBuildBSPLighting && !Options.bOnlyBuildVisibility)
		{
			if (!Options.bOnlyBuildSelected)
			{
				// Build it all
				for (INT i = 0; i < Level->ModelComponents.Num(); i++)
				{
					Level->ModelComponents(i)->InvalidateLightingCache();
				}
			}
			else
			{
				GLightmassDebugOptions.bGatherBSPSurfacesAcrossComponents = FALSE;
				Model->GroupAllNodes(Level, Lights);
				bBuildBSPLighting = FALSE;
				// Build only selected brushes/surfaces
				TArray<ABrush*> SelectedBrushes;
				for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
				{
					AActor* Actor = Level->Actors(ActorIndex);
					if(Actor)
					{
						ABrush* Brush = Cast<ABrush>(Actor);
						if (Brush && Brush->IsSelected())
						{
							SelectedBrushes.AddItem(Brush);
						}
					}
				}

				TArray<INT> SelectedSurfaceIndices;
				// Find selected surfaces...
				for (INT SurfIdx = 0; SurfIdx < Model->Surfs.Num(); SurfIdx++)
				{
					UBOOL bSurfaceSelected = FALSE;
					FBspSurf& Surf = Model->Surfs(SurfIdx);
					if ((Surf.PolyFlags & PF_Selected) != 0)
					{
						SelectedSurfaceIndices.AddItem(SurfIdx);
						bSurfaceSelected = TRUE;
					}
					else
					{
						INT DummyIdx;
						if (SelectedBrushes.FindItem(Surf.Actor, DummyIdx) == TRUE)
						{
							SelectedSurfaceIndices.AddItem(SurfIdx);
							bSurfaceSelected = TRUE;
						}
					}

					if (bSurfaceSelected == TRUE)
					{
						// Find it's model component...
						for (INT NodeIdx = 0; NodeIdx < Model->Nodes.Num(); NodeIdx++)
						{
							const FBspNode& Node = Model->Nodes(NodeIdx);
							if (Node.iSurf == SurfIdx)
							{
								UModelComponent* SomeModelComponent = Level->ModelComponents(Node.ComponentIndex);
								if (SomeModelComponent)
								{
									SelectedModelComponents.AddUniqueItem(SomeModelComponent);
									for (INT InnerNodeIndex = 0; InnerNodeIndex < SomeModelComponent->Nodes.Num(); InnerNodeIndex++)
									{
										FBspNode& InnerNode = Model->Nodes(SomeModelComponent->Nodes(InnerNodeIndex));
										SelectedSurfaceIndices.AddUniqueItem(InnerNode.iSurf);										
									}
								}
							}
						}
					}
				}

				// Pass 2...
				if (SelectedSurfaceIndices.Num() > 0)
				{
					for (INT SSIdx = 0; SSIdx < SelectedSurfaceIndices.Num(); SSIdx++)
					{
						INT SurfIdx = SelectedSurfaceIndices(SSIdx);
						// Find it's model component...
						for (INT NodeIdx = 0; NodeIdx < Model->Nodes.Num(); NodeIdx++)
						{
							const FBspNode& Node = Model->Nodes(NodeIdx);
							if (Node.iSurf == SurfIdx)
							{
								UModelComponent* SomeModelComponent = Level->ModelComponents(Node.ComponentIndex);
								if (SomeModelComponent)
								{
									SelectedModelComponents.AddUniqueItem(SomeModelComponent);
									for (INT InnerNodeIndex = 0; InnerNodeIndex < SomeModelComponent->Nodes.Num(); InnerNodeIndex++)
									{
										FBspNode& InnerNode = Model->Nodes(SomeModelComponent->Nodes(InnerNodeIndex));
										SelectedSurfaceIndices.AddUniqueItem(InnerNode.iSurf);										
									}
								}
							}
						}
					}
				}

				if (SelectedSurfaceIndices.Num() > 0)
				{
					// Fill in a list of all the node group to rebuild...
					bBuildBSPLighting = FALSE;
					for (TMap<INT, FNodeGroup*>::TIterator It(Model->NodeGroups); It; ++It)
					{
						FNodeGroup* NodeGroup = It.Value();
						if (NodeGroup && (NodeGroup->Nodes.Num() > 0))
						{
							for (INT GroupNodeIdx = 0; GroupNodeIdx < NodeGroup->Nodes.Num(); GroupNodeIdx++)
							{
								INT CheckIdx;
								if (SelectedSurfaceIndices.FindItem(Model->Nodes(NodeGroup->Nodes(GroupNodeIdx)).iSurf, CheckIdx) == TRUE)
								{
									NodeGroupsToBuild.AddUniqueItem(NodeGroup);
									bBuildBSPLighting = TRUE;
								}
							}
						}
					}
				}
			}
		}

		if (bBuildBSPLighting && !bForceNoPrecomputedLighting)
		{
			if (!Options.bOnlyBuildSelected || Options.bOnlyBuildVisibility)
			{
				// generate BSP mappings across the whole level
				AddBSPStaticLightingInfo(Level, bBuildBSPLighting);
			}
			else
			{
				if (NodeGroupsToBuild.Num() > 0)
				{
					bObjectsToBuildLightingForFound = TRUE;
					AddBSPStaticLightingInfo(Level, NodeGroupsToBuild);
				}
			}
		}

		// Gather static lighting info from actors.
		for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
		{
			AActor* Actor = Level->Actors(ActorIndex);
			if(Actor)
			{
				const UBOOL bBuildActorLighting =
					bBuildLightingForLevel &&
					Options.bBuildActors &&
					(!Options.bOnlyBuildSelected || Actor->IsSelected());

				if (bBuildActorLighting)
				{
					// Recombine all instances so when they split based on lighting they never rejoin
					UBOOL bAssumeLightingSuccess = TRUE;
					UBOOL bIgnoreTextureForBatching = TRUE;
					UInstancedStaticMeshComponent::ResolveInstancedLightmapsForActor(Actor, bAssumeLightingSuccess, bIgnoreTextureForBatching);

					bObjectsToBuildLightingForFound = TRUE;
					if (!Options.bOnlyBuildVisibility)
					{
						ALight* Light = Cast<ALight>(Actor);
						if (Light)
						{
							// Invalidate cached data without regenerating light guids
							// The light guids are still being used in streaming levels that aren't being built
							Light->InvalidateLightingForRebuild(Options.bOnlyBuildVisibleLevels);
						}
						else
						{
							Actor->InvalidateLightingCache();
						}
					}
				}

				if (GLightmassDebugOptions.bUseDeterministicLighting)
				{
					// Need to give the actor the chance to ensure components are in the same order each time.
					Actor->OrderComponentsForDeterministicLighting();
				}

				TArray<ULightComponent*> ActorRelevantLights;
				UBOOL bActorAcceptsLights = FALSE;
				for(INT ComponentIndex = 0;ComponentIndex < Actor->AllComponents.Num();ComponentIndex++)
				{
					UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Actor->AllComponents(ComponentIndex));
					if(Primitive)
					{
						bActorAcceptsLights = bActorAcceptsLights || Primitive->bAcceptsLights;
						for(INT LightIndex = 0;LightIndex < Lights.Num();LightIndex++)
						{
							ULightComponent* Light = Lights(LightIndex);
							// Only add enabled lights or lights that can potentially be enabled at runtime (toggleable)
							if ((Light->bEnabled || !Light->UseDirectLightMap) && Light->AffectsPrimitive(Primitive, TRUE))
							{
								ActorRelevantLights.AddUniqueItem(Light);
							}
						}
					}
				}
				TArray<FStaticLightingPrimitiveInfo> PrimitiveInfos;
				// Allow the actor to provide static lighting info for all of its components first
				if (Actor->GetActorStaticLightingInfo(PrimitiveInfos,ActorRelevantLights,Options))
				{
					for (INT PrimitiveIndex = 0; PrimitiveIndex < PrimitiveInfos.Num(); PrimitiveIndex++)
					{
						AddPrimitiveStaticLightingInfo(PrimitiveInfos(PrimitiveIndex),bBuildActorLighting,bActorAcceptsLights);
					}
				}
				// Call GetStaticLightingInfo for each component if GetActorStaticLightingInfo return FALSE
				else
				{
					// Gather static lighting info from each of the actor's components.
					for(INT ComponentIndex = 0;ComponentIndex < Actor->AllComponents.Num();ComponentIndex++)
					{
						UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Actor->AllComponents(ComponentIndex));
						if(Primitive && !bForceNoPrecomputedLighting)
						{
							// Find the lights relevant to the primitive.
							TArray<ULightComponent*> PrimitiveRelevantLights;
							for(INT LightIndex = 0;LightIndex < Lights.Num();LightIndex++)
							{
								ULightComponent* Light = Lights(LightIndex);
								// Only add enabled lights or lights that can potentially be enabled at runtime (toggleable)
								if ((Light->bEnabled || !Light->UseDirectLightMap) && Light->AffectsPrimitive(Primitive, TRUE))
								{
									PrimitiveRelevantLights.AddItem(Light);
								}
							}

							// Query the component for its static lighting info.
							FStaticLightingPrimitiveInfo PrimitiveInfo;
							Primitive->GetStaticLightingInfo(PrimitiveInfo,PrimitiveRelevantLights,Options);
							if (PrimitiveInfo.Meshes.Num() > 0 && !Actor->bMovable)
							{
								if (GWorld->GetWorldInfo()->bPrecomputeVisibility)
								{
									// Make sure the level gets dirtied since we are changing the visibility Id of a component in it
									bMarkLevelDirty = TRUE;
								}
								
								PrimitiveInfo.VisibilityId = Primitive->VisibilityId = NextVisibilityId;
								NextVisibilityId++;
							}
							AddPrimitiveStaticLightingInfo(PrimitiveInfo,bBuildActorLighting,Primitive->bAcceptsLights);
						}
					}
				}
			}
			GWarn->UpdateProgress( ++ActorsInvalidated, ActorsToInvalidate );
		}

		if (bMarkLevelDirty)
		{
			Level->MarkPackageDirty();
		}
	}

	if (Options.bOnlyBuildSelected)
	{
		GWarn->LightingBuild_Add(MCTYPE_WARNING, NULL, *Localize(TEXT("Lightmass"), TEXT("LightmassError_BuildSelected"), TEXT("UnrealEd")));

		if (!bObjectsToBuildLightingForFound)
		{
			GWarn->LightingBuild_Add(MCTYPE_ERROR, NULL, *Localize(TEXT("Lightmass"), TEXT("LightmassError_BuildSelectedNothingSelected"), TEXT("UnrealEd")));
		}
	}

	LightmassStatistics.GatherLightingInfoTime += appSeconds() - GatherLightingInfoStartTime;

	// Now that light Guids have been finalized, setup a map from Guid to light for quick lookup during import
	for (INT LightIndex = 0; LightIndex < Lights.Num(); LightIndex++)
	{
		ULightComponent* CurrentLight = Lights(LightIndex);
		GuidToLightMap.Set(CurrentLight->LightmapGuid, CurrentLight);
	}

	// Sort the mappings - and tag meshes if doing deterministic mapping
	if (GLightmassDebugOptions.bSortMappings)
	{
		Sort<USE_COMPARE_CONSTREF(FStaticLightingMappingSortHelper,UnShadow)>(&(UnSortedMappings(0)),UnSortedMappings.Num());
		for (INT SortIndex = 0; SortIndex < UnSortedMappings.Num(); SortIndex++)
		{
			FStaticLightingMapping* Mapping = UnSortedMappings(SortIndex).Mapping;
			Mappings.AddItem(Mapping);
			if (GLightmassDebugOptions.bUseDeterministicLighting)
			{
				if (Mapping->bProcessMapping)
				{
					if (Mapping->Mesh)
					{
						Mapping->Mesh->Guid = FGuid(0,0,0,DeterministicIndex++);
					}
				}
			}
		}
		UnSortedMappings.Empty();
	}

	// Verify deterministic lighting setup, if it is enabled...
	if (GLightmassDebugOptions.bUseDeterministicLighting)
	{
		for (INT CheckMapIdx = 0; CheckMapIdx < Mappings.Num(); CheckMapIdx++)
		{
			if (Mappings(CheckMapIdx)->bProcessMapping)
			{
				FGuid CheckGuid = Mappings(CheckMapIdx)->Mesh->Guid;
				if ((CheckGuid.A != 0) ||
					(CheckGuid.B != 0) || 
					(CheckGuid.C != 0) ||
					(CheckGuid.D >= (UINT)(Mappings.Num()))
					)
				{
					warnf(NAME_Warning, TEXT("Lightmass: Error in deterministic lighting for %s:%s"),
						*(Mappings(CheckMapIdx)->Mesh->Guid.String()), *(Mappings(CheckMapIdx)->GetDescription()));
				}
			}
		}
	}

	const UINT NumStaticLightingThreads = bAllowMultiThreadedStaticLighting ? Max<UINT>(0,GNumHardwareThreads - NUM_STATIC_LIGHTING_UNUSED_THREADS) : 1;

	// if we are dumping binary results, clear up any existing ones
	if (Options.bDumpBinaryResults)
	{
		FStaticLightingSystem::ClearBinaryDumps(Options.bUseLightmass);
	}

	LightmassStatistics.CollectTime += appSeconds() - CollectStartTime;
	DOUBLE ProcessingStartTime = appSeconds();

	UBOOL bLightingSuccessful = TRUE;
	if (!bForceNoPrecomputedLighting)
	{
		if ( Options.bUseLightmass )
		{
			UBOOL bSavedUpdateStatus_LightMap = FLightMap2D::GetStatusUpdate();
			UBOOL bSavedUpdateStatus_ShadowMap = UShadowMap2D::GetStatusUpdate();
			if (GLightmassDebugOptions.bImmediateProcessMappings)
			{
				FLightMap2D::SetStatusUpdate(FALSE);
				UShadowMap2D::SetStatusUpdate(FALSE);
			}

#if WITH_MANAGED_CODE
			bLightingSuccessful = LightmassProcess();
#endif //#if WITH_MANAGED_CODE

			if (GLightmassDebugOptions.bImmediateProcessMappings)
			{
				FLightMap2D::SetStatusUpdate(bSavedUpdateStatus_LightMap);
				UShadowMap2D::SetStatusUpdate(bSavedUpdateStatus_ShadowMap);
			}
		}
		else
		{
			MultithreadProcess(NumStaticLightingThreads);
			bLightingSuccessful = !bBuildCanceled;
		}
	}

	// End the static lighting progress bar.
	GWarn->EndSlowTask();

	LightmassStatistics.ProcessingTime += appSeconds() - ProcessingStartTime;
	DOUBLE ApplyStartTime = appSeconds();

	// Now that the lighting is done, we can tell the model components to use their new elements,
	// instead of the pre-lighting ones
	UModelComponent::ApplyTempElements(bLightingSuccessful);

	LightmassStatistics.ApplyTime += appSeconds() - ApplyStartTime;
	DOUBLE EncodingStartTime = appSeconds();

	// Flush pending shadow-map and light-map encoding.
	UShadowMap2D::EncodeTextures( bLightingSuccessful );
	FLightMap2D::EncodeTextures( bLightingSuccessful, TRUE );

	// let the instanced components split/join themselves based on lighting
	UInstancedStaticMeshComponent::ResolveInstancedLightmaps(bLightingSuccessful);

	LightmassStatistics.EncodingTime += appSeconds() - EncodingStartTime;
	DOUBLE FinishingStartTime = appSeconds();

	// Mark lights of the computed level to have valid precomputed lighting.
	for (INT LevelIndex = 0; LevelIndex < GWorld->Levels.Num(); LevelIndex++)
	{
		ULevel* Level = GWorld->Levels(LevelIndex);

		if (GWorld->PersistentLevel == Level)
		{
			Level->PrecomputedVisibilityHandler.UpdateScene(GWorld->Scene);
			Level->PrecomputedVolumeDistanceField.UpdateScene(GWorld->Scene);
		}

		const UBOOL bBuildLightingForLevel = Options.ShouldBuildLightingForLevel( Level );
		UINT ActorCount = Level->Actors.Num();
		for (UINT ActorIndex = 0; ActorIndex < ActorCount; ++ActorIndex)
		{
			AActor* Actor = Level->Actors(ActorIndex);
			if(Actor)
			{
				const UBOOL bBuildActorLighting =
					bBuildLightingForLevel &&
					Options.bBuildActors &&
					(!Options.bOnlyBuildSelected || Actor->IsSelected());

				if (bBuildActorLighting)
				{
					Actor->FinalizeStaticLighting();
				}

				if (bLightingSuccessful && !Options.bOnlyBuildSelected)
				{
					ALight* Light = Cast<ALight>(Level->Actors(ActorIndex));
					if(Light && (Light->LightComponent->HasStaticShadowing() || Light->LightComponent->HasStaticLighting()))
					{
						Light->LightComponent->bPrecomputedLightingIsValid = TRUE;
					}
				}
			}
		}

		// Store off the quality of the lighting for the level if lighting was successful and we build lighting for this level.
		if( bLightingSuccessful && bBuildLightingForLevel )
		{
			if( Options.bUseLightmass )
			{
				Level->GetWorldInfo()->LevelLightingQuality = Options.QualityLevel;
			}
			else
			{
				Level->GetWorldInfo()->LevelLightingQuality = Quality_NoGlobalIllumination;
			}
		}
	}

	// Ensure all primitives which were marked dirty by the lighting build are updated.
	// First clear all components so that any references to static lighting assets held 
	// by scene proxies will be fully released before any components are reattached.
	GWorld->ClearComponents();
	GWorld->UpdateComponents(FALSE);

	// Clear the world's lighting needs rebuild flag.
	if ( bLightingSuccessful )
	{
		GWorld->GetWorldInfo()->SetMapNeedsLightingFullyRebuilt( FALSE );
	}

	// Clean up old shadow-map and light-map data.
	UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

	// Inform the content browser to update the asset list as it could have changed as a result of the lighting build
	GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateAssetList ) );

	// Log execution time.
	LightmassStatistics.FinishingTime += appSeconds() - FinishingStartTime;
	LightmassStatistics.TotalTime += appSeconds() - StartTime;
	ReportStatistics( NumStaticLightingThreads );

	// Report failed lighting build (don't count cancelled builds as failure).
	if ( !bLightingSuccessful && !bBuildCanceled )
	{
		appMsgf( AMT_OK, TEXT("The lighting build failed! See the log for more information!") );
	}
}

/**
 * Reports lighting build statistics to the log.
 * @param NumStaticLightingThreads	Number of threads used for static lighting
 */
void FStaticLightingSystem::ReportStatistics( INT NumStaticLightingThreads )
{
	extern UBOOL GLightmassStatsMode;
	if ( Options.bUseLightmass )
	{
		if ( GLightmassStatsMode )
		{
			DOUBLE TrackedTime =
				LightmassStatistics.StartupTime
				+ LightmassStatistics.CollectTime
				+ LightmassStatistics.ProcessingTime
				+ LightmassStatistics.ImportTime
				+ LightmassStatistics.ApplyTime
				+ LightmassStatistics.EncodingTime
				+ LightmassStatistics.FinishingTime;
			DOUBLE UntrackedTime = LightmassStatistics.TotalTime - TrackedTime;
			debugf(
				TEXT("Illumination: %s total\n")
				TEXT("   %3.1f%%\t%8.1fs    Untracked time\n")
				, *appPrettyTime(LightmassStatistics.TotalTime)
				, UntrackedTime / LightmassStatistics.TotalTime * 100.0
				, UntrackedTime
			);
			debugf(
				TEXT("Breakdown of Illumination time\n")
				TEXT("   %3.1f%%\t%8.1fs \tStarting up\n")
				TEXT("   %3.1f%%\t%8.1fs \tCollecting\n")
				TEXT("   %3.1f%%\t%8.1fs \t--> Preparing lights\n")
				TEXT("   %3.1f%%\t%8.1fs \t--> Gathering lighting info\n")
				TEXT("   %3.1f%%\t%8.1fs \tProcessing\n")
				TEXT("   %3.1f%%\t%8.1fs \tImporting\n")
				TEXT("   %3.1f%%\t%8.1fs \tApplying\n")
				TEXT("   %3.1f%%\t%8.1fs \tEncoding\n")
				TEXT("   %3.1f%%\t%8.1fs \tFinishing\n")
				, LightmassStatistics.StartupTime / LightmassStatistics.TotalTime * 100.0
				, LightmassStatistics.StartupTime
				, LightmassStatistics.CollectTime / LightmassStatistics.TotalTime * 100.0
				, LightmassStatistics.CollectTime
				, LightmassStatistics.PrepareLightsTime / LightmassStatistics.TotalTime * 100.0
				, LightmassStatistics.PrepareLightsTime
				, LightmassStatistics.GatherLightingInfoTime / LightmassStatistics.TotalTime * 100.0
				, LightmassStatistics.GatherLightingInfoTime
				, LightmassStatistics.ProcessingTime / LightmassStatistics.TotalTime * 100.0
				, LightmassStatistics.ProcessingTime
				, LightmassStatistics.ImportTime / LightmassStatistics.TotalTime * 100.0
				, LightmassStatistics.ImportTime
				, LightmassStatistics.ApplyTime / LightmassStatistics.TotalTime * 100.0
				, LightmassStatistics.ApplyTime
				, LightmassStatistics.EncodingTime / LightmassStatistics.TotalTime * 100.0
				, LightmassStatistics.EncodingTime
				, LightmassStatistics.FinishingTime / LightmassStatistics.TotalTime * 100.0
				, LightmassStatistics.FinishingTime
				);
			debugf(
				TEXT("Breakdown of Processing time\n")
				TEXT("   %3.1f%%\t%8.1fs \tCollecting Lightmass scene\n")
				TEXT("   %3.1f%%\t%8.1fs \tExporting\n")
				TEXT("   %3.1f%%\t%8.1fs \tLightmass\n")
				TEXT("   %3.1f%%\t%8.1fs \tSwarm startup\n")
				TEXT("   %3.1f%%\t%8.1fs \tSwarm callback\n")
				TEXT("   %3.1f%%\t%8.1fs \tSwarm job\n")
				TEXT("   %3.1f%%\t%8.1fs \tImporting\n")
				TEXT("   %3.1f%%\t%8.1fs \tApplying\n")
				, LightmassStatistics.CollectLightmassSceneTime / LightmassStatistics.TotalTime * 100.0
				, LightmassStatistics.CollectLightmassSceneTime
				, LightmassStatistics.ExportTime / LightmassStatistics.TotalTime * 100.0
				, LightmassStatistics.ExportTime
				, LightmassStatistics.LightmassTime / LightmassStatistics.TotalTime * 100.0
				, LightmassStatistics.LightmassTime
				, LightmassStatistics.SwarmStartupTime / LightmassStatistics.TotalTime * 100.0
				, LightmassStatistics.SwarmStartupTime
				, LightmassStatistics.SwarmCallbackTime / LightmassStatistics.TotalTime * 100.0
				, LightmassStatistics.SwarmCallbackTime
				, LightmassStatistics.SwarmJobTime / LightmassStatistics.TotalTime * 100.0
				, LightmassStatistics.SwarmJobTime
				, LightmassStatistics.ImportTimeInProcessing / LightmassStatistics.TotalTime * 100.0
				, LightmassStatistics.ImportTimeInProcessing
				, LightmassStatistics.ApplyTimeInProcessing / LightmassStatistics.TotalTime * 100.0
				, LightmassStatistics.ApplyTimeInProcessing
			);

			debugf(
				TEXT("Scratch counters\n")
				TEXT("   %3.1f%%\tScratch0\n")
				TEXT("   %3.1f%%\tScratch1\n")
				TEXT("   %3.1f%%\tScratch2\n")
				TEXT("   %3.1f%%\tScratch3\n")
				, LightmassStatistics.Scratch0
				, LightmassStatistics.Scratch1
				, LightmassStatistics.Scratch2
				, LightmassStatistics.Scratch3
			);

			FLOAT NumLightmapTotalTexels = FLOAT(Max<QWORD>(GNumLightmapTotalTexels,1));
			FLOAT NumShadowmapTotalTexels = FLOAT(Max<QWORD>(GNumShadowmapTotalTexels,1));
			FLOAT LightmapTexelsToMT = FLOAT(NUM_DIRECTIONAL_LIGHTMAP_COEF)/FLOAT(NUM_STORED_LIGHTMAP_COEF)/1024.0f/1024.0f;	// Strip out the SimpleLightMap
			FLOAT ShadowmapTexelsToMT = 1.0f/1024.0f/1024.0f;
			debugf( TEXT("Lightmap textures: %.1f M texels (%.1f%% mapped, %.1f%% unmapped, %.1f%% wasted by packing, %.1f M non-pow2 texels)")
				, NumLightmapTotalTexels * LightmapTexelsToMT
				, 100.0f * FLOAT(GNumLightmapMappedTexels) / NumLightmapTotalTexels
				, 100.0f * FLOAT(GNumLightmapUnmappedTexels) / NumLightmapTotalTexels
				, 100.0f * FLOAT(GNumLightmapTotalTexels - GNumLightmapMappedTexels - GNumLightmapUnmappedTexels) / NumLightmapTotalTexels
				, GNumLightmapTotalTexelsNonPow2 * LightmapTexelsToMT
				);
			debugf( TEXT("Shadowmap textures: %.1f M texels (%.1f%% mapped, %.1f%% unmapped, %.1f%% wasted by packing)")
				, NumShadowmapTotalTexels * ShadowmapTexelsToMT
				, 100.0f * FLOAT(GNumShadowmapMappedTexels) / NumShadowmapTotalTexels
				, 100.0f * FLOAT(GNumShadowmapUnmappedTexels) / NumShadowmapTotalTexels
				, 100.0f * FLOAT(GNumShadowmapTotalTexels - GNumShadowmapMappedTexels - GNumShadowmapUnmappedTexels) / NumShadowmapTotalTexels
				);

			for ( INT LevelIndex=0; LevelIndex < GWorld->Levels.Num(); LevelIndex++ )
			{
				ULevel* Level = GWorld->Levels(LevelIndex);
				debugf( TEXT("Level %2d - Lightmaps: %.1f MB. Shadowmaps: %.1f MB."), LevelIndex, Level->LightmapTotalSize/1024.0f, Level->ShadowmapTotalSize/1024.0f );
			}
		}
		else	//if ( GLightmassStatsMode)
		{
			warnf( NAME_Log, TEXT("Illumination: %s (%s encoding lightmaps)"), *appPrettyTime(LightmassStatistics.TotalTime), *appPrettyTime(LightmassStatistics.EncodingTime) );
		}
	}
	else	//if ( Options.bUseLightmass )
	{
		warnf( NAME_Log, TEXT("Illumination: %s (%s encoding lightmaps), %i threads"), *appPrettyTime(LightmassStatistics.TotalTime), *appPrettyTime(LightmassStatistics.EncodingTime), NumStaticLightingThreads );
	}
	debugf( TEXT("Lightmap texture memory:  %.1f MB (%.1f MB on Xbox 360, %.1f MB streaming, %.1f MB non-streaming), %d textures"),
		GLightmapTotalSize/1024.0f/1024.0f,
		GLightmapTotalSize360/1024.0f/1024.0f,
		GLightmapTotalStreamingSize/1024.0f/1024.0f,
		(GLightmapTotalSize - GLightmapTotalStreamingSize)/1024.0f/1024.0f,
		GNumLightmapTextures * NUM_DIRECTIONAL_LIGHTMAP_COEF / NUM_STORED_LIGHTMAP_COEF);
	debugf( TEXT("Shadowmap texture memory: %.1f MB (%.1f MB on Xbox 360, %.1f MB streaming, %.1f MB non-streaming), %d textures"),
		GShadowmapTotalSize/1024.0f/1024.0f,
		GShadowmapTotalSize360/1024.0f/1024.0f,
		GShadowmapTotalStreamingSize/1024.0f/1024.0f,
		(GShadowmapTotalSize - GShadowmapTotalStreamingSize)/1024.0f/1024.0f,
		GNumShadowmapTextures );
	GTaskPerfTracker->AddTask( TEXT("Rebuilding lighting"), *GWorld->GetOutermost()->GetName(), LightmassStatistics.TotalTime );
	GTaskPerfTracker->AddTask( TEXT("Encoding lightmaps"), *GWorld->GetOutermost()->GetName(), LightmassStatistics.EncodingTime );
}

void FStaticLightingSystem::CompleteDeterministicMappings(class FLightmassProcessor* LightmassProcessor)
{
	if (LightmassProcessor && GLightmassDebugOptions.bUseImmediateImport && GLightmassDebugOptions.bImmediateProcessMappings)
	{
		// Already completed in the Lightmass Run function...
		return;
	}

	DOUBLE ImportAndApplyStartTime = appSeconds();
	DOUBLE ApplyTime = 0.0;

	INT CurrentStep = Mappings.Num();
	INT TotalSteps = Mappings.Num() * 2;
	GWarn->StatusUpdatef( CurrentStep, TotalSteps, TEXT("Importing and applying deterministic mappings...") );

	// Process all the texture mappings first...
	for (INT MappingIndex = 0; MappingIndex < Mappings.Num(); MappingIndex++)
	{
		FStaticLightingTextureMapping* TextureMapping = Mappings(MappingIndex)->GetTextureMapping();
		if (TextureMapping)
		{
			debugfSlow(TEXT("%32s Completed - %s"), *(TextureMapping->GetDescription()), *(TextureMapping->GetLightingGuid().String()));

			if (LightmassProcessor)
			{
#if WITH_MANAGED_CODE
				if (!GLightmassDebugOptions.bUseImmediateImport)
				{
					LightmassProcessor->ImportMapping(TextureMapping->GetLightingGuid(), TRUE);
				}
				else
				{
					DOUBLE ApplyStartTime = appSeconds();
					LightmassProcessor->ProcessMapping(TextureMapping->GetLightingGuid());
					ApplyTime += appSeconds() - ApplyStartTime;
				}
#endif //#if WITH_MANAGED_CODE
			}
			else
			{
				FTextureMappingStaticLightingData* TMData = GetTextureMappingElement(TextureMapping);
				if (TMData)
				{
					// output the data to some binary files
					if (Options.bDumpBinaryResults)
					{
						FStaticLightingSystem::DumpLightMapsToDisk(
							TMData->Mapping->Mesh->Guid, 
							TMData->Mapping->GetDescription(),
							TMData->LightMapData, 
							TMData->ShadowMaps, 
							FALSE);
					}
					DOUBLE ApplyStartTime = appSeconds();
					ApplyMapping(TextureMapping, TMData->LightMapData, TMData->ShadowMaps, NULL);
					ApplyTime += appSeconds() - ApplyStartTime;
				}
				else
				{
					warnf(NAME_Warning, TEXT("DETERMINISTIC: Texture mapping not found!"));
				}
			}
			GWarn->UpdateProgress( ++CurrentStep, TotalSteps );
		}
	}

	// Process all the vertex mappings second...
	for (INT MappingIndex = 0; MappingIndex < Mappings.Num(); MappingIndex++)
	{
		FStaticLightingVertexMapping* VertexMapping = Mappings(MappingIndex)->GetVertexMapping();
		if (VertexMapping)
		{
			debugfSlow(TEXT("%32s Completed - %s"), *(VertexMapping->GetDescription()), *(VertexMapping->GetLightingGuid().String()));

			if (LightmassProcessor)
			{
#if WITH_MANAGED_CODE
				if (!GLightmassDebugOptions.bUseImmediateImport)
				{
					LightmassProcessor->ImportMapping(VertexMapping->GetLightingGuid(), TRUE);
				}
				else
				{
					DOUBLE ApplyStartTime = appSeconds();
					LightmassProcessor->ProcessMapping(VertexMapping->GetLightingGuid());
					ApplyTime += appSeconds() - ApplyStartTime;
				}
#endif //#if WITH_MANAGED_CODE
			}
			else
			{
				FVertexMappingStaticLightingData* VMData = GetVertexMappingElement(VertexMapping);
				if (VMData)
				{
					// output the data to some binary files
					if (Options.bDumpBinaryResults)
					{
						FStaticLightingSystem::DumpLightMapsToDisk(
							VMData->Mapping->Mesh->Guid, 
							VMData->Mapping->GetDescription(),
							VMData->LightMapData, 
							VMData->ShadowMaps, 
							FALSE);
					}
					DOUBLE ApplyStartTime = appSeconds();
					ApplyMapping(VertexMapping, VMData->LightMapData, VMData->ShadowMaps, NULL);
					ApplyTime += appSeconds() - ApplyStartTime;
				}
				else
				{
					warnf(NAME_Warning, TEXT("DETERMINISTIC: Vertex mapping not found!"));
				}
			}
			GWarn->UpdateProgress( ++CurrentStep, TotalSteps );
		}
	}

	CompleteTextureMappingList.Clear();
	CompleteVertexMappingList.Clear();

	LightmassStatistics.ImportTimeInProcessing += appSeconds() - ImportAndApplyStartTime - ApplyTime;
	LightmassStatistics.ApplyTimeInProcessing += ApplyTime;
}

/**
 * Applies the static lighting to the mappings in the list, and clears the list. 
 *
 * @param bUseLightmass If TRUE, this lighting was done using offline rendering
 * @param bDumpBinaryResults If TRUE, lightmap/shadowmap data will be dumped out to the Logs\Lighting directory
 */
template<typename StaticLightingDataType>
void FStaticLightingSystem::TCompleteStaticLightingList<StaticLightingDataType>::ApplyAndClear(UBOOL bUseLightmass, UBOOL bDumpBinaryResults)
{
	while(FirstElement)
	{
		// Atomically read the complete list and clear the shared head pointer.
		TList<StaticLightingDataType>* LocalFirstElement;
		do { LocalFirstElement = FirstElement; }
		while(appInterlockedCompareExchangePointer((void**)&FirstElement,NULL,LocalFirstElement) != LocalFirstElement);

		// Traverse the local list.
		TList<StaticLightingDataType>* Element = LocalFirstElement;
		while(Element)
		{
			// output the data to some binary files
			if (bDumpBinaryResults)
			{
				FStaticLightingSystem::DumpLightMapsToDisk(
					Element->Element.Mapping->Mesh->Guid, 
					Element->Element.Mapping->GetDescription(), 
					Element->Element.LightMapData, Element->Element.ShadowMaps, bUseLightmass);
			}

			System.ApplyMapping(Element->Element.Mapping, Element->Element.LightMapData,Element->Element.ShadowMaps, NULL);

			// Delete this link and advance to the next.
			TList<StaticLightingDataType>* NextElement = Element->Next;
			delete Element;
			Element = NextElement;
		};
	};
}

/**
 *	Empty out the list...
 */
template<typename StaticLightingDataType>
void FStaticLightingSystem::TCompleteStaticLightingList<StaticLightingDataType>::Clear()
{
	while(FirstElement)
	{
		// Atomically read the complete list and clear the shared head pointer.
		TList<StaticLightingDataType>* LocalFirstElement;
		do { LocalFirstElement = FirstElement; }
		while(appInterlockedCompareExchangePointer((void**)&FirstElement,NULL,LocalFirstElement) != LocalFirstElement);

		// Traverse the local list.
		TList<StaticLightingDataType>* Element = LocalFirstElement;
		while(Element)
		{
			// Delete this link and advance to the next.
			TList<StaticLightingDataType>* NextElement = Element->Next;
			delete Element;
			Element = NextElement;
		};
	};
}

/**
 *	Retrieve the mapping lighting data for the given mapping...
 */
FStaticLightingSystem::FVertexMappingStaticLightingData* FStaticLightingSystem::GetVertexMappingElement(FStaticLightingVertexMapping* VertexMapping)
{
	TList<FStaticLightingSystem::FVertexMappingStaticLightingData>* CheckElement = CompleteVertexMappingList.GetFirstElement();
	while (CheckElement)
	{
		if (CheckElement->Element.Mapping == VertexMapping)
		{
			return &(CheckElement->Element);
		}
		CheckElement = CheckElement->Next;
	}
	return NULL;
}

FStaticLightingSystem::FTextureMappingStaticLightingData* FStaticLightingSystem::GetTextureMappingElement(FStaticLightingTextureMapping* TextureMapping)
{
	TList<FStaticLightingSystem::FTextureMappingStaticLightingData>* CheckElement = CompleteTextureMappingList.GetFirstElement();
	while (CheckElement)
	{
		if (CheckElement->Element.Mapping == TextureMapping)
		{
			return &(CheckElement->Element);
		}
		CheckElement = CheckElement->Next;
	}
	return NULL;
}

/**
 * Generates mappings/meshes for all BSP in the given level
 *
 * @param Level Level to build BSP lighting info for
 * @param bBuildLightingForBSP If TRUE, we need BSP mappings generated as well as the meshes
 */
void FStaticLightingSystem::AddBSPStaticLightingInfo(ULevel* Level, UBOOL bBuildLightingForBSP)
{
	// For BSP, we aren't Component-centric, so we can't use the GetStaticLightingInfo 
	// function effectively. Instead, we look across all nodes in the Level's model and
	// generate NodeGroups - which are groups of nodes that are coplanar, adjacent, and 
	// have the same lightmap resolution (henceforth known as being "conodes"). Each 
	// NodeGroup will get a mapping created for it

	// cache the model
	UModel* Model = Level->Model;

	// reset the number of incomplete groups
	Model->NumIncompleteNodeGroups = 0;
	Model->CachedMappings.Empty();

	// create all NodeGroups
	Model->GroupAllNodes(Level, Lights);

	// now we need to make the mappings/meshes
	UBOOL bLocalUseCustomImportanceVolume = FALSE;
	UBOOL bMarkLevelDirty = FALSE;
	for (TMap<INT, FNodeGroup*>::TIterator It(Model->NodeGroups); It; ++It)
	{
		FNodeGroup* NodeGroup = It.Value();

		if (NodeGroup->Nodes.Num())
		{
			// get one of the surfaces/components from the NodeGroup
			// @lmtodo: Remove need for GetSurfaceLightMapResolution to take a surfaceindex, or a ModelComponent :)
			UModelComponent* SomeModelComponent = Level->ModelComponents(Model->Nodes(NodeGroup->Nodes(0)).ComponentIndex);
			INT SurfaceIndex = Model->Nodes(NodeGroup->Nodes(0)).iSurf;

			// fill out the NodeGroup/mapping, as UModelComponent::GetStaticLightingInfo did
			SomeModelComponent->GetSurfaceLightMapResolution(SurfaceIndex, TRUE, NodeGroup->SizeX, NodeGroup->SizeY, NodeGroup->WorldToMap, &NodeGroup->Nodes);
			NodeGroup->MapToWorld = NodeGroup->WorldToMap.Inverse();

			// Cache the surface's vertices and triangles.
			NodeGroup->BoundingBox.Init();

			UBOOL bForceLightMap = FALSE;

			TArray<INT> ComponentVisibilityIds;
			for(INT NodeIndex = 0;NodeIndex < NodeGroup->Nodes.Num();NodeIndex++)
			{
				const FBspNode& Node = Model->Nodes(NodeGroup->Nodes(NodeIndex));
				const FBspSurf& NodeSurf = Model->Surfs(Node.iSurf);
				// If ANY surfaces in this group has ForceLightMap set, they all get it...
				if ((NodeSurf.PolyFlags & PF_ForceLightMap) > 0)
				{
					bForceLightMap = TRUE;
				}
				const FVector& TextureBase = Model->Points(NodeSurf.pBase);
				const FVector& TextureX = Model->Vectors(NodeSurf.vTextureU);
				const FVector& TextureY = Model->Vectors(NodeSurf.vTextureV);
				const INT BaseVertexIndex = NodeGroup->Vertices.Num();
				// Compute the surface's tangent basis.
				FVector NodeTangentX = Model->Vectors(NodeSurf.vTextureU).SafeNormal();
				FVector NodeTangentY = Model->Vectors(NodeSurf.vTextureV).SafeNormal();
				FVector NodeTangentZ = Model->Vectors(NodeSurf.vNormal).SafeNormal();

				// Generate the node's vertices.
				for(UINT VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
				{
					const FVert& Vert = Model->Verts(Node.iVertPool + VertexIndex);
					const FVector& VertexWorldPosition = Model->Points(Vert.pVertex);

					FStaticLightingVertex* DestVertex = new(NodeGroup->Vertices) FStaticLightingVertex;
					DestVertex->WorldPosition = VertexWorldPosition;
					DestVertex->TextureCoordinates[0].X = ((VertexWorldPosition - TextureBase) | TextureX) / 128.0f;
					DestVertex->TextureCoordinates[0].Y = ((VertexWorldPosition - TextureBase) | TextureY) / 128.0f;
					DestVertex->TextureCoordinates[1].X = NodeGroup->WorldToMap.TransformFVector(VertexWorldPosition).X;
					DestVertex->TextureCoordinates[1].Y = NodeGroup->WorldToMap.TransformFVector(VertexWorldPosition).Y;
					DestVertex->WorldTangentX = NodeTangentX;
					DestVertex->WorldTangentY = NodeTangentY;
					DestVertex->WorldTangentZ = NodeTangentZ;

					// Include the vertex in the surface's bounding box.
					NodeGroup->BoundingBox += VertexWorldPosition;
				}

				// Generate the node's vertex indices.
				for(UINT VertexIndex = 2;VertexIndex < Node.NumVertices;VertexIndex++)
				{
					NodeGroup->TriangleVertexIndices.AddItem(BaseVertexIndex + 0);
					NodeGroup->TriangleVertexIndices.AddItem(BaseVertexIndex + VertexIndex);
					NodeGroup->TriangleVertexIndices.AddItem(BaseVertexIndex + VertexIndex - 1);

					// track the source surface for each triangle
					NodeGroup->TriangleSurfaceMap.AddItem(Node.iSurf);
				}

				UModelComponent* Component = Level->ModelComponents(Node.ComponentIndex);
				if (Component->VisibilityId == INDEX_NONE)
				{
					if (GWorld->GetWorldInfo()->bPrecomputeVisibility)
					{
						// Make sure the level gets dirtied since we are changing the visibility Id of a component in it
						bMarkLevelDirty = TRUE;
					}
					Component->VisibilityId = NextVisibilityId;
					NextVisibilityId++;
				}
				ComponentVisibilityIds.AddUniqueItem(Component->VisibilityId);
			}

			// Continue only if the component accepts lights (all components in a node group have the same value)
			// TODO: If we expose CastShadow for BSP in the future, reenable this condition and make sure
			// node grouping logic is updated to account for CastShadow as well
			//if (SomeModelComponent->bAcceptsLights || SomeModelComponent->CastShadow)
			{
				// Create the object to represent the surface's mapping/mesh to the static lighting system,
				// the model is now the owner, and all nodes have the same 
				FBSPSurfaceStaticLighting* SurfaceStaticLighting = new FBSPSurfaceStaticLighting(NodeGroup, Model, SomeModelComponent, bForceLightMap);
				// Give the surface mapping the visibility Id's of all components that have nodes in it
				// This results in fairly ineffective precomputed visibility with BSP but is necessary since BSP mappings contain geometry from multiple components
				SurfaceStaticLighting->VisibilityIds = ComponentVisibilityIds;

				Meshes.AddItem(SurfaceStaticLighting);
				if (Options.bUseLightmass == TRUE)
				{
					LightingMeshBounds += SurfaceStaticLighting->BoundingBox;
				}
				else
				{
					AggregateMesh.AddMesh(SurfaceStaticLighting);
				}
				if (bBuildLightingForBSP || Options.bUseLightmass)
				{
					FStaticLightingMapping* CurrentMapping = SurfaceStaticLighting;
					if (GLightmassDebugOptions.bSortMappings)
					{
						INT InsertIndex = UnSortedMappings.AddZeroed();
						FStaticLightingMappingSortHelper& Helper = UnSortedMappings(InsertIndex);
						Helper.Mapping = CurrentMapping;
						Helper.NumTexels = CurrentMapping->GetTexelCount();
					}
					else
					{
						Mappings.AddItem(CurrentMapping);
						if (GLightmassDebugOptions.bUseDeterministicLighting && bBuildLightingForBSP)
						{
							CurrentMapping->Mesh->Guid = FGuid(0,0,0,DeterministicIndex++);
						}
					}

					if (bBuildLightingForBSP)
					{
						CurrentMapping->bProcessMapping = SomeModelComponent->bAcceptsLights;
						if (Options.bUseLightmass)
						{
							// Add it to the CustomImportanceVolume
							CustomImportanceBoundingBox += SurfaceStaticLighting->BoundingBox;
						}
					}
					else
					{
						// If any mapping is skipped, we assume we are using the custom importance volume.
						bLocalUseCustomImportanceVolume = TRUE;
					}

					if (SomeModelComponent->bAcceptsLights)
					{
						// count how many node groups have yet to come back as complete
						Model->NumIncompleteNodeGroups++;

						// add this mapping to the list of mappings to be applied later
						Model->CachedMappings.AddItem(SurfaceStaticLighting);
					}
				}

				// Set the bUseCustomImportanceVolume flag if any mappings were skipped.
				if (bLocalUseCustomImportanceVolume == TRUE)
				{
					bUseCustomImportanceVolume = TRUE;
				}
			}
		}
	}

	if (bMarkLevelDirty)
	{
		Level->MarkPackageDirty();
	}
}

/**
 * Generates mappings/meshes for the given NodeGroups
 *
 * @param Level					Level to build BSP lighting info for
 * @param NodeGroupsToBuild		The node groups to build the BSP lighting info for
 */
void FStaticLightingSystem::AddBSPStaticLightingInfo(ULevel* Level, TArray<FNodeGroup*>& NodeGroupsToBuild)
{
	// For BSP, we aren't Component-centric, so we can't use the GetStaticLightingInfo 
	// function effectively. Instead, we look across all nodes in the Level's model and
	// generate NodeGroups - which are groups of nodes that are coplanar, adjacent, and 
	// have the same lightmap resolution (henceforth known as being "conodes"). Each 
	// NodeGroup will get a mapping created for it

	// cache the model
	UModel* Model = Level->Model;

	// reset the number of incomplete groups
	Model->NumIncompleteNodeGroups = 0;
	Model->CachedMappings.Empty();

	// now we need to make the mappings/meshes
	UBOOL bLocalUseCustomImportanceVolume = TRUE;
	for (INT NodeGroupIdx = 0; NodeGroupIdx < NodeGroupsToBuild.Num(); NodeGroupIdx++)
	{
		FNodeGroup* NodeGroup = NodeGroupsToBuild(NodeGroupIdx);
		if (NodeGroup && NodeGroup->Nodes.Num())
		{
			// get one of the surfaces/components from the NodeGroup
			// @lmtodo: Remove need for GetSurfaceLightMapResolution to take a surfaceindex, or a ModelComponent :)
			UModelComponent* SomeModelComponent = Level->ModelComponents(Model->Nodes(NodeGroup->Nodes(0)).ComponentIndex);
			INT SurfaceIndex = Model->Nodes(NodeGroup->Nodes(0)).iSurf;

			// fill out the NodeGroup/mapping, as UModelComponent::GetStaticLightingInfo did
			SomeModelComponent->GetSurfaceLightMapResolution(SurfaceIndex, TRUE, NodeGroup->SizeX, NodeGroup->SizeY, NodeGroup->WorldToMap, &NodeGroup->Nodes);
			NodeGroup->MapToWorld = NodeGroup->WorldToMap.Inverse();

			// Cache the surface's vertices and triangles.
			NodeGroup->BoundingBox.Init();

			UBOOL bForceLightMap = FALSE;

			for(INT NodeIndex = 0;NodeIndex < NodeGroup->Nodes.Num();NodeIndex++)
			{
				const FBspNode& Node = Model->Nodes(NodeGroup->Nodes(NodeIndex));
				const FBspSurf& NodeSurf = Model->Surfs(Node.iSurf);
				// If ANY surfaces in this group has ForceLightMap set, they all get it...
				if ((NodeSurf.PolyFlags & PF_ForceLightMap) > 0)
				{
					bForceLightMap = TRUE;
				}
				const FVector& TextureBase = Model->Points(NodeSurf.pBase);
				const FVector& TextureX = Model->Vectors(NodeSurf.vTextureU);
				const FVector& TextureY = Model->Vectors(NodeSurf.vTextureV);
				const INT BaseVertexIndex = NodeGroup->Vertices.Num();
				// Compute the surface's tangent basis.
				FVector NodeTangentX = Model->Vectors(NodeSurf.vTextureU).SafeNormal();
				FVector NodeTangentY = Model->Vectors(NodeSurf.vTextureV).SafeNormal();
				FVector NodeTangentZ = Model->Vectors(NodeSurf.vNormal).SafeNormal();

				// Generate the node's vertices.
				for(UINT VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
				{
					const FVert& Vert = Model->Verts(Node.iVertPool + VertexIndex);
					const FVector& VertexWorldPosition = Model->Points(Vert.pVertex);

					FStaticLightingVertex* DestVertex = new(NodeGroup->Vertices) FStaticLightingVertex;
					DestVertex->WorldPosition = VertexWorldPosition;
					DestVertex->TextureCoordinates[0].X = ((VertexWorldPosition - TextureBase) | TextureX) / 128.0f;
					DestVertex->TextureCoordinates[0].Y = ((VertexWorldPosition - TextureBase) | TextureY) / 128.0f;
					DestVertex->TextureCoordinates[1].X = NodeGroup->WorldToMap.TransformFVector(VertexWorldPosition).X;
					DestVertex->TextureCoordinates[1].Y = NodeGroup->WorldToMap.TransformFVector(VertexWorldPosition).Y;
					DestVertex->WorldTangentX = NodeTangentX;
					DestVertex->WorldTangentY = NodeTangentY;
					DestVertex->WorldTangentZ = NodeTangentZ;

					// Include the vertex in the surface's bounding box.
					NodeGroup->BoundingBox += VertexWorldPosition;
				}

				// Generate the node's vertex indices.
				for(UINT VertexIndex = 2;VertexIndex < Node.NumVertices;VertexIndex++)
				{
					NodeGroup->TriangleVertexIndices.AddItem(BaseVertexIndex + 0);
					NodeGroup->TriangleVertexIndices.AddItem(BaseVertexIndex + VertexIndex);
					NodeGroup->TriangleVertexIndices.AddItem(BaseVertexIndex + VertexIndex - 1);

					// track the source surface for each triangle
					NodeGroup->TriangleSurfaceMap.AddItem(Node.iSurf);
				}
			}

			// Continue only if the component accepts lights (all components in a node group have the same value)
			// TODO: If we expose CastShadow for BSP in the future, reenable this condition and make sure
			// node grouping logic is updated to account for CastShadow as well
			//if (SomeModelComponent->bAcceptsLights || SomeModelComponent->CastShadow)
			{
				// Create the object to represent the surface's mapping/mesh to the static lighting system,
				// the model is now the owner, and all nodes have the same 
				FBSPSurfaceStaticLighting* SurfaceStaticLighting = new FBSPSurfaceStaticLighting(NodeGroup, Model, SomeModelComponent, bForceLightMap);
				Meshes.AddItem(SurfaceStaticLighting);
				if (Options.bUseLightmass == TRUE)
				{
					LightingMeshBounds += SurfaceStaticLighting->BoundingBox;
				}
				else
				{
					AggregateMesh.AddMesh(SurfaceStaticLighting);
				}

				FStaticLightingMapping* CurrentMapping = SurfaceStaticLighting;
				if (GLightmassDebugOptions.bSortMappings)
				{
					INT InsertIndex = UnSortedMappings.AddZeroed();
					FStaticLightingMappingSortHelper& Helper = UnSortedMappings(InsertIndex);
					Helper.Mapping = CurrentMapping;
					Helper.NumTexels = CurrentMapping->GetTexelCount();
				}
				else
				{
					Mappings.AddItem(CurrentMapping);
					if (GLightmassDebugOptions.bUseDeterministicLighting)
					{
						CurrentMapping->Mesh->Guid = FGuid(0,0,0,DeterministicIndex++);
					}
				}

				CurrentMapping->bProcessMapping = SomeModelComponent->bAcceptsLights;
				if (Options.bUseLightmass)
				{
					// Add it to the CustomImportanceVolume
					CustomImportanceBoundingBox += SurfaceStaticLighting->BoundingBox;
				}

				if (SomeModelComponent->bAcceptsLights)
				{
					// count how many node groups have yet to come back as complete
					Model->NumIncompleteNodeGroups++;

					// add this mapping to the list of mappings to be applied later
					Model->CachedMappings.AddItem(SurfaceStaticLighting);
				}

				// Set the bUseCustomImportanceVolume flag if any mappings were skipped.
				if (bLocalUseCustomImportanceVolume == TRUE)
				{
					bUseCustomImportanceVolume = TRUE;
				}
			}
		}
	}
}

void FStaticLightingSystem::AddPrimitiveStaticLightingInfo(FStaticLightingPrimitiveInfo& PrimitiveInfo, UBOOL bBuildActorLighting, UBOOL bAcceptsLights)
{
	// Verify a one to one relationship between mappings and meshes
	//@todo - merge FStaticLightingMesh and FStaticLightingMapping
	check(PrimitiveInfo.Meshes.Num() == PrimitiveInfo.Mappings.Num());

	// Add the component's shadow casting meshes to the system.
	for(INT MeshIndex = 0;MeshIndex < PrimitiveInfo.Meshes.Num();MeshIndex++)
	{
		FStaticLightingMesh* Mesh = PrimitiveInfo.Meshes(MeshIndex);
		Mesh->VisibilityIds.AddItem(PrimitiveInfo.VisibilityId);
		if (GLightmassDebugOptions.bUseDeterministicLighting && !GLightmassDebugOptions.bSortMappings && bBuildActorLighting)
		{
			if (Mesh)
			{
				Mesh->Guid = FGuid(0,0,0,DeterministicIndex++);
			}
		}
		Meshes.AddItem(Mesh);
		if (Options.bUseLightmass == TRUE)
		{
			LightingMeshBounds += Mesh->BoundingBox;
		}
		else
		{
			AggregateMesh.AddMesh(Mesh);
		}
	}

	// If lighting is being built for this component, add its mappings to the system.
	if (bBuildActorLighting || Options.bUseLightmass)
	{
		UBOOL bLocalUseCustomImportanceVolume = FALSE;
		for(INT MappingIndex = 0;MappingIndex < PrimitiveInfo.Mappings.Num();MappingIndex++)
		{
			FStaticLightingMapping* CurrentMapping = PrimitiveInfo.Mappings(MappingIndex);
			if (GLightmassDebugOptions.bUseDeterministicLighting && GbLogAddingMappings)
			{
				FStaticLightingMesh* SLMesh = CurrentMapping->Mesh;
				if (SLMesh)
				{
					debugfSlow(TEXT("Adding %32s: 0x%08p - %s"), *(CurrentMapping->GetDescription()), (PTRINT)(SLMesh->Component), *(SLMesh->Guid.String()));
				}
				else
				{
					debugfSlow(TEXT("Adding %32s: 0x%08x - %s"), *(CurrentMapping->GetDescription()), 0, TEXT("NO MESH????"));
				}
			}

			if (bBuildActorLighting)
			{
				// Verify that we only process a mapping if it accepts lights
				check(bAcceptsLights);
				CurrentMapping->bProcessMapping = bAcceptsLights;

				// Add it to the CustomImportanceVolume
				if (Options.bUseLightmass)
				{
					CustomImportanceBoundingBox += CurrentMapping->Mesh->BoundingBox;
				}
			}
			else
			{
				// If any mapping is skipped, we assume we are using the custom importance volume.
				bLocalUseCustomImportanceVolume = TRUE;
			}

			if (GLightmassDebugOptions.bSortMappings)
			{
				INT InsertIndex = UnSortedMappings.AddZeroed();
				FStaticLightingMappingSortHelper& Helper = UnSortedMappings(InsertIndex);
				Helper.Mapping = CurrentMapping;
				Helper.NumTexels = Helper.Mapping->GetTexelCount();
			}
			else
			{
				Mappings.AddItem(CurrentMapping);
			}
		}

		// Set the bUseCustomImportanceVolume flag if any mappings were skipped.
		if (bLocalUseCustomImportanceVolume == TRUE)
		{
			bUseCustomImportanceVolume = TRUE;
		}
	}
}

#if WITH_MANAGED_CODE
/**
 * Exports the scene to Lightmass and starts the process.
 * @return	TRUE if lighting was successful, otherwise FALSE
 **/
UBOOL FStaticLightingSystem::LightmassProcess()
{
	// A separate statistics structure for tracking this routines times
	FLightmassStatistics LightmassProcessStatistics;

	GWarn->StatusUpdatef( 0, Meshes.Num() + Mappings.Num(), TEXT("Collecting the scene...") );

	DOUBLE SwarmStartupStartTime = appSeconds();

	if (Options.bOnlyBuildVisibility && !GWorld->GetWorldInfo()->bPrecomputeVisibility)
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd("BuildFailed_VisibilityOnlyButVisibilityDisabled"));
		return FALSE;
	}

	// Create the processor
	FLightmassProcessor* LightmassProcessor = new FLightmassProcessor(*this, Options.bDumpBinaryResults, Options.bOnlyBuildVisibility, Options.bOnlyBuildVisibleLevels);
	check(LightmassProcessor);
	if (LightmassProcessor->IsSwarmConnectionIsValid() == FALSE)
	{
		warnf(NAME_Warning, TEXT("Failed to connect to Swarm."));
		appMsgf(AMT_OK, TEXT("Failed to connect to Swarm."));
		delete LightmassProcessor;
		LightmassProcessStatistics.SwarmStartupTime += appSeconds() - SwarmStartupStartTime;
		return FALSE;
	}

	LightmassProcessStatistics.SwarmStartupTime += appSeconds() - SwarmStartupStartTime;
	DOUBLE CollectLightmassSceneStartTime = appSeconds();

	// Grab the exporter and fill in the meshes
	//@todo. This should be exported to the 'processor' as it will be used on the input side as well...
	FLightmassExporter* LightmassExporter = LightmassProcessor->GetLightmassExporter();
	check(LightmassExporter);

	// The Level settings...
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
	if (WorldInfo)
	{
		LightmassExporter->SetLevelSettings(WorldInfo->LightmassSettings);
	}
	else
	{
		FLightmassWorldInfoSettings TempSettings;
		LightmassExporter->SetLevelSettings(TempSettings);
	}
	LightmassExporter->SetNumUnusedLocalCores(Options.NumUnusedLocalCores);
	LightmassExporter->SetQualityLevel(Options.QualityLevel);
	LightmassExporter->SetbVisibleLevelsOnly(Options.bOnlyBuildVisibleLevels);

	if (GWorld->PersistentLevel && Options.ShouldBuildLightingForLevel( GWorld->PersistentLevel ))
	{
		LightmassExporter->SetLevelName(GWorld->PersistentLevel->GetPathName());
	}

	LightmassExporter->ClearImportanceVolumes();
	for( TObjectIterator<ALightmassImportanceVolume> It ; It ; ++It )
	{
		ALightmassImportanceVolume* LMIVolume = *It;
		if (GWorld->ContainsActor(LMIVolume))
		{
			LightmassExporter->AddImportanceVolume(LMIVolume);
		}
	}

	for( TObjectIterator<ALightmassCharacterIndirectDetailVolume> It ; It ; ++It )
	{
		ALightmassCharacterIndirectDetailVolume* LMDetailVolume = *It;
		if (GWorld->ContainsActor(LMDetailVolume))
		{
			LightmassExporter->AddCharacterIndirectDetailVolume(LMDetailVolume);
		}
	}

	if (bUseCustomImportanceVolume)
	{
		FLOAT CustomImportanceVolumeExpandBy = 0.0f;
		verify(GConfig->GetFloat(TEXT("DevOptions.StaticLightingSceneConstants"), TEXT("CustomImportanceVolumeExpandBy"), CustomImportanceVolumeExpandBy, GLightmassIni));
		CustomImportanceBoundingBox.ExpandBy(CustomImportanceVolumeExpandBy);
		LightmassExporter->SetCustomImportanceVolume(CustomImportanceBoundingBox);
	}
	else
	{
		FLOAT MinimumImportanceVolumeExtentWithoutWarning = 0.0f;
		verify(GConfig->GetFloat(TEXT("DevOptions.StaticLightingSceneConstants"), TEXT("MinimumImportanceVolumeExtentWithoutWarning"), MinimumImportanceVolumeExtentWithoutWarning, GLightmassIni));

		// If the scene is large enough...
		if (LightingMeshBounds.GetExtent().SizeSquared() > (MinimumImportanceVolumeExtentWithoutWarning * MinimumImportanceVolumeExtentWithoutWarning))
		{
			// And the importance volume is too small...
			if (LightmassExporter->GetImportanceVolumes().Num() == 0)
			{
				// Emit a serious warning to the user about performance.
				GWarn->LightingBuild_Add(MCTYPE_CRITICALERROR, NULL, *Localize(TEXT("Lightmass"), TEXT("LightmassError_MissingImportanceVolume"), TEXT("UnrealEd")));
			}
		}
	}

	// Meshes
	for( INT MeshIdx=0; !GEditor->GetMapBuildCancelled() && MeshIdx < Meshes.Num(); MeshIdx++ )
	{
		Meshes(MeshIdx)->ExportMeshInstance(LightmassExporter);
		GWarn->UpdateProgress( MeshIdx, Meshes.Num() + Mappings.Num() );
	}

	// Mappings
	for( INT MappingIdx=0; !GEditor->GetMapBuildCancelled() && MappingIdx < Mappings.Num(); MappingIdx++ )
	{
		Mappings(MappingIdx)->ExportMapping(LightmassExporter);
		GWarn->UpdateProgress( Meshes.Num() + MappingIdx, Meshes.Num() + Mappings.Num() );
	}

	for (TSparseArray<UDominantSpotLightComponent*>::TConstIterator LightIt(GWorld->DominantSpotLights); LightIt; ++LightIt)
	{
		UDominantSpotLightComponent* CurrentLight = *LightIt;
		// Make sure all dominant spot lights get exported even if they don't affect any primitives, 
		// Because we need to send a dominant shadow task to Lightmass for them anyway
		LightmassExporter->AddLight(CurrentLight);
	}

	if (GWorld->DominantDirectionalLight && !GWorld->DominantDirectionalLight->GetOwner()->bMovable)
	{
		LightmassExporter->AddLight(GWorld->DominantDirectionalLight);
	}

	LightmassProcessStatistics.CollectLightmassSceneTime += appSeconds() - CollectLightmassSceneStartTime;

	// Run!
	UBOOL bSuccessful = FALSE;
	UBOOL bOpenJobSuccessful = FALSE;
	if ( !GEditor->GetMapBuildCancelled() )
	{
		debugf(TEXT("Running Lightmass w/ Deterministic Lighting %s"), GLightmassDebugOptions.bUseDeterministicLighting ? TEXT("ENABLED") : TEXT("DISABLED"));
		LightmassProcessor->SetUseDeterministicLighting(GLightmassDebugOptions.bUseDeterministicLighting);
		debugf(TEXT("Running Lightmass w/ ImmediateImport mode %s"), GLightmassDebugOptions.bUseImmediateImport ? TEXT("ENABLED") : TEXT("DISABLED"));
		LightmassProcessor->SetImportCompletedMappingsImmediately(GLightmassDebugOptions.bUseImmediateImport);
		debugf(TEXT("Running Lightmass w/ ImmediateProcess mode %s"), GLightmassDebugOptions.bImmediateProcessMappings ? TEXT("ENABLED") : TEXT("DISABLED"));
		LightmassProcessor->SetProcessCompletedMappingsImmediately(GLightmassDebugOptions.bImmediateProcessMappings);
		debugf(TEXT("Running Lightmass w/ Sorting mode %s"), GLightmassDebugOptions.bSortMappings ? TEXT("ENABLED") : TEXT("DISABLED"));
		debugf(TEXT("Running Lightmass w/ Mapping paddings %s"), GLightmassDebugOptions.bPadMappings ? TEXT("ENABLED") : TEXT("DISABLED"));
		debugf(TEXT("Running Lightmass w/ Mapping debug paddings %s"), GLightmassDebugOptions.bDebugPaddings ? TEXT("ENABLED") : TEXT("DISABLED"));

		DOUBLE SwarmJobStartTime = appSeconds();
		bOpenJobSuccessful = LightmassProcessor->OpenJob();
		LightmassProcessStatistics.SwarmJobTime += appSeconds() - SwarmJobStartTime;

		if (bOpenJobSuccessful)
		{
			bSuccessful = LightmassProcessor->Run();
		}
	}

	if (bSuccessful && GLightmassDebugOptions.bUseDeterministicLighting)
	{
		CompleteDeterministicMappings(LightmassProcessor);
	}

	if (bOpenJobSuccessful)
	{
		DOUBLE CloseJobStartTime = appSeconds();
		bSuccessful = LightmassProcessor->CloseJob() && bSuccessful;
		LightmassProcessStatistics.SwarmJobTime += appSeconds() - CloseJobStartTime;
	}

	// Add in the time measurements from the LightmassProcessor
	LightmassStatistics += LightmassProcessor->GetStatistics();

	// We're done.
	delete LightmassProcessor;

	// Check the for build cancellation.
	bBuildCanceled = bBuildCanceled || GEditor->GetMapBuildCancelled();
	bSuccessful = bSuccessful && !bBuildCanceled;
	if (bSuccessful)
	{
		GWarn->LightingBuildInfo_Refresh();
	}

	// Finish up timing statistics
	LightmassStatistics += LightmassProcessStatistics;

	return bSuccessful;
}
#endif //#if WITH_MANAGED_CODE

/**
 * Clear out all the binary dump log files, so the next run will have just the needed files
 * @param bUseLightmass If TRUE, this lighting was done using offline rendering
 */
void FStaticLightingSystem::ClearBinaryDumps(UBOOL bUseLightmass)
{
	GFileManager->DeleteDirectory(*FString::Printf(TEXT("%sLogs\\Lighting_%s"), *appGameDir(), bUseLightmass ? TEXT("Lightmass") : TEXT("UnrealEd")), FALSE, TRUE);
}

/**
 * Dump texture map data to a series of output binary files
 *
 * @param MappingGuid Guid of the mapping this set of texture maps belongs to
 * @param LightMapData Light map information
 * @param ShadowMaps Collection of shadow maps to dump
 * @param bUseLightmass If TRUE, this lighting was done using offline rendering
 */
void FStaticLightingSystem::DumpLightMapsToDisk(const FGuid& MappingGuid, const FString& Description, FLightMapData2D* LightMapData, TMap<ULightComponent*,FShadowMapData2D*>& ShadowMaps, UBOOL bUseLightmass)
{
	FString BaseFilename = FString::Printf(TEXT("%sLogs\\Lighting_%s\\%s_%s_T"), *appGameDir(), bUseLightmass ? TEXT("Lightmass") : TEXT("UnrealEd"), 
		*MappingGuid.String(), *Description);

	// write out the 2d lightmap data
	FArchive* OutLightmap = GFileManager->CreateFileWriter(*FString::Printf(TEXT("%s_LM.bin"), *BaseFilename));
	for (UINT Y = 0; Y < LightMapData->GetSizeY(); Y++)
	{
		for (UINT X = 0; X < LightMapData->GetSizeX(); X++)
		{
			OutLightmap->Serialize(&(*LightMapData)(X, Y), sizeof(FLightSample));
		}
	}
	delete OutLightmap;

	INT ShadowIndex = 0;
	// write out all 2d shadowmap data
	for (TMap<ULightComponent*,FShadowMapData2D*>::TIterator It(ShadowMaps); It; ++It, ++ShadowIndex)
	{
		FArchive* OutShadowMap= GFileManager->CreateFileWriter(*FString::Printf(TEXT("%s_SM_%d.bin"), *BaseFilename, ShadowIndex));
		It.Value()->Serialize(OutShadowMap);
		delete OutShadowMap;
	}
}

/**
 * Dump vertex map data to a series of output binary files
 *
 * @param MappingGuid Guid of the mapping this set of vertex maps belongs to
 * @param LightMapData Light map information
 * @param ShadowMaps Collection of shadow maps to dump
 * @param bUseLightmass If TRUE, this lighting was done using offline rendering
 */
void FStaticLightingSystem::DumpLightMapsToDisk(const FGuid& MappingGuid, const FString& Description, FLightMapData1D* LightMapData, TMap<ULightComponent*,FShadowMapData1D*>& ShadowMaps, UBOOL bUseLightmass)
{
	// get mapping guid
	FString BaseFilename = FString::Printf(TEXT("%sLogs\\Lighting_%s\\%s_%s_V"), *appGameDir(), bUseLightmass ? TEXT("Lightmass") : TEXT("UnrealEd"), 
		*MappingGuid.String(), *Description);

	// write out the 1d lightmap data
	FArchive* OutLightmap = GFileManager->CreateFileWriter(*FString::Printf(TEXT("%s_LM.bin"), *BaseFilename));
	for (INT X = 0; X < LightMapData->GetSize(); X++)
	{
		OutLightmap->Serialize(&(*LightMapData)(X), sizeof(FLightSample));
	}
	delete OutLightmap;

	INT ShadowIndex = 0;
	// write out all 1d shadowmap data
	for (TMap<ULightComponent*,FShadowMapData1D*>::TIterator It(ShadowMaps); It; ++It, ++ShadowIndex)
	{
		FShadowMapData1D* Shadow = It.Value();
		FArchive* OutShadowMap = GFileManager->CreateFileWriter(*FString::Printf(TEXT("%s_SM_%d.bin"), *BaseFilename, ShadowIndex));
		for (INT X = 0; X < Shadow->GetSize(); X++)
		{
			OutShadowMap->Serialize(&(*Shadow)(X), sizeof(FLOAT));
		}
		delete OutShadowMap;
	}
}

/** Marks all lights used in the calculated lightmap as used in a lightmap, and calls Apply on the texture mapping. */
void FStaticLightingSystem::ApplyMapping(
	FStaticLightingTextureMapping* TextureMapping,
	FLightMapData2D* LightMapData, 
	const TMap<ULightComponent*,FShadowMapData2D*>& ShadowMapData, 
	FQuantizedLightmapData* QuantizedData) const
{
	TArray<FGuid>& LightGuids = LightMapData ? LightMapData->LightGuids : QuantizedData->LightGuids;
	// Flag the lights in the mapping's light-map as having been used in a light-map.
	for (INT GuidIndex = 0; GuidIndex < LightGuids.Num(); GuidIndex++)
	{
		FGuid& CurrentGuid = LightGuids(GuidIndex);
		ULightComponent* FoundLight = GuidToLightMap.FindRef(CurrentGuid);
		if (FoundLight)
		{
			FoundLight->bHasLightEverBeenBuiltIntoLightMap = TRUE;
		}
		else
		{
			// Mesh area lights currently export with a 0 Guid
			//@todo - Create a representation of the mesh area light for dynamic lighting in game
			check(CurrentGuid == FGuid(0,0,0,0));
		}
	}
	TextureMapping->Apply(LightMapData, ShadowMapData, QuantizedData);
}

/** Marks all lights used in the calculated lightmap as used in a lightmap, and calls Apply on the vertex mapping. */
void FStaticLightingSystem::ApplyMapping(
	FStaticLightingVertexMapping* VertexMapping,
	FLightMapData1D* LightMapData, 
	const TMap<ULightComponent*,FShadowMapData1D*>& ShadowMapData, 
	FQuantizedLightmapData* QuantizedData) const
{
	TArray<FGuid>& LightGuids = LightMapData ? LightMapData->LightGuids : QuantizedData->LightGuids;
	// Flag the lights in the mapping's light-map as having been used in a light-map.
	for (INT GuidIndex = 0; GuidIndex < LightGuids.Num(); GuidIndex++)
	{
		FGuid& CurrentGuid = LightGuids(GuidIndex);
		ULightComponent* FoundLight = GuidToLightMap.FindRef(CurrentGuid);
		if (FoundLight)
		{
			FoundLight->bHasLightEverBeenBuiltIntoLightMap = TRUE;
		}
		else
		{
			// Mesh area lights currently export with a 0 Guid
			//@todo - Create a representation of the mesh area light for dynamic lighting in game
			check(CurrentGuid == FGuid(0,0,0,0));
		}
	}
	VertexMapping->Apply(LightMapData, ShadowMapData, QuantizedData);
}

#endif

/**
 * Builds lighting information depending on passed in options.
 *
 * @param	Options		Options determining on what and how lighting is built
 */
void UEditorEngine::BuildLighting(const FLightingBuildOptions& Options)
{
#if !CONSOLE

	// Forcibly shut down all texture property windows as they become invalid during a light build
	TArray<FTrackableEntry> TrackableWindows;
	WxTrackableWindow::GetTrackableWindows( TrackableWindows );

	// Iterate through each trackable window, finding any texture property dialogs.
	for( INT WindowIdx = 0; WindowIdx < TrackableWindows.Num(); ++WindowIdx )
	{
		wxWindow* window = TrackableWindows(WindowIdx).Window;
		if( window->GetId() == XRCID("ID_DLG_TEXTURE_PROPERTIES") )
		{
			// Send a close event to each texture property window to properly shut down.
			wxCloseEvent closeEvent( wxEVT_CLOSE_WINDOW );
			window->GetEventHandler()->ProcessEvent( closeEvent );
		}
	}
	
	// Invoke the static lighting system.
	{FStaticLightingSystem System(Options);}
#endif

	// Commit the changes to the world's BSP surfaces.
	GWorld->CommitModelSurfaces();

	// Regenerate LOD texture
	GUnrealEd->RegenerateProcBuildingTextures(Options.bGenerateBuildingLODTex);

	TArray<AImageReflectionSceneCapture*> EmptyArray;
	GUnrealEd->RegenerateImageReflectionTextures(EmptyArray);
}

/** Renders the scene to texture for each ImageReflectionSceneCapture in ReflectionActors, or all the ones in the scene if ReflectionActors is empty. */
void UUnrealEdEngine::RegenerateImageReflectionTextures(const TArray<AImageReflectionSceneCapture*>& ReflectionActors)
{
	TArray<AImageReflectionSceneCapture*> Reflections = ReflectionActors;

	if (Reflections.Num() == 0)
	{
		for (TObjectIterator<AImageReflectionSceneCapture> ReflectionIt; ReflectionIt; ++ReflectionIt)
		{
			AImageReflectionSceneCapture* Reflection = *ReflectionIt;
			const UBOOL bReflectionIsInWorld = GWorld->ContainsActor(Reflection);
			if (bReflectionIsInWorld && Reflection->ImageReflectionComponent->bEnabled)
			{
				Reflections.AddUniqueItem(Reflection);
			}
		}
	}
	
	if (Reflections.Num() > 0)
	{
		GWarn->PushStatus();
		GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("RegenerateImageReflectionTextures")), FALSE);

		const DOUBLE StartTime = appSeconds();
		
		// Allocate a scratch pad render target
		UTextureRenderTarget2D* TempRenderTarget = CastChecked<UTextureRenderTarget2D>(UObject::StaticConstructObject(UTextureRenderTarget2D::StaticClass(), UObject::GetTransientPackage(), NAME_None, RF_Transient));
		// Use a floating point format so we can access the linear HDR color values after the render
		TempRenderTarget->Init(GEngine->ImageReflectionTextureSize, GEngine->ImageReflectionTextureSize, PF_FloatRGB);
		check(TempRenderTarget->GameThread_GetRenderTargetResource());

		for (INT ReflectionIndex = 0; ReflectionIndex < Reflections.Num(); ReflectionIndex++)
		{
			AImageReflectionSceneCapture* Reflection = Reflections(ReflectionIndex);
			GWarn->StatusUpdatef(ReflectionIndex, Reflections.Num(), *FString::Printf( TEXT("%s"), *Reflection->GetPathName()));

			extern void GenerateImageReflectionTexture(AImageReflectionSceneCapture* Reflection, UTextureRenderTarget2D* TempRenderTarget);
			GenerateImageReflectionTexture(Reflection, TempRenderTarget);
		}

		// Make sure the scratch pad gets destroyed
		UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

		debugf(TEXT("Finish RegenerateImageReflectionTextures (Took %f secs)"), appSeconds() - StartTime);

		GWarn->EndSlowTask();
		GWarn->PopStatus();

		// make sure the new stuff shows up in the GB
		GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateAssetListUI | CBR_ObjectCreated));
	}
}

/**
 * Creates multiple worker threads and starts the process locally.
 **/
void FStaticLightingSystem::MultithreadProcess( UINT NumStaticLightingThreads )
{
	// Prepare the aggregate mesh for raytracing.
	AggregateMesh.PrepareForRaytracing();

	// Spawn the static lighting threads.
	for(UINT ThreadIndex = 1;ThreadIndex < NumStaticLightingThreads;ThreadIndex++)
	{
		FStaticLightingThreadRunnable* ThreadRunnable = new(Threads) FStaticLightingThreadRunnable(this);
		ThreadRunnable->Thread = GThreadFactory->CreateThread(ThreadRunnable, TEXT("StaticLightingThread"), 0, 0, 0, TPri_Normal);
	}

	// Start the static lighting thread loop on the main thread, too.
	// Once it returns, all static lighting mappings have begun processing.
	ThreadLoop(TRUE);

	// Stop the static lighting threads.
	for(INT ThreadIndex = 0;ThreadIndex < Threads.Num();ThreadIndex++)
	{
		// Wait for the thread to exit.
		Threads(ThreadIndex).Thread->WaitForCompletion();

		// Check that it didn't terminate with an error.
		Threads(ThreadIndex).CheckHealth();

		// Destroy the thread.
		GThreadFactory->Destroy(Threads(ThreadIndex).Thread);
	}
	Threads.Empty();

	// Apply the last completed mappings.
	if (!GLightmassDebugOptions.bUseDeterministicLighting)
	{
		CompleteVertexMappingList.ApplyAndClear(Options.bUseLightmass, Options.bDumpBinaryResults);
		CompleteTextureMappingList.ApplyAndClear(Options.bUseLightmass, Options.bDumpBinaryResults);
	}
	else
	{
		// In Deterministic mode, apply all mappings.
		CompleteDeterministicMappings(NULL);
	}
}
