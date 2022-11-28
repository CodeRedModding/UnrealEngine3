/*=============================================================================
	UnModel.h: Unreal UModel definition.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

//
//	FPointRegion - Identifies a unique convex volume in the world.
//

struct FPointRegion
{
	// Variables.
	class AZoneInfo* Zone;			// Zone actor.
	INT				 iLeaf;			// Bsp leaf.
	BYTE             ZoneNumber;	// Zone number.

	// Constructors.
	FPointRegion()
	{}
	FPointRegion( class AZoneInfo* InLevel )
	:	Zone(InLevel), iLeaf(INDEX_NONE), ZoneNumber(0)
	{}
	FPointRegion( class AZoneInfo* InZone, INT InLeaf, BYTE InZoneNumber )
	:	Zone(InZone), iLeaf(InLeaf), ZoneNumber(InZoneNumber)
	{}
};

//
//	FBspNode
//

// Flags associated with a Bsp node.
enum EBspNodeFlags
{
	// Flags.
	NF_NotCsg			= 0x01, // Node is not a Csg splitter, i.e. is a transparent poly.
	NF_NotVisBlocking   = 0x04, // Node does not block visibility, i.e. is an invisible collision hull.
	NF_BrightCorners	= 0x10, // Temporary.
	NF_IsNew 		 	= 0x20, // Editor: Node was newly-added.
	NF_IsFront     		= 0x40, // Filter operation bounding-sphere precomputed and guaranteed to be front.
	NF_IsBack      		= 0x80, // Guaranteed back.
};

//
// FBspNode defines one node in the Bsp, including the front and back
// pointers and the polygon data itself.  A node may have 0 or 3 to (MAX_NODE_VERTICES-1)
// vertices. If the node has zero vertices, it's only used for splitting and
// doesn't contain a polygon (this happens in the editor).
//
// vNormal, vTextureU, vTextureV, and others are indices into the level's
// vector table.  iFront,iBack should be INDEX_NONE to indicate no children.
//
// If iPlane==INDEX_NONE, a node has no coplanars.  Otherwise iPlane
// is an index to a coplanar polygon in the Bsp.  All polygons that are iPlane
// children can only have iPlane children themselves, not fronts or backs.
//
struct FBspNode // 58 bytes
{
	enum {MAX_NODE_VERTICES=16};	// Max vertices in a Bsp node, pre clipping.
	enum {MAX_FINAL_VERTICES=24};	// Max vertices in a Bsp node, post clipping.
	enum {MAX_ZONES=64};			// Max zones per level.

	// Persistent information.
	FPlane		Plane;			// 16 Plane the node falls into (X, Y, Z, W).
	FZoneSet	ZoneMask;		// 8  Bit mask for all zones at or below this node (up to 64).
	INT			iVertPool;		// 4  Index of first vertex in vertex pool, =iTerrain if NumVertices==0 and NF_TerrainFront.
	INT			iSurf;			// 4  Index to surface information.

	// iBack:  4  Index to node in front (in direction of Normal).
	// iFront: 4  Index to node in back  (opposite direction as Normal).
	// iPlane: 4  Index to next coplanar poly in coplanar list.
	union { INT iBack; INT iChild[1]; };
	        INT iFront;
			INT iPlane;

	INT		iCollisionBound;// 4  Collision bound.

	BYTE	iZone[2];		// 2  Visibility zone in 1=front, 0=back.
	BYTE	NumVertices;	// 1  Number of vertices in node.
	BYTE	NodeFlags;		// 1  Node flags.
	INT		iLeaf[2];		// 8  Leaf in back and front, INDEX_NONE=not a leaf.
	

	// Functions.
	UBOOL IsCsg( DWORD ExtraFlags=0 ) const
	{
		return (NumVertices>0) && !(NodeFlags & (NF_IsNew | NF_NotCsg | ExtraFlags));
	}
	UBOOL ChildOutside( INT iChild, UBOOL Outside, DWORD ExtraFlags=0 ) const
	{
		return iChild ? (Outside || IsCsg(ExtraFlags)) : (Outside && !IsCsg(ExtraFlags));
	}
	friend FArchive& operator<<( FArchive& Ar, FBspNode& N );
};

//
//	FZoneProperties
//

struct FZoneProperties
{
public:
	// Variables.
	AZoneInfo*	ZoneActor;		// Optional actor defining the zone's property.
	FLOAT		LastRenderTime;	// Most recent level TimeSeconds when rendered.
	FZoneSet	Connectivity;	// (Connect[i]&(1<<j))==1 if zone i is adjacent to zone j.
	FZoneSet	Visibility;		// (Connect[i]&(1<<j))==1 if zone i can see zone j.

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FZoneProperties& P )
	{
		return Ar
				<< *(UObject**)&P.ZoneActor
				<< P.Connectivity
				<< P.Visibility
				<< P.LastRenderTime;
	}
};

//
//	FLeaf
//

struct FLeaf
{
	INT		iZone;          // The zone this convex volume is in.

	// Functions.
	FLeaf()
	{}
	FLeaf(INT iInZone):
		iZone(iInZone)
	{}
	friend FArchive& operator<<( FArchive& Ar, FLeaf& L )
	{
		Ar << L.iZone;
		return Ar;
	}
};

//
//	FBspSurf
//

//
// One Bsp polygon.  Lists all of the properties associated with the
// polygon's plane.  Does not include a point list; the actual points
// are stored along with Bsp nodes, since several nodes which lie in the
// same plane may reference the same poly.
//
struct FBspSurf
{
public:

	UMaterialInstance*	Material;		// 4 Material.
	DWORD				PolyFlags;		// 4 Polygon flags.
	INT					pBase;			// 4 Polygon & texture base point index (where U,V==0,0).
	INT					vNormal;		// 4 Index to polygon normal.
	INT					vTextureU;		// 4 Texture U-vector index.
	INT					vTextureV;		// 4 Texture V-vector index.
	INT					iBrushPoly;		// 4 Editor brush polygon index.
	ABrush*				Actor;			// 4 Brush actor owning this Bsp surface.
	FPlane				Plane;			// 16 The plane this surface lies on.
	FLOAT				LightMapScale;	// 4 The number of units/lightmap texel on this surface.

	// Functions.
	friend FArchive& operator<<( FArchive& Ar, FBspSurf& Surf );
};

// Flags describing effects and properties of a Bsp polygon.
enum EPolyFlags
{
	// Regular in-game flags.
	PF_Invisible		= 0x00000001,	// Poly is invisible.
	PF_NotSolid			= 0x00000008,	// Poly is not solid, doesn't block.
	PF_Semisolid	  	= 0x00000020,	// Poly is semi-solid = collision solid, Csg nonsolid.
	PF_GeomMarked	  	= 0x00000040,	// Geometry mode sometimes needs to mark polys for processing later.
	PF_TwoSided			= 0x00000100,	// Poly is visible from both sides.
	PF_Portal			= 0x04000000,	// Portal between iZones.
	PF_Mirrored         = 0x20000000,   // Mirrored BSP surface.

	// Editor flags.
	PF_Memorized     	= 0x01000000,	// Editor: Poly is remembered.
	PF_Selected      	= 0x02000000,	// Editor: Poly is selected.

	// Internal.
	PF_EdProcessed 		= 0x40000000,	// FPoly was already processed in editorBuildFPolys.
	PF_EdCut       		= 0x80000000,	// FPoly has been split by SplitPolyWithPlane.

	// Combinations of flags.
	PF_NoEdit			= PF_Memorized | PF_Selected | PF_EdProcessed | PF_EdCut,
	PF_NoImport			= PF_NoEdit | PF_Memorized | PF_Selected | PF_EdProcessed | PF_EdCut,
	PF_AddLast			= PF_Semisolid | PF_NotSolid,
	PF_NoAddToBSP		= PF_EdCut | PF_EdProcessed | PF_Selected | PF_Memorized
};

//
//	UModel
//

enum {MAX_NODES  = 65536};
enum {MAX_POINTS = 128000};
class UModel : public UObject
{
	DECLARE_CLASS(UModel,UObject,0,Engine)

	// Arrays and subobjects.
	UPolys*						Polys;
	TTransArray<FBspNode>		Nodes;
	TTransArray<FVert>			Verts;
	TTransArray<FVector>		Vectors;
	TTransArray<FVector>		Points;
	TTransArray<FBspSurf>		Surfs;
	TArray<INT>					LeafHulls;
	TArray<FLeaf>				Leaves;
	TArray<INT>					PortalNodes;

	// Other variables.
	UBOOL						RootOutside;
	UBOOL						Linked;
	INT							MoverLink;
	INT							NumSharedSides;
	INT							NumZones;
	FZoneProperties				Zones[FBspNode::MAX_ZONES];
	FBoxSphereBounds			Bounds;

	// Constructors.
	UModel()
	: RootOutside( 1 )
	, Surfs( this )
	, Vectors( this )
	, Points( this )
	, Verts( this )
	, Nodes( this )
	{
		EmptyModel( 1, 0 );
	}
	UModel( ABrush* Owner, UBOOL InRootOutside=1 );

	// UObject interface.
	void Serialize( FArchive& Ar );
	virtual void Rename( const TCHAR* InName=NULL, UObject* NewOuter=NULL );

	// UModel interface.
	virtual UBOOL PointCheck
	(
		FCheckResult	&Result,
		AActor			*Owner,
		FVector			Location,
		FVector			Extent
	);
	virtual UBOOL LineCheck
	(
		FCheckResult	&Result,
		AActor			*Owner,
		FVector			End,
		FVector			Start,
		FVector			Extent,
		DWORD			TraceFlags
	);

	void Modify( UBOOL DoTransArrays=0 );
	void BuildBound();
	void Transform( ABrush* Owner );
	void EmptyModel( INT EmptySurfInfo, INT EmptyPolys );
	void ShrinkModel();
	BYTE FastLineCheck(FVector End,FVector Start);	

	/**
	 * Returns the scale dependent texture factor used by the texture streaming code.	
	 *
	 * @return scale dependent texture factor
	 */
	FLOAT GetStreamingTextureFactor();

	/**
	 * Calculates the zones which contain a box-sphere bounding volume.
	 *
	 * @param Bounds	The bounding volume to calculate the intersecting zones for.
	 * @return			A bitmask with the bits corresponding to the intersecting zones set.
	 */
	FZoneSet GetZoneMask(const FBoxSphereBounds& Bounds) const;

	// UModel transactions.
	void ModifySelectedSurfs( UBOOL UpdateMaster );
	void ModifyAllSurfs( UBOOL UpdateMaster );
	void ModifySurf( INT Index, UBOOL UpdateMaster );

	// UModel collision functions.
	typedef void (*PLANE_FILTER_CALLBACK )(UModel *Model, INT iNode, int Param);
	typedef void (*SPHERE_FILTER_CALLBACK)(UModel *Model, INT iNode, int IsBack, int Outside, int Param);
	FPointRegion PointRegion( AZoneInfo* Zone, FVector Location ) const;
	FLOAT FindNearestVertex
	(
		const FVector	&SourcePoint,
		FVector			&DestPoint,
		FLOAT			MinRadius,
		INT				&pVertex
	) const;
	void PrecomputeSphereFilter
	(
		const FPlane	&Sphere
	);
	INT ConvexVolumeMultiCheck( FBox& Box, FPlane* Planes, INT NumPlanes, FVector Normal, TArray<INT>& Result );
};

//
//	FBitmapStrip - A RLE encoded strip of the visibility bitmap: 1-16 copies of a 4-bit greyscale value.
//

struct FBitmapStrip
{
private:

	union
	{
#ifdef XBOX
#pragma reverse_bitfield(on)
#endif
		struct
		{
			BYTE	Length	: 4,
					Value	: 4;
		};
#ifdef XBOX
#pragma reverse_bitfield(off)
#endif

		BYTE	Encoding;
	};

public:

	// Constructors.

	FBitmapStrip(UINT InValue)
	{
		check(sizeof(FBitmapStrip) == sizeof(BYTE));
		check(InValue < 16);
		Length = 0;
		Value = InValue & 0xf;
	}
	FBitmapStrip() {}

	// Accessors.

	UINT GetLength() const { return Length + 1; }
	UINT GetValue() const { return Value; }

	void IncLength()
	{
		check(Length < 15);
		Length++;
	}

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FBitmapStrip& S)
	{
		return Ar << S.Encoding;
	}
};

//
//	FStaticModelLightSurface - The visibility bitmap for a single BSP surface.
//

struct FStaticModelLightSurface
{
	UINT						SurfaceIndex;
	TArray<INT>					Nodes;
	TLazyArray<FBitmapStrip>	BitmapStrips;
	FVector						LightMapX,
								LightMapY,
								LightMapBase;
	FVector2D					Min,
								Max;
	UINT						BaseX,
								BaseY,
								SizeX,
								SizeY;

	// Constructors/destructor.

	FStaticModelLightSurface(
		UINT InSurfaceIndex,
		const TArray<INT>& InNodes,
		const FVector& InLightMapX,
		const FVector& InLightMapY,
		const FPlane& Plane,
		FLOAT MinX,
		FLOAT MinY,
		FLOAT MaxX,
		FLOAT MaxY,
		UINT InSizeX,
		UINT InSizeY
		);
	FStaticModelLightSurface() {}

	// Transform functions.

	FMatrix WorldToTexture() const;
	FMatrix TextureToWorld() const;

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FStaticModelLightSurface& B)
	{
		Ar << B.SurfaceIndex << B.Nodes << B.BitmapStrips << B.LightMapX << B.LightMapY << B.LightMapBase << B.Min << B.Max << B.BaseX << B.BaseY << B.SizeX << B.SizeY;
		return Ar;
	}
};

//
//	FStaticModelLightTexture
//

struct FStaticModelLightTexture: FTexture2D
{
	DECLARE_RESOURCE_TYPE(FStaticModelLightTexture,FTexture2D);

	TArray<FStaticModelLightSurface>	Surfaces;

	// Constructor.

	FStaticModelLightTexture(UINT InSizeX,UINT InSizeY);
	FStaticModelLightTexture();

	// FTexture2D interface.

	virtual void GetData(void* Buffer,UINT MipIndex,UINT StrideY);
	virtual UBOOL GetClampX() { return 1; }
	virtual UBOOL GetClampY() { return 1; }

	// FResource interface.

	virtual FString DescribeResource() { return TEXT("BSP shadow map"); }

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FStaticModelLightTexture& T)
	{
		return Ar << T.SizeX << T.SizeY << T.Surfaces;
	}
};

//
//	FStaticModelLight
//

struct FStaticModelLight
{
	class UModelComponent*						ModelComponent;
	ULightComponent*							Light;
	TIndirectArray<FStaticModelLightTexture>	Textures;

	// Constructors.

	FStaticModelLight(UModelComponent* InModelComponent,ULightComponent* InLight);
	FStaticModelLight() {}

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FStaticModelLight& L)
	{
		Ar << *(UObject**)&L.ModelComponent << L.Light << L.Textures;
		return Ar;
	}
};

//
//	UModelComponent - Represents the level's BSP tree.
//

class UModelComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UModelComponent,UPrimitiveComponent,0,Engine);

	ULevel*								Level;
	INT									ZoneIndex;
	UModel*								Model;
	struct FModelMeshObject*			MeshObject;
	TIndirectArray<FStaticModelLight>	StaticLights;
	TArray<ULightComponent*>			IgnoreLights;

	// InvalidateSurfaces - Called when a surface in the BSP changes.

	void InvalidateSurfaces();

	/**
	 * Returns the material and material instance resources that should be used for rendering.
	 *
	 * @param Context				The view that is being rendered.
	 * @param SurfMaterial			The material applied to the BSP surface.
	 * @param Selected				True if the surface is selected.
	 * @param OutMaterialInstance	On return, the material instance resource to use for rendering.
	 *
	 * @return The material resource to use for rendering.
	 */
	FMaterial* GetMaterialResources(const FSceneContext& Context,UMaterialInstance* SurfMaterial,UBOOL Selected,FMaterialInstance*& OutMaterialInstance) const;

	// UPrimitiveComponent interface.

	virtual EStaticLightingAvailability GetStaticLightPrimitive(ULightComponent* Light,FStaticLightPrimitive*& OutStaticLightPrimitive);
	virtual void CacheLighting();
	virtual void InvalidateLightingCache(ULightComponent* Light);

	virtual void UpdateBounds();
	virtual void GetZoneMask(UModel* Model);

	// FPrimitiveViewInterface interface.

	virtual DWORD GetLayerMask(const FSceneContext& Context) const;
	virtual void Render(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);
	virtual void RenderShadowVolume(const struct FSceneContext& Context,struct FShadowRenderInterface* PRI,ULightComponent* Light);
	virtual void RenderShadowDepth(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);
	virtual void RenderHitTest(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);

	// UActorComponent interface.

	virtual void Created();
	virtual void Update();
	virtual void Destroyed();

	virtual void Precache(FPrecacheInterface* P);

	virtual void InvalidateLightingCache();

	// UObject interface.

	virtual void Serialize(FArchive& Ar);
};
