/////////////////////////////////////////////////////////////////////////////
// Name:        xh_radbx.h
// Purpose:     XML resource handler for radio box
// Author:      Bob Mitchell
// Created:     2000/03/21
// RCS-ID:      $Id: xh_radbx.h,v 1.2 2002/09/07 12:10:21 GD Exp $
// Copyright:   (c) 2000 Bob Mitchell and Verant Interactive
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XH_RADBX_H_
#define _WX_XH_RADBX_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "xh_radbx.h"
#endif

#include "wx/xrc/xmlres.h"

#if wxUSE_RADIOBOX

class WXXMLDLLEXPORT wxRadioBoxXmlHandler : public wxXmlResourceHandler
{
public:
    wxRadioBoxXmlHandler();
    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);
private:
    bool m_insideBox;
    wxArrayString strList;
};

#endif

#endif // _WX_XH_RADBX_H_
