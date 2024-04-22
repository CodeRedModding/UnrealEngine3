/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleTypeDataBeam extends ParticleModuleTypeDataBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);

enum EBeamMethod
{
	PEBM_Distance,
    PEBM_EndPoints,
    PEBM_EndPoints_Interpolated,
    PEBM_UserSet_EndPoints,
    PEBM_UserSet_EndPoints_Interpolated
};
var(Beam)					EBeamMethod				BeamMethod;

// Distance is only used if BeamMethod is Distance
var(Beam)					rawdistributionfloat	Distance;
// Default end-point to use...
var(Beam)					rawdistributionvector	EndPoint;
var(Beam)					int						TessellationFactor;
var(Beam)					rawdistributionfloat	EmitterStrength;
var(Beam)					rawdistributionfloat	TargetStrength;

enum EBeamEndPointMethod
{
	PEBEPM_Calculated,
	PEBEPM_Distribution,
	PEBEPM_Distribution_Constant
};
var(Beam)					EBeamEndPointMethod		EndPointMethod;
var(Beam)					rawdistributionvector	EndPointDirection;

// Texture settings
var(Beam)					int						TextureTile;

//@todo. Remove these once finialized - START
var(Beam)					bool					RenderGeometry;
var(Beam)					bool					RenderDirectLine;
var(Beam)					bool					RenderLines;
var(Beam)					bool					RenderTessellation;
//@todo. Remove these once finialized - END

cpptext
{
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
	virtual void	PreUpdate(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);

	FVector			DetermineEndPointPosition(FParticleEmitterInstance* Owner, FLOAT DeltaTime);
	FVector			DetermineParticlePosition(FParticleEmitterInstance* Owner, FBaseParticle* pkParticle, FLOAT DeltaTime);

	virtual UINT	RequiredBytes(FParticleEmitterInstance* Owner = NULL);

	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent);

	virtual void	PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true

	TessellationFactor=1

	Begin Object Class=DistributionFloatConstant Name=DistributionDistance
	End Object
	Distance=(Distribution=DistributionDistance)

	Begin Object Class=DistributionVectorConstant Name=DistributionEndPoint
	End Object
	EndPoint=(Distribution=DistributionEndPoint)

	Begin Object Class=DistributionFloatConstant Name=DistributionEmitterStrength
		Constant=1000.0
	End Object
	EmitterStrength=(Distribution=DistributionEmitterStrength)
	
	Begin Object Class=DistributionFloatConstant Name=DistributionTargetStrength
		Constant=1000.0
	End Object
	TargetStrength=(Distribution=DistributionTargetStrength)

	EndPointMethod=PEBEPM_Calculated

	Begin Object Class=DistributionVectorConstant Name=DistributionEndPointDirection
		Constant=(X=1,Y=0,Z=0)
	End Object
	EndPointDirection=(Distribution=DistributionEndPointDirection)

//@todo. Remove these once finialized - START
	RenderGeometry=true
	RenderDirectLine=false
	RenderLines=false
	RenderTessellation=false
//@todo. Remove these once finialized - END
}
