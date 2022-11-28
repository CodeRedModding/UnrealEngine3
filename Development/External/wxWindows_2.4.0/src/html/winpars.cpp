/////////////////////////////////////////////////////////////////////////////
// Name:        winpars.cpp
// Purpose:     wxHtmlParser class (generic parser)
// Author:      Vaclav Slavik
// RCS-ID:      $Id: winpars.cpp,v 1.35.2.5 2002/12/30 18:55:33 VS Exp $
// Copyright:   (c) 1999 Vaclav Slavik
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////


#ifdef __GNUG__
#pragma implementation "winpars.h"
#endif

#include "wx/wxprec.h"

#include "wx/defs.h"
#if wxUSE_HTML && wxUSE_STREAMS

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WXPRECOMP
    #include "wx/intl.h"
    #include "wx/dc.h"
#endif

#include "wx/html/htmldefs.h"
#include "wx/html/winpars.h"
#include "wx/html/htmlwin.h"
#include "wx/fontmap.h"
#include "wx/log.h"


//-----------------------------------------------------------------------------
// wxHtmlWinParser
//-----------------------------------------------------------------------------


wxList wxHtmlWinParser::m_Modules;

wxHtmlWinParser::wxHtmlWinParser(wxHtmlWindow *wnd) : wxHtmlParser()
{
    m_tmpStrBuf = NULL;
    m_tmpStrBufSize = 0;
    m_Window = wnd;
    m_Container = NULL;
    m_DC = NULL;
    m_CharHeight = m_CharWidth = 0;
    m_UseLink = FALSE;
#if !wxUSE_UNICODE
    m_EncConv = NULL;
    m_InputEnc = wxFONTENCODING_ISO8859_1;
    m_OutputEnc = wxFONTENCODING_DEFAULT;
#endif

    {
        int i, j, k, l, m;
        for (i = 0; i < 2; i++)
            for (j = 0; j < 2; j++)
                for (k = 0; k < 2; k++)
                    for (l = 0; l < 2; l++)
                        for (m = 0; m < 7; m++)
                        {
                            m_FontsTable[i][j][k][l][m] = NULL;
                            m_FontsFacesTable[i][j][k][l][m] = wxEmptyString;
#if !wxUSE_UNICODE
                            m_FontsEncTable[i][j][k][l][m] = wxFONTENCODING_DEFAULT;
#endif
                        }
#ifdef __WXMSW__
        static int default_sizes[7] = {7, 8, 10, 12, 16, 22, 30};
#elif defined(__WXMAC__)
        static int default_sizes[7] = {9, 12, 14, 18, 24, 30, 36};
#else
        static int default_sizes[7] = {10, 12, 14, 16, 19, 24, 32};
#endif
        SetFonts(wxT(""), wxT(""), default_sizes);
    }

    // fill in wxHtmlParser's tables:
    wxNode *node = m_Modules.GetFirst();
    while (node)
    {
        wxHtmlTagsModule *mod = (wxHtmlTagsModule*) node->GetData();
        mod->FillHandlersTable(this);
        node = node->GetNext();
    }
}

wxHtmlWinParser::~wxHtmlWinParser()
{
    int i, j, k, l, m;

    for (i = 0; i < 2; i++)
        for (j = 0; j < 2; j++)
            for (k = 0; k < 2; k++)
                for (l = 0; l < 2; l++)
                    for (m = 0; m < 7; m++)
                    {
                        if (m_FontsTable[i][j][k][l][m] != NULL)
                            delete m_FontsTable[i][j][k][l][m];
                    }
#if !wxUSE_UNICODE
    delete m_EncConv;
#endif
    delete[] m_tmpStrBuf;
}

void wxHtmlWinParser::AddModule(wxHtmlTagsModule *module)
{
    m_Modules.Append(module);
}

void wxHtmlWinParser::RemoveModule(wxHtmlTagsModule *module)
{
    m_Modules.DeleteObject(module);
}

void wxHtmlWinParser::SetFonts(wxString normal_face, wxString fixed_face, const int *sizes)
{
    int i, j, k, l, m;

    for (i = 0; i < 7; i++) m_FontsSizes[i] = sizes[i];
    m_FontFaceFixed = fixed_face;
    m_FontFaceNormal = normal_face;

#if !wxUSE_UNICODE
    SetInputEncoding(m_InputEnc);
#endif

    for (i = 0; i < 2; i++)
        for (j = 0; j < 2; j++)
            for (k = 0; k < 2; k++)
                for (l = 0; l < 2; l++)
                    for (m = 0; m < 7; m++) {
                        if (m_FontsTable[i][j][k][l][m] != NULL)
                        {
                            delete m_FontsTable[i][j][k][l][m];
                            m_FontsTable[i][j][k][l][m] = NULL;
                        }
                    }
}

void wxHtmlWinParser::InitParser(const wxString& source)
{
    wxHtmlParser::InitParser(source);
    wxASSERT_MSG(m_DC != NULL, wxT("no DC assigned to wxHtmlWinParser!!"));

    m_FontBold = m_FontItalic = m_FontUnderlined = m_FontFixed = FALSE;
    m_FontSize = 3; //default one
    CreateCurrentFont();           // we're selecting default font into
    m_DC->GetTextExtent( wxT("H"), &m_CharWidth, &m_CharHeight);
                /* NOTE : we're not using GetCharWidth/Height() because
                   of differences under X and win
                 */

    m_UseLink = FALSE;
    m_Link = wxHtmlLinkInfo( wxT(""), wxT("") );
    m_LinkColor.Set(0, 0, 0xFF);
    m_ActualColor.Set(0, 0, 0);
    m_Align = wxHTML_ALIGN_LEFT;
    m_tmpLastWasSpace = FALSE;

    OpenContainer();
    OpenContainer();

#if !wxUSE_UNICODE
    wxString charset = ExtractCharsetInformation(source);
    if (!charset.empty())
    {
        wxFontEncoding enc = wxFontMapper::Get()->CharsetToEncoding(charset);
        if (enc != wxFONTENCODING_SYSTEM)
          SetInputEncoding(enc);
    }
#endif

    m_Container->InsertCell(new wxHtmlColourCell(m_ActualColor));
    m_Container->InsertCell(new wxHtmlFontCell(CreateCurrentFont()));
}

void wxHtmlWinParser::DoneParser()
{
    m_Container = NULL;
#if !wxUSE_UNICODE
    SetInputEncoding(wxFONTENCODING_ISO8859_1); // for next call
#endif
    wxHtmlParser::DoneParser();
}

wxObject* wxHtmlWinParser::GetProduct()
{
    wxHtmlContainerCell *top;

    CloseContainer();
    OpenContainer();

    top = m_Container;
    while (top->GetParent()) top = top->GetParent();
    return top;
}

wxFSFile *wxHtmlWinParser::OpenURL(wxHtmlURLType type,
                                   const wxString& url) const
{
    // FIXME - normalize the URL to full path before passing to
    //         OnOpeningURL!!
    if ( m_Window )
    {
        wxString myurl(url);
        wxHtmlOpeningStatus status;
        for (;;)
        {
            wxString redirect;
            status = m_Window->OnOpeningURL(type, myurl, &redirect);
            if ( status != wxHTML_REDIRECT )
                break;

            myurl = redirect;
        }

        if ( status == wxHTML_BLOCK )
            return NULL;

        return GetFS()->OpenFile(myurl);
    }

    return wxHtmlParser::OpenURL(type, url);
}

void wxHtmlWinParser::AddText(const wxChar* txt)
{
    wxHtmlCell *c;
    size_t i = 0,
           x,
           lng = wxStrlen(txt);
    register wxChar d;
    int templen = 0;
    wxChar nbsp = GetEntitiesParser()->GetCharForCode(160 /* nbsp */);

    if (lng+1 > m_tmpStrBufSize)
    {
        delete[] m_tmpStrBuf;
        m_tmpStrBuf = new wxChar[lng+1];
        m_tmpStrBufSize = lng+1;
    }
    wxChar *temp = m_tmpStrBuf;

    if (m_tmpLastWasSpace)
    {
        while ((i < lng) &&
               ((txt[i] == wxT('\n')) || (txt[i] == wxT('\r')) || (txt[i] == wxT(' ')) ||
                (txt[i] == wxT('\t')))) i++;
    }

    while (i < lng)
    {
        x = 0;
        d = temp[templen++] = txt[i];
        if ((d == wxT('\n')) || (d == wxT('\r')) || (d == wxT(' ')) || (d == wxT('\t')))
        {
            i++, x++;
            while ((i < lng) && ((txt[i] == wxT('\n')) || (txt[i] == wxT('\r')) ||
                                 (txt[i] == wxT(' ')) || (txt[i] == wxT('\t')))) i++, x++;
        }
        else i++;

        if (x)
        {
            temp[templen-1] = wxT(' ');
            temp[templen] = 0;
#if 0 // VS - WHY was this here?!
            if (templen == 1) continue;
#endif
            templen = 0;
#if !wxUSE_UNICODE
            if (m_EncConv)
                m_EncConv->Convert(temp);
#endif
            size_t len = wxStrlen(temp);
            for (size_t j = 0; j < len; j++)
                if (temp[j] == nbsp)
                    temp[j] = wxT(' ');
            c = new wxHtmlWordCell(temp, *(GetDC()));
            if (m_UseLink)
                c->SetLink(m_Link);
            m_Container->InsertCell(c);
            m_tmpLastWasSpace = TRUE;
        }
    }

    if (templen && (templen > 1 || temp[0] != wxT(' ')))
    {
        temp[templen] = 0;
#if !wxUSE_UNICODE
        if (m_EncConv)
            m_EncConv->Convert(temp);
#endif
        size_t len = wxStrlen(temp);
        for (size_t j = 0; j < len; j++)
            if (temp[j] == nbsp)
                temp[j] = wxT(' ');
        c = new wxHtmlWordCell(temp, *(GetDC()));
        if (m_UseLink)
            c->SetLink(m_Link);
        m_Container->InsertCell(c);
        m_tmpLastWasSpace = FALSE;
    }
}



wxHtmlContainerCell* wxHtmlWinParser::OpenContainer()
{
    m_Container = new wxHtmlContainerCell(m_Container);
    m_Container->SetAlignHor(m_Align);
    m_tmpLastWasSpace = TRUE;
        /* to avoid space being first character in paragraph */
    return m_Container;
}



wxHtmlContainerCell* wxHtmlWinParser::SetContainer(wxHtmlContainerCell *c)
{
    m_tmpLastWasSpace = TRUE;
        /* to avoid space being first character in paragraph */
    return m_Container = c;
}



wxHtmlContainerCell* wxHtmlWinParser::CloseContainer()
{
    m_Container = m_Container->GetParent();
    return m_Container;
}


void wxHtmlWinParser::SetFontSize(int s)
{
    if (s < 1) s = 1;
    else if (s > 7) s = 7;
    m_FontSize = s;
}



wxFont* wxHtmlWinParser::CreateCurrentFont()
{
    int fb = GetFontBold(),
        fi = GetFontItalic(),
        fu = GetFontUnderlined(),
        ff = GetFontFixed(),
        fs = GetFontSize() - 1 /*remap from <1;7> to <0;6>*/ ;

    wxString face = ff ? m_FontFaceFixed : m_FontFaceNormal;
    wxString *faceptr = &(m_FontsFacesTable[fb][fi][fu][ff][fs]);
    wxFont **fontptr = &(m_FontsTable[fb][fi][fu][ff][fs]);
#if !wxUSE_UNICODE
    wxFontEncoding *encptr = &(m_FontsEncTable[fb][fi][fu][ff][fs]);
#endif

    if (*fontptr != NULL && (*faceptr != face
#if !wxUSE_UNICODE
                             || *encptr != m_OutputEnc
#endif
                            ))
    {
        delete *fontptr;
        *fontptr = NULL;
    }

    if (*fontptr == NULL)
    {
        *faceptr = face;
        *fontptr = new wxFont(
                       (int) (m_FontsSizes[fs] * m_PixelScale),
                       ff ? wxMODERN : wxSWISS,
                       fi ? wxITALIC : wxNORMAL,
                       fb ? wxBOLD : wxNORMAL,
                       fu ? TRUE : FALSE, face
#if wxUSE_UNICODE
                       );
#else
                       , m_OutputEnc);
        *encptr = m_OutputEnc;
#endif
    }
    m_DC->SetFont(**fontptr);
    return (*fontptr);
}



void wxHtmlWinParser::SetLink(const wxHtmlLinkInfo& link)
{
    m_Link = link;
    m_UseLink = (link.GetHref() != wxEmptyString);
}


void wxHtmlWinParser::SetFontFace(const wxString& face)
{
    if (GetFontFixed()) m_FontFaceFixed = face;
    else m_FontFaceNormal = face;

#if !wxUSE_UNICODE
    if (m_InputEnc != wxFONTENCODING_DEFAULT)
        SetInputEncoding(m_InputEnc);
#endif
}



#if !wxUSE_UNICODE
void wxHtmlWinParser::SetInputEncoding(wxFontEncoding enc)
{
    m_InputEnc = m_OutputEnc = wxFONTENCODING_DEFAULT;
    if (m_EncConv)
    {
        delete m_EncConv;
        m_EncConv = NULL;
    }

    if (enc == wxFONTENCODING_DEFAULT) return;

    wxFontEncoding altfix, altnorm;
    bool availfix, availnorm;

    // exact match?
    availnorm = wxFontMapper::Get()->IsEncodingAvailable(enc, m_FontFaceNormal);
    availfix = wxFontMapper::Get()->IsEncodingAvailable(enc, m_FontFaceFixed);
    if (availnorm && availfix)
        m_OutputEnc = enc;

    // alternatives?
    else if (wxFontMapper::Get()->GetAltForEncoding(enc, &altnorm, m_FontFaceNormal, FALSE) &&
             wxFontMapper::Get()->GetAltForEncoding(enc, &altfix, m_FontFaceFixed, FALSE) &&
             altnorm == altfix)
        m_OutputEnc = altnorm;

    // at least normal face?
    else if (availnorm)
        m_OutputEnc = enc;
    else if (wxFontMapper::Get()->GetAltForEncoding(enc, &altnorm, m_FontFaceNormal, FALSE))
        m_OutputEnc = altnorm;

    // okay, let convert to ISO_8859-1, available always
    else
        m_OutputEnc = wxFONTENCODING_DEFAULT;

    m_InputEnc = enc;
    if (m_OutputEnc == wxFONTENCODING_DEFAULT)
        GetEntitiesParser()->SetEncoding(wxFONTENCODING_SYSTEM);
    else
        GetEntitiesParser()->SetEncoding(m_OutputEnc);

    if (m_InputEnc == m_OutputEnc) return;

    m_EncConv = new wxEncodingConverter();
    if (!m_EncConv->Init(m_InputEnc,
                           (m_OutputEnc == wxFONTENCODING_DEFAULT) ?
                                      wxFONTENCODING_ISO8859_1 : m_OutputEnc,
                           wxCONVERT_SUBSTITUTE))
    { // total failture :-(
        wxLogError(_("Failed to display HTML document in %s encoding"),
                   wxFontMapper::GetEncodingName(enc).c_str());
        m_InputEnc = m_OutputEnc = wxFONTENCODING_DEFAULT;
        delete m_EncConv;
        m_EncConv = NULL;
    }
}
#endif




//-----------------------------------------------------------------------------
// wxHtmlWinTagHandler
//-----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxHtmlWinTagHandler, wxHtmlTagHandler)

//-----------------------------------------------------------------------------
// wxHtmlTagsModule
//-----------------------------------------------------------------------------

// NB: This is *NOT* winpars.cpp's initialization and shutdown code!!
//     This module is an ancestor for tag handlers modules defined
//     in m_*.cpp files with TAGS_MODULE_BEGIN...TAGS_MODULE_END construct.
//
//     Do not add any winpars.cpp shutdown or initialization code to it,
//     create a new module instead!

IMPLEMENT_DYNAMIC_CLASS(wxHtmlTagsModule, wxModule)

bool wxHtmlTagsModule::OnInit()
{
    wxHtmlWinParser::AddModule(this);
    return TRUE;
}

void wxHtmlTagsModule::OnExit()
{
    wxHtmlWinParser::RemoveModule(this);
}

#endif

