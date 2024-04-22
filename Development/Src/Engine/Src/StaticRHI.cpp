/*=============================================================================
	DynamicRHI.cpp: Dynamically bound Render Hardware Interface implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "RHI.h"

#if USE_STATIC_RHI

extern STATIC_RHI_CLASS_NAME* CreateStaticRHI();


// Globals.
STATIC_RHI_CLASS_NAME* GStaticRHI = NULL;


void RHIInit( UBOOL bIsEditor )
{
	if(!GStaticRHI)
	{		
		GStaticRHI = CreateStaticRHI();
	}
}

void RHIExit()
{
	// Destruct the static RHI.
	delete GStaticRHI;
	GStaticRHI = NULL;
}



#endif