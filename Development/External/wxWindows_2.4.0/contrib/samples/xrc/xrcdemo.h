//-----------------------------------------------------------------------------
// Name:        xmldemo.cpp
// Purpose:     XML resources sample: Main application file
// Author:      Robert O'Connor (rob@medicalmnemonics.com), Vaclav Slavik
// RCS-ID:      $Id: xrcdemo.h,v 1.2 2002/09/07 12:12:23 GD Exp $
// Copyright:   (c) Robert O'Connor and Vaclav Slavik
// Licence:     wxWindows licence
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Begin single inclusion of this .h file condition
//-----------------------------------------------------------------------------

#ifndef _XRCDEMO_H_
#define _XRCDEMO_H_

//-----------------------------------------------------------------------------
// GCC interface
//-----------------------------------------------------------------------------

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "xrcdemo.h"
#endif

//-----------------------------------------------------------------------------
// Headers
//-----------------------------------------------------------------------------

#include "wx/app.h"             // wxApp

//-----------------------------------------------------------------------------
// Class definition: MyApp
//-----------------------------------------------------------------------------

// Define a new application type, each program should derive a class from wxApp
class MyApp : public wxApp
{

public:

    // Override base class virtuals:
    // wxApp::OnInit() is called on application startup and is a good place 
    // for the app initialization (doing it here and not in the ctor 
    // allows to have an error return: if OnInit() returns false, the 
    // application terminates)
    virtual bool OnInit();
    
};

//-----------------------------------------------------------------------------
// End single inclusion of this .h file condition
//-----------------------------------------------------------------------------

#endif  //_XRCDEMO_H_
