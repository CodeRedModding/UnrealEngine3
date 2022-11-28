/////////////////////////////////////////////////////////////////////////////
// Name:        common/fontcmn.cpp
// Purpose:     implementation of wxFontBase methods
// Author:      Vadim Zeitlin
// Modified by:
// Created:     20.09.99
// RCS-ID:      $Id: fontcmn.cpp,v 1.30.2.1 2002/12/10 16:10:02 JS Exp $
// Copyright:   (c) wxWindows team
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
#pragma implementation "fontbase.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/font.h"
#include "wx/intl.h"
#endif // WX_PRECOMP

#include "wx/gdicmn.h"
#include "wx/fontutil.h" // for wxNativeFontInfo
#include "wx/fontmap.h"

#include "wx/tokenzr.h"

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxFontBase
// ----------------------------------------------------------------------------

wxFontEncoding wxFontBase::ms_encodingDefault = wxFONTENCODING_SYSTEM;

/* static */
void wxFontBase::SetDefaultEncoding(wxFontEncoding encoding)
{
    // GetDefaultEncoding() should return something != wxFONTENCODING_DEFAULT
    // and, besides, using this value here doesn't make any sense
    wxCHECK_RET( encoding != wxFONTENCODING_DEFAULT,
                 _T("can't set default encoding to wxFONTENCODING_DEFAULT") );

    ms_encodingDefault = encoding;
}

wxFontBase::~wxFontBase()
{
    // this destructor is required for Darwin
}

/* static */
wxFont *wxFontBase::New(int size,
                        int family,
                        int style,
                        int weight,
                        bool underlined,
                        const wxString& face,
                        wxFontEncoding encoding)
{
    return new wxFont(size, family, style, weight, underlined, face, encoding);
}

/* static */
wxFont *wxFontBase::New(const wxNativeFontInfo& info)
{
    return new wxFont(info);
}

/* static */
wxFont *wxFontBase::New(const wxString& strNativeFontDesc)
{
    wxNativeFontInfo fontInfo;
    if ( !fontInfo.FromString(strNativeFontDesc) )
        return new wxFont(*wxNORMAL_FONT);

    return New(fontInfo);
}

bool wxFontBase::IsFixedWidth() const
{
    return GetFamily() == wxFONTFAMILY_TELETYPE;
}

wxNativeFontInfo *wxFontBase::GetNativeFontInfo() const
{
#ifdef wxNO_NATIVE_FONTINFO
    wxNativeFontInfo *fontInfo = new wxNativeFontInfo();

    fontInfo->SetPointSize(GetPointSize());
    fontInfo->SetFamily((wxFontFamily)GetFamily());
    fontInfo->SetStyle((wxFontStyle)GetStyle());
    fontInfo->SetWeight((wxFontWeight)GetWeight());
    fontInfo->SetUnderlined(GetUnderlined());
    fontInfo->SetFaceName(GetFaceName());
    fontInfo->SetEncoding(GetEncoding());

    return fontInfo;
#else
    return (wxNativeFontInfo *)NULL;
#endif
}

void wxFontBase::SetNativeFontInfo(const wxNativeFontInfo& info)
{
#ifdef wxNO_NATIVE_FONTINFO
    SetPointSize(info.pointSize);
    SetFamily(info.family);
    SetStyle(info.style);
    SetWeight(info.weight);
    SetUnderlined(info.underlined);
    SetFaceName(info.faceName);
    SetEncoding(info.encoding);
#else
    (void)info;
#endif
}

wxString wxFontBase::GetNativeFontInfoDesc() const
{
    wxString fontDesc;
    wxNativeFontInfo *fontInfo = GetNativeFontInfo();
    if ( fontInfo )
    {
        fontDesc = fontInfo->ToString();
        delete fontInfo;
    }

    return fontDesc;
}

wxString wxFontBase::GetNativeFontInfoUserDesc() const
{
    wxString fontDesc;
    wxNativeFontInfo *fontInfo = GetNativeFontInfo();
    if ( fontInfo )
    {
        fontDesc = fontInfo->ToUserString();
        delete fontInfo;
    }

    return fontDesc;
}

void wxFontBase::SetNativeFontInfo(const wxString& info)
{
    wxNativeFontInfo fontInfo;
    if ( !info.empty() && fontInfo.FromString(info) )
    {
        SetNativeFontInfo(fontInfo);
    }
}

void wxFontBase::SetNativeFontInfoUserDesc(const wxString& info)
{
    wxNativeFontInfo fontInfo;
    if ( !info.empty() && fontInfo.FromUserString(info) )
    {
        SetNativeFontInfo(fontInfo);
    }
}

wxFont& wxFont::operator=(const wxFont& font)
{
    if ( this != &font )
        Ref(font);

    return (wxFont &)*this;
}

bool wxFontBase::operator==(const wxFont& font) const
{
    // either it is the same font, i.e. they share the same common data or they
    // have different ref datas but still describe the same font
    return GetFontData() == font.GetFontData() ||
           (
            Ok() == font.Ok() &&
            GetPointSize() == font.GetPointSize() &&
            GetFamily() == font.GetFamily() &&
            GetStyle() == font.GetStyle() &&
            GetWeight() == font.GetWeight() &&
            GetUnderlined() == font.GetUnderlined() &&
            GetFaceName() == font.GetFaceName() &&
            GetEncoding() == font.GetEncoding()
           );
}

bool wxFontBase::operator!=(const wxFont& font) const
{
    return !(*this == font);
}

wxString wxFontBase::GetFamilyString() const
{
    wxCHECK_MSG( Ok(), wxT("wxDEFAULT"), wxT("invalid font") );

    switch ( GetFamily() )
    {
        case wxDECORATIVE:   return wxT("wxDECORATIVE");
        case wxROMAN:        return wxT("wxROMAN");
        case wxSCRIPT:       return wxT("wxSCRIPT");
        case wxSWISS:        return wxT("wxSWISS");
        case wxMODERN:       return wxT("wxMODERN");
        case wxTELETYPE:     return wxT("wxTELETYPE");
        default:             return wxT("wxDEFAULT");
    }
}

wxString wxFontBase::GetStyleString() const
{
    wxCHECK_MSG( Ok(), wxT("wxDEFAULT"), wxT("invalid font") );

    switch ( GetStyle() )
    {
        case wxNORMAL:   return wxT("wxNORMAL");
        case wxSLANT:    return wxT("wxSLANT");
        case wxITALIC:   return wxT("wxITALIC");
        default:         return wxT("wxDEFAULT");
    }
}

wxString wxFontBase::GetWeightString() const
{
    wxCHECK_MSG( Ok(), wxT("wxDEFAULT"), wxT("invalid font") );

    switch ( GetWeight() )
    {
        case wxNORMAL:   return wxT("wxNORMAL");
        case wxBOLD:     return wxT("wxBOLD");
        case wxLIGHT:    return wxT("wxLIGHT");
        default:         return wxT("wxDEFAULT");
    }
}

// ----------------------------------------------------------------------------
// wxNativeFontInfo
// ----------------------------------------------------------------------------

#ifdef wxNO_NATIVE_FONTINFO

// These are the generic forms of FromString()/ToString.
//
// convert to/from the string representation: format is
//      version;pointsize;family;style;weight;underlined;facename;encoding

bool wxNativeFontInfo::FromString(const wxString& s)
{
    long l;

    wxStringTokenizer tokenizer(s, _T(";"));

    wxString token = tokenizer.GetNextToken();
    //
    //  Ignore the version for now
    //

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return FALSE;
    pointSize = (int)l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return FALSE;
    family = (wxFontFamily)l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return FALSE;
    style = (wxFontStyle)l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return FALSE;
    weight = (wxFontWeight)l;

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return FALSE;
    underlined = l != 0;

    faceName = tokenizer.GetNextToken();

#ifndef __WXMAC__
    if( !faceName )
        return FALSE;
#endif

    token = tokenizer.GetNextToken();
    if ( !token.ToLong(&l) )
        return FALSE;
    encoding = (wxFontEncoding)l;

    return TRUE;
}

wxString wxNativeFontInfo::ToString() const
{
    wxString s;

    s.Printf(_T("%d;%d;%d;%d;%d;%d;%s;%d"),
             0,                                 // version
             pointSize,
             family,
             (int)style,
             (int)weight,
             underlined,
             faceName.GetData(),
             (int)encoding);

    return s;
}

void wxNativeFontInfo::Init()
{
    pointSize = wxNORMAL_FONT->GetPointSize();
    family = wxFONTFAMILY_DEFAULT;
    style = wxFONTSTYLE_NORMAL;
    weight = wxFONTWEIGHT_NORMAL;
    underlined = FALSE;
    faceName.clear();
    encoding = wxFONTENCODING_DEFAULT;
}

int wxNativeFontInfo::GetPointSize() const
{
    return pointSize;
}

wxFontStyle wxNativeFontInfo::GetStyle() const
{
    return style;
}

wxFontWeight wxNativeFontInfo::GetWeight() const
{
    return weight;
}

bool wxNativeFontInfo::GetUnderlined() const
{
    return underlined;
}

wxString wxNativeFontInfo::GetFaceName() const
{
    return faceName;
}

wxFontFamily wxNativeFontInfo::GetFamily() const
{
    return family;
}

wxFontEncoding wxNativeFontInfo::GetEncoding() const
{
    return encoding;
}

void wxNativeFontInfo::SetPointSize(int pointsize)
{
    pointSize = pointsize;
}

void wxNativeFontInfo::SetStyle(wxFontStyle style_)
{
    style = style_;
}

void wxNativeFontInfo::SetWeight(wxFontWeight weight_)
{
    weight = weight_;
}

void wxNativeFontInfo::SetUnderlined(bool underlined_)
{
    underlined = underlined_;
}

void wxNativeFontInfo::SetFaceName(wxString facename_)
{
    faceName = facename_;
}

void wxNativeFontInfo::SetFamily(wxFontFamily family_)
{
    family = family_;
}

void wxNativeFontInfo::SetEncoding(wxFontEncoding encoding_)
{
    encoding = encoding_;
}

#endif // generic wxNativeFontInfo implementation

// conversion to/from user-readable string: this is used in the generic
// versions and under MSW as well because there is no standard font description
// format there anyhow (but there is a well-defined standard for X11 fonts used
// by wxGTK and wxMotif)

#if defined(wxNO_NATIVE_FONTINFO) || defined(__WXMSW__) || defined (__WXPM__)

wxString wxNativeFontInfo::ToUserString() const
{
    wxString desc;

    // first put the adjectives, if any - this is English-centric, of course,
    // but what else can we do?
    if ( GetUnderlined() )
    {
        desc << _("underlined ");
    }

    switch ( GetWeight() )
    {
        default:
            wxFAIL_MSG( _T("unknown font weight") );
            // fall through

        case wxFONTWEIGHT_NORMAL:
            break;

        case wxFONTWEIGHT_LIGHT:
            desc << _("light ");
            break;

        case wxFONTWEIGHT_BOLD:
            desc << _("bold ");
            break;
    }

    switch ( GetStyle() )
    {
        default:
            wxFAIL_MSG( _T("unknown font style") );
            // fall through

        case wxFONTSTYLE_NORMAL:
            break;

            // we don't distinguish between the two for now anyhow...
        case wxFONTSTYLE_ITALIC:
        case wxFONTSTYLE_SLANT:
            desc << _("italic");
            break;
    }

    wxString face = GetFaceName();
    if ( !face.empty() )
    {
        desc << _T(' ') << face;
    }

    int size = GetPointSize();
    if ( size != wxNORMAL_FONT->GetPointSize() )
    {
        desc << _T(' ') << size;
    }

#if wxUSE_FONTMAP
    wxFontEncoding enc = GetEncoding();
    if ( enc != wxFONTENCODING_DEFAULT && enc != wxFONTENCODING_SYSTEM )
    {
        desc << _T(' ') << wxFontMapper::Get()->GetEncodingName(enc);
    }
#endif // wxUSE_FONTMAP

    return desc;
}

bool wxNativeFontInfo::FromUserString(const wxString& s)
{
    // reset to the default state
    Init();

    // parse a more or less free form string
    //
    // TODO: we should handle at least the quoted facenames
    wxStringTokenizer tokenizer(s, _T(";, "), wxTOKEN_STRTOK);

    wxString face;
    unsigned long size;

#if wxUSE_FONTMAP
    wxFontEncoding encoding;
#endif // wxUSE_FONTMAP

    while ( tokenizer.HasMoreTokens() )
    {
        wxString token = tokenizer.GetNextToken();

        // normalize it
        token.Trim(TRUE).Trim(FALSE).MakeLower();

        // look for the known tokens
        if ( token == _T("underlined") || token == _("underlined") )
        {
            SetUnderlined(TRUE);
        }
        else if ( token == _T("light") || token == _("light") )
        {
            SetWeight(wxFONTWEIGHT_LIGHT);
        }
        else if ( token == _T("bold") || token == _("bold") )
        {
            SetWeight(wxFONTWEIGHT_BOLD);
        }
        else if ( token == _T("italic") || token == _("italic") )
        {
            SetStyle(wxFONTSTYLE_ITALIC);
        }
        else if ( token.ToULong(&size) )
        {
            SetPointSize(size);
        }
#if wxUSE_FONTMAP
        else if ( (encoding = wxFontMapper::Get()->CharsetToEncoding(token, FALSE))
                    != wxFONTENCODING_DEFAULT )
        {
            SetEncoding(encoding);
        }
#endif // wxUSE_FONTMAP
        else // assume it is the face name
        {
            if ( !face.empty() )
            {
                face += _T(' ');
            }

            face += token;

            // skip the code which resets face below
            continue;
        }

        // if we had had the facename, we shouldn't continue appending tokens
        // to it (i.e. "foo bold bar" shouldn't result in the facename "foo
        // bar")
        if ( !face.empty() )
        {
            SetFaceName(face);
            face.clear();
        }
    }

    // we might not have flushed it inside the loop
    if ( !face.empty() )
    {
        SetFaceName(face);
    }

    return TRUE;
}

#endif // generic or wxMSW or wxOS2

