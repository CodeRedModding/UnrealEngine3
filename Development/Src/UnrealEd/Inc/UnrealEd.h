/*=============================================================================
	UnrealEd.h: UnrealEd public header file.
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

	* Created by Jack Porter
=============================================================================*/

#ifndef _INC_UNREALED
#define _INC_UNREALED

#ifdef _INC_CORE
#error UnrealEd.h needs to be the first include to ensure the order of wxWindows/ Windows header inclusion
#endif

#define ADDEVENTHANDLER( id, event, func )\
{\
	wxEvtHandler* eh = GetEventHandler();\
	eh->Connect( id, event, (wxObjectEventFunction)(wxEventFunction)(wxCommandEventFunction)func );\
}

class WxExpressionBase;

/**
 * According to the following blog HINSTANCE == HMODULE so we'll just reuse the global
 * hInstance pointer.
 * 
 * http://blogs.msdn.com/oldnewthing/archive/2004/06/14/155107.aspx
 */
#define GetUnrealEdModuleHandle() (hInstance)

/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/

#define LEFT_BUTTON_BAR_SZ			(WxButtonGroup::BUTTON_SZ*2)

/*-----------------------------------------------------------------------------
	Option proxies.
-----------------------------------------------------------------------------*/

enum
{
	OPTIONS_2DSHAPERSHEET,
	OPTIONS_2DSHAPEREXTRUDE,
	OPTIONS_2DSHAPEREXTRUDETOPOINT,
	OPTIONS_2DSHAPEREXTRUDETOBEVEL,
	OPTIONS_2DSHAPERREVOLVE,
	OPTIONS_2DSHAPERBEZIERDETAIL,
	OPTIONS_DUPOBJECT,
	OPTIONS_NEWCLASSFROMSEL,
};

#define CREATE_OPTION_PROXY( ID, Class )\
{\
	GApp->EditorFrame->OptionProxies->Set( ID,\
		CastChecked<UOptionsProxy>(GUnrealEd->StaticConstructObject(Class::StaticClass(),GUnrealEd->Level->GetOuter(),NAME_None,RF_Public|RF_Standalone) ) );\
	UOptionsProxy* Proxy = *GApp->EditorFrame->OptionProxies->Find( ID );\
	check(Proxy);\
	Proxy->InitFields();\
}

// Engine
#include "XWindow.h"
#include "Window.h"
#include "..\..\Editor\Src\EditorPrivate.h"
#include "UnInterpolationHitProxy.h"

struct FGenericBrowserTypeInfo
{
	UClass*	Class;
	FColor	BorderColor;
	wxMenu*	ContextMenu;

	FGenericBrowserTypeInfo( UClass* InClass, FColor& InBorderColor, wxMenu* InContextMenu )
	{
		Class = InClass;
		BorderColor = InBorderColor;
		ContextMenu = InContextMenu;
	}

	inline UBOOL operator==( const FGenericBrowserTypeInfo& Other )
	{
		return ( Class == Other.Class );
	}
};

#include "UnrealEdClasses.h"

#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>

enum {
	GI_NUM_SELECTED			= 1,
	GI_CLASSNAME_SELECTED	= 2,
	GI_NUM_SURF_SELECTED	= 4,
	GI_CLASS_SELECTED		= 8
};

typedef struct
{
	INT iValue;
	FString String;
	UClass*	pClass;
} FGetInfoRet;

// Editor hit proxies

struct HMaterial: HHitProxy
{
	DECLARE_HIT_PROXY(HMaterial,HHitProxy);

	UMaterial*	Material;

	HMaterial(UMaterial* InMaterial): Material(InMaterial) {}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << *(UObject**)&Material;
	}
};

struct HMaterialExpression : HHitProxy
{
	DECLARE_HIT_PROXY(HMaterialExpression,HHitProxy);

	UMaterialExpression*	Expression;

	HMaterialExpression( UMaterialExpression* InExpression ): Expression(InExpression) {}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Expression;
	}
};

struct HInputHandle : HHitProxy
{
	DECLARE_HIT_PROXY(HInputHandle,HHitProxy);

	FExpressionInput*	Input;

	HInputHandle(FExpressionInput* InInput): Input(InInput) {}
};

struct HOutputHandle : HHitProxy
{
	DECLARE_HIT_PROXY(HOutputHandle,HHitProxy);

	UMaterialExpression*	Expression;
	FExpressionOutput		Output;

	HOutputHandle(UMaterialExpression* InExpression,const FExpressionOutput& InOutput):
		Expression(InExpression),
		Output(InOutput)
	{}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Expression;
	}
};

struct HDeleteExpression : HHitProxy
{
	DECLARE_HIT_PROXY(HDeleteExpression,HHitProxy);

	UMaterialExpression*	Expression;

	HDeleteExpression(UMaterialExpression* InExpression):
		Expression(InExpression)
	{}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Expression;
	}
};

// unrealed stuff

#include "..\..\core\inc\unmsg.h"
#include "..\src\GeomFitUtils.h"

extern class UUnrealEdEngine* GUnrealEd;

#include "GenericDlgOptions.h"
#include "UnTexAlignTools.h"
#include "UnrealEdMisc.h"
#include "Browser.h"
#include "TwoDeeShapeEditor.h"
#include "DlgLoadErrors.h"
#include "DlgSearchActors.h"
#include "DlgMapCheck.h"
#include "BrowserGroup.h"
#include "CurveEd.h"
#include "MaterialEditor.h"
#include "PhAT.h"
#include "BuildPropSheet.h"
#include "new\Docking.h"
#include "new\Windows.h"
#include "ButtonBar.h"
#include "new\WEditorFrame.h"
#include "new\Utils.h"
#include "new\ToolBars.h"
#include "new\Modes.h"
#include "UnEdModeInterpolation.h"
#include "new\Browsers.h"
#include "new\UnrealEdEngine.h"
#include "new\FileHelpers.h"
#include "new\Dialogs.h"
#include "new\Menus.h"
#include "new\Viewports.h"
#include "new\StatusBars.h"
#include "new\FFeedbackContextEditor.h"
#include "new\FCallbackDeviceEditor.h"
#include "new\EditorFrame.h"
#include "..\..\Launch\Inc\LaunchApp.h"
#include "new\UnrealEdApp.h"
#include "Cascade.h"
#include "new\Wizards.h"
#include "StaticMeshEditor.h"

extern WxUnrealEdApp* GApp;

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

// Browsers
extern ULevel* GPrefabLevel;		// A temporary level we assign to the prefab viewport, where we hold the prefabs actors for viewing.
extern FTexAlignTools GTexAlignTools;

extern FRebuildTools GRebuildTools;

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
#endif
