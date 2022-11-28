// --------------------------------------------------------------------------
// Name: sndaiff.h
// Purpose:
// Date: 08/11/1999
// Author: Guilhem Lavaux <lavaux@easynet.fr> (C) 1999
// CVSID: $Id: sndaiff.h,v 1.2 2000/06/04 08:38:36 GL Exp $
// --------------------------------------------------------------------------
#ifndef _WX_SNDAIFF_H
#define _WX_SNDAIFF_H

#ifdef __GNUG__
#pragma interface "sndaiff.h"
#endif

#include "wx/defs.h"
#include "wx/stream.h"
#include "wx/mmedia/sndbase.h"
#include "wx/mmedia/sndcodec.h"
#include "wx/mmedia/sndfile.h"

//
// AIFF codec
//

class WXDLLEXPORT wxSoundAiff: public wxSoundFileStream {
public:
    wxSoundAiff(wxInputStream& stream, wxSoundStream& io_sound);
    wxSoundAiff(wxOutputStream& stream, wxSoundStream& io_sound);
    ~wxSoundAiff();
    
    bool CanRead();
    wxString GetCodecName() const;
    
protected:
    bool PrepareToPlay(); 
    bool PrepareToRecord(wxUint32 time);
    bool FinishRecording();
    bool RepositionStream(wxUint32 position);
    
    wxUint32 GetData(void *buffer, wxUint32 len);
    wxUint32 PutData(const void *buffer, wxUint32 len);
protected:
    off_t m_base_offset;
};

#endif
