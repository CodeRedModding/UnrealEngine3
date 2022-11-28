/////////////////////////////////////////////////////////////////////////////
// Name:        dialoged.h
// Purpose:     Dialog Editor application header file
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// RCS-ID:      $Id: dialoged.h,v 1.8 2002/09/07 12:05:25 GD Exp $
// Copyright:   (c) Julian Smart
// Licence:   	wxWindows license
/////////////////////////////////////////////////////////////////////////////

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "dialoged.h"
#endif

#ifndef dialogedh
#define dialogedh

#include "wx/proplist.h"
#include "reseditr.h"

class MyChild;

// Define a new application
class MyApp: public wxApp
{
public:
    MyApp(void);
    bool OnInit(void);
    int OnExit(void);
    
private:
    DECLARE_EVENT_TABLE()
};

DECLARE_APP(MyApp)

extern wxFrame *GetMainFrame(void);

#endif
