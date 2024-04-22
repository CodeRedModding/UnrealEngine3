/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemGameSpy.h"

#if WITH_GAMESPY

#define STATIC_LINKING_MOJO 1

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) FName ONLINESUBSYSTEMGAMESPY_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "OnlineSubsystemGameSpyClasses.h"
#undef AUTOGENERATE_NAME

#undef AUTOGENERATE_FUNCTION
#undef NAMES_ONLY

// Register natives.
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "OnlineSubsystemGameSpyClasses.h"
#undef AUTOGENERATE_NAME

#undef AUTOGENERATE_FUNCTION
#undef NATIVES_ONLY
#undef NAMES_ONLY

IMPLEMENT_CLASS(UOnlineSubsystemGameSpy);
IMPLEMENT_CLASS(UOnlineGameInterfaceGameSpy);

#endif


/**
 * Initialize registrants, basically calling StaticClass() to create the class and also 
 * populating the lookup table.
 *
 * @param	Lookup	current index into lookup table
 */
void AutoInitializeRegistrantsOnlineSubsystemGameSpy( INT& Lookup )
{
#if WITH_GAMESPY
	AUTO_INITIALIZE_REGISTRANTS_ONLINESUBSYSTEMGAMESPY;
#endif
}

/**
 * Auto generates names.
 */
void AutoGenerateNamesOnlineSubsystemGameSpy()
{
#if WITH_GAMESPY
	#define NAMES_ONLY
    #define AUTOGENERATE_NAME(name) ONLINESUBSYSTEMGAMESPY_##name = FName(TEXT(#name));
		#include "OnlineSubsystemGameSpyNames.h"
	#undef AUTOGENERATE_NAME

	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#include "OnlineSubsystemGameSpyClasses.h"
	#undef AUTOGENERATE_FUNCTION
	#undef NAMES_ONLY
#endif
}

