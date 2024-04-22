/*=============================================================================
	ForceFieldShapeSphere.cpp
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineForceFieldClasses.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

IMPLEMENT_CLASS(UForceFieldShapeSphere);

#if WITH_NOVODEX
NxForceFieldShapeDesc* UForceFieldShapeSphere::CreateNxDesc()
{
	NxSphereForceFieldShapeDesc* ffShapeDesc = new NxSphereForceFieldShapeDesc;

	ffShapeDesc->radius = U2PScale * eventGetRadius();

	return ffShapeDesc;
}
#endif

