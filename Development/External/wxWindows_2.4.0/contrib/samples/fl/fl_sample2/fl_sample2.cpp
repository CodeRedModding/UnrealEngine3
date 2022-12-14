/////////////////////////////////////////////////////////////////////////////
// Name:        fl_sample2.cpp
// Purpose:     Contrib. demo
// Author:      Aleksandras Gluchovas
// Modified by: Sebastian Haase (June 21, 2001)
// Created:     24/11/98     
// RCS-ID:      $Id: fl_sample2.cpp,v 1.1 2001/07/24 15:27:12 JS Exp $
// Copyright:   (c) Aleksandras Gluchovas
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "wx/textctrl.h"

// fl headers
#include "wx/fl/controlbar.h"

// plugins used
#include "wx/fl/barhintspl.h"
#include "wx/fl/hintanimpl.h"

#define ID_LOAD  102
#define ID_STORE 103
#define ID_QUIT  104


class MyApp: public wxApp
{ 
public:
    bool OnInit(void);
};

class MyFrame: public wxFrame
{
protected:
    wxFrameLayout* mpLayout;
    wxWindow*      mpClientWnd;
    
    wxTextCtrl* CreateTextCtrl( const wxString& value );
        
public:
    MyFrame( wxWindow* parent, char *title );
    ~MyFrame();
    
    void populateMyFrame();
    void OnLoad( wxCommandEvent& event );
    void OnStore( wxCommandEvent& event );
    void OnQuit( wxCommandEvent& event );
    
    bool OnClose(void) { return TRUE; }
    
    DECLARE_EVENT_TABLE()
};

/***** Implementation for class MyApp *****/

IMPLEMENT_APP    (MyApp)

bool MyApp::OnInit(void)
{
    // wxWindows boiler-plate:
    
    MyFrame *frame = new MyFrame(NULL, "wxFrameLayout sample");
    
    wxMenu *file_menu = new wxMenu;
    
    file_menu->Append( ID_LOAD,  "&Load layout"  );
    file_menu->Append( ID_STORE, "&Store layout"  );
    file_menu->AppendSeparator();
    
    file_menu->Append( ID_QUIT, "E&xit" );
    
    wxMenuBar *menu_bar = new wxMenuBar;
    
    menu_bar->Append(file_menu, "&File");
    
    frame->CreateStatusBar(3);
    frame->SetMenuBar(menu_bar);
    
    frame->Show(TRUE);
    SetTopWindow(frame);
    frame->populateMyFrame();
    
    return TRUE;
}

/***** Immlementation for class MyFrame *****/

BEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_MENU( ID_LOAD,  MyFrame::OnLoad  )
    EVT_MENU( ID_STORE, MyFrame::OnStore )
    EVT_MENU( ID_QUIT,  MyFrame::OnQuit  )
END_EVENT_TABLE()

MyFrame::MyFrame( wxWindow* parent, char *title )
    : wxFrame( parent, -1, "NewTest-II", wxDefaultPosition,
          wxSize( 700, 500 ),
          wxCLIP_CHILDREN | wxMINIMIZE_BOX | wxMAXIMIZE_BOX |
          wxTHICK_FRAME   | wxSYSTEM_MENU  | wxCAPTION,
          "freimas" )
{
}

void MyFrame::populateMyFrame()
{
    mpClientWnd = CreateTextCtrl( "Client window" );
    
    mpLayout = new wxFrameLayout( this, mpClientWnd );
        
    ///  mpLayout->PushDefaultPlugins();
    ///  mpLayout->AddPlugin( CLASSINFO( cbBarHintsPlugin ) ); // facny "X"es and beveal for barso
    ///  //mpLayout->AddPlugin( CLASSINFO( cbHintAnimationPlugin ) );
        
    cbDimInfo sizes( 80,65,     // when docked horizontally      
                     80,165,    // when docked vertically        
                     180,30,    // when floated                  
                     TRUE,      // the bar is fixed-size
                     5,         // vertical gap (bar border)
                     5          // horizontal gap (bar border)
                   ); 
        
    // drop-in 20 bars    
    for( int i = 1; i <= 10; ++i )
    {
        wxSleep(1);
        wxYield(); // CHECK!
        
        char buf[4];
        sprintf( buf, "%d", i );
        wxString name = wxString("Bar-");
        name += buf;
        
        //sizes.mIsFixed = i % 2 > 0; // every fifth bar is not fixed-size
        
        if ( !sizes.mIsFixed ) name += " (flexible)";
        //      mpLayout->AddBar( CreateTextCtrl(name),// bar window
        mpLayout->AddBar( new wxTextCtrl(this, -1, name),// bar window
                          sizes, i % MAX_PANES,// alignment ( 0-top,1-bottom, etc)
                          0,                   // insert into 0th row (vert. position)
                          0,                   // offset from the start of row (in pixels)
                          name                 // name to refere in customization pop-ups
                        );

        mpLayout->RecalcLayout(true);
        
        // Layout();
        // Refresh();
    }
}

MyFrame::~MyFrame()
{
    // layout is not a window, should be released manually
    if ( mpLayout ) 
        delete mpLayout;
}

wxTextCtrl* MyFrame::CreateTextCtrl( const wxString& value )
{
    wxTextCtrl* pCtrl = new wxTextCtrl( this, -1, value, 
                                wxPoint(0,0), wxSize(1,1), wxTE_MULTILINE );
    
    pCtrl->SetBackgroundColour( wxColour( 255,255,255 ) );
    
    return pCtrl;
}

void MyFrame::OnLoad( wxCommandEvent& event )
{
    wxMessageBox("Hey - you found a BIG question-mark !!");
}

void MyFrame::OnStore( wxCommandEvent& event )
{
    wxMessageBox("Hey - you found another BIG question-mark !!");
}

void MyFrame::OnQuit( wxCommandEvent& event )
{
    Show( FALSE ); // TRICK:: hide it, to avoid flickered destruction
    
    Close(TRUE);
}

