/////////////////////////////////////////////////////////////////////////////
// Name:        htmlwin.cpp
// Purpose:     wxHtmlWindow class for parsing & displaying HTML (implementation)
// Author:      Vaclav Slavik
// RCS-ID:      $Id: htmlwin.cpp,v 1.62.2.1 2002/11/04 22:46:22 VZ Exp $
// Copyright:   (c) 1999 Vaclav Slavik
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////


#ifdef __GNUG__
#pragma implementation "htmlwin.h"
#pragma implementation "htmlproc.h"
#endif

#include "wx/wxprec.h"

#include "wx/defs.h"
#if wxUSE_HTML && wxUSE_STREAMS

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WXPRECOMP
    #include "wx/log.h"
    #include "wx/intl.h"
    #include "wx/dcclient.h"
    #include "wx/frame.h"
#endif

#include "wx/html/htmlwin.h"
#include "wx/html/htmlproc.h"
#include "wx/list.h"

#include "wx/arrimpl.cpp"
#include "wx/listimpl.cpp"

//-----------------------------------------------------------------------------
// wxHtmlHistoryItem
//-----------------------------------------------------------------------------

// item of history list
class WXDLLEXPORT wxHtmlHistoryItem
{
public:
    wxHtmlHistoryItem(const wxString& p, const wxString& a) {m_Page = p, m_Anchor = a, m_Pos = 0;}
    int GetPos() const {return m_Pos;}
    void SetPos(int p) {m_Pos = p;}
    const wxString& GetPage() const {return m_Page;}
    const wxString& GetAnchor() const {return m_Anchor;}

private:
    wxString m_Page;
    wxString m_Anchor;
    int m_Pos;
};


//-----------------------------------------------------------------------------
// our private arrays:
//-----------------------------------------------------------------------------

WX_DECLARE_OBJARRAY(wxHtmlHistoryItem, wxHtmlHistoryArray);
WX_DEFINE_OBJARRAY(wxHtmlHistoryArray);

WX_DECLARE_LIST(wxHtmlProcessor, wxHtmlProcessorList);
WX_DEFINE_LIST(wxHtmlProcessorList);

//-----------------------------------------------------------------------------
// wxHtmlWindow
//-----------------------------------------------------------------------------


void wxHtmlWindow::Init()
{
    m_tmpMouseMoved = FALSE;
    m_tmpLastLink = NULL;
    m_tmpLastCell = NULL;
    m_tmpCanDrawLocks = 0;
    m_FS = new wxFileSystem();
    m_RelatedStatusBar = -1;
    m_RelatedFrame = NULL;
    m_TitleFormat = wxT("%s");
    m_OpenedPage = m_OpenedAnchor = m_OpenedPageTitle = wxEmptyString;
    m_Cell = NULL;
    m_Parser = new wxHtmlWinParser(this);
    m_Parser->SetFS(m_FS);
    m_HistoryPos = -1;
    m_HistoryOn = TRUE;
    m_History = new wxHtmlHistoryArray;
    m_Processors = NULL;
    m_Style = 0;
    SetBorders(10);
}

bool wxHtmlWindow::Create(wxWindow *parent, wxWindowID id,
                          const wxPoint& pos, const wxSize& size,
                          long style, const wxString& name)
{
    if (!wxScrolledWindow::Create(parent, id, pos, size,
                                  style | wxVSCROLL | wxHSCROLL, name))
        return FALSE;

    m_Style = style;
    SetPage(wxT("<html><body></body></html>"));
    return TRUE;
}


wxHtmlWindow::~wxHtmlWindow()
{
    HistoryClear();

    if (m_Cell) delete m_Cell;

    delete m_Parser;
    delete m_FS;
    delete m_History;
    delete m_Processors;
}



void wxHtmlWindow::SetRelatedFrame(wxFrame* frame, const wxString& format)
{
    m_RelatedFrame = frame;
    m_TitleFormat = format;
}



void wxHtmlWindow::SetRelatedStatusBar(int bar)
{
    m_RelatedStatusBar = bar;
}



void wxHtmlWindow::SetFonts(wxString normal_face, wxString fixed_face, const int *sizes)
{
    wxString op = m_OpenedPage;

    m_Parser->SetFonts(normal_face, fixed_face, sizes);
    // fonts changed => contents invalid, so reload the page:
    SetPage(wxT("<html><body></body></html>"));
    if (!op.IsEmpty()) LoadPage(op);
}



bool wxHtmlWindow::SetPage(const wxString& source)
{
    wxString newsrc(source);

    // pass HTML through registered processors:
    if (m_Processors || m_GlobalProcessors)
    {
        wxHtmlProcessorList::Node *nodeL, *nodeG;
        int prL, prG;

        nodeL = (m_Processors) ? m_Processors->GetFirst() : NULL;
        nodeG = (m_GlobalProcessors) ? m_GlobalProcessors->GetFirst() : NULL;

        // VS: there are two lists, global and local, both of them sorted by
        //     priority. Since we have to go through _both_ lists with
        //     decreasing priority, we "merge-sort" the lists on-line by
        //     processing that one of the two heads that has higher priority
        //     in every iteration
        while (nodeL || nodeG)
        {
            prL = (nodeL) ? nodeL->GetData()->GetPriority() : -1;
            prG = (nodeG) ? nodeG->GetData()->GetPriority() : -1;
            if (prL > prG)
            {
                if (nodeL->GetData()->IsEnabled())
                    newsrc = nodeL->GetData()->Process(newsrc);
                nodeL = nodeL->GetNext();
            }
            else // prL <= prG
            {
                if (nodeG->GetData()->IsEnabled())
                    newsrc = nodeG->GetData()->Process(newsrc);
                nodeG = nodeG->GetNext();
            }
        }
    }

    // ...and run the parser on it:
    wxClientDC *dc = new wxClientDC(this);
    dc->SetMapMode(wxMM_TEXT);
    SetBackgroundColour(wxColour(0xFF, 0xFF, 0xFF));
    m_OpenedPage = m_OpenedAnchor = m_OpenedPageTitle = wxEmptyString;
    m_Parser->SetDC(dc);
    if (m_Cell)
    {
        delete m_Cell;
        m_Cell = NULL;
    }
    m_Cell = (wxHtmlContainerCell*) m_Parser->Parse(newsrc);
    delete dc;
    m_Cell->SetIndent(m_Borders, wxHTML_INDENT_ALL, wxHTML_UNITS_PIXELS);
    m_Cell->SetAlignHor(wxHTML_ALIGN_CENTER);
    CreateLayout();
    if (m_tmpCanDrawLocks == 0)
        Refresh();
    return TRUE;
}

bool wxHtmlWindow::AppendToPage(const wxString& source)
{
    return SetPage(*(GetParser()->GetSource()) + source);
}

bool wxHtmlWindow::LoadPage(const wxString& location)
{
    wxBusyCursor busyCursor;

    wxFSFile *f;
    bool rt_val;
    bool needs_refresh = FALSE;

    m_tmpCanDrawLocks++;
    if (m_HistoryOn && (m_HistoryPos != -1))
    {
        // store scroll position into history item:
        int x, y;
        GetViewStart(&x, &y);
        (*m_History)[m_HistoryPos].SetPos(y);
    }

    if (location[0] == wxT('#'))
    {
        // local anchor:
        wxString anch = location.Mid(1) /*1 to end*/;
        m_tmpCanDrawLocks--;
        rt_val = ScrollToAnchor(anch);
        m_tmpCanDrawLocks++;
    }
    else if (location.Find(wxT('#')) != wxNOT_FOUND && location.BeforeFirst(wxT('#')) == m_OpenedPage)
    {
        wxString anch = location.AfterFirst(wxT('#'));
        m_tmpCanDrawLocks--;
        rt_val = ScrollToAnchor(anch);
        m_tmpCanDrawLocks++;
    }
    else if (location.Find(wxT('#')) != wxNOT_FOUND &&
             (m_FS->GetPath() + location.BeforeFirst(wxT('#'))) == m_OpenedPage)
    {
        wxString anch = location.AfterFirst(wxT('#'));
        m_tmpCanDrawLocks--;
        rt_val = ScrollToAnchor(anch);
        m_tmpCanDrawLocks++;
    }

    else
    {
        needs_refresh = TRUE;
        // load&display it:
        if (m_RelatedStatusBar != -1)
        {
            m_RelatedFrame->SetStatusText(_("Connecting..."), m_RelatedStatusBar);
            Refresh(FALSE);
        }

        f = m_Parser->OpenURL(wxHTML_URL_PAGE, location);

        if (f == NULL)
        {
            wxLogError(_("Unable to open requested HTML document: %s"), location.c_str());
            m_tmpCanDrawLocks--;
            return FALSE;
        }

        else
        {
            wxNode *node;
            wxString src = wxEmptyString;

            if (m_RelatedStatusBar != -1)
            {
                wxString msg = _("Loading : ") + location;
                m_RelatedFrame->SetStatusText(msg, m_RelatedStatusBar);
                Refresh(FALSE);
            }

            node = m_Filters.GetFirst();
            while (node)
            {
                wxHtmlFilter *h = (wxHtmlFilter*) node->GetData();
                if (h->CanRead(*f))
                {
                    src = h->ReadFile(*f);
                    break;
                }
                node = node->GetNext();
            }
            if (src == wxEmptyString)
            {
                if (m_DefaultFilter == NULL) m_DefaultFilter = GetDefaultFilter();
                src = m_DefaultFilter->ReadFile(*f);
            }

            m_FS->ChangePathTo(f->GetLocation());
            rt_val = SetPage(src);
            m_OpenedPage = f->GetLocation();
            if (f->GetAnchor() != wxEmptyString)
            {
                ScrollToAnchor(f->GetAnchor());
            }

            delete f;

            if (m_RelatedStatusBar != -1) m_RelatedFrame->SetStatusText(_("Done"), m_RelatedStatusBar);
        }
    }

    if (m_HistoryOn) // add this page to history there:
    {
        int c = m_History->GetCount() - (m_HistoryPos + 1);

        if (m_HistoryPos < 0 ||
            (*m_History)[m_HistoryPos].GetPage() != m_OpenedPage ||
            (*m_History)[m_HistoryPos].GetAnchor() != m_OpenedAnchor)
        {
            m_HistoryPos++;
            for (int i = 0; i < c; i++)
                m_History->RemoveAt(m_HistoryPos);
            m_History->Add(new wxHtmlHistoryItem(m_OpenedPage, m_OpenedAnchor));
        }
    }

    if (m_OpenedPageTitle == wxEmptyString)
        OnSetTitle(wxFileNameFromPath(m_OpenedPage));

    if (needs_refresh)
    {
        m_tmpCanDrawLocks--;
        Refresh();
    }
    else
        m_tmpCanDrawLocks--;

    return rt_val;
}



bool wxHtmlWindow::ScrollToAnchor(const wxString& anchor)
{
    const wxHtmlCell *c = m_Cell->Find(wxHTML_COND_ISANCHOR, &anchor);
    if (!c)
    {
        wxLogWarning(_("HTML anchor %s does not exist."), anchor.c_str());
        return FALSE;
    }
    else
    {
        int y;

        for (y = 0; c != NULL; c = c->GetParent()) y += c->GetPosY();
        Scroll(-1, y / wxHTML_SCROLL_STEP);
        m_OpenedAnchor = anchor;
        return TRUE;
    }
}


void wxHtmlWindow::OnSetTitle(const wxString& title)
{
    if (m_RelatedFrame)
    {
        wxString tit;
        tit.Printf(m_TitleFormat, title.c_str());
        m_RelatedFrame->SetTitle(tit);
    }
    m_OpenedPageTitle = title;
}





void wxHtmlWindow::CreateLayout()
{
    int ClientWidth, ClientHeight;

    if (!m_Cell) return;

    if (m_Style & wxHW_SCROLLBAR_NEVER)
    {
        SetScrollbars(wxHTML_SCROLL_STEP, 1, m_Cell->GetWidth() / wxHTML_SCROLL_STEP, 0); // always off
        GetClientSize(&ClientWidth, &ClientHeight);
        m_Cell->Layout(ClientWidth);
    }

    else {
        GetClientSize(&ClientWidth, &ClientHeight);
        m_Cell->Layout(ClientWidth);
        if (ClientHeight < m_Cell->GetHeight() + GetCharHeight())
        {
            SetScrollbars(
                  wxHTML_SCROLL_STEP, wxHTML_SCROLL_STEP,
                  m_Cell->GetWidth() / wxHTML_SCROLL_STEP,
                  (m_Cell->GetHeight() + GetCharHeight()) / wxHTML_SCROLL_STEP
                  /*cheat: top-level frag is always container*/);
        }
        else /* we fit into window, no need for scrollbars */
        {
            SetScrollbars(wxHTML_SCROLL_STEP, 1, m_Cell->GetWidth() / wxHTML_SCROLL_STEP, 0); // disable...
            GetClientSize(&ClientWidth, &ClientHeight);
            m_Cell->Layout(ClientWidth); // ...and relayout
        }
    }
}



void wxHtmlWindow::ReadCustomization(wxConfigBase *cfg, wxString path)
{
    wxString oldpath;
    wxString tmp;
    int p_fontsizes[7];
    wxString p_fff, p_ffn;

    if (path != wxEmptyString)
    {
        oldpath = cfg->GetPath();
        cfg->SetPath(path);
    }

    m_Borders = cfg->Read(wxT("wxHtmlWindow/Borders"), m_Borders);
    p_fff = cfg->Read(wxT("wxHtmlWindow/FontFaceFixed"), m_Parser->m_FontFaceFixed);
    p_ffn = cfg->Read(wxT("wxHtmlWindow/FontFaceNormal"), m_Parser->m_FontFaceNormal);
    for (int i = 0; i < 7; i++)
    {
        tmp.Printf(wxT("wxHtmlWindow/FontsSize%i"), i);
        p_fontsizes[i] = cfg->Read(tmp, m_Parser->m_FontsSizes[i]);
    }
    SetFonts(p_ffn, p_fff, p_fontsizes);

    if (path != wxEmptyString)
        cfg->SetPath(oldpath);
}



void wxHtmlWindow::WriteCustomization(wxConfigBase *cfg, wxString path)
{
    wxString oldpath;
    wxString tmp;

    if (path != wxEmptyString)
    {
        oldpath = cfg->GetPath();
        cfg->SetPath(path);
    }

    cfg->Write(wxT("wxHtmlWindow/Borders"), (long) m_Borders);
    cfg->Write(wxT("wxHtmlWindow/FontFaceFixed"), m_Parser->m_FontFaceFixed);
    cfg->Write(wxT("wxHtmlWindow/FontFaceNormal"), m_Parser->m_FontFaceNormal);
    for (int i = 0; i < 7; i++)
    {
        tmp.Printf(wxT("wxHtmlWindow/FontsSize%i"), i);
        cfg->Write(tmp, (long) m_Parser->m_FontsSizes[i]);
    }

    if (path != wxEmptyString)
        cfg->SetPath(oldpath);
}



bool wxHtmlWindow::HistoryBack()
{
    wxString a, l;

    if (m_HistoryPos < 1) return FALSE;

    // store scroll position into history item:
    int x, y;
    GetViewStart(&x, &y);
    (*m_History)[m_HistoryPos].SetPos(y);

    // go to previous position:
    m_HistoryPos--;

    l = (*m_History)[m_HistoryPos].GetPage();
    a = (*m_History)[m_HistoryPos].GetAnchor();
    m_HistoryOn = FALSE;
    m_tmpCanDrawLocks++;
    if (a == wxEmptyString) LoadPage(l);
    else LoadPage(l + wxT("#") + a);
    m_HistoryOn = TRUE;
    m_tmpCanDrawLocks--;
    Scroll(0, (*m_History)[m_HistoryPos].GetPos());
    Refresh();
    return TRUE;
}

bool wxHtmlWindow::HistoryCanBack()
{
    if (m_HistoryPos < 1) return FALSE;
    return TRUE ;
}


bool wxHtmlWindow::HistoryForward()
{
    wxString a, l;

    if (m_HistoryPos == -1) return FALSE;
    if (m_HistoryPos >= (int)m_History->GetCount() - 1)return FALSE;

    m_OpenedPage = wxEmptyString; // this will disable adding new entry into history in LoadPage()

    m_HistoryPos++;
    l = (*m_History)[m_HistoryPos].GetPage();
    a = (*m_History)[m_HistoryPos].GetAnchor();
    m_HistoryOn = FALSE;
    m_tmpCanDrawLocks++;
    if (a == wxEmptyString) LoadPage(l);
    else LoadPage(l + wxT("#") + a);
    m_HistoryOn = TRUE;
    m_tmpCanDrawLocks--;
    Scroll(0, (*m_History)[m_HistoryPos].GetPos());
    Refresh();
    return TRUE;
}

bool wxHtmlWindow::HistoryCanForward()
{
    if (m_HistoryPos == -1) return FALSE;
    if (m_HistoryPos >= (int)m_History->GetCount() - 1)return FALSE;
    return TRUE ;
}


void wxHtmlWindow::HistoryClear()
{
    m_History->Empty();
    m_HistoryPos = -1;
}

void wxHtmlWindow::AddProcessor(wxHtmlProcessor *processor)
{
    if (!m_Processors)
    {
        m_Processors = new wxHtmlProcessorList;
        m_Processors->DeleteContents(TRUE);
    }
    wxHtmlProcessorList::Node *node;

    for (node = m_Processors->GetFirst(); node; node = node->GetNext())
    {
        if (processor->GetPriority() > node->GetData()->GetPriority())
        {
            m_Processors->Insert(node, processor);
            return;
        }
    }
    m_Processors->Append(processor);
}

/*static */ void wxHtmlWindow::AddGlobalProcessor(wxHtmlProcessor *processor)
{
    if (!m_GlobalProcessors)
    {
        m_GlobalProcessors = new wxHtmlProcessorList;
        m_GlobalProcessors->DeleteContents(TRUE);
    }
    wxHtmlProcessorList::Node *node;

    for (node = m_GlobalProcessors->GetFirst(); node; node = node->GetNext())
    {
        if (processor->GetPriority() > node->GetData()->GetPriority())
        {
            m_GlobalProcessors->Insert(node, processor);
            return;
        }
    }
    m_GlobalProcessors->Append(processor);
}



wxList wxHtmlWindow::m_Filters;
wxHtmlFilter *wxHtmlWindow::m_DefaultFilter = NULL;
wxCursor *wxHtmlWindow::s_cur_hand = NULL;
wxCursor *wxHtmlWindow::s_cur_arrow = NULL;
wxHtmlProcessorList *wxHtmlWindow::m_GlobalProcessors = NULL;

void wxHtmlWindow::CleanUpStatics()
{
    delete m_DefaultFilter;
    m_DefaultFilter = NULL;
    m_Filters.DeleteContents(TRUE);
    m_Filters.Clear();
    delete m_GlobalProcessors;
    m_GlobalProcessors = NULL;
    delete s_cur_hand;
    delete s_cur_arrow;
}



void wxHtmlWindow::AddFilter(wxHtmlFilter *filter)
{
    m_Filters.Append(filter);
}




void wxHtmlWindow::OnLinkClicked(const wxHtmlLinkInfo& link)
{
    LoadPage(link.GetHref());
}

void wxHtmlWindow::OnCellClicked(wxHtmlCell *cell,
                                 wxCoord x, wxCoord y,
                                 const wxMouseEvent& event)
{
    wxCHECK_RET( cell, _T("can't be called with NULL cell") );

    cell->OnMouseClick(this, x, y, event);
}

void wxHtmlWindow::OnCellMouseHover(wxHtmlCell * WXUNUSED(cell),
                                    wxCoord WXUNUSED(x), wxCoord WXUNUSED(y))
{
    // do nothing here
}

void wxHtmlWindow::OnDraw(wxDC& dc)
{
    if (m_tmpCanDrawLocks > 0 || m_Cell == NULL) return;

    int x, y;
    wxRect rect = GetUpdateRegion().GetBox();

    dc.SetMapMode(wxMM_TEXT);
    dc.SetBackgroundMode(wxTRANSPARENT);
    GetViewStart(&x, &y);

    m_Cell->Draw(dc, 0, 0,
                 y * wxHTML_SCROLL_STEP + rect.GetTop(),
                 y * wxHTML_SCROLL_STEP + rect.GetBottom());
}




void wxHtmlWindow::OnSize(wxSizeEvent& event)
{
    wxScrolledWindow::OnSize(event);
    CreateLayout();
    Refresh();
}


void wxHtmlWindow::OnMouseEvent(wxMouseEvent& event)
{
    m_tmpMouseMoved = TRUE;

    if (event.ButtonDown())
    {
        SetFocus();
        if ( m_Cell )
        {
            int sx, sy;
            GetViewStart(&sx, &sy);
            sx *= wxHTML_SCROLL_STEP;
            sy *= wxHTML_SCROLL_STEP;

            wxPoint pos = event.GetPosition();
            pos.x += sx;
            pos.y += sy;

            wxHtmlCell *cell = m_Cell->FindCellByPos(pos.x, pos.y);

            // VZ: is it possible that we don't find anything at all?
            // VS: yes. FindCellByPos returns terminal cell and
            //     containers may have empty borders
            if ( cell )
                OnCellClicked(cell, pos.x, pos.y, event);
        }
    }
}



void wxHtmlWindow::OnIdle(wxIdleEvent& WXUNUSED(event))
{
    if (s_cur_hand == NULL)
    {
        s_cur_hand = new wxCursor(wxCURSOR_HAND);
        s_cur_arrow = new wxCursor(wxCURSOR_ARROW);
    }

    if (m_tmpMouseMoved && (m_Cell != NULL))
    {
        int sx, sy;
        GetViewStart(&sx, &sy);
        sx *= wxHTML_SCROLL_STEP;
        sy *= wxHTML_SCROLL_STEP;

        int x, y;
        wxGetMousePosition(&x, &y);
        ScreenToClient(&x, &y);
        x += sx;
        y += sy;

        wxHtmlCell *cell = m_Cell->FindCellByPos(x, y);
        if ( cell != m_tmpLastCell )
        {
            wxHtmlLinkInfo *lnk = cell ? cell->GetLink(x, y) : NULL;

            if (lnk != m_tmpLastLink)
            {
                if (lnk == NULL)
                {
                    SetCursor(*s_cur_arrow);
                    if (m_RelatedStatusBar != -1)
                        m_RelatedFrame->SetStatusText(wxEmptyString, m_RelatedStatusBar);
                }
                else
                {
                    SetCursor(*s_cur_hand);
                    if (m_RelatedStatusBar != -1)
                        m_RelatedFrame->SetStatusText(lnk->GetHref(), m_RelatedStatusBar);
                }
                m_tmpLastLink = lnk;
            }

            m_tmpLastCell = cell;
        }
        else // mouse moved but stayed in the same cell
        {
            if ( cell )
                OnCellMouseHover(cell, x, y);
        }

        m_tmpMouseMoved = FALSE;
    }
}


IMPLEMENT_ABSTRACT_CLASS(wxHtmlProcessor,wxObject)

IMPLEMENT_DYNAMIC_CLASS(wxHtmlWindow,wxScrolledWindow)

BEGIN_EVENT_TABLE(wxHtmlWindow, wxScrolledWindow)
    EVT_SIZE(wxHtmlWindow::OnSize)
    EVT_LEFT_DOWN(wxHtmlWindow::OnMouseEvent)
    EVT_RIGHT_DOWN(wxHtmlWindow::OnMouseEvent)
    EVT_MOTION(wxHtmlWindow::OnMouseEvent)
    EVT_IDLE(wxHtmlWindow::OnIdle)
END_EVENT_TABLE()





// A module to allow initialization/cleanup
// without calling these functions from app.cpp or from
// the user's application.

class wxHtmlWinModule: public wxModule
{
DECLARE_DYNAMIC_CLASS(wxHtmlWinModule)
public:
    wxHtmlWinModule() : wxModule() {}
    bool OnInit() { return TRUE; }
    void OnExit() { wxHtmlWindow::CleanUpStatics(); }
};

IMPLEMENT_DYNAMIC_CLASS(wxHtmlWinModule, wxModule)


// This hack forces the linker to always link in m_* files
// (wxHTML doesn't work without handlers from these files)
#include "wx/html/forcelnk.h"
FORCE_WXHTML_MODULES()

#endif
