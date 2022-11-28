/////////////////////////////////////////////////////////////////////////////
// Name:        common/timercmn.cpp
// Purpose:     Common timer implementation
// Author:
//    Original version by Julian Smart
//    Vadim Zeitlin got rid of all ifdefs (11.12.99)
//    Sylvain Bougnoux added wxStopWatch class
//    Guillermo Rodriguez <guille@iies.es> rewrote from scratch (Dic/99)
// Modified by:
// Created:     04/01/98
// RCS-ID:      $Id: timercmn.cpp,v 1.62.2.2 2002/10/15 16:02:54 JS Exp $
// Copyright:   (c) Julian Smart and Markus Holzem
//              (c) 1999 Guillermo Rodriguez <guille@iies.es>
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// wxWin headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "timerbase.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/thread.h"
#endif

#include "wx/timer.h"
#include "wx/longlong.h"

// ----------------------------------------------------------------------------
// System headers
// ----------------------------------------------------------------------------

#if defined(__WIN32__)
    #include <windows.h>
#endif

#if defined(__WIN32__) && !defined(HAVE_FTIME) && !defined(__MWERKS__)
    #define HAVE_FTIME
#endif

#if defined(__VISAGECPP__) && !defined(HAVE_FTIME)
    #define HAVE_FTIME
#  if __IBMCPP__ >= 400
    #  define ftime(x) _ftime(x)
#  endif
#endif

#if defined(__MWERKS__) && defined(__WXMSW__)
#   undef HAVE_FTIME
#   undef HAVE_GETTIMEOFDAY
#endif

#include <time.h>
#ifndef __WXMAC__
    #include <sys/types.h>      // for time_t
#endif

#if defined(HAVE_GETTIMEOFDAY)
    #include <sys/time.h>
    #include <unistd.h>
#elif defined(HAVE_FTIME)
    #include <sys/timeb.h>
#endif

#ifdef __WXMAC__
    #include <Timer.h>
    #include <DriverServices.h>
#endif
// ----------------------------------------------------------------------------
// wxWin macros
// ----------------------------------------------------------------------------

#if wxUSE_GUI && wxUSE_TIMER
    IMPLEMENT_DYNAMIC_CLASS(wxTimerEvent, wxEvent)
#endif // wxUSE_GUI

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

// on some really old systems gettimeofday() doesn't have the second argument,
// define wxGetTimeOfDay() to hide this difference
#ifdef HAVE_GETTIMEOFDAY
    #ifdef WX_GETTIMEOFDAY_NO_TZ
        struct timezone;
        #define wxGetTimeOfDay(tv, tz)      gettimeofday(tv)
    #else
        #define wxGetTimeOfDay(tv, tz)      gettimeofday((tv), (tz))
    #endif
#endif // HAVE_GETTIMEOFDAY

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxTimerBase
// ----------------------------------------------------------------------------

#if wxUSE_GUI && wxUSE_TIMER

wxTimerBase::~wxTimerBase()
{
    // this destructor is required for Darwin
}

void wxTimerBase::Notify()
{
    // the base class version generates an event if it has owner - which it
    // should because otherwise nobody can process timer events
    wxCHECK_RET( m_owner, _T("wxTimer::Notify() should be overridden.") );

    wxTimerEvent event(m_idTimer, m_milli);
    (void)m_owner->ProcessEvent(event);
}

bool wxTimerBase::Start(int milliseconds, bool oneShot)
{
    // under MSW timers only work when they're started from the main thread so
    // let the caller know about it
#if wxUSE_THREADS
    wxASSERT_MSG( wxThread::IsMain(),
                  _T("timer can only be started from the main thread") );
#endif // wxUSE_THREADS

    if ( IsRunning() )
    {
        // not stopping the already running timer might work for some
        // platforms (no problems under MSW) but leads to mysterious crashes
        // on the others (GTK), so to be on the safe side do it here
        Stop();
    }

    if ( milliseconds != -1 )
    {
        m_milli = milliseconds;
    }

    m_oneShot = oneShot;

    return TRUE;
}

#endif // wxUSE_GUI

// ----------------------------------------------------------------------------
// wxStopWatch
// ----------------------------------------------------------------------------

#if wxUSE_LONGLONG

void wxStopWatch::Start(long t)
{
    m_t0 = wxGetLocalTimeMillis() - t;
    m_pause = 0;
}

long wxStopWatch::GetElapsedTime() const
{
    return (wxGetLocalTimeMillis() - m_t0).GetLo();
}

long wxStopWatch::Time() const
{
    return m_pauseCount ? m_pause : GetElapsedTime();
}

#endif // wxUSE_LONGLONG

// ----------------------------------------------------------------------------
// old timer functions superceded by wxStopWatch
// ----------------------------------------------------------------------------

#if wxUSE_LONGLONG

static wxLongLong wxStartTime = 0l;

// starts the global timer
void wxStartTimer()
{
    wxStartTime = wxGetLocalTimeMillis();
}

// Returns elapsed time in milliseconds
long wxGetElapsedTime(bool resetTimer)
{
    wxLongLong oldTime = wxStartTime;
    wxLongLong newTime = wxGetLocalTimeMillis();

    if ( resetTimer )
        wxStartTime = newTime;

    return (newTime - oldTime).GetLo();
}

#endif // wxUSE_LONGLONG

// ----------------------------------------------------------------------------
// the functions to get the current time and timezone info
// ----------------------------------------------------------------------------

// Get local time as seconds since 00:00:00, Jan 1st 1970
long wxGetLocalTime()
{
    struct tm tm;
    time_t t0, t1;

    // This cannot be made static because mktime can overwrite it.
    //
    memset(&tm, 0, sizeof(tm));
    tm.tm_year  = 70;
    tm.tm_mon   = 0;
    tm.tm_mday  = 5;        // not Jan 1st 1970 due to mktime 'feature'
    tm.tm_hour  = 0;
    tm.tm_min   = 0;
    tm.tm_sec   = 0;
    tm.tm_isdst = -1;       // let mktime guess

    // Note that mktime assumes that the struct tm contains local time.
    //
    t1 = time(&t1);         // now
    t0 = mktime(&tm);       // origin

    // Return the difference in seconds.
    //
    if (( t0 != (time_t)-1 ) && ( t1 != (time_t)-1 ))
        return (long)difftime(t1, t0) + (60 * 60 * 24 * 4);

    wxLogSysError(_("Failed to get the local system time"));
    return -1;
}

// Get UTC time as seconds since 00:00:00, Jan 1st 1970
long wxGetUTCTime()
{
    struct tm tm;
    struct tm *ptm;
    time_t t0, t1;

    // This cannot be made static because mktime can overwrite it
    //
    memset(&tm, 0, sizeof(tm));
    tm.tm_year  = 70;
    tm.tm_mon   = 0;
    tm.tm_mday  = 5;        // not Jan 1st 1970 due to mktime 'feature'
    tm.tm_hour  = 0;
    tm.tm_min   = 0;
    tm.tm_sec   = 0;
    tm.tm_isdst = -1;       // let mktime guess

    // Note that mktime assumes that the struct tm contains local time.
    //
    t1 = time(&t1);         // now
    t0 = mktime(&tm);       // origin in localtime

    if (( t0 != (time_t)-1 ) && ( t1 != (time_t)-1 ))
    {
        // To get t0 as GMT we convert to a struct tm with gmtime,
        // and then back again.
        //
        ptm = gmtime(&t0);

        if (ptm)
        {
            memcpy(&tm, ptm, sizeof(tm));
            t0 = mktime(&tm);

            if (t0 != (time_t)-1 )
                return (long)difftime(t1, t0) + (60 * 60 * 24 * 4);
            wxLogSysError(_("mktime() failed"));
        }
        else
        {
            wxLogSysError(_("gmtime() failed"));
        }
    }

    wxLogError(_("Failed to get the UTC system time."));

    return -1;
}

#if wxUSE_LONGLONG

// Get local time as milliseconds since 00:00:00, Jan 1st 1970
wxLongLong wxGetLocalTimeMillis()
{
    wxLongLong val = 1000l;

    // If possible, use a function which avoids conversions from
    // broken-up time structures to milliseconds

#if defined(__WXMSW__) && defined(__MWERKS__)
    // This should probably be the way all WXMSW compilers should do it
    // Go direct to the OS for time

    SYSTEMTIME thenst = { 1970, 1, 4, 1, 0, 0, 0, 0 };  // 00:00:00 Jan 1st 1970
    FILETIME thenft;
    SystemTimeToFileTime( &thenst, &thenft );
    wxLongLong then( thenft.dwHighDateTime, thenft.dwLowDateTime );   // time in 100 nanoseconds

    SYSTEMTIME nowst;
    GetLocalTime( &nowst );
    FILETIME nowft;
    SystemTimeToFileTime( &nowst, &nowft );
    wxLongLong now( nowft.dwHighDateTime, nowft.dwLowDateTime );   // time in 100 nanoseconds

    return ( now - then ) / 10000.0;  // time from 00:00:00 Jan 1st 1970 to now in milliseconds
    
#elif defined(HAVE_GETTIMEOFDAY)
    struct timeval tp;
    if ( wxGetTimeOfDay(&tp, (struct timezone *)NULL) != -1 )
    {
        val *= tp.tv_sec;
        return (val + (tp.tv_usec / 1000));
    }
    else
    {
        wxLogError(_("wxGetTimeOfDay failed."));
        return 0;
    }
#elif defined(HAVE_FTIME)
    struct timeb tp;

    // ftime() is void and not int in some mingw32 headers, so don't
    // test the return code (well, it shouldn't fail anyhow...)
    (void)ftime(&tp);
    val *= tp.time;
    return (val + tp.millitm);
#elif defined(__WXMAC__)
    
    UInt64 gMilliAtStart = 0 ;
    Nanoseconds upTime = AbsoluteToNanoseconds( UpTime() ) ;
    if ( gMilliAtStart == 0 )
    {
        time_t start = time(NULL) ;
        gMilliAtStart = ((UInt64) start) * 1000L ;
        gMilliAtStart -= upTime.lo / 1000 ;
        gMilliAtStart -= ( ( (UInt64) upTime.hi ) << 32 ) / 1000 ;
    }
    UInt64 millival = gMilliAtStart ;
    millival += upTime.lo / 1000 ;
    millival += ( ( (UInt64) upTime.hi ) << 32 ) / 1000 ;
    val = millival ;
    return val ;
#else // no gettimeofday() nor ftime()
    // We use wxGetLocalTime() to get the seconds since
    // 00:00:00 Jan 1st 1970 and then whatever is available
    // to get millisecond resolution.
    //
    // NOTE that this might lead to a problem if the clocks
    // use different sources, so this approach should be
    // avoided where possible.

    val *= wxGetLocalTime();

// GRG: This will go soon as all WIN32 seem to have ftime
#if defined (__WIN32__)
    // If your platform/compiler needs to use two different functions
    // to get ms resolution, please do NOT just shut off these warnings,
    // drop me a line instead at <guille@iies.es>
    #warning "Possible clock skew bug in wxGetLocalTimeMillis()!"

    SYSTEMTIME st;
    ::GetLocalTime(&st);
    val += st.wMilliseconds;
#else // !Win32
    // If your platform/compiler does not support ms resolution please
    // do NOT just shut off these warnings, drop me a line instead at
    // <guille@iies.es>

    #if defined(__VISUALC__) || defined (__WATCOMC__)
        #pragma message("wxStopWatch will be up to second resolution!")
    #elif defined(__BORLANDC__)
        #pragma message "wxStopWatch will be up to second resolution!"
    #else
        #warning "wxStopWatch will be up to second resolution!"
    #endif // compiler
#endif

    return val;

#endif // time functions
}

#endif // wxUSE_LONGLONG

