/*=============================================================================
	DlgCheckBoxList.h: UnrealEd dialog for displaying a list of check boxes.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DLG_CHECK_BOX_LIST_H__
#define __DLG_CHECK_BOX_LIST_H__

const int StandardCheckBoxListBorder = 5;
const int CheckBoxListButtonBaseIndex = wxID_HIGHEST + 100;

template<typename ObjectType>
struct CheckBoxListEntry 
{
	FString EntryName;		//Name of the checkbox
	FString IconName; 		// Name of an icon to show next to the checkbox
	FString IconToolTip;	// ToolTip to display for the icon
	ObjectType Object; 		// The object associated with this entry
	INT State;			// The state of the checkbox
	UBOOL bDisabled; 		// if the entry is disabled
	wxCheckBox* CheckBox; 		// checkbox widget
	wxStaticText* CheckBoxText;	// Separate check box text for disabing text and not checkbox
	wxStaticBitmap* Icon; 		// Icon widget
};

struct CheckBoxButtonEntry
{
	FString ButtonName; 	// The name of the button
	FString Tooltip; 	// The tooltip for the button
	INT ButtonID; 		// The button id for receiving events
	UBOOL bDisabled;	// If the button is disabled
};

/**
  * UnrealEd check box list child window.  Can be used independently, but was built for the WxDlgCheckBoxList
  */
template<typename ObjectType>
class WxCheckBoxListWindow : public wxPanel
{
public:
	/**
	 * Constructor
	 */
	WxCheckBoxListWindow(void) : CheckListWindow(NULL), CheckAllButton(NULL), UncheckAllButton(NULL), LastClickedIndex(-1)
	{

	}

	/**
	 * Destructor
	 */
	virtual ~WxCheckBoxListWindow(void)
	{

	}

	/**
	 * After the entries have been added, this is used to generate the WxWidgets
	 */
	void Create(wxWindow* InParent)
	{
		wxPanel::Create(InParent, -1);

		//Main Sizer
		wxStaticBoxSizer* MainVerticalSizer = new wxStaticBoxSizer(wxVERTICAL, this);
		SetSizer(MainVerticalSizer );

		//wrapper window for proper border drawing
		wxPanel* BorderPanel = new wxPanel(this, -1);
		MainVerticalSizer->Add(BorderPanel, 1, wxALIGN_TOP | wxALIGN_CENTER | wxGROW | wxALL, 0);
		//Border Sizer
		wxBoxSizer* BorderSizer = new wxBoxSizer(wxVERTICAL);
		BorderPanel->SetSizer(BorderSizer);
		BorderPanel->SetOwnBackgroundColour(*wxBLACK);
		//Border events
		//BorderPanel->wxEvtHandler::Connect(wxID_ANY, wxEVT_ERASE_BACKGROUND , wxEraseEventHandler(WxCheckBoxListWindow<ObjectType>::OnErase));
		//BorderPanel->wxEvtHandler::Connect(wxID_ANY, wxEVT_PAINT , wxPaintEventHandler(WxCheckBoxListWindow<ObjectType>::OnWhiteBackgroundPaint));

		{
			//Check Box List Box
			check (CheckListWindow == NULL);
			CheckListWindow = new wxScrolledWindow();
			CheckListWindow->Create(BorderPanel, -1);
			//CheckListWindow->wxEvtHandler::Connect(wxID_ANY, wxEVT_ERASE_BACKGROUND , wxEraseEventHandler(WxCheckBoxListWindow<ObjectType>::OnErase));
			CheckListWindow->wxEvtHandler::Connect(wxID_ANY, wxEVT_PAINT , wxPaintEventHandler(WxCheckBoxListWindow<ObjectType>::OnWhiteBackgroundPaint));

			wxBoxSizer* CheckBoxSizer = new wxBoxSizer(wxVERTICAL);

			//set all check boxes to tri-state enabled
			for (int i = 0; i < CheckBoxListEntries.Num(); ++i)
			{
				// Create the widgets for the current entry
				CheckBoxListEntry<ObjectType>& CurrentEntry = CheckBoxListEntries(i);
				CurrentEntry.CheckBox = new wxCheckBox(CheckListWindow, i, TEXT(""), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE);
				//CurrentEntry.CheckBox = new wxCheckBox(CheckListWindow, i, *CurrentEntry.EntryName, wxDefaultPosition, wxDefaultSize, wxCHK_3STATE);
				//CurrentEntry.CheckBox->Enable( !CurrentEntry.bDisabled );
				CurrentEntry.CheckBox->SetOwnBackgroundColour(*wxWHITE);
				// Create the checbox text separate from the checkbox itself. This is so we can receive check events even if an entry is disabled
				CurrentEntry.CheckBoxText = new wxStaticText(CheckListWindow, i, *CurrentEntry.EntryName);
				CurrentEntry.CheckBoxText->Enable(!CurrentEntry.bDisabled);
				CurrentEntry.CheckBoxText->SetOwnBackgroundColour(*wxWHITE);
				INT wxFlags = wxALIGN_LEFT | wxALIGN_TOP | wxLEFT | wxRIGHT;
				if (i == 0)
				{
					wxFlags |= wxTOP;
				}
				else if (i == CheckBoxListEntries.Num()-1)
				{
					wxFlags |= wxBOTTOM;
				}
				
				// Add a horizontal sizer to correctly line up bitmaps, checkboxes, and text
				wxBoxSizer* BitmapCheckBoxSizer = new wxBoxSizer(wxHORIZONTAL);
				if( CurrentEntry.IconName.Len() > 0 )
				{
					// We have a valid icon name, create the bitmap
					WxBitmap Bitmap;
					Bitmap.Load(CurrentEntry.IconName);
				
					CurrentEntry.Icon = new wxStaticBitmap(CheckListWindow, i, Bitmap);
					// Set the background color of the bitmap to white since the background of the 
					// check box list we are on top of has a white background.  This ensures transparency works correctly
					CurrentEntry.Icon->SetOwnBackgroundColour(*wxWHITE);
					if( CurrentEntry.IconToolTip.Len() > 0 )
					{
						// Set a tooltip if we have one
						CurrentEntry.Icon->SetToolTip( *CurrentEntry.IconToolTip);
					}
					BitmapCheckBoxSizer->Add(CurrentEntry.Icon, 0, wxRIGHT, 5);
				}
				BitmapCheckBoxSizer->Add(CurrentEntry.CheckBox, 0, wxBOTTOM, 5);
				BitmapCheckBoxSizer->Add(CurrentEntry.CheckBoxText,0, wxLEFT, 3);
				CheckBoxSizer->Add(BitmapCheckBoxSizer, 0, wxFlags, 5);
			}
			CheckListWindow->SetAutoLayout(true);
			CheckListWindow->SetSizer(CheckBoxSizer);
		}

		Connect(-1, wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(WxCheckBoxListWindow::OnCheckClicked));

		RefreshAllChecks();

		//add checklistwindow to border sizer
		BorderSizer->Add(CheckListWindow, 1, wxALIGN_TOP | wxALIGN_CENTER | wxGROW | wxALL, 1);

		//Button Sizer
		wxBoxSizer* ButtonSizer = new wxBoxSizer(wxHORIZONTAL);
		MainVerticalSizer->Add(ButtonSizer , 0, wxALIGN_BOTTOM | wxALIGN_LEFT, 0);// | wxSHAPED | wxGROW | wxALL, 0);

		//Check All
		const INT CheckAllIndex = 0;
		CheckAllButton = new wxButton();
		CheckAllButton->Create(this, CheckAllIndex, *LocalizeUnrealEd("CheckBoxListWindow_CheckAll"));
		ButtonSizer->Add(CheckAllButton, 1, wxALIGN_CENTER | wxALL, StandardCheckBoxListBorder);// | wxSHAPED | wxGROW | wxALL, 0);
		Connect(CheckAllIndex, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxCheckBoxListWindow::SelectAll));

		//Uncheck All
		const INT UncheckAllIndex = 1;
		UncheckAllButton = new wxButton();
		UncheckAllButton->Create(this, UncheckAllIndex, *LocalizeUnrealEd("CheckBoxListWindow_UncheckAll"));
		ButtonSizer->Add(UncheckAllButton, 1, wxALIGN_CENTER  | wxALL, StandardCheckBoxListBorder);// | wxSHAPED | wxGROW | wxALL, 0);
		Connect(UncheckAllIndex, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxCheckBoxListWindow::UnSelectAll));

		Layout();
	}

	/** Empty background event. */
	void OnErase( wxEraseEvent& Event )
	{
	}
	
	/** Empty draw event. */
	void OnPaint( wxPaintEvent& Event )
	{
	}

	/** Sets background to white. */
	void OnWhiteBackgroundPaint( wxPaintEvent& Event )
	{
		wxBufferedPaintDC dc( this );
		wxRect rc = GetClientRect();

		dc.SetBrush( wxBrush( *wxWHITE, wxSOLID ) );
		dc.SetPen( *wxTRANSPARENT_PEN );
		dc.DrawRectangle( rc.x,rc.y, rc.width,rc.height);

		dc.SetBrush(wxNullBrush);
		dc.SetPen(wxNullPen);
	}

	/** 
	 * Enables Scrolling for this window
	 * Not called automatically in case the desired height of the window is needed.  Dialog ignores this
	 */
	void EnableScrolling (void)
	{
		//enable scrolling after layout to get the real height
		CheckListWindow->EnableScrolling(false, true);
		CheckListWindow->SetScrollRate(0,10);
	}


	/**
	 * Selects all checkboxes and repopulates the window
	 */
	void SelectAll( wxCommandEvent& InEvent )
	{
		for ( INT EntryIdx = 0; EntryIdx < CheckBoxListEntries.Num(); ++EntryIdx )
		{
			SetChecked( EntryIdx, TRUE );
		}

		RefreshAllChecks();
	}

	/**
	 * Unselects all checkboxes and repopulates the window
	 */
	void UnSelectAll( wxCommandEvent& InEvent )
	{
		for ( INT EntryIdx = 0; EntryIdx < CheckBoxListEntries.Num(); ++EntryIdx  )
		{
			SetChecked( EntryIdx, FALSE );
		}

		RefreshAllChecks();
	}


	/**
	 * Callback when individual check box is clicked
	 */
	void OnCheckClicked( wxCommandEvent& InEvent )
	{
		INT EntryIndex = InEvent.GetId();
		check(IsWithin<INT>(EntryIndex, 0, CheckBoxListEntries.Num()));

		const UBOOL bShiftKeyDown = ( GetKeyState(VK_LSHIFT) & 0x8000 ) || ( GetKeyState(VK_RSHIFT) & 0x8000 );
		
		// If the shift key is down and we have a valid last clicked index perform a shift selection 
		if( bShiftKeyDown && LastClickedIndex != -1 )
		{	
			// The state of the checkbox that was just clicked
			INT ClickedBoxState = CheckBoxListEntries( EntryIndex ).State;
	
			// Indices to use for a range to shift select items
			INT StartIdx,EndIdx;
			if( LastClickedIndex < EntryIndex )
			{
				// Shift selecting down, make sure the start is less than the end
				StartIdx = LastClickedIndex;
				EndIdx = EntryIndex; 
			}
			else
			{
				// Shift selecting up, make sure the start is less than the end
				StartIdx = EntryIndex;
				EndIdx = LastClickedIndex;
			}

			// Go through each check box in the shift select range and modify its state
			for( INT CheckIdx = StartIdx; CheckIdx <= EndIdx; ++CheckIdx )
			{
				CheckBoxListEntry<ObjectType>& CurEntry = CheckBoxListEntries(CheckIdx);

				if( CurEntry.State == ClickedBoxState || ( ( CurEntry.State == wxCHK_UNDETERMINED || CurEntry.State == wxCHK_CHECKED ) && ( ClickedBoxState == wxCHK_UNDETERMINED || ClickedBoxState == wxCHK_CHECKED ) ) )
				{
					// If the current entries state is the same as the box that was clicked on state, we should toggle the state of the check box.
					// Note that this statement checks to see if the current and clicked state are both checked or unchecked.  For the purposes of shift-selection a check box is checked even if its state is the "undetermined" state
					SetChecked( CheckIdx, !ClickedBoxState );
				}
			}

		}
		else
		{
			// User is not shift selecting, toggle the check box that was clicked
			SetChecked( EntryIndex, !CheckBoxListEntries(EntryIndex).State );
		}

		// Store the new last clicked index
		LastClickedIndex = EntryIndex;

		// Update the checkboxes visuals
		RefreshAllChecks();
	}

	/**
	 * Add entry to the list of check boxes
	 * @param InObject - Templatized object to use in the list
	 * @param InEntryName - The name respresenting that entry
	 * @param InState - 0 for off, 1 for on, 2 for undetermined
	 * @param InDisabled - TRUE of the check box should be disabled
	 * @param InIconName - The name of the icon we should place to the left of the checkbox
	 * @param InIconToolTip - The tooltip for the icon.  If the icon name isn't the tooltip will not appear.
	 */
	void AddCheck(ObjectType InObject, const FString& InEntryName, const INT InState, UBOOL InDisabled = FALSE, const FString& InIconName = TEXT(""), const FString& InIconToolTip = TEXT(""))
	{
		CheckBoxListEntry<ObjectType> CheckEntry;
		CheckEntry.EntryName = InEntryName;
		CheckEntry.Object = InObject;
		CheckEntry.State = InState;
		CheckEntry.bDisabled = InDisabled;
		CheckEntry.IconName = InIconName;
		CheckEntry.IconToolTip = InIconToolTip;
		CheckBoxListEntries.AddItem(CheckEntry);
	}

	/**
	 * Function to get the results
	 */
	void GetResults (OUT TArray<ObjectType>& OutObjects, const INT InDesiredSelectionState)
	{
		for (INT i = 0; i < CheckBoxListEntries.Num(); ++i)
		{
			if (CheckBoxListEntries(i).State == InDesiredSelectionState)
			{
				OutObjects.AddItem(CheckBoxListEntries(i).Object);
			}
		}
	}

	/**
	 * Sets the title of the "check all" button
	 * Intentionally not exposed in the dialog version as that window should have enough specific text.
	 */
	void SetCheckAllButtonTitle (const FString& InTitle)
	{
		check(CheckAllButton);
		CheckAllButton->SetLabel(*InTitle);
	}
	/**
	 * Sets the title of the "un-check all" button
	 * Intentionally not exposed in the dialog version as that window should have enough specific text.
	 */
	void SetUncheckAllButtonTitle (const FString& InTitle)
	{
		check(UncheckAllButton);
		UncheckAllButton->SetLabel(*InTitle);
	}
	/**
	 * Sets visibility for the CheckAllButton
	 * @param bShow - if true, the window will be shown
	 */
	void ShowCheckAllButton(const UBOOL bInShow)
	{
		check(CheckAllButton);
		CheckAllButton->Show(bInShow ? true : false);
	}
	/**
	 * Sets visibility for the UncheckAllButton
	 * @param bShow - if true, the window will be shown
	 */
	void ShowUncheckAllButton(const UBOOL bInShow)
	{
		check(CheckAllButton);
		UncheckAllButton->Show(bInShow : true : false);
	}

private:
	/**
	 * Sets the checked state of a check box
	 * 
	 * @param EntryIndex	The index of the check box to change
	 * @param bShouldCheck	If true the check box should be checked.  Note: if the check box is disabled the check box will use the "undetermined" state
	 */
	void SetChecked( INT EntryIndex, UBOOL bShouldCheck )
	{
		check( IsWithin<INT>( EntryIndex, 0, CheckBoxListEntries.Num() ) );

		CheckBoxListEntry<ObjectType>& Entry = CheckBoxListEntries( EntryIndex );
		if( bShouldCheck && Entry.bDisabled == TRUE )
		{
			Entry.State = wxCHK_UNDETERMINED;
		}
		else if( bShouldCheck )
		{
			Entry.State = wxCHK_CHECKED;
		}
		else
		{
			Entry.State = wxCHK_UNCHECKED;
		}
	}

	/**
	 * Repopulates the check boxes with the proper values
	 */
	void RefreshAllChecks (void)
	{
		check(CheckListWindow);
		//update the state of the checks
		for (int i = 0; i < CheckBoxListEntries.Num(); ++i)
		{
			check(CheckBoxListEntries(i).CheckBox);
			CheckBoxListEntries(i).CheckBox->Set3StateValue(static_cast<wxCheckBoxState >(CheckBoxListEntries(i).State));
		}

		//pass it up that a refresh just happened
		wxCommandEvent Event;
		Event.SetEventType(ID_UI_REFRESH_CHECK_LIST_BOX);
		Event.SetEventObject(this);
		GetEventHandler()->AddPendingEvent(Event);
	}

	/**
	 * wxWindow version of check box list box
	 */
	wxScrolledWindow* CheckListWindow;
	/**
	 * Button that checks all check boxes
	 */
	wxButton* CheckAllButton;
	/**
	 * Button that unchecks all check boxes
	 */
	wxButton* UncheckAllButton;
	
	/**
	 * Index of last clicked check box for shift select support 
	 */
	INT LastClickedIndex;

	/**
	 * List of all check box names, IDs, and states
	 */
	TArray < CheckBoxListEntry<ObjectType> > CheckBoxListEntries;
};

/**
  * UnrealEd dialog for a check box list 
  */
template<typename ObjectType>
class WxDlgCheckBoxList : public wxDialog
{
public:
	/**
	 * Default Constructor
	 */
	WxDlgCheckBoxList(void)
	{
		CheckBoxListWindow = new WxCheckBoxListWindow<ObjectType>();

		//nothing is selected
		ActionID = -1;
	}

	/**
	 * Destructor
	 */
	virtual ~WxDlgCheckBoxList(void)
	{
		delete CheckBoxListWindow;
	}

	/**
	 * Event for receiving "Action" button clicks
	 */
	void OnButtonClicked( wxCommandEvent& InEvent )
	{
		INT ButtonIndex = InEvent.GetId() - CheckBoxListButtonBaseIndex;
		check(IsWithin<INT>(ButtonIndex, 0, ButtonEntries.Num()));
		ActionID = ButtonEntries(ButtonIndex).ButtonID;

		EndModal(ActionID);
	}

	/**
	 * Add entry to the list of check boxes
	 * @param InObject - Templatized object to use in the list
	 * @param InEntryName - The name respresenting that entry
	 * @param InState - 0 for off, 1 for on, 2 for undetermined
	 * @param InDisabled - TRUE of the check box should be disabled
	 * @param InIconName - The name of the icon we should place to the left of the checkbox
	 * @param InIconToolTip - The tooltip for the icon.  If the icon name isnt the tooltip will not appear.
	 */
	void AddCheck(ObjectType InObject, const FString& InEntryName, const INT InState, UBOOL InDisabled = FALSE, const FString& InIconName = TEXT(""), const FString& InIconToolTip = TEXT(""))
	{
		check(CheckBoxListWindow);
		CheckBoxListWindow->AddCheck(InObject, InEntryName, InState, InDisabled, InIconName, InIconToolTip);
	}
	/**
	 * Add buttons that signify the results
	 *
	 * @param InButtonId	The ID of the button for recognizing events
	 * @param InButtonName	The text that should be displayed on the button
	 * @param InTooltip	The tooltip associated with the button
	 * @param InDisabled	TRUE if the button should be disabled
	 */
	void AddButton(const INT InButtonID, const FString& InButtonName, const FString& InTooltip, UBOOL InDisabled = FALSE)
	{
		if (InButtonID >= 0)
		{
			CheckBoxButtonEntry ButtonEntry;
			ButtonEntry.ButtonID = InButtonID;
			ButtonEntry.ButtonName = InButtonName;
			ButtonEntry.Tooltip = InTooltip;
			ButtonEntry.bDisabled = InDisabled;
			ButtonEntries.AddItem(ButtonEntry);
		}
		else
		{
			//appMsgf(AMT_OK, TEXT("Invalid ID"));
		}
	}

	/**
	 * Displays the progress dialog. Loads the position of the dialog from config or centers the dialog on the main application
	 *
	 * @param InTitle		The title of the dialog
	 * @param BodyText		The body text to display
	 * @param InSize		The Size of the dialog box
	 * @param ConfigSetting	Optional config setting name where the location of the dialog should be saved (so it appears in the same place between sessions)
	 */
	INT ShowDialog(const FString& InTitle, const FString& BodyText, const FIntPoint& InSize, const FString& ConfigSetting)
	{
		wxSize Size;
		Size.Set(InSize.X, InSize.Y);

		wxDialog::Create(GApp->EditorFrame, -1, *InTitle, wxDefaultPosition, Size);

		//Main Sizer
		wxBoxSizer* MainVerticalSizer = new wxBoxSizer(wxVERTICAL);
		SetSizer(MainVerticalSizer );

		//Main Label
		wxStaticText* BodyLabel = new wxStaticText();
		BodyLabel->Create(this, -1, *BodyText);
		MainVerticalSizer->Add(BodyLabel, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT  | wxALL, StandardCheckBoxListBorder);

		//Check Box Window
		check(CheckBoxListWindow);
		CheckBoxListWindow->Create(this);
		CheckBoxListWindow->EnableScrolling();
		MainVerticalSizer->Add(CheckBoxListWindow, 1, wxALIGN_TOP | wxALIGN_CENTER | wxGROW | wxLEFT | wxRIGHT, StandardCheckBoxListBorder);

		//Create Buttons
		wxBoxSizer* ButtonSizer = new wxBoxSizer(wxHORIZONTAL);
		MainVerticalSizer->Add(ButtonSizer , 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT | wxSHAPED | wxGROW, 0);// | wxSHAPED | wxGROW | wxALL, 0);

		for (int i = 0; i < ButtonEntries.Num(); ++i)
		{
			const CheckBoxButtonEntry& CurrentEntry = ButtonEntries(i);
			wxButton* TempButton = new wxButton();
			TempButton->Create(this, CheckBoxListButtonBaseIndex + i, *ButtonEntries(i).ButtonName);
			TempButton->SetToolTip(*ButtonEntries(i).Tooltip);
			TempButton->Enable(!CurrentEntry.bDisabled);
	

			//callback
			Connect(CheckBoxListButtonBaseIndex + i, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgCheckBoxList::OnButtonClicked) );
			Connect(CheckBoxListButtonBaseIndex + i, wxEVT_UPDATE_UI, wxUpdateUIEventHandler( WxDlgCheckBoxList::OnButtonUpdateUIHelper ) );
			ButtonSizer->Add(TempButton, 1, wxALIGN_RIGHT | wxALL, StandardCheckBoxListBorder);// | wxSHAPED | wxGROW | wxALL, 0);
			if (i==0)
			{
				TempButton->SetFocus();
				SetAffirmativeId(CheckBoxListButtonBaseIndex);
			}
		}

		//have to be two to have an automatic cancel
		if (ButtonEntries.Num() > 1)
		{
			SetEscapeId(CheckBoxListButtonBaseIndex + ButtonEntries.Num()-1 );
		}

		Layout();

		// Center the dialog in the parent window.
		// If a config setting was supplied, this position may change.
		Centre();

		wxRect DefaultRect = GetRect();
		// Load the position of the dialog from config
		FWindowUtil::LoadPosSize(ConfigSetting, this, DefaultRect.x , DefaultRect.y, DefaultRect.width, DefaultRect.height);

		ShowModal();

		// The user may have moved the dialog, save its location
		FWindowUtil::SavePosSize(ConfigSetting, this);

		return ActionID;
	}

	/**
	 * Function to get the results
	 */
	void GetResults (OUT TArray<ObjectType>& OutObjects, const INT InDesiredSelectionState)
	{
		check(CheckBoxListWindow);
		CheckBoxListWindow->GetResults(OutObjects, InDesiredSelectionState);
	}
protected:
	/** virtual update UI event handler function so we can pass update UI events to children  */
	virtual void OnButtonUpdateUI( wxUpdateUIEvent& In ) {}

	/** Non virtual helper function for trapping button UI events. */
	void OnButtonUpdateUIHelper( wxUpdateUIEvent& In )
	{
		// Call virtual version since wxWidgets cannot call virtual functions in event handlers
		OnButtonUpdateUI( In );
	}

	/**
	 * Internal Check Box List Window.  Pre-allocated to let it hold the actual check entries
	 */
	WxCheckBoxListWindow<ObjectType> * CheckBoxListWindow;

	/**
	 * Descriptions of the buttons to display as "action" items that close the dialog.
	 */
	TArray<CheckBoxButtonEntry> ButtonEntries;

	/**
	 * The index in the ButtonEntries Array that was select as a result
	 */
	INT ActionID;
};

/**
* UnrealEd wrapper generic dialog for a check box list with default button OK/Cancel
*/
template<typename ObjectType>
class WxGenericDlgCheckBoxList : public WxDlgCheckBoxList<ObjectType>
{
private:
	void AddDefaultButtons()
	{
		AddButton(wxID_OK, *LocalizeUnrealEd("OK"), *LocalizeUnrealEd("OK"));
		AddButton(wxID_CANCEL, *LocalizeUnrealEd("Cancel"), *LocalizeUnrealEd("Cancel"));
	}

public:
	INT ShowDialog(const FString& InTitle, const FString& BodyText, const FIntPoint& InSize, const FString& ConfigSetting)
	{
		// add default button
		AddDefaultButtons();

		return WxDlgCheckBoxList::ShowDialog(InTitle, BodyText, InSize, ConfigSetting);
	}
};
#endif // __DLG_CHECK_BOX_LIST_H__
