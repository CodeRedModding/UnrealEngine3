/*=============================================================================
	UnFaceFXMorphTargetProxy.cpp: FaceFX Face Graph node proxy to support
	animating Unreal morph targets.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

#if WITH_FACEFX

#include "UnFaceFXMorphTargetProxy.h"
#include "EngineMaterialClasses.h"

//------------------------------------------------------------------------------
// FFaceFXMorphTargetProxy.
//------------------------------------------------------------------------------

FFaceFXMorphTargetProxy::FFaceFXMorphTargetProxy()
	: SkeletalMeshComponent(NULL)
	, MorphTargetName(NAME_None)
{
}

FFaceFXMorphTargetProxy::~FFaceFXMorphTargetProxy()
{
}

void FFaceFXMorphTargetProxy::Update( FLOAT Value )
{
	if( SkeletalMeshComponent )
	{
		UMorphTarget* MorphTarget = SkeletalMeshComponent->FindMorphTarget(MorphTargetName);
		if( MorphTarget )
		{
			if( Value > 0.0f || Value < 0.0f )
			{
				SkeletalMeshComponent->ActiveMorphs.AddItem(FActiveMorph(MorphTarget, Value));
			}
		}
	}
}

void FFaceFXMorphTargetProxy::SetSkeletalMeshComponent( USkeletalMeshComponent* InSkeletalMeshComponent )
{
	SkeletalMeshComponent = InSkeletalMeshComponent;
}

UBOOL FFaceFXMorphTargetProxy::Link( const FName& InMorphTargetName )
{
	MorphTargetName = InMorphTargetName;
	return TRUE;
}

#endif // WITH_FACEFX
