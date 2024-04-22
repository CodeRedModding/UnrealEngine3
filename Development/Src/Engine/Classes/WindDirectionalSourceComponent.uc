/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class WindDirectionalSourceComponent extends ActorComponent
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew;

var native private	transient noimport const pointer SceneProxy{FWindSourceSceneProxy};

var() interp float Strength;
var deprecated float Phase;
var deprecated float Frequency;
var() interp float Speed;

cpptext
{
protected:
	// UActorComponent interface.
	virtual void Attach();
	virtual void Detach( UBOOL bWillReattach = FALSE );
	virtual void UpdateTransform();
public:
	
	/**
	 * Creates a proxy to represent the wind source to the scene manager in the rendering thread.
	 * @return The proxy object.
	 */
	 virtual class FWindSourceSceneProxy* CreateSceneProxy() const;
}

defaultproperties
{
	Strength=1.0
	Frequency=1.0
	Speed=1.0
}
