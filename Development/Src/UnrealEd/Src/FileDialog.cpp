/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "FileDialog.h"

WxFileDialog::WxFileDialog(wxWindow* parent, const wxString& message /* =  */, const wxString& defaultDir /* =  */, const wxString& defaultFile /* =  */, const wxString& wildcard /* =  */, long style /* = 0 */, const wxPoint& pos /* = wxDefaultPosition */) : 
wxFileDialog(parent, message, defaultDir, defaultFile, wildcard, style, pos)
{

}

/**
 * Overloaded version of the normal wxFileDialog ShowModal function.  This function calls the parent's ShowModal, and then resets the FileManager directory
 * to the default directory before returning.
 * @return Returns wxID_OK if the user pressed OK, and wxID_CANCEL otherwise.
 */
int WxFileDialog::ShowModal()
{
	GCallbackEvent->Send( CALLBACK_EditorPreModal );
	MakeModal(true);
	const int ReturnCode = wxFileDialog::ShowModal();
	MakeModal(false);
	GCallbackEvent->Send( CALLBACK_EditorPostModal );

	GFileManager->SetDefaultDirectory();
	return ReturnCode;
}
