/*=============================================================================
	NxTornadoAngularForceFieldCapsule.cpp
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineForceFieldClasses.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

IMPLEMENT_CLASS(ANxTornadoForceFieldCapsule);

void ANxTornadoForceFieldCapsule::InitRBPhys()
{
	Super::InitRBPhys();
}

void ANxTornadoForceFieldCapsule::TermRBPhys(FRBPhysScene* Scene) 
{
	Super::TermRBPhys(Scene);
}


void ANxTornadoForceFieldCapsule::TickSpecial(FLOAT DeltaSeconds)
{
	Super::TickSpecial(DeltaSeconds);
}

#if WITH_EDITOR
void ANxTornadoForceFieldCapsule::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	const FVector ModifiedScale = DeltaScale * 500.0f;

	if(!ModifiedScale.IsUniform())
	{
		const FLOAT XZMultiplier = ( ModifiedScale.X > 0.0f || ModifiedScale.Z > 0.0f ) ? 1.0f : -1.0f;
		const FLOAT YMultiplier = ( ModifiedScale.Y > 0.0f ) ? 1.0f : -1.0f;

		FLOAT x = ModifiedScale.X;
		FLOAT z = ModifiedScale.Z;
		ForceRadius += XZMultiplier * appSqrt(x*x+z*z);
		ForceTopRadius += XZMultiplier * appSqrt(x*x+z*z);
		ForceHeight += YMultiplier * Abs(ModifiedScale.Y);
	}
	else
	{
		const FLOAT Multiplier = ( ModifiedScale.X > 0.0f || ModifiedScale.Y > 0.0f || ModifiedScale.Z > 0.0f ) ? 1.0f : -1.0f;

		ForceRadius += Multiplier * ModifiedScale.Size();
		ForceTopRadius += Multiplier * ModifiedScale.Size();
		ForceHeight += Multiplier * ModifiedScale.Size();
	}

	ForceRadius = Max( 0.0f, ForceRadius );
	ForceHeight = Max( 0.0f, ForceHeight );

	PostEditChange();
}
#endif

/** Update the render component to match the force radius. */
void ANxTornadoForceFieldCapsule::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(RenderComponent)
	{
		{
			FComponentReattachContext ReattachContext(RenderComponent);
			RenderComponent->CapsuleRadius = ForceRadius;
			RenderComponent->CapsuleHeight = ForceHeight;
		}
	}
}

FPointer ANxTornadoForceFieldCapsule::DefineForceFieldShapeDesc()
{
#if WITH_NOVODEX
	// Capsule
	NxCapsuleForceFieldShapeDesc* ffShapeDesc = new NxCapsuleForceFieldShapeDesc();
	//NxCapsuleForceFieldShapeDesc ffShapeDesc;
	ffShapeDesc->radius = U2PScale*ForceRadius;
	ffShapeDesc->height = U2PScale*ForceHeight;
	ffShapeDesc->pose.t.y += U2PScale*HeightOffset;

	return ffShapeDesc;
#else
	return NULL;
#endif
}



