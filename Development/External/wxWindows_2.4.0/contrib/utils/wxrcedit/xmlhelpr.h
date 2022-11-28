/////////////////////////////////////////////////////////////////////////////
// Purpose:     XML resources editor
// Author:      Vaclav Slavik
// Created:     2000/05/05
// RCS-ID:      $Id: xmlhelpr.h,v 1.4 2002/09/07 12:15:24 GD Exp $
// Copyright:   (c) 2000 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "xmlhelpr.h"
#endif

#ifndef _XMLHELPR_H_
#define _XMLHELPR_H_

// some helper functions:

void XmlWriteValue(wxXmlNode *parent, const wxString& name, const wxString& value);
wxString XmlReadValue(wxXmlNode *parent, const wxString& name);

// Finds a subnode of parent named <name>
// (may be recursive, e.g. "name1/name2" means
// <parent><name1><name2>value</name2></name1></parent>
wxXmlNode *XmlFindNode(wxXmlNode *parent, const wxString& name);
wxXmlNode *XmlFindNodeSimple(wxXmlNode *parent, const wxString& path);

wxString XmlGetClass(wxXmlNode *parent);
void XmlSetClass(wxXmlNode *parent, const wxString& classname);

#endif 
