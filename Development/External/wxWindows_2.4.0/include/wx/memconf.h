///////////////////////////////////////////////////////////////////////////////
// Name:        wx/memconf.h
// Purpose:     wxMemoryConfig class: a wxConfigBase implementation which only
//              stores the settings in memory (thus they are lost when the
//              program terminates)
// Author:      Vadim Zeitlin
// Modified by:
// Created:     22.01.00
// RCS-ID:      $Id: memconf.h,v 1.1 2000/01/22 23:15:51 VZ Exp $
// Copyright:   (c) 2000 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

/*
 * NB: I don't see how this class may possibly be useful to the application
 *     program (as the settings are lost on program termination), but it is
 *     handy to have it inside wxWindows. So for now let's say that this class
 *     is private and should only be used by wxWindows itself - this might
 *     change in the future.
 */

#ifndef _WX_MEMCONF_H_
#define _WX_MEMCONF_H_

// no #pragma interface because no implementation file

#if wxUSE_CONFIG

#include "wx/fileconf.h"   // the base class

// ----------------------------------------------------------------------------
// wxMemoryConfig: a config class which stores settings in non-persistent way
// ----------------------------------------------------------------------------

// notice that we inherit from wxFileConfig which already stores its data in
// memory and just disable file reading/writing - this is probably not optimal
// and might be changed in future as well (this class will always deriev from
// wxConfigBase though)
class wxMemoryConfig : public wxFileConfig
{
public:
    // default (and only) ctor
    wxMemoryConfig() : wxFileConfig(wxEmptyString,  // default app name
                                    wxEmptyString,  // default vendor name
                                    wxEmptyString,  // no local config file
                                    wxEmptyString,  // no system config file
                                    0)              // don't use any files
    {
    }
};

#endif // wxUSE_CONFIG

#endif // _WX_MEMCONF_H_
