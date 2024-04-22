/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "RemoteControlFrame.h"
#include "RemoteControlPageFactory.h"
#include "RemoteControlPage.h"
#include "RemoteControlGame.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FRemoteControlPageFactory registration.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Convenience type definition for the RemoteControl page factory registry.
 */
typedef TArray<FRemoteControlPageFactory*> FactoryRegistryType;

/**
 * Retreives the RemoteControl factory registry singleton instance.
 */
static FactoryRegistryType &GetFactoryRegistry()
{
	static FactoryRegistryType SRegistry;
	return SRegistry;
}


/**
 * Register a RemoteControl page factory.
 */
void WxRemoteControlFrame::RegisterPageFactory(FRemoteControlPageFactory *InFactory)
{
	if ( InFactory )
	{
		FactoryRegistryType& FactoryRegistry = GetFactoryRegistry();
		FactoryRegistry.AddUniqueItem( InFactory );
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxRemoteControlFrame
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(WxRemoteControlFrame, wxFrame)
	EVT_CLOSE(WxRemoteControlFrame::OnClose)
	EVT_MENU(ID_REMOTECONTROL_SWITCH_TO_GAME, WxRemoteControlFrame::OnSwitchToGame)
	EVT_MENU(ID_REMOTECONTROL_REFRESH_PAGE, WxRemoteControlFrame::OnPageRefresh)
	EVT_ACTIVATE(WxRemoteControlFrame::OnActivate)
	EVT_NOTEBOOK_PAGE_CHANGED(ID_REMOTECONTROL_NOTEBOOK, WxRemoteControlFrame::OnPageChanged)
END_EVENT_TABLE()

WxRemoteControlFrame::WxRemoteControlFrame(FRemoteControlGame *InGame
									 , wxWindow *InParent
									 , wxWindowID InID)
	:	wxFrame(InParent
				, InID
				, TEXT("RemoteControl")
				, InGame->GetRemoteControlPosition()
				, wxDefaultSize
				, wxDEFAULT_FRAME_STYLE|wxCLIP_CHILDREN)
				, Game(InGame)
{
	// Allocate a panel and sizer.
	MainPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL|wxNO_BORDER|wxCLIP_CHILDREN);
	MainSizer = new wxBoxSizer(wxVERTICAL);

	// Create a notebook.
	Notebook = new wxNotebook(MainPanel , ID_REMOTECONTROL_NOTEBOOK, wxDefaultPosition, wxDefaultSize, wxCLIP_CHILDREN);
	MainSizer->Add(Notebook, 1, wxALL|wxEXPAND);

	// Add all pages registered in the page factory.
	FactoryRegistryType &FactoryRegistry = GetFactoryRegistry();

	bool bSelectedPage = true;
	for(INT PageFactoryIndex = 0; PageFactoryIndex < FactoryRegistry.Num(); ++PageFactoryIndex)
	{
		FRemoteControlPageFactory* CurPageFactory = FactoryRegistry( PageFactoryIndex );
		WxRemoteControlPage *NewPage = CurPageFactory->CreatePage( Game, Notebook );
		if ( NewPage )
		{
			const TCHAR* PageTitle = NewPage->GetPageTitle();
			Notebook->AddPage( NewPage, PageTitle, bSelectedPage );
			// All other pages are deselected.
			bSelectedPage = false;
		}
	}

	// Register the sizer.
	MainPanel->SetSizer(MainSizer);

	// Set up accelerator table.
	wxAcceleratorEntry Entries[2];
	Entries[0].Set(wxACCEL_NORMAL, WXK_F4, ID_REMOTECONTROL_SWITCH_TO_GAME);
	Entries[1].Set(wxACCEL_NORMAL, WXK_F5, ID_REMOTECONTROL_REFRESH_PAGE);
	const wxAcceleratorTable Accel(sizeof(Entries)/sizeof(wxAcceleratorEntry), Entries);
	SetAcceleratorTable(Accel);

	// Update UI for the active page.
	RefreshSelectedPage();

	// Finalize the window layout.
	MainSizer->Fit( this );
	MainSizer->SetSizeHints( this );

	// Initialize the RC's size and position from ini.
	if(GConfig)
	{
		INT PosX = 0;
		INT PosY = 0;
		INT Width = 0;
		INT Height = 0;

		GConfig->GetInt(TEXT("DebugWindows"), TEXT("RemoteControlX"), PosX, GGameIni);
		GConfig->GetInt(TEXT("DebugWindows"), TEXT("RemoteControlY"), PosY, GGameIni);
		GConfig->GetInt(TEXT("DebugWindows"), TEXT("RemoteControlWidth"), Width, GGameIni);
		GConfig->GetInt(TEXT("DebugWindows"), TEXT("RemoteControlHeight"), Height, GGameIni);

		SetSize(PosX, PosY, Width, Height);
	}
}

WxRemoteControlFrame::~WxRemoteControlFrame()
{
	SaveToINI();
}

/**
 * Saves the window position and size to the game .ini
 */
void WxRemoteControlFrame::SaveToINI()
{
	if(GConfig)
	{
		INT PosX = 0;
		INT PosY = 0;

		GetPosition(&PosX, &PosY);

		INT Width = 0;
		INT Height = 0;

		GetSize(&Width, &Height);

		GConfig->SetInt(TEXT("DebugWindows"), TEXT("RemoteControlX"), PosX, GGameIni);
		GConfig->SetInt(TEXT("DebugWindows"), TEXT("RemoteControlY"), PosY, GGameIni);
		GConfig->SetInt(TEXT("DebugWindows"), TEXT("RemoteControlWidth"), Width, GGameIni);
		GConfig->SetInt(TEXT("DebugWindows"), TEXT("RemoteControlHeight"), Height, GGameIni);
	}
}

/**
 * Override of show so we notify FRemoteControlGame of visibility state.
 */
bool WxRemoteControlFrame::Show(bool bShow)
{
	if( bShow )
	{
		if( !IsShown() )
		{
			Game->OnRemoteControlShow();
		}
	}
	else
	{
		SaveToINI();

		if( IsShown() )
		{
			Game->OnRemoteControlHide();
		}
	}
	return wxFrame::Show( bShow );
}

/**
 * Repositions RemoteControl so its locked to upper right of game window.
 */
void WxRemoteControlFrame::Reposition()
{
	const wxPoint Point( Game->GetRemoteControlPosition() );
	if( Point != wxDefaultPosition )
	{
		Move( Point );
	}
}

/**
 * Helper function to refresh the selected page.
 */
void WxRemoteControlFrame::RefreshSelectedPage(UBOOL bForce)
{
	const int SelectedPageIndex = Notebook->GetSelection();

	if( SelectedPageIndex != -1 )
	{
		WxRemoteControlPage *SelectedPage = static_cast<WxRemoteControlPage *>(Notebook->GetPage((size_t)SelectedPageIndex));
		check( SelectedPage );
		SelectedPage->RefreshPage(bForce);
	}
}

/**
 * Close event -- prevents window destruction; instead, hides the RemoteControl window.
 */
void WxRemoteControlFrame::OnClose(wxCloseEvent &In)
{
	Show(FALSE);
	Game->SetFocusToGame();
}

/**
 * Selected when user decides to refresh a page.
 */
void WxRemoteControlFrame::OnSwitchToGame(wxCommandEvent &In)
{
	Game->SetFocusToGame();
}

/**
 * Selected when user decides to refresh the active page.
 */
void WxRemoteControlFrame::OnPageRefresh(wxCommandEvent &In)
{
	RefreshSelectedPage(TRUE);
}

/**
 * Selected when the window is activated; refreshes the current page and updates property windows.
 */
void WxRemoteControlFrame::OnActivate(wxActivateEvent &In)
{
	if( In.GetActive() )
	{
		// Redraw the acitve page.
		RefreshSelectedPage();

		// Kill off any invalid property windows.
		Game->RefreshActorPropertyWindowList();
	}
}

/**
 * Called when the active page changes.
 */
void WxRemoteControlFrame::OnPageChanged(wxNotebookEvent &In)
{
	RefreshSelectedPage();
}
