///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/regex.cpp
// Purpose:     regular expression matching
// Author:      Karsten Ball�der and Vadim Zeitlin
// Modified by:
// Created:     13.07.01
// RCS-ID:      $Id: regex.cpp,v 1.8.2.1 2002/11/09 00:25:37 VS Exp $
// Copyright:   (c) 2000 Karsten Ball�der <ballueder@gmx.net>
//                  2001 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "regex.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_REGEX

#ifndef WX_PRECOMP
    #include "wx/object.h"
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/intl.h"
#endif //WX_PRECOMP

// FreeBSD & Watcom require this, it probably doesn't hurt for others
#if defined(__UNIX__) || defined(__WATCOMC__)
    #include <sys/types.h>
#endif

#include <regex.h>

#include "wx/regex.h"

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// the real implementation of wxRegEx
class wxRegExImpl
{
public:
    // ctor and dtor
    wxRegExImpl();
    ~wxRegExImpl();

    // return TRUE if Compile() had been called successfully
    bool IsValid() const { return m_isCompiled; }

    // RE operations
    bool Compile(const wxString& expr, int flags = 0);
    bool Matches(const wxChar *str, int flags = 0) const;
    bool GetMatch(size_t *start, size_t *len, size_t index = 0) const;
    int Replace(wxString *pattern, const wxString& replacement,
                size_t maxMatches = 0) const;

private:
    // return the string containing the error message for the given err code
    wxString GetErrorMsg(int errorcode) const;

    // free the RE if compiled
    void Free()
    {
        if ( IsValid() )
        {
            regfree(&m_RegEx);

            m_isCompiled = FALSE;
        }
    }

    // compiled RE
    regex_t     m_RegEx;

    // the subexpressions data
    regmatch_t *m_Matches;
    size_t      m_nMatches;

    // TRUE if m_RegEx is valid
    bool        m_isCompiled;
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxRegExImpl
// ----------------------------------------------------------------------------

wxRegExImpl::wxRegExImpl()
{
    m_isCompiled = FALSE;
    m_Matches = NULL;
    m_nMatches = 0;
}

wxRegExImpl::~wxRegExImpl()
{
    Free();

    delete [] m_Matches;
}

wxString wxRegExImpl::GetErrorMsg(int errorcode) const
{
    wxString msg;

    // first get the string length needed
    int len = regerror(errorcode, &m_RegEx, NULL, 0);
    if ( len > 0 )
    {
        len++;

#if wxUSE_UNICODE
        wxCharBuffer buf(len);

        (void)regerror(errorcode, &m_RegEx, (char *)buf.data(), len);

        msg = wxString(buf.data(), wxConvLibc);
#else // !Unicode
        (void)regerror(errorcode, &m_RegEx, msg.GetWriteBuf(len), len);

        msg.UngetWriteBuf();
#endif // Unicode/!Unicode
    }
    else // regerror() returned 0
    {
        msg = _("unknown error");
    }

    return msg;
}

bool wxRegExImpl::Compile(const wxString& expr, int flags)
{
    Free();

    // translate our flags to regcomp() ones
    wxASSERT_MSG( !(flags &
                        ~(wxRE_BASIC | wxRE_ICASE | wxRE_NOSUB | wxRE_NEWLINE)),
                  _T("unrecognized flags in wxRegEx::Compile") );

    int flagsRE = 0;
    if ( !(flags & wxRE_BASIC) )
        flagsRE |= REG_EXTENDED;
    if ( flags & wxRE_ICASE )
        flagsRE |= REG_ICASE;
    if ( flags & wxRE_NOSUB )
        flagsRE |= REG_NOSUB;
    if ( flags & wxRE_NEWLINE )
        flagsRE |= REG_NEWLINE;

    // compile it
    int errorcode = regcomp(&m_RegEx, expr.mb_str(), flagsRE);
    if ( errorcode )
    {
        wxLogError(_("Invalid regular expression '%s': %s"),
                   expr.c_str(), GetErrorMsg(errorcode).c_str());

        m_isCompiled = FALSE;
    }
    else // ok
    {
        // don't allocate the matches array now, but do it later if necessary
        if ( flags & wxRE_NOSUB )
        {
            // we don't need it at all
            m_nMatches = 0;
        }
        else
        {
            // we will alloc the array later (only if really needed) but count
            // the number of sub-expressions in the regex right now

            // there is always one for the whole expression
            m_nMatches = 1;

            // and some more for bracketed subexperessions
            const wxChar *cptr = expr.c_str();
            wxChar prev = _T('\0');
            while ( *cptr != _T('\0') )
            {
                // is this a subexpr start, i.e. "(" for extended regex or
                // "\(" for a basic one?
                if ( *cptr == _T('(') &&
                     (flags & wxRE_BASIC ? prev == _T('\\')
                                         : prev != _T('\\')) )
                {
                    m_nMatches++;
                }

                prev = *cptr;
                cptr++;
            }
        }

        m_isCompiled = TRUE;
    }

    return IsValid();
}

bool wxRegExImpl::Matches(const wxChar *str, int flags) const
{
    wxCHECK_MSG( IsValid(), FALSE, _T("must successfully Compile() first") );

    // translate our flags to regexec() ones
    wxASSERT_MSG( !(flags & ~(wxRE_NOTBOL | wxRE_NOTEOL)),
                  _T("unrecognized flags in wxRegEx::Matches") );

    int flagsRE = 0;
    if ( flags & wxRE_NOTBOL )
        flagsRE |= REG_NOTBOL;
    if ( flags & wxRE_NOTEOL )
        flagsRE |= REG_NOTEOL;

    // allocate matches array if needed
    wxRegExImpl *self = wxConstCast(this, wxRegExImpl);
    if ( !m_Matches && m_nMatches )
    {
        self->m_Matches = new regmatch_t[m_nMatches];
    }

    // do match it
    int rc = regexec(&self->m_RegEx, wxConvertWX2MB(str), m_nMatches, m_Matches, flagsRE);

    switch ( rc )
    {
        case 0:
            // matched successfully
            return TRUE;

        default:
            // an error occured
            wxLogError(_("Failed to match '%s' in regular expression: %s"),
                       str, GetErrorMsg(rc).c_str());
            // fall through

        case REG_NOMATCH:
            // no match
            return FALSE;
    }
}

bool wxRegExImpl::GetMatch(size_t *start, size_t *len, size_t index) const
{
    wxCHECK_MSG( IsValid(), FALSE, _T("must successfully Compile() first") );
    wxCHECK_MSG( m_Matches, FALSE, _T("can't use with wxRE_NOSUB") );
    wxCHECK_MSG( index < m_nMatches, FALSE, _T("invalid match index") );

    const regmatch_t& match = m_Matches[index];

    if ( start )
        *start = match.rm_so;
    if ( len )
        *len = match.rm_eo - match.rm_so;

    return TRUE;
}

int wxRegExImpl::Replace(wxString *text,
                         const wxString& replacement,
                         size_t maxMatches) const
{
    wxCHECK_MSG( text, -1, _T("NULL text in wxRegEx::Replace") );
    wxCHECK_MSG( IsValid(), -1, _T("must successfully Compile() first") );

    // the replacement text
    wxString textNew;

    // attempt at optimization: don't iterate over the string if it doesn't
    // contain back references at all
    bool mayHaveBackrefs =
        replacement.find_first_of(_T("\\&")) != wxString::npos;

    if ( !mayHaveBackrefs )
    {
        textNew = replacement;
    }

    // the position where we start looking for the match
    //
    // NB: initial version had a nasty bug because it used a wxChar* instead of
    //     an index but the problem is that replace() in the loop invalidates
    //     all pointers into the string so we have to use indices instead
    size_t matchStart = 0;

    // number of replacement made: we won't make more than maxMatches of them
    // (unless maxMatches is 0 which doesn't limit the number of replacements)
    size_t countRepl = 0;

    // note that "^" shouldn't match after the first call to Matches() so we
    // use wxRE_NOTBOL to prevent it from happening
    while ( (!maxMatches || countRepl < maxMatches) &&
            Matches(text->c_str() + matchStart, countRepl ? wxRE_NOTBOL : 0) )
    {
        // the string possibly contains back references: we need to calculate
        // the replacement text anew after each match
        if ( mayHaveBackrefs )
        {
            mayHaveBackrefs = FALSE;
            textNew.clear();
            textNew.reserve(replacement.length());

            for ( const wxChar *p = replacement.c_str(); *p; p++ )
            {
                size_t index = (size_t)-1;

                if ( *p == _T('\\') )
                {
                    if ( wxIsdigit(*++p) )
                    {
                        // back reference
                        wxChar *end;
                        index = (size_t)wxStrtoul(p, &end, 10);
                        p = end - 1; // -1 to compensate for p++ in the loop
                    }
                    //else: backslash used as escape character
                }
                else if ( *p == _T('&') )
                {
                    // treat this as "\0" for compatbility with ed and such
                    index = 0;
                }

                // do we have a back reference?
                if ( index != (size_t)-1 )
                {
                    // yes, get its text
                    size_t start, len;
                    if ( !GetMatch(&start, &len, index) )
                    {
                        wxFAIL_MSG( _T("invalid back reference") );

                        // just eat it...
                    }
                    else
                    {
                        textNew += wxString(text->c_str() + matchStart + start,
                                            len);

                        mayHaveBackrefs = TRUE;
                    }
                }
                else // ordinary character
                {
                    textNew += *p;
                }
            }
        }

        size_t start, len;
        if ( !GetMatch(&start, &len) )
        {
            // we did have match as Matches() returned true above!
            wxFAIL_MSG( _T("internal logic error in wxRegEx::Replace") );

            return -1;
        }

        matchStart += start;
        text->replace(matchStart, len, textNew);

        countRepl++;

        matchStart += textNew.length();
    }

    return countRepl;
}

// ----------------------------------------------------------------------------
// wxRegEx: all methods are mostly forwarded to wxRegExImpl
// ----------------------------------------------------------------------------

void wxRegEx::Init()
{
    m_impl = NULL;
}


wxRegEx::~wxRegEx()
{
    delete m_impl;
}

bool wxRegEx::Compile(const wxString& expr, int flags)
{
    if ( !m_impl )
    {
        m_impl = new wxRegExImpl;
    }

    if ( !m_impl->Compile(expr, flags) )
    {
        // error message already given in wxRegExImpl::Compile
        delete m_impl;
        m_impl = NULL;

        return FALSE;
    }

    return TRUE;
}

bool wxRegEx::Matches(const wxChar *str, int flags) const
{
    wxCHECK_MSG( IsValid(), FALSE, _T("must successfully Compile() first") );

    return m_impl->Matches(str, flags);
}

bool wxRegEx::GetMatch(size_t *start, size_t *len, size_t index) const
{
    wxCHECK_MSG( IsValid(), FALSE, _T("must successfully Compile() first") );

    return m_impl->GetMatch(start, len, index);
}

wxString wxRegEx::GetMatch(const wxString& text, size_t index) const
{
    size_t start, len;
    if ( !GetMatch(&start, &len, index) )
        return wxEmptyString;

    return text.Mid(start, len);
}

int wxRegEx::Replace(wxString *pattern,
                     const wxString& replacement,
                     size_t maxMatches) const
{
    wxCHECK_MSG( IsValid(), -1, _T("must successfully Compile() first") );

    return m_impl->Replace(pattern, replacement, maxMatches);
}

#endif // wxUSE_REGEX
