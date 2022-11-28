#include "UnrealEd.h"
#include "FFeedbackContextWindows.h"
#include "..\..\Launch\Resources\resource.h"

WxUnrealEdApp*		GApp;
UUnrealEdEngine*	GUnrealEd;
WxDlgAddSpecial*	GDlgAddSpecial;

bool WxUnrealEdApp::OnInit()
{
	WxLaunchApp::OnInit();

	GApp = this;

	// Get the editor
	EditorFrame = new WxEditorFrame( NULL, -1, TEXT("UnrealEd"), wxDefaultPosition, wxDefaultSize );
	EditorFrame->Maximize();
	
	// Init windowing.
	IMPLEMENT_WINDOWCLASS(WEditorFrame,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
	IMPLEMENT_WINDOWCLASS(W2DShapeEditor,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
	IMPLEMENT_WINDOWCLASS(WBrowser,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
	IMPLEMENT_WINDOWCLASS(WBrowserGroup,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
	IMPLEMENT_WINDOWCLASS(WPhAT,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
	IMPLEMENT_WINDOWCLASS(WBuildPropSheet,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
	IMPLEMENT_WINDOWCLASS(WPageOptions,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);
	IMPLEMENT_WINDOWCLASS(WPageLevelStats,CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW);

	// Windows.
	OldEditorFrame = new WEditorFrame;
	OldEditorFrame->OpenWindow();
	OldEditorFrame->MoveWindow( 0, 0, 5, 5, 1 );
	OldEditorFrame->Show(0);

	GEditorModeTools.Init();
	GEditorModeTools.FindMode( EM_Default )->ModeBar = new WxModeBarDefault( (wxWindow*)GApp->EditorFrame, -1 );
	GEditorModeTools.FindMode( EM_TerrainEdit )->ModeBar = new WxModeBarTerrainEdit( (wxWindow*)GApp->EditorFrame, -1 );
	GEditorModeTools.FindMode( EM_Geometry )->ModeBar = new WxModeBarGeometry( (wxWindow*)GApp->EditorFrame, -1 );
	GEditorModeTools.FindMode( EM_Texture )->ModeBar = new WxModeBarTexture( (wxWindow*)GApp->EditorFrame, -1 );

	GEditorModeTools.Modes.AddItem( new FEdModeInterpEdit() );
	GEditorModeTools.FindMode( EM_InterpEdit )->ModeBar = new WxModeBarInterpEdit( GApp->EditorFrame, -1 );

	// Global dialogs

	GDlgAddSpecial = new WxDlgAddSpecial();

	// Main window
	GUnrealEd->hWndMain = OldEditorFrame->hWnd;

	// Set up autosave timer.  We ping the engine once a minute and it determines when and
	// how to do the autosave.
	SetTimer( OldEditorFrame->hWnd, 900, 60000, NULL);

	// Initialize "last dir" array

	GApp->LastDir[LD_UNR]		= GConfig->GetStr( TEXT("Directories"), TEXT("UNR"),		GEditorIni );
	GApp->LastDir[LD_BRUSH]		= GConfig->GetStr( TEXT("Directories"), TEXT("BRUSH"),		GEditorIni );
	GApp->LastDir[LD_2DS]		= GConfig->GetStr( TEXT("Directories"), TEXT("2DS"),		GEditorIni );
	GApp->LastDir[LD_PSA]		= GConfig->GetStr( TEXT("Directories"), TEXT("PSA"),		GEditorIni );
	GApp->LastDir[LD_GENERIC]	= GConfig->GetStr( TEXT("Directories"), TEXT("GENERIC"),	GEditorIni );

	if( !GConfig->GetString( TEXT("URL"), TEXT("MapExt"), GApp->MapExt, GEngineIni ) )
		appErrorf(TEXT("MapExt needs to be defined in .ini!"));
	GUnrealEd->Exec( *FString::Printf(TEXT("MODE MAPEXT=%s"), *GApp->MapExt ) );

	// Init the editor tools.
	GRebuildTools.Init();
	GTexAlignTools.Init();

	EditorFrame->ButtonBar = new WxButtonBar;
	EditorFrame->ButtonBar->Create( (wxWindow*)EditorFrame, -1 );
	EditorFrame->ButtonBar->Show();

	DlgSearchActors = new WDlgSearchActors( NULL, OldEditorFrame );
	DlgSearchActors->DoModeless(0);

	DlgMapCheck = new WDlgMapCheck( NULL, OldEditorFrame );
	DlgMapCheck->DoModeless(0);
	//joegtemp -- Set the handle to use for GWarn->MapCheck_Xxx() calls
	//@hack - this relies on GWarn being of type FFeedbackContextEditor!!!
	((FFeedbackContextEditor*) GWarn)->hWndMapCheckDlg = (DWORD)DlgMapCheck->hWnd;
	((FFeedbackContextEditor*) GWarn)->winEditorFrame = (DWORD)EditorFrame;

	DlgLoadErrors = new WDlgLoadErrors( NULL, OldEditorFrame );
	DlgLoadErrors->DoModeless(0);

	BuildSheet = new WBuildPropSheet( TEXT("Build Options"), OldEditorFrame );
	BuildSheet->OpenWindow();
	BuildSheet->Show( FALSE );

	DlgSurfProp = new WxDlgSurfaceProperties();
	DlgSurfProp->Show( 0 );

	ObjectPropertyWindow = NULL;

	// New browsers

	BrowserGroup = new WBrowserGroup( TEXT("Group Browser"), OldEditorFrame, OldEditorFrame->hWnd );
	BrowserGroup->OpenWindow( 1 );

	ExitSplash();

	EditorFrame->OptionProxies = new TMap<INT,UOptionsProxy*>;
	EditorFrame->OptionProxyInit();

	ShowWindow( OldEditorFrame->hWnd, SW_SHOWMAXIMIZED );

	// Resources

	MaterialEditor_RightHandleOn	= CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.MaterialEditor_RightHandleOn") ));
	MaterialEditor_RightHandle		= CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.MaterialEditor_RightHandle") ));
	MaterialEditor_RGBA				= CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.MaterialEditor_RGBA") ));
	MaterialEditor_R				= CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.MaterialEditor_R") ));
	MaterialEditor_G				= CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.MaterialEditor_G") ));
	MaterialEditor_B				= CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.MaterialEditor_B") ));
	MaterialEditor_A				= CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.MaterialEditor_A") ));
	MaterialEditor_LeftHandle		= CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.MaterialEditor_LeftHandle") ));
	MaterialEditor_ControlPanelFill = CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.MaterialEditor_ControlPanelFill") ));
	MaterialEditor_ControlPanelCap	= CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.MaterialEditor_ControlPanelCap") ));
	UpArrow							= CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.MaterialEditor_Up") ));
	DownArrow						= CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.MaterialEditor_Down") ));
	MaterialEditor_LabelBackground	= CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.MaterialEditor_LabelBackground") ));
	MaterialEditor_Delete			= CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.MaterialEditor_Delete") ));

	UClass* Parent = UMaterialExpression::StaticClass();
	INT ID = IDM_NEW_SHADER_EXPRESSION_START;

	if( Parent )
	{
		for(TObjectIterator<UClass> It;It;++It)
			if(It->IsChildOf(UMaterialExpression::StaticClass()) && !(It->ClassFlags & CLASS_Abstract))
			{
				ShaderExpressionMap.Set( ID, *It );
				ID++;
			}
	}

	// Do final set up on the editor frame and show it

	EditorFrame->SetUp();
	EditorFrame->Show();

	// Load the viewport configuration from the INI file.

	EditorFrame->ViewportConfigData = new FViewportConfig_Data;
	if( !EditorFrame->ViewportConfigData->LoadFromINI() )
	{
		EditorFrame->ViewportConfigData->SetTemplate( VC_2_2_Split );
		EditorFrame->ViewportConfigData->Apply( EditorFrame->ViewportContainer );
	}

	OldEditorFrame->Show( 0 );

	SetCurrentMode( EM_Default );

	TwoDeeShapeEditor = NULL;

	EditorFrame->DockingRoot->ShowDockableWindow( DWT_GENERIC_BROWSER );

	return 1;
}

int WxUnrealEdApp::OnExit()
{
	GRebuildTools.Shutdown();

	if( GLogConsole )
		GLogConsole->Show( FALSE );

	//delete EditorFrame->OptionProxies;

	KillTimer( OldEditorFrame->hWnd, 900 );

	delete OldEditorFrame;

	return WxLaunchApp::OnExit();

}

// Activates the appropriate browser for the type of resource being browsed

void WxUnrealEdApp::CB_Browse()
{
	FStringOutputDevice	GetPropResult;
	GUnrealEd->Get( TEXT("OBJ"), TEXT("BROWSECLASS"), GetPropResult );

	if( !appStrcmp( *GetPropResult, TEXT("Class") ) )
		EditorFrame->DockingRoot->ShowDockableWindow( DWT_ACTOR_BROWSER );
	else if( !appStrcmp( *GetPropResult, TEXT("Animation") ) )
		EditorFrame->DockingRoot->ShowDockableWindow( DWT_ANIMATION_BROWSER );
	else
		EditorFrame->DockingRoot->ShowDockableWindow( DWT_GENERIC_BROWSER );

}

// Uses the currently selected resource.

void WxUnrealEdApp::CB_UseCurrent()
{
	FStringOutputDevice	GetPropResult;
	FString Cur;

	GUnrealEd->Get( TEXT("OBJ"), TEXT("BROWSECLASS"), GetPropResult );
	UClass* BrowseClass = FindObject<UClass>( ANY_PACKAGE, *GetPropResult );

	if( BrowseClass == UStaticMesh::StaticClass() && GSelectionTools.GetTop<UStaticMesh>() )
	{
		Cur = GSelectionTools.GetTop<UStaticMesh>()->GetPathName();
	}
	else if( BrowseClass == UMaterialInstance::StaticClass() && GSelectionTools.GetTop<UMaterialInstance>() )
	{
		Cur = GSelectionTools.GetTop<UMaterialInstance>()->GetPathName();
	}
	else if( BrowseClass == UTexture::StaticClass() && GSelectionTools.GetTop<UTexture>() )
	{
		Cur = GSelectionTools.GetTop<UTexture>()->GetPathName();
	}
	else if( BrowseClass == USoundNodeWave::StaticClass() && GSelectionTools.GetTop<USoundNodeWave>() )
	{
		Cur = GSelectionTools.GetTop<USoundNodeWave>()->GetPathName();
	}
	//else if( BrowseClass == UMusic::StaticClass() && GSelectionTools.GetTop<UMusic>() )
	//{
	//	Cur = GSelectionTools.GetTop<UMusic>()->GetPathName();
	//}
	else if( BrowseClass == USkeletalMesh::StaticClass() && GSelectionTools.GetTop<USkeletalMesh>() )
	{
		Cur = GSelectionTools.GetTop<USkeletalMesh>()->GetPathName();
	}
	else
	{
		// Use the first selected actor we find

		for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
		{
			AActor* Actor = GUnrealEd->Level->Actors(i);
			if( Actor && GSelectionTools.IsSelected( Actor ) )
			{
				Cur = Actor->GetPathName();
				break;
			}
		}
	}

	if( Cur.Len() )
		GUnrealEd->Set( TEXT("OBJ"), TEXT("NOTECURRENT"), *FString::Printf(TEXT("CLASS=%s OBJECT=%s"), *GetPropResult, *Cur));

}

// Current selection changes (either actors or BSP surfaces).

void WxUnrealEdApp::CB_SelectionChanged()
{
	DlgSurfProp->RefreshPages();
	//SurfPropSheet->PropSheet->RefreshPages();
}


// Tells all viewport frames to paint themselves

void WxUnrealEdApp::CB_ViewportUpdateWindowFrame()
{
}

// Shows the surface properties dialog

void WxUnrealEdApp::CB_SurfProps()
{
	DlgSurfProp->Show( 1 );
	//SurfPropSheet->Show( TRUE );

}

// Shows the actor properties dialog

void WxUnrealEdApp::CB_ActorProps()
{
	GUnrealEd->ShowActorProperties();

}

// Saves the current map

void WxUnrealEdApp::CB_SaveMap()
{
	EditorFrame->FileHelper.Save();
}

// Asks the user to save changes to the current map and presents them with a "file open" dialog

void WxUnrealEdApp::CB_LoadMap()
{
	EditorFrame->FileHelper.Load();

}

// Plays the current map

void WxUnrealEdApp::CB_PlayMap()
{
	GUnrealEd->PlayMap();

}

// Called whenever the user changes the camera mode

void WxUnrealEdApp::CB_CameraModeChanged()
{
	EditorFrame->ModeBar = GEditorModeTools.GetCurrentMode()->ModeBar;
    wxSizeEvent  eventSize;
	EditorFrame->OnSize( eventSize );
}

// Called whenever an actor has one of it's properties changed

void WxUnrealEdApp::CB_ActorPropertiesChanged()
{
}

void WxUnrealEdApp::CB_SaveMapAs()
{
	EditorFrame->FileHelper.SaveAs();
}

void WxUnrealEdApp::CB_DisplayLoadErrors()
{
	DlgLoadErrors->Refresh();
	DlgLoadErrors->Show(1);

}

void WxUnrealEdApp::CB_RefreshEditor( DWORD InFlags )
{
	EditorFrame->DockingRoot->RefreshEditor( InFlags );

	if( InFlags&ERefreshEditor_LayerBrowser )
		EditorFrame->DockingRoot->FindDockableWindow( DWT_LAYER_BROWSER )->Update();
	if( InFlags&ERefreshEditor_GroupBrowser )
		BrowserGroup->UpdateAll();
	if( InFlags&ERefreshEditor_GenericBrowser )
		EditorFrame->DockingRoot->FindDockableWindow( DWT_GENERIC_BROWSER )->Update();

	if( GEditorModeTools.GetCurrentMode()->ModeBar )
		GEditorModeTools.GetCurrentMode()->ModeBar->Refresh();

}

// Tells the editor that something has been done to change the map.  Can be
// anything from loading a whole new map to changing the BSP.

void WxUnrealEdApp::CB_MapChange( UBOOL InRebuildCollisionHash )
{
	// Rebuild the collision hash if this map change is something major ("new", "open", etc).
	// Minor things like brush subtraction will set it to "0".

	if( InRebuildCollisionHash )
	{	
		GEditor->ClearComponents();
		if( GEditor->Level )
			GEditor->Level->CleanupLevel();
	}

	GEditor->UpdateComponents();

	GEditorModeTools.GetCurrentMode()->MapChangeNotify();

	GApp->SetCurrentMode( GEditorModeTools.GetCurrentModeID() );

	CB_RefreshEditor( ERefreshEditor_AllBrowsers );

}

void WxUnrealEdApp::CB_AnimationSeqRightClicked()
{
	POINT pt;
	HMENU menu = LoadMenuIdX(hInstance, IDMENU_AnimationViewer_Context),
		submenu = GetSubMenu( menu, 0 );
	::GetCursorPos( &pt );

	TrackPopupMenu( submenu,
		TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
		pt.x, pt.y, 0,
		GUnrealEd->hWndMain, NULL); //#EDN! - which parent ?
	
	DestroyMenu( menu );

}

void WxUnrealEdApp::CB_RedrawAllViewports()
{
	GUnrealEd->RedrawLevel( GUnrealEd->Level );
	GUnrealEd->RedrawAllViewports( 1 );

}

void WxUnrealEdApp::CB_Undo()
{
	if( GEditorModeTools.GetCurrentModeID() == EM_Geometry )
	{
		FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools.GetCurrentMode();
		mode->PostUndo();
	}
}

void WxUnrealEdApp::SetCurrentMode( EEditorMode InMode )
{
	GEditorModeTools.SetCurrentMode( InMode );

	// Show/Hide mode bars as appropriate

	for( INT x = 0 ; x < GEditorModeTools.Modes.Num() ; ++x )
		GEditorModeTools.Modes(x)->ModeBar->Show(0);

	// Set the current modebar to show up at the top of the editor frame

	EditorFrame->ModeBar = GEditorModeTools.GetCurrentMode()->ModeBar;
	GEditorModeTools.GetCurrentMode()->ModeBar->Show(1);
	EditorFrame->GetStatusBar()->Refresh();

	// Force the frame to resize itself to accomodate the new mode bar

    wxSizeEvent eventSize;
	EditorFrame->OnSize( eventSize );

}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
