/////////////////////////////////////////////////////////////////////////////
// Name:        wx/tbarbase.h
// Purpose:     Base class for toolbar classes
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: tbarbase.h,v 1.38 2002/08/31 11:29:11 GD Exp $
// Copyright:   (c) Julian Smart and Markus Holzem
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TBARBASE_H_
#define _WX_TBARBASE_H_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "tbarbase.h"
#endif

#include "wx/defs.h"

#if wxUSE_TOOLBAR

#include "wx/bitmap.h"
#include "wx/list.h"
#include "wx/control.h"

class WXDLLEXPORT wxToolBarBase;
class WXDLLEXPORT wxToolBarToolBase;
class WXDLLEXPORT wxImage;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

WXDLLEXPORT_DATA(extern const wxChar*) wxToolBarNameStr;
WXDLLEXPORT_DATA(extern const wxSize) wxDefaultSize;
WXDLLEXPORT_DATA(extern const wxPoint) wxDefaultPosition;

enum wxToolBarToolStyle
{
    wxTOOL_STYLE_BUTTON    = 1,
    wxTOOL_STYLE_SEPARATOR = 2,
    wxTOOL_STYLE_CONTROL
};

// ----------------------------------------------------------------------------
// wxToolBarTool is a toolbar element.
//
// It has a unique id (except for the separators which always have id -1), the
// style (telling whether it is a normal button, separator or a control), the
// state (toggled or not, enabled or not) and short and long help strings. The
// default implementations use the short help string for the tooltip text which
// is popped up when the mouse pointer enters the tool and the long help string
// for the applications status bar.
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxToolBarToolBase : public wxObject
{
public:
    // ctors & dtor
    // ------------

    wxToolBarToolBase(wxToolBarBase *tbar = (wxToolBarBase *)NULL,
                      int id = wxID_SEPARATOR,
                      const wxString& label = wxEmptyString,
                      const wxBitmap& bmpNormal = wxNullBitmap,
                      const wxBitmap& bmpDisabled = wxNullBitmap,
                      wxItemKind kind = wxITEM_NORMAL,
                      wxObject *clientData = (wxObject *) NULL,
                      const wxString& shortHelpString = wxEmptyString,
                      const wxString& longHelpString = wxEmptyString)
        : m_label(label),
          m_shortHelpString(shortHelpString),
          m_longHelpString(longHelpString)
    {
        m_tbar = tbar;
        m_id = id;
        m_clientData = clientData;

        m_bmpNormal = bmpNormal;
        m_bmpDisabled = bmpDisabled;

        m_kind = kind;

        m_enabled = TRUE;
        m_toggled = FALSE;

        m_toolStyle = id == wxID_SEPARATOR ? wxTOOL_STYLE_SEPARATOR
                                           : wxTOOL_STYLE_BUTTON;
    }

    wxToolBarToolBase(wxToolBarBase *tbar, wxControl *control)
    {
        m_tbar = tbar;
        m_control = control;
        m_id = control->GetId();

        m_kind = wxITEM_MAX;    // invalid value

        m_enabled = TRUE;
        m_toggled = FALSE;

        m_toolStyle = wxTOOL_STYLE_CONTROL;
    }

    ~wxToolBarToolBase();

    // accessors
    // ---------

    // general
    int GetId() const { return m_id; }

    wxControl *GetControl() const
    {
        wxASSERT_MSG( IsControl(), _T("this toolbar tool is not a control") );

        return m_control;
    }

    wxToolBarBase *GetToolBar() const { return m_tbar; }

    // style
    bool IsButton() const { return m_toolStyle == wxTOOL_STYLE_BUTTON; }
    bool IsControl() const { return m_toolStyle == wxTOOL_STYLE_CONTROL; }
    bool IsSeparator() const { return m_toolStyle == wxTOOL_STYLE_SEPARATOR; }
    int GetStyle() const { return m_toolStyle; }
    wxItemKind GetKind() const
    {
        wxASSERT_MSG( IsButton(), _T("only makes sense for buttons") );

        return m_kind;
    }

    // state
    bool IsEnabled() const { return m_enabled; }
    bool IsToggled() const { return m_toggled; }
    bool CanBeToggled() const
        { return m_kind == wxITEM_CHECK || m_kind == wxITEM_RADIO; }

    // attributes
    const wxBitmap& GetNormalBitmap() const { return m_bmpNormal; }
    const wxBitmap& GetDisabledBitmap() const { return m_bmpDisabled; }

    const wxBitmap& GetBitmap() const
        { return IsEnabled() ? GetNormalBitmap() : GetDisabledBitmap(); }

    wxString GetLabel() const { return m_label; }

    wxString GetShortHelp() const { return m_shortHelpString; }
    wxString GetLongHelp() const { return m_longHelpString; }

    wxObject *GetClientData() const
    {
        if ( m_toolStyle == wxTOOL_STYLE_CONTROL )
        {
            return (wxObject*)m_control->GetClientData();
        }
        else
        {
            return m_clientData;
        }
    }

    // modifiers: return TRUE if the state really changed
    bool Enable(bool enable);
    bool Toggle(bool toggle);
    bool SetToggle(bool toggle);
    bool SetShortHelp(const wxString& help);
    bool SetLongHelp(const wxString& help);

    void Toggle() { Toggle(!IsToggled()); }

    void SetNormalBitmap(const wxBitmap& bmp) { m_bmpNormal = bmp; }
    void SetDisabledBitmap(const wxBitmap& bmp) { m_bmpDisabled = bmp; }

    virtual void SetLabel(const wxString& label) { m_label = label; }

    void SetClientData(wxObject *clientData)
    {
        if ( m_toolStyle == wxTOOL_STYLE_CONTROL )
        {
            m_control->SetClientData(clientData);
        }
        else
        {
            m_clientData = clientData;
        }
    }

    // add tool to/remove it from a toolbar
    virtual void Detach() { m_tbar = (wxToolBarBase *)NULL; }
    virtual void Attach(wxToolBarBase *tbar) { m_tbar = tbar; }

    // compatibility only, don't use
#if WXWIN_COMPATIBILITY_2_2
    const wxBitmap& GetBitmap1() const { return GetNormalBitmap(); }
    const wxBitmap& GetBitmap2() const { return GetDisabledBitmap(); }

    void SetBitmap1(const wxBitmap& bmp) { SetNormalBitmap(bmp); }
    void SetBitmap2(const wxBitmap& bmp) { SetDisabledBitmap(bmp); }
#endif // WXWIN_COMPATIBILITY_2_2

protected:
    wxToolBarBase *m_tbar;  // the toolbar to which we belong (may be NULL)

    // tool parameters
    int m_toolStyle;    // see enum wxToolBarToolStyle
    int m_id;           // the tool id, wxID_SEPARATOR for separator
    wxItemKind m_kind;  // for normal buttons may be wxITEM_NORMAL/CHECK/RADIO

    // as controls have their own client data, no need to waste memory
    union
    {
        wxObject         *m_clientData;
        wxControl        *m_control;
    };

    // tool state
    bool m_toggled;
    bool m_enabled;

    // normal and disabled bitmaps for the tool, both can be invalid
    wxBitmap m_bmpNormal;
    wxBitmap m_bmpDisabled;

    // the button label
    wxString m_label;

    // short and long help strings
    wxString m_shortHelpString;
    wxString m_longHelpString;
};

// a list of toolbar tools
WX_DECLARE_EXPORTED_LIST(wxToolBarToolBase, wxToolBarToolsList);

// ----------------------------------------------------------------------------
// the base class for all toolbars
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxToolBarBase : public wxControl
{
public:
    wxToolBarBase();
    virtual ~wxToolBarBase();

    // toolbar construction
    // --------------------

    // the full AddTool() function
    //
    // If bmpDisabled is wxNullBitmap, a shadowed version of the normal bitmap
    // is created and used as the disabled image.
    wxToolBarToolBase *AddTool(int id,
                               const wxString& label,
                               const wxBitmap& bitmap,
                               const wxBitmap& bmpDisabled,
                               wxItemKind kind = wxITEM_NORMAL,
                               const wxString& shortHelp = wxEmptyString,
                               const wxString& longHelp = wxEmptyString,
                               wxObject *data = NULL)
    {
        return DoAddTool(id, label, bitmap, bmpDisabled, kind,
                         shortHelp, longHelp, data);
    }

    // the most common AddTool() version
    wxToolBarToolBase *AddTool(int id,
                               const wxString& label,
                               const wxBitmap& bitmap,
                               const wxString& shortHelp = wxEmptyString,
                               wxItemKind kind = wxITEM_NORMAL)
    {
        return AddTool(id, label, bitmap, wxNullBitmap, kind, shortHelp);
    }

    // add a check tool, i.e. a tool which can be toggled
    wxToolBarToolBase *AddCheckTool(int id,
                                    const wxString& label,
                                    const wxBitmap& bitmap,
                                    const wxBitmap& bmpDisabled = wxNullBitmap,
                                    const wxString& shortHelp = wxEmptyString,
                                    const wxString& longHelp = wxEmptyString,
                                    wxObject *data = NULL)
    {
        return AddTool(id, label, bitmap, bmpDisabled, wxITEM_CHECK,
                       shortHelp, longHelp, data);
    }

    // add a radio tool, i.e. a tool which can be toggled and releases any
    // other toggled radio tools in the same group when it happens
    wxToolBarToolBase *AddRadioTool(int id,
                                    const wxString& label,
                                    const wxBitmap& bitmap,
                                    const wxBitmap& bmpDisabled = wxNullBitmap,
                                    const wxString& shortHelp = wxEmptyString,
                                    const wxString& longHelp = wxEmptyString,
                                    wxObject *data = NULL)
    {
        return AddTool(id, label, bitmap, bmpDisabled, wxITEM_RADIO,
                       shortHelp, longHelp, data);
    }


    // insert the new tool at the given position, if pos == GetToolsCount(), it
    // is equivalent to AddTool()
    virtual wxToolBarToolBase *InsertTool
                               (
                                    size_t pos,
                                    int id,
                                    const wxString& label,
                                    const wxBitmap& bitmap,
                                    const wxBitmap& bmpDisabled = wxNullBitmap,
                                    wxItemKind kind = wxITEM_NORMAL,
                                    const wxString& shortHelp = wxEmptyString,
                                    const wxString& longHelp = wxEmptyString,
                                    wxObject *clientData = NULL
                               );

    // add an arbitrary control to the toolbar, return TRUE if ok (notice that
    // the control will be deleted by the toolbar and that it will also adjust
    // its position/size)
    //
    // NB: the control should have toolbar as its parent
    virtual wxToolBarToolBase *AddControl(wxControl *control);
    virtual wxToolBarToolBase *InsertControl(size_t pos, wxControl *control);
    
    // get the control with the given id or return NULL
    virtual wxControl *FindControl( int id );

    // add a separator to the toolbar
    virtual wxToolBarToolBase *AddSeparator();
    virtual wxToolBarToolBase *InsertSeparator(size_t pos);

    // remove the tool from the toolbar: the caller is responsible for actually
    // deleting the pointer
    virtual wxToolBarToolBase *RemoveTool(int id);

    // delete tool either by index or by position
    virtual bool DeleteToolByPos(size_t pos);
    virtual bool DeleteTool(int id);

    // delete all tools
    virtual void ClearTools();

    // must be called after all buttons have been created to finish toolbar
    // initialisation
    virtual bool Realize();

    // tools state
    // -----------

    virtual void EnableTool(int id, bool enable);
    virtual void ToggleTool(int id, bool toggle);

    // Set this to be togglable (or not)
    virtual void SetToggle(int id, bool toggle);

    // set/get tools client data (not for controls)
    virtual wxObject *GetToolClientData(int id) const;
    virtual void SetToolClientData(int id, wxObject *clientData);

    // return TRUE if the tool is toggled
    virtual bool GetToolState(int id) const;

    virtual bool GetToolEnabled(int id) const;

    virtual void SetToolShortHelp(int id, const wxString& helpString);
    virtual wxString GetToolShortHelp(int id) const;
    virtual void SetToolLongHelp(int id, const wxString& helpString);
    virtual wxString GetToolLongHelp(int id) const;

    // margins/packing/separation
    // --------------------------

    virtual void SetMargins(int x, int y);
    void SetMargins(const wxSize& size)
        { SetMargins((int) size.x, (int) size.y); }
    virtual void SetToolPacking(int packing)
        { m_toolPacking = packing; }
    virtual void SetToolSeparation(int separation)
        { m_toolSeparation = separation; }

    virtual wxSize GetToolMargins() const { return wxSize(m_xMargin, m_yMargin); }
    virtual int GetToolPacking() const { return m_toolPacking; }
    virtual int GetToolSeparation() const { return m_toolSeparation; }

    // toolbar geometry
    // ----------------

    // set the number of toolbar rows
    virtual void SetRows(int nRows);

    // the toolbar can wrap - limit the number of columns or rows it may take
    void SetMaxRowsCols(int rows, int cols)
        { m_maxRows = rows; m_maxCols = cols; }
    int GetMaxRows() const { return m_maxRows; }
    int GetMaxCols() const { return m_maxCols; }

    // get/set the size of the bitmaps used by the toolbar: should be called
    // before adding any tools to the toolbar
    virtual void SetToolBitmapSize(const wxSize& size)
        { m_defaultWidth = size.x; m_defaultHeight = size.y; };
    virtual wxSize GetToolBitmapSize() const
        { return wxSize(m_defaultWidth, m_defaultHeight); }

    // the button size in some implementations is bigger than the bitmap size:
    // get the total button size (by default the same as bitmap size)
    virtual wxSize GetToolSize() const
        { return GetToolBitmapSize(); } ;

    // returns a (non separator) tool containing the point (x, y) or NULL if
    // there is no tool at this point (corrdinates are client)
    virtual wxToolBarToolBase *FindToolForPosition(wxCoord x,
                                                   wxCoord y) const = 0;

    // return TRUE if this is a vertical toolbar, otherwise FALSE
    bool IsVertical() const { return HasFlag(wxTB_VERTICAL); }


    // the old versions of the various methods kept for compatibility
    // don't use in the new code!
    // --------------------------------------------------------------

    wxToolBarToolBase *AddTool(int id,
                               const wxBitmap& bitmap,
                               const wxBitmap& bmpDisabled,
                               bool toggle = FALSE,
                               wxObject *clientData = NULL,
                               const wxString& shortHelpString = wxEmptyString,
                               const wxString& longHelpString = wxEmptyString)
    {
        return AddTool(id, wxEmptyString,
                       bitmap, bmpDisabled,
                       toggle ? wxITEM_CHECK : wxITEM_NORMAL,
                       shortHelpString, longHelpString, clientData);
    }

    wxToolBarToolBase *AddTool(int id,
                               const wxBitmap& bitmap,
                               const wxString& shortHelpString = wxEmptyString,
                               const wxString& longHelpString = wxEmptyString)
    {
        return AddTool(id, wxEmptyString,
                       bitmap, wxNullBitmap, wxITEM_NORMAL,
                       shortHelpString, longHelpString, NULL);
    }

    wxToolBarToolBase *AddTool(int id,
                               const wxBitmap& bitmap,
                               const wxBitmap& bmpDisabled,
                               bool toggle,
                               wxCoord xPos,
                               wxCoord yPos = -1,
                               wxObject *clientData = NULL,
                               const wxString& shortHelp = wxEmptyString,
                               const wxString& longHelp = wxEmptyString)
    {
        return DoAddTool(id, wxEmptyString, bitmap, bmpDisabled,
                         toggle ? wxITEM_CHECK : wxITEM_NORMAL,
                         shortHelp, longHelp, clientData, xPos, yPos);
    }

    wxToolBarToolBase *InsertTool(size_t pos,
                                  int id,
                                  const wxBitmap& bitmap,
                                  const wxBitmap& bmpDisabled = wxNullBitmap,
                                  bool toggle = FALSE,
                                  wxObject *clientData = NULL,
                                  const wxString& shortHelp = wxEmptyString,
                                  const wxString& longHelp = wxEmptyString)
    {
        return InsertTool(pos, id, wxEmptyString, bitmap, bmpDisabled,
                          toggle ? wxITEM_CHECK : wxITEM_NORMAL,
                          shortHelp, longHelp, clientData);
    }

    // event handlers
    // --------------

    // NB: these functions are deprecated, use EVT_TOOL_XXX() instead!

    // Only allow toggle if returns TRUE. Call when left button up.
    virtual bool OnLeftClick(int id, bool toggleDown);

    // Call when right button down.
    virtual void OnRightClick(int id, long x, long y);

    // Called when the mouse cursor enters a tool bitmap.
    // Argument is -1 if mouse is exiting the toolbar.
    virtual void OnMouseEnter(int id);

    // more deprecated functions
    // -------------------------

#if WXWIN_COMPATIBILITY
    void SetDefaultSize(int w, int h) { SetDefaultSize(wxSize(w, h)); }
    long GetDefaultWidth() const { return m_defaultWidth; }
    long GetDefaultHeight() const { return m_defaultHeight; }
    int GetDefaultButtonWidth() const { return (int) GetDefaultButtonSize().x; };
    int GetDefaultButtonHeight() const { return (int) GetDefaultButtonSize().y; };
    virtual void SetDefaultSize(const wxSize& size) { SetToolBitmapSize(size); }
    virtual wxSize GetDefaultSize() const { return GetToolBitmapSize(); }
    virtual wxSize GetDefaultButtonSize() const { return GetToolSize(); }
#endif // WXWIN_COMPATIBILITY

    // use GetToolMargins() instead
    wxSize GetMargins() const { return GetToolMargins(); }

    // implementation only from now on
    // -------------------------------

    size_t GetToolsCount() const { return m_tools.GetCount(); }

    void OnIdle(wxIdleEvent& event);

    // Do the toolbar button updates (check for EVT_UPDATE_UI handlers)
    virtual void DoToolbarUpdates();

    // don't want toolbars to accept the focus
    virtual bool AcceptsFocus() const { return FALSE; }

protected:
    // to implement in derived classes
    // -------------------------------

    // create a new toolbar tool and add it to the toolbar, this is typically
    // implemented by just calling InsertTool()
    virtual wxToolBarToolBase *DoAddTool
                               (
                                   int id,
                                   const wxString& label,
                                   const wxBitmap& bitmap,
                                   const wxBitmap& bmpDisabled,
                                   wxItemKind kind,
                                   const wxString& shortHelp = wxEmptyString,
                                   const wxString& longHelp = wxEmptyString,
                                   wxObject *clientData = NULL,
                                   wxCoord xPos = -1,
                                   wxCoord yPos = -1
                               );

    // the tool is not yet inserted into m_tools list when this function is
    // called and will only be added to it if this function succeeds
    virtual bool DoInsertTool(size_t pos, wxToolBarToolBase *tool) = 0;

    // the tool is still in m_tools list when this function is called, it will
    // only be deleted from it if it succeeds
    virtual bool DoDeleteTool(size_t pos, wxToolBarToolBase *tool) = 0;

    // called when the tools enabled flag changes
    virtual void DoEnableTool(wxToolBarToolBase *tool, bool enable) = 0;

    // called when the tool is toggled
    virtual void DoToggleTool(wxToolBarToolBase *tool, bool toggle) = 0;

    // called when the tools "can be toggled" flag changes
    virtual void DoSetToggle(wxToolBarToolBase *tool, bool toggle) = 0;

    // the functions to create toolbar tools
    virtual wxToolBarToolBase *CreateTool(int id,
                                          const wxString& label,
                                          const wxBitmap& bmpNormal,
                                          const wxBitmap& bmpDisabled,
                                          wxItemKind kind,
                                          wxObject *clientData,
                                          const wxString& shortHelp,
                                          const wxString& longHelp) = 0;

    virtual wxToolBarToolBase *CreateTool(wxControl *control) = 0;

    // helper functions
    // ----------------

    // find the tool by id
    wxToolBarToolBase *FindById(int id) const;

    // the list of all our tools
    wxToolBarToolsList m_tools;

    // the offset of the first tool
    int m_xMargin;
    int m_yMargin;

    // the maximum number of toolbar rows/columns
    int m_maxRows;
    int m_maxCols;

    // the tool packing and separation
    int m_toolPacking,
        m_toolSeparation;

    // the size of the toolbar bitmaps
    wxCoord m_defaultWidth, m_defaultHeight;

private:
    DECLARE_EVENT_TABLE()
    DECLARE_CLASS(wxToolBarBase)
};

// Helper function for creating the image for disabled buttons
bool wxCreateGreyedImage(const wxImage& in, wxImage& out) ;

#endif // wxUSE_TOOLBAR

#endif
    // _WX_TBARBASE_H_

