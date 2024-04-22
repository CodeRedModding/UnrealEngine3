/*=============================================================================
NxForceFieldGeneric.cpp
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineForceFieldClasses.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#include "UserForceFieldLinearKernel.h"
#endif // WITH_NOVODEX

IMPLEMENT_CLASS(ANxForceFieldGeneric);


void ANxForceFieldGeneric::DoInitRBPhys()
{
#if WITH_NOVODEX
	LinearKernel = NULL;
	ForceField = NULL;
	InitRBPhys();
#endif
}

void ANxForceFieldGeneric::InitRBPhys()
{
#if WITH_NOVODEX
	NxScene* nxScene = GWorld->RBPhysScene->GetNovodexPrimaryScene();
	NxForceFieldLinearKernelDesc kernelDesc;
	check(LinearKernel == NULL);
	WaitForNovodexScene(*nxScene);
	LinearKernel = UserForceFieldLinearKernel::Create(nxScene->createForceFieldLinearKernel(kernelDesc), nxScene);

	Super::InitRBPhys();
#endif
}

void ANxForceFieldGeneric::TermRBPhys(FRBPhysScene* Scene)
{
#if WITH_NOVODEX
	Super::TermRBPhys(Scene);

	if (LinearKernel && Scene)
	{
		NxScene* nxScene = Scene->GetNovodexPrimaryScene();
		if(nxScene->checkResults(NX_ALL_FINISHED, false))
		{
			GNovodexPendingKillForceFieldLinearKernels.AddItem(LinearKernel);
		}
		else
		{
			LinearKernel->Destroy();
		}
	}
	LinearKernel = NULL;
#endif
}

void ANxForceFieldGeneric::DefineForceFunction(FPointer ForceFieldDesc)
{
#if WITH_NOVODEX
	NxForceFieldLinearKernelDesc lKernelDesc;

	NxForceFieldDesc& ffDesc = *(NxForceFieldDesc*)ForceFieldDesc;

	// Define force function
	switch (Coordinates)
	{
	case FFG_CARTESIAN		: ffDesc.coordinates = NX_FFC_CARTESIAN; break;
	case FFG_SPHERICAL		: ffDesc.coordinates = NX_FFC_SPHERICAL; break;
	case FFG_CYLINDRICAL	: ffDesc.coordinates = NX_FFC_CYLINDRICAL; break;
	case FFG_TOROIDAL		: ffDesc.coordinates = NX_FFC_TOROIDAL; break;
	}

	LinearKernel->setConstant(U2NVectorCopy(Constant));

	LinearKernel->setPositionMultiplier(NxMat33(
		U2NVectorCopy(PositionMultiplierX),
		U2NVectorCopy(PositionMultiplierY),
		U2NVectorCopy(PositionMultiplierZ)
		));
	LinearKernel->setPositionTarget(U2NPosition(PositionTarget));

	LinearKernel->setVelocityMultiplier(NxMat33(
		U2NVectorCopy(VelocityMultiplierX),
		U2NVectorCopy(VelocityMultiplierY),
		U2NVectorCopy(VelocityMultiplierZ)
		));
	LinearKernel->setVelocityTarget(U2NPosition(VelocityTarget));

	LinearKernel->setNoise(U2NVectorCopy(Noise));

	LinearKernel->setFalloffLinear(U2NVectorCopy(FalloffLinear));
	LinearKernel->setFalloffQuadratic(U2NVectorCopy(FalloffQuadratic));

	LinearKernel->setTorusRadius(U2PScale*TorusRadius);

	ffDesc.kernel = *LinearKernel;
#endif
}

FPointer ANxForceFieldGeneric::DefineForceFieldShapeDesc()
{
#if WITH_NOVODEX
	return Shape ? Shape->CreateNxDesc() : NULL;
#else
	return NULL;
#endif
}

void ANxForceFieldGeneric::PostLoad()
{
	Super::PostLoad();

	if( DrawComponent != NULL )
	{
		Components.AddItem(DrawComponent);
	}
}

void ANxForceFieldGeneric::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (appStrstr(*(PropertyThatChanged->GetName()), TEXT("Shape")) != NULL)
		{
			// original Shape object is gone
			DetachComponent(DrawComponent);
			DrawComponent = NULL;

			// if new Shape object is available
			if (Shape && Shape->eventGetDrawComponent())
			{
				DrawComponent = Shape->eventGetDrawComponent();
				Shape->eventFillByBox(FVector(RoughExtentX, RoughExtentY, RoughExtentZ));
				AttachComponent(DrawComponent);
			}
		}
		else if (Shape && Shape->eventGetDrawComponent())
		{
			FComponentReattachContext ReattachContext(Shape->eventGetDrawComponent());
			if ((appStrstr(*(PropertyThatChanged->GetName()), TEXT("RoughExtentX")) != NULL)
				||(appStrstr(*(PropertyThatChanged->GetName()), TEXT("RoughExtentY")) != NULL)
				||(appStrstr(*(PropertyThatChanged->GetName()), TEXT("RoughExtentZ")) != NULL))
			{
				Shape->eventFillByBox(FVector(RoughExtentX, RoughExtentY, RoughExtentZ));
			}
		}
	}
}

#if WITH_EDITOR
void ANxForceFieldGeneric::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	FVector ModifiedScale = DeltaScale*500;

	RoughExtentX += ModifiedScale.X;
	RoughExtentY += ModifiedScale.Y;
	RoughExtentZ += ModifiedScale.Z;

	RoughExtentX = Max(0.0f, RoughExtentX);
	RoughExtentY = Max(0.0f, RoughExtentY);
	RoughExtentZ = Max(0.0f, RoughExtentZ);

	if (Shape && Shape->eventGetDrawComponent())
	{
		FComponentReattachContext ReattachContext(Shape->eventGetDrawComponent());
		Shape->eventFillByBox(FVector(RoughExtentX, RoughExtentY, RoughExtentZ));
	}

}
#endif
