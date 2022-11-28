/////////////////////////////////////////////////////////////////////////////
// Name:        wave.h
// Purpose:     wxWave class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: wave.h,v 1.8 2001/07/14 03:25:40 RD Exp $
// Copyright:   (c) Julian Smart
// Licence:   	wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_WAVE_H_
#define _WX_WAVE_H_

#ifdef __GNUG__
#pragma interface "wave.h"
#endif

#if wxUSE_WAVE

#include "wx/object.h"

class WXDLLEXPORT wxWave : public wxObject
{
public:
  wxWave();
  wxWave(const wxString& fileName, bool isResource = FALSE);
  wxWave(int size, const wxByte* data);
  ~wxWave();

public:
  // Create from resource or file
  bool  Create(const wxString& fileName, bool isResource = FALSE);
  // Create from data
  bool Create(int size, const wxByte* data);

  bool  IsOk() const { return (m_waveData ? TRUE : FALSE); };
  bool  Play(bool async = TRUE, bool looped = FALSE) const;

protected:
  bool  Free();

private:
  wxByte* m_waveData;
  int   m_waveLength;
  bool  m_isResource;
};
#endif
#endif

