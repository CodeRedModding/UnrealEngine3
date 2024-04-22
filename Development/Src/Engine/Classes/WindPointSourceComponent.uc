/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class WindPointSourceComponent extends WindDirectionalSourceComponent
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew;

var const DrawSphereComponent PreviewRadiusComponent;

var() interp float Radius;

cpptext
{
protected:
	void UpdatePreviewRadius();
	// UActorComponent interface.
	virtual void Attach();
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
	Radius=1024.0
}
