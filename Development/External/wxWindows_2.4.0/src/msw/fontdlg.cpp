/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/fontdlg.cpp
// Purpose:     wxFontDialog class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: fontdlg.cpp,v 1.15 2002/08/30 20:34:25 JS Exp $
// Copyright:   (c) Julian Smart and Markus Holzem
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "fontdlg.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_FONTDLG

#ifndef WX_PRECOMP
    #include "wx/defs.h"
    #include "wx/utils.h"
    #include "wx/dialog.h"
#endif

#include "wx/fontdlg.h"

#if !defined(__WIN32__) || defined(__SALFORDC__) || defined(__WXWINE__)
#include <windows.h>
#include <commdlg.h>
#endif

#include "wx/msw/private.h"
#include "wx/cmndata.h"
#include "wx/log.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// ----------------------------------------------------------------------------
// wxWin macros
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxFontDialog, wxDialog)

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxFontDialog
// ----------------------------------------------------------------------------

int wxFontDialog::ShowModal()
{
    DWORD flags = CF_SCREENFONTS | CF_NOSIMULATIONS;

    LOGFONT logFont;

    CHOOSEFONT chooseFontStruct;
    wxZeroMemory(chooseFontStruct);

    chooseFontStruct.lStructSize = sizeof(CHOOSEFONT);
    if ( m_parent )
        chooseFontStruct.hwndOwner = GetHwndOf(m_parent);
    chooseFontStruct.lpLogFont = &logFont;

    if ( m_fontData.initialFont.Ok() )
    {
        flags |= CF_INITTOLOGFONTSTRUCT;
        wxFillLogFont(&logFont, &m_fontData.initialFont);
    }

    if ( m_fontData.fontColour.Ok() )
    {
        chooseFontStruct.rgbColors = wxColourToRGB(m_fontData.fontColour);

        // need this for the colour to be taken into account
        flags |= CF_EFFECTS;
    }

    // CF_ANSIONLY flag is obsolete for Win32
    if ( !m_fontData.GetAllowSymbols() )
    {
#ifdef __WIN16__
      flags |= CF_ANSIONLY;
#else // Win32
      flags |= CF_SELECTSCRIPT;
      logFont.lfCharSet = ANSI_CHARSET;
#endif // Win16/32
    }

    if ( m_fontData.GetEnableEffects() )
      flags |= CF_EFFECTS;
    if ( m_fontData.GetShowHelp() )
      flags |= CF_SHOWHELP;

    if ( m_fontData.minSize != 0 || m_fontData.maxSize != 0 )
    {
        chooseFontStruct.nSizeMin = m_fontData.minSize;
        chooseFontStruct.nSizeMax = m_fontData.maxSize;
        flags |= CF_LIMITSIZE;
    }

    chooseFontStruct.Flags = flags;

    if ( ChooseFont(&chooseFontStruct) != 0 )
    {
        wxRGBToColour(m_fontData.fontColour, chooseFontStruct.rgbColors);
        m_fontData.chosenFont = wxCreateFontFromLogFont(&logFont);
        m_fontData.EncodingInfo().facename = logFont.lfFaceName;
        m_fontData.EncodingInfo().charset = logFont.lfCharSet;

        return wxID_OK;
    }
    else
    {
        // common dialog failed - why?
#ifdef __WXDEBUG__
        DWORD dwErr = CommDlgExtendedError();
        if ( dwErr != 0 )
        {
            // this msg is only for developers
            wxLogError(wxT("Common dialog failed with error code %0lx."),
                       dwErr);
        }
        //else: it was just cancelled
#endif

        return wxID_CANCEL;
    }
}

#endif // wxUSE_FONTDLG
