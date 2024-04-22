/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleLocationStaticVertSurface extends ParticleModuleLocationBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

enum ELocationStaticVertSurfaceSource
{
	VERTSTATICSURFACESOURCE_Vert,
	VERTSTATICSURFACESOURCE_Surface
};

/**
 *	Whether the module uses Verts or Surfaces for locations.
 *
 *	VERTSURFACESOURCE_Vert		- Use Verts as the source locations.
 *	VERTSURFACESOURCE_Surface	- Use Surfaces as the source locations.
 */
var(VertSurface)	ELocationStaticVertSurfaceSource	SourceType;

/** An offset to apply to each vert/surface */
var(VertSurface)	vector	UniversalOffset;

/** If TRUE, update the particle locations each frame with that of the vert/surface */
var(VertSurface)	bool	bUpdatePositionEachFrame;

/** If TRUE, rotate mesh emitter meshes to orient w/ the vert/surface */
var(VertSurface)	bool	bOrientMeshEmitters;

/**
 *	The parameter name of the static mesh actor that supplies the StaticMeshComponent for in-game.
 */
var(VertSurface)	name	StaticMeshActorParamName;

/** The name of the static mesh to use in the editor */
var(VertSurface)	editoronly	StaticMesh	EditorStaticMesh;

/** When TRUE use the RestrictToNormal and NormalTolerance values to check surface normals */
var(VertSurface)	bool	bEnforceNormalCheck;

/** Use this normal to restrict spawning locations */
var(VertSurface)	vector	NormalToCompare;

/** Normal tolerance.  0 degrees means it must be an exact match, 180 degrees means it can be any angle. */
var(VertSurface)	float			NormalCheckToleranceDegrees;

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

	/**
	 *	Retrieve the position for the given vert or face index.
	 *
	 *	@param	Owner					The particle emitter instance that is being setup
	 *	@param	InStaticMeshComponent	The static mesh component to use as the source
	 *	@param	InPrimaryVertexIndex	The index of the only vertex (vert mode) or the first vertex (surface mode)
	 *	@param	OutPosition				The position for the particle location
	 *	@param	OutRotation				Optional orientation for the particle (mesh emitters)
	 *  @param  bSpawning				When TRUE and when using normal check on surfaces, will return false if the check fails.
	 *	
	 *	@return	UBOOL					TRUE if successful, FALSE if not
	 */
	UBOOL GetParticleLocation(FParticleEmitterInstance* Owner, UStaticMeshComponent* InStaticMeshComponent, INT InPrimaryVertexIndex, FVector& OutPosition, FQuat* OutRotation, UBOOL bSpawning = FALSE);
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true
	bFinalUpdateModule=true

	bSupported3DDrawMode=true

	SourceType=VERTSTATICSURFACESOURCE_Vert
	StaticMeshActorParamName="VertSurfaceActor"
	bOrientMeshEmitters=true

	bEnforceNormalCheck=false
}
