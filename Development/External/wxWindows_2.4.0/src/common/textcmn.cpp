///////////////////////////////////////////////////////////////////////////////
// Name:        common/textcmn.cpp
// Purpose:     implementation of platform-independent functions of wxTextCtrl
// Author:      Julian Smart
// Modified by:
// Created:     13.07.99
// RCS-ID:      $Id: textcmn.cpp,v 1.22 2002/08/05 18:04:07 RR Exp $
// Copyright:   (c) wxWindows team
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

#ifdef __GNUG__
    #pragma implementation "textctrlbase.h"
#endif

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_TEXTCTRL

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/textctrl.h"
#endif // WX_PRECOMP

#include "wx/ffile.h"

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

// we don't have any objects of type wxTextCtrlBase in the program, only
// wxTextCtrl, so this cast is safe
#define TEXTCTRL(ptr)   ((wxTextCtrl *)(ptr))

// ============================================================================
// implementation
// ============================================================================

IMPLEMENT_DYNAMIC_CLASS(wxTextUrlEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_COMMAND_TEXT_UPDATED)
DEFINE_EVENT_TYPE(wxEVT_COMMAND_TEXT_ENTER)
DEFINE_EVENT_TYPE(wxEVT_COMMAND_TEXT_URL)
DEFINE_EVENT_TYPE(wxEVT_COMMAND_TEXT_MAXLEN)

// ----------------------------------------------------------------------------
// ctor
// ----------------------------------------------------------------------------

wxTextCtrlBase::wxTextCtrlBase()
{
}

wxTextCtrlBase::~wxTextCtrlBase()
{
}

// ----------------------------------------------------------------------------
// style functions - not implemented here
// ----------------------------------------------------------------------------

/* static */
wxTextAttr wxTextAttr::Combine(const wxTextAttr& attr,
                               const wxTextAttr& attrDef,
                               const wxTextCtrlBase *text)
{
    wxFont font = attr.GetFont();
    if ( !font.Ok() )
    {
        font = attrDef.GetFont();

        if ( text && !font.Ok() )
            font = text->GetFont();
    }

    wxColour colFg = attr.GetTextColour();
    if ( !colFg.Ok() )
    {
        colFg = attrDef.GetTextColour();

        if ( text && !colFg.Ok() )
            colFg = text->GetForegroundColour();
    }

    wxColour colBg = attr.GetBackgroundColour();
    if ( !colBg.Ok() )
    {
        colBg = attrDef.GetBackgroundColour();

        if ( text && !colBg.Ok() )
            colBg = text->GetBackgroundColour();
    }

    return wxTextAttr(colFg, colBg, font);
}

// apply styling to text range
bool wxTextCtrlBase::SetStyle(long WXUNUSED(start), long WXUNUSED(end),
                              const wxTextAttr& WXUNUSED(style))
{
    // to be implemented in derived TextCtrl classes
    return FALSE;
}

// change default text attributes
bool wxTextCtrlBase::SetDefaultStyle(const wxTextAttr& style)
{
    // keep the old attributes if the new style doesn't specify them unless the
    // new style is empty - then reset m_defaultStyle (as there is no other way
    // to do it)
    if ( style.IsDefault() )
        m_defaultStyle = style;
    else
        m_defaultStyle = wxTextAttr::Combine(style, m_defaultStyle, this);

    return TRUE;
}

// get default text attributes
const wxTextAttr& wxTextCtrlBase::GetDefaultStyle() const
{
    return m_defaultStyle;
}

// ----------------------------------------------------------------------------
// file IO functions
// ----------------------------------------------------------------------------

bool wxTextCtrlBase::LoadFile(const wxString& filename)
{
#if wxUSE_FFILE
    wxFFile file(filename);
    if ( file.IsOpened() )
    {
        wxString text;
        if ( file.ReadAll(&text) )
        {
            SetValue(text);

            DiscardEdits();

            m_filename = filename;

            return TRUE;
        }
    }

    wxLogError(_("File couldn't be loaded."));
#endif // wxUSE_FFILE

    return FALSE;
}

bool wxTextCtrlBase::SaveFile(const wxString& filename)
{
    wxString filenameToUse = filename.IsEmpty() ? m_filename : filename;
    if ( !filenameToUse )
    {
        // what kind of message to give? is it an error or a program bug?
        wxLogDebug(wxT("Can't save textctrl to file without filename."));

        return FALSE;
    }

#if wxUSE_FFILE
    wxFFile file(filename, "w");
    if ( file.IsOpened() && file.Write(GetValue()) )
    {
        // it's not modified any longer
        DiscardEdits();

        m_filename = filename;

        return TRUE;
    }

    wxLogError(_("The text couldn't be saved."));
#endif // wxUSE_FFILE

    return FALSE;
}

// ----------------------------------------------------------------------------
// stream-like insertion operator
// ----------------------------------------------------------------------------

wxTextCtrl& wxTextCtrlBase::operator<<(const wxString& s)
{
    AppendText(s);
    return *TEXTCTRL(this);
}

wxTextCtrl& wxTextCtrlBase::operator<<(float f)
{
    wxString str;
    str.Printf(wxT("%.2f"), f);
    AppendText(str);
    return *TEXTCTRL(this);
}

wxTextCtrl& wxTextCtrlBase::operator<<(double d)
{
    wxString str;
    str.Printf(wxT("%.2f"), d);
    AppendText(str);
    return *TEXTCTRL(this);
}

wxTextCtrl& wxTextCtrlBase::operator<<(int i)
{
    wxString str;
    str.Printf(wxT("%d"), i);
    AppendText(str);
    return *TEXTCTRL(this);
}

wxTextCtrl& wxTextCtrlBase::operator<<(long i)
{
    wxString str;
    str.Printf(wxT("%ld"), i);
    AppendText(str);
    return *TEXTCTRL(this);
}

wxTextCtrl& wxTextCtrlBase::operator<<(const wxChar c)
{
    return operator<<(wxString(c));
}

// ----------------------------------------------------------------------------
// streambuf methods implementation
// ----------------------------------------------------------------------------

#ifndef NO_TEXT_WINDOW_STREAM

int wxTextCtrlBase::overflow(int c)
{
    AppendText((wxChar)c);

    // return something different from EOF
    return 0;
}

#endif // NO_TEXT_WINDOW_STREAM

// ----------------------------------------------------------------------------
// clipboard stuff
// ----------------------------------------------------------------------------

bool wxTextCtrlBase::CanCopy() const
{
    // can copy if there's a selection
    long from, to;
    GetSelection(&from, &to);
    return from != to;
}

bool wxTextCtrlBase::CanCut() const
{
    // can cut if there's a selection and if we're not read only
    return CanCopy() && IsEditable();
}

bool wxTextCtrlBase::CanPaste() const
{
    // can paste if we are not read only
    return IsEditable();
}

// ----------------------------------------------------------------------------
// emulating key presses
// ----------------------------------------------------------------------------

bool wxTextCtrlBase::EmulateKeyPress(const wxKeyEvent& event)
{
    // the generic version is unused in wxMSW
#ifndef __WIN32__
    wxChar ch;
    int keycode = event.GetKeyCode();
    switch ( keycode )
    {
        case WXK_NUMPAD0:
        case WXK_NUMPAD1:
        case WXK_NUMPAD2:
        case WXK_NUMPAD3:
        case WXK_NUMPAD4:
        case WXK_NUMPAD5:
        case WXK_NUMPAD6:
        case WXK_NUMPAD7:
        case WXK_NUMPAD8:
        case WXK_NUMPAD9:
            ch = _T('0') + keycode - WXK_NUMPAD0;
            break;

        case WXK_MULTIPLY:
        case WXK_NUMPAD_MULTIPLY:
            ch = _T('*');
            break;

        case WXK_ADD:
        case WXK_NUMPAD_ADD:
            ch = _T('+');
            break;

        case WXK_SUBTRACT:
        case WXK_NUMPAD_SUBTRACT:
            ch = _T('-');
            break;

        case WXK_DECIMAL:
        case WXK_NUMPAD_DECIMAL:
            ch = _T('.');
            break;

        case WXK_DIVIDE:
        case WXK_NUMPAD_DIVIDE:
            ch = _T('/');
            break;

        default:
            if ( keycode < 256 && keycode >= 0 && wxIsprint(keycode) )
            {
                // FIXME this is not going to work for non letters...
                if ( !event.ShiftDown() )
                {
                    keycode = wxTolower(keycode);
                }

                ch = (wxChar)keycode;
            }
            else
            {
                ch = _T('\0');
            }
    }

    if ( ch )
    {
        WriteText(ch);

        return TRUE;
    }
#endif // !__WIN32__

    return FALSE;
}

// ----------------------------------------------------------------------------
// selection and ranges
// ----------------------------------------------------------------------------

void wxTextCtrlBase::SelectAll()
{
    SetSelection(0, GetLastPosition());
}

wxString wxTextCtrlBase::GetStringSelection() const
{
    long from, to;
    GetSelection(&from, &to);

    return GetRange(from, to);
}

wxString wxTextCtrlBase::GetRange(long from, long to) const
{
    wxString sel;
    if ( from < to )
    {
        sel = GetValue().Mid(from, to - from);
    }

    return sel;
}

#else // !wxUSE_TEXTCTRL

// define this one even if !wxUSE_TEXTCTRL because it is also used by other
// controls (wxComboBox and wxSpinCtrl)
#include "wx/event.h"

DEFINE_EVENT_TYPE(wxEVT_COMMAND_TEXT_UPDATED)

#endif // wxUSE_TEXTCTRL/!wxUSE_TEXTCTRL

