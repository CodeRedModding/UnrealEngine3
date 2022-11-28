/////////////////////////////////////////////////////////////////////////////
// Name:        wx/toolbar.h
// Purpose:     wxToolBar interface declaration
// Author:      Vadim Zeitlin
// Modified by:
// Created:     20.11.99
// RCS-ID:      $Id: toolbar.h,v 1.25 2002/03/15 00:33:05 VZ Exp $
// Copyright:   (c) wxWindows team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TOOLBAR_H_BASE_
#define _WX_TOOLBAR_H_BASE_

#include "wx/tbarbase.h"     // the base class for all toolbars

#if wxUSE_TOOLBAR
    #if !wxUSE_TOOLBAR_NATIVE && !defined(__WXUNIVERSAL__)
        #include "wx/tbarsmpl.h"

        class WXDLLEXPORT wxToolBar : public wxToolBarSimple
        {
        public:
            wxToolBar() { }

            wxToolBar(wxWindow *parent,
                      wxWindowID id,
                      const wxPoint& pos = wxDefaultPosition,
                      const wxSize& size = wxDefaultSize,
                      long style = wxNO_BORDER | wxTB_HORIZONTAL,
                      const wxString& name = wxToolBarNameStr)
                : wxToolBarSimple(parent, id, pos, size, style, name) { }

            // the most commonly used version of AddTool()
            wxToolBarToolBase *AddTool(int id,
                               const wxBitmap& bitmap,
                               const wxString& shortHelpString = wxEmptyString,
                               const wxString& longHelpString = wxEmptyString)
            {
                return wxToolBarSimple::AddTool(id, bitmap, wxNullBitmap, FALSE, -1, -1, NULL,
                       shortHelpString, longHelpString);
            }

            // old form
            wxToolBarToolBase *AddTool
                               (
                                   int id,
                                   const wxBitmap& bitmap,
                                   const wxBitmap& pushedBitmap,
                                   bool toggle,
                                   wxObject *clientData = NULL,
                                   const wxString& shortHelpString = wxEmptyString,
                                   const wxString& longHelpString = wxEmptyString
                               )
            {
                return wxToolBarSimple::AddTool(id, bitmap, pushedBitmap, toggle, -1, -1, clientData,
                       shortHelpString, longHelpString);
            }

            // virtual overridden
            virtual wxToolBarToolBase *AddTool
                               (
                                   int id,
                                   const wxBitmap& bitmap,
                                   const wxBitmap& pushedBitmap,
                                   bool toggle,
                                   wxCoord xPos,
                                   wxCoord yPos = -1,
                                   wxObject *clientData = NULL,
                                   const wxString& shortHelpString = wxEmptyString,
                                   const wxString& longHelpString = wxEmptyString
                               )
            {
                return wxToolBarSimple::AddTool(id, bitmap, pushedBitmap, toggle, xPos, yPos, clientData,
                       shortHelpString, longHelpString);
            }

        private:
            DECLARE_DYNAMIC_CLASS(wxToolBar)
        };
    #else // wxUSE_TOOLBAR_NATIVE
        #if defined(__WXUNIVERSAL__)
           #include "wx/univ/toolbar.h"
        #elif defined(__WXMSW__) && defined(__WIN95__)
           #include "wx/msw/tbar95.h"
        #elif defined(__WXMSW__)
           #include "wx/msw/tbarmsw.h"
        #elif defined(__WXMOTIF__)
           #include "wx/motif/toolbar.h"
        #elif defined(__WXGTK__)
           #include "wx/gtk/tbargtk.h"
        #elif defined(__WXMAC__)
           #include "wx/mac/toolbar.h"
        #elif defined(__WXPM__)
           #include "wx/os2/toolbar.h"
        #elif defined(__WXSTUBS__)
           #include "wx/stubs/toolbar.h"
        #endif
    #endif // !wxUSE_TOOLBAR_NATIVE/wxUSE_TOOLBAR_NATIVE
#endif // wxUSE_TOOLBAR

#endif
    // _WX_TOOLBAR_H_BASE_
