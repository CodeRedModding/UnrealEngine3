//=============================================================================
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class InteractiveFoliageActor extends StaticMeshActor
	native(Foliage)
	placeable;

/** Collision cylinder */
var private{private} CylinderComponent CylinderComponent;

/**
 * Position of the last actor to enter the collision cylinder.
 * This currently does not handle multiple actors affecting the foliage simultaneously.
 */
var private{private} transient vector TouchingActorEntryPosition;

/** Simulated physics state */
var private{private} transient vector FoliageVelocity;
var private{private} transient vector FoliageForce;
var private{private} transient vector FoliagePosition;

/** Scales forces applied from damage events. */
var(FoliagePhysics) float FoliageDamageImpulseScale;

/** Scales forces applied from touch events. */
var(FoliagePhysics) float FoliageTouchImpulseScale;

/** Determines how strong the force that pushes toward the spring's center will be. */
var(FoliagePhysics) float FoliageStiffness;

/**
 * Same as FoliageStiffness, but the strength of this force increases with the square of the distance to the spring's center.
 * This force is used to prevent the spring from extending past a certain point due to touch and damage forces.
 */
var(FoliagePhysics) float FoliageStiffnessQuadratic;

/**
 * Determines the amount of energy lost by the spring as it oscillates.
 * This force is similar to air friction.
 */
var(FoliagePhysics) float FoliageDamping;

/** Clamps the magnitude of each damage force applied. */
var(FoliagePhysics) float MaxDamageImpulse;

/** Clamps the magnitude of each touch force applied. */
var(FoliagePhysics) float MaxTouchImpulse;

/** Clamps the magnitude of combined forces applied each update. */
var(FoliagePhysics) float MaxForce;

//@todo - hook this up
var float Mass;

cpptext
{
protected:
	void SetupCollisionCylinder();
public:
	virtual void TickSpecial(FLOAT DeltaSeconds);
	virtual void Spawned();
	virtual void PostLoad();
};

native simulated event TakeDamage(int Damage, Controller EventInstigator, vector HitLocation, vector Momentum, class<DamageType> DamageType, optional TraceHitInfo HitInfo, optional Actor DamageCauser);
native simulated event Touch( Actor Other, PrimitiveComponent OtherComp, vector HitLocation, vector HitNormal );

defaultproperties
{
	Begin Object Class=InteractiveFoliageComponent Name=FoliageMeshComponent0
		bAllowApproximateOcclusion=TRUE
		bForceDirectLightMap=TRUE
		bUsePrecomputedShadows=TRUE
		// Foliage actors are usually animated using vertex position offset, which does not work correctly with decals
		bAcceptsStaticDecals=FALSE
		bAcceptsDynamicDecals=FALSE
	End Object
	StaticMeshComponent=FoliageMeshComponent0
	Components.Remove(StaticMeshComponent0)
	Components.Add(FoliageMeshComponent0)

	// Needs to receive Touch and TakeDamage
	bCollideActors=true
	// Don't want to block actors, just be notified when they are touching
	bBlockActors=false
	// Block bullets so we can receive TakeDamage from them, the damage will be passed through with PassThroughDamage
	bProjTarget=true
	BlockRigidBody=false
	
	// Add a cylinder component to be used for collision
	Begin Object Class=CylinderComponent Name=CollisionCylinder
		// These get overwritten on load or spawn
		CollisionRadius=+00060.000000
		CollisionHeight=+00200.000000
		CollideActors=true
		BlockActors=false
		// Don't want the cylinder to block bullets
		BlockZeroExtent=false
		BlockNonZeroExtent=true
	End Object
	CollisionComponent=CollisionCylinder
	CylinderComponent=CollisionCylinder
	Components.Add(CollisionCylinder)

	bWorldGeometry=false
	// Has to be dynamic to get ticked
	bStatic=false
	bMovable=false
	bNoDelete=true
	// Visual effects should tick during async work
	TickGroup=TG_DuringAsyncWork

	FoliageDamageImpulseScale=20
	FoliageTouchImpulseScale=10
	FoliageStiffness=10
	FoliageStiffnessQuadratic=.3
	FoliageDamping=2
	MaxDamageImpulse=100000
	MaxTouchImpulse=1000
	MaxForce=100000
	Mass=1
}
