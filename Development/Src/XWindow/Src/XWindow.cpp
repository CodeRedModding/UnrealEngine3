/*=============================================================================
	XWindow.cpp: GUI window management code through wxWindows.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#include "XWindowPrivate.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

#define MSG_BOX_CAPTION		TEXT("Unreal Engine")

// Displays an error message.

INT XWindow_ShowErrorMessage( const FString& InText )
{
	wxMessageDialog dlg( wxTheApp->GetTopWindow(),
		*InText,
		MSG_BOX_CAPTION,
		wxICON_ERROR | wxCENTRE | wxOK );
	return dlg.ShowModal();
}

// Asks the user a yes/no question.

INT XWindow_AskQuestion( const FString& InText )
{
	wxMessageDialog dlg( wxTheApp->GetTopWindow(),
		*InText,
		MSG_BOX_CAPTION,
		wxICON_QUESTION | wxCENTRE | wxYES_NO | wxYES_DEFAULT );
	return dlg.ShowModal();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

