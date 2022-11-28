class ParticleSystemComponent extends PrimitiveComponent
	native(Particle)
	hidecategories(Object)
	hidecategories(Physics)
	hidecategories(Collision)
	hidecategories(Rendering)
	editinlinenew;

struct ParticleEmitterInstancePointer
{
	var native const pointer P;
};

var()				const	ParticleSystem							Template;
var		native		const	array<ParticleEmitterInstancePointer>	EmitterInstances;

/** If true, activate on creation. */
var()						bool									bAutoActivate;
var					const	bool									bWasCompleted;
var					const	bool									bSuppressSpawning;

/*** Parameter list here... ***/
struct native ParticleSysParam
{
	var()	name		Name;
	var()	float		Scalar;
	var()	vector		Vector;
	var()	color		Color;
	var()	actor		Actor;
};

// 
// 
var()	editinline array<ParticleSysParam>		InstanceParameters;

var		vector									OldPosition;
var		vector									PartSysVelocity;

var		float									WarmupTime;

native final function SetTemplate(ParticleSystem NewTemplate);
native final function ActivateSystem();
native final function DeactivateSystem();

cpptext
{
	// UObject interface
	virtual void PostLoad();
	virtual void Destroy();
	virtual void PreEditChange();
	virtual void PostEditChange(UProperty* PropertyThatChanged);
	virtual void Serialize( FArchive& Ar );

	// UActorComponent interface
	virtual void Created();
	virtual void Destroyed();

	// UPrimitiveComponent interface
	virtual void UpdateBounds();
	virtual void Tick(FLOAT DeltaTime);
	
	// FPrimitiveViewInterface interface
	virtual DWORD GetLayerMask(const FSceneContext& Context) const;
	virtual void Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
	virtual void RenderForeground(const FSceneContext& Context, struct FPrimitiveRenderInterface* PRI);

	// UParticleSystemComponent interface
	void InitParticles();
	void ResetParticles();
	void UpdateInstances();
	void SetTemplate(class UParticleSystem* NewTemplate);
	UBOOL HasCompleted();

	void InitializeSystem();

	// InstanceParameters interface
	INT SetFloatParameter(const FName& Name, FLOAT Param);
	INT SetVectorParameter(const FName& Name, const FVector& Param);
	INT SetColorParameter(const FName& Name, const FColor& Param);
	INT SetActorParameter(const FName& Name, const AActor* Param);
}

defaultproperties
{
	bAutoActivate=true
	OldPosition=(X=0,Y=0,Z=0)
	PartSysVelocity=(X=0,Y=0,Z=0)
	WarmupTime=0
}

native function int SetFloatParameter(name ParameterName, float Param);
native function int SetVectorParameter(name ParameterName, vector Param);
native function int SetColorParameter(name ParameterName, color Param);
native function int SetActorParameter(name ParameterName, actor Param);
