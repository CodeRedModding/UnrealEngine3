/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemSteamworks.h"

#if WITH_UE3_NETWORKING && WITH_STEAMWORKS

// Generate declarations for names and native script functions of this package
#define STATIC_LINKING_MOJO 1
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) FName ONLINESUBSYSTEMSTEAMWORKS_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "OnlineSubsystemSteamworksClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

// Generate lookup table for the native script functions of this package
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "OnlineSubsystemSteamworksClasses.h"
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
void AutoInitializeRegistrantsOnlineSubsystemSteamworks( INT& Lookup )
{
	AUTO_INITIALIZE_REGISTRANTS_ONLINESUBSYSTEMSTEAMWORKS;
	extern void RegisterSteamworksNetClasses(void);
	RegisterSteamworksNetClasses();
}

/**
 * Auto generates names.
 */
void AutoGenerateNamesOnlineSubsystemSteamworks()
{
	#define NAMES_ONLY
	#define AUTOGENERATE_NAME(name) ONLINESUBSYSTEMSTEAMWORKS_##name = FName(TEXT(#name));
	#include "OnlineSubsystemSteamworksNames.h"
	#undef AUTOGENERATE_NAME

	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#include "OnlineSubsystemSteamworksClasses.h"
	#undef AUTOGENERATE_FUNCTION
	#undef NAMES_ONLY
}

IMPLEMENT_CLASS(UOnlineSubsystemSteamworks);
IMPLEMENT_CLASS(UOnlineAuthInterfaceSteamworks);
IMPLEMENT_CLASS(UOnlineGameInterfaceSteamworks);
IMPLEMENT_CLASS(UOnlineLobbyInterfaceSteamworks);

#else	// #if WITH_UE3_NETWORKING && WITH_STEAMWORKS

/**
 * Initialize registrants, basically calling StaticClass() to create the class and also 
 * populating the lookup table.
 *
 * Stub for linkage (referenced in LaunchEngineLoop.cpp as extern)
 */
void AutoInitializeRegistrantsOnlineSubsystemSteamworks( INT& )
{
}

/**
 * Auto generates names.
 *
 * Stub for linkage (referenced in LaunchEngineLoop.cpp as extern)
 */
void AutoGenerateNamesOnlineSubsystemSteamworks()
{
}

#endif	// #if WITH_UE3_NETWORKING && WITH_STEAMWORKS

