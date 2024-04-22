/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class PathTargetPoint extends KeyPoint
	native(AI);

/**
 * replaces IsA(NavigationPoint) check for primitivecomponents 
 */
native function bool ShouldBeHiddenBySHOW_NavigationNodes();

defaultproperties
{
	Begin Object Name=Sprite
		Sprite=Texture2D'EditorResources.PathTarget'
		Scale=0.35
		SpriteCategoryName="Navigation"
	End Object

	Begin Object Class=ArrowComponent Name=Arrow
		ArrowColor=(R=150,G=200,B=255)
		ArrowSize=0.5
		bTreatAsASprite=True
		HiddenGame=true
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Navigation"
	End Object
	Components.Add(Arrow)

	Begin Object Class=CylinderComponent Name=CollisionCylinder
		CollisionRadius=+0050.000000
		CollisionHeight=+0073.000000
	End Object
	CollisionComponent=CollisionCylinder
	Components.Add(CollisionCylinder)

	bStatic=False
	bNoDelete=true
	bMovable=True

	bHidden=False

	SupportedEvents(4)=class'SeqEvent_AIReachedRouteActor'
}
