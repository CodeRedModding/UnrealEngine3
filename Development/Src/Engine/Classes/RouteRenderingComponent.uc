/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class RouteRenderingComponent extends PrimitiveComponent
	native(AI)
	hidecategories(Object)
	editinlinenew;

cpptext
{
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual void UpdateBounds();
};
