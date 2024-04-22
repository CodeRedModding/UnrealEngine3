/*=============================================================================
	WinDrv.cpp: Unreal Windows viewport and platform driver.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "WinDrv.h"

#if WITH_UE3_NETWORKING
#include "UnIpDrv.h"
#include "EnginePlatformInterfaceClasses.h"
#include "WinDrvClasses.h"

#define STATIC_LINKING_MOJO 1

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) FName WINDRV_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "WinDrvClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

// Register natives.
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "WinDrvClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NATIVES_ONLY
#undef NAMES_ONLY

/**
 * Initialize registrants, basically calling StaticClass() to create the class and also 
 * populating the lookup table.
 *
 * @param	Lookup	current index into lookup table
 */
void AutoInitializeRegistrantsWinDrv( INT& Lookup )
{
	AUTO_INITIALIZE_REGISTRANTS_WINDRV;
}

/**
 * Auto generates names.
 */
void AutoGenerateNamesWinDrv()
{
	#define NAMES_ONLY
	#define AUTOGENERATE_NAME(name) WINDRV_##name = FName(TEXT(#name));
		#include "WinDrvNames.h"
	#undef AUTOGENERATE_NAME

	#define AUTOGENERATE_FUNCTION(cls,idx,name)
		#include "WinDrvClasses.h"
	#undef AUTOGENERATE_FUNCTION
	
	#undef NAMES_ONLY
}

#if CHECK_NATIVE_CLASS_SIZES
#if _MSC_VER
#pragma optimize( "", off )
#endif

void AutoCheckNativeClassSizesWinDrv( UBOOL& Mismatch )
{
#define NAMES_ONLY
#define AUTOGENERATE_NAME( name )
#define AUTOGENERATE_FUNCTION( cls, idx, name )
#define VERIFY_CLASS_SIZES
#include "WinDrvClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY
#undef VERIFY_CLASS_SIZES
}

#if _MSC_VER
#pragma optimize( "", on )
#endif
#endif

#endif	//#if WITH_UE3_NETWORKING
