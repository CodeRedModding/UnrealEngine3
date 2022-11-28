/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/dcprint.h
// Purpose:     wxPrinterDC class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: dcprint.h,v 1.8 2001/07/11 10:06:50 JS Exp $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DCPRINT_H_
#define _WX_DCPRINT_H_

#ifdef __GNUG__
    #pragma interface "dcprint.h"
#endif

#if wxUSE_PRINTING_ARCHITECTURE

#include "wx/dc.h"
#include "wx/cmndata.h"

class WXDLLEXPORT wxPrinterDC : public wxDC
{
public:
    // Create a printer DC (obsolete function: use wxPrintData version now)
    wxPrinterDC(const wxString& driver, const wxString& device, const wxString& output, bool interactive = TRUE, int orientation = wxPORTRAIT);

    // Create from print data
    wxPrinterDC(const wxPrintData& data);

    wxPrinterDC(WXHDC theDC);

    // override some base class virtuals
    virtual bool StartDoc(const wxString& message);
    virtual void EndDoc();
    virtual void StartPage();
    virtual void EndPage();

protected:
    virtual void DoDrawBitmap(const wxBitmap &bmp, wxCoord x, wxCoord y,
                              bool useMask = FALSE);
    virtual bool DoBlit(wxCoord xdest, wxCoord ydest,
                        wxCoord width, wxCoord height,
                        wxDC *source, wxCoord xsrc, wxCoord ysrc,
                        int rop = wxCOPY, bool useMask = FALSE, wxCoord xsrcMask = -1, wxCoord ysrcMask = -1);

    // init the dc
    void Init();

    wxPrintData m_printData;

private:
    DECLARE_CLASS(wxPrinterDC)
};

// Gets an HDC for the default printer configuration
// WXHDC WXDLLEXPORT wxGetPrinterDC(int orientation);

// Gets an HDC for the specified printer configuration
WXHDC WXDLLEXPORT wxGetPrinterDC(const wxPrintData& data);

#endif // wxUSE_PRINTING_ARCHITECTURE

#endif
    // _WX_DCPRINT_H_

