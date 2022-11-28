/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/treectlg.h
// Purpose:     wxTreeCtrl class
// Author:      Robert Roebling
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: treectlg.h,v 1.20.2.3 2002/12/29 07:48:10 RL Exp $
// Copyright:   (c) 1997,1998 Robert Roebling
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

#ifndef _GENERIC_TREECTRL_H_
#define _GENERIC_TREECTRL_H_

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "treectlg.h"
#endif

#if wxUSE_TREECTRL

#include "wx/scrolwin.h"
#include "wx/pen.h"
#include "wx/imaglist.h"

// -----------------------------------------------------------------------------
// forward declaration
// -----------------------------------------------------------------------------

class WXDLLEXPORT wxGenericTreeItem;

class WXDLLEXPORT wxTreeItemData;

class WXDLLEXPORT wxTreeRenameTimer;
class WXDLLEXPORT wxTreeFindTimer;
class WXDLLEXPORT wxTreeTextCtrl;
class WXDLLEXPORT wxTextCtrl;

// -----------------------------------------------------------------------------
// wxGenericTreeCtrl - the tree control
// -----------------------------------------------------------------------------

class WXDLLEXPORT wxGenericTreeCtrl : public wxScrolledWindow
{
public:
    // creation
    // --------
    wxGenericTreeCtrl() { Init(); }

    wxGenericTreeCtrl(wxWindow *parent, wxWindowID id = -1,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = wxTR_DEFAULT_STYLE,
               const wxValidator &validator = wxDefaultValidator,
               const wxString& name = wxTreeCtrlNameStr)
    {
        Init();
        Create(parent, id, pos, size, style, validator, name);
    }

    virtual ~wxGenericTreeCtrl();

    bool Create(wxWindow *parent, wxWindowID id = -1,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxTR_DEFAULT_STYLE,
                const wxValidator &validator = wxDefaultValidator,
                const wxString& name = wxTreeCtrlNameStr);

    // accessors
    // ---------

        // get the total number of items in the control
    size_t GetCount() const;

        // indent is the number of pixels the children are indented relative to
        // the parents position. SetIndent() also redraws the control
        // immediately.
    unsigned int GetIndent() const { return m_indent; }
    void SetIndent(unsigned int indent);

        // spacing is the number of pixels between the start and the Text
    unsigned int GetSpacing() const { return m_spacing; }
    void SetSpacing(unsigned int spacing);

        // image list: these functions allow to associate an image list with
        // the control and retrieve it. Note that when assigned with
        // SetImageList, the control does _not_ delete
        // the associated image list when it's deleted in order to allow image
        // lists to be shared between different controls. If you use
        // AssignImageList, the control _does_ delete the image list.
        //
        // The normal image list is for the icons which correspond to the
        // normal tree item state (whether it is selected or not).
        // Additionally, the application might choose to show a state icon
        // which corresponds to an app-defined item state (for example,
        // checked/unchecked) which are taken from the state image list.
    wxImageList *GetImageList() const;
    wxImageList *GetStateImageList() const;
    wxImageList *GetButtonsImageList() const;

    void SetImageList(wxImageList *imageList);
    void SetStateImageList(wxImageList *imageList);
    void SetButtonsImageList(wxImageList *imageList);
    void AssignImageList(wxImageList *imageList);
    void AssignStateImageList(wxImageList *imageList);
    void AssignButtonsImageList(wxImageList *imageList);

    // Functions to work with tree ctrl items.

    // accessors
    // ---------

        // retrieve item's label
    wxString GetItemText(const wxTreeItemId& item) const;
        // get one of the images associated with the item (normal by default)
    int GetItemImage(const wxTreeItemId& item,
                     wxTreeItemIcon which = wxTreeItemIcon_Normal) const;
        // get the data associated with the item
    wxTreeItemData *GetItemData(const wxTreeItemId& item) const;

        // get the item's text colour
    wxColour GetItemTextColour(const wxTreeItemId& item) const;

        // get the item's background colour
    wxColour GetItemBackgroundColour(const wxTreeItemId& item) const;

        // get the item's font
    wxFont GetItemFont(const wxTreeItemId& item) const;

    // modifiers
    // ---------

        // set item's label
    void SetItemText(const wxTreeItemId& item, const wxString& text);
        // get one of the images associated with the item (normal by default)
    void SetItemImage(const wxTreeItemId& item, int image,
                      wxTreeItemIcon which = wxTreeItemIcon_Normal);
        // associate some data with the item
    void SetItemData(const wxTreeItemId& item, wxTreeItemData *data);

        // force appearance of [+] button near the item. This is useful to
        // allow the user to expand the items which don't have any children now
        // - but instead add them only when needed, thus minimizing memory
        // usage and loading time.
    void SetItemHasChildren(const wxTreeItemId& item, bool has = TRUE);

        // the item will be shown in bold
    void SetItemBold(const wxTreeItemId& item, bool bold = TRUE);

        // set the item's text colour
    void SetItemTextColour(const wxTreeItemId& item, const wxColour& col);

        // set the item's background colour
    void SetItemBackgroundColour(const wxTreeItemId& item, const wxColour& col);

        // set the item's font (should be of the same height for all items)
    void SetItemFont(const wxTreeItemId& item, const wxFont& font);

        // set the window font
    virtual bool SetFont( const wxFont &font );

       // set the styles.  No need to specify a GetWindowStyle here since
       // the base wxWindow member function will do it for us
    void SetWindowStyle(const long styles);

    // item status inquiries
    // ---------------------

        // is the item visible (it might be outside the view or not expanded)?
    bool IsVisible(const wxTreeItemId& item) const;
        // does the item has any children?
    bool HasChildren(const wxTreeItemId& item) const
      { return ItemHasChildren(item); }
    bool ItemHasChildren(const wxTreeItemId& item) const;
        // is the item expanded (only makes sense if HasChildren())?
    bool IsExpanded(const wxTreeItemId& item) const;
        // is this item currently selected (the same as has focus)?
    bool IsSelected(const wxTreeItemId& item) const;
        // is item text in bold font?
    bool IsBold(const wxTreeItemId& item) const;
        // does the layout include space for a button?

    // number of children
    // ------------------

        // if 'recursively' is FALSE, only immediate children count, otherwise
        // the returned number is the number of all items in this branch
    size_t GetChildrenCount(const wxTreeItemId& item, bool recursively = TRUE);

    // navigation
    // ----------

    // wxTreeItemId.IsOk() will return FALSE if there is no such item

        // get the root tree item
    wxTreeItemId GetRootItem() const { return m_anchor; }

        // get the item currently selected (may return NULL if no selection)
    wxTreeItemId GetSelection() const { return m_current; }

        // get the items currently selected, return the number of such item
    size_t GetSelections(wxArrayTreeItemIds&) const;

        // get the parent of this item (may return NULL if root)
    wxTreeItemId GetItemParent(const wxTreeItemId& item) const;

#if WXWIN_COMPATIBILITY_2_2
        // deprecated:  Use GetItemParent instead.
    wxTreeItemId GetParent(const wxTreeItemId& item) const
    	{ return GetItemParent( item ); }

    	// Expose the base class method hidden by the one above.
    wxWindow *GetParent() const { return wxScrolledWindow::GetParent(); }
#endif  // WXWIN_COMPATIBILITY_2_2

        // for this enumeration function you must pass in a "cookie" parameter
        // which is opaque for the application but is necessary for the library
        // to make these functions reentrant (i.e. allow more than one
        // enumeration on one and the same object simultaneously). Of course,
        // the "cookie" passed to GetFirstChild() and GetNextChild() should be
        // the same!

        // get the first child of this item
    wxTreeItemId GetFirstChild(const wxTreeItemId& item, long& cookie) const;
        // get the next child
    wxTreeItemId GetNextChild(const wxTreeItemId& item, long& cookie) const;
        // get the last child of this item - this method doesn't use cookies
    wxTreeItemId GetLastChild(const wxTreeItemId& item) const;

        // get the next sibling of this item
    wxTreeItemId GetNextSibling(const wxTreeItemId& item) const;
        // get the previous sibling
    wxTreeItemId GetPrevSibling(const wxTreeItemId& item) const;

        // get first visible item
    wxTreeItemId GetFirstVisibleItem() const;
        // get the next visible item: item must be visible itself!
        // see IsVisible() and wxTreeCtrl::GetFirstVisibleItem()
    wxTreeItemId GetNextVisible(const wxTreeItemId& item) const;
        // get the previous visible item: item must be visible itself!
    wxTreeItemId GetPrevVisible(const wxTreeItemId& item) const;

        // Only for internal use right now, but should probably be public
    wxTreeItemId GetNext(const wxTreeItemId& item) const;

    // operations
    // ----------

        // add the root node to the tree
    wxTreeItemId AddRoot(const wxString& text,
                         int image = -1, int selectedImage = -1,
                         wxTreeItemData *data = NULL);

        // insert a new item in as the first child of the parent
    wxTreeItemId PrependItem(const wxTreeItemId& parent,
                             const wxString& text,
                             int image = -1, int selectedImage = -1,
                             wxTreeItemData *data = NULL);

        // insert a new item after a given one
    wxTreeItemId InsertItem(const wxTreeItemId& parent,
                            const wxTreeItemId& idPrevious,
                            const wxString& text,
                            int image = -1, int selectedImage = -1,
                            wxTreeItemData *data = NULL);

        // insert a new item before the one with the given index
    wxTreeItemId InsertItem(const wxTreeItemId& parent,
                            size_t index,
                            const wxString& text,
                            int image = -1, int selectedImage = -1,
                            wxTreeItemData *data = NULL);

        // insert a new item in as the last child of the parent
    wxTreeItemId AppendItem(const wxTreeItemId& parent,
                            const wxString& text,
                            int image = -1, int selectedImage = -1,
                            wxTreeItemData *data = NULL);

        // delete this item and associated data if any
    void Delete(const wxTreeItemId& item);
        // delete all children (but don't delete the item itself)
        // NB: this won't send wxEVT_COMMAND_TREE_ITEM_DELETED events
    void DeleteChildren(const wxTreeItemId& item);
        // delete all items from the tree
        // NB: this won't send wxEVT_COMMAND_TREE_ITEM_DELETED events
    void DeleteAllItems();

        // expand this item
    void Expand(const wxTreeItemId& item);
        // expand this item and all subitems recursively
    void ExpandAll(const wxTreeItemId& item);
        // collapse the item without removing its children
    void Collapse(const wxTreeItemId& item);
        // collapse the item and remove all children
    void CollapseAndReset(const wxTreeItemId& item);
        // toggles the current state
    void Toggle(const wxTreeItemId& item);

        // remove the selection from currently selected item (if any)
    void Unselect();
    void UnselectAll();
        // select this item
    void SelectItem(const wxTreeItemId& item, bool unselect_others=TRUE, bool extended_select=FALSE);
        // make sure this item is visible (expanding the parent item and/or
        // scrolling to this item if necessary)
    void EnsureVisible(const wxTreeItemId& item);
        // scroll to this item (but don't expand its parent)
    void ScrollTo(const wxTreeItemId& item);
    void AdjustMyScrollbars();

        // The first function is more portable (because easier to implement
        // on other platforms), but the second one returns some extra info.
    wxTreeItemId HitTest(const wxPoint& point)
        { int dummy; return HitTest(point, dummy); }
    wxTreeItemId HitTest(const wxPoint& point, int& flags);

        // get the bounding rectangle of the item (or of its label only)
    bool GetBoundingRect(const wxTreeItemId& item,
                         wxRect& rect,
                         bool textOnly = FALSE) const;

        // Start editing the item label: this (temporarily) replaces the item
        // with a one line edit control. The item will be selected if it hadn't
        // been before.
    void EditLabel( const wxTreeItemId& item ) { Edit( item ); }
    void Edit( const wxTreeItemId& item );
        // returns a pointer to the text edit control if the item is being
        // edited, NULL otherwise (it's assumed that no more than one item may
        // be edited simultaneously)
    wxTextCtrl* GetEditControl() const;

    // sorting
        // this function is called to compare 2 items and should return -1, 0
        // or +1 if the first item is less than, equal to or greater than the
        // second one. The base class version performs alphabetic comparaison
        // of item labels (GetText)
    virtual int OnCompareItems(const wxTreeItemId& item1,
                               const wxTreeItemId& item2);
        // sort the children of this item using OnCompareItems
        //
        // NB: this function is not reentrant and not MT-safe (FIXME)!
    void SortChildren(const wxTreeItemId& item);

    // deprecated functions: use Set/GetItemImage directly
        // get the selected item image
    int GetItemSelectedImage(const wxTreeItemId& item) const
        { return GetItemImage(item, wxTreeItemIcon_Selected); }
        // set the selected item image
    void SetItemSelectedImage(const wxTreeItemId& item, int image)
        { SetItemImage(item, image, wxTreeItemIcon_Selected); }

    // implementation only from now on

    // overridden base class virtuals
    virtual bool SetBackgroundColour(const wxColour& colour);
    virtual bool SetForegroundColour(const wxColour& colour);

    // callbacks
    void OnPaint( wxPaintEvent &event );
    void OnSetFocus( wxFocusEvent &event );
    void OnKillFocus( wxFocusEvent &event );
    void OnChar( wxKeyEvent &event );
    void OnMouse( wxMouseEvent &event );
    void OnIdle( wxIdleEvent &event );

    // implementation helpers
protected:
    friend class wxGenericTreeItem;
    friend class wxTreeRenameTimer;
    friend class wxTreeFindTimer;
    friend class wxTreeTextCtrl;

    wxFont               m_normalFont;
    wxFont               m_boldFont;

    wxGenericTreeItem   *m_anchor;
    wxGenericTreeItem   *m_current,
                        *m_key_current;
    unsigned short       m_indent;
    unsigned short       m_spacing;
    int                  m_lineHeight;
    wxPen                m_dottedPen;
    wxBrush             *m_hilightBrush,
                        *m_hilightUnfocusedBrush;
    bool                 m_hasFocus;
    bool                 m_dirty;
    bool                 m_ownsImageListNormal,
                         m_ownsImageListState,
                         m_ownsImageListButtons;
    bool                 m_isDragging; // true between BEGIN/END drag events
    bool                 m_lastOnSame;  // last click on the same item as prev
    wxImageList         *m_imageListNormal,
                        *m_imageListState,
                        *m_imageListButtons;

    int                  m_dragCount;
    wxPoint              m_dragStart;
    wxGenericTreeItem   *m_dropTarget;
    wxCursor             m_oldCursor;  // cursor is changed while dragging
    wxGenericTreeItem   *m_oldSelection;
    wxTreeTextCtrl      *m_textCtrl;

    wxTimer             *m_renameTimer;

    wxBitmap            *m_arrowRight,
                        *m_arrowDown;

    // incremental search data
    wxString             m_findPrefix;
    wxTimer             *m_findTimer;

    // the common part of all ctors
    void Init();

    // misc helpers
    void SendDeleteEvent(wxGenericTreeItem *itemBeingDeleted);

    void DrawBorder(const wxTreeItemId& item);
    void DrawLine(const wxTreeItemId& item, bool below);
    void DrawDropEffect(wxGenericTreeItem *item);

    wxTreeItemId DoInsertItem(const wxTreeItemId& parent,
                              size_t previous,
                              const wxString& text,
                              int image, int selectedImage,
                              wxTreeItemData *data);

    // called by wxTextTreeCtrl when it marks itself for deletion
    void ResetTextControl();

    // find the first item starting with the given prefix after the given item
    wxTreeItemId FindItem(const wxTreeItemId& id, const wxString& prefix) const;

    bool HasButtons(void) const
        { return (m_imageListButtons != NULL)
              || HasFlag(wxTR_TWIST_BUTTONS|wxTR_HAS_BUTTONS); }

    void CalculateLineHeight();
    int  GetLineHeight(wxGenericTreeItem *item) const;
    void PaintLevel( wxGenericTreeItem *item, wxDC& dc, int level, int &y );
    void PaintItem( wxGenericTreeItem *item, wxDC& dc);

    void CalculateLevel( wxGenericTreeItem *item, wxDC &dc, int level, int &y );
    void CalculatePositions();
    void CalculateSize( wxGenericTreeItem *item, wxDC &dc );

    void RefreshSubtree( wxGenericTreeItem *item );
    void RefreshLine( wxGenericTreeItem *item );

    // redraw all selected items
    void RefreshSelected();

    // RefreshSelected() recursive helper
    void RefreshSelectedUnder(wxGenericTreeItem *item);

    void OnRenameTimer();
    bool OnRenameAccept(wxGenericTreeItem *item, const wxString& value);
    void OnRenameCancelled(wxGenericTreeItem *item);

    void FillArray(wxGenericTreeItem*, wxArrayTreeItemIds&) const;
    void SelectItemRange( wxGenericTreeItem *item1, wxGenericTreeItem *item2 );
    bool TagAllChildrenUntilLast(wxGenericTreeItem *crt_item, wxGenericTreeItem *last_item, bool select);
    bool TagNextChildren(wxGenericTreeItem *crt_item, wxGenericTreeItem *last_item, bool select);
    void UnselectAllChildren( wxGenericTreeItem *item );

private:
    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxGenericTreeCtrl)
};

#if !defined(__WXMSW__) || defined(__WIN16__) || defined(__WXUNIVERSAL__)
/*
 * wxTreeCtrl has to be a real class or we have problems with
 * the run-time information.
 */

class WXDLLEXPORT wxTreeCtrl: public wxGenericTreeCtrl
{
    DECLARE_DYNAMIC_CLASS(wxTreeCtrl)

public:
    wxTreeCtrl() {}

    wxTreeCtrl(wxWindow *parent, wxWindowID id = -1,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = wxTR_DEFAULT_STYLE,
               const wxValidator &validator = wxDefaultValidator,
               const wxString& name = wxTreeCtrlNameStr)
    : wxGenericTreeCtrl(parent, id, pos, size, style, validator, name)
    {
    }
};
#endif // !__WXMSW__ || __WIN16__ || __WXUNIVERSAL__

#endif // wxUSE_TREECTRL

#endif // _GENERIC_TREECTRL_H_

