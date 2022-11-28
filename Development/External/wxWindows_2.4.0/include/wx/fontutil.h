/////////////////////////////////////////////////////////////////////////////
// Name:        wx/fontutil.h
// Purpose:     font-related helper functions
// Author:      Vadim Zeitlin
// Modified by:
// Created:     05.11.99
// RCS-ID:      $Id: fontutil.h,v 1.19.2.1 2002/11/09 15:20:31 RR Exp $
// Copyright:   (c) wxWindows team
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

// General note: this header is private to wxWindows and is not supposed to be
// included by user code. The functions declared here are implemented in
// msw/fontutil.cpp for Windows, unix/fontutil.cpp for GTK/Motif &c.

#ifndef _WX_FONTUTIL_H_
#define _WX_FONTUTIL_H_

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "fontutil.h"
#endif

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/font.h"        // for wxFont and wxFontEncoding

#if defined(__WXMSW__)
    #include <windows.h>
    #include "wx/msw/winundef.h"
#endif

#if defined(_WX_X_FONTLIKE)

// the symbolic names for the XLFD fields (with examples for their value)
//
// NB: we suppose that the font always starts with the empty token (font name
//     registry field) as we never use nor generate it anyhow
enum wxXLFDField
{
    wxXLFD_FOUNDRY,     // adobe
    wxXLFD_FAMILY,      // courier, times, ...
    wxXLFD_WEIGHT,      // black, bold, demibold, medium, regular, light
    wxXLFD_SLANT,       // r/i/o (roman/italique/oblique)
    wxXLFD_SETWIDTH,    // condensed, expanded, ...
    wxXLFD_ADDSTYLE,    // whatever - usually nothing
    wxXLFD_PIXELSIZE,   // size in pixels
    wxXLFD_POINTSIZE,   // size in points
    wxXLFD_RESX,        // 72, 75, 100, ...
    wxXLFD_RESY,
    wxXLFD_SPACING,     // m/p/c (monospaced/proportional/character cell)
    wxXLFD_AVGWIDTH,    // average width in 1/10 pixels
    wxXLFD_REGISTRY,    // iso8859, rawin, koi8, ...
    wxXLFD_ENCODING,    // 1, r, r, ...
    wxXLFD_MAX
};

#endif // _WX_X_FONTLIKE

// ----------------------------------------------------------------------------
// types
// ----------------------------------------------------------------------------

// wxNativeFontInfo is platform-specific font representation: this struct
// should be considered as opaque font description only used by the native
// functions, the user code can only get the objects of this type from
// somewhere and pass it somewhere else (possibly save them somewhere using
// ToString() and restore them using FromString())
//
// NB: it is a POD currently for max efficiency but if it continues to grow
//     further it might make sense to make it a real class with virtual methods
struct WXDLLEXPORT wxNativeFontInfo
{
#if wxUSE_PANGO
    PangoFontDescription *description;
#elif defined(_WX_X_FONTLIKE)
    // the members can't be accessed directly as we only parse the
    // xFontName on demand
private:
    // the components of the XLFD
    wxString     fontElements[wxXLFD_MAX];

    // the full XLFD
    wxString     xFontName;

    // true until SetXFontName() is called
    bool         m_isDefault;

    // return true if we have already initialized fontElements
    inline bool HasElements() const;

public:
    // init the elements from an XLFD, return TRUE if ok
    bool FromXFontName(const wxString& xFontName);

    // return false if we were never initialized with a valid XLFD
    bool IsDefault() const { return m_isDefault; }

    // return the XLFD (using the fontElements if necessary)
    wxString GetXFontName() const;

    // get the given XFLD component
    wxString GetXFontComponent(wxXLFDField field) const;

    // change the font component
    void SetXFontComponent(wxXLFDField field, const wxString& value);

    // set the XFLD
    void SetXFontName(const wxString& xFontName);
#elif defined(__WXMSW__)
    LOGFONT      lf;
#elif defined(__WXPM__)
    // OS/2 native structures that define a font
    FATTRS       fa;
    FONTMETRICS  fm;
    FACENAMEDESC fn;
#else // other platforms
    //
    //  This is a generic implementation that should work on all ports
    //  without specific support by the port.
    //
    #define wxNO_NATIVE_FONTINFO

    int           pointSize;
    wxFontFamily  family;
    wxFontStyle   style;
    wxFontWeight  weight;
    bool          underlined;
    wxString      faceName;
    wxFontEncoding encoding;
#endif // platforms

    // default ctor (default copy ctor is ok)
    wxNativeFontInfo() { Init(); }

    // reset to the default state
    void Init();

    // accessors and modifiers for the font elements
    int GetPointSize() const;
    wxFontStyle GetStyle() const;
    wxFontWeight GetWeight() const;
    bool GetUnderlined() const;
    wxString GetFaceName() const;
    wxFontFamily GetFamily() const;
    wxFontEncoding GetEncoding() const;

    void SetPointSize(int pointsize);
    void SetStyle(wxFontStyle style);
    void SetWeight(wxFontWeight weight);
    void SetUnderlined(bool underlined);
    void SetFaceName(wxString facename);
    void SetFamily(wxFontFamily family);
    void SetEncoding(wxFontEncoding encoding);

    // it is important to be able to serialize wxNativeFontInfo objects to be
    // able to store them (in config file, for example)
    bool FromString(const wxString& s);
    wxString ToString() const;

    // we also want to present the native font descriptions to the user in some
    // human-readable form (it is not platform independent neither, but can
    // hopefully be understood by the user)
    bool FromUserString(const wxString& s);
    wxString ToUserString() const;
};

// ----------------------------------------------------------------------------
// font-related functions (common)
// ----------------------------------------------------------------------------

// translate a wxFontEncoding into native encoding parameter (defined above),
// returning TRUE if an (exact) macth could be found, FALSE otherwise (without
// attempting any substitutions)
extern bool wxGetNativeFontEncoding(wxFontEncoding encoding,
                                    wxNativeEncodingInfo *info);

// test for the existence of the font described by this facename/encoding,
// return TRUE if such font(s) exist, FALSE otherwise
extern bool wxTestFontEncoding(const wxNativeEncodingInfo& info);

// ----------------------------------------------------------------------------
// font-related functions (X and GTK)
// ----------------------------------------------------------------------------

#ifdef _WX_X_FONTLIKE
    #include "wx/unix/fontutil.h"
#endif // X || GDK

// ----------------------------------------------------------------------------
// font-related functions (MGL)
// ----------------------------------------------------------------------------

#ifdef __WXMGL__
    #include "wx/mgl/fontutil.h"
#endif // __WXMGL__

#endif // _WX_FONTUTIL_H_
