/////////////////////////////////////////////////////////////////////////////
// Name:        font.h
// Purpose:     wxFont class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: font.h,v 1.19.2.1 2002/09/22 20:58:43 VZ Exp $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FONT_H_
#define _WX_FONT_H_

#ifdef __GNUG__
    #pragma interface "font.h"
#endif

// ----------------------------------------------------------------------------
// wxFont
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxFont : public wxFontBase
{
public:
    // ctors and such
    wxFont() { Init(); }
    wxFont(const wxFont& font) { Init(); Ref(font); }

    wxFont(int size,
           int family,
           int style,
           int weight,
           bool underlined = FALSE,
           const wxString& face = wxEmptyString,
           wxFontEncoding encoding = wxFONTENCODING_DEFAULT)
    {
        Init();

        (void)Create(size, family, style, weight, underlined, face, encoding);
    }

    wxFont(const wxNativeFontInfo& info, WXHFONT hFont = 0)
    {
        Init();

        Create(info, hFont);
    }

    wxFont(const wxString& fontDesc);

    bool Create(int size,
                int family,
                int style,
                int weight,
                bool underlined = FALSE,
                const wxString& face = wxEmptyString,
                wxFontEncoding encoding = wxFONTENCODING_DEFAULT);

    bool Create(const wxNativeFontInfo& info, WXHFONT hFont = 0);

    virtual ~wxFont();

    // assignment
    wxFont& operator=(const wxFont& font);

    // implement base class pure virtuals
    virtual int GetPointSize() const;
    virtual int GetFamily() const;
    virtual int GetStyle() const;
    virtual int GetWeight() const;
    virtual bool GetUnderlined() const;
    virtual wxString GetFaceName() const;
    virtual wxFontEncoding GetEncoding() const;
    virtual wxNativeFontInfo* GetNativeFontInfo() const;

    virtual void SetPointSize(int pointSize);
    virtual void SetFamily(int family);
    virtual void SetStyle(int style);
    virtual void SetWeight(int weight);
    virtual void SetFaceName(const wxString& faceName);
    virtual void SetUnderlined(bool underlined);
    virtual void SetEncoding(wxFontEncoding encoding);
    virtual void SetNativeFontInfo(const wxNativeFontInfo& info);

    virtual bool IsFixedWidth() const;

    // implementation only from now on
    // -------------------------------

    virtual bool IsFree() const;
    virtual bool RealizeResource();
    virtual WXHANDLE GetResourceHandle() const;
    virtual bool FreeResource(bool force = FALSE);

    // for consistency with other wxMSW classes
    WXHFONT GetHFONT() const;

    /*
       virtual bool UseResource();
       virtual bool ReleaseResource();
     */

protected:
    // common part of all ctors
    void Init();

    void Unshare();

private:
    DECLARE_DYNAMIC_CLASS(wxFont)
};

#endif
    // _WX_FONT_H_
