/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 * DO NOT USE, try the NxForceField classes instead!
 */
class RB_CylindricalForceActor extends RigidBodyBase
	native(ForceField)
	placeable;

/** Used to preview the radius of the force. */
var()	DrawCylinderComponent			RenderComponent;

/** Strength of the force applied by this actor.*/
var()	interp float	RadialStrength;

/** Rotational strength of the force applied around the cylinder axis.*/
var()	interp float	RotationalStrength;

/** Strength of the force applied along the cylinder axis */
var()	interp float	LiftStrength;

/** Lift falloff height, 0-1, lift starts to fall off in a linear way above this height */
var()	interp float	LiftFalloffHeight;

/** Velocity above which the radial force is ignored. */
var()	interp float	EscapeVelocity;

/** Radius of influence of the force at the bottom of the cylinder. */
var()	interp float	ForceRadius;

/** Radius of the force field at the top */
var()	interp float	ForceTopRadius;

/** Height of force cylinder */
var()	interp float	ForceHeight;

/** Offset from the actor base to the center of the force field */
var()	interp float	HeightOffset;

/** Indicates whether the force is active at the moment. */
var()	bool	bForceActive;

/** Apply force field to cloth */
var()	bool	bForceApplyToCloth;

/** Apply force field to fluid */
var()	bool	bForceApplyToFluid;

/** Apply force field to rigid bodies */
var()	bool	bForceApplyToRigidBodies;

/** Apply force field to projectiles like rockets */
var()	bool	bForceApplyToProjectiles;

//TODO: Remove the above booleans and just use the channels
/** Which types of object to apply this force field to */
var()	const RBCollisionChannelContainer CollideWithChannels;

cpptext
{
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void TickSpecial(FLOAT DeltaSeconds);
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
}

replication
{
	if (bNetDirty)
		bForceActive;
}

/** Handling Toggle event from Kismet. */
simulated function OnToggle(SeqAct_Toggle inAction)
{
	if(inAction.InputLinks[0].bHasImpulse)
	{
		bForceActive = true;
	}
	else if(inAction.InputLinks[1].bHasImpulse)
	{
		bForceActive = false;
	}
	else if(inAction.InputLinks[2].bHasImpulse)
	{
		bForceActive = !bForceActive;
	}

	SetForcedInitialReplicatedProperty(Property'Engine.RB_CylindricalForceActor.bForceActive', (bForceActive == default.bForceActive));
}

defaultproperties
{
	Begin Object Class=DrawCylinderComponent Name=DrawCylinder0
		CylinderRadius=200.0
		CylinderTopRadius=200.0
		CylinderHeight=200.0
		bDrawOnlyIfSelected=True
	End Object

	RenderComponent=DrawCylinder0
	Components.Add(DrawCylinder0)

	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_RadForce'
		HiddenGame=True
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Physics"
	End Object
	Components.Add(Sprite)

	TickGroup=TG_PreAsyncWork

	ForceRadius=200.0
	ForceTopRadius=200.0
	ForceHeight=200.0
	LiftFalloffHeight=0.0
	EscapeVelocity=10000.0

	RemoteRole=ROLE_SimulatedProxy
	bNoDelete=true
	bAlwaysRelevant=true
	NetUpdateFrequency=0.1
	bOnlyDirtyReplication=true
	
	bForceApplyToCloth=True
	bForceApplyToFluid=True
	bForceApplyToRigidBodies=True
	bForceApplyToProjectiles=False
	
	CollideWithChannels={( 
                Default=True,
                Pawn=True,
                Vehicle=True,
                Water=True,
                GameplayPhysics=True,
                EffectPhysics=True,
                Untitled1=True,
                Untitled2=True,
                Untitled3=True,
                FluidDrain=True,
                Cloth=True
                )}
	
}
