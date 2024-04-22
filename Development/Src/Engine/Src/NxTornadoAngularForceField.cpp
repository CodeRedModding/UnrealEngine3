/*=============================================================================
	NxTornadoAngularForceField.cpp: 
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineForceFieldClasses.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

#include "ForceFunctionTornadoAngular.h"

IMPLEMENT_CLASS(ANxTornadoAngularForceField);


void ANxTornadoAngularForceField::DefineForceFunction(FPointer ForceFieldDesc)
{
#if WITH_NOVODEX
	NxForceFieldDesc& ffDesc = *(NxForceFieldDesc*)ForceFieldDesc;

	Kernel->setTornadoHeight(U2PScale*ForceHeight);
	Kernel->setRadius(U2PScale*ForceRadius);
	Kernel->setRadiusTop(U2PScale*ForceTopRadius);
	Kernel->setEscapeVelocitySq(U2PScale*U2PScale*EscapeVelocity*EscapeVelocity);
	Kernel->setRotationalStrength(RotationalStrength);
	Kernel->setRadialStrength(RadialStrength);
	Kernel->setBSpecialRadialForce(BSpecialRadialForceMode);
	Kernel->setLiftFallOffHeight(U2PScale*LiftFalloffHeight);
	Kernel->setLiftStrength(LiftStrength);
	Kernel->setSelfRotationStrength(SelfRotationStrength);

	ffDesc.kernel = Kernel;
	ffDesc.coordinates = NX_FFC_CYLINDRICAL;
#endif
}

void ANxTornadoAngularForceField::InitRBPhys()
{
#if WITH_NOVODEX
	check(Kernel == NULL);
	Kernel = new NxForceFieldKernelTornadoAngular;

	Super::InitRBPhys();
#endif
}

void ANxTornadoAngularForceField::TermRBPhys(FRBPhysScene* Scene)
{
#if WITH_NOVODEX
	Super::TermRBPhys(Scene);

	delete Kernel;
	Kernel = NULL;
#endif
}

