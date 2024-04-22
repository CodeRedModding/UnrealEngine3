/*=============================================================================
	UnrealEd.cpp: UnrealEd package file
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnrealEdPrivateClasses.h"
#include "StaticMeshEditor.h"
#include "PropertyUtils.h"
#include "PropertyWindowManager.h"
#include "GameFrameworkGameStatsClasses.h"
#include "UnrealEdGameStatsClasses.h"
#include "AssetSelection.h"
#include "Factories.h"
#include "UnEdTran.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

/**
 * Provides access to the FEditorModeTools singleton.
 */
class FEditorModeTools& GEditorModeTools()
{
	static FEditorModeTools* EditorModeToolsSingleton = new FEditorModeTools;
	return *EditorModeToolsSingleton;
}

FEditorLevelViewportClient* GCurrentLevelEditingViewportClient = NULL;
/** Tracks the last level editing viewport client that received a key press. */
FEditorLevelViewportClient* GLastKeyLevelEditingViewportClient = NULL;

/**
 * Returns the path to the engine's editor resources directory (e.g. "/../../Engine/Editor/")
 */
const FString GetEditorResourcesDir()
{
	return FString( appBaseDir() ) * appEngineDir() * FString( TEXT("EditorResources") PATH_SEPARATOR );
}

DEFINE_EVENT_TYPE(wxEVT_DOCKINGCHANGE)

#define STATIC_LINKING_MOJO 1

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) FName UNREALED_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "UnrealEdClasses.h"
#undef AUTOGENERATE_NAME

#include "UnrealEdCascadeClasses.h"
#include "UnrealEdGameStatsClasses.h"
#include "UnrealEdPrivateClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

// Register natives.
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "UnrealEdClasses.h"
#undef AUTOGENERATE_NAME

#include "UnrealEdCascadeClasses.h"
#include "UnrealEdGameStatsClasses.h"
#include "UnrealEdPrivateClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NATIVES_ONLY
#undef NAMES_ONLY

#if CHECK_NATIVE_CLASS_SIZES
#if _MSC_VER
#pragma optimize( "", off )
#endif

void AutoCheckNativeClassSizesUnrealEd( UBOOL& Mismatch )
{
#define NAMES_ONLY
#define AUTOGENERATE_NAME( name )
#define AUTOGENERATE_FUNCTION( cls, idx, name )
#define VERIFY_CLASS_SIZES
#include "UnrealEdClasses.h"
#include "UnrealEdCascadeClasses.h"
#include "UnrealEdPrivateClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY
#undef VERIFY_CLASS_SIZES
}

#if _MSC_VER
#pragma optimize( "", on )
#endif
#endif

/**
 * Initialize registrants, basically calling StaticClass() to create the class and also 
 * populating the lookup table.
 *
 * @param	Lookup	current index into lookup table
 */
void AutoInitializeRegistrantsUnrealEd( INT& Lookup )
{
	AUTO_INITIALIZE_REGISTRANTS_UNREALED
	AUTO_INITIALIZE_REGISTRANTS_UNREALED_CASCADE
	AUTO_INITIALIZE_REGISTRANTS_UNREALED_GAMESTATS
	AUTO_INITIALIZE_REGISTRANTS_UNREALED_PRIVATE
}

/**
 * Auto generates names.
 */
void AutoGenerateNamesUnrealEd()
{
	#define NAMES_ONLY
	#define AUTOGENERATE_NAME(name) UNREALED_##name=FName(TEXT(#name));
		#include "UnrealEdNames.h"
	#undef AUTOGENERATE_NAME

	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#include "UnrealEdClasses.h"
	#include "UnrealEdCascadeClasses.h"
	#include "UnrealEdGameStatsClasses.h"
	#include "UnrealEdPrivateClasses.h"
	#undef AUTOGENERATE_FUNCTION
	#undef NAMES_ONLY
}

/*-----------------------------------------------------------------------------
	UUnrealEdEngine.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UUnrealEdEngine);
/*-----------------------------------------------------------------------------
	UUnrealEdTypes.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UUnrealEdTypes);
