///////////////////////////////////////////////////////////////////////////////
// Name:        wx/univ/scrtimer.h
// Purpose:     wxScrollTimer: small helper class for wxScrollArrow/Thumb
// Author:      Vadim Zeitlin
// Modified by:
// Created:     18.02.01
// RCS-ID:      $Id: scrtimer.h,v 1.3 2001/07/02 19:42:26 VZ Exp $
// Copyright:   (c) 2001 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// NB: this class is implemented in scrolbar.cpp

#include "wx/timer.h"

// ----------------------------------------------------------------------------
// wxScrollTimer: the timer used when the arrow or scrollbar shaft is kept
// pressed
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxScrollTimer : public wxTimer
{
public:
    // default ctor
    wxScrollTimer();

    // start generating the events
    void StartAutoScroll();

    // the base class method
    virtual void Notify();

protected:
    // to implement in derived classes: perform the scroll action and return
    // TRUE to continue scrolling or FALSE to stop
    virtual bool DoNotify() = 0;

    // should we skip the next timer event?
    bool m_skipNext;
};

