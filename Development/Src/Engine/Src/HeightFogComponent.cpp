/*=============================================================================
	HeightFogComponent.cpp: Height fog implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineFogVolumeClasses.h"

IMPLEMENT_CLASS(UHeightFogComponent);

void UHeightFogComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	Super::SetParentToWorld(ParentToWorld);
	Height = ParentToWorld.GetOrigin().Z;
}

void UHeightFogComponent::Attach()
{
	Super::Attach();
	if(bEnabled && Density > DELTA / 1000.0f)
	{
		Scene->AddHeightFog(this);
	}
}

void UHeightFogComponent::UpdateTransform()
{
	Super::UpdateTransform();
	Scene->RemoveHeightFog(this);
	if(bEnabled && Density > DELTA / 1000.0f)
	{
		Scene->AddHeightFog(this);
	}
}

void UHeightFogComponent::Detach( UBOOL bWillReattach )
{
	Super::Detach( bWillReattach );
	Scene->RemoveHeightFog(this);
}

void UHeightFogComponent::SetEnabled(UBOOL bSetEnabled)
{
	if(bEnabled != bSetEnabled)
	{
		// Update bEnabled, and begin a deferred component reattach.
		bEnabled = bSetEnabled;
		BeginDeferredReattach();
	}
}

IMPLEMENT_CLASS(UExponentialHeightFogComponent);

void UExponentialHeightFogComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	Super::SetParentToWorld(ParentToWorld);
	FogHeight = ParentToWorld.GetOrigin().Z;
}

void UExponentialHeightFogComponent::Attach()
{
	Super::Attach();
	if(bEnabled && FogDensity > DELTA && FogMaxOpacity > DELTA)
	{
		Scene->AddExponentialHeightFog(this);
	}
}

void UExponentialHeightFogComponent::UpdateTransform()
{
	Super::UpdateTransform();
	Scene->RemoveExponentialHeightFog(this);
	if(bEnabled && FogDensity > DELTA && FogMaxOpacity > DELTA)
	{
		Scene->AddExponentialHeightFog(this);
	}
}

void UExponentialHeightFogComponent::Detach( UBOOL bWillReattach )
{
	Super::Detach( bWillReattach );
	Scene->RemoveExponentialHeightFog(this);
}

void UExponentialHeightFogComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FogDensity = Clamp(FogDensity, 0.0f, 10.0f);
	FogHeightFalloff = Clamp(FogHeightFalloff, 0.0f, 2.0f);
	FogMaxOpacity = Clamp(FogMaxOpacity, 0.0f, 1.0f);
	StartDistance = Clamp(StartDistance, 0.0f, (FLOAT)WORLD_MAX);
	LightTerminatorAngle = Clamp(LightTerminatorAngle, 0.0f, 180.0f);
	OppositeLightBrightness = Max(OppositeLightBrightness, 0.0f);
	LightInscatteringBrightness = Max(LightInscatteringBrightness, 0.0f);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UExponentialHeightFogComponent::SetEnabled(UBOOL bSetEnabled)
{
	if(bEnabled != bSetEnabled)
	{
		// Update bEnabled, and begin a deferred component reattach.
		bEnabled = bSetEnabled;
		BeginDeferredReattach();
	}
}
