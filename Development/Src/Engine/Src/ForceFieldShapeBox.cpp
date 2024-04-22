/*=============================================================================
	ForceFieldShapeBox.cpp
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineForceFieldClasses.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

IMPLEMENT_CLASS(UForceFieldShapeBox);

#if WITH_NOVODEX
NxForceFieldShapeDesc* UForceFieldShapeBox::CreateNxDesc()
{
	NxBoxForceFieldShapeDesc * ret = new NxBoxForceFieldShapeDesc;
	ret->dimensions = U2NPosition(eventGetRadii());
	
	return ret;
}
#endif

