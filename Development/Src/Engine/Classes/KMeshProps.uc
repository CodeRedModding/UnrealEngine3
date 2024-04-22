/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
// It would be nice if you could subclass structs in script.. ah well. Don't want overhead of making these UObjects.

class KMeshProps extends Object
	native
	noexport;

// KAggregateGeom and all Elems are in UNREAL scale.
// InertiaTensor, COMOffset & Volume are in PHYSICS scale.

struct KSphereElem
{
	var() editconst Matrix	TM;
	var() editconst float	Radius;
	
	/** Disable rigid body collision for this shape. */
	var()			bool	bNoRBCollision;

	/** Check against this shape even when per-poly collision is being used. */
	var()			bool	bPerPolyShape;

	structdefaultproperties
	{
		Radius=1
		TM=(XPlane=(X=1,Y=0,Z=0,W=0),YPlane=(X=0,Y=1,Z=0,W=0),ZPlane=(X=0,Y=0,Z=1,W=0),WPlane=(X=0,Y=0,Z=0,W=1))
	}
};

struct KBoxElem
{
	var() editconst Matrix	TM;
	var() editconst float	X, Y, Z; // length (not radius)
	
	/** Disable rigid body collision for this shape. */
	var()			bool	bNoRBCollision;

	/** Check against this shape even when per-poly collision is being used. */
	var()			bool	bPerPolyShape;

	structdefaultproperties
	{
		X=1
		Y=1
		Z=1
		TM=(XPlane=(X=1,Y=0,Z=0,W=0),YPlane=(X=0,Y=1,Z=0,W=0),ZPlane=(X=0,Y=0,Z=1,W=0),WPlane=(X=0,Y=0,Z=0,W=1))
	}
};

struct KSphylElem
{
	var() editconst Matrix	TM; // The transform assumes the sphyl axis points down Z.
	var() editconst float	Radius;
	var() editconst float	Length; // This is of line-segment ie. add Radius to both ends to find total length.

	/** Disable rigid body collision for this shape. */
	var()			bool	bNoRBCollision;

	/** Check against this shape even when per-poly collision is being used. */
	var()			bool	bPerPolyShape;

	structdefaultproperties
	{
		Radius=1
		Length=1
		TM=(XPlane=(X=1,Y=0,Z=0,W=0),YPlane=(X=0,Y=1,Z=0,W=0),ZPlane=(X=0,Y=0,Z=1,W=0),WPlane=(X=0,Y=0,Z=0,W=1))
	}
};

/** One convex hull, used for simplified collision. */
struct KConvexElem
{
	/** Array of indices that make up the convex hull. */
	var	array<vector>			VertexData;

	/** Array of planes holding the vertex data in SIMD order */
	var array<plane>			PermutedVertexData;

	/** Index buffer for triangles making up the faces of this convex hull. */
	var	array<int>				FaceTriData;

	/** All different directions of edges in this hull. */
	var array<vector>			EdgeDirections;

	/** All different directions of face normals in this hull. */
	var array<vector>			FaceNormalDirections;

	/** Array of the planes that make up this convex hull. */
	var array<plane>			FacePlaneData;

	/** Bounding box of this convex hull. */
	var	box						ElemBox;
};

struct KAggregateGeom
{
	var() editfixedsize array<KSphereElem>			SphereElems;
	var() editfixedsize array<KBoxElem>				BoxElems;
	var() editfixedsize array<KSphylElem>			SphylElems;
	var() editfixedsize array<KConvexElem>			ConvexElems;
	var native nontransactional noimport pointer	RenderInfo;

	/** Collision against this geom will not specially handle the "close and parallel" case.  Special-case. */
	var() bool										bSkipCloseAndParallelChecks; 
};

var() vector			COMNudge; // User-entered offset. UNREAL UNITS
var() KAggregateGeom	AggGeom;
