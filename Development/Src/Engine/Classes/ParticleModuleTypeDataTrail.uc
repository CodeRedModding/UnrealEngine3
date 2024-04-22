/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleTypeDataTrail extends ParticleModuleTypeDataBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);

//@todo. Remove these once trails are finialized - START
var(Trail)	bool								RenderGeometry;
var(Trail)	bool								RenderLines;
var(Trail)	bool								RenderTessellation;
//@todo. Remove these once trails are finialized - END

var(Trail)	bool								Tapered;
var(Trail)	int									TessellationFactor;
var(Trail)	rawdistributionfloat				Tension;

var(Trail)	bool								SpawnByDistance;
var(Trail)	vector								SpawnDistance;

cpptext
{
	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
}

defaultproperties
{
	Tapered=false
	TessellationFactor=1

	Begin Object Class=DistributionFloatConstant Name=DistributionTension
		Constant=0.0
	End Object
	Tension=(Distribution=DistributionTension)

	SpawnByDistance=false
	SpawnDistance=(X=5.0,Y=5.0,Z=5.0)
//@todo. Remove these once trails are finialized - START
	RenderGeometry=true
	RenderLines=false
	RenderTessellation=false
//@todo. Remove these once trails are finialized - END
}
