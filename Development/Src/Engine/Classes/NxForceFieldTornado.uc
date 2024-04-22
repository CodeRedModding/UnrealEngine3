/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class NxForceFieldTornado extends NxForceField
	native(ForceField)
	placeable;

var() editinline ForceFieldShape Shape;

var ActorComponent DrawComponent;

/** Strength of the force applied by this actor.*/
var()	interp float	RadialStrength;

/** Rotational strength of the force applied around the cylinder axis.*/
var()	interp float	RotationalStrength;

/** Strength of the force applied along the cylinder axis */
var()	interp float	LiftStrength;

/** Radius of influence of the force at the bottom of the cylinder. */
var()	interp float	ForceRadius;

/** Radius of the force field at the top */
var()	interp float	ForceTopRadius;

/** Lift falloff height, 0-1, lift starts to fall off in a linear way above this height */
var()	interp float	LiftFalloffHeight;

/** Velocity above which the radial force is ignored. */
var()	interp float	EscapeVelocity;

/** Height of force cylinder */
var()	interp float	ForceHeight;

/** Offset from the actor base to the center of the force field */
var()	interp float	HeightOffset;

/**  */
var()	bool BSpecialRadialForceMode;

/** */
var()	interp float	SelfRotationStrength;

/** custom force field kernel */
var const native transient pointer		Kernel{class NxForceFieldKernelTornadoAngular};

cpptext
{
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);
	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual FPointer DefineForceFieldShapeDesc();
	virtual void SetForceFieldPose(FPointer ForceFieldDesc);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
	virtual void PostLoad();
}

/** 
 * This is used to InitRBPhys for a dynamically spawned ForceField.
 * Used for starting RBPhsys on dyanmically spawned force fields.  This will probably need to set some transient pointer to NULL  
 **/
native function DoInitRBPhys();


defaultproperties
{
	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_RadForce'
		HiddenGame=True
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Physics"
	End Object
	Components.Add(Sprite)

	ForceRadius=200.0
	ForceTopRadius=200.0
	ForceHeight=200.0
}
