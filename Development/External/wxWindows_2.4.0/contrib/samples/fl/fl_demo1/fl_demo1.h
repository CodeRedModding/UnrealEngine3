/////////////////////////////////////////////////////////////////////////////
// Name:        No names yet.
// Purpose:     Contrib. demo
// Author:      Aleksandras Gluchovas
// Modified by: Sebastian Haase (June 21, 2001)
// Created:     04/11/98
// RCS-ID:      $Id: fl_demo1.h,v 1.3 2002/09/07 12:12:21 GD Exp $
// Copyright:   (c) Aleksandras Gluchovas
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

#ifndef __NEW_TEST_G__
#define __NEW_TEST_G__

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "fl_demo1.h"
#endif

#define NEW_TEST_SAVE 1101
#define NEW_TEST_LOAD 1102
#define NEW_TEST_EXIT 1103

#include "wx/panel.h"
#include "wx/statline.h"    

// Define a new application type
class MyApp: public wxApp
{ 
public:
    bool OnInit(void);
};

// Define a new frame type
class MyFrame: public wxFrame
{ 
public:
    wxFrameLayout*  mpLayout;
    wxTextCtrl*     mpClientWnd;
    
    wxTextCtrl* CreateTextCtrl( const wxString& value );
    
public:
    MyFrame(wxFrame *frame);
    virtual ~MyFrame();
    
    bool OnClose(void) { Show(FALSE); return TRUE; }
    
    void OnLoad( wxCommandEvent& event );
    void OnSave( wxCommandEvent& event );
    void OnExit( wxCommandEvent& event );
    
    DECLARE_EVENT_TABLE()
};

/*
 * Quick example of your own Separator class...
 */
class wxMySeparatorLine : public wxStaticLine
{
public:
    wxMySeparatorLine() 
    {}
    wxMySeparatorLine( wxWindow *parent, wxWindowID id) 
        : wxStaticLine( parent, id)
    {}

protected:
   virtual void DoSetSize( int x, int y,
                           int width, int height,
                           int sizeFlags = wxSIZE_AUTO);
};

#endif

