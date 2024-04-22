/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#if WITH_MANAGED_CODE
	#include "FileSystemNotificationShared.h"
#endif
#include "DlgFolderList.h"

BEGIN_EVENT_TABLE( WxDlgFolderList, wxDialog )
	EVT_BUTTON( IDM_DlgFolderList_Add, WxDlgFolderList::OnAdd )
	EVT_BUTTON( IDM_DlgFolderList_Remove, WxDlgFolderList::OnRemove )
	EVT_CLOSE( WxDlgFolderList::OnClose )
	EVT_BUTTON( wxID_CANCEL, WxDlgFolderList::OnCancel )
END_EVENT_TABLE()

WxDlgFolderList::WxDlgFolderList(WxEditorFrame *parent, wxWindowID id)
	:	wxDialog( parent, id, *LocalizeUnrealEd("SetFileListeners"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE )
{
	// Colour the background appropriately
	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));

	// Add button to the top left of the window
	wxPoint addPos( 8, 8 );
	wxButton *pAdd = new wxButton( this, IDM_DlgFolderList_Add, *LocalizeUnrealEd("Add"), addPos, wxDefaultSize );
	INT iAddWidth, iAddHeight;
	pAdd->GetSize( &iAddWidth, &iAddHeight );

	// Remove button next to the add button
	const wxPoint removePos( addPos.x + iAddWidth + addPos.x, addPos.y );
	wxButton *pRemove = new wxButton( this, IDM_DlgFolderList_Remove, *LocalizeUnrealEd("Remove"), removePos, wxDefaultSize );
	INT iRemoveWidth, iRemoveHeight;
	pRemove->GetSize( &iRemoveWidth, &iRemoveHeight );

	// Cancel button next to the remove button
	const wxPoint cancelPos( removePos.x + iRemoveWidth + addPos.x, addPos.y );
	wxButton *pCancel = new wxButton( this, wxID_CANCEL, *LocalizeUnrealEd("Cancel"), cancelPos, wxDefaultSize );
	INT iCancelWidth, iCancelHeight;
	pCancel->GetSize( &iCancelWidth, &iCancelHeight );

	// List box underneath the buttons
	const wxSize listSize( 384, Max( 128, ( addPos.x + iAddWidth + addPos.x + iRemoveWidth + addPos.x + iCancelWidth + addPos.x ) ) );
	const wxPoint listPos( addPos.x, addPos.y + iAddHeight + addPos.y );
	pListBox = new wxListBox( this, IDM_DlgFolderList, listPos, listSize, 0, NULL, ( wxLB_SINGLE | wxLB_NEEDED_SB | wxLB_HSCROLL | wxLB_SORT ) );
	INT iListWidth, iListHeight;
	pListBox->GetSize( &iListWidth, &iListHeight );

	const wxSize textSize( addPos.x + iListWidth + addPos.x, -1 );
	const wxPoint textPos( -1, addPos.y + iAddHeight + addPos.y + iListHeight + addPos.y );
	wxTextCtrl *pText = new wxTextCtrl(this, wxID_ANY, *LocalizeUnrealEd("SetFileListenersText"), textPos, textSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_AUTO_URL);

	// Reposition and Resize the window so that it encompasses the children
	FWindowUtil::LoadPosSize( TEXT("DlgFolderList"), this );
	Fit();

	// Import those paths that are already in the ini file
	ImportPaths();
	bChanged = FALSE;
}

WxDlgFolderList::~WxDlgFolderList()
{
	
}

void WxDlgFolderList::OnAdd( wxCommandEvent& In )
{
	// Select a directory and add it to the list (if not already)
	FString Directory;
	if (PromptUserForDirectory(Directory, *LocalizeUnrealEd("SetFileListeners"), *GApp->LastDir[LD_GENERIC_OPEN]))
	{
		FString relDir = GFileManager->ConvertToRelativePath( *Directory ) + TEXT( "\\" );
		FString absDir = GFileManager->ConvertToAbsolutePath( *Directory ) + TEXT( "\\" );
		if ( relDir != absDir )
		{
			// Prompt the user, do they wish to save this as abs or rel (deleting the other if it's present)
			if ( appMsgf(AMT_YesNo, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("SetFileListenersPrompt"), *relDir, *absDir))) )
			{
				RemovePath( FindPath( absDir ) );
				AppendPath( relDir, TRUE );
			}
			else
			{
				RemovePath( FindPath( relDir ) );
				AppendPath( absDir, TRUE );
			}
		}
		else
		{
			AppendPath( absDir, TRUE );
		}
	}
}

void WxDlgFolderList::OnRemove( wxCommandEvent& In )
{
	// Remove the currently selected directory from the list
	RemovePath( pListBox->GetSelection() );
}

void WxDlgFolderList::OnClose( wxCloseEvent& In )
{
	if ( bChanged )
	{
		bChanged = FALSE;
		ExportPaths();

#if WITH_MANAGED_CODE
		// Reboot the file system notification system, so the changes take affect
		CloseFileSystemNotification();
		SetFileSystemNotificationsForEditor(GEditor->AccessUserSettings().bAutoReimportTextures, GEditor->AccessUserSettings().bAutoReimportApexAssets);
		SetFileSystemNotificationsForAnimSet(GEditor->AccessUserSettings().bAutoReimportAnimSets);
#endif
	}

	// Save the window position
	FWindowUtil::SavePosSize( TEXT("DlgFolderList"), this );

	// Delete the external ptr to this widget
	WxEditorFrame* pParent = static_cast<WxEditorFrame*>( GetParent() );
	check( pParent->DlgFolderList == this );
	pParent->DlgFolderList = NULL;

	// Kill the widget
	Destroy();
}

INT WxDlgFolderList::FindPath( const FString& Directory ) const
{
	const wxString strDir = *Directory;
	return pListBox->FindString( strDir );
}

void WxDlgFolderList::RemovePath( const INT iIndex )
{
	if ( iIndex != wxNOT_FOUND )
	{
		pListBox->Delete( iIndex );
		bChanged = TRUE;
	}
}

void WxDlgFolderList::ImportPaths( void )
{
	// Loop through all those in the ini file and add to the box list
	TArray<FString> FileListenerDirectories;
	GConfig->GetSingleLineArray(TEXT("FileListener"), TEXT("AdditionalFileListenerDirectories"), FileListenerDirectories, GEditorUserSettingsIni);
	if ( FileListenerDirectories.Num() < 2 )
	{
		GConfig->GetArray(TEXT("FileListener"), TEXT("AdditionalFileListenerDirectories"), FileListenerDirectories, GEditorUserSettingsIni);
	}
	for (INT i = 0; i < FileListenerDirectories.Num(); ++i)
	{
		const FString &Directory = FileListenerDirectories(i);
		AppendPath( Directory, FALSE );
	}
}

void WxDlgFolderList::AppendPath( const FString& Directory, const UBOOL bSelect )
{
	INT iIndex = FindPath( Directory );
	if ( iIndex == wxNOT_FOUND )
	{
		const wxString strDir = *Directory;
		iIndex = pListBox->Append( strDir );
		bChanged = TRUE;
	}
	if ( bSelect )
	{
		pListBox->Select( iIndex );
	}
}

void WxDlgFolderList::ExportPaths( void )
{
	// Extract all the directories from the listbox to the ini file
	TArray<FString> FileListenerDirectories;
	const wxArrayString strDirs = pListBox->GetStrings();
	for (UINT i = 0; i < strDirs.GetCount(); ++i)
	{
		const wxString &strDir = strDirs.Item( i );
		const FString Directory( strDir.c_str() );
		FileListenerDirectories.AddItem( Directory );
	}
	GConfig->SetArray(TEXT("FileListener"), TEXT("AdditionalFileListenerDirectories"), FileListenerDirectories, GEditorUserSettingsIni);
}
