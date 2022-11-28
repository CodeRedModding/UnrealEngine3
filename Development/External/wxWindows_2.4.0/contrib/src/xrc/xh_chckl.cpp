/////////////////////////////////////////////////////////////////////////////
// Name:        xh_chckl.cpp
// Purpose:     XRC resource for wxCheckList
// Author:      Bob Mitchell
// Created:     2000/03/21
// RCS-ID:      $Id: xh_chckl.cpp,v 1.6 2002/09/01 17:11:38 VS Exp $
// Copyright:   (c) 2000 Bob Mitchell and Verant Interactive
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
 
#ifdef __GNUG__
#pragma implementation "xh_chckl.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/xrc/xh_chckl.h"
#include "wx/checklst.h"
#include "wx/intl.h"

wxCheckListXmlHandler::wxCheckListXmlHandler() 
: wxXmlResourceHandler(), m_insideBox(FALSE)
{
    // no styles
    AddWindowStyles();
}

wxObject *wxCheckListXmlHandler::DoCreateResource()
{ 
    if (m_class == wxT("wxCheckList"))
    {
        // need to build the list of strings from children
        m_insideBox = TRUE;
        CreateChildrenPrivately(NULL, GetParamNode(wxT("content")));
        wxString *strings = (wxString *) NULL;
        if (strList.GetCount() > 0)
        {
            strings = new wxString[strList.GetCount()];
            int count = strList.GetCount();
            for(int i = 0; i < count; i++)
                strings[i] = strList[i];
        }

        XRC_MAKE_INSTANCE(control, wxCheckListBox)

        control->Create(m_parentAsWindow,
                        GetID(),
                        GetPosition(), GetSize(),
                        strList.GetCount(),
                        strings,
                        GetStyle(),
                        wxDefaultValidator,
                        GetName());

        // step through children myself (again.)
        wxXmlNode *n = GetParamNode(wxT("content"));
        if (n) n = n->GetChildren();
        int i = 0;
        while (n)
        {
            if (n->GetType() != wxXML_ELEMENT_NODE ||
                n->GetName() != wxT("item"))
               { n = n->GetNext(); continue; }

            // checking boolean is a bit ugly here (see GetBool() )
            wxString v = n->GetPropVal(wxT("checked"), wxEmptyString);
            v.MakeLower();
            if (v && v == wxT("1"))
                control->Check( i, TRUE );

            i++;        
            n = n->GetNext();
        }
        
        SetupWindow(control);

        if (strings != NULL)
            delete[] strings;
        strList.Clear();    // dump the strings   

        return control;
    }
    else
    {
        // on the inside now.
        // handle <item checked="boolean">Label</item>

        // add to the list
        wxString str = GetNodeContent(m_node);
        if (m_resource->GetFlags() & wxXRC_USE_LOCALE)
            str = wxGetTranslation(str);
        strList.Add(str);
        return NULL;
    }
}

bool wxCheckListXmlHandler::CanHandle(wxXmlNode *node)
{
    return (IsOfClass(node, wxT("wxCheckList")) ||
           (m_insideBox && node->GetName() == wxT("item")));
}


