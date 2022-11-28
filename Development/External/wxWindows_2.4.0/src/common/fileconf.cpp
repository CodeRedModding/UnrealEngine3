///////////////////////////////////////////////////////////////////////////////
// Name:        fileconf.cpp
// Purpose:     implementation of wxFileConfig derivation of wxConfig
// Author:      Vadim Zeitlin
// Modified by:
// Created:     07.04.98 (adapted from appconf.cpp)
// RCS-ID:      $Id: fileconf.cpp,v 1.90.2.5 2002/10/29 21:47:44 RR Exp $
// Copyright:   (c) 1997 Karsten Ball�der   &  Vadim Zeitlin
//                       Ballueder@usa.net     <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#pragma implementation "fileconf.h"
#endif

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include  "wx/wxprec.h"

#ifdef    __BORLANDC__
  #pragma hdrstop
#endif  //__BORLANDC__

#if wxUSE_CONFIG

#ifndef   WX_PRECOMP
  #include  "wx/string.h"
  #include  "wx/intl.h"
#endif  //WX_PRECOMP

#include  "wx/app.h"
#include  "wx/dynarray.h"
#include  "wx/file.h"
#include  "wx/log.h"
#include  "wx/textfile.h"
#include  "wx/memtext.h"
#include  "wx/config.h"
#include  "wx/fileconf.h"

#if wxUSE_STREAMS
    #include  "wx/stream.h"
#endif // wxUSE_STREAMS

#include  "wx/utils.h"    // for wxGetHomeDir

#if defined(__WXMAC__)
  #include  "wx/mac/private.h"  // includes mac headers
#endif

#if defined(__WXMSW__)
  #include "wx/msw/private.h"
#endif  //windows.h
#if defined(__WXPM__)
  #define INCL_DOS
  #include <os2.h>
#endif

#include  <stdlib.h>
#include  <ctype.h>

// headers needed for umask()
#ifdef __UNIX__
    #include <sys/types.h>
    #include <sys/stat.h>
#endif // __UNIX__

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------
#define CONST_CAST ((wxFileConfig *)this)->

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

#ifndef MAX_PATH
  #define MAX_PATH 512
#endif

// ----------------------------------------------------------------------------
// global functions declarations
// ----------------------------------------------------------------------------

// compare functions for sorting the arrays
static int LINKAGEMODE CompareEntries(wxFileConfigEntry *p1, wxFileConfigEntry *p2);
static int LINKAGEMODE CompareGroups(wxFileConfigGroup *p1, wxFileConfigGroup *p2);

// filter strings
static wxString FilterInValue(const wxString& str);
static wxString FilterOutValue(const wxString& str);

static wxString FilterInEntryName(const wxString& str);
static wxString FilterOutEntryName(const wxString& str);

// get the name to use in wxFileConfig ctor
static wxString GetAppName(const wxString& appname);

// ============================================================================
// private classes
// ============================================================================

// ----------------------------------------------------------------------------
// "template" array types
// ----------------------------------------------------------------------------

WX_DEFINE_SORTED_EXPORTED_ARRAY(wxFileConfigEntry *, ArrayEntries);
WX_DEFINE_SORTED_EXPORTED_ARRAY(wxFileConfigGroup *, ArrayGroups);

// ----------------------------------------------------------------------------
// wxFileConfigLineList
// ----------------------------------------------------------------------------

// we store all lines of the local config file as a linked list in memory
class wxFileConfigLineList
{
public:
  void SetNext(wxFileConfigLineList *pNext)  { m_pNext = pNext; }
  void SetPrev(wxFileConfigLineList *pPrev)  { m_pPrev = pPrev; }

  // ctor
  wxFileConfigLineList(const wxString& str,
                       wxFileConfigLineList *pNext = NULL) : m_strLine(str)
    { SetNext(pNext); SetPrev(NULL); }

  // next/prev nodes in the linked list
  wxFileConfigLineList *Next() const { return m_pNext;  }
  wxFileConfigLineList *Prev() const { return m_pPrev;  }

  // get/change lines text
  void SetText(const wxString& str) { m_strLine = str;  }
  const wxString& Text() const { return m_strLine; }

private:
  wxString  m_strLine;                  // line contents
  wxFileConfigLineList *m_pNext,        // next node
                       *m_pPrev;        // previous one
};

// ----------------------------------------------------------------------------
// wxFileConfigEntry: a name/value pair
// ----------------------------------------------------------------------------

class wxFileConfigEntry
{
private:
  wxFileConfigGroup *m_pParent; // group that contains us

  wxString      m_strName,      // entry name
                m_strValue;     //       value
  bool          m_bDirty:1,     // changed since last read?
                m_bImmutable:1, // can be overriden locally?
                m_bHasValue:1;  // set after first call to SetValue()

  int           m_nLine;        // used if m_pLine == NULL only

  // pointer to our line in the linked list or NULL if it was found in global
  // file (which we don't modify)
  wxFileConfigLineList *m_pLine;

public:
  wxFileConfigEntry(wxFileConfigGroup *pParent,
                    const wxString& strName, int nLine);

  // simple accessors
  const wxString& Name()        const { return m_strName;    }
  const wxString& Value()       const { return m_strValue;   }
  wxFileConfigGroup *Group()    const { return m_pParent;    }
  bool            IsDirty()     const { return m_bDirty;     }
  bool            IsImmutable() const { return m_bImmutable; }
  bool            IsLocal()     const { return m_pLine != 0; }
  int             Line()        const { return m_nLine;      }
  wxFileConfigLineList *
                  GetLine()     const { return m_pLine;      }

  // modify entry attributes
  void SetValue(const wxString& strValue, bool bUser = TRUE);
  void SetDirty();
  void SetLine(wxFileConfigLineList *pLine);
};

// ----------------------------------------------------------------------------
// wxFileConfigGroup: container of entries and other groups
// ----------------------------------------------------------------------------

class wxFileConfigGroup
{
private:
  wxFileConfig *m_pConfig;          // config object we belong to
  wxFileConfigGroup  *m_pParent;    // parent group (NULL for root group)
  ArrayEntries  m_aEntries;         // entries in this group
  ArrayGroups   m_aSubgroups;       // subgroups
  wxString      m_strName;          // group's name
  bool          m_bDirty;           // if FALSE => all subgroups are not dirty
  wxFileConfigLineList *m_pLine;    // pointer to our line in the linked list
  wxFileConfigEntry *m_pLastEntry;  // last entry/subgroup of this group in the
  wxFileConfigGroup *m_pLastGroup;  // local file (we insert new ones after it)

  // DeleteSubgroupByName helper
  bool DeleteSubgroup(wxFileConfigGroup *pGroup);

public:
  // ctor
  wxFileConfigGroup(wxFileConfigGroup *pParent, const wxString& strName, wxFileConfig *);

  // dtor deletes all entries and subgroups also
  ~wxFileConfigGroup();

  // simple accessors
  const wxString& Name()    const { return m_strName; }
  wxFileConfigGroup    *Parent()  const { return m_pParent; }
  wxFileConfig   *Config()  const { return m_pConfig; }
  bool            IsDirty() const { return m_bDirty;  }

  const ArrayEntries& Entries() const { return m_aEntries;   }
  const ArrayGroups&  Groups()  const { return m_aSubgroups; }
  bool  IsEmpty() const { return Entries().IsEmpty() && Groups().IsEmpty(); }

  // find entry/subgroup (NULL if not found)
  wxFileConfigGroup *FindSubgroup(const wxChar *szName) const;
  wxFileConfigEntry *FindEntry   (const wxChar *szName) const;

  // delete entry/subgroup, return FALSE if doesn't exist
  bool DeleteSubgroupByName(const wxChar *szName);
  bool DeleteEntry(const wxChar *szName);

  // create new entry/subgroup returning pointer to newly created element
  wxFileConfigGroup *AddSubgroup(const wxString& strName);
  wxFileConfigEntry *AddEntry   (const wxString& strName, int nLine = wxNOT_FOUND);

  // will also recursively set parent's dirty flag
  void SetDirty();
  void SetLine(wxFileConfigLineList *pLine);

  // rename: no checks are done to ensure that the name is unique!
  void Rename(const wxString& newName);

  //
  wxString GetFullName() const;

  // get the last line belonging to an entry/subgroup of this group
  wxFileConfigLineList *GetGroupLine();     // line which contains [group]
  wxFileConfigLineList *GetLastEntryLine(); // after which our subgroups start
  wxFileConfigLineList *GetLastGroupLine(); // after which the next group starts

  // called by entries/subgroups when they're created/deleted
  void SetLastEntry(wxFileConfigEntry *pEntry) { m_pLastEntry = pEntry; }
  void SetLastGroup(wxFileConfigGroup *pGroup) { m_pLastGroup = pGroup; }
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// static functions
// ----------------------------------------------------------------------------
wxString wxFileConfig::GetGlobalDir()
{
  wxString strDir;

#ifdef __VMS__ // Note if __VMS is defined __UNIX is also defined
    strDir = wxT("sys$manager:");
#elif defined(__WXMAC__)
    strDir = wxMacFindFolder(  (short) kOnSystemDisk, kPreferencesFolderType, kDontCreateFolder ) ;
#elif defined( __UNIX__ )
    strDir = wxT("/etc/");
#elif defined(__WXPM__)
    ULONG aulSysInfo[QSV_MAX] = {0};
    UINT drive;
    APIRET rc;

    rc = DosQuerySysInfo( 1L, QSV_MAX, (PVOID)aulSysInfo, sizeof(ULONG)*QSV_MAX);
    if (rc == 0)
    {
        drive = aulSysInfo[QSV_BOOT_DRIVE - 1];
        strDir.Printf(wxT("%c:\\OS2\\"), 'A'+drive-1);
    }
#elif defined(__WXSTUBS__)
    wxASSERT_MSG( FALSE, wxT("TODO") ) ;
#elif defined(__DOS__)
    // There's no such thing as global cfg dir in MS-DOS, let's return
    // current directory (FIXME_MGL?)
    return wxT(".\\");
#else // Windows
    wxChar szWinDir[MAX_PATH];
    ::GetWindowsDirectory(szWinDir, MAX_PATH);

    strDir = szWinDir;
    strDir << wxT('\\');
#endif // Unix/Windows

    return strDir;
}

wxString wxFileConfig::GetLocalDir()
{
    wxString strDir;

#if defined(__WXMAC__) || defined(__DOS__)
    // no local dir concept on Mac OS 9 or MS-DOS
    return GetGlobalDir() ;
#else
    wxGetHomeDir(&strDir);

#ifdef  __UNIX__
#ifdef __VMS
    if (strDir.Last() != wxT(']'))
#endif
    if (strDir.Last() != wxT('/')) strDir << wxT('/');
#else
    if (strDir.Last() != wxT('\\')) strDir << wxT('\\');
#endif
#endif

    return strDir;
}

wxString wxFileConfig::GetGlobalFileName(const wxChar *szFile)
{
    wxString str = GetGlobalDir();
    str << szFile;

    if ( wxStrchr(szFile, wxT('.')) == NULL )
#if defined( __WXMAC__ )
        str << " Preferences";
#elif defined( __UNIX__ )
        str << wxT(".conf");
#else   // Windows
        str << wxT(".ini");
#endif  // UNIX/Win

    return str;
}

wxString wxFileConfig::GetLocalFileName(const wxChar *szFile)
{
#ifdef __VMS__ 
    // On VMS I saw the problem that the home directory was appended
    // twice for the configuration file. Does that also happen for
    // other platforms?
    wxString str = wxT( '.' );
#else
    wxString str = GetLocalDir();
#endif

#if defined( __UNIX__ ) && !defined( __VMS ) && !defined( __WXMAC__ )
    str << wxT('.');
#endif

    str << szFile;

#if defined(__WINDOWS__) || defined(__DOS__)
    if ( wxStrchr(szFile, wxT('.')) == NULL )
        str << wxT(".ini");
#endif

#ifdef __WXMAC__
    str << " Preferences";
#endif

    return str;
}

// ----------------------------------------------------------------------------
// ctor
// ----------------------------------------------------------------------------

void wxFileConfig::Init()
{
    m_pCurrentGroup =
    m_pRootGroup    = new wxFileConfigGroup(NULL, wxT(""), this);

    m_linesHead =
    m_linesTail = NULL;

    // It's not an error if (one of the) file(s) doesn't exist.

    // parse the global file
    if ( !m_strGlobalFile.IsEmpty() && wxFile::Exists(m_strGlobalFile) )
    {
        wxTextFile fileGlobal(m_strGlobalFile);

#if defined(__WXGTK20__) && wxUSE_UNICODE
        if ( fileGlobal.Open( wxConvUTF8 ) ) 
#else
        if ( fileGlobal.Open() ) 
#endif
        {
            Parse(fileGlobal, FALSE /* global */);
            SetRootPath();
        }
        else
        {
            wxLogWarning(_("can't open global configuration file '%s'."), m_strGlobalFile.c_str());
        }
    }

    // parse the local file
    if ( !m_strLocalFile.IsEmpty() && wxFile::Exists(m_strLocalFile) )
    {
        wxTextFile fileLocal(m_strLocalFile);
#if defined(__WXGTK20__) && wxUSE_UNICODE
        if ( fileLocal.Open( wxConvUTF8 ) ) 
#else
        if ( fileLocal.Open() ) 
#endif
        {
            Parse(fileLocal, TRUE /* local */);
            SetRootPath();
        }
        else
        {
            wxLogWarning(_("can't open user configuration file '%s'."),  m_strLocalFile.c_str() );
        }
    }
}

// constructor supports creation of wxFileConfig objects of any type
wxFileConfig::wxFileConfig(const wxString& appName, const wxString& vendorName,
                           const wxString& strLocal, const wxString& strGlobal,
                           long style)
            : wxConfigBase(::GetAppName(appName), vendorName,
                           strLocal, strGlobal,
                           style),
              m_strLocalFile(strLocal), m_strGlobalFile(strGlobal)
{
    // Make up names for files if empty
    if ( m_strLocalFile.IsEmpty() && (style & wxCONFIG_USE_LOCAL_FILE) )
        m_strLocalFile = GetLocalFileName(GetAppName());

    if ( m_strGlobalFile.IsEmpty() && (style & wxCONFIG_USE_GLOBAL_FILE) )
        m_strGlobalFile = GetGlobalFileName(GetAppName());

    // Check if styles are not supplied, but filenames are, in which case
    // add the correct styles.
    if ( !m_strLocalFile.IsEmpty() )
        SetStyle(GetStyle() | wxCONFIG_USE_LOCAL_FILE);

    if ( !m_strGlobalFile.IsEmpty() )
        SetStyle(GetStyle() | wxCONFIG_USE_GLOBAL_FILE);

    // if the path is not absolute, prepend the standard directory to it
    // UNLESS wxCONFIG_USE_RELATIVE_PATH style is set
    if ( !(style & wxCONFIG_USE_RELATIVE_PATH) )
    {
        if ( !m_strLocalFile.IsEmpty() && !wxIsAbsolutePath(m_strLocalFile) )
        {
            wxString strLocal = m_strLocalFile;
            m_strLocalFile = GetLocalDir();
            m_strLocalFile << strLocal;
        }

        if ( !m_strGlobalFile.IsEmpty() && !wxIsAbsolutePath(m_strGlobalFile) )
        {
            wxString strGlobal = m_strGlobalFile;
            m_strGlobalFile = GetGlobalDir();
            m_strGlobalFile << strGlobal;
        }
    }

    SetUmask(-1);
    
    Init();
}

#if wxUSE_STREAMS

wxFileConfig::wxFileConfig(wxInputStream &inStream)
{
    // always local_file when this constructor is called (?)
    SetStyle(GetStyle() | wxCONFIG_USE_LOCAL_FILE);

    m_pCurrentGroup =
    m_pRootGroup    = new wxFileConfigGroup(NULL, wxT(""), this);

    m_linesHead =
    m_linesTail = NULL;

    // translate everything to the current (platform-dependent) line
    // termination character
    wxString strTrans;
    {
        wxString strTmp;

        char buf[1024];
        while ( !inStream.Read(buf, WXSIZEOF(buf)).Eof() )
            strTmp.append(wxConvertMB2WX(buf), inStream.LastRead());

        strTmp.append(wxConvertMB2WX(buf), inStream.LastRead());

        strTrans = wxTextBuffer::Translate(strTmp);
    }

    wxMemoryText memText;

    // Now we can add the text to the memory text. To do this we extract line
    // by line from the translated string, until we've reached the end.
    //
    // VZ: all this is horribly inefficient, we should do the translation on
    //     the fly in one pass saving both memory and time (TODO)

    const wxChar *pEOL = wxTextBuffer::GetEOL(wxTextBuffer::typeDefault);
    const size_t EOLLen = wxStrlen(pEOL);

    int posLineStart = strTrans.Find(pEOL);
    while ( posLineStart != -1 )
    {
        wxString line(strTrans.Left(posLineStart));

        memText.AddLine(line);

        strTrans = strTrans.Mid(posLineStart + EOLLen);

        posLineStart = strTrans.Find(pEOL);
    }

    // also add whatever we have left in the translated string.
    memText.AddLine(strTrans);

    // Finally we can parse it all.
    Parse(memText, TRUE /* local */);

    SetRootPath();
}

#endif // wxUSE_STREAMS

void wxFileConfig::CleanUp()
{
  delete m_pRootGroup;

  wxFileConfigLineList *pCur = m_linesHead;
  while ( pCur != NULL ) {
    wxFileConfigLineList *pNext = pCur->Next();
    delete pCur;
    pCur = pNext;
  }
}

wxFileConfig::~wxFileConfig()
{
  Flush();

  CleanUp();
}

// ----------------------------------------------------------------------------
// parse a config file
// ----------------------------------------------------------------------------

void wxFileConfig::Parse(wxTextBuffer& buffer, bool bLocal)
{
  const wxChar *pStart;
  const wxChar *pEnd;
  wxString strLine;

  size_t nLineCount = buffer.GetLineCount();
  
  for ( size_t n = 0; n < nLineCount; n++ )
  {
    strLine = buffer[n];

    // add the line to linked list
    if ( bLocal )
      LineListAppend(strLine);

    // skip leading spaces
    for ( pStart = strLine; wxIsspace(*pStart); pStart++ )
      ;

    // skip blank/comment lines
    if ( *pStart == wxT('\0')|| *pStart == wxT(';') || *pStart == wxT('#') )
      continue;

    if ( *pStart == wxT('[') ) {          // a new group
      pEnd = pStart;

      while ( *++pEnd != wxT(']') ) {
        if ( *pEnd == wxT('\\') ) {
            // the next char is escaped, so skip it even if it is ']'
            pEnd++;
        }

        if ( *pEnd == wxT('\n') || *pEnd == wxT('\0') ) {
            // we reached the end of line, break out of the loop
            break;
        }
      }

      if ( *pEnd != wxT(']') ) {
        wxLogError(_("file '%s': unexpected character %c at line %d."),
                   buffer.GetName(), *pEnd, n + 1);
        continue; // skip this line
      }

      // group name here is always considered as abs path
      wxString strGroup;
      pStart++;
      strGroup << wxCONFIG_PATH_SEPARATOR
               << FilterInEntryName(wxString(pStart, pEnd - pStart));

      // will create it if doesn't yet exist
      SetPath(strGroup);

      if ( bLocal )
        m_pCurrentGroup->SetLine(m_linesTail);

      // check that there is nothing except comments left on this line
      bool bCont = TRUE;
      while ( *++pEnd != wxT('\0') && bCont ) {
        switch ( *pEnd ) {
          case wxT('#'):
          case wxT(';'):
            bCont = FALSE;
            break;

          case wxT(' '):
          case wxT('\t'):
            // ignore whitespace ('\n' impossible here)
            break;

          default:
            wxLogWarning(_("file '%s', line %d: '%s' ignored after group header."),
                         buffer.GetName(), n + 1, pEnd);
            bCont = FALSE;
        }
      }
    }
    else {                        // a key
      const wxChar *pEnd = pStart;
      while ( *pEnd && *pEnd != wxT('=') && !wxIsspace(*pEnd) ) {
        if ( *pEnd == wxT('\\') ) {
          // next character may be space or not - still take it because it's
          // quoted (unless there is nothing)
          pEnd++;
          if ( !*pEnd ) {
            // the error message will be given below anyhow
            break;
          }
        }

        pEnd++;
      }

      wxString strKey(FilterInEntryName(wxString(pStart, pEnd)));

      // skip whitespace
      while ( wxIsspace(*pEnd) )
        pEnd++;

      if ( *pEnd++ != wxT('=') ) {
        wxLogError(_("file '%s', line %d: '=' expected."),
                   buffer.GetName(), n + 1);
      }
      else {
        wxFileConfigEntry *pEntry = m_pCurrentGroup->FindEntry(strKey);

        if ( pEntry == NULL ) {
          // new entry
          pEntry = m_pCurrentGroup->AddEntry(strKey, n);

          if ( bLocal )
            pEntry->SetLine(m_linesTail);
        }
        else {
          if ( bLocal && pEntry->IsImmutable() ) {
            // immutable keys can't be changed by user
            wxLogWarning(_("file '%s', line %d: value for immutable key '%s' ignored."),
                         buffer.GetName(), n + 1, strKey.c_str());
            continue;
          }
          // the condition below catches the cases (a) and (b) but not (c):
          //  (a) global key found second time in global file
          //  (b) key found second (or more) time in local file
          //  (c) key from global file now found in local one
          // which is exactly what we want.
          else if ( !bLocal || pEntry->IsLocal() ) {
            wxLogWarning(_("file '%s', line %d: key '%s' was first found at line %d."),
                         buffer.GetName(), n + 1, strKey.c_str(), pEntry->Line());

            if ( bLocal )
              pEntry->SetLine(m_linesTail);
          }
        }

        // skip whitespace
        while ( wxIsspace(*pEnd) )
          pEnd++;

        wxString value = pEnd;
        if ( !(GetStyle() & wxCONFIG_USE_NO_ESCAPE_CHARACTERS) )
            value = FilterInValue(value);

        pEntry->SetValue(value, FALSE);
      }
    }
  }
}

// ----------------------------------------------------------------------------
// set/retrieve path
// ----------------------------------------------------------------------------

void wxFileConfig::SetRootPath()
{
  m_strPath.Empty();
  m_pCurrentGroup = m_pRootGroup;
}

void wxFileConfig::SetPath(const wxString& strPath)
{
  wxArrayString aParts;

  if ( strPath.IsEmpty() ) {
    SetRootPath();
    return;
  }

  if ( strPath[0] == wxCONFIG_PATH_SEPARATOR ) {
    // absolute path
    wxSplitPath(aParts, strPath);
  }
  else {
    // relative path, combine with current one
    wxString strFullPath = m_strPath;
    strFullPath << wxCONFIG_PATH_SEPARATOR << strPath;
    wxSplitPath(aParts, strFullPath);
  }

  // change current group
  size_t n;
  m_pCurrentGroup = m_pRootGroup;
  for ( n = 0; n < aParts.Count(); n++ ) {
    wxFileConfigGroup *pNextGroup = m_pCurrentGroup->FindSubgroup(aParts[n]);
    if ( pNextGroup == NULL )
      pNextGroup = m_pCurrentGroup->AddSubgroup(aParts[n]);
    m_pCurrentGroup = pNextGroup;
  }

  // recombine path parts in one variable
  m_strPath.Empty();
  for ( n = 0; n < aParts.Count(); n++ ) {
    m_strPath << wxCONFIG_PATH_SEPARATOR << aParts[n];
  }
}

// ----------------------------------------------------------------------------
// enumeration
// ----------------------------------------------------------------------------

bool wxFileConfig::GetFirstGroup(wxString& str, long& lIndex) const
{
  lIndex = 0;
  return GetNextGroup(str, lIndex);
}

bool wxFileConfig::GetNextGroup (wxString& str, long& lIndex) const
{
  if ( size_t(lIndex) < m_pCurrentGroup->Groups().Count() ) {
    str = m_pCurrentGroup->Groups()[(size_t)lIndex++]->Name();
    return TRUE;
  }
  else
    return FALSE;
}

bool wxFileConfig::GetFirstEntry(wxString& str, long& lIndex) const
{
  lIndex = 0;
  return GetNextEntry(str, lIndex);
}

bool wxFileConfig::GetNextEntry (wxString& str, long& lIndex) const
{
  if ( size_t(lIndex) < m_pCurrentGroup->Entries().Count() ) {
    str = m_pCurrentGroup->Entries()[(size_t)lIndex++]->Name();
    return TRUE;
  }
  else
    return FALSE;
}

size_t wxFileConfig::GetNumberOfEntries(bool bRecursive) const
{
  size_t n = m_pCurrentGroup->Entries().Count();
  if ( bRecursive ) {
    wxFileConfigGroup *pOldCurrentGroup = m_pCurrentGroup;
    size_t nSubgroups = m_pCurrentGroup->Groups().Count();
    for ( size_t nGroup = 0; nGroup < nSubgroups; nGroup++ ) {
      CONST_CAST m_pCurrentGroup = m_pCurrentGroup->Groups()[nGroup];
      n += GetNumberOfEntries(TRUE);
      CONST_CAST m_pCurrentGroup = pOldCurrentGroup;
    }
  }

  return n;
}

size_t wxFileConfig::GetNumberOfGroups(bool bRecursive) const
{
  size_t n = m_pCurrentGroup->Groups().Count();
  if ( bRecursive ) {
    wxFileConfigGroup *pOldCurrentGroup = m_pCurrentGroup;
    size_t nSubgroups = m_pCurrentGroup->Groups().Count();
    for ( size_t nGroup = 0; nGroup < nSubgroups; nGroup++ ) {
      CONST_CAST m_pCurrentGroup = m_pCurrentGroup->Groups()[nGroup];
      n += GetNumberOfGroups(TRUE);
      CONST_CAST m_pCurrentGroup = pOldCurrentGroup;
    }
  }

  return n;
}

// ----------------------------------------------------------------------------
// tests for existence
// ----------------------------------------------------------------------------

bool wxFileConfig::HasGroup(const wxString& strName) const
{
  wxConfigPathChanger path(this, strName);

  wxFileConfigGroup *pGroup = m_pCurrentGroup->FindSubgroup(path.Name());
  return pGroup != NULL;
}

bool wxFileConfig::HasEntry(const wxString& strName) const
{
  wxConfigPathChanger path(this, strName);

  wxFileConfigEntry *pEntry = m_pCurrentGroup->FindEntry(path.Name());
  return pEntry != NULL;
}

// ----------------------------------------------------------------------------
// read/write values
// ----------------------------------------------------------------------------

bool wxFileConfig::DoReadString(const wxString& key, wxString* pStr) const
{
  wxConfigPathChanger path(this, key);

  wxFileConfigEntry *pEntry = m_pCurrentGroup->FindEntry(path.Name());
  if (pEntry == NULL) {
    return FALSE;
  }

  *pStr = pEntry->Value();

  return TRUE;
}

bool wxFileConfig::DoReadLong(const wxString& key, long *pl) const
{
  wxString str;
  if ( !Read(key, & str) )
  {
    return FALSE;
  }
  return str.ToLong(pl) ;
}

bool wxFileConfig::DoWriteString(const wxString& key, const wxString& szValue)
{
    wxConfigPathChanger     path(this, key);
    wxString                strName = path.Name();
  
    wxLogTrace( _T("wxFileConfig"),
                _T("  Writing String '%s' = '%s' to Group '%s'"),
                strName.c_str(),
                szValue.c_str(),
                GetPath().c_str() );

    if ( strName.IsEmpty() )
    {
            // setting the value of a group is an error

        wxASSERT_MSG( wxIsEmpty(szValue), wxT("can't set value of a group!") );

            // ... except if it's empty in which case it's a way to force it's creation

        wxLogTrace( _T("wxFileConfig"),
                    _T("  Creating group %s"),
                    m_pCurrentGroup->Name().c_str() );

        m_pCurrentGroup->SetDirty();

            // this will add a line for this group if it didn't have it before

        (void)m_pCurrentGroup->GetGroupLine();
    }
    else
    {
            // writing an entry
            // check that the name is reasonable

        if ( strName[0u] == wxCONFIG_IMMUTABLE_PREFIX )
        {
            wxLogError( _("Config entry name cannot start with '%c'."),
                        wxCONFIG_IMMUTABLE_PREFIX);
            return FALSE;
        }

        wxFileConfigEntry   *pEntry = m_pCurrentGroup->FindEntry(strName);

        if ( pEntry == 0 )
        {
            wxLogTrace( _T("wxFileConfig"),
                        _T("  Adding Entry %s"),
                        strName.c_str() );
            pEntry = m_pCurrentGroup->AddEntry(strName);
        }

        wxLogTrace( _T("wxFileConfig"),
                    _T("  Setting value %s"),
                    szValue.c_str() );
        pEntry->SetValue(szValue);
    }

    return TRUE;
}

bool wxFileConfig::DoWriteLong(const wxString& key, long lValue)
{
  return Write(key, wxString::Format(_T("%ld"), lValue));
}

bool wxFileConfig::Flush(bool /* bCurrentOnly */)
{
  if ( LineListIsEmpty() || !m_pRootGroup->IsDirty() || !m_strLocalFile )
    return TRUE;

#ifdef __UNIX__
  // set the umask if needed
  mode_t umaskOld = 0;
  if ( m_umask != -1 )
  {
      umaskOld = umask((mode_t)m_umask);
  }
#endif // __UNIX__

  wxTempFile file(m_strLocalFile);

  if ( !file.IsOpened() )
  {
    wxLogError(_("can't open user configuration file."));
    return FALSE;
  }

  // write all strings to file
  for ( wxFileConfigLineList *p = m_linesHead; p != NULL; p = p->Next() )
  {
    wxString line = p->Text();
    line += wxTextFile::GetEOL();
#if wxUSE_UNICODE
    wxCharBuffer buf = wxConvLocal.cWX2MB( line );
    if ( !file.Write( (const char*)buf, strlen( (const char*) buf ) ) )
#else
    if ( !file.Write( line.c_str(), line.Len() ) )
#endif
    {
      wxLogError(_("can't write user configuration file."));
      return FALSE;
    }
  }

  bool ret = file.Commit();

#if defined(__WXMAC__)
  if ( ret )
  {
  	FSSpec spec ;

  	wxMacFilename2FSSpec( m_strLocalFile , &spec ) ;
  	FInfo finfo ;
  	if ( FSpGetFInfo( &spec , &finfo ) == noErr )
  	{
  		finfo.fdType = 'TEXT' ;
  		finfo.fdCreator = 'ttxt' ;
  		FSpSetFInfo( &spec , &finfo ) ;
  	}
  }
#endif // __WXMAC__

#ifdef __UNIX__
  // restore the old umask if we changed it
  if ( m_umask != -1 )
  {
      (void)umask(umaskOld);
  }
#endif // __UNIX__

  return ret;
}

// ----------------------------------------------------------------------------
// renaming groups/entries
// ----------------------------------------------------------------------------

bool wxFileConfig::RenameEntry(const wxString& oldName,
                               const wxString& newName)
{
    // check that the entry exists
    wxFileConfigEntry *oldEntry = m_pCurrentGroup->FindEntry(oldName);
    if ( !oldEntry )
        return FALSE;

    // check that the new entry doesn't already exist
    if ( m_pCurrentGroup->FindEntry(newName) )
        return FALSE;

    // delete the old entry, create the new one
    wxString value = oldEntry->Value();
    if ( !m_pCurrentGroup->DeleteEntry(oldName) )
        return FALSE;

    wxFileConfigEntry *newEntry = m_pCurrentGroup->AddEntry(newName);
    newEntry->SetValue(value);

    return TRUE;
}

bool wxFileConfig::RenameGroup(const wxString& oldName,
                               const wxString& newName)
{
    // check that the group exists
    wxFileConfigGroup *group = m_pCurrentGroup->FindSubgroup(oldName);
    if ( !group )
        return FALSE;

    // check that the new group doesn't already exist
    if ( m_pCurrentGroup->FindSubgroup(newName) )
        return FALSE;

    group->Rename(newName);

    return TRUE;
}

// ----------------------------------------------------------------------------
// delete groups/entries
// ----------------------------------------------------------------------------

bool wxFileConfig::DeleteEntry(const wxString& key, bool bGroupIfEmptyAlso)
{
  wxConfigPathChanger path(this, key);

  if ( !m_pCurrentGroup->DeleteEntry(path.Name()) )
    return FALSE;

  if ( bGroupIfEmptyAlso && m_pCurrentGroup->IsEmpty() ) {
    if ( m_pCurrentGroup != m_pRootGroup ) {
      wxFileConfigGroup *pGroup = m_pCurrentGroup;
      SetPath(wxT(".."));  // changes m_pCurrentGroup!
      m_pCurrentGroup->DeleteSubgroupByName(pGroup->Name());
    }
    //else: never delete the root group
  }

  return TRUE;
}

bool wxFileConfig::DeleteGroup(const wxString& key)
{
  wxConfigPathChanger path(this, key);

  return m_pCurrentGroup->DeleteSubgroupByName(path.Name());
}

bool wxFileConfig::DeleteAll()
{
  CleanUp();

  if ( wxRemove(m_strLocalFile) == -1 )
    wxLogSysError(_("can't delete user configuration file '%s'"), m_strLocalFile.c_str());

  m_strLocalFile = m_strGlobalFile = wxT("");
  Init();

  return TRUE;
}

// ----------------------------------------------------------------------------
// linked list functions
// ----------------------------------------------------------------------------

    // append a new line to the end of the list

wxFileConfigLineList *wxFileConfig::LineListAppend(const wxString& str)
{
    wxLogTrace( _T("wxFileConfig"),
                _T("    ** Adding Line '%s'"),
                str.c_str() );
    wxLogTrace( _T("wxFileConfig"),
                _T("        head: %s"),
                ((m_linesHead) ? m_linesHead->Text().c_str() : wxEmptyString) );
    wxLogTrace( _T("wxFileConfig"),
                _T("        tail: %s"),
                ((m_linesTail) ? m_linesTail->Text().c_str() : wxEmptyString) );

    wxFileConfigLineList *pLine = new wxFileConfigLineList(str);

    if ( m_linesTail == NULL )
    {
        // list is empty
        m_linesHead = pLine;
    }
    else
    {
        // adjust pointers
        m_linesTail->SetNext(pLine);
        pLine->SetPrev(m_linesTail);
    }

    m_linesTail = pLine;

    wxLogTrace( _T("wxFileConfig"),
                _T("        head: %s"),
                ((m_linesHead) ? m_linesHead->Text().c_str() : wxEmptyString) );
    wxLogTrace( _T("wxFileConfig"),
                _T("        tail: %s"),
                ((m_linesTail) ? m_linesTail->Text().c_str() : wxEmptyString) );

    return m_linesTail;
}

    // insert a new line after the given one or in the very beginning if !pLine

wxFileConfigLineList *wxFileConfig::LineListInsert(const wxString& str,
                                                   wxFileConfigLineList *pLine)
{
    wxLogTrace( _T("wxFileConfig"),
                _T("    ** Inserting Line '%s' after '%s'"),
                str.c_str(),
                ((pLine) ? pLine->Text().c_str() : wxEmptyString) );
    wxLogTrace( _T("wxFileConfig"),
                _T("        head: %s"),
                ((m_linesHead) ? m_linesHead->Text().c_str() : wxEmptyString) );
    wxLogTrace( _T("wxFileConfig"),
                _T("        tail: %s"),
                ((m_linesTail) ? m_linesTail->Text().c_str() : wxEmptyString) );

    if ( pLine == m_linesTail )
        return LineListAppend(str);

    wxFileConfigLineList *pNewLine = new wxFileConfigLineList(str);
    if ( pLine == NULL )
    {
        // prepend to the list
        pNewLine->SetNext(m_linesHead);
        m_linesHead->SetPrev(pNewLine);
        m_linesHead = pNewLine;
    }
    else
    {
        // insert before pLine
        wxFileConfigLineList *pNext = pLine->Next();
        pNewLine->SetNext(pNext);
        pNewLine->SetPrev(pLine);
        pNext->SetPrev(pNewLine);
        pLine->SetNext(pNewLine);
    }

    wxLogTrace( _T("wxFileConfig"),
                _T("        head: %s"),
                ((m_linesHead) ? m_linesHead->Text().c_str() : wxEmptyString) );
    wxLogTrace( _T("wxFileConfig"),
                _T("        tail: %s"),
                ((m_linesTail) ? m_linesTail->Text().c_str() : wxEmptyString) );

    return pNewLine;
}

void wxFileConfig::LineListRemove(wxFileConfigLineList *pLine)
{
    wxLogTrace( _T("wxFileConfig"),
                _T("    ** Removing Line '%s'"),
                pLine->Text().c_str() );
    wxLogTrace( _T("wxFileConfig"),
                _T("        head: %s"),
                ((m_linesHead) ? m_linesHead->Text().c_str() : wxEmptyString) );
    wxLogTrace( _T("wxFileConfig"),
                _T("        tail: %s"),
                ((m_linesTail) ? m_linesTail->Text().c_str() : wxEmptyString) );

    wxFileConfigLineList    *pPrev = pLine->Prev(),
                            *pNext = pLine->Next();

        // first entry?

    if ( pPrev == NULL )
        m_linesHead = pNext;
    else
        pPrev->SetNext(pNext);

        // last entry?

    if ( pNext == NULL )
        m_linesTail = pPrev;
    else
        pNext->SetPrev(pPrev);

    wxLogTrace( _T("wxFileConfig"),
                _T("        head: %s"),
                ((m_linesHead) ? m_linesHead->Text().c_str() : wxEmptyString) );
    wxLogTrace( _T("wxFileConfig"),
                _T("        tail: %s"),
                ((m_linesTail) ? m_linesTail->Text().c_str() : wxEmptyString) );

    delete pLine;
}

bool wxFileConfig::LineListIsEmpty()
{
    return m_linesHead == NULL;
}

// ============================================================================
// wxFileConfig::wxFileConfigGroup
// ============================================================================

// ----------------------------------------------------------------------------
// ctor/dtor
// ----------------------------------------------------------------------------

// ctor
wxFileConfigGroup::wxFileConfigGroup(wxFileConfigGroup *pParent,
                                       const wxString& strName,
                                       wxFileConfig *pConfig)
                         : m_aEntries(CompareEntries),
                           m_aSubgroups(CompareGroups),
                           m_strName(strName)
{
  m_pConfig = pConfig;
  m_pParent = pParent;
  m_bDirty  = FALSE;
  m_pLine   = NULL;

  m_pLastEntry = NULL;
  m_pLastGroup = NULL;
}

// dtor deletes all children
wxFileConfigGroup::~wxFileConfigGroup()
{
  // entries
  size_t n, nCount = m_aEntries.Count();
  for ( n = 0; n < nCount; n++ )
    delete m_aEntries[n];

  // subgroups
  nCount = m_aSubgroups.Count();
  for ( n = 0; n < nCount; n++ )
    delete m_aSubgroups[n];
}

// ----------------------------------------------------------------------------
// line
// ----------------------------------------------------------------------------

void wxFileConfigGroup::SetLine(wxFileConfigLineList *pLine)
{
    wxASSERT( m_pLine == 0 ); // shouldn't be called twice
    m_pLine = pLine;
}

/*
  This is a bit complicated, so let me explain it in details. All lines that
  were read from the local file (the only one we will ever modify) are stored
  in a (doubly) linked list. Our problem is to know at which position in this
  list should we insert the new entries/subgroups. To solve it we keep three
  variables for each group: m_pLine, m_pLastEntry and m_pLastGroup.

  m_pLine points to the line containing "[group_name]"
  m_pLastEntry points to the last entry of this group in the local file.
  m_pLastGroup                   subgroup

  Initially, they're NULL all three. When the group (an entry/subgroup) is read
  from the local file, the corresponding variable is set. However, if the group
  was read from the global file and then modified or created by the application
  these variables are still NULL and we need to create the corresponding lines.
  See the following functions (and comments preceding them) for the details of
  how we do it.

  Also, when our last entry/group are deleted we need to find the new last
  element - the code in DeleteEntry/Subgroup does this by backtracking the list
  of lines until it either founds an entry/subgroup (and this is the new last
  element) or the m_pLine of the group, in which case there are no more entries
  (or subgroups) left and m_pLast<element> becomes NULL.

  NB: This last problem could be avoided for entries if we added new entries
      immediately after m_pLine, but in this case the entries would appear
      backwards in the config file (OTOH, it's not that important) and as we
      would still need to do it for the subgroups the code wouldn't have been
      significantly less complicated.
*/

// Return the line which contains "[our name]". If we're still not in the list,
// add our line to it immediately after the last line of our parent group if we
// have it or in the very beginning if we're the root group.
wxFileConfigLineList *wxFileConfigGroup::GetGroupLine()
{
    wxLogTrace( _T("wxFileConfig"),
                _T("  GetGroupLine() for Group '%s'"),
                Name().c_str() );

    if ( m_pLine == 0 )
    {
        wxLogTrace( _T("wxFileConfig"),
                    _T("    Getting Line item pointer") );

        wxFileConfigGroup   *pParent = Parent();

            // this group wasn't present in local config file, add it now

        if ( pParent != 0 )
        {
            wxLogTrace( _T("wxFileConfig"),
                        _T("    checking parent '%s'"),
                        pParent->Name().c_str() );

            wxString    strFullName;

            strFullName << wxT("[")         // +1: no '/'
                        << FilterOutEntryName(GetFullName().c_str() + 1)
                        << wxT("]");
            m_pLine = m_pConfig->LineListInsert(strFullName,
                                                pParent->GetLastGroupLine());
            pParent->SetLastGroup(this);  // we're surely after all the others
        }
        else
        {
            // we return NULL, so that LineListInsert() will insert us in the
            // very beginning
        }
    }

    return m_pLine;
}

// Return the last line belonging to the subgroups of this group (after which
// we can add a new subgroup), if we don't have any subgroups or entries our
// last line is the group line (m_pLine) itself.
wxFileConfigLineList *wxFileConfigGroup::GetLastGroupLine()
{
        // if we have any subgroups, our last line is
        // the last line of the last subgroup

    if ( m_pLastGroup != 0 )
    {
        wxFileConfigLineList *pLine = m_pLastGroup->GetLastGroupLine();

        wxASSERT( pLine != 0 );  // last group must have !NULL associated line
        return pLine;
    }

        // no subgroups, so the last line is the line of thelast entry (if any)

    return GetLastEntryLine();
}

// return the last line belonging to the entries of this group (after which
// we can add a new entry), if we don't have any entries we will add the new
// one immediately after the group line itself.
wxFileConfigLineList *wxFileConfigGroup::GetLastEntryLine()
{
    wxLogTrace( _T("wxFileConfig"),
                _T("  GetLastEntryLine() for Group '%s'"),
                Name().c_str() );

    if ( m_pLastEntry != 0 )
    {
        wxFileConfigLineList    *pLine = m_pLastEntry->GetLine();

        wxASSERT( pLine != 0 );  // last entry must have !NULL associated line
        return pLine;
    }

        // no entries: insert after the group header

    return GetGroupLine();
}

// ----------------------------------------------------------------------------
// group name
// ----------------------------------------------------------------------------

void wxFileConfigGroup::Rename(const wxString& newName)
{
    m_strName = newName;

    wxFileConfigLineList *line = GetGroupLine();
    wxString strFullName;
    strFullName << wxT("[") << (GetFullName().c_str() + 1) << wxT("]"); // +1: no '/'
    line->SetText(strFullName);

    SetDirty();
}

wxString wxFileConfigGroup::GetFullName() const
{
  if ( Parent() )
    return Parent()->GetFullName() + wxCONFIG_PATH_SEPARATOR + Name();
  else
    return wxT("");
}

// ----------------------------------------------------------------------------
// find an item
// ----------------------------------------------------------------------------

// use binary search because the array is sorted
wxFileConfigEntry *
wxFileConfigGroup::FindEntry(const wxChar *szName) const
{
  size_t i,
       lo = 0,
       hi = m_aEntries.Count();
  int res;
  wxFileConfigEntry *pEntry;

  while ( lo < hi ) {
    i = (lo + hi)/2;
    pEntry = m_aEntries[i];

    #if wxCONFIG_CASE_SENSITIVE
      res = wxStrcmp(pEntry->Name(), szName);
    #else
      res = wxStricmp(pEntry->Name(), szName);
    #endif

    if ( res > 0 )
      hi = i;
    else if ( res < 0 )
      lo = i + 1;
    else
      return pEntry;
  }

  return NULL;
}

wxFileConfigGroup *
wxFileConfigGroup::FindSubgroup(const wxChar *szName) const
{
  size_t i,
       lo = 0,
       hi = m_aSubgroups.Count();
  int res;
  wxFileConfigGroup *pGroup;

  while ( lo < hi ) {
    i = (lo + hi)/2;
    pGroup = m_aSubgroups[i];

    #if wxCONFIG_CASE_SENSITIVE
      res = wxStrcmp(pGroup->Name(), szName);
    #else
      res = wxStricmp(pGroup->Name(), szName);
    #endif

    if ( res > 0 )
      hi = i;
    else if ( res < 0 )
      lo = i + 1;
    else
      return pGroup;
  }

  return NULL;
}

// ----------------------------------------------------------------------------
// create a new item
// ----------------------------------------------------------------------------

// create a new entry and add it to the current group
wxFileConfigEntry *wxFileConfigGroup::AddEntry(const wxString& strName, int nLine)
{
    wxASSERT( FindEntry(strName) == 0 );

    wxFileConfigEntry   *pEntry = new wxFileConfigEntry(this, strName, nLine);

    m_aEntries.Add(pEntry);
    return pEntry;
}

// create a new group and add it to the current group
wxFileConfigGroup *wxFileConfigGroup::AddSubgroup(const wxString& strName)
{
    wxASSERT( FindSubgroup(strName) == 0 );

    wxFileConfigGroup   *pGroup = new wxFileConfigGroup(this, strName, m_pConfig);

    m_aSubgroups.Add(pGroup);
    return pGroup;
}

// ----------------------------------------------------------------------------
// delete an item
// ----------------------------------------------------------------------------

/*
  The delete operations are _very_ slow if we delete the last item of this
  group (see comments before GetXXXLineXXX functions for more details),
  so it's much better to start with the first entry/group if we want to
  delete several of them.
 */

bool wxFileConfigGroup::DeleteSubgroupByName(const wxChar *szName)
{
  return DeleteSubgroup(FindSubgroup(szName));
}

// Delete the subgroup and remove all references to it from
// other data structures.
bool wxFileConfigGroup::DeleteSubgroup(wxFileConfigGroup *pGroup)
{
    wxLogTrace( _T("wxFileConfig"),
                _T("Deleting group '%s' from '%s'"),
                pGroup->Name().c_str(),
                Name().c_str() );

    wxLogTrace( _T("wxFileConfig"),
                _T("  (m_pLine) = prev: %p, this %p, next %p"),
                ((m_pLine) ? m_pLine->Prev() : 0),
                m_pLine,
                ((m_pLine) ? m_pLine->Next() : 0) );
    wxLogTrace( _T("wxFileConfig"),
                _T("  text: '%s'"),
                ((m_pLine) ? m_pLine->Text().c_str() : wxEmptyString) );

    wxCHECK_MSG( pGroup != 0, FALSE, _T("deleting non existing group?") );

        // delete all entries

    size_t  nCount = pGroup->m_aEntries.Count();

    wxLogTrace(_T("wxFileConfig"),
               _T("Removing %lu Entries"),
               (unsigned long)nCount );

    for ( size_t nEntry = 0; nEntry < nCount; nEntry++ )
    {
        wxFileConfigLineList    *pLine = pGroup->m_aEntries[nEntry]->GetLine();

        if ( pLine != 0 )
        {
            wxLogTrace( _T("wxFileConfig"),
                        _T("    '%s'"),
                        pLine->Text().c_str() );
            m_pConfig->LineListRemove(pLine);
        }
    }

        // and subgroups of this subgroup

    nCount = pGroup->m_aSubgroups.Count();

    wxLogTrace( _T("wxFileConfig"),
                _T("Removing %lu SubGroups"),
                (unsigned long)nCount );

    for ( size_t nGroup = 0; nGroup < nCount; nGroup++ )
    {
        pGroup->DeleteSubgroup(pGroup->m_aSubgroups[0]);
    }

        // finally the group itself

    wxFileConfigLineList    *pLine = pGroup->m_pLine;

    if ( pLine != 0 )
    {
        wxLogTrace( _T("wxFileConfig"),
                    _T("  Removing line entry for Group '%s' : '%s'"),
                    pGroup->Name().c_str(),
                    pLine->Text().c_str() );
        wxLogTrace( _T("wxFileConfig"),
                    _T("  Removing from Group '%s' : '%s'"),
                    Name().c_str(),
                    ((m_pLine) ? m_pLine->Text().c_str() : wxEmptyString) );

            // notice that we may do this test inside the previous "if"
            // because the last entry's line is surely !NULL

        if ( pGroup == m_pLastGroup )
        {
            wxLogTrace( _T("wxFileConfig"),
                        _T("  ------- Removing last group -------") );

                // our last entry is being deleted, so find the last one which stays.
                // go back until we find a subgroup or reach the group's line, unless
                // we are the root group, which we'll notice shortly.

            wxFileConfigGroup       *pNewLast = 0;
            size_t                   nSubgroups = m_aSubgroups.Count();
            wxFileConfigLineList    *pl;

            for ( pl = pLine->Prev(); pl != m_pLine; pl = pl->Prev() )
            {
                    // is it our subgroup?

                for ( size_t n = 0; (pNewLast == 0) && (n < nSubgroups); n++ )
                {
                    // do _not_ call GetGroupLine! we don't want to add it to the local
                    // file if it's not already there

                    if ( m_aSubgroups[n]->m_pLine == m_pLine )
                        pNewLast = m_aSubgroups[n];
                }

                if ( pNewLast != 0 ) // found?
                    break;
            }

            if ( pl == m_pLine || m_pParent == 0 )
            {
                wxLogTrace( _T("wxFileConfig"),
                            _T("  ------- No previous group found -------") );
                
                wxASSERT_MSG( !pNewLast || m_pLine == 0,
                              _T("how comes it has the same line as we?") );

                    // we've reached the group line without finding any subgroups,
                    // or realised we removed the last group from the root.

                m_pLastGroup = 0;
            }
            else
            {
                wxLogTrace( _T("wxFileConfig"),
                            _T("  ------- Last Group set to '%s' -------"),
                            pNewLast->Name().c_str() );

                m_pLastGroup = pNewLast;
            }
        }

        m_pConfig->LineListRemove(pLine);
    }
    else
    {
        wxLogTrace( _T("wxFileConfig"),
                    _T("  No line entry for Group '%s'?"),
                    pGroup->Name().c_str() );
    }

    SetDirty();

    m_aSubgroups.Remove(pGroup);
    delete pGroup;

    return TRUE;
}

bool wxFileConfigGroup::DeleteEntry(const wxChar *szName)
{
  wxFileConfigEntry *pEntry = FindEntry(szName);
  wxCHECK( pEntry != NULL, FALSE );  // deleting non existing item?

  wxFileConfigLineList *pLine = pEntry->GetLine();
  if ( pLine != NULL ) {
    // notice that we may do this test inside the previous "if" because the
    // last entry's line is surely !NULL
    if ( pEntry == m_pLastEntry ) {
      // our last entry is being deleted - find the last one which stays
      wxASSERT( m_pLine != NULL );  // if we have an entry with !NULL pLine...

      // go back until we find another entry or reach the group's line
      wxFileConfigEntry *pNewLast = NULL;
      size_t n, nEntries = m_aEntries.Count();
      wxFileConfigLineList *pl;
      for ( pl = pLine->Prev(); pl != m_pLine; pl = pl->Prev() ) {
        // is it our subgroup?
        for ( n = 0; (pNewLast == NULL) && (n < nEntries); n++ ) {
          if ( m_aEntries[n]->GetLine() == m_pLine )
            pNewLast = m_aEntries[n];
        }

        if ( pNewLast != NULL ) // found?
          break;
      }

      if ( pl == m_pLine ) {
        wxASSERT( !pNewLast );  // how comes it has the same line as we?

        // we've reached the group line without finding any subgroups
        m_pLastEntry = NULL;
      }
      else
        m_pLastEntry = pNewLast;
    }

    m_pConfig->LineListRemove(pLine);
  }

  // we must be written back for the changes to be saved
  SetDirty();

  m_aEntries.Remove(pEntry);
  delete pEntry;

  return TRUE;
}

// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------
void wxFileConfigGroup::SetDirty()
{
  m_bDirty = TRUE;
  if ( Parent() != NULL )             // propagate upwards
    Parent()->SetDirty();
}

// ============================================================================
// wxFileConfig::wxFileConfigEntry
// ============================================================================

// ----------------------------------------------------------------------------
// ctor
// ----------------------------------------------------------------------------
wxFileConfigEntry::wxFileConfigEntry(wxFileConfigGroup *pParent,
                                       const wxString& strName,
                                       int nLine)
                         : m_strName(strName)
{
  wxASSERT( !strName.IsEmpty() );

  m_pParent = pParent;
  m_nLine   = nLine;
  m_pLine   = NULL;

  m_bDirty =
  m_bHasValue = FALSE;

  m_bImmutable = strName[0] == wxCONFIG_IMMUTABLE_PREFIX;
  if ( m_bImmutable )
    m_strName.erase(0, 1);  // remove first character
}

// ----------------------------------------------------------------------------
// set value
// ----------------------------------------------------------------------------

void wxFileConfigEntry::SetLine(wxFileConfigLineList *pLine)
{
  if ( m_pLine != NULL ) {
    wxLogWarning(_("entry '%s' appears more than once in group '%s'"),
                 Name().c_str(), m_pParent->GetFullName().c_str());
  }

  m_pLine = pLine;
  Group()->SetLastEntry(this);
}

// second parameter is FALSE if we read the value from file and prevents the
// entry from being marked as 'dirty'
void wxFileConfigEntry::SetValue(const wxString& strValue, bool bUser)
{
    if ( bUser && IsImmutable() )
    {
        wxLogWarning( _("attempt to change immutable key '%s' ignored."),
                      Name().c_str());
        return;
    }

        // do nothing if it's the same value: but don't test for it
        // if m_bHasValue hadn't been set yet or we'd never write
        // empty values to the file

    if ( m_bHasValue && strValue == m_strValue )
        return;

    m_bHasValue = TRUE;
    m_strValue = strValue;

    if ( bUser )
    {
        wxString    strValFiltered;

        if ( Group()->Config()->GetStyle() & wxCONFIG_USE_NO_ESCAPE_CHARACTERS )
        {
            strValFiltered = strValue;
        }
        else {
            strValFiltered = FilterOutValue(strValue);
        }

        wxString    strLine;
        strLine << FilterOutEntryName(m_strName) << wxT('=') << strValFiltered;

        if ( m_pLine != 0 )
        {
            // entry was read from the local config file, just modify the line
            m_pLine->SetText(strLine);
        }
        else {
            // add a new line to the file
            wxASSERT( m_nLine == wxNOT_FOUND );   // consistency check

            m_pLine = Group()->Config()->LineListInsert(strLine,
                                                        Group()->GetLastEntryLine());
            Group()->SetLastEntry(this);
        }

        SetDirty();
    }
}

void wxFileConfigEntry::SetDirty()
{
  m_bDirty = TRUE;
  Group()->SetDirty();
}

// ============================================================================
// global functions
// ============================================================================

// ----------------------------------------------------------------------------
// compare functions for array sorting
// ----------------------------------------------------------------------------

int CompareEntries(wxFileConfigEntry *p1, wxFileConfigEntry *p2)
{
  #if wxCONFIG_CASE_SENSITIVE
    return wxStrcmp(p1->Name(), p2->Name());
  #else
    return wxStricmp(p1->Name(), p2->Name());
  #endif
}

int CompareGroups(wxFileConfigGroup *p1, wxFileConfigGroup *p2)
{
  #if wxCONFIG_CASE_SENSITIVE
    return wxStrcmp(p1->Name(), p2->Name());
  #else
    return wxStricmp(p1->Name(), p2->Name());
  #endif
}

// ----------------------------------------------------------------------------
// filter functions
// ----------------------------------------------------------------------------

// undo FilterOutValue
static wxString FilterInValue(const wxString& str)
{
  wxString strResult;
  strResult.Alloc(str.Len());

  bool bQuoted = !str.IsEmpty() && str[0] == '"';

  for ( size_t n = bQuoted ? 1 : 0; n < str.Len(); n++ ) {
    if ( str[n] == wxT('\\') ) {
      switch ( str[++n] ) {
        case wxT('n'):
          strResult += wxT('\n');
          break;

        case wxT('r'):
          strResult += wxT('\r');
          break;

        case wxT('t'):
          strResult += wxT('\t');
          break;

        case wxT('\\'):
          strResult += wxT('\\');
          break;

        case wxT('"'):
          strResult += wxT('"');
          break;
      }
    }
    else {
      if ( str[n] != wxT('"') || !bQuoted )
        strResult += str[n];
      else if ( n != str.Len() - 1 ) {
        wxLogWarning(_("unexpected \" at position %d in '%s'."),
                     n, str.c_str());
      }
      //else: it's the last quote of a quoted string, ok
    }
  }

  return strResult;
}

// quote the string before writing it to file
static wxString FilterOutValue(const wxString& str)
{
   if ( !str )
      return str;

  wxString strResult;
  strResult.Alloc(str.Len());

  // quoting is necessary to preserve spaces in the beginning of the string
  bool bQuote = wxIsspace(str[0]) || str[0] == wxT('"');

  if ( bQuote )
    strResult += wxT('"');

  wxChar c;
  for ( size_t n = 0; n < str.Len(); n++ ) {
    switch ( str[n] ) {
      case wxT('\n'):
        c = wxT('n');
        break;

      case wxT('\r'):
        c = wxT('r');
        break;

      case wxT('\t'):
        c = wxT('t');
        break;

      case wxT('\\'):
        c = wxT('\\');
        break;

      case wxT('"'):
        if ( bQuote ) {
          c = wxT('"');
          break;
        }
        //else: fall through

      default:
        strResult += str[n];
        continue;   // nothing special to do
    }

    // we get here only for special characters
    strResult << wxT('\\') << c;
  }

  if ( bQuote )
    strResult += wxT('"');

  return strResult;
}

// undo FilterOutEntryName
static wxString FilterInEntryName(const wxString& str)
{
  wxString strResult;
  strResult.Alloc(str.Len());

  for ( const wxChar *pc = str.c_str(); *pc != '\0'; pc++ ) {
    if ( *pc == wxT('\\') )
      pc++;

    strResult += *pc;
  }

  return strResult;
}

// sanitize entry or group name: insert '\\' before any special characters
static wxString FilterOutEntryName(const wxString& str)
{
  wxString strResult;
  strResult.Alloc(str.Len());

  for ( const wxChar *pc = str.c_str(); *pc != wxT('\0'); pc++ ) {
    wxChar c = *pc;

    // we explicitly allow some of "safe" chars and 8bit ASCII characters
    // which will probably never have special meaning
    // NB: note that wxCONFIG_IMMUTABLE_PREFIX and wxCONFIG_PATH_SEPARATOR
    //     should *not* be quoted
    if ( !wxIsalnum(c) && !wxStrchr(wxT("@_/-!.*%"), c) && ((c & 0x80) == 0) )
      strResult += wxT('\\');

    strResult += c;
  }

  return strResult;
}

// we can't put ?: in the ctor initializer list because it confuses some
// broken compilers (Borland C++)
static wxString GetAppName(const wxString& appName)
{
    if ( !appName && wxTheApp )
        return wxTheApp->GetAppName();
    else
        return appName;
}

#endif // wxUSE_CONFIG


// vi:sts=4:sw=4:et
