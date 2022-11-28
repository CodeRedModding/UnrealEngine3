
#include "UnrealEd.h"

/*-----------------------------------------------------------------------------
	FViewportConfig_Viewport.
-----------------------------------------------------------------------------*/

FViewportConfig_Viewport::FViewportConfig_Viewport()
{
	ViewportType = LVT_Perspective;
	ViewMode = SVM_BrushWireframe;
	ShowFlags = SHOW_DefaultEditor | SHOW_ModeWidgets;
	bEnabled = 0;
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

			Desc = TEXT("2x2 Split");

			ViewportTemplates[0].bEnabled = 1;
			ViewportTemplates[0].ViewportType = LVT_OrthoXY;
			ViewportTemplates[0].ViewMode = SVM_BrushWireframe;

			ViewportTemplates[1].bEnabled = 1;
			ViewportTemplates[1].ViewportType = LVT_OrthoXZ;
			ViewportTemplates[1].ViewMode = SVM_BrushWireframe;

			ViewportTemplates[2].bEnabled = 1;
			ViewportTemplates[2].ViewportType = LVT_OrthoYZ;
			ViewportTemplates[2].ViewMode = SVM_BrushWireframe;

			ViewportTemplates[3].bEnabled = 1;
			ViewportTemplates[3].ViewportType = LVT_Perspective;
			ViewportTemplates[3].ViewMode = SVM_Lit;

			break;

		case VC_1_2_Split:

			Desc = TEXT("1x2 Split");

			ViewportTemplates[0].bEnabled = 1;
			ViewportTemplates[0].ViewportType = LVT_Perspective;
			ViewportTemplates[0].ViewMode = SVM_Lit;

			ViewportTemplates[1].bEnabled = 1;
			ViewportTemplates[1].ViewportType = LVT_OrthoXY;
			ViewportTemplates[1].ViewMode = SVM_BrushWireframe;

			ViewportTemplates[2].bEnabled = 1;
			ViewportTemplates[2].ViewportType = LVT_OrthoXZ;
			ViewportTemplates[2].ViewMode = SVM_BrushWireframe;

			break;

		case VC_1_1_Split:

			Desc = TEXT("1x1 Split");

			ViewportTemplates[0].bEnabled = 1;
			ViewportTemplates[0].ViewportType = LVT_Perspective;
			ViewportTemplates[0].ViewMode = SVM_Lit;

			ViewportTemplates[1].bEnabled = 1;
			ViewportTemplates[1].ViewportType = LVT_OrthoXY;
			ViewportTemplates[1].ViewMode = SVM_BrushWireframe;

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
	ShowFlags = SHOW_DefaultEditor | SHOW_ModeWidgets;

	ViewMode = SVM_BrushWireframe;
	SashPos = 0;
}

FVCD_Viewport::~FVCD_Viewport()
{
}

/*-----------------------------------------------------------------------------
	FViewportConfig_Data.
-----------------------------------------------------------------------------*/

FViewportConfig_Data::FViewportConfig_Data()
{
}

FViewportConfig_Data::~FViewportConfig_Data()
{
}

/**
 * Tells this viewport configuration to create its windows and apply its settings.
 */

void FViewportConfig_Data::Apply( wxWindow* InParent )
{
	SplitterWindows.Empty();

	for( INT x = 0 ; x < 4 ; ++x )
	{
		if( Viewports[x].bEnabled && Viewports[x].ViewportWindow != NULL )
		{
			Viewports[x].ViewportWindow->DestroyChildren();
			Viewports[x].ViewportWindow->Destroy();
			Viewports[x].ViewportWindow = NULL;
		}
	}

	wxRect rc = InParent->GetClientRect();

	// Set up the splitters and viewports as per the template defaults.

	wxSplitterWindow *MainSplitter = NULL;
	INT SashPos = 0;
	FString Key;

	switch( Template )
	{
		case VC_2_2_Split:
		{
			wxSplitterWindow *TopSplitter, *BottomSplitter;

			MainSplitter = new wxSplitterWindow( InParent, ID_SPLITTERWINDOW, wxPoint(0,0), wxSize(rc.GetWidth(), rc.GetHeight()), wxSP_3D );
			TopSplitter = new wxSplitterWindow( MainSplitter, ID_SPLITTERWINDOW+1, wxPoint(0,0), wxSize(rc.GetWidth(), rc.GetHeight()/2), wxSP_3D );
			BottomSplitter = new wxSplitterWindow( MainSplitter, ID_SPLITTERWINDOW+2, wxPoint(0,0), wxSize(rc.GetWidth(), rc.GetHeight()/2), wxSP_3D );

			SplitterWindows.AddItem( MainSplitter );
			SplitterWindows.AddItem( TopSplitter );
			SplitterWindows.AddItem( BottomSplitter );

			for( INT x = 0 ; x < 4 ; ++x )
			{
				if( Viewports[x].bEnabled )
				{
					Viewports[x].ViewportWindow = new WxLevelViewportWindow;
					Viewports[x].ViewportWindow->Create( MainSplitter, -1 );
					Viewports[x].ViewportWindow->SetUp( x, Viewports[x].ViewportType, Viewports[x].ViewMode, Viewports[x].ShowFlags );
				}
			}

			Viewports[0].ViewportWindow->Reparent( TopSplitter );
			Viewports[1].ViewportWindow->Reparent( TopSplitter );
			Viewports[2].ViewportWindow->Reparent( BottomSplitter );
			Viewports[3].ViewportWindow->Reparent( BottomSplitter );

			GConfig->GetInt( TEXT("ViewportConfig"), TEXT("Splitter0"), SashPos, GEditorIni );
			MainSplitter->SplitHorizontally( TopSplitter, BottomSplitter, SashPos );
			GConfig->GetInt( TEXT("ViewportConfig"), TEXT("Splitter1"), SashPos, GEditorIni );
			TopSplitter->SplitVertically( Viewports[0].ViewportWindow, Viewports[1].ViewportWindow, SashPos );
			GConfig->GetInt( TEXT("ViewportConfig"), TEXT("Splitter2"), SashPos, GEditorIni );
			BottomSplitter->SplitVertically( Viewports[3].ViewportWindow, Viewports[2].ViewportWindow, SashPos );
		}
		break;

		case VC_1_2_Split:
		{
			wxSplitterWindow *RightSplitter;

			MainSplitter = new wxSplitterWindow( InParent, ID_SPLITTERWINDOW, wxPoint(0,0), wxSize(rc.GetWidth(), rc.GetHeight()), wxSP_3D );
			RightSplitter = new wxSplitterWindow( MainSplitter, ID_SPLITTERWINDOW+1, wxPoint(0,0), wxSize(rc.GetWidth(), rc.GetHeight()/2), wxSP_3D );

			SplitterWindows.AddItem( MainSplitter );
			SplitterWindows.AddItem( RightSplitter );

			for( INT x = 0 ; x < 4 ; ++x )
			{
				if( Viewports[x].bEnabled )
				{
					Viewports[x].ViewportWindow = new WxLevelViewportWindow;
					Viewports[x].ViewportWindow->Create( MainSplitter, -1 );
					Viewports[x].ViewportWindow->SetUp( x, Viewports[x].ViewportType, Viewports[x].ViewMode, Viewports[x].ShowFlags );
				}
			}

			Viewports[0].ViewportWindow->Reparent( MainSplitter );
			Viewports[1].ViewportWindow->Reparent( RightSplitter );
			Viewports[2].ViewportWindow->Reparent( RightSplitter );

			GConfig->GetInt( TEXT("ViewportConfig"), TEXT("Splitter0"), SashPos, GEditorIni );
			MainSplitter->SplitVertically( Viewports[0].ViewportWindow, RightSplitter, SashPos );
			GConfig->GetInt( TEXT("ViewportConfig"), TEXT("Splitter1"), SashPos, GEditorIni );
			RightSplitter->SplitHorizontally( Viewports[1].ViewportWindow, Viewports[2].ViewportWindow, SashPos );
		}
		break;

		case VC_1_1_Split:
		{
			MainSplitter = new wxSplitterWindow( InParent, ID_SPLITTERWINDOW, wxPoint(0,0), wxSize(rc.GetWidth(), rc.GetHeight()), wxSP_3D );

			SplitterWindows.AddItem( MainSplitter );


			for( INT x = 0 ; x < 4 ; ++x )
			{
				if( Viewports[x].bEnabled )
				{
					Viewports[x].ViewportWindow = new WxLevelViewportWindow;
					Viewports[x].ViewportWindow->Create( MainSplitter, -1 );
					Viewports[x].ViewportWindow->SetUp( x, Viewports[x].ViewportType, Viewports[x].ViewMode, Viewports[x].ShowFlags );
				}
			}

			Viewports[0].ViewportWindow->Reparent( MainSplitter );
			Viewports[1].ViewportWindow->Reparent( MainSplitter );

			GConfig->GetInt( TEXT("ViewportConfig"), TEXT("Splitter0"), SashPos, GEditorIni );
			MainSplitter->SplitHorizontally( Viewports[0].ViewportWindow, Viewports[1].ViewportWindow, SashPos );
		}
		break;
	}

	// Make sure the splitters will resize with the editor

	Sizer = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer* WkSizer = new wxBoxSizer( wxHORIZONTAL );
	WkSizer->Add( MainSplitter, 100, wxEXPAND | wxALL, 0 );

	Sizer->Add( WkSizer, 1, wxEXPAND | wxALL );
	InParent->SetSizer( Sizer );
	
	// Apply the custom settings contained in this instance.

	for( INT x = 0 ; x < 4 ; ++x )
	{
		if( Viewports[x].bEnabled )
		{
			Viewports[x].ViewportWindow->ViewportType = Viewports[x].ViewportType;
			Viewports[x].ViewportWindow->ShowFlags = Viewports[x].ShowFlags;
			Viewports[x].ViewportWindow->ViewMode = Viewports[x].ViewMode;
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
	for( INT x = 0 ; x < 4 ; ++x )
	{
		FVCD_Viewport* viewport = &Viewports[x];

		if( viewport->bEnabled )
		{
			viewport->ViewportType = (ELevelViewportType)viewport->ViewportWindow->ViewportType;
			viewport->ShowFlags = viewport->ViewportWindow->ShowFlags;
			viewport->ViewMode = viewport->ViewportWindow->ViewMode;
		}
	}
}

/**
 * Loads the custom data elements from InData to this instance.
 *
 * @param	InData	The instance to load the data from.
 */

void FViewportConfig_Data::Load( FViewportConfig_Data* InData )
{
	for( INT x = 0 ; x < 4 ; ++x )
	{
		FVCD_Viewport* Src = &InData->Viewports[x];

		if( Src->bEnabled )
		{
			// Find a matching viewport to copy the data into

			for( INT y = 0 ; y < 4 ; ++y )
			{
				FVCD_Viewport* Dst = &Viewports[y];

				if( Dst->bEnabled && Dst->ViewportType == Src->ViewportType )
				{
					Dst->ViewportType = Src->ViewportType;
					Dst->ShowFlags = Src->ShowFlags;
					Dst->ViewMode = Src->ViewMode;
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
	Save();

	GConfig->EmptySection( TEXT("ViewportConfig"), GEditorIni );
	GConfig->SetInt( TEXT("ViewportConfig"), TEXT("Template"), Template, GEditorIni );

	for( INT x = 0 ; x < SplitterWindows.Num() ; ++x )
	{
		FString Key = FString::Printf( TEXT("Splitter%d"), x );
		GConfig->SetInt( TEXT("ViewportConfig"), *Key, SplitterWindows(x)->GetSashPosition(), GEditorIni );
	}

	for( INT x = 0 ; x < 4 ; ++x )
	{
		FVCD_Viewport* viewport = &Viewports[x];

		FString Key = FString::Printf( TEXT("Viewport%d"), x );

		GConfig->SetBool( TEXT("ViewportConfig"), *(Key+TEXT("_Enabled")), viewport->bEnabled, GEditorIni );

		if( viewport->bEnabled )
		{
			GConfig->SetInt( TEXT("ViewportConfig"), *(Key+TEXT("_ViewportType")), viewport->ViewportType, GEditorIni );
			GConfig->SetInt( TEXT("ViewportConfig"), *(Key+TEXT("_ShowFlags")), viewport->ShowFlags, GEditorIni );
			GConfig->SetInt( TEXT("ViewportConfig"), *(Key+TEXT("_ViewMode")), viewport->ViewMode, GEditorIni );
		}
	}
}

/**
 * Attempts to load the viewport configuration from the editors INI file.  If unsuccessful,
 * it returns 0 to the caller.
 */

UBOOL FViewportConfig_Data::LoadFromINI()
{
	INT Wk = VC_None;
	GConfig->GetInt( TEXT("ViewportConfig"), TEXT("Template"), Wk, GEditorIni );

	if( Wk == VC_None )
	{
		return 0;
	}

	Template = (EViewportConfig)Wk;
	GApp->EditorFrame->ViewportConfigData->SetTemplate( Template );

	for( INT x = 0 ; x < 4 ; ++x )
	{
		FVCD_Viewport* viewport = &Viewports[x];

		FString Key = FString::Printf( TEXT("Viewport%d"), x );

		GConfig->GetBool( TEXT("ViewportConfig"), *(Key+TEXT("_Enabled")), viewport->bEnabled, GEditorIni );

		if( viewport->bEnabled )
		{
			GConfig->GetInt( TEXT("ViewportConfig"), *(Key+TEXT("_ViewportType")), Wk, GEditorIni );
			viewport->ViewportType = (ELevelViewportType)Wk;

			GConfig->GetInt( TEXT("ViewportConfig"), *(Key+TEXT("_ShowFlags")), Wk, GEditorIni );
			viewport->ShowFlags = Wk | SHOW_ModeWidgets;

			GConfig->GetInt( TEXT("ViewportConfig"), *(Key+TEXT("_ViewMode")), Wk, GEditorIni );
			viewport->ViewMode = (ESceneViewMode)Wk;
		}
	}

	GApp->EditorFrame->ViewportConfigData->Load( this );
	GApp->EditorFrame->ViewportConfigData->Apply( GApp->EditorFrame->ViewportContainer );

	return 1;
}

/*-----------------------------------------------------------------------------
	WxLevelViewportWindow.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxLevelViewportWindow, wxWindow )
	EVT_SIZE(WxLevelViewportWindow::OnSize)
END_EVENT_TABLE()

WxLevelViewportWindow::WxLevelViewportWindow()
	: FEditorLevelViewportClient()
{
	ToolBar = NULL;
	Viewport = NULL;
}

WxLevelViewportWindow::~WxLevelViewportWindow()
{
	GEngine->Client->CloseViewport(Viewport);
	Viewport = NULL;
}

void WxLevelViewportWindow::SetUp( INT InViewportNum, ELevelViewportType InViewportType, ESceneViewMode InViewMode, DWORD InShowFlags )
{
	// ToolBar

	ToolBar = new WxLevelViewportToolBar( this, -1, this );

	// Viewport

	Viewport = GEngine->Client->CreateWindowChildViewport( (FViewportClient*)this, (HWND)GetHandle() );
	Viewport->CaptureJoystickInput(false);

	ViewportType = InViewportType;
	ViewMode = InViewMode;
	ShowFlags = InShowFlags;

    wxSizeEvent eventSize;
	OnSize( eventSize );

	ToolBar->UpdateUI();
}

void WxLevelViewportWindow::OnSize( wxSizeEvent& InEvent )
{
	if( ToolBar )
	{
		wxRect rc = GetClientRect();
		rc.y += WxLevelViewportToolBar::TOOLBAR_H;
		rc.height -= WxLevelViewportToolBar::TOOLBAR_H;

		ToolBar->SetSize( rc.GetWidth(), WxLevelViewportToolBar::TOOLBAR_H );
		::SetWindowPos( (HWND)Viewport->GetWindow(), HWND_TOP, rc.GetLeft()+1, rc.GetTop()+1, rc.GetWidth()-2, rc.GetHeight()-2, SWP_SHOWWINDOW );
	}

}

/*-----------------------------------------------------------------------------
	EViewportHolder.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( EViewportHolder, wxPanel )
	EVT_SIZE(EViewportHolder::OnSize)
END_EVENT_TABLE()

EViewportHolder::EViewportHolder( wxWindow* InParent, wxWindowID InID, bool InWantScrollBar )
	: wxPanel( InParent, InID )
{
	Viewport = NULL;
	ScrollBar = NULL;
	SBPos = SBRange = 0;

	if( InWantScrollBar )
		ScrollBar = new wxScrollBar( this, ID_BROWSER_SCROLL_BAR, wxDefaultPosition, wxDefaultSize, wxSB_VERTICAL );
}

void EViewportHolder::SetViewport( FChildViewport* InViewport )
{
	Viewport = InViewport;
}

void EViewportHolder::OnSize( wxSizeEvent& InEvent )
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

}

// Updates the scrollbar so it looks right and is in the right position

void EViewportHolder::UpdateScrollBar( INT InPos, INT InRange )
{
	if( ScrollBar )
		ScrollBar->SetScrollbar( InPos, Viewport->GetSizeY(), InRange, Viewport->GetSizeY() );
}

INT EViewportHolder::GetScrollThumbPos()
{
	return ( ScrollBar ? ScrollBar->GetThumbPosition() : 0 );
}

void EViewportHolder::SetScrollThumbPos( INT InPos )
{
	if( ScrollBar )
		ScrollBar->SetThumbPosition( InPos );
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
