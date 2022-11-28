//-----------------------------------------------------------------------------
// Name:        derivdlg.cpp
// Purpose:     XML resources sample: A derived dialog
// Author:      Robert O'Connor (rob@medicalmnemonics.com), Vaclav Slavik
// RCS-ID:      $Id: derivdlg.cpp,v 1.1.2.1 2002/11/09 23:18:16 VS Exp $
// Copyright:   (c) Robert O'Connor and Vaclav Slavik
// Licence:     wxWindows licence
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// GCC implementation
//-----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "derivdlg.h"
#endif

//-----------------------------------------------------------------------------
// Standard wxWindows headers
//-----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

// For all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWindows headers)
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

//-----------------------------------------------------------------------------
// Header of this .cpp file
//-----------------------------------------------------------------------------

#include "derivdlg.h"

//-----------------------------------------------------------------------------
// Remaining headers: Needed wx headers, then wx/contrib headers, then application headers
//-----------------------------------------------------------------------------

#include "wx/xrc/xmlres.h"              // XRC XML resouces

//-----------------------------------------------------------------------------
// Event table: connect the events to the handler functions to process them
//-----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(PreferencesDialog, wxDialog)
    EVT_BUTTON( XRCID( "my_button" ), PreferencesDialog::OnMyButtonClicked )    
    EVT_UPDATE_UI(XRCID( "my_checkbox" ), PreferencesDialog::OuUpdateUIMyCheckbox )
    // Note that the ID here isn't a XRCID, it is one of the standard wx ID's.
    EVT_BUTTON( wxID_OK, PreferencesDialog::OnOK )
END_EVENT_TABLE()

//-----------------------------------------------------------------------------
// Public members
//-----------------------------------------------------------------------------
// Constructor (Notice how small and easy it is)
PreferencesDialog::PreferencesDialog(wxWindow* parent)
{    
    wxXmlResource::Get()->LoadDialog(this, parent, wxT("derived_dialog"));
}

// Destructor. (Empty, as I don't need anything special done when destructing).
PreferencesDialog::~PreferencesDialog()
{
}

//-----------------------------------------------------------------------------
// Private members (including the event handlers)
//-----------------------------------------------------------------------------

void PreferencesDialog::OnMyButtonClicked( wxCommandEvent &event )
{
    // Construct a message dialog.
    wxMessageDialog msgDlg(this, _("You clicked on My Button"));    
    
    // Show it modally.
    msgDlg.ShowModal();
}


// Update the enabled/disabled state of the edit/delete buttons depending on 
// whether a row (item) is selected in the listctrl
void PreferencesDialog::OuUpdateUIMyCheckbox( wxUpdateUIEvent &event )
{
    // Get a boolean value of whether the checkbox is checked
    bool myCheckBoxIsChecked;    
    // You could just write:
    // myCheckBoxIsChecked = event.IsChecked();
    // since the event that was passed into this function already has the 
    // is a pointer to the right control. However, 
    // this is the XRCCTRL way (which is more obvious as to what is going on).
    myCheckBoxIsChecked = XRCCTRL(*this, "my_checkbox", wxCheckBox)->IsChecked();

    // Now call either Enable(TRUE) or Enable(FALSE) on the textctrl, depending
    // on the value of that boolean.  
    XRCCTRL(*this, "my_textctrl", wxTextCtrl)->Enable(myCheckBoxIsChecked);  
}


void PreferencesDialog::OnOK( wxCommandEvent& event )
{
    // Construct a message dialog (An extra parameters to put a cancel button on).
    wxMessageDialog msgDlg2(this, _("Press OK to close Derived dialog, or Cancel to abort"),
                            _("Overriding base class OK button handler"),
                            wxOK | wxCANCEL | wxCENTER );
    
    // Show the message dialog, and if it returns wxID_OK (ie they clicked on OK button)...
    if (msgDlg2.ShowModal() == wxID_OK)
    {
        // ...then end this Preferences dialog.        
        EndModal( wxID_OK );
        // You could also have used event.Skip() which would then skip up
        // to the wxDialog's event table and see if there was a EVT_BUTTON
        // handler for wxID_OK and if there was, then execute that code. 
    }
    
    // Otherwise do nothing.
}
