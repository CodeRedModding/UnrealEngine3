/*=============================================================================
ForceFunctionSample.h
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef FORCE_FUNCTION_SAMPLE_H
#define FORCE_FUNCTION_SAMPLE_H

#if WITH_NOVODEX
#include "NxForceFieldKernelDefs.h"

NX_START_FORCEFIELD(Sample)

/* kernel constants: */

NxFConst(TornadoHeight);                       // height of tornado. Must be > Epsilon
NxFConst(RecipTornadoHeight);                  // 1 / tornado height
NxFConst(BaseRadius);                          // base radius of tornado
NxFConst(RadiusDelta);                         // max radius - base radius
NxFConst(EyeRadius);                           // radius inside which no radial/tangential force is applied

// lift parameters
NxFConst(LiftFalloffHeight);                   // fraction of height at which falloff starts. Should be <1
NxFConst(RecipOneMinusLiftFalloffHeight);      // precomputed constant
NxFConst(LiftStrength);                        // scale factor for lift

// rotational force strength
NxFConst(RotationalStrength);                  // scale factor for tangential force

// radial force parameters
NxFConst(RadialStrength);                      // scale factor for radial force
NxBConst(UseSpecialRadialForce);               // SRF is inward radial force applied if vel is inwards and low
NxFConst(EscapeVelocitySq);                    // SRF only if velocity^2 is less
NxFConst(MinOutwardVelocity);                  // SRF only if radial velocity is less

// force field kernel function: 
// implicit parameters: 
//    Position - vector constant
//    Velocity - vector constant
//    force    - vector assignable
//    torque   - vector assignable

NX_START_FUNCTION  

// problem 1 : Position.getX() and Position.getY() can not be directly used.
NxVector pos = Position;
NxFloat radius = pos.getX();
NxFloat height = pos.getY();

//problem 2 :NxFailIf(height < 0.0f || height > TornadoHeight);// can not be directly used.
NxFailIf((height < 0.0f) | (height > TornadoHeight));

NxFloat bodyHeightRatio = height * RecipTornadoHeight;
NxFloat radiusAtBodyHeight = BaseRadius + RadiusDelta * bodyHeightRatio;

NxFailIf(radius > radiusAtBodyHeight);

// lift force

NxFloat liftFalloffRatio = NxSelect(
								bodyHeightRatio > LiftFalloffHeight,  
								1.0f,  
								(1.0f - bodyHeightRatio) * RecipOneMinusLiftFalloffHeight);

NxFloat liftForce = LiftStrength * liftFalloffRatio;
// problem 3 : force = NxVector(0, liftForce, 0) // can not be directly used.   
force.setX(0);
force.setY(liftForce);
force.setZ(0);

// only do radial and tangential forces if outside the eye

NxFinishIf(radius < EyeRadius);

NxFloat factor = radius * radiusAtBodyHeight.recip();
NxFloat tangentForce = (1.0f - factor) * RotationalStrength;

// radial force - either SRF or outward force

// find same problems as 1 and 2.
//NxFloat srfFactor = NxSelect(
//							 Velocity.getX() < MinOutwardVelocity && Velocity.dot(Velocity) < EscapeVelocitySq, 
//							 factor, 
//							 0.0f);
NxVector vel = Velocity;
NxFloat srfFactor = NxSelect(
						 (vel.getX() < MinOutwardVelocity) & (vel.dot(vel) < EscapeVelocitySq), 
						 factor, 
						 0.0f);

NxFloat radialForce = RadialStrength * NxSelect(UseSpecialRadialForce, srfFactor, 1.0f - factor);

// find same problem as 3
//force = vector(radialForce, liftForce, tangentForce);
force.setX(radialForce);
force.setY(liftForce);
force.setZ(tangentForce);

NX_END_FUNCTION  


NX_END_FORCEFIELD(Sample)

#endif // WITH_NOVODEX

#endif //FORCE_FUNCTION_SAMPLE_H
