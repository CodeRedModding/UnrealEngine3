/*=============================================================================
	DlgMapCheck.h: UnrealEd dialog for displaying map errors.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DLGMAPCHECK_H__
#define __DLGMAPCHECK_H__

#include "TrackableWindow.h"


/** Struct that holds information about a entry in the ErrorWarningList. */
struct FCheckErrorWarningInfo
{
	/** The actor and/or object that is related to this error/warning.  Can be NULL if not relevant. */
	UObject* Object;

	/** The level that this object belonged to upon add, needed since unloading of packages could invalidate Object */
	ULevel* Level;

	/** What type of error/warning this is */
	MapCheckType Type;

	/** What group this message is in - if any */
	MapCheckGroup Group;

	/** The name of this Actor */
	FString Name;

	/** The level this Actor was found in */
	FString LevelName;

	/** The message we want to display to the LD. */
	FString Message;

	/** The UDN page where help can be found for this error/warning. */
	FString UDNPage;

	/** UDN Page that describes the warning in detail. */
	FString	UDNHelpString;

	/** The time taken to execute (for lighting builds). */
	DOUBLE ExecutionTime;

	UBOOL operator==(const FCheckErrorWarningInfo& Other) const
	{
		return (
			(Object == Other.Object) &&
			(Type == Other.Type) &&
			(Message == Other.Message) &&
			(UDNPage == Other.UDNPage) &&
			(ExecutionTime == Other.ExecutionTime)
			);
	}
};

/**
 * Dialog that displays map errors and allows the user to focus on and delete
 * any actors associated with those errors.
 */
class WxDlgMapCheck : public WxTrackableDialog, public FSerializableObject
{
public:
	WxDlgMapCheck(wxWindow* InParent);
	virtual ~WxDlgMapCheck();

	/**
	 *	Initialize the dialog box.
	 */
	virtual void Initialize();

	/**
	 * This function is called when the window has been selected from within the ctrl + tab dialog.
	 */
	virtual void OnSelected();

	/**
	 * Shows the dialog only if there are messages to display.
	 */
	virtual void ShowConditionally();

	/**
	 * Clears out the list of messages appearing in the window.
	 */
	virtual void ClearMessageList();

	/**
	 * Freezes the message list.
	 */
	void FreezeMessageList();

	/**
	 * Thaws the message list.
	 */
	void ThawMessageList();

	/**
	 * Returns TRUE if the dialog has any map errors, FALSE if not
	 *
	 * @return TRUE if the dialog has any map errors, FALSE if not
	 */
	UBOOL HasAnyErrors() const;

	/**
	 * Is this item visible?...
	 *
	 * @param	InType					The message type (error/warning/...).
	 * @param	InGroup					The message group (kismet/mobile/...).
	 */
	UBOOL IsThisVisible( MapCheckType InType, MapCheckGroup InGroup );

	/**
	 * Gets a suitable string from the specified MapCheck filter type and group...
	 *
	 * @param	InType					The message type (error/warning/...).
	 * @param	InGroup					The message group (kismet/mobile/...).
	 */
	static FString GetErrorTypeString( MapCheckType InType, MapCheckGroup InGroup );

	/**
	 * Gets a suitable column label from the specified MapCheck filter type and group...
	 *
	 * @param	InType					The message type (error/warning/...).
	 * @param	InGroup					The message group (kismet/mobile/...).
	 */
	static FString GetErrorTypeColumnLabel( MapCheckType InType, MapCheckGroup InGroup );

	/**
	 * Gets a suitable icon from the specified MapCheck filter type and group...
	 *
	 * @param	InType					The message type (error/warning/...).
	 * @param	InGroup					The message group (kismet/mobile/...).
	 */
	static INT GetIconIndex( MapCheckType InType, MapCheckGroup InGroup );

	/**
	 * Adds a message to the map check dialog, to be displayed when the dialog is shown.
	 *
	 * @param	InType					The message type (error/warning/...).
	 * @param	InGroup					The message group (kismet/mobile/...).
	 * @param	InActor					Actor associated with the message; can be NULL.
	 * @param	InMessage				The message to display.
	 * @param	InUDNPage				UDN Page to visit if the user needs more info on the warning.  This will send the user to https://udn.epicgames.com/Three/MapErrors#InUDNPage. 
	 */
	static void AddItem(MapCheckType InType, MapCheckGroup InGroup, UObject* InActor, const TCHAR* InMessage, const TCHAR* InUDNPage=TEXT(""));

	virtual void Serialize(FArchive& Ar);

	/**
	* Loads the list control with the contents of the GErrorWarningInfoList array.
	*/
	void LoadListCtrl();

	virtual bool Show( bool show = true );

protected:
	wxListCtrl*						ErrorWarningList;
	wxImageList*					ImageList;
	TArray<UObject*>				ReferencedObjects;
	TArray<FCheckErrorWarningInfo>*	ErrorWarningInfoList;
	FString							DlgPosSizeName;
	INT								SortColumn;

	UBOOL ListFilter_CriticalError;
	UBOOL ListFilter_Error;
	UBOOL ListFilter_PerformanceWarning;
	UBOOL ListFilter_Warning;
	UBOOL ListFilter_Note;
	UBOOL ListFilter_Info;

	UBOOL ListFilter_Kismet;
	UBOOL ListFilter_MobilePlatform;

	wxCheckBox* CheckBox_CriticalError;
	wxCheckBox* CheckBox_Error;
	wxCheckBox* CheckBox_PerformanceWarning;
	wxCheckBox* CheckBox_Warning;
	wxCheckBox* CheckBox_Note;
	wxCheckBox* CheckBox_Info;

	wxCheckBox* CheckBox_Kismet;
	wxCheckBox* CheckBox_MobilePlatform;

	wxButton*	CloseButton;
	wxButton*	RefreshButton;
	wxButton*	GotoButton;
	wxButton*	DeleteButton;
	wxButton*	PropertiesButton;
	wxButton*	HelpButton;
	wxButton*	CopyToClipboardButton;

	/**
	 * Removes all items from the map check dialog that pertain to the specified object.
	 *
	 * @param	Object		The object to match when removing items.
	 */
	void RemoveObjectItems(UObject* Object);

	/** Event handler for when the close button is clicked on. */
	void OnClose(wxCommandEvent& In);

	/** Event handler for when the refresh button is clicked on. */
	virtual void OnRefresh(wxCommandEvent& In);

	/** Event handler for when the goto button is clicked on. */
	virtual void OnGoTo(wxCommandEvent& In);

	/** Event handler for when the delete button is clicked on. */
	virtual void OnDelete(wxCommandEvent& In);

	/** Event handler for when the properties button is clicked on. */
	virtual void OnGotoProperties(wxCommandEvent& In);

	/** Event handler for when the "Show Help" button is clicked on. */
	void OnShowHelpPage(wxCommandEvent& In);

	/** 
	 * Event handler for when the "Copy to Clipboard" button is clicked on. Copies the contents
	 * of the dialog to the clipboard. If no items are selected, the entire contents are copied.
	 * If any items are selected, only selected items are copied.
	 */
	void OnCopyToClipboard(wxCommandEvent& In);

	/** Event handler for when a message is clicked on. */
	virtual void OnItemActivated(wxListEvent& In);

	/** Event handler for when a column is clicked on. */
	void OnColumnClicked (wxListEvent& In);

	/** Event handler for when wx wants to update UI elements. */
	void OnUpdateUI(wxUpdateUIEvent& In);

	/** Event handler for when the user changes the check status of the filtering categories */
	void OnFilter(wxCommandEvent& In);

	/** Event handler for when the filtering check boxes need updating/redrawing */
	void OnUpdateFilterUI(wxUpdateUIEvent& In);

	/** Filter check boxes need updating/redrawing */
	void OnSetFilterCheckBoxes();

public:
	/** Set up the columns for the ErrorWarningList control. */
	virtual void SetupErrorWarningListColumns();

	/** 
	 *	Get the level/package name for the given object.
	 *
	 *	@param	InObject	The object to retrieve the level/package name for.
	 *
	 *	@return	FString		The name of the level/package.
	 */
	static FString GetLevelOrPackageName(UObject* InObject);

protected:
	/** Sort based on active column. */
	virtual void SortErrorWarningInfoList();

protected:
	DECLARE_EVENT_TABLE()
};

static TArray<FCheckErrorWarningInfo> GErrorWarningInfoList;

#endif // __DLGMAPCHECK_H__
