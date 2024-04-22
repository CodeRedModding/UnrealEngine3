/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class NxRadialForceField extends NxForceField
	native(ForceField)
	placeable;


/** Used to preview the radius of the force. */
var	DrawSphereComponent			RenderComponent;

/** Strength of the force applied by this actor. Positive forces are applied outwards. */
var()	interp float	ForceStrength;

/** Radius of influence of the force. */
var()	interp float	ForceRadius;

/** Way in which the force falls off as objects are further away from the location. */
var()	PrimitiveComponent.ERadialImpulseFalloff	ForceFalloff;

/** linear force field kernel */
var const native transient pointer		LinearKernel{class UserForceFieldLinearKernel};

cpptext
{
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void TickSpecial(FLOAT DeltaSeconds);
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual FPointer DefineForceFieldShapeDesc();
}

defaultproperties
{
	Begin Object Class=DrawSphereComponent Name=DrawSphere0
		SphereColor=(R=64,G=70,B=255,A=255)
		SphereRadius=200.0
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

	RemoteRole=ROLE_SimulatedProxy
	bNoDelete=true
	bAlwaysRelevant=true
	NetUpdateFrequency=0.1
	bOnlyDirtyReplication=true
}
