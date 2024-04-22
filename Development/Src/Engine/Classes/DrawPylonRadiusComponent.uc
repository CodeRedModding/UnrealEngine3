/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DrawPylonRadiusComponent extends DrawSphereComponent
	native
	hidecategories(Physics,Collision,PrimitiveComponent,Rendering);

cpptext
{
	FPrimitiveSceneProxy* CreateSceneProxy();
	void UpdateBounds();
	void Attach();
};

defaultproperties
{
	SphereColor=(R=173,G=239,B=231,A=255)
	SphereSides=32

	AlwaysLoadOnClient=False
	AlwaysLoadOnServer=False

	AbsoluteScale=TRUE
}
