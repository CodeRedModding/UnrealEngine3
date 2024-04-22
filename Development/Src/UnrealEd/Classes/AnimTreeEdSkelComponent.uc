/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class AnimTreeEdSkelComponent extends SkeletalMeshComponent
	native;

var transient native const pointer	AnimTreeEdPtr;

cpptext
{
	// UPrimitiveComponent interface.
	virtual void Render(const FSceneView* View, class FPrimitiveDrawInterface* PDI);

	// USkeletalMeshComponent interface.
	virtual UBOOL LegLineCheck(const FVector& Start, const FVector& End, FVector& HitLocation, FVector& HitNormal, const FVector& Extent = FVector(0.f));
}
