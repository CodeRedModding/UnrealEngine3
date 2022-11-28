/////////////////////////////////////////////////////////////////////////////
// Purpose:     XML resources editor
// Author:      Vaclav Slavik
// Created:     2000/05/05
// RCS-ID:      $Id: treedt.h,v 1.3 2002/09/07 12:15:24 GD Exp $
// Copyright:   (c) 2000 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "treedt.h"
#endif

#ifndef _TREEDT_H_
#define _TREEDT_H_


#include "wx/treectrl.h"

class WXDLLEXPORT wxXmlNode;

class XmlTreeData : public wxTreeItemData
{
    public:
        XmlTreeData(wxXmlNode *n) : Node(n) {}        
        wxXmlNode *Node;
};


#endif
