/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 * DO NOT USE, try the NxForceField classes instead!
 */
class RB_RadialForceActor extends RigidBodyBase
	native(ForceField)
	placeable;

enum ERadialForceType
{
	RFT_Force,		// Use as a force
	RFT_Impulse,	// Use as an impulse, gets deactivated when applied once
};

/** Used to preview the radius of the force. */
var	DrawSphereComponent			RenderComponent;

/** Strength of the force applied by this actor. Positive forces are applied outwards. */
var()	interp float	ForceStrength;

/** Radius of influence of the force. */
var()	interp float	ForceRadius;

/** How strongly objects orbit around Z axis of actor. */
var()	interp float	SwirlStrength;

/** How strongly to spin objects around their local Z. */
var()	interp float	SpinTorque;

/** Way in which the force falls off as objects are further away from the location. */
var()	PrimitiveComponent.ERadialImpulseFalloff	ForceFalloff;

/** Indicates whether the force is active at the moment. */
var()	bool	bForceActive;

/** Indicates which type of force mode is used. */
var()	ERadialForceType	RadialForceMode;

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

	SetForcedInitialReplicatedProperty(Property'Engine.RB_RadialForceActor.bForceActive', (bForceActive == default.bForceActive));
}

defaultproperties
{
	Begin Object Class=DrawSphereComponent Name=DrawSphere0
		SphereColor=(R=64,G=70,B=255,A=255)
		SphereRadius=200.0
		bDrawOnlyIfSelected=True
	End Object
	RenderComponent=DrawSphere0
	Components.Add(DrawSphere0)

	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_RadForce'
		HiddenGame=True
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Physics"
	End Object
	Components.Add(Sprite)

	TickGroup=TG_PreAsyncWork

	ForceStrength=10.0
	ForceRadius=200.0
	ForceFalloff=RIF_Constant
	RadialForceMode=RFT_Force

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
                Untitled4=True,
                Cloth=True,
                FluidDrain=True,
                )}
}
