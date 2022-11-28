class ParticleEmitter extends Object
	native(Particle)
	collapsecategories		
	hidecategories(Object)
	editinlinenew
	abstract;

var(Particle)	name														EmitterName;

var				editinline export					array<ParticleModule>	Modules;
// Associates module with offset into particle structure where module specific data is stored.
//TMap<UParticleModule*,UINT>												ModuleOffsetMap;

// Module<SINGULAR> used for emitter type "extension".
var				export								ParticleModule			TypeDataModule;
// Modules used for initialization.
var				native								array<ParticleModule>	SpawnModules;
// Modules used for ticking.
var				native								array<ParticleModule>	UpdateModules;

var(Particle)										bool					UseLocalSpace;
var(Particle)	editinlinenotify export noclear		distributionfloat		SpawnRate;

var(Cascade)		enum EEmitterRenderMode
{
	ERM_Normal,
	ERM_Point,
	ERM_Cross,
	ERM_None
} EmitterRenderMode;

var(Particle)	float	EmitterDuration;
var(Particle)	int		EmitterLoops; // 0 indicates loop continuously

var(Cascade)	color	EmitterEditorColor;

var				bool	ConvertedModules;
var				int		PeakActiveParticles;

cpptext
{
	virtual void PostEditChange(UProperty* PropertyThatChanged);
	virtual FParticleEmitterInstance* CreateInstance( UParticleSystemComponent* InComponent );
		
	virtual void SetToSensibleDefaults() {}

	virtual DWORD GetLayerMask() const { return PLM_Opaque; }

	virtual void PostLoad();
	virtual void UpdateModuleLists();

	void SetEmitterName(FName& Name);
	FName& GetEmitterName();
	
	// For Cascade
	void AddEmitterCurvesToEditor( UInterpCurveEdSetup* EdSetup );
	void RemoveEmitterCurvesFromEditor( UInterpCurveEdSetup* EdSetup );
}

defaultproperties
{
	EmitterName="Particle Emitter"
	
	EmitterRenderMode=ERM_Normal
	
	ConvertedModules=true
	PeakActiveParticles=0;

	EmitterDuration=1.0
	EmitterLoops=0

	EmitterEditorColor=(R=0,G=150,B=150)
	
	Begin Object Class=DistributionFloatConstant Name=DistributionSpawnRate
	End Object
	SpawnRate=DistributionSpawnRate
}