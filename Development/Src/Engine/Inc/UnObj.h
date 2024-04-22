/*=============================================================================
	UnObj.h: Standard Unreal object definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
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
class				UChildConnection;
class		UInteraction;
class		UCheatManager;
class		UChannel;
class			UActorChannel;
class		UMaterialInterface;
class		FColor;
struct		FLightmassPrimitiveSettings;

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

	UBOOL operator==( const FZoneSet& Other ) const
	{
		return MaskBits == Other.MaskBits;
	}
	UBOOL operator!=( const FZoneSet& Other ) const
	{
		return MaskBits != Other.MaskBits;
	}

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
	FLightmassPrimitiveSettings
-----------------------------------------------------------------------------*/
/** 
 *	Per-object settings for Lightmass
 */
//@warning: this structure is manually mirrored in EngineTypes.uc
struct FLightmassPrimitiveSettings
{
	/** If TRUE, this object will be lit as if it receives light from both sides of its polygons. */
	BITFIELD	bUseTwoSidedLighting:1;
	/** If TRUE, this object will only shadow indirect lighting.  					*/
	BITFIELD	bShadowIndirectOnly:1;
	/** If TRUE, allow using the emissive for static lighting.						*/
	BITFIELD	bUseEmissiveForStaticLighting:1;
	/** Direct lighting falloff exponent for mesh area lights created from emissive areas on this primitive. */
	FLOAT		EmissiveLightFalloffExponent;
	/** 
	 * Direct lighting influence radius.  
	 * The default is 0, which means the influence radius should be automatically generated based on the emissive light brightness.
	 * Values greater than 0 override the automatic method.
	 */
	FLOAT		EmissiveLightExplicitInfluenceRadius;
	/** Scales the emissive contribution of all materials applied to this object.	*/
	FLOAT		EmissiveBoost;
	/** Scales the diffuse contribution of all materials applied to this object.	*/
	FLOAT		DiffuseBoost;
	/** Scales the specular contribution of all materials applied to this object.	*/
	FLOAT		SpecularBoost;
	/** Fraction of samples taken that must be occluded in order to reach full occlusion. */
	FLOAT		FullyOccludedSamplesFraction;

	FLightmassPrimitiveSettings()
	{
	}

	FLightmassPrimitiveSettings(ENativeConstructor)
		: bUseTwoSidedLighting(FALSE)
		, bShadowIndirectOnly(FALSE)
		, bUseEmissiveForStaticLighting(FALSE)
		, EmissiveLightFalloffExponent(2.0f)
		, EmissiveLightExplicitInfluenceRadius(0.0f)
		, EmissiveBoost(1.0f)
		, DiffuseBoost(1.0f)
		, SpecularBoost(1.0f)
		, FullyOccludedSamplesFraction(1.0f)
	{}

	explicit FORCEINLINE FLightmassPrimitiveSettings(EEventParm)
	{
		appMemzero(this, sizeof(FLightmassPrimitiveSettings));
	}

	friend UBOOL operator==(const FLightmassPrimitiveSettings& A, const FLightmassPrimitiveSettings& B)
	{
		//@lmtodo. Do we want a little 'leeway' in joining 
		if ((A.bUseTwoSidedLighting != B.bUseTwoSidedLighting) ||
			(A.bShadowIndirectOnly != B.bShadowIndirectOnly) || 
			(A.bUseEmissiveForStaticLighting != B.bUseEmissiveForStaticLighting) || 
			(fabsf(A.EmissiveLightFalloffExponent - B.EmissiveLightFalloffExponent) > SMALL_NUMBER) ||
			(fabsf(A.EmissiveLightExplicitInfluenceRadius - B.EmissiveLightExplicitInfluenceRadius) > SMALL_NUMBER) ||
			(fabsf(A.EmissiveBoost - B.EmissiveBoost) > SMALL_NUMBER) ||
			(fabsf(A.DiffuseBoost - B.DiffuseBoost) > SMALL_NUMBER) ||
			(fabsf(A.SpecularBoost - B.SpecularBoost) > SMALL_NUMBER) ||
			(fabsf(A.FullyOccludedSamplesFraction - B.FullyOccludedSamplesFraction) > SMALL_NUMBER))
		{
			return FALSE;
		}
		return TRUE;
	}

	// Functions.
	friend FArchive& operator<<(FArchive& Ar, FLightmassPrimitiveSettings& Settings);

	UBOOL CheckForInvalidSettings() const
	{
		return (EmissiveLightFalloffExponent < 0.0f) || 
			(EmissiveLightExplicitInfluenceRadius < 0.0f) ||
			(EmissiveLightExplicitInfluenceRadius > WORLD_MAX) ||
			(FullyOccludedSamplesFraction < 0.0f) ||
			(FullyOccludedSamplesFraction > 1.0f);
	}
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
#if CONSOLE
	// Store up to 4 vertices inline.
	typedef TArray<FVector,TInlineAllocator<4> > VerticesArrayType;
#else
	// Store up to 16 vertices inline.
	typedef TArray<FVector,TInlineAllocator<16> > VerticesArrayType;
#endif

	FVector				Base;					// Base point of polygon.
	FVector				Normal;					// Normal of polygon.
	FVector				TextureU;				// Texture U vector.
	FVector				TextureV;				// Texture V vector.
	VerticesArrayType	Vertices;
	DWORD				PolyFlags;				// FPoly & Bsp poly bit flags (PF_).
	ABrush*				Actor;					// Brush where this originated, or NULL.
	UMaterialInterface*	Material;				// Material.
	FName				RulesetVariation;		// Name of variation within a ProcBuilding Ruleset for this face
	FName				ItemName;				// Item name.
	INT					iLink;					// iBspSurf, or brush fpoly index of first identical polygon, or MAXWORD.
	INT					iBrushPoly;				// Index of editor solid's polygon this originated from.
	DWORD				SmoothingMask;			// A mask used to determine which smoothing groups this polygon is in.  SmoothingMask & (1 << GroupNumber)
	FLOAT				ShadowMapScale;			// The number of units/shadowmap texel on this surface.
	DWORD				LightingChannels;		// Lighting channels of affecting lights.
	
	// This MUST be the format of FLightmassPrimitiveSettings
	// The Lightmass settings for surfaces generated from this poly
 	FLightmassPrimitiveSettings		LightmassSettings;

	/**
	 * Constructor, initializing all member variables.
	 */
	FPoly();

	// Custom functions.
	void Init();
	void Reverse();
	void Transform(const FVector &PreSubtract,const FVector &PostAdd);
	int Fix();
	int CalcNormal( UBOOL bSilent = 0 );
	int SplitWithPlane(const FVector &Base,const FVector &Normal,FPoly *FrontPoly,FPoly *BackPoly,int VeryPrecise) const;
	int SplitWithNode(const UModel *Model,INT iNode,FPoly *FrontPoly,FPoly *BackPoly,int VeryPrecise) const;
	int SplitWithPlaneFast(const FPlane& Plane,FPoly *FrontPoly,FPoly *BackPoly) const;
	int Split(const FVector &Normal, const FVector &Base );
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
	UBOOL IsConvex();
	INT Triangulate( ABrush* InOwnerBrush, TArray<FPoly>& OutTriangles );
	INT GetVertexIndex( FVector& InVtx );
	FVector GetMidPoint();
	static FPoly BuildInfiniteFPoly(const FPlane& InPlane);
	static void OptimizeIntoConvexPolys( ABrush* InOwnerBrush, TArray<FPoly>& InPolygons );
	static void GetOutsideWindings( ABrush* InOwnerBrush, TArray<FPoly>& InPolygons, TArray< TArray<FVector> >& InWindings, UBOOL bFlipNormals = TRUE);

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FPoly& Poly );

	// Inlines.
	int IsBackfaced( const FVector &Point ) const
		{return ((Point-Base) | Normal) < 0.f;}
	int IsCoplanar( const FPoly &Test ) const
		{return Abs((Base - Test.Base)|Normal)<0.01f && Abs(Normal|Test.Normal)>0.9999f;}

	friend UBOOL operator==(const FPoly& A,const FPoly& B)
	{
		if(A.Vertices.Num() != B.Vertices.Num())
		{
			return FALSE;
		}

		for(INT VertexIndex = 0;VertexIndex < A.Vertices.Num();VertexIndex++)
		{
			if(A.Vertices(VertexIndex) != B.Vertices(VertexIndex))
			{
				return FALSE;
			}
		}

		return TRUE;
	}
	friend UBOOL operator!=(const FPoly& A,const FPoly& B)
	{
		return !(A == B);
	}
};

//
// List of FPolys.
//
class UPolys : public UObject
{
	DECLARE_CLASS_INTRINSIC(UPolys,UObject,0,Engine)

	// Elements.
	TTransArray<FPoly> Element;

	// Constructors.
	UPolys()
	: Element( this )
	{}

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	/**
	* Note that the object has been modified.  If we are currently recording into the 
	* transaction buffer (undo/redo), save a copy of this object into the buffer and 
	* marks the package as needing to be saved.
	*
	* @param	bAlwaysMarkDirty	if TRUE, marks the package dirty even if we aren't
	*								currently recording an active undo/redo transaction
	*/
	virtual void Modify(UBOOL bAlwaysMarkDirty = FALSE);

	// UObject interface.
	void Serialize( FArchive& Ar );

	/**
	 * Fixup improper load flags on save so that the load flags are always up to date
	 */
	void PreSave();
};

