// Name:        univ/themes/win32.cpp
// Purpose:     wxUniversal theme implementing Win32-like LNF
// Author:      Vadim Zeitlin
// Modified by:
// Created:     06.08.00
// RCS-ID:      $Id: win32.cpp,v 1.53 2002/04/20 23:36:16 VZ Exp $
// Copyright:   (c) 2000 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

// ===========================================================================
// declarations
// ===========================================================================

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/timer.h"
    #include "wx/intl.h"
    #include "wx/dc.h"
    #include "wx/window.h"

    #include "wx/dcmemory.h"

    #include "wx/button.h"
    #include "wx/listbox.h"
    #include "wx/checklst.h"
    #include "wx/combobox.h"
    #include "wx/scrolbar.h"
    #include "wx/slider.h"
    #include "wx/textctrl.h"
    #include "wx/toolbar.h"

    #ifdef __WXMSW__
        // for COLOR_* constants
        #include "wx/msw/private.h"
    #endif
#endif // WX_PRECOMP

#include "wx/notebook.h"
#include "wx/spinbutt.h"
#include "wx/settings.h"
#include "wx/menu.h"
#include "wx/artprov.h"
#include "wx/toplevel.h"

#include "wx/univ/scrtimer.h"
#include "wx/univ/renderer.h"
#include "wx/univ/inphand.h"
#include "wx/univ/colschem.h"
#include "wx/univ/theme.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

static const int BORDER_THICKNESS = 2;

// the offset between the label and focus rect around it
static const int FOCUS_RECT_OFFSET_X = 1;
static const int FOCUS_RECT_OFFSET_Y = 1;

static const int FRAME_BORDER_THICKNESS            = 3;
static const int RESIZEABLE_FRAME_BORDER_THICKNESS = 4;
static const int FRAME_TITLEBAR_HEIGHT             = 18;
static const int FRAME_BUTTON_WIDTH                = 16;
static const int FRAME_BUTTON_HEIGHT               = 14;

static const size_t NUM_STATUSBAR_GRIP_BANDS = 3;
static const size_t WIDTH_STATUSBAR_GRIP_BAND = 4;
static const size_t STATUSBAR_GRIP_SIZE =
    WIDTH_STATUSBAR_GRIP_BAND*NUM_STATUSBAR_GRIP_BANDS;

enum IndicatorType
{
    IndicatorType_Check,
    IndicatorType_Radio,
    IndicatorType_Menu,
    IndicatorType_Max
};

enum IndicatorState
{
    IndicatorState_Normal,
    IndicatorState_Pressed, // this one is for check/radioboxes
    IndicatorState_Selected = IndicatorState_Pressed, // for menus
    IndicatorState_Disabled,
    IndicatorState_SelectedDisabled,    // only for the menus
    IndicatorState_Max
};

enum IndicatorStatus
{
    IndicatorStatus_Checked,
    IndicatorStatus_Unchecked,
    IndicatorStatus_Max
};

// wxWin32Renderer: draw the GUI elements in Win32 style
// ----------------------------------------------------------------------------

class wxWin32Renderer : public wxRenderer
{
public:
    // constants
    enum wxArrowDirection
    {
        Arrow_Left,
        Arrow_Right,
        Arrow_Up,
        Arrow_Down,
        Arrow_Max
    };

    enum wxArrowStyle
    {
        Arrow_Normal,
        Arrow_Disabled,
        Arrow_Pressed,
        Arrow_Inversed,
        Arrow_InversedDisabled,
        Arrow_StateMax
    };

    enum wxFrameButtonType
    {
        FrameButton_Close,
        FrameButton_Minimize,
        FrameButton_Maximize,
        FrameButton_Restore,
        FrameButton_Help,
        FrameButton_Max
    };

    // ctor
    wxWin32Renderer(const wxColourScheme *scheme);

    // implement the base class pure virtuals
    virtual void DrawBackground(wxDC& dc,
                                const wxColour& col,
                                const wxRect& rect,
                                int flags = 0,
                                wxWindow *window = NULL);
    virtual void DrawLabel(wxDC& dc,
                           const wxString& label,
                           const wxRect& rect,
                           int flags = 0,
                           int alignment = wxALIGN_LEFT | wxALIGN_TOP,
                           int indexAccel = -1,
                           wxRect *rectBounds = NULL);
    virtual void DrawButtonLabel(wxDC& dc,
                                 const wxString& label,
                                 const wxBitmap& image,
                                 const wxRect& rect,
                                 int flags = 0,
                                 int alignment = wxALIGN_LEFT | wxALIGN_TOP,
                                 int indexAccel = -1,
                                 wxRect *rectBounds = NULL);
    virtual void DrawBorder(wxDC& dc,
                            wxBorder border,
                            const wxRect& rect,
                            int flags = 0,
                            wxRect *rectIn = (wxRect *)NULL);
    virtual void DrawHorizontalLine(wxDC& dc,
                                    wxCoord y, wxCoord x1, wxCoord x2);
    virtual void DrawVerticalLine(wxDC& dc,
                                  wxCoord x, wxCoord y1, wxCoord y2);
    virtual void DrawFrame(wxDC& dc,
                           const wxString& label,
                           const wxRect& rect,
                           int flags = 0,
                           int alignment = wxALIGN_LEFT,
                           int indexAccel = -1);
    virtual void DrawTextBorder(wxDC& dc,
                                wxBorder border,
                                const wxRect& rect,
                                int flags = 0,
                                wxRect *rectIn = (wxRect *)NULL);
    virtual void DrawButtonBorder(wxDC& dc,
                                  const wxRect& rect,
                                  int flags = 0,
                                  wxRect *rectIn = (wxRect *)NULL);
    virtual void DrawArrow(wxDC& dc,
                           wxDirection dir,
                           const wxRect& rect,
                           int flags = 0);
    virtual void DrawScrollbarArrow(wxDC& dc,
                                    wxDirection dir,
                                    const wxRect& rect,
                                    int flags = 0)
        { DrawArrow(dc, dir, rect, flags); }
    virtual void DrawScrollbarThumb(wxDC& dc,
                                    wxOrientation orient,
                                    const wxRect& rect,
                                    int flags = 0);
    virtual void DrawScrollbarShaft(wxDC& dc,
                                    wxOrientation orient,
                                    const wxRect& rect,
                                    int flags = 0);
    virtual void DrawScrollCorner(wxDC& dc,
                                  const wxRect& rect);
    virtual void DrawItem(wxDC& dc,
                          const wxString& label,
                          const wxRect& rect,
                          int flags = 0);
    virtual void DrawCheckItem(wxDC& dc,
                               const wxString& label,
                               const wxBitmap& bitmap,
                               const wxRect& rect,
                               int flags = 0);
    virtual void DrawCheckButton(wxDC& dc,
                                 const wxString& label,
                                 const wxBitmap& bitmap,
                                 const wxRect& rect,
                                 int flags = 0,
                                 wxAlignment align = wxALIGN_LEFT,
                                 int indexAccel = -1);
    virtual void DrawRadioButton(wxDC& dc,
                                 const wxString& label,
                                 const wxBitmap& bitmap,
                                 const wxRect& rect,
                                 int flags = 0,
                                 wxAlignment align = wxALIGN_LEFT,
                                 int indexAccel = -1);
    virtual void DrawToolBarButton(wxDC& dc,
                                   const wxString& label,
                                   const wxBitmap& bitmap,
                                   const wxRect& rect,
                                   int flags);
    virtual void DrawTextLine(wxDC& dc,
                              const wxString& text,
                              const wxRect& rect,
                              int selStart = -1,
                              int selEnd = -1,
                              int flags = 0);
    virtual void DrawLineWrapMark(wxDC& dc, const wxRect& rect);
    virtual void DrawTab(wxDC& dc,
                         const wxRect& rect,
                         wxDirection dir,
                         const wxString& label,
                         const wxBitmap& bitmap = wxNullBitmap,
                         int flags = 0,
                         int indexAccel = -1);

    virtual void DrawSliderShaft(wxDC& dc,
                                 const wxRect& rect,
                                 wxOrientation orient,
                                 int flags = 0,
                                 wxRect *rectShaft = NULL);
    virtual void DrawSliderThumb(wxDC& dc,
                                 const wxRect& rect,
                                 wxOrientation orient,
                                 int flags = 0);
    virtual void DrawSliderTicks(wxDC& dc,
                                 const wxRect& rect,
                                 const wxSize& sizeThumb,
                                 wxOrientation orient,
                                 int start,
                                 int end,
                                 int step = 1,
                                 int flags = 0);

    virtual void DrawMenuBarItem(wxDC& dc,
                                 const wxRect& rect,
                                 const wxString& label,
                                 int flags = 0,
                                 int indexAccel = -1);
    virtual void DrawMenuItem(wxDC& dc,
                              wxCoord y,
                              const wxMenuGeometryInfo& geometryInfo,
                              const wxString& label,
                              const wxString& accel,
                              const wxBitmap& bitmap = wxNullBitmap,
                              int flags = 0,
                              int indexAccel = -1);
    virtual void DrawMenuSeparator(wxDC& dc,
                                   wxCoord y,
                                   const wxMenuGeometryInfo& geomInfo);

    virtual void DrawStatusField(wxDC& dc,
                                 const wxRect& rect,
                                 const wxString& label,
                                 int flags = 0);

    // titlebars
    virtual void DrawFrameTitleBar(wxDC& dc,
                                   const wxRect& rect,
                                   const wxString& title,
                                   const wxIcon& icon,
                                   int flags,
                                   int specialButton = 0,
                                   int specialButtonFlags = 0);
    virtual void DrawFrameBorder(wxDC& dc,
                                 const wxRect& rect,
                                 int flags);
    virtual void DrawFrameBackground(wxDC& dc,
                                     const wxRect& rect,
                                     int flags);
    virtual void DrawFrameTitle(wxDC& dc,
                                const wxRect& rect,
                                const wxString& title,
                                int flags);
    virtual void DrawFrameIcon(wxDC& dc,
                               const wxRect& rect,
                               const wxIcon& icon,
                               int flags);
    virtual void DrawFrameButton(wxDC& dc,
                                 wxCoord x, wxCoord y,
                                 int button,
                                 int flags = 0);
    virtual wxRect GetFrameClientArea(const wxRect& rect, int flags) const;
    virtual wxSize GetFrameTotalSize(const wxSize& clientSize, int flags) const;
    virtual wxSize GetFrameMinSize(int flags) const;
    virtual wxSize GetFrameIconSize() const;
    virtual int HitTestFrame(const wxRect& rect, const wxPoint& pt, int flags) const;

    virtual void GetComboBitmaps(wxBitmap *bmpNormal,
                                 wxBitmap *bmpFocus,
                                 wxBitmap *bmpPressed,
                                 wxBitmap *bmpDisabled);

    virtual void AdjustSize(wxSize *size, const wxWindow *window);
    virtual wxRect GetBorderDimensions(wxBorder border) const;
    virtual bool AreScrollbarsInsideBorder() const;

    virtual wxSize GetScrollbarArrowSize() const
        { return m_sizeScrollbarArrow; }
    virtual wxRect GetScrollbarRect(const wxScrollBar *scrollbar,
                                    wxScrollBar::Element elem,
                                    int thumbPos = -1) const;
    virtual wxCoord GetScrollbarSize(const wxScrollBar *scrollbar);
    virtual wxHitTest HitTestScrollbar(const wxScrollBar *scrollbar,
                                       const wxPoint& pt) const;
    virtual wxCoord ScrollbarToPixel(const wxScrollBar *scrollbar,
                                     int thumbPos = -1);
    virtual int PixelToScrollbar(const wxScrollBar *scrollbar, wxCoord coord);
    virtual wxCoord GetListboxItemHeight(wxCoord fontHeight)
        { return fontHeight + 2; }
    virtual wxSize GetCheckBitmapSize() const
        { return wxSize(13, 13); }
    virtual wxSize GetRadioBitmapSize() const
        { return wxSize(12, 12); }
    virtual wxCoord GetCheckItemMargin() const
        { return 0; }

    virtual wxSize GetToolBarButtonSize(wxCoord *separator) const
        { if ( separator ) *separator = 5; return wxSize(16, 15); }
    virtual wxSize GetToolBarMargin() const
        { return wxSize(4, 4); }

    virtual wxRect GetTextTotalArea(const wxTextCtrl *text,
                                    const wxRect& rect) const;
    virtual wxRect GetTextClientArea(const wxTextCtrl *text,
                                     const wxRect& rect,
                                     wxCoord *extraSpaceBeyond) const;

    virtual wxSize GetTabIndent() const { return wxSize(2, 2); }
    virtual wxSize GetTabPadding() const { return wxSize(6, 5); }

    virtual wxCoord GetSliderDim() const { return 20; }
    virtual wxCoord GetSliderTickLen() const { return 4; }
    virtual wxRect GetSliderShaftRect(const wxRect& rect,
                                      wxOrientation orient) const;
    virtual wxSize GetSliderThumbSize(const wxRect& rect,
                                      wxOrientation orient) const;
    virtual wxSize GetProgressBarStep() const { return wxSize(16, 32); }

    virtual wxSize GetMenuBarItemSize(const wxSize& sizeText) const;
    virtual wxMenuGeometryInfo *GetMenuGeometry(wxWindow *win,
                                                const wxMenu& menu) const;

    virtual wxSize GetStatusBarBorders(wxCoord *borderBetweenFields) const;

protected:
    // helper of DrawLabel() and DrawCheckOrRadioButton()
    void DoDrawLabel(wxDC& dc,
                     const wxString& label,
                     const wxRect& rect,
                     int flags = 0,
                     int alignment = wxALIGN_LEFT | wxALIGN_TOP,
                     int indexAccel = -1,
                     wxRect *rectBounds = NULL,
                     const wxPoint& focusOffset
                        = wxPoint(FOCUS_RECT_OFFSET_X, FOCUS_RECT_OFFSET_Y));

    // common part of DrawLabel() and DrawItem()
    void DrawFocusRect(wxDC& dc, const wxRect& rect);

    // DrawLabel() and DrawButtonLabel() helper
    void DrawLabelShadow(wxDC& dc,
                         const wxString& label,
                         const wxRect& rect,
                         int alignment,
                         int indexAccel);

    // DrawButtonBorder() helper
    void DoDrawBackground(wxDC& dc,
                          const wxColour& col,
                          const wxRect& rect,
                          wxWindow *window = NULL );

    // DrawBorder() helpers: all of them shift and clip the DC after drawing
    // the border

    // just draw a rectangle with the given pen
    void DrawRect(wxDC& dc, wxRect *rect, const wxPen& pen);

    // draw the lower left part of rectangle
    void DrawHalfRect(wxDC& dc, wxRect *rect, const wxPen& pen);

    // draw the rectange using the first brush for the left and top sides and
    // the second one for the bottom and right ones
    void DrawShadedRect(wxDC& dc, wxRect *rect,
                        const wxPen& pen1, const wxPen& pen2);

    // draw the normal 3D border
    void DrawRaisedBorder(wxDC& dc, wxRect *rect);

    // draw the sunken 3D border
    void DrawSunkenBorder(wxDC& dc, wxRect *rect);

    // draw the border used for scrollbar arrows
    void DrawArrowBorder(wxDC& dc, wxRect *rect, bool isPressed = FALSE);

    // public DrawArrow()s helper
    void DrawArrow(wxDC& dc, const wxRect& rect,
                   wxArrowDirection arrowDir, wxArrowStyle arrowStyle);

    // DrawArrowButton is used by DrawScrollbar and DrawComboButton
    void DrawArrowButton(wxDC& dc, const wxRect& rect,
                         wxArrowDirection arrowDir,
                         wxArrowStyle arrowStyle);

    // DrawCheckButton/DrawRadioButton helper
    void DrawCheckOrRadioButton(wxDC& dc,
                                const wxString& label,
                                const wxBitmap& bitmap,
                                const wxRect& rect,
                                int flags,
                                wxAlignment align,
                                int indexAccel,
                                wxCoord focusOffsetY);

    // draw a normal or transposed line (useful for using the same code fo both
    // horizontal and vertical widgets)
    void DrawLine(wxDC& dc,
                  wxCoord x1, wxCoord y1,
                  wxCoord x2, wxCoord y2,
                  bool transpose = FALSE)
    {
        if ( transpose )
            dc.DrawLine(y1, x1, y2, x2);
        else
            dc.DrawLine(x1, y1, x2, y2);
    }

    // get the standard check/radio button bitmap
    wxBitmap GetIndicator(IndicatorType indType, int flags);
    wxBitmap GetCheckBitmap(int flags)
        { return GetIndicator(IndicatorType_Check, flags); }
    wxBitmap GetRadioBitmap(int flags)
        { return GetIndicator(IndicatorType_Radio, flags); }

private:
    const wxColourScheme *m_scheme;

    // the sizing parameters (TODO make them changeable)
    wxSize m_sizeScrollbarArrow;

    // GDI objects we use for drawing
    wxColour m_colDarkGrey,
             m_colHighlight;

    wxPen m_penBlack,
          m_penDarkGrey,
          m_penLightGrey,
          m_penHighlight;

    wxFont m_titlebarFont;

    // the checked and unchecked bitmaps for DrawCheckItem()
    wxBitmap m_bmpCheckBitmaps[IndicatorStatus_Max];

    // the bitmaps returned by GetIndicator()
    wxBitmap m_bmpIndicators[IndicatorType_Max]
                            [IndicatorState_Max]
                            [IndicatorStatus_Max];

    // titlebar icons:
    wxBitmap m_bmpFrameButtons[FrameButton_Max];

    // first row is for the normal state, second - for the disabled
    wxBitmap m_bmpArrows[Arrow_StateMax][Arrow_Max];
};

// ----------------------------------------------------------------------------
// wxWin32InputHandler and derived classes: process the keyboard and mouse
// messages according to Windows standards
// ----------------------------------------------------------------------------

class wxWin32InputHandler : public wxInputHandler
{
public:
    wxWin32InputHandler(wxWin32Renderer *renderer);

    virtual bool HandleKey(wxInputConsumer *control,
                           const wxKeyEvent& event,
                           bool pressed);
    virtual bool HandleMouse(wxInputConsumer *control,
                             const wxMouseEvent& event);

protected:
    wxWin32Renderer *m_renderer;
};

class wxWin32ScrollBarInputHandler : public wxStdScrollBarInputHandler
{
public:
    wxWin32ScrollBarInputHandler(wxWin32Renderer *renderer,
                                 wxInputHandler *handler);

    virtual bool HandleMouse(wxInputConsumer *control, const wxMouseEvent& event);
    virtual bool HandleMouseMove(wxInputConsumer *control, const wxMouseEvent& event);

    virtual bool OnScrollTimer(wxScrollBar *scrollbar,
                               const wxControlAction& action);

protected:
    virtual bool IsAllowedButton(int button) { return button == 1; }

    virtual void Highlight(wxScrollBar *scrollbar, bool doIt)
    {
        // we don't highlight anything
    }

    // the first and last event which caused the thumb to move
    wxMouseEvent m_eventStartDrag,
                 m_eventLastDrag;

    // have we paused the scrolling because the mouse moved?
    bool m_scrollPaused;

    // we remember the interval of the timer to be able to restart it
    int m_interval;
};

class wxWin32CheckboxInputHandler : public wxStdCheckboxInputHandler
{
public:
    wxWin32CheckboxInputHandler(wxInputHandler *handler)
        : wxStdCheckboxInputHandler(handler) { }

    virtual bool HandleKey(wxInputConsumer *control,
                           const wxKeyEvent& event,
                           bool pressed);
};

class wxWin32TextCtrlInputHandler : public wxStdTextCtrlInputHandler
{
public:
    wxWin32TextCtrlInputHandler(wxInputHandler *handler)
        : wxStdTextCtrlInputHandler(handler) { }

    virtual bool HandleKey(wxInputConsumer *control,
                           const wxKeyEvent& event,
                           bool pressed);
};

class wxWin32StatusBarInputHandler : public wxStdInputHandler
{
public:
    wxWin32StatusBarInputHandler(wxInputHandler *handler);

    virtual bool HandleMouse(wxInputConsumer *consumer,
                             const wxMouseEvent& event);

    virtual bool HandleMouseMove(wxInputConsumer *consumer,
                                 const wxMouseEvent& event);

protected:
    // is the given point over the statusbar grip?
    bool IsOnGrip(wxWindow *statbar, const wxPoint& pt) const;

private:
    // the cursor we had replaced with the resize one
    wxCursor m_cursorOld;

    // was the mouse over the grip last time we checked?
    bool m_isOnGrip;
};

class wxWin32SystemMenuEvtHandler;

class wxWin32FrameInputHandler : public wxStdFrameInputHandler
{
public:
    wxWin32FrameInputHandler(wxInputHandler *handler);
    ~wxWin32FrameInputHandler();

    virtual bool HandleMouse(wxInputConsumer *control,
                             const wxMouseEvent& event);

    virtual bool HandleActivation(wxInputConsumer *consumer, bool activated);
                             
    void PopupSystemMenu(wxTopLevelWindow *window, const wxPoint& pos) const;

private:
    // was the mouse over the grip last time we checked?
    wxWin32SystemMenuEvtHandler *m_menuHandler;
};

// ----------------------------------------------------------------------------
// wxWin32ColourScheme: uses (default) Win32 colours
// ----------------------------------------------------------------------------

class wxWin32ColourScheme : public wxColourScheme
{
public:
    virtual wxColour Get(StdColour col) const;
    virtual wxColour GetBackground(wxWindow *win) const;
};

// ----------------------------------------------------------------------------
// wxWin32ArtProvider
// ----------------------------------------------------------------------------

class wxWin32ArtProvider : public wxArtProvider
{
protected:
    virtual wxBitmap CreateBitmap(const wxArtID& id,
                                  const wxArtClient& client,
                                  const wxSize& size);
};

// ----------------------------------------------------------------------------
// wxWin32Theme
// ----------------------------------------------------------------------------

WX_DEFINE_ARRAY(wxInputHandler *, wxArrayHandlers);

class wxWin32Theme : public wxTheme
{
public:
    wxWin32Theme();
    virtual ~wxWin32Theme();

    virtual wxRenderer *GetRenderer();
    virtual wxArtProvider *GetArtProvider();
    virtual wxInputHandler *GetInputHandler(const wxString& control);
    virtual wxColourScheme *GetColourScheme();

private:
    // get the default input handler
    wxInputHandler *GetDefaultInputHandler();

    wxWin32Renderer *m_renderer;
    
    wxWin32ArtProvider *m_artProvider;

    // the names of the already created handlers and the handlers themselves
    // (these arrays are synchronized)
    wxSortedArrayString m_handlerNames;
    wxArrayHandlers m_handlers;

    wxWin32InputHandler *m_handlerDefault;

    wxWin32ColourScheme *m_scheme;

    WX_DECLARE_THEME(win32)
};

// ----------------------------------------------------------------------------
// standard bitmaps
// ----------------------------------------------------------------------------

// frame buttons bitmaps

static const char *frame_button_close_xpm[] = {
"12 10 2 1",
"         c None",
".        c black",
"            ",
"  ..    ..  ",
"   ..  ..   ",
"    ....    ",
"     ..     ",
"    ....    ",
"   ..  ..   ",
"  ..    ..  ",
"            ",
"            "};

static const char *frame_button_help_xpm[] = {
"12 10 2 1",
"         c None",
".        c #000000",
"    ....    ",
"   ..  ..   ",
"   ..  ..   ",
"      ..    ",
"     ..     ",
"     ..     ",
"            ",
"     ..     ",
"     ..     ",
"            "};

static const char *frame_button_maximize_xpm[] = {
"12 10 2 1",
"         c None",
".        c #000000",
" .........  ",
" .........  ",
" .       .  ",
" .       .  ",
" .       .  ",
" .       .  ",
" .       .  ",
" .       .  ",
" .........  ",
"            "};

static const char *frame_button_minimize_xpm[] = {
"12 10 2 1",
"         c None",
".        c #000000",
"            ",
"            ",
"            ",
"            ",
"            ",
"            ",
"            ",
"  ......    ",
"  ......    ",
"            "};

static const char *frame_button_restore_xpm[] = {
"12 10 2 1",
"         c None",
".        c #000000",
"   ......   ",
"   ......   ",
"   .    .   ",
" ...... .   ",
" ...... .   ",
" .    ...   ",
" .    .     ",
" .    .     ",
" ......     ",
"            "};

// menu bitmaps

static const char *checked_menu_xpm[] = {
/* columns rows colors chars-per-pixel */
"9 9 2 1",
"w c None",
"b c black",
/* pixels */
"wwwwwwwww",
"wwwwwwwbw",
"wwwwwwbbw",
"wbwwwbbbw",
"wbbwbbbww",
"wbbbbbwww",
"wwbbbwwww",
"wwwbwwwww",
"wwwwwwwww"
};

static const char *selected_checked_menu_xpm[] = {
/* columns rows colors chars-per-pixel */
"9 9 2 1",
"w c None",
"b c white",
/* pixels */
"wwwwwwwww",
"wwwwwwwbw",
"wwwwwwbbw",
"wbwwwbbbw",
"wbbwbbbww",
"wbbbbbwww",
"wwbbbwwww",
"wwwbwwwww",
"wwwwwwwww"
};

static const char *disabled_checked_menu_xpm[] = {
/* columns rows colors chars-per-pixel */
"9 9 3 1",
"w c None",
"b c #7f7f7f",
"W c #e0e0e0",
/* pixels */
"wwwwwwwww",
"wwwwwwwbw",
"wwwwwwbbW",
"wbwwwbbbW",
"wbbwbbbWW",
"wbbbbbWWw",
"wwbbbWWww",
"wwwbWWwww",
"wwwwWwwww"
};

static const char *selected_disabled_checked_menu_xpm[] = {
/* columns rows colors chars-per-pixel */
"9 9 2 1",
"w c None",
"b c #7f7f7f",
/* pixels */
"wwwwwwwww",
"wwwwwwwbw",
"wwwwwwbbw",
"wbwwwbbbw",
"wbbwbbbww",
"wbbbbbwww",
"wwbbbwwww",
"wwwbwwwww",
"wwwwwwwww"
};

// checkbox and radiobox bitmaps below

static const char *checked_xpm[] = {
/* columns rows colors chars-per-pixel */
"13 13 5 1",
"w c white",
"b c black",
"d c #7f7f7f",
"g c #c0c0c0",
"h c #e0e0e0",
/* pixels */
"ddddddddddddh",
"dbbbbbbbbbbgh",
"dbwwwwwwwwwgh",
"dbwwwwwwwbwgh",
"dbwwwwwwbbwgh",
"dbwbwwwbbbwgh",
"dbwbbwbbbwwgh",
"dbwbbbbbwwwgh",
"dbwwbbbwwwwgh",
"dbwwwbwwwwwgh",
"dbwwwwwwwwwgh",
"dgggggggggggh",
"hhhhhhhhhhhhh"
};

static const char *pressed_checked_xpm[] = {
/* columns rows colors chars-per-pixel */
"13 13 4 1",
"b c black",
"d c #7f7f7f",
"g c #c0c0c0",
"h c #e0e0e0",
/* pixels */
"ddddddddddddh",
"dbbbbbbbbbbgh",
"dbggggggggggh",
"dbgggggggbggh",
"dbggggggbbggh",
"dbgbgggbbbggh",
"dbgbbgbbbgggh",
"dbgbbbbbggggh",
"dbggbbbgggggh",
"dbgggbggggggh",
"dbggggggggggh",
"dgggggggggggh",
"hhhhhhhhhhhhh"
};

static const char *pressed_disabled_checked_xpm[] = {
/* columns rows colors chars-per-pixel */
"13 13 4 1",
"b c black",
"d c #7f7f7f",
"g c #c0c0c0",
"h c #e0e0e0",
/* pixels */
"ddddddddddddh",
"dbbbbbbbbbbgh",
"dbggggggggggh",
"dbgggggggdggh",
"dbggggggddggh",
"dbgdgggdddggh",
"dbgddgdddgggh",
"dbgdddddggggh",
"dbggdddgggggh",
"dbgggdggggggh",
"dbggggggggggh",
"dgggggggggggh",
"hhhhhhhhhhhhh"
};

static const char *checked_item_xpm[] = {
/* columns rows colors chars-per-pixel */
"13 13 3 1",
"w c white",
"b c black",
"d c #808080",
/* pixels */
"wwwwwwwwwwwww",
"wdddddddddddw",
"wdwwwwwwwwwdw",
"wdwwwwwwwbwdw",
"wdwwwwwwbbwdw",
"wdwbwwwbbbwdw",
"wdwbbwbbbwwdw",
"wdwbbbbbwwwdw",
"wdwwbbbwwwwdw",
"wdwwwbwwwwwdw",
"wdwwwwwwwwwdw",
"wdddddddddddw",
"wwwwwwwwwwwww"
};

static const char *unchecked_xpm[] = {
/* columns rows colors chars-per-pixel */
"13 13 5 1",
"w c white",
"b c black",
"d c #7f7f7f",
"g c #c0c0c0",
"h c #e0e0e0",
/* pixels */
"ddddddddddddh",
"dbbbbbbbbbbgh",
"dbwwwwwwwwwgh",
"dbwwwwwwwwwgh",
"dbwwwwwwwwwgh",
"dbwwwwwwwwwgh",
"dbwwwwwwwwwgh",
"dbwwwwwwwwwgh",
"dbwwwwwwwwwgh",
"dbwwwwwwwwwgh",
"dbwwwwwwwwwgh",
"dgggggggggggh",
"hhhhhhhhhhhhh"
};

static const char *pressed_unchecked_xpm[] = {
/* columns rows colors chars-per-pixel */
"13 13 4 1",
"b c black",
"d c #7f7f7f",
"g c #c0c0c0",
"h c #e0e0e0",
/* pixels */
"ddddddddddddh",
"dbbbbbbbbbbgh",
"dbggggggggggh",
"dbggggggggggh",
"dbggggggggggh",
"dbggggggggggh",
"dbggggggggggh",
"dbggggggggggh",
"dbggggggggggh",
"dbggggggggggh",
"dbggggggggggh",
"dbggggggggggh",
"hhhhhhhhhhhhh"
};

static const char *unchecked_item_xpm[] = {
/* columns rows colors chars-per-pixel */
"13 13 2 1",
"w c white",
"d c #808080",
/* pixels */
"wwwwwwwwwwwww",
"wdddddddddddw",
"wdwwwwwwwwwdw",
"wdwwwwwwwwwdw",
"wdwwwwwwwwwdw",
"wdwwwwwwwwwdw",
"wdwwwwwwwwwdw",
"wdwwwwwwwwwdw",
"wdwwwwwwwwwdw",
"wdwwwwwwwwwdw",
"wdwwwwwwwwwdw",
"wdddddddddddw",
"wwwwwwwwwwwww"
};

static const char *checked_radio_xpm[] = {
/* columns rows colors chars-per-pixel */
"12 12 6 1",
"  c None",
"w c white",
"b c black",
"d c #7f7f7f",
"g c #c0c0c0",
"h c #e0e0e0",
/* pixels */
"    dddd    ",
"  ddbbbbdd  ",
" dbbwwwwbbh ",
" dbwwwwwwgh ",
"dbwwwbbwwwgh",
"dbwwbbbbwwgh",
"dbwwbbbbwwgh",
"dbwwwbbwwwgh",
" dbwwwwwwgh ",
" dggwwwwggh ",
"  hhgggghh  ",
"    hhhh    "
};

static const char *pressed_checked_radio_xpm[] = {
/* columns rows colors chars-per-pixel */
"12 12 6 1",
"  c None",
"w c white",
"b c black",
"d c #7f7f7f",
"g c #c0c0c0",
"h c #e0e0e0",
/* pixels */
"    dddd    ",
"  ddbbbbdd  ",
" dbbggggbbh ",
" dbgggggggh ",
"dbgggbbggggh",
"dbggbbbbgggh",
"dbggbbbbgggh",
"dbgggbbggggh",
" dbgggggggh ",
" dggggggggh ",
"  hhgggghh  ",
"    hhhh    "
};

static const char *pressed_disabled_checked_radio_xpm[] = {
/* columns rows colors chars-per-pixel */
"12 12 6 1",
"  c None",
"w c white",
"b c black",
"d c #7f7f7f",
"g c #c0c0c0",
"h c #e0e0e0",
/* pixels */
"    dddd    ",
"  ddbbbbdd  ",
" dbbggggbbh ",
" dbgggggggh ",
"dbgggddggggh",
"dbggddddgggh",
"dbggddddgggh",
"dbgggddggggh",
" dbgggggggh ",
" dggggggggh ",
"  hhgggghh  ",
"    hhhh    ",
};

static const char *unchecked_radio_xpm[] = {
/* columns rows colors chars-per-pixel */
"12 12 6 1",
"  c None",
"w c white",
"b c black",
"d c #7f7f7f",
"g c #c0c0c0",
"h c #e0e0e0",
/* pixels */
"    dddd    ",
"  ddbbbbdd  ",
" dbbwwwwbbh ",
" dbwwwwwwgh ",
"dbwwwwwwwwgh",
"dbwwwwwwwwgh",
"dbwwwwwwwwgh",
"dbwwwwwwwwgh",
" dbwwwwwwgh ",
" dggwwwwggh ",
"  hhgggghh  ",
"    hhhh    "
};

static const char *pressed_unchecked_radio_xpm[] = {
/* columns rows colors chars-per-pixel */
"12 12 6 1",
"  c None",
"w c white",
"b c black",
"d c #7f7f7f",
"g c #c0c0c0",
"h c #e0e0e0",
/* pixels */
"    dddd    ",
"  ddbbbbdd  ",
" dbbggggbbh ",
" dbgggggggh ",
"dbgggggggggh",
"dbgggggggggh",
"dbgggggggggh",
"dbgggggggggh",
" dbgggggggh ",
" dggggggggh ",
"  hhgggghh  ",
"    hhhh    "
};

static const char **
    xpmIndicators[IndicatorType_Max][IndicatorState_Max][IndicatorStatus_Max] =
{
    // checkboxes first
    {
        // normal state
        { checked_xpm, unchecked_xpm },

        // pressed state
        { pressed_checked_xpm, pressed_unchecked_xpm },

        // disabled state
        { pressed_disabled_checked_xpm, pressed_unchecked_xpm },
    },

    // radio
    {
        // normal state
        { checked_radio_xpm, unchecked_radio_xpm },

        // pressed state
        { pressed_checked_radio_xpm, pressed_unchecked_radio_xpm },

        // disabled state
        { pressed_disabled_checked_radio_xpm, pressed_unchecked_radio_xpm },
    },

    // menu
    {
        // normal state
        { checked_menu_xpm, NULL },

        // selected state
        { selected_checked_menu_xpm, NULL },

        // disabled state
        { disabled_checked_menu_xpm, NULL },

        // disabled selected state
        { selected_disabled_checked_menu_xpm, NULL },
    }
};

static const char **xpmChecked[IndicatorStatus_Max] =
{
    checked_item_xpm,
    unchecked_item_xpm
};

// ============================================================================
// implementation
// ============================================================================

WX_IMPLEMENT_THEME(wxWin32Theme, win32, wxTRANSLATE("Win32 theme"));

// ----------------------------------------------------------------------------
// wxWin32Theme
// ----------------------------------------------------------------------------

wxWin32Theme::wxWin32Theme()
{
    m_scheme = NULL;
    m_renderer = NULL;
    m_handlerDefault = NULL;
    m_artProvider = NULL;
}

wxWin32Theme::~wxWin32Theme()
{
    size_t count = m_handlers.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        if ( m_handlers[n] != m_handlerDefault )
            delete m_handlers[n];
    }

    delete m_handlerDefault;

    delete m_renderer;
    delete m_scheme;
    wxArtProvider::RemoveProvider(m_artProvider);
}

wxRenderer *wxWin32Theme::GetRenderer()
{
    if ( !m_renderer )
    {
        m_renderer = new wxWin32Renderer(GetColourScheme());
    }

    return m_renderer;
}

wxArtProvider *wxWin32Theme::GetArtProvider()
{
    if ( !m_artProvider )
    {
        m_artProvider = new wxWin32ArtProvider;
    }

    return m_artProvider;
}

wxInputHandler *wxWin32Theme::GetDefaultInputHandler()
{
    if ( !m_handlerDefault )
    {
        m_handlerDefault = new wxWin32InputHandler(m_renderer);
    }

    return m_handlerDefault;
}

wxInputHandler *wxWin32Theme::GetInputHandler(const wxString& control)
{
    wxInputHandler *handler;
    int n = m_handlerNames.Index(control);
    if ( n == wxNOT_FOUND )
    {
        // create a new handler
        if ( control == wxINP_HANDLER_SCROLLBAR )
            handler = new wxWin32ScrollBarInputHandler(m_renderer,
                                                       GetDefaultInputHandler());
#if wxUSE_BUTTON
        else if ( control == wxINP_HANDLER_BUTTON )
            handler = new wxStdButtonInputHandler(GetDefaultInputHandler());
#endif // wxUSE_BUTTON
#if wxUSE_CHECKBOX
        else if ( control == wxINP_HANDLER_CHECKBOX )
            handler = new wxWin32CheckboxInputHandler(GetDefaultInputHandler());
#endif // wxUSE_CHECKBOX
#if wxUSE_COMBOBOX
        else if ( control == wxINP_HANDLER_COMBOBOX )
            handler = new wxStdComboBoxInputHandler(GetDefaultInputHandler());
#endif // wxUSE_COMBOBOX
#if wxUSE_LISTBOX
        else if ( control == wxINP_HANDLER_LISTBOX )
            handler = new wxStdListboxInputHandler(GetDefaultInputHandler());
#endif // wxUSE_LISTBOX
#if wxUSE_CHECKLISTBOX
        else if ( control == wxINP_HANDLER_CHECKLISTBOX )
            handler = new wxStdCheckListboxInputHandler(GetDefaultInputHandler());
#endif // wxUSE_CHECKLISTBOX
#if wxUSE_TEXTCTRL
        else if ( control == wxINP_HANDLER_TEXTCTRL )
            handler = new wxWin32TextCtrlInputHandler(GetDefaultInputHandler());
#endif // wxUSE_TEXTCTRL
#if wxUSE_SLIDER
        else if ( control == wxINP_HANDLER_SLIDER )
            handler = new wxStdSliderButtonInputHandler(GetDefaultInputHandler());
#endif // wxUSE_SLIDER
#if wxUSE_SPINBTN
        else if ( control == wxINP_HANDLER_SPINBTN )
            handler = new wxStdSpinButtonInputHandler(GetDefaultInputHandler());
#endif // wxUSE_SPINBTN
#if wxUSE_NOTEBOOK
        else if ( control == wxINP_HANDLER_NOTEBOOK )
            handler = new wxStdNotebookInputHandler(GetDefaultInputHandler());
#endif // wxUSE_NOTEBOOK
#if wxUSE_STATUSBAR
        else if ( control == wxINP_HANDLER_STATUSBAR )
            handler = new wxWin32StatusBarInputHandler(GetDefaultInputHandler());
#endif // wxUSE_STATUSBAR
#if wxUSE_TOOLBAR
        else if ( control == wxINP_HANDLER_TOOLBAR )
            handler = new wxStdToolbarInputHandler(GetDefaultInputHandler());
#endif // wxUSE_TOOLBAR
        else if ( control == wxINP_HANDLER_TOPLEVEL )
            handler = new wxWin32FrameInputHandler(GetDefaultInputHandler());
        else
            handler = GetDefaultInputHandler();

        n = m_handlerNames.Add(control);
        m_handlers.Insert(handler, n);
    }
    else // we already have it
    {
        handler = m_handlers[n];
    }

    return handler;
}

wxColourScheme *wxWin32Theme::GetColourScheme()
{
    if ( !m_scheme )
    {
        m_scheme = new wxWin32ColourScheme;
    }
    return m_scheme;
}

// ============================================================================
// wxWin32ColourScheme
// ============================================================================

wxColour wxWin32ColourScheme::GetBackground(wxWindow *win) const
{
    wxColour col;
    if ( win->UseBgCol() )
    {
        // use the user specified colour
        col = win->GetBackgroundColour();
    }

    if ( win->IsContainerWindow() )
    {
        wxTextCtrl *text = wxDynamicCast(win, wxTextCtrl);
        if ( text )
        {
            if ( !text->IsEnabled() ) // not IsEditable()
                col = Get(CONTROL);
            //else: execute code below
        }

        if ( !col.Ok() )
        {
            // doesn't depend on the state
            col = Get(WINDOW);
        }
    }
    else
    {
        int flags = win->GetStateFlags();

        // the colour set by the user should be used for the normal state
        // and for the states for which we don't have any specific colours
        if ( !col.Ok() || (flags & wxCONTROL_PRESSED) != 0 )
        {
            if ( wxDynamicCast(win, wxScrollBar) )
                col = Get(flags & wxCONTROL_PRESSED ? SCROLLBAR_PRESSED
                                                    : SCROLLBAR);
            else
                col = Get(CONTROL);
        }
    }

    return col;
}

wxColour wxWin32ColourScheme::Get(wxWin32ColourScheme::StdColour col) const
{
    switch ( col )
    {
        // use the system colours under Windows
#if defined(__WXMSW__)
        case WINDOW:            return wxColour(GetSysColor(COLOR_WINDOW));

        case CONTROL_PRESSED:
        case CONTROL_CURRENT:
        case CONTROL:           return wxColour(GetSysColor(COLOR_BTNFACE));

        case CONTROL_TEXT:      return wxColour(GetSysColor(COLOR_BTNTEXT));

#if defined(COLOR_3DLIGHT)
        case SCROLLBAR:         return wxColour(GetSysColor(COLOR_3DLIGHT));
#else
        case SCROLLBAR:         return wxColour(0xe0e0e0);
#endif
        case SCROLLBAR_PRESSED: return wxColour(GetSysColor(COLOR_BTNTEXT));

        case HIGHLIGHT:         return wxColour(GetSysColor(COLOR_HIGHLIGHT));
        case HIGHLIGHT_TEXT:    return wxColour(GetSysColor(COLOR_HIGHLIGHTTEXT));

#if defined(COLOR_3DDKSHADOW)
        case SHADOW_DARK:       return wxColour(GetSysColor(COLOR_3DDKSHADOW));
#else
        case SHADOW_DARK:       return wxColour(GetSysColor(COLOR_3DHADOW));
#endif

        case CONTROL_TEXT_DISABLED:
        case SHADOW_HIGHLIGHT:  return wxColour(GetSysColor(COLOR_BTNHIGHLIGHT));

        case SHADOW_IN:         return wxColour(GetSysColor(COLOR_BTNFACE));

        case CONTROL_TEXT_DISABLED_SHADOW:
        case SHADOW_OUT:        return wxColour(GetSysColor(COLOR_BTNSHADOW));

        case TITLEBAR:          return wxColour(GetSysColor(COLOR_INACTIVECAPTION));
        case TITLEBAR_ACTIVE:   return wxColour(GetSysColor(COLOR_ACTIVECAPTION));
        case TITLEBAR_TEXT:     return wxColour(GetSysColor(COLOR_INACTIVECAPTIONTEXT));
        case TITLEBAR_ACTIVE_TEXT: return wxColour(GetSysColor(COLOR_CAPTIONTEXT));

        case DESKTOP:           return wxColour(0x808000);
#else // !__WXMSW__
        // use the standard Windows colours elsewhere
        case WINDOW:            return *wxWHITE;

        case CONTROL_PRESSED:
        case CONTROL_CURRENT:
        case CONTROL:           return wxColour(0xc0c0c0);

        case CONTROL_TEXT:      return *wxBLACK;

        case SCROLLBAR:         return wxColour(0xe0e0e0);
        case SCROLLBAR_PRESSED: return *wxBLACK;

        case HIGHLIGHT:         return wxColour(0x800000);
        case HIGHLIGHT_TEXT:    return wxColour(0xffffff);

        case SHADOW_DARK:       return *wxBLACK;

        case CONTROL_TEXT_DISABLED:return wxColour(0xe0e0e0);
        case SHADOW_HIGHLIGHT:  return wxColour(0xffffff);

        case SHADOW_IN:         return wxColour(0xc0c0c0);

        case CONTROL_TEXT_DISABLED_SHADOW:
        case SHADOW_OUT:        return wxColour(0x7f7f7f);

        case TITLEBAR:          return wxColour(0xaeaaae);
        case TITLEBAR_ACTIVE:   return wxColour(0x820300);
        case TITLEBAR_TEXT:     return wxColour(0xc0c0c0);
        case TITLEBAR_ACTIVE_TEXT:return *wxWHITE;

        case DESKTOP:           return wxColour(0x808000);
#endif // __WXMSW__

        case GAUGE:             return Get(HIGHLIGHT);

        case MAX:
        default:
            wxFAIL_MSG(_T("invalid standard colour"));
            return *wxBLACK;
    }
}

// ============================================================================
// wxWin32Renderer
// ============================================================================

// ----------------------------------------------------------------------------
// construction
// ----------------------------------------------------------------------------

wxWin32Renderer::wxWin32Renderer(const wxColourScheme *scheme)
{
    // init data
    m_scheme = scheme;
    m_sizeScrollbarArrow = wxSize(16, 16);

    // init colours and pens
    m_penBlack = wxPen(wxSCHEME_COLOUR(scheme, SHADOW_DARK), 0, wxSOLID);

    m_colDarkGrey = wxSCHEME_COLOUR(scheme, SHADOW_OUT);
    m_penDarkGrey = wxPen(m_colDarkGrey, 0, wxSOLID);

    m_penLightGrey = wxPen(wxSCHEME_COLOUR(scheme, SHADOW_IN), 0, wxSOLID);

    m_colHighlight = wxSCHEME_COLOUR(scheme, SHADOW_HIGHLIGHT);
    m_penHighlight = wxPen(m_colHighlight, 0, wxSOLID);

    m_titlebarFont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    m_titlebarFont.SetWeight(wxFONTWEIGHT_BOLD);

    // init the arrow bitmaps
    static const size_t ARROW_WIDTH = 7;
    static const size_t ARROW_LENGTH = 4;

    wxMask *mask;
    wxMemoryDC dcNormal,
               dcDisabled,
               dcInverse;
    for ( size_t n = 0; n < Arrow_Max; n++ )
    {
        bool isVertical = n > Arrow_Right;
        int w, h;
        if ( isVertical )
        {
            w = ARROW_WIDTH;
            h = ARROW_LENGTH;
        }
        else
        {
            h = ARROW_WIDTH;
            w = ARROW_LENGTH;
        }

        // disabled arrow is larger because of the shadow
        m_bmpArrows[Arrow_Normal][n].Create(w, h);
        m_bmpArrows[Arrow_Disabled][n].Create(w + 1, h + 1);

        dcNormal.SelectObject(m_bmpArrows[Arrow_Normal][n]);
        dcDisabled.SelectObject(m_bmpArrows[Arrow_Disabled][n]);

        dcNormal.SetBackground(*wxWHITE_BRUSH);
        dcDisabled.SetBackground(*wxWHITE_BRUSH);
        dcNormal.Clear();
        dcDisabled.Clear();

        dcNormal.SetPen(m_penBlack);
        dcDisabled.SetPen(m_penDarkGrey);

        // calculate the position of the point of the arrow
        wxCoord x1, y1;
        if ( isVertical )
        {
            x1 = (ARROW_WIDTH - 1)/2;
            y1 = n == Arrow_Up ? 0 : ARROW_LENGTH - 1;
        }
        else // horizontal
        {
            x1 = n == Arrow_Left ? 0 : ARROW_LENGTH - 1;
            y1 = (ARROW_WIDTH - 1)/2;
        }

        wxCoord x2 = x1,
                y2 = y1;

        if ( isVertical )
            x2++;
        else
            y2++;

        for ( size_t i = 0; i < ARROW_LENGTH; i++ )
        {
            dcNormal.DrawLine(x1, y1, x2, y2);
            dcDisabled.DrawLine(x1, y1, x2, y2);

            if ( isVertical )
            {
                x1--;
                x2++;

                if ( n == Arrow_Up )
                {
                    y1++;
                    y2++;
                }
                else // down arrow
                {
                    y1--;
                    y2--;
                }
            }
            else // left or right arrow
            {
                y1--;
                y2++;

                if ( n == Arrow_Left )
                {
                    x1++;
                    x2++;
                }
                else
                {
                    x1--;
                    x2--;
                }
            }
        }

        // draw the shadow for the disabled one
        dcDisabled.SetPen(m_penHighlight);
        switch ( n )
        {
            case Arrow_Left:
                y1 += 2;
                dcDisabled.DrawLine(x1, y1, x2, y2);
                break;

            case Arrow_Right:
                x1 = ARROW_LENGTH - 1;
                y1 = (ARROW_WIDTH - 1)/2 + 1;
                x2 = 0;
                y2 = ARROW_WIDTH;
                dcDisabled.DrawLine(x1, y1, x2, y2);
                dcDisabled.DrawLine(++x1, y1, x2, ++y2);
                break;

            case Arrow_Up:
                x1 += 2;
                dcDisabled.DrawLine(x1, y1, x2, y2);
                break;

            case Arrow_Down:
                x1 = ARROW_WIDTH - 1;
                y1 = 1;
                x2 = (ARROW_WIDTH - 1)/2;
                y2 = ARROW_LENGTH;
                dcDisabled.DrawLine(x1, y1, x2, y2);
                dcDisabled.DrawLine(++x1, y1, x2, ++y2);
                break;

        }

        // create the inversed bitmap but only for the right arrow as we only
        // use it for the menus
        if ( n == Arrow_Right )
        {
            m_bmpArrows[Arrow_Inversed][n].Create(w, h);
            dcInverse.SelectObject(m_bmpArrows[Arrow_Inversed][n]);
            dcInverse.Clear();
            dcInverse.Blit(0, 0, w, h,
                          &dcNormal, 0, 0,
                          wxXOR);
            dcInverse.SelectObject(wxNullBitmap);

            mask = new wxMask(m_bmpArrows[Arrow_Inversed][n], *wxBLACK);
            m_bmpArrows[Arrow_Inversed][n].SetMask(mask);

            m_bmpArrows[Arrow_InversedDisabled][n].Create(w, h);
            dcInverse.SelectObject(m_bmpArrows[Arrow_InversedDisabled][n]);
            dcInverse.Clear();
            dcInverse.Blit(0, 0, w, h,
                          &dcDisabled, 0, 0,
                          wxXOR);
            dcInverse.SelectObject(wxNullBitmap);

            mask = new wxMask(m_bmpArrows[Arrow_InversedDisabled][n], *wxBLACK);
            m_bmpArrows[Arrow_InversedDisabled][n].SetMask(mask);
        }

        dcNormal.SelectObject(wxNullBitmap);
        dcDisabled.SelectObject(wxNullBitmap);

        mask = new wxMask(m_bmpArrows[Arrow_Normal][n], *wxWHITE);
        m_bmpArrows[Arrow_Normal][n].SetMask(mask);
        mask = new wxMask(m_bmpArrows[Arrow_Disabled][n], *wxWHITE);
        m_bmpArrows[Arrow_Disabled][n].SetMask(mask);

        m_bmpArrows[Arrow_Pressed][n] = m_bmpArrows[Arrow_Normal][n];
    }

    // init the frame buttons bitmaps
    m_bmpFrameButtons[FrameButton_Close] = wxBitmap(frame_button_close_xpm);
    m_bmpFrameButtons[FrameButton_Minimize] = wxBitmap(frame_button_minimize_xpm);
    m_bmpFrameButtons[FrameButton_Maximize] = wxBitmap(frame_button_maximize_xpm);
    m_bmpFrameButtons[FrameButton_Restore] = wxBitmap(frame_button_restore_xpm);
    m_bmpFrameButtons[FrameButton_Help] = wxBitmap(frame_button_help_xpm);
}

// ----------------------------------------------------------------------------
// border stuff
// ----------------------------------------------------------------------------

/*
   The raised border in Win32 looks like this:

   IIIIIIIIIIIIIIIIIIIIIIB
   I                    GB
   I                    GB  I = white       (HILIGHT)
   I                    GB  H = light grey  (LIGHT)
   I                    GB  G = dark grey   (SHADOI)
   I                    GB  B = black       (DKSHADOI)
   I                    GB  I = hIghlight (COLOR_3DHILIGHT)
   I                    GB
   IGGGGGGGGGGGGGGGGGGGGGB
   BBBBBBBBBBBBBBBBBBBBBBB

   The sunken border looks like this:

   GGGGGGGGGGGGGGGGGGGGGGI
   GBBBBBBBBBBBBBBBBBBBBHI
   GB                   HI
   GB                   HI
   GB                   HI
   GB                   HI
   GB                   HI
   GB                   HI
   GHHHHHHHHHHHHHHHHHHHHHI
   IIIIIIIIIIIIIIIIIIIIIII

   The static border (used for the controls which don't get focus) is like
   this:

   GGGGGGGGGGGGGGGGGGGGGGW
   G                     W
   G                     W
   G                     W
   G                     W
   G                     W
   G                     W
   G                     W
   WWWWWWWWWWWWWWWWWWWWWWW

   The most complicated is the double border:

   HHHHHHHHHHHHHHHHHHHHHHB
   HWWWWWWWWWWWWWWWWWWWWGB
   HWHHHHHHHHHHHHHHHHHHHGB
   HWH                 HGB
   HWH                 HGB
   HWH                 HGB
   HWH                 HGB
   HWHHHHHHHHHHHHHHHHHHHGB
   HGGGGGGGGGGGGGGGGGGGGGB
   BBBBBBBBBBBBBBBBBBBBBBB

   And the simple border is, well, simple:

   BBBBBBBBBBBBBBBBBBBBBBB
   B                     B
   B                     B
   B                     B
   B                     B
   B                     B
   B                     B
   B                     B
   B                     B
   BBBBBBBBBBBBBBBBBBBBBBB
*/

void wxWin32Renderer::DrawRect(wxDC& dc, wxRect *rect, const wxPen& pen)
{
    // draw
    dc.SetPen(pen);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(*rect);

    // adjust the rect
    rect->Inflate(-1);
}

void wxWin32Renderer::DrawHalfRect(wxDC& dc, wxRect *rect, const wxPen& pen)
{
    // draw the bottom and right sides
    dc.SetPen(pen);
    dc.DrawLine(rect->GetLeft(), rect->GetBottom(),
                rect->GetRight() + 1, rect->GetBottom());
    dc.DrawLine(rect->GetRight(), rect->GetTop(),
                rect->GetRight(), rect->GetBottom());

    // adjust the rect
    rect->width--;
    rect->height--;
}

void wxWin32Renderer::DrawShadedRect(wxDC& dc, wxRect *rect,
                                     const wxPen& pen1, const wxPen& pen2)
{
    // draw the rectangle
    dc.SetPen(pen1);
    dc.DrawLine(rect->GetLeft(), rect->GetTop(),
                rect->GetLeft(), rect->GetBottom());
    dc.DrawLine(rect->GetLeft() + 1, rect->GetTop(),
                rect->GetRight(), rect->GetTop());
    dc.SetPen(pen2);
    dc.DrawLine(rect->GetRight(), rect->GetTop(),
                rect->GetRight(), rect->GetBottom());
    dc.DrawLine(rect->GetLeft(), rect->GetBottom(),
                rect->GetRight() + 1, rect->GetBottom());

    // adjust the rect
    rect->Inflate(-1);
}

void wxWin32Renderer::DrawRaisedBorder(wxDC& dc, wxRect *rect)
{
    DrawShadedRect(dc, rect, m_penHighlight, m_penBlack);
    DrawShadedRect(dc, rect, m_penLightGrey, m_penDarkGrey);
}

void wxWin32Renderer::DrawSunkenBorder(wxDC& dc, wxRect *rect)
{
    DrawShadedRect(dc, rect, m_penDarkGrey, m_penHighlight);
    DrawShadedRect(dc, rect, m_penBlack, m_penLightGrey);
}

void wxWin32Renderer::DrawArrowBorder(wxDC& dc, wxRect *rect, bool isPressed)
{
    if ( isPressed )
    {
        DrawRect(dc, rect, m_penDarkGrey);

        // the arrow is usually drawn inside border of width 2 and is offset by
        // another pixel in both directions when it's pressed - as the border
        // in this case is more narrow as well, we have to adjust rect like
        // this:
        rect->Inflate(-1);
        rect->x++;
        rect->y++;
    }
    else
    {
        DrawShadedRect(dc, rect, m_penLightGrey, m_penBlack);
        DrawShadedRect(dc, rect, m_penHighlight, m_penDarkGrey);
    }
}

void wxWin32Renderer::DrawBorder(wxDC& dc,
                                 wxBorder border,
                                 const wxRect& rectTotal,
                                 int WXUNUSED(flags),
                                 wxRect *rectIn)
{
    int i;

    wxRect rect = rectTotal;

    switch ( border )
    {
        case wxBORDER_SUNKEN:
            for ( i = 0; i < BORDER_THICKNESS / 2; i++ )
            {
                DrawSunkenBorder(dc, &rect);
            }
            break;

        case wxBORDER_STATIC:
            DrawShadedRect(dc, &rect, m_penDarkGrey, m_penHighlight);
            break;

        case wxBORDER_RAISED:
            for ( i = 0; i < BORDER_THICKNESS / 2; i++ )
            {
                DrawRaisedBorder(dc, &rect);
            }
            break;

        case wxBORDER_DOUBLE:
            DrawArrowBorder(dc, &rect);
            DrawRect(dc, &rect, m_penLightGrey);
            break;

        case wxBORDER_SIMPLE:
            for ( i = 0; i < BORDER_THICKNESS / 2; i++ )
            {
                DrawRect(dc, &rect, m_penBlack);
            }
            break;

        default:
            wxFAIL_MSG(_T("unknown border type"));
            // fall through

        case wxBORDER_DEFAULT:
        case wxBORDER_NONE:
            break;
    }

    if ( rectIn )
        *rectIn = rect;
}

wxRect wxWin32Renderer::GetBorderDimensions(wxBorder border) const
{
    wxCoord width;
    switch ( border )
    {
        case wxBORDER_RAISED:
        case wxBORDER_SUNKEN:
            width = BORDER_THICKNESS;
            break;

        case wxBORDER_SIMPLE:
        case wxBORDER_STATIC:
            width = 1;
            break;

        case wxBORDER_DOUBLE:
            width = 3;
            break;

        default:
        { 
            // char *crash = NULL;
            // *crash = 0;
            wxFAIL_MSG(_T("unknown border type"));
            // fall through
        }

        case wxBORDER_DEFAULT:
        case wxBORDER_NONE:
            width = 0;
            break;
    }

    wxRect rect;
    rect.x =
    rect.y =
    rect.width =
    rect.height = width;

    return rect;
}

bool wxWin32Renderer::AreScrollbarsInsideBorder() const
{
    return TRUE;
}

// ----------------------------------------------------------------------------
// borders
// ----------------------------------------------------------------------------

void wxWin32Renderer::DrawTextBorder(wxDC& dc,
                                     wxBorder border,
                                     const wxRect& rect,
                                     int flags,
                                     wxRect *rectIn)
{
    // text controls are not special under windows
    DrawBorder(dc, border, rect, flags, rectIn);
}

void wxWin32Renderer::DrawButtonBorder(wxDC& dc,
                                       const wxRect& rectTotal,
                                       int flags,
                                       wxRect *rectIn)
{
    wxRect rect = rectTotal;

    if ( flags & wxCONTROL_PRESSED )
    {
        // button pressed: draw a double border around it
        DrawRect(dc, &rect, m_penBlack);
        DrawRect(dc, &rect, m_penDarkGrey);
    }
    else
    {
        // button not pressed

        if ( flags & (wxCONTROL_FOCUSED | wxCONTROL_ISDEFAULT) )
        {
            // button either default or focused (or both): add an extra border around it
            DrawRect(dc, &rect, m_penBlack);
        }

        // now draw a normal button
        DrawShadedRect(dc, &rect, m_penHighlight, m_penBlack);
        DrawHalfRect(dc, &rect, m_penDarkGrey);
    }

    if ( rectIn )
    {
        *rectIn = rect;
    }
}

// ----------------------------------------------------------------------------
// lines and frame
// ----------------------------------------------------------------------------

void wxWin32Renderer::DrawHorizontalLine(wxDC& dc,
                                         wxCoord y, wxCoord x1, wxCoord x2)
{
    dc.SetPen(m_penDarkGrey);
    dc.DrawLine(x1, y, x2 + 1, y);
    dc.SetPen(m_penHighlight);
    y++;
    dc.DrawLine(x1, y, x2 + 1, y);
}

void wxWin32Renderer::DrawVerticalLine(wxDC& dc,
                                       wxCoord x, wxCoord y1, wxCoord y2)
{
    dc.SetPen(m_penDarkGrey);
    dc.DrawLine(x, y1, x, y2 + 1);
    dc.SetPen(m_penHighlight);
    x++;
    dc.DrawLine(x, y1, x, y2 + 1);
}

void wxWin32Renderer::DrawFrame(wxDC& dc,
                                const wxString& label,
                                const wxRect& rect,
                                int flags,
                                int alignment,
                                int indexAccel)
{
    wxCoord height = 0; // of the label
    wxRect rectFrame = rect;
    if ( !label.empty() )
    {
        // the text should touch the top border of the rect, so the frame
        // itself should be lower
        dc.GetTextExtent(label, NULL, &height);
        rectFrame.y += height / 2;
        rectFrame.height -= height / 2;

        // we have to draw each part of the frame individually as we can't
        // erase the background beyond the label as it might contain some
        // pixmap already, so drawing everything and then overwriting part of
        // the frame with label doesn't work

        // TODO: the +5 and space insertion should be customizable

        wxRect rectText;
        rectText.x = rectFrame.x + 5;
        rectText.y = rect.y;
        rectText.width = rectFrame.width - 7; // +2 border width
        rectText.height = height;

        wxString label2;
        label2 << _T(' ') << label << _T(' ');
        if ( indexAccel != -1 )
        {
            // adjust it as we prepended a space
            indexAccel++;
        }

        wxRect rectLabel;
        DrawLabel(dc, label2, rectText, flags, alignment, indexAccel, &rectLabel);

        StandardDrawFrame(dc, rectFrame, rectLabel);
    }
    else
    {
        // just draw the complete frame
        DrawShadedRect(dc, &rectFrame, m_penDarkGrey, m_penHighlight);
        DrawShadedRect(dc, &rectFrame, m_penHighlight, m_penDarkGrey);
    }
}

// ----------------------------------------------------------------------------
// label
// ----------------------------------------------------------------------------

void wxWin32Renderer::DrawFocusRect(wxDC& dc, const wxRect& rect)
{
    // VZ: this doesn't work under Windows, the dotted pen has dots of 3
    //     pixels each while we really need dots here... PS_ALTERNATE might
    //     work, but it is for NT 5 only
#if 0
    DrawRect(dc, &rect, wxPen(*wxBLACK, 0, wxDOT));
#else
    // draw the pixels manually: note that to behave in the same manner as
    // DrawRect(), we must exclude the bottom and right borders from the
    // rectangle
    wxCoord x1 = rect.GetLeft(),
            y1 = rect.GetTop(),
            x2 = rect.GetRight(),
            y2 = rect.GetBottom();

    dc.SetPen(wxPen(*wxBLACK, 0, wxSOLID));

    // this seems to be closer than what Windows does than wxINVERT although
    // I'm still not sure if it's correct
    dc.SetLogicalFunction(wxAND_REVERSE);

    wxCoord z;
    for ( z = x1 + 1; z < x2; z += 2 )
        dc.DrawPoint(z, rect.GetTop());

    wxCoord shift = z == x2 ? 0 : 1;
    for ( z = y1 + shift; z < y2; z += 2 )
        dc.DrawPoint(x2, z);

    shift = z == y2 ? 0 : 1;
    for ( z = x2 - shift; z > x1; z -= 2 )
        dc.DrawPoint(z, y2);

    shift = z == x1 ? 0 : 1;
    for ( z = y2 - shift; z > y1; z -= 2 )
        dc.DrawPoint(x1, z);

    dc.SetLogicalFunction(wxCOPY);
#endif // 0/1
}

void wxWin32Renderer::DrawLabelShadow(wxDC& dc,
                                      const wxString& label,
                                      const wxRect& rect,
                                      int alignment,
                                      int indexAccel)
{
    // draw shadow of the text
    dc.SetTextForeground(m_colHighlight);
    wxRect rectShadow = rect;
    rectShadow.x++;
    rectShadow.y++;
    dc.DrawLabel(label, rectShadow, alignment, indexAccel);

    // make the text grey
    dc.SetTextForeground(m_colDarkGrey);
}

void wxWin32Renderer::DrawLabel(wxDC& dc,
                                const wxString& label,
                                const wxRect& rect,
                                int flags,
                                int alignment,
                                int indexAccel,
                                wxRect *rectBounds)
{
    DoDrawLabel(dc, label, rect, flags, alignment, indexAccel, rectBounds);
}

void wxWin32Renderer::DoDrawLabel(wxDC& dc,
                                  const wxString& label,
                                  const wxRect& rect,
                                  int flags,
                                  int alignment,
                                  int indexAccel,
                                  wxRect *rectBounds,
                                  const wxPoint& focusOffset)
{
    // the underscores are not drawn for focused controls in wxMSW
    if ( flags & wxCONTROL_FOCUSED )
    {
        indexAccel = -1;
    }

    if ( flags & wxCONTROL_DISABLED )
    {
        // the combination of wxCONTROL_SELECTED and wxCONTROL_DISABLED
        // currently only can happen for a menu item and it seems that Windows
        // doesn't draw the shadow in this case, so we don't do it neither
        if ( flags & wxCONTROL_SELECTED )
        {
            // just make the label text greyed out
            dc.SetTextForeground(m_colDarkGrey);
        }
        else // draw normal disabled label
        {
            DrawLabelShadow(dc, label, rect, alignment, indexAccel);
        }
    }

    wxRect rectLabel;
    dc.DrawLabel(label, wxNullBitmap, rect, alignment, indexAccel, &rectLabel);

    if ( flags & wxCONTROL_DISABLED )
    {
        // restore the fg colour
        dc.SetTextForeground(*wxBLACK);
    }

    if ( flags & wxCONTROL_FOCUSED )
    {
        if ( focusOffset.x || focusOffset.y )
        {
            rectLabel.Inflate(focusOffset.x, focusOffset.y);
        }

        DrawFocusRect(dc, rectLabel);
    }

    if ( rectBounds )
        *rectBounds = rectLabel;
}

void wxWin32Renderer::DrawButtonLabel(wxDC& dc,
                                      const wxString& label,
                                      const wxBitmap& image,
                                      const wxRect& rect,
                                      int flags,
                                      int alignment,
                                      int indexAccel,
                                      wxRect *rectBounds)
{
    // the underscores are not drawn for focused controls in wxMSW
    if ( flags & wxCONTROL_PRESSED )
    {
        indexAccel = -1;
    }

    wxRect rectLabel = rect;
    if ( !label.empty() )
    {
        // shift the label if a button is pressed
        if ( flags & wxCONTROL_PRESSED )
        {
            rectLabel.x++;
            rectLabel.y++;
        }

        if ( flags & wxCONTROL_DISABLED )
        {
            DrawLabelShadow(dc, label, rectLabel, alignment, indexAccel);
        }

        // leave enough space for the focus rectangle
        if ( flags & wxCONTROL_FOCUSED )
        {
            rectLabel.Inflate(-2);
        }
    }

    dc.DrawLabel(label, image, rectLabel, alignment, indexAccel, rectBounds);

    if ( !label.empty() && (flags & wxCONTROL_FOCUSED) )
    {
        if ( flags & wxCONTROL_PRESSED )
        {
            // the focus rectangle is never pressed, so undo the shift done
            // above
            rectLabel.x--;
            rectLabel.y--;
            rectLabel.width--;
            rectLabel.height--;
        }

        DrawFocusRect(dc, rectLabel);
    }
}

// ----------------------------------------------------------------------------
// (check)listbox items
// ----------------------------------------------------------------------------

void wxWin32Renderer::DrawItem(wxDC& dc,
                               const wxString& label,
                               const wxRect& rect,
                               int flags)
{
    wxDCTextColourChanger colChanger(dc);

    if ( flags & wxCONTROL_SELECTED )
    {
        colChanger.Set(wxSCHEME_COLOUR(m_scheme, HIGHLIGHT_TEXT));

        wxColour colBg = wxSCHEME_COLOUR(m_scheme, HIGHLIGHT);
        dc.SetBrush(wxBrush(colBg, wxSOLID));
        dc.SetPen(wxPen(colBg, 0, wxSOLID));
        dc.DrawRectangle(rect);
    }

    wxRect rectText = rect;
    rectText.x += 2;
    rectText.width -= 2;
    dc.DrawLabel(label, wxNullBitmap, rectText);

    if ( flags & wxCONTROL_FOCUSED )
    {
        DrawFocusRect(dc, rect);
    }
}

void wxWin32Renderer::DrawCheckItem(wxDC& dc,
                                    const wxString& label,
                                    const wxBitmap& bitmap,
                                    const wxRect& rect,
                                    int flags)
{
    wxBitmap bmp;
    if ( bitmap.Ok() )
    {
        bmp = bitmap;
    }
    else // use default bitmap
    {
        IndicatorStatus i = flags & wxCONTROL_CHECKED
                                ? IndicatorStatus_Checked
                                : IndicatorStatus_Unchecked;

        if ( !m_bmpCheckBitmaps[i].Ok() )
        {
            m_bmpCheckBitmaps[i] = wxBitmap(xpmChecked[i]);
        }

        bmp = m_bmpCheckBitmaps[i];
    }

    dc.DrawBitmap(bmp, rect.x, rect.y + (rect.height - bmp.GetHeight()) / 2 - 1,
                  TRUE /* use mask */);

    wxRect rectLabel = rect;
    int bmpWidth = bmp.GetWidth();
    rectLabel.x += bmpWidth;
    rectLabel.width -= bmpWidth;

    DrawItem(dc, label, rectLabel, flags);
}

// ----------------------------------------------------------------------------
// check/radio buttons
// ----------------------------------------------------------------------------

wxBitmap wxWin32Renderer::GetIndicator(IndicatorType indType, int flags)
{
    IndicatorState indState;
    if ( flags & wxCONTROL_SELECTED )
        indState = flags & wxCONTROL_DISABLED ? IndicatorState_SelectedDisabled
                                              : IndicatorState_Selected;
    else if ( flags & wxCONTROL_DISABLED )
        indState = IndicatorState_Disabled;
    else if ( flags & wxCONTROL_PRESSED )
        indState = IndicatorState_Pressed;
    else
        indState = IndicatorState_Normal;

    IndicatorStatus indStatus = flags & wxCONTROL_CHECKED
                                    ? IndicatorStatus_Checked
                                    : IndicatorStatus_Unchecked;

    wxBitmap bmp = m_bmpIndicators[indType][indState][indStatus];
    if ( !bmp.Ok() )
    {
        const char **xpm = xpmIndicators[indType][indState][indStatus];
        if ( xpm )
        {
            // create and cache it
            bmp = wxBitmap(xpm);
            m_bmpIndicators[indType][indState][indStatus] = bmp;
        }
    }

    return bmp;
}

void wxWin32Renderer::DrawCheckOrRadioButton(wxDC& dc,
                                             const wxString& label,
                                             const wxBitmap& bitmap,
                                             const wxRect& rect,
                                             int flags,
                                             wxAlignment align,
                                             int indexAccel,
                                             wxCoord focusOffsetY)
{
    // calculate the position of the bitmap and of the label
    wxCoord heightBmp = bitmap.GetHeight();
    wxCoord xBmp,
            yBmp = rect.y + (rect.height - heightBmp) / 2;

    wxRect rectLabel;
    dc.GetMultiLineTextExtent(label, NULL, &rectLabel.height);
    rectLabel.y = rect.y + (rect.height - rectLabel.height) / 2;

    // align label vertically with the bitmap - looks nicer like this
    rectLabel.y -= (rectLabel.height - heightBmp) % 2;

    // calc horz position
    if ( align == wxALIGN_RIGHT )
    {
        xBmp = rect.GetRight() - bitmap.GetWidth();
        rectLabel.x = rect.x + 3;
        rectLabel.SetRight(xBmp);
    }
    else // normal (checkbox to the left of the text) case
    {
        xBmp = rect.x;
        rectLabel.x = xBmp + bitmap.GetWidth() + 5;
        rectLabel.SetRight(rect.GetRight());
    }

    dc.DrawBitmap(bitmap, xBmp, yBmp, TRUE /* use mask */);

    DoDrawLabel(
                dc, label, rectLabel,
                flags,
                wxALIGN_LEFT | wxALIGN_TOP,
                indexAccel,
                NULL,         // we don't need bounding rect
                // use custom vert focus rect offset
                wxPoint(FOCUS_RECT_OFFSET_X, focusOffsetY)
               );
}

void wxWin32Renderer::DrawRadioButton(wxDC& dc,
                                      const wxString& label,
                                      const wxBitmap& bitmap,
                                      const wxRect& rect,
                                      int flags,
                                      wxAlignment align,
                                      int indexAccel)
{
    wxBitmap bmp;
    if ( bitmap.Ok() )
        bmp = bitmap;
    else
        bmp = GetRadioBitmap(flags);

    DrawCheckOrRadioButton(dc, label,
                           bmp,
                           rect, flags, align, indexAccel,
                           FOCUS_RECT_OFFSET_Y); // default focus rect offset
}

void wxWin32Renderer::DrawCheckButton(wxDC& dc,
                                      const wxString& label,
                                      const wxBitmap& bitmap,
                                      const wxRect& rect,
                                      int flags,
                                      wxAlignment align,
                                      int indexAccel)
{
    wxBitmap bmp;
    if ( bitmap.Ok() )
        bmp = bitmap;
    else
        bmp = GetCheckBitmap(flags);

    DrawCheckOrRadioButton(dc, label,
                           bmp,
                           rect, flags, align, indexAccel,
                           0); // no focus rect offset for checkboxes
}

void wxWin32Renderer::DrawToolBarButton(wxDC& dc,
                                        const wxString& label,
                                        const wxBitmap& bitmap,
                                        const wxRect& rectOrig,
                                        int flags)
{
    if ( !label.empty() || bitmap.Ok() )
    {
        wxRect rect = rectOrig;
        rect.Deflate(BORDER_THICKNESS);

        if ( flags & wxCONTROL_PRESSED )
        {
            DrawBorder(dc, wxBORDER_SUNKEN, rect, flags);
        }
        else if ( flags & wxCONTROL_CURRENT )
        {
            DrawBorder(dc, wxBORDER_RAISED, rect, flags);
        }

        dc.DrawLabel(label, bitmap, rect, wxALIGN_CENTRE);
    }
    else // a separator
    {
        // leave a small gap aroudn the line, also account for the toolbar
        // border itself
        DrawVerticalLine(dc, rectOrig.x + rectOrig.width/2,
                         rectOrig.y + 2*BORDER_THICKNESS,
                         rectOrig.GetBottom() - BORDER_THICKNESS);
    }
}

// ----------------------------------------------------------------------------
// text control
// ----------------------------------------------------------------------------

void wxWin32Renderer::DrawTextLine(wxDC& dc,
                                   const wxString& text,
                                   const wxRect& rect,
                                   int selStart,
                                   int selEnd,
                                   int flags)
{
    // nothing special to do here
    StandardDrawTextLine(dc, text, rect, selStart, selEnd, flags);
}

void wxWin32Renderer::DrawLineWrapMark(wxDC& dc, const wxRect& rect)
{
    // we don't draw them
}

// ----------------------------------------------------------------------------
// notebook
// ----------------------------------------------------------------------------

void wxWin32Renderer::DrawTab(wxDC& dc,
                              const wxRect& rectOrig,
                              wxDirection dir,
                              const wxString& label,
                              const wxBitmap& bitmap,
                              int flags,
                              int indexAccel)
{
    wxRect rect = rectOrig;

    // the current tab is drawn indented (to the top for default case) and
    // bigger than the other ones
    const wxSize indent = GetTabIndent();
    if ( flags & wxCONTROL_SELECTED )
    {
        switch ( dir )
        {
            default:
                wxFAIL_MSG(_T("invaild notebook tab orientation"));
                // fall through

            case wxTOP:
                rect.Inflate(indent.x, 0);
                rect.y -= indent.y;
                rect.height += indent.y;
                break;

            case wxBOTTOM:
                rect.Inflate(indent.x, 0);
                rect.height += indent.y;
                break;

            case wxLEFT:
            case wxRIGHT:
                wxFAIL_MSG(_T("TODO"));
                break;
        }
    }

    // draw the text, image and the focus around them (if necessary)
    wxRect rectLabel = rect;
    rectLabel.Deflate(1, 1);
    DrawButtonLabel(dc, label, bitmap, rectLabel,
                    flags, wxALIGN_CENTRE, indexAccel);

    // now draw the tab border itself (maybe use DrawRoundedRectangle()?)
    static const wxCoord CUTOFF = 2; // radius of the rounded corner
    wxCoord x = rect.x,
            y = rect.y,
            x2 = rect.GetRight(),
            y2 = rect.GetBottom();

    // FIXME: all this code will break if the tab indent or the border width,
    //        it is tied to the fact that both of them are equal to 2
    switch ( dir )
    {
        default:
        case wxTOP:
            dc.SetPen(m_penHighlight);
            dc.DrawLine(x, y2, x, y + CUTOFF);
            dc.DrawLine(x, y + CUTOFF, x + CUTOFF, y);
            dc.DrawLine(x + CUTOFF, y, x2 - CUTOFF + 1, y);

            dc.SetPen(m_penBlack);
            dc.DrawLine(x2, y2, x2, y + CUTOFF);
            dc.DrawLine(x2, y + CUTOFF, x2 - CUTOFF, y);

            dc.SetPen(m_penDarkGrey);
            dc.DrawLine(x2 - 1, y2, x2 - 1, y + CUTOFF - 1);

            if ( flags & wxCONTROL_SELECTED )
            {
                dc.SetPen(m_penLightGrey);

                // overwrite the part of the border below this tab
                dc.DrawLine(x + 1, y2 + 1, x2 - 1, y2 + 1);

                // and the shadow of the tab to the left of us
                dc.DrawLine(x + 1, y + CUTOFF + 1, x + 1, y2 + 1);
            }
            break;

        case wxBOTTOM:
            dc.SetPen(m_penHighlight);
            // we need to continue one pixel further to overwrite the corner of
            // the border for the selected tab
            dc.DrawLine(x, y - (flags & wxCONTROL_SELECTED ? 1 : 0),
                        x, y2 - CUTOFF);
            dc.DrawLine(x, y2 - CUTOFF, x + CUTOFF, y2);

            dc.SetPen(m_penBlack);
            dc.DrawLine(x + CUTOFF, y2, x2 - CUTOFF + 1, y2);
            dc.DrawLine(x2, y, x2, y2 - CUTOFF);
            dc.DrawLine(x2, y2 - CUTOFF, x2 - CUTOFF, y2);

            dc.SetPen(m_penDarkGrey);
            dc.DrawLine(x + CUTOFF, y2 - 1, x2 - CUTOFF + 1, y2 - 1);
            dc.DrawLine(x2 - 1, y, x2 - 1, y2 - CUTOFF + 1);

            if ( flags & wxCONTROL_SELECTED )
            {
                dc.SetPen(m_penLightGrey);

                // overwrite the part of the (double!) border above this tab
                dc.DrawLine(x + 1, y - 1, x2 - 1, y - 1);
                dc.DrawLine(x + 1, y - 2, x2 - 1, y - 2);

                // and the shadow of the tab to the left of us
                dc.DrawLine(x + 1, y2 - CUTOFF, x + 1, y - 1);
            }
            break;

        case wxLEFT:
        case wxRIGHT:
            wxFAIL_MSG(_T("TODO"));
    }
}

// ----------------------------------------------------------------------------
// slider
// ----------------------------------------------------------------------------

wxSize wxWin32Renderer::GetSliderThumbSize(const wxRect& rect,
                                           wxOrientation orient) const
{
    wxSize size;

    wxRect rectShaft = GetSliderShaftRect(rect, orient);
    if ( orient == wxHORIZONTAL )
    {
        size.y = rect.height - 6;
        size.x = wxMin(size.y / 2, rectShaft.width);
    }
    else // vertical
    {
        size.x = rect.width - 6;
        size.y = wxMin(size.x / 2, rectShaft.height);
    }

    return size;
}

wxRect wxWin32Renderer::GetSliderShaftRect(const wxRect& rectOrig,
                                           wxOrientation orient) const
{
    static const wxCoord SLIDER_MARGIN = 6;

    wxRect rect = rectOrig;

    if ( orient == wxHORIZONTAL )
    {
        // make the rect of minimal width and centre it
        rect.height = 2*BORDER_THICKNESS;
        rect.y = rectOrig.y + (rectOrig.height - rect.height) / 2;
        if ( rect.y < 0 )
            rect.y = 0;

        // leave margins on the sides
        rect.Deflate(SLIDER_MARGIN, 0);
    }
    else // vertical
    {
        // same as above but in other direction
        rect.width = 2*BORDER_THICKNESS;
        rect.x = rectOrig.x + (rectOrig.width - rect.width) / 2;
        if ( rect.x < 0 )
            rect.x = 0;

        rect.Deflate(0, SLIDER_MARGIN);
    }

    return rect;
}

void wxWin32Renderer::DrawSliderShaft(wxDC& dc,
                                      const wxRect& rectOrig,
                                      wxOrientation orient,
                                      int flags,
                                      wxRect *rectShaft)
{
    if ( flags & wxCONTROL_FOCUSED )
    {
        DrawFocusRect(dc, rectOrig);
    }

    wxRect rect = GetSliderShaftRect(rectOrig, orient);

    if ( rectShaft )
        *rectShaft = rect;

    DrawSunkenBorder(dc, &rect);
}

void wxWin32Renderer::DrawSliderThumb(wxDC& dc,
                                      const wxRect& rect,
                                      wxOrientation orient,
                                      int flags)
{
    /*
       we are drawing a shape of this form

       HHHHHHB <--- y
       H    DB
       H    DB
       H    DB   where H is hightlight colour
       H    DB         D    dark grey
       H    DB         B    black
       H    DB
       H    DB <--- y3
        H  DB
         HDB
          B    <--- y2

       ^  ^  ^
       |  |  |
       x x3  x2

       The interior of this shape is filled with the hatched brush if the thumb
       is pressed.
    */

    DrawBackground(dc, wxNullColour, rect, flags);

    bool transpose = orient == wxVERTICAL;

    wxCoord x, y, x2, y2;
    if ( transpose )
    {
        x = rect.y;
        y = rect.x;
        x2 = rect.GetBottom();
        y2 = rect.GetRight();
    }
    else
    {
        x = rect.x;
        y = rect.y;
        x2 = rect.GetRight();
        y2 = rect.GetBottom();
    }

    // the size of the pointed part of the thumb
    wxCoord sizeArrow = (transpose ? rect.height : rect.width) / 2;

    wxCoord x3 = x + sizeArrow,
            y3 = y2 - sizeArrow;

    dc.SetPen(m_penHighlight);
    DrawLine(dc, x, y, x2, y, transpose);
    DrawLine(dc, x, y + 1, x, y2 - sizeArrow, transpose);
    DrawLine(dc, x, y3, x3, y2, transpose);

    dc.SetPen(m_penBlack);
    DrawLine(dc, x3, y2, x2, y3, transpose);
    DrawLine(dc, x2, y3, x2, y - 1, transpose);

    dc.SetPen(m_penDarkGrey);
    DrawLine(dc, x3, y2 - 1, x2 - 1, y3, transpose);
    DrawLine(dc, x2 - 1, y3, x2 - 1, y, transpose);

    if ( flags & wxCONTROL_PRESSED )
    {
        // TODO: MSW fills the entire area inside, not just the rect
        wxRect rectInt = rect;
        if ( transpose )
            rectInt.SetRight(y3);
        else
            rectInt.SetBottom(y3);
        rectInt.Deflate(2);

#if !defined(__WXMGL__)
        static const char *stipple_xpm[] = {
            /* columns rows colors chars-per-pixel */
            "2 2 2 1",
            "  c None",
            "w c white",
            /* pixels */
            "w ",
            " w",
        };
#else
        // VS: MGL can only do 8x8 stipple brushes
        static const char *stipple_xpm[] = {
            /* columns rows colors chars-per-pixel */
            "8 8 2 1",
            "  c None",
            "w c white",
            /* pixels */
            "w w w w ",
            " w w w w",
            "w w w w ",
            " w w w w",
            "w w w w ",
            " w w w w",
            "w w w w ",
            " w w w w",
        };
#endif
        dc.SetBrush(wxBrush(stipple_xpm));

        dc.SetTextForeground(wxSCHEME_COLOUR(m_scheme, SHADOW_HIGHLIGHT));
        dc.SetTextBackground(wxSCHEME_COLOUR(m_scheme, CONTROL));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(rectInt);
    }
}

void wxWin32Renderer::DrawSliderTicks(wxDC& dc,
                                      const wxRect& rect,
                                      const wxSize& sizeThumb,
                                      wxOrientation orient,
                                      int start,
                                      int end,
                                      int step,
                                      int flags)
{
    if ( end == start )
    {
        // empty slider?
        return;
    }

    // the variable names correspond to horizontal case, but they can be used
    // for both orientations
    wxCoord x1, x2, y1, y2, len, widthThumb;
    if ( orient == wxHORIZONTAL )
    {
        x1 = rect.GetLeft();
        x2 = rect.GetRight();

        // draw from bottom to top to leave one pixel space between the ticks
        // and the slider as Windows do
        y1 = rect.GetBottom();
        y2 = rect.GetTop();

        len = rect.width;

        widthThumb = sizeThumb.x;
    }
    else // vertical
    {
        x1 = rect.GetTop();
        x2 = rect.GetBottom();

        y1 = rect.GetRight();
        y2 = rect.GetLeft();

        len = rect.height;

        widthThumb = sizeThumb.y;
    }

    // the first tick should be positioned in such way that a thumb drawn in
    // the first position points down directly to it
    x1 += widthThumb / 2;
    x2 -= widthThumb / 2;

    // this also means that we have slightly less space for the ticks in
    // between the first and the last
    len -= widthThumb;

    dc.SetPen(m_penBlack);

    int range = end - start;
    for ( int n = 0; n < range; n += step )
    {
        wxCoord x = x1 + (len*n) / range;

        DrawLine(dc, x, y1, x, y2, orient == wxVERTICAL);
    }

    // always draw the line at the end position
    DrawLine(dc, x2, y1, x2, y2, orient == wxVERTICAL);
}

// ----------------------------------------------------------------------------
// menu and menubar
// ----------------------------------------------------------------------------

// wxWin32MenuGeometryInfo: the wxMenuGeometryInfo used by wxWin32Renderer
class WXDLLEXPORT wxWin32MenuGeometryInfo : public wxMenuGeometryInfo
{
public:
    virtual wxSize GetSize() const { return m_size; }

    wxCoord GetLabelOffset() const { return m_ofsLabel; }
    wxCoord GetAccelOffset() const { return m_ofsAccel; }

    wxCoord GetItemHeight() const { return m_heightItem; }

private:
    // the total size of the menu
    wxSize m_size;

    // the offset of the start of the menu item label
    wxCoord m_ofsLabel;

    // the offset of the start of the accel label
    wxCoord m_ofsAccel;

    // the height of a normal (not separator) item
    wxCoord m_heightItem;

    friend wxMenuGeometryInfo *
        wxWin32Renderer::GetMenuGeometry(wxWindow *, const wxMenu&) const;
};

// FIXME: all constants are hardcoded but shouldn't be
static const wxCoord MENU_LEFT_MARGIN = 9;
static const wxCoord MENU_RIGHT_MARGIN = 18;
static const wxCoord MENU_VERT_MARGIN = 3;

// the margin around bitmap/check marks (on each side)
static const wxCoord MENU_BMP_MARGIN = 2;

// the margin between the labels and accel strings
static const wxCoord MENU_ACCEL_MARGIN = 8;

// the separator height in pixels: in fact, strangely enough, the real height
// is 2 but Windows adds one extra pixel in the bottom margin, so take it into
// account here
static const wxCoord MENU_SEPARATOR_HEIGHT = 3;

// the size of the standard checkmark bitmap
static const wxCoord MENU_CHECK_SIZE = 9;

void wxWin32Renderer::DrawMenuBarItem(wxDC& dc,
                                      const wxRect& rectOrig,
                                      const wxString& label,
                                      int flags,
                                      int indexAccel)
{
    wxRect rect = rectOrig;
    rect.height--;

    wxDCTextColourChanger colChanger(dc);

    if ( flags & wxCONTROL_SELECTED )
    {
        colChanger.Set(wxSCHEME_COLOUR(m_scheme, HIGHLIGHT_TEXT));

        wxColour colBg = wxSCHEME_COLOUR(m_scheme, HIGHLIGHT);
        dc.SetBrush(wxBrush(colBg, wxSOLID));
        dc.SetPen(wxPen(colBg, 0, wxSOLID));
        dc.DrawRectangle(rect);
    }

    // don't draw the focus rect around menu bar items
    DrawLabel(dc, label, rect, flags & ~wxCONTROL_FOCUSED,
              wxALIGN_CENTRE, indexAccel);
}

void wxWin32Renderer::DrawMenuItem(wxDC& dc,
                                   wxCoord y,
                                   const wxMenuGeometryInfo& gi,
                                   const wxString& label,
                                   const wxString& accel,
                                   const wxBitmap& bitmap,
                                   int flags,
                                   int indexAccel)
{
    const wxWin32MenuGeometryInfo& geometryInfo =
        (const wxWin32MenuGeometryInfo&)gi;

    wxRect rect;
    rect.x = 0;
    rect.y = y;
    rect.width = geometryInfo.GetSize().x;
    rect.height = geometryInfo.GetItemHeight();

    // draw the selected item specially
    wxDCTextColourChanger colChanger(dc);
    if ( flags & wxCONTROL_SELECTED )
    {
        colChanger.Set(wxSCHEME_COLOUR(m_scheme, HIGHLIGHT_TEXT));

        wxColour colBg = wxSCHEME_COLOUR(m_scheme, HIGHLIGHT);
        dc.SetBrush(wxBrush(colBg, wxSOLID));
        dc.SetPen(wxPen(colBg, 0, wxSOLID));
        dc.DrawRectangle(rect);
    }

    // draw the bitmap: use the bitmap provided or the standard checkmark for
    // the checkable items
    wxBitmap bmp = bitmap;
    if ( !bmp.Ok() && (flags & wxCONTROL_CHECKED) )
    {
        bmp = GetIndicator(IndicatorType_Menu, flags);
    }

    if ( bmp.Ok() )
    {
        rect.SetRight(geometryInfo.GetLabelOffset());
        wxControlRenderer::DrawBitmap(dc, bmp, rect);
    }

    // draw the label
    rect.x = geometryInfo.GetLabelOffset();
    rect.SetRight(geometryInfo.GetAccelOffset());

    DrawLabel(dc, label, rect, flags, wxALIGN_CENTRE_VERTICAL, indexAccel);

    // draw the accel string
    rect.x = geometryInfo.GetAccelOffset();
    rect.SetRight(geometryInfo.GetSize().x);

    // NB: no accel index here
    DrawLabel(dc, accel, rect, flags, wxALIGN_CENTRE_VERTICAL);

    // draw the submenu indicator
    if ( flags & wxCONTROL_ISSUBMENU )
    {
        rect.x = geometryInfo.GetSize().x - MENU_RIGHT_MARGIN;
        rect.width = MENU_RIGHT_MARGIN;

        wxArrowStyle arrowStyle;
        if ( flags & wxCONTROL_DISABLED )
            arrowStyle = flags & wxCONTROL_SELECTED ? Arrow_InversedDisabled
                                                    : Arrow_Disabled;
        else if ( flags & wxCONTROL_SELECTED )
            arrowStyle = Arrow_Inversed;
        else
            arrowStyle = Arrow_Normal;

        DrawArrow(dc, rect, Arrow_Right, arrowStyle);
    }
}

void wxWin32Renderer::DrawMenuSeparator(wxDC& dc,
                                        wxCoord y,
                                        const wxMenuGeometryInfo& geomInfo)
{
    DrawHorizontalLine(dc, y + MENU_VERT_MARGIN, 0, geomInfo.GetSize().x);
}

wxSize wxWin32Renderer::GetMenuBarItemSize(const wxSize& sizeText) const
{
    wxSize size = sizeText;

    // FIXME: menubar height is configurable under Windows
    size.x += 12;
    size.y += 6;

    return size;
}

wxMenuGeometryInfo *wxWin32Renderer::GetMenuGeometry(wxWindow *win,
                                                     const wxMenu& menu) const
{
    // prepare the dc: for now we draw all the items with the system font
    wxClientDC dc(win);
    dc.SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));

    // the height of a normal item
    wxCoord heightText = dc.GetCharHeight();

    // the total height
    wxCoord height = 0;

    // the max length of label and accel strings: the menu width is the sum of
    // them, even if they're for different items (as the accels should be
    // aligned)
    //
    // the max length of the bitmap is never 0 as Windows always leaves enough
    // space for a check mark indicator
    wxCoord widthLabelMax = 0,
            widthAccelMax = 0,
            widthBmpMax = MENU_LEFT_MARGIN;

    for ( wxMenuItemList::Node *node = menu.GetMenuItems().GetFirst();
          node;
          node = node->GetNext() )
    {
        // height of this item
        wxCoord h;

        wxMenuItem *item = node->GetData();
        if ( item->IsSeparator() )
        {
            h = MENU_SEPARATOR_HEIGHT;
        }
        else // not separator
        {
            h = heightText;

            wxCoord widthLabel;
            dc.GetTextExtent(item->GetLabel(), &widthLabel, NULL);
            if ( widthLabel > widthLabelMax )
            {
                widthLabelMax = widthLabel;
            }

            wxCoord widthAccel;
            dc.GetTextExtent(item->GetAccelString(), &widthAccel, NULL);
            if ( widthAccel > widthAccelMax )
            {
                widthAccelMax = widthAccel;
            }

            const wxBitmap& bmp = item->GetBitmap();
            if ( bmp.Ok() )
            {
                wxCoord widthBmp = bmp.GetWidth();
                if ( widthBmp > widthBmpMax )
                    widthBmpMax = widthBmp;
            }
            //else if ( item->IsCheckable() ): no need to check for this as
            // MENU_LEFT_MARGIN is big enough to show the check mark
        }

        h += 2*MENU_VERT_MARGIN;

        // remember the item position and height
        item->SetGeometry(height, h);

        height += h;
    }

    // bundle the metrics into a struct and return it
    wxWin32MenuGeometryInfo *gi = new wxWin32MenuGeometryInfo;

    gi->m_ofsLabel = widthBmpMax + 2*MENU_BMP_MARGIN;
    gi->m_ofsAccel = gi->m_ofsLabel + widthLabelMax;
    if ( widthAccelMax > 0 )
    {
        // if we actually have any accesl, add a margin
        gi->m_ofsAccel += MENU_ACCEL_MARGIN;
    }

    gi->m_heightItem = heightText + 2*MENU_VERT_MARGIN;

    gi->m_size.x = gi->m_ofsAccel + widthAccelMax + MENU_RIGHT_MARGIN;
    gi->m_size.y = height;

    return gi;
}

// ----------------------------------------------------------------------------
// status bar
// ----------------------------------------------------------------------------

static const wxCoord STATBAR_BORDER_X = 2;
static const wxCoord STATBAR_BORDER_Y = 2;

wxSize wxWin32Renderer::GetStatusBarBorders(wxCoord *borderBetweenFields) const
{
    if ( borderBetweenFields )
        *borderBetweenFields = 2;

    return wxSize(STATBAR_BORDER_X, STATBAR_BORDER_Y);
}

void wxWin32Renderer::DrawStatusField(wxDC& dc,
                                      const wxRect& rect,
                                      const wxString& label,
                                      int flags)
{
    wxRect rectIn;

    if ( flags & wxCONTROL_ISDEFAULT )
    {
        // draw the size grip: it is a normal rect except that in the lower
        // right corner we have several bands which may be used for dragging
        // the status bar corner
        //
        // each band consists of 4 stripes: m_penHighlight, double
        // m_penDarkGrey and transparent one
        wxCoord x2 = rect.GetRight(),
                y2 = rect.GetBottom();

        // draw the upper left part of the rect normally
        dc.SetPen(m_penDarkGrey);
        dc.DrawLine(rect.GetLeft(), rect.GetTop(), rect.GetLeft(), y2);
        dc.DrawLine(rect.GetLeft() + 1, rect.GetTop(), x2, rect.GetTop());

        // draw the grey stripes of the grip
        size_t n;
        wxCoord ofs = WIDTH_STATUSBAR_GRIP_BAND - 1;
        for ( n = 0; n < NUM_STATUSBAR_GRIP_BANDS; n++, ofs += WIDTH_STATUSBAR_GRIP_BAND )
        {
            dc.DrawLine(x2 - ofs + 1, y2 - 1, x2, y2 - ofs);
            dc.DrawLine(x2 - ofs, y2 - 1, x2, y2 - ofs - 1);
        }

        // draw the white stripes
        dc.SetPen(m_penHighlight);
        ofs = WIDTH_STATUSBAR_GRIP_BAND + 1;
        for ( n = 0; n < NUM_STATUSBAR_GRIP_BANDS; n++, ofs += WIDTH_STATUSBAR_GRIP_BAND )
        {
            dc.DrawLine(x2 - ofs + 1, y2 - 1, x2, y2 - ofs);
        }

        // draw the remaining rect boundaries
        ofs -= WIDTH_STATUSBAR_GRIP_BAND;
        dc.DrawLine(x2, rect.GetTop(), x2, y2 - ofs + 1);
        dc.DrawLine(rect.GetLeft(), y2, x2 - ofs + 1, y2);

        rectIn = rect;
        rectIn.Deflate(1);

        rectIn.width -= STATUSBAR_GRIP_SIZE;
    }
    else // normal pane
    {
        DrawBorder(dc, wxBORDER_STATIC, rect, flags, &rectIn);
    }

    rectIn.Deflate(STATBAR_BORDER_X, STATBAR_BORDER_Y);

    wxDCClipper clipper(dc, rectIn);
    DrawLabel(dc, label, rectIn, flags, wxALIGN_LEFT | wxALIGN_CENTRE_VERTICAL);
}

// ----------------------------------------------------------------------------
// combobox
// ----------------------------------------------------------------------------

void wxWin32Renderer::GetComboBitmaps(wxBitmap *bmpNormal,
                                      wxBitmap *bmpFocus,
                                      wxBitmap *bmpPressed,
                                      wxBitmap *bmpDisabled)
{
    static const wxCoord widthCombo = 16;
    static const wxCoord heightCombo = 17;

    wxMemoryDC dcMem;

    if ( bmpNormal )
    {
        bmpNormal->Create(widthCombo, heightCombo);
        dcMem.SelectObject(*bmpNormal);
        DrawArrowButton(dcMem, wxRect(0, 0, widthCombo, heightCombo),
                        Arrow_Down, Arrow_Normal);
    }

    if ( bmpPressed )
    {
        bmpPressed->Create(widthCombo, heightCombo);
        dcMem.SelectObject(*bmpPressed);
        DrawArrowButton(dcMem, wxRect(0, 0, widthCombo, heightCombo),
                        Arrow_Down, Arrow_Pressed);
    }

    if ( bmpDisabled )
    {
        bmpDisabled->Create(widthCombo, heightCombo);
        dcMem.SelectObject(*bmpDisabled);
        DrawArrowButton(dcMem, wxRect(0, 0, widthCombo, heightCombo),
                        Arrow_Down, Arrow_Disabled);
    }
}

// ----------------------------------------------------------------------------
// background
// ----------------------------------------------------------------------------

void wxWin32Renderer::DoDrawBackground(wxDC& dc,
                                       const wxColour& col,
                                       const wxRect& rect,
                                       wxWindow *window )
{
    wxBrush brush(col, wxSOLID);
    dc.SetBrush(brush);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(rect);
}

void wxWin32Renderer::DrawBackground(wxDC& dc,
                                     const wxColour& col,
                                     const wxRect& rect,
                                     int flags,
                                     wxWindow *window )
{
    // just fill it with the given or default bg colour
    wxColour colBg = col.Ok() ? col : wxSCHEME_COLOUR(m_scheme, CONTROL);
    DoDrawBackground(dc, colBg, rect, window );
}

// ----------------------------------------------------------------------------
// scrollbar
// ----------------------------------------------------------------------------

void wxWin32Renderer::DrawArrow(wxDC& dc,
                                wxDirection dir,
                                const wxRect& rect,
                                int flags)
{
    // get the bitmap for this arrow
    wxArrowDirection arrowDir;
    switch ( dir )
    {
        case wxLEFT:    arrowDir = Arrow_Left; break;
        case wxRIGHT:   arrowDir = Arrow_Right; break;
        case wxUP:      arrowDir = Arrow_Up; break;
        case wxDOWN:    arrowDir = Arrow_Down; break;

        default:
            wxFAIL_MSG(_T("unknown arrow direction"));
            return;
    }

    wxArrowStyle arrowStyle;
    if ( flags & wxCONTROL_PRESSED )
    {
        // can't be pressed and disabled
        arrowStyle = Arrow_Pressed;
    }
    else
    {
        arrowStyle = flags & wxCONTROL_DISABLED ? Arrow_Disabled : Arrow_Normal;
    }

    DrawArrowButton(dc, rect, arrowDir, arrowStyle);
}

void wxWin32Renderer::DrawArrow(wxDC& dc,
                                const wxRect& rect,
                                wxArrowDirection arrowDir,
                                wxArrowStyle arrowStyle)
{
    const wxBitmap& bmp = m_bmpArrows[arrowStyle][arrowDir];

    // under Windows the arrows always have the same size so just centre it in
    // the provided rectangle
    wxCoord x = rect.x + (rect.width - bmp.GetWidth()) / 2,
            y = rect.y + (rect.height - bmp.GetHeight()) / 2;

    // Windows does it like this...
    if ( arrowDir == Arrow_Left )
        x--;

    // draw it
    dc.DrawBitmap(bmp, x, y, TRUE /* use mask */);
}

void wxWin32Renderer::DrawArrowButton(wxDC& dc,
                                      const wxRect& rectAll,
                                      wxArrowDirection arrowDir,
                                      wxArrowStyle arrowStyle)
{
    wxRect rect = rectAll;
    DoDrawBackground(dc, wxSCHEME_COLOUR(m_scheme, CONTROL), rect);
    DrawArrowBorder(dc, &rect, arrowStyle == Arrow_Pressed);
    DrawArrow(dc, rect, arrowDir, arrowStyle);
}

void wxWin32Renderer::DrawScrollbarThumb(wxDC& dc,
                                         wxOrientation orient,
                                         const wxRect& rect,
                                         int flags)
{
    // we don't use the flags, the thumb never changes appearance
    wxRect rectThumb = rect;
    DrawArrowBorder(dc, &rectThumb);
    DrawBackground(dc, wxNullColour, rectThumb);
}

void wxWin32Renderer::DrawScrollbarShaft(wxDC& dc,
                                         wxOrientation orient,
                                         const wxRect& rectBar,
                                         int flags)
{
    wxColourScheme::StdColour col = flags & wxCONTROL_PRESSED
                                    ? wxColourScheme::SCROLLBAR_PRESSED
                                    : wxColourScheme::SCROLLBAR;
    DoDrawBackground(dc, m_scheme->Get(col), rectBar);
}

void wxWin32Renderer::DrawScrollCorner(wxDC& dc, const wxRect& rect)
{
    DoDrawBackground(dc, wxSCHEME_COLOUR(m_scheme, CONTROL), rect);
}

wxRect wxWin32Renderer::GetScrollbarRect(const wxScrollBar *scrollbar,
                                         wxScrollBar::Element elem,
                                         int thumbPos) const
{
    return StandardGetScrollbarRect(scrollbar, elem,
                                    thumbPos, m_sizeScrollbarArrow);
}

wxCoord wxWin32Renderer::GetScrollbarSize(const wxScrollBar *scrollbar)
{
    return StandardScrollBarSize(scrollbar, m_sizeScrollbarArrow);
}

wxHitTest wxWin32Renderer::HitTestScrollbar(const wxScrollBar *scrollbar,
                                            const wxPoint& pt) const
{
    return StandardHitTestScrollbar(scrollbar, pt, m_sizeScrollbarArrow);
}

wxCoord wxWin32Renderer::ScrollbarToPixel(const wxScrollBar *scrollbar,
                                          int thumbPos)
{
    return StandardScrollbarToPixel(scrollbar, thumbPos, m_sizeScrollbarArrow);
}

int wxWin32Renderer::PixelToScrollbar(const wxScrollBar *scrollbar,
                                      wxCoord coord)
{
    return StandardPixelToScrollbar(scrollbar, coord, m_sizeScrollbarArrow);
}

// ----------------------------------------------------------------------------
// top level windows
// ----------------------------------------------------------------------------

int wxWin32Renderer::HitTestFrame(const wxRect& rect, const wxPoint& pt, int flags) const
{
    wxRect client = GetFrameClientArea(rect, flags);

    if ( client.Inside(pt) )
        return wxHT_TOPLEVEL_CLIENT_AREA;

    if ( flags & wxTOPLEVEL_TITLEBAR )
    {
        wxRect client = GetFrameClientArea(rect, flags & ~wxTOPLEVEL_TITLEBAR);

        if ( flags & wxTOPLEVEL_ICON )
        {
            if ( wxRect(client.GetPosition(), GetFrameIconSize()).Inside(pt) )
                return wxHT_TOPLEVEL_ICON;
        }

        wxRect btnRect(client.GetRight() - 2 - FRAME_BUTTON_WIDTH,
                       client.GetTop() + (FRAME_TITLEBAR_HEIGHT-FRAME_BUTTON_HEIGHT)/2,
                       FRAME_BUTTON_WIDTH, FRAME_BUTTON_HEIGHT);

        if ( flags & wxTOPLEVEL_BUTTON_CLOSE )
        {
            if ( btnRect.Inside(pt) )
                return wxHT_TOPLEVEL_BUTTON_CLOSE;
            btnRect.x -= FRAME_BUTTON_WIDTH + 2;
        }
        if ( flags & wxTOPLEVEL_BUTTON_MAXIMIZE )
        {
            if ( btnRect.Inside(pt) )
                return wxHT_TOPLEVEL_BUTTON_MAXIMIZE;
            btnRect.x -= FRAME_BUTTON_WIDTH;
        }
        if ( flags & wxTOPLEVEL_BUTTON_RESTORE )
        {
            if ( btnRect.Inside(pt) )
                return wxHT_TOPLEVEL_BUTTON_RESTORE;
            btnRect.x -= FRAME_BUTTON_WIDTH;
        }
        if ( flags & wxTOPLEVEL_BUTTON_ICONIZE )
        {
            if ( btnRect.Inside(pt) )
                return wxHT_TOPLEVEL_BUTTON_ICONIZE;
            btnRect.x -= FRAME_BUTTON_WIDTH;
        }
        if ( flags & wxTOPLEVEL_BUTTON_HELP )
        {
            if ( btnRect.Inside(pt) )
                return wxHT_TOPLEVEL_BUTTON_HELP;
            btnRect.x -= FRAME_BUTTON_WIDTH;
        }

        if ( pt.y >= client.y && pt.y < client.y + FRAME_TITLEBAR_HEIGHT )
            return wxHT_TOPLEVEL_TITLEBAR;
    }

    if ( (flags & wxTOPLEVEL_BORDER) && !(flags & wxTOPLEVEL_MAXIMIZED) )
    {
        // we are certainly at one of borders, lets decide which one:

        int border = 0;
        // dirty trick, relies on the way wxHT_TOPLEVEL_XXX are defined!
        if ( pt.x < client.x )
            border |= wxHT_TOPLEVEL_BORDER_W;
        else if ( pt.x >= client.width + client.x )
            border |= wxHT_TOPLEVEL_BORDER_E;
        if ( pt.y < client.y )
            border |= wxHT_TOPLEVEL_BORDER_N;
        else if ( pt.y >= client.height + client.y )
            border |= wxHT_TOPLEVEL_BORDER_S;
        return border;
    }

    return wxHT_NOWHERE;
}

void wxWin32Renderer::DrawFrameTitleBar(wxDC& dc,
                                        const wxRect& rect,
                                        const wxString& title,
                                        const wxIcon& icon,
                                        int flags,
                                        int specialButton,
                                        int specialButtonFlags)
{
    if ( (flags & wxTOPLEVEL_BORDER) && !(flags & wxTOPLEVEL_MAXIMIZED) )
    {
        DrawFrameBorder(dc, rect, flags);
    }
    if ( flags & wxTOPLEVEL_TITLEBAR )
    {
        DrawFrameBackground(dc, rect, flags);
        if ( flags & wxTOPLEVEL_ICON )
            DrawFrameIcon(dc, rect, icon, flags);
        DrawFrameTitle(dc, rect, title, flags);

        wxRect client = GetFrameClientArea(rect, flags & ~wxTOPLEVEL_TITLEBAR);
        wxCoord x,y;
        x = client.GetRight() - 2 - FRAME_BUTTON_WIDTH;
        y = client.GetTop() + (FRAME_TITLEBAR_HEIGHT-FRAME_BUTTON_HEIGHT)/2;

        if ( flags & wxTOPLEVEL_BUTTON_CLOSE )
        {
            DrawFrameButton(dc, x, y, wxTOPLEVEL_BUTTON_CLOSE,
                            (specialButton == wxTOPLEVEL_BUTTON_CLOSE) ?
                            specialButtonFlags : 0);
            x -= FRAME_BUTTON_WIDTH + 2;
        }
        if ( flags & wxTOPLEVEL_BUTTON_MAXIMIZE )
        {
            DrawFrameButton(dc, x, y, wxTOPLEVEL_BUTTON_MAXIMIZE,
                            (specialButton == wxTOPLEVEL_BUTTON_MAXIMIZE) ?
                            specialButtonFlags : 0);
            x -= FRAME_BUTTON_WIDTH;
        }
        if ( flags & wxTOPLEVEL_BUTTON_RESTORE )
        {
            DrawFrameButton(dc, x, y, wxTOPLEVEL_BUTTON_RESTORE,
                            (specialButton == wxTOPLEVEL_BUTTON_RESTORE) ?
                            specialButtonFlags : 0);
            x -= FRAME_BUTTON_WIDTH;
        }
        if ( flags & wxTOPLEVEL_BUTTON_ICONIZE )
        {
            DrawFrameButton(dc, x, y, wxTOPLEVEL_BUTTON_ICONIZE,
                            (specialButton == wxTOPLEVEL_BUTTON_ICONIZE) ?
                            specialButtonFlags : 0);
            x -= FRAME_BUTTON_WIDTH;
        }
        if ( flags & wxTOPLEVEL_BUTTON_HELP )
        {
            DrawFrameButton(dc, x, y, wxTOPLEVEL_BUTTON_HELP,
                            (specialButton == wxTOPLEVEL_BUTTON_HELP) ?
                            specialButtonFlags : 0);
            x -= FRAME_BUTTON_WIDTH;
        }
    }
}

void wxWin32Renderer::DrawFrameBorder(wxDC& dc,
                                      const wxRect& rect,
                                      int flags)
{
    if ( !(flags & wxTOPLEVEL_BORDER) ) return;

    wxRect r(rect);

    DrawShadedRect(dc, &r, m_penLightGrey, m_penBlack);
    DrawShadedRect(dc, &r, m_penHighlight, m_penDarkGrey);
    DrawShadedRect(dc, &r, m_penLightGrey, m_penLightGrey);
    if ( flags & wxTOPLEVEL_RESIZEABLE )
        DrawShadedRect(dc, &r, m_penLightGrey, m_penLightGrey);
}

void wxWin32Renderer::DrawFrameBackground(wxDC& dc,
                                          const wxRect& rect,
                                          int flags)
{
    if ( !(flags & wxTOPLEVEL_TITLEBAR) ) return;

    wxColour col = (flags & wxTOPLEVEL_ACTIVE) ?
                   wxSCHEME_COLOUR(m_scheme, TITLEBAR_ACTIVE) :
                   wxSCHEME_COLOUR(m_scheme, TITLEBAR);

    wxRect r = GetFrameClientArea(rect, flags & ~wxTOPLEVEL_TITLEBAR);
    r.height = FRAME_TITLEBAR_HEIGHT;

    DrawBackground(dc, col, r);
}

void wxWin32Renderer::DrawFrameTitle(wxDC& dc,
                                     const wxRect& rect,
                                     const wxString& title,
                                     int flags)
{
    wxColour col = (flags & wxTOPLEVEL_ACTIVE) ?
                   wxSCHEME_COLOUR(m_scheme, TITLEBAR_ACTIVE_TEXT) :
                   wxSCHEME_COLOUR(m_scheme, TITLEBAR_TEXT);

    wxRect r = GetFrameClientArea(rect, flags & ~wxTOPLEVEL_TITLEBAR);
    r.height = FRAME_TITLEBAR_HEIGHT;
    if ( flags & wxTOPLEVEL_ICON )
    {
        r.x += FRAME_TITLEBAR_HEIGHT;
        r.width -= FRAME_TITLEBAR_HEIGHT + 2;
    }
    else
    {
        r.x += 1;
        r.width -= 3;
    }

    if ( flags & wxTOPLEVEL_BUTTON_CLOSE )
        r.width -= FRAME_BUTTON_WIDTH + 2;
    if ( flags & wxTOPLEVEL_BUTTON_MAXIMIZE )
        r.width -= FRAME_BUTTON_WIDTH;
    if ( flags & wxTOPLEVEL_BUTTON_RESTORE )
        r.width -= FRAME_BUTTON_WIDTH;
    if ( flags & wxTOPLEVEL_BUTTON_ICONIZE )
        r.width -= FRAME_BUTTON_WIDTH;
    if ( flags & wxTOPLEVEL_BUTTON_HELP )
        r.width -= FRAME_BUTTON_WIDTH;

    dc.SetFont(m_titlebarFont);
    dc.SetTextForeground(col);

    wxCoord textW;
    dc.GetTextExtent(title, &textW, NULL);
    if ( textW > r.width )
    {
        // text is too big, let's shorten it and add "..." after it:
        size_t len = title.length();
        wxCoord WSoFar, letterW;

        dc.GetTextExtent(wxT("..."), &WSoFar, NULL);
        if ( WSoFar > r.width )
        {
            // not enough space to draw anything
            return;
        }

        wxString s;
        s.Alloc(len);
        for (size_t i = 0; i < len; i++)
        {
            dc.GetTextExtent(title[i], &letterW, NULL);
            if ( letterW + WSoFar > r.width )
                break;
            WSoFar += letterW;
            s << title[i];
        }
        s << wxT("...");
        dc.DrawLabel(s, wxNullBitmap, r,
                     wxALIGN_LEFT | wxALIGN_CENTRE_VERTICAL);
    }
    else
        dc.DrawLabel(title, wxNullBitmap, r,
                     wxALIGN_LEFT | wxALIGN_CENTRE_VERTICAL);
}

void wxWin32Renderer::DrawFrameIcon(wxDC& dc,
                                    const wxRect& rect,
                                    const wxIcon& icon,
                                    int flags)
{
    if ( icon.Ok() )
    {
        wxRect r = GetFrameClientArea(rect, flags & ~wxTOPLEVEL_TITLEBAR);
        dc.DrawIcon(icon, r.x, r.y);
    }
}

void wxWin32Renderer::DrawFrameButton(wxDC& dc,
                                      wxCoord x, wxCoord y,
                                      int button,
                                      int flags)
{
    wxRect r(x, y, FRAME_BUTTON_WIDTH, FRAME_BUTTON_HEIGHT);

    size_t idx = 0;
    switch (button)
    {
        case wxTOPLEVEL_BUTTON_CLOSE:    idx = FrameButton_Close; break;
        case wxTOPLEVEL_BUTTON_MAXIMIZE: idx = FrameButton_Maximize; break;
        case wxTOPLEVEL_BUTTON_ICONIZE: idx = FrameButton_Minimize; break;
        case wxTOPLEVEL_BUTTON_RESTORE:  idx = FrameButton_Restore; break;
        case wxTOPLEVEL_BUTTON_HELP:     idx = FrameButton_Help; break;
        default:
            wxFAIL_MSG(wxT("incorrect button specification"));
    }

    if ( flags & wxCONTROL_PRESSED )
    {
        DrawShadedRect(dc, &r, m_penBlack, m_penHighlight);
        DrawShadedRect(dc, &r, m_penDarkGrey, m_penLightGrey);
        DrawBackground(dc, wxSCHEME_COLOUR(m_scheme, CONTROL), r);
        dc.DrawBitmap(m_bmpFrameButtons[idx], r.x+1, r.y+1, TRUE);
    }
    else
    {
        DrawShadedRect(dc, &r, m_penHighlight, m_penBlack);
        DrawShadedRect(dc, &r, m_penLightGrey, m_penDarkGrey);
        DrawBackground(dc, wxSCHEME_COLOUR(m_scheme, CONTROL), r);
        dc.DrawBitmap(m_bmpFrameButtons[idx], r.x, r.y, TRUE);
    }
}


wxRect wxWin32Renderer::GetFrameClientArea(const wxRect& rect,
                                           int flags) const
{
    wxRect r(rect);

    if ( (flags & wxTOPLEVEL_BORDER) && !(flags & wxTOPLEVEL_MAXIMIZED) )
    {
        int border = (flags & wxTOPLEVEL_RESIZEABLE) ?
                        RESIZEABLE_FRAME_BORDER_THICKNESS :
                        FRAME_BORDER_THICKNESS;
        r.Inflate(-border);
    }
    if ( flags & wxTOPLEVEL_TITLEBAR )
    {
        r.y += FRAME_TITLEBAR_HEIGHT;
        r.height -= FRAME_TITLEBAR_HEIGHT;
    }

    return r;
}

wxSize wxWin32Renderer::GetFrameTotalSize(const wxSize& clientSize,
                                     int flags) const
{
    wxSize s(clientSize);

    if ( (flags & wxTOPLEVEL_BORDER) && !(flags & wxTOPLEVEL_MAXIMIZED) )
    {
        int border = (flags & wxTOPLEVEL_RESIZEABLE) ?
                        RESIZEABLE_FRAME_BORDER_THICKNESS :
                        FRAME_BORDER_THICKNESS;
        s.x += 2*border;
        s.y += 2*border;
    }
    if ( flags & wxTOPLEVEL_TITLEBAR )
        s.y += FRAME_TITLEBAR_HEIGHT;

    return s;
}

wxSize wxWin32Renderer::GetFrameMinSize(int flags) const
{
    wxSize s(0, 0);

    if ( (flags & wxTOPLEVEL_BORDER) && !(flags & wxTOPLEVEL_MAXIMIZED) )
    {
        int border = (flags & wxTOPLEVEL_RESIZEABLE) ?
                        RESIZEABLE_FRAME_BORDER_THICKNESS :
                        FRAME_BORDER_THICKNESS;
        s.x += 2*border;
        s.y += 2*border;
    }

    if ( flags & wxTOPLEVEL_TITLEBAR )
    {
        s.y += FRAME_TITLEBAR_HEIGHT;

        if ( flags & wxTOPLEVEL_ICON )
            s.x += FRAME_TITLEBAR_HEIGHT + 2;
        if ( flags & wxTOPLEVEL_BUTTON_CLOSE )
            s.x += FRAME_BUTTON_WIDTH + 2;
        if ( flags & wxTOPLEVEL_BUTTON_MAXIMIZE )
            s.x += FRAME_BUTTON_WIDTH;
        if ( flags & wxTOPLEVEL_BUTTON_RESTORE )
            s.x += FRAME_BUTTON_WIDTH;
        if ( flags & wxTOPLEVEL_BUTTON_ICONIZE )
            s.x += FRAME_BUTTON_WIDTH;
        if ( flags & wxTOPLEVEL_BUTTON_HELP )
            s.x += FRAME_BUTTON_WIDTH;
    }

    return s;
}

wxSize wxWin32Renderer::GetFrameIconSize() const
{
    return wxSize(16, 16);
}


// ----------------------------------------------------------------------------
// standard icons
// ----------------------------------------------------------------------------

static char *error_xpm[]={
"32 32 5 1",
". c None",
"# c #800000",
"b c #808080",
"a c #ff0000",
"c c #ffffff",
"...........########.............",
"........###aaaaaaaa###..........",
".......#aaaaaaaaaaaaaa#.........",
".....##aaaaaaaaaaaaaaaa##.......",
"....#aaaaaaaaaaaaaaaaaaaa#......",
"...#aaaaaaaaaaaaaaaaaaaaaa#.....",
"...#aaaaaaaaaaaaaaaaaaaaaa#b....",
"..#aaaaaacaaaaaaaaaacaaaaaa#b...",
".#aaaaaacccaaaaaaaacccaaaaaa#...",
".#aaaaacccccaaaaaacccccaaaaa#b..",
".#aaaaaacccccaaaacccccaaaaaa#bb.",
"#aaaaaaaacccccaacccccaaaaaaaa#b.",
"#aaaaaaaaaccccccccccaaaaaaaaa#b.",
"#aaaaaaaaaaccccccccaaaaaaaaaa#bb",
"#aaaaaaaaaaaccccccaaaaaaaaaaa#bb",
"#aaaaaaaaaaaccccccaaaaaaaaaaa#bb",
"#aaaaaaaaaaccccccccaaaaaaaaaa#bb",
"#aaaaaaaaaccccccccccaaaaaaaaa#bb",
"#aaaaaaaacccccaacccccaaaaaaaa#bb",
".#aaaaaacccccaaaacccccaaaaaa#bbb",
".#aaaaacccccaaaaaacccccaaaaa#bbb",
".#aaaaaacccaaaaaaaacccaaaaaa#bb.",
"..#aaaaaacaaaaaaaaaacaaaaaa#bbb.",
"...#aaaaaaaaaaaaaaaaaaaaaa#bbbb.",
"...#aaaaaaaaaaaaaaaaaaaaaa#bbb..",
"....#aaaaaaaaaaaaaaaaaaaa#bbb...",
".....##aaaaaaaaaaaaaaaa##bbbb...",
"......b#aaaaaaaaaaaaaa#bbbbb....",
".......b###aaaaaaaa###bbbbb.....",
".........bb########bbbbbb.......",
"..........bbbbbbbbbbbbbb........",
".............bbbbbbbb..........."};

static char *info_xpm[]={
"32 32 6 1",
". c None",
"d c #000000",
"c c #0000ff",
"# c #808080",
"a c #c0c0c0",
"b c #ffffff",
"...........########.............",
"........###abbbbbba###..........",
"......##abbbbbbbbbbbba##........",
".....#abbbbbbbbbbbbbbbba#.......",
"....#bbbbbbbaccccabbbbbbbd......",
"...#bbbbbbbbccccccbbbbbbbbd.....",
"..#bbbbbbbbbccccccbbbbbbbbbd....",
".#abbbbbbbbbaccccabbbbbbbbbad...",
".#bbbbbbbbbbbbbbbbbbbbbbbbbbd#..",
"#abbbbbbbbbbbbbbbbbbbbbbbbbbad#.",
"#bbbbbbbbbbcccccccbbbbbbbbbbbd#.",
"#bbbbbbbbbbbbcccccbbbbbbbbbbbd##",
"#bbbbbbbbbbbbcccccbbbbbbbbbbbd##",
"#bbbbbbbbbbbbcccccbbbbbbbbbbbd##",
"#bbbbbbbbbbbbcccccbbbbbbbbbbbd##",
"#abbbbbbbbbbbcccccbbbbbbbbbbad##",
".#bbbbbbbbbbbcccccbbbbbbbbbbd###",
".#abbbbbbbbbbcccccbbbbbbbbbad###",
"..#bbbbbbbbcccccccccbbbbbbbd###.",
"...dbbbbbbbbbbbbbbbbbbbbbbd####.",
"....dbbbbbbbbbbbbbbbbbbbbd####..",
".....dabbbbbbbbbbbbbbbbad####...",
"......ddabbbbbbbbbbbbadd####....",
".......#dddabbbbbbaddd#####.....",
"........###dddabbbd#######......",
"..........####dbbbd#####........",
".............#dbbbd##...........",
"...............dbbd##...........",
"................dbd##...........",
".................dd##...........",
"..................###...........",
"...................##..........."};

static char *question_xpm[]={
"32 32 6 1",
". c None",
"c c #000000",
"d c #0000ff",
"# c #808080",
"a c #c0c0c0",
"b c #ffffff",
"...........########.............",
"........###abbbbbba###..........",
"......##abbbbbbbbbbbba##........",
".....#abbbbbbbbbbbbbbbba#.......",
"....#bbbbbbbbbbbbbbbbbbbbc......",
"...#bbbbbbbaddddddabbbbbbbc.....",
"..#bbbbbbbadabbddddabbbbbbbc....",
".#abbbbbbbddbbbbddddbbbbbbbac...",
".#bbbbbbbbddddbbddddbbbbbbbbc#..",
"#abbbbbbbbddddbaddddbbbbbbbbac#.",
"#bbbbbbbbbaddabddddbbbbbbbbbbc#.",
"#bbbbbbbbbbbbbadddbbbbbbbbbbbc##",
"#bbbbbbbbbbbbbdddbbbbbbbbbbbbc##",
"#bbbbbbbbbbbbbddabbbbbbbbbbbbc##",
"#bbbbbbbbbbbbbddbbbbbbbbbbbbbc##",
"#abbbbbbbbbbbbbbbbbbbbbbbbbbac##",
".#bbbbbbbbbbbaddabbbbbbbbbbbc###",
".#abbbbbbbbbbddddbbbbbbbbbbac###",
"..#bbbbbbbbbbddddbbbbbbbbbbc###.",
"...cbbbbbbbbbaddabbbbbbbbbc####.",
"....cbbbbbbbbbbbbbbbbbbbbc####..",
".....cabbbbbbbbbbbbbbbbac####...",
"......ccabbbbbbbbbbbbacc####....",
".......#cccabbbbbbaccc#####.....",
"........###cccabbbc#######......",
"..........####cbbbc#####........",
".............#cbbbc##...........",
"...............cbbc##...........",
"................cbc##...........",
".................cc##...........",
"..................###...........",
"...................##..........."};

static char *warning_xpm[]={
"32 32 6 1",
". c None",
"c c #000000",
"# c #808000",
"d c #808080",
"b c #c0c0c0",
"a c #ffff00",
".............###................",
"............#aabc...............",
"...........#aaaabcd.............",
"...........#aaaaacdd............",
"..........#aaaaaabcdd...........",
"..........#aaaaaaacdd...........",
".........#aaaaaaaabcdd..........",
".........#aaaaaaaaacdd..........",
"........#aaaaaaaaaabcdd.........",
"........#aaabcccbaaacdd.........",
".......#aaaacccccaaabcdd........",
".......#aaaacccccaaaacdd........",
"......#aaaaacccccaaaabcdd.......",
"......#aaaaacccccaaaaacdd.......",
".....#aaaaaacccccaaaaabcdd......",
".....#aaaaaa#ccc#aaaaaacdd......",
"....#aaaaaaabcccbaaaaaabcdd.....",
"....#aaaaaaaacccaaaaaaaacdd.....",
"...#aaaaaaaaa#c#aaaaaaaabcdd....",
"...#aaaaaaaaabcbaaaaaaaaacdd....",
"..#aaaaaaaaaaacaaaaaaaaaabcdd...",
"..#aaaaaaaaaaaaaaaaaaaaaaacdd...",
".#aaaaaaaaaaabccbaaaaaaaaabcdd..",
".#aaaaaaaaaaaccccaaaaaaaaaacdd..",
"#aaaaaaaaaaaaccccaaaaaaaaaabcdd.",
"#aaaaaaaaaaaabccbaaaaaaaaaaacdd.",
"#aaaaaaaaaaaaaaaaaaaaaaaaaaacddd",
"#aaaaaaaaaaaaaaaaaaaaaaaaaabcddd",
".#aaaaaaaaaaaaaaaaaaaaaaaabcdddd",
"..#ccccccccccccccccccccccccddddd",
"....ddddddddddddddddddddddddddd.",
".....ddddddddddddddddddddddddd.."};

wxBitmap wxWin32ArtProvider::CreateBitmap(const wxArtID& id,
                                          const wxArtClient& WXUNUSED(client),
                                          const wxSize& WXUNUSED(size))
{
    if ( id == wxART_INFORMATION )
        return wxBitmap(info_xpm);
    if ( id == wxART_ERROR )
        return wxBitmap(error_xpm);
    if ( id == wxART_WARNING )
        return wxBitmap(warning_xpm);
    if ( id == wxART_QUESTION )
        return wxBitmap(question_xpm);
    return wxNullBitmap;
}


// ----------------------------------------------------------------------------
// text control geometry
// ----------------------------------------------------------------------------

static inline int GetTextBorderWidth()
{
    return 1;
}

wxRect wxWin32Renderer::GetTextTotalArea(const wxTextCtrl *text,
                                         const wxRect& rect) const
{
    wxRect rectTotal = rect;

    wxCoord widthBorder = GetTextBorderWidth();
    rectTotal.Inflate(widthBorder);

    // this is what Windows does
    rectTotal.height++;

    return rectTotal;
}

wxRect wxWin32Renderer::GetTextClientArea(const wxTextCtrl *text,
                                          const wxRect& rect,
                                          wxCoord *extraSpaceBeyond) const
{
    wxRect rectText = rect;

    // undo GetTextTotalArea()
    if ( rectText.height > 0 )
        rectText.height--;

    wxCoord widthBorder = GetTextBorderWidth();
    rectText.Inflate(-widthBorder);

    if ( extraSpaceBeyond )
        *extraSpaceBeyond = 0;

    return rectText;
}

// ----------------------------------------------------------------------------
// size adjustments
// ----------------------------------------------------------------------------

void wxWin32Renderer::AdjustSize(wxSize *size, const wxWindow *window)
{
#if wxUSE_SCROLLBAR
    if ( wxDynamicCast(window, wxScrollBar) )
    {
        // we only set the width of vert scrollbars and height of the
        // horizontal ones
        if ( window->GetWindowStyle() & wxSB_HORIZONTAL )
            size->y = m_sizeScrollbarArrow.y;
        else
            size->x = m_sizeScrollbarArrow.x;

        // skip border width adjustments, they don't make sense for us
        return;
    }
#endif // wxUSE_SCROLLBAR/!wxUSE_SCROLLBAR

#if wxUSE_BUTTON
    if ( wxDynamicCast(window, wxButton) )
    {
        if ( !(window->GetWindowStyle() & wxBU_EXACTFIT) )
        {
            // TODO: don't harcode all this
            size->x += 3*window->GetCharWidth();

            wxCoord heightBtn = (11*(window->GetCharHeight() + 8))/10;
            if ( size->y < heightBtn - 8 )
                size->y = heightBtn;
            else
                size->y += 9;
        }

        // no border width adjustments for buttons
        return;
    }
#endif // wxUSE_BUTTON

    // take into account the border width
    wxRect rectBorder = GetBorderDimensions(window->GetBorder());
    size->x += rectBorder.x + rectBorder.width;
    size->y += rectBorder.y + rectBorder.height;
}

// ============================================================================
// wxInputHandler
// ============================================================================

// ----------------------------------------------------------------------------
// wxWin32InputHandler
// ----------------------------------------------------------------------------

wxWin32InputHandler::wxWin32InputHandler(wxWin32Renderer *renderer)
{
    m_renderer = renderer;
}

bool wxWin32InputHandler::HandleKey(wxInputConsumer *control,
                                    const wxKeyEvent& event,
                                    bool pressed)
{
    return FALSE;
}

bool wxWin32InputHandler::HandleMouse(wxInputConsumer *control,
                                      const wxMouseEvent& event)
{
    // clicking on the control gives it focus
    if ( event.ButtonDown() )
    {
        wxWindow *win = control->GetInputWindow();

        if (( wxWindow::FindFocus() != control->GetInputWindow() ) &&
            ( win->AcceptsFocus() ) )
        {
            win->SetFocus();

            return TRUE;
        }
    }

    return FALSE;
}

// ----------------------------------------------------------------------------
// wxWin32ScrollBarInputHandler
// ----------------------------------------------------------------------------

wxWin32ScrollBarInputHandler::
wxWin32ScrollBarInputHandler(wxWin32Renderer *renderer,
                             wxInputHandler *handler)
        : wxStdScrollBarInputHandler(renderer, handler)
{
    m_scrollPaused = FALSE;
    m_interval = 0;
}

bool wxWin32ScrollBarInputHandler::OnScrollTimer(wxScrollBar *scrollbar,
                                                 const wxControlAction& action)
{
    // stop if went beyond the position of the original click (this can only
    // happen when we scroll by pages)
    bool stop = FALSE;
    if ( action == wxACTION_SCROLL_PAGE_DOWN )
    {
        stop = m_renderer->HitTestScrollbar(scrollbar, m_ptStartScrolling)
                != wxHT_SCROLLBAR_BAR_2;
    }
    else if ( action == wxACTION_SCROLL_PAGE_UP )
    {
        stop = m_renderer->HitTestScrollbar(scrollbar, m_ptStartScrolling)
                != wxHT_SCROLLBAR_BAR_1;
    }

    if ( stop )
    {
        StopScrolling(scrollbar);

        scrollbar->Refresh();

        return FALSE;
    }

    return wxStdScrollBarInputHandler::OnScrollTimer(scrollbar, action);
}

bool wxWin32ScrollBarInputHandler::HandleMouse(wxInputConsumer *control,
                                               const wxMouseEvent& event)
{
    // remember the current state
    bool wasDraggingThumb = m_htLast == wxHT_SCROLLBAR_THUMB;

    // do process the message
    bool rc = wxStdScrollBarInputHandler::HandleMouse(control, event);

    // analyse the changes
    if ( !wasDraggingThumb && (m_htLast == wxHT_SCROLLBAR_THUMB) )
    {
        // we just started dragging the thumb, remember its initial position to
        // be able to restore it if the drag is cancelled later
        m_eventStartDrag = event;
    }

    return rc;
}

bool wxWin32ScrollBarInputHandler::HandleMouseMove(wxInputConsumer *control,
                                                   const wxMouseEvent& event)
{
    // we don't highlight scrollbar elements, so there is no need to process
    // mouse move events normally - only do it while mouse is captured (i.e.
    // when we're dragging the thumb or pressing on something)
    if ( !m_winCapture )
        return FALSE;

    if ( event.Entering() )
    {
        // we're not interested in this at all
        return FALSE;
    }

    wxScrollBar *scrollbar = wxStaticCast(control->GetInputWindow(), wxScrollBar);
    wxHitTest ht;
    if ( m_scrollPaused )
    {
        // check if the mouse returned to its original location

        if ( event.Leaving() )
        {
            // it surely didn't
            return FALSE;
        }

        ht = m_renderer->HitTestScrollbar(scrollbar, event.GetPosition());
        if ( ht == m_htLast )
        {
            // yes it did, resume scrolling
            m_scrollPaused = FALSE;
            if ( m_timerScroll )
            {
                // we were scrolling by line/page, restart timer
                m_timerScroll->Start(m_interval);

                Press(scrollbar, TRUE);
            }
            else // we were dragging the thumb
            {
                // restore its last location
                HandleThumbMove(scrollbar, m_eventLastDrag);
            }

            return TRUE;
        }
    }
    else // normal case, scrolling hasn't been paused
    {
        // if we're scrolling the scrollbar because the arrow or the shaft was
        // pressed, check that the mouse stays on the same scrollbar element

#if 0
        // Always let thumb jump back if we leave the scrollbar
        if ( event.Moving() )
        {
            ht = m_renderer->HitTestScrollbar(scrollbar, event.GetPosition());
        }
        else // event.Leaving()
        {
            ht = wxHT_NOWHERE;
        }
#else
        // Jump back only if we get far away from it
        wxPoint pos = event.GetPosition();
        if (scrollbar->HasFlag( wxVERTICAL ))
        {
            if (pos.x > -40 && pos.x < scrollbar->GetSize().x+40)
               pos.x = 5;
        }
        else
        {
            if (pos.y > -40 && pos.y < scrollbar->GetSize().y+40)
               pos.y = 5;
        }
        ht = m_renderer->HitTestScrollbar(scrollbar, pos );
#endif

        // if we're dragging the thumb and the mouse stays in the scrollbar, it
        // is still ok - we only want to catch the case when the mouse leaves
        // the scrollbar here
        if ( m_htLast == wxHT_SCROLLBAR_THUMB && ht != wxHT_NOWHERE )
        {
            ht = wxHT_SCROLLBAR_THUMB;
        }

        if ( ht != m_htLast )
        {
            // what were we doing? 2 possibilities: either an arrow/shaft was
            // pressed in which case we have a timer and so we just stop it or
            // we were dragging the thumb
            if ( m_timerScroll )
            {
                // pause scrolling
                m_interval = m_timerScroll->GetInterval();
                m_timerScroll->Stop();
                m_scrollPaused = TRUE;

                // unpress the arrow
                Press(scrollbar, FALSE);
            }
            else // we were dragging the thumb
            {
                // remember the current thumb position to be able to restore it
                // if the mouse returns to it later
                m_eventLastDrag = event;

                // and restore the original position (before dragging) of the
                // thumb for now
                HandleThumbMove(scrollbar, m_eventStartDrag);
            }

            return TRUE;
        }
    }

    return wxStdScrollBarInputHandler::HandleMouseMove(control, event);
}

// ----------------------------------------------------------------------------
// wxWin32CheckboxInputHandler
// ----------------------------------------------------------------------------

bool wxWin32CheckboxInputHandler::HandleKey(wxInputConsumer *control,
                                            const wxKeyEvent& event,
                                            bool pressed)
{
    if ( pressed )
    {
        wxControlAction action;
        int keycode = event.GetKeyCode();
        switch ( keycode )
        {
            case WXK_SPACE:
                action = wxACTION_CHECKBOX_TOGGLE;
                break;

            case WXK_SUBTRACT:
            case WXK_NUMPAD_SUBTRACT:
                action = wxACTION_CHECKBOX_CHECK;
                break;

            case WXK_ADD:
            case WXK_NUMPAD_ADD:
            case WXK_NUMPAD_EQUAL:
                action = wxACTION_CHECKBOX_CLEAR;
                break;
        }

        if ( !!action )
        {
            control->PerformAction(action);

            return TRUE;
        }
    }

    return FALSE;
}

// ----------------------------------------------------------------------------
// wxWin32TextCtrlInputHandler
// ----------------------------------------------------------------------------

bool wxWin32TextCtrlInputHandler::HandleKey(wxInputConsumer *control,
                                            const wxKeyEvent& event,
                                            bool pressed)
{
    // handle only MSW-specific text bindings here, the others are handled in
    // the base class
    if ( pressed )
    {
        int keycode = event.GetKeyCode();

        wxControlAction action;
        if ( keycode == WXK_DELETE && event.ShiftDown() )
        {
            action = wxACTION_TEXT_CUT;
        }
        else if ( keycode == WXK_INSERT )
        {
            if ( event.ControlDown() )
                action = wxACTION_TEXT_COPY;
            else if ( event.ShiftDown() )
                action = wxACTION_TEXT_PASTE;
        }

        if ( action != wxACTION_NONE )
        {
            control->PerformAction(action);

            return TRUE;
        }
    }

    return wxStdTextCtrlInputHandler::HandleKey(control, event, pressed);
}

// ----------------------------------------------------------------------------
// wxWin32StatusBarInputHandler
// ----------------------------------------------------------------------------

wxWin32StatusBarInputHandler::
wxWin32StatusBarInputHandler(wxInputHandler *handler)
    : wxStdInputHandler(handler)
{
    m_isOnGrip = FALSE;
}

bool wxWin32StatusBarInputHandler::IsOnGrip(wxWindow *statbar,
                                            const wxPoint& pt) const
{
    if ( statbar->HasFlag(wxST_SIZEGRIP) &&
         statbar->GetParent()->HasFlag(wxRESIZE_BORDER) )
    {
        wxTopLevelWindow *
            parentTLW = wxDynamicCast(statbar->GetParent(), wxTopLevelWindow);

        wxCHECK_MSG( parentTLW, FALSE,
                     _T("the status bar should be a child of a TLW") );

        // a maximized window can't be resized anyhow
        if ( !parentTLW->IsMaximized() )
        {
            // VZ: I think that the standard Windows behaviour is to only
            //     show the resizing cursor when the mouse is on top of the
            //     grip itself but apparently different Windows versions behave
            //     differently (?) and it seems a better UI to allow resizing
            //     the status bar even when the mouse is above the grip
            wxSize sizeSbar = statbar->GetSize();

            int diff = sizeSbar.x - pt.x;
            return diff >= 0 && diff < (wxCoord)STATUSBAR_GRIP_SIZE;
        }
    }

    return FALSE;
}

bool wxWin32StatusBarInputHandler::HandleMouse(wxInputConsumer *consumer,
                                               const wxMouseEvent& event)
{
    if ( event.Button(1) )
    {
        if ( event.ButtonDown(1) )
        {
            wxWindow *statbar = consumer->GetInputWindow();

            if ( IsOnGrip(statbar, event.GetPosition()) )
            {
                wxTopLevelWindow *tlw = wxDynamicCast(statbar->GetParent(),
                                                      wxTopLevelWindow);
                if ( tlw )
                {
                    tlw->PerformAction(wxACTION_TOPLEVEL_RESIZE,
                                       wxHT_TOPLEVEL_BORDER_SE);

                    statbar->SetCursor(m_cursorOld);

                    return TRUE;
                }
            }
        }
    }

    return wxStdInputHandler::HandleMouse(consumer, event);
}

bool wxWin32StatusBarInputHandler::HandleMouseMove(wxInputConsumer *consumer,
                                                   const wxMouseEvent& event)
{
    wxWindow *statbar = consumer->GetInputWindow();

    bool isOnGrip = IsOnGrip(statbar, event.GetPosition());
    if ( isOnGrip != m_isOnGrip )
    {
        m_isOnGrip = isOnGrip;
        if ( isOnGrip )
        {
            m_cursorOld = statbar->GetCursor();
            statbar->SetCursor(wxCURSOR_SIZENWSE);
        }
        else
        {
            statbar->SetCursor(m_cursorOld);
        }
    }

    return wxStdInputHandler::HandleMouseMove(consumer, event);
}

// ----------------------------------------------------------------------------
// wxWin32FrameInputHandler
// ----------------------------------------------------------------------------

class wxWin32SystemMenuEvtHandler : public wxEvtHandler
{
public:
    wxWin32SystemMenuEvtHandler(wxWin32FrameInputHandler *handler);
    
    void Attach(wxInputConsumer *consumer);
    void Detach();
    
private:
    DECLARE_EVENT_TABLE()
    void OnSystemMenu(wxCommandEvent &event);
    void OnCloseFrame(wxCommandEvent &event);
    void OnClose(wxCloseEvent &event);
   
    wxWin32FrameInputHandler *m_inputHnd;
    wxTopLevelWindow         *m_wnd;
    wxAcceleratorTable        m_oldAccelTable;
};

wxWin32SystemMenuEvtHandler::wxWin32SystemMenuEvtHandler(
                                wxWin32FrameInputHandler *handler)
{
    m_inputHnd = handler;
    m_wnd = NULL;
}

void wxWin32SystemMenuEvtHandler::Attach(wxInputConsumer *consumer)
{
    wxASSERT_MSG( m_wnd == NULL, _T("can't attach the handler twice!") );

    m_wnd = wxStaticCast(consumer->GetInputWindow(), wxTopLevelWindow);
    m_wnd->PushEventHandler(this);
    
    // VS: This code relies on using generic implementation of 
    //     wxAcceleratorTable in wxUniv!
    wxAcceleratorTable table = *m_wnd->GetAcceleratorTable();
    m_oldAccelTable = table;
    table.Add(wxAcceleratorEntry(wxACCEL_ALT, WXK_SPACE, wxID_SYSTEM_MENU));
    table.Add(wxAcceleratorEntry(wxACCEL_ALT, WXK_F4, wxID_CLOSE_FRAME));
    m_wnd->SetAcceleratorTable(table);
}

void wxWin32SystemMenuEvtHandler::Detach()
{
    if ( m_wnd )
    {
        m_wnd->SetAcceleratorTable(m_oldAccelTable);
        m_wnd->RemoveEventHandler(this); 
        m_wnd = NULL;
    }
}

BEGIN_EVENT_TABLE(wxWin32SystemMenuEvtHandler, wxEvtHandler)
    EVT_MENU(wxID_SYSTEM_MENU, wxWin32SystemMenuEvtHandler::OnSystemMenu)
    EVT_MENU(wxID_CLOSE_FRAME, wxWin32SystemMenuEvtHandler::OnCloseFrame)
    EVT_CLOSE(wxWin32SystemMenuEvtHandler::OnClose)
END_EVENT_TABLE()

void wxWin32SystemMenuEvtHandler::OnSystemMenu(wxCommandEvent &WXUNUSED(event))
{
    int border = ((m_wnd->GetWindowStyle() & wxRESIZE_BORDER) &&
                  !m_wnd->IsMaximized()) ?
                      RESIZEABLE_FRAME_BORDER_THICKNESS :
                      FRAME_BORDER_THICKNESS;
    wxPoint pt = m_wnd->GetClientAreaOrigin();
    pt.x = -pt.x + border;
    pt.y = -pt.y + border + FRAME_TITLEBAR_HEIGHT;

    wxAcceleratorTable table = *m_wnd->GetAcceleratorTable();
    m_wnd->SetAcceleratorTable(wxNullAcceleratorTable);
    m_inputHnd->PopupSystemMenu(m_wnd, pt);
    m_wnd->SetAcceleratorTable(table);
}

void wxWin32SystemMenuEvtHandler::OnCloseFrame(wxCommandEvent &WXUNUSED(event))
{
    m_wnd->PerformAction(wxACTION_TOPLEVEL_BUTTON_CLICK,
                         wxTOPLEVEL_BUTTON_CLOSE);
}

void wxWin32SystemMenuEvtHandler::OnClose(wxCloseEvent &event)
{
    m_wnd = NULL;
    event.Skip();
}


wxWin32FrameInputHandler::wxWin32FrameInputHandler(wxInputHandler *handler)
        : wxStdFrameInputHandler(handler)
{
    m_menuHandler = new wxWin32SystemMenuEvtHandler(this);
}

wxWin32FrameInputHandler::~wxWin32FrameInputHandler()
{
    if ( m_menuHandler )
    {
        m_menuHandler->Detach();
        delete m_menuHandler;
    }
}

bool wxWin32FrameInputHandler::HandleMouse(wxInputConsumer *consumer,
                                           const wxMouseEvent& event)
{
    if ( event.LeftDClick() || event.LeftDown() || event.RightDown() )
    {
        wxTopLevelWindow *tlw =
            wxStaticCast(consumer->GetInputWindow(), wxTopLevelWindow);

        long hit = tlw->HitTest(event.GetPosition());

        if ( event.LeftDClick() && hit == wxHT_TOPLEVEL_TITLEBAR )
        {
            tlw->PerformAction(wxACTION_TOPLEVEL_BUTTON_CLICK,
                               tlw->IsMaximized() ? wxTOPLEVEL_BUTTON_RESTORE
                                                  : wxTOPLEVEL_BUTTON_MAXIMIZE);
            return TRUE;
        }
        else if ( tlw->GetWindowStyle() & wxSYSTEM_MENU )
        {
            if ( (event.LeftDown() && hit == wxHT_TOPLEVEL_ICON) ||
                 (event.RightDown() && 
                      (hit == wxHT_TOPLEVEL_TITLEBAR || 
                       hit == wxHT_TOPLEVEL_ICON)) )
            {
                PopupSystemMenu(tlw, event.GetPosition());
                return TRUE;
            }
        }
    }

    return wxStdFrameInputHandler::HandleMouse(consumer, event);
}

void wxWin32FrameInputHandler::PopupSystemMenu(wxTopLevelWindow *window, 
                                               const wxPoint& pos) const
{
    wxMenu *menu = new wxMenu;

    if ( window->GetWindowStyle() & wxMAXIMIZE_BOX )
        menu->Append(wxID_RESTORE_FRAME , _("&Restore"));
    menu->Append(wxID_MOVE_FRAME , _("&Move"));
    if ( window->GetWindowStyle() & wxRESIZE_BORDER )
        menu->Append(wxID_RESIZE_FRAME , _("&Size"));
    if ( wxSystemSettings::HasFeature(wxSYS_CAN_ICONIZE_FRAME) )
        menu->Append(wxID_ICONIZE_FRAME , _("Mi&nimize"));
    if ( window->GetWindowStyle() & wxMAXIMIZE_BOX )
        menu->Append(wxID_MAXIMIZE_FRAME , _("Ma&ximize"));
    menu->AppendSeparator();
    menu->Append(wxID_CLOSE_FRAME, _("Close\tAlt-F4"));
    
    if ( window->GetWindowStyle() & wxMAXIMIZE_BOX )
    {
        if ( window->IsMaximized() )
        {
            menu->Enable(wxID_MAXIMIZE_FRAME, FALSE);
            menu->Enable(wxID_MOVE_FRAME, FALSE);
            if ( window->GetWindowStyle() & wxRESIZE_BORDER )
                menu->Enable(wxID_RESIZE_FRAME, FALSE);
        }
        else
            menu->Enable(wxID_RESTORE_FRAME, FALSE);
    }

    window->PopupMenu(menu, pos);
    delete menu;
}

bool wxWin32FrameInputHandler::HandleActivation(wxInputConsumer *consumer, 
                                                bool activated)
{
    if ( consumer->GetInputWindow()->GetWindowStyle() & wxSYSTEM_MENU )
    {
        // always detach if active frame changed:
        m_menuHandler->Detach();

        if ( activated )
        {
            m_menuHandler->Attach(consumer);
        }
    }

    return wxStdFrameInputHandler::HandleActivation(consumer, activated);
}
