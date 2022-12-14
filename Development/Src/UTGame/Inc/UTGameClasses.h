/*===========================================================================
    C++ class definitions exported from UnrealScript.
    This is automatically generated by the tools.
    DO NOT modify this manually! Edit the corresponding .uc files instead!
===========================================================================*/
#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,4)
#endif


#ifndef NAMES_ONLY
#define AUTOGENERATE_NAME(name) extern FName UTGAME_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#endif

AUTOGENERATE_NAME(AnimFire)
AUTOGENERATE_NAME(AnimStopFire)
AUTOGENERATE_NAME(DelayedWarning)
AUTOGENERATE_NAME(GetBlendTime)
AUTOGENERATE_NAME(MonitoredPawnAlert)

#ifndef NAMES_ONLY


struct UTBot_eventMonitoredPawnAlert_Parms
{
};
struct UTBot_eventDelayedWarning_Parms
{
};
class AUTBot : public AAIController
{
public:
    BITFIELD bHuntPlayer:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD bEnemyInfoValid:1;
    FLOAT WarningDelay GCC_PACK(PROPERTY_ALIGNMENT);
    class AProjectile* WarningProjectile;
    FVector MonitorStartLoc;
    class APawn* MonitoredPawn;
    FLOAT MonitorMaxDistSq;
    FVector LastSeenPos;
    FVector LastSeeingPos;
    FLOAT LastSeenTime;
    DECLARE_FUNCTION(execCanMakePathTo);
    DECLARE_FUNCTION(execWaitToSeeEnemy);
    void eventMonitoredPawnAlert()
    {
        ProcessEvent(FindFunctionChecked(UTGAME_MonitoredPawnAlert),NULL);
    }
    void eventDelayedWarning()
    {
        ProcessEvent(FindFunctionChecked(UTGAME_DelayedWarning),NULL);
    }
    DECLARE_CLASS(AUTBot,AAIController,0,UTGame)
	DECLARE_FUNCTION(execPollWaitToSeeEnemy)
	UBOOL Tick( FLOAT DeltaSeconds, ELevelTick TickType );
	virtual void UpdateEnemyInfo(APawn* AcquiredEnemy);
};

enum EWeaponHand
{
    HAND_Hidden             =0,
    HAND_Centered           =1,
    HAND_Left               =2,
    HAND_Right              =3,
    HAND_MAX                =4,
};

class AUTPawn : public APawn
{
public:
    BITFIELD bNoTeamBeacon:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD bBehindView:1;
    BITFIELD bFixedView:1;
    BITFIELD bCanDoubleJump:1;
    FVector FixedViewLoc GCC_PACK(PROPERTY_ALIGNMENT);
    FRotator FixedViewRot;
    TArrayNoInit<class UClass*> DefaultInventory;
    INT spree;
    FLOAT DodgeSpeed;
    FLOAT DodgeSpeedZ;
    BYTE CurrentDir;
    BYTE WeaponHand;
    BYTE FlashCount;
    BYTE FiringMode;
    INT MultiJumpRemaining GCC_PACK(PROPERTY_ALIGNMENT);
    INT MaxMultiJump;
    INT MultiJumpBoost;
    INT SuperHealthMax;
    FLOAT ShieldStrength;
    class UClass* SoundGroupClass;
    class UClass* CurrentWeaponAttachmentClass;
    class AUTWeaponAttachment* CurrentWeaponAttachment;
    FVector FlashLocation;
    FName WeaponBone;
    DECLARE_CLASS(AUTPawn,APawn,0|CLASS_Config,UTGame)
	virtual UBOOL TryJumpUp(FVector Dir, DWORD TraceFlags);
};


struct UTAnimBlendByWeapon_eventAnimStopFire_Parms
{
    FLOAT SpecialBlendTime;
};
struct UTAnimBlendByWeapon_eventAnimFire_Parms
{
    FName FireSequence;
    BITFIELD bAutoFire;
    FLOAT AnimRate;
    FLOAT SpecialBlendTime;
};
class UUTAnimBlendByWeapon : public UAnimNodeBlendPerBone
{
public:
    BITFIELD bLooping:1 GCC_PACK(PROPERTY_ALIGNMENT);
    FLOAT BlendTime GCC_PACK(PROPERTY_ALIGNMENT);
    void eventAnimStopFire(FLOAT SpecialBlendTime)
    {
        UTAnimBlendByWeapon_eventAnimStopFire_Parms Parms;
        Parms.SpecialBlendTime=SpecialBlendTime;
        ProcessEvent(FindFunctionChecked(UTGAME_AnimStopFire),&Parms);
    }
    void eventAnimFire(FName FireSequence, BITFIELD bAutoFire, FLOAT AnimRate, FLOAT SpecialBlendTime)
    {
        UTAnimBlendByWeapon_eventAnimFire_Parms Parms;
        Parms.FireSequence=FireSequence;
        Parms.bAutoFire=bAutoFire;
        Parms.AnimRate=AnimRate;
        Parms.SpecialBlendTime=SpecialBlendTime;
        ProcessEvent(FindFunctionChecked(UTGAME_AnimFire),&Parms);
    }
    DECLARE_CLASS(UUTAnimBlendByWeapon,UAnimNodeBlendPerBone,0,UTGame)
	virtual void OnChildAnimEnd(UAnimNodeSequence* Child);
};


struct UTAnimBlendBase_eventGetBlendTime_Parms
{
    INT ChildIndex;
    BITFIELD bGetDefault;
    FLOAT ReturnValue;
};
class UUTAnimBlendBase : public UAnimNodeBlendList
{
public:
    FLOAT BlendTime;
    TArrayNoInit<FLOAT> ChildBlendTimes;
    FLOAT eventGetBlendTime(INT ChildIndex, BITFIELD bGetDefault)
    {
        UTAnimBlendBase_eventGetBlendTime_Parms Parms;
        Parms.ReturnValue=0;
        Parms.ChildIndex=ChildIndex;
        Parms.bGetDefault=bGetDefault;
        ProcessEvent(FindFunctionChecked(UTGAME_GetBlendTime),&Parms);
        return Parms.ReturnValue;
    }
    DECLARE_CLASS(UUTAnimBlendBase,UAnimNodeBlendList,0,UTGame)
    NO_DEFAULT_CONSTRUCTOR(UUTAnimBlendBase)
};

enum EBlendDirTypes
{
    FBDir_Forward           =0,
    FBDir_Back              =1,
    FBDir_Left              =2,
    FBDir_Right             =3,
    FBDir_None              =4,
    FBDir_MAX               =5,
};

class UUTAnimBlendByDirection : public UUTAnimBlendBase
{
public:
    BITFIELD bAdjustRateByVelocity:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BYTE LastDirection GCC_PACK(PROPERTY_ALIGNMENT);
    DECLARE_CLASS(UUTAnimBlendByDirection,UUTAnimBlendBase,0,UTGame)
	// AnimNode interface
	virtual	void TickAnim( float DeltaSeconds, float TotalWeight  );
	virtual void SetActiveChild( INT ChildIndex, FLOAT BlendTime );
	virtual EBlendDirTypes Get4WayDir();
};

enum EDodgeBlends
{
    DODGEBLEND_None         =0,
    DODGEBLEND_Forward      =1,
    DODGEBLEND_Backward     =2,
    DODGEBLEND_Left         =3,
    DODGEBLEND_Right        =4,
    DODGEBLEND_MAX          =5,
};

class UUTAnimBlendByDodge : public UUTAnimBlendByDirection
{
public:
    FName DodgeAnims[16];
    BYTE CurrentDodge;
    DECLARE_CLASS(UUTAnimBlendByDodge,UUTAnimBlendByDirection,0,UTGame)
	virtual	void TickAnim( float DeltaSeconds, float TotalWeight  );
};

enum EBlendFallTypes
{
    FBT_Up                  =0,
    FBT_Down                =1,
    FBT_PreLand             =2,
    FBT_Land                =3,
    FBT_None                =4,
    FBT_MAX                 =5,
};

class UUTAnimBlendByFall : public UUTAnimBlendBase
{
public:
    BITFIELD bDodgeFall:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BYTE FallState GCC_PACK(PROPERTY_ALIGNMENT);
    FLOAT LastFallingVelocity GCC_PACK(PROPERTY_ALIGNMENT);
    DECLARE_CLASS(UUTAnimBlendByFall,UUTAnimBlendBase,0,UTGame)
	virtual	void TickAnim( float DeltaSeconds, float TotalWeight  );
	virtual void SetActiveChild( INT ChildIndex, FLOAT BlendTime );
	virtual void OnChildAnimEnd(UAnimNodeSequence* Child);
	virtual void ChangeFallState(EBlendFallTypes NewState);
};


class UUTAnimBlendByIdle : public UUTAnimBlendBase
{
public:
    DECLARE_CLASS(UUTAnimBlendByIdle,UUTAnimBlendBase,0,UTGame)
	// AnimNode interface
	virtual	void TickAnim( float DeltaSeconds, FLOAT TotalWeight  );
};


class UUTAnimBlendByPhysics : public UUTAnimBlendBase
{
public:
    INT PhysicsMap[12];
    INT LastPhysics;
    DECLARE_CLASS(UUTAnimBlendByPhysics,UUTAnimBlendBase,0,UTGame)
	virtual	void TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight  );
};


class UUTAnimBlendByPosture : public UUTAnimBlendBase
{
public:
    DECLARE_CLASS(UUTAnimBlendByPosture,UUTAnimBlendBase,0,UTGame)
	virtual	void TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight  );
};


class UUTAnimNodeSequence : public UAnimNodeSequence
{
public:
    BITFIELD bAutoStart:1 GCC_PACK(PROPERTY_ALIGNMENT);
    BITFIELD bResetOnActivate:1;
    DECLARE_CLASS(UUTAnimNodeSequence,UAnimNodeSequence,0,UTGame)
	virtual void OnBecomeActive();
};


class UUTCheatManager : public UCheatManager
{
public:
    DECLARE_CLASS(UUTCheatManager,UCheatManager,0,UTGame)
    NO_DEFAULT_CONSTRUCTOR(UUTCheatManager)
};

#endif

AUTOGENERATE_FUNCTION(AUTBot,-1,execCanMakePathTo);
AUTOGENERATE_FUNCTION(AUTBot,-1,execWaitToSeeEnemy);

#ifndef NAMES_ONLY
#undef AUTOGENERATE_NAME
#undef AUTOGENERATE_FUNCTION
#endif

#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

#ifdef STATIC_LINKING_MOJO
#ifndef UTGAME_NATIVE_DEFS
#define UTGAME_NATIVE_DEFS

DECLARE_NATIVE_TYPE(UTGame,UUTAnimBlendBase);
DECLARE_NATIVE_TYPE(UTGame,UUTAnimBlendByDirection);
DECLARE_NATIVE_TYPE(UTGame,UUTAnimBlendByDodge);
DECLARE_NATIVE_TYPE(UTGame,UUTAnimBlendByFall);
DECLARE_NATIVE_TYPE(UTGame,UUTAnimBlendByIdle);
DECLARE_NATIVE_TYPE(UTGame,UUTAnimBlendByPhysics);
DECLARE_NATIVE_TYPE(UTGame,UUTAnimBlendByPosture);
DECLARE_NATIVE_TYPE(UTGame,UUTAnimBlendByWeapon);
DECLARE_NATIVE_TYPE(UTGame,UUTAnimNodeSequence);
DECLARE_NATIVE_TYPE(UTGame,AUTBot);
DECLARE_NATIVE_TYPE(UTGame,UUTCheatManager);
DECLARE_NATIVE_TYPE(UTGame,AUTPawn);

#define AUTO_INITIALIZE_REGISTRANTS_UTGAME \
	UUTAnimBlendBase::StaticClass(); \
	UUTAnimBlendByDirection::StaticClass(); \
	UUTAnimBlendByDodge::StaticClass(); \
	UUTAnimBlendByFall::StaticClass(); \
	UUTAnimBlendByIdle::StaticClass(); \
	UUTAnimBlendByPhysics::StaticClass(); \
	UUTAnimBlendByPosture::StaticClass(); \
	UUTAnimBlendByWeapon::StaticClass(); \
	UUTAnimNodeSequence::StaticClass(); \
	AUTBot::StaticClass(); \
	GNativeLookupFuncs[Lookup++] = &FindUTGameAUTBotNative; \
	UUTCheatManager::StaticClass(); \
	AUTPawn::StaticClass(); \

#endif // UTGAME_NATIVE_DEFS

#ifdef NATIVES_ONLY
NATIVE_INFO(AUTBot) GUTGameAUTBotNatives[] = 
{ 
	MAP_NATIVE(AUTBot,execCanMakePathTo)
	MAP_NATIVE(AUTBot,execWaitToSeeEnemy)
	{NULL,NULL}
};
IMPLEMENT_NATIVE_HANDLER(UTGame,AUTBot);

#endif // NATIVES_ONLY
#endif // STATIC_LINKING_MOJO

#ifdef VERIFY_CLASS_SIZES
VERIFY_CLASS_SIZE_NODIE(UUTAnimBlendBase)
VERIFY_CLASS_SIZE_NODIE(UUTAnimBlendByDirection)
VERIFY_CLASS_SIZE_NODIE(UUTAnimBlendByDodge)
VERIFY_CLASS_SIZE_NODIE(UUTAnimBlendByFall)
VERIFY_CLASS_SIZE_NODIE(UUTAnimBlendByIdle)
VERIFY_CLASS_SIZE_NODIE(UUTAnimBlendByPhysics)
VERIFY_CLASS_SIZE_NODIE(UUTAnimBlendByPosture)
VERIFY_CLASS_SIZE_NODIE(UUTAnimBlendByWeapon)
VERIFY_CLASS_SIZE_NODIE(UUTAnimNodeSequence)
VERIFY_CLASS_SIZE_NODIE(AUTBot)
VERIFY_CLASS_SIZE_NODIE(UUTCheatManager)
VERIFY_CLASS_SIZE_NODIE(AUTPawn)
#endif // VERIFY_CLASS_SIZES
