/*===========================================================================
    C++ class definitions exported from UnrealScript.
    This is automatically generated by the tools.
    DO NOT modify this manually! Edit the corresponding .uc files instead!
===========================================================================*/
#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,4)
#endif


#ifndef NAMES_ONLY
#define AUTOGENERATE_NAME(name) extern FName WARFAREGAME_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#endif

AUTOGENERATE_NAME(EndFiringEvent)
AUTOGENERATE_NAME(FireModeChanged)
AUTOGENERATE_NAME(IsReloading)
AUTOGENERATE_NAME(StartFiringEvent)

#ifndef NAMES_ONLY

struct FPlayerInfo
{
    class AController* Player;
    BITFIELD bFriendly:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD bVisible:1;
    FLOAT LastSeenTime GCC_PACK(PROPERTY_ALIGNMENT);
    FVector LastSeenLocation;
    BITFIELD bAudible:1 GCC_PACK(PROPERTY_ALIGNMENT);
    FLOAT LastHeardTime GCC_PACK(PROPERTY_ALIGNMENT);
    FVector LastHeardLocation;
};

struct FNoiseInfo
{
    class AActor* NoiseMaker;
    FLOAT Loudness;
    FLOAT HeardTime;
    FVector HeardLocation;
};

struct FGoalInfo
{
    class AActor* GoalActor;
    BITFIELD bInterruptable:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD bReachable:1;
    class AActor* CachedMoveTarget GCC_PACK(PROPERTY_ALIGNMENT);
    BYTE MoveAttempts;
};

struct FRouteGoalInfo
{
    class ARoute* GoalRoute;
    INT MoveDirection;
    BITFIELD bInterruptable:1 GCC_PACK(PROPERTY_ALIGNMENT);
};

#define UCONST_MAX_MOVE_ATTEMPTS 10
#define UCONST_NOISE_DURATION 15.f
#define UCONST_SIGHT_DURATION 10.f

class AWarAIController : public AAIController
{
public:
    TArrayNoInit<FPlayerInfo> PlayerList;
    INT EnemyIdx;
    TArrayNoInit<FNoiseInfo> RecentNoise;
    FGoalInfo MoveGoal;
    BITFIELD bMoveAborted:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD bDelayingMove:1;
    class AActor* StepAsideGoal GCC_PACK(PROPERTY_ALIGNMENT);
    FLOAT MaxStepAsideDist;
    TArrayNoInit<class AActor*> InvalidMoveGoals;
    FRouteGoalInfo RouteGoal;
    class AActor* ShootTarget;
    FLOAT LastShotTime;
    FLOAT UnderFireMeter;
    class AFileLog* AILogFile;
    DECLARE_FUNCTION(execAILog);
    DECLARE_CLASS(AWarAIController,AAIController,0,WarfareGame)
    NO_DEFAULT_CONSTRUCTOR(AWarAIController)
};


class AWarScout : public AScout
{
public:
    DECLARE_CLASS(AWarScout,AScout,0|CLASS_Config,WarfareGame)
	virtual void InitForPathing()
	{
		Super::InitForPathing();
		bJumpCapable = 0;
		bCanJump = 0;
	}
};

enum EEvadeDirection
{
    ED_None                 =0,
    ED_Forward              =1,
    ED_Backward             =2,
    ED_Left                 =3,
    ED_Right                =4,
    ED_MAX                  =5,
};
struct FLocDmgStruct
{
    FName BoneNameArray[4];
    FStringNoInit BodyPartName;
    FLOAT fDamageMultiplier;
};


class AWarPawn : public APawn
{
public:
    BYTE EvadeDirection;
    BYTE CoverType;
    BYTE CoverAction;
    FVector CoverTranslationOffset GCC_PACK(PROPERTY_ALIGNMENT);
    TArrayNoInit<FLOAT> CoverMovementPct;
    BITFIELD bIsSprinting:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD bPlayDeathAnim:1;
    BITFIELD bAwardEliteKillBonus:1;
    FLOAT SprintingPct GCC_PACK(PROPERTY_ALIGNMENT);
    FLOAT MovementPct;
    TArrayNoInit<class UClass*> DefaultInventory;
    FStringNoInit VoiceType;
    INT spree;
    FLOAT ShieldAmount;
    FLOAT ShieldRechargeRate;
    FLOAT ShieldRechargeDelay;
    FLOAT LastShieldHit;
    TArrayNoInit<FLocDmgStruct> LocDmgArray;
    class UClass* SoundGroupClass;
    FLOAT KillReward;
    FLOAT FastKillBonus;
    FLOAT EliteKillBonus;
    BITFIELD bJumpingDownLedge:1 GCC_PACK(PROPERTY_ALIGNMENT);
    FLOAT MaxJumpDownDistance GCC_PACK(PROPERTY_ALIGNMENT);
    FLOAT MinJumpDownDistance;
    class AJumpPoint* JumpPoint;
    FVector2D CoverProtectionFOV;
    DECLARE_FUNCTION(execJumpDownLedge);
    DECLARE_CLASS(AWarPawn,APawn,0|CLASS_Config,WarfareGame)
	virtual FLOAT MaxSpeedModifier();
    FVector CheckForLedges(FVector AccelDir, FVector Delta, FVector GravDir, int &bCheckedFall, int &bMustJump );
};

enum ECoverDirection
{
    CD_Default              =0,
    CD_Left                 =1,
    CD_Right                =2,
    CD_Up                   =3,
    CD_MAX                  =4,
};
struct FInteractableInfo
{
    class ATrigger* InteractTrigger;
    BITFIELD bUsuable:1 GCC_PACK(PROPERTY_ALIGNMENT);
    class USeqEvent_Used* Event GCC_PACK(PROPERTY_ALIGNMENT);
};


class AWarPC : public APlayerController
{
public:
    FLOAT UseCoverInterval;
    BITFIELD bLateComer:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD bDontUpdate:1;
    BITFIELD bTargetingMode:1;
    BITFIELD bDebugCover:1;
    BITFIELD bDebugAI:1;
    FVector2D CoverAdhesionFOV GCC_PACK(PROPERTY_ALIGNMENT);
    FLOAT CoverAcquireDistPct;
    BITFIELD bReplicatePawnCover:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD bAssessMode:1;
    BITFIELD bWasLeft:1;
    BITFIELD bWasRight:1;
    FVector LastDesiredCoverLocation GCC_PACK(PROPERTY_ALIGNMENT);
    FRotator LastDesiredCoverRotation;
    class ACoverNode* PrimaryCover;
    class ACoverNode* SecondaryCover;
    BYTE CoverDirection;
    FLOAT CoverTransitionTime GCC_PACK(PROPERTY_ALIGNMENT);
    FLOAT CoverTransitionCountHold;
    FLOAT CoverTransitionCountDown;
    TArrayNoInit<FInteractableInfo> InteractableList;
    INT Cash;
    FLOAT LastKillTime;
    INT MultiKillCount;
    INT Reward_2kills;
    INT Reward_3kills;
    INT Reward_multikills;
    INT Reward_MeleeKillBonus;
    class UClass* PrimaryWeaponClass;
    class UClass* SecondaryWeaponClass;
    FLOAT VehicleCheckRadius;
    DECLARE_CLASS(AWarPC,APlayerController,0|CLASS_Config,WarfareGame)
	virtual UBOOL WantsLedgeCheck()
	{
		return 1;
	}
};

struct FHUDElementInfo
{
    BITFIELD bEnabled:1 GCC_PACK(PROPERTY_ALIGNMENT);
    FLOAT DrawX GCC_PACK(PROPERTY_ALIGNMENT);
    FLOAT DrawY;
    BITFIELD bCenterX:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD bCenterY:1;
    FColor DrawColor GCC_PACK(PROPERTY_ALIGNMENT);
    INT DrawW;
    INT DrawH;
    class UTexture2D* DrawIcon;
    class UMaterial* DrawMaterial;
    FLOAT DrawU;
    FLOAT DrawV;
    FLOAT DrawUSize;
    FLOAT DrawVSize;
    FLOAT DrawScale;
    class UFont* DrawFont;
    FStringNoInit DrawLabel;
};

struct FCashDisplayStruct
{
    INT DeltaCash;
    FLOAT Life;
};

#define UCONST_NUM_CASH_ENTRIES 4

class AWarHUD : public AHUD
{
public:
    TArrayNoInit<FHUDElementInfo> HUDElements;
    FLOAT GlobalElementScale;
    class UTexture2D* XboxA_Tex;
    TArrayNoInit<class UTexture2D*> HealthSymbols;
    BITFIELD bDrawMeleeInfo:1 GCC_PACK(PROPERTY_ALIGNMENT);
    FCashDisplayStruct CashEntries[4] GCC_PACK(PROPERTY_ALIGNMENT);
    INT OldCashAmount;
    FLOAT DeltaCashLifeTime;
    FLOAT CashRelPosY;
    FColor FadeColor;
    FLOAT PreviousFadeAlpha;
    FLOAT DesiredFadeAlpha;
    FLOAT FadeAlpha;
    FLOAT FadeAlphaTime;
    FLOAT DesiredFadeAlphaTime;
    DECLARE_CLASS(AWarHUD,AHUD,0|CLASS_Transient|CLASS_Config,WarfareGame)
    NO_DEFAULT_CONSTRUCTOR(AWarHUD)
};


class AWarPRI : public APlayerReplicationInfo
{
public:
    FName SquadName;
    DECLARE_CLASS(AWarPRI,APlayerReplicationInfo,0,WarfareGame)
    NO_DEFAULT_CONSTRUCTOR(AWarPRI)
};

struct FSquadInfo
{
    FName SquadName;
    TArrayNoInit<class AController*> members;
};


class AWarTeamInfo : public ATeamInfo
{
public:
    TArrayNoInit<class AController*> TeamMembers;
    TArrayNoInit<FSquadInfo> Squads;
    class AFileLog* TeamLogFile;
    DECLARE_CLASS(AWarTeamInfo,ATeamInfo,0,WarfareGame)
    NO_DEFAULT_CONSTRUCTOR(AWarTeamInfo)
};

struct FFireModeStruct
{
    FStringNoInit Name;
    FLOAT FireRate;
    INT Recoil;
    FLOAT Inaccuracy;
    INT Damage;
    class UClass* DamageTypeClass;
};

#define UCONST_RELOAD_FIREMODE 129
#define UCONST_MELEE_FIREMODE 128

struct WarWeapon_eventIsReloading_Parms
{
    BITFIELD ReturnValue;
};
struct WarWeapon_eventEndFiringEvent_Parms
{
    BYTE FireModeNum;
};
struct WarWeapon_eventStartFiringEvent_Parms
{
    BYTE FireModeNum;
};
struct WarWeapon_eventFireModeChanged_Parms
{
    BYTE OldFireModeNum;
    BYTE NewFireModeNum;
};
class AWarWeapon : public AWeapon
{
public:
    BITFIELD bForceReload:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD bIsFiring:1;
    TArrayNoInit<FFireModeStruct> FireModeInfoArray GCC_PACK(PROPERTY_ALIGNMENT);
    FName WeaponFireAnim;
    class USoundCue* FireSound;
    BYTE SelectedFireMode;
    BYTE ConsShotCount;
    class USoundCue* FireModeCycleSound GCC_PACK(PROPERTY_ALIGNMENT);
    INT MagazineSize;
    INT AmmoUsedCount;
    INT CriticalAmmoCount;
    FLOAT ReloadDuration;
    class UMeshComponent* Mesh;
    class UTransformComponent* MeshTranform;
    FVector FireOffset;
    class UMeshComponent* MuzzleFlashMesh;
    class UTransformComponent* MuzzleFlashTransform;
    class UPointLightComponent* MuzzleFlashLight;
    class UTransformComponent* MuzzleFlashLightTransform;
    FLOAT MuzzleFlashLightBrightness;
    FLOAT fMeleeRange;
    INT MeleeDamage;
    FLOAT fMeleeDuration;
    FName PawnFiringAnim;
    FName PawnIdleAnim;
    FName PawnReloadAnim;
    FName PawnMeleeAnim;
    class USoundCue* WeaponReloadSound;
    class USoundCue* WeaponEquipSound;
    FLOAT CrosshairExpandStrength;
    TArrayNoInit<FHUDElementInfo> CrosshairElements;
    TArrayNoInit<FHUDElementInfo> HUDElements;
    FHUDElementInfo WeapNameHUDElemnt;
    class UMaterialInstanceConstant* HUDAmmoMaterialInstance;
    class UMaterial* HUDAmmoMaterial;
    BITFIELD eventIsReloading()
    {
        WarWeapon_eventIsReloading_Parms Parms;
        Parms.ReturnValue=0;
        ProcessEvent(FindFunctionChecked(WARFAREGAME_IsReloading),&Parms);
        return Parms.ReturnValue;
    }
    void eventEndFiringEvent(BYTE FireModeNum)
    {
        WarWeapon_eventEndFiringEvent_Parms Parms;
        Parms.FireModeNum=FireModeNum;
        ProcessEvent(FindFunctionChecked(WARFAREGAME_EndFiringEvent),&Parms);
    }
    void eventStartFiringEvent(BYTE FireModeNum)
    {
        WarWeapon_eventStartFiringEvent_Parms Parms;
        Parms.FireModeNum=FireModeNum;
        ProcessEvent(FindFunctionChecked(WARFAREGAME_StartFiringEvent),&Parms);
    }
    void eventFireModeChanged(BYTE OldFireModeNum, BYTE NewFireModeNum)
    {
        WarWeapon_eventFireModeChanged_Parms Parms;
        Parms.OldFireModeNum=OldFireModeNum;
        Parms.NewFireModeNum=NewFireModeNum;
        ProcessEvent(FindFunctionChecked(WARFAREGAME_FireModeChanged),&Parms);
    }
    DECLARE_CLASS(AWarWeapon,AWeapon,0|CLASS_Config,WarfareGame)
	void PreNetReceive();
	void PostNetReceive();
};


class AProj_Grenade : public AProjectile
{
public:
    BITFIELD bPerformGravity:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD bFreeze:1;
    FLOAT fGravityScale GCC_PACK(PROPERTY_ALIGNMENT);
    INT MaxBounceCount;
    INT BounceCount;
    FLOAT VelocityDampingFactor;
    FLOAT fBounciness;
    FLOAT CookedTime;
    TArrayNoInit<FVector> CachedPositions;
    DECLARE_CLASS(AProj_Grenade,AProjectile,0,WarfareGame)
	void physProjectile(FLOAT deltaTime, INT Iterations);
};

enum ESpawnType
{
    LOCUST_Attack           =0,
    LOCUST_Defend           =1,
    GEAR_Follow             =2,
    GEAR_Defend             =3,
    ESpawnType_MAX          =4,
};
struct FSpawnSet
{
    class UClass* ControllerClass;
    class UClass* PawnClass;
    TArrayNoInit<class UClass*> InventoryList;
    INT TeamIndex;
};


class UWarActorFactoryAI : public UActorFactory
{
public:
    BYTE SpawnType;
    FName SquadName GCC_PACK(PROPERTY_ALIGNMENT);
    TArrayNoInit<FSpawnSet> SpawnSets;
    TArrayNoInit<class UClass*> InventoryList;
    DECLARE_CLASS(UWarActorFactoryAI,UActorFactory,0|CLASS_Config,WarfareGame)
	virtual AActor* CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation);
};


class UWarAnim_CoverFireBlendNode : public UAnimNodeBlendPerBone
{
public:
    FLOAT LastFireTime;
    FLOAT FireBlendOutDelay;
    DECLARE_CLASS(UWarAnim_CoverFireBlendNode,UAnimNodeBlendPerBone,0,WarfareGame)
	virtual void TickAnim(FLOAT DeltaSeconds, FLOAT TotalWeight);
};


class UAnimNodeBlendByPhysics : public UAnimNodeBlendList
{
public:
    INT LastMap;
    TArrayNoInit<INT> PhysicsMap;
    DECLARE_CLASS(UAnimNodeBlendByPhysics,UAnimNodeBlendList,0,WarfareGame)
	virtual	void TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight );
};


class UWarAnim_BaseBlendNode : public UAnimNodeBlendList
{
public:
    BYTE LastEvadeDirection;
    DECLARE_CLASS(UWarAnim_BaseBlendNode,UAnimNodeBlendList,0,WarfareGame)
	virtual void TickAnim(FLOAT DeltaSeconds, FLOAT TotalWeight);
	virtual void OnChildAnimEnd(UAnimNodeSequence* Child);
};


class UWarAnim_CoverBlendNode : public UAnimNodeBlendList
{
public:
    BYTE LastCoverType;
    DECLARE_CLASS(UWarAnim_CoverBlendNode,UAnimNodeBlendList,0,WarfareGame)
	virtual void TickAnim(FLOAT DeltaSeconds, FLOAT TotalWeight);
};


class UWarAnim_CoverMoveBlendNode : public UAnimNodeBlendList
{
public:
    DECLARE_CLASS(UWarAnim_CoverMoveBlendNode,UAnimNodeBlendList,0,WarfareGame)
	virtual void TickAnim(FLOAT DeltaSeconds, FLOAT TotalWeight);
};


class UWarAnim_EvadeBlendNode : public UAnimNodeBlendList
{
public:
    DECLARE_CLASS(UWarAnim_EvadeBlendNode,UAnimNodeBlendList,0,WarfareGame)
    NO_DEFAULT_CONSTRUCTOR(UWarAnim_EvadeBlendNode)
};


class UWarAnim_CoverSequenceNode : public UAnimNodeSequence
{
public:
    BITFIELD bIntroTransition:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD bOutroTransition:1;
    FName IntroAnimSeqName GCC_PACK(PROPERTY_ALIGNMENT);
    FName IdleAnimSeqName;
    FName OutroAnimSeqName;
    DECLARE_CLASS(UWarAnim_CoverSequenceNode,UAnimNodeSequence,0,WarfareGame)
	virtual void TickAnim(FLOAT DeltaSeconds, FLOAT TotalWeight);
};


class UWarCheatManager : public UCheatManager
{
public:
    DECLARE_CLASS(UWarCheatManager,UCheatManager,0,WarfareGame)
    NO_DEFAULT_CONSTRUCTOR(UWarCheatManager)
};


class USeqAct_GetCash : public USequenceAction
{
public:
    DECLARE_CLASS(USeqAct_GetCash,USequenceAction,0,WarfareGame)
	virtual void Activated()
	{
		TArray<UObject**> objVars;
		GetObjectVars(objVars,TEXT("Target"));
		INT cashAmt = 0;
		for (INT idx = 0; idx < objVars.Num(); idx++)
		{
			AWarPC *pc = Cast<AWarPC>(*(objVars(idx)));
			if (pc != NULL)
			{
				cashAmt += pc->Cash;
			}
		}
		TArray<INT*> intVars;
		GetIntVars(intVars,TEXT("Cash"));
		for (INT idx = 0; idx < intVars.Num(); idx++)
		{
			*(intVars(idx)) = cashAmt;
		}
	}
};


class USeqAct_GetTeammate : public USequenceAction
{
public:
    TArrayNoInit<class UClass*> RequiredInventory;
    DECLARE_CLASS(USeqAct_GetTeammate,USequenceAction,0,WarfareGame)
	virtual void Activated();
	virtual void DeActivated()
	{
		// do nothing, outputs activated in Activated()
	}
};


class USeqAct_AIShootAtTarget : public USeqAct_Latent
{
public:
    BITFIELD bInterruptable:1 GCC_PACK(PROPERTY_ALIGNMENT);
    DECLARE_CLASS(USeqAct_AIShootAtTarget,USeqAct_Latent,0,WarfareGame)
	virtual UBOOL UpdateOp(FLOAT deltaTime);
};


class UUIElement : public UObject
{
public:
    BITFIELD bEnabled:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD bCenterX:1;
    BITFIELD bCenterY:1;
    BITFIELD bAcceptsFocus:1;
    BITFIELD bDrawBorder:1;
    BITFIELD bSizeDrag:1;
    class UUIElement* ParentElement GCC_PACK(PROPERTY_ALIGNMENT);
    INT DrawX;
    INT DrawY;
    INT DrawW;
    INT DrawH;
    FColor DrawColor;
    INT FocusId;
    class UTexture2D* BorderIcon;
    INT BorderSize;
    FColor BorderColor;
    INT BorderU;
    INT BorderV;
    INT BorderUSize;
    INT BorderVSize;
    BITFIELD bSelected:1 GCC_PACK(PROPERTY_ALIGNMENT);
    DECLARE_CLASS(UUIElement,UObject,0,WarfareGame)
	virtual void DrawElement(UCanvas *inCanvas);
	virtual void GetDimensions(INT &X, INT &Y, INT &W, INT &H);
};


class UUIContainer : public UUIElement
{
public:
    TArrayNoInit<class UUIElement*> Elements;
    BITFIELD bHandleInput:1 GCC_PACK(PROPERTY_ALIGNMENT);
    class UConsole* Console GCC_PACK(PROPERTY_ALIGNMENT);
    DECLARE_FUNCTION(execFindElement);
    DECLARE_FUNCTION(execDraw);
    DECLARE_CLASS(UUIContainer,UUIElement,0,WarfareGame)
	virtual void DrawElement(UCanvas *inCanvas);
	UUIElement* FindElement(FName searchName);

	// for generic browser
	void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	INT GetThumbnailLabels( TArray<FString>* InLabels );
};


class UUIElement_Icon : public UUIElement
{
public:
    class UTexture2D* DrawIcon;
    FLOAT DrawU;
    FLOAT DrawV;
    INT DrawUSize;
    INT DrawVSize;
    FLOAT DrawScale;
    DECLARE_CLASS(UUIElement_Icon,UUIElement,0,WarfareGame)
	virtual void DrawElement(UCanvas *inCanvas);
	virtual void GetDimensions(INT &X, INT &Y, INT &W, INT &H);
};


class UUIElement_Label : public UUIElement
{
public:
    class UFont* DrawFont;
    FStringNoInit DrawLabel;
    DECLARE_CLASS(UUIElement_Label,UUIElement,0,WarfareGame)
	virtual void DrawElement(UCanvas *inCanvas);
	virtual void GetDimensions(INT &X, INT &Y, INT &W, INT &H);
};

#endif

AUTOGENERATE_FUNCTION(UUIContainer,-1,execFindElement);
AUTOGENERATE_FUNCTION(UUIContainer,-1,execDraw);
AUTOGENERATE_FUNCTION(AWarAIController,-1,execAILog);
AUTOGENERATE_FUNCTION(AWarPawn,-1,execJumpDownLedge);

#ifndef NAMES_ONLY
#undef AUTOGENERATE_NAME
#undef AUTOGENERATE_FUNCTION
#endif

#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

#ifdef STATIC_LINKING_MOJO
#ifndef WARFAREGAME_NATIVE_DEFS
#define WARFAREGAME_NATIVE_DEFS

DECLARE_NATIVE_TYPE(WarfareGame,UAnimNodeBlendByPhysics);
DECLARE_NATIVE_TYPE(WarfareGame,AProj_Grenade);
DECLARE_NATIVE_TYPE(WarfareGame,USeqAct_AIShootAtTarget);
DECLARE_NATIVE_TYPE(WarfareGame,USeqAct_GetCash);
DECLARE_NATIVE_TYPE(WarfareGame,USeqAct_GetTeammate);
DECLARE_NATIVE_TYPE(WarfareGame,UUIContainer);
DECLARE_NATIVE_TYPE(WarfareGame,UUIElement);
DECLARE_NATIVE_TYPE(WarfareGame,UUIElement_Icon);
DECLARE_NATIVE_TYPE(WarfareGame,UUIElement_Label);
DECLARE_NATIVE_TYPE(WarfareGame,UWarActorFactoryAI);
DECLARE_NATIVE_TYPE(WarfareGame,AWarAIController);
DECLARE_NATIVE_TYPE(WarfareGame,UWarAnim_BaseBlendNode);
DECLARE_NATIVE_TYPE(WarfareGame,UWarAnim_CoverBlendNode);
DECLARE_NATIVE_TYPE(WarfareGame,UWarAnim_CoverFireBlendNode);
DECLARE_NATIVE_TYPE(WarfareGame,UWarAnim_CoverMoveBlendNode);
DECLARE_NATIVE_TYPE(WarfareGame,UWarAnim_CoverSequenceNode);
DECLARE_NATIVE_TYPE(WarfareGame,UWarAnim_EvadeBlendNode);
DECLARE_NATIVE_TYPE(WarfareGame,UWarCheatManager);
DECLARE_NATIVE_TYPE(WarfareGame,AWarHUD);
DECLARE_NATIVE_TYPE(WarfareGame,AWarPawn);
DECLARE_NATIVE_TYPE(WarfareGame,AWarPC);
DECLARE_NATIVE_TYPE(WarfareGame,AWarPRI);
DECLARE_NATIVE_TYPE(WarfareGame,AWarScout);
DECLARE_NATIVE_TYPE(WarfareGame,AWarTeamInfo);
DECLARE_NATIVE_TYPE(WarfareGame,AWarWeapon);

#define AUTO_INITIALIZE_REGISTRANTS_WARFAREGAME \
	UAnimNodeBlendByPhysics::StaticClass(); \
	AProj_Grenade::StaticClass(); \
	USeqAct_AIShootAtTarget::StaticClass(); \
	USeqAct_GetCash::StaticClass(); \
	USeqAct_GetTeammate::StaticClass(); \
	UUIContainer::StaticClass(); \
	GNativeLookupFuncs[Lookup++] = &FindWarfareGameUUIContainerNative; \
	UUIElement::StaticClass(); \
	UUIElement_Icon::StaticClass(); \
	UUIElement_Label::StaticClass(); \
	UWarActorFactoryAI::StaticClass(); \
	AWarAIController::StaticClass(); \
	GNativeLookupFuncs[Lookup++] = &FindWarfareGameAWarAIControllerNative; \
	UWarAnim_BaseBlendNode::StaticClass(); \
	UWarAnim_CoverBlendNode::StaticClass(); \
	UWarAnim_CoverFireBlendNode::StaticClass(); \
	UWarAnim_CoverMoveBlendNode::StaticClass(); \
	UWarAnim_CoverSequenceNode::StaticClass(); \
	UWarAnim_EvadeBlendNode::StaticClass(); \
	UWarCheatManager::StaticClass(); \
	AWarHUD::StaticClass(); \
	AWarPawn::StaticClass(); \
	GNativeLookupFuncs[Lookup++] = &FindWarfareGameAWarPawnNative; \
	AWarPC::StaticClass(); \
	AWarPRI::StaticClass(); \
	AWarScout::StaticClass(); \
	AWarTeamInfo::StaticClass(); \
	AWarWeapon::StaticClass(); \

#endif // WARFAREGAME_NATIVE_DEFS

#ifdef NATIVES_ONLY
NATIVE_INFO(UUIContainer) GWarfareGameUUIContainerNatives[] = 
{ 
	MAP_NATIVE(UUIContainer,execFindElement)
	MAP_NATIVE(UUIContainer,execDraw)
	{NULL,NULL}
};
IMPLEMENT_NATIVE_HANDLER(WarfareGame,UUIContainer);

NATIVE_INFO(AWarAIController) GWarfareGameAWarAIControllerNatives[] = 
{ 
	MAP_NATIVE(AWarAIController,execAILog)
	{NULL,NULL}
};
IMPLEMENT_NATIVE_HANDLER(WarfareGame,AWarAIController);

NATIVE_INFO(AWarPawn) GWarfareGameAWarPawnNatives[] = 
{ 
	MAP_NATIVE(AWarPawn,execJumpDownLedge)
	{NULL,NULL}
};
IMPLEMENT_NATIVE_HANDLER(WarfareGame,AWarPawn);

#endif // NATIVES_ONLY
#endif // STATIC_LINKING_MOJO

#ifdef VERIFY_CLASS_SIZES
VERIFY_CLASS_SIZE_NODIE(UAnimNodeBlendByPhysics)
VERIFY_CLASS_SIZE_NODIE(AProj_Grenade)
VERIFY_CLASS_SIZE_NODIE(USeqAct_AIShootAtTarget)
VERIFY_CLASS_SIZE_NODIE(USeqAct_GetCash)
VERIFY_CLASS_SIZE_NODIE(USeqAct_GetTeammate)
VERIFY_CLASS_SIZE_NODIE(UUIContainer)
VERIFY_CLASS_SIZE_NODIE(UUIElement)
VERIFY_CLASS_SIZE_NODIE(UUIElement_Icon)
VERIFY_CLASS_SIZE_NODIE(UUIElement_Label)
VERIFY_CLASS_SIZE_NODIE(UWarActorFactoryAI)
VERIFY_CLASS_SIZE_NODIE(AWarAIController)
VERIFY_CLASS_SIZE_NODIE(UWarAnim_BaseBlendNode)
VERIFY_CLASS_SIZE_NODIE(UWarAnim_CoverBlendNode)
VERIFY_CLASS_SIZE_NODIE(UWarAnim_CoverFireBlendNode)
VERIFY_CLASS_SIZE_NODIE(UWarAnim_CoverMoveBlendNode)
VERIFY_CLASS_SIZE_NODIE(UWarAnim_CoverSequenceNode)
VERIFY_CLASS_SIZE_NODIE(UWarAnim_EvadeBlendNode)
VERIFY_CLASS_SIZE_NODIE(UWarCheatManager)
VERIFY_CLASS_SIZE_NODIE(AWarHUD)
VERIFY_CLASS_SIZE_NODIE(AWarPawn)
VERIFY_CLASS_SIZE_NODIE(AWarPC)
VERIFY_CLASS_SIZE_NODIE(AWarPRI)
VERIFY_CLASS_SIZE_NODIE(AWarScout)
VERIFY_CLASS_SIZE_NODIE(AWarTeamInfo)
VERIFY_CLASS_SIZE_NODIE(AWarWeapon)
#endif // VERIFY_CLASS_SIZES
