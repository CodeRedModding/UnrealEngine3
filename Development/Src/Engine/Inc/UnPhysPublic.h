/*=============================================================================
	UnPhysPublic.h
	Rigid Body Physics Public Types
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "NvApexManager.h"

/**
 * Physics stats
 */
enum EPhysicsStats
{
	STAT_RBTotalDynamicsTime = STAT_PhysicsFirstStat,
	STAT_SkelMesh_SetMeshTime,
	STAT_SkelMesh_SetPhysAssetTime,
	STAT_PhysicsKickOffDynamicsTime,
 	STAT_PhysicsFetchDynamicsTime,
 	STAT_PhysicsFluidMeshEmitterUpdate,
	STAT_PhysicsOutputStats,
	STAT_PhysicsEventTime,
	STAT_PhysicsWaitTime,
	STAT_RBSubstepTime,
	STAT_TotalSWDynamicBodies,
	STAT_AwakeSWDynamicBodies,
	STAT_SWSolverBodies,
	STAT_SWNumPairs,
	STAT_SWNumContacts,
	STAT_SWNumJoints,
	STAT_HWSolverBodies,
	STAT_HWNumPairs,
	STAT_HWNumContacts,
	STAT_HWNumConvexMeshes,
	STAT_HWSceneMemoryUsed,
	STAT_HWTempMemoryUsed,
	STAT_HWTimeIncludingGpuWait,
	STAT_NumSubsteps,
	STAT_RBBroadphaseUpdate,
	STAT_RBBroadphaseGetPairs,
	STAT_RBNearphase,
	STAT_RBSolver,
	STAT_NumConvexMeshes,
	STAT_NumTriMeshes,
	STAT_NovodexTotalAllocationSize,
	STAT_NovodexNumAllocations,
	STAT_NovodexAllocatorTime,
};

enum EPhysicsGpuMemStats
{
	STAT_PhysicsGpuMemTotal = STAT_PhysicsGpuMemFirstStat,
	STAT_PhysicsGpuMemFluid,
	STAT_PhysicsGpuMemClothSoftBody,
	STAT_PhysicsGpuMemShared,
};

enum EPhysicsFluidStats
{
	STAT_TotalFluids = STAT_PhysicsFluidFirstStat,
	STAT_TotalFluidEmitters,
	STAT_ActiveFluidParticles,
	STAT_TotalFluidParticles,
	STAT_TotalFluidPackets,
	STAT_PhysXEmitterVerticalSync,
	STAT_PhysXEmitterVerticalTick,
};

enum EPhysicsClothStats
{
	STAT_TotalCloths = STAT_PhysicsClothFirstStat,
	STAT_ActiveCloths,
	STAT_ActiveClothVertices,
	STAT_TotalClothVertices,
	STAT_ActiveAttachedClothVertices,
	STAT_TotalAttachedClothVertices,
};

enum EPhysicsFieldsStats
{
	STAT_RadialForceFieldTick = STAT_PhysicsFieldsFirstStat,
	STAT_CylindricalForceFieldTick,
	STAT_ProjectileForceFieldTick,
};

#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,PROPERTY_ALIGNMENT)
#endif

// These need to be public for UnrealEd etc.
const FLOAT P2UScale = 50.0f;
const FLOAT U2PScale = 0.02f;

const FLOAT Rad2U = 10430.2192f;
const FLOAT U2Rad = 0.000095875262f;

const FLOAT PhysSkinWidth = 0.025f;

#if WITH_NOVODEX
/** Used to know when cached physics data needs to be re-cooked due to a version increase. */
extern INT	GCurrentPhysXVersion;
// Touch this value if something in UE3 related to physics changes
extern BYTE	CurrentEpicPhysDataVersion;
// This value will auto-update as the SDK changes
extern INT	GCurrentCachedPhysDataVersion;

#if WITH_APEX
class FIApexScene;
#endif

#endif

/** Enum indicating different type of objects for rigid-body collision purposes. */
enum ERBCollisionChannel
{
	RBCC_Default			= 0,
	RBCC_Nothing			= 1, // Special channel that nothing should request collision with.
	RBCC_Pawn				= 2,
	RBCC_Vehicle			= 3,
	RBCC_Water				= 4,
	RBCC_GameplayPhysics	= 5,
	RBCC_EffectPhysics		= 6,
	RBCC_Untitled1			= 7,
	RBCC_Untitled2			= 8,
	RBCC_Untitled3			= 9,
	RBCC_Untitled4			= 10,
	RBCC_Cloth				= 11,
	RBCC_FluidDrain			= 12,
	RBCC_SoftBody			= 13,
	RBCC_FracturedMeshPart	= 14,
	RBCC_BlockingVolume		= 15,
	RBCC_DeadPawn			= 16,
	RBCC_Clothing           = 17,
	RBCC_ClothingCollision  = 18
};

/** 
 *	Container for indicating a set of collision channel that this object will collide with. 
 *	Mirrored manually in PrimitiveComponent.uc
 */
struct FRBCollisionChannelContainer
{
	union
	{
		struct
		{
			BITFIELD	Default:1;
			BITFIELD	Nothing:1;  // This is reserved to allow an object to opt-out of all collisions, and should not be set.
			BITFIELD	Pawn:1;
			BITFIELD	Vehicle:1;
			BITFIELD	Water:1;
			BITFIELD	GameplayPhysics:1;
			BITFIELD	EffectPhysics:1;
			BITFIELD	Untitled1:1;
			BITFIELD	Untitled2:1;
			BITFIELD	Untitled3:1;
			BITFIELD	Untitled4:1;
			BITFIELD	Cloth:1;
			BITFIELD	FluidDrain:1;
			BITFIELD	SoftBody:1;
			BITFIELD	FracturedMeshPart:1;
			BITFIELD	BlockingVolume:1;
			BITFIELD	DeadPawn:1;
			BITFIELD    Clothing:1;
			BITFIELD    ClothingCollision:1;
		};
		DWORD Bitfield;
	};

	/** Default constructor does nothing */
	FRBCollisionChannelContainer() {}

	/** This constructor will zero out the struct */
	FRBCollisionChannelContainer(INT);

	/** Set the status of a particular channel in the structure. */
	void SetChannel(ERBCollisionChannel Channel, UBOOL bNewState);
};

/** 
 *	Information about on 
 *	@see OnRigidBodyCollision
 */
struct FRigidBodyCollisionInfo
{
	/** Actor involved inthe collision */
	AActor*					Actor;

	/** Component of Actor involved in the collision. */
	UPrimitiveComponent*	Component;

	/** Index of body if this is in a PhysicsAsset. INDEX_NONE otherwise. */
	INT						BodyIndex;

	FRigidBodyCollisionInfo() :
		Actor(NULL),
		Component(NULL),
		BodyIndex(INDEX_NONE)
	{}

#if WITH_NOVODEX
	class NxActor* GetNxActor() const;
#endif
};

/** 
 *	Information about one contact between a pair of rigid bodies. 
 *	@see OnRigidBodyCollision
 */
struct FRigidBodyContactInfo
{
	/** Position of contact in world space, Unreal scale. */
	FVector					ContactPosition;

	/** Normal of contact point in world space, unit length. */
	FVector					ContactNormal;

	/** Penetration at contact point. */
	FLOAT					ContactPenetration;

	/** velocity at contact position for both objects at the time of the collision, Unreal scale
	 * element 0 is from 'This' object, element 1 is from 'other' object
	 */
	FVector ContactVelocity[2];

	/** 
	 *	PhysicalMaterials involved in collision. 
	 *	Element 0 is from 'This' object, element 1 is from 'Other' object.
	 */
	UPhysicalMaterial*		PhysMaterial[2];

	FRigidBodyContactInfo(	const FVector& InContactPosition, 
							const FVector& InContactNormal, 
							FLOAT InPenetration, 
							const FVector& InContactVelocity0, 
							const FVector& InContactVelocity1,
							UPhysicalMaterial* InPhysMat0, 
							UPhysicalMaterial* InPhysMat1 ) :	
		ContactPosition(InContactPosition), 
		ContactNormal(InContactNormal),
		ContactPenetration(InPenetration)
	{
		ContactVelocity[0] = InContactVelocity0;
		ContactVelocity[1] = InContactVelocity1;
		PhysMaterial[0] = InPhysMat0;
		PhysMaterial[1] = InPhysMat1;
	}

	/** Swap the order of info in this info  */
	void SwapOrder();
};

/////////////////  Convex support /////////////////


//Helper structure for the closest points between objects test
struct FSimplexVertex
{
	FVector Vertex;	    //Point returned on the simplex
	FVector VertexInA;  //Vertex from convex primitive A when doing a Minkowski Sum
	FVector VertexInB;	//Vertex from convex primitive B when doing a Minkowski Sum

	FSimplexVertex() : Vertex(0,0,0) , VertexInA(0,0,0) , VertexInB(0,0,0)
	{}
};

/* 
* An interface for any arbitrary convex shape to submit to a closest points between two objects test
* simply implement the function to return an explicit/implicit point on the shape in the further extent 
* along the given direction vector
*/
class IGJKHelper
{
public:

	/** 
	*  Get the vertex furthest on the shape in the direction specified
	*  @param	Direction - direction to find the further extent of the shape
	*  Returns Point of further extent on the shape (with Minkowski vertex information if applicable)
	*/
	virtual FSimplexVertex GetSupportingVertex(const FVector& Direction) = 0;

protected:
	IGJKHelper() {}
	~IGJKHelper() {}
};

/////////////////  FRBPhysScene /////////////////

/** Information about the collision as a whole, such as force and contact points, used by FCollisionNotifyInfo*/
struct FCollisionImpactData
{
	/** Array of information about each contact point between the two objects. */
	TArray<FRigidBodyContactInfo>	ContactInfos;

	/** The force of the two objects pushing on each other directly */
	FVector	TotalNormalForceVector;

	/** The force of the two objects sliding against each other */
	FVector TotalFrictionForceVector;

	FCollisionImpactData()
	{}
	FCollisionImpactData(EEventParm)
    {
		appMemzero(this, sizeof(FCollisionImpactData));
    }

	/** Iterate over ContactInfos array and swap order of information */
	void SwapContactOrders();
};
/** One entry in the array of collision notifications pending execution at the end of the physics engine run. */
struct FCollisionNotifyInfo
{
	/** If this notification should be called for the Actor in Info0. */
	UBOOL							bCallEvent0;

	/** If this notification should be called for the Actor in Info1. */
	UBOOL							bCallEvent1;

	/** Information about the first object involved in the collision. */
	FRigidBodyCollisionInfo			Info0;

	/** Information about the second object involved in the collision. */
	FRigidBodyCollisionInfo			Info1;

	/** Information about the collision itself */
	FCollisionImpactData			RigidCollisionData;

	FCollisionNotifyInfo() :
		bCallEvent0(FALSE),
		bCallEvent1(FALSE)
	{}

	/** Check that is is valid to call a notification for this entry. Looks at the bDeleteMe flags on both Actors. */
	UBOOL IsValidForNotify() const;
};

/** One entry in the array of 'push' notifications (when the sensor body overlaps a physics object). */
struct FPushNotifyInfo
{
	/** Pawn that owns the sensor and is doing the pushing. */
	class APawn*			Pusher;

	/** Actor that is being pushed. */
	FRigidBodyCollisionInfo	PushedInfo;

	/** Information about contact points between 'sensor' push body and physics body. */
	TArray<FRigidBodyContactInfo>	ContactInfos;
};


/** Container object for a physics engine 'scene'. */

class FRBPhysScene
{
public:
	/** Mapping between PhysicalMaterial class name and physics-engine specific MaterialIndex. */
	TMap<FName, UINT>				MaterialMap;

	/** List of materials not currently being used, that can be used again. */
	TArray<UINT>					UnusedMaterials;

	/** Array of collision notifications pending execution at the end of the physics engine run. */
	TArray<FCollisionNotifyInfo>	PendingCollisionNotifies;

	/** Array of 'push' notifies (overlaps with Pawn's 'push' sensor body). */
	TArray<FPushNotifyInfo>			PendingPushNotifies;
	
#if WITH_NOVODEX
	// Cloth velocity buffer opt
	TArray<FVector>					ClothVelocityScratch;

	/** Used to track the number of substeps for stats */
	INT								NumSubSteps;
	/** This is used to get the actual NxScene from the GNovodexSceneMap. */
	INT								NovodexSceneIndex;

	/** Utility for looking up the NxScene associated with this FRBPhysScene. */
	class NxScene*       GetNovodexPrimaryScene(void);

	/** Utility for looking up the NxApexScene associated with this FRBPhysScene. */
	physx::apex::NxApexScene*   GetApexScene(void);
	
	/** get the rigid-body compartment that is derived off the primary scene. */
	class NxCompartment* GetNovodexRigidBodyCompartment(void);
	
	/** get the fluid compartment that is derived off the primary scene. */
	class NxCompartment* GetNovodexFluidCompartment(void);
	
	/** get the cloth compartment that is derived off the primary scene. */
	class NxCompartment* GetNovodexClothCompartment(void);
	
	/** get the soft-body compartment that is derived off the primary scene. */
	class NxCompartment* GetNovodexSoftBodyCompartment(void);
   
   	/** Add any debug lines from the physics scene to the supplied line batcher. */
   	void AddNovodexDebugLines(class ULineBatchComponent* LineBatcherToUse);

	class FPhysXVerticalEmitter* PhysXEmitterManager;

	/** TRUE if double buffering is currently enabled. */
	UBOOL                           UsingBufferedScene;
	
	/** TRUE if the compartments are currently simulating. */
	UBOOL							CompartmentsRunning;

	INT								CompartmentFrameNumber;

#if WITH_APEX
	/** A pointer to the FIApexScene interface class. */
	FIApexScene						*ApexScene;
	/** Get the current FIApexScene interface */
	FIApexScene						*GetApexScene(void) const { return ApexScene; };
	/** Sets the current FIApexScene interface */
	void						    SetApexScene(FIApexScene *apexScene) { ApexScene = apexScene; };
#endif

#endif
   
   	FRBPhysScene()
#if WITH_NOVODEX
	:	NovodexSceneIndex( -1 )
	,PhysXEmitterManager(NULL)
	,UsingBufferedScene(FALSE)
	,CompartmentsRunning(FALSE)
	,CompartmentFrameNumber(0)
#endif
	{}

	void SetGravity(const FVector& NewGrav);
	UINT FindPhysMaterialIndex(UPhysicalMaterial* PhysMat);

	/** When a UPhysicalMaterial goes away, remove from mapping and discard NxMaterial. */
	void RemovePhysMaterial(UPhysicalMaterial* PhysMat);
};

FRBPhysScene*		CreateRBPhysScene(const FVector& Gravity);
void				DestroyRBPhysScene(FRBPhysScene* Scene);
void				TickRBPhysScene(FRBPhysScene* Scene, FLOAT DeltaTime);
void				DeferredReleaseNxJoint(class NxJoint* InJoint, UBOOL bFreezeBody);
void				DeferredReleaseNxActor(class NxActor* InActor, UBOOL bFreezeBody);
void				DeferredRBResourceCleanup(FRBPhysScene* PhysScene, UBOOL bCalledFromTick = FALSE);
/**
 * Waits for the physics scene to be done, fires events, and adds debug lines
 *
 * @param RBPhysScene - the scene to wait for processing to be done on
 */
void WaitRBPhysScene(FRBPhysScene* RBPhysScene);

/**
 * Waits for the physics scene to be done when using Compartments, fires events, and adds debug lines
 *
 * @param RBPhysScene - the scene to wait for processing to be done on
 */
void WaitPhysCompartments(FRBPhysScene* RBPhysScene);

/** 
 *	Call after WaitRBPhysScene to make deferred OnRigidBodyCollision calls. 
 *
 * @param RBPhysScene - the scene to process deferred collision events
 */
void DispatchRBCollisionNotifies(FRBPhysScene* RBPhysScene);

/////////////////  Script Structs /////////////////

struct FRigidBodyState
{
	FRigidBodyState() {}

	FRigidBodyState(INT)
	{
		Position = LinVel = AngVel = FVector(0,0,0);
		Quaternion = FQuat::Identity;
		bNewData = 0;
	}

	FVector		Position;
	FQuat		Quaternion;
	FVector		LinVel; // UCONST_RBSTATE_LINVELSCALE times actual (precision reasons)
	FVector		AngVel; // UCONST_RBSTATE_ANGVELSCALE times actual (precision reasons)
	BYTE		bNewData;
};


// ---------------------------------------------
//	Rigid-body relevant StaticMesh extensions
//	These have to be included in any build, preserve binary package compatibility.
// ---------------------------------------------

// Might be handy somewhere...
enum EKCollisionPrimitiveType
{
	KPT_Sphere = 0,
	KPT_Box,
	KPT_Sphyl,
	KPT_Convex,
	KPT_Unknown
};

// --- COLLISION ---
class FKSphereElem
{
public:
	FMatrix TM;
	FLOAT Radius;
	BITFIELD bNoRBCollision:1;
	BITFIELD bPerPolyShape:1;

	FKSphereElem() {}

	FKSphereElem( FLOAT r ) 
	: Radius(r) {}

	void	DrawElemWire(class FPrimitiveDrawInterface* PDI, const FMatrix& ElemTM, FLOAT Scale, const FColor Color);
	void	DrawElemSolid(class FPrimitiveDrawInterface* PDI, const FMatrix& ElemTM, FLOAT Scale, const FMaterialRenderProxy* MaterialRenderProxy);
	FBox	CalcAABB(const FMatrix& BoneTM, FLOAT Scale);

	/** Returns FALSE if there is a HIT (Unreal style) */
	UBOOL	LineCheck(FCheckResult& Result, const FMatrix& Matrix,  FLOAT Scale, const FVector& End, const FVector& Start, const FVector& Extent) const;
};

class FKBoxElem
{
public:
	FMatrix TM;
	FLOAT X, Y, Z; // Length (not radius) in each dimension
	BITFIELD bNoRBCollision:1;
	BITFIELD bPerPolyShape:1;

	FKBoxElem() {}

	FKBoxElem( FLOAT s ) 
	: X(s), Y(s), Z(s) {}

	FKBoxElem( FLOAT InX, FLOAT InY, FLOAT InZ ) 
	: X(InX), Y(InY), Z(InZ) {}	

	void	DrawElemWire(class FPrimitiveDrawInterface* PDI, const FMatrix& ElemTM, FLOAT Scale, const FColor Color);
	void	DrawElemSolid(class FPrimitiveDrawInterface* PDI, const FMatrix& ElemTM, FLOAT Scale, const FMaterialRenderProxy* MaterialRenderProxy);

	FBox	CalcAABB(const FMatrix& BoneTM, FLOAT Scale);

	/** Returns FALSE if there is a HIT (Unreal style) */
	UBOOL	LineCheck(FCheckResult& Result, const FMatrix& Matrix, FLOAT Scale, const FVector& End, const FVector& Start, const FVector& Extent, UBOOL bSkipCloseAndParallelChecks=FALSE) const;

	/** Returns FALSE if there is a HIT (Unreal style) */
	UBOOL	PointCheck(FCheckResult& Result, FLOAT& OutBestDistance, const FMatrix& BoxMatrix, FLOAT BoxScale, const FVector& Location, const FVector& Extent) const;
};

class FKSphylElem
{
public:
	FMatrix TM;
	FLOAT Radius;
	FLOAT Length;
	BITFIELD bNoRBCollision:1;
	BITFIELD bPerPolyShape:1;

	FKSphylElem() {}

	FKSphylElem( FLOAT InRadius, FLOAT InLength )
	: Radius(InRadius), Length(InLength) {}

	void	DrawElemWire(class FPrimitiveDrawInterface* PDI, const FMatrix& ElemTM, FLOAT Scale, const FColor Color);
	void	DrawElemSolid(class FPrimitiveDrawInterface* PDI, const FMatrix& ElemTM, FLOAT Scale, const FMaterialRenderProxy* MaterialRenderProxy);

	FBox	CalcAABB(const FMatrix& BoneTM, FLOAT Scale);

	/** Returns FALSE if there is a HIT (Unreal style) */
	UBOOL	LineCheck(FCheckResult& Result, const FMatrix& Matrix, FLOAT Scale, const FVector& End, const FVector& Start, const FVector& Extent) const;
};


/** One convex hull, used for simplified collision. */
class FKConvexElem
{
public:
	/** Array of indices that make up the convex hull. */
	TArrayNoInit<FVector>			VertexData;

	/** Array of planes holding the vertex data in SIMD order */
	TArrayNoInit<FPlane>			PermutedVertexData;

	/** Index buffer for triangles making up the faces of this convex hull. */
	TArrayNoInit<INT>				FaceTriData;

	/** All different directions of edges in this hull. */
	TArrayNoInit<FVector>			EdgeDirections;

	/** All different directions of face normals in this hull. */
	TArrayNoInit<FVector>			FaceNormalDirections;

	/** Array of the planes that make up this convex hull. */
	TArrayNoInit<FPlane>			FacePlaneData;

	/** Bounding box of this convex hull. */
	FBox							ElemBox;

	FKConvexElem() {}

	void	DrawElemWire(class FPrimitiveDrawInterface* PDI, const FMatrix& ElemTM, const FVector& Scale3D, const FColor Color);
	void	AddCachedSolidConvexGeom(TArray<FDynamicMeshVertex>& VertexBuffer, TArray<INT>& IndexBuffer, const FColor VertexColor);

	/** Reset the hull to empty all arrays */
	void	Reset();

	FBox	CalcAABB(const FMatrix& BoneTM, const FVector& Scale3D);

	/** Returns FALSE if there is a HIT (Unreal style) */
	UBOOL	LineCheck(FCheckResult& Result, const FMatrix& WorldToBox, const FVector& LocalEnd, const FVector& LocalStart, const FVector& BoxExtent, UBOOL bSkipCloseAndParallelChecks=FALSE) const;

	/** Returns FALSE if there is a HIT (Unreal style) */
	UBOOL	PointCheck(FCheckResult& Result, FLOAT& OutBestDistance, const FMatrix& WorldToBox, const FVector& LocalLocation, const FVector& BoxExtent) const;

	/** 
	*	Utility that determines if a point is completely contained within this convex hull. 
	*	If it returns TRUE (the point is within the hull), will also give you the normal of the nearest surface, and distance from it.
	*/
	UBOOL	PointIsWithin(const FVector& LocalLocation, FVector& OutLocalNormal, FLOAT& OutBestDistance) const;

	/** Returns TRUE if this convex hull is entirely above the supplied plane. */
	UBOOL	IsOutsidePlane(const FPlane& Plane);

	/** Process the VertexData array to generate FaceTriData, EdgeDirections etc. Will modify existing VertexData. */
	UBOOL	GenerateHullData();

	/** Utility for creating a convex hull from a set of planes. Will reset current state of this elem. */
	UBOOL	HullFromPlanes(const TArray<FPlane>& InPlanes, const TArray<FVector>& SnapVerts);

	/** Creates a copy of the vertex data in SIMD ready form */
	void	PermuteVertexData(void);

	/** Cut away the part of this hull that is in front of this plane. */
	void	SliceHull(const FPlane& SlicePlane);

	/** Calculate the surface area and volume. */
	void	CalcSurfaceAreaAndVolume( FLOAT & Area, FLOAT & Volume ) const;

	/** See if the supplied vector is parallel to one of the edge directions in the convex element. */
	UBOOL	DirIsFaceEdge(const FVector& InDir) const;


	friend FArchive& operator<<(FArchive& Ar,FKConvexElem& Elem)
	{
		if (Ar.IsLoading())
		{
			// Initialize the TArrayNoInit members
			appMemzero(&Elem, sizeof(FKConvexElem));
		}

		Ar << Elem.VertexData;
		Ar << Elem.PermutedVertexData;
		Ar << Elem.FaceTriData;
		Ar << Elem.EdgeDirections;
		Ar << Elem.FaceNormalDirections;
		Ar << Elem.FacePlaneData;
		Ar << Elem.ElemBox;
		return Ar;
	}
};

/** Cooked physics information for a single convex mesh. */
class FKCachedConvexDataElement
{
public:
	/** Cooked data stream for physics engine for one convex hull. */
	TArray<BYTE>	ConvexElementData;

	friend FArchive& operator<<( FArchive& Ar, FKCachedConvexDataElement& S )
	{
		S.ConvexElementData.BulkSerialize(Ar);
		return Ar;
	}
};

/** Intermediate cooked data from the physics engine for a collection of convex hulls. */
class FKCachedConvexData
{
public:
	/** Array of cooked physics data - one element for each convex hull.*/
	TArray<FKCachedConvexDataElement>	CachedConvexElements;

	friend FArchive& operator<<( FArchive& Ar, FKCachedConvexData& S )
	{
		Ar << S.CachedConvexElements;
		return Ar;
	}
};

/** Intermediate cooked data from the physics engine for a triangle mesh. */
class FKCachedPerTriData
{
public:
	/** Cooked data for physics engine representing a mesh at a particular scale. */
	TArray<BYTE>	CachedPerTriData;

	friend FArchive& operator<<( FArchive& Ar, FKCachedPerTriData& S )
	{
		S.CachedPerTriData.BulkSerialize(Ar);
		return Ar;
	}
};


class FConvexCollisionVertexBuffer : public FVertexBuffer 
{
public:
	TArray<FDynamicMeshVertex> Vertices;

	virtual void InitRHI();
};

class FConvexCollisionIndexBuffer : public FIndexBuffer 
{
public:
	TArray<INT> Indices;

	virtual void InitRHI();
};

class FConvexCollisionVertexFactory : public FLocalVertexFactory
{
public:

	FConvexCollisionVertexFactory()
	{}

	/** Initialization constructor. */
	FConvexCollisionVertexFactory(const FConvexCollisionVertexBuffer* VertexBuffer)
	{
		InitConvexVertexFactory(VertexBuffer);
	}


	void InitConvexVertexFactory(const FConvexCollisionVertexBuffer* VertexBuffer);
};

class FKConvexGeomRenderInfo
{
public:
	FConvexCollisionVertexBuffer* VertexBuffer;
	FConvexCollisionIndexBuffer* IndexBuffer;
	FConvexCollisionVertexFactory* CollisionVertexFactory;
};


/** 
 *	Describes the collision geometry used by the rigid body physics. This a collection of primitives, each with a
 *	transformation matrix from the mesh origin.
 */
class FKAggregateGeom
{
public:
	TArrayNoInit<FKSphereElem>		SphereElems;
	TArrayNoInit<FKBoxElem>			BoxElems;
	TArrayNoInit<FKSphylElem>		SphylElems;
	TArrayNoInit<FKConvexElem>		ConvexElems;
	FKConvexGeomRenderInfo*			RenderInfo;
	BITFIELD						bSkipCloseAndParallelChecks:1;
	
	FKAggregateGeom() {}

public:

	INT GetElementCount()
	{
		return SphereElems.Num() + SphylElems.Num() + BoxElems.Num() + ConvexElems.Num();
	}

	void EmptyElements()
	{
		BoxElems.Empty();
		ConvexElems.Empty();
		SphylElems.Empty();
		SphereElems.Empty();
	}

#if WITH_NOVODEX
	class NxActorDesc*	InstanceNovodexGeom(const FVector& uScale3D, FKCachedConvexData* InCacheData, UBOOL bCreateCCDSkel, const TCHAR* debugName);
	void InstanceNovodexForceField(class UserForceFieldShapeGroup& ffsGroup, const FVector& uScale3D, FKCachedConvexData* InCacheData, UBOOL bCreateCCDSkel, const TCHAR* debugName);
#endif // WITH_NOVODEX

	void	DrawAggGeom(class FPrimitiveDrawInterface* PDI, const FMatrix& ParentTM, const FVector& Scale3D, const FColor Color, const FMaterialRenderProxy* MatInst, UBOOL bPerHullColor, UBOOL bDrawSolid);

	/** Release the RenderInfo (if its there) and safely clean up any resources. Call on the game thread. */
	void	FreeRenderInfo();

	FBox	CalcAABB(const FMatrix& BoneTM, const FVector& Scale3D);

	/**
	  * Calculates a tight box-sphere bounds for the aggregate geometry; this is more expensive than CalcAABB
	  * (tight meaning the sphere may be smaller than would be required to encompass the AABB, but all individual components lie within both the box and the sphere)
	  *
	  * @param Output The output box-sphere bounds calculated for this set of aggregate geometry
	  *	@param BoneTM Transform matrix
	  *	@param Scale3D Scale vector
	  */
	void	CalcBoxSphereBounds(FBoxSphereBounds& Output, const FMatrix& BoneTM, const FVector& Scale3D);

	/** Returns FALSE if there is a HIT (Unreal style) */
	UBOOL	LineCheck(FCheckResult& Result, const FMatrix& Matrix, const FVector& Scale3D, const FVector& End, const FVector& Start, const FVector& Extent, UBOOL bStopAtAnyHit, UBOOL bPerPolyShapes) const;

	/** Returns FALSE if there is a HIT (Unreal style) */
	UBOOL	PointCheck(FCheckResult& Result, const FMatrix& Matrix, const FVector& Scale3D, const FVector& Location, const FVector& Extent) const;
	

	/*GJKResult*/ BYTE ClosestPointOnAggGeomToComponent(const FMatrix& LocalToWorld, class UPrimitiveComponent*& OtherComponent, FVector& PointOnComponentA, FVector& PointOnComponentB);

	/*GJKResult*/ BYTE ClosestPointOnAggGeomToPoint(const FMatrix& LocalToWorld, IGJKHelper* ExtentHelper, FVector& OutPointA, FVector& OutPointB);

};

// This is the mass data (inertia tensor, centre-of-mass offset) optionally saved along with the graphics.
// This applies to the mesh at default scale, and with a mass of 1.
// This is in PHYSICS scale.

class UKMeshProps : public UObject
{
	DECLARE_CLASS_NOEXPORT(UKMeshProps,UObject,0,Engine);

	FVector				COMNudge;
	FKAggregateGeom		AggGeom;

	UKMeshProps() {}

	// UObject interface
	void Serialize( FArchive& Ar );

	// UKMeshProps interface
	void	CopyMeshPropsFrom(UKMeshProps* fromProps);
};

void	InitGameRBPhys();
void	DestroyGameRBPhys();

/**
 * Converts a UModel to a set of convex hulls for.  Any convex elements already in
 * outGeom will be destroyed.  WARNING: the input model can have no single polygon or
 * set of coplanar polygons which merge to more than FPoly::MAX_VERTICES vertices.
 *
 * @param		outGeom					[out] The resultion collision geometry.
 * @param		inModel					The input BSP.
 * @param		deleteContainedHulls	[in] Delete existing convex hulls
 * @return								TRUE on success, FALSE on failure because of vertex count overflow.
 */
UBOOL	KModelToHulls(FKAggregateGeom* outGeom, UModel* inModel, UBOOL deleteContainedHulls=TRUE );

/**
 *	Take an FKAggregateGeom and cook the convex hull data into a FKCachedConvexData for a particular scale.
 *	This is a slow process, so is best done off-line if possible, and the results stored.
 *
 *	@param		OutCacheData	Will be filled with cooked data, one entry for each convex hull.
 *	@param		ConvexElems		The input aggregate geometry. Each convex element will be cooked.
 *	@param		Scale3D			The 3D scale that the geometry should be cooked at.
 *	@param		debugName		Debug name string, used for printing warning messages and the like.
 */
void	MakeCachedConvexDataForAggGeom(FKCachedConvexData* OutCacheData, const TArray<FKConvexElem>& ConvexElems, const FVector& Scale3D, const TCHAR* debugName);

/**
 *	Take a UStaticMesh and cook the per-tri data into an OutCacheData for a particular scale.
 *	This is a slow process, so is best done off-line if possible, and the results stored.
 */
void	MakeCachedPerTriMeshDataForStaticMesh(FKCachedPerTriData* OutCacheData, class UStaticMesh* InMesh, const FVector& Scale3D, const TCHAR* DebugName);

UBOOL	ExecRBCommands(const TCHAR* Cmd, FOutputDevice* Ar);

/** Util to list to log all currently awake rigid bodies */
void	ListAwakeRigidBodies(UBOOL bIncludeKinematic);

/** Change the global physics-data cooking mode to cook to Xenon target. */
void	SetPhysCookingXenon();

/** Change the global physics-data cooking mode to cook to PS3 target. */
void	SetPhysCookingPS3();

enum ECookedPhysicsDataEndianess
{
	CPDE_Unknown,
	CPDE_LittleEndian,
	CPDE_BigEndian
};

/** Utility to determine the endian-ness of a set of cooked physics data. */
ECookedPhysicsDataEndianess	GetCookedPhysDataEndianess(const TArray<BYTE>& InData);

FMatrix FindBodyMatrix(AActor* Actor, FName BoneName);
FBox	FindBodyBox(AActor* Actor, FName BoneName);

/** Given a set of AABBs, returns all overlapping pairs.  Each pair is packed into a DWORD, with indices
	in the high and low WORDs.  Each index refers to the AABBs array.
 */
struct FIntPair	{ INT A, B; };	// Type for return data
void OverlapAABBs( const TArray<FBox> & AABBs, TArray<FIntPair> & Pairs );

UBOOL ConvexOverlap( FKConvexElem & Elem1, const FMatrix & LocalToWorld1, const FVector & Scale3D1,
					 FKConvexElem & Elem2, const FMatrix & LocalToWorld2, const FVector & Scale3D2,
					 FLOAT Padding );



/*
 *  GJK Support Vertex Implementation for a generic convex hull object
 */
class GJKHelperConvex : public IGJKHelper
{

public:

	GJKHelperConvex(const FKConvexElem& ConvexHull, const FMatrix& LocalToWorld)
	{
		// Transform all convex verts into world space
		WorldVertices.Add(ConvexHull.VertexData.Num());
		for(INT i=0; i<ConvexHull.VertexData.Num(); i++)
		{
			WorldVertices(i) = LocalToWorld.TransformFVector( ConvexHull.VertexData(i) );
		}

		CachedVertexIndex = 0;

		//Make vertex adjacency data
		INT VertCount = ConvexHull.VertexData.Num();
		INT TriCount = ConvexHull.FaceTriData.Num() / 3;

		VertexAdjacencyMap.Empty(VertCount);
		VertexAdjacencyMap.AddZeroed(VertCount);

		for(INT i=0; i<TriCount; i++)
		{
			const INT I0 = ConvexHull.FaceTriData((i*3)+0);
			const INT I1 = ConvexHull.FaceTriData((i*3)+2);
			const INT I2 = ConvexHull.FaceTriData((i*3)+1);

			VertexAdjacencyMap(I0).AddUniqueItem(I1);
			VertexAdjacencyMap(I0).AddUniqueItem(I2);

			VertexAdjacencyMap(I1).AddUniqueItem(I0);
			VertexAdjacencyMap(I1).AddUniqueItem(I2);

			VertexAdjacencyMap(I2).AddUniqueItem(I0);
			VertexAdjacencyMap(I2).AddUniqueItem(I1);
		}
	}

	/** Get the vertex furthest on the shape in the direction specified **/
	virtual FSimplexVertex GetSupportingVertex(const FVector& Direction)
	{
		//Hill Climbing
		UBOOL bIsOptimal;

		FVector CurrentVertex;
		FVector CachedVertex = WorldVertices(CachedVertexIndex);

		FLOAT CurDotProd;
		FLOAT MaxDotProd = (CachedVertex | Direction);
		INT FailSafeCount = 0;
		do 
		{
			bIsOptimal = TRUE;
			const TArray<INT>& AdjVertices = VertexAdjacencyMap(CachedVertexIndex);
			for (INT i=0; i<AdjVertices.Num(); i++)
			{
				CurrentVertex = WorldVertices(AdjVertices(i));
				CurDotProd = (CurrentVertex | Direction);
				//Make sure we are making progress before we mark a new vertex
				if (CurDotProd - MaxDotProd > 0.0001f) 
				{
					CachedVertexIndex = AdjVertices(i);
					CachedVertex = WorldVertices(CachedVertexIndex);
					MaxDotProd = (CachedVertex | Direction);
					FailSafeCount++;
					bIsOptimal = FALSE;
				}
			}
		} while(!bIsOptimal && FailSafeCount < 500);

		FSimplexVertex Result;
		Result.Vertex = WorldVertices(CachedVertexIndex);
		return Result;
	}

private:

	INT CachedVertexIndex;
	TArray<FVector> WorldVertices;

	/** Mapping of vertex index to adjacent vertices **/
	TArray< TArray<INT> > VertexAdjacencyMap;
};

/*
 *  GJK Support Vertex Implementation for an implicit sphere
 */
class GJKHelperSphere : public IGJKHelper
{
public:
	GJKHelperSphere(const FKSphereElem& SphereElem, const FMatrix& LocalToWorld)
	{
		FMatrix ElemTM = SphereElem.TM;
		ElemTM *= LocalToWorld;

		//Assume uniform scaling
		FLOAT Scale = FVector(LocalToWorld.TransformNormal(FVector(1,0,0))).Size();

		SphereCenter = ElemTM.GetOrigin();
		SphereRadius = SphereElem.Radius * Scale;
	}

	/** Get the vertex furthest on the shape in the direction specified **/
	virtual FSimplexVertex GetSupportingVertex(const FVector& Direction)
	{
		FSimplexVertex Result;
		const FVector& NormDir = Direction.SafeNormal();
		Result.Vertex = SphereCenter + SphereRadius * NormDir;
		return Result;
	}

private:

	FVector SphereCenter;
	FLOAT   SphereRadius;
};

/*
 *  GJK Support Vertex Implementation for the minkowski sum of two convex primitives
 */
class GJKHelperMinkowski : public IGJKHelper
{
public:
	GJKHelperMinkowski(IGJKHelper* ConvexObjA, IGJKHelper* ConvexObjB)
	{
		ConvexA = ConvexObjA;
		ConvexB = ConvexObjB;
	}

	/** Get the vertex furthest on the shape in the direction specified **/
	virtual FSimplexVertex GetSupportingVertex(const FVector& Direction)
	{
		FSimplexVertex Result;

		// Minkowski Support = SuppA(D) - SuppB(-D)
		Result.VertexInA = ConvexA->GetSupportingVertex(Direction).Vertex;
		Result.VertexInB = ConvexB->GetSupportingVertex(-Direction).Vertex;
		Result.Vertex = Result.VertexInA - Result.VertexInB;

		return Result;
	}

private:

	IGJKHelper* ConvexA;
	IGJKHelper* ConvexB;
};

/*
 *  GJK Support Vertex Implementation for an implicit cylinder
 */
class GJKHelperCylinder : public IGJKHelper
{
public:
	//assumes aligned with z axis in world space
	GJKHelperCylinder(const FLOAT HeightIn, const FLOAT RadiusIn, const FMatrix& LocalToWorld)
	{
		FLOAT Scale = FVector(LocalToWorld.TransformNormal(FVector(1,0,0))).Size();

		Height = HeightIn * Scale;
		Radius = RadiusIn * Scale;
		Position = LocalToWorld.GetOrigin();
	}

	GJKHelperCylinder(const FCylinder& Cylinder, const FMatrix& LocalToWorld)
	{
		FLOAT Scale = FVector(LocalToWorld.TransformNormal(FVector(1,0,0))).Size();

		Height = Cylinder.Height * Scale;
		Radius = Cylinder.Radius * Scale;
		Position = LocalToWorld.GetOrigin();
	}

	virtual FSimplexVertex GetSupportingVertex(const FVector& Direction)
	{
		FSimplexVertex Result;
		static FLOAT GridSnapTol = 0.01f;

#if 1
		//Snap the direction to quantize results (helps implicit surfaces)
		const FVector& NewThing = Direction.SafeNormal().GridSnap(GridSnapTol);
		const FVector& NormDir = NewThing.SafeNormal2D();
		Result.Vertex.Set(Position.X + Radius * NormDir.X, Position.Y + Radius * NormDir.Y, Direction.Z > 0.0f ? Position.Z + Height : Position.Z);
		return Result;
#else
		const FVector& NormDir = NewThing.SafeNormal2D();
		const FVector TempVec(Position.X + Radius * NormDir.X, Position.Y + Radius * NormDir.Y, Direction.Z > 0.0f ? Position.Z + Height : Position.Z);

		//Snap the result to quantize results (helps implicit surfaces)
		Result.Vertex = TempVec.GridSnap(GridSnapTol);
		return Result;
#endif
	}

	FLOAT Height;
	FLOAT Radius;
	FVector Position;
};

/*
 *  GJK Support Vertex Implementation for a point in world space
 */
class GJKHelperPoint : public IGJKHelper
{
public:
	//assumes aligned with z axis in world space
	GJKHelperPoint(const FVector& PointIn)
	{
		Point.Vertex = PointIn;
	}

	virtual FSimplexVertex GetSupportingVertex(const FVector& Direction)
	{
		return Point;
	}

private:

	FSimplexVertex Point;
};

/*
 *  GJK Support Vertex Implementation for any box shape
 */
class GJKHelperBox : public IGJKHelper
{
public:
	GJKHelperBox(const FKBoxElem& BoxElem, const FMatrix& LocalToWorld)
	{
		FVector Radii;
		// X,Y,Z box member variables are LENGTH not RADIUS
		Radii.X = 0.5f * BoxElem.X;
		Radii.Y = 0.5f * BoxElem.Y;
		Radii.Z = 0.5f * BoxElem.Z;

		FMatrix ElemTM = BoxElem.TM;

		FVector LocalBoxPoints[8] = {	FVector(Radii.X, Radii.Y, Radii.Z),
										FVector(Radii.X, -Radii.Y, Radii.Z),
										FVector(-Radii.X, Radii.Y, Radii.Z),
										FVector(-Radii.X, -Radii.Y, Radii.Z),
										FVector(Radii.X, Radii.Y, -Radii.Z),
										FVector(Radii.X, -Radii.Y, -Radii.Z),
										FVector(-Radii.X, Radii.Y, -Radii.Z),
										FVector(-Radii.X, -Radii.Y, -Radii.Z) };

		//Transform the box into world space
		ElemTM *= LocalToWorld;
		for (INT i=0; i<8; i++)
		{
			BoxPoints[i] = ElemTM.TransformFVector(LocalBoxPoints[i]);
		}
	}

	GJKHelperBox(const FOrientedBox& Box)
	{
	   Box.CalcVertices(BoxPoints);
	}

	/** Get the vertex furthest on the shape in the direction specified **/
	FSimplexVertex GetSupportingVertex(const FVector& Direction)
	{
		FSimplexVertex Result;

		//Calculate all dot products
		FLOAT DotProds[8];
		for (INT i=0; i<8; i++)
		{
			DotProds[i] = Direction | BoxPoints[i]; 
		}

		//Iterate over all points and return the one whose VdotD is greatest
		INT MaxIndex = 0;
		for (INT i=1; i<8; i++)
		{
			if (DotProds[i] > DotProds[MaxIndex])
			{
				MaxIndex = i;
			}
		}

		Result.Vertex = BoxPoints[MaxIndex];
		return Result;
	}

private:

	FVector BoxPoints[8];
};

/*-----------------------------------------------------------------------------
FSimplex - a simplex in n dimensional space comprised of n+1 points
	0-D -> Point
	1-D -> Line
	2-D -> Triangle
	3-D -> Tetrahedron
-----------------------------------------------------------------------------*/
class FSimplex
{
public:

	FSimplex() {}
	~FSimplex() {}

	/*
	 *  Initialize the simplex (convex hull case)
	 *  @param	Dimensions - dimensionality of this simplex // UNUSED
	 *  @param	GJKHelper - an interface that can provide points on a convex primitive
	 */
	void Init(INT Dimensions, IGJKHelper* GJKHelper);

	/*
	 *  Returns the point on the simplex
	 *  closest to the given point in world space
	 *  @param	Point - a point in world coordinates
	 *  Returns a struct containing the point closest on the simplex (and Minkowski verts if applicable)
	 */
	FSimplexVertex ComputeMinimumNorm(const FVector& Point);

	/*
	 *  Given a point on the simplex, possibly reduce the simplex to the minimum set 
	 *  of points necessary to describe the given point
	 *	@param	Point - a point in world coordinates
	 */
	void Reduce(const FSimplexVertex& Point);

	/* 
	 *  Increase the simplex dimension by using this additional point
	 *	@param	Point - a point in world coordinates
	 */
	void Increase(const FSimplexVertex& Point);

	//The vertices that make up the current simplex
	TArray<FSimplexVertex> Vertices;

	//Current state of the barycentric coordinates from the last call to Reduce()
	FVector4 BaryCoords;
};

enum GJKResult
{
	GJK_Intersect, 
	GJK_NoIntersection, 
	GJK_Fail
};

/**
* Calculates the closest point on a given convex primitive to a point given
* @param POI - Point in world space to determine closest point to
* @param Primitive - Convex primitive 
* @param OutPoint - Point on primitive closest to POI given 
* 
* @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
*/
GJKResult ClosestPointOnConvexPrimitive(const FVector& POI, IGJKHelper* Primitive, FVector& OutPoint);

/**
* Calculates the closest point on a given convex primitive to another given convex primitive
* @param PrimitiveA - Convex primitive
* @param PrimitiveB - Convex primitive  
* @param OutPointA - Point on primitive A closest to primitive B
* @param OutPointB - Point on primitive B closest to primitive A
* 
* @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
*/
GJKResult ClosestPointsBetweenConvexPrimitives(IGJKHelper* PrimitiveA, IGJKHelper* PrimitiveB, FVector& OutPointA, FVector& OutPointB);

#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif
