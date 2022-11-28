/////////////////////////////////////////////////////////////////////////////
// Name:        unix/utilsunx.cpp
// Purpose:     generic Unix implementation of many wx functions
// Author:      Vadim Zeitlin
// Id:          $Id: utilsunx.cpp,v 1.86.2.4 2002/11/14 13:51:57 GD Exp $
// Copyright:   (c) 1998 Robert Roebling, Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/defs.h"
#include "wx/string.h"

#include "wx/intl.h"
#include "wx/log.h"
#include "wx/app.h"

#include "wx/utils.h"
#include "wx/process.h"
#include "wx/thread.h"

#include "wx/wfstream.h"

#ifdef HAVE_STATFS
#  ifdef __BSD__
#    include <sys/param.h>
#    include <sys/mount.h>
#  else
#    include <sys/vfs.h>
#  endif
#endif // HAVE_STATFS

// not only the statfs syscall is called differently depending on platform, but
// we also can't use "struct statvfs" under Solaris because it breaks down if
// HAVE_LARGEFILE_SUPPORT == 1 and we must use statvfs_t instead
#ifdef HAVE_STATVFS
    #include <sys/statvfs.h>

    #define statfs statvfs
# ifdef __HPUX__
    #define wxStatFs struct statvfs
# else
    #define wxStatFs statvfs_t
# endif
#elif HAVE_STATFS
    #define wxStatFs struct statfs
#endif // HAVE_STAT[V]FS

#if wxUSE_GUI
    #include "wx/unix/execute.h"
#endif

// SGI signal.h defines signal handler arguments differently depending on
// whether _LANGUAGE_C_PLUS_PLUS is set or not - do set it
#if defined(__SGI__) && !defined(_LANGUAGE_C_PLUS_PLUS)
    #define _LANGUAGE_C_PLUS_PLUS 1
#endif // SGI hack

#include <stdarg.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>          // for O_WRONLY and friends
#include <time.h>           // nanosleep() and/or usleep()
#include <ctype.h>          // isspace()
#include <sys/time.h>       // needed for FD_SETSIZE

#ifdef HAVE_UNAME
    #include <sys/utsname.h> // for uname()
#endif // HAVE_UNAME

// ----------------------------------------------------------------------------
// conditional compilation
// ----------------------------------------------------------------------------

// many versions of Unices have this function, but it is not defined in system
// headers - please add your system here if it is the case for your OS.
// SunOS < 5.6 (i.e. Solaris < 2.6) and DG-UX are like this.
#if !defined(HAVE_USLEEP) && \
    (defined(__SUN__) && !defined(__SunOs_5_6) && \
                         !defined(__SunOs_5_7) && !defined(__SUNPRO_CC)) || \
     defined(__osf__) || defined(__EMX__)
    extern "C"
    {
        #ifdef __SUN__
            int usleep(unsigned int usec);
        #else // !Sun
            #ifdef __EMX__
                /* I copied this from the XFree86 diffs. AV. */
                #define INCL_DOSPROCESS
                #include <os2.h>
                inline void usleep(unsigned long delay)
                {
                    DosSleep(delay ? (delay/1000l) : 1l);
                }
            #else // !Sun && !EMX
                void usleep(unsigned long usec);
            #endif
        #endif // Sun/EMX/Something else
    };

    #define HAVE_USLEEP 1
#endif // Unices without usleep()

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// sleeping
// ----------------------------------------------------------------------------

void wxSleep(int nSecs)
{
    sleep(nSecs);
}

void wxUsleep(unsigned long milliseconds)
{
#if defined(HAVE_NANOSLEEP)
    timespec tmReq;
    tmReq.tv_sec = (time_t)(milliseconds / 1000);
    tmReq.tv_nsec = (milliseconds % 1000) * 1000 * 1000;

    // we're not interested in remaining time nor in return value
    (void)nanosleep(&tmReq, (timespec *)NULL);
#elif defined(HAVE_USLEEP)
    // uncomment this if you feel brave or if you are sure that your version
    // of Solaris has a safe usleep() function but please notice that usleep()
    // is known to lead to crashes in MT programs in Solaris 2.[67] and is not
    // documented as MT-Safe
    #if defined(__SUN__) && wxUSE_THREADS
        #error "usleep() cannot be used in MT programs under Solaris."
    #endif // Sun

    usleep(milliseconds * 1000); // usleep(3) wants microseconds
#elif defined(HAVE_SLEEP)
    // under BeOS sleep() takes seconds (what about other platforms, if any?)
    sleep(milliseconds * 1000);
#else // !sleep function
    #error "usleep() or nanosleep() function required for wxUsleep"
#endif // sleep function
}

// ----------------------------------------------------------------------------
// process management
// ----------------------------------------------------------------------------

int wxKill(long pid, wxSignal sig, wxKillError *rc)
{
    int err = kill((pid_t)pid, (int)sig);
    if ( rc )
    {
        switch ( errno )
        {
            case 0:
                *rc = wxKILL_OK;
                break;

            case EINVAL:
                *rc = wxKILL_BAD_SIGNAL;
                break;

            case EPERM:
                *rc = wxKILL_ACCESS_DENIED;
                break;

            case ESRCH:
                *rc = wxKILL_NO_PROCESS;
                break;

            default:
                // this goes against Unix98 docs so log it
                wxLogDebug(_T("unexpected kill(2) return value %d"), err);

                // something else...
                *rc = wxKILL_ERROR;
        }
    }

    return err;
}

#define WXEXECUTE_NARGS   127

long wxExecute( const wxString& command, int flags, wxProcess *process )
{
    wxCHECK_MSG( !command.IsEmpty(), 0, wxT("can't exec empty command") );

    int argc = 0;
    wxChar *argv[WXEXECUTE_NARGS];
    wxString argument;
    const wxChar *cptr = command.c_str();
    wxChar quotechar = wxT('\0'); // is arg quoted?
    bool escaped = FALSE;

    // split the command line in arguments
    do
    {
        argument=wxT("");
        quotechar = wxT('\0');

        // eat leading whitespace:
        while ( wxIsspace(*cptr) )
            cptr++;

        if ( *cptr == wxT('\'') || *cptr == wxT('"') )
            quotechar = *cptr++;

        do
        {
            if ( *cptr == wxT('\\') && ! escaped )
            {
                escaped = TRUE;
                cptr++;
                continue;
            }

            // all other characters:
            argument += *cptr++;
            escaped = FALSE;

            // have we reached the end of the argument?
            if ( (*cptr == quotechar && ! escaped)
                 || (quotechar == wxT('\0') && wxIsspace(*cptr))
                 || *cptr == wxT('\0') )
            {
                wxASSERT_MSG( argc < WXEXECUTE_NARGS,
                              wxT("too many arguments in wxExecute") );

                argv[argc] = new wxChar[argument.length() + 1];
                wxStrcpy(argv[argc], argument.c_str());
                argc++;

                // if not at end of buffer, swallow last character:
                if(*cptr)
                    cptr++;

                break; // done with this one, start over
            }
        } while(*cptr);
    } while(*cptr);
    argv[argc] = NULL;

    // do execute the command
    long lRc = wxExecute(argv, flags, process);

    // clean up
    argc = 0;
    while( argv[argc] )
        delete [] argv[argc++];

    return lRc;
}

// ----------------------------------------------------------------------------
// wxShell
// ----------------------------------------------------------------------------

static wxString wxMakeShellCommand(const wxString& command)
{
    wxString cmd;
    if ( !command )
    {
        // just an interactive shell
        cmd = _T("xterm");
    }
    else
    {
        // execute command in a shell
        cmd << _T("/bin/sh -c '") << command << _T('\'');
    }

    return cmd;
}

bool wxShell(const wxString& command)
{
    return wxExecute(wxMakeShellCommand(command), wxEXEC_SYNC) == 0;
}

bool wxShell(const wxString& command, wxArrayString& output)
{
    wxCHECK_MSG( !!command, FALSE, _T("can't exec shell non interactively") );

    return wxExecute(wxMakeShellCommand(command), output);
}

// Shutdown or reboot the PC
bool wxShutdown(wxShutdownFlags wFlags)
{
    wxChar level;
    switch ( wFlags )
    {
        case wxSHUTDOWN_POWEROFF:
            level = _T('0');
            break;

        case wxSHUTDOWN_REBOOT:
            level = _T('6');
            break;

        default:
            wxFAIL_MSG( _T("unknown wxShutdown() flag") );
            return FALSE;
    }

    return system(wxString::Format(_T("init %c"), level).mb_str()) == 0;
}


#if wxUSE_GUI

void wxHandleProcessTermination(wxEndProcessData *proc_data)
{
    // notify user about termination if required
    if ( proc_data->process )
    {
        proc_data->process->OnTerminate(proc_data->pid, proc_data->exitcode);
    }

    // clean up
    if ( proc_data->pid > 0 )
    {
       delete proc_data;
    }
    else
    {
       // let wxExecute() know that the process has terminated
       proc_data->pid = 0;
    }
}

#endif // wxUSE_GUI

// ----------------------------------------------------------------------------
// wxStream classes to support IO redirection in wxExecute
// ----------------------------------------------------------------------------

#if wxUSE_STREAMS

// ----------------------------------------------------------------------------
// wxPipeInputStream: stream for reading from a pipe
// ----------------------------------------------------------------------------

class wxPipeInputStream : public wxFileInputStream
{
public:
    wxPipeInputStream(int fd) : wxFileInputStream(fd) { }

    // return TRUE if the pipe is still opened
    bool IsOpened() const { return !Eof(); }

    // return TRUE if we have anything to read, don't block
    virtual bool CanRead() const;
};

bool wxPipeInputStream::CanRead() const
{
    if ( m_lasterror == wxSTREAM_EOF )
        return FALSE;

    // check if there is any input available
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    const int fd = m_file->fd();

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    switch ( select(fd + 1, &readfds, NULL, NULL, &tv) )
    {
        case -1:
            wxLogSysError(_("Impossible to get child process input"));
            // fall through

        case 0:
            return FALSE;

        default:
            wxFAIL_MSG(_T("unexpected select() return value"));
            // still fall through

        case 1:
            // input available -- or maybe not, as select() returns 1 when a
            // read() will complete without delay, but it could still not read
            // anything
            return !Eof();
    }
}

// define this to let wxexec.cpp know that we know what we're doing
#define _WX_USED_BY_WXEXECUTE_
#include "../common/execcmn.cpp"

#endif // wxUSE_STREAMS

// ----------------------------------------------------------------------------
// wxPipe: this encapsulates pipe() system call
// ----------------------------------------------------------------------------

class wxPipe
{
public:
    // the symbolic names for the pipe ends
    enum Direction
    {
        Read,
        Write
    };

    enum
    {
        INVALID_FD = -1
    };

    // default ctor doesn't do anything
    wxPipe() { m_fds[Read] = m_fds[Write] = INVALID_FD; }

    // create the pipe, return TRUE if ok, FALSE on error
    bool Create()
    {
        if ( pipe(m_fds) == -1 )
        {
            wxLogSysError(_("Pipe creation failed"));

            return FALSE;
        }

        return TRUE;
    }

    // return TRUE if we were created successfully
    bool IsOk() const { return m_fds[Read] != INVALID_FD; }

    // return the descriptor for one of the pipe ends
    int operator[](Direction which) const
    {
        wxASSERT_MSG( which >= 0 && (size_t)which < WXSIZEOF(m_fds),
                      _T("invalid pipe index") );

        return m_fds[which];
    }

    // detach a descriptor, meaning that the pipe dtor won't close it, and
    // return it
    int Detach(Direction which)
    {
        wxASSERT_MSG( which >= 0 && (size_t)which < WXSIZEOF(m_fds),
                      _T("invalid pipe index") );

        int fd = m_fds[which];
        m_fds[which] = INVALID_FD;

        return fd;
    }

    // close the pipe descriptors
    void Close()
    {
        for ( size_t n = 0; n < WXSIZEOF(m_fds); n++ )
        {
            if ( m_fds[n] != INVALID_FD )
                close(m_fds[n]);
        }
    }

    // dtor closes the pipe descriptors
    ~wxPipe() { Close(); }

private:
    int m_fds[2];
};

// ----------------------------------------------------------------------------
// wxExecute: the real worker function
// ----------------------------------------------------------------------------

#ifdef __VMS
    #pragma message disable codeunreachable
#endif

long wxExecute(wxChar **argv,
               int flags,
               wxProcess *process)
{
    // for the sync execution, we return -1 to indicate failure, but for async
    // case we return 0 which is never a valid PID
    //
    // we define this as a macro, not a variable, to avoid compiler warnings
    // about "ERROR_RETURN_CODE value may be clobbered by fork()"
    #define ERROR_RETURN_CODE ((flags & wxEXEC_SYNC) ? -1 : 0)

    wxCHECK_MSG( *argv, ERROR_RETURN_CODE, wxT("can't exec empty command") );

#if wxUSE_UNICODE
    int mb_argc = 0;
    char *mb_argv[WXEXECUTE_NARGS];

    while (argv[mb_argc])
    {
        wxWX2MBbuf mb_arg = wxConvertWX2MB(argv[mb_argc]);
        mb_argv[mb_argc] = strdup(mb_arg);
        mb_argc++;
    }
    mb_argv[mb_argc] = (char *) NULL;

    // this macro will free memory we used above
    #define ARGS_CLEANUP                                 \
        for ( mb_argc = 0; mb_argv[mb_argc]; mb_argc++ ) \
            free(mb_argv[mb_argc])
#else // ANSI
    // no need for cleanup
    #define ARGS_CLEANUP

    wxChar **mb_argv = argv;
#endif // Unicode/ANSI

#if wxUSE_GUI
    // create pipes
    wxPipe pipeEndProcDetect;
    if ( !pipeEndProcDetect.Create() )
    {
        wxLogError( _("Failed to execute '%s'\n"), *argv );

        ARGS_CLEANUP;

        return ERROR_RETURN_CODE;
    }
#endif // wxUSE_GUI

    // pipes for inter process communication
    wxPipe pipeIn,      // stdin
           pipeOut,     // stdout
           pipeErr;     // stderr

    if ( process && process->IsRedirected() )
    {
        if ( !pipeIn.Create() || !pipeOut.Create() || !pipeErr.Create() )
        {
            wxLogError( _("Failed to execute '%s'\n"), *argv );

            ARGS_CLEANUP;

            return ERROR_RETURN_CODE;
        }
    }

    // fork the process
    //
    // NB: do *not* use vfork() here, it completely breaks this code for some
    //     reason under Solaris (and maybe others, although not under Linux)
    //     But on OpenVMS we do not have fork so we have to use vfork and
    //     cross our fingers that it works.
#ifdef __VMS
   pid_t pid = vfork();
#else
   pid_t pid = fork();
#endif
   if ( pid == -1 )     // error?
    {
        wxLogSysError( _("Fork failed") );

        ARGS_CLEANUP;

        return ERROR_RETURN_CODE;
    }
    else if ( pid == 0 )  // we're in child
    {
        // These lines close the open file descriptors to to avoid any
        // input/output which might block the process or irritate the user. If
        // one wants proper IO for the subprocess, the right thing to do is to
        // start an xterm executing it.
        if ( !(flags & wxEXEC_SYNC) )
        {
            for ( int fd = 0; fd < FD_SETSIZE; fd++ )
            {
                if ( fd == pipeIn[wxPipe::Read]
                        || fd == pipeOut[wxPipe::Write]
                        || fd == pipeErr[wxPipe::Write]
#if wxUSE_GUI
                        || fd == pipeEndProcDetect[wxPipe::Write]
#endif // wxUSE_GUI
                   )
                {
                    // don't close this one, we still need it
                    continue;
                }

                // leave stderr opened too, it won't do any harm
                if ( fd != STDERR_FILENO )
                    close(fd);
            }
        }

#if !defined(__VMS) && !defined(__EMX__)
        if ( flags & wxEXEC_MAKE_GROUP_LEADER )
        {
            // Set process group to child process' pid.  Then killing -pid
            // of the parent will kill the process and all of its children.
            setsid();
        }
#endif // !__VMS

#if wxUSE_GUI
        // reading side can be safely closed but we should keep the write one
        // opened
        pipeEndProcDetect.Detach(wxPipe::Write);
        pipeEndProcDetect.Close();
#endif // wxUSE_GUI

        // redirect stdin, stdout and stderr
        if ( pipeIn.IsOk() )
        {
            if ( dup2(pipeIn[wxPipe::Read], STDIN_FILENO) == -1 ||
                 dup2(pipeOut[wxPipe::Write], STDOUT_FILENO) == -1 ||
                 dup2(pipeErr[wxPipe::Write], STDERR_FILENO) == -1 )
            {
                wxLogSysError(_("Failed to redirect child process input/output"));
            }

            pipeIn.Close();
            pipeOut.Close();
            pipeErr.Close();
        }

        execvp (*mb_argv, mb_argv);

        // there is no return after successful exec()
        _exit(-1);

        // some compilers complain about missing return - of course, they
        // should know that exit() doesn't return but what else can we do if
        // they don't?
        //
        // and, sure enough, other compilers complain about unreachable code
        // after exit() call, so we can just always have return here...
#if defined(__VMS) || defined(__INTEL_COMPILER)
        return 0;
#endif
    }
    else // we're in parent
    {
        ARGS_CLEANUP;

        // prepare for IO redirection

#if wxUSE_STREAMS
        // the input buffer bufOut is connected to stdout, this is why it is
        // called bufOut and not bufIn
        wxStreamTempInputBuffer bufOut,
                                bufErr;
#endif // wxUSE_STREAMS

        if ( process && process->IsRedirected() )
        {
#if wxUSE_STREAMS
            wxOutputStream *inStream =
                new wxFileOutputStream(pipeIn.Detach(wxPipe::Write));

            wxPipeInputStream *outStream =
                new wxPipeInputStream(pipeOut.Detach(wxPipe::Read));

            wxPipeInputStream *errStream =
                new wxPipeInputStream(pipeErr.Detach(wxPipe::Read));

            process->SetPipeStreams(outStream, inStream, errStream);

            bufOut.Init(outStream);
            bufErr.Init(errStream);
#endif // wxUSE_STREAMS
        }

        if ( pipeIn.IsOk() )
        {
            pipeIn.Close();
            pipeOut.Close();
            pipeErr.Close();
        }

#if wxUSE_GUI && !defined(__WXMICROWIN__)
        wxEndProcessData *data = new wxEndProcessData;

        data->tag = wxAddProcessCallback
                    (
                        data,
                        pipeEndProcDetect.Detach(wxPipe::Read)
                    );

        pipeEndProcDetect.Close();

        if ( flags & wxEXEC_SYNC )
        {
            // we may have process for capturing the program output, but it's
            // not used in wxEndProcessData in the case of sync execution
            data->process = NULL;

            // sync execution: indicate it by negating the pid
            data->pid = -pid;

            wxBusyCursor bc;
            wxWindowDisabler wd;

            // data->pid will be set to 0 from GTK_EndProcessDetector when the
            // process terminates
            while ( data->pid != 0 )
            {
#if wxUSE_STREAMS
                bufOut.Update();
                bufErr.Update();
#endif // wxUSE_STREAMS

                // give GTK+ a chance to call GTK_EndProcessDetector here and
                // also repaint the GUI
                wxYield();
            }

            int exitcode = data->exitcode;

            delete data;

            return exitcode;
        }
        else // async execution
        {
            // async execution, nothing special to do - caller will be
            // notified about the process termination if process != NULL, data
            // will be deleted in GTK_EndProcessDetector
            data->process  = process;
            data->pid      = pid;

            return pid;
        }
#else // !wxUSE_GUI

        wxASSERT_MSG( flags & wxEXEC_SYNC,
                      wxT("async execution not supported yet") );

        int exitcode = 0;
        if ( waitpid(pid, &exitcode, 0) == -1 || !WIFEXITED(exitcode) )
        {
            wxLogSysError(_("Waiting for subprocess termination failed"));
        }

        return exitcode;
#endif // wxUSE_GUI
    }

    return ERROR_RETURN_CODE;
}

#ifdef __VMS
    #pragma message enable codeunreachable
#endif

#undef ERROR_RETURN_CODE
#undef ARGS_CLEANUP

// ----------------------------------------------------------------------------
// file and directory functions
// ----------------------------------------------------------------------------

const wxChar* wxGetHomeDir( wxString *home  )
{
    *home = wxGetUserHome( wxString() );
    wxString tmp;
    if ( home->IsEmpty() )
        *home = wxT("/");
#ifdef __VMS
    tmp = *home;
    if ( tmp.Last() != wxT(']'))
        if ( tmp.Last() != wxT('/')) *home << wxT('/');
#endif
    return home->c_str();
}

#if wxUSE_UNICODE
const wxMB2WXbuf wxGetUserHome( const wxString &user )
#else // just for binary compatibility -- there is no 'const' here
char *wxGetUserHome( const wxString &user )
#endif
{
    struct passwd *who = (struct passwd *) NULL;

    if ( !user )
    {
        wxChar *ptr;

        if ((ptr = wxGetenv(wxT("HOME"))) != NULL)
        {
#if wxUSE_UNICODE
            wxWCharBuffer buffer( ptr );
            return buffer;
#else
            return ptr;
#endif
        }
        if ((ptr = wxGetenv(wxT("USER"))) != NULL || (ptr = wxGetenv(wxT("LOGNAME"))) != NULL)
        {
            who = getpwnam(wxConvertWX2MB(ptr));
        }

        // We now make sure the the user exists!
        if (who == NULL)
        {
            who = getpwuid(getuid());
        }
    }
    else
    {
      who = getpwnam (user.mb_str());
    }

    return wxConvertMB2WX(who ? who->pw_dir : 0);
}

// ----------------------------------------------------------------------------
// network and user id routines
// ----------------------------------------------------------------------------

// retrieve either the hostname or FQDN depending on platform (caller must
// check whether it's one or the other, this is why this function is for
// private use only)
static bool wxGetHostNameInternal(wxChar *buf, int sz)
{
    wxCHECK_MSG( buf, FALSE, wxT("NULL pointer in wxGetHostNameInternal") );

    *buf = wxT('\0');

    // we're using uname() which is POSIX instead of less standard sysinfo()
#if defined(HAVE_UNAME)
    struct utsname uts;
    bool ok = uname(&uts) != -1;
    if ( ok )
    {
        wxStrncpy(buf, wxConvertMB2WX(uts.nodename), sz - 1);
        buf[sz] = wxT('\0');
    }
#elif defined(HAVE_GETHOSTNAME)
    bool ok = gethostname(buf, sz) != -1;
#else // no uname, no gethostname
    wxFAIL_MSG(wxT("don't know host name for this machine"));

    bool ok = FALSE;
#endif // uname/gethostname

    if ( !ok )
    {
        wxLogSysError(_("Cannot get the hostname"));
    }

    return ok;
}

bool wxGetHostName(wxChar *buf, int sz)
{
    bool ok = wxGetHostNameInternal(buf, sz);

    if ( ok )
    {
        // BSD systems return the FQDN, we only want the hostname, so extract
        // it (we consider that dots are domain separators)
        wxChar *dot = wxStrchr(buf, wxT('.'));
        if ( dot )
        {
            // nuke it
            *dot = wxT('\0');
        }
    }

    return ok;
}

bool wxGetFullHostName(wxChar *buf, int sz)
{
    bool ok = wxGetHostNameInternal(buf, sz);

    if ( ok )
    {
        if ( !wxStrchr(buf, wxT('.')) )
        {
            struct hostent *host = gethostbyname(wxConvertWX2MB(buf));
            if ( !host )
            {
                wxLogSysError(_("Cannot get the official hostname"));

                ok = FALSE;
            }
            else
            {
                // the canonical name
                wxStrncpy(buf, wxConvertMB2WX(host->h_name), sz);
            }
        }
        //else: it's already a FQDN (BSD behaves this way)
    }

    return ok;
}

bool wxGetUserId(wxChar *buf, int sz)
{
    struct passwd *who;

    *buf = wxT('\0');
    if ((who = getpwuid(getuid ())) != NULL)
    {
        wxStrncpy (buf, wxConvertMB2WX(who->pw_name), sz - 1);
        return TRUE;
    }

    return FALSE;
}

bool wxGetUserName(wxChar *buf, int sz)
{
    struct passwd *who;

    *buf = wxT('\0');
    if ((who = getpwuid (getuid ())) != NULL)
    {
        // pw_gecos field in struct passwd is not standard
#ifdef HAVE_PW_GECOS
       char *comma = strchr(who->pw_gecos, ',');
       if (comma)
           *comma = '\0'; // cut off non-name comment fields
       wxStrncpy (buf, wxConvertMB2WX(who->pw_gecos), sz - 1);
#else // !HAVE_PW_GECOS
       wxStrncpy (buf, wxConvertMB2WX(who->pw_name), sz - 1);
#endif // HAVE_PW_GECOS/!HAVE_PW_GECOS
       return TRUE;
    }

    return FALSE;
}

#ifndef __WXMAC__
wxString wxGetOsDescription()
{
#ifndef WXWIN_OS_DESCRIPTION
    #error WXWIN_OS_DESCRIPTION should be defined in config.h by configure
#else
    return wxString::FromAscii( WXWIN_OS_DESCRIPTION );
#endif
}
#endif

// this function returns the GUI toolkit version in GUI programs, but OS
// version in non-GUI ones
#if !wxUSE_GUI

int wxGetOsVersion(int *majorVsn, int *minorVsn)
{
    int major, minor;
    char name[256];

    if ( sscanf(WXWIN_OS_DESCRIPTION, "%s %d.%d", name, &major, &minor) != 3 )
    {
        // unreckognized uname string format
        major = minor = -1;
    }

    if ( majorVsn )
        *majorVsn = major;
    if ( minorVsn )
        *minorVsn = minor;

    return wxUNIX;
}

#endif // !wxUSE_GUI

unsigned long wxGetProcessId()
{
    return (unsigned long)getpid();
}

long wxGetFreeMemory()
{
#if defined(__LINUX__)
    // get it from /proc/meminfo
    FILE *fp = fopen("/proc/meminfo", "r");
    if ( fp )
    {
        long memFree = -1;

        char buf[1024];
        if ( fgets(buf, WXSIZEOF(buf), fp) && fgets(buf, WXSIZEOF(buf), fp) )
        {
            long memTotal, memUsed;
            sscanf(buf, "Mem: %ld %ld %ld", &memTotal, &memUsed, &memFree);
        }

        fclose(fp);

        return memFree;
    }
#elif defined(__SUN__) && defined(_SC_AVPHYS_PAGES)
    return sysconf(_SC_AVPHYS_PAGES)*sysconf(_SC_PAGESIZE);
//#elif defined(__FREEBSD__) -- might use sysctl() to find it out, probably
#endif

    // can't find it out
    return -1;
}

bool wxGetDiskSpace(const wxString& path, wxLongLong *pTotal, wxLongLong *pFree)
{
#if defined(HAVE_STATFS) || defined(HAVE_STATVFS)
    // the case to "char *" is needed for AIX 4.3
    wxStatFs fs;
    if ( statfs((char *)(const char*)path.fn_str(), &fs) != 0 )
    {
        wxLogSysError( wxT("Failed to get file system statistics") );

        return FALSE;
    }

    // under Solaris we also have to use f_frsize field instead of f_bsize
    // which is in general a multiple of f_frsize
#ifdef HAVE_STATVFS
    wxLongLong blockSize = fs.f_frsize;
#else // HAVE_STATFS
    wxLongLong blockSize = fs.f_bsize;
#endif // HAVE_STATVFS/HAVE_STATFS

    if ( pTotal )
    {
        *pTotal = wxLongLong(fs.f_blocks) * blockSize;
    }

    if ( pFree )
    {
        *pFree = wxLongLong(fs.f_bavail) * blockSize;
    }

    return TRUE;
#else // !HAVE_STATFS && !HAVE_STATVFS
    return FALSE;
#endif // HAVE_STATFS
}

// ----------------------------------------------------------------------------
// env vars
// ----------------------------------------------------------------------------

bool wxGetEnv(const wxString& var, wxString *value)
{
    // wxGetenv is defined as getenv()
    wxChar *p = wxGetenv(var);
    if ( !p )
        return FALSE;

    if ( value )
    {
        *value = p;
    }

    return TRUE;
}

bool wxSetEnv(const wxString& variable, const wxChar *value)
{
#if defined(HAVE_SETENV)
    return setenv(variable.mb_str(),
                  value ? (const char *)wxString(value).mb_str()
                        : NULL,
                  1 /* overwrite */) == 0;
#elif defined(HAVE_PUTENV)
    wxString s = variable;
    if ( value )
        s << _T('=') << value;

    // transform to ANSI
    const char *p = s.mb_str();

    // the string will be free()d by libc
    char *buf = (char *)malloc(strlen(p) + 1);
    strcpy(buf, p);

    return putenv(buf) == 0;
#else // no way to set an env var
    return FALSE;
#endif
}

// ----------------------------------------------------------------------------
// signal handling
// ----------------------------------------------------------------------------

#if wxUSE_ON_FATAL_EXCEPTION

#include <signal.h>

extern "C" void wxFatalSignalHandler(wxTYPE_SA_HANDLER)
{
    if ( wxTheApp )
    {
        // give the user a chance to do something special about this
        wxTheApp->OnFatalException();
    }

    abort();
}

bool wxHandleFatalExceptions(bool doit)
{
    // old sig handlers
    static bool s_savedHandlers = FALSE;
    static struct sigaction s_handlerFPE,
                            s_handlerILL,
                            s_handlerBUS,
                            s_handlerSEGV;

    bool ok = TRUE;
    if ( doit && !s_savedHandlers )
    {
        // install the signal handler
        struct sigaction act;

        // some systems extend it with non std fields, so zero everything
        memset(&act, 0, sizeof(act));

        act.sa_handler = wxFatalSignalHandler;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;

        ok &= sigaction(SIGFPE, &act, &s_handlerFPE) == 0;
        ok &= sigaction(SIGILL, &act, &s_handlerILL) == 0;
        ok &= sigaction(SIGBUS, &act, &s_handlerBUS) == 0;
        ok &= sigaction(SIGSEGV, &act, &s_handlerSEGV) == 0;
        if ( !ok )
        {
            wxLogDebug(_T("Failed to install our signal handler."));
        }

        s_savedHandlers = TRUE;
    }
    else if ( s_savedHandlers )
    {
        // uninstall the signal handler
        ok &= sigaction(SIGFPE, &s_handlerFPE, NULL) == 0;
        ok &= sigaction(SIGILL, &s_handlerILL, NULL) == 0;
        ok &= sigaction(SIGBUS, &s_handlerBUS, NULL) == 0;
        ok &= sigaction(SIGSEGV, &s_handlerSEGV, NULL) == 0;
        if ( !ok )
        {
            wxLogDebug(_T("Failed to uninstall our signal handler."));
        }

        s_savedHandlers = FALSE;
    }
    //else: nothing to do

    return ok;
}

#endif // wxUSE_ON_FATAL_EXCEPTION

// ----------------------------------------------------------------------------
// error and debug output routines (deprecated, use wxLog)
// ----------------------------------------------------------------------------

#if WXWIN_COMPATIBILITY_2_2

void wxDebugMsg( const char *format, ... )
{
  va_list ap;
  va_start( ap, format );
  vfprintf( stderr, format, ap );
  fflush( stderr );
  va_end(ap);
}

void wxError( const wxString &msg, const wxString &title )
{
  wxFprintf( stderr, _("Error ") );
  if (!title.IsNull()) wxFprintf( stderr, wxT("%s "), WXSTRINGCAST(title) );
  if (!msg.IsNull()) wxFprintf( stderr, wxT(": %s"), WXSTRINGCAST(msg) );
  wxFprintf( stderr, wxT(".\n") );
}

void wxFatalError( const wxString &msg, const wxString &title )
{
  wxFprintf( stderr, _("Error ") );
  if (!title.IsNull()) wxFprintf( stderr, wxT("%s "), WXSTRINGCAST(title) );
  if (!msg.IsNull()) wxFprintf( stderr, wxT(": %s"), WXSTRINGCAST(msg) );
  wxFprintf( stderr, wxT(".\n") );
  exit(3); // the same exit code as for abort()
}

#endif // WXWIN_COMPATIBILITY_2_2

