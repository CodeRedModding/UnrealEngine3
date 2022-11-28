/////////////////////////////////////////////////////////////////////////////
// Name:        wx/choice.h
// Purpose:     wxChoice class interface
// Author:      Vadim Zeitlin
// Modified by:
// Created:     26.07.99
// RCS-ID:      $Id: choice.h,v 1.19 2002/08/31 11:29:09 GD Exp $
// Copyright:   (c) wxWindows team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CHOICE_H_BASE_
#define _WX_CHOICE_H_BASE_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "choicebase.h"
#endif

#if wxUSE_CHOICE

#include "wx/ctrlsub.h"     // the base class

// ----------------------------------------------------------------------------
// global data
// ----------------------------------------------------------------------------

WXDLLEXPORT_DATA(extern const wxChar*) wxChoiceNameStr;

// ----------------------------------------------------------------------------
// wxChoice allows to select one of a non-modifiable list of strings
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxChoiceBase : public wxControlWithItems
{
public:
    // all generic methods are in wxControlWithItems
    virtual ~wxChoiceBase();

    // single selection logic
    virtual void SetSelection(int n) = 0;
    virtual bool SetStringSelection(const wxString& s);

    // don't override this
    virtual void Select(int n) { SetSelection(n); }

    // set/get the number of columns in the control (as they're not supporte on
    // most platforms, they do nothing by default)
    virtual void SetColumns(int WXUNUSED(n) = 1 ) { }
    virtual int GetColumns() const { return 1 ; }

    // emulate selecting the item event.GetInt()
    void Command(wxCommandEvent& event);
};

// ----------------------------------------------------------------------------
// include the platform-dependent class definition
// ----------------------------------------------------------------------------

#if defined(__WXUNIVERSAL__)
    #include "wx/univ/choice.h"
#elif defined(__WXMSW__)
    #include "wx/msw/choice.h"
#elif defined(__WXMOTIF__)
    #include "wx/motif/choice.h"
#elif defined(__WXGTK__)
    #include "wx/gtk/choice.h"
#elif defined(__WXMAC__)
    #include "wx/mac/choice.h"
#elif defined(__WXPM__)
    #include "wx/os2/choice.h"
#elif defined(__WXSTUBS__)
    #include "wx/stubs/choice.h"
#endif

#endif // wxUSE_CHOICE

#endif
    // _WX_CHOICE_H_BASE_
