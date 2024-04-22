/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnPath.h"
#include "BusyCursor.h"
#include "FileHelpers.h"
#include "ScopedTransaction.h"
#include "UnFaceFXSupport.h"
#include "Kismet.h"
#include "EngineSequenceClasses.h"
#include "SpeedTree.h"
#include "PropertyWindow.h"
#include "LevelUtils.h"
#include "EngineMeshClasses.h"
#include "UnObjectTools.h"
#include "UnPackageTools.h"
#include "EngineProcBuildingClasses.h"
#include "EditorLevelUtils.h"
#include "EditorBuildUtils.h"
#include "EngineAnimClasses.h"

#if WITH_MANAGED_CODE
#include "ContentBrowserShared.h"
#endif

#if WITH_FBX
#include "UnFbxExporter.h"
#endif

#if WITH_FACEFX
using namespace OC3Ent;
using namespace Face;
#endif

//@hack: this needs to be cleaned up!
static TCHAR TempStr[MAX_EDCMD], TempName[MAX_EDCMD], Temp[MAX_EDCMD];
static WORD Word1, Word2, Word4;

/**
 * Dumps a set of selected objects to debugf.
 */
static void PrivateDumpSelection(USelection* Selection)
{
	for ( USelection::TObjectConstIterator Itor = Selection->ObjectConstItor() ; Itor ; ++Itor )
	{
		UObject *CurObject = *Itor;
		if ( CurObject )
		{
			debugf(TEXT("    %s"), *CurObject->GetClass()->GetName() );
		}
		else
		{
			debugf(TEXT("    NULL object"));
		}
	}
}

UBOOL UUnrealEdEngine::Exec( const TCHAR* Stream, FOutputDevice& Ar )
{
	const TCHAR* Str = Stream;
	// disallow set commands in the editor as that modifies the default object, affecting object serialization
	if (ParseCommand(&Str, TEXT("SET")) || ParseCommand(&Str, TEXT("SETNOPEC")))
	{
		Ar.Logf(TEXT("Set commands not allowed in the editor"));
		return TRUE;
	}

	//for thumbnail reclaimation post save
	UPackage* Pkg = NULL;
	//thumbs that are loaded expressly for the sake of saving.  To be deleted again post-save
	TArray<FString> ThumbNamesToUnload;
	
	// Peek for the SavePackage command and generate thumbnails for the package if we need to
	// NOTE: The actual package saving happens in the UEditorEngine::Exec_Obj, but we do the 
	//		 thumbnail generation here in UnrealEd
	if( ParseCommand(&Str,TEXT("OBJ")) )
	{
		if( ParseCommand( &Str, TEXT( "SavePackage" ) ) )
		{
			static TCHAR TempFname[MAX_EDCMD];
			if( Parse( Str, TEXT( "FILE=" ), TempFname, 256 ) && ParseObject<UPackage>( Str, TEXT( "Package=" ), Pkg, NULL ) )
			{
				// Update any thumbnails for objects in this package that were modified or generate
				// new thumbnails for objects that don't have any

				UBOOL bSilent = FALSE;
				ParseUBOOL( Str, TEXT( "SILENT=" ), bSilent );

				// Make a list of packages to query (in our case, just the package we're saving)
				TArray< UPackage* > Packages;
				Packages.AddItem( Pkg );

				// Allocate a new thumbnail map if we need one
				if( !Pkg->ThumbnailMap.IsValid() )
				{
					Pkg->ThumbnailMap.Reset( new FThumbnailMap() );
				}

				// OK, now query all of the browsable objects in the package we're about to save
				TArray< UObject* > BrowsableObjectsInPackage;
				TArray<UClass*> ClassesUsingSharedThumbnails;
#if WITH_MANAGED_CODE
				if( FContentBrowser::IsInitialized() )
				{
					// NOTE: The package should really be fully loaded before we try to generate thumbnails
					PackageTools::GetObjectsInPackages(
						&Packages,														// Packages to search
						&FContentBrowser::GetActiveInstance().GetBrowsableObjectTypeMap(),	// Allowed object types
						BrowsableObjectsInPackage );									// Out: Objects

					ClassesUsingSharedThumbnails = *(FContentBrowser::GetActiveInstance().GetSharedThumbnailClasses());
				}
#endif	// #if WITH_MANAGED_CODE


				// Check to see if any of the objects need thumbnails generated
				TSet< UObject* > ObjectsMissingThumbnails;
				TSet< UObject* > ObjectsWithThumbnails;
				for( INT CurObjectIndex = 0; CurObjectIndex < BrowsableObjectsInPackage.Num(); ++CurObjectIndex )
				{
					UObject* CurObject = BrowsableObjectsInPackage( CurObjectIndex );
					check( CurObject != NULL );

					UBOOL bUsesSharedThumbnail = FALSE;
					for ( INT Idx = 0; Idx < ClassesUsingSharedThumbnails.Num(); Idx++ )
					{
						UClass* Cls = ClassesUsingSharedThumbnails(Idx);
						if ( CurObject->IsA(Cls) )
						{
							bUsesSharedThumbnail = TRUE;
							break;
						}
					}

					// Archetypes always use a shared thumbnail
					if( CurObject->HasAllFlags( RF_ArchetypeObject ) )
					{
						bUsesSharedThumbnail = TRUE;
					}

					UBOOL bPrintThumbnailDiagnostics = FALSE;
					GConfig->GetBool(TEXT("Thumbnails"), TEXT("Debug"), bPrintThumbnailDiagnostics, GEditorUserSettingsIni);

					const FObjectThumbnail* ExistingThumbnail = ThumbnailTools::FindCachedThumbnail( CurObject->GetFullName() );
					if (bPrintThumbnailDiagnostics)
					{
						debugf(NAME_Debug, *FString::Printf(TEXT("Saving Thumb for %s"), *CurObject->GetFullName()));
						debugf(NAME_Debug, *FString::Printf(TEXT("   Thumb existed = %d"), (ExistingThumbnail!=NULL) ? 1: 0));
						debugf(NAME_Debug, *FString::Printf(TEXT("   Shared Thumb = %d"), (bUsesSharedThumbnail) ? 1: 0));
					}
					//if it's not generatable, let's make sure it doesn't have a custom thumbnail before saving
					if (!ExistingThumbnail && bUsesSharedThumbnail)
					{
						//let it load the custom icons from disk
						// @todo CB: Batch up requests for multiple thumbnails!
						TArray< FName > ObjectFullNames;
						FName ObjectFullNameFName( *CurObject->GetFullName() );
						ObjectFullNames.AddItem( ObjectFullNameFName );

						// Load thumbnails
						FThumbnailMap& LoadedThumbnails = Pkg->AccessThumbnailMap();
						if( ThumbnailTools::LoadThumbnailsForObjects( ObjectFullNames, LoadedThumbnails ) )
						{
							//store off the names of the thumbnails that were loaded as part of a save so we can delete them after the save
							ThumbNamesToUnload.AddItem(ObjectFullNameFName.ToString());

							if (bPrintThumbnailDiagnostics)
							{
								debugf(NAME_Debug, *FString::Printf(TEXT("   Unloaded thumb loaded successfully")));
							}

							ExistingThumbnail = LoadedThumbnails.Find( ObjectFullNameFName );
							if (bPrintThumbnailDiagnostics)
							{
								debugf(NAME_Debug, *FString::Printf(TEXT("   Newly loaded thumb exists = %d"), (ExistingThumbnail!=NULL) ? 1: 0));
								if (ExistingThumbnail)
								{
									debugf(NAME_Debug, *FString::Printf(TEXT("   Thumb created after proper version = %d"), (ExistingThumbnail->IsCreatedAfterCustomThumbsEnabled()) ? 1: 0));
								}
							}

							if (ExistingThumbnail && !ExistingThumbnail->IsCreatedAfterCustomThumbsEnabled())
							{
								if (bPrintThumbnailDiagnostics)
								{
									debugf(NAME_Debug, *FString::Printf(TEXT("   WIPING OUT THUMBNAIL!!!!")));
								}

								//Casting away const to save memory behind the scenes
								FObjectThumbnail* ThumbToClear = (FObjectThumbnail*)ExistingThumbnail;
								ThumbToClear->SetImageSize(0, 0);
								ThumbToClear->AccessImageData().Empty();
							}
						}
						else
						{
							if (bPrintThumbnailDiagnostics)
							{
								debugf(NAME_Debug, *FString::Printf(TEXT("   Unloaded thumb does not exist"), (bUsesSharedThumbnail) ? 1: 0));
							}
						}
					}

					if( ExistingThumbnail != NULL && !ExistingThumbnail->IsEmpty() )
					{
						//if this asset has a generatable thumb OR the custom thumb was created after custom thumbs were allowed.
						if (!bUsesSharedThumbnail || ExistingThumbnail->IsCreatedAfterCustomThumbsEnabled())
						{
							ObjectsWithThumbnails.Add( CurObject );
							if (bPrintThumbnailDiagnostics)
							{
								debugf(NAME_Debug, *FString::Printf(TEXT("   Added to ObjectsWithThumbs List!!!!")));
							}
						}
					}
					else
					{
						if( !bUsesSharedThumbnail )
						{
							ObjectsMissingThumbnails.Add( CurObject );
							if (bPrintThumbnailDiagnostics)
							{
								debugf(NAME_Debug, *FString::Printf(TEXT("   Added to ObjectsMissingThumbs List!!!!")));
							}
						}
					}
				}


				if( BrowsableObjectsInPackage.Num() > 0 )
				{
					// Missing some thumbnails, so go ahead and try to generate them now

					// Start a busy cursor
					const FScopedBusyCursor BusyCursor;

					if( !bSilent )
					{
						const UBOOL bWantProgressMeter = TRUE;
						GWarn->BeginSlowTask( *LocalizeUnrealEd( TEXT( "SavingPackage_GeneratingThumbnails" ) ), bWantProgressMeter );
					}

					Ar.Logf( TEXT( "OBJ SavePackage: Generating thumbnails for [%i] asset(s) in package [%s] ([%i] browsable assets)..." ), ObjectsMissingThumbnails.Num(), *Pkg->GetName(), BrowsableObjectsInPackage.Num() );

					for( INT CurObjectIndex = 0; CurObjectIndex < BrowsableObjectsInPackage.Num(); ++CurObjectIndex )
					{
						UObject* CurObject = BrowsableObjectsInPackage( CurObjectIndex );
						check( CurObject != NULL );

						if( !bSilent )
						{
							GWarn->UpdateProgress( CurObjectIndex, BrowsableObjectsInPackage.Num() );
						}


						UBOOL bNeedEmptyThumbnail = FALSE;
						if( ObjectsMissingThumbnails.Contains( CurObject ) )
						{
							// Generate a thumbnail!
							FObjectThumbnail* GeneratedThumbnail = ThumbnailTools::GenerateThumbnailForObject( CurObject );
							if( GeneratedThumbnail != NULL )
							{
								Ar.Logf( TEXT( "OBJ SavePackage:     Rendered thumbnail for [%s]" ), *CurObject->GetFullName() );
							}
							else
							{
								// Couldn't generate a thumb; perhaps this object doesn't support thumbnails?
								bNeedEmptyThumbnail = TRUE;
							}
						}
						else if( !ObjectsWithThumbnails.Contains( CurObject ) )
						{
							// Even though this object uses a shared thumbnail, we'll add a "dummy thumbnail" to
							// the package (zero dimension) for all browsable assets so that the Content Browser
							// can quickly verify that existence of assets on the fly.
							bNeedEmptyThumbnail = TRUE;
						}


						// Create an empty thumbnail if we need to.  All browsable assets need at least a placeholder
						// thumbnail so the Content Browser can check for non-existent assets in the background
						if( bNeedEmptyThumbnail )
						{
							UPackage* MyOutermostPackage = CastChecked< UPackage >( CurObject->GetOutermost() );
							ThumbnailTools::CacheEmptyThumbnail( CurObject->GetFullName(), MyOutermostPackage );
						}
					}

					Ar.Logf( TEXT( "OBJ SavePackage: Finished generating thumbnails for package [%s]" ), *Pkg->GetName() );

					if( !bSilent )
					{
						GWarn->UpdateProgress( 1, 1 );
						GWarn->EndSlowTask();
					}
				}
			}
		}
	}

	UBOOL bExecSucceeded = UEditorEngine::Exec( Stream, Ar );

	//if we loaded thumbs for saving, purge them back from the package
	//append loaded thumbs onto the existing thumbs list
	if (Pkg)
	{
		for (INT ThumbRemoveIndex = 0; ThumbRemoveIndex < ThumbNamesToUnload.Num(); ++ThumbRemoveIndex)
		{
			ThumbnailTools::CacheThumbnail(ThumbNamesToUnload(ThumbRemoveIndex), NULL, Pkg);
		}
	}

	if(bExecSucceeded)
	{
		return TRUE;
	}

	if( ParseCommand(&Str, TEXT("DUMPMODELGUIDS")) )
	{
		for (TObjectIterator<UModel> It; It; ++It)
		{
			debugf(TEXT("%s Guid = '%s'"), *It->GetFullName(), *It->LightingGuid.String());
		}
	}

	if( ParseCommand(&Str, TEXT("DUMPSELECTION")) )
	{
		debugf(TEXT("Selected Actors:"));
		PrivateDumpSelection( GetSelectedActors() );
		debugf(TEXT("Selected Non-Actors:"));
		PrivateDumpSelection( GetSelectedObjects() );
	}
	//----------------------------------------------------------------------------------
	// EDIT
	//
	if( ParseCommand(&Str,TEXT("EDIT")) )
	{
		if( Exec_Edit( Str, Ar ) )
		{
			return TRUE;
		}
	}
	//------------------------------------------------------------------------------------
	// ACTOR: Actor-related functions
	//
	else if (ParseCommand(&Str,TEXT("ACTOR")))
	{
		if( Exec_Actor( Str, Ar ) )
		{
			return TRUE;
		}
	}
	//------------------------------------------------------------------------------------
	// CTRLTAB: Brings up the ctrl + tab window
	//
	else if(ParseCommand(&Str, TEXT("CTRLTAB")))
	{
		UBOOL bIsShiftDown = FALSE;

		ParseUBOOL(Str, TEXT("SHIFTDOWN="), bIsShiftDown);

		WxTrackableWindowBase::HandleCtrlTab(GApp->EditorFrame, bIsShiftDown);
	}
	//------------------------------------------------------------------------------------
	// SKELETALMESH: SkeletalMesh-related functions
	//
	else if(ParseCommand(&Str, TEXT("SKELETALMESH")))
	{
		if(Exec_SkeletalMesh(Str, Ar))
		{
			return TRUE;
		}
	}
	//------------------------------------------------------------------------------------
	// MODE management (Global EDITOR mode):
	//
	else if( ParseCommand(&Str,TEXT("MODE")) )
	{
		if( Exec_Mode( Str, Ar ) )
		{
			return TRUE;
		}
	}
	//----------------------------------------------------------------------------------
	// PIVOT
	//
	else if( ParseCommand(&Str,TEXT("PIVOT")) )
	{
		if(		Exec_Pivot( Str, Ar ) )
		{
			return TRUE;
		}
	}
	else if (ParseCommand(&Str,TEXT("BUILDLIGHTING")))
	{
		FEditorBuildUtils::EditorBuild(IDM_BUILD_LIGHTING);
	}
	// BUILD PATHS
	else if (ParseCommand(&Str,TEXT("BUILDPATHS")))
	{
		FEditorBuildUtils::EditorBuild(IDM_BUILD_AI_PATHS);
	}
	//----------------------------------------------------------------------------------
	// QUERY VALUE
	//
	else if (ParseCommand(&Str, TEXT("QUERYVALUE")))
	{
		FString Key;
		// get required key value
		if (!ParseToken(Str, Key, FALSE))
		{
			return FALSE;
		}

		FString Label;
		// get required prompt
		if (!ParseToken(Str, Label, FALSE))
		{
			return FALSE;
		}

		FString Default;
		// default is optional
		ParseToken(Str, Default, FALSE);

		wxTextEntryDialog Dlg(NULL, *Label, *Key, *Default);

		if(Dlg.ShowModal() == wxID_OK)
		{
			// if the user hit OK, pass back the result in the OutputDevice
			Ar.Log(Dlg.GetValue());
		}

		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("UNMOUNTALLFACEFX")) )
	{
#if WITH_FACEFX
		for( TObjectIterator<UFaceFXAsset> It; It; ++It )
		{
			UFaceFXAsset* Asset = *It;
			FxActor* fActor = Asset->GetFxActor();
			if(fActor)
			{
				// If its open in studio - do not modify it (warn instead).
				if(fActor->IsOpenInStudio())
				{
					appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("CannotUnmountFaceFXOpenInStudio"), *Asset->GetPathName()));
				}
				else
				{
					// Copy array, as we will be unmounting things and changing the one on the asset!
					TArray<UFaceFXAnimSet*> MountedSets = Asset->MountedFaceFXAnimSets;
					for(INT i=0; i<MountedSets.Num(); i++)
					{
						UFaceFXAnimSet* Set = MountedSets(i);
						Asset->UnmountFaceFXAnimSet(Set);
						debugf( TEXT("Unmounting: %s From %s"), *Set->GetName(), *Asset->GetName() );
					}
				}
			}
		}
#endif // WITH_FACEFX

		return TRUE;
	}
	else if (ParseCommand(&Str, TEXT("CLEANUPOLDBUILDINGTEXTURES")))
	{
		CleanupOldBuildingTextures();

		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("REGENALLPROCBUILDINGS")) )
	{
		CleanupOldBuildings(FALSE, FALSE);

		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("REGENSELPROCBUILDINGS")) )
	{
		CleanupOldBuildings(TRUE, FALSE);

		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("INSTCOMPCOUNT")) )
	{
		InstancedMeshComponentCount();

		return TRUE;
	}
	else if (ParseCommand(&Str, TEXT("FixupProcBuildingLODQuadsAfterSave")))
	{
		AProcBuilding::FixupProcBuildingLODQuadsAfterSave();
	}
	else if (ParseCommand(&Str, TEXT("FixupSkeletalMeshConversions")))
	{
		//fix up 
		for(TObjectIterator<USkeletalMeshComponent> Iter; Iter; ++Iter)
		{
			USkeletalMeshComponent* SkelMeshComponent = *Iter;
			if (SkelMeshComponent->GetOwner() && SkelMeshComponent->Animations)
			{
				//if this animation node was parented to a skeletal mesh component (not the actor or world) and it was not THIS skeletal mesh component
				USkeletalMeshComponent* ParentObject = Cast<USkeletalMeshComponent>(SkelMeshComponent->Animations->GetOuter());
				if (ParentObject && (ParentObject != SkelMeshComponent))
				{
					UAnimTree* NewTemplate = SkelMeshComponent->AnimTreeTemplate;
					if (NewTemplate == NULL)
					{
						NewTemplate = Cast<UAnimTree>(SkelMeshComponent->Animations);
					}

					//Create from template if possible
					if (NewTemplate != NULL)
					{
						SkelMeshComponent->SetAnimTreeTemplate(NewTemplate);
						SkelMeshComponent->MarkPackageDirty();
						debugf( TEXT("  %s converted successfully with template."), *(SkelMeshComponent->GetOwner()->GetName()));
					}
					else 
					{
						//copy the nodes
						TArray<UAnimNode*> SrcNodes;
						TArray<UAnimNode*> DstNodes;
						TMap<UAnimNode*,UAnimNode*> SrcToDestNodeMap;
						SrcNodes.AddItem(SkelMeshComponent->Animations);
						UAnimTree::CopyAnimNodes(SrcNodes, SkelMeshComponent, DstNodes, SrcToDestNodeMap);
						if (DstNodes.Num() == 1)
						{
							SkelMeshComponent->Animations = DstNodes(0);
							SkelMeshComponent->MarkPackageDirty();
							debugf( TEXT("  %s converted successfully with copy."), *(SkelMeshComponent->GetOwner()->GetName()));
						}
						else
						{
							debugf( TEXT("  %s failed to convert."), *(SkelMeshComponent->GetOwner()->GetName()));
						}
					}
				}
			}
		}

		return TRUE;
	}
#if WITH_EDITOR
	else if (ParseCommand(&Str, TEXT("RestoreLandscapeAfterSave")))
	{
		ALandscapeProxy::RestoreLandscapeAfterSave();
	}
	else if (ParseCommand(&Str, TEXT("RestoreLandscapeLayerInfos")))
	{
		if (!PlayWorld && GWorld && GWorld->GetWorldInfo())
		{
			// Delay SetupActor() after all Actors are loaded
			for (FActorIterator It; It; ++It)
			{
				ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*It);
				if (Proxy)
				{
					Proxy->UpdateLandscapeActor(Proxy->LandscapeActor);
					Proxy->InitRBPhysEditor();

					for (TArray<ULandscapeComponent*>::TIterator It(Proxy->LandscapeComponents); It; ++It)
					{
						ULandscapeComponent* Comp = *It;
						if (Comp)
						{
							Comp->SetupActor(TRUE);
						}
					}

					for (TArray<ULandscapeHeightfieldCollisionComponent*>::TIterator It(Proxy->CollisionComponents); It; ++It)
					{
						ULandscapeHeightfieldCollisionComponent* Comp = *It;
						if (Comp)
						{
							Comp->SetupActor(TRUE);
						}
					}
				}
			}

			// Search all layer info into LandscapeInfo
			for (TMap<FGuid, ULandscapeInfo*>::TIterator It(GWorld->GetWorldInfo()->LandscapeInfoMap); It; ++It)
			{
				ULandscapeInfo* LandscapeInfo = It.Value();
				if (LandscapeInfo)
				{
					LandscapeInfo->UpdateLayerInfoMap();

					// PostEditUndo for Components
					for ( TMap<QWORD, ULandscapeComponent*>::TIterator It(LandscapeInfo->XYtoComponentMap); It; ++It )
					{
						ULandscapeComponent* Comp = It.Value();
						if (Comp && Comp->bNeedPostUndo)
						{
							Comp->PostEditUndo();
						}
					}
				}
			}

			for (TMap<FGuid, ULandscapeInfo*>::TIterator It(GWorld->GetWorldInfo()->LandscapeInfoMap); It; ++It)
			{
				ULandscapeInfo* LandscapeInfo = It.Value();
				if (LandscapeInfo)
				{
					LandscapeInfo->CheckValidate();
				}
			}
		}
	}
	else if (ParseCommand(&Str, TEXT("UpdateLandscapeEditorData")))
	{
		if (!PlayWorld && GWorld && GWorld->GetWorldInfo())
		{
			for (FActorIterator It; It; ++It)
			{
				ALandscape* Landscape = Cast<ALandscape>(*It);
				if (Landscape)
				{
					Landscape->UpdateOldLayerInfo();
				}
				ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*It);
				if (Proxy)
				{
					Proxy->GetLandscapeInfo(); // Generate one

					// Fixed up FLandscapeLayerInfoStruct
					for (int i = 0; i < Proxy->LayerInfoObjs.Num(); ++i)
					{
						if (Proxy->LayerInfoObjs(i).Owner == NULL)
						{
							Proxy->LayerInfoObjs(i).Owner = Proxy;
						}
					}
				}
			}

			UBOOL bShouldShowMapCheckWindow = FALSE;
			// Search all layer info into LandscapeInfo
			for (TMap<FGuid, ULandscapeInfo*>::TIterator It(GWorld->GetWorldInfo()->LandscapeInfoMap); It; ++It)
			{
				ULandscapeInfo* LandscapeInfo = It.Value();
				if (LandscapeInfo)
				{
					if (LandscapeInfo->LandscapeProxy && (LandscapeInfo->LandscapeProxy->LandscapeGuid != It.Key() || LandscapeInfo->LandscapeProxy->HasAnyFlags(RF_BeginDestroyed) ))
					{
						// Remove invalid infos
						LandscapeInfo->LandscapeProxy = NULL;
						GWorld->GetWorldInfo()->LandscapeInfoMap.RemoveKey(It.Key());
					}
					else
					{
						bShouldShowMapCheckWindow |= LandscapeInfo->UpdateLayerInfoMap();
					}
				}
			}

			// for removing 
			TMap<FGuid, ALandscapeGizmoActiveActor*> GizmoMap;
			TArray<FString> ErrorLandscapeNames;

			// Delay SetupActor() after all Actors are loaded
			for (FActorIterator It; It; ++It)
			{
				ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*It);
				if (Proxy)
				{
					if (!Proxy->bIsSetup || Proxy->bResetup)
					{
						Proxy->UpdateLandscapeActor(Proxy->LandscapeActor);

						// LayerInfo fixed up
						for (INT Idx = 0; Idx < Proxy->LayerInfoObjs.Num(); Idx++)
						{
							Proxy->LayerInfoObjs(Idx).bSelected = FALSE;
							Proxy->LayerInfoObjs(Idx).DebugColorChannel = 0;
						}

						Proxy->InitRBPhysEditor();

						for (TArray<ULandscapeComponent*>::TIterator It(Proxy->LandscapeComponents); It; ++It)
						{
							ULandscapeComponent* Comp = *It;
							if (Comp)
							{
								switch(Comp->SetupActor(Proxy->bResetup))
								{
								case LSE_CollsionXY:
									if (!Proxy->bResetup)
									{
										GWarn->MapCheck_Add( MCTYPE_WARNING, Proxy, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT( "MapCheck_Message_LandscapeComponentPostLoad_Warning" )), *Proxy->GetName(), Comp->SectionBaseX, Comp->SectionBaseY) ), TEXT( "LandscapeComponentPostLoad_Warning" ), MCGROUP_DEFAULT );
										bShouldShowMapCheckWindow = TRUE;
										//Comp->ConditionalDetach();
										//Proxy->LandscapeComponents.RemoveItem(Comp);
										//It--;
									}
									break;
								case LSE_NoLandscapeInfo:
									ErrorLandscapeNames.AddItem(Proxy->GetName());
									break;
								case LSE_NoLayerInfo:
									// No Fixed Layer deletion when no LayerInfo available
									GWarn->MapCheck_Add( MCTYPE_WARNING, Comp, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT ("MapCheck_Message_NoFixedUpDeletedLayerWeightmap") ), *GetName() ) ), TEXT( "FixedUpDeletedLayerWeightmap" ) );
									bShouldShowMapCheckWindow = TRUE;
									break;
								default:
									break;
								}
							}
						}

						for (TArray<ULandscapeHeightfieldCollisionComponent*>::TIterator It(Proxy->CollisionComponents); It; ++It)
						{
							ULandscapeHeightfieldCollisionComponent* Comp = *It;
							if (Comp)
							{
								switch(Comp->SetupActor(Proxy->bResetup))
								{
								case LSE_CollsionXY:
									if (!Proxy->bResetup)
									{
										GWarn->MapCheck_Add( MCTYPE_WARNING, Proxy, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT( "MapCheck_Message_LandscapeCollisionPostLoad_Warning" )), *Proxy->GetName(), Comp->SectionBaseX, Comp->SectionBaseY ) ), TEXT( "LandscapeCollisionPostLoad_Warning" ), MCGROUP_DEFAULT );
										bShouldShowMapCheckWindow = TRUE;
										//Comp->ConditionalDetach();
										//Proxy->CollisionComponents.RemoveItem(Comp);
										//It--;
									}
									break;
								case LSE_NoLandscapeInfo:
									ErrorLandscapeNames.AddItem(Proxy->GetName());
									break;
								default:
									break;
								}
							}
						}

						Proxy->bIsSetup = TRUE;
						//Proxy->bResetup = FALSE;
					}
				}
				else
				{
					ALandscapeGizmoActiveActor* Gizmo = Cast<ALandscapeGizmoActiveActor>(*It);
					if (Gizmo && Gizmo->TargetLandscapeInfo)
					{
						if (!GizmoMap.FindRef(Gizmo->TargetLandscapeInfo->LandscapeGuid))
						{
							GizmoMap.Set(Gizmo->TargetLandscapeInfo->LandscapeGuid, Gizmo);
						}
						else
						{
							GWorld->DestroyActor(Gizmo);
						}
					}
				}
			}

			// Check for Landscape Actor is in Persistent level
			for (FActorIterator It; It; ++It)
			{
				ALandscape* Landscape = Cast<ALandscape>(*It);
				if (Landscape)
				{
					ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
					if (Info)
					{
						if (Cast<ALandscape>(Info->LandscapeProxy) != Landscape)
						{
							bShouldShowMapCheckWindow = TRUE;
							GWarn->MapCheck_Add( MCTYPE_ERROR, Landscape, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT( "MapCheck_Message_LandscapePostLoad_Warning" )), *Landscape->GetName()) ), TEXT( "LandscapePostLoad_Warning" ), MCGROUP_DEFAULT );
						}
						Landscape->bLockLocation = (Info->XYtoComponentMap.Num() != Landscape->LandscapeComponents.Num());
					}
				}

				ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*It);
				if (Proxy && Proxy->bResetup)
				{
					for (TArray<ULandscapeHeightfieldCollisionComponent*>::TIterator It(Proxy->CollisionComponents); It; ++It)
					{
						ULandscapeHeightfieldCollisionComponent* Comp = *It;
						if (Comp)
						{
							Comp->UpdateAddCollisions(TRUE);
						}
					}
					Proxy->bResetup = FALSE;
				}
			}

			// Fixed up for Landscape fix match case
			for (TMap<FGuid, ULandscapeInfo*>::TIterator It(GWorld->GetWorldInfo()->LandscapeInfoMap); It; ++It)
			{
				ULandscapeInfo* LandscapeInfo = It.Value();
				if (LandscapeInfo)
				{
					TSet<ALandscapeProxy*> SelectProxies;
					for (TMap<QWORD, ULandscapeComponent*>::TIterator It(LandscapeInfo->XYtoComponentMap) ; It; ++It )
					{
						ULandscapeComponent* Comp = It.Value();
						ULandscapeHeightfieldCollisionComponent* Collision = LandscapeInfo->XYtoCollisionComponentMap.FindRef(It.Key());
						if (Comp && Collision)
						{
							if (Comp->GetLandscapeProxy()->GetLevel() != Collision->GetLandscapeProxy()->GetLevel())
							{
								ALandscapeProxy* FromProxy = Collision->GetLandscapeProxy();
								ALandscapeProxy* DestProxy = Comp->GetLandscapeProxy();
								// From MoveToLevelTool
								FromProxy->CollisionComponents.RemoveItem(Collision);
								Collision->Rename(NULL, DestProxy);
								DestProxy->CollisionComponents.AddItem(Collision);
								Collision->ConditionalDetach();
								SelectProxies.Add(FromProxy);
								SelectProxies.Add(DestProxy);
							}
						}
					}

					for(TSet<ALandscapeProxy*>::TIterator It(SelectProxies);It;++It)
					{
						(*It)->ConditionalUpdateComponents();
						(*It)->MarkPackageDirty();
						(*It)->InitRBPhysEditor();
					}
				}
			}
			
			// Update add collision
			for (TMap<FGuid, ULandscapeInfo*>::TIterator It(GWorld->GetWorldInfo()->LandscapeInfoMap); It; ++It)
			{
				ULandscapeInfo* LandscapeInfo = It.Value();
				if (LandscapeInfo)
				{
					LandscapeInfo->UpdateAllAddCollisions();
				}
			}

			if( GCallbackEvent )
			{
				// For Landscape List Update
				GCallbackEvent->Send( CALLBACK_WorldChange );
			}

			// Warning after setup
			for (TArray<FString>::TIterator It(ErrorLandscapeNames); It; ++It )
			{
				bShouldShowMapCheckWindow = TRUE;
				GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT( "MapCheck_Message_LandscapeNoLandscapeInfo_Warning" )), *(*It) ) ), TEXT( "LandscapeNoLandscapeInfo_Warning" ), MCGROUP_DEFAULT );
			}

			if (bShouldShowMapCheckWindow)
			{
				GWarn->MapCheck_Show();
			}
		}
		return TRUE;
	}
	else if (ParseCommand(&Str, TEXT("UpdateLandscapeSetup")))
	{
		if (GWorld)
		{
			for (FActorIterator ActorIt; ActorIt; ++ActorIt)
			{
				ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*ActorIt);
				if (Proxy)
				{
					for (TArray<ULandscapeComponent*>::TIterator It(Proxy->LandscapeComponents); It; ++It )
					{
						ULandscapeComponent* Comp = *It;
						if( Comp )
						{
							Comp->SetupActor(TRUE);
						}
					}

					for (TArray<ULandscapeHeightfieldCollisionComponent*>::TIterator It(Proxy->CollisionComponents); It; ++It )
					{
						ULandscapeHeightfieldCollisionComponent* Comp = *It;
						if( Comp )
						{
							Comp->SetupActor(TRUE);
						}
					}
				}
			}
		}
	}
#endif // WITH_EDITOR
	else if (ParseCommand(&Str, TEXT("UpdateAddLandscapeComponents")))
	{
		if (GWorld && GWorld->GetWorldInfo())
		{
			for (TMap<FGuid, ULandscapeInfo*>::TIterator It(GWorld->GetWorldInfo()->LandscapeInfoMap); It; ++It)
			{
				ULandscapeInfo* LandscapeInfo = It.Value();
				if (LandscapeInfo)
				{
					LandscapeInfo->UpdateAllAddCollisions();
				}
			}
		}
	}
	else if( ParseCommand(&Str,TEXT("ProcBuildingUpdate")) )
	{
		AProcBuilding* Building = FindObject<AProcBuilding>(NULL, Str);

		// Make sure this building is valid and hasn't been queued for deletion
		if( Building != NULL && !Building->bDeleteMe )
		{
			GApp->CB_ProcBuildingUpdate(Building);
		}
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("FIXBUILDINGLODS")) )
	{
		AProcBuilding* Building = FindObject<AProcBuilding>(NULL, Str);
		GApp->CB_FixProcBuildingLODs();
		return TRUE;
	}
	else if ( ParseCommand(&Str, TEXT("GROUPS")) )
	{
		if( Exec_Group( Str, Ar ) )
		{
			return TRUE;
		}
	}

	return FALSE;
}

/** @return Returns whether or not the user is able to autosave. **/
UBOOL UUnrealEdEngine::CanAutosave() const
{
	return ( GEditor->GetUserSettings().bAutoSaveEnable && !GIsSlowTask && !GEditorModeTools().IsModeActive( EM_InterpEdit ) && !PlayWorld );
}

/** @return Returns whether or not autosave is going to occur within the next minute. */
UBOOL UUnrealEdEngine::AutoSaveSoon() const
{
	UBOOL bResult = FALSE;
	if(CanAutosave())
	{
		FLOAT TimeTillAutoSave = (FLOAT)GEditor->GetUserSettings().AutoSaveTimeMinutes - AutosaveCount/60.0f;
		bResult = TimeTillAutoSave < 1.0f && TimeTillAutoSave > 0.0f;
	}

	return bResult;
}

/** @return Returns the amount of time until the next autosave in seconds. */
INT UUnrealEdEngine::GetTimeTillAutosave() const
{
	INT Result = -1;

	if(CanAutosave())
	{
		Result = appTrunc(60*(GEditor->GetUserSettings().AutoSaveTimeMinutes - AutosaveCount/60.0f));
	}

	return Result;
}



/**
 * Checks to see if any worlds are dirty (that is, they need to be saved.)
 *
 * @return TRUE if any worlds are dirty
 */
UBOOL UUnrealEdEngine::AnyWorldsAreDirty() const
{
	// Get the set of all reference worlds.
	TArray<UWorld*> WorldsArray;
	FLevelUtils::GetWorlds( WorldsArray, TRUE );

	if ( WorldsArray.Num() > 0 )
	{
		FString FinalFilename;
		for ( INT WorldIndex = 0 ; WorldIndex < WorldsArray.Num() ; ++WorldIndex )
		{
			UWorld* World = WorldsArray( WorldIndex );
			UPackage* Package = Cast<UPackage>( World->GetOuter() );
			check( Package );

			// If this world needs saving . . .
			if ( Package->IsDirty() )
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/**
* Returns true if the user is currently interacting with a viewport.
*/
UBOOL UUnrealEdEngine::IsUserInteracting()
{
	// Check to see if the user is in the middle of a drag operation.
	UBOOL bUserIsInteracting = FALSE;
	for( INT ClientIndex = 0 ; ClientIndex < ViewportClients.Num() ; ++ClientIndex )
	{
		// Check for tracking and capture.  If a viewport has mouse capture, it could be locking the mouse to the viewport, which means if we prompt with a dialog
		// while the mouse is locked to a viewport, we wont be able to interact with the dialog.  
		if ( ViewportClients(ClientIndex)->bIsTracking ||  ViewportClients(ClientIndex)->Viewport->HasMouseCapture() )
		{
			bUserIsInteracting = TRUE;
			break;
		}
	}

	if( !bUserIsInteracting )
	{
		// When a property window is open and the user is dragging to modify a property with a spinbox control, 
		// the viewport clients will have bIsTracking to FALSE. 
		// We check for the state of the right and left mouse buttons and assume the user is interacting with something if a mouse button is pressed down
		UBOOL bLeftDown = GetAsyncKeyState(VK_LBUTTON) & 0x8000;
		UBOOL bRightDown = GetAsyncKeyState(VK_RBUTTON) & 0x8000;
		bUserIsInteracting = bLeftDown || bRightDown;
	}

	return bUserIsInteracting;
}

/** Attempts to autosave the level and/or content packages, if those features are enabled. */
void UUnrealEdEngine::AttemptAutosave()
{
	// Don't autosave if disabled or if it is not yet time to autosave.
	const UEditorUserSettings& EditorUserSettings = GEditor->GetUserSettings();
	const UBOOL bTimeToAutosave = ( EditorUserSettings.bAutoSaveEnable && ( AutosaveCount/60.0f >= static_cast<FLOAT>( EditorUserSettings.AutoSaveTimeMinutes ) ) );
	if ( bTimeToAutosave )
	{
		// Don't autosave during interpolation editing, if there's another slow task
		// already in progress, or while a PIE world is playing.
		const UBOOL bCanAutosave = CanAutosave();

		if( bCanAutosave )
		{
			// Don't interrupt the user with an autosave.
			if ( !IsUserInteracting() )
			{
				bIsAutoSaving = TRUE;
				GApp->AutosaveState = WxUnrealEdApp::AUTOSAVE_Saving;

				SaveConfig();

				// Make sure the autosave directory exists before attempting to write the file.
				const FString AbsoluteAutoSaveDir( FString(appBaseDir()) * AutoSaveDir );
				GFileManager->MakeDirectory( *AbsoluteAutoSaveDir, 1 );

				// Autosave maps and/or content packages based on user settings.
				const INT NewAutoSaveIndex = (AutoSaveIndex+1)%10;
				
				UBOOL bLevelSaved = FALSE;
				UBOOL bAssetsSaved = FALSE;

				if ( EditorUserSettings.bAutoSaveMaps )
				{
					bLevelSaved = FEditorFileUtils::AutosaveMap( AbsoluteAutoSaveDir, NewAutoSaveIndex );
				}
				if ( EditorUserSettings.bAutoSaveContent && GApp->AutosaveState != WxUnrealEdApp::AUTOSAVE_Cancelled)
				{
					bAssetsSaved = FEditorFileUtils::AutosaveContentPackages( AbsoluteAutoSaveDir, NewAutoSaveIndex, &EditorUserSettings.PackagesToSave );
				}

				if ( bLevelSaved || bAssetsSaved )
				{
					// If a level was actually saved, update the autosave index.
					AutoSaveIndex = NewAutoSaveIndex;
				}

				ResetAutosaveTimer();

				if(GApp->AutosaveState == WxUnrealEdApp::AUTOSAVE_Cancelled)
				{
					warnf(TEXT("Autosave was cancelled."));
				}

				bIsAutoSaving = FALSE;
				GApp->AutosaveState = WxUnrealEdApp::AUTOSAVE_Inactive;
			}
		}
	}
}

/**
 * Attempts to prompt the user to checkout modified packages from source control.  
 * Will defer prompting the user if they are interacting with something
 */
void UUnrealEdEngine::AttemptModifiedPackageNotification()
{
#if HAVE_SCC
	if( bNeedToPromptForCheckout )
	{
		// Defer prompting for checkout if we cant prompt because of the following:
		// The user is interacting with something,
		// We are performing a slow task
		// We have a play world
		// The user disabled prompting on package modification
		// A window has capture on the mouse
		UBOOL bCanPrompt = !IsUserInteracting() && !GIsSlowTask && !PlayWorld && GEditor->GetUserSettings().bPromptForCheckoutOnPackageModification && GetCapture() == NULL;
		
		if( bCanPrompt )
		{
			// The user is not interacting with anything, prompt to checkout packages that have been modified
	
			FString PackageNames;
			for( TMap<UPackage*,BYTE>::TIterator It(PackageToNotifyState); It; ++It )
			{
				// Only notify about packages in the map that are pending.  Ones we already prompted about should be ignored
				if( It.Value() == NS_PendingPrompt )
				{
					PackageNames += It.Key()->GetName() + "\n";
					// Set the state of the package to prompted by balloon.
					// This will not show up in new balloon prompts but will show up in any modal checkout dialog
					It.Value() = NS_BalloonPrompted;
				}
			}
			
			// Show the balloon
			FShowBalloonNotification::ShowNotification( TEXT("Packages Need Checkout"), PackageNames, ID_CHECKOUT_BALLOON_ID );
			// No longer have a pending prompt.
			bNeedToPromptForCheckout = FALSE;
		}
	}
#endif
}

/** 
 * Alerts the user to any packages that have been modified which have been previously saved with an engine version newer than
 * the current version. These packages cannot be saved, so the user should be alerted ASAP.
 */
void UUnrealEdEngine::AttemptWarnAboutPackageEngineVersions()
{
	if ( bNeedWarningForPkgEngineVer )
	{
		const UBOOL bCanPrompt = !IsUserInteracting() && !GIsSlowTask && !PlayWorld && GetCapture() == NULL;
		if ( bCanPrompt )
		{
			FString PackageNames;
			for ( TMap<FString, BYTE>::TIterator MapIter( PackagesCheckedForEngineVersion ); MapIter; ++MapIter )
			{
				if ( MapIter.Value() == VWS_PendingWarn )
				{
					PackageNames += FString::Printf( TEXT("%s\n"), *MapIter.Key() );
					MapIter.Value() = VWS_Warned;
				}
			}
			appMsgf( AMT_OK, LocalizeSecure( LocalizeError( TEXT("PackagesSavedWithNewerVersion"), TEXT("Core") ), *PackageNames ) );
			bNeedWarningForPkgEngineVer = FALSE;
		}
	}
}

/** 
 * Prompts the user with a modal checkout dialog to checkout packages from source control.   
 * This should only be called by the auto prompt to checkout package notification system.
 * For a general checkout packages routine use FEditorFileUtils::PromptToCheckoutPackages
 *
 * @param bPromptAll	If true we prompt for all packages in the PackageToNotifyState map.  If false only prompt about ones we have never prompted about before
 */
void UUnrealEdEngine::PromptToCheckoutModifiedPackages( UBOOL bPromptAll )
{
#if HAVE_SCC
	TArray<UPackage*> PackagesToCheckout;
	if( bPromptAll )
	{
		PackageToNotifyState.GenerateKeyArray( PackagesToCheckout );
	}
	else
	{
		for( TMap<UPackage*,BYTE>::TIterator It(PackageToNotifyState); It; ++It )
		{
			if( It.Value() == NS_BalloonPrompted || It.Value() == NS_PendingPrompt )
			{
				PackagesToCheckout.AddItem( It.Key() );
				It.Value() = NS_DialogPrompted;
			}
		}
	}

	FEditorFileUtils::PromptToCheckoutPackages( TRUE, PackagesToCheckout, NULL, TRUE );
#endif
}

/**
 * Checks to see if there are any packages in the PackageToNotifyState map that are not checked out by the user
 *
 * @return True if packages need to be checked out.
 */
UBOOL UUnrealEdEngine::DoDirtyPackagesNeedCheckout() const
{
	UBOOL bPackagesNeedCheckout = FALSE;
#if HAVE_SCC
	if( FSourceControl::IsEnabled() )
	{
		for( TMap<UPackage*,BYTE>::TConstIterator It(PackageToNotifyState); It; ++It )
		{
			const UPackage* Package = It.Key();
			const INT SCCState = GPackageFileCache->GetSourceControlState( *Package->GetName() );
			if( SCCState == SCC_ReadOnly || SCCState == SCC_NotCurrent || SCCState == SCC_CheckedOutOther )
			{
				bPackagesNeedCheckout = TRUE;
				break;
			}
		}
	}
#endif
	return bPackagesNeedCheckout;
}

/**
 * Called when a map is about to be unloaded so map packages can be removed from the package checkout data.
 */
void UUnrealEdEngine::PurgeOldPackageCheckoutData()
{
	// Remove all map packages from the package checkout data.
	// Note: Don't have to do this for content packages as they cant be unloaded if they are modified.
	for( TMap<UPackage*,BYTE>::TIterator It(PackageToNotifyState); It; ++It )
	{
		if( It.Key()->PackageFlags & PKG_ContainsMap )
		{
			It.RemoveCurrent();
		}
	}
}

UBOOL UUnrealEdEngine::Exec_Edit( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("CUT")) )
	{
		TArray<FEdMode*> ActiveModes; 
		GEditorModeTools().GetActiveModes( ActiveModes );
		for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
		{
			if (ActiveModes(ModeIndex)->ProcessEditCut())
			{
				return TRUE;
			}
		}
		CopySelectedActorsToClipboard( TRUE, TRUE );
	}
	else if( ParseCommand(&Str,TEXT("COPY")) )
	{
		TArray<FEdMode*> ActiveModes; 
		GEditorModeTools().GetActiveModes( ActiveModes );
		for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
		{
			if (ActiveModes(ModeIndex)->ProcessEditCopy())
			{
				return TRUE;
			}
		}
		CopySelectedActorsToClipboard( FALSE, TRUE );
	}
	else if( ParseCommand(&Str,TEXT("PASTE")) )
	{
		TArray<FEdMode*> ActiveModes; 
		GEditorModeTools().GetActiveModes( ActiveModes );
		for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
		{
			if (ActiveModes(ModeIndex)->ProcessEditPaste())
			{
				return TRUE;
			}
		}

		const FVector SaveClickLocation = GEditor->ClickLocation;

		enum EPasteTo
		{
			PT_OriginalLocation	= 0,
			PT_Here				= 1,
			PT_WorldOrigin		= 2
		} PasteTo = PT_OriginalLocation;

		FString TransName = *LocalizeUnrealEd("Paste");
		if( Parse( Str, TEXT("TO="), TempStr, 15 ) )
		{
			if( !appStrcmp( TempStr, TEXT("HERE") ) )
			{
				PasteTo = PT_Here;
				TransName = *LocalizeUnrealEd("PasteHere");
			}
			else
			{
				if( !appStrcmp( TempStr, TEXT("ORIGIN") ) )
				{
					PasteTo = PT_WorldOrigin;
					TransName = *LocalizeUnrealEd("PasteToWorldOrigin");
				}
			}
		}


		FVector AnyActorLocation = FVector::ZeroVector;
		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			AnyActorLocation = Actor->Location;
			break;
		}

		// Check to see if the user wants to place the actor into a streaming grid network
		ULevel* DesiredLevel = EditorLevelUtils::GetLevelForPlacingNewActor( AnyActorLocation );

		// Don't allow pasting to levels that are locked
		if( !FLevelUtils::IsLevelLocked( DesiredLevel ) )
		{
			// Make sure the desired level is current
			ULevel* OldCurrentLevel = GWorld->CurrentLevel;
			GWorld->CurrentLevel = DesiredLevel;

			const FScopedTransaction Transaction( *TransName );

			GEditor->SelectNone( TRUE, FALSE );
			edactPasteSelected( FALSE, FALSE, TRUE );

			if( PasteTo != PT_OriginalLocation )
			{
				// Get a bounding box for all the selected actors locations.
				FBox bbox(0);
				INT NumActorsToMove = 0;

				for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
				{
					AActor* Actor = static_cast<AActor*>( *It );
					checkSlow( Actor->IsA(AActor::StaticClass()) );

					bbox += Actor->Location;
					++NumActorsToMove;
				}

				if ( NumActorsToMove > 0 )
				{
					// Figure out which location to center the actors around.
					const FVector Origin( PasteTo == PT_Here ? SaveClickLocation : FVector(0,0,0) );

					// Compute how far the actors have to move.
					const FVector Location = bbox.GetCenter();
					const FVector Adjust = Origin - Location;

					// Move the actors.
					AActor* SingleActor = NULL;
					for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
					{
						AActor* Actor = static_cast<AActor*>( *It );
						checkSlow( Actor->IsA(AActor::StaticClass()) );

						SingleActor = Actor;
						Actor->Location += Adjust;
						Actor->PostEditMove(TRUE);
						Actor->ForceUpdateComponents();
					}

					// Update the pivot location.
					check(SingleActor);
					SetPivot( SingleActor->Location, FALSE, TRUE );
				}
			}

			GWorld->CurrentLevel = OldCurrentLevel;

			RedrawLevelEditingViewports();

			// If required, update the Bsp of any levels that received a pasted brush actor
			RebuildAlteredBSP();
		}
		else
		{
			appMsgf(AMT_OK, TEXT("PasteActor: %s"), *LocalizeUnrealEd(TEXT("Error_OperationDisallowedOnLockedLevel")));
			return NULL;
		}
	}

	return FALSE;
}

UBOOL UUnrealEdEngine::Exec_Pivot( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("HERE")) )
	{
		NoteActorMovement();
		SetPivot( ClickLocation, FALSE, FALSE );
		FinishAllSnaps();
		RedrawLevelEditingViewports();
	}
	else if( ParseCommand(&Str,TEXT("SNAPPED")) )
	{
		NoteActorMovement();
		SetPivot( ClickLocation, TRUE, FALSE );
		FinishAllSnaps();
		RedrawLevelEditingViewports();
	}
	else if( ParseCommand(&Str,TEXT("CENTERSELECTION")) )
	{
		NoteActorMovement();

		// Figure out the center location of all selections

		INT Count = 0;
		FVector Center(0,0,0);

		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			Center += Actor->Location;
			Count++;
		}

		if( Count > 0 )
		{
			ClickLocation = Center / Count;

			SetPivot( ClickLocation, FALSE, FALSE );
			FinishAllSnaps();
		}

		RedrawLevelEditingViewports();
	}

	return FALSE;
}

static void MirrorActors(const FVector& MirrorScale)
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd("MirroringActors") );

	// Fires CALLBACK_LevelDirtied when falling out of scope.
	FScopedLevelDirtied		LevelDirtyCallback;

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		const FVector PivotLocation = GEditorModeTools().PivotLocation;

		if( Actor->IsABrush() )
		{
			ABrush* Brush = (ABrush*)Actor;
			Brush->Brush->Modify();

			const FVector LocalToWorldOffset = ( Brush->Location - PivotLocation );
			const FVector LocationOffset = ( LocalToWorldOffset * MirrorScale ) - LocalToWorldOffset;

			Brush->Location += LocationOffset;
			Brush->PrePivot *= MirrorScale;

			for( INT poly = 0 ; poly < Brush->Brush->Polys->Element.Num() ; poly++ )
			{
				FPoly* Poly = &(Brush->Brush->Polys->Element(poly));

				Poly->TextureU *= MirrorScale;
				Poly->TextureV *= MirrorScale;

				Poly->Base += LocalToWorldOffset;
				Poly->Base *= MirrorScale;
				Poly->Base -= LocalToWorldOffset;
				Poly->Base -= LocationOffset;

				for( INT vtx = 0 ; vtx < Poly->Vertices.Num(); vtx++ )
				{
					Poly->Vertices(vtx) += LocalToWorldOffset;
					Poly->Vertices(vtx) *= MirrorScale;
					Poly->Vertices(vtx) -= LocalToWorldOffset;
					Poly->Vertices(vtx) -= LocationOffset;
				}

				Poly->Reverse();
				Poly->CalcNormal();
			}

			Brush->ClearComponents();
		}
		else
		{
			Actor->Modify();
			Actor->EditorApplyMirror( MirrorScale, PivotLocation );
		}

		Actor->InvalidateLightingCache();
		Actor->PostEditMove( TRUE );

		Actor->MarkPackageDirty();
		LevelDirtyCallback.Request();
	}

	if ( GEditorModeTools().IsModeActive( EM_Geometry ) )
	{
		// If we are in geometry mode, make sure to update the mode with new source data for selected brushes
		FEdModeGeometry* Mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode( EM_Geometry );
		Mode->GetFromSource();
	}

	GEditor->RedrawLevelEditingViewports();
}

/**
* Gathers up a list of selection FPolys from selected static meshes.
*
* @return	A TArray containing FPolys representing the triangles in the selected static meshes (note that these
*           triangles are transformed into world space before being added to the array.
*/

TArray<FPoly*> GetSelectedPolygons()
{
	// Build a list of polygons from all selected static meshes

	TArray<FPoly*> SelectedPolys;

	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		for(INT j=0; j<Actor->AllComponents.Num(); j++)
		{
			// If its a static mesh component, with a static mesh
			UActorComponent* Comp = Actor->AllComponents(j);
			UStaticMeshComponent* SMComp = Cast<UStaticMeshComponent>(Comp);
			if(SMComp && SMComp->StaticMesh)
			{
				UStaticMesh* StaticMesh = SMComp->StaticMesh;
				const FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*) StaticMesh->LODModels(0).RawTriangles.Lock(LOCK_READ_ONLY);

				if( StaticMesh->LODModels(0).RawTriangles.GetElementCount() )
				{
					for( INT TriangleIndex = 0 ; TriangleIndex < StaticMesh->LODModels(0).RawTriangles.GetElementCount() ; TriangleIndex++ )
					{
						const FStaticMeshTriangle& Triangle = RawTriangleData[TriangleIndex];
						FPoly* Polygon = new FPoly;

						// Add the poly

						Polygon->Init();
						Polygon->PolyFlags = PF_DefaultFlags;

						new(Polygon->Vertices) FVector(Actor->LocalToWorld().TransformFVector( Triangle.Vertices[2] ) );
						new(Polygon->Vertices) FVector(Actor->LocalToWorld().TransformFVector( Triangle.Vertices[1] ) );
						new(Polygon->Vertices) FVector(Actor->LocalToWorld().TransformFVector( Triangle.Vertices[0] ) );

						Polygon->CalcNormal(1);
						Polygon->Fix();
						if( Polygon->Vertices.Num() > 2 )
						{
							if( !Polygon->Finalize( NULL, 1 ) )
							{
								SelectedPolys.AddItem( Polygon );
							}
						}

						// And add a flipped version of it to account for negative scaling

						Polygon = new FPoly;

						Polygon->Init();
						Polygon->PolyFlags = PF_DefaultFlags;

						new(Polygon->Vertices) FVector(Actor->LocalToWorld().TransformFVector( Triangle.Vertices[2] ) );
						new(Polygon->Vertices) FVector(Actor->LocalToWorld().TransformFVector( Triangle.Vertices[0] ) );
						new(Polygon->Vertices) FVector(Actor->LocalToWorld().TransformFVector( Triangle.Vertices[1] ) );

						Polygon->CalcNormal(1);
						Polygon->Fix();
						if( Polygon->Vertices.Num() > 2 )
						{
							if( !Polygon->Finalize( NULL, 1 ) )
							{
								SelectedPolys.AddItem( Polygon );
							}
						}
					}
				}
				StaticMesh->LODModels(0).RawTriangles.Unlock();
			}
		}
	}

	return SelectedPolys;
}

/**
* Creates an axis aligned bounding box based on the bounds of SelectedPolys.  This bounding box
* is then copied into the builder brush.  This function is a set up function that the blocking volume
* creation execs will call before doing anything fancy.
*
* @param	SelectedPolys	The list of selected FPolys to create the bounding box from.
*/

void CreateBoundingBoxBuilderBrush( const TArray<FPoly*> SelectedPolys, UBOOL bSnapVertsToGrid )
{
	int x;
	FPoly* poly;
	FBox bbox(0);
	FVector Vertex;

	for( x = 0 ; x < SelectedPolys.Num() ; ++x )
	{
		poly = SelectedPolys(x);

		for( int v = 0 ; v < poly->Vertices.Num() ; ++v )
		{
			if( bSnapVertsToGrid )
			{
				Vertex = poly->Vertices(v).GridSnap(GEditor->Constraints.GetGridSize());
			}
			else
			{
				Vertex = poly->Vertices(v);
			}

			bbox += Vertex;
		}
	}

	// Change the builder brush to match the bounding box so that it exactly envelops the selected meshes

	FVector extent = bbox.GetExtent();
	UCubeBuilder* CubeBuilder = ConstructObject<UCubeBuilder>( UCubeBuilder::StaticClass() );
	CubeBuilder->X = extent.X * 2;
	CubeBuilder->Y = extent.Y * 2;
	CubeBuilder->Z = extent.Z * 2;
	CubeBuilder->eventBuild();

	GWorld->GetBrush()->Location = bbox.GetCenter();

	GWorld->GetBrush()->ClearComponents();
	GWorld->GetBrush()->ConditionalUpdateComponents();
}

/**
* Take a plane and creates a gigantic triangle polygon that lies along it.  The blocking
* volume creation routines call this when they are cutting geometry and need to create
* capping polygons.
*
* This polygon is so huge that it doesn't matter where the vertices actually land.
*
* @param	InPlane		The plane to lay the polygon on
* @return	An FPoly representing the giant triangle we created (NULL if there was a problem)
*/

FPoly* CreateHugeTrianglePolygonOnPlane( const FPlane* InPlane )
{
	// Using the plane normal, get 2 good axis vectors

	FVector A, B;
	InPlane->SafeNormal().FindBestAxisVectors( A, B );

	// Create 4 vertices from the plane origin and the 2 axis generated above

	FPoly* Triangle = new FPoly();

	FVector Center = FVector( InPlane->X, InPlane->Y, InPlane->Z ) * InPlane->W;
	FVector V0 = Center + (A * WORLD_MAX);
	FVector V1 = Center + (B * WORLD_MAX);
	FVector V2 = Center - (((A + B) / 2.0f) * WORLD_MAX);

	// Create a triangle that lays on InPlane

	Triangle->Init();
	Triangle->PolyFlags = PF_DefaultFlags;

	new(Triangle->Vertices) FVector( V0 );
	new(Triangle->Vertices) FVector( V2 );
	new(Triangle->Vertices) FVector( V1 );

	Triangle->CalcNormal(1);
	Triangle->Fix();
	if( Triangle->Finalize( NULL, 1 ) )
	{
		delete Triangle;
		Triangle = NULL;
	}

	return Triangle;
}

/**
* Utility function to quickly set the collision type on selected static meshes.
*
* @param	InCollisionType		The collision type to use (COLLIDE_??)
*/

void SetCollisionTypeOnSelectedActors( BYTE InCollisionType )
{
	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );
		AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor);

		if( StaticMeshActor )
		{
			StaticMeshActor->Modify();
			StaticMeshActor->CollisionType = InCollisionType;
			StaticMeshActor->SetCollisionFromCollisionType();
		}
	}
}

UBOOL UUnrealEdEngine::Exec_SkeletalMesh( const TCHAR* Str, FOutputDevice& Ar )
{
	//This command sets the offset and orientation for all skeletal meshes within the set of currently selected packages
	if(ParseCommand(&Str, TEXT("CHARBITS"))) //SKELETALMESH CHARBITS
	{
		FVector Offset = FVector(0.0f, 0.0f, 0.0f);
		FRotator Orientation = FRotator(0, 0, 0);
		UBOOL bHasOffset = GetFVECTOR(Str, TEXT("OFFSET="), Offset);
		
		TCHAR Temp[80];
		UBOOL bHasOrientation = GetSUBSTRING(Str, TEXT("ORIENTATION="), Temp, 80);
		
		//If orientation is present do custom parsing to allow for a proper conversion from a floating point representation of degress
		//to its integer representation in FRotator. GetFROTATOR() does not allow us to do this.
		if(bHasOrientation)
		{
			FLOAT Value = 0.0f;
			
			if(Parse(Temp, TEXT("YAW="), Value))
			{
				Value = appFmod(Value, 360.0f); //Make sure it's in the range 0-360
				Orientation.Yaw = (INT)(Value / 360.0f * 65536); //Convert the angle to int craziness
			}

			if(Parse(Temp, TEXT("PITCH="), Value))
			{
				Value = appFmod(Value, 360.0f); //Make sure it's in the range 0-360
				Orientation.Pitch = (INT)(Value / 360.0f * 65536); //Convert the angle to int craziness
			}

			if(Parse(Temp, TEXT("ROLL="), Value))
			{
				Value = appFmod(Value, 360.0f); //Make sure it's in the range 0-360
				Orientation.Roll = (INT)(Value / 360.0f * 65536); //Convert the angle to int craziness
			}
		}

		return TRUE;
	}

	return FALSE;
}

UBOOL UUnrealEdEngine::Exec_Actor( const TCHAR* Str, FOutputDevice& Ar )
{
	// Keep a pointer to the beginning of the string to use for message displaying purposes
	const TCHAR* const FullStr = Str;

	if( ParseCommand(&Str,TEXT("ADD")) )
	{
		UClass* Class;
		if( ParseObject<UClass>( Str, TEXT("CLASS="), Class, ANY_PACKAGE ) )
		{
			AActor* Default   = Class->GetDefaultActor();
			FVector Collision = Default->GetCylinderExtent();
			INT bSnap = 1;
			Parse(Str,TEXT("SNAP="),bSnap);
			if( bSnap )
			{
				Constraints.Snap( ClickLocation, FVector(0, 0, 0) );
			}
			FVector Location = ClickLocation + ClickPlane * (FBoxPushOut(ClickPlane,Collision) + 0.1);
			if( bSnap )
			{
				Constraints.Snap( Location, FVector(0, 0, 0) );
			}

			// Determine if we clicked on the background.
			const FIntPoint& CurrentMousePos = GCurrentLevelEditingViewportClient->CurrentMousePos;
			HHitProxy* HitProxy = GCurrentLevelEditingViewportClient->Viewport->GetHitProxy( CurrentMousePos.X, CurrentMousePos.Y );
			// If the hit proxy is NULL we clicked on the background
			UBOOL bClickedOnBackground = (HitProxy == NULL);

			AActor* NewActor = AddActor( Class, Location );

			if( NewActor && bClickedOnBackground && GCurrentLevelEditingViewportClient->ViewportType == LVT_Perspective )
			{
				// Only move the actor in front of the camera if we didn't click on something useful like bsp or another actor and if we are in the perspective view
				MoveActorInFrontOfCamera( *NewActor, GCurrentLevelEditingViewportClient->ViewLocation, GCurrentLevelEditingViewportClient->ViewRotation.Vector() );
			}

			RedrawLevelEditingViewports();
			return TRUE;
		}
	}
	else if( ParseCommand(&Str,TEXT("CREATE_BV_BOUNDINGBOX")) )
	{
		const FScopedTransaction Transaction( TEXT("Create Bounding Box Blocking Volume") );
		GWorld->GetBrush()->Modify();

		UBOOL bSnapToGrid=0;
		ParseUBOOL( Str, TEXT("SNAPTOGRID="), bSnapToGrid );

		// Create a bounding box for the selected static mesh triangles and set the builder brush to match it

		TArray<FPoly*> SelectedPolys = GetSelectedPolygons();
		CreateBoundingBoxBuilderBrush( SelectedPolys, bSnapToGrid );

		// Create the blocking volume

		GUnrealEd->Exec( TEXT("BRUSH ADDVOLUME CLASS=BlockingVolume") );

		// Set up collision on selected actors

		SetCollisionTypeOnSelectedActors( COLLIDE_BlockWeapons );

		// Clean up memory

		for( int x = 0 ; x < SelectedPolys.Num() ; ++x )
		{
			delete SelectedPolys(x);
		}

		SelectedPolys.Empty();

		// Finish up

		RedrawLevelEditingViewports();
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("CREATE_BV_CONVEXVOLUME")) )
	{
		const FScopedTransaction Transaction( TEXT("Create Convex Blocking Volume") );
		GWorld->GetBrush()->Modify();

		UBOOL bSnapToGrid=0;
		ParseUBOOL( Str, TEXT("SNAPTOGRID="), bSnapToGrid );

		// The rejection tolerance.  When figuring out which planes to cut the blocking volume cube with
		// the code will reject any planes that are less than "NormalTolerance" different in their normals.
		//
		// This cuts down on the number of planes that will be used for generating the cutting planes and,
		// as a side effect, eliminates duplicates.

		FLOAT NormalTolerance = 0.25f;
		Parse( Str, TEXT("NORMALTOLERANCE="), NormalTolerance );

		FVector NormalLimits( 1.0f, 1.0f, 1.0f );
		Parse( Str, TEXT("NLIMITX="), NormalLimits.X );
		Parse( Str, TEXT("NLIMITY="), NormalLimits.Y );
		Parse( Str, TEXT("NLIMITZ="), NormalLimits.Z );

		// Create a bounding box for the selected static mesh triangles and set the builder brush to match it

		TArray<FPoly*> SelectedPolys = GetSelectedPolygons();
		CreateBoundingBoxBuilderBrush( SelectedPolys, bSnapToGrid );

		// Get a list of the polygons that make up the builder brush

		FPoly* poly;
		TArray<FPoly>* BuilderBrushPolys = new TArray<FPoly>( GWorld->GetBrush()->Brush->Polys->Element );

		// Create a list of valid splitting planes

		TArray<FPlane*> SplitterPlanes;

		for( int p = 0 ; p < SelectedPolys.Num() ; ++p )
		{
			// Get a splitting plane from the first poly in our selection

			poly = SelectedPolys(p);
			FPlane* SplittingPlane = new FPlane( poly->Vertices(0), poly->Normal );

			// Make sure this poly doesn't clip any other polys in the selection.  If it does, we can't use it for generating the convex volume.

			UBOOL bUseThisSplitter = TRUE;

			for( int pp = 0 ; pp < SelectedPolys.Num() && bUseThisSplitter ; ++pp )
			{
				FPoly* ppoly = SelectedPolys(pp);

				if( p != pp && !(poly->Normal - ppoly->Normal).IsNearlyZero() )
				{
					int res = ppoly->SplitWithPlaneFast( *SplittingPlane, NULL, NULL );

					if( res == SP_Split || res == SP_Front )
					{
						// Whoops, this plane clips polygons (and/or sits between static meshes) in the selection so it can't be used
						bUseThisSplitter = FALSE;
					}
				}
			}

			// If this polygons plane doesn't clip the selection in any way, we can carve the builder brush with it. Save it.

			if( bUseThisSplitter )
			{
				// Move the plane into the same coordinate space as the builder brush

				*SplittingPlane = SplittingPlane->TransformBy( GWorld->GetBrush()->WorldToLocal() );

				// Before keeping this plane, make sure there aren't any existing planes that have a normal within the rejection tolerance.

				UBOOL bAddPlaneToList = TRUE;

				for( int x = 0 ; x < SplitterPlanes.Num() ; ++x )
				{
					FPlane* plane = SplitterPlanes(x);

					if( plane->SafeNormal().Equals( SplittingPlane->SafeNormal(), NormalTolerance ) )
					{
						bAddPlaneToList = FALSE;
						break;
					}
				}

				// As a final test, make sure that this planes normal falls within the normal limits that were defined

				if( Abs( SplittingPlane->SafeNormal().X ) > NormalLimits.X )
				{
					bAddPlaneToList = FALSE;
				}
				if( Abs( SplittingPlane->SafeNormal().Y ) > NormalLimits.Y )
				{
					bAddPlaneToList = FALSE;
				}
				if( Abs( SplittingPlane->SafeNormal().Z ) > NormalLimits.Z )
				{
					bAddPlaneToList = FALSE;
				}

				// If this plane passed every test - it's a keeper!

				if( bAddPlaneToList )
				{
					SplitterPlanes.AddItem( SplittingPlane );
				}
				else
				{
					delete SplittingPlane;
				}
			}
		}

		// The builder brush is a bounding box at this point that fully surrounds the selected static meshes.
		// Now we will carve away at it using the splitting planes we collected earlier.  When this process
		// is complete, we will have a convex volume inside of the builder brush that can then be used to add
		// a blocking volume.

		TArray<FPoly> NewBuilderBrushPolys;

		for( int sp = 0 ; sp < SplitterPlanes.Num() ; ++sp )
		{
			FPlane* plane = SplitterPlanes(sp);

			// Carve the builder brush with each splitting plane we collected.  We place the results into
			// NewBuilderBrushPolys since we don't want to overwrite the original array just yet.

			UBOOL bNeedCapPoly = TRUE;

			for( int bp = 0 ; bp < BuilderBrushPolys->Num() ; ++bp )
			{
				FPoly* poly = &(*BuilderBrushPolys)(bp);

				FPoly Front, Back;
				int res = poly->SplitWithPlane( FVector( plane->X, plane->Y, plane->Z ) * plane->W, plane->SafeNormal(), &Front, &Back, TRUE );
				switch( res )
				{
					// Ignore these results.  We don't want them.
					case SP_Coplanar:
					case SP_Front:
						break;

					// In the case of a split, keep the polygon on the back side of the plane.
					case SP_Split:
					{
						NewBuilderBrushPolys.AddItem( Back );
						bNeedCapPoly = TRUE;
					}
					break;

					// By default, just keep the polygon that we had.
					default:
					{
						NewBuilderBrushPolys.AddItem( (*BuilderBrushPolys)(bp) );
					}
					break;
				}
			}

			// NewBuilderBrushPolys contains the newly clipped polygons so copy those into
			// the real array of polygons.

			BuilderBrushPolys = new TArray<FPoly>( NewBuilderBrushPolys );
			NewBuilderBrushPolys.Empty();

			// If any splitting occured, we need to generate a cap polygon to cover the hole.

			if( bNeedCapPoly )
			{
				// Create a large triangle polygon that covers the newly formed hole in the builder brush.

				FPoly* CappingPoly = CreateHugeTrianglePolygonOnPlane( plane );

				if( CappingPoly )
				{
					// Now we do the clipping the other way around.  We are going to use the polygons in the builder brush to
					// create planes which will clip the huge triangle polygon we just created.  When this process is over,
					// we will be left with a new polygon that covers the newly formed hole in the builder brush.

					for( int bp = 0 ; bp < BuilderBrushPolys->Num() ; ++bp )
					{
						FPoly* poly = &((*BuilderBrushPolys)(bp));
						FPlane* plane = new FPlane( poly->Vertices(0), poly->Vertices(1), poly->Vertices(2) );

						FPoly Front, Back;
						int res = CappingPoly->SplitWithPlane( FVector( plane->X, plane->Y, plane->Z ) * plane->W, plane->SafeNormal(), &Front, &Back, TRUE );
						switch( res )
						{
							case SP_Split:
							{
								*CappingPoly = Back;
							}
							break;
						}
					}

					// Add that new polygon into the builder brush polys as a capping polygon.

					BuilderBrushPolys->AddItem( *CappingPoly );
				}
			}
		}

		// Create a new builder brush from the freshly clipped polygons.

		GWorld->GetBrush()->Brush->Polys->Element.Empty();

		for( int x = 0 ; x < BuilderBrushPolys->Num() ; ++x )
		{
			GWorld->GetBrush()->Brush->Polys->Element.AddItem( (*BuilderBrushPolys)(x) );
		}

		GWorld->GetBrush()->ClearComponents();
		GWorld->GetBrush()->ConditionalUpdateComponents();

		// Create the blocking volume

		GUnrealEd->Exec( TEXT("BRUSH ADDVOLUME CLASS=BlockingVolume") );

		// Set up collision on selected actors

		SetCollisionTypeOnSelectedActors( COLLIDE_BlockWeapons );

		// Clean up memory

		for( int x = 0 ; x < SelectedPolys.Num() ; ++x )
		{
			delete SelectedPolys(x);
		}

		SelectedPolys.Empty();

		for( int x = 0 ; x < SplitterPlanes.Num() ; ++x )
		{
			delete SplitterPlanes(x);
		}

		SplitterPlanes.Empty();

		delete BuilderBrushPolys;

		// Finish up

		RedrawLevelEditingViewports();
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("MIRROR")) )
	{
		FVector MirrorScale( 1, 1, 1 );
		GetFVECTOR( Str, MirrorScale );
		// We can't have zeroes in the vector
		if( !MirrorScale.X )		MirrorScale.X = 1;
		if( !MirrorScale.Y )		MirrorScale.Y = 1;
		if( !MirrorScale.Z )		MirrorScale.Z = 1;
		MirrorActors( MirrorScale );
		RebuildAlteredBSP(); // Update the Bsp of any levels containing a modified brush
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("HIDE")) )
	{
		if( ParseCommand(&Str,TEXT("SELECTED")) ) // ACTOR HIDE SELECTED
		{
			if ( ParseCommand(&Str,TEXT("STARTUP")) ) // ACTOR HIDE SELECTED STARTUP
			{
				const FScopedTransaction Transaction( *LocalizeUnrealEd("HideSelectedAtStartup") );
				edactHideSelectedStartup();
				return TRUE;
			}
			else
			{
				const FScopedTransaction Transaction( *LocalizeUnrealEd("HideSelected") );
				edactHideSelected();
				SelectNone( TRUE, TRUE );
				return TRUE;
			}
		}
		else if( ParseCommand(&Str,TEXT("UNSELECTED")) ) // ACTOR HIDE UNSELECTEED
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd("HideUnselected") );
			edactHideUnselected();
			SelectNone( TRUE, TRUE );
			return TRUE;
		}
	}
	else if( ParseCommand(&Str,TEXT("UNHIDE")) ) 
	{
		if ( ParseCommand(&Str,TEXT("ALL")) ) // ACTOR UNHIDE ALL
		{
			if ( ParseCommand(&Str,TEXT("STARTUP")) ) // ACTOR UNHIDE ALL STARTUP
			{
				const FScopedTransaction Transaction( *LocalizeUnrealEd("ShowAllAtStartup") );
				edactUnHideAllStartup();
				return TRUE;
			}
			else
			{
				const FScopedTransaction Transaction( *LocalizeUnrealEd("UnHideAll") );
				edactUnHideAll();
				return TRUE;
			}
		}
		else if( ParseCommand(&Str,TEXT("SELECTED")) )	// ACTOR UNHIDE SELECTED
		{
			if ( ParseCommand(&Str,TEXT("STARTUP")) ) // ACTOR UNHIDE SELECTED STARTUP
			{
				const FScopedTransaction Transaction( *LocalizeUnrealEd("ShowSelectedAtStartup") );
				edactUnHideSelectedStartup();
				return TRUE;
			}
		}
	}
	else if( ParseCommand(&Str, TEXT("APPLYTRANSFORM")) )
	{
		appMsgf( AMT_OK, LocalizeSecure( LocalizeUnrealEd("Error_TriedToExecDeprecatedCmd"), FullStr ) );
	}
	else if( ParseCommand(&Str, TEXT("REPLACE")) )
	{
		UClass* Class;
		if( ParseCommand(&Str, TEXT("BRUSH")) ) // ACTOR REPLACE BRUSH
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd("ReplaceSelectedBrushActors") );
			edactReplaceSelectedBrush();
			return TRUE;
		}
		else if( ParseObject<UClass>( Str, TEXT("CLASS="), Class, ANY_PACKAGE ) ) // ACTOR REPLACE CLASS=<class>
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd("ReplaceSelectedNonBrushActors") );
			edactReplaceSelectedNonBrushWithClass( Class );
			return TRUE;
		}
	}
	else if ( ParseCommand(&Str, TEXT("LINKSELECTED"))) // ACTOR LINKSELECTED
	{
		// modify selected actors so undo/redo works
		for(FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			AActor* Actor = static_cast<AActor*>(*It);
			Actor->Modify();
		}

		// loop through all selected actors and notify any that implement the linkselection interface
		for(FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			AActor* Actor = static_cast<AActor*>(*It);
			checkSlow(Actor->IsA(AActor::StaticClass()));

			IEditorLinkSelectionInterface* Interface = InterfaceCast<IEditorLinkSelectionInterface>(Actor);
			if(Interface != NULL)
			{
				Interface->LinkSelection(GEditor->GetSelectedActors());
			}
		}

		RedrawLevelEditingViewports();
	}
	else if ( ParseCommand(&Str, TEXT("UNLINKSELECTED"))) // ACTOR LINKSELECTED
	{
		// modify selected actors so undo/redo works
		for(FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			AActor* Actor = static_cast<AActor*>(*It);
			Actor->Modify();
		}

		// loop through all selected actors and notify any that implement the linkselection interface
		for(FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			AActor* Actor = static_cast<AActor*>(*It);
			checkSlow(Actor->IsA(AActor::StaticClass()));

			IEditorLinkSelectionInterface* Interface = InterfaceCast<IEditorLinkSelectionInterface>(Actor);
			if(Interface != NULL)
			{
				Interface->UnLinkSelection(GEditor->GetSelectedActors());
			}
		}

		RedrawLevelEditingViewports();
	}
	else if( ParseCommand(&Str, TEXT("ATTACH")) ) // ACTOR ATTACH
	{
		GUnrealEd->AttachSelectedActors();
	}
	else if ( ParseCommand(&Str, TEXT("ADDTOATTACHEDITOR")) ) // ACTOR ADD TO ATTACHMENT EDITOR
	{
		GUnrealEd->AddSelectedToAttachmentEditor();
	}
	//@todo locked levels - handle the rest of these....is this required, or can we assume that actors in locked levels can't be selected
	else if( ParseCommand(&Str,TEXT("SELECT")) )
	{
		if( ParseCommand(&Str,TEXT("NONE")) ) // ACTOR SELECT NONE
		{
			return Exec( TEXT("SELECT NONE") );
		}
		else if( ParseCommand(&Str,TEXT("ALL")) ) // ACTOR SELECT ALL
		{
			if(ParseCommand(&Str, TEXT("FROMOBJ"))) // ACTOR SELECT ALL FROMOBJ
			{		
				UBOOL bHasStaticMeshes = FALSE;
				UBOOL bHasSpeedTrees = FALSE;
				TArray<UClass*> ClassesToSelect;

				for(FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
				{
					AActor* Actor = static_cast<AActor*>(*It);
					checkSlow(Actor->IsA(AActor::StaticClass()));

					if( Actor->IsA(AStaticMeshActor::StaticClass()) || Actor->IsA(ADynamicSMActor::StaticClass()) || Actor->IsA(AFracturedStaticMeshActor::StaticClass()) )
					{
						bHasStaticMeshes = TRUE;
					}
					else if(Actor->IsA(ASpeedTreeActor::StaticClass()))
					{
						bHasSpeedTrees = TRUE;
					}
					else
					{
						ClassesToSelect.AddUniqueItem(Actor->GetClass());
					}
				}

				const FScopedTransaction Transaction(*LocalizeUnrealEd("SelectAll"));
				if(bHasStaticMeshes)
				{
					edactSelectMatchingStaticMesh(FALSE);
				}

				if(bHasSpeedTrees)
				{
					GUnrealEd->SelectMatchingSpeedTrees();
				}

				if(ClassesToSelect.Num() > 0)
				{
					for(int Index = 0; Index < ClassesToSelect.Num(); ++Index)
					{
						edactSelectOfClass(ClassesToSelect(Index));
					}
				}

				return TRUE;
			}
			else
			{
				const FScopedTransaction Transaction( *LocalizeUnrealEd("SelectAll") );
				edactSelectAll();
				return TRUE;
			}
		}
		else if( ParseCommand(&Str,TEXT("INSIDE") ) ) // ACTOR SELECT INSIDE
		{
			appMsgf( AMT_OK, LocalizeSecure( LocalizeUnrealEd("Error_TriedToExecDeprecatedCmd"), FullStr ) );
		}
		else if( ParseCommand(&Str,TEXT("INVERT") ) ) // ACTOR SELECT INVERT
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd("SelectInvert") );
			edactSelectInvert();
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("OFCLASS")) ) // ACTOR SELECT OFCLASS CLASS=<class>
		{
			UClass* Class;
			if( ParseObject<UClass>(Str,TEXT("CLASS="),Class,ANY_PACKAGE) )
			{
				const FScopedTransaction Transaction( *LocalizeUnrealEd("SelectOfClass") );
				edactSelectOfClass( Class );
			}
			else
			{
				Ar.Log( NAME_ExecWarning, TEXT("Missing class") );
			}
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("OFSUBCLASS")) ) // ACTOR SELECT OFSUBCLASS CLASS=<class>
		{
			UClass* Class;
			if( ParseObject<UClass>(Str,TEXT("CLASS="),Class,ANY_PACKAGE) )
			{
				const FScopedTransaction Transaction( *LocalizeUnrealEd("SelectSubclassOfClass") );
				edactSelectSubclassOf( Class );
			}
			else
			{
				Ar.Log( NAME_ExecWarning, TEXT("Missing class") );
			}
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("BASED")) ) // ACTOR SELECT BASED
		{
			edactSelectBased();
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("BYPROPERTY")) ) // ACTOR SELECT BYPROPERTY
		{
			GEditor->SelectByPropertyColoration();
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("DELETED")) ) // ACTOR SELECT DELETED
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd("SelectDeleted") );
			edactSelectDeleted();
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("MATCHINGSTATICMESH")) ) // ACTOR SELECT MATCHINGSTATICMESH
		{
			const UBOOL bAllClasses = ParseCommand( &Str, TEXT("ALLCLASSES") );
			const FScopedTransaction Transaction( *LocalizeUnrealEd("SelectMatchingStaticMesh") );
			edactSelectMatchingStaticMesh( bAllClasses );
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("MATCHINGSKELETALMESH")) ) // ACTOR SELECT MATCHINGSKELETALMESH
		{
			const UBOOL bAllClasses = ParseCommand( &Str, TEXT("ALLCLASSES") );
			const FScopedTransaction Transaction( *LocalizeUnrealEd("SelectMatchingSkeletalMesh") );
			edactSelectMatchingSkeletalMesh( bAllClasses );
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("MATCHINGMATERIAL")) )
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd("SelectAllWithMatchingMaterial") );
			edactSelectMatchingMaterial();
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("MATCHINGEMITTER")) )
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd("SelectMatchingEmitter") );
			edactSelectMatchingEmitter();
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("MATCHINGPROCBUILDINGRULESETS")) ) // ACTOR SELECT MATCHINGPROCBUILDINGRULESETS
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd("SelectMatchingProcBuildingsByRuleset") );
			edactSelectMatchingProcBuildingsByRuleset();
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("KISMETREF")) ) // ACTOR SELECT KISMETREF
		{
			const UBOOL bReferenced = ParseCommand( &Str, TEXT("1") );
			const UBOOL bCurrent = !ParseCommand( &Str, TEXT("ALL") );
			if ( bReferenced )
			{
				if ( bCurrent )
				{
					const FScopedTransaction Transaction( *LocalizeUnrealEd("SelectKismetReferencedActors") );
				}
				else
				{
					const FScopedTransaction Transaction( *LocalizeUnrealEd("SelectKismetReferencedActorsAll") );
				}
			}
			else
			{
				if ( bCurrent )
				{
					const FScopedTransaction Transaction( *LocalizeUnrealEd("SelectKismetUnreferencedActors") );
				}
				else
				{
					const FScopedTransaction Transaction( *LocalizeUnrealEd("SelectKismetUnreferencedActorsAll") );
				}
			}
			edactSelectKismetReferencedActors( bReferenced, bCurrent );
			return TRUE;
		}
		else if (ParseCommand(&Str, TEXT("RELEVANTLIGHTS")))	// ACTOR SELECT RELEVANTLIGHTS
		{
			debugf(TEXT("Select relevant lights!"));
			edactSelectRelevantLights(FALSE);
		}
		else if (ParseCommand(&Str, TEXT("RELEVANTDOMINANTLIGHTS")))
		{
			debugf(TEXT("Select relevant dominant lights!"));
			edactSelectRelevantLights(TRUE);
		}
		else
		{
			// Get actor name.
			FName ActorName(NAME_None);
			if ( Parse( Str, TEXT("NAME="), ActorName ) )
			{
				AActor* Actor = FindObject<AActor>( GWorld->CurrentLevel, *ActorName.ToString() );
				const FScopedTransaction Transaction( *LocalizeUnrealEd("SelectToggleSingleActor") );
				SelectActor( Actor, !(Actor && Actor->IsSelected()), FALSE, TRUE );
			}
			return TRUE;
		}
	}
	else if( ParseCommand(&Str,TEXT("DELETE")) )		// ACTOR SELECT DELETE
	{
		const FScopedTransaction Transaction( *LocalizeUnrealEd("DeleteActors") );
		edactDeleteSelected();
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("UPDATE")) )		// ACTOR SELECT UPDATE
	{
		UBOOL bLockedLevel = FALSE;
		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			if ( !Actor->IsTemplate() && FLevelUtils::IsLevelLocked(Actor) )
			{
				bLockedLevel = TRUE;
			}
			else
			{
				Actor->PreEditChange(NULL);
				Actor->PostEditChange();
			}
		}

		if ( bLockedLevel )
		{
			appMsgf(AMT_OK, TEXT("Update Actor: %s"), *LocalizeUnrealEd(TEXT("Error_OperationDisallowedOnLockedLevel")));
		}
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("SET")) )
	{
		// @todo DB: deprecate the ACTOR SET exec.
		RedrawLevelEditingViewports();
		GCallbackEvent->Send( CALLBACK_RefreshEditor_LevelBrowser );
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("BAKEPREPIVOT")) )
	{
		FScopedLevelDirtied		LevelDirtyCallback;
		FScopedActorPropertiesChange	ActorPropertiesChangeCallback;

		// Bakes the current pivot position into all selected actors as their PrePivot

		FEditorModeTools& EditorModeTools = GEditorModeTools();

		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			Actor->Modify();
			FVector Delta( EditorModeTools.PivotLocation - Actor->Location );

			if( Actor->IsBrush() )
			{
				Actor->Location += Delta;
				Actor->PrePivot += Delta;
			}
			else
			{
				// Account for actor rotation and scaling when applying the delta

				const FRotationMatrix Matrix = FRotationMatrix( Actor->Rotation );
				Delta = Matrix.InverseTransformFVector( Delta );

				Delta /= Actor->DrawScale3D;
				Delta /= Actor->DrawScale;

				// The pivot location becomes the new actor location and the prepivot is adjusted by the delta.

				Actor->Location = EditorModeTools.PivotLocation;
				Actor->PrePivot += Delta;
			}

			Actor->PostEditMove( TRUE );
			Actor->ForceUpdateComponents();
		}

		GUnrealEd->NoteSelectionChange();
	} 
	else if( ParseCommand(&Str,TEXT("UNBAKEPREPIVOT")) )
	{
		FScopedLevelDirtied		LevelDirtyCallback;
		FScopedActorPropertiesChange	ActorPropertiesChangeCallback;

		// Resets the PrePivot of the selected actors to 0,0,0 while leaving them in the same world location.

		FEditorModeTools& EditorModeTools = GEditorModeTools();

		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			Actor->Modify();
			FVector Delta = Actor->PrePivot;

			if( Actor->IsBrush() )
			{
				Actor->Location -= Delta;
				Actor->PrePivot = FVector(0,0,0);
			}
			else
			{
				// Account for actor rotation and scaling when applying the delta

				Delta *= Actor->DrawScale3D;
				Delta *= Actor->DrawScale;

				const FRotationMatrix Matrix = FRotationMatrix( Actor->Rotation );
				Delta = Matrix.TransformFVector( Delta );

				// The pivot location becomes the new actor location and the prepivot is adjusted by the delta.

				Actor->Location -= Delta;
				Actor->PrePivot = FVector(0,0,0);
			}

			Actor->PostEditMove( TRUE );
			Actor->ForceUpdateComponents();
		}

		GUnrealEd->NoteSelectionChange();
	}
	else if( ParseCommand(&Str,TEXT("RESET")) )
	{
		FScopedTransaction Transaction( *LocalizeUnrealEd("ResetActors") );

		UBOOL Location=0;
		UBOOL Pivot=0;
		UBOOL Rotation=0;
		UBOOL Scale=0;
		if( ParseCommand(&Str,TEXT("LOCATION")) )
		{
			Location=1;
			ResetPivot();
		}
		else if( ParseCommand(&Str, TEXT("PIVOT")) )
		{
			Pivot=1;
			ResetPivot();
		}
		else if( ParseCommand(&Str,TEXT("ROTATION")) )
		{
			Rotation=1;
		}
		else if( ParseCommand(&Str,TEXT("SCALE")) )
		{
			Scale=1;
		}
		else if( ParseCommand(&Str,TEXT("ALL")) )
		{
			Location=Rotation=Scale=1;
			ResetPivot();
		}

		// Fires CALLBACK_LevelDirtied when falling out of scope.
		FScopedLevelDirtied		LevelDirtyCallback;

		UBOOL bHadLockedLevels = FALSE;
		UBOOL bModifiedActors = FALSE;
		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			if ( !Actor->IsTemplate() && FLevelUtils::IsLevelLocked(Actor) )
			{
				bHadLockedLevels = TRUE;
			}
			else
			{
				bModifiedActors = TRUE;

				Actor->PreEditChange(NULL);
				Actor->Modify();

				if( Location ) 
				{
					Actor->Location  = FVector(0.f,0.f,0.f);
				}
				if( Location ) 
				{
					Actor->PrePivot  = FVector(0.f,0.f,0.f);
				}
				if( Pivot && Actor->IsABrush() )
				{
					ABrush* Brush = (ABrush*)(Actor);
					Brush->Location -= Brush->PrePivot;
					Brush->PrePivot = FVector(0.f,0.f,0.f);
					Brush->PostEditChange();
				}
				if( Scale ) 
				{
					Actor->DrawScale = 1.0f;
				}

				Actor->MarkPackageDirty();
				LevelDirtyCallback.Request();
			}
		}

		if ( bHadLockedLevels )
		{
			appMsgf(AMT_OK, TEXT("Reset Actor: %s"), *LocalizeUnrealEd(TEXT("Error_OperationDisallowedOnLockedLevel")));
		}

		if ( bModifiedActors )
		{
			RedrawLevelEditingViewports();
		}
		else
		{
			Transaction.Cancel();
		}
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("DUPLICATE")) )
	{
		//@todo locked levels - if all actor levels are locked, cancel the transaction
		const FScopedTransaction Transaction( *LocalizeUnrealEd("DuplicateActors") );

		// if not specially handled by the current editing mode,
		if (!GEditorModeTools().HandleDuplicate())
		{
			// duplicate selected
			edactDuplicateSelected(TRUE);
			RebuildAlteredBSP(); // Update the Bsp of any levels containing a modified brush
		}
		RedrawLevelEditingViewports();
		return TRUE;
	}
	else if( ParseCommand(&Str, TEXT("ALIGN")) )
	{
		if( ParseCommand(&Str,TEXT("SNAPTOFLOOR")) )
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd("SnapActorsToFloor") );

			UBOOL bAlign=0;
			ParseUBOOL( Str, TEXT("ALIGN="), bAlign );

			UBOOL bUsePivot = FALSE;
			ParseUBOOL( Str, TEXT("USEPIVOT="), bUsePivot );

			// Fires CALLBACK_LevelDirtied when falling out of scope.
			FScopedLevelDirtied		LevelDirtyCallback;

			for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
			{
				AActor* Actor = static_cast<AActor*>( *It );
				checkSlow( Actor->IsA(AActor::StaticClass()) );

				Actor->Modify();
				MoveActorToFloor(Actor,bAlign,bUsePivot);
				Actor->InvalidateLightingCache();
				Actor->ForceUpdateComponents();

				Actor->MarkPackageDirty();
				LevelDirtyCallback.Request();
			}

			AActor* Actor = GetSelectedActors()->GetBottom<AActor>();
			if( Actor )
			{
				SetPivot( Actor->Location, FALSE, TRUE );

				if( GEditor->bGroupingActive ) 
				{
					// set group pivot for the root-most group
					AGroupActor* ActorGroupRoot = AGroupActor::GetRootForActor(Actor, TRUE, TRUE);
					if(ActorGroupRoot)
					{
						ActorGroupRoot->CenterGroupLocation();
					}
				}
			}

			RedrawLevelEditingViewports();
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("MOVETOGRID")) )
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd("MoveActorToGrid") );

			// Update the pivot location.
			const FVector OldPivot = GetPivotLocation();
			const FVector NewPivot = OldPivot.GridSnap(Constraints.GetGridSize());
			const FVector Delta = NewPivot - OldPivot;

			SetPivot( NewPivot, FALSE, TRUE );

			// Fires CALLBACK_LevelDirtied when falling out of scope.
			FScopedLevelDirtied		LevelDirtyCallback;

			for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
			{
				AActor* Actor = static_cast<AActor*>( *It );
				checkSlow( Actor->IsA(AActor::StaticClass()) );

				Actor->Modify();

				const FVector OldLocation = Actor->Location;

				GWorld->FarMoveActor( Actor, OldLocation+Delta, FALSE, FALSE, TRUE );
				Actor->InvalidateLightingCache();
				Actor->ForceUpdateComponents();

				Actor->MarkPackageDirty();
				LevelDirtyCallback.Request();
			}

			RedrawLevelEditingViewports();
			RebuildAlteredBSP(); // Update the Bsp of any levels containing a modified brush
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("ORIGIN")) )
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd("Undo_SnapBrushOrigin") );
			edactAlignOrigin();
			RedrawLevelEditingViewports();
			return TRUE;
		}
		else // "VERTS" (default)
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd("Undo_SnapBrushVertices") );
			edactAlignVertices();
			RedrawLevelEditingViewports();
			RebuildAlteredBSP(); // Update the Bsp of any levels containing a modified brush
			return TRUE;
		}
	}
	else if( ParseCommand(&Str,TEXT("TOGGLE")) )
	{
		if( ParseCommand(&Str,TEXT("LOCKMOVEMENT")) )			// ACTOR TOGGLE LOCKMOVEMENT
		{
			// Fires CALLBACK_LevelDirtied when falling out of scope.
			FScopedLevelDirtied		LevelDirtyCallback;

			for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
			{
				AActor* Actor = static_cast<AActor*>( *It );
				checkSlow( Actor->IsA(AActor::StaticClass()) );

				Actor->Modify();
				Actor->bLockLocation = !Actor->bLockLocation;

				Actor->MarkPackageDirty();
				LevelDirtyCallback.Request();
			}
		}

		RedrawLevelEditingViewports();
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("LEVELCURRENT")) )
	{
		MakeSelectedActorsLevelCurrent();
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("LEVELGRIDVOLUMECURRENT")) )
	{
		MakeSelectedActorsLevelGridVolumeCurrent();
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("MOVETOCURRENT")) )
	{
		const UBOOL bUseCurrentLevelGridVolume = TRUE;
		MoveSelectedActorsToCurrentLevel( bUseCurrentLevelGridVolume );
		return TRUE;
	}
	else if(ParseCommand(&Str, TEXT("FIND"))) //ACTOR FIND KISMET
	{
		if(ParseCommand(&Str, TEXT("KISMET")))
		{
			AActor *FirstActor = GEditor->GetSelectedActors()->GetTop<AActor>();
			
			if(FirstActor)
			{
				// Get the kismet sequence for the level the actor belongs to.
				USequence* RootSeq = GWorld->GetGameSequence(FirstActor->GetLevel());
				if(RootSeq && RootSeq->ReferencesObject(FirstActor))
				{
					WxKismet::FindActorReferences(FirstActor);
				}
			}

			return TRUE;
		}
	}
	else if(ParseCommand(&Str, TEXT("SYNCBROWSERMATERIALINSTANCE")))
	{
		GApp->EditorFrame->SyncMaterialToGenericBrowser(0, 0, FALSE);
		return TRUE;
	}
	else if(ParseCommand(&Str, TEXT("SYNCBROWSERMATERIAL")))
	{
		GApp->EditorFrame->SyncMaterialToGenericBrowser(0, 0, TRUE);
		return TRUE;
	}
	else if(ParseCommand(&Str, TEXT("SYNCBROWSERTEXTURE")))
	{
		GApp->EditorFrame->SyncTextureToGenericBrowser(0, 0, 0);
		return TRUE;
	}
	else if(ParseCommand(&Str, TEXT("SYNCBROWSER")))
	{
		GApp->EditorFrame->SyncToContentBrowser();
		return TRUE;
	}
	else if(ParseCommand(&Str, TEXT("DESELECT")))
	{
		const FScopedTransaction Transaction( TEXT("Deselect Actor(s)") );
		GEditor->GetSelectedActors()->Modify();

		//deselects everything in UnrealEd
		GUnrealEd->SelectNone(TRUE, TRUE);
		
		//Destroys any visible property windows associated with actors
		for(int Index = 0; Index < GUnrealEd->ActorProperties.Num(); ++Index)
		{
			GUnrealEd->ActorProperties(Index)->Destroy();
		}

		GUnrealEd->ActorProperties.Empty();
		return TRUE;
	}
	else if(ParseCommand(&Str, TEXT("EXPORT")))
	{
#if WITH_FBX
		if(ParseCommand(&Str, TEXT("FBX")))
		{
			WxFileDialog ExportFileDialog(GApp->EditorFrame, 
				*LocalizeUnrealEd("StaticMeshEditor_ExportToPromptTitle"), 
				*(GApp->LastDir[LD_GENERIC_EXPORT]), 
				TEXT(""), 
				TEXT("FBX document|*.fbx"),
				wxSAVE | wxOVERWRITE_PROMPT, 
				wxDefaultPosition);

			// Show dialog and execute the export if the user did not cancel out
			if( ExportFileDialog.ShowModal() == wxID_OK )
			{
				// Get the filename from dialog
				wxString ExportFilename = ExportFileDialog.GetPath();
				FFilename FileName = ExportFilename.c_str();
				GApp->LastDir[LD_GENERIC_EXPORT] = FileName.GetPath(); // Save path as default for next time.
				UnFbx::CFbxExporter* Exporter = UnFbx::CFbxExporter::GetInstance();
				Exporter->CreateDocument();
				for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
				{
					AActor* Actor = static_cast<AActor*>( *It );
					if (Actor->IsA(AActor::StaticClass()))
					{
						if (Actor->IsA(AStaticMeshActor::StaticClass()))
						{
							Exporter->ExportStaticMesh(Actor, ((AStaticMeshActor*)Actor)->StaticMeshComponent, NULL);
						}
						else if (Actor->IsA(ASkeletalMeshActor::StaticClass()))
						{
							Exporter->ExportSkeletalMesh(Actor, ((ASkeletalMeshActor*)Actor)->SkeletalMeshComponent);
						}
						else if (Actor->IsA(ADynamicSMActor::StaticClass()))
						{
							Exporter->ExportStaticMesh(Actor, ((ADynamicSMActor*)Actor)->StaticMeshComponent, NULL );
						}
						else if (Actor->IsA(ABrush::StaticClass()))
						{
							Exporter->ExportBrush( (ABrush*)Actor, NULL, TRUE );
						}
					}
				}
				Exporter->WriteToFile(*FileName);
			}

			return TRUE;
		}
#endif
	}

	return FALSE;
}


UBOOL UUnrealEdEngine::Exec_Mode( const TCHAR* Str, FOutputDevice& Ar )
{
	Word2 = EM_None;

	UBOOL DWord1;
	if( ParseCommand(&Str,TEXT("WIDGETCOORDSYSTEMCYCLE")) )
	{
		INT Wk = GEditorModeTools().CoordSystem;
		Wk++;

		if( Wk == COORD_Max )
		{
			Wk -= COORD_Max;
		}
		GEditorModeTools().CoordSystem = (ECoordSystem)Wk;
		GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
		GCallbackEvent->Send( CALLBACK_UpdateUI );
	}
	if( ParseCommand(&Str,TEXT("WIDGETMODECYCLE")) )
	{
		GEditorModeTools().CycleWidgetMode();
	}
	if( ParseUBOOL(Str,TEXT("GRID="), DWord1) )
	{
		FinishAllSnaps();
		Constraints.GridEnabled = DWord1;
		GCallbackEvent->Send( CALLBACK_UpdateUI );
	}
	if( ParseUBOOL(Str,TEXT("ROTGRID="), DWord1) )
	{
		FinishAllSnaps();
		Constraints.RotGridEnabled=DWord1;
		GCallbackEvent->Send( CALLBACK_UpdateUI );
	}
	if( ParseUBOOL(Str,TEXT("SNAPVERTEX="), DWord1) )
	{
		FinishAllSnaps();
		Constraints.SnapVertices=DWord1;
		GCallbackEvent->Send( CALLBACK_UpdateUI );
	}
	if( ParseUBOOL(Str,TEXT("ALWAYSSHOWTERRAIN="), DWord1) )
	{
		FinishAllSnaps();
		AlwaysShowTerrain=DWord1;
	}
	if( ParseUBOOL(Str,TEXT("USEACTORROTATIONGIZMO="), DWord1) )
	{
		FinishAllSnaps();
		UseActorRotationGizmo=DWord1;
	}
	if( ParseUBOOL(Str,TEXT("SHOWBRUSHMARKERPOLYS="), DWord1) )
	{
		FinishAllSnaps();
		bShowBrushMarkerPolys=DWord1;
	}
	if( ParseUBOOL(Str,TEXT("SELECTIONLOCK="), DWord1) )
	{
		FinishAllSnaps();
		// If -1 is passed in, treat it as a toggle.  Otherwise, use the value as a literal assignment.
		if( DWord1 == -1 )
			GEdSelectionLock=(GEdSelectionLock == 0) ? 1 : 0;
		else
			GEdSelectionLock=DWord1;
		Word1=MAXWORD;
	}
	Parse(Str,TEXT("MAPEXT="), GApp->DefaultMapExt);
	if( ParseUBOOL(Str,TEXT("USESIZINGBOX="), DWord1) )
	{
		FinishAllSnaps();
		// If -1 is passed in, treat it as a toggle.  Otherwise, use the value as a literal assignment.
		if( DWord1 == -1 )
			UseSizingBox=(UseSizingBox == 0) ? 1 : 0;
		else
			UseSizingBox=DWord1;
		Word1=MAXWORD;
	}
	
	if(GCurrentLevelEditingViewportClient)
	{
		Parse( Str, TEXT("SPEED="), GCurrentLevelEditingViewportClient->CameraSpeed );
	}

	Parse( Str, TEXT("SNAPDIST="), Constraints.SnapDistance );
	//
	// Major modes:
	//
	if 		(ParseCommand(&Str,TEXT("CAMERAMOVE")))		Word2 = EM_Default;
	else if (ParseCommand(&Str,TEXT("TERRAINEDIT")))	Word2 = EM_TerrainEdit;
	else if	(ParseCommand(&Str,TEXT("GEOMETRY"))) 		Word2 = EM_Geometry;
	else if	(ParseCommand(&Str,TEXT("STATICMESH")))		Word2 = EM_StaticMesh;
	else if	(ParseCommand(&Str,TEXT("TEXTURE"))) 		Word2 = EM_Texture;
	else if (ParseCommand(&Str,TEXT("COVEREDIT")))		Word2 = EM_CoverEdit;
	else if (ParseCommand(&Str,TEXT("MESHPAINT")))		Word2 = EM_MeshPaint;
	else if (ParseCommand(&Str,TEXT("LANDSCAPE")))		Word2 = EM_Landscape;
	else if (ParseCommand(&Str,TEXT("FOLIAGE")))		Word2 = EM_Foliage;

	if( Word2 != EM_None )
		GCallbackEvent->Send( CALLBACK_ChangeEditorMode, Word2 );

	// Reset the roll on all viewport cameras
	for(UINT ViewportIndex = 0;ViewportIndex < (UINT)ViewportClients.Num();ViewportIndex++)
	{
		if(ViewportClients(ViewportIndex)->ViewportType == LVT_Perspective)
			ViewportClients(ViewportIndex)->ViewRotation.Roll = 0;
	}

	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );

	return TRUE;
}

UBOOL UUnrealEdEngine::Exec_Group( const TCHAR* Str, FOutputDevice& Ar )
{
	if(GEditor->bGroupingActive)
	{
		if( ParseCommand(&Str,TEXT("REGROUP")) )
		{
			GUnrealEd->edactRegroupFromSelected();
			return TRUE;
		}
		else if ( ParseCommand(&Str,TEXT("UNGROUP")) )
		{
			GUnrealEd->edactUngroupFromSelected();
			return TRUE;
		}
	}

	if ( ParseCommand(&Str,TEXT("TOGGLEMODE")) )
	{
		AGroupActor::ToggleGroupMode();
		return TRUE;
	}

	return FALSE;
}