/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "FileHelpers.h"
#include "InterpEditor.h"
#include "LevelBrowser.h"
#include "Kismet.h"
#include "Menus.h"
#include "PropertyWindow.h"
#include "PropertyWindowManager.h"
#include "EngineAnimClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "BusyCursor.h"
#include "DlgGenericComboEntry.h"
#include "Sentinel.h"
#include "GameStatsBrowser.h"
#include "DlgCheckBoxList.h"
#include "UnConsoleSupportContainer.h"
#include "EditorLevelUtils.h"
#include "AVIWriter.h"
#include "FileHelpers.h"

#if WITH_APEX
#include "NvApexManager.h"
#endif

#if WITH_MANAGED_CODE
	#include "ContentBrowserShared.h"
#endif

IMPLEMENT_COMPARE_CONSTREF( FString, UnrealEdEngine, { return appStricmp( *A, *B ); } )

/**
 * Initialize the engine.
 */
void UUnrealEdEngine::Init()
{
	Super::Init();

	// Register for the package modified callback to catch packages that have been modified and need to be checked out.
	GCallbackEvent->Register( CALLBACK_PackageModified, this );

	// Initialize this value to false, we arent autosaving.
	bIsAutoSaving = FALSE;

	InitBuilderBrush();

	// Iterate over all always fully loaded packages and load them.
	for( INT PackageNameIndex=0; PackageNameIndex<PackagesToBeFullyLoadedAtStartup.Num(); PackageNameIndex++ )
	{
		const FString& PackageName = PackagesToBeFullyLoadedAtStartup(PackageNameIndex);
		FString Filename;
		// Load package if it's found in the package file cache.
		if( GPackageFileCache->FindPackageFile( *PackageName, NULL, Filename ) )
		{
			LoadPackage( NULL, *Filename, LOAD_None );
		}
	}

	//make sure these have been set to the appropriate defaults
	if (GEditor->PlayInEditorWidth == 0)
	{
		GEditor->PlayInEditorWidth = GSystemSettings.ResX;
	}
	if (GEditor->PlayInEditorHeight == 0)
	{
		GEditor->PlayInEditorHeight = GSystemSettings.ResY;
	}


#if WITH_MANAGED_CODE

	if ( !GIsUCCMake && FContentBrowser::ShouldUseContentBrowser() )
	{
		TArray<FFilename> LocFilenames;
		LocFilenames.AddItem(FString::Printf(TEXT("..\\..\\Engine\\Localization\\%s\\UnrealEd.%s"), *appGetLanguageExt(), *appGetLanguageExt()));
		LocFilenames.AddItem(FString::Printf(TEXT("..\\..\\Engine\\Localization\\%s\\EditorTips.%s"), *appGetLanguageExt(), *appGetLanguageExt()));

		FString GBString, GBStringLC;
		GBString = TEXT("Generic Browser");
	
		FString CBString, CBStringLC;
		if ( !GConfig->GetString(TEXT("Caption"), TEXT("ContentBrowser_Caption"), CBString, *LocFilenames(0)) )
		{
			CBString = TEXT("Content Browser");
		}
		GBStringLC = GBString.ToLower();
		CBStringLC = CBString.ToLower();

		// replace loc strings which reference the Generic Browser with Content Browser in the editor loc files
		for ( INT Idx = 0; Idx < LocFilenames.Num(); Idx++ )
		{
			FFilename& LocFilename = LocFilenames(Idx);
			FConfigFile* LocFile = GConfig->Find(*LocFilename, TRUE);
			if ( LocFile != NULL )
			{
				for ( TMap<FString,FConfigSection>::TIterator FileItor(*LocFile); FileItor; ++FileItor )
				{
					FString& SectionName = FileItor.Key();
					FConfigSection& Section = FileItor.Value();

					if ( SectionName != TEXT("Caption") )
					{
						for ( FConfigSectionMap::TIterator SectionItor(Section); SectionItor; ++SectionItor )
						{
							const FName SectionKey = SectionItor.Key();
							FString& SectionValue = SectionItor.Value();

							if ( SectionValue.InStr(GBString) != INDEX_NONE )
							{
//								warnf(TEXT("Replacing [%s.%s.%s] with %s"), *LocFilename.GetBaseFilename(), *SectionName, *SectionKey, *SectionValue.Replace(*GBString, *CBString));
								Section.Set(*SectionKey.ToString(), SectionValue.Replace(*GBString, *CBString));
							}
							else if ( SectionValue.InStr(GBStringLC) != INDEX_NONE )
							{
//								warnf(TEXT("Replacing [%s.%s.%s] with %s"), *LocFilename.GetBaseFilename(), *SectionName, *SectionKey, *SectionValue.Replace(*GBStringLC, *CBStringLC));
								Section.Set(*SectionKey.ToString(), SectionValue.Replace(*GBStringLC, *CBStringLC));
							}
						}
					}
				}
			}
			else
			{
				warnf(TEXT("######################################## FAILED TO FIND LOC FILE %s"), *LocFilename);
			}			
		}
		// more strings here
	}
	
#endif

	// Populate the data structures related to the sprite category visibility feature for use elsewhere in the editor later
	TMap<FString, FName> LocalizedToUnlocalizedMap;

	// Iterate over all classes searching for those which derive from AActor and are neither deprecated nor abstract.
	// It would be nice to only check placeable classes here, but we cannot do that as some non-placeable classes
	// still end up in the editor (with sprites) procedurally, such as prefab instances and landscape actors.
	for ( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
	{
		if ( ClassIt->IsChildOf( AActor::StaticClass() )
			&& !( ClassIt->ClassFlags & CLASS_Abstract )
			&& !( ClassIt->ClassFlags & CLASS_Deprecated ) )
		{
			// Check if the class default actor has sprite components or arrow components that should be treated as
			// sprites, and if so, add their categories to the array
			const AActor* CurDefaultClassActor = ClassIt->GetDefaultActor();
			if ( CurDefaultClassActor )
			{
				for ( TArray<UActorComponent*>::TConstIterator CompIter( CurDefaultClassActor->Components ); CompIter; ++CompIter )
				{
					const USpriteComponent* CurSpriteComponent = Cast<USpriteComponent>( *CompIter );
					const UArrowComponent* CurArrowComponent = Cast<UArrowComponent>( *CompIter );
					if ( CurSpriteComponent )
					{
						FString LocalizedCategory = LocalizeUnrealEd( *CurSpriteComponent->SpriteCategoryName.ToString() );
						LocalizedToUnlocalizedMap.Set( LocalizedCategory, CurSpriteComponent->SpriteCategoryName );
						SortedSpriteCategories.AddUniqueItem( LocalizedCategory );
					}
					else if ( CurArrowComponent && CurArrowComponent->bTreatAsASprite )
					{
						FString LocalizedCategory = LocalizeUnrealEd( *CurArrowComponent->SpriteCategoryName.ToString() );
						LocalizedToUnlocalizedMap.Set( LocalizedCategory, CurArrowComponent->SpriteCategoryName );
						SortedSpriteCategories.AddUniqueItem( LocalizedCategory );
					}
				}
			}
		}
	}

	// Sort the localized array of categories
	Sort<USE_COMPARE_CONSTREF(FString, UnrealEdEngine)>( &SortedSpriteCategories(0), SortedSpriteCategories.Num() );

	// Iterate over the sorted list, constructing a mapping of unlocalized categories to the index the localized category
	// resides in. This is an optimization to prevent having to localize values repeatedly.
	for ( TArray<FString>::TConstIterator SortedIter( SortedSpriteCategories ); SortedIter; ++SortedIter )
	{
		const FName* UnlocalizedCategory = LocalizedToUnlocalizedMap.Find( *SortedIter );
		check( UnlocalizedCategory );
		UnlocalizedCategoryToIndexMap.Set( *UnlocalizedCategory, SortedIter.GetIndex() );
	}
}

void UUnrealEdEngine::PreExit()
{
	// Notify edit modes we're mode at exit
	GEditorModeTools().Shutdown();

	FAVIWriter* AVIWriter = FAVIWriter::GetInstance();
	if (AVIWriter)
	{
		AVIWriter->Close();
	}

	Super::PreExit();
}

void UUnrealEdEngine::FinishDestroy()
{
	GCallbackEvent->Unregister( CALLBACK_PackageModified, this );
	Super::FinishDestroy();
}

void UUnrealEdEngine::Tick(FLOAT DeltaSeconds)
{
	Super::Tick( DeltaSeconds );

#if WITH_APEX
	if ( GApexManager )
	{
		GApexManager->Pump();
	}
#endif

	// Increment the "seconds since last autosave" counter, then try to autosave.
	if (!GSlowTaskOccurred && !bAutosaveCountPaused)
	{
		AutosaveCount += DeltaSeconds;
	}
	if (!GIsSlowTask)
	{
		GSlowTaskOccurred = FALSE;
	}

	// Display any load errors that happened while starting up the editor.
	static UBOOL bFirstTick = TRUE;
	if (bFirstTick && GEdLoadErrors.Num())
	{
		GCallbackEvent->Send( CALLBACK_DisplayLoadErrors );
	}
	bFirstTick = FALSE;

	GApp->EditorFrame->UpdateDirtiedUI();	

	// Tick the Levelbrowser so it can do things like process deferred actors that have recently moved/changed
	WxLevelBrowser* LevelBrowser = GUnrealEd->GetBrowser<WxLevelBrowser>( TEXT("LevelBrowser") );
	if( LevelBrowser != NULL )
	{
		LevelBrowser->Tick(DeltaSeconds);
	}

	AttemptAutosave();

	// Try and notify the user about modified packages needing checkout
	AttemptModifiedPackageNotification();

	// Attempt to warn about any packages that have been modified but were previously
	// saved with an engine version newer than the current one
	AttemptWarnAboutPackageEngineVersions();

	FAVIWriter* AVIWriter = FAVIWriter::GetInstance();
	if (AVIWriter)
	{
		AVIWriter->Update();
	}
}


void UUnrealEdEngine::Send( ECallbackEventType InType, UObject* InObject )
{
	switch( InType )
	{
	case CALLBACK_PackageModified:
		{
			// The passed in object should never be NULL
			check(InObject);

			UPackage* Package = InObject->GetOutermost();
			const FString PackageName = Package->GetName();

			// Alert the user if they have modified a package that won't be able to be saved because
			// it's already been saved with an engine version that is newer than the current one.
			if ( !GIsRoutingPostLoad && !PackagesCheckedForEngineVersion.HasKey( PackageName ) )
			{
				EEngineVerWarningState WarningStateToSet = VWS_WarningUnnecessary;
				
				FString PackageFileName;
				if ( GPackageFileCache->FindPackageFile( *Package->GetName(), NULL, PackageFileName ) )
				{
					// If a package has never been loaded then it won't properly have the PKG_SavedWithNewerVersion flag set on its
					// package flags. Therefore a file reader is necessary to find the package file summary for its saved engine version.
					FArchive* PackageReader = GFileManager->CreateFileReader( *PackageFileName );
					if ( PackageReader )
					{
						FPackageFileSummary Summary;
						*PackageReader << Summary;

						if ( Summary.EngineVersion > GEngineVersion )
						{
							WarningStateToSet = VWS_PendingWarn;
							bNeedWarningForPkgEngineVer = TRUE;
						}
					}
					delete PackageReader;
				}
				PackagesCheckedForEngineVersion.Set( PackageName, WarningStateToSet );
			}
#if HAVE_SCC
			if( !Package->IsDirty() )
			{
				// This package was saved, the user should be prompted again if they checked in the package
				PackageToNotifyState.Remove( Package );
			}
			else
			{
				// Find out if we have already asked the user to modify this package
				const BYTE* PromptState = PackageToNotifyState.Find( Package );
				const UBOOL bAlreadyAsked = PromptState != NULL;

				// Get the source control state of the package
				const INT SCCState = GPackageFileCache->GetSourceControlState( *Package->GetName() );

				// During an autosave, packages are saved in the autosave directory which switches off their dirty flags.
				// To preserve the pre-autosave state, any saved package is then remarked as dirty because it wasn't saved in the normal location where it would be picked up by source control.
				// Any callback that happens during an autosave is bogus since a package wasnt  marked dirty due to a user modification.
				if( !bIsAutoSaving && 
					!GIsEditorLoadingMap && // Don't ask if the package was modified as a result of a load
					!bAlreadyAsked && // Don't ask if we already asked once!
					GEditor->GetUserSettings().bPromptForCheckoutOnPackageModification && // Only prompt if the user has specified to be prompted on modification
					(SCCState == SCC_ReadOnly || SCCState == SCC_NotCurrent || SCCState == SCC_CheckedOutOther) )
				{
					// Allow packages that are not checked out to pass through.
					// Allow packages that are not current or checked out by others pass through.  
					// The user wont be able to checkout these packages but the checkout dialog will show up with a special icon 
					// to let the user know they wont be able to checkout the package they are modifying.

					PackageToNotifyState.Set( Package, NS_PendingPrompt );
					// We need to prompt since a new package was added
					bNeedToPromptForCheckout = TRUE;
				}
			}
#endif
		}
		break;
	default:
		break;
	}
}

/**
 * Resets the autosave timer.
 */
void UUnrealEdEngine::ResetAutosaveTimer()
{
	//debugf( TEXT("Resetting autosave timer") );

	// Reset the "seconds since last autosave" counter.
	AutosaveCount = 0.0f;
}

void UUnrealEdEngine::PauseAutosaveTimer(UBOOL bPaused)
{
	bAutosaveCountPaused = bPaused;
}

/**
 * Creates an editor derived from the wxInterpEd class.
 *
 * @return		A heap-allocated WxInterpEd instance.
 */
WxInterpEd *UUnrealEdEngine::CreateInterpEditor( wxWindow* InParent, wxWindowID InID, class USeqAct_Interp* InInterp )
{
	return new WxInterpEd( InParent, InID, InInterp );
}

/**
* Creates an editor derived from the WxCameraAnimEd class.
*
* @return		A heap-allocated WxCameraAnimEd instance.
*/
WxCameraAnimEd *UUnrealEdEngine::CreateCameraAnimEditor( wxWindow* InParent, wxWindowID InID, class USeqAct_Interp* InInterp )
{
	return new WxCameraAnimEd( InParent, InID, InInterp );
}

void UUnrealEdEngine::ShowActorProperties()
{
	// See if we have any unlocked property windows available.  If not, create a new one.

	INT x;

	for( x = 0 ; x < ActorProperties.Num() ; ++x )
	{
		if( !ActorProperties(x)->IsLocked() )
		{
			ActorProperties(x)->Show();
			ActorProperties(x)->Raise();
			break;
		}
	}

	// If no unlocked property windows are available, create a new one

	if( x == ActorProperties.Num() )
	{
		WxPropertyWindowFrame* pw = new WxPropertyWindowFrame;
		pw->Create( GApp->EditorFrame, -1, this );

		//tell this property window to execute ACTOR DESELECT when the escape key is pressed
		pw->SetFlags(EPropertyWindowFlags::ExecDeselectOnEscape, TRUE);

		ActorProperties.AddItem( pw );
		UpdatePropertyWindows();
		pw->Show();
		pw->Raise();
	}
}

void UUnrealEdEngine::ShowWorldProperties( const UBOOL bExpandTo, const FString& PropertyName )
{
	if( !LevelProperties )
	{
		LevelProperties = new WxPropertyWindowFrame;
		LevelProperties->Create( GApp->EditorFrame, -1, this );
		LevelProperties->SetFlags(EPropertyWindowFlags::SupportsCustomControls, TRUE);
	}
	LevelProperties->Show();
	LevelProperties->SetObject( GWorld->GetWorldInfo(), EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories );
	if( bExpandTo )
	{
		LevelProperties->ExpandToItem( PropertyName, INDEX_NONE, TRUE );
	}
}

/**
 * Pastes clipboard text into a clippad entry
 */
void UUnrealEdEngine::PasteClipboardIntoClipPad()
{
	AWorldInfo* Info = GWorld->GetWorldInfo();
	FString ClipboardText = appClipboardPaste();

	WxDlgGenericComboEntry* dlg = new WxDlgGenericComboEntry( FALSE, FALSE );

	// Gather up the existing clip pad entry names

	TArray<FString> ClipPadNames;

	for( int x = 0 ; x < Info->ClipPadEntries.Num() ; ++x )
	{
		ClipPadNames.AddItem( Info->ClipPadEntries(x)->Title );
	}

	// Ask where the user wants to save this clip

	if( dlg->ShowModal( TEXT("ClipPadSaveAs"), TEXT("SaveAs"), ClipPadNames, 0, TRUE ) == wxID_OK )
	{
		// If the name chosen matches an existing clip, overwrite it.  Otherwise, create a new clip entry.

		int idx = -1;

		for( int x = 0 ; x < Info->ClipPadEntries.Num() ; ++x )
		{
			if( Info->ClipPadEntries(x)->Title == dlg->GetComboBoxString() )
			{
				idx = x;
				break;
			}
		}

		if( idx == -1 )
		{
			Info->ClipPadEntries.AddItem( ConstructObject<UClipPadEntry>( UClipPadEntry::StaticClass(), Info ) );
			idx = Info->ClipPadEntries.Num() - 1;
		}

		Info->ClipPadEntries( idx )->Title = dlg->GetComboBoxString();
		Info->ClipPadEntries( idx )->Text = ClipboardText;
	}
}


/**
* Updates the mouse position status bar field.
*
* @param PositionType	Mouse position type, used to decide how to generate the status bar string.
* @param Position		Position vector, has the values we need to generate the string.  These values are dependent on the position type.
*/
void UUnrealEdEngine::UpdateMousePositionText( EMousePositionType PositionType, const FVector &Position )
{
	FString PositionString;

	switch(PositionType)
	{
	case MP_WorldspacePosition:
		{
			PositionString = FString::Printf(LocalizeSecure(LocalizeUnrealEd("StatusBarMouseWorldspacePosition"), (INT)(Position.X + 0.5f), (INT)(Position.Y + 0.5f), (INT)(Position.Z + 0.5f)));
		}
		break;
	case MP_Translate:
		{
			PositionString = FString::Printf(LocalizeSecure(LocalizeUnrealEd("StatusBarWidgetPosition"), Position.X, Position.Y, Position.Z));
		}	
		break;
	case MP_Rotate:
		{
			FVector NormalizedRotation;

			for ( INT Idx = 0; Idx < 3; Idx++)
			{
				NormalizedRotation[Idx] = 360.f * (Position[Idx] / 65536.f);

				const INT Revolutions = NormalizedRotation[Idx] / 360.f;
				NormalizedRotation[Idx] -= Revolutions * 360;
			}


			// 0x00B0 is the Unicode code point for the Degrees symbol.
			PositionString = FString::Printf(LocalizeSecure(LocalizeUnrealEd("StatusBarWidgetRotation"), NormalizedRotation.X, TEXT("\xB0"), NormalizedRotation.Y, TEXT("\xB0"), NormalizedRotation.Z, TEXT("\xB0")));
		}	
		break;
	case MP_Scale:
		{
			FLOAT ScaleFactor;
			FVector FinalScale = Position;
			FinalScale /= FVector(GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize());

			if(GEditor->Constraints.SnapScaleEnabled)
			{
				ScaleFactor = GEditor->Constraints.ScaleGridSize;
			}
			else
			{
				ScaleFactor = 10.0f;
			}

			FinalScale *= FVector(ScaleFactor, ScaleFactor, ScaleFactor);
			PositionString = FString::Printf(LocalizeSecure(LocalizeUnrealEd("StatusBarWidgetScale"), 
				FinalScale.X, TEXT('%'), FinalScale.Y, TEXT('%'), FinalScale.Z, TEXT('%')));
		}	
		break;
	
	case MP_CameraSpeed:
		{
			PositionString = FString::Printf( LocalizeSecure( LocalizeUnrealEd( "StatusBarCameraSpeed_F" ), Position.X ) );
		}
		break;

	case MP_None:
		{
			PositionString = TEXT("");
		}
		break;
	case MP_NoChange:
		{
			PositionString = LocalizeUnrealEd("StatusBarNoChange");
		}
		break;
	default:
		checkMsg(0, "Unhandled mouse position type.");
                break;
	}

	GApp->EditorFrame->StatusBars[SB_Standard]->SetMouseWorldspacePositionText(*PositionString);
}


/**
* Returns whether or not the map build in progressed was cancelled by the user. 
*/
UBOOL UUnrealEdEngine::GetMapBuildCancelled() const
{
	return GApp->GetMapBuildCancelled();
}

/**
* Sets the flag that states whether or not the map build was cancelled.
*
* @param InCancelled	New state for the cancelled flag.
*/
void UUnrealEdEngine::SetMapBuildCancelled( UBOOL InCancelled )
{
	GApp->SetMapBuildCancelled( InCancelled );
}


/**
 * @return Returns the global instance of the editor options class.
 */
UUnrealEdOptions* UUnrealEdEngine::GetUnrealEdOptions()
{
	if(EditorOptionsInst == NULL)
	{
		EditorOptionsInst = ConstructObject<UUnrealEdOptions>(UUnrealEdOptions::StaticClass());
	}

	return EditorOptionsInst;
}


/**
* Closes the main editor frame.
*/ 
void UUnrealEdEngine::CloseEditor()
{
	GApp->EditorFrame->Close();
}

void UUnrealEdEngine::ShowUnrealEdContextMenu()
{
	WxMainContextMenu ContextMenu;
	FTrackPopupMenu tpm( GApp->EditorFrame, &ContextMenu );
	tpm.Show();
}

void UUnrealEdEngine::ShowUnrealEdContextSurfaceMenu()
{
	WxMainContextSurfaceMenu SurfaceMenu;
	FTrackPopupMenu tpm( GApp->EditorFrame, &SurfaceMenu );
	tpm.Show();
}

void UUnrealEdEngine::ShowUnrealEdContextCoverSlotMenu(ACoverLink *Link, FCoverSlot &Slot)
{
	WxMainContextCoverSlotMenu CoverSlotMenu(Link,Slot);
	FTrackPopupMenu tpm( GApp->EditorFrame, &CoverSlotMenu );
	tpm.Show();
}


/**
 * @return TRUE if selection of translucent objects in perspective viewports is allowed
 */
UBOOL UUnrealEdEngine::AllowSelectTranslucent() const
{
	return GEditor->GetUserSettings().bAllowSelectTranslucent;
}


/**
 * @return TRUE if only editor-visible levels should be loaded in Play-In-Editor sessions
 */
UBOOL UUnrealEdEngine::OnlyLoadEditorVisibleLevelsInPIE() const
{
	return GEditor->GetUserSettings().bOnlyLoadVisibleLevelsInPIE;
}


/**
* Redraws all level editing viewport clients.
 *
 * @param	bInvalidateHitProxies		[opt] If TRUE (the default), invalidates cached hit proxies too.
 */
void UUnrealEdEngine::RedrawLevelEditingViewports(UBOOL bInvalidateHitProxies)
{
	if( GApp && GApp->EditorFrame && GApp->EditorFrame->ViewportConfigData )
	{
		for( INT ViewportIndex = 0 ; ViewportIndex < GApp->EditorFrame->ViewportConfigData->GetViewportCount() ; ++ViewportIndex )
		{
			FVCD_Viewport& CurViewport = GApp->EditorFrame->ViewportConfigData->AccessViewport(ViewportIndex);
			if( CurViewport.bEnabled && CurViewport.ViewportWindow != NULL )
			{
				if ( bInvalidateHitProxies )
				{
					// Invalidate hit proxies and display pixels.
					CurViewport.ViewportWindow->Viewport->Invalidate();
				}
				else
				{
					// Invalidate only display pixels.
					CurViewport.ViewportWindow->Viewport->InvalidateDisplay();
				}
			}
		}
	}
}

void UUnrealEdEngine::SetCurrentClass( UClass* InClass )
{
	USelection* SelectionSet = GetSelectedObjects();
	SelectionSet->SelectNone( UClass::StaticClass() );

	if(InClass != NULL)
	{
		SelectionSet->Select( InClass );
	}
}

// Fills a TArray with loaded UPackages

void UUnrealEdEngine::GetPackageList( TArray<UPackage*>* InPackages, UClass* InClass )
{
	InPackages->Empty();

	for( FObjectIterator It ; It ; ++It )
	{
		if( It->GetOuter() && It->GetOuter() != UObject::GetTransientPackage() )
		{
			UObject* TopParent = NULL;

			if( InClass == NULL || It->IsA( InClass ) )
				TopParent = It->GetOutermost();

			if( Cast<UPackage>(TopParent) )
				InPackages->AddUniqueItem( (UPackage*)TopParent );
		}
	}
}

/**
 * Returns whether saving the specified package is allowed
 */
UBOOL UUnrealEdEngine::CanSavePackage( UPackage* PackageToSave )
{
#if !WITH_FACEFX
	if (PackageToSave && ((PackageToSave->PackageFlags & PKG_ContainsFaceFXData) != 0))
	{
		warnf(NAME_Warning, TEXT("%s contains FaceFX data. You will NOT be allowed to save it!"), *(PackageToSave->GetName()));
		return FALSE;
	}
#endif	//#if !WITH_FACEFX

#if WITH_MANAGED_CODE
	FContentBrowser &CBInstance = FContentBrowser::GetActiveInstance();
	return CBInstance.AllowPackageSave( PackageToSave );
#else
	return TRUE;
#endif
}


/** Returns the thumbnail manager and creates it if missing */
UThumbnailManager* UUnrealEdEngine::GetThumbnailManager()
{
	// Create it if we need to
	if (ThumbnailManager == NULL)
	{
		if (ThumbnailManagerClassName.Len() > 0)
		{
			// Try to load the specified class
			UClass* Class = LoadObject<UClass>(NULL,*ThumbnailManagerClassName,
				NULL,LOAD_None,NULL);
			if (Class != NULL)
			{
				// Create an instance of this class
				ThumbnailManager = ConstructObject<UThumbnailManager>(Class);
			}
		}
		// If the class couldn't be loaded or is the wrong type, fallback to ours
		if (ThumbnailManager == NULL)
		{
			ThumbnailManager = ConstructObject<UThumbnailManager>(UThumbnailManager::StaticClass());
		}
		// Tell it to load all of its classes
		ThumbnailManager->Initialize();
	}
	return ThumbnailManager;
}

/**
 * Returns the browser manager and creates it if missing
 */
UBrowserManager* UUnrealEdEngine::GetBrowserManager(void)
{
	// Create it if we need to
	if (BrowserManager == NULL)
	{
		if (BrowserManagerClassName.Len() > 0)
		{
			// Try to load the specified class
			UClass* Class = LoadObject<UClass>(NULL,*BrowserManagerClassName,
				NULL,LOAD_None,NULL);
			if (Class != NULL)
			{
				// Create an instance of this class
				BrowserManager = ConstructObject<UBrowserManager>(Class);
			}
		}
		// If the class couldn't be loaded or is the wrong type, fallback to ours
		if (BrowserManager == NULL)
		{
			BrowserManager = ConstructObject<UBrowserManager>(UBrowserManager::StaticClass());
		}
	}
	return BrowserManager;
}

/**
 * Serializes this object to an archive.
 *
 * @param	Ar	the archive to serialize to.
 */
void UUnrealEdEngine::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << ThumbnailManager << BrowserManager;
	Ar << MaterialCopyPasteBuffer;
	Ar << AnimationCompressionAlgorithms;
	Ar << MatineeCopyPasteBuffer;
	Ar << SoundCueCopyPasteBuffer;
	if( Ar.IsAllowingReferenceElimination() && Ar.IsObjectReferenceCollector() )
	{
		Ar << PackageToNotifyState;
	}
}

/**
 * If all selected actors belong to the same level, that level is made the current level.
 */
void UUnrealEdEngine::MakeSelectedActorsLevelCurrent()
{
	ULevel* LevelToMakeCurrent = NULL;

	// Look to the selected actors for the level to make current.
	// If actors from multiple levels are selected, do nothing.
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		ULevel* ActorLevel = Actor->GetLevel();

		if ( !LevelToMakeCurrent )
		{
			// First assignment.
			LevelToMakeCurrent = ActorLevel;
		}
		else if ( LevelToMakeCurrent != ActorLevel )
		{
			// Actors from multiple levels are selected -- abort.
			LevelToMakeCurrent = NULL;
			break;
		}
	}

	if ( LevelToMakeCurrent )
	{
		// Update the level browser with the new current level.
		WxLevelBrowser* LevelBrowser = GetBrowser<WxLevelBrowser>( TEXT("LevelBrowser") );
		LevelBrowser->MakeLevelCurrent( LevelToMakeCurrent );

		// If there are any kismet windows open . . .
		if ( GApp->KismetWindows.Num() > 0 )
		{
			// . . . and if the level has a kismet sequence associate with it . . .
			USequence* LevelSequence = GWorld->GetGameSequence( LevelToMakeCurrent );
			if ( LevelSequence )
			{
				// Grab the first one and set the workspace to be that of the level.
				WxKismet* FirstKismetWindow = GApp->KismetWindows(0);
				FirstKismetWindow->ChangeActiveSequence( LevelSequence, TRUE );
			}
		}
	}
}



/**
 * If all selected actors belong to the same level grid volume, that level grid volume is made current.
 */
void UUnrealEdEngine::MakeSelectedActorsLevelGridVolumeCurrent()
{
	ALevelGridVolume* LevelGridVolumeToMakeCurrent = NULL;

	// Look to the selected actors for the level grid volume to make current.
	// If actors from multiple level grid volumes are selected, do nothing.
	for( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		ALevelGridVolume* ActorLevelGridVolume = EditorLevelUtils::GetLevelGridVolumeForActor( Actor );
		if( ActorLevelGridVolume != NULL )
		{
			if ( !LevelGridVolumeToMakeCurrent )
			{
				// First assignment.
				LevelGridVolumeToMakeCurrent = ActorLevelGridVolume;
			}
			else if ( LevelGridVolumeToMakeCurrent != ActorLevelGridVolume )
			{
				// Actors from multiple level grid volumes are selected -- abort.
				LevelGridVolumeToMakeCurrent = NULL;
				break;
			}
		}
		else
		{
			// At least one of the selected actors is not in a level grid volume
			LevelGridVolumeToMakeCurrent = NULL;
			break;
		}
	}

	if ( LevelGridVolumeToMakeCurrent )
	{
		// Update the level browser with the new current level.
		WxLevelBrowser* LevelBrowser = GetBrowser<WxLevelBrowser>( TEXT("LevelBrowser") );
		LevelBrowser->MakeLevelGridVolumeCurrent( LevelGridVolumeToMakeCurrent );
	}
}


/** Hook for game stats tool to render things in viewport. */
void UUnrealEdEngine::GameStatsRender(FEditorLevelViewportClient* ViewportClient, const FSceneView* View, FCanvas* Canvas, ELevelViewportType ViewportType)
{
	check(GApp);
	if(GApp->GameStatsVisualizer)
	{
		GApp->GameStatsVisualizer->RenderStats(ViewportClient, View, Canvas, ViewportType);
	}
}

/** Hook for game stats tool to render things in 3D viewport. */
void UUnrealEdEngine::GameStatsRender3D(const FSceneView* View,class FPrimitiveDrawInterface* PDI, ELevelViewportType ViewportType)
{
	check(GApp);
	if(GApp->GameStatsVisualizer)
	{
		GApp->GameStatsVisualizer->RenderStats3D(View, PDI, ViewportType);
	}
}

/** Hook for game stats tool to be informed about mouse movements (for tool tip) */
void UUnrealEdEngine::GameStatsMouseMove(FEditorLevelViewportClient* ViewportClient, INT X, INT Y)
{
	check(GApp);
	if(GApp->GameStatsVisualizer)
	{
		GApp->GameStatsVisualizer->MouseMoved(ViewportClient, X, Y);
	}
}

/** Hook for game to be informed about key input */
void UUnrealEdEngine::GameStatsInputKey(FEditorLevelViewportClient* ViewportClient, FName Key,EInputEvent Event)
{
	check(GApp);
	if(GApp->GameStatsVisualizer)
	{
		GApp->GameStatsVisualizer->InputKey(ViewportClient, Key, Event);
	}
}

void UUnrealEdEngine::SentinelStatRender(FEditorLevelViewportClient* ViewportClient, const FSceneView* View, FCanvas* Canvas)
{
	check(GApp);
	if(GApp->SentinelTool)
	{
		GApp->SentinelTool->RenderStats(ViewportClient, View, Canvas);
	}
}

void UUnrealEdEngine::SentinelStatRender3D(const FSceneView* View,class FPrimitiveDrawInterface* PDI)
{
	check(GApp);
	if(GApp->SentinelTool)
	{
		GApp->SentinelTool->RenderStats3D(View, PDI);
	}
}

void UUnrealEdEngine::SentinelMouseMove(FEditorLevelViewportClient* ViewportClient, INT X, INT Y)
{
	check(GApp);
	if(GApp->SentinelTool)
	{
		GApp->SentinelTool->MouseMoved(ViewportClient, X, Y);
	}
}

void UUnrealEdEngine::SentinelInputKey(FEditorLevelViewportClient* ViewportClient, FName Key,EInputEvent Event)
{
	check(GApp);
	if(GApp->SentinelTool)
	{
		GApp->SentinelTool->InputKey(ViewportClient, Key, Event);
	}
}

/**
 * Get the index of the provided sprite category
 *
 * @param	InSpriteCategory	Sprite category to get the index of
 *
 * @return	Index of the provided sprite category, if possible; INDEX_NONE otherwise
 */
INT UUnrealEdEngine::GetSpriteCategoryIndex( const FName& InSpriteCategory )
{
	// Find the sprite category in the unlocalized to index map, if possible
	const INT* CategoryIndexPtr = UnlocalizedCategoryToIndexMap.Find( InSpriteCategory );
	
	const INT CategoryIndex = CategoryIndexPtr ? *CategoryIndexPtr : INDEX_NONE;

	return CategoryIndex;
}


//////////////////////////////////////////////////////////////////////////
// UUnrealEdOptions
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_CLASS(UUnrealEdOptions);
IMPLEMENT_CLASS(UUnrealEdKeyBindings);

/**
 * Generates a mapping from commands to their parent sets for quick lookup.
 */
void UUnrealEdOptions::GenerateCommandMap()
{
	CommandMap.Empty();
	for(INT CmdIdx=0; CmdIdx<EditorCommands.Num(); CmdIdx++)
	{
		FEditorCommand &Cmd = EditorCommands(CmdIdx);

		CommandMap.Set(Cmd.CommandName, CmdIdx);
	}
}

/**
 * Attempts to execute a command bound to a hotkey.
 *
 * @param Key			Key name
 * @param bAltDown		Whether or not ALT is pressed.
 * @param bCtrlDown		Whether or not CONTROL is pressed.
 * @param bShiftDown	Whether or not SHIFT is pressed.
 * @param EditorSet		Set of bindings to search in.
 */
void UUnrealEdOptions::ExecuteBinding(FName Key, UBOOL bAltDown, UBOOL bCtrlDown, UBOOL bShiftDown, FName EditorSet)
{
	
	FString Cmd = GetExecCommand(Key, bAltDown, bCtrlDown, bShiftDown, EditorSet);
	if(Cmd.Len())
	{
		GUnrealEd->Exec(*Cmd);
	}
}

/**
 * Attempts to locate a command bound to a hotkey.
 *
 * @param Key			Key name
 * @param bAltDown		Whether or not ALT is pressed.
 * @param bCtrlDown		Whether or not CONTROL is pressed.
 * @param bShiftDown	Whether or not SHIFT is pressed.
 * @param EditorSet		Set of bindings to search in.
 */
FString UUnrealEdOptions::GetExecCommand(FName Key, UBOOL bAltDown, UBOOL bCtrlDown, UBOOL bShiftDown, FName EditorSet)
{
	TArray<FEditorKeyBinding> &KeyBindings = EditorKeyBindings->KeyBindings;
	FString Result;

	for(INT BindingIdx=0; BindingIdx<KeyBindings.Num(); BindingIdx++)
	{
		FEditorKeyBinding &Binding = KeyBindings(BindingIdx);
		INT* CommandIdx = CommandMap.Find(Binding.CommandName);

		if(CommandIdx && EditorCommands.IsValidIndex(*CommandIdx))
		{
			FEditorCommand &Cmd = EditorCommands(*CommandIdx);

			if(Cmd.Parent == EditorSet)
			{
				// See if this key binding matches the key combination passed in.
				if(bAltDown == Binding.bAltDown && bCtrlDown == Binding.bCtrlDown && bShiftDown == Binding.bShiftDown && Key == Binding.Key)
				{
					INT* EditorCommandIdx = CommandMap.Find(Binding.CommandName);

					if(EditorCommandIdx && EditorCommands.IsValidIndex(*EditorCommandIdx))
					{
						FEditorCommand &EditorCommand = EditorCommands(*EditorCommandIdx);
						Result = EditorCommand.ExecCommand;
					}
					break;
				}
			}
		}
	}

	return Result;
}

/**
 * Attempts to locate a command name bound to a hotkey.
 *
 * @param Key			Key name
 * @param bAltDown		Whether or not ALT is pressed.
 * @param bCtrlDown		Whether or not CONTROL is pressed.
 * @param bShiftDown	Whether or not SHIFT is pressed.
 * @param EditorSet		Set of bindings to search in.
 *
 * @return Name of the command if found, NAME_None otherwise.
 */
FName UUnrealEdOptions::GetCommand(FName Key, UBOOL bAltDown, UBOOL bCtrlDown, UBOOL bShiftDown, FName EditorSet)
{
	TArray<FEditorKeyBinding> &KeyBindings = EditorKeyBindings->KeyBindings;
	FName Result(NAME_None);

	for(INT BindingIdx=0; BindingIdx<KeyBindings.Num(); BindingIdx++)
	{
		FEditorKeyBinding &Binding = KeyBindings(BindingIdx);
		INT* CommandIdx = CommandMap.Find(Binding.CommandName);

		if(CommandIdx && EditorCommands.IsValidIndex(*CommandIdx))
		{
			FEditorCommand &Cmd = EditorCommands(*CommandIdx);

			if(Cmd.Parent == EditorSet)
			{
				// See if this key binding matches the key combination passed in.
				if(bAltDown == Binding.bAltDown && bCtrlDown == Binding.bCtrlDown && bShiftDown == Binding.bShiftDown && Key == Binding.Key)
				{
					INT* EditorCommandIdx = CommandMap.Find(Binding.CommandName);

					if(EditorCommandIdx && EditorCommands.IsValidIndex(*EditorCommandIdx))
					{
						FEditorCommand &EditorCommand = EditorCommands(*EditorCommandIdx);
						Result = EditorCommand.CommandName;
					}
					break;
				}
			}
		}
	}

	return Result;
}


/**
 * Checks to see if a key is bound yet.
 *
 * @param Key			Key name
 * @param bAltDown		Whether or not ALT is pressed.
 * @param bCtrlDown		Whether or not CONTROL is pressed.
 * @param bShiftDown	Whether or not SHIFT is pressed.
 * @return Returns whether or not the specified key event is already bound to a command or not.
 */
UBOOL UUnrealEdOptions::IsKeyBound(FName Key, UBOOL bAltDown, UBOOL bCtrlDown, UBOOL bShiftDown, FName EditorSet)
{
	UBOOL bResult = FALSE;

	TArray<FEditorKeyBinding> &KeyBindings = EditorKeyBindings->KeyBindings;
	for(INT BindingIdx=0; BindingIdx<KeyBindings.Num(); BindingIdx++)
	{
		FEditorKeyBinding &Binding = KeyBindings(BindingIdx);
		INT* CommandIdx = CommandMap.Find(Binding.CommandName);

		if(CommandIdx && EditorCommands.IsValidIndex(*CommandIdx))
		{
			FEditorCommand &Cmd = EditorCommands(*CommandIdx);

			if(Cmd.Parent == EditorSet)
			{
				if(bAltDown == Binding.bAltDown && bCtrlDown == Binding.bCtrlDown && bShiftDown == Binding.bShiftDown && Key == Binding.Key)
				{
					bResult = TRUE;
					break;
				}
			}
		}
	}

	return bResult;
}

/**
 * Binds a hotkey.
 *
 * @param Key			Key name
 * @param bAltDown		Whether or not ALT is pressed.
 * @param bCtrlDown		Whether or not CONTROL is pressed.
 * @param bShiftDown	Whether or not SHIFT is pressed.
 * @param Command	Command to bind to.
 */
void UUnrealEdOptions::BindKey(FName Key, UBOOL bAltDown, UBOOL bCtrlDown, UBOOL bShiftDown, FName Command)
{
	UBOOL bFoundKey = FALSE;

	INT* InCmdIdx = CommandMap.Find(Command);

	if(InCmdIdx && EditorCommands.IsValidIndex(*InCmdIdx))
	{
		FEditorCommand &InCmd = EditorCommands(*InCmdIdx);
		FName EditorSet = InCmd.Parent;
		TArray<FEditorKeyBinding> KeysToKeep;

		// Loop through all existing bindings and see if there is a key bound with the same parent, if so, replace it.
		TArray<FEditorKeyBinding> &KeyBindings = EditorKeyBindings->KeyBindings;
		for(INT BindingIdx=0; BindingIdx<KeyBindings.Num(); BindingIdx++)
		{
			UBOOL bKeepKey = TRUE;
			FEditorKeyBinding &Binding = KeyBindings(BindingIdx);
			INT* CommandIdx = CommandMap.Find(Binding.CommandName);

			if(CommandIdx && EditorCommands.IsValidIndex(*CommandIdx))
			{
				FEditorCommand &Cmd = EditorCommands(*CommandIdx);

				if(Cmd.Parent == EditorSet)
				{
					if(bAltDown == Binding.bAltDown && bCtrlDown == Binding.bCtrlDown && bShiftDown == Binding.bShiftDown && Binding.Key == Key)
					{
						bKeepKey = FALSE;
					}
					else if(Cmd.CommandName==Command)
					{
						bKeepKey = FALSE;
					}
				}
			}

			if(bKeepKey==FALSE)
			{
				KeyBindings.Remove(BindingIdx);
				BindingIdx--;
			}
		}

		// Make a new binding
		FEditorKeyBinding NewBinding;
		NewBinding.bAltDown = bAltDown;
		NewBinding.bCtrlDown = bCtrlDown;
		NewBinding.bShiftDown = bShiftDown;
		NewBinding.Key = Key;
		NewBinding.CommandName = Command;
		
		KeyBindings.AddItem(NewBinding);
	}
}


/**
 * Retreives a editor key binding for a specified command.
 *
 * @param Command		Command to retrieve a key binding for.
 *
 * @return A pointer to a keybinding if one exists, NULL otherwise.
 */
FEditorKeyBinding* UUnrealEdOptions::GetKeyBinding(FName Command)
{
	FEditorKeyBinding *Result = NULL;

	TArray<FEditorKeyBinding> &KeyBindings = EditorKeyBindings->KeyBindings;
	for(INT BindingIdx=0; BindingIdx<KeyBindings.Num(); BindingIdx++)
	{
		FEditorKeyBinding &Binding = KeyBindings(BindingIdx);

		if(Binding.CommandName == Command)
		{
			Result = &Binding;
			break;
		}
	}

	return Result;
}



/**
 * Creates an embedded Play In Editor viewport window (if possible)
 *
 * @param ViewportClient The viewport client the new viewport will be associated with
 * @param InPlayInViewportIndex Viewport index to play in, or -1 for "don't care"
 *
 * @return TRUE if successful
 */
UBOOL UUnrealEdEngine::CreateEmbeddedPIEViewport( UGameViewportClient* ViewportClient, INT InPlayInViewportIndex )
{
	// NOTE: Overridden from parent class

	// Select an level editor viewport window to use for Play-In-Editor
	FVCD_Viewport* TargetViewport = NULL;
	{
		// If we were supplied a specific viewport index, try that first
		if( InPlayInViewportIndex != -1 )
		{
			const UINT NumViewports = GApp->EditorFrame->ViewportConfigData->GetViewportCount();
			if( InPlayInViewportIndex >= 0 && ( UINT )InPlayInViewportIndex < NumViewports )
			{
				TargetViewport = &GApp->EditorFrame->ViewportConfigData->AccessViewport( InPlayInViewportIndex );
			}
		}

		// If there was no specific viewport index and if we have an active level editor viewport, prefer that
		if( TargetViewport == NULL && GCurrentLevelEditingViewportClient != NULL )
		{
			const UINT NumViewports = GApp->EditorFrame->ViewportConfigData->GetViewportCount();
			for( UINT CurViewportIndex = 0; CurViewportIndex < NumViewports; ++CurViewportIndex )
			{
				FVCD_Viewport* CurrentViewport = &GApp->EditorFrame->ViewportConfigData->AccessViewport( CurViewportIndex );
				if( CurrentViewport->ViewportWindow == GCurrentLevelEditingViewportClient )
				{
					// Use the currently active level editor viewport
					TargetViewport = CurrentViewport;
					break;
				}
			}
		}

		// Still haven't found anything?  We'll look for any perspective viewports
		if( TargetViewport == NULL )
		{
			// Use a perspective viewport if we still haven't found anything
			if( GCurrentLevelEditingViewportClient != NULL )
			{
				const UINT NumViewports = GApp->EditorFrame->ViewportConfigData->GetViewportCount();
				for( UINT CurViewportIndex = 0; CurViewportIndex < NumViewports; ++CurViewportIndex )
				{
					FVCD_Viewport* CurrentViewport = &GApp->EditorFrame->ViewportConfigData->AccessViewport( CurViewportIndex );
					if( CurrentViewport->bEnabled && CurrentViewport->ViewportType == LVT_Perspective )
					{
						// Use a perspective viewport
						TargetViewport = CurrentViewport;
						break;
					}
				}
			}
		}
	}
		
	if( TargetViewport != NULL )
	{
		WxPIEContainerWindow* PIEContainerWindow =
			WxPIEContainerWindow::CreatePIEWindowAndPossessViewport( ViewportClient, TargetViewport );
		if( PIEContainerWindow != NULL )
		{
			// Success!
			return TRUE;
		}
	}

	return FALSE;
}


/**
 * Does the update for volume actor visibility
 *
 * @param ActorsToUpdate	The list of actors to update
 * @param ViewClient		The viewport client to apply visibility changes to
 */
static void InternalUpdateVolumeActorVisibility( TArray<AActor*>& ActorsToUpdate, const FEditorLevelViewportClient& ViewClient, TArray<AActor*>& OutActorsThatChanged )
{
	for( INT ActorIdx = 0; ActorIdx < ActorsToUpdate.Num(); ++ActorIdx )
	{
		AActor* ActorToUpdate = ActorsToUpdate(ActorIdx);

		const UBOOL bIsVisible = ViewClient.IsVolumeVisibleInViewport( *ActorToUpdate );

		QWORD OriginalViews = ActorToUpdate->HiddenEditorViews;
		if( bIsVisible )
		{
			// If the actor should be visible, unset the bit for the actor in this viewport
			ActorToUpdate->HiddenEditorViews &= ~((QWORD)1<<ViewClient.ViewIndex);	
		}
		else
		{
			if( ActorToUpdate->IsSelected() )
			{
				// We are hiding the actor, make sure its not selected anymore
				GEditor->SelectActor( ActorToUpdate, FALSE, NULL, TRUE  );
			}

			// If the actor should be hidden, set the bit for the actor in this viewport
			ActorToUpdate->HiddenEditorViews |= ((QWORD)1<<ViewClient.ViewIndex);	
		}

		if( OriginalViews != ActorToUpdate->HiddenEditorViews )
		{
			// At least one actor has visibility changes
			OutActorsThatChanged.AddUniqueItem( ActorToUpdate );
		}
	}
}

/**
 * Updates the volume actor visibility for all viewports based on the passed in volume class
 * 
 * @param InVolumeActorClass	The type of volume actors to update.  If NULL is passed in all volume actor types are updated.
 * @param InViewport			The viewport where actor visibility should apply.  Pass NULL for all editor viewports.
 */
void UUnrealEdEngine::UpdateVolumeActorVisibility( const UClass* InVolumeActorClass, FEditorLevelViewportClient* InViewport )
{
	const UClass* VolumeClassToCheck = InVolumeActorClass ? InVolumeActorClass : AVolume::StaticClass();
	
	// Build a list of actors that need to be updated.  Only take actors of the passed in volume class.  
	TArray< AActor *> ActorsToUpdate;
	for( FActorIterator It; It; ++It)
	{
		AActor* Actor = *It;

		if( Actor->IsA( VolumeClassToCheck ) )
		{
			ActorsToUpdate.AddItem(Actor);
		}
	}

	if( ActorsToUpdate.Num() > 0 )
	{
		TArray< AActor* > ActorsThatChanged;
		if( !InViewport )
		{
			// Update the visibility state of each actor for each viewport
			for( INT ViewportIdx = 0; ViewportIdx < ViewportClients.Num(); ++ViewportIdx )
			{
				FEditorLevelViewportClient& ViewClient = *ViewportClients(ViewportIdx);

				if (ViewClient.bEditorFrameClient)
				{
					// Only update the editor frame clients as those are the only viewports right now that show volumes.
					InternalUpdateVolumeActorVisibility( ActorsToUpdate, ViewClient, ActorsThatChanged );
					if( ActorsThatChanged.Num() )
					{
						// If actor visibility changed in the viewport, it needs to be redrawn
						ViewClient.Invalidate();
					}
				}
			}
		}
		else
		{
			if ( InViewport->bEditorFrameClient )
			{
				// Only update the editor frame clients as those are the only viewports right now that show volumes.
				InternalUpdateVolumeActorVisibility( ActorsToUpdate, *InViewport, ActorsThatChanged );
				if( ActorsThatChanged.Num() )
				{	
					// If actor visibility changed in the viewport, it needs to be redrawn
					InViewport->Invalidate();
				}
			}
		}

		// Push all changes in the actors to the scene proxy so the render thread correctly updates visibility
		for( INT ActorIdx = 0; ActorIdx < ActorsThatChanged.Num(); ++ActorIdx )
		{
			AActor* ActorToUpdate = ActorsThatChanged( ActorIdx );

			// Find all attached primitive components and update the scene proxy with the actors updated visibility map
			for( INT ComponentIdx = 0; ComponentIdx < ActorToUpdate->Components.Num(); ++ComponentIdx )
			{
				UActorComponent* Component = ActorToUpdate->Components(ComponentIdx);
				if (Component && Component->IsAttached())
				{
					UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
					if (PrimitiveComponent)
					{
						// Push visibility to the render thread
						PrimitiveComponent->PushEditorVisibilityToProxy( ActorToUpdate->HiddenEditorViews );
					}
				}
			}
		}
	}
}

/** 
* Brings up a dialog, allowing a user to edit the play world url
*
* @param ConsoleIndex			A valid console index or -1 if editing for an play in editor session.  This index sets up the proper map name to display in the url edit dialog.
* @param bUseMobilePreview		True to use mobile preview mode (only supported on PC)
*/
void UUnrealEdEngine::EditPlayWorldURL( INT ConsoleIndex, const UBOOL bUseMobilePreview )
{
	// Get the previously edited url. This will be the starting url
	FString StartURLString = GEditor->UserEditedPlayWorldURL;

	FString Prefix = PLAYWORLD_PACKAGE_PREFIX;

	//Whether or not this platform allows different resolution previews
	UBOOL bAdjustableResolution = FALSE;

	if ( ConsoleIndex != -1 )
	{
		// If a valid console index was passed, get the name of the console
		FConsoleSupport* Console = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport(ConsoleIndex);
		// make a per-platform name for the map.
		FString ConsoleName = FString(Console->GetPlatformName());
		// Make sure the console name is the same as the one generated when starting a play world
		if( ConsoleName == CONSOLESUPPORT_NAME_360 )
		{
			ConsoleName = FString(CONSOLESUPPORT_NAME_360_SHORT);
		}
		else if( ConsoleName == CONSOLESUPPORT_NAME_IPHONE )
		{
			ConsoleName = TEXT( "IOS" );
		}
		else if (ConsoleName == CONSOLESUPPORT_NAME_PC)
		{
			bAdjustableResolution = TRUE;
		}
		
		Prefix= FString(PLAYWORLD_CONSOLE_BASE_PACKAGE_PREFIX) + ConsoleName;

	}
	
	// Make sure the play world map name corresponds to the same map name that will be saved when starting a play world.
	// The map name will be different based on what toolbar button the user pressed
	const FString MapName = Prefix + GWorld->GetOutermost()->GetName();

	if( StartURLString.Len() == 0 )
	{
		// If there was no starting URL string, build one from scratch
		StartURLString = BuildPlayWorldURL(*MapName);
	}

	FURL StartURL(NULL, *StartURLString, TRAVEL_Absolute );
	if( StartURL.Map != MapName )
	{
		// Ensure the current URL map name is the same as the map name for the play world
		StartURL.Map = MapName;
	}

	// Initialize the play world url dialog
	WxDlgEditPlayWorldURL Dlg( StartURL.String(), bUseMobilePreview, bAdjustableResolution);

	// Show the dialog
	INT Response = Dlg.ShowModal();
	if( Response == WxDlgEditPlayWorldURL::Option_PlayWithURL || Response == WxDlgEditPlayWorldURL::Option_SaveURL )
	{
		// If the user did not press cancel, save the url string.
		FString URLString = Dlg.GetURL();

		// Make sure the map was not changed.  The map name has to be the same as the loaded map
		FURL EditedURL(NULL, *URLString, TRAVEL_Absolute);
		if( EditedURL.Map != MapName )
		{
			// Display a warning to the user if the map name was changed
			appMsgf( AMT_OK, *LocalizeUnrealEd( TEXT("EditPlayWorldURL_MapNameError") ), *LocalizeUnrealEd( TEXT("EditPlayWorldURL_MapNameErrorTitle") ) );
			// Fix the map name
			EditedURL.Map = MapName;
		}

		// Save the url string
		UserEditedPlayWorldURL = EditedURL.String();

		if( Response == WxDlgEditPlayWorldURL::Option_PlayWithURL)
		{
			// If the user pressed play with url, start a play world session.
			PlayMap( NULL, NULL, ConsoleIndex, INDEX_NONE, bUseMobilePreview );
		}
	}

}

/**
 * Displays a prompt to save and check in or just mark as dirty any material packages with dependent changes (either parent materials or
 * material functions) as determined on load.
 *
 */
void UUnrealEdEngine::PromptToSaveChangedDependentMaterialPackages()
{
	if( MaterialPackagesWithDependentChanges.Num() > 0)
	{
		for(TSet<UPackage*>::TIterator PackageIter(MaterialPackagesWithDependentChanges); PackageIter; ++PackageIter)
		{
			debugf(TEXT("Dependent material package changed named %s"), *(*PackageIter)->GetName());
		}

		TArray<UPackage*> PackagesToCheckout = MaterialPackagesWithDependentChanges.Array();
		TArray<UPackage*> PackagesNotNeedingCheckout;

		FEditorFileUtils::PromptToCheckoutPackages(FALSE, PackagesToCheckout, &PackagesNotNeedingCheckout, FALSE, TRUE);

		MaterialPackagesWithDependentChanges.Empty();
	}
}
