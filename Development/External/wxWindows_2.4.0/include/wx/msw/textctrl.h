/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/textctrl.h
// Purpose:     wxTextCtrl class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: textctrl.h,v 1.44.2.2 2002/11/04 21:49:42 JS Exp $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TEXTCTRL_H_
#define _WX_TEXTCTRL_H_

#ifdef __GNUG__
    #pragma interface "textctrl.h"
#endif

class WXDLLEXPORT wxTextCtrl : public wxTextCtrlBase
{
public:
    // creation
    // --------

    wxTextCtrl() { Init(); }
    wxTextCtrl(wxWindow *parent, wxWindowID id,
               const wxString& value = wxEmptyString,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = 0,
               const wxValidator& validator = wxDefaultValidator,
               const wxString& name = wxTextCtrlNameStr)
    {
        Init();

        Create(parent, id, value, pos, size, style, validator, name);
    }
    ~wxTextCtrl();

    bool Create(wxWindow *parent, wxWindowID id,
                const wxString& value = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxTextCtrlNameStr);

    // implement base class pure virtuals
    // ----------------------------------

    virtual wxString GetValue() const;
    virtual void SetValue(const wxString& value);

    virtual wxString GetRange(long from, long to) const;

    virtual int GetLineLength(long lineNo) const;
    virtual wxString GetLineText(long lineNo) const;
    virtual int GetNumberOfLines() const;

    virtual bool IsModified() const;
    virtual bool IsEditable() const;

    virtual void GetSelection(long* from, long* to) const;

    // operations
    // ----------

    // editing
    virtual void Clear();
    virtual void Replace(long from, long to, const wxString& value);
    virtual void Remove(long from, long to);

    // load the controls contents from the file
    virtual bool LoadFile(const wxString& file);

    // clears the dirty flag
    virtual void DiscardEdits();

    virtual void SetMaxLength(unsigned long len);

    // writing text inserts it at the current position, appending always
    // inserts it at the end
    virtual void WriteText(const wxString& text);
    virtual void AppendText(const wxString& text);

#ifdef __WIN32__
    virtual bool EmulateKeyPress(const wxKeyEvent& event);
#endif // __WIN32__

#if wxUSE_RICHEDIT
    // apply text attribute to the range of text (only works with richedit
    // controls)
    virtual bool SetStyle(long start, long end, const wxTextAttr& style);
    virtual bool SetDefaultStyle(const wxTextAttr& style);
#endif // wxUSE_RICHEDIT

    // translate between the position (which is just an index in the text ctrl
    // considering all its contents as a single strings) and (x, y) coordinates
    // which represent column and line.
    virtual long XYToPosition(long x, long y) const;
    virtual bool PositionToXY(long pos, long *x, long *y) const;

    virtual void ShowPosition(long pos);

    // Clipboard operations
    virtual void Copy();
    virtual void Cut();
    virtual void Paste();

    virtual bool CanCopy() const;
    virtual bool CanCut() const;
    virtual bool CanPaste() const;

    // Undo/redo
    virtual void Undo();
    virtual void Redo();

    virtual bool CanUndo() const;
    virtual bool CanRedo() const;

    // Insertion point
    virtual void SetInsertionPoint(long pos);
    virtual void SetInsertionPointEnd();
    virtual long GetInsertionPoint() const;
    virtual long GetLastPosition() const;

    virtual void SetSelection(long from, long to);
    virtual void SetEditable(bool editable);

    // Implementation from now on
    // --------------------------

    virtual void SetWindowStyleFlag(long style);

    virtual void Command(wxCommandEvent& event);
    virtual bool MSWCommand(WXUINT param, WXWORD id);
    virtual WXHBRUSH OnCtlColor(WXHDC pDC, WXHWND pWnd, WXUINT nCtlColor,
            WXUINT message, WXWPARAM wParam, WXLPARAM lParam);

    // In WIN16, need to override normal erasing because
    // Ctl3D doesn't use the wxWindows background colour.
#ifdef __WIN16__
    void OnEraseBackground(wxEraseEvent& event);
#endif

#if wxUSE_RICHEDIT
    virtual bool MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM *result);

    int GetRichVersion() const { return m_verRichEdit; }
    bool IsRich() const { return m_verRichEdit != 0; }

    // rich edit controls are not compatible with normal ones and wem ust set
    // the colours for them otherwise
    virtual bool SetBackgroundColour(const wxColour& colour);
    virtual bool SetForegroundColour(const wxColour& colour);
#endif // wxUSE_RICHEDIT

    virtual void AdoptAttributesFromHWND();

    virtual bool AcceptsFocus() const;

    // callbacks
    void OnDropFiles(wxDropFilesEvent& event);
    void OnChar(wxKeyEvent& event); // Process 'enter' if required

    void OnCut(wxCommandEvent& event);
    void OnCopy(wxCommandEvent& event);
    void OnPaste(wxCommandEvent& event);
    void OnUndo(wxCommandEvent& event);
    void OnRedo(wxCommandEvent& event);
    void OnDelete(wxCommandEvent& event);
    void OnSelectAll(wxCommandEvent& event);

    void OnUpdateCut(wxUpdateUIEvent& event);
    void OnUpdateCopy(wxUpdateUIEvent& event);
    void OnUpdatePaste(wxUpdateUIEvent& event);
    void OnUpdateUndo(wxUpdateUIEvent& event);
    void OnUpdateRedo(wxUpdateUIEvent& event);
    void OnUpdateDelete(wxUpdateUIEvent& event);
    void OnUpdateSelectAll(wxUpdateUIEvent& event);

    // Show a context menu for Rich Edit controls (the standard
    // EDIT control has one already)
    void OnRightClick(wxMouseEvent& event);

protected:
    // common part of all ctors
    void Init();

    // intercept WM_GETDLGCODE
    virtual long MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);

    // call this to increase the size limit (will do nothing if the current
    // limit is big enough)
    //
    // returns true if we increased the limit to allow entering more text,
    // false if we hit the limit set by SetMaxLength() and so didn't change it
    bool AdjustSpaceLimit();

#if wxUSE_RICHEDIT
    // replace the selection or the entire control contents with the given text
    // in the specified encoding
    bool StreamIn(const wxString& value,
                  wxFontEncoding encoding,
                  bool selOnly);
#endif // wxUSE_RICHEDIT

    // replace the contents of the selection or of the entire control with the
    // given text
    void DoWriteText(const wxString& text, bool selectionOnly = TRUE);

    // set the selection possibly without scrolling the caret into view
    void DoSetSelection(long from, long to, bool scrollCaret = TRUE);

    // return true if there is a non empty selection in the control
    bool HasSelection() const;

    // get the length of the line containing the character at the given
    // position
    long GetLengthOfLineContainingPos(long pos) const;

    // send TEXT_UPDATED event, return TRUE if it was handled, FALSE otherwise
    bool SendUpdateEvent();

    // override some base class virtuals
    virtual bool MSWShouldPreProcessMessage(WXMSG* pMsg);
    virtual wxSize DoGetBestSize() const;

    virtual WXDWORD MSWGetStyle(long style, WXDWORD *exstyle) const;

#if wxUSE_RICHEDIT
    // we're using RICHEDIT (and not simple EDIT) control if this field is not
    // 0, it also gives the version of the RICHEDIT control being used (1, 2 or
    // 3 so far)
    int m_verRichEdit;
#endif // wxUSE_RICHEDIT

    // if TRUE, SendUpdateEvent() will eat the next event (see comments in the
    // code as to why this is needed)
    bool m_suppressNextUpdate;

private:
    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxTextCtrl)

    wxMenu* m_privateContextMenu;
};

#endif
    // _WX_TEXTCTRL_H_
