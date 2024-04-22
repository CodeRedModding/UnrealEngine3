/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNDECALRENDERDATA_H__
#define __UNDECALRENDERDATA_H__

#include "UnSkeletalMesh.h"		// For FRigidSkinVertex and FSoftSkinVertex
#include "LocalDecalVertexFactory.h"

// Forward declarations.
class FDecalRenderData;

/**
 * Helper function for determining when to render opaque and transparent decals
 * 
 * @param bSurfaceOpaqueRelevance - TRUE if the surface upon which the decals are placed has opaque materials attached
 * @param bSurfaceTranslucentRelevance - TRUE if the surface upon which the decals are placed has non-opaque materials attached
 * @param bTransparentRenderPass - TRUE if the rendering pipeline is in the translucency phase
 * @param bDrawOpaqueDecals - Return TRUE if opaque decals should be rendered now
 * @param bDrawTransparentDecals - Return TRUE if transparent decals should be rendered now
 */
void GetDrawDecalFilters(const UBOOL bSurfaceOpaqueRelevance, const UBOOL bSurfaceTranslucentRelevance, const UBOOL bTransparentRenderPass, UBOOL& bDrawOpaqueDecals, UBOOL& bDrawTransparentDecals);

/**
 * Class for representing the state of a decal, to be used in the rendering thread by
 * receiving primitives for intersecting with their geometry.
 */
class FDecalState
{
public:
	const UDecalComponent*	DecalComponent;
	UMaterialInterface*		DecalMaterial;

	FVector OrientationVector;
	FVector HitLocation;
	FVector HitNormal;
	FVector HitTangent;
	FVector HitBinormal;

	FVector FrustumVerts[8];

	FLOAT OffsetX;
	FLOAT OffsetY;

	FLOAT Width;
	FLOAT Height;

	FLOAT DepthBias;
	FLOAT SlopeScaleDepthBias;
	INT SortOrder;

	/** World space bounds of this decal's frustum, cached here so the render thread can talk to it */
	FBox Bounds;

	/** The squared cull distance for this decal (cached from decal component's CullDistance) */
	FLOAT SquaredCullDistance;

	/** distance (world space) of near plane to the decal origin */
	FLOAT NearPlaneDistance;
	/** distance (world space) of far plane to the decal origin */
	FLOAT FarPlaneDistance;	

	TArray<FPlane> Planes;
	/** Matrix that transforms world-space positions to decal texture coordinates. */
	FMatrix WorldTexCoordMtx;

	/** Name of the bone that was hit. */
	FName HitBone;
	/** Index of the hit bone. */
	INT HitBoneIndex;
	/** If not -1, specifies the level into the world's level array of the BSP node that was hit. */
	INT HitLevelIndex;
	/** Used to pass information of which BSP nodes where hit */
	TArray<INT> HitNodeIndices;
	/** 
	* If not -1, specifies the fracture static mesh fragment type allowed for projection.
	* If the index is the core fragment then the decal will only project on core triangles.
	* If the index is a non-core fragment then the decal will only project on non-core triangles.
	*/
	INT FracturedStaticMeshComponentIndex;

	/** parent local to world when decal was initially attached */
	FMatrix AttachmentLocalToWorld;

	BYTE DepthPriorityGroup;
	BITFIELD bNoClip:1;

	/** TRUE if we should use the already-clipped decal representation instead of clipping it at render-time */
	BITFIELD bUseSoftwareClip:1;

	BITFIELD bProjectOnBackfaces:1;
	BITFIELD bFlipBackfaceDirection:1;
	BITFIELD bProjectOnBSP:1;
	BITFIELD bProjectOnStaticMeshes:1;
	BITFIELD bProjectOnSkeletalMeshes:1;
	BITFIELD bProjectOnTerrain:1;
	BITFIELD bDecalMaterialHasStaticLightingUsage:1;
	BITFIELD bDecalMaterialHasUnlitLightingModel:1;
	BITFIELD bStaticDecal:1;
	BITFIELD bMovableDecal:1; 

	FMaterialViewRelevance MaterialViewRelevance;

	/**
	 * computes an axis-aligned bounding box of the decal frustum vertices projected to the screen.
	 *
	 * @param		SceneView				Scene projection
	 * @param		OutMin					[out] Min vertex of the screen AABB.
	 * @param		OutMax					[out] Max vertex of the screen AABB.
	 * @param		FrustumVertexTransform	A transform applied to the frustum verts before screen projection.
	 * @return								FALSE if the AABB has zero area, TRUE otherwise.
	 */
	UBOOL QuadToClippedScreenSpaceAABB(const class FSceneView* SceneView, FVector2D& OutMin, FVector2D& OutMax, const FMatrix& FrustumVertexTransform=FMatrix::Identity) const;

	/**
	 * Transforms the decal frustum vertices by the specified matrix.
	 */
	void TransformFrustumVerts(const FMatrix& FrustumVertexTransform);

	/**
	* Transforms the decal frustum vertices by the specified matrix.
	*/
	void TransformFrustumVerts(const FBoneAtom& FrustumVertexTransform);

	/**
	* Update AttachmentLocalToWorld and anything dependent on it
	*/
	void UpdateAttachmentLocalToWorld(const FMatrix& InAttachmentLocalToWorld);

	/** 
	* @return memory used by this struct 
	*/
	DWORD GetMemoryFootprint();

};

class FDecalInteraction
{
public:
	/** Decal component that is being attached */
	UDecalComponent*	Decal;
	/** Render data including vertex/index data needed to render the decal */
	FDecalRenderData*	RenderData;
	/** State needed to render the decal */
	FDecalState			DecalState;
	/** Optional static mesh element for this decal/receiver association */
	class FStaticMesh*	DecalStaticMesh;

	/** 
	* Default Constructor 
	*/
	FDecalInteraction();
	/** 
	* Constructor 
	* @param InDecal - decal component for this interaction
	* @param InRenderData - render data generated for this interaction
	*/
	FDecalInteraction(UDecalComponent* InDecal, FDecalRenderData* InRenderData);
	/** 
	* Copy Constructor  
	*/
	FDecalInteraction(const FDecalInteraction& Copy);
	/**
	* Assignment
	*/
	FDecalInteraction& operator=(const FDecalInteraction& Other);
	/**
	* Destructor
	*/
	~FDecalInteraction();

	/** 
	* Generate the static mesh using the render proxy of the receiver the decal is attaching to 
	* Updates DecalStaticMesh and also adds it to the list of DecalSTaticMeshes in FScene
	* @param PrimitiveSceneInfo - primitive info for the receiving mesh
	*/
	void CreateDecalStaticMesh(FPrimitiveSceneInfo* PrimitiveSceneInfo);

private:
	/**
	* Copies all the decal members except and deletes the existing DecalStaticMesh
	* @param Copy - other decal interaction to copy from
	*/
	void SafeCopy(const FDecalInteraction& Copy);	
};

class FDecalVertex
{
public:
	FDecalVertex() {}
	FDecalVertex(const FVector& InPosition,
				const FPackedNormal& InTangentX,
				 const FPackedNormal& InTangentZ,
				 const FVector2D& InLightMapCoordinate)
		:	Position( InPosition )
		,	TangentX( InTangentX )
		,	TangentZ( InTangentZ )
		,	LightMapCoordinate( InLightMapCoordinate )
	{}

	/**
	* FDecalVertex serializer.
	*/
	friend FArchive& operator<<(FArchive& Ar, FDecalVertex& DecalVertex);

	FVector			Position;
	FPackedNormal	TangentX;
	FPackedNormal	TangentZ;

	/** Decal vertex light map coordinated.  Added in engine version VER_DECAL_ADDED_DECAL_VERTEX_LIGHTMAP_COORD. */
	FVector2D		LightMapCoordinate;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

class FDecalVertexBuffer : public FVertexBuffer
{
public:

	/**
	* @param	InDecalRenderData	Pointer to parent structure.
	*/
	FDecalVertexBuffer(FDecalRenderData* InDecalRenderData);

	// FRenderResource interface.
	virtual void InitRHI();

	virtual FString GetFriendlyName() const
	{
		return FString( TEXT("Decal vertex buffer") );
	}

	/**
	* @return number of vertices.
	*/
	FORCEINLINE INT GetNumVertices() const
	{
		return NumVertices;
	}	

protected:
	/** Pointer to parent structure, used in GetData() and associated functions. */
	FDecalRenderData*	DecalRenderData;
	INT NumVertices;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	FDecalRenderData
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Baseclass for receiver-specific decal resources. */
class FReceiverResource
{
public:
	/** Asserts that resources are not initialized; clients must manually release resources!. */
	virtual ~FReceiverResource()
	{
		check( !IsInitialized() );
	}

	/** Releases the resource if it is currently initialized. */
	void Release_RenderingThread()
	{
		if ( IsInitialized() )
		{
			OnRelease_RenderingThread();
			bInitialized = FALSE;
		}
	}

	/** Callback for resource release, to be implemented by derived classes. */
	virtual void OnRelease_RenderingThread() = 0;

	/** 
	* @return memory usage in bytes for the receiver resource data for a given decal
	*/
	virtual INT GetMemoryUsage() = 0;

	/** @return		TRUE if the resource is initialized, FALSE otherwise. */
	FORCEINLINE UBOOL IsInitialized() const
	{
		return bInitialized;
	}

	

protected:
	FReceiverResource()
		: bInitialized( FALSE )
	{}

	/** Called by derived classes to indicate that resources have been initialized. */
	void SetInitialized()
	{
		bInitialized = TRUE;
	}

private:
	/** Flag indicating whether resources are currently initialized. */
	UBOOL bInitialized;
};

class FDecalRenderData
{
public:

	FDecalRenderData(
		FLightCacheInterface* InLCI=NULL, 
		UBOOL bInUsesVertexResources=TRUE, 
		UBOOL bInUsesIndexResources=TRUE, 
		const FVertexFactory* InReceiverVertexFactory=NULL
		)
		:	DecalVertexBuffer( this )
		,	DecalVertexFactory( NULL )
		,	ReceiverVertexFactory( InReceiverVertexFactory )
		,	NumTriangles( 0 )
		,	NumVerticesInitialized( 0 )
		,	NumIndicesInitialized( 0 )
		,	Data( 0 )
		,	LCI( InLCI )
		,	bUsesVertexResources( InReceiverVertexFactory ? FALSE : bInUsesVertexResources )
		,	bUsesIndexResources( bInUsesIndexResources )
		,	DecalBlendRange(-1.f,1.f)
		,	InstanceIndex( INDEX_NONE )
	{}

	/**
	 * Prepares resources for deletion.
	 * This is only called by the rendering thread.
	 */
	~FDecalRenderData();

	/**
	 * Initializes resources.
	 * This is only called by the game thread, when a receiver is attached to a decal.
	 */
	void InitResources_GameThread();

	/**
	 * Add an index to the decal index buffer.
	 *
	 * @param	Index		The index to add.
	 */
	FORCEINLINE void AddIndex(WORD Index)
	{
		IndexBuffer.Indices.AddItem( Index );
	}

	/**
	 * Returns the number of vertices.
	 */
	FORCEINLINE INT GetNumVertices() const
	{
		return Vertices.Num();
	}

	/**
	 * Returns the number of indices.
	 */
	FORCEINLINE INT GetNumIndices() const
	{
		return IndexBuffer.Indices.Num();
	}

	/**
	* Render clipped decal vertices and their tangents 
	*/
	void DebugDraw(FPrimitiveDrawInterface* PDI,const FDecalState& DecalState,const FMatrix& LocalToWorld, INT DPGIdx) const;

	/** Source data. */
	TArray<FDecalVertex>			Vertices;

	/** Vertex buffer. */
	FDecalVertexBuffer				DecalVertexBuffer;

	/** Decal vertex factory. */
	FDecalVertexFactoryBase*		DecalVertexFactory;
	const FVertexFactory*			ReceiverVertexFactory;

	/** New index buffer used in Gemini. */
	FRawIndexBuffer					IndexBuffer;

	/** Number of triangles represented by this vertex set. */
	UINT							NumTriangles;

	UINT							NumVerticesInitialized;
	UINT							NumIndicesInitialized;

	/** Members available for decal receiver-specific use. */
	INT								Data;

	/** List of receiver-specific resources; managed by this decal and used by receiver types/proxies. */
	TArray<FReceiverResource*>		ReceiverResources;

	/** The static lighting cache for the mesh the decal is attached to. */
	FLightCacheInterface*			LCI;

	/**
	 * Vertex lightmap for the decal.  Used by e.g. decals on vertex lightmapped static meshes,
	 * where the mesh'light map samples are resampled according to the decal vertex indices.
	 * If NULL and the decal material is lit, the receiver's light map will be used.
	 */
	FLightMapRef					LightMap1D;
	/** Shadow maps associated with static decals. These are copied from the base mesh and remapped. */
	TArray<UShadowMap1D*>			ShadowMap1D;

	/** 'Rigid' vertices for decals on CPU-skinned meshes. */
	TArray<FRigidSkinVertex>		RigidVertices;
	/** 'Soft' vertices for decals on CPU-skinned meshes. */
	TArray<FSoftSkinVertex>			SoftVertices;
	/** Fracture geometry fragments that this decal affects */
	TSet<INT>						FragmentIndices;

	/** Used to indicate whether the FDecalRenderData's vertex buffer/factory resources are used. */
	BITFIELD						bUsesVertexResources:1;
	/** Used to indicate whether the FDecalRenderData's index buffer resources should are used. */
	BITFIELD						bUsesIndexResources:1;

	/** dot product range for blending decal. Used by the decal vertex factory */
	FVector2D						DecalBlendRange;

	/** Used to remap indices for vertex lighting. */
	TArray<INT>						SampleRemapping;

	/** Instance index, optionally used for components that support mesh instancing */
	INT								InstanceIndex;


	/** 
	* @return memory usage in bytes for the render data for a given decal
	*/
	INT GetMemoryUsage();

private:
	/**
	 * Prepares resources for deletion.
	 * This is only called by the rendering thread.
	 */
	void ReleaseResources_RenderingThread();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	FStaticReceiverData
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FStaticReceiverData
{
public:
	/** The receiving component. */
	UPrimitiveComponent*	Component;
	/** Component-specific instance index on the receiving component, for instanced primitives */
	INT						InstanceIndex;
	/** Source vertex data. */
	TArray<FDecalVertex>	Vertices;
	/** Index buffer. */
	TArray<WORD>			Indices;
	/** Number of decal triangles. */
	UINT					NumTriangles;
	/** Lightmap vertex buffer. */
	FLightMapRef			LightMap1D;
	/** Shadowmap vertex buffers */
	TArray<UShadowMap1D*>	ShadowMap1D;
	/** Members available for decal receiver-specific use. */
	INT						Data;

	FStaticReceiverData()
		: InstanceIndex( INDEX_NONE )
		, NumTriangles( 0 )
		, Data(0)
	{}

	/**
	 * FStaticReceiverData serializer.
	 */
	friend FArchive& operator<<(FArchive& Ar, FStaticReceiverData& Tgt);
};

/**
* Stores local space decal info used for rendering and decal vertex generation
*/
class FDecalLocalSpaceInfo
{
public:
	const FDecalState* Decal;
	FMatrix TextureTransform;
	FVector LocalLocation;
	FVector LocalTangent;
	FVector LocalBinormal;
	FVector LocalNormal;
	FLOAT LocalDet;

	FDecalLocalSpaceInfo(
		const FDecalState* InDecal, 
		const FMatrix& InReceiverLocalToWorld, 
		const FMatrix& InReceiverWorldToLocal
		);
};

/**
 * Stores local space decal info as well as frustum planes necessary for CPU clipping
 */
class FDecalLocalSpaceInfoClip : public FDecalLocalSpaceInfo
{
public:
	FConvexVolume Convex;
	FVector LocalLookVector;
	FVector TextureHitLocation;
	
	FDecalLocalSpaceInfoClip(
		const FDecalState* InDecal, 
		const FMatrix& InReceiverLocalToWorld, 
		const FMatrix& InReceiverWorldToLocal
		);	

	/**
	* Computes decal texture coordinates from the the specified world-space position.
	*
	* @param		InPos			World-space position.
	* @param		OutTexCoords	[out] Decal texture coordinates.
	*/
	void ComputeTextureCoordinates(const FVector& InPos, FVector2D& OutTexCoords) const;	
};

/**
 * CPU Clipper for decal polygons.
 */
class FDecalPoly
{
public:
	FVector			  FaceNormal;
	TArray<FVector>	  Vertices;
	TArray<FVector2D> ShadowTexCoords;
	TArray<INT>		  Indices;

	void Init()
	{
		// Avoid re-allocation.
		Vertices.Reset();
		ShadowTexCoords.Reset();
		Indices.Reset();
	}

	// Returns FALSE if a problem occurs.
	UBOOL CalcNormal()
	{
		checkSlow( Vertices.Num() == 3 );
		FaceNormal = (Vertices(1) - Vertices(0)) ^ (Vertices(2) - Vertices(0));
		return FaceNormal.Normalize(KINDA_SMALL_NUMBER);
	}

	UBOOL ClipAgainstConvex(const FConvexVolume& Convex)
	{
		for( INT PlaneIndex = 0 ; PlaneIndex < Convex.Planes.Num() ; ++PlaneIndex )
		{
			const FPlane&	Plane = Convex.Planes(PlaneIndex);
			if( !Split(-FVector(Plane),Plane * Plane.W) )
			{
				return FALSE;
			}
		}
		return TRUE;
	}

	// Split a poly and keep only the front half.
	// @return		Number of vertices, 0 if clipped away.
	INT Split( const FVector &Normal, const FVector &Base );

	/**
	 * Split with plane quickly for in-game geometry operations.
	 * Results are always valid. May return sliver polys.
	 */
	INT SplitWithPlaneFast(const FPlane& Plane, FDecalPoly* FrontPoly) const;
};

#endif // __UNDECALRENDERDATA_H__
