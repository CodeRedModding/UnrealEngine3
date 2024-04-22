/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef UNFRACTUREDSTATICMESH_H
#define UNFRACTUREDSTATICMESH_H

#include "UnStaticMeshLight.h"

/** 
 * Version used to track when UFracturedStaticMesh's need to be rebuilt.  
 * This does not actually force a rebuild on load unlike STATICMESH_VERSION.
 * Instead, it provides a mechanism to warn content authors when a non-critical FSM rebuild is needed.
 * Do not use this version for native serialization backwards compatibility, instead use VER_LATEST_ENGINE/VER_LATEST_ENGINE_LICENSEE.
 */
extern const WORD FSMNonCriticalBuildVersion;
extern const WORD LicenseeFSMNonCriticalBuildVersion;

/**
 * Stores information about a fragment of the mesh that can be hidden
 */
struct FFragmentInfo
{
	/** Center used to generate this voronoi region. */
	FVector Center;
	/** Convex hull used to cut out this chunk. */
	FKConvexElem ConvexHull;
	/** Bounds of the grpahics verts of this chunk. */
	FBoxSphereBounds Bounds;
	/** Neighbours of this chunk - corresponds to planes in ConvexHull.FacePlaneData, and needs to match its size. 255 indicates an exterior face or invalid neighbour. */
	TArrayNoInit<BYTE> Neighbours;
	/** Size of connection to neighbour. Also needs to be same size as Neighbours and ConvexHull.FacePlaneData. */
	TArrayNoInit<FLOAT> NeighbourDims;
	/** If chunk can be destroyed when shot. */
	UBOOL bCanBeDestroyed;
	/** Used when finding unsupported islands of chunks to break off. */
	UBOOL bRootFragment;
	/** If TRUE, this piece never spawns a physics chunk. */
	UBOOL bNeverSpawnPhysicsChunk;
	/** Average normal of 'exterior' triangles of this chunk. */
	FVector AverageExteriorNormal;

	FFragmentInfo() {}

	FFragmentInfo(	FVector InCenter, 
					const FKConvexElem& InConvexHull, 
					const TArray<BYTE>& InNeighbours, 
					const TArray<FLOAT>& InNeighbourDims, 
					UBOOL bInCanBeDestroyed, 
					UBOOL bInRootFragment, 
					UBOOL bInNeverSpawnPhysicsChunk, 
					const FVector& InAverageExteriorNormal) :
		Center(InCenter),
		ConvexHull(InConvexHull),
		Bounds(InConvexHull.ElemBox),
		bCanBeDestroyed(bInCanBeDestroyed),
		bRootFragment(bInRootFragment),
		bNeverSpawnPhysicsChunk(bInNeverSpawnPhysicsChunk),
		AverageExteriorNormal(InAverageExteriorNormal)
	{
		appMemzero(&Neighbours, sizeof(TArray<BYTE>));
		Neighbours = InNeighbours;

		appMemzero(&NeighbourDims, sizeof(TArray<FLOAT>));
		NeighbourDims = InNeighbourDims;
	}

	friend FArchive& operator<<(FArchive& Ar,FFragmentInfo& FragmentInfo)
	{
		if (Ar.IsLoading())
		{
			// Initialize the TArrayNoInit members
			appMemzero(&FragmentInfo, sizeof(FFragmentInfo));
		}

		Ar << FragmentInfo.Center;
		Ar << FragmentInfo.ConvexHull;
		Ar << FragmentInfo.Bounds;
		
		if(Ar.Ver() >= VER_FRAGMENT_NEIGHBOUR_INFO)
		{
			Ar << FragmentInfo.Neighbours;
		}

		if(Ar.Ver() >= VER_FRAGMENT_DESTROY_FLAGS)
		{
			Ar << FragmentInfo.bCanBeDestroyed;
			Ar << FragmentInfo.bRootFragment;
		}
		else if(Ar.IsLoading())
		{
			// Old data is all destroyable, no root frags.
			FragmentInfo.bCanBeDestroyed = TRUE;
			FragmentInfo.bRootFragment = FALSE;
		}

		if(Ar.Ver() >= VER_FRACTURE_CORE_ROTATION_PERCHUNKPHYS)
		{
			Ar << FragmentInfo.bNeverSpawnPhysicsChunk;
		}
		else if(Ar.IsLoading())
		{
			FragmentInfo.bNeverSpawnPhysicsChunk = FALSE;
		}

		if(Ar.Ver() >= VER_FRAGMENT_EXT_NORMAL_NEIGH_DIM)
		{
			Ar << FragmentInfo.AverageExteriorNormal;
			Ar << FragmentInfo.NeighbourDims;
		}
		else if(Ar.IsLoading())
		{
			// Zero out exterior normal in old versions
			FragmentInfo.AverageExteriorNormal = FVector(0,0,0);

			// Just use 1.0 for old content neighbour dims
			INT NumNeigh = FragmentInfo.Neighbours.Num();
			FragmentInfo.NeighbourDims.Add(NumNeigh);
			for(INT i=0; i<NumNeigh; i++)
			{
				FragmentInfo.NeighbourDims(i) = 1.f;
			}
		}

		return Ar;
	}
};

/**
 *	Contains pre-build raw triangles that will correspond to a fragment in the built mesh
 *	Temp structure used in editor/tool - not saved to disk
 */
struct FRawFragmentInfo
{
	/** Center used to generate this voronoi region. */
	FVector Center;
	/** Convex hull used to cut out this chunk. */
	FKConvexElem ConvexHull;
	/** Raw input triangles making up the graphics of this fragment. */
	TArray<FStaticMeshTriangle> RawTriangles;
	/** Neighbours of this chunk - corresponds to planes in ConvexHull.FacePlaneData, and needs to match its size. 255 indicates an exterior face or invalid neighbour. */
	TArray<BYTE> Neighbours;
	/** Size of connection to neighbour. Also needs to be same size as Neighbours and ConvexHull.FacePlaneData. */
	TArray<FLOAT> NeighbourDims;
	/** If chunk can be destroyed when shot. */
	UBOOL bCanBeDestroyed;
	/** Used when finding unsupported islands of chunks to break off. */
	UBOOL bRootFragment;
	/** If TRUE, this piece never spawns a physics chunk. */
	UBOOL bNeverSpawnPhysicsChunk;
	/** Average normal of 'exterior' triangles of this chunk. */
	FVector AverageExteriorNormal;

	FRawFragmentInfo(	FVector InCenter, 
						const FKConvexElem& InConvexHull, 
						const TArray<INT>& InNeighbours, 
						const TArray<FLOAT>& InNeighbourDims, 
						UBOOL bInCanBeDestroyed, 
						UBOOL bInRootFragment, 
						UBOOL bInNeverSpawnPhysicsChunk,
						const FVector& InAverageExteriorNormal, 
						const TArray<FStaticMeshTriangle>& InRawTriangles) :
		Center(InCenter),
		ConvexHull(InConvexHull),
		RawTriangles(InRawTriangles),
		bCanBeDestroyed(bInCanBeDestroyed),
		bRootFragment(bInRootFragment),
		bNeverSpawnPhysicsChunk(bInNeverSpawnPhysicsChunk),
		AverageExteriorNormal(InAverageExteriorNormal)
	{
		check(InConvexHull.FacePlaneData.Num() == InNeighbours.Num());
		check(InNeighbourDims.Num() == InNeighbours.Num());

		// Convert and copy neighbours table
		appMemzero(&Neighbours, sizeof(TArray<BYTE>));
		Neighbours.AddZeroed(InNeighbours.Num());
		for(INT i=0; i<InNeighbours.Num(); i++)
		{
			if(InNeighbours(i) == INDEX_NONE)
			{
				Neighbours(i) = 255; // Indicates 'null' neighbour
			}
			else
			{
				check(InNeighbours(i) >= 0 && InNeighbours(i) < 255);
				Neighbours(i) = (BYTE)InNeighbours(i);
			}
		}

		// Copy neighbour dimension
		appMemzero(&NeighbourDims, sizeof(TArray<FLOAT>));
		NeighbourDims = InNeighbourDims;
	}
};

class FBoneInfluenceVertexBuffer : public FVertexBuffer
{
public:

	FBoneInfluenceVertexBuffer(class UFracturedStaticMesh* InMesh);
	virtual void InitRHI();
	virtual void ReleaseRHI();
	INT GetStride() const;

private:
	UFracturedStaticMesh* Mesh;
};

/**
 * UFracturedStaticMesh - A static mesh that contains additional information about how it can be split up
 */
class UFracturedStaticMesh : public UStaticMesh
{
	DECLARE_CLASS_INTRINSIC(UFracturedStaticMesh,UStaticMesh,CLASS_SafeReplace|CLASS_CollapseCategories|0,Engine);
public:

	/** Static mesh that this fractured mesh was built from, used to rebuild in the editor. */
	UStaticMesh* SourceStaticMesh;

	/** Static mesh that is used as the core of this fractured mesh. Editor only - data is duplicated into this mesh. */
	UStaticMesh* SourceCoreMesh;

	/** Amount that SourceCoreMesh is scaled when added. */
	FLOAT CoreMeshScale;

	/** Amount that SourceCoreMesh is scaled in 3D when added. */
	FVector CoreMeshScale3D;

	/** Offset of SourceCoreMesh relative to this FSM. */
	FVector CoreMeshOffset;

	/** Rotation of SourceCoreMesh relative to this FSM. */
	FRotator CoreMeshRotation;

	/** Save plane bias used for this FSM. */ 
	FVector PlaneBias;

	/** If TRUE, slice away the 'backs' of chunks based on the collision geometry of the core. */
	UBOOL bSliceUsingCoreCollision;

	/** DEPRECATED */
	UParticleSystem* FragmentDestroyEffect;

	/** Particle effects to play when chunk is hidden - one is randomly selected. */
	TArray<UParticleSystem*> FragmentDestroyEffects;

	/** Scaling for destruction effect. */
	FLOAT FragmentDestroyEffectScale;

	/** Used to scale how much damage needs to be done before a part is hidden. */
	FLOAT FragmentHealthScale;

	/** Minimum health for a fragment. */
	FLOAT FragmentMinHealth;

	/** Maximum health for a fragment. */
	FLOAT FragmentMaxHealth;

	/** All fragments should have the same health (uses a chunk size of 1.0x1.0) */
	UBOOL bUniformFragmentHealth;

	/** Linear speed at which chunks fly off. */
	FLOAT ChunkLinVel;

	/** Angular speed at which chunks fly off. */
	FLOAT ChunkAngVel;

	/** Increases horizontal component of part spawn velocity */
	FLOAT ChunkLinHorizontalScale;

	/** Scales the speed chunks fly out during an explosion. */
	FLOAT ExplosionVelScale;

	/** When a large piece falls off this mesh, have it explode on impact. */
	UBOOL bCompositeChunksExplodeOnImpact;

	/** If TRUE, don't spawn new physics for isolated chunks, even if there is no core. */
	UBOOL bFixIsolatedChunks;

	/** If TRUE, always spawn physics for isolated chunks, even if there is a core */
	UBOOL bAlwaysBreakOffIsolatedIslands;

	/** If TRUE, damage will cause parts to be spawned as physics objects. If FALSE, parts will simply be hidden and particle effect play. */
	UBOOL bSpawnPhysicsChunks;

	/** Controls the chance of physics chunks being spawned from normal damage (if bSpawnPhysicsChunks is TRUE). Value between 0.0 and 1.0. */
	FLOAT NormalChanceOfPhysicsChunk;

	/** Controls the chance of physics chunks being spawned from explosions (if bSpawnPhysicsChunks is TRUE). Value between 0.0 and 1.0. */
	FLOAT ExplosionChanceOfPhysicsChunk;

	/** Each spawned physics chunk from normal damage will be assigned a scale within this min and max. */
	FLOAT NormalPhysicsChunkScaleMin;
	FLOAT NormalPhysicsChunkScaleMax;

	/** Each spawned physics chunk from explosions will be assigned a scale within this min and max. */
	FLOAT ExplosionPhysicsChunkScaleMin;
	FLOAT ExplosionPhysicsChunkScaleMax;

	/** When looking for 'islands' - discard any connections less than this area. */
	FLOAT MinConnectionSupportArea;

	/** If set, this will be set as Material[OutsideMaterialIndex] on spawned dynamic parts. */
	UMaterialInterface* DynamicOutsideMaterial;

	/** If set, this will be set as Material[OutsideMaterialIndex] on the base mesh as soon as a part is broken off. */
	UMaterialInterface*	LoseChunkOutsideMaterial;

	/** The material index which corresponds to the 'outside' of the mesh. */
	INT OutsideMaterialIndex;

	/* Vertex buffer storing a bone index for each vertex of LOD0 */
	FBoneInfluenceVertexBuffer* InfluenceVertexBuffer;

	/* Internal version used to warn content authors that the FSM needs to be resliced. */
	WORD NonCriticalBuildVersion;
	WORD LicenseeNonCriticalBuildVersion;

private:

	/* Chunks of the static mesh that can be shown or hidden. */
	TArrayNoInit<FFragmentInfo> Fragments;

	/* Index of the core fragment. */
	INT CoreFragmentIndex;

	/* Index of the interior element, used to optimize fragment visibility based on neighbor information. */
	INT InteriorElementIndex;

public:

	// NB: When adding properties, please update WxStaticMeshEditor::SliceMesh, so they are preserved when mesh is re-sliced!
	//////////////////////////////////////////////////////////////////////////


	// UObject interface.

	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	void StaticConstructor();
	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void FinishDestroy();

	// Accessors
	const TArray<FFragmentInfo>& GetFragments() const;
	INT GetNumFragments() const;
	INT GetCoreFragmentIndex() const;

	/**
	 * Returns whether all neighbors of the given fragment are visible.
	 */
	UBOOL AreAllNeighborFragmentsVisible(INT FragmentIndex, const TArray<BYTE>& VisibleFragments) const;

	/** Returns if this fragment is destroyable. */
	UBOOL IsFragmentDestroyable(INT FragmentIndex) const;
	/** Changes if a fragmen is destroyable. */
	void SetFragmentDestroyable(INT FragmentIndex, UBOOL bDestroyable);

	/** Returns if this is a supporting 'root' fragment.  */
	UBOOL IsRootFragment(INT FragmentIndex) const;
	/** Changes if a fragment is a 'root' fragment. */
	void SetIsRootFragment(INT FragmentIndex, UBOOL bRootFragment);

	/** Returns if this fragment should never spawn a physics object.  */
	UBOOL IsNoPhysFragment(INT FragmentIndex) const;
	/** Changes if a fragment should never spawn a physics object. */
	void SetIsNoPhysFragment(INT FragmentIndex, UBOOL bNoPhysFragment);

	/** Returns bounding box of a particular chunk (graphics verts) in local (component) space. */
	FBox GetFragmentBox(INT FragmentIndex) const;
	/** Returns average exterior normal of a particular chunk. */
	FVector GetFragmentAverageExteriorNormal(INT FragmentIndex) const;

	/** Read-only access to interior element index. */
	INT GetInteriorElementIndex() const { return InteriorElementIndex; }

	/**
	 * Creates a fractured static mesh from raw triangle data.
	 * @param Outer - the new object's outer
	 * @param Name - name of the object to be created
	 * @param Flags - object creation flags
	 * @param RawFragments - raw triangles associated with a specific fragment
	 * @param BaseLODInfo - the LOD info to use for the new UFracturedStaticMesh's base LOD
	 * @param BaseElements - the Elements to use for the new UFracturedStaticMesh's base LOD
	 */
	static UFracturedStaticMesh* CreateFracturedStaticMesh(
		UObject* Outer,
		const TCHAR* Name, 
		EObjectFlags Flags, 
		TArray<FRawFragmentInfo>& RawFragments, 
		const FStaticMeshLODInfo& BaseLODInfo,
		INT InteriorElementIndex,
		const TArray<FStaticMeshElement>& BaseElements,
		UFracturedStaticMesh* ExistingFracturedMesh);

private:
	// UStaticMesh functions
	virtual void InitResources();
	virtual void ReleaseResources();

	friend void MergeStaticMesh(UStaticMesh* DestMesh, UStaticMesh* SourceMesh, const struct FMergeStaticMeshParams& Params);
};


/** Handles static lighting for a UFracturedStaticMeshComponent */
class FFracturedStaticLightingMesh : public FStaticMeshStaticLightingMesh
{
public:

	/** Initialization constructor. */
	FFracturedStaticLightingMesh(
		const class UFracturedStaticMeshComponent* InPrimitive,
		INT InLODIndex,
		const TArray<ULightComponent*>& InRelevantLights);

	virtual UBOOL ShouldCastShadow(ULightComponent* Light,const FStaticLightingMapping* Receiver) const;

	virtual UBOOL IsUniformShadowCaster() const;

	virtual FLightRayIntersection IntersectLightRay(const FVector& Start,const FVector& End,UBOOL bFindNearestIntersection) const;

private:

	/** The fractured mesh this mesh represents. */
	const UFracturedStaticMesh* FracturedMesh;

	/** The fractured component this mesh represents. */
	const class UFracturedStaticMeshComponent* const FracturedComponent;
};

#endif
