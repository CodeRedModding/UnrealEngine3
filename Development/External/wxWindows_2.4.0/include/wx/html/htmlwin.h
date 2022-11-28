/////////////////////////////////////////////////////////////////////////////
// Name:        htmlwin.h
// Purpose:     wxHtmlWindow class for parsing & displaying HTML
// Author:      Vaclav Slavik
// RCS-ID:      $Id: htmlwin.h,v 1.32 2002/08/31 11:29:12 GD Exp $
// Copyright:   (c) 1999 Vaclav Slavik
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////


#ifndef _WX_HTMLWIN_H_
#define _WX_HTMLWIN_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "htmlwin.h"
#endif

#include "wx/defs.h"
#if wxUSE_HTML

#include "wx/window.h"
#include "wx/scrolwin.h"
#include "wx/config.h"
#include "wx/treectrl.h"
#include "wx/html/winpars.h"
#include "wx/html/htmlcell.h"
#include "wx/filesys.h"
#include "wx/html/htmlfilt.h"

class wxHtmlProcessor;
class wxHtmlWinModule;
class wxHtmlHistoryArray;
class wxHtmlProcessorList;


// wxHtmlWindow flags:
#define wxHW_SCROLLBAR_NEVER    0x0002
#define wxHW_SCROLLBAR_AUTO     0x0004

// enums for wxHtmlWindow::OnOpeningURL
enum wxHtmlOpeningStatus
{
    wxHTML_OPEN,
    wxHTML_BLOCK,
    wxHTML_REDIRECT
};

//--------------------------------------------------------------------------------
// wxHtmlWindow
//                  (This is probably the only class you will directly use.)
//                  Purpose of this class is to display HTML page (either local
//                  file or downloaded via HTTP protocol) in a window. Width
//                  of window is constant - given in constructor - virtual height
//                  is changed dynamicly depending on page size.
//                  Once the window is created you can set it's content by calling
//                  SetPage(text) or LoadPage(filename).
//--------------------------------------------------------------------------------

class WXDLLEXPORT wxHtmlWindow : public wxScrolledWindow
{
    DECLARE_DYNAMIC_CLASS(wxHtmlWindow)
    friend class wxHtmlWinModule;

public:
    wxHtmlWindow() { Init(); }
    wxHtmlWindow(wxWindow *parent, wxWindowID id = -1,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = wxHW_SCROLLBAR_AUTO,
                 const wxString& name = wxT("htmlWindow"))
    {
        Init();
        Create(parent, id, pos, size, style, name);
    }
    ~wxHtmlWindow();

    bool Create(wxWindow *parent, wxWindowID id = -1,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxHW_SCROLLBAR_AUTO,
                const wxString& name = wxT("htmlWindow"));

    // Set HTML page and display it. !! source is HTML document itself,
    // it is NOT address/filename of HTML document. If you want to
    // specify document location, use LoadPage() istead
    // Return value : FALSE if an error occured, TRUE otherwise
    bool SetPage(const wxString& source);

    // Append to current page
    bool AppendToPage(const wxString& source);

    // Load HTML page from given location. Location can be either
    // a) /usr/wxGTK2/docs/html/wx.htm
    // b) http://www.somewhere.uk/document.htm
    // c) ftp://ftp.somesite.cz/pub/something.htm
    // In case there is no prefix (http:,ftp:), the method
    // will try to find it itself (1. local file, then http or ftp)
    // After the page is loaded, the method calls SetPage() to display it.
    // Note : you can also use path relative to previously loaded page
    // Return value : same as SetPage
    virtual bool LoadPage(const wxString& location);

    // Returns full location of opened page
    wxString GetOpenedPage() const {return m_OpenedPage;}
    // Returns anchor within opened page
    wxString GetOpenedAnchor() const {return m_OpenedAnchor;}
    // Returns <TITLE> of opened page or empty string otherwise
    wxString GetOpenedPageTitle() const {return m_OpenedPageTitle;}

    // Sets frame in which page title will  be displayed. Format is format of
    // frame title, e.g. "HtmlHelp : %s". It must contain exactly one %s
    void SetRelatedFrame(wxFrame* frame, const wxString& format);
    wxFrame* GetRelatedFrame() const {return m_RelatedFrame;}

    // After(!) calling SetRelatedFrame, this sets statusbar slot where messages
    // will be displayed. Default is -1 = no messages.
    void SetRelatedStatusBar(int bar);

    // Sets fonts to be used when displaying HTML page.
    void SetFonts(wxString normal_face, wxString fixed_face, const int *sizes);

    // Sets space between text and window borders.
    void SetBorders(int b) {m_Borders = b;}

    // Saves custom settings into cfg config. it will use the path 'path'
    // if given, otherwise it will save info into currently selected path.
    // saved values : things set by SetFonts, SetBorders.
    virtual void ReadCustomization(wxConfigBase *cfg, wxString path = wxEmptyString);
    // ...
    virtual void WriteCustomization(wxConfigBase *cfg, wxString path = wxEmptyString);

    // Goes to previous/next page (in browsing history)
    // Returns TRUE if successful, FALSE otherwise
    bool HistoryBack();
    bool HistoryForward();
    bool HistoryCanBack();
    bool HistoryCanForward();
    // Resets history
    void HistoryClear();

    // Returns pointer to conteiners/cells structure.
    // It should be used ONLY when printing
    wxHtmlContainerCell* GetInternalRepresentation() const {return m_Cell;}

    // Adds input filter
    static void AddFilter(wxHtmlFilter *filter);

    // Returns a pointer to the parser.
    wxHtmlWinParser *GetParser() const { return m_Parser; }

    // Adds HTML processor to this instance of wxHtmlWindow:
    void AddProcessor(wxHtmlProcessor *processor);
    // Adds HTML processor to wxHtmlWindow class as whole:
    static void AddGlobalProcessor(wxHtmlProcessor *processor);

    // what would we do with it?
    virtual bool AcceptsFocusFromKeyboard() const { return FALSE; }

    // -- Callbacks --

    // Sets the title of the window
    // (depending on the information passed to SetRelatedFrame() method)
    virtual void OnSetTitle(const wxString& title);

    // Called when the mouse hovers over a cell: (x, y) are logical coords
    // Default behaviour is to do nothing at all
    virtual void OnCellMouseHover(wxHtmlCell *cell, wxCoord x, wxCoord y);

    // Called when user clicks on a cell. Default behavior is to call
    // OnLinkClicked() if this cell corresponds to a hypertext link
    virtual void OnCellClicked(wxHtmlCell *cell,
                               wxCoord x, wxCoord y,
                               const wxMouseEvent& event);

    // Called when user clicked on hypertext link. Default behavior is to
    // call LoadPage(loc)
    virtual void OnLinkClicked(const wxHtmlLinkInfo& link);

    // Called when wxHtmlWindow wants to fetch data from an URL (e.g. when
    // loading a page or loading an image). The data are downloaded if and only if
    // OnOpeningURL returns TRUE. If OnOpeningURL returns wxHTML_REDIRECT,
    // it must set *redirect to the new URL
    virtual wxHtmlOpeningStatus OnOpeningURL(wxHtmlURLType WXUNUSED(type),
                                             const wxString& WXUNUSED(url),
                                             wxString *WXUNUSED(redirect)) const
        { return wxHTML_OPEN; }

protected:
    void Init();

    // Scrolls to anchor of this name. (Anchor is #news
    // or #features etc. it is part of address sometimes:
    // http://www.ms.mff.cuni.cz/~vsla8348/wxhtml/index.html#news)
    // Return value : TRUE if anchor exists, FALSE otherwise
    bool ScrollToAnchor(const wxString& anchor);

    // Prepares layout (= fill m_PosX, m_PosY for fragments) based on
    // actual size of window. This method also setup scrollbars
    void CreateLayout();

    void OnDraw(wxDC& dc);
    void OnSize(wxSizeEvent& event);
    void OnMouseEvent(wxMouseEvent& event);
    void OnIdle(wxIdleEvent& event);

    // Returns new filter (will be stored into m_DefaultFilter variable)
    virtual wxHtmlFilter *GetDefaultFilter() {return new wxHtmlFilterPlainText;}

    // cleans static variables
    static void CleanUpStatics();

protected:
    // This is pointer to the first cell in parsed data.
    // (Note: the first cell is usually top one = all other cells are sub-cells of this one)
    wxHtmlContainerCell *m_Cell;
    // parser which is used to parse HTML input.
    // Each wxHtmlWindow has it's own parser because sharing one global
    // parser would be problematic (because of reentrancy)
    wxHtmlWinParser *m_Parser;
    // contains name of actualy opened page or empty string if no page opened
    wxString m_OpenedPage;
    // contains name of current anchor within m_OpenedPage
    wxString m_OpenedAnchor;
    // contains title of actualy opened page or empty string if no <TITLE> tag
    wxString m_OpenedPageTitle;
    // class for opening files (file system)
    wxFileSystem* m_FS;

    wxFrame *m_RelatedFrame;
    wxString m_TitleFormat;
    // frame in which page title should be displayed & number of it's statusbar
    // reserved for usage with this html window
    int m_RelatedStatusBar;

    // borders (free space between text and window borders)
    // defaults to 10 pixels.
    int m_Borders;

    int m_Style;

private:
    // a flag indicated if mouse moved
    // (if TRUE we will try to change cursor in last call to OnIdle)
    bool m_tmpMouseMoved;
    // contains last link name
    wxHtmlLinkInfo *m_tmpLastLink;
    // contains the last (terminal) cell which contained the mouse
    wxHtmlCell *m_tmpLastCell;
    // if >0 contents of the window is not redrawn
    // (in order to avoid ugly blinking)
    int m_tmpCanDrawLocks;

    // list of HTML filters
    static wxList m_Filters;
    // this filter is used when no filter is able to read some file
    static wxHtmlFilter *m_DefaultFilter;

    static wxCursor *s_cur_hand;
    static wxCursor *s_cur_arrow;

    wxHtmlHistoryArray *m_History;
    // browser history
    int m_HistoryPos;
    // if this FLAG is false, items are not added to history
    bool m_HistoryOn;

    // html processors array:
    wxHtmlProcessorList *m_Processors;
    static wxHtmlProcessorList *m_GlobalProcessors;

    DECLARE_EVENT_TABLE()
};


#endif

#endif // _WX_HTMLWIN_H_

