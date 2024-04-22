/*=============================================================================
UnPhysicalMaterial.h: Helper functions for PhysicalMaterial 
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// NOTE: move the PhysicalMaterial classes into here

#ifndef _UN_PHYSICAL_MATERIAL_H_
#define _UN_PHYSICAL_MATERIAL_H_

#include "EnginePhysicsClasses.h"

/**
 * This is a helper function which will set the PhysicalMaterial based on
 * whether or not we have a PhysMaterial set from a Skeletal Mesh or if we
 * are getting it from the Environment.
 *
 **/
UPhysicalMaterial* DetermineCorrectPhysicalMaterial( const FCheckResult& HitData );


#endif // _UN_PHYSICAL_MATERIAL_H_


