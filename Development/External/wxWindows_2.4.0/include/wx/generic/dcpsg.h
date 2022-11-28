/////////////////////////////////////////////////////////////////////////////
// Name:        dcps.h
// Purpose:     wxPostScriptDC class
// Author:      Julian Smart and others
// Modified by:
// RCS-ID:      $Id: dcpsg.h,v 1.21 2002/09/13 22:00:44 RR Exp $
// Copyright:   (c) Julian Smart, Robert Roebling and Markus Holzem
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DCPSG_H_
#define _WX_DCPSG_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "dcpsg.h"
#endif

#include "wx/dc.h"

#if wxUSE_PRINTING_ARCHITECTURE

#if wxUSE_POSTSCRIPT

#include "wx/dialog.h"
#include "wx/module.h"
#include "wx/cmndata.h"

//-----------------------------------------------------------------------------
// classes
//-----------------------------------------------------------------------------

class wxPostScriptDC;

//-----------------------------------------------------------------------------
// wxPostScriptDC
//-----------------------------------------------------------------------------

class WXDLLEXPORT wxPostScriptDC: public wxDC
{
public:
  wxPostScriptDC();

  // Recommended constructor
  wxPostScriptDC(const wxPrintData& printData);
  
  ~wxPostScriptDC();

#if WXWIN_COMPATIBILITY_2_2
  wxPostScriptDC( const wxString &output, bool interactive = FALSE, wxWindow *parent = NULL )
      { Create( output, interactive, parent ); }
  bool Create ( const wxString &output, bool interactive = FALSE, wxWindow *parent = NULL );
#endif

  virtual bool Ok() const;

  virtual void BeginDrawing() {}
  virtual void EndDrawing() {}

  bool DoFloodFill(wxCoord x1, wxCoord y1, const wxColour &col, int style=wxFLOOD_SURFACE );
  bool DoGetPixel(wxCoord x1, wxCoord y1, wxColour *col) const;

  void DoDrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2);
  void DoCrossHair(wxCoord x, wxCoord y) ;
  void DoDrawArc(wxCoord x1,wxCoord y1,wxCoord x2,wxCoord y2,wxCoord xc,wxCoord yc);
  void DoDrawEllipticArc(wxCoord x,wxCoord y,wxCoord w,wxCoord h,double sa,double ea);
  void DoDrawPoint(wxCoord x, wxCoord y);
  void DoDrawLines(int n, wxPoint points[], wxCoord xoffset = 0, wxCoord yoffset = 0);
  void DoDrawPolygon(int n, wxPoint points[], wxCoord xoffset = 0, wxCoord yoffset = 0, int fillStyle=wxODDEVEN_RULE);
  void DoDrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height);
  void DoDrawRoundedRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height, double radius = 20);
  void DoDrawEllipse(wxCoord x, wxCoord y, wxCoord width, wxCoord height);

  void DoDrawSpline(wxList *points);

  bool DoBlit(wxCoord xdest, wxCoord ydest, wxCoord width, wxCoord height,
            wxDC *source, wxCoord xsrc, wxCoord ysrc, int rop = wxCOPY, bool useMask = FALSE,
            wxCoord xsrcMask = -1, wxCoord ysrcMask = -1);
  bool CanDrawBitmap() const { return TRUE; }

  void DoDrawIcon( const wxIcon& icon, wxCoord x, wxCoord y );
  void DoDrawBitmap( const wxBitmap& bitmap, wxCoord x, wxCoord y, bool useMask=FALSE );

  void DoDrawText(const wxString& text, wxCoord x, wxCoord y );
  void DoDrawRotatedText(const wxString& text, wxCoord x, wxCoord y, double angle);

  void Clear();
  void SetFont( const wxFont& font );
  void SetPen( const wxPen& pen );
  void SetBrush( const wxBrush& brush );
  void SetLogicalFunction( int function );
  void SetBackground( const wxBrush& brush );

  void DoSetClippingRegion(wxCoord x, wxCoord y, wxCoord width, wxCoord height);
  void DestroyClippingRegion();

  void DoSetClippingRegionAsRegion( const wxRegion &WXUNUSED(clip) ) { }

  bool StartDoc(const wxString& message);
  void EndDoc();
  void StartPage();
  void EndPage();

  wxCoord GetCharHeight() const;
  wxCoord GetCharWidth() const;
  bool CanGetTextExtent() const { return TRUE; }
  void DoGetTextExtent(const wxString& string, wxCoord *x, wxCoord *y,
                     wxCoord *descent = (wxCoord *) NULL,
                     wxCoord *externalLeading = (wxCoord *) NULL,
                     wxFont *theFont = (wxFont *) NULL ) const;

  void DoGetSize(int* width, int* height) const;
  void DoGetSizeMM(int *width, int *height) const;

  // Resolution in pixels per logical inch
  wxSize GetPPI() const;

  void SetAxisOrientation( bool xLeftRight, bool yBottomUp );
  void SetDeviceOrigin( wxCoord x, wxCoord y );

  void SetBackgroundMode(int WXUNUSED(mode)) { }
  void SetPalette(const wxPalette& WXUNUSED(palette)) { }

  wxPrintData& GetPrintData() { return m_printData; }
  void SetPrintData(const wxPrintData& data) { m_printData = data; }

  virtual int GetDepth() const { return 24; }
  
  static void SetResolution(int ppi);
  static int GetResolution();
  
private:  
    static float ms_PSScaleFactor;

protected:
    FILE*             m_pstream;    // PostScript output stream
    wxString          m_title;
    unsigned char     m_currentRed;
    unsigned char     m_currentGreen;
    unsigned char     m_currentBlue;
    int               m_pageNumber;
    bool              m_clipping;
    double            m_underlinePosition;
    double            m_underlineThickness;
    wxPrintData       m_printData;
    
private:
    DECLARE_DYNAMIC_CLASS(wxPostScriptDC)
};


#if WXWIN_COMPATIBILITY_2_2
// Print Orientation
enum
{
    PS_PORTRAIT = wxPORTRAIT,
    PS_LANDSCAPE = wxLANDSCAPE
};

// Print Actions
enum
{
    PS_NONE = wxPRINT_MODE_NONE,
    PS_PREVIEW = wxPRINT_MODE_PREVIEW,
    PS_FILE = wxPRINT_MODE_FILE,
    PS_PRINTER = wxPRINT_MODE_PRINTER
};
    
class wxPrintSetupData: public wxPrintData
{
public:
    wxPrintSetupData() {}
    
    void SetPrinterOrientation( int orient ) 
        { SetOrientation( orient ); }
    void SetPrinterMode( wxPrintMode mode ) 
        { SetPrintMode( mode ); }
    void SetAFMPath( const wxString &path ) 
        { SetFontMetricPath( path ); }
    
    void SetPaperName(const wxString& paper) { m_paperName = paper; }
    void SetPrinterFile(const wxString& file) { m_printerFile = file; }
    wxString GetPaperName() const { return m_paperName; }
    wxString GetPrinterFile() const { return m_printerFile; };
    
    wxString        m_paperName;
    wxString        m_printerFile;
};

WXDLLEXPORT_DATA(extern wxPrintSetupData*) wxThePrintSetupData;
WXDLLEXPORT extern void wxInitializePrintSetupData(bool init = TRUE);
#endif


#endif
    // wxUSE_POSTSCRIPT

#endif
    // wxUSE_PRINTING_ARCHITECTURE

#endif
        // _WX_DCPSG_H_
