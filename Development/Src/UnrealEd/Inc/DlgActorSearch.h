/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DLGACTORSEARCH_H__
#define __DLGACTORSEARCH_H__

/**
* wxWidgets timer ID
*/
#define ACTORSEARCH_TIMER_ID 9998

/** delay after a new character is entered into the search dialog to wait before updating the list (to give them time to enter a whole string instead of useless updating every time a char is put in) **/
#define SEARCH_TEXT_UPDATE_DELAY 750

class WxDlgActorSearch : public wxDialog, public FSerializableObject
{
public:
	enum EActorSearchFields
	{
		ASF_Name,		// Search by name
		ASF_Level,		// Search by level
		ASF_Layer,		// Search by layer
		ASF_PathName,	// Search by path name
		ASF_Tag			// Search by tag
	};

	WxDlgActorSearch( wxWindow* InParent );
	virtual ~WxDlgActorSearch();

	/**
	 * Empties the search list, releases actor references, etc.
	 */
	void Clear();

	/** Updates the results list using the current filter and search options set. */
	void UpdateResults();

	/** Serializes object references to the specified archive. */
	virtual void Serialize(FArchive& Ar);

	class FActorSearchOptions
	{
	public:
		FActorSearchOptions()
			: Column( ASF_Name )
			, bSortAscending( TRUE )
		{}
		/** The column currently being used for sorting. */
		EActorSearchFields Column;
		/** Denotes ascending/descending sort order. */
		UBOOL bSortAscending;
	};

	/**
	 * Utility method for getting the correct string to search by for a given actor in a specified search field
	 *
	 * @param	InActor		Actor to find a search string for
	 * @param	SearchField	Search field to retrieve a search string for
	 *
	 * @return String representing the actor's search string for the given search field
	 */
	static FString GetActorSearchString( AActor* InActor, EActorSearchFields SearchField );

protected:
	/** Maximum number of results viewable in the Find Actors dialog */
	static const int MaxViewableResuts = 1000;

	wxTextCtrl *SearchForEdit;
	wxRadioButton *StartsWithRadio;
	wxRadioButton *ContainingRadio;
	wxComboBox *InsideOfCombo;
	wxListCtrl *ResultsList;
	wxStaticText *ResultsLabel;

	TArray<AActor*> ReferencedActors;

	FActorSearchOptions SearchOptions;

	/** timer to handle updating of search items **/
	wxTimer _updateResultsTimer;

	/** Wx Event Handlers */
	void OnTimer( wxTimerEvent& event );
	void OnSearchTextChanged( wxCommandEvent& In );
	void OnColumnClicked( wxListEvent& In );
	void OnItemActivated( wxListEvent& In );
	void OnGoTo( wxCommandEvent& In );
	void OnDelete( wxCommandEvent& In );
	void OnProperties( wxCommandEvent& In );

	/** Refreshes the results list when the window gets focus. */
	void OnActivate( wxActivateEvent& In);

	/** Called after the user interacts with the "Inside Of" combo box */
	void OnInsideOfComboBoxChanged( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

#endif // __DLGACTORSEARCH_H__
