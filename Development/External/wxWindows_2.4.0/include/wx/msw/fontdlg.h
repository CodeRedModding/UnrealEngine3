/////////////////////////////////////////////////////////////////////////////
// Name:        fontdlg.h
// Purpose:     wxFontDialog class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: fontdlg.h,v 1.5 2002/05/12 22:26:01 VZ Exp $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_FONTDLG_H_
#define _WX_MSW_FONTDLG_H_

#ifdef __GNUG__
    #pragma interface "fontdlg.h"
#endif

// ----------------------------------------------------------------------------
// wxFontDialog
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxFontDialog : public wxFontDialogBase
{
public:
    wxFontDialog() : wxFontDialogBase() { /* must be Create()d later */ }
    wxFontDialog(wxWindow *parent)
        : wxFontDialogBase(parent) { Create(parent); }
    wxFontDialog(wxWindow *parent, const wxFontData& data)
        : wxFontDialogBase(parent, data) { Create(parent, data); }

    virtual int ShowModal();

    // deprecated interface, don't use
    wxFontDialog(wxWindow *parent, const wxFontData *data)
        : wxFontDialogBase(parent, data) { Create(parent, data); }

protected:
    DECLARE_DYNAMIC_CLASS(wxFontDialog)
};

#endif
    // _WX_MSW_FONTDLG_H_

