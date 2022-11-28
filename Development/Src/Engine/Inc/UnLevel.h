/*=============================================================================
	UnLevel.h: ULevel definition.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

/*-----------------------------------------------------------------------------
	Network notification sink.
-----------------------------------------------------------------------------*/

//
// Accepting connection responses.
//
enum EAcceptConnection
{
	ACCEPTC_Reject,	// Reject the connection.
	ACCEPTC_Accept, // Accept the connection.
	ACCEPTC_Ignore, // Ignore it, sending no reply, while server travelling.
};

//
// The net code uses this to send notifications.
//
class FNetworkNotify
{
public:
	virtual EAcceptConnection NotifyAcceptingConnection()=0;
	virtual void NotifyAcceptedConnection( class UNetConnection* Connection )=0;
	virtual UBOOL NotifyAcceptingChannel( class UChannel* Channel )=0;
	virtual ULevel* NotifyGetLevel()=0;
	virtual void NotifyReceivedText( UNetConnection* Connection, const TCHAR* Text )=0;
	virtual UBOOL NotifySendingFile( UNetConnection* Connection, FGuid GUID )=0;
	virtual void NotifyReceivedFile( UNetConnection* Connection, INT PackageIndex, const TCHAR* Error, UBOOL Skipped )=0;
	virtual void NotifyProgress( const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds )=0;
};

/**
 * Encapsulates the data and logic used to determine visibility of a primitive.
 */
class FLevelVisibilitySet
{
public:

	/**
	 * Encapsulates the intersection of a visibility set and another volume.
	 */
	class FSubset
	{
		friend class FLevelVisibilitySet;
	public:

		/**
		 * Checks whether this subset is empty.
		 *
		 * @return	One if the subset doesn't contain any points, zero otherwise.
		 */
		UBOOL IsEmpty() const { return VisibleZones.IsEmpty(); }

	protected:

		/** The set of zones which have visibility volumes in this subset. */
		FZoneSet VisibleZones;

		/** The set of zones which have visibility volumes that entirely contain this subset. */
		FZoneSet EntirelyVisibleZones;

		// Constructor.
		FSubset(const FZoneSet& InVisibleZones,const FZoneSet& InEntirelyVisibleZones):
			VisibleZones(InVisibleZones),
			EntirelyVisibleZones(InEntirelyVisibleZones)
		{}
	};

	// Constructor.
	FLevelVisibilitySet(ULevel* InLevel,const FPlane& InViewOrigin,const FZoneSet& InIgnoreZones,UBOOL InUsePortals):
		Level(InLevel),
		ViewOrigin(InViewOrigin),
		VisibleZones(FZoneSet::NoZones()),
		IgnoreZones(InIgnoreZones),
		UsePortals(InUsePortals)
	{}

	/**
	 * Returns a visibility subset for the intersection of a box with another subset.
	 *
	 * @param	Subset - The subset to intersect.
	 *
	 * @param	BoxOrigin - The origin of the box to intersect.
	 *
	 * @param	BoxExtent - The extent of the box to intersect.
	 *
	 * @return	The subset contained by both Box and Subset.
	 */
	FSubset GetBoxIntersectionSubset(const FSubset& Subset,const FVector& BoxOrigin,const FVector& BoxExtent) const;

	/**
	 * Checks to see if a primitive is contained by either a visibility subset or the entire set.
	 * The primitive must intersect the volume used to create the subset.
	 * The only fully determined return value is that the primitive is not in either the visibility
	 * set or the given subset.  Another possibility is that it can be determined to be in the
	 * visibility set, but not necessarily the subset.  The final possibility is the primitive is
	 * not in the subset, but it isn't determined whether it is in the full set or not.
	 *
	 * @param	Subset - A subset the primitive intersects.
	 *
	 * @param	Primitive - The primitive to check for containment.
	 *
	 * @return	CR_ContainedBySet - The primitive is contained by the visibility set, but not necessarily the subset.
	 *			CR_NotContainedBySet - The primitive is not contained by the visibility set.
	 *			CR_NotContainedBySubset - The primitive is not contained by the given subset, but it may be contained
	 *										by another subset of the visibility set.
	 */
	enum EContainmentResult
	{
		CR_ContainedBySet,
		CR_NotContainedBySet,
		CR_NotContainedBySubset
	};
	EContainmentResult ContainsPrimitive(UPrimitiveComponent* Primitive,const FSubset& Subset) const;

	/**
	 * @return	A subset which is used to refer to the entire visibility set.
	 */
	FSubset GetFullSubset() const;

	/**
	 * Adds a visibility volume to a specific zone.
	 * Extends the portals visible through the new visibility volume and recurses with the extended visibility volumes.
	 *
	 * @param	ZoneIndex - The index of the zone the Volume is a visibility volume for.
	 *
	 * @param	Volume - The visibility volume
	 */
	void AddVisibilityVolume(INT ZoneIndex,const FConvexVolume& Volume);

	/**
	 * Adds a visibility volume for all zones.
	 *
	 * @param	Volume - The visibility volume
	 */
	void AddGlobalVisibilityVolume(const FConvexVolume& Volume);

protected:

	/** The level this represents visibility for. */
	ULevel* Level;

	/** A set of convex volumes which contain visible points in a specific zone. */
	TMultiMap<INT,FConvexVolume> VisibilityVolumes;

	/** The set of zones which are possibly visible. */
	FZoneSet VisibleZones;

	/** The set of zones which should be entirely ignored. */
	FZoneSet IgnoreZones;

	/**
	 * The point which visibility is being computed for.  Uses a homogenous vector to represent both finite points
	 * and infinitely distant points(directions).
	 */
	FPlane ViewOrigin;

	/** True if the viewer can see through portals. */
	UBOOL UsePortals;
};

/**
 * An interface for making spatial queries against the primitives in a level.
 */
class FPrimitiveHashBase
{
public:
	// FPrimitiveHashBase interface.
	virtual ~FPrimitiveHashBase() {};
	virtual void Tick()=0;
	virtual void AddPrimitive( UPrimitiveComponent* Primitive )=0;
	virtual void RemovePrimitive( UPrimitiveComponent* Primitive )=0;
	virtual FCheckResult* ActorLineCheck( FMemStack& Mem, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags, AActor *SourceActor )=0;
	virtual FCheckResult* ActorPointCheck( FMemStack& Mem, const FVector& Location, const FVector& Extent, DWORD TraceFlags, UBOOL bSingleResult=0 )=0;
	virtual FCheckResult* ActorRadiusCheck( FMemStack& Mem, const FVector& Location, FLOAT Radius )=0;
	virtual FCheckResult* ActorEncroachmentCheck( FMemStack& Mem, AActor* Actor, FVector Location, FRotator Rotation, DWORD TraceFlags )=0;
	virtual FCheckResult* ActorOverlapCheck( FMemStack& Mem, AActor* Actor, const FBox& Box, UBOOL bBlockRigidBodyOnly)=0;

	virtual void GetVisiblePrimitives(const FLevelVisibilitySet& VisibilitySet,TArray<UPrimitiveComponent*>& Primitives) = 0;
	virtual void GetIntersectingPrimitives(const FBox& Box,TArray<UPrimitiveComponent*>& Primitives) = 0;

	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar) = 0;
};

FPrimitiveHashBase* GNewCollisionHash(ULevel* InLevel);

/*-----------------------------------------------------------------------------
	ULevel base.
-----------------------------------------------------------------------------*/

//
// A game level.
//
class ULevelBase : public UObject, public FNetworkNotify
{
	DECLARE_ABSTRACT_CLASS(ULevelBase,UObject,0,Engine)

	// Database.
	TTransArray<AActor*> Actors;

	// Variables.
	class UNetDriver*	NetDriver;
	class UEngine*		Engine;
	FURL				URL;

	// Constructors.
	ULevelBase( UEngine* InOwner, const FURL& InURL=FURL(NULL) );

	// UObject interface.
	void Serialize( FArchive& Ar );
	void Destroy();

	// FNetworkNotify interface.
	void NotifyProgress( const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds );

protected:
	ULevelBase()
	: Actors( this )
	{}
};

//
//	ULineBatchComponent
//

class ULineBatchComponent : public UPrimitiveComponent, public FPrimitiveRenderInterface
{
	DECLARE_CLASS(ULineBatchComponent,UPrimitiveComponent,0,Engine);

	struct FLine
	{
		FVector	Start,
				End;
		FColor	Color;
		FLOAT	RemainingLifeTime;

		FLine(const FVector& InStart,const FVector& InEnd,FColor InColor,FLOAT InLifeTime):
			Start(InStart),
			End(InEnd),
			Color(InColor),
			RemainingLifeTime(InLifeTime)
		{}
	};

	TArray<FLine>	BatchedLines;
	FLOAT			DefaultLifeTime;

	// FPrimitiveRenderInterface interface.

	virtual FViewport* GetViewport() { return NULL; }
	virtual UBOOL IsHitTesting() { return 0; }

	virtual void DrawLine(const FVector& Start,const FVector& End,FColor Color);
	virtual void DrawLine(const FVector& Start,const FVector& End,FColor Color,FLOAT LifeTime);

	// UPrimitiveComponent interface.

	virtual void Tick(FLOAT DeltaTime);
	virtual void Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
};

/*-----------------------------------------------------------------------------
	ULevel class.
-----------------------------------------------------------------------------*/

//
// Trace options.
//
enum ETraceFlags
{
	// Bitflags.
	TRACE_Pawns				= 0x00001, // Check collision with pawns.
	TRACE_Movers			= 0x00002, // Check collision with movers.
	TRACE_Level				= 0x00004, // Check collision with BSP level geometry.
	TRACE_Volumes			= 0x00008, // Check collision with soft volume boundaries.
	TRACE_Others			= 0x00010, // Check collision with all other kinds of actors.
	TRACE_OnlyProjActor		= 0x00020, // Check collision with other actors only if they are projectile targets
	TRACE_Blocking			= 0x00040, // Check collision with other actors only if they block the check actor
	TRACE_LevelGeometry		= 0x00080, // Check collision with other actors which are static level geometry
	TRACE_ShadowCast		= 0x00100, // Check collision with shadow casting actors
	TRACE_StopAtFirstHit	= 0x00200, // Stop when find any collision (for visibility checks)
	TRACE_SingleResult		= 0x00400, // Stop when find guaranteed first nearest collision (for SingleLineCheck)
	TRACE_Debug				= 0x00800, // used for debugging specific traces
	TRACE_Material			= 0x01000, // Request that Hit.Material return the material the trace hit.
	TRACE_Projectors		= 0x02000, // Check collision with projectors
	TRACE_AcceptProjectors	= 0x04000, // Check collision with Actors with bAcceptsProjectors == true
	TRACE_Visible			= 0x08000,
	TRACE_Terrain			= 0x10000, // Check collision with terrain
	TRACE_Water				= 0x20000, // Check collision with water volumes (must have TRACE_Volumes also set)

	// Combinations.
	TRACE_Actors		= TRACE_Pawns | TRACE_Movers | TRACE_Others | TRACE_LevelGeometry | TRACE_Terrain,
	TRACE_AllColliding  = TRACE_Level | TRACE_Actors | TRACE_Volumes,
	TRACE_ProjTargets	= TRACE_OnlyProjActor | TRACE_AllColliding,
	TRACE_AllBlocking	= TRACE_Blocking | TRACE_AllColliding,
	TRACE_World = TRACE_Level | TRACE_Movers | TRACE_LevelGeometry | TRACE_Terrain,
	TRACE_Hash = TRACE_Pawns | TRACE_Movers | TRACE_Volumes | TRACE_Others | TRACE_LevelGeometry | TRACE_Terrain,
};

//
// Level updating.
//
#if _MSC_VER
enum ELevelTick
{
	LEVELTICK_TimeOnly		= 0,	// Update the level time only.
	LEVELTICK_ViewportsOnly	= 1,	// Update time and viewports.
	LEVELTICK_All			= 2,	// Update all.
};
#endif

/**
 * Stores the visibility set for a particular light.
 */
struct FLightVisibility
{
	ULightComponent*	Light;
	FLevelVisibilitySet	VisibilitySet;

	// Constructor.
	FLightVisibility(ULightComponent* InLight,ULevel* InLevel):
		Light(InLight),
		VisibilitySet(InLevel,InLight->GetPosition(),FZoneSet::NoZones(),1)
	{}
};

//
//	ELevelViewportType
//

enum ELevelViewportType
{
	LVT_None = -1,
	LVT_OrthoXY = 0,
	LVT_OrthoXZ = 1,
	LVT_OrthoYZ = 2,
	LVT_Perspective = 3
};

struct FLevelViewportInfo
{
	FVector CamPosition;
	FRotator CamRotation;
	FLOAT CamOrthoZoom;

	FLevelViewportInfo() {}

	FLevelViewportInfo(const FVector& InCamPosition, const FRotator& InCamRotation, FLOAT InCamOrthoZoom)
	{
		CamPosition = InCamPosition;
		CamRotation = InCamRotation;
		CamOrthoZoom = InCamOrthoZoom;
	}

	friend FArchive& operator<<( FArchive& Ar, FLevelViewportInfo& I )
	{
		return Ar << I.CamPosition << I.CamRotation << I.CamOrthoZoom;
	}
};

//
// The level object.  Contains the level's actor list, Bsp information, and brush list.
//
class ULevel : public ULevelBase, public FScene
{
	DECLARE_CLASS(ULevel,ULevelBase,0,Engine)
	NO_DEFAULT_CONSTRUCTOR(ULevel)

	// Main variables, always valid.
	UModel*										Model;
	TArray<UModelComponent*>					ModelComponents;
	DOUBLE										TimeSeconds;
	TMap<FString,FString>						TravelInfo;
	class USequence*							GameSequence;

	// Only valid in memory.
	TDynamicMap<UPrimitiveComponent*,DOUBLE>	DynamicPrimitives;
	FPrimitiveHashBase*							Hash;
	AActor*										FirstDeleted;
	struct FActorLink*							NewlySpawned;
	UBOOL										InTick, Ticked;
	INT											iFirstDynamicActor, iFirstNetRelevantActor, NetTag;
	INT											PlayerNum;	// Counter for allocating game-unique controller player numbers.

	TArray<FLightVisibility>					Lights;
	ULineBatchComponent*						LineBatcher;
	ULineBatchComponent*						PersistentLineBatcher;

	TArray<UPrimitiveComponent*>				PhysPrimComponents;

	UBOOL										bShowLineChecks;
	UBOOL										bShowExtentLineChecks;
	UBOOL										bShowPointChecks;

	/** Saved editor viewport states - one for each view type. Indexed using ELevelViewportType above. */
	FLevelViewportInfo							EditorViews[4];

#ifdef WITH_NOVODEX
	class NxScene*								NovodexScene;
	class NxActor*								LevelBSPActor;
	class NovodexDebugRenderer*					DebugRenderer;
#endif // WITH_NOVODEX

	/** Array of information needed for streaming static textures */
	TArray<FStreamableTextureInfo>				StaticStreamableTextureInfos;

	// Constructor.
	ULevel( UEngine* InEngine, UBOOL RootOutside );

	// UObject interface.
	void Serialize( FArchive& Ar );
	void Destroy();
	void PostLoad();
	/**
	 * Presave function, gets called once before the level gets serialized (multiple times) for saving.
	 * Used to rebuild streaming data on save.
	 */
	virtual void PreSave();

	// ULevel interface.
	virtual void Modify( UBOOL DoTransArrays=0 );
	virtual void Tick( ELevelTick TickType, FLOAT DeltaSeconds );
	virtual void TickNetClient( FLOAT DeltaSeconds );
	virtual void TickNetServer( FLOAT DeltaSeconds );
	virtual INT ServerTickClients( FLOAT DeltaSeconds );
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	virtual void ShrinkLevel();
	virtual void CompactActors();
	virtual UBOOL Listen( FString& Error );
	virtual UBOOL IsServer();
	virtual UBOOL MoveActor( AActor *Actor, const FVector& Delta, const FRotator& NewRotation, FCheckResult &Hit, UBOOL Test=0, UBOOL IgnorePawns=0, UBOOL bIgnoreBases=0, UBOOL bNoFail=0 );
	virtual UBOOL FarMoveActor( AActor* Actor, const FVector& DestLocation, UBOOL Test=0, UBOOL bNoCheck=0, UBOOL bAttachedMove=0 );
	UBOOL EditorDestroyActor( AActor* Actor );
	virtual UBOOL DestroyActor( AActor* Actor, UBOOL bNetForce=0, UBOOL bDeferRefCleanup=false );
	virtual void CleanupDestroyed( UBOOL bForce );
	virtual AActor* SpawnActor( UClass* Class, FName InName=NAME_None, const FVector& Location=FVector(0,0,0), const FRotator& Rotation=FRotator(0,0,0), AActor* Template=NULL, UBOOL bNoCollisionFail=0, UBOOL bRemoteOwned=0, AActor* Owner=NULL, APawn* Instigator=NULL, UBOOL bNoFail=0 );
	virtual ABrush*	SpawnBrush();
	virtual APlayerController* SpawnPlayActor( UPlayer* Player, ENetRole RemoteRole, const FURL& URL, FString& Error );
	virtual UBOOL FindSpot( const FVector& Extent, FVector& Location );
	virtual UBOOL CheckSlice( FVector& Location, const FVector& Extent, INT& bKeepTrying );
	virtual UBOOL CheckEncroachment( AActor* Actor, FVector TestLocation, FRotator TestRotation, UBOOL bTouchNotify );
	virtual UBOOL SinglePointCheck( FCheckResult& Hit, const FVector& Location, const FVector& Extent, ALevelInfo* Level, UBOOL bActors );
	virtual UBOOL EncroachingWorldGeometry( FCheckResult& Hit, const FVector& Location, const FVector& Extent, ALevelInfo* Level);
	virtual UBOOL SingleLineCheck( FCheckResult& Hit, AActor* SourceActor, const FVector& End, const FVector& Start, DWORD TraceFlags, const FVector& Extent=FVector(0,0,0) );
	virtual FCheckResult* MultiPointCheck( FMemStack& Mem, const FVector& Location, const FVector& Extent, ALevelInfo* Level, UBOOL bActors, UBOOL bOnlyWorldGeometry=0, UBOOL bSingleResult=0 );
	virtual FCheckResult* MultiLineCheck( FMemStack& Mem, const FVector& End, const FVector& Start, const FVector& Size, ALevelInfo* LevelInfo, DWORD TraceFlags, AActor* SourceActor );
	virtual void UpdateTime( ALevelInfo* Info );
	virtual UBOOL IsPaused();
	virtual void WelcomePlayer( UNetConnection* Connection, TCHAR* Optional=TEXT("") );

	/**
	 * Cleans up components, streaming data and assorted other intermediate data.	
	 */
	void CleanupLevel();
	/**
	 * Rebuilds static streaming data.	
	 */
	void BuildStreamingData();

	void InitLevelRBPhys();
	void TermLevelRBPhys();
	void TickLevelRBPhys(FLOAT DeltaSeconds);

	// BeginPlay - Begins gameplay in the level.

	void BeginPlay(FURL InURL,UPackageMap* PackageMap);

	// FNetworkNotify interface.
	EAcceptConnection NotifyAcceptingConnection();
	void NotifyAcceptedConnection( class UNetConnection* Connection );
	UBOOL NotifyAcceptingChannel( class UChannel* Channel );
	ULevel* NotifyGetLevel() {return this;}
	void NotifyReceivedText( UNetConnection* Connection, const TCHAR* Text );
	void NotifyReceivedFile( UNetConnection* Connection, INT PackageIndex, const TCHAR* Error, UBOOL Skipped );
	UBOOL NotifySendingFile( UNetConnection* Connection, FGuid GUID );

	// Accessors.
	ABrush* Brush()
	{
		check(Actors.Num()>=2);
		check(Cast<ABrush>(Actors(1))!=NULL);
		check(Cast<ABrush>(Actors(1))->BrushComponent);
		check(Cast<ABrush>(Actors(1))->Brush!=NULL);
		return Cast<ABrush>(Actors(1));
	}
	INT GetActorIndex( AActor* Actor )
	{
		for( INT i=0; i<Actors.Num(); i++ )
			if( Actors(i) == Actor )
				return i;
		appErrorf( TEXT("Actor not found: %s"), *Actor->GetFullName() );
		return INDEX_NONE;
	}
	ALevelInfo* GetLevelInfo()
	{
		check(Actors(0));
		checkSlow(Actors(0)->IsA(ALevelInfo::StaticClass()));
		return (ALevelInfo*)Actors(0);
	}
	AZoneInfo* GetZoneActor( INT iZone )
	{
		return Model->Zones[iZone].ZoneActor ? Model->Zones[iZone].ZoneActor : GetLevelInfo();
	}
	UBOOL ToFloor( AActor* InActor, UBOOL InAlign, AActor* InIgnoreActor );

	void ClearComponents();
	void UpdateComponents();

	/**
	 * Invalidates the cached data used to render the level's UModel.
	 */
	void InvalidateModelGeometry();

	/**
	 * Discards the cached data used to render the level's UModel.  Assumes that the
	 * faces and vertex positions haven't changed, only the applied materials.
	 *
	 * @param SurfIndex	The index of the surface that changed.
	 */
	void InvalidateModelSurface(INT SurfIndex);

	// FScene interface.

	virtual void AddPrimitive(UPrimitiveComponent* Primitive);
	virtual void RemovePrimitive(UPrimitiveComponent* Primitive);
	virtual void AddLight(ULightComponent* Light);
	virtual void RemoveLight(ULightComponent* Light);
	virtual void GetVisiblePrimitives(const FSceneContext& Context,const FConvexVolume& Frustum,TArray<UPrimitiveComponent*>& OutPrimitives);
	virtual void GetRelevantLights(UPrimitiveComponent* Primitive,TArray<ULightComponent*>& OutLights);
	virtual FLOAT GetSceneTime() { return GetLevelInfo()->TimeSeconds; }
};

/*-----------------------------------------------------------------------------
	Iterators.
-----------------------------------------------------------------------------*/

//
// Iterate through all static brushes in a level.
//
class FStaticBrushIterator
{
public:
	// Public constructor.
	FStaticBrushIterator( ULevel *InLevel )
	:	Level   ( InLevel   )
	,	Index   ( -1        )
	{
		checkSlow(Level!=NULL);
		++*this;
	}
	void operator++()
	{
		do
		{
			if( ++Index >= Level->Actors.Num() )
			{
				Level = NULL;
				break;
			}
		} while
		(	!Level->Actors(Index)
		||	!Level->Actors(Index)->IsStaticBrush() );
	}
	ABrush* operator* ()
	{
		checkSlow(Level);
		checkSlow(Index<Level->Actors.Num());
		checkSlow(Level->Actors(Index));
		checkSlow(Level->Actors(Index)->IsStaticBrush());
		return (ABrush*)Level->Actors(Index);
	}
	ABrush* operator-> ()
	{
		checkSlow(Level);
		checkSlow(Index<Level->Actors.Num());
		checkSlow(Level->Actors(Index));
		checkSlow(Level->Actors(Index)->IsStaticBrush());
		return (ABrush*)Level->Actors(Index);
	}
	operator UBOOL()
	{
		return Level != NULL;
	}
protected:
	ULevel*		Level;
	INT		    Index;
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

