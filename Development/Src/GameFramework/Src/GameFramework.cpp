/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "GameFramework.h"
#include "EngineAnimClasses.h"
#include "EngineDecalClasses.h"
#include "EngineParticleClasses.h"

#include "GameFrameworkClasses.h"
#include "GameFrameworkAnimClasses.h"
#include "GameFrameworkCameraClasses.h"
#include "GameFrameworkSpecialMovesClasses.h"
#include "GameFrameworkGameStatsClasses.h"

#define STATIC_LINKING_MOJO 1

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) FName GAMEFRAMEWORK_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "GameFrameworkClasses.h"
#undef AUTOGENERATE_NAME

#include "GameFrameworkAnimClasses.h"
#include "GameFrameworkCameraClasses.h"
#include "GameFrameworkSpecialMovesClasses.h"
#include "GameFrameworkGameStatsClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef NAMES_ONLY

// Register natives.
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "GameFrameworkClasses.h"
#undef AUTOGENERATE_NAME

#include "GameFrameworkAnimClasses.h"
#include "GameFrameworkCameraClasses.h"
#include "GameFrameworkSpecialMovesClasses.h"
#include "GameFrameworkGameStatsClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef NATIVES_ONLY
#undef NAMES_ONLY

#if CHECK_NATIVE_CLASS_SIZES
#if _MSC_VER
#pragma optimize( "", off )
#endif

void AutoCheckNativeClassSizesGameFramework( UBOOL& Mismatch )
{
#define NAMES_ONLY
#define AUTOGENERATE_NAME( name )
#define AUTOGENERATE_FUNCTION( cls, idx, name )
#define VERIFY_CLASS_SIZES
#include "GameFrameworkClasses.h"
#include "GameFrameworkAnimClasses.h"
#include "GameFrameworkCameraClasses.h"
#include "GameFrameworkSpecialMovesClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef VERIFY_CLASS_SIZES
#undef NAMES_ONLY
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
void AutoInitializeRegistrantsGameFramework( INT& Lookup )
{
	AUTO_INITIALIZE_REGISTRANTS_GAMEFRAMEWORK
	AUTO_INITIALIZE_REGISTRANTS_GAMEFRAMEWORK_ANIM
	AUTO_INITIALIZE_REGISTRANTS_GAMEFRAMEWORK_CAMERA
	AUTO_INITIALIZE_REGISTRANTS_GAMEFRAMEWORK_SPECIALMOVES
	AUTO_INITIALIZE_REGISTRANTS_GAMEFRAMEWORK_GAMESTATS
}

/**
 * Auto generates names.
 */
void AutoGenerateNamesGameFramework()
{
	#define NAMES_ONLY
    #define AUTOGENERATE_NAME(name) GAMEFRAMEWORK_##name = FName(TEXT(#name));
		#include "GameFrameworkNames.h"
	#undef AUTOGENERATE_NAME

	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#include "GameFrameworkClasses.h"
	#include "GameFrameworkAnimClasses.h"
	#include "GameFrameworkCameraClasses.h"
	#include "GameFrameworkSpecialMovesClasses.h"
	#include "GameFrameworkGameStatsClasses.h"
	#undef AUTOGENERATE_FUNCTION
	#undef NAMES_ONLY
}


IMPLEMENT_CLASS(UGameTypes);
IMPLEMENT_CLASS(AFrameworkGame);
IMPLEMENT_CLASS(UGameCheatManager);
