/////////////////////////////////////////////////////////////////////////////
// Name:        module.h
// Purpose:     Modules handling
// Author:      Wolfram Gloger/adapted by Guilhem Lavaux
// Modified by:
// Created:     04/11/98
// RCS-ID:      $Id: module.h,v 1.6 2002/08/31 11:29:11 GD Exp $
// Copyright:   (c) Wolfram Gloger and Guilhem Lavaux
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MODULEH__
#define _WX_MODULEH__

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "module.h"
#endif

#include "wx/object.h"
#include "wx/list.h"

// declare a linked list of modules
class wxModule;
WX_DECLARE_EXPORTED_LIST(wxModule, wxModuleList);

// declaring a class derived from wxModule will automatically create an
// instance of this class on program startup, call its OnInit() method and call
// OnExit() on program termination (but only if OnInit() succeeded)
class WXDLLEXPORT wxModule : public wxObject
{
public:
    wxModule() {}
    virtual ~wxModule() {}

    	// if module init routine returns FALSE application
	// will fail to startup

    bool Init() { return OnInit(); }
    void Exit() { OnExit(); }

	// Override both of these
        // called on program startup

    virtual bool OnInit() = 0;

    	// called just before program termination, but only if OnInit()
        // succeeded
    
    virtual void OnExit() = 0;

    static void RegisterModule(wxModule* module);
    static void RegisterModules();
    static bool InitializeModules();
    static void CleanUpModules();

    	// used by wxObjectLoader when unloading shared libs's

    static void UnregisterModule(wxModule* module);

protected:
    static wxModuleList m_modules;

    DECLARE_CLASS(wxModule)
};

#endif // _WX_MODULEH__

