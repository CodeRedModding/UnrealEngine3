/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DIALOGS_H__
#define __DIALOGS_H__


/*-----------------------------------------------------------------------------
	WxDualFileDialog
-----------------------------------------------------------------------------*/
class WxDualFileDialog : public wxDialog
{
public:

	WxDualFileDialog(const FString& Title, const FString& InFile1Label, const FString& InFile2Label, const FString& InFileOptionsMask, UBOOL bInMustExist, LONG WindowStyle);
	virtual ~WxDualFileDialog() {}

	/** 
	 *   Returns all the filenames (empty or otherwise) specified in the text entry boxes
	 * @param Filenames - array of filenames returned
	 */
	void GetFilenames(TArray<FFilename>& Filenames)
	{
		Filenames.Empty(FileSelections.Num());
		for (INT i=0; i<FileSelections.Num(); i++)
		{
			Filenames.AddItem(FileSelections(i).Filename);
		}
	}

	/**
	 *   @return TRUE if all filenames selected are valid, FALSE otherwise
	 */
	UBOOL VerifyFilenames();

protected:

	struct FileSelectionData
	{
		/** ID of the ellipse widget */
		INT EllipseID;
		/** ID of the file text ctrl */
		INT TextCtrlID;
		/** Label above the filename option */
		FString FileLabel;
		/** Name of the file in the text edit box */
		FFilename Filename;
		/** File mask when opening the file dialog */
		FString FileOptionsMask;
		/** File text control */
		wxTextCtrl* FileTextCtrl;
		/** Are we choosing files that already exist */
		UBOOL bMustExist;
	};

	/** Array of file textctrl/ellipse pair and their properties */
	TArray<FileSelectionData> FileSelections;

	/** 
	 * Constructs the buttons, panels, etc. necessary for the dialog to work
	 */
	void ConstructWidgets(const FString& FileLabel1, const FString& FileLabel2, const FString& FileOptionsMask, UBOOL bMustExist);

	/**
	* Callback when the OK button is clicked, shuts down the options panel
	* @param In - Properties of the event triggered
	*/
	void OnOK( wxCommandEvent& In );

	/**
	* Callback when the window is closed, shuts down the options panel
	* @param In - Properties of the event triggered
	*/
	void OnClose(wxCloseEvent& In);

	/**
	 * Callback when user pushes any of the file open buttons
	 * @param In - Properties of the event triggered
	 */
	void OnFileOpenClicked(wxCommandEvent& In);

	/**
	 * Callback when user types in any of the filename text controls
	 * @param In - Properties of the event triggered
	 */
	void OnFilenameTextChanged(wxCommandEvent& In);

	/** 
	 *   Helper for opening a file dialog with the appropriate information
	 * @param LocalizeTitleName - text markup for the string to appear in the dialog
	 * @param FileOptionsMask - string describing the filename wildcards/descriptions
	 * @param bMustExist - whether or not the file must exist during selection
	 * @param FileSelection - the file selected by the user
	 * @return TRUE if user selected a file, FALSE otherwise
	 */
	UBOOL OpenFileDialog(const FString& LocalizeTitleName, const FString& FileOptionsMask,  UBOOL bMustExist, FFilename& FileSelection);

	/** 
	 *   Helper for creating each file text ctrl / file open ellipse pair
	 * @param ThisDialog - the dialog object
	 * @param MainDialogSizer - the topmost sizer containing the widgets to be created
	 * @param FileSelection - a description of the widget
	 */
	void CreateFileDialog(wxDialog* ThisDialog, wxBoxSizer* MainDialogSizer, FileSelectionData& FileSelection);

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxChoiceDialogBase
-----------------------------------------------------------------------------*/

/**
 * Base class for choice-based dialogs. Should not be used on its own, but instead
 * inherited from to provide choice-based dialogs with potentially dramatically
 * different main panels (therefore has pure virtual destructor to prevent instantiation). 
 * Inheriting classes should implement their primary "window" as a new wxPanel and then 
 * send it to WxChoiceDialogBase::ConstructWidgets in order to properly build the dialog. 
 * ConstructWidgets will place the panel above the appropriate buttons with a sizer. 
 * See WxChoiceDialog as an example.
 */
class WxChoiceDialogBase : public wxDialog
{
public:

	enum EDialogChoiceType
	{
		/** A regular enabled choice. */
		DCT_Regular,
		/** A choice that will be focused by default. */
		DCT_DefaultAffirmative,
		/** The default choice when user hits Esc or closes the window (the safe choice). */
		DCT_DefaultCancel,
		/** This option is inactive; no button will be generated. */
		DCT_Inactive,
	};

	/** Basic structure representing a user choice within the dialog. */
	struct Choice
	{
		/** Construct a disabled choice; one that is not seen by the user. */
		Choice():
			ReturnCode(0),
			TypeOfChoice(DCT_Inactive)
	{			
	}

	/**
	 * Construct a choice.
	 *
	 * @param InReturnCode	Value returned by dialog if this choice is chosen.
	 * @param InButtonText	Text to appear on the button corresponding to this choice.
	 */
	Choice( INT InReturnCode, const FString & InButtonText, const EDialogChoiceType InTypeOfChoice = DCT_Regular ):
		ReturnCode( InReturnCode ),
		ButtonText( InButtonText ),
		TypeOfChoice( InTypeOfChoice )
	{
	}

	/** 
	 * Should we present this choice to the user
	 *
	 * @return	TRUE if the choice is active and should be presented to the user, FALSE otherwise
	 */
	UBOOL IsChoiceActive() const { return TypeOfChoice != DCT_Inactive; }

	/** The code to return if this option is chosen */
	INT ReturnCode;

	/** The text to appear in the button corresponding to this choice */
	FString ButtonText;

	/** Is this choice the default affirmative action? (i.e. user presses Enter) or the default cancel choice? (i.e. user hits Esc or closes dialog) */
	EDialogChoiceType TypeOfChoice;
	};

	/**
	 * Construct a choice-dialog base
	 *
	 * @param Title			The title of the dialog
	 * @param WindowStyle	Flags representing the style of the window
	 * @param InChoiceOne	Text and return code associated with the first button.
	 * @param InChoiceTwo	Optional text and return code associated with the second button.
	 * @param InChoiceThree	Optional text and return code associated with the third button.
	 * @param InChoiceFour	Optional text and return code associated with the fourth button.
	 * @param InChoiceFive	Optional text and return code associated with the fifth button.	 
	 */
	WxChoiceDialogBase(
		FString Title
		, LONG WindowStyle
		, const WxChoiceDialogBase::Choice& InChoiceOne
		, const WxChoiceDialogBase::Choice& InChoiceTwo = Choice()
		, const WxChoiceDialogBase::Choice& InChoiceThree = Choice()
		, const WxChoiceDialogBase::Choice& InChoiceFour = Choice()
		, const WxChoiceDialogBase::Choice& InChoiceFive = Choice());

	/** Pure virtual destructor so others can inherit from this dialog box, but not instantiate one */
	virtual ~WxChoiceDialogBase()=0;

	/** Returns the choice made by the user (will be DefaultChoice until the dialog is closed) */
	const WxChoiceDialogBase::Choice& GetChoice();

protected:

	/** Gets a button ID for a button associated with a specific choice */
	static INT GetButtonIdFor( INT InChoiceID );

	/** 
	 * Constructs the buttons, panels, etc. necessary for the dialog to work
	 *
	 * @param InPanel - Panel to use for the top part of the dialog, this allows child classes to specify unique/special panels in the dialog
	 */
	void ConstructWidgets(wxPanel* InPanel);

	/** Window closed event handler; required for the X to work. */
	void OnClose( wxCloseEvent& Event );
	
	/** Handle user clicking on button one */
	void ButtonOneChosen(wxCommandEvent& In);

	/** Handle user clicking on button two*/
	void ButtonTwoChosen(wxCommandEvent& In);

	/** Handle user clicking on button three */
	void ButtonThreeChosen(wxCommandEvent& In);

	/** Handle user clicking on button three */
	void ButtonFourChosen(wxCommandEvent& In);

	/** Handle user clicking on button three */
	void ButtonFiveChosen(wxCommandEvent& In);

	/** Store the choice made by the user */
	INT UserChoice;

	TArray< Choice > Choices;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxChoiceDialog
-----------------------------------------------------------------------------*/

/**
 * Dialog that presents the user with choices using static text on a white,
 * non-sizable panel
 */
class WxChoiceDialog : public WxChoiceDialogBase
{
public:
	/**
	 * Static function that creates a WxChoiceDialog in one of several standard configurations
	 * and immediately prompts the user with the provided message
	 * (This mimics appMsgf's functionality; appMsgf forwards to this in editor)
	 *
	 * @param InMessage	Message to display on the dialog
	 * @param InType		Type of dialog to display (YesNo, OKCancel, etc.)
	 */
	static INT WxAppMsgF( const FString& InMessage, EAppMsgType InType );

	/**
	 * Construct a Choice Dialog
	 *
	 * @param Prompt		The message that appears to the user
	 * @param Title			The title of the dialog
	 * @param InChoiceOne	Text and return code associated with the first button.
	 * @param InChoiceTwo	Optional text and return code associated with the second button.
	 * @param InChoiceThree Optional text and return code associated with the third button.
	 * @param InChoiceFour	Optional text and return code associated with the fourth button.
	 * @param InChoiceFive	Optional text and return code associated with the fifth button.	 
	 */
	WxChoiceDialog(
		  FString Prompt
		, FString Title
		, const WxChoiceDialogBase::Choice& InChoiceOne
		, const WxChoiceDialogBase::Choice& InChoiceTwo = Choice()
		, const WxChoiceDialogBase::Choice& InChoiceThree = Choice()
		, const WxChoiceDialogBase::Choice& InChoiceFour = Choice()
		, const WxChoiceDialogBase::Choice& InChoiceFive = Choice());

	/** Virtual destructor so others can inherit from this dialog box */
	virtual ~WxChoiceDialog();
};

/*-----------------------------------------------------------------------------
	WxLongChoiceDialog
-----------------------------------------------------------------------------*/

/**
 * Dialog that presents the user with choices using a scrollable text control on
 * a re-sizable panel
 */
class WxLongChoiceDialog : public WxChoiceDialogBase
{
public:

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
	WxLongChoiceDialog(
		  FString Prompt
		, FString Title
		, const WxChoiceDialogBase::Choice& InChoiceOne
		, const WxChoiceDialogBase::Choice& InChoiceTwo = Choice()
		, const WxChoiceDialogBase::Choice& InChoiceThree = Choice()
		, const WxChoiceDialogBase::Choice& InChoiceFour = Choice()
		, const WxChoiceDialogBase::Choice& InChoiceFive = Choice());

	/** Virtual destructor so others can inherit from this dialog box */
	virtual ~WxLongChoiceDialog();
};

/**
 * Simple dialog that prompts the user with a message and an OK button. Designed to be
 * exclusively used from the heap and exclusively modeless, as the class destroys itself
 * as soon as the user presses the OK button or closes the dialog.
 */
class WxModelessPrompt : public wxDialog
{
public:
	/**
	 * Construct a WxModelessPrompt object
	 *
	 * @param	Prompt	Message to prompt the user with
	 * @param	Title	Title of the dialog
	 */
	WxModelessPrompt( const FString& Prompt, const FString& Title );

	/**
	 * Destroy a WxModelessPrompt object
	 */
	virtual ~WxModelessPrompt() {}

private:

	/**
	 * Overridden from the base wxDialog version; intentionally asserts and is made private
	 * to prevent its use
	 */
	virtual int ShowModal();
	
	/**
	 * Called automatically by wxWidgets when the user presses the OK button
	 *
	 * @param	Event	Event automatically generated by wxWidgets when the user presses the OK button
	 */
	void OnOk( wxCommandEvent& Event );

	/**
	 * Called automatically by wxWidgets when the user closes the window (such as via the X)
	 *
	 * @param	Event	Event automatically generated by wxWidgets when the user closes the window
	 */
	void OnClose( wxCloseEvent& Event );

	DECLARE_EVENT_TABLE()
};


/*-----------------------------------------------------------------------------
	WxSuppressableWarningDialog
-----------------------------------------------------------------------------*/
/**
 * A Dialog that displays a warning message to the user and provides the option to not display it in the future
 */
class WxSuppressableWarningDialog : public wxDialog
{
public:
	/**
	* Constructs a warning dialog that can be suppressed.
	*
	* @param Prompt			The message that appears to the user
	* @param Title				The title of the dialog
	* @param InIniSettingName	The name of the entry in the EditorUserSettings INI were the state of the "Disable this warning" check box is stored
	* @param IncludeYesNoButtons If TRUE, Yes/No buttons will be used for the warning rather than just an OK button
	*/
	WxSuppressableWarningDialog ( const FString& InPrompt, const FString& InTitle, const FString& InIniSettingName, UBOOL IncludeYesNoButtons=FALSE );
	virtual ~WxSuppressableWarningDialog();

	/**
	 * Displays the dialog if the user did not disable being prompted for this warning message.
	 * If the warning message is displayed the state of the disable check box will be saved to the user setting ini via the variable name passed into the constructor
	 */
	virtual INT ShowModal();

private:
	wxCheckBox* DisableCheckBox;
	// The variable name in the user settings ini where we should look for and store  whether or not this dialog should be shown
	FString IniSettingName;
	// Store the prompt so we can log the message even if the dialog should not be displayed.
	FString Prompt;

	wxButton* OkButton;
	wxButton* YesButton;
	wxButton* NoButton;

	void OnYes(wxCommandEvent &Event);	// nb. we also use OnYes for the OK button
	void OnNo(wxCommandEvent &Event);
	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgBindHotkeys
-----------------------------------------------------------------------------*/

/**
 * This class is used to bind hotkeys for the editor.
 */
class WxDlgBindHotkeys : public wxFrame
{
public:
	WxDlgBindHotkeys(wxWindow* InParent);
	~WxDlgBindHotkeys();

	/** Saves settings for this dialog to the INI. */
	void SaveSettings();

	/** Loads settings for this dialog from the INI. */
	void LoadSettings();

	/** Starts the binding process for the current command. */
	void StartBinding(INT CommandIdx);

	/** Finishes the binding for the current command using the provided event. */
	void FinishBinding(wxKeyEvent &Event);

	/** Stops the binding process for the current command. */
	void StopBinding();

	/** @return Whether or not we are currently binding a command. */
	UBOOL IsBinding() 
	{
		return CurrentlyBindingIdx != -1;
	}

	/**
	 * @return Generates a descriptive binding string based on the key combinations provided.
	 */
	FString GenerateBindingText(UBOOL bAltDown, UBOOL bCtrlDown, UBOOL bShiftDown, FName Key);

private:
	
	/** Builds the category tree. */
	void BuildCategories();

	/** Builds the command list using the currently selected category. */
	void BuildCommands();

	/** Refreshes the binding text for the currently visible binding widgets. */
	void RefreshBindings();

	/** Updates the scrollbar for the command view. */
	void UpdateScrollbar();

	/** Window closed event handler. */
	void OnClose(wxCloseEvent& Event);

	/** Category selected handler. */
	void OnCategorySelected(wxTreeEvent& Event);

	/** Handler to let the user load a config from a file. */
	void OnLoadConfig(wxCommandEvent& Event);
	
	/** Handler to let the user save the current config to a file. */
	void OnSaveConfig(wxCommandEvent& Event);
	
	/** Handler to reset bindings to default. */
	void OnResetToDefaults(wxCommandEvent& Event);

	/** Bind key button pressed handler. */
	void OnBindKey(wxCommandEvent& Event);

	/** OK Button pressed handler. */
	void OnOK(wxCommandEvent &Event);

	/** Handler for key binding events. */
	void OnKeyDown(wxKeyEvent& Event);

	/** Which command we are currently binding a key to. */
	INT CurrentlyBindingIdx;

	/** Tree control to display all of the available command categories to bind to. */
	WxTreeCtrl* CommandCategories;

	/** Splitter to separate tree and command views. */
	wxSplitterWindow* MainSplitter;

	/** Panel to store commands. */
	wxScrolledWindow* CommandPanel;

	/** Currently binding label text. */
	wxStaticText* BindLabel;

	/** Size of 1 command item. */
	INT ItemSize;

	/** Number of currently visible commands. */
	INT NumVisibleItems;

	/** The list of currently visible commands. */
	TArray<FEditorCommand> VisibleCommands;

	/** Struct of controls in a command panel. */
	struct FCommandPanel
	{
		wxTextCtrl* BindingWidget;
		wxButton* BindingButton;
		wxPanel* BindingPanel;
		FName CommandName;
	};

	/** Array of currently visible binding controls. */
	TArray<FCommandPanel> VisibleBindingControls;

	/** A mapping of category names to their array of commands. */
	TMap< FName, TArray<FEditorCommand> >	CommandMap;

	/** Mapping of category names to tree id's. */
	TMap<FName, wxTreeItemId> ParentMap;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgPackageGroupName.
-----------------------------------------------------------------------------*/

class WxDlgPackageGroupName : public wxDialog
{
public:
	WxDlgPackageGroupName();
	virtual ~WxDlgPackageGroupName();

	const FString& GetPackage() const
	{
		return Package;
	}
	const FString& GetGroup() const
	{
		return Group;
	}
	const FString& GetObjectName() const
	{
		return Name;
	}

	enum EControlOptions
	{
		PGN_Package	= 0x01,
		PGN_Group	= 0x02,
		PGN_Name	= 0x04,
		PGN_Default	= PGN_Package | PGN_Group | PGN_Name,
		PGN_NoName	= PGN_Package | PGN_Group
	};

	int ShowModal(const FString& InPackage, const FString& InGroup, const FString& InName, EControlOptions InOptions = PGN_Default );
private:
	using wxDialog::ShowModal;		// Hide parent implementation
public:

	virtual bool Validate();
	UBOOL ProcessNewAssetDlg(UPackage** NewObjPackage, FString* NewObjName, UBOOL bAllowCreateOverExistingOfSameType, UClass* NewObjClass);

protected:
	FString Package, Group, Name;
	wxBoxSizer* PGNSizer;
	wxPanel* PGNPanel;
	WxPkgGrpNameCtrl* PGNCtrl;


private:
	void OnOK( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgNewArchetype.
-----------------------------------------------------------------------------*/

class WxDlgNewArchetype : public WxDlgPackageGroupName
{
public:
	/////////////////////////
	// wxWindow interface.

	virtual bool Validate();
};

/*-----------------------------------------------------------------------------
	WxDlgAddSpecial.
-----------------------------------------------------------------------------*/

class WxDlgAddSpecial : public wxDialog
{
public:
	WxDlgAddSpecial();
	virtual ~WxDlgAddSpecial();

private:
	wxCheckBox *PortalCheck, *InvisibleCheck, *TwoSidedCheck;
	wxRadioButton *SolidRadio, *SemiSolidRadio, *NonSolidRadio;

	void OnOK( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgGenericStringEntry
-----------------------------------------------------------------------------*/

class WxDlgGenericStringEntry : public wxDialog
{
public:
	/**
	 * @param	LocalizeWindow	When TRUE, the window will be localized; Un-localized, otherwise. 
	 */
	WxDlgGenericStringEntry( UBOOL LocalizeWindow = TRUE );
	virtual ~WxDlgGenericStringEntry();

	const FString& GetEnteredString() const
	{
		return EnteredString;
	}
	wxTextCtrl& GetStringEntry() const
	{
		return *StringEntry;
	}
	wxStaticText& GetStringCaption() const
	{
		return *StringCaption;
	}

	int ShowModal( const TCHAR* DialogTitle, const TCHAR* Caption, const TCHAR* DefaultString );
private:
	using wxDialog::ShowModal;		// Hide parent implementation
public:

	void Init();

private:
	FString			EnteredString;
	wxTextCtrl		*StringEntry;
	wxStaticText	*StringCaption;

	/** When TRUE, the window will be localized; Un-localized, otherwise.  */
	UBOOL			bLocalizeWindow;

	void OnOK( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgGenericStringWrappedEntry
-----------------------------------------------------------------------------*/

class WxDlgGenericStringWrappedEntry : public wxDialog
{
public:
	WxDlgGenericStringWrappedEntry();
	virtual ~WxDlgGenericStringWrappedEntry();

	const FString& GetEnteredString() const
	{
		return EnteredString;
	}
	const wxTextCtrl& GetStringEntry() const
	{
		return *StringEntry;
	}

	int ShowModal( const TCHAR* DialogTitle, const TCHAR* Caption, const TCHAR* DefaultString );
private:
	using wxDialog::ShowModal;		// Hide parent implementation
public:

	void Init();

private:
	FString			EnteredString;
	wxTextCtrl		*StringEntry;
	wxStaticText	*StringCaption;

	void FinalizeDialog();
	void OnEnter( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgGenericSlider
-----------------------------------------------------------------------------*/

class WxDlgGenericSlider : public wxDialog
{
public:
	explicit WxDlgGenericSlider( wxWindow* InParent );
	virtual ~WxDlgGenericSlider();

	/**
	 * Shows the dialog box and waits for the user to respond.
	 * @param DialogTitle - Title of the dialog box.
	 * @param Caption - Caption next to the slider.
	 * @param MinValue - Minimum allowed slider value.
	 * @param MaxValue - Maximum allowed slider value.
	 * @param DefaultValue - Default slider value.
	 * @returns wxID_OK or wxID_CANCEL depending on which button the user dismissed the dialog with.
	 */
	int ShowModal( const TCHAR* DialogTitle, const TCHAR* Caption, INT MinValue, INT MaxValue, INT DefaultValue );

	/** Returns the value of the slider. */
	INT GetValue() const
	{
		return Slider->GetValue();
	}

private:
	wxBoxSizer* TopSizer;
	wxStaticBoxSizer* Sizer;
	wxSlider* Slider;

	using wxDialog::ShowModal; // Hide parent implementation

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgMorphLODImport
-----------------------------------------------------------------------------*/

class WxDlgMorphLODImport : public wxDialog
{
public:
	WxDlgMorphLODImport( wxWindow* InParent, const TArray<UObject*>& InMorphObjects, INT MaxLODIdx, const TCHAR* SrcFileName );
	virtual ~WxDlgMorphLODImport();

	const INT GetSelectedMorph() const
	{
		return MorphObjectsList->GetSelection();
	}

	const INT GetLODLevel() const
	{
		return LODLevelList->GetSelection();
	}

private:
	wxComboBox*	MorphObjectsList;
	wxComboBox* LODLevelList;

	DECLARE_EVENT_TABLE()
};

#if WITH_FBX

/*-----------------------------------------------------------------------------
	WxDlgMorphLODFbxImport
-----------------------------------------------------------------------------*/
class WxDlgMorphLODFbxImport : public wxDialog
{
public:
    WxDlgMorphLODFbxImport( wxWindow* InParent, const char* InMorphName, const TArray<UObject*>& InMorphObjects, INT LODIdx );
    virtual ~WxDlgMorphLODFbxImport() { }

    INT GetSelectedMorph() const
    {
        return MorphObjectsList->GetSelection();
    }

private:
    wxComboBox* MorphObjectsList;

    DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgStaticMeshLODFbxImport
-----------------------------------------------------------------------------*/
struct FbxMeshInfo
{
	char* Name;
	INT FaceNum;
	INT VertexNum;
	UBOOL bTriangulated;
	INT MaterialNum;
	UBOOL bIsSkelMesh;
	char* SkeletonRoot;
	INT SkeletonElemNum;
	char* LODGroup;
	INT LODLevel;
	INT MorphNum;
};

struct FbxSceneInfo
{
	// data for static mesh
	INT NonSkinnedMeshNum;
	
	//data for skeletal mesh
	INT SkinnedMeshNum;

	// common data
	INT TotalGeometryNum;
	INT TotalMaterialNum;
	INT TotalTextureNum;
	
	TArray<FbxMeshInfo> MeshInfo;
	
	// only one take supported currently
	char* TakeName;
	DOUBLE FrameRate;
	DOUBLE TotalTime;

};

/*-----------------------------------------------------------------------------
WxDlgFbxSceneInfo
-----------------------------------------------------------------------------*/
class WxDlgFbxSceneInfo : public wxDialog
{
public:
	WxDlgFbxSceneInfo( wxWindow* InParent, struct FbxSceneInfo& SceneInfo );
	virtual ~WxDlgFbxSceneInfo() { }

private:

	DECLARE_EVENT_TABLE()
};
#endif // WITH_FBX

/*-----------------------------------------------------------------------------
	WxDlgSurfaceProperties
-----------------------------------------------------------------------------*/

class WxDlgSurfaceProperties : public wxDialog
{
public:
	WxDlgSurfaceProperties();
	virtual ~WxDlgSurfaceProperties();

	void MarkDirty(void) { bDirty = TRUE; }

private:
	//when idle, refresh pages for 
	void RefreshPages();

	/**
	 * Event handler for when a the window is idle.
	 *
	 * @param	In	Information about the event.
	 */
	void OnIdle(wxIdleEvent &In);

	class WxSurfacePropertiesPanel* PropertiesPanel;
	UBOOL bDirty;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgColor.
-----------------------------------------------------------------------------*/

class WxDlgColor : public wxColourDialog
{
public:
	WxDlgColor();
	virtual ~WxDlgColor();

	void LoadColorData( wxColourData* InData );
	void SaveColorData( wxColourData* InData );

	bool Create( wxWindow* InParent, wxColourData* InData );

	int ShowModal();

  	void OnOK( wxCommandEvent& event );

	DECLARE_EVENT_TABLE()
};

//--------------------
//Color Pickers
//--------------------

//forward declares
class WxPropertyWindow;
class FPropertyNode;

//-------------------------------------
//Color Picker Constants
//-------------------------------------
namespace ColorPickerConstants
{
	enum ColorPickerResults
	{
		ColorAccepted,
		ColorRejected
	};

	const double MaxAllowedUpdateEventTime = .05;
}

struct FColorChannelStruct
{
	FColorChannelStruct()
	{
		Red = Green = Blue = Alpha = NULL;
	}

	FLOAT* Red;
	FLOAT* Green;
	FLOAT* Blue;
	FLOAT* Alpha;
};

struct FPickColorStruct
{
	FPickColorStruct(void)
	{
		PropertyWindow = NULL;
		PropertyNode = NULL;
		bSendEventsOnlyOnMouseUp = FALSE;
		bModal = FALSE;
		bUseSrgb = TRUE;
	}

	/**PropertyWindows to lock during a change*/
	WxPropertyWindow* PropertyWindow;
	/**Property Node that represent these colors*/
	FPropertyNode* PropertyNode;
	/**Windows to be refreshed when a color changes*/
	TArray <wxWindow*> RefreshWindows;
	/**FColors*/
	TArray<FColor*> DWORDColorArray;
	/**FLinearColors*/
	TArray<FLinearColor*> FLOATColorArray;
	/**Partial Colors that are treated like FLinearColors*/
	TArray<FColorChannelStruct> PartialFLOATColorArray;

	/**The object to call PreEditChange and PostEditChange on during color modification*/
	TArray<UObject*> ParentObjects;

	/**TRUE if ONLY mouse up events should cause events to get sent*/
	UBOOL bSendEventsOnlyOnMouseUp;

	/**Whether or not to launch the window as modal or not*/
	UBOOL bModal;

	/**Wether or not to use sRGB for the brushes used in the preview*/
	UBOOL bUseSrgb;

};

/**
 * General function for choosing a color.  Implementation can be wx or WPF
 * @param ColorStruct - Contains pointers to data to update, windows to refresh and parameters for how to display the dialog
 */
ColorPickerConstants::ColorPickerResults PickColor(const FPickColorStruct& ColorStruct);
/**
 * If the color picker is bound to this window in some way, unbind immediately or the color picker will be in a bad state
 * @param InWindowToUnbindFrom - Window that is being shut down that could leave the color picker in a bad state
 */
void UnBindColorPickers(wxWindow* InWindowToUnbindFrom);
/**
 * If the color picker is bound to a particular object that is being destroyed, unbind immediately or the color picker will be in a bad state
 * @param InObject - UObject that is being deleted that could leave the color picker in a bad state
 */
void UnBindColorPickers(UObject* InObject);

/*-----------------------------------------------------------------------------
	WxDlgLoadErrors
-----------------------------------------------------------------------------*/

/**
 * Dialog that displays various file and object-related loading errors
 */
class WxDlgLoadErrors : public wxDialog
{
public:
	/**
	 * Construct a WxDlgLoadErrors dialog
	 *
	 * @param	InParent	Parent window of this dialog
	 */
	WxDlgLoadErrors( wxWindow* InParent );

	/** Two text controls to use to display the various errors; one for packages, one for objects */
	wxTextCtrl *PackageErrors, *ObjectErrors;

	/**
	 * Update the dialog with all of the current load errors and adjust its size based upon the length of the
	 * errors.
	 */
	void Update();
};

/*-----------------------------------------------------------------------------
	WxDlgEditPlayWorldURL
-----------------------------------------------------------------------------*/

class WxDlgEditPlayWorldURL : public wxDialog
{
public:
	/** Dialog button ID's */
	enum EEditPlayWorldOption
	{
		// Cancel the url editing dialog without saving the url.
		Option_Cancel, 
		// Save the url. 
		Option_SaveURL,
		// Save and start a play session with the url.
		Option_PlayWithURL,
	};

	WxDlgEditPlayWorldURL( const FString& InStartURL, const UBOOL bUseMobilePreview, const UBOOL bAdjustableResolution );

	/** Returns the user modified URL */
	const FString& GetURL() const { return URL; }

private:
	/** URL editing text box */
	wxTextCtrl* URLTextBox;
	/** Storage for the edited url */
	FString URL;

	/** Resolution Combo box */
	WxComboBox* ResolutionComboBox;
	/** Features Combo box */
	WxComboBox* FeaturesComboBox;

	// Setup the main size for this dialog
	wxBoxSizer* DialogSizer;

	/** Left side of the dialog */
	wxBoxSizer* TitleSizer;
	/** Right side of the dialog */
	wxBoxSizer* WidgetSizer;

	//label for resolution sizer
	wxBoxSizer* CommandlineTitleSizer;
	/** Sizer for resolution size editboxes */
	wxBoxSizer* CommandlineSizer;

	//label for resolution sizer
	//label for resolution sizer
	wxBoxSizer* ResolutionTitleSizer;
	/** Sizer for resolution size editboxes */
	wxBoxSizer* ResolutionSizer;

	//label for resolution sizer
	wxBoxSizer* CustomResolutionTitleSizer;
	/** Sizer for custom resolution size editboxes */
	wxBoxSizer* CustomResolutionSizer;

	//label for mobile orientation sizer
	wxBoxSizer* MobileOrientationTitleSizer;
	/** Sizer for mobile orientation options */
	wxBoxSizer* MobileOrientationSizer;

	//label for mobile orientation sizer
	wxBoxSizer* MobileFeaturesTitleSizer;
	/** Sizer for mobile features options */
	wxBoxSizer* MobileFeaturesSizer;

	/** X TextField. */
	WxTextCtrl* XSizeTextCtrl;
	/** Y TextField. */
	WxTextCtrl* YSizeTextCtrl;

	//landscape UI
	wxRadioButton* LandscapeModeRadioButton;
	wxStaticBitmap* LandscapeStaticImage;

	//portrait UI
	wxRadioButton* PortraitModeRadioButton;
	wxStaticBitmap* PortaitStaticImage;

	//Dialog buttons
	wxButton* PlayButton;
	wxButton* OkButton;
	wxButton* CancelButton;

	/** Whether we're accepting events yet or not */
	UBOOL bInited;

	/** Build URL (commandline) options */
	void BuildUrlOptions (const FString& InStartURL);

	/** Build resolution combo box */
	void BuildResolutionOptions (const UBOOL bUseMobilePreview);

	/** Builds UI for custom resolution */
	void BuildCustomResolutionOptions (void);

	/** Builds UI for mobile orientation */
	void BuildOrientationOptions (void);

	/** Builds UI for mobile features */
	void BuildMobileFeatureOptions (void);

	/** Get the starting index based on the EditorPlayResolution settings */
	INT GetCurrentResolutionIndex(const UBOOL bUseMobilePreview) const;

	/** Gets the index of the current feature set */
	INT GetCurrentFeaturesIndex (void) const;

	/** Shared function to change the feature set and update (optionally) the controls */
	void SetFeaturesIndex(INT FeaturesIndex, const UBOOL bUpdateControl);

	/** Changes the edit box contents when changing resolutions */
	void UpdateCustomResolution(const INT ResolutionIndex);

	/** Update Orientation Radio button */
	void UpdateOrientationUI ();

	/** Callback for resolution combo changed event */
	void OnResolutionChanged( wxCommandEvent& In );

	/** Callback for features combo changed event */
	void OnFeaturesChanged( wxCommandEvent& In );

	/** Called when the user presses the enter key on one of the textboxes. */
	void CommitCustomResolution(wxCommandEvent& In);	

	/** Event when an orientation radio button is pressed */
	void OnChangeOrientation( wxCommandEvent& In);

	/** Event when orientation icon is clicked */
	void OnLeftClick( wxMouseEvent& In );

	/** Called when a button is clicked on the dialog */
	void OnButtonClicked( wxCommandEvent &In );

	/** Makes custom horizontal sizer for this widget (maintains height for a row) */
	wxBoxSizer* CreateNewSizer (wxSizer* ParentSizer, const INT Orientation);

	DECLARE_EVENT_TABLE()

};
/**
* Helper method for popping up a directory dialog for the user.  OutDirectory will be 
* set to the empty string if the user did not select the OK button.
*
* @param	OutDirectory	[out] The resulting path.
* @param	Message			A message to display in the directory dialog.
* @param	DefaultPath		An optional default path.
* @return					TRUE if the user selected the OK button, FALSE otherwise.
*/
UBOOL PromptUserForDirectory(FString& OutDirectory, const FName& Message, const FName& DefaultPath = NAME_None);

#endif // __DIALOGS_H__
