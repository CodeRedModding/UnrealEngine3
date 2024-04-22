/**
 *	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class ParticleModuleRequired extends ParticleModule
	native(Particle)
	editinlinenew
	hidecategories(Object,Cascade);

//=============================================================================
//	General
//=============================================================================
/** The material to utilize for the emitter at this LOD level.						*/
var(Emitter)						MaterialInterface			Material;
/** 
 *	The screen alignment to utilize for the emitter at this LOD level.
 *	One of the following:
 *	PSA_Square			- Uniform scale (via SizeX) facing the camera
 *	PSA_Rectangle		- Non-uniform scale (via SizeX and SizeY) facing the camera
 *	PSA_Velocity		- Orient the particle towards both the camera and the direction 
 *						  the particle is moving. Non-uniform scaling is allowed.
 *	PSA_TypeSpecific	- Use the alignment method indicated int he type data module.
 */
var(Emitter)						EParticleScreenAlignment	ScreenAlignment;

/** If TRUE, negative size values will flip the sprite image on that axis */
var(Emitter)						bool						bAllowImageFlipping;
/**
 *	If bAllowImageFlipping is enabled, and the alignment mode is square, than when
 *	TRUE, a -X size value will flip both the U and V. When FALSE, the sign of the
 *	Y size will still determine image flipping on V.
 */
var(Emitter)						bool						bSquareImageFlipping;

/** If TRUE, update the emitter in local space										*/
var(Emitter)						bool						bUseLocalSpace;
/** If TRUE, kill the emitter when the particle system is deactivated				*/
var(Emitter)						bool						bKillOnDeactivate;
/** If TRUE, kill the emitter when it completes										*/
var(Emitter)						bool						bKillOnCompleted;
/** Whether this emitter requires sorting as specified by artist.					*/
var deprecated bool bRequiresSorting;

enum EParticleSortMode
{
	PSORTMODE_None,
	PSORTMODE_ViewProjDepth,
	PSORTMODE_DistanceToView,
	PSORTMODE_Age_OldestFirst,
	PSORTMODE_Age_NewestFirst
};

/**
 *	The sorting mode to use for this emitter.
 *	PSORTMODE_None				- No sorting required.
 *	PSORTMODE_ViewProjDepth		- Sort by view projected depth of the particle.
 *	PSORTMODE_DistanceToView	- Sort by distance of particle to view in world space.
 *	PSORTMODE_Age_OldestFirst	- Sort by age, oldest drawn first.
 *	PSORTMODE_Age_NewestFirst	- Sort by age, newest drawn first.
 *
 */
var(Emitter) EParticleSortMode		SortMode;

/**
 *	If TRUE, the EmitterTime for the emitter will be calculated by
 *	modulating the SecondsSinceCreation by the EmitterDuration. As
 *	this can lead to issues w/ looping and variable duration, a new
 *	approach has been implemented. 
 *	If FALSE, this new approach is utilized, and the EmitterTime is
 *	simply incremented by DeltaTime each tick. When the emitter 
 *	loops, it adjusts the EmitterTime by the current EmitterDuration
 *	resulting in proper looping/delay behavior.
 */
var(Emitter) bool					bUseLegacyEmitterTime;

/** Whether to enable particles' culling for near distance. Works only with sprite/SubUV emitter. */
var(DistanceCulling)	bool		bEnableNearParticleCulling;

/** Whether to enable particles' culling for far distance. Works only with sprite/SubUV emitter. */
var(DistanceCulling)	bool		bEnableFarParticleCulling;

/** Near cull distance. Alpha=0.0f, NearCullDistance < NearFadeDistance. Particles closer than this distance are almost free to rendering by GPU. */
var(DistanceCulling)	float		NearCullDistance;

/** Near fade distance. Alpha=Initial. Fade transition from NearCullDistance. */
var(DistanceCulling)	float		NearFadeDistance;

/** Far fade distance. Alpha=Initial. Fade transition to FarCullDistance. */
var(DistanceCulling)	float		FarFadeDistance;

/** Far cull distance. Alpha=0.0f, FarFadeDistance < FarCullDistance. Particles further than this distance are almost free to rendering by GPU. */
var(DistanceCulling)	float		FarCullDistance;

/** 
 *	How long, in seconds, the emitter will run before looping.
 *	If set to 0, the emitter will never loop.
 */
var(Duration)						float						EmitterDuration;
/** 
 *	The low end of the emitter duration if using a range.
 */
var(Duration)						float						EmitterDurationLow;
/**
 *	If TRUE, select the emitter duration from the range 
 *		[EmitterDurationLow..EmitterDuration]
 */
var(Duration)						bool						bEmitterDurationUseRange;
/** 
 *	If TRUE, recalculate the emitter duration on each loop.
 */
var(Duration)						bool						bDurationRecalcEachLoop;

/** The number of times to loop the emitter.
 *	0 indicates loop continuously
 */
var(Duration)						int							EmitterLoops;

//=============================================================================
//	Spawn-related
//=============================================================================
/** The rate at which to spawn particles									*/
var							rawdistributionfloat		SpawnRate;

//=============================================================================
//	Burst-related
//=============================================================================
/** The method to utilize when burst-emitting particles						*/
var							EParticleBurstMethod		ParticleBurstMethod;

/** The array of burst entries.												*/
var		export noclear		array<ParticleBurst>		BurstList;

//=============================================================================
//	Delay-related
//=============================================================================
/**
 *	Indicates the time (in seconds) that this emitter should be delayed in the particle system.
 */
var(Delay)							float						EmitterDelay;
/** 
 *	The low end of the emitter delay if using a range.
 */
var(Delay)						float						EmitterDelayLow;
/**
 *	If TRUE, select the emitter delay from the range 
 *		[EmitterDelayLow..EmitterDelay]
 */
var(Delay)						bool						bEmitterDelayUseRange;
/**
 *	If TRUE, the emitter will be delayed only on the first loop.
 */
var(Delay)							bool						bDelayFirstLoopOnly;

//=============================================================================
//	SubUV-related
//=============================================================================
/** 
 *	The interpolation method to used for the SubUV image selection.
 *	One of the following:
 *	PSUVIM_None			- Do not apply SubUV modules to this emitter. 
 *	PSUVIM_Linear		- Smoothly transition between sub-images in the given order, 
 *						  with no blending between the current and the next
 *	PSUVIM_Linear_Blend	- Smoothly transition between sub-images in the given order, 
 *						  blending between the current and the next 
 *	PSUVIM_Random		- Pick the next image at random, with no blending between 
 *						  the current and the next 
 *	PSUVIM_Random_Blend	- Pick the next image at random, blending between the current 
 *						  and the next 
 */
var(SubUV)							EParticleSubUVInterpMethod	InterpolationMethod;

/** The number of sub-images horizontally in the texture							*/
var(SubUV)							int							SubImages_Horizontal;

/** The number of sub-images vertically in the texture								*/
var(SubUV)							int							SubImages_Vertical;

/** Whether to scale the UV or not - ie, the model wasn't setup with sub uvs		*/
var(SubUV)							bool						bScaleUV;

/**
 *	The amount of time (particle-relative, 0.0 to 1.0) to 'lock' on a random sub image
 *	    0.0 = change every frame
 *      1.0 = select a random image at spawn and hold for the life of the particle
 */
var									float						RandomImageTime;

/** The number of times to change a random image over the life of the particle.		*/
var(SubUV)							int							RandomImageChanges;

/** SUB-UV RELATIVE INTERNAL MEMBERS												*/
var									bool						bDirectUV;


/** Override the system MacroUV settings                                            */
var(MacroUV)                       	bool                        bOverrideSystemMacroUV; 

/** Local space position that UVs generated with the ParticleMacroUV material node will be centered on. */
var(MacroUV) 						vector 						MacroUVPosition; 

/** World space radius that UVs generated with the ParticleMacroUV material node will tile based on. */
var(MacroUV) 						float 						MacroUVRadius; 

/**
 *	If TRUE, use the MaxDrawCount to limit the number of particles rendered.
 *	NOTE: This does not limit the number spawned/updated, only what is drawn.
 */
var(Rendering)						bool						bUseMaxDrawCount;
/**
 *	The maximum number of particles to DRAW for this emitter.
 *	If set to 0, it will use whatever number are present.
 */
var(Rendering)						int							MaxDrawCount;

/**
 * Fraction of the screen that the particle system's bounds must be larger than for the emitter to be rendered downsampled.
 * The default is 0, which means downsampling is not allowed.  
 * A value of .5 means that the particle system's bounds must take up half of the screen or more before the emitter will be rendered at a lower resolution.
 * Downsampled translucency renders significantly faster than full resolution, except that there is a fairly large constant overhead for every emitter that is downsampled.
 * For this reason, it's best to only use downsampling on emitters that are known to have a fillrate cost larger than the constant overhead.  
 * A value of .5 is usually a good tradeoff when downsampling is desired.
 * The quality of downsampled translucency is also affected, high frequency details will be lost and opaque edges in front of the translucency will appear more aliased.
 * Note: This functionality uses the bounding radius so it's important that the particle system's bounds are accurate, use bUseFixedRelativeBoundingBox if necessary.
 */
var									float						DownsampleThresholdScreenFraction;

enum EEmitterNormalsMode
{
	/** Default mode, normals are based on the camera facing geometry. */
	ENM_CameraFacing,
	/** Normals are generated from a sphere centered at NormalsSphereCenter. */
	ENM_Spherical,
	/** Normals are generated from a cylinder going through NormalsSphereCenter, in the direction NormalsCylinderDirection. */
	ENM_Cylindrical
};

/** Normal generation mode for this emitter LOD. */
var(Normals)						EEmitterNormalsMode			EmitterNormalsMode;
/** 
 * When EmitterNormalsMode is ENM_Spherical, particle normals are created to face away from NormalsSphereCenter. 
 * NormalsSphereCenter is in local space.
 */
var(Normals)						vector						NormalsSphereCenter;
/** 
 * When EmitterNormalsMode is ENM_Cylindrical, 
 * particle normals are created to face away from the cylinder going through NormalsSphereCenter in the direction NormalsCylinderDirection. 
 * NormalsCylinderDirection is in local space.
 */
var(Normals)						vector						NormalsCylinderDirection;


/**
 * Ensures that movement generated from the orbit module is applied to velocity-aligned particles
 */
var(Emitter)						bool						bOrbitModuleAffectsVelocityAlignment;

//=============================================================================
//	C++
//=============================================================================
cpptext
{
	virtual void	PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void	PostLoad();

	/**
	 *	Called when the module is created, this function allows for setting values that make
	 *	sense for the type of emitter they are being used in.
	 *
	 *	@param	Owner			The UParticleEmitter that the module is being added to.
	 */
	virtual void SetToSensibleDefaults(UParticleEmitter* Owner);

	/** 
	 *	Add all curve-editable Objects within this module to the curve editor.
	 *
	 *	@param	EdSetup		The CurveEd setup to use for adding curved.
	 */
	virtual	void	AddModuleCurvesToEditor(UInterpCurveEdSetup* EdSetup)
	{
		// Overide the base implementation to prevent old SpawnRate from being added...
	}

	/**
	 *	Retrieve the ModuleType of this module.
	 *
	 *	@return	EModuleType		The type of module this is.
	 */
	virtual EModuleType	GetModuleType() const	{	return EPMT_Required;	}

	virtual UBOOL	GenerateLODModuleValues(UParticleModule* SourceModule, FLOAT Percentage, UParticleLODLevel* LODLevel);
}

//=============================================================================
//	Default properties
//=============================================================================
defaultproperties
{
	bSpawnModule=TRUE
	bUpdateModule=TRUE

	EmitterDuration=1.0
	EmitterDurationLow=0.0
	bEmitterDurationUseRange=FALSE

	EmitterDelay=0.0
	EmitterDelayLow=0.0
	bEmitterDelayUseRange=FALSE

	EmitterLoops=0

	Begin Object Class=DistributionFloatConstant Name=RequiredDistributionSpawnRate
	End Object
	SpawnRate=(Distribution=RequiredDistributionSpawnRate)

	SubImages_Horizontal=1
	SubImages_Vertical=1

	bUseMaxDrawCount=TRUE
	MaxDrawCount=500

	LODDuplicate=TRUE

	NormalsSphereCenter=(x=0,y=0,z=100)
	NormalsCylinderDirection=(x=0,y=0,z=1)

	bUseLegacyEmitterTime=TRUE

	bAllowImageFlipping=false
	bSquareImageFlipping=false

	bEnableNearParticleCulling=false
	NearCullDistance=10.0
	NearFadeDistance=150.0

	bEnableFarParticleCulling=false
	FarFadeDistance=2000.0
	FarCullDistance=2100.0
}
