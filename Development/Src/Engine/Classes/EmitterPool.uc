/**
 * this class manages a pool of ParticleSystemComponents
 * it is meant to be used for single shot effects spawned during gameplay
 * to reduce object overhead that would otherwise be caused by spawning dozens of Emitters
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class EmitterPool extends Actor
	native
	transient
	config(Game);

/** template to base pool components off of - should not be used for effects or attached to anything */
var protected ParticleSystemComponent PSCTemplate;

/** components currently in the pool */
var transient const array<ParticleSystemComponent> PoolComponents;
/** components currently active */
var transient array<ParticleSystemComponent> ActiveComponents;
/** maximum allowed active components - if this is greater than 0 and is exceeded, the oldest active effect is taken */
var int MaxActiveEffects;

/** option to log out the names of all active effects when the pool overflows */
var globalconfig bool bLogPoolOverflow;
var globalconfig bool bLogPoolOverflowList;

/** list of components that should be relative to an Actor */
struct native EmitterBaseInfo
{
	var ParticleSystemComponent PSC;
	var Actor Base;
	var vector RelativeLocation;
	var rotator RelativeRotation;
	var bool bInheritBaseScale;
};
var transient array<EmitterBaseInfo> RelativePSCs;

/**
 *	The amount of time to allow the SMC and MIC arrays to be beyond their ideals.
 */
var float				SMC_MIC_ReductionTime;
var transient float		SMC_MIC_CurrentReductionTime;

/**
 *	The ideal number of StaticMeshComponents and MaterialInstanceConstants.
 *	If their counts are greater than this for more than ReductionTime, then
 *	they will be chopped down to their respective settings.
 */
var int					IdealStaticMeshComponents;
var int					IdealMaterialInstanceConstants;

/** The free StaticMeshComponents used by emitters in this pool */
var private transient const array<StaticMeshComponent> FreeSMComponents;

/** The free MaterialInstanceConstants used by emitters in this pool */
var private transient const array<MaterialInstanceConstant> FreeMatInstConsts;

cpptext
{
	virtual void TickSpecial(FLOAT DeltaTime);
}

/** set to each pool component's finished delegate to return it to the pool
 * for custom lifetime PSCs, must be called manually when done with the component
 */
native function OnParticleSystemFinished(ParticleSystemComponent PSC);

/** Cleans up the pool components, removing any unused
 * 
 *  @param  bClearActive    If TRUE, clear active as well as inactive pool components
 */
native final function ClearPoolComponents(bool bClearActive = false);

/** internal - detaches the given PSC and returns it to the pool */
protected native final function ReturnToPool(ParticleSystemComponent PSC);

/** internal - moves the SMComponents from given PSC to the pool free list */
protected native final function FreeStaticMeshComponents(ParticleSystemComponent PSC);

/**
 *	internal - retrieves a SMComponent from the pool free list
 *
 *	@param	bCreateNewObject	If TRUE, create an SMC w/ the pool as its outer
 *
 *	@return	StaticMeshComponent	The SMC, NULL if none was available/created
 */
protected native final function StaticMeshComponent GetFreeStaticMeshComponent(bool bCreateNewObject = true);

/** internal - moves the MIConstants from given SMComponent to the pool free list */
protected native final function FreeMaterialInstanceConstants(StaticMeshComponent SMC);

/**
*	internal - retrieves a MaterialInstanceConstant from the pool free list
*
*	@param	bCreateNewObject			If TRUE, create an MIC w/ the pool as its outer
*
*	@return	MaterialInstanceConstant	The MIC, NULL if none was available/created
*/
protected native final function MaterialInstanceConstant GetFreeMatInstConsts(bool bCreateNewObject = true);

/** internal - helper for spawning functions
 * gets a component from the appropriate pool array (checks PerEmitterPools)
 * includes creating a new one if necessary as well as taking one from the active list if the max number active has been exceeded
 * @return the ParticleSystemComponent to use
 */
protected native final function ParticleSystemComponent GetPooledComponent(ParticleSystem EmitterTemplate, bool bAutoActivate);

/** plays the specified effect at the given location and rotation, taking a component from the pool or creating as necessary
 * @note: the component is returned so the caller can perform any additional modifications (parameters, etc),
 * 	but it shouldn't keep the reference around as the component will be returned to the pool as soon as the effect is complete
 * @param EmitterTemplate - particle system to create
 * @param SpawnLocation - location to place the effect in world space
 * @param SpawnRotation (opt) - rotation to place the effect in world space
 * @param AttachToActor (opt) - if specified, component will move along with this Actor
 * @param InInstigator (opt) - if specified and the particle system is lit, the new component will only share particle light environments with other components with matching instigators
 * @param MaxDLEPooledReuses (opt) - if specified, limits how many components can use the same particle light environment.  This is effectively a tradeoff between performance and particle lighting update rate.  
 * @param bInheritScaleFromBase (opt) - if TRUE scale from the base actor will be applied
 * @return the ParticleSystemComponent the effect will use
 */
native function ParticleSystemComponent SpawnEmitter(ParticleSystem EmitterTemplate, vector SpawnLocation, optional rotator SpawnRotation, optional Actor AttachToActor, optional Actor InInstigator, optional int MaxDLEPooledReuses, optional bool bInheritScaleFromBase);

/** spawns a particle system attached to a SkeletalMeshComponent instead of to another Actor or to nothing
 * as with SpawnEmitter(), the caller should avoid persistent references to the returned component as it will
 * get automatically reclaimed when the effect is complete
 * @note: if the owning Actor is destroyed before the effect completes, the ParticleSystemComponent will end up
 *	being marked pending kill and therefore eventually destroyed as well. The pool handles this gracefully,
 *	although it's obviously suboptimal.
 * @param EmitterTemplate - particle system to create
 * @param Mesh - mesh component to attach to
 * @param AttachPointName - bone or socket to attach to
 * @param bAttachToSocket (opt) - whether AttachPointName is a socket or bone name
 * @param RelativeLoc (opt) - offset from bone location to place the effect (not used when attaching to socket)
 * @param RelativeRot (opt) - offset from bone rotation to place the effect (not used when attaching to socket)
 * @return the ParticleSystemComponent the effect will use
 */
native function ParticleSystemComponent SpawnEmitterMeshAttachment( ParticleSystem EmitterTemplate, 
	SkeletalMeshComponent Mesh, name AttachPointName, optional bool bAttachToSocket, 
	optional vector RelativeLoc, optional rotator RelativeRot );

/** spawns a pooled component that has a custom lifetime (controlled by the caller)
 * the caller is responsible for attaching/detaching the component
 * as well as calling our OnParticleSystemFinished() function when it is done with the component
 * the pool may take the component back early - if it does, it will trigger the component's OnSystemFinished delegate
 * so the caller can guarantee that it will be triggered
 * @param EmitterTemplate - particle system to create
 * @param bSkipAutoActivate - if TRUE, do not autoactivate the component when retrieving it from the pool
 * @return the ParticleSystemComponent to use
 */
native function ParticleSystemComponent SpawnEmitterCustomLifetime(ParticleSystem EmitterTemplate, optional bool bSkipAutoActivate);

defaultproperties
{
	Begin Object Class=ParticleSystemComponent Name=ParticleSystemComponent0
 		AbsoluteTranslation=true
 		AbsoluteRotation=true
		SecondsBeforeInactive=0.0
	End Object
	PSCTemplate=ParticleSystemComponent0

	TickGroup=TG_DuringAsyncWork

	SMC_MIC_ReductionTime=2.5
	IdealStaticMeshComponents=250
	IdealMaterialInstanceConstants=250
}
