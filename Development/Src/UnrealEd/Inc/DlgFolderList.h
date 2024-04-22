/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DLGFOLDERLIST_H__
#define __DLGFOLDERLIST_H__

// Forward declarations.
class WxEditorFrame;

/**
 * Set File Listeners dialog.
 */
class WxDlgFolderList : public wxDialog
{
public:
	WxDlgFolderList(WxEditorFrame *parent, wxWindowID id);
	virtual ~WxDlgFolderList();

private:

	UBOOL				bChanged;
	wxListBox*	pListBox;

	void OnAdd( wxCommandEvent& In );
	void OnRemove( wxCommandEvent& In );
	void OnCancel( wxCommandEvent& In ) { wxCloseEvent CloseEvent; OnClose(CloseEvent); }
	void OnClose( wxCloseEvent& In );

	INT FindPath( const FString& Directory ) const;
	void RemovePath( const INT iIndex );
	void ImportPaths( void );
	void AppendPath( const FString& Directory, const UBOOL bSelect );
	void ExportPaths( void );

	DECLARE_EVENT_TABLE()
};

#endif // __DLGFOLDERLIST_H__
