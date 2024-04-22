/*=============================================================================
	UnFPoly.cpp: FPoly implementation (Editor polygons).
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#if WITH_EDITOR
#include "GeomTools.h"
#endif


#define PolyCheck( x )			check( x )

/*-----------------------------------------------------------------------------
	FLightmassPrimitiveSettings
-----------------------------------------------------------------------------*/
// Functions.
FArchive& operator<<(FArchive& Ar, FLightmassPrimitiveSettings& Settings)
{
	if (Ar.Ver() >= VER_SHADOW_INDIRECT_ONLY_OPTION)
	{
		UBOOL Temp = Settings.bUseTwoSidedLighting;
		Ar << Temp;
		Settings.bUseTwoSidedLighting = Temp;
		Temp = Settings.bShadowIndirectOnly;
		Ar << Temp;
		Settings.bShadowIndirectOnly = Temp;
		Ar << Settings.FullyOccludedSamplesFraction;
	}
	else
	{
		Settings.bUseTwoSidedLighting = FALSE;
		Settings.bShadowIndirectOnly = FALSE;
		Settings.FullyOccludedSamplesFraction = 1.0f;
	}

	if (Ar.Ver() >= VER_INTEGRATED_LIGHTMASS)
	{
		UBOOL Temp = Settings.bUseEmissiveForStaticLighting;
		Ar << Temp;
		Settings.bUseEmissiveForStaticLighting = Temp;
	}
	else
	{
		Settings.bUseEmissiveForStaticLighting = FALSE;
	}

	if (Ar.Ver() >= VER_INTEGRATED_LIGHTMASS)
	{
		Ar << Settings.EmissiveLightFalloffExponent;
	}
	else
	{
		Settings.bUseEmissiveForStaticLighting = FALSE;
	}

	if (Ar.Ver() >= VER_ADDDED_EXPLICIT_EMISSIVE_LIGHT_RADIUS)
	{
		Ar << Settings.EmissiveLightExplicitInfluenceRadius;
	}
	else
	{
		Settings.EmissiveLightExplicitInfluenceRadius = 0.0f;
	}
	
	Ar << Settings.EmissiveBoost;
	Ar << Settings.DiffuseBoost;
	Ar << Settings.SpecularBoost;

	return Ar;
}

/**
 * Constructor, initializing all member variables.
 */
FPoly::FPoly()
{
	Init();
}

/**
 * Initialize everything in an  editor polygon structure to defaults.
 * Changes to these default values should also be mirrored to UPolysExporterT3D::ExportText(...).
 */
void FPoly::Init()
{
	Base			= FVector(0,0,0);
	Normal			= FVector(0,0,0);
	TextureU		= FVector(0,0,0);
	TextureV		= FVector(0,0,0);
	Vertices.Empty();
	PolyFlags       = PF_DefaultFlags;
	Actor			= NULL;
	Material        = NULL;
	RulesetVariation= NAME_None;
	ItemName        = NAME_None;
	iLink           = INDEX_NONE;
	iBrushPoly		= INDEX_NONE;
	SmoothingMask	= 0;
	ShadowMapScale	= 32.0f;

	FLightingChannelContainer		ChannelInitializer;
	ChannelInitializer.Bitfield		= 0;
	ChannelInitializer.BSP			= TRUE;
	ChannelInitializer.bInitialized	= TRUE;
	LightingChannels				= ChannelInitializer.Bitfield;

	LightmassSettings = FLightmassPrimitiveSettings(EC_NativeConstructor);
}

/**
 * Reverse an FPoly by reversing the normal and reversing the order of its vertices.
 */
void FPoly::Reverse()
{
	FVector Temp;
	int i,c;

	Normal *= -1;

	c=Vertices.Num()/2;
	for( i=0; i<c; i++ )
	{
		// Flip all points except middle if odd number of points.
		Temp      = Vertices(i);

		Vertices(i) = Vertices((Vertices.Num()-1)-i);
		Vertices((Vertices.Num()-1)-i) = Temp;
	}
}

/**
 * Fix up an editor poly by deleting vertices that are identical.  Sets
 * vertex count to zero if it collapses.  Returns number of vertices, 0 or >=3.
 */
int FPoly::Fix()
{
	int i,j,prev;

	j=0; prev=Vertices.Num()-1;
	for( i=0; i<Vertices.Num(); i++ )
	{
		if( !FPointsAreSame( Vertices(i), Vertices(prev) ) )
		{
			if( j != i )
				Vertices(j) = Vertices(i);
			prev = j;
			j    ++;
		}
// 		else debugf( NAME_Warning, TEXT("FPoly::Fix: Collapsed a point") );
	}
	if(j < 3)
	{
		Vertices.Empty();
	}
	else if(j < Vertices.Num())
	{
		Vertices.Remove(j,Vertices.Num() - j);
	}
	return Vertices.Num();
}

/**
 * Computes the 2D area of the polygon.  Returns zero if the polygon has less than three verices.
 */
FLOAT FPoly::Area()
{
	// If there are less than 3 verts
	if(Vertices.Num() < 3)
	{
		return 0.f;
	}

	FVector Side1,Side2;
	FLOAT Area;
	int i;

	Area  = 0.f;
	Side1 = Vertices(1) - Vertices(0);
	for( i=2; i<Vertices.Num(); i++ )
	{
		Side2 = Vertices(i) - Vertices(0);
		Area += (Side1 ^ Side2).Size() * 0.5f;
		Side1 = Side2;
	}
	return Area;
}

/**
 * Split with plane. Meant to be numerically stable.
 */
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
	check(Vertices.Num()>=3);

	// See if the polygon is split by SplitPoly, or it's on either side, or the
	// polys are coplanar.  Go through all of the polygon points and
	// calculate the minimum and maximum signed distance (in the direction
	// of the normal) from each point to the plane of SplitPoly.
	for( i=0; i<Vertices.Num(); i++ )
	{
		Dist = FPointPlaneDist( Vertices(i), PlaneBase, PlaneNormal );

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

		*FrontPoly = *this; // Copy all info.
		FrontPoly->PolyFlags |= PF_EdCut; // Mark as cut.
		FrontPoly->Vertices.Empty();

		*BackPoly = *this; // Copy all info.
		BackPoly->PolyFlags |= PF_EdCut; // Mark as cut.
		BackPoly->Vertices.Empty();

		j = Vertices.Num()-1; // Previous vertex; have PrevStatus already.

		for( i=0; i<Vertices.Num(); i++ )
		{
			PrevDist	= Dist;
      		Dist		= FPointPlaneDist( Vertices(i), PlaneBase, PlaneNormal );

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
						new(FrontPoly->Vertices) FVector(Vertices(i));
						new(BackPoly->Vertices) FVector(Vertices(i));
					}
					else
					{
						new(BackPoly->Vertices) FVector(Vertices(i));
						new(FrontPoly->Vertices) FVector(Vertices(i));
					}
				}
				else if( (PrevDist >= -Thresh) && (PrevDist < +Thresh) )
				{
					// Previous point lies on plane.
					if (Status == V_FRONT)
					{
						new(FrontPoly->Vertices) FVector(Vertices(j));
						new(FrontPoly->Vertices) FVector(Vertices(i));
					}
					else
					{
						new(BackPoly->Vertices) FVector(Vertices(j));
						new(BackPoly->Vertices) FVector(Vertices(i));
					}
				}
				else
				{
					// Intersection point is in between.
					Intersection = FLinePlaneIntersection(Vertices(j),Vertices(i),PlaneBase,PlaneNormal);

					if( PrevStatus == V_FRONT )
					{
						new(FrontPoly->Vertices) FVector(Intersection);
						new(BackPoly->Vertices) FVector(Intersection);
						new(BackPoly->Vertices) FVector(Vertices(i));
					}
					else
					{
						new(BackPoly->Vertices) FVector(Intersection);
						new(FrontPoly->Vertices) FVector(Intersection);
						new(FrontPoly->Vertices) FVector(Vertices(i));
					}
				}
			}
			else
			{
        		if (Status==V_FRONT) new(FrontPoly->Vertices)FVector(Vertices(i));
        		else                 new(BackPoly->Vertices)FVector(Vertices(i));
			}
			j          = i;
			PrevStatus = Status;
		}

		// Handle possibility of sliver polys due to precision errors.
		if( FrontPoly->Fix()<3 )
		{
// 			debugf( NAME_Warning, TEXT("FPoly::SplitWithPlane: Ignored front sliver") );
			return SP_Back;
		}
		else if( BackPoly->Fix()<3 )
	    {
// 			debugf( NAME_Warning, TEXT("FPoly::SplitWithPlane: Ignored back sliver") );
			return SP_Front;
		}
		else return SP_Split;
	}
}

/**
 * Split with a Bsp node.
 */
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

/**
 * Split with plane quickly for in-game geometry operations.
 * Results are always valid. May return sliver polys.
 */
int FPoly::SplitWithPlaneFast
(
	const FPlane&	Plane,
	FPoly*			FrontPoly,
	FPoly*			BackPoly
) const
{
	FMemMark MemMark(GMainThreadMemStack);
	enum EPlaneClassification
	{
		V_FRONT=0,
		V_BACK=1
	};
	EPlaneClassification Status,PrevStatus;
	EPlaneClassification* VertStatus = new(GMainThreadMemStack) EPlaneClassification[Vertices.Num()];
	int Front=0,Back=0;

	EPlaneClassification* StatusPtr = &VertStatus[0];
	for( int i=0; i<Vertices.Num(); i++ )
	{
		FLOAT Dist = Plane.PlaneDot(Vertices(i));
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
	ESplitType Result;
	if( !Front )
	{
		if( Back ) Result = SP_Back;
		else       Result = SP_Coplanar;
	}
	else if( !Back )
	{
		Result = SP_Front;
	}
	else
	{
		// Split.
		if( FrontPoly )
		{
			const FVector *V  = &Vertices(0);
			const FVector *W  = &Vertices(Vertices.Num()-1);
			FVector *V1       = &FrontPoly->Vertices(0);
			FVector *V2       = &BackPoly ->Vertices(0);
			PrevStatus        = VertStatus         [Vertices.Num()-1];
			StatusPtr         = &VertStatus        [0];

			for( int i=0; i<Vertices.Num(); i++ )
			{
				Status = *StatusPtr++;
				if( Status != PrevStatus )
				{
					// Crossing.
					const FVector& Intersection = FLinePlaneIntersection( *W, *V, Plane );
					new(FrontPoly->Vertices) FVector(Intersection);
					new(BackPoly->Vertices) FVector(Intersection);
					if( PrevStatus == V_FRONT )
					{
						new(BackPoly->Vertices) FVector(*V);
					}
					else
					{
						new(FrontPoly->Vertices) FVector(*V);
					}
				}
				else if( Status==V_FRONT )
				{
					new(FrontPoly->Vertices) FVector(*V);
				}
				else
				{
					new(BackPoly->Vertices) FVector(*V);
				}

				PrevStatus = Status;
				W          = V++;
			}
			FrontPoly->Base			= Base;
			FrontPoly->Normal		= Normal;
			FrontPoly->PolyFlags	= PolyFlags;

			BackPoly->Base			= Base;
			BackPoly->Normal		= Normal;
			BackPoly->PolyFlags		= PolyFlags;
		}
		Result = SP_Split;
	}

	MemMark.Pop();

	return Result;
}

/**
 * Compute normal of an FPoly.  Works even if FPoly has 180-degree-angled sides (which
 * are often created during T-joint elimination).  Returns nonzero result (plus sets
 * normal vector to zero) if a problem occurs.
 */
int FPoly::CalcNormal( UBOOL bSilent )
{
	Normal = FVector(0,0,0);
	for( int i=2; i<Vertices.Num(); i++ )
		Normal += (Vertices(i-1) - Vertices(0)) ^ (Vertices(i) - Vertices(0));

	if( Normal.SizeSquared() < (FLOAT)THRESH_ZERO_NORM_SQUARED )
	{
// 		if( !bSilent )
// 			debugf( NAME_Warning, TEXT("FPoly::CalcNormal: Zero-area polygon") );
		return 1;
	}
	Normal.Normalize();
	return 0;
}

/**
 * Transform an editor polygon with a coordinate system, a pre-transformation
 * addition, and a post-transformation addition.
 */
void FPoly::Transform
(
	const FVector&		PreSubtract,
	const FVector&		PostAdd
)
{
	FVector 	Temp;
	int 		i;

	Base = (Base - PreSubtract) + PostAdd;
	for( i=0; i<Vertices.Num(); i++ )
		Vertices(i)  = (Vertices(i) - PreSubtract) + PostAdd;

	// Transform normal.  Since the transformation coordinate system is
	// orthogonal but not orthonormal, it has to be renormalized here.
	Normal = Normal.SafeNormal();

}

/**
 * Remove colinear vertices and check convexity.  Returns 1 if convex, 0 if nonconvex or collapsed.
 */
INT FPoly::RemoveColinears()
{
	FMemMark MemMark(GMainThreadMemStack);
	FVector* SidePlaneNormal = new(GMainThreadMemStack) FVector[Vertices.Num()];
	FVector  Side;
	INT      i,j;
	UBOOL Result = TRUE;

	for( i=0; i<Vertices.Num(); i++ )
	{
		j=(i+Vertices.Num()-1)%Vertices.Num();

		// Create cutting plane perpendicular to both this side and the polygon's normal.
		Side = Vertices(i) - Vertices(j);
		SidePlaneNormal[i] = Side ^ Normal;

		if( !SidePlaneNormal[i].Normalize() )
		{
			// Eliminate these nearly identical points.
			Vertices.Remove(i,1);
			if(Vertices.Num() < 3)
			{
				// Collapsed.
				Vertices.Empty();
				Result = FALSE;
				break;
			}
			i--;
		}
	}
	if(Result)
	{
		for( i=0; i<Vertices.Num(); i++ )
		{
			j=(i+1)%Vertices.Num();

			if( FPointsAreNear(SidePlaneNormal[i],SidePlaneNormal[j],FLOAT_NORMAL_THRESH) )
			{
				// Eliminate colinear points.
				appMemcpy (&SidePlaneNormal[i],&SidePlaneNormal[i+1],(Vertices.Num()-(i+1)) * sizeof (FVector));
				Vertices.Remove(i,1);
				if(Vertices.Num() < 3)
				{
					// Collapsed.
					Vertices.Empty();
					Result = FALSE;
					break;
				}
				i--;
			}
			else
			{
				switch( SplitWithPlane (Vertices(i),SidePlaneNormal[i],NULL,NULL,0) )
				{
					case SP_Front:
						Result = FALSE;
						break;
					case SP_Split:
						Result = FALSE;
						break;
					// SP_BACK: Means it's convex
					// SP_COPLANAR: Means it's probably convex (numerical precision)
				}
				if(!Result)
				{
					break;
				}
			}
		}
	}

	MemMark.Pop();

	return Result; // Ok.
}

/**
 * Checks to see if the specified line intersects this poly or not.  If "Intersect" is
 * a valid pointer, it is filled in with the intersection point.
 */
UBOOL FPoly::DoesLineIntersect( FVector Start, FVector End, FVector* Intersect )
{
	// If the ray doesn't cross the plane, don't bother going any further.
	const float DistStart = FPointPlaneDist( Start, Vertices(0), Normal );
	const float DistEnd = FPointPlaneDist( End, Vertices(0), Normal );

	if( (DistStart < 0 && DistEnd < 0) || (DistStart > 0 && DistEnd > 0 ) )
	{
		return 0;
	}

	// Get the intersection of the line and the plane.
	FVector Intersection = FLinePlaneIntersection(Start,End,Vertices(0),Normal);
	if( Intersect )	*Intersect = Intersection;
	if( Intersection == Start || Intersection == End )
	{
		return 0;
	}

	// Check if the intersection point is actually on the poly.
	return OnPoly( Intersection );
}

/**
 * Checks to see if the specified vertex is on this poly.  Assumes the vertex is on the same
 * plane as the poly and that the poly is convex.
 *
 * This can be combined with FLinePlaneIntersection to perform a line-fpoly intersection test.
 */
UBOOL FPoly::OnPoly( FVector InVtx )
{
	FVector  SidePlaneNormal;
	FVector  Side;

	for( INT x = 0 ; x < Vertices.Num() ; x++ )
	{
		// Create plane perpendicular to both this side and the polygon's normal.
		Side = Vertices(x) - Vertices((x-1 < 0 ) ? Vertices.Num()-1 : x-1 );
		SidePlaneNormal = Side ^ Normal;
		SidePlaneNormal.Normalize();

		// If point is not behind all the planes created by this polys edges, it's outside the poly.
		if( FPointPlaneDist( InVtx, Vertices(x), SidePlaneNormal ) > THRESH_POINT_ON_PLANE )
		{
			return 0;
		}
	}

	return 1;
}

// Inserts a vertex into the poly at a specific position.
//
void FPoly::InsertVertex( INT InPos, FVector InVtx )
{
	check( InPos <= Vertices.Num() );

	Vertices.InsertItem(InVtx,InPos);
}

// Removes a vertex from the polygons list of vertices
//
void FPoly::RemoveVertex( FVector InVtx )
{
	Vertices.RemoveItem(InVtx);
}

/**
 * Checks to see if all the vertices on a polygon are coplanar.
 */

UBOOL FPoly::IsCoplanar()
{
	// 3 or fewer vertices is automatically coplanar

	if( Vertices.Num() <= 3 )
	{
		return 1;
	}

	CalcNormal(1);

	for( INT x = 0 ; x < Vertices.Num() ; ++x )
	{
		if( !OnPlane( Vertices(x) ) )
		{
			return 0;
		}
	}

	// If we got this far, the poly has to be coplanar.

	return 1;
}

/**
* Checks to see if this polygon is a convex shape.
*
* @return	TRUE if this polygon is convex.
*/

UBOOL FPoly::IsConvex()
{
	// Create a set of planes that represent each edge of the polygon.

	TArray<FPlane> Planes;

	for( INT EdgeVert = 0 ; EdgeVert < Vertices.Num() ; ++EdgeVert )
	{
		const FVector& vtx1 = Vertices(EdgeVert);
		const FVector& vtx2 = Vertices((EdgeVert + 1) % Vertices.Num());
		FVector v2v = vtx2 - vtx1;

		FVector EdgeNormal = v2v ^ Normal;

		for( INT CheckVertLoop = 2 ; CheckVertLoop < Vertices.Num(); ++CheckVertLoop )
		{
			INT CheckVert = (CheckVertLoop + EdgeVert) % Vertices.Num();
			FVector RelativePos = Vertices(CheckVert) - Vertices(EdgeVert);

			if (0.0 < (EdgeNormal | RelativePos))
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

/**
 * Breaks down this polygon into triangles.  The triangles are then returned to the caller in an array.
 *
 * @param	InOwnerBrush	The brush that owns this polygon.
 * @param	InTriangles		An array to store the resulting triangles in.
 *
 * @return	The number of triangles created
 */

INT FPoly::Triangulate( ABrush* InOwnerBrush, TArray<FPoly>& OutTriangles )
{
#if WITH_EDITOR

	if( Vertices.Num() < 3 )
	{
		return 0;
	}

	FClipSMPolygon Polygon(0);

	for( INT v = 0 ; v < Vertices.Num() ; ++v )
	{
		FClipSMVertex vtx;
		vtx.Pos = Vertices(v);

		// Init other data so that VertsAreEqual won't compare garbage
		vtx.TangentX = FVector(0.f, 0.f, 0.f);
		vtx.TangentY = FVector(0.f, 0.f, 0.f);
		vtx.TangentZ = FVector(0.f, 0.f, 0.f);
		vtx.Color = FColor(0, 0, 0);
		for( INT uvIndex=0; uvIndex<ARRAY_COUNT(vtx.UVs); ++uvIndex )
		{
			vtx.UVs[uvIndex] = FVector2D(0.f, 0.f);
		}


		Polygon.Vertices.AddItem( vtx );
	}

	Polygon.FaceNormal = Normal;

	// Attempt to triangulate this polygon
	TArray<FClipSMTriangle> Triangles;
	if( TriangulatePoly( Triangles, Polygon ) )
	{
		// Create a set of FPolys from the triangles

		OutTriangles.Empty();
		FPoly TrianglePoly;

		for( INT p = 0 ; p < Triangles.Num() ; ++p )
		{
			FClipSMTriangle* tri = &(Triangles(p));

			TrianglePoly.Init();
			TrianglePoly.Base = tri->Vertices[0].Pos;

			TrianglePoly.Vertices.AddItem( tri->Vertices[0].Pos );
			TrianglePoly.Vertices.AddItem( tri->Vertices[1].Pos );
			TrianglePoly.Vertices.AddItem( tri->Vertices[2].Pos );

			if( TrianglePoly.Finalize( InOwnerBrush, 0 ) == 0 )
			{
				OutTriangles.AddItem( TrianglePoly );
			}
		}
	}

#endif

	return OutTriangles.Num();
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

	for( INT v = 0 ; v < Vertices.Num() ; ++v )
	{
		if( Vertices(v) == InVtx )
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

	for( INT v = 0 ; v < Vertices.Num() ; ++v )
	{
		mid += Vertices(v);
	}

	return mid / Vertices.Num();
}

/**
* Builds a huge poly aligned with the specified plane.  This poly is
* carved up by the calling routine and used as a capping poly following a clip operation.
*
* @param	InPlane		The plane to lay the polygon on
*
* @return	The resulting polygon
*/
FPoly FPoly::BuildInfiniteFPoly(const FPlane& InPlane)
{
	FVector Axis1, Axis2;

	// Find two non-problematic axis vectors.
	InPlane.FindBestAxisVectors( Axis1, Axis2 );

	// Set up the FPoly.
	FPoly EdPoly;
	EdPoly.Init();
	EdPoly.Normal.X    = InPlane.X;
	EdPoly.Normal.Y    = InPlane.Y;
	EdPoly.Normal.Z    = InPlane.Z;
	EdPoly.Base        = EdPoly.Normal * InPlane.W;
	EdPoly.Vertices.AddItem( EdPoly.Base + Axis1*HALF_WORLD_MAX + Axis2*HALF_WORLD_MAX );
	EdPoly.Vertices.AddItem( EdPoly.Base - Axis1*HALF_WORLD_MAX + Axis2*HALF_WORLD_MAX );
	EdPoly.Vertices.AddItem( EdPoly.Base - Axis1*HALF_WORLD_MAX - Axis2*HALF_WORLD_MAX );
	EdPoly.Vertices.AddItem( EdPoly.Base + Axis1*HALF_WORLD_MAX - Axis2*HALF_WORLD_MAX );

	return EdPoly;
}

/**
* Optimizes a set of polygons into a smaller set of convex polygons.
*
* @param	InOwnerBrush	The brush that owns the polygons.
* @param	InPolygons		A set of polygons that will be replaced with a new set of polygons that are merged together as much as possible.
*/

void FPoly::OptimizeIntoConvexPolys( ABrush* InOwnerBrush, TArray<FPoly>& InPolygons )
{
	UBOOL bDidMergePolygons = TRUE;

	while( bDidMergePolygons )
	{
		bDidMergePolygons = FALSE;

		for( INT p = 0 ; p < InPolygons.Num() && !bDidMergePolygons ; ++p )
		{
			const FPoly* PolyMain = &InPolygons(p);

			// Find all the polygons that neighbor this one (aka share an edge)

			for( INT p2 = 0 ; p2 < InPolygons.Num() && !bDidMergePolygons ; ++p2 )
			{
				const FPoly* PolyNeighbor = &InPolygons(p2);

				if( PolyMain != PolyNeighbor && PolyMain->Normal.Equals( PolyNeighbor->Normal ) )
				{
					// See if PolyNeighbor is sharing an edge with Poly

					INT idx1 = INDEX_NONE, idx2 = INDEX_NONE;
					FVector EdgeVtx1(0), EdgeVtx2(0);

					for( INT v = 0 ; v < PolyMain->Vertices.Num() ; ++v )
					{
						FVector vtx = PolyMain->Vertices(v);

						INT idx = INDEX_NONE;

						for( INT v2 = 0 ; v2 < PolyNeighbor->Vertices.Num() ; ++v2 )
						{
							const FVector* vtx2 = &PolyNeighbor->Vertices(v2);

							if( vtx.Equals( *vtx2 ) )
							{
								idx = v2;
								break;
							}
						}

						if( idx != INDEX_NONE )
						{
							if( idx1 == INDEX_NONE )
							{
								idx1 = v;
								EdgeVtx1 = vtx;
							}
							else if( idx2 == INDEX_NONE )
							{
								idx2 = v;
								EdgeVtx2 = vtx;
								break;
							}
						}
					}

					if( idx1 != INDEX_NONE && idx2 != INDEX_NONE )
					{
						// Found a shared edge.  Let's see if we can merge these polygons together.

						// Create a list of cutting planes

						TArray<FPlane> CuttingPlanes;

						for( INT v = 0 ; v < PolyMain->Vertices.Num() ; ++v )
						{
							const FVector* vtx1 = &PolyMain->Vertices(v);
							const FVector* vtx2 = &PolyMain->Vertices((v+1)%PolyMain->Vertices.Num());

							if( (vtx1->Equals( EdgeVtx1 ) && vtx2->Equals( EdgeVtx2 ) )
								|| (vtx1->Equals( EdgeVtx2 ) && vtx2->Equals( EdgeVtx1 ) ) )
							{
								// If these verts are on the shared edge, don't include this edge in the cutting planes array.
								continue;
							}

							FPlane plane( *vtx1, *vtx2, (*vtx2) + (PolyMain->Normal * 16.f) );
							CuttingPlanes.AddItem( plane );
						}

						for( INT v = 0 ; v < PolyNeighbor->Vertices.Num() ; ++v )
						{
							const FVector* vtx1 = &PolyNeighbor->Vertices(v);
							const FVector* vtx2 = &PolyNeighbor->Vertices((v+1)%PolyNeighbor->Vertices.Num());

							if( (vtx1->Equals( EdgeVtx1 ) && vtx2->Equals( EdgeVtx2 ) )
								|| (vtx1->Equals( EdgeVtx2 ) && vtx2->Equals( EdgeVtx1 ) ) )
							{
								// If these verts are on the shared edge, don't include this edge in the cutting planes array.
								continue;
							}

							FPlane plane( *vtx1, *vtx2, (*vtx2) + (PolyNeighbor->Normal * 16.f) );
							CuttingPlanes.AddItem( plane );
						}

						// Make sure that all verts lie behind all cutting planes.  This serves as our convexity check for the merged polygon.

						UBOOL bMergedPolyIsConvex = TRUE;
						FLOAT dot;

						for( INT p = 0 ; p < CuttingPlanes.Num() && bMergedPolyIsConvex ; ++p )
						{
							FPlane* plane = &CuttingPlanes(p);

							for( INT v = 0 ; v < PolyMain->Vertices.Num() ; ++v )
							{
								dot = plane->PlaneDot( PolyMain->Vertices(v) );
								if( dot > THRESH_POINT_ON_PLANE )
								{
									bMergedPolyIsConvex = FALSE;
									break;
								}
							}

							for( INT v = 0 ; v < PolyNeighbor->Vertices.Num() && bMergedPolyIsConvex ; ++v )
							{
								dot = plane->PlaneDot( PolyNeighbor->Vertices(v) );
								if( dot > THRESH_POINT_ON_PLANE )
								{
									bMergedPolyIsConvex = FALSE;
									break;
								}
							}
						}

						if( bMergedPolyIsConvex )
						{
							// OK, the resulting polygon will result in a convex polygon.  So create it by clipping a large polygon using
							// the cutting planes we created earlier.  The resulting polygon will be the merged result.

							FPlane NormalPlane = FPlane( PolyMain->Vertices(0), PolyMain->Vertices(1), PolyMain->Vertices(2) );
							FPoly PolyMerged = BuildInfiniteFPoly( NormalPlane );
							PolyMerged.Finalize( InOwnerBrush, 1 );

							FPoly Front, Back;
							INT result;

							for( INT p = 0 ; p < CuttingPlanes.Num() ; ++ p )
							{
								const FPlane* Plane = &CuttingPlanes(p);

								result = PolyMerged.SplitWithPlane( Plane->SafeNormal() * Plane->W, Plane->SafeNormal(), &Front, &Back, 1 );

								if( result == SP_Split )
								{
									PolyMerged = Back;
								}
							}

							PolyMerged.Reverse();

							// Verts that result from these clips are assured of being on the 1 grid.  Go through and snap them
							// there to make sure (aka avoid float drift).

							for( INT v = 0 ; v < PolyMerged.Vertices.Num() ; ++v )
							{
								FVector* vtx = &PolyMerged.Vertices(v);
								*vtx = vtx->GridSnap( 1.0f );
							}

							PolyMerged.CalcNormal(1);

							if( PolyMerged.Finalize( InOwnerBrush, 1 ) == 0 )
							{
								// Remove the original polygons from the list

								INT idx1 = InPolygons.FindItemIndex( *PolyMain );
								INT idx2 = InPolygons.FindItemIndex( *PolyNeighbor );

								if( idx2 > idx1 )
								{
									InPolygons.Remove( idx2 );
									InPolygons.Remove( idx1 );
								}
								else
								{
									InPolygons.Remove( idx1 );
									InPolygons.Remove( idx2 );
								}

								// Add the newly merged polygon into the list

								InPolygons.AddItem( PolyMerged );

								// Tell the outside loop that we merged polygons and need to run through it all again

								bDidMergePolygons = TRUE;
							}
						}
					}
				}
			}
		}
	}
}

/**
* Takes a set of polygons and returns a vertex array representing the outside winding
* for them.
*
* NOTE : This will work for convex or concave sets of polygons but not for concave polygons with holes.
*
* @param	InOwnerBrush	The brush that owns the polygons.
* @param	InPolygons		The set of polygons that you want to get the winding for
* @param	InWindings		The resulting sets of vertices that represent the windings
*/

void FPoly::GetOutsideWindings( ABrush* InOwnerBrush, TArray<FPoly>& InPolygons, TArray< TArray<FVector> >& InWindings, UBOOL bFlipNormals /*= TRUE*/ )
{
	InWindings.Empty();
	FVector SaveNormal(0);

	// Break up every polygon passed into triangles

	TArray<FPoly> Triangles;

	for( INT p = 0 ; p < InPolygons.Num() ; ++p )
	{
		FPoly* Poly = &InPolygons(p);

		SaveNormal = Poly->Normal;

		TArray<FPoly> Polys;
		Poly->Triangulate( InOwnerBrush, Polys );

		Triangles.Append( Polys );
	}

	// Generate a list of ordered edges that represent the outside winding

	TArray<FEdge> EdgePool;

	for( INT p = 0 ; p < Triangles.Num() ; ++p )
	{
		FPoly* Poly = &Triangles(p);

		// Create a list of edges that are in this shape and set their counts to the number of times they are used.

		for( INT v = 0 ; v < Poly->Vertices.Num() ; ++v )
		{
			const FVector vtx0 = Poly->Vertices(v);
			const FVector vtx1 = Poly->Vertices( (v+1) % Poly->Vertices.Num() );

			FEdge Edge( vtx0, vtx1 );

			INT idx;
			if( EdgePool.FindItem( Edge, idx ) )
			{
				EdgePool(idx).Count++;
			}
			else
			{
				Edge.Count = 1;
				EdgePool.AddUniqueItem( Edge );
			}
		}
	}

	// Remove any edges from the list that are used more than once.  This will leave us with a collection of edges that represents the outside of the brush shape.

	for( INT e = 0 ; e < EdgePool.Num() ; ++e )
	{
		const FEdge* Edge = &EdgePool(e);

		if( Edge->Count > 1 )
		{
			EdgePool.Remove( e );
			e = -1;
		}
	}

	// Organize the remaining edges in the list so that the vertices will meet up, start to end, properly to form a continuous outline around the brush shape.

	while( EdgePool.Num() )
	{
		TArray<FEdge> OrderedEdges;

		FEdge Edge0 = EdgePool(0);

		OrderedEdges.AddItem( Edge0 );

		for( INT e = 1 ; e < EdgePool.Num() ; ++e )
		{
			FEdge Edge1 = EdgePool(e);

			if( Edge0.Vertex[1].Equals( Edge1.Vertex[0] ) )
			{
				// If these edges are already lined up correctly then add Edge1 into the ordered array, remove it from the pool and start over.

				OrderedEdges.AddItem( Edge1 );
				Edge0 = Edge1;
				EdgePool.Remove( e );
				e = -1;
			}
			else if( Edge0.Vertex[1].Equals( Edge1.Vertex[1] ) )
			{
				// If these edges are lined up but the verts are backwards, swap the verts on Edge1, add it into the ordered array, remove it from the pool and start over.

				Exchange( Edge1.Vertex[0], Edge1.Vertex[1] );

				OrderedEdges.AddItem( Edge1 );
				Edge0 = Edge1;
				EdgePool.Remove( e );
				e = -1;
			}
		}

		if(bFlipNormals)
		{
			// Create a polygon from the first 3 edges.  Compare the normal of the original brush shape polygon and see if they match.  If
			// they don't, the list of edges will need to be flipped so it faces the other direction.

			if( OrderedEdges.Num() > 2 )
			{
				FPoly TestPoly;
				TestPoly.Init();

				TestPoly.Vertices.AddItem( OrderedEdges(0).Vertex[0] );
				TestPoly.Vertices.AddItem( OrderedEdges(1).Vertex[0] );
				TestPoly.Vertices.AddItem( OrderedEdges(2).Vertex[0] );

				if( TestPoly.Finalize( InOwnerBrush, 1 ) == 0 )
				{
					if( TestPoly.Normal.Equals( SaveNormal ) == FALSE )
					{
						TArray<FEdge> SavedEdges = OrderedEdges;
						OrderedEdges.Empty();

						for( INT e = SavedEdges.Num() - 1 ; e > -1 ; --e )
						{
							FEdge* Edge = &SavedEdges(e);

							Exchange( Edge->Vertex[0], Edge->Vertex[1] );
							OrderedEdges.AddItem( *Edge );
						}
					}
				}
			}
		}

		// Create the winding array

		TArray<FVector> WindingVerts;
		for( INT e = 0 ; e < OrderedEdges.Num() ; ++e )
		{
			FEdge* Edge = &OrderedEdges(e);

			WindingVerts.AddItem( Edge->Vertex[0] );
		}

		InWindings.AddItem( WindingVerts );
	}
}

/**
 * Checks to see if the specified vertex lies on this polygons plane.
 */
UBOOL FPoly::OnPlane( FVector InVtx )
{
	return ( Abs( FPointPlaneDist( InVtx, Vertices(0), Normal ) ) < THRESH_POINT_ON_PLANE );
}

/**
 * Split a poly and keep only the front half. Returns number of vertices, 0 if clipped away.
 */
int FPoly::Split( const FVector &Normal, const FVector &Base )
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
			return Vertices.Num();
		default:
			return Vertices.Num();
	}
}

/**
 * Compute all remaining polygon parameters (normal, etc) that are blank.
 * Returns 0 if ok, nonzero if problem.
 */
int FPoly::Finalize( ABrush* InOwner, int NoError )
{
	// Check for problems.
	Fix();
	if( Vertices.Num()<3 )
	{
		// Since we don't have enough vertices, remove this polygon from the brush
		check( InOwner );
		for( INT p = 0 ; p < InOwner->Brush->Polys->Element.Num() ; ++p )
		{
			if( InOwner->Brush->Polys->Element(p) == *this )
			{
				InOwner->Brush->Polys->Element.Remove(p);
				break;
			}
		}
	
// 		debugf( NAME_Warning, TEXT("FPoly::Finalize: Not enough vertices (%i)"), Vertices.Num() );
		if( NoError )
			return -1;
		else
		{
// 			debugf( TEXT("FPoly::Finalize: Not enough vertices (%i) : polygon removed from brush"), Vertices.Num() );
			return -2;
		}
	}

	// If no normal, compute from cross-product and normalize it.
	if( Normal.IsZero() && Vertices.Num()>=3 )
	{
		if( CalcNormal() )
		{
// 			debugf( NAME_Warning, TEXT("FPoly::Finalize: Normalization failed, verts=%i, size=%f"), Vertices.Num(), Normal.Size() );
			if( NoError )
				return -1;
			else
				appErrorf( LocalizeSecure(LocalizeUnrealEd("Error_FinalizeNormalizationFailed"), Vertices.Num(), Normal.Size()) );
		}
	}

	// If texture U and V coordinates weren't specified, generate them.
	if( TextureU.IsZero() && TextureV.IsZero() )
	{
		for( int i=1; i<Vertices.Num(); i++ )
		{
			TextureU = ((Vertices(0) - Vertices(i)) ^ Normal).SafeNormal();
			TextureV = (Normal ^ TextureU).SafeNormal();
			if( TextureU.SizeSquared()!=0 && TextureV.SizeSquared()!=0 )
				break;
		}
	}
	return 0;
}

/**
 * Return whether this poly and Test are facing each other.
 * The polys are facing if they are noncoplanar, one or more of Test's points is in 
 * front of this poly, and one or more of this poly's points are behind Test.
 */
int FPoly::Faces( const FPoly &Test ) const
{
	// Coplanar implies not facing.
	if( IsCoplanar( Test ) )
		return 0;

	// If this poly is frontfaced relative to all of Test's points, they're not facing.
	for( int i=0; i<Test.Vertices.Num(); i++ )
	{
		if( !IsBackfaced( Test.Vertices(i) ) )
		{
			// If Test is frontfaced relative to on or more of this poly's points, they're facing.
			for( i=0; i<Vertices.Num(); i++ )
				if( Test.IsBackfaced( Vertices(i) ) )
					return 1;
			return 0;
		}
	}
	return 0;
}

/**
* Static constructor called once per class during static initialization via IMPLEMENT_CLASS
* macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
* properties for native- only classes.
*/
void UPolys::StaticConstructor()
{
	UClass* TheClass = GetClass();
	const DWORD SkipIndexIndex = TheClass->EmitStructArrayBegin( STRUCT_OFFSET( UPolys, Element ), sizeof(FPoly) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( FPoly, Actor ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( FPoly, Material ) );
	TheClass->EmitStructArrayEnd( SkipIndexIndex );
}

/**
* Note that the object has been modified.  If we are currently recording into the 
* transaction buffer (undo/redo), save a copy of this object into the buffer and 
* marks the package as needing to be saved.
*
* @param	bAlwaysMarkDirty	if TRUE, marks the package dirty even if we aren't
*								currently recording an active undo/redo transaction
*/
void UPolys::Modify(UBOOL bAlwaysMarkDirty)
{
	Super::Modify(bAlwaysMarkDirty);

	Element.ModifyAllItems();
}

	// UObject interface.
void UPolys::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	if( Ar.IsTransacting() )
	{
		Ar << Element;
	}
	else
	{
		Element.CountBytes( Ar );
		INT DbNum=Element.Num(), DbMax=DbNum;
		Ar << DbNum << DbMax;

		UObject* ElementOwner = Element.GetOwner();
		Ar << ElementOwner;

		Element.SetOwner(ElementOwner);

		if( Ar.IsLoading() )
		{
			Element.Empty( DbNum );
			Element.AddZeroed( DbNum );
		}
		for( INT i=0; i<Element.Num(); i++ )
		{		
			Ar << Element(i);
		}
	}
}

/**
 * Fixup improper load flags on save so that the load flags are always up to date
 */
void UPolys::PreSave()
{
	Super::PreSave();

#if WITH_EDITORONLY_DATA
	UObject* FlagsSource = NULL;
	// Figure out what outer to copy load flags from (usually model, or the brush holding a model)
	if (GetOuter()->IsA(UModel::StaticClass()) && GetOuter()->GetOuter() && GetOuter()->GetOuter()->IsA(ABrush::StaticClass()))
	{
		FlagsSource = GetOuter()->GetOuter();
	}
	else
	{
		FlagsSource = GetOuter();
	}

	// reset our not load flags
	ClearFlags(RF_NotForEdit | RF_NotForClient | RF_NotForServer);

	// propagate the not for flags from the source
	if (FlagsSource->HasAnyFlags(RF_NotForEdit))
	{
		ClearFlags(RF_LoadForEdit);
		SetFlags(RF_NotForEdit);
	}
	if (FlagsSource->HasAnyFlags(RF_NotForClient))
	{
		ClearFlags(RF_LoadForClient);
		SetFlags(RF_NotForClient);
	}
	if (FlagsSource->HasAnyFlags(RF_NotForServer))
	{
		ClearFlags(RF_LoadForServer);
		SetFlags(RF_NotForServer);
	}
#endif // WITH_EDITORONLY_DATA
}

IMPLEMENT_CLASS(UPolys);
