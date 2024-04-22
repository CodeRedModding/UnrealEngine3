/*=============================================================================
	ForceFieldShapeCapsule.cpp
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineForceFieldClasses.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

IMPLEMENT_CLASS(UForceFieldShapeCapsule);

#if WITH_NOVODEX
NxForceFieldShapeDesc* UForceFieldShapeCapsule::CreateNxDesc()
{
	NxCapsuleForceFieldShapeDesc* ffShapeDesc = new NxCapsuleForceFieldShapeDesc;
	ffShapeDesc->radius = U2PScale*eventGetRadius();
	ffShapeDesc->height = U2PScale*eventGetHeight();
	// make the PhysX capsule z upwards.
	// UE render capsule is x upward by default. But for bounding alignment with cylinder, ForceFieldShapeCapsule is rotated to be z upward.
	ffShapeDesc->pose.M.rotX(-NxPi/2);

	return ffShapeDesc;
}
#endif

