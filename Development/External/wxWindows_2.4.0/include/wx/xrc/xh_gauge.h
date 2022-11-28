/////////////////////////////////////////////////////////////////////////////
// Name:        xh_gauge.h
// Purpose:     XML resource handler for wxGauge
// Author:      Bob Mitchell
// Created:     2000/03/21
// RCS-ID:      $Id: xh_gauge.h,v 1.2 2002/09/07 12:10:21 GD Exp $
// Copyright:   (c) 2000 Bob Mitchell and Verant Interactive
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XH_GAUGE_H_
#define _WX_XH_GAUGE_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "xh_gauge.h"
#endif

#include "wx/defs.h"

#if wxUSE_GAUGE

#include "wx/xrc/xmlres.h"


class WXXMLDLLEXPORT wxGaugeXmlHandler : public wxXmlResourceHandler
{
    enum
    {
        wxGAUGE_DEFAULT_RANGE = 100
    };

public:
    wxGaugeXmlHandler();
    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);
};

#endif

#endif // _WX_XH_GAUGE_H_
