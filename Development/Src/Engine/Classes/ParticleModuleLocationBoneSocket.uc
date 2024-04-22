/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleLocationBoneSocket extends ParticleModuleLocationBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

enum ELocationBoneSocketSource
{
	BONESOCKETSOURCE_Bones,
	BONESOCKETSOURCE_Sockets
};

/**
 *	Whether the module uses Bones or Sockets for locations.
 *
 *	BONESOCKETSOURCE_Bones		- Use Bones as the source locations.
 *	BONESOCKETSOURCE_Sockets	- Use Sockets as the source locations.
 */
var(BoneSocket)	ELocationBoneSocketSource	SourceType;

/** An offset to apply to each bone/socket */
var(BoneSocket)	vector	UniversalOffset;

struct native LocationBoneSocketInfo
{
	/** The name of the bone/socket on the skeletal mesh */
	var()	name	BoneSocketName;
	/** The offset from the bone/socket to use */
	var()	vector	Offset;
};

/** The name(s) of the bone/socket(s) to position at */
var(BoneSocket)	array<LocationBoneSocketInfo>	SourceLocations;

enum ELocationBoneSocketSelectionMethod
{
	BONESOCKETSEL_Sequential,
	BONESOCKETSEL_Random,
	BONESOCKETSEL_RandomExhaustive
};

/**
 *	The method by which to select the bone/socket to spawn at.
 *
 *	SEL_Sequential			- loop through the bone/socket array in order
 *	SEL_Random				- randomly select a bone/socket from the array
 *	SEL_RandomExhaustive	- randomly select a bone/socket, but never the same one twice until all have been used, then reset
 */
var(BoneSocket)	ELocationBoneSocketSelectionMethod	SelectionMethod;

/** If TRUE, update the particle locations each frame with that of the bone/socket */
var(BoneSocket)	bool	bUpdatePositionEachFrame;

/** If TRUE, rotate mesh emitter meshes to orient w/ the socket */
var(BoneSocket)	bool	bOrientMeshEmitters;

/**
 *	The parameter name of the skeletal mesh actor that supplies the SkelMeshComponent for in-game.
 */
var(BoneSocket)	name	SkelMeshActorParamName;

/** The name of the skeletal mesh to use in the editor */
var(BoneSocket)	editoronly	SkeletalMesh	EditorSkelMesh;

/** If TRUE, the spawned particle will have the velocity of the bone or socket at the spawn time. */
var(BoneSocket) bool	bInheritVelocityAtSpawn;

/** Store off the last update time so we can get a delta in spawn. */
var				float	LastUpdateTime;

cpptext
{
	/**
	 *	Called when a property has change on an instance of the module.
	 *
	 *	@param	PropertyChangedEvent		Information on the change that occurred.
	 */
	virtual void	PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

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
	 *	Return TRUE if this module impacts rotation of Mesh emitters
	 *	@return	UBOOL		TRUE if the module impacts mesh emitter rotation
	 */
	virtual UBOOL	TouchesMeshRotation() const	{ return TRUE; }

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
	 *	Retrieve the position for the given socket index.
	 *
	 *	@param	Owner					The particle emitter instance that is being setup
	 *	@param	InSkelMeshComponent		The skeletal mesh component to use as the source
	 *	@param	InBoneSocketIndex		The index of the bone/socket of interest
	 *	@param	OutPosition				The position for the particle location
	 *	@param	OutRotation				Optional orientation for the particle (mesh emitters)
	 *	
	 *	@return	UBOOL					TRUE if successful, FALSE if not
	 */
	UBOOL GetParticleLocation(FParticleEmitterInstance* Owner, USkeletalMeshComponent* InSkelMeshComponent, INT InBoneSocketIndex, FVector& OutPosition, FQuat* OutRotation);
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true
	bFinalUpdateModule=true

	bSupported3DDrawMode=true

	SourceType=BONESOCKETSOURCE_Sockets
	SkelMeshActorParamName="BoneSocketActor"
	bOrientMeshEmitters=true
}
