///////////////////////////////////////////////////////////////////////////////
// Name:        wx/regex.h
// Purpose:     regular expression matching
// Author:      Karsten Ball�der
// Modified by: VZ at 13.07.01 (integrated to wxWin)
// Created:     05.02.2000
// RCS-ID:      $Id: regex.h,v 1.6 2002/08/31 11:29:11 GD Exp $
// Copyright:   (c) 2000 Karsten Ball�der <ballueder@gmx.net>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_REGEX_H_
#define _WX_REGEX_H_

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "regex.h"
#endif

#include "wx/defs.h"

#if wxUSE_REGEX

class WXDLLEXPORT wxString;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// flags for regex compilation: these can be used with Compile()
enum
{
    // use extended regex syntax (default)
    wxRE_EXTENDED = 0,

    // use basic RE syntax
    wxRE_BASIC    = 2,

    // ignore case in match
    wxRE_ICASE    = 4,

    // only check match, don't set back references
    wxRE_NOSUB    = 8,

    // if not set, treat '\n' as an ordinary character, otherwise it is
    // special: it is not matched by '.' and '^' and '$' always match
    // after/before it regardless of the setting of wxRE_NOT[BE]OL
    wxRE_NEWLINE  = 16,

    // default flags
    wxRE_DEFAULT  = wxRE_EXTENDED
};

// flags for regex matching: these can be used with Matches()
//
// these flags are mainly useful when doing several matches in a long string,
// they can be used to prevent erroneous matches for '^' and '$'
enum
{
    // '^' doesn't match at the start of line
    wxRE_NOTBOL = 32,

    // '$' doesn't match at the end of line
    wxRE_NOTEOL = 64
};

// ----------------------------------------------------------------------------
// wxRegEx: a regular expression
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxRegExImpl;

class WXDLLEXPORT wxRegEx
{
public:
    // default ctor: use Compile() later
    wxRegEx() { Init(); }

    // create and compile
    wxRegEx(const wxString& expr, int flags = wxRE_DEFAULT)
    {
        Init();
        (void)Compile(expr, flags);
    }

    // return TRUE if this is a valid compiled regular expression
    bool IsValid() const { return m_impl != NULL; }

    // compile the string into regular expression, return TRUE if ok or FALSE
    // if string has a syntax error
    bool Compile(const wxString& pattern, int flags = wxRE_DEFAULT);

    // matches the precompiled regular expression against a string, return
    // TRUE if matches and FALSE otherwise
    //
    // flags may be combination of wxRE_NOTBOL and wxRE_NOTEOL
    //
    // may only be called after successful call to Compile()
    bool Matches(const wxChar *text, int flags = 0) const;

    // get the start index and the length of the match of the expression
    // (index 0) or a bracketed subexpression (index != 0)
    //
    // may only be called after successful call to Matches()
    //
    // return FALSE if no match or on error
    bool GetMatch(size_t *start, size_t *len, size_t index = 0) const;

    // return the part of string corresponding to the match, empty string is
    // returned if match failed
    //
    // may only be called after successful call to Matches()
    wxString GetMatch(const wxString& text, size_t index = 0) const;

    // replaces the current regular expression in the string pointed to by
    // pattern, with the text in replacement and return number of matches
    // replaced (maybe 0 if none found) or -1 on error
    //
    // the replacement text may contain backreferences (\number) which will be
    // replaced with the value of the corresponding subexpression in the
    // pattern match
    //
    // maxMatches may be used to limit the number of replacements made, setting
    // it to 1, for example, will only replace first occurence (if any) of the
    // pattern in the text while default value of 0 means replace all
    int Replace(wxString *text, const wxString& replacement,
                size_t maxMatches = 0) const;

    // replace the first occurence
    int ReplaceFirst(wxString *text, const wxString& replacement) const
        { return Replace(text, replacement, 1); }

    // replace all occurences: this is actually a synonym for Replace()
    int ReplaceAll(wxString *text, const wxString& replacement) const
        { return Replace(text, replacement, 0); }

    // dtor not virtual, don't derive from this class
    ~wxRegEx();

private:
    // common part of all ctors
    void Init();

    // the real guts of this class
    wxRegExImpl *m_impl;

    // as long as the class wxRegExImpl is not ref-counted,
    // instances of the handle wxRegEx must not be copied.
    wxRegEx(const wxRegEx&);
    wxRegEx &operator=(const wxRegEx&);
};

#endif // wxUSE_REGEX

#endif // _WX_REGEX_H_

