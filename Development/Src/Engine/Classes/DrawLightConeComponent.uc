/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DrawLightConeComponent extends DrawConeComponent
	native(Light)
	hidecategories(Physics,Collision,PrimitiveComponent,Rendering);

cpptext
{
	/**
	 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
	 * @return The proxy object.
	 */
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
}

defaultproperties
{
	AlwaysLoadOnClient=False
	AlwaysLoadOnServer=False
	AbsoluteScale=TRUE
}
