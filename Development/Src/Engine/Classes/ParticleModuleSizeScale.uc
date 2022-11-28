class ParticleModuleSizeScale extends ParticleModuleSizeBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);

var() editinlinenotify export noclear	distributionvector	SizeScale;
var()									bool				EnableX;
var()									bool				EnableY;
var()									bool				EnableZ;

cpptext
{
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
}

defaultproperties
{
	bSpawnModule=false
	bUpdateModule=true

	EnableX=true
	EnableY=true
	EnableZ=true

	Begin Object Class=DistributionVectorConstant Name=DistributionSizeScale
	End Object
	SizeScale=DistributionSizeScale
}