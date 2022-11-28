/////////////////////////////////////////////////////////////////////////////
// Name:        helpview.h
// Purpose:     HelpView application
//              A standalone viewer for wxHTML Help (.htb) files
// Author:      Vaclav Slavik, Julian Smart
// Modified by:
// Created:     2002-07-09
// RCS-ID:      $Id: helpview.cpp,v 1.2.2.5 2002/12/09 18:37:20 JS Exp $
// Copyright:   (c) 2002 Vaclav Slavik, Julian Smart and others
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#pragma implementation "help.cpp"
#endif

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWindows headers
#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "wx/image.h"
#include "wx/wxhtml.h"
#include "wx/fs_zip.h"
#include "wx/log.h"
#include "wx/artprov.h"
#include "wx/filedlg.h"

#include "helpview.h"

class AlternateArtProvider : public wxArtProvider
{
protected:
    virtual wxBitmap CreateBitmap(const wxArtID& id, const wxArtClient& client,
                                  const wxSize& size);
};

IMPLEMENT_APP(hvApp)

hvApp::hvApp()
{
#if hvUSE_IPC
    m_server = NULL;
#endif
}

bool hvApp::OnInit()
{
#ifdef __WXMOTIF__
    delete wxLog::SetActiveTarget(new wxLogStderr); // So dialog boxes aren't used
#endif
	
    wxArtProvider::PushProvider(new AlternateArtProvider);
	
    int istyle = wxHF_DEFAULT_STYLE;
	
    wxString service, windowName, book[10], titleFormat, argStr;
    int bookCount = 0;
    int i;
    bool hasService = FALSE;
    bool hasWindowName = FALSE;
    bool createServer = FALSE;
	
#if hvUSE_IPC
    m_server = NULL;
#endif
	
    // Help books are recognized by extension ".hhp" ".htb" or ".zip".
    // Service and window_name can occur anywhere in arguments,
    // but service must be first
    // Other arguments (topic?) could be added
	
    //  modes of operation:
    //  1) no arguments - stand alone, prompt user for book
    //  2) books only - stand alone, open books
    //  3) "--server" as (any) arg - start connection with default service;
    //     also open any books passed as other arguments
    //  4) at least one argument which is not book, and not "--server" - take first
    //     such argument as service, second (if present) as window name,
    //     start service, open any books
	
    for( i=1; i < argc; i++ )
    {
		argStr = argv[i];
		
		if ( argStr.Find( wxT(".hhp") ) >= 0 ||
			argStr.Find( wxT(".htb") ) >= 0 ||
			argStr.Find( wxT(".zip") ) >= 0 ) {
			book[bookCount] = argStr;
			bookCount++; 
		}
		else if ( argStr == wxT("--server") )
		{
			createServer = TRUE;
#if defined(__WXMSW__)
			service = wxT("generic_helpservice");
#elif defined(__UNIX__)
			service = wxT("/tmp/") + wxString(wxT("generic_helpservice"));
#else
			service = wxT("4242");
#endif
		}
		else if ( !hasService )
		{
			service = argStr;
			hasService = TRUE;
			createServer = TRUE;
		}
		else if ( !hasWindowName )
		{
			windowName = argStr;
			hasWindowName = TRUE;
		}
		else if ( argStr.Find( wxT("--Style") )  >= 0 )
		{
			long i;
			wxString numb = argStr.AfterLast(wxT('e'));
			if ( !(numb.ToLong(&i) ) )
			{
				wxLogError( wxT("Integer conversion failed for --Style") );
			}
			else
			{
				istyle = i;
			}
		}
		else
		{
			//unknown - could be topic?
		}
    }
	
    //no book - query user
    if ( bookCount < 1 )
    {
		wxString s = wxFileSelector( wxT("Open help file"),
			wxGetCwd(),
			wxEmptyString,
			wxEmptyString,
			wxT(
			"Help books (*.htb)|*.htb|Help books (*.zip)|*.zip|\
			HTML Help Project (*.hhp)|*.hhp"),
			wxOPEN | wxFILE_MUST_EXIST,
			NULL);
		
		if (!s.IsEmpty())
		{
			book[0] = s;
			bookCount = 1;
		}
    } 
	
#if hvUSE_IPC
	
    if ( createServer ) {
		// Create a new server
		m_server = new hvServer;
		
		if ( !m_server->Create(service) ) {
			wxString wxm = wxT("Server Create failed - service: ");
			wxString xxm = wxm << service;
			wxLogError( xxm );
			//if MSW quits here, probably another copy already exists
			return FALSE;
			
		}
		createServer = FALSE; 
    }
	
#endif  // hvUSE_IPC
	
    //now add help
    wxInitAllImageHandlers();
    wxFileSystem::AddHandler(new wxZipFSHandler); 
	
    SetVendorName(wxT("wxWindows") );
    SetAppName(wxT("wxHTMLHelpServer") ); 
    wxConfig::Get(); // create an instance
	
    m_helpController = new wxHtmlHelpController( istyle );
	
    if ( !hasWindowName )
        titleFormat = wxT("Help: %s") ;
    else
    {
		//remove underscores
		windowName.Replace( wxT("_"), wxT(" ") );
		titleFormat = windowName;
    }
	
    m_helpController->SetTitleFormat( titleFormat );
	
    for( i=0; i < bookCount; i++ )
    {
        wxFileName fileName(book[i]);
		m_helpController->AddBook(fileName);
    }
	
#ifdef __WXMOTIF__
    delete wxLog::SetActiveTarget(new wxLogGui);
#endif
	
    m_helpController -> DisplayContents();
	
    return TRUE;
}


int hvApp::OnExit()
{
#if hvUSE_IPC
    wxNode* node = m_connections.First();
    while (node)
    {
        wxNode* next = node->Next();
        hvConnection* connection = (hvConnection*) node->Data();
        connection->Disconnect();
        delete connection;
        node = next;
    }
    m_connections.Clear();
	
    if (m_server)
    {
        delete m_server;
        m_server = NULL;
    }
#endif
	
    delete m_helpController;
    delete wxConfig::Set(NULL);
	
    return 0;
}

bool hvApp::OpenBook(wxHtmlHelpController* controller)
{
    wxString s = wxFileSelector(_("Open help file"),
        wxGetCwd(),
        wxEmptyString,
        wxEmptyString,
        _(
		"Help books (*.htb)|*.htb|Help books (*.zip)|*.zip|\
		HTML Help Project (*.hhp)|*.hhp"),
		wxOPEN | wxFILE_MUST_EXIST,
		NULL);
	
    if (!s.IsEmpty())
    {
        wxString ext = s.Right(4).Lower();
        if (ext == _T(".zip") || ext == _T(".htb") || ext == _T(".hhp"))
        {
            wxBusyCursor bcur;
            wxFileName fileName(s);
            controller->AddBook(fileName);
            return TRUE;
        }
    }
    return FALSE;
}

/*
* Art provider class
*/

// ---------------------------------------------------------------------
// helper macros
// ---------------------------------------------------------------------

// Standard macro for getting a resource from XPM file:
#define ART(artId, xpmRc) \
if ( id == artId ) return wxBitmap(xpmRc##_xpm);

// Compatibility hack to use wxApp::GetStdIcon of overriden by the user
#if WXWIN_COMPATIBILITY_2_2
#define GET_STD_ICON_FROM_APP(iconId) \
	if ( client == wxART_MESSAGE_BOX ) \
{ \
	wxIcon icon = wxTheApp->GetStdIcon(iconId); \
	if ( icon.Ok() ) \
{ \
	wxBitmap bmp; \
	bmp.CopyFromIcon(icon); \
	return bmp; \
} \
}
#else
#define GET_STD_ICON_FROM_APP(iconId)
#endif

// There are two ways of getting the standard icon: either via XPMs or via
// wxIcon ctor. This depends on the platform:
#if defined(__WXUNIVERSAL__)
#define CREATE_STD_ICON(iconId, xpmRc) return wxNullBitmap;
#elif defined(__WXGTK__) || defined(__WXMOTIF__)
#define CREATE_STD_ICON(iconId, xpmRc) return wxBitmap(xpmRc##_xpm);
#else
#define CREATE_STD_ICON(iconId, xpmRc) \
{ \
	wxIcon icon(_T(iconId)); \
	wxBitmap bmp; \
	bmp.CopyFromIcon(icon); \
	return bmp; \
}
#endif

// Macro used in CreateBitmap to get wxICON_FOO icons:
#define ART_MSGBOX(artId, iconId, xpmRc) \
    if ( id == artId ) \
{ \
	GET_STD_ICON_FROM_APP(iconId) \
	CREATE_STD_ICON(#iconId, xpmRc) \
}

// ---------------------------------------------------------------------
// XPMs with the art
// ---------------------------------------------------------------------

// XPM hack: make the arrays const
//#define static static const

#include "bitmaps/helpback.xpm"
#include "bitmaps/helpbook.xpm"
#include "bitmaps/helpdown.xpm"
#include "bitmaps/helpforward.xpm"
#include "bitmaps/helpoptions.xpm"
#include "bitmaps/helppage.xpm"
#include "bitmaps/helpsidepanel.xpm"
#include "bitmaps/helpup.xpm"
#include "bitmaps/helpuplevel.xpm"
#include "bitmaps/helpicon.xpm"
#include "bitmaps/helpopen.xpm"

//#undef static

// ---------------------------------------------------------------------
// CreateBitmap routine
// ---------------------------------------------------------------------

wxBitmap AlternateArtProvider::CreateBitmap(const wxArtID& id,
                                            const wxArtClient& client,
                                            const wxSize& WXUNUSED(size))
{
    ART(wxART_HELP_SIDE_PANEL,                     helpsidepanel)
		ART(wxART_HELP_SETTINGS,                       helpoptions)
		ART(wxART_HELP_BOOK,                           helpbook)
		ART(wxART_HELP_FOLDER,                         helpbook)
		ART(wxART_HELP_PAGE,                           helppage)
		//ART(wxART_ADD_BOOKMARK,                        addbookm)
		//ART(wxART_DEL_BOOKMARK,                        delbookm)
		ART(wxART_GO_BACK,                             helpback)
		ART(wxART_GO_FORWARD,                          helpforward)
		ART(wxART_GO_UP,                               helpup)
		ART(wxART_GO_DOWN,                             helpdown)
		ART(wxART_GO_TO_PARENT,                        helpuplevel)
		ART(wxART_FILE_OPEN,                           helpopen)
		if (client == wxART_HELP_BROWSER)
		{
			//ART(wxART_FRAME_ICON,                          helpicon)
			ART(wxART_HELP,                          helpicon)
		}
		
		//ART(wxART_GO_HOME,                             home)
		
		// Any wxWindows icons not implemented here
		// will be provided by the default art provider.
		return wxNullBitmap;
}

#if hvUSE_IPC

wxConnectionBase *hvServer::OnAcceptConnection(const wxString& topic)
{
    if (topic == wxT("HELP"))
        return new hvConnection();
    else
        return NULL;
}

// ----------------------------------------------------------------------------
// hvConnection
// ----------------------------------------------------------------------------

hvConnection::hvConnection()
: wxConnection()
{
    wxGetApp().GetConnections().Append(this);
}

hvConnection::~hvConnection()
{
    wxGetApp().GetConnections().DeleteObject(this);
}

bool hvConnection::OnExecute(const wxString& WXUNUSED(topic),
                             wxChar *data,
                             int WXUNUSED(size),
                             wxIPCFormat WXUNUSED(format))
{
	//    wxLogStatus("Execute command: %s", data);
	
	if ( !wxStrncmp( data, wxT("--intstring"), 11 ) )
	{
        long i;
		wxString argStr = data;
		wxString numb = argStr.AfterLast(wxT('g'));
		if ( !(numb.ToLong(&i) ) ) {
			wxLogError( wxT("Integer conversion failed for --intstring") );
		}
		else {				 
			wxGetApp().GetHelpController()->Display(int(i));
		}
	}
	else
	{
		wxGetApp().GetHelpController()->Display(data);
	}
    
    return TRUE;
}

bool hvConnection::OnPoke(const wxString& WXUNUSED(topic),
                          const wxString& item,
                          wxChar *data,
                          int WXUNUSED(size),
                          wxIPCFormat WXUNUSED(format))
{
	//    wxLogStatus("Poke command: %s = %s", item.c_str(), data);
	//topic is not tested
	
	if ( wxGetApp().GetHelpController() )
	{
		if ( item == wxT("--AddBook") )
		{
			wxGetApp().GetHelpController()->AddBook(data);
		}
		else if ( item == wxT("--DisplayContents") )
		{
			wxGetApp().GetHelpController()->DisplayContents();
		}
		else if ( item == wxT("--DisplayIndex") )
		{
			wxGetApp().GetHelpController()->DisplayIndex();
		}
		else if ( item == wxT("--KeywordSearch") )
		{
			wxGetApp().GetHelpController()->KeywordSearch(data);
		}
		else if ( item == wxT("--SetTitleFormat") )
		{
			wxString newname = data; 
			newname.Replace( wxT("_"), wxT(" ") );
			wxGetApp().GetHelpController()->SetTitleFormat(newname);
			//does not redraw title bar?
			//wxGetApp().GetHelpController()->ReFresh(); - or something
		}
		else if ( item == wxT("--SetTempDir") )
		{
			wxGetApp().GetHelpController()->SetTempDir(data);
		}
		else if ( item == wxT("--YouAreDead") )
		{
			// don't really know how to kill app from down here...
			// use wxKill from client instead
			//wxWindow *win = wxTheApp->GetTopWindow();
			//if ( win )
			//	win->Destroy();
		}
	}
	
    return TRUE;
}

wxChar *hvConnection::OnRequest(const wxString& WXUNUSED(topic),
								const wxString& WXUNUSED(item),
								int * WXUNUSED(size),
								wxIPCFormat WXUNUSED(format))
{
    return NULL;
}

bool hvConnection::OnStartAdvise(const wxString& WXUNUSED(topic),
                                 const wxString& WXUNUSED(item))
{
    return TRUE;
}

#endif
    // hvUSE_IPC
