/*============================================================================
	SocketManager.h: AnimSet viewer's socket manager.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SOCKETMANAGER_H__
#define __SOCKETMANAGER_H__

enum
{
	ID_SM_WSRotXEntry,
	ID_SM_WSRotYEntry,
	ID_SM_WSRotZEntry
};

// Forward declarations.
class WxAnimSetViewer;
class WxPropertyWindow;

/**
 * Toolbar for the socket manager.
 */
class WxSocketManagerToolBar : public WxToolBar
{
public:
	WxSocketManagerToolBar(wxWindow* InParent, wxWindowID InID);

private:
	WxMaskedBitmap TranslateB;
	WxMaskedBitmap RotateB;
	WxMaskedBitmap ClearPreviewB;
	WxMaskedBitmap CopyB;
	WxMaskedBitmap PasteB;
};

enum EASVSocketMoveMode
{
	SMM_Rotate,
	SMM_Translate
};


/**
 * Socket manager.
 */
class WxSocketManager : public WxTrackableFrame, public FNotifyHook
{
public:
	WxSocketManager(WxAnimSetViewer* InASV, wxWindowID InID);

	wxListBox*				SocketList;
	WxPropertyWindowHost*	SocketProps;	
	wxButton*				NewSocketButton;
	wxButton*				DeleteSocketButton;
	WxSocketManagerToolBar*	ToolBar;
	WxAnimSetViewer*		AnimSetViewer;
	EASVSocketMoveMode		MoveMode;
	wxTextCtrl				*WSRotXEntry, *WSRotYEntry, *WSRotZEntry;

	void OnClose(wxCloseEvent& In);
	void OnRotEntry(wxCommandEvent& In);

	/**
	* Called when the window has been selected in the ctrl + tab dialog box.
	*/
	virtual void OnSelected();

	/**
	 * Calls the UpdateSocketPreviewComps function if we have change the preview Skel/Static Mesh.
	 */
	virtual void NotifyPostChange(void* Src, UProperty* PropertyThatChanged);

	virtual void NotifyExec(void* Src, const TCHAR* Cmd);

	void UpdateTextEntry();
	void ProcessTextEntry();

private:
	DECLARE_EVENT_TABLE()
};

#endif // __SOCKETMANAGER_H__
