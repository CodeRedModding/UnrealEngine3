// It would be nice if you could subclass structs in script.. ah well. Don't want overhead of making these UObjects.

class KMeshProps extends Object
	native
	noexport;

// KAggregateGeom and all Elems are in UNREAL scale.
// InertiaTensor, COMOffset & Volume are in PHYSICS scale.

struct KSphereElem
{
	var() Matrix	TM;
	var() float	Radius;
	
	structdefaults
	{
		Radius=1
		TM=(XPlane=(X=1,Y=0,Z=0,W=0),YPlane=(X=0,Y=1,Z=0,W=0),ZPlane=(X=0,Y=0,Z=1,W=0),WPlane=(X=0,Y=0,Z=0,W=1))
	};
};

struct KBoxElem
{
	var() Matrix	TM;
	var() float	X, Y, Z; // length (not radius)

	structdefaults
	{
		X=1
		Y=1
		Z=1
		TM=(XPlane=(X=1,Y=0,Z=0,W=0),YPlane=(X=0,Y=1,Z=0,W=0),ZPlane=(X=0,Y=0,Z=1,W=0),WPlane=(X=0,Y=0,Z=0,W=1))
	};
};

struct KSphylElem
{
	var() Matrix	TM; // The transform assumes the sphyl axis points down Z.
	var() float		Radius;
	var() float		Length; // This is of line-segment ie. add Radius to both ends to find total length.

	structdefaults
	{
		Radius=1
		Length=1
		TM=(XPlane=(X=1,Y=0,Z=0,W=0),YPlane=(X=0,Y=1,Z=0,W=0),ZPlane=(X=0,Y=0,Z=1,W=0),WPlane=(X=0,Y=0,Z=0,W=1))
	};
};

struct KConvexElem
{
	var() array<vector>				VertexData;
	var transient array<plane>		PlaneData;
};

struct KAggregateGeom
{
	var() array<KSphereElem>		SphereElems;
	var() array<KBoxElem>			BoxElems;
	var() array<KSphylElem>			SphylElems;
	var() array<KConvexElem>		ConvexElems;
};

var() vector			COMNudge; // User-entered offset. UNREAL UNITS
var() KAggregateGeom	AggGeom;
