/*=============================================================================
	UnPhysPublic.h
	Rigid Body Physics Public Types
=============================================================================*/

#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,PROPERTY_ALIGNMENT)
#endif

// These need to be public for UnrealEd etc.
const FLOAT P2UScale = 50.0f;
const FLOAT U2PScale = 0.02f;

const FLOAT Rad2U = 10430.2192f;
const FLOAT U2Rad = 0.000095875262f;

/////////////////  Script Structs /////////////////

typedef struct _FRigidBodyState
{
	FVector		Position;
	FQuat		Quaternion;
	FVector		LinVel;
	FVector		AngVel;
} FRigidBodyState;


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

	FKSphereElem() {}

	FKSphereElem( FLOAT r ) 
	: Radius(r) {}

	void	DrawElemWire(struct FPrimitiveRenderInterface* PRI, const FMatrix& ElemTM, FLOAT Scale, const FColor Color);
	void	DrawElemSolid(struct FPrimitiveRenderInterface* PRI, const FMatrix& ElemTM, FLOAT Scale, FMaterial* Material, FMaterialInstance* MaterialInstance);
	FBox	CalcAABB(const FMatrix& BoneTM, FLOAT Scale);

	UBOOL	LineCheck(FCheckResult& Result, const FMatrix& Matrix,  FLOAT Scale, const FVector& End, const FVector& Start, const FVector& Extent);
};

class FKBoxElem
{
public:
	FMatrix TM;
	FLOAT X, Y, Z; // Length (not radius) in each dimension

	FKBoxElem() {}

	FKBoxElem( FLOAT s ) 
	: X(s), Y(s), Z(s) {}

	FKBoxElem( FLOAT InX, FLOAT InY, FLOAT InZ ) 
	: X(InX), Y(InY), Z(InZ) {}	

	void	DrawElemWire(struct FPrimitiveRenderInterface* PRI, const FMatrix& ElemTM, FLOAT Scale, const FColor Color);
	void	DrawElemSolid(struct FPrimitiveRenderInterface* PRI, const FMatrix& ElemTM, FLOAT Scale, FMaterial* Material, FMaterialInstance* MaterialInstance);

	FBox	CalcAABB(const FMatrix& BoneTM, FLOAT Scale);
	UBOOL	LineCheck(FCheckResult& Result, const FMatrix& Matrix, FLOAT Scale, const FVector& End, const FVector& Start, const FVector& Extent);
};

class FKSphylElem
{
public:
	FMatrix TM;
	FLOAT Radius;
	FLOAT Length;

	FKSphylElem() {}

	FKSphylElem( FLOAT InRadius, FLOAT InLength )
	: Radius(InRadius), Length(InLength) {}

	void	DrawElemWire(struct FPrimitiveRenderInterface* PRI, const FMatrix& ElemTM, FLOAT Scale, const FColor Color);
	void	DrawElemSolid(struct FPrimitiveRenderInterface* PRI, const FMatrix& ElemTM, FLOAT Scale, FMaterial* Material, FMaterialInstance* MaterialInstance);

	FBox	CalcAABB(const FMatrix& BoneTM, FLOAT Scale);
	UBOOL	LineCheck(FCheckResult& Result, const FMatrix& Matrix, FLOAT Scale, const FVector& End, const FVector& Start, const FVector& Extent);
};

class FKConvexElem
{
public:
	TArrayNoInit<FVector> VertexData;
	TArray<FPlane>  PlaneData;
	
	FKConvexElem() {}

	void	DrawElemWire(struct FPrimitiveRenderInterface* PRI, const FMatrix& ElemTM, const FVector& Scale3D, const FColor Color);
	void	DrawElemSolid(struct FPrimitiveRenderInterface* PRI, const FMatrix& ElemTM, const FVector& Scale3D, FMaterial* Material, FMaterialInstance* MaterialInstance);

	FBox	CalcAABB(const FMatrix& BoneTM, FLOAT Scale);
	UBOOL	LineCheck(FCheckResult& Result, const FMatrix& Matrix, const FVector& Scale3D, const FVector& End, const FVector& Start, const FVector& Extent);
};

// A static mesh can have a pointer to one of these things, that describes
// the collision geometry used by the rigid body physics. This a collection of primitives, each with a
// transformation matrix from the mesh origin.
class FKAggregateGeom
{
public:
	TArrayNoInit<FKSphereElem>		SphereElems;
	TArrayNoInit<FKBoxElem>			BoxElems;
	TArrayNoInit<FKSphylElem>		SphylElems;
	TArrayNoInit<FKConvexElem>		ConvexElems;

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

#ifdef WITH_NOVODEX
	class NxActorDesc*	InstanceNovodexGeom(const FVector& uScale3D, const TCHAR* debugName);
#endif // WITH_NOVODEX

	void	DrawAggGeom(struct FPrimitiveRenderInterface* PRI, const FMatrix& ParentTM, const FVector& Scale3D,	const FColor Color, UBOOL bNoConvex = false);

	FBox	CalcAABB(const FMatrix& BoneTM, FLOAT Scale);
	UBOOL	LineCheck(FCheckResult& Result, const FMatrix& Matrix, const FVector& Scale3D, const FVector& End, const FVector& Start, const FVector& Extent);
};

// This is the mass data (inertia tensor, centre-of-mass offset) optionally saved along with the graphics.
// This applies to the mesh at default scale, and with a mass of 1.
// This is in PHYSICS scale.

class UKMeshProps : public UObject
{
	DECLARE_CLASS(UKMeshProps,UObject,0,Engine);

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
void	KModelToHulls(FKAggregateGeom* outGeom, UModel* inModel, const FVector& prePivot);
UBOOL	ExecRBCommands(const TCHAR* Cmd, FOutputDevice* Ar, ULevel* level);

QWORD	RBActorsToKey(AActor* a1, AActor* a2);
QWORD	RigidBodyIndicesToKey(INT Body1Index, INT Body2Index);

FMatrix FindBodyMatrix(AActor* Actor, FName BoneName);
FBox	FindBodyBox(AActor* Actor, FName BoneName);

#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif
