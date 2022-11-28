///////////////////////////////////////////////////////////////////////////////
// Name:        wx/checklst.h
// Purpose:     wxCheckListBox class interface
// Author:      Vadim Zeitlin
// Modified by:
// Created:     12.09.00
// RCS-ID:      $Id: checklst.h,v 1.4 2001/08/25 14:52:25 VZ Exp $
// Copyright:   (c) Vadim Zeitlin
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CHECKLST_H_BASE_
#define _WX_CHECKLST_H_BASE_

#if wxUSE_CHECKLISTBOX

#include "wx/listbox.h"

// ----------------------------------------------------------------------------
// wxCheckListBox: a listbox whose items may be checked
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxCheckListBoxBase : public wxListBox
{
public:
    // check list box specific methods
    virtual bool IsChecked(size_t item) const = 0;
    virtual void Check(size_t item, bool check = TRUE) = 0;
};

#if defined(__WXUNIVERSAL__)
    #include "wx/univ/checklst.h"
#elif defined(__WXMSW__)
    #include "wx/msw/checklst.h"
#elif defined(__WXMOTIF__)
    #include "wx/motif/checklst.h"
#elif defined(__WXGTK__)
    #include "wx/gtk/checklst.h"
#elif defined(__WXMAC__)
    #include "wx/mac/checklst.h"
#elif defined(__WXPM__)
    #include "wx/os2/checklst.h"
#elif defined(__WXSTUBS__)
    #include "wx/stubs/checklst.h"
#endif

#endif // wxUSE_CHECKLISTBOX

#endif
    // _WX_CHECKLST_H_BASE_
