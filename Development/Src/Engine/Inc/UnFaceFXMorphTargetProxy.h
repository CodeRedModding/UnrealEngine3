/*=============================================================================
	UnFaceFXMorphTargetProxy.h: FaceFX Face Graph node proxy to support
	animating Unreal morph targets.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if WITH_FACEFX

#ifndef UnFaceFXMorphTargetProxy_H__
#define UnFaceFXMorphTargetProxy_H__

class FFaceFXMorphTargetProxy
{
public:
	// Constructor.
	FFaceFXMorphTargetProxy();
	// Destructor.
	~FFaceFXMorphTargetProxy();

	// Updates the morph target.
	void Update( FLOAT Value );

	// Sets the skeletal mesh that owns the morph target that the proxy controls.
	void SetSkeletalMeshComponent( USkeletalMeshComponent* InSkeletalMeshComponent );

	// Links the proxy to the morph target that it controls.
	UBOOL Link( const FName& InMorphTargetName );

protected:
	// A pointer to the skeletal mesh component that owns the morph target 
	// that the proxy controls.
	USkeletalMeshComponent* SkeletalMeshComponent; 
	// The name of the morph target that the proxy controls.
	FName MorphTargetName;
};

#endif

#endif // WITH_FACEFX
