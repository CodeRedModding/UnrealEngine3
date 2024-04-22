/*=============================================================================
	ConvexVolume.h: Convex volume definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * Encapsulates the inside and/or outside state of an intersection test.
 */
struct FOutcode
{
private:

	BITFIELD	Inside : 1,
				Outside : 1;

public:

	// Constructor.

	FOutcode():
		Inside(0), Outside(0)
	{}
	FOutcode(UBOOL InInside,UBOOL InOutside):
		Inside(InInside), Outside(InOutside)
	{}

	// Accessors.

	FORCEINLINE void SetInside(UBOOL NewInside) { Inside = NewInside; }
	FORCEINLINE void SetOutside(UBOOL NewOutside) { Outside = NewOutside; }
	FORCEINLINE UBOOL GetInside() const { return Inside; }
	FORCEINLINE UBOOL GetOutside() const { return Outside; }
};

//
//	FConvexVolume
//

struct FConvexVolume
{
public:

	typedef TArray<FPlane,TInlineAllocator<6> > FPlaneArray;
	typedef TArray<FPlane,TInlineAllocator<8> > FPermutedPlaneArray;

	FPlaneArray Planes;
	/** This is the set of planes pre-permuted to SSE/Altivec form */
	FPermutedPlaneArray PermutedPlanes;

	FConvexVolume()
	{
//		INT N = 5;
	}

	/**
	 * Builds the set of planes used to clip against. Also, puts the planes
	 * into a form more readily used by SSE/Altivec so 4 planes can be
	 * clipped against at once.
	 */
	FConvexVolume(const TArray<FPlane>& InPlanes) :
		Planes( InPlanes )
	{
		Init();
	}

	/**
	 * Builds the permuted planes for SSE/Altivec fast clipping
	 */
	void Init(void);

	/**
	 * Clips a polygon to the volume.
	 *
	 * @param	Polygon - The polygon to be clipped.  If the true is returned, contains the
	 *			clipped polygon.
	 *
	 * @return	Returns false if the polygon is entirely outside the volume and true otherwise.
	 */
	UBOOL ClipPolygon(class FPoly& Polygon) const;

	// Intersection tests.

	FOutcode GetBoxIntersectionOutcode(const FVector& Origin,const FVector& Extent) const;

	UBOOL IntersectBox(const FVector& Origin,const FVector& Extent) const;
	UBOOL IntersectBox(const FVector& Origin,const FVector& Extent, UBOOL& bOutFullyContained) const;
	UBOOL IntersectSphere(const FVector& Origin,const FLOAT& Radius) const;
	UBOOL IntersectSphere(const FVector& Origin,const FLOAT& Radius, UBOOL& bOutFullyContained) const;

	/**
	 * Intersection test with a translated axis-aligned box.
	 * @param Origin - Origin of the box.
	 * @param Translation - Translation to apply to the box.
	 * @param Extent - Extent of the box along each axis.
	 * @returns TRUE if this convex volume intersects the given translated box.
	 */
	UBOOL IntersectBox(const FVector& Origin,const FVector& Translation,const FVector& Extent) const;

	/**
	 * Serializor
	 *
	 * @param	Ar				Archive to serialize data to
	 * @param	ConvexVolume	Convex volumes to serialize to archive
	 *
	 * @return passed in archive
	 */
	friend FArchive& operator<<(FArchive& Ar,FConvexVolume& ConvexVolume);
};

/**
 * Creates a convex volume bounding the view frustum for a view-projection matrix.
 *
 * @param [out]	The FConvexVolume which contains the view frustum bounds.
 * @param		ViewProjectionMatrix - The view-projection matrix which defines the view frustum.
 * @param		bUseNearPlane - True if the convex volume should be bounded by the view frustum's near clipping plane.
 */
extern void GetViewFrustumBounds(FConvexVolume& OutResult, const FMatrix& ViewProjectionMatrix,UBOOL bUseNearPlane);
