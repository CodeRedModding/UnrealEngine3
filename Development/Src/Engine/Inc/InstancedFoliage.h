/*=============================================================================
	InstancedFoliage.h: Instanced foliage type definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/


/**
 * Flags stored with each instance
 */
enum EFoliageInstanceFlags
{
	FOLIAGE_AlignToNormal				= 0x00000001,
	FOLIAGE_NoRandomYaw					= 0x00000002,
	FOLIAGE_Readjusted					= 0x00000004,
};

/**
 *	FFoliageInstancePlacementInfo - placement info an individual instance
 */
struct FFoliageInstancePlacementInfo
{
	FVector Location;
	FRotator Rotation;
	FRotator PreAlignRotation;
	FVector DrawScale3D;
	FLOAT ZOffset;
	DWORD Flags;

	FFoliageInstancePlacementInfo()
	:	Location(0.f,0.f,0.f)
	,	Rotation(0,0,0)
	,	PreAlignRotation(0,0,0)
	,	DrawScale3D(1.f,1.f,1.f)
	,	ZOffset(0.f)
	,	Flags(0)
	{}
};

/**
 *	FFoliageInstance - editor info an individual instance
 */
struct FFoliageInstance : public FFoliageInstancePlacementInfo
{
	class UActorComponent* Base;
	INT ClusterIndex;	// -1 if this instance is invalid and its array slot can be reused.

	FFoliageInstance()
	:	Base(NULL)
	,	ClusterIndex(-1)
	{}


	friend FArchive& operator<<( FArchive& Ar, FFoliageInstance& Instance );

	FMatrix GetInstanceTransform() const
	{
		FMatrix InstanceTransform = FScaleMatrix(DrawScale3D);
		InstanceTransform *= FRotationMatrix(Rotation);
		InstanceTransform *= FTranslationMatrix(Location);
		check( !InstanceTransform.ContainsNaN() );
		return InstanceTransform;
	}

	void AlignToNormal( const FVector& InNormal, FLOAT AlignMaxAngle=0.f )
	{
		Flags |= FOLIAGE_AlignToNormal;

		FRotator AlignRotation = InNormal.Rotation();
		// Static meshes are authored along the vertical axis rather than the X axis, so we add 90 degrees to the static mesh's Pitch.
		AlignRotation.Pitch -= 16384;
		// Clamp its value inside +/- one rotation
		AlignRotation.Pitch &= 65535;
		if( AlignRotation.Pitch > 32767 ) 
		{
			AlignRotation.Pitch -= 65536;
		}

		// limit the maximum pitch angle if it's > 0.
		if( AlignMaxAngle > 0.f )
		{
			INT MaxPitch = appRound(65535.f * AlignMaxAngle / 360.f);
			if( AlignRotation.Pitch > MaxPitch )
			{
				AlignRotation.Pitch = MaxPitch;
			}
			else
			if( AlignRotation.Pitch < -MaxPitch )
			{
				AlignRotation.Pitch = -MaxPitch;
			}								
		}

		PreAlignRotation = Rotation;
		Rotation = FRotator( FQuat(AlignRotation) * FQuat(Rotation) );
	}
};

/**
 *	FFoliageInstanceCluster - render info for a cluster of meshes
 */
struct FFoliageInstanceCluster
{
	UInstancedStaticMeshComponent* ClusterComponent;
	FBoxSphereBounds Bounds;
	TArray<INT> InstanceIndices;	// index into editor editor Instances array

	FFoliageInstanceCluster()
	{}

	FFoliageInstanceCluster(UInstancedStaticMeshComponent* InClusterComponent, const FBoxSphereBounds& InBounds)
	:	ClusterComponent(InClusterComponent)
	,	Bounds(InBounds)
	{}

	friend FArchive& operator<<( FArchive& Ar, FFoliageInstanceCluster& Cluster );
};

/** 
 * FFoliageComponentHashInfo
 * Cached instance list and component location info stored in the ComponentHash.
 * Used for moving quick updates after operations on components with foliage painted on them.
 */
struct FFoliageComponentHashInfo
{
	// tors
	FFoliageComponentHashInfo()
	:	CachedLocation(0,0,0)
	,	CachedRotation(0,0,0)
	,	CachedDrawScale(0,0,0)
	{}

	FFoliageComponentHashInfo( UActorComponent* InComponent )
	:	CachedLocation(0,0,0)
	,	CachedRotation(0,0,0)
	,	CachedDrawScale(0,0,0)
	{
		UpdateLocationFromActor(InComponent);
	}

	// Cache the location and rotation from the actor
	void UpdateLocationFromActor( UActorComponent* InComponent )
	{
		if( InComponent )
		{
			AActor* Owner = Cast<AActor>(InComponent->GetOuter());
			if( Owner )
			{
				CachedLocation = Owner->Location;
				CachedRotation = Owner->Rotation;
				CachedDrawScale = Owner->DrawScale * Owner->DrawScale3D;
			}
		}
	}

	// serializer
	friend FArchive& operator<<( FArchive& Ar, FFoliageComponentHashInfo& ComponentHashInfo );

	FVector CachedLocation;
	FRotator CachedRotation;
	FVector CachedDrawScale;
	TSet<INT> Instances;
};

/**
 *	FFoliageMeshInfo - editor info for all matching foliage meshes
 */
struct FFoliageMeshInfo
{
	// Cluster allocation data and pointers to components
	TArray<FFoliageInstanceCluster> InstanceClusters;

	// Editor-only placed instances
	TArray<FFoliageInstance> Instances;
	
	// Transient, editor-only locality hash of instances
	struct FFoliageInstanceHash* InstanceHash;

	// Transient, editor-only set of instances per component
	TMap<class UActorComponent*, FFoliageComponentHashInfo > ComponentHash;

	// Transient, editor-only list of free instance slots.
	TArray<INT> FreeInstanceIndices;

	// Transient, editor-only list of selected instances.
	TArray<INT> SelectedIndices;

	// UI settings
	class UInstancedFoliageSettings* Settings;

	FFoliageMeshInfo();
	FFoliageMeshInfo( const FFoliageMeshInfo& Other );

	~FFoliageMeshInfo();

#if WITH_EDITOR
	void AddInstance( class AInstancedFoliageActor* InIFA, class UStaticMesh* InMesh, const FFoliageInstance& InNewInstance );
	void RemoveInstances( class AInstancedFoliageActor* InIFA, const TArray<INT>& InInstancesToRemove );
	void PreMoveInstances( class AInstancedFoliageActor* InIFA, const TArray<INT>& InInstancesToMove );
	void PostMoveInstances( class AInstancedFoliageActor* InIFA, const TArray<INT>& InInstancesMoved );
	void PostUpdateInstances( class AInstancedFoliageActor* InIFA, const TArray<INT>& InInstancesUpdated, UBOOL bReAddToHash=FALSE );
	void DuplicateInstances( class AInstancedFoliageActor* InIFA, class UStaticMesh* InMesh, const TArray<INT>& InInstancesToDuplicate );
	void GetInstancesInsideSphere( const FSphere& Sphere, TArray<INT>& OutInstances );
	UBOOL CheckForOverlappingSphere( const FSphere& Sphere );
	UBOOL CheckForOverlappingInstanceExcluding( INT TestInstanceIdx, FLOAT Radius, TSet<INT>& ExcludeInstances );
	void ApplyInstancedFoliageSettings( UInstancedStaticMeshComponent* InClusterComponent );

	// Destroy existing clusters and reassign all instances to new clusters
	void ReallocateClusters( class AInstancedFoliageActor* InIFA, class UStaticMesh* InMesh );
	// Update settings in the clusters based on the current settings (eg culling distance, collision, ...)
	void UpdateClusterSettings( class AInstancedFoliageActor* InIFA );
	void SelectInstances( class AInstancedFoliageActor* InIFA, UBOOL bSelect, TArray<INT>& Instances );
#endif

#if _DEBUG
	void CheckValid();
#endif
	friend FArchive& operator<<( FArchive& Ar, FFoliageMeshInfo& MeshInfo );
};

//
// FFoliageInstanceHash
//

#define FOLIAGE_HASH_CELL_BITS 9	// 512x512 grid

struct FFoliageInstanceHash
{
	INT HashCellBits;
private:

	QWORD MakeKey( INT CellX, INT CellY )
	{
		return ((QWORD)(*(DWORD*)(&CellX)) << 32) | (*(DWORD*)(&CellY) & 0xffffffff);
	}

	QWORD MakeKey( const FVector& Location )
	{
		return  MakeKey( appFloor(Location.X) >> HashCellBits, appFloor(Location.Y) >> HashCellBits );
	}

	// Locality map
	TMap<QWORD, TSet<INT> > CellMap;
public:
	FFoliageInstanceHash( INT InHashCellBits = FOLIAGE_HASH_CELL_BITS )
	:	HashCellBits(InHashCellBits)
	{}

	void InsertInstance(const FVector& InstanceLocation, INT InstanceIndex)
	{
		QWORD Key = MakeKey(InstanceLocation);

		TSet<INT>& NewSet = CellMap.FindOrAdd(Key);
		NewSet.Add(InstanceIndex);
	}

	void RemoveInstance(const FVector& InstanceLocation, INT InstanceIndex)
	{
		QWORD Key = MakeKey(InstanceLocation);

		TSet<INT>* Set = CellMap.Find(Key);
		check( Set != NULL );
		INT RemoveCount = Set->RemoveKey(InstanceIndex);
		check( RemoveCount == 1 );
	}

	void GetInstancesOverlappingBox( const FBox& InBox, TSet<INT>& OutInstanceIndices )
	{
		INT MinX = appFloor(InBox.Min.X) >> HashCellBits;
		INT MinY = appFloor(InBox.Min.Y) >> HashCellBits;
		INT MaxX = appFloor(InBox.Max.X) >> HashCellBits;
		INT MaxY = appFloor(InBox.Max.Y) >> HashCellBits;

		for( INT y=MinY;y<=MaxY;y++ )
		{
			for( INT x=MinX;x<=MaxX;x++ )
			{
				QWORD Key = MakeKey(x,y);
				TSet<INT>* Set = CellMap.Find(Key);
				if( Set != NULL )
				{
					OutInstanceIndices.Add( *Set );
				}
			}
		}
	}

#if _DEBUG
	void CheckInstanceCount(INT InCount)
	{
		INT HashCount = 0;
		for( TMap<QWORD, TSet<INT> >::TConstIterator It(CellMap); It; ++It )
		{
			HashCount += It.Value().Num();
		}	
		check( HashCount == InCount );
	}
#endif

	void Empty()
	{
		CellMap.Empty();
	}

	friend FArchive& operator<<( FArchive& Ar, FFoliageInstanceHash& Hash )
	{
		Ar << Hash.CellMap;
		return Ar;
	}
};


/** InstancedStaticMeshInstance hit proxy */
struct HInstancedStaticMeshInstance : public HHitProxy
{
	UInstancedStaticMeshComponent* Component;
	INT InstanceIndex;

	DECLARE_HIT_PROXY(HInstancedStaticMeshInstance,HHitProxy);
	HInstancedStaticMeshInstance(UInstancedStaticMeshComponent* InComponent, INT InInstanceIndex): HHitProxy(HPP_World), Component(InComponent), InstanceIndex(InInstanceIndex) {}

	virtual void Serialize(FArchive& Ar);

	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Cross;
	}
};
