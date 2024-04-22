/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class FaceFXStudioSkelComponent extends SkeletalMeshComponent
	native;

var transient native const pointer RenderWidgetUE3Ptr;

cpptext
{
	// UPrimitiveComponent interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
}
