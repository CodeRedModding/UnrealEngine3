/////////////////////////////////////////////////////////////////////////////
// Name:        filefn.cpp
// Purpose:     File- and directory-related functions
// Author:      Julian Smart
// Modified by:
// Created:     29/01/98
// RCS-ID:      $Id: filefn.cpp,v 1.159.2.6 2002/11/21 21:49:20 RR Exp $
// Copyright:   (c) 1998 Julian Smart
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "filefn.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"
#include "wx/defs.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/utils.h"
#include "wx/intl.h"
#include "wx/file.h"
#include "wx/filename.h"
#include "wx/dir.h"

// there are just too many of those...
#ifdef __VISUALC__
    #pragma warning(disable:4706)   // assignment within conditional expression
#endif // VC++

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__WATCOMC__)
    #if !(defined(_MSC_VER) && (_MSC_VER > 800))
        #include <errno.h>
    #endif
#endif

#if defined(__WXMAC__)
  #include  "wx/mac/private.h"  // includes mac headers
#endif

#include <time.h>

#ifndef __MWERKS__
    #include <sys/types.h>
    #include <sys/stat.h>
#else
    #include <stat.h>
    #include <unistd.h>
    #include <unix.h>
#endif

#ifdef __UNIX__
    #include <unistd.h>
    #include <dirent.h>
    #include <fcntl.h>
#endif

#ifdef __WXPM__
    #include <process.h>
    #include "wx/os2/private.h"
#endif
#if defined(__WINDOWS__) && !defined(__WXMICROWIN__) && !defined(__WXWINE__)
#if !defined( __GNUWIN32__ ) && !defined( __MWERKS__ ) && !defined(__SALFORDC__)
    #include <direct.h>
    #include <dos.h>
    #include <io.h>
#endif // __WINDOWS__
#endif // native Win compiler

#if defined(__DOS__)
    #ifdef __WATCOMC__
        #include <direct.h>
        #include <dos.h>
        #include <io.h>
    #endif
    #ifdef __DJGPP__
        #include <unistd.h>
    #endif
#endif

#ifdef __BORLANDC__ // Please someone tell me which version of Borland needs
                    // this (3.1 I believe) and how to test for it.
                    // If this works for Borland 4.0 as well, then no worries.
    #include <dir.h>
#endif

#ifdef __SALFORDC__
    #include <dir.h>
    #include <unix.h>
#endif

#include "wx/log.h"

// No, Cygwin doesn't appear to have fnmatch.h after all.
#if defined(HAVE_FNMATCH_H)
    #include "fnmatch.h"
#endif

#ifdef __WINDOWS__
    #include <windows.h>
    #include "wx/msw/mslu.h"

    // sys/cygwin.h is needed for cygwin_conv_to_full_win32_path()
    //
    // note that it must be included after <windows.h>
    #ifdef __GNUWIN32__
        #ifdef __CYGWIN__
            #include <sys/cygwin.h>
        #endif

        #ifndef __TWIN32__
            #include <sys/unistd.h>
        #endif
    #endif // __GNUWIN32__
#endif // __WINDOWS__

// TODO: Borland probably has _wgetcwd as well?
#ifdef _MSC_VER
    #define HAVE_WGETCWD
#endif

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

#ifndef _MAXPATHLEN
    #define _MAXPATHLEN 1024
#endif

#ifdef __WXMAC__
#    include "MoreFiles.h"
#    include "MoreFilesExtras.h"
#    include "FullPath.h"
#    include "FSpCompat.h"
#endif

// ----------------------------------------------------------------------------
// private globals
// ----------------------------------------------------------------------------

// MT-FIXME: get rid of this horror and all code using it
static wxChar wxFileFunctionsBuffer[4*_MAXPATHLEN];

#if defined(__VISAGECPP__) && __IBMCPP__ >= 400
//
// VisualAge C++ V4.0 cannot have any external linkage const decs
// in headers included by more than one primary source
//
const off_t wxInvalidOffset = (off_t)-1;
#endif

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

// we need to translate Mac filenames before passing them to OS functions
#define OS_FILENAME(s) (s.fn_str())

// ============================================================================
// implementation
// ============================================================================

#ifdef wxNEED_WX_UNISTD_H

WXDLLEXPORT int wxStat( const wxChar *file_name, wxStructStat *buf )
{
    return stat( wxConvFile.cWX2MB( file_name ), buf );
}

WXDLLEXPORT int wxAccess( const wxChar *pathname, int mode )
{
    return access( wxConvFile.cWX2MB( pathname ), mode );
}

WXDLLEXPORT int wxOpen( const wxChar *pathname, int flags, mode_t mode )
{
    return open( wxConvFile.cWX2MB( pathname ), flags, mode );
}

#endif
   // wxNEED_WX_UNISTD_H

// ----------------------------------------------------------------------------
// wxPathList
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxPathList, wxStringList)

void wxPathList::Add (const wxString& path)
{
    wxStringList::Add (WXSTRINGCAST path);
}

// Add paths e.g. from the PATH environment variable
void wxPathList::AddEnvList (const wxString& envVariable)
{
  static const wxChar PATH_TOKS[] =
#ifdef __WINDOWS__
        wxT(" ;"); // Don't seperate with colon in DOS (used for drive)
#else
        wxT(" :;");
#endif

  wxChar *val = wxGetenv (WXSTRINGCAST envVariable);
  if (val && *val)
    {
      wxChar *s = copystring (val);
      wxChar *save_ptr, *token = wxStrtok (s, PATH_TOKS, &save_ptr);

      if (token)
      {
          Add (copystring (token));
          while (token)
          {
              if ((token = wxStrtok ((wxChar *) NULL, PATH_TOKS, &save_ptr)) != NULL)
                  Add (wxString(token));
          }
      }

      // suppress warning about unused variable save_ptr when wxStrtok() is a
      // macro which throws away its third argument
      save_ptr = token;

      delete [] s;
    }
}

// Given a full filename (with path), ensure that that file can
// be accessed again USING FILENAME ONLY by adding the path
// to the list if not already there.
void wxPathList::EnsureFileAccessible (const wxString& path)
{
    wxString path_only(wxPathOnly(path));
    if ( !path_only.IsEmpty() )
    {
        if ( !Member(path_only) )
            Add(path_only);
    }
}

bool wxPathList::Member (const wxString& path)
{
  for (wxNode * node = First (); node != NULL; node = node->Next ())
  {
      wxString path2((wxChar *) node->Data ());
      if (
#if defined(__WINDOWS__) || defined(__VMS__) || defined (__WXMAC__)
      // Case INDEPENDENT
          path.CompareTo (path2, wxString::ignoreCase) == 0
#else
      // Case sensitive File System
          path.CompareTo (path2) == 0
#endif
        )
        return TRUE;
  }
  return FALSE;
}

wxString wxPathList::FindValidPath (const wxString& file)
{
  if (wxFileExists (wxExpandPath(wxFileFunctionsBuffer, file)))
    return wxString(wxFileFunctionsBuffer);

  wxChar buf[_MAXPATHLEN];
  wxStrcpy(buf, wxFileFunctionsBuffer);

  wxChar *filename = (wxChar*) NULL; /* shut up buggy egcs warning */
  filename = wxIsAbsolutePath (buf) ? wxFileNameFromPath (buf) : (wxChar *)buf;

  for (wxNode * node = First (); node; node = node->Next ())
    {
      wxChar *path = (wxChar *) node->Data ();
      wxStrcpy (wxFileFunctionsBuffer, path);
      wxChar ch = wxFileFunctionsBuffer[wxStrlen(wxFileFunctionsBuffer)-1];
      if (ch != wxT('\\') && ch != wxT('/'))
        wxStrcat (wxFileFunctionsBuffer, wxT("/"));
      wxStrcat (wxFileFunctionsBuffer, filename);
#ifdef __WINDOWS__
      wxUnix2DosFilename (wxFileFunctionsBuffer);
#endif
      if (wxFileExists (wxFileFunctionsBuffer))
      {
        return wxString(wxFileFunctionsBuffer);        // Found!
      }
    }                                // for()

  return wxString(wxT(""));                    // Not found
}

wxString wxPathList::FindAbsoluteValidPath (const wxString& file)
{
    wxString f = FindValidPath(file);
    if ( wxIsAbsolutePath(f) )
        return f;

    wxString buf;
    wxGetWorkingDirectory(wxStringBuffer(buf, _MAXPATHLEN), _MAXPATHLEN);

    if ( !wxEndsWithPathSeparator(buf) )
    {
        buf += wxFILE_SEP_PATH;
    }
    buf += f;

    return buf;
}

bool
wxFileExists (const wxString& filename)
{
    // we must use GetFileAttributes() instead of the ANSI C functions because
    // it can cope with network (UNC) paths unlike them
#if defined(__WIN32__) && !defined(__WXMICROWIN__)
    DWORD ret = ::GetFileAttributes(filename);

    return (ret != (DWORD)-1) && !(ret & FILE_ATTRIBUTE_DIRECTORY);
#else // !__WIN32__
    wxStructStat st;
    return wxStat(filename, &st) == 0 && (st.st_mode & S_IFREG);
#endif // __WIN32__/!__WIN32__
}

bool
wxIsAbsolutePath (const wxString& filename)
{
    if (filename != wxT(""))
    {
#if defined(__WXMAC__) && !defined(__DARWIN__)
        // Classic or Carbon CodeWarrior like
        // Carbon with Apple DevTools is Unix like

        // This seems wrong to me, but there is no fix. since
        // "MacOS:MyText.txt" is absolute whereas "MyDir:MyText.txt"
        // is not. Or maybe ":MyDir:MyText.txt" has to be used? RR.
        if (filename.Find(':') != wxNOT_FOUND && filename[0] != ':')
            return TRUE ;
#else
        // Unix like or Windows
        if (filename[0] == wxT('/'))
            return TRUE;
#endif
#ifdef __VMS__
        if ((filename[0] == wxT('[') && filename[1] != wxT('.')))
            return TRUE;
#endif
#ifdef __WINDOWS__
        // MSDOS like
        if (filename[0] == wxT('\\') || (wxIsalpha (filename[0]) && filename[1] == wxT(':')))
            return TRUE;
#endif
    }
    return FALSE ;
}

/*
 * Strip off any extension (dot something) from end of file,
 * IF one exists. Inserts zero into buffer.
 *
 */

void wxStripExtension(wxChar *buffer)
{
  int len = wxStrlen(buffer);
  int i = len-1;
  while (i > 0)
  {
    if (buffer[i] == wxT('.'))
    {
      buffer[i] = 0;
      break;
    }
    i --;
  }
}

void wxStripExtension(wxString& buffer)
{
  size_t len = buffer.Length();
  size_t i = len-1;
  while (i > 0)
  {
    if (buffer.GetChar(i) == wxT('.'))
    {
      buffer = buffer.Left(i);
      break;
    }
    i --;
  }
}

// Destructive removal of /./ and /../ stuff
wxChar *wxRealPath (wxChar *path)
{
#ifdef __WXMSW__
  static const wxChar SEP = wxT('\\');
  wxUnix2DosFilename(path);
#else
  static const wxChar SEP = wxT('/');
#endif
  if (path[0] && path[1]) {
    /* MATTHEW: special case "/./x" */
    wxChar *p;
    if (path[2] == SEP && path[1] == wxT('.'))
      p = &path[0];
    else
      p = &path[2];
    for (; *p; p++)
      {
        if (*p == SEP)
          {
            if (p[1] == wxT('.') && p[2] == wxT('.') && (p[3] == SEP || p[3] == wxT('\0')))
              {
                wxChar *q;
                for (q = p - 1; q >= path && *q != SEP; q--);
                if (q[0] == SEP && (q[1] != wxT('.') || q[2] != wxT('.') || q[3] != SEP)
                    && (q - 1 <= path || q[-1] != SEP))
                  {
                    wxStrcpy (q, p + 3);
                    if (path[0] == wxT('\0'))
                      {
                        path[0] = SEP;
                        path[1] = wxT('\0');
                      }
#ifdef __WXMSW__
                    /* Check that path[2] is NULL! */
                    else if (path[1] == wxT(':') && !path[2])
                      {
                        path[2] = SEP;
                        path[3] = wxT('\0');
                      }
#endif
                    p = q - 1;
                  }
              }
            else if (p[1] == wxT('.') && (p[2] == SEP || p[2] == wxT('\0')))
              wxStrcpy (p, p + 2);
          }
      }
  }
  return path;
}

// Must be destroyed
wxChar *wxCopyAbsolutePath(const wxString& filename)
{
  if (filename == wxT(""))
    return (wxChar *) NULL;

  if (! wxIsAbsolutePath(wxExpandPath(wxFileFunctionsBuffer, filename))) {
    wxChar  buf[_MAXPATHLEN];
    buf[0] = wxT('\0');
    wxGetWorkingDirectory(buf, WXSIZEOF(buf));
    wxChar ch = buf[wxStrlen(buf) - 1];
#ifdef __WXMSW__
    if (ch != wxT('\\') && ch != wxT('/'))
        wxStrcat(buf, wxT("\\"));
#else
    if (ch != wxT('/'))
        wxStrcat(buf, wxT("/"));
#endif
    wxStrcat(buf, wxFileFunctionsBuffer);
    return copystring( wxRealPath(buf) );
  }
  return copystring( wxFileFunctionsBuffer );
}

/*-
 Handles:
   ~/ => home dir
   ~user/ => user's home dir
   If the environment variable a = "foo" and b = "bar" then:
   Unix:
        $a        =>        foo
        $a$b        =>        foobar
        $a.c        =>        foo.c
        xxx$a        =>        xxxfoo
        ${a}!        =>        foo!
        $(b)!        =>        bar!
        \$a        =>        \$a
   MSDOS:
        $a        ==>        $a
        $(a)        ==>        foo
        $(a)$b        ==>        foo$b
        $(a)$(b)==>        foobar
        test.$$        ==>        test.$$
 */

/* input name in name, pathname output to buf. */

wxChar *wxExpandPath(wxChar *buf, const wxChar *name)
{
    register wxChar *d, *s, *nm;
    wxChar          lnm[_MAXPATHLEN];
    int             q;

    // Some compilers don't like this line.
//    const wxChar    trimchars[] = wxT("\n \t");

    wxChar      trimchars[4];
    trimchars[0] = wxT('\n');
    trimchars[1] = wxT(' ');
    trimchars[2] = wxT('\t');
    trimchars[3] = 0;

#ifdef __WXMSW__
     const wxChar     SEP = wxT('\\');
#else
     const wxChar     SEP = wxT('/');
#endif
    buf[0] = wxT('\0');
    if (name == NULL || *name == wxT('\0'))
        return buf;
    nm = copystring(name); // Make a scratch copy
    wxChar *nm_tmp = nm;

    /* Skip leading whitespace and cr */
    while (wxStrchr((wxChar *)trimchars, *nm) != NULL)
        nm++;
    /* And strip off trailing whitespace and cr */
    s = nm + (q = wxStrlen(nm)) - 1;
    while (q-- && wxStrchr((wxChar *)trimchars, *s) != NULL)
        *s = wxT('\0');

    s = nm;
    d = lnm;
#ifdef __WXMSW__
    q = FALSE;
#else
    q = nm[0] == wxT('\\') && nm[1] == wxT('~');
#endif

    /* Expand inline environment variables */
#ifdef __VISAGECPP__
    while (*d)
    {
      *d++ = *s;
      if(*s == wxT('\\'))
      {
        *(d - 1) = *++s;
        if (*d)
        {
          s++;
          continue;
        }
        else
           break;
      }
      else
#else
    while ((*d++ = *s) != 0) {
#  ifndef __WXMSW__
        if (*s == wxT('\\')) {
            if ((*(d - 1) = *++s)) {
                s++;
                continue;
            } else
                break;
        } else
#  endif
#endif
#ifdef __WXMSW__
        if (*s++ == wxT('$') && (*s == wxT('{') || *s == wxT(')')))
#else
        if (*s++ == wxT('$'))
#endif
        {
            register wxChar  *start = d;
            register int     braces = (*s == wxT('{') || *s == wxT('('));
            register wxChar  *value;
            while ((*d++ = *s) != 0)
                if (braces ? (*s == wxT('}') || *s == wxT(')')) : !(wxIsalnum(*s) || *s == wxT('_')) )
                    break;
                else
                    s++;
            *--d = 0;
            value = wxGetenv(braces ? start + 1 : start);
            if (value) {
                for ((d = start - 1); (*d++ = *value++) != 0;);
                d--;
                if (braces && *s)
                    s++;
            }
        }
    }

    /* Expand ~ and ~user */
    nm = lnm;
    if (nm[0] == wxT('~') && !q)
    {
        /* prefix ~ */
        if (nm[1] == SEP || nm[1] == 0)
        {        /* ~/filename */
        // FIXME: wxGetUserHome could return temporary storage in Unicode mode
            if ((s = WXSTRINGCAST wxGetUserHome(wxT(""))) != NULL) {
                if (*++nm)
                    nm++;
            }
        } else
        {                /* ~user/filename */
            register wxChar  *nnm;
            register wxChar  *home;
            for (s = nm; *s && *s != SEP; s++);
            int was_sep; /* MATTHEW: Was there a separator, or NULL? */
            was_sep = (*s == SEP);
            nnm = *s ? s + 1 : s;
            *s = 0;
        // FIXME: wxGetUserHome could return temporary storage in Unicode mode
            if ((home = WXSTRINGCAST wxGetUserHome(wxString(nm + 1))) == NULL) {
               if (was_sep) /* replace only if it was there: */
                   *s = SEP;
                s = NULL;
            } else {
                nm = nnm;
                s = home;
            }
        }
    }

    d = buf;
    if (s && *s) { /* MATTHEW: s could be NULL if user '~' didn't exist */
        /* Copy home dir */
        while (wxT('\0') != (*d++ = *s++))
          /* loop */;
        // Handle root home
        if (d - 1 > buf && *(d - 2) != SEP)
          *(d - 1) = SEP;
    }
    s = nm;
    while ((*d++ = *s++) != 0);
    delete[] nm_tmp; // clean up alloc
    /* Now clean up the buffer */
    return wxRealPath(buf);
}

/* Contract Paths to be build upon an environment variable
   component:

   example: "/usr/openwin/lib", OPENWINHOME --> ${OPENWINHOME}/lib

   The call wxExpandPath can convert these back!
 */
wxChar *
wxContractPath (const wxString& filename, const wxString& envname, const wxString& user)
{
  static wxChar dest[_MAXPATHLEN];

  if (filename == wxT(""))
    return (wxChar *) NULL;

  wxStrcpy (dest, WXSTRINGCAST filename);
#ifdef __WXMSW__
  wxUnix2DosFilename(dest);
#endif

  // Handle environment
  const wxChar *val = (const wxChar *) NULL;
  wxChar *tcp = (wxChar *) NULL;
  if (envname != WXSTRINGCAST NULL && (val = wxGetenv (WXSTRINGCAST envname)) != NULL &&
     (tcp = wxStrstr (dest, val)) != NULL)
    {
        wxStrcpy (wxFileFunctionsBuffer, tcp + wxStrlen (val));
        *tcp++ = wxT('$');
        *tcp++ = wxT('{');
        wxStrcpy (tcp, WXSTRINGCAST envname);
        wxStrcat (tcp, wxT("}"));
        wxStrcat (tcp, wxFileFunctionsBuffer);
    }

  // Handle User's home (ignore root homes!)
  size_t len = 0;
  if ((val = wxGetUserHome (user)) != NULL &&
      (len = wxStrlen(val)) > 2 &&
      wxStrncmp(dest, val, len) == 0)
    {
      wxStrcpy(wxFileFunctionsBuffer, wxT("~"));
      if (user != wxT(""))
             wxStrcat(wxFileFunctionsBuffer, (const wxChar*) user);
      wxStrcat(wxFileFunctionsBuffer, dest + len);
      wxStrcpy (dest, wxFileFunctionsBuffer);
    }

  return dest;
}

// Return just the filename, not the path (basename)
wxChar *wxFileNameFromPath (wxChar *path)
{
    wxString p = path;
    wxString n = wxFileNameFromPath(p);

    return path + p.length() - n.length();
}

wxString wxFileNameFromPath (const wxString& path)
{
    wxString name, ext;
    wxFileName::SplitPath(path, NULL, &name, &ext);

    wxString fullname = name;
    if ( !ext.empty() )
    {
        fullname << wxFILE_SEP_EXT << ext;
    }

    return fullname;
}

// Return just the directory, or NULL if no directory
wxChar *
wxPathOnly (wxChar *path)
{
    if (path && *path)
    {
        static wxChar buf[_MAXPATHLEN];

        // Local copy
        wxStrcpy (buf, path);

        int l = wxStrlen(path);
        int i = l - 1;

        // Search backward for a backward or forward slash
        while (i > -1)
        {
#if defined(__WXMAC__) && !defined(__DARWIN__)
            // Classic or Carbon CodeWarrior like
            // Carbon with Apple DevTools is Unix like
            if (path[i] == wxT(':') )
            {
                buf[i] = 0;
                return buf;
            }
#else
            // Unix like or Windows
            if (path[i] == wxT('/') || path[i] == wxT('\\'))
            {
                buf[i] = 0;
                return buf;
            }
#endif
#ifdef __VMS__
            if (path[i] == wxT(']'))
            {
                buf[i+1] = 0;
                return buf;
            }
#endif
            i --;
        }

#if defined(__WXMSW__) || defined(__WXPM__)
        // Try Drive specifier
        if (wxIsalpha (buf[0]) && buf[1] == wxT(':'))
        {
            // A:junk --> A:. (since A:.\junk Not A:\junk)
            buf[2] = wxT('.');
            buf[3] = wxT('\0');
            return buf;
        }
#endif
    }
    return (wxChar *) NULL;
}

// Return just the directory, or NULL if no directory
wxString wxPathOnly (const wxString& path)
{
    if (path != wxT(""))
    {
        wxChar buf[_MAXPATHLEN];

        // Local copy
        wxStrcpy (buf, WXSTRINGCAST path);

        int l = path.Length();
        int i = l - 1;

        // Search backward for a backward or forward slash
        while (i > -1)
        {
#if defined(__WXMAC__) && !defined(__DARWIN__)
            // Classic or Carbon CodeWarrior like
            // Carbon with Apple DevTools is Unix like
            if (path[i] == wxT(':') )
            {
                buf[i] = 0;
                return wxString(buf);
            }
#else
            // Unix like or Windows
            if (path[i] == wxT('/') || path[i] == wxT('\\'))
            {
                buf[i] = 0;
                return wxString(buf);
            }
#endif
#ifdef __VMS__
            if (path[i] == wxT(']'))
            {
                buf[i+1] = 0;
                return wxString(buf);
            }
#endif
            i --;
        }

#if defined(__WXMSW__) || defined(__WXPM__)
        // Try Drive specifier
        if (wxIsalpha (buf[0]) && buf[1] == wxT(':'))
        {
            // A:junk --> A:. (since A:.\junk Not A:\junk)
            buf[2] = wxT('.');
            buf[3] = wxT('\0');
            return wxString(buf);
        }
#endif
    }
    return wxString(wxT(""));
}

// Utility for converting delimiters in DOS filenames to UNIX style
// and back again - or we get nasty problems with delimiters.
// Also, convert to lower case, since case is significant in UNIX.

#if defined(__WXMAC__)
wxString wxMacFSSpec2MacFilename( const FSSpec *spec )
{
#ifdef __DARWIN__
    int         i;
    int         j;
    OSErr       theErr;
    OSStatus    theStatus;
    Boolean 	isDirectory = false;
    Str255	theParentPath = "\p";
    FSSpec      theParentSpec;
    FSRef       theParentRef;
    char        theFileName[FILENAME_MAX];
    char        thePath[FILENAME_MAX];

    strcpy(thePath, "");

    // GD: Separate file name from path and make a FSRef to the parent
    //     directory. This is necessary since FSRefs cannot reference files
    //     that have not yet been created.
    //     Based on example code from Apple Technical Note TN2022
    //       http://developer.apple.com/technotes/tn/tn2022.html

    // check whether we are converting a directory
    isDirectory = ((spec->name)[spec->name[0]] == ':');
    // count length of file name
    for (i = spec->name[0] - (isDirectory ? 1 : 0); ((spec->name[i] != ':') && (i > 0)); i--);
    // copy file name
    //   prepend path separator since it will later be appended to the path
    theFileName[0] = wxFILE_SEP_PATH;
    for (j = i + 1; j <= spec->name[0] - (isDirectory ? 1 : 0); j++) {
        theFileName[j - i] = spec->name[j];
    }
    theFileName[j - i] = '\0';
    // copy path if any
    for (j = 1; j <= i; j++) {
        theParentPath[++theParentPath[0]] = spec->name[j];
    }
    theErr = FSMakeFSSpec(spec->vRefNum, spec->parID, theParentPath, &theParentSpec);
    if (theErr == noErr) {
        // convert the FSSpec to an FSRef
        theErr = FSpMakeFSRef(&theParentSpec, &theParentRef);
    }
    if (theErr == noErr) {
        // get the POSIX path associated with the FSRef
        theStatus = FSRefMakePath(&theParentRef,
                                  (UInt8 *)thePath, sizeof(thePath));
    }
    if (theStatus == noErr) {
        // append file name to path
        //   includes previously prepended path separator
        strcat(thePath, theFileName);
    }

    // create path string for return value
    wxString result( thePath ) ;
#else
    Handle    myPath ;
    short     length ;

    // get length of path and allocate handle
    FSpGetFullPath( spec , &length , &myPath ) ;
    ::SetHandleSize( myPath , length + 1 ) ;
    ::HLock( myPath ) ;
    (*myPath)[length] = 0 ;
    if ((length > 0) && ((*myPath)[length-1] == ':'))
        (*myPath)[length-1] = 0 ;

    // create path string for return value
    wxString result( (char*) *myPath ) ;

    // free allocated handle
    ::HUnlock( myPath ) ;
    ::DisposeHandle( myPath ) ;
#endif

    return result ;
}
#ifndef __DARWIN__
// Mac file names are POSIX (Unix style) under Darwin
// therefore the conversion functions below are not needed

static char sMacFileNameConversion[ 1000 ] ;

#endif
void wxMacFilename2FSSpec( const char *path , FSSpec *spec )
{
	OSStatus err = noErr ;
#ifdef __DARWIN__
    FSRef theRef;

    // get the FSRef associated with the POSIX path
    err = FSPathMakeRef((const UInt8 *) path, &theRef, NULL);
    // convert the FSRef to an FSSpec
    err = FSGetCatalogInfo(&theRef, kFSCatInfoNone, NULL, NULL, spec, NULL);
#else
	if ( strchr( path , ':' ) == NULL )
    {
    	// try whether it is a volume / or a mounted volume
        strncpy( sMacFileNameConversion , path , 1000 ) ;
        sMacFileNameConversion[998] = 0 ;
        strcat( sMacFileNameConversion , ":" ) ;
        err = FSpLocationFromFullPath( strlen(sMacFileNameConversion) , sMacFileNameConversion , spec ) ;
    }
    else
    {
    	err = FSpLocationFromFullPath( strlen(path) , path , spec ) ;
    }
#endif
}

#ifndef __DARWIN__

wxString wxMac2UnixFilename (const char *str)
{
    char *s = sMacFileNameConversion ;
    strcpy( s , str ) ;
    if (s)
    {
        memmove( s+1 , s ,strlen( s ) + 1) ;
        if ( *s == ':' )
            *s = '.' ;
        else
            *s = '/' ;

        while (*s)
        {
            if (*s == ':')
                *s = '/';
            else
                *s = wxTolower(*s);        // Case INDEPENDENT
            s++;
        }
    }
    return wxString(sMacFileNameConversion) ;
}

wxString wxUnix2MacFilename (const char *str)
{
    char *s = sMacFileNameConversion ;
    strcpy( s , str ) ;
    if (s)
    {
        if ( *s == '.' )
        {
            // relative path , since it goes on with slash which is translated to a :
            memmove( s , s+1 ,strlen( s ) ) ;
        }
        else if ( *s == '/' )
        {
            // absolute path -> on mac just start with the drive name
            memmove( s , s+1 ,strlen( s ) ) ;
        }
        else
        {
            wxASSERT_MSG( 1 , "unkown path beginning" ) ;
        }
        while (*s)
        {
            if (*s == '/' || *s == '\\')
            {
                // convert any back-directory situations
                if ( *(s+1) == '.' && *(s+2) == '.' && ( (*(s+3) == '/' || *(s+3) == '\\') ) )
                {
                    *s = ':';
                    memmove( s+1 , s+3 ,strlen( s+3 ) + 1 ) ;
                }
                else
                    *s = ':';
            }
            s++ ;
        }
    }
    return wxString (sMacFileNameConversion) ;
}

wxString wxMacFSSpec2UnixFilename( const FSSpec *spec )
{
    return wxMac2UnixFilename( wxMacFSSpec2MacFilename( spec) ) ;
}

void wxUnixFilename2FSSpec( const char *path , FSSpec *spec )
{
    wxString var = wxUnix2MacFilename( path ) ;
    wxMacFilename2FSSpec( var , spec ) ;
}
#endif // ! __DARWIN__

#endif // __WXMAC__

void
wxDos2UnixFilename (char *s)
{
  if (s)
    while (*s)
      {
        if (*s == '\\')
          *s = '/';
#ifdef __WXMSW__
        else
          *s = wxTolower (*s);        // Case INDEPENDENT
#endif
        s++;
      }
}

void
#if defined(__WXMSW__) || defined(__WXPM__)
wxUnix2DosFilename (wxChar *s)
#else
wxUnix2DosFilename (wxChar *WXUNUSED(s) )
#endif
{
// Yes, I really mean this to happen under DOS only! JACS
#if defined(__WXMSW__) || defined(__WXPM__)
  if (s)
    while (*s)
      {
        if (*s == wxT('/'))
          *s = wxT('\\');
        s++;
      }
#endif
}

// Concatenate two files to form third
bool
wxConcatFiles (const wxString& file1, const wxString& file2, const wxString& file3)
{
  wxString outfile;
  if ( !wxGetTempFileName( wxT("cat"), outfile) )
      return FALSE;

  FILE *fp1 = (FILE *) NULL;
  FILE *fp2 = (FILE *) NULL;
  FILE *fp3 = (FILE *) NULL;
  // Open the inputs and outputs
  if ((fp1 = wxFopen ( file1, wxT("rb"))) == NULL ||
      (fp2 = wxFopen ( file2, wxT("rb"))) == NULL ||
      (fp3 = wxFopen ( outfile, wxT("wb"))) == NULL)
    {
      if (fp1)
        fclose (fp1);
      if (fp2)
        fclose (fp2);
      if (fp3)
        fclose (fp3);
      return FALSE;
    }

  int ch;
  while ((ch = getc (fp1)) != EOF)
    (void) putc (ch, fp3);
  fclose (fp1);

  while ((ch = getc (fp2)) != EOF)
    (void) putc (ch, fp3);
  fclose (fp2);

  fclose (fp3);
  bool result = wxRenameFile(outfile, file3);
  return result;
}

// Copy files
bool
wxCopyFile (const wxString& file1, const wxString& file2, bool overwrite)
{
#if defined(__WIN32__) && !defined(__WXMICROWIN__)
    // CopyFile() copies file attributes and modification time too, so use it
    // instead of our code if available
    //
    // NB: 3rd parameter is bFailIfExists i.e. the inverse of overwrite
    if ( !::CopyFile(file1, file2, !overwrite) )
    {
        wxLogSysError(_("Failed to copy the file '%s' to '%s'"),
                      file1.c_str(), file2.c_str());

        return FALSE;
    }
#elif defined(__WXPM__)
    if ( ::DosCopy(file2, file2, overwrite ? DCPY_EXISTING : 0) != 0 )
        return FALSE;
#else // !Win32

    wxStructStat fbuf;
    // get permissions of file1
    if ( wxStat( file1.c_str(), &fbuf) != 0 )
    {
        // the file probably doesn't exist or we haven't the rights to read
        // from it anyhow
        wxLogSysError(_("Impossible to get permissions for file '%s'"),
                      file1.c_str());
        return FALSE;
    }

    // open file1 for reading
    wxFile fileIn(file1, wxFile::read);
    if ( !fileIn.IsOpened() )
        return FALSE;

    // remove file2, if it exists. This is needed for creating
    // file2 with the correct permissions in the next step
    if ( wxFileExists(file2)  && (!overwrite || !wxRemoveFile(file2)))
    {
        wxLogSysError(_("Impossible to overwrite the file '%s'"),
                      file2.c_str());
        return FALSE;
    }

#ifdef __UNIX__
    // reset the umask as we want to create the file with exactly the same
    // permissions as the original one
    mode_t oldUmask = umask( 0 );
#endif // __UNIX__

    // create file2 with the same permissions than file1 and open it for
    // writing
    
    wxFile fileOut;
    if ( !fileOut.Create(file2, overwrite, fbuf.st_mode & 0777) )
        return FALSE;

#ifdef __UNIX__
    /// restore the old umask
    umask(oldUmask);
#endif // __UNIX__

    // copy contents of file1 to file2
    char buf[4096];
    size_t count;
    for ( ;; )
    {
        count = fileIn.Read(buf, WXSIZEOF(buf));
        if ( fileIn.Error() )
            return FALSE;

        // end of file?
        if ( !count )
            break;

        if ( fileOut.Write(buf, count) < count )
            return FALSE;
    }

    // we can expect fileIn to be closed successfully, but we should ensure
    // that fileOut was closed as some write errors (disk full) might not be
    // detected before doing this
    if ( !fileIn.Close() || !fileOut.Close() )
        return FALSE;

#if !defined(__VISAGECPP__) && !defined(__WXMAC__) || defined(__UNIX__)
    // no chmod in VA.  Should be some permission API for HPFS386 partitions
    // however
    if ( chmod(OS_FILENAME(file2), fbuf.st_mode) != 0 )
    {
        wxLogSysError(_("Impossible to set permissions for the file '%s'"),
                      file2.c_str());
        return FALSE;
    }
#endif // OS/2 || Mac
#endif // __WXMSW__ && __WIN32__

    return TRUE;
}

bool
wxRenameFile (const wxString& file1, const wxString& file2)
{
  // Normal system call
  if ( wxRename (file1, file2) == 0 )
    return TRUE;

  // Try to copy
  if (wxCopyFile(file1, file2)) {
    wxRemoveFile(file1);
    return TRUE;
  }
  // Give up
  return FALSE;
}

bool wxRemoveFile(const wxString& file)
{
#if defined(__VISUALC__) \
 || defined(__BORLANDC__) \
 || defined(__WATCOMC__) \
 || defined(__GNUWIN32__)
  int res = wxRemove(file);
#else
  int res = unlink(OS_FILENAME(file));
#endif

  return res == 0;
}

bool wxMkdir(const wxString& dir, int perm)
{
#if defined(__WXMAC__) && !defined(__UNIX__)
  return (mkdir( dir , 0 ) == 0);
#else // !Mac
    const wxChar *dirname = dir.c_str();

    // assume mkdir() has 2 args on non Windows-OS/2 platforms and on Windows too
    // for the GNU compiler
#if (!(defined(__WXMSW__) || defined(__WXPM__) || defined(__DOS__))) || (defined(__GNUWIN32__) && !defined(__MINGW32__)) || defined(__WXWINE__) || defined(__WXMICROWIN__)
    if ( mkdir(wxFNCONV(dirname), perm) != 0 )
#elif defined(__WXPM__)
    if (::DosCreateDir((PSZ)dirname, NULL) != 0) // enhance for EAB's??
#elif defined(__DOS__)
  #if defined(__WATCOMC__)
    (void)perm;
    if ( wxMkDir(wxFNSTRINGCAST wxFNCONV(dirname)) != 0 )
  #elif defined(__DJGPP__)
    if ( mkdir(wxFNCONV(dirname), perm) != 0 )
  #else
    #error "Unsupported DOS compiler!"
  #endif
#else  // !MSW, !DOS and !OS/2 VAC++
    (void)perm;
    if ( wxMkDir(wxFNSTRINGCAST wxFNCONV(dirname)) != 0 )
#endif // !MSW/MSW
    {
        wxLogSysError(_("Directory '%s' couldn't be created"), dirname);

        return FALSE;
    }

    return TRUE;
#endif // Mac/!Mac
}

bool wxRmdir(const wxString& dir, int WXUNUSED(flags))
{
#ifdef __VMS__
  return FALSE; //to be changed since rmdir exists in VMS7.x
#elif defined(__WXPM__)
  return (::DosDeleteDir((PSZ)dir.c_str()) == 0);
#else

#ifdef __SALFORDC__
  return FALSE; // What to do?
#else
  return (wxRmDir(OS_FILENAME(dir)) == 0);
#endif

#endif
}

// does the path exists? (may have or not '/' or '\\' at the end)
bool wxPathExists(const wxChar *pszPathName)
{
    wxString strPath(pszPathName);

#ifdef __WINDOWS__
    // Windows fails to find directory named "c:\dir\" even if "c:\dir" exists,
    // so remove all trailing backslashes from the path - but don't do this for
    // the pathes "d:\" (which are different from "d:") nor for just "\"
    while ( wxEndsWithPathSeparator(strPath) )
    {
        size_t len = strPath.length();
        if ( len == 1 || (len == 3 && strPath[len - 2] == _T(':')) )
            break;

        strPath.Truncate(len - 1);
    }
#endif // __WINDOWS__

#if defined(__WIN32__) && !defined(__WXMICROWIN__)
    // stat() can't cope with network paths
    DWORD ret = ::GetFileAttributes(strPath);

    return (ret != (DWORD)-1) && (ret & FILE_ATTRIBUTE_DIRECTORY);
#else // !__WIN32__

    wxStructStat st;
#ifndef __VISAGECPP__
    return wxStat(pszPathName, &st) == 0 && ((st.st_mode & S_IFMT) == S_IFDIR);
#else
    // S_IFMT not supported in VA compilers.. st_mode is a 2byte value only
    return wxStat(pszPathName, &st) == 0 && (st.st_mode == S_IFDIR);
#endif

#endif // __WIN32__/!__WIN32__
}

// Get a temporary filename, opening and closing the file.
wxChar *wxGetTempFileName(const wxString& prefix, wxChar *buf)
{
    wxString filename = wxFileName::CreateTempFileName(prefix);
    if ( filename.empty() )
        return NULL;

    if ( buf )
        wxStrcpy(buf, filename);
    else
        buf = copystring(filename);

    return buf;
}

bool wxGetTempFileName(const wxString& prefix, wxString& buf)
{
    buf = wxFileName::CreateTempFileName(prefix);

    return !buf.empty();
}

// Get first file name matching given wild card.

static wxDir *gs_dir = NULL;
static wxString gs_dirPath;

wxString wxFindFirstFile(const wxChar *spec, int flags)
{
    wxSplitPath(spec, &gs_dirPath, NULL, NULL);
    if ( gs_dirPath.IsEmpty() )
        gs_dirPath = wxT(".");
    if ( gs_dirPath.Last() != wxFILE_SEP_PATH )
        gs_dirPath << wxFILE_SEP_PATH;

    if (gs_dir)
        delete gs_dir;
    gs_dir = new wxDir(gs_dirPath);

    if ( !gs_dir->IsOpened() )
    {
        wxLogSysError(_("Can not enumerate files '%s'"), spec);
        return wxEmptyString;
    }

    int dirFlags = 0;
    switch (flags)
    {
        case wxDIR:  dirFlags = wxDIR_DIRS; break;
        case wxFILE: dirFlags = wxDIR_FILES; break;
        default:     dirFlags = wxDIR_DIRS | wxDIR_FILES; break;
    }

    wxString result;
    gs_dir->GetFirst(&result, wxFileNameFromPath(wxString(spec)), dirFlags);
    if ( result.IsEmpty() )
    {
        wxDELETE(gs_dir);
        return result;
    }

    return gs_dirPath + result;
}

wxString wxFindNextFile()
{
    wxASSERT_MSG( gs_dir, wxT("You must call wxFindFirstFile before!") );

    wxString result;
    gs_dir->GetNext(&result);

    if ( result.IsEmpty() )
    {
        wxDELETE(gs_dir);
        return result;
    }

    return gs_dirPath + result;
}


// Get current working directory.
// If buf is NULL, allocates space using new, else
// copies into buf.
wxChar *wxGetWorkingDirectory(wxChar *buf, int sz)
{
    if ( !buf )
    {
        buf = new wxChar[sz + 1];
    }

    bool ok = FALSE;

    // for the compilers which have Unicode version of _getcwd(), call it
    // directly, for the others call the ANSI version and do the translation
#if !wxUSE_UNICODE
    #define cbuf buf
#else // wxUSE_UNICODE
    bool needsANSI = TRUE;

    #if !defined(HAVE_WGETCWD) || wxUSE_UNICODE_MSLU
        // This is not legal code as the compiler 
        // is allowed destroy the wxCharBuffer.
        // wxCharBuffer c_buffer(sz);
        // char *cbuf = (char*)(const char*)c_buffer;
        char cbuf[_MAXPATHLEN];
    #endif

    #ifdef HAVE_WGETCWD
        #if wxUSE_UNICODE_MSLU
            if ( wxGetOsVersion() != wxWIN95 )
        #else
            char *cbuf = NULL; // never really used because needsANSI will always be FALSE
        #endif
            {
                ok = _wgetcwd(buf, sz) != NULL;
                needsANSI = FALSE;
            }
    #endif

    if ( needsANSI )
#endif // wxUSE_UNICODE
    {
    #ifdef _MSC_VER
        ok = _getcwd(cbuf, sz) != NULL;
    #elif defined(__WXMAC__) && !defined(__DARWIN__)
        FSSpec cwdSpec ;
        FCBPBRec pb;
        OSErr error;
        Str255  fileName ;
        pb.ioNamePtr = (StringPtr) &fileName;
        pb.ioVRefNum = 0;
        pb.ioRefNum = LMGetCurApRefNum();
        pb.ioFCBIndx = 0;
        error = PBGetFCBInfoSync(&pb);
        if ( error == noErr )
        {
            cwdSpec.vRefNum = pb.ioFCBVRefNum;
            cwdSpec.parID = pb.ioFCBParID;
            cwdSpec.name[0] = 0 ;
            wxString res = wxMacFSSpec2MacFilename( &cwdSpec ) ;

            strcpy( cbuf , res ) ;
            cbuf[res.length()]=0 ;

            ok = TRUE;
        }
        else
        {
            ok = FALSE;
        }
    #elif defined(__VISAGECPP__) || (defined (__OS2__) && defined (__WATCOMC__))
        APIRET rc;
        rc = ::DosQueryCurrentDir( 0 // current drive
                                  ,cbuf
                                  ,(PULONG)&sz
                                 );
        ok = rc != 0;
    #else // !Win32/VC++ !Mac !OS2
        ok = getcwd(cbuf, sz) != NULL;
    #endif // platform

    #if wxUSE_UNICODE
        // finally convert the result to Unicode if needed
        wxConvFile.MB2WC(buf, cbuf, sz);
    #endif // wxUSE_UNICODE
    }

    if ( !ok )
    {
        wxLogSysError(_("Failed to get the working directory"));

        // VZ: the old code used to return "." on error which didn't make any
        //     sense at all to me - empty string is a better error indicator
        //     (NULL might be even better but I'm afraid this could lead to
        //     problems with the old code assuming the return is never NULL)
        buf[0] = _T('\0');
    }
    else // ok, but we might need to massage the path into the right format
    {
#ifdef __DJGPP__
        // VS: DJGPP is a strange mix of DOS and UNIX API and returns paths
        //     with / deliminers. We don't like that.
        for (wxChar *ch = buf; *ch; ch++)
        {
            if (*ch == wxT('/'))
                *ch = wxT('\\');
        }
#endif // __DJGPP__

// MBN: we hope that in the case the user is compiling a GTK+/Motif app,
//      he needs Unix as opposed to Win32 pathnames
#if defined( __CYGWIN__ ) && defined( __WINDOWS__ )
        // another example of DOS/Unix mix (Cygwin)
        wxString pathUnix = buf;
        cygwin_conv_to_full_win32_path(pathUnix, buf);
#endif // __CYGWIN__
    }

    return buf;

#if !wxUSE_UNICODE
    #undef cbuf
#endif
}

wxString wxGetCwd()
{
    wxChar *buffer = new wxChar[_MAXPATHLEN];
    wxGetWorkingDirectory(buffer, _MAXPATHLEN);
    wxString str( buffer );
    delete [] buffer;
 
    return str;
}

bool wxSetWorkingDirectory(const wxString& d)
{
#if defined(__UNIX__) || defined(__WXMAC__) || defined(__DOS__)
  return (chdir(wxFNSTRINGCAST d.fn_str()) == 0);
#elif defined(__WXPM__)
  return (::DosSetCurrentDir((PSZ)d.c_str()) == 0);
#elif defined(__WINDOWS__)

#ifdef __WIN32__
  return (bool)(SetCurrentDirectory(d) != 0);
#else
  // Must change drive, too.
  bool isDriveSpec = ((strlen(d) > 1) && (d[1] == ':'));
  if (isDriveSpec)
  {
    wxChar firstChar = d[0];

    // To upper case
    if (firstChar > 90)
      firstChar = firstChar - 32;

    // To a drive number
    unsigned int driveNo = firstChar - 64;
    if (driveNo > 0)
    {
       unsigned int noDrives;
       _dos_setdrive(driveNo, &noDrives);
    }
  }
  bool success = (chdir(WXSTRINGCAST d) == 0);

  return success;
#endif

#endif
}

// Get the OS directory if appropriate (such as the Windows directory).
// On non-Windows platform, probably just return the empty string.
wxString wxGetOSDirectory()
{
#if defined(__WINDOWS__) && !defined(__WXMICROWIN__)
    wxChar buf[256];
    GetWindowsDirectory(buf, 256);
    return wxString(buf);
#else
    return wxEmptyString;
#endif
}

bool wxEndsWithPathSeparator(const wxChar *pszFileName)
{
    size_t len = wxStrlen(pszFileName);

    return len && wxIsPathSeparator(pszFileName[len - 1]);
}

// find a file in a list of directories, returns false if not found
bool wxFindFileInPath(wxString *pStr, const wxChar *pszPath, const wxChar *pszFile)
{
    // we assume that it's not empty
    wxCHECK_MSG( !wxIsEmpty(pszFile), FALSE,
                 _T("empty file name in wxFindFileInPath"));

    // skip path separator in the beginning of the file name if present
    if ( wxIsPathSeparator(*pszFile) )
        pszFile++;

    // copy the path (strtok will modify it)
    wxChar *szPath = new wxChar[wxStrlen(pszPath) + 1];
    wxStrcpy(szPath, pszPath);

    wxString strFile;
    wxChar *pc, *save_ptr;
    for ( pc = wxStrtok(szPath, wxPATH_SEP, &save_ptr);
          pc != NULL;
          pc = wxStrtok((wxChar *) NULL, wxPATH_SEP, &save_ptr) )
    {
        // search for the file in this directory
        strFile = pc;
        if ( !wxEndsWithPathSeparator(pc) )
            strFile += wxFILE_SEP_PATH;
        strFile += pszFile;

        if ( wxFileExists(strFile) ) {
            *pStr = strFile;
            break;
        }
    }

    // suppress warning about unused variable save_ptr when wxStrtok() is a
    // macro which throws away its third argument
    save_ptr = pc;

    delete [] szPath;

    return pc != NULL;  // if true => we breaked from the loop
}

void WXDLLEXPORT wxSplitPath(const wxChar *pszFileName,
                             wxString *pstrPath,
                             wxString *pstrName,
                             wxString *pstrExt)
{
    // it can be empty, but it shouldn't be NULL
    wxCHECK_RET( pszFileName, wxT("NULL file name in wxSplitPath") );

    wxFileName::SplitPath(pszFileName, pstrPath, pstrName, pstrExt);
}

time_t WXDLLEXPORT wxFileModificationTime(const wxString& filename)
{
    wxStructStat buf;
    wxStat( filename, &buf);
    
    return buf.st_mtime;
}


//------------------------------------------------------------------------
// wild character routines
//------------------------------------------------------------------------

bool wxIsWild( const wxString& pattern )
{
    wxString tmp = pattern;
    wxChar *pat = WXSTRINGCAST(tmp);
    while (*pat) 
    {
        switch (*pat++) 
        {
        case wxT('?'): case wxT('*'): case wxT('['): case wxT('{'):
            return TRUE;
        case wxT('\\'):
            if (!*pat++)
                return FALSE;
        }
    }
    return FALSE;
}

/*
* Written By Douglas A. Lewis <dalewis@cs.Buffalo.EDU>
*
* The match procedure is public domain code (from ircII's reg.c)
*/

bool wxMatchWild( const wxString& pat, const wxString& text, bool dot_special )
{
        if (text.empty())
        {
                /* Match if both are empty. */
                return pat.empty();
        }
        
        const wxChar *m = pat.c_str(),
        *n = text.c_str(),
        *ma = NULL,
        *na = NULL,
        *mp = NULL,
        *np = NULL;
        int just = 0,
        pcount = 0,
        acount = 0,
        count = 0;

        if (dot_special && (*n == wxT('.')))
        {
                /* Never match so that hidden Unix files 
                 * are never found. */
                return FALSE;
        }

        for (;;)
        {
                if (*m == wxT('*'))
                {
                        ma = ++m;
                        na = n;
                        just = 1;
                        mp = NULL;
                        acount = count;
                }
                else if (*m == wxT('?'))
                {
                        m++;
                        if (!*n++)
                        return FALSE;
                }
                else
                {
                        if (*m == wxT('\\'))
                        {
                                m++;
                                /* Quoting "nothing" is a bad thing */
                                if (!*m)
                                return FALSE;
                        }
                        if (!*m)
                        {
                                /*
                                * If we are out of both strings or we just
                                * saw a wildcard, then we can say we have a
                                * match
                                */
                                if (!*n)
                                return TRUE;
                                if (just)
                                return TRUE;
                                just = 0;
                                goto not_matched;
                        }
                        /*
                        * We could check for *n == NULL at this point, but
                        * since it's more common to have a character there,
                        * check to see if they match first (m and n) and
                        * then if they don't match, THEN we can check for
                        * the NULL of n
                        */
                        just = 0;
                        if (*m == *n)
                        {
                                m++;
                                if (*n == wxT(' '))
                                mp = NULL;
                                count++;
                                n++;
                        }
                        else
                        {

                                not_matched:

                                /*
                                * If there are no more characters in the
                                * string, but we still need to find another
                                * character (*m != NULL), then it will be
                                * impossible to match it
                                */
                                if (!*n)
                                return FALSE;
                                if (mp)
                                {
                                        m = mp;
                                        if (*np == wxT(' '))
                                        {
                                                mp = NULL;
                                                goto check_percent;
                                        }
                                        n = ++np;
                                        count = pcount;
                                }
                                else
                                check_percent:

                                if (ma)
                                {
                                        m = ma;
                                        n = ++na;
                                        count = acount;
                                }
                                else
                                return FALSE;
                        }
                }
        }
}


#ifdef __VISUALC__
    #pragma warning(default:4706)   // assignment within conditional expression
#endif // VC++
