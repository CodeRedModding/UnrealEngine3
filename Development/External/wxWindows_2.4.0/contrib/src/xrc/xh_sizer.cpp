/////////////////////////////////////////////////////////////////////////////
// Name:        xh_sizer.cpp
// Purpose:     XRC resource for wxBoxSizer
// Author:      Vaclav Slavik
// Created:     2000/03/21
// RCS-ID:      $Id: xh_sizer.cpp,v 1.6.2.1 2002/12/08 13:22:17 VS Exp $
// Copyright:   (c) 2000 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
 
#ifdef __GNUG__
#pragma implementation "xh_sizer.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/xrc/xh_sizer.h"
#include "wx/sizer.h"
#include "wx/log.h"
#include "wx/statbox.h"
#include "wx/notebook.h"
#include "wx/tokenzr.h"

bool wxSizerXmlHandler::IsSizerNode(wxXmlNode *node)
{
    return (IsOfClass(node, wxT("wxBoxSizer"))) ||
           (IsOfClass(node, wxT("wxStaticBoxSizer"))) ||
           (IsOfClass(node, wxT("wxGridSizer"))) ||
           (IsOfClass(node, wxT("wxFlexGridSizer")));
}



wxSizerXmlHandler::wxSizerXmlHandler() 
: wxXmlResourceHandler(), m_isInside(FALSE), m_parentSizer(NULL)
{
    XRC_ADD_STYLE(wxHORIZONTAL);
    XRC_ADD_STYLE(wxVERTICAL);

    // and flags
    XRC_ADD_STYLE(wxLEFT);
    XRC_ADD_STYLE(wxRIGHT);
    XRC_ADD_STYLE(wxTOP);
    XRC_ADD_STYLE(wxBOTTOM);
    XRC_ADD_STYLE(wxNORTH);
    XRC_ADD_STYLE(wxSOUTH);
    XRC_ADD_STYLE(wxEAST);
    XRC_ADD_STYLE(wxWEST);
    XRC_ADD_STYLE(wxALL);

    XRC_ADD_STYLE(wxGROW);
    XRC_ADD_STYLE(wxEXPAND);
    XRC_ADD_STYLE(wxSHAPED);
    XRC_ADD_STYLE(wxSTRETCH_NOT);

    XRC_ADD_STYLE(wxALIGN_CENTER);
    XRC_ADD_STYLE(wxALIGN_CENTRE);
    XRC_ADD_STYLE(wxALIGN_LEFT);
    XRC_ADD_STYLE(wxALIGN_TOP);
    XRC_ADD_STYLE(wxALIGN_RIGHT);
    XRC_ADD_STYLE(wxALIGN_BOTTOM);
    XRC_ADD_STYLE(wxALIGN_CENTER_HORIZONTAL);
    XRC_ADD_STYLE(wxALIGN_CENTRE_HORIZONTAL);
    XRC_ADD_STYLE(wxALIGN_CENTER_VERTICAL);
    XRC_ADD_STYLE(wxALIGN_CENTRE_VERTICAL);
    
    XRC_ADD_STYLE(wxADJUST_MINSIZE);
}



wxObject *wxSizerXmlHandler::DoCreateResource()
{ 
    if (m_class == wxT("sizeritem"))
    {
        wxXmlNode *n = GetParamNode(wxT("object"));

       if ( !n )
           n = GetParamNode(wxT("object_ref"));

        if (n)
        {
            bool old_ins = m_isInside;
            wxSizer *old_par = m_parentSizer;
            m_isInside = FALSE;
            if (!IsSizerNode(n)) m_parentSizer = NULL;
            wxObject *item = CreateResFromNode(n, m_parent, NULL);
            m_isInside = old_ins;
            m_parentSizer = old_par;
            wxSizer *sizer = wxDynamicCast(item, wxSizer);
            wxWindow *wnd = wxDynamicCast(item, wxWindow);
            wxSize minsize = GetSize(wxT("minsize"));

            if (sizer)
            {
                m_parentSizer->Add(sizer, GetLong(wxT("option")), 
                                   GetStyle(wxT("flag")), GetDimension(wxT("border")));
                if (!(minsize == wxDefaultSize))
                    m_parentSizer->SetItemMinSize(sizer, minsize.x, minsize.y);
            }
            else if (wnd)
            {
                m_parentSizer->Add(wnd, GetLong(wxT("option")), 
                                   GetStyle(wxT("flag")), GetDimension(wxT("border")));
                if (!(minsize == wxDefaultSize))
                    m_parentSizer->SetItemMinSize(wnd, minsize.x, minsize.y);
            }
            else 
                wxLogError(wxT("Error in resource."));

            return item;
        }
        else /*n == NULL*/
        {
            wxLogError(wxT("Error in resource: no control/sizer within sizer's <item> tag."));
            return NULL;
        }
    }
    
    else if (m_class == wxT("spacer"))
    {
        wxCHECK_MSG(m_parentSizer, NULL, wxT("Incorrect syntax of XRC resource: spacer not within sizer!"));
        wxSize sz = GetSize();
        m_parentSizer->Add(sz.x, sz.y,
            GetLong(wxT("option")), GetStyle(wxT("flag")), GetDimension(wxT("border")));
        return NULL;
    }
    

    else {
        wxSizer *sizer = NULL;
        
        wxXmlNode *parentNode = m_node->GetParent();

        wxCHECK_MSG(m_parentSizer != NULL ||
                ((IsOfClass(parentNode, wxT("wxPanel")) ||
                  IsOfClass(parentNode, wxT("wxFrame")) ||
                  IsOfClass(parentNode, wxT("wxDialog"))) &&
                 parentNode->GetType() == wxXML_ELEMENT_NODE), NULL,
                wxT("Incorrect use of sizer: parent is not 'wxDialog', 'wxFrame' or 'wxPanel'."));

        if (m_class == wxT("wxBoxSizer"))
            sizer = new wxBoxSizer(GetStyle(wxT("orient"), wxHORIZONTAL));

        else if (m_class == wxT("wxStaticBoxSizer"))
        {
            sizer = new wxStaticBoxSizer(
                         new wxStaticBox(m_parentAsWindow,
                                         GetID(),
                                         GetText(wxT("label")),
                                         wxDefaultPosition, wxDefaultSize,
                                         0/*style*/,
                                         GetName()),
                         GetStyle(wxT("orient"), wxHORIZONTAL));
        }
        
        else if (m_class == wxT("wxGridSizer"))
            sizer = new wxGridSizer(GetLong(wxT("rows")), GetLong(wxT("cols")),
                                    GetDimension(wxT("vgap")), GetDimension(wxT("hgap")));
                                    
        else if (m_class == wxT("wxFlexGridSizer"))
        {
            wxFlexGridSizer *fsizer = 
                  new wxFlexGridSizer(GetLong(wxT("rows")), GetLong(wxT("cols")),
                      GetDimension(wxT("vgap")), GetDimension(wxT("hgap")));
            sizer = fsizer;
            wxStringTokenizer tkn;
            unsigned long l;
            tkn.SetString(GetParamValue(wxT("growablerows")), wxT(","));
            while (tkn.HasMoreTokens())
            {
                if (!tkn.GetNextToken().ToULong(&l))
                    wxLogError(wxT("growablerows must be comma-separated list of row numbers"));
                else
                    fsizer->AddGrowableRow(l);
            }
            tkn.SetString(GetParamValue(wxT("growablecols")), wxT(","));
            while (tkn.HasMoreTokens())
            {
                if (!tkn.GetNextToken().ToULong(&l))
                    wxLogError(wxT("growablecols must be comma-separated list of column numbers"));
                else
                    fsizer->AddGrowableCol(l);
            }
        }

        wxSize minsize = GetSize(wxT("minsize"));
        if (!(minsize == wxDefaultSize))
            sizer->SetMinSize(minsize);

        wxSizer *old_par = m_parentSizer;
        m_parentSizer = sizer;
        bool old_ins = m_isInside;
        m_isInside = TRUE;
        CreateChildren(m_parent, TRUE/*only this handler*/);
        m_isInside = old_ins;
        m_parentSizer = old_par;
        
        if (m_parentSizer == NULL) // setup window:
        {
            m_parentAsWindow->SetAutoLayout(TRUE);
            m_parentAsWindow->SetSizer(sizer);

            wxXmlNode *nd = m_node;
            m_node = parentNode;
            if (GetSize() == wxDefaultSize)
                sizer->Fit(m_parentAsWindow);
            m_node = nd;

            if (m_parentAsWindow->GetWindowStyle() & (wxRESIZE_BOX | wxRESIZE_BORDER))
                sizer->SetSizeHints(m_parentAsWindow);
        }
        
        return sizer;
    }
}



bool wxSizerXmlHandler::CanHandle(wxXmlNode *node)
{
    return ((!m_isInside && IsSizerNode(node)) ||
            (m_isInside && IsOfClass(node, wxT("sizeritem"))) ||
            (m_isInside && IsOfClass(node, wxT("spacer"))));
}
