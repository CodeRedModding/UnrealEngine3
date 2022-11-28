#ifndef _WX_FILEDLG_H_BASE_
#define _WX_FILEDLG_H_BASE_

#if wxUSE_FILEDLG

enum
{
    wxOPEN              = 0x0001,
    wxSAVE              = 0x0002,
    wxOVERWRITE_PROMPT  = 0x0004,
    wxHIDE_READONLY     = 0x0008,
    wxFILE_MUST_EXIST   = 0x0010,
    wxMULTIPLE          = 0x0020,
    wxCHANGE_DIR        = 0x0040
};

#if defined (__WXUNIVERSAL__)
#include "wx/generic/filedlgg.h"
#elif defined(__WXMSW__)
#include "wx/msw/filedlg.h"
#elif defined(__WXMOTIF__)
#include "wx/motif/filedlg.h"
#elif defined(__WXGTK__)
#include "wx/generic/filedlgg.h"
#elif defined(__WXX11__)
#include "wx/generic/filedlgg.h"
#elif defined(__WXMGL__)
#include "wx/generic/filedlgg.h"
#elif defined(__WXMAC__)
#include "wx/mac/filedlg.h"
#elif defined(__WXPM__)
#include "wx/os2/filedlg.h"
#elif defined(__WXSTUBS__)
#include "wx/stubs/filedlg.h"
#endif

#endif // wxUSE_FILEDLG

#endif
    // _WX_FILEDLG_H_BASE_
