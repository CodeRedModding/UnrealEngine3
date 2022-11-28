/////////////////////////////////////////////////////////////////////////////
// Name:        xh_radbx.cpp
// Purpose:     XRC resource for wxRadioBox
// Author:      Bob Mitchell
// Created:     2000/03/21
// RCS-ID:      $Id: xh_radbx.cpp,v 1.6 2002/09/01 17:11:38 VS Exp $
// Copyright:   (c) 2000 Bob Mitchell and Verant Interactive
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
 
#ifdef __GNUG__
#pragma implementation "xh_radbx.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/xrc/xh_radbx.h"
#include "wx/radiobox.h"
#include "wx/intl.h"

#if wxUSE_RADIOBOX

wxRadioBoxXmlHandler::wxRadioBoxXmlHandler() 
: wxXmlResourceHandler(), m_insideBox(FALSE)
{
    XRC_ADD_STYLE(wxRA_SPECIFY_COLS);
    XRC_ADD_STYLE(wxRA_HORIZONTAL);
    XRC_ADD_STYLE(wxRA_SPECIFY_ROWS);
    XRC_ADD_STYLE(wxRA_VERTICAL);
    AddWindowStyles();
}

wxObject *wxRadioBoxXmlHandler::DoCreateResource()
{ 
    if( m_class == wxT("wxRadioBox"))
    {
        // find the selection
        long selection = GetLong( wxT("selection"), -1 );

        // need to build the list of strings from children
        m_insideBox = TRUE;
        CreateChildrenPrivately( NULL, GetParamNode(wxT("content")));
        wxString *strings = (wxString *) NULL;
        if( strList.GetCount() > 0 )
        {
            strings = new wxString[strList.GetCount()];
            int count = strList.GetCount();
            for( int i = 0; i < count; i++ )
                strings[i]=strList[i];
        }

        XRC_MAKE_INSTANCE(control, wxRadioBox)

        control->Create(m_parentAsWindow,
                        GetID(),
                        GetText(wxT("label")),
                        GetPosition(), GetSize(),
                        strList.GetCount(),
                        strings,
                        GetLong(wxT("dimension"), 1),
                        GetStyle(),
                        wxDefaultValidator,
                        GetName());

        if (selection != -1)
            control->SetSelection(selection);

        SetupWindow(control);

        if (strings != NULL)
            delete[] strings;
        strList.Clear();    // dump the strings   

        return control;
    }
    else
    {
        // on the inside now.
        // handle <item selected="boolean">Label</item>

        // add to the list
        wxString str = GetNodeContent(m_node);
        if (m_resource->GetFlags() & wxXRC_USE_LOCALE)
            str = wxGetTranslation(str);
        strList.Add(str);

        return NULL;
    }

}

bool wxRadioBoxXmlHandler::CanHandle(wxXmlNode *node)
{
    return (IsOfClass(node, wxT("wxRadioBox")) ||
           (m_insideBox && node->GetName() == wxT("item")));
}

#endif
