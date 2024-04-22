/**
 *	ParticleModuleTypeDataRibbon
 *	Provides the base data for ribbon (drop trail) emitters.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleTypeDataRibbon extends ParticleModuleTypeDataBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

//*****************************************************************************
// General Trail Variables
//*****************************************************************************
/**
 *	The maximum amount to tessellate between two particles of the trail. 
 *	Depending on the distance between the particles and the tangent change, the 
 *	system will select a number of tessellation points 
 *		[0..MaxTessellationBetweenParticles]
 */
var		int		MaxTessellationBetweenParticles;

/**
 *	The number of sheets to render for the trail.
 */
var(Trail)		int		SheetsPerTrail;

/** The number of live trails									*/
var(Trail)		int		MaxTrailCount;

/** Max particles per trail										*/
var(Trail)		int		MaxParticleInTrailCount;

/** 
 *	If true, when the system is deactivated, mark trails as dead.
 *	This means they will still render, but will not have more particles
 *	added to them, even if the system re-activates...
 */
var(Trail)		bool 	bDeadTrailsOnDeactivate;

/**
 *	If true, when the source of a trail is 'lost' (ie, the source particle
 *	dies), mark the current trail as dead.
 */
var(Trail)		bool	bDeadTrailsOnSourceLoss;

/** If true, do not join the trail to the source position 		*/
var(Trail)		bool 	bClipSourceSegement;

/** If true, recalculate the previous tangent when a new particle is spawned */
var(Trail)		bool 	bEnablePreviousTangentRecalculation;

/** If true, recalculate tangents every frame to allow velocity/acceleration to be applied */
var(Trail)		bool 	bTangentRecalculationEveryFrame;

/** If true, ribbon will spawn a particle when it first starts moving */
var(Trail)		bool	bSpawnInitialParticle;

enum ETrailsRenderAxisOption
{
	Trails_CameraUp,
	Trails_SourceUp,
	Trails_WorldUp
};
/** 
 *	The 'render' axis for the trail (what axis the trail is stretched out on)
 *		Trails_CameraUp - Traditional camera-facing trail.
 *		Trails_SourceUp - Use the up axis of the source for each spawned particle.
 *		Trails_WorldUp  - Use the world up axis.
 */
var(Trail)	ETrailsRenderAxisOption	RenderAxis;

//*************************************************************************************************
// Trail Spawning Variables
//*************************************************************************************************
/**
 *	The tangent scalar for spawning.
 *	Angles between tangent A and B are mapped to [0.0f .. 1.0f]
 *	This is then multiplied by TangentTessellationScalar to give the number of particles to spawn
 */
var(Spawn)		float	TangentSpawningScalar;

//*************************************************************************************************
// Trail Rendering Variables
//*************************************************************************************************
/** If TRUE, render the trail geometry (this should typically be on) */
var(Rendering)	bool	bRenderGeometry;
/** If TRUE, render stars at each spawned particle point along the trail */
var(Rendering)	bool	bRenderSpawnPoints;
/** If TRUE, render a line showing the tangent at each spawned particle point along the trail */
var(Rendering)	bool	bRenderTangents;
/** If TRUE, render the tessellated path between spawned particles */
var(Rendering)	bool	bRenderTessellation;

/** 
 *	The (estimated) covered distance to tile the 2nd UV set at.
 *	If 0.0, a second UV set will not be passed in.
 */
var(Rendering)	float	TilingDistance;

/** 
 *	The distance step size for tessellation.
 *	# Tessellation Points = Trunc((Distance Between Spawned Particles) / DistanceTessellationStepSize))
 */
var(Rendering)	float	DistanceTessellationStepSize;
/** 
 *	If this flag is enabled, the system will scale the number of interpolated vertices
 *	based on the difference in the tangents of neighboring particles.
 *	Each pair of neighboring particles will compute the following CheckTangent value:
 *		CheckTangent = ((ParticleA Tangent DOT ParticleB Tangent) - 1.0f) * 0.5f
 *	If CheckTangent is LESS THAN 0.5, then the DistanceTessellationStepSize will be 
 *	scaled based on the result. This will map so that from parallel to orthogonal 
 *	(0..90 degrees) will scale from [0..1]. Anything greater than 90 degrees will clamp 
 *	at a scale of 1.
 */
var(Rendering)	bool	bEnableTangentDiffInterpScale;
/** 
 *	The tangent scalar for tessellation.
 *	Angles between tangent A and B are mapped to [0.0f .. 1.0f]
 *	This is then multiplied by TangentTessellationScalar to give the number of points to tessellate
 */
var(Rendering)	float	TangentTessellationScalar;

//*************************************************************************************************
// C++ Text
//*************************************************************************************************
cpptext
{
	virtual UINT	RequiredBytes(FParticleEmitterInstance* Owner = NULL);
	virtual void	PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent);
}

//*************************************************************************************************
// Default properties
//*************************************************************************************************
defaultproperties
{
	MaxTessellationBetweenParticles=25
	SheetsPerTrail=1
	MaxTrailCount=1
	MaxParticleInTrailCount=500
	bDeadTrailsOnDeactivate=true
	bClipSourceSegement=true
	bEnablePreviousTangentRecalculation=true
	bTangentRecalculationEveryFrame=false
	bDeadTrailsOnSourceLoss=true

	// Spawning
	TangentSpawningScalar=0.0f

	// Rendering
	bRenderGeometry=true
	bRenderSpawnPoints=false
	bRenderTangents=false
	bRenderTessellation=false
	DistanceTessellationStepSize=15.0f
	TangentTessellationScalar=5.0f
}
