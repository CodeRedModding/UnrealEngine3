/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleSubUVSelect extends ParticleModuleSubUVBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/**
 *	The horizontal (X) and vertical (Y) index of the sub-image desired.
 *	Value is retrieved using the RelativeTime of the particle.
 */
var(SubUV) rawdistributionvector	SubImageSelect;

cpptext
{
	virtual void Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);
}

defaultproperties
{
	bSpawnModule=false
	bUpdateModule=true

	Begin Object Class=DistributionVectorConstant Name=DistributionSubImageSelect
	End Object
	SubImageSelect=(Distribution=DistributionSubImageSelect)
}
