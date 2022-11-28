// --------------------------------------------------------------------------
// Name: sndoss.h
// Purpose:
// Date: 08/11/1999
// Author: Guilhem Lavaux <lavaux@easynet.fr> (C) 1999
// CVSID: $Id: sndoss.h,v 1.2 2000/06/04 08:38:36 GL Exp $
// --------------------------------------------------------------------------
#ifndef _WX_SNDOSS_H
#define _WX_SNDOSS_H

#ifdef __GNUG__
#pragma interface "sndoss.h"
#endif

#include "wx/defs.h"
#include "wx/string.h"
#include "wx/mmedia/sndbase.h"
#include "wx/mmedia/sndpcm.h"

//
// OSS output class
//

class WXDLLEXPORT wxSoundStreamOSS : public wxSoundStream {
 public:
  wxSoundStreamOSS(const wxString& dev_name = wxT("/dev/dsp"));
  ~wxSoundStreamOSS();

  wxSoundStream& Read(void *buffer, wxUint32 len);
  wxSoundStream& Write(const void *buffer, wxUint32 len);
  wxUint32 GetBestSize() const;

  bool SetSoundFormat(const wxSoundFormatBase& format);

  bool StartProduction(int evt);
  bool StopProduction();

  bool QueueFilled() const;

  // You should not call this.
  void WakeUpEvt(int evt);
 protected:
  // OSS device
  int m_fd;
  wxUint32 m_bufsize;
  int m_tag;
  bool m_oss_stop, m_oss_ok, m_q_filled;
  wxString m_devname;

 private:
  bool SetupFormat(wxSoundFormatPcm *pcm);
  void DetectBest(wxSoundFormatPcm *pcm);
};

#endif
