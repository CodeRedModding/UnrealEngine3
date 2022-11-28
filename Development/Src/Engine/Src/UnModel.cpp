/*=============================================================================
	UnModel.cpp: Unreal model functions
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UModelComponent);

/*-----------------------------------------------------------------------------
	Struct serializers.
-----------------------------------------------------------------------------*/

FArchive& operator<<( FArchive& Ar, FBspSurf& Surf )
{
	Ar << Surf.Material;
	Ar << Surf.PolyFlags << Surf.pBase << Surf.vNormal;
	Ar << Surf.vTextureU << Surf.vTextureV;
	Ar << Surf.iBrushPoly;
	Ar << Surf.Actor;
	Ar << Surf.Plane;
	Ar << Surf.LightMapScale;

	return Ar;
}

FArchive& operator<<( FArchive& Ar, FPoly& Poly )
{
	Ar << Poly.NumVertices;
	Ar << Poly.Base << Poly.Normal << Poly.TextureU << Poly.TextureV;
	for( INT i=0; i<Poly.NumVertices; i++ )
	{	
		Ar << Poly.Vertex[i];
	}
	Ar << Poly.PolyFlags;
	Ar << Poly.Actor << Poly.ItemName;
	Ar << Poly.Material;
	Ar << Poly.iLink << Poly.iBrushPoly;
	Ar << Poly.LightMapScale;

	return Ar;
}

FArchive& operator<<( FArchive& Ar, FBspNode& N )
{
	Ar << N.Plane << N.ZoneMask << N.NodeFlags << N.iVertPool << N.iSurf;
	Ar << N.iChild[0] << N.iChild[1] << N.iChild[2];
	Ar << N.iCollisionBound;

	Ar << N.iZone[0] << N.iZone[1];
	Ar << N.NumVertices;
	Ar << N.iLeaf[0] << N.iLeaf[1];

	if( Ar.IsLoading() )
		N.NodeFlags &= ~(NF_IsNew|NF_IsFront|NF_IsBack);

	return Ar;
}

/*---------------------------------------------------------------------------------------
	UModel object implementation.
---------------------------------------------------------------------------------------*/

void UModel::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	Ar << Bounds;

	if( Ar.Ver() < 180 )
	{
		URB_BodySetup* BodySetup;
		Ar << BodySetup;
	}

	Ar << Vectors << Points << Nodes << Surfs << Verts << NumSharedSides << NumZones;
	for( INT i=0; i<NumZones; i++ )
		Ar << Zones[i];
	Ar << Polys;
	if( Polys && !Ar.IsTrans() )
		Ar.Preload( Polys );

	if( Ar.Ver() < 180 )
	{
		TArray<FBox> Bounds;
		Ar << Bounds;
	}

	Ar << LeafHulls << Leaves;
	Ar << RootOutside << Linked;	
	Ar << PortalNodes;
}
void UModel::ModifySurf( INT Index, INT UpdateMaster )
{
	Surfs.ModifyItem( Index );
	FBspSurf& Surf = Surfs(Index);
	if( UpdateMaster && Surf.Actor )
	{
		Surf.Actor->Brush->Polys->Element.ModifyItem( Surf.iBrushPoly );
	}

}
void UModel::ModifyAllSurfs( INT UpdateMaster )
{
	for( INT i=0; i<Surfs.Num(); i++ )
		ModifySurf( i, UpdateMaster );

}
void UModel::ModifySelectedSurfs( INT UpdateMaster )
{
	for( INT i=0; i<Surfs.Num(); i++ )
		if( Surfs(i).PolyFlags & PF_Selected )
			ModifySurf( i, UpdateMaster );

}

void UModel::Rename( const TCHAR* InName, UObject* NewOuter )
{
	// Also rename the UPolys.
    if( NewOuter && Polys && Polys->GetOuter() == GetOuter() )
		Polys->Rename(Polys->GetName(), NewOuter);

    Super::Rename( InName, NewOuter );

}

IMPLEMENT_CLASS(UModel);

/*---------------------------------------------------------------------------------------
	UModel implementation.
---------------------------------------------------------------------------------------*/

//
// Lock a model.
//
void UModel::Modify( UBOOL DoTransArrays )
{
	// Modify all child objects.
	//warning: Don't modify self because model contains a dynamic array.
	if( Polys   ) Polys->Modify();

}

//
// Empty the contents of a model.
//
void UModel::EmptyModel( INT EmptySurfInfo, INT EmptyPolys )
{
	Nodes			.Empty();
	LeafHulls		.Empty();
	Leaves			.Empty();
	Verts			.Empty();
	PortalNodes		.Empty();

	if( EmptySurfInfo )
	{
		Vectors.Empty();
		Points.Empty();
		Surfs.Empty();
	}
	if( EmptyPolys )
	{
		Polys = new( GetOuter(), NAME_None, RF_Transactional )UPolys;
	}

	// Init variables.
	NumSharedSides	= 4;
	NumZones = 0;
	for( INT i=0; i<FBspNode::MAX_ZONES; i++ )
	{
		Zones[i].ZoneActor    = NULL;
		Zones[i].Connectivity = FZoneSet::IndividualZone(i);
		Zones[i].Visibility   = FZoneSet::AllZones();
	}
}

//
// Create a new model and allocate all objects needed for it.
//
UModel::UModel( ABrush* Owner, UBOOL InRootOutside )
:	RootOutside	( InRootOutside )
,	Surfs		( this )
,	Vectors		( this )
,	Points		( this )
,	Verts		( this )
,	Nodes		( this )
{
	SetFlags( RF_Transactional );
	EmptyModel( 1, 1 );
	if( Owner )
	{
		check(Owner->BrushComponent);
		Owner->Brush = this;
		Owner->InitPosRotScale();
	}
}

//
// Build the model's bounds (min and max).
//
void UModel::BuildBound()
{
	if( Polys && Polys->Element.Num() )
	{
		TArray<FVector> Points;
		for( INT i=0; i<Polys->Element.Num(); i++ )
			for( INT j=0; j<Polys->Element(i).NumVertices; j++ )
				Points.AddItem(Polys->Element(i).Vertex[j]);
		Bounds = FBoxSphereBounds( &Points(0), Points.Num() );
	}
}

//
// Transform this model by its coordinate system.
//
void UModel::Transform( ABrush* Owner )
{
	check(Owner);

	Polys->Element.ModifyAllItems();

	for( INT i=0; i<Polys->Element.Num(); i++ )
		Polys->Element( i ).Transform( Owner->PrePivot, Owner->Location);

}

/**
 * Returns the scale dependent texture factor used by the texture streaming code.	
 */
FLOAT UModel::GetStreamingTextureFactor()
{
	FLOAT MaxTextureRatio = 0.f;

	if( Polys )
	{
		for( INT PolyIndex=0; PolyIndex<Polys->Element.Num(); PolyIndex++ )
		{
			FPoly& Poly = Polys->Element(PolyIndex);

			// Assumes that all triangles on a given poly share the same texture scale.
			if( Poly.NumVertices > 2 )
			{
				FLOAT		L1	= (Poly.Vertex[0] - Poly.Vertex[1]).Size(),
							L2	= (Poly.Vertex[0] - Poly.Vertex[2]).Size();

				FVector2D	UV0	= FVector2D( (Poly.Vertex[0] - Poly.Base) | Poly.TextureU, (Poly.Vertex[0] - Poly.Base) | Poly.TextureV ),
							UV1	= FVector2D( (Poly.Vertex[1] - Poly.Base) | Poly.TextureU, (Poly.Vertex[1] - Poly.Base) | Poly.TextureV ),
							UV2	= FVector2D( (Poly.Vertex[2] - Poly.Base) | Poly.TextureU, (Poly.Vertex[2] - Poly.Base) | Poly.TextureV );

				if( Abs(L1 * L2) > Square(SMALL_NUMBER) )
				{
					FLOAT		T1	= (UV0 - UV1).Size() / 128,
								T2	= (UV0 - UV2).Size() / 128;

					if( Abs(T1 * T2) > Square(SMALL_NUMBER) )
					{
						MaxTextureRatio = Max3( MaxTextureRatio, L1 / T1, L1 / T2 );
					}
				}
			}
		}
	}

	return MaxTextureRatio;
}

/*---------------------------------------------------------------------------------------
	UModel basic implementation.
---------------------------------------------------------------------------------------*/

//
// Shrink all stuff to its minimum size.
//
void UModel::ShrinkModel()
{
	Vectors		.Shrink();
	Points		.Shrink();
	Verts		.Shrink();
	Nodes		.Shrink();
	Surfs		.Shrink();
	if( Polys     ) Polys    ->Element.Shrink();
	LeafHulls	.Shrink();
	PortalNodes	.Shrink();
}

//
//	UModel::GetZoneMask
//

static FZoneSet GetZoneMaskRecurse(const UModel* Model,const FBoxSphereBounds& Bounds,INT NodeIndex,INT ZoneIndex)
{
	FZoneSet	ZoneMask = FZoneSet::NoZones();

	while(NodeIndex != INDEX_NONE)
	{
		const FBspNode&	Node = Model->Nodes(NodeIndex);
		FLOAT			Distance = Node.Plane.PlaneDot(Bounds.Origin),
						PushOut = Min(Bounds.SphereRadius,FBoxPushOut(Node.Plane,Bounds.BoxExtent));

		if(Distance < -PushOut)
		{
			NodeIndex = Node.iChild[0];
			ZoneIndex = Node.iZone[0];
		}
		else if(Distance > PushOut)
		{
			NodeIndex = Node.iChild[1];
			ZoneIndex = Node.iZone[1];
		}
		else
		{
			ZoneMask |= GetZoneMaskRecurse(Model,Bounds,Node.iChild[1],Node.iZone[1]);
			NodeIndex = Node.iChild[0];
			ZoneIndex = Node.iZone[0];
		}
	};

	if(ZoneIndex)
		ZoneMask |= FZoneSet::IndividualZone(ZoneIndex);

	return ZoneMask;
}

/**
 * Finds the zones touched by a bounding volume.
 * 
 * @param Bounds The bounding volume.
 * 
 * @return The zones which are touched by Bounds.
 */
FZoneSet UModel::GetZoneMask(const FBoxSphereBounds& Bounds) const
{
	if(Nodes.Num() && NumZones)
	{
		FZoneSet	Result = GetZoneMaskRecurse(this,Bounds,0,0);
		if(Result.IsEmpty())
			return FZoneSet::IndividualZone(0);
		else
			return Result;
	}
	else
		return FZoneSet::IndividualZone(0);
}

//
//	UModelComponent::Serialize
//

void UModelComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << Model;
	Ar << StaticLights;
}
