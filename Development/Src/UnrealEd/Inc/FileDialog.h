/*=============================================================================
	FileDialog.h:  UnrealEd dialog for choosing a file.  Always resets to the default directory to after the dialog has closed.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __FILEDIALOG_H__
#define __FILEDIALOG_H__

/**
  * UnrealEd dialog for choosing a file.  Always resets to the default directory to after the dialog has closed.
  */
class WxFileDialog : public wxFileDialog
{
public:
	WxFileDialog(wxWindow* parent, const wxString& message = TEXT("Choose a file"), const wxString& defaultDir = TEXT(""), const wxString& defaultFile = TEXT(""), const wxString& wildcard = TEXT("*.*"), long style = 0, const wxPoint& pos = wxDefaultPosition);

	/**
	 * Overloaded version of the normal wxFileDialog ShowModal function.  This function calls the parent's ShowModal, and then resets the FileManager directory
	 * to the default directory before returning.
	 * @return Returns wxID_OK if the user pressed OK, and wxID_CANCEL otherwise.
	 */
	int ShowModal();

private:

};

#endif

