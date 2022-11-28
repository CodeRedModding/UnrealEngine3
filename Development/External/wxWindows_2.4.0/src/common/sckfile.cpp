/////////////////////////////////////////////////////////////////////////////
// Name:        sckfile.cpp
// Purpose:     File protocol
// Author:      Guilhem Lavaux
// Modified by:
// Created:     20/07/97
// RCS-ID:      $Id: sckfile.cpp,v 1.13 2001/12/31 23:30:38 VS Exp $
// Copyright:   (c) 1997, 1998 Guilhem Lavaux
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////
#ifdef __GNUG__
#pragma implementation "sckfile.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
  #pragma hdrstop
#endif

#ifndef WX_PRECOMP
  #include "wx/defs.h"
#endif

#if wxUSE_STREAMS && wxUSE_PROTOCOL_FILE

#include <stdio.h>
#include "wx/wfstream.h"
#include "wx/protocol/file.h"

IMPLEMENT_DYNAMIC_CLASS(wxFileProto, wxProtocol)
IMPLEMENT_PROTOCOL(wxFileProto, wxT("file"), NULL, FALSE)

wxFileProto::wxFileProto()
  : wxProtocol()
{
}

wxFileProto::~wxFileProto()
{
}

wxInputStream *wxFileProto::GetInputStream(const wxString& path)
{
  return new wxFileInputStream(path);
}

#endif // wxUSE_STREAMS && wxUSE_PROTOCOL_FILE

