/*=============================================================================
	UnFaceFXMaterialParameterProxy.h: FaceFX Face Graph node proxy to support
	animating Unreal material parameters.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if WITH_FACEFX

#ifndef UnFaceFXMaterialParameterProxy_H__
#define UnFaceFXMaterialParameterProxy_H__

// Forward declarations.
class USkeletalMeshComponent;
class UMaterialInstanceConstant;

class FFaceFXMaterialParameterProxy
{
public:
	// Constructor.
	FFaceFXMaterialParameterProxy();
	// Destructor.
	~FFaceFXMaterialParameterProxy();

	// Updates the material parameter.
	void Update( FLOAT Value );

	// Sets the skeletal mesh that owns the material parameter that the proxy
	// controls.
	void SetSkeletalMeshComponent( USkeletalMeshComponent* InSkeletalMeshComponent );

	// Links the proxy to the material parameter that it controls.
	UBOOL Link( INT InMaterialSlotID, const FName& InScalarParameterName );

protected:
	// A pointer to the skeletal mesh component that owns the material parameter 
	// that the proxy controls.
	USkeletalMeshComponent* SkeletalMeshComponent;
	// A pointer to the material instance constant that the proxy controls.
	UMaterialInstanceConstant* MaterialInstanceConstant;
	// The material slot id.
	INT MaterialSlotID;
	// The name of the scalar parameter that the proxy controls.
	FName ScalarParameterName;
};

#endif

#endif // WITH_FACEFX
