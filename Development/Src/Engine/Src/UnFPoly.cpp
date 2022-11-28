/*=============================================================================
	UnFPoly.cpp: FPoly implementation (Editor polygons).
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"

/*---------------------------------------------------------------------------------------
	FPoly class implementation.
---------------------------------------------------------------------------------------*/

//
// Initialize everything in an  editor polygon structure to defaults.
//
void FPoly::Init()
{
	Base			= FVector(0,0,0);
	Normal			= FVector(0,0,0);
	TextureU		= FVector(0,0,0);
	TextureV		= FVector(0,0,0);
	PolyFlags       = 0;
	Actor			= NULL;
	Material        = NULL;
	ItemName        = NAME_None;
	NumVertices     = 0;
	iLink           = INDEX_NONE;
	iBrushPoly		= INDEX_NONE;
	SmoothingMask	= 0;
	LightMapScale	= 8.0f;
}

//
// Reverse an FPoly by reversing the normal and reversing the order of its
// vertices.
//
void FPoly::Reverse()
{
	FVector Temp;
	int i,c;

	Normal *= -1;

	c=NumVertices/2;
	for( i=0; i<c; i++ )
	{
		// Flip all points except middle if odd number of points.
		Temp      = Vertex[i];
		Vertex[i] = Vertex[(NumVertices-1)-i];
		Vertex[(NumVertices-1)-i] = Temp;
	}
}

//
// Fix up an editor poly by deleting vertices that are identical.  Sets
// vertex count to zero if it collapses.  Returns number of vertices, 0 or >=3.
//
int FPoly::Fix()
{
	int i,j,prev;

	j=0; prev=NumVertices-1;
	for( i=0; i<NumVertices; i++ )
	{
		if( !FPointsAreSame( Vertex[i], Vertex[prev] ) )
		{
			if( j != i )
				Vertex[j] = Vertex[i];
			prev = j;
			j    ++;
		}
		else debugf( NAME_Warning, TEXT("FPoly::Fix: Collapsed a point") );
	}
	if (j>=3) NumVertices = j;
	else      NumVertices = 0;
	return NumVertices;
}

//
// Compute the 2D area.
//
FLOAT FPoly::Area()
{
	FVector Side1,Side2;
	FLOAT Area;
	int i;

	Area  = 0.f;
	Side1 = Vertex[1] - Vertex[0];
	for( i=2; i<NumVertices; i++ )
	{
		Side2 = Vertex[i] - Vertex[0];
		Area += (Side1 ^ Side2).Size();
		Side1 = Side2;
	}
	return Area;
}

//
// Split with plane. Meant to be numerically stable.
//
int FPoly::SplitWithPlane
(
	const FVector	&PlaneBase,
	const FVector	&PlaneNormal,
	FPoly			*FrontPoly,
	FPoly			*BackPoly,
	int				VeryPrecise
) const
{
	FVector 	Intersection;
	FLOAT   	Dist=0,MaxDist=0,MinDist=0;
	FLOAT		PrevDist,Thresh;
	enum 	  	{V_FRONT,V_BACK,V_EITHER} Status,PrevStatus=V_EITHER;
	int     	i,j;

	if (VeryPrecise)	Thresh = THRESH_SPLIT_POLY_PRECISELY;	
	else				Thresh = THRESH_SPLIT_POLY_WITH_PLANE;

	// Find number of vertices.
	check(NumVertices>=3);
	check(NumVertices<=MAX_VERTICES);

	// See if the polygon is split by SplitPoly, or it's on either side, or the
	// polys are coplanar.  Go through all of the polygon points and
	// calculate the minimum and maximum signed distance (in the direction
	// of the normal) from each point to the plane of SplitPoly.
	for( i=0; i<NumVertices; i++ )
	{
		Dist = FPointPlaneDist( Vertex[i], PlaneBase, PlaneNormal );

		if( i==0 || Dist>MaxDist ) MaxDist=Dist;
		if( i==0 || Dist<MinDist ) MinDist=Dist;

		if      (Dist > +Thresh) PrevStatus = V_FRONT;
		else if (Dist < -Thresh) PrevStatus = V_BACK;
	}
	if( MaxDist<Thresh && MinDist>-Thresh )
	{
		return SP_Coplanar;
	}
	else if( MaxDist < Thresh )
	{
		return SP_Back;
	}
	else if( MinDist > -Thresh )
	{
		return SP_Front;
	}
	else
	{
		// Split.
		if( FrontPoly==NULL )
			return SP_Split; // Caller only wanted status.
		if( NumVertices > MAX_VERTICES )
			appErrorf( TEXT("%s"), TEXT("FPoly::SplitWithPlane: Vertex overflow") );

		*FrontPoly = *this; // Copy all info.
		FrontPoly->PolyFlags |= PF_EdCut; // Mark as cut.
		FrontPoly->NumVertices =  0;

		*BackPoly = *this; // Copy all info.
		BackPoly->PolyFlags |= PF_EdCut; // Mark as cut.
		BackPoly->NumVertices = 0;

		j = NumVertices-1; // Previous vertex; have PrevStatus already.

		for( i=0; i<NumVertices; i++ )
		{
			PrevDist	= Dist;
      		Dist		= FPointPlaneDist( Vertex[i], PlaneBase, PlaneNormal );

			if      (Dist > +Thresh)  	Status = V_FRONT;
			else if (Dist < -Thresh)  	Status = V_BACK;
			else						Status = PrevStatus;

			if( Status != PrevStatus )
	        {
				// Crossing.  Either Front-to-Back or Back-To-Front.
				// Intersection point is naturally on both front and back polys.
				if( (Dist >= -Thresh) && (Dist < +Thresh) )
				{
					// This point lies on plane.
					if( PrevStatus == V_FRONT )
					{
						FrontPoly->Vertex[FrontPoly->NumVertices++] = Vertex[i];
						BackPoly ->Vertex[BackPoly ->NumVertices++] = Vertex[i];
					}
					else
					{
						BackPoly ->Vertex[BackPoly ->NumVertices++] = Vertex[i];
						FrontPoly->Vertex[FrontPoly->NumVertices++] = Vertex[i];
					}
				}
				else if( (PrevDist >= -Thresh) && (PrevDist < +Thresh) )
				{
					// Previous point lies on plane.
					if (Status == V_FRONT)
					{
						FrontPoly->Vertex[FrontPoly->NumVertices++] = Vertex[j];
						FrontPoly->Vertex[FrontPoly->NumVertices++] = Vertex[i];
					}
					else
					{
						BackPoly ->Vertex[BackPoly ->NumVertices++] = Vertex[j];
						BackPoly ->Vertex[BackPoly ->NumVertices++] = Vertex[i];
					}
				}
				else
				{
					// Intersection point is in between.
					Intersection = FLinePlaneIntersection(Vertex[j],Vertex[i],PlaneBase,PlaneNormal);

					if( PrevStatus == V_FRONT )
					{
						FrontPoly->Vertex[FrontPoly->NumVertices++] = Intersection;
						BackPoly ->Vertex[BackPoly ->NumVertices++] = Intersection;
						BackPoly ->Vertex[BackPoly ->NumVertices++]	= Vertex[i];
					}
					else
					{
						BackPoly ->Vertex[BackPoly ->NumVertices++] = Intersection;
						FrontPoly->Vertex[FrontPoly->NumVertices++] = Intersection;
						FrontPoly->Vertex[FrontPoly->NumVertices++]	= Vertex[i];
					}
				}
			}
			else
			{
        		if (Status==V_FRONT) FrontPoly->Vertex[FrontPoly->NumVertices++] = Vertex[i];
        		else                 BackPoly ->Vertex[BackPoly ->NumVertices++] = Vertex[i];
			}
			j          = i;
			PrevStatus = Status;
		}

		// Handle possibility of sliver polys due to precision errors.
		if( FrontPoly->Fix()<3 )
		{
			debugf( NAME_Warning, TEXT("FPoly::SplitWithPlane: Ignored front sliver") );
			return SP_Back;
		}
		else if( BackPoly->Fix()<3 )
	    {
			debugf( NAME_Warning, TEXT("FPoly::SplitWithPlane: Ignored back sliver") );
			return SP_Front;
		}
		else return SP_Split;
	}
}

//
// Split with a Bsp node.
//
int FPoly::SplitWithNode
(
	const UModel	*Model,
	INT				iNode,
	FPoly			*FrontPoly,
	FPoly			*BackPoly,
	INT				VeryPrecise
) const
{
	const FBspNode &Node = Model->Nodes(iNode       );
	const FBspSurf &Surf = Model->Surfs(Node.iSurf  );

	return SplitWithPlane
	(
		Model->Points (Model->Verts(Node.iVertPool).pVertex),
		Model->Vectors(Surf.vNormal),
		FrontPoly, 
		BackPoly, 
		VeryPrecise
	);
}

//
// Split with plane quickly for in-game geometry operations.
// Results are always valid. May return sliver polys.
//
int FPoly::SplitWithPlaneFast
(
	const FPlane	Plane,
	FPoly*			FrontPoly,
	FPoly*			BackPoly
) const
{
	enum {V_FRONT=0,V_BACK=1} Status,PrevStatus,VertStatus[MAX_VERTICES],*StatusPtr;
	int Front=0,Back=0;

	StatusPtr = &VertStatus[0];
	for( int i=0; i<NumVertices; i++ )
	{
		FLOAT Dist = Plane.PlaneDot(Vertex[i]);
		if( Dist >= 0.f )
		{
			*StatusPtr++ = V_FRONT;
			if( Dist > +THRESH_SPLIT_POLY_WITH_PLANE )
				Front=1;
		}
		else
		{
			*StatusPtr++ = V_BACK;
			if( Dist < -THRESH_SPLIT_POLY_WITH_PLANE )
				Back=1;
		}
	}
	if( !Front )
	{
		if( Back ) return SP_Back;
		else       return SP_Coplanar;
	}
	if( !Back )
	{
		return SP_Front;
	}
	else
	{
		// Split.
		if( FrontPoly )
		{
			const FVector *V  = &Vertex            [0];
			const FVector *W  = &Vertex            [NumVertices-1];
			FVector *V1       = &FrontPoly->Vertex [0];
			FVector *V2       = &BackPoly ->Vertex [0];
			PrevStatus        = VertStatus         [NumVertices-1];
			StatusPtr         = &VertStatus        [0];

			int N1=0, N2=0;
			for( int i=0; i<NumVertices; i++ )
			{
				Status = *StatusPtr++;
				if( Status != PrevStatus )
				{
					// Crossing.
					*V1++ = *V2++ = FLinePlaneIntersection( *W, *V, Plane );
					if( PrevStatus == V_FRONT )	{*V2++ = *V; N1++; N2+=2;}
					else {*V1++ = *V; N2++; N1+=2;};
				}
				else if( Status==V_FRONT ) {*V1++ = *V; N1++;}
				else {*V2++ = *V; N2++;};

				PrevStatus = Status;
				W          = V++;
			}
			FrontPoly->NumVertices	= N1;
			FrontPoly->Base			= Base;
			FrontPoly->Normal		= Normal;
			FrontPoly->PolyFlags	= PolyFlags;

			BackPoly->NumVertices	= N2;
			BackPoly->Base			= Base;
			BackPoly->Normal		= Normal;
			BackPoly->PolyFlags		= PolyFlags;
		}
		return SP_Split;
	}
}

//
// Split an FPoly in half.
//
void FPoly::SplitInHalf( FPoly *OtherHalf )
{
	int m = NumVertices/2;
	int i;

	if( (NumVertices<=3) || (NumVertices>MAX_VERTICES) )
		appErrorf( TEXT("FPoly::SplitInHalf: %i Vertices"), NumVertices );

	*OtherHalf = *this;

	OtherHalf->NumVertices = (NumVertices-m) + 1;
	NumVertices            = (m            ) + 1;

	for( i=0; i<(OtherHalf->NumVertices-1); i++ )
	{
		OtherHalf->Vertex[i] = Vertex[i+m];
	}
	OtherHalf->Vertex[OtherHalf->NumVertices-1] = Vertex[0];

	PolyFlags            |= PF_EdCut;
	OtherHalf->PolyFlags |= PF_EdCut;
	
}

//
// Compute normal of an FPoly.  Works even if FPoly has 180-degree-angled sides (which
// are often created during T-joint elimination).  Returns nonzero result (plus sets
// normal vector to zero) if a problem occurs.
//
int FPoly::CalcNormal( UBOOL bSilent )
{
	Normal = FVector(0,0,0);
	for( int i=2; i<NumVertices; i++ )
		Normal += (Vertex[i-1] - Vertex[0]) ^ (Vertex[i] - Vertex[0]);

	if( Normal.SizeSquared() < (FLOAT)THRESH_ZERO_NORM_SQUARED )
	{
		if( !bSilent )
			debugf( NAME_Warning, TEXT("FPoly::CalcNormal: Zero-area polygon") );
		return 1;
	}
	Normal.Normalize();
	return 0;
}

//
// Transform an editor polygon with a coordinate system, a pre-transformation
// addition, and a post-transformation addition:
//
void FPoly::Transform
(
	const FVector&		PreSubtract,
	const FVector&		PostAdd
)
{
	FVector 	Temp;
	int 		i;

	TextureU = TextureU;
	TextureV = TextureV;

	Base = (Base - PreSubtract) + PostAdd;
	for( i=0; i<NumVertices; i++ )
		Vertex[i]  = (Vertex[i] - PreSubtract) + PostAdd;

	// Transform normal.  Since the transformation coordinate system is
	// orthogonal but not orthonormal, it has to be renormalized here.
	Normal = Normal.SafeNormal();

}

//
// Remove colinear vertices and check convexity.  Returns 1 if convex, 0 if
// nonconvex or collapsed.
//
INT FPoly::RemoveColinears()
{
	FVector  SidePlaneNormal[MAX_VERTICES];
	FVector  Side;
	INT      i,j;

	for( i=0; i<NumVertices; i++ )
	{
		j=i-1; if (j<0) j=NumVertices-1;

		// Create cutting plane perpendicular to both this side and the polygon's normal.
		Side = Vertex[i] - Vertex[j];
		SidePlaneNormal[i] = Side ^ Normal;

		if( !SidePlaneNormal[i].Normalize() )
		{
			// Eliminate these nearly identical points.
			appMemcpy( &Vertex[i], &Vertex[i+1], (NumVertices-(i+1)) * sizeof (FVector) );
			if (--NumVertices<3) {NumVertices = 0; return 0;}; // Collapsed.
			i--;
		}
	}
	for( i=0; i<NumVertices; i++ )
	{
		j=i+1; if (j>=NumVertices) j=0;

		if( FPointsAreNear(SidePlaneNormal[i],SidePlaneNormal[j],FLOAT_NORMAL_THRESH) )
	    {
			// Eliminate colinear points.
			appMemcpy (&Vertex[i],&Vertex[i+1],(NumVertices-(i+1)) * sizeof (FVector));
			appMemcpy (&SidePlaneNormal[i],&SidePlaneNormal[i+1],(NumVertices-(i+1)) * sizeof (FVector));
			if (--NumVertices<3) {NumVertices = 0; return 0;}; // Collapsed.
			i--;
		}
		else
		{
			for( j=0; j<NumVertices; j++ )
	        {
				if (j != i)
				{
					switch( SplitWithPlane (Vertex[i],SidePlaneNormal[i],NULL,NULL,0) )
					{
						case SP_Front: return 0; // Nonconvex + Numerical precision error
						case SP_Split: return 0; // Nonconvex
						// SP_BACK: Means it's convex
						// SP_COPLANAR: Means it's probably convex (numerical precision)
					}
				}
			}
		}
	}
	return 1; // Ok.
}

// Checks to see if the specified line intersects this poly or not.  If "Intersect" is
// a valid pointer, it is filled in with the intersection point.
UBOOL FPoly::DoesLineIntersect( FVector Start, FVector End, FVector* Intersect )
{
	// If the ray doesn't cross the plane, don't bother going any further.
	float DistStart, DistEnd;
	DistStart = FPointPlaneDist( Start, Vertex[0], Normal );
	DistEnd = FPointPlaneDist( End, Vertex[0], Normal );
	if( (DistStart < 0 && DistEnd < 0) || (DistStart > 0 && DistEnd > 0 ) )
	{
		return 0;
	}

	// Get the intersection of the line and the plane.
	FVector Intersection = FLinePlaneIntersection(Start,End,Vertex[0],Normal);
	if( Intersect )	*Intersect = Intersection;
	if( Intersection == Start || Intersection == End )
	{
		return 0;
	}

	// Check if the intersection point is actually on the poly.
	return OnPoly( Intersection );
}
//
// Checks to see if the specified vertex is on this poly.  Assumes the vertex is on the same
// plane as the poly and that the poly is convex.
//
// This can be combined with FLinePlaneIntersection to perform a line-fpoly intersection test.
//
UBOOL FPoly::OnPoly( FVector InVtx )
{
	FVector  SidePlaneNormal;
	FVector  Side;

	for( INT x = 0 ; x < NumVertices ; x++ )
	{
		// Create plane perpendicular to both this side and the polygon's normal.
		Side = Vertex[x] - Vertex[(x-1 < 0 ) ? NumVertices-1 : x-1 ];
		SidePlaneNormal = Side ^ Normal;
		SidePlaneNormal.Normalize();

		// If point is not behind all the planes created by this polys edges, it's outside the poly.
		if( FPointPlaneDist( InVtx, Vertex[x], SidePlaneNormal ) > THRESH_POINT_ON_PLANE )
			return 0;
	}

	return 1;
}

// Inserts a vertex into the poly at a specific position.
//
void FPoly::InsertVertex( INT InPos, FVector InVtx )
{
	check( InPos <= NumVertices );

	TArray<FVector> NewVerts;

	// Copy the existing vertices to a temp array
	for( INT x = 0 ; x < NumVertices ; x++ )
		new( NewVerts )FVector( Vertex[x] );

	// Insert the new vertex
	NewVerts.Insert( InPos );
	NewVerts( InPos ) = InVtx;

	// Copy the temp array into the real vertex list and clean up
	for( INT x = 0 ; x < NewVerts.Num() ; x++ )
		Vertex[x] = NewVerts(x);

	NumVertices++;

}

// Removes a vertex from the polygons list of vertices
//
void FPoly::RemoveVertex( FVector InVtx )
{
	// Copy all vertices that are not equal to InVtx into a temp array.

	TArray<FVector> NewVerts;

	for( INT x = 0 ; x < NumVertices ; ++x )
	{
		if( Vertex[x] != InVtx )
		{
			NewVerts.AddItem( Vertex[x] );
		}
	}

	// Overwrite the vertex list information with the contents
	// of the temp array.

	NumVertices = NewVerts.Num();

	for( INT x = 0 ; x < NewVerts.Num() ; ++x )
	{
		Vertex[x] = NewVerts(x);
	}
}

/**
 * Checks to see if all the vertices on a polygon are coplanar.
 */

UBOOL FPoly::IsCoplanar()
{
	// 3 or fewer vertices is automatically coplanar

	if( NumVertices < 3 )
	{
		return 1;
	}

	CalcNormal(1);

	for( INT x = 0 ; x < NumVertices ; ++x )
	{
		if( !OnPlane( Vertex[x] ) )
		{
			return 0;
		}
	}

	// If we got this far, the poly has to be coplanar.

	return 1;
}

/**
 * Breaks up this polygon into seperate triangles.
 *
 * NOTE: Assumes that the polygon is convex and breaks it up
 * into a triangle strip configuration (in layout only - not polygon winding).
 *
 * NOTE: It is up to the caller to make sure this original
 * polygon is removed from the brush afterwards!
 *
 * @param	InOwner		The ABrush we want to add the new triangles into
 *
 * @return	The number of triangles created
 */

INT FPoly::Triangulate( ABrush* InOwner )
{
	if( NumVertices < 3 )
	{
		return 0;
	}

	FPoly* NewTriangle;
	INT Count = 0;
	UBOOL bFlip = 0;
	INT Mid = NumVertices/2;
	FVector v0, v1, v2;
	INT Front,Back;

	v0 = Vertex[0];
	v1 = Vertex[1];
	Front = 2;
	Back = NumVertices-1;
	for( INT x = 2 ; x < NumVertices ; ++x )
	{
		if( bFlip )
		{
			v2 = Vertex[ Back ];
		}
		else
		{
			v2 = Vertex[ Front ];
		}

		NewTriangle = new( InOwner->Brush->Polys->Element )FPoly();

		NewTriangle->Init();
		NewTriangle->Vertex[0] = v0;
		NewTriangle->Vertex[1] = v1;
		NewTriangle->Vertex[2] = v2;
		NewTriangle->NumVertices = 3;

		NewTriangle->CalcNormal();
		NewTriangle->TextureU = TextureU;
		NewTriangle->TextureV = TextureV;
		NewTriangle->Base = Base;
		NewTriangle->PolyFlags = PolyFlags;
		NewTriangle->Material = Material;
		NewTriangle->PolyFlags &= ~PF_GeomMarked;

		Count++;

		if( bFlip )
		{
			v0 = v2;
			Back--;
		}
		else
		{
			v1 = v2;
			Front++;
		}

		bFlip = !bFlip;
	}

	debugf( TEXT("FPoly::Triangulate : Created %d triangles"), Count );
	return Count;
}

/**
 * Finds the index of the specific vertex.
 *
 * @param	InVtx	The vertex to find the index of
 *
 * @return	The index of the vertex, if found.  Otherwise INDEX_NONE.
 */

INT FPoly::GetVertexIndex( FVector& InVtx )
{
	INT idx = INDEX_NONE;

	for( INT v = 0 ; v < NumVertices ; ++v )
	{
		if( Vertex[v] == InVtx )
		{
			idx = v;
			break;
		}
	}

	return idx;
}

/**
 * Computes the mid point of the polygon (in local space).
 */

FVector FPoly::GetMidPoint()
{
	FVector mid(0,0,0);

	for( INT v = 0 ; v < NumVertices ; ++v )
	{
		mid += Vertex[v];
	}

	return mid / NumVertices;
}

// Checks to see if the specified vertex lies on this polygons plane.
//
UBOOL FPoly::OnPlane( FVector InVtx )
{
	return ( ( FPointPlaneDist( InVtx, Vertex[0], Normal ) > -THRESH_POINT_ON_PLANE )
		&& ( FPointPlaneDist( InVtx, Vertex[0], Normal ) < THRESH_POINT_ON_PLANE ) );
}

//
// Split a poly and keep only the front half. Returns number of vertices,
// 0 if clipped away.
//
int FPoly::Split( const FVector &Normal, const FVector &Base, int NoOverflow )
{
	if( NoOverflow && NumVertices>=FPoly::VERTEX_THRESHOLD )
	{
		// Don't split it, just reject it.
		if( SplitWithPlaneFast( FPlane(Base,Normal), NULL, NULL )==SP_Back )
			return 0;
		else
			return NumVertices;
	}
	else
	{
		// Split it.
		FPoly Front, Back;
		Front.Init();
		Back.Init();
		switch( SplitWithPlaneFast( FPlane(Base,Normal), &Front, &Back ))
		{
			case SP_Back:
				return 0;
			case SP_Split:
				*this = Front;
				return NumVertices;
			default:
				return NumVertices;
		}
	}
}

//
// Compute all remaining polygon parameters (normal, etc) that are blank.
// Returns 0 if ok, nonzero if problem.
//
int FPoly::Finalize( ABrush* InOwner, int NoError )
{
	// Check for problems.
	Fix();
	if( NumVertices<3 )
	{
		// Since we don't have enough vertices, remove this polygon from the brush

		for( INT p = 0 ; p < InOwner->Brush->Polys->Element.Num() ; ++p )
		{
			if( InOwner->Brush->Polys->Element(p) == *this )
			{
				InOwner->Brush->Polys->Element.Remove(p);
				break;
			}
		}

		debugf( NAME_Warning, TEXT("FPoly::Finalize: Not enough vertices (%i)"), NumVertices );
		if( NoError )
			return -1;
		else
		{
			debugf( TEXT("FPoly::Finalize: Not enough vertices (%i) : polygon removed from brush"), NumVertices );
			return -2;
		}
	}

	// If no normal, compute from cross-product and normalize it.
	if( Normal.IsZero() && NumVertices>=3 )
	{
		if( CalcNormal() )
		{
			debugf( NAME_Warning, TEXT("FPoly::Finalize: Normalization failed, verts=%i, size=%f"), NumVertices, Normal.Size() );
			if( NoError )
				return -1;
			else
				appErrorf( TEXT("FPoly::Finalize: Normalization failed, verts=%i, size=%f"), NumVertices, Normal.Size() );
		}
	}

	// If texture U and V coordinates weren't specified, generate them.
	if( TextureU.IsZero() && TextureV.IsZero() )
	{
		for( int i=1; i<NumVertices; i++ )
		{
			TextureU = ((Vertex[0] - Vertex[i]) ^ Normal).SafeNormal();
			TextureV = (Normal ^ TextureU).SafeNormal();
			if( TextureU.SizeSquared()!=0 && TextureV.SizeSquared()!=0 )
				break;
		}
	}
	return 0;
}

/*---------------------------------------------------------------------------------------
	FPolys object implementation.
---------------------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UPolys);

/*---------------------------------------------------------------------------------------
	Backfacing.
---------------------------------------------------------------------------------------*/

//
// Return whether this poly and Test are facing each other.
// The polys are facing if they are noncoplanar, one or more of Test's points is in 
// front of this poly, and one or more of this poly's points are behind Test.
//
int FPoly::Faces( const FPoly &Test ) const
{
	// Coplanar implies not facing.
	if( IsCoplanar( Test ) )
		return 0;

	// If this poly is frontfaced relative to all of Test's points, they're not facing.
	for( int i=0; i<Test.NumVertices; i++ )
	{
		if( !IsBackfaced( Test.Vertex[i] ) )
		{
			// If Test is frontfaced relative to on or more of this poly's points, they're facing.
			for( i=0; i<NumVertices; i++ )
				if( Test.IsBackfaced( Vertex[i] ) )
					return 1;
			return 0;
		}
	}
	return 0;
}

/*---------------------------------------------------------------------------------------
	The End.
---------------------------------------------------------------------------------------*/

