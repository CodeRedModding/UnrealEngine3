/////////////////////////////////////////////////////////////////////////////
// Name:        printps.cpp
// Purpose:     Postscript print/preview framework
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// RCS-ID:      $Id: printps.cpp,v 1.26 2002/08/30 13:47:57 CE Exp $
// Copyright:   (c) Julian Smart and Markus Holzem
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "printps.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/defs.h"

#if wxUSE_PRINTING_ARCHITECTURE && wxUSE_POSTSCRIPT

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/dc.h"
    #include "wx/app.h"
    #include "wx/msgdlg.h"
    #include "wx/intl.h"
    #include "wx/progdlg.h"
    #include "wx/log.h"									  
#endif

#include "wx/generic/printps.h"
#include "wx/dcprint.h"
#include "wx/printdlg.h"
#include "wx/generic/prntdlgg.h"
#include "wx/generic/progdlgg.h"
#include "wx/paper.h"

#include <stdlib.h>

// ----------------------------------------------------------------------------
// wxWin macros
// ----------------------------------------------------------------------------

    IMPLEMENT_DYNAMIC_CLASS(wxPostScriptPrinter, wxPrinterBase)
    IMPLEMENT_CLASS(wxPostScriptPrintPreview, wxPrintPreviewBase)
    
// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// Printer
// ----------------------------------------------------------------------------

wxPostScriptPrinter::wxPostScriptPrinter(wxPrintDialogData *data)
                   : wxPrinterBase(data)
{
}

wxPostScriptPrinter::~wxPostScriptPrinter()
{
}

bool wxPostScriptPrinter::Print(wxWindow *parent, wxPrintout *printout, bool prompt)
{
    sm_abortIt = FALSE;
    sm_abortWindow = (wxWindow *) NULL;

    if (!printout)
    {
        sm_lastError = wxPRINTER_ERROR;
        return FALSE;
    }

    printout->SetIsPreview(FALSE);

    // 4/9/99, JACS: this is a silly place to allow preparation, considering
    // the DC and no parameters have been set in the printout object.
    // Moved further down.

    // printout->OnPreparePrinting();

    // Get some parameters from the printout, if defined
    int fromPage, toPage;
    int minPage, maxPage;
    printout->GetPageInfo(&minPage, &maxPage, &fromPage, &toPage);

    if (maxPage == 0)
    {
        sm_lastError = wxPRINTER_ERROR;
        return FALSE;
    }

    m_printDialogData.SetMinPage(minPage);
    m_printDialogData.SetMaxPage(maxPage);
    if (fromPage != 0)
        m_printDialogData.SetFromPage(fromPage);
    if (toPage != 0)
        m_printDialogData.SetToPage(toPage);

    if (minPage != 0)
    {
        m_printDialogData.EnablePageNumbers(TRUE);
        if (m_printDialogData.GetFromPage() < m_printDialogData.GetMinPage())
            m_printDialogData.SetFromPage(m_printDialogData.GetMinPage());
        else if (m_printDialogData.GetFromPage() > m_printDialogData.GetMaxPage())
            m_printDialogData.SetFromPage(m_printDialogData.GetMaxPage());
        if (m_printDialogData.GetToPage() > m_printDialogData.GetMaxPage())
            m_printDialogData.SetToPage(m_printDialogData.GetMaxPage());
        else if (m_printDialogData.GetToPage() < m_printDialogData.GetMinPage())
            m_printDialogData.SetToPage(m_printDialogData.GetMinPage());
    }
    else
        m_printDialogData.EnablePageNumbers(FALSE);


    // Create a suitable device context
    wxDC *dc = (wxDC *) NULL;
    if (prompt)
    {
        dc = PrintDialog(parent);
        if (!dc)
            return FALSE;
    }
    else
    {
        dc = new wxPostScriptDC(GetPrintDialogData().GetPrintData());
    }

    // May have pressed cancel.
    if (!dc || !dc->Ok())
    {
        if (dc) delete dc;
        sm_lastError = wxPRINTER_ERROR;
        return FALSE;
    }

    wxSize ScreenPixels = wxGetDisplaySize();
    wxSize ScreenMM = wxGetDisplaySizeMM();

    printout->SetPPIScreen( (int) ((ScreenPixels.GetWidth() * 25.4) / ScreenMM.GetWidth()),
                            (int) ((ScreenPixels.GetHeight() * 25.4) / ScreenMM.GetHeight()) );
    printout->SetPPIPrinter( wxPostScriptDC::GetResolution(),
                             wxPostScriptDC::GetResolution() );

    // Set printout parameters
    printout->SetDC(dc);

    int w, h;
    dc->GetSize(&w, &h);
    printout->SetPageSizePixels((int)w, (int)h);
    dc->GetSizeMM(&w, &h);
    printout->SetPageSizeMM((int)w, (int)h);

    // Create an abort window
    wxBeginBusyCursor();

    printout->OnPreparePrinting();

    int
       pagesPerCopy = m_printDialogData.GetToPage()-m_printDialogData.GetFromPage()+1,
       totalPages = pagesPerCopy * m_printDialogData.GetNoCopies(),
       printedPages = 0;
    // Open the progress bar dialog
    wxProgressDialog *progressDialog = new wxProgressDialog (
       printout->GetTitle(),
       _("Printing..."),
       totalPages,
       parent,
       wxPD_CAN_ABORT|wxPD_AUTO_HIDE|wxPD_APP_MODAL);

    printout->OnBeginPrinting();

    sm_lastError = wxPRINTER_NO_ERROR;

    bool keepGoing = TRUE;

    int copyCount;
    for (copyCount = 1; copyCount <= m_printDialogData.GetNoCopies(); copyCount ++)
    {
        if (!printout->OnBeginDocument(m_printDialogData.GetFromPage(), m_printDialogData.GetToPage()))
        {
            wxEndBusyCursor();
            wxLogError(_("Could not start printing."));
            sm_lastError = wxPRINTER_ERROR;
            break;
        }
        if (sm_abortIt)
        {
            sm_lastError = wxPRINTER_CANCELLED;
            break;
        }

        int pn;
        for (pn = m_printDialogData.GetFromPage(); keepGoing && (pn <= m_printDialogData.GetToPage()) && printout->HasPage(pn);
        pn++)
        {
            if (sm_abortIt)
            {
                keepGoing = FALSE;
                sm_lastError = wxPRINTER_CANCELLED;
                break;
            }
            else
            {
               wxString msg;
               msg.Printf(_("Printing page %d..."), printedPages+1);
               if(progressDialog->Update(printedPages++, msg))
               {
                  dc->StartPage();
                  printout->OnPrintPage(pn);
                  dc->EndPage();
               }
               else
               {
                  sm_abortIt = TRUE;
                  sm_lastError = wxPRINTER_CANCELLED;
                  keepGoing = FALSE;
               }
            }
	    wxYield();
        }
        printout->OnEndDocument();
    }

    printout->OnEndPrinting();
    delete progressDialog;

    wxEndBusyCursor();

    delete dc;

    return (sm_lastError == wxPRINTER_NO_ERROR);
}

wxDC* wxPostScriptPrinter::PrintDialog(wxWindow *parent)
{
    wxDC* dc = (wxDC*) NULL;
    wxGenericPrintDialog* dialog = new wxGenericPrintDialog(parent, & m_printDialogData);
    int ret = dialog->ShowModal() ;
    if (ret == wxID_OK)
    {
        dc = dialog->GetPrintDC();
        m_printDialogData = dialog->GetPrintDialogData();
        if (dc == NULL)
            sm_lastError = wxPRINTER_ERROR;
        else
            sm_lastError = wxPRINTER_NO_ERROR;
    }
    else
        sm_lastError = wxPRINTER_CANCELLED;

    dialog->Destroy();

    return dc;
}

bool wxPostScriptPrinter::Setup(wxWindow *parent)
{
    wxGenericPrintDialog* dialog = new wxGenericPrintDialog(parent, & m_printDialogData);
    dialog->GetPrintDialogData().SetSetupDialog(TRUE);

    int ret = dialog->ShowModal();

    if (ret == wxID_OK)
    {
        m_printDialogData = dialog->GetPrintDialogData();
    }

    dialog->Destroy();

    return (ret == wxID_OK);
}

// ----------------------------------------------------------------------------
// Print preview
// ----------------------------------------------------------------------------

void wxPostScriptPrintPreview::Init(wxPrintout * WXUNUSED(printout),
                                    wxPrintout * WXUNUSED(printoutForPrinting))
{
    // Have to call it here since base constructor can't call it
    DetermineScaling();
}

wxPostScriptPrintPreview::wxPostScriptPrintPreview(wxPrintout *printout,
                                                   wxPrintout *printoutForPrinting,
                                                   wxPrintDialogData *data)
                        : wxPrintPreviewBase(printout, printoutForPrinting, data)
{
    Init(printout, printoutForPrinting);
}

wxPostScriptPrintPreview::wxPostScriptPrintPreview(wxPrintout *printout,
                                                   wxPrintout *printoutForPrinting,
                                                   wxPrintData *data)
                        : wxPrintPreviewBase(printout, printoutForPrinting, data)
{
    Init(printout, printoutForPrinting);
}

wxPostScriptPrintPreview::~wxPostScriptPrintPreview()
{
}

bool wxPostScriptPrintPreview::Print(bool interactive)
{
    if (!m_printPrintout)
        return FALSE;
    wxPostScriptPrinter printer(& m_printDialogData);
    return printer.Print(m_previewFrame, m_printPrintout, interactive);
}

void wxPostScriptPrintPreview::DetermineScaling()
{
    wxPaperSize paperType = m_printDialogData.GetPrintData().GetPaperId();
    if (paperType == wxPAPER_NONE)
        paperType = wxPAPER_NONE;

    wxPrintPaperType *paper = wxThePrintPaperDatabase->FindPaperType(paperType);
    if (!paper)
        paper = wxThePrintPaperDatabase->FindPaperType(wxPAPER_A4);

    if (paper)
    {
        wxSize ScreenPixels = wxGetDisplaySize();
        wxSize ScreenMM = wxGetDisplaySizeMM();

        m_previewPrintout->SetPPIScreen( (int) ((ScreenPixels.GetWidth() * 25.4) / ScreenMM.GetWidth()),
                                         (int) ((ScreenPixels.GetHeight() * 25.4) / ScreenMM.GetHeight()) );
        m_previewPrintout->SetPPIPrinter(wxPostScriptDC::GetResolution(), wxPostScriptDC::GetResolution()); 

        wxSize sizeDevUnits(paper->GetSizeDeviceUnits());
        sizeDevUnits.x = (wxCoord)((float)sizeDevUnits.x * wxPostScriptDC::GetResolution() / 72.0);
        sizeDevUnits.y = (wxCoord)((float)sizeDevUnits.y * wxPostScriptDC::GetResolution() / 72.0);
        wxSize sizeTenthsMM(paper->GetSize());
        wxSize sizeMM(sizeTenthsMM.x / 10, sizeTenthsMM.y / 10);

        // If in landscape mode, we need to swap the width and height.
        if ( m_printDialogData.GetPrintData().GetOrientation() == wxLANDSCAPE )
        {
            m_pageWidth = sizeDevUnits.y;
            m_pageHeight = sizeDevUnits.x;
            m_previewPrintout->SetPageSizeMM(sizeMM.y, sizeMM.x);
            m_previewPrintout->SetPageSizePixels(m_pageWidth, m_pageHeight);
        }
        else
        {
            m_pageWidth = sizeDevUnits.x;
            m_pageHeight = sizeDevUnits.y;
            m_previewPrintout->SetPageSizeMM(sizeMM.x, sizeMM.y);
            m_previewPrintout->SetPageSizePixels(m_pageWidth, m_pageHeight);
        }

        // At 100%, the page should look about page-size on the screen.
        m_previewScale = (float)0.8 * 72.0 / (float)wxPostScriptDC::GetResolution();
    }
}

#endif
