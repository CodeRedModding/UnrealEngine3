//! @file SubstanceAirEd.cpp
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include <UnrealEd.h>
#include <Factories.h>

#include "SubstanceAirEdBrowserClasses.h"
#include "SubstanceAirEdCommandlet.h"
#include "SubstanceAirEdFactoryClasses.h"
#include "SubstanceAirEdThumbnailRendererClasses.h"

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirInstanceFactoryClasses.h"


/*-----------------------------------------------------------------------------
	The following must be done once per package.
-----------------------------------------------------------------------------*/

#define STATIC_LINKING_MOJO 1

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) FName SUBSTANCE_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "SubstanceAirEdBrowserClasses.h"
#include "SubstanceAirEdFactoryClasses.h"
#include "SubstanceAirEdThumbnailRendererClasses.h"
#undef AUTOGENERATE_NAME

#undef AUTOGENERATE_FUNCTION
#undef NAMES_ONLY

// Register natives.
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "SubstanceAirEdBrowserClasses.h"
#include "SubstanceAirEdFactoryClasses.h"
#include "SubstanceAirEdThumbnailRendererClasses.h"
#undef AUTOGENERATE_NAME

#undef AUTOGENERATE_FUNCTION
#undef NATIVES_ONLY
#undef NAMES_ONLY

/**
 * Initialize registrants, basically calling StaticClass() to create the class and also 
 * populating the lookup table.
 *
 * @param	Lookup	current index into lookup table
 */
void AutoInitializeRegistrantsSubstanceAirEd( INT& Lookup )
{
	AUTO_INITIALIZE_REGISTRANTS_SUBSTANCEAIRED_BROWSER;
	AUTO_INITIALIZE_REGISTRANTS_SUBSTANCEAIRED_FACTORIES;
	AUTO_INITIALIZE_REGISTRANTS_SUBSTANCEAIRED_COMMANDLET;
	AUTO_INITIALIZE_REGISTRANTS_SUBSTANCEAIRED_THUMBNAILRENDERER;
}

/**
 * Auto generates names.
 */
void AutoGenerateNamesSubstanceAirEd()
{
	#define NAMES_ONLY
    #define AUTOGENERATE_NAME(name) SUBSTANCE_##name = FName(TEXT(#name));

	#undef AUTOGENERATE_NAME

	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#include "SubstanceAirEdBrowserClasses.h"
	#include "SubstanceAirEdFactoryClasses.h"
	#include "SubstanceAirEdThumbnailRendererClasses.h"
	#undef AUTOGENERATE_FUNCTION
	#undef NAMES_ONLY
}
