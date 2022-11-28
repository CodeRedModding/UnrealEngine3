/////////////////////////////////////////////////////////////////////////////
// Name:        artprov.h
// Purpose:     wxArtProvider class
// Author:      Vaclav Slavik
// Modified by:
// Created:     18/03/2002
// RCS-ID:      $Id: artprov.h,v 1.10.2.1 2002/11/03 17:18:18 VS Exp $
// Copyright:   (c) Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ARTPROV_H_
#define _WX_ARTPROV_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "artprov.h"
#endif

#include "wx/string.h"
#include "wx/bitmap.h"
#include "wx/icon.h"

class WXDLLEXPORT wxArtProvidersList;
class WXDLLEXPORT wxArtProviderCache;
class wxArtProviderModule;

// ----------------------------------------------------------------------------
// Types
// ----------------------------------------------------------------------------

typedef wxString wxArtClient;
typedef wxString wxArtID;

#define wxART_MAKE_CLIENT_ID_FROM_STR(id)  (wxString(id)+_T("_C"))
#define wxART_MAKE_CLIENT_ID(id)           _T(#id) _T("_C")
#define wxART_MAKE_ART_ID_FROM_STR(id)     (id)
#define wxART_MAKE_ART_ID(id)              _T(#id)

// ----------------------------------------------------------------------------
// Art clients
// ----------------------------------------------------------------------------

#define wxART_TOOLBAR              wxART_MAKE_CLIENT_ID(wxART_TOOLBAR)         
#define wxART_MENU                 wxART_MAKE_CLIENT_ID(wxART_MENU)            
#define wxART_FRAME_ICON           wxART_MAKE_CLIENT_ID(wxART_FRAME_ICON)      

#define wxART_CMN_DIALOG           wxART_MAKE_CLIENT_ID(wxART_CMN_DIALOG)      
#define wxART_HELP_BROWSER         wxART_MAKE_CLIENT_ID(wxART_HELP_BROWSER)    
#define wxART_MESSAGE_BOX          wxART_MAKE_CLIENT_ID(wxART_MESSAGE_BOX)     

#define wxART_OTHER                wxART_MAKE_CLIENT_ID(wxART_OTHER)           

// ----------------------------------------------------------------------------
// Art IDs
// ----------------------------------------------------------------------------

#define wxART_ADD_BOOKMARK         wxART_MAKE_ART_ID(wxART_ADD_BOOKMARK)       
#define wxART_DEL_BOOKMARK         wxART_MAKE_ART_ID(wxART_DEL_BOOKMARK)       
#define wxART_HELP_SIDE_PANEL      wxART_MAKE_ART_ID(wxART_HELP_SIDE_PANEL)    
#define wxART_HELP_SETTINGS        wxART_MAKE_ART_ID(wxART_HELP_SETTINGS)      
#define wxART_HELP_BOOK            wxART_MAKE_ART_ID(wxART_HELP_BOOK)          
#define wxART_HELP_FOLDER          wxART_MAKE_ART_ID(wxART_HELP_FOLDER)        
#define wxART_HELP_PAGE            wxART_MAKE_ART_ID(wxART_HELP_PAGE)          
#define wxART_GO_BACK              wxART_MAKE_ART_ID(wxART_GO_BACK)            
#define wxART_GO_FORWARD           wxART_MAKE_ART_ID(wxART_GO_FORWARD)         
#define wxART_GO_UP                wxART_MAKE_ART_ID(wxART_GO_UP)              
#define wxART_GO_DOWN              wxART_MAKE_ART_ID(wxART_GO_DOWN)            
#define wxART_GO_TO_PARENT         wxART_MAKE_ART_ID(wxART_GO_TO_PARENT)       
#define wxART_GO_HOME              wxART_MAKE_ART_ID(wxART_GO_HOME)            
#define wxART_FILE_OPEN            wxART_MAKE_ART_ID(wxART_FILE_OPEN)          
#define wxART_PRINT                wxART_MAKE_ART_ID(wxART_PRINT)              
#define wxART_HELP                 wxART_MAKE_ART_ID(wxART_HELP)               
#define wxART_TIP                  wxART_MAKE_ART_ID(wxART_TIP)                
#define wxART_REPORT_VIEW          wxART_MAKE_ART_ID(wxART_REPORT_VIEW)        
#define wxART_LIST_VIEW            wxART_MAKE_ART_ID(wxART_LIST_VIEW)          
#define wxART_NEW_DIR              wxART_MAKE_ART_ID(wxART_NEW_DIR)            
#define wxART_FOLDER               wxART_MAKE_ART_ID(wxART_FOLDER)             
#define wxART_GO_DIR_UP            wxART_MAKE_ART_ID(wxART_GO_DIR_UP)          
#define wxART_EXECUTABLE_FILE      wxART_MAKE_ART_ID(wxART_EXECUTABLE_FILE)    
#define wxART_NORMAL_FILE          wxART_MAKE_ART_ID(wxART_NORMAL_FILE)        
#define wxART_TICK_MARK            wxART_MAKE_ART_ID(wxART_TICK_MARK)          
#define wxART_CROSS_MARK           wxART_MAKE_ART_ID(wxART_CROSS_MARK)         
#define wxART_ERROR                wxART_MAKE_ART_ID(wxART_ERROR)              
#define wxART_QUESTION             wxART_MAKE_ART_ID(wxART_QUESTION)           
#define wxART_WARNING              wxART_MAKE_ART_ID(wxART_WARNING)            
#define wxART_INFORMATION          wxART_MAKE_ART_ID(wxART_INFORMATION)        

// ----------------------------------------------------------------------------
// wxArtProvider class
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxArtProvider : public wxObject
{
public:
    // Add new provider to the top of providers stack.
    static void PushProvider(wxArtProvider *provider);

    // Remove latest added provider and delete it.
    static bool PopProvider();

    // Remove provider. The provider must have been added previously!
    // The provider is _not_ deleted.
    static bool RemoveProvider(wxArtProvider *provider);
    
    // Query the providers for bitmap with given ID and return it. Return
    // wxNullBitmap if no provider provides it.
    static wxBitmap GetBitmap(const wxArtID& id, 
                              const wxArtClient& client = wxART_OTHER,
                              const wxSize& size = wxDefaultSize);

    // Query the providers for icon with given ID and return it. Return
    // wxNullIcon if no provider provides it.
    static wxIcon GetIcon(const wxArtID& id,
                          const wxArtClient& client = wxART_OTHER,
                          const wxSize& size = wxDefaultSize);

protected:
    friend class wxArtProviderModule;
    // Initializes default provider
    static void InitStdProvider();
    // Destroy caches & all providers
    static void CleanUpProviders();

    // Derived classes must override this method to create requested 
    // art resource. This method is called only once per instance's
    // lifetime for each requested wxArtID.
    virtual wxBitmap CreateBitmap(const wxArtID& WXUNUSED(id),
                                  const wxArtClient& WXUNUSED(client),
                                  const wxSize& WXUNUSED(size)) = 0;

private:
    // list of providers:
    static wxArtProvidersList *sm_providers;
    // art resources cache (so that CreateXXX is not called that often):
    static wxArtProviderCache *sm_cache;

    DECLARE_ABSTRACT_CLASS(wxArtProvider)
};


#endif // _WX_ARTPROV_H_
