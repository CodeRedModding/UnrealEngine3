/////////////////////////////////////////////////////////////////////////////
// Name:        printps.h
// Purpose:     wxPostScriptPrinter, wxPostScriptPrintPreview
//              wxGenericPageSetupDialog
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: printps.h,v 1.8 2002/09/13 22:00:45 RR Exp $
// Copyright:   (c)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __PRINTPSH__
#define __PRINTPSH__

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "printps.h"
#endif

#include "wx/prntbase.h"

#if wxUSE_PRINTING_ARCHITECTURE && wxUSE_POSTSCRIPT

// ----------------------------------------------------------------------------
// Represents the printer: manages printing a wxPrintout object
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxPostScriptPrinter : public wxPrinterBase
{
public:
    wxPostScriptPrinter(wxPrintDialogData *data = (wxPrintDialogData *) NULL);
    virtual ~wxPostScriptPrinter();

    virtual bool Print(wxWindow *parent, wxPrintout *printout, bool prompt = TRUE);
    virtual wxDC* PrintDialog(wxWindow *parent);
    virtual bool Setup(wxWindow *parent);
    
private:
    DECLARE_DYNAMIC_CLASS(wxPostScriptPrinter)
};

// ----------------------------------------------------------------------------
// wxPrintPreview: programmer creates an object of this class to preview a
// wxPrintout.
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxPostScriptPrintPreview : public wxPrintPreviewBase
{
public:
    wxPostScriptPrintPreview(wxPrintout *printout,
                             wxPrintout *printoutForPrinting = (wxPrintout *) NULL,
                             wxPrintDialogData *data = (wxPrintDialogData *) NULL);
    wxPostScriptPrintPreview(wxPrintout *printout,
                             wxPrintout *printoutForPrinting,
                             wxPrintData *data);

    virtual ~wxPostScriptPrintPreview();

    virtual bool Print(bool interactive);
    virtual void DetermineScaling();

private:
    void Init(wxPrintout *printout, wxPrintout *printoutForPrinting);

private:
    DECLARE_CLASS(wxPostScriptPrintPreview)
};

#endif

#endif
// __PRINTPSH__
