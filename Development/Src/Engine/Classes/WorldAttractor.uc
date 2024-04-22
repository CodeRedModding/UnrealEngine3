//=============================================================================
// WorldAttractor.
//
// WorldAttractor is a placeable generic attractor.
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class WorldAttractor extends Actor
	native(Physics)
	hidecategories(Lighting,LightColor,Force)
	placeable;

/**
 *  TRUE if this attractor has any effect.
 */
var() bool bEnabled;

/**
 *  The duration in seconds to loop over the values in the distributions below.
 */
var() float LoopDuration;

/**
 *  The current time through the loop.
 */
var float CurrentTime;

/**
 *  Falloff type enumeration.
 */
enum EWorldAttractorFalloffType
{
	FOFF_Constant,
	FOFF_Linear,
	FOFF_Exponent
};

/**
 *  Type of falloff.
 *
 *  FOFF_Constant - Falloff is constant so just use the strength for the whole range.
 *  FOFF_Linear - Linear falloff over the range.
 *  FOFF_ExponentExponential falloff over the range.
 */
var() EWorldAttractorFalloffType FalloffType;

/**
 *  Optional falloff exponent for FOFF_Exponent type.
 */
var() interp matineerawdistributionfloat FalloffExponent;

/**
 *  Range of the attractor.
 */
var() interp matineerawdistributionfloat Range;

/**
 *  Strength of the attraction over time.
 */
var() interp matineerawdistributionfloat Strength;

/**
 *  Radius bounding the attraction point for use with collisions.
 */
var() interp float CollisionRadius;

/**
 *  Drag coefficient, a value of 1.0f means no drag, a value > 1.0f means accelerate.
 *  This value is multiplied with the DragCoefficient value in the attractor to get the
 *  resultant drag coefficient and generate the drag force.
 */
var() interp matineerawdistributionfloat DragCoefficient;

/**
 *  Apply the drag when the particle is within this radius.
 */
var() interp matineerawdistributionfloat DragRadius;

struct native WorldAttractorData
{
	var bool bEnabled;
	var vector Location;
	var EWorldAttractorFalloffType FalloffType;
	var float FalloffExponent;
	var float Range;
	var float Strength;
};

cpptext
{
	/**
	 *  Override these to manage the list of AWorldAttractor instances.  These will call either RegisterAttractor or
	 *  UnregisterAttractor as necessary and then call the base class implementation.
	 */
	virtual void Spawned();
	virtual void PostLoad();
	/** ticks the actor
	 * @return TRUE if the actor was ticked, FALSE if it was aborted (e.g. because it's in stasis)
	 */
	virtual UBOOL Tick( FLOAT DeltaTime, enum ELevelTick TickType );
	virtual void BeginDestroy();
	virtual void Serialize(FArchive& Ar);
	void SetZone( UBOOL bTest, UBOOL bForceRefresh );

	/**
	 *  Get the attraction data at the provided time.
	 *  @param Time This is time relative to whatever the calling function desires (particle life, emitter life, matinee start, etc.)
	 *  @param Data Returned data for the given time.
	 */
	void GetAttractorDataAtTime(const FLOAT Time, FWorldAttractorData& Data);

	/**
	 *  Generate the attraction velocity to add to an actor's current velocity.
	 *  @param CurrentLocation The location of the actor at the start of this tick.
	 *  @param CurrentTime This is time relative to whatever the calling function desires (particle life, emitter life, matinee start, etc.)
	 *  @param DeltaTime This is the time since the last update call.
	 *  @param ParticleBoundingRadius Used to calculate drag.
	 *  @returns The velocity to add to the actor's current velocity.
	 */
	FVector GetVelocityForAttraction(const FVector CurrentLocation, const FLOAT CurrentTime, const FLOAT DeltaTime, const FLOAT ParticleBoundingRadius = 0.0f);

	/**
	 *  Tests for a collision and if there is a collision the actions in the array are executed on the particle.
	 *  @param ParticleEmitter The particle emitter that owns the particle to test.
	 *  @param ParticleIndex Used to look up the particle in the emitter instance.
	 *  @returns TRUE if the particle was affected by one of the actions.
	 */
	UBOOL HandleParticleCollision(FParticleEmitterInstance* ParticleEmitter, INT ParticleIndex);
}

function OnSetWorldAttractorParam(SeqAct_SetWorldAttractorParam Action)
{
	if(Action.bEnabledField)
	{
		bEnabled = Action.bEnabled;
	}
	if(Action.bFalloffTypeField)
	{
		FalloffType = Action.FalloffType;
	}
	if(Action.bRangeField)
	{
		Range = Action.Range;
	}
	if(Action.bStrengthField)
	{
		Strength = Action.Strength;
	}
}

defaultproperties
{
	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_Pickup'
		HiddenGame=true
		HiddenEditor=false
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
	End Object
	Components.Add(Sprite)

	Begin Object Class=DistributionFloatConstant Name=DistributionFalloffExponent
	End Object
	FalloffExponent=(Distribution=DistributionFalloffExponent)

	Begin Object Class=DistributionFloatConstant Name=DistributionStrength
	End Object
	Strength=(Distribution=DistributionStrength)

	Begin Object Class=DistributionFloatConstant Name=DistributionRange
		Constant=1.0f
	End Object
	Range=(Distribution=DistributionRange)

	Begin Object Class=DistributionFloatConstant Name=DistributionDragCoefficient
		Constant=1.0f
	End Object
	DragCoefficient=(Distribution=DistributionDragCoefficient)

	Begin Object Class=DistributionFloatConstant Name=DistributionDragRadius
		Constant=0.0f
	End Object
	DragRadius=(Distribution=DistributionDragRadius)

	bHidden=FALSE

	bCollideActors=false

	FalloffType=FOFF_Constant;

	bEnabled=true;

	TickGroup=TG_PreAsyncWork;

	CurrentTime=0.0f;

	LoopDuration=0.0f;
}
