#include "UnrealEd.h"

#include "UnLinkedObjEditor.h"
#include "Kismet.h"

extern class WxUnrealEdApp* GApp;

/*-----------------------------------------------------------------------------
	FileHelper.
-----------------------------------------------------------------------------*/

FFileHelper::FFileHelper()
{
}

FFileHelper::~FFileHelper()
{
}

void FFileHelper::Load()
{
	AskSaveChanges();

	wxFileDialog dlg( GApp->EditorFrame, TEXT("Open"), *GetDefaultDirectory(), TEXT(""), *GetFilter(FHI_Load), wxOPEN | wxFILE_MUST_EXIST | wxHIDE_READONLY );
	if( dlg.ShowModal() == wxID_OK )
		LoadFile( FString( dlg.GetPath() ) );

	GFileManager->SetDefaultDirectory();
}

void FFileHelper::Import( UBOOL InFlag1 )
{
	AskSaveChanges();

	wxFileDialog dlg( GApp->EditorFrame, TEXT("Import"), *GetDefaultDirectory(), TEXT(""), *GetFilter(FHI_Import), wxOPEN | wxFILE_MUST_EXIST | wxHIDE_READONLY );
	if( dlg.ShowModal() == wxID_OK )
		ImportFile( FString( dlg.GetPath() ), InFlag1 );

	GFileManager->SetDefaultDirectory();
}

void FFileHelper::SaveAs()
{
	wxFileDialog dlg( GApp->EditorFrame, TEXT("Save As"), *GetDefaultDirectory(), *GetFilename().GetCleanFilename(), *GetFilter(FHI_Save), wxSAVE | wxHIDE_READONLY | wxOVERWRITE_PROMPT );
	if( dlg.ShowModal() == wxID_OK )
		SaveFile( FString( dlg.GetPath() ) );
}

void FFileHelper::Save()
{
	CheckSaveAs();
}

void FFileHelper::Export( UBOOL InFlag1 )
{
	wxFileDialog dlg( GApp->EditorFrame, TEXT("Export"), *GetDefaultDirectory(), *GetFilename().GetBaseFilename(), *GetFilter(FHI_Export), wxSAVE | wxHIDE_READONLY | wxOVERWRITE_PROMPT );
	if( dlg.ShowModal() == wxID_OK )
	{
		ExportFile( FString( dlg.GetPath() ), InFlag1 );
	}
}

// Checks to see if the file is dirty and if so, asks the user if they want to save it.

void FFileHelper::AskSaveChanges()
{
	if( IsDirty() )
	{
		FString Question = FString::Printf( TEXT("Do you want to save the changes to %s?"), ( GetFilename().Len() ? *GetFilename().GetBaseFilename() : TEXT("Untitled") ) );
		if( XWindow_AskQuestion(Question) == wxID_YES )
		{
			CheckSaveAs();
		}
	}
}

// Checks if we have a valid filename to save the file as.

void FFileHelper::CheckSaveAs()
{
	if( !GetFilename().Len() )
		SaveAs();
	else
		SaveFile( GetFilename() );
}

FString FFileHelper::GetFilter( EFileHelperIdx InIdx )
{
	return TEXT("All files (*.*)|*.*");
}

/*-----------------------------------------------------------------------------
	FileHelperMap.
-----------------------------------------------------------------------------*/

FFileHelperMap::FFileHelperMap()
{
}

FFileHelperMap::~FFileHelperMap()
{
}

void FFileHelperMap::LoadFile( FFilename InFilename )
{
	// Change out of Matinee when opening new map, so we avoid editing data in the old one.
	if( GEditorModeTools.GetCurrentModeID() == EM_InterpEdit )
	{
		GApp->SetCurrentMode( EM_Default );
	}

	// Before opening new file, close any windows onto existing level.
	WxKismet::CloseAllKismetWindows();

	GApp->EditorFrame->SetMapFilename( *InFilename );
	GUnrealEd->Exec( *FString::Printf( TEXT("MAP LOAD FILE=\"%s\""), *InFilename ) );

	GApp->LastDir[LD_UNR] = InFilename.GetPath();

	GApp->EditorFrame->MainMenuBar->MRUFiles->AddItem( *InFilename );

	// Restore camera views to current viewports
	for( INT i=0 ; i<4; i++)
	{
		WxLevelViewportWindow* VC = GApp->EditorFrame->ViewportConfigData->Viewports[i].ViewportWindow;
		if( VC && (VC->GetLevel() == GUnrealEd->Level) && (VC->ViewportType != LVT_None) )
		{
			check(VC->ViewportType >= 0 && VC->ViewportType < 4);

			VC->ViewLocation = GUnrealEd->Level->EditorViews[VC->ViewportType].CamPosition;
			VC->ViewRotation = GUnrealEd->Level->EditorViews[VC->ViewportType].CamRotation;
			VC->OrthoZoom = GUnrealEd->Level->EditorViews[VC->ViewportType].CamOrthoZoom;
		}
	}

	GCallback->Send( CALLBACK_RefreshEditor, ERefreshEditor_Misc | ERefreshEditor_AllBrowsers | ERefreshEditor_ActorBrowser );
}

// InFlag1 = Merge with existing map?

void FFileHelperMap::ImportFile( FFilename InFilename, UBOOL InFlag1 )
{
	GWarn->BeginSlowTask( TEXT("Importing Map"), 1 );

	if( InFlag1 )
		GUnrealEd->Exec( *FString::Printf( TEXT("MAP IMPORTADD FILE=\"%s\""), *InFilename ) );
	else
	{
		GApp->EditorFrame->SetMapFilename( TEXT("") );
		GUnrealEd->Exec( *FString::Printf( TEXT("MAP IMPORT FILE=\"%s\""), *InFilename ) );
	}

	GWarn->EndSlowTask();

	GUnrealEd->RedrawLevel( GUnrealEd->Level );

	GApp->LastDir[LD_UNR] = InFilename.GetPath();

	GCallback->Send( CALLBACK_RefreshEditor, ERefreshEditor_Misc | ERefreshEditor_AllBrowsers | ERefreshEditor_ActorBrowser );
}

void FFileHelperMap::SaveFile( FFilename InFilename )
{
	// Must exit Interpolation Editing mode before you can save - so it can reset everything to its initial state.
	if( GEditorModeTools.GetCurrentModeID() == EM_InterpEdit )
	{
		UBOOL ExitInterp = appMsgf( 1, TEXT("You must exit Interpolation Editing mode before saving level.\nDo you wish to do this now and continue?") );
		if(!ExitInterp)
			return;

		GApp->SetCurrentMode( EM_Default );
	}

	// Save the current viewport states.
	for(INT i=0; i<4; i++)
	{
		GUnrealEd->Level->EditorViews[i] = FLevelViewportInfo( FVector(0), FRotator(0,0,0), 10000.0f );
	}


	// Iterate over viewports and save position.
	for(INT i=0; i<4; i++)
	{
		WxLevelViewportWindow* VC = GApp->EditorFrame->ViewportConfigData->Viewports[i].ViewportWindow;
		if(VC && (VC->ViewportType != LVT_None))
		{
			check(VC->ViewportType >= 0 && VC->ViewportType < 4);
			GUnrealEd->Level->EditorViews[VC->ViewportType] = FLevelViewportInfo( VC->ViewLocation, VC->ViewRotation, VC->OrthoZoom );	
		}
	}

	// Set the map filename
	GApp->EditorFrame->SetMapFilename( *InFilename );

	GUnrealEd->Exec( TEXT("POLYGON DELETE") );
	GUnrealEd->Exec( *FString::Printf( TEXT("MAP SAVE FILE=\"%s\""), *InFilename ) );
	GUnrealEd->GetLevel()->MarkPackageDirty( 0 );

	GApp->EditorFrame->MainMenuBar->MRUFiles->AddItem( InFilename );
	GApp->LastDir[LD_UNR] = InFilename.GetPath();

	GUnrealEd->Trans->Reset( TEXT("Map Saved") );

	GFileManager->SetDefaultDirectory();
}

// InFlag1 = Export selected actors only?

void FFileHelperMap::ExportFile( FFilename InFilename, UBOOL InFlag1 )
{
	GUnrealEd->Exec( TEXT("POLYGON DELETE") );
	GUnrealEd->Exec( *FString::Printf(TEXT("MAP EXPORT FILE=\"%s\" SELECTEDONLY=%d"), *InFilename, InFlag1 ) );

	GApp->LastDir[LD_UNR] = InFilename.GetPath();
}

void FFileHelperMap::NewFile()
{
	AskSaveChanges();

	// Change out of Matinee when opening new map, so we avoid editing data in the old one.
	if( GEditorModeTools.GetCurrentModeID() == EM_InterpEdit )
	{
		GApp->SetCurrentMode( EM_Default );
	}

	// Before opening new file, close any windows onto existing level.
	WxKismet::CloseAllKismetWindows();

	GUnrealEd->Exec( TEXT("MAP NEW") );
}

UBOOL FFileHelperMap::IsDirty()
{
	return ((UPackage*)GUnrealEd->GetLevel()->GetOutermost())->bDirty;
}

FString FFileHelperMap::GetDefaultDirectory()
{
	return GApp->LastDir[LD_UNR];
}

FFilename FFileHelperMap::GetFilename()
{
	return GApp->EditorFrame->MapFilename;
}

FString FFileHelperMap::GetFilter( EFileHelperIdx InIdx )
{
	switch( InIdx )
	{
		case FHI_Load:
		case FHI_Save:
			return FString::Printf( TEXT("Map files (*.%s)|*.%s|All files (*.*)|*.*"), *GApp->MapExt, *GApp->MapExt );

		case FHI_Import:
			return TEXT("Unreal Text (*.t3d)|*.t3d|All Files|*.*");

		case FHI_Export:
			return TEXT("Supported formats (*.t3d,*.stl)|*.t3d;*.stl|Unreal Text (*.t3d)|*.t3d|Stereo Litho (*.stl)|*.stl|Object (*.obj)|*.obj|All Files|*.*");
	}

	check(0);	// Unknown index

	return TEXT("");
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
