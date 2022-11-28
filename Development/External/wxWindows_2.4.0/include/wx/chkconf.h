/*
 * Name:        wx/chkconf.h
 * Purpose:     check the config settings for consistency
 * Author:      Vadim Zeitlin
 * Modified by:
 * Created:     09.08.00
 * RCS-ID:      $Id: chkconf.h,v 1.32.2.1 2002/11/10 20:57:46 MBN Exp $
 * Copyright:   (c) 2000 Vadim Zeitlin <vadim@wxwindows.org>
 * Licence:     wxWindows license
 */

/* THIS IS A C FILE, DON'T USE C++ FEATURES (IN PARTICULAR COMMENTS) IN IT */

/*
   this global setting determintes what should we do if the setting FOO
   requires BAR and BAR is not set: we can either silently define BAR
   (default, recommended) or give an error and abort (mainly useful for
   developpers only)
 */
#define wxABORT_ON_CONFIG_ERROR

/*
   global features
 */

/* GUI build by default */
#if !defined(wxUSE_GUI)
#   define wxUSE_GUI 1
#endif /* !defined(wxUSE_GUI) */

/* wxBase doesn't need compatibility settings as it's a new port */
#if !wxUSE_GUI
#   undef WXWIN_COMPATIBILITY
#   undef WXWIN_COMPATIBILITY_2
#   undef WXWIN_COMPATIBILITY_2_2
#   define WXWIN_COMPATIBILITY 0
#   define WXWIN_COMPATIBILITY_2 0
#   define WXWIN_COMPATIBILITY_2_2 0
#endif /* !wxUSE_GUI */

/*
   tests for non GUI features
 */

#ifndef wxUSE_DYNLIB_CLASS
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_DYNLIB_CLASS must be defined."
#   else
#       define wxUSE_DYNLIB_CLASS 0
#   endif
#endif /* !defined(wxUSE_DYNLIB_CLASS) */

#ifndef wxUSE_FILESYSTEM
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_FILESYSTEM must be defined."
#   else
#       define wxUSE_FILESYSTEM 0
#   endif
#endif /* !defined(wxUSE_FILESYSTEM) */

/* don't give an error about this one yet, it's not fully implemented */
#ifndef wxUSE_FSVOLUME
#   define wxUSE_FSVOLUME 0
#endif /* !defined(wxUSE_FSVOLUME) */

#ifndef wxUSE_DYNAMIC_LOADER
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_DYNAMIC_LOADER must be defined."
#   else
#       define wxUSE_DYNAMIC_LOADER 0
#   endif
#endif /* !defined(wxUSE_DYNAMIC_LOADER) */

#ifndef wxUSE_LOG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_LOG must be defined."
#   else
#       define wxUSE_LOG 0
#   endif
#endif /* !defined(wxUSE_LOG) */

#ifndef wxUSE_LONGLONG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_LONGLONG must be defined."
#   else
#       define wxUSE_LONGLONG 0
#   endif
#endif /* !defined(wxUSE_LONGLONG) */

#ifndef wxUSE_MIMETYPE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_MIMETYPE must be defined."
#   else
#       define wxUSE_MIMETYPE 0
#   endif
#endif /* !defined(wxUSE_MIMETYPE) */

#ifndef wxUSE_PROLOGIO
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_PROLOGIO must be defined."
#   else
#       define wxUSE_PROLOGIO 0
#   endif
#endif /* !defined(wxUSE_PROLOGIO) */

#ifndef wxUSE_PROTOCOL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_PROTOCOL must be defined."
#   else
#       define wxUSE_PROTOCOL 0
#   endif
#endif /* !defined(wxUSE_PROTOCOL) */

/* we may not define wxUSE_PROTOCOL_XXX if wxUSE_PROTOCOL is set to 0 */
#if !wxUSE_PROTOCOL
#   undef wxUSE_PROTOCOL_HTTP
#   undef wxUSE_PROTOCOL_FTP
#   undef wxUSE_PROTOCOL_FILE
#   define wxUSE_PROTOCOL_HTTP 0
#   define wxUSE_PROTOCOL_FTP 0
#   define wxUSE_PROTOCOL_FILE 0
#endif /* wxUSE_PROTOCOL */

#ifndef wxUSE_PROTOCOL_HTTP
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_PROTOCOL_HTTP must be defined."
#   else
#       define wxUSE_PROTOCOL_HTTP 0
#   endif
#endif /* !defined(wxUSE_PROTOCOL_HTTP) */

#ifndef wxUSE_PROTOCOL_FTP
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_PROTOCOL_FTP must be defined."
#   else
#       define wxUSE_PROTOCOL_FTP 0
#   endif
#endif /* !defined(wxUSE_PROTOCOL_FTP) */

#ifndef wxUSE_PROTOCOL_FILE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_PROTOCOL_FILE must be defined."
#   else
#       define wxUSE_PROTOCOL_FILE 0
#   endif
#endif /* !defined(wxUSE_PROTOCOL_FILE) */

#ifndef wxUSE_REGEX
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_REGEX must be defined."
#   else
#       define wxUSE_REGEX 0
#   endif
#endif /* !defined(wxUSE_REGEX) */

#ifndef wxUSE_SOCKETS
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SOCKETS must be defined."
#   else
#       define wxUSE_SOCKETS 0
#   endif
#endif /* !defined(wxUSE_SOCKETS) */

#ifndef wxUSE_STREAMS
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_STREAMS must be defined."
#   else
#       define wxUSE_STREAMS 0
#   endif
#endif /* !defined(wxUSE_STREAMS) */

#ifndef wxUSE_STOPWATCH
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_STOPWATCH must be defined."
#   else
#       define wxUSE_STOPWATCH 0
#   endif
#endif /* !defined(wxUSE_STOPWATCH) */

#ifndef wxUSE_TEXTBUFFER
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TEXTBUFFER must be defined."
#   else
#       define wxUSE_TEXTBUFFER 0
#   endif
#endif /* !defined(wxUSE_TEXTBUFFER) */

#ifndef wxUSE_TEXTFILE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TEXTFILE must be defined."
#   else
#       define wxUSE_TEXTFILE 0
#   endif
#endif /* !defined(wxUSE_TEXTFILE) */

#ifndef wxUSE_UNICODE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_UNICODE must be defined."
#   else
#       define wxUSE_UNICODE 0
#   endif
#endif /* !defined(wxUSE_UNICODE) */

#ifndef wxUSE_URL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_URL must be defined."
#   else
#       define wxUSE_URL 0
#   endif
#endif /* !defined(wxUSE_URL) */

/*
   all these tests are for GUI only
 */
#if wxUSE_GUI

/*
   all of the settings tested below must be defined or we'd get an error from
   preprocessor about invalid integer expression
 */

#ifndef wxUSE_ACCEL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_ACCEL must be defined."
#   else
#       define wxUSE_ACCEL 0
#   endif
#endif /* !defined(wxUSE_ACCEL) */

#ifndef wxUSE_BMPBUTTON
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_BMPBUTTON must be defined."
#   else
#       define wxUSE_BMPBUTTON 0
#   endif
#endif /* !defined(wxUSE_BMPBUTTON) */

#ifndef wxUSE_BUTTON
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_BUTTON must be defined."
#   else
#       define wxUSE_BUTTON 0
#   endif
#endif /* !defined(wxUSE_BUTTON) */

#ifndef wxUSE_CALENDARCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_CALENDARCTRL must be defined."
#   else
#       define wxUSE_CALENDARCTRL 0
#   endif
#endif /* !defined(wxUSE_CALENDARCTRL) */

#ifndef wxUSE_CARET
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_CARET must be defined."
#   else
#       define wxUSE_CARET 0
#   endif
#endif /* !defined(wxUSE_CARET) */

#ifndef wxUSE_CHECKBOX
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_CHECKBOX must be defined."
#   else
#       define wxUSE_CHECKBOX 0
#   endif
#endif /* !defined(wxUSE_CHECKBOX) */

#ifndef wxUSE_CHECKLISTBOX
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_CHECKLISTBOX must be defined."
#   else
#       define wxUSE_CHECKLISTBOX 0
#   endif
#endif /* !defined(wxUSE_CHECKLISTBOX) */

#ifndef wxUSE_CHOICE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_CHOICE must be defined."
#   else
#       define wxUSE_CHOICE 0
#   endif
#endif /* !defined(wxUSE_CHOICE) */

#ifndef wxUSE_CHOICEDLG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_CHOICEDLG must be defined."
#   else
#       define wxUSE_CHOICEDLG 0
#   endif
#endif /* !defined(wxUSE_CHOICEDLG) */

#ifndef wxUSE_CLIPBOARD
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_CLIPBOARD must be defined."
#   else
#       define wxUSE_CLIPBOARD 0
#   endif
#endif /* !defined(wxUSE_CLIPBOARD) */

#ifndef wxUSE_COLOURDLG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_COLOURDLG must be defined."
#   else
#       define wxUSE_COLOURDLG 0
#   endif
#endif /* !defined(wxUSE_COLOURDLG) */

#ifndef wxUSE_COMBOBOX
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_COMBOBOX must be defined."
#   else
#       define wxUSE_COMBOBOX 0
#   endif
#endif /* !defined(wxUSE_COMBOBOX) */

#ifndef wxUSE_DATAOBJ
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_DATAOBJ must be defined."
#   else
#       define wxUSE_DATAOBJ 0
#   endif
#endif /* !defined(wxUSE_DATAOBJ) */

#ifndef wxUSE_DOC_VIEW_ARCHITECTURE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_DOC_VIEW_ARCHITECTURE must be defined."
#   else
#       define wxUSE_DOC_VIEW_ARCHITECTURE 0
#   endif
#endif /* !defined(wxUSE_DOC_VIEW_ARCHITECTURE) */

#ifndef wxUSE_FILEDLG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_FILEDLG must be defined."
#   else
#       define wxUSE_FILEDLG 0
#   endif
#endif /* !defined(wxUSE_FILEDLG) */

#ifndef wxUSE_FONTDLG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_FONTDLG must be defined."
#   else
#       define wxUSE_FONTDLG 0
#   endif
#endif /* !defined(wxUSE_FONTDLG) */

#ifndef wxUSE_FONTMAP
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_FONTMAP must be defined."
#   else
#       define wxUSE_FONTMAP 0
#   endif
#endif /* !defined(wxUSE_FONTMAP) */

#ifndef wxUSE_GAUGE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_GAUGE must be defined."
#   else
#       define wxUSE_GAUGE 0
#   endif
#endif /* !defined(wxUSE_GAUGE) */

#ifndef wxUSE_GRID
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_GRID must be defined."
#   else
#       define wxUSE_GRID 0
#   endif
#endif /* !defined(wxUSE_GRID) */

#ifndef wxUSE_HELP
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_HELP must be defined."
#   else
#       define wxUSE_HELP 0
#   endif
#endif /* !defined(wxUSE_HELP) */

#ifndef wxUSE_HTML
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_HTML must be defined."
#   else
#       define wxUSE_HTML 0
#   endif
#endif /* !defined(wxUSE_HTML) */

#ifndef wxUSE_ICO_CUR
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_ICO_CUR must be defined."
#   else
#       define wxUSE_ICO_CUR 0
#   endif
#endif /* !defined(wxUSE_ICO_CUR) */

#ifndef wxUSE_IFF
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_IFF must be defined."
#   else
#       define wxUSE_IFF 0
#   endif
#endif /* !defined(wxUSE_IFF) */

#ifndef wxUSE_IMAGLIST
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_IMAGLIST must be defined."
#   else
#       define wxUSE_IMAGLIST 0
#   endif
#endif /* !defined(wxUSE_IMAGLIST) */

#ifndef wxUSE_JOYSTICK
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_JOYSTICK must be defined."
#   else
#       define wxUSE_JOYSTICK 0
#   endif
#endif /* !defined(wxUSE_JOYSTICK) */

#ifndef wxUSE_LISTBOX
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_LISTBOX must be defined."
#   else
#       define wxUSE_LISTBOX 0
#   endif
#endif /* !defined(wxUSE_LISTBOX) */

#ifndef wxUSE_LISTCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_LISTCTRL must be defined."
#   else
#       define wxUSE_LISTCTRL 0
#   endif
#endif /* !defined(wxUSE_LISTCTRL) */

#ifndef wxUSE_LOGGUI
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_LOGGUI must be defined."
#   else
#       define wxUSE_LOGGUI 0
#   endif
#endif /* !defined(wxUSE_LOGGUI) */

#ifndef wxUSE_LOGWINDOW
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_LOGWINDOW must be defined."
#   else
#       define wxUSE_LOGWINDOW 0
#   endif
#endif /* !defined(wxUSE_LOGWINDOW) */

#ifndef wxUSE_LOG_DIALOG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_LOG_DIALOG must be defined."
#   else
#       define wxUSE_LOG_DIALOG 0
#   endif
#endif /* !defined(wxUSE_LOG_DIALOG) */

#ifndef wxUSE_MDI_ARCHITECTURE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_MDI_ARCHITECTURE must be defined."
#   else
#       define wxUSE_MDI_ARCHITECTURE 0
#   endif
#endif /* !defined(wxUSE_MDI_ARCHITECTURE) */

#ifndef wxUSE_MENUS
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_MENUS must be defined."
#   else
#       define wxUSE_MENUS 0
#   endif
#endif /* !defined(wxUSE_MENUS) */

#ifndef wxUSE_MSGDLG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_MSGDLG must be defined."
#   else
#       define wxUSE_MSGDLG 0
#   endif
#endif /* !defined(wxUSE_MSGDLG) */

#ifndef wxUSE_NEW_GRID
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_NEW_GRID must be defined."
#   else
#       define wxUSE_NEW_GRID 0
#   endif
#endif /* !defined(wxUSE_NEW_GRID) */

#ifndef wxUSE_NOTEBOOK
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_NOTEBOOK must be defined."
#   else
#       define wxUSE_NOTEBOOK 0
#   endif
#endif /* !defined(wxUSE_NOTEBOOK) */

#ifndef wxUSE_PALETTE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_PALETTE must be defined."
#   else
#       define wxUSE_PALETTE 0
#   endif
#endif /* !defined(wxUSE_PALETTE) */

#ifndef wxUSE_POPUPWIN
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_POPUPWIN must be defined."
#   else
#       define wxUSE_POPUPWIN 0
#   endif
#endif /* !defined(wxUSE_POPUPWIN) */

#ifndef wxUSE_PRINTING_ARCHITECTURE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_PRINTING_ARCHITECTURE must be defined."
#   else
#       define wxUSE_PRINTING_ARCHITECTURE 0
#   endif
#endif /* !defined(wxUSE_PRINTING_ARCHITECTURE) */

#ifndef wxUSE_RADIOBOX
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_RADIOBOX must be defined."
#   else
#       define wxUSE_RADIOBOX 0
#   endif
#endif /* !defined(wxUSE_RADIOBOX) */

#ifndef wxUSE_RADIOBTN
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_RADIOBTN must be defined."
#   else
#       define wxUSE_RADIOBTN 0
#   endif
#endif /* !defined(wxUSE_RADIOBTN) */

#ifndef wxUSE_SASH
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SASH must be defined."
#   else
#       define wxUSE_SASH 0
#   endif
#endif /* !defined(wxUSE_SASH) */

#ifndef wxUSE_SCROLLBAR
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SCROLLBAR must be defined."
#   else
#       define wxUSE_SCROLLBAR 0
#   endif
#endif /* !defined(wxUSE_SCROLLBAR) */

#ifndef wxUSE_SLIDER
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SLIDER must be defined."
#   else
#       define wxUSE_SLIDER 0
#   endif
#endif /* !defined(wxUSE_SLIDER) */

#ifndef wxUSE_SPINBTN
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SPINBTN must be defined."
#   else
#       define wxUSE_SPINBTN 0
#   endif
#endif /* !defined(wxUSE_SPINBTN) */

#ifndef wxUSE_SPINCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SPINCTRL must be defined."
#   else
#       define wxUSE_SPINCTRL 0
#   endif
#endif /* !defined(wxUSE_SPINCTRL) */

#ifndef wxUSE_SPLASH
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SPLASH must be defined."
#   else
#       define wxUSE_SPLASH 0
#   endif
#endif /* !defined(wxUSE_SPLASH) */

#ifndef wxUSE_SPLITTER
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SPLITTER must be defined."
#   else
#       define wxUSE_SPLITTER 0
#   endif
#endif /* !defined(wxUSE_SPLITTER) */

#ifndef wxUSE_STATBMP
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_STATBMP must be defined."
#   else
#       define wxUSE_STATBMP 0
#   endif
#endif /* !defined(wxUSE_STATBMP) */

#ifndef wxUSE_STATBOX
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_STATBOX must be defined."
#   else
#       define wxUSE_STATBOX 0
#   endif
#endif /* !defined(wxUSE_STATBOX) */

#ifndef wxUSE_STATLINE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_STATLINE must be defined."
#   else
#       define wxUSE_STATLINE 0
#   endif
#endif /* !defined(wxUSE_STATLINE) */

#ifndef wxUSE_STATTEXT
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_STATTEXT must be defined."
#   else
#       define wxUSE_STATTEXT 0
#   endif
#endif /* !defined(wxUSE_STATTEXT) */

#ifndef wxUSE_STATUSBAR
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_STATUSBAR must be defined."
#   else
#       define wxUSE_STATUSBAR 0
#   endif
#endif /* !defined(wxUSE_STATUSBAR) */

#ifndef wxUSE_TAB_DIALOG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TAB_DIALOG must be defined."
#   else
#       define wxUSE_TAB_DIALOG 0
#   endif
#endif /* !defined(wxUSE_TAB_DIALOG) */

#ifndef wxUSE_TEXTCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TEXTCTRL must be defined."
#   else
#       define wxUSE_TEXTCTRL 0
#   endif
#endif /* !defined(wxUSE_TEXTCTRL) */

#ifndef wxUSE_TIPWINDOW
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TIPWINDOW must be defined."
#   else
#       define wxUSE_TIPWINDOW 0
#   endif
#endif /* !defined(wxUSE_TIPWINDOW) */

#ifndef wxUSE_TOOLBAR
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TOOLBAR must be defined."
#   else
#       define wxUSE_TOOLBAR 0
#   endif
#endif /* !defined(wxUSE_TOOLBAR) */

#ifndef wxUSE_TOOLTIPS
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TOOLTIPS must be defined."
#   else
#       define wxUSE_TOOLTIPS 0
#   endif
#endif /* !defined(wxUSE_TOOLTIPS) */

#ifndef wxUSE_TREECTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TREECTRL must be defined."
#   else
#       define wxUSE_TREECTRL 0
#   endif
#endif /* !defined(wxUSE_TREECTRL) */

#ifndef wxUSE_WX_RESOURCES
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_WX_RESOURCES must be defined."
#   else
#       define wxUSE_WX_RESOURCES 0
#   endif
#endif /* !defined(wxUSE_WX_RESOURCES) */

#ifndef wxUSE_WXHTML_HELP
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_WXHTML_HELP must be defined."
#   else
#       define wxUSE_WXHTML_HELP 0
#   endif
#endif /* !defined(wxUSE_WXHTML_HELP) */

#endif /* wxUSE_GUI */

/*
   check consistency of the settings
 */

#if wxUSE_PROTOCOL_FILE || wxUSE_PROTOCOL_FTP || wxUSE_PROTOCOL_HTTP
#   if !wxUSE_PROTOCOL
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_PROTOCOL_XXX requires wxUSE_PROTOCOL"
#        else
#            undef wxUSE_PROTOCOL
#            define wxUSE_PROTOCOL 1
#        endif
#   endif
#endif /* wxUSE_PROTOCOL_XXX */

#if wxUSE_URL
#   if !wxUSE_PROTOCOL
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_URL requires wxUSE_PROTOCOL"
#        else
#            undef wxUSE_PROTOCOL
#            define wxUSE_PROTOCOL 1
#        endif
#   endif
#endif /* wxUSE_URL */

#if wxUSE_PROTOCOL
#   if !wxUSE_SOCKETS
#       if wxUSE_PROTOCOL_HTTP || wxUSE_PROTOCOL_FTP
#           ifdef wxABORT_ON_CONFIG_ERROR
#               error "wxUSE_PROTOCOL_FTP/HTTP requires wxUSE_SOCKETS"
#           else
#               undef wxUSE_SOCKETS
#               define wxUSE_SOCKETS 1
#           endif
#       endif
#   endif

#   if !wxUSE_STREAMS
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_PROTOCOL requires wxUSE_STREAMS"
#       else
#           undef wxUSE_STREAMS
#           define wxUSE_STREAMS 1
#       endif
#   endif
#endif /* wxUSE_PROTOCOL */

/* have to test for wxUSE_HTML before wxUSE_FILESYSTEM */
#if wxUSE_HTML
#   if !wxUSE_FILESYSTEM
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxHTML requires wxFileSystem"
#       else
#           undef wxUSE_FILESYSTEM
#           define wxUSE_FILESYSTEM 1
#       endif
#   endif
#endif /* wxUSE_HTML */

#if wxUSE_FILESYSTEM
#   if !wxUSE_STREAMS
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_FILESYSTEM requires wxUSE_STREAMS"
#       else
#           undef wxUSE_STREAMS
#           define wxUSE_STREAMS 1
#       endif
#   endif
#endif /* wxUSE_FILESYSTEM */

#if wxUSE_STOPWATCH || wxUSE_DATETIME
#    if !wxUSE_LONGLONG
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_STOPWATCH and wxUSE_DATETIME require wxUSE_LONGLONG"
#        else
#            undef wxUSE_LONGLONG
#            define wxUSE_LONGLONG 1
#        endif
#    endif
#endif /* wxUSE_STOPWATCH */

#if wxUSE_MIMETYPE && !wxUSE_TEXTFILE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_MIMETYPE requires wxUSE_TEXTFILE"
#   else
#       undef wxUSE_TEXTFILE
#       define wxUSE_TEXTFILE 1
#   endif
#endif /* wxUSE_MIMETYPE */

#if wxUSE_TEXTFILE && !wxUSE_TEXTBUFFER
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TEXTFILE requires wxUSE_TEXTBUFFER"
#   else
#       undef wxUSE_TEXTBUFFER
#       define wxUSE_TEXTBUFFER 1
#   endif
#endif /* wxUSE_TEXTFILE */

#if wxUSE_TEXTFILE && !wxUSE_FILE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TEXTFILE requires wxUSE_FILE"
#   else
#       undef wxUSE_FILE
#       define wxUSE_FILE 1
#   endif
#endif /* wxUSE_TEXTFILE */

#if wxUSE_UNICODE_MSLU && !wxUSE_UNICODE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_UNICODE_MSLU requires wxUSE_UNICODE"
#   else
#       undef wxUSE_UNICODE
#       define wxUSE_UNICODE 1
#   endif
#endif /* wxUSE_UNICODE_MSLU */

#if wxUSE_ODBC && wxUSE_UNICODE
#   ifdef wxABORT_ON_CONFIG_ERROR
        /* (ODBC classes aren't Unicode-compatible yet) */
#       error "wxUSE_ODBC can't be used with wxUSE_UNICODE"
#   else
#       undef wxUSE_ODBC
#       define wxUSE_ODBC 0
#   endif
#endif /* wxUSE_ODBC */

/* the rest of the tests is for the GUI settings only */
#if wxUSE_GUI

#if wxUSE_BUTTON || \
    wxUSE_CALENDARCTRL || \
    wxUSE_CARET || \
    wxUSE_COMBOBOX || \
    wxUSE_BMPBUTTON || \
    wxUSE_CHECKBOX || \
    wxUSE_CHECKLISTBOX || \
    wxUSE_CHOICE || \
    wxUSE_GAUGE || \
    wxUSE_GRID || \
    wxUSE_NEW_GRID || \
    wxUSE_LISTBOX || \
    wxUSE_LISTCTRL || \
    wxUSE_NOTEBOOK || \
    wxUSE_RADIOBOX || \
    wxUSE_RADIOBTN || \
    wxUSE_SCROLLBAR || \
    wxUSE_SLIDER || \
    wxUSE_SPINBTN || \
    wxUSE_SPINCTRL || \
    wxUSE_STATBMP || \
    wxUSE_STATBOX || \
    wxUSE_STATLINE || \
    wxUSE_STATTEXT || \
    wxUSE_STATUSBAR || \
    wxUSE_TEXTCTRL || \
    wxUSE_TOOLBAR || \
    wxUSE_TREECTRL
#    if !wxUSE_CONTROLS
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_CONTROLS unset but some controls used"
#        else
#            undef wxUSE_CONTROLS
#            define wxUSE_CONTROLS 1
#        endif
#    endif
#endif /* controls */

/* wxUniv-specific dependencies */
#if defined(__WXUNIVERSAL__)
#   if (wxUSE_COMBOBOX || wxUSE_MENUS) && !wxUSE_POPUPWIN
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_POPUPWIN must be defined to use comboboxes/menus"
#       else
#           undef wxUSE_POPUPWIN
#           define wxUSE_POPUPWIN 1
#       endif
#   endif

#   if wxUSE_COMBOBOX
#      if !wxUSE_LISTBOX
#           ifdef wxABORT_ON_CONFIG_ERROR
#               error "wxComboBox requires wxListBox in wxUniversal"
#           else
#               undef wxUSE_LISTBOX
#               define wxUSE_LISTBOX 1
#           endif
#      endif
#   endif /* wxUSE_COMBOBOX */

#   if wxUSE_RADIOBTN
#      if !wxUSE_CHECKBOX
#           ifdef wxABORT_ON_CONFIG_ERROR
#               error "wxUSE_RADIOBTN requires wxUSE_CHECKBOX in wxUniversal"
#           else
#               undef wxUSE_CHECKBOX
#               define wxUSE_CHECKBOX 1
#           endif
#      endif
#   endif /* wxUSE_RADIOBTN */

#   if wxUSE_TEXTCTRL
#       if !wxUSE_CARET
#           ifdef wxABORT_ON_CONFIG_ERROR
#               error "wxTextCtrl requires wxCaret in wxUniversal"
#           else
#               undef wxUSE_CARET
#               define wxUSE_CARET 1
#           endif
#       endif /* wxUSE_CARET */

#       if !wxUSE_SCROLLBAR
#           ifdef wxABORT_ON_CONFIG_ERROR
#               error "wxTextCtrl requires wxScrollBar in wxUniversal"
#           else
#               undef wxUSE_SCROLLBAR
#               define wxUSE_SCROLLBAR 1
#           endif
#       endif /* wxUSE_SCROLLBAR */
#   endif /* wxUSE_TEXTCTRL */
#endif /* __WXUNIVERSAL__ */

/* wxGTK-specific dependencies */
#ifdef __WXGTK__
#   ifndef __WXUNIVERSAL__
#       if wxUSE_MDI_ARCHITECTURE && !wxUSE_MENUS
#           ifdef wxABORT_ON_CONFIG_ERROR
#               error "MDI requires wxUSE_MENUS in wxGTK"
#           else
#               undef wxUSE_MENUS
#               define wxUSE_MENUS 1
#           endif
#       endif
#   endif /* !__WXUNIVERSAL__ */

#   if wxUSE_JOYSTICK
#       if !wxUSE_THREADS
#           ifdef wxABORT_ON_CONFIG_ERROR
#               error "wxJoystick requires threads in wxGTK"
#           else
#               undef wxUSE_JOYSTICK
#               define wxUSE_JOYSTICK 0
#           endif
#       endif
#   endif
#endif /* wxGTK && !wxUniv */

/* wxMSW-specific dependencies */
#ifdef __WXMSW__
#   ifndef wxUSE_UNICODE_MSLU
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_UNICODE_MSLU must be defined."
#       else
#           define wxUSE_UNICODE_MSLU 0
#       endif
#   endif  /* wxUSE_UNICODE_MSLU */
#   ifndef wxUSE_MS_HTML_HELP
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_MS_HTML_HELP must be defined."
#       else
#           define wxUSE_MS_HTML_HELP 0
#       endif
#   endif /* !defined(wxUSE_MS_HTML_HELP) */
#   ifndef wxUSE_DIALUP_MANAGER
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_DIALUP_MANAGER must be defined."
#       else
#           define wxUSE_DIALUP_MANAGER 0
#       endif
#   endif /* !defined(wxUSE_DIALUP_MANAGER) */

#   if !wxUSE_DYNAMIC_LOADER
#       if wxUSE_MS_HTML_HELP
#           ifdef wxABORT_ON_CONFIG_ERROR
#               error "wxUSE_MS_HTML_HELP requires wxUSE_DYNAMIC_LOADER."
#           else
#               define wxUSE_DYNAMIC_LOADER 1
#           endif
#       endif
#       if wxUSE_DIALUP_MANAGER
#           ifdef wxABORT_ON_CONFIG_ERROR
#               error "wxUSE_DIALUP_MANAGER requires wxUSE_DYNAMIC_LOADER."
#           else
#               define wxUSE_DYNAMIC_LOADER 1
#           endif
#       endif
#   endif  /* wxUSE_DYNAMIC_LOADER */
#endif /* wxMSW */

/* wxMotif-specific dependencies */
#if defined(__WXMOTIF__) && wxUSE_NOTEBOOK && !wxUSE_TAB_DIALOG
#  undef wxUSE_TAB_DIALOG
#  define wxUSE_TAB_DIALOG 1
#endif
#if defined(__WXMOTIF__) && wxUSE_TOGGLEBTN
#  undef wxUSE_TOGGLEBTN
#  define wxUSE_TOGGLEBTN 0
#endif

/* wxMGL-specific dependencies */
#ifdef __WXMGL__
#   if !wxUSE_PALETTE
#       error "wxMGL requires wxUSE_PALETTE=1"
#   endif
#endif /* wxMGL */

/* generic controls dependencies */
#if !defined(__WXMSW__) || defined(__WXUNIVERSAL__)
#   if wxUSE_FONTDLG || wxUSE_FILEDLG || wxUSE_CHOICEDLG
        /* all common controls are needed by these dialogs */
#       if !defined(wxUSE_CHOICE) || \
           !defined(wxUSE_TEXTCTRL) || \
           !defined(wxUSE_BUTTON) || \
           !defined(wxUSE_CHECKBOX) || \
           !defined(wxUSE_STATTEXT)
#           ifdef wxABORT_ON_CONFIG_ERROR
#               error "These common controls are needed by common dialogs"
#           else
#               undef wxUSE_CHOICE
#               define wxUSE_CHOICE 1
#               undef wxUSE_TEXTCTRL
#               define wxUSE_TEXTCTRL 1
#               undef wxUSE_BUTTON
#               define wxUSE_BUTTON 1
#               undef wxUSE_CHECKBOX
#               define wxUSE_CHECKBOX 1
#               undef wxUSE_STATTEXT
#               define wxUSE_STATTEXT 1
#           endif
#       endif
#   endif
#endif /* !wxMSW || wxUniv */

/* common dependencies */
#if wxUSE_CALENDARCTRL
#   if !(wxUSE_SPINBTN && wxUSE_COMBOBOX)
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxCalendarCtrl requires wxSpinButton and wxComboBox"
#       else
#           undef wxUSE_SPINBTN
#           undef wxUSE_COMBOBOX
#           define wxUSE_SPINBTN 1
#           define wxUSE_COMBOBOX 1
#       endif
#   endif
#endif /* wxUSE_CALENDARCTRL */

#if wxUSE_CHECKLISTBOX
#   if !wxUSE_LISTBOX
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxCheckListBox requires wxListBox"
#        else
#            undef wxUSE_LISTBOX
#            define wxUSE_LISTBOX 1
#        endif
#   endif
#endif /* wxUSE_RADIOBTN */

#if wxUSE_WXHTML_HELP
#   if !wxUSE_HELP || !wxUSE_HTML || !wxUSE_COMBOBOX
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "Built in help controller can't be compiled"
#       else
#           undef wxUSE_HELP
#           define wxUSE_HELP 1
#           undef wxUSE_HTML
#           define wxUSE_HTML 1
#           undef wxUSE_COMBOBOX
#           define wxUSE_COMBOBOX 1
#       endif
#   endif
#endif /* wxUSE_WXHTML_HELP */

#if wxUSE_PRINTING_ARCHITECTURE
#   if !wxUSE_COMBOBOX
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "Print dialog requires wxUSE_COMBOBOX"
#       else
#           undef wxUSE_COMBOBOX
#           define wxUSE_COMBOBOX 1
#       endif
#   endif
#endif /* wxUSE_PRINTING_ARCHITECTURE */

#if wxUSE_DOC_VIEW_ARCHITECTURE
#   if !wxUSE_MENUS
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "DocView requires wxUSE_MENUS"
#        else
#            undef wxUSE_MENUS
#            define wxUSE_MENUS 1
#        endif
#   endif
#endif /* wxUSE_DOC_VIEW_ARCHITECTURE */

#if !wxUSE_FILEDLG
#   if wxUSE_DOC_VIEW_ARCHITECTURE || wxUSE_WXHTML_HELP
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxFileDialog must be compiled as well"
#       else
#           undef wxUSE_FILEDLG
#           define wxUSE_FILEDLG 1
#       endif
#   endif
#endif /* wxUSE_FILEDLG */

#if !wxUSE_IMAGLIST
#   if wxUSE_TREECTRL || wxUSE_NOTEBOOK || wxUSE_LISTCTRL
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxImageList must be compiled as well"
#       else
#           undef wxUSE_IMAGLIST
#           define wxUSE_IMAGLIST 1
#       endif
#   endif
#endif /* !wxUSE_IMAGLIST */

#if !wxUSE_MSGDLG
#   ifdef wxABORT_ON_CONFIG_ERROR
        /* FIXME: should compile without it, of course, but doesn't */
#       error "wxMessageBox is always needed"
#   else
#       undef wxUSE_MSGDLG
#       define wxUSE_MSGDLG 1
#   endif
#endif

#if wxUSE_RADIOBOX
#   if !wxUSE_RADIOBTN || !wxUSE_STATBOX
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_RADIOBOX requires wxUSE_RADIOBTN and wxUSE_STATBOX"
#        else
#            undef wxUSE_RADIOBTN
#            undef wxUSE_STATBOX
#            define wxUSE_RADIOBTN 1
#            define wxUSE_STATBOX 1
#        endif
#   endif
#endif /* wxUSE_RADIOBOX */

#if wxUSE_LOGWINDOW
#    if !wxUSE_TEXTCTRL
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_LOGWINDOW requires wxUSE_TEXTCTRL"
#        else
#            undef wxUSE_TEXTCTRL
#            define wxUSE_TEXTCTRL 1
#        endif
#    endif
#endif /* wxUSE_LOGWINDOW */

#if wxUSE_LOG_DIALOG
#    if !wxUSE_LISTCTRL
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_LOG_DIALOG requires wxUSE_LISTCTRL"
#        else
#            undef wxUSE_LISTCTRL
#            define wxUSE_LISTCTRL 1
#        endif
#    endif
#endif /* wxUSE_LOG_DIALOG */

/* I wonder if we shouldn't just remove all occurrences of
   wxUSE_DYNAMIC_CLASSES from the sources? */
#if !defined(wxUSE_DYNAMIC_CLASSES) || !wxUSE_DYNAMIC_CLASSES
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_DYNAMIC_CLASSES must be defined as 1"
#   else
#       undef wxUSE_DYNAMIC_CLASSES
#       define wxUSE_DYNAMIC_CLASSES 1
#   endif
#endif /* wxUSE_DYNAMIC_CLASSES */

#if wxUSE_CLIPBOARD && !wxUSE_DATAOBJ
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxClipboard requires wxDataObject"
#   else
#       undef wxUSE_DATAOBJ
#       define wxUSE_DATAOBJ 1
#   endif
#endif /* wxUSE_CLIPBOARD */

#if wxUSE_WX_RESOURCES && !wxUSE_PROLOGIO
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxr resources require PrologIO"
#   else
#       undef wxUSE_PROLOGIO
#       define wxUSE_PROLOGIO 1
#   endif
#endif /* wxUSE_WX_RESOURCES */

#endif /* wxUSE_GUI */

