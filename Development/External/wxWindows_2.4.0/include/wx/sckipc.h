/////////////////////////////////////////////////////////////////////////////
// Name:        sckipc.h
// Purpose:     Interprocess communication implementation (wxSocket version)
// Author:      Julian Smart
// Modified by: Guilhem Lavaux (big rewrite) May 1997, 1998
//              Guillermo Rodriguez (updated for wxSocket v2) Jan 2000
//                                  (callbacks deprecated)    Mar 2000
// Created:     1993
// RCS-ID:      $Id: sckipc.h,v 1.19 2002/09/03 11:22:55 JS Exp $
// Copyright:   (c) Julian Smart 1993
//              (c) Guilhem Lavaux 1997, 1998
//              (c) 2000 Guillermo Rodriguez <guille@iies.es>
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SCKIPC_H
#define _WX_SCKIPC_H

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "sckipc.h"
#endif

#include "wx/defs.h"

#if wxUSE_SOCKETS && wxUSE_IPC

#include "wx/ipcbase.h"
#include "wx/socket.h"
#include "wx/sckstrm.h"
#include "wx/datstrm.h"

/*
 * Mini-DDE implementation

   Most transactions involve a topic name and an item name (choose these
   as befits your application).

   A client can:

   - ask the server to execute commands (data) associated with a topic
   - request data from server by topic and item
   - poke data into the server
   - ask the server to start an advice loop on topic/item
   - ask the server to stop an advice loop

   A server can:

   - respond to execute, request, poke and advice start/stop
   - send advise data to client

   Note that this limits the server in the ways it can send data to the
   client, i.e. it can't send unsolicited information.
 *
 */

class WXDLLEXPORT wxTCPServer;
class WXDLLEXPORT wxTCPClient;

class WXDLLEXPORT wxTCPConnection: public wxConnectionBase
{
  DECLARE_DYNAMIC_CLASS(wxTCPConnection)

public:
  wxTCPConnection(wxChar *buffer, int size);
  wxTCPConnection();
  virtual ~wxTCPConnection();

  // Calls that CLIENT can make
  virtual bool Execute(const wxChar *data, int size = -1, wxIPCFormat format = wxIPC_TEXT);
  virtual wxChar *Request(const wxString& item, int *size = NULL, wxIPCFormat format = wxIPC_TEXT);
  virtual bool Poke(const wxString& item, wxChar *data, int size = -1, wxIPCFormat format = wxIPC_TEXT);
  virtual bool StartAdvise(const wxString& item);
  virtual bool StopAdvise(const wxString& item);

  // Calls that SERVER can make
  virtual bool Advise(const wxString& item, wxChar *data, int size = -1, wxIPCFormat format = wxIPC_TEXT);

  // Calls that both can make
  virtual bool Disconnect(void);

  // Callbacks to BOTH - override at will
  // Default behaviour is to delete connection and return TRUE
  virtual bool OnDisconnect(void) { delete this; return TRUE; }

  // To enable the compressor (NOTE: not implemented!)
  void Compress(bool on);

protected:
  wxSocketBase       *m_sock;
  wxSocketStream     *m_sockstrm;
  wxDataInputStream  *m_codeci;
  wxDataOutputStream *m_codeco;
  wxString            m_topic;

  friend class wxTCPServer;
  friend class wxTCPClient;
  friend class wxTCPEventHandler;

private:
  //
  // We're hiding an Execute method in ConnectionBase
  //
  virtual bool Execute(const wxString& str)
    { return Execute(str, -1, wxIPC_TEXT); }
};

class wxTCPServer: public wxServerBase
{
  DECLARE_DYNAMIC_CLASS(wxTCPServer)

public:
  wxTCPConnection *topLevelConnection;

  wxTCPServer();
  virtual ~wxTCPServer();

  // Returns FALSE on error (e.g. port number is already in use)
  virtual bool Create(const wxString& serverName);

  // Callbacks to SERVER - override at will
  virtual wxConnectionBase *OnAcceptConnection(const wxString& topic);

protected:
  wxSocketServer *m_server;

#ifdef __UNIX_LIKE__
  // the name of the file associated to the Unix domain socket, may be empty
  wxString m_filename;
#endif // __UNIX_LIKE__
};

class wxTCPClient: public wxClientBase
{
  DECLARE_DYNAMIC_CLASS(wxTCPClient)

public:
  wxTCPClient();
  virtual ~wxTCPClient();

  virtual bool ValidHost(const wxString& host);

  // Call this to make a connection. Returns NULL if cannot.
  virtual wxConnectionBase *MakeConnection(const wxString& host,
                                           const wxString& server,
                                           const wxString& topic);

  // Callbacks to CLIENT - override at will
  virtual wxConnectionBase *OnMakeConnection();
};

#endif // wxUSE_SOCKETS && wxUSE_IPC

#endif // _WX_SCKIPC_H
