/**********************************************************************

Filename    :   GFxUI.cpp
Content     :   GFx UE name registartion / static linking file

Copyright   :   (c) 2001-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/

#include "GFxUI.h"

#include "GFxUIClasses.h"
#include "EngineSequenceClasses.h"
#include "GFxUIUISequenceClasses.h"
#include "EngineUIPrivateClasses.h"
#include "GFxUIUIPrivateClasses.h"

// Declaration used for force linking of GFxStats.cpp.
extern void GFX_InitUnStats();

#define STATIC_LINKING_MOJO 1

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) FName GFXUI_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "GFxUIClasses.h"
//#if (VER_LATEST_ENGINE >= 583)
#undef AUTOGENERATE_NAME
//#endif
#include "GFxUIUISequenceClasses.h"
#include "GFxUIUIPrivateClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

// Register natives.
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "GFxUIClasses.h"
#include "GFxUIUISequenceClasses.h"
#include "GFxUIUIPrivateClasses.h"
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
void AutoInitializeRegistrantsGFxUI ( INT& Lookup )
{
	AUTO_INITIALIZE_REGISTRANTS_GFXUI;
	AUTO_INITIALIZE_REGISTRANTS_GFXUI_UISEQUENCE;

#if WITH_GFx
	// Force linker to include GFx constructors by calling this function externally.
	GFX_InitUnStats();
#endif // WITH_GFx
}

/**
 * Auto generates names.
 */
void AutoGenerateNamesGFxUI()
{
#define NAMES_ONLY
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#define AUTOGENERATE_NAME(name) GFXUI_##name = FName(TEXT(#name));
#include "GFxUIClasses.h"
#include "GFxUIUISequenceClasses.h"
#include "GFxUIUIPrivateClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY
}


#if CHECK_NATIVE_CLASS_SIZES

void AutoCheckNativeClassSizesGFxUI ( UBOOL& Mismatch )
{
#define NAMES_ONLY
#define AUTOGENERATE_NAME( name )
#define AUTOGENERATE_FUNCTION( cls, idx, name )
#define VERIFY_CLASS_SIZES
#include "GFxUIClasses.h"
#include "GFxUIUISequenceClasses.h"
#include "GFxUIUIPrivateClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY
#undef VERIFY_CLASS_SIZES
}

#endif //CHECK_NATIVE_CLASS_SIZES
