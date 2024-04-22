/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 */

class GameCrowdInteractionPoint extends Actor
	native
	abstract
	hidecategories(Advanced)
	hidecategories(Collision)
	hidecategories(Display)
	hidecategories(Actor)
	hidecategories(Movement)
	hidecategories(Physics)
	ClassGroup(Crowd)
	placeable;

/** If this interactionpoint is currently enabled */
var()	bool			bIsEnabled;

/** Cylinder component  */
var()	CylinderComponent	CylinderComponent;

cpptext
{
#if WITH_EDITOR
	// AActor interface.
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
}

replication
{
	if (bNoDelete)
		bIsEnabled;
}

function OnToggle(SeqAct_Toggle action)
{
	if (action.InputLinks[0].bHasImpulse)
	{
		// turn on
		bIsEnabled = TRUE;
	}
	else if (action.InputLinks[1].bHasImpulse)
	{
		// turn off
		bIsEnabled = FALSE;
	}
	else if (action.InputLinks[2].bHasImpulse)
	{
		// toggle
		bIsEnabled = !bIsEnabled;
	}

	// Make this actor net relevant, and force replication, even if it now does not differ from class defaults.
	ForceNetRelevant();
}

defaultproperties
{
	TickGroup=TG_DuringAsyncWork

	bIsEnabled=true

	bCollideActors=FALSE
	bNoDelete=true

	Begin Object Class=CylinderComponent NAME=CollisionCylinder
		CollideActors=FALSE
		bDrawNonColliding=TRUE
		CollisionRadius=+0200.000000
		CollisionHeight=+0040.000000
		CylinderColor=(R=0,G=255,B=0)
		bDrawBoundingBox=FALSE
	End Object
	CylinderComponent=CollisionCylinder
	CollisionComponent=CollisionCylinder
	Components.Add(CollisionCylinder)

	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.Crowd.T_Crowd_Behavior'
		Scale=0.5		
		HiddenGame=TRUE
		HiddenEditor=FALSE
		AlwaysLoadOnClient=FALSE
		AlwaysLoadOnServer=FALSE
	End Object
	Components.Add(Sprite)
}