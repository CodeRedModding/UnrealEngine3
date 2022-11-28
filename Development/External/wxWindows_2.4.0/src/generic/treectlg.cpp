/////////////////////////////////////////////////////////////////////////////
// Name:        treectlg.cpp
// Purpose:     generic tree control implementation
// Author:      Robert Roebling
// Created:     01/02/97
// Modified:    22/10/98 - almost total rewrite, simpler interface (VZ)
// Id:          $Id: treectlg.cpp,v 1.85.2.4 2002/12/29 07:48:18 RL Exp $
// Copyright:   (c) 1998 Robert Roebling, Julian Smart and Markus Holzem
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// =============================================================================
// declarations
// =============================================================================

// -----------------------------------------------------------------------------
// headers
// -----------------------------------------------------------------------------

#ifdef __GNUG__
  #pragma implementation "treectlg.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_TREECTRL

#include "wx/treebase.h"
#include "wx/generic/treectlg.h"
#include "wx/timer.h"
#include "wx/textctrl.h"
#include "wx/imaglist.h"
#include "wx/settings.h"
#include "wx/dcclient.h"

// -----------------------------------------------------------------------------
// array types
// -----------------------------------------------------------------------------

class WXDLLEXPORT wxGenericTreeItem;

WX_DEFINE_EXPORTED_ARRAY(wxGenericTreeItem *, wxArrayGenericTreeItems);

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

static const int NO_IMAGE = -1;

static const int PIXELS_PER_UNIT = 10;

// ----------------------------------------------------------------------------
// Aqua arrows
// ----------------------------------------------------------------------------

/* XPM */
static const char *aqua_arrow_right[] = {
/* columns rows colors chars-per-pixel */
"13 11 4 1",
"  c None",
"b c #C0C0C0",
"c c #707070",
"d c #A0A0A0",
/* pixels */
"    b        ",
"    ddb      ",
"    cccdb    ",
"    cccccd   ",
"    ccccccdb ",
"    ccccccccd",
"    ccccccdb ",
"    cccccb   ",
"    cccdb    ",
"    ddb      ",
"    b        "
};

/* XPM */
static const char *aqua_arrow_down[] = {
/* columns rows colors chars-per-pixel */
"13 11 4 1",
"  c None",
"b c #C0C0C0",
"c c #707070",
"d c #A0A0A0",
/* pixels */
"             ",
"             ",
" bdcccccccdb ",
"  dcccccccd  ",
"  bcccccccb  ",
"   dcccccd   ",
"   bcccccb   ",
"    bcccd    ",
"     dcd     ",
"     bcb     ",
"      d      "
};

// -----------------------------------------------------------------------------
// private classes
// -----------------------------------------------------------------------------

// timer used for enabling in-place edit
class WXDLLEXPORT wxTreeRenameTimer: public wxTimer
{
public:
    // start editing the current item after half a second (if the mouse hasn't
    // been clicked/moved)
    enum { DELAY = 500 };

    wxTreeRenameTimer( wxGenericTreeCtrl *owner );

    virtual void Notify();

private:
    wxGenericTreeCtrl *m_owner;
};

// control used for in-place edit
class WXDLLEXPORT wxTreeTextCtrl: public wxTextCtrl
{
public:
    wxTreeTextCtrl(wxGenericTreeCtrl *owner, wxGenericTreeItem *item);

protected:
    void OnChar( wxKeyEvent &event );
    void OnKeyUp( wxKeyEvent &event );
    void OnKillFocus( wxFocusEvent &event );

    bool AcceptChanges();
    void Finish();

private:
    wxGenericTreeCtrl  *m_owner;
    wxGenericTreeItem  *m_itemEdited;
    wxString            m_startValue;
    bool                m_finished;

    DECLARE_EVENT_TABLE()
};

// timer used to clear wxGenericTreeCtrl::m_findPrefix if no key was pressed
// for a sufficiently long time
class WXDLLEXPORT wxTreeFindTimer : public wxTimer
{
public:
    // reset the current prefix after half a second of inactivity
    enum { DELAY = 500 };

    wxTreeFindTimer( wxGenericTreeCtrl *owner ) { m_owner = owner; }

    virtual void Notify() { m_owner->m_findPrefix.clear(); }

private:
    wxGenericTreeCtrl *m_owner;
};

// a tree item
class WXDLLEXPORT wxGenericTreeItem
{
public:
    // ctors & dtor
    wxGenericTreeItem() { m_data = NULL; }
    wxGenericTreeItem( wxGenericTreeItem *parent,
                       const wxString& text,
                       int image,
                       int selImage,
                       wxTreeItemData *data );

    ~wxGenericTreeItem();

    // trivial accessors
    wxArrayGenericTreeItems& GetChildren() { return m_children; }

    const wxString& GetText() const { return m_text; }
    int GetImage(wxTreeItemIcon which = wxTreeItemIcon_Normal) const
        { return m_images[which]; }
    wxTreeItemData *GetData() const { return m_data; }

    // returns the current image for the item (depending on its
    // selected/expanded/whatever state)
    int GetCurrentImage() const;

    void SetText( const wxString &text );
    void SetImage(int image, wxTreeItemIcon which) { m_images[which] = image; }
    void SetData(wxTreeItemData *data) { m_data = data; }

    void SetHasPlus(bool has = TRUE) { m_hasPlus = has; }

    void SetBold(bool bold) { m_isBold = bold; }

    int GetX() const { return m_x; }
    int GetY() const { return m_y; }

    void SetX(int x) { m_x = x; }
    void SetY(int y) { m_y = y; }

    int  GetHeight() const { return m_height; }
    int  GetWidth()  const { return m_width; }

    void SetHeight(int h) { m_height = h; }
    void SetWidth(int w) { m_width = w; }

    wxGenericTreeItem *GetParent() const { return m_parent; }

    // operations
        // deletes all children notifying the treectrl about it if !NULL
        // pointer given
    void DeleteChildren(wxGenericTreeCtrl *tree = NULL);

    // get count of all children (and grand children if 'recursively')
    size_t GetChildrenCount(bool recursively = TRUE) const;

    void Insert(wxGenericTreeItem *child, size_t index)
    { m_children.Insert(child, index); }

    void GetSize( int &x, int &y, const wxGenericTreeCtrl* );

        // return the item at given position (or NULL if no item), onButton is
        // TRUE if the point belongs to the item's button, otherwise it lies
        // on the button's label
    wxGenericTreeItem *HitTest( const wxPoint& point,
                                const wxGenericTreeCtrl *,
                                int &flags,
                                int level );

    void Expand() { m_isCollapsed = FALSE; }
    void Collapse() { m_isCollapsed = TRUE; }

    void SetHilight( bool set = TRUE ) { m_hasHilight = set; }

    // status inquiries
    bool HasChildren() const { return !m_children.IsEmpty(); }
    bool IsSelected()  const { return m_hasHilight != 0; }
    bool IsExpanded()  const { return !m_isCollapsed; }
    bool HasPlus()     const { return m_hasPlus || HasChildren(); }
    bool IsBold()      const { return m_isBold != 0; }

    // attributes
        // get them - may be NULL
    wxTreeItemAttr *GetAttributes() const { return m_attr; }
        // get them ensuring that the pointer is not NULL
    wxTreeItemAttr& Attr()
    {
        if ( !m_attr )
        {
            m_attr = new wxTreeItemAttr;
            m_ownsAttr = TRUE;
        }
        return *m_attr;
    }
        // set them
    void SetAttributes(wxTreeItemAttr *attr)
    {
        if ( m_ownsAttr ) delete m_attr;
        m_attr = attr;
        m_ownsAttr = FALSE;
    }
        // set them and delete when done
    void AssignAttributes(wxTreeItemAttr *attr)
    {
        SetAttributes(attr);
        m_ownsAttr = TRUE;
    }

private:
    // since there can be very many of these, we save size by chosing
    // the smallest representation for the elements and by ordering
    // the members to avoid padding.
    wxString            m_text;         // label to be rendered for item

    wxTreeItemData     *m_data;         // user-provided data

    wxArrayGenericTreeItems m_children; // list of children
    wxGenericTreeItem  *m_parent;       // parent of this item

    wxTreeItemAttr     *m_attr;         // attributes???

    // tree ctrl images for the normal, selected, expanded and
    // expanded+selected states
    short               m_images[wxTreeItemIcon_Max];

    wxCoord             m_x;            // (virtual) offset from top
    short               m_y;            // (virtual) offset from left
    short               m_width;        // width of this item
    unsigned char       m_height;       // height of this item

    // use bitfields to save size
    int                 m_isCollapsed :1;
    int                 m_hasHilight  :1; // same as focused
    int                 m_hasPlus     :1; // used for item which doesn't have
                                          // children but has a [+] button
    int                 m_isBold      :1; // render the label in bold font
    int                 m_ownsAttr    :1; // delete attribute when done
};

// =============================================================================
// implementation
// =============================================================================

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

// translate the key or mouse event flags to the type of selection we're
// dealing with
static void EventFlagsToSelType(long style,
                                bool shiftDown,
                                bool ctrlDown,
                                bool &is_multiple,
                                bool &extended_select,
                                bool &unselect_others)
{
    is_multiple = (style & wxTR_MULTIPLE) != 0;
    extended_select = shiftDown && is_multiple;
    unselect_others = !(extended_select || (ctrlDown && is_multiple));
}

// check if the given item is under another one
static bool IsDescendantOf(wxGenericTreeItem *parent, wxGenericTreeItem *item)
{
    while ( item )
    {
        if ( item == parent )
        {
            // item is a descendant of parent
            return TRUE;
        }

        item = item->GetParent();
    }

    return FALSE;
}

// -----------------------------------------------------------------------------
// wxTreeRenameTimer (internal)
// -----------------------------------------------------------------------------

wxTreeRenameTimer::wxTreeRenameTimer( wxGenericTreeCtrl *owner )
{
    m_owner = owner;
}

void wxTreeRenameTimer::Notify()
{
    m_owner->OnRenameTimer();
}

//-----------------------------------------------------------------------------
// wxTreeTextCtrl (internal)
//-----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxTreeTextCtrl,wxTextCtrl)
    EVT_CHAR           (wxTreeTextCtrl::OnChar)
    EVT_KEY_UP         (wxTreeTextCtrl::OnKeyUp)
    EVT_KILL_FOCUS     (wxTreeTextCtrl::OnKillFocus)
END_EVENT_TABLE()

wxTreeTextCtrl::wxTreeTextCtrl(wxGenericTreeCtrl *owner,
                               wxGenericTreeItem *item)
              : m_itemEdited(item), m_startValue(item->GetText())
{
    m_owner = owner;
    m_finished = FALSE;

    int w = m_itemEdited->GetWidth(),
        h = m_itemEdited->GetHeight();

    int x, y;
    m_owner->CalcScrolledPosition(item->GetX(), item->GetY(), &x, &y);

    int image_h = 0,
        image_w = 0;

    int image = item->GetCurrentImage();
    if ( image != NO_IMAGE )
    {
        if ( m_owner->m_imageListNormal )
        {
            m_owner->m_imageListNormal->GetSize( image, image_w, image_h );
            image_w += 4;
        }
        else
        {
            wxFAIL_MSG(_T("you must create an image list to use images!"));
        }
    }

    // FIXME: what are all these hardcoded 4, 8 and 11s really?
    x += image_w;
    w -= image_w + 4;

    (void)Create(m_owner, wxID_ANY, m_startValue,
                 wxPoint(x - 4, y - 4), wxSize(w + 11, h + 8));
}

bool wxTreeTextCtrl::AcceptChanges()
{
    const wxString value = GetValue();

    if ( value == m_startValue )
    {
        // nothing changed, always accept
        return TRUE;
    }

    if ( !m_owner->OnRenameAccept(m_itemEdited, value) )
    {
        // vetoed by the user
        return FALSE;
    }

    // accepted, do rename the item
    m_owner->SetItemText(m_itemEdited, value);

    return TRUE;
}

void wxTreeTextCtrl::Finish()
{
    if ( !m_finished )
    {
        m_owner->ResetTextControl();

        wxPendingDelete.Append(this);

        m_finished = TRUE;

        m_owner->SetFocus(); // This doesn't work. TODO.
    }
}

void wxTreeTextCtrl::OnChar( wxKeyEvent &event )
{
    switch ( event.m_keyCode )
    {
        case WXK_RETURN:
            if ( !AcceptChanges() )
            {
                // vetoed by the user, don't disappear
                break;
            }
            //else: fall through

        case WXK_ESCAPE:
            Finish();
            m_owner->OnRenameCancelled(m_itemEdited);
            break;

        default:
            event.Skip();
    }
}

void wxTreeTextCtrl::OnKeyUp( wxKeyEvent &event )
{
    if ( !m_finished )
    {
        // auto-grow the textctrl:
        wxSize parentSize = m_owner->GetSize();
        wxPoint myPos = GetPosition();
        wxSize mySize = GetSize();
        int sx, sy;
        GetTextExtent(GetValue() + _T("M"), &sx, &sy);
        if (myPos.x + sx > parentSize.x)
            sx = parentSize.x - myPos.x;
        if (mySize.x > sx)
            sx = mySize.x;
        SetSize(sx, -1);
    }

    event.Skip();
}

void wxTreeTextCtrl::OnKillFocus( wxFocusEvent &event )
{
    if ( m_finished )
    {
        event.Skip();
        return;
    }

    if ( AcceptChanges() )
    {
        Finish();
    }
}

// -----------------------------------------------------------------------------
// wxGenericTreeItem
// -----------------------------------------------------------------------------

wxGenericTreeItem::wxGenericTreeItem(wxGenericTreeItem *parent,
                                     const wxString& text,
                                     int image, int selImage,
                                     wxTreeItemData *data)
                 : m_text(text)
{
    m_images[wxTreeItemIcon_Normal] = image;
    m_images[wxTreeItemIcon_Selected] = selImage;
    m_images[wxTreeItemIcon_Expanded] = NO_IMAGE;
    m_images[wxTreeItemIcon_SelectedExpanded] = NO_IMAGE;

    m_data = data;
    m_x = m_y = 0;

    m_isCollapsed = TRUE;
    m_hasHilight = FALSE;
    m_hasPlus = FALSE;
    m_isBold = FALSE;

    m_parent = parent;

    m_attr = (wxTreeItemAttr *)NULL;
    m_ownsAttr = FALSE;

    // We don't know the height here yet.
    m_width = 0;
    m_height = 0;
}

wxGenericTreeItem::~wxGenericTreeItem()
{
    delete m_data;

    if (m_ownsAttr) delete m_attr;

    wxASSERT_MSG( m_children.IsEmpty(),
                  wxT("please call DeleteChildren() before deleting the item") );
}

void wxGenericTreeItem::DeleteChildren(wxGenericTreeCtrl *tree)
{
    size_t count = m_children.Count();
    for ( size_t n = 0; n < count; n++ )
    {
        wxGenericTreeItem *child = m_children[n];
        if (tree)
            tree->SendDeleteEvent(child);

        child->DeleteChildren(tree);
        delete child;
    }

    m_children.Empty();
}

void wxGenericTreeItem::SetText( const wxString &text )
{
    m_text = text;
}

size_t wxGenericTreeItem::GetChildrenCount(bool recursively) const
{
    size_t count = m_children.Count();
    if ( !recursively )
        return count;

    size_t total = count;
    for (size_t n = 0; n < count; ++n)
    {
        total += m_children[n]->GetChildrenCount();
    }

    return total;
}

void wxGenericTreeItem::GetSize( int &x, int &y,
                                 const wxGenericTreeCtrl *theButton )
{
    int bottomY=m_y+theButton->GetLineHeight(this);
    if ( y < bottomY ) y = bottomY;
    int width = m_x +  m_width;
    if ( x < width ) x = width;

    if (IsExpanded())
    {
        size_t count = m_children.Count();
        for ( size_t n = 0; n < count; ++n )
        {
            m_children[n]->GetSize( x, y, theButton );
        }
    }
}

wxGenericTreeItem *wxGenericTreeItem::HitTest(const wxPoint& point,
                                              const wxGenericTreeCtrl *theCtrl,
                                              int &flags,
                                              int level)
{
    // for a hidden root node, don't evaluate it, but do evaluate children
    if ( !(level == 0 && theCtrl->HasFlag(wxTR_HIDE_ROOT)) )
    {
        // evaluate the item
        int h = theCtrl->GetLineHeight(this);
        if ((point.y > m_y) && (point.y < m_y + h))
        {
            int y_mid = m_y + h/2;
            if (point.y < y_mid )
                flags |= wxTREE_HITTEST_ONITEMUPPERPART;
            else
                flags |= wxTREE_HITTEST_ONITEMLOWERPART;

            // 5 is the size of the plus sign
            int xCross = m_x - theCtrl->GetSpacing();
            if ((point.x > xCross-5) && (point.x < xCross+5) &&
                (point.y > y_mid-5) && (point.y < y_mid+5) &&
                HasPlus() && theCtrl->HasButtons() )
            {
                flags |= wxTREE_HITTEST_ONITEMBUTTON;
                return this;
            }

            if ((point.x >= m_x) && (point.x <= m_x+m_width))
            {
                int image_w = -1;
                int image_h;

                // assuming every image (normal and selected) has the same size!
                if ( (GetImage() != NO_IMAGE) && theCtrl->m_imageListNormal )
                    theCtrl->m_imageListNormal->GetSize(GetImage(),
                                                        image_w, image_h);

                if ((image_w != -1) && (point.x <= m_x + image_w + 1))
                    flags |= wxTREE_HITTEST_ONITEMICON;
                else
                    flags |= wxTREE_HITTEST_ONITEMLABEL;

                return this;
            }

            if (point.x < m_x)
                flags |= wxTREE_HITTEST_ONITEMINDENT;
            if (point.x > m_x+m_width)
                flags |= wxTREE_HITTEST_ONITEMRIGHT;

            return this;
        }

        // if children are expanded, fall through to evaluate them
        if (m_isCollapsed) return (wxGenericTreeItem*) NULL;
    }

    // evaluate children
    size_t count = m_children.Count();
    for ( size_t n = 0; n < count; n++ )
    {
        wxGenericTreeItem *res = m_children[n]->HitTest( point,
                                                         theCtrl,
                                                         flags,
                                                         level + 1 );
        if ( res != NULL )
            return res;
    }

    return (wxGenericTreeItem*) NULL;
}

int wxGenericTreeItem::GetCurrentImage() const
{
    int image = NO_IMAGE;
    if ( IsExpanded() )
    {
        if ( IsSelected() )
        {
            image = GetImage(wxTreeItemIcon_SelectedExpanded);
        }

        if ( image == NO_IMAGE )
        {
            // we usually fall back to the normal item, but try just the
            // expanded one (and not selected) first in this case
            image = GetImage(wxTreeItemIcon_Expanded);
        }
    }
    else // not expanded
    {
        if ( IsSelected() )
            image = GetImage(wxTreeItemIcon_Selected);
    }

    // maybe it doesn't have the specific image we want,
    // try the default one instead
    if ( image == NO_IMAGE ) image = GetImage();

    return image;
}

// -----------------------------------------------------------------------------
// wxGenericTreeCtrl implementation
// -----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxGenericTreeCtrl, wxScrolledWindow)

BEGIN_EVENT_TABLE(wxGenericTreeCtrl,wxScrolledWindow)
    EVT_PAINT          (wxGenericTreeCtrl::OnPaint)
    EVT_MOUSE_EVENTS   (wxGenericTreeCtrl::OnMouse)
    EVT_CHAR           (wxGenericTreeCtrl::OnChar)
    EVT_SET_FOCUS      (wxGenericTreeCtrl::OnSetFocus)
    EVT_KILL_FOCUS     (wxGenericTreeCtrl::OnKillFocus)
    EVT_IDLE           (wxGenericTreeCtrl::OnIdle)
END_EVENT_TABLE()

#if !defined(__WXMSW__) || defined(__WIN16__) || defined(__WXUNIVERSAL__)
/*
 * wxTreeCtrl has to be a real class or we have problems with
 * the run-time information.
 */

IMPLEMENT_DYNAMIC_CLASS(wxTreeCtrl, wxGenericTreeCtrl)
#endif

// -----------------------------------------------------------------------------
// construction/destruction
// -----------------------------------------------------------------------------

void wxGenericTreeCtrl::Init()
{
    m_current = m_key_current = m_anchor = (wxGenericTreeItem *) NULL;
    m_hasFocus = FALSE;
    m_dirty = FALSE;

    m_lineHeight = 10;
    m_indent = 15;
    m_spacing = 18;

    m_hilightBrush = new wxBrush
                         (
                            wxSystemSettings::GetColour
                            (
                                wxSYS_COLOUR_HIGHLIGHT
                            ),
                            wxSOLID
                         );

    m_hilightUnfocusedBrush = new wxBrush
                              (
                                 wxSystemSettings::GetColour
                                 (
                                     wxSYS_COLOUR_BTNSHADOW
                                 ),
                                 wxSOLID
                              );

    m_imageListNormal = m_imageListButtons =
    m_imageListState = (wxImageList *) NULL;
    m_ownsImageListNormal = m_ownsImageListButtons =
    m_ownsImageListState = FALSE;

    m_dragCount = 0;
    m_isDragging = FALSE;
    m_dropTarget = m_oldSelection = (wxGenericTreeItem *)NULL;
    m_textCtrl = NULL;

    m_renameTimer = NULL;
    m_findTimer = NULL;

    m_lastOnSame = FALSE;

    m_normalFont = wxSystemSettings::GetFont( wxSYS_DEFAULT_GUI_FONT );
    m_boldFont = wxFont(m_normalFont.GetPointSize(),
                        m_normalFont.GetFamily(),
                        m_normalFont.GetStyle(),
                        wxBOLD,
                        m_normalFont.GetUnderlined(),
                        m_normalFont.GetFaceName(),
                        m_normalFont.GetEncoding());
}

bool wxGenericTreeCtrl::Create(wxWindow *parent,
                               wxWindowID id,
                               const wxPoint& pos,
                               const wxSize& size,
                               long style,
                               const wxValidator &validator,
                               const wxString& name )
{
#ifdef __WXMAC__
    int major,minor;
    wxGetOsVersion( &major, &minor );

    if (style & wxTR_HAS_BUTTONS) style |= wxTR_MAC_BUTTONS;
    if (style & wxTR_HAS_BUTTONS) style &= ~wxTR_HAS_BUTTONS;
    style &= ~wxTR_LINES_AT_ROOT;
    style |= wxTR_NO_LINES;
    if (major < 10)
        style |= wxTR_ROW_LINES;
    if (major >= 10)
        style |= wxTR_AQUA_BUTTONS;
#endif

    if (style & wxTR_AQUA_BUTTONS)
    {
        m_arrowRight = new wxBitmap( aqua_arrow_right );
        m_arrowDown = new wxBitmap( aqua_arrow_down );
    }
    else
    {
        m_arrowRight = NULL;
        m_arrowDown = NULL;
    }

    wxScrolledWindow::Create( parent, id, pos, size,
                              style|wxHSCROLL|wxVSCROLL, name );

    // If the tree display has no buttons, but does have
    // connecting lines, we can use a narrower layout.
    // It may not be a good idea to force this...
    if (!HasButtons() && !HasFlag(wxTR_NO_LINES))
    {
        m_indent= 10;
        m_spacing = 10;
    }

#if wxUSE_VALIDATORS
    SetValidator( validator );
#endif

    SetForegroundColour( wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT) );
    SetBackgroundColour( wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX) );

//  m_dottedPen = wxPen( "grey", 0, wxDOT );  too slow under XFree86
    m_dottedPen = wxPen( wxT("grey"), 0, 0 );

    return TRUE;
}

wxGenericTreeCtrl::~wxGenericTreeCtrl()
{
    delete m_hilightBrush;
    delete m_hilightUnfocusedBrush;

    delete m_arrowRight;
    delete m_arrowDown;

    DeleteAllItems();

    delete m_renameTimer;
    delete m_findTimer;

    if (m_ownsImageListNormal)
        delete m_imageListNormal;
    if (m_ownsImageListState)
        delete m_imageListState;
    if (m_ownsImageListButtons)
        delete m_imageListButtons;
}

// -----------------------------------------------------------------------------
// accessors
// -----------------------------------------------------------------------------

size_t wxGenericTreeCtrl::GetCount() const
{
    return m_anchor == NULL ? 0u : m_anchor->GetChildrenCount();
}

void wxGenericTreeCtrl::SetIndent(unsigned int indent)
{
    m_indent = (unsigned short) indent;
    m_dirty = TRUE;
}

void wxGenericTreeCtrl::SetSpacing(unsigned int spacing)
{
    m_spacing = (unsigned short) spacing;
    m_dirty = TRUE;
}

size_t wxGenericTreeCtrl::GetChildrenCount(const wxTreeItemId& item, bool recursively)
{
    wxCHECK_MSG( item.IsOk(), 0u, wxT("invalid tree item") );

    return ((wxGenericTreeItem*) item.m_pItem)->GetChildrenCount(recursively);
}

void wxGenericTreeCtrl::SetWindowStyle(const long styles)
{
    if (!HasFlag(wxTR_HIDE_ROOT) && (styles & wxTR_HIDE_ROOT))
    {
        // if we will hide the root, make sure children are visible
        m_anchor->SetHasPlus();
        m_anchor->Expand();
        CalculatePositions();
    }

    // right now, just sets the styles.  Eventually, we may
    // want to update the inherited styles, but right now
    // none of the parents has updatable styles
    m_windowStyle = styles;
    m_dirty = TRUE;
}

// -----------------------------------------------------------------------------
// functions to work with tree items
// -----------------------------------------------------------------------------

wxString wxGenericTreeCtrl::GetItemText(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxT(""), wxT("invalid tree item") );

    return ((wxGenericTreeItem*) item.m_pItem)->GetText();
}

int wxGenericTreeCtrl::GetItemImage(const wxTreeItemId& item,
                             wxTreeItemIcon which) const
{
    wxCHECK_MSG( item.IsOk(), -1, wxT("invalid tree item") );

    return ((wxGenericTreeItem*) item.m_pItem)->GetImage(which);
}

wxTreeItemData *wxGenericTreeCtrl::GetItemData(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), NULL, wxT("invalid tree item") );

    return ((wxGenericTreeItem*) item.m_pItem)->GetData();
}

wxColour wxGenericTreeCtrl::GetItemTextColour(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxNullColour, wxT("invalid tree item") );

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    return pItem->Attr().GetTextColour();
}

wxColour wxGenericTreeCtrl::GetItemBackgroundColour(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxNullColour, wxT("invalid tree item") );

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    return pItem->Attr().GetBackgroundColour();
}

wxFont wxGenericTreeCtrl::GetItemFont(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxNullFont, wxT("invalid tree item") );

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    return pItem->Attr().GetFont();
}

void wxGenericTreeCtrl::SetItemText(const wxTreeItemId& item, const wxString& text)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    wxClientDC dc(this);
    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    pItem->SetText(text);
    CalculateSize(pItem, dc);
    RefreshLine(pItem);
}

void wxGenericTreeCtrl::SetItemImage(const wxTreeItemId& item,
                              int image,
                              wxTreeItemIcon which)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    pItem->SetImage(image, which);

    wxClientDC dc(this);
    CalculateSize(pItem, dc);
    RefreshLine(pItem);
}

void wxGenericTreeCtrl::SetItemData(const wxTreeItemId& item, wxTreeItemData *data)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    ((wxGenericTreeItem*) item.m_pItem)->SetData(data);
}

void wxGenericTreeCtrl::SetItemHasChildren(const wxTreeItemId& item, bool has)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    pItem->SetHasPlus(has);
    RefreshLine(pItem);
}

void wxGenericTreeCtrl::SetItemBold(const wxTreeItemId& item, bool bold)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    // avoid redrawing the tree if no real change
    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    if ( pItem->IsBold() != bold )
    {
        pItem->SetBold(bold);
        RefreshLine(pItem);
    }
}

void wxGenericTreeCtrl::SetItemTextColour(const wxTreeItemId& item,
                                   const wxColour& col)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    pItem->Attr().SetTextColour(col);
    RefreshLine(pItem);
}

void wxGenericTreeCtrl::SetItemBackgroundColour(const wxTreeItemId& item,
                                         const wxColour& col)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    pItem->Attr().SetBackgroundColour(col);
    RefreshLine(pItem);
}

void wxGenericTreeCtrl::SetItemFont(const wxTreeItemId& item, const wxFont& font)
{
    wxCHECK_RET( item.IsOk(), wxT("invalid tree item") );

    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    pItem->Attr().SetFont(font);
    RefreshLine(pItem);
}

bool wxGenericTreeCtrl::SetFont( const wxFont &font )
{
    wxScrolledWindow::SetFont(font);

    m_normalFont = font ;
    m_boldFont = wxFont(m_normalFont.GetPointSize(),
                        m_normalFont.GetFamily(),
                        m_normalFont.GetStyle(),
                        wxBOLD,
                        m_normalFont.GetUnderlined(),
                        m_normalFont.GetFaceName(),
                        m_normalFont.GetEncoding());

    return TRUE;
}


// -----------------------------------------------------------------------------
// item status inquiries
// -----------------------------------------------------------------------------

bool wxGenericTreeCtrl::IsVisible(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), FALSE, wxT("invalid tree item") );

    // An item is only visible if it's not a descendant of a collapsed item
    wxGenericTreeItem *pItem = (wxGenericTreeItem*) item.m_pItem;
    wxGenericTreeItem* parent = pItem->GetParent();
    while (parent)
    {
        if (!parent->IsExpanded())
            return FALSE;
        parent = parent->GetParent();
    }

    int startX, startY;
    GetViewStart(& startX, & startY);

    wxSize clientSize = GetClientSize();

    wxRect rect;
    if (!GetBoundingRect(item, rect))
        return FALSE;
    if (rect.GetWidth() == 0 || rect.GetHeight() == 0)
        return FALSE;
    if (rect.GetBottom() < 0 || rect.GetTop() > clientSize.y)
        return FALSE;
    if (rect.GetRight() < 0 || rect.GetLeft() > clientSize.x)
        return FALSE;

    return TRUE;
}

bool wxGenericTreeCtrl::ItemHasChildren(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), FALSE, wxT("invalid tree item") );

    // consider that the item does have children if it has the "+" button: it
    // might not have them (if it had never been expanded yet) but then it
    // could have them as well and it's better to err on this side rather than
    // disabling some operations which are restricted to the items with
    // children for an item which does have them
    return ((wxGenericTreeItem*) item.m_pItem)->HasPlus();
}

bool wxGenericTreeCtrl::IsExpanded(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), FALSE, wxT("invalid tree item") );

    return ((wxGenericTreeItem*) item.m_pItem)->IsExpanded();
}

bool wxGenericTreeCtrl::IsSelected(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), FALSE, wxT("invalid tree item") );

    return ((wxGenericTreeItem*) item.m_pItem)->IsSelected();
}

bool wxGenericTreeCtrl::IsBold(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), FALSE, wxT("invalid tree item") );

    return ((wxGenericTreeItem*) item.m_pItem)->IsBold();
}

// -----------------------------------------------------------------------------
// navigation
// -----------------------------------------------------------------------------

wxTreeItemId wxGenericTreeCtrl::GetItemParent(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );

    return ((wxGenericTreeItem*) item.m_pItem)->GetParent();
}

wxTreeItemId wxGenericTreeCtrl::GetFirstChild(const wxTreeItemId& item, long& cookie) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );

    cookie = 0;
    return GetNextChild(item, cookie);
}

wxTreeItemId wxGenericTreeCtrl::GetNextChild(const wxTreeItemId& item, long& cookie) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );

    wxArrayGenericTreeItems& children = ((wxGenericTreeItem*) item.m_pItem)->GetChildren();
    if ( (size_t)cookie < children.Count() )
    {
        return children.Item((size_t)cookie++);
    }
    else
    {
        // there are no more of them
        return wxTreeItemId();
    }
}

wxTreeItemId wxGenericTreeCtrl::GetLastChild(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );

    wxArrayGenericTreeItems& children = ((wxGenericTreeItem*) item.m_pItem)->GetChildren();
    return (children.IsEmpty() ? wxTreeItemId() : wxTreeItemId(children.Last()));
}

wxTreeItemId wxGenericTreeCtrl::GetNextSibling(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );

    wxGenericTreeItem *i = (wxGenericTreeItem*) item.m_pItem;
    wxGenericTreeItem *parent = i->GetParent();
    if ( parent == NULL )
    {
        // root item doesn't have any siblings
        return wxTreeItemId();
    }

    wxArrayGenericTreeItems& siblings = parent->GetChildren();
    int index = siblings.Index(i);
    wxASSERT( index != wxNOT_FOUND ); // I'm not a child of my parent?

    size_t n = (size_t)(index + 1);
    return n == siblings.Count() ? wxTreeItemId() : wxTreeItemId(siblings[n]);
}

wxTreeItemId wxGenericTreeCtrl::GetPrevSibling(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );

    wxGenericTreeItem *i = (wxGenericTreeItem*) item.m_pItem;
    wxGenericTreeItem *parent = i->GetParent();
    if ( parent == NULL )
    {
        // root item doesn't have any siblings
        return wxTreeItemId();
    }

    wxArrayGenericTreeItems& siblings = parent->GetChildren();
    int index = siblings.Index(i);
    wxASSERT( index != wxNOT_FOUND ); // I'm not a child of my parent?

    return index == 0 ? wxTreeItemId()
                      : wxTreeItemId(siblings[(size_t)(index - 1)]);
}

// Only for internal use right now, but should probably be public
wxTreeItemId wxGenericTreeCtrl::GetNext(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );

    wxGenericTreeItem *i = (wxGenericTreeItem*) item.m_pItem;

    // First see if there are any children.
    wxArrayGenericTreeItems& children = i->GetChildren();
    if (children.GetCount() > 0)
    {
         return children.Item(0);
    }
    else
    {
         // Try a sibling of this or ancestor instead
         wxTreeItemId p = item;
         wxTreeItemId toFind;
         do
         {
              toFind = GetNextSibling(p);
              p = GetItemParent(p);
         } while (p.IsOk() && !toFind.IsOk());
         return toFind;
    }
}

wxTreeItemId wxGenericTreeCtrl::GetFirstVisibleItem() const
{
    wxTreeItemId id = GetRootItem();
    if (!id.IsOk())
        return id;

    do
    {
        if (IsVisible(id))
              return id;
        id = GetNext(id);
    } while (id.IsOk());

    return wxTreeItemId();
}

wxTreeItemId wxGenericTreeCtrl::GetNextVisible(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );

    wxTreeItemId id = item;
    if (id.IsOk())
    {
        while (id = GetNext(id), id.IsOk())
        {
            if (IsVisible(id))
                return id;
        }
    }
    return wxTreeItemId();
}

wxTreeItemId wxGenericTreeCtrl::GetPrevVisible(const wxTreeItemId& item) const
{
    wxCHECK_MSG( item.IsOk(), wxTreeItemId(), wxT("invalid tree item") );

    wxFAIL_MSG(wxT("not implemented"));

    return wxTreeItemId();
}

// called by wxTextTreeCtrl when it marks itself for deletion
void wxGenericTreeCtrl::ResetTextControl()
{
  m_textCtrl = NULL;
}

// find the first item starting with the given prefix after the given item
wxTreeItemId wxGenericTreeCtrl::FindItem(const wxTreeItemId& idParent,
                                         const wxString& prefixOrig) const
{
    // match is case insensitive as this is more convenient to the user: having
    // to press Shift-letter to go to the item starting with a capital letter
    // would be too bothersome
    wxString prefix = prefixOrig.Lower();

    // determine the starting point: we shouldn't take the current item (this
    // allows to switch between two items starting with the same letter just by
    // pressing it) but we shouldn't jump to the next one if the user is
    // continuing to type as otherwise he might easily skip the item he wanted
    wxTreeItemId id = idParent;
    if ( prefix.length() == 1 )
    {
        id = GetNext(id);
    }

    // look for the item starting with the given prefix after it
    while ( id.IsOk() && !GetItemText(id).Lower().StartsWith(prefix) )
    {
        id = GetNext(id);
    }

    // if we haven't found anything...
    if ( !id.IsOk() )
    {
        // ... wrap to the beginning
        id = GetRootItem();
        if ( HasFlag(wxTR_HIDE_ROOT) )
        {
            // can't select virtual root
            id = GetNext(id);
        }

        // and try all the items (stop when we get to the one we started from)
        while ( id != idParent && !GetItemText(id).Lower().StartsWith(prefix) )
        {
            id = GetNext(id);
        }
    }

    return id;
}

// -----------------------------------------------------------------------------
// operations
// -----------------------------------------------------------------------------

wxTreeItemId wxGenericTreeCtrl::DoInsertItem(const wxTreeItemId& parentId,
                                      size_t previous,
                                      const wxString& text,
                                      int image, int selImage,
                                      wxTreeItemData *data)
{
    wxGenericTreeItem *parent = (wxGenericTreeItem*) parentId.m_pItem;
    if ( !parent )
    {
        // should we give a warning here?
        return AddRoot(text, image, selImage, data);
    }

    m_dirty = TRUE;     // do this first so stuff below doesn't cause flicker

    wxGenericTreeItem *item =
        new wxGenericTreeItem( parent, text, image, selImage, data );

    if ( data != NULL )
    {
        data->m_pItem = (long) item;
    }

    parent->Insert( item, previous );

    return item;
}

wxTreeItemId wxGenericTreeCtrl::AddRoot(const wxString& text,
                                 int image, int selImage,
                                 wxTreeItemData *data)
{
    wxCHECK_MSG( !m_anchor, wxTreeItemId(), wxT("tree can have only one root") );

    m_dirty = TRUE;     // do this first so stuff below doesn't cause flicker

    m_anchor = new wxGenericTreeItem((wxGenericTreeItem *)NULL, text,
                                   image, selImage, data);
    if ( data != NULL )
    {
        data->m_pItem = (long) m_anchor;
    }

    if (HasFlag(wxTR_HIDE_ROOT))
    {
        // if root is hidden, make sure we can navigate
        // into children
        m_anchor->SetHasPlus();
        m_anchor->Expand();
        CalculatePositions();
    }

    if (!HasFlag(wxTR_MULTIPLE))
    {
        m_current = m_key_current = m_anchor;
        m_current->SetHilight( TRUE );
    }

    return m_anchor;
}

wxTreeItemId wxGenericTreeCtrl::PrependItem(const wxTreeItemId& parent,
                                     const wxString& text,
                                     int image, int selImage,
                                     wxTreeItemData *data)
{
    return DoInsertItem(parent, 0u, text, image, selImage, data);
}

wxTreeItemId wxGenericTreeCtrl::InsertItem(const wxTreeItemId& parentId,
                                    const wxTreeItemId& idPrevious,
                                    const wxString& text,
                                    int image, int selImage,
                                    wxTreeItemData *data)
{
    wxGenericTreeItem *parent = (wxGenericTreeItem*) parentId.m_pItem;
    if ( !parent )
    {
        // should we give a warning here?
        return AddRoot(text, image, selImage, data);
    }

    int index = -1;
    if (idPrevious.IsOk())
    {
        index = parent->GetChildren().Index((wxGenericTreeItem*) idPrevious.m_pItem);
        wxASSERT_MSG( index != wxNOT_FOUND,
                      wxT("previous item in wxGenericTreeCtrl::InsertItem() is not a sibling") );
    }

    return DoInsertItem(parentId, (size_t)++index, text, image, selImage, data);
}

wxTreeItemId wxGenericTreeCtrl::InsertItem(const wxTreeItemId& parentId,
                                    size_t before,
                                    const wxString& text,
                                    int image, int selImage,
                                    wxTreeItemData *data)
{
    wxGenericTreeItem *parent = (wxGenericTreeItem*) parentId.m_pItem;
    if ( !parent )
    {
        // should we give a warning here?
        return AddRoot(text, image, selImage, data);
    }

    return DoInsertItem(parentId, before, text, image, selImage, data);
}

wxTreeItemId wxGenericTreeCtrl::AppendItem(const wxTreeItemId& parentId,
                                    const wxString& text,
                                    int image, int selImage,
                                    wxTreeItemData *data)
{
    wxGenericTreeItem *parent = (wxGenericTreeItem*) parentId.m_pItem;
    if ( !parent )
    {
        // should we give a warning here?
        return AddRoot(text, image, selImage, data);
    }

    return DoInsertItem( parent, parent->GetChildren().Count(), text,
                         image, selImage, data);
}

void wxGenericTreeCtrl::SendDeleteEvent(wxGenericTreeItem *item)
{
    wxTreeEvent event( wxEVT_COMMAND_TREE_DELETE_ITEM, GetId() );
    event.m_item = (long) item;
    event.SetEventObject( this );
    ProcessEvent( event );
}

void wxGenericTreeCtrl::DeleteChildren(const wxTreeItemId& itemId)
{
    m_dirty = TRUE;     // do this first so stuff below doesn't cause flicker

    wxGenericTreeItem *item = (wxGenericTreeItem*) itemId.m_pItem;
    item->DeleteChildren(this);
}

void wxGenericTreeCtrl::Delete(const wxTreeItemId& itemId)
{
    m_dirty = TRUE;     // do this first so stuff below doesn't cause flicker

    wxGenericTreeItem *item = (wxGenericTreeItem*) itemId.m_pItem;

    wxGenericTreeItem *parent = item->GetParent();

    // don't keep stale pointers around!
    if ( IsDescendantOf(item, m_key_current) )
    {
        m_key_current = parent;
    }

    if ( IsDescendantOf(item, m_current) )
    {
        m_current = parent;
    }

    // remove the item from the tree
    if ( parent )
    {
        parent->GetChildren().Remove( item );  // remove by value
    }
    else // deleting the root
    {
        // nothing will be left in the tree
        m_anchor = NULL;
    }

    // and delete all of its children and the item itself now
    item->DeleteChildren(this);
    SendDeleteEvent(item);
    delete item;
}

void wxGenericTreeCtrl::DeleteAllItems()
{
    if ( m_anchor )
    {
        Delete(m_anchor);
    }
}

void wxGenericTreeCtrl::Expand(const wxTreeItemId& itemId)
{
    wxGenericTreeItem *item = (wxGenericTreeItem*) itemId.m_pItem;

    wxCHECK_RET( item, _T("invalid item in wxGenericTreeCtrl::Expand") );
    wxCHECK_RET( !HasFlag(wxTR_HIDE_ROOT) || itemId != GetRootItem(),
                 _T("can't expand hidden root") );

    if ( !item->HasPlus() )
        return;

    if ( item->IsExpanded() )
        return;

    wxTreeEvent event( wxEVT_COMMAND_TREE_ITEM_EXPANDING, GetId() );
    event.m_item = (long) item;
    event.SetEventObject( this );

    if ( ProcessEvent( event ) && !event.IsAllowed() )
    {
        // cancelled by program
        return;
    }

    item->Expand();
    CalculatePositions();

    RefreshSubtree(item);

    event.SetEventType(wxEVT_COMMAND_TREE_ITEM_EXPANDED);
    ProcessEvent( event );
}

void wxGenericTreeCtrl::ExpandAll(const wxTreeItemId& item)
{
    if ( !HasFlag(wxTR_HIDE_ROOT) || item != GetRootItem())
    {
        Expand(item);
        if ( !IsExpanded(item) )
            return;
    }

    long cookie;
    wxTreeItemId child = GetFirstChild(item, cookie);
    while ( child.IsOk() )
    {
        ExpandAll(child);

        child = GetNextChild(item, cookie);
    }
}

void wxGenericTreeCtrl::Collapse(const wxTreeItemId& itemId)
{
    wxCHECK_RET( !HasFlag(wxTR_HIDE_ROOT) || itemId != GetRootItem(),
                 _T("can't collapse hidden root") );

    wxGenericTreeItem *item = (wxGenericTreeItem*) itemId.m_pItem;

    if ( !item->IsExpanded() )
        return;

    wxTreeEvent event( wxEVT_COMMAND_TREE_ITEM_COLLAPSING, GetId() );
    event.m_item = (long) item;
    event.SetEventObject( this );
    if ( ProcessEvent( event ) && !event.IsAllowed() )
    {
        // cancelled by program
        return;
    }

    item->Collapse();

#if 0  // TODO why should items be collapsed recursively?
    wxArrayGenericTreeItems& children = item->GetChildren();
    size_t count = children.Count();
    for ( size_t n = 0; n < count; n++ )
    {
        Collapse(children[n]);
    }
#endif

    CalculatePositions();

    RefreshSubtree(item);

    event.SetEventType(wxEVT_COMMAND_TREE_ITEM_COLLAPSED);
    ProcessEvent( event );
}

void wxGenericTreeCtrl::CollapseAndReset(const wxTreeItemId& item)
{
    Collapse(item);
    DeleteChildren(item);
}

void wxGenericTreeCtrl::Toggle(const wxTreeItemId& itemId)
{
    wxGenericTreeItem *item = (wxGenericTreeItem*) itemId.m_pItem;

    if (item->IsExpanded())
        Collapse(itemId);
    else
        Expand(itemId);
}

void wxGenericTreeCtrl::Unselect()
{
    if (m_current)
    {
        m_current->SetHilight( FALSE );
        RefreshLine( m_current );

        m_current = NULL;
    }
}

void wxGenericTreeCtrl::UnselectAllChildren(wxGenericTreeItem *item)
{
    if (item->IsSelected())
    {
        item->SetHilight(FALSE);
        RefreshLine(item);
    }

    if (item->HasChildren())
    {
        wxArrayGenericTreeItems& children = item->GetChildren();
        size_t count = children.Count();
        for ( size_t n = 0; n < count; ++n )
        {
            UnselectAllChildren(children[n]);
        }
    }
}

void wxGenericTreeCtrl::UnselectAll()
{
    wxTreeItemId rootItem = GetRootItem();

    // the tree might not have the root item at all
    if ( rootItem )
    {
        UnselectAllChildren((wxGenericTreeItem*) rootItem.m_pItem);
    }
}

// Recursive function !
// To stop we must have crt_item<last_item
// Algorithm :
// Tag all next children, when no more children,
// Move to parent (not to tag)
// Keep going... if we found last_item, we stop.
bool wxGenericTreeCtrl::TagNextChildren(wxGenericTreeItem *crt_item, wxGenericTreeItem *last_item, bool select)
{
    wxGenericTreeItem *parent = crt_item->GetParent();

    if (parent == NULL) // This is root item
        return TagAllChildrenUntilLast(crt_item, last_item, select);

    wxArrayGenericTreeItems& children = parent->GetChildren();
    int index = children.Index(crt_item);
    wxASSERT( index != wxNOT_FOUND ); // I'm not a child of my parent?

    size_t count = children.Count();
    for (size_t n=(size_t)(index+1); n<count; ++n)
    {
        if (TagAllChildrenUntilLast(children[n], last_item, select)) return TRUE;
    }

    return TagNextChildren(parent, last_item, select);
}

bool wxGenericTreeCtrl::TagAllChildrenUntilLast(wxGenericTreeItem *crt_item, wxGenericTreeItem *last_item, bool select)
{
    crt_item->SetHilight(select);
    RefreshLine(crt_item);

    if (crt_item==last_item)
        return TRUE;

    if (crt_item->HasChildren())
    {
        wxArrayGenericTreeItems& children = crt_item->GetChildren();
        size_t count = children.Count();
        for ( size_t n = 0; n < count; ++n )
        {
            if (TagAllChildrenUntilLast(children[n], last_item, select))
                return TRUE;
        }
    }

  return FALSE;
}

void wxGenericTreeCtrl::SelectItemRange(wxGenericTreeItem *item1, wxGenericTreeItem *item2)
{
    // item2 is not necessary after item1
    wxGenericTreeItem *first=NULL, *last=NULL;

    // choice first' and 'last' between item1 and item2
    if (item1->GetY()<item2->GetY())
    {
        first=item1;
        last=item2;
    }
    else
    {
        first=item2;
        last=item1;
    }

    bool select = m_current->IsSelected();

    if ( TagAllChildrenUntilLast(first,last,select) )
        return;

    TagNextChildren(first,last,select);
}

void wxGenericTreeCtrl::SelectItem(const wxTreeItemId& itemId,
                                   bool unselect_others,
                                   bool extended_select)
{
    wxCHECK_RET( itemId.IsOk(), wxT("invalid tree item") );

    bool is_single=!(GetWindowStyleFlag() & wxTR_MULTIPLE);
    wxGenericTreeItem *item = (wxGenericTreeItem*) itemId.m_pItem;

    //wxCHECK_RET( ( (!unselect_others) && is_single),
    //           wxT("this is a single selection tree") );

    // to keep going anyhow !!!
    if (is_single)
    {
        if (item->IsSelected())
            return; // nothing to do
        unselect_others = TRUE;
        extended_select = FALSE;
    }
    else if ( unselect_others && item->IsSelected() )
    {
        // selection change if there is more than one item currently selected
        wxArrayTreeItemIds selected_items;
        if ( GetSelections(selected_items) == 1 )
            return;
    }

    wxTreeEvent event( wxEVT_COMMAND_TREE_SEL_CHANGING, GetId() );
    event.m_item = (long) item;
    event.m_itemOld = (long) m_current;
    event.SetEventObject( this );
    // TODO : Here we don't send any selection mode yet !

    if ( GetEventHandler()->ProcessEvent( event ) && !event.IsAllowed() )
        return;

    wxTreeItemId parent = GetItemParent( itemId );
    while (parent.IsOk())
    {
        if (!IsExpanded(parent))
            Expand( parent );

        parent = GetItemParent( parent );
    }

    EnsureVisible( itemId );

    // ctrl press
    if (unselect_others)
    {
        if (is_single) Unselect(); // to speed up thing
        else UnselectAll();
    }

    // shift press
    if (extended_select)
    {
        if ( !m_current )
        {
            m_current = m_key_current = (wxGenericTreeItem*) GetRootItem().m_pItem;
        }

        // don't change the mark (m_current)
        SelectItemRange(m_current, item);
    }
    else
    {
        bool select=TRUE; // the default

        // Check if we need to toggle hilight (ctrl mode)
        if (!unselect_others)
            select=!item->IsSelected();

        m_current = m_key_current = item;
        m_current->SetHilight(select);
        RefreshLine( m_current );
    }

    event.SetEventType(wxEVT_COMMAND_TREE_SEL_CHANGED);
    GetEventHandler()->ProcessEvent( event );
}

void wxGenericTreeCtrl::FillArray(wxGenericTreeItem *item,
                                  wxArrayTreeItemIds &array) const
{
    if ( item->IsSelected() )
        array.Add(wxTreeItemId(item));

    if ( item->HasChildren() )
    {
        wxArrayGenericTreeItems& children = item->GetChildren();
        size_t count = children.GetCount();
        for ( size_t n = 0; n < count; ++n )
            FillArray(children[n], array);
    }
}

size_t wxGenericTreeCtrl::GetSelections(wxArrayTreeItemIds &array) const
{
    array.Empty();
    wxTreeItemId idRoot = GetRootItem();
    if ( idRoot.IsOk() )
    {
        FillArray((wxGenericTreeItem*) idRoot.m_pItem, array);
    }
    //else: the tree is empty, so no selections

    return array.Count();
}

void wxGenericTreeCtrl::EnsureVisible(const wxTreeItemId& item)
{
    if (!item.IsOk()) return;

    wxGenericTreeItem *gitem = (wxGenericTreeItem*) item.m_pItem;

    // first expand all parent branches
    wxGenericTreeItem *parent = gitem->GetParent();

    if ( HasFlag(wxTR_HIDE_ROOT) )
    {
        while ( parent != m_anchor )
        {
            Expand(parent);
            parent = parent->GetParent();
        }
    }
    else
    {
        while ( parent )
        {
            Expand(parent);
            parent = parent->GetParent();
        }
    }

    //if (parent) CalculatePositions();

    ScrollTo(item);
}

void wxGenericTreeCtrl::ScrollTo(const wxTreeItemId &item)
{
    if (!item.IsOk()) return;

    // We have to call this here because the label in
    // question might just have been added and no screen
    // update taken place.
    if (m_dirty) wxYieldIfNeeded();

    wxGenericTreeItem *gitem = (wxGenericTreeItem*) item.m_pItem;

    // now scroll to the item
    int item_y = gitem->GetY();

    int start_x = 0;
    int start_y = 0;
    GetViewStart( &start_x, &start_y );
    start_y *= PIXELS_PER_UNIT;

    int client_h = 0;
    int client_w = 0;
    GetClientSize( &client_w, &client_h );

    if (item_y < start_y+3)
    {
        // going down
        int x = 0;
        int y = 0;
        m_anchor->GetSize( x, y, this );
        y += PIXELS_PER_UNIT+2; // one more scrollbar unit + 2 pixels
        x += PIXELS_PER_UNIT+2; // one more scrollbar unit + 2 pixels
        int x_pos = GetScrollPos( wxHORIZONTAL );
        // Item should appear at top
        SetScrollbars( PIXELS_PER_UNIT, PIXELS_PER_UNIT, x/PIXELS_PER_UNIT, y/PIXELS_PER_UNIT, x_pos, item_y/PIXELS_PER_UNIT );
    }
    else if (item_y+GetLineHeight(gitem) > start_y+client_h)
    {
        // going up
        int x = 0;
        int y = 0;
        m_anchor->GetSize( x, y, this );
        y += PIXELS_PER_UNIT+2; // one more scrollbar unit + 2 pixels
        x += PIXELS_PER_UNIT+2; // one more scrollbar unit + 2 pixels
        item_y += PIXELS_PER_UNIT+2;
        int x_pos = GetScrollPos( wxHORIZONTAL );
        // Item should appear at bottom
        SetScrollbars( PIXELS_PER_UNIT, PIXELS_PER_UNIT, x/PIXELS_PER_UNIT, y/PIXELS_PER_UNIT, x_pos, (item_y+GetLineHeight(gitem)-client_h)/PIXELS_PER_UNIT );
    }
}

// FIXME: tree sorting functions are not reentrant and not MT-safe!
static wxGenericTreeCtrl *s_treeBeingSorted = NULL;

static int LINKAGEMODE tree_ctrl_compare_func(wxGenericTreeItem **item1,
                                  wxGenericTreeItem **item2)
{
    wxCHECK_MSG( s_treeBeingSorted, 0, wxT("bug in wxGenericTreeCtrl::SortChildren()") );

    return s_treeBeingSorted->OnCompareItems(*item1, *item2);
}

int wxGenericTreeCtrl::OnCompareItems(const wxTreeItemId& item1,
                               const wxTreeItemId& item2)
{
    return wxStrcmp(GetItemText(item1), GetItemText(item2));
}

void wxGenericTreeCtrl::SortChildren(const wxTreeItemId& itemId)
{
    wxCHECK_RET( itemId.IsOk(), wxT("invalid tree item") );

    wxGenericTreeItem *item = (wxGenericTreeItem*) itemId.m_pItem;

    wxCHECK_RET( !s_treeBeingSorted,
                 wxT("wxGenericTreeCtrl::SortChildren is not reentrant") );

    wxArrayGenericTreeItems& children = item->GetChildren();
    if ( children.Count() > 1 )
    {
        m_dirty = TRUE;

        s_treeBeingSorted = this;
        children.Sort(tree_ctrl_compare_func);
        s_treeBeingSorted = NULL;
    }
    //else: don't make the tree dirty as nothing changed
}

wxImageList *wxGenericTreeCtrl::GetImageList() const
{
    return m_imageListNormal;
}

wxImageList *wxGenericTreeCtrl::GetButtonsImageList() const
{
    return m_imageListButtons;
}

wxImageList *wxGenericTreeCtrl::GetStateImageList() const
{
    return m_imageListState;
}

void wxGenericTreeCtrl::CalculateLineHeight()
{
    wxClientDC dc(this);
    m_lineHeight = (int)(dc.GetCharHeight() + 4);

    if ( m_imageListNormal )
    {
        // Calculate a m_lineHeight value from the normal Image sizes.
        // May be toggle off. Then wxGenericTreeCtrl will spread when
        // necessary (which might look ugly).
        int n = m_imageListNormal->GetImageCount();
        for (int i = 0; i < n ; i++)
        {
            int width = 0, height = 0;
            m_imageListNormal->GetSize(i, width, height);
            if (height > m_lineHeight) m_lineHeight = height;
        }
    }

    if (m_imageListButtons)
    {
        // Calculate a m_lineHeight value from the Button image sizes.
        // May be toggle off. Then wxGenericTreeCtrl will spread when
        // necessary (which might look ugly).
        int n = m_imageListButtons->GetImageCount();
        for (int i = 0; i < n ; i++)
        {
            int width = 0, height = 0;
            m_imageListButtons->GetSize(i, width, height);
            if (height > m_lineHeight) m_lineHeight = height;
        }
    }

    if (m_lineHeight < 30)
        m_lineHeight += 2;                 // at least 2 pixels
    else
        m_lineHeight += m_lineHeight/10;   // otherwise 10% extra spacing
}

void wxGenericTreeCtrl::SetImageList(wxImageList *imageList)
{
    if (m_ownsImageListNormal) delete m_imageListNormal;
    m_imageListNormal = imageList;
    m_ownsImageListNormal = FALSE;
    m_dirty = TRUE;
    // Don't do any drawing if we're setting the list to NULL,
    // since we may be in the process of deleting the tree control.
    if (imageList)
        CalculateLineHeight();
}

void wxGenericTreeCtrl::SetStateImageList(wxImageList *imageList)
{
    if (m_ownsImageListState) delete m_imageListState;
    m_imageListState = imageList;
    m_ownsImageListState = FALSE;
}

void wxGenericTreeCtrl::SetButtonsImageList(wxImageList *imageList)
{
    if (m_ownsImageListButtons) delete m_imageListButtons;
    m_imageListButtons = imageList;
    m_ownsImageListButtons = FALSE;
    m_dirty = TRUE;
    CalculateLineHeight();
}

void wxGenericTreeCtrl::AssignImageList(wxImageList *imageList)
{
    SetImageList(imageList);
    m_ownsImageListNormal = TRUE;
}

void wxGenericTreeCtrl::AssignStateImageList(wxImageList *imageList)
{
    SetStateImageList(imageList);
    m_ownsImageListState = TRUE;
}

void wxGenericTreeCtrl::AssignButtonsImageList(wxImageList *imageList)
{
    SetButtonsImageList(imageList);
    m_ownsImageListButtons = TRUE;
}

// -----------------------------------------------------------------------------
// helpers
// -----------------------------------------------------------------------------

void wxGenericTreeCtrl::AdjustMyScrollbars()
{
    if (m_anchor)
    {
        int x = 0, y = 0;
        m_anchor->GetSize( x, y, this );
        y += PIXELS_PER_UNIT+2; // one more scrollbar unit + 2 pixels
        x += PIXELS_PER_UNIT+2; // one more scrollbar unit + 2 pixels
        int x_pos = GetScrollPos( wxHORIZONTAL );
        int y_pos = GetScrollPos( wxVERTICAL );
        SetScrollbars( PIXELS_PER_UNIT, PIXELS_PER_UNIT, x/PIXELS_PER_UNIT, y/PIXELS_PER_UNIT, x_pos, y_pos );
    }
    else
    {
        SetScrollbars( 0, 0, 0, 0 );
    }
}

int wxGenericTreeCtrl::GetLineHeight(wxGenericTreeItem *item) const
{
    if (GetWindowStyleFlag() & wxTR_HAS_VARIABLE_ROW_HEIGHT)
        return item->GetHeight();
    else
        return m_lineHeight;
}

void wxGenericTreeCtrl::PaintItem(wxGenericTreeItem *item, wxDC& dc)
{
    // TODO implement "state" icon on items

    wxTreeItemAttr *attr = item->GetAttributes();
    if ( attr && attr->HasFont() )
        dc.SetFont(attr->GetFont());
    else if (item->IsBold())
        dc.SetFont(m_boldFont);

    long text_w = 0, text_h = 0;
    dc.GetTextExtent( item->GetText(), &text_w, &text_h );

    int image_h = 0, image_w = 0;
    int image = item->GetCurrentImage();
    if ( image != NO_IMAGE )
    {
        if ( m_imageListNormal )
        {
            m_imageListNormal->GetSize( image, image_w, image_h );
            image_w += 4;
        }
        else
        {
            image = NO_IMAGE;
        }
    }

    int total_h = GetLineHeight(item);

    if ( item->IsSelected() )
    {
        dc.SetBrush(*(m_hasFocus ? m_hilightBrush : m_hilightUnfocusedBrush));
    }
    else
    {
        wxColour colBg;
        if ( attr && attr->HasBackgroundColour() )
            colBg = attr->GetBackgroundColour();
        else
            colBg = m_backgroundColour;
        dc.SetBrush(wxBrush(colBg, wxSOLID));
    }

    int offset = HasFlag(wxTR_ROW_LINES) ? 1 : 0;

    if ( HasFlag(wxTR_FULL_ROW_HIGHLIGHT) )
    {
        int x, y, w, h;

        DoGetPosition(&x, &y);
        DoGetSize(&w, &h);
        dc.DrawRectangle(x, item->GetY()+offset, w, total_h-offset);
    }
    else
    {
        if ( item->IsSelected() && image != NO_IMAGE )
        {
            // If it's selected, and there's an image, then we should
            // take care to leave the area under the image painted in the
            // background colour.
            dc.DrawRectangle( item->GetX() + image_w - 2, item->GetY()+offset,
                              item->GetWidth() - image_w + 2, total_h-offset );
        }
        else
        {
            dc.DrawRectangle( item->GetX()-2, item->GetY()+offset,
                              item->GetWidth()+2, total_h-offset );
        }
    }

    if ( image != NO_IMAGE )
    {
        dc.SetClippingRegion( item->GetX(), item->GetY(), image_w-2, total_h );
        m_imageListNormal->Draw( image, dc,
                                 item->GetX(),
                                 item->GetY() +((total_h > image_h)?((total_h-image_h)/2):0),
                                 wxIMAGELIST_DRAW_TRANSPARENT );
        dc.DestroyClippingRegion();
    }

    dc.SetBackgroundMode(wxTRANSPARENT);
    int extraH = (total_h > text_h) ? (total_h - text_h)/2 : 0;
    dc.DrawText( item->GetText(),
                 (wxCoord)(image_w + item->GetX()),
                 (wxCoord)(item->GetY() + extraH));

    // restore normal font
    dc.SetFont( m_normalFont );
}

// Now y stands for the top of the item, whereas it used to stand for middle !
void wxGenericTreeCtrl::PaintLevel( wxGenericTreeItem *item, wxDC &dc, int level, int &y )
{
    int x = level*m_indent;
    if (!HasFlag(wxTR_HIDE_ROOT))
    {
        x += m_indent;
    }
    else if (level == 0)
    {
        // always expand hidden root
        int origY = y;
        wxArrayGenericTreeItems& children = item->GetChildren();
        int count = children.Count();
        if (count > 0)
        {
            int n = 0, oldY;
            do {
                oldY = y;
                PaintLevel(children[n], dc, 1, y);
            } while (++n < count);

            if (!HasFlag(wxTR_NO_LINES) && HasFlag(wxTR_LINES_AT_ROOT) && count > 0)
            {
                // draw line down to last child
                origY += GetLineHeight(children[0])>>1;
                oldY += GetLineHeight(children[n-1])>>1;
                dc.DrawLine(3, origY, 3, oldY);
            }
        }
        return;
    }

    item->SetX(x+m_spacing);
    item->SetY(y);

    int h = GetLineHeight(item);
    int y_top = y;
    int y_mid = y_top + (h>>1);
    y += h;

    int exposed_x = dc.LogicalToDeviceX(0);
    int exposed_y = dc.LogicalToDeviceY(y_top);

    if (IsExposed(exposed_x, exposed_y, 10000, h))  // 10000 = very much
    {
        wxPen *pen =
#ifndef __WXMAC__
            // don't draw rect outline if we already have the
            // background color under Mac
            (item->IsSelected() && m_hasFocus) ? wxBLACK_PEN :
#endif // !__WXMAC__
            wxTRANSPARENT_PEN;

        wxColour colText;
        if ( item->IsSelected() )
        {
            colText = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT);
        }
        else
        {
            wxTreeItemAttr *attr = item->GetAttributes();
            if (attr && attr->HasTextColour())
                colText = attr->GetTextColour();
            else
                colText = GetForegroundColour();
        }

        // prepare to draw
        dc.SetTextForeground(colText);
        dc.SetPen(*pen);

        // draw
        PaintItem(item, dc);

        if (HasFlag(wxTR_ROW_LINES))
        {
            // if the background colour is white, choose a
            // contrasting color for the lines
            dc.SetPen(*((GetBackgroundColour() == *wxWHITE)
                         ? wxMEDIUM_GREY_PEN : wxWHITE_PEN));
            dc.DrawLine(0, y_top, 10000, y_top);
            dc.DrawLine(0, y, 10000, y);
        }

        // restore DC objects
        dc.SetBrush(*wxWHITE_BRUSH);
        dc.SetPen(m_dottedPen);
        dc.SetTextForeground(*wxBLACK);

        if (item->HasPlus() && HasButtons())  // should the item show a button?
        {
            if (!HasFlag(wxTR_NO_LINES))
            {
                if (x > (signed)m_indent)
                    dc.DrawLine(x - m_indent, y_mid, x - 5, y_mid);
                else if (HasFlag(wxTR_LINES_AT_ROOT))
                    dc.DrawLine(3, y_mid, x - 5, y_mid);
                dc.DrawLine(x + 5, y_mid, x + m_spacing, y_mid);
            }

            if (m_imageListButtons != NULL)
            {
                // draw the image button here
                int image_h = 0, image_w = 0, image = wxTreeItemIcon_Normal;
                if (item->IsExpanded()) image = wxTreeItemIcon_Expanded;
                if (item->IsSelected())
                    image += wxTreeItemIcon_Selected - wxTreeItemIcon_Normal;
                m_imageListButtons->GetSize(image, image_w, image_h);
                int xx = x - (image_w>>1);
                int yy = y_mid - (image_h>>1);
                dc.SetClippingRegion(xx, yy, image_w, image_h);
                m_imageListButtons->Draw(image, dc, xx, yy,
                                         wxIMAGELIST_DRAW_TRANSPARENT);
                dc.DestroyClippingRegion();
            }
            else if (HasFlag(wxTR_TWIST_BUTTONS))
            {
                // draw the twisty button here

                if (HasFlag(wxTR_AQUA_BUTTONS))
                {
                    if (item->IsExpanded())
                        dc.DrawBitmap( *m_arrowDown, x-5, y_mid-6, TRUE );
                    else
                        dc.DrawBitmap( *m_arrowRight, x-5, y_mid-6, TRUE );
                }
                else
                {
                    dc.SetBrush(*m_hilightBrush);
                    dc.SetPen(*wxBLACK_PEN);
                    wxPoint button[3];

                    if (item->IsExpanded())
                    {
                        button[0].x = x-5;
                        button[0].y = y_mid-2;
                        button[1].x = x+5;
                        button[1].y = y_mid-2;
                        button[2].x = x;
                        button[2].y = y_mid+3;
                    }
                    else
                    {
                        button[0].y = y_mid-5;
                        button[0].x = x-2;
                        button[1].y = y_mid+5;
                        button[1].x = x-2;
                        button[2].y = y_mid;
                        button[2].x = x+3;
                    }
                    dc.DrawPolygon(3, button);
                    dc.SetPen(m_dottedPen);
                }
            }
            else // if (HasFlag(wxTR_HAS_BUTTONS))
            {
                // draw the plus sign here
                dc.SetPen(*wxGREY_PEN);
                dc.SetBrush(*wxWHITE_BRUSH);
                dc.DrawRectangle(x-5, y_mid-4, 11, 9);
                dc.SetPen(*wxBLACK_PEN);
                dc.DrawLine(x-2, y_mid, x+3, y_mid);
                if (!item->IsExpanded())
                    dc.DrawLine(x, y_mid-2, x, y_mid+3);
                dc.SetPen(m_dottedPen);
            }
        }
        else if (!HasFlag(wxTR_NO_LINES))  // no button; maybe a line?
        {
            // draw the horizontal line here
            int x_start = x;
            if (x > (signed)m_indent)
                x_start -= m_indent;
            else if (HasFlag(wxTR_LINES_AT_ROOT))
                x_start = 3;
            dc.DrawLine(x_start, y_mid, x + m_spacing, y_mid);
        }
    }

    if (item->IsExpanded())
    {
        wxArrayGenericTreeItems& children = item->GetChildren();
        int count = children.Count();
        if (count > 0)
        {
            int n = 0, oldY;
            ++level;
            do {
                oldY = y;
                PaintLevel(children[n], dc, level, y);
            } while (++n < count);

            if (!HasFlag(wxTR_NO_LINES) && count > 0)
            {
                // draw line down to last child
                oldY += GetLineHeight(children[n-1])>>1;
                if (HasButtons()) y_mid += 5;
                dc.DrawLine(x, y_mid, x, oldY);
            }
        }
    }
}

void wxGenericTreeCtrl::DrawDropEffect(wxGenericTreeItem *item)
{
    if ( item )
    {
        if ( item->HasPlus() )
        {
            // it's a folder, indicate it by a border
            DrawBorder(item);
        }
        else
        {
            // draw a line under the drop target because the item will be
            // dropped there
            DrawLine(item, TRUE /* below */);
        }

        SetCursor(wxCURSOR_BULLSEYE);
    }
    else
    {
        // can't drop here
        SetCursor(wxCURSOR_NO_ENTRY);
    }
}

void wxGenericTreeCtrl::DrawBorder(const wxTreeItemId &item)
{
    wxCHECK_RET( item.IsOk(), _T("invalid item in wxGenericTreeCtrl::DrawLine") );

    wxGenericTreeItem *i = (wxGenericTreeItem*) item.m_pItem;

    wxClientDC dc(this);
    PrepareDC( dc );
    dc.SetLogicalFunction(wxINVERT);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);

    int w = i->GetWidth() + 2;
    int h = GetLineHeight(i) + 2;

    dc.DrawRectangle( i->GetX() - 1, i->GetY() - 1, w, h);
}

void wxGenericTreeCtrl::DrawLine(const wxTreeItemId &item, bool below)
{
    wxCHECK_RET( item.IsOk(), _T("invalid item in wxGenericTreeCtrl::DrawLine") );

    wxGenericTreeItem *i = (wxGenericTreeItem*) item.m_pItem;

    wxClientDC dc(this);
    PrepareDC( dc );
    dc.SetLogicalFunction(wxINVERT);

    int x = i->GetX(),
        y = i->GetY();
    if ( below )
    {
        y += GetLineHeight(i) - 1;
    }

    dc.DrawLine( x, y, x + i->GetWidth(), y);
}

// -----------------------------------------------------------------------------
// wxWindows callbacks
// -----------------------------------------------------------------------------

void wxGenericTreeCtrl::OnPaint( wxPaintEvent &WXUNUSED(event) )
{
    wxPaintDC dc(this);
    PrepareDC( dc );

    if ( !m_anchor)
        return;

    dc.SetFont( m_normalFont );
    dc.SetPen( m_dottedPen );

    // this is now done dynamically
    //if(GetImageList() == NULL)
    // m_lineHeight = (int)(dc.GetCharHeight() + 4);

    int y = 2;
    PaintLevel( m_anchor, dc, 0, y );
}

void wxGenericTreeCtrl::OnSetFocus( wxFocusEvent &event )
{
    m_hasFocus = TRUE;

    RefreshSelected();

    event.Skip();
}

void wxGenericTreeCtrl::OnKillFocus( wxFocusEvent &event )
{
    m_hasFocus = FALSE;

    RefreshSelected();

    event.Skip();
}

void wxGenericTreeCtrl::OnChar( wxKeyEvent &event )
{
    wxTreeEvent te( wxEVT_COMMAND_TREE_KEY_DOWN, GetId() );
    te.m_evtKey = event;
    te.SetEventObject( this );
    if ( GetEventHandler()->ProcessEvent( te ) )
    {
        // intercepted by the user code
        return;
    }

    if ( (m_current == 0) || (m_key_current == 0) )
    {
        event.Skip();
        return;
    }

    // how should the selection work for this event?
    bool is_multiple, extended_select, unselect_others;
    EventFlagsToSelType(GetWindowStyleFlag(),
                        event.ShiftDown(),
                        event.ControlDown(),
                        is_multiple, extended_select, unselect_others);

    // + : Expand
    // - : Collaspe
    // * : Expand all/Collapse all
    // ' ' | return : activate
    // up    : go up (not last children!)
    // down  : go down
    // left  : go to parent
    // right : open if parent and go next
    // home  : go to root
    // end   : go to last item without opening parents
    // alnum : start or continue searching for the item with this prefix
    int keyCode = event.KeyCode();
    switch ( keyCode )
    {
        case '+':
        case WXK_ADD:
            if (m_current->HasPlus() && !IsExpanded(m_current))
            {
                Expand(m_current);
            }
            break;

        case '*':
        case WXK_MULTIPLY:
            if ( !IsExpanded(m_current) )
            {
                // expand all
                ExpandAll(m_current);
                break;
            }
            //else: fall through to Collapse() it

        case '-':
        case WXK_SUBTRACT:
            if (IsExpanded(m_current))
            {
                Collapse(m_current);
            }
            break;

        case ' ':
        case WXK_RETURN:
            if ( !event.HasModifiers() )
            {
                wxTreeEvent event( wxEVT_COMMAND_TREE_ITEM_ACTIVATED, GetId() );
                event.m_item = (long) m_current;
                event.SetEventObject( this );
                GetEventHandler()->ProcessEvent( event );
            }

            // in any case, also generate the normal key event for this key,
            // even if we generated the ACTIVATED event above: this is what
            // wxMSW does and it makes sense because you might not want to
            // process ACTIVATED event at all and handle Space and Return
            // directly (and differently) which would be impossible otherwise
            event.Skip();
            break;

            // up goes to the previous sibling or to the last
            // of its children if it's expanded
        case WXK_UP:
            {
                wxTreeItemId prev = GetPrevSibling( m_key_current );
                if (!prev)
                {
                    prev = GetItemParent( m_key_current );
                    if ((prev == GetRootItem()) && HasFlag(wxTR_HIDE_ROOT))
                    {
                        break;  // don't go to root if it is hidden
                    }
                    if (prev)
                    {
                        long cookie = 0;
                        wxTreeItemId current = m_key_current;
                        // TODO: Huh?  If we get here, we'd better be the first child of our parent.  How else could it be?
                        if (current == GetFirstChild( prev, cookie ))
                        {
                            // otherwise we return to where we came from
                            SelectItem( prev, unselect_others, extended_select );
                            m_key_current= (wxGenericTreeItem*) prev.m_pItem;
                            break;
                        }
                    }
                }
                if (prev)
                {
                    while ( IsExpanded(prev) && HasChildren(prev) )
                    {
                        wxTreeItemId child = GetLastChild(prev);
                        if ( child )
                        {
                            prev = child;
                        }
                    }

                    SelectItem( prev, unselect_others, extended_select );
                    m_key_current=(wxGenericTreeItem*) prev.m_pItem;
                }
            }
            break;

            // left arrow goes to the parent
        case WXK_LEFT:
            {
                wxTreeItemId prev = GetItemParent( m_current );
                if ((prev == GetRootItem()) && HasFlag(wxTR_HIDE_ROOT))
                {
                    // don't go to root if it is hidden
                    prev = GetPrevSibling( m_current );
                }
                if (prev)
                {
                    SelectItem( prev, unselect_others, extended_select );
                }
            }
            break;

        case WXK_RIGHT:
            // this works the same as the down arrow except that we
            // also expand the item if it wasn't expanded yet
            Expand(m_current);
            // fall through

        case WXK_DOWN:
            {
                if (IsExpanded(m_key_current) && HasChildren(m_key_current))
                {
                    long cookie = 0;
                    wxTreeItemId child = GetFirstChild( m_key_current, cookie );
                    SelectItem( child, unselect_others, extended_select );
                    m_key_current=(wxGenericTreeItem*) child.m_pItem;
                }
                else
                {
                    wxTreeItemId next = GetNextSibling( m_key_current );
                    if (!next)
                    {
                        wxTreeItemId current = m_key_current;
                        while (current && !next)
                        {
                            current = GetItemParent( current );
                            if (current) next = GetNextSibling( current );
                        }
                    }
                    if (next)
                    {
                        SelectItem( next, unselect_others, extended_select );
                        m_key_current=(wxGenericTreeItem*) next.m_pItem;
                    }
                }
            }
            break;

            // <End> selects the last visible tree item
        case WXK_END:
            {
                wxTreeItemId last = GetRootItem();

                while ( last.IsOk() && IsExpanded(last) )
                {
                    wxTreeItemId lastChild = GetLastChild(last);

                    // it may happen if the item was expanded but then all of
                    // its children have been deleted - so IsExpanded() returned
                    // TRUE, but GetLastChild() returned invalid item
                    if ( !lastChild )
                        break;

                    last = lastChild;
                }

                if ( last.IsOk() )
                {
                    SelectItem( last, unselect_others, extended_select );
                }
            }
            break;

            // <Home> selects the root item
        case WXK_HOME:
            {
                wxTreeItemId prev = GetRootItem();
                if (!prev)
                    break;

                if ( HasFlag(wxTR_HIDE_ROOT) )
                {
                    long dummy;
                    prev = GetFirstChild(prev, dummy);
                    if (!prev)
                        break;
                }

                SelectItem( prev, unselect_others, extended_select );
            }
            break;

        default:
            // do not use wxIsalnum() here
            if ( !event.HasModifiers() &&
                 ((keyCode >= '0' && keyCode <= '9') ||
                  (keyCode >= 'a' && keyCode <= 'z') ||
                  (keyCode >= 'A' && keyCode <= 'Z' )))
            {
                // find the next item starting with the given prefix
                char ch = (char)keyCode;

                wxTreeItemId id = FindItem(m_current, m_findPrefix + (wxChar)ch);
                if ( !id.IsOk() )
                {
                    // no such item
                    break;
                }

                SelectItem(id);

                m_findPrefix += ch;

                // also start the timer to reset the current prefix if the user
                // doesn't press any more alnum keys soon -- we wouldn't want
                // to use this prefix for a new item search
                if ( !m_findTimer )
                {
                    m_findTimer = new wxTreeFindTimer(this);
                }

                m_findTimer->Start(wxTreeFindTimer::DELAY, wxTIMER_ONE_SHOT);
            }
            else
            {
                event.Skip();
            }
    }
}

wxTreeItemId wxGenericTreeCtrl::HitTest(const wxPoint& point, int& flags)
{
    // JACS: removed wxYieldIfNeeded() because it can cause the window
    // to be deleted from under us if a close window event is pending

    int w, h;
    GetSize(&w, &h);
    flags=0;
    if (point.x<0) flags |= wxTREE_HITTEST_TOLEFT;
    if (point.x>w) flags |= wxTREE_HITTEST_TORIGHT;
    if (point.y<0) flags |= wxTREE_HITTEST_ABOVE;
    if (point.y>h) flags |= wxTREE_HITTEST_BELOW;
    if (flags) return wxTreeItemId();

    if (m_anchor == NULL)
    {
        flags = wxTREE_HITTEST_NOWHERE;
        return wxTreeItemId();
    }

    wxGenericTreeItem *hit =  m_anchor->HitTest(CalcUnscrolledPosition(point),
                                                this, flags, 0);
    if (hit == NULL)
    {
        flags = wxTREE_HITTEST_NOWHERE;
        return wxTreeItemId();
    }
    return hit;
}

// get the bounding rectangle of the item (or of its label only)
bool wxGenericTreeCtrl::GetBoundingRect(const wxTreeItemId& item,
                                        wxRect& rect,
                                        bool WXUNUSED(textOnly)) const
{
    wxCHECK_MSG( item.IsOk(), FALSE, _T("invalid item in wxGenericTreeCtrl::GetBoundingRect") );

    wxGenericTreeItem *i = (wxGenericTreeItem*) item.m_pItem;

    int startX, startY;
    GetViewStart(& startX, & startY);

    rect.x = i->GetX() - startX*PIXELS_PER_UNIT;
    rect.y = i->GetY() - startY*PIXELS_PER_UNIT;
    rect.width = i->GetWidth();
    //rect.height = i->GetHeight();
    rect.height = GetLineHeight(i);

    return TRUE;
}

void wxGenericTreeCtrl::Edit( const wxTreeItemId& item )
{
    wxCHECK_RET( item.IsOk(), _T("can't edit an invalid item") );

    wxGenericTreeItem *itemEdit = (wxGenericTreeItem *)item.m_pItem;

    wxTreeEvent te( wxEVT_COMMAND_TREE_BEGIN_LABEL_EDIT, GetId() );
    te.m_item = (long) itemEdit;
    te.SetEventObject( this );
    if ( GetEventHandler()->ProcessEvent( te ) && !te.IsAllowed() )
    {
        // vetoed by user
        return;
    }

    // We have to call this here because the label in
    // question might just have been added and no screen
    // update taken place.
    if ( m_dirty )
        wxYieldIfNeeded();

    m_textCtrl = new wxTreeTextCtrl(this, itemEdit);

    m_textCtrl->SetFocus();
}

// returns a pointer to the text edit control if the item is being
// edited, NULL otherwise (it's assumed that no more than one item may
// be edited simultaneously)
wxTextCtrl* wxGenericTreeCtrl::GetEditControl() const
{
    return m_textCtrl;
}

bool wxGenericTreeCtrl::OnRenameAccept(wxGenericTreeItem *item,
                                       const wxString& value)
{
    wxTreeEvent le( wxEVT_COMMAND_TREE_END_LABEL_EDIT, GetId() );
    le.m_item = (long) item;
    le.SetEventObject( this );
    le.m_label = value;
    le.m_editCancelled = FALSE;

    return !GetEventHandler()->ProcessEvent( le ) || le.IsAllowed();
}

void wxGenericTreeCtrl::OnRenameCancelled(wxGenericTreeItem *item)
{
    // let owner know that the edit was cancelled
    wxTreeEvent le( wxEVT_COMMAND_TREE_END_LABEL_EDIT, GetId() );
    le.m_item = (long) item;
    le.SetEventObject( this );
    le.m_label = wxEmptyString;
    le.m_editCancelled = FALSE;

    GetEventHandler()->ProcessEvent( le );
}




void wxGenericTreeCtrl::OnRenameTimer()
{
    Edit( m_current );
}

void wxGenericTreeCtrl::OnMouse( wxMouseEvent &event )
{
    if ( !m_anchor ) return;

    // we process left mouse up event (enables in-place edit), right down
    // (pass to the user code), left dbl click (activate item) and
    // dragging/moving events for items drag-and-drop
    if ( !(event.LeftDown() ||
           event.LeftUp() ||
           event.RightDown() ||
           event.LeftDClick() ||
           event.Dragging() ||
           ((event.Moving() || event.RightUp()) && m_isDragging)) )
    {
        event.Skip();

        return;
    }

    wxPoint pt = CalcUnscrolledPosition(event.GetPosition());

    int flags = 0;
    wxGenericTreeItem *item = m_anchor->HitTest(pt, this, flags, 0);

    if ( event.Dragging() && !m_isDragging )
    {
        if (m_dragCount == 0)
            m_dragStart = pt;

        m_dragCount++;

        if (m_dragCount != 3)
        {
            // wait until user drags a bit further...
            return;
        }

        wxEventType command = event.RightIsDown()
                              ? wxEVT_COMMAND_TREE_BEGIN_RDRAG
                              : wxEVT_COMMAND_TREE_BEGIN_DRAG;

        wxTreeEvent nevent( command, GetId() );
        nevent.m_item = (long) m_current;
        nevent.SetEventObject(this);

        // by default the dragging is not supported, the user code must
        // explicitly allow the event for it to take place
        nevent.Veto();

        if ( GetEventHandler()->ProcessEvent(nevent) && nevent.IsAllowed() )
        {
            // we're going to drag this item
            m_isDragging = TRUE;

            // remember the old cursor because we will change it while
            // dragging
            m_oldCursor = m_cursor;

            // in a single selection control, hide the selection temporarily
            if ( !(GetWindowStyleFlag() & wxTR_MULTIPLE) )
            {
                m_oldSelection = (wxGenericTreeItem*) GetSelection().m_pItem;

                if ( m_oldSelection )
                {
                    m_oldSelection->SetHilight(FALSE);
                    RefreshLine(m_oldSelection);
                }
            }

            CaptureMouse();
        }
    }
    else if ( event.Moving() )
    {
        if ( item != m_dropTarget )
        {
            // unhighlight the previous drop target
            DrawDropEffect(m_dropTarget);

            m_dropTarget = item;

            // highlight the current drop target if any
            DrawDropEffect(m_dropTarget);

            wxYieldIfNeeded();
        }
    }
    else if ( (event.LeftUp() || event.RightUp()) && m_isDragging )
    {
        // erase the highlighting
        DrawDropEffect(m_dropTarget);

        if ( m_oldSelection )
        {
            m_oldSelection->SetHilight(TRUE);
            RefreshLine(m_oldSelection);
            m_oldSelection = (wxGenericTreeItem *)NULL;
        }

        // generate the drag end event
        wxTreeEvent event(wxEVT_COMMAND_TREE_END_DRAG, GetId());

        event.m_item = (long) item;
        event.m_pointDrag = pt;
        event.SetEventObject(this);

        (void)GetEventHandler()->ProcessEvent(event);

        m_isDragging = FALSE;
        m_dropTarget = (wxGenericTreeItem *)NULL;

        ReleaseMouse();

        SetCursor(m_oldCursor);

        wxYieldIfNeeded();
    }
    else
    {
        // here we process only the messages which happen on tree items

        m_dragCount = 0;

        if (item == NULL) return;  /* we hit the blank area */

        if ( event.RightDown() )
        {
            wxTreeEvent nevent(wxEVT_COMMAND_TREE_ITEM_RIGHT_CLICK, GetId());
            nevent.m_item = (long) item;
            nevent.m_pointDrag = CalcScrolledPosition(pt);
            nevent.SetEventObject(this);
            GetEventHandler()->ProcessEvent(nevent);
        }
        else if ( event.LeftUp() )
        {
            if ( m_lastOnSame )
            {
                if ( (item == m_current) &&
                     (flags & wxTREE_HITTEST_ONITEMLABEL) &&
                     HasFlag(wxTR_EDIT_LABELS) )
                {
                    if ( m_renameTimer )
                    {
                        if ( m_renameTimer->IsRunning() )
                            m_renameTimer->Stop();
                    }
                    else
                    {
                        m_renameTimer = new wxTreeRenameTimer( this );
                    }

                    m_renameTimer->Start( wxTreeRenameTimer::DELAY, TRUE );
                }

                m_lastOnSame = FALSE;
            }
        }
        else // !RightDown() && !LeftUp() ==> LeftDown() || LeftDClick()
        {
            if ( event.LeftDown() )
            {
                m_lastOnSame = item == m_current;
            }

            if ( flags & wxTREE_HITTEST_ONITEMBUTTON )
            {
                // only toggle the item for a single click, double click on
                // the button doesn't do anything (it toggles the item twice)
                if ( event.LeftDown() )
                {
                    Toggle( item );
                }

                // don't select the item if the button was clicked
                return;
            }

            // how should the selection work for this event?
            bool is_multiple, extended_select, unselect_others;
            EventFlagsToSelType(GetWindowStyleFlag(),
                                event.ShiftDown(),
                                event.ControlDown(),
                                is_multiple, extended_select, unselect_others);

            SelectItem(item, unselect_others, extended_select);

            // For some reason, Windows isn't recognizing a left double-click,
            // so we need to simulate it here.  Allow 200 milliseconds for now.
            if ( event.LeftDClick() )
            {
                // double clicking should not start editing the item label
                if ( m_renameTimer )
                    m_renameTimer->Stop();

                m_lastOnSame = FALSE;

                // send activate event first
                wxTreeEvent nevent( wxEVT_COMMAND_TREE_ITEM_ACTIVATED, GetId() );
                nevent.m_item = (long) item;
                nevent.m_pointDrag = CalcScrolledPosition(pt);
                nevent.SetEventObject( this );
                if ( !GetEventHandler()->ProcessEvent( nevent ) )
                {
                    // if the user code didn't process the activate event,
                    // handle it ourselves by toggling the item when it is
                    // double clicked
                    if ( item->HasPlus() )
                    {
                        Toggle(item);
                    }
                }
            }
        }
    }
}

void wxGenericTreeCtrl::OnIdle( wxIdleEvent &WXUNUSED(event) )
{
    /* after all changes have been done to the tree control,
     * we actually redraw the tree when everything is over */

    if (!m_dirty) return;

    m_dirty = FALSE;

    CalculatePositions();
    Refresh();
    AdjustMyScrollbars();
}

void wxGenericTreeCtrl::CalculateSize( wxGenericTreeItem *item, wxDC &dc )
{
    wxCoord text_w = 0;
    wxCoord text_h = 0;

    wxTreeItemAttr *attr = item->GetAttributes();
    if ( attr && attr->HasFont() )
        dc.SetFont(attr->GetFont());
    else if ( item->IsBold() )
        dc.SetFont(m_boldFont);

    dc.GetTextExtent( item->GetText(), &text_w, &text_h );
    text_h+=2;

    // restore normal font
    dc.SetFont( m_normalFont );

    int image_h = 0;
    int image_w = 0;
    int image = item->GetCurrentImage();
    if ( image != NO_IMAGE )
    {
        if ( m_imageListNormal )
        {
            m_imageListNormal->GetSize( image, image_w, image_h );
            image_w += 4;
        }
    }

    int total_h = (image_h > text_h) ? image_h : text_h;

    if (total_h < 30)
        total_h += 2;            // at least 2 pixels
    else
        total_h += total_h/10;   // otherwise 10% extra spacing

    item->SetHeight(total_h);
    if (total_h>m_lineHeight)
        m_lineHeight=total_h;

    item->SetWidth(image_w+text_w+2);
}

// -----------------------------------------------------------------------------
// for developper : y is now the top of the level
// not the middle of it !
void wxGenericTreeCtrl::CalculateLevel( wxGenericTreeItem *item, wxDC &dc, int level, int &y )
{
    int x = level*m_indent;
    if (!HasFlag(wxTR_HIDE_ROOT))
    {
        x += m_indent;
    }
    else if (level == 0)
    {
        // a hidden root is not evaluated, but its
        // children are always calculated
        goto Recurse;
    }

    CalculateSize( item, dc );

    // set its position
    item->SetX( x+m_spacing );
    item->SetY( y );
    y += GetLineHeight(item);

    if ( !item->IsExpanded() )
    {
        // we don't need to calculate collapsed branches
        return;
    }

  Recurse:
    wxArrayGenericTreeItems& children = item->GetChildren();
    size_t n, count = children.Count();
    ++level;
    for (n = 0; n < count; ++n )
        CalculateLevel( children[n], dc, level, y );  // recurse
}

void wxGenericTreeCtrl::CalculatePositions()
{
    if ( !m_anchor ) return;

    wxClientDC dc(this);
    PrepareDC( dc );

    dc.SetFont( m_normalFont );

    dc.SetPen( m_dottedPen );
    //if(GetImageList() == NULL)
    // m_lineHeight = (int)(dc.GetCharHeight() + 4);

    int y = 2;
    CalculateLevel( m_anchor, dc, 0, y ); // start recursion
}

void wxGenericTreeCtrl::RefreshSubtree(wxGenericTreeItem *item)
{
    if (m_dirty) return;

    wxSize client = GetClientSize();

    wxRect rect;
    CalcScrolledPosition(0, item->GetY(), NULL, &rect.y);
    rect.width = client.x;
    rect.height = client.y;

    Refresh(TRUE, &rect);

    AdjustMyScrollbars();
}

void wxGenericTreeCtrl::RefreshLine( wxGenericTreeItem *item )
{
    if (m_dirty) return;

    wxRect rect;
    CalcScrolledPosition(0, item->GetY(), NULL, &rect.y);
    rect.width = GetClientSize().x;
    rect.height = GetLineHeight(item); //dc.GetCharHeight() + 6;

    Refresh(TRUE, &rect);
}

void wxGenericTreeCtrl::RefreshSelected()
{
    // TODO: this is awfully inefficient, we should keep the list of all
    //       selected items internally, should be much faster
    if ( m_anchor )
        RefreshSelectedUnder(m_anchor);
}

void wxGenericTreeCtrl::RefreshSelectedUnder(wxGenericTreeItem *item)
{
    if ( item->IsSelected() )
        RefreshLine(item);

    const wxArrayGenericTreeItems& children = item->GetChildren();
    size_t count = children.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        RefreshSelectedUnder(children[n]);
    }
}

// ----------------------------------------------------------------------------
// changing colours: we need to refresh the tree control
// ----------------------------------------------------------------------------

bool wxGenericTreeCtrl::SetBackgroundColour(const wxColour& colour)
{
    if ( !wxWindow::SetBackgroundColour(colour) )
        return FALSE;

    Refresh();

    return TRUE;
}

bool wxGenericTreeCtrl::SetForegroundColour(const wxColour& colour)
{
    if ( !wxWindow::SetForegroundColour(colour) )
        return FALSE;

    Refresh();

    return TRUE;
}

#endif // wxUSE_TREECTRL
