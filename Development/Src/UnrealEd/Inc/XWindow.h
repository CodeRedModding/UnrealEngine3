/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _XWINDOW_H_
#define _XWINDOW_H_

#pragma warning(disable : 4996) // 'function' was was declared deprecated

#define ADDEVENTHANDLER( id, event, func )\
{\
	wxEvtHandler* eh = GetEventHandler();\
	eh->Connect( id, event, (wxObjectEventFunction)(wxEventFunction)(wxCommandEventFunction)func );\
}

/*-----------------------------------------------------------------------------
	Defines.
-----------------------------------------------------------------------------*/

// Helpful values

#define STD_BORDER_SZ				2
#define STD_CONTROL_BUFFER			4
#define STD_CONTROL_SPACING			8
#define DOCK_GUTTER_SZ				8
#define DOCKING_DRAG_BAR_H			16
#define DEF_DOCKING_DEPTH			128
#define STD_DRAG_LIMIT				8
#define STD_SASH_SZ					4
#define STD_TNAIL_HIGHLIGHT_EDGE	4
#define STD_TNAIL_PAD				18
#define STD_TNAIL_PAD_HALF			(STD_TNAIL_PAD/2)
#define STD_SPLITTER_SZ				200
#define MOVEMENTSPEED_SLOW			4
#define MOVEMENTSPEED_NORMAL		12
#define MOVEMENTSPEED_FAST			32
#define MOVEMENTSPEED_VERYFAST		64
#define SCROLLBAR_SZ				17

/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/

#ifdef _INC_CORE
#error XWindow.h needs to be the first include to ensure the order of wxWindows/ Windows header inclusion
#endif


#include "UnBuild.h"

#ifdef _WINDOWS
#include "PreWindowsApi.h"
#ifndef STRICT
#define STRICT
#endif
#include "MinWindows.h"
#endif

#define wxUSE_GUI 1
#ifdef _DEBUG
#define __WXDEBUG__
#define WXDEBUG 1
#endif

// use wxWidgets as a DLL
#if defined(WXUSINGDLL) && !WXUSINGDLL
#undef WXUSINGDLL
#endif

#ifndef WXUSINGDLL
#define WXUSINGDLL 1
#endif

// this is needed in VC8 land to get the XP look and feel.  (c.f. http://www.wxwidgets.org/wiki/index.php/MSVC )
#if wxUSE_GUI
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='X86' publicKeyToken='6595b64144ccf1df'\"")
#endif // wxUSE_GUI

#include "wx/wxprec.h"
#ifndef WX_PRECOMP
	#include "wx/wx.h"
#endif
#include <wx/notebook.h>
#include <wx/treectrl.h>
#include <wx/splitter.h>
#include <wx/listbox.h>
#include <wx/listctrl.h>
#include <wx/xrc/xmlres.h>
#include <wx/spinbutt.h>
#include <wx/colordlg.h>
#include <wx/scrolbar.h>
#include <wx/scrolwin.h>
#include <wx/image.h>
#include <wx/gauge.h>
#include <wx/toolbar.h>
#include <wx/dialog.h>
#include <wx/statusbr.h>
#include <wx/valgen.h>
#include <wx/dnd.h>
#include <wx/wizard.h>
#include <wx/html/htmlwin.h>
#include <wx/splash.h>
#include <wx/imaglist.h>
#include <wx/tglbtn.h>
#include <wx/statline.h>
#include <wx/spinctrl.h>
#include <wx/grid.h>
#include <wx/dcbuffer.h>
#include <wx/tipdlg.h>
#include <wx/wfstream.h>
#include <wx/tooltip.h>
#include <wx/uri.h>


#include <wx/aui/aui.h>
#include <wx/richtext/richtextctrl.h>

#if WITH_FACEFX_STUDIO
#include <wx/fileconf.h>
#include <wx/confbase.h>
#include <wx/textfile.h>
#include <wx/choice.h>
#include <wx/popupwin.h>
#include <wx/combobox.h>
#include <wx/grid.h>
#include <wx/docview.h>
#include <wx/slider.h>
#include <wx/geometry.h>
#include <wx/string.h>
#include <wx/colour.h>
#include <wx/wxhtml.h>
#include <wx/valgen.h>
#include <wx/valtext.h>
#include <wx/tokenzr.h>
#include <wx/image.h>
#include <wx/dynlib.h>
#include <wx/accel.h>
#include <wx/file.h>
#include <wx/font.h>
#include <wx/fontdlg.h>
#include <wx/textdlg.h>
#include <wx/dir.h>
#include <wx/help.h>
#include <wx/minifram.h>
#include "../../../External/FaceFX/Studio/Main/Inc/FxWxWrappers.h"
#endif // WITH_FACEFX_STUDIO

#ifdef _WINDOWS
#include "PostWindowsApi.h"
#endif

// the following symbols are defined in wxWidgets so we need to undef them before we enter Unreal Land
#undef __WIN32__
#undef __INTEL__
#undef DECLARE_CLASS
#undef DECLARE_ABSTRACT_CLASS
#undef IMPLEMENT_CLASS

#endif
