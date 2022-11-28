class ParticleModule extends Object
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object)
	abstract;

var		bool	bSpawnModule;
var		bool	bUpdateModule;

var		bool	bCurvesAsColor;

var(Cascade)	color	ModuleEditorColor;

var		array< class<ParticleEmitter> >	AllowedEmitterClasses;

var		bool	b3DDrawMode;
var		bool	bSupported3DDrawMode;

cpptext
{
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
	virtual INT RequiredBytes();

	// For Cascade
	void GetCurveObjects( TArray<UObject*>& OutCurves );
	void AddModuleCurvesToEditor( UInterpCurveEdSetup* EdSetup );
	void RemoveModuleCurvesFromEditor( UInterpCurveEdSetup* EdSetup );
	UBOOL ModuleHasCurves();
	UBOOL IsDisplayedInCurveEd(  UInterpCurveEdSetup* EdSetup );

	virtual void Render3DPreview(FParticleEmitterInstance* Owner, const FSceneContext& Context,FPrimitiveRenderInterface* PRI)	{};
}

defaultproperties
{
	AllowedEmitterClasses.Add(class'Engine.ParticleEmitter')
	bSupported3DDrawMode = false;
	b3DDrawMode = false;
}
