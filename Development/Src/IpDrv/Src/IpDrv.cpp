/*=============================================================================
	IpDrv.cpp: Unreal TCP/IP driver definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnIpDrv.h"
#include "HTTPDownload.h"

#if WITH_UE3_NETWORKING

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

#define STATIC_LINKING_MOJO 1

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) FName IPDRV_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "IpDrvClasses.h"
#undef AUTOGENERATE_NAME
#include "IpDrvUIPrivateClasses.h"

#undef AUTOGENERATE_FUNCTION
#undef NAMES_ONLY

// Import natives
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "IpDrvClasses.h"
#include "IpDrvUIPrivateClasses.h"
#undef AUTOGENERATE_NAME

#undef AUTOGENERATE_FUNCTION
#undef NATIVES_ONLY
#undef NAMES_ONLY

#if CHECK_NATIVE_CLASS_SIZES
#if _MSC_VER
#pragma optimize( "", off )
#endif

void AutoCheckNativeClassSizesIpDrv( UBOOL& Mismatch )
{
#define NAMES_ONLY
#define AUTOGENERATE_NAME( name )
#define AUTOGENERATE_FUNCTION( cls, idx, name )
#define VERIFY_CLASS_SIZES
#include "IpDrvClasses.h"
#include "IpDrvUIPrivateClasses.h"
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
void AutoInitializeRegistrantsIpDrv( INT& Lookup )
{
	AUTO_INITIALIZE_REGISTRANTS_IPDRV
	AUTO_INITIALIZE_REGISTRANTS_IPDRV_UIPRIVATE
}

/**
 * Auto generates names.
 */
void AutoGenerateNamesIpDrv()
{
	#define NAMES_ONLY
	#define AUTOGENERATE_NAME(name) IPDRV_##name=FName(TEXT(#name));
		#include "IpDrvNames.h"
	#undef AUTOGENERATE_NAME

	#define AUTOGENERATE_FUNCTION(cls,idx,name)
		#include "IpDrvClasses.h"
		#include "IpDrvUIPrivateClasses.h"
	#undef AUTOGENERATE_FUNCTION
	#undef NAMES_ONLY
}

DECLARE_STAT_NOTIFY_PROVIDER_FACTORY(FStatNotifyProvider_UDPFactory,
	FStatNotifyProvider_UDP,UdpProvider);



/*-----------------------------------------------------------------------------
	Global variables (maybe move this to DebugCommunication.cpp)
-----------------------------------------------------------------------------*/

#if !SHIPPING_PC_GAME
/** Network communication between UE3 executable and the "Unreal Console" tool.								*/
FDebugServer*	GDebugChannel						= NULL;
#endif

#endif	//#if WITH_UE3_NETWORKING
