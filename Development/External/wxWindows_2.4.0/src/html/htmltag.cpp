/////////////////////////////////////////////////////////////////////////////
// Name:        htmltag.cpp
// Purpose:     wxHtmlTag class (represents single tag)
// Author:      Vaclav Slavik
// RCS-ID:      $Id: htmltag.cpp,v 1.30.2.1 2002/11/04 22:46:22 VZ Exp $
// Copyright:   (c) 1999 Vaclav Slavik
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////


#ifdef __GNUG__
#pragma implementation "htmltag.h"
#endif

#include "wx/wxprec.h"

#include "wx/defs.h"
#if wxUSE_HTML

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WXPRECOMP
#endif

#include "wx/html/htmltag.h"
#include "wx/html/htmlpars.h"
#include "wx/colour.h"
#include <stdio.h> // for vsscanf
#include <stdarg.h>


//-----------------------------------------------------------------------------
// wxHtmlTagsCache
//-----------------------------------------------------------------------------

struct wxHtmlCacheItem
{
    // this is "pos" value passed to wxHtmlTag's constructor.
    // it is position of '<' character of the tag
    int Key;

    // end positions for the tag:
    // end1 is '<' of ending tag,
    // end2 is '>' or both are
    // -1 if there is no ending tag for this one...
    // or -2 if this is ending tag  </...>
    int End1, End2;

    // name of this tag
    wxChar *Name;
};


IMPLEMENT_CLASS(wxHtmlTagsCache,wxObject)

#define CACHE_INCREMENT  64

wxHtmlTagsCache::wxHtmlTagsCache(const wxString& source)
{
    const wxChar *src = source.c_str();
    int tg, stpos;
    int lng = source.Length();
    wxChar tagBuffer[256];

    m_Cache = NULL;
    m_CacheSize = 0;
    m_CachePos = 0;

    int pos = 0;
    while (pos < lng)
    {
        if (src[pos] == wxT('<'))   // tag found:
        {
            if (m_CacheSize % CACHE_INCREMENT == 0)
                m_Cache = (wxHtmlCacheItem*) realloc(m_Cache, (m_CacheSize + CACHE_INCREMENT) * sizeof(wxHtmlCacheItem));
            tg = m_CacheSize++;
            m_Cache[tg].Key = stpos = pos++;

            int i;
            for ( i = 0;
                  pos < lng && i < (int)WXSIZEOF(tagBuffer) - 1 &&
                  src[pos] != wxT('>') && !wxIsspace(src[pos]);
                  i++, pos++ )
            {
                tagBuffer[i] = wxToupper(src[pos]);
            }
            tagBuffer[i] = _T('\0');

            m_Cache[tg].Name = new wxChar[i+1];
            memcpy(m_Cache[tg].Name, tagBuffer, (i+1)*sizeof(wxChar));

            while (pos < lng && src[pos] != wxT('>')) pos++;

            if (src[stpos+1] == wxT('/')) // ending tag:
            {
                m_Cache[tg].End1 = m_Cache[tg].End2 = -2;
                // find matching begin tag:
                for (i = tg; i >= 0; i--)
                    if ((m_Cache[i].End1 == -1) && (wxStrcmp(m_Cache[i].Name, tagBuffer+1) == 0))
                    {
                        m_Cache[i].End1 = stpos;
                        m_Cache[i].End2 = pos + 1;
                        break;
                    }
            }
            else
            {
                m_Cache[tg].End1 = m_Cache[tg].End2 = -1;
            }
        }

        pos++;
    }

    // ok, we're done, now we'll free .Name members of cache - we don't need it anymore:
    for (int i = 0; i < m_CacheSize; i++)
    {
        delete[] m_Cache[i].Name;
        m_Cache[i].Name = NULL;
    }
}

void wxHtmlTagsCache::QueryTag(int at, int* end1, int* end2)
{
    if (m_Cache == NULL) return;
    if (m_Cache[m_CachePos].Key != at)
    {
        int delta = (at < m_Cache[m_CachePos].Key) ? -1 : 1;
        do
        {
            m_CachePos += delta;
        }
        while (m_Cache[m_CachePos].Key != at);
    }
    *end1 = m_Cache[m_CachePos].End1;
    *end2 = m_Cache[m_CachePos].End2;
}




//-----------------------------------------------------------------------------
// wxHtmlTag
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS(wxHtmlTag,wxObject)

wxHtmlTag::wxHtmlTag(wxHtmlTag *parent,
                     const wxString& source, int pos, int end_pos,
                     wxHtmlTagsCache *cache,
                     wxHtmlEntitiesParser *entParser) : wxObject()
{
    /* Setup DOM relations */

    m_Next = NULL;
    m_FirstChild = m_LastChild = NULL;
    m_Parent = parent;
    if (parent)
    {
        m_Prev = m_Parent->m_LastChild;
        if (m_Prev == NULL)
            m_Parent->m_FirstChild = this;
        else
            m_Prev->m_Next = this;
        m_Parent->m_LastChild = this;
    }
    else
        m_Prev = NULL;

    /* Find parameters and their values: */

    int i;
    wxChar c;

    // fill-in name, params and begin pos:
    i = pos+1;

    // find tag's name and convert it to uppercase:
    while ((i < end_pos) &&
           ((c = source[i++]) != wxT(' ') && c != wxT('\r') &&
             c != wxT('\n') && c != wxT('\t') &&
             c != wxT('>')))
    {
        if ((c >= wxT('a')) && (c <= wxT('z')))
            c -= (wxT('a') - wxT('A'));
        m_Name << c;
    }

    // if the tag has parameters, read them and "normalize" them,
    // i.e. convert to uppercase, replace whitespaces by spaces and
    // remove whitespaces around '=':
    if (source[i-1] != wxT('>'))
    {
        #define IS_WHITE(c) (c == wxT(' ') || c == wxT('\r') || \
                             c == wxT('\n') || c == wxT('\t'))
        wxString pname, pvalue;
        wxChar quote;
        enum
        {
            ST_BEFORE_NAME = 1,
            ST_NAME,
            ST_BEFORE_EQ,
            ST_BEFORE_VALUE,
            ST_VALUE
        } state;

        quote = 0;
        state = ST_BEFORE_NAME;
        while (i < end_pos)
        {
            c = source[i++];

            if (c == wxT('>') && !(state == ST_VALUE && quote != 0))
            {
                if (state == ST_BEFORE_EQ || state == ST_NAME)
                {
                    m_ParamNames.Add(pname);
                    m_ParamValues.Add(wxEmptyString);
                }
                else if (state == ST_VALUE && quote == 0)
                {
                    m_ParamNames.Add(pname);
                    if (entParser)
                        m_ParamValues.Add(entParser->Parse(pvalue));
                    else
                        m_ParamValues.Add(pvalue);
                }
                break;
            }
            switch (state)
            {
                case ST_BEFORE_NAME:
                    if (!IS_WHITE(c))
                    {
                        pname = c;
                        state = ST_NAME;
                    }
                    break;
                case ST_NAME:
                    if (IS_WHITE(c))
                        state = ST_BEFORE_EQ;
                    else if (c == wxT('='))
                        state = ST_BEFORE_VALUE;
                    else
                        pname << c;
                    break;
                case ST_BEFORE_EQ:
                    if (c == wxT('='))
                        state = ST_BEFORE_VALUE;
                    else if (!IS_WHITE(c))
                    {
                        m_ParamNames.Add(pname);
                        m_ParamValues.Add(wxEmptyString);
                        pname = c;
                        state = ST_NAME;
                    }
                    break;
                case ST_BEFORE_VALUE:
                    if (!IS_WHITE(c))
                    {
                        if (c == wxT('"') || c == wxT('\''))
                            quote = c, pvalue = wxEmptyString;
                        else
                            quote = 0, pvalue = c;
                        state = ST_VALUE;
                    }
                    break;
                case ST_VALUE:
                    if ((quote != 0 && c == quote) ||
                        (quote == 0 && IS_WHITE(c)))
                    {
                        m_ParamNames.Add(pname);
                        if (quote == 0)
                        {
                            // VS: backward compatibility, no real reason,
                            //     but wxHTML code relies on this... :(
                            pvalue.MakeUpper();
                        }
                        if (entParser)
                            m_ParamValues.Add(entParser->Parse(pvalue));
                        else
                            m_ParamValues.Add(pvalue);
                        state = ST_BEFORE_NAME;
                    }
                    else
                        pvalue << c;
                    break;
            }
        }

        #undef IS_WHITE
   }
   m_Begin = i;

   cache->QueryTag(pos, &m_End1, &m_End2);
   if (m_End1 > end_pos) m_End1 = end_pos;
   if (m_End2 > end_pos) m_End2 = end_pos;
}

wxHtmlTag::~wxHtmlTag()
{
    wxHtmlTag *t1, *t2;
    t1 = m_FirstChild;
    while (t1)
    {
        t2 = t1->GetNextSibling();
        delete t1;
        t1 = t2;
    }
}

bool wxHtmlTag::HasParam(const wxString& par) const
{
    return (m_ParamNames.Index(par, FALSE) != wxNOT_FOUND);
}

wxString wxHtmlTag::GetParam(const wxString& par, bool with_commas) const
{
    int index = m_ParamNames.Index(par, FALSE);
    if (index == wxNOT_FOUND)
        return wxEmptyString;
    if (with_commas)
    {
        // VS: backward compatibility, seems to be never used by wxHTML...
        wxString s;
        s << wxT('"') << m_ParamValues[index] << wxT('"');
        return s;
    }
    else
        return m_ParamValues[index];
}

int wxHtmlTag::ScanParam(const wxString& par,
                         const wxChar *format,
                         void *param) const
{
    wxString parval = GetParam(par);
    return wxSscanf(parval, format, param);
}

bool wxHtmlTag::GetParamAsColour(const wxString& par, wxColour *clr) const
{
    wxString str = GetParam(par);

    if (str.IsEmpty()) return FALSE;
    if (str.GetChar(0) == wxT('#'))
    {
        unsigned long tmp;
        if (ScanParam(par, wxT("#%lX"), &tmp) != 1)
            return FALSE;
        *clr = wxColour((unsigned char)((tmp & 0xFF0000) >> 16),
					    (unsigned char)((tmp & 0x00FF00) >> 8),
					    (unsigned char)(tmp & 0x0000FF));
        return TRUE;
    }
    else
    {
        // Handle colours defined in HTML 4.0:
        #define HTML_COLOUR(name,r,g,b)                 \
            if (str.IsSameAs(wxT(name), FALSE))         \
                { *clr = wxColour(r,g,b); return TRUE; }
        HTML_COLOUR("black",   0x00,0x00,0x00)
        HTML_COLOUR("silver",  0xC0,0xC0,0xC0)
        HTML_COLOUR("gray",    0x80,0x80,0x80)
        HTML_COLOUR("white",   0xFF,0xFF,0xFF)
        HTML_COLOUR("maroon",  0x80,0x00,0x00)
        HTML_COLOUR("red",     0xFF,0x00,0x00)
        HTML_COLOUR("purple",  0x80,0x00,0x80)
        HTML_COLOUR("fuchsia", 0xFF,0x00,0xFF)
        HTML_COLOUR("green",   0x00,0x80,0x00)
        HTML_COLOUR("lime",    0x00,0xFF,0x00)
        HTML_COLOUR("olive",   0x80,0x80,0x00)
        HTML_COLOUR("yellow",  0xFF,0xFF,0x00)
        HTML_COLOUR("navy",    0x00,0x00,0x80)
        HTML_COLOUR("blue",    0x00,0x00,0xFF)
        HTML_COLOUR("teal",    0x00,0x80,0x80)
        HTML_COLOUR("aqua",    0x00,0xFF,0xFF)
        #undef HTML_COLOUR
    }

    return FALSE;
}

bool wxHtmlTag::GetParamAsInt(const wxString& par, int *clr) const
{
    if (!HasParam(par)) return FALSE;
    long i;
    bool succ = GetParam(par).ToLong(&i);
    *clr = (int)i;
    return succ;
}

wxString wxHtmlTag::GetAllParams() const
{
    // VS: this function is for backward compatiblity only,
    //     never used by wxHTML
    wxString s;
    size_t cnt = m_ParamNames.GetCount();
    for (size_t i = 0; i < cnt; i++)
    {
        s << m_ParamNames[i];
        s << wxT('=');
        if (m_ParamValues[i].Find(wxT('"')) != wxNOT_FOUND)
            s << wxT('\'') << m_ParamValues[i] << wxT('\'');
        else
            s << wxT('"') << m_ParamValues[i] << wxT('"');
    }
    return s;
}

wxHtmlTag *wxHtmlTag::GetFirstSibling() const
{
    if (m_Parent)
        return m_Parent->m_FirstChild;
    else
    {
        wxHtmlTag *cur = (wxHtmlTag*)this;
        while (cur->m_Prev)
            cur = cur->m_Prev;
        return cur;
    }
}

wxHtmlTag *wxHtmlTag::GetLastSibling() const
{
    if (m_Parent)
        return m_Parent->m_LastChild;
    else
    {
        wxHtmlTag *cur = (wxHtmlTag*)this;
        while (cur->m_Next)
            cur = cur->m_Next;
        return cur;
    }
}

wxHtmlTag *wxHtmlTag::GetNextTag() const
{
    if (m_FirstChild) return m_FirstChild;
    if (m_Next) return m_Next;
    wxHtmlTag *cur = m_Parent;
    if (!cur) return NULL;
    while (cur->m_Parent && !cur->m_Next)
        cur = cur->m_Parent;
    return cur->m_Next;
}

#endif
