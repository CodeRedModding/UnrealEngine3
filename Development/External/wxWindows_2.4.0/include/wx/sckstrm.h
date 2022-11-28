/////////////////////////////////////////////////////////////////////////////
// Name:        sckstrm.h
// Purpose:     wxSocket*Stream
// Author:      Guilhem Lavaux
// Modified by:
// Created:     17/07/97
// RCS-ID:      $Id: sckstrm.h,v 1.10 2002/08/31 11:29:11 GD Exp $
// Copyright:   (c)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef __SCK_STREAM_H__
#define __SCK_STREAM_H__

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface
#endif

#include "wx/stream.h"

#if wxUSE_SOCKETS && wxUSE_STREAMS

#include "wx/socket.h"

class WXDLLEXPORT wxSocketOutputStream : public wxOutputStream
{
 public:
  wxSocketOutputStream(wxSocketBase& s);
  ~wxSocketOutputStream();

  off_t SeekO( off_t WXUNUSED(pos), wxSeekMode WXUNUSED(mode) )
    { return -1; }
  off_t TellO() const
    { return -1; }

 protected:
  wxSocketBase *m_o_socket;

  size_t OnSysWrite(const void *buffer, size_t bufsize);
};

class WXDLLEXPORT wxSocketInputStream : public wxInputStream
{
 public:
  wxSocketInputStream(wxSocketBase& s);
  ~wxSocketInputStream();

  off_t SeekI( off_t WXUNUSED(pos), wxSeekMode WXUNUSED(mode) )
    { return -1; }
  off_t TellI() const
    { return -1; }

 protected:
  wxSocketBase *m_i_socket;

  size_t OnSysRead(void *buffer, size_t bufsize);
};

class WXDLLEXPORT wxSocketStream : public wxSocketInputStream,
                   public wxSocketOutputStream
{
 public:
  wxSocketStream(wxSocketBase& s);
  ~wxSocketStream();
};

#endif
  // wxUSE_SOCKETS && wxUSE_STREAMS

#endif
  // __SCK_STREAM_H__
