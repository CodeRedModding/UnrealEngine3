//! @file SubstanceAirEdGraphInstanceEditorWindowShared.h
//! @brief Substance Air Graph Instance editor declaration
//! @contact antoine.gonzalez@allegorithmic.com
//! @copyright Allegorithmic. All rights reserved.

#ifndef __SubstanceAirGraphInstanceEditorWindowShared_h__
#define __SubstanceAirGraphInstanceEditorWindowShared_h__

#ifdef _MSC_VER
	#pragma once
#endif

// NOTE: This file is included as MANAGED for CLR .cpp files, but UNMANAGED for the rest of the app

#include "InteropShared.h"


#ifdef __cplusplus_cli

// ... MANAGED ONLY definitions go here ...

ref class MGraphInstanceEditorWindow;
ref class MWPFFrame;

#include "UnrealEdCLR.h"

#include "ManagedCodeSupportCLR.h"
#include "MeshPaintWindowShared.h"

#else // #ifdef __cplusplus_cli

#include "Dialogs.h"

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirTextureClasses.h"

#endif // #else

namespace SubstanceAir
{
struct FGraphInstance;
}


/**
 * Graph Instance Editor window class (shared between native and managed code)
 */
class FGraphInstanceEditorWindow
	: public FCallbackEventDevice
{

public:

	/** Static: Allocate and initialize mesh paint window */
	static FGraphInstanceEditorWindow* CreateGraphInstanceEditorWindow(
		SubstanceAir::FGraphInstance* InGraphInstance,
		const HWND InParentWindowHandle );

	/** Constructor */
	FGraphInstanceEditorWindow();

	/** Destructor */
	virtual ~FGraphInstanceEditorWindow();

	/** Initialize the mesh paint window */
	UBOOL InitGraphInstanceEditorWindow(
		SubstanceAir::FGraphInstance* InGraphInstance,
		const HWND InParentWindowHandle );

	/** Refresh all properties */
	void RefreshAllProperties();

	/** Saves window settings to the Graph Instance Editor settings structure */
	void SaveWindowSettings();

	/** Returns true if the mouse cursor is over the GraphInstanceEditor window */
	UBOOL IsMouseOverWindow();

	void Send(ECallbackEventType Event);
	void Send(ECallbackEventType InType, UObject* InObject);
	void Send(ECallbackEventType InType, const FString& InString, UObject* InObject);

	static SubstanceAir::FGraphInstance* GetEditedInstance();
	static SubstanceAir::FGraphInstance* GraphInstance;

protected:

	FPickColorStruct PickColorStruct;
	FLinearColor PickColorData;

	/** Managed reference to the actual window control */
	AutoGCRoot( MWPFFrame^ ) GraphInstanceEditorFrame;
	AutoGCRoot( MGraphInstanceEditorWindow^ ) GraphInstanceEditorPanel;
};

#endif	// __SubstanceAirGraphInstanceEditorWindowShared_h__
