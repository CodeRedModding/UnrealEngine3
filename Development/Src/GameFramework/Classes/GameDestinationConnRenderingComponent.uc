/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameDestinationConnRenderingComponent extends PrimitiveComponent
	native
	hidecategories(Object)
	editinlinenew;

cpptext
{
	/**
	 * Creates a new scene proxy for the path rendering component.
	 * @return	Pointer to the FConnectionRenderingSceneProxy
	 */
	virtual FPrimitiveSceneProxy* CreateSceneProxy();

	virtual void UpdateBounds();
};

defaultproperties
{
	HiddenGame=true
	AlwaysLoadOnClient=false
	AlwaysLoadOnServer=false
}
