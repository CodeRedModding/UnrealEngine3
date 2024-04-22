/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class NxGenericForceFieldCapsule extends NxGenericForceField
	native(ForceField)
	placeable;


/** Used to preview the radius of the force. */
var	DrawCapsuleComponent			RenderComponent;

/** Radius of influence of the force. */
var()	float	CapsuleHeight;
var()	float	CapsuleRadius;

cpptext
{
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
	virtual FPointer DefineForceFieldShapeDesc();
}


defaultproperties
{
	Begin Object Class=DrawCapsuleComponent Name=DrawCapsule0
		CapsuleColor=(R=64,G=70,B=255,A=255)
		CapsuleRadius=200.0
		CapsuleHeight=200.0
	End Object
	RenderComponent=DrawCapsule0
	Components.Add(DrawCapsule0)

	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_RadForce'
		HiddenGame=True
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Physics"
	End Object
	Components.Add(Sprite)

	TickGroup=TG_PreAsyncWork

	CapsuleHeight=200.0
	CapsuleRadius=200.0

	RemoteRole=ROLE_SimulatedProxy
	bNoDelete=true
	bAlwaysRelevant=true
	NetUpdateFrequency=0.1
	bOnlyDirtyReplication=true
}
