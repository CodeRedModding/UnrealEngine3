/*=============================================================================
	UnrealEdEngine.cpp: UnrealEd engine functions.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#include "UnrealEd.h"

void UUnrealEdEngine::ShowActorProperties()
{
	// See if we have any unlocked property windows available.  If not, create a new one.

	INT x;

	for( x = 0 ; x < ActorProperties.Num() ; ++x )
	{
		if( !ActorProperties(x)->IsLocked() )
		{
			ActorProperties(x)->Show();
			break;
		}
	}

	// If no unlocked property windows are available, create a new one

	if( x == ActorProperties.Num() )
	{
		WxPropertyWindowFrame* pw = new WxPropertyWindowFrame( GApp->EditorFrame, -1, 1, this );
		ActorProperties.AddItem( pw );
		UpdatePropertyWindows();
		pw->Show();
	}
}

void UUnrealEdEngine::ShowLevelProperties()
{
	if( !LevelProperties )
		LevelProperties = new WxPropertyWindowFrame( GApp->EditorFrame, -1, 0, this );
	LevelProperties->PropertyWindow->SetObject( Level->Actors(0), 0,1,1 );
	LevelProperties->Show();
}

void UUnrealEdEngine::ShowClassProperties( UClass* Class )
{
	WClassProperties* ClassProperties = new WClassProperties( TEXT("ClassProperties"), CPF_Edit, *FString::Printf(TEXT("Default %s Properties"),*Class->GetPathName()), Class );
	ClassProperties->OpenWindow( (HWND)GApp->EditorFrame->GetHandle() );
	ClassProperties->SetNotifyHook( this );
	ClassProperties->ForceRefresh();
	ClassProperties->Show(1);
}

void UUnrealEdEngine::ShowSoundProperties( USoundNodeWave* Sound )
{
	WxPropertyWindowFrame* Properties = new WxPropertyWindowFrame( GApp->EditorFrame, -1, 0, this );
	Properties->bAllowClose = 1;
	Properties->PropertyWindow->SetObject( Sound, 1,1,1 );
	Properties->SetTitle( *FString::Printf( TEXT("Sound %s"), Sound->GetPathName() ) );
	Properties->Show();
}

void UUnrealEdEngine::ShowUnrealEdContextMenu()
{
	WxMainContextMenu* ContextMenu = new WxMainContextMenu();

	FTrackPopupMenu tpm( GApp->EditorFrame, ContextMenu );
	tpm.Show();
	delete ContextMenu;
}

void UUnrealEdEngine::ShowUnrealEdContextSurfaceMenu()
{
	WxMainContextSurfaceMenu* SurfaceMenu = new WxMainContextSurfaceMenu();

	FTrackPopupMenu tpm( GApp->EditorFrame, SurfaceMenu );
	tpm.Show();
	delete SurfaceMenu;
}

void UUnrealEdEngine::RedrawLevel(ULevel* Level)
{
	if( GApp->EditorFrame->ViewportConfigData )
	{
		for( INT x = 0 ; x < 4 ; x++ )
		{
			if( GApp->EditorFrame->ViewportConfigData->Viewports[x].bEnabled )
			{
				GApp->EditorFrame->ViewportConfigData->Viewports[x].ViewportWindow->Viewport->Invalidate();
			}
		}
	}
}

void UUnrealEdEngine::SetCurrentClass( UClass* InClass )
{
	GSelectionTools.SelectNone<UClass>();
	GSelectionTools.Select( InClass );
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
 * Looks at all currently loaded packages and prompts the user to save them
 * if their "bDirty" flag is set.
 */
void UUnrealEdEngine::SaveDirtyPackages()
{
	FString FileTypes( TEXT("All Files|*.*") );
	for(INT i=0; i<GSys->Extensions.Num(); i++)
		FileTypes += FString::Printf( TEXT("|(*.%s)|*.%s"), *GSys->Extensions(i), *GSys->Extensions(i) );

	// Iterate over all loaded top packages and see whether they have been modified.
	for( TObjectIterator<UPackage> It; It; ++It )
	{
		UPackage*	Package			= *It;
		UBOOL		IgnorePackage	= 0;

		// Only look at root packages.
		IgnorePackage |= Package->GetOuter() != NULL;
		// Ignore packages that haven't been modified.
		IgnorePackage |= !Package->bDirty;
		// Don't try to save "MyLevel" or "Transient" package.
		IgnorePackage |= appStricmp( Package->GetName(), TEXT("MyLevel") ) == 0;
		IgnorePackage |= appStricmp( Package->GetName(), TEXT("Transient") ) == 0;
			
		if( !IgnorePackage )
		{
			FString ExistingFile, Filename, Directory;

			if( GPackageFileCache->FindPackageFile( Package->GetName(), NULL, ExistingFile ) )
			{
				FString BaseFilename, Extension;
				GPackageFileCache->SplitPath( *ExistingFile, Directory, BaseFilename, Extension );
				Filename = FString::Printf( TEXT("%s.%s"), *BaseFilename, *Extension );
			}
			else
			{
				Directory = *GFileManager->GetCurrentDirectory();
				Filename = FString::Printf( TEXT("%s.upk"), Package->GetName() );
			}
		
			if( appMsgf( 1, TEXT("The package '%s' (%s) has been changed and needs to be saved.  Save it now?"), Package->GetName(), *Filename ) )
			{
				wxFileDialog SaveFileDialog( NULL, TEXT("Save Package"), *Directory, *Filename,	*FileTypes, wxSAVE,	wxDefaultPosition );
				FString SaveFileName;

				if( SaveFileDialog.ShowModal() == wxID_OK )
				{
					SaveFileName = FString( SaveFileDialog.GetPath() );

					if( GFileManager->IsReadOnly( *SaveFileName ) )
					{
						appMsgf( 0, TEXT("Couldn't save package - maybe file is read-only?") );
						continue;
					}

					GUnrealEd->Exec( *FString::Printf(TEXT("OBJ SAVEPACKAGE PACKAGE=\"%s\" FILE=\"%s\" SILENT=TRUE"), Package->GetName(), *SaveFileName) );
				}
			}
		}
	}
		
	GFileManager->SetDefaultDirectory();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
