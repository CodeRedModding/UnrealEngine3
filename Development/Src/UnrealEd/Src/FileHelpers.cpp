/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "FileHelpers.h"
#include "LevelUtils.h"
#include "UnLinkedObjEditor.h"
#include "Kismet.h"
#include "Sentinel.h"
#include "BusyCursor.h"
#include "Database.h"
#include "GameStatsBrowser.h"
#include "CrossLevelReferences.h"
#include "SourceControl.h"
#include "DlgCheckBoxList.h"
#include "MRUFavoritesList.h"
#include "EditorBuildUtils.h"
#include "UnPackageTools.h"
#if WITH_MANAGED_CODE
#include "NewMapShared.h"
#include "NewProjectShared.h"
#endif	//#if WITH_MANAGED_CODE

//definition of flag used to do special work when we're attempting to load the "startup map"
UBOOL FEditorFileUtils::bIsLoadingSimpleStartupMap = FALSE;


/**
 * Queries the user if they want to quit out of interpolation editing before save.
 *
 * @return		TRUE if in interpolation editing mode, FALSE otherwise.
 */
static UBOOL InInterpEditMode()
{
	// Must exit Interpolation Editing mode before you can save - so it can reset everything to its initial state.
	if( GEditorModeTools().IsModeActive( EM_InterpEdit ) )
	{
		const UBOOL ExitInterp = appMsgf( AMT_YesNo, *LocalizeUnrealEd("Prompt_21") );
		if(!ExitInterp)
		{
			return TRUE;
		}

		GEditorModeTools().DeactivateMode( EM_InterpEdit );
	}
	return FALSE;
}

/**
 * Maps loaded level packages to the package filenames.
 */
static TMap<FName, FFilename> LevelFilenames;

static void RegisterLevelFilename(UObject* Object, const FFilename& NewLevelFilename)
{
	const FName PackageName(*Object->GetOutermost()->GetName());
	//debugf(TEXT("RegisterLevelFilename: package %s to name %s"), *PackageName, *NewLevelFilename );
	FFilename* ExistingFilenamePtr = LevelFilenames.Find( PackageName );
	if ( ExistingFilenamePtr )
	{
		// Update the existing entry with the new filename.
		*ExistingFilenamePtr = NewLevelFilename;
	}
	else
	{
		// Set for the first time.
		LevelFilenames.Set( PackageName, NewLevelFilename );
	}

	// Mirror the world's filename to UnrealEd's title bar.
	if ( Object == GWorld )
	{
		GApp->EditorFrame->RefreshCaption( &NewLevelFilename );
	}
}

///////////////////////////////////////////////////////////////////////////////

static FFilename GetFilename(const FName& PackageName)
{
	FFilename* Result = LevelFilenames.Find( PackageName );
	if ( !Result )
	{
		//debugf(TEXT("GetFilename with package %s, returning EMPTY"), *PackageName );
		return FFilename(TEXT(""));
	}
	// Verify that the file still exists, if it does not, reset the level filename
	else if ( GFileManager->FileSize( **Result ) == INDEX_NONE )
	{
		*Result = FFilename(TEXT(""));
		if ( GWorld && GWorld->GetOutermost()->GetFName() == PackageName )
		{
			GApp->EditorFrame->RefreshCaption( Result );
		}
	}

	//debugf(TEXT("GetFilename with package %s, returning %s"), *PackageName, **Result );
	return *Result;
}

static FFilename GetFilename(UObject* LevelObject)
{
	return GetFilename( LevelObject->GetOutermost()->GetFName() );
}

///////////////////////////////////////////////////////////////////////////////

static const FString& GetDefaultDirectory()
{
	return GApp->LastDir[LD_UNR];
}

///////////////////////////////////////////////////////////////////////////////

/**
* Returns a file filter string appropriate for a specific file interaction.
*
* @param	Interaction		A file interaction to get a filter string for.
* @return					A filter string.
*/
FString FEditorFileUtils::GetFilterString(EFileInteraction Interaction)
{
	FString Result;

	switch( Interaction )
	{
	case FI_Load:
	case FI_Save:
		{
			FString MapExtensions;

			UINT Count = 0;
			// For every supported map extension, make an entry for it in the file dialog.
			for( TSet<FString>::TConstIterator It(GApp->SupportedMapExtensions); It; ++It )
			{
				if( Count > 0 )
				{
					MapExtensions +=";";
				}

				MapExtensions += FString::Printf( TEXT("*.%s"), **It );
				++Count;
			}

			Result = FString::Printf( TEXT("Map files (%s)|%s|All files (*.*)|*.*"), *MapExtensions, *MapExtensions );

		
		}
		break;
	case FI_Import:
		Result = TEXT("Unreal Text (*.t3d)|*.t3d|All Files|*.*");
		break;

	case FI_Export:
		Result = TEXT("Object (*.obj)|*.obj|Unreal Text (*.t3d)|*.t3d|Stereo Litho (*.stl)|*.stl|LOD Export (*.lod.obj)|*.lod.obj|FBX (*.fbx)|*.fbx|All Files|*.*");
		break;

	default:
		checkMsg( 0, "Unkown EFileInteraction" );
	}

	return Result;
}

///////////////////////////////////////////////////////////////////////////////

/**
 * Writes current viewport views into the specified world in preparation for saving.
 */
static void SaveViewportViews(UWorld* World)
{
	// @todo: Support saving and restoring floating viewports with level?

	// Check that this world is GWorld to avoid stomping on the saved views of sub-levels.
	if ( World == GWorld )
	{
		for(INT i=0; i<4; i++)
		{
			WxLevelViewportWindow* VC = GApp->EditorFrame->ViewportConfigData->Viewports[i].ViewportWindow;
			if(VC && (VC->ViewportType != LVT_None))
			{
				check(VC->ViewportType >= 0 && VC->ViewportType < 4);
				World->EditorViews[VC->ViewportType] = FLevelViewportInfo( VC->ViewLocation, VC->ViewRotation, VC->OrthoZoom );	
			}
		}
	}
}

/**
 * Reads views from the specified world into the current views.
 */
static void RestoreViewportViews(UWorld* World)
{
	// @todo: Support saving and restoring floating viewports with level?

	// Check that this world is GWorld to avoid stomping on the saved views of sub-levels.
	if ( World == GWorld )
	{
		for( INT i=0 ; i<4; i++)
		{
			WxLevelViewportWindow* VC = GApp->EditorFrame->ViewportConfigData->Viewports[i].ViewportWindow;
			if( VC && (VC->ViewportType != LVT_None) )
			{
				check(VC->ViewportType >= 0 && VC->ViewportType < 4);
				VC->ViewLocation	= World->EditorViews[VC->ViewportType].CamPosition;
				VC->ViewRotation	= World->EditorViews[VC->ViewportType].CamRotation;
				VC->OrthoZoom		= World->EditorViews[VC->ViewportType].CamOrthoZoom;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

/**
 * Presents the user with a file dialog whose contents are intially InFilename.
 *
 * @return		TRUE if OK was clicked and OutFilename contains a result, FALSE otherwise.
 */
static UBOOL PresentFileDialog(const FFilename& InFilename, FFilename& OutFilename)
{
	WxFileDialog dlg( GApp->EditorFrame,
					  *LocalizeUnrealEd("SaveAs"),
					  *GetDefaultDirectory(),
					  *InFilename.GetCleanFilename(),
					  *FEditorFileUtils::GetFilterString(FI_Save),
					  wxSAVE | wxOVERWRITE_PROMPT );

	// Disable autosaving while the "Save As..." dialog is up.
	const UBOOL bOldAutoSaveState = GEditor->GetUserSettings().bAutoSaveEnable;
	GEditor->AccessUserSettings().bAutoSaveEnable = FALSE;

	const UBOOL bOK = (dlg.ShowModal() == wxID_OK);

	// Restore autosaving to its previous state.
	GEditor->AccessUserSettings().bAutoSaveEnable = bOldAutoSaveState;

	if( bOK )
	{
		OutFilename = FFilename( dlg.GetPath() );
	}

	return bOK;
}

/**
 * @param	World					The world to save.
 * @param	ForceFilename			If non-NULL, save the level package to this name (full path+filename).
 * @param	OverridePath			If non-NULL, override the level path with this path.
 * @param	FilenamePrefix			If non-NULL, prepend this string the the level filename.
 * @param	bCollectGarbage			If TRUE, request garbage collection before saving.
 * @param	bRenamePackageToFile	If TRUE, rename the level package to the filename if save was successful.
 * @param	bCheckDirty				If TRUE, don't save the level if it is not dirty.
 * @param	FinalFilename			[out] The full path+filename the level was saved to.
 * @param	bAutosaving				Should be set to TRUE if autosaving; passed to UWorld::SaveWorld.
 * @param	bPIESaving				Should be set to TRUE if saving for PIE; passed to UWorld::SaveWorld.
 * @return							TRUE if the level was saved.
 */
static UBOOL SaveWorld(UWorld* World,
					   const FFilename* ForceFilename,
					   const TCHAR* OverridePath,
					   const TCHAR* FilenamePrefix,
					   UBOOL bCollectGarbage,
					   UBOOL bRenamePackageToFile,
					   UBOOL bCheckDirty,
					   FString& FinalFilename,
					   UBOOL bAutosaving,
					   UBOOL bPIESaving)
{
	if ( !World )
	{
		return FALSE;
	}

	UPackage* Package = Cast<UPackage>( World->GetOuter() );
	if ( !Package )
	{
		return FALSE;
	}

	// Don't save if the world doesn't need saving.
	if ( bCheckDirty && !Package->IsDirty() )
	{
		return FALSE;
	}

	// make sure any textures in this package are cooked and cached for iPhone if needed
	// unless it's an auto or PIE save - these won't be used on iPhone
	if (!bAutosaving && !bPIESaving)
	{
		::PreparePackageForMobile(Package, FALSE, World);
	}

	FString PackageName = Package->GetName();

	FFilename	ExistingFilename;
	FString		Path;
	FFilename	CleanFilename;

	// Does a filename already exist for this package?
	const UBOOL bPackageExists = GPackageFileCache->FindPackageFile( *PackageName, NULL, ExistingFilename );

	if ( ForceFilename )
	{
		Path				= ForceFilename->GetPath();
		CleanFilename		= ForceFilename->GetCleanFilename();
	}
	else if ( bPackageExists )
	{
		if( bPIESaving && appStristr( *ExistingFilename, *( FString( TEXT( "." ) ) + FURL::DefaultMapExt ) ) == NULL )
		{
			// If package exists, but doesn't feature the default extension, it will not load when launched,
			// Change the extension of the map to the default for the auto-save
			Path			= GEditor->AutoSaveDir;
			CleanFilename	= PackageName + TEXT(".") + FURL::DefaultMapExt;
		}
		else
		{
			// We're not forcing a filename, so go with the filename that exists.
			Path			= ExistingFilename.GetPath();
			CleanFilename	= ExistingFilename.GetCleanFilename();
		}
	}
	else
	{
		// No package filename exists and none was specified, so save the package in the autosaves folder.
		Path			= GEditor->AutoSaveDir;
		CleanFilename	= PackageName + TEXT(".") + FURL::DefaultMapExt;
	}

	// Optionally override path.
	if ( OverridePath )
	{
		FinalFilename = FString(OverridePath) + PATH_SEPARATOR;
	}
	else
	{
		FinalFilename = Path + PATH_SEPARATOR;
	}

	// Apply optional filename prefix.
	if ( FilenamePrefix )
	{
		FinalFilename += FString(FilenamePrefix);
	}

	// Munge remaining clean filename minus path + extension with path and optional prefix.
	FinalFilename += CleanFilename;

	// Before doing any work, check to see if 1) the package name is in use by another object; and 2) the file is writable.
	UBOOL bSuccess = FALSE;

	if ( Package->Rename( *CleanFilename.GetBaseFilename(), NULL, REN_Test ) == FALSE )
	{
		appMsgf(AMT_OK,	*LocalizeUnrealEd("Error_PackageNameExists"));
	}
	else if(GFileManager->IsReadOnly(*FinalFilename) == FALSE)
	{
		// Save the world package after doing optional garbage collection.
		const FScopedBusyCursor BusyCursor;

		FString MapFileName = FFilename( FinalFilename ).GetCleanFilename();
		const FString LocalizedSavingMap(
			*FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT("SavingMap_F") ), *MapFileName ) ) );
		GWarn->BeginSlowTask( *LocalizedSavingMap, TRUE );

		bSuccess = World->SaveWorld( FinalFilename, bCollectGarbage, bAutosaving, bPIESaving );
		GWarn->EndSlowTask();

		if( bRenamePackageToFile )
		{
			// If the package was saved successfully, make sure the UPackage has the same name as the file.
			if ( bSuccess )
			{
				Package->Rename( *CleanFilename.GetBaseFilename(), NULL, REN_ForceNoResetLoaders );
			}
		}
	}
	else
	{
		appMsgf(AMT_OK,	LocalizeSecure(LocalizeUnrealEd("PackageFileIsReadOnly"), *FinalFilename));
	}

	return bSuccess;
}


/** Renames a single level, preserving the common suffix */
UBOOL RenameStreamingLevel( FString& LevelToRename, const FString& OldBaseLevelName, const FString& NewBaseLevelName )
{
	// Make sure the level starts with the original level name
	if( LevelToRename.StartsWith( OldBaseLevelName ) )	// Not case sensitive
	{
		// Grab the tail of the streaming level name, basically everything after the old base level name
		FString SuffixToPreserve = LevelToRename.Right( LevelToRename.Len() - OldBaseLevelName.Len() );

		// Rename the level!
		LevelToRename = NewBaseLevelName + SuffixToPreserve;

		return TRUE;
	}

	return FALSE;
}



/**
 * Renames streaming level names in all sequence objects and sub-sequence objects
 */
UBOOL RenameStreamingLevelsInSequenceRecursively( USequence* Sequence, const FString& OldBaseLevelName, const FString& NewBaseLevelName, const UBOOL bDoReplace )
{
	UBOOL bFoundAnythingToRename = FALSE;

	if( Sequence != NULL )
	{
		// Iterate over Objects in this Sequence
		for ( INT OpIndex = 0; OpIndex < Sequence->SequenceObjects.Num(); OpIndex++ )
		{
			USequenceOp* SeqOp = Cast<USequenceOp>( Sequence->SequenceObjects(OpIndex) );
			if ( SeqOp != NULL )
			{
				// If this is a subsequence, then recurse
				USequence* SubSeq = Cast<USequence>( SeqOp );
				if( SubSeq != NULL )
				{
					if( RenameStreamingLevelsInSequenceRecursively( SubSeq, OldBaseLevelName, NewBaseLevelName, bDoReplace ) )
					{
						bFoundAnythingToRename = TRUE;
					}
				}
				else
				{
					// Process USeqAct_LevelStreaming nodes
					USeqAct_LevelStreaming* LevelStreamingSeqAct = Cast< USeqAct_LevelStreaming >( SeqOp );
					if( LevelStreamingSeqAct != NULL )
					{
						FString LevelNameToRename = LevelStreamingSeqAct->LevelName.ToString();
						if( RenameStreamingLevel( LevelNameToRename, OldBaseLevelName, NewBaseLevelName ) )
						{
							bFoundAnythingToRename = TRUE;
							if( bDoReplace )
							{
								// Level was renamed!
								LevelStreamingSeqAct->LevelName = FName( *LevelNameToRename );
								LevelStreamingSeqAct->MarkPackageDirty();
							}
						}
					}

					// Process USeqAct_LevelVisibility nodes
					USeqAct_LevelVisibility* LevelVisibilitySeqAct = Cast< USeqAct_LevelVisibility >( SeqOp );
					if( LevelVisibilitySeqAct != NULL )
					{
						FString LevelNameToRename = LevelVisibilitySeqAct->LevelName.ToString();
						if( RenameStreamingLevel( LevelNameToRename, OldBaseLevelName, NewBaseLevelName ) )
						{
							bFoundAnythingToRename = TRUE;
							if( bDoReplace )
							{
								// Level was renamed!
								LevelVisibilitySeqAct->LevelName = FName( *LevelNameToRename );
								LevelVisibilitySeqAct->MarkPackageDirty();
							}
						}
					}

					// Process USeqAct_MultiLevelStreaming nodes
					USeqAct_MultiLevelStreaming* MultiLevelStreamingSeqAct = Cast< USeqAct_MultiLevelStreaming >( SeqOp );
					if( MultiLevelStreamingSeqAct != NULL )
					{
						for( INT CurLevelIndex = 0; CurLevelIndex < MultiLevelStreamingSeqAct->Levels.Num(); ++CurLevelIndex )
						{
							FString LevelNameToRename = MultiLevelStreamingSeqAct->Levels( CurLevelIndex ).LevelName.ToString();
							if( RenameStreamingLevel( LevelNameToRename, OldBaseLevelName, NewBaseLevelName ) )
							{
								bFoundAnythingToRename = TRUE;
								if( bDoReplace )
								{
									// Level was renamed!
									MultiLevelStreamingSeqAct->Levels( CurLevelIndex ).LevelName = FName( *LevelNameToRename );
									MultiLevelStreamingSeqAct->MarkPackageDirty();
								}
							}
						}
					}
				}
			}
		}
	}

	return bFoundAnythingToRename;
}


/**
 * Prompts the user with a dialog for selecting a filename.
 */
static UBOOL SaveAsImplementation( const FFilename& DefaultFilename, const UBOOL bAllowStreamingLevelRename )
{
	WxFileDialog dlg( GApp->EditorFrame,
					  *LocalizeUnrealEd("SaveAs"),
					  *GetDefaultDirectory(),
					  *DefaultFilename.GetCleanFilename(),
					  *FEditorFileUtils::GetFilterString(FI_Save),
					  wxSAVE | wxOVERWRITE_PROMPT );

	// Disable autosaving while the "Save As..." dialog is up.
	const UBOOL bOldAutoSaveState = GEditor->GetUserSettings().bAutoSaveEnable;
	GEditor->AccessUserSettings().bAutoSaveEnable = FALSE;

	// "Save As" will always update the editor's MRU level list
	const UBOOL bAddToMRUList = TRUE;

	UBOOL bStatus = FALSE;

	// Loop through until a valid filename is given or the user presses cancel
	UBOOL bFilenameIsValid = FALSE;
	while( !bFilenameIsValid )
	{
		if( dlg.ShowModal() == wxID_OK )
		{
			const FString FileToCheck = dlg.GetPath().c_str();
			FString ErrorMessage;
			if( !FEditorFileUtils::IsFilenameValidForSaving( FileToCheck, ErrorMessage ) )
			{
				FFilename Filename = FileToCheck;
				appMsgf( AMT_OK, *ErrorMessage );
				// Start the loop over, prompting for save again
				continue;
			}
			else
			{
				// Filename is valid, do not ask to save again.
				bFilenameIsValid = TRUE;
			}

			// Check to see if there are streaming level associated with the P map, and if so, we'll
			// prompt to rename those and fixup all of the named-references to levels in the maps.
			UBOOL bCanRenameStreamingLevels = FALSE;
			FString OldBaseLevelName, NewBaseLevelName;

			if( bAllowStreamingLevelRename )
			{
				const FString OldLevelName = DefaultFilename.GetBaseFilename();
				const FString NewLevelName = FFilename( dlg.GetPath() ).GetBaseFilename();

				// The old and new level names must have a common suffix.  We'll detect that now.
				INT NumSuffixChars = 0;
				{
					for( INT CharsFromEndIndex = 0; ; ++CharsFromEndIndex )
					{
						const INT OldLevelNameCharIndex = ( OldLevelName.Len() - 1 ) - CharsFromEndIndex;
						const INT NewLevelNameCharIndex = ( NewLevelName.Len() - 1 ) - CharsFromEndIndex;

						if( OldLevelNameCharIndex <= 0 || NewLevelNameCharIndex <= 0 )
						{
							// We've processed all characters in at least one of the strings!
							break;
						}

						if( appToUpper( OldLevelName[ OldLevelNameCharIndex ] ) != appToUpper( NewLevelName[ NewLevelNameCharIndex ] ) )
						{
							// Characters don't match.  We have the common suffix now.
							break;
						}

						// We have another common character in the suffix!
						++NumSuffixChars;
					}

				}


				// We can only proceed if we found a common suffix
				if( NumSuffixChars > 0 )
				{
					FString CommonSuffix = NewLevelName.Right( NumSuffixChars );

					OldBaseLevelName = OldLevelName.Left( OldLevelName.Len() - CommonSuffix.Len() );
					NewBaseLevelName = NewLevelName.Left( NewLevelName.Len() - CommonSuffix.Len() );


					// OK, make sure this is really the persistent level
					if( GWorld->CurrentLevel == GWorld->PersistentLevel )
					{
						// Check to see if we actually have anything to rename
						UBOOL bAnythingToRename = FALSE;
						{
							// Check for Kismet sequences to rename
							USequence* LevelKismet = GWorld->GetGameSequence( NULL ); // CurLevel );
							if( LevelKismet != NULL )
							{
								const UBOOL bDoReplace = FALSE;
								if( RenameStreamingLevelsInSequenceRecursively( LevelKismet, OldBaseLevelName, NewBaseLevelName, bDoReplace ) )
								{
									bAnythingToRename = TRUE;
								}
							}

							// Check for contained streaming levels
							AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
							if( WorldInfo != NULL )
							{
								for( INT CurStreamingLevelIndex = 0; CurStreamingLevelIndex < WorldInfo->StreamingLevels.Num(); ++CurStreamingLevelIndex )
								{
									ULevelStreaming* CurStreamingLevel = WorldInfo->StreamingLevels( CurStreamingLevelIndex );
									if( CurStreamingLevel != NULL )
									{
										// Update the package name
										FString PackageNameToRename = CurStreamingLevel->PackageName.ToString();
										if( RenameStreamingLevel( PackageNameToRename, OldBaseLevelName, NewBaseLevelName ) )
										{
											bAnythingToRename = TRUE;
										}
									}
								}
							}
						}

						if( bAnythingToRename )
						{
							// OK, we can go ahead and rename levels
							bCanRenameStreamingLevels = TRUE;
						}
					}
				}
			}


			if( bCanRenameStreamingLevels )
			{
				// Prompt to update streaming levels and such
				// Return value:  0 = yes, 1 = no, 2 = cancel
				const INT DlgResult =
					appMsgf( AMT_YesNoCancel,
					*FString::Printf( LocalizeSecure( LocalizeUnrealEd( "SaveLevelAs_PromptToRenameStreamingLevels_F" ), *DefaultFilename.GetBaseFilename(), *FFilename( dlg.GetPath() ).GetBaseFilename() ) ) );

				if( DlgResult != 2 )	// Cancel?
				{
					if( DlgResult == 0 )	// Yes?
					{
						// Rename all named level references in Kismet
						USequence* LevelKismet = GWorld->GetGameSequence( NULL ); // CurLevel );
						if( LevelKismet != NULL )
						{
							const UBOOL bDoReplace = TRUE;
							if( RenameStreamingLevelsInSequenceRecursively( LevelKismet, OldBaseLevelName, NewBaseLevelName, bDoReplace ) )
							{
								// We renamed something!
							}
						}

						// Update streaming level names
						AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
						if( WorldInfo != NULL )
						{
							for( INT CurStreamingLevelIndex = 0; CurStreamingLevelIndex < WorldInfo->StreamingLevels.Num(); ++CurStreamingLevelIndex )
							{
								ULevelStreaming* CurStreamingLevel = WorldInfo->StreamingLevels( CurStreamingLevelIndex );
								if( CurStreamingLevel != NULL )
								{
									// Update the package name
									FString PackageNameToRename = CurStreamingLevel->PackageName.ToString();
									if( RenameStreamingLevel( PackageNameToRename, OldBaseLevelName, NewBaseLevelName ) )
									{
										CurStreamingLevel->PackageName = FName( *PackageNameToRename );

										// Level was renamed!
										CurStreamingLevel->MarkPackageDirty();
									}
								}
							}
						}
					}

					// Save the level!
					bStatus = FEditorFileUtils::SaveMap( GWorld, FString( dlg.GetPath() ), bAddToMRUList );
				}
				else
				{
					// User canceled, nothing to do.
				}
			}
			else
			{
				// Save the level
				bStatus = FEditorFileUtils::SaveMap( GWorld, FString( dlg.GetPath() ), bAddToMRUList );
			}

		}
		else
		{
			// User canceled the save dialog, do not prompt again.
			break;
		}

	}

	// Restore autosaving to its previous state.
	GEditor->AccessUserSettings().bAutoSaveEnable = bOldAutoSaveState;

	// Refresh the content browser so the newly saved map will show up in the content browsers package list
	GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_UpdatePackageList | CBR_UpdateSCCState  , NULL ) );

	return bStatus;

}

/**
 * Saves the file, checking first if we have a valid filename to save the file as.
 *
 * @param	LevelObject		The level object.
 * @param	Filename		The filename to use.
 * @return					TRUE if a valid filename exists, or FALSE otherwise.
 */
static UBOOL CheckSaveAs(UObject* LevelObject, const FString& Filename)
{
	if( !Filename.Len() )
	{
		const UBOOL bAllowStreamingLevelRename = FALSE;
		return SaveAsImplementation( Filename, bAllowStreamingLevelRename );
	}
	else
	{
		// No need to update MRU list when saving levels that are already known
		const UBOOL bAddToMRUList = FALSE;
		return FEditorFileUtils::SaveMap( GWorld, Filename, bAddToMRUList );
	}
}

/**
 * @return		TRUE if GWorld's package is dirty.
 */
static UBOOL IsWorldDirty()
{
	UPackage* Package = CastChecked<UPackage>(GWorld->GetOuter());
	return Package->IsDirty();
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FEditorFileUtils
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Does a saveAs for the specified level.
 *
 * @param	Level		The level to be SaveAs'd.
 * @return				TRUE if the level was saved.
 */
UBOOL FEditorFileUtils::SaveAs(UObject* LevelObject)
{
	const FFilename DefaultFilename = GetFilename( LevelObject );

	// We'll allow the map to be renamed when saving a level as a new file name this way
	const UBOOL bAllowStreamingLevelRename = TRUE;

	return SaveAsImplementation( DefaultFilename, bAllowStreamingLevelRename );
}

/**
 * Checks to see if GWorld's package is dirty and if so, asks the user if they want to save it.
 * If the user selects yes, does a save or SaveAs, as necessary.
 */	
UBOOL FEditorFileUtils::AskSaveChanges()
{
	UBOOL bYesToAllSelected = FALSE;

	if( IsWorldDirty() )
	{
		const FFilename LevelPackageFilename = GetFilename( GWorld );
		const FString Question = FString::Printf( LocalizeSecure(LocalizeUnrealEd("Prompt_20"), ( LevelPackageFilename.Len() ? *LevelPackageFilename.GetBaseFilename() : *LocalizeUnrealEd("Untitled") )) );
		
		INT result = appMsgf( AMT_YesNoYesAllNoAllCancel, *Question);
		if( result == ART_Yes || result == ART_YesAll )
		{
			if( CheckSaveAs( GWorld, LevelPackageFilename ) == FALSE )
			{
				// something went wrong or the user cancelled.
				return FALSE;
			}

			if( result == ART_YesAll )
			{
				// save all sublevels automatically
				bYesToAllSelected = TRUE;
			}
		}
		else if( result == ART_NoAll )
		{
			// early out, no point in iterating through all the worlds. The user doesn't want to save.
			return TRUE;
		}
		else if( result == ART_Cancel )
		{
			// the user wants to stay where they are
			return FALSE;
		}
	}

	// Update cull distance volumes (and associated primitives).
	GWorld->UpdateCullDistanceVolumes();

	// Clean up any old worlds.
	UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

	// Get the set of all reference worlds, excluding GWorld.
	TArray<UWorld*> WorldsArray;
	FLevelUtils::GetWorlds( WorldsArray, FALSE );

	// Calling FLevelUtils::GetWorlds forces a garbage collect, so we shouldn't need to collect garbage again
	UBOOL bAlreadyCollectedGarbage = TRUE;

	UBOOL bSuccess = TRUE;

	for ( INT WorldIndex = 0 ; WorldIndex < WorldsArray.Num() ; ++WorldIndex )
	{
		UWorld* World = WorldsArray( WorldIndex );
		UPackage* Package = Cast<UPackage>( World->GetOuter() );
		check( Package );

		// If this world needs saving and no to all hasn't been selected
		if ( Package->IsDirty())
		{
			FString FinalFilename;
			INT result;
		
			if( bYesToAllSelected == FALSE )
			{
				// ask to save if Yes To All hasn't been selected
				const FString Question = FString::Printf( LocalizeSecure(LocalizeUnrealEd("Prompt_20"), *Package->GetName()) );
				result = appMsgf( AMT_YesNoYesAllNoAllCancel, *Question );
			}
			else
			{
				// if yes to all has been selected save every package
				result = ART_Yes;
			}

			if( result ==  ART_Yes || result == ART_YesAll )
			{
				if( SaveWorld( World, NULL,
					NULL, NULL,
					!bAlreadyCollectedGarbage,
					TRUE, FALSE,
					 FinalFilename,
					FALSE, FALSE ) == FALSE)
				{
					//couldn't save the package, early out
					bSuccess = FALSE;
					break;
				}

				// Collect garbage if necessary.
				if ( !bAlreadyCollectedGarbage )
				{
					bAlreadyCollectedGarbage = TRUE;
				}

				if( result == ART_YesAll )
				{
					bYesToAllSelected = TRUE;
				}
			}
			else if( result == ART_NoAll )
			{
				// early out, the user doesn't want to save anything
				bSuccess = TRUE;
				break;
			}
			else if ( result == ART_Cancel )
			{
				// the user pressed cancel
				bSuccess = FALSE;
				break;
			}
		}
	}

	return bSuccess;
}

/**
 * Presents the user with a file dialog for importing.
 * If the import is not a merge (bMerging is FALSE), AskSaveChanges() is called first.
 *
 * @param	bMerge	If TRUE, merge the file into this map.  If FsALSE, merge the map into a blank map.
 */
void FEditorFileUtils::Import(UBOOL bMerge)
{
	// If we are importing into the existing map, we don't need to try to save the existing one, as it's not going away.
	if ( !bMerge )
	{
		// If there are any unsaved changes to the current level, see if the user wants to save those first.
		UBOOL bPromptUserToSave = TRUE;
		UBOOL bSaveMapPackages = TRUE;
		UBOOL bSaveContentPackages = FALSE;
		if( SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages) == FALSE )
		{
			// something went wrong or the user pressed cancel.  Return to the editor so the user doesn't lose their changes		
			return;
		}

		// toss all the cross level stuff, because we are making a new level and don't need to slow down the cleanup process
		GCrossLevelReferenceManager->Reset();
	}

	WxFileDialog dlg( GApp->EditorFrame, *LocalizeUnrealEd("Import"), *GetDefaultDirectory(), TEXT(""), *GetFilterString(FI_Import), wxOPEN | wxFILE_MUST_EXIST );
	if( dlg.ShowModal() == wxID_OK )
	{
		Import( FString( dlg.GetPath() ), bMerge );
	}
}

void FEditorFileUtils::Import(const FFilename& InFilename, UBOOL bMerge)
{
	const FScopedBusyCursor BusyCursor;

	FString MapFileName = InFilename.GetCleanFilename();
	const FString LocalizedImportingMap(
		*FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT("ImportingMap_F") ), *MapFileName ) ) );

	GWarn->BeginSlowTask( *LocalizedImportingMap, TRUE );

	if( bMerge )
	{
		GUnrealEd->Exec( *FString::Printf( TEXT("MAP IMPORTADD FILE=\"%s\""), *InFilename ) );
	}
	else
	{
		GUnrealEd->Exec( *FString::Printf( TEXT("MAP IMPORT FILE=\"%s\""), *InFilename ) );
		FEditorFileUtils::ResetLevelFilenames();
	}

	GWarn->EndSlowTask();

	GUnrealEd->RedrawLevelEditingViewports();

	GApp->LastDir[LD_UNR] = InFilename.GetPath();

	GCallbackEvent->Send(CALLBACK_RefreshEditor_AllBrowsers);
}

/**
 * Saves the specified level.  SaveAs is performed as necessary.
 *
 * @param	Level				The level to be saved.
 * @param	DefaultFilename		File name to use for this level if it doesn't have one yet (or empty string to prompt)
 *
 * @return				TRUE if the level was saved.
 */
UBOOL FEditorFileUtils::SaveLevel(ULevel* Level, const FFilename& DefaultFilename )
{
	UBOOL bLevelWasSaved = FALSE;

	// Update cull distance volumes (and associated primitives).
	GWorld->UpdateCullDistanceVolumes();

	// Disallow the save if in interpolation editing mode and the user doesn't want to exit interpolation mode.
	if ( Level && !InInterpEditMode() )
	{
		// Check and see if this is a new map.
		const UBOOL bIsPersistentLevelCurrent = (Level == GWorld->PersistentLevel);

		// If the user trying to save the persistent level?
		if ( bIsPersistentLevelCurrent )
		{
			// Check to see if the persistent level is a new map (ie if it has been saved before).
			FFilename Filename = GetFilename( GWorld );
			if( !Filename.Len() )
			{
				// No file name, provided, so use the default file name we were given if we have one
				Filename = FFilename( DefaultFilename );
			}

			if( !Filename.Len() )
			{
				// Present the user with a SaveAs dialog.
				const UBOOL bAllowStreamingLevelRename = FALSE;
				bLevelWasSaved = SaveAsImplementation( Filename, bAllowStreamingLevelRename );
				return bLevelWasSaved;
			}
		}

		////////////////////////////////
		// At this point, we know the level we're saving has been saved before,
		// so don't bother checking the filename.

		UWorld* WorldToSave = Cast<UWorld>( Level->GetOuter() );
		if ( WorldToSave )
		{
			// Only save the camera views for GWorld.  Otherwise, this would stomp on camera
			// views for levels saved on their own and then included in another world.
			if ( WorldToSave == GWorld )
			{
				SaveViewportViews( WorldToSave );
			}

			FString FinalFilename;
			bLevelWasSaved = SaveWorld( WorldToSave,
										DefaultFilename.Len() > 0 ? &DefaultFilename : NULL,
										NULL, NULL,
										TRUE, TRUE, FALSE,
										FinalFilename,
										FALSE, FALSE );

			if ( bLevelWasSaved )
			{
				// Refresh the level browser now that the level's dirty flag has been unset.
				GCallbackEvent->Send( CALLBACK_RefreshEditor_LevelBrowser );

				// We saved the map, so unless there are any other dirty levels, go ahead and reset the autosave timer
				if( !GUnrealEd->AnyWorldsAreDirty() )
				{
					GUnrealEd->ResetAutosaveTimer();
				}
			}
		}
	}

	return bLevelWasSaved;
}

void FEditorFileUtils::Export(UBOOL bExportSelectedActorsOnly)
{
	// Can't export from cooked packages.
	if ( GWorld->GetOutermost()->PackageFlags & PKG_Cooked )
	{
		return;
	}

	// @todo: extend this to multiple levels.
	const FFilename LevelFilename = GetFilename( GWorld );//->GetOutermost()->GetName() );
	WxFileDialog dlg( GApp->EditorFrame, *LocalizeUnrealEd("Export"), *GetDefaultDirectory(), *LevelFilename.GetBaseFilename(), *GetFilterString(FI_Export), wxSAVE | wxOVERWRITE_PROMPT );
	if( dlg.ShowModal() == wxID_OK )
	{
		const FFilename DlgFilename( dlg.GetPath() );
		GUnrealEd->ExportMap( *DlgFilename, bExportSelectedActorsOnly );
		GApp->LastDir[LD_UNR] = DlgFilename.GetPath();
	}
}

// Small class to help out the check box list dialog with things only the checkout dialog should know about
class WxCheckOutPackageDlg : public WxDlgCheckBoxList<UPackage*>
{
public:
	enum ECheckoutButtonID
	{
		CB_Checkout,
		CB_MakeWritable,
		CB_ConnectToSC,
		CB_Cancel,
		CB_ResaveAndCheckin,
		CB_MarkAsDirty,
		CB_MarkAsNotDirty
	};

	WxCheckOutPackageDlg() {}
	~WxCheckOutPackageDlg() {}
protected:
	virtual void OnButtonUpdateUI( wxUpdateUIEvent& In )
	{
		INT ButtonId = ButtonEntries( In.GetId() - CheckBoxListButtonBaseIndex ).ButtonID;
		if( ButtonId == CB_Checkout || ButtonId == CB_ResaveAndCheckin )
		{
			// Get a list of currently checked packages
			TArray< UPackage* > CheckedPackages; 
			GetResults( CheckedPackages, wxCHK_CHECKED );
			if( CheckedPackages.Num() == 0 )
			{
				// If no packages are checked, disable this button
				In.Enable( FALSE );
			}
			else
			{
				// Packages are checked, enable the button
				In.Enable( TRUE );
			}
		}
		else if( ButtonId == CB_MakeWritable )
		{
			// Get a list of currently checked packages
			// For the writable button, we also populate the list with packages that cannot be checked out (undetermined state)
			// as those can still be made writable.
			TArray< UPackage* > CheckedPackages; 
			GetResults( CheckedPackages, wxCHK_CHECKED );
			GetResults( CheckedPackages, wxCHK_UNDETERMINED );
			if( CheckedPackages.Num() == 0 )
			{	
				// Nothing is selected, disable the button
				In.Enable( FALSE );
			}
			else
			{
				// Packages are selected, enable the button
				In.Enable( TRUE );
			}
		}
		else if ( ButtonId == CB_ConnectToSC )
		{
#if HAVE_SCC
	        if ( FSourceControl::IsEnabled() )
	        {
	            In.Enable(FALSE);
	        }
	        else
	        {
	            In.Enable(TRUE);
	        }
#endif
		}
	}
};

/**
 * Prompt the user with a check-box dialog allowing him/her to check out the provided packages
 * from source control, if desired
 *
 * @param	bCheckDirty						If TRUE, non-dirty packages won't be added to the dialog
 * @param	PackagesToCheckOut				Reference to array of packages to prompt the user with for possible check out
 * @param	OutPackagesNotNeedingCheckout	If not NULL, this array will be populated with packages that the user was not prompted about and do not need to be checked out to save.  Useful for saving packages even if the user canceled the checkout dialog.
 * @param	bPromptingAfterModify			If TRUE, we are prompting the user after an object has been modified, which changes the cancel button to "Ask me later"
 * @param	bPromptingForDependentMaterialPackages If TRUE, this is being launched after loading a material with a dependency change.  This changes the button options, but the functionality is maintained.
 * 
 * @return	TRUE if the user did not cancel out of the dialog and has potentially checked out some files
 *			(or if there is no source control integration); FALSE if the user cancelled the dialog
 */
UBOOL FEditorFileUtils::PromptToCheckoutPackages(UBOOL bCheckDirty, const TArray<UPackage*>& PackagesToCheckOut, TArray<UPackage*>* OutPackagesNotNeedingCheckout, const UBOOL bPromptingAfterModify, const UBOOL bPromptingForDependentMaterialPackages )
{
	UBOOL bResult = TRUE;

#if HAVE_SCC
	if ( FSourceControl::IsEnabled() )
	{
		// Update the source control status of all potentially relevant packages
		FSourceControl::UpdatePackageStatus(PackagesToCheckOut);
	}
	
	// The checkout dialog to show users if any packages need to be checked out
	WxCheckOutPackageDlg PkgCheckOutDlg;

	// Add any of the packages which do not report as editable by source control, yet are currently in the source control depot
	// If the user has specified to check for dirty packages, only add those which are dirty
	UBOOL bPackagesAdded = FALSE;
	
	// If we found at least one package that can be checked out, this will be true
	UBOOL bHavePackageToCheckOut = FALSE;

	// Icon names for the checkout dialog
	const FString NotCheckedOutIcon( TEXT("SCC_DlgReadOnly.png") );
	const FString NotCheckedOutToolTip( LocalizeUnrealEd( TEXT("CheckOutDlg_NotCheckedOutTip") ) );
	const FString CheckedOutByOtherIcon( TEXT("SCC_DlgCheckedOutOther.png") );
	const FString CheckedOutByOtherToolTip( LocalizeUnrealEd( TEXT("CheckOutDlg_CheckedOutByOtherTip") ) );
	const FString NotCurrentIcon( TEXT("SCC_DlgNotCurrent.png") );
	const FString NotCurrentToolTip( LocalizeUnrealEd( TEXT("CheckOutDlg_NotCurrentTip") ) );


	// Iterate through all the packages and add them to the dialog if necessary.
	for ( TArray<UPackage*>::TConstIterator PackageIter( PackagesToCheckOut ); PackageIter; ++PackageIter )
	{
		UPackage* CurPackage = *PackageIter;
		FString Filename;
		// Assume the package is read only just in case we cant find a file
		UBOOL bPkgReadOnly = TRUE;
		// Find the filename for this package
		UBOOL bFoundFile = GPackageFileCache->FindPackageFile( *CurPackage->GetName(), NULL, Filename );
		if( bFoundFile )
		{
			// determine if the package file is read only
			bPkgReadOnly = GFileManager->IsReadOnly( *Filename );
		}
	
		INT SCCState = GPackageFileCache->GetSourceControlState(*CurPackage->GetName());
		// Package does not need to be checked out if its already checked out (and we're not the dependent material window), we are ignoring it for source control, or we dont care about the scc state
		UBOOL bSCCCanEdit = (SCCState == SCC_CheckedOut && !bPromptingForDependentMaterialPackages) || (SCCState == SCC_Ignore) || (SCCState == SCC_DontCare);
		
		if ( !bSCCCanEdit && (bPromptingForDependentMaterialPackages || (bPkgReadOnly && SCCState != SCC_NotInDepot)) && ( !bCheckDirty || ( bCheckDirty && CurPackage->IsDirty() ) ) )
		{
			if( SCCState == SCC_NotCurrent )
			{				
				// This package is not at the head revision and it should be ghosted as a result
				PkgCheckOutDlg.AddCheck( CurPackage, CurPackage->GetName(), wxCHK_UNCHECKED, TRUE, NotCurrentIcon, NotCurrentToolTip );
			}
			else if( SCCState == SCC_CheckedOutOther )
			{
				// This package is checked out by someone else so it should be ghosted
				PkgCheckOutDlg.AddCheck( CurPackage, CurPackage->GetName(), wxCHK_UNCHECKED, TRUE, CheckedOutByOtherIcon, CheckedOutByOtherToolTip );
			}
			else if(SCCState == SCC_NotInDepot)
			{
				// This package is being added so don't assume we want to submit it automatically
				PkgCheckOutDlg.AddCheck( CurPackage, CurPackage->GetName(), wxCHK_UNCHECKED, FALSE, NotCheckedOutIcon, NotCheckedOutToolTip);
			}
			else
			{
				bHavePackageToCheckOut = TRUE;
				//Add this package to the dialog if its not checked out, in the source control depot, dirty(if we are checking), and read only
				PkgCheckOutDlg.AddCheck( CurPackage, CurPackage->GetName(), wxCHK_CHECKED, FALSE, NotCheckedOutIcon , NotCheckedOutToolTip );
			}
			bPackagesAdded = TRUE;
		}
		else if ( bPkgReadOnly && bFoundFile && !FSourceControl::IsEnabled() )
		{
			// This package is read only but source control is not enabled, show the dialog so users can save the package by making the file writable.
			PkgCheckOutDlg.AddCheck( CurPackage, CurPackage->GetName(), wxCHK_UNCHECKED, TRUE, NotCheckedOutIcon , NotCheckedOutToolTip );
			bPackagesAdded = TRUE;
		}
		else if ( OutPackagesNotNeedingCheckout )
		{
			// The current package does not need to be checked out in order to save.
			OutPackagesNotNeedingCheckout->AddItem( CurPackage );
		}

	}

	// If any packages were added to the dialog, show the dialog to the user and allow them to select which files to check out
	if ( bPackagesAdded )
	{
		FIntPoint WindowSize(450, 400);

		// Prepare the buttons for the checkout dialog
		if(!bPromptingForDependentMaterialPackages)
		{
			// The checkout button should be disabled if no packages can be checked out.
			PkgCheckOutDlg.AddButton(WxCheckOutPackageDlg::CB_Checkout, *LocalizeUnrealEd( TEXT("CheckOutDlg_CheckOutButton") ), *LocalizeUnrealEd( TEXT("CheckOutDlg_CheckOutTooltip") ), !bHavePackageToCheckOut );
	
			// Make writable button to make checked files writable
			PkgCheckOutDlg.AddButton(WxCheckOutPackageDlg::CB_MakeWritable, *LocalizeUnrealEd( TEXT("CheckOutDlg_MakeWritableButton") ), *LocalizeUnrealEd( TEXT("CheckOutDlg_MakeWritableTooltip") ) );

	        // Add the ability to reconnect to source control if we've lost/closed our connection
    	    PkgCheckOutDlg.AddButton(WxCheckOutPackageDlg::CB_ConnectToSC, *LocalizeUnrealEd( TEXT("CheckOutDlg_Reconnect") ),*LocalizeUnrealEd( TEXT("CheckOutDlg_ReconnectTooltip") ) );

			// The cancel button should be different if we are prompting during a modify.
			const FString CancelButtonText  = bPromptingAfterModify ? LocalizeUnrealEd( TEXT("CheckOutDlg_AskMeLater") ) : LocalizeUnrealEd( TEXT("Cancel") );
			const FString CancelButtonToolTip = bPromptingAfterModify ? LocalizeUnrealEd( TEXT("CheckOutDlg_AskMeLaterToolTip") ) : LocalizeUnrealEd( TEXT("CheckOutDlg_CancelTooltip") ); 
			PkgCheckOutDlg.AddButton(WxCheckOutPackageDlg::CB_Cancel, *CancelButtonText, *CancelButtonToolTip );
		}
		else
		{
			// The checkout button should be disabled if no packages can be checked out.
			PkgCheckOutDlg.AddButton(WxCheckOutPackageDlg::CB_ResaveAndCheckin, *LocalizeUnrealEd( TEXT("CheckOutDlg_ResaveAndCheckin") ), *LocalizeUnrealEd( TEXT("CheckOutDlg_ResaveAndCheckinTip") ), !bHavePackageToCheckOut );

			// Make writable button to make checked files writable
			PkgCheckOutDlg.AddButton(WxCheckOutPackageDlg::CB_MarkAsDirty, *LocalizeUnrealEd( TEXT("CheckOutDlg_MarkAsDirty") ), *LocalizeUnrealEd( TEXT("CheckOutDlg_MarkAsDirtyTip") ) );
			// The cancel button should be different if we are prompting during a modify.
			PkgCheckOutDlg.AddButton(WxCheckOutPackageDlg::CB_MarkAsNotDirty, *LocalizeUnrealEd( TEXT("CheckOutDlg_MarkAsNotDirty") ), *LocalizeUnrealEd( TEXT("CheckOutDlg_MarkAsNotDirtyTip") ) );
		}

		// Show the dialog and store the user's response
		INT ButtonID = PkgCheckOutDlg.ShowDialog( bPromptingForDependentMaterialPackages ? *LocalizeUnrealEd( TEXT("CheckOutDlg_MaterialDependencyTitle") ) : *LocalizeUnrealEd( TEXT("CheckOutDlg_Title") ),
			bPromptingForDependentMaterialPackages ? *LocalizeUnrealEd( TEXT("CheckOutDlg_MaterialDependencyChangeMessage") ) : *LocalizeUnrealEd( TEXT("CheckOutDlg_Message") ),
			WindowSize, "CheckoutPacakgesDlg" );

		// If the user has not cancelled out of the dialog
		if ( ButtonID == WxCheckOutPackageDlg::CB_Checkout )
		{
			// Collect the packages that should be checked out from the user's choices in the dialog
			TArray<UPackage*> PkgsToCheckOut;
			PkgCheckOutDlg.GetResults( PkgsToCheckOut, wxCHK_CHECKED );

			UBOOL bPackageFailedCheckout = FALSE;
			FString PkgsWhichFailedCheckout = LocalizeUnrealEd( TEXT("FailedCheckOutDlg_Message") );

			// Attempt to check out each package the user specified to be checked out
			TArray<FString> PackageNames;
			for( TArray<UPackage*>::TIterator PkgsToCheckOutIter( PkgsToCheckOut ); PkgsToCheckOutIter; ++PkgsToCheckOutIter )
			{
				UPackage* PackageToCheckOut = *PkgsToCheckOutIter;
				INT SCCState = GPackageFileCache->GetSourceControlState(*PackageToCheckOut->GetName());

				// Check out the package if possible
				if( SCCState == SCC_ReadOnly )
				{
					PackageNames.AddItem(PackageToCheckOut->GetName());
				}
				// If the package couldn't be checked out, log it so the list of failures can be displayed afterwards
				else
				{
					PkgsWhichFailedCheckout += FString::Printf( TEXT("\n%s"), *PackageToCheckOut->GetName() );
					bPackageFailedCheckout = TRUE;
				}
			}
			if (PackageNames.Num())
			{
				FSourceControl::ConvertPackageNamesToSourceControlPaths(PackageNames);
				FSourceControl::CheckOut(NULL, PackageNames);
			}

			// If any packages failed the check out process, report them to the user so they know
			if ( bPackageFailedCheckout )
			{
				WxLongChoiceDialog FailedCheckoutDlg(
					*PkgsWhichFailedCheckout,
					*LocalizeUnrealEd("FailedCheckoutDlg_Title"),
					WxChoiceDialogBase::Choice( AMT_OK, *LocalizeUnrealEd("OK"), WxChoiceDialogBase::DCT_DefaultCancel ) );

				FailedCheckoutDlg.ShowModal();
			}
		}
		else if( ButtonID == WxCheckOutPackageDlg::CB_MakeWritable )
		{
			// Collect the packages that should be made writable out from the user's choices in the dialog
			TArray<UPackage*> PkgsToMakeWritable;
			// Both undetermined and checked should be made writable.  Undetermined is only available when packages cant be checked out
			PkgCheckOutDlg.GetResults( PkgsToMakeWritable, wxCHK_UNDETERMINED );
			PkgCheckOutDlg.GetResults( PkgsToMakeWritable, wxCHK_CHECKED);

			UBOOL bPackageFailedWritable = FALSE;
			FString PkgsWhichFailedWritable = LocalizeUnrealEd( TEXT("FailedMakingWritable_Message") );

			// Attempt to make writable each package the user checked
			for( TArray<UPackage*>::TIterator PkgsToMakeWritableIter( PkgsToMakeWritable ); PkgsToMakeWritableIter; ++PkgsToMakeWritableIter )
			{
				UPackage* PackageToMakeWritable = *PkgsToMakeWritableIter;
				FString Filename;

				UBOOL bFoundFile = GPackageFileCache->FindPackageFile( *PackageToMakeWritable->GetName(), NULL, Filename );
				if( bFoundFile )
				{
					// Get the fully qualified filename.
					const FString FullFilename = appConvertRelativePathToFull( Filename );
					// Get the current file attributes and knock off the read only flag
					DWORD FileAttributes = GetFileAttributesW( *Filename );
					if( SetFileAttributesW( *Filename, FileAttributes & ~FILE_ATTRIBUTE_READONLY) == 0)
					{
						bPackageFailedWritable = TRUE;
						PkgsWhichFailedWritable += FString::Printf( TEXT("\n%s"), *PackageToMakeWritable->GetName() );
					}
				}
			}

			if ( bPackageFailedWritable )
			{
				WxLongChoiceDialog FailedWritableDlg(
					*PkgsWhichFailedWritable,
					*LocalizeUnrealEd("FailedMakingWritableDlg_Title"),
					WxChoiceDialogBase::Choice( AMT_OK, *LocalizeUnrealEd("OK"), WxChoiceDialogBase::DCT_DefaultCancel ) );

				FailedWritableDlg.ShowModal();
			}

		}
		else if (ButtonID == WxCheckOutPackageDlg::CB_ConnectToSC)
		{
	        // Preserve the user's ExceptOnWarning preference (the other settings will be auto-acquired)
	        UBOOL bPreservedExceptOnWarningSetting = TRUE;
	        GConfig->GetBool( TEXT("SourceControl"), TEXT("ExceptOnWarning"), bPreservedExceptOnWarningSetting, GEditorUserSettingsIni );
    		
	        // Reset the user's source control settings, forcibly setting disabled to FALSE, and restoring the ExceptOnWarning preference
	        GConfig->EmptySection( TEXT("SourceControl"), GEditorUserSettingsIni );
	        GConfig->SetBool( TEXT("SourceControl"), TEXT("Disabled"), FALSE, GEditorUserSettingsIni );
	        GConfig->SetBool( TEXT("SourceControl"), TEXT("ExceptOnWarning"), bPreservedExceptOnWarningSetting, GEditorUserSettingsIni );
	        FSourceControl::Init();
	        bResult = PromptToCheckoutPackages(bCheckDirty, PackagesToCheckOut, OutPackagesNotNeedingCheckout, bPromptingAfterModify); 
		    
		}
		else if (ButtonID == WxCheckOutPackageDlg::CB_ResaveAndCheckin)
		{
			TSet<UPackage*> SetOfPackagesToSubmit;

			SetOfPackagesToSubmit.Add( PackagesToCheckOut );
			/*for(TArray<UPackage*>::TConstIterator ConvertFromArrayIter(PackagesToCheckOut); ConvertFromArrayIter; ++ConvertFromArrayIter)
			{
				SetOfPackagesToSubmit.Add(*ConvertFromArrayIter);
			}*/

			if(OutPackagesNotNeedingCheckout)
			{
				SetOfPackagesToSubmit.Add( *OutPackagesNotNeedingCheckout );
			/*	for(TArray<UPackage*>::TIterator ConvertFromArrayIter(*OutPackagesNotNeedingCheckout); ConvertFromArrayIter; ++ConvertFromArrayIter)
				{
					SetOfPackagesToSubmit.Add(*ConvertFromArrayIter);
				}*/
			}

			for ( TSet<UPackage*>::TConstIterator PkgIter( SetOfPackagesToSubmit ); PkgIter; ++PkgIter )
			{
				UPackage* CurPkg = *PkgIter;
				const FString PkgName = CurPkg->GetName();
				const INT PackageStatus = GPackageFileCache->GetSourceControlState( *PkgName );
				if ( PackageStatus == SCC_ReadOnly )
				{
					FSourceControl::CheckOut(CurPkg);
				}
			}
			
			FSourceControl::UpdatePackageStatus(PackagesToCheckOut);
			if(PackageTools::SavePackages(PackagesToCheckOut, FALSE))
			{
				if(OutPackagesNotNeedingCheckout)
				{
					FSourceControl::UpdatePackageStatus(*OutPackagesNotNeedingCheckout);
					PackageTools::SavePackages(*OutPackagesNotNeedingCheckout, FALSE);
				}

				FEditorBuildUtils::SaveAndCheckInPackages(SetOfPackagesToSubmit, LocalizeUnrealEd("ChangeDescriptionMaterialDepChanges"));
			}
			else
			{
				appMsgf( AMT_OK, *LocalizeUnrealEd("Error_CouldntSavePackage") );
			}
		}
		else if (ButtonID == WxCheckOutPackageDlg::CB_MarkAsDirty || ButtonID == WxCheckOutPackageDlg::CB_MarkAsNotDirty)
		{
			for(TArray<UPackage*>::TConstIterator DirtyFlagIter(PackagesToCheckOut); DirtyFlagIter; ++DirtyFlagIter)
			{
				(**DirtyFlagIter).SetDirtyFlag(ButtonID == WxCheckOutPackageDlg::CB_MarkAsDirty);
			}

			if(OutPackagesNotNeedingCheckout)
			{
				for(TArray<UPackage*>::TIterator DirtyFlagIter(*OutPackagesNotNeedingCheckout); DirtyFlagIter; ++DirtyFlagIter)
				{
					(**DirtyFlagIter).SetDirtyFlag(ButtonID == WxCheckOutPackageDlg::CB_MarkAsDirty);
				}
			}
		}
		// Handle the case of the user canceling out of the dialog
		else
		{
			bResult = FALSE;
		}
	}

	// Update again to catch potentially new SCC states
	GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateSCCState ));

#endif // HAVE_SCC

	return bResult;
}

/**
 * Prompt the user with a check-box dialog allowing him/her to check out relevant level packages 
 * from source control
 *
 * @param	bCheckDirty					If TRUE, non-dirty packages won't be added to the dialog
 * @param	SpecificLevelsToCheckOut	If specified, only the provided levels' packages will display in the
 *										dialog if they are under source control; If nothing is specified, all levels
 *										referenced by GWorld whose packages are under source control will be displayed
 * @param	OutPackagesNotNeedingCheckout	If not null, this array will be populated with packages that the user was not prompted about and do not need to be checked out to save.  Useful for saving packages even if the user canceled the checkout dialog.
 *
 * @return	TRUE if the user did not cancel out of the dialog and has potentially checked out some files (or if there is
 *			no source control integration); FALSE if the user cancelled the dialog
 */
UBOOL FEditorFileUtils::PromptToCheckoutLevels(UBOOL bCheckDirty, const TArray<ULevel*>* const SpecificLevelsToCheckOut, TArray<UPackage*>* OutPackagesNotNeedingCheckout )
{
	UBOOL bResult = TRUE;

	// Only attempt to display the dialog and check out packages if source control integration is present
#if HAVE_SCC
	TArray<UPackage*> WorldPackages;
	UBOOL bPackagesAdded = FALSE;

	// If levels were specified by the user, they should be the only ones considered potentially relevant
	if ( SpecificLevelsToCheckOut )
	{
		for ( TArray<ULevel*>::TConstIterator SpecificLevelsIter( *SpecificLevelsToCheckOut ); SpecificLevelsIter; ++SpecificLevelsIter )
		{
			UPackage* LevelsWorldPackage = ( *SpecificLevelsIter )->GetOutermost();

			// If the user has specified to check if the package is dirty, do so before deeming
			// the package potentially relevant
			if ( LevelsWorldPackage && ( !bCheckDirty || ( bCheckDirty && LevelsWorldPackage->IsDirty() ) )  )
			{
				WorldPackages.AddUniqueItem( LevelsWorldPackage );
			}
		}
	}

	// If no level was specified by the user, consider GWorld and all referenced worlds
	else
	{
		// Handle the special case of GWorld
		const FFilename GWorldFilename = GetFilename( GWorld );
		
		// If the GWorld doesn't have a file name, it's a new map which has never been saved (and can't possibly be in source control)
		// If it does have a file name, it should be considered potentially relevant, because it may be under source control
		
		// Note: There is a minor hack at play here in that GWorld is considered regardless of it being dirty or not because
		// other code is going to explicitly save it later, whether the user has specified to check dirty or not. If the
		// dirty check was enforced, the user would encounter an error where the map was not displayed in the dialog to check
		// out, but would then later get an error about the map not being writable.
		if ( GWorldFilename.Len() > 0 )
		{
			UPackage* GWorldPackage = Cast<UPackage>( GWorld->GetOuter() );
			if ( GWorldPackage )
			{
				WorldPackages.AddUniqueItem( GWorldPackage );
			}
		}

		// Clean up any old worlds.
		UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

		// Handle all referenced worlds
		TArray<UWorld*> WorldsArray;
		FLevelUtils::GetWorlds( WorldsArray, FALSE );

		for ( TArray<UWorld*>::TConstIterator WorldIter( WorldsArray ); WorldIter; ++WorldIter )
		{
			const UWorld* const World = *WorldIter;
			UPackage* WorldPackage = Cast<UPackage>( World->GetOuter() );
			
			// If the user has specified to check if the package is dirty, do so before deeming
			// the package potentially relevant
			if ( WorldPackage && ( !bCheckDirty || ( bCheckDirty && WorldPackage->IsDirty() ) ) )
			{
				WorldPackages.AddUniqueItem( WorldPackage );
			}
		}
	}

	// Prompt the user with the provided packages if they prove to be relevant (i.e. in source control and not checked out)
	// Note: The user's dirty flag option is not passed in here because it's already been taken care of within the function (with a special case)
	bResult = FEditorFileUtils::PromptToCheckoutPackages( FALSE, WorldPackages, OutPackagesNotNeedingCheckout );
#endif // HAVE_SCC

	return bResult;
}

/**
 * Overloaded version of PromptToCheckOutLevels which prompts the user with a check-box dialog allowing
 * him/her to check out the relevant level package if necessary
 *
 * @param	bCheckDirty				If TRUE, non-dirty packages won't be added to the dialog
 * @param	SpecificLevelToCheckOut	The level whose package will display in the dialog if it is
 *									under source control
 *
 * @return	TRUE if the user did not cancel out of the dialog and has potentially checked out some files (or if there is
 *			no source control integration); FALSE if the user cancelled the dialog
 */
UBOOL FEditorFileUtils::PromptToCheckoutLevels(UBOOL bCheckDirty, ULevel* SpecificLevelToCheckOut)
{
	check( SpecificLevelToCheckOut != NULL );

	// Add the specified level to an array and use the other version of this function
	TArray<ULevel*> LevelsToCheckOut;
	LevelsToCheckOut.AddUniqueItem( SpecificLevelToCheckOut );

	return FEditorFileUtils::PromptToCheckoutLevels( bCheckDirty, &LevelsToCheckOut );	
}

/** If TRUE, FWindowsViewport::UpdateModifierState() will enqueue rather than process immediately. */
extern UBOOL GShouldEnqueueModifierStateUpdates;

/**
 * If a PIE world exists, give the user the option to terminate it.
 *
 * @return				TRUE if a PIE session exists and the user refused to end it, FALSE otherwise.
 */
static UBOOL ShouldAbortBecauseOfPIEWorld()
{
	// If a PIE world exists, warn the user that the PIE session will be terminated.
	if ( GEditor->PlayWorld )
	{
		const FString Question = FString::Printf( *LocalizeUnrealEd("Prompt_ThisActionWillTerminatePIEContinue") );
		if( appMsgf( AMT_YesNo, *Question ) )
		{
			// End the play world.
			GEditor->EndPlayMap();
		}
		else
		{
			// User didn't want to end the PIE session -- abort the load.
			return TRUE;
		}
	}
	return FALSE;
}
/**
 * Prompts the user to save the current map if necessary, the presents a load dialog and
 * loads a new map if selected by the user.
 */
void FEditorFileUtils::LoadMap()
{
	GShouldEnqueueModifierStateUpdates = TRUE;
	WxFileDialog dlg( GApp->EditorFrame, *LocalizeUnrealEd("Open"), *GetDefaultDirectory(), TEXT(""), *GetFilterString(FI_Load), wxOPEN | wxFILE_MUST_EXIST );
	if( dlg.ShowModal() == wxID_OK )
	{
		// If there are any unsaved changes to the current level, see if the user wants to save those first.
		UBOOL bPromptUserToSave = TRUE;
		UBOOL bSaveMapPackages = TRUE;
		UBOOL bSaveContentPackages = FALSE;
		if( FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages) == FALSE )
		{
			// something went wrong or the user pressed cancel.  Return to the editor so the user doesn't lose their changes		
			return;
		}

		LoadMap( FString( dlg.GetPath() ) );
	}
	GShouldEnqueueModifierStateUpdates = FALSE;
}

/**
 * Loads the specified map.  Does not prompt the user to save the current map.
 *
 * @param	Filename		Map package filename, including path.
 *
 * @param	LoadAsTemplate	Forces the map to load into an untitled outermost package
 *							preventing the map saving over the original file.
 */
void FEditorFileUtils::LoadMap(const FFilename& Filename, UBOOL LoadAsTemplate)
{
	DOUBLE LoadStartTime = appSeconds();

	const FScopedBusyCursor BusyCursor;

	UBOOL bKismetWasOpen = FALSE;

	// If a PIE world exists, warn the user that the PIE session will be terminated.
	// Abort if the user refuses to terminate the PIE session.
	if ( ShouldAbortBecauseOfPIEWorld() )
	{
		return;
	}

	// Change out of Matinee when opening new map, so we avoid editing data in the old one.
	if( GEditorModeTools().IsModeActive( EM_InterpEdit ) )
	{
		GEditorModeTools().ActivateMode( EM_Default );
	}

	// Also change out of Landscape mode to ensure all references are cleared.
	if( GEditorModeTools().IsModeActive( EM_Landscape ) )
	{
		GEditorModeTools().DeactivateMode( EM_Landscape );
	}

	// Change out of mesh paint mode when loading a map
	if( GEditorModeTools().IsModeActive( EM_MeshPaint ) )
	{
		GEditorModeTools().DeactivateMode( EM_MeshPaint );
	}

	// Check if we should re-open the kismet window after loading the new map
	if( GApp->KismetWindows.Num() > 0 )
	{
		bKismetWasOpen = TRUE;
	}

	// Before opening new file, close any windows onto existing level.
	WxKismet::CloseAllKismetWindows();

	// Close Sentinel if its open
	if(GApp->SentinelTool)
	{
		GApp->SentinelTool->Close(true);
	}

	// toss all the cross level stuff, because we are loading a new level and don't need to slow down the cleanup process
	GCrossLevelReferenceManager->Reset();

	// Toss any mobile emulation materials...
#if WITH_EDITOR
	//@todo.MOBEMU: Would be nice to *not* toss ones for 'always loaded' objects to avoid
	//				having to recreate them.
	FMobileEmulationMaterialManager::GetManager()->ClearCachedMaterials();
#endif

	FString LoadCommand = FString::Printf( TEXT("MAP LOAD FILE=\"%s\" TEMPLATE=%d"), *Filename, LoadAsTemplate );
	GUnrealEd->Exec( *LoadCommand );
	ResetLevelFilenames();
	//only register the file if the name wasn't changed as a result of loading
	if (GWorld->GetOutermost()->GetName() == Filename.GetBaseFilename())
	{
		RegisterLevelFilename( GWorld, Filename );
	}

	if( !bIsLoadingSimpleStartupMap && !LoadAsTemplate )
	{
		// Don't set the last directory when loading the simple map or template as it is confusing to users
		GApp->LastDir[LD_UNR] = Filename.GetPath();
	}

	//ensure the name wasn't mangled during load before adding to the Recent File list
	if (GWorld->GetOutermost()->GetName() == Filename.GetBaseFilename())
	{
		GApp->EditorFrame->GetMRUFavoritesList()->AddMRUItem( *Filename );
	}

	// Restore camera views to current viewports
	RestoreViewportViews( GWorld );

	GCallbackEvent->Send(CALLBACK_RefreshEditor_AllBrowsers);
	//force full rebuild of content browser so it doesn't happen on first subsequent asset load	
	GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateAssetList));

	if( !ParseParam(appCmdLine(),TEXT("DEMOMODE")) )
	{
		// Check for deprecated actor classes.
		GEditor->Exec( TEXT("MAP CHECKDEP DONTCLEARMESSAGES DONTDOSLOWREFCHECK") );
		GWarn->MapCheck_ShowConditionally();
	}

	// Track time spent loading map.
	GTaskPerfTracker->AddTask( TEXT("Editor map load"), *Filename.GetBaseFilename(), appSeconds() - LoadStartTime );

	// Save the shader caches after a map load so if something bad happens and the editor does not shut down properly
	// shaders for that map dont have to be recompiled.
	SaveLocalShaderCaches();

	if(bKismetWasOpen)
	{
		WxKismet::OpenKismet( NULL, TRUE, GApp->EditorFrame );
	}

	// Update volume actor visibility for each viewport since we loaded a level which could
	// potentially contain volumes.
	GUnrealEd->UpdateVolumeActorVisibility(NULL);

#if WITH_EDITOR
	UBOOL bPreviousEmulationEnabled = GEmulateMobileRendering;
	//force off to get a toggle event when enabled
	SetMobileRenderingEmulation( FALSE, GWorld->GetWorldInfo()->bUseGammaCorrection );
	//Now restore the original setting
	SetMobileRenderingEmulation( bPreviousEmulationEnabled, GWorld->GetWorldInfo()->bUseGammaCorrection );
#endif
}

/**
 * Saves the specified map package, returning TRUE on success.
 *
 * @param	World			The world to save.
 * @param	Filename		Map package filename, including path.
 * @param	bAddToMRUList	True if the level should be added to the editor's MRU level list
 *
 * @return					TRUE if the map was saved successfully.
 */
UBOOL FEditorFileUtils::SaveMap(UWorld* World, const FFilename& Filename, const UBOOL bAddToMRUList )
{
	UBOOL bLevelWasSaved = FALSE;

	// Disallow the save if in interpolation editing mode and the user doesn't want to exit interpolation mode.
	if ( !InInterpEditMode() )
	{
		DOUBLE SaveStartTime = appSeconds();

		// Update cull distance volumes (and associated primitives).
		GWorld->UpdateCullDistanceVolumes();

		// Save the camera views for this world.
		SaveViewportViews( World );

		// Only save the world if GEditor is null, the Persistent Level is not using Externally referenced objects or the user wants to continue regardless
		if ( !GEditor || !GEditor->PackageUsingExternalObjects(World->PersistentLevel) || appMsgf(AMT_YesNo, *LocalizeUnrealEd("Warning_UsingExternalPackage")) )
		{
			FString FinalFilename;
			bLevelWasSaved = SaveWorld( World, &Filename,
										NULL, NULL,
										TRUE, TRUE, FALSE,
										FinalFilename,
										FALSE, FALSE );
		}

		// If the map saved, then put it into the MRU and mark it as not dirty.
		if ( bLevelWasSaved )
		{
			// Set the map filename.
			RegisterLevelFilename( World, Filename );

			World->MarkPackageDirty( FALSE );

			// Update the editor's MRU level list if we were asked to do that for this level
			if( bAddToMRUList )
			{
				GApp->EditorFrame->GetMRUFavoritesList()->AddMRUItem( Filename );
				GApp->LastDir[LD_UNR] = Filename.GetPath();
			}

			// We saved the map, so unless there are any other dirty levels, go ahead and reset the autosave timer
			if( !GUnrealEd->AnyWorldsAreDirty() )
			{
				GUnrealEd->ResetAutosaveTimer();
			}
		}

		GUnrealEd->ResetTransaction( *LocalizeUnrealEd("MapSaved") );

		GFileManager->SetDefaultDirectory();

		// Track time spent saving map.
		GTaskPerfTracker->AddTask( TEXT("Editor map save"), *Filename.GetBaseFilename(), appSeconds() - SaveStartTime );
	}

	return bLevelWasSaved;
}

/**
 * Prompts the user to save the current map if necessary, then creates a new (blank) map.
 */
void FEditorFileUtils::NewMap()
{
	UBOOL bKismetWasOpen = FALSE;

	// If a PIE world exists, warn the user that the PIE session will be terminated.
	// Abort if the user refuses to terminate the PIE session.
	if ( ShouldAbortBecauseOfPIEWorld() )
	{
		return;
	}

	// If there are any unsaved changes to the current level, see if the user wants to save those first.
	UBOOL bPromptUserToSave = TRUE;
	UBOOL bSaveMapPackages = TRUE;
	UBOOL bSaveContentPackages = FALSE;
	if( FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages) == FALSE )
	{
		// something went wrong or the user pressed cancel.  Return to the editor so the user doesn't lose their changes		
		return;
	}
	
	const FScopedBusyCursor BusyCursor;

	// Change out of Matinee when opening new map, so we avoid editing data in the old one.
	if( GEditorModeTools().IsModeActive( EM_InterpEdit ) )
	{
		GEditorModeTools().DeactivateMode( EM_InterpEdit );
	}

	// Also change out of Landscape mode to ensure all references are cleared.
	if( GEditorModeTools().IsModeActive( EM_Landscape ) )
	{
		GEditorModeTools().DeactivateMode( EM_Landscape );
	}

	// Change out of mesh paint mode when opening a new map.
	if( GEditorModeTools().IsModeActive( EM_MeshPaint ) )
	{
		GEditorModeTools().DeactivateMode( EM_MeshPaint );
	}

	// Check if we should re-open the kismet window after loading the new map
	if( GApp->KismetWindows.Num() > 0 )
	{
		bKismetWasOpen = TRUE;
	}

	// Before opening new file, close any windows onto existing level.
	WxKismet::CloseAllKismetWindows();

	// Close Sentinel if its open
	if(GApp->SentinelTool)
	{
		GApp->SentinelTool->Close(true);
	}

	// toss all the cross level stuff, because we are making a new level and don't need to slow down the cleanup process
	GCrossLevelReferenceManager->Reset();

	GUnrealEd->NewMap();

	ResetLevelFilenames();

	if(bKismetWasOpen)
	{
		WxKismet::OpenKismet( NULL, TRUE, GApp->EditorFrame );
	}
}

/**
 * Shows the new map screen/dialog. Gives the user a choice of new blank or
 * templated maps and deals with overwrite prompts etc.
 */
void FEditorFileUtils::NewMapInteractive()
{
// With managed-code - show new map screen and preform the users selected operation (new blank map, new template map or cancel)
// If not compiling with managed-code - fallback on old behaviour - new blank map
#if WITH_MANAGED_CODE
	TArray<UTemplateMapMetadata*> Templates;
	UTemplateMapMetadata::GenerateTemplateMetadataList(Templates);
	FString TemplateName;
	// Show the new map screen - will return FALSE if the user cancels and perform no action.
	if (FNewMapScreen::DisplayNewMapScreen(Templates, TemplateName))
	{
		// The new map screen will return a blank TemplateName if the user has selected to begin a new blank map
		if (TemplateName.IsEmpty())
		{
			FEditorFileUtils::NewMap();
		}
		else
		{
			// New map screen returned a non-empty TemplateName, so the user has selected to begin from a template map
			UBOOL TemplateFound = FALSE;
			const FString& Ext = TEXT(".dnfmap");

			// Search all template map folders for a match with TemplateName
			for (INT folderIdx = 0; folderIdx < GEditor->TemplateMapFolders.Num(); folderIdx++)
			{
				const FString& Folder = GEditor->TemplateMapFolders(folderIdx);

				FString TemplatePath = FString::Printf(TEXT("%s\\%s%s"), *Folder, *TemplateName, *Ext);
				if (0 < GFileManager->FileSize(*TemplatePath))
				{
					// File found because the size check came back non-zero
					TemplateFound = TRUE;

					// If there are any unsaved changes to the current level, see if the user wants to save those first.
					UBOOL bPromptUserToSave = TRUE;
					UBOOL bSaveMapPackages = TRUE;
					UBOOL bSaveContentPackages = FALSE;
					if (TRUE == FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages))
					{
						// Load the template map file - passes LoadAsTemplate==TRUE making the
						// level load into an untitled package that won't save over the template
						FEditorFileUtils::LoadMap(*TemplatePath, TRUE);
					}

					break;
				}
			}

			if (!TemplateFound)
			{
				GWarn->Logf(NAME_Warning, TEXT("Couldn't find template map file %s"), *TemplateName);
				FEditorFileUtils::NewMap();
			}
		}
	}
#else
	FEditorFileUtils::NewMap();
#endif
}

/**
 * Shows the new project screen/dialog. Guides the user through a series of
 * required to setup and customize a new project.
 */
void FEditorFileUtils::NewProjectInteractive()
{
	FNewUDKProjectWizard wiz;
	FNewUDKProjectSettings::Get().ProjectWizardMode = EUDKProjWizMode::Interactive;
	wiz.RunUDKProjectWizard();

}

/**
 * Clears current level filename so that the user must SaveAs on next Save.
 * Called by NewMap() after the contents of the map are cleared.
 * Also called after loading a map template so that the template isn't overwritten.
 */
void FEditorFileUtils::ResetLevelFilenames()
{
	// Empty out any existing filenames.
	LevelFilenames.Empty();

	// Register a blank filename
	const FName PackageName(*GWorld->GetOutermost()->GetName());
	const FFilename EmptyFilename;
	LevelFilenames.Set( PackageName, EmptyFilename );

	GApp->EditorFrame->RefreshCaption( &EmptyFilename );
}

/**
 * Saves all levels to the specified directory.
 *
 * @param	AbsoluteAutosaveDir		Autosave directory.
 * @param	AutosaveIndex			Integer prepended to autosave filenames..
 */
UBOOL FEditorFileUtils::AutosaveMap(const FString& AbsoluteAutosaveDir, INT AutosaveIndex)
{
	const FScopedBusyCursor BusyCursor;
	DOUBLE SaveStartTime = appSeconds();

	UBOOL bLevelWasSaved = FALSE;

	// Update cull distance volumes (and associated primitives).
	GWorld->UpdateCullDistanceVolumes();

	// Clean up any old worlds.
	UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

	// Get the set of all reference worlds.
	TArray<UWorld*> WorldsArray;
	FLevelUtils::GetWorlds( WorldsArray, TRUE );

	WxUnrealEdApp* EdApp = static_cast<WxUnrealEdApp*>(GApp);

	if ( WorldsArray.Num() > 0 )
	{
		FString FinalFilename;
		for ( INT WorldIndex = 0 ; WorldIndex < WorldsArray.Num() && EdApp->AutosaveState != WxUnrealEdApp::AUTOSAVE_Cancelled ; ++WorldIndex )
		{
			UWorld* World = WorldsArray( WorldIndex );
			UPackage* Package = Cast<UPackage>( World->GetOuter() );
			check( Package );

			// If this world needs saving . . .
			if ( Package->IsDirty() )
			{
				// Come up with a meaningful name for the autosave file.
				const FString PackageName = Package->GetName();
				const FString AutosaveFilename( FFilename(PackageName).GetBaseFilename() );

				// Create an autosave filename.
				const FFilename Filename( AbsoluteAutosaveDir * *FString::Printf( TEXT("%s_Auto%i.%s"), *AutosaveFilename, AutosaveIndex, *GApp->DefaultMapExt ) );
				//debugf( NAME_Log, TEXT("Autosaving '%s'"), *Filename );
				bLevelWasSaved |= SaveWorld( World, &Filename,
											 NULL, NULL,
											 FALSE, FALSE, TRUE,
											 FinalFilename,
											 TRUE, FALSE );

				// Remark the package as being dirty, as saving will have undiritied the package.
				Package->MarkPackageDirty();
			}
			else
			{
				//debugf( NAME_Log, TEXT("No need to autosave '%s', not dirty"), *Package->GetName() );
			}
		}

		// Track time spent saving map.
		GTaskPerfTracker->AddTask( TEXT("Editor map autosave (incl. sublevels)"), *GWorld->GetOutermost()->GetName(), appSeconds() - SaveStartTime );
	}

	return bLevelWasSaved && EdApp->AutosaveState != WxUnrealEdApp::AUTOSAVE_Cancelled;
}

/**
 * Saves all asset packages to the specified directory.
 *
 * @param	AbsoluteAutosaveDir		Autosave directory.
 * @param	AutosaveIndex					Integer prepended to autosave filenames.
 * @param PackagesToSave				List of packages to be saved, if empty saves all
 *
 * @return	TRUE if one or more packages were autosaved; FALSE otherwise
 */
UBOOL FEditorFileUtils::AutosaveContentPackages(const FString& AbsoluteAutosaveDir, INT AutosaveIndex, const TArrayNoInit<FString>* PackagesToSave)
{
	const FScopedBusyCursor BusyCursor;
	DOUBLE SaveStartTime = appSeconds();
	
	UBOOL bSavedPkgs = FALSE;
	const UPackage* TransientPackage = UObject::GetTransientPackage();
	
	// Check all packages for dirty, non-map, non-transient packages
	for ( TObjectIterator<UPackage> PackageIter; PackageIter; ++PackageIter )
	{
		UPackage* CurPackage = *PackageIter;
		const UBOOL bIsMapPackage = Cast<UWorld>( UObject::StaticFindObject( UWorld::StaticClass(), CurPackage, TEXT("TheWorld") ) ) != NULL;

		// If the package is dirty and is not the transient package or a map package, we'd like to autosave it
		if ( CurPackage && CurPackage->IsDirty() && !bIsMapPackage && ( CurPackage != TransientPackage ) )
		{
			// Only save this package if none have been specified or it's in the specified list
			if ( !PackagesToSave || ( PackagesToSave->Num() == 0 ) || PackagesToSave->ContainsItem( CurPackage->GetName() ) )
			{
				// In order to save, the package must be fully-loaded first
				if( !CurPackage->IsFullyLoaded() )
				{
					GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("FullyLoadingPackages")), TRUE );
					CurPackage->FullyLoad();
					GWarn->EndSlowTask();
				}

				// Come up with a meaningful name for the autosave file.
				const FFilename PackageName = CurPackage->GetName();
				const FString AutosaveBaseFilename( PackageName.GetBaseFilename() );

				// Create an autosave filename.
				const FFilename AutosaveFilename( AbsoluteAutosaveDir * *FString::Printf( TEXT("%s_Auto%i.%s"), *AutosaveBaseFilename, AutosaveIndex, TEXT("upk") ) );
				GUnrealEd->Exec( *FString::Printf( TEXT("OBJ SAVEPACKAGE PACKAGE=\"%s\" FILE=\"%s\" SILENT=TRUE"), *PackageName, *AutosaveFilename ) );

				// Re-mark the package as dirty, because autosaving it will have cleared the dirty flag
				CurPackage->MarkPackageDirty();
				bSavedPkgs = TRUE;
			}
		}
	}
	
	if ( bSavedPkgs )
	{	
		// Track time spent saving asset packages
		GTaskPerfTracker->AddTask( TEXT("Editor content packages autosave"), TEXT(""), appSeconds() - SaveStartTime );
	}

	return bSavedPkgs;
}
/**
 * Actually save a package. Prompting for Save as if necessary
 *
 * @param Package						The package to save.
 * @param OutPackageLocallyWritable		Set to TRUE if the provided package was locally writable but not under source control (of if source control is disabled).
 * @return	ART_Yes if package saving was a success, ART_No if the package saving failed and the user doesn't want to retry, ART_Cancel if the user wants to cancel everything 
 */
static INT InternalSavePackage( UPackage* PackageToSave, UBOOL& bOutPackageLocallyWritable )
{
	// What we will be returning. Assume for now that everything will go fine
	INT ReturnCode = ART_Yes;

	// Assume the package is locally writable in case SCC is disabled; if SCC is enabled, it will
	// correctly set this value later
	bOutPackageLocallyWritable = TRUE;

	UBOOL bShouldRetrySave = TRUE;
	UWorld*	AssociatedWorld	= static_cast<UWorld*>( UObject::StaticFindObject(UWorld::StaticClass(), PackageToSave, TEXT("TheWorld")) );
	const UBOOL	bIsMapPackage = AssociatedWorld != NULL;

	// Place were we should save the file, including the filename
	FString FinalPackageSavePath;
	// Just the filename
	FString FinalPackageFilename;

	// True if we should attempt saving
	UBOOL bAttemptSave = TRUE;

	FString ExistingFilename;
	if( GPackageFileCache->FindPackageFile( *PackageToSave->GetName(), NULL, ExistingFilename ) )
	{
		// The file already exists, no need to prompt for save as
		FString BaseFilename, Extension, Directory;
		// Split the path to get the filename without the directory structure
		GPackageFileCache->SplitPath(*ExistingFilename, Directory, BaseFilename, Extension );
		// The final save path is whatever the existing filename is
		FinalPackageSavePath = ExistingFilename;
		// Format the filename we found from splitting the path
		FinalPackageFilename = FString::Printf( TEXT("%s.%s"), *BaseFilename, *Extension );
	}
	else
	{
		// There wont be a "not checked out from SCC but writable on disk" conflict if the package is new.
		bOutPackageLocallyWritable = FALSE;

		// Make a list of file types
		// We have to ask for save as.
		FString FileTypes( *LocalizeUnrealEd("AllFiles") );
		for(INT i=0; i<GSys->Extensions.Num(); i++)
		{
			FileTypes += FString::Printf( TEXT("|(*.%s)|*.%s"), *GSys->Extensions(i), *GSys->Extensions(i) );
		}

		const FString Directory = *GetDefaultDirectory();
		if (bIsMapPackage)
		{
			FinalPackageFilename = FString::Printf( TEXT("Untitled.%s"), *GApp->DefaultMapExt );
		}
		else
		{
			FinalPackageFilename = FString::Printf( TEXT("%s.upk"), *PackageToSave->GetName() );
		}

		// The number of times the user pressed cancel
		INT NumSkips = 0;

		// If the user presses cancel more than this time, they really don't want to save the file
		const INT NumSkipsBeforeAbort = 2;

		// if the user hit cancel on the Save dialog, ask again what the user wants to do, 
		// we shouldn't assume they want to skip the file
		// This loop continues indefinitely if the user does not supply a valid filename.  They must supply a valid filename or press cancel
		while( NumSkips < NumSkipsBeforeAbort )
		{
			WxFileDialog SaveFileDialog( GApp->EditorFrame, *LocalizeUnrealEd("SavePackage"), *Directory, *FinalPackageFilename, *FileTypes, wxSAVE | wxOVERWRITE_PROMPT, wxDefaultPosition );
			if( SaveFileDialog.ShowModal() == wxID_OK )
			{
				FinalPackageFilename = FString( SaveFileDialog.GetPath() );

				// If the supplied file name is missing an extension then give it the default package
				// file extension.
				if( FinalPackageFilename.Len() > 0 && FFilename( FinalPackageFilename ).GetExtension().Len() == 0 )
				{
					if( bIsMapPackage )
					{
						// Map packages should have the map extension
						FinalPackageFilename += FString::Printf( TEXT(".%s"), *GApp->DefaultMapExt );
					}
					else
					{
						// Content packages should have the content extension
						FinalPackageFilename += TEXT(".upk");
					}
				}
			
				FString ErrorMessage;
				if( !FEditorFileUtils::IsFilenameValidForSaving( FinalPackageFilename, ErrorMessage ) )
				{
					FFilename Filename( FinalPackageFilename );
					appMsgf( AMT_OK, *ErrorMessage );
					// Start the loop over, prompting for save again
					continue;
				}
				else
				{
					FinalPackageSavePath = FinalPackageFilename;
					// Stop looping, we successfully got a valid path and filename to save
					break;
				}
			}
			else
			{
				// if the user hit cancel on the Save dialog, ask again what the user wants to do, 
				// we shouldn't assume they want to skip the file unless they press cancel several times
				++NumSkips;
				if( NumSkips == NumSkipsBeforeAbort )
				{
					// They really want to stop
					bAttemptSave = FALSE;
					ReturnCode = ART_Cancel;
				}
			}
		}
	}

	// The name of the package
	FString PackageName = PackageToSave->GetName();

	// attempt the save

	while( bAttemptSave )
	{
		UBOOL bWasSuccessful = FALSE;
		if ( bIsMapPackage )
		{
			const UBOOL bIsPersistentLevel = ( AssociatedWorld == GWorld );
			const UBOOL bAddToMRUList = bIsPersistentLevel;

			// have a Helper attempt to save the map
			bWasSuccessful = FEditorFileUtils::SaveMap( AssociatedWorld, FinalPackageSavePath, bAddToMRUList );
			debugf( TEXT("Saving Map: %s"), *PackageName);
		}
		else
		{
			// normally, we just save the package
			bWasSuccessful = GUnrealEd->Exec( *FString::Printf( TEXT("OBJ SAVEPACKAGE PACKAGE=\"%s\" FILE=\"%s\" SILENT=TRUE"), *PackageName, *FinalPackageSavePath ) );
			debugf(TEXT("Saving Package: %s"), *PackageName);
		}

#if HAVE_SCC
		if (FSourceControl::IsEnabled())
		{
			// Assume the package was correctly checked out from SCC
			bOutPackageLocallyWritable = FALSE;

			// Update SCC status as it might have changed
			TArray< UPackage* > PackagesToUpdate;
			PackagesToUpdate.AddItem( PackageToSave );
			FSourceControl::UpdatePackageStatus( PackagesToUpdate );

			const INT SCCState = GPackageFileCache->GetSourceControlState( *PackageToSave->GetName() );
			// If the package is in the depot, and not recognized as editable by source control, and not read-only, then we know the user has made the package locally writable!
			const UBOOL bSCCCanEdit = (SCCState == SCC_CheckedOut) || (SCCState == SCC_Ignore) || (SCCState == SCC_DontCare);
			const UBOOL bInDepot = (SCCState != SCC_NotInDepot);
			if ( !bSCCCanEdit && bInDepot && !GFileManager->IsReadOnly( *FinalPackageSavePath ) )
			{
				bOutPackageLocallyWritable = TRUE;
			}
		}
		else
		{
			// If source control is disabled then we dont care if the package is locally writable
			bOutPackageLocallyWritable = FALSE;
		}
#else 
	// If source control isn't available then we dont care if the package is locally writable
	bOutPackageLocallyWritable = FALSE;
#endif

		// Handle all failures the same way.
		if ( !bWasSuccessful )
		{
			// ask the user what to do if we failed
			const INT CancelRetryContinueReply = appMsgf( AMT_CancelRetryContinue, LocalizeSecure(LocalizeUnrealEd("Prompt_26"), *PackageName, *FinalPackageFilename) );
			switch ( CancelRetryContinueReply )
			{
			case 0: // Cancel
				// if this happens, the user wants to stop everything
				ReturnCode = ART_Cancel;
				bAttemptSave = FALSE;
				break;
			case 1: // Retry
				bAttemptSave = TRUE;
				break;
			case 2: // Continue
				ReturnCode = ART_No;// this is if it failed to save, but the user wants to skip saving it
				bAttemptSave = FALSE;
				break;
			default:
				// Should not get here
				check(0);
				break;
			}
		}
		else
		{
			// If we were successful at saving, there is no need to attempt to save again
			bAttemptSave = FALSE;
			ReturnCode = ART_Yes;
		}
		
	}

	return ReturnCode;

}

/**
 * Shows a dialog warning a user about packages which failed to save
 * 
 * @param Packages that should be displayed in the dialog
 */
static void WarnUserAboutFailedSave( const TArray<UPackage*>& InFailedPackages )
{
	// Warn the user if any packages failed to save
	if ( InFailedPackages.Num() > 0 )
	{
		FString FailedPackagesMessage = FString::Printf( *LocalizeUnrealEd("FailedSavePrompt_Message") );
		for ( TArray<UPackage*>::TConstIterator FailedIter( InFailedPackages ); FailedIter; ++FailedIter )
		{
			FailedPackagesMessage += FString::Printf( TEXT("%s\n"), *( (*FailedIter)->GetName() ) );
		}

		// Display warning
		WxLongChoiceDialog FailedSaveDlg(
			*FailedPackagesMessage,
			*LocalizeUnrealEd("FailedSavePrompt_Title"),
			WxChoiceDialogBase::Choice( AMT_OK, *LocalizeUnrealEd("OK"), WxChoiceDialogBase::DCT_DefaultCancel ) );

		FailedSaveDlg.ShowModal();
	}
}

/**
 * Looks at all currently loaded packages and saves them if their "bDirty" flag is set, optionally prompting the user to select which packages to save)
 * 
 * @param	bPromptUserToSave			TRUE if we should prompt the user to save dirty packages we found. FALSE to assume all dirty packages should be saved.  Regardless of this setting the user will be prompted for checkout(if needed) unless bFastSave is set
 * @param	bSaveMapPackages			TRUE if map packages should be saved
 * @param	bSaveContentPackages		TRUE if we should save content packages. 
 * @param	bFastSave					TRUE if we should do a fast save. (I.E dont prompt the user to save or checkout and only save packages that are currently writable). 
 * @return								TRUE on success, FALSE on fail.
 */
UBOOL FEditorFileUtils::SaveDirtyPackages(const UBOOL bPromptUserToSave, const UBOOL bSaveMapPackages, const UBOOL bSaveContentPackages, const UBOOL bFastSave )
{
	UBOOL bReturnCode = TRUE;

	// A list of all packages that need to be saved
	TArray<UPackage*> PackagesToSave;

	if( bSaveMapPackages )
	{
		// If we are saving map packages, collect all valid worlds and see if their package is dirty
		TArray<UWorld*> Worlds;
		FLevelUtils::GetWorlds( Worlds, TRUE );

		for( INT WorldIdx = 0; WorldIdx < Worlds.Num(); ++WorldIdx  )
		{
			UPackage* WorldPackage = Worlds( WorldIdx )->GetOutermost();
			if( WorldPackage->IsDirty() && (WorldPackage->PackageFlags & PKG_PlayInEditor) == 0 )
			{
				// IF the package is dirty and its not a pie package, add the world package to the list of packages to save
				PackagesToSave.AddItem( WorldPackage );
			}
		}
	}

	// Don't iterate through content packages if we dont plan on saving them
	if( bSaveContentPackages )
	{
		// Make a list of all content packages that we should save
		for ( TObjectIterator<UPackage> It; It; ++It )
		{
			UPackage*	Package					= *It;
			UBOOL		bShouldIgnorePackage	= FALSE;
			UWorld*		AssociatedWorld			= static_cast<UWorld*>( UObject::StaticFindObject(UWorld::StaticClass(), Package, TEXT("TheWorld")) );
			const UBOOL	bIsMapPackage			= AssociatedWorld != NULL;

			// Only look at root packages.
			bShouldIgnorePackage |= Package->GetOuter() != NULL;
			// Don't try to save "Transient" package.
			bShouldIgnorePackage |= Package == UObject::GetTransientPackage();
			// Ignore PIE packages.
			bShouldIgnorePackage |= (Package->PackageFlags & PKG_PlayInEditor) != 0;
			// Ignore packages that haven't been modified.
			bShouldIgnorePackage |= !Package->IsDirty();
			// Ignore map packages, they are caught above.
			bShouldIgnorePackage |= bIsMapPackage; 

			if( !bShouldIgnorePackage )
			{
				PackagesToSave.AddItem( Package );
			}
		}
	}

	if( PackagesToSave.Num() > 0 ) 
	{
		if( !bFastSave )
		{
			const FEditorFileUtils::EPromptReturnCode Return = FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, TRUE, bPromptUserToSave);
			if( Return == FEditorFileUtils::EPromptReturnCode::PR_Cancelled )
			{
				// Only cancel should return false and stop whatever we were doing before.(like closing the editor)
				// If failure is returned, the user was given ample times to retry saving the package and didn't want to
				// So we should continue with whatever we were doing.  
				bReturnCode = FALSE;
			}
		}
		else
		{
			GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("SavingPackage")), TRUE );

			// Packages that failed to save
			TArray< UPackage* > FailedPackages;

			for ( TArray<UPackage*>::TConstIterator PkgIter( PackagesToSave ); PkgIter; ++PkgIter )
			{
				UPackage* CurPackage = *PkgIter;

				// Check if a file exists for this package
				FString Filename;
				UBOOL bFoundFile = GPackageFileCache->FindPackageFile( *CurPackage->GetName(), NULL, Filename );
				if( bFoundFile )
				{
					// determine if the package file is read only
					const UBOOL bPkgReadOnly = GFileManager->IsReadOnly( *Filename );

					// Only save writable files in fast mode
					if ( !bPkgReadOnly )
					{	
						if( !CurPackage->IsFullyLoaded() )
						{
							// Packages must be fully loaded to save
							CurPackage->FullyLoad();
						}

						// Save the package
						UBOOL bPackageLocallyWritable;
						const INT SaveStatus = InternalSavePackage( CurPackage, bPackageLocallyWritable );

						if( SaveStatus == ART_No )
						{
							// The package could not be saved so add it to the failed array 
							FailedPackages.AddItem( CurPackage );

						}
					}
				}
				
			}
			GWarn->EndSlowTask();

			// Warn the user about any packages which failed to save.
			WarnUserAboutFailedSave( FailedPackages );
		}
	}
	
	return bReturnCode;
}

/**
 * Optionally prompts the user for which of the provided packages should be saved, and then additionally prompts the user to check-out any of
 * the provided packages which are under source control. If the user cancels their way out of either dialog, no packages are saved. It is possible the user
 * will be prompted again, if the saving process fails for any reason. In that case, the user will be prompted on a package-by-package basis, allowing them
 * to retry saving, skip trying to save the current package, or to again cancel out of the entire dialog. If the user skips saving a package that failed to save,
 * the package will be added to the optional OutFailedPackages array, and execution will continue. After all packages are saved (or not), the user is provided with
 * a warning about any packages that were writable on disk but not in source control, as well as a warning about which packages failed to save.
 *
 * @param		PackagesToSave				The list of packages to save.  Both map and content packages are supported 
 * @param		bCheckDirty					If TRUE, only packages that are dirty in PackagesToSave will be saved	
 * @param		bPromptToSave				If TRUE the user will be prompted with a list of packages to save, otherwise all passed in packages are saved
 * @param		OutFailedPackages			[out] If specified, will be filled in with all of the packages that failed to save successfully
 *
 * @return		An enum value signifying success, failure, user declined, or cancellation. If any packages at all failed to save during execution, the return code will be 
 *				failure, even if other packages successfully saved. If the user cancels at any point during any prompt, the return code will be cancellation, even though it
 *				is possible some packages have been successfully saved (if the cancel comes on a later package that can't be saved for some reason). If the user opts the "Don't
 *				Save" option on the dialog, the return code will indicate the user has declined out of the prompt. This way calling code can distinguish between a decline and a cancel
 *				and then proceed as planned, or abort its operation accordingly.
 */
FEditorFileUtils::EPromptReturnCode FEditorFileUtils::PromptForCheckoutAndSave( const TArray<UPackage*>& InPackages, UBOOL bCheckDirty, UBOOL bPromptToSave, TArray<UPackage*>* OutFailedPackages )
{
	// Initialize the value we will return to indicate success
	FEditorFileUtils::EPromptReturnCode ReturnResponse = PR_Success;
	
	// Keep a list of packages that have been filtered to be saved specifically; this could occur as the result of prompting the user
	// for which packages to save or from filtering by whether the package is dirty or not. This method allows us to save loop iterations and array copies.
	TArray<UPackage*> FilteredPackages;
	UBOOL bUseFilteredArray = FALSE;

	// Prompt the user for which packages they would like to save
	if( bPromptToSave )
	{
		// We are relying on the user to specify which packages to save, so we'll be using the filtered array
		bUseFilteredArray = TRUE;

		// Set up the save package dialog
		WxDlgCheckBoxList<UPackage*> SavePkgDlg;
		SavePkgDlg.AddButton( ART_Yes, *LocalizeUnrealEd( TEXT("SavePkgPrompt_SaveButton") ), *LocalizeUnrealEd( TEXT("SavePkgPrompt_SaveButtonTooltip") ) );
		SavePkgDlg.AddButton(ART_No, *LocalizeUnrealEd( TEXT("SavePkgPrompt_DontSaveButton") ), *LocalizeUnrealEd( TEXT("SavePkgPrompt_DontSaveButtonTooltip") ) );
		SavePkgDlg.AddButton( ART_Cancel, *LocalizeUnrealEd( TEXT("SavePkgPrompt_CancelButton") ), *LocalizeUnrealEd( TEXT("SavePkgPrompt_CancelButtonTooltip") ) );

		UBOOL bAddedItemsToDlg = FALSE;
		for ( TArray<UPackage*>::TConstIterator PkgIter( InPackages ); PkgIter; ++PkgIter )
		{
			UPackage* CurPackage = *PkgIter;
			check( CurPackage );

			// If the caller set bCheckDirty to TRUE, only consider dirty packages
			if ( !bCheckDirty || ( bCheckDirty && CurPackage->IsDirty() ) )
			{
				SavePkgDlg.AddCheck( CurPackage, CurPackage->GetName(), wxCHK_CHECKED );
				bAddedItemsToDlg = TRUE;
			}
		}

		if ( bAddedItemsToDlg )
		{
			// If valid packages were added to the dialog, display it to the user
			const INT UserResponse = SavePkgDlg.ShowDialog( *LocalizeUnrealEd( TEXT("SavePkgPrompt_Title") ), *LocalizeUnrealEd( TEXT("SavePkgPrompt_BodyText") ), FIntPoint( 400, 400 ), TEXT("SavePkgPromptDlg")  );

			// If the user has responded yes, they want to save the packages they have checked
			if ( UserResponse == ART_Yes )
			{
				SavePkgDlg.GetResults( FilteredPackages, wxCHK_CHECKED );
			}
			// If the user has responded they don't wish to save, set the response type accordingly
			else if ( UserResponse == ART_No )
			{
				ReturnResponse = PR_Declined;
			}
			// If the user has cancelled from the dialog, set the response type accordingly
			else
			{
				ReturnResponse = PR_Cancelled;
			}
		}
	}
	else
	{
		// The user will not be prompted about which files to save, so consider all provided packages directly
		// (Don't consider non-dirty packages if the caller has specified bCheckDirty as TRUE)
		if ( bCheckDirty )
		{
			// We'll be filtering by the dirty flag, so we'll use the filtered array
			bUseFilteredArray = TRUE;
			for ( TArray<UPackage*>::TConstIterator PkgIter( InPackages ); PkgIter; ++PkgIter )
			{
				UPackage* CurPackage = *PkgIter;
				check( CurPackage );

				if ( CurPackage->IsDirty() )
				{
					FilteredPackages.AddItem( CurPackage );
				}
			}
		}
	}

	// Determine which packages to save based upon the status of bUseFilteredArray
	const TArray<UPackage*>& PackagesToSave = bUseFilteredArray ? FilteredPackages : InPackages;

	// If there are any packages to save and the user didn't decline/cancel, then first prompt to check out any that are under source control, and then
	// go ahead and save the specified packages
	if ( PackagesToSave.Num() > 0 && ReturnResponse == PR_Success )
	{
		TArray<UPackage*> FailedPackages;
		TArray<UPackage*> WritablePackageFiles;

		TArray<UPackage*> PackagesNotNeedingCheckout;

		// Prompt to check-out any packages under source control
		const UBOOL UserResponse = FEditorFileUtils::PromptToCheckoutPackages( FALSE, PackagesToSave, &PackagesNotNeedingCheckout );
		if( UserResponse || PackagesNotNeedingCheckout.Num() > 0 )
		{
			// Even if the user cancelled the checkout dialog, still save packages not needing checkout
			const TArray<UPackage*>& FinalSaveList = ( !UserResponse && PackagesNotNeedingCheckout.Num() > 0 ) ? PackagesNotNeedingCheckout : PackagesToSave;

			const FScopedBusyCursor BusyCursor;
			GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("SavingPackage")), TRUE );
			for( TArray<UPackage*>::TConstIterator PackageIter( FinalSaveList ); PackageIter; ++PackageIter )
			{
				UPackage* Package = *PackageIter;
				
				if( !Package->IsFullyLoaded() )
				{
					// Packages must be fully loaded to save.
					Package->FullyLoad();
				}

				GWarn->StatusUpdatef( PackageIter.GetIndex(), PackagesToSave.Num(), *FString::Printf( LocalizeSecure(LocalizeUnrealEd("SavingPackagef"), *Package->GetName() )) );
				
				// Save the package
				UBOOL bPackageLocallyWritable;
				const INT SaveStatus = InternalSavePackage( Package, bPackageLocallyWritable );
				
				// If InternalSavePackage reported that the provided package was locally writable, add it to the list of writable files
				// to warn the user about
				if ( bPackageLocallyWritable )
				{
					WritablePackageFiles.AddItem( Package );
				}

				if( SaveStatus == ART_No )
				{
					// The package could not be saved so add it to the failed array and change the return response to indicate failure
					FailedPackages.AddItem( Package );
					ReturnResponse = PR_Failure;
				}
				else if( SaveStatus == ART_Cancel )
				{
					// No need to save anything else, the user wants to cancel everything
					ReturnResponse = PR_Cancelled;
					break;
				}
			}
			GWarn->EndSlowTask();

			if( UserResponse == FALSE && PackagesNotNeedingCheckout.Num() > 0 )
			{
				// Return response should still be PR_Cancelled even if the user cancelled the source control dialog but there were writable packages we could save.
				// This is in case the save is happing during editor exit. We don't want to shutdown the editor if some packages failed to save.
				ReturnResponse = PR_Cancelled;
			}

			// If any packages were saved that weren't actually in source control but instead forcibly made writable,
			// then warn the user about those packages
			if( WritablePackageFiles.Num() > 0 )
			{
				FString WritableFiles;
				for( TArray<UPackage*>::TIterator PackageIter( WritablePackageFiles ); PackageIter; ++PackageIter )
				{
					// A warning message was created.  Try and show it.
					WritableFiles += FString::Printf( TEXT("\n%s"), *(*PackageIter)->GetName() );
				}

				const FString WritableFileWarning = FString::Printf( LocalizeSecure( LocalizeUnrealEd("Warning_WritablePackagesNotCheckedOut"), *WritableFiles ) );
				WxSuppressableWarningDialog PromptForWritableFiles( *WritableFileWarning, *LocalizeUnrealEd("Warning_WritablePackagesNotCheckedOutTitle"), "WritablePackagesNotCheckedOut" );
				PromptForWritableFiles.ShowModal();
			}

			// Warn the user if any packages failed to save
			if ( FailedPackages.Num() > 0 )
			{
				// Set the failure array to have the same contents as the local one.
				// The local one is required so we can always display the error, even if an array is not provided.
				if ( OutFailedPackages )
				{
					*OutFailedPackages = FailedPackages;
				}

				// Show a dialog for the failed packages
				WarnUserAboutFailedSave( FailedPackages );
			}
		}
		else
		{
			// The user cancelled the checkout dialog, so set the return response accordingly  
			ReturnResponse = PR_Cancelled;
		}

	}

	return ReturnResponse;
}

/**
 * Save all packages corresponding to the specified UWorlds, with the option to override their path and also
 * apply a prefix.
 *
 * @param	WorldsArray		The set of UWorlds to save.
 * @param	OverridePath	Path override, can be NULL
 * @param	Prefix			Optional prefix for base filename, can be NULL
 * @param	bIncludeGWorld	If TRUE, save GWorld along with other worlds.
 * @param	bCheckDirty		If TRUE, don't save level packages that aren't dirty.
 * @param	bPIESaving		Should be set to TRUE if saving for PIE; passed to UWorld::SaveWorld.
 * @return					TRUE if at least one level was saved.
 *							If bPIESaving, will be TRUE is ALL worlds were saved.
 */
UBOOL appSaveWorlds(const TArray<UWorld*>& WorldsArray, const TCHAR* OverridePath, const TCHAR* Prefix, UBOOL bIncldudeGWorld, UBOOL bCheckDirty, UBOOL bPIESaving)
{
	const FScopedBusyCursor BusyCursor;

	// Update cull distance volumes (and associated primitives).
	GWorld->UpdateCullDistanceVolumes();

	// Apply level prefixes.
	FString WorldPackageName = GWorld->GetOutermost()->GetName();
	if ( Prefix )
	{
		WorldPackageName = FString(Prefix) + WorldPackageName;

		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		for ( INT LevelIndex=0; LevelIndex<WorldInfo->StreamingLevels.Num(); LevelIndex++ )
		{
			ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
			if ( StreamingLevel )
			{
				// Apply prefix so this level references the soon to be saved other world packages.
				if( StreamingLevel->PackageName != NAME_None )
				{
					StreamingLevel->PackageName = FName( *(FString(Prefix) + StreamingLevel->PackageName.ToString()) );
				}
			}
		}

		// make sure the prefix is set for all saving operations 
		GCrossLevelReferenceManager->SetPIEPrefix(Prefix);

		// cross level imports need to be renamed to match the prefix'ed map names
		for ( INT WorldIndex = 0 ; WorldIndex < WorldsArray.Num() ; ++WorldIndex )
		{
			// go over all the levels that this world references by cross level
			UPackage* WorldPackage = WorldsArray(WorldIndex)->GetOutermost();
			for (INT LevelImportIndex = 0; LevelImportIndex < WorldPackage->ImportGuids.Num(); LevelImportIndex++)
			{
				FLevelGuids& LevelGuids = WorldPackage->ImportGuids(LevelImportIndex);

				// prepend the prefix to the level import's name
				LevelGuids.LevelName = FName(*(FString(Prefix) + LevelGuids.LevelName.ToString()) );
			}
		}
	}

	// Save all packages containing levels that are currently "referenced" by the global world pointer.
	UBOOL bSavedAll = TRUE;
	UBOOL bAtLeastOneLevelWasSaved = FALSE;
	FString FinalFilename;
	for ( INT WorldIndex = 0 ; WorldIndex < WorldsArray.Num() ; ++WorldIndex )
	{
		UWorld* World = WorldsArray(WorldIndex);
		const UBOOL bLevelWasSaved = SaveWorld( World, NULL,
												OverridePath, Prefix,
												FALSE, TRUE, bCheckDirty,
												FinalFilename,
												FALSE, bPIESaving );
		if ( bLevelWasSaved )
		{
			bAtLeastOneLevelWasSaved = TRUE;
		}
		else
		{
			bSavedAll = FALSE;
		}
	}

	// Remove prefix from referenced level names in AWorldInfo.
	if ( Prefix )
	{
		GCrossLevelReferenceManager->SetPIEPrefix(TEXT(""));

		// Iterate over referenced levels and remove prefix.
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		for ( INT LevelIndex=0; LevelIndex<WorldInfo->StreamingLevels.Num(); LevelIndex++ )
		{
			ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
			if( StreamingLevel && StreamingLevel->PackageName != NAME_None )
			{
				const FString PrefixedName = StreamingLevel->PackageName.ToString();
				StreamingLevel->PackageName = FName( *PrefixedName.Right( PrefixedName.Len() - appStrlen( Prefix ) ) );
			}
		}

		// fix up the cross level import level names to their original values
		for ( INT WorldIndex = 0 ; WorldIndex < WorldsArray.Num() ; ++WorldIndex )
		{
			// go over all the levels that this world references by cross level
			UPackage* WorldPackage = WorldsArray(WorldIndex)->GetOutermost();
			for (INT LevelImportIndex = 0; LevelImportIndex < WorldPackage->ImportGuids.Num(); LevelImportIndex++)
			{
				FLevelGuids& LevelGuids = WorldPackage->ImportGuids(LevelImportIndex);

				const FString PrefixedName = LevelGuids.LevelName.ToString();
				LevelGuids.LevelName = FName( *PrefixedName.Right( PrefixedName.Len() - appStrlen( Prefix ) ) );
			}
		}
	}

	return (bPIESaving ? bSavedAll : bAtLeastOneLevelWasSaved);
}

/**
 * Save all packages containing UWorld objects with the option to override their path and also
 * apply a prefix.
 *
 * @param	OverridePath	Path override, can be NULL
 * @param	Prefix			Optional prefix for base filename, can be NULL
 * @param	bIncludeGWorld	If TRUE, save GWorld along with other worlds.
 * @param	bCheckDirty		If TRUE, don't save level packages that aren't dirty.
 * @param	bSaveOnlyWritable	If true, only save files that are writable on disk.
 * @param	bPIESaving		Should be set to TRUE if saving for PIE; passed to UWorld::SaveWorld.
 * @return					TRUE if at least one level was saved.
 */
UBOOL appSaveAllWorlds(const TCHAR* OverridePath, const TCHAR* Prefix, UBOOL bIncludeGWorld, UBOOL bCheckDirty ,UBOOL bSaveOnlyWritable, UBOOL bPIESaving, FEditorFileUtils::EGarbageCollectionOption Option)
{
	if( Option == FEditorFileUtils::GCO_CollectGarbage )
	{
		// Clean up any old worlds.
		UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
	}

	// Get the set of all reference worlds.
	TArray<UWorld*> WorldsArray;
	FLevelUtils::GetWorlds( WorldsArray, bIncludeGWorld );

	TArray<UWorld*> WorldsToSave;
	if( bSaveOnlyWritable )
	{
		// Find all worlds in packages that have writable files.
		FString Filename;
		for( INT WorldIdx = 0; WorldIdx < WorldsArray.Num(); ++WorldIdx )
		{
			UWorld* CurWorld = WorldsArray( WorldIdx );
			if( GPackageFileCache->FindPackageFile( *CurWorld->GetOutermost()->GetName(), NULL, Filename ) )
			{
				// If the package has a filename, check if its read only
				if( !GFileManager->IsReadOnly( *Filename ) )
				{
					// if its not read only it can be saved.
					WorldsToSave.AddItem( CurWorld );
				}
			}

		}
	}
	else
	{
		WorldsToSave = WorldsArray;
	}

	return appSaveWorlds( WorldsToSave, OverridePath, Prefix, bIncludeGWorld, bCheckDirty, bPIESaving );
}

/**
 * Saves all levels associated with GWorld.
 *
 * @param	bCheckDirty		If TRUE, don't save level packages that aren't dirty.
 */
void FEditorFileUtils::SaveAllWritableLevels( UBOOL bCheckDirty )
{
	// Disallow the save if in interpolation editing mode and the user doesn't want to exit interpolation mode.
	if ( !InInterpEditMode() )
	{
		const UBOOL bSaveOnlyWritable = TRUE;

		// Special case for the persistent level which may not yet have been saved and thus
		// have no filename associated with it.
		const FFilename Filename = GetFilename( GWorld );
		if( ( bCheckDirty && GWorld->GetOutermost()->IsDirty() || !bCheckDirty ) 
			&& Filename.Len() > 0 && 
			!GFileManager->IsReadOnly( *Filename ) )
		{
			const UBOOL bAddToMRUList = TRUE;
			FEditorFileUtils::SaveMap( GWorld, Filename, bAddToMRUList );
		}

		// Check other levels.
		if ( appSaveAllWorlds( NULL, NULL, FALSE, bCheckDirty, bSaveOnlyWritable, FALSE, FEditorFileUtils::GCO_SkipGarbageCollection ) )
		{
			// Refresh the level browser now that a levels' dirty flag has been unset.
			GCallbackEvent->Send( CALLBACK_RefreshEditor_LevelBrowser );

			GEditor->ResetAutosaveTimer();
		}
	}
}

/**
 * Checks to see if a filename is valid for saving.
 * A filename must be under MAX_UNREAL_FILENAME_LENGTH to be saved
 *
 * @param Filename	Filename, with or without path information, to check.
 * @param OutError	If an error occurs, this is the reason why
 */
UBOOL FEditorFileUtils::IsFilenameValidForSaving( const FString& Filename, FString& OutError )
{
	UBOOL bFilenameIsValid = FALSE;

	// Get the clean filename (filename with extension but without path )
	FFilename FileInfo = Filename;
	const FString BaseFilename = FileInfo.GetBaseFilename();

	// Check length of the filename, If we're currently cooking for Windows, then we won't fail due to long file name length
	if( ( BaseFilename.Len() > 0 ) && 
		  ( BaseFilename.Len() <= MAX_UNREAL_FILENAME_LENGTH || ( GIsCooking && GCookingTarget == UE3::PLATFORM_Windows ) ) )
	{
		bFilenameIsValid = TRUE;

		// Check that the name isn't the name of a UClass
		for ( TObjectIterator<UClass> It; It; ++It )
		{
			UClass* Class = *It;
			if ( Class->GetName() == BaseFilename )
			{
				bFilenameIsValid = FALSE;
				break;
			}
		}
		if( !bFilenameIsValid )
		{
			OutError += FString::Printf( LocalizeSecure( LocalizeUnrealEd( "Error_FilenameDisallowed" ), *BaseFilename ) );
		}
	}
	else
	{
		OutError += FString::Printf( LocalizeSecure( LocalizeUnrealEd( "Error_FilenameIsTooLongForCooking" ), *BaseFilename, FEditorFileUtils::MAX_UNREAL_FILENAME_LENGTH ) );
	}

	return bFilenameIsValid;
}



/**
 * Static: Returns the simple map file name, or an empty string if no simple map is set for this game
 *
 * @return	The name of the simple map
 */
FString FEditorFileUtils::GetSimpleMapName()
{
	FString SimpleMapName;
	GConfig->GetString( TEXT("UnrealEd.SimpleMap"), TEXT("SimpleMapName"), SimpleMapName, GEditorIni );
	return SimpleMapName;
}


/** Loads the simple map for UDK */
void FEditorFileUtils::LoadSimpleMapAtStartup ()
{
//	bIsLoadingSimpleStartupMap = TRUE;
	FEditorFileUtils::LoadMap( GetSimpleMapName() );
//	bIsLoadingSimpleStartupMap = FALSE;
}


