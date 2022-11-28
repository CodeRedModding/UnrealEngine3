/////////////////////////////////////////////////////////////////////////////
// Name:        validate.cpp
// Purpose:     wxValidator
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// RCS-ID:      $Id: validate.cpp,v 1.9 1999/12/14 23:42:49 VS Exp $
// Copyright:   (c) Julian Smart and Markus Holzem
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#pragma implementation "validate.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
  #pragma hdrstop
#endif

#ifndef WX_PRECOMP
  #include "wx/defs.h"
#endif

#if wxUSE_VALIDATORS

#ifndef WX_PRECOMP
  #include "wx/window.h"
#endif

#include "wx/validate.h"

const wxValidator wxDefaultValidator;

    IMPLEMENT_DYNAMIC_CLASS(wxValidator, wxEvtHandler)

// VZ: personally, I think TRUE would be more appropriate - these bells are
//     _annoying_
bool wxValidator::ms_isSilent = FALSE;

wxValidator::wxValidator()
{
  m_validatorWindow = (wxWindow *) NULL;
}

wxValidator::~wxValidator()
{
}

#endif
  // wxUSE_VALIDATORS
