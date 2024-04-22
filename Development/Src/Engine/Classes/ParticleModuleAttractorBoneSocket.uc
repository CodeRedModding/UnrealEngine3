/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleAttractorBoneSocket extends ParticleModuleAttractorBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/**
 *  Falloff type enumeration.
 */
enum EBoneSocketAttractorFalloffType
{
	BSFOFF_Constant,
	BSFOFF_Linear,
	BSFOFF_Exponent
};

/**
 *  Type of falloff.
 *
 *  FOFF_Constant - Falloff is constant so just use the strength for the whole range.
 *  FOFF_Linear - Linear falloff over the range.
 *  FOFF_ExponentExponential falloff over the range.
 */
var() EBoneSocketAttractorFalloffType FalloffType;

/** When TRUE the attraction force is particle life relative, when FALSE it is emitter life relative. */
var() bool bParticleLifeRelative;

/**
 *  Optional falloff exponent for FOFF_Exponent type.
 */
var() interp rawdistributionfloat FalloffExponent;

/**
 *  Range of the attractor.
 */
var() interp rawdistributionfloat Range;

/**
 *  Strength of the attraction over time.
 */
var() interp rawdistributionfloat Strength;

/**
 *  Radius bounding the attraction point for use with collisions.
 */
var() interp rawdistributionfloat CollisionRadius;

/**
 *  Drag coefficient, a value of 1.0f means no drag, a value > 1.0f means accelerate.
 *  This value is multiplied with the DragCoefficient value in the attractor to get the
 *  resultant drag coefficient and generate the drag force.
 */
var() interp rawdistributionfloat DragCoefficient;

/**
 *  Apply the drag when the particle is within this radius.
 */
var() interp rawdistributionfloat DragRadius;

enum ELocationBoneSocketDestination
{
	BONESOCKETDEST_Bones,
	BONESOCKETDEST_Sockets
};

/**
 *	Whether the module uses Bones or Sockets for locations.
 *
 *	BONESOCKETSOURCE_Bones		- Use Bones as the source locations.
 *	BONESOCKETSOURCE_Sockets	- Use Sockets as the source locations.
 */
var(BoneSocket)	ELocationBoneSocketDestination	DestinationType;

/** Only applies to Bone destination type.  When TRUE, the particles will be attracted to points along the length of the bone. */
var(BoneSocket) bool bAttractAlongLengthOfBone;

/** An offset to apply to each bone/socket */
var(BoneSocket)	vector	UniversalOffset;

struct native AttractLocationBoneSocketInfo
{
	/** The name of the bone/socket on the skeletal mesh */
	var()	name	BoneSocketName;
	/** The offset from the bone/socket to use */
	var()	vector	Offset;
};

/** The name(s) of the bone/socket(s) to position at */
var(BoneSocket)	array<AttractLocationBoneSocketInfo>	SourceLocations;

enum ELocationBoneSocketDestSelectionMethod
{
	BONESOCKETDESTSEL_Sequential,
	BONESOCKETDESTSEL_Random,
	BONESOCKETDESTSEL_RandomExhaustive,
	BONESOCKETDESTSEL_BlendAll
};

/**
 *	The method by which to select the bone/socket to spawn at.
 *
 *	SEL_Sequential			- loop through the bone/socket array in order
 *	SEL_Random				- randomly select a bone/socket from the array
 *	SEL_RandomExhaustive	- randomly select a bone/socket, but never the same one twice until all have been used, then reset
 *  SEL_BlendAll			- weights every entry in the list equally and attracts to all of them simultaneously
 */
var(BoneSocket)	ELocationBoneSocketDestSelectionMethod	SelectionMethod;

// TODO: Orient meshes on entry
/** If TRUE, rotate mesh emitter meshes to orient w/ the socket */
//var(BoneSocket)	bool	bOrientMeshEmitters;

/**
 *	The parameter name of the skeletal mesh actor that supplies the SkelMeshComponent for in-game.
 */
var(BoneSocket)	name	SkelMeshActorParamName;

/** The name of the skeletal mesh to use in the editor */
var(BoneSocket)	editoronly	SkeletalMesh	EditorSkelMesh;

cpptext
{
	/**
	 *	Called on a particle that is freshly spawned by the emitter.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that spawned the particle.
	 *	@param	Offset		The modules offset into the data payload of the particle.
	 *	@param	SpawnTime	The time of the spawn.
	 */
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);

	/**
	 *	Called on a particle that is being updated by its emitter.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *	@param	Offset		The modules offset into the data payload of the particle.
	 *	@param	DeltaTime	The time since the last update.
	 */
	virtual void	Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);

	/**
	 *	Called on an emitter when all other update operations have taken place
	 *	INCLUDING bounding box cacluations!
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *	@param	Offset		The modules offset into the data payload of the particle.
	 *	@param	DeltaTime	The time since the last update.
	 */
	virtual void	FinalUpdate(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);

	/**
	 *	Returns the number of bytes that the module requires in the particle payload block.
	 *
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *
	 *	@return	UINT		The number of bytes the module needs per particle.
	 */
	virtual UINT	RequiredBytes(FParticleEmitterInstance* Owner = NULL);

	/**
	 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *
	 *	@return UINT		The number of bytes the module needs per emitter instance.
	 */
	virtual UINT	RequiredBytesPerInstance(FParticleEmitterInstance* Owner = NULL);

	/**
	 *	Allows the module to prep its 'per-instance' data block.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *	@param	InstData	Pointer to the data block for this module.
	 */
	virtual UINT	PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData);

	/**
	 *	Helper function used by the editor to auto-populate a placed AEmitter with any
	 *	instance parameters that are utilized.
	 *
	 *	@param	PSysComp		The particle system component to be populated.
	 */
	virtual void	AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp);

#if WITH_EDITOR
	/**
	 *	Get the number of custom entries this module has. Maximum of 3.
	 *
	 *	@return	INT		The number of custom menu entries
	 */
	virtual INT GetNumberOfCustomMenuOptions() const;

	/**
	 *	Get the display name of the custom menu entry.
	 *
	 *	@param	InEntryIndex		The custom entry index (0-2)
	 *	@param	OutDisplayString	The string to display for the menu
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE if not
	 */
	virtual UBOOL GetCustomMenuEntryDisplayString(INT InEntryIndex, FString& OutDisplayString) const;

	/**
	 *	Perform the custom menu entry option.
	 *
	 *	@param	InEntryIndex		The custom entry index (0-2) to perform
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE if not
	 */
	virtual UBOOL PerformCustomMenuEntry(INT InEntryIndex);
#endif

	/**
	 *	Retrieve the velocity and position for the given particle/socket combination.
	 *
	 *	@param	Owner					The particle emitter instance that is being setup
	 *	@param	InSkelMeshComponent		The skeletal mesh component to use as the source
	 *	@param	InBoneSocketIndex		The index of the bone/socket of interest
	 *  @param  InCurrentLocation		The current location of the particle
	 *  @param  InCurrentTime			The current time (either particle or emitter depending on module settings)
	 *  @param  InBoneLerpAlpha			The alpha value to lerp between the ends of a bone when bAttractAlongLengthOfBone is TRUE
	 *	@param	OutVelocityToAdd		The velocity to add for this particle
	 *	@param	OutDestinationLocation	Location of the bone or socket attracting the particle
	 *	
	 *	@return	UBOOL					TRUE if successful, FALSE if not
	 */
	UBOOL GetVelocityForAttraction(FParticleEmitterInstance* Owner, USkeletalMeshComponent* InSkelMeshComponent, INT InBoneSocketIndex,
		FVector& InCurrentLocation, FLOAT InCurrentTime, FLOAT InBoneLerpAlpha, FVector& OutVelocityToAdd, FVector& OutDestinationLocation);
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true
	bFinalUpdateModule=true

	bSupported3DDrawMode=true

	Begin Object Class=DistributionFloatConstant Name=DistributionFalloffExponent
	End Object
	FalloffExponent=(Distribution=DistributionFalloffExponent)

	Begin Object Class=DistributionFloatConstant Name=DistributionDragCoefficient
		Constant=1.0f
	End Object
	DragCoefficient=(Distribution=DistributionDragCoefficient)

	Begin Object Class=DistributionFloatConstant Name=DistributionDragRadius
		Constant=0.0f
	End Object
	DragRadius=(Distribution=DistributionDragRadius)

	Begin Object Class=DistributionFloatConstant Name=DistributionStrength
	End Object
	Strength=(Distribution=DistributionStrength)

	Begin Object Class=DistributionFloatConstant Name=DistributionRange
	End Object
	Range=(Distribution=DistributionRange)

	DestinationType=BONESOCKETDEST_Sockets
	SkelMeshActorParamName="BoneSocketActor"
	bAttractAlongLengthOfBone=false
}
