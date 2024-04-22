/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DlgActorSearch.h"
#include "ScopedTransaction.h"
#include "BusyCursor.h"
#include "LevelUtils.h"

BEGIN_EVENT_TABLE( WxDlgActorSearch, wxDialog )
	EVT_TIMER( ACTORSEARCH_TIMER_ID, WxDlgActorSearch::OnTimer )
END_EVENT_TABLE()

WxDlgActorSearch::WxDlgActorSearch( wxWindow* InParent )
{
	const bool bSuccess = wxXmlResource::Get()->LoadDialog( this, InParent, TEXT("ID_DLG_ACTOR_SEARCH") );
	check( bSuccess );

	SearchForEdit = (wxTextCtrl*)FindWindow( XRCID( "ID_SEARCH_FOR" ) );
	check( SearchForEdit != NULL );
	StartsWithRadio = (wxRadioButton*)FindWindow( XRCID( "ID_STARTING_WITH" ) );
	check( StartsWithRadio != NULL );
	ContainingRadio = (wxRadioButton*)FindWindow( XRCID( "ID_CONTAINING" ) );
	check( ContainingRadio != NULL );
	ResultsList = (wxListCtrl*)FindWindow( XRCID( "ID_RESULTS" ) );
	check( ResultsList != NULL );
	ResultsLabel = (wxStaticText*)FindWindow( XRCID( "ID_RESULTS_TEXT" ) );
	check( ResultsLabel != NULL );
	InsideOfCombo = (wxComboBox*)FindWindow( XRCID( "IDCB_INSIDEOF" ) );
	check( InsideOfCombo != NULL );

	InsideOfCombo->Append( *LocalizeUnrealEd("Name") );
	InsideOfCombo->Append( *LocalizeUnrealEd("Level") );
	InsideOfCombo->Append( *LocalizeUnrealEd("Layer") );
	InsideOfCombo->Append( *LocalizeUnrealEd("PathName") );
	InsideOfCombo->Append( *LocalizeUnrealEd("Tag") );
	InsideOfCombo->SetSelection( 0 );

	StartsWithRadio->SetValue( 0 );
	ContainingRadio->SetValue( 1 );

	ResultsList->InsertColumn( ASF_Name, *LocalizeUnrealEd("Name"), wxLIST_FORMAT_LEFT, 128 );
	ResultsList->InsertColumn( ASF_Level, *LocalizeUnrealEd("Level"), wxLIST_FORMAT_LEFT, 128 );
	ResultsList->InsertColumn( ASF_Layer, *LocalizeUnrealEd("Layer"), wxLIST_FORMAT_LEFT, 128 );
	ResultsList->InsertColumn( ASF_PathName, *LocalizeUnrealEd("PathName"), wxLIST_FORMAT_LEFT, 300 );
	ResultsList->InsertColumn( ASF_Tag, *LocalizeUnrealEd("Tag"), wxLIST_FORMAT_LEFT, 128 );

	ADDEVENTHANDLER( XRCID("ID_SEARCH_FOR"), wxEVT_COMMAND_TEXT_UPDATED, &WxDlgActorSearch::OnSearchTextChanged );
	ADDEVENTHANDLER( XRCID("ID_STARTING_WITH"), wxEVT_COMMAND_RADIOBUTTON_SELECTED, &WxDlgActorSearch::OnSearchTextChanged );
	ADDEVENTHANDLER( XRCID("ID_CONTAINING"), wxEVT_COMMAND_RADIOBUTTON_SELECTED, &WxDlgActorSearch::OnSearchTextChanged );
	wxEvtHandler* eh = GetEventHandler();
	eh->Connect( XRCID("ID_RESULTS"), wxEVT_COMMAND_LIST_COL_CLICK, wxListEventHandler(WxDlgActorSearch::OnColumnClicked) );
	eh->Connect( XRCID("ID_RESULTS"), wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEventHandler(WxDlgActorSearch::OnItemActivated) );
	ADDEVENTHANDLER( XRCID("IDPB_GOTOACTOR"), wxEVT_COMMAND_BUTTON_CLICKED, &WxDlgActorSearch::OnGoTo )
	ADDEVENTHANDLER( XRCID("IDPB_DELETEACTOR"), wxEVT_COMMAND_BUTTON_CLICKED, &WxDlgActorSearch::OnDelete )
	ADDEVENTHANDLER( XRCID("IDPB_PROPERTIES"), wxEVT_COMMAND_BUTTON_CLICKED, &WxDlgActorSearch::OnProperties )
	eh->Connect( this->GetId(), wxEVT_ACTIVATE, wxActivateEventHandler(WxDlgActorSearch::OnActivate) );
	ADDEVENTHANDLER( XRCID("IDCB_INSIDEOF"), wxEVT_COMMAND_COMBOBOX_SELECTED, &WxDlgActorSearch::OnInsideOfComboBoxChanged );

	FWindowUtil::LoadPosSize( TEXT("DlgActorSearch"), this, 256, 256, 450, 400 );

	//Guard around this window not existing yet and therefore doesn't have a world to reference
	if (GWorld)
	{
		UpdateResults();
	}

	FLocalizeWindow( this );

	_updateResultsTimer.SetOwner(this,ACTORSEARCH_TIMER_ID);
}

WxDlgActorSearch::~WxDlgActorSearch()
{
	FWindowUtil::SavePosSize( TEXT("DlgActorSearch"), this );
	_updateResultsTimer.Stop();
}

void WxDlgActorSearch::OnTimer( wxTimerEvent& event )
{
	UpdateResults();
}

void WxDlgActorSearch::OnSearchTextChanged( wxCommandEvent& In )
{
	// if the timer was already running, reset the timer and start over
	if(_updateResultsTimer.IsRunning())
	{
		_updateResultsTimer.Stop();		
	}
	_updateResultsTimer.Start(SEARCH_TEXT_UPDATE_DELAY,wxTIMER_ONE_SHOT);
}

void WxDlgActorSearch::OnColumnClicked( wxListEvent& In )
{
	const INT Column = In.GetColumn();

	if( Column > -1 )
	{
		if( Column == SearchOptions.Column )
		{
			// Clicking on the same column will flip the sort order
			SearchOptions.bSortAscending = !SearchOptions.bSortAscending;
		}
		else
		{
			// Clicking on a new column will set that column as current and reset the sort order.
			SearchOptions.Column = static_cast<EActorSearchFields>( In.GetColumn() );
			SearchOptions.bSortAscending = TRUE;
		}
	}

	UpdateResults();
}

void WxDlgActorSearch::OnItemActivated( wxListEvent& In )
{
	wxCommandEvent Event;
	OnGoTo( Event );
}

/**
 * Empties the search list, releases actor references, etc.
 */
void WxDlgActorSearch::Clear()
{
	ResultsList->DeleteAllItems();
	ReferencedActors.Empty();
	ResultsLabel->SetLabel( *FString::Printf( LocalizeSecure(LocalizeUnrealEd("F_ObjectsFound"), 0, 0) ) );
}

/**
 * Orders items in the actor search dialog's list view.
 */
static int wxCALLBACK WxActorResultsListSort(UPTRINT InItem1, UPTRINT InItem2, UPTRINT InSortData)
{
	// Determine the sort order of the provided actors using the current sort data
	AActor* A = reinterpret_cast<AActor*>( InItem1 );
	AActor* B = reinterpret_cast<AActor*>( InItem2 );
	WxDlgActorSearch::FActorSearchOptions* SearchOptions = reinterpret_cast<WxDlgActorSearch::FActorSearchOptions*>( InSortData );
	check( A && B && SearchOptions );

	// Get the search string for each actor
	const FString StringA = WxDlgActorSearch::GetActorSearchString( A, SearchOptions->Column );
	const FString StringB = WxDlgActorSearch::GetActorSearchString( B, SearchOptions->Column );

	return appStricmp( *StringA, *StringB ) * ( SearchOptions->bSortAscending ? 1 : -1 );
}

/**
 * Updates the contents of the results list.
 */
void WxDlgActorSearch::UpdateResults()
{
	const FScopedBusyCursor BusyCursor;
	ResultsList->Freeze();
	{
		// Before emptying the list, preserve the state of each item based on actor
		TMap<PTRINT, LONG> ExistingResultValues;
		const INT NumItems = ResultsList->GetItemCount();
		for ( INT ItemIndex = 0; ItemIndex < NumItems; ++ItemIndex )
		{
			ExistingResultValues.Set( ResultsList->GetItemData( ItemIndex ), ResultsList->GetItemState( ItemIndex, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED ) );
		}

		// Empties the search list, releases actor references, etc.
		Clear();

		// Get the text to search with.  If it's empty, leave.

		FString SearchText = (const TCHAR*)SearchForEdit->GetValue();
		SearchText = SearchText.ToUpper();

		// Looks through all actors and see which ones meet the search criteria
		const EActorSearchFields SearchFields = static_cast<EActorSearchFields>( InsideOfCombo->GetSelection() );
		
		INT NumMatchingActors = 0;
		for( FActorIterator It; It; ++It )
		{
			AActor* Actor = *It;
			// skip transient actors (path building scout, etc)
			if ( !Actor->HasAnyFlags(RF_Transient) && !Actor->IsTemplate() && !FLevelUtils::IsLevelLocked( Actor->GetLevel() ) && Actor->bEditable )
			{
				UBOOL bFoundItem = FALSE;
				
				if( SearchText.Len() == 0 )
				{
					// If there is no search text, show all actors.
					bFoundItem = TRUE;
				}
				else
				{
					FString CompString = GetActorSearchString( Actor, SearchFields );

					// Starts with/contains.
					if( StartsWithRadio->GetValue() )
					{
						if( CompString.ToUpper().StartsWith( SearchText ) )
						{
							bFoundItem = TRUE;
						}
					}
					else
					{
						if( CompString.ToUpper().InStr( SearchText ) != INDEX_NONE )
						{
							bFoundItem = TRUE;
						}
					}
				}

				// If the actor matches the criteria, add it to the list.
				if( bFoundItem )
				{
					++NumMatchingActors;
					if ( ResultsList->GetItemCount() < MaxViewableResuts )
					{
						const LONG Idx = ResultsList->InsertItem( ASF_Name, *Actor->GetName() );
						ResultsList->SetItem( Idx, ASF_Level, *Actor->GetOutermost()->GetName() );
						ResultsList->SetItem( Idx, ASF_PathName, *Actor->GetPathName() );
						ResultsList->SetItem( Idx, ASF_Layer, *Actor->Layer.ToString() );
						ResultsList->SetItem( Idx, ASF_Tag, *Actor->Tag.ToString() );

						PTRINT ActorAsPtrInt = reinterpret_cast< PTRINT >( Actor ); 
						ResultsList->SetItemPtrData( Idx, ActorAsPtrInt );
						
						// Try to restore the state of an item if the actor was previously in the list before update
						const LONG* const OldState = ExistingResultValues.Find( ActorAsPtrInt );
						if ( OldState )
						{
							ResultsList->SetItemState( Idx, *OldState, *OldState );
						}
						ReferencedActors.AddItem( Actor );
					}
					
				}
			}
		}

		// Sort based on the current options
		ResultsList->SortItems( WxActorResultsListSort, reinterpret_cast< UPTRINT >( &SearchOptions ) );
		
		ResultsLabel->SetLabel( *FString::Printf( LocalizeSecure(LocalizeUnrealEd("F_ObjectsFound"), NumMatchingActors, ResultsList->GetItemCount()) ) );
	}
	ResultsList->Thaw();

	
}

/** Serializes object references to the specified archive. */
void WxDlgActorSearch::Serialize(FArchive& Ar)
{
	Ar << ReferencedActors;
}

/**
 * Utility method for getting the correct string to search by for a given actor in a specified search field
 *
 * @param	InActor		Actor to find a search string for
 * @param	SearchField	Search field to retrieve a search string for
 *
 * @return String representing the actor's search string for the given search field
 */
FString WxDlgActorSearch::GetActorSearchString( AActor* InActor, EActorSearchFields SearchField )
{
	FString SearchString;
	switch ( SearchField )
	{
		// Handle name search
		case ASF_Name:	
			SearchString = InActor->GetName();
			break;

		// Handle level name search
		case ASF_Level:
			SearchString = InActor->GetOutermost()->GetName();
			break;

		// Handle tag search
		case ASF_Tag:
			SearchString = InActor->Tag.ToString();
			break;

		// Handle path name search
		case ASF_PathName:
			SearchString = InActor->GetPathName();
			break;

		// Handle layer search
		case ASF_Layer:
			SearchString = InActor->Layer.ToString();
			break;
		default:
			break;
	};
	return SearchString;
}

void WxDlgActorSearch::OnGoTo( wxCommandEvent& In )
{
	long ItemIndex = ResultsList->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	AActor* Actor = (AActor*)(ResultsList->GetItemData( ItemIndex ));

	const FScopedTransaction Transaction( *LocalizeUnrealEd("ActorSearchGoto") );

	// Deselect all actors, then focus on the first actor in the list, and simply select the rest.
	GEditor->Exec(TEXT("ACTOR SELECT NONE"));

	while( ItemIndex != -1 )
	{
		Actor = (AActor*)(ResultsList->GetItemData( ItemIndex ));
		if ( Actor )
		{
			GEditor->SelectActor( Actor, TRUE, NULL, FALSE, TRUE );
		}

		// Advance list iterator.
		ItemIndex = ResultsList->GetNextItem(ItemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	}

	GEditor->NoteSelectionChange();
	GEditor->Exec( TEXT("CAMERA ALIGN") );
}

void WxDlgActorSearch::OnDelete( wxCommandEvent& In )
{
	GEditor->Exec(TEXT("ACTOR SELECT NONE"));

	long ItemIndex = ResultsList->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );

	while( ItemIndex != -1 )
	{
		AActor* Actor = (AActor*)ResultsList->GetItemData( ItemIndex );
		if ( Actor )
		{
			GEditor->SelectActor( Actor, TRUE, NULL, FALSE, TRUE );
		}
		ItemIndex = ResultsList->GetNextItem( ItemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	}

	GEditor->Exec( TEXT("DELETE") );
	UpdateResults();
}

void WxDlgActorSearch::OnProperties(wxCommandEvent& In )
{
	long ItemIndex = ResultsList->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	AActor* Actor = (AActor*)(ResultsList->GetItemData( ItemIndex ));

	const FScopedTransaction Transaction( *LocalizeUnrealEd("ActorSearchSelectActors") );

	// Deselect all actors, then select the actors that are selected in our list, then 
	GEditor->Exec(TEXT("ACTOR SELECT NONE"));

	while( ItemIndex != -1 )
	{
		Actor = (AActor*)(ResultsList->GetItemData( ItemIndex ));
		if ( Actor )
		{
			GEditor->SelectActor( Actor, TRUE, NULL, FALSE, TRUE );
		}

		// Advance list iterator.
		ItemIndex = ResultsList->GetNextItem(ItemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	}

	GEditor->NoteSelectionChange();
	GEditor->Exec( TEXT("EDCALLBACK SELECTEDPROPS") );
}

/** Refreshes the results list when the window gets focus. */
void WxDlgActorSearch::OnActivate(wxActivateEvent& In)
{
	const UBOOL bActivated = In.GetActive();
	
	if( bActivated )
	{
		// Find which index in the list control is currently visible as the top entry in the control
		INT HitTestResultFlags = 0;
		const INT TopmostVisibleIndex = ResultsList->HitTest( wxPoint( 0, 0 ), HitTestResultFlags );

		// Find the bounds of the item in the list control in order to help restore scroll position after an update
		wxRect ItemBounds;
		if ( TopmostVisibleIndex != wxNOT_FOUND )
		{
			ResultsList->GetItemRect( TopmostVisibleIndex, ItemBounds, wxLIST_RECT_LABEL );
		}

		// Cache where the scroll bar is currently positioned in order to restore scroll position after an update
		const INT ScrollBarPos = ResultsList->GetScrollPos( wxVERTICAL );
	
		UpdateResults();
		
		// Restore the scroll bar position, if possible, by scrolling the list control by a number of pixels equal to
		// the height of one item multiplied by the cached scroll bar position
		if ( ScrollBarPos <= ResultsList->GetItemCount() && TopmostVisibleIndex != wxNOT_FOUND )
		{
			ResultsList->ScrollList( 0, ScrollBarPos * ItemBounds.GetHeight() );
		}

		// Grant focus to the results list if any of its items are selected
		if ( ResultsList->GetSelectedItemCount() > 0 )
		{
			ResultsList->SetFocus();
		}
	}
}


/** Called after the user interacts with the "Inside Of" combo box */
void WxDlgActorSearch::OnInsideOfComboBoxChanged( wxCommandEvent& In ) 
{
     UpdateResults();
}
