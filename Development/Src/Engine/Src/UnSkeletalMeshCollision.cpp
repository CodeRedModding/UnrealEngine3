/*=============================================================================
	UnSkeletalMeshCollision.cpp: Skeletal mesh collision code
	Copyright 2003 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"

UBOOL USkeletalMeshComponent::LineCheck(
                FCheckResult &Result,
                const FVector& End,
                const FVector& Start,
                const FVector& Extent,
				DWORD TraceFlags)
{
	// Can only do line-tests against skeletal meshes if we have a physics asset for it.
	if(PhysicsAsset)
		return PhysicsAsset->LineCheck(Result, this, Start, End, Extent);
	else
		return 1;

}

UBOOL USkeletalMeshComponent::PointCheck(
	FCheckResult&	Result,
	const FVector&	Location,
	const FVector&	Extent)
{
	return 1;

}

