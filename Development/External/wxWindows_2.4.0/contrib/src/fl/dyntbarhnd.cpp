/////////////////////////////////////////////////////////////////////////////
// Name:        dyntbarhnd.cpp
// Purpose:     cbDynToolBarDimHandler implementation.
// Author:      Aleksandras Gluchovas
// Modified by:
// Created:     23/01/99
// RCS-ID:      $Id: dyntbarhnd.cpp,v 1.2 2002/01/21 22:34:41 JS Exp $
// Copyright:   (c) Aleksandras Gluchovas
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
    #pragma implementation "dyntbarhnd.h"
#endif

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "wx/fl/dyntbarhnd.h"
/***** Implementation for class cbDynToolBarDimHandler *****/

IMPLEMENT_DYNAMIC_CLASS( cbDynToolBarDimHandler, cbBarDimHandlerBase )

void cbDynToolBarDimHandler::OnChangeBarState(cbBarInfo* pBar, int newState )
{
    // nothing
}

void cbDynToolBarDimHandler::OnResizeBar( cbBarInfo* pBar, 
                                          const wxSize& given, 
                                          wxSize& preferred )
{
    wxASSERT( pBar->mpBarWnd ); // DBG:: should be present

    wxDynamicToolBar* pTBar = (wxDynamicToolBar*)pBar->mpBarWnd;

    pTBar->GetPreferredDim( given, preferred );
}

