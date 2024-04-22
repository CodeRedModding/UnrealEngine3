/*=============================================================================
	UnPath.h: Path node creation and ReachSpec creations and management specific classes
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __PATH_H
#define __PATH_H

#include "UnOctree.h"
#include "UnPathDebug.h"
#include "GenericOctree.h"

#define MAXPATHDIST			1200 // maximum distance for paths between two nodes
#define MAXPATHDISTSQ		MAXPATHDIST*MAXPATHDIST
#define	PATHPRUNING			1.2f // maximum relative length of indirect path to allow pruning of direct reachspec between two pathnodes
#define MINMOVETHRESHOLD	4.1f // minimum distance to consider an AI predicted move valid
#define SWIMCOSTMULTIPLIER	3.5f // cost multiplier for paths which require swimming
#define CROUCHCOSTMULTIPLIER 1.1f // cost multiplier for paths which require crouching
#define MAXTESTMOVESIZE		200.f // maximum size used for test moves

#define VEHICLEADJUSTDIST 400.f // used as threshold for adjusting trajectory of vehicles
#define TESTMINJUMPDIST  128.f	// assume any bJumpCapable pawn can jump this far in walk move test

// magic number distances used by AI/networking
#define CLOSEPROXIMITY		500.f
#define	PROXIMATESOUNDTHRESHOLD 50.f
#define MINSOUNDOVERLAPTIME	0.18f
#define MAXSOUNDOVERLAPTIME	0.2f
#define NEARSIGHTTHRESHOLD	2000.f
#define MEDSIGHTTHRESHOLD	3162.f
#define FARSIGHTTHRESHOLD	8000.f
#define	PROXIMATESOUNDTHRESHOLDSQUARED PROXIMATESOUNDTHRESHOLD*PROXIMATESOUNDTHRESHOLD
#define CLOSEPROXIMITYSQUARED CLOSEPROXIMITY*CLOSEPROXIMITY
#define NEARSIGHTTHRESHOLDSQUARED NEARSIGHTTHRESHOLD*NEARSIGHTTHRESHOLD
#define MEDSIGHTTHRESHOLDSQUARED MEDSIGHTTHRESHOLD*MEDSIGHTTHRESHOLD
#define FARSIGHTTHRESHOLDSQUARED FARSIGHTTHRESHOLD*FARSIGHTTHRESHOLD

//Reachability flags - using bits to save space
enum EReachSpecFlags
{
	R_WALK = 1,	//walking required
	R_FLY = 2,   //flying required 
	R_SWIM = 4,  //swimming required
	R_JUMP = 8,   // jumping required
	R_HIGHJUMP = 16	// higher jump required (not useable without special abilities)
}; 

// NavMesh generator types
enum ENavigationMeshGeneratorType
{
	NavMeshGenerator_Native = 0,
	NavMeshGenerator_Recast = 1,
};

#if !PS3 && !FINAL_RELEASE
#define MAX_LOOP_ITTS 2048
#define checkFatalPathFailure(expr,Message,failreturnval) \
{\
	if(!(expr))\
	{\
		if(GIsEditor && GIsPlayInEditorWorld)\
		{\
			\
			appMsgf(AMT_OK, Message);\
\
			UBOOL bCalledExec = FALSE;\
			for ( INT ViewportIndex = 0; ViewportIndex < GEngine->GamePlayers.Num(); ViewportIndex++ )\
			{\
				ULocalPlayer* Player = GEngine->GamePlayers(ViewportIndex);\
				if ( Player->Exec(TEXT("Exit"), *GLog) )\
				{\
					return failreturnval;\
				}\
				bCalledExec = TRUE;\
			}\
			if ( !bCalledExec )\
			{\
				GEngine->Exec(TEXT("Exit"));\
			}\
			GEngine->StaticExec(TEXT("EXIT"));\
		}\
		else\
		{\
			checkMsg(expr,Message);\
		}\
		return failreturnval;\
	}\
}
#endif


#define LOG_DETAILED_PATHFINDING_STATS 0

#if LOG_DETAILED_PATHFINDING_STATS
#define TRACK_DETAILED_PATHFINDING_STATS(Object)	FScopedDetailTickStats DetailedPathFindingStats(GDetailedPathFindingStats,Object);
extern struct FDetailedTickStats GDetailedPathFindingStats;
#else
#define TRACK_DETAILED_PATHFINDING_STATS(Object)
#endif

#define MAXSORTED 32
class FSortedPathList
{
public:
	ANavigationPoint *Path[MAXSORTED];
	INT Dist[MAXSORTED];
	int numPoints;

	FSortedPathList() { numPoints = 0; }
	ANavigationPoint* FindStartAnchor(APawn *Searcher); 
	ANavigationPoint* FindEndAnchor(APawn *Searcher, AActor *GoalActor, FVector EndLocation, UBOOL bAnyVisible, UBOOL bOnlyCheckVisible); 
	void AddPath(ANavigationPoint * node, INT dist);
};

class FPathBuilder
{
public:
	static AScout* GetScout();
	static void DestroyScout();
#if WITH_EDITOR
	static void Exec( const TCHAR* Str );
	typedef UBOOL (*MapCancelledFunc)();
	static UBOOL NavMeshWorld();
	static void BuildPaths(UBOOL bBuildReachSpecs, UBOOL bOnlyBuildSelected, MapCancelledFunc MapCancelled);
	static UBOOL IsBuildingPaths() { return bBuildingPaths; }
	static void AddBoxToPathBuildBounds( const FBox& Box );
	static UBOOL IsPtWithinPathBuildBounds(const FVector& Point );
#else
	static UBOOL IsBuildingPaths() { return FALSE; }
#endif
	static void SetPathingVersionNum(DWORD InVersionNum) { LoadedPathVersionNum = Max<INT>(LoadedPathVersionNum,InVersionNum); }
	static DWORD GetLoadedPathDataVersion() { return LoadedPathVersionNum; }

private:
	static AScout *Scout;
	static UBOOL bBuildingPaths;
	/** pathing data version number of the last loaded navmesh */
	static DWORD LoadedPathVersionNum;
	/** bounds within which we are building paths (used for partial builds to determine what needs to be rebuilt) */
	static FBox PathBuildBounds;
};

#define DO_PATHCONSTRAINT_PROFILING 0
#ifndef __PATHCONSTRAINTPROFILING
#define __PATHCONSTRAINTPROFILING
#if DO_PATHCONSTRAINT_PROFILING 

struct FConstraintProfileDatum
{
public:
	FConstraintProfileDatum(FName Name)
	{
		CallCount=0;
		MaxTime=-1.f;
		AvgTime=-1.f;
		TotalTime=0.f;
		ConstraintName=Name;
	}

	INT CallCount;
	FLOAT MaxTime;
	FLOAT AvgTime;
	FLOAT TotalTime;
	FName ConstraintName;
};

#define SCOPETIMER(CALLCOUNT,CALLAVG,CALLMAX,CALLTOTAL,ID) \
class ScopeTimer##ID \
{ \
public:\
	FLOAT Time; \
	ScopeTimer##ID() \
{\
	CALLCOUNT ## ++;\
	Time=0.f;\
	CLOCK_CYCLES(Time);\
}\
	~ScopeTimer##ID()\
{\
	UNCLOCK_CYCLES(Time);\
	CALLTOTAL+=Time;\
	if(CALLAVG < 0.f)\
{\
	CALLAVG = Time;\
}\
		else\
{\
	CALLAVG += (Time - CALLAVG)/CALLCOUNT;\
}\
	if(Time > CALLMAX)\
{\
	CALLMAX = Time;\
}\
}\
};\
	ScopeTimer##ID TheTimer##ID = ScopeTimer##ID();

#else
#define SCOPETIMER(CALLCOUNT,CALLAVG,CALLMAX,CALLTOTAL,ID) {}
#endif

#if 0
#define SCOPE_QUICK_TIMER_NAVHANDLECPP(TIMERNAME) \
	static DOUBLE AverageTime##TIMERNAME = -1.0; \
	static INT NumItts##TIMERNAME = 0; \
class TimerClassName##TIMERNAME \
{ \
public:\
	TimerClassName##TIMERNAME() \
{ \
	StartTimerTime = appSeconds(); \
} \
	\
	~TimerClassName##TIMERNAME() \
{\
	FLOAT Duration = appSeconds() - StartTimerTime;\
	if(AverageTime##TIMERNAME < 0.f)\
{\
	AverageTime##TIMERNAME = Duration;\
}\
			else\
{\
	AverageTime##TIMERNAME += (Duration - AverageTime##TIMERNAME)/++NumItts##TIMERNAME;\
}\
	\
	debugf(TEXT("Task %s took %.2f(ms) Avg %.2f(ms) -- %d mins %d secs"),TEXT(#TIMERNAME),Duration*1000.f,AverageTime##TIMERNAME*1000.f, appFloor(Duration/60.f), appFloor(Duration)%60);\
}\
	DOUBLE StartTimerTime;\
};\
	TimerClassName##TIMERNAME TIMERNAME = TimerClassName##TIMERNAME();
#else
#define SCOPE_QUICK_TIMER_NAVHANDLECPP(blah) {}
#endif
#endif

/** specialized faster versions of FNavigationOctreeObject Owner accessor for common types in ENavOctreeObjectType enum */
template<> FORCEINLINE UObject* FNavigationOctreeObject::GetOwner<UObject>()
{
	return Owner;
}
template<> FORCEINLINE ANavigationPoint* FNavigationOctreeObject::GetOwner<ANavigationPoint>()
{
	return (OwnerType & NAV_NavigationPoint) ? (ANavigationPoint*)Owner : NULL;
}
template<> FORCEINLINE UReachSpec* FNavigationOctreeObject::GetOwner<UReachSpec>()
{
	return (OwnerType & NAV_ReachSpec) ? (UReachSpec*)Owner : NULL;
}

/** a single node in the navigation octree */
class FNavigationOctreeNode : public FOctreeNodeBase
{
private:
	/** children of this node, either NULL or 8 elements */
	FNavigationOctreeNode* Children;

	/** objects in this node */
	TArray<struct FNavigationOctreeObject*> Objects;

public:
	/** constructor */
	FNavigationOctreeNode()
		: Children(NULL)
	{}
	/** destructor, clears out Objects array and deletes Children if they exist */
	~FNavigationOctreeNode()
	{
		// clear out node pointer in all of the objects
		for (INT i = 0; i < Objects.Num(); i++)
		{
			Objects(i)->OctreeNode = NULL;
		}
		Objects.Empty();
		// delete children
		if (Children != NULL)
		{
			delete [] Children;
			Children = NULL;
		}
	}

	/** filters an object with the given bounding box through this node, either adding it or passing it to its children
	 * if the object is added to this node, the node may also be split if it exceeds the maximum number of objects allowed for a node without children
	 * assumes the bounding box fits in this node and always succeeds
	 * @param Object the object to filter
	 * @param NodeBounds the bounding box for this node
	 */
	void FilterObject(struct FNavigationOctreeObject* Object, const FOctreeNodeBounds& NodeBounds);

	/** searches for an entry for the given object and removes it if found
	 * @param Object the object to remove
	 * @return true if the object was found and removed, false if it wasn't found
	 */
	UBOOL RemoveObject(struct FNavigationOctreeObject* Object)
	{
		return (Objects.RemoveItem(Object) > 0);
	}

	/** returns all objects in this node and all children whose bounding box intersects with the given sphere
	 * @param Point the center of the sphere
	 * @param RadiusSquared squared radius of the sphere
	 * @param Extent bounding box for the sphere
	 * @param OutObjects (out) all objects found in the radius
	 * @param NodeBounds the bounding box for this node
	 */
	void RadiusCheck(const FVector& Point, FLOAT RadiusSquared, const FBox& Extent, TArray<FNavigationOctreeObject*>& OutObjects, const FOctreeNodeBounds& NodeBounds);

	/** checks the given box against the objects in this node and returns all objects found that intersect with it
	 * recurses down to children that intersect the box
	 * @param Box the box to check
	 * @param NodeBounds the bounding box for this node
	 * @param OutObjects (out) all objects found that intersect
	 */
	void OverlapCheck(const FBox& Box, TArray<FNavigationOctreeObject*>& OutObjects, const FOctreeNodeBounds& NodeBounds);

	/** counts the number of nodes and objects there are in the octree
	 * @param NumNodes (out) incremented by the number of nodes (this one plus all children)
	 * @param NumObjects (out) incremented by the total number of objects in this node and its child nodes
	 */
	void CollectStats(INT& NumNodes, INT& NumObjects);

	UBOOL FindObject( UObject* Owner, UBOOL bRecurseChildren );
};

/**
 * Octree containing NavigationPoint/ReachSpec bounding boxes for quickly determining things like whether a given location is on the path network 
 *
 * We deliberatly don't serialize its object references as the octree won't be the only reference and the AddReferencedObject interface doesn't
 * support NULLing out references anyways.
 */
class FNavigationOctree 
{
private:
	/** the root node encompassing the entire world */
	class FNavigationOctreeNode* RootNode;

	/** the bounds of the root node; should be the size of the world */
	static const FOctreeNodeBounds RootNodeBounds;

public:
	/** constructor, creates the root node */
	FNavigationOctree()
	{
		RootNode = new FNavigationOctreeNode;
	}
	/** destructor, deletes the root node */
	~FNavigationOctree()
	{
		delete RootNode;
		RootNode = NULL;
	}

	/** adds an object with the given bounding box
	 * @param Object the object to add
	 * @note this method assumes the object is not already in the octree
	 */
	void AddObject(struct FNavigationOctreeObject* Object);
	/** removes the given object from the octree
	 * @param Object the object to remove
	 * @return true if the object was in the octree and was removed, false if it wasn't in the octree
	 */
	UBOOL RemoveObject(struct FNavigationOctreeObject* Object);

	/** returns all objects in the octree whose bounding box intersects with the given sphere
	 * @param Point the center of the sphere
	 * @param Radius radius of the sphere
	 * @param OutObjects (out) all objects found in the radius
	 */
	void RadiusCheck(const FVector& Point, FLOAT Radius, TArray<FNavigationOctreeObject*>& OutObjects)
	{
		FVector Extent(Radius, Radius, Radius);
		RootNode->RadiusCheck(Point, Radius * Radius, FBox(Point - Extent, Point + Extent), OutObjects, RootNodeBounds);
	}

	/** checks the given point with the given extent against the octree and returns all objects found that intersect with it
	 * @param Point the origin for the point check
	 * @param Extent extent of the box for the point check
	 * @param OutObjects (out) all objects found that intersect
	 */
	void PointCheck(const FVector& Point, const FVector& Extent, TArray<FNavigationOctreeObject*>& OutObjects)
	{
		RootNode->OverlapCheck(FBox(Point - Extent, Point + Extent), OutObjects, RootNodeBounds);
	}

	/** checks the given box against the octree and returns all objects found that intersect with it
	 * @param Box the box to check
	 * @param OutObjects (out) all objects found that intersect
	 */
	void OverlapCheck(const FBox& Box, TArray<FNavigationOctreeObject*>& OutObjects)
	{
		RootNode->OverlapCheck(Box, OutObjects, RootNodeBounds);
	}

	/** console command handler for implementing debugging commands */
	UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar);

	/** 
	 * Removes all objects from octree.
	 */
	void RemoveAllObjects();
};

///////////////////////////////////
///////// NAVIGATION MESH /////////
///////////////////////////////////

#define WORLD_SPACE TRUE
#define LOCAL_SPACE FALSE

// will hold edge deletes while this is in scope
#define SCOPED_EDGE_DELETE_HOLD(ID) \
class ScopeHold##ID \
{ \
public:\
	ScopeHold##ID() \
{\
	FNavMeshWorld::HoldEdgeDeletes();\
}\
	~ScopeHold##ID()\
{\
	FNavMeshWorld::RemoveEdgeDeleteHold();\
}\
};\
	ScopeHold##ID TheHold##ID = ScopeHold##ID();

/** Typedef for nav mesh pylon octree */
typedef TOctree<class APylon*, struct FPylonOctreeSemantics> FPylonOctreeType;

typedef TMultiMap< class IInterface_NavMeshPathObstacle*, FPolyReference> ObstacleToPolyMapType;
typedef TMap<FNavMeshPolyBase*, IInterface_NavMeshPathObject*> PolyToPOMap;
typedef TLookupMap< class UNavigationHandle* > ActiveHandleList;
typedef TMap<FNavMeshEdgeBase*, UBOOL> EdgeDeletionQueue;
// FnavMeshWorld is a wrapper which
// holds global data relavent to the navigation mesh
class FNavMeshWorld
{
public:
	FNavMeshWorld() : PylonOctree(NULL),EdgeDeletionHoldStackDepth(0) {}

	/** octree for nav mesh pylons   */
	FPylonOctreeType*							PylonOctree;

	/** Map of active mesh obstacles to the polys they are linked to currently */
	ObstacleToPolyMapType ActiveObstacles;

	/** list of active handles in the world -- useful for things like clearing refs to meshes being deleted */
	ActiveHandleList ActiveHandles;

	/** stack depth for holds that have been placed on outgoing edge deletion notifications */
	INT EdgeDeletionHoldStackDepth;

	/** list of edges pending deletion, and pending deletion notification to handles referencing them */
	EdgeDeletionQueue EdgesPendingDeletion;
	
	/** 
	 * if we have no edge deletion holds at the moment, trigger the edge to be deleted.. otherwise
	 * queue it for deletion
	 * @param Edge - the edge to be deleted/queued for deletion
	 * @param bJustNotify - just notify of delete, don't actually delete the edge (for edges which are in buffers and not on the heap)
	 */
	static void DestroyEdge(FNavMeshEdgeBase* Edge, UBOOL bJustNotify);

	/**
	 * puts a hold on deletions and deletion notifications, they will instead be queued and flushed once the hold is removed
	 */
	static void HoldEdgeDeletes();

	/**
	 * removes a hold from edge deletes, if depth reaches 0 will then flush the queue
	 */
	static void RemoveEdgeDeleteHold();

private:
	/**
	 * will run through the queue of edges pending delete and notify handles they're being deleted, and then
	 * perform the deletion
	 */
	static void FlushEdgeDeletionQueue();
public:

	/**
	 * will return the NavMesh world if one exists, or create one if not and return that
	 */
	static FNavMeshWorld* GetNavMeshWorld();

	/**
	 * cleans up the navmesh world
	 */
	static void DestroyNavMeshWorld();

	/**
	 * will return the pylon octree if it's set up already, or if not create it and return the newly created octree
	 * @param bDontCreateNew - if TRUE, a new pylon octree will not be created if there isn't one already.. instead NULL will be returned
	 */
	static FPylonOctreeType* GetPylonOctree(UBOOL bDontCreateNew=FALSE);

	/**
	 * iterate through all pylons in given octree and draw their bounds
	 * @param OctreePtr - pointer to the octree
	 */
	static void DrawPylonOctreeBounds( const FPylonOctreeType* inOctree );

	/**
	 * called to allow the navmesh world to have cross level references set up
	 * (e.g. emit references to active obstacles)
	 */
	static void GetActorReferences(TArray<FActorReference*> &ActorRefs, UBOOL bIsRemovingLevel);


	/**
	 * this function will mark an incoming handle as active (place it in the active list)
	 * @param NewActiveHandle - handle to mark as active
	 */
	static void RegisterActiveHandle(UNavigationHandle* NewActiveHandle);

	/**
	 * this function will remove a handle from the active list
	 * @param HandleToRemove - the handle to remove from the list
	 */
	static void UnregisterActiveHandle(UNavigationHandle* HandleToRemove);

	/** 
	 * will clear any refs to the passed level in all active handles
	 */
	static void ClearRefsToLevel(ULevel* Level);

	/**
	 * clears all references from handles to meshes (e.g. during seamless travel)
	 */
	static void ClearAllNavMeshRefs();

	/**
	 * called after a level is loaded.  Gives pylons a chance to do work
	 * once cross level refs have been fixed up
	 * @param LeveJustFixedUp - the level that just got cross level refs fixed up
	 */
	static void PostCrossLevelRefsFixup(ULevel* LevelJustFixedUp);

	/**
	 * for DEBUG only, used to verify that all pathobjects referenced by the mesh currently are valid
	 */
	static void VerifyPathObjects();

	/**
	 * for DEBUG only, used to verify that all pathobstacles registered currently are valid
	 */
	static void VerifyPathObstacles();

	/**
	 * for DEBUG only, used to print out summaries of all obstacles registered currently 
	 */
	static void PrintObstacleInfo();

	/**
	* for DEBUG only, will draw all edges that don't support the passed entity params 
	* @param PathParams - path params for the entity you want to verify
	*/
	static void DrawNonSupportingEdges( const FNavMeshPathParams& PathParams );

	/**
	 * for DEBUG only, PrintAllPathObjectEdges
	 * - iterates through all edges and prints out info if they are pathobject edges
	 */
	static void PrintAllPathObjectEdges();

	/**
	 * for DEBUG only, verifies all cover references
	 */
	static void VerifyCoverReferences();
};

#if VTABLE_AT_END_OF_CLASS
// See UObjectBase for information about this class
class FNavMeshObjectBase
{
public:
	virtual ~FNavMeshObjectBase()
	{
	}
};
#endif

// base class for navigation mesh objects 
struct FNavMeshObject
#if VTABLE_AT_END_OF_CLASS
	: public FNavMeshObjectBase
#endif
{
	/** Ptr to owning nav mesh */
	class UNavigationMeshBase* NavMesh;

	// Constructor
	FNavMeshObject( UNavigationMeshBase* Mesh ) :
		NavMesh(Mesh)
	{
	}
};

struct FNavMeshPolyBase;

typedef WORD VERTID;
enum {MAXVERTID = MAXWORD};
//#define GRIDSIZE (AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_StepSize/6.0f)
#define GRIDSIZE 5.0f

// should be 1/GRIDSIZE
#define GRIDSIZE_MULT 0.2f 

class FMeshVertex : public FVector
{
public:
	FMeshVertex() : FVector(0)
	{
		VertexHash = GetHashVal();
	}
	FMeshVertex(FVector InV) : FVector(InV)
	{
		VertexHash = GetHashVal();
	}

	inline DWORD GetHashVal() const
	{
		// convert to int bucket index (figure out which grid quad this point is in).. do this
		// to avoid floating point precision issues (e.g. -90 comes out of gridsnap as -89.99999992 sometimes, which hoses the crc)
		// note: we don't care about Z!
		const INT XBucket = appRound(X * GRIDSIZE_MULT);
		const INT YBucket = appRound(Y * GRIDSIZE_MULT);

		// combine bucket values by bitshifting Y over and adding them
		const DWORD CombinedBucketVal = XBucket + (YBucket << 16);
		return CombinedBucketVal;
	}

	/** BUILD: list of polys that use this vertex */
	TArray<FNavMeshPolyBase*> ContainingPolys;
	/** RUNTIME: List of polys that use this vertex - for runtime use */
	TArray<WORD> PolyIndices;
	DWORD VertexHash;

	INT GetNumContainingPolys()
	{
		return (ContainingPolys.Num() > 0 ) ? ContainingPolys.Num() : PolyIndices.Num();
	}

	FNavMeshPolyBase* GetContainingPolyAtIdx(INT Idx, UNavigationMeshBase* NavMesh );


	/**
	 *	will attempt to determine if this vertex is on the boundary of the mesh
	 * @param MyID - the ID of the vertex to test
	 * @return - TRUE if this vert is on the border (some false positives, no false negatives)
	 */
	UBOOL IsBorderVert(VERTID MyID);

	FORCEINLINE FVector ToVector(){	return FVector(X,Y,Z);}

	friend FArchive& operator<<( FArchive& Ar, FMeshVertex& V )
	{
		Ar << V.X << V.Y << V.Z;
		Ar << V.PolyIndices;
		return Ar;
	}

	void DebugLog( INT Idx );

	UBOOL operator==( const FMeshVertex& V ) const;

	FMeshVertex operator=( const FVector& V )
	{
		this->X = V.X;
		this->Y = V.Y;
		this->Z = V.Z;
		this->VertexHash = GetHashVal();
		return *this;
	}

	FMeshVertex operator=( const FMeshVertex& V )
	{
		this->X = V.X;
		this->Y = V.Y;
		this->Z = V.Z;
		this->VertexHash = GetHashVal();
		return *this;
	}
};

typedef TLookupMap< VERTID > ControlPointList; 
typedef TMap< FNavMeshPolyBase* , ControlPointList > ControlPointMap;

// RectangleConfiguration struct, used during mesh simplifcation to store potential poly configs
struct RectangleConfiguration
{
	RectangleConfiguration(struct FNavMeshPolyBase* Poly);
	UBOOL operator>(const RectangleConfiguration& Other) const;
	UBOOL operator==(const RectangleConfiguration& Other) const;
	TArray<VERTID> Verts;
	ControlPointList ControlPoints;
	FNavMeshPolyBase* OrigPoly;
};
inline DWORD GetTypeHash(const RectangleConfiguration& Config)
{	
	// MT->Note: this is an intentionally collision heavy hash so that 
	//			 configs with the same verts bot offset still fall in the same bucket
	DWORD Sum = 0; 
	for(INT Idx=0;Idx<Config.Verts.Num();++Idx)
	{
		Sum += Config.Verts(Idx); 
	}
	return Sum;
}

inline DWORD GetTypeHash(const FMeshVertex& Vert)
{
	return Vert.VertexHash;
}

#define DECLARE_EDGE_CLASS(CLASSNAME) \
	static DWORD CLASSNAME##Constructor(TArray<BYTE>& DataBuffer);\
	static void Register();\
	static FName ClassName;

typedef FNavMeshEdgeBase* PathCardinalType;
typedef PathCardinalType PathOpenList;

struct FNavMeshEdgeBase : public FNavMeshObject
{
	// Packed bools
	/** path bookkeeping - are we on the closed list? */
	BITFIELD	bAlreadyVisited:1;
	/** path bookkeeping - is this edge not the longest edge in its group (and therefore should be skipped) ? */
	BITFIELD	bNotLongestEdgeInGroup:1;
	/** TRUE if this edge is pending delete */
	BITFIELD	bPendingDelete:1;
	/** TRUE if this edge is cross-pylon (used to determine which getpoly functions we need to call) */
	BITFIELD	bIsCrossPylon:1;


	/** Vert indices */
	VERTID Vert0;
	VERTID Vert1;
	/** Polys attached to this edge (used only during build) */
	TArray<FNavMeshPolyBase*> BuildTempEdgePolys;
	/** Computed effective (e.g. supported) edge length */
	FLOAT EffectiveEdgeLength;

// edge center is in local space so we want people to use the accessor!
protected:
	/** Center of the edges */
	FVector EdgeCenter;

	/** cached perpendicular of this edge */
	FVector EdgePerp;

public:
	/** Type of edge */
	BYTE	EdgeType;
	/** Extra edge cost */
	DWORD	ExtraEdgeCost;
	/** when edges overlap due to multiple sizes, they are grouped together, this identifies their group */
	BYTE EdgeGroupID;

	/** Bookkeeping for pathfinding */
	// -- weights 
	//  --- g
	INT		VisitedPathWeight;
	//  --- g+h
	INT		EstimatedOverallPathWeight;
	// -- closed list
	// NOTE: closed list is determined by 'bAlreadyVisited' which is above due to bool packing

	// -- open list
	FNavMeshEdgeBase* NextOpenOrdered;
	FNavMeshEdgeBase* PrevOpenOrdered;
	// -- A* predecessor
	FNavMeshEdgeBase* PreviousPathEdge;
	// -- position along predecessor edge taken to get to this edge
	FVector PreviousPosition;	
	// -- saved ID for the last pathfind this poly had its variables set for (used to track when this poly's pathfinding vars are stale) */
	DWORD SavedSessionID;
	// -- Destination poly ID, the ID (0/1) of the poly assocated with this edge that we are going toward for this pathfind 
	BYTE DestinationPolyID;
	/** end pathfinding bookkeeping */




	// Constructor	
	FNavMeshEdgeBase() : FNavMeshObject(NULL),
		bAlreadyVisited(FALSE),
		bNotLongestEdgeInGroup(FALSE),
		bPendingDelete(FALSE),
		bIsCrossPylon(FALSE),
		Vert0(MAXVERTID),
		Vert1(MAXVERTID),
		EffectiveEdgeLength(0.f),
		EdgeCenter(0.f),
		EdgeType(0),
		ExtraEdgeCost(0),
		EdgeGroupID(MAXBYTE),
		VisitedPathWeight(-1),
		EstimatedOverallPathWeight(-1),
		NextOpenOrdered(NULL),
		PrevOpenOrdered(NULL),
		PreviousPathEdge(NULL),
		PreviousPosition(0.f),
		SavedSessionID(MAXDWORD),
		DestinationPolyID(0)
		{}
	FNavMeshEdgeBase( UNavigationMeshBase* Mesh, VERTID IdxV1, VERTID IdxV2 );

	DECLARE_EDGE_CLASS(FNavMeshEdgeBase)


	/**
	 * Will conditionally clear out transient variable used during a path search. (if SessionID does not match the saved ID)
	 * @param SessionID - the ID of the current path search (to determine if we need clearing or not)
	 */
	FORCEINLINE void ConditionalClear(INT SessionID)
	{
		if(SavedSessionID != SessionID)
		{
			bAlreadyVisited		= FALSE;
			bNotLongestEdgeInGroup = FALSE;
			VisitedPathWeight		= 0;
			EstimatedOverallPathWeight = 0;
			NextOpenOrdered		= NULL;
			PrevOpenOrdered		= NULL;
			PreviousPathEdge	= NULL;
			DestinationPolyID   = 0;

			SavedSessionID = SessionID;
		}
	}

	/**
	 * returns the polygon being used as the destination for this edge during pathfinding
	 * (e.g. what direction through this edge we're going)
	 */
	FNavMeshPolyBase* GetPathDestinationPoly()
	{
		return ( DestinationPolyID == 0 ) ? GetPoly0() : GetPoly1();
	}

	/**
	 * returns TRUE if the passed edge is in the same edge group as this edge
	 * @param OtherEdge - edge to check for groupedness
	 */
	FORCEINLINE UBOOL IsInSameGroupAs(FNavMeshEdgeBase* OtherEdge)
	{
		if ( OtherEdge == NULL || EdgeGroupID==MAXBYTE)
		{
			return FALSE;
		}
		if ( OtherEdge->EdgeGroupID != EdgeGroupID )
		{
			return FALSE;
		}

		UBOOL bIsCrossPylonEdge = IsCrossPylon();
		if( EdgeType != OtherEdge->EdgeType || bIsCrossPylonEdge != OtherEdge->IsCrossPylon() )
		{
			return FALSE;
		}


		if( !bIsCrossPylonEdge )
		{
			return (Poly0 == OtherEdge->Poly0 && Poly1 == OtherEdge->Poly1) || 
				   (Poly0 == OtherEdge->Poly1 && Poly1 == OtherEdge->Poly0);
		}

		// if we are CP we need to call getpoly (avoid this normally cuz it's slow)
		FNavMeshPolyBase* MyPoly0 = GetPoly0();
		FNavMeshPolyBase* MyPoly1 = GetPoly1();

		FNavMeshPolyBase* OtherPoly0 = OtherEdge->GetPoly0();
		FNavMeshPolyBase* OtherPoly1 = OtherEdge->GetPoly1();
		return ( (MyPoly0 == OtherPoly0 && MyPoly1 == OtherPoly1) || (MyPoly1 == OtherPoly0 && MyPoly0 == OtherPoly1) );

	}


	/**
	 * NOTE: this will return only VALID edges ( static edges which are marked invalid will be skipped )
	 * gets a list of all edges in the same group as this edge coming from the designated srcpoly
	 * @param SrcPoly - poly we want the group for (all edges from this poly to the poly this edge points to that match this edge's group ID)
	 * @param out_EdgesInGroup - the edges in the group
	 */
	void GetAllEdgesInGroup(FNavMeshPolyBase* SrcPoly, TArray<FNavMeshEdgeBase*>& out_EdgesInGroup);

	/**
	 * NOTE: returns ONLY static (built offline) edges, ignores dynamic edges
	 * gets a list of all edges in the same group as this edge coming from the designated srcpoly
	 * @param SrcPoly - poly we want the group for (all edges from this poly to the poly this edge points to that match this edge's group ID)
	 * @param out_EdgesInGroup - the edges in the group
	 */
	void GetAllStaticEdgesInGroup(FNavMeshPolyBase* SrcPoly, TArray<FNavMeshEdgeBase*>& out_EdgesInGroup);
	/**
	 * determines if a move from this edge to the provided edge is valid, given the supplied pathparams
	 * @param PathParams - pathing parameters we're doing our determination for
	 * @param NextEdge - the edge we need to move to from this one
	 * @param PolyToMoveThru - the polygon we'd be moving through edge to edge
	 * @return TRUE if the move is possible
	 */
	virtual UBOOL SupportsMoveToEdge(const FNavMeshPathParams& PathParams, FNavMeshEdgeBase* NextEdge, FNavMeshPolyBase* PolyToMoveThru);

	/**
	 * will return whether or not this edge uses the passed vert
	 * @param VertID - the vert to check for
	 * @return TRUE If the vert is being used by this edge
	 */
	virtual UBOOL HasVert(VERTID VertID)
	{
		return ( Vert0 == VertID || Vert1 == VertID );
	}

	/**
	 * this function is called after a poly linked to this edge is replaced with a submesh for a pathobstacle
	 * allows special edges to have a chance to add extra data after the mesh is split
	 * @param Poly - the poly that was just disabled and replaced with a submesh
	 * @param NewSubMesh - the submesh that now represents the poly
	 * @param bFromBackRef - are we adding edges from a backreference? (e.g. B just got rebuild, and we're doing edge adds for an edge that points from A->B)
	 */
	virtual void PostSubMeshUpdateForOwningPoly(FNavMeshPolyBase* Poly, UNavigationMeshBase* New_SubMesh, UBOOL bFromBackRef=FALSE){}


	// returns a debug string with info about this edge (to be overidden by sublcasses)
	virtual FString GetClassSpecificDebugText();
	
	// will print basic debug info about this edge, as well as class specific info about it
	FString GetDebugText();

	// returns the poly attached to this edge which is NOT the passed poly
	virtual FNavMeshPolyBase* GetOtherPoly(FNavMeshPolyBase* Poly);
	
	/**
	 * returns the cost of this edge for the passed object
	 * @param Interface - the interface of the entity pathing along this edge
	 * @param PreviousPoint - the point on the predecessor edge we are pathing from (and which we are computing costs from)
	 * @param out_PathEdgePoint - the point we used along this edge to compute cost
	 * @param SourcePoly - the source poly for this edge's consideration ( the poly we're coming from )
	 * @return - cost of traversing this edge
	 */
	virtual INT CostFor( 	const FNavMeshPathParams& PathParams,
							const FVector& PreviousPoint,
							FVector& out_PathEdgePoint,
							FNavMeshPolyBase* SourcePoly);
	
	/**
	 * returns whether or not this edge supports the searcher	
	 * @param PathParams - parameter struct containing information about the searching entity
	 * @param CurPoly - poly we're coming from (e.g. the poly we're trying to leave)
	 * @param PredecessorEdge - the edge we're coming to this edge from 
	 * @return TRUE if this edge supports the searcher
	 */
	virtual UBOOL Supports( const FNavMeshPathParams& PathParams,
							FNavMeshPolyBase* CurPoly,
							FNavMeshEdgeBase* PredecessorEdge);
	
	/**
	* attempts to find edges which are close to co-linear forming a span
	* @param out_Edges	- OUT the edges found in teh span
	*/
	virtual void FindSpanEdges(TLookupMap<FNavMeshEdgeBase*>& out_Edges);

	/*
	* uses the midpoint of the found span to determine the location furthest away from boundaries
	* @return the location furthest away from boundaries along the edge
	*/
	FVector FindMultiEdgeSpanMidPoint();
	
	virtual void DrawEdge( FDebugRenderSceneProxy* DRSP, FColor C, FVector DrawOffset=FVector(0,0,0) );
	virtual void DrawEdge( ULineBatchComponent* LineBatcher, FColor C, FVector DrawOffset=FVector(0,0,0) );
	virtual FColor GetEdgeColor();

	/**
	 *	will get the vertex location at the specified local index 
	 * @param LocalIdx - local index of vert you want a location for
	 * @param bWorldSpace - whether you want the loc in world space or not
	 * @return the vertex location
	 */
	virtual FVector GetVertLocation(INT LocalIdx, UBOOL bWorldSpace=WORLD_SPACE);

	/**
	 *	returns the first poly associated with this edge
	 * @return the first poly associated with this edge
	 */
	FNavMeshPolyBase* GetPoly0();

	/**
	 *	returns the second poly associated with this edge
	 * @return the second poly associated with this edge
	 */
	FNavMeshPolyBase* GetPoly1();


	/**
	 * NOTE: this version is virtual, and only called on cross-pylon edges\
	 *	returns the first poly associated with this edge
	 * @return the first poly associated with this edge
	 */
	virtual FNavMeshPolyBase* CrossPylon_GetPoly0(){return NULL;}

	/**
	 * NOTE: this version is virtual, and only called on cross-pylon edges
	 *	returns the second poly associated with this edge
	 * @return the second poly associated with this edge
	 */
	virtual FNavMeshPolyBase* CrossPylon_GetPoly1(){return NULL;}

	/**
	 *	sets the first poly associated with this edge
	 * @param Poly0 the first poly to be associated with this edge
	 */
	virtual void SetPoly0(FNavMeshPolyBase* Poly0);
	
	/**
	 *	sets the second poly associated with this edge
	 * @param Poly0 the second poly to be associated with this edge
	 */
	virtual void SetPoly1(FNavMeshPolyBase* Poly1);

	/**
	 *	will return the center of the edge
	 * @param bWorldSpace - true if you want the center in world space
	 * @return the center of the edge
	 */
	FVector GetEdgeCenter(UBOOL bWorldSpace=WORLD_SPACE);

	/**
	 * returns the length of this edge
	 * @return legnth of the edge or -1.0f if it could not be calculated 
	 */
	FORCEINLINE FLOAT GetEdgeLength()
	{
		return (GetVertLocation(0,LOCAL_SPACE) - GetVertLocation(1,LOCAL_SPACE)).Size();
	}

	/**
	 * returns the length of this edge
	 * @return legnth of the edge or -1.0f if it could not be calculated 
	 */
	FORCEINLINE FLOAT GetEdgeLengthSq()
	{
		return (GetVertLocation(0,LOCAL_SPACE) - GetVertLocation(1,LOCAL_SPACE)).SizeSquared();
	}

	/** 
	 * Sets EdgeCenter variable based on the Edge Verts
	 */
	void UpdateEdgeCenter( UNavigationMeshBase* NavMesh );

	/** 
	 * Generates the cached perpdir for this edge
	 */
	void UpdateEdgePerpDir();

	/**
	 *	Returns a vector perpindicular to this edge (edge norm cross edgedir)
	 * @param bWorldSpace - whether this operation should be in world space or local space
	 * @return - the perp vector
	 */
	FVector GetEdgePerpDir(UBOOL bWorldSpace=WORLD_SPACE);

	/**
	 *	will return an approriate normal for this edge (avg of two polys it connects)
	 * @param bWorldSpace - should be TRUE if you want a normal in world space
	 * @return - edge normal
	 */
	FVector GetEdgeNormal(UBOOL bWorldSpace=WORLD_SPACE);


	/** 
	 * returns the point that the passed searcher should run to in order to pass over this edge
	 * (will return a safe point along the edge to run to that does not collide)
	 * @param EntityRadius - the radius of the entity we need to build an edge position for
	 * @param InfluencePosition - the position we should try and get close to with our edge position
	 * @param EntityPosition    - the position the entity will be in when it starts moving toward this edge
	 * @param Handle			- the handle that's requesting the edge position
	 * @param bFirstPass		- when TRUE do not do kisspoint (corner) avoidance, only take closest point along edge
	 */
	virtual FVector GetEdgeDestination( const FNavMeshPathParams& PathParams,
										FLOAT EntityRadius,
										const FVector& InfluencePosition,
										const FVector& EntityPosition,
										UNavigationHandle* Handle,
										UBOOL bFirstPass = FALSE );

	/**
	 * @return FVector the point where the entity will be after moving through this edge, mostly useful for special edges
	 */
	virtual FVector GetFinalEdgeDestination() { return GetEdgeCenter();	}

	/**
	 *	Will return the distance of the passed point to this edge
	 *  @param InPoint - point to find distance from
	 *  @param bWorldSpace- whether the incoming point is in WorldSpace or not
	 *  @param out_ClosestPt - output param for closest point on the edge to the provided inPoint
	 *  @return - distance from edge to inpoint
	 */
	FLOAT PointDistToEdge(const FVector& InPoint, UBOOL bWorldSpace=WORLD_SPACE, FVector* out_ClosestPt=NULL);

	/**
	 *	Called from NavigationHandle.SuggestMovePreparation.  Allows this edge to run custom logic
	 *  related to traversing this edge (e.g. jump, play special anim, etc.)
	 * @param C - controller about to traverse this edge
	 * @param out_Movept - point where this we should move to cross this edge (stuffed with non-special edge value to begin)
	 * @return TRUE if this edge handled all movement to this edge, FALSE if the AI should still move to movept
	 */
	virtual UBOOL PrepareMoveThru( class AController* C, FVector& out_MovePt );

	/**
	 * IsOneWayEdge
	 * - indicates whether or not this edge is a non-shared edge, or an edge which could be added to one adjacent poly and not the other
	 *  (e.g. special edges that only work in one direction should return TRUE)
	 * NOTE: By default edges that are one way are ignored during obstacle mesh creation
	 */
	virtual UBOOL IsOneWayEdge() { return FALSE; }

	/**
	 * @return - should this edge be considered when building obstacle geometry? (default yes)
	 */
	virtual UBOOL ShouldBeConsideredForObstacleGeo(){ return !IsOneWayEdge(); }

	/**
	 * @return - should we add obstacle geometry for this edge as a post step during submesh builds? (e.g. special edges that guys can't simply walk over)
	 */
	virtual UBOOL NeedsObstacleGeoAddedDuringSubMeshBuilds(){ return FALSE; }

	// Returns the type of edge
	virtual BYTE  GetEdgeType() { return NAVEDGE_Normal; }

	virtual FArchive& Serialize( FArchive& Ar );
	virtual void SerializeEdgeVerts( FArchive& Ar );
	friend FArchive& operator<<( FArchive& Ar, FNavMeshEdgeBase& E )
	{
		return E.Serialize( Ar );
	}

	/**
	 * will add this edge to whatever cache lists it needs to (default is just to the edgeptr list)
	 * @param Mesh - mesh associated with this edge
	 */
	virtual void Cache(class UNavigationMeshBase* Mesh);
	
	/**
	 * virtual function that allows some minimal type checking (determines if this edge is derived from FNavMeshCrossPylonEdge)
	 */
	UBOOL IsCrossPylon(){ return bIsCrossPylon; }

	
	/**
	 * IsValid()
	 * allows edges which require special initialization to invalidate themselves when 
	 * said initialization has not taken place
	 * @param bAllowTopLevelEdgesWhenSubMeshPresent - when true edges which have been superceded by submesh edges will be allowed
	 * NOTE: edges which link to polys which have a submesh will indicate they are invalid
	 */
	virtual UBOOL IsValid(UBOOL bAllowTopLevelEdgesWhenSubMeshPresent=FALSE);

	/**
	 * AllowMoveToNextEdge
	 * this function is called from GetNextMoveLocation and allows specialized edges
	 * to keep getnextmovelocation from moving to the next edge in line when the edge's special behavior has not been triggered yet
	 * (e.g. an edge which trips an anim or some such)
	 * @param PathParams - path param struct associated with the entity being processed
	 * @param bInPoly0 - are we in poly0 of ttis edge?
	 * @param bInPoly1 - are we in poly1 of this edge?
	 * @return TRUE if it's OK to move to the next edge
	 */
	 virtual UBOOL AllowMoveToNextEdge(FNavMeshPathParams& PathParams, UBOOL bInPoly0, UBOOL bInPoly1){ return TRUE; }

	 /** 
	  * allows this edge to override getnextmovelocation
	  * @param Handle - handle who is using this edge
	  * @param out_Dest - out param describing the destination we should strive for
	  * @param Arrivaldistance - distance within which we will be considered 'arrived' at a point
	  * @param out_ReturnStatus - value GetNextMoveLocation should return
	  * @return TRUE if this edge is overriding getnextmovelocation
	  */
	 virtual UBOOL OverrideGetNextMoveLocation(UNavigationHandle* Handle, FVector& out_Dest, FLOAT ArrivalDistance, UBOOL& out_ReturnStatus )
	 {
		 return FALSE;
	 }

	/**
	 * called on edges to allow them to notify actorref code that they do infact reference actors in another level
	 * @param ActorRefs - list of actor refs currently
	 * @param bIsRemovingLevel -whether we're removing a level (e.g. emit refs that should be nulled)
	 * @param bIsDynamicEdge - whether this edge is dynamic or not
	 */
	virtual void GetActorReferences(TArray<FActorReference*> &ActorRefs, UBOOL bIsRemovingLevel, UBOOL bIsDynamicEdge){}

	/**
	 * called when levels are being saved and cross level refs need to be cleared
	 */
	virtual UBOOL ClearCrossLevelReferences(){return FALSE;}

	/**
	 * will do a line check against ONLY this edge's related obstacle mesh for its navmesh(es).  Does not do a full obstaclelinecheck (doesn't conform to terrain, or check walkable mesh)	
	 * @param Result - output hit result describing the hit
	 * @param End - endpoint of line check
	 * @param Start - start point of linecheck
	 * @param Extent - extent of box to sweep
	 * @param TraceFlags - trace flags to pass to LineCheck()
	 * @return TRUE if there was no hit
	 */
	UBOOL LimitedObstacleLineCheck( FCheckResult& Result, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags );
	
	/**
	 * will do a point check against ONLY this edge's related obstacle mesh for its navmesh(es). 
	 * @param Result - output hit result describing the hit
	 * @param Pt - point to check
	 * @param Extent of box around point to check
	 * @param TraceFlags - traceflags to pass to PointCheck()
	 * @return TRUE if there was no hit
	 */
	UBOOL LimitedObstaclePointCheck( FCheckResult& Result, const FVector& Pt, const FVector& Extent, DWORD TraceFlags );
	
	/**
	 * callback after a path for an AI is generated, allowing each edge to modify things on the handle if necessary
	 * @param Handle - the handle whose path was just constructed
	 * @param PathIdx - index into the handle path array that points to this edge
	 * @return TRUE if we modified the pathcache
	 */
	virtual UBOOL PathConstructedWithThisEdge(UNavigationHandle* Handle, INT PathIdx){return FALSE;}

	friend UBOOL ExistsEdgeFromPoly1ToPoly2Fast(FNavMeshPolyBase* Poly1, FNavMeshPolyBase* Poly2);

private:
	WORD Poly0;
	WORD Poly1;
};


// normal edge that is only added to one poly along edge (e.g. Only PolyEdges of Poly0 will have a ref to this edge)
struct FNavMeshBasicOneWayEdge : public FNavMeshEdgeBase
{
	DECLARE_EDGE_CLASS(FNavMeshBasicOneWayEdge)

	FNavMeshBasicOneWayEdge( UNavigationMeshBase* Mesh, VERTID IdxV1, VERTID IdxV2 )
		: FNavMeshEdgeBase(Mesh,IdxV1,IdxV2){}

	FNavMeshBasicOneWayEdge() : FNavMeshEdgeBase() {}


	virtual UBOOL IsOneWayEdge() { return TRUE; }
	virtual UBOOL NeedsObstacleGeoAddedDuringSubMeshBuilds(){ return TRUE; }

	// returns a debug string with info about this edge
	virtual FString GetClassSpecificDebugText() { return FString(TEXT("FNavMeshBasicOneWayEdge")); }

	virtual FColor GetEdgeColor();

	virtual void DrawEdge( FDebugRenderSceneProxy* DRSP, FColor C, FVector DrawOffset=FVector(0,0,0) );
	virtual void DrawEdge( ULineBatchComponent* LineBatcher, FColor C, FVector DrawOffset=FVector(0,0,0) );

};

// Specialized edge used for linking polys between pylons
// @NOTE: EdgePoly index buffer indexes into the CrossPylon ref array, not the normal pylon array
// @NOTE: EdgeVerts contains indecies for both meshes (e.g. mesh0_vert0,mesh0_vert1,Mesh1_vert0,Mesh1_vert1)
struct FNavMeshCrossPylonEdge : public FNavMeshEdgeBase
{
	DECLARE_EDGE_CLASS(FNavMeshCrossPylonEdge)

	// Constructor	
	FNavMeshCrossPylonEdge( UNavigationMeshBase* OwningMesh,
							APylon* Pylon0,
							WORD Pylon0PolyIdx,
							VERTID Pylon0IdxV1,
							VERTID Pylon0IdxV2,
							APylon* Pylon1,
							WORD Pylon1PolyIdx,
							VERTID Pylon1IdxV1,
							VERTID Pylon1IdxV2 );

	FNavMeshCrossPylonEdge( UNavigationMeshBase* OwningMesh,
							struct FNavMeshPolyBase* Poly0,
							VERTID Pylon0IdxV1,
							VERTID Pylon0IdxV2,
							struct FNavMeshPolyBase* Poly1,
							VERTID Pylon1IdxV1,
							VERTID Pylon1IdxV2 );

	FNavMeshCrossPylonEdge() : FNavMeshEdgeBase() { bIsCrossPylon=TRUE; }

	/**
	 * will return whether or not this edge uses the passed vert
	 * @param VertID - the vert to check for
	 * @return TRUE If the vert is being used by this edge
	 */
	virtual UBOOL HasVert(VERTID VertID)
	{
		return (FNavMeshEdgeBase::HasVert(VertID) || OtherPylonVert0 == VertID || OtherPylonVert1 == VertID );
	}

	// returns a debug string with info about this edge
	virtual FString GetClassSpecificDebugText() { return FString::Printf(TEXT("FNavMeshCrossPylonEdge (Valid? %i)"),IsValid()); }


	/**
	 *	given one poly associated with this edge, will return the other
	 * @param Poly - first poly (to find adjacent poly for)
	 * @return - other poly
	 */
	virtual FNavMeshPolyBase* GetOtherPoly(FNavMeshPolyBase* Poly);

	/**
	 * (overidden from base)
	 *	will get the vertex location at the specified local index 
	 * @param LocalIdx - local index of vert you want a location for
	 * @param bWorldSpace - whether you want the loc in world space or not
	 * @return the vertex location
	 */
	virtual FVector GetVertLocation(INT LocalIdx, UBOOL bWorldSpace=WORLD_SPACE);

	virtual FNavMeshPolyBase* CrossPylon_GetPoly0();
	virtual FNavMeshPolyBase* CrossPylon_GetPoly1();

	virtual void SetPoly0(FNavMeshPolyBase* Poly0);
	virtual void SetPoly1(FNavMeshPolyBase* Poly1);

	virtual void DrawEdge( FDebugRenderSceneProxy* DRSP, FColor C, FVector DrawOffset=FVector(0,0,0) );
	virtual FColor GetEdgeColor();

	// Overidden to also add ourselves to the crosspylonedge list
	virtual void Cache(class UNavigationMeshBase* Mesh);
	
	/**
	 * IsValid()
	 * -overidden to mark ourselves invalid if one or more of our refs isn't loaded
	 */
	virtual UBOOL IsValid(UBOOL bAllowTopLevelEdgesWhenSubMeshPresent=FALSE);

	virtual INT CostFor( const FNavMeshPathParams& PathParams,
						const FVector& PreviousPoint,
						FVector& out_PathEdgePoint,
						FNavMeshPolyBase* SourcePoly );

	
	/**
	*	Finds obstacle geo near this edge and links the edge to it
	* @param EdgeIdx - index of new edge 
	* @param ObstacleMesh - obstacle mesh to link this edge to 
	* @param bDynamicEdge - TRUE if this edge is dynamic (added at runtime)
	*/
	void LinkToObstacleGeo(WORD EdgeIdx, UNavigationMeshBase* ObstacleMesh, UBOOL bDynamicEdge=FALSE);

	/**
	 * called on edges to allow them to notify actorref code that they do infact reference actors in another level
	 * @param ActorRefs - list of actor refs currently
	 * @param bIsRemovingLevel -whether we're removing a level (e.g. emit refs that should be nulled)
	 * @param bIsDynamicEdge - whether this edge is dynamic or not
	 */
	virtual void GetActorReferences(TArray<FActorReference*> &ActorRefs, UBOOL bIsRemovingLevel, UBOOL bIsDynamicEdge);

	/**
	 * called when levels are being saved and cross level refs need to be cleared
	 */
	virtual UBOOL ClearCrossLevelReferences();


	virtual FArchive& Serialize( FArchive& Ar );

	virtual void SerializeEdgeVerts( FArchive& Ar );


	/** Vert indices for the other pylon */
	VERTID OtherPylonVert0;
	VERTID OtherPylonVert1;

	FPolyReference Poly0Ref;
	FPolyReference Poly1Ref;

	// ID of the obstacle mesh poly this edge is linked to 
	WORD ObstaclePolyID;

};


/**
 * this is a dummy edge which gets added to destination polys of one-way specialized edges so we have a reference back
 * to the source edge
 */
 struct FNavMeshOneWayBackRefEdge : public FNavMeshCrossPylonEdge
 {
	DECLARE_EDGE_CLASS(FNavMeshOneWayBackRefEdge )

	// Constructor	
	FNavMeshOneWayBackRefEdge( UNavigationMeshBase* OwningMesh,
							APylon* Pylon0,
							WORD Pylon0PolyIdx,
							VERTID Pylon0IdxV1,
							VERTID Pylon0IdxV2,
							APylon* Pylon1,
							WORD Pylon1PolyIdx,
							VERTID Pylon1IdxV1,
							VERTID Pylon1IdxV2 ) : FNavMeshCrossPylonEdge( OwningMesh,
																			Pylon0,Pylon0PolyIdx,Pylon0IdxV1,Pylon0IdxV2,
																			Pylon1, Pylon1PolyIdx, Pylon1IdxV1, Pylon1IdxV2)
																			{EdgeType=NAVEDGE_BackRefDummy;}

	FNavMeshOneWayBackRefEdge( UNavigationMeshBase* OwningMesh,
						struct FNavMeshPolyBase* Poly0,
							VERTID Pylon0IdxV1,
							VERTID Pylon0IdxV2,
						struct FNavMeshPolyBase* Poly1,
							VERTID Pylon1IdxV1,
							VERTID Pylon1IdxV2 ) : FNavMeshCrossPylonEdge( OwningMesh, Poly0, Pylon0IdxV1, Pylon0IdxV2,
																			Poly1, Pylon1IdxV1, Pylon1IdxV2)
																			{EdgeType=NAVEDGE_BackRefDummy;}
	FNavMeshOneWayBackRefEdge() : FNavMeshCrossPylonEdge() {}


	/**
	 * returns whether or not this edge supports the searcher	
	 * @param PathParams - parameter struct containing information about the searching entity
	 * @param CurPoly - poly we're coming from (e.g. the poly we're trying to leave)
	 * @param PredecessorEdge - the edge we're coming to this edge from 
	 * @return TRUE if this edge supports the searcher
	 */
	virtual UBOOL Supports( const FNavMeshPathParams& PathParams,
							FNavMeshPolyBase* CurPoly,
							FNavMeshEdgeBase* PredecessorEdge)
	{
		// never valid for pathing
		return FALSE;
	}

	/**
	 * returns the cost of this edge for the passed object
	 * @param Interface - the interface of the entity pathing along this edge
	 * @param PreviousPoint - the point on the predecessor edge we are pathing from (and which we are computing costs from)
	 * @param out_PathEdgePoint - the point we used along this edge to compute cost
	 * @param SourcePoly - the source poly for this edge's consideration ( the poly we're coming from )
	 * @return - cost of traversing this edge
	 */
	virtual INT CostFor( 	const FNavMeshPathParams& PathParams,
							const FVector& PreviousPoint,
							FVector& out_PathEdgePoint,
							FNavMeshPolyBase* SourcePoly)
	{
		return UCONST_BLOCKEDPATHCOST;
	}

	// returns a debug string with info about this edge
	virtual FString GetClassSpecificDebugText()
	{ 
		return FString::Printf(TEXT("FNavMeshOneWayBackRefEdge (not valid for pathing)")); 
	}

	/**
	 * this function is called after a poly linked to this edge is replaced with a submesh for a pathobstacle
	 * allows special edges to have a chance to add extra data after the mesh is split
	 * @param Poly - the poly that was just disabled and replaced with a submesh
	 * @param NewSubMesh - the submesh that now represents the poly
	 * @param bFromBackRef - are we adding edges from a backreference? (e.g. B just got rebuild, and we're doing edge adds for an edge that points from A->B)
	 */
	virtual void PostSubMeshUpdateForOwningPoly(FNavMeshPolyBase* Poly, UNavigationMeshBase* New_SubMesh, UBOOL bFromBackRef=FALSE);

	/**
	 * IsOneWayEdge
	 * - indicates whether or not this edge is a non-shared edge, or an edge which could be added to one adjacent poly and not the other
	 *  (e.g. special edges that only work in one direction should return TRUE)
	 * NOTE: By default edges that are one way are ignored during obstacle mesh creation
	 */
	virtual UBOOL IsOneWayEdge() { return TRUE; }


	virtual BYTE GetEdgeType()
	{
		return NAVEDGE_BackRefDummy;
	}
 };

// Base class poly edge for special moves
struct FNavMeshSpecialMoveEdge : public FNavMeshCrossPylonEdge
{
	DECLARE_EDGE_CLASS(FNavMeshSpecialMoveEdge)
	/** Relevant actor that this special move is linked to */
	FActorReference RelActor;
	/** Relevant item associated with relative actor, if any */
	INT RelItem;

	/** Destination that this move will take us (from edge center) */
	FBasedPosition MoveDest;
	/** Direction this edge goes (1 == right or up, -1 = left or down) */
	INT MoveDir;

	FNavMeshSpecialMoveEdge() : FNavMeshCrossPylonEdge(), MoveDest(EC_EventParm), MoveDir(0) {}
	FNavMeshSpecialMoveEdge( UNavigationMeshBase* OwningMesh,
							APylon* Pylon0,
							WORD Pylon0PolyIdx,
							VERTID Pylon0IdxV1,
							VERTID Pylon0IdxV2,
							APylon* Pylon1,
							WORD Pylon1PolyIdx,
							VERTID Pylon1IdxV1,
							VERTID Pylon1IdxV2 );

	FNavMeshSpecialMoveEdge( UNavigationMeshBase* OwningMesh,
							struct FNavMeshPolyBase* Poly0,
							VERTID Pylon0IdxV1,
							VERTID Pylon0IdxV2,
							struct FNavMeshPolyBase* Poly1,
							VERTID Pylon1IdxV1,
							VERTID Pylon1IdxV2 );
	
							
	/**
	 * this function is called after a poly linked to this edge is replaced with a submesh for a pathobstacle
	 * allows special edges to have a chance to add extra data after the mesh is split
	 * @param Poly - the poly that was just disabled and replaced with a submesh
	 * @param NewSubMesh - the submesh that now represents the poly
	 * @param bFromBackRef - are we adding edges from a backreference? (e.g. B just got rebuild, and we're doing edge adds for an edge that points from A->B)
	 */
	virtual void PostSubMeshUpdateForOwningPoly(FNavMeshPolyBase* Poly, UNavigationMeshBase* New_SubMesh, UBOOL bFromBackRef=FALSE);

	/**
	 * called when a special move edge is being re-fixed up after an obstacle has changed the mesh
	 * allows each subclass to add it own proper type
	 * is responsible for adding a new edge represented by the two spans
	 * @param Source_Span - Span for source 
	 * @param Dest_Span - Span for dest
	 */
	void AddDynamicEdgeForSpan(UNavigationMeshBase* NavMesh, struct FPolySegmentSpan& Source_Span, struct FPolySegmentSpan& Dest_Span);

	/**
	 * wrapper each subclass should override which should simply mirror AddDynamicCrossPylonEdge and pass the proper type
	 * to the template (e.g. AddOneWayCrossPylonEdgeToMesh<FNavMeshMantleEdge>(EdgeStart, EdgeEnd, ConnectedPolys, &EdgePtr );
	 */
	virtual FNavMeshSpecialMoveEdge* AddTypedEdgeForObstacleReStitch(UNavigationMeshBase* NavMesh, const FVector& StartPt, const FVector& EndPt, TArray<FNavMeshPolyBase*>& ConnectedPolys){check(FALSE&&"IMPLEMENT THIS FUNCTION");return NULL;}


	// returns a debug string with info about this edge
	virtual FString GetClassSpecificDebugText()
	{ 
		return FString::Printf(TEXT("FNavMeshSpecialMoveEdge (Actor: %s RelItem: %i MoveDir: %i)"),(*RelActor!=NULL) ? *(*RelActor)->GetName() : TEXT("NULL"),RelItem,MoveDir); 
	}

	virtual FColor GetEdgeColor();
	virtual void DrawEdge( FDebugRenderSceneProxy* DRSP, FColor C, FVector DrawOffset=FVector(0,0,0) );

	virtual UBOOL PrepareMoveThru( class AController* C, FVector& out_MovePt );
	// This edge only goes from Poly0 -> Poly1? 
	virtual UBOOL IsOneWayEdge() { return TRUE; }

	virtual FArchive& Serialize( FArchive& Ar );

	/**
	 * called on edges to allow them to notify actorref code that they do infact reference actors in another level
	 * @param ActorRefs - list of actor refs currently
	 * @param bIsRemovingLevel -whether we're removing a level (e.g. emit refs that should be nulled)
	 * @param bIsDynamicEdge - whether this edge is dynamic or not
	 */
	virtual void GetActorReferences(TArray<FActorReference*> &ActorRefs, UBOOL bIsRemovingLevel, UBOOL bIsDynamicEdge);

	/**
	 * called when levels are being saved and cross level refs need to be cleared
	 */
	virtual UBOOL ClearCrossLevelReferences();

	virtual UBOOL ShouldBeConsideredForObstacleGeo(){ return FALSE; }

	virtual FVector GetEdgeDestination( const FNavMeshPathParams& PathParams,
										FLOAT EntityRadius,
										const FVector& InfluencePosition,
										const FVector& EntityPosition,
										UNavigationHandle* Handle,
										UBOOL bFirstPass = FALSE );

	virtual FVector GetFinalEdgeDestination() { return *MoveDest;	}
};

// Poly edge for mantling
struct FNavMeshMantleEdge : public FNavMeshSpecialMoveEdge
{
	DECLARE_EDGE_CLASS(FNavMeshMantleEdge)

	FNavMeshMantleEdge() : FNavMeshSpecialMoveEdge() {}
	FNavMeshMantleEdge( UNavigationMeshBase* OwningMesh,
						APylon* Pylon0,
						WORD Pylon0PolyIdx,
						VERTID Pylon0IdxV1,
						VERTID Pylon0IdxV2,
						APylon* Pylon1,
						WORD Pylon1PolyIdx,
						VERTID Pylon1IdxV1,
						VERTID Pylon1IdxV2 );

	FNavMeshMantleEdge( UNavigationMeshBase* OwningMesh,
							struct FNavMeshPolyBase* Poly0,
							VERTID Pylon0IdxV1,
							VERTID Pylon0IdxV2,
							struct FNavMeshPolyBase* Poly1,
							VERTID Pylon1IdxV1,
							VERTID Pylon1IdxV2 );


	UBOOL AllowMoveToNextEdge(FNavMeshPathParams& PathParams, UBOOL bInPoly0, UBOOL bInPoly1)
	{
		// always return false, suggest move preparation will remove the edge from the path cache
		return FALSE;
	}

	// returns a debug string with info about this edge
	virtual FString GetClassSpecificDebugText()
	{ 
		return FString::Printf(TEXT("FNavMeshMantleEdge (Actor: %s RelItem: %i MoveDir: %i)"),(*RelActor!=NULL) ? *(*RelActor)->GetName() : TEXT("NULL"),RelItem,MoveDir); 
	}


	/**
	 * returns whether or not this edge supports the searcher	
	 * @param PathParams - parameter struct containing information about the searching entity
	 * @param CurPoly - poly we're coming from (e.g. the poly we're trying to leave)
	 * @param PredecessorEdge - the edge we're coming to this edge from 
	 * @return TRUE if this edge supports the searcher
	 */
	virtual UBOOL Supports( const FNavMeshPathParams& PathParams,
							FNavMeshPolyBase* CurPoly,
							FNavMeshEdgeBase* PredecessorEdge);

	// Returns the type of edge
	virtual BYTE  GetEdgeType() { return NAVEDGE_Mantle; }

	virtual FVector GetEdgeDestination( const FNavMeshPathParams& PathParams,
										FLOAT EntityRadius,
										const FVector& InfluencePosition,
										const FVector& EntityPosition,
										UNavigationHandle* Handle,
										UBOOL bFirstPass = FALSE );

	virtual UBOOL PrepareMoveThru( class AController* C, FVector& out_MovePt );

	/**
	 * wrapper each subclass should override which should simply mirror AddDynamicCrossPylonEdge and pass the proper type
	 * to the template (e.g. AddOneWayCrossPylonEdgeToMesh<FNavMeshMantleEdge>(EdgeStart, EdgeEnd, ConnectedPolys, &EdgePtr );
	 */
	virtual FNavMeshSpecialMoveEdge* AddTypedEdgeForObstacleReStitch(class UNavigationMeshBase* NavMesh, const FVector& StartPt, const FVector& EndPt, TArray<FNavMeshPolyBase*>& ConnectedPolys);
};

// Poly edge for cover slips
struct FNavMeshCoverSlipEdge : public FNavMeshSpecialMoveEdge
{
	DECLARE_EDGE_CLASS(FNavMeshCoverSlipEdge)


	FNavMeshCoverSlipEdge() : FNavMeshSpecialMoveEdge() {}
	FNavMeshCoverSlipEdge(	UNavigationMeshBase* OwningMesh,
							APylon* Pylon0,
							WORD Pylon0PolyIdx,
							VERTID Pylon0IdxV1,
							VERTID Pylon0IdxV2,
							APylon* Pylon1,
							WORD Pylon1PolyIdx,
							VERTID Pylon1IdxV1,
							VERTID Pylon1IdxV2 );

	FNavMeshCoverSlipEdge( UNavigationMeshBase* OwningMesh,
						struct FNavMeshPolyBase* Poly0,
							VERTID Pylon0IdxV1,
							VERTID Pylon0IdxV2,
						struct FNavMeshPolyBase* Poly1,
							VERTID Pylon1IdxV1,
							VERTID Pylon1IdxV2 );

	// returns a debug string with info about this edge
	virtual FString GetClassSpecificDebugText()
	{ 
		return FString::Printf(TEXT("FNavMeshCoverSlipEdge (Actor: %s RelItem: %i MoveDir: %i)"),(*RelActor!=NULL) ? *(*RelActor)->GetName() : TEXT("NULL"),RelItem,MoveDir); 
	}

	/**
	 * returns whether or not this edge supports the searcher	
	 * @param PathParams - parameter struct containing information about the searching entity
	 * @param CurPoly - poly we're coming from (e.g. the poly we're trying to leave)
	 * @param PredecessorEdge - the edge we're coming to this edge from 
	 * @return TRUE if this edge supports the searcher
	 */
	virtual UBOOL Supports( const FNavMeshPathParams& PathParams,
							FNavMeshPolyBase* CurPoly,
							FNavMeshEdgeBase* PredecessorEdge);

	// Returns the type of edge
	virtual BYTE  GetEdgeType() { return NAVEDGE_Coverslip; }

	// less expensive, so they get used more often
	virtual INT CostFor( const FNavMeshPathParams& PathParams,
		const FVector& PreviousPoint,
		FVector& out_PathEdgePoint,
		FNavMeshPolyBase* SourcePoly );

	/**
	 * callback after a path for an AI is generated, allowing each edge to modify things on the handle if necessary
	 * @param Handle - the handle whose path was just constructed
	 * @param PathIdx - index into the handle path array that points to this edge
	 * @return TRUE if we modified the pathcache
	 */
	virtual UBOOL PathConstructedWithThisEdge(UNavigationHandle* Handle, INT PathIdx);

	/**
	 * wrapper each subclass should override which should simply mirror AddDynamicCrossPylonEdge and pass the proper type
	 * to the template (e.g. AddOneWayCrossPylonEdgeToMesh<FNavMeshMantleEdge>(EdgeStart, EdgeEnd, ConnectedPolys, &EdgePtr );
	 */
	virtual FNavMeshSpecialMoveEdge* AddTypedEdgeForObstacleReStitch(class UNavigationMeshBase* NavMesh, const FVector& StartPt, const FVector& EndPt, TArray<FNavMeshPolyBase*>& ConnectedPolys);


	/**
	 *	Called from NavigationHandle.SuggestMovePreparation.  Allows this edge to run custom logic
	 *  related to traversing this edge (e.g. jump, play special anim, etc.)
	 * @param C - controller about to traverse this edge
	 * @param out_Movept - point where this we should move to cross this edge (stuffed with non-special edge value to begin)
	 * @return TRUE if this edge handled all movement to this edge, FALSE if the AI should still move to movept
	 */
	virtual UBOOL PrepareMoveThru( class AController* C, FVector& out_MovePt );

	 /** 
	  * allows this edge to override getnextmovelocation
	  * @param Handle - handle who is using this edge
	  * @param out_Dest - out param describing the destination we should strive for
	  * @param Arrivaldistance - distance within which we will be considered 'arrived' at a point
	  * @param out_ReturnStatus - value GetNextMoveLocation should return
	  * @return TRUE if this edge is overriding getnextmovelocation
	  */
	 virtual UBOOL OverrideGetNextMoveLocation(UNavigationHandle* Handle, FVector& out_Dest, FLOAT ArrivalDistance, UBOOL& out_ReturnStatus );

};

// Poly edge for one-way drop down situations (e.g. bot can fall down this edge, but not climb up)
struct FNavMeshDropDownEdge : public FNavMeshCrossPylonEdge
{
	DECLARE_EDGE_CLASS(FNavMeshDropDownEdge)
	
	// Constructor	
	FNavMeshDropDownEdge( UNavigationMeshBase* OwningMesh,
		APylon* Pylon0,
		WORD Pylon0PolyIdx,
		VERTID Pylon0IdxV1,
		VERTID Pylon0IdxV2,
		APylon* Pylon1,
		WORD Pylon1PolyIdx,
		VERTID Pylon1IdxV1,
		VERTID Pylon1IdxV2 ) : 
						FNavMeshCrossPylonEdge(OwningMesh, 
											   Pylon0,
											   Pylon0PolyIdx,
											   Pylon0IdxV1,
											   Pylon0IdxV2,
											   Pylon1,
											   Pylon1PolyIdx,
											   Pylon1IdxV1,
											   Pylon1IdxV2) {}

	FNavMeshDropDownEdge( UNavigationMeshBase* Mesh, VERTID IdxV1, VERTID IdxV2 );

	FNavMeshDropDownEdge() : FNavMeshCrossPylonEdge(),DropHeight(0.f) {}

	// This edge only goes from Poly0 -> Poly1? 
	virtual UBOOL IsOneWayEdge() { return TRUE; }

	virtual FColor GetEdgeColor();

	virtual BYTE  GetEdgeType() { return NAVEDGE_DropDown; }

	UBOOL PrepareMoveThru( AController* C, FVector& out_MovePt );

	
	/**
	 * returns whether or not this edge supports the searcher	
	 * @param PathParams - parameter struct containing information about the searching entity
	 * @param CurPoly - poly we're coming from (e.g. the poly we're trying to leave)
	 * @param PredecessorEdge - the edge we're coming to this edge from 
	 * @return TRUE if this edge supports the searcher
	 */
	virtual UBOOL Supports( const FNavMeshPathParams& PathParams,
							FNavMeshPolyBase* CurPoly,
							FNavMeshEdgeBase* PredecessorEdge);

	virtual FArchive& Serialize( FArchive& Ar );

	// returns a debug string with info about this edge
	virtual FString GetClassSpecificDebugText() { return FString::Printf(TEXT("FNavMeshDropDownEdge (DropHeight: %.2f)"),DropHeight); }

	// overidden to draw an arrow down
	virtual void DrawEdge( FDebugRenderSceneProxy* DRSP, FColor C, FVector DrawOffset=FVector(0,0,0) );


	FLOAT DropHeight;
	
};

// Poly edge for one-way drop down situations (e.g. bot can fall down this edge, but not climb up)
struct FNavMeshPathObjectEdge : public FNavMeshCrossPylonEdge
{
	DECLARE_EDGE_CLASS(FNavMeshPathObjectEdge)

	FActorReference PathObject;

	// ID used by the pathobjects to identify and distinguish between edges linked to the same PO (e.g. up vs down on ladders)
	INT InternalPathObjectID;

	// Constructor	
	FNavMeshPathObjectEdge( UNavigationMeshBase* OwningMesh,
		APylon* Pylon0,
		WORD Pylon0PolyIdx,
		VERTID Pylon0IdxV1,
		VERTID Pylon0IdxV2,
		APylon* Pylon1,
		WORD Pylon1PolyIdx,
		VERTID Pylon1IdxV1,
		VERTID Pylon1IdxV2 ) : 
	FNavMeshCrossPylonEdge(OwningMesh, 
		Pylon0,
		Pylon0PolyIdx,
		Pylon0IdxV1,
		Pylon0IdxV2,
		Pylon1,
		Pylon1PolyIdx,
		Pylon1IdxV1,
		Pylon1IdxV2)
		{EdgeType= NAVEDGE_PathObject;}

	FNavMeshPathObjectEdge( UNavigationMeshBase* OwningMesh,
		struct FNavMeshPolyBase* Poly0,
		VERTID Pylon0IdxV1,
		VERTID Pylon0IdxV2,
		struct FNavMeshPolyBase* Poly1,
		VERTID Pylon1IdxV1,
		VERTID Pylon1IdxV2 ) : 
	FNavMeshCrossPylonEdge(OwningMesh, 
		Poly0,
		Pylon0IdxV1,
		Pylon0IdxV2,
		Poly1,
		Pylon1IdxV1,
		Pylon1IdxV2)
		{EdgeType = NAVEDGE_PathObject;}

	FNavMeshPathObjectEdge() : FNavMeshCrossPylonEdge(), PathObject(), InternalPathObjectID(-1) {EdgeType=NAVEDGE_PathObject;}



	/**
	 * this function is called after a poly linked to this edge is replaced with a submesh for a pathobstacle
	 * allows special edges to have a chance to add extra data after the mesh is split
	 * @param Poly - the poly that was just disabled and replaced with a submesh
	 * @param NewSubMesh - the submesh that now represents the poly
	 * @param bFromBackRef - are we adding edges from a backreference? (e.g. B just got rebuild, and we're doing edge adds for an edge that points from A->B)
	 */
	virtual void PostSubMeshUpdateForOwningPoly(FNavMeshPolyBase* Poly, UNavigationMeshBase* New_SubMesh, UBOOL bFromBackRef=FALSE);

	// returns a debug string with info about this edge
	virtual FString GetClassSpecificDebugText() { return FString::Printf(TEXT("FNavMeshPathObjectEdge (POActor: %s)"),(*PathObject!=NULL) ? *(*PathObject)->GetName() : TEXT("NULL")); }

	// This edge only goes from Poly0 -> Poly1? 
	virtual UBOOL IsOneWayEdge() { return TRUE; }
	virtual UBOOL ShouldBeConsideredForObstacleGeo(){ return FALSE; }

	virtual BYTE  GetEdgeType() { return NAVEDGE_PathObject; }


	// returns whether or not this edge supports the searcher
	virtual INT CostFor( 	const FNavMeshPathParams& PathParams,
							const FVector& PreviousPoint,
							FVector& out_PathEdgePoint,
							FNavMeshPolyBase* SourcePoly );

		/**
	 * returns whether or not this edge supports the searcher	
	 * @param PathParams - parameter struct containing information about the searching entity
	 * @param CurPoly - poly we're coming from (e.g. the poly we're trying to leave)
	 * @param PredecessorEdge - the edge we're coming to this edge from 
	 * @return TRUE if this edge supports the searcher
	 */
	virtual UBOOL Supports( const FNavMeshPathParams& PathParams,
							FNavMeshPolyBase* CurPoly,
							FNavMeshEdgeBase* PredecessorEdge);

	virtual UBOOL PrepareMoveThru( AController* C, FVector& out_MovePt );

	/** 
	* overidden to pass off this functionality to the path object
	*/
	virtual FVector GetEdgeDestination( const FNavMeshPathParams& PathParams,
										FLOAT EntityRadius,
										const FVector& InfluencePosition,
										const FVector& EntityPosition,
										UNavigationHandle* Handle,
										UBOOL bFirstPass = FALSE );


	/**
	 * called on edges to allow them to notify actorref code that they do infact reference actors in another level
	 * @param ActorRefs - list of actor refs currently
	 * @param bIsRemovingLevel -whether we're removing a level (e.g. emit refs that should be nulled)
	 * @param bIsDynamicEdge - whether this edge is dynamic or not
	 */
	virtual void GetActorReferences(TArray<FActorReference*> &ActorRefs, UBOOL bIsRemovingLevel, UBOOL bIsDynamicEdge);

	virtual FArchive& Serialize( FArchive& Ar );

	virtual void DrawEdge( FDebugRenderSceneProxy* DRSP, FColor C, FVector DrawOffset=FVector(0,0,0) );

	virtual FColor GetEdgeColor();

	virtual UBOOL AllowMoveToNextEdge(FNavMeshPathParams& PathParams, UBOOL bInPoly0, UBOOL bInPoly1);

	/**
	 * called when levels are being saved and cross level refs need to be cleared
	 */
	virtual UBOOL ClearCrossLevelReferences();

};

typedef TDoubleLinkedList<FNavMeshPolyBase*> PolyList;
struct FNavMeshPolyBase : public FNavMeshObject
{
	/** Index into NavMesh poly array - only valid after fixup */
	WORD Item;
	/** Vert indices */
	TArray<VERTID> PolyVerts;
	/** List of edges */
	// MT->Note: If this poly is in the OBSTACLE mesh, and it has an edge in this list it's
	//           actually an ID from the navigation mesh, (since the obstacle mesh has no edges)
	//			 This enables us to tie obstacle polys to cross-pylon edges 
	TArray<WORD> PolyEdges;

private:
	/** additional cost to apply to this poly only for the next pathfinding attempt */
	INT TransientCost;
	/** list of polys that have a non-zero TransientCost - clear TransientCost at end of next pathfinding */
	static TArray<FNavMeshPolyBase*> TransientCostedPolys;

protected:
	/** Center of the triangle */
	// polycenter is in local space, so make people use the accessor 
	FVector PolyCenter;
public:
	/** Position we should expand from when expanding this poly during exploration (used to keep expansion in phase) */
	FVector PolyBuildLoc;
	/** Normal of this poly */
	FVector PolyNormal;
	FBox	BoxBounds;
	FOctreeElementId OctreeId;
	
	/** list node of our position in the border poly list (NULL if not a border polygon) */
	PolyList::TDoubleLinkedListNode* BorderListNode;

	/** Cover available in this polygon */
	TArray<FCoverReference>	PolyCover;

	/** height of entity that this poly supports */
	FLOAT PolyHeight;

	/** number of dynamic obstacles which are affecting this polygon */
	WORD NumObstaclesAffectingThisPoly;

	/** SessionID of the last path search that touched this polygon (useful for determining if the search has seen this poly yet in evaluagegoal)*/
	INT SavedPathSessionID;

	// Constructor
	FNavMeshPolyBase() : 
		FNavMeshObject(NULL),
		TransientCost(0),
		BorderListNode(NULL),
		NumObstaclesAffectingThisPoly(0),
		SavedPathSessionID(MAXINT){}
	FNavMeshPolyBase( UNavigationMeshBase* Mesh, const TArray<VERTID>& inPolyIndices, FLOAT PolyHeight);
	~FNavMeshPolyBase();

	void DrawPoly( ULineBatchComponent* LineBatcher, FColor C, FVector DrawOffset=FVector(0,0,0) );
	void DrawPoly( FDebugRenderSceneProxy* DRSP, FColor C, FVector DrawOffset=FVector(0,0,0) );
	void DrawSolidPoly( FDynamicMeshBuilder& MeshBuilder );

	void DebugLogVerts( TCHAR* PolyName );

	/**
	 * given the passed point, will adjust the point to the passed height obove this poly (in a cardinal direction)
	 * @param out_Pt - point to adjust
	 * @param DesiredHeight - desired offset from poly
	 */
	void AdjustPositionToDesiredHeightAbovePoly(FVector& out_Pt, FLOAT DesiredHeight,UBOOL bWorldSpace=WORLD_SPACE);

	/**
	 *	called after vertexes for this polygon change and poly params need to be recalculated (e.g. norm, center, etc)
	 * @param Direction to be considered Up (optional, use normal if none specified)
	 */
	void RecalcAfterVertChange(FVector* PolyUp=NULL);

#define DEFAULT_EPSILON 1.0f
	/**
	 * uses crossings test to determine of a point is within a poly
	 * @param point		- the point to check
	 * @param bWorldSpace - whether or not the incoming point is in local space or world space
	 * @param Epsilon	- threshold value to use for comparison operations
	 * @return UBOOL    - whether or not the passed point is within this polygon
	 */
	UBOOL ContainsPoint( const FVector& Point, UBOOL bWorldSpace=LOCAL_SPACE, FLOAT Epsilon=DEFAULT_EPSILON ) const;

#define DEFAULT_BOX_PADDING 10.f
	/**
	 * determines if the passed FBox should be considered 'in' this poly (e.g. are we inside the poly's bounds, and close to its surface)
	 * @param Box			- box to test
	 * @param bWorldSpace	- what frame the incoming box i sin
	 * @return UBOOL		- TRUE if this poly is valid for the passed box
	 */
	UBOOL ContainsBox( const FBox& Box, UBOOL bWorldSpace=WORLD_SPACE, FLOAT BoxPadding=DEFAULT_BOX_PADDING) const;

	/**
	* determines if the passed point is within this poly or not
	* STATIC version which takes a list of vectors as the poly we're checking against
	* @param PolyVerts  - verts of the poly we're testing
	* @param point		- the point to check
	* @return UBOOL    - whether or not the passed point is within this polygon
	* @NOTE - does not expand poly like non-static version
	*/
	static UBOOL ContainsPoint( const TArray<FVector>& PolyVerts, const FVector& Point );

	/**
	 *	@return TRUE if this is a "border" poly (e.g. a poly on the boundary of exploration)
	 */
	UBOOL IsBorderPoly();

	/**
	 * Sets this polygon as a border (or exploration boundary) polygon, and maintains adjacent border-ness
	 *	@param bBorderPoly - TRUE if this should be marked as a border poly
	 *  @param AdjacentPolys - list of polys adjacent to this one
	 */
	void  SetBorderPoly(UBOOL bBorderPoly, TArray<FNavMeshPolyBase*>* AdjacentPolys=NULL);
	
	/**
	 *	returns a list of polys adjacent to this one which are also border polys
	 * @param OutPolys - out param filled with adjacent border polys
	 */
	void GetAdjacentBorderPolys(TArray<FNavMeshPolyBase*>& OutPolys);

	/**
	 *	returns a list of polys adjacent to this one (only valid during builds)
	 * @param OutPolys - out param filled with adjacent polys
	 */
	void GetAdjacentPolys(TArray<FNavMeshPolyBase*>& OutPolys);

	/**
	 * given an ideal cylinder location and radius, will fit that cylinder in this polygon as best it can and return the new position
	 * @param CylIdealLoc - ideal location for cylinder
	 * @param CylRadius   - radius of cylinder
	 * @param CylHalfHeight - half the height of the cylinder
	 * @param out_BestLoc - the best-fit location for the cylinder
	 * @param bForceInteriorPosition - when FALSE, if the ideal position is within the poly, and not colliding with the obstacl mesh
	                                   that position will be used.  Set to TRUE if you're looking for a position which allows the cylinder
									   to be fully within the polygon
	* @return - whether or not a suitable position was found
	*/
	UBOOL GetBestLocationForCyl(const FVector& CylIdealLoc, FLOAT CylRadius, FLOAT CylHalfHeight, FVector& out_BestLoc, UBOOL bForceInteriorPosition=FALSE);

	/**
	 * Returns the closest point to TestPt that is on this polygon
	 * Note: is not quite the same as canonical closestptonpoly because our definition of 'in' is that of
	 *       of a volume described by planes at the edges of the polygon, going in the direction of the edge normal, not the poly normal
	 * @param TestPt - point to find the closest point on this poly to
	 * @param bWorldSpace - the vector space the incoming point is in (and that the outgoing point should be in)
	 * @return the closest point
	 */
	FVector GetClosestPointOnPoly( const FVector& TestPt, UBOOL bWorldSpace=TRUE );

	/** 
	 * Finds the pt on a given segment that crosses an boundary of this poly 
	 * @param S1 - point one of segment
	 * @Param S2 - point two of segment
	 * @param out_Intersection - out param filled with intersection point
	 * @return TRUE if there was an intersection 
	 */
	UBOOL GetBoundaryIntersection( FVector& S1, FVector& S2, FVector& out_Intersection );

	/** 
	 * Get local index into PolyVerts adjacent to given local index 
	 * @param LocalVertIdx - index in poly's vert array of vertex we're interested in
	 * @param Dir - direction to get adjacent vert in (forward/bwd)
	 * @return local index adjacent to the passed local index in the direction specified
	 */
	INT GetAdjacentVertexIndex( INT LocalVertIdx, INT Dir );

	/** 
	 * Get vertex pool index adjacent to given local index 
	 * @param LocalVertIdx - index in poly's vert array of vertex we're interested in
	 * @param Dir - direction to get adjacent vert in (forward/bwd)
	 * @return - mesh's vertex ID of adjacent vert
	 */
	VERTID GetAdjacentVertPoolIndex( INT LocalVertsIdx, INT Dir );

	/** 
	 * Gets vert location for given local index 
	 * @param LocalVertIdx - local index of vert you want location of
	 * @param bWOrldSpace - TRUE if you want the location in world space
	 * @return the location of the vert
	 */
	FVector GetVertLocation( INT LocalVertIdx, UBOOL bWorldSpace=WORLD_SPACE ) const;
	
	/**
	 * finds an edge in this poly's edge list which points back to the destination poly
	 * @param DestPoly - Poly to find an edge to
	 * @param bAllowTopLevelEdgesWhenSubMeshPresent - whether or not we want to allow top level edges that link to polys with obstacles 
	 * @return - the edge found NULL of none was found
	 */
	FNavMeshEdgeBase* GetEdgeTo( FNavMeshPolyBase* DestPoly, UBOOL bAllowTopLevelEdgesWhenSubMeshPresent=FALSE );
	
	/** @returns the number of edges this poly has (including cross pylon edges) */
	INT GetNumEdges();
	
	/**
	 * @@WARNING! - this may return NULL if the edge you requested is currently invalid 
	 * returns the edge at the given index (Cross Pylon edges start after the last normal edge idx)
	 * @param Idx - local (to this poly) index of the edge requested
	 * @param MeshOverride - Mesh to grab edges from, using this poly's index info (used to get linked edges from the obstacle mesh)
	 *                       default indicates to use this poly's mesh
	 * @param bAllowTopLevelEdgesWhenSubMeshPresent - when TRUE edges which have been superceded by submesh edges will still be returned 
	 * @return - the Edge requested
	*/
	FNavMeshEdgeBase* GetEdgeFromIdx(INT Idx,UNavigationMeshBase* MeshOverride=NULL, UBOOL bAllowTopLevelEdgesWhenSubMeshPresent=FALSE);

	/**
	 * UpdateDynamicObstaclesForEdge
	 * - this function will ensure that any updates that need to be made are made for the edge at the given Idx
	 * @param SessionID - the path session ID for the currently running path (used to determine if we've already checked this edge's poly or not)
	 * @param OtherPoly - other poly of the edge (local to this poly
	 */
	void UpdateDynamicObstaclesForEdge(INT SessionID, FNavMeshPolyBase* OtherPoly);
	
	/**
	 * sets the cached centroid fo this poly
	 * @param ctr - new poly center
	 */	 
	void SetPolyCenter(FVector Ctr)
	{
		PolyCenter = Ctr;
	}

	/**
	 *	Returns the cached normal of this poly
	 * @param bWorldSpace - true if you want the normal in world space
	 * @return the normal
	 */
	FVector GetPolyNormal(UBOOL bWorldSpace=WORLD_SPACE) const;

	/**
	 *	GetPolyCenter get poly centroid
	 * @param bWorldSpace - TRUE if you want the centroid in world space
	 * @return - the poly centroid
	 */
	FVector GetPolyCenter(UBOOL bWorldSpace=WORLD_SPACE) const;

	/**
	 *	return the bounds of this polygon 
	 * @param bWorldSpace - TRUE if you want the bounds in world space
	 * @return FBox representing the bounds of htis poly
	 */
	FBox GetPolyBounds(UBOOL bWorldSpace=WORLD_SPACE) const;

	/**
	 * resets this poly's verts, removes all claims to them
	 */
	void ClearVerts();
	
	/**
	 * Adds the passed verts to a polygon (to be used on empty polygons)
	 * @param inVertIndices - vertex IDs to be added to this poly
	 */
	void AddVerts(const TArray<VERTID>& inVertIndices);
	
	/**
	 * returns the area of this polygon
	 */
	FLOAT CalcArea();

	/**
	 * takes an input list of vert IDs and calculates the area of the polygon they represent
	 * @param Verts - vert IDs to calc area for
	 * @param NavMesh - the navigation mesh these vert IDs are from
	 * @return - the area of the polygon
	 */
	static FLOAT CalcArea(const TArray<VERTID>& Verts, UNavigationMeshBase* NavMesh);

	/**
	* takes an input list of vert locations and calculates the area of the polygon they represent
	* @param Verts - the vert locations to calculate area for
	* @return - the area of the polygon
	*/
	static FLOAT CalcArea(const TArray<FVector>& Verts);

	/**
	 * calculates the normal for this poly based on its verts
	 * @param bWorldSpace - what frame of reference the poly should be returned in
	 * @return - the calculated normal
	 */
	FVector CalcNormal(UBOOL bWorldSpace=WORLD_SPACE);

	/**
	 * given a list of vertIDs..
	 * calculates the normal for this poly based on its verts
	 * @param Verts - the array of vertIDs to use to calculate the normal
	 * @param NavMesh - the navmesh which owns the verts we hare getting IDs for
	 * @param bWorldSpace - what frame of reference the poly should be returned in
	 * @return - the calculated normal
	 */
	static FVector CalcNormal(const TArray<VERTID>& Verts, UNavigationMeshBase* NavMesh, UBOOL bWorldSpace=LOCAL_SPACE);
	/**
	 * calculate normal of the given polygon based on world space vertex locations
	 * @param VertLocs - Locations of verts to use to calc normal
	 * @return - normal of poly
	 */
	static FVector CalcNormal(const TArray<FVector>& VertLocs );
	
	/**
	*	calculate the centroid of this polygon and return it
	* @param bWorldSpace
	* @return the calc'd centroid 
	*/
	FVector CalcCenter(UBOOL bWorldSpace=LOCAL_SPACE);

	/**
	*	Calculate the centroid of the polygon represented by the passed VERTIDs/mesh
	* @param Verts - IDs of verts to use for calc
	* @param NavMesh - mesh where the verts live
	* @param bWorldSpace - TRUE if center should be in  world space
	*/
	static FVector CalcCenter(const TArray<VERTID>& Verts, UNavigationMeshBase* NavMesh,UBOOL bWorldSpace=LOCAL_SPACE);

	/** @returns the height of this poly from the poly bounds */
	FLOAT GetPolyHeight();

	/** 
	 * Adds a cover reference to this polygon
	 * @param Ref - the cover reference to add
	 */
	void AddCoverReference(const FCoverReference& Ref);

	/**
	 * looks for the passed cover reference in this poly's poly cover array
	 * @param Ref - the cover reference to remove
	 * @return - whether the passed reference was found, and removed
	 */
	UBOOL RemoveCoverReference(const FCoverReference& Ref);
	/**
	 * removes the cover reference at the passed index
	 * @param Idx - the index of the cover reference to remove
	 */
	void RemoveCoverReference(INT Idx);

	/**
	 * will remove any cover refs that are tied to the passed cover link
	 *@param Link - any slots referenced by this poly that belong to the passed link will get removed
	 */
	void RemoveAllCoverReferencesToCoverLink(ACoverLink* Link);

	/**
	 *	RemoveVertexAtLocalIdx
	 *  -removes a vertex from this polygon (based on the local index of the vert)
	 * @param LocalIdx - local index of the vert to remove
	 * @param bDontRemoveFromList - when TRUE the vert will not be removed from the poly list, but this poly will be removed from its containingpoly list if needed
	 */
	void RemoveVertexAtLocalIdx(INT LocalIdx, UBOOL bDontRemoveFromList=FALSE);

	/**
	 *	RemoveVertex
	 *  - will remove ALL instances of the passed VERTID from this polygon
	 *  @param VertexID - VERTID of the vertex we should remove from this poly
	 */
	void RemoveVertex(VERTID VertexID);

	/**
	 * uses SAT to determine if the passed set of points intersects this poly
	 * @param OtherPolyVerts	-	Vertex locations representing poly to check collision against
	 * @param bWorldSpace		-	What reference frame the incoming polys are in
	 * @return whehter or not the passed poly collides with this one
	 */
	UBOOL IntersectsPoly(const TArray<FVector>& OtherPolyVerts, UBOOL bWorldSpace=LOCAL_SPACE, const FLOAT ExpandDist=0.f);

	/**
	* uses SAT to determine if the passed poly intersects this one
	* @param Poly	-	the polygoin to check collision against
	* @param bWorldSpace - should be TRUE if the incoming poly is in world space
	* @param ExpandDist - fudge factor to expand the passed poly by 
	* @return whehter or not the passed poly collides with this one
	*/
	UBOOL IntersectsPoly(FNavMeshPolyBase* Poly, UBOOL bWorldSpace=LOCAL_SPACE, const FLOAT ExpandDist=0.f);

	
	/**
	 * determines if the passed line segment intersects with this polygon (in 2D)
	 * @param SegPt0		 - starting point for line segment
	 * @param SegPt1		 - end point for line segment
	 * @param out_EntryPoint - the point at which the line segment enters the polygon (if any)
	 * @param out_ExitPoint  - the point at which the line segment leaves the polygon (if any)
	 * @param bWorldSpace	 - whether the input and output should be in wordlspace or not
	 * @return				 - whether or not the segment intersects with this polygon
	 */
	UBOOL IntersectsPoly2D(const FVector& SegPt0, const FVector& SegPt1, FVector& out_EntryPoint, FVector& out_ExitPoint,UBOOL bWorldSpace=LOCAL_SPACE);

	/**
	 * static version of IntersectsPoly2D which takes an array of points rather than using this' polyverts
	 * (assumes everything is in the same space)
	 * @param Poly			 - array of points representing the poly
	 * @param SegPt0		 - starting point for line segment
	 * @param SegPt1		 - end point for line segment
	 * @param out_EntryPoint - the point at which the line segment enters the polygon (if any)
	 * @param out_ExitPoint  - the point at which the line segment leaves the polygon (if any)
	 * @return				 - whether or not the segment intersects with this polygon
	 */
	static UBOOL IntersectsPoly2D(const TArray<FVector>& Poly, const FVector& SegPt0, const FVector& SegPt1, FVector& out_EntryPoint, FVector& out_ExitPoint, FVector PolyNormal=FVector(0.f));

	/** sets TransientCost on this poly, adding it to the list to clear later */
	FORCEINLINE void SetTransientCost(INT NewCost)
	{
		TransientCost = NewCost;
		if (TransientCost != 0)
		{
			// AddItem() here optimizes for the most common case (only set once per pathfind)
			TransientCostedPolys.AddItem(this);
		}
		else
		{
			TransientCostedPolys.RemoveItem(this);
		}
	}
	FORCEINLINE INT GetTransientCost()
	{
		return TransientCost;
	}

	/** called to reset TransientCost on all entries in the TransientCostedPolys list */
	static void ClearTransientCosts();
	
	/**
	 * Helper function which will return obstacle info for this poly (if any)
	 * @return - the found obstacle info (NULL if none found)
	 */
	struct FPolyObstacleInfo* GetObstacleInfo();

	/**
	 * GetSubMesh
	 * - returns the submesh for this poly if one exists
	 * @return - the submesh for this poly (NULL of none)
	 */
	UNavigationMeshBase* GetSubMesh();

	/**
	 * @returns TRUE if this poly is within a submesh
	 */
	UBOOL IsSubMeshPoly();

	/**
	 * returns the parent poly for this poly (NULL if this poly is not in a submesh)
	 */
	FNavMeshPolyBase* GetParentPoly();

	/**
	 * returns an FPlane that represents the plane of the surface of this poly
	 * @param bWorldSpace - whether the resulting plane should be in worldspace or not
	 * @return FPlane of this poly
	 */
	FPlane GetPolyPlane(UBOOL bWorldSpace=TRUE) const
	{
		return FPlane(GetPolyCenter(bWorldSpace),GetPolyNormal(bWorldSpace));
	}

	friend FArchive& operator<<( FArchive& Ar, FNavMeshPolyBase& T )
	{
		Ar << T.PolyVerts;
		Ar << T.PolyEdges;
		Ar << T.PolyCenter;
		Ar << T.PolyNormal;
		Ar << T.BoxBounds;

		if( Ar.Ver() >= VER_NAVMESH_COVERREF )
		{
			Ar << T.PolyCover;
		}

		if( Ar.Ver() >= VER_NAVMESH_POLYHEIGHT)
		{
			Ar << T.PolyHeight;
		}
		else if(Ar.IsLoading())
		{
			T.PolyHeight = 0.f;
		}

		return Ar;
	}

	void DebugLog( INT Idx );

	/**
	 * @param PathParams - path params representing the AI we're asking about
	 * @returns TRUE if this poly is escapable by the entity represented by the passed pathparams
	 */
	UBOOL IsEscapableBy( const FNavMeshPathParams& PathParams );

	/**
	 * called by APylon::GetActorReferences, allows each poly to supply its own refs
	 */
	void GetActorReferences(TArray<FActorReference*>& ActorRefs, UBOOL bIsRemovingLevel);
};

#include "UnkDOP.h"

typedef TkDOPTree<class FNavMeshCollisionDataProvider,WORD>	NavMeshKDOPTree;
typedef TkDOP<class FNavMeshCollisionDataProvider,WORD>	NavMeshKDOP;



template<typename,typename> class TOctree;

typedef TOctree<struct FNavMeshPolyBase*, struct FNavPolyOctreeSemantics> FPolyOctreeType;

typedef TMultiMap<FMeshVertex,VERTID> FVertHash;

// Add cover association to nav mesh polys 
#define VER_VERNUM_COVERASSOC	1
// 10-29-2008 Added Obstacle Mesh
#define VER_VERNUM_OBSTACLEMESH 2
// 1-26-2009 changed relactor in specialmove edge to be actor reference
#define VER_VERNUM_RELACTORACTORREF 3
// 1-30-2009 fixed poly refs not being serialized in cross pylon edges
#define VER_CROSSPYLONEDGESERIALIZATION 4
// 2-10-2009 added poly height var to polygons
#define VER_POLYHEIGHT 5
// 2-16-2009 made all special move edges cross-pylon
#define VER_SPECIALMOVEEDGES_CROSS_PYLON 6
// 3-10-2009 removed baseactor reference
#define VER_REMOVED_BASEACTOR 7
// 3-16-2009 serialize starting transforms
#define VER_SERIALIZE_TRANSFORMS 8
// 3-17-2009 serialize border edge list
#define VER_SERIALIZE_BORDEREDGELIST 9
// 6/11/2009 
#define VER_EFFECTIVE_EDGE_LEN 10
// 6/23/2009 - added pathdata version number
#define VER_PATHDATA_VERSION 11
// 7/13/2009 - add bounds for meshes
#define VER_MESH_BOUNDS 12 
// 9/11/2009 - fixed divide by 0 in mantle edge creation causing NAN verts to get added
#define VER_FIXED_DIVBYZERO 13
// 10/6/2009 - fixed dropheight not getting serialized
#define VER_DROPHEIGHT_SERIALIZATION 14
// 01/27/2010 - navmeshes now are simplified with an algorithm that results in less polys
#define VER_BETTER_POLY_SIMPLIFICATION 15
// 3/2/2010   - now support 3D scale, so drawscale is used.. set drawscale to 1 if it was set before
#define VER_PYLON_DRAWSCALE 16
// 3/16/2010  - fixed issues causing no edge linkage
#define VER_EDGELINKAGE_FIX 17
// 3/18/2010  - fixed isconvex,polymerge,squaremerge
#define VER_MESHGEN_FIXES 18
// 5/10/2010  - fixed specialmoveedge relactor serialization
#define VER_SPECIALMOVEEDGE_FIX 19
// 6/24/2010  - fixed jumplinks 
#define VER_JUMPLINK_FIX 20
// 6/25/2010  - fixed mantle generation, want paths rebuilt on ALL
#define VER_MANTLE_FIX 21
// 8/12/2010 - fixed coverslots just outside of navmesh not registering
#define VER_COVERSLOT_FIX 22
// 8/26/2010 - enforced max spacing between cover slots
#define VER_COVERSLOT_SPACING_FIX 23
// 8/30/2010 - setcollisionforpathfinding fix
#define VER_PATHCOLLISION_FIX 24
// 9/7/2010 - second path collision drop 
#define VER_PATHCOLLISION_FIX2 25
// 9/22/2010 - third path collision drop 
#define VER_PATHCOLLISION_FIX3 26
// 9/28/2010 - removed edgeverts array
#define VER_REMOVED_EDGEVERTS_ARRAY 27
// 10/13/2010 - added turret exposure shizzle
#define VER_TURRET_COVER_EXPOSURE 28
// 12/1/2010 - switched to generated edge widths
#define VER_EDGE_WIDTH_GENERATION 29
// 1/14/2011 - added edge groups
#define VER_EDGE_GROUPS 30
// 1/14/2011 - serialize edge obstaclepolyID
#define VER_SERIALIZE_OBSTACLEPOLYID 31
// 1/26/2011 - removed edgelength
#define VER_REMOVED_EDGELENGTH 32
// 1/31/2011 - fixed cross-pylon edge generation
#define VER_FIXED_CROSS_PYLON_EDGES 33
// 2/2/2011 - fixed edges near cross pylon edges not getting generated correctly
#define VER_FIXED_NEAR_CROSS_PYLON 34
// 2/18/2011 - fourth path collision drop 
#define VER_PATHCOLLISION_FIX4 35
// 2/18/2011 - changed to A* on edges instead of polys
#define VER_ASTAR_WITH_EDGES 36
// 2/19/2011 - fixed borderedgesegments getting bad data
#define VER_FIXED_BORDER_EDGESEGS 37
// 3/22/2011 - combatzone poly linkage
#define VER_COMBATZONE_POLY_LINKAGE 38
// 4/25/2011 - fixed combatzone poly map centroid and clearing issues
#define VER_COMBATZONE_FIX 39
// 5/12/2011 - changed combatzones to use offline pathobject functionality
#define VER_PATHOBJECT_IMPROVEMENT 40
// 5/17/2011 - cached edge perp 
#define VER_EDGEPERP 41
// 5/22/2011 - fixed mantle edges linking into GDOs
#define VER_FIXED_GDO_MANTLEEDGE 42
// 6/9/2011 - fixed GDOs not reporting correct poly height
#define VER_FIXED_GDO_HEIGHT 43

#define VER_LATEST_NAVMESH		VER_FIXED_GDO_HEIGHT

// if a navmesh is loaded which was generated with a version less than this, PATHS NEED TO BE REBUILT warnings will let fly
#define VER_MIN_PATHING			VER_FIXED_GDO_HEIGHT


// struct that contains the necessary info for a particular edge (data, and type info)
struct FEdgeStorageDatum
{
	FEdgeStorageDatum(DWORD Offset, WORD Size, FName Name) : DataPtrOffset(Offset), DataSize(Size), ClassName(Name)
	{}

	FEdgeStorageDatum()
		: DataPtrOffset(0)
		  ,DataSize(0)
		  ,ClassName(FName(NAME_None))
	{}

	// offset from the start of the data buffer
	DWORD DataPtrOffset;
	// byte count for this edge
	WORD   DataSize;
	// classname of this edge
	FName ClassName;
	friend FArchive& operator<<( FArchive& Ar, FEdgeStorageDatum& T )
	{
		Ar << T.DataPtrOffset;
		Ar << T.DataSize;
		Ar << T.ClassName;
		return Ar;
	}

};

typedef TArray< class IInterface_NavMeshPathObstacle* > PolyObstacleList;

// struct used to store information the status of a particular polygon in the navmesh
// in relation to dynamic obstacles that are on it
struct FPolyObstacleInfo
{
	FPolyObstacleInfo(FNavMeshPolyBase* InPoly) : bNeedRecompute(FALSE)
						  ,LinkedObstacles()
						  ,SubMesh(NULL)
						  ,Poly(InPoly)	
						  ,bAffectsDynamicObstacleMesh(TRUE)
	{}

	/** TRUE if this polygon is dirty and needs to be recomputed prior to any pathing on it */
	UBOOL bNeedRecompute;

	/** list of navmeshpathobstacles which are associated with this polygon (which overlap it) */
	PolyObstacleList LinkedObstacles;

	/** pointer to the mesh that has been created in place to avoid the obstacles associated with this poly */
	UNavigationMeshBase* SubMesh;

	/** pointer to the polygon this obstacle info is for */
	FNavMeshPolyBase* Poly;

	/** a list of polys in the obstacle mesh which were added for this submesh (and should be removed when this submesh is rebuilt) */
	PolyList ObstacleMeshPolys;

	/** bool indicating whether this poly's obstacle configuration affects the dynamic obstacle mesh (used for early outing of building obstacle mesh when not needed) */
	UBOOL bAffectsDynamicObstacleMesh;

	/**
	 * this will add the passed obstacle to the list of obstacles that are affecting the poly
	 * attached to this ObstacleInfo
	 * Note: will mark both the poly and the mesh as needing obstacle rebuilds
	 * @param Obst - the interface of the obstacle which is affecting this poly
	 */
	void AddLinkedObstacle(IInterface_NavMeshPathObstacle* Obst);

	/**
	 * this removes association of the passed obstacle with the poly attached to this obstacle info
	 * Note: will mark both the poly and the mesh as needing obstacle rebuilds
	 * @param Obst - the obstacle to remove
	 */
	void RemoveLinkedObstacle(IInterface_NavMeshPathObstacle* Obst);

	/**
	 * MarkNeedsRebuild
	 * marks this info as needing to be rebuilt
	 */
	void MarkNeedsRebuild();
	
};
typedef TMap< WORD, FPolyObstacleInfo > PolyObstacleInfoList;

typedef DWORD(*FNavMeshEdgeCtor)(TArray<BYTE>& Buffer);
typedef TMultiMap<WORD, FNavMeshCrossPylonEdge*> DynamicEdgeList;
typedef TDoubleLinkedList<FNavMeshCrossPylonEdge*> IncomingEdgeListType;
// TMap doesn't handle values being functors, wrap it in a struct
struct CtorContainer
{
	CtorContainer(FNavMeshEdgeCtor InCtor) : Ctor(InCtor) {	}
	FNavMeshEdgeCtor Ctor;
};

typedef TMap<FNavMeshPolyBase*, IInterface_NavMeshPathObstacle*> PolyToObstacleMap;

/**
 * UNavigationMeshBase - the primary container of the mesh itself.  
 * Meshes are used for multiple purposes (obstaclemesh/navigationmesh/etc.)
 * Meshes contain a pool of verts, a pool of polys, and a pool of edges that 
 * represent walkable connections between polys 
 */
class UNavigationMeshBase : public UObject
{
	DECLARE_CLASS_INTRINSIC(UNavigationMeshBase,UObject,0,Engine)

public:
	/** map of edgeclass names to edge class constructors */
	static TMap<FName, CtorContainer> GEdgeNameCtorMap;

	/** Pool of verts */
	TArray<FMeshVertex>			Verts;

	/** high water mark for static (saved) verts in the pool */
	INT							StaticVertCount;
	
protected:

	// array of edgestoragedata the stores pointer into data buffer
	TArray<FEdgeStorageDatum>    EdgeStorageData;
	// actual edge data
	TArray<BYTE>				EdgeDataBuffer;



public:
	/** flushed storage data after it's no longer needed */
	void FlushEdgeStorageData() { EdgeStorageData.Empty(); }
	/** @returns allocated size of edge storage data */
	DWORD GetAllocatedEdgeStorageDataSize(){ return EdgeStorageData.GetAllocatedSize(); }
	/** @returns the number of allocated bytes for the current edge buffer */
	DWORD GetAllocatedEdgeBufferSize(){ return EdgeDataBuffer.GetAllocatedSize(); }
	
	/**  mesh that represents 'drop down' space (will contain all polys which would have been valid using maxdropheight instead of maxstepheight) */
	UNavigationMeshBase* DropEdgeMesh;

	/**
	 * this function will store the incoming edge in the buffer and set it up for serialization
	 * @param NewEdge - the temporary edge to store
	 * @param out_EdgeIdx - the index the edge was added at 
	 * @return - the typed new edge at its final location
	 */
	template< typename T> T* AddEdgeData(T& NewEdge, WORD& out_EdgeIdx);

	/**  array of pointers to edges (point back to edge data buffer) */
	TArray<FNavMeshEdgeBase*>	EdgePtrs;

	/** array of pointers to cross-pylon edges (data for these is still in EdgeDataBuffer, this is just for speedy iteration) */
	TArray<FNavMeshCrossPylonEdge*> CrossPylonEdges;

	/** USED DURING MESH GENERATION: list of polys on the heap used during mesh builds */
	PolyList BuildPolys;
	/** used in special circumstances where poly data is in the buildpolys list, but we need to index it quickly (e.g. dynamic obstacle mesh) */
	TArray<FNavMeshPolyBase*> BuildPolyIndexMap;

	/** USED AT RUNTIME: Pool of triangles - objects for runtime */
	TArray<FNavMeshPolyBase> Polys;

	/** USED DURING MESH GENERATION: List of nodes that are on the border of the mesh */
	PolyList BorderPolys;

	// dynamic edges are edges added at runtime, and are not serialized (they are transient)
	DynamicEdgeList DynamicEdges;
	
	// list of cross pylon edges that reference this mesh (for use when this mesh needs to be cleaned up)
	IncomingEdgeListType IncomingDynamicEdges;

	/** the path session ID this mesh was last updated using - indicates when we need to rebuild submeshes */
	INT SavedSessionID;

	/** this will be TRUE when dynamic obstacles need to be recalculated for at east one poly in this mesh */
	UBOOL bNeedsObstacleRecompute;

	/** when TRUE this mesh has been cleaned up and is ready for deletion */
	UBOOL bMeshHasBeenCleanedUp;

	// border edge segments are only used for dynamic meshes (meshes that move) and represent any edges of polys which
	// are on the border of the mesh.. These edges are what are checked against other meshes for purposes of dynamic
	// mesh stitching
	struct BorderEdgeInfo
	{
		BorderEdgeInfo() : Vert0(MAXWORD), Vert1(MAXWORD), Poly(MAXWORD) {}

		BorderEdgeInfo(VERTID InVert0, VERTID InVert1, WORD PolyIdx) : Vert0(InVert0), Vert1(InVert1), Poly(PolyIdx) {}
		VERTID Vert0;
		VERTID Vert1;
		WORD Poly;

		friend FArchive& operator<<( FArchive& Ar, BorderEdgeInfo& T )
		{
			Ar << T.Vert0;
			Ar << T.Vert1;
			Ar << T.Poly;
			return Ar;
		}

	};
	TArray<BorderEdgeInfo> BorderEdgeSegments;

	/** Map of polygons which are being affected by dynamic obstacles, and the info related */
	PolyObstacleInfoList PolyObstacleInfoMap;

	/** Map of submeshes to their parent poly (in this mesh) */
	TMap< UNavigationMeshBase*, WORD > SubMeshToParentPolyMap;

	/** Map of submesh polygon IDs to obstacles that the poly is internal to (e.g. geometry inside the obstacle) */
	PolyToObstacleMap SubMeshPolyIDToLinkeObstacleMap;

	/** bounds of this mesh (used for octree checks, etc)  */
	FBox	BoxBounds;

	/** indicates whether this mesh has a non-identity transform (and whether we need to transform or not) */
	UBOOL bNeedsTransform;

	/** cached version of the pylon that owns this mesh */
	APylon* OwningPylon;

	/**
	 * Build bounds for this navmesh
	 * - will build the bounds for this mesh based on the polys in it
	 */
	void BuildBounds();

	/**
	 * Mark an edge as being used by the passed NavigationHandle
	 * @param Edge - the edge which needs to be marked
	 * @param Handle - the handle using the edge
	 */
	void MarkEdgeAsActive(FNavMeshEdgeBase* Edge, UNavigationHandle* Handle);

	/**
	 * Removes a handle from an edge's list of actively using handles
	 * @param Edge - the edge which needs to be un-marked
	 * @param Handle - the handle using the edge
	 */
	 void UnMarkEdgeAsActive(FNavMeshEdgeBase* Edge, UNavigationHandle* Handle);

	 /** 
	  * adds the cross pylon edge that references this mesh to a list so we know when to clean it up
	  * @param EdgeThatRefs - the edge that is reffing this mesh we need to track
	  */
	  void NotifyEdgeRefOfMesh(FNavMeshCrossPylonEdge* EdgeThatRefs);

	  /**
	   * removes an entry in the reffing edge list (e.g. an edge in another mesh that used to be reffing us is being destroyed, so stop tracking it)
	   * @param EdgeThatUsedtoRef - the edge that used to ref this mesh that no longer does
	   */
	  void RemoveEdgeRefOfMesh(FNavMeshCrossPylonEdge* EdgeThatNoLongerRefs);

public:
	UNavigationMeshBase()
	{
		NavMeshVersionNum=VER_LATEST_NAVMESH;
		VersionAtGenerationTime=0;
		PolyOctree=NULL;
		VertHash=NULL;
		LocalToWorld = FMatrix::Identity;
		WorldToLocal = FMatrix::Identity;
		KDOPInitialized=FALSE;
		DropEdgeMesh=NULL;
		BoxBounds = FBox(0);
		SavedSessionID=MAXINT;
		bNeedsObstacleRecompute=FALSE;
		bMeshHasBeenCleanedUp=FALSE;
		bNeedsTransform=FALSE;
		OwningPylon=NULL;
	}

#if !FINAL_RELEASE

	/**
	* Determines if any connections in the nav mesh are not valid.
	* @return  UBOOL - TRUE if all connections are valid.
	*/
	UBOOL AreNavMeshConnectionsValid (void);

	/**
	* Determines is any connections defined in edges are not valid
	* @return  UBOOL - TRUE if all connections are valid
	*/
	UBOOL AreEdgesValid(void);

	/**
	* Determines if any connection defined in polys are not valid
	* @return  UBOOL - TRUE if all connections are valid
	*/
	UBOOL ArePolysValid(void);

	/**
	* Determines if any connections defined in vertices are not valid
	* @return  UBOOL
	*/
	UBOOL AreVerticesValid(void);
#endif
	/**
	 * @returns the pylon associated with this navmesh
	 */
	APylon* GetPylon();

	void InitTransform(APylon* OwningPylon)
	{
		LocalToWorld = OwningPylon->GetMeshLocalToWorld();
		WorldToLocal = OwningPylon->GetMeshWorldToLocal();
		bNeedsTransform = (!OwningPylon->IsStatic() || OwningPylon->bImportedMesh);
	}

	/**
	 * will do what's necessary to get the passed local space vector into worldspace
	 * @param V - vector to trasnform
	 * @return trasnformed vector
	 */
	FORCEINLINE FVector L2WTransformFVector(const FVector &V) const
	{
		if(! bNeedsTransform )
		{
			return V;
		}

		return LocalToWorld.TransformFVector(V);
	}

	/**
	 * will do what's necessary to get the passed world space vector into localspace
	 * @param V - vector to trasnform
	 * @return trasnformed vector
	 */
	FORCEINLINE FVector W2LTransformFVector(const FVector &V) const
	{
		if( !bNeedsTransform )
		{
			return V;
		}

		return WorldToLocal.TransformFVector(V);
	}

	/**
	 * will do what's necessary to get the passed local space normal into world space
	 * @param V - vector to trasnform
	 * @return trasnformed vector
	 */
	FORCEINLINE FVector L2WTransformNormal(const FVector &V) const
	{
		if( !bNeedsTransform )
		{
			return V;
		}

		return LocalToWorld.TransformNormal(V);
	}

	/**
	 * will do what's necessary to get the passed world space normal into localspace
	 * @param V - vector to trasnform
	 * @return trasnformed vector
	 */
	FORCEINLINE FVector W2LTransformNormal(const FVector &V) const
	{
		if( !bNeedsTransform )
		{
			return V;
		}

		return WorldToLocal.TransformNormal(V);
	}

	INT GetResourceSize();

	/**
	 *	@Returns TRUE if this mesh is an obstacle mesh, and not one for navigation
	 */
	UBOOL IsObstacleMesh() { return (GetPylon() && OwningPylon->ObstacleMesh==this);}

	/**
	 *	@Returns TRUE if this mesh is an obstacle mesh that was created at runtime
	 */
	UBOOL IsDynamicObstacleMesh() { return (GetPylon() && OwningPylon->DynamicObstacleMesh==this);}

	/**
	 *	@Returns TRUE if this mesh is a submesh of another higher level mesh
	 */
	UBOOL IsSubMesh() { return (GetPylon() && OwningPylon->NavMeshPtr != this && OwningPylon->ObstacleMesh != this && OwningPylon->DynamicObstacleMesh != this); }


	/**
	 *	@Returns the obstacle mesh associated with this mesh (could be this)
	 */
	UNavigationMeshBase* GetObstacleMesh() { return (IsObstacleMesh()) ? this : (GetPylon()) ? OwningPylon->ObstacleMesh : NULL; }

	// this will return the topmost mesh (if this is a submesh, get the parent, if not return self)
	UNavigationMeshBase* GetTopLevelMesh() {return (GetPylon()) ? OwningPylon->NavMeshPtr : NULL;}

	/** 
	* Add a dynamic (runtime) vert to the pool -- return the index of the new vert
	* Might be slow because we have no vert hash at this point, uses octree check
	* @param inV		  -	world space position of the new vertex to be added
	* @param bWorldSpace - (optional) if TRUE vert location will be treated as worldspace, defaults to TRUE
	* @return		-	the ID of the added (or already existing) vertex
	*/
	VERTID AddDynamicVert( const FVector& inV, UBOOL bWorldSpace=TRUE );

	/**
	 * this will search the vertex hash for a vert that is clsoe to the passed location
	 * @param InVec - location for new vert
	 * @param bWorldSpace - (optional) TRUE if the incoming vert is in world space
	 * @param MaxZDelta - (optional) max vertical distance between verts to be considered the 'same'
	 * @param bUseTopMatchingVert - (optional) when TRUE the vert that is highest will be used to break vertical ties
	 * @param MaxDelta - (optional) max distance between verts to be considered the 'same'
	 * @return VERTID of vert found (or -1 if none is found)
	 */
	VERTID FindVert( const FVector& inV, UBOOL bWorldSpace=TRUE, FLOAT MaxZDelta=-1.0f, UBOOL bUseTopMatchingVert=FALSE, FLOAT MaxDelta=-1.0f  );

	/**
	 *	this will add a new mesh vertex at the supplied location, or return the ID of one existing already which is close enough to be considered the same
	 * @param InVec - location for new vert
	 * @param bWorldSpace - (optional) TRUE if the incoming vert is in world space
	 * @param MaxZDelta - (optional) max vertical distance between verts to be considered the 'same'
	 * @param bUseTopMatchingVert - (optional) when TRUE the vert that is highest will be used to break vertical ties
	 * @param MaxDelta - (optional) max distance between verts to be considered the 'same'
	 * @return VERTID of either new vert or the vert which was found to be close enough
	 */
	VERTID AddVert( const FVector& inV, UBOOL bWorldSpace=TRUE, FLOAT MaxZDelta=-1.0f, UBOOL bUseTopMatchingVert=FALSE, FLOAT MaxDelta=-1.0f  );

	/**
	 * @param VertId      - the index of the vertex you want the location of
	 * @param bWorldSpace - if TRUE, value will be in world space, otherwise will be in local space
	 * @param bOneWay	  - TRUE if we should only add a link to the added edge from poly0->poly1, and not Poly1->Poly0
	 * @return - the location of the vertex
	 */
	FVector GetVertLocation( VERTID VertId, UBOOL bWorldSpace=TRUE) const;

	/** 
	 * Move vertex to new location and update vertex hash
	 * @param VertId      - index of moved vertex
	 * @param inV         - new location
	 * @param bWorldSpace - if true, new location is in world space
	 */
	void MoveVert( VERTID VertId, const FVector& inV, UBOOL bWorldSpace=TRUE);

	/** Add a edge to the pool -- return index to that edge
	 * @param inV1 - location of first vertex for the edge
	 * @param inV2 - location of second vertex for the edge
	 * @param SupporteEdgeWidth - the width of dudes this edge supports
	 * @param EdgeGroupID - ID that groups this edge with others that overlap it
	 * @param ConnectedPolys - polys this edge is going to connect (0 idx should be owning poly)
	 * @param out_EdgePtr - optional output param stuffed with the created edge
	 * @param bOneWay - should we only add this edge from source to dest?
	 * @param Vert0ID - vertexID to use when adding adding the edge (saves vertex ID lookup)
	 * @param Vert1ID - vertexID to use when adding adding the edge (saves vertex ID lookup)
	 * @return FALSE if we coudl not add verts for this edge (e.g. we ran out of vert IDs)
	 */
	template< typename T > UBOOL AddEdge( const FVector& inV1,
										const FVector& inV2,
										FLOAT SupportedEdgeWidth,
										BYTE EdgeGroupID,
										TArray<FNavMeshPolyBase*>* ConnectedPolys = NULL,
										T** out_EdgePtr = NULL,
										UBOOL bOneWay = FALSE,
										VERTID Vert0ID=MAXVERTID,
										VERTID Vert1ID=MAXVERTID );
	/**
	 *	adds an edge at the passed locations linking the connectingpolys together (to be used for edges that link between meshes)
	 *  @param inV1 - first vert of edge
	 *  @param inV2 - second vert of edge
	 *  @param ConnectedPolys - polys this edge connects
	 *  @param SupportedEdgeWidth - the width of the guy that this edge supports
	 *  @param EdgeGroupID - the ID of the edge group which this is a part of
	 */
	void AddCrossPylonEdge( const FVector& inV1, const FVector& inV2, TArray<FNavMeshPolyBase*>& ConnectedPolys, FLOAT SupportedEdgeWidth, BYTE EdgeGroupID );

	/**
	 * just like addcrosspylonedge except it does not try to check for existing edges,
	 * and it does not add an edge in both directions (useful for pathobject edges, other special edges, etc.)
	 * @param StartPt - start point of one way edge
	 * @param EndPt - end point of one way edge
	 * @param ConnectedPolys - array containing the polys that this edge links
	 * @param SupportedEdgeWidth - width this edge supports
	 * @param out_EdgePtrs - optional out param stuffed with the new edge
	 * @param bForce - whether to add an edge even if there is one similar in the same spot
	 * @param out_EdgeId - optional out param stuffed with ID of added edge
	 * @return TRUE if success
	 */
	template< class T > UBOOL AddOneWayCrossPylonEdgeToMesh(const FVector& StartPt,
															const FVector& EndPt,
															TArray<FNavMeshPolyBase*>& ConnectedPolys,
															FLOAT SupportedEdgeWidth=-1.0f,
															BYTE EdgeGroupID=MAXBYTE,
															T** out_EdgePtr=NULL,
															UBOOL bForce=FALSE,
															UBOOL bAddDummyRefEdge=TRUE,
															INT* out_EdgeId=NULL);

	/**
	 * adds a cross pylon edge between the two polys placed in 'ConnectedPolys'.. will add edge as dynamic, meaning it
	 * will not be serialized and will only be used for the current run
	 * @param inV1 - start point of edge
	 * @param inV2 - end point of edge
	 * @param ConnectedPolys - list of polys this edge connects (should be 2 in length)
	 * @param SupportedEdgeWidth - the size of guys this edge supports
	 * @param EdgeGroupID - ID that identifies this edge with the edge group it's in
	 * @param bOneWay - should be TRUE if we should add an edge only from Poly0 to Poly1 not vice versa
	 * @param CreatedEdges - optional parameter for out array stuffed with edges taht get created
	 * @param Poly0Vert0Idx - optional param indicating the vertex ID we should use to add the edge (to save us from looking it up)
	 * @param Poly0Vert1Idx	- optional param indicating the vertex ID we should use to add the edge (to save us from looking it up)
	 * @param Poly1Vert0Idx	- optional param indicating the vertex ID we should use to add the edge (to save us from looking it up)
	 * @param Poly1Vert1Idx	- optional param indicating the vertex ID we should use to add the edge (to save us from looking it up)
	 */
	template < typename T > void AddDynamicCrossPylonEdge( const FVector& inV1,
															const FVector& inV2,
															TArray<FNavMeshPolyBase*>& ConnectedPolys,
															FLOAT SupportedEdgeWidth,
															BYTE EdgeGroupID,
															UBOOL bOneWay=FALSE,
															TArray<T*>* CreatedEdges=NULL, 
															VERTID Poly0Vert0Idx=MAXVERTID, 
															VERTID Poly0Vert1Idx=MAXVERTID, 
															VERTID Poly1Vert0Idx=MAXVERTID, 
															VERTID Poly1Vert1Idx=MAXVERTID);

	/**
	 * removes a dynamic cross pylon edge from this mesh.  Also handles removing corresponding edges from other meshes
	 * @param Edge - the edge to be removed
	 */
	void RemoveDynamicCrossPylonEdge( FNavMeshCrossPylonEdge* Edge ); 

	/**
	 * called from RemoveDynamicCrossPylonEdge
	 * - is responsible for clearing out verts added after the staticvertcount barrier periodically
	 */
	void PruneDynamicVerts();

	/**
	 * this is for getting meshes to re-link themselves to the rest of the world after being moved at runtime
	 * this will wipe all dynamic edges, and create new ones where applicable
	 */
	void RebuildDynamicEdgeConnections();

	struct FEdgeRef
	{
		UNavigationMeshBase* Mesh;
		WORD EdgeId;
	};
protected:
	/**
	 * this function is called by RebuildDynamicEdgeConnections, and will add dynamic edge links from PylonA to PylonB
	 *    this function will move along the boundary of PylonA, adding edges where PylonA's outer boundary edges intersect
	 *    with PylonB's mesh
	 * @param PylonA - first pylon in pylon pair to be linked together
	 * @param PylonB - second pylon in pylon pair to be linked together
	 */
	 void CreateDynamicEdgesForPylonAtoPylonB(APylon* PylonA, APylon* PylonB);


	 /**
	  * looks for a dropedgemesh on this navmesh and will attempt to 
	  * create the proper dropdown edges based on the drop edge mesh
	  */
	 void BuildDropDownEdges(TArray<FEdgeRef>& out_AddedEdges);
public:

	/**
	 * Copy a poly from another mesh into this one
	 * @param Poly - other poly to copy
	 * @return - the new poly in this mesh
	 */
	FNavMeshPolyBase* CopyPolyIntoMesh(const FNavMeshPolyBase* Poly);
	/**
	 * Add a poly to the pool -- return that new poly 
	 * @param inVerts     - A list of FVectors representing this new polygon
	 * @param PolyHeight  - (optional) if non-negative, will be used as the height of this new poly, otherwise a height will be calculated 
	 * @param bWorldSpace - (optional) if TRUE, treats incoming vert locations as world space locations
	 * @param MaxVertSnapHeight - (optional) the max height that verts will be snapped together 
	 * @param MaxVertSnap - (optional) the max overall distance a vert can be snapped together (default is GRIDSIZE)
	 * @return - the poly just added
	 */
	FNavMeshPolyBase* AddPoly( const TArray<FVector>& inVerts, FLOAT PolyHeight=-1.f, UBOOL bWorldSpace=TRUE, FLOAT MaxVertSnapHeight=-1.0f, FLOAT MaxVertSnapDist=-1.0f );

	
	/**
	 *	will add a new polygon to the mesh based on a list of pre-existing vertex IDs
	 * @param inVertIndices - list of vert IDs to add from
	 * @param PolyHeight - height of poly (-1 means calculate height now)
	 * @return a ref to the new poly, or NULL if none was created
	 */
	FNavMeshPolyBase* AddPolyFromVertIndices( const TArray<VERTID>& inVertIndices, FLOAT PolyHeight);

	/** Remove a poly from the lists, return TRUE if successful */
	UBOOL RemovePoly( FNavMeshPolyBase* PolyForRemoval )
	{
		if(PolyForRemoval->BorderListNode != NULL)
		{
			BorderPolys.RemoveNode(PolyForRemoval->BorderListNode);
			PolyForRemoval->BorderListNode=NULL;
		}
		static TArray<FNavMeshPolyBase*> AdjacentPolys;
		
		if( !GIsGame ) // mt-> we don't care about border polys/verts at runtime and this is slow
		{
			AdjacentPolys.Reset(AdjacentPolys.Num());
			PolyForRemoval->GetAdjacentPolys(AdjacentPolys);
		}

		// unclaim verts
		for(INT VertIdx=0;VertIdx<PolyForRemoval->PolyVerts.Num();VertIdx++)
		{
			Verts(PolyForRemoval->PolyVerts(VertIdx)).ContainingPolys.RemoveItem(PolyForRemoval);
		}

		if ( !GIsGame )
		{
			PolyForRemoval->SetBorderPoly(FALSE,&AdjacentPolys);
		}

		BuildPolys.RemoveNode(PolyForRemoval);
		RemovePolyFromOctree(PolyForRemoval);
		delete PolyForRemoval;
		return TRUE;
	}

	// ***  edge accessors *** 
	
	/**
	 * return number of edges this mesh currently has
	 */
	WORD GetNumEdges()
	{
		return Max<INT>(EdgePtrs.Num(), EdgeStorageData.Num());
	}

	/**
	 * return the edge at the specified edge ID
	 */
	FNavMeshEdgeBase* GetEdgeAtIdx(WORD Idx)
	{
		const INT NumEdgePtrs = EdgePtrs.Num();
		const INT NumStorangeData = EdgeStorageData.Num();
		if(NumEdgePtrs > 0 && (NumEdgePtrs == NumStorangeData || NumStorangeData == 0))
		{
			return EdgePtrs(Idx);
		}
		else
		{
			// if we have more than 0 in the edgeptrs array it means we're cached but out of sync, regen
			if( EdgePtrs.Num() > 0 )
			{
				PopulateEdgePtrCache();
			}

			checkSlowish(EdgeStorageData.Num() > 0 && EdgeDataBuffer.Num() > 0);
			FEdgeStorageDatum& StorageDatum = EdgeStorageData(Idx);
			return (FNavMeshEdgeBase*)(&EdgeDataBuffer(0) + StorageDatum.DataPtrOffset);
		}
	}

	// called to initialize class constructors for edges
	static void InitializeEdgeClasses();

	// this function will construct new emtpy edges of the proper types according to the edge data
	void ConstructLoadedEdges();

	// this function will add cached ptrs to edges to the edgePtrs array (and do whatever other caching needs to be done)
	void PopulateEdgePtrCache();

	void FlushEdges()
	{
		// destruct edges 
		for(INT EdgeIdx=0;EdgeIdx<GetNumEdges();++EdgeIdx)
		{
			FNavMeshEdgeBase* Edge = GetEdgeAtIdx(EdgeIdx);
			Edge->~FNavMeshEdgeBase();
		}

		EdgeDataBuffer.Empty();
		EdgePtrs.Empty();
		EdgeStorageData.Empty();
		CrossPylonEdges.Empty();

		for( INT PolyIdx=0;PolyIdx<Polys.Num();++PolyIdx)
		{
			FNavMeshPolyBase* CurPoly = &Polys(PolyIdx);
			CurPoly->PolyEdges.Empty();
		}
	}

	/** 
	* function to delete and clean up dynamically added edges 
	*/
	void FlushDynamicEdges();


	/**
	 * GetPolySegmentSpanList
	 * - Will search only polys in this mesh, and return a list of spans of polys which intersect the line segment given
	 * @param Start - start of span
	 * @param End   - end of span
	 * @param out_Spans - out array of spans populated with intersecting poly spans
	 * @param bWorldSpace - reference frame incoming params are in
	 * @param bIgnoreDynamic - whether or not to ignore dynamic (sub) meshes (don't use this unless you know what you're doing.. :) Typically If there are dynamci polys around, they should be used over the static ones
	 * @param bReturnBothDynamicAndStatic - if TRUE, BOTH dynamic and static polys will be returned.. using this is *DANGEROUS*! most of the time you should use dynamic polys if they exist
	 *                                      as they are the 'correct' representation of the mesh at that point
	 */
	void GetPolySegmentSpanList(const FVector& Start, const FVector& End, TArray<struct FPolySegmentSpan>& out_Spans, UBOOL bWorldSpace=WORLD_SPACE,UBOOL bIgnoreDynamic=FALSE, UBOOL bReturnBothDynamicAndStatic=FALSE);

	/**
	 * ContainsPointOnBorder
	 *  returns TRUE if the point passed is within the border polys of this mesh
	 * @param Point - point to check for border-ness
	 * @return TRUE if point is on border
	 */	
	UBOOL ContainsPointOnBorder(const FVector& Point);

	/** 
	 * ContainsPoint
	 *  Tests whether the passed point is in this mesh
	 * @param Point - point to test
	 * @return TRUE if the point passed is within the border polys of this mesh
	 */
	UBOOL ContainsPoint(const FVector& Point);

	/**
	 * IntersectsEdge
	 *  tests to see if this mesh intersects with the passed edge
	 * @param Start - start of edge to test
	 * @param End - end of edge to test
	 * @param bWorldSpace - reference frame of incoming edge
	 * @return TRUE If edge intersects
	 */
	UBOOL IntersectsEdge(const FVector& Start, const FVector& End, UBOOL bWorldSpace=WORLD_SPACE);

	/**
	 * IntersectsPoly
	 *  tests to see if the passed poly intersects with this mesh
	 * @param Poly - array of vertIDs which describe the incoming poly 
	 * @param TraceFlags - flas for trace dilineation 
	 * @return TRUE if the poly described by the list of verts intersects the mesh
	 */
	UBOOL IntersectsPoly(TArray<VERTID>& Poly, DWORD TraceFlags=0);

	/**
	 * IntersectsPoly
	 *  tests to see if the poly described by the passed vert locations intersects with this mesh
	 * @param InPolyVertLocs - array of vert locations that describes the incoming polygon
	 * @param out_IntersectingPoly - the poly in this mesh that intersects the passed poly (if any)
	 * @param ExclusionPolys - polys we don't want to collide against
	 * @param bWordlSpace - refernece frame of incoming vert locations
	 * @param MinNormalZ - minimum Z for intersecting polys (e.g. polys steeper than this will be ignored)
	 * @param TraceFlags - flags used for trace dilineation
	 * @return TRUE if the poly described by the list of points intersects the mesh
	 */
	UBOOL IntersectsPoly(TArray<FVector>& InPolyVertLocs,
						FNavMeshPolyBase*& out_IntersectingPoly,
						TArray<FNavMeshPolyBase*>* ExclusionPolys=NULL,
						UBOOL bWorldSpace=WORLD_SPACE,
						const FLOAT MinNormalZ=-1.f, 
						DWORD TraceFlags=0);

	/**
	 * returns TRUE if the AABB passed in intersects with the bounds of a poly in this mesh
	 * @param Center - center of AABB
	 * @param Extent - Extent of AABB
	 * @param out_IntersecingPoly - out param filled with intersecting poly ( if any ) 
	 * @param bWorldSpace - TRUE If the params are in world space
	 * @param TraceFlags - flags used for trace dilineation
	 * @return TRUE if there is an intersection
	 */
	UBOOL IntersectsPolyBounds(const FVector& Center, const FVector& Extent, FNavMeshPolyBase*& out_IntersectingPoly, UBOOL bWorldSpace=WORLD_SPACE, DWORD TraceFlags=0 );
	
	/**
	 * returns TRUE if the vertex passed is an acute corner
	 * @param Vert - vertID of the vert we want to test
	 * @return TRUE if it is acute
	 */
	UBOOL VertIsAcute( VERTID Vert );

	/**
	 *	internal function which will run simplification steps based on configuration parameters
	 * @param bSkipSquareMerge - should be TRUE if htis is a second pass merge (skips expensive merge steps)
	 */
	INT SimplifyMesh(UBOOL bSkipSquareMerge=FALSE);

	// ** attempts to fill in gaps left by stairstepped exploration
	void FillInBorderGaps();

	/** 
	 * attempt to combine two polys using hertel-mehlhorn poly merging
	 * If combination is succesful, delete the two old polys and add a new one for the combined version
	 * @param Poly1 - the first poly to be combined (with Poly2)
	 * @param Poly2 - the second poly to be combined (with Poly1)
	 * @param SharedVert1 - the vert index of the first vertex which makes up the shared edge between these polys
	 * @param SharedVert2 - the vert index of the second vertex which makes up the shared edge between these polys
	 * @param bIgnoreSlop - don't test for slope differences when trying to combine polys
	 * @param AxisMask    - this will be multiplied against all locations during processing, use to specify an axis to be ignored
	 * @param ControlPointMap - map of control points which the combined poly must remain close to
	 * @param bDoEdgeSmoothing - whether or not to try and remove 'jaggy' edges 
	 * @return - return NULL if no combination was possible, or the newly combined polys
	 */
	//@return - returns a ptr to the new poly if the merge succeeded, NULL if not
	FNavMeshPolyBase* TryCombinePolys(FNavMeshPolyBase* Poly1, FNavMeshPolyBase* Poly2,
									  VERTID SharedVert1, VERTID SharedVert2, UBOOL bIgnoreSlope=FALSE,
									  FVector AxisMask=FVector(1.f,1.0f,0.f), ControlPointMap* ControlPointMap=NULL,
									  UBOOL bDoEdgeSmoothing=TRUE);
	/**
	 * used to remove verts that are unecessary during poly merge
	 * @param NewVertList - list of verts representing the new poly we're simplifying
	 * @param ExcludedPolys - list of polys to exclude from 'is point on edge' checks
	 * @param Idx           - current index into NewVertList array we're checking currently
	 * @param bCheckArea    - whether or not to check poly area
	 * @param EdgeOffLineTolerance - when considering a vertex, this is the distance off the line between prev<->next under which we will consider removing this vertex
	 * @param Normal        - of the polygon we're attempting to smoooth
	 * @param bConvexPoly   - TRUE if we should assume polys are convex and it's safe to do convex dependent tests)
	 * @return - TRUE If succesful
	 */
	UBOOL PerformEdgeSmoothingStep(	const TArray<VERTID>& NewVertList,
									TArray<FNavMeshPolyBase*>& ExcludedPolys,
									const TLookupMap<VERTID>& VertsOnEdges_IncludingEndPoints,
									const TLookupMap<VERTID>& VertsOnEdges_NotIncludingEndPoints,
									INT Idx,
									FLOAT EdgeOffLineTolerance,
									const FVector& Norm, 
									UBOOL bConvexPoly);

	/**
	 *	tries to remove unnecessary verts from the given poly
	 * @param Poly - the poly to try and remove unnecessary verts from
	 * @return - number of vertexes removed
	 */
	INT SimplifyEdgesOfPoly(FNavMeshPolyBase* Poly);

	/**
	 * tries to align edges of the given poly with edges of polys adjacent to it
	 * @param Poly - the poly to align
	 * @return the number of verts added in the process
	 */
	INT AlignAdjacentEdgesForPoly(FNavMeshPolyBase* Poly);

		/** 
	 * snap close internal concave verts! 
	 * when merging into concave slabs sometimes you'll get corner verts which are very close to the boundary of the poly
	 * and cause tiny sliver cases that are difficult to decompose into convex shapes.
	 * this function attempts to find these cases and snap the verts onto the boundary 
	 * @param Poly - the poly to snap internal concave verts for
	 */
	void SnapCloseInternalConcaveVertsForPoly(FNavMeshPolyBase* Poly);


	/**
	 * will attempt to combine two polygons by finding an adjacent section and removing interior edges
	 * if polys are not adjacent, polys are not compatible slope, or not compatible height this will return NULL
	 * @param PolyA - first poly to try and combine
	 * @param PolyB - second poly to try and combine
	 * @param COntrolPointMap - control point map to test final poly against
	 * @return - the new combined poly, or NULL if none was possible
	 */	
	FNavMeshPolyBase* TryCombinePolysConcave(
										FNavMeshPolyBase* Poly1,
										FNavMeshPolyBase* Poly2
									        );
	/**
	 * will triangualte the passed poly using ear clipping
	 * @param PolyVerts - vertex list for poly to be triangulated
	 * @param PolyNorm - normal of poly we're triangulating
	 * @param out_VertexBuffer - output buffer of triangles created
	 */	
	void TriangulatePoly( FNavMeshPolyBase* Poly, TArray<VERTID>& out_VertexBuffer);

	/**
	 * will triangualte the passed poly using ear clipping
	 * @param PolyVerts - vertex list for poly to be triangulated
	 * @param PolyNorm - normal of poly we're triangulating
	 * @param out_VertexBuffer - output buffer of triangles created
	 */	
	void TriangulatePoly( const TArray< VERTID >& PolyVerts, const FVector& PolyNorm, TArray<VERTID>& out_VertexBuffer);

	/**
	 *	will decompose a concave poly into a list of convex ones
	 * @param Poly - the (presumably concave) poly to decompose into convex polys
	 * @param bPartialFail - indicates this is the result of a previous decomposition attempt so we should handle it specially
	 * @return - the number of polys generated for the start poly
	 */
	INT DecomposePolyToConvexPrimitives( FNavMeshPolyBase* Poly, INT FailDepth );

	/**
	 * used during concave poly merging (TryCOmbinePolysConcave)
	 * adds verts from Poly to a the vertlist representing the new combined poly.  
	 * will remove duplicate verts, and redundant verts
	 * @param Poly - poly to add verts from
	 * @param StartingLocalVertIdx - local vertIdx to start adding verts from
	 * @param out_NewPolyVertIDs - out var of array representing new combined poly 
	 */
	void AddVertsToCombinedPolyForConcaveMerge( FNavMeshPolyBase* Poly,FNavMeshPolyBase* OtherPoly, INT StartingLocalVertIdx, TArray<VERTID>& out_NewPolyVertIDs );

	/**
	 * Used during concave poly merging (TryCombinePolysConcave).  Determines the vertx in the passed poly to start adding to the combined poly
	 * Choose StartVert:
	 *	Start with either SharingEdgeVert_0 or SharingEdgeVert_1 
	 *	whichever's next is not one of the EdgePoints
	 *  if both verts have nexts which are not edgepoints, choose the one which is within the edge segment
	 * @param Poly - poly to choose starting vert for
	 * @param EdgePt0 - location of found edgepoint_0
	 * @param EdgePt1 - location of found edgepoint_1
	 * @param SharingEdgeVert_0 - vert on the edge of the poly that is adjacent to the other poly (edge which was used to find EdgePts)
	 * @param SharingEdgeVert_1 - other vert on the edge of the poly that is adjacent to the other poly (edge which was used to find EdgePts)
	 */
	INT FindStartingIndex( FNavMeshPolyBase* Poly, FVector EdgePt0, FVector EdgePt1, VERTID SharingEdgeVert_0, VERTID SharingEdgeVert_1 );

	void DrawMesh(FDebugRenderSceneProxy* DRSP, APylon* Pylon);
	void DrawMesh(FDebugRenderSceneProxy* DRSP, APylon* Pylon, FColor C);

	void DrawSolidMesh(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags);

	/**
	 * will return whether or not the poly represented by the passed list of verids is convex
	 * @param VertList - list of vertex IDs that represent the polygon to be tested
	 * @param Tolerace - the tolerance with which to test for convexity
	 * @param PolyNormal - normal of poly (if already computed.. to save cycles)
	 * @return TRUE if the polygon is convex 
	 */
	UBOOL IsConvex(const TArray<VERTID>& VertList, FLOAT Tolerance=-1.f, FVector PolyNormal=FVector(0.f));

	/**
	 * <<static version that takes actual locations>>
	 * will return whether or not the poly represented by the passed list of verids is convex
	 * @param VertList - list of vertex locations that represent the polygon to be tested
	 * @param Tolerace - the tolerance with which to test for convexity
	 * @param PolyNormal - normal of poly (if already computed.. to save cycles)
	 * @return TRUE if the polygon is convex 
	 */
	static UBOOL IsConvex(const TArray<FVector>& VertList, FLOAT Tolerance=-1.f, FVector PolyNormal=FVector(0.f));

	virtual void BeginDestroy();

	virtual void FinishDestroy();

	/**
	 * Add a poly to the polygon octree for this navmesh
	 * @param Poly - the poly to add
	 */
	void  AddPolyToOctree(FNavMeshPolyBase* Poly);

	/**
	 * removes a poly from the polygon octree for this mesh
	 * @param Poly - the poly to remove
	 */
	void  RemovePolyFromOctree(FNavMeshPolyBase* Poly);

	FORCEINLINE DWORD Ver() { return NavMeshVersionNum; }
	virtual void Serialize(FArchive& Ar);

	/**
	* Callback used to allow object register its direct object references that are not already covered by
	* the token stream.
	*
	* @param ObjectArray	array to add referenced objects to via AddReferencedObject
	* - this is here to keep the navmesh from being GC'd at runtime
	*/
	virtual void AddReferencedObjects( TArray<UObject*>& ObjectArray );

	/**
	 * FixupForSaving - massage this mesh's data to be suitable for serialization to disk
	 */
	virtual void FixupForSaving();

	/**
	 * CopyDataToBuildStructures - move this mesh's data back into its build structures. Basically, the inverse of FixupForSaving()
	 */
	virtual void CopyDataToBuildStructures();

	/**
	 * CreateEdgeConnections
	 * create Edges between polygons where applicable
	 */
	virtual void CreateEdgeConnections( UBOOL bTest = FALSE );

	/**
	 * RebuildObstacleMeshGeometryForLinkedEdges
	 * - will split obstacle mesh polys where necessary in order to 
	 *   have obstacle mesh geo that lines up with edges linked to it
	 * @param MeshThatOwnsTheEdges - the mesh that owns the edges linked to our polys (since this is an obstacle msh eh?)
	 */
	void RebuildObstacleMeshGeometryForLinkedEdges(UNavigationMeshBase* MeshThatOwnsTheEdges);

	/**
	 * will add appropriate obstacle mesh geometry for path obstacles who split the mesh
	 * called after edges are created because we don't want to pollute 
	 * @param Mesh - the (walkable) navmesh we are adding geo for
	 */
	void BuildObstacleGeoForPathobjects(UNavigationMeshBase* MeshThatOwnsTheEdges);
	
	// struct to hold temporary 'overlapping' edge data when finding merge candidates
	struct FEdgeTuple
	{
		FEdgeTuple(const FVector& In0,
					const FVector& In1,
					FNavMeshPolyBase* InPoly0,
					VERTID InPoly0V0,
					VERTID InPoly0V1,
					FNavMeshPolyBase* InPoly1,			
					VERTID InPoly1V0,
					VERTID InPoly1V1,
					FNavMeshEdgeBase* InOuterEdge, 
					FNavMeshEdgeBase* InInnerEdge 
					) :
			 Pt0(In0)
			,Pt1(In1)
			,Poly0_Vert0(InPoly0V0)
			,Poly0_Vert1(InPoly0V1)
			,Poly1_Vert0(InPoly1V0)
			,Poly1_Vert1(InPoly1V1)
			,Poly0(InPoly0)
			,Poly1(InPoly1)	
			,OuterEdge(InOuterEdge)
			,InnerEdge(InInnerEdge)
		{	
		}

		FVector Pt0;
		FVector Pt1;
		VERTID Poly0_Vert0;
		VERTID Poly0_Vert1;
		VERTID Poly1_Vert0;
		VERTID Poly1_Vert1;

		FNavMeshPolyBase* Poly0;
		FNavMeshPolyBase* Poly1;

		FNavMeshEdgeBase* OuterEdge;
		FNavMeshEdgeBase* InnerEdge;
	};


	/**
	 * CreateEdgeConnectionsInternal
	 * -internal function called by CreateEdgeConnections which does the heavy lifting for edge creation
	 * @param Edges - list of polygon line segments to use for creation of saved/real Edge Connections
	 * @param bTest - whether this is a test only 
	 * @param bCrossPylonEdgesOnly - if TRUE only cross pylon edges will be created 
	 * @param out_Edges - optional output array of found edges (when supplied actual edges will not be created, instead this array is populated)
	 * @param EdgeDetlaOverride - allows override of MAX_EDGE_DELTA 
	 * @param MinEdgeLengthOverride - allows override of MIN_EDGE_LENGTH
	 * @param POMap - optional param for a map that links polys to path objects (used offline to link edges to POs)
	 */
	virtual void CreateEdgeConnectionsInternal( TArray<FNavMeshEdgeBase>& Edges,
												UBOOL bTest = FALSE,
												UBOOL bCrossPylonEdgesOnly = FALSE,
												UBOOL bAddDynamicEdges = FALSE,
												TArray<FEdgeTuple>* out_Edges=NULL,
												FLOAT EdgeDeltaOverride=-1.f,
												FLOAT MinEdgeLengthOverride=-1.f,
												PolyToPOMap* POMap=NULL);

	/**
	 * CreateEdgeConnectionsInternalFast
	 * -internal function called by CreateEdgeConnections which does the heavy lifting for edge creation
	 * (faster version of create edge connections internal for use at runtime)
	 * @param Edges - list of polygon line segments to use for creation of saved/real Edge Connections
	 * @param bTest - whether this is a test only 
	 * @param bCrossPylonEdgesOnly - if TRUE only cross pylon edges will be created 
	 * @param PolyObstMap - optional map input dictating relationships between polys in this mesh and obstacles (used during internal geo split edge creation)
	 * @param out_Edges - optional output array of found edges (when supplied actual edges will not be created, instead this array is populated)
	 * @param EdgeDetlaOverride - allows override of MAX_EDGE_DELTA 
	 * @param MinEdgeLengthOverride - allows override of MIN_EDGE_LENGTH
	 */
	virtual void CreateEdgeConnectionsInternalFast( TArray<FNavMeshEdgeBase>& Edges,
												UBOOL bTest = FALSE,
												UBOOL bCrossPylonEdgesOnly = FALSE,
												UBOOL bAddDynamicEdges = FALSE,
												PolyToObstacleMap* PolyObstMap = NULL,
												TArray<FEdgeTuple>* out_Edges=NULL,
												FLOAT EdgeDeltaOverride=-1.f,
												FLOAT MinEdgeLengthOverride=-1.f);

	/**
	 * BuildEdgesFromSupportedWidths
	 * - this will run the length of the edge and split it up by supported width, calling ArbitrateAndAddEdgeForPolys on each segment found
	 * @param Vert0 - first vertex of edge
	 * @param Vert1 - second vertex of edge
	 * @param ConnectedPolys - list of polys we are linking together
	 * @param bDynamicEdges - whether we should be added to dynamic lists instead of static ones
	 * @param Poly0Obst - navmeshobstacle (if any) assoicated with poly 0
	 * @param Poly1Obst - navmeshobstacle (if any) assoicated with poly 1
	 * @param OuterEdge - outer test edge we're adding real edges for (used to test vert locs against)
	 * @param InnerEdge - inner test edge we're adding real edges for (used to test vert locs against)
	 * @param POMap - optional param for a map that links polys to path objects (used offline to link edges to POs)
	 * @return - FALSE if we ran out of vert indexes 
	 */
	virtual UBOOL BuildEdgesFromSupportedWidths( const FVector& Vert0,
											  const FVector& Vert1,
											  TArray<FNavMeshPolyBase*>& ConnectedPolys,
											  UBOOL bDynamicEdges,
											  IInterface_NavMeshPathObstacle* Poly0Obst,
											  IInterface_NavMeshPathObstacle* Poly1Obst,
											  FNavMeshEdgeBase* OuterEdge,
											  FNavMeshEdgeBase* InnerEdge,
											  PolyToPOMap* POMap=NULL
											  );


	
	struct FEdgeWidthPair
	{
		FEdgeWidthPair(FVector InPt0, FVector InPt1, FLOAT InWidth, BYTE InEdgeGroupID): Pt0(InPt0), Pt1(InPt1), SupportedWidth(InWidth), EdgeGroupID(InEdgeGroupID){}
		FVector Pt0;
		FVector Pt1;
		FLOAT SupportedWidth;
		BYTE EdgeGroupID;
	};
	/**
	 * will slide along the given span and build a list of edges and supported widths for that span
	 * @param Start - start of segment
	 * @param End - End of segment
	 * @param out_EdgePairs - array to put resulting structs in defining the edges to add
	 * @param bSkipDirectOnSurfaces - when TRUE obstacle mesh polys which are diretly above this edge segment will be skipped
	                                  (used for path object edges which get built on obstacle mesh)
	*/
	void BuildEdgesFromSegmentSpan(const FVector& Start, const FVector& End, TArray<FEdgeWidthPair>& out_EdgePairs, UBOOL bSkipDirectOnSurfaces);

	/**
	 * ArbitrateAndAddEdgeForPolys
	 * - this is called from CreateEdgeConnectionsInternal and will determien what edges need to be added
	 *   for the polys passed.. including path obstacle edge abritration
	 * @param Vert0 - first vertex of edge
	 * @param Vert1 - second vertex of edge
	 * @param ConnectedPolys - list of polys we are linking together
	 * @param bDynamicEdges - whether we should be added to dynamic lists instead of static ones
	 * @param Poly0Obst - navmeshobstacle (if any) assoicated with poly 0
	 * @param Poly1Obst - navmeshobstacle (if any) assoicated with poly 1
	 * @param OuterEdge - outer test edge we're adding real edges for (used to test vert locs against)
	 * @param InnerEdge - inner test edge we're adding real edges for (used to test vert locs against)
	 * @param SupportedEdgeWidth - the width of guy this edge supports
	 * @param EdgeGroupID - ID for the edge's group
	 * @param Poly0PO - path object associated with poly 0 
	 * @param Poly1PO - path object associated with poly 1
	 * @return - FALSE if we ran out of vert indexes 
	 */
	virtual UBOOL ArbitrateAndAddEdgeForPolys( const FVector& Vert0,
											  const FVector& Vert1,
											  const TArray<FNavMeshPolyBase*>& ConnectedPolys,
											  UBOOL bDynamicEdges,
											  IInterface_NavMeshPathObstacle* Poly0Obst,
											  IInterface_NavMeshPathObstacle* Poly1Obst,
											  FNavMeshEdgeBase* OuterEdge,
											  FNavMeshEdgeBase* InnerEdge,
											  FLOAT SupportedEdgeWidth, 
											  BYTE EdgeGroupID,
											  IInterface_NavMeshPathObject* Poly0PO = NULL,
											  IInterface_NavMeshPathObject* Poly1PO = NULL
											  );
	
	/** 
	 * MergeDropDownMesh - simplify the drop down mesh where possible
	 */
	void MergeDropDownMesh();

	/** 
	 * AddTempEdgesForPoly - adds temporary "edges" for each poly for each linesegment the polygon makes up
	 *						 Used during edge creation
	 * @param Poly - poly to add temp edges for
	 * @param Edges - output array to add edges to
	 * @param TestBox - optional, if provided only edges which intersect with this box will be added
	 * @param bWorldSpace - TRUE if we are working in local space
	 */
	void AddTempEdgesForPoly( FNavMeshPolyBase& Poly, TArray<FNavMeshEdgeBase>& Edges, const FBox* TestBox=NULL, UBOOL bWorldSpace=WORLD_SPACE);
	
	/**
	 * snap mesh to other mesh's edges.. used for imported meshes to snap/align themselves
	 * to edges nearby in the provided mesh
	 * @param OtherMesh - mesh to snap to
	 */
	void SnapMeshVertsToOtherMesh(UNavigationMeshBase* OtherMesh);

	/**
	 * SplitMeshForIntersectingImportedMeshes
	 * Subdivides polys at intersections with other meshes to allow for easy cross-pylon edge creation with other NavMeshes
	 */
	void SplitMeshForIntersectingImportedMeshes();


	/**
	 * GetAlLVertsNearPoint 
	 *  returns all vertices within the passed extent centered about Pt
	 * @param Pt - the center of the box to check
	 * @Param Extent - extent describing the box to check within
	 * @param Result - output array of vertids which fall within the box
	 */
	void GetAllVertsNearPoint(const FVector& Pt, const FVector& Extent, TArray<VERTID>& Result);

	/**
	 * GetAllPolysNearPoint
	 *  will find all polygons which fall within the pass extent
	 * @param Pt - center of extent
	 * @param Extent - extent describing the box to check within
	 * @param Result - output array that polys within the extent are copied to
	 */
	void GetAllPolysNearPoint(const FVector& Pt, const FVector& Extent, TArray<FNavMeshPolyBase*>& Result);
	
	/**
	 * GetPolyFromPoint
	 *  Find the polygon which contains the passed point
	 * @param Pt - the point to find a polygon for
	 * @param Up - up vector of the point we're trying to find ( polys are volumetric, corner cases will have overlaps )
	 * @param bWorldSpace - what ref frame the passed point is in
	 * @return - the poly found that contains the point, NULL if none found
	 */
	FNavMeshPolyBase* GetPolyFromPoint(const FVector& Pt, FLOAT MinWalkableZ,UBOOL bWorldSpace=WORLD_SPACE);


	/**
	* GetPolyFromBox
	*  Find the polygon which contains the box
	* @param Box - the box we're checking for
	* @param Up - up vector of the point we're trying to find ( polys are volumetric, corner cases will have overlaps )
	* @param bWorldSpace - what ref frame the passed point is in
	* @return - the poly found that contains the point, NULL if none found
	*/
	FNavMeshPolyBase* GetPolyFromBox(const FBox& Box, FLOAT MinWalkableZ,UBOOL bWorldSpace=WORLD_SPACE);

	/**
	 * GetPolyFromId
	 *  Returns the polygon associated with the passed poly ID
	 */
	FNavMeshPolyBase* GetPolyFromId(WORD Id);

	/**
	 * will remove "silly" polygons from the mesh (e.g. polys which are less than a certain area)
	 * @return - the number of polys removed 
	 */
	INT CullSillyPolys();

	/**
	 * constructs the KDOP tree for this mesh
	 * @param bFromBuildStructures (optional) - when TRUE the kdop will be constructed from build poly lists and not the normal 'polys' array
	 */
	void BuildKDOP(UBOOL bFromBuildStructures=FALSE);

	/**
	 * constructs the KDOP tree for this mesh, forced update
	 * @param bFromBuildStructures (optional) - when TRUE the kdop will be constructed from build poly lists and not the normal 'polys' array
	 */
	void ForcedBuildKDOP(UBOOL bFromBuildStructures=FALSE);

	/**
	 * Perform a line check against this mesh
	 *
	 * @param NormalNavigationMesh - the mesh of obstaclemesh/navigationmehs pairs which represents the navigation mesh
	 * @param Result - output of the result of the linecheck
	 * @param End - end point for linecheck
	 * @param Start - start point for line check
	 * @param Extent - the extent to use for the line/box check
	 * @param TraceFlags - flags to use during the trace
	 * @return TRUE if check did NOT hit anything
	 */
	UBOOL LineCheck( class UNavigationMeshBase* NormalNavigationMesh, FCheckResult& Result, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags, FNavMeshPolyBase** out_HitPoly=NULL );


	/**
	 * performs a linecheck only againts the polys in the list
	 * @param Start - start of linecheck
	 * @param End - .. end of line check
	 * @param Extent - extent of linecheck to perform (for swept box check)
	 * @param Polys - polys to check against
	 * @param out_HitLoc - location on the poly we hit
	 * @param out_HitTime - optional param stuffed with hit time
	 * @param out_HitPoly - optional param, supplied with the poly that was hit
	 * @return TRUE if we hit something
	 */
	static UBOOL LineCheckAgainstSpecificPolys(const FVector Start, const FVector End, const FVector Extent, const TArray<FNavMeshPolyBase*>& Polys, FVector& out_HitLoc, FLOAT* out_HitTime=NULL, FNavMeshPolyBase** out_HitPoly=NULL);
	
	/**
	 * Perform a point check against this mesh
	 *
	 * @param NormalNavigationMesh - the mesh of obstaclemesh/navigationmehs pairs which represents the navigation mesh
	 * @param Result - output of the result of the linecheck
	 * @param Pt - the point to check
	 * @param Extent - the extent to use for the point check
	 * @param TraceFlags - flags to use during the trace
	 * @return TRUE if check did NOT hit anything
	 */
	UBOOL PointCheck( class UNavigationMeshBase* NormalNavigationMesh, FCheckResult& Result, const FVector& Pt, const FVector& Extent, DWORD TraceFlags, FNavMeshPolyBase** out_HitPoly=NULL );

	/** 
	 * GetIntersectingPolys (to passed extent)
	 * - returns a list of polys intersecting the passed extent
	 * @param Loc - location of center of extent
	 * @param Extent - extent of box to check
	 * @param out_Polys - output array of intersecting polys
	 * @param bWorldSpace - reference frame incoming params are in
	 * @param bIgnoreDynamic - whether or not to ignore dynamic (sub) meshes (don't use this unless you know what you're doing.. :) Typically If there are dynamci polys around, they should be used over the static ones
	 * @param bReturnBothDynamicAndStatic - if TRUE, BOTH dynamic and static polys will be returned.. using this is *DANGEROUS*! most of the time you should use dynamic polys if they exist
	 *                                      as they are the 'correct' representation of the mesh at that point
	 * @param bDontClearSubmeshList=FALSE - passed TRUE when this is getting called from a recursive situation (e.g. getting called on a submesh)
	 */
	void GetIntersectingPolys(const FVector& Loc, const FVector& Extent, TArray<FNavMeshPolyBase*>& out_Polys, UBOOL bWorldSpace=WORLD_SPACE, UBOOL bIgnoreDynamic=FALSE, UBOOL bReturnBothDynamicAndStatic=FALSE, UBOOL bRecursedForSubmesh=FALSE, DWORD TraceFlags=0);
	
	/** 
	 * GetIntersectingPolys (to passed poly made up of vertids)
	 * returns a list of polys intersecting the passed poly
	 * @param PolyVerts - verts of poly to check intersections for
	 * @param out_Polys - output array of intersecting polys
	 * @param bWorldSpace - reference frame of incoming vert lcoations
	 */	
	void GetIntersectingPolys(const TArray<FVector>& PolyVerts, TArray<FNavMeshPolyBase*>& out_Polys, UBOOL bWorldSpace, DWORD TraceFlags=0);

	/** 
	* GetIntersectingPolys (to passed poly made up of vert locations)
	* returns a list of polys intersecting the passed poly
	* @param PolyVerts - verts of poly to check intersections for
	* @param out_Polys - output array of intersecting polys
	*/	
	void GetIntersectingPolys(const TArray<VERTID>& PolyVerts, TArray<FNavMeshPolyBase*>& out_Polys, DWORD TraceFlags=0);


	/**
	 * Try to merge adjacent polys when the resulting poly remains convex
	 * 
	 * @param AxisMask - indicates by default we will tell trycombinepolys to ignore Z when testing for aligned verts
	 * @param bDoEdgeSMoothing - whether or not we should attempt to smooth out edges while merging
	 * @return - number of polys merged 
	 */
	// 
	INT MergePolys(FVector AxisMask=FVector(1.0f,1.0f,0.f), UBOOL bDoEdgeSmoothing=TRUE);

	/**
	 *	will merge the mesh into concave slabs split by slope only (to be triangulated later)
	 */
	INT MergePolysConcave();

	/**
	 *	this will run around to all the polys in the mesh and attempt to remove unnecessary verts
	 */
	void SimplifyEdgesOfMesh();

	/**
	 * this will keep edges of adjacent polys aligned together and prevent large vertical gaps between adjacent polys
	 */
	void AlignAdjacentPolys();

	/** 
	 * snap close internal concave verts! 
	 * when merging into concave slabs sometimes you'll get corner verts which are very close to the boundary of the poly
	 * and cause tiny sliver cases that are difficult to decompose into convex shapes.
	 * this function attempts to find these cases and snap the verts onto the boundary 
	 */
	void SnapCloseInternalConcaveVerts();

	/**
	 * this will triangulate all polys in the mesh.  First by generating a tri list, and then removing all old polys and adding triangles
	 */
	void TriangulateMesh();

	/**
	 *	ConvexinateMesh
	 *  Decomposes concave slabs into convex polys (calls DecomposePolyToConvexPrimitives on each poly, adding the resulting polys and removing the concave one)
	 *
	 */
	void ConvexinateMesh();

	/**
	 * builds a list of poly edges which are on the border of the mesh, for use with stitching etc..
	 */
	void BuildBorderEdgeList(UBOOL bForSubMesh=FALSE);

	/**
	 * ImportBuildPolys
	 *  imports BuildPolys list from external source, calculating all derived values and skipping over invalid items.
	 * @param InVerts - list of vert locations
	 * @param InPolys - list of polygons to import (must have: PolyVerts, PolyHeight)
	 * @param InBorderPolys - border information, matching size of InPolys array (1 = poly on navmesh boundary)
	 * @param BoundsZOffset - height offset for lowering poly bounds
	 */
	void ImportBuildPolys(TArray<FMeshVertex>& InVerts, TArray<FNavMeshPolyBase*>& InPolys, const TArray<BYTE>& InBorderPolys, FLOAT BoundsZOffset=0);

	/**
	 * RuntimeMoveVertex
	 *  moves a vertex of already built poly and updates all runtime structures of navmesh and obstacle mesh
	 *  apart from bounds and KDOP tree
	 * @param VertId - Id of vertex to move
	 * @param Location - new vertex location in world space
	 * @param BoundsZOffset - height offset for lowering poly bounds
	 */
	void RuntimeMoveVertex(VERTID VertId, const FVector& Location, FLOAT BoundsZOffset=0);

	/**
	 * sets up VertHash mapping, required for FindVert in runtime
	 */
	void PrepareVertexHash();

	/**
	 * AddSquarePolyFromExtent
	 *  Adds a square polygon to the mesh
	 * @param Poly - list of vert locations that represent the incoming polygon
	 * @param Location - center of the poly we're adding
	 * @param PolyHeight - the height supported at this polygon ( if -1.0f a new height will be calculated )
	 * @return - the poly added 
	 */
	FNavMeshPolyBase* AddSquarePolyFromExtent(TArray<FVector>& Poly, const FVector& Location, FLOAT PolyHeight=-1.0f);
	
	/**
	 * AddTriangesForSlopedPoly
	 *  Adds two triangles for the square passed, split perp to the slope 
	 * @param Poly - list of vert locations that represent the incoming polygon
	 * @param Location - center of the poly we're adding
	 * @param PolyNorm - normal of poly being added
	 * @return - one of the polys added 
	 */
	FNavMeshPolyBase* AddTriangesForSlopedPoly(TArray<FVector>& Poly, const FVector& Location, const FVector& PolyNorm);	

	/**
	 * BuildPolyFromExtentAndHitInfo
	 *  will construct a polygon from the passed location, width, and normal
	 * @param Location - location (center) of poly
	 * @param HitNormal - normal of the ground at this spot
	 * @param HalfWidth - halfwidth of square to create
	 * @param out_Poly - array containing the poly constructed
	 */
	void BuildPolyFromExtentAndHitInfo(const FVector& Location, const FVector& HitNormal, FLOAT HalfWidth, TArray<FVector>& out_Poly);

	/**
	* BuildSubMeshForPoly
	*  Will build a sub-mesh for the given polygon based on the obstacles registered that affect
	*  that poly
	* @param PolyIdx - the index of the poly being built
	* @param out_ObstaclesThatWereJustBuilt - output array keeping track of which polys were build (since this can trigger adjacent builds)
	* @return - TRUE if successful
	*/
	UBOOL BuildSubMeshForPoly(WORD PolyIdx, TArray<FPolyObstacleInfo*>& out_ObstaclesThatWereJustBuilt);

	/**
	 * Called after a submesh is built for a poly, allows metadata to get relinked to the new polys
	 * @param PolyID - index of poly whose submesh was just built
	 * @param SubMesh - submesh just built for this poly
	 */
	void RebuildMetaDataLinkageForSubMesh(WORD PolyID, UNavigationMeshBase* SubMesh);

	// storage struct for describing shapes to split the mesh with (used by SplitMeshAboutShapes())
	struct FMeshSplitingShape
	{
		// CW list of verts describing convex shape to split mesh with
		TArray<FVector> Shape;
		// height above shape with which to split
		FLOAT			Height;
		// identifier for this shape
		INT				ID;
		// does this shape need internal geometry to be preserved (not deleted right away)
		UBOOL			bKeepInternalGeo;
	};

	// tuple struct for storing a binding between a temporary mesh created to store polys internal to a shape,
	// and the index of that shape
	struct FMeshPolySplitShapePair
	{
		// temporary mesh created to describe internal mesh geo
		UNavigationMeshBase* Mesh;
		// id of the shape associated with this temporary mesh
		INT ShapeID;
	};
	

	
	/**
	 * This function takes a list of shape descriptors and will split the mesh about those shapes 
	 * (e.g. treat the shapes as cookie cutters, split the mesh based on them) and returns a list of temporary
	 * meshes created to describe internal geometry if any was created
	 * @Param Shapes - list of shapes to split mesh with
	 * @param out_internalMeshes - list of temporary internal meshes which have been created during splitting (if any)
	 * @return - FALSE on failure
	 */
	UBOOL SplitMeshAboutShapes(const TArray<FMeshSplitingShape>& Shapes, TArray<FMeshPolySplitShapePair>& out_InternalMeshes);

	/**
	 * marks obstacle geo associated with cross pylon edges on the passed poly as disabled so that edge generation can commence
	 * @param PolyID - ID associated with the poly whose edges we are looking at 
	 * @param out_AffectedPolys - list of obstacle mesh polys affected by the change
	 * @param bNewCollisionState - whether to disable or enable collision (TRUE indicates we should turn collision back on)
	 */
	void ChangeObstacleMeshCollisionForCrossPylonEdges(WORD PolyID, TArray<FNavMeshPolyBase*>& out_AffectedPolys, UBOOL bNewCollisionState);

	/**
	 * builds edges for a submesh of a given polygon
	 * @param PolyIdx - id of the poly to build submesh edges for
	 * @param ObstaclesThatWereJustBuild - list of obstacles that we just built this iteration 
	 * @return - TRUE If successful 
	 */
	UBOOL BuildSubMeshEdgesForPoly(WORD PolyIdx, const TArray<FPolyObstacleInfo*>& ObstaclesThatWereJustBuilt);

	/**
	 * builds edges to top level polys (to adjacent submeshes) that were just cleared of obstacles
	 * @param PolyIdx - id of the poly to build submesh edges for
	 * @return - TRUE If successful 
	 */
	UBOOL BuildSubMeshEdgesForJustClearedTLPoly(WORD PolyIdx);

	/**
	* splits all polys in this mesh along the line provided
	* @param EdgePt0 - end point 0 of the edge we're splitting for
	* @param EdgePt1 - edge point 1 of the edge we're splitting for
	* @param Height  - height of edge (e.g. make a vertical surface starting at the end points provided that's this high)
	*				   defaults to -1 which indicates infinite height
	* @return - TRUE if succesful
	*/
	UBOOL SplitMeshAboutEdge(const FVector& EdgePt0, const FVector& EdgePt1, const FVector& Up, FLOAT Height=-1.0f); 

	/**
	* Generates vert lists by splitting the passed polygon along the supplied plane
	*  @param Poly - poly to split
	*  @param Plane - plane to split along
	*  @param out_Poly1Verts - output array for verts in poly1
	*  @param out_Poly2Verts - output array for vertsin poly2
	*  @return TRUE if succesful 
	*/
	UBOOL SplitPolyAlongPlane(const FNavMeshPolyBase* Poly, const FPlane& Plane, TArray<FVector>& out_Poly1Verts, TArray<FVector>& out_Poly2Verts);

	/**
	 * will split this mesh about any path objects that need the mesh to be split
	 * @return TRUE If sucessful 
	 */
	UBOOL SplitMeshAboutPathObjects();

	/**
	 * This will ensure that the state of dynamic obstacles is up to date, and ready for 
	 * use by an entity
	 * @param out_ObstaclesThatWereJustBuild - list of obstacles stuffed by what was just built
	 */
	void UpdateDynamicObstacles(TArray<FPolyObstacleInfo*>& ObstaclesThatWereJustBuilt);

	/**
	 * builds all submeshes that need building (only geo, no edges/obstacle mesh)
	 * @param out_ObstaclesThatWereJustBuild - list of obstacle infos for the submeshes we just built
	 */
	void BuildAllSubMeshGeometry(TArray<FPolyObstacleInfo*>& out_ObstaclesThatWereJustBuilt);

	/**
	 * FinishSubMeshBuilds
	 * - will run through all the submeshes in this navmesh that were just built and finish them up (build edges, built up obstacle mesh)
	 * @param ObstaclesThatWereJustBuild - list of infos that we need to touch up
	 */
	void FinishSubMeshBuilds(TArray<FPolyObstacleInfo*>& out_ObstaclesThatWereJustBuilt);

	/**
	 * GetPolyObstacleInfo will call into the map and try to find a FPolyObstacleInfo corresponding to the passed
	 * Poly
	 * @param Poly - the poly to lookup
	 * @return - the info found for the passed poly (NULL if none found)
	 */
	FPolyObstacleInfo* GetPolyObstacleInfo(FNavMeshPolyBase* Poly);

	/**
	* this function will notify any AIs who are using this mesh that it's about to be deleted
	* @param ObstacleInfoForThisMesh - if this mesh is associated with a poly in the top level mesh, the obstacleinfo will be passed along
	*                                  that is associated with the obstacle for the submesh (used to do extra cleanup in this case)
	*/
	void CleanupMeshReferences(FPolyObstacleInfo* ObstacleInfoForThisMesh=NULL);

	/**
	 * removes polys in a dynamic obstacle mesh that were added for this navmesh
	 * @param ObstacleInfoForThisMesh - the obstacleinfo associated with this mesh (because it's a submesh)
	 */
	void RemovePolysFromDynamicObstacleMeshForMe(FPolyObstacleInfo* ObstacleInfoForThisMesh);


	/** 
	 * Setter for setting whether this mesh needs obstacle recompute or not
	 * @param bNeedsRecompute - incoming bool indicating if this mesh needs recompute
	 */
	void SetNeedsRecompute(UBOOL bNeedsRecompute);


	/**
	 * Called when our owning pylon gets OnRemoveFromWorld called on it. 
	 * will flush all submeshes and clear refs to this mesh
	 */
	void OnRemoveFromWorld();

	struct FDynamicSnapInfo
	{
		VERTID VertId;
		FVector VertWorldLocation;
	};

	/** 
	 * Applies dynamic snap data to mesh vertices and recalculates bounds and properties of affected polys.
	 * Used in simple adjusting to neighbour NavMeshes in runtime (DynamicPylon.ApplyDynamicSnap)
	 * @param SnapData - vertex move data
	 * @param BoundsZOffset - height offset for lowering poly bounds
	 */
	void ApplyDynamicSnap(const TArray<FDynamicSnapInfo>& SnapData, FLOAT BoundsZOffset=0);

	/**
	 * Reverts all changes caused by dynamic snap
	 */
	void RevertDynamicSnap();

	/** disables RevertDynamicSnap function */
	UBOOL bSkipDynamicSnapRevert;

private:
	
	/** temporary data used for reverting dynamic vertex snap */
	TArray<FDynamicSnapInfo> DynamicSnapRevertData;
	
	/**
	 * attempts to construct the largest squares possible out of the grid created during exploration
	 * @return - the number of polys removed via merging
	 */
	INT MergeSquares();

	/**
	 * will find situations where three polys can be merged into two, and do so
	 * @return - the number of polys removed via merging
	 */
	INT ThreeToTwoMerge();

	/**
	 * TryBackFillBorderCorner
	 *  will attempt to create a new triangle polygon to fill in the corner specified
	 * @param Vert1 - first vertex of triangle ot try
	 * @param Vert2 - second vertex of triangle to try
	 * @param Vert3 - third vertex of triangle to try
	 * @param bProbeOnly - whether we are just testing or if we should actually add the poly
	 * @return - the area of the poly added (0 if none)
	 */
	FLOAT TryBackFillBorderCorner(VERTID Vert1, VERTID Vert2, VERTID Vert3, UBOOL bProbeOnly);
	

	/** 
	 * FindAdjacentPolysSharingExactlyOneVertex
	 *  attempts to find a polygon bording the passed polygon that shared exactly one vertex with it
	 *  (used during three->two merge)
	 * @param ChkPoly - the poly to find an adjacent one from
	 * @param out_AdjacentPolys - list of polys which are adjacent and share one vert
	 * @param out_SharedVertexPoolIndexes - list of vertids which are shared by these adjacent polys
	 * @return TRUE if one was found
	 */
	UBOOL FindAdjacentPolysSharingExactlyOneVertex( FNavMeshPolyBase* ChkPoly, TArray<FNavMeshPolyBase*>& out_AdjacentPolys, TArray<VERTID>& out_SharedVertexPoolIndexes );
	
	/** 
	 * FindAdjacentPolyToBothPolys
	 *  attempts to find a polygon which is adjacent to the two passed in
	 *  (used during three->two merge)
	 * @param SharedPoolIndex - vertID of the vertex shared by the two polys
	 * @param PolyA - first poly to find sharing poly for
	 * @param PolyB - second poly to find sharing poly for
	 * @param out_AdjacentPoly - poly which is adjacent to both polys 
	 * @return - TRUE if an adjacent poly was found
	 */
	UBOOL FindAdjacentPolyToBothPolys( VERTID SharedPoolIndex, FNavMeshPolyBase* PolyA, FNavMeshPolyBase* PolyB, FNavMeshPolyBase*& out_AdjacentPoly );
	
	/**
	 * ChoosePolyToSplit
	 *  determines which of the three polys is the best to be split in order to facilitate a 3->2 merge
	 *  (used during three->two merge)
	 * @param PolyA - poly candidate
	 * @param PolyB - poly candidate
	 * @param PolyC - poly candidate
	 * @param out_SplitIndex - the vertid of the vertex we're splitting along
	 * @return TRUE if a poly was found to split
	 */
	UBOOL ChoosePolyToSplit( FNavMeshPolyBase* PolyA, FNavMeshPolyBase* PolyB, FNavMeshPolyBase* PolyC, VERTID SharedPoolIndex, FNavMeshPolyBase*& out_PolyToSplit, FNavMeshPolyBase*& out_PolyToKeep, VERTID& out_SplitIndex );
	
	/**
	 * CalcSplitVertexLocation
	 *  caclulates where the new vertex should be added when splitting the bigger polygon
	 *  (used during three->two merge)
	 * @param PolyToSplit - the polygon we're splitting
	 * @param SplitPoolIndex - the vertid of the vertex we're splitting along
	 * @param SplitDir - the direction we're splitting
	 * @param out_SplitPt - the point we chose to add a new vertex
	 * @return TRUE of a location was calculated
	 */
	UBOOL CalcSplitVertexLocation( FNavMeshPolyBase* PolyToSplit, VERTID SplitPoolIndex, FVector& SplitDir, FVector& out_SplitPt );

	/**
	 * ShiftVertsToCCWatIdxZero
	 *  Will shift around the vertlist until the vertex at 0 is top right of the poly
	 * @param VertList - the list of verts to shift around
	 */
	void ShiftVertsToCCWatIdxZero(TArray<VERTID>& VertList);

	/** Octree of polygons for quick lookups */
	FPolyOctreeType* PolyOctree;

	/** Hash of verts.. used during build to quickly detect when a vertex has already been added in a location*/
	FVertHash*		 VertHash;

	/** whether or not the KDOP for this mesh has been built yet */
	UBOOL			 KDOPInitialized;

public:
	// the version this mesh was saved at (for serialization)
	DWORD			 NavMeshVersionNum;

	// the version this mesh was generated at (for 'needs rebuild' warnings)
	DWORD			 VersionAtGenerationTime;

	/** KDOP Tree for this navigation mesh - used for linechecks against this mesh */
	NavMeshKDOPTree KDOPTree;

	/** Local to world transform for this mesh currently */
	FMatrix LocalToWorld;

	/** World to local transform for this mesh currently */
	FMatrix WorldToLocal;
};

#define IMPLEMENT_EDGE_CLASS(CLASSNAME) \
	FName CLASSNAME::ClassName = FName(TEXT(#CLASSNAME));\
	void CLASSNAME::Register(){UNavigationMeshBase::GEdgeNameCtorMap.Set(CLASSNAME::ClassName,CtorContainer(&CLASSNAME::CLASSNAME##Constructor));}\
	DWORD CLASSNAME::CLASSNAME##Constructor(TArray<BYTE>& DataBuffer)\
	{\
		DWORD Offset = DataBuffer.Num();\
		DataBuffer.Add(sizeof(CLASSNAME));\
		void* DataPtr = (FNavMeshEdgeBase*)(&DataBuffer(0) + Offset);\
		CLASSNAME Temp;\
		appMemzero(DataPtr,sizeof(CLASSNAME));\
		*(void**)DataPtr= *(void**)&Temp;\
		return Offset;\
	}


template< typename T>
T* UNavigationMeshBase::AddEdgeData(T& NewEdge, WORD& out_EdgeIdx)
{
	// make the buffer bigger to accommodate the new edge
	DWORD Offset = EdgeDataBuffer.Num();
	EdgeDataBuffer.Add(sizeof(T));

	T* Edge = (T*)(&EdgeDataBuffer(0)+Offset);
	T TempEdge;
	// @note: clang will generate a warning about this overwriting the vtable in some T types
	// the (void*) cast silences it in this case. could also use -Wno-non-pod-memaccess
	appMemcpy((void*)Edge,(void*)&TempEdge,sizeof(T));

	out_EdgeIdx=EdgeStorageData.AddItem(FEdgeStorageDatum(Offset,sizeof(T),T::ClassName));
		*Edge = NewEdge;
	
	return Edge;
}

template< class T > 
UBOOL UNavigationMeshBase::AddEdge( const FVector& inV1,
								   const FVector& inV2,
								   FLOAT SupportedEdgeWidth,
								   BYTE EdgeGroupID,
								   TArray<FNavMeshPolyBase*>* ConnectedPolys,
								   T** out_EdgePtr,
								   UBOOL bOneWay,
								   VERTID V1Idx,
								   VERTID V2Idx  )
{
	UBOOL bCrossPylonEdge = FALSE;

	TArray<APylon*> ConnectedPylons;
	checkSlow(ConnectedPolys->Num() == 2);
	if( ConnectedPolys != NULL )
	{
		FNavMeshPolyBase* Poly = NULL;
		for( INT ConnectedIdx = 0; ConnectedIdx < ConnectedPolys->Num(); ConnectedIdx++ )
		{
			Poly = (*ConnectedPolys)(ConnectedIdx);
			ConnectedPylons.AddUniqueItem(Poly->NavMesh->GetPylon());
		}
	}
	bCrossPylonEdge = ConnectedPylons.Num() > 1;

	if(bCrossPylonEdge)
	{
		if( out_EdgePtr != NULL )
		{
			*out_EdgePtr = NULL;
		}
		AddCrossPylonEdge( inV1, inV2, *ConnectedPolys, SupportedEdgeWidth, EdgeGroupID );
		return TRUE;
	}

	check(ConnectedPylons.Num() < 3);
	APylon* Py = GetPylon();
	FLOAT MaxHeightDelta = -1.f;
	if(Py != NULL && Py->bImportedMesh)
	{
		MaxHeightDelta = GRIDSIZE;
	}

	if( V1Idx == MAXVERTID )
	{
		V1Idx = AddVert(inV1,WORLD_SPACE,MaxHeightDelta,TRUE);
	}

	if( V2Idx == MAXVERTID )
	{
		V2Idx = AddVert(inV2,WORLD_SPACE,MaxHeightDelta,TRUE);
	}

	checkSlowish(V1Idx != V2Idx);

	if( V1Idx == MAXVERTID || V2Idx == MAXVERTID )
	{
		*out_EdgePtr = NULL;
		return FALSE;
	}

	VERTID NewEdgeID = MAXWORD;
	FNavMeshEdgeBase* NewEdge = NULL;
	T TempNewEdge = T(this, V1Idx, V2Idx);
	T* TypedNewEdge = NULL; // typed ref to the new edge which will be returned if we just created an edge

	// Loop through all edges...
	// If any of the edges share the same verts (in either order) just use the existing edge
	for( INT EdgeIdx = 0; EdgeIdx < GetNumEdges(); EdgeIdx++ )
	{
		FNavMeshEdgeBase* Edge = GetEdgeAtIdx(EdgeIdx);
		if( Edge->EdgeType == TempNewEdge.GetEdgeType()	&&
			Edge->HasVert(V1Idx) && 
			Edge->HasVert(V2Idx) &&
			Edge->BuildTempEdgePolys.ContainsItem((*ConnectedPolys)(0)) &&
			Edge->BuildTempEdgePolys.ContainsItem((*ConnectedPolys)(1)) &&
			Edge->IsOneWayEdge() == TempNewEdge.IsOneWayEdge() )
		{
			NewEdge=Edge;
			NewEdgeID = EdgeIdx;
			break;
		}
	}

	// Otherwise...
	if( NewEdgeID == MAXWORD )
	{
		TypedNewEdge = AddEdgeData<T>(TempNewEdge,NewEdgeID);
		NewEdge = TypedNewEdge;

		checkSlowish(NewEdge);

		// copy over the width this edge supports
		NewEdge->EffectiveEdgeLength = SupportedEdgeWidth;

		// copy over group ID
		NewEdge->EdgeGroupID = EdgeGroupID;
	}

	if( ConnectedPolys != NULL )
	{
		checkSlowish(NewEdge != NULL);
		FNavMeshEdgeBase& Edge = *NewEdge;
		for( INT ConnectedIdx = 0; ConnectedIdx < ConnectedPolys->Num(); ConnectedIdx++ )
		{
			FNavMeshPolyBase* Poly = (*ConnectedPolys)(ConnectedIdx);
			checkSlowish(Poly != NULL);
			if ( ConnectedIdx == 0 || !(Edge.IsOneWayEdge()||bOneWay) )
			{
				Poly->PolyEdges.AddUniqueItem(NewEdgeID);
			}
			Edge.BuildTempEdgePolys.AddUniqueItem(Poly);
		}

		checkSlowish((*ConnectedPolys)(0) != (*ConnectedPolys)(1));
		Edge.SetPoly0((*ConnectedPolys)(0));
		Edge.SetPoly1((*ConnectedPolys)(1));
		Edge.UpdateEdgePerpDir();
	}

	if( out_EdgePtr != NULL )
	{
		*out_EdgePtr = (ConnectedPolys != NULL) ? (T*)NewEdge : NULL;
	}

	return ( NewEdge != NULL );
}

template< class T >
UBOOL UNavigationMeshBase::AddOneWayCrossPylonEdgeToMesh(const FVector& StartPt,
								const FVector& EndPt,
								TArray<FNavMeshPolyBase*>& ConnectedPolys,
								FLOAT SupportedEdgeWidth,
								BYTE EdgeGroupID,
								T** out_EdgePtr,
								UBOOL bForce,
								UBOOL bAddDummyRefEdge, 
								INT* out_EdgeId)
{
	check(ConnectedPolys.Num() ==2);
	// get vert ids for starting mesh
	VERTID StartMeshVertIdx0 = AddVert(StartPt);
	VERTID StartMeshVertIdx1 = AddVert(EndPt);


	// find the pylon/mesh for the end point
	FNavMeshPolyBase* EndPoly=ConnectedPolys(1);
	check(EndPoly);
	UNavigationMeshBase* EndMesh = EndPoly->NavMesh;


	// get vert ids for ending mesh
	VERTID EndMeshVertIdx0 = EndMesh->AddVert(StartPt);
	VERTID EndMeshVertIdx1 = EndMesh->AddVert(EndPt);

	// check for vert index overflow
	if( StartMeshVertIdx0 == MAXVERTID || StartMeshVertIdx1 == MAXVERTID ||
		EndMeshVertIdx0 == MAXVERTID || EndMeshVertIdx1 == MAXVERTID )
	{
		*out_EdgePtr = NULL;
		return FALSE;
	}

	WORD NewEdgeID = MAXWORD;
	FNavMeshEdgeBase* NewEdge = NULL;
	T TempNewEdge = T(this, GetPylon(),ConnectedPolys(0)->Item,StartMeshVertIdx0,StartMeshVertIdx1,
					 EndMesh->GetPylon(),EndPoly->Item,EndMeshVertIdx0,EndMeshVertIdx1);
	T* TypedNewEdge = NULL; // typed ref to the new edge which will be returned if we just created an edge


	// look for identical edges, and skip the add if there is one
	if( !bForce )
	{
		FNavMeshPolyBase* StartPoly = ConnectedPolys(0);
		for(INT StartPolyEdgeIdx=0;StartPolyEdgeIdx<StartPoly->GetNumEdges(); ++StartPolyEdgeIdx)
		{
			FNavMeshEdgeBase* Edge = StartPoly->GetEdgeFromIdx(StartPolyEdgeIdx);
			if(Edge != NULL)
			{
				if(Edge->GetEdgeType() == TempNewEdge.GetEdgeType() &&
					Edge->HasVert(EndMeshVertIdx0) &&
					Edge->HasVert(EndMeshVertIdx1))
				{
					if( out_EdgePtr != NULL )
					{
						*out_EdgePtr=NULL;
					}
					if( out_EdgeId != NULL )
					{
						*out_EdgeId=-1;
					}

					return TRUE;
				}
			}
		}
	}

	TypedNewEdge = AddEdgeData<T>(TempNewEdge,NewEdgeID);
	NewEdge = TypedNewEdge;

	// copy over computed supported edge width
	NewEdge->EffectiveEdgeLength = SupportedEdgeWidth;

	// copy over edge group ID 
	NewEdge->EdgeGroupID = EdgeGroupID;

	checkSlowish(NewEdge);

	checkSlowish(NewEdge != NULL);
	FNavMeshEdgeBase& Edge = *NewEdge;
	FNavMeshPolyBase* Poly = ConnectedPolys(0);
	checkSlowish(Poly->NavMesh == this);
	checkSlowish(Poly != NULL);
	Poly->PolyEdges.AddUniqueItem(NewEdgeID);
	Edge.BuildTempEdgePolys.AddUniqueItem(Poly);

	
	checkSlowish((ConnectedPolys)(0) != (ConnectedPolys)(1));
	Edge.SetPoly0((ConnectedPolys)(0));
	Edge.SetPoly1((ConnectedPolys)(1));
	Edge.UpdateEdgePerpDir();
	
	if ( NewEdge != NULL && bAddDummyRefEdge )
	{
		static TArray<FNavMeshPolyBase*> TempPolys;
		TempPolys.Reset(2);
		TempPolys = ConnectedPolys;
		TempPolys.Swap(0,1);
		TempPolys(0)->NavMesh->AddOneWayCrossPylonEdgeToMesh<FNavMeshOneWayBackRefEdge>(StartPt,EndPt,TempPolys,SupportedEdgeWidth,MAXBYTE,NULL,FALSE,FALSE);			
		// since we just added an edge, the NewEdge pointer could be wrong (if we realloc'd) grab the address again
		NewEdge = GetEdgeAtIdx(NewEdgeID);
	}

	if( out_EdgePtr != NULL )
	{
		*out_EdgePtr = (T*)NewEdge;
	}

	if( out_EdgeId != NULL )
	{
		*out_EdgeId=NewEdgeID;
	}

	return (NewEdge != NULL);
}

static UBOOL EdgesAreEqual(const FVector& Edge0_Vert0,const FVector& Edge0_Vert1,const FVector& Edge1_Vert0,const FVector& Edge1_Vert1, FLOAT Tolerance=5.0f)
{
	UBOOL bVert0Matches = (Edge0_Vert0.Equals(Edge1_Vert0,Tolerance) || Edge0_Vert0.Equals(Edge1_Vert1,Tolerance));
	UBOOL bVert1Matches = (Edge0_Vert1.Equals(Edge1_Vert0,Tolerance) || Edge0_Vert1.Equals(Edge1_Vert1,Tolerance));

	return bVert0Matches && bVert1Matches;
}

template< class T >
void UNavigationMeshBase::AddDynamicCrossPylonEdge( const FVector& inV1,
												   const FVector& inV2,
												   TArray<FNavMeshPolyBase*>& ConnectedPolys,
												   FLOAT SupportedEdgeWidth,
												   BYTE EdgeGroupID,
												   UBOOL bOneWay,
												   TArray<T*>* CreatedEdges,
												   VERTID Poly0Vert0Idx,
												   VERTID Poly0Vert1Idx,
												   VERTID Poly1Vert0Idx,
												   VERTID Poly1Vert1Idx)
{
	FNavMeshPolyBase* Poly0 = ConnectedPolys(0);
	FNavMeshPolyBase* Poly1 = ConnectedPolys(1);
	check(Poly0 != Poly1);
	FNavMeshCrossPylonEdge* NewEdge = NULL;

	UNavigationMeshBase* Mesh = Poly0->NavMesh;

	// see if the is already a dynamic edge from this poly to the destination
	TArray<FNavMeshCrossPylonEdge*> Edges;
	Mesh->DynamicEdges.MultiFind(Poly0->Item,Edges);
	for(INT EdgeIdx=0;EdgeIdx<Edges.Num();EdgeIdx++)
	{
		FNavMeshCrossPylonEdge* OtherEdge = Edges(EdgeIdx);

		// we already have an edge that does this, just use that one
		if(OtherEdge->GetOtherPoly(Poly0) == Poly1 && EdgesAreEqual(inV1,inV2,OtherEdge->GetVertLocation(0),OtherEdge->GetVertLocation(1)))
		{
			NewEdge = OtherEdge;
			break;
		}
	}
	
	APylon* Pylon0 = Poly0->NavMesh->GetPylon();
	APylon* Pylon1 = Poly1->NavMesh->GetPylon();


	if(NewEdge == NULL)
	{
		if ( Poly0Vert0Idx == MAXVERTID )
		{
			Poly0Vert0Idx = Poly0->NavMesh->AddDynamicVert(inV1,WORLD_SPACE);
		}

		if ( Poly0Vert1Idx == MAXVERTID )
		{
			Poly0Vert1Idx = Poly0->NavMesh->AddDynamicVert(inV2,WORLD_SPACE);
		}
	}
	else
	{
		Poly0Vert0Idx = NewEdge->Vert0;
		Poly0Vert1Idx = NewEdge->Vert1;
	}

	if(Poly1->NavMesh != Poly0->NavMesh)
	{
		if ( Poly1Vert0Idx == MAXVERTID )
		{
			Poly1Vert0Idx = Poly1->NavMesh->AddDynamicVert(inV1,WORLD_SPACE);
		}

		if( Poly1Vert1Idx == MAXVERTID )
		{
			Poly1Vert1Idx = Poly1->NavMesh->AddDynamicVert(inV2,WORLD_SPACE);
		}
	}
	else
	{
		Poly1Vert0Idx = Poly0Vert0Idx;
		Poly1Vert1Idx = Poly0Vert1Idx;
	}

	if(NewEdge == NULL)
	{	
		// ** add edge to pylon0

		// Add the edge to the pool
		NewEdge = new T(Mesh, 
			Poly0,Poly0Vert0Idx,Poly0Vert1Idx,
			Poly1,Poly1Vert0Idx,Poly1Vert1Idx);
		Mesh->DynamicEdges.AddUnique(Poly0->Item,NewEdge);

		// copy over supported width for this edge
		NewEdge->EffectiveEdgeLength = SupportedEdgeWidth;

		// copy over group ID
		NewEdge->EdgeGroupID = EdgeGroupID;


		NewEdge->SetPoly0( Poly0 );
		NewEdge->SetPoly1( Poly1 );
		NewEdge->UpdateEdgePerpDir();

		// MT-NOTE->dynamic edges are not linked to obstacle geo, pathing must take place through them

		if(CreatedEdges != NULL)
		{
			CreatedEdges->AddItem((T*)NewEdge);
		}

		if(Poly0 != Poly1)
		{
			Poly1->NavMesh->NotifyEdgeRefOfMesh(NewEdge);
		}
	}


	// ** add edge to pylon1
	if(!bOneWay && (NewEdge == NULL || !NewEdge->IsOneWayEdge()))
	{
		NewEdge = NULL;
		Mesh = Poly1->NavMesh;

		// see if the is already a dynamic edge from this poly to the destination
		Edges.Empty();
		Mesh->DynamicEdges.MultiFind(Poly1->Item,Edges);
		for(INT EdgeIdx=0;EdgeIdx<Edges.Num();EdgeIdx++)
		{
			FNavMeshCrossPylonEdge* OtherEdge = Edges(EdgeIdx);

			// we already have an edge that does this, just use that one
			if(OtherEdge->GetOtherPoly(Poly1) == Poly0 && EdgesAreEqual(inV1,inV2,OtherEdge->GetVertLocation(0),OtherEdge->GetVertLocation(1)))
			{
				NewEdge = OtherEdge;
				break;
			}
		}

		if(NewEdge == NULL)
		{
			// Add the edge to the pool
			NewEdge = 	new T(Mesh,
				Poly1,Poly1Vert0Idx,Poly1Vert1Idx,
				Poly0,Poly0Vert0Idx,Poly0Vert1Idx);

			// copy over supported width for this edge
			NewEdge->EffectiveEdgeLength = SupportedEdgeWidth;

			// copy over group ID
			NewEdge->EdgeGroupID = EdgeGroupID;

			Mesh->DynamicEdges.AddUnique(Poly1->Item,NewEdge);
			NewEdge->SetPoly0( Poly1 );
			NewEdge->SetPoly1( Poly0 );
			NewEdge->UpdateEdgePerpDir();

			// MT-NOTE->dynamic edges are not linked to obstacle geo, pathing must take place through them

			if(CreatedEdges != NULL)
			{
				CreatedEdges->AddItem((T*)NewEdge);
			}

			if(Poly1 != Poly0)
			{
				Poly0->NavMesh->NotifyEdgeRefOfMesh(NewEdge);
			}
		}
	}



}


FORCEINLINE UBOOL IsEdgeIDToIgnore(WORD EdgeID)
{
	return EdgeID == MAXWORD || EdgeID == MAXWORD-1;
}

#define NAVMESHTRACE_IgnoreLinklessEdges  0x00001  // use this to ignore edges linked to the obstacle mesh which are not valid (e.g. don't allow disabling of obstacle polys that have been disabled with edgeID MAXWORD)

/** This struct provides the interface into the skeletal mesh collision data */
class FNavMeshCollisionDataProvider
{
	/** The mesh that is being collided against */
	class UNavigationMeshBase* Mesh;
	/** The mesh that is linked to the obstacle mesh (the one AIs path on) */
	class UNavigationMeshBase* ForNavMesh;
	/** flags used to trace dilineation */
	DWORD TraceFlags;

	/** Hide default ctor */
	FNavMeshCollisionDataProvider(void)
	{
	}

public:
	/** Sets the component and mesh members */
	FORCEINLINE FNavMeshCollisionDataProvider(class UNavigationMeshBase* ObstacleNavMesh, class UNavigationMeshBase* InNavMesh, DWORD InTraceFlags) :
	  Mesh(ObstacleNavMesh)
	  ,ForNavMesh(InNavMesh)
	  ,TraceFlags(InTraceFlags)
	  {
	  }

	  FORCEINLINE const FVector& GetVertex(VERTID Index) const
	  {
		  return Mesh->Verts(Index);
	  }

	  FORCEINLINE UMaterialInterface* GetMaterial(WORD MaterialIndex) const
	  {
		  return NULL;
	  }

	  FORCEINLINE INT GetItemIndex(WORD MaterialIndex) const
	  {
		  return MaterialIndex;
	  }

	  /*FORCEINLINE*/ UBOOL ShouldCheckMaterial(INT MaterialIndex) const
	  {
		  // if this is not the obstacle mesh, always check
		  if(Mesh == ForNavMesh)
		  {
			  return TRUE;
		  }

		  // grab the poly from the OBSTACLE mesh that we just intersected with
		  FNavMeshPolyBase* LinkedPoly = Mesh->GetPolyFromId(MaterialIndex);
		  
		  // this can happen under normal circumstances (e.g. polys have been pulled out of the dynamicobstacle mesh on streamout but the full mesh hasn't been rebuilt yet)
		  if( LinkedPoly == NULL )
		  {
			return FALSE;
		  }

		  // if we have no linked edges, we need to collide
		  if(LinkedPoly->PolyEdges.Num()==0)
		  {
			  return TRUE;
		  }
		  else
		  {
			  // if this poly has any edges associated with it, see if the pylons they reference
			  // are both loaded

			  // special designator to disable this surface even if edges are not loaded/valid
			  if (!(TraceFlags & NAVMESHTRACE_IgnoreLinklessEdges))
			  {
				  for (INT EdgeIdx = 0; EdgeIdx < LinkedPoly->PolyEdges.Num(); EdgeIdx++)
				  {
					  if (IsEdgeIDToIgnore(LinkedPoly->PolyEdges(EdgeIdx)))
					  {
						  return FALSE;
					  }
				  }
			  }

			  if( ForNavMesh == NULL )
			  {
				  return TRUE;
			  }

			  // ** Disable collision IFF /ALL/ edges are loaded and valid.. otherwise leave it on
			  for(INT EdgeIdx=LinkedPoly->GetNumEdges()-1;EdgeIdx>=0;--EdgeIdx)
			  {
				  // EdgeID is from the NORMAL mesh
				  FNavMeshEdgeBase* LinkedEdge = LinkedPoly->GetEdgeFromIdx(EdgeIdx,ForNavMesh,TRUE);
				  if(LinkedEdge == NULL)
				  {
					  return TRUE;
				  }


//				  checkSlowish(LinkedEdge->IsCrossPylon());
				  FNavMeshCrossPylonEdge* CPEdge = (FNavMeshCrossPylonEdge*)LinkedEdge;

				  const FNavMeshPolyBase* RESTRICT Poly0 = CPEdge->GetPoly0();  
				  const FNavMeshPolyBase* RESTRICT Poly1 = CPEdge->GetPoly1();

				  // if both references aren't valid, then consider this poly as valid collision
				  if(Poly0 == NULL || Poly1 == NULL || Poly0->NavMesh==NULL || Poly1->NavMesh == NULL)
				  {
					  return TRUE;
				  }
				  // if either pylon is set to force collision then do so even if both poly refs are loaded and valid
				  else if( Poly0->NavMesh->GetPylon()->bForceObstacleMeshCollision || Poly1->NavMesh->GetPylon()->bForceObstacleMeshCollision )
				  {
					  return TRUE;
				  }

				  if( LinkedEdge->EdgeType == NAVEDGE_DropDown )
				  {
					  LinkedPoly->PolyEdges.RemoveSwap(EdgeIdx,1);
					  return TRUE;
				  }

			  }


			  return FALSE;
		  }
	  }

	  FORCEINLINE const TkDOPTree<FNavMeshCollisionDataProvider,WORD>& GetkDOPTree(void) const
	  {
		  return Mesh->KDOPTree;
	  }

	  FORCEINLINE const FMatrix& GetLocalToWorld(void) const
	  {
		  return Mesh->LocalToWorld;
	  }

	  FORCEINLINE const FMatrix& GetWorldToLocal(void) const
	  {
		  return Mesh->WorldToLocal;
	  }

	  FORCEINLINE FMatrix GetLocalToWorldTransposeAdjoint(void) const
	  {
		  return GetLocalToWorld().TransposeAdjoint();
	  }

	  FORCEINLINE FLOAT GetDeterminant(void) const
	  {
		  return GetLocalToWorld().Determinant();
	  }
};

inline DWORD GetTypeHash(const FCoverInfo& CoverInfo)
{
	return GetTypeHash(CoverInfo.Link) + CoverInfo.SlotIdx;
}

#endif // __PATH_H
