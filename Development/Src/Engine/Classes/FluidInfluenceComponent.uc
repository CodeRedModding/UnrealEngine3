/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class FluidInfluenceComponent extends PrimitiveComponent
	dependson(FluidSurfaceActor)
	native(Fluid)
	AutoExpandCategories(FluidInfluenceComponent)
	hidecategories(Object,Collision,Lighting,Physics,PrimitiveComponent,Rendering)
	editinlinenew;


enum EInfluenceType
{
	Fluid_Flow,
	Fluid_Raindrops,
	Fluid_Wave,
	Fluid_Sphere,
};



/** Whether the effect is active and applying forces to the fluid. */
var() bool bActive;

/** If a specific FluidSurfaceActor is set, this influence won't automatically affect any other fluid and MaxDistance is ignored. */
var() FluidSurfaceActor FluidActor;

/** Type of fluid influence (a flow of waves, raindrops, or a single wave). */
var() EInfluenceType InfluenceType;

/** Maximum distance (from the fluid plane) from where this influence will affect a fluid. */
var() float MaxDistance;


/** Strength of the influencing force. */
var(FluidWave) interp float WaveStrength;

/** Wave frequency (can be 0 for a standing wave). */
var(FluidWave) interp float WaveFrequency;

/** Angular phase, in 0-360 degrees. */
var(FluidWave) interp float WavePhase;

/** Radius of the wave, in world space units. */
var(FluidWave) interp float WaveRadius;



/** Whether raindrops should fill the entire fluid (TRUE), or just in a circular area around the influenceactor (FALSE). */
var(FluidRaindrops) interp bool RaindropFillEntireFluid;

/** Radius of the area where raindrops fall. */
var(FluidRaindrops) interp float RaindropAreaRadius;

/** Radius of each raindrop, in world space units. */
var(FluidRaindrops) interp float RaindropRadius;

/** Strength of each raindrop. */
var(FluidRaindrops) interp float RaindropStrength;

/** Number of raindrops per second. */
var(FluidRaindrops) interp float RaindropRate;



/** How fast the flow moves thru the fluid, in world space units per second. */
var(FluidFlow) interp float FlowSpeed;

/** Number of flow ripples generated on the fluid surface. */
var(FluidFlow) interp int FlowNumRipples;

/** How much each flow ripple should oscillate sideways while moving down that flow direction. */
var(FluidFlow) interp float FlowSideMotionRadius;

/** Radius of each flow wave, in world space units. */
var(FluidFlow) interp float FlowWaveRadius;

/** Strength of each wave ripple. */
var(FluidFlow) interp float FlowStrength;

/** Frequency of up/down and sideways motion of each ripple. */
var(FluidFlow) interp float FlowFrequency;


/** Outer radius of the 3D sphere. While inside this radius, the force will increase as it moves closer to the fluid plane. */
var(FluidSphere) interp float SphereOuterRadius;

/** Inner radius of the 3D sphere. While inside this radius, the force will decrease as it moves closer to the fluid plane. */
var(FluidSphere) interp float SphereInnerRadius;

/** Strength of the force applied by the sphere. */
var(FluidSphere) interp float SphereStrength;

/** The "toggle" Kismet event will set this to true, which will enable/disable the influence for 1 tick, then automatically go back to its previous state. */
var transient bool bIsToggleTriggered;

var private native transient float CurrentAngle;
var private native transient float CurrentTimer;

/** The currently affected FluidSurfaceActor. */
var private native transient FluidSurfaceActor CurrentFluidActor;


cpptext
{
public:
	// Base class interfaces
	virtual void	PostLoad( );
	virtual void	PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void	UpdateBounds( );
	virtual void	Tick( FLOAT DeltaTime );

	// Influence functions
	void			UpdateFlow( FLOAT DeltaSeconds );
	void			UpdateRaindrops( FLOAT DeltaSeconds );
	void			UpdateWave( FLOAT DeltaSeconds );
	void			UpdateSphere( FLOAT DeltaSeconds );
	void			CheckSettings( UBOOL bUpdateIcon );
	UBOOL			IsTouching( AFluidSurfaceActor* Fluid );
}



defaultproperties
{
	bActive=true
	InfluenceType=Fluid_Wave
	MaxDistance=1000.0

	WaveStrength=40.0
	WaveFrequency=1.0
	WavePhase=0.0
	WaveRadius=50.0

	RaindropFillEntireFluid=True
	RaindropAreaRadius=300.0
	RaindropRadius=10.0
	RaindropStrength=5.0
	RaindropRate=20.0

	FlowSpeed=100.0
	FlowNumRipples=10
	FlowSideMotionRadius=30.0
	FlowWaveRadius=50.0
	FlowStrength=20.0
	FlowFrequency=4.0
	
	SphereOuterRadius=100.0
	SphereInnerRadius=50.0
	SphereStrength=-40.0

	bTickInEditor=True
	CollideActors=False
	BlockZeroExtent=False
	BlockNonZeroExtent=False
	BlockRigidBody=False
}
