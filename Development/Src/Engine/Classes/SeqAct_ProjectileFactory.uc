/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_ProjectileFactory extends SeqAct_ActorFactory
	native(Sequence);

cpptext
{
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	virtual UBOOL UpdateOp(FLOAT DeltaTime);
	virtual void DeActivated();

	virtual void Spawned(UObject *NewSpawn);
};

/**
 *	The particle system to spawn as the muzzle flash.
 */
var(MuzzleFlash)	ParticleSystem		PSTemplate;

/** 
 *	The name of the socket to spawn the muzzle flash at.
 *	If set, takes precedence over the bone name.
 */
var(MuzzleFlash)	Name				SocketName;

/** 
 *	The name of the bone to spawn the muzzle flash at.
 *	If SocketName is set, it takes precedence over this.
 */
var(MuzzleFlash)	Name				BoneName;

/**
 * Return the version number for this class.  Child classes should increment this method by calling Super then adding
 * a individual class version to the result.  When a class is first created, the number should be 0; each time one of the
 * link arrays is modified (VariableLinks, OutputLinks, InputLinks, etc.), the number that is added to the result of
 * Super.GetObjClassVersion() should be incremented by 1.
 *
 * @return	the version number for this specific class.
 */
static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 0;
};

defaultproperties
{
	ObjName="Projectile Factory"
	ObjCategory="Actor"

	InputLinks(0)=(LinkDesc="Spawn Projectile")
// 	InputLinks(1)=(LinkDesc="Enable")
// 	InputLinks(2)=(LinkDesc="Disable")
// 	InputLinks(3)=(LinkDesc="Toggle")

// 	VariableLinks.Empty
// 	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Spawn Point",PropertyName=SpawnPoints)
// 	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Spawned",MinVars=0,bWriteable=true)
// 	VariableLinks(2)=(ExpectedType=class'SeqVar_Int',LinkDesc="Spawn Count",PropertyName=SpawnCount)
// 	VariableLinks(3)=(ExpectedType=class'SeqVar_Vector',LinkDesc="Spawn Location",PropertyName=SpawnLocations)
// 	VariableLinks(4)=(ExpectedType=class'SeqVar_Vector',LinkDesc="Spawn Direction",PropertyName=SpawnOrientations)

// 	SpawnCount=1
 	SpawnDelay=0.0f
// 	bCheckSpawnCollision=TRUE
// 	LastSpawnIdx=-1
// 	bEnabled=TRUE
}
