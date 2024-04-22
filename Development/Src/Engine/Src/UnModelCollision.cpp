/*=============================================================================
    UnPhys.cpp: Simple physics and occlusion testing for editor
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

DECLARE_CYCLE_STAT(TEXT("BSP Line Check"),STAT_BSPZeroExtentTime,STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("BSP Extent Check"),STAT_BSPExtentTime,STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("BSP Point Check"),STAT_BSPPointTime,STATGROUP_Collision);

/*---------------------------------------------------------------------------------------
   Primitive BoxCheck support.
---------------------------------------------------------------------------------------*/

//
// Info used by all hull box collision routines.
//
struct FBoxCheckInfo
{
	// Hull flags.
	enum EConvolveFlags
	{
		CV_XM = 0x01,
		CV_XP = 0x02,
		CV_YM = 0x04,
		CV_YP = 0x08,
		CV_ZM = 0x10,
		CV_ZP = 0x20,
	};

	// Hull variables.
	FCheckResult&	Hit;
	UModel&			Model;
	AActor*			Owner;
	FMatrix			Matrix;
	FVector			Extent;
	DWORD			ExtraFlags;
	INT				NumHulls;
	FVector			LocalHit;
	FBox			Box;
	FLOAT			T0,T1;
	FMatrix			MatrixTA;
	FLOAT			DetM;

	// Hull info.
	FPlane			Hulls[64];
	INT				Flags[64];
	const INT		*HullNodes;

	// Constructor.
	FBoxCheckInfo
	(
		FCheckResult&	InHit,
		UModel&			InModel,
		AActor*			InOwner,
		const FMatrix*	InOwnerLocalToWorld,
		FVector			InExtent,
		DWORD			InExtraFlags
	)
	:	Hit				(InHit)
	,	Model			(InModel)
	,	Owner			(InOwner)
	,	Matrix			((InOwnerLocalToWorld && InOwner) ? *InOwnerLocalToWorld : (InOwner ? InOwner->LocalToWorld() : FMatrix::Identity))
	,	Extent			(InExtent)
	,	ExtraFlags		(InExtraFlags)
	,	Box				(0)
	{
		MatrixTA = Matrix.TransposeAdjoint();
		DetM = Matrix.Determinant();
	}

	// Functions.
	void SetupHulls( const FBspNode& Node )
	{
		// Get nodes on this leaf's collision hull.
		HullNodes = &Model.LeafHulls( Node.iCollisionBound );
		for( NumHulls=0; HullNodes[NumHulls]!=INDEX_NONE && NumHulls<ARRAY_COUNT(Hulls); NumHulls++ )
		{
			FPlane& Hull = Hulls[NumHulls];
			Hull         = Model.Nodes(HullNodes[NumHulls] & ~0x40000000).Plane;
			if( Owner )
				Hull = Hull.TransformByUsingAdjointT( Matrix, DetM, MatrixTA );
			if( HullNodes[NumHulls] & 0x40000000 )
				Hull = Hull.Flip();
			Flags[NumHulls]
			=	((Hull.X < 0.f) ? CV_XM : (Hull.X > 0.f) ? CV_XP : 0)
			|	((Hull.Y < 0.f) ? CV_YM : (Hull.Y > 0.f) ? CV_YP : 0)
			|	((Hull.Z < 0.f) ? CV_ZM : (Hull.Z > 0.f) ? CV_ZP : 0);
		}

		// Get precomputed maxima.
		const FLOAT* Temp = (FLOAT*)&Model.LeafHulls( Node.iCollisionBound + NumHulls + 1);
		Box.Min.X = Temp[0]; Box.Min.Y = Temp[1]; Box.Min.Z = Temp[2];
		Box.Max.X = Temp[3]; Box.Max.Y = Temp[4]; Box.Max.Z = Temp[5];
	}
};

// Hull edge convolution macro.
#define CONVOLVE_EDGE(a,b,c,mask) \
{ \
	if( (Or2 & (mask)) == (mask) ) \
	{ \
		FVector C1 = FVector(a,b,c)^Hulls[i]; \
		FVector C2 = FVector(a,b,c)^Hulls[j]; \
		if( (C1 | C2) > 0.001f ) \
		{ \
			FVector I,D;\
			FIntersectPlanes2( I, D, Hulls[i], Hulls[j] ); \
			FVector A = (FVector(a,b,c) ^ D).UnsafeNormal(); \
			if( (FVector(Hulls[i]) | A) < 0.f ) \
				A *= -1.f; \
			if( !ClipTo( FPlane(I,A), INDEX_NONE ) ) \
				goto NoBlock; \
		} \
	} \
}

// Collision primitive clipper.
#define CLIP_COLLISION_PRIMITIVE \
{ \
	/* Check collision against hull planes. */ \
	FVector Hit=FVector(0,0,0); \
	for( int i=0; i<NumHulls; i++ ) \
		if( !ClipTo( Hulls[i], HullNodes[i] & ~0x40000000) ) \
			goto NoBlock; \
	\
	/* Check collision against hull extent, minus small epsilon so flat hit nodes are identified. */ \
	if( !Owner ) \
	{ \
		if \
		(   !ClipTo( FPlane( 0.f, 0.f,-1.0,0.1-Box.Min.Z), INDEX_NONE ) \
		||	!ClipTo( FPlane( 0.f, 0.f,+1.0,Box.Max.Z+0.1), INDEX_NONE ) \
		||	!ClipTo( FPlane(-1.f, 0.f, 0.0,0.1-Box.Min.X), INDEX_NONE ) \
		||	!ClipTo( FPlane(+1.f, 0.f, 0.0,Box.Max.X-0.1), INDEX_NONE ) \
		||  !ClipTo( FPlane( 0.f,-1.f, 0.0,0.1-Box.Min.Y), INDEX_NONE ) \
		||	!ClipTo( FPlane( 0.f,+1.f, 0.0,Box.Max.Y-0.1), INDEX_NONE ) ) \
			goto NoBlock; \
	} \
	else \
	{ \
		FMatrix TA = Matrix.TransposeAdjoint(); \
		FLOAT DetM = Matrix.Determinant(); \
		if \
		(   !ClipTo( FPlane( 0.f, 0.f,-1.0,0.1-Box.Min.Z).TransformByUsingAdjointT(Matrix, DetM, TA), INDEX_NONE ) \
		||	!ClipTo( FPlane( 0.f, 0.f,+1.0,Box.Max.Z+0.1).TransformByUsingAdjointT(Matrix, DetM, TA), INDEX_NONE ) \
		||	!ClipTo( FPlane(-1.f, 0.f, 0.0,0.1-Box.Min.X).TransformByUsingAdjointT(Matrix, DetM, TA), INDEX_NONE ) \
		||	!ClipTo( FPlane(+1.f, 0.f, 0.0,Box.Max.X-0.1).TransformByUsingAdjointT(Matrix, DetM, TA), INDEX_NONE ) \
		||  !ClipTo( FPlane( 0.f,-1.f, 0.0,0.1-Box.Min.Y).TransformByUsingAdjointT(Matrix, DetM, TA), INDEX_NONE ) \
		||	!ClipTo( FPlane( 0.f,+1.f, 0.0,Box.Max.Y-0.1).TransformByUsingAdjointT(Matrix, DetM, TA), INDEX_NONE ) ) \
			goto NoBlock; \
	} \
	\
	/* Check collision against hull edges. */ \
	for( int i=0; i<NumHulls; i++ ) \
	{ \
		for( int j=0; j<i; j++ ) \
		{ \
			/* Convolve with edge. */ \
			INT Or2 = Flags[i] | Flags[j]; \
			CONVOLVE_EDGE(1,0,0,CV_XM|CV_XP); \
			CONVOLVE_EDGE(0,1,0,CV_YM|CV_YP); \
			CONVOLVE_EDGE(0,0,1,CV_ZM|CV_ZP); \
		} \
	} \
}

/*---------------------------------------------------------------------------------------
   Primitive PointCheck.
---------------------------------------------------------------------------------------*/

//
// Box check worker class.
//
struct FBoxPointCheckInfo : public FBoxCheckInfo
{
	// Variables.
	const FVector		Point;
	FLOAT				BestDist;

	// Constructor.
	FBoxPointCheckInfo
	(
		FCheckResult		&InHit,
		UModel				&InModel,
		AActor*				InOwner,
		const FMatrix*		InOwnerLocalToWorld,
		FVector				InPoint,
		FVector				InExtent,
		DWORD				InExtraFlags
	)
	:	FBoxCheckInfo	    (InHit, InModel, InOwner, InOwnerLocalToWorld, InExtent, InExtraFlags)
	,	Point				(InPoint)
	,	BestDist			(100000)
	{}

	// Functions.
	UBOOL ClipTo( const FPlane& Hull, INT Item )
	{
		FLOAT Push = FBoxPushOut( Hull, Extent );
		FLOAT Dist = Hull.PlaneDot( Point );
		if( Dist>0 && Dist<BestDist && Dist<Push )
		{
			BestDist      = Dist;
			Hit.Location  = Point + 1.02f * Hull * (Push - Dist);
			Hit.Normal    = Hull;
			Hit.Actor     = Owner;
			Hit.Item      = Item;
			Hit.Time      = 0.f;
		}
		return Dist < Push;
	}
	UBOOL BoxPointCheck( INT iParent, INT iNode, UBOOL Outside )
	{
		UBOOL Result = 1;
		while( iNode != INDEX_NONE )
		{
			// Compute distance between start and end points and this node's plane.
			const  FBspNode& Node = Model.Nodes( iNode );
			FPlane Plane; if (Owner) Plane = Node.Plane.TransformByUsingAdjointT(Matrix, DetM, MatrixTA); else Plane = Node.Plane;
			FLOAT  PushOut        = FBoxPushOut   ( Plane, Extent * 1.1f );
			FLOAT  Dist           = Plane.PlaneDot( Point );

			// Recurse with front.
			if( Dist > -PushOut )
				if( !BoxPointCheck( iNode, Node.iFront, Outside || Node.IsCsg(ExtraFlags) ) )
					Result = 0;

			// Loop with back.
			iParent = iNode;
			iNode   = Node.iBack;
			Outside = Outside && !Node.IsCsg(ExtraFlags);
			if( Dist > PushOut )
				goto NoBlock;
		}
		if( !Outside && Model.Nodes(iParent).iCollisionBound!=INDEX_NONE )
		{
#if 0
			while( iParent != INDEX_NONE )
			{
				FBspNode& Parent = Model->Nodes->Element(iParent);
				NewSetupHulls( Parent );
				CLIP_COLLISION_PRIMITIVE;
				iParent = Parent.iPlane;
			}
#else
			// Reached a solid leaf, so setup hulls and clip it.
			SetupHulls(Model.Nodes(iParent));
			CLIP_COLLISION_PRIMITIVE;
#endif
			// We hit.
			Result = 0;
		}
		NoBlock:;
		return Result;
	}
};

//
// See if a box with the specified collision info fits at Point.  If it fits, returns 1.
//
UBOOL UModel::PointCheck
(
	FCheckResult&	Hit,
	AActor*			Owner,
	const FMatrix*	OwnerLocalToWorld,
	FVector			Location,
	FVector			Extent
)
{
	// Perform the check.
	Hit.Normal    = FVector(0,0,0);
	Hit.Location  = Location;
	Hit.Actor     = Owner;
	Hit.Time      = 0.f;
	UBOOL Outside = RootOutside;
	if( Nodes.Num() )
	{
		if( Extent != FVector(0,0,0) )
		{
			// Perform expensive box convolution check.
			FBoxPointCheckInfo Check( Hit, *this, Owner, OwnerLocalToWorld, Location, Extent, 0 );
			Outside = Check.BoxPointCheck( 0, 0, Outside );
			check(Hit.Actor==Owner);
		}
		else
		{
			// Perform simple point check.
			INT iPrevNode = INDEX_NONE, iNode=0;
			UBOOL IsFront=0;

			// Check for a Owner + offset matrix
			FMatrix Matrix = OwnerLocalToWorld && Owner ? *OwnerLocalToWorld :
				// Check for just an Owner matrix
				Owner ? Owner->LocalToWorld() :
				// BSP is in world space
				FMatrix::Identity;
			FMatrix MatrixTA = Matrix.TransposeAdjoint();
			FLOAT DetM = Matrix.Determinant();

			do
			{
				iPrevNode = iNode;
				const FBspNode& Node = Nodes(iNode);
				IsFront = Node.Plane.TransformByUsingAdjointT(Matrix, DetM, MatrixTA).PlaneDot(Location) > 0.f;
				Outside = Node.ChildOutside( IsFront, Outside );
				iNode   = Node.iChild[IsFront];
			} while( iNode != INDEX_NONE );
			Hit.Item = iPrevNode*2 + IsFront;
		}
	}

	return Outside;
}

void UModel::GetSurfacePlanes(
	const AActor*	Owner,
	TArray<FPlane>& OutPlanes)
{
	if( Nodes.Num() )
	{
		const FMatrix Matrix = 
			// Check for just an Owner matrix
			Owner ? Owner->LocalToWorld() :
			// BSP is in world space
			FMatrix::Identity;
		const FMatrix MatrixTA = Matrix.TransposeAdjoint();
		const FLOAT DetM = Matrix.Determinant();

		for (INT SurfaceIndex = 0; SurfaceIndex < Surfs.Num(); SurfaceIndex++)
		{
			OutPlanes.AddItem(Surfs(SurfaceIndex).Plane.TransformByUsingAdjointT(Matrix, DetM, MatrixTA));
		}
	}
}

/*---------------------------------------------------------------------------------------
   Fast line check.
---------------------------------------------------------------------------------------*/

// Fast line check.
static FBspNode* GLineCheckNodes;
static BYTE LineCheckInner( INT iNode, FVector End, FVector Start, BYTE Outside )
{
	while( iNode != INDEX_NONE )
	{
		const FBspNode&	Node = GLineCheckNodes[iNode];
		FLOAT Dist1	         = Node.Plane.PlaneDot(Start);
		FLOAT Dist2	         = Node.Plane.PlaneDot(End  );
		BYTE  NotCsg         = Node.NodeFlags & NF_NotCsg;
		INT   G1             = *(INT*)&Dist1 >= 0;
		INT   G2             = *(INT*)&Dist2 >= 0;
		if( G1!=G2 )
		{
			FVector Middle;
			FLOAT Alpha = Dist1/(Dist1-Dist2);
			Middle.X    = Start.X + (End.X-Start.X) * Alpha;
			Middle.Y    = Start.Y + (End.Y-Start.Y) * Alpha;
			Middle.Z    = Start.Z + (End.Z-Start.Z) * Alpha;
			if( !LineCheckInner(Node.iChild[G2],Middle,End,G2^((G2^Outside) & NotCsg)) )
				return 0;
			End = Middle;
		}
		Outside = G1^((G1^Outside)&NotCsg);
		iNode   = Node.iChild[G1];
	}
	return Outside;
}
BYTE UModel::FastLineCheck( FVector End, FVector Start )
{
	SCOPE_CYCLE_COUNTER(STAT_BSPZeroExtentTime);

	GLineCheckNodes = &Nodes(0);
	return Nodes.Num() ? LineCheckInner(0,End,Start,RootOutside) : RootOutside;
}

/*---------------------------------------------------------------------------------------
   LineCheck support.
---------------------------------------------------------------------------------------*/

//
// Recursive minion of UModel::LineCheck.
//
static UBOOL GOutOfCorner;
UBOOL LineCheck
(
	FCheckResult&	Hit,
	UModel&			Model,
	const FMatrix*	Matrix,
	INT  			iHit,
	INT				iNode,
	FVector			End, 
	FVector			Start,
	UBOOL			Outside,
	DWORD			InNodeFlags
)
{
	// Pre-calculate the adjoint, for transforming planes into world space.
	FMatrix TA = Matrix ? Matrix->TransposeAdjoint() : FMatrix::Identity;
	FLOAT DetM = Matrix ? Matrix->Determinant(): 1.0f;

	while( iNode != INDEX_NONE )
	{
		const FBspNode*	Node = &Model.Nodes(iNode);

		// Check side-of-plane for both points.
		FLOAT Dist1	= Matrix ? Node->Plane.TransformByUsingAdjointT(*Matrix, DetM, TA).PlaneDot(Start) : Node->Plane.PlaneDot(Start);
		FLOAT Dist2	= Matrix ? Node->Plane.TransformByUsingAdjointT(*Matrix, DetM, TA).PlaneDot(End)   : Node->Plane.PlaneDot(End);

		// Classify line based on both distances.
		if( Dist1>-0.001f && Dist2>-0.001f )
		{
			// Both points are in front.
			Outside |= Node->IsCsg(InNodeFlags & ~NF_BrightCorners);
			iNode    = Node->iFront;
		}
		else if( Dist1<0.001f && Dist2<0.001f )
		{
			// Both points are in back.
			Outside = Outside && !Node->IsCsg(InNodeFlags & ~NF_BrightCorners);
			iNode    = Node->iBack;
		}
		else
		{
			// Line is split and guranteed to be non-parallel to plane, so TimeDenominator != 0.
			FVector Middle     = Start + (Start-End) * (Dist1/(Dist2-Dist1));
			INT     FrontFirst = Dist1>0.f;

			// Recurse with front part.
			if( !LineCheck( Hit, Model, Matrix, iHit, Node->iChild[FrontFirst], Middle, Start, Node->ChildOutside(FrontFirst,Outside,InNodeFlags), InNodeFlags ) )
				return 0;

			// Loop with back part.
			Outside = Node->ChildOutside( 1-FrontFirst, Outside, InNodeFlags );
			iHit    = iNode;
			iNode   = Node->iChild[1-FrontFirst];
			Start   = Middle;
		}
	}
	if( !Outside )
	{
		// We have encountered the first collision.
		if( GOutOfCorner || !(InNodeFlags&NF_BrightCorners) )
		{
		Hit.Location  = Start;
		Hit.Normal    = Model.Nodes(iHit).Plane;
		Hit.Item      = iHit;		
		}
		else Outside=1;
	}
	else GOutOfCorner=1;
	return Outside;
}

/*---------------------------------------------------------------------------------------
   Primitive LineCheck.
---------------------------------------------------------------------------------------*/

//
// Tracing worker class.
//
struct FBoxLineCheckInfo : public FBoxCheckInfo
{
	// Variables.
	const FVector		End;
	const FVector		Start;
	FVector				Vector;
	FLOAT				Dist;
	UBOOL				DidHit;

	// Constructor.
	FBoxLineCheckInfo
	(
		FCheckResult&	Hit,
		UModel&			InModel,
		AActor*			InOwner,
		const FMatrix*	InOwnerLocalToWorld,
		FVector			InEnd,
		FVector			InStart,
		FVector			InExtent,
		DWORD			InExtraFlags
	)
	:	FBoxCheckInfo	(Hit, InModel, InOwner, InOwnerLocalToWorld, InExtent, InExtraFlags)
	,	End				(InEnd)
	,	Start			(InStart)
	,	Vector			(InEnd-InStart)
	,	Dist			(Vector.Size())
	,	DidHit			(0)
	{}

	// Tracer.
	UBOOL ClipTo( const FPlane& Hull, INT Item )
	{
		FLOAT PushOut = FBoxPushOut( Hull, Extent );
		FLOAT D0      = Hull.PlaneDot(Start);
		FLOAT D1      = Hull.PlaneDot(End);

		FLOAT AdjD0 = D0-PushOut;
		if( D0>D1 && AdjD0>=-PushOut && AdjD0<0 )
			AdjD0 = 0.f;

		FLOAT T = (AdjD0)/(D0-D1);

		if     ( (D0-D1) < -0.00001f      )	{ if( T < T1 ) { T1 = T; }                  }
		else if( (D0-D1) > +0.00001f      ) { if( T > T0 ) { T0 = T; LocalHit = Hull; } }
		else if( D0>PushOut && D1>PushOut ) { return 0;                                 }

		return T0 < T1;
	}
	void BoxLineCheck( INT iParent, INT iNode, UBOOL IsFront, UBOOL Outside )
	{
		while( iNode != INDEX_NONE )
		{
			// Compute distance between start and end points and this node's plane.
			const FBspNode& Node       = Model.Nodes(iNode);
			FPlane          Plane; 
			if (Owner) 
				Plane = Node.Plane.TransformByUsingAdjointT(Matrix, DetM, MatrixTA); 
			else 
				Plane = Node.Plane;



			FLOAT           D0         = Plane.PlaneDot(Start);
			FLOAT           D1         = Plane.PlaneDot(End);
			FLOAT           PushOut    = FBoxPushOut( Plane, Extent *1.1f );
			UBOOL           Use[2]     = {D0<=PushOut || D1<=PushOut, D0>=-PushOut || D1>=-PushOut};
			UBOOL           FrontFirst = D0 >= D1;

			// Traverse down nearest side then furthest side.
			if( Use[FrontFirst] )
				BoxLineCheck( iNode, Node.iChild[FrontFirst], FrontFirst, Node.ChildOutside(FrontFirst, Outside) );
			if( !Use[1-FrontFirst] )
				return;

			iParent = iNode;
			iNode   = Node.iChild[ 1-FrontFirst ];
			Outside = Node.ChildOutside( 1-FrontFirst, Outside );
			IsFront = !FrontFirst;
		}
		const FBspNode& Parent = Model.Nodes(iParent);
		if( Outside==0 && Parent.iCollisionBound!=INDEX_NONE )
		{
			// Init.
			SetupHulls(Parent);
			T0       = -1.f; 
			T1       = Hit.Time;
			LocalHit = FVector(0,0,0);

			// Perform collision clipping.
			CLIP_COLLISION_PRIMITIVE;

			// See if we hit.
			if( T0>-1.f && T0<T1 && T1>0.f )
			{
				Hit.Time	  = T0;
				Hit.Normal	  = LocalHit;
				Hit.Actor     = Owner;
				DidHit        = 1;
			}
			NoBlock:;
		}
	}
};

//
// ClipNode - return the node containing a specified location from a number of coplanar nodes.
//
INT ClipNode( UModel& Model, INT iNode, FVector HitLocation )
{
	while( iNode != INDEX_NONE )
	{	
		FBspNode& Node = Model.Nodes(iNode);
		INT NumVertices = Node.NumVertices;

		// Only consider this node if it has some vertices.
		if(NumVertices > 0)
		{
			INT iVertPool = Node.iVertPool;

			FVector PrevPt = Model.Points(Model.Verts(iVertPool+NumVertices-1).pVertex);
			FVector Normal = Model.Surfs(Node.iSurf).Plane;
			FLOAT PrevDot = 0.f;

			for( INT i=0;i<NumVertices;i++ )
			{
				FVector Pt = Model.Points(Model.Verts(iVertPool+i).pVertex);
				FLOAT Dot = FPlane( Pt, Normal^(Pt-PrevPt) ).PlaneDot(HitLocation);
				// Check for sign change
				if( (Dot < 0.f && PrevDot > 0.f) ||	(Dot > 0.f && PrevDot < 0.f) )
					goto TryNextNode;
				PrevPt = Pt;
				PrevDot = Dot;
			}

			return iNode;
		}

TryNextNode:;
		iNode = Node.iPlane;
	}
	return INDEX_NONE;
}


//
// Try moving a collision box from Start to End and see what it collides
// with. Returns 1 if unblocked, 0 if blocked.
//
// Note: This function assumes that Start is a valid starting point which
// is not stuck in solid space (i.e. CollisionPointCheck(Start)=1). If Start is
// stuck in solid space, the returned time will be meaningless.
//
UBOOL UModel::LineCheck
(
	FCheckResult&	Hit,
	AActor*			Owner,
	const FMatrix*	OwnerLocalToWorld,
	FVector			End,
	FVector			Start,
	FVector			Extent,
	DWORD			TraceFlags
)
{
#if STATS
	DWORD Counter = Extent == FVector(0,0,0) ? STAT_BSPZeroExtentTime : STAT_BSPExtentTime;
	SCOPE_CYCLE_COUNTER(Counter);
#endif

	DWORD ExtraNodeFlags = 0;
	if(TraceFlags & TRACE_Visible)
		ExtraNodeFlags = NF_NotVisBlocking;

	// Perform the trace.
	if( Nodes.Num() )
	{
		if( Extent == FVector(0,0,0) )
		{
			// Perform simple line trace.
			GOutOfCorner = 0;
			UBOOL Outside = 0;
            FMatrix M; 
			if( Owner )
			{
				// Check for a Owner + offset matrix, otherwise use the owner's matrix
				M = OwnerLocalToWorld ? *OwnerLocalToWorld : Owner->LocalToWorld();
				Outside = ::LineCheck( Hit, *this, &M, 0, 0, End, Start, RootOutside, ExtraNodeFlags );
			}
			else
			{
				Outside = ::LineCheck( Hit, *this, NULL, 0, 0, End, Start, RootOutside, ExtraNodeFlags );
			}
			if( !Outside )
			{
				FVector V       = End-Start;
				Hit.Time        = ((Hit.Location-Start)|V)/(V|V);
				Hit.Time		= Clamp( Hit.Time - 0.5f / V.Size(), 0.f, 1.f );
				Hit.Location	= Start + V * Hit.Time;
				Hit.Actor		= Owner;
				if( TraceFlags & TRACE_Material )
				{
					Hit.Item = ::ClipNode( *this, Hit.Item, Hit.Location );
					if( Hit.Item == INDEX_NONE )
						Hit.Material = NULL;
					else
						Hit.Material = Surfs(Nodes(Hit.Item).iSurf).Material;
				}
				if ( Owner )
                {
					Hit.Normal = M.TransposeAdjoint().TransformNormal(Hit.Normal);

					if( Owner->DrawScale != 1.0f || Owner->DrawScale3D != FVector(1,1,1) )
					{
						Hit.Normal.Normalize();
					}
                }

				// see if hit normal is "impossible" (i.e. >90 degrees)
				if( (-V | Hit.Normal) < 0.0f )
				{
					// flip it
					Hit.Normal = -Hit.Normal;
				}
			}
			return Outside;	
		}
		else
		{
			// Perform expensive box convolution trace.
			Hit.Time = 2.f;
			FBoxLineCheckInfo Trace( Hit, *this, Owner, OwnerLocalToWorld, End, Start, Extent, ExtraNodeFlags );
			Trace.BoxLineCheck( 0, 0, 0, RootOutside );

			if( Trace.DidHit )
			{
				if (TraceFlags & TRACE_Accurate)
				{
					Hit.Time = Clamp(Hit.Time,0.0f,1.0f);
				}
				else
				{
					// Truncate by 10% clamped between 0.1 and 1 world units.
					Hit.Time = Clamp( Hit.Time - Clamp(0.1f,0.1f/Trace.Dist, 1.f/Trace.Dist),0.f, 1.f );
				}
				Hit.Location  = Start + (End-Start) * Hit.Time;
				return Hit.Time==1.f;
			}
			else 
				return 1;
		}
	}
	else 
		return RootOutside;
}

/*---------------------------------------------------------------------------------------
   Point searching.
---------------------------------------------------------------------------------------*/

//
// Find closest vertex to a point at or below a node in the Bsp.  If no vertices
// are closer than MinRadius, returns -1.
//
static FLOAT FindNearestVertex
(
	const UModel	&Model, 
	const FVector	&SourcePoint,
	FVector			&DestPoint, 
	FLOAT			MinRadius, 
	INT				iNode, 
	INT				&pVertex
)
{
	FLOAT ResultRadius = -1.f;
	while( iNode != INDEX_NONE )
	{
		const FBspNode	*Node	= &Model.Nodes(iNode);
		const INT	    iBack   = Node->iBack;
		const FLOAT PlaneDist = Node->Plane.PlaneDot( SourcePoint );
		if( PlaneDist>=-MinRadius && Node->iFront!=INDEX_NONE )
		{
			// Check front.
			const FLOAT TempRadius = FindNearestVertex (Model,SourcePoint,DestPoint,MinRadius,Node->iFront,pVertex);
			if (TempRadius >= 0.f) {ResultRadius = TempRadius; MinRadius = TempRadius;};
		}
		if( PlaneDist>-MinRadius && PlaneDist<=MinRadius )
		{
			// Check this node's poly's vertices.
			while( iNode != INDEX_NONE )
			{
				// Loop through all coplanars.
				Node                    = &Model.Nodes	(iNode);
				const FBspSurf* Surf    = &Model.Surfs	(Node->iSurf);
				const FVector *Base	    = &Model.Points	(Surf->pBase);
				const FLOAT TempRadiusSquared	= FDistSquared( SourcePoint, *Base );

				if( TempRadiusSquared < Square(MinRadius) )
				{
					pVertex = Surf->pBase;
					ResultRadius = MinRadius = appSqrt(TempRadiusSquared);
					DestPoint = *Base;
				}

				const FVert* VertPool = &Model.Verts(Node->iVertPool);
				for (BYTE B=0; B<Node->NumVertices; B++)
				{
					const FVector *Vertex   = &Model.Points(VertPool->pVertex);
					const FLOAT TempRadiusSquared2 = FDistSquared( SourcePoint, *Vertex );
					if( TempRadiusSquared2 < Square(MinRadius) )
					{
						pVertex      = VertPool->pVertex;
						ResultRadius = MinRadius = appSqrt(TempRadiusSquared2);
						DestPoint    = *Vertex;
					}
					VertPool++;
				}
				iNode = Node->iPlane;
			}
		}
		if( PlaneDist > MinRadius )
			break;
		iNode = iBack;
	}
	return ResultRadius;
}

//
// Find Bsp node vertex nearest to a point (within a certain radius) and
// set the location.  Returns distance, or -1.f if no point was found.
//
FLOAT UModel::FindNearestVertex
(
	const FVector	&SourcePoint,
	FVector			&DestPoint,
	FLOAT			MinRadius, 
	INT				&pVertex
) const
{
	return Nodes.Num() ? ::FindNearestVertex( *this,SourcePoint,DestPoint,MinRadius,0,pVertex ) : -1.f;
}

/*---------------------------------------------------------------------------------------
   Bound filter precompute.
---------------------------------------------------------------------------------------*/

//
// Recursive worker function for UModel::PrecomputeSphereFilter.
//
static void PrecomputeSphereFilter( UModel& Model, INT iNode, const FPlane& Sphere )
{
	do
	{
		FBspNode* Node   = &Model.Nodes( iNode );
		Node->NodeFlags &= ~(NF_IsFront | NF_IsBack);
		FLOAT Dist       = Node->Plane.PlaneDot( Sphere );
		if( Dist < -Sphere.W )
		{
			Node->NodeFlags |= NF_IsBack;
			iNode = Node->iBack;
		}
		else
		{	
			if( Dist > Sphere.W )
				Node->NodeFlags |= NF_IsFront;
			else if( Node->iBack != INDEX_NONE )
				PrecomputeSphereFilter( Model, Node->iBack, Sphere );
			iNode = Node->iFront;
		}
	}
	while( iNode != INDEX_NONE );
}

//
// Precompute the front/back test for a bounding sphere.  Tags all nodes that
// the sphere falls into with a NF_IsBack tag (if the sphere is entirely in back
// of the node), a NF_IsFront tag (if the sphere is entirely in front of the node),
// or neither (if the sphere is split by the node).  This only affects nodes
// that the sphere falls in.  Thus, it is not necessary to perform any cleanup
// after precomputing the filter as long as you're sure the sphere completely
// encloses the object whose filter you're precomputing.
//
void UModel::PrecomputeSphereFilter( const FPlane& Sphere )
{
	if( Nodes.Num() )
		::PrecomputeSphereFilter( *this, 0, Sphere );

}

/**
 * Update passed in array with list of nodes intersecting box.
 *
 * @warning: this is an approximation and may result in false positives
 *
 * @param			Box						Box to filter down the BSP
 * @param	[out]	OutNodeIndices			Node indices that fall into box
 * @param	[out]	OutComponentIndices		Component indices associated with nodes
 */
void UModel::GetBoxIntersectingNodesAndComponents( const FBox& Box, TArray<INT>& OutNodeIndices, TArray<INT>& OutComponentIndices  ) const
{
	if ( Nodes.Num() == 0 )
	{
		return;
	}

	INT*	iNodes	= new INT[Nodes.Num()]; //@todo: this could be a static buffer, e.g. TArray
	INT		Index	= 0;
	iNodes[0]		= 0;

	const FVector Origin = Box.GetCenter();
	const FVector Extent = Box.GetExtent();

	// Avoid recursion by operating on a stack, iterating while there is work left to do.
	while( Index >= 0 )
	{
		// Pick node from stack.
		INT iNode		= iNodes[Index--];

		// Check distance...
		const FBspNode& Node= Nodes(iNode);
		FLOAT PushOut		= FBoxPushOut(Node.Plane,Extent);
		FLOAT Distance		= Node.Plane.PlaneDot(Origin);
		// ... and keep track whether it's in front or behind.
		UBOOL bIsFront		= FALSE;
		UBOOL bIsBack		= FALSE;

		// Check whether node is behind.
		if( Distance < +PushOut )
		{
			if( Node.iBack != INDEX_NONE )
			{
				iNodes[++Index] = Node.iBack;
			}
			bIsBack = TRUE;
		}

		// Check whether node is in front.
		if( Distance > -PushOut )
		{
			if( Node.iFront != INDEX_NONE )
			{
				iNodes[++Index] = Node.iFront;
			}
			bIsFront = TRUE;
		}

		// If it's both in front and behind it means it's intersecting.
		if( bIsFront && bIsBack )
		{
			// Handle coplanar nodes.
			if( Node.iPlane != INDEX_NONE )
			{
				iNodes[++Index] = Node.iPlane;
			}

			// Check poly against box, if intersect or contains, add node.
			if( IsNodeBBIntersectingBox( Node, Box ) )
			{
				OutNodeIndices.AddItem( iNode );
				OutComponentIndices.AddUniqueItem( Node.ComponentIndex );
			}
		}
	}

	delete [] iNodes;
}

/**
 * Returns whether bounding box of polygon associated with passed in node intersects passed in box.
 *
 * @param	Node	Node to check
 * @param	Box		Box to check node against
 * @return	TRUE if node polygon intersects box or is entirely contained, FALSE otherwise
 */
UBOOL UModel::IsNodeBBIntersectingBox( const FBspNode& Node, const FBox& Box ) const
{
	// Create node bounding box.
	FBox NodeBB;
	GetNodeBoundingBox( Node, NodeBB );

	// Return whether node BB intersects vertex BB.
	return Box.Intersect( NodeBB );
}

/**
 * Creates a bounding box for the passed in node
 *
 * @param	Node	Node to create a bounding box for
 * @param	OutBox	The created box
 */
void UModel::GetNodeBoundingBox( const FBspNode& Node, FBox& OutBox ) const
{
	OutBox.Init();
	const INT FirstVertexIndex = Node.iVertPool;
	for( INT VertexIndex=0; VertexIndex<Node.NumVertices; VertexIndex++ )
	{
		const FVert&	ModelVert	= Verts( FirstVertexIndex + VertexIndex );
		const FVector	Vertex		= Points( ModelVert.pVertex );
		OutBox += Vertex;
	}
}

