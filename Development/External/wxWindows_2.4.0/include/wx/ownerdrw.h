///////////////////////////////////////////////////////////////////////////////
// Name:        ownerdrw.h
// Purpose:     interface for owner-drawn GUI elements
// Author:      Vadim Zeitlin
// Modified by:
// Created:     11.11.97
// RCS-ID:      $Id: ownerdrw.h,v 1.17 2002/09/02 19:15:40 GD Exp $
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

#ifndef   _OWNERDRW_H
#define   _OWNERDRW_H

#if wxUSE_OWNER_DRAWN

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "ownerdrw.h"
#endif

#include "wx/bitmap.h"
#include "wx/colour.h"
#include "wx/font.h"

// ----------------------------------------------------------------------------
// wxOwnerDrawn - a mix-in base class, derive from it to implement owner-drawn
//                behaviour
//
// wxOwnerDrawn supports drawing of an item with non standard font, color and
// also supports 3 bitmaps: either a checked/unchecked bitmap for a checkable
// element or one unchangeable bitmap otherwise.
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxOwnerDrawn
{
public:
  // ctor & dtor
  wxOwnerDrawn(const wxString& str = wxEmptyString,
               bool bCheckable = FALSE,
               bool bMenuItem = FALSE); // FIXME kludge for colors
  virtual ~wxOwnerDrawn() { }

  // fix appearance
  void SetFont(const wxFont& font)
      { m_font = font; m_bOwnerDrawn = TRUE; }

  wxFont& GetFont() const { return (wxFont &)m_font; }

  void SetTextColour(const wxColour& colText)
      { m_colText = colText; m_bOwnerDrawn = TRUE; }

  wxColour& GetTextColour() const { return (wxColour&) m_colText; }

  void SetBackgroundColour(const wxColour& colBack)
      { m_colBack = colBack; m_bOwnerDrawn = TRUE; }

  wxColour& GetBackgroundColour() const
      { return (wxColour&) m_colBack ; }

  void SetBitmaps(const wxBitmap& bmpChecked,
                  const wxBitmap& bmpUnchecked = wxNullBitmap)
      { m_bmpChecked = bmpChecked;
        m_bmpUnchecked = bmpUnchecked;
        m_bOwnerDrawn = TRUE; }

  void SetBitmap(const wxBitmap& bmpChecked)
      { m_bmpChecked = bmpChecked;
        m_bOwnerDrawn = TRUE; }

  const wxBitmap& GetBitmap(bool bChecked = TRUE) const
      { return (bChecked ? m_bmpChecked : m_bmpUnchecked); }

  // the height of the menu checkmark (or bitmap) is determined by the font
  // for the current item, but the width should be always the same (for the
  // items to be aligned), so by default it's taken to be the same as for
  // the last item (and default width for the first one).
  //
  // NB: default is too small for bitmaps, but ok for checkmarks.
  void SetMarginWidth(int nWidth)
  {
      ms_nLastMarginWidth = m_nMarginWidth = (size_t) nWidth;
      if ( ((size_t) nWidth) != ms_nDefaultMarginWidth )
          m_bOwnerDrawn = TRUE;
  }

  int GetMarginWidth() const { return (int) m_nMarginWidth; }
  static int GetDefaultMarginWidth() { return (int) ms_nDefaultMarginWidth; }

  // accessors
  void SetName(const wxString& strName)  { m_strName = strName; }
  const wxString& GetName() const { return m_strName;    }
  void SetCheckable(bool checkable) { m_bCheckable = checkable; }
  bool IsCheckable() const { return m_bCheckable; }

  // this is for menu items only: accel string is drawn right aligned after the
  // menu item if not empty
  void SetAccelString(const wxString& strAccel) { m_strAccel = strAccel; }

  // this function might seem strange, but if it returns FALSE it means that
  // no non-standard attribute are set, so there is no need for this control
  // to be owner-drawn. Moreover, you can force owner-drawn to FALSE if you
  // want to change, say, the color for the item but only if it is owner-drawn
  // (see wxMenuItem::wxMenuItem for example)
  bool IsOwnerDrawn() const { return m_bOwnerDrawn;   }
  void ResetOwnerDrawn() { m_bOwnerDrawn = FALSE;  }

public:
  // constants used in OnDrawItem
  // (they have the same values as corresponding Win32 constants)
  enum wxODAction
  {
    wxODDrawAll       = 0x0001,   // redraw entire control
    wxODSelectChanged = 0x0002,   // selection changed (see Status.Select)
    wxODFocusChanged  = 0x0004    // keyboard focus changed (see Status.Focus)
  };

  enum wxODStatus
  {
    wxODSelected  = 0x0001,         // control is currently selected
    wxODGrayed    = 0x0002,         // item is to be grayed
    wxODDisabled  = 0x0004,         // item is to be drawn as disabled
    wxODChecked   = 0x0008,         // item is to be checked
    wxODHasFocus  = 0x0010,         // item has the keyboard focus
    wxODDefault   = 0x0020          // item is the default item
  };

  // virtual functions to implement drawing (return TRUE if processed)
  virtual bool OnMeasureItem(size_t *pwidth, size_t *pheight);
  virtual bool OnDrawItem(wxDC& dc, const wxRect& rc, wxODAction act, wxODStatus stat);

protected:
  wxString  m_strName,      // label for a manu item
            m_strAccel;     // the accel string ("Ctrl-F17") if any

private:
  static size_t ms_nDefaultMarginWidth; // menu check mark width
  static size_t ms_nLastMarginWidth;    // handy for aligning all items

  bool      m_bCheckable,   // used only for menu or check listbox items
            m_bOwnerDrawn;  // true if something is non standard

  wxFont    m_font;         // font to use for drawing
  wxColour  m_colText,      // color ----"---"---"----
            m_colBack;      // background color
  wxBitmap  m_bmpChecked,   // bitmap to put near the item
            m_bmpUnchecked; // (checked is used also for 'uncheckable' items)

  size_t    m_nHeight,      // font height
            m_nMinHeight,   // minimum height, as determined by user's system settings
            m_nMarginWidth; // space occupied by bitmap to the left of the item
};

#endif // wxUSE_OWNER_DRAWN

#endif
  // _OWNERDRW_H
