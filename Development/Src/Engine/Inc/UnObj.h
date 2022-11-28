/*=============================================================================
	UnObj.h: Standard Unreal object definitions.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

/*----------------------------------------------------------------------------
	Forward declarations.
----------------------------------------------------------------------------*/

// All engine classes.
class		UModel;
class		UPolys;
class		ULevelBase;
class			ULevel;
class			UPendingLevel;
class		UPlayer;
class			UViewport;
class			UNetConnection;
class		UInteraction;
class		UCheatManager;
class		UChannel;
class			UActorChannel;
class		UMaterialInstance;
class		FColor;

// Other classes.
class  AActor;
class  ABrush;
class  UTerrainSector;
class  APhysicsVolume;

// Typedefs
typedef void* HMeshAnim;

/**
 * A set of zone indices.
 * Uses a fixed size bitmask for storage.
 */
struct FZoneSet
{
	FZoneSet(): MaskBits(0) {}

	// Pre-defined sets.

	static FZoneSet IndividualZone(INT ZoneIndex) { return FZoneSet(((QWORD)1) << ZoneIndex); }
	static FZoneSet AllZones() { return FZoneSet(~(QWORD)0); }
	static FZoneSet NoZones() { return FZoneSet(0); }

	// Accessors.

	UBOOL ContainsZone(INT ZoneIndex) const
	{
		return (MaskBits & (((QWORD)1) << ZoneIndex)) != 0;
	}

	void AddZone(INT ZoneIndex)
	{
		MaskBits |= (((QWORD)1) << ZoneIndex);
	}

	void RemoveZone(INT ZoneIndex)
	{
		MaskBits &= ~(((QWORD)1) << ZoneIndex);
	}

	UBOOL IsEmpty() const { return MaskBits == 0; }
	UBOOL IsNotEmpty() const { return MaskBits != 0; }

	// Operators.

	friend FZoneSet operator|(const FZoneSet& A,const FZoneSet& B)
	{
		return FZoneSet(A.MaskBits | B.MaskBits);
	}

	friend FZoneSet& operator|=(FZoneSet& A,const FZoneSet& B)
	{
		A.MaskBits |= B.MaskBits;
		return A;
	}

	friend FZoneSet operator&(const FZoneSet& A,const FZoneSet& B)
	{
		return FZoneSet(A.MaskBits & B.MaskBits);
	}

	// Serialization.

	friend FArchive& operator<<(FArchive& Ar,FZoneSet& S)
	{
		return Ar << S.MaskBits;
	}

private:

	FZoneSet(QWORD InMaskBits): MaskBits(InMaskBits) {}

	/** A mask containing a bit representing set inclusion for each of the 64 zones. */
	QWORD	MaskBits;
};

/*-----------------------------------------------------------------------------
	UPolys.
-----------------------------------------------------------------------------*/

// Results from FPoly.SplitWithPlane, describing the result of splitting
// an arbitrary FPoly with an arbitrary plane.
enum ESplitType
{
	SP_Coplanar		= 0, // Poly wasn't split, but is coplanar with plane
	SP_Front		= 1, // Poly wasn't split, but is entirely in front of plane
	SP_Back			= 2, // Poly wasn't split, but is entirely in back of plane
	SP_Split		= 3, // Poly was split into two new editor polygons
};

//
// A general-purpose polygon used by the editor.  An FPoly is a free-standing
// class which exists independently of any particular level, unlike the polys
// associated with Bsp nodes which rely on scads of other objects.  FPolys are
// used in UnrealEd for internal work, such as building the Bsp and performing
// boolean operations.
//
class FPoly
{
public:
	enum {MAX_VERTICES=16}; // Maximum vertices an FPoly may have.
	enum {VERTEX_THRESHOLD=MAX_VERTICES-2}; // Threshold for splitting into two.

	FVector				Base;					// Base point of polygon.
	FVector				Normal;					// Normal of polygon.
	FVector				TextureU;				// Texture U vector.
	FVector				TextureV;				// Texture V vector.
	FVector				Vertex[MAX_VERTICES];	// Actual vertices.
	DWORD				PolyFlags;				// FPoly & Bsp poly bit flags (PF_).
	ABrush*				Actor;					// Brush where this originated, or NULL.
	UMaterialInstance*	Material;				// Material.
	FName				ItemName;				// Item name.
	INT					NumVertices;			// Number of vertices.
	INT					iLink;					// iBspSurf, or brush fpoly index of first identical polygon, or MAXWORD.
	INT					iBrushPoly;				// Index of editor solid's polygon this originated from.
	DWORD				SmoothingMask;			// A mask used to determine which smoothing groups this polygon is in.  SmoothingMask & (1 << GroupNumber)
	FLOAT				LightMapScale;			// The number of units/lightmap texel on this surface.

	// Custom functions.
	void Init();
	void Reverse();
	void SplitInHalf(FPoly *OtherHalf);
	void Transform(const FVector &PreSubtract,const FVector &PostAdd);
	int Fix();
	int CalcNormal( UBOOL bSilent = 0 );
	int SplitWithPlane(const FVector &Base,const FVector &Normal,FPoly *FrontPoly,FPoly *BackPoly,int VeryPrecise) const;
	int SplitWithNode(const UModel *Model,INT iNode,FPoly *FrontPoly,FPoly *BackPoly,int VeryPrecise) const;
	int SplitWithPlaneFast(const FPlane Plane,FPoly *FrontPoly,FPoly *BackPoly) const;
	int Split(const FVector &Normal, const FVector &Base, int NoOverflow=0 );
	int RemoveColinears();
	int Finalize( ABrush* InOwner, int NoError );
	int Faces(const FPoly &Test) const;
	FLOAT Area();
	UBOOL DoesLineIntersect( FVector Start, FVector End, FVector* Intersect = NULL );
	UBOOL OnPoly( FVector InVtx );
	UBOOL OnPlane( FVector InVtx );
	void InsertVertex( INT InPos, FVector InVtx );
	void RemoveVertex( FVector InVtx );
	UBOOL IsCoplanar();
	INT Triangulate( ABrush* InOwner );
	INT GetVertexIndex( FVector& InVtx );
	FVector GetMidPoint();

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FPoly& Poly );

	// Inlines.
	int IsBackfaced( const FVector &Point ) const
		{return ((Point-Base) | Normal) < 0.f;}
	int IsCoplanar( const FPoly &Test ) const
		{return Abs((Base - Test.Base)|Normal)<0.01f && Abs(Normal|Test.Normal)>0.9999f;}

	inline UBOOL operator==( FPoly Other )
	{
		if( NumVertices != Other.NumVertices )
			return 0;

		for( int x = 0 ; x < NumVertices ; x++ )
			if( Vertex[x] != Other.Vertex[x] )
				return 0;

		return 1;
	}
	inline UBOOL operator!=( FPoly Other )
	{
		if( NumVertices != Other.NumVertices )
			return 1;

		for( int x = 0 ; x < NumVertices ; x++ )
			if( Vertex[x] != Other.Vertex[x] )
				return 1;

		return 0;
	}
};

//
// List of FPolys.
//
class UPolys : public UObject
{
	DECLARE_CLASS(UPolys,UObject,CLASS_RuntimeStatic,Engine)

	// Elements.
	TTransArray<FPoly> Element;

	// Constructors.
	UPolys()
	: Element( this )
	{}

	// UObject interface.
	void Serialize( FArchive& Ar )
	{
		Super::Serialize( Ar );
		if( Ar.IsTrans() )
		{
			Ar << Element;
		}
		else
		{
			Element.CountBytes( Ar );
			INT DbNum=Element.Num(), DbMax=DbNum;
			Ar << DbNum << DbMax;
			if( Ar.IsLoading() )
			{
				Element.Empty( DbNum );
				Element.AddZeroed( DbNum );
			}
			for( INT i=0; i<Element.Num(); i++ )
				Ar << Element(i);
		}
	}
};


/*-----------------------------------------------------------------------------
	FVerts.
-----------------------------------------------------------------------------*/

//
// One vertex associated with a Bsp node's polygon.  Contains a vertex index
// into the level's FPoints table, and a unique number which is common to all
// other sides in the level which are cospatial with this side.
//
class FVert
{
public:
	// Variables.
	INT 	pVertex;	// Index of vertex.
	INT		iSide;		// If shared, index of unique side. Otherwise INDEX_NONE.

	// Functions.
	friend FArchive& operator<< (FArchive &Ar, FVert &Vert)
	{
		return Ar << Vert.pVertex << Vert.iSide;
	}
};
