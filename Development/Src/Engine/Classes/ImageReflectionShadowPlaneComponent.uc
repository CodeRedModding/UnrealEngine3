/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ImageReflectionShadowPlaneComponent extends PrimitiveComponent
	native(Mesh)
	placeable
	editinlinenew;

/** Whether to render the reflection. */
var() bool bEnabled;

var plane ReflectionPlane;

cpptext
{
protected:
	// ActorComponent interface.
	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual void Attach();
	virtual void UpdateTransform();
	virtual void Detach( UBOOL bWillReattach = FALSE );
}

/**
* Changes the enabled state of the image reflection component.
* @param bSetEnabled - The new value for bEnabled.
*/
final native function SetEnabled(bool bSetEnabled);

defaultproperties
{
	bEnabled=true
	ReflectionPlane=(X=0.0,Y=0.0,Z=1.0,W=86.0)
	bCastDynamicShadow=FALSE
	BlockRigidBody=FALSE
	CollideActors=FALSE
	bForceDirectLightMap=FALSE
	bAcceptsDynamicLights=FALSE
	bAcceptsLights=FALSE
	CastShadow=FALSE
	bUsePrecomputedShadows=FALSE
	bAcceptsStaticDecals=FALSE
	bAcceptsDynamicDecals=FALSE
	bUseAsOccluder=FALSE
	HiddenGame=true
}
