/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "LevelBrowser.h"
#include "EngineSequenceClasses.h"
#include "LevelUtils.h"
#include "SceneManager.h"
#include "FileHelpers.h"
#include "BusyCursor.h"
#include "ScopedTransaction.h"
#include "Kismet.h"
#include "PropertyWindow.h"
#include "DlgGenericComboEntry.h"
#include "DlgCheckBoxList.h"
#include "EditorLevelUtils.h"

#if WITH_MANAGED_CODE
	// CLR includes
	#include "ContentBrowserShared.h"
#endif

/** Implements FString sorting for LevelBrowser.cpp */
IMPLEMENT_COMPARE_CONSTREF( FString, LevelBrowser, { return appStricmp(*A,*B); } );



namespace LevelBrowser 
{

	/**
	 * Deselects all BSP surfaces in the specified level.
	 *
	 * @param	Level		The level for which to deslect all levels.
	 */
	void LevelBrowser_DeselectAllSurfacesForLevel(ULevel* Level)
	{
		UModel* Model = Level->Model;
		for( INT SurfaceIndex = 0 ; SurfaceIndex < Model->Surfs.Num() ; ++SurfaceIndex )
		{
			FBspSurf& Surf = Model->Surfs(SurfaceIndex);
			if( Surf.PolyFlags & PF_Selected )
			{
				Model->ModifySurf( SurfaceIndex, FALSE );
				Surf.PolyFlags &= ~PF_Selected;
			}
		}
	}


	/**
	 * Encapsulates calls to setting level visibility so that selected actors
	 * in levels that are being hidden can be deselected.
	 */
	void SetLevelVisibility(ULevel* Level, UBOOL bVisible)
	{
		ULevelStreaming* LevelStreaming = NULL;
		if ( GWorld->PersistentLevel != Level )
		{
			LevelStreaming = FLevelUtils::FindStreamingLevel( Level );
		}
		//this call hides all owned actors, etc
		FLevelUtils::SetLevelVisibility(LevelStreaming, Level, bVisible, TRUE );

		// If the level is being hidden, deselect actors and surfaces that belong to this level.
		if ( !bVisible )
		{
			USelection* SelectedActors = GEditor->GetSelectedActors();
			SelectedActors->Modify();
			TTransArray<AActor*>& Actors = Level->Actors;
			for ( INT ActorIndex = 0 ; ActorIndex < Actors.Num() ; ++ActorIndex )
			{
				AActor* Actor = Actors( ActorIndex );
				if ( Actor )
				{
					SelectedActors->Deselect( Actor );
				}
			}
			LevelBrowser_DeselectAllSurfacesForLevel( Level );

			// Tell the editor selection status was changed.
			GEditor->NoteSelectionChange();
		}
		else
		{
			// Update out-of-date proc buildings that might have just been made visible
			GUnrealEd->CleanupOldBuildings(FALSE, TRUE);
		}
	}
}




///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxLevelBrowserMenuBar
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class WxLevelBrowserMenuBar : public wxMenuBar
{
public:
	WxLevelBrowserMenuBar()
	{
		// Level menu.
		wxMenu* LevelMenu = new wxMenu();
		LevelMenu->Append( IDM_LB_NewLevel, *LocalizeUnrealEd("NewLevel"), TEXT("") );
		LevelMenu->Append( IDM_LB_NewLevelFromSelectedActors, *LocalizeUnrealEd("NewLevelFromSelectedActors"), TEXT("") );
		LevelMenu->Append( IDM_LB_ImportLevel, *LocalizeUnrealEd("ImportLevelE"), TEXT("") );
		LevelMenu->AppendSeparator();
		LevelMenu->Append( IDM_LB_MakeCurrentLevel, *LocalizeUnrealEd("MakeCurrent"), TEXT("") );

		// For options only presented when actors are selected.
		UBOOL bActorSelected = FALSE;
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			bActorSelected = TRUE;
			break;
		}

		LevelMenu->AppendSeparator();
		LevelMenu->Append( IDM_LB_UpdateLevelsForAllActors, *LocalizeUnrealEd( TEXT( "LevelBrowserMenu_UpdateLevelsForAllActors" ) ), *LocalizeUnrealEd( TEXT( "LevelBrowserMenu_UpdateLevelsForAllActors_Help" ) ) );
		LevelMenu->AppendCheckItem( IDM_LB_AutoUpdateLevelsForChangedActors, *LocalizeUnrealEd( TEXT( "LevelBrowserMenu_AutoUpdateLevelsForChangedActors" ) ), *LocalizeUnrealEd( TEXT( "LevelBrowserMenu_AutoUpdateLevelsForChangedActors_Help" ) ) );
		LevelMenu->AppendSeparator();
		LevelMenu->Append( IDM_LB_MergeVisibleLevels, *LocalizeUnrealEd("MergeVisibleLevels"), TEXT("") );
		LevelMenu->Append( IDM_LB_RemoveLevelFromWorld, *LocalizeUnrealEd("RemoveLevelFromWorld"), TEXT("") );

		// View menu.
		wxMenu* ViewMenu = new wxMenu();
		ViewMenu->Append( IDM_RefreshBrowser, *LocalizeUnrealEd("RefreshWithHotkey"), TEXT("") );
#if WITH_MANAGED_CODE
		ViewMenu->Append( IDM_LB_ShowSelectedLevelsInContentBrowser, *LocalizeUnrealEd("ShowSelectedLevelsInContentBrowser"), TEXT("") );
#endif
		ViewMenu->Append( ID_UI_LEVER_BROWSER_TOGGLE_FILTER_WINDOW, *LocalizeUnrealEd("LevelBrowser_KeywordFilter_ToggleFilterWindowVisibility"));
		ViewMenu->AppendSeparator();
		ViewMenu->AppendCheckItem( IMD_LB_ShowSizeData, *LocalizeUnrealEd("LevelBrowser_Menu_LevelMemorySizeData") );

		Append( LevelMenu, *LocalizeUnrealEd("Level") );
		Append( ViewMenu, *LocalizeUnrealEd("View") );

		WxBrowser::AddDockingMenu( this );
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxScrollablePane
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class WxScrollablePane : public wxScrolledWindow
{
public:
	WxScrollablePane(wxWindow* InParent, WxLevelBrowser* InLevelBrowser)
		:	wxScrolledWindow( InParent, -1, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxVSCROLL )
		,	LevelBrowser( InLevelBrowser )
	{
		SetScrollbars(1,1,0,0);
		// Set the x and y scrolling increments.  This is necessary for the scrolling to work!
		SetScrollRate( ScrollRate, ScrollRate );
	}

	/**
	 * Derived function that allows custom visibility (for levels or panes, etc)
	 */
	virtual UBOOL IsChildWindowVisible(const INT InWindowIndex) const { return TRUE; }


protected:
	/**
	 * Does the layout for the pane's child windows.
	 */
	template<class ChildWindowType>
	void LayoutWindows(TArray<ChildWindowType*>& ChildWindows)
	{
		Freeze();


		INT NumVisibleChildWindows = 0;
		for ( INT ChildWindowIndex = 0 ; ChildWindowIndex < ChildWindows.Num() ; ++ChildWindowIndex )
		{
			UBOOL bShouldBeVisible = IsChildWindowVisible(ChildWindowIndex);

			ChildWindowType* CurChildWindow = ChildWindows( ChildWindowIndex );
			check (CurChildWindow);

			if (bShouldBeVisible)
			{
				CurChildWindow->Show();
				++NumVisibleChildWindows;
			}
			else
			{
				//filtered out
				CurChildWindow->Hide();
			}
		}

		Layout();

		Thaw();
	}

	/** Scroll rate when using the scroll bar or mouse wheel. */
	static const INT ScrollRate = 15;
	
	/** Handle to the level browser that owns this level pane. */
	WxLevelBrowser*			LevelBrowser;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxLevelPane
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/**
 * The panel on the left side of the level browser that contains a scrollable
 * list of levels in the world.
 */
class WxLevelPane : public WxScrollablePane
{
public:
	class WxLevelWindow : public wxPanel
	{
	public:
		WxLevelWindow(wxWindow* InParent, WxLevelBrowser* InLevelBrowser, ULevel* InLevel, ALevelGridVolume* InLevelGridVolume, const TCHAR* InDescription);

		ULevel* GetLevel()				{ return Level; }
		const ULevel* GetLevel() const	{ return Level; }

		ALevelGridVolume* GetLevelGridVolume()				{ return LevelGridVolume; }
		const ALevelGridVolume* GetLevelGridVolume() const	{ return LevelGridVolume; }

		/**
		 * Returns a level browser item for this level window
		 *
		 * @return	An item that represents either a level or level grid volume
		 */
		FLevelBrowserItem GetLevelItem()
		{
			if( Level != NULL )
			{
				return FLevelBrowserItem( Level );
			}
			else
			{
				return FLevelBrowserItem( LevelGridVolume );
			}
		}

		/**
		 * Updates UI elements from the level managed by this level window.  Refreshes.
		 */
		void UpdateUI()
		{
			if (LockCheckButton)
			{
				const UBOOL bLevelLocked = Level != NULL && FLevelUtils::IsLevelLocked( Level );
				INT TestState = bLevelLocked ? WxBitmapCheckButton::STATE_On : WxBitmapCheckButton::STATE_Off;
				LockCheckButton->SetCurrentState(TestState);
			}
		}

		/**
		 * Returns true if this level is currently visible.
		 */
		UBOOL IsLevelVisible() const
		{
			return FLevelUtils::IsLevelVisible( Level );
		}

		/**
		 * Sets this level's visibility to the specified value.  Refreshes indirectly.
		 */
		void SetLevelVisibility(UBOOL bVisible)
		{
			check( Level != NULL );
			const UBOOL bLevelCurrentlyVisible = FLevelUtils::IsLevelVisible( Level );
			if ( bLevelCurrentlyVisible != bVisible )
			{
				LevelBrowser::SetLevelVisibility( Level, bVisible );
				UpdateUI();
			}
		}

		/**
		 * Attempt to lock/unlock the level of this window
		 *
		 * @param	bLocked	If TRUE, attempt to lock the level; If FALSE, attempt to unlock the level
		 */
		void SetLevelLocked(UBOOL bLocked)
		{
			check( Level != NULL );

			// Do nothing if attempting to set the level to the same locked state or if trying to lock/unlock the p-level
			if ( bLocked == FLevelUtils::IsLevelLocked( Level ) || Level == GWorld->PersistentLevel )
			{
				return;
			}

			// If locking the level, deselect all of its actors and BSP surfaces
			if ( bLocked )
			{
				USelection* SelectedActors = GEditor->GetSelectedActors();
				SelectedActors->Modify();

				// Deselect all level actors 
				for ( TArray<AActor*>::TConstIterator LevelActorIterator( Level->Actors ); LevelActorIterator; ++LevelActorIterator )
				{
					AActor* CurActor = *LevelActorIterator;
					if ( CurActor )
					{
						SelectedActors->Deselect( CurActor );
					}
				}

				// Deselect all level BSP surfaces
				LevelBrowser::LevelBrowser_DeselectAllSurfacesForLevel( Level );

				// Tell the editor selection status was changed.
				GEditor->NoteSelectionChange();

				// If locking the current level, reset the p-level as the current level
				if ( Level == GWorld->CurrentLevel )
				{
					LevelBrowser->MakeLevelCurrent( GWorld->PersistentLevel );
					LevelBrowser->RequestUpdate();
				}
			}

			// Change the level's locked status
			FLevelUtils::ToggleLevelLock( Level );
			UpdateUI();
		}

	private:

		/** The level associated with this window, if any */
		ULevel*				Level;

		/** The level grid volume associated with this window, if any */
		ALevelGridVolume*	LevelGridVolume;

		WxLevelBrowser*		LevelBrowser;
		FString				Description;

		WxBitmapCheckButton*	VisibleCheckButton;
		WxBitmap				VisibleImage;
		WxBitmap				NotVisibleImage;

		WxBitmapCheckButton*	LockCheckButton;
		WxBitmap				LockImage;
		WxBitmap				UnlockImage;

		wxStaticText*			TitleText;

		WxBitmapButton*			KismetButton;
		wxButton*				PickColorButton;

		WxBitmapButton*			SaveDirtyLevelButton;
		WxBitmapButton*			SaveLevelButton;

		///////////////////
		// Wx events.
		/**
		 * Inits the "Eye" visibility check button
		 */ 
		void InitVisibleCheckButton(void);
		/**
		 * Inits the "Lock/Unlock" check button
		 */ 
		void InitLockButton(void);

		void OnPaint(wxPaintEvent& In);
		void OnLeftButtonDown(wxMouseEvent& In);
		void OnRightButtonDown(wxMouseEvent& In);
		/** Event when visibility button has been toggled*/
		void OnVisibleCheckChanged(wxCommandEvent& In);
		/** Event when the window is testing UI Updates */
		void UI_IsLevelVisible(wxUpdateUIEvent& In);
		/** Event when level lock button has been toggled*/
		void OnLevelLockChanged(wxCommandEvent& In);
		void OnLeftDoubleClick(wxMouseEvent& In);
		void OnSaveLevel(wxCommandEvent& In);
		void OnOpenKismet(wxCommandEvent& In);
		void OnPickColor(wxCommandEvent& In);
		// Declare an empty erase background callback to prevent flicker.
		void OnEraseBackground(wxEraseEvent& event) {};

		DECLARE_EVENT_TABLE();
	};

	WxLevelPane(wxWindow* InParent, WxLevelBrowser* InLevelBrowser)
		:	WxScrollablePane( InParent, InLevelBrowser )
	{}

	/**
	 * Destroys child windows and empties out the list of managed levels.
	 */
	void Clear()
	{
		DestroyChildren();
		LevelWindows.Empty();
	}

	void LayoutLevelWindows()
	{
		LayoutWindows( LevelWindows );
	}

	/**
	 * Adds the specified level to the level list.  Does nothing if the specified level already exists.
	 *
	 * @param	InLevel			The level to add.
	 * @param	Description		A level description, displayed at the top of the level window.
	 */
	void AddLevel( ULevel* InLevel )
	{
		FString LevelDescription = InLevel->GetOutermost()->GetName();
		if( InLevel == GWorld->PersistentLevel )
		{
			LevelDescription = *LocalizeUnrealEd( "PersistentLevel" );
		}

		// Do nothing if the level already exists.
		for ( INT LevelWindowIndex = 0 ; LevelWindowIndex < LevelWindows.Num() ; ++LevelWindowIndex )
		{
			WxLevelWindow* CurLevelWindow = LevelWindows( LevelWindowIndex );
			if ( CurLevelWindow->GetLevel() == InLevel )
			{
				return;
			}
		}
		// Add the level to the list of levels appearing in the level pane.
		WxLevelWindow* NewWindow = new WxLevelWindow( this, LevelBrowser, InLevel, NULL, *LevelDescription );
		GetSizer()->Add(NewWindow, 0, wxGROW | wxALIGN_TOP | wxALIGN_LEFT | wxBOTTOM, 2);
		LevelWindows.AddItem( NewWindow );
	}


	/**
	 * Adds the specified level grid volume the list
	 *
	 * @param	LevelGridVolume		The level grid volume to add
	 */
	void AddLevelGridVolume( ALevelGridVolume* LevelGridVolume )
	{
		// Do nothing if the level grid volume already exists.
		for ( INT LevelWindowIndex = 0 ; LevelWindowIndex < LevelWindows.Num() ; ++LevelWindowIndex )
		{
			WxLevelWindow* CurLevelWindow = LevelWindows( LevelWindowIndex );
			if ( CurLevelWindow->GetLevelGridVolume() == LevelGridVolume )
			{
				return;
			}
		}

		// Add the level grid volume to the list appearing in the level pane.
		WxLevelWindow* NewWindow = new WxLevelWindow( this, LevelBrowser, NULL, LevelGridVolume, *LevelGridVolume->GetLevelGridVolumeName() );
		GetSizer()->Add(NewWindow, 0, wxGROW | wxALIGN_TOP | wxALIGN_LEFT | wxBOTTOM, 2);
		LevelWindows.AddItem( NewWindow );
	}



	/**
	 * Sync's the specified level's level window.  Refreshes indirectly.
	 */
	void UpdateUIForLevel( const ULevel* InLevel )
	{
		if ( InLevel )
		{
			for( INT LevelWindowIndex = 0 ; LevelWindowIndex < LevelWindows.Num() ; ++LevelWindowIndex )
			{
				WxLevelWindow* CurLevelWindow = LevelWindows( LevelWindowIndex );
				if ( InLevel == CurLevelWindow->GetLevel() )
				{
					CurLevelWindow->UpdateUI();
					break;
				}
			}
		}
		else
		{
			for( INT LevelWindowIndex = 0 ; LevelWindowIndex < LevelWindows.Num() ; ++LevelWindowIndex )
			{
				WxLevelWindow* CurLevelWindow = LevelWindows( LevelWindowIndex );
				CurLevelWindow->UpdateUI();
			}
		}
	}

	/**
	 * Sets the specified level's visibility.
	 */
	void SetLevelVisibility(const ULevel* InLevel, UBOOL bVisible)
	{
		for ( INT LevelWindowIndex = 0 ; LevelWindowIndex < LevelWindows.Num() ; ++LevelWindowIndex )
		{
			WxLevelWindow* CurLevelWindow = LevelWindows( LevelWindowIndex );
			if ( CurLevelWindow->GetLevel() == InLevel )
			{
				CurLevelWindow->SetLevelVisibility( bVisible );
			}
		}
	}

	/**
	 * Sets the specified level's visibility.
	 */
	void SetAllLevelVisibility(UBOOL bVisible)
	{
		const FScopedBusyCursor BusyCursor;
		for ( INT LevelWindowIndex = 0 ; LevelWindowIndex < LevelWindows.Num() ; ++LevelWindowIndex )
		{
			WxLevelWindow* CurLevelWindow = LevelWindows( LevelWindowIndex );
			if( CurLevelWindow->GetLevelItem().IsLevel() )
			{
				CurLevelWindow->SetLevelVisibility( bVisible );
			}
		}
	}

	/**
	 * Attempt to lock/unlock the specified level
	 *
	 * @param	InLevel	Level which should be locked/unlocked
	 * @param	bLocked	If TRUE, lock the level; If FALSE, unlock the level
	 */
	void SetLevelLocked(ULevel* InLevel, UBOOL bLocked)
	{
		check( InLevel );

		for ( TArray<WxLevelWindow*>::TIterator LevelWindowIter( LevelWindows ); LevelWindowIter; ++LevelWindowIter )
		{
			WxLevelWindow* CurLevelWindow = *LevelWindowIter;
			
			// Find the level in the list of level windows
			if ( CurLevelWindow->GetLevel() == InLevel )
			{
				// Lock/unlock the level as requested
				CurLevelWindow->SetLevelLocked( bLocked );
				break;
			}
		}
	}

	/**
	 * Attempt to lock/unlock all levels
	 *
	 * @param	bLocked	If TRUE, lock all levels; if FALSE, unlock all levels
	 */
	void SetAllLevelLocked(UBOOL bLocked)
	{
		// Lock/unlock each level as requested
		for ( TArray<WxLevelWindow*>::TIterator LevelWindowIterator( LevelWindows ); LevelWindowIterator ; ++LevelWindowIterator )
		{
			WxLevelWindow* CurLevelWindow = *LevelWindowIterator;
			if( CurLevelWindow->GetLevelItem().IsLevel() )
			{
				CurLevelWindow->SetLevelLocked( bLocked );
			}
		}
	}

	////////////////////////////////
	// Level window iterator

	typedef TArray<WxLevelWindow*>::TIterator TLevelWindowIterator;

	TLevelWindowIterator LevelWindowIterator()
	{
		return TLevelWindowIterator( LevelWindows );
	}

	/**
	 * Derived function that allows custom visibility (for levels or panes, etc)
	 */
	virtual UBOOL IsChildWindowVisible(const INT InWindowIndex) const
	{
		check(IsWithin<INT>(InWindowIndex, 0, LevelWindows.Num()));

		ULevel* TestLevel = LevelWindows(InWindowIndex)->GetLevel();
		return IsChildWindowVisible( TestLevel );
	}
	virtual UBOOL IsChildWindowVisible(ULevel* InLevel) const
	{
		if (!InLevel)
		{
			return TRUE;
		}

		ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel(InLevel);
		return IsChildWindowVisible( StreamingLevel );
	}
	virtual UBOOL IsChildWindowVisible(ULevelStreaming* InLevelStreaming) const
	{
		if (!InLevelStreaming)
		{
			return TRUE;
		}

		//no keywords
		if (FilterStrings.Num() == 0)
		{
			return TRUE;
		}
		for (INT i = 0; i < FilterStrings.Num(); ++i)
		{
			const FString& TestString = FilterStrings(i);
			//has a checked key word filter
			if (InLevelStreaming->Keywords.ContainsItem(TestString))
			{
				return TRUE;
			}
			//the user requested "none" to be included and this doesn't have any keywords
			if ((TestString == TEXT("None")) && (InLevelStreaming->Keywords.Num()==0))
			{
				return TRUE;
			}
		}

		return FALSE; 
	}


	/**
	 * Sets the keywords that level panes will be filtered by
	 */
	void SetFilterStrings(TArray<FString>& InFilterStrings) { FilterStrings = InFilterStrings; }

private:

	/** A list of levels appearing in this pane. */
	TArray<WxLevelWindow*>	LevelWindows;

	/** List of strings by which to filter the level windows.  Empty means allow all windows */
	TArray<FString> FilterStrings;

	void OnSize(wxSizeEvent& In)
	{
		//LayoutLevelWindows();
		//Refresh();//////////////////////
	}

	/**
	 * Clicking off any levels deselects all levels.
	 */
	void OnLeftButtonDown(wxMouseEvent& In)
	{
		LevelBrowser->DeselectAllLevels();
	}

	void OnMouseWheel(wxMouseEvent& In)
	{
		In.Skip();
	}

	DECLARE_EVENT_TABLE();
};

BEGIN_EVENT_TABLE( WxLevelPane, wxPanel )
	EVT_SIZE( WxLevelPane::OnSize )
	EVT_LEFT_DOWN( WxLevelPane::OnLeftButtonDown )
	EVT_MOUSEWHEEL( WxLevelPane::OnMouseWheel )
END_EVENT_TABLE()


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Helper class for non flickery buttons
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class WxNonFlickerButton: public wxButton
{
public:
	WxNonFlickerButton(wxWindow* InParent, wxWindowID InID, const wxString& InText, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = 0)
		: wxButton (InParent, InID, InText, pos, size, style)
	{
	}

	// Declare an empty erase background callback to prevent flicker.
	void OnEraseBackground(wxEraseEvent& event) {};

	DECLARE_EVENT_TABLE();
};

BEGIN_EVENT_TABLE( WxNonFlickerButton, wxButton)
	EVT_ERASE_BACKGROUND( WxNonFlickerButton::OnEraseBackground )
END_EVENT_TABLE()

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxLevelPane::WxLevelWindow
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_EVENT_TABLE( WxLevelPane::WxLevelWindow, wxPanel)
	EVT_PAINT( WxLevelPane::WxLevelWindow::OnPaint )
	EVT_LEFT_DOWN( WxLevelPane::WxLevelWindow::OnLeftButtonDown )
	EVT_RIGHT_DOWN( WxLevelPane::WxLevelWindow::OnRightButtonDown )
	EVT_LEFT_DCLICK( WxLevelPane::WxLevelWindow::OnLeftDoubleClick )
	EVT_BUTTON( IDCK_LB_LevelVisible, WxLevelPane::WxLevelWindow::OnVisibleCheckChanged )
	EVT_UPDATE_UI( IDCK_LB_LevelVisible,  WxLevelPane::WxLevelWindow::UI_IsLevelVisible )
	EVT_BUTTON( IDCK_LB_LevelLock, WxLevelPane::WxLevelWindow::OnLevelLockChanged )
	EVT_BUTTON( IDM_LB_DirtyLevel, WxLevelPane::WxLevelWindow::OnSaveLevel )
	EVT_BUTTON( IDM_LB_SaveLevel, WxLevelPane::WxLevelWindow::OnSaveLevel )
	EVT_BUTTON( IDM_LB_OpenKismet, WxLevelPane::WxLevelWindow::OnOpenKismet )
	EVT_BUTTON( IDM_LB_PickColor, WxLevelPane::WxLevelWindow::OnPickColor )
	EVT_ERASE_BACKGROUND( WxLevelPane::WxLevelWindow::OnEraseBackground )
END_EVENT_TABLE()

#define TOOL_BUTTON_SIZE 18

WxLevelPane::WxLevelWindow::WxLevelWindow(wxWindow* InParent,
										  WxLevelBrowser* InLevelBrowser,
										  ULevel* InLevel,
										  ALevelGridVolume* InLevelGridVolume,
										  const TCHAR* InDescription)
										  :	wxPanel( InParent )
										  ,	Level( InLevel )
										  , LevelGridVolume( InLevelGridVolume )
										  ,	LevelBrowser( InLevelBrowser )
										  ,	Description( InDescription )
										  , VisibleCheckButton( NULL )
										  , LockCheckButton( NULL )
										  , TitleText( NULL )
										  , KismetButton( NULL )
										  , PickColorButton( NULL )
										  , SaveDirtyLevelButton (NULL)
										  , SaveLevelButton (NULL)
{

	TitleText = new WxPassThroughStaticText(this, IDCK_LB_TitleLabel, TEXT(""));

	wxBoxSizer* MainSizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* ToolSizer = new wxBoxSizer(wxHORIZONTAL);

	//add spacer distance for Outdated badge
	ToolSizer->Add(8, -1, 0);

	//Init complex buttons
	InitLockButton();
	InitVisibleCheckButton();

	//kismet button
	WxBitmap* KismetBitmap = new WxMaskedBitmap( TEXT("Kismet") );
	KismetButton = new WxBitmapButton( this, IDM_LB_OpenKismet, *KismetBitmap, wxDefaultPosition, wxSize(TOOL_BUTTON_SIZE, TOOL_BUTTON_SIZE));
	KismetButton->SetToolTip( *LocalizeUnrealEd("ToolTip_OpenKismetForThisLevel") );
	delete KismetBitmap;
	

	// Level Color button
	wxColour LevelColor( wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE) );
	if( Level != NULL )
	{
		ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel( Level );
		if ( LevelStreaming )
		{
			const FColor& LevelDrawColor = LevelStreaming->DrawColor;
			LevelColor = wxColour( LevelDrawColor.R, LevelDrawColor.G, LevelDrawColor.B );
		}
	}

	PickColorButton = new WxNonFlickerButton( this, IDM_LB_PickColor, wxEmptyString, wxDefaultPosition, wxSize(TOOL_BUTTON_SIZE*2, TOOL_BUTTON_SIZE));
	PickColorButton->SetToolTip( *LocalizeUnrealEd("ToolTip_PickColorForThisLevel") );
	PickColorButton->SetBackgroundColour(LevelColor);

	//Save button
	WxBitmap* DirtyDiskBitmap = new WxMaskedBitmap( TEXT("FileDirty.png") );
	SaveDirtyLevelButton = new WxBitmapButton( this, IDM_LB_DirtyLevel, *DirtyDiskBitmap, wxDefaultPosition, wxSize(TOOL_BUTTON_SIZE, TOOL_BUTTON_SIZE) );;
	SaveDirtyLevelButton->SetToolTip( *LocalizeUnrealEd("ToolTip_SaveLevel") );
	delete DirtyDiskBitmap;

	WxBitmap* NonDirtyDiskBitmap = new WxMaskedBitmap( TEXT("FileUpToDate.png") );
	SaveLevelButton = new WxBitmapButton( this, IDM_LB_SaveLevel, *NonDirtyDiskBitmap, wxDefaultPosition, wxSize(TOOL_BUTTON_SIZE, TOOL_BUTTON_SIZE) );
	SaveLevelButton->SetToolTip( *LocalizeUnrealEd("ToolTip_SaveLevel") );
	delete NonDirtyDiskBitmap;

	const UBOOL bWantSaveButtons = ( Level != NULL );
	const UBOOL bWantKismetButton = ( Level != NULL );
	const UBOOL bWantLockButton = ( Level != NULL && GWorld->PersistentLevel != Level );
	const UBOOL bWantVisibilityButton = ( Level != NULL );
	const UBOOL bWantColorButton = ( Level != NULL && GWorld->PersistentLevel != Level );

	if( bWantSaveButtons )
	{
		ToolSizer->Add(SaveDirtyLevelButton, 0, wxGROW | wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT | wxSHAPED, 0);// | wxSHAPED | wxGROW | wxALL, 0);
		ToolSizer->Add(SaveLevelButton, 0, wxGROW | wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT | wxSHAPED, 0);// | wxSHAPED | wxGROW | wxALL, 0);
	}
	else
	{
		ToolSizer->Add( SaveDirtyLevelButton->GetSize().GetWidth(), -1, 0 );
		SaveDirtyLevelButton->Hide();

		ToolSizer->Add( SaveLevelButton->GetSize().GetWidth(), -1, 0 );
		SaveLevelButton->Hide();
	}

	if( bWantKismetButton )
	{
		ToolSizer->Add(KismetButton, 0, wxGROW | wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT | wxSHAPED, 0);// | wxSHAPED | wxGROW | wxALL, 0);
	}
	else
	{
		ToolSizer->Add( KismetButton->GetSize().GetWidth(), -1, 0 );
		KismetButton->Hide();
	}

	if( bWantLockButton )
	{
		ToolSizer->Add(LockCheckButton, 0, wxGROW | wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT | wxSHAPED, 0);// | wxSHAPED | wxGROW | wxALL, 0);
	}
	else
	{
		ToolSizer->Add(LockCheckButton->GetSize().GetWidth(), -1, 0);
		LockCheckButton->Hide();
	}

	if( bWantVisibilityButton )
	{
		ToolSizer->Add(VisibleCheckButton, 0, wxGROW | wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT | wxSHAPED, 0);// | wxSHAPED | wxGROW | wxALL, 0);
	}
	else
	{
		ToolSizer->Add(VisibleCheckButton, 0, wxGROW | wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT | wxSHAPED, 0);// | wxSHAPED | wxGROW | wxALL, 0);
		VisibleCheckButton->Hide();
	}

	if( bWantColorButton )
	{
		ToolSizer->Add(PickColorButton, 0, wxGROW | wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT | wxSHAPED, 0);// | wxSHAPED | wxGROW | wxALL, 0);
	}
	else
	{
		ToolSizer->Add(PickColorButton->GetSize().GetWidth(), -1, 0);
		PickColorButton->Hide();
	}

	ToolSizer->Add(TitleText, 1, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT | wxLEFT | wxRIGHT, 3);

	MainSizer->Add(ToolSizer, 1, wxALL | wxGROW, 3);
	SetSizer(MainSizer);

	Layout();

	if( SaveDirtyLevelButton != NULL )
	{
		SaveDirtyLevelButton->Hide();
	}

	UpdateUI();
}

/** Creates bitmap button for visibility*/
void WxLevelPane::WxLevelWindow::InitVisibleCheckButton()
{
	FString VisibleIconFileName = TEXT("VisibilityEyeOn.png");
	FString NotVisibleIconFileName = TEXT("VisibilityEyeOff.png");
	VisibleImage.Load( VisibleIconFileName );
	NotVisibleImage.Load( NotVisibleIconFileName );

	VisibleCheckButton = new WxBitmapCheckButton(this, this, IDCK_LB_LevelVisible, &NotVisibleImage, &VisibleImage, wxDefaultPosition, wxSize(TOOL_BUTTON_SIZE, TOOL_BUTTON_SIZE));
	//VisibleCheckButton->SetSize(ImageSize);
	VisibleCheckButton->SetToolTip(*LocalizeUnrealEd("LevelBrowser_LevelVisible_Tooltip"));
}

/** Creates bitmap button for visiblity*/
void WxLevelPane::WxLevelWindow::InitLockButton()
{
	FString LockOnFileName = TEXT("LockOn.png");
	FString LockOffFileName = TEXT("LockOff.png");
	LockImage.Load ( LockOnFileName );
	UnlockImage.Load( LockOffFileName );

	/** Favorite Toggle Button*/
	LockCheckButton = new WxBitmapCheckButton(this, this, IDCK_LB_LevelLock, &UnlockImage, &LockImage, wxDefaultPosition, wxSize(TOOL_BUTTON_SIZE, TOOL_BUTTON_SIZE));
	//LockCheckButton->SetSize(ImageSize);
	LockCheckButton->SetToolTip(*LocalizeUnrealEd("LevelBrowser_LockLevel_Tooltip"));
}

void WxLevelPane::WxLevelWindow::OnPaint(wxPaintEvent& In)
{
	wxBufferedPaintDC dc(this);
	const wxRect rc( GetClientRect() );

	wxColour TextColor;
	wxColour BackgroundColor;
	if ( LevelBrowser->IsItemSelected( GetLevelItem() ) )
	{
		TextColor = *wxWHITE;
		BackgroundColor = wxColour(16, 32, 128);
	}
	else
	{
		TextColor = *wxBLACK;
		BackgroundColor = wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);
	}

	wxPen BackgroundPen = *wxTRANSPARENT_PEN;
	dc.SetPen( BackgroundPen );

	wxRect Rect = rc;
	dc.SetBrush( wxBrush( BackgroundColor, wxSOLID ) );
	dc.DrawRectangle( Rect.x,Rect.y, Rect.width,Rect.height);



	FLOAT LightmapSize = 0.0f;
	FLOAT ShadowmapSize = 0.0f;
	FLOAT FileSize = 0.0f;
	UBOOL bAnyLevelsUnsaved = FALSE;
	INT LevelActorCount = 0;

	static TArray< ULevel* > StaticVolumeLevels;
	StaticVolumeLevels.Reset();

	if( Level != NULL )
	{
		StaticVolumeLevels.AddItem( Level );
	}
	else if( LevelGridVolume != NULL )
	{
		// Gather levels associated with this level grid volume
		static TArray< ULevelStreaming* > StaticVolumeLevelStreamings;
		LevelGridVolume->GetLevelsForAllCells( StaticVolumeLevelStreamings );	// Out
		for( INT CurLevelIndex = 0; CurLevelIndex < StaticVolumeLevelStreamings.Num(); ++CurLevelIndex )
		{
			ULevelStreaming* CurLevelStreaming = StaticVolumeLevelStreamings( CurLevelIndex );
			if( ensure( CurLevelStreaming != NULL ) )
			{
				if( CurLevelStreaming->LoadedLevel != NULL )
				{
					StaticVolumeLevels.AddItem( CurLevelStreaming->LoadedLevel );
				}
			}
		}
	}


	for( INT CurLevelIndex = 0; CurLevelIndex < StaticVolumeLevels.Num(); ++CurLevelIndex )
	{
		ULevel* CurLevel = StaticVolumeLevels( CurLevelIndex );

		// Count actors
		// NOTE: We subtract two here to omit "default actors" in the count (default brush, and WorldInfo)
		LevelActorCount += CurLevel->Actors.Num()-2;

		// Count deleted and hidden actors
		INT NumDeletedActors = 0;
		for (INT ActorIdx = 0; ActorIdx < CurLevel->Actors.Num(); ++ActorIdx)
		{
			AActor* Actor = CurLevel->Actors(ActorIdx);
			if (!Actor)
			{
				++NumDeletedActors;
			}
		}
		// Subtract deleted and hidden actors from the actor count
		LevelActorCount -= NumDeletedActors;

		// Is this level's package dirty?
		UPackage* Package = CastChecked<UPackage>( CurLevel->GetOutermost() );
		if ( Package->IsDirty() )
		{
			bAnyLevelsUnsaved = TRUE;
		}

		// Update metrics
		static const FLOAT ByteConversion = 1.0f / 1024.0f;
		LightmapSize += CurLevel->LightmapTotalSize * ByteConversion;
		ShadowmapSize += CurLevel->ShadowmapTotalSize * ByteConversion;
		FileSize += CurLevel->GetOutermost()->GetFileSize() * ByteConversion * ByteConversion;
	}


	FString DescriptionString( FString::Printf( LocalizeSecure(LocalizeUnrealEd("LayerDesc_F"), *Description, LevelActorCount ) ) );

	// Append an asterisk if the level's package is unsaved.
	if( bAnyLevelsUnsaved )
	{
		DescriptionString += TEXT("*");

		//draw outdated badge
		dc.SetBrush( wxBrush( *wxRED, wxSOLID ) );
		dc.DrawRectangle( Rect.x,Rect.y, 8,Rect.height);
	}


	//turn on the proper button
	if( Level != NULL )
	{
		bool bWasDirtyLevelButtonShown = SaveDirtyLevelButton->IsShown();
		bool bShowDirtyLevelButton = bAnyLevelsUnsaved ? true : false;
		SaveDirtyLevelButton->Show(bShowDirtyLevelButton);
		SaveLevelButton->Show(!bShowDirtyLevelButton);
		if (bWasDirtyLevelButtonShown != bShowDirtyLevelButton)
		{
			//just cause the window to relayout as buttons have toggled and need to be repositioned
			Layout();
		}
	}

	wxColour LevelColor( wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE) );
	if( Level != NULL )
	{
		ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel( Level );
		if ( LevelStreaming )
		{
			const FColor& LevelDrawColor = LevelStreaming->DrawColor;
			LevelColor = wxColour( LevelDrawColor.R, LevelDrawColor.G, LevelDrawColor.B );
		}
	}

	if( PickColorButton != NULL )
	{
		PickColorButton->SetBackgroundColour(LevelColor);
	}


	// Append a string indicating the level is the current level.
	wxFont Font( dc.GetFont() );
	if ( ( Level != NULL && GWorld->CurrentLevel == Level ) ||
		 ( LevelGridVolume != NULL && GWorld->CurrentLevelGridVolume == LevelGridVolume ) )
	{
		// If a level grid volume is current, then don't emphasize a specific level being current
		if( Level == NULL || GWorld->CurrentLevelGridVolume == NULL )
		{
			Font.SetWeight( wxBOLD );
		}

		DescriptionString += TEXT("  ");
		if( GetLevelItem().IsLevel() )
		{
			DescriptionString += LocalizeUnrealEd( TEXT( "LevelBrowser_CurrentLevel" ) );
		}
		else
		{
			DescriptionString += LocalizeUnrealEd( TEXT( "LevelBrowser_CurrentLevelGridVolume" ) );
		}
	}
	
	// If size data is enabled, include it as part of the description string/label
	if ( LevelBrowser->IsSizeDataEnabled() )
	{
		DescriptionString += FString::Printf( TEXT("  %s: %.2f MB, %s: %.2f MB, %s: %.2f MB" ), 
			*LocalizeUnrealEd("LevelBrowser_LightMapsMemory"), LightmapSize, 
			*LocalizeUnrealEd("LevelBrowser_ShadowMapsMemory"), ShadowmapSize,
			*LocalizeUnrealEd("FileSize"), FileSize );
	}

	//update the title
	check(TitleText);
	TitleText->SetForegroundColour( TextColor );
	TitleText->SetBackgroundColour( BackgroundColor );
	TitleText->SetFont(Font);
	TitleText->SetLabel(*DescriptionString);

}

void WxLevelPane::WxLevelWindow::OnLeftButtonDown(wxMouseEvent& In)
{
	//set focus to this window so it can properly handle events.  For some reason this isn't happening automatically
	SetFocus();

	if ( !In.ControlDown() && !In.ShiftDown() )
	{
		// Clicking sets this level as the selected level.
		LevelBrowser->SelectSingleLevelItem( GetLevelItem() );
	}
	else if ( In.ControlDown() && !In.ShiftDown() )
	{
		// Ctrl clicking toggles this level's selection status.
		const UBOOL bIsSelected = LevelBrowser->IsItemSelected( GetLevelItem() );
		if ( bIsSelected )
		{
			LevelBrowser->DeselectLevelItem( GetLevelItem() );
		}
		else
		{
			LevelBrowser->SelectLevelItem( GetLevelItem() );
		}
	}
	else if ( !In.ControlDown() && In.ShiftDown() )
	{
		// Shift clicking selects the level in a manner similar to shift-clicking
		// in Windows Explorer
		LevelBrowser->ShiftSelectLevelItem( GetLevelItem() );
	}

	Refresh();
}

void WxLevelPane::WxLevelWindow::OnRightButtonDown(wxMouseEvent& In)
{
	class WxLevelPopupMenu : public wxMenu
	{
	public:
		WxLevelPopupMenu( FLevelBrowserItem InLevelItem )
		{
			INT NumItems = 0;

			if( InLevelItem.IsLevel() )
			{
				ULevel* Level = InLevelItem.GetLevel();

				// Don't offer the option to load/unload if the level is locked.
				if ( !FLevelUtils::IsLevelLocked( Level ) )
				{
					Append( IDM_LB_MakeCurrentLevel, *LocalizeUnrealEd("MakeCurrent"), TEXT("") );
					++NumItems;
				}

				if ( NumItems )
				{
					AppendSeparator();
					NumItems = 0;
				}
				

				// Don't offer the option to load/unload or edit properties if this is the persistent level.
				if ( Level != GWorld->PersistentLevel )
				{
					Append( IDM_LB_LevelProperties, *LocalizeUnrealEd("LevelBrowser_Properties"), TEXT("") );
					++NumItems;
				}

				if ( NumItems )
				{
					AppendSeparator();
					NumItems = 0;
				}
				

	#if WITH_MANAGED_CODE
				Append( IDM_LB_ShowSelectedLevelsInContentBrowser, *LocalizeUnrealEd("ShowSelectedLevelsInContentBrowser"), TEXT("") );
	#endif
				Append( IDM_LB_ShowSelectedLevelsInSceneManager, *LocalizeUnrealEd("ShowSelectedLevelsInSceneManager"), TEXT("") );
				Append( IDM_LB_SelectAllActors, *LocalizeUnrealEd("SelectAllActors"), TEXT("") );
				++NumItems;

				if ( NumItems )
				{
					AppendSeparator();
					NumItems = 0;
				}
				Append( IDM_LB_SaveSelectedLevels, *LocalizeUnrealEd("SaveSelectedLevels"), TEXT("") );
				++NumItems;

				// Operations based on selected levels.
				AppendSeparator();
				Append( IDM_LB_ShowSelectedLevels, *LocalizeUnrealEd("ShowSelectedLevels"), TEXT("") );
				Append( IDM_LB_HideSelectedLevels, *LocalizeUnrealEd("HideSelectedLevels"), TEXT("") );
				Append( IDM_LB_ShowOnlySelectedLevels, *LocalizeUnrealEd("ShowOnlySelectedLevels"), TEXT("") );
				Append( IDM_LB_ShowOnlyUnselectedLevels, *LocalizeUnrealEd("ShowOnlyUnselectedLevels"), TEXT("") );
				Append( IDM_LB_ShowAllLevels, *LocalizeUnrealEd("ShowAllLevels"), TEXT("") );
				Append( IDM_LB_HideAllLevels, *LocalizeUnrealEd("HideAllLevels"), TEXT("") );
				AppendSeparator();
				Append( IDM_LB_LockSelectedLevels, *LocalizeUnrealEd("LockSelectedLevels"), TEXT("") );
				Append( IDM_LB_UnlockSelectedLevels, *LocalizeUnrealEd("UnlockSelectedLevels"), TEXT("") );
				Append( IDM_LB_LockAllLevels, *LocalizeUnrealEd("LockAllLevels"), TEXT("") );
				Append( IDM_LB_UnlockAllLevels, *LocalizeUnrealEd("UnlockAllLevels"), TEXT("") );
				AppendSeparator();
				Append( IDM_LB_SelectAllLevels, *LocalizeUnrealEd("SelectAllLevels"), TEXT("") );
				Append( IDM_LB_DeselectAllLevels, *LocalizeUnrealEd("DeselectAllLevels"), TEXT("") );
				Append( IDM_LB_InvertLevelSelection, *LocalizeUnrealEd("InvertLevelSelection"), TEXT("") );

				// For options only presented when actors are selected.
				UBOOL bActorSelected = FALSE;
				UBOOL bStreamingLevelVolumeSelected = FALSE;
				for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
				{
					bActorSelected = TRUE;
					if ( Cast<ALevelStreamingVolume>( *It ) )
					{
						bStreamingLevelVolumeSelected = TRUE;
						break;
					}
				}

				AppendSeparator();
				// If any level streaming volumes are selected, present the option to associate the volumes with selected levels.
				if ( bStreamingLevelVolumeSelected )
				{
					Append( IDM_LB_AddSelectedLevelsToLevelStreamingVolumes, *LocalizeUnrealEd("AddStreamingVolumes"), TEXT("") );
					Append( IDM_LB_SetSelectedLevelsToLevelStreamingVolumes, *LocalizeUnrealEd("SetStreamingVolumes"), TEXT("") );
				}
				Append( IDM_LB_ClearLevelStreamingVolumeAssignments, *LocalizeUnrealEd("ClearStreamingVolumes"), TEXT("") );
				Append( IDM_LB_SelectAssociatedStreamingVolumes, *LocalizeUnrealEd("SelectAssociatedStreamingVolumes"), TEXT("") );

				AppendSeparator();

				Append( IDM_LB_AssignKeywords, *LocalizeUnrealEd("LevelBrowser_AssignKeywords"), TEXT("") );

				AppendSeparator();

				// See if this item is the selected item
				UBOOL bLevelItemIsCurrent = (GWorld->CurrentLevel == Level);
				if(bLevelItemIsCurrent)
				{
					Append( IDM_LB_MoveSelectedActorsToThisLevel, *LocalizeUnrealEd("LevelBrowser_MoveActorsToThisLevel"), TEXT("") );
				}
				else
				{
					Append( IDM_LB_MoveSelectedActorsToThisLevel, *LocalizeUnrealEd("LevelBrowser_MakeCurrentMoveActorsToThisLevel"), TEXT("") );
				}
			}
			else if( InLevelItem.IsLevelGridVolume() )
			{
				ALevelGridVolume* LevelGridVolume = InLevelItem.GetLevelGridVolume();

				Append( IDM_LB_MakeCurrentLevel, *LocalizeUnrealEd("MakeCurrent"), TEXT("") );

				AppendSeparator();

				Append( IDM_LB_LevelProperties, *LocalizeUnrealEd("LevelBrowser_Properties"), TEXT("") );

				AppendSeparator();

				Append( IDM_LB_SelectAllActors, *LocalizeUnrealEd("SelectAllActors"), TEXT("") );

				AppendSeparator();

				Append( IDM_LB_SelectAllLevels, *LocalizeUnrealEd("SelectAllLevels"), TEXT("") );
				Append( IDM_LB_DeselectAllLevels, *LocalizeUnrealEd("DeselectAllLevels"), TEXT("") );
				Append( IDM_LB_InvertLevelSelection, *LocalizeUnrealEd("InvertLevelSelection"), TEXT("") );

				AppendSeparator();

				Append( IDM_LB_UpdateLevelsForAllActors, *LocalizeUnrealEd( TEXT( "LevelBrowserMenu_UpdateLevelsForAllActors" ) ), *LocalizeUnrealEd( TEXT( "LevelBrowserMenu_UpdateLevelsForAllActors_Help" ) ) );

				AppendSeparator();

				// See if this item is the selected item
				UBOOL bLevelItemIsCurrent = (GWorld->CurrentLevelGridVolume == LevelGridVolume);
				if(bLevelItemIsCurrent)
				{
					Append( IDM_LB_MoveSelectedActorsToThisLevel, *LocalizeUnrealEd("LevelBrowser_MoveActorsToThisLevel"), TEXT("") );
				}
				else
				{
					Append( IDM_LB_MoveSelectedActorsToThisLevel, *LocalizeUnrealEd("LevelBrowser_MakeCurrentMoveActorsToThisLevel"), TEXT("") );
				}
			}
		}
	};


	//set focus to this window so it can properly handle events.  For some reason this isn't happening automatically
	SetFocus();

	const UBOOL bIsSelected = LevelBrowser->IsItemSelected( GetLevelItem() );
	if( !bIsSelected )
	{
		// Clicking sets this level as the selected level.
		LevelBrowser->SelectSingleLevelItem( GetLevelItem() );
		Refresh();
	}


	// Display the context menu
	{
		WxLevelPopupMenu Menu( GetLevelItem() );
		FTrackPopupMenu tpm( this, &Menu );
		tpm.Show();
	}
}

void WxLevelPane::WxLevelWindow::OnVisibleCheckChanged(wxCommandEvent& In)
{
	check( Level != NULL );
	const UBOOL bNewLevelVisibility = !FLevelUtils::IsLevelVisible( Level );
	SetLevelVisibility( bNewLevelVisibility );
	LevelBrowser->SelectSingleLevelItem( GetLevelItem() );
}


/** Event when the window is testing UI Updates */
void WxLevelPane::WxLevelWindow::UI_IsLevelVisible( wxUpdateUIEvent& In )
{
	UBOOL bIsVisible = IsLevelVisible();

	INT TestState = bIsVisible ? WxBitmapCheckButton::STATE_On : WxBitmapCheckButton::STATE_Off;
	if (TestState != VisibleCheckButton->GetCurrentState()->ID)	//to stop circular events from happening
	{
		//set button state
		VisibleCheckButton->SetCurrentState(TestState);
	}

	In.Check( bIsVisible ? true : false );
}

void WxLevelPane::WxLevelWindow::OnLevelLockChanged(wxCommandEvent& In)
{
	check( Level != NULL );

	// Toggle the locked state of the level
	SetLevelLocked( !FLevelUtils::IsLevelLocked( Level ) );

	// The level window was clicked on, so select this level.
	LevelBrowser->SelectSingleLevelItem( GetLevelItem() );
}

void WxLevelPane::WxLevelWindow::OnLeftDoubleClick(wxMouseEvent& In)
{
	// Double clicking on the level window makes the level associated with this window current.
	if( Level != NULL )
	{
		LevelBrowser->MakeLevelCurrent( Level );
	}
	else if( LevelGridVolume != NULL )
	{
		LevelBrowser->MakeLevelGridVolumeCurrent( LevelGridVolume );
	}
}

void WxLevelPane::WxLevelWindow::OnSaveLevel(wxCommandEvent& In)
{
	if ( !FLevelUtils::IsLevelVisible( Level ) )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("UnableToSaveInvisibleLevels") );
		return;
	}
	else if ( FLevelUtils::IsLevelLocked(Level) )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("UnableToSaveLockedLevels") );
		return;
	}

	LevelBrowser->SelectSingleLevelItem( GetLevelItem() );

	// Prompt the user to check out the level from source control before saving if it's currently under source control
	if ( Level && FEditorFileUtils::PromptToCheckoutLevels( FALSE, Level ) )
	{
		FEditorFileUtils::SaveLevel( Level );
	}
}

void WxLevelPane::WxLevelWindow::OnOpenKismet(wxCommandEvent& In)
{
	check( Level != NULL );

	LevelBrowser->SelectSingleLevelItem( GetLevelItem() );

	UBOOL bSequenceCreated = FALSE;

	// Create a sequence for the persistent level if it does not exist
	USequence* PLevelSequence = GWorld->PersistentLevel->GetGameSequence();
	if( !PLevelSequence )
	{
		PLevelSequence = ConstructObject<USequence>( USequence::StaticClass(), GWorld->PersistentLevel, TEXT("Main_Sequence"), RF_Transactional );
		GWorld->SetGameSequence( PLevelSequence, GWorld->PersistentLevel );
		PLevelSequence->MarkPackageDirty();
		bSequenceCreated = TRUE;
	}

	// Open this level's kismet sequence, creating one if it does not exist.
	USequence* Sequence = Level->GetGameSequence();
	if ( !Sequence )
	{
		// The level has no sequence -- create a new one.
		Sequence = ConstructObject<USequence>( USequence::StaticClass(), Level, TEXT("Main_Sequence"), RF_Transactional );
		GWorld->SetGameSequence( Sequence, Level );
		Sequence->MarkPackageDirty();
		bSequenceCreated = TRUE;

	}

	// If any sequence was created send callbacks.
	if( bSequenceCreated )
	{
		GCallbackEvent->Send( CALLBACK_LevelDirtied );
		GCallbackEvent->Send( CALLBACK_RefreshEditor_Kismet );
	}

	WxKismet::OpenSequenceInKismet( Sequence, GApp->EditorFrame );
}

void WxLevelPane::WxLevelWindow::OnPickColor(wxCommandEvent& In)
{
	check( Level != NULL );
	LevelBrowser->SelectSingleLevelItem( GetLevelItem() );
	if ( GWorld->PersistentLevel != Level )
	{
		// Initialize the color data for the picker window.
		ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel( Level );
		check( StreamingLevel );

		// Get topmost window for modal dialog to prevent clicking away
		wxWindow* TopWindow = this;
		while( TopWindow->GetParent() )
		{
			TopWindow = TopWindow->GetParent();
		}

		//save off because this window will get recreated during the color browsing
		WxLevelBrowser* SavedOffBrowser = LevelBrowser;

		FPickColorStruct PickColorStruct;
		PickColorStruct.RefreshWindows.AddItem(LevelBrowser);
		PickColorStruct.DWORDColorArray.AddItem(&(StreamingLevel->DrawColor));
		PickColorStruct.bModal = TRUE;

		if (PickColor(PickColorStruct) == ColorPickerConstants::ColorAccepted)
		{
			StreamingLevel->Modify();
			
			// Update the loaded level's components so the change in color will apply immediately
			StreamingLevel->LoadedLevel->UpdateComponents();
		
			SavedOffBrowser->UpdateLevelPropertyWindow();
			GCallbackEvent->Send( CALLBACK_LevelDirtied );
			SavedOffBrowser->RequestUpdate();
			GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxLevelBrowser
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The toolbar appearing at the top of the level browser.
 */
class WxLevelBrowserToolBar : public WxToolBar
{
public:
	WxLevelBrowserToolBar(wxWindow* InParent, wxWindowID InID)
		:	WxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_3DBUTTONS )
	{
		ReimportB.Load( TEXT("Reimport") );
		AddTool( IDM_LB_LevelProperties, TEXT(""), ReimportB, *LocalizeUnrealEd("Properties") );
		AddSeparator();
		Realize();
	}
private:
	WxMaskedBitmap ReimportB;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxLevelBrowser
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE( WxLevelBrowser, WxBrowser )
	EVT_SIZE( WxLevelBrowser::OnSize )
	EVT_MENU( IDM_RefreshBrowser, WxLevelBrowser::OnRefresh )

	EVT_MENU( IDM_LB_NewLevel, WxLevelBrowser::OnNewLevel )
	EVT_MENU( IDM_LB_NewLevelFromSelectedActors, WxLevelBrowser::OnNewLevelFromSelectedActors )
	EVT_MENU( IDM_LB_ImportLevel, WxLevelBrowser::OnImportLevel )
	EVT_MENU( IDM_LB_RemoveLevelFromWorld, WxLevelBrowser::OnRemoveLevelFromWorld )

	EVT_MENU( IDM_LB_MakeCurrentLevel, WxLevelBrowser::OnMakeLevelCurrent )
	EVT_MENU( IDM_LB_MergeVisibleLevels, WxLevelBrowser::OnMergeVisibleLevels )
	EVT_MENU( IDM_LB_SaveSelectedLevels, WxLevelBrowser::OnSaveSelectedLevels )
	EVT_MENU( IDM_LB_ShowSelectedLevelsInSceneManager, WxLevelBrowser::ShowSelectedLevelsInSceneManager )
	EVT_MENU( IDM_LB_ShowSelectedLevelsInContentBrowser, WxLevelBrowser::ShowSelectedLevelsInContentBrowser )
	EVT_MENU( IDM_LB_SelectAllActors, WxLevelBrowser::OnSelectAllActors )
	EVT_MENU( IDM_LB_LevelProperties, WxLevelBrowser::OnProperties )

	EVT_MENU( IDM_LB_UpdateLevelsForAllActors, WxLevelBrowser::OnUpdateLevelsForAllActors )
	EVT_MENU( IDM_LB_AutoUpdateLevelsForChangedActors, WxLevelBrowser::OnAutoUpdateLevelsForChangedActors )
	EVT_UPDATE_UI( IDM_LB_AutoUpdateLevelsForChangedActors, WxLevelBrowser::UpdateUI_AutoUpdateLevelsForChangedActors )

	EVT_MENU( IDM_LB_ShowOnlySelectedLevels, WxLevelBrowser::OnShowOnlySelectedLevels )
	EVT_MENU( IDM_LB_ShowOnlyUnselectedLevels, WxLevelBrowser::OnShowOnlyUnselectedLevels )

	EVT_MENU( IDM_LB_ShowSelectedLevels, WxLevelBrowser::OnShowSelectedLevels )
	EVT_MENU( IDM_LB_HideSelectedLevels, WxLevelBrowser::OnHideSelectedLevels )
	EVT_MENU( IDM_LB_ShowAllLevels, WxLevelBrowser::OnShowAllLevels )
	EVT_MENU( IDM_LB_HideAllLevels, WxLevelBrowser::OnHideAllLevels )

	EVT_MENU( IDM_LB_LockSelectedLevels, WxLevelBrowser::OnLockSelectedLevels )
	EVT_MENU( IDM_LB_UnlockSelectedLevels, WxLevelBrowser::OnUnlockSelectedLevels )
	EVT_MENU( IDM_LB_LockAllLevels, WxLevelBrowser::OnLockAllLevels )
	EVT_MENU( IDM_LB_UnlockAllLevels, WxLevelBrowser::OnUnlockAllLevels )

	EVT_MENU( IDM_LB_SelectAllLevels, WxLevelBrowser::OnSelectAllLevels )
	EVT_MENU( IDM_LB_DeselectAllLevels, WxLevelBrowser::OnDeselectAllLevels )
	EVT_MENU( IDM_LB_InvertLevelSelection, WxLevelBrowser::OnInvertSelection )

	EVT_MENU( IDM_LB_MoveSelectedActorsToThisLevel, WxLevelBrowser::MoveActorsToThisLevel )

	EVT_MENU( IDM_LB_AddSelectedLevelsToLevelStreamingVolumes, WxLevelBrowser::OnAddStreamingLevelVolumes )
	EVT_MENU( IDM_LB_SetSelectedLevelsToLevelStreamingVolumes, WxLevelBrowser::OnSetStreamingLevelVolumes )
	EVT_MENU( IDM_LB_ClearLevelStreamingVolumeAssignments, WxLevelBrowser::OnClearStreamingLevelVolumes )
	EVT_MENU( IDM_LB_SelectAssociatedStreamingVolumes, WxLevelBrowser::OnSelectStreamingLevelVolumes )

	EVT_MENU( IDM_LB_AssignKeywords, WxLevelBrowser::OnAssignKeywords )

	EVT_MENU( IDM_LB_ShowSelectedLevels, WxLevelBrowser::OnShowSelectedLevels )

	EVT_MENU( IDM_LB_ShiftLevelUp, WxLevelBrowser::ShiftLevelUp )
	EVT_MENU( IDM_LB_ShiftLevelDown, WxLevelBrowser::ShiftLevelDown )

	EVT_MENU( ID_UI_LEVER_BROWSER_TOGGLE_FILTER_WINDOW, WxLevelBrowser::OnToggleFilterWindow )
	EVT_MENU( IMD_LB_ShowSizeData, WxLevelBrowser::OnShowSizeData )
	
	EVT_BUTTON( ID_UI_NO_FILTER_STRINGS_DETAIL, WxLevelBrowser::OnFilterStringsDetail )

	EVT_MOUSEWHEEL( WxLevelBrowser::OnMouseWheel )

END_EVENT_TABLE()

WxLevelBrowser::WxLevelBrowser()
	:	bDeferredUpdateLevelsForAllActors( FALSE )
	,	FilterWindow( NULL )
	,	FilterCheckWindow( NULL )
	,	LevelPane( NULL )
	,	RightSidePanel( NULL )
	,	SplitterWnd( NULL )
	,	LevelFilterSplitter( NULL )
	,	LevelPropertyWindow( NULL )
	,	bDoFullUpdate( FALSE )
	,	bDoTickUpdate( FALSE )
	,	bUpdateOnActivated( FALSE )
	,	bShouldDisplaySizeData( FALSE )
	,	bUpdateSuppressed( FALSE )
	,	SuppressUpdateMutex( 0 )
{
	// Register which events we want
	GCallbackEvent->Register(CALLBACK_LevelDirtied,this);
	GCallbackEvent->Register(CALLBACK_WorldChange,this);
	GCallbackEvent->Register(CALLBACK_UpdateLevelsForAllActors,this);
	GCallbackEvent->Register(CALLBACK_RefreshEditor_LevelBrowser,this);
	GCallbackEvent->Register(CALLBACK_OnActorMoved,this);
	GCallbackEvent->Register(CALLBACK_PackageSaved,this);
	GCallbackEvent->Register(CALLBACK_Undo,this);
}


WxLevelBrowser::~WxLevelBrowser()
{
	// Unregister call back events
	GCallbackEvent->UnregisterAll( this );
}

/**
* Forwards the call to our base class to create the window relationship.
* Creates any internally used windows after that
*
* @param DockID the unique id to associate with this dockable window
* @param FriendlyName the friendly name to assign to this window
* @param Parent the parent of this window (should be a Notebook)
*/
void WxLevelBrowser::Create(INT DockID, const TCHAR* FriendlyName, wxWindow* Parent)
{
	WxBrowser::Create( DockID, FriendlyName, Parent );

	// Create the sizer.
	wxBoxSizer* Sizer = new wxBoxSizer( wxVERTICAL );
	SetSizer( Sizer );

	// Create the menu bar that sits at the top of the browser window.
	MenuBar = new WxLevelBrowserMenuBar();

	// @todo DB: figure out why toolbar registration isn't working for WxBrowser-derived classes.
	//JB - Toolbars are only intended to be used with frames and NOT with just windows/panels
	//ToolBar = new WxLevelBrowserToolBar( (wxWindow*)this, -1 );


	// Create the splitter separating the level and group panes.
	//const long SplitterStyles = wxSP_LIVE_UPDATE;// | wxSP_FULLSASH;
	//SplitterWnd = new wxSplitterWindow( this, -1 );//, wxDefaultPosition, wxDefaultSize, SplitterStyles );
	SplitterWnd = new wxSplitterWindow( this, -1, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE );//wxSP_3DBORDER|wxSP_3DSASH|wxNO_BORDER );

	//big wrapper window for the left side of the window
	wxPanel* LeftPanel = new wxPanel(SplitterWnd);
	wxBoxSizer* LeftPanelSizer = new wxBoxSizer( wxVERTICAL );
	LeftPanel->SetSizer( LeftPanelSizer );

	LevelFilterSplitter = new wxSplitterWindow( LeftPanel, -1, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE );
	LeftPanelSizer->Add(LevelFilterSplitter, 1, wxGROW|wxALL, 0 );

	FilterWindow = new wxPanel(LevelFilterSplitter);
	{
		wxBoxSizer* FilterTitleSizer = new wxBoxSizer( wxVERTICAL );
		FilterWindow->SetSizer( FilterTitleSizer );

		//Create label for filter panel
		wxStaticText* FilterTitle = new wxStaticText();
		FilterTitle->Create(FilterWindow, -1, *LocalizeUnrealEd( "LevelBrowser_KeywordFilter_Title" ) );
		FilterTitleSizer->Add(FilterTitle, 0, wxLEFT | wxRIGHT, 5 );

		//Create Filter Pane
		FilterCheckWindow = new WxCheckBoxListWindow<FString>();
		FConfigSection* LevelKeywords = GConfig->GetSectionPrivate( TEXT("LevelBrowser.Keywords"), 0, 1, GEditorIni);
		if (LevelKeywords)
		{
			const BOOL bSelected = FALSE;
			for( FConfigSectionMap::TIterator It(*LevelKeywords); It; ++It )
			{
				FilterCheckWindow->AddCheck( *(It.Value()), *(It.Value()), bSelected);
			}
			FilterCheckWindow->AddCheck( TEXT("None" ), TEXT("None"), bSelected);
		}
		//create check list panel
		FilterCheckWindow->Create(FilterWindow);
		FilterCheckWindow->ShowCheckAllButton(FALSE);
		FilterCheckWindow->SetUncheckAllButtonTitle(*LocalizeUnrealEd( "LevelBrowser_KeywordFilter_UncheckAll" ));

		//if there are no keywords, change the display
		if (!LevelKeywords || (LevelKeywords->Num() == 0))
		{
			FilterTitle->Hide();
			FilterCheckWindow->Hide();

			wxButton* FilterDetailsButton = new wxButton();
			FilterDetailsButton->Create(FilterWindow, ID_UI_NO_FILTER_STRINGS_DETAIL, *LocalizeUnrealEd("LevelBrowser_KeywordFilter_NoFilterStringsButton"));
			FilterTitleSizer->Add(FilterDetailsButton, 0, wxGROW|wxALL, 5 );
		}
		FilterTitleSizer->Add(FilterCheckWindow, 1, wxGROW|wxALL, 0 );

		FilterWindow->Fit();
	}

	//Level Pane
	LevelPane = new WxLevelPane( LevelFilterSplitter, this );
	wxSizer* LevelSizer = new wxBoxSizer(wxVERTICAL);
	LevelPane->SetSizer(LevelSizer);

	// Create the level and group planes.
	RightSidePanel = new wxScrolledWindow( SplitterWnd, -1, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxVSCROLL );
	wxSizer* RightSidePanelSizer = new wxBoxSizer(wxVERTICAL);
	RightSidePanel->SetSizer(RightSidePanelSizer);

	Sizer->Add( SplitterWnd, 1, wxGROW|wxALL, 0 );

	// Tell the sizer to match the sizer's minimum size.
	GetSizer()->Fit( this );

	// Set size hints to honour minimum size.
	GetSizer()->SetSizeHints( this );

	// Initialize the left and right panes of the splitter window.
	const INT InitialSashPosition = 400;
	SplitterWnd->SplitVertically( LeftPanel, RightSidePanel, InitialSashPosition );
	SplitterWnd->Unsplit(RightSidePanel);

	const INT InitialFilterHeight = Min(200, FilterWindow->GetClientRect().GetHeight());
	//Split Filter from Levels
	LevelFilterSplitter->SplitHorizontally( FilterWindow, LevelPane, InitialFilterHeight );

	// Show/hide the filter window as appropriate, based on ini settings
	UBOOL bShowFilterWindow = TRUE;
	GConfig->GetBool( TEXT("LevelBrowser"), TEXT("ShowFilterWindow"), bShowFilterWindow, GEditorUserSettingsIni );
	SetFilterWindowVisibility( bShowFilterWindow );

	Layout();

	//MUST be done after the split so the scrolling window isn't crushed
	FilterCheckWindow->EnableScrolling();

	//listen for new filter events
	wxEvtHandler::Connect(wxID_ANY, ID_UI_REFRESH_CHECK_LIST_BOX, wxCommandEventHandler(WxLevelBrowser::FilterChange));

	// Force an update so we populate the level list when the editor first starts
	bDoFullUpdate = TRUE;
	Update();
}

/** 
 * Command handler for the the keyword filter changing
 *
 * @param	Event	Information about the event.
 */
void WxLevelBrowser::FilterChange(wxCommandEvent &Event)
{
	check(FilterCheckWindow);

	TArray<FString> Keywords;
	FilterCheckWindow->GetResults(Keywords, wxCHK_CHECKED);
	
	Freeze();
	
	LevelPane->SetFilterStrings(Keywords);
	LayoutLevelWindows();
	FixupScrollbar();

	Thaw();
}

/** Helper function to force wx to resize the scroll bar */
void WxLevelBrowser::FixupScrollbar(void)
{
	LevelPane->FitInside();
}

/** 
 * Helper function to re-layout level pane AND deselect levels that are now hidden due to filtering
 */
void WxLevelBrowser::LayoutLevelWindows(void)
{
	UpdateLevelPane();

	//unselected windows that are hidden due to filtering
	for ( WxLevelPane::TLevelWindowIterator WindowIterator( LevelPane->LevelWindowIterator() ); WindowIterator; ++WindowIterator )
	{
		WxLevelPane::WxLevelWindow* LevelWindow = *WindowIterator;
		check(LevelWindow);
		if (!LevelWindow->IsShown())
		{
			if( LevelWindow->GetLevel() != NULL )
			{
				DeselectLevelItem(LevelWindow->GetLevel());
			}
		}
	}
}


void WxLevelBrowser::OnSize(wxSizeEvent& In)
{
	// During the creation process a sizing message can be sent so don't
	// handle it until we are initialized
	if ( bAreWindowsInitialized )
	{
		const wxRect rc( GetClientRect() );
		SplitterWnd->SetSize( rc );
	}
}

/**
 * Disassociates all objects from the level property window, then hides it.
 */
void WxLevelBrowser::ClearPropertyWindow()
{
	if ( LevelPropertyWindow )
	{
		LevelPropertyWindow->RemoveAllObjects();
		LevelPropertyWindow->Show( FALSE );
	}
}

void WxLevelBrowser::Send(ECallbackEventType Event)
{
	if( bUpdateSuppressed )
	{
		return;
	}

	// if we are currently in the PIE world, there's no need to update the level browser
	if (GIsPlayInEditorWorld)
	{
		return;
	}

	if( Event == CALLBACK_UpdateLevelsForAllActors )
	{
		// Check for re-entrancy, as the methods we call here can actually trigger another
		// CALLBACK_OnActorMoved while moving actors between levels
		if( !EditorLevelUtils::IsCurrentlyUpdatingLevelsForActors() )
		{
			// Queue an update of all actors
			bDeferredUpdateLevelsForAllActors = TRUE;
		}
		else
		{
			warnf( NAME_DevLevelTools, TEXT( "AutoLevel: (Warning) Ignored UpdateLevelsForAllActors callback while already processing actors" ) );
		}
	}
	else if ( Event == CALLBACK_LevelDirtied || Event == CALLBACK_Undo )
	{
		// Level dirties require only a redraw.
		RequestUpdate();
	}
	else
	{
		if ( Event == CALLBACK_WorldChange )
		{
			ClearPropertyWindow();
		}

		//reset all actor updates since the actors will no longer be valid
		DeferredActorsToUpdateLevelsFor.Empty();

		// Do a full rebuild.
		Update();
	}
}


void WxLevelBrowser::Send(ECallbackEventType Event, UObject* InObject)
{
	if( Event == CALLBACK_OnActorMoved )
	{
		// Check for re-entrancy, as the methods we call here can actually trigger another
		// CALLBACK_OnActorMoved while moving actors between levels
		if( !EditorLevelUtils::IsCurrentlyUpdatingLevelsForActors() )
		{
			AActor* MovedActor = CastChecked<AActor>( InObject );
			if( !MovedActor->bDeleteMe )
			{
				// Ignore actors in transient buffer levels
				if( MovedActor->GetLevel()->GetName() != TEXT("TransLevelMoveBuffer") )
				{
					warnf( NAME_DevLevelTools, TEXT( "AutoLevel: Received OnActorMoved for %s (adding to list of actors to update)" ), *MovedActor->GetName() );

					// Add this actor to the list of actors that may need to be moved to new levels.  Note that
					// this is a static array that we'll process later on.  It's not safe to move actors between
					// levels or to change the selected actor set in the current stack frame
					DeferredActorsToUpdateLevelsFor.AddUniqueItem( MovedActor );
				}
			}
		}
		else
		{
			warnf( NAME_DevLevelTools, TEXT( "AutoLevel: (Warning) Ignored OnActorMoved for (%s) while already processing actors" ), *InObject->GetName() );
		}
	}
}

/**
 * Notifies all observers that are registered for this event type
 * that the event has fired
 *
 * @param InType the event that was fired
 * @param InString the string information associated with this event
 * @param InObject the object associated with this event
 */
void WxLevelBrowser::Send(ECallbackEventType InType,const FString& InString, UObject* InObject)
{
	// If a package was saved, check to see if it's a non-PIE map package. If so, perform a quick UI update
	// of the level browser to clear dirty flags, etc.
	if ( InType == CALLBACK_PackageSaved )
	{
		UPackage* ObjAsPackage = Cast<UPackage>( InObject );
		if ( ObjAsPackage && ObjAsPackage->ContainsMap() && !ObjAsPackage->RootPackageHasAnyFlags( PKG_PlayInEditor ) )
		{
			RequestUpdate();
		}
	}
}



/** Should be called before every tick in the editor to make sure that recently moved/changed actors are
    moved (copied/pasted) into their most-appropriate levels, if needed */
void WxLevelBrowser::ProcessUpdatesToDeferredActors()
{
	check( !GIsPlayInEditorWorld );

	// Check for re-entrancy
	static UBOOL StaticIsProcessingDeferredActors = FALSE;
	check( !StaticIsProcessingDeferredActors );
	StaticIsProcessingDeferredActors = TRUE;

	if( DeferredActorsToUpdateLevelsFor.Num() > 0 || bDeferredUpdateLevelsForAllActors )
	{
		// Is auto-updating of level grid volume actors enabled right now?
		if( GEditor->GetUserSettings().bUpdateActorsInGridLevelsImmediately )
		{
			// Never move actors around while Matinee is open as their locations may be temporary
			if( !GEditorModeTools().IsModeActive(EM_InterpEdit) )
			{
				// Update levels (level grid volumes)
				if( bDeferredUpdateLevelsForAllActors )
				{
					warnf( NAME_DevLevelTools, TEXT( "AutoLevel: Processing all actors" ) );

					EditorLevelUtils::UpdateLevelsForAllActors();
				}
				else
				{
					warnf( NAME_DevLevelTools, TEXT( "AutoLevel: Processing %i deferred actors" ), DeferredActorsToUpdateLevelsFor.Num() );

					EditorLevelUtils::UpdateLevelsForActorsInLevelGridVolumes( DeferredActorsToUpdateLevelsFor );
				}

				warnf( NAME_DevLevelTools, TEXT( "AutoLevel: Finished processing deferred actors" ) );
			}
		}

		
		// Clear the list of actors.  Many of these actors may have been deleted at this point anyway (copies
		// pasted into the destination level.)
		DeferredActorsToUpdateLevelsFor.Reset();
		bDeferredUpdateLevelsForAllActors = FALSE;
	}

	StaticIsProcessingDeferredActors = FALSE;
}


/**
 * Adds entries to the browser's accelerator key table.  Derived classes should call up to their parents.
 */
void WxLevelBrowser::AddAcceleratorTableEntries(TArray<wxAcceleratorEntry>& Entries)
{
	WxBrowser::AddAcceleratorTableEntries( Entries );
	Entries.AddItem( wxAcceleratorEntry( wxACCEL_NORMAL, WXK_UP, IDM_LB_ShiftLevelUp ) );
	Entries.AddItem( wxAcceleratorEntry( wxACCEL_NORMAL, WXK_DOWN, IDM_LB_ShiftLevelDown ) );
}

/** Handler for IDM_RefreshBrowser events; updates the browser contents. */
void WxLevelBrowser::OnRefresh(wxCommandEvent& In)
{
	// Rebuild the level, layer and actor panes.
	Update();
}

void WxLevelBrowser::RequestUpdate(UBOOL bFullUpdate)
{
//	bDoFullUpdate |= bFullUpdate;
	Refresh();
}

void WxLevelBrowser::Update()
{
	UBOOL bUpdate = IsShownOnScreen() && ( SuppressUpdateMutex == 0 );
	if( bDoFullUpdate )
	{
		bUpdate = TRUE;
	}
	if ( bUpdate )
	{
		TArray<FLevelBrowserItem> PreviousLevels = AllLevelItems;
		GenerateNewLevelList(AllLevelItems);

		if (PreviousLevels != AllLevelItems)
		{
			BeginUpdate();

			UpdateLevelList();
			UpdateLevelPane();

			EndUpdate();
		}
		bDoFullUpdate = FALSE;
		bUpdateOnActivated = FALSE;
	}
	else
	{
		bUpdateOnActivated = TRUE;
	}
}

void WxLevelBrowser::Activated()
{
	WxBrowser::Activated();
	if ( bUpdateOnActivated && SuppressUpdateMutex == 0 )
	{
		Update();
	}
}

/**
 * Generate the new list of levels to display
 * @param OutLevelList - List of Levels that should be presently displayed
 */
void WxLevelBrowser::GenerateNewLevelList(TArray<FLevelBrowserItem>& OutLevelList)
{
	OutLevelList.Empty();

	if ( GWorld )
	{
		// Construct a list of all level grid volumes
		// @todo perf: Ideally don't iterate over all actors here (cache level grid volumes?)
		TArray< ALevelGridVolume* > LevelGridVolumes;
		for( FActorIterator ActorIt; ActorIt; ++ActorIt )
		{
			// Is this actor a level grid volume?
			ALevelGridVolume* LevelGridVolume = Cast< ALevelGridVolume >( *ActorIt );
			if( LevelGridVolume != NULL )
			{
				// Sort volumes alphabetically as we add them
				INT BestInsertIndex = INDEX_NONE;
				for( INT CurVolumeIndex = 0; CurVolumeIndex < LevelGridVolumes.Num(); ++CurVolumeIndex )
				{
					ALevelGridVolume* OtherLevelGridVolume = LevelGridVolumes( CurVolumeIndex );
					check( OtherLevelGridVolume != NULL );
					if( LevelGridVolume->GetLevelGridVolumeName() < OtherLevelGridVolume->GetLevelGridVolumeName() )
					{
						BestInsertIndex = CurVolumeIndex;
					}
				}

				if( BestInsertIndex != INDEX_NONE )
				{
					LevelGridVolumes.InsertItem( LevelGridVolume, BestInsertIndex );
				}
				else
				{
					LevelGridVolumes.AddItem( LevelGridVolume );
				}
			}
		}

		// Add level grid volumes
		for( INT CurVolumeIndex = 0; CurVolumeIndex < LevelGridVolumes.Num(); ++CurVolumeIndex )
		{
			ALevelGridVolume* LevelGridVolume = LevelGridVolumes( CurVolumeIndex );
			check( LevelGridVolume != NULL );
			OutLevelList.AddItem( FLevelBrowserItem( LevelGridVolume ) );
		}

		// Add main level.
		OutLevelList.AddItem( FLevelBrowserItem( GWorld->PersistentLevel ) );

		// Add secondary levels.
		AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();
		for( INT LevelIndex = 0 ; LevelIndex < WorldInfo->StreamingLevels.Num() ; ++LevelIndex )
		{
			ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
			if( StreamingLevel && StreamingLevel->LoadedLevel )
			{
				OutLevelList.AddItem( FLevelBrowserItem( StreamingLevel->LoadedLevel ) );
			}
		}
	}
}


void WxLevelBrowser::UpdateLevelList()
{
	// Clear level list.
	LevelPane->Clear();
	SelectedLevelItems.Empty();

	for( INT CurItemIndex = 0; CurItemIndex < AllLevelItems.Num(); ++CurItemIndex )
	{
		FLevelBrowserItem CurItem = AllLevelItems( CurItemIndex );
		if( CurItem.IsLevel() )
		{
			LevelPane->AddLevel( CurItem.GetLevel() );
		}
		else if( CurItem.IsLevelGridVolume() )
		{
			LevelPane->AddLevelGridVolume( CurItem.GetLevelGridVolume() );
		}
	}

	FixupScrollbar();
}

void WxLevelBrowser::UpdateLevelPane()
{
	LevelPane->LayoutLevelWindows();
}

void WxLevelBrowser::UpdateUIForLevel( const ULevel* InLevel )
{
	LevelPane->UpdateUIForLevel( InLevel );
}

/**
 * Clears the level selection, then sets the specified level.  Refreshes.
 */
void WxLevelBrowser::SelectSingleLevelItem( FLevelBrowserItem InLevelItem )
{
	SelectedLevelItems.Empty();
	SelectedLevelItems.AddItem( InLevelItem );
	RequestUpdate();
}

/**
 * Adds the specified level to the selection set.  Refreshes.
 */
void WxLevelBrowser::SelectLevelItem( FLevelBrowserItem InLevelItem )
{
	SelectedLevelItems.AddUniqueItem( InLevelItem );
	RequestUpdate();
}

/**
 * Selects the level (and other levels as appropriate) as if it were shift-clicked upon. Behavior
 * roughly mimics that of Windows Explorer with some slight differences. Refreshes.
 *
 * @param	InLevelItem	Level that was shift-clicked upon
 */
void WxLevelBrowser::ShiftSelectLevelItem( FLevelBrowserItem InLevelItem )
{	
	INT InLevelIndex = 0;
	INT StartNewSelectionIndex = 0;

	// Handle the case where a previous selection was made prior to this shift-click selection
	if ( SelectedLevelItems.Num() > 0 )
	{
		// We want to mimic Windows Explorer behavior as much as possible, so we need to find the
		// item that was selected last to potentially use as our starting selection index 
		// (going up to the newly-clicked level). Because the selected levels are stored in an array 
		// that is cleared each time a multi-selection occurs, the last selected level should be the last 
		// item in the array.
		FLevelBrowserItem LastSelectedLevelItem = SelectedLevelItems( SelectedLevelItems.Num() - 1 );
		TArray<FLevelBrowserItem> PreviousSelectionRange;
		
		// Iterate through all of the levels (not just the selected ones) to find the index of the newly-clicked level
		// as well as the index of the level that was last selected.
		for ( WxLevelPane::TLevelWindowIterator WindowIterator( LevelPane->LevelWindowIterator() ); WindowIterator; ++WindowIterator )
		{
			FLevelBrowserItem CurWindowLevelItem = ( *WindowIterator )->GetLevelItem();
			
			// Determine the index of the newly-clicked level in the level array
			if ( CurWindowLevelItem == InLevelItem )
			{
				InLevelIndex = WindowIterator.GetIndex();
			}

			// Determine the index of the last selected level in the level array
			if ( CurWindowLevelItem == LastSelectedLevelItem )
			{
				StartNewSelectionIndex = WindowIterator.GetIndex();
				
				// To further mimic Windows Explorer behavior we need to identify if the last selected
				// level is currently within a selection range. If it is, the first selected level within
				// the selection range will actually be used for the starting index, instead of the last
				// selected. In order to facilitate this, we need to store the pointers of the levels
				// in the selection range.
				WxLevelPane::TLevelWindowIterator SelectedRangeReverseIterator( WindowIterator );
				--SelectedRangeReverseIterator;

				
				// Find all of the selected levels in the range whose indices occur prior to the last selected level
				for ( ; SelectedRangeReverseIterator; --SelectedRangeReverseIterator)
				{
					FLevelBrowserItem PreviousWindowLevelItem = ( *SelectedRangeReverseIterator )->GetLevelItem();
					if ( IsItemSelected( PreviousWindowLevelItem ) )
					{
						PreviousSelectionRange.AddUniqueItem( PreviousWindowLevelItem );
					}
					else
					{
						break;
					}
				}
				
				WxLevelPane::TLevelWindowIterator SelectedRangeForwardIterator( WindowIterator );
				++SelectedRangeForwardIterator;

				// Find all of the selected levels in the range whose indices occur after the last selected level
				for ( ; SelectedRangeForwardIterator; ++SelectedRangeForwardIterator)
				{
					FLevelBrowserItem ForwardWindowLevelItem = ( *SelectedRangeForwardIterator )->GetLevelItem();
					if ( IsItemSelected( ForwardWindowLevelItem ) )
					{
						PreviousSelectionRange.AddUniqueItem( ForwardWindowLevelItem );
					}
					else
					{
						break;
					}
				}
			}
		}

		// If the last selection was part of a selection range, we need to determine which level in the range was selected first
		// and use that level's index as the index to start the new selection from. This is actually a slight difference from
		// the Windows Explorer functionality, which uses the first index (as we do) if the range was formed by shift-clicks, but
		// differentiates and uses the last selected item if the previous click was a Ctrl-click.
		if ( PreviousSelectionRange.Num() > 0 )
		{
			INT FirstSelectedInRangeIndex = INT_MAX;

			// We know which levels were previously selected, but not in which order they were selected yet. We want to determine the
			// first selected by comparing their indices within the selected array. Note that we cannot simply use the first element in the
			// array to find the first selected in the range, because it's possible the user formed the range via CTRL-clicks, and their
			// first selection was outside of the range.
			for ( TArray<FLevelBrowserItem>::TConstIterator PrevSelectionIterator( PreviousSelectionRange ); PrevSelectionIterator; ++PrevSelectionIterator )
			{
				INT CurIndex = INT_MAX;
				SelectedLevelItems.FindItem( *PrevSelectionIterator, CurIndex );
				if ( CurIndex < FirstSelectedInRangeIndex )
				{
					FirstSelectedInRangeIndex = CurIndex;
				}
			}
			
			// Now we know which level in the range was first selected, we need to find its index in the overall window array to
			// determine the starting index of the new selection
			FLevelBrowserItem FirstSelectedLevelItem = SelectedLevelItems( FirstSelectedInRangeIndex );
			for ( WxLevelPane::TLevelWindowIterator WindowIterator( LevelPane->LevelWindowIterator() ); WindowIterator; ++WindowIterator )
			{
				FLevelBrowserItem CurWindowLevelItem = ( *WindowIterator )->GetLevelItem();
				if ( FirstSelectedLevelItem == CurWindowLevelItem )
				{
					StartNewSelectionIndex = WindowIterator.GetIndex();
					break;
				}
			}
		}

		// Discard the previous selection
		SelectedLevelItems.Empty();

		// Determine which way the selection will proceed depending upon the starting index relative to the newly-clicked index
		const INT SelectionDirection = ( StartNewSelectionIndex <  InLevelIndex ) ? 1 : -1;
		const INT EndIndex = InLevelIndex + SelectionDirection;

		// Start the iterator at the determined starting index
		WxLevelPane::TLevelWindowIterator WindowSelectionIterator( LevelPane->LevelWindowIterator() );
		WindowSelectionIterator += StartNewSelectionIndex;

		// Select the levels in the newly calculated range
		for ( ; WindowSelectionIterator && WindowSelectionIterator.GetIndex() != EndIndex; WindowSelectionIterator += SelectionDirection )
		{
			FLevelBrowserItem CurWindowLevelItem = ( *WindowSelectionIterator )->GetLevelItem();
			if ( LevelPane->IsChildWindowVisible( CurWindowLevelItem.GetLevel() ) )
			{
				SelectedLevelItems.AddUniqueItem( CurWindowLevelItem );
			}
		}

		RequestUpdate();
	}

	// If nothing was previously selected, just go ahead and single select the shift-clicked level
	else
	{
		SelectSingleLevelItem( InLevelItem );
	}
}

/**
 * Selects all selected levels.  Refreshes.
 */
void WxLevelBrowser::SelectAllLevels()
{
	SelectedLevelItems.Empty();
	for ( WxLevelPane::TLevelWindowIterator It( LevelPane->LevelWindowIterator() ) ; It ; ++It )
	{
		WxLevelPane::WxLevelWindow* CurWindow = *It;
		if( CurWindow->GetLevelItem().IsLevel() )
		{
			SelectedLevelItems.AddUniqueItem( CurWindow->GetLevelItem() );
		}
	}
	RequestUpdate();
}

/**
 * Deselects the specified level.  Refreshes.
 */	
void WxLevelBrowser::DeselectLevelItem( FLevelBrowserItem InLevelItem )
{
	for ( INT LevelIndex = 0 ; LevelIndex < SelectedLevelItems.Num() ; ++LevelIndex )
	{
		const FLevelBrowserItem& CurLevel = SelectedLevelItems( LevelIndex );
		if ( CurLevel == InLevelItem )
		{
			SelectedLevelItems.Remove( LevelIndex );
			RequestUpdate();
			break;
		}
	}
}

/**
 * Deselects all selected levels.  Refreshes.
 */
void WxLevelBrowser::DeselectAllLevels()
{
	SelectedLevelItems.Empty();
	RequestUpdate();
}

/**
 * Inverts the level selection.  Refreshes.
 */
void WxLevelBrowser::InvertLevelSelection()
{
	TArray<FLevelBrowserItem> LevelsToBeSelected;

	// Iterate over all levels and mark unselected levels for selection.
	for ( WxLevelPane::TLevelWindowIterator It( LevelPane->LevelWindowIterator() ) ; It ; ++It )
	{
		WxLevelPane::WxLevelWindow* CurWindow = *It;
		if ( !IsItemSelected( CurWindow->GetLevelItem() ) )
		{
			LevelsToBeSelected.AddItem( CurWindow->GetLevelItem() );
		}
	}

	// Clear out current selections.
	SelectedLevelItems.Empty();

	// Select marked levels.
	for ( INT LevelIndex = 0 ; LevelIndex < LevelsToBeSelected.Num() ; ++LevelIndex )
	{
		FLevelBrowserItem CurLevelItem = LevelsToBeSelected( LevelIndex );
		SelectedLevelItems.AddItem( CurLevelItem );
	}
	RequestUpdate();
}

/**
 * @return		TRUE if the specified level is selected in the level browser.
 */
UBOOL WxLevelBrowser::IsItemSelected( FLevelBrowserItem InLevelItem ) const
{
	return SelectedLevelItems.ContainsItem( InLevelItem );
}



/**
 * Returns the head of the selection list, or NULL if nothing is selected.
 */
FLevelBrowserItem* WxLevelBrowser::GetSelectedLevelItem()
{
	TSelectedLevelItemIterator It( SelectedLevelItemIterator() );
	FLevelBrowserItem* SelectedLevelItem = It ? &( *It ) : NULL;
	return SelectedLevelItem;
}

/**
 * Returns the head of the selection list, or NULL if nothing is selected.
 */
const FLevelBrowserItem* WxLevelBrowser::GetSelectedLevelItem() const
{
	TSelectedLevelItemConstIterator It( SelectedLevelItemConstIterator() );
	const FLevelBrowserItem* SelectedLevelItem = It ? &( *It ) : NULL;
	return SelectedLevelItem;
}

/**
 * Returns NULL if the number of selected level is zero or more than one.  Otherwise,
 * returns the singly selected level.
 */
FLevelBrowserItem* WxLevelBrowser::GetSingleSelectedLevelItem()
{
	// See if there is a single level selected.
	FLevelBrowserItem* SingleSelectedLevel = NULL;
	for ( TSelectedLevelItemIterator It = SelectedLevelItemIterator() ; It ; ++It )
	{
		FLevelBrowserItem& LevelItem = *It;
		{
			if ( !SingleSelectedLevel )
			{
				SingleSelectedLevel = &LevelItem;
			}
			else
			{
				// Multiple levels are selected.
				return NULL;
			}
		}
	}
	return SingleSelectedLevel;
}

WxLevelBrowser::TSelectedLevelItemIterator WxLevelBrowser::SelectedLevelItemIterator()
{
	return TSelectedLevelItemIterator( SelectedLevelItems );
}

WxLevelBrowser::TSelectedLevelItemConstIterator WxLevelBrowser::SelectedLevelItemConstIterator() const
{
	return TSelectedLevelItemConstIterator( SelectedLevelItems );
}

/**
 * Returns the number of selected levels.
 */
INT WxLevelBrowser::GetNumSelectedLevelItems() const
{
	return SelectedLevelItems.Num();
}

/**
 * Displays a property window for the selected levels.
 */
void WxLevelBrowser::ShowPropertiesForSelectedLevelItems()
{
	TArray<UObject*> ObjectsWithProperties;

	for ( TSelectedLevelItemIterator It = SelectedLevelItemIterator() ; It ; ++It )
	{
		if( It->IsLevel() )
		{
			ULevel* Level = It->GetLevel();
			ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel( Level ); 
			if ( StreamingLevel )
			{
				ObjectsWithProperties.AddItem( StreamingLevel );
			}
		}
		else if( It->IsLevelGridVolume() )
		{
			ALevelGridVolume* LevelGridVolume = It->GetLevelGridVolume();
			ObjectsWithProperties.AddItem( LevelGridVolume );
		}
	}

	if ( ObjectsWithProperties.Num() )
	{
		// Allocate a new property window if none exists.
		if ( !LevelPropertyWindow )
		{
			LevelPropertyWindow = new WxPropertyWindowFrame;
			LevelPropertyWindow->Create( GApp->EditorFrame, -1, this );
		}

		// Display the level in the property window.
		//LevelPropertyWindow->AllowClose();
		LevelPropertyWindow->SetObjectArray( ObjectsWithProperties, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories );
		LevelPropertyWindow->Show();
	}
}

/**
 * Refreshes the level browser if any level properties were modified.  Refreshes indirectly.
 */
void WxLevelBrowser::NotifyPostChange(void* Src, UProperty* PropertyThatChanged)
{
	// Get the level that was modified from the property window, if it was specified
	if( Src )
	{
		// Check if ULevelStreaming::bShouldBeVisibleInEditor was the property that changed.
		UBOOL bVisibilityChanged = FALSE;
		if ( PropertyThatChanged )
		{
			bVisibilityChanged = appStricmp( *PropertyThatChanged->GetName(), TEXT("bShouldBeVisibleInEditor") ) == 0;
		}

		WxPropertyControl* PropertyWindowItem = static_cast<WxPropertyControl*>( Src );
		WxPropertyWindow* PropertyWindow = PropertyWindowItem->GetPropertyWindow();
		for ( WxPropertyWindow::TObjectIterator Itor( PropertyWindow->ObjectIterator() ) ; Itor ; ++Itor )
		{
			// Need to see what this is - could be a LevelGridVolume
			ULevelStreaming* LevelStreaming = Cast<ULevelStreaming>( *Itor );
			if(LevelStreaming)
			{
				if ( bVisibilityChanged )
				{
					LevelBrowser::SetLevelVisibility( LevelStreaming->LoadedLevel, LevelStreaming->bShouldBeVisibleInEditor );
				}
				LevelPane->UpdateUIForLevel( LevelStreaming->LoadedLevel );
			}
		}
	}
	else
	{
		// We can't get at the level.  Just do a simple refresh.
		RequestUpdate();
	}
}

/**
 * Used to track when the level browser's level property window is destroyed.
 */
void WxLevelBrowser::NotifyDestroy(void* Src)
{
	if ( Src == LevelPropertyWindow )
	{
		LevelPropertyWindow = NULL;
	}
}

/**
 * Clear out object references to allow levels to be GCed.
 *
 * @param	Ar			The archive to serialize with.
 */
void WxLevelBrowser::Serialize(FArchive& Ar)
{
	if ( !GIsPlayInEditorWorld )
	{
		LevelPane->Clear();
		SelectedLevelItems.Empty();
		AllLevelItems.Empty();
		bUpdateOnActivated = TRUE;	// But update the window if it becomes active
	}
}

/**
 * Updates the property window that contains level streaming objects, if it exists.
 */
void WxLevelBrowser::UpdateLevelPropertyWindow()
{
	if ( LevelPropertyWindow )
	{
		LevelPropertyWindow->Rebuild();
	}
}


/**
 * Presents an "Open File" dialog to the user and adds the selected level(s) to the world.  Refreshes.
 */
void WxLevelBrowser::ImportLevelsFromFile()
{
	// Disallow for cooked packages.
	if( GWorld && GWorld->GetOutermost()->PackageFlags & PKG_Cooked )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_OperationDisallowedOnCookedContent") );
		return;
	}

	WxFileDialog FileDialog( GApp->EditorFrame,
								*LocalizeUnrealEd("Open"),
								*GApp->LastDir[LD_UNR],
								TEXT(""),
								*FEditorFileUtils::GetFilterString(FI_Load),
								wxOPEN | wxFILE_MUST_EXIST | wxMULTIPLE );

	if( FileDialog.ShowModal() == wxID_OK )
	{
		// Get the set of selected paths from the dialog
		wxArrayString FilePaths;
		FileDialog.GetPaths( FilePaths );

		TArray<FString> Filenames;
		for( UINT FileIndex = 0 ; FileIndex < FilePaths.Count() ; ++FileIndex )
		{
			// Strip paths from to get the level package names.
			const FFilename FilePath( FilePaths[FileIndex] );

			// make sure the level is in our package cache, because the async loading code will use this to find it
			FString PackageFileName;
			if (!GPackageFileCache->FindPackageFile( *FilePath.GetBaseFilename(), NULL, PackageFileName))
			{
				appMsgf( AMT_OK, *LocalizeUnrealEd("Error_LevelImportFromExternal"));
				return;				
			}

			Filenames.AddItem( FilePath.GetBaseFilename() );
		}

		// Sort the level packages alphabetically by name.
		Sort<USE_COMPARE_CONSTREF(FString, LevelBrowser)>( &Filenames(0), Filenames.Num() );

		// Fire CALLBACK_LevelDirtied when falling out of scope.
		FScopedLevelDirtied LevelDirtyCallback;

		// Try to add the levels that were specified in the dialog.
		for( INT FileIndex = 0 ; FileIndex < Filenames.Num() ; ++FileIndex )
		{
			const FString& BaseFilename	= Filenames(FileIndex);
			if ( EditorLevelUtils::AddLevelToWorld( *BaseFilename ) )
			{
				LevelDirtyCallback.Request();
			}
		} // for each file

		// For safety
		if( GEditorModeTools().IsModeActive( EM_Landscape ) )
		{
			GEditorModeTools().ActivateMode( EM_Default );
		}

		// refresh editor windows
		GCallbackEvent->Send( CALLBACK_RefreshEditor_AllBrowsers );
	}
}



/**
 * Makes the specified level the current level.  Refreshes.
 */
void WxLevelBrowser::MakeLevelCurrent(ULevel* InLevel)
{
	UBOOL bNeedUpdate = FALSE;

	// Make sure there is no grid volume 'current'
	if( GWorld->CurrentLevelGridVolume != NULL )
	{
		GWorld->CurrentLevelGridVolume = NULL;

		// Fire off an event
		GCallbackEvent->Send( CALLBACK_NewCurrentLevel );

		bNeedUpdate = TRUE;
	}


	// If something is selected and not already current . . .
	if ( InLevel && InLevel != GWorld->CurrentLevel )
	{
		// Locked levels can't be made current.
		if ( !FLevelUtils::IsLevelLocked( InLevel ) )
		{ 
			// Make current.
			GWorld->CurrentLevel = InLevel;

			// Fire off an event
			GCallbackEvent->Send( CALLBACK_NewCurrentLevel );


			// Deselect all selected builder brushes.
			UBOOL bDeselectedSomething = FALSE;
			for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
			{
				AActor* Actor = static_cast<AActor*>( *It );
				checkSlow( Actor->IsA(AActor::StaticClass()) );

				if ( Actor->IsBrush() && Actor->IsABuilderBrush() )
				{
					GEditor->SelectActor( Actor, FALSE, NULL, FALSE );
					bDeselectedSomething = TRUE;
				}
			}

			// Send a selection change callback if necessary.
			if ( bDeselectedSomething )
			{
				GEditor->NoteSelectionChange();
			}

			// Force the current level to be visible.
			LevelPane->SetLevelVisibility( GWorld->CurrentLevel, TRUE );

			bNeedUpdate = TRUE;
		}
		else
		{
			appMsgf(AMT_OK, TEXT("MakeLevelCurrent: %s"), *LocalizeUnrealEd(TEXT("Error_OperationDisallowedOnLockedLevel")));
		}
	}


	if( bNeedUpdate )
	{
		// Refresh the level browser.
		RequestUpdate();
	}
}



/**
 * Makes the specified level grid volume 'current'.  Refreshes.
 */
void WxLevelBrowser::MakeLevelGridVolumeCurrent(ALevelGridVolume* InLevelGridVolume)
{
	// If something is selected and not already current . . .
	if ( InLevelGridVolume != GWorld->CurrentLevelGridVolume )
	{
		// Make current.
		GWorld->CurrentLevelGridVolume = InLevelGridVolume;

		// Fire off an event
		GCallbackEvent->Send( CALLBACK_NewCurrentLevel );

		// Refresh the level browser.
		RequestUpdate();
	}
}


/**
 * Merges all visible levels into the persistent level at the top.  All included levels are then removed.
 *
 * @param bDiscardHiddenLevels	If TRUE, all hidden levels are discarded when the visible levels are merged; if FALSE, they are preserved
 *
 * @param bForceSaveAs			If TRUE, prompts the user to save newly merged level into a different filename. If FALSE, will just remain dirty.		
 */
void WxLevelBrowser::MergeVisibleLevels( UBOOL bDiscardHiddenLevels, UBOOL bForceSaveAs )
{
	// Disallow for a cooked GWorld or any visible levels that are cooked.
	UBOOL bFoundCookedPackage = ( GWorld->GetOutermost()->PackageFlags & PKG_Cooked ) ? TRUE : FALSE;
	UBOOL bFoundLockedLevel = FALSE;
	AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();
	for( INT LevelIndex = 0 ; LevelIndex < WorldInfo->StreamingLevels.Num() && !bFoundCookedPackage ; ++LevelIndex )
	{
		const ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
		if ( StreamingLevel && StreamingLevel->bShouldBeVisibleInEditor )
		{
			if ( StreamingLevel->LoadedLevel->GetOutermost()->PackageFlags & PKG_Cooked )
			{
				bFoundCookedPackage = TRUE;
			}

			if ( FLevelUtils::IsLevelLocked(StreamingLevel->LoadedLevel) )
			{
				bFoundLockedLevel = TRUE;
			}
		}
	}

	if( bFoundCookedPackage )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_OperationDisallowedOnCookedContent") );
		return;
	}

	if ( bFoundLockedLevel )
	{
		appMsgf(AMT_OK, TEXT("MergeVisibleLevels: %s"), *LocalizeUnrealEd(TEXT("Error_OperationDisallowedOnLockedLevel")));
		return;
	}

	// Make the persistent level current
	MakeLevelCurrent( GWorld->PersistentLevel );

	// Keep a list of which levels should be removed, as the user might have specified to keep hidden levels
	TArray<ULevelStreaming*> LevelsToRemove;

	// Move all actors from visible sublevels into the persistent level
	for( INT LevelIndex = 0 ; LevelIndex < WorldInfo->StreamingLevels.Num() ; ++LevelIndex )
	{
		ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);

		// If the level is visible, it needs to have all of its actors moved to the current one
		if ( StreamingLevel && StreamingLevel->bShouldBeVisibleInEditor )
		{
			for ( INT ActorIndex = 2 ; ActorIndex < StreamingLevel->LoadedLevel->Actors.Num() ; ++ActorIndex )
			{
				AActor* Actor = StreamingLevel->LoadedLevel->Actors( ActorIndex );
				if ( Actor )
				{
					GEditor->SelectActor( Actor, TRUE, NULL, FALSE, TRUE );
				}
			}

			const UBOOL bUseCurrentLevelGridVolume = TRUE;
			GEditor->MoveSelectedActorsToCurrentLevel( bUseCurrentLevelGridVolume );
			LevelsToRemove.AddItem( StreamingLevel );
		}
		// Handle the case where the level is hidden, but the user wants to discard hidden levels anyway
		else if ( bDiscardHiddenLevels )
		{
			LevelsToRemove.AddItem( StreamingLevel );
		}
	}

	// Remove all requested sublevels from the persistent level
	for ( TArray<ULevelStreaming*>::TIterator RemoveLevelIter( LevelsToRemove ); RemoveLevelIter; ++RemoveLevelIter )
	{
		ULevelStreaming* StreamingLevel = *RemoveLevelIter;
		if( StreamingLevel && StreamingLevel->LoadedLevel )
		{
			DeselectLevelItem( StreamingLevel->LoadedLevel );
			EditorLevelUtils::RemoveLevelFromWorld( StreamingLevel->LoadedLevel );
		}
	}

	// Ask the user to save the flattened file

	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
	GCallbackEvent->Send( CALLBACK_RefreshEditor_LevelBrowser );

	// Prompt user to save newly merged level into a different filename

	if (bForceSaveAs)
	{
		FEditorFileUtils::SaveAs( GWorld );
	}
}

/**
 * Selects all actors in the selected levels.
 */
void WxLevelBrowser::SelectActorsOfSelectedLevelItems()
{
	const FScopedBusyCursor BusyCursor;

	const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("SelectAllActors")) );
	GEditor->GetSelectedActors()->Modify();
	GEditor->SelectNone( FALSE, TRUE );

	TArray< ULevel* > LevelsToSelectActorsIn;
	for ( TSelectedLevelItemIterator It = SelectedLevelItemIterator() ; It ; ++It )
	{
		if( It->IsLevel() )
		{
			ULevel* Level = It->GetLevel();

			ULevel* LevelToSelect = NULL;
			if ( Level == GWorld->PersistentLevel)
			{
				LevelToSelect = Level;
			}
			else
			{
				ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel( Level ); 
				if ( StreamingLevel && StreamingLevel->LoadedLevel )
				{
					LevelToSelect = StreamingLevel->LoadedLevel;
				}
			}

			if ( LevelToSelect )
			{
				LevelsToSelectActorsIn.AddUniqueItem( LevelToSelect );
			}
		}
		else if( It->IsLevelGridVolume() )
		{
			ALevelGridVolume* LevelGridVolume = It->GetLevelGridVolume();
			TArray< ULevelStreaming* > LevelStreamingsForVolume;
			LevelGridVolume->GetLevelsForAllCells( LevelStreamingsForVolume );	// Out
			
			for( INT CurLevelIndex = 0; CurLevelIndex < LevelStreamingsForVolume.Num(); ++CurLevelIndex )
			{
				ULevelStreaming* CurLevelStreaming = LevelStreamingsForVolume( CurLevelIndex );
				if( CurLevelStreaming->LoadedLevel != NULL )
				{
					LevelsToSelectActorsIn.AddUniqueItem( CurLevelStreaming->LoadedLevel );
				}
			}
		}
	}


	if( LevelsToSelectActorsIn.Num() > 0 )
	{
		for( INT CurLevelIndex = 0; CurLevelIndex < LevelsToSelectActorsIn.Num(); ++CurLevelIndex )
		{
			ULevel* LevelToSelect = LevelsToSelectActorsIn( CurLevelIndex );
			if ( !FLevelUtils::IsLevelLocked(LevelToSelect) )
			{
				for ( INT ActorIndex = 2 ; ActorIndex < LevelToSelect->Actors.Num() ; ++ActorIndex )
				{
					AActor* Actor = LevelToSelect->Actors( ActorIndex );
					if ( Actor )
					{
						GEditor->SelectActor( Actor, TRUE, NULL, FALSE, FALSE );
					}
				}
			}
			else
			{
				warnf(TEXT("SelectActorsOfSelectedLevelItems: %s"), *LocalizeUnrealEd(TEXT("Error_OperationDisallowedOnLockedLevel")));
			}
		}

		GEditor->NoteSelectionChange();
	}
}


/**
 * Sets visibility for the selected levels.
 */
void WxLevelBrowser::SetSelectedLevelVisibility(UBOOL bVisible)
{
	const FScopedBusyCursor BusyCursor;
	for ( TSelectedLevelItemIterator It = SelectedLevelItemIterator() ; It ; ++It )
	{
		if( It->IsLevel() )
		{
			ULevel* Level = It->GetLevel();
			LevelPane->SetLevelVisibility( Level, bVisible );
		}
	}
	RequestUpdate();
}

/**
 * @param bShowSelected		If TRUE, show only selected levels; if FALSE show only unselected levels.
 */
void WxLevelBrowser::ShowOnlySelectedLevels(UBOOL bShowSelected)
{
	const FScopedBusyCursor BusyCursor;
	for ( WxLevelPane::TLevelWindowIterator It( LevelPane->LevelWindowIterator() ) ; It ; ++It )
	{
		WxLevelPane::WxLevelWindow* CurWindow = *It;
		if( CurWindow->GetLevelItem().IsLevel() )
		{
			if ( IsItemSelected( CurWindow->GetLevelItem() ) )
			{
				CurWindow->SetLevelVisibility( bShowSelected );
			}
			else
			{
				CurWindow->SetLevelVisibility( !bShowSelected );
			}
		}
	}
}

/**
 * Increases the mutex which controls whether Update() will be executed.  Should be called just before beginning an operation which might result in a
 * call to Update, if the calling code must manually call Update immediately afterwards.
 */
void WxLevelBrowser::DisableUpdate()
{
	SuppressUpdateMutex++;
}

/**
 * Decreases the mutex which controls whether Update() will be executed.
 */
void WxLevelBrowser::EnableUpdate()
{
	SuppressUpdateMutex--;
	check(SuppressUpdateMutex>=0);
}

namespace LevelBrowser {

/**
 * Shifts the specified level up or down in the level ordering, as determined by the bUp flag.
 *
 * @param	InLevel		Level to shift.
 * @param	bUp			TRUE to shift level up one position, FALSe to shife level down one position.
 * @return				TRUE if the shift occurred, FALSE if the shift did not occur.
 */
static UBOOL ShiftLevel(const ULevel* InLevel, UBOOL bUp)
{
	UBOOL bResult = FALSE;
	if ( InLevel )
	{
		if ( FLevelUtils::IsLevelLocked(const_cast<ULevel*>(InLevel)) )
		{
			appMsgf(AMT_OK, TEXT("ShiftLevel: %s"), *LocalizeUnrealEd(TEXT("Error_OperationDisallowedOnLockedLevel")));
		}
		else
		{
			INT PrevFoundLevelIndex = -1;
			INT FoundLevelIndex = -1;
			INT PostFoundLevelIndex = -1;
			AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();
			for( INT LevelIndex = 0 ; LevelIndex < WorldInfo->StreamingLevels.Num() ; ++LevelIndex )
			{
				ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
				if( StreamingLevel && StreamingLevel->LoadedLevel )
				{
					if ( FoundLevelIndex > -1 )
					{
						// Mark the first valid index after the found level and stop searching.
						PostFoundLevelIndex = LevelIndex;
						break;
					}
					else
					{
						if ( StreamingLevel->LoadedLevel == InLevel )
						{
							// We've found the level.
							FoundLevelIndex = LevelIndex;
						}
						else
						{
							// Mark this level as being the index before the found level.
							PrevFoundLevelIndex = LevelIndex;
						}
					}
				}
			}

			// If we found the level . . .
			if ( FoundLevelIndex > -1 )
			{
				// Check if we found a destination index to swap it to.
				const INT DestIndex = bUp ? PrevFoundLevelIndex : PostFoundLevelIndex;
				const UBOOL bFoundPrePost = DestIndex > -1;
				if ( bFoundPrePost )
				{
					// Swap the level into position.
					const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("ShiftLevelInLevelBrowser")) );
					WorldInfo->Modify();
					WorldInfo->StreamingLevels.SwapItems( FoundLevelIndex, DestIndex );
					WorldInfo->MarkPackageDirty();
					bResult = TRUE;
				}
			}
		}
	}
	return bResult;
}

} // namespace LevelBrowser

/**
 * Helper function for shifting level ordering; if a single level is selected, the
 * level is shifted up or down one position in the WorldInfo streaming level ordering,
 * depending on the value of the bUp argument.
 *
 * @param	bUp		TRUE to shift level up one position, FALSe to shift level down one position.
 */
void WxLevelBrowser::ShiftSingleSelectedLevel(UBOOL bUp)
{
	// Shift a singly selected level the requested direction.
	FLevelBrowserItem* SingleSelectedLevel = GetSingleSelectedLevelItem();
	if( SingleSelectedLevel != NULL && SingleSelectedLevel->IsLevel() )
	{
		const UBOOL bShiftOccurred = LevelBrowser::ShiftLevel( SingleSelectedLevel->GetLevel(), bUp );

		// Redraw window contents if necessary.
		if ( bShiftOccurred )
		{
			RequestDelayedUpdate();
		}
	}
}

/**
 * Helper method to split/unsplit the level/filter panes as necessary.
 *
 * @param	bShow	If true, the filter pane should be shown, split horizontally above the level pane. If false,
 *					only the level pane will be shown.
 */
void WxLevelBrowser::SetFilterWindowVisibility( UBOOL bShow )
{
	check( FilterWindow );
	const UBOOL bAlreadySplit = LevelFilterSplitter->IsSplit();
	if ( bShow && !bAlreadySplit )
	{
		const INT InitialFilterHeight = Min( 200, FilterWindow->GetClientRect().GetHeight() );

		// Split Filter from Levels
		LevelFilterSplitter->SplitHorizontally( FilterWindow, LevelPane, InitialFilterHeight );
	}
	else if ( !bShow && bAlreadySplit )
	{
		// Hide the Filter Window and just show the level pane
		LevelFilterSplitter->Unsplit( FilterWindow );
	}

	// Save off state of the filter window
	GConfig->SetBool( TEXT("LevelBrowser"), TEXT("ShowFilterWindow"), bShow, GEditorUserSettingsIni );	
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Wx events.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Called when the user selects the "New Level" option from the level browser's file menu.
 */
void WxLevelBrowser::OnNewLevel(wxCommandEvent& In)
{
	EditorLevelUtils::CreateNewLevel( FALSE );
}

/**
 * Called when the user selects the "New Level From Selected Actors" option from the level browser's file menu.
 */
void WxLevelBrowser::OnNewLevelFromSelectedActors(wxCommandEvent& In)
{
	EditorLevelUtils::CreateNewLevel( TRUE );
}

/**
 * Called when the user selects the "Import Level" option from the level browser's file menu.
 */
void WxLevelBrowser::OnImportLevel(wxCommandEvent& In)
{
	ImportLevelsFromFile();
	// Update volume actor visibility for each viewport since we loaded a level which could
	// potentially contain volumes
	GUnrealEd->UpdateVolumeActorVisibility(NULL);
}

void WxLevelBrowser::OnRemoveLevelFromWorld(wxCommandEvent& In)
{
	// If we have dirty levels that can be removed from the world
	UBOOL bHaveDirtyLevels = FALSE;
	// Gather levels to remove
	TArray< ULevel* > LevelsToRemove;
	for ( TSelectedLevelItemIterator It = SelectedLevelItemIterator() ; It ; ++It )
	{
		if( It->IsLevel() )
		{
		    ULevel* CurLevel = It->GetLevel();
		    if( CurLevel != NULL )
		    {
			    if( CurLevel->GetOutermost()->IsDirty() && !FLevelUtils::IsLevelLocked( CurLevel ) )
			    {
				    // this level is dirty and can be removed from the world
				    bHaveDirtyLevels = TRUE;
			    }
			    LevelsToRemove.AddItem( CurLevel );
			}
		}
	}

	UBOOL bShouldRemoveLevels = TRUE;
	if( bHaveDirtyLevels )
	{
		// Warn the user that they are about to remove a dirty level from the world
		bShouldRemoveLevels = appMsgf( AMT_YesNo, *LocalizeUnrealEd( "LevelBrowser_RemovingDirtyLevelFromWorld" ) );
	}

	if( bShouldRemoveLevels)
	{
		// Disassociate selected levels from streaming volumes since the levels will be removed
		ClearStreamingLevelVolumes();

		// Unselect everything since all selected levels will be removed anyway
		DeselectAllLevels();

		if( GEditorModeTools().IsModeActive( EM_Landscape ) )
		{
			GEditorModeTools().ActivateMode(EM_Default);
		}

		// Remove each level!
		for( INT CurLevelIndex = 0; CurLevelIndex < LevelsToRemove.Num(); ++CurLevelIndex )
		{
			ULevel* CurLevel = LevelsToRemove( CurLevelIndex );

			EditorLevelUtils::RemoveLevelFromWorld( CurLevel );
		}
	}
}

void WxLevelBrowser::OnMakeLevelCurrent(wxCommandEvent& In)
{
	FLevelBrowserItem* SelectedLevelItem = GetSelectedLevelItem();
	if( SelectedLevelItem != NULL )
	{
		if( SelectedLevelItem->IsLevel() )
		{
			MakeLevelCurrent( SelectedLevelItem->GetLevel() );
		}
		else if( SelectedLevelItem->IsLevelGridVolume() )
		{
			MakeLevelGridVolumeCurrent( SelectedLevelItem->GetLevelGridVolume() );
		}
	}
}

void WxLevelBrowser::MoveActorsToThisLevel(wxCommandEvent& In)
{
	FLevelBrowserItem* SelectedLevelItem = GetSelectedLevelItem();
	if( SelectedLevelItem != NULL )
	{
		if( SelectedLevelItem->IsLevel() )
		{
			MakeLevelCurrent( SelectedLevelItem->GetLevel() );
		}
		else if( SelectedLevelItem->IsLevelGridVolume() )
		{
			MakeLevelGridVolumeCurrent( SelectedLevelItem->GetLevelGridVolume() );
		}

		GEditor->MoveSelectedActorsToCurrentLevel( TRUE );
	}
}

void WxLevelBrowser::OnMergeVisibleLevels(wxCommandEvent& In)
{
	// First check to see if any of the levels are currently hidden; if so, we'll display a prompt to the user
	// asking if hidden levels should be discarded or kept
	UBOOL bSomeLevelsHidden = FALSE;
	for( TArray<ULevelStreaming*>::TConstIterator LevelIter( GWorld->GetWorldInfo()->StreamingLevels ); LevelIter; ++LevelIter )
	{
		const ULevelStreaming* CurStreamingLevel = *LevelIter;
		if ( CurStreamingLevel && !( CurStreamingLevel->bShouldBeVisibleInEditor ) )
		{
			bSomeLevelsHidden = TRUE;
			break;
		}
	}
	
	UBOOL bDiscardHidden = FALSE;
	UBOOL bProceedWithMerge = TRUE;
	
	// If some of the levels were hidden, prompt the user for what to do with them
	if ( bSomeLevelsHidden )
	{
		enum MergeVisibleReturnCode
		{
			MVR_KeepHidden,
			MVR_DiscardHidden,
			MVR_CancelMerge
		};

		WxChoiceDialog DiscardHiddenLevelsPrompt( 
			LocalizeUnrealEd("MergeVisibleLevelsPromptMessage"), 
			LocalizeUnrealEd("MergeVisibleLevels"),
			WxChoiceDialogBase::Choice( MVR_KeepHidden, LocalizeUnrealEd("MergeVisibleLevelsPromptKeepHidden"), WxChoiceDialogBase::DCT_DefaultAffirmative ),
			WxChoiceDialogBase::Choice( MVR_DiscardHidden, LocalizeUnrealEd("MergeVisibleLevelsPromptDiscardHidden"), WxChoiceDialogBase::DCT_Regular ),
			WxChoiceDialogBase::Choice( MVR_CancelMerge, LocalizeUnrealEd("MergeVisibleLevelsPromptCancelMerge"), WxChoiceDialogBase::DCT_DefaultCancel ) );
		DiscardHiddenLevelsPrompt.ShowModal();
		bDiscardHidden = ( DiscardHiddenLevelsPrompt.GetChoice().ReturnCode == MVR_DiscardHidden );
		bProceedWithMerge = ( DiscardHiddenLevelsPrompt.GetChoice().ReturnCode != MVR_CancelMerge );
	}

	if( bProceedWithMerge )
	{
		MergeVisibleLevels( bDiscardHidden );
	}
}


void WxLevelBrowser::OnUpdateLevelsForAllActors( wxCommandEvent& In )
{
	// Never move actors around while Matinee is open as their locations may be temporary
	if( !GEditorModeTools().IsModeActive(EM_InterpEdit) )
	{
		EditorLevelUtils::UpdateLevelsForAllActors();
	}
}


void WxLevelBrowser::OnAutoUpdateLevelsForChangedActors( wxCommandEvent& In )
{
	GEditor->AccessUserSettings().bUpdateActorsInGridLevelsImmediately = In.IsChecked();
	GEditor->SaveUserSettings();
}


void WxLevelBrowser::UpdateUI_AutoUpdateLevelsForChangedActors( wxUpdateUIEvent& In )
{
	In.Check( GEditor->GetUserSettings().bUpdateActorsInGridLevelsImmediately );
}


void WxLevelBrowser::OnSaveSelectedLevels(wxCommandEvent& In)
{
	// NOTE: We'll build a list of levels to save here.  We don't want to use the SelectedLevels member
	//   since that list will be reset when Serialize is called
	TArray< ULevel* > LevelsToSave;
	for ( TSelectedLevelItemIterator It = SelectedLevelItemIterator() ; It ; ++It )
	{
		if( It->IsLevel() )
		{
			ULevel* Level = It->GetLevel();
			LevelsToSave.AddItem( Level );
		}
	}

	TArray< UPackage* > PackagesNotNeedingCheckout;
	// Prompt the user to check out the levels from source control before saving
	if ( FEditorFileUtils::PromptToCheckoutLevels( FALSE, &LevelsToSave, &PackagesNotNeedingCheckout ) )
	{
		for ( TArray<ULevel*>::TIterator It( LevelsToSave ); It ; ++It )
		{
			ULevel* Level = *It;
			FEditorFileUtils::SaveLevel( Level );
		}
	}
	else if ( PackagesNotNeedingCheckout.Num() > 0 )
	{
		// The user cancelled the checkout dialog but some packages didnt need to be checked out in order to save
		// For each selected level if the package its in didnt need to be saved, save the level!
		for ( TArray<ULevel*>::TIterator It( LevelsToSave ) ; It ; ++It )
		{
			ULevel* Level = *It;
			if( PackagesNotNeedingCheckout.ContainsItem( Level->GetOutermost() ) )
			{
				FEditorFileUtils::SaveLevel( Level );
			}
		}
	}
	
	// Select the levels that were saved successfully
	DeselectAllLevels();
	for ( TArray<ULevel*>::TIterator It( LevelsToSave ) ; It ; ++It )
	{
		ULevel* Level = *It;
		SelectLevelItem( FLevelBrowserItem( Level ) );
	}

	RequestUpdate();
}

void WxLevelBrowser::ShowSelectedLevelsInSceneManager(wxCommandEvent& In)
{
	WxSceneManager* SceneManager = GUnrealEd->GetBrowser<WxSceneManager>( TEXT("SceneManager") );
	if ( SceneManager )
	{
		TArray< ULevel* > LevelsToShow;
		for( INT LevelIdx = 0; LevelIdx < SelectedLevelItems.Num(); ++LevelIdx )
		{
			if( SelectedLevelItems( LevelIdx ).IsLevel() )
			{
				LevelsToShow.AddItem( SelectedLevelItems( LevelIdx ).GetLevel() );
			}
		}

		SceneManager->SetActiveLevels( LevelsToShow );
		GUnrealEd->GetBrowserManager()->ShowWindow( SceneManager->GetDockID(), TRUE );
	}
}

void WxLevelBrowser::ShowSelectedLevelsInContentBrowser(wxCommandEvent& In)
{
#if WITH_MANAGED_CODE
	if( FContentBrowser::IsInitialized() && SelectedLevelItems.Num() > 0 )
	{
		FContentBrowser &CBInstance = FContentBrowser::GetActiveInstance();
		// Request to focus the active ContentBrowser
		GCallbackEvent->Send( FCallbackEventParameters (NULL, CALLBACK_RefreshContentBrowser, CBR_FocusBrowser|CBR_EmptySelection) );

		// Build a list of packages from the level list
		TArray< UPackage *> SelectedPackages;
		for( INT LevelIdx = 0; LevelIdx < SelectedLevelItems.Num(); ++LevelIdx )
		{
			if( SelectedLevelItems( LevelIdx ).IsLevel() )
			{
				// The ULevel's outermost is the package it's in
				SelectedPackages.AddItem( SelectedLevelItems( LevelIdx ).GetLevel()->GetOutermost() );
			}
		}

		// Select the levels in the content browser
		CBInstance.SyncToPackages( SelectedPackages );
	}
#endif
}

/**
 * Selects all actors in the selected levels.
 */
void WxLevelBrowser::OnSelectAllActors(wxCommandEvent& In)
{
	SelectActorsOfSelectedLevelItems();
}

void WxLevelBrowser::ShiftLevelUp(wxCommandEvent& In)
{
	ShiftSingleSelectedLevel( TRUE );
}

void WxLevelBrowser::ShiftLevelDown(wxCommandEvent& In)
{
	ShiftSingleSelectedLevel( FALSE );
}

/** Event to bring up appMsgf with details on how to add filters */
void WxLevelBrowser::OnFilterStringsDetail(wxCommandEvent& In)
{
	appMsgf( AMT_OK, *LocalizeUnrealEd("LevelBrowser_KeywordFilter_NoFilterStringsDetail") );
}

/** Event to toggle the visiblity of the filter window */
void WxLevelBrowser::OnToggleFilterWindow(wxCommandEvent& In)
{
	SetFilterWindowVisibility( !LevelFilterSplitter->IsSplit() );
}

/**
 * Called when the user checks or unchecks the option to display level size data
 *
 * @param	In	Event automatically generated by wxWidgets when the user checks/unchecks the option
 */
void WxLevelBrowser::OnShowSizeData(wxCommandEvent& In)
{
	bShouldDisplaySizeData = In.IsChecked();
	RequestUpdate();
}

/**
 * Shows the selected level in a property window.
 */
void WxLevelBrowser::OnProperties(wxCommandEvent& In)
{
	ShowPropertiesForSelectedLevelItems();
}

void WxLevelBrowser::OnShowOnlySelectedLevels(wxCommandEvent& In)
{
	ShowOnlySelectedLevels( TRUE );
}

void WxLevelBrowser::OnShowOnlyUnselectedLevels(wxCommandEvent& In)
{
	ShowOnlySelectedLevels( FALSE );
}

void WxLevelBrowser::OnShowSelectedLevels(wxCommandEvent& In)
{
	SetSelectedLevelVisibility( TRUE );
}

void WxLevelBrowser::OnHideSelectedLevels(wxCommandEvent& In)
{
	SetSelectedLevelVisibility( FALSE );
}

void WxLevelBrowser::OnShowAllLevels(wxCommandEvent& In)
{
	LevelPane->SetAllLevelVisibility( TRUE );
}

void WxLevelBrowser::OnHideAllLevels(wxCommandEvent& In)
{
	LevelPane->SetAllLevelVisibility( FALSE );
}

/**
 * Called when the user elects to lock all of the currently selected levels.
 *
 * @param	In	Event generated by wxWidget when the appropriate menu item is selected
 */
void WxLevelBrowser::OnLockSelectedLevels(wxCommandEvent& In)
{
	// Attempt to lock each selected level
	for ( TSelectedLevelItemIterator SelectedLevelIter( SelectedLevelItems ); SelectedLevelIter; ++SelectedLevelIter )
	{
		if( SelectedLevelIter->IsLevel() )
		{
			LevelPane->SetLevelLocked( SelectedLevelIter->GetLevel(), TRUE );
		}
	}
}

/**
 * Called when the user elects to unlock all of the currently selected levels.
 *
 * @param	In	Event generated by wxWidget when the appropriate menu item is selected
 */
void WxLevelBrowser::OnUnlockSelectedLevels(wxCommandEvent& In)
{
	// Attempt to unlock each selected level
	for ( TSelectedLevelItemIterator SelectedLevelIter( SelectedLevelItems ); SelectedLevelIter; ++SelectedLevelIter )
	{
		if( SelectedLevelIter->IsLevel() )
		{
			LevelPane->SetLevelLocked( SelectedLevelIter->GetLevel(), FALSE );
		}
	}
}

/**
 * Called when the user elects to lock all of the levels.
 *
 * @param	In	Event generated by wxWidget when the appropriate menu item is selected
 */
void WxLevelBrowser::OnLockAllLevels(wxCommandEvent& In)
{
	LevelPane->SetAllLevelLocked( TRUE );
}

/**
 * Called when the user elects to unlock all of the levels.
 *
 * @param	In	Event generated by wxWidget when the appropriate menu item is selected
 */
void WxLevelBrowser::OnUnlockAllLevels(wxCommandEvent& In)
{
	LevelPane->SetAllLevelLocked( FALSE );
}

void WxLevelBrowser::OnSelectAllLevels(wxCommandEvent& In)
{
	SelectAllLevels();
}

void WxLevelBrowser::OnDeselectAllLevels(wxCommandEvent& In)
{
	DeselectAllLevels();
}

void WxLevelBrowser::OnInvertSelection(wxCommandEvent& In)
{
	InvertLevelSelection();
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Iterates over selected levels and sets streaming volume associations.
void WxLevelBrowser::SetStreamingLevelVolumes(const TArray<ALevelStreamingVolume*>& LevelStreamingVolumes)
{
	ClearStreamingLevelVolumes();
	AddStreamingLevelVolumes( LevelStreamingVolumes );
}

// Iterates over selected levels and adds streaming volume associations.
void WxLevelBrowser::AddStreamingLevelVolumes(const TArray<ALevelStreamingVolume*>& LevelStreamingVolumes)
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("AddStreamingVolumes")) );

	for ( TSelectedLevelItemIterator It = SelectedLevelItemIterator() ; It ; ++It )
	{
		if( It->IsLevel() )
		{
			ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel( It->GetLevel() ); 
			if ( StreamingLevel )
			{
				StreamingLevel->Modify();
				for ( INT i = 0 ; i < LevelStreamingVolumes.Num() ; ++i )
				{
					ALevelStreamingVolume* LevelStreamingVolume = LevelStreamingVolumes(i);

					// Associate the level to the volume.
					LevelStreamingVolume->Modify();
					LevelStreamingVolume->StreamingLevels.AddUniqueItem( StreamingLevel );

					// Associate the volume to the level.
					StreamingLevel->EditorStreamingVolumes.AddUniqueItem( LevelStreamingVolume );
				}
			}
		}
	}
}

// Iterates over selected levels and clears all streaming volume associations.
void WxLevelBrowser::ClearStreamingLevelVolumes()
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("ClearStreamingVolumes")) );

	for ( TSelectedLevelItemIterator It = SelectedLevelItemIterator() ; It ; ++It )
	{
		if( It->IsLevel() )
		{
			ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel( It->GetLevel() ); 
			if ( StreamingLevel )
			{
				StreamingLevel->Modify();

				// Disassociate the level from the volume.
				for ( INT i = 0 ; i < StreamingLevel->EditorStreamingVolumes.Num() ; ++i )
				{
					ALevelStreamingVolume* LevelStreamingVolume = StreamingLevel->EditorStreamingVolumes(i);
					if ( LevelStreamingVolume )
					{
						LevelStreamingVolume->Modify();
						LevelStreamingVolume->StreamingLevels.RemoveItem( StreamingLevel );
					}
				}

				// Disassociate the volumes from the level.
				StreamingLevel->EditorStreamingVolumes.Empty();
			}
		}
	}
}

// Iterates over selected levels and selects all streaming volume associations.
void WxLevelBrowser::SelectStreamingLevelVolumes()
{
	// Iterate over selected levels and make a list of volumes to select.
	TArray<ALevelStreamingVolume*> LevelStreamingVolumesToSelect;
	for ( TSelectedLevelItemIterator It = SelectedLevelItemIterator() ; It ; ++It )
	{
		if( It->IsLevel() )
		{
			ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel( It->GetLevel() ); 
			if ( StreamingLevel )
			{
				for ( INT i = 0 ; i < StreamingLevel->EditorStreamingVolumes.Num() ; ++i )
				{
					ALevelStreamingVolume* LevelStreamingVolume = StreamingLevel->EditorStreamingVolumes(i);
					if ( LevelStreamingVolume )
					{
						LevelStreamingVolumesToSelect.AddItem( LevelStreamingVolume );
					}
				}
			}
		}
	}

	// Select the volumes.
	const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("SelectAssociatedStreamingVolumes")) );
	GEditor->GetSelectedActors()->Modify();
	GEditor->SelectNone( FALSE, TRUE );

	for ( INT i = 0 ; i < LevelStreamingVolumesToSelect.Num() ; ++i )
	{
		ALevelStreamingVolume* LevelStreamingVolume = LevelStreamingVolumesToSelect(i);
		GEditor->SelectActor( LevelStreamingVolume, TRUE, NULL, FALSE, TRUE );
	}


	GEditor->NoteSelectionChange();
}

static void AssembleSelectedLevelStreamingVolumes(TArray<ALevelStreamingVolume*>& LevelStreamingVolumes)
{
	LevelStreamingVolumes.Empty();
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		ALevelStreamingVolume* StreamingVolume = Cast<ALevelStreamingVolume>( *It );
		if ( StreamingVolume && StreamingVolume->GetLevel() == GWorld->PersistentLevel )
		{
			LevelStreamingVolumes.AddItem( StreamingVolume );
		}
	}

	if ( LevelStreamingVolumes.Num() == 0 )
	{
		appMsgf( AMT_OK, TEXT("Streaming volume assignment failed.  No LevelStreamingVolumes in the persistent level were selected.") );
	}
}
void WxLevelBrowser::OnSetStreamingLevelVolumes(wxCommandEvent& In)
{
	TArray<ALevelStreamingVolume*> LevelStreamingVolumes;
	AssembleSelectedLevelStreamingVolumes( LevelStreamingVolumes );
	if ( LevelStreamingVolumes.Num() > 0 )
	{
		SetStreamingLevelVolumes( LevelStreamingVolumes );
		UpdateLevelPropertyWindow();
	}
}

void WxLevelBrowser::OnAddStreamingLevelVolumes(wxCommandEvent& In)
{
	TArray<ALevelStreamingVolume*> LevelStreamingVolumes;
	AssembleSelectedLevelStreamingVolumes( LevelStreamingVolumes );
	if ( LevelStreamingVolumes.Num() > 0 )
	{
		AddStreamingLevelVolumes( LevelStreamingVolumes );
		UpdateLevelPropertyWindow();
	}
}

void WxLevelBrowser::OnClearStreamingLevelVolumes(wxCommandEvent& In)
{
	// Prompt the user to see if they definitely want to clear streaming level volumes
	if ( appMsgf( AMT_YesNo, *LocalizeUnrealEd("ConfirmClearStreamingVolumesDlgMsg") ) )
	{
		ClearStreamingLevelVolumes();
		UpdateLevelPropertyWindow();
	}
}

void WxLevelBrowser::OnSelectStreamingLevelVolumes(wxCommandEvent& In)
{
	SelectStreamingLevelVolumes();
	UpdateLevelPropertyWindow();
}

void WxLevelBrowser::OnAssignKeywords(wxCommandEvent& In)
{
	//Create Key Dialog
	WxDlgCheckBoxList<FString> FilterDialog;

	FilterDialog.AddButton(ART_Yes, *LocalizeUnrealEd("LevelBrowser_KeywordDialog_SaveKeywords"), *LocalizeUnrealEd("LevelBrowser_KeywordDialog_SaveKeywords_Tooltop"));
	FilterDialog.AddButton(ART_Cancel, *LocalizeUnrealEd("LevelBrowser_KeywordDialog_Cancel"), *LocalizeUnrealEd("LevelBrowser_KeywordDialog_Cancel_Tooltip"));

	FConfigSection* LevelKeywords = GConfig->GetSectionPrivate( TEXT("LevelBrowser.Keywords"), 0, 1, GEditorIni);
	UBOOL bShowDialog = FALSE;
	if (LevelKeywords)
	{
		for( FConfigSectionMap::TIterator It(*LevelKeywords); It; ++It )
		{
			FString CurrentKeyword = It.Value();

			//assume both all in and all out
			UBOOL AllHaveKeyword = TRUE;
			UBOOL AllDoNotHaveKeyword = TRUE;
			for (INT LevelIndex = 0; LevelIndex < SelectedLevelItems.Num(); ++LevelIndex)
			{
				if( SelectedLevelItems( LevelIndex ).IsLevel() )
				{
					ULevel* TestLevel = SelectedLevelItems(LevelIndex).GetLevel();
					check(TestLevel);
					ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel(TestLevel);
					//PMAP
					if (!StreamingLevel)
					{
						continue;
					}
					bShowDialog = TRUE;
					if (StreamingLevel->Keywords.ContainsItem(CurrentKeyword))
					{
						AllDoNotHaveKeyword = FALSE;
					}
					else
					{
						AllHaveKeyword = FALSE;
					}
				}
			}

			//support tri-state level
			INT CheckState;
			if (AllHaveKeyword)
			{
				CheckState = wxCHK_CHECKED;
			}
			else if (AllDoNotHaveKeyword)
			{
				CheckState = wxCHK_UNCHECKED;
			}
			else
			{
				CheckState = wxCHK_UNDETERMINED;
			}

			FilterDialog.AddCheck(CurrentKeyword, CurrentKeyword, CheckState);
		}
	}

	if (bShowDialog)
	{
		FIntPoint WindowSize(400, 400);
		INT ActionID = FilterDialog.ShowDialog(*LocalizeUnrealEd("LevelBrowser_KeywordDialog_Title"), *LocalizeUnrealEd("LevelBrowser_KeywordDialog_Message"), WindowSize, TEXT("LevelKeywords"));

		//if we are commiting the keywords
		if (ActionID == ART_Yes)
		{
			//get "turn off" items
			TArray<FString> AllOffStrings;
			FilterDialog.GetResults(AllOffStrings, wxCHK_UNCHECKED);

			//get "turn on" items
			TArray<FString> AllOnStrings;
			FilterDialog.GetResults(AllOnStrings, wxCHK_CHECKED);

			//for every level
			for (INT LevelIndex = 0; LevelIndex < SelectedLevelItems.Num(); ++LevelIndex)
			{
				if( SelectedLevelItems( LevelIndex ).IsLevel() )
				{
					ULevel* TestLevel = SelectedLevelItems(LevelIndex).GetLevel();
					check(TestLevel);
					ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel(TestLevel);
					//PMAP
					if (!StreamingLevel)
					{
						continue;
					}

					//remove all "off" keywords
					for (int OffIndex = 0; OffIndex < AllOffStrings.Num(); ++OffIndex)
					{
						StreamingLevel->Keywords.RemoveItem(AllOffStrings(OffIndex));
					}
					//add on items
					for (int OnIndex = 0; OnIndex < AllOnStrings.Num(); ++OnIndex)
					{
						StreamingLevel->Keywords.AddUniqueItem(AllOnStrings(OnIndex));
					}
				}
			}
		}

		LayoutLevelWindows();
	}
}

void WxLevelBrowser::OnMouseWheel(wxMouseEvent& In)
{
	//The LevelPane rarely has focus which is why it rarely receives mouse wheel events. 
	//Send mouse wheel events directly to the level pane if the level browser has focus.
	LevelPane->GetEventHandler()->ProcessEvent(In);
}


/**
 * WxBrowser: Loads the window state for this dockable window
 *
 * @param bInInitiallyHidden True if the window should be hidden if we have no saved state
 */
void WxLevelBrowser::LoadWindowState( const UBOOL bInInitiallyHidden )
{
	WxBrowser::LoadWindowState( bInInitiallyHidden );

	GetFrame()->Connect(
		IDM_LB_AutoUpdateLevelsForChangedActors,	//	id
		wxEVT_UPDATE_UI,					//	event type
		wxUpdateUIEventHandler(WxLevelBrowser::UpdateUI_AutoUpdateLevelsForChangedActors),
		(wxObject*)NULL,
		this
		);
}

/**
 * Used to perform any per-frame actions
 *
 * @param DeltaSeconds Time since the last call
 */
void WxLevelBrowser::Tick( FLOAT DeltaSeconds )
{
	// Process deferred actors that have recently moved/changed that we may need to move to a new
	// level based on their level grid volume membership
	ProcessUpdatesToDeferredActors();

	if(bDoTickUpdate == TRUE)
	{
		bDoTickUpdate = FALSE;

		// Copy off the current selection since a call to Update may wipe it
		TArray<FLevelBrowserItem> LevelsToBeReselected = SelectedLevelItems;

		Update();
		
		// The update may wipe out the user selection.  If it was wiped we Re-Select levels here
		if(SelectedLevelItems.Num() == 0)
		{	
			for ( INT LevelIndex = 0 ; LevelIndex < LevelsToBeReselected.Num() ; ++LevelIndex )
			{
				FLevelBrowserItem CurLevelItem = LevelsToBeReselected( LevelIndex );
				SelectedLevelItems.AddItem( CurLevelItem );
			}
		}

		RequestUpdate();
	}
}


