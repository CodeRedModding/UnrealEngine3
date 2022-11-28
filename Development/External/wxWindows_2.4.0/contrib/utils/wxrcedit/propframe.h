/////////////////////////////////////////////////////////////////////////////
// Purpose:     XML resources editor
// Author:      Vaclav Slavik
// Created:     2000/05/05
// RCS-ID:      $Id: propframe.h,v 1.3 2002/09/07 12:15:24 GD Exp $
// Copyright:   (c) 2000 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "propframe.h"
#endif

#ifndef _PROPFRAME_H_
#define _PROPFRAME_H_

#include "splittree.h"

class WXDLLEXPORT wxXmlNode;
class PropEditCtrl;
#include "wx/hash.h"
#include "wx/frame.h"
#include "nodesdb.h"

class PropertiesFrame : public wxFrame
{
    public:
        PropertiesFrame();
        ~PropertiesFrame();
        
        void ShowProps(wxXmlNode *node);
        
        void ClearProps();
        void AddProps(PropertyInfoArray& plist);
        void AddSingleProp(const PropertyInfo& pinfo, wxTreeItemId *root = NULL);
      
        static PropertiesFrame *Get();
           
    private:

        static PropertiesFrame *ms_Instance;
        wxXmlNode *m_Node;

        wxRemotelyScrolledTreeCtrl *m_tree;
        wxThinSplitterWindow *m_splitter;
	    wxSplitterScrolledWindow *m_scrolledWindow;
        wxTreeCompanionWindow *m_valueWindow;
        
        wxHashTable m_EditCtrls;
        
        friend class PropEditCtrl;
};


#endif
