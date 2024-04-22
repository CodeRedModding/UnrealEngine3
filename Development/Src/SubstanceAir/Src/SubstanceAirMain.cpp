//! @file SubstanceAirMain.cpp
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include <Engine.h>
#include <EngineSequenceClasses.h>
#include <EngineInterpolationClasses.h>

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirPackage.h"
#include "SubstanceAirInstanceFactoryClasses.h"
#include "SubstanceAirTextureClasses.h"
#include "SubstanceAirImageInputClasses.h"
#include "SubstanceAirActorClasses.h"
#include "SubstanceAirSequenceClasses.h"
#include "SubstanceAirInterpolationClasses.h"

/*-----------------------------------------------------------------------------
	The following must be done once per package.
-----------------------------------------------------------------------------*/

#define STATIC_LINKING_MOJO 1

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) FName SUBSTANCE_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)

#undef AUTOGENERATE_NAME

#include "SubstanceAirTextureClasses.h"
#include "SubstanceAirInstanceFactoryClasses.h"
#include "SubstanceAirImageInputClasses.h"
#include "SubstanceAirActorClasses.h"
#include "SubstanceAirInterpolationClasses.h"
#include "SubstanceAirSequenceClasses.h"

#undef AUTOGENERATE_FUNCTION
#undef NAMES_ONLY

// Register natives.
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)

#undef AUTOGENERATE_NAME

#include "SubstanceAirTextureClasses.h"
#include "SubstanceAirInstanceFactoryClasses.h"
#include "SubstanceAirImageInputClasses.h"
#include "SubstanceAirActorClasses.h"
#include "SubstanceAirInterpolationClasses.h"
#include "SubstanceAirSequenceClasses.h"

#undef AUTOGENERATE_FUNCTION
#undef NATIVES_ONLY
#undef NAMES_ONLY

/**
 * Initialize registrants, basically calling StaticClass() to create the class and also 
 * populating the lookup table.
 *
 * @param	Lookup	current index into lookup table
 */
void AutoInitializeRegistrantsSubstanceAir( INT& Lookup )
{
	AUTO_INITIALIZE_REGISTRANTS_SUBSTANCEAIR_TEXTURE;
	AUTO_INITIALIZE_REGISTRANTS_SUBSTANCEAIR_INSTANCEFACTORY;
	AUTO_INITIALIZE_REGISTRANTS_SUBSTANCEAIR_IMAGEINPUT;
	AUTO_INITIALIZE_REGISTRANTS_SUBSTANCEAIR_INTERPOLATION;
	AUTO_INITIALIZE_REGISTRANTS_SUBSTANCEAIR_ACTOR;
	AUTO_INITIALIZE_REGISTRANTS_SUBSTANCEAIR_SEQUENCE;
}

/**
 * Auto generates names.
 */
void AutoGenerateNamesSubstanceAir()
{
	#define NAMES_ONLY
    #define AUTOGENERATE_NAME(name) SUBSTANCE_##name = FName(TEXT(#name));
	#include "SubstanceAirNames.h"
	#undef AUTOGENERATE_NAME
	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#include "SubstanceAirTextureClasses.h"
	#include "SubstanceAirInstanceFactoryClasses.h"
	#include "SubstanceAirImageInputClasses.h"
	#include "SubstanceAirActorClasses.h"
	#include "SubstanceAirInterpolationClasses.h"
	#include "SubstanceAirSequenceClasses.h"
	#undef AUTOGENERATE_FUNCTION
	#undef NAMES_ONLY
}
