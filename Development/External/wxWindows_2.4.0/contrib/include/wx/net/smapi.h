/////////////////////////////////////////////////////////////////////////////
// Name:        smapi.h
// Purpose:     Simple MAPI classes
// Author:      PJ Naughter <pjna@naughter.com>
// Modified by: Julian Smart
// Created:     2001-08-21
// RCS-ID:      $Id: smapi.h,v 1.2 2002/09/07 12:10:20 GD Exp $
// Copyright:   (c) PJ Naughter
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "smapi.h"
#endif

#ifndef _WX_SMAPI_H_
#define _WX_SMAPI_H_

#include "wx/net/msg.h"

class wxMapiData;

//The class which encapsulates the MAPI connection
class wxMapiSession
{
public:
    //Constructors / Destructors
    wxMapiSession();
    ~wxMapiSession();
    
    //Logon / Logoff Methods
    bool Logon(const wxString& sProfileName, const wxString& sPassword = wxEmptyString, wxWindow* pParentWnd = NULL);
    bool LoggedOn() const;
    bool Logoff();
    
    //Send a message
    bool Send(wxMailMessage& message);
    
    //General MAPI support
    bool MapiInstalled() const;
    
    //Error Handling
    long GetLastError() const;
    
protected:
    //Methods
    void Initialise();
    void Deinitialise(); 
    bool Resolve(const wxString& sName, void* lppRecip1);

    wxMapiData* m_data;
    
};


#endif //_WX_SMAPI_H_
