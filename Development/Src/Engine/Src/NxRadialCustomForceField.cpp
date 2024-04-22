/*=============================================================================
NxRadialCustomForceField.cpp:
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 


#include "EnginePrivate.h"
#include "EngineForceFieldClasses.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

#include "ForceFunctionRadial.h"

IMPLEMENT_CLASS(ANxRadialCustomForceField);


void ANxRadialCustomForceField::DefineForceFunction(FPointer ForceFieldDesc)
{
#if WITH_NOVODEX
	NxForceFieldDesc& ffDesc = *(NxForceFieldDesc*)ForceFieldDesc;

	Kernel->setRadialStrength(ForceStrength);
	Kernel->setRadius(U2PScale*ForceRadius);
	Kernel->setRadiusRecip(1.0f/(U2PScale*ForceRadius));
	Kernel->setSelfRotationStrength(SelfRotationStrength);
	Kernel->setBLinearFalloff(ForceFalloff==RIF_Linear);

	ffDesc.kernel = Kernel;
	ffDesc.coordinates = NX_FFC_SPHERICAL;
#endif
}

void ANxRadialCustomForceField::InitRBPhys()
{
#if WITH_NOVODEX
	check(Kernel == NULL);
	Kernel = new NxForceFieldKernelRadial;

	Super::Super::InitRBPhys();
#endif
}

void ANxRadialCustomForceField::TermRBPhys(FRBPhysScene* Scene)
{
#if WITH_NOVODEX
	Super::Super::TermRBPhys(Scene);

	delete Kernel;
	Kernel = NULL;
#endif
}

