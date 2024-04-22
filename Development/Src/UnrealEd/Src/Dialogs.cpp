/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnTexAlignTools.h"
#include "ScopedTransaction.h"
#include "PropertyWindow.h"
#include "BusyCursor.h"
#include "UnConsoleSupportContainer.h"
#include "Editor.h"

#if WITH_MANAGED_CODE
	#include "ColorPickerShared.h"
#endif

///////////////////////////////////////////////////////////////////////////////
//
// Local classes.
//
///////////////////////////////////////////////////////////////////////////////

class WxSurfacePropertiesPanel : public wxPanel, public FNotifyHook, public FSerializableObject
{
public:
	WxSurfacePropertiesPanel( wxWindow* InParent );

	/**
	 * Called by WxDlgSurfaceProperties::RefreshPages().
	 */
	void RefreshPage();

	////////////////////////////
	// FNotifyHook interface
	virtual void NotifyPostChange( void* Src, UProperty* PropertyThatChanged );

	////////////////////////////
	// FSerializableObject interface
	/**
	 * Since this class holds onto an object reference, it needs to be serialized
	 * so that the objects aren't GCed out from underneath us.
	 *
	 * @param	Ar			The archive to serialize with.
	 */
	virtual void Serialize(FArchive& Ar)
	{
		Ar << LightingChannelsObject;
		Ar << LevelLightmassSettingsObjects;
		Ar << SelectedLightmassSettingsObjects;
	}

private:
	void Pan( INT InU, INT InV );
	void Scale( FLOAT InScaleU, FLOAT InScaleV, UBOOL InRelative );

	void OnU1( wxCommandEvent& In );
	void OnU4( wxCommandEvent& In );
	void OnU16( wxCommandEvent& In );
	void OnU64( wxCommandEvent& In );
	void OnUCustom( wxCommandEvent& In );

	void OnV1( wxCommandEvent& In );
	void OnV4( wxCommandEvent& In );
	void OnV16( wxCommandEvent& In );
	void OnV64( wxCommandEvent& In );
	void OnVCustom( wxCommandEvent& In );

	void OnFlipU( wxCommandEvent& In );
	void OnFlipV( wxCommandEvent& In );
	void OnRot45( wxCommandEvent& In );
	void OnRot90( wxCommandEvent& In );
	void OnRotCustom( wxCommandEvent& In );

	void OnApply( wxCommandEvent& In );
	void OnScaleSimple( wxCommandEvent& In );
	void OnScaleCustom( wxCommandEvent& In );
	void OnLightMapResChange( wxCommandEvent& In );
	void OnAcceptsLightsChange( wxCommandEvent& In );
	void OnAcceptsDynamicLightsChange( wxCommandEvent& In );
	void OnForceLightmapChange( wxCommandEvent& In );
	void OnAlignSelChange( wxCommandEvent& In );
	void OnApplyAlign( wxCommandEvent& In );

	/**
	 * Sets passed in poly flag on selected surfaces.
	 *
 	 * @param PolyFlag	PolyFlag to toggle on selected surfaces
	 * @param Value		Value to set the flag to.
	 */
	void SetPolyFlag( DWORD PolyFlag, UBOOL Value );

	/** Sets lighting channels for selected surfaces to the specified value. */
	void SetLightingChannelsForSelectedSurfaces(DWORD NewBitfield);
	/** Sets Lightmass settings for selected surfaces to the specified value. */
	void SetLightmassSettingsForSelectedSurfaces(FLightmassPrimitiveSettings& InSettings);

	wxPanel* Panel;

	wxRadioButton *SimpleScaleButton;
	wxRadioButton *CustomScaleButton;

	wxComboBox *SimpleCB;

	wxStaticText *CustomULabel;
	wxStaticText *CustomVLabel;

	wxTextCtrl *CustomUEdit;
	wxTextCtrl *CustomVEdit;

	wxCheckBox *RelativeCheck;
	wxTextCtrl *LightMapResEdit;
	UBOOL		bSettingLightMapRes;

	/** Checkbox for PF_AcceptsLights */
	wxCheckBox*	AcceptsLightsCheck;
	/** Checkbox for PF_AcceptsDynamicLights */
	wxCheckBox*	AcceptsDynamicLightsCheck;
	/** Checkbox for PF_ForceLigthMap */
	wxCheckBox* ForceLightMapCheck;

	UBOOL bUseSimpleScaling;
	/** Property window containg texture alignment options. */
	WxPropertyWindowHost* PropertyWindow;
	wxListBox *AlignList;

	/** Property window containing surface lighting channels. */
	WxPropertyWindowHost* ChannelsPropertyWindow;
	/** The lighting channels object embedded in this window. */
	ULightingChannelsObject* LightingChannelsObject;
	/** Property window containing surface Lightmass primitive settings. */
	WxPropertyWindowHost* LightmassPropertyWindow;
	/** The Lightmass primitive settings object embedded in this window. */
	typedef TArray<ULightmassPrimitiveSettingsObject*> TLightmassSettingsObjectArray;
	TArray<TLightmassSettingsObjectArray> LevelLightmassSettingsObjects;
	TLightmassSettingsObjectArray SelectedLightmassSettingsObjects;
};


/*-----------------------------------------------------------------------------
	WxDualFileDialog
-----------------------------------------------------------------------------*/
BEGIN_EVENT_TABLE(WxDualFileDialog, wxDialog)
EVT_CLOSE( WxDualFileDialog::OnClose )
EVT_BUTTON( wxID_OK, WxDualFileDialog::OnOK )
END_EVENT_TABLE()

WxDualFileDialog::WxDualFileDialog(const FString& Title, const FString& InFile1Label, const FString& InFile2Label, const FString& InFileOptionsMask, UBOOL bInMustExist, LONG WindowStyle)				
: wxDialog( GApp->EditorFrame, wxID_ANY, *Title, wxDefaultPosition, wxDefaultSize, WindowStyle ) 
{
	ConstructWidgets(InFile1Label, InFile2Label, InFileOptionsMask, bInMustExist);
}

/** 
 *   Helper for creating each file text ctrl / file open ellipse pair
 * @param ThisDialog - the dialog object
 * @param MainDialogSizer - the topmost sizer containing the widgets to be created
 * @param FileSelection - a description of the widget
 */
void WxDualFileDialog::CreateFileDialog(wxDialog* ThisDialog, wxBoxSizer* MainDialogSizer, FileSelectionData& FileSelection)
{
	wxBoxSizer* FileWidgetSizer = new wxBoxSizer(wxVERTICAL);
	MainDialogSizer->Add(FileWidgetSizer, 0, wxGROW|wxALL, 2);

	wxStaticBox* FileStaticBox = new wxStaticBox(ThisDialog, wxID_ANY, *FileSelection.FileLabel);
	wxStaticBoxSizer* FileStaticBoxSizer = new wxStaticBoxSizer(FileStaticBox, wxHORIZONTAL);
	FileWidgetSizer->Add(FileStaticBoxSizer, 1, wxGROW|wxALL, 5);

	FileSelection.FileTextCtrl = new wxTextCtrl( ThisDialog, FileSelection.TextCtrlID, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	FileStaticBoxSizer->Add(FileSelection.FileTextCtrl, 1, wxGROW|wxALL, 5);
	ADDEVENTHANDLER(FileSelection.TextCtrlID, wxEVT_COMMAND_TEXT_UPDATED, &WxDualFileDialog::OnFilenameTextChanged);
	ADDEVENTHANDLER(FileSelection.TextCtrlID, wxEVT_COMMAND_TEXT_ENTER, &WxDualFileDialog::OnFilenameTextChanged);

	wxButton* FileOpen1 = new wxButton( ThisDialog,  FileSelection.EllipseID, _("..."), wxDefaultPosition, wxSize(30, -1), 0 );
	FileStaticBoxSizer->Add(FileOpen1, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
	ADDEVENTHANDLER(FileSelection.EllipseID, wxEVT_COMMAND_BUTTON_CLICKED, &WxDualFileDialog::OnFileOpenClicked);
}

/** 
 * Constructs the buttons, panels, etc. necessary for the dialog to work
 */
void WxDualFileDialog::ConstructWidgets(const FString& FileLabel1, const FString& FileLabel2, const FString& FileOptionsMask, UBOOL bMustExist)
{
	wxDialog* ThisDialog = this;

	// Main dialog sizer
	wxBoxSizer* MainDialogSizer = new wxBoxSizer(wxVERTICAL);
	ThisDialog->SetSizer(MainDialogSizer);

	// -- First text ctrl / ellipse pair contained within a static text dialog representing one file choice option 
	INT FileDialogIdx = FileSelections.AddZeroed();
	FileSelectionData& FileDialog1 = FileSelections(FileDialogIdx);
	FileDialog1.EllipseID = ID_DUALFILEDIALOG_FILEOPEN1;
	FileDialog1.TextCtrlID = ID_DUALFILEDIALOG_FILENAME1;
	FileDialog1.FileLabel = FileLabel1;
	FileDialog1.FileOptionsMask = FileOptionsMask;
	FileDialog1.bMustExist = bMustExist;
	CreateFileDialog(ThisDialog, MainDialogSizer, FileDialog1);

	// -- Second text ctrl / ellipse pair contained within a static text dialog representing one file choice option 
	FileDialogIdx = FileSelections.AddZeroed();
	FileSelectionData& FileDialog2 = FileSelections(FileDialogIdx);
	FileDialog2.EllipseID = ID_DUALFILEDIALOG_FILEOPEN2;
	FileDialog2.TextCtrlID = ID_DUALFILEDIALOG_FILENAME2;
	FileDialog2.FileLabel = FileLabel2;
	FileDialog2.FileOptionsMask = FileOptionsMask;
	FileDialog2.bMustExist = bMustExist;
	CreateFileDialog(ThisDialog, MainDialogSizer, FileDialog2);

	// OK / CANCEL buttons at the bottom
	wxBoxSizer* OKCancelBarSizer = new wxBoxSizer(wxHORIZONTAL);
	MainDialogSizer->Add(OKCancelBarSizer, 0, wxALIGN_RIGHT|wxALL, 5);

	OKCancelBarSizer->Add(100, 5, 10, wxGROW|wxALL, 5);

	wxButton* itemButton13 = new wxButton( ThisDialog, wxID_OK, _("OK"), wxDefaultPosition, wxDefaultSize, 0 );
	OKCancelBarSizer->Add(itemButton13, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	wxButton* itemButton14 = new wxButton( ThisDialog, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
	OKCancelBarSizer->Add(itemButton14, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	// Localize the contents of the dialog
	FLocalizeWindow( ThisDialog, FALSE, TRUE );
}

/** 
 *   Helper for opening a file dialog with the appropriate information
 * @param LocalizeTitleName - text markup for the string to appear in the dialog
 * @param FileOptionsMask - string describing the filename wildcards/descriptions
 * @param bMustExist - whether or not the file must exist during selection
 * @param FileSelection - the file selected by the user
 * @return TRUE if user selected a file, FALSE otherwise
 */
UBOOL WxDualFileDialog::OpenFileDialog(const FString& LocalizeTitleName, const FString& FileOptionsMask, UBOOL bMustExist, FFilename& FileSelection)
{
	UBOOL bFileChosen = FALSE;

	long Style = wxOPEN;
	if (bMustExist)
	{
		Style |= wxFILE_MUST_EXIST;
	}

	// Display the file open dialog for selecting the base mesh .PSK or .FBX file.
	WxFileDialog ImportFileDialog( this, 
		*LocalizeUnrealEd(*LocalizeTitleName), 
		*(GApp->LastDir[LD_PSA]),
		TEXT(""),
		*FileOptionsMask,
		Style,
		wxDefaultPosition);

	wxArrayString ImportFilePaths;
	// Only continue if we pressed OK and have only one file selected. 
	if( ImportFileDialog.ShowModal() == wxID_OK )
	{			
		ImportFileDialog.GetPaths(ImportFilePaths);
		if( ImportFilePaths.Count() == 1 )
		{
			FileSelection = (const TCHAR*)ImportFilePaths[0];
			GApp->LastDir[LD_PSA] = FileSelection.GetPath(); // Save path as default for next time.
			bFileChosen = TRUE;
		}
	}

	return bFileChosen;
}

/**
 *   @return TRUE if all filenames selected are valid, FALSE otherwise
 */
UBOOL WxDualFileDialog::VerifyFilenames()
{
	UBOOL bFilesOk = TRUE;

	for (INT FileIdx=0; bFilesOk && FileIdx<FileSelections.Num(); FileIdx++)
	{
		FileSelectionData& FileSelection = FileSelections(FileIdx);
		if (FileSelection.bMustExist)
		{
			bFilesOk &= (FileSelection.Filename.Len() > 0 && FileSelection.Filename.FileExists());
		}
	}

	return bFilesOk;
}

/**
* Callback when user pushes the any of the file open buttons (ellipses)
* @param In - Properties of the event triggered
*/
void WxDualFileDialog::OnFileOpenClicked(wxCommandEvent& In)
{
	INT EventID = In.GetId();
	for (INT i=0; i<FileSelections.Num(); i++)
	{
		FileSelectionData& FileSelection = FileSelections(i);
		if (EventID == FileSelection.EllipseID)
		{
			if (OpenFileDialog(FileSelection.FileLabel, FileSelection.FileOptionsMask, FileSelection.bMustExist, FileSelection.Filename))
			{
				// Transfer the choice into the text dialog
				FileSelection.FileTextCtrl->SetValue(*FileSelection.Filename);
			}
			break;
		}
	}
}

/**
 * Callback when user types text into a filename dialog
 * @param In - Properties of the event triggered
 */
void WxDualFileDialog::OnFilenameTextChanged(wxCommandEvent& In)
{	
	wxObject* EventObject = In.GetEventObject();
	wxTextCtrl* TextCtrl = wxDynamicCast(EventObject, wxTextCtrl);
	if (TextCtrl)
	{
		for (INT i=0; i<FileSelections.Num(); i++)
		{
			FileSelectionData& FileSelection = FileSelections(i);
			if (TextCtrl == FileSelection.FileTextCtrl)
			{
				FileSelection.Filename = TextCtrl->GetValue().GetData();
				break;
			}	
		}
	}
}

/**
 * Callback when the OK button is clicked, shuts down the options panel
 * @param In - Properties of the event triggered
 */
void WxDualFileDialog::OnOK( wxCommandEvent& In )
{
	if (VerifyFilenames())
	{
		wxDialog::AcceptAndClose();
	}
	else
	{
		appMsgf( AMT_OK, TEXT("Invalid file(s) specified.") );
	}
}

/**
 * Callback when the window is closed, shuts down the options panel
 * @param In - Properties of the event triggered
 */
void WxDualFileDialog::OnClose(wxCloseEvent& In)
{
}

/*-----------------------------------------------------------------------------
	WxChoiceDialogBase
-----------------------------------------------------------------------------*/
BEGIN_EVENT_TABLE(WxChoiceDialogBase, wxDialog)
// Using Connect() instead of event table; see constructor
END_EVENT_TABLE()

/**
 * Base class for choice-based dialogs. Should not be used on its own, but instead
 * inherited from to provide choice-based dialogs with potentially dramatically
 * different main panels (therefore has pure virtual destructor to prevent instantiation). 
 * Inheriting classes should implement their primary "window" as a new wxPanel and then 
 * send it to WxChoiceDialogBase::ConstructWidgets in order to properly build the dialog. 
 * ConstructWidgets will place the panel above the appropriate buttons with a sizer. 
 * See WxChoiceDialog as an example.
 */
WxChoiceDialogBase::WxChoiceDialogBase(
	  FString Title
	, LONG WindowStyle
	, const WxChoiceDialogBase::Choice& InChoiceOne
	, const WxChoiceDialogBase::Choice& InChoiceTwo/* = Choice() */
	, const WxChoiceDialogBase::Choice& InChoiceThree/* = Choice() */
	, const WxChoiceDialogBase::Choice& InChoiceFour /* = Choice() */
	, const WxChoiceDialogBase::Choice& InChoiceFive /* = Choice() */)
	: wxDialog( GApp->EditorFrame, wxID_ANY, *Title, wxDefaultPosition, wxDefaultSize, WindowStyle )
{
	// If the parent window isn't shown, this dialog window can become "lost" if we click around
	// Fix this by forcing the window to the top instead
	wxWindow* Parent = GetParent();
	if( Parent )
	{
		if( !Parent->IsShownOnScreen() )
		{
			SetWindowStyleFlag( WindowStyle | wxSTAY_ON_TOP );
			Refresh();
		}
	}

	// Store the choices specified by user.
	Choices.AddItem( InChoiceOne );

	if ( InChoiceTwo.IsChoiceActive() )
	{
		Choices.AddItem( InChoiceTwo );
	}

	if ( InChoiceThree.IsChoiceActive() )
	{
		Choices.AddItem( InChoiceThree );
	}

	if ( InChoiceFour.IsChoiceActive() )
	{
		Choices.AddItem( InChoiceFour );
	}

	if ( InChoiceFive.IsChoiceActive() )
	{
		Choices.AddItem( InChoiceFive );
	}

	// Register event handlers
	this->Connect( wxID_ANY, wxEVT_CLOSE_WINDOW, wxCloseEventHandler(WxChoiceDialogBase::OnClose) );
	this->Connect( GetButtonIdFor(0), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxChoiceDialogBase::ButtonOneChosen) );
	this->Connect( GetButtonIdFor(1), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxChoiceDialogBase::ButtonTwoChosen) );
	this->Connect( GetButtonIdFor(2), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxChoiceDialogBase::ButtonThreeChosen) );
	this->Connect( GetButtonIdFor(3), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxChoiceDialogBase::ButtonFourChosen) );
	this->Connect( GetButtonIdFor(4), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxChoiceDialogBase::ButtonFiveChosen) );
}


/** 
 * Pure virtual destructor so others can inherit from this dialog box, but not instantiate one
 * NOTE: An implementation is still required in this case to allow inherited classes to successfully destroy themselves 
 */
WxChoiceDialogBase::~WxChoiceDialogBase() {}

/** Returns the choice made by the user (will be DefaultChoice until the dialog is closed) */
const WxChoiceDialogBase::Choice& WxChoiceDialogBase::GetChoice()
{
	return Choices(UserChoice);
}

/** Gets a button ID for a button associated with a specific choice */
INT WxChoiceDialogBase::GetButtonIdFor( INT InChoiceID )
{
	// We are picking some wxIDs that 
	static INT ButtonIDs[] = { wxID_HIGHEST, wxID_HIGHEST+1, wxID_HIGHEST+2, wxID_HIGHEST+3, wxID_HIGHEST+4 };
	return ButtonIDs[InChoiceID];
}

/** 
 * Constructs the buttons, panels, etc. necessary for the dialog to work
 *
 * @param InPanel - Panel to use for the top part of the dialog, this allows child classes to specify unique/special panels in the dialog
 */
void WxChoiceDialogBase::ConstructWidgets(wxPanel* InPanel)
{
	// VerticalSizer is the top-level sizer; it will contain the InPanel on top and the HorizontalSizer with buttons on the bottom
	wxBoxSizer* TopSizer = new wxBoxSizer(wxVERTICAL);
	this->SetSizer( TopSizer );

	TopSizer->Add(InPanel, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL|wxALIGN_LEFT, 0 );

	// Create the buttons and arrange them in a horizontal sizer.
	TArray< wxButton* > Buttons;
	wxBoxSizer* HorizontalSizer = new wxBoxSizer(wxHORIZONTAL);
	for (int ChoiceIndex = 0; ChoiceIndex < Choices.Num(); ++ChoiceIndex )
	{
		wxButton* NewButton = new wxButton( this, GetButtonIdFor(ChoiceIndex), *(Choices(ChoiceIndex).ButtonText) );
		Buttons.AddItem( NewButton );
		HorizontalSizer->Add( NewButton, 1, wxALIGN_RIGHT );
		HorizontalSizer->SetItemMinSize( NewButton, 80, 28 );
		HorizontalSizer->AddSpacer(10);
	}

	TopSizer->Add(HorizontalSizer, 0, wxALL|wxALIGN_RIGHT, 10 );
	TopSizer->Fit(this);

	// Determine the defaults specified by user.
	INT DefaultAffirmativeChoiceID = INDEX_NONE;
	INT DefaultCancelChoiceID = INDEX_NONE;
	for (INT ChoiceIndex = 0; ChoiceIndex < Choices.Num(); ++ChoiceIndex)
	{
		if ( Choices(ChoiceIndex).TypeOfChoice == DCT_DefaultAffirmative )
		{
			DefaultAffirmativeChoiceID = ChoiceIndex;
		}
		else if ( Choices(ChoiceIndex).TypeOfChoice == DCT_DefaultCancel )
		{
			DefaultCancelChoiceID = ChoiceIndex;
		}
	}

	if (Choices.Num() == 1)
	{
		if (DefaultCancelChoiceID == INDEX_NONE)
		{
			DefaultCancelChoiceID = DefaultAffirmativeChoiceID;
		}
		else //(DefaultAffirmativeChoiceID == INDEX_NONE)
		{
			DefaultAffirmativeChoiceID = DefaultCancelChoiceID;
		}
	}

	// You must specify the default affirmative choice and the default cancel choice.
	// These are the choices to be used when the user presses enter and escape respectively.
	check( DefaultAffirmativeChoiceID != INDEX_NONE );
	check( DefaultCancelChoiceID != INDEX_NONE );

	Buttons( DefaultAffirmativeChoiceID )->SetFocus();

	this->UserChoice = DefaultCancelChoiceID;
	SetAffirmativeId( GetButtonIdFor(DefaultAffirmativeChoiceID) );
	SetEscapeId( GetButtonIdFor(DefaultCancelChoiceID) );

	Centre();
}

/** Window closed event handler. */
void WxChoiceDialogBase::OnClose( wxCloseEvent& Event )
{
	this->Hide();

	//wxDialog::
	Event.Veto();
}

/** Handle user clicking on button one */
void WxChoiceDialogBase::ButtonOneChosen( wxCommandEvent& In )
{
	UserChoice = 0;
	wxDialog::AcceptAndClose();
}

/** Handle user clicking on button two */
void WxChoiceDialogBase::ButtonTwoChosen( wxCommandEvent& In )
{
	UserChoice = 1;
	wxDialog::AcceptAndClose();
}

/** Handle user clicking on button three */
void WxChoiceDialogBase::ButtonThreeChosen( wxCommandEvent& In )
{
	UserChoice = 2;
	wxDialog::AcceptAndClose();
}

/** Handle user clicking on button four */
void WxChoiceDialogBase::ButtonFourChosen( wxCommandEvent& In )
{
	UserChoice = 3;
	wxDialog::AcceptAndClose();
}


/** Handle user clicking on button five */
void WxChoiceDialogBase::ButtonFiveChosen( wxCommandEvent& In )
{
	UserChoice = 4;
	wxDialog::AcceptAndClose();
}

/*-----------------------------------------------------------------------------
	WxChoiceDialog
-----------------------------------------------------------------------------*/

/**
 * Static function that creates a WxChoiceDialog in one of several standard configurations
 * and immediately prompts the user with the provided message
 * (This mimics appMsgf's functionality; appMsgf forwards to this in editor)
 *
 * @param InMessage	Message to display on the dialog
 * @param InType		Type of dialog to display (YesNo, OKCancel, etc.)
 */
INT WxChoiceDialog::WxAppMsgF( const FString& InMessage, EAppMsgType InType )
{
	const FString Title( LocalizeUnrealEd( TEXT("GenericDialog_WindowTitle") ) );

	// Force a release of the mouse to handle cases such as the down event of
	// a mouse click are calling appMsgf
	::ReleaseCapture();

	switch( InType )
	{
	case AMT_YesNo:
		{
			WxChoiceDialog MyDialog(
				InMessage, Title,
				WxChoiceDialogBase::Choice( 1, LocalizeUnrealEd( TEXT("GenericDialog_Yes") ), DCT_DefaultAffirmative ),
				WxChoiceDialogBase::Choice( 0, LocalizeUnrealEd( TEXT("GenericDialog_No") ), DCT_DefaultCancel ) );
			
			MyDialog.ShowModal();
			return MyDialog.GetChoice().ReturnCode;
		}
		break;

	case AMT_OKCancel:
		{
			WxChoiceDialog MyDialog(
				InMessage, Title,
				WxChoiceDialogBase::Choice( 1, LocalizeUnrealEd( TEXT("GenericDialog_OK") ), DCT_DefaultAffirmative ),
				WxChoiceDialogBase::Choice( 0, LocalizeUnrealEd( TEXT("GenericDialog_Cancel") ), DCT_DefaultCancel ) );
			
			MyDialog.ShowModal();
			return MyDialog.GetChoice().ReturnCode;
		}
		break;

	case AMT_YesNoCancel:
		{
			WxChoiceDialog MyDialog(
				InMessage, Title,
				WxChoiceDialogBase::Choice( 0, LocalizeUnrealEd( TEXT("GenericDialog_Yes") ), DCT_DefaultAffirmative ),
				WxChoiceDialogBase::Choice( 1, LocalizeUnrealEd( TEXT("GenericDialog_No") ) ),
				WxChoiceDialogBase::Choice( 2, LocalizeUnrealEd( TEXT("GenericDialog_Cancel") ), DCT_DefaultCancel ) );

			MyDialog.ShowModal();
			return MyDialog.GetChoice().ReturnCode;
		}
		break;

	case AMT_CancelRetryContinue:
		{
			WxChoiceDialog MyDialog(
				InMessage, Title,
				WxChoiceDialogBase::Choice( 0, LocalizeUnrealEd( TEXT("GenericDialog_Cancel") ), DCT_DefaultCancel ),
				WxChoiceDialogBase::Choice( 1, LocalizeUnrealEd( TEXT("GenericDialog_Retry") ), DCT_DefaultAffirmative ),
				WxChoiceDialogBase::Choice( 2, LocalizeUnrealEd( TEXT("GenericDialog_Continue") ) ) );

			MyDialog.ShowModal();
			return MyDialog.GetChoice().ReturnCode;
		}
		break;
	
	case AMT_YesNoYesAllNoAll:
		{
			WxChoiceDialog MyDialog(
				InMessage, Title,
				WxChoiceDialogBase::Choice( 1, LocalizeUnrealEd( TEXT("GenericDialog_Yes") ), DCT_DefaultAffirmative ),
				WxChoiceDialogBase::Choice( 2, LocalizeUnrealEd( TEXT("GenericDialog_YesToAll") ) ),
				WxChoiceDialogBase::Choice( 0, LocalizeUnrealEd( TEXT("GenericDialog_No") ), DCT_DefaultCancel ),
				WxChoiceDialogBase::Choice( 3, LocalizeUnrealEd( TEXT("GenericDialog_NoToAll") ) ) );
			
			MyDialog.ShowModal();
			return MyDialog.GetChoice().ReturnCode;
		}
		break;

	case AMT_YesNoYesAllNoAllCancel:
		{
			WxChoiceDialog MyDialog(
				InMessage, Title,
				WxChoiceDialogBase::Choice( 1, LocalizeUnrealEd( TEXT("GenericDialog_Yes") ), DCT_DefaultAffirmative ),
				WxChoiceDialogBase::Choice( 2, LocalizeUnrealEd( TEXT("GenericDialog_YesToAll") ) ),
				WxChoiceDialogBase::Choice( 0, LocalizeUnrealEd( TEXT("GenericDialog_No") ) ),				
				WxChoiceDialogBase::Choice( 3, LocalizeUnrealEd( TEXT("GenericDialog_NoToAll") ) ),
				WxChoiceDialogBase::Choice( 4, LocalizeUnrealEd( TEXT("GenericDialog_Cancel") ), DCT_DefaultCancel ) );

			MyDialog.ShowModal();
			return MyDialog.GetChoice().ReturnCode;
		}
		break;
	default:
		{
			WxChoiceDialog MyDialog(
				InMessage, Title,
				Choice( 1, LocalizeUnrealEd( TEXT("GenericDialog_OK") ), DCT_DefaultAffirmative )
			);
			
			MyDialog.ShowModal();
			return MyDialog.GetChoice().ReturnCode;
		}
		break;
	}
}

/**
 * Construct a Choice Dialog
 *
 * @param Prompt			The message that appears to the user
 * @param Title			The title of the dialog
 * @param InChoiceOne	Text and return code associated with the first button.
 * @param InChoiceTwo	Optional text and return code associated with the second button.
 * @param InChoiceThree	Optional text and return code associated with the third button.
 * @param InChoiceFour	Optional text and return code associated with the fourth button.
 * @param InChoiceFive	Optional text and return code associated with the fifth button.	 
 */
WxChoiceDialog::WxChoiceDialog(
		  FString Prompt
		, FString Title
		, const WxChoiceDialogBase::Choice& InChoiceOne
		, const WxChoiceDialogBase::Choice& InChoiceTwo/* = Choice() */
		, const WxChoiceDialogBase::Choice& InChoiceThree/* = Choice() */
		, const WxChoiceDialogBase::Choice& InChoiceFour /* = Choice() */
		, const WxChoiceDialogBase::Choice& InChoiceFive /* = Choice() */)
	: WxChoiceDialogBase( Title, wxDEFAULT_DIALOG_STYLE&~wxCLOSE_BOX, InChoiceOne, InChoiceTwo, InChoiceThree, InChoiceFour, InChoiceFive )
{
	// Create the TextPanel and make its background white.
	wxPanel* TextPanel = new wxPanel(this);
	TextPanel->SetBackgroundColour(*wxWHITE);
	
	// Put the prompt text into the TextPanel; use a sizer to align the text
	wxBoxSizer* TextSizer = new wxBoxSizer(wxHORIZONTAL);
	TextPanel->SetSizer(TextSizer);

	TextSizer->Add(10, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	// Create the prompt text as static text on the TextPanel
	wxStaticText* PromptStaticText = new wxStaticText(TextPanel, wxID_ANY, *Prompt, wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);	
	PromptStaticText->Wrap(450);
	TextSizer->Add(PromptStaticText, 0, wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 25 );

	TextSizer->Add(10, 5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);	
	TextSizer->Fit(TextPanel);

	// Construct the remaining widgets using the new text panel as the top of the window
	ConstructWidgets(TextPanel);
}

/** Virtual destructor so others can inherit from this dialog box */
WxChoiceDialog::~WxChoiceDialog() {}

/*-----------------------------------------------------------------------------
	WxLongChoiceDialog
-----------------------------------------------------------------------------*/

/**
 * Construct a Long Choice Dialog
 *
 * @param Prompt			The message that appears to the user
 * @param Title			The title of the dialog
 * @param InChoiceOne	Text and return code associated with the first button.
 * @param InChoiceTwo	Optional text and return code associated with the second button.
 * @param InChoiceThree	Optional text and return code associated with the third button.
 * @param InChoiceFour	Optional text and return code associated with the fourth button.
 * @param InChoiceFive	Optional text and return code associated with the fifth button.	 
 */
WxLongChoiceDialog::WxLongChoiceDialog(
		  FString Prompt
		, FString Title
		, const WxChoiceDialogBase::Choice& InChoiceOne
		, const WxChoiceDialogBase::Choice& InChoiceTwo/* = Choice() */
		, const WxChoiceDialogBase::Choice& InChoiceThree/* = Choice() */
		, const WxChoiceDialogBase::Choice& InChoiceFour /* = Choice() */
		, const WxChoiceDialogBase::Choice& InChoiceFive /* = Choice() */)
	: WxChoiceDialogBase( Title, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER, InChoiceOne, InChoiceTwo, InChoiceThree, InChoiceFour, InChoiceFive )
{
	// Create the TextPanel and make its background white.
	wxPanel* TextPanel = new wxPanel(this);
	TextPanel->SetBackgroundColour(*wxWHITE);

	// Put the prompt text into the TextPanel; use a sizer to align the text
	wxBoxSizer* TextSizer = new wxBoxSizer(wxHORIZONTAL);
	TextPanel->SetSizer(TextSizer);

	// Create the prompt text as a text control on the TextPanel
	wxTextCtrl* PromptText = new wxTextCtrl( TextPanel, wxID_ANY, *Prompt, wxDefaultPosition, wxSize(550, 400), wxTE_MULTILINE | wxTE_READONLY );
	TextSizer->Add(PromptText, 1, wxEXPAND|wxALL, 5 );

	// Construct the remaining widgets using the new text panel as the top of the window
	ConstructWidgets(TextPanel);

	// Set the minimum size of the window to be the size it is constructed at in order to prevent
	// the user from sizing the window so small it covers up buttons, etc.
	SetMinSize(GetSize());
}

/** Virtual destructor so others can inherit from this dialog box */
WxLongChoiceDialog::~WxLongChoiceDialog() {}


/*-----------------------------------------------------------------------------
	WxModelessPrompt
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxModelessPrompt, wxDialog )
	EVT_CLOSE( WxModelessPrompt::OnClose )
	EVT_BUTTON( wxID_OK, WxModelessPrompt::OnOk )
END_EVENT_TABLE()

/**
 * Construct a WxModelessPrompt object
 *
 * @param	Prompt	Message to prompt the user with
 * @param	Title	Title of the dialog
 */
WxModelessPrompt::WxModelessPrompt( const FString& Prompt, const FString& Title )
: wxDialog( GApp->EditorFrame, wxID_ANY, *Title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER )
{
	// Create a sizer for the entire dialog
	wxBoxSizer* MainSizer = new wxBoxSizer( wxVERTICAL );
	this->SetSizer( MainSizer );

	// Create a new panel to house the text control
	wxPanel* TextPanel = new wxPanel( this );
	TextPanel->SetBackgroundColour( *wxWHITE );

	wxBoxSizer* TextSizer = new wxBoxSizer( wxHORIZONTAL );
	TextPanel->SetSizer( TextSizer );

	// Create a text control to house the prompt
	wxTextCtrl* PromptText = new wxTextCtrl( TextPanel, wxID_ANY, *Prompt, wxDefaultPosition, wxSize( 550, 400 ), wxTE_MULTILINE | wxTE_READONLY );
	TextSizer->Add( PromptText, 1, wxEXPAND | wxALL, 5 );

	MainSizer->Add( TextPanel, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT, 0 );

	// Create a simple OK button and sizer for it
	wxSizer* ButtonSizer = CreateButtonSizer( wxOK );
	MainSizer->Add( ButtonSizer, 0, wxALL | wxALIGN_RIGHT, 10 );
	
	MainSizer->Fit(this);
	SetMinSize( wxSize( 200, 200 ) );
	Center();
}

/**
 * Overridden from the base wxDialog version; intentionally asserts and is made private
 * to prevent its use
 */
int WxModelessPrompt::ShowModal()
{
	checkf( 0, TEXT("Modeless Prompt should not be used modally, as it deletes itself upon close!") );
	return 0;
}

/**
 * Called automatically by wxWidgets when the user presses the OK button
 *
 * @param	Event	Event automatically generated by wxWidgets when the user presses the OK button
 */
void WxModelessPrompt::OnOk( wxCommandEvent& Event )
{
	// Hide the window and destroy it
	Show( FALSE );
	Destroy();
}

/**
 * Called automatically by wxWidgets when the user closes the window (such as via the X)
 *
 * @param	Event	Event automatically generated by wxWidgets when the user closes the window
 */
void WxModelessPrompt::OnClose( wxCloseEvent& Event )
{
	// Destroy the window
	Destroy();
}

/*-----------------------------------------------------------------------------
WxSuppressableWarningDialog
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxSuppressableWarningDialog, wxDialog)
	EVT_BUTTON(wxID_OK, WxSuppressableWarningDialog::OnYes)
	EVT_BUTTON(wxID_YES, WxSuppressableWarningDialog::OnYes)
	EVT_BUTTON(wxID_NO, WxSuppressableWarningDialog::OnNo)
END_EVENT_TABLE()

/**
 * Constructs a warning dialog that can be suppressed.
 *
 * @param Prompt			The message that appears to the user
 * @param Title				The title of the dialog
 * @param InIniSettingName	The name of the entry in the EditorUserSettings INI were the state of the "Disable this warning" check box is stored
 * @param IncludeYesNoButtons If TRUE, Yes/No buttons will be used for the warning rather than just an OK button
 */
WxSuppressableWarningDialog::WxSuppressableWarningDialog( const FString& InPrompt, const FString& InTitle, const FString& InIniSettingName, UBOOL IncludeYesNoButtons )
	: wxDialog( GApp->EditorFrame, wxID_ANY, *InTitle, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE ), IniSettingName( InIniSettingName ), Prompt( InPrompt )
{
	// Create a panel with a scrollable text box. This contains the warning prompt
	wxPanel* TextPanel = new wxPanel(this);
	wxBoxSizer* TextSizer = new wxBoxSizer(wxHORIZONTAL);
	TextPanel->SetSizer(TextSizer);

	// Set the text control for the warning prompt
	wxTextCtrl* PromptText = new wxTextCtrl( TextPanel, wxID_ANY, *Prompt, wxDefaultPosition, wxSize(200,200), wxTE_READONLY | wxTE_MULTILINE );
	TextSizer->Add(PromptText, 1, wxEXPAND|wxALL, 5 );

	// Create the main sizer for this dialog
	wxBoxSizer* DialogSizer = new wxBoxSizer(wxVERTICAL);

	// Add the text panel to the sizer
	DialogSizer->Add(TextPanel, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL|wxALIGN_LEFT, 0 );

	// Create the sizer for the check box and ok buttons
	wxBoxSizer* ButtonSizer = new wxBoxSizer(wxHORIZONTAL);

	// Add yes/no/ok buttons as required
	OkButton = YesButton = NoButton = NULL;
	if( IncludeYesNoButtons )
	{
		YesButton = new wxButton( this, wxID_YES, *LocalizeUnrealEd("Yes") );
		NoButton = new wxButton( this, wxID_NO, *LocalizeUnrealEd("No") );
		YesButton->SetFocus();
	}
	else
	{
		OkButton = new wxButton( this, wxID_OK, *LocalizeUnrealEd("Ok") );
		OkButton->SetFocus();
	}

	// Check box for disabling this warning from appearing
	DisableCheckBox = new wxCheckBox( this, wxID_ANY, *LocalizeUnrealEd( TEXT("SuppressableWarningDlg_DisableThisWarning") ) );

	ButtonSizer->AddSpacer( 10 );
	// The disable check box should be aligned with the text in the OK button and not the top of the button
	ButtonSizer->Add( DisableCheckBox,1, wxTOP| wxRIGHT, 14 );
	// The disable check box should be to the left of the ok button with a liberal amout of space to separate the two buttons
	ButtonSizer->Add( 75,0 );
	if( YesButton )
	{
		ButtonSizer->Add( YesButton, 1, wxALL, 10 );
	}
	if( NoButton )
	{
		ButtonSizer->Add( NoButton, 1, wxALL, 10 );
	}
	if( OkButton )
	{
		ButtonSizer->Add( OkButton, 1, wxALL, 10 );
	}

	DialogSizer->Add(ButtonSizer, 0, wxALL|wxALIGN_BOTTOM );

	Centre();
	SetSizerAndFit( DialogSizer );
}

/**
 * Displays the warning dialog if the user did not disable being prompted for this warning message.
 * If the warning message is displayed the state of the disable check box will be saved to the user settings ini in the location passed into the constructor
 */
INT WxSuppressableWarningDialog::ShowModal( )
{
	const FString ConfigSection = TEXT("SuppressableDialogs");
	// Assume we should not suppress the dialog
	UBOOL bShouldSuppressDialog = FALSE;
	// Get the setting from the config file.
	GConfig->GetBool( *ConfigSection, *IniSettingName, bShouldSuppressDialog, GEditorUserSettingsIni );

	INT RetCode = 0;
	if( !bShouldSuppressDialog )
	{
		RetCode = wxDialog::ShowModal();

		if( RetCode == wxID_OK )
		{
			// Set the ini variable to the state of the disable check box
			bShouldSuppressDialog = DisableCheckBox->IsChecked();
			GConfig->SetBool( *ConfigSection, *IniSettingName, bShouldSuppressDialog, GEditorUserSettingsIni );
		}
	}
	else
	{
		// If the dialog is suppressed, log the warning
		GWarn->Logf( *Prompt );
	}

	return RetCode;
}

void WxSuppressableWarningDialog::OnYes(wxCommandEvent &Event)
{
	EndModal( wxID_OK );
}
void WxSuppressableWarningDialog::OnNo(wxCommandEvent &Event)
{
	EndModal( wxID_CANCEL );
}

WxSuppressableWarningDialog::~WxSuppressableWarningDialog()
{
	// All allocations in the constructor are deleted by wxWidgets
}

/**
 * WxDlgBindHotkeys
 */
namespace
{
	static const wxColour DlgBindHotkeys_SelectedBackgroundColor(255,218,171);
	static const wxColour DlgBindHotkeys_LightColor(227, 227, 239);
	static const wxColour DlgBindHotkeys_DarkColor(200, 200, 212);
}


class WxKeyBinder : public wxTextCtrl
{
public:
	WxKeyBinder(wxWindow* Parent, class WxDlgBindHotkeys* InParentDlg) : 
	  wxTextCtrl(Parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY),
	  ParentDlg(InParentDlg)
	{

	}

private:

	/** Handler for when a key is pressed or released. */
	void OnKeyEvent(wxKeyEvent& Event);

	/** Handler for when focus is lost */
	void OnKillFocus(wxFocusEvent& Event);

	/** Pointer to the parent of this class. */
	class WxDlgBindHotkeys* ParentDlg;

	DECLARE_EVENT_TABLE()
};


BEGIN_EVENT_TABLE(WxKeyBinder, wxTextCtrl)
	EVT_KEY_DOWN(OnKeyEvent)
	EVT_KEY_UP(OnKeyEvent)
	EVT_KILL_FOCUS(OnKillFocus)
END_EVENT_TABLE()


/** Handler for when a key is pressed or released. */
void WxKeyBinder::OnKeyEvent(wxKeyEvent& Event)
{
	if(ParentDlg->IsBinding())
	{
		if(Event.GetKeyCode() != WXK_SHIFT && Event.GetKeyCode() != WXK_CONTROL && Event.GetKeyCode() != WXK_ALT && Event.GetEventType() == wxEVT_KEY_DOWN)
		{
			FName Key = GApp->GetKeyName(Event);

			if(Key==KEY_Escape)
			{
				ParentDlg->StopBinding();
			}
			else
			{
				ParentDlg->FinishBinding(Event);
			}
		}
		else
		{
			FString BindString = ParentDlg->GenerateBindingText(Event.AltDown(), Event.ControlDown(), Event.ShiftDown(), NAME_None);
			SetValue(*BindString);
		}
	}
}

/** Handler for when focus is lost */
void WxKeyBinder::OnKillFocus(wxFocusEvent& Event)
{
	ParentDlg->StopBinding();
}

BEGIN_EVENT_TABLE(WxDlgBindHotkeys, wxFrame)
	EVT_CLOSE(OnClose)
	EVT_BUTTON(ID_DLGBINDHOTKEYS_LOAD_CONFIG, OnLoadConfig)
	EVT_BUTTON(ID_DLGBINDHOTKEYS_SAVE_CONFIG, OnSaveConfig)
	EVT_BUTTON(ID_DLGBINDHOTKEYS_RESET_TO_DEFAULTS, OnResetToDefaults)
	EVT_BUTTON(wxID_OK, OnOK)
	EVT_TREE_SEL_CHANGED(ID_DLGBINDHOTKEYS_TREE, OnCategorySelected)
	EVT_COMMAND_RANGE(ID_DLGBINDHOTKEYS_BIND_KEY_START, ID_DLGBINDHOTKEYS_BIND_KEY_END, wxEVT_COMMAND_BUTTON_CLICKED, OnBindKey)
	EVT_KEY_DOWN(OnKeyDown)
	EVT_CHAR(OnKeyDown)
END_EVENT_TABLE()

WxDlgBindHotkeys::WxDlgBindHotkeys(wxWindow* InParent) : 
wxFrame(InParent, wxID_ANY, *LocalizeUnrealEd("DlgBindHotkeys_Title"),wxDefaultPosition, wxSize(640,480), wxDEFAULT_FRAME_STYLE | wxWANTS_CHARS),
ItemSize(20),
NumVisibleItems(0),
CommandPanel(NULL),
CurrentlyBindingIdx(-1)
{
	wxSizer* PanelSizer = new wxBoxSizer(wxHORIZONTAL);
	{		
		wxPanel* MainPanel = new wxPanel(this);
		{
			wxSizer* MainSizer = new wxBoxSizer(wxVERTICAL);
			{
				MainSplitter = new wxSplitterWindow(MainPanel);
				{
					CommandPanel = new wxScrolledWindow(MainSplitter);
					CommandCategories = new WxTreeCtrl(MainSplitter, ID_DLGBINDHOTKEYS_TREE, NULL, wxTR_HIDE_ROOT | wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT);
					
					MainSplitter->SplitVertically(CommandCategories, CommandPanel);
					MainSplitter->SetMinimumPaneSize(128);
				}
				MainSizer->Add(MainSplitter, 1, wxEXPAND | wxALL, 2);

				wxBoxSizer* HSizer = new wxBoxSizer(wxHORIZONTAL);
				{
					wxButton* LoadButton = new wxButton(MainPanel, ID_DLGBINDHOTKEYS_LOAD_CONFIG, *LocalizeUnrealEd("LoadConfig"));
					wxButton* SaveButton = new wxButton(MainPanel, ID_DLGBINDHOTKEYS_SAVE_CONFIG, *LocalizeUnrealEd("SaveConfig"));
					wxButton* ResetToDefaults = new wxButton(MainPanel, ID_DLGBINDHOTKEYS_RESET_TO_DEFAULTS, *LocalizeUnrealEd("ResetToDefaults"));
					BindLabel = new wxStaticText(MainPanel, wxID_ANY, TEXT(""));
					BindLabel->GetFont().SetWeight(wxFONTWEIGHT_BOLD);

					HSizer->Add(LoadButton, 0, wxEXPAND | wxALL, 2);
					HSizer->Add(SaveButton, 0, wxEXPAND | wxALL, 2);
					HSizer->Add(ResetToDefaults, 0, wxEXPAND | wxALL, 2);
					HSizer->Add(BindLabel, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
					
					wxBoxSizer* VSizer = new wxBoxSizer(wxVERTICAL);
					{
						wxButton* OKButton = new wxButton(MainPanel, wxID_OK, *LocalizeUnrealEd("OK"));
						VSizer->Add(OKButton, 0, wxALIGN_RIGHT | wxALL, 2);
					}
					HSizer->Add(VSizer,1, wxEXPAND);
				}
				MainSizer->Add(HSizer, 0, wxEXPAND | wxALL, 2);
			}
			MainPanel->SetSizer(MainSizer);
		}
		PanelSizer->Add(MainPanel, 1, wxEXPAND);
	}
	SetSizer(PanelSizer);
	SetAutoLayout(TRUE);

	// Build category tree
	BuildCategories();

	// Load window options.
	LoadSettings();

	// Initially select the first item in the category list
	CommandCategories->SelectItem( CommandCategories->GetFirstVisibleItem() );
}

/** Saves settings for this dialog to the INI. */
void WxDlgBindHotkeys::SaveSettings()
{
	FWindowUtil::SavePosSize(TEXT("DlgBindHotkeys"), this);

	GConfig->SetInt(TEXT("DlgBindHotkeys"), TEXT("SplitterPos"), MainSplitter->GetSashPosition(), GEditorUserSettingsIni);

	// Save the key bindings object
	UUnrealEdOptions* Options = GUnrealEd->GetUnrealEdOptions();
	
	if(Options)
	{
		UUnrealEdKeyBindings* KeyBindings = Options->EditorKeyBindings;
		
		if(KeyBindings)
		{
			FString UserKeybindings = appGameConfigDir() + GGameName;
			UserKeybindings += TEXT("EditorUserKeybindings.ini");

			KeyBindings->SaveConfig(CPF_Config, *UserKeybindings);
		}
	}
}

/** Loads settings for this dialog from the INI. */
void WxDlgBindHotkeys::LoadSettings()
{
	FWindowUtil::LoadPosSize(TEXT("DlgBindHotkeys"), this, -1, -1, 640, 480);

	INT SashPos = 256;

	GConfig->GetInt(TEXT("DlgBindHotkeys"), TEXT("SplitterPos"), SashPos, GEditorUserSettingsIni);
	MainSplitter->SetSashPosition(SashPos);

	// Load the user's settings
	UUnrealEdOptions* Options = GUnrealEd->GetUnrealEdOptions();

	if(Options)
	{
		UUnrealEdKeyBindings* KeyBindings = Options->EditorKeyBindings;

		if(KeyBindings)
		{
			// Load user bindings first.
			FString UserKeyBindings = appGameConfigDir() + GGameName;
			UserKeyBindings += TEXT("EditorUserKeyBindings.ini");
			KeyBindings->ReloadConfig(NULL, *UserKeyBindings);

			TArray<FEditorKeyBinding> UserKeys = Options->EditorKeyBindings->KeyBindings;

			// Reload Defaults
			KeyBindings->ReloadConfig();

			// Generate a binding map for quick lookup.
			TMap<FName, INT> BindingMap;

			BindingMap.Empty();

			for(INT BindingIdx=0; BindingIdx<KeyBindings->KeyBindings.Num(); BindingIdx++)
			{
				FEditorKeyBinding &KeyBinding = KeyBindings->KeyBindings(BindingIdx);

				BindingMap.Set(KeyBinding.CommandName, BindingIdx);
			}

			// Merge in user keys
			for(INT KeyIdx=0; KeyIdx<UserKeys.Num();KeyIdx++)
			{
				FEditorKeyBinding &UserKey = UserKeys(KeyIdx);

				INT* BindingIdx = BindingMap.Find(UserKey.CommandName);

				if(BindingIdx && KeyBindings->KeyBindings.IsValidIndex(*BindingIdx))
				{
					FEditorKeyBinding &DefaultBinding = KeyBindings->KeyBindings(*BindingIdx);
					DefaultBinding = UserKey;
				}
				else
				{
					KeyBindings->KeyBindings.AddItem(UserKey);
				}
			}
		}
	}
}

/** Builds the category tree. */
void WxDlgBindHotkeys::BuildCategories()
{
	ParentMap.Empty();
	CommandCategories->DeleteAllItems();
	CommandCategories->AddRoot(TEXT("Root"));
	
	// Add all of the categories to the tree.
	UUnrealEdOptions* Options = GUnrealEd->GetUnrealEdOptions();
	Options->GenerateCommandMap();

	if(Options)
	{
		TArray<FEditorCommandCategory> &Categories = Options->EditorCategories;

		for(INT CategoryIdx=0; CategoryIdx<Categories.Num(); CategoryIdx++)
		{
			FEditorCommandCategory &Category = Categories(CategoryIdx);
			wxTreeItemId ParentId = CommandCategories->GetRootItem();

			if(Category.Parent != NAME_None)
			{
				wxTreeItemId* ParentIdPtr = ParentMap.Find(Category.Parent);

				if(ParentIdPtr)
				{
					ParentId = *ParentIdPtr;
				}
			}

			FString CategoryName = Localize(TEXT("CommandCategoryNames"),*Category.Name.ToString(),TEXT("UnrealEd"));
			wxTreeItemId TreeItem = CommandCategories->AppendItem(ParentId, *CategoryName);
			ParentMap.Set(Category.Name, TreeItem);

			// Create the command map entry for this category.
			TArray<FEditorCommand> Commands;
			for(INT CmdIdx=0; CmdIdx<Options->EditorCommands.Num(); CmdIdx++)
			{
				FEditorCommand &Command = Options->EditorCommands(CmdIdx);

				if(Command.Parent == Category.Name)
				{
					Commands.AddItem(Command);
				}
			}

			if(Commands.Num())
			{
				CommandMap.Set(Category.Name, Commands);
			}
		}
	}
}

/** Builds the command list using the currently selected category. */
void WxDlgBindHotkeys::BuildCommands()
{
	UUnrealEdOptions* Options = GUnrealEd->GetUnrealEdOptions();

	CommandPanel->Freeze();

	// Remove all children from the panel.
	CommandPanel->DestroyChildren();
	
	VisibleBindingControls.Empty();

	FName ParentName;
	const FName* ParentNamePtr = ParentMap.FindKey(CommandCategories->GetSelection());

	if(ParentNamePtr != NULL)
	{
		ParentName = *ParentNamePtr;
		if(ParentName != NAME_None)
		{
			TArray<FEditorCommand> *CommandsPtr = CommandMap.Find(ParentName);

			if(CommandsPtr)
			{
				wxBoxSizer* PanelSizer = new wxBoxSizer(wxVERTICAL);
				{
					// Loop through all commands and add a panel for each one.
					TArray<FEditorCommand> &Commands = *CommandsPtr;

					for(INT CmdIdx=0; CmdIdx<Commands.Num(); CmdIdx++)
					{
						FEditorCommand &Command = Commands(CmdIdx);

						// Add a button, label, and binding box for each command.
						wxPanel* ItemPanel = new wxPanel(CommandPanel);
						{
							FCommandPanel PanelWidgets;
							PanelWidgets.BindingPanel = ItemPanel;
							PanelWidgets.CommandName = Command.CommandName;

							ItemPanel->SetBackgroundColour((CmdIdx%2==0) ? DlgBindHotkeys_LightColor : DlgBindHotkeys_DarkColor);

							FString CommandName = Localize(TEXT("CommandNames"),*Command.CommandName.ToString(),TEXT("UnrealEd"));
							wxBoxSizer* ItemSizer = new wxBoxSizer(wxHORIZONTAL);
							{
								wxBoxSizer* VSizer = new wxBoxSizer(wxVERTICAL);
								{
									wxStaticText* CommandLabel = new wxStaticText(ItemPanel, wxID_ANY, *CommandName);
									CommandLabel->GetFont().SetWeight(wxFONTWEIGHT_BOLD);
									VSizer->Add(CommandLabel, 0, wxEXPAND | wxTOP, 2);

									wxTextCtrl* CurrentBinding = new WxKeyBinder(ItemPanel, this);
									VSizer->Add(CurrentBinding, 0, wxEXPAND | wxBOTTOM, 6);

									// Store reference to the binding widget.
									PanelWidgets.BindingWidget = CurrentBinding;
								}
								ItemSizer->Add(VSizer, 1, wxEXPAND | wxALL, 4);

								wxButton* SetBindingButton = new wxButton(ItemPanel, ID_DLGBINDHOTKEYS_BIND_KEY_START+CmdIdx, *LocalizeUnrealEd("Bind"));
								ItemSizer->Add(SetBindingButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 8);

								PanelWidgets.BindingButton = SetBindingButton;
							}
							ItemPanel->SetSizer(ItemSizer);			

							// Store pointers to each of the controls we created for future use.
							VisibleBindingControls.AddItem(PanelWidgets);
						}
						PanelSizer->Add(ItemPanel, 0, wxEXPAND);
						PanelSizer->RecalcSizes();

						ItemSize = ItemPanel->GetSize().GetHeight();
					}

					VisibleCommands = Commands;
					NumVisibleItems = Commands.Num();
				}
				CommandPanel->SetSizer(PanelSizer);
			}
		}
	}

	// Refresh binding text
	RefreshBindings();

	// Start drawing the command panel again
	CommandPanel->Thaw();

	// Updates the scrollbar and refreshes this window.
	CommandPanel->Layout();
	CommandPanel->Refresh();
	UpdateScrollbar();
	CommandPanel->Refresh();
}

/** Refreshes the binding text for the currently visible binding widgets. */
void WxDlgBindHotkeys::RefreshBindings()
{
	UUnrealEdOptions* Options = GUnrealEd->GetUnrealEdOptions();

	// Loop through all visible items and update the current binding text.
	for(INT BindingIdx=0; BindingIdx<VisibleBindingControls.Num(); BindingIdx++)
	{
		FCommandPanel &BindingWidgets = VisibleBindingControls(BindingIdx);
		wxTextCtrl* CurrentBinding = BindingWidgets.BindingWidget;

		// Set the key binding text.
		FEditorKeyBinding* KeyBinding = Options->GetKeyBinding(BindingWidgets.CommandName);

		FString BindingText;

		if(KeyBinding)
		{
			BindingText = GenerateBindingText(KeyBinding->bAltDown, KeyBinding->bCtrlDown, KeyBinding->bShiftDown, KeyBinding->Key);								
		}
		else
		{
			BindingText = *LocalizeUnrealEd("NotBound");
		}

		CurrentBinding->SetValue(*BindingText);
	}
}

/** Updates the scrollbar for the command view. */
void WxDlgBindHotkeys::UpdateScrollbar()
{
	if(CommandPanel)
	{
		CommandPanel->EnableScrolling(FALSE, TRUE);
		CommandPanel->SetScrollbars(0, ItemSize, 0, NumVisibleItems);
	}
}

/** Starts the binding process for the specified command index. */
void WxDlgBindHotkeys::StartBinding(INT CommandIdx)
{
	CurrentlyBindingIdx = CommandIdx;

	// Set focus to the binding control
	FCommandPanel &BindingWidgets = VisibleBindingControls(CommandIdx);

	// Change the background of the current command
	BindingWidgets.BindingPanel->SetBackgroundColour(DlgBindHotkeys_SelectedBackgroundColor);
	BindingWidgets.BindingPanel->Refresh();

	// Set focus to the binding widget
	wxTextCtrl* BindingControl = BindingWidgets.BindingWidget;
	BindingControl->SetFocus();

	// Disable all binding buttons.
	for(INT ButtonIdx=0; ButtonIdx<VisibleBindingControls.Num(); ButtonIdx++)
	{
		VisibleBindingControls(ButtonIdx).BindingButton->Disable();
	}

	// Show binding text
	BindLabel->SetLabel(*LocalizeUnrealEd("PressKeysToBind"));
}

/** Finishes the binding for the current command using the provided event. */
void WxDlgBindHotkeys::FinishBinding(wxKeyEvent &Event)
{
	// Finish binding the key.
	UUnrealEdOptions* Options = GUnrealEd->GetUnrealEdOptions();

	FName KeyName = GApp->GetKeyName(Event);

	if(KeyName != NAME_None && Options->EditorCommands.IsValidIndex(CurrentlyBindingIdx))
	{
		FName Command = Options->EditorCommands(CurrentlyBindingIdx).CommandName;
		
		Options->BindKey(KeyName, Event.AltDown(), Event.ControlDown(), Event.ShiftDown(), Command);
	}

	StopBinding();
}

/** Stops the binding process for the current command. */
void WxDlgBindHotkeys::StopBinding()
{
	if(CurrentlyBindingIdx != -1)
	{		
		// Reset the background color
		FCommandPanel &BindingWidgets = VisibleBindingControls(CurrentlyBindingIdx);
		BindingWidgets.BindingPanel->SetBackgroundColour((CurrentlyBindingIdx%2==0) ? DlgBindHotkeys_LightColor : DlgBindHotkeys_DarkColor);
		BindingWidgets.BindingPanel->Refresh();

		// Enable all binding buttons.
		for(INT ButtonIdx=0; ButtonIdx<VisibleBindingControls.Num(); ButtonIdx++)
		{
			VisibleBindingControls(ButtonIdx).BindingButton->Enable();
		}

		// Hide binding text
		BindLabel->SetLabel(TEXT(""));

		// Refresh binding text
		RefreshBindings();

		CurrentlyBindingIdx = -1;
	}
}

/**
 * @return Generates a descriptive binding string based on the key combinations provided.
 */
FString WxDlgBindHotkeys::GenerateBindingText(UBOOL bAltDown, UBOOL bCtrlDown, UBOOL bShiftDown, FName Key)
{
	// Build a string describing this key binding.
	FString BindString;

	if(bCtrlDown)
	{
		BindString += TEXT("Ctrl + ");
	}

	if(bAltDown)
	{
		BindString += TEXT("Alt + ");
	}

	if(bShiftDown)
	{
		BindString += TEXT("Shift + ");
	}

	if(Key != NAME_None)
	{
		BindString += Key.ToString();
	}

	return BindString;
}

/** Window closed event handler. */
void WxDlgBindHotkeys::OnClose(wxCloseEvent& Event)
{
	SaveSettings();

	// Hide the dialog
	Hide();

	// Veto the close
	Event.Veto();
}

/** Category selected handler. */
void WxDlgBindHotkeys::OnCategorySelected(wxTreeEvent& Event)
{
	BuildCommands();
}

/** Handler to let the user load a config from a file. */
void WxDlgBindHotkeys::OnLoadConfig(wxCommandEvent& Event)
{
	// Get the filename
	const wxChar* FileTypes = TEXT("INI Files (*.ini)|*.ini|All Files (*.*)|*.*");

	WxFileDialog Dlg( this, 
		*LocalizeUnrealEd("LoadKeyConfig"), 
		*appGameConfigDir(),
		TEXT(""),
		FileTypes,
		wxOPEN | wxFILE_MUST_EXIST,
		wxDefaultPosition);

	if(Dlg.ShowModal()==wxID_OK)
	{
		wxString Filename = Dlg.GetPath();
		UUnrealEdOptions* Options = GUnrealEd->GetUnrealEdOptions();

		if(Options)
		{
			UUnrealEdKeyBindings* KeyBindings = Options->EditorKeyBindings;

			if(KeyBindings)
			{
				KeyBindings->ReloadConfig(NULL, Filename);

				// Refresh binding text
				RefreshBindings();
			}
		}
	}
}

/** Handler to let the user save the current config to a file. */
void WxDlgBindHotkeys::OnSaveConfig(wxCommandEvent& Event)
{
	// Get the filename
	const wxChar* FileTypes = TEXT("INI Files (*.ini)|*.ini|All Files (*.*)|*.*");

	WxFileDialog Dlg( this, 
		*LocalizeUnrealEd("SaveKeyConfig"), 
		*appGameConfigDir(),
		TEXT(""),
		FileTypes,
		wxSAVE,
		wxDefaultPosition);

	if(Dlg.ShowModal()==wxID_OK)
	{
		wxString Filename = Dlg.GetPath();
		UUnrealEdOptions* Options = GUnrealEd->GetUnrealEdOptions();

		if(Options)
		{
			UUnrealEdKeyBindings* KeyBindings = Options->EditorKeyBindings;

			if(KeyBindings)
			{
				KeyBindings->SaveConfig(CPF_Config, Filename);
			}
		}
	}
}

/** Handler to reset bindings to default. */
void WxDlgBindHotkeys::OnResetToDefaults(wxCommandEvent& Event)
{
	if(wxMessageBox(*LocalizeUnrealEd("AreYouSureYouWantDefaults"), TEXT(""), wxYES_NO | wxCENTRE, this) == wxYES)
	{
		// Load the user's settings
		UUnrealEdOptions* Options = GUnrealEd->GetUnrealEdOptions();

		if(Options)
		{
			UUnrealEdKeyBindings* KeyBindings = Options->EditorKeyBindings;

			if(KeyBindings)
			{
				KeyBindings->ReloadConfig();

				// Refresh binding text
				RefreshBindings();
			}
		}
	}
}

/** Bind key button pressed handler. */
void WxDlgBindHotkeys::OnBindKey(wxCommandEvent& Event)
{
	INT CommandIdx = Event.GetId() - ID_DLGBINDHOTKEYS_BIND_KEY_START;

	if(CommandIdx >=0 && CommandIdx < VisibleCommands.Num())
	{
		StartBinding(CommandIdx);
	}
	else
	{
		CurrentlyBindingIdx = -1;
	}
}

/** OK Button pressed handler. */
void WxDlgBindHotkeys::OnOK(wxCommandEvent &Event)
{
	SaveSettings();

	// Hide the dialog
	Hide();
}

/** Handler for key binding events. */
void WxDlgBindHotkeys::OnKeyDown(wxKeyEvent& Event)
{
	Event.Skip();
}

WxDlgBindHotkeys::~WxDlgBindHotkeys()
{
	
}

/*-----------------------------------------------------------------------------
	WxDlgPackageGroupName.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgPackageGroupName, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgPackageGroupName::OnOK )
END_EVENT_TABLE()

WxDlgPackageGroupName::WxDlgPackageGroupName()
{
	wxDialog::Create(NULL, wxID_ANY, TEXT("PackageGroupName"), wxDefaultPosition, wxDefaultSize );

	wxBoxSizer* HorizontalSizer = new wxBoxSizer(wxHORIZONTAL);
	{
		wxBoxSizer* InfoStaticBoxSizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Info"));
		{
			
			PGNCtrl = new WxPkgGrpNameCtrl( this, -1, NULL, TRUE );
			PGNCtrl->SetSizer(PGNCtrl->FlexGridSizer);
			InfoStaticBoxSizer->Add(PGNCtrl, 1, wxEXPAND);
		}
		HorizontalSizer->Add(InfoStaticBoxSizer, 1, wxALIGN_TOP|wxALL|wxEXPAND, 5);
		
		wxBoxSizer* ButtonSizer = new wxBoxSizer(wxVERTICAL);
		{
			wxButton* ButtonOK = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxDefaultSize, 0 );
			ButtonOK->SetDefault();
			ButtonSizer->Add(ButtonOK, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			wxButton* ButtonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
			ButtonSizer->Add(ButtonCancel, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
		}
		HorizontalSizer->Add(ButtonSizer, 0, wxALIGN_TOP|wxALL, 5);
		
	}
	SetSizer(HorizontalSizer);
	
	FWindowUtil::LoadPosSize( TEXT("DlgPackageGroupName"), this );
	GetSizer()->Fit(this);

	FLocalizeWindow( this );
}

WxDlgPackageGroupName::~WxDlgPackageGroupName()
{
	FWindowUtil::SavePosSize( TEXT("DlgPackageGroupName"), this );
}

int WxDlgPackageGroupName::ShowModal(const FString& InPackage, const FString& InGroup, const FString& InName, EControlOptions InOptions )
{
	Package = InPackage;
	Group = InGroup;
	Name = InName;

	PGNCtrl->PkgCombo->SetValue( *InPackage );
	PGNCtrl->PkgCombo->Enable((InOptions & PGN_Package) != 0);
	PGNCtrl->GrpEdit->SetValue( *InGroup );
	PGNCtrl->GrpEdit->Enable((InOptions & PGN_Group) != 0);
	PGNCtrl->NameEdit->SetValue( *InName );
	PGNCtrl->NameEdit->Enable((InOptions & PGN_Name) != 0);

	return wxDialog::ShowModal();
}

bool WxDlgPackageGroupName::Validate()
{
	bool bResult = wxDialog::Validate();

	if ( bResult )
	{
		// validate that the object name is valid
		//@todo ronp - move this functionality to a WxTextValidator
		FString Reason;
		if( !FIsValidObjectName( *Name, Reason )
		||	!FIsValidGroupName( *Package, Reason )
		||	!FIsValidGroupName( *Group, Reason, TRUE ) )
		{
			appMsgf( AMT_OK, *Reason );
			bResult = false;
		}
	}

	return bResult;
}

void WxDlgPackageGroupName::OnOK( wxCommandEvent& In )
{
	Package = PGNCtrl->PkgCombo->GetValue();
	Group = PGNCtrl->GrpEdit->GetValue();
	Name = PGNCtrl->NameEdit->GetValue();

	wxDialog::AcceptAndClose();
}

/** Util for  */
UBOOL WxDlgPackageGroupName::ProcessNewAssetDlg(UPackage** NewObjPackage, FString* NewObjName, UBOOL bAllowCreateOverExistingOfSameType, UClass* NewObjClass)
{
	check(NewObjPackage);
	check(NewObjName);

	FString Pkg;
	FString PName = GetPackage(), GName = GetGroup(), OName = GetObjectName();

	// Was a group specified?
	if( GName.Len() > 0 )
	{
		Pkg = FString::Printf(TEXT("%s.%s"),*PName,*GName);
	}
	else
	{
		Pkg = FString::Printf(TEXT("%s"),*PName);
	}
	UObject* ExistingPackage = UObject::FindPackage(NULL, *Pkg);
	FString Reason;

	// Verify the package an object name.
	if(!PName.Len() || !OName.Len())
	{
		appMsgf(AMT_OK,*LocalizeUnrealEd("Error_InvalidInput"));
		return FALSE;
	}
	// Verify the object name.
	else if(!FIsValidObjectName( *OName, Reason )
		||	!FIsValidGroupName(*GName, Reason, TRUE)
		||	!FIsValidGroupName(*PName, Reason) )
	{
		appMsgf( AMT_OK, *Reason );
		return FALSE;
	}

	*NewObjPackage = UObject::CreatePackage(NULL,*Pkg);

	// See if the name is already in use.
	UObject* FindObj = UObject::StaticFindObject( UObject::StaticClass(), *NewObjPackage, *GetObjectName() );
	if(FindObj && (!bAllowCreateOverExistingOfSameType || FindObj->GetClass() != NewObjClass))
	{
		appMsgf(AMT_OK, TEXT("Invalid object name.  An object already exists with the class name '%s'"), *FindObj->GetName());
		return FALSE;
	}

	// Verify the object name is unique withing the package.
	*NewObjName = OName;
	return TRUE;
}

/*-----------------------------------------------------------------------------
	WxDlgNewArchetype.
-----------------------------------------------------------------------------*/

bool WxDlgNewArchetype::Validate()
{
	bool bResult = wxDialog::Validate();
	if ( bResult )
	{
		FString	QualifiedName;
		if( Group.Len() )
		{
			QualifiedName = Package + TEXT(".") + Group + TEXT(".") + Name;
		}
		else
		{
			QualifiedName = Package + TEXT(".") + Name;
		}

		// validate that the object name is valid
		//@todo ronp - move this functionality to a WxTextValidator
		FString Reason;
		if( !FIsValidObjectName( *Name, Reason )
		||	!FIsValidGroupName( *Package, Reason )
		||	!FIsValidGroupName( *Group, Reason, TRUE )
		||	!FIsUniqueObjectName( *QualifiedName, ANY_PACKAGE, Reason ) )
		{
			appMsgf( AMT_OK, *Reason );
			bResult = false;
		}
	}

	return bResult;
}

/*-----------------------------------------------------------------------------
	WxDlgAddSpecial.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgAddSpecial, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgAddSpecial::OnOK )
END_EVENT_TABLE()

WxDlgAddSpecial::WxDlgAddSpecial()
{
	const bool bSuccess = wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_ADDSPECIAL") );
	check( bSuccess );

	PortalCheck = (wxCheckBox*)FindWindow( XRCID( "IDCK_PORTAL" ) );
	check( PortalCheck != NULL );
	InvisibleCheck = (wxCheckBox*)FindWindow( XRCID( "IDCK_INVISIBLE" ) );
	check( InvisibleCheck != NULL );
	TwoSidedCheck = (wxCheckBox*)FindWindow( XRCID( "IDCK_TWOSIDED" ) );
	check( TwoSidedCheck != NULL );
	SolidRadio = (wxRadioButton*)FindWindow( XRCID( "IDRB_SOLID" ) );
	check( SolidRadio != NULL );
	SemiSolidRadio = (wxRadioButton*)FindWindow( XRCID( "IDRB_SEMISOLID" ) );
	check( SemiSolidRadio != NULL );
	NonSolidRadio = (wxRadioButton*)FindWindow( XRCID( "IDRB_NONSOLID" ) );
	check( NonSolidRadio != NULL );

	FWindowUtil::LoadPosSize( TEXT("DlgAddSpecial"), this );
	FLocalizeWindow( this );
}

WxDlgAddSpecial::~WxDlgAddSpecial()
{
	FWindowUtil::SavePosSize( TEXT("DlgAddSpecial"), this );
}

void WxDlgAddSpecial::OnOK( wxCommandEvent& In )
{
	INT Flags = 0;

	if( PortalCheck->GetValue() )		Flags |= PF_Portal;
	if( InvisibleCheck->GetValue() )	Flags |= PF_Invisible;
	if( TwoSidedCheck->GetValue() )		Flags |= PF_TwoSided;
	if( SemiSolidRadio->GetValue() )	Flags |= PF_Semisolid;
	if( NonSolidRadio->GetValue() )		Flags |= PF_NotSolid;

	GUnrealEd->Exec( *FString::Printf(TEXT("BRUSH ADD FLAGS=%d"), Flags));

	wxDialog::AcceptAndClose();
}

/*-----------------------------------------------------------------------------
	WxDlgGenericStringEntry.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgGenericStringEntry, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgGenericStringEntry::OnOK )
END_EVENT_TABLE()

/**
 * @param	LocalizeWindow	When TRUE, the window will be localized; Un-localized, otherwise. 
 */
WxDlgGenericStringEntry::WxDlgGenericStringEntry( UBOOL LocalizeWindow /*= TRUE*/ )
:	bLocalizeWindow(LocalizeWindow)
{
	const bool bSuccess = wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_GENERICSTRINGENTRY") );
	check( bSuccess );

	StringEntry = (wxTextCtrl*)FindWindow( XRCID( "IDEC_STRINGENTRY" ) );
	check( StringEntry != NULL );
	StringCaption = (wxStaticText*)FindWindow( XRCID( "IDEC_STRINGCAPTION" ) );
	check( StringCaption != NULL );

	ADDEVENTHANDLER( XRCID("IDEC_STRINGENTRY"), wxEVT_COMMAND_TEXT_ENTER, &WxDlgGenericStringEntry::OnOK );

	EnteredString = FString( TEXT("") );

	FWindowUtil::LoadPosSize( TEXT("DlgGenericStringEntry"), this );
}

WxDlgGenericStringEntry::~WxDlgGenericStringEntry()
{
	FWindowUtil::SavePosSize( TEXT("DlgGenericStringEntry"), this );
}

int WxDlgGenericStringEntry::ShowModal(const TCHAR* DialogTitle, const TCHAR* Caption, const TCHAR* DefaultString)
{
	SetTitle( DialogTitle );
	StringCaption->SetLabel( Caption );

	if( bLocalizeWindow )
	{
		FLocalizeWindow( this );
	}

	StringEntry->SetValue( DefaultString );

	return wxDialog::ShowModal();
}

void WxDlgGenericStringEntry::OnOK( wxCommandEvent& In )
{
	EnteredString = StringEntry->GetValue();

	wxDialog::AcceptAndClose();
}

/*-----------------------------------------------------------------------------
	WxDlgGenericStringWrappedEntry.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgGenericStringWrappedEntry, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgGenericStringWrappedEntry::OnEnter )
END_EVENT_TABLE()

WxDlgGenericStringWrappedEntry::WxDlgGenericStringWrappedEntry()
{
	const bool bSuccess = wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_GENERICSTRINGWRAPPEDENTRY") );
	check( bSuccess );

	StringEntry = (wxTextCtrl*)FindWindow( XRCID( "IDEC_STRINGENTRY" ) );
	check( StringEntry != NULL );
	StringCaption = (wxStaticText*)FindWindow( XRCID( "IDEC_STRINGCAPTION" ) );
	check( StringCaption != NULL );

	ADDEVENTHANDLER( XRCID("IDEC_STRINGENTRY"), wxEVT_COMMAND_TEXT_ENTER, &WxDlgGenericStringWrappedEntry::OnEnter );

	EnteredString = FString( TEXT("") );

	FWindowUtil::LoadPosSize( TEXT("DlgGenericStringWrappedEntry"), this );
}

WxDlgGenericStringWrappedEntry::~WxDlgGenericStringWrappedEntry()
{
	FWindowUtil::SavePosSize( TEXT("DlgGenericStringWrappedEntry"), this );
}

int WxDlgGenericStringWrappedEntry::ShowModal(const TCHAR* DialogTitle, const TCHAR* Caption, const TCHAR* DefaultString)
{
	SetTitle( DialogTitle );
	StringCaption->SetLabel( Caption );

	FLocalizeWindow( this );

	StringEntry->SetValue( DefaultString );

	return wxDialog::ShowModal();
}

void WxDlgGenericStringWrappedEntry::FinalizeDialog()
{
	EnteredString = StringEntry->GetValue();

	wxDialog::AcceptAndClose();
}

void WxDlgGenericStringWrappedEntry::OnEnter( wxCommandEvent& In )
{
	const UBOOL bCtrlDown = GetAsyncKeyState(VK_CONTROL) & 0x8000;

	if( bCtrlDown )
	{
		StringEntry->WriteText( TEXT("\n") );
	}
	else
	{
		FinalizeDialog();
	}
}

/*-----------------------------------------------------------------------------
	WxDlgGenericSlider
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgGenericSlider, wxDialog)
END_EVENT_TABLE()

WxDlgGenericSlider::WxDlgGenericSlider( wxWindow* InParent )
	: wxDialog( InParent, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE )
{
	const INT BorderSize = 5;

	// Setup controls
	TopSizer = new wxBoxSizer( wxVERTICAL );
	SetSizer( TopSizer );
	{
		Sizer = new wxStaticBoxSizer( wxHORIZONTAL, this, wxString() );
		{
			Sizer->AddSpacer( BorderSize );
			// NOTE: Proper min/max/value settings will be filled in later in ShowModal()
			Slider = new wxSlider( this, -1, 1, 1, 1, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_LABELS );
			wxSize SliderMinSize = Slider->GetMinSize();
			Slider->SetMinSize( wxSize( Max<INT>( SliderMinSize.GetWidth(), 250 ), SliderMinSize.GetHeight() ) );
			Sizer->Add( Slider,	1, wxEXPAND | wxALL, BorderSize );
		}
		TopSizer->Add( Sizer, 1, wxEXPAND | wxALL, BorderSize );
	}
	TopSizer->AddSpacer( BorderSize * 2 );
	{
		wxBoxSizer* ButtonSizer = new wxBoxSizer( wxHORIZONTAL );
		{
			wxButton* OkButton = new wxButton( this, wxID_OK, *LocalizeUnrealEd( TEXT("&OK") ) );
			ButtonSizer->Add( OkButton, 1, wxALIGN_RIGHT, BorderSize );
			wxButton* CancelButton = new wxButton( this, wxID_CANCEL, *LocalizeUnrealEd( TEXT("&Cancel") ) );
			ButtonSizer->Add( CancelButton, 1, wxALIGN_RIGHT, BorderSize );
		}
		TopSizer->Add( ButtonSizer, 0, wxALIGN_RIGHT | wxALL, BorderSize );
	}
}

WxDlgGenericSlider::~WxDlgGenericSlider()
{
}

/**
 * Shows the dialog box and waits for the user to respond.
 * @param DialogTitle - Title of the dialog box.
 * @param Caption - Caption next to the slider.
 * @param MinValue - Minimum allowed slider value.
 * @param MaxValue - Maximum allowed slider value.
 * @param DefaultValue - Default slider value.
 * @returns wxID_OK or wxID_CANCEL depending on which button the user dismissed the dialog with.
 */
int WxDlgGenericSlider::ShowModal( const TCHAR* DialogTitle, const TCHAR* Caption, INT MinValue, INT MaxValue, INT DefaultValue )
{
	check( Sizer );
	check( Slider );
	SetTitle( DialogTitle );
	Sizer->GetStaticBox()->SetLabel( Caption );
	Slider->SetRange( MinValue, MaxValue );
	Slider->SetValue( DefaultValue );
	GetSizer()->Fit( this );
	return wxDialog::ShowModal();
}

/*-----------------------------------------------------------------------------
WxDlgMorphLODImport.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgMorphLODImport, wxDialog)
END_EVENT_TABLE()

#if WITH_FBX
/*-----------------------------------------------------------------------------
WxDlgMorphLODFbxImport.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgMorphLODFbxImport, wxDialog)
END_EVENT_TABLE()

/*-----------------------------------------------------------------------------
WxDlgMorphTargetFbxImport.
-----------------------------------------------------------------------------*/

//BEGIN_EVENT_TABLE(WxDlgMorphTargetFbxImport, wxDialog)
//END_EVENT_TABLE()

/*-----------------------------------------------------------------------------
WxDlgFbxSceneInfo.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgFbxSceneInfo, wxDialog)
END_EVENT_TABLE()
#endif // WITH_FBX

/**
* Find the number of matching characters in-order between two strings
* @param StrA - string to compare
* @param StrB - string to compare
* @return number of matching characters
*/
static INT NumMatchingChars( const FString& StrA, const FString& StrB )
{
	INT MaxLength = Min(StrA.Len(),StrB.Len());
	INT NumMatching=0;
	if( MaxLength > 0 )
	{
		// find start index for first character 
		INT StrANumMatching=0;
		INT StrAStartIdx = StrA.InStr( FString::Printf(TEXT("%c"),StrB[0]) );
		if( StrAStartIdx != INDEX_NONE )
		{
			for( INT Idx=StrAStartIdx; Idx < MaxLength; Idx++ )
			{
				if( StrA[Idx] != StrB[Idx-StrAStartIdx] ) 
				{
					break;
				}
				else 
				{
					StrANumMatching++;
				}
			}
		}

		// find start index for first character 
		INT StrBNumMatching=0;
		INT StrBStartIdx = StrB.InStr( FString::Printf(TEXT("%c"),StrA[0]) );
		if( StrBStartIdx != INDEX_NONE )
		{
			for( INT Idx=StrBStartIdx; Idx < MaxLength; Idx++ )
			{
				if( StrB[Idx] != StrA[Idx-StrBStartIdx] ) 
				{
					break;
				}
				else 
				{
					StrBNumMatching++;
				}
			}
		}

		NumMatching = Max(StrANumMatching, StrBNumMatching);
	}
	return NumMatching;
}

WxDlgMorphLODImport::WxDlgMorphLODImport( wxWindow* InParent, const TArray<UObject*>& InMorphObjects, INT MaxLODIdx, const TCHAR* SrcFileName )
:	wxDialog(InParent,wxID_ANY,*LocalizeUnrealEd("WxDlgMorphLODImport_Title"),wxDefaultPosition,wxDefaultSize,wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER)
{
	wxBoxSizer* MainSizer = new wxBoxSizer(wxVERTICAL);
	{
		// filename text
		wxStaticText* FileNameText = new wxStaticText(
			this,
			wxID_ANY,
			*FString::Printf(TEXT("%s %s"),*LocalizeUnrealEd("WxDlgMorphLODImport_Filename"),SrcFileName)
			);

		wxBoxSizer* MorphListHorizSizer = new wxBoxSizer(wxHORIZONTAL);
		{
			// label for morph target list
			wxStaticText* MorphNameText = new wxStaticText(
				this,
				wxID_ANY,
				*LocalizeUnrealEd("WxDlgMorphLODImport_MorphObject")
				);
			// fill in the text for each entry of the morph list dropdown
			// and also find the best matching morph target for the given filename
			wxArrayString MorphListText;
			FString BestMatch;
			INT BestMatchNumChars=-1;
			for( INT MorphIdx=0; MorphIdx < InMorphObjects.Num(); MorphIdx++ )
			{
				UObject* Morph = InMorphObjects(MorphIdx);
				check(Morph);
				if( Morph )
				{
					MorphListText.Add( *Morph->GetName() );
				}
				INT NumMatching = NumMatchingChars(FString(SrcFileName),Morph->GetName());
				if( NumMatching > BestMatchNumChars )
				{
					BestMatch = Morph->GetName();
					BestMatchNumChars = NumMatching;
				}				
			}

			// create the morph list dropdown
			MorphObjectsList = new wxComboBox(
				this,
				wxID_ANY,
				wxString(*BestMatch),
				wxDefaultPosition,
				wxDefaultSize,
				MorphListText,
				wxCB_DROPDOWN|wxCB_READONLY
				);
			// add the lable and dropdown to the sizer
			MorphListHorizSizer->Add( MorphNameText, 0, wxEXPAND|wxALL, 4 );
			MorphListHorizSizer->Add( MorphObjectsList, 1, wxEXPAND|wxALL, 4 );

		}

		wxBoxSizer* LODHorizSizer = new wxBoxSizer(wxHORIZONTAL);
		{
			// label for LOD list
			wxStaticText* LODNameText = new wxStaticText(
				this,
				wxID_ANY,
				*LocalizeUnrealEd("WxDlgMorphLODImport_LODLevel")
				);
			// fill in the text for each LOD index
			wxArrayString LODListText;
			for( INT LODIdx=0; LODIdx <= MaxLODIdx; LODIdx++ )
			{
				LODListText.Add( *FString::Printf(TEXT("%d"),LODIdx) );
			}
			// create the LOD index dropdown
			LODLevelList = new wxComboBox(
				this,
				wxID_ANY,
				wxString(*FString::Printf(TEXT("%d"),MaxLODIdx)),
				wxDefaultPosition,
				wxDefaultSize,
				LODListText,
				wxCB_DROPDOWN|wxCB_READONLY
				);

			LODHorizSizer->Add( LODNameText, 0, wxEXPAND|wxALL, 4 );
			LODHorizSizer->Add( LODLevelList, 1, wxEXPAND|wxALL, 4 );
		}

		MainSizer->Add( FileNameText, 0, wxEXPAND|wxALL, 4 );
		MainSizer->Add( MorphListHorizSizer, 0, wxEXPAND );
		MainSizer->Add( LODHorizSizer, 0, wxEXPAND );

		wxBoxSizer* ButtonSizer = new wxBoxSizer(wxHORIZONTAL);
		{
			// ok button
			wxButton* OkButton = new wxButton(this, wxID_OK, *LocalizeUnrealEd("OK"));
			ButtonSizer->Add(OkButton, 0, wxCENTER | wxALL, 5);
			// cancel button
			wxButton* CancelButton = new wxButton(this, wxID_CANCEL, *LocalizeUnrealEd("Cancel"));
			ButtonSizer->Add(CancelButton, 0, wxCENTER | wxALL, 5);
		}
		MainSizer->Add(ButtonSizer, 0, wxALIGN_RIGHT | wxALL, 2);
	}

	SetSizer(MainSizer);
	FWindowUtil::LoadPosSize(TEXT("DlgMorphLODImport"),this,-1,-1,350,150);
}

WxDlgMorphLODImport::~WxDlgMorphLODImport()
{
	FWindowUtil::SavePosSize(TEXT("DlgMorphLODImport"),this);
}

#if WITH_FBX
WxDlgMorphLODFbxImport::WxDlgMorphLODFbxImport( wxWindow* InParent, const char* InMorphName, const TArray<UObject*>& InMorphObjects, INT LODIdx )
:	wxDialog(InParent,wxID_ANY,*LocalizeUnrealEd("WxDlgMorphLODImport_Title"),wxDefaultPosition,wxDefaultSize,wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER)
{
	wxBoxSizer* MainSizer = new wxBoxSizer(wxVERTICAL);
	{
		// head text
		wxStaticText* LODText = new wxStaticText(
			this,
			wxID_ANY,
			*FString::Printf(LocalizeSecure(LocalizeUnrealEd("WxDlgMorphLODFbxImport_Header"), ANSI_TO_TCHAR(InMorphName), LODIdx)) 
			);

		
		// fill in the text for each entry of the morph list dropdown
		// and also find the best matching morph target for the given filename
		wxArrayString MorphListText;
		FString BestMatch;
		INT BestMatchNumChars=-1;
		for( INT MorphIdx=0; MorphIdx < InMorphObjects.Num(); MorphIdx++ )
		{
			UObject* Morph = InMorphObjects(MorphIdx);
			check(Morph);
			if( Morph )
			{
				MorphListText.Add( *Morph->GetName() );
			}
			INT NumMatching = NumMatchingChars(FString(InMorphName),Morph->GetName());
			if( NumMatching > BestMatchNumChars )
			{
				BestMatch = Morph->GetName();
				BestMatchNumChars = NumMatching;
			}				
		}

		// create the morph list dropdown
		MorphObjectsList = new wxComboBox(
			this,
			wxID_ANY,
			*BestMatch,
			wxDefaultPosition,
			wxDefaultSize,
			MorphListText,
			wxCB_DROPDOWN|wxCB_READONLY
			) ;

			
		MainSizer->Add( LODText, 0, wxEXPAND|wxALL, 4 );
		MainSizer->Add( MorphObjectsList, 0, wxEXPAND );
		
		wxBoxSizer* ButtonSizer = new wxBoxSizer(wxHORIZONTAL);
		{
			// ok button
			wxButton* OkButton = new wxButton(this, wxID_OK, *LocalizeUnrealEd("OK"));
			ButtonSizer->Add(OkButton, 0, wxCENTER | wxALL, 5);
			// cancel button
			wxButton* CancelButton = new wxButton(this, wxID_CANCEL, *LocalizeUnrealEd("Cancel"));
			ButtonSizer->Add(CancelButton, 0, wxCENTER | wxALL, 5);
		}
		MainSizer->Add(ButtonSizer, 0, wxALIGN_RIGHT | wxALL, 2);
	}

	MainSizer->Fit(this);    
	MainSizer->SetSizeHints(this);
	SetSizer(MainSizer);
}

WxDlgFbxSceneInfo::WxDlgFbxSceneInfo( wxWindow* InParent, struct FbxSceneInfo& SceneInfo )
:wxDialog(InParent,wxID_ANY,*LocalizeUnrealEd("WxDlgFbxSceneInfo_Title"),wxDefaultPosition,wxDefaultSize,wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER)
{
	wxBoxSizer* MainSizer = new wxBoxSizer(wxVERTICAL);
	{
		FString Text;
		wxSize TextSize(250, 16);

		// generic stat info
		wxStaticBox* StatsBox = new wxStaticBox(this, wxID_ANY, wxT("Stats"));
		wxStaticBoxSizer* StatsSizer = new wxStaticBoxSizer(StatsBox, wxVERTICAL);
		{
			Text = *FString::Printf(TEXT("       %s %d"), ANSI_TO_TCHAR("Static Meshes: "), SceneInfo.NonSkinnedMeshNum);
			StatsSizer->Add( new wxStaticText(
				this,
				wxID_ANY,
				*Text,
				wxDefaultPosition,
				TextSize ) );
			Text = *FString::Printf(TEXT("       %s %d"), ANSI_TO_TCHAR("Skeletal Meshes: "), SceneInfo.SkinnedMeshNum);
			StatsSizer->Add( new wxStaticText(
				this,
				wxID_ANY,
				*Text ,
				wxDefaultPosition,
				TextSize ) );
			Text = *FString::Printf(TEXT("       %s%d"), ANSI_TO_TCHAR("Total Geometric Objects: "), SceneInfo.TotalGeometryNum);
			StatsSizer->Add( new wxStaticText(
				this,
				wxID_ANY,
				*Text,
				wxDefaultPosition,
				TextSize ) );
			// don't display material and texture info before get the UI option back
			/*
			Text = *FString::Printf(TEXT("       %s%d"), ANSI_TO_TCHAR("Total Materials: "), SceneInfo.TotalMaterialNum);
			StatsSizer->Add( new wxStaticText(
				this,
				wxID_ANY,
				*Text,
				wxDefaultPosition,
				TextSize ) );
			Text = *FString::Printf(TEXT("       %s%d"), ANSI_TO_TCHAR("Total Textures: "), SceneInfo.TotalTextureNum);
			StatsSizer->Add( new wxStaticText(
				this,
				wxID_ANY,
				*Text,
				wxDefaultPosition,
				TextSize ) );
			*/
		}		
		MainSizer->Add(StatsSizer, 0, wxALIGN_CENTER | wxALL, 5);

		wxStaticBox* MeshBox = new wxStaticBox(this, wxID_ANY, wxT("Meshes"));
		wxStaticBoxSizer* MeshSizer = new wxStaticBoxSizer(MeshBox, wxVERTICAL);
		// create the tree view for meshes
		wxTreeCtrl* lMeshTreeCtrl = new wxTreeCtrl( this, wxID_ANY, wxPoint(0, 0), wxSize(250, 300), wxTR_HAS_BUTTONS|wxTR_NO_LINES|wxTR_HIDE_ROOT|wxTR_LINES_AT_ROOT);
		wxTreeItemId lRootId = lMeshTreeCtrl->AddRoot(wxT("Root"), -1, -1, NULL);
		
		INT GeometryIndex;
		for ( GeometryIndex = 0; GeometryIndex < SceneInfo.MeshInfo.Num(); GeometryIndex++ )
		{
			FbxMeshInfo MeshInfo = SceneInfo.MeshInfo(GeometryIndex);
			Text = *FString::Printf(TEXT("%s"), ANSI_TO_TCHAR(MeshInfo.Name));
			// create one item for each mesh
			wxTreeItemId lItemId = lMeshTreeCtrl->AppendItem(lRootId, *Text, -1, -1, NULL);

			Text = *FString::Printf(TEXT("       %s%s"), ANSI_TO_TCHAR("Mesh Type: "), ANSI_TO_TCHAR(MeshInfo.bIsSkelMesh? "Skeletal" : "Static"));
			wxTreeItemId lSubItemId = lMeshTreeCtrl->AppendItem(lItemId, *Text, -1, -1, NULL);
			Text = *FString::Printf(TEXT("       %s%d"), ANSI_TO_TCHAR("Faces: "), MeshInfo.FaceNum);
			lSubItemId = lMeshTreeCtrl->AppendItem(lItemId, *Text, -1, -1, NULL);
			Text = *FString::Printf(TEXT("       %s%d"), ANSI_TO_TCHAR("Vertices: "), MeshInfo.VertexNum);
			lSubItemId = lMeshTreeCtrl->AppendItem(lItemId, *Text, -1, -1, NULL);
			Text = *FString::Printf(TEXT("       %s%s"), ANSI_TO_TCHAR("Triangulated: "), ANSI_TO_TCHAR(MeshInfo.bTriangulated ? "Yes" : "No"));
			lSubItemId = lMeshTreeCtrl->AppendItem(lItemId, *Text, -1, -1, NULL);
			// don't display material and texture info before get the UI option back
			//Text = *FString::Printf(TEXT("       %s%d"), ANSI_TO_TCHAR("Material Count: "), MeshInfo.MaterialNum);
			//lSubItemId = lMeshTreeCtrl->AppendItem(lItemId, *Text, -1, -1, NULL);
			if (MeshInfo.LODGroup)
			{
				Text = *FString::Printf(TEXT("       %s%s"), ANSI_TO_TCHAR("LOD Group: "), ANSI_TO_TCHAR(MeshInfo.LODGroup));
				lSubItemId = lMeshTreeCtrl->AppendItem(lItemId, *Text, -1, -1, NULL);
				Text = *FString::Printf(TEXT("          %s%d"), ANSI_TO_TCHAR("LOD Index: "), MeshInfo.LODLevel);
				lSubItemId = lMeshTreeCtrl->AppendItem(lItemId, *Text, -1, -1, NULL);
			}

			if (MeshInfo.bIsSkelMesh && MeshInfo.SkeletonRoot)
			{
				Text = *FString::Printf(TEXT("       %s%s"), ANSI_TO_TCHAR("Skeletal Root: "), ANSI_TO_TCHAR(MeshInfo.SkeletonRoot));
				lSubItemId = lMeshTreeCtrl->AppendItem(lItemId, *Text, -1, -1, NULL);
				Text = *FString::Printf(TEXT("          %s%d"), ANSI_TO_TCHAR("Skeletal Elements: "), MeshInfo.SkeletonElemNum);
				lSubItemId = lMeshTreeCtrl->AppendItem(lItemId, *Text, -1, -1, NULL);
				Text = *FString::Printf(TEXT("       %s%d"), ANSI_TO_TCHAR("Morph Count: "), MeshInfo.MorphNum);
				lSubItemId = lMeshTreeCtrl->AppendItem(lItemId, *Text, -1, -1, NULL);
			}
		}
		MeshSizer->Add(lMeshTreeCtrl);
		MainSizer->Add(MeshSizer, 0, wxALIGN_CENTER | wxALL, 5);

		// create box for animation info
		if ( SceneInfo.SkinnedMeshNum > 0 )
		{
			wxStaticBox* AnimBox = new wxStaticBox(this, wxID_ANY, wxT("Animation"));
			wxStaticBoxSizer* AnimSizer = new wxStaticBoxSizer(AnimBox, wxVERTICAL);
			{
				if ( SceneInfo.TakeName)
				{
					Text = *FString::Printf(TEXT("       %s%s"), ANSI_TO_TCHAR("Take Name: "), ANSI_TO_TCHAR(SceneInfo.TakeName));
					AnimSizer->Add( new wxStaticText(
						this,
						wxID_ANY,
						*Text,
						wxDefaultPosition,
						TextSize ) );
				}
				Text = *FString::Printf(TEXT("       %s%f (FPS)"), ANSI_TO_TCHAR("Frame Rate: "), SceneInfo.FrameRate);
				AnimSizer->Add( new wxStaticText(
					this,
					wxID_ANY,
					*Text,
					wxDefaultPosition,
					TextSize) );
				Text = *FString::Printf(TEXT("       %s%f"), ANSI_TO_TCHAR("Total Time: "), SceneInfo.TotalTime);
				AnimSizer->Add( new wxStaticText(
					this,
					wxID_ANY,
					*Text,
					wxDefaultPosition,
					TextSize ) );
			}
			MainSizer->Add(AnimSizer, 0, wxALIGN_RIGHT | wxALL, 5);
		}

		wxBoxSizer* ButtonSizer = new wxBoxSizer(wxHORIZONTAL);
		{
			// ok button
			wxButton* OkButton = new wxButton(this, wxID_OK, *LocalizeUnrealEd("OK"));
			ButtonSizer->Add(OkButton, 0, wxCENTER | wxALL, 5);
		}

		MainSizer->Add(ButtonSizer, 0, wxALIGN_RIGHT | wxALL, 2);
	}

	MainSizer->Fit(this);
	MainSizer->SetSizeHints(this);
	SetSizer(MainSizer);
}

#endif // WITH_FBX

/*-----------------------------------------------------------------------------
	XDlgSurfPropPage1.
-----------------------------------------------------------------------------*/

WxSurfacePropertiesPanel::WxSurfacePropertiesPanel( wxWindow* InParent )
	:	wxPanel( InParent, -1 ),
		bUseSimpleScaling( TRUE )
{
	Panel = (wxPanel*)wxXmlResource::Get()->LoadPanel( this, TEXT("ID_SURFPROP_PANROTSCALE") );
	check( Panel != NULL );
	SimpleCB = (wxComboBox*)FindWindow( XRCID( "IDCB_SIMPLE" ) );
	check( SimpleCB != NULL );
	CustomULabel = (wxStaticText*)FindWindow( XRCID( "IDSC_CUSTOM_U" ) );
	check( CustomULabel != NULL );
	CustomVLabel = (wxStaticText*)FindWindow( XRCID( "IDSC_CUSTOM_V" ) );
	check( CustomVLabel != NULL );
	CustomUEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_CUSTOM_U" ) );
	check( CustomUEdit != NULL );
	CustomVEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_CUSTOM_V" ) );
	check( CustomVEdit != NULL );
	SimpleScaleButton = (wxRadioButton*)FindWindow( XRCID( "IDRB_SIMPLE" ) );
	check( SimpleScaleButton != NULL );
	CustomScaleButton = (wxRadioButton*)FindWindow( XRCID( "IDRB_CUSTOM" ) );
	check( CustomScaleButton != NULL );
	RelativeCheck = (wxCheckBox*)FindWindow( XRCID( "IDCK_RELATIVE" ) );
	check( RelativeCheck != NULL );
	LightMapResEdit = (wxTextCtrl*)FindWindow( XRCID( "IDSC_LIGHTMAPRES" ) );
	check( LightMapResEdit != NULL );
	bSettingLightMapRes = FALSE;
	AcceptsLightsCheck = (wxCheckBox*)FindWindow( XRCID( "IDCK_ACCEPTSLIGHTS" ) );
	check( AcceptsLightsCheck != NULL );
	AcceptsDynamicLightsCheck = (wxCheckBox*)FindWindow( XRCID( "IDCK_ACCEPTSDYNAMICLIGHTS" ) );
	check( AcceptsDynamicLightsCheck != NULL );
	ForceLightMapCheck = (wxCheckBox*)FindWindow( XRCID( "IDCK_FORCELIGHTMAP" ) );
	check( ForceLightMapCheck != NULL );

	SimpleScaleButton->SetValue( 1 );

	ADDEVENTHANDLER( XRCID("IDPB_U_1"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfacePropertiesPanel::OnU1 );
	ADDEVENTHANDLER( XRCID("IDPB_U_4"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfacePropertiesPanel::OnU4 );
	ADDEVENTHANDLER( XRCID("IDPB_U_16"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfacePropertiesPanel::OnU16 );
	ADDEVENTHANDLER( XRCID("IDPB_U_64"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfacePropertiesPanel::OnU64 );
	ADDEVENTHANDLER( XRCID("IDPB_U_CUSTOM"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfacePropertiesPanel::OnUCustom );

	ADDEVENTHANDLER( XRCID("IDPB_V_1"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfacePropertiesPanel::OnV1 );
	ADDEVENTHANDLER( XRCID("IDPB_V_4"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfacePropertiesPanel::OnV4 );
	ADDEVENTHANDLER( XRCID("IDPB_V_16"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfacePropertiesPanel::OnV16 );
	ADDEVENTHANDLER( XRCID("IDPB_V_64"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfacePropertiesPanel::OnV64 );
	ADDEVENTHANDLER( XRCID("IDPB_V_CUSTOM"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfacePropertiesPanel::OnVCustom );

	ADDEVENTHANDLER( XRCID("IDPB_ROT_45"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfacePropertiesPanel::OnRot45 );
	ADDEVENTHANDLER( XRCID("IDPB_ROT_90"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfacePropertiesPanel::OnRot90 );
	ADDEVENTHANDLER( XRCID("IDPB_ROT_CUSTOM"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfacePropertiesPanel::OnRotCustom );

	ADDEVENTHANDLER( XRCID("IDPB_FLIP_U"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfacePropertiesPanel::OnFlipU );
	ADDEVENTHANDLER( XRCID("IDPB_FLIP_V"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfacePropertiesPanel::OnFlipV );
	
	ADDEVENTHANDLER( XRCID("IDPB_APPLY"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfacePropertiesPanel::OnApply );
	ADDEVENTHANDLER( XRCID("IDRB_SIMPLE"), wxEVT_COMMAND_RADIOBUTTON_SELECTED, &WxSurfacePropertiesPanel::OnScaleSimple );
	ADDEVENTHANDLER( XRCID("IDRB_CUSTOM"), wxEVT_COMMAND_RADIOBUTTON_SELECTED, &WxSurfacePropertiesPanel::OnScaleCustom );
	ADDEVENTHANDLER( XRCID("IDSC_LIGHTMAPRES"), wxEVT_COMMAND_TEXT_UPDATED, &WxSurfacePropertiesPanel::OnLightMapResChange );
	ADDEVENTHANDLER( XRCID("IDCK_ACCEPTSLIGHTS"), wxEVT_COMMAND_CHECKBOX_CLICKED, &WxSurfacePropertiesPanel::OnAcceptsLightsChange );
	ADDEVENTHANDLER( XRCID("IDCK_ACCEPTSDYNAMICLIGHTS"), wxEVT_COMMAND_CHECKBOX_CLICKED, &WxSurfacePropertiesPanel::OnAcceptsDynamicLightsChange );
	ADDEVENTHANDLER( XRCID("IDCK_FORCELIGHTMAP"), wxEVT_COMMAND_CHECKBOX_CLICKED, &WxSurfacePropertiesPanel::OnForceLightmapChange );

	CustomULabel->Disable();
	CustomVLabel->Disable();
	CustomUEdit->Disable();
	CustomVEdit->Disable();

	
	// Setup alignment properties.
	AlignList = (wxListBox*)FindWindow( XRCID( "IDLB_ALIGNMENT" ) );
	check( AlignList );

	ADDEVENTHANDLER( XRCID("IDLB_ALIGNMENT"), wxEVT_COMMAND_LISTBOX_DOUBLECLICKED, &WxSurfacePropertiesPanel::OnApplyAlign );
	ADDEVENTHANDLER( XRCID("IDLB_ALIGNMENT"), wxEVT_COMMAND_LISTBOX_SELECTED, &WxSurfacePropertiesPanel::OnAlignSelChange );
	ADDEVENTHANDLER( XRCID("IDPB_APPLYALIGN"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfacePropertiesPanel::OnApplyAlign);

	// Initialize controls.
	for( INT x = 0 ; x < GTexAlignTools.Aligners.Num() ; ++x )
	{
		AlignList->Append( *GTexAlignTools.Aligners(x)->Desc );
	}
	AlignList->SetSelection( 0 );

	//do this once before any property windows are loaded
	FLocalizeWindow( this );

	const INT PropertyWindowID = -1;
	const UBOOL bShowPropertyWindowTools = FALSE;

	// Property window for the texture alignment tools.
	{
		wxPanel* PropertyPanel;
		PropertyPanel = (wxPanel*)FindWindow( XRCID( "ID_PROPERTYWINDOW_PANEL" ) );
		check( PropertyPanel );

		wxStaticBoxSizer* PropertySizer = new wxStaticBoxSizer(wxVERTICAL, PropertyPanel, TEXT("Options"));
		{
			PropertyWindow = new WxPropertyWindowHost;
			PropertyWindow->Create( PropertyPanel, GUnrealEd, PropertyWindowID, bShowPropertyWindowTools);
			
			wxSize PanelSize = PropertyPanel->GetSize();
			PanelSize.SetWidth(PanelSize.GetWidth() - 5);
			PanelSize.SetHeight(PanelSize.GetHeight() - 15);
			PropertyWindow->SetMinSize(PanelSize);

			PropertySizer->Add(PropertyWindow, 1, wxEXPAND);
		}
		PropertyPanel->SetSizer(PropertySizer);
		PropertySizer->Fit(PropertyPanel);
	}

	// Property window for the surface lighting channels.
	{
		wxPanel* PropertyPanel;
		PropertyPanel = (wxPanel*)FindWindow( XRCID( "ID_ChannelsPropertyWindow" ) );
		check( PropertyPanel );

		wxStaticBoxSizer* PropertySizer = new wxStaticBoxSizer(wxVERTICAL, PropertyPanel, TEXT("LightingChannels"));
		{
			ChannelsPropertyWindow = new WxPropertyWindowHost;
			ChannelsPropertyWindow->Create( PropertyPanel, this, PropertyWindowID, bShowPropertyWindowTools);
			
			wxSize PanelSize = PropertyPanel->GetSize();
			PanelSize.SetWidth(PanelSize.GetWidth() - 5);
			PanelSize.SetHeight(PanelSize.GetHeight() - 15);
			ChannelsPropertyWindow->SetMinSize(PanelSize);

			PropertySizer->Add(ChannelsPropertyWindow, 1, wxEXPAND);
		}
		PropertyPanel->SetSizer(PropertySizer);
		PropertySizer->Fit(PropertyPanel);
	}

	// Allocate a new surface lighting channels object.
	LightingChannelsObject = ConstructObject<ULightingChannelsObject>( ULightingChannelsObject::StaticClass() );

	// Property window for the surface Lightmass properties.
	{
		wxPanel* PropertyPanel;
		PropertyPanel = (wxPanel*)FindWindow( XRCID( "ID_LightmassPropertyWindow" ) );
		check( PropertyPanel );

		wxStaticBoxSizer* PropertySizer = new wxStaticBoxSizer(wxVERTICAL, PropertyPanel, TEXT("LightmassSettings"));
		{
			LightmassPropertyWindow = new WxPropertyWindowHost;
			LightmassPropertyWindow->Create( PropertyPanel, this, PropertyWindowID, bShowPropertyWindowTools );
			
			wxSize PanelSize = PropertyPanel->GetSize();
			PanelSize.SetWidth(PanelSize.GetWidth() - 5);
			PanelSize.SetHeight(PanelSize.GetHeight() - 15);
			LightmassPropertyWindow->SetMinSize(PanelSize);

			PropertySizer->Add(LightmassPropertyWindow, 1, wxEXPAND);
		}
		PropertyPanel->SetSizer(PropertySizer);
		PropertySizer->Fit(PropertyPanel);
	}
}

void WxSurfacePropertiesPanel::Pan( INT InU, INT InV )
{
	const FLOAT Mod = GetAsyncKeyState(VK_SHIFT) & 0x8000 ? -1.f : 1.f;
	GUnrealEd->Exec( *FString::Printf( TEXT("POLY TEXPAN U=%f V=%f"), InU * Mod, InV * Mod ) );
}

void WxSurfacePropertiesPanel::Scale( FLOAT InScaleU, FLOAT InScaleV, UBOOL InRelative )
{
	if( InScaleU == 0.f )
	{
		InScaleU = 1.f;
	}
	if( InScaleV == 0.f )
	{
		InScaleV = 1.f;
	}

	InScaleU = 1.0f / InScaleU;
	InScaleV = 1.0f / InScaleV;

	GUnrealEd->Exec( *FString::Printf( TEXT("POLY TEXSCALE %s UU=%f VV=%f"), InRelative?TEXT("RELATIVE"):TEXT(""), InScaleU, InScaleV ) );
}

void WxSurfacePropertiesPanel::RefreshPage()
{
	// Find the shadowmap scale on the selected surfaces.

	FLOAT	ShadowMapScale				= 0.0f;
	UBOOL	bValidScale					= FALSE;
	// Keep track of how many surfaces are selected and how many have the options set.
	INT		AcceptsLightsCount			= 0;
	INT		AcceptsDynamicLightsCount	= 0;
	INT		ForceLightMapCount			= 0;
	INT		SelectedSurfaceCount		= 0;

	FString MaterialNameForTitle;

	LightingChannelsObject->LightingChannels.Bitfield = 0;
	LightingChannelsObject->LightingChannels.bInitialized = TRUE;

	LevelLightmassSettingsObjects.Empty();
	SelectedLightmassSettingsObjects.Empty();

	UBOOL bSawSelectedSurface = FALSE;
	if ( GWorld )
	{
		for ( INT LevelIndex = 0 ; LevelIndex < GWorld->Levels.Num() ; ++LevelIndex )
		{
			const ULevel* Level = GWorld->Levels(LevelIndex);
			const UModel* Model = Level->Model;

			INT NewIndex = LevelLightmassSettingsObjects.AddZeroed(1);
			TLightmassSettingsObjectArray& ObjArray = LevelLightmassSettingsObjects(NewIndex);

			for(INT SurfaceIndex = 0; SurfaceIndex < Model->Surfs.Num();SurfaceIndex++)
			{
				const FBspSurf&	Surf =  Model->Surfs(SurfaceIndex);

				if(Surf.PolyFlags & PF_Selected)
				{
					bSawSelectedSurface = TRUE;
					LightingChannelsObject->LightingChannels.Bitfield |= Surf.LightingChannels.Bitfield;

					if(SelectedSurfaceCount == 0)
					{
						ShadowMapScale = Surf.ShadowMapScale;
						bValidScale = TRUE;
					}
					else if(bValidScale && ShadowMapScale != Surf.ShadowMapScale)
					{
						bValidScale = FALSE;
					}

					// Keep track of selected surface count and how many have the particular options set so we can display
					// the correct state of checked, unchecked or mixed.
					SelectedSurfaceCount++;
					if( Surf.PolyFlags & PF_AcceptsLights )
					{
						AcceptsLightsCount++;
					}
					if( Surf.PolyFlags & PF_AcceptsDynamicLights )
					{
						AcceptsDynamicLightsCount++;
					}
					if( Surf.PolyFlags & PF_ForceLightMap )
					{
						ForceLightMapCount++;
					}

					FLightmassPrimitiveSettings TempSettings = Model->LightmassSettings(Surf.iLightmassIndex);
					INT FoundIndex = INDEX_NONE;
					for (INT CheckIndex = 0; CheckIndex < ObjArray.Num(); CheckIndex++)
					{
						if (ObjArray(CheckIndex)->LightmassSettings == TempSettings)
						{
							FoundIndex = CheckIndex;
							break;
						}
					}
					if (FoundIndex == INDEX_NONE)
					{
						ULightmassPrimitiveSettingsObject* LightmassSettingsObject = 
							ConstructObject<ULightmassPrimitiveSettingsObject>(ULightmassPrimitiveSettingsObject::StaticClass());
						LightmassSettingsObject->LightmassSettings = TempSettings;
						ObjArray.AddItem(LightmassSettingsObject);
						SelectedLightmassSettingsObjects.AddItem(LightmassSettingsObject);
					}

					//Find Material Name for title
					FString NewName;
					if (Surf.Material)
					{
						NewName = Surf.Material->GetName();
					}
					else
					{
						NewName = LocalizeUnrealEd("SurfaceProperties_NoMaterial");;
					}
					//if we've never assigned a material before
					if (MaterialNameForTitle.Len() == 0)
					{
						MaterialNameForTitle = NewName;
					}
					else
					{
						//already been assigned once, check if it's the same
						if (NewName != MaterialNameForTitle)
						{
							MaterialNameForTitle = LocalizeUnrealEd("SurfaceProperties_MultipleMaterials");
						}
					}
				}
			}
		}
	}

	// Select the appropriate scale.
	FString ScaleString;
	if( bValidScale )
	{
		ScaleString = FString::Printf(TEXT("%.1f"),ShadowMapScale);
	}
	else
	{
		ScaleString = TEXT("");
	}
	bSettingLightMapRes = TRUE;
	LightMapResEdit->SetValue(*ScaleString);
	bSettingLightMapRes = FALSE;
	LightMapResEdit->SetEditable(TRUE);

	// Set AcceptsLights state.
	if( AcceptsLightsCount == 0 )
	{
		AcceptsLightsCheck->Set3StateValue( wxCHK_UNCHECKED );
	}
	else if( AcceptsLightsCount == SelectedSurfaceCount )
	{
		AcceptsLightsCheck->Set3StateValue( wxCHK_CHECKED );
	}
	else
	{
		AcceptsLightsCheck->Set3StateValue( wxCHK_UNDETERMINED );
	}

	// Set AcceptsDynamicLights state.
	if( AcceptsDynamicLightsCount == 0 )
	{
		AcceptsDynamicLightsCheck->Set3StateValue( wxCHK_UNCHECKED );
	}
	else if( AcceptsDynamicLightsCount == SelectedSurfaceCount )
	{
		AcceptsDynamicLightsCheck->Set3StateValue( wxCHK_CHECKED );
	}
	else
	{
		AcceptsDynamicLightsCheck->Set3StateValue( wxCHK_UNDETERMINED );
	}

	// Set ForceLightMap state.
	if( ForceLightMapCount == 0 )
	{
		ForceLightMapCheck->Set3StateValue( wxCHK_UNCHECKED );
	}
	else if( ForceLightMapCount == SelectedSurfaceCount )
	{
		ForceLightMapCheck->Set3StateValue( wxCHK_CHECKED );
	}
	else
	{
		ForceLightMapCheck->Set3StateValue( wxCHK_UNDETERMINED );
	}

	// Refresh property windows.
	PropertyWindow->Freeze();
	PropertyWindow->SetObject( GTexAlignTools.Aligners(AlignList->GetSelection()), EPropertyWindowFlags::NoFlags );
	PropertyWindow->Thaw();
	ChannelsPropertyWindow->Freeze();
	if ( bSawSelectedSurface )
	{
		ChannelsPropertyWindow->SetObject( LightingChannelsObject, EPropertyWindowFlags::NoFlags );
		ChannelsPropertyWindow->ExpandAllItems();
		LightmassPropertyWindow->SetObjectArray(SelectedLightmassSettingsObjects, EPropertyWindowFlags::NoFlags );
		LightmassPropertyWindow->ExpandAllItems();
	}
	else
	{
		ChannelsPropertyWindow->RemoveAllObjects();
		LightmassPropertyWindow->RemoveAllObjects();
	}
	ChannelsPropertyWindow->Thaw();

	WxDlgSurfaceProperties* ParentDlg = wxDynamicCast(GetParent(), WxDlgSurfaceProperties);
	if (ParentDlg != NULL)
	{
		FString NewTitleString;
		if (SelectedSurfaceCount)
		{
			NewTitleString = FString::Printf(LocalizeSecure(LocalizeUnrealEd("SurfaceProperties_Title"), SelectedSurfaceCount, *MaterialNameForTitle));
		}
		else
		{
			NewTitleString = LocalizeUnrealEd("SurfaceProperties_EmptyTitle");
		}
		ParentDlg->SetTitle(*NewTitleString);
	}
}

/** Sets lighting channels for selected surfaces to the specified value. */
void WxSurfacePropertiesPanel::SetLightingChannelsForSelectedSurfaces(DWORD NewBitfield)
{
	UBOOL bSawLightingChannelsChange = FALSE;
	for ( INT LevelIndex = 0 ; LevelIndex < GWorld->Levels.Num() ; ++LevelIndex )
	{
		ULevel* Level = GWorld->Levels(LevelIndex);
		UModel* Model = Level->Model;
		for ( INT SurfaceIndex = 0 ; SurfaceIndex < Model->Surfs.Num() ; ++SurfaceIndex )
		{
			FBspSurf& Surf = Model->Surfs(SurfaceIndex);
			if ( (Surf.PolyFlags&PF_Selected) != 0 && Surf.Actor != NULL )
			{
				if ( Surf.LightingChannels.Bitfield != NewBitfield )
				{
					Surf.LightingChannels.Bitfield = NewBitfield;
					Surf.Actor->Brush->Polys->Element(Surf.iBrushPoly).LightingChannels = NewBitfield;
					bSawLightingChannelsChange = TRUE;
				}
			}
		}
	}
	if ( bSawLightingChannelsChange )
	{
		GWorld->MarkPackageDirty();
		GCallbackEvent->Send( CALLBACK_LevelDirtied );
	}
}

/** Sets Lightmass settings for selected surfaces to the specified value. */
void WxSurfacePropertiesPanel::SetLightmassSettingsForSelectedSurfaces(FLightmassPrimitiveSettings& InSettings)
{
	UBOOL bSawLightmassSettingsChange = FALSE;
	for (INT LevelIndex = 0 ; LevelIndex < GWorld->Levels.Num() ; ++LevelIndex)
	{
		ULevel* Level = GWorld->Levels(LevelIndex);
		UModel* Model = Level->Model;
		for (INT SurfaceIndex = 0 ; SurfaceIndex < Model->Surfs.Num() ; ++SurfaceIndex)
		{
			FBspSurf& Surf = Model->Surfs(SurfaceIndex);
			if (((Surf.PolyFlags&PF_Selected) != 0) && (Surf.Actor != NULL))
			{
				INT LookupIndex = Clamp<INT>(Surf.iLightmassIndex, 0, Model->LightmassSettings.Num());
				FLightmassPrimitiveSettings& Settings = Model->LightmassSettings(LookupIndex);
 				if (!(Settings == InSettings))
 				{
					// See if we can find the one of interest...
					INT FoundLightmassIndex = INDEX_NONE;
					if (Model->LightmassSettings.FindItem(InSettings, FoundLightmassIndex) == FALSE)
					{
						FoundLightmassIndex = Model->LightmassSettings.AddItem(InSettings);
					}
					Surf.iLightmassIndex = FoundLightmassIndex;
//  					Settings.EmissiveBoost = InSettings.EmissiveBoost;
//  					Settings.DiffuseBoost = InSettings.DiffuseBoost;
//  					Settings.SpecularBoost = InSettings.SpecularBoost;
 					bSawLightmassSettingsChange = TRUE;
					Surf.Actor->Brush->Polys->Element(Surf.iBrushPoly).LightmassSettings = InSettings;
 				}
			}
		}

		// Clean out unused Lightmass settings from the model...
		if (bSawLightmassSettingsChange)
		{
			TArray<UBOOL> UsedIndices;
			UsedIndices.Empty(Model->LightmassSettings.Num());
			UsedIndices.AddZeroed(Model->LightmassSettings.Num());
			for (INT SurfaceIndex = 0 ; SurfaceIndex < Model->Surfs.Num() ; ++SurfaceIndex)
			{
				FBspSurf& Surf = Model->Surfs(SurfaceIndex);
				if (Surf.Actor != NULL)
				{
					if ((Surf.iLightmassIndex >= 0) && (Surf.iLightmassIndex < Model->LightmassSettings.Num()))
					{
						UsedIndices(Surf.iLightmassIndex) = TRUE;
					}
				}
			}

			for (INT UsedIndex = UsedIndices.Num() - 1; UsedIndex >= 0; UsedIndex--)
			{
				if (UsedIndices(UsedIndex) == FALSE)
				{
					Model->LightmassSettings.Remove(UsedIndex);
					for (INT SurfaceIndex = 0 ; SurfaceIndex < Model->Surfs.Num() ; ++SurfaceIndex)
					{
						FBspSurf& Surf = Model->Surfs(SurfaceIndex);
						if (Surf.Actor != NULL)
						{
							check (Surf.iLightmassIndex != UsedIndex);
							if (Surf.iLightmassIndex > UsedIndex)
							{
								Surf.iLightmassIndex--;
								check(Surf.iLightmassIndex >= 0);
							}
						}
					}
				}
			}
		}
	}
	if (bSawLightmassSettingsChange)
	{
		GWorld->MarkPackageDirty();
		GCallbackEvent->Send( CALLBACK_LevelDirtied );
	}
}

void WxSurfacePropertiesPanel::NotifyPostChange( void* Src, UProperty* PropertyThatChanged )
{
	SetLightingChannelsForSelectedSurfaces( LightingChannelsObject->LightingChannels.Bitfield );
	if (SelectedLightmassSettingsObjects.Num() > 0)
	{
		SetLightmassSettingsForSelectedSurfaces(SelectedLightmassSettingsObjects(0)->LightmassSettings);
	}
}

void WxSurfacePropertiesPanel::OnU1( wxCommandEvent& In )
{
	Pan( 1, 0 );
}

void WxSurfacePropertiesPanel::OnU4( wxCommandEvent& In )
{
	Pan( 4, 0 );
}

void WxSurfacePropertiesPanel::OnU16( wxCommandEvent& In )
{
	Pan( 16, 0 );
}

void WxSurfacePropertiesPanel::OnU64( wxCommandEvent& In )
{
	Pan( 64, 0 );
}

void WxSurfacePropertiesPanel::OnUCustom( wxCommandEvent& In )
{
	wxTextEntryDialog Dlg(this, *LocalizeUnrealEd(TEXT("UPanAmount")), *LocalizeUnrealEd(TEXT("PanU")));

	if(Dlg.ShowModal() == wxID_OK)
	{
		const wxString StrValue = Dlg.GetValue();
		const INT PanValue = appAtoi(StrValue);

		Pan( PanValue, 0 );
	}

}

void WxSurfacePropertiesPanel::OnV1( wxCommandEvent& In )
{
	Pan( 0, 1 );
}

void WxSurfacePropertiesPanel::OnV4( wxCommandEvent& In )
{
	Pan( 0, 4 );
}

void WxSurfacePropertiesPanel::OnV16( wxCommandEvent& In )
{
	Pan( 0, 16 );
}

void WxSurfacePropertiesPanel::OnV64( wxCommandEvent& In )
{
	Pan( 0, 64 );
}

void WxSurfacePropertiesPanel::OnVCustom( wxCommandEvent& In )
{
	wxTextEntryDialog Dlg(this, *LocalizeUnrealEd(TEXT("VPanAmount")), *LocalizeUnrealEd(TEXT("PanV")));

	if(Dlg.ShowModal() == wxID_OK)
	{
		const wxString StrValue = Dlg.GetValue();
		const INT PanValue = appAtoi(StrValue);

		Pan( 0, PanValue );
	}
}

void WxSurfacePropertiesPanel::OnFlipU( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY TEXMULT UU=-1 VV=1") );
}

void WxSurfacePropertiesPanel::OnFlipV( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY TEXMULT UU=1 VV=-1") );
}

void WxSurfacePropertiesPanel::OnRot45( wxCommandEvent& In )
{
	const FLOAT Mod = GetAsyncKeyState(VK_SHIFT) & 0x8000 ? -1.f : 1.f;
	const FLOAT UU = 1.0f / appSqrt(2.f);
	const FLOAT VV = 1.0f / appSqrt(2.f);
	const FLOAT UV = (1.0f / appSqrt(2.f)) * Mod;
	const FLOAT VU = -(1.0f / appSqrt(2.f)) * Mod;
	GUnrealEd->Exec( *FString::Printf( TEXT("POLY TEXMULT UU=%f VV=%f UV=%f VU=%f"), UU, VV, UV, VU ) );
}

void WxSurfacePropertiesPanel::OnRot90( wxCommandEvent& In )
{
	const FLOAT Mod = GetAsyncKeyState(VK_SHIFT) & 0x8000 ? -1 : 1;
	const FLOAT UU = 0.f;
	const FLOAT VV = 0.f;
	const FLOAT UV = 1.f * Mod;
	const FLOAT VU = -1.f * Mod;
	GUnrealEd->Exec( *FString::Printf( TEXT("POLY TEXMULT UU=%f VV=%f UV=%f VU=%f"), UU, VV, UV, VU ) );
}

void WxSurfacePropertiesPanel::OnRotCustom( wxCommandEvent& In )
{
	wxTextEntryDialog Dlg(this, *LocalizeUnrealEd(TEXT("RotationAmount")), *LocalizeUnrealEd(TEXT("Rotation")));

	if(Dlg.ShowModal() == wxID_OK)
	{
		wxString StrValue = Dlg.GetValue();
		const FLOAT RotateDegrees = appAtof(StrValue);
		const FLOAT RotateRadians = RotateDegrees / 180.0f * PI;

		const FLOAT UU = cos(RotateRadians);
		const FLOAT VV = UU;
		const FLOAT UV = -sin(RotateRadians);
		const FLOAT VU = sin(RotateRadians);
		GUnrealEd->Exec( *FString::Printf( TEXT("POLY TEXMULT UU=%f VV=%f UV=%f VU=%f"), UU, VV, UV, VU ) );
	}


}

void WxSurfacePropertiesPanel::OnApply( wxCommandEvent& In )
{
	FLOAT UScale, VScale;

	if( bUseSimpleScaling )
	{
		UScale = VScale = appAtof( SimpleCB->GetValue() );
	}
	else
	{
		UScale = appAtof( CustomUEdit->GetValue() );
		VScale = appAtof( CustomVEdit->GetValue() );
	}

	const UBOOL bRelative = RelativeCheck->GetValue();
	Scale( UScale, VScale, bRelative );
}

void WxSurfacePropertiesPanel::OnScaleSimple( wxCommandEvent& In )
{
	bUseSimpleScaling = TRUE;

	CustomULabel->Disable();
	CustomVLabel->Disable();
	CustomUEdit->Disable();
	CustomVEdit->Disable();
	SimpleCB->Enable();
}

void WxSurfacePropertiesPanel::OnScaleCustom( wxCommandEvent& In )
{
	bUseSimpleScaling = FALSE;

	CustomULabel->Enable();
	CustomVLabel->Enable();
	CustomUEdit->Enable();
	CustomVEdit->Enable();
	SimpleCB->Disable();
}

void WxSurfacePropertiesPanel::OnLightMapResChange( wxCommandEvent& In )
{
	if (bSettingLightMapRes == TRUE)
	{
		return;
	}

	const FLOAT ShadowMapScale = Clamp<FLOAT>(appAtof(LightMapResEdit->GetValue()), 0.0f, 65536.0);

	UBOOL bSawLightingChannelsChange = FALSE;
	for ( INT LevelIndex = 0 ; LevelIndex < GWorld->Levels.Num() ; ++LevelIndex )
	{
		ULevel* Level = GWorld->Levels(LevelIndex);
		UModel* Model = Level->Model;
		for(INT SurfaceIndex = 0;SurfaceIndex < Model->Surfs.Num();SurfaceIndex++)
		{
			FBspSurf&	Surf = Model->Surfs(SurfaceIndex);
			if ( (Surf.PolyFlags&PF_Selected) != 0 && Surf.Actor != NULL )
			{
				Surf.Actor->Brush->Polys->Element(Surf.iBrushPoly).ShadowMapScale = ShadowMapScale;
				Surf.ShadowMapScale = ShadowMapScale;
				bSawLightingChannelsChange = TRUE;
			}
		}
	}

	if ( bSawLightingChannelsChange )
	{
		GWorld->MarkPackageDirty();
		GCallbackEvent->Send( CALLBACK_LevelDirtied );
	}
}

/**
 * Sets passed in poly flag on selected surfaces.
 *
 * @param PolyFlag	PolyFlag to toggle on selected surfaces
 * @param Value		Value to set the flag to.
 */
void WxSurfacePropertiesPanel::SetPolyFlag( DWORD PolyFlag, UBOOL Value )
{
	check( PolyFlag & PF_ModelComponentMask );
	{
		const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("ToggleSurfaceFlags")) );
		for ( INT LevelIndex = 0 ; LevelIndex < GWorld->Levels.Num() ; ++LevelIndex )
		{
			ULevel* Level = GWorld->Levels(LevelIndex);
			UModel* Model = Level->Model;
			Model->ModifySelectedSurfs( TRUE );

			for( INT SurfaceIndex=0; SurfaceIndex<Model->Surfs.Num(); SurfaceIndex++ )
			{
				FBspSurf& Surf = Model->Surfs(SurfaceIndex);
				if( Surf.PolyFlags & PF_Selected )
				{
					if(Value)
					{
						Surf.PolyFlags |= PolyFlag;
					}
					else
					{
						Surf.PolyFlags &= ~PolyFlag;
					}

					// Propagate toggled flags to poly.
					GEditor->polyUpdateMaster( Model, SurfaceIndex, TRUE );

					// If any surface in this level is updated, flag the level as
					// needing a rebuild prior to lighting for correct results
					Level->bGeometryDirtyForLighting = TRUE;
				}
			}
		}
	}

	GWorld->MarkPackageDirty();
	GCallbackEvent->Send( CALLBACK_LevelDirtied );
}

void WxSurfacePropertiesPanel::OnAcceptsLightsChange( wxCommandEvent& In )
{
	SetPolyFlag( PF_AcceptsLights,  In.IsChecked());
}
void WxSurfacePropertiesPanel::OnAcceptsDynamicLightsChange( wxCommandEvent& In )
{
	SetPolyFlag( PF_AcceptsDynamicLights,  In.IsChecked() );
}
void WxSurfacePropertiesPanel::OnForceLightmapChange( wxCommandEvent& In )
{
	SetPolyFlag( PF_ForceLightMap,  In.IsChecked() );
}



void WxSurfacePropertiesPanel::OnAlignSelChange( wxCommandEvent& In )
{
	RefreshPage();
}

void WxSurfacePropertiesPanel::OnApplyAlign( wxCommandEvent& In )
{
	UTexAligner* SelectedAligner = GTexAlignTools.Aligners( AlignList->GetSelection() );
	for ( INT LevelIndex = 0 ; LevelIndex < GWorld->Levels.Num() ; ++LevelIndex )
	{
		ULevel* Level = GWorld->Levels(LevelIndex);
		SelectedAligner->Align( TEXALIGN_None, Level->Model );
	}
}

/*-----------------------------------------------------------------------------
	WxDlgSurfaceProperties.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgSurfaceProperties, wxDialog)
EVT_IDLE(WxDlgSurfaceProperties::OnIdle)
END_EVENT_TABLE()

WxDlgSurfaceProperties::WxDlgSurfaceProperties() : 
wxDialog(GApp->EditorFrame, wxID_ANY, TEXT("SurfaceProperties"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxSYSTEM_MENU | wxCAPTION)
{
	bDirty = TRUE;

	//do this once before any children are loaded
	FLocalizeWindow( this );

	wxBoxSizer* DialogSizer = new wxBoxSizer(wxVERTICAL);
	{
		PropertiesPanel = new WxSurfacePropertiesPanel( this );
		DialogSizer->Add( PropertiesPanel, 1, wxEXPAND );
	}
	SetSizer(DialogSizer);

	
	FWindowUtil::LoadPosSize( TEXT("SurfaceProperties"), this );

	Fit();
	RefreshPages();
}

WxDlgSurfaceProperties::~WxDlgSurfaceProperties()
{
	FWindowUtil::SavePosSize( TEXT("SurfaceProperties"), this );
}

void WxDlgSurfaceProperties::RefreshPages()
{
	PropertiesPanel->RefreshPage();
}

/**
 * Event handler for when a the window is idle.
 *
 * @param	In	Information about the event.
 */
void WxDlgSurfaceProperties::OnIdle(wxIdleEvent &In)
{
	//no need to go through the trouble when it's not visible
	if (bDirty && IsShownOnScreen())
	{
		RefreshPages();
		GApp->EditorFrame->UpdateUI();

		//mark up to date
		bDirty = FALSE;
	}
}

/*-----------------------------------------------------------------------------
	WxDlgColor.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgColor, wxColourDialog)
EVT_BUTTON( wxID_OK, WxDlgColor::OnOK )
END_EVENT_TABLE()

WxDlgColor::WxDlgColor()
{
}

WxDlgColor::~WxDlgColor()
{
	wxColourData cd = GetColourData();
	SaveColorData( &cd );
}

bool WxDlgColor::Create( wxWindow* InParent, wxColourData* InData )
{
	LoadColorData( InData );

	return wxColourDialog::Create( InParent, InData );
}

/**
* Loads custom color data from the INI file.
*
* @param	InData	The color data structure to load into
*/

void WxDlgColor::LoadColorData( wxColourData* InData )
{
	for( INT x = 0 ; x < 16 ; ++x )
	{
		const FString Key = FString::Printf( TEXT("Color%d"), x );
		INT Color = wxColour(255,255,255).GetPixel();
		GConfig->GetInt( TEXT("ColorDialog"), *Key, Color, GEditorUserSettingsIni );
		InData->SetCustomColour( x, wxColour( Color ) );
	}
}

int WxDlgColor::ShowModal()
{
	// Get the current mouse cursor position so the dialog can be placed near the mouse when shown
	POINT CurMouseCursorPos;
	if ( ::GetCursorPos( &CurMouseCursorPos ) )
	{
		// Try to center the dialog under the mouse, based on half its width and height
		const wxRect& DlgColorScreenRect = GetScreenRect();
		Move( CurMouseCursorPos.x - ( DlgColorScreenRect.GetWidth() >> 1 ), CurMouseCursorPos.y - ( DlgColorScreenRect.GetHeight() >> 1 ) );
	}

	GCallbackEvent->Send( CALLBACK_EditorPreModal );
	MakeModal(true);
	const int ReturnCode = wxColourDialog::ShowModal();
	MakeModal(false);
	GCallbackEvent->Send( CALLBACK_EditorPostModal );


	return ReturnCode;
}

/**
* Saves custom color data to the INI file.
*
* @param	InData	The color data structure to save from
*/

void WxDlgColor::SaveColorData( wxColourData* InData )
{
	for( INT x = 0 ; x < 16 ; ++x )
	{
		const FString Key = FString::Printf( TEXT("Color%d"), x );
		GConfig->SetInt( TEXT("ColorDialog"), *Key, InData->GetCustomColour(x).GetPixel(), GEditorUserSettingsIni );
	}
}

void WxDlgColor::OnOK(wxCommandEvent& event)
{
	wxDialog::AcceptAndClose();
}

/**
 * General function for choosing a color.  Implementation can be wx or WPF
 * @param ColorStruct - Contains pointers to data to update, windows to refresh and parameters for how to display the dialog
 */
ColorPickerConstants::ColorPickerResults PickColor(const FPickColorStruct& ColorStruct)
{
#if WITH_MANAGED_CODE
	return PickColorWPF(ColorStruct);
#else
	return ColorPickerConstants::ColorRejected;
#endif
}
/*-----------------------------------------------------------------------------
	WxDlgLoadErrors.
-----------------------------------------------------------------------------*/

/**
 * Construct a WxDlgLoadErrors dialog
 *
 * @param	InParent	Parent window of this dialog
 */
WxDlgLoadErrors::WxDlgLoadErrors( wxWindow* InParent )
{
	// Load the main dialog from the xrc file
	const bool bSuccess = wxXmlResource::Get()->LoadDialog( this, InParent, TEXT("ID_DLG_LOAD_ERRORS") );
	check( bSuccess );

	// Load the two text controls from the xrc file
	PackageErrors = wxDynamicCast( FindWindow( XRCID( "IDLB_FILES" ) ), wxTextCtrl );
	check( PackageErrors != NULL );
	ObjectErrors = wxDynamicCast( FindWindow( XRCID( "IDLB_OBJECTS" ) ), wxTextCtrl );
	check( ObjectErrors != NULL );

	FLocalizeWindow( this );
}

/**
 * Update the dialog with all of the current load errors and adjust its size based upon the length of the
 * errors.
 */
void WxDlgLoadErrors::Update()
{
	// Remove any existing errors in the dialog
	PackageErrors->Clear();
	ObjectErrors->Clear();

	// Get monitor information for the monitor this window is primarily on
	HWND Wnd = static_cast<HWND>( GetHandle() );
	HMONITOR Monitor = MonitorFromWindow( Wnd, MONITOR_DEFAULTTOPRIMARY );
	MONITORINFO MonitorInfo;
	MonitorInfo.cbSize = sizeof( MONITORINFO );
	GetMonitorInfo( Monitor, &MonitorInfo );

	// Calculate the maximum width to allow for either error column
	// Each error column should be given a little under half of the screen's width to potentially grow to
	const INT SpacingOffset = 25;
	const INT MaxWidthPerErrorColumn = abs( MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left )/2 - SpacingOffset;

	INT MaxPackageErrorWidth = 0;
	INT MaxObjectErrorWidth = 0;

	// Iterate through all of the global load errors and add each one to the dialog
	for( TArray<FEdLoadError>::TConstIterator ErrorIterator( GEdLoadErrors ); ErrorIterator; ++ErrorIterator )
	{
		FString ErrorString = FString::Printf(TEXT("%s\n"), *( *ErrorIterator ).Desc );
		
		wxSize StrSize;
		
		// Place file error types in the package column
		if( ( *ErrorIterator ).Type == FEdLoadError::TYPE_FILE )
		{
			// Compute the width of the string to see if the dialog should be resized to accommodate it
			PackageErrors->GetTextExtent( *ErrorString, &StrSize.x, NULL );
			if ( StrSize.x > MaxPackageErrorWidth )
			{
				MaxPackageErrorWidth = StrSize.x;
			}
			PackageErrors->AppendText( *ErrorString );
		}
		// Place resource errors in the object column
		else
		{
			// Compute the width of the string to see if the dialog should be resized to accommodate it
			ObjectErrors->GetTextExtent( *ErrorString, &StrSize.x, NULL );
			if ( StrSize.x > MaxObjectErrorWidth )
			{
				MaxObjectErrorWidth = StrSize.x;
			}
			ObjectErrors->AppendText( *ErrorString );
		}
	}
	
	const wxSize& PackageErrorSize = PackageErrors->GetSize();
	const wxSize& ObjectErrorSize = ObjectErrors->GetSize();

	// Resize the dialog columns appropriately with respect to string sizes
	// Don't allow any one column to exceed the calculated maximum from above, based on the user's screen resolution
	if ( MaxPackageErrorWidth + SpacingOffset <= MaxWidthPerErrorColumn )
	{
		PackageErrors->SetSizeHints( MaxPackageErrorWidth + SpacingOffset, PackageErrorSize.GetHeight() );
	}
	else
	{
		PackageErrors->SetSizeHints( MaxWidthPerErrorColumn, PackageErrorSize.GetHeight() );
	}
	if ( MaxObjectErrorWidth + SpacingOffset <= MaxWidthPerErrorColumn )
	{
		ObjectErrors->SetSizeHints( MaxObjectErrorWidth + SpacingOffset, ObjectErrorSize.GetHeight() );
	}
	else
	{
		ObjectErrors->SetSizeHints( MaxWidthPerErrorColumn, ObjectErrorSize.GetHeight() );
	}

	// Adjust the dialog to fit to the newly sized columns
	Fit();
	Center();
}

/** Pass through bitmap */

class WxPassThroughStaticBitmap : public wxStaticBitmap
{
	/**
	 * To pass event back up to the parent window
	 */
	void OnLeftClick( wxMouseEvent& In )
	{
		//just go up to the parent
		In.ResumePropagation(1);
		//allows the parent to handle the event
		TryParent(In);
		In.ResumePropagation(0);	//now that we've hand propagated, don't bother trying in the base case.
		In.Skip();
	}

	DECLARE_EVENT_TABLE()
};
BEGIN_EVENT_TABLE(WxPassThroughStaticBitmap, wxStaticBitmap)
	EVT_LEFT_DOWN(WxPassThroughStaticBitmap::OnLeftClick)
END_EVENT_TABLE()

/*-----------------------------------------------------------------------------
WxDlgEditPlayWorldURL.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgEditPlayWorldURL, wxDialog)
	EVT_COMBOBOX( IDM_BuildPlay_Resolution, WxDlgEditPlayWorldURL::OnResolutionChanged )
	EVT_TEXT(IDM_BuildPlay_CustomResolutionWidth, WxDlgEditPlayWorldURL::CommitCustomResolution)
	EVT_TEXT(IDM_BuildPlay_CustomResolutionHeight, WxDlgEditPlayWorldURL::CommitCustomResolution)
	EVT_RADIOBUTTON(IDM_BuildPlay_Landscape, WxDlgEditPlayWorldURL::OnChangeOrientation)
	EVT_RADIOBUTTON(IDM_BuildPlay_Portrait, WxDlgEditPlayWorldURL::OnChangeOrientation)
	EVT_LEFT_DOWN(WxDlgEditPlayWorldURL::OnLeftClick)
	EVT_COMBOBOX( IDM_BuildPlay_Mobile_Features, WxDlgEditPlayWorldURL::OnFeaturesChanged )
END_EVENT_TABLE()

WxDlgEditPlayWorldURL::WxDlgEditPlayWorldURL( const FString& InStartURL, const UBOOL bUseMobilePreview, const UBOOL bAdjustableResolution ) 
	: URLTextBox(NULL) 
	, URL(InStartURL)
	, wxDialog(GApp->EditorFrame, wxID_ANY, *LocalizeUnrealEd(TEXT("EditPlayWorldURLDlg_Title")), wxDefaultPosition, wxDefaultSize, wxCAPTION  )
	, ResolutionComboBox(NULL)
	, DialogSizer(NULL)
	, TitleSizer(NULL)
	, WidgetSizer(NULL)
	, ResolutionTitleSizer(NULL)
	, ResolutionSizer(NULL)
	, CustomResolutionTitleSizer(NULL)
	, CustomResolutionSizer(NULL)
	, MobileOrientationTitleSizer(NULL)
	, MobileOrientationSizer(NULL)
	, MobileFeaturesTitleSizer(NULL)
	, MobileFeaturesSizer(NULL)
	, XSizeTextCtrl(NULL)
	, YSizeTextCtrl(NULL)
	, LandscapeModeRadioButton(NULL)
	, LandscapeStaticImage(NULL)
	, PortraitModeRadioButton(NULL)
	, PortaitStaticImage(NULL)
	, PlayButton(NULL)
	, OkButton(NULL)
	, CancelButton(NULL)
	, bInited(FALSE)
{
	// Setup the main size for this dialog
	DialogSizer = new wxBoxSizer( wxHORIZONTAL );
	SetSizer( DialogSizer );

	// Setup a sizer for the titles
	TitleSizer = CreateNewSizer(DialogSizer, wxVERTICAL);

	// Setup a sizer for the titles
	WidgetSizer = CreateNewSizer(DialogSizer, wxVERTICAL);

	BuildUrlOptions(InStartURL);

	//Resolution Settings
	if (bAdjustableResolution || bUseMobilePreview)
	{
		BuildResolutionOptions(bUseMobilePreview);
		BuildCustomResolutionOptions ();
		BuildOrientationOptions();
		BuildMobileFeatureOptions();

		//wait until after all resolution widgets have been built
		//Get default index based on resolution, bUseMobilePreview
		INT SelectedIndex = GetCurrentResolutionIndex(bUseMobilePreview);
		ResolutionComboBox->Select( SelectedIndex >= 0 ? SelectedIndex : BPD_DEFAULT );
		UpdateCustomResolution(SelectedIndex);

		TitleSizer->Show(ResolutionTitleSizer, bAdjustableResolution || bUseMobilePreview);
		WidgetSizer->Show(ResolutionSizer, bAdjustableResolution || bUseMobilePreview);
		TitleSizer->Show(MobileOrientationTitleSizer, (bUseMobilePreview == TRUE));
		WidgetSizer->Show(MobileOrientationSizer, (bUseMobilePreview == TRUE));
		TitleSizer->Show(MobileFeaturesTitleSizer, (bUseMobilePreview == TRUE));
		WidgetSizer->Show(MobileFeaturesSizer, (bUseMobilePreview == TRUE));
	}

	// Setup the buttons on the dialog
	PlayButton = new wxButton( this, Option_PlayWithURL, *LocalizeUnrealEd( TEXT("EditPlayWorldURLDlg_PlayUsingURL") ), wxDefaultPosition, wxDefaultSize, 0 );
	OkButton = new wxButton( this, Option_SaveURL, *LocalizeUnrealEd( TEXT("EditPlayWorldURLDlg_Save") ), wxDefaultPosition, wxDefaultSize, 0 );
	CancelButton = new wxButton( this, Option_Cancel, *LocalizeUnrealEd( TEXT("Cancel") ), wxDefaultPosition, wxDefaultSize, 0 );

	// Setup the button sizer and add the buttons to it.
	wxBoxSizer* ButtonSizer = new wxBoxSizer( wxHORIZONTAL );
	WidgetSizer->Add( ButtonSizer, 0, wxALIGN_RIGHT|wxALL, 5 );

	ButtonSizer->Add( PlayButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	ButtonSizer->Add( OkButton, wxID_OK, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	ButtonSizer->Add( CancelButton, wxID_CANCEL, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	// Connect the buttons to event handlers
	this->Connect( Option_PlayWithURL, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgEditPlayWorldURL::OnButtonClicked) );
	this->Connect( Option_SaveURL, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgEditPlayWorldURL::OnButtonClicked) );
	this->Connect( Option_Cancel, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgEditPlayWorldURL::OnButtonClicked) );

	// Layout the dialog
	Fit();
	SetAutoLayout(TRUE);
	DialogSizer->SetSizeHints(this);
	Centre();

	bInited = TRUE;
}

/** Build URL (commandline) options */
void WxDlgEditPlayWorldURL::BuildUrlOptions (const FString& InStartURL)
{
	CommandlineTitleSizer = CreateNewSizer(TitleSizer, wxHORIZONTAL);
	CommandlineSizer = CreateNewSizer(WidgetSizer, wxHORIZONTAL);

	// Setup the url text box and label
	wxStaticText* Label = new wxStaticText( this, -1, *LocalizeUnrealEd( TEXT("EditPlayWorldURLDlg_URLLabel") ), wxDefaultPosition, wxDefaultSize, 0 );
	URLTextBox = new wxTextCtrl( this, -1, *InStartURL, wxDefaultPosition, wxSize(300,-1), 0 );
	CommandlineTitleSizer->Add( Label, 1, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	CommandlineSizer->Add( URLTextBox, 1, wxEXPAND | wxALL|wxALIGN_CENTER_VERTICAL, 5 );
}

static UBOOL PSVitaSupported( void )
{
	const INT ConsoleCount = FConsoleSupportContainer::GetConsoleSupportContainer()->GetNumConsoleSupports();
	for( INT CurConsoleIndex = 0; CurConsoleIndex < ConsoleCount; ++CurConsoleIndex )
	{
		FConsoleSupport* Console = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport( CurConsoleIndex );
		if( Console != NULL && appStricmp( Console->GetPlatformName(), CONSOLESUPPORT_NAME_NGP ) == 0 )
		{
			return TRUE;
		}
	}
	return FALSE;
}

/** Returns width and height based on the enumeration */
void GetDeviceResolution(const INT DeviceIndex, INT& OutWidth, INT& OutHeight)
{
	switch (DeviceIndex)
	{
	case BPD_DEFAULT:
		OutWidth = GSystemSettings.ResX;
		OutHeight = GSystemSettings.ResY;
		break;
	case BPD_IPHONE_3GS:
		OutWidth = 480;
		OutHeight = 320;
		break;
	case BPD_IPHONE_4:
	case BPD_IPHONE_4S:
		OutWidth = 960;
		OutHeight = 640;
		break;
	case BPD_IPHONE_5:
		OutWidth = 1136;
		OutHeight = 640;
		break;
	case BPD_IPOD_TOUCH_4:
		OutWidth = 960;
		OutHeight = 640;
		break;
	case BPD_IPOD_TOUCH_5:
		OutWidth = 1136;
		OutHeight = 640;
		break;
	case BPD_IPAD:
		OutWidth = 1024;
		OutHeight = 768;
		break;
	case BPD_IPAD2:
		OutWidth = 1024;
		OutHeight = 768;
		break;
	case BPD_IPAD3:
		OutWidth = 1440;
		OutHeight = 1080;
		break;
	case BPD_IPAD4:
		OutWidth = 2048;
		OutHeight = 1536;
		break;
	case BPD_IPAD_MINI:
		OutWidth = 1024;
		OutHeight = 768;
		break;
#if !UDK
	case BPD_PSVITA:
		OutWidth = 960;
		OutHeight = 544;
		break;
	case BPD_XBOX_360:
		OutWidth = 1280;
		OutHeight = 720;
		break;
	case BPD_PS3:
		OutWidth = 1280;
		OutHeight = 720;
		break;
#endif
	default:
		OutWidth = GEditor->PlayInEditorWidth;
		OutHeight = GEditor->PlayInEditorHeight;
		break;
	}
}

/** Build resolution combo box */
void WxDlgEditPlayWorldURL::BuildResolutionOptions (const UBOOL bUseMobilePreview)
{
	//create sizers
	ResolutionTitleSizer = CreateNewSizer(TitleSizer, wxHORIZONTAL);
	ResolutionSizer = CreateNewSizer(WidgetSizer, wxHORIZONTAL);

	wxString* EmptyComboBoxStrings = NULL;
	ResolutionComboBox = new WxComboBox( this, IDM_BuildPlay_Resolution, TEXT(""), wxDefaultPosition, wxSize(200, -1), 0, EmptyComboBoxStrings, wxCB_READONLY );

	INT PrintWidth, PrintHeight;

	FString ResolutionFormat = TEXT(" (%d x %d)");

	//add default width/height
	FString DefaultResolutionString = *LocalizeUnrealEd("BuildPlay_Default");
	GetDeviceResolution(BPD_DEFAULT, OUT PrintWidth, OUT PrintHeight);
	DefaultResolutionString += FString::Printf(*ResolutionFormat, PrintWidth, PrintHeight);
	ResolutionComboBox->Append( *DefaultResolutionString);

	GetDeviceResolution(BPD_CUSTOM, OUT PrintWidth, OUT PrintHeight);
	ResolutionComboBox->Append( *(LocalizeUnrealEd("BuildPlay_Custom") + FString::Printf(*ResolutionFormat, PrintWidth, PrintHeight)) );

	if (bUseMobilePreview)
	{
		//add iPhone 3gs/iPod 
		GetDeviceResolution(BPD_IPHONE_3GS, OUT PrintWidth, OUT PrintHeight);
		ResolutionComboBox->Append( *(LocalizeUnrealEd("BuildPlay_iPhone3GS") + FString::Printf(*ResolutionFormat, PrintWidth, PrintHeight)) );

		//add iPhone 4
		GetDeviceResolution(BPD_IPHONE_4, OUT PrintWidth, OUT PrintHeight);
		ResolutionComboBox->Append( *(LocalizeUnrealEd("BuildPlay_iPhone4") + FString::Printf(*ResolutionFormat, PrintWidth, PrintHeight)) );

		//add iPhone 4S
		GetDeviceResolution(BPD_IPHONE_4S, OUT PrintWidth, OUT PrintHeight);
		ResolutionComboBox->Append( *(LocalizeUnrealEd("BuildPlay_iPhone4S") + FString::Printf(*ResolutionFormat, PrintWidth, PrintHeight)) );

		//add iPhone 5
		GetDeviceResolution(BPD_IPHONE_5, OUT PrintWidth, OUT PrintHeight);
		ResolutionComboBox->Append( *(LocalizeUnrealEd("BuildPlay_iPhone5") + FString::Printf(*ResolutionFormat, PrintWidth, PrintHeight)) );

		//add iPod 4
		GetDeviceResolution(BPD_IPOD_TOUCH_4, OUT PrintWidth, OUT PrintHeight);
		ResolutionComboBox->Append( *(LocalizeUnrealEd("BuildPlay_iPodTouch4") + FString::Printf(*ResolutionFormat, PrintWidth, PrintHeight)) );

		//add iPod 5
		GetDeviceResolution(BPD_IPOD_TOUCH_5, OUT PrintWidth, OUT PrintHeight);
		ResolutionComboBox->Append( *(LocalizeUnrealEd("BuildPlay_iPodTouch5") + FString::Printf(*ResolutionFormat, PrintWidth, PrintHeight)) );

		//add iPad
		GetDeviceResolution(BPD_IPAD, OUT PrintWidth, OUT PrintHeight);
		ResolutionComboBox->Append( *(LocalizeUnrealEd("BuildPlay_iPad") + FString::Printf(*ResolutionFormat, PrintWidth, PrintHeight)) );

		// iPad 2
		GetDeviceResolution(BPD_IPAD2, OUT PrintWidth, OUT PrintHeight);
		ResolutionComboBox->Append( *(LocalizeUnrealEd("BuildPlay_iPad2") + FString::Printf(*ResolutionFormat, PrintWidth, PrintHeight)) );

		// iPad 3
		GetDeviceResolution(BPD_IPAD3, OUT PrintWidth, OUT PrintHeight);
		ResolutionComboBox->Append( *(LocalizeUnrealEd("BuildPlay_iPad3") + FString::Printf(*ResolutionFormat, PrintWidth, PrintHeight)) );

		// iPad 4
		GetDeviceResolution(BPD_IPAD4, OUT PrintWidth, OUT PrintHeight);
		ResolutionComboBox->Append( *(LocalizeUnrealEd("BuildPlay_iPad4") + FString::Printf(*ResolutionFormat, PrintWidth, PrintHeight)) );

		// iPad Mini
		GetDeviceResolution(BPD_IPAD_MINI, OUT PrintWidth, OUT PrintHeight);
		ResolutionComboBox->Append( *(LocalizeUnrealEd("BuildPlay_iPadMini") + FString::Printf(*ResolutionFormat, PrintWidth, PrintHeight)) );

#if !UDK
		// PS Vita
		if( PSVitaSupported() )
		{
			GetDeviceResolution(BPD_PSVITA, OUT PrintWidth, OUT PrintHeight);
			ResolutionComboBox->Append( *(LocalizeUnrealEd("BuildPlay_PSVita") + FString::Printf(*ResolutionFormat, PrintWidth, PrintHeight)) );
		}
#endif
	}

	wxStaticText* ResolutionLabel = new wxStaticText(this, wxID_ANY, *LocalizeUnrealEd("BuildPlay_Resolution"));
	ResolutionTitleSizer->Add( ResolutionLabel, 1, wxALL|wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 4 );
	ResolutionSizer->Add( ResolutionComboBox, 1, wxEXPAND|wxALL|wxALIGN_CENTER_VERTICAL, 4 );
}

/** Builds UI for custom resolution */
void WxDlgEditPlayWorldURL::BuildCustomResolutionOptions (void)
{
	//custom resolution sizer (for width/height text ctrls)
	CustomResolutionTitleSizer = CreateNewSizer(TitleSizer, wxHORIZONTAL);
	CustomResolutionSizer = CreateNewSizer(WidgetSizer, wxHORIZONTAL);

	//empty title
	wxStaticText* CustomTitleLabel = new wxStaticText(this, wxID_ANY, TEXT(" "));
	CustomResolutionTitleSizer->Add( CustomTitleLabel, 0, wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_LEFT, 4 );

	/** X TextField. */
	XSizeTextCtrl = new WxTextCtrl(this, IDM_BuildPlay_CustomResolutionWidth, TEXT(""));
	XSizeTextCtrl->SetMaxLength(5);
	CustomResolutionSizer->Add( XSizeTextCtrl, 0, wxEXPAND|wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_LEFT, 4 );

	/** Multiply Size Label. */
	wxStaticText* MultiplyLabel = new wxStaticText(this, wxID_ANY, *LocalizeUnrealEd("BuildPlay_Multiply"));
	CustomResolutionSizer->Add( MultiplyLabel, 0, wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_LEFT, 4 );

	/** Y TextField. */
	YSizeTextCtrl = new WxTextCtrl(this, IDM_BuildPlay_CustomResolutionHeight, TEXT(""));
	YSizeTextCtrl->SetMaxLength(5);
	CustomResolutionSizer->Add( YSizeTextCtrl, 0, wxEXPAND|wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_LEFT, 4 );
}

/** Builds UI for mobile orientation */
void WxDlgEditPlayWorldURL::BuildOrientationOptions (void)
{
	// Mobile Orientation Sizer
	MobileOrientationTitleSizer = CreateNewSizer(TitleSizer, wxHORIZONTAL);
	MobileOrientationSizer = CreateNewSizer(WidgetSizer, wxHORIZONTAL);

	//Title
	wxStaticText* MobileOrientationLabel = new wxStaticText(this, wxID_ANY, *LocalizeUnrealEd("BuildPlay_Orientation"));
	MobileOrientationTitleSizer->Add( MobileOrientationLabel, 0, wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_LEFT, 4 );

	//landscape radio button
	LandscapeModeRadioButton = new wxRadioButton();
	LandscapeModeRadioButton->Create( this, IDM_BuildPlay_Landscape, TEXT(""), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	MobileOrientationSizer->Add( LandscapeModeRadioButton, 0, wxEXPAND|wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_LEFT , 4 );

	//landscape image
	WxBitmap LandscapeBitmap;
	LandscapeBitmap.Load(TEXT("MobileLandscape.png"));
	LandscapeStaticImage = new WxPassThroughStaticBitmap();
	LandscapeStaticImage->Create(this, IDM_BuildPlay_Landscape, LandscapeBitmap);
	MobileOrientationSizer->Add( LandscapeStaticImage, 0, wxEXPAND|wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_LEFT , 4 );

	/** Landscape Label. */
	wxStaticText* LandscapeLabel = new wxStaticText(this, wxID_ANY, *LocalizeUnrealEd("BuildPlay_Landscape"));
	MobileOrientationSizer->Add( LandscapeLabel, 0, wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_LEFT, 4 );

	//portrait radio button
	PortraitModeRadioButton = new wxRadioButton();
	PortraitModeRadioButton->Create( this, IDM_BuildPlay_Portrait, TEXT(""), wxDefaultPosition, wxDefaultSize);
	MobileOrientationSizer->Add( PortraitModeRadioButton, 0, wxEXPAND|wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_LEFT , 4 );

	//portrait image
	WxBitmap PortraitBitmap;
	PortraitBitmap.Load(TEXT("MobilePortrait.png"));
	PortaitStaticImage = new WxPassThroughStaticBitmap();
	PortaitStaticImage->Create(this, IDM_BuildPlay_Portrait, PortraitBitmap);
	MobileOrientationSizer->Add( PortaitStaticImage, 0, wxEXPAND|wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_LEFT , 4 );

	/** Portrait Label. */
	wxStaticText* PortraitLabel = new wxStaticText(this, wxID_ANY, *LocalizeUnrealEd("BuildPlay_Portrait"));
	MobileOrientationSizer->Add( PortraitLabel, 0, wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_LEFT, 4 );

	UpdateOrientationUI();
}

/** Builds UI for mobile features */
void WxDlgEditPlayWorldURL::BuildMobileFeatureOptions (void)
{
	// Mobile Feature Sizer
	MobileFeaturesTitleSizer = CreateNewSizer(TitleSizer, wxHORIZONTAL);
	MobileFeaturesSizer = CreateNewSizer(WidgetSizer, wxHORIZONTAL);

	wxString* EmptyComboBoxStrings = NULL;
	FeaturesComboBox = new WxComboBox( this, IDM_BuildPlay_Mobile_Features, *LocalizeUnrealEd("BuildPlay_Features"), wxDefaultPosition, wxSize(200, -1), 0, EmptyComboBoxStrings, wxCB_READONLY );

	//add default
	FeaturesComboBox->Append( *LocalizeUnrealEd("BuildPlay_Default") );

	//add iphone 3gs/ipod 
	FeaturesComboBox->Append( *LocalizeUnrealEd("BuildPlay_iPhone3GS") );

	//add iPhone 4
	FeaturesComboBox->Append( *LocalizeUnrealEd("BuildPlay_iPhone4") );

	//add iPhone 4S
	FeaturesComboBox->Append( *LocalizeUnrealEd("BuildPlay_iPhone4S") );

	//add iPhone 45
	FeaturesComboBox->Append( *LocalizeUnrealEd("BuildPlay_iPhone5") );

	//add iPod 4
	FeaturesComboBox->Append( *LocalizeUnrealEd("BuildPlay_iPodTouch4") );

	//add iPod 5
	FeaturesComboBox->Append( *LocalizeUnrealEd("BuildPlay_iPodTouch5") );

	//add iPad
	FeaturesComboBox->Append( *LocalizeUnrealEd("BuildPlay_iPad") );

	// iPad 2
	FeaturesComboBox->Append( *LocalizeUnrealEd("BuildPlay_iPad2") );

	// iPad 3
	FeaturesComboBox->Append( *LocalizeUnrealEd("BuildPlay_iPad3") );

	// iPad 4
	FeaturesComboBox->Append( *LocalizeUnrealEd("BuildPlay_iPad4") );

	// iPad Mini
	FeaturesComboBox->Append( *LocalizeUnrealEd("BuildPlay_iPadMini") );

	// Flash
#if !UDK
	FeaturesComboBox->Append( TEXT("Flash") );
#endif

	// PS Vita
	if( PSVitaSupported() )
	{
		FeaturesComboBox->Append( *LocalizeUnrealEd("BuildPlay_PSVita") );
	}

	//Get default index based on resolution, bUseMobilePreview
	INT FeaturesIndex = GetCurrentFeaturesIndex();
	SetFeaturesIndex(FeaturesIndex, TRUE);

	wxStaticText* MobileFeaturesLabel = new wxStaticText(this, wxID_ANY, *LocalizeUnrealEd("BuildPlay_Features"));
	MobileFeaturesTitleSizer->Add( MobileFeaturesLabel, 0, wxALL|wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT, 4 );

	MobileFeaturesSizer->Add( FeaturesComboBox, 1, wxEXPAND|wxALL|wxALIGN_CENTER_VERTICAL, 4 );
}


/** Get the starting index based on the EditorPlayResolution settings */
INT WxDlgEditPlayWorldURL::GetCurrentResolutionIndex(const UBOOL bUseMobilePreview) const
{
	//get default resolution
	INT Width = GEditor->PlayInEditorWidth;
	INT Height = GEditor->PlayInEditorHeight;

	for (INT i = 0; i < BPD_RESOLUTION_MAX; ++i)
	{
		//skip custom as it is the default
		if (i == BPD_CUSTOM)
		{
			if (!bUseMobilePreview)
			{
				//if not mobile, just assume custom
				break;
			}
			continue;
		}

		INT TestWidth, TestHeight;
		GetDeviceResolution(i, OUT TestWidth, OUT TestHeight);
		if ((Width == TestWidth) && (Height == TestHeight))
		{
			return i;
		} 
	}
	return BPD_CUSTOM;
}

/** Gets the index of the current feature set */
INT WxDlgEditPlayWorldURL::GetCurrentFeaturesIndex (void) const
{
	return (INT)GEditor->BuildPlayDevice;
}

/** Shared function to change the feature set and update (optionally) the controls */
void WxDlgEditPlayWorldURL::SetFeaturesIndex(INT FeaturesIndex, const UBOOL bUpdateControl)
{
	if (bInited || bUpdateControl)
	{
		GEditor->BuildPlayDevice = (EBuildPlayDevice)FeaturesIndex;

		// Update the mobile emulation settings now that the target platform has changed
		SetMobileRenderingEmulation(GEmulateMobileRendering, GUseGammaCorrectionForMobileEmulation);

		if (bUpdateControl)
		{
			if (FeaturesIndex > BPD_DEFAULT)
			{
				--FeaturesIndex;
			}
			FeaturesComboBox->Select( FeaturesIndex );
		}
	}
}

/** Changes the edit box contents when changing resolutions */
void WxDlgEditPlayWorldURL::UpdateCustomResolution(const INT ResolutionIndex)
{
	INT NewWidth, NewHeight;
	GetDeviceResolution(ResolutionIndex, NewWidth, NewHeight);

	//update x text ctrl
	check(XSizeTextCtrl);
	XSizeTextCtrl->SetValue(*FString::Printf(TEXT("%d"), NewWidth));

	//update y text ctrl
	check(YSizeTextCtrl);
	YSizeTextCtrl->SetValue(*FString::Printf(TEXT("%d"), NewHeight));

	FString ResolutionFormat = TEXT(" (%d x %d)");
	ResolutionComboBox->SetString(BPD_CUSTOM, *(LocalizeUnrealEd("BuildPlay_Custom") + FString::Printf(*ResolutionFormat, NewWidth, NewHeight)) );
	//re-send the selection in case we just reconstructed the custom entry
	ResolutionComboBox->Select(ResolutionIndex);

	//show/hide custom resolution sizer recursively
	DialogSizer->Show(CustomResolutionTitleSizer, ResolutionIndex == BPD_CUSTOM, TRUE);
	DialogSizer->Show(CustomResolutionSizer, ResolutionIndex == BPD_CUSTOM, TRUE);
	Layout();
	Fit();
}

/** Update Orientation Radio button */
void WxDlgEditPlayWorldURL::UpdateOrientationUI ()
{
	LandscapeModeRadioButton->SetValue(!GEditor->bMobilePreviewPortrait);
	PortraitModeRadioButton->SetValue(GEditor->bMobilePreviewPortrait);
}

/** Callback for combo changed event */
void WxDlgEditPlayWorldURL::OnResolutionChanged( wxCommandEvent& In )
{
	if (bInited)
	{
		INT NewResolutionIndex = In.GetInt();
		GetDeviceResolution(NewResolutionIndex, GEditor->PlayInEditorWidth, GEditor->PlayInEditorHeight);
		UpdateCustomResolution(NewResolutionIndex);

		//Get default index based on resolution, bUseMobilePreview
		INT FeaturesIndex = In.GetInt();
		SetFeaturesIndex(FeaturesIndex, TRUE);
	}
}

/** Callback for features combo changed event */
void WxDlgEditPlayWorldURL::OnFeaturesChanged( wxCommandEvent& In )
{
	if (bInited)
	{
		INT FeatureIndex = In.GetInt();
		//"Custom" is removed from the feature list.  So, account for that off-by-one issue
		if (FeatureIndex > BPD_DEFAULT)
		{
			++FeatureIndex;
		}
		//do not update the combo, as we're already in a callback
		SetFeaturesIndex(FeatureIndex, FALSE);
	}
}

/** Called when the user presses the enter key on one of the textboxes. */
void WxDlgEditPlayWorldURL::CommitCustomResolution(wxCommandEvent& In)
{
	if (bInited)
	{
		check(XSizeTextCtrl);
		check(YSizeTextCtrl);

		FString RequestedXSize = (const TCHAR*)XSizeTextCtrl->GetValue();
		FString RequestedYSize = (const TCHAR*)YSizeTextCtrl->GetValue();

		UBOOL bValidNumbers = (RequestedXSize.IsNumeric() && RequestedYSize.IsNumeric());
		if (bValidNumbers)
		{
			//the 256 value is to prevent it from causing the default from changing (see FSystemSettings::SetResolution for where the default is over written)
			GEditor->PlayInEditorWidth = appAtoi(*RequestedXSize);
			GEditor->PlayInEditorWidth = Max(GEditor->PlayInEditorWidth, 256);

			GEditor->PlayInEditorHeight = appAtoi(*RequestedYSize);
			GEditor->PlayInEditorHeight = Max(GEditor->PlayInEditorHeight, 256);
		}
		PlayButton->Enable(bValidNumbers == TRUE);
		OkButton->Enable(bValidNumbers == TRUE);
	}
}

/** Event when an orientation radio button is pressed */
void WxDlgEditPlayWorldURL::OnChangeOrientation( wxCommandEvent& In)
{
	if (bInited)
	{
		GEditor->bMobilePreviewPortrait = In.GetId() == IDM_BuildPlay_Portrait;
	}
}

/** Event when orientation icon is clicked */
void WxDlgEditPlayWorldURL::OnLeftClick( wxMouseEvent& In )
{
	if (bInited && LandscapeStaticImage && PortaitStaticImage)
	{
		// Get the current mouse cursor position so the dialog can be placed near the mouse when shown
		POINT ClickPosition;
		if ( ::GetCursorPos( &ClickPosition ) )
		{
			wxRect LandscapeImageRect = LandscapeStaticImage->GetScreenRect();
			wxRect PortraitImageRect = PortaitStaticImage->GetScreenRect();
			if (LandscapeImageRect.Contains(ClickPosition.x, ClickPosition.y))
			{
				GEditor->bMobilePreviewPortrait = FALSE;
				UpdateOrientationUI();
			}
			else if (PortraitImageRect.Contains(ClickPosition.x, ClickPosition.y))
			{
				GEditor->bMobilePreviewPortrait = TRUE;
				UpdateOrientationUI();
			}
		}
	}
}


void WxDlgEditPlayWorldURL::OnButtonClicked( wxCommandEvent& In )
{
	// Get the ID of the button that was clicked
	INT ID = In.GetId();
	if( ID == Option_PlayWithURL || ID == Option_SaveURL )
	{
		// If the ID was play with url or save url, save the url that was edited
		URL = URLTextBox->GetValue();
	}

	// Close the dialog, returing the ID of the button that was clicked.
	EndModal(ID);
}

#define PlayWorldDialogMinSizerHeight 32
/** Makes custom horizontal sizer for this widget (maintains height for a row) */
wxBoxSizer* WxDlgEditPlayWorldURL::CreateNewSizer (wxSizer* ParentSizer, const INT Orientation)
{
	check(ParentSizer);

	wxBoxSizer* NewSizer = new wxBoxSizer( Orientation );
	ParentSizer->Add( NewSizer, 0, wxEXPAND|wxALL|wxALIGN_CENTER_VERTICAL, 2 );

	NewSizer->SetMinSize(1, PlayWorldDialogMinSizerHeight);

	return NewSizer;
}



UBOOL PromptUserForDirectory(FString& OutDirectory, const FName& Message, const FName& DefaultPath)
{
	UBOOL bResult = TRUE;

	wxString strDefPath;
	if (DefaultPath != NAME_None)
	{
		strDefPath = *DefaultPath.ToString();
	}

	wxDirDialog	DirDialog(GApp->EditorFrame, wxString(*Message.ToString()), strDefPath);
	if (DirDialog.ShowModal() == wxID_OK)
	{
		OutDirectory = FString(DirDialog.GetPath());
	}
	else
	{
		OutDirectory = TEXT("");
		bResult = FALSE;
	}

	return bResult;
}
