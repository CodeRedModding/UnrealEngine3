/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DrawLightRadiusComponent extends DrawSphereComponent
	native(Light)
	noexport
	hidecategories(Physics,Collision,PrimitiveComponent,Rendering);

defaultproperties
{
	SphereColor=(R=173,G=239,B=231,A=255)
	SphereSides=32

	AlwaysLoadOnClient=False
	AlwaysLoadOnServer=False

	AbsoluteScale=TRUE
}
