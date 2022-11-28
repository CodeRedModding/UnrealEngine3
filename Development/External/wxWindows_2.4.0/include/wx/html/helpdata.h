/////////////////////////////////////////////////////////////////////////////
// Name:        helpdata.h
// Purpose:     wxHtmlHelpData
// Notes:       Based on htmlhelp.cpp, implementing a monolithic
//              HTML Help controller class,  by Vaclav Slavik
// Author:      Harm van der Heijden and Vaclav Slavik
// RCS-ID:      $Id: helpdata.h,v 1.21.2.1 2002/11/11 00:00:11 VS Exp $
// Copyright:   (c) Harm van der Heijden and Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_HELPDATA_H_
#define _WX_HELPDATA_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "helpdata.h"
#endif

#include "wx/defs.h"

#if wxUSE_HTML

#include "wx/object.h"
#include "wx/string.h"
#include "wx/filesys.h"
#include "wx/dynarray.h"
#include "wx/font.h"

class WXDLLEXPORT wxHtmlHelpData;

//--------------------------------------------------------------------------------
// helper classes & structs
//--------------------------------------------------------------------------------

class WXDLLEXPORT wxHtmlBookRecord
{
public:
    wxHtmlBookRecord(const wxString& bookfile, const wxString& basepath, 
                     const wxString& title, const wxString& start)
    {
        m_BookFile = bookfile;
        m_BasePath = basepath;
        m_Title = title;
        m_Start = start;
        // for debugging, give the contents index obvious default values
        m_ContentsStart = m_ContentsEnd = -1;
    }
    wxString GetBookFile() const { return m_BookFile; }
    wxString GetTitle() const { return m_Title; }
    wxString GetStart() const { return m_Start; }
    wxString GetBasePath() const { return m_BasePath; }
    /* SetContentsRange: store in the bookrecord where in the index/contents lists the
     * book's records are stored. This to facilitate searching in a specific book.
     * This code will have to be revised when loading/removing books becomes dynamic.
     * (as opposed to appending only)
     * Note that storing index range is pointless, because the index is alphab. sorted. */
    void SetContentsRange(int start, int end) { m_ContentsStart = start; m_ContentsEnd = end; }
    int GetContentsStart() const { return m_ContentsStart; }
    int GetContentsEnd() const { return m_ContentsEnd; }

    void SetTitle(const wxString& title) { m_Title = title; }
    void SetBasePath(const wxString& path) { m_BasePath = path; }
    void SetStart(const wxString& start) { m_Start = start; }

    // returns full filename of page (which is part of the book), 
    // i.e. with book's basePath prepended. If page is already absolute 
    // path, basePath is _not_ prepended.
    wxString GetFullPath(const wxString &page) const;

protected:
    wxString m_BookFile;
    wxString m_BasePath;
    wxString m_Title;
    wxString m_Start;
    int m_ContentsStart;
    int m_ContentsEnd;
};


WX_DECLARE_EXPORTED_OBJARRAY(wxHtmlBookRecord, wxHtmlBookRecArray);


struct wxHtmlContentsItem
{
    short int m_Level;
    int m_ID;
    wxChar* m_Name;
    wxChar* m_Page;
    wxHtmlBookRecord *m_Book;
    
    // returns full filename of m_Page, i.e. with book's basePath prepended
    wxString GetFullPath() const { return m_Book->GetFullPath(m_Page); }
};

//------------------------------------------------------------------------------
// wxHtmlSearchEngine
//                  This class takes input streams and scans them for occurence
//                  of keyword(s)
//------------------------------------------------------------------------------

class WXDLLEXPORT wxHtmlSearchEngine : public wxObject
{
public:
    wxHtmlSearchEngine() : wxObject() {m_Keyword = NULL; }
    ~wxHtmlSearchEngine() {if (m_Keyword) delete[] m_Keyword; }

    // Sets the keyword we will be searching for
    virtual void LookFor(const wxString& keyword, bool case_sensitive, bool whole_words_only);

    // Scans the stream for the keyword.
    // Returns TRUE if the stream contains keyword, fALSE otherwise
    virtual bool Scan(const wxFSFile& file);

private:
    wxChar *m_Keyword;
    bool m_CaseSensitive;
    bool m_WholeWords;
};


// State information of a search action. I'd have prefered to make this a nested
// class inside wxHtmlHelpData, but that's against coding standards :-(
// Never construct this class yourself, obtain a copy from
// wxHtmlHelpData::PrepareKeywordSearch(const wxString& key)
class WXDLLEXPORT wxHtmlSearchStatus
{
public:
    // constructor; supply wxHtmlHelpData ptr, the keyword and (optionally) the
    // title of the book to search. By default, all books are searched.
    wxHtmlSearchStatus(wxHtmlHelpData* base, const wxString& keyword,
                       bool case_sensitive, bool whole_words_only,
                       const wxString& book = wxEmptyString);
    bool Search();  // do the next iteration
    bool IsActive() { return m_Active; }
    int GetCurIndex() { return m_CurIndex; }
    int GetMaxIndex() { return m_MaxIndex; }
    const wxString& GetName() { return m_Name; }
    wxHtmlContentsItem* GetContentsItem() { return m_ContentsItem; }

private:
    wxHtmlHelpData* m_Data;
    wxHtmlSearchEngine m_Engine;
    wxString m_Keyword, m_Name;
    wxChar *m_LastPage;
    wxHtmlContentsItem* m_ContentsItem;
    bool m_Active;   // search is not finished
    int m_CurIndex;  // where we are now
    int m_MaxIndex;  // number of files we search
    // For progress bar: 100*curindex/maxindex = % complete
};

class WXDLLEXPORT wxHtmlHelpData : public wxObject
{
    DECLARE_DYNAMIC_CLASS(wxHtmlHelpData)
    friend class wxHtmlSearchStatus;

public:
    wxHtmlHelpData();
    ~wxHtmlHelpData();

    // Sets directory where temporary files are stored.
    // These temp files are index & contents file in binary (much faster to read)
    // form. These files are NOT deleted on program's exit.
    void SetTempDir(const wxString& path);

    // Adds new book. 'book' is location of .htb file (stands for "html book").
    // See documentation for details on its format.
    // Returns success.
    bool AddBook(const wxString& book);
    bool AddBookParam(const wxFSFile& bookfile,
                      wxFontEncoding encoding,
                      const wxString& title, const wxString& contfile,
                      const wxString& indexfile = wxEmptyString,
                      const wxString& deftopic = wxEmptyString,
                      const wxString& path = wxEmptyString);

    // Some accessing stuff:

    // returns URL of page on basis of (file)name
    wxString FindPageByName(const wxString& page);
    // returns URL of page on basis of MS id
    wxString FindPageById(int id);

    const wxHtmlBookRecArray& GetBookRecArray() { return m_BookRecords; }
    wxHtmlContentsItem* GetContents() { return m_Contents; }
    int GetContentsCnt() { return m_ContentsCnt; }
    wxHtmlContentsItem* GetIndex() { return m_Index; }
    int GetIndexCnt() { return m_IndexCnt; }

protected:
    wxString m_TempPath;

    wxHtmlBookRecArray m_BookRecords;
    // each book has one record in this array:
    wxHtmlContentsItem* m_Contents;
    int m_ContentsCnt;   
    wxHtmlContentsItem* m_Index; // list of all available books and pages.
    int m_IndexCnt;  // list of index items

protected:
    // Imports .hhp files (MS HTML Help Workshop)
    bool LoadMSProject(wxHtmlBookRecord *book, wxFileSystem& fsys,
                       const wxString& indexfile, const wxString& contentsfile);
    // Reads binary book
    bool LoadCachedBook(wxHtmlBookRecord *book, wxInputStream *f);
    // Writes binary book
    bool SaveCachedBook(wxHtmlBookRecord *book, wxOutputStream *f);
};

#endif

#endif
