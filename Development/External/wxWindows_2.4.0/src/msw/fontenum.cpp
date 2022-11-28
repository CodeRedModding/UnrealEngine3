///////////////////////////////////////////////////////////////////////////////
// Name:        msw/fontenum.cpp
// Purpose:     wxFontEnumerator class for Windows
// Author:      Julian Smart
// Modified by: Vadim Zeitlin to add support for font encodings
// Created:     04/01/98
// RCS-ID:      $Id: fontenum.cpp,v 1.29 2002/08/30 20:34:25 JS Exp $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "fontenum.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
  #pragma hdrstop
#endif

#if wxUSE_FONTMAP

#ifndef WX_PRECOMP
  #include "wx/font.h"
#endif

#include "wx/fontutil.h"
#include "wx/fontenum.h"
#include "wx/fontmap.h"

#include "wx/msw/private.h"

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// the helper class which calls ::EnumFontFamilies() and whose OnFont() is
// called from the callback passed to this function and, in its turn, calls the
// appropariate wxFontEnumerator method
class wxFontEnumeratorHelper
{
public:
    wxFontEnumeratorHelper(wxFontEnumerator *fontEnum);

    // control what exactly are we enumerating
        // we enumerate fonts with given enocding
    bool SetEncoding(wxFontEncoding encoding);
        // we enumerate fixed-width fonts
    void SetFixedOnly(bool fixedOnly) { m_fixedOnly = fixedOnly; }
        // we enumerate the encodings available in this family
    void SetFamily(const wxString& family);

    // call to start enumeration
    void DoEnumerate();

    // called by our font enumeration proc
    bool OnFont(const LPLOGFONT lf, const LPTEXTMETRIC tm) const;

private:
    // the object we forward calls to OnFont() to
    wxFontEnumerator *m_fontEnum;

    // if != -1, enum only fonts which have this encoding
    int m_charset;

    // if not empty, enum only the fonts with this facename
    wxString m_facename;

    // if not empty, enum only the fonts in this family
    wxString m_family;

    // if TRUE, enum only fixed fonts
    bool m_fixedOnly;

    // if TRUE, we enumerate the encodings, not fonts
    bool m_enumEncodings;

    // the list of charsets we already found while enumerating charsets
    wxArrayInt m_charsets;

    // the list of facenames we already found while enumerating facenames
    wxArrayString m_facenames;
};

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

#ifndef __WXMICROWIN__
int CALLBACK wxFontEnumeratorProc(LPLOGFONT lplf, LPTEXTMETRIC lptm,
                                  DWORD dwStyle, LONG lParam);
#endif

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxFontEnumeratorHelper
// ----------------------------------------------------------------------------

wxFontEnumeratorHelper::wxFontEnumeratorHelper(wxFontEnumerator *fontEnum)
{
    m_fontEnum = fontEnum;
    m_charset = DEFAULT_CHARSET;
    m_fixedOnly = FALSE;
    m_enumEncodings = FALSE;
}

void wxFontEnumeratorHelper::SetFamily(const wxString& family)
{
    m_enumEncodings = TRUE;
    m_family = family;
}

bool wxFontEnumeratorHelper::SetEncoding(wxFontEncoding encoding)
{
    if ( encoding != wxFONTENCODING_SYSTEM )
    {
        wxNativeEncodingInfo info;
        if ( !wxGetNativeFontEncoding(encoding, &info) )
        {
#if wxUSE_FONTMAP
            if ( !wxFontMapper::Get()->GetAltForEncoding(encoding, &info) )
#endif // wxUSE_FONTMAP
            {
                // no such encodings at all
                return FALSE;
            }
        }

        m_charset = info.charset;
        m_facename = info.facename;
    }

    return TRUE;
}

#if defined(__WXWINE__)
    #define wxFONTENUMPROC FONTENUMPROCEX
#elif (defined(__GNUWIN32__) && !defined(__CYGWIN10__) && !wxCHECK_W32API_VERSION( 1, 1 ))
    #if wxUSE_NORLANDER_HEADERS
        #define wxFONTENUMPROC int(*)(const LOGFONT *, const TEXTMETRIC *, long unsigned int, LPARAM)
    #else
        #define wxFONTENUMPROC int(*)(ENUMLOGFONTEX *, NEWTEXTMETRICEX*, int, LPARAM)
    #endif
#else
    #define wxFONTENUMPROC FONTENUMPROC
#endif

void wxFontEnumeratorHelper::DoEnumerate()
{
#ifndef __WXMICROWIN__
    HDC hDC = ::GetDC(NULL);

#ifdef __WIN32__
    LOGFONT lf;
    lf.lfCharSet = m_charset;
    wxStrncpy(lf.lfFaceName, m_facename, WXSIZEOF(lf.lfFaceName));
    lf.lfPitchAndFamily = 0;
    ::EnumFontFamiliesEx(hDC, &lf, (wxFONTENUMPROC)wxFontEnumeratorProc,
                         (LPARAM)this, 0 /* reserved */) ;
#else // Win16
    ::EnumFonts(hDC, (LPTSTR)NULL, (FONTENUMPROC)wxFontEnumeratorProc,
    #ifdef STRICT
               (LPARAM)
    #else
               (LPSTR)
    #endif
               this);
#endif // Win32/16

    ::ReleaseDC(NULL, hDC);
#endif
}

bool wxFontEnumeratorHelper::OnFont(const LPLOGFONT lf,
                                    const LPTEXTMETRIC tm) const
{
    if ( m_enumEncodings )
    {
        // is this a new charset?
        int cs = lf->lfCharSet;
        if ( m_charsets.Index(cs) == wxNOT_FOUND )
        {
            wxConstCast(this, wxFontEnumeratorHelper)->m_charsets.Add(cs);

            wxFontEncoding enc = wxGetFontEncFromCharSet(cs);
            return m_fontEnum->OnFontEncoding(lf->lfFaceName,
                                              wxFontMapper::GetEncodingName(enc));
        }
        else
        {
            // continue enumeration
            return TRUE;
        }
    }

    if ( m_fixedOnly )
    {
        // check that it's a fixed pitch font (there is *no* error here, the
        // flag name is misleading!)
        if ( tm->tmPitchAndFamily & TMPF_FIXED_PITCH )
        {
            // not a fixed pitch font
            return TRUE;
        }
    }

    if ( m_charset != DEFAULT_CHARSET )
    {
        // check that we have the right encoding
        if ( lf->lfCharSet != m_charset )
        {
            return TRUE;
        }
    }
    else // enumerating fonts in all charsets
    {
        // we can get the same facename twice or more in this case because it
        // may exist in several charsets but we only want to return one copy of
        // it (note that this can't happen for m_charset != DEFAULT_CHARSET)
        if ( m_facenames.Index(lf->lfFaceName) != wxNOT_FOUND )
        {
            // continue enumeration
            return TRUE;
        }

        wxConstCast(this, wxFontEnumeratorHelper)->
            m_facenames.Add(lf->lfFaceName);
    }

    return m_fontEnum->OnFacename(lf->lfFaceName);
}

// ----------------------------------------------------------------------------
// wxFontEnumerator
// ----------------------------------------------------------------------------

bool wxFontEnumerator::EnumerateFacenames(wxFontEncoding encoding,
                                          bool fixedWidthOnly)
{
    wxFontEnumeratorHelper fe(this);
    if ( fe.SetEncoding(encoding) )
    {
        fe.SetFixedOnly(fixedWidthOnly);

        fe.DoEnumerate();
    }
    // else: no such fonts, unknown encoding

    return TRUE;
}

bool wxFontEnumerator::EnumerateEncodings(const wxString& family)
{
    wxFontEnumeratorHelper fe(this);
    fe.SetFamily(family);
    fe.DoEnumerate();

    return TRUE;
}

// ----------------------------------------------------------------------------
// Windows callbacks
// ----------------------------------------------------------------------------

#ifndef __WXMICROWIN__
int CALLBACK wxFontEnumeratorProc(LPLOGFONT lplf, LPTEXTMETRIC lptm,
                                  DWORD WXUNUSED(dwStyle), LONG lParam)
{

    // we used to process TrueType fonts only, but there doesn't seem to be any
    // reasons to restrict ourselves to them here
#if 0
    // Get rid of any fonts that we don't want...
    if ( dwStyle != TRUETYPE_FONTTYPE )
    {
        // continue enumeration
        return TRUE;
    }
#endif // 0

    wxFontEnumeratorHelper *fontEnum = (wxFontEnumeratorHelper *)lParam;

    return fontEnum->OnFont(lplf, lptm);
}
#endif

#endif // wxUSE_FONTMAP
