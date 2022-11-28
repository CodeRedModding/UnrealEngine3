/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/treectrl.h
// Purpose:     wxTreeCtrl class
// Author:      Julian Smart
// Modified by: Vadim Zeitlin to be less MSW-specific on 10/10/98
// Created:     01/02/97
// RCS-ID:      $Id: treectrl.h,v 1.51.2.5 2002/12/29 15:42:10 RL Exp $
// Copyright:   (c) Julian Smart and Markus Holzem
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TREECTRL_H_
#define _WX_TREECTRL_H_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma interface "treectrl.h"
#endif

#if wxUSE_TREECTRL

#include "wx/textctrl.h"
#include "wx/dynarray.h"
#include "wx/treebase.h"

#ifdef __GNUWIN32__
    // Cygwin windows.h defines these identifiers
    #undef GetFirstChild
    #undef GetNextSibling
#endif // Cygwin

// the type for "untyped" data
typedef long wxDataType;

// fwd decl
class  WXDLLEXPORT wxImageList;
class  WXDLLEXPORT wxDragImage;
struct WXDLLEXPORT wxTreeViewItem;

// a callback function used for sorting tree items, it should return -1 if the
// first item precedes the second, +1 if the second precedes the first or 0 if
// they're equivalent
class wxTreeItemData;

// NB: all the following flags are for compatbility only and will be removed in the
//     next versions

// flags for deprecated `Expand(int action)'
enum
{
    wxTREE_EXPAND_EXPAND,
    wxTREE_EXPAND_COLLAPSE,
    wxTREE_EXPAND_COLLAPSE_RESET,
    wxTREE_EXPAND_TOGGLE
};

// flags for deprecated InsertItem() variant
#define wxTREE_INSERT_FIRST 0xFFFF0001
#define wxTREE_INSERT_LAST  0xFFFF0002

// ----------------------------------------------------------------------------
// wxTreeCtrl
// ----------------------------------------------------------------------------
class WXDLLEXPORT wxTreeCtrl : public wxControl
{
public:
    // creation
    // --------
    wxTreeCtrl() { Init(); }

    wxTreeCtrl(wxWindow *parent, wxWindowID id = -1,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT,
               const wxValidator& validator = wxDefaultValidator,
               const wxString& name = wxTreeCtrlNameStr)
    {
        Create(parent, id, pos, size, style, validator, name);
    }

    virtual ~wxTreeCtrl();

    bool Create(wxWindow *parent, wxWindowID id = -1,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxTreeCtrlNameStr);

    // accessors
    // ---------

        // get the total number of items in the control
    size_t GetCount() const;

        // indent is the number of pixels the children are indented relative to
        // the parents position. SetIndent() also redraws the control
        // immediately.
    unsigned int GetIndent() const;
    void SetIndent(unsigned int indent);

    // spacing is the number of pixels between the start and the Text
        // not implemented under wxMSW
    unsigned int GetSpacing() const { return 18; } // return wxGTK default
    void SetSpacing(unsigned int WXUNUSED(spacing)) { }

        // image list: these functions allow to associate an image list with
        // the control and retrieve it. Note that the control does _not_ delete
        // the associated image list when it's deleted in order to allow image
        // lists to be shared between different controls.
        //
        // The normal image list is for the icons which correspond to the
        // normal tree item state (whether it is selected or not).
        // Additionally, the application might choose to show a state icon
        // which corresponds to an app-defined item state (for example,
        // checked/unchecked) which are taken from the state image list.
    wxImageList *GetImageList() const;
    wxImageList *GetStateImageList() const;

    void SetImageList(wxImageList *imageList);
    void SetStateImageList(wxImageList *imageList);
    void AssignImageList(wxImageList *imageList);
    void AssignStateImageList(wxImageList *imageList);

    // Functions to work with tree ctrl items. Unfortunately, they can _not_ be
    // member functions of wxTreeItem because they must know the tree the item
    // belongs to for Windows implementation and storing the pointer to
    // wxTreeCtrl in each wxTreeItem is just too much waste.

    // accessors
    // ---------

        // retrieve items label
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

        // set items label
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

        // the item will be shown with a drop highlight
    void SetItemDropHighlight(const wxTreeItemId& item, bool highlight = TRUE);

        // set the items text colour
    void SetItemTextColour(const wxTreeItemId& item, const wxColour& col);

        // set the items background colour
    void SetItemBackgroundColour(const wxTreeItemId& item, const wxColour& col);

        // set the items font (should be of the same height for all items)
    void SetItemFont(const wxTreeItemId& item, const wxFont& font);

    // item status inquiries
    // ---------------------

        // is the item visible (it might be outside the view or not expanded)?
    bool IsVisible(const wxTreeItemId& item) const;
        // does the item has any children?
    bool ItemHasChildren(const wxTreeItemId& item) const;
        // is the item expanded (only makes sense if HasChildren())?
    bool IsExpanded(const wxTreeItemId& item) const;
        // is this item currently selected (the same as has focus)?
    bool IsSelected(const wxTreeItemId& item) const;
        // is item text in bold font?
    bool IsBold(const wxTreeItemId& item) const;

    // number of children
    // ------------------

        // if 'recursively' is FALSE, only immediate children count, otherwise
        // the returned number is the number of all items in this branch
    size_t GetChildrenCount(const wxTreeItemId& item,
                            bool recursively = TRUE) const;

    // navigation
    // ----------

    // wxTreeItemId.IsOk() will return FALSE if there is no such item

        // get the root tree item
    wxTreeItemId GetRootItem() const;

        // get the item currently selected (may return NULL if no selection)
    wxTreeItemId GetSelection() const;

        // get the items currently selected, return the number of such item
        //
        // NB: this operation is expensive and can take a long time for a
        //     control with a lot of items (~ O(number of items)).
    size_t GetSelections(wxArrayTreeItemIds& selections) const;

        // get the parent of this item (may return NULL if root)
    wxTreeItemId GetItemParent(const wxTreeItemId& item) const;

#if WXWIN_COMPATIBILITY_2_2
        // deprecated:  Use GetItemParent instead.
    wxTreeItemId GetParent(const wxTreeItemId& item) const
    	{ return GetItemParent( item ); }

    	// Expose the base class method hidden by the one above.
    wxWindow *GetParent() const { return wxControl::GetParent(); }
#endif  // WXWIN_COMPATIBILITY_2_2

        // for this enumeration function you must pass in a "cookie" parameter
        // which is opaque for the application but is necessary for the library
        // to make these functions reentrant (i.e. allow more than one
        // enumeration on one and the same object simultaneously). Of course,
        // the "cookie" passed to GetFirstChild() and GetNextChild() should be
        // the same!

        // get the first child of this item
    wxTreeItemId GetFirstChild(const wxTreeItemId& item, long& _cookie) const;
        // get the next child
    wxTreeItemId GetNextChild(const wxTreeItemId& item, long& _cookie) const;
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
        // collapse the item without removing its children
    void Collapse(const wxTreeItemId& item);
        // collapse the item and remove all children
    void CollapseAndReset(const wxTreeItemId& item);
        // toggles the current state
    void Toggle(const wxTreeItemId& item);

        // remove the selection from currently selected item (if any)
    void Unselect();
        // unselect all items (only makes sense for multiple selection control)
    void UnselectAll();
        // select this item
    void SelectItem(const wxTreeItemId& item);
        // make sure this item is visible (expanding the parent item and/or
        // scrolling to this item if necessary)
    void EnsureVisible(const wxTreeItemId& item);
        // scroll to this item (but don't expand its parent)
    void ScrollTo(const wxTreeItemId& item);

        // start editing the item label: this (temporarily) replaces the item
        // with a one line edit control. The item will be selected if it hadn't
        // been before. textCtrlClass parameter allows you to create an edit
        // control of arbitrary user-defined class deriving from wxTextCtrl.
    wxTextCtrl* EditLabel(const wxTreeItemId& item,
                          wxClassInfo* textCtrlClass = CLASSINFO(wxTextCtrl));
        // returns the same pointer as StartEdit() if the item is being edited,
        // NULL otherwise (it's assumed that no more than one item may be
        // edited simultaneously)
    wxTextCtrl* GetEditControl() const;
        // end editing and accept or discard the changes to item label
    void EndEditLabel(const wxTreeItemId& item, bool discardChanges = FALSE);

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

    // helpers
    // -------

        // determine to which item (if any) belongs the given point (the
        // coordinates specified are relative to the client area of tree ctrl)
        // and fill the flags parameter with a bitmask of wxTREE_HITTEST_xxx
        // constants.
        //
        // The first function is more portable (because easier to implement
        // on other platforms), but the second one returns some extra info.
    wxTreeItemId HitTest(const wxPoint& point)
        { int dummy; return HitTest(point, dummy); }
    wxTreeItemId HitTest(const wxPoint& point, int& flags);

        // get the bounding rectangle of the item (or of its label only)
    bool GetBoundingRect(const wxTreeItemId& item,
                         wxRect& rect,
                         bool textOnly = FALSE) const;

    // deprecated
    // ----------

    // these methods are deprecated and will be removed in future versions of
    // wxWindows, they're here for compatibility only, don't use them in new
    // code (the comments indicate why these methods are now useless and how to
    // replace them)

        // use Expand, Collapse, CollapseAndReset or Toggle
    void ExpandItem(const wxTreeItemId& item, int action);

        // use AddRoot, PrependItem or AppendItem
    wxTreeItemId InsertItem(const wxTreeItemId& parent,
                            const wxString& text,
                            int image = -1, int selImage = -1,
                            long insertAfter = wxTREE_INSERT_LAST);

        // use Set/GetImageList and Set/GetStateImageList
    wxImageList *GetImageList(int) const
        { return GetImageList(); }
    void SetImageList(wxImageList *imageList, int)
        { SetImageList(imageList); }

    // use Set/GetItemImage directly
        // get the selected item image
    int GetItemSelectedImage(const wxTreeItemId& item) const
        { return GetItemImage(item, wxTreeItemIcon_Selected); }
        // set the selected item image
    void SetItemSelectedImage(const wxTreeItemId& item, int image)
        { SetItemImage(item, image, wxTreeItemIcon_Selected); }

    // implementation
    // --------------

    virtual long MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);
    virtual bool MSWCommand(WXUINT param, WXWORD id);
    virtual bool MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM *result);

    // override some base class virtuals
    virtual bool SetBackgroundColour(const wxColour &colour);
    virtual bool SetForegroundColour(const wxColour &colour);

    // get/set the check state for the item (only for wxTR_MULTIPLE)
    bool IsItemChecked(const wxTreeItemId& item) const;
    void SetItemCheck(const wxTreeItemId& item, bool check = TRUE);

protected:
    // SetImageList helper
    void SetAnyImageList(wxImageList *imageList, int which);

    // refresh a single item
    void RefreshItem(const wxTreeItemId& item);

    wxTextCtrl  *m_textCtrl;        // used while editing the item label
    wxImageList *m_imageListNormal, // images for tree elements
                *m_imageListState;  // special images for app defined states
    bool         m_ownsImageListNormal, m_ownsImageListState;

private:
    // the common part of all ctors
    void Init();

    // helper functions
    inline bool DoGetItem(wxTreeViewItem *tvItem) const;
    inline void DoSetItem(wxTreeViewItem *tvItem);

    inline void DoExpand(const wxTreeItemId& item, int flag);

    wxTreeItemId DoInsertItem(const wxTreeItemId& parent,
                              wxTreeItemId hInsertAfter,
                              const wxString& text,
                              int image, int selectedImage,
                              wxTreeItemData *data);

    int DoGetItemImageFromData(const wxTreeItemId& item,
                               wxTreeItemIcon which) const;
    void DoSetItemImageFromData(const wxTreeItemId& item,
                                int image,
                                wxTreeItemIcon which) const;
    void DoSetItemImages(const wxTreeItemId& item, int image, int imageSel);

    void DeleteTextCtrl();

    // support for additional item images which we implement using
    // wxTreeItemIndirectData technique - see the comments in msw/treectrl.cpp
    void SetIndirectItemData(const wxTreeItemId& item,
                             class wxTreeItemIndirectData *data);
    bool HasIndirectData(const wxTreeItemId& item) const;
    bool IsDataIndirect(wxTreeItemData *data) const
        { return data && data->GetId().m_pItem == 0; }

    // the hash storing the items attributes (indexed by items ids)
    wxHashTable m_attrs;

    // TRUE if the hash above is not empty
    bool m_hasAnyAttr;

    // used for dragging
    wxDragImage *m_dragImage;

    // Virtual root item, if wxTR_HIDE_ROOT is set.
    void* m_pVirtualRoot;

    // the starting item for selection with Shift
    WXHTREEITEM m_htSelStart;

    friend class wxTreeItemIndirectData;
    friend class wxTreeSortHelper;

    DECLARE_DYNAMIC_CLASS(wxTreeCtrl)
};

#endif // wxUSE_TREECTRL

#endif
    // _WX_TREECTRL_H_
