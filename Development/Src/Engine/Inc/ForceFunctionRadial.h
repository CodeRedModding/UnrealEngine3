/*=============================================================================
ForceFunctionRadial.h
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef FORCE_FUNCTION_RADIAL_H
#define FORCE_FUNCTION_RADIAL_H

#if WITH_NOVODEX
#include "NxForceFieldKernelDefs.h"

NX_START_FORCEFIELD(Radial)

/* kernel constants: */
NxFConst(RadialStrength);
NxFConst(Radius);		
NxFConst(SelfRotationStrength);
NxFConst(RadiusRecip);

NxBConst(BLinearFalloff);

// force field kernel function: 
// implicit parameters: 
//    Position - vector constant
//    Velocity - vector constant
//    force    - vector assignable
//    torque   - vector assignable

NX_START_FUNCTION  

NxFloat r = Position.magnitude();

NxFailIf(r > Radius);

NxFloat factor = NxSelect(BLinearFalloff, 1 - r*RadiusRecip, 1);

force.setX(factor*RadialStrength);
force.setY(0);
force.setZ(0);

torque.setX(factor*SelfRotationStrength*Velocity.getX());
torque.setY(0);
torque.setZ(0);

NX_END_FUNCTION  


NX_END_FORCEFIELD(Radial)

#endif // WITH_NOVODEX

#endif //FORCE_FUNCTION_RADIAL_H
