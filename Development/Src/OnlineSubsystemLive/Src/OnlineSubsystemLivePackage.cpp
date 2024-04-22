/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemLive.h"

#if WITH_UE3_NETWORKING && (XBOX || WITH_PANORAMA)

// Generate declarations for names and native script functions of this package
#define STATIC_LINKING_MOJO 1
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) FName ONLINESUBSYSTEMLIVE_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "OnlineSubsystemLiveClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

// Generate lookup table for the native script functions of this package
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "OnlineSubsystemLiveClasses.h"
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
void AutoInitializeRegistrantsOnlineSubsystemLive( INT& Lookup )
{
	AUTO_INITIALIZE_REGISTRANTS_ONLINESUBSYSTEMLIVE;
	extern void RegisterLiveNetClasses(void);
	RegisterLiveNetClasses();
}

/**
 * Auto generates names.
 */
void AutoGenerateNamesOnlineSubsystemLive()
{
	#define NAMES_ONLY
    #define AUTOGENERATE_NAME(name) ONLINESUBSYSTEMLIVE_##name = FName(TEXT(#name));
		#include "OnlineSubsystemLiveNames.h"
	#undef AUTOGENERATE_NAME

	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#include "OnlineSubsystemLiveClasses.h"
	#undef AUTOGENERATE_FUNCTION
	#undef NAMES_ONLY
}

#else	// #if WITH_UE3_NETWORKING && (XBOX || WITH_PANORAMA)

/**
 * Initialize registrants, basically calling StaticClass() to create the class and also 
 * populating the lookup table.
 *
 * Stub for linkage (referenced in LaunchEngineLoop.cpp as extern)
 */
void AutoInitializeRegistrantsOnlineSubsystemLive( INT& )
{
}

/**
 * Auto generates names.
 *
 * Stub for linkage (referenced in LaunchEngineLoop.cpp as extern)
 */
void AutoGenerateNamesOnlineSubsystemLive()
{
}

#endif	// #if WITH_UE3_NETWORKING && (XBOX || WITH_PANORAMA)
