/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleAttractorSkelVertSurface extends ParticleModuleAttractorBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/**
 *  Falloff type enumeration.
 */
enum EVertSurfaceAttractorFalloffType
{
	VSFOFF_Constant,
	VSFOFF_Linear,
	VSFOFF_Exponent
};

/**
 *  Type of falloff.
 *
 *  FOFF_Constant - Falloff is constant so just use the strength for the whole range.
 *  FOFF_Linear - Linear falloff over the range.
 *  FOFF_Exponent - Exponential falloff over the range.
 */
var() EVertSurfaceAttractorFalloffType FalloffType;

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

enum EAttractorSkelVertSurfaceDestination
{
	VERTSURFACEDEST_Vert,
	VERTSURFACEDEST_Surface
};

/**
 *	Whether the module uses Verts or Surfaces for locations.
 *
 *	VERTSURFACEDEST_Vert		- Use Verts as the destination locations.
 *	VERTSURFACEDEST_Surface		- Use Surfaces as the destination locations.
 */
var(VertSurface)	EAttractorSkelVertSurfaceDestination	DestinationType;

/** An offset to apply to each vert/surface */
var(VertSurface)	vector	UniversalOffset;

/**
 *	The parameter name of the skeletal mesh actor that supplies the SkelMeshComponent for in-game.
 */
var(VertSurface)	name	SkelMeshActorParamName;

/** The name of the skeletal mesh to use in the editor */
var(VertSurface)	editoronly	SkeletalMesh	EditorSkelMesh;

/** This module will only spawn from verts or surfaces associated with the bones in this list */
var(VertSurface)	array<Name>	ValidAssociatedBones;

/** When TRUE use the RestrictToNormal and NormalTolerance values to check surface normals */
var(VertSurface)	bool	bEnforceNormalCheck;

/** Use this normal to restrict spawning locations */
var(VertSurface)	vector	NormalToCompare;

/** Normal tolerance.  0 degrees means it must be an exact match, 180 degrees means it can be any angle. */
var(VertSurface)	float	NormalCheckToleranceDegrees;

/** Normal tolerance.  Value between 1.0 and -1.0 with 1.0 being exact match, 0.0 being everything up to
    perpendicular and -1.0 being any direction or don't restrict at all. */
var					float	NormalCheckTolerance;

/**
 *	Array of material indices that are valid materials to spawn from.
 *	If empty, any material will be considered valid
 */
var(VertSurface)	array<int>	ValidMaterialIndices;

cpptext
{
	/**
	 *	Called after loading the module.
	 */
	virtual void PostLoad();

	/**
	 *	Called when a property has change on an instance of the module.
	 *
	 *	@param	PropertyChangedEvent		Information on the change that occurred.
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

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
	 *	Allows the module to prep its 'per-instance' data block.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *	@param	InstData	Pointer to the data block for this module.
	 */
	virtual UINT	PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData);

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
	 *	Retrieve the velocity and position for the given particle/socket combination.
	 *
	 *	@param	Owner					The particle emitter instance that is being setup
	 *	@param	InSkelMeshComponent		The skeletal mesh component to use as the source
	 *	@param	InPrimaryVertexIndex	The index of the primary vertice
	 *  @param  InCurrentLocation		The current location of the particle
	 *  @param  InCurrentTime			The current time (either particle or emitter depending on module settings)
	 *	@param	OutVelocityToAdd		The velocity to add for this particle
	 *	@param	OutDestinationLocation	Location of the bone or socket attracting the particle
	 *	
	 *	@return	UBOOL					TRUE if successful, FALSE if not
	 */
	UBOOL GetVelocityForAttraction(FParticleEmitterInstance* Owner, USkeletalMeshComponent* InSkelMeshComponent, INT InPrimaryVertexIndex,
		FVector& InCurrentLocation, FLOAT InCurrentTime, FVector& OutVelocityToAdd, FVector& OutDestinationLocation);
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

	DestinationType=VERTSURFACEDEST_Vert
	SkelMeshActorParamName="VertSurfaceActor"

	bEnforceNormalCheck=false
}
