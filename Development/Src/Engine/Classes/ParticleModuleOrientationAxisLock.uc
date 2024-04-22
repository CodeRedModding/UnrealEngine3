/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleOrientationAxisLock extends ParticleModuleOrientationBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

// Flags indicating lock
enum EParticleAxisLock
{
	/** No locking to an axis...							*/
	EPAL_NONE,
	/** Lock the sprite facing towards the positive X-axis	*/
	EPAL_X,
	/** Lock the sprite facing towards the positive Y-axis	*/
	EPAL_Y,
	/** Lock the sprite facing towards the positive Z-axis	*/
	EPAL_Z,
	/** Lock the sprite facing towards the negative X-axis	*/
	EPAL_NEGATIVE_X,
	/** Lock the sprite facing towards the negative Y-axis	*/
	EPAL_NEGATIVE_Y,
	/** Lock the sprite facing towards the negative Z-axis	*/
	EPAL_NEGATIVE_Z,
	/** Lock the sprite rotation on the X-axis				*/
	EPAL_ROTATE_X,
	/** Lock the sprite rotation on the Y-axis				*/
	EPAL_ROTATE_Y,
	/** Lock the sprite rotation on the Z-axis				*/
	EPAL_ROTATE_Z
};

/** The lock axis flag setting.
 *	Can be one of the following:
 *		EPAL_NONE			No locking to an axis.
 *		EPAL_X				Lock the sprite facing towards +X.
 *		EPAL_Y				Lock the sprite facing towards +Y.
 *		EPAL_Z				Lock the sprite facing towards +Z.
 *		EPAL_NEGATIVE_X		Lock the sprite facing towards -X.
 *		EPAL_NEGATIVE_Y		Lock the sprite facing towards -Y.
 *		EPAL_NEGATIVE_Z		Lock the sprite facing towards -Z.
 *		EPAL_ROTATE_X		Lock the sprite rotation on the X-axis.
 *		EPAL_ROTATE_Y		Lock the sprite rotation on the Y-axis.
 *		EPAL_ROTATE_Z		Lock the sprite rotation on the Z-axis.
 */
var(Orientation) EParticleAxisLock	LockAxisFlags;

cpptext
{
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
	virtual void	Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);

	virtual void	PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	virtual void	SetLockAxis(EParticleAxisLock eLockFlags);
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true
}
