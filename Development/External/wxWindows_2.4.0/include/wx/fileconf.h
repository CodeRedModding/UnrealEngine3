///////////////////////////////////////////////////////////////////////////////
// Name:        wx/fileconf.h
// Purpose:     wxFileConfig derivation of wxConfigBase
// Author:      Vadim Zeitlin
// Modified by:
// Created:     07.04.98 (adapted from appconf.cpp)
// RCS-ID:      $Id: fileconf.h,v 1.38 2002/08/31 11:29:10 GD Exp $
// Copyright:   (c) 1997 Karsten Ball�der   &  Vadim Zeitlin
//                       Ballueder@usa.net     <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

#ifndef   _FILECONF_H
#define   _FILECONF_H

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "fileconf.h"
#endif

#include "wx/defs.h"

#if wxUSE_CONFIG

#include "wx/textfile.h"
#include "wx/string.h"

// ----------------------------------------------------------------------------
// wxFileConfig
// ----------------------------------------------------------------------------

/*
  wxFileConfig derives from base Config and implements file based config class,
  i.e. it uses ASCII disk files to store the information. These files are
  alternatively called INI, .conf or .rc in the documentation. They are
  organized in groups or sections, which can nest (i.e. a group contains
  subgroups, which contain their own subgroups &c). Each group has some
  number of entries, which are "key = value" pairs. More precisely, the format
  is:

  # comments are allowed after either ';' or '#' (Win/UNIX standard)

  # blank lines (as above) are ignored

  # global entries are members of special (no name) top group
  written_for = Windows
  platform    = Linux

  # the start of the group 'Foo'
  [Foo]                           # may put comments like this also
  # following 3 lines are entries
  key = value
  another_key = "  strings with spaces in the beginning should be quoted, \
                   otherwise the spaces are lost"
  last_key = but you don't have to put " normally (nor quote them, like here)

  # subgroup of the group 'Foo'
  # (order is not important, only the name is: separator is '/', as in paths)
  [Foo/Bar]
  # entries prefixed with "!" are immutable, i.e. can't be changed if they are
  # set in the system-wide config file
  !special_key = value
  bar_entry = whatever

  [Foo/Bar/Fubar]   # depth is (theoretically :-) unlimited
  # may have the same name as key in another section
  bar_entry = whatever not

  You have {read/write/delete}Entry functions (guess what they do) and also
  setCurrentPath to select current group. enum{Subgroups/Entries} allow you
  to get all entries in the config file (in the current group). Finally,
  flush() writes immediately all changed entries to disk (otherwise it would
  be done automatically in dtor)

  wxFileConfig manages not less than 2 config files for each program: global
  and local (or system and user if you prefer). Entries are read from both of
  them and the local entries override the global ones unless the latter is
  immutable (prefixed with '!') in which case a warning message is generated
  and local value is ignored. Of course, the changes are always written to local
  file only.

  The names of these files can be specified in a number of ways. First of all,
  you can use the standard convention: using the ctor which takes 'strAppName'
  parameter will probably be sufficient for 90% of cases. If, for whatever
  reason you wish to use the files with some other names, you can always use the
  second ctor.

  wxFileConfig also may automatically expand the values of environment variables
  in the entries it reads: for example, if you have an entry
    score_file = $HOME/.score
  a call to Read(&str, "score_file") will return a complete path to .score file
  unless the expansion was previousle disabled with SetExpandEnvVars(FALSE) call
  (it's on by default, the current status can be retrieved with
   IsExpandingEnvVars function).
*/
class WXDLLEXPORT wxFileConfigGroup;
class WXDLLEXPORT wxFileConfigEntry;
class WXDLLEXPORT wxFileConfigLineList;
class WXDLLEXPORT wxInputStream;

class WXDLLEXPORT wxFileConfig : public wxConfigBase
{
public:
  // construct the "standard" full name for global (system-wide) and
  // local (user-specific) config files from the base file name.
  //
  // the following are the filenames returned by this functions:
  //            global                local
  // Unix   /etc/file.ext           ~/.file
  // Win    %windir%\file.ext   %USERPROFILE%\file.ext
  //
  // where file is the basename of szFile, ext is it's extension
  // or .conf (Unix) or .ini (Win) if it has none
  static wxString GetGlobalFileName(const wxChar *szFile);
  static wxString GetLocalFileName(const wxChar *szFile);

  // ctor & dtor
    // New constructor: one size fits all. Specify wxCONFIG_USE_LOCAL_FILE or
    // wxCONFIG_USE_GLOBAL_FILE to say which files should be used.
  wxFileConfig(const wxString& appName,
               const wxString& vendorName = wxT(""),
               const wxString& localFilename = wxT(""),
               const wxString& globalFilename = wxT(""),
               long style = wxCONFIG_USE_LOCAL_FILE);

#if wxUSE_STREAMS
    // ctor that takes an input stream.
  wxFileConfig(wxInputStream &inStream);
#endif // wxUSE_STREAMS

    // dtor will save unsaved data
  virtual ~wxFileConfig();

  // under Unix, set the umask to be used for the file creation, do nothing
  // under other systems
#ifdef __UNIX__
  void SetUmask(int mode) { m_umask = mode; }
#else // !__UNIX__
  void SetUmask(int WXUNUSED(mode)) { }
#endif // __UNIX__/!__UNIX__

  // implement inherited pure virtual functions
  virtual void SetPath(const wxString& strPath);
  virtual const wxString& GetPath() const { return m_strPath; }

  virtual bool GetFirstGroup(wxString& str, long& lIndex) const;
  virtual bool GetNextGroup (wxString& str, long& lIndex) const;
  virtual bool GetFirstEntry(wxString& str, long& lIndex) const;
  virtual bool GetNextEntry (wxString& str, long& lIndex) const;

  virtual size_t GetNumberOfEntries(bool bRecursive = FALSE) const;
  virtual size_t GetNumberOfGroups(bool bRecursive = FALSE) const;

  virtual bool HasGroup(const wxString& strName) const;
  virtual bool HasEntry(const wxString& strName) const;

  virtual bool Flush(bool bCurrentOnly = FALSE);

  virtual bool RenameEntry(const wxString& oldName, const wxString& newName);
  virtual bool RenameGroup(const wxString& oldName, const wxString& newName);

  virtual bool DeleteEntry(const wxString& key, bool bGroupIfEmptyAlso = TRUE);
  virtual bool DeleteGroup(const wxString& szKey);
  virtual bool DeleteAll();

public:
  // functions to work with this list
  wxFileConfigLineList *LineListAppend(const wxString& str);
  wxFileConfigLineList *LineListInsert(const wxString& str,
                           wxFileConfigLineList *pLine);    // NULL => Prepend()
  void      LineListRemove(wxFileConfigLineList *pLine);
  bool      LineListIsEmpty();

protected:
  virtual bool DoReadString(const wxString& key, wxString *pStr) const;
  virtual bool DoReadLong(const wxString& key, long *pl) const;

  virtual bool DoWriteString(const wxString& key, const wxString& szValue);
  virtual bool DoWriteLong(const wxString& key, long lValue);

private:
  // GetXXXFileName helpers: return ('/' terminated) directory names
  static wxString GetGlobalDir();
  static wxString GetLocalDir();

  // common part of all ctors (assumes that m_str{Local|Global}File are already
  // initialized
  void Init();

  // common part of from dtor and DeleteAll
  void CleanUp();

  // parse the whole file
  void Parse(wxTextBuffer& buffer, bool bLocal);

  // the same as SetPath("/")
  void SetRootPath();

  // member variables
  // ----------------
  wxFileConfigLineList *m_linesHead,        // head of the linked list
                       *m_linesTail;        // tail

  wxString    m_strLocalFile,     // local  file name passed to ctor
              m_strGlobalFile;    // global
  wxString    m_strPath;          // current path (not '/' terminated)

  wxFileConfigGroup *m_pRootGroup,      // the top (unnamed) group
                    *m_pCurrentGroup;   // the current group

#ifdef __UNIX__
  int m_umask;                    // the umask to use for file creation
#endif // __UNIX__
};

#endif
  // wxUSE_CONFIG

#endif  
  //_FILECONF_H

