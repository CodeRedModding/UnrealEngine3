/////////////////////////////////////////////////////////////////////////////
// Name:        dcpsg.cpp
// Purpose:     Generic wxPostScriptDC implementation
// Author:      Julian Smart, Robert Roebling, Markus Holzhem
// Modified by:
// Created:     04/01/98
// RCS-ID:      $Id: dcpsg.cpp,v 1.88.2.3 2002/11/09 17:35:36 RR Exp $
// Copyright:   (c) Julian Smart and Markus Holzem
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#pragma implementation "dcpsg.h"
#endif

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
#endif // WX_PRECOMP

#if wxUSE_PRINTING_ARCHITECTURE

#if wxUSE_POSTSCRIPT

#include "wx/setup.h"

#include "wx/window.h"
#include "wx/dcmemory.h"
#include "wx/utils.h"
#include "wx/intl.h"
#include "wx/filedlg.h"
#include "wx/app.h"
#include "wx/msgdlg.h"
#include "wx/image.h"
#include "wx/log.h"
#include "wx/generic/dcpsg.h"
#include "wx/printdlg.h"
#include "wx/button.h"
#include "wx/stattext.h"
#include "wx/radiobox.h"
#include "wx/textctrl.h"
#include "wx/prntbase.h"
#include "wx/paper.h"
#include "wx/filefn.h"

#include <math.h>

#ifdef __WXMSW__

#ifdef DrawText
#undef DrawText
#endif

#ifdef StartDoc
#undef StartDoc
#endif

#ifdef GetCharWidth
#undef GetCharWidth
#endif

#ifdef FindWindow
#undef FindWindow
#endif

#endif

//-----------------------------------------------------------------------------
// start and end of document/page
//-----------------------------------------------------------------------------

static const char *wxPostScriptHeaderConicTo = "\
/conicto {\n\
    /to_y exch def\n\
    /to_x exch def\n\
    /conic_cntrl_y exch def\n\
    /conic_cntrl_x exch def\n\
    currentpoint\n\
    /p0_y exch def\n\
    /p0_x exch def\n\
    /p1_x p0_x conic_cntrl_x p0_x sub 2 3 div mul add def\n\
    /p1_y p0_y conic_cntrl_y p0_y sub 2 3 div mul add def\n\
    /p2_x p1_x to_x p0_x sub 1 3 div mul add def\n\
    /p2_y p1_y to_y p0_y sub 1 3 div mul add def\n\
    p1_x p1_y p2_x p2_y to_x to_y curveto\n\
}  bind def\n\
";
      
static const char *wxPostScriptHeaderEllipse = "\
/ellipsedict 8 dict def\n\
ellipsedict /mtrx matrix put\n\
/ellipse {\n\
    ellipsedict begin\n\
    /endangle exch def\n\
    /startangle exch def\n\
    /yrad exch def\n\
    /xrad exch def\n\
    /y exch def\n\
    /x exch def\n\
    /savematrix mtrx currentmatrix def\n\
    x y translate\n\
    xrad yrad scale\n\
    0 0 1 startangle endangle arc\n\
    savematrix setmatrix\n\
    end\n\
    } def\n\
";

static const char *wxPostScriptHeaderEllipticArc= "\
/ellipticarcdict 8 dict def\n\
ellipticarcdict /mtrx matrix put\n\
/ellipticarc\n\
{ ellipticarcdict begin\n\
  /do_fill exch def\n\
  /endangle exch def\n\
  /startangle exch def\n\
  /yrad exch def\n\
  /xrad exch def \n\
  /y exch def\n\
  /x exch def\n\
  /savematrix mtrx currentmatrix def\n\
  x y translate\n\
  xrad yrad scale\n\
  do_fill { 0 0 moveto } if\n\
  0 0 1 startangle endangle arc\n\
  savematrix setmatrix\n\
  do_fill { fill }{ stroke } ifelse\n\
  end\n\
} def\n";

static const char *wxPostScriptHeaderSpline = "\
/DrawSplineSection {\n\
    /y3 exch def\n\
    /x3 exch def\n\
    /y2 exch def\n\
    /x2 exch def\n\
    /y1 exch def\n\
    /x1 exch def\n\
    /xa x1 x2 x1 sub 0.666667 mul add def\n\
    /ya y1 y2 y1 sub 0.666667 mul add def\n\
    /xb x3 x2 x3 sub 0.666667 mul add def\n\
    /yb y3 y2 y3 sub 0.666667 mul add def\n\
    x1 y1 lineto\n\
    xa ya xb yb x3 y3 curveto\n\
    } def\n\
";

static const char *wxPostScriptHeaderColourImage = "\
%% define 'colorimage' if it isn't defined\n\
%%   ('colortogray' and 'mergeprocs' come from xwd2ps\n\
%%     via xgrab)\n\
/colorimage where   %% do we know about 'colorimage'?\n\
  { pop }           %% yes: pop off the 'dict' returned\n\
  {                 %% no:  define one\n\
    /colortogray {  %% define an RGB->I function\n\
      /rgbdata exch store    %% call input 'rgbdata'\n\
      rgbdata length 3 idiv\n\
      /npixls exch store\n\
      /rgbindx 0 store\n\
      0 1 npixls 1 sub {\n\
        grays exch\n\
        rgbdata rgbindx       get 20 mul    %% Red\n\
        rgbdata rgbindx 1 add get 32 mul    %% Green\n\
        rgbdata rgbindx 2 add get 12 mul    %% Blue\n\
        add add 64 idiv      %% I = .5G + .31R + .18B\n\
        put\n\
        /rgbindx rgbindx 3 add store\n\
      } for\n\
      grays 0 npixls getinterval\n\
    } bind def\n\
\n\
    %% Utility procedure for colorimage operator.\n\
    %% This procedure takes two procedures off the\n\
    %% stack and merges them into a single procedure.\n\
\n\
    /mergeprocs { %% def\n\
      dup length\n\
      3 -1 roll\n\
      dup\n\
      length\n\
      dup\n\
      5 1 roll\n\
      3 -1 roll\n\
      add\n\
      array cvx\n\
      dup\n\
      3 -1 roll\n\
      0 exch\n\
      putinterval\n\
      dup\n\
      4 2 roll\n\
      putinterval\n\
    } bind def\n\
\n\
    /colorimage { %% def\n\
      pop pop     %% remove 'false 3' operands\n\
      {colortogray} mergeprocs\n\
      image\n\
    } bind def\n\
  } ifelse          %% end of 'false' case\n\
";

#if wxUSE_PANGO
#else
static char wxPostScriptHeaderReencodeISO1[] =
    "\n/reencodeISO {\n"
"dup dup findfont dup length dict begin\n"
"{ 1 index /FID ne { def }{ pop pop } ifelse } forall\n"
"/Encoding ISOLatin1Encoding def\n"
"currentdict end definefont\n"
"} def\n"
"/ISOLatin1Encoding [\n"
"/.notdef/.notdef/.notdef/.notdef/.notdef/.notdef/.notdef/.notdef\n"
"/.notdef/.notdef/.notdef/.notdef/.notdef/.notdef/.notdef/.notdef\n"
"/.notdef/.notdef/.notdef/.notdef/.notdef/.notdef/.notdef/.notdef\n"
"/.notdef/.notdef/.notdef/.notdef/.notdef/.notdef/.notdef/.notdef\n"
"/space/exclam/quotedbl/numbersign/dollar/percent/ampersand/quoteright\n"
"/parenleft/parenright/asterisk/plus/comma/minus/period/slash\n"
"/zero/one/two/three/four/five/six/seven/eight/nine/colon/semicolon\n"
"/less/equal/greater/question/at/A/B/C/D/E/F/G/H/I/J/K/L/M/N\n"
"/O/P/Q/R/S/T/U/V/W/X/Y/Z/bracketleft/backslash/bracketright\n"
"/asciicircum/underscore/quoteleft/a/b/c/d/e/f/g/h/i/j/k/l/m\n"
"/n/o/p/q/r/s/t/u/v/w/x/y/z/braceleft/bar/braceright/asciitilde\n"
"/.notdef/.notdef/.notdef/.notdef/.notdef/.notdef/.notdef/.notdef\n"
"/.notdef/.notdef/.notdef/.notdef/.notdef/.notdef/.notdef/.notdef\n"
"/.notdef/dotlessi/grave/acute/circumflex/tilde/macron/breve\n"
"/dotaccent/dieresis/.notdef/ring/cedilla/.notdef/hungarumlaut\n";

static char wxPostScriptHeaderReencodeISO2[] =
"/ogonek/caron/space/exclamdown/cent/sterling/currency/yen/brokenbar\n"
"/section/dieresis/copyright/ordfeminine/guillemotleft/logicalnot\n"
"/hyphen/registered/macron/degree/plusminus/twosuperior/threesuperior\n"
"/acute/mu/paragraph/periodcentered/cedilla/onesuperior/ordmasculine\n"
"/guillemotright/onequarter/onehalf/threequarters/questiondown\n"
"/Agrave/Aacute/Acircumflex/Atilde/Adieresis/Aring/AE/Ccedilla\n"
"/Egrave/Eacute/Ecircumflex/Edieresis/Igrave/Iacute/Icircumflex\n"
"/Idieresis/Eth/Ntilde/Ograve/Oacute/Ocircumflex/Otilde/Odieresis\n"
"/multiply/Oslash/Ugrave/Uacute/Ucircumflex/Udieresis/Yacute\n"
"/Thorn/germandbls/agrave/aacute/acircumflex/atilde/adieresis\n"
"/aring/ae/ccedilla/egrave/eacute/ecircumflex/edieresis/igrave\n"
"/iacute/icircumflex/idieresis/eth/ntilde/ograve/oacute/ocircumflex\n"
"/otilde/odieresis/divide/oslash/ugrave/uacute/ucircumflex/udieresis\n"
"/yacute/thorn/ydieresis\n"
        "] def\n\n";
#endif

//-------------------------------------------------------------------------------
// wxPostScriptDC
//-------------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxPostScriptDC, wxDC)

float wxPostScriptDC::ms_PSScaleFactor = 10.0;

void wxPostScriptDC::SetResolution(int ppi)
{
    ms_PSScaleFactor = (float)ppi / 72.0;
}

int wxPostScriptDC::GetResolution()
{
    return (int)(ms_PSScaleFactor * 72.0);
}

//-------------------------------------------------------------------------------

wxPostScriptDC::wxPostScriptDC ()
{
    m_pstream = (FILE*) NULL;

    m_currentRed = 0;
    m_currentGreen = 0;
    m_currentBlue = 0;

    m_pageNumber = 0;

    m_clipping = FALSE;

    m_underlinePosition = 0.0;
    m_underlineThickness = 0.0;

    m_signX =  1;  // default x-axis left to right
    m_signY = -1;  // default y-axis bottom up -> top down
}

wxPostScriptDC::wxPostScriptDC (const wxPrintData& printData)
{
    m_pstream = (FILE*) NULL;

    m_currentRed = 0;
    m_currentGreen = 0;
    m_currentBlue = 0;

    m_pageNumber = 0;

    m_clipping = FALSE;

    m_underlinePosition = 0.0;
    m_underlineThickness = 0.0;

    m_signX =  1;  // default x-axis left to right
    m_signY = -1;  // default y-axis bottom up -> top down

    m_printData = printData;

    m_ok = TRUE;
}

wxPostScriptDC::~wxPostScriptDC ()
{
    if (m_pstream)
    {
        fclose( m_pstream );
        m_pstream = (FILE*) NULL;
    }
}

#if WXWIN_COMPATIBILITY_2_2
bool wxPostScriptDC::Create( const wxString &output, bool interactive, wxWindow *parent )
{
    wxPrintData data;
    data.SetFilename( output );
    data.SetPrintMode( wxPRINT_MODE_FILE );
    
    if (interactive)
    {
        wxPrintDialogData ddata( data );
        wxPrintDialog dialog( parent, &data );
        dialog.GetPrintDialogData().SetSetupDialog(TRUE);
        if (dialog.ShowModal() != wxID_OK)
        {
            m_ok = FALSE;
            return FALSE;
        }
        data = dialog.GetPrintDialogData().GetPrintData();
    }
    
    return TRUE;
}
#endif


bool wxPostScriptDC::Ok() const
{
  return m_ok;
}

void wxPostScriptDC::DoSetClippingRegion (wxCoord x, wxCoord y, wxCoord w, wxCoord h)
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    if (m_clipping) DestroyClippingRegion();

    wxDC::DoSetClippingRegion(x, y, w, h);

    m_clipping = TRUE;
    fprintf( m_pstream,
            "gsave\n newpath\n"
            "%d %d moveto\n"
            "%d %d lineto\n"
            "%d %d lineto\n"
            "%d %d lineto\n"
            "closepath clip newpath\n",
            LogicalToDeviceX(x),   LogicalToDeviceY(y),
            LogicalToDeviceX(x+w), LogicalToDeviceY(y),
            LogicalToDeviceX(x+w), LogicalToDeviceY(y+h),
            LogicalToDeviceX(x),   LogicalToDeviceY(y+h) );
}


void wxPostScriptDC::DestroyClippingRegion()
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    if (m_clipping)
    {
        m_clipping = FALSE;
        fprintf( m_pstream, "grestore\n" );
    }

    wxDC::DestroyClippingRegion();
}

void wxPostScriptDC::Clear()
{
    wxFAIL_MSG( wxT("wxPostScriptDC::Clear not implemented.") );
}

bool wxPostScriptDC::DoFloodFill (wxCoord WXUNUSED(x), wxCoord WXUNUSED(y), const wxColour &WXUNUSED(col), int WXUNUSED(style))
{
    wxFAIL_MSG( wxT("wxPostScriptDC::FloodFill not implemented.") );
    return FALSE;
}

bool wxPostScriptDC::DoGetPixel (wxCoord WXUNUSED(x), wxCoord WXUNUSED(y), wxColour * WXUNUSED(col)) const
{
    wxFAIL_MSG( wxT("wxPostScriptDC::GetPixel not implemented.") );
    return FALSE;
}

void wxPostScriptDC::DoCrossHair (wxCoord WXUNUSED(x), wxCoord WXUNUSED(y))
{
    wxFAIL_MSG( wxT("wxPostScriptDC::CrossHair not implemented.") );
}

void wxPostScriptDC::DoDrawLine (wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2)
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    if  (m_pen.GetStyle() == wxTRANSPARENT) return;

    SetPen( m_pen );

    fprintf( m_pstream,
            "newpath\n"
            "%d %d moveto\n"
            "%d %d lineto\n"
            "stroke\n",
            LogicalToDeviceX(x1), LogicalToDeviceY(y1),
            LogicalToDeviceX(x2), LogicalToDeviceY (y2) );

    CalcBoundingBox( x1, y1 );
    CalcBoundingBox( x2, y2 );
}

#define RAD2DEG 57.29577951308

void wxPostScriptDC::DoDrawArc (wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2, wxCoord xc, wxCoord yc)
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    wxCoord dx = x1 - xc;
    wxCoord dy = y1 - yc;
    wxCoord radius = (wxCoord) sqrt( (double)(dx*dx+dy*dy) );
    double alpha1, alpha2;

    if (x1 == x2 && y1 == y2)
    {
        alpha1 = 0.0;
        alpha2 = 360.0;
    }
    else if (radius == 0.0)
    {
        alpha1 = alpha2 = 0.0;
    }
    else
    {
        alpha1 = (x1 - xc == 0) ?
            (y1 - yc < 0) ? 90.0 : -90.0 :
                -atan2(double(y1-yc), double(x1-xc)) * RAD2DEG;
        alpha2 = (x2 - xc == 0) ?
            (y2 - yc < 0) ? 90.0 : -90.0 :
                -atan2(double(y2-yc), double(x2-xc)) * RAD2DEG;
    }
    while (alpha1 <= 0)   alpha1 += 360;
    while (alpha2 <= 0)   alpha2 += 360; // adjust angles to be between
    while (alpha1 > 360)  alpha1 -= 360; // 0 and 360 degree
    while (alpha2 > 360)  alpha2 -= 360;

    if (m_brush.GetStyle() != wxTRANSPARENT)
    {
        SetBrush( m_brush );

        fprintf( m_pstream,
                "newpath\n"
                "%d %d %d %d %d %d ellipse\n"
                "%d %d lineto\n"
                "closepath\n"
                "fill\n",
                LogicalToDeviceX(xc), LogicalToDeviceY(yc), LogicalToDeviceXRel(radius), LogicalToDeviceYRel(radius), (wxCoord)alpha1, (wxCoord) alpha2,
                LogicalToDeviceX(xc), LogicalToDeviceY(yc) );

        CalcBoundingBox( xc-radius, yc-radius );
        CalcBoundingBox( xc+radius, yc+radius );
    }

    if (m_pen.GetStyle() != wxTRANSPARENT)
    {
        SetPen( m_pen );

        fprintf( m_pstream,
                "newpath\n"
                "%d %d %d %d %d %d ellipse\n"
                "%d %d lineto\n"
                "stroke\n"
                "fill\n",
                LogicalToDeviceX(xc), LogicalToDeviceY(yc), LogicalToDeviceXRel(radius), LogicalToDeviceYRel(radius), (wxCoord)alpha1, (wxCoord) alpha2,
                LogicalToDeviceX(xc), LogicalToDeviceY(yc) );

        CalcBoundingBox( xc-radius, yc-radius );
        CalcBoundingBox( xc+radius, yc+radius );
    }
}

void wxPostScriptDC::DoDrawEllipticArc(wxCoord x,wxCoord y,wxCoord w,wxCoord h,double sa,double ea)
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    if (sa>=360 || sa<=-360) sa=sa-int(sa/360)*360;
    if (ea>=360 || ea<=-360) ea=ea-int(ea/360)*360;
    if (sa<0) sa+=360;
    if (ea<0) ea+=360;

    if (sa==ea)
    {
        DrawEllipse(x,y,w,h);
        return;
    }

    if (m_brush.GetStyle () != wxTRANSPARENT)
    {
        SetBrush( m_brush );

        fprintf( m_pstream,
                "newpath\n"
                "%d %d %d %d %d %d true ellipticarc\n",
                LogicalToDeviceX(x+w/2), LogicalToDeviceY(y+h/2), LogicalToDeviceXRel(w/2), LogicalToDeviceYRel(h/2), (wxCoord)sa, (wxCoord)ea );

        CalcBoundingBox( x ,y );
        CalcBoundingBox( x+w, y+h );
    }

    if (m_pen.GetStyle () != wxTRANSPARENT)
    {
        SetPen( m_pen );

        fprintf(m_pstream,
                "newpath\n"
                "%d %d %d %d %d %d false ellipticarc\n",
                LogicalToDeviceX(x+w/2), LogicalToDeviceY(y+h/2), LogicalToDeviceXRel(w/2), LogicalToDeviceYRel(h/2), (wxCoord)sa, (wxCoord)ea );

        CalcBoundingBox( x ,y );
        CalcBoundingBox( x+w, y+h );
    }
}

void wxPostScriptDC::DoDrawPoint (wxCoord x, wxCoord y)
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    if (m_pen.GetStyle() == wxTRANSPARENT) return;

    SetPen (m_pen);

    fprintf( m_pstream,
            "newpath\n"
            "%d %d moveto\n"
            "%d %d lineto\n"
            "stroke\n",
            LogicalToDeviceX(x),   LogicalToDeviceY(y),
            LogicalToDeviceX(x+1), LogicalToDeviceY(y) );

    CalcBoundingBox( x, y );
}

void wxPostScriptDC::DoDrawPolygon (int n, wxPoint points[], wxCoord xoffset, wxCoord yoffset, int WXUNUSED(fillStyle))
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    if (n <= 0) return;

    if (m_brush.GetStyle () != wxTRANSPARENT)
    {
        SetBrush( m_brush );

        fprintf( m_pstream, "newpath\n" );

        wxCoord xx = LogicalToDeviceX(points[0].x + xoffset);
        wxCoord yy = LogicalToDeviceY(points[0].y + yoffset);

        fprintf( m_pstream, "%d %d moveto\n", xx, yy );

        CalcBoundingBox( points[0].x + xoffset, points[0].y + yoffset );

        for (int i = 1; i < n; i++)
        {
            xx = LogicalToDeviceX(points[i].x + xoffset);
            yy = LogicalToDeviceY(points[i].y + yoffset);

            fprintf( m_pstream, "%d %d lineto\n", xx, yy );

            CalcBoundingBox( points[i].x + xoffset, points[i].y + yoffset);
        }

        fprintf( m_pstream, "fill\n" );
    }

    if (m_pen.GetStyle () != wxTRANSPARENT)
    {
        SetPen( m_pen );

        fprintf( m_pstream, "newpath\n" );

        wxCoord xx = LogicalToDeviceX(points[0].x + xoffset);
        wxCoord yy = LogicalToDeviceY(points[0].y + yoffset);

        fprintf( m_pstream, "%d %d moveto\n", xx, yy );

        CalcBoundingBox( points[0].x + xoffset, points[0].y + yoffset );

        for (int i = 1; i < n; i++)
        {
            xx = LogicalToDeviceX(points[i].x + xoffset);
            yy = LogicalToDeviceY(points[i].y + yoffset);

            fprintf( m_pstream, "%d %d lineto\n", xx, yy );

            CalcBoundingBox( points[i].x + xoffset, points[i].y + yoffset);
        }

        fprintf( m_pstream, "closepath\n" );
        fprintf( m_pstream, "stroke\n" );
    }
}

void wxPostScriptDC::DoDrawLines (int n, wxPoint points[], wxCoord xoffset, wxCoord yoffset)
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    if (m_pen.GetStyle() == wxTRANSPARENT) return;

    if (n <= 0) return;

    SetPen (m_pen);

    int i;
    for ( i =0; i<n ; i++ )
    {
        CalcBoundingBox( LogicalToDeviceX(points[i].x+xoffset), LogicalToDeviceY(points[i].y+yoffset));
    }

    fprintf( m_pstream,
            "newpath\n"
            "%d %d moveto\n",
            LogicalToDeviceX(points[0].x+xoffset), LogicalToDeviceY(points[0].y+yoffset) );

    for (i = 1; i < n; i++)
    {
        fprintf( m_pstream,
                "%d %d lineto\n",
                LogicalToDeviceX(points[i].x+xoffset), LogicalToDeviceY(points[i].y+yoffset) );
    }

    fprintf( m_pstream, "stroke\n" );
}

void wxPostScriptDC::DoDrawRectangle (wxCoord x, wxCoord y, wxCoord width, wxCoord height)
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    if (m_brush.GetStyle () != wxTRANSPARENT)
    {
        SetBrush( m_brush );

        fprintf( m_pstream,
                "newpath\n"
                "%d %d moveto\n"
                "%d %d lineto\n"
                "%d %d lineto\n"
                "%d %d lineto\n"
                "closepath\n"
                "fill\n",
                LogicalToDeviceX(x),         LogicalToDeviceY(y),
                LogicalToDeviceX(x + width), LogicalToDeviceY(y),
                LogicalToDeviceX(x + width), LogicalToDeviceY(y + height),
                LogicalToDeviceX(x),         LogicalToDeviceY(y + height) );

        CalcBoundingBox( x, y );
        CalcBoundingBox( x + width, y + height );
    }

    if (m_pen.GetStyle () != wxTRANSPARENT)
    {
        SetPen (m_pen);

        fprintf( m_pstream,
                "newpath\n"
                "%d %d moveto\n"
                "%d %d lineto\n"
                "%d %d lineto\n"
                "%d %d lineto\n"
                "closepath\n"
                "stroke\n",
                LogicalToDeviceX(x),         LogicalToDeviceY(y),
                LogicalToDeviceX(x + width), LogicalToDeviceY(y),
                LogicalToDeviceX(x + width), LogicalToDeviceY(y + height),
                LogicalToDeviceX(x),         LogicalToDeviceY(y + height) );

        CalcBoundingBox( x, y );
        CalcBoundingBox( x + width, y + height );
    }
}

void wxPostScriptDC::DoDrawRoundedRectangle (wxCoord x, wxCoord y, wxCoord width, wxCoord height, double radius)
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    if (radius < 0.0)
    {
        // Now, a negative radius is interpreted to mean
        // 'the proportion of the smallest X or Y dimension'
        double smallest = 0.0;
        if (width < height)
        smallest = width;
        else
        smallest = height;
        radius =  (-radius * smallest);
    }

    wxCoord rad = (wxCoord) radius;

    if (m_brush.GetStyle () != wxTRANSPARENT)
    {
        SetBrush( m_brush );

        /* Draw rectangle anticlockwise */
        fprintf( m_pstream,
                "newpath\n"
                "%d %d %d 90 180 arc\n"
                "%d %d moveto\n"
                "%d %d %d 180 270 arc\n"
                "%d %d lineto\n"
                "%d %d %d 270 0 arc\n"
                "%d %d lineto\n"
                "%d %d %d 0 90 arc\n"
                "%d %d lineto\n"
                "closepath\n"
                "fill\n",
                LogicalToDeviceX(x + rad), LogicalToDeviceY(y + rad), LogicalToDeviceXRel(rad),
                LogicalToDeviceX(x), LogicalToDeviceY(y + rad),
                LogicalToDeviceX(x + rad), LogicalToDeviceY(y + height - rad), LogicalToDeviceXRel(rad),
                LogicalToDeviceX(x + width - rad), LogicalToDeviceY(y + height),
                LogicalToDeviceX(x + width - rad), LogicalToDeviceY(y + height - rad), LogicalToDeviceXRel(rad),
                LogicalToDeviceX(x + width), LogicalToDeviceY(y + rad),
                LogicalToDeviceX(x + width - rad), LogicalToDeviceY(y + rad), LogicalToDeviceXRel(rad),
                LogicalToDeviceX(x + rad), LogicalToDeviceY(y) );

        CalcBoundingBox( x, y );
        CalcBoundingBox( x + width, y + height );
    }

    if (m_pen.GetStyle () != wxTRANSPARENT)
    {
        SetPen (m_pen);

        /* Draw rectangle anticlockwise */
        fprintf( m_pstream,
                "newpath\n"
                "%d %d %d 90 180 arc\n"
                "%d %d moveto\n"
                "%d %d %d 180 270 arc\n"
                "%d %d lineto\n"
                "%d %d %d 270 0 arc\n"
                "%d %d lineto\n"
                "%d %d %d 0 90 arc\n"
                "%d %d lineto\n"
                "closepath\n"
                "stroke\n",
                LogicalToDeviceX(x + rad), LogicalToDeviceY(y + rad), LogicalToDeviceXRel(rad),
                LogicalToDeviceX(x), LogicalToDeviceY(y + rad),
                LogicalToDeviceX(x + rad), LogicalToDeviceY(y + height - rad), LogicalToDeviceXRel(rad),
                LogicalToDeviceX(x + width - rad), LogicalToDeviceY(y + height),
                LogicalToDeviceX(x + width - rad), LogicalToDeviceY(y + height - rad), LogicalToDeviceXRel(rad),
                LogicalToDeviceX(x + width), LogicalToDeviceY(y + rad),
                LogicalToDeviceX(x + width - rad), LogicalToDeviceY(y + rad), LogicalToDeviceXRel(rad),
                LogicalToDeviceX(x + rad), LogicalToDeviceY(y) );

        CalcBoundingBox( x, y );
        CalcBoundingBox( x + width, y + height );
    }
}

void wxPostScriptDC::DoDrawEllipse (wxCoord x, wxCoord y, wxCoord width, wxCoord height)
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    if (m_brush.GetStyle () != wxTRANSPARENT)
    {
        SetBrush (m_brush);

        fprintf( m_pstream,
                "newpath\n"
                "%d %d %d %d 0 360 ellipse\n"
                "fill\n",
                LogicalToDeviceX(x + width / 2), LogicalToDeviceY(y + height / 2),
                LogicalToDeviceXRel(width / 2), LogicalToDeviceYRel(height / 2) );

        CalcBoundingBox( x - width, y - height );
        CalcBoundingBox( x + width, y + height );
    }

    if (m_pen.GetStyle () != wxTRANSPARENT)
    {
        SetPen (m_pen);

        fprintf( m_pstream,
                "newpath\n"
                "%d %d %d %d 0 360 ellipse\n"
                "stroke\n",
                LogicalToDeviceX(x + width / 2), LogicalToDeviceY(y + height / 2),
                LogicalToDeviceXRel(width / 2), LogicalToDeviceYRel(height / 2) );

        CalcBoundingBox( x - width, y - height );
        CalcBoundingBox( x + width, y + height );
    }
}

void wxPostScriptDC::DoDrawIcon( const wxIcon& icon, wxCoord x, wxCoord y )
{
    DrawBitmap( icon, x, y, TRUE );
}

/* this has to be char, not wxChar */
static char hexArray[] = "0123456789ABCDEF";
static void LocalDecToHex( int dec, char *buf )
{
    int firstDigit = (int)(dec/16.0);
    int secondDigit = (int)(dec - (firstDigit*16.0));
    buf[0] = hexArray[firstDigit];
    buf[1] = hexArray[secondDigit];
    buf[2] = 0;
}

void wxPostScriptDC::DoDrawBitmap( const wxBitmap& bitmap, wxCoord x, wxCoord y, bool WXUNUSED(useMask) )
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    if (!bitmap.Ok()) return;

    wxImage image = bitmap.ConvertToImage();

    if (!image.Ok()) return;

    wxCoord w = image.GetWidth();
    wxCoord h = image.GetHeight();

    wxCoord ww = LogicalToDeviceXRel(image.GetWidth());
    wxCoord hh = LogicalToDeviceYRel(image.GetHeight());

    wxCoord xx = LogicalToDeviceX(x);
    wxCoord yy = LogicalToDeviceY(y + bitmap.GetHeight());

    fprintf( m_pstream,
            "/origstate save def\n"
            "20 dict begin\n"
            "/pix %d string def\n"
            "/grays %d string def\n"
            "/npixels 0 def\n"
            "/rgbindx 0 def\n"
            "%d %d translate\n"
            "%d %d scale\n"
            "%d %d 8\n"
            "[%d 0 0 %d 0 %d]\n"
            "{currentfile pix readhexstring pop}\n"
            "false 3 colorimage\n",
            w, w, xx, yy, ww, hh, w, h, w, -h, h );


    for (int j = 0; j < h; j++)
    {
        for (int i = 0; i < w; i++)
        {
            char buffer[5];
            LocalDecToHex( image.GetRed(i,j), buffer );
            fprintf( m_pstream, buffer );
            LocalDecToHex( image.GetGreen(i,j), buffer );
            fprintf( m_pstream, buffer );
            LocalDecToHex( image.GetBlue(i,j), buffer );
            fprintf( m_pstream, buffer );
        }
        fprintf( m_pstream, "\n" );
    }

    fprintf( m_pstream, "end\n" );
    fprintf( m_pstream, "origstate restore\n" );
}

void wxPostScriptDC::SetFont( const wxFont& font )
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    if (!font.Ok())  return;

    m_font = font;

#if wxUSE_PANGO
#else
    int Style = m_font.GetStyle();
    int Weight = m_font.GetWeight();

    const char *name;
    switch (m_font.GetFamily())
    {
        case wxTELETYPE:
        case wxMODERN:
        {
            if (Style == wxITALIC)
            {
                if (Weight == wxBOLD)
                    name = "/Courier-BoldOblique";
                else
                    name = "/Courier-Oblique";
            }
            else
            {
                if (Weight == wxBOLD)
                    name = "/Courier-Bold";
                else
                    name = "/Courier";
            }
            break;
        }
        case wxROMAN:
        {
            if (Style == wxITALIC)
            {
                if (Weight == wxBOLD)
                    name = "/Times-BoldItalic";
                else
                    name = "/Times-Italic";
            }
            else
            {
                if (Weight == wxBOLD)
                    name = "/Times-Bold";
                else
                    name = "/Times-Roman";
            }
            break;
        }
        case wxSCRIPT:
        {
            name = "/ZapfChancery-MediumItalic";
            Style  = wxNORMAL;
            Weight = wxNORMAL;
            break;
        }
        case wxSWISS:
        default:
        {
            if (Style == wxITALIC)
            {
                if (Weight == wxBOLD)
                    name = "/Helvetica-BoldOblique";
                else
                    name = "/Helvetica-Oblique";
            }
            else
            {
                if (Weight == wxBOLD)
                    name = "/Helvetica-Bold";
                else
                    name = "/Helvetica";
            }
            break;
        }
    }

    fprintf( m_pstream, name );
    fprintf( m_pstream, " reencodeISO def\n" );
    fprintf( m_pstream, name );
    fprintf( m_pstream, " findfont\n" );

    char buffer[100];
    sprintf( buffer, "%f scalefont setfont\n", LogicalToDeviceYRel(m_font.GetPointSize() * 1000) / 1000.0F);
                // this is a hack - we must scale font size (in pts) according to m_scaleY but
                // LogicalToDeviceYRel works with wxCoord type (int or longint). Se we first convert font size
                // to 1/1000th of pt and then back.
    for (int i = 0; i < 100; i++)
        if (buffer[i] == ',') buffer[i] = '.';
    fprintf( m_pstream, buffer );
#endif
}

void wxPostScriptDC::SetPen( const wxPen& pen )
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    if (!pen.Ok()) return;

    int oldStyle = m_pen.GetStyle();

    m_pen = pen;

    {
        char buffer[100];
        #ifdef __WXMSW__
        sprintf( buffer, "%f setlinewidth\n", LogicalToDeviceXRel(1000 * m_pen.GetWidth()) / 1000.0f );
        #else
        sprintf( buffer, "%f setlinewidth\n", LogicalToDeviceXRel(1000 * m_pen.GetWidth()) / 1000.0f );
        #endif
        for (int i = 0; i < 100; i++)
            if (buffer[i] == ',') buffer[i] = '.';
        fprintf( m_pstream, buffer );
    }

/*
     Line style - WRONG: 2nd arg is OFFSET

     Here, I'm afraid you do not conceive meaning of parameters of 'setdash'
     operator correctly. You should look-up this in the Red Book: the 2nd parame-
     ter is not number of values in the array of the first one, but an offset
     into this description of the pattern. I mean a real *offset* not index
     into array. I.e. If the command is [3 4] 1 setdash   is used, then there
     will be first black line *2* units wxCoord, then space 4 units, then the
     pattern of *3* units black, 4 units space will be repeated.
*/

    static const char *dotted = "[2 5] 2";
    static const char *short_dashed = "[4 4] 2";
    static const char *wxCoord_dashed = "[4 8] 2";
    static const char *dotted_dashed = "[6 6 2 6] 4";

    const char *psdash = (char *) NULL;
    switch (m_pen.GetStyle())
    {
        case wxDOT:           psdash = dotted;         break;
        case wxSHORT_DASH:    psdash = short_dashed;   break;
        case wxLONG_DASH:     psdash = wxCoord_dashed;    break;
        case wxDOT_DASH:      psdash = dotted_dashed;  break;
        case wxSOLID:
        case wxTRANSPARENT:
        default:              psdash = "[] 0";         break;
    }

    if (oldStyle != m_pen.GetStyle())
    {
        fprintf( m_pstream, psdash );
        fprintf( m_pstream," setdash\n" );
    }

    // Line colour
    unsigned char red = m_pen.GetColour().Red();
    unsigned char blue = m_pen.GetColour().Blue();
    unsigned char green = m_pen.GetColour().Green();

    if (!m_colour)
    {
        // Anything not white is black
        if (! (red == (unsigned char) 255 &&
               blue == (unsigned char) 255 &&
               green == (unsigned char) 255) )
        {
            red = (unsigned char) 0;
            green = (unsigned char) 0;
            blue = (unsigned char) 0;
        }
        // setgray here ?
    }

    if (!(red == m_currentRed && green == m_currentGreen && blue == m_currentBlue))
    {
        double redPS = (double)(red) / 255.0;
        double bluePS = (double)(blue) / 255.0;
        double greenPS = (double)(green) / 255.0;

        char buffer[100];
        sprintf( buffer,
                "%.8f %.8f %.8f setrgbcolor\n",
                redPS, greenPS, bluePS );
        for (int i = 0; i < 100; i++)
            if (buffer[i] == ',') buffer[i] = '.';
        fprintf( m_pstream, buffer );

        m_currentRed = red;
        m_currentBlue = blue;
        m_currentGreen = green;
    }
}

void wxPostScriptDC::SetBrush( const wxBrush& brush )
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    if (!brush.Ok()) return;

    m_brush = brush;

    // Brush colour
    unsigned char red = m_brush.GetColour().Red();
    unsigned char blue = m_brush.GetColour().Blue();
    unsigned char green = m_brush.GetColour().Green();

    if (!m_colour)
    {
        // Anything not white is black
        if (! (red == (unsigned char) 255 &&
               blue == (unsigned char) 255 &&
               green == (unsigned char) 255) )
        {
            red = (unsigned char) 0;
            green = (unsigned char) 0;
            blue = (unsigned char) 0;
        }
        // setgray here ?
    }

    if (!(red == m_currentRed && green == m_currentGreen && blue == m_currentBlue))
    {
        double redPS = (double)(red) / 255.0;
        double bluePS = (double)(blue) / 255.0;
        double greenPS = (double)(green) / 255.0;

        char buffer[100];
        sprintf( buffer,
                "%.8f %.8f %.8f setrgbcolor\n",
                redPS, greenPS, bluePS );
        for (int i = 0; i < 100; i++)
            if (buffer[i] == ',') buffer[i] = '.';
        fprintf( m_pstream, buffer );

        m_currentRed = red;
        m_currentBlue = blue;
        m_currentGreen = green;
    }
}

#if wxUSE_PANGO

#define PANGO_ENABLE_ENGINE

#ifdef __WXGTK20__
#include "wx/gtk/private.h"
#include "gtk/gtk.h"
#else
#include "wx/x11/private.h"
#endif

#include "wx/fontutil.h"
#include <pango/pangoft2.h>
#include <freetype/ftglyph.h>

#ifndef FT_Outline_Decompose
  FT_EXPORT( FT_Error )  FT_Outline_Decompose(
                           FT_Outline*              outline,
                           const FT_Outline_Funcs*  interface,
                           void*                    user );
#endif

typedef struct _OutlineInfo OutlineInfo;
struct _OutlineInfo {
  FILE *file;
};

static int paps_move_to( FT_Vector* to,
			 void *user_data)
{
  OutlineInfo *outline_info = (OutlineInfo*)user_data;
  fprintf(outline_info->file, "%d %d moveto\n",
	  (int)to->x ,
	  (int)to->y );
  return 0;
}

static int paps_line_to( FT_Vector*  to,
			 void *user_data)
{
  OutlineInfo *outline_info = (OutlineInfo*)user_data;
  fprintf(outline_info->file, "%d %d lineto\n",
	  (int)to->x ,
	  (int)to->y );
  return 0;
}

static int paps_conic_to( FT_Vector*  control,
			  FT_Vector*  to,
			  void *user_data)
{
  OutlineInfo *outline_info = (OutlineInfo*)user_data;
  fprintf(outline_info->file, "%d %d %d %d conicto\n",
	  (int)control->x  ,
	  (int)control->y  ,
	  (int)to->x   ,
	  (int)to->y  );
  return 0;
}

static int paps_cubic_to( FT_Vector*  control1,
			  FT_Vector*  control2,
			  FT_Vector*  to,
			  void *user_data)
{
  OutlineInfo *outline_info = (OutlineInfo*)user_data;
  fprintf(outline_info->file,
	  "%d %d %d %d %d %d curveto\n",
	  (int)control1->x , 
	  (int)control1->y ,
	  (int)control2->x ,
	  (int)control2->y ,
	  (int)to->x ,
	  (int)to->y );
  return 0;
}

void draw_bezier_outline(FILE *file,
			 FT_Face face,
			 FT_UInt glyph_index,
			 int pos_x,
			 int pos_y,
             int scale_x,
             int scale_y )
{
  FT_Int load_flags = FT_LOAD_NO_BITMAP;
  FT_Glyph glyph;

  FT_Outline_Funcs outlinefunc = 
  {
    paps_move_to,
    paps_line_to,
    paps_conic_to,
    paps_cubic_to
  };
  
  OutlineInfo outline_info;
  outline_info.file = file;

  fprintf(file, "gsave\n");
  fprintf(file, "%d %d translate\n", pos_x, pos_y );
  // FT2 scales outlines to 26.6 pixels so the code below
  // should read 26600 instead of the 60000.
  fprintf(file, "%d 60000 div %d 60000 div scale\n", scale_x, scale_y );
  fprintf(file, "0 0 0 setrgbcolor\n");

  FT_Load_Glyph(face, glyph_index, load_flags);
  FT_Get_Glyph (face->glyph, &glyph);
  FT_Outline_Decompose (&(((FT_OutlineGlyph)glyph)->outline),
                        &outlinefunc, &outline_info);
  fprintf(file, "closepath fill grestore \n");
  
  FT_Done_Glyph (glyph);
}

#endif

void wxPostScriptDC::DoDrawText( const wxString& text, wxCoord x, wxCoord y )
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

#if wxUSE_PANGO
    int dpi = GetResolution();
    dpi = 300;
    PangoContext *context = pango_ft2_get_context ( dpi, dpi );

    pango_context_set_language (context, pango_language_from_string ("en_US"));
    pango_context_set_base_dir (context, PANGO_DIRECTION_LTR );

    pango_context_set_font_description (context, m_font.GetNativeFontInfo()->description );

    PangoLayout *layout = pango_layout_new (context);
#if wxUSE_UNICODE
    wxCharBuffer buffer = wxConvUTF8.cWC2MB( text );
#else
    wxCharBuffer buffer = wxConvUTF8.cWC2MB( wxConvLocal.cWX2WC( text ) );
#endif
	pango_layout_set_text( layout, (const char*) buffer, strlen(buffer) );

    PangoRectangle rect;
    pango_layout_get_extents(layout, NULL, &rect);
    
    int xx = x * PANGO_SCALE;
    int yy = y * PANGO_SCALE + (rect.height*2/3);
    
    int scale_x = LogicalToDeviceXRel( 1000 );
    int scale_y = LogicalToDeviceYRel( 1000 );
    
    // Loop over lines in layout
    int num_lines = pango_layout_get_line_count( layout );
    for (int i = 0; i < num_lines; i++)
    {
        PangoLayoutLine *line = pango_layout_get_line( layout, i );
        
        // Loop over runs in line
        GSList *runs_list = line->runs;
        while (runs_list)
        {
            PangoLayoutRun *run = (PangoLayoutRun*) runs_list->data;
            PangoItem *item = run->item;
            PangoGlyphString *glyphs = run->glyphs;
            PangoAnalysis *analysis = &item->analysis;
            PangoFont *font = analysis->font;
            FT_Face ft_face = pango_ft2_font_get_face(font);
            
            int num_glyphs = glyphs->num_glyphs;
            for (int glyph_idx = 0; glyph_idx < num_glyphs; glyph_idx++)
            {
                PangoGlyphGeometry geometry = glyphs->glyphs[glyph_idx].geometry;
                int pos_x = xx + geometry.x_offset;
                int pos_y = yy - geometry.y_offset;
                xx += geometry.width;
                
                draw_bezier_outline( m_pstream, ft_face,
			      (FT_UInt)(glyphs->glyphs[glyph_idx].glyph),
			      LogicalToDeviceX( pos_x / PANGO_SCALE ), 
                  LogicalToDeviceY( pos_y / PANGO_SCALE ),
                  scale_x, scale_y );
            }
            runs_list = runs_list->next;
        }
	}

    g_object_unref( G_OBJECT( layout ) );
#else
    wxCoord text_w, text_h, text_descent;

    GetTextExtent(text, &text_w, &text_h, &text_descent);

    // VZ: this seems to be unnecessary, so taking it out for now, if it
    //     doesn't create any problems, remove this comment entirely
    //SetFont( m_font );

    if (m_textForegroundColour.Ok())
    {
        unsigned char red = m_textForegroundColour.Red();
        unsigned char blue = m_textForegroundColour.Blue();
        unsigned char green = m_textForegroundColour.Green();

        if (!m_colour)
        {
            // Anything not white is black
            if (! (red == (unsigned char) 255 &&
                        blue == (unsigned char) 255 &&
                        green == (unsigned char) 255))
            {
                red = (unsigned char) 0;
                green = (unsigned char) 0;
                blue = (unsigned char) 0;
            }
        }

        // maybe setgray here ?
        if (!(red == m_currentRed && green == m_currentGreen && blue == m_currentBlue))
        {
            double redPS = (double)(red) / 255.0;
            double bluePS = (double)(blue) / 255.0;
            double greenPS = (double)(green) / 255.0;

            char buffer[100];
            sprintf( buffer,
                "%.8f %.8f %.8f setrgbcolor\n",
                redPS, greenPS, bluePS );
            for (int i = 0; i < 100; i++)
                if (buffer[i] == ',') buffer[i] = '.';
            fprintf( m_pstream, buffer );

            m_currentRed = red;
            m_currentBlue = blue;
            m_currentGreen = green;
        }
    }

    int size = m_font.GetPointSize();

//    wxCoord by = y + (wxCoord)floor( double(size) * 2.0 / 3.0 ); // approximate baseline
//    commented by V. Slavik and replaced by accurate version
//        - note that there is still rounding error in text_descent!
    wxCoord by = y + size - text_descent; // baseline
    fprintf( m_pstream, "%d %d moveto\n", LogicalToDeviceX(x), LogicalToDeviceY(by) );

    fprintf( m_pstream, "(" );
    const wxWX2MBbuf textbuf = text.mb_str();
    size_t len = strlen(textbuf);
    size_t i;
    for (i = 0; i < len; i++)
    {
        int c = (unsigned char) textbuf[i];
        if (c == ')' || c == '(' || c == '\\')
        {
            /* Cope with special characters */
            fprintf( m_pstream, "\\" );
            fputc(c, m_pstream);
        }
        else if ( c >= 128 )
        {
            /* Cope with character codes > 127 */
            fprintf(m_pstream, "\\%o", c);
        }
        else
        {
            fputc(c, m_pstream);
        }
    }

    fprintf( m_pstream, ") show\n" );

    if (m_font.GetUnderlined())
    {
        wxCoord uy = (wxCoord)(y + size - m_underlinePosition);
        char buffer[100];

        sprintf( buffer,
                "gsave\n"
                "%d %d moveto\n"
                "%f setlinewidth\n"
                "%d %d lineto\n"
                "stroke\n"
                "grestore\n",
                LogicalToDeviceX(x), LogicalToDeviceY(uy),
                m_underlineThickness,
                LogicalToDeviceX(x + text_w), LogicalToDeviceY(uy) );
        for (i = 0; i < 100; i++)
            if (buffer[i] == ',') buffer[i] = '.';
        fprintf( m_pstream, buffer );
    }

    CalcBoundingBox( x, y );
    CalcBoundingBox( x + size * text.Length() * 2/3 , y );
#endif
}

void wxPostScriptDC::DoDrawRotatedText( const wxString& text, wxCoord x, wxCoord y, double angle )
{
    if (angle == 0.0)
    {
        DoDrawText(text, x, y);
        return;
    }

    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    SetFont( m_font );

    if (m_textForegroundColour.Ok())
    {
        unsigned char red = m_textForegroundColour.Red();
        unsigned char blue = m_textForegroundColour.Blue();
        unsigned char green = m_textForegroundColour.Green();

        if (!m_colour)
        {
            // Anything not white is black
            if (! (red == (unsigned char) 255 &&
                   blue == (unsigned char) 255 &&
                   green == (unsigned char) 255))
            {
                red = (unsigned char) 0;
                green = (unsigned char) 0;
                blue = (unsigned char) 0;
            }
        }

        // maybe setgray here ?
        if (!(red == m_currentRed && green == m_currentGreen && blue == m_currentBlue))
        {
            double redPS = (double)(red) / 255.0;
            double bluePS = (double)(blue) / 255.0;
            double greenPS = (double)(green) / 255.0;

            char buffer[100];
            sprintf( buffer,
                "%.8f %.8f %.8f setrgbcolor\n",
                redPS, greenPS, bluePS );
            for (int i = 0; i < 100; i++)
                if (buffer[i] == ',') buffer[i] = '.';
            fprintf( m_pstream, buffer );

            m_currentRed = red;
            m_currentBlue = blue;
            m_currentGreen = green;
        }
    }

    int size = m_font.GetPointSize();

    long by = y + (long)floor( double(size) * 2.0 / 3.0 ); // approximate baseline

    // FIXME only correct for 90 degrees
    fprintf(m_pstream, "%d %d moveto\n",
            LogicalToDeviceX((wxCoord)(x + size)), LogicalToDeviceY((wxCoord)by) );

    char buffer[100];
    sprintf(buffer, "%.8f rotate\n", angle);
    size_t i;
    for (i = 0; i < 100; i++)
        if (buffer[i] == ',') buffer[i] = '.';
    fprintf(m_pstream, buffer);

    fprintf( m_pstream, "(" );
    const wxWX2MBbuf textbuf = text.mb_str();
    size_t len = strlen(textbuf);
    for (i = 0; i < len; i++)
    {
        int c = (unsigned char) textbuf[i];
        if (c == ')' || c == '(' || c == '\\')
        {
            /* Cope with special characters */
            fprintf( m_pstream, "\\" );
            fputc(c, m_pstream);
        }
        else if ( c >= 128 )
        {
            /* Cope with character codes > 127 */
            fprintf(m_pstream, "\\%o", c);
        }
        else
        {
            fputc(c, m_pstream);
        }
    }

    fprintf( m_pstream, ") show\n" );

    sprintf( buffer, "%.8f rotate\n", -angle );
    for (i = 0; i < 100; i++)
        if (buffer[i] == ',') buffer[i] = '.';
    fprintf( m_pstream, buffer );

    if (m_font.GetUnderlined())
    {
        wxCoord uy = (wxCoord)(y + size - m_underlinePosition);
        wxCoord w, h;
        char buffer[100];
        GetTextExtent(text, &w, &h);

        sprintf( buffer,
                 "gsave\n"
                 "%d %d moveto\n"
                 "%f setlinewidth\n"
                 "%d %d lineto\n"
                 "stroke\n"
                 "grestore\n",
                 LogicalToDeviceX(x), LogicalToDeviceY(uy),
                 m_underlineThickness,
                 LogicalToDeviceX(x + w), LogicalToDeviceY(uy) );
        for (i = 0; i < 100; i++)
            if (buffer[i] == ',') buffer[i] = '.';
        fprintf( m_pstream, buffer );
    }

    CalcBoundingBox( x, y );
    CalcBoundingBox( x + size * text.Length() * 2/3 , y );
}

void wxPostScriptDC::SetBackground (const wxBrush& brush)
{
    m_backgroundBrush = brush;
}

void wxPostScriptDC::SetLogicalFunction (int WXUNUSED(function))
{
    wxFAIL_MSG( wxT("wxPostScriptDC::SetLogicalFunction not implemented.") );
}

void wxPostScriptDC::DoDrawSpline( wxList *points )
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    SetPen( m_pen );

    double a, b, c, d, x1, y1, x2, y2, x3, y3;
    wxPoint *p, *q;

    wxNode *node = points->First();
    p = (wxPoint *)node->Data();
    x1 = p->x;
    y1 = p->y;

    node = node->Next();
    p = (wxPoint *)node->Data();
    c = p->x;
    d = p->y;
    x3 = a = (double)(x1 + c) / 2;
    y3 = b = (double)(y1 + d) / 2;

    fprintf( m_pstream,
            "newpath\n"
            "%d %d moveto\n"
            "%d %d lineto\n",
            LogicalToDeviceX((wxCoord)x1), LogicalToDeviceY((wxCoord)y1),
            LogicalToDeviceX((wxCoord)x3), LogicalToDeviceY((wxCoord)y3) );

    CalcBoundingBox( (wxCoord)x1, (wxCoord)y1 );
    CalcBoundingBox( (wxCoord)x3, (wxCoord)y3 );

    while ((node = node->Next()) != NULL)
    {
        q = (wxPoint *)node->Data();

        x1 = x3;
        y1 = y3;
        x2 = c;
        y2 = d;
        c = q->x;
        d = q->y;
        x3 = (double)(x2 + c) / 2;
        y3 = (double)(y2 + d) / 2;

        fprintf( m_pstream,
                "%d %d %d %d %d %d DrawSplineSection\n",
                LogicalToDeviceX((wxCoord)x1), LogicalToDeviceY((wxCoord)y1),
                LogicalToDeviceX((wxCoord)x2), LogicalToDeviceY((wxCoord)y2),
                LogicalToDeviceX((wxCoord)x3), LogicalToDeviceY((wxCoord)y3) );

        CalcBoundingBox( (wxCoord)x1, (wxCoord)y1 );
        CalcBoundingBox( (wxCoord)x3, (wxCoord)y3 );
    }

    /*
       At this point, (x2,y2) and (c,d) are the position of the
       next-to-last and last point respectively, in the point list
     */

    fprintf( m_pstream,
            "%d %d lineto\n"
            "stroke\n",
            LogicalToDeviceX((wxCoord)c), LogicalToDeviceY((wxCoord)d) );
}

wxCoord wxPostScriptDC::GetCharWidth() const
{
    // Chris Breeze: reasonable approximation using wxMODERN/Courier
    return (wxCoord) (GetCharHeight() * 72.0 / 120.0);
}


void wxPostScriptDC::SetAxisOrientation( bool xLeftRight, bool yBottomUp )
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    m_signX = (xLeftRight ? 1 : -1);
    m_signY = (yBottomUp  ? 1 : -1);

    // FIXME there is no such function in MSW nor in OS2/PM
#if !defined(__WXMSW__) && !defined(__WXPM__)
    ComputeScaleAndOrigin();
#endif
}

void wxPostScriptDC::SetDeviceOrigin( wxCoord x, wxCoord y )
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    int h = 0;
    int w = 0;
    GetSize( &w, &h );

    wxDC::SetDeviceOrigin( x, h-y );
}

void wxPostScriptDC::DoGetSize(int* width, int* height) const
{
    wxPaperSize id = m_printData.GetPaperId();

    wxPrintPaperType *paper = wxThePrintPaperDatabase->FindPaperType(id);

    if (!paper) paper = wxThePrintPaperDatabase->FindPaperType(wxPAPER_A4);

    int w = 595;
    int h = 842;
    if (paper)
    {
        w = paper->GetSizeDeviceUnits().x;
        h = paper->GetSizeDeviceUnits().y;
    }

    if (m_printData.GetOrientation() == wxLANDSCAPE)
    {
        int tmp = w;
        w = h;
        h = tmp;
    }

    if (width) *width = (int)(w * ms_PSScaleFactor);
    if (height) *height = (int)(h * ms_PSScaleFactor);
}

void wxPostScriptDC::DoGetSizeMM(int *width, int *height) const
{
    wxPaperSize id = m_printData.GetPaperId();

    wxPrintPaperType *paper = wxThePrintPaperDatabase->FindPaperType(id);

    if (!paper) paper = wxThePrintPaperDatabase->FindPaperType(wxPAPER_A4);

    int w = 210;
    int h = 297;
    if (paper)
    {
        w = paper->GetWidth() / 10;
        h = paper->GetHeight() / 10;
    }

    if (m_printData.GetOrientation() == wxLANDSCAPE)
    {
        int tmp = w;
        w = h;
        h = tmp;
    }

    if (width) *width = w;
    if (height) *height = h;
}

// Resolution in pixels per logical inch
wxSize wxPostScriptDC::GetPPI(void) const
{
    return wxSize((int)(72 * ms_PSScaleFactor),
                  (int)(72 * ms_PSScaleFactor));
}


bool wxPostScriptDC::StartDoc( const wxString& message )
{
    wxCHECK_MSG( m_ok, FALSE, wxT("invalid postscript dc") );

    if (m_printData.GetFilename() == wxT(""))
    {
        wxString filename = wxGetTempFileName( wxT("ps") );
        m_printData.SetFilename(filename);
    }

    m_pstream = wxFopen( m_printData.GetFilename().c_str(), wxT("w+") );  // FIXME: use fn_str() here under Unicode?

    if (!m_pstream)
    {
        wxLogError( _("Cannot open file for PostScript printing!"));
        m_ok = FALSE;
        return FALSE;
    }

    m_ok = TRUE;

    fprintf( m_pstream, "%%!PS-Adobe-2.0\n" );
    fprintf( m_pstream, "%%%%Title: %s\n", (const char *) m_title.ToAscii() );
    fprintf( m_pstream, "%%%%Creator: wxWindows PostScript renderer\n" );
    fprintf( m_pstream, "%%%%CreationDate: %s\n", (const char *) wxNow().ToAscii() );
    if (m_printData.GetOrientation() == wxLANDSCAPE)
        fprintf( m_pstream, "%%%%Orientation: Landscape\n" );
    else
        fprintf( m_pstream, "%%%%Orientation: Portrait\n" );
    
    // fprintf( m_pstream, "%%%%Pages: %d\n", (wxPageNumber - 1) );
    
    char *paper = "A4";
    switch (m_printData.GetPaperId())
    {
       case wxPAPER_LETTER: paper = "Letter"; break;             // Letter: paper ""; 8 1/2 by 11 inches
       case wxPAPER_LEGAL: paper = "Legal"; break;              // Legal, 8 1/2 by 14 inches
       case wxPAPER_A4: paper = "A4"; break;          // A4 Sheet, 210 by 297 millimeters
       case wxPAPER_TABLOID: paper = "Tabloid"; break;     // Tabloid, 11 by 17 inches
       case wxPAPER_LEDGER: paper = "Ledger"; break;      // Ledger, 17 by 11 inches
       case wxPAPER_STATEMENT: paper = "Statement"; break;   // Statement, 5 1/2 by 8 1/2 inches
       case wxPAPER_EXECUTIVE: paper = "Executive"; break;   // Executive, 7 1/4 by 10 1/2 inches
       case wxPAPER_A3: paper = "A3"; break;          // A3 sheet, 297 by 420 millimeters
       case wxPAPER_A5: paper = "A5"; break;          // A5 sheet, 148 by 210 millimeters
       case wxPAPER_B4: paper = "B4"; break;          // B4 sheet, 250 by 354 millimeters
       case wxPAPER_B5: paper = "B5"; break;          // B5 sheet, 182-by-257-millimeter paper
       case wxPAPER_FOLIO: paper = "Folio"; break;       // Folio, 8-1/2-by-13-inch paper
       case wxPAPER_QUARTO: paper = "Quaro"; break;      // Quarto, 215-by-275-millimeter paper
       case wxPAPER_10X14: paper = "10x14"; break;       // 10-by-14-inch sheet
       default: paper = "A4";
    }
    fprintf( m_pstream, "%%%%DocumentPaperSizes: %s\n", paper );
    fprintf( m_pstream, "%%%%EndComments\n\n" );

    fprintf( m_pstream, "%%%%BeginProlog\n" );
    fprintf( m_pstream, wxPostScriptHeaderConicTo );
    fprintf( m_pstream, wxPostScriptHeaderEllipse );
    fprintf( m_pstream, wxPostScriptHeaderEllipticArc );
    fprintf( m_pstream, wxPostScriptHeaderColourImage );
#if wxUSE_PANGO
#else
    fprintf( m_pstream, wxPostScriptHeaderReencodeISO1 );
    fprintf( m_pstream, wxPostScriptHeaderReencodeISO2 );
#endif
    if (wxPostScriptHeaderSpline)
        fprintf( m_pstream, wxPostScriptHeaderSpline );
    fprintf( m_pstream, "%%%%EndProlog\n" );

    SetBrush( *wxBLACK_BRUSH );
    SetPen( *wxBLACK_PEN );
    SetBackground( *wxWHITE_BRUSH );
    SetTextForeground( *wxBLACK );

    // set origin according to paper size
    SetDeviceOrigin( 0,0 );

    wxPageNumber = 1;
    m_pageNumber = 1;
    m_title = message;
    return TRUE;
}

void wxPostScriptDC::EndDoc ()
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    if (m_clipping)
    {
        m_clipping = FALSE;
        fprintf( m_pstream, "grestore\n" );
    }

    fclose( m_pstream );
    m_pstream = (FILE *) NULL;

#if 0    
    // THE FOLLOWING HAS BEEN CONTRIBUTED BY Andy Fyfe <andy@hyperparallel.com>
    wxCoord wx_printer_translate_x, wx_printer_translate_y;
    double wx_printer_scale_x, wx_printer_scale_y;

    wx_printer_translate_x = (wxCoord)m_printData.GetPrinterTranslateX();
    wx_printer_translate_y = (wxCoord)m_printData.GetPrinterTranslateY();

    wx_printer_scale_x = m_printData.GetPrinterScaleX();
    wx_printer_scale_y = m_printData.GetPrinterScaleY();

    // Compute the bounding box.  Note that it is in the default user
    // coordinate system, thus we have to convert the values.
    wxCoord minX = (wxCoord) LogicalToDeviceX(m_minX);
    wxCoord minY = (wxCoord) LogicalToDeviceY(m_minY);
    wxCoord maxX = (wxCoord) LogicalToDeviceX(m_maxX);
    wxCoord maxY = (wxCoord) LogicalToDeviceY(m_maxY);

    // LOG2DEV may have changed the minimum to maximum vice versa
    if ( minX > maxX ) { wxCoord tmp = minX; minX = maxX; maxX = tmp; }
    if ( minY > maxY ) { wxCoord tmp = minY; minY = maxY; maxY = tmp; }

    // account for used scaling (boundingbox is before scaling in ps-file)
    double scale_x = m_printData.GetPrinterScaleX() / ms_PSScaleFactor;
    double scale_y = m_printData.GetPrinterScaleY() / ms_PSScaleFactor;

    wxCoord llx, lly, urx, ury;
    llx = (wxCoord) ((minX+wx_printer_translate_x)*scale_x);
    lly = (wxCoord) ((minY+wx_printer_translate_y)*scale_y);
    urx = (wxCoord) ((maxX+wx_printer_translate_x)*scale_x);
    ury = (wxCoord) ((maxY+wx_printer_translate_y)*scale_y);
    // (end of bounding box computation)


    // If we're landscape, our sense of "x" and "y" is reversed.
    if (m_printData.GetOrientation() == wxLANDSCAPE)
    {
        wxCoord tmp;
        tmp = llx; llx = lly; lly = tmp;
        tmp = urx; urx = ury; ury = tmp;

        // We need either the two lines that follow, or we need to subtract
        // min_x from real_translate_y, which is commented out below.
        llx = llx - (wxCoord)(m_minX*wx_printer_scale_y);
        urx = urx - (wxCoord)(m_minX*wx_printer_scale_y);
    }

    // The Adobe specifications call for integers; we round as to make
    // the bounding larger.
    fprintf( m_pstream,
            "%%%%BoundingBox: %d %d %d %d\n",
            (wxCoord)floor((double)llx), (wxCoord)floor((double)lly),
            (wxCoord)ceil((double)urx), (wxCoord)ceil((double)ury) );

    // To check the correctness of the bounding box, postscript commands
    // to draw a box corresponding to the bounding box are generated below.
    // But since we typically don't want to print such a box, the postscript
    // commands are generated within comments.  These lines appear before any
    // adjustment of scale, rotation, or translation, and hence are in the
    // default user coordinates.
    fprintf( m_pstream, "%% newpath\n" );
    fprintf( m_pstream, "%% %d %d moveto\n", llx, lly );
    fprintf( m_pstream, "%% %d %d lineto\n", urx, lly );
    fprintf( m_pstream, "%% %d %d lineto\n", urx, ury );
    fprintf( m_pstream, "%% %d %d lineto closepath stroke\n", llx, ury );
#endif

#if defined(__X__) || defined(__WXGTK__)
    if (m_ok && (m_printData.GetPrintMode() == wxPRINT_MODE_PRINTER))    
    {
        wxString command;
        command += m_printData.GetPrinterCommand();
        command += wxT(" ");
        command += m_printData.GetFilename();

        wxExecute( command, TRUE );
        wxRemoveFile( m_printData.GetFilename() );
    }
#endif
}

void wxPostScriptDC::StartPage()
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    fprintf( m_pstream, "%%%%Page: %d\n", wxPageNumber++ );

    //  What is this one supposed to do? RR.
//  *m_pstream << "matrix currentmatrix\n";

    // Added by Chris Breeze

    // Each page starts with an "initgraphics" which resets the
    // transformation and so we need to reset the origin
    // (and rotate the page for landscape printing)

    // Output scaling
    wxCoord translate_x, translate_y;
    double scale_x, scale_y;

    translate_x = (wxCoord)m_printData.GetPrinterTranslateX();
    translate_y = (wxCoord)m_printData.GetPrinterTranslateY();

    scale_x = m_printData.GetPrinterScaleX();
    scale_y = m_printData.GetPrinterScaleY();

    if (m_printData.GetOrientation() == wxLANDSCAPE)
    {
        int h;
        GetSize( (int*) NULL, &h );
        translate_y -= h;
        fprintf( m_pstream, "90 rotate\n" );

        // I copied this one from a PostScript tutorial, but to no avail. RR.
        // fprintf( m_pstream, "90 rotate llx neg ury nef translate\n" );
    }

    char buffer[100];
    sprintf( buffer, "%.8f %.8f scale\n", scale_x / ms_PSScaleFactor,
                                          scale_y / ms_PSScaleFactor);
    for (int i = 0; i < 100; i++)
        if (buffer[i] == ',') buffer[i] = '.';
    fprintf( m_pstream, buffer );

    fprintf( m_pstream, "%d %d translate\n", translate_x, translate_y );
}

void wxPostScriptDC::EndPage ()
{
    wxCHECK_RET( m_ok && m_pstream, wxT("invalid postscript dc") );

    fprintf( m_pstream, "showpage\n" );
}

bool wxPostScriptDC::DoBlit( wxCoord xdest, wxCoord ydest,
                           wxCoord fwidth, wxCoord fheight,
                           wxDC *source,
                           wxCoord xsrc, wxCoord ysrc,
                           int rop, bool WXUNUSED(useMask), wxCoord WXUNUSED(xsrcMask), wxCoord WXUNUSED(ysrcMask) )
{
    wxCHECK_MSG( m_ok && m_pstream, FALSE, wxT("invalid postscript dc") );

    wxCHECK_MSG( source, FALSE, wxT("invalid source dc") );

    /* blit into a bitmap */
    wxBitmap bitmap( (int)fwidth, (int)fheight );
    wxMemoryDC memDC;
    memDC.SelectObject(bitmap);
    memDC.Blit(0, 0, fwidth, fheight, source, xsrc, ysrc, rop); /* TODO: Blit transparently? */
    memDC.SelectObject(wxNullBitmap);

    /* draw bitmap. scaling and positioning is done there */
    DrawBitmap( bitmap, xdest, ydest );

    return TRUE;
}

wxCoord wxPostScriptDC::GetCharHeight() const
{
    if (m_font.Ok())
        return m_font.GetPointSize();
    else
        return 12;
}

void wxPostScriptDC::DoGetTextExtent(const wxString& string,
                                     wxCoord *x, wxCoord *y,
                                     wxCoord *descent, wxCoord *externalLeading,
                                     wxFont *theFont ) const
{
    wxFont *fontToUse = theFont;

    if (!fontToUse) fontToUse = (wxFont*) &m_font;

    wxCHECK_RET( fontToUse, wxT("GetTextExtent: no font defined") );

    if (string.IsEmpty())
    {
        if (x) (*x) = 0;
        if (y) (*y) = 0;
        return;
    }
    
#if wxUSE_PANGO
    int dpi = GetResolution();
    PangoContext *context = pango_ft2_get_context ( dpi, dpi );
    
    pango_context_set_language (context, pango_language_from_string ("en_US"));
    pango_context_set_base_dir (context, PANGO_DIRECTION_LTR );

    PangoLayout *layout = pango_layout_new (context);
    
    PangoFontDescription *desc = fontToUse->GetNativeFontInfo()->description;
    pango_layout_set_font_description(layout, desc);
#if wxUSE_UNICODE
        const wxCharBuffer data = wxConvUTF8.cWC2MB( string );
        pango_layout_set_text(layout, (const char*) data, strlen( (const char*) data ));
#else
        const wxWCharBuffer wdata = wxConvLocal.cMB2WC( string );
        const wxCharBuffer data = wxConvUTF8.cWC2MB( wdata );
        pango_layout_set_text(layout, (const char*) data, strlen( (const char*) data ));
#endif
    PangoLayoutLine *line = (PangoLayoutLine *)pango_layout_get_lines(layout)->data;
 
    PangoRectangle rect;
    pango_layout_line_get_extents(line, NULL, &rect);
    
    if (x) (*x) = (wxCoord) ( m_scaleX * rect.width / PANGO_SCALE );
    if (y) (*y) = (wxCoord) ( m_scaleY * rect.height / PANGO_SCALE );
    if (descent)
    {
        // Do something about metrics here
        (*descent) = 0;
    }
    if (externalLeading) (*externalLeading) = 0;  // ??
    
    g_object_unref( G_OBJECT( layout ) );
#else
   // GTK 2.0

    const wxWX2MBbuf strbuf = string.mb_str();

#if !wxUSE_AFM_FOR_POSTSCRIPT
    /* Provide a VERY rough estimate (avoid using it).
     * Produces accurate results for mono-spaced font
     * such as Courier (aka wxMODERN) */

    int height = 12;
    if (fontToUse)
    {
        height = fontToUse->GetPointSize();
    }
    if ( x )
        *x = strlen (strbuf) * height * 72 / 120;
    if ( y )
        *y = (wxCoord) (height * 1.32);    /* allow for descender */
    if (descent) *descent = 0;
    if (externalLeading) *externalLeading = 0;
#else

    /* method for calculating string widths in postscript:
    /  read in the AFM (adobe font metrics) file for the
    /  actual font, parse it and extract the character widths
    /  and also the descender. this may be improved, but for now
    /  it works well. the AFM file is only read in if the
    /  font is changed. this may be chached in the future.
    /  calls to GetTextExtent with the font unchanged are rather
    /  efficient!!!
    /
    /  for each font and style used there is an AFM file necessary.
    /  currently i have only files for the roman font family.
    /  I try to get files for the other ones!
    /
    /  CAVE: the size of the string is currently always calculated
    /        in 'points' (1/72 of an inch). this should later on be
    /        changed to depend on the mapping mode.
    /  CAVE: the path to the AFM files must be set before calling this
    /        function. this is usually done by a call like the following:
    /        wxSetAFMPath("d:\\wxw161\\afm\\");
    /
    /  example:
    /
    /    wxPostScriptDC dc(NULL, TRUE);
    /    if (dc.Ok()){
    /      wxSetAFMPath("d:\\wxw161\\afm\\");
    /      dc.StartDoc("Test");
    /      dc.StartPage();
    /      wxCoord w,h;
    /      dc.SetFont(new wxFont(10, wxROMAN, wxNORMAL, wxNORMAL));
    /      dc.GetTextExtent("Hallo",&w,&h);
    /      dc.EndPage();
    /      dc.EndDoc();
    /    }
    /
    /  by steve (stefan.hammes@urz.uni-heidelberg.de)
    /  created: 10.09.94
    /  updated: 14.05.95 */

    /* these static vars are for storing the state between calls */
    static int lastFamily= INT_MIN;
    static int lastSize= INT_MIN;
    static int lastStyle= INT_MIN;
    static int lastWeight= INT_MIN;
    static int lastDescender = INT_MIN;
    static int lastWidths[256]; /* widths of the characters */

    double UnderlinePosition = 0.0;
    double UnderlineThickness = 0.0;

    // Get actual parameters
    int Family = fontToUse->GetFamily();
    int Size =   fontToUse->GetPointSize();
    int Style =  fontToUse->GetStyle();
    int Weight = fontToUse->GetWeight();

    // If we have another font, read the font-metrics
    if (Family!=lastFamily || Size!=lastSize || Style!=lastStyle || Weight!=lastWeight)
    {
        // Store actual values
        lastFamily = Family;
        lastSize =   Size;
        lastStyle =  Style;
        lastWeight = Weight;

        const wxChar *name = NULL;

        switch (Family)
        {
            case wxMODERN:
            case wxTELETYPE:
            {
                if ((Style == wxITALIC) && (Weight == wxBOLD)) name = wxT("CourBoO.afm");
                else if ((Style != wxITALIC) && (Weight == wxBOLD)) name = wxT("CourBo.afm");
                else if ((Style == wxITALIC) && (Weight != wxBOLD)) name = wxT("CourO.afm");
                else name = wxT("Cour.afm");
                break;
            }
            case wxROMAN:
            {
                if ((Style == wxITALIC) && (Weight == wxBOLD)) name = wxT("TimesBoO.afm");
                else if ((Style != wxITALIC) && (Weight == wxBOLD)) name = wxT("TimesBo.afm");
                else if ((Style == wxITALIC) && (Weight != wxBOLD)) name = wxT("TimesO.afm");
                else name = wxT("TimesRo.afm");
                break;
            }
            case wxSCRIPT:
            {
                name = wxT("Zapf.afm");
                Style = wxNORMAL;
                Weight = wxNORMAL;
            }
            case wxSWISS:
            default:
            {
                if ((Style == wxITALIC) && (Weight == wxBOLD)) name = wxT("HelvBoO.afm");
                else if ((Style != wxITALIC) && (Weight == wxBOLD)) name = wxT("HelvBo.afm");
                else if ((Style == wxITALIC) && (Weight != wxBOLD)) name = wxT("HelvO.afm");
                else name = wxT("Helv.afm");
                break;
            }
        }

        FILE *afmFile = NULL;
        
        // Get the directory of the AFM files
        wxString afmName;
        if (!m_printData.GetFontMetricPath().IsEmpty())
        {
            afmName = m_printData.GetFontMetricPath();
            afmName << wxFILE_SEP_PATH << name;
            afmFile = wxFopen(afmName,wxT("r"));
        }

#if defined(__UNIX__) && !defined(__VMS__)
        if (afmFile==NULL)
        {
           afmName = wxGetDataDir();
           afmName <<  wxFILE_SEP_PATH
#if defined(__LINUX__) || defined(__FREEBSD__)
                   << wxT("gs_afm") << wxFILE_SEP_PATH
#else
                   << wxT("afm") << wxFILE_SEP_PATH
#endif
                   << name;
           afmFile = wxFopen(afmName,wxT("r"));
        }
#endif

        /* 2. open and process the file
           /  a short explanation of the AFM format:
           /  we have for each character a line, which gives its size
           /  e.g.:
           /
           /    C 63 ; WX 444 ; N question ; B 49 -14 395 676 ;
           /
           /  that means, we have a character with ascii code 63, and width
           /  (444/1000 * fontSize) points.
           /  the other data is ignored for now!
           /
           /  when the font has changed, we read in the right AFM file and store the
           /  character widths in an array, which is processed below (see point 3.). */
        if (afmFile==NULL)
        {
            wxLogDebug( wxT("GetTextExtent: can't open AFM file '%s'"), afmName.c_str() );
            wxLogDebug( wxT("               using approximate values"));
            for (int i=0; i<256; i++) lastWidths[i] = 500; /* an approximate value */
            lastDescender = -150; /* dito. */
        }
        else
        {
            /* init the widths array */
            for(int i=0; i<256; i++) lastWidths[i] = INT_MIN;
            /* some variables for holding parts of a line */
            char cString[10],semiString[10],WXString[10],descString[20];
            char upString[30], utString[30], encString[50];
            char line[256];
            int ascii,cWidth;
            /* read in the file and parse it */
            while(fgets(line,sizeof(line),afmFile)!=NULL)
            {
                /* A.) check for descender definition */
                if (strncmp(line,"Descender",9)==0)
                {
                    if ((sscanf(line,"%s%d",descString,&lastDescender)!=2) ||
                            (strcmp(descString,"Descender")!=0))
                    {
                        wxLogDebug( wxT("AFM-file '%s': line '%s' has error (bad descender)"), afmName.c_str(),line );
                    }
                }
                /* JC 1.) check for UnderlinePosition */
                else if(strncmp(line,"UnderlinePosition",17)==0)
                {
                    if ((sscanf(line,"%s%lf",upString,&UnderlinePosition)!=2) ||
                            (strcmp(upString,"UnderlinePosition")!=0))
                    {
                        wxLogDebug( wxT("AFM-file '%s': line '%s' has error (bad UnderlinePosition)"), afmName.c_str(), line );
                    }
                }
                /* JC 2.) check for UnderlineThickness */
                else if(strncmp(line,"UnderlineThickness",18)==0)
                {
                    if ((sscanf(line,"%s%lf",utString,&UnderlineThickness)!=2) ||
                            (strcmp(utString,"UnderlineThickness")!=0))
                    {
                        wxLogDebug( wxT("AFM-file '%s': line '%s' has error (bad UnderlineThickness)"), afmName.c_str(), line );
                    }
                }
                /* JC 3.) check for EncodingScheme */
                else if(strncmp(line,"EncodingScheme",14)==0)
                {
                    if ((sscanf(line,"%s%s",utString,encString)!=2) ||
                            (strcmp(utString,"EncodingScheme")!=0))
                    {
                        wxLogDebug( wxT("AFM-file '%s': line '%s' has error (bad EncodingScheme)"), afmName.c_str(), line );
                    }
                    else if (strncmp(encString, "AdobeStandardEncoding", 21))
                    {
                        wxLogDebug( wxT("AFM-file '%s': line '%s' has error (unsupported EncodingScheme %s)"),
                                afmName.c_str(),line, encString);
                    }
                }
                /* B.) check for char-width */
                else if(strncmp(line,"C ",2)==0)
                {
                    if (sscanf(line,"%s%d%s%s%d",cString,&ascii,semiString,WXString,&cWidth)!=5)
                    {
                        wxLogDebug(wxT("AFM-file '%s': line '%s' has an error (bad character width)"),afmName.c_str(),line);
                    }
                    if(strcmp(cString,"C")!=0 || strcmp(semiString,";")!=0 || strcmp(WXString,"WX")!=0)
                    {
                        wxLogDebug(wxT("AFM-file '%s': line '%s' has a format error"),afmName.c_str(),line);
                    }
                    /* printf("            char '%c'=%d has width '%d'\n",ascii,ascii,cWidth); */
                    if (ascii>=0 && ascii<256)
                    {
                        lastWidths[ascii] = cWidth; /* store width */
                    }
                    else
                    {
                        /* MATTHEW: this happens a lot; don't print an error */
                        /* wxLogDebug("AFM-file '%s': ASCII value %d out of range",afmName.c_str(),ascii); */
                    }
                }
                /* C.) ignore other entries. */
            }
            fclose(afmFile);
        }
        /* hack to compute correct values for german 'Umlaute'
           /  the correct way would be to map the character names
           /  like 'adieresis' to corresp. positions of ISOEnc and read
           /  these values from AFM files, too. Maybe later ... */
        lastWidths[196] = lastWidths['A'];  // �
        lastWidths[228] = lastWidths['a'];  // �
        lastWidths[214] = lastWidths['O'];  // �
        lastWidths[246] = lastWidths['o'];  // �
        lastWidths[220] = lastWidths['U'];  // �
        lastWidths[252] = lastWidths['u'];  // �
        lastWidths[223] = lastWidths[251];  // �

        /* JC: calculate UnderlineThickness/UnderlinePosition */

        // VS: dirty, but is there any better solution?
        double *pt;
        pt = (double*) &m_underlinePosition;
        *pt = LogicalToDeviceYRel((wxCoord)(UnderlinePosition * fontToUse->GetPointSize())) / 1000.0f;
        pt = (double*) &m_underlineThickness;
        *pt = LogicalToDeviceYRel((wxCoord)(UnderlineThickness * fontToUse->GetPointSize())) / 1000.0f;

    }


    /* 3. now the font metrics are read in, calc size this
       /  is done by adding the widths of the characters in the
       /  string. they are given in 1/1000 of the size! */

    long sum=0;
    wxCoord height=Size; /* by default */
    unsigned char *p;
    for(p=(unsigned char *)wxMBSTRINGCAST strbuf; *p; p++)
    {
        if(lastWidths[*p]== INT_MIN)
        {
            wxLogDebug(wxT("GetTextExtent: undefined width for character '%c' (%d)"), *p,*p);
            sum += lastWidths[' ']; /* assume space */
        }
        else
        {
            sum += lastWidths[*p];
        }
    }

    double widthSum = sum;
    widthSum *= Size;
    widthSum /= 1000.0F;

    /* add descender to height (it is usually a negative value) */
    //if (lastDescender != INT_MIN)
    //{
    //    height += (wxCoord)(((-lastDescender)/1000.0F) * Size); /* MATTHEW: forgot scale */
    //}
    // - commented by V. Slavik - height already contains descender in it
    //   (judging from few experiments)

    /* return size values */
    if ( x )
        *x = (wxCoord)widthSum;
    if ( y )
        *y = height;

    /* return other parameters */
    if (descent)
    {
        if(lastDescender!=INT_MIN)
        {
            *descent = (wxCoord)(((-lastDescender)/1000.0F) * Size); /* MATTHEW: forgot scale */
        }
        else
        {
            *descent = 0;
        }
    }

    /* currently no idea how to calculate this! */
    if (externalLeading) *externalLeading = 0;
#endif
    // Use AFM

#endif
    // GTK 2.0
}

#if WXWIN_COMPATIBILITY_2_2
WXDLLEXPORT wxPrintSetupData *wxThePrintSetupData = 0;

void wxInitializePrintSetupData(bool init)
{
    if (init)
    {
        // gets initialized in the constructor
        wxThePrintSetupData = new wxPrintSetupData;
    }
    else
    {
        delete wxThePrintSetupData;

        wxThePrintSetupData = (wxPrintSetupData *) NULL;
    }
}

// A module to allow initialization/cleanup of PostScript-related
// things without calling these functions from app.cpp.

class WXDLLEXPORT wxPostScriptModule: public wxModule
{
DECLARE_DYNAMIC_CLASS(wxPostScriptModule)
public:
    wxPostScriptModule() {}
    bool OnInit();
    void OnExit();
};

IMPLEMENT_DYNAMIC_CLASS(wxPostScriptModule, wxModule)

bool wxPostScriptModule::OnInit()
{
    wxInitializePrintSetupData();

    return TRUE;
}

void wxPostScriptModule::OnExit()
{
    wxInitializePrintSetupData(FALSE);
}
#endif
  // WXWIN_COMPATIBILITY_2_2

#endif
  // wxUSE_POSTSCRIPT

#endif
  // wxUSE_PRINTING_ARCHITECTURE


// vi:sts=4:sw=4:et
