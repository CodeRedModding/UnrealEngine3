/////////////////////////////////////////////////////////////////////////////
// Name:        m_list.cpp
// Purpose:     wxHtml module for lists
// Author:      Vaclav Slavik
// RCS-ID:      $Id: m_list.cpp,v 1.15.2.3 2002/11/09 00:07:34 VS Exp $
// Copyright:   (c) 1999 Vaclav Slavik
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////
#ifdef __GNUG__
#pragma implementation
#endif

#include "wx/wxprec.h"


#include "wx/defs.h"
#if wxUSE_HTML && wxUSE_STREAMS

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WXPRECOMP
    #include "wx/brush.h"
    #include "wx/dc.h"
#endif

#include "wx/html/forcelnk.h"
#include "wx/html/m_templ.h"

#include "wx/html/htmlcell.h"

FORCE_LINK_ME(m_list)


//-----------------------------------------------------------------------------
// wxHtmlListmarkCell
//-----------------------------------------------------------------------------

class wxHtmlListmarkCell : public wxHtmlCell
{
    private:
        wxBrush m_Brush;
    public:
        wxHtmlListmarkCell(wxDC *dc, const wxColour& clr);
        void Draw(wxDC& dc, int x, int y, int view_y1, int view_y2);
};

wxHtmlListmarkCell::wxHtmlListmarkCell(wxDC* dc, const wxColour& clr) : wxHtmlCell(), m_Brush(clr, wxSOLID)
{
    m_Width =  dc->GetCharHeight();
    m_Height = dc->GetCharHeight();
    m_Descent = 0;
}



void wxHtmlListmarkCell::Draw(wxDC& dc, int x, int y, int WXUNUSED(view_y1), int WXUNUSED(view_y2))
{
    dc.SetBrush(m_Brush);
    dc.DrawEllipse(x + m_PosX + m_Width / 3, y + m_PosY + m_Height / 3, 
                   (m_Width / 3), (m_Width / 3));
}




//-----------------------------------------------------------------------------
// The list handler:
//-----------------------------------------------------------------------------


TAG_HANDLER_BEGIN(OLULLI, "OL,UL,LI")

    TAG_HANDLER_VARS
        int m_Numbering;
                // this is number of actual item of list or 0 for dots

    TAG_HANDLER_CONSTR(OLULLI)
    {
        m_Numbering = 0;
    }

    TAG_HANDLER_PROC(tag)
    {
        wxHtmlContainerCell *c;

        // List Item:
        if (tag.GetName() == wxT("LI"))
        {
            m_WParser->GetContainer()->SetIndent(0, wxHTML_INDENT_TOP);
                // this is to prevent indetation in <li><p> case
            m_WParser->CloseContainer();
            m_WParser->CloseContainer();

            c = m_WParser->OpenContainer();
            c->SetWidthFloat(2 * m_WParser->GetCharWidth(), wxHTML_UNITS_PIXELS);
            if (m_Numbering == 0)
            {
                // Centering gives more space after the bullet
                c->SetAlignHor(wxHTML_ALIGN_CENTER);
                c->InsertCell(new wxHtmlListmarkCell(m_WParser->GetDC(), m_WParser->GetActualColor()));
            }
            else
            {
                c->SetAlignHor(wxHTML_ALIGN_RIGHT);
                wxString mark;
                mark.Printf(wxT("%i."), m_Numbering);
                c->InsertCell(new wxHtmlWordCell(mark, *(m_WParser->GetDC())));
            }
            m_WParser->CloseContainer();

            c = m_WParser->OpenContainer();
            c->SetIndent(m_WParser->GetCharWidth() / 4, wxHTML_INDENT_LEFT);
            c->SetWidthFloat(-2 * m_WParser->GetCharWidth(), wxHTML_UNITS_PIXELS);

            m_WParser->OpenContainer();

            if (m_Numbering != 0) m_Numbering++;

            return FALSE;
        }

        // Begin of List (not-numbered): "UL", "OL"
        else
        {
            int oldnum = m_Numbering;

            if (tag.GetName() == wxT("UL")) m_Numbering = 0;
            else m_Numbering = 1;

            c = m_WParser->GetContainer();
            if (c->GetFirstCell() != NULL)
            {
                m_WParser->CloseContainer();
                m_WParser->OpenContainer();
                c = m_WParser->GetContainer();
            }
            c->SetAlignHor(wxHTML_ALIGN_LEFT);
            c->SetIndent(2 * m_WParser->GetCharWidth(), wxHTML_INDENT_LEFT);
            m_WParser->OpenContainer()->SetAlignVer(wxHTML_ALIGN_TOP);

            m_WParser->OpenContainer();
            m_WParser->OpenContainer();
            ParseInner(tag);

            m_WParser->GetContainer()->SetIndent(0, wxHTML_INDENT_TOP);
                // this is to prevent indetation in <li><p> case
            m_WParser->CloseContainer();

            m_WParser->CloseContainer();
            m_WParser->CloseContainer();
            m_WParser->CloseContainer();
            m_WParser->OpenContainer();

            m_Numbering = oldnum;
            return TRUE;
        }
    }

TAG_HANDLER_END(OLULLI)


TAGS_MODULE_BEGIN(List)

    TAGS_MODULE_ADD(OLULLI)

TAGS_MODULE_END(List)

#endif
