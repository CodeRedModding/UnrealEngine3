/////////////////////////////////////////////////////////////////////////////
// Name:        wx/colrdlgg.h
// Purpose:     wxColourDialog
// Author:      Vadim Zeitiln
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: colordlg.h,v 1.12.2.1 2002/10/29 00:02:07 VZ Exp $
// Copyright:   (c) wxWindows team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COLORDLG_H_BASE_
#define _WX_COLORDLG_H_BASE_

#if wxUSE_COLOURDLG

#if defined(__WXMSW__) && !defined(__WXUNIVERSAL__)
    #include "wx/msw/colordlg.h"
#elif defined(__WXMAC__) && !defined(__WXUNIVERSAL__)
    #include "wx/mac/colordlg.h"
#else
    #include "wx/generic/colrdlgg.h"

    #define wxColourDialog wxGenericColourDialog
    #define sm_classwxColourDialog sm_classwxGenericColourDialog
#endif

// get the colour from user and return it
wxColour WXDLLEXPORT
wxGetColourFromUser(wxWindow *parent = (wxWindow *)NULL,
                    const wxColour& colInit = wxNullColour);

#endif // wxUSE_COLOURDLG

#endif
    // _WX_COLORDLG_H_BASE_
