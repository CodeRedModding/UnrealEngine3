/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "LayerBrowser.h"
#include "Controls.h"
#include "ScopedTransaction.h"
#include "LayerUtils.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Utility methods.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_COMPARE_CONSTREF( FString, LayerBrowser, { return appStricmp(*A,*B); } );

/**
 * @return		TRUE if the actor can be considered by the layer browser, FALSE otherwise.
 */
static FORCEINLINE UBOOL IsValid(const AActor* Actor)
{
	return ( !Actor->IsABuilderBrush() && ( Actor->bHiddenEd == FALSE && Actor->GetClass()->GetDefaultActor()->bHiddenEd == FALSE ) );
}

/**
 * @return		The number of selected actors.
 */
static INT NumActorsSelected()
{
	INT NumSelected = 0;

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		++NumSelected;
	}

	return NumSelected;
}

/**
 * Iterates over the specified wxCheckListBox object and extracts entries to the output string list.
 */
static void GetAllLayerNames(const wxCheckListBox* LayerList, TArray<FString>& OutLayerNames)
{
	const INT NumEntries = LayerList->GetCount();

	// Assemble a list of layer names.
	for( INT Index = 0 ; Index < NumEntries ; ++Index )
	{
		OutLayerNames.AddItem( LayerList->GetString(Index).c_str() );
	}
}

/**
 * Iterates over the specified wxListBox object and extracts selected entries to the output string list.
 */
static void GetSelectedNames(const wxListBox* InList, TArray<FString>& OutSelectedNames)
{
	wxArrayInt Selections;
	const INT NumSelections = InList->GetSelections( Selections );

	// Assemble a list of selected layer names.
	for( INT SelectionIndex = 0 ; SelectionIndex < NumSelections ; ++SelectionIndex )
	{
		OutSelectedNames.AddItem( InList->GetString( Selections[SelectionIndex] ).c_str() );
	}
}

/** Iterates over the specified wxListView object and extracts selected entries to the output string list. */
static void GetSelectedActors(const wxListView* InList, TArray<AActor*>& OutSelectedActors)
{
	// Assemble a list of selected layer names.
	INT SelectionIndex = InList->GetFirstSelected();
	while ( SelectionIndex != -1 )
	{
		OutSelectedActors.AddItem( (AActor*)InList->GetItemData(SelectionIndex) );
		SelectionIndex = InList->GetNextSelected( SelectionIndex );
	}
}

/**
 * Iterates over the specified wxCheckListBox object and extracts selected entry indices to the output index list.
 */
static void GetSelectedLayerIndices(const wxCheckListBox* LayerList, TArray<INT>& OutSelectedLayerIndices)
{
	wxArrayInt Selections;
	const INT NumSelections = LayerList->GetSelections( Selections );

	// Assemble a list of selected layer indices.
	for( INT SelectionIndex = 0 ; SelectionIndex < NumSelections ; ++SelectionIndex )
	{
		OutSelectedLayerIndices.AddItem( static_cast<INT>( Selections[SelectionIndex] ) );
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxDlgLayer
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Class for the "New Layer"/"Rename Layer" dialog
 */
class WxDlgLayer : public wxDialog
{
public:
	WxDlgLayer()
	{
		const bool bSuccess = wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_LAYER") );
		check( bSuccess );

		NameEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_NAME" ) );
		check( NameEdit != NULL );

		FWindowUtil::LoadPosSize( TEXT("DlgLayer"), this );
		FLocalizeWindow( this );
	}

	~WxDlgLayer()
	{
		FWindowUtil::SavePosSize( TEXT("DlgLayer"), this );
	}

	const FString& GetObjectName() const
	{
		return Name;
	}

	/**
	 * Displays the dialog.
	 *
	 * @param		bInNew		If TRUE, behave has a "New Layer" dialog; if FALSE, "Rename Layer"
	 * @param		InName		The string with which to initialize the edit box.
	 */
	int ShowModal(UBOOL bInNew, const FString& InName)
	{
		if( bInNew )
		{
			SetTitle( *LocalizeUnrealEd("NewLayer") );
		}
		else
		{
			SetTitle( *LocalizeUnrealEd("RenameLayer") );
		}

		Name = InName;
		NameEdit->SetValue( *Name );

		return wxDialog::ShowModal();
	}
private:
	using wxDialog::ShowModal;		// Hide parent implementation
public:

private:
	FString Name;
	wxTextCtrl *NameEdit;

	void OnOK(wxCommandEvent& In)
	{
		Name = NameEdit->GetValue();
		wxDialog::AcceptAndClose();
	}

	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(WxDlgLayer, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgLayer::OnOK )
END_EVENT_TABLE()

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxLayerMenu
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class WxLayerMenu : public wxMenu
{
public:
	WxLayerMenu()
	{
		Append( IDMN_LB_NEW_LAYER, *LocalizeUnrealEd("NewE"), *LocalizeUnrealEd("ToolTip_140") );
		Append( IDMN_LB_RENAME_LAYER, *LocalizeUnrealEd("RenameE"), *LocalizeUnrealEd("ToolTip_141") );
		Append( IDMN_LB_DELETE_LAYER, *LocalizeUnrealEd("Delete"), *LocalizeUnrealEd("ToolTip_142") );
		AppendSeparator();
		Append( IDMN_LB_ADD_TO_LAYER, *LocalizeUnrealEd("AddSelectedActorsLayer"), *LocalizeUnrealEd("ToolTip_143") );
		Append( IDMN_LB_DELETE_FROM_LAYER, *LocalizeUnrealEd("DeleteSelectedActorsLayer"), *LocalizeUnrealEd("ToolTip_144") );
		AppendSeparator();
		Append( IDMN_LB_SELECT, *LocalizeUnrealEd("SelectActors"), *LocalizeUnrealEd("ToolTip_145") );
		Append( IDMN_LB_DESELECT, *LocalizeUnrealEd("DeselectActors"), *LocalizeUnrealEd("ToolTip_146") );
		AppendSeparator();
		Append( IDMN_LB_ALLLAYERSVISIBLE, *LocalizeUnrealEd("MakeAllLayersVisible"), *LocalizeUnrealEd("ToolTip_146") );
	}

	~WxLayerMenu(){}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxMBLayerBrowser
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The Layer browser menu bar.
 */
class WxMBLayerBrowser : public wxMenuBar
{
public:
	WxMBLayerBrowser()
	{
		// Layer menu
		WxLayerMenu* LayerMenu = new WxLayerMenu();

		// View menu
		wxMenu* ViewMenu = new wxMenu();	
		ViewMenu->Append( IDM_RefreshBrowser, *LocalizeUnrealEd("RefreshWithHotkey"), *LocalizeUnrealEd("ToolTip_147") );

		Append( LayerMenu, *LocalizeUnrealEd("Layer") );
		Append( ViewMenu, *LocalizeUnrealEd("View") );

		WxBrowser::AddDockingMenu( this );
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxLayerList
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The Layer listbox.
 */
class WxLayerList : public wxCheckListBox
{
public:
	WxLayerList(WxLayerBrowser* Browser, wxSplitterWindow* Parent, wxWindowID Id)
		:
		wxCheckListBox(Parent, Id, wxDefaultPosition, wxDefaultSize, 0, wxLB_EXTENDED ),
		ParentBrowser(Browser)
	{
	}

	void OnRightButtonDown( wxMouseEvent& In )
	{
		ParentBrowser->OnRightButtonDown(In);
	}

	DECLARE_EVENT_TABLE();

private:
	WxLayerBrowser* ParentBrowser;
};

BEGIN_EVENT_TABLE( WxLayerList, wxCheckListBox )
	EVT_RIGHT_DOWN( WxLayerList::OnRightButtonDown )
END_EVENT_TABLE()

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxLayerBrowser
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE( WxLayerBrowser, WxBrowser )
	EVT_RIGHT_DOWN( WxLayerBrowser::OnRightButtonDown )
	EVT_MENU( IDM_RefreshBrowser, WxLayerBrowser::OnRefresh )
	EVT_MENU( IDMN_LB_NEW_LAYER, WxLayerBrowser::OnNewLayer )
	EVT_MENU( IDMN_LB_DELETE_LAYER, WxLayerBrowser::OnDeleteLayer )
	EVT_MENU( IDMN_LB_SELECT, WxLayerBrowser::OnSelect )
	EVT_MENU( IDMN_LB_DESELECT, WxLayerBrowser::OnDeselect )
	EVT_MENU( IDMN_LB_ALLLAYERSVISIBLE, WxLayerBrowser::OnAllLayersVisible )
	EVT_MENU( IDMN_LB_RENAME_LAYER, WxLayerBrowser::OnRename )
	EVT_MENU( IDMN_LB_ADD_TO_LAYER, WxLayerBrowser::OnAddToLayer )
	EVT_MENU( IDMN_LB_DELETE_FROM_LAYER, WxLayerBrowser::OnDeleteSelectedActorsFromLayers )
	EVT_CHECKLISTBOX( ID_LB_Layers, WxLayerBrowser::OnToggled )
	EVT_LISTBOX( ID_LB_Layers, WxLayerBrowser::OnLayerSelectionChange )
	EVT_LISTBOX_DCLICK( ID_LB_Layers, WxLayerBrowser::OnDoubleClick )
	EVT_LIST_COL_CLICK( ID_LB_ActorList, WxLayerBrowser::OnActorListColumnClicked )
	EVT_LIST_ITEM_SELECTED( ID_LB_ActorList, WxLayerBrowser::OnActorListItemClicked )
	EVT_LIST_ITEM_ACTIVATED( ID_LB_ActorList, WxLayerBrowser::OnActorListItemDblClicked )
	EVT_SIZE( WxLayerBrowser::OnSize )
END_EVENT_TABLE()

WxLayerBrowser::WxLayerBrowser()
	:	bAllowToggling( TRUE )
	,	bUpdateOnActivated( FALSE )
{
	// Register our callbacks
	GCallbackEvent->Register(CALLBACK_LayerChange,this);
	GCallbackEvent->Register(CALLBACK_RefreshEditor_LayerBrowser,this);
	// Tap into the same updates as the level browser for e.g. level visibility, actor adding, etc.
	GCallbackEvent->Register(CALLBACK_RefreshEditor_LevelBrowser,this);
	GCallbackEvent->Register(CALLBACK_Undo,this);
}

/** Clears references the actor list and references to any actors. */
void WxLayerBrowser::ClearActorReferences()
{
	ActorList->DeleteAllItems();
	ReferencedActors.Empty();
}

/** Serialize actor references held by the browser. */
void WxLayerBrowser::Serialize(FArchive& Ar)
{
	Ar << ReferencedActors;
}

void WxLayerBrowser::Send(ECallbackEventType Event)
{
	// We've received a callback that we registered for.  We're expected to refresh now.

	// Make sure we're not still referencing actors, since we may have received this event because
	// the current map is being cleared out (to load or create a new one, etc.)
	ClearActorReferences();

	// Refresh the Layer Browser GUI
	Update();
}

/**
 * Forwards the call to our base class to create the window relationship.
 * Creates any internally used windows after that
 *
 * @param DockID the unique id to associate with this dockable window
 * @param FriendlyName the friendly name to assign to this window
 * @param Parent the parent of this window (should be a Notebook)
 */
void WxLayerBrowser::Create(INT DockID,const TCHAR* FriendlyName,wxWindow* Parent)
{
	WxBrowser::Create(DockID,FriendlyName,Parent);

	wxBoxSizer* MainSizer = new wxBoxSizer(wxVERTICAL);
	{
		SplitterWindow = new wxSplitterWindow( this, -1, wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE | wxSP_BORDER );
		{
			LayerList = new WxLayerList(this, SplitterWindow, ID_LB_Layers);
			//LayerList = new wxCheckListBox( SplitterWindow, ID_LB_Layers, wxDefaultPosition, wxDefaultSize, 0, wxLB_EXTENDED );
			ActorList = new WxListView( SplitterWindow, ID_LB_ActorList, wxDefaultPosition, wxDefaultSize, wxLC_REPORT );
			{
				ActorList->InsertColumn( 0, *LocalizeUnrealEd("Class"), wxLIST_FORMAT_LEFT, 160 );
				ActorList->InsertColumn( 1, *LocalizeUnrealEd("Actors"), wxLIST_FORMAT_LEFT, 260 );
			}
		}
		SplitterWindow->SplitVertically( LayerList, ActorList, 100 );
		SplitterWindow->SetSashGravity( 0.1 );
		SplitterWindow->SetMinimumPaneSize( 100 );
	}
	MainSizer->Add( SplitterWindow, 1, wxEXPAND );

	MenuBar = new WxMBLayerBrowser();

	SetAutoLayout( true );
	SetSizer( MainSizer );
	MainSizer->Fit(this);
}

void WxLayerBrowser::Update()
{
	if ( IsShownOnScreen() && !GIsPlayInEditorWorld && GWorld )
	{
		BeginUpdate();

		// Loops through all the actors in the world and assembles a list of unique layer names.
		// Actors can belong to multiple layers by separating the layer names with commas.
		TArray<FString> UniqueLayers;
		FLayerUtils::GetAllLayers(UniqueLayers);

		// Assemble the set of visible layers.
		TArray<FString> LayerArray;
		GWorld->GetWorldInfo()->VisibleLayers.ParseIntoArray( &LayerArray, TEXT(","), FALSE );

		// Assemble a list of selected layer names.
		TArray<FString> SelectedLayerNames;
		GetSelectedNames( LayerList, SelectedLayerNames );

		// Add the list of unique layer names to the layer listbox.
		bAllowToggling = FALSE;
		LayerList->Clear();

		Sort<USE_COMPARE_CONSTREF(FString,LayerBrowser)>( UniqueLayers.GetTypedData(), UniqueLayers.Num() );
		for( INT LayerIndex = 0 ; LayerIndex < UniqueLayers.Num() ; ++LayerIndex )
		{
			const FString& LayerName = UniqueLayers(LayerIndex);
			const INT NewLayerIndex = LayerList->Append( *LayerName );

			// Restore the layer's selection status.
			const UBOOL bWasSelected = SelectedLayerNames.ContainsItem( *LayerName );
			if ( bWasSelected )
			{
				LayerList->SetSelection( NewLayerIndex );
			}

			// Set the layers visibility status.
			const UBOOL bVisible = LayerArray.ContainsItem( *LayerName );
			LayerList->Check( LayerIndex, bVisible ? true : false );
		}
		bAllowToggling = TRUE;

		UpdateActorVisibility();
		PopulateActorList();
		bUpdateOnActivated = FALSE;

		EndUpdate();
	}
	else
	{
		bUpdateOnActivated = TRUE;
	}
}

void WxLayerBrowser::Activated()
{
	WxBrowser::Activated();
	if ( bUpdateOnActivated )
	{
		Update();
	}
}

/**
 * Loops through all actors in the world and updates their visibility based on which layers are checked.
 */
void WxLayerBrowser::UpdateActorVisibility()
{
	FString VisibleLayersList;
	TArray<FString> VisibleLayers;

	for( UINT LayerIndex = 0 ; LayerIndex < LayerList->GetCount() ; ++LayerIndex )
	{
		const UBOOL bIsChecked = LayerList->IsChecked( LayerIndex );
		if( bIsChecked )
		{
			const wxString LayerName( LayerList->GetString( LayerIndex ) );

			FString Wk( TEXT(",") );
			Wk += LayerName;
			Wk += TEXT(",");
			new( VisibleLayers )FString( Wk );

			if( VisibleLayersList.Len() )
			{
				VisibleLayersList += TEXT(",");
			}
			VisibleLayersList += LayerName;
		}
	}

	TArray<AActor*> ActorsNeedingComponentUpdate;
	UBOOL bActorWasDeselected = FALSE;
	for( FActorIterator It ; It ; ++It )
	{
		AActor* Actor = *It;

		if(	IsValid( Actor ) )
		{
			//Actor->Modify();

			FString ActorLayer( TEXT(",") );
			ActorLayer += Actor->Layer.ToString();
			ActorLayer += TEXT(",");

			INT LayerIndex;
			for( LayerIndex = 0 ; LayerIndex < VisibleLayers.Num() ; ++LayerIndex )
			{
				if( ActorLayer.InStr( VisibleLayers(LayerIndex) ) != INDEX_NONE )
				{
					//debugf(TEXT("F: %s"), *Actor->GetPathName() );
					if ( Actor->bHiddenEdLayer )
					{
						ActorsNeedingComponentUpdate.AddItem( Actor );
					}
					Actor->bHiddenEdLayer = FALSE;
					break;
				}
			}

			// If the actor isn't part of a visible layer, hide and de-select it.
			if( LayerIndex == VisibleLayers.Num() )
			{
				//debugf(TEXT("T: %s"), *Actor->GetPathName() );
				if ( !Actor->bHiddenEdLayer )
				{
					ActorsNeedingComponentUpdate.AddItem( Actor );
				}
				//if the actor was selected, mark it as unselected, else don't bother
				if (Actor->IsSelected())
				{
					bActorWasDeselected = TRUE;
					GEditor->SelectActor( Actor, FALSE, NULL, FALSE, TRUE );
				}
				Actor->bHiddenEdLayer = TRUE;
			}
		}
	}

	GWorld->GetWorldInfo()->VisibleLayers = VisibleLayersList;

	// Update components for actors whose hidden status changed.
	for ( INT ActorIndex = 0 ; ActorIndex < ActorsNeedingComponentUpdate.Num() ; ++ActorIndex )
	{
		ActorsNeedingComponentUpdate(ActorIndex)->ForceUpdateComponents( FALSE, FALSE );
	}

	if ( bActorWasDeselected )
	{
		GEditor->NoteSelectionChange();
	}
	GEditor->RedrawLevelEditingViewports();
}

class FGBActorEntry
{
public:
	FString ActorName;
	FString ClassName;
	AActor* Actor;
	FGBActorEntry() {}
	FGBActorEntry(const FString& InActorName, const FString& InClassName, AActor* InActor)
		: ActorName( InActorName )
		, ClassName( InClassName )
		, Actor( InActor )
	{}
};

/** If TRUE, sort by ActorClassName.  If FALSE, sort by OuterName.ActorName. */
static UBOOL bSortByClass = FALSE;
/** Controls whether or not to sort ascending/descending (1/-1). */
static INT SortAscending = -1;

IMPLEMENT_COMPARE_CONSTREF( FGBActorEntry, LayerBrowser, \
{ \
	const INT NameResult = (bSortByClass ? 1 : SortAscending) * appStricmp(*A.ActorName,*B.ActorName); \
	const INT ClassResult = (bSortByClass ? SortAscending : 1) * appStricmp(*A.ClassName,*B.ClassName); \
	if ( bSortByClass ) \
		return ClassResult == 0 ? NameResult : ClassResult; \
	else \
		return NameResult == 0 ? ClassResult : NameResult; \
} );

/** Populates the actor list with actors in the selected layers. */
void WxLayerBrowser::PopulateActorList()
{
	// Assemble a list of selected layer names.
	TArray<FString> SelectedLayerNames;
	GetSelectedNames( LayerList, SelectedLayerNames );

	ActorList->Freeze();
	ClearActorReferences();
	if ( SelectedLayerNames.Num() > 0 )
	{
		TArray<FGBActorEntry> ActorNames;
		for( FActorIterator It ; It ; ++It )
		{
			AActor* Actor = *It;
			if( IsValid( Actor ) )
			{
				for ( INT LayerIndex = 0 ; LayerIndex < SelectedLayerNames.Num() ; ++LayerIndex )
				{
					if ( Actor->IsInLayer( *SelectedLayerNames(LayerIndex) ) )
					{
						const UObject* StopOuter = Actor->GetOuter() ? Actor->GetOuter()->GetOuter() : NULL;
						ActorNames.AddItem( FGBActorEntry(Actor->GetPathName( StopOuter ), Actor->GetClass()->GetName(), Actor) );
						break;
					}
				}
			}
		}
		Sort<USE_COMPARE_CONSTREF(FGBActorEntry,LayerBrowser)>( ActorNames.GetTypedData(), ActorNames.Num() );
		for ( INT ActorIndex = 0 ; ActorIndex < ActorNames.Num() ; ++ActorIndex )
		{
			FGBActorEntry& Actor = ActorNames(ActorIndex);
			const INT ListIndex = ActorList->InsertItem( 0, *Actor.ClassName );
			ActorList->SetItem( ListIndex, 1, *Actor.ActorName );
			ActorList->SetItemPtrData( ListIndex, (PTRINT)(void*)Actor.Actor );
			ReferencedActors.AddItem( Actor.Actor );
		}
	}
	ActorList->Thaw();
}

/** Called when the tile of a column in the actor list is clicked. */
void WxLayerBrowser::OnActorListColumnClicked(wxListEvent& In)
{
	const int Column = In.GetColumn();
	if( Column > -1 )
	{
		const UBOOL bOldSortByClass = bSortByClass;
		bSortByClass = Column == 0 ? TRUE : FALSE;
		if ( bSortByClass == bOldSortByClass )
		{
			// The user clicked on the column already used for sort,
			// so toggle ascending/descending sort.
			SortAscending = -SortAscending;
		}
		else
		{
			// The user clicked on a new column, so set default ordering.
			SortAscending = -1;
		}

		PopulateActorList();
	}
}

/** Called when an actor in the actor list is clicked. */
void WxLayerBrowser::OnActorListItemClicked(wxListEvent& In)
{
	ActorListSelectAndFocus(FALSE);
}

/** Called when an actor in the actor list is double-clicked. */
void WxLayerBrowser::OnActorListItemDblClicked(wxListEvent& In)
{
	ActorListSelectAndFocus(TRUE);
}

void WxLayerBrowser::ActorListSelectAndFocus(UBOOL bFocus)
{
	// Assemble a list of selected layer names.
	TArray<AActor*> SelectedActors;
	GetSelectedActors( ActorList, SelectedActors );

	const FScopedTransaction* Transaction = NULL;
	UBOOL bSelectedActor = FALSE;
	for ( INT ActorIndex = 0 ; ActorIndex < SelectedActors.Num() ; ++ActorIndex )
	{
		AActor* Actor = SelectedActors(ActorIndex);
		if ( Actor )
		{
			if ( !bSelectedActor )
			{
				Transaction = new FScopedTransaction( *LocalizeUnrealEd("ActorSearchGoto") );
				GEditor->SelectNone( FALSE, TRUE );
				bSelectedActor = TRUE;
			}
			GEditor->SelectActor( Actor, TRUE, NULL, FALSE, TRUE );
		}
	}
	if ( bSelectedActor )
	{
		GEditor->NoteSelectionChange();
		if (bFocus)
		{
			GEditor->Exec( TEXT("CAMERA ALIGN") );
		}
		delete Transaction;
	}		
}

/**
 * Selects/de-selects actors by layer.
 * @return		TRUE if at least one actor was selected.
 */
UBOOL WxLayerBrowser::SelectActors(UBOOL InSelect)
{
	// Assemble a list of selected layer names.
	TArray<FString> SelectedLayerNames;
	GetSelectedNames( LayerList, SelectedLayerNames );

	// Select actors belonging to the selected layers.
	const UBOOL bResult = FLayerUtils::SelectActorsInLayers( SelectedLayerNames, InSelect, GUnrealEd );
	if ( bResult )
	{
		GUnrealEd->NoteSelectionChange();
	}
	return bResult;
}

/** Called when a list item check box is toggled. */
void WxLayerBrowser::OnToggled( wxCommandEvent& In )
{
	if( bAllowToggling )
	{
		UpdateActorVisibility();
	}
}

/** Called when item selection in the layer list changes. */
void WxLayerBrowser::OnLayerSelectionChange( wxCommandEvent& In )
{
	PopulateActorList();
}

/** Called when a list item in the layer browser is double clicked. */
void WxLayerBrowser::OnDoubleClick( wxCommandEvent& In )
{
	// Remove all existing selections.
	wxArrayInt Selections;
	const INT NumSelections = LayerList->GetSelections( Selections );

	for( INT x = 0 ; x < NumSelections ; ++x )
	{
		LayerList->Deselect( x );
	}

	// Make as visible the layer that was double clicked.
	const INT SelectionIndex = In.GetSelection();
	if ( !LayerList->IsChecked(SelectionIndex) )
	{
		LayerList->Check( SelectionIndex, true );
		UpdateActorVisibility();
	}

	// Select the layer that was double clicked.
	LayerList->Select( SelectionIndex );
	PopulateActorList();

	// Select the actors in the newly selected layer.
	GEditor->SelectNone( FALSE, TRUE );
	if ( !SelectActors( TRUE ) )
	{
		// Select actors returned FALSE, meaning that no actors were selected.
		// So, call NoteSelectionChange to cover the above SelectNone operation.
		GUnrealEd->NoteSelectionChange();
	}
}

/** Sets all layers to visible. */
void WxLayerBrowser::OnAllLayersVisible( wxCommandEvent& In )
{
	if( bAllowToggling )
	{
		for( UINT LayerIndex = 0 ; LayerIndex < LayerList->GetCount() ; ++LayerIndex )
		{
			LayerList->Check( LayerIndex, true );
		}
		UpdateActorVisibility();
	}
}

/** Creates a new layer. */
void WxLayerBrowser::OnNewLayer( wxCommandEvent& In )
{
	if( NumActorsSelected() == 0 )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_MustSelectActorsToCreateLayer") );
		return;
	}

	// Generate a suggested layer name to use as a default.
	FString DefaultName;

	INT LayerIndex = 1;
	do
	{
		DefaultName = FString::Printf(LocalizeSecure(LocalizeUnrealEd("Layer_F"), LayerIndex++));
	} while ( LayerList->FindString( *DefaultName ) != -1 );

	WxDlgLayer dlg;
	if( dlg.ShowModal( TRUE, DefaultName ) == wxID_OK )
	{
		if( GWorld->GetWorldInfo()->VisibleLayers.Len() )
		{
			GWorld->GetWorldInfo()->VisibleLayers += TEXT(",");
		}
		GWorld->GetWorldInfo()->VisibleLayers += dlg.GetObjectName();

		const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("AddSelectedActorsLayer")) );

		FLayerUtils::AddSelectedActorsToLayer( dlg.GetObjectName() );

		GUnrealEd->UpdatePropertyWindows();
		Update();
	}
}

/** Deletes a layer. */
void WxLayerBrowser::OnDeleteLayer( wxCommandEvent& In )
{
	// Assemble a list of selected layer names.
	TArray<FString> SelectedLayerNames;
	GetSelectedNames( LayerList, SelectedLayerNames );

	if ( SelectedLayerNames.Num() > 0 )
	{
		const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("DeleteLayer")) );

		for( FActorIterator It ; It ; ++It )
		{
			AActor* Actor = *It;
			if( IsValid( Actor ) )
			{
				UBOOL bActorAlreadyModified = TRUE;		// Flag to ensure that Actor->Modify is only called once.
				for ( INT LayerIndex = 0 ; LayerIndex < SelectedLayerNames.Num() ; ++LayerIndex )
				{
					bActorAlreadyModified = FLayerUtils::RemoveActorFromLayer( Actor, SelectedLayerNames(LayerIndex), bActorAlreadyModified );
				}
			}
		}

		GUnrealEd->UpdatePropertyWindows();
		Update();
	}
}

/** Handler for IDM_RefreshBrowser events; updates the browser contents. */
void WxLayerBrowser::OnRefresh( wxCommandEvent& In )
{
	Update();
}

/** Selects actors in the selected layers. */
void WxLayerBrowser::OnSelect( wxCommandEvent& In )
{
	SelectActors( TRUE );
}

/** De-selects actors in the selected layers. */
void WxLayerBrowser::OnDeselect( wxCommandEvent& In )
{
	SelectActors( FALSE );
}

/** Presents a rename dialog for each selected layer. */
void WxLayerBrowser::OnRename( wxCommandEvent& In )
{
	// Assemble a list of selected layer indices.
	TArray<INT> SelectedLayerIndices;
	GetSelectedLayerIndices( LayerList, SelectedLayerIndices );

	// Assemble a list of all layer names.
	TArray<FString> AllLayerNames;
	GetAllLayerNames( LayerList, AllLayerNames );

	UBOOL bRenamedLayer = FALSE;

	// For each selected layer, display a rename dialog.
	WxDlgLayer dlg;
	for( INT LayerIndex = 0 ; LayerIndex < SelectedLayerIndices.Num() ; ++LayerIndex )
	{
		const FString& CurLayerName = AllLayerNames( SelectedLayerIndices(LayerIndex) );
		if( dlg.ShowModal( FALSE, CurLayerName ) == wxID_OK )
		{
			const FString& NewLayerName = dlg.GetObjectName();
			if ( CurLayerName != NewLayerName )
			{
				// If the name already exists, notify the user and request a new name.
				if ( AllLayerNames.ContainsItem( NewLayerName ) )
				{
					appMsgf( AMT_OK, *LocalizeUnrealEd("Error_RenameFailedLayerAlreadyExists") );
					--LayerIndex;
				}
				else
				{
					const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("RenameLayer")) );

					// Iterate over all actors, swapping layers.
					for( FActorIterator It ; It ; ++It )
					{
						AActor* Actor = *It;
						if( IsValid( Actor ) )
						{
							if ( Actor->IsInLayer( *CurLayerName ) )
							{
								FLayerUtils::RemoveActorFromLayer( Actor, CurLayerName, TRUE );
								FLayerUtils::AddActorToLayer( Actor, NewLayerName, TRUE );
							}
						}
					}

					// Mark the new layer as visible.
					if( GWorld->GetWorldInfo()->VisibleLayers.Len() )
					{
						GWorld->GetWorldInfo()->VisibleLayers += TEXT(",");
					}
					GWorld->GetWorldInfo()->VisibleLayers += dlg.GetObjectName();

					// update all views's hidden layers if they had this one
					for (INT ViewIndex = 0; ViewIndex < GUnrealEd->ViewportClients.Num(); ViewIndex++)
					{
						FEditorLevelViewportClient* ViewportClient = GUnrealEd->ViewportClients(ViewIndex);
						if (ViewportClient->ViewHiddenLayers.RemoveItem(FName(*CurLayerName)) > 0)
						{
							ViewportClient->ViewHiddenLayers.AddUniqueItem(FName(*NewLayerName));
							ViewportClient->Invalidate();
						}

						FLayerUtils::UpdatePerViewVisibility(ViewIndex, FName(*NewLayerName));
					}


					// Flag an update.
					bRenamedLayer = TRUE;
				}
			}
		}
	}

	if ( bRenamedLayer )
	{
		GUnrealEd->UpdatePropertyWindows();
		Update();
	}
}

/** Adds level-selected actors to the selected layers. */
void WxLayerBrowser::OnAddToLayer( wxCommandEvent& In )
{
	// Assemble a list of selected layer names.
	TArray<FString> SelectedLayerNames;
	GetSelectedNames( LayerList, SelectedLayerNames );

	const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("AddSelectedActorsLayer")) );

	// Add selected actors to the selected layers.
	if ( FLayerUtils::AddSelectedActorsToLayers( SelectedLayerNames ) )
	{
		UpdateActorVisibility();
		PopulateActorList();
		GUnrealEd->UpdatePropertyWindows();
	}
}

/** Deletes level-selected actors from the selected layers. */
void WxLayerBrowser::OnDeleteSelectedActorsFromLayers( wxCommandEvent& In )
{
	// Assemble a list of selected layer names.
	TArray<FString> SelectedLayerNames;
	GetSelectedNames( LayerList, SelectedLayerNames );

	const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("DeleteSelectedActorsLayer")) );

	// Remove selected actors from the selected layers.
	if ( FLayerUtils::RemoveSelectedActorsFromLayers( SelectedLayerNames ) )
	{
		GUnrealEd->UpdatePropertyWindows();
		Update();
	}
}

/** Returns the key to use when looking up values. */
const TCHAR* WxLayerBrowser::GetLocalizationKey() const
{
	return TEXT("LayerBrowser");
}

/** Responds to size events by updating the splitter. */
void WxLayerBrowser::OnSize(wxSizeEvent& In)
{
	// During the creation process a sizing message can be sent so don't
	// handle it until we are initialized
	if ( bAreWindowsInitialized )
	{
		SplitterWindow->SetSize( GetClientRect() );
	}
}

/** Presents a context menu for layer list items. */
void WxLayerBrowser::OnRightButtonDown( wxMouseEvent& In )
{
	int Item = LayerList->HitTest(In.GetPosition());

	if (wxNOT_FOUND != Item)
	{
		if (!LayerList->IsSelected(Item))
		{
			wxArrayInt Sel;
			LayerList->GetSelections(Sel);
			for (size_t i = 0; i < Sel.GetCount(); i++)
			{
				LayerList->Deselect(Sel[i]);
			}
			LayerList->SetSelection(Item);
		}

		LayerList->Refresh();

		WxLayerMenu ContextMenu;

		FTrackPopupMenu Popup(this, &ContextMenu);
		Popup.Show();
	}
}
