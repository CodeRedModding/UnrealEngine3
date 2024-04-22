/*=============================================================================
ForceFunctionTornado.h
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef FORCE_FUNCTION_TORNADO_H
#define FORCE_FUNCTION_TORNADO_H

#if WITH_NOVODEX
#include "NxForceFieldKernelDefs.h"

NX_START_FORCEFIELD(Tornado)

/* kernel constants: */
NxFConst(TornadoHeight);//
NxFConst(Radius);
NxFConst(RadiusTop);
NxFConst(EscapeVelocitySq);

NxFConst(RotationalStrength);
NxFConst(RadialStrength);
NxBConst(BSpecialRadialForce);

NxFConst(LiftFallOffHeight); // // assuming bottom is 0 and top is TornadoHeight, in the range of [0, TornadoHeight]
NxFConst(LiftStrength);

// force field kernel function: 
// implicit parameters: 
//    Position - vector constant
//    Velocity - vector constant
//    force    - vector assignable
//    torque   - vector assignable

NX_START_FUNCTION  

NxFailIf(TornadoHeight < KINDA_SMALL_NUMBER);

NxVector pos = Position;
NxFloat singularExclude = NxSelect(pos.getX() > KINDA_SMALL_NUMBER, 1, 0);

NxFloat RadiusDelta = RadiusTop - Radius;
NxFloat th = TornadoHeight;
NxFloat RadiusAtBodyHeight = Radius + RadiusDelta*pos.getY()*th.recip();

NxFloat factor = pos.getX()*RadiusAtBodyHeight.recip();
// tangent strength
force.setZ((1 - factor)*RotationalStrength*singularExclude);

NxVector vel = Velocity;
NxFloat specialFactor = NxSelect(BSpecialRadialForce, 1, 0);
NxFloat specialSubFactor = NxSelect((vel.getX() > KINDA_SMALL_NUMBER) & (vel.dot(vel) < EscapeVelocitySq), 1, 0);

NxFloat xBranch0 = factor*RadialStrength;
NxFloat xBranch1 = (1-factor)*RadialStrength;
// radial strength
force.setX((xBranch0*specialFactor*specialSubFactor + xBranch1*(1-specialFactor))*singularExclude);

// up strength
NxFloat yBranch0 = 1 - (pos.getY() - LiftFallOffHeight)*(TornadoHeight - LiftFallOffHeight).recip();// assuming bottom is 0 and top is TornadoHeight
NxFloat l = NxSelect(pos.getY() > LiftFallOffHeight, yBranch0, 1) * LiftStrength;
force.setY(l);

NX_END_FUNCTION  


NX_END_FORCEFIELD(Tornado)

#endif // WITH_NOVODEX

#endif //FORCE_FUNCTION_TORNADO_H
