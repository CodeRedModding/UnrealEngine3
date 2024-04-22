/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleSpawnPerUnit extends ParticleModuleSpawnBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

//=============================================================================
//	Properties
//=============================================================================
/** 
 *	The scalar to apply to the distance traveled.
 *	The value from SpawnPerUnit is divided by this value to give the actual
 *	number of particles per unit. 
 */
var(Spawn)	float						UnitScalar;
 
/** 
 *	The amount to spawn per meter distribution.
 *	The value is retrieved using the EmitterTime.
 */
var(Spawn)	rawdistributionfloat		SpawnPerUnit;

/**
 *	If TRUE, process the default spawn rate when not moving...
 *	When not moving, skip the default spawn rate.
 *	If FALSE, return the bProcessSpawnRate setting.
 */
var(Spawn)	bool						bIgnoreSpawnRateWhenMoving;

/**
 *	The tolerance for moving vs. not moving w.r.t. the bIgnoreSpawnRateWhenMoving flag.
 *	Ie, if (DistanceMoved < (UnitScalar x MovementTolerance)) then consider it not moving.
 */
var(Spawn)	float						MovementTolerance;

/**
 *	The maximum valid movement for a single frame.
 *	If 0.0, then the check is not performed.
 *	Currently, if the distance moved between frames is greater than this
 *	then NO particles will be spawned.
 *	This is primiarily intended to cover cases where the PSystem is 
 *	attached to teleporting objects.
 */
var(Spawn)	float						MaxFrameDistance;

/**
 *	If the distance (in units) moved in a single frame is less than this value, the 
 *	frame is ignored. 
 *	If the distance (in units) moved in a single frame is greater than or equal to 
 *	this value, then the distance is applied to the tracked total distance moved.
 */
var(Spawn)	float						MinFrameDistance;


/** If TRUE, ignore the X-component of the movement */
var(Spawn)	bool						bIgnoreMovementAlongX;
/** If TRUE, ignore the Y-component of the movement */
var(Spawn)	bool						bIgnoreMovementAlongY;
/** If TRUE, ignore the Z-component of the movement */
var(Spawn)	bool						bIgnoreMovementAlongZ;

//=============================================================================
//	C++
//=============================================================================
cpptext
{
	/**
	 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *
	 *	@return UINT		The number fo bytes the module needs per emitter instance.
	 */
	virtual UINT	RequiredBytesPerInstance(FParticleEmitterInstance* Owner = NULL);

	/**
	 *	Retrieve the spawn amount this module is contributing.
	 *	Note that if multiple Spawn-specific modules are present, if any one
	 *	of them ignores the SpawnRate processing it will be ignored.
	 *
	 *	@param	Owner		The particle emitter instance that is spawning.
	 *	@param	Offset		The offset into the particle payload for the module.
	 *	@param	OldLeftover	The bit of timeslice left over from the previous frame.
	 *	@param	DeltaTime	The time that has expired since the last frame.
	 *	@param	Number		The number of particles to spawn. (OUTPUT)
	 *	@param	Rate		The spawn rate of the module. (OUTPUT)
	 *
	 *	@return	UBOOL		FALSE if the SpawnRate should be ignored.
	 *						TRUE if the SpawnRate should still be processed.
	 */
	virtual UBOOL GetSpawnAmount(FParticleEmitterInstance* Owner, INT Offset, FLOAT OldLeftover, 
		FLOAT DeltaTime, INT& Number, FLOAT& Rate);
}

//=============================================================================
//	Default properties
//=============================================================================
defaultproperties
{
	bSpawnModule=true
	bUpdateModule=false

	UnitScalar=50.0
	Begin Object Class=DistributionFloatConstant Name=RequiredDistributionSpawnPerUnit
		Constant=0.0
	End Object
	SpawnPerUnit=(Distribution=RequiredDistributionSpawnPerUnit)
	
	MovementTolerance=0.1
}
