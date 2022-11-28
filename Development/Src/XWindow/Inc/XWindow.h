/*=============================================================================
	XWindow.h: GUI window management code through wxWindows

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#ifndef _XWINDOW_H_
#define _XWINDOW_H_

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
#define MOVEMENTSPEED_SLOW			1
#define MOVEMENTSPEED_NORMAL		4
#define MOVEMENTSPEED_FAST			16
#define SCROLLBAR_SZ				17

enum EDockableWindowType
{
	DWT_UNUSED					= 0,
	DWT_BROWSER_START			= 1,
		DWT_GROUP_BROWSER		= 1,
		DWT_ACTOR_BROWSER		= 2,
		DWT_ANIMATION_BROWSER	= 3,
		DWT_LAYER_BROWSER		= 4,
	DWT_BROWSER_END             = 5,
	DWT_GENERIC_BROWSER			= 14,
};

/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/

#ifdef _INC_CORE
#error XWindow.h needs to be the first include to ensure the order of wxWindows/ Windows header inclusion
#endif

#pragma pack( push, 8 )
#ifndef STRICT
#define STRICT
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#define wxUSE_GUI 1
#ifdef _DEBUG
#define __WXDEBUG__
#define WXDEBUG 1
#endif

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
#pragma pack( pop )

#undef __WIN32__
#undef DECLARE_CLASS
#undef DECLARE_ABSTRACT_CLASS
#undef IMPLEMENT_CLASS

#include "Engine.h"

#include "ResourceIDs.h"

#include "Bitmaps.h"
#include "MRUList.h"
#include "Controls.h"
#include "Dialogs.h"
#include "Buttons.h"
#include "Utils.h"
#include "DragDrop.h"
#include "Properties.h"
#include "PropertyUtils.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

void XWindow_StartUp( wxApp* InApp );
INT XWindow_ShowErrorMessage( const FString& InText );
INT XWindow_AskQuestion( const FString& InText );

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
