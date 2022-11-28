/*-*- c++ -*-********************************************************
 * helphtml.h - base class for html based help controllers          *
 *                                                                  *
 * (C) 1999 by Karsten Ball�der (Ballueder@usa.net)                 *
 *                                                                  *
 * $Id: helphtml.h,v 1.12.2.1 2002/10/29 21:47:26 RR Exp $
 *******************************************************************/

#ifndef __WX_HELPHTML_H_
#define __WX_HELPHTML_H_

#if wxUSE_HELP

#if defined(__GNUG__) && !defined(__APPLE__)
#   pragma interface "helphtml.h"
#endif

#include "wx/helpbase.h"
#include "wx/frame.h"

/// Path separator.
#ifdef __WXMSW__
#define WXEXTHELP_SEPARATOR _T('\\')
#elif defined(__WXMAC__)
#define WXEXTHELP_SEPARATOR _T(':')
#else
#define WXEXTHELP_SEPARATOR _T('/')
#endif

class WXDLLEXPORT wxExtHelpMapList;


/**
   This class is the base class for all html help implementations.
   It requires the name of a directory containing the documentation
   and a file mapping numerical Section numbers to relative URLS.

   The map file contains two or three fields per line:
   numeric_id  relative_URL  [; comment/documentation]

   The numeric_id is the id used to look up the entry in
   DisplaySection()/DisplayBlock(). The relative_URL is a filename of
   an html file, relative to the help directory. The optional
   comment/documentation field (after a ';') is used for keyword
   searches, so some meaningful text here does not hurt.
   If the documentation itself contains a ';', only the part before
   that will be displayed in the listbox, but all of it used for search.

   Lines starting with ';' will be ignored.
*/

class WXDLLEXPORT wxHTMLHelpControllerBase : public wxHelpControllerBase
{
DECLARE_ABSTRACT_CLASS(wxHTMLHelpControllerBase)
   public:
   wxHTMLHelpControllerBase(void);
   virtual ~wxHTMLHelpControllerBase(void);

   /** This must be called to tell the controller where to find the
       documentation.
       If a locale is set, look in file/localename, i.e.
       If passed "/usr/local/myapp/help" and the current wxLocale is
       set to be "de", then look in "/usr/local/myapp/help/de/"
       first and fall back to "/usr/local/myapp/help" if that
       doesn't exist.

       @param file - NOT a filename, but a directory name.
       @return true on success
   */
   virtual bool Initialize(const wxString& dir, int WXUNUSED(server))
      { return Initialize(dir); }

   /** This must be called to tell the controller where to find the
       documentation.
       If a locale is set, look in file/localename, i.e.
       If passed "/usr/local/myapp/help" and the current wxLocale is
       set to be "de", then look in "/usr/local/myapp/help/de/"
       first and fall back to "/usr/local/myapp/help" if that
       doesn't exist.
       @param dir - directory name where to fine the help files
       @return true on success
   */
   virtual bool Initialize(const wxString& dir);

   /** If file is "", reloads file given in Initialize.
       @file Name of help directory.
       @return true on success
   */
   virtual bool LoadFile(const wxString& file = wxT(""));

   /** Display list of all help entries.
       @return true on success
   */
   virtual bool DisplayContents(void);
   /** Display help for id sectionNo.
       @return true on success
   */
   virtual bool DisplaySection(int sectionNo);
   /** Display help for id sectionNo -- identical with DisplaySection().
       @return true on success
   */
   virtual bool DisplaySection(const wxString& section);
   /** Display help for URL (using DisplayHelp) or keyword (using KeywordSearch)
       @return true on success
   */
   virtual bool DisplayBlock(long blockNo);
   /** Search comment/documentation fields in map file and present a
       list to chose from.
       @key k string to search for, empty string will list all entries
       @return true on success
   */
   virtual bool KeywordSearch(const wxString& k);

   /// does nothing
   virtual bool Quit(void);
   /// does nothing
   virtual void OnQuit(void);

   /// Call the browser using a relative URL.
   virtual bool DisplayHelp(const wxString &) = 0;

   /// Allows one to override the default settings for the help frame.
   virtual void SetFrameParameters(const wxString& WXUNUSED(title),
                                   const wxSize& WXUNUSED(size),
                                   const wxPoint& WXUNUSED(pos) = wxDefaultPosition,
                                   bool WXUNUSED(newFrameEachTime) = FALSE)
      {
         // does nothing by default
      }
   /// Obtains the latest settings used by the help frame and the help 
   /// frame.
   virtual wxFrame *GetFrameParameters(wxSize *WXUNUSED(size) = NULL,
                                   wxPoint *WXUNUSED(pos) = NULL,
                                   bool *WXUNUSED(newFrameEachTime) = NULL)
      {
         return (wxFrame*) NULL;// does nothing by default
      }

 protected:
   /// Filename of currently active map file.
   wxString         m_MapFile;
   /// How many entries do we have in the map file?
   int              m_NumOfEntries;
   /// A list containing all id,url,documentation triples.
   wxList          *m_MapList;
   /// Deletes the list and all objects.
   void DeleteList(void);
};

#endif // wxUSE_HELP

#endif // __WX_HELPHTML_H_
