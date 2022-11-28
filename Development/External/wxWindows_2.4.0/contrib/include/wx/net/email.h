/////////////////////////////////////////////////////////////////////////////
// Name:        email.h
// Purpose:     wxEmail: portable email client class
// Author:      Julian Smart
// Modified by:
// Created:     2001-08-21
// RCS-ID:      $Id: email.h,v 1.3 2002/09/07 12:10:20 GD Exp $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "email.h"
#endif

#ifndef _WX_EMAIL_H_
#define _WX_EMAIL_H_

#include "wx/net/msg.h"

/*
 * wxEmail
 * Miscellaneous email functions
 */

class wxEmail
{
public:
//// Ctor/dtor
    wxEmail() {};

//// Operations

    // Send a message.
    // Specify profile, or leave it to wxWindows to find the current user name
    static bool Send(wxMailMessage& message, const wxString& profileName = wxEmptyString,
        const wxString& sendMail = wxT("/usr/lib/sendmail -t"));
    
protected:
};


#endif //_WX_EMAIL_H_

