/*===========================================================================
    C++ class definitions exported from UnrealScript.
    This is automatically generated by the tools.
    DO NOT modify this manually! Edit the corresponding .uc files instead!
===========================================================================*/
#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,4)
#endif


#ifndef NAMES_ONLY
#define AUTOGENERATE_NAME(name) extern FName ENGINE_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#endif

AUTOGENERATE_NAME(OnParticleSystemFinished)

#ifndef NAMES_ONLY


struct Emitter_eventOnParticleSystemFinished_Parms
{
    class UParticleSystemComponent* FinishedComponent;
};
class AEmitter : public AActor
{
public:
    class UParticleSystemComponent* ParticleSystemComponent;
    BITFIELD bDestroyOnSystemFinish:1 GCC_PACK(PROPERTY_ALIGNMENT);
    DECLARE_FUNCTION(execSetTemplate);
    void eventOnParticleSystemFinished(class UParticleSystemComponent* FinishedComponent)
    {
        Emitter_eventOnParticleSystemFinished_Parms Parms;
        Parms.FinishedComponent=FinishedComponent;
        ProcessEvent(FindFunctionChecked(ENGINE_OnParticleSystemFinished),&Parms);
    }
    DECLARE_CLASS(AEmitter,AActor,0,Engine)
	void SetTemplate(UParticleSystem* NewTemplate, UBOOL bDestroyOnFinish=false);
};

struct FParticleSysParam
{
    FName Name;
    FLOAT Scalar;
    FVector Vector;
    FColor Color;
    class AActor* Actor;
};


class UParticleSystemComponent : public UPrimitiveComponent
{
public:
    class UParticleSystem* Template;
    TArrayNoInit<FParticleEmitterInstancePointer> EmitterInstances;
    BITFIELD bAutoActivate:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD bWasCompleted:1;
    BITFIELD bSuppressSpawning:1;
    TArrayNoInit<FParticleSysParam> InstanceParameters GCC_PACK(PROPERTY_ALIGNMENT);
    FVector OldPosition;
    FVector PartSysVelocity;
    FLOAT WarmupTime;
    DECLARE_FUNCTION(execSetActorParameter);
    DECLARE_FUNCTION(execSetColorParameter);
    DECLARE_FUNCTION(execSetVectorParameter);
    DECLARE_FUNCTION(execSetFloatParameter);
    DECLARE_FUNCTION(execDeactivateSystem);
    DECLARE_FUNCTION(execActivateSystem);
    DECLARE_FUNCTION(execSetTemplate);
    DECLARE_CLASS(UParticleSystemComponent,UPrimitiveComponent,0,Engine)
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
};

enum EEmitterRenderMode
{
    ERM_Normal              =0,
    ERM_Point               =1,
    ERM_Cross               =2,
    ERM_None                =3,
    ERM_MAX                 =4,
};

class UParticleEmitter : public UObject
{
public:
    FName EmitterName;
    TArrayNoInit<class UParticleModule*> Modules;
    class UParticleModule* TypeDataModule;
    TArrayNoInit<class UParticleModule*> SpawnModules;
    TArrayNoInit<class UParticleModule*> UpdateModules;
    BITFIELD UseLocalSpace:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD ConvertedModules:1;
    class UDistributionFloat* SpawnRate GCC_PACK(PROPERTY_ALIGNMENT);
    BYTE EmitterRenderMode;
    FLOAT EmitterDuration GCC_PACK(PROPERTY_ALIGNMENT);
    INT EmitterLoops;
    FColor EmitterEditorColor;
    INT PeakActiveParticles;
    DECLARE_CLASS(UParticleEmitter,UObject,0,Engine)
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
};


class UParticleMeshEmitter : public UParticleEmitter
{
public:
    class UStaticMesh* Mesh;
    BITFIELD CastShadows:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD DoCollisions:1;
    DECLARE_CLASS(UParticleMeshEmitter,UParticleEmitter,0,Engine)
	virtual FParticleEmitterInstance* CreateInstance( UParticleSystemComponent* InComponent );
	virtual void SetToSensibleDefaults();

	virtual DWORD GetLayerMask() const;
};

enum EParticleScreenAlignment
{
    PSA_Square              =0,
    PSA_Rectangle           =1,
    PSA_Velocity            =2,
    PSA_MAX                 =3,
};

class UParticleSpriteEmitter : public UParticleEmitter
{
public:
    BYTE ScreenAlignment;
    class UMaterialInstance* Material GCC_PACK(PROPERTY_ALIGNMENT);
    DECLARE_CLASS(UParticleSpriteEmitter,UParticleEmitter,0,Engine)
	virtual FParticleEmitterInstance* CreateInstance( UParticleSystemComponent* InComponent );
	virtual void SetToSensibleDefaults();

	virtual DWORD GetLayerMask() const;
};

enum ESubUVInterpolationMethod
{
    PSSUV_Linear            =0,
    PSSUV_Linear_Blend      =1,
    PSSUV_Random            =2,
    PSSUV_Random_Blend      =3,
    PSSUV_MAX               =4,
};

class UParticleSpriteSubUVEmitter : public UParticleSpriteEmitter
{
public:
    INT SubImages_Horizontal;
    INT SubImages_Vertical;
    BYTE InterpolationMethod;
    DECLARE_CLASS(UParticleSpriteSubUVEmitter,UParticleSpriteEmitter,0,Engine)
	virtual FParticleEmitterInstance* CreateInstance( UParticleSystemComponent* InComponent );
	virtual void SetToSensibleDefaults();

	virtual DWORD GetLayerMask() const;
};


class UParticleModule : public UObject
{
public:
    BITFIELD bSpawnModule:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD bUpdateModule:1;
    BITFIELD bCurvesAsColor:1;
    BITFIELD b3DDrawMode:1;
    BITFIELD bSupported3DDrawMode:1;
    FColor ModuleEditorColor GCC_PACK(PROPERTY_ALIGNMENT);
    TArrayNoInit<class UClass*> AllowedEmitterClasses;
    DECLARE_CLASS(UParticleModule,UObject,0,Engine)
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
};


class UParticleModuleAccelerationBase : public UParticleModule
{
public:
    DECLARE_CLASS(UParticleModuleAccelerationBase,UParticleModule,0,Engine)
    NO_DEFAULT_CONSTRUCTOR(UParticleModuleAccelerationBase)
};


class UParticleModuleAcceleration : public UParticleModuleAccelerationBase
{
public:
    class UDistributionVector* Acceleration;
    DECLARE_CLASS(UParticleModuleAcceleration,UParticleModuleAccelerationBase,0,Engine)
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
	virtual INT RequiredBytes();
};


class UParticleModuleAccelerationOverLifetime : public UParticleModuleAccelerationBase
{
public:
    class UDistributionVector* AccelOverLife;
    DECLARE_CLASS(UParticleModuleAccelerationOverLifetime,UParticleModuleAccelerationBase,0,Engine)
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
};


class UParticleModuleAttractorBase : public UParticleModule
{
public:
    DECLARE_CLASS(UParticleModuleAttractorBase,UParticleModule,0,Engine)
    NO_DEFAULT_CONSTRUCTOR(UParticleModuleAttractorBase)
};


class UParticleModuleAttractorLine : public UParticleModuleAttractorBase
{
public:
    FVector EndPoint0;
    FVector EndPoint1;
    class UDistributionFloat* Range;
    class UDistributionFloat* Strength;
    DECLARE_CLASS(UParticleModuleAttractorLine,UParticleModuleAttractorBase,0,Engine)
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );

	virtual void Render3DPreview(FParticleEmitterInstance* Owner, const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
};


class UParticleModuleAttractorPoint : public UParticleModuleAttractorBase
{
public:
    class UDistributionVector* Position;
    class UDistributionFloat* Range;
    class UDistributionFloat* Strength;
    BITFIELD StrengthByDistance:1 GCC_PACK(PROPERTY_ALIGNMENT);
    DECLARE_CLASS(UParticleModuleAttractorPoint,UParticleModuleAttractorBase,0,Engine)
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );

	virtual void Render3DPreview(FParticleEmitterInstance* Owner, const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
};


class UParticleModuleCollisionBase : public UParticleModule
{
public:
    DECLARE_CLASS(UParticleModuleCollisionBase,UParticleModule,0,Engine)
    NO_DEFAULT_CONSTRUCTOR(UParticleModuleCollisionBase)
};


class UParticleModuleCollision : public UParticleModuleCollisionBase
{
public:
    class UDistributionVector* DampingFactor;
    class UDistributionFloat* MaxCollisions;
    BITFIELD bApplyPhysics:1 GCC_PACK(PROPERTY_ALIGNMENT);
    class UDistributionFloat* ParticleMass GCC_PACK(PROPERTY_ALIGNMENT);
    DECLARE_CLASS(UParticleModuleCollision,UParticleModuleCollisionBase,0,Engine)
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
	virtual INT RequiredBytes();
};


class UParticleModuleColorBase : public UParticleModule
{
public:
    DECLARE_CLASS(UParticleModuleColorBase,UParticleModule,0,Engine)
    NO_DEFAULT_CONSTRUCTOR(UParticleModuleColorBase)
};


class UParticleModuleColor : public UParticleModuleColorBase
{
public:
    class UDistributionVector* StartColor;
    DECLARE_CLASS(UParticleModuleColor,UParticleModuleColorBase,0,Engine)
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
};


class UParticleModuleColorByParameter : public UParticleModuleColorBase
{
public:
    FName ColorParam;
    DECLARE_CLASS(UParticleModuleColorByParameter,UParticleModuleColorBase,0,Engine)
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
};


class UParticleModuleColorOverLife : public UParticleModuleColorBase
{
public:
    class UDistributionVector* ColorOverLife;
    DECLARE_CLASS(UParticleModuleColorOverLife,UParticleModuleColorBase,0,Engine)
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
};


class UParticleModuleLifetimeBase : public UParticleModule
{
public:
    DECLARE_CLASS(UParticleModuleLifetimeBase,UParticleModule,0,Engine)
    NO_DEFAULT_CONSTRUCTOR(UParticleModuleLifetimeBase)
};


class UParticleModuleLifetime : public UParticleModuleLifetimeBase
{
public:
    class UDistributionFloat* Lifetime;
    DECLARE_CLASS(UParticleModuleLifetime,UParticleModuleLifetimeBase,0,Engine)
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
};


class UParticleModuleLocationBase : public UParticleModule
{
public:
    DECLARE_CLASS(UParticleModuleLocationBase,UParticleModule,0,Engine)
    NO_DEFAULT_CONSTRUCTOR(UParticleModuleLocationBase)
};


class UParticleModuleLocation : public UParticleModuleLocationBase
{
public:
    class UDistributionVector* StartLocation;
    DECLARE_CLASS(UParticleModuleLocation,UParticleModuleLocationBase,0,Engine)
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
	virtual void Render3DPreview(FParticleEmitterInstance* Owner, const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
};


class UParticleModuleRotationBase : public UParticleModule
{
public:
    DECLARE_CLASS(UParticleModuleRotationBase,UParticleModule,0,Engine)
    NO_DEFAULT_CONSTRUCTOR(UParticleModuleRotationBase)
};


class UParticleModuleMeshRotation : public UParticleModuleRotationBase
{
public:
    class UDistributionVector* StartRotation;
    DECLARE_CLASS(UParticleModuleMeshRotation,UParticleModuleRotationBase,0,Engine)
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
};


class UParticleModuleRotation : public UParticleModuleRotationBase
{
public:
    class UDistributionFloat* StartRotation;
    DECLARE_CLASS(UParticleModuleRotation,UParticleModuleRotationBase,0,Engine)
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
};


class UParticleModuleRotationOverLifetime : public UParticleModuleRotationBase
{
public:
    class UDistributionFloat* RotationOverLife;
    DECLARE_CLASS(UParticleModuleRotationOverLifetime,UParticleModuleRotationBase,0,Engine)
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
};


class UParticleModuleRotationRateBase : public UParticleModule
{
public:
    DECLARE_CLASS(UParticleModuleRotationRateBase,UParticleModule,0,Engine)
    NO_DEFAULT_CONSTRUCTOR(UParticleModuleRotationRateBase)
};


class UParticleModuleMeshRotationRate : public UParticleModuleRotationRateBase
{
public:
    class UDistributionVector* StartRotationRate;
    DECLARE_CLASS(UParticleModuleMeshRotationRate,UParticleModuleRotationRateBase,0,Engine)
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
};


class UParticleModuleRotationRate : public UParticleModuleRotationRateBase
{
public:
    class UDistributionFloat* StartRotationRate;
    DECLARE_CLASS(UParticleModuleRotationRate,UParticleModuleRotationRateBase,0,Engine)
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
};


class UParticleModuleRotationRateMultiplyLife : public UParticleModuleRotationRateBase
{
public:
    class UDistributionFloat* LifeMultiplier;
    DECLARE_CLASS(UParticleModuleRotationRateMultiplyLife,UParticleModuleRotationRateBase,0,Engine)
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
};


class UParticleModuleSizeBase : public UParticleModule
{
public:
    DECLARE_CLASS(UParticleModuleSizeBase,UParticleModule,0,Engine)
    NO_DEFAULT_CONSTRUCTOR(UParticleModuleSizeBase)
};


class UParticleModuleSize : public UParticleModuleSizeBase
{
public:
    class UDistributionVector* StartSize;
    DECLARE_CLASS(UParticleModuleSize,UParticleModuleSizeBase,0,Engine)
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
};


class UParticleModuleSizeMultiplyLife : public UParticleModuleSizeBase
{
public:
    class UDistributionVector* LifeMultiplier;
    BITFIELD MultiplyX:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD MultiplyY:1;
    BITFIELD MultiplyZ:1;
    DECLARE_CLASS(UParticleModuleSizeMultiplyLife,UParticleModuleSizeBase,0,Engine)
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
};


class UParticleModuleSizeMultiplyVelocity : public UParticleModuleSizeBase
{
public:
    class UDistributionVector* VelocityMultiplier;
    BITFIELD MultiplyX:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD MultiplyY:1;
    BITFIELD MultiplyZ:1;
    DECLARE_CLASS(UParticleModuleSizeMultiplyVelocity,UParticleModuleSizeBase,0,Engine)
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
};


class UParticleModuleSizeScale : public UParticleModuleSizeBase
{
public:
    class UDistributionVector* SizeScale;
    BITFIELD EnableX:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD EnableY:1;
    BITFIELD EnableZ:1;
    DECLARE_CLASS(UParticleModuleSizeScale,UParticleModuleSizeBase,0,Engine)
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
};


class UParticleModuleSubUVBase : public UParticleModule
{
public:
    DECLARE_CLASS(UParticleModuleSubUVBase,UParticleModule,0,Engine)
    NO_DEFAULT_CONSTRUCTOR(UParticleModuleSubUVBase)
};


class UParticleModuleSubUV : public UParticleModuleSubUVBase
{
public:
    class UDistributionFloat* SubImageIndex;
    DECLARE_CLASS(UParticleModuleSubUV,UParticleModuleSubUVBase,0,Engine)
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
};


class UParticleModuleTypeDataBase : public UParticleModule
{
public:
    DECLARE_CLASS(UParticleModuleTypeDataBase,UParticleModule,0,Engine)
	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent);
	virtual void SetToSensibleDefaults();
};


class UParticleModuleTypeDataMesh : public UParticleModuleTypeDataBase
{
public:
    class UStaticMesh* Mesh;
    BITFIELD CastShadows:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD DoCollisions:1;
    DECLARE_CLASS(UParticleModuleTypeDataMesh,UParticleModuleTypeDataBase,0,Engine)
	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent);
	virtual void SetToSensibleDefaults();
	
	virtual void PostEditChange(UProperty* PropertyThatChanged);

	virtual DWORD GetLayerMask() const;
};

enum ESubUVInterpMethod
{
    PSSUVIM_Linear          =0,
    PSSUVIM_Linear_Blend    =1,
    PSSUVIM_Random          =2,
    PSSUVIM_Random_Blend    =3,
    PSSUVIM_MAX             =4,
};

class UParticleModuleTypeDataSubUV : public UParticleModuleTypeDataBase
{
public:
    INT SubImages_Horizontal;
    INT SubImages_Vertical;
    BYTE InterpolationMethod;
    DECLARE_CLASS(UParticleModuleTypeDataSubUV,UParticleModuleTypeDataBase,0,Engine)
	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent);
	virtual void SetToSensibleDefaults();

	virtual void PostEditChange(UProperty* PropertyThatChanged);

};


class UParticleModuleTypeDataTrail : public UParticleModuleTypeDataBase
{
public:
    BITFIELD Tapered:1 GCC_PACK(PROPERTY_ALIGNMENT);
    INT TessellationFactor GCC_PACK(PROPERTY_ALIGNMENT);
    DECLARE_CLASS(UParticleModuleTypeDataTrail,UParticleModuleTypeDataBase,0,Engine)
	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent);
	virtual void SetToSensibleDefaults();
	
	virtual void PostEditChange(UProperty* PropertyThatChanged);

	virtual DWORD GetLayerMask() const;
};


class UParticleModuleVelocityBase : public UParticleModule
{
public:
    DECLARE_CLASS(UParticleModuleVelocityBase,UParticleModule,0,Engine)
    NO_DEFAULT_CONSTRUCTOR(UParticleModuleVelocityBase)
};


class UParticleModuleVelocity : public UParticleModuleVelocityBase
{
public:
    class UDistributionVector* StartVelocity;
    class UDistributionFloat* StartVelocityRadial;
    DECLARE_CLASS(UParticleModuleVelocity,UParticleModuleVelocityBase,0,Engine)
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
};


class UParticleModuleVelocityInheritParent : public UParticleModuleVelocityBase
{
public:
    DECLARE_CLASS(UParticleModuleVelocityInheritParent,UParticleModuleVelocityBase,0,Engine)
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
};


class UParticleModuleVelocityOverLifetime : public UParticleModuleVelocityBase
{
public:
    class UDistributionVector* VelOverLife;
    DECLARE_CLASS(UParticleModuleVelocityOverLifetime,UParticleModuleVelocityBase,0,Engine)
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
};

enum EParticleSystemUpdateMode
{
    EPSUM_RealTime          =0,
    EPSUM_FixedTime         =1,
    EPSUM_MAX               =2,
};

class UParticleSystem : public UObject
{
public:
    BYTE SystemUpdateMode;
    FLOAT UpdateTime_FPS GCC_PACK(PROPERTY_ALIGNMENT);
    FLOAT UpdateTime_Delta;
    FLOAT WarmupTime;
    TArrayNoInit<class UParticleEmitter*> Emitters;
    class UParticleSystemComponent* PreviewComponent;
    FRotator ThumbnailAngle;
    FLOAT ThumbnailDistance;
    FLOAT ThumbnailWarmup;
    class UInterpCurveEdSetup* CurveEdSetup;
    DECLARE_CLASS(UParticleSystem,UObject,0,Engine)
	// UObject interface.
	virtual void PostEditChange(UProperty* PropertyThatChanged);
	virtual void PostLoad();
	
	// Browser window.
	void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	INT GetThumbnailLabels( TArray<FString>* InLabels );
};

#endif

AUTOGENERATE_FUNCTION(AEmitter,-1,execSetTemplate);
AUTOGENERATE_FUNCTION(UParticleSystemComponent,-1,execSetActorParameter);
AUTOGENERATE_FUNCTION(UParticleSystemComponent,-1,execSetColorParameter);
AUTOGENERATE_FUNCTION(UParticleSystemComponent,-1,execSetVectorParameter);
AUTOGENERATE_FUNCTION(UParticleSystemComponent,-1,execSetFloatParameter);
AUTOGENERATE_FUNCTION(UParticleSystemComponent,-1,execDeactivateSystem);
AUTOGENERATE_FUNCTION(UParticleSystemComponent,-1,execActivateSystem);
AUTOGENERATE_FUNCTION(UParticleSystemComponent,-1,execSetTemplate);

#ifndef NAMES_ONLY
#undef AUTOGENERATE_NAME
#undef AUTOGENERATE_FUNCTION
#endif

#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

