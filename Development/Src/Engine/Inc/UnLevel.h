/*=============================================================================
	UnLevel.h: ULevel definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
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

/**
 * The net code uses this to send notifications.
 */
class FNetworkNotify
{
public:
	virtual EAcceptConnection NotifyAcceptingConnection() PURE_VIRTUAL(FNetworkNotify::NotifyAcceptedConnection,return ACCEPTC_Ignore;);
	virtual void NotifyAcceptedConnection( class UNetConnection* Connection ) PURE_VIRTUAL(FNetworkNotify::NotifyAcceptedConnection,);
	virtual UBOOL NotifyAcceptingChannel( class UChannel* Channel ) PURE_VIRTUAL(FNetworkNotify::NotifyAcceptingChannel,return FALSE;);
	virtual class UWorld* NotifyGetWorld() PURE_VIRTUAL(FNetworkNotify::NotifyGetWorld,return NULL;);
	/** handler for messages sent through a remote connection's control channel
	 * not required to handle the message, but if it reads any data from Bunch, it MUST read the ENTIRE data stream for that message (i.e. use FNetControlMessage<TYPE>::Receive())
	 */
	virtual void NotifyControlMessage(UNetConnection* Connection, BYTE MessageType, class FInBunch& Bunch) PURE_VIRTUAL(FNetworkNotify::NotifyReceivedText,);
	virtual UBOOL NotifySendingFile( UNetConnection* Connection, FGuid GUID ) PURE_VIRTUAL(FNetworkNotify::NotifySendingFile,return FALSE;);
	virtual void NotifyReceivedFile( UNetConnection* Connection, INT PackageIndex, const TCHAR* Error, UBOOL Skipped ) PURE_VIRTUAL(FNetworkNotify::NotifyReceivedFile,);
	virtual void NotifyProgress( EProgressMessageType MessageType, const FString& Title, const FString& Message ) PURE_VIRTUAL(FNetworkNotify::NotifyProgress,);

	// Notifications sent for peer connections
	
	/**
	 * Handler for control channel messages sent on a peer connection
	 *
	 * @param Connection net connection that received the message bunch
	 * @param MessageType type of the message bunch
	 * @param Bunch bunch containing the data for the message type read from the connection
	 */
	virtual void NotifyPeerControlMessage(UNetConnection* Connection, BYTE MessageType, class FInBunch& Bunch) PURE_VIRTUAL(FNetworkNotify::NotifyReceivedText,);
	/**
	 * Determine if peer connections are currently being accepted
	 *
	 * @return EAcceptConnection type based on if ready to accept a new peer connection
	 */
	virtual EAcceptConnection NotifyAcceptingPeerConnection() PURE_VIRTUAL(FNetworkNotify::NotifyAcceptingPeerConnection,return ACCEPTC_Ignore;);
	/**
	 * Notify that a new peer connection was created from the listening socket
	 *
	 * @param Connection net connection that was just created
	 */
	virtual void NotifyAcceptedPeerConnection( class UNetConnection* Connection ) PURE_VIRTUAL(FNetworkNotify::NotifyAcceptedPeerConnection,);	
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
	virtual FCheckResult* ActorLineCheck( FMemStack& Mem, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags, AActor *SourceActor, ULightComponent* SourceLight )=0;
	virtual FCheckResult* ActorPointCheck( FMemStack& Mem, const FVector& Location, const FVector& Extent, DWORD TraceFlags )=0;
	/**
	 * Finds all actors that are touched by a sphere (point + radius). If
	 * bUseOverlap is false, only the centers of the bounding boxes are
	 * considered. If true, it does a full sphere/box check.
	 *
	 * @param Mem the mem stack to allocate results from
	 * @param Location the center of the sphere
	 * @param Radius the size of the sphere to check for overlaps with
	 * @param bUseOverlap whether to use the full box or just the center
	 */
	virtual FCheckResult* ActorRadiusCheck( FMemStack& Mem, const FVector& Location, FLOAT Radius, UBOOL bUseOverlap = FALSE )=0;
	virtual FCheckResult* ActorEncroachmentCheck( FMemStack& Mem, AActor* Actor, FVector Location, FRotator Rotation, DWORD TraceFlags )=0;

	/**
	 * Finds all actors that are touched by a sphere (point + radius).
	 *
	 * @param	Mem			The mem stack to allocate results from.
	 * @param	Actor		The actor to ignore overlaps with.
	 * @param	Location	The center of the sphere.
	 * @param	Radius		The size of the sphere to check for overlaps with.
	 */
	virtual FCheckResult* ActorOverlapCheck(FMemStack& Mem, AActor* Actor, const FVector& Location, FLOAT Radius)=0;

	/**
	 * Finds all actors that are touched by a sphere (point + radius).
	 *
	 * @param	Mem			The mem stack to allocate results from.
	 * @param	Actor		The actor to ignore overlaps with.
	 * @param	Location	The center of the sphere.
	 * @param	Radius		The size of the sphere to check for overlaps with.
	 * @param	TraceFlags	Options for the trace.
	 */
	virtual FCheckResult* ActorOverlapCheck(FMemStack& Mem, AActor* Actor, const FVector& Location, FLOAT Radius, DWORD TraceFlags)=0;

	/**
	 * Finds all pawns that are touched by a sphere (point + radius).
	 *
	 * @param	Mem			The mem stack to allocate results from.
	 * @param	Actor		The actor to ignore overlaps with.
	 * @param	Location	The center of the sphere.
	 * @param	Radius		The size of the sphere to check for overlaps with.
	 */
	virtual FCheckResult* PawnOverlapCheck(FMemStack& Mem, AActor* Actor, const FVector& Location, FLOAT Radius)=0;

	/**
	 * Finds all actors that are touched by a sphere (point + radius) and pass Actor->WantsOverlapCheckWith().
	 *
	 * @param	Mem			The mem stack to allocate results from.
	 * @param	Actor		The actor to ignore overlaps with.
	 * @param	Location	The center of the sphere.
	 * @param	Radius		The size of the sphere to check for overlaps with.
	 */
	virtual FCheckResult* RestrictedOverlapCheck(FMemStack& Mem, AActor* Actor, const FVector& Location, FLOAT Radius)=0;

	virtual void GetIntersectingPrimitives(const FBox& Box,TArray<UPrimitiveComponent*>& Primitives) = 0;
	/**
	 * Retrieves all primitives in hash.
	 *
	 * @param	Primitives [out]	Array primitives are being added to
	 */
	virtual void GetPrimitives(TArray<UPrimitiveComponent*>& Primitives) const = 0;

	/** Try and remove empty/nearly empty nodes. */
	virtual void CollapseTreeChildren() = 0;

	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar) = 0;
};

//
//	ULineBatchComponent
//
class ULineBatchComponent : public UPrimitiveComponent, public FPrimitiveDrawInterface
{
	DECLARE_CLASS_NOEXPORT(ULineBatchComponent,UPrimitiveComponent,0,Engine);

	struct FLine
	{
		FVector			Start,
						End;
		FLinearColor	Color;
		FLOAT			Thickness;
		FLOAT			RemainingLifeTime;
		BYTE			DepthPriority;

		FLine(const FVector& InStart,const FVector& InEnd,const FLinearColor& InColor,FLOAT InLifeTime,FLOAT InThickness,BYTE InDepthPriority):
			Start(InStart),
			End(InEnd),
			Color(InColor),
			Thickness(InThickness),
			RemainingLifeTime(InLifeTime),
			DepthPriority(InDepthPriority)
		{}
	};

	struct FPoint
	{
		FVector Position;
		FLinearColor Color;
		FLOAT PointSize;
		BYTE DepthPriority;

		FPoint(const FVector& InPosition, const FLinearColor& InColor, const FLOAT InPointSize, const BYTE InDepthPriority) :
		Position(InPosition), Color(InColor), PointSize(InPointSize), DepthPriority(InDepthPriority)
		{}
	};

	TArray<FLine>	BatchedLines;
	TArray<FPoint>	BatchedPoints;
	FLOAT			DefaultLifeTime;

	/** Default constructor. */
	ULineBatchComponent():
		FPrimitiveDrawInterface(NULL),
		DefaultLifeTime(0.f)
	{}

	/** Provide many lines to draw - faster than calling DrawLine many times. */
	void DrawLines(const TArray<FLine>& InLines);

	/** Draw a box */
	void DrawBox(const FBox& Box, const FMatrix& TM, const FColor& Color, BYTE DepthPriorityGroup);

	// FPrimitiveDrawInterface
	virtual UBOOL IsHitTesting() { return FALSE; }
	virtual void SetHitProxy(HHitProxy* HitProxy) {}
	virtual void RegisterDynamicResource(FDynamicPrimitiveResource* DynamicResource) {}
	virtual INT DrawMesh(const FMeshBatch& Mesh) { return 0; }
	// FPrimitiveDrawInterface.
	virtual void DrawSprite(
		const FVector& Position,
		FLOAT SizeX,
		FLOAT SizeY,
		const FTexture* Sprite,
		const FLinearColor& Color,
		BYTE DepthPriority,
		FLOAT U,
		FLOAT UL,
		FLOAT V,
		FLOAT VL,
		BYTE BlendMode = 1 /*SE_BLEND_Masked*/
		) {}

	virtual void DrawLine(
		const FVector& Start,
		const FVector& End,
		const FLinearColor& Color,
		BYTE DepthPriority,
		const FLOAT Thickness = 0.0f
		);

	virtual void DrawPoint(
		const FVector& Position,
		const FLinearColor& Color,
		FLOAT PointSize,
		BYTE DepthPriority
		);

	/** Allocates a vertex for dynamic mesh rendering. */
	virtual INT AddVertex(
		const FVector4& InPosition,
		const FVector2D& InTextureCoordinate,
		const FLinearColor& InColor,
		BYTE InDepthPriorityGroup
		) { return INDEX_NONE; }

	/** Draws dynamic mesh triangle using vertices specified by AddVertex. */
	virtual void DrawTriangle(
		INT V0,
		INT V1,
		INT V2,
		const FMaterialRenderProxy* Material,
		BYTE InDepthPriorityGroup
		) {}

	// UPrimitiveComponent interface.

	/**
	 * Creates a new scene proxy for the line batcher component.
	 * @return	Pointer to the FLineBatcherSceneProxy
	 */
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual void Tick(FLOAT DeltaTime);
};

//
// MoveActor options.
//
enum EMoveFlags
{
	// Bitflags.
	MOVE_IgnoreBases		= 0x00001, // ignore collisions with things the Actor is based on
	MOVE_NoFail				= 0x00002, // ignore conditions that would normally cause MoveActor() to abort (such as encroachment)
	MOVE_TraceHitMaterial	= 0x00004, // figure out material was hit for any collisions
	MOVE_SingleBlocking		= 0x00008, // stop at the first blocking result when moving (useful for fast-moving weapon projectiles)
};

//
// Trace options.
//
enum ETraceFlags
{
	// Bitflags.
	TRACE_Pawns					= 0x00001, // Check collision with pawns.
	TRACE_Movers				= 0x00002, // Check collision with movers.
	TRACE_Level					= 0x00004, // Check collision with BSP level geometry.
	TRACE_Volumes				= 0x00008, // Check collision with soft volume boundaries.
	TRACE_Others				= 0x00010, // Check collision with all other kinds of actors.
	TRACE_OnlyProjActor			= 0x00020, // Check collision with other actors only if they are projectile targets
	TRACE_Blocking				= 0x00040, // Check collision with other actors only if they block the check actor
	TRACE_LevelGeometry			= 0x00080, // Check collision with other actors which are static level geometry
	TRACE_ShadowCast			= 0x00100, // Check collision with shadow casting actors
	TRACE_StopAtAnyHit			= 0x00200, // Stop when find any collision (for visibility checks)
	TRACE_SingleResult			= 0x00400, // Stop when find guaranteed first nearest collision (for SingleLineCheck)
	TRACE_Material				= 0x00800, // Request that Hit.Material return the material the trace hit.
	TRACE_Visible				= 0x01000,
	TRACE_Terrain				= 0x02000, // Check collision with terrain
	TRACE_Tesselation			= 0x04000, // Check collision against highest tessellation level (not valid for box checks)  (no longer used)
	TRACE_PhysicsVolumes		= 0x08000, // Check collision with physics volumes
	TRACE_TerrainIgnoreHoles	= 0x10000, // Ignore terrain holes when checking collision
	TRACE_ComplexCollision		= 0x20000, // Ignore simple collision on static meshes and always do per poly
	TRACE_AllComponents			= 0x40000, // Don't discard collision results of actors that have already been tagged.  Currently adhered to only by ActorOverlapCheck.
	TRACE_Accurate				= 0x80000, // Don't do the legacy pullback by an arbitrary amount of collision results
	TRACE_MoveIgnoresDestruction= 0x100000, // Skip collision with dynamic rigid bodies

	// Combinations.
	TRACE_Hash					= TRACE_Pawns	|	TRACE_Movers |	TRACE_Volumes	|	TRACE_Others			|	TRACE_Terrain	|	TRACE_LevelGeometry,
	TRACE_Actors				= TRACE_Pawns	|	TRACE_Movers |	TRACE_Others	|	TRACE_LevelGeometry		|	TRACE_Terrain,
	TRACE_World					= TRACE_Level	|	TRACE_Movers |	TRACE_Terrain	|	TRACE_LevelGeometry,
	TRACE_AllColliding			= TRACE_Level	|	TRACE_Actors |	TRACE_Volumes,
	TRACE_ProjTargets			= TRACE_AllColliding	| TRACE_OnlyProjActor,
	TRACE_AllBlocking			= TRACE_Blocking		| TRACE_AllColliding,
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

/*-----------------------------------------------------------------------------
	ULevel base.
-----------------------------------------------------------------------------*/

//
// A game level.
//
class ULevelBase : public UObject
{
	DECLARE_ABSTRACT_CLASS_INTRINSIC(ULevelBase,UObject,0,Engine)

	/** Array of all actors in this level, used by FActorIteratorBase and derived classes */
	TTransArray<AActor*> Actors;
	/** URL associated with this level. */
	FURL					URL;

	// Constructors.
	ULevelBase( const FURL& InURL );

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	void Serialize( FArchive& Ar );

protected:
	ULevelBase()
	:	Actors( this )
	{}
};

/*-----------------------------------------------------------------------------
	ULevel class.
-----------------------------------------------------------------------------*/

/** 
 *	Used for storing cached data for simplified static mesh collision at a particular scaling.
 *	One entry, relating to a particular static mesh cached at a particular scaling, giving an index into the CachedPhysSMDataStore. 
 */
struct FCachedPhysSMData
{
	/** Scale of mesh that the data was cached for. */
	FVector				Scale3D;

	/** Index into CachedPhysSMDataStore that this cached data is stored at. */
	INT					CachedDataIndex;

	/** Serializer function. */
	friend FArchive& operator<<( FArchive& Ar, FCachedPhysSMData& D )
	{
		Ar << D.Scale3D << D.CachedDataIndex;
		return Ar;
	}
};

/** 
 *	Used for storing cached data for per-tri static mesh collision at a particular scaling. 
 */
struct FCachedPerTriPhysSMData
{
	/** Scale of mesh that the data was cached for. */
	FVector			Scale3D;

	/** Index into array Cached data for this mesh at this scale. */
	INT				CachedDataIndex;

	/** Serializer function. */
	friend FArchive& operator<<( FArchive& Ar, FCachedPerTriPhysSMData& D )
	{
		Ar << D.Scale3D << D.CachedDataIndex;
		return Ar;
	}
};

/** A precomputed visibility cell, whose data is stored in FCompressedVisibilityChunk. */
class FPrecomputedVisibilityCell
{
public:

	/** World space min of the cell. */
	FVector Min;

	/** Index into FPrecomputedVisibilityBucket::CellDataChunks of this cell's data. */
	WORD ChunkIndex;

	/** Index into the decompressed chunk data of this cell's visibility data. */
	WORD DataOffset;

	friend FArchive& operator<<( FArchive& Ar, FPrecomputedVisibilityCell& D )
	{
		Ar << D.Min << D.ChunkIndex << D.DataOffset;
		return Ar;
	}
};

/** A chunk of compressed visibility data from multiple FPrecomputedVisibilityCell's. */
class FCompressedVisibilityChunk
{
public:
	/** Whether the chunk is compressed. */
	UBOOL bCompressed;

	/** Size of the uncompressed chunk. */
	INT UncompressedSize;

	/** Compressed visibility data if bCompressed is TRUE. */
	TArray<BYTE> Data;

	friend FArchive& operator<<( FArchive& Ar, FCompressedVisibilityChunk& D )
	{
		Ar << D.bCompressed << D.UncompressedSize << D.Data;
		return Ar;
	}
};

/** A bucket of visibility cells that have the same spatial hash. */
class FPrecomputedVisibilityBucket
{
public:
	/** Size in bytes of the data of each cell. */
	INT CellDataSize;

	/** Cells in this bucket. */
	TArray<FPrecomputedVisibilityCell> Cells;

	/** Data chunks corresponding to Cells. */
	TArray<FCompressedVisibilityChunk> CellDataChunks;

	friend FArchive& operator<<( FArchive& Ar, FPrecomputedVisibilityBucket& D )
	{
		Ar << D.CellDataSize << D.Cells << D.CellDataChunks;
		return Ar;
	}
};

/** Handles operations on precomputed visibility data for a level. */
class FPrecomputedVisibilityHandler
{
public:

	FPrecomputedVisibilityHandler() :
		Id(NextId)
	{
		NextId++;
	}
	
	~FPrecomputedVisibilityHandler() 
	{ 
		UpdateVisibilityStats(FALSE);
	}

	/** Updates visibility stats. */
	void UpdateVisibilityStats(UBOOL bAllocating) const;

	/** Sets this visibility handler to be actively used by the rendering scene. */
	void UpdateScene(FSceneInterface* Scene) const;

	/** Invalidates the level's precomputed visibility and frees any memory used by the handler. */
	void Invalidate(FSceneInterface* Scene);

	INT GetId() const { return Id; }

	friend FArchive& operator<<( FArchive& Ar, FPrecomputedVisibilityHandler& D );
	
private:

	/** World space origin of the cell grid. */
	FVector2D PrecomputedVisibilityCellBucketOriginXY;

	/** World space size of every cell in x and y. */
	FLOAT PrecomputedVisibilityCellSizeXY;

	/** World space height of every cell. */
	FLOAT PrecomputedVisibilityCellSizeZ;

	/** Number of cells in each bucket in x and y. */
	INT	PrecomputedVisibilityCellBucketSizeXY;

	/** Number of buckets in x and y. */
	INT	PrecomputedVisibilityNumCellBuckets;

	static INT NextId;

	/** Id used by the renderer to know when cached visibility data is valid. */
	INT Id;

	/** Visibility bucket data. */
	TArray<FPrecomputedVisibilityBucket> PrecomputedVisibilityCellBuckets;

	friend class FLightmassProcessor;
	friend class FSceneViewState;
};

/** Volume distance field generated by Lightmass, used by image based reflections for shadowing. */
class FPrecomputedVolumeDistanceField
{
public:

	/** Sets this volume distance field to be actively used by the rendering scene. */
	void UpdateScene(FSceneInterface* Scene) const;

	/** Invalidates the level's volume distance field and frees any memory used by it. */
	void Invalidate(FSceneInterface* Scene);

	friend FArchive& operator<<( FArchive& Ar, FPrecomputedVolumeDistanceField& D );

private:
	/** Largest world space distance stored in the volume. */
	FLOAT VolumeMaxDistance;
	/** World space bounding box of the volume. */
	FBox VolumeBox;
	/** Volume dimensions. */
	INT VolumeSizeX;
	INT VolumeSizeY;
	INT VolumeSizeZ;
	/** Distance field data. */
	TArray<FColor> Data;

	friend class FScene;
	friend class FLightmassProcessor;
};

struct FGuidPair
{
public:
	FGuid	Guid;
	DWORD	RefId;

	friend FArchive& operator<<( FArchive& Ar, FGuidPair& GP )
	{
		Ar << GP.Guid << GP.RefId;
		return Ar;
	}
};

struct FCoverIndexPair
{
public:
	DWORD	ActorRefItem;
	BYTE	SlotIdx;

	UBOOL IsEqual( class ULevel* Level, ACoverLink* TestLink, INT TestSlotIdx );

	friend FArchive& operator<<( FArchive& Ar, FCoverIndexPair& IP )
	{
		Ar << IP.ActorRefItem << IP.SlotIdx;
		return Ar;
	}
};

//
// The level object.  Contains the level's actor list, Bsp information, and brush list.
//
class ULevel : public ULevelBase
{
	DECLARE_CLASS_INTRINSIC(ULevel,ULevelBase,0,Engine)
	NO_DEFAULT_CONSTRUCTOR(ULevel)

	/** BSP UModel.																									*/
	UModel*										Model;
	/** BSP Model components used for rendering.																	*/
	TArray<UModelComponent*>					ModelComponents;
	/** The level's Kismet sequences */
	class TArray<USequence*>					GameSequences;

	/** Static information used by texture streaming code, generated during PreSave									*/
	TMap<UTexture2D*,TArray<FStreamableTextureInstance> >	TextureToInstancesMap;

	/** Information about textures on dynamic primitives. Used by texture streaming code, generated during PreSave.		*/
	TMap<UPrimitiveComponent*,TArray<FDynamicTextureInstance> >	DynamicTextureInstances;

	/** Set of textures used by PrimitiveComponents that have bForceMipStreaming checked. */
	TMap<UTexture2D*,UBOOL>									ForceStreamTextures;

	/** Index into Actors array pointing to first net relevant actor. Used as an optimization for FActorIterator	*/
	INT											iFirstNetRelevantActor;
	/** Index into Actors array pointing to first dynamic actor. Used as an optimization for FActorIterator			*/
	INT											iFirstDynamicActor;

	/** Total number of KB used for lightmap textures in the level. */
	FLOAT										LightmapTotalSize;
	/** Total number of KB used for shadowmap textures in the level. */
	FLOAT										ShadowmapTotalSize;

#if WITH_NOVODEX
	/** Physics scene index. */
	INT											SceneIndex;

	/** Novodex shape for the level BSP. */
	class NxTriangleMesh*						LevelBSPPhysMesh;
   
   	/** Novodex representation of the level BSP geometry (triangle mesh). */
	class NxActor*								LevelBSPActor;

	/** Novodex representation of the level BSP geometry (convex hulls). */
	class NxActor*								LevelConvexBSPActor;
#endif
	
	/** Cached BSP triangle-mesh data for use with the physics engine. */
	TArray<BYTE>								CachedPhysBSPData;

	/** 
	 *	Indicates version that CachedPhysBSPData was created at. 
	 *	Compared against CurrentCachedPhysDataVersion.
	 */
	INT											CachedPhysBSPDataVersion;

	/** 
	 *	Mapping between a particular static mesh used in the level and indices of pre-cooked physics data for it at various scales. 
	 *	Index in FCachedPhysSMData refers to object in the CachedPhysSMDataStore.
	 */
	TMultiMap<UStaticMesh*, FCachedPhysSMData>	CachedPhysSMDataMap;

	/** 
	 *	Indicates version that CachedPhysSMDataMap was created at. 
	 *	Compared against CurrentCachedPhysDataVersion.
	 */
	INT											CachedPhysSMDataVersion;

	/** 
	 *	Store of processsed physics data for static meshes at different scales. 
	 *	Use CachedPhysSMDataMap to find index into this array.
	 */
	TArray<FKCachedConvexData>					CachedPhysSMDataStore;

	/** 
	 *	Store of physics cooked mesh data for per-triangle static mesh collision.
	 *	Index in FCachedPerTriPhysSMData refers to object in the CachedPhysPerTriSMDataStore.
	 */
	TMultiMap<UStaticMesh*, FCachedPerTriPhysSMData>	CachedPhysPerTriSMDataMap;

	/** 
	 *	Store of processsed physics data for static meshes per-tri collision at different scales. 
	 *	Use CachedPhysPerTriSMDataMap to find index into this array.
	 */
	TArray<FKCachedPerTriData>					CachedPhysPerTriSMDataStore;

	/** Simplified collision data for the mesh (convex hulls). */
	FKAggregateGeom								ConvexBSPAggGeom;

	/** Cached BSP convex hulls data for use with the physics engine. */
	FKCachedConvexData							CachedPhysConvexBSPData;

	/** 
	 *	Indicates version that CachedPhysConvexBSPData was created at. 
	 *	Compared against CurrentCachedPhysConvexBSPVersion.
	 */
	INT											CachedPhysConvexBSPVersion;

	/**
	 * Start and end of the navigation list for this level, used for quickly fixing up
	 * when streaming this level in/out.
	 */
	class ANavigationPoint						*NavListStart, *NavListEnd;
	class ACoverLink							*CoverListStart, *CoverListEnd;
	class APylon								*PylonListStart, *PylonListEnd;

	/** Stores guids and refid into CoverLinkRefs array for all unresolved crosslevel cover links */
	TArray<FGuidPair>			CrossLevelCoverGuidRefs;
	/** Stores a ptr to all coverlinks referenced by some other object (ie a firelink struct) */
	TArray<class ACoverLink*>	CoverLinkRefs;
	/** Stores slotindex and reference into CoverLinkRefs array for a coverlink */
	TArray<FCoverIndexPair>		CoverIndexPairs;

	/** array of Actors in this level that currently can be and want to be ticked */
	TArray<AActor*> TickableActors;
	/** array of Actors that need to be removed from the TickableActors array at the end of the frame */
	TArray<AActor*> PendingUntickableActors;
	/**
	 * List of actors that have cross level actor references that need to be fixed up when
	 * streaming a level in/out.
	 */
	TArray<class AActor*>						CrossLevelActors;

	/** 
	* The precomputed light information for this level.  
	* The extra level of indirection is to allow forward declaring FPrecomputedLightVolume.
	*/
	class FPrecomputedLightVolume*				PrecomputedLightVolume;

	/** Contains precomputed visibility data for this level. */
	FPrecomputedVisibilityHandler				PrecomputedVisibilityHandler;

	/** Precomputed volume distance field for this level. */
	FPrecomputedVolumeDistanceField				PrecomputedVolumeDistanceField;

	/** Fence used to track when the rendering thread has finished referencing this ULevel's resources. */
	FRenderCommandFence							RemoveFromSceneFence;

	/** transform to level contents applied by the level streaming system (@see UWorld::AddToWorld()) */
	FMatrix AppliedLevelTransform;

	/** Whether components are currently attached or not. */
	UBOOL										bAreComponentsCurrentlyAttached;

	/** Whether the geometry needs to be rebuilt for correct lighting */
	UBOOL										bGeometryDirtyForLighting;

	/** The below variables are used temporarily while making a level visible.				*/

	/** Whether the level is currently pending being made visible.							*/
	UBOOL										bHasVisibilityRequestPending;
	/** Whether we already moved actors.													*/
	UBOOL										bAlreadyMovedActors;
	/** Whether we already updated components.												*/
	UBOOL										bAlreadyUpdatedComponents;
	/** Whether we already associated streamable resources.									*/
	UBOOL										bAlreadyAssociatedStreamableResources;
	/** Whether we already initialized actors.												*/
	UBOOL										bAlreadyInitializedActors;
	/** Whether we already routed beginplay on actors.										*/
	UBOOL										bAlreadyRoutedActorBeginPlay;
	/** Whether we already fixed up cross-level paths.										*/
	UBOOL										bAlreadyFixedUpCrossLevelRefs;
	/** Whether we already routed sequence begin play.										*/
	UBOOL										bAlreadyRoutedSequenceBeginPlay;
	/** Whether we already sorted the actor list.											*/
	UBOOL										bAlreadySortedActorList;
	/** Current index into actors array for updating components.							*/
	INT											CurrentActorIndexForUpdateComponents;
	/** Whether we already created the physics engine version of the levelBSP.				*/
	UBOOL										bAlreadyCreateBSPPhysMesh;
	/** Whether we have already called InitActorRBPhys on all Actors in the level.			*/
	UBOOL										bAlreadyInitializedAllActorRBPhys;
	/** Current index into actors array for initializing Actor rigid-body physics.			*/
	INT											CurrentActorIndexForInitActorsRBPhys;
	
#if PERF_TRACK_DETAILED_ASYNC_STATS
	/** Mapping of how long each actor class takes to have UpdateComponents called on it */
	TMap<const UClass*,struct FMapTimeEntry>		UpdateComponentsTimePerActorClass;
#endif // PERF_TRACK_DETAILED_ASYNC_STATS

	// Constructor.
	ULevel( const FURL& InURL );

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	// UObject interface.
	void Serialize( FArchive& Ar );
	virtual void BeginDestroy();
	virtual UBOOL IsReadyForFinishDestroy();
	virtual void FinishDestroy();

	/** Called before an Undo action occurs */
	virtual void PreEditUndo();

	/** Called after an Undo action occurs */
	virtual void PostEditUndo();

	/**
	 * Callback used to allow object register its direct object references that are not already covered by
	 * the token stream.
	 *
	 * @param ObjectArray	array to add referenced objects to via AddReferencedObject
	 */
	void AddReferencedObjects( TArray<UObject*>& ObjectArray );

	/**
	 * Removes existing line batch components from actors and associates streaming data with level.
	 */
	void PostLoad();

	/**
	 * Clears all components of actors associated with this level (aka in Actors array) and 
	 * also the BSP model components.
	 */
	void ClearComponents();

	/**
	 * Updates all components of actors associated with this level (aka in Actors array) and 
	 * creates the BSP model components.
	 */
	void UpdateComponents();

	/**
	 * Incrementally updates all components of actors associated with this level.
	 *
	 * @param NumComponentsToUpdate	Number of components to update in this run, 0 for all
	 */
	void IncrementalUpdateComponents( INT NumComponentsToUpdate );

	/**
	 * Invalidates the cached data used to render the level's UModel.
	 */
	void InvalidateModelGeometry();

	/**
	 * Updates the model components associated with this level
	 */
	void UpdateModelComponents();

	/**
	 * Commits changes made to the UModel's surfaces.
	 */
	void CommitModelSurfaces();

	/**
	 * Discards the cached data used to render the level's UModel.  Assumes that the
	 * faces and vertex positions haven't changed, only the applied materials.
	 */
	void InvalidateModelSurface();

	/**
	 * Makes sure that all light components have valid GUIDs associated.
	 */
	void ValidateLightGUIDs();

	/**
	 * Sorts the actor list by net relevancy and static behaviour. First all not net relevant static
	 * actors, then all net relevant static actors and then the rest. This is done to allow the dynamic
	 * and net relevant actor iterators to skip large amounts of actors.
	 */
	void SortActorList();

	/**
	 * Initializes all actors after loading completed.
	 *
	 * @param bForDynamicActorsOnly If TRUE, this function will only act on non static actors
	 */
	void InitializeActors(UBOOL bForDynamicActorsOnly=FALSE);

	/**
	 * Routes pre and post begin play to actors and also sets volumes.
	 *
	 * @param bForDynamicActorsOnly If TRUE, this function will only act on non static actors
	 */
	void RouteBeginPlay(UBOOL bForDynamicActorsOnly=FALSE);

	/**
	 * Presave function, gets called once before the level gets serialized (multiple times) for saving.
	 * Used to rebuild streaming data on save.
	 */
	void PreSave();	

	/**
	 * Rebuilds static streaming data for all levels in the specified UWorld.
	 *
	 * @param World				Which world to rebuild streaming data for. If NULL, all worlds will be processed.
	 * @param TargetLevel		[opt] Specifies a single level to process. If NULL, all levels will be processed.
	 * @param TargetTexture		[opt] Specifies a single texture to process. If NULL, all textures will be processed.
	 */
	static void BuildStreamingData(UWorld* World, ULevel* TargetLevel=NULL, UTexture2D* TargetTexture=NULL);

	/**
	 * Triggers a call to BuildStreamingData(GWorld,NULL,NULL) within a few seconds.
	 */
	static void TriggerStreamingDataRebuild();

	/**
	 * Calls BuildStreamingData(GWorld,NULL,NULL) if it has been triggered within the last few seconds.
	 */
	static void ConditionallyBuildStreamingData();

	/**
	 *	Retrieves the array of streamable texture isntances.
	 *
	 */
	TArray<FStreamableTextureInstance>* GetStreamableTextureInstances(UTexture2D*& TargetTexture);

	/**
	 * Returns the default brush for this level.
	 *
	 * @return		The default brush for this level.
	 */
	ABrush* GetBrush() const;

	/**
	 * Returns the world info for this level.
	 *
	 * @return		The AWorldInfo for this level.
	 */
	AWorldInfo* GetWorldInfo() const;

	/** 
	 * Associate teleporters with the portal volume they are in
	 */
	void AssociatePortals( void );

	/**
	 * Returns the sequence located at the index specified.
	 * @return	a pointer to the root USequence object for this level (may be NULL)
	 */
	USequence* GetGameSequence() const;

	/** Create physics engine representation of the level BSP. */
	void InitLevelBSPPhysMesh();

	/**
	 *	Iterates over all actors calling InitRBPhys on them.
	 */
	void IncrementalInitActorsRBPhys(INT NumActorsToInit);

	/**
	 *	Terminate physics for this level. Destroy level BSP physics, and iterates over all actors
	 *	calling TermRBPhys on them.
	 */
	void TermLevelRBPhys(FRBPhysScene* Scene);

	/** Reset stats used for seeing where time goes initialising physics. */
	void ResetInitRBPhysStats();

	/** Output stats for initialising physics. */
	void OutputInitRBPhysStats();

	/** Build cached BSP triangle-mesh data for physics engine. */
	void BuildPhysBSPData();

	/** Create the cache of cooked collision data for static meshes used in this level. */
	void BuildPhysStaticMeshCache();

	/**  Clear the static mesh cooked collision data cache. */
	void ClearPhysStaticMeshCache();

	/** 
	 *	Utility for finding if we have cached data for a paricular static mesh at a particular scale.
	 *	Returns NULL if there is no cached data.
	 */
	FKCachedConvexData* FindPhysStaticMeshCachedData(UStaticMesh* InMesh, const FVector& InScale3D);

	/** 
	 *	Utility for finding if we have cached per-triangle data for a paricular static mesh at a particular scale.
	 *	Returns NULL if there is no cached data.
	 */
	FKCachedPerTriData* FindPhysPerTriStaticMeshCachedData(UStaticMesh* InMesh, const FVector& InScale3D);

	/**
	 * Utility searches this level's actor list for any actors of the specified type.
	 */
	UBOOL HasAnyActorsOfType(UClass *SearchType);

	/**
	 * Returns TRUE if this level has any path nodes.
	 */
	UBOOL HasPathNodes();

	/**
	 * Adds a pathnode to the navigation list.
	 */
	void AddToNavList( class ANavigationPoint *Nav, UBOOL bDebugNavList = FALSE );

	/**
	 * Removes a pathnode from the navigation list.
	 */
	void RemoveFromNavList( class ANavigationPoint *Nav, UBOOL bDebugNavList = FALSE );

	/**
	 * Resets the level nav list.
	 */
	void ResetNavList();

	/** finds all Material references pointing to the specified material relevant to material parameter modifiers (Matinee tracks, etc)
	 * in this level and adds entries to the arrays in the structure
	 */
	void GetMaterialRefs(struct FMaterialReferenceList& ReferenceInfo, UBOOL bFindPostProcessRefsOnly = FALSE);

	/**
	 * Clears references to any coverlinks in CoverLinkRefs array and readds the guid to the CrossLevelCoverGuidRefs to be resolved again later if needed 
	 * @param LevelBeingRemoved - when non null, indicates which level is being removed that we should clear refs to (if null, clear all refs that go outside our level)
	 */
	void ClearCrossLevelCoverReferences( ULevel* LevelBeingRemoved = NULL );

	/**
	 * When adding this level to the world, fixup all the necessary FGuidPair entries that are relevant
	 * When removing this level, calls ClearCrossLevelCoverReferences
	 */
	void FixupCrossLevelCoverReferences( UBOOL bRemovingLevel, TMap<FGuid, AActor*>* GuidHash, ULevel* LevelBeingAddedOrRemoved );

	/**
	 * Emtpies all the crosslevelcoverref arrays
	 */
	void PurgeCrossLevelCoverArrays();

private:

	/**
	 * Recreates the array of tickable actors starting from the given actor index. 
	 *
	 * @param	StartIndex	The index to start in the level's master Actor list. This 
	 *						MUST be zero or greater. Otherwise, the function will crash. 
	 *						Default StartIndex is zero.
	 */
	void RebuildTickableActors( INT StartIndex = 0 );


	/** Whether we have a pending call to BuildStreamingData(). */
	static UBOOL bStreamingDataDirty;

	/** Timestamp (in appSeconds) when the next call to BuildStreamingData() should be made, if bDirtyStreamingData is TRUE. */
	static DOUBLE BuildStreamingDataTimer;
};
