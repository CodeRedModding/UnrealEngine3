/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEd.h"
#include "LevelViewportToolBar.h"
#include "ViewportsContainer.h"
#include "DropTarget.h"


/*-----------------------------------------------------------------------------
	FViewportConfig_Viewport.
-----------------------------------------------------------------------------*/

FViewportConfig_Viewport::FViewportConfig_Viewport()
{
	ViewportType = LVT_Perspective;
	ShowFlags = (SHOW_DefaultEditor&~SHOW_ViewMode_Mask) | SHOW_ViewMode_BrushWireframe | SHOW_ModeWidgets;
	bEnabled = 0;
	bSetListenerPosition = 0;
}

FViewportConfig_Viewport::~FViewportConfig_Viewport()
{
}

/*-----------------------------------------------------------------------------
	FViewportConfig_Template.
-----------------------------------------------------------------------------*/

FViewportConfig_Template::FViewportConfig_Template()
{
}

FViewportConfig_Template::~FViewportConfig_Template()
{
}

void FViewportConfig_Template::Set( EViewportConfig InViewportConfig )
{
	ViewportConfig = InViewportConfig;

	switch( ViewportConfig )
	{
		case VC_2_2_Split:

			Desc = *LocalizeUnrealEd("2x2Split");

			ViewportTemplates[0].bEnabled = 1;
			ViewportTemplates[0].ViewportType = LVT_OrthoXZ;
			ViewportTemplates[0].ShowFlags = (SHOW_DefaultEditor&~SHOW_ViewMode_Mask) | SHOW_ViewMode_BrushWireframe;

			ViewportTemplates[1].bEnabled = 1;
			ViewportTemplates[1].ViewportType = LVT_OrthoYZ;
			ViewportTemplates[1].ShowFlags = (SHOW_DefaultEditor&~SHOW_ViewMode_Mask) | SHOW_ViewMode_BrushWireframe;

			ViewportTemplates[2].bEnabled = 1;
			ViewportTemplates[2].ViewportType = LVT_OrthoXY;
			ViewportTemplates[2].ShowFlags = (SHOW_DefaultEditor&~SHOW_ViewMode_Mask) | SHOW_ViewMode_BrushWireframe;

			ViewportTemplates[3].bEnabled = 1;
			ViewportTemplates[3].bSetListenerPosition = 1;
			ViewportTemplates[3].ViewportType = LVT_Perspective;
			ViewportTemplates[3].ShowFlags = (SHOW_DefaultEditor&~SHOW_ViewMode_Mask) | SHOW_ViewMode_Lit;

			break;

		case VC_1_2_Split:

			Desc = *LocalizeUnrealEd("1x2Split");

			ViewportTemplates[0].bEnabled = 1;
			ViewportTemplates[0].bSetListenerPosition = 1;
			ViewportTemplates[0].ViewportType = LVT_Perspective;
			ViewportTemplates[0].ShowFlags = (SHOW_DefaultEditor&~SHOW_ViewMode_Mask) | SHOW_ViewMode_Lit;

			ViewportTemplates[1].bEnabled = 1;
			ViewportTemplates[1].ViewportType = LVT_OrthoXY;
			ViewportTemplates[1].ShowFlags = (SHOW_DefaultEditor&~SHOW_ViewMode_Mask) | SHOW_ViewMode_BrushWireframe;

			ViewportTemplates[2].bEnabled = 1;
			ViewportTemplates[2].ViewportType = LVT_OrthoXZ;
			ViewportTemplates[2].ShowFlags = (SHOW_DefaultEditor&~SHOW_ViewMode_Mask) | SHOW_ViewMode_BrushWireframe;

			break;

		case VC_1_1_SplitH:

			Desc = *LocalizeUnrealEd("1x1SplitH");

			ViewportTemplates[0].bEnabled = 1;
			ViewportTemplates[0].bSetListenerPosition = 1;
			ViewportTemplates[0].ViewportType = LVT_Perspective;
			ViewportTemplates[0].ShowFlags = (SHOW_DefaultEditor&~SHOW_ViewMode_Mask) | SHOW_ViewMode_Lit;

			ViewportTemplates[1].bEnabled = 1;
			ViewportTemplates[1].ViewportType = LVT_OrthoXY;
			ViewportTemplates[1].ShowFlags = (SHOW_DefaultEditor&~SHOW_ViewMode_Mask) | SHOW_ViewMode_BrushWireframe;

			break;

		case VC_1_1_SplitV:

			Desc = *LocalizeUnrealEd("1x1SplitV");

			ViewportTemplates[0].bEnabled = 1;
			ViewportTemplates[0].bSetListenerPosition = 1;
			ViewportTemplates[0].ViewportType = LVT_Perspective;
			ViewportTemplates[0].ShowFlags = (SHOW_DefaultEditor&~SHOW_ViewMode_Mask) | SHOW_ViewMode_Lit;

			ViewportTemplates[1].bEnabled = 1;
			ViewportTemplates[1].ViewportType = LVT_OrthoXY;
			ViewportTemplates[1].ShowFlags = (SHOW_DefaultEditor&~SHOW_ViewMode_Mask) | SHOW_ViewMode_BrushWireframe;

			break;

		default:
			check(0);	// Unknown viewport config
			break;
	}
}

/*-----------------------------------------------------------------------------
	FVCD_Viewport.
-----------------------------------------------------------------------------*/

FVCD_Viewport::FVCD_Viewport()
{
	ViewportWindow = NULL;
	FloatingViewportFrame = NULL;
	PIEContainerWindow = NULL;
	ShowFlags = (SHOW_DefaultEditor&~SHOW_ViewMode_Mask) | SHOW_ViewMode_BrushWireframe | SHOW_ModeWidgets;

	SashPos = 0;
}

FVCD_Viewport::~FVCD_Viewport()
{
}


// This will save and unsplit the windows to allow for
// re-splitting later.
void WxSplitterWindow::SaveAndUnsplit(wxWindow *toRemove)
{
	OldWindows[0] = GetWindow1();
	OldWindows[1] = GetWindow2();

	Unsplit(toRemove);
}

// Can be called after an Unsplit to re-split the window as
// it was before the Unsplit.
void WxSplitterWindow::ReSplit()
{
	if(!IsSplit() && OldWindows[0] != NULL && OldWindows[1] != NULL)
	{
		if(GetSplitMode() == wxSPLIT_HORIZONTAL)
		{
			SplitHorizontally(OldWindows[0], OldWindows[1]);
		}
		else
		{
			SplitVertically(OldWindows[0], OldWindows[1]);
		}
	}
}

/*-----------------------------------------------------------------------------
	FViewportConfig_Data.
-----------------------------------------------------------------------------*/

FViewportConfig_Data::FViewportConfig_Data()
{
	MaximizedViewport = -1;
	CustomFOV = 90;
}

FViewportConfig_Data::~FViewportConfig_Data()
{
	// Clean up floating viewports
	for( INT FloatingViewportIndex = 0; FloatingViewportIndex < FloatingViewports.Num(); ++FloatingViewportIndex )
	{
		FVCD_Viewport* FloatingViewport = FloatingViewports( FloatingViewportIndex );
		if( FloatingViewport != NULL )
		{
			// Unbind callback interface
			if( FloatingViewport->FloatingViewportFrame != NULL )
			{
				FloatingViewport->FloatingViewportFrame->SetCallbackInterface( NULL );
			}

			// NOTE: No other real cleanup work to do here; the floating windows will be destroyed by their parent
			//   window naturally
		}
	}
}

/**
 * Tells this viewport configuration to create its windows and apply its settings.
 *
 * @param	InParent	The parent viewport containing holding the viewports and splitters. 
 * @param	bFoundMaximizedViewport	TRUE if one of the viewports will be maximized; FALSE, otherwise. 
 */
void FViewportConfig_Data::Apply( WxViewportsContainer* InParent, UBOOL bFoundMaximizedViewport/*=FALSE*/ )
{
	// NOTE: Floating viewports currently won't be affected by this function.  Any floating windows will just
	// be left intact.

	for( INT CurViewportIndex = 0; CurViewportIndex < 4; ++CurViewportIndex )
	{
		FVCD_Viewport& CurViewport = Viewports[ CurViewportIndex ];
		if( CurViewport.bEnabled && CurViewport.ViewportWindow != NULL )
		{
			CurViewport.ViewportWindow->DestroyChildren();
			CurViewport.ViewportWindow->Destroy();
			CurViewport.ViewportWindow = NULL;
		}

		if( CurViewport.PIEContainerWindow != NULL )
		{
			CurViewport.PIEContainerWindow->DestroyChildren();
			CurViewport.PIEContainerWindow->Destroy();
		}
	}

	MaximizedViewport = -1;
	for( int CurSplitterIndex = 0; CurSplitterIndex < SplitterWindows.Num(); ++CurSplitterIndex )
	{
		SplitterWindows( CurSplitterIndex )->DestroyChildren();
		SplitterWindows( CurSplitterIndex )->Destroy();
	}
	SplitterWindows.Empty();


	wxRect rc = InParent->GetClientRect();

	// Set up the splitters and viewports as per the template defaults.

	WxSplitterWindow *MainSplitter = NULL;
	INT SashPos = 0;
	FString Key;

	switch( Template )
	{
		case VC_2_2_Split:
		{
			WxSplitterWindow *TopSplitter, *BottomSplitter;

			MainSplitter = new WxSplitterWindow( InParent, ID_SPLITTERWINDOW, wxPoint(0,0), wxSize(rc.GetWidth(), rc.GetHeight()), wxSP_3D );
			TopSplitter = new WxSplitterWindow( MainSplitter, ID_SPLITTERWINDOW+1, wxPoint(0,0), wxSize(rc.GetWidth(), rc.GetHeight()/2), wxSP_3D );
			BottomSplitter = new WxSplitterWindow( MainSplitter, ID_SPLITTERWINDOW+2, wxPoint(0,0), wxSize(rc.GetWidth(), rc.GetHeight()/2), wxSP_3D );

			// Hide the splitters until the viewport configuration has been finalized
			// This prevents splitters from showing up in the wrong position for a few frames.
			MainSplitter->Show(FALSE);
			TopSplitter->Show(FALSE);
			BottomSplitter->Show(FALSE);

			SplitterWindows.AddItem( MainSplitter );
			SplitterWindows.AddItem( TopSplitter );
			SplitterWindows.AddItem( BottomSplitter );

			// Connect splitter events so we can link the top and bottom splitters if the ViewportResizeTogether option is set.
			InParent->ConnectSplitterEvents( TopSplitter, BottomSplitter );

			for( INT x = 0 ; x < 4 ; ++x )
			{
				if( Viewports[x].bEnabled )
				{
					Viewports[x].ViewportWindow = new WxLevelViewportWindow;
					Viewports[x].ViewportWindow->Create( MainSplitter, -1 );
					Viewports[x].ViewportWindow->SetUp( Viewports[x].ViewportType, Viewports[x].bSetListenerPosition, Viewports[x].ShowFlags );
					Viewports[x].ViewportWindow->SetLabel( wxT("LevelViewport") );
				}
			}

			Viewports[0].ViewportWindow->Reparent( TopSplitter );
			Viewports[1].ViewportWindow->Reparent( TopSplitter );
			Viewports[2].ViewportWindow->Reparent( BottomSplitter );
			Viewports[3].ViewportWindow->Reparent( BottomSplitter );

			MainSplitter->SetLabel( wxT("MainSplitter") );
			TopSplitter->SetLabel( wxT("TopSplitter") );
			BottomSplitter->SetLabel( wxT("BottomSplitter") );

			// To give the default perspective viewport more screen space, make the sash 
			// position split at 30% of the total screen width instead of 50%.
			INT SashPos_Horizontal = rc.GetHeight() * 0.3f;
			// To give the default perspective viewport more screen space, make the sash 
			// position split where the right viewports are squished, such that all the 
			// toolbar icons are visible, but no extra space is added. To do this, you 
			// have specify a negative number to wxWidgets as the sash position. This will 
			// subtract the given width starting at the right-side of the screen. Some extra
			// padding was added to see the end of the last viewport toolbar icon.
			INT ToolBarWidth = Viewports[1].ViewportWindow->ToolBar->GetBestSize().GetWidth() + Viewports[1].ViewportWindow->MaximizeToolBar->GetBestSize().GetWidth() + 5;
			const INT MaxWidthForRightViewports = Min( rc.GetWidth() / 2, ToolBarWidth );
			INT SashPos_Vertical = -MaxWidthForRightViewports;

			if( bFoundMaximizedViewport)
			{
				MainSplitter->SplitHorizontally( TopSplitter, BottomSplitter, 0 );
				TopSplitter->SplitVertically( Viewports[0].ViewportWindow, Viewports[1].ViewportWindow, 0 );
				BottomSplitter->SplitVertically( Viewports[3].ViewportWindow, Viewports[2].ViewportWindow, 0 );
			}
			else
			{
				GConfig->GetInt( TEXT("ViewportConfig"), TEXT("Splitter0"), SashPos_Horizontal, GEditorUserSettingsIni );
				MainSplitter->SplitHorizontally( TopSplitter, BottomSplitter, SashPos_Horizontal );
				GConfig->GetInt( TEXT("ViewportConfig"), TEXT("Splitter1"), SashPos_Vertical, GEditorUserSettingsIni );
				TopSplitter->SplitVertically( Viewports[0].ViewportWindow, Viewports[1].ViewportWindow, SashPos_Vertical );
				GConfig->GetInt( TEXT("ViewportConfig"), TEXT("Splitter2"), SashPos_Vertical, GEditorUserSettingsIni );
				BottomSplitter->SplitVertically( Viewports[3].ViewportWindow, Viewports[2].ViewportWindow, SashPos_Vertical );
			}
		}
		break;

		case VC_1_2_Split:
		{
			WxSplitterWindow *RightSplitter;

			MainSplitter = new WxSplitterWindow( InParent, ID_SPLITTERWINDOW, wxPoint(0,0), wxSize(rc.GetWidth(), rc.GetHeight()), wxSP_3D );
			RightSplitter = new WxSplitterWindow( MainSplitter, ID_SPLITTERWINDOW+1, wxPoint(0,0), wxSize(rc.GetWidth(), rc.GetHeight()/2), wxSP_3D );

			// Hide the splitters until the viewport configuration has been finalized
			// This prevents splitters from showing up in the wrong position for a few frames.
			MainSplitter->Show(FALSE);
			RightSplitter->Show(FALSE);

			SplitterWindows.AddItem( MainSplitter );
			SplitterWindows.AddItem( RightSplitter );

			// Disconnect Splitter Events
			InParent->DisconnectSplitterEvents();

			for( INT x = 0 ; x < 4 ; ++x )
			{
				if( Viewports[x].bEnabled )
				{
					Viewports[x].ViewportWindow = new WxLevelViewportWindow;
					Viewports[x].ViewportWindow->Create( MainSplitter, -1 );
					Viewports[x].ViewportWindow->SetUp( Viewports[x].ViewportType, Viewports[x].bSetListenerPosition, Viewports[x].ShowFlags );
					Viewports[x].ViewportWindow->SetLabel( wxT("LevelViewport") );
				}
			}

			Viewports[0].ViewportWindow->Reparent( MainSplitter );
			Viewports[1].ViewportWindow->Reparent( RightSplitter );
			Viewports[2].ViewportWindow->Reparent( RightSplitter );

			MainSplitter->SetLabel( wxT("MainSplitter") );
			RightSplitter->SetLabel( wxT("RightSplitter") );

			if( bFoundMaximizedViewport)
			{
				MainSplitter->SplitVertically( Viewports[0].ViewportWindow, RightSplitter, 1 );
				RightSplitter->SplitHorizontally( Viewports[1].ViewportWindow, Viewports[2].ViewportWindow, 1 );
			}
			else
			{
				GConfig->GetInt( TEXT("ViewportConfig"), TEXT("Splitter0"), SashPos, GEditorUserSettingsIni );
				MainSplitter->SplitVertically( Viewports[0].ViewportWindow, RightSplitter, SashPos );
				GConfig->GetInt( TEXT("ViewportConfig"), TEXT("Splitter1"), SashPos, GEditorUserSettingsIni );
				RightSplitter->SplitHorizontally( Viewports[1].ViewportWindow, Viewports[2].ViewportWindow, SashPos );
			}
		}
		break;

		case VC_1_1_SplitH:
		case VC_1_1_SplitV:
		{
			MainSplitter = new WxSplitterWindow( InParent, ID_SPLITTERWINDOW, wxPoint(0,0), wxSize(rc.GetWidth(), rc.GetHeight()), wxSP_3D );

			// Hide the splitters until the viewport configuration has been finalized
			// This prevents splitters from showing up in the wrong position for a few frames.
			MainSplitter->Show(FALSE);

			SplitterWindows.AddItem( MainSplitter );

			// Disconnect Splitter Events
			InParent->DisconnectSplitterEvents();

			for( INT x = 0 ; x < 4 ; ++x )
			{
				if( Viewports[x].bEnabled )
				{
					Viewports[x].ViewportWindow = new WxLevelViewportWindow;
					Viewports[x].ViewportWindow->Create( MainSplitter, -1 );
					Viewports[x].ViewportWindow->SetUp( Viewports[x].ViewportType, Viewports[x].bSetListenerPosition, Viewports[x].ShowFlags );
					Viewports[x].ViewportWindow->SetLabel( wxT("LevelViewport") );
				}
			}

			Viewports[0].ViewportWindow->Reparent( MainSplitter );
			Viewports[1].ViewportWindow->Reparent( MainSplitter );

			MainSplitter->SetLabel( wxT("MainSplitter") );

			if( bFoundMaximizedViewport)
			{
				SashPos = 1;
			}
			else
			{
				GConfig->GetInt( TEXT("ViewportConfig"), TEXT("Splitter0"), SashPos, GEditorUserSettingsIni );
			}

			if(Template == VC_1_1_SplitH)
			{
				MainSplitter->SplitHorizontally( Viewports[0].ViewportWindow, Viewports[1].ViewportWindow, SashPos );
			}
			else
			{
				MainSplitter->SplitVertically( Viewports[0].ViewportWindow, Viewports[1].ViewportWindow, SashPos );
			}
		}
		break;
	}

	// Make sure the splitters will resize with the editor
	wxBoxSizer* WkSizer = new wxBoxSizer( wxHORIZONTAL );
	WkSizer->Add( MainSplitter, 1, wxEXPAND | wxALL, 0 );

	Sizer = new wxBoxSizer( wxVERTICAL );
	Sizer->Add( WkSizer, 1, wxEXPAND | wxALL, 0 );
	InParent->SetSizer( Sizer );
	
	// Apply the custom settings contained in this instance.

	for( INT x = 0 ; x < 4 ; ++x )
	{
		if( Viewports[x].bEnabled )
		{
			check(Viewports[x].ViewportWindow);
			FString Key = FString::Printf( TEXT("Viewport%d"), x );
			GConfig->GetFloat( TEXT("ViewportConfig"), *(Key+TEXT("_CameraSpeed")), Viewports[x].ViewportWindow->CameraSpeed, GEditorUserSettingsIni );
			UBOOL bRealtimeMode(FALSE);
			UBOOL bRealtimeSettingFound = GConfig->GetBool( TEXT("ViewportConfig"), *(Key+TEXT("_Realtime")), bRealtimeMode, GEditorUserSettingsIni );

			// Default to realtime mode for perspective views if the preference is set
			Viewports[x].ViewportWindow->SetRealtime(bRealtimeMode || (!bRealtimeSettingFound && Viewports[x].ViewportType == LVT_Perspective
																		&& GEditor->AccessUserSettings().bStartInRealtimeMode));

			Viewports[x].ViewportWindow->ViewportType = Viewports[x].ViewportType;
			Viewports[x].ViewportWindow->ToolBar->UpdateUI();
			Viewports[x].ViewportWindow->SetEditorFrameClient(TRUE);
			Viewports[x].ViewportWindow->ResizeToolBar();
			//Viewports[x].ViewportWindow->Viewport->CaptureJoystickInput(TRUE);

			// Restore the camera position/rotation from the previous viewport of this type
			UBOOL MatchFound = FALSE;
			for( INT idx = 0; idx < OldViewportData.Num(); ++idx )
			{
				if ( Viewports[x].ViewportType == OldViewportData(idx).Type )
				{
					MatchFound = TRUE;

					Viewports[x].ViewportWindow->ViewLocation = OldViewportData(idx).Location;
					Viewports[x].ViewportWindow->ViewRotation = OldViewportData(idx).Orientation;

					break;
				}
			}

			if( Viewports[x].ViewportType == LVT_Perspective )
			{
				// Only perspective windows will have Matinee preview features turned on by default
				Viewports[x].ViewportWindow->SetAllowMatineePreview( TRUE );

				// Assign default camera location/rotation for perspective camera
				if (!MatchFound)
				{
					Viewports[x].ViewportWindow->ViewLocation = EditorViewportDefs::DefaultPerspectiveViewLocation;
					Viewports[x].ViewportWindow->ViewRotation = EditorViewportDefs::DefaultPerspectiveViewRotation;
				}
			}
		}
	}

	// Show all splitter windows now that the viewports have been configured
	for( INT SplitterIndex = 0; SplitterIndex < SplitterWindows.Num(); ++SplitterIndex )
	{
		SplitterWindows(SplitterIndex)->Show(TRUE);
	}
}

/*
 * Scales the splitter windows proportionally.
 * Used when switching between windowed and maximized mode.
 */
void FViewportConfig_Data::ResizeProportionally( FLOAT ScaleX, FLOAT ScaleY, UBOOL bRedraw/*=TRUE*/ )
{
	if ( MaximizedViewport >= 0 )
	{
		Layout();
	}
	else
	{
		for( INT x = 0 ; x < SplitterWindows.Num() ; ++x )
		{
			wxSplitterWindow* Splitter = SplitterWindows(x);
			wxSize WindowSize = Splitter->GetSize();
			FLOAT Scale, OldSize;
			if (Splitter->GetSplitMode() == wxSPLIT_HORIZONTAL)
			{
				Scale = ScaleY;
				OldSize = WindowSize.y;
			}
			else
			{
				Scale = ScaleX;
				OldSize = WindowSize.x;
			}
			FLOAT NewSize = FLOAT(OldSize) * Scale;
			FLOAT Proportion = FLOAT(Splitter->GetSashPosition()) / OldSize;
			INT Sash = appTrunc( Proportion * NewSize );
			Splitter->SetSashPosition( Sash, bRedraw == TRUE );
		}
	}
}

/**
 * Sets up this instance with the data from a template.
 */
void FViewportConfig_Data::SetTemplate( EViewportConfig InTemplate )
{
	Template = InTemplate;

	// Find the template

	FViewportConfig_Template* vct = NULL;
	for( INT x = 0 ; x < GApp->EditorFrame->ViewportConfigTemplates.Num() ; ++x )
	{
		FViewportConfig_Template* vctWk = GApp->EditorFrame->ViewportConfigTemplates(x);
		if( vctWk->ViewportConfig == InTemplate )
		{
			vct = vctWk;
			break;
		}
	}

	check( vct );	// If NULL, the template config type is unknown

	// Copy the templated data into our local vars

	*this = *vct;
}

/**
 * Updates custom data elements.
 */

void FViewportConfig_Data::Save()
{
	for( INT x = 0 ; x < GetViewportCount(); ++x )
	{
		FVCD_Viewport* viewport = &AccessViewport( x );

		if( viewport->bEnabled )
		{
			viewport->ViewportType = (ELevelViewportType)viewport->ViewportWindow->ViewportType;

			if(viewport->ViewportWindow->bInGameViewMode)
			{
				viewport->ShowFlags = viewport->ViewportWindow->LastShowFlags;
			}
			else
			{
				viewport->ShowFlags = viewport->ViewportWindow->ShowFlags;
			}

			// Make sure we don't save temporary lit mode settings
			if( viewport->ViewportWindow->bForcedUnlitShowFlags )
			{
				if( ( viewport->ShowFlags & SHOW_ViewMode_Unlit ) == SHOW_ViewMode_Unlit )
				{
					viewport->ShowFlags |= SHOW_Lighting;
				}
			}

		}
	}
}

/**
 * Loads the custom data elements from InData to this instance.
 *
 * @param	InData	The instance to load the data from.
 */

void FViewportConfig_Data::Load( FViewportConfig_Data* InData, UBOOL bTransferFloatingViewports )
{
	if( bTransferFloatingViewports )
	{
		// We expect the destination floating viewport array to be empty at this point
		if( FloatingViewports.Num() == 0 )
		{
			// Transfer ownership of floating viewports
			FloatingViewports.Add( InData->FloatingViewports.Num() );
			for( INT FloatingViewportIndex = 0; FloatingViewportIndex < InData->FloatingViewports.Num(); ++FloatingViewportIndex )
			{
				FloatingViewports( FloatingViewportIndex ) = InData->FloatingViewports( FloatingViewportIndex );
				//make sure the callback interface is updated
                WxFloatingViewportFrame* ViewportFrame = FloatingViewports( FloatingViewportIndex )->FloatingViewportFrame;
                check(ViewportFrame );
                ViewportFrame->SetCallbackInterface(this);
			}

			// Kill the source pointer array so that it doesn't get cleaned up
			InData->FloatingViewports.Reset();
		}
	}


	for( INT x = 0 ; x < InData->GetViewportCount(); ++x )
	{
		const FVCD_Viewport* Src = &InData->GetViewport( x );

		if( Src->bEnabled )
		{
			// Find a matching viewport to copy the data into

			for( INT y = 0 ; y < GetViewportCount(); ++y )
			{
				FVCD_Viewport* Dst = &AccessViewport( y );

				if( Dst->bEnabled && Dst->ViewportType == Src->ViewportType )
				{
					Dst->ViewportType = Src->ViewportType;
					Dst->ShowFlags = Src->ShowFlags;
				}
			}
		}
	}
}

/**
 * Saves out the current viewport configuration to the editors INI file.
 */

void FViewportConfig_Data::SaveToINI()
{
	const TCHAR *SectionName = TEXT("ViewportConfig");

	Save();

	GConfig->EmptySection( SectionName, GEditorUserSettingsIni );
	
	// used to identify new show flags, so we can default those to have the new SHOW_DefaultEditor state
	GConfig->SetString( SectionName, TEXT("DefaultEditor_ShowFlagsBinaryString2"), *SHOW_DefaultEditor.ToString(), GEditorUserSettingsIni );

	GConfig->SetInt( SectionName, TEXT("Template"), Template, GEditorUserSettingsIni );

	// Save MaximizedViewport setting
	GConfig->SetInt( SectionName, TEXT( "MaximizedViewport" ), MaximizedViewport, GEditorUserSettingsIni );

	// Save CustomFOV setting
	GConfig->SetInt( SectionName, TEXT( "CustomFOV" ), CustomFOV, GEditorUserSettingsIni );

	if ( !IsViewportMaximized() )
	{
		for( INT x = 0 ; x < SplitterWindows.Num() ; ++x )
		{
			FString Key = FString::Printf( TEXT("Splitter%d"), x );
			GConfig->SetInt( SectionName, *Key, SplitterWindows(x)->GetSashPosition(), GEditorUserSettingsIni );
		}
	}

	for( INT x = 0 ; x < 4 ; ++x )
	{
		FVCD_Viewport* viewport = &Viewports[x];

		FString Key = FString::Printf( TEXT("Viewport%d"), x );

		GConfig->SetBool( SectionName, *(Key+TEXT("_Enabled")), viewport->bEnabled, GEditorUserSettingsIni );

		if( viewport->bEnabled )
		{
			GConfig->SetInt( SectionName, *(Key+TEXT("_ViewportType")), viewport->ViewportType, GEditorUserSettingsIni );

			GConfig->SetString( SectionName, *(Key+TEXT("_ShowFlagsBinaryString2")), *viewport->ShowFlags.ToString(), GEditorUserSettingsIni );
			GConfig->SetFloat(SectionName, *(Key+TEXT("_CameraSpeed")), viewport->ViewportWindow->CameraSpeed, GEditorUserSettingsIni);
			GConfig->SetBool(SectionName, *(Key+TEXT("_Realtime")), viewport->ViewportWindow->GetIsRealtime(), GEditorUserSettingsIni);
		}
	}

	// Save floating viewport info
	TArray< FString > MyKeys;
	for( INT Index = 0; Index < FloatingViewports.Num(); Index++ )
	{
		FVCD_Viewport* Viewport = FloatingViewports( Index );
		if( !Viewport || !Viewport->ViewportWindow )
		{
			continue;
		}

		wxRect ScreenRect = Viewport->FloatingViewportFrame->GetScreenRect();

		FString ShowFlagsBinaryString = Viewport->ShowFlags.ToString();

		FString MyOutput = *FString::Printf(
			TEXT( "%d,%d,%d,%s,%d,%d,%d,%d" ),
			( INT )Viewport->bEnabled, ( INT )Viewport->ViewportType, ( INT )Viewport->bSetListenerPosition, *ShowFlagsBinaryString,
			ScreenRect.GetX(), ScreenRect.GetY(), ScreenRect.GetWidth(), ScreenRect.GetHeight()
		);

		MyKeys.AddItem( MyOutput );
	}
	GConfig->SetArray( TEXT( "FloatingViewports" ), TEXT( "Viewport" ), MyKeys, GEditorUserSettingsIni );
}

/**
 * Attempts to load the viewport configuration from the editors INI file.  If unsuccessful,
 * it returns 0 to the caller.
 */
UBOOL FViewportConfig_Data::LoadFromINI()
{
	const TCHAR *SectionName = TEXT("ViewportConfig");

	INT Wk = VC_None;
	GConfig->GetInt( SectionName, TEXT("Template"), Wk, GEditorUserSettingsIni );

	if( Wk == VC_None )
	{
		return 0;
	}

	// used to identify new show flags, so we can default those to have the new SHOW_DefaultEditor state
	EShowFlags SavedDefaultEditor = SHOW_DefaultEditor & ~(SHOW_DepthOfField | SHOW_ImageReflections | SHOW_SubsurfaceScattering | SHOW_LightFunctions | SHOW_Tessellation);
	{
		FString ShowFlagsStr;
		if( GConfig->GetString( SectionName, TEXT("DefaultEditor_ShowFlagsBinaryString2"), ShowFlagsStr, GEditorUserSettingsIni ) )
		{
			SavedDefaultEditor = EShowFlags(ShowFlagsStr);
		}
	}
	// to fixup when the SHOW_DefaultEditor changes (e.g. new show flags)
	EShowFlags ChangedMask = SavedDefaultEditor ^ SHOW_DefaultEditor;

	Template = (EViewportConfig)Wk;
	GApp->EditorFrame->ViewportConfigData->SetTemplate( Template );

	// NOTE: Configuration of floating viewport windows is currently not loaded or saved to .ini files

	for( INT x = 0 ; x < 4 ; ++x )
	{
		FString Key = FString::Printf( TEXT("Viewport%d"), x );

		UBOOL bIsEnabled = FALSE;
		UBOOL bFoundViewport = GConfig->GetBool( SectionName, *(Key+TEXT("_Enabled")), bIsEnabled, GEditorUserSettingsIni );

		if( bFoundViewport && bIsEnabled )
		{
			FVCD_Viewport* viewport = &Viewports[x];

			viewport->bEnabled = bIsEnabled;

			GConfig->GetInt( SectionName, *(Key+TEXT("_ViewportType")), Wk, GEditorUserSettingsIni );
			viewport->ViewportType = (ELevelViewportType)Wk;

			FString ShowFlagsStr;
			if( GConfig->GetString( SectionName, *(Key+TEXT("_ShowFlagsBinaryString2")), ShowFlagsStr, GEditorUserSettingsIni ) )
			{
				viewport->ShowFlags = EShowFlags( ShowFlagsStr );
			}

			// if the SHOW_DefaultEditor changes we don't use the serialized data, we use the new default state instead
			viewport->ShowFlags = (viewport->ShowFlags & ~ChangedMask) | (SHOW_DefaultEditor & ChangedMask);

			viewport->ShowFlags |= SHOW_ModeWidgets;

			// always enable LOD on startup, since it disables MinDrawDistance
			viewport->ShowFlags |= SHOW_LOD;

			// Remove postprocess on non-perspective viewports
			if( viewport->ViewportType != LVT_Perspective )
			{
				viewport->ShowFlags &= ~SHOW_PostProcess;
			}

			// Always clear the StreamingBounds flag at start-up
			viewport->ShowFlags &= ~SHOW_StreamingBounds;

			//ensure that we are in a supported view mode
			EShowFlags ToolbarSupportedViewModes = (SHOW_ViewMode_Mask & (~SHOW_VertexColors));
			if ((viewport->ShowFlags & ToolbarSupportedViewModes) == 0)
			{
				viewport->ShowFlags |= SHOW_ViewMode_Lit;
			}
		}
	}

	// Load and apply MaximizedViewport setting
	INT ViewportToMaximize = -1;
	GConfig->GetInt( SectionName, TEXT( "MaximizedViewport" ), ViewportToMaximize, GEditorUserSettingsIni );

	// Load CustomFOV setting
	GConfig->GetInt( SectionName, TEXT( "CustomFOV" ), CustomFOV, GEditorUserSettingsIni );

	// No need to transfer floating viewports since we're not reinstantiating the object
	const UBOOL bTransferFloatingViewports = FALSE;
	const UBOOL bHaveMaximizedViewport = ( ViewportToMaximize >= 0 && ViewportToMaximize < 4 );
	GApp->EditorFrame->ViewportConfigData->Load( this, bTransferFloatingViewports );
	GApp->EditorFrame->ViewportConfigData->Apply( GApp->EditorFrame->ViewportContainer, bHaveMaximizedViewport );

	if( bHaveMaximizedViewport )
	{
		if( Viewports[ ViewportToMaximize ].bEnabled && MaximizedViewport != ViewportToMaximize )
		{
			ToggleMaximize( Viewports[ ViewportToMaximize ].ViewportWindow->Viewport );
			if( MaximizedViewport != -1 )
			{
				Viewports[ MaximizedViewport ].ViewportWindow->Invalidate();
			}
		}
	}

	// Load floating viewport info, parse it, initialize and create them
	TArray< FString > MyKeys;
	GConfig->GetArray( TEXT( "FloatingViewports" ), TEXT( "Viewport" ), MyKeys, GEditorUserSettingsIni );
	for( INT Index = 0; Index < MyKeys.Num(); Index++ )
	{
		FString Key = MyKeys( Index );

		TArray< FString > KeyArray;
		Key.ParseIntoArray( &KeyArray, TEXT( "," ), FALSE );

		if( KeyArray.Num() != 8 )
		{
			continue;
		}
		UBOOL bEnabled = ( appStrtoi( *KeyArray( 0 ), NULL, 0 ) != 0 );
		ELevelViewportType ViewportType = ( ELevelViewportType )appStrtoi( *KeyArray( 1 ), NULL, 0 );
		UBOOL bSetListenerPosition = ( appStrtoi( *KeyArray( 2 ), NULL, 0 ) != 0 );
		EShowFlags ShowFlags = EShowFlags( KeyArray( 3 ) );
		INT XPos = appStrtoi( *KeyArray( 4 ), NULL, 0 );
		INT YPos = appStrtoi( *KeyArray( 5 ), NULL, 0 );
		INT Width = appStrtoi( *KeyArray( 6 ), NULL, 0 );
		INT Height = appStrtoi( *KeyArray( 7 ), NULL, 0 );

		FFloatingViewportParams ViewportParams;
		ViewportParams.ParentWxWindow = GApp->EditorFrame;
		ViewportParams.ViewportType = ViewportType;
		ViewportParams.ShowFlags = ShowFlags;
		ViewportParams.Width = Width;
		ViewportParams.Height = Height;

		INT NewViewportIndex = INDEX_NONE;
		UBOOL bResultValue = OpenNewFloatingViewport( ViewportParams, NewViewportIndex );

		if( bResultValue )
		{
			FVCD_Viewport& NewViewport = AccessViewport( NewViewportIndex );
			NewViewport.bEnabled = bEnabled;
			NewViewport.bSetListenerPosition = bSetListenerPosition;
			NewViewport.FloatingViewportFrame->SetPosition( wxPoint( XPos, YPos ) );
		}
	}

	return 1;
}

/**
 * Either resizes all viewports so that the specified Viewport is fills the entire editor window,
 * or restores all viewports to their previous sizes.
 * WxEditorFrame will lock all splitter drag bars when a viewport is maximized.
 * This function is called by clicking a button on the viewport toolbar.
 */
void FViewportConfig_Data::ToggleMaximize( FViewport* Viewport )
{
	for( INT x = 0; x < 4 && Viewports[x].ViewportWindow; ++x )
	{
		if ( Viewport == Viewports[x].ViewportWindow->Viewport )
		{
			// Already maximized?
			if ( MaximizedViewport == x )
			{
				// Restore all viewports:

				// Restore all sash positions:
				for( INT n = 0; n < SplitterWindows.Num(); ++n )
				{
					INT SashPos;
					FString Key = FString::Printf( TEXT("Splitter%d"), n );
					if ( GConfig->GetInt( TEXT("ViewportConfig"), *Key, SashPos, GEditorUserSettingsIni ) )
					{
						SplitterWindows(n)->ReSplit();
						SplitterWindows(n)->SetSashPosition( SashPos );
					}
				}
				MaximizedViewport = -1;
			}
			else
			{
				// Maximize this viewport:

				// Save sash positions if no other viewport was maximized (should never happen, but anyway)
				if ( MaximizedViewport < 0 )
				{
					for( INT n = 0; n < SplitterWindows.Num(); ++n )
					{
						FString Key = FString::Printf( TEXT("Splitter%d"), n);
						INT SashPos = SplitterWindows(n)->GetSashPosition();
						GConfig->SetInt( TEXT("ViewportConfig"), *Key, SashPos, GEditorUserSettingsIni );
					}
				}

				MaximizedViewport = x;
				Layout();
			}
			break;
		}
	}
}

/**
 * Lets the ViewportConfig layout the splitter windows.
 * If a viewport is maximized, set up all sash positions so that the maximized viewport covers the entire window.
 */
void FViewportConfig_Data::Layout()
{
	if ( IsViewportMaximized() )
	{
		INT TreePath[3] = {0,0,0};	// Maximum 3 splitters
		check( SplitterWindows.Num() <= 3 );
		
		wxWindow* ContainedWindow = Viewports[MaximizedViewport].ViewportWindow;

		// If we have a PIE container window, then we should always be looking for that window
		if( Viewports[MaximizedViewport].PIEContainerWindow != NULL )
		{
			ContainedWindow = Viewports[MaximizedViewport].PIEContainerWindow;
		}

		INT WhichWindow;

		INT SplitterIndex = FindSplitter( ContainedWindow, &WhichWindow );
		while( SplitterIndex >= 0 )
		{
			TreePath[SplitterIndex] = WhichWindow;
			ContainedWindow = SplitterWindows(SplitterIndex);
			SplitterIndex = FindSplitter( ContainedWindow, &WhichWindow );
		}
		for( INT n=0; n < SplitterWindows.Num(); ++n )
		{
			WxSplitterWindow* Splitter = SplitterWindows(n);
			wxSize Size = Splitter->GetClientSize();
			INT MaxPos = (Splitter->GetSplitMode() == wxSPLIT_HORIZONTAL) ? Size.y : Size.x;
			if ( TreePath[n] == 1 )
			{
				if(Splitter->GetWindow2())
				{
					Splitter->SaveAndUnsplit(Splitter->GetWindow2());
				}
			}
			else if ( TreePath[n] == 2 )
			{
				if(Splitter->GetWindow1())
				{
					Splitter->SaveAndUnsplit(Splitter->GetWindow1());
				}
			}
			else
			{
				Splitter->SetSashPosition(0, FALSE);
			}
			Splitter->UpdateSize();
		}
	}
}

/**
 * Returns TRUE if a viewport is maximized, otherwise FALSE.
 */
UBOOL FViewportConfig_Data::IsViewportMaximized()
{
	return (MaximizedViewport >= 0);
}

/**
 * Helper function to allow setting of a viewports size explicitely
 * @param Viewport - Viewport to resize
 * @param InWidth - New width requested for this viewport
 * @param InHeight - New height requested for this viewport
 */
void FViewportConfig_Data::ResizeViewportToSize(FViewport* Viewport, const INT InWidth, const INT InHeight)
{
	UBOOL bFoundViewport = FALSE;
	INT ViewportWidth = InWidth;
	INT ViewportHeight = InHeight;

	for( INT x = 0; x < 4 && Viewports[x].ViewportWindow; ++x )
	{
		if ( Viewport == Viewports[x].ViewportWindow->Viewport )
		{
			if (Template != VC_2_2_Split)
			{
				UBOOL bResetSplitters = FALSE;
				GApp->EditorFrame->SetViewportConfig(VC_2_2_Split);
			}

			//negate the width if 1,2 (right side)
			if ((x==1) || (x==2))
			{
				ViewportWidth = SplitterWindows(0)->GetSize().GetWidth() - ViewportWidth;
			}

			//negate the height if 2,3 (bottom)
			if ((x==2) || (x==3))
			{
				ViewportHeight = SplitterWindows(0)->GetClientSize().GetHeight() - ViewportHeight;
			}

			SplitterWindows(0)->SetSashPosition(ViewportHeight);
			SplitterWindows(1)->SetSashPosition(ViewportWidth);
			SplitterWindows(2)->SetSashPosition(ViewportWidth);

			bFoundViewport = TRUE;
			break;
		}
	}
	//if this is a floating viewport
	if (!bFoundViewport)
	{
		// Clean up floating viewports
		for( INT FloatingViewportIndex = 0; FloatingViewportIndex < FloatingViewports.Num(); ++FloatingViewportIndex )
		{
			FVCD_Viewport* FloatingViewport = FloatingViewports( FloatingViewportIndex );
			if( FloatingViewport->ViewportWindow &&  FloatingViewport->ViewportWindow->Viewport == Viewport )
			{
				//adjust frame relative to client size
				//ViewportWidth = ViewportWidth + Viewport->ViewportClient->get
				FloatingViewport->FloatingViewportFrame->SetSize(ViewportWidth, ViewportHeight);
				break;
			}
		}
	}
}

/**
 * Finds which SplitterWindow contains the specified window.
 * Returns the index in the SplitterWindow array, or -1 if not found.
 * It also returns which window it was:
 *    WindowID == 1 if it was the first window (GetWindow1)
 *    WindowID == 2 if it was the second window (GetWindow2)
 * WindowID may be NULL.
 */
INT FViewportConfig_Data::FindSplitter( wxWindow* ContainedWindow, INT *WhichWindow/*=NULL*/ )
{
	// First, find the ViewportWindow:
	INT i;
	wxSplitterWindow* Splitter;
	INT Found=0;
	for( i=0; i < SplitterWindows.Num(); ++i )
	{
		Splitter = SplitterWindows(i);
		if ( ContainedWindow == Splitter->GetWindow1() )
		{
			Found = 1;
			break;
		}
		if ( ContainedWindow == Splitter->GetWindow2() )
		{
			Found = 2;
			break;
		}
	}
	if ( !Found )
	{
		return -1;
	}
	if ( WhichWindow )
	{
		*WhichWindow = Found;
	}
	return i;
}



/**
 * Opens a new floating viewport window
 *
 * @param InNewViewportParams - The settings to use for the newly created viewport
 * @param OutViewportIndex [out] Index of newly created viewport
 *
 * @return Returns TRUE if everything went OK
 */
UBOOL FViewportConfig_Data::OpenNewFloatingViewport(const FFloatingViewportParams& InViewportParams, INT& OutViewportIndex, UBOOL bDisablePlayInViewport )
{
	// This will be filled in later if everything goes OK
	OutViewportIndex = INDEX_NONE;

	// Floating viewports will never default to being listeners
	const UBOOL bSetListenerPosition = FALSE;

	// Setup window size
	wxSize WindowSize( InViewportParams.Width, InViewportParams.Height );
	if( WindowSize.GetX() <= 0 || WindowSize.GetY() <= 0 )
	{
		WindowSize = wxSize( 1280, 720 );
	}

	// Create a container window for the floating viewport
	WxFloatingViewportFrame* NewViewportFrame = NULL;
	{
		// Start off with a default frame style (caption, resizable, minimize, maximize, close, system menu, child clip)
		INT ContainerWindowWxStyle = wxDEFAULT_FRAME_STYLE;

		// We don't want the window to appear in the task bar on platforms that support that
		ContainerWindowWxStyle |= wxFRAME_NO_TASKBAR;

		// We never want the window to be hidden by the main UnrealEd window
		ContainerWindowWxStyle |= wxFRAME_FLOAT_ON_PARENT;

		// Setup window title
		FString WindowTitle;
		if (InViewportParams.Title.Len() > 0)
		{
			WindowTitle = InViewportParams.Title;
		} 
		else
		{
			WindowTitle = FString::Printf( LocalizeSecure( LocalizeUnrealEd( "FloatingViewportWindowTitle_F" ), FloatingViewports.Num() ) );
		}

		// Create the viewport frame!
		NewViewportFrame = new WxFloatingViewportFrame( InViewportParams.ParentWxWindow, -1, WindowTitle, wxDefaultPosition, WindowSize, ContainerWindowWxStyle );
		check( NewViewportFrame != NULL );
	}


	// Create and initialize viewport window
	WxLevelViewportWindow* NewLevelViewportWindow = new WxLevelViewportWindow();
	check( NewLevelViewportWindow != NULL );
	{
		if( !NewLevelViewportWindow->Create( NewViewportFrame, -1, wxDefaultPosition, WindowSize, 0 ) )
		{
			NewLevelViewportWindow->Destroy();
			NewViewportFrame->Destroy();
			return FALSE;
		}
		
		// Let the viewport window know that it's going to be floating.  Must be called before we call SetUp!
		NewLevelViewportWindow->SetFloatingViewport( TRUE );
		NewLevelViewportWindow->AllowPlayInViewport( !bDisablePlayInViewport );
		NewLevelViewportWindow->SetLabel( wxT( "FloatingLevelViewport" ) );
		NewLevelViewportWindow->SetUp( InViewportParams.ViewportType, bSetListenerPosition, InViewportParams.ShowFlags );
		NewLevelViewportWindow->SetEditorFrameClient(TRUE);
	}


	// Create and initialize viewport window container
	FVCD_Viewport* NewViewport = new FVCD_Viewport();
	check( NewViewport != NULL );
	{
		NewViewport->bEnabled = TRUE;
		NewViewport->ViewportType = NewLevelViewportWindow->ViewportType = InViewportParams.ViewportType;
		NewViewport->ShowFlags = NewLevelViewportWindow->ShowFlags = InViewportParams.ShowFlags;
		NewViewport->bSetListenerPosition = NewLevelViewportWindow->bSetListenerPosition = bSetListenerPosition;
		NewViewport->ViewportWindow = NewLevelViewportWindow;
		NewViewport->FloatingViewportFrame = NewViewportFrame;
	}


	if( NewLevelViewportWindow->ViewportType == LVT_Perspective )
	{
		// Only perspective windows will have Matinee preview features turned on by default
		NewLevelViewportWindow->SetAllowMatineePreview( TRUE );

		// Assign default camera location/rotation for perspective camera
		NewLevelViewportWindow->ViewLocation = EditorViewportDefs::DefaultPerspectiveViewLocation;
		NewLevelViewportWindow->ViewRotation = EditorViewportDefs::DefaultPerspectiveViewRotation;
	}

	// @todo: Support loading/saving floating viewport camera speed pref?
	// 	FString Key = FString::Printf( TEXT("Viewport%d"), x );
	// 	GConfig->GetFloat( TEXT("ViewportConfig"), *(Key+TEXT("_CameraSpeed")), Viewports[x].ViewportWindow->CameraSpeed, GEditorUserSettingsIni );


	// Add new viewport to floating viewport list
	OutViewportIndex = 4 + FloatingViewports.Num();
	FloatingViewports.AddItem( NewViewport );

	// Assign callback interface to viewport frame so we'll find out about OnClose events
	NewViewportFrame->SetCallbackInterface( this );

	// OK, now show the window!
	NewViewportFrame->Show( TRUE );


	return TRUE;
}



void FViewportConfig_Data::OnFloatingViewportClosed( WxFloatingViewportFrame* InViewportFrame )
{
	// Clean up floating viewports
	for( INT FloatingViewportIndex = 0; FloatingViewportIndex < FloatingViewports.Num(); ++FloatingViewportIndex )
	{
		FVCD_Viewport* FloatingViewport = FloatingViewports( FloatingViewportIndex );
		if( FloatingViewport != NULL )
		{
			if( FloatingViewport->FloatingViewportFrame == InViewportFrame )
			{
				// OK, this viewport is being destroyed

				// See if the viewport was acting as a view parent
				if ( FloatingViewport->ViewportWindow->ViewState->IsViewParent() )
				{
					FlushRenderingCommands();
			
					// Clear all existing view parent status
					for( INT ViewportIndex = 0; ViewportIndex < GetViewportCount(); ++ViewportIndex )
					{
						FVCD_Viewport& CurViewport = AccessViewport( ViewportIndex );
						if( CurViewport.bEnabled )
						{
							CurViewport.ViewportWindow->ViewState->SetViewParent( NULL );
							CurViewport.ViewportWindow->Invalidate();
						}
					}
				}

				// NOTE: The viewport windows will take care of destroying themselves after this function is called
				FloatingViewport->FloatingViewportFrame = NULL;
				FloatingViewport->ViewportWindow = NULL;

				// Kill our wrapper object
				delete FloatingViewport;

				// Remove from the list
				FloatingViewports.Remove( FloatingViewportIndex );
				break;
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	WxLevelViewportWindow.
-----------------------------------------------------------------------------*/

/** Define a custom event type to use to update the camera speed UI */
DEFINE_EVENT_TYPE( EVT_CAMSPEED_UPDATE )

BEGIN_EVENT_TABLE( WxLevelViewportWindow, wxWindow )
	EVT_SIZE( WxLevelViewportWindow::OnSize )
	EVT_SET_FOCUS( WxLevelViewportWindow::OnSetFocus )
	EVT_MENU_RANGE( ID_CAMSPEED_SLOW, ID_CAMSPEED_VERYFAST, WxLevelViewportWindow::OnCameraSpeedSelection )
	EVT_UPDATE_UI( ID_CAMSPEED_CYCLE_BUTTON, WxLevelViewportWindow::OnCameraUIUpdate )
END_EVENT_TABLE()

WxLevelViewportWindow::WxLevelViewportWindow()
	: FEditorLevelViewportClient()
{
	bAllowAmbientOcclusion = TRUE;
	ToolBar = NULL;
	MaximizeToolBar = NULL;
	Viewport = NULL;
	bVariableFarPlane = TRUE;
	bDrawVertices = TRUE;

	// Draw the base attachment volume in this viewport
	bDrawBaseInfo = TRUE;
}

WxLevelViewportWindow::~WxLevelViewportWindow()
{
	GEngine->Client->CloseViewport(Viewport);
	delete ToolBar;
	Viewport = NULL;
}

void WxLevelViewportWindow::SetUp( ELevelViewportType InViewportType, UBOOL bInSetListenerPosition, EShowFlags InShowFlags )
{
	// Set viewport parameters first.  These may be queried by the level viewport toolbar, etc.
	ViewportType = InViewportType;
	bSetListenerPosition = bInSetListenerPosition;
	ShowFlags = InShowFlags;
	LastShowFlags = InShowFlags;

	// Validate show flags
	{
		EShowFlags ViewModeShowFlags = ShowFlags & SHOW_ViewMode_Mask;

		// Valid mode that we can represent on the level viewport tool bar, good to go!
		const EShowFlags FlagsValidForToolbar =
			  SHOW_ViewMode_BrushWireframe
			| SHOW_ViewMode_Wireframe
			| SHOW_ViewMode_Unlit
			| SHOW_ViewMode_Lit
			| SHOW_ViewMode_LightingOnly
			| SHOW_ViewMode_LightComplexity
			| SHOW_ViewMode_TextureDensity
			| SHOW_ViewMode_ShaderComplexity
			| SHOW_ViewMode_LightMapDensity
			| SHOW_ViewMode_LitLightmapDensity;

		// Not a valid mode, but still sometimes OK in the editor, such as when the user
		// toggles fog on and off while in a normal view mode.  We allow this in the editor but
		// we don't want to restore this mode at startup.
		if( !(ViewModeShowFlags & FlagsValidForToolbar) )
		{
			debugf(TEXT("Viewport flags are invalid: Setting to defaults!"));
			EShowFlags NewViewModeFlags = SHOW_ViewMode_Lit;
			if (IsPerspective() == FALSE)
			{
				// It's an ortho...
				NewViewModeFlags = SHOW_ViewMode_BrushWireframe;
			}

			ShowFlags &= ~SHOW_ViewMode_Mask;
			ShowFlags |= NewViewModeFlags;

			LastShowFlags = ShowFlags;
		}
	}

	// Create viewport
	Viewport = GEngine->Client->CreateWindowChildViewport( (FViewportClient*)this, (HWND)GetHandle() );
	if( Viewport != NULL )
	{
		Viewport->CaptureJoystickInput(false);
		::SetWindowText( (HWND)Viewport->GetWindow(), TEXT("Viewport") );
	}

	SetDropTarget(new WxObjectPathNameDropTarget(this));

	// ToolBar
	ToolBar = new WxLevelViewportToolBar( this, -1, this );
	ToolBar->SetLabel( wxT("ToolBar") );

	// Toolbar for the maximize button
	MaximizeToolBar = new WxLevelViewportMaximizeToolBar( this, -1, this );
	MaximizeToolBar->SetLabel( wxT("DialogOptions") );

	ResizeToolBar();

	ToolBar->UpdateUI();
}

/**
 * Forces an OnSize event
 */
void WxLevelViewportWindow::ResizeToolBar()
{
	wxSizeEvent DummyEvent;
	OnSize( DummyEvent );
}

UBOOL WxLevelViewportWindow::InputAxis(FViewport* Viewport,INT ControllerId,FName Key,FLOAT Delta,FLOAT DeltaTime, UBOOL bGamePad )
{
	return FEditorLevelViewportClient::InputAxis( Viewport, ControllerId, Key, Delta, DeltaTime, bGamePad );
}

/**
 * Set the viewport type of the client
 *
 * @param InViewportType	The viewport type to set the client to
 */
void WxLevelViewportWindow::SetViewportType( ELevelViewportType InViewportType )
{
	FEditorLevelViewportClient::SetViewportType( InViewportType );
	if ( ToolBar )
	{
		ToolBar->UpdateUI();
	}
}

void WxLevelViewportWindow::GetViewportDimensions( FIntPoint& out_Origin, FIntPoint& out_Size )
{
	if ( (ToolBar != NULL) && (MaximizeToolBar != NULL) )
	{
		wxRect rc = GetClientRect();
		INT MaximizeToolBarHeight = IsFloatingViewport()? 0: MaximizeToolBar->GetDialogOptionsHeight();
		INT ToolBarHeight = ToolBar->GetToolbarHeight();
		INT MaxHeight = Max(ToolBarHeight, MaximizeToolBarHeight);
		rc.y += MaxHeight+1;
		rc.height -= MaxHeight+1;

		out_Origin.X = rc.GetLeft() + 1;
		out_Origin.Y = rc.GetTop() + 1;
		out_Size.X = rc.GetWidth() - 2;
		out_Size.Y = rc.GetHeight() - 2;
	}
	else
	{
		FEditorLevelViewportClient::GetViewportDimensions(out_Origin, out_Size);
	}
}

void WxLevelViewportWindow::OnSize( wxSizeEvent& InEvent )
{
	if( ToolBar && MaximizeToolBar )
	{
		wxRect rc = GetClientRect();
		INT MaximizeToolBarHeight = IsFloatingViewport()? 0: MaximizeToolBar->GetDialogOptionsHeight();
		INT MaximizeToolBarWidth = IsFloatingViewport()? 0: MaximizeToolBar->GetDialogOptionsWidth();
		INT ToolBarHeight = ToolBar->GetToolbarHeight();
		INT ToolBarWidth = rc.GetWidth() - MaximizeToolBarWidth;
		INT MaxHeight = Max(ToolBarHeight, MaximizeToolBarHeight);
		rc.y += MaxHeight+1;
		rc.height -= MaxHeight+1;

		ToolBar->SetSize( ToolBarWidth, MaxHeight+2 );
		MaximizeToolBar->Move( ToolBarWidth, 0 );
		MaximizeToolBar->SetSize( MaximizeToolBarWidth, MaxHeight+2 );
		::SetWindowPos( (HWND)Viewport->GetWindow(), HWND_TOP, rc.GetLeft()+1, rc.GetTop()+1, rc.GetWidth()-2, rc.GetHeight()-2, SWP_SHOWWINDOW );
	}
	InEvent.Skip();
}

void WxLevelViewportWindow::OnSetFocus(wxFocusEvent& In)
{
	if ( Viewport )
	{
		::SetFocus( (HWND) Viewport->GetWindow() );
	}
}

/**
 * Called in response to the camera speed being altered via toolbar button
 *
 * @param	Event	Event containing the Id specifying which speed was selected
 */
void WxLevelViewportWindow::OnCameraSpeedSelection(wxCommandEvent& Event)
{
	const INT EventID = Event.GetId();
	
	// Set the speed of the camera in response to the ID sent from clicking
	// the camera speed button
	switch ( EventID )
	{
		case ID_CAMSPEED_SLOW:
			CameraSpeed = MOVEMENTSPEED_SLOW; 
			break;
		case ID_CAMSPEED_NORMAL:
			CameraSpeed = MOVEMENTSPEED_NORMAL;
			break;
		case ID_CAMSPEED_FAST:
			CameraSpeed = MOVEMENTSPEED_FAST;
			break;
		case ID_CAMSPEED_VERYFAST:
			CameraSpeed = MOVEMENTSPEED_VERYFAST;
			break;
		default:
			checkf( 0, TEXT("Camera Speed selection sent an invalid ID!") );
			break;
	};
}

/**
 * Called during idle time by wxWidgets to update the UI in relation to the camera
 * speed button
 *
 * @param	Event	Event generated by wxWidgets in relation to the camera speed button
 */
void WxLevelViewportWindow::OnCameraUIUpdate(wxUpdateUIEvent& Event)
{
	// Determine the event ID to send to the toolbar based upon the currently set camera speed
	INT CurSpeedID;
	if( CameraSpeed <= MOVEMENTSPEED_SLOW )
	{
		CurSpeedID = ID_CAMSPEED_SLOW;
	}
	else
	if( CameraSpeed <= MOVEMENTSPEED_NORMAL )
	{
		CurSpeedID = ID_CAMSPEED_NORMAL;
	}
	else
	if( CameraSpeed <= MOVEMENTSPEED_FAST )
	{
		CurSpeedID = ID_CAMSPEED_FAST;
	}
	else
	{
		CurSpeedID = ID_CAMSPEED_VERYFAST;
	}

	// Create an event to send to the toolbar containing the current speed ID
	// (With this, the toolbar can update its icon if it has gotten out of sync with the viewport window)
	wxCommandEvent CameraSpeedEvent( EVT_CAMSPEED_UPDATE, CurSpeedID );
	ToolBar->GetEventHandler()->ProcessEvent( CameraSpeedEvent );
}

/**
 * Checks to see if the current input event modified any show flags.
 * @param Key				Key that was pressed.
 * @param bControlDown		Flag for whether or not the control key is held down.
 * @param bAltDown			Flag for whether or not the alt key is held down.
 * @param bShiftDown		Flag for whether or not the shift key is held down.
 * @return					Flag for whether or not we handled the input.
 */
UBOOL WxLevelViewportWindow::CheckForShowFlagInput(FName Key, UBOOL bControlDown, UBOOL bAltDown, UBOOL bShiftDown)
{
	UBOOL ReturnValue = FEditorLevelViewportClient::CheckForShowFlagInput(Key, bControlDown, bAltDown, bShiftDown);
	if ( ReturnValue )
	{
		ToolBar->UpdateUI();
	}	
	return ReturnValue;
}



/*-----------------------------------------------------------------------------
	WxFloatingViewportFrame
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxFloatingViewportFrame, WxTrackableFrame )
	EVT_CLOSE( WxFloatingViewportFrame::OnClose )
	EVT_SIZE( WxFloatingViewportFrame::OnSize )
END_EVENT_TABLE()


WxFloatingViewportFrame::WxFloatingViewportFrame( wxWindow* InParent, wxWindowID InID, const FString& InTitle, const wxPoint& pos, const wxSize& size, long style )
	: WxTrackableFrame( InParent, InID, *InTitle, pos, size, style ),
	  CallbackInterface( NULL )
{
}



WxFloatingViewportFrame::~WxFloatingViewportFrame()
{
	/**Inform owning window that we are now closing*/
	if( CallbackInterface != NULL )
	{
		CallbackInterface->OnFloatingViewportClosed( this );
	}
	CallbackInterface = NULL;

	// Kill the window's children, and the window itself
	DestroyChildren();
	Destroy();
}



/** Sets the callback interface for this viewport */
void WxFloatingViewportFrame::SetCallbackInterface( IFloatingViewportCallback* NewCallbackInterface )
{
	CallbackInterface = NewCallbackInterface;
}



/** Called when the window is closed */
void WxFloatingViewportFrame::OnClose( wxCloseEvent& InEvent )
{
	if( CallbackInterface != NULL )
	{
		CallbackInterface->OnFloatingViewportClosed( this );
	}

	// Kill the window's children, and the window itself
	DestroyChildren();
	Destroy();
}



/** Called when the window is resized */
void WxFloatingViewportFrame::OnSize( wxSizeEvent& InEvent )
{
	// Update size of child windows
	for( UINT CurChildWindowIndex = 0; CurChildWindowIndex < GetChildren().size(); ++CurChildWindowIndex )
	{
		wxWindow* CurChildWindow = GetChildren()[ CurChildWindowIndex ];
		CurChildWindow->SetSize( GetClientRect().GetSize() );
	}
	InEvent.Skip();
}



/** This function is called when the WxTrackableDialog has been selected from within the ctrl + tab dialog. */
void WxFloatingViewportFrame::OnSelected()
{
	// This is kind of weird right here.  Ideally, WxTrackableFrame would be doing this instead of our derived class
	// (and similar derived classes), just like WxTrackableDialog does.  However, it seems that would introduce 
	// window focus problems with very specific frame windows, so we do it here in our derived class.
	Raise();
}



/*-----------------------------------------------------------------------------
	WxViewportHolder
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxViewportHolder, wxPanel )
	EVT_SIZE(WxViewportHolder::OnSize)
END_EVENT_TABLE()

WxViewportHolder::WxViewportHolder( wxWindow* InParent, wxWindowID InID, bool InWantScrollBar, const wxPoint& pos, const wxSize& size, long style)
	: wxPanel( InParent, InID, pos, size, style ), bAutoDestroy(FALSE)
{
	Viewport = NULL;
	ScrollBar = NULL;
	SBPos = SBRange = 0;

	// Set a black background
	SetBackgroundColour( wxColour( 0, 0, 0 ) );

	if( InWantScrollBar )
		ScrollBar = new wxScrollBar( this, ID_BROWSER_SCROLL_BAR, wxDefaultPosition, wxDefaultSize, wxSB_VERTICAL );
}

WxViewportHolder::~WxViewportHolder()
{
	if ( bAutoDestroy )
	{
		GEngine->Client->CloseViewport(Viewport);
		SetViewport(NULL);
	}
}

void WxViewportHolder::SetViewport( FViewport* InViewport )
{
	Viewport = InViewport;
}

void WxViewportHolder::OnSize( wxSizeEvent& InEvent )
{
	if( Viewport )
	{
		wxRect rc = GetClientRect();
		wxRect rcSB;
		if( ScrollBar )
			rcSB = ScrollBar->GetClientRect();

		SetWindowPos( (HWND)Viewport->GetWindow(), HWND_TOP, rc.GetLeft(), rc.GetTop(), rc.GetWidth()-rcSB.GetWidth(), rc.GetHeight(), SWP_SHOWWINDOW );

		if( ScrollBar )
			ScrollBar->SetSize( rc.GetLeft()+rc.GetWidth()-rcSB.GetWidth(), rc.GetTop(), rcSB.GetWidth(), rc.GetHeight() );
	}

	InEvent.Skip();
}

// Updates the scrollbar so it looks right and is in the right position

void WxViewportHolder::UpdateScrollBar( INT InPos, INT InRange )
{
	if( ScrollBar )
		ScrollBar->SetScrollbar( InPos, Viewport->GetSizeY(), InRange, Viewport->GetSizeY() );
}

INT WxViewportHolder::GetScrollThumbPos()
{
	return ( ScrollBar ? ScrollBar->GetThumbPosition() : 0 );
}

void WxViewportHolder::SetScrollThumbPos( INT InPos )
{
	if( ScrollBar )
		ScrollBar->SetThumbPosition( InPos );
}




/*-----------------------------------------------------------------------------
	WxPIEContainerWindow
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxPIEContainerWindow, wxPanel )
	EVT_SIZE( WxPIEContainerWindow::OnSize )
	EVT_SET_FOCUS( WxPIEContainerWindow::OnSetFocus )
END_EVENT_TABLE()


/** Constructor.  Should only be called from within CreatePIEContainerWindow() */
WxPIEContainerWindow::WxPIEContainerWindow( wxWindow* InParent, wxWindowID InID, const wxPoint& pos, const wxSize& size, long style, const FString& InTitle )
	: wxPanel( InParent, InID, pos, size, style, *InTitle ),
	  Viewport( NULL ),
	  bIsEmbeddedInFloatingWindow( FALSE )
{
	// Set a black background
	SetBackgroundColour( wxColour( 0, 0, 0 ) );

	GCallbackEvent->Register( CALLBACK_EndPIE, this );
}



/** Destructor */
WxPIEContainerWindow::~WxPIEContainerWindow()
{
	GCallbackEvent->UnregisterAll( this );

	UBOOL bWasEmbeddedInFloatingWindow  = bIsEmbeddedInFloatingWindow;

	// Make sure everything's been cleaned up
	ClosePIEWindowAndRestoreViewport();

	// Make sure viewport client gets a chance to clean up if the user closed the entire floating window
	if( bWasEmbeddedInFloatingWindow )
	{
		if( Viewport != NULL && Viewport->GetClient() != NULL )
		{
			Viewport->GetClient()->CloseRequested( Viewport );
		}
	}

	// NOTE: We don't need to destroy our Viewport object here since either the window itself, because
	//   the viewport client will take care of that for us.
	Viewport = NULL;
}



/**
 * Creates a Play In Editor viewport window and embeds it into a level editor viewport (if possible)
 *
 * NOTE: This is a static method
 *
 * @param ViewportClient The viewport client the new viewport will be associated with
 * @param TargetViewport The viewport window to possess
 *
 * @return Newly created WxPIEContainerWindow if successful, otherwise NULL
 */
WxPIEContainerWindow* WxPIEContainerWindow::CreatePIEWindowAndPossessViewport( UGameViewportClient* ViewportClient,
																			   FVCD_Viewport* TargetViewport )
{
	// Viewport should never already have a PIE container at this point
	check( TargetViewport->PIEContainerWindow == NULL );

	// Try to create an embedded PIE viewport
	if( TargetViewport->ViewportWindow != NULL )
	{
		// Is this a floating viewport?  If so, we need to handle it differently
		if( TargetViewport->FloatingViewportFrame != NULL )
		{
			// Hide actual viewport window
			TargetViewport->ViewportWindow->Hide();

			// Create and initialize container window
			TargetViewport->PIEContainerWindow =
				new WxPIEContainerWindow(
					TargetViewport->FloatingViewportFrame,
					-1,
					wxPoint( 0, 0 ),
					wxSize( TargetViewport->FloatingViewportFrame->GetClientRect().GetSize() ) );

			// NOTE: We call Initialize after ReplaceWindow so that the size of this window will be correct
			//   before creating the child window
			if( TargetViewport->PIEContainerWindow->Initialize( ViewportClient ) )
			{
				TargetViewport->PIEContainerWindow->bIsEmbeddedInFloatingWindow = TRUE;

				// Done!
				return TargetViewport->PIEContainerWindow;
			}
			else
			{
				// Undo our changes so far
				TargetViewport->PIEContainerWindow->Destroy();
				TargetViewport->PIEContainerWindow = NULL;

				// Restore the viewport window
				TargetViewport->ViewportWindow->Show();
			}
		}
		else
		{
			// Figure out which splitter window is the parent of the current viewport
			INT SplitterIndex = GApp->EditorFrame->ViewportConfigData->FindSplitter( TargetViewport->ViewportWindow );
			if( SplitterIndex >= 0 )
			{
				wxSplitterWindow* ParentSplitterWindow =
					GApp->EditorFrame->ViewportConfigData->SplitterWindows( SplitterIndex );

				// Hide actual viewport window
				TargetViewport->ViewportWindow->Hide();

				// Create and initialize container window
				TargetViewport->PIEContainerWindow = new WxPIEContainerWindow( ParentSplitterWindow, -1 );

				// Replace the viewport window associated with the splitter with the new PIE container window
				ParentSplitterWindow->ReplaceWindow( TargetViewport->ViewportWindow, TargetViewport->PIEContainerWindow );

				// NOTE: We call Initialize after ReplaceWindow so that the size of this window will be correct
				//   before creating the child window
				if( TargetViewport->PIEContainerWindow->Initialize( ViewportClient ) )
				{
					// Done!
					return TargetViewport->PIEContainerWindow;
				}
				else
				{
					// Undo our changes so far
					ParentSplitterWindow->ReplaceWindow( TargetViewport->PIEContainerWindow, TargetViewport->ViewportWindow );

					TargetViewport->PIEContainerWindow->Destroy();
					TargetViewport->PIEContainerWindow = NULL;

					// Restore the viewport window
					TargetViewport->ViewportWindow->Show();
				}
			}
		}
	}

	// Failed
	return NULL;
}



UBOOL WxPIEContainerWindow::Initialize( UGameViewportClient* ViewportClient )
{
	SetLabel( TEXT( "PlayInEditorContainerWindow" ) );

	// Now create a child viewport for this window
	Viewport = GEngine->Client->CreateWindowChildViewport(
		ViewportClient,
		( HWND )GetHandle(),
		GetClientRect().width,
		GetClientRect().height,
		0,
		0 );
	if( Viewport != NULL )
	{
		::SetWindowText( ( HWND )Viewport->GetWindow(), TEXT( "PlayInEditorViewport" ) );

		// Update the viewport client's window pointer
		ViewportClient->SetViewport( Viewport );

		// Set focus to the PIE viewport
		::ShowWindow( ( HWND )Viewport->GetWindow(), SW_SHOW );
		::SetFocus( ( HWND )Viewport->GetWindow() );

		// Done!
		return TRUE;
	}

	return FALSE;
}



/** Closes the the Play-In-Editor window and restores any previous viewport */
void WxPIEContainerWindow::ClosePIEWindowAndRestoreViewport()
{
	// Find the window for this viewport client
	FVCD_Viewport* ViewportWithPIE = NULL;
	{
		const UINT NumViewports = GApp->EditorFrame->ViewportConfigData->GetViewportCount();
		for( UINT CurViewportIndex = 0; CurViewportIndex < NumViewports; ++CurViewportIndex )
		{
			FVCD_Viewport* CurrentViewport = &GApp->EditorFrame->ViewportConfigData->AccessViewport( CurViewportIndex );
			if( CurrentViewport->PIEContainerWindow == this )
			{
				ViewportWithPIE = CurrentViewport;
				break;
			}
		}
	}

	if( ViewportWithPIE != NULL )
	{
		// We'll only need to restore the original viewport if one actually exists
		if( ViewportWithPIE->ViewportWindow != NULL )
		{
			// Is this a floating viewport?  Those are handled a bit differently
			if( ViewportWithPIE->FloatingViewportFrame != NULL )
			{
				// Show the original viewport window that we hid before launching PIE
				ViewportWithPIE->ViewportWindow->Show();
			}
			else
			{
				// Find the splitter window that's the parent of the PIE container window
				INT SplitterIndex = GApp->EditorFrame->ViewportConfigData->FindSplitter( ViewportWithPIE->PIEContainerWindow );
				if( SplitterIndex >= 0 )
				{
					wxSplitterWindow* ParentSplitterWindow =
						GApp->EditorFrame->ViewportConfigData->SplitterWindows( SplitterIndex );

					// Show the original viewport window that we hid before launching PIE
					ViewportWithPIE->ViewportWindow->Show();

					// Replace the PIE container window associated with the splitter with the original viewport
					ParentSplitterWindow->ReplaceWindow( ViewportWithPIE->PIEContainerWindow, ViewportWithPIE->ViewportWindow );
				}
			}
		}

		// Detach from the editor viewport completely
		ViewportWithPIE->PIEContainerWindow = NULL;
	}

	// No longer embedded
	bIsEmbeddedInFloatingWindow = FALSE;
}



void WxPIEContainerWindow::OnSize( wxSizeEvent& InEvent )
{
	if( Viewport != NULL )
	{
		wxRect rc = GetClientRect();

		// Update the viewport window's size
		::SetWindowPos( ( HWND )Viewport->GetWindow(), HWND_TOP, 0, 0, rc.GetWidth(), rc.GetHeight(), SWP_SHOWWINDOW );
	}

	InEvent.Skip();
}

/** Called when the window receives focus */
void WxPIEContainerWindow::OnSetFocus( wxFocusEvent& In )
{
	if ( Viewport )
	{
		::SetFocus( (HWND) Viewport->GetWindow() );
	}
}


/** Called from the global event handler when a registered event is fired */
void WxPIEContainerWindow::Send( ECallbackEventType InType )
{
	if( InType == CALLBACK_EndPIE )
	{
		// Close the Play-In-Editor window and restore a previous viewport if needed
		// NOTE: This will call Destroy on the window!
		ClosePIEWindowAndRestoreViewport();

		// Also, kill self
		Destroy();
	}
}
