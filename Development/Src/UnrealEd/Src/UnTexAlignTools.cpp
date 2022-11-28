/*=============================================================================
	UnTexAlignTools.cpp: Tools for aligning textures on surfaces
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#include "UnrealEd.h"

class FTexAlignTools;

INT GetMajorAxis( FVector InNormal, INT InForceAxis )
{
	// Figure out the major axis information.
	INT Axis = TAXIS_X;
	if( ::fabs(InNormal.Y) >= 0.5f ) Axis = TAXIS_Y;
	else 
	{
		// Only check Z if we aren't aligned to walls
		if( InForceAxis != TAXIS_WALLS )
			if( ::fabs(InNormal.Z) >= 0.5f ) Axis = TAXIS_Z;
	}

	return Axis;

}

// Checks the normal of the major axis ... if it's negative, returns 1.
UBOOL ShouldFlipVectors( FVector InNormal, INT InAxis )
{
	if( InAxis == TAXIS_X )
		if( InNormal.X < 0 ) return 1;
	if( InAxis == TAXIS_Y )
		if( InNormal.Y < 0 ) return 1;
	if( InAxis == TAXIS_Z )
		if( InNormal.Z < 0 ) return 1;

	return 0;

}

/*------------------------------------------------------------------------------
	UTexAligner.

	Base class for all texture aligners.
------------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UTexAligner);

UTexAligner::UTexAligner()
{
	Desc = TEXT("N/A");

	TAxisEnum = new( GetClass(), TEXT("TAxis") )UEnum( NULL );
	new(TAxisEnum->Names)FName( TEXT("TAXIS_X") );
	new(TAxisEnum->Names)FName( TEXT("TAXIS_Y") );
	new(TAxisEnum->Names)FName( TEXT("TAXIS_Z") );
	new(TAxisEnum->Names)FName( TEXT("TAXIS_WALLS") );
	new(TAxisEnum->Names)FName( TEXT("TAXIS_AUTO") );

	TAxis = TAXIS_AUTO;
	UTile = VTile = 1.f;

	DefTexAlign = TEXALIGN_Default;
}

void UTexAligner::InitFields()
{
	new(GetClass(),TEXT("VTile"),	RF_Public)UFloatProperty	(CPP_PROPERTY(VTile),	TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("UTile"),	RF_Public)UFloatProperty	(CPP_PROPERTY(UTile),	TEXT(""), CPF_Edit );
}

void UTexAligner::Align( ETexAlign InTexAlignType, UModel* InModel )
{
	//
	// Build an initial list of BSP surfaces to be aligned.
	//
	
	FPoly EdPoly;
	TArray<FBspSurfIdx> InitialSurfList;
	FBox PolyBBox(1);

	for( INT i = 0 ; i < InModel->Surfs.Num() ; i++ )
	{
		FBspSurf* Surf = &InModel->Surfs(i);
		GEditor->polyFindMaster( InModel, i, EdPoly );
		FVector Normal = InModel->Vectors( Surf->vNormal );

		if( Surf->PolyFlags & PF_Selected )
		{
			new(InitialSurfList)FBspSurfIdx( Surf, i );

			switch( InTexAlignType )
			{
				case TEXALIGN_Face:
				//case TEXALIGN_Cylinder:
				{
					for( INT x = 0 ; x < EdPoly.NumVertices ; x++ )
						PolyBBox += (EdPoly.Vertex[x] + Surf->Actor->Location);
				}
				break;
			}
		}
	}

	//
	// Create a final list of BSP surfaces ... 
	//
	// - allows for rejection of surfaces
	// - allows for specific ordering of faces
	//

	TArray<FBspSurfIdx> FinalSurfList;
	FVector Normal;

	for( INT i = 0 ; i < InitialSurfList.Num() ; i++ )
	{
		FBspSurfIdx* Surf = &InitialSurfList(i);
		Normal = InModel->Vectors( Surf->Surf->vNormal );
		GEditor->polyFindMaster( InModel, Surf->Idx, EdPoly );

		UBOOL bOK = 1;
		/*
		switch( InTexAlignType )
		{
		}
		*/

		if( bOK )
			new(FinalSurfList)FBspSurfIdx( Surf->Surf, Surf->Idx );
	}

	//
	// Align the final surfaces.
	//

	for( INT i = 0 ; i < FinalSurfList.Num() ; i++ )
	{
		FBspSurfIdx* Surf = &FinalSurfList(i);
		GEditor->polyFindMaster( InModel, Surf->Idx, EdPoly );
		Normal = InModel->Vectors( Surf->Surf->vNormal );

		AlignSurf( InTexAlignType == TEXALIGN_None ? DefTexAlign : InTexAlignType, InModel, Surf, &EdPoly, &Normal );

		GEditor->polyUpdateMaster( InModel, Surf->Idx, 1 );
	}

	GEditor->RedrawLevel( GEditor->Level );

}

// Aligns a specific BSP surface
void UTexAligner::AlignSurf( ETexAlign InTexAlignType, UModel* InModel, FBspSurfIdx* InSurfIdx, FPoly* InPoly, FVector* InNormal )
{
}

/*------------------------------------------------------------------------------
	UTexAlignerPlanar

	Aligns according to which axis the poly is most facing.
------------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UTexAlignerPlanar);

UTexAlignerPlanar::UTexAlignerPlanar()
{
	Desc = TEXT("Planar");
	DefTexAlign = TEXALIGN_Planar;
}

void UTexAlignerPlanar::InitFields()
{
	UTexAligner::InitFields();
	new(GetClass(),TEXT("TAxis"),	RF_Public)UByteProperty		(CPP_PROPERTY(TAxis),	TEXT(""), CPF_Edit, TAxisEnum );
}

void UTexAlignerPlanar::AlignSurf( ETexAlign InTexAlignType, UModel* InModel, FBspSurfIdx* InSurfIdx, FPoly* InPoly, FVector* InNormal )
{
	if( InTexAlignType == TEXALIGN_PlanarAuto )
		TAxis = TAXIS_AUTO;
	else if( InTexAlignType == TEXALIGN_PlanarWall )
		TAxis = TAXIS_WALLS;
	else if( InTexAlignType == TEXALIGN_PlanarFloor )
		TAxis = TAXIS_Z;

	INT Axis = GetMajorAxis( *InNormal, TAxis );

	if( TAxis != TAXIS_AUTO && TAxis != TAXIS_WALLS )
		Axis = TAxis;

	UBOOL bFlip = ShouldFlipVectors( *InNormal, Axis );

	// Determine the texturing vectors.
	FVector U, V;
	if( Axis == TAXIS_X )
	{
		U = FVector(0, (bFlip ? 1 : -1) ,0);
		V = FVector(0,0,-1);
	}
	else if( Axis == TAXIS_Y )
	{
		U = FVector((bFlip ? -1 : 1),0,0);
		V = FVector(0,0,-1);
	}
	else
	{
		U = FVector((bFlip ? 1 : -1),0,0);
		V = FVector(0,-1,0);
	}

	FVector Base = FVector(0,0,0);

	U *= UTile;
	V *= VTile;

	InSurfIdx->Surf->pBase = GEditor->bspAddPoint(InModel,&Base,0);
	InSurfIdx->Surf->vTextureU = GEditor->bspAddVector( InModel, &U, 0);
	InSurfIdx->Surf->vTextureV = GEditor->bspAddVector( InModel, &V, 0);

}

/*------------------------------------------------------------------------------
	UTexAlignerDefault

	Aligns to a default settting.
------------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UTexAlignerDefault);

UTexAlignerDefault::UTexAlignerDefault()
{
	Desc = TEXT("Default");
	DefTexAlign = TEXALIGN_Default;
}

void UTexAlignerDefault::InitFields()
{
	UTexAligner::InitFields();
}

void UTexAlignerDefault::AlignSurf( ETexAlign InTexAlignType, UModel* InModel, FBspSurfIdx* InSurfIdx, FPoly* InPoly, FVector* InNormal )
{
	InPoly->Base = FVector(0,0,0);
	InPoly->TextureU = FVector(0,0,0);
	InPoly->TextureV = FVector(0,0,0);
	InPoly->Finalize( NULL, 0 );
	InPoly->Transform( FVector(0,0,0), FVector(0,0,0) );

	InPoly->TextureU *= UTile;
	InPoly->TextureV *= VTile;

	InSurfIdx->Surf->vTextureU = GEditor->bspAddVector( InModel, &InPoly->TextureU, 0);
	InSurfIdx->Surf->vTextureV = GEditor->bspAddVector( InModel, &InPoly->TextureV, 0);

}

/*------------------------------------------------------------------------------
	UTexAlignerBox

	Aligns to the best U and V axis according to the polys normal.
------------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UTexAlignerBox);

UTexAlignerBox::UTexAlignerBox()
{
	Desc = TEXT("Box");
	DefTexAlign = TEXALIGN_Box;
}

void UTexAlignerBox::InitFields()
{
	UTexAligner::InitFields();
}

void UTexAlignerBox::AlignSurf( ETexAlign InTexAlignType, UModel* InModel, FBspSurfIdx* InSurfIdx, FPoly* InPoly, FVector* InNormal )
{
	FVector U, V;

	InNormal->FindBestAxisVectors( V, U );
	U *= -1.0;
	V *= -1.0;

	U *= UTile;
	V *= VTile;

	FVector	Base = FVector(0,0,0);

	InSurfIdx->Surf->pBase = GEditor->bspAddPoint(InModel,&Base,0);
	InSurfIdx->Surf->vTextureU = GEditor->bspAddVector( InModel, &U, 0 );
	InSurfIdx->Surf->vTextureV = GEditor->bspAddVector( InModel, &V, 0 );

}

/*------------------------------------------------------------------------------
	UTexAlignerFace

	Aligns the texture so it fits on the poly exactly.
------------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UTexAlignerFace);

UTexAlignerFace::UTexAlignerFace()
{
	Desc = TEXT("Face");
	DefTexAlign = TEXALIGN_Box;
}

void UTexAlignerFace::InitFields()
{
	UTexAligner::InitFields();
}

static QSORT_RETURN CDECL CompareVerts( const FVector* A, const FVector* B )
{
	return A->Size() < B->Size();
}

void UTexAlignerFace::AlignSurf( ETexAlign InTexAlignType, UModel* InModel, FBspSurfIdx* InSurfIdx, FPoly* InPoly, FVector* InNormal )
{
	UBOOL bSubtractive = (InSurfIdx->Surf->Actor->CsgOper == CSG_Subtract);

	// Transform the polygons vertices so that the poly faces due East

	FRotator NormalRot = InNormal->Rotation();
	FRotator DiffRot = NormalRot * -1;

	FVector NewVerts[FPoly::MAX_VERTICES];
	appMemset( NewVerts, 0, sizeof(FVector)*FPoly::MAX_VERTICES );
	FBox bboxWorld(0), bboxPlane(0);

	for( INT x = 0 ; x < InPoly->NumVertices ; ++x )
	{
		bboxWorld += InSurfIdx->Surf->Actor->LocalToWorld().TransformFVector( InPoly->Vertex[x] );

		FVector vtx = FRotationMatrix( DiffRot ).TransformFVector( InPoly->Vertex[x] );
		bboxPlane += vtx;

		NewVerts[x] = vtx;
	}

	// Find the lowest vertex (the one with the smallest values).  This will be tex coord 0,1

	FVector SortedVtx[FPoly::MAX_VERTICES];
	appMemcpy( SortedVtx, InPoly->Vertex, sizeof(FVector)*FPoly::MAX_VERTICES );
	appQsort( SortedVtx, InPoly->NumVertices, sizeof(FVector), (QSORT_COMPARE)CompareVerts );

	INT SortedVtxIdx = -1;
	for( INT x = 0 ; x < InPoly->NumVertices ; ++x )
		if( InPoly->Vertex[x] == SortedVtx[0] )
		{
			SortedVtxIdx = x;
			break;
		}
		
	// Figure out how large the polygon is in the U and V directions

	FLOAT USz = ( FVector( 0, bboxPlane.Max.Y, 0 ) - FVector( 0, bboxPlane.Min.Y, 0 ) ).Size(),
		VSz = ( FVector( 0, 0, bboxPlane.Max.Z ) - FVector( 0, 0, bboxPlane.Min.Z ) ).Size();

	// Figure out how large the texture is in the U and V directions

	FLOAT	MaterialUSize = 128,
			MaterialVSize = 128;

	// Compute useful U and V vectors to use for creating vertices.

	FVector UVec = SortedVtx[1] - SortedVtx[0], VVec;
	UVec.Normalize();
	UVec *= (bSubtractive ? -1 : 1);
	VVec = (UVec ^ *InNormal) * -1;

	// Generate texture coordinates for the lowest-1, lowest and lowest+1 vertices

	FVector Verts[3], UV[3];

	Verts[0] = SortedVtx[0] + (VVec * VSz);
	Verts[1] = SortedVtx[0];
	Verts[2] = SortedVtx[0] + (UVec * USz);

	UV[0] = FVector( 0,				MaterialVSize,	0 );
	UV[1] = FVector( 0,				0,				0 );
	UV[2] = FVector( MaterialUSize, 0,				0 );

	// Convert those coordinates to vectors

	FVector Base, U, V;
	FTexCoordsToVectors(
		Verts[0], UV[0],
		Verts[1], UV[1],
		Verts[2], UV[2],
		&Base, &U, &V );

	Base = bboxWorld.Min;

	// Assign the vectors to the polygon

	U *= UTile;
	V *= VTile;
	
	InSurfIdx->Surf->pBase = GEditor->bspAddPoint(InModel,&Base,0);
	InSurfIdx->Surf->vTextureU = GEditor->bspAddVector( InModel, &U, 0 );
	InSurfIdx->Surf->vTextureV = GEditor->bspAddVector( InModel, &V, 0 );

}

/*
	FRotator NormalRot = InNormal->Rotation();
	FRotator DiffRot = NormalRot * -1;

	FLOAT MaterialUSize = InPoly->Material ? InPoly->Material->MaterialUSize() : 128.f,
		MaterialVSize = InPoly->Material ? InPoly->Material->MaterialVSize() : 128.f;

	// Transform all the polys verts onto a 2D plane.  At the same time, get a worldspace and planespace bounding box.

	TArray<FVector> NewVerts;
	FBox bboxWorld(0), bboxPlane(0);

	for( INT x = 0 ; x < InPoly->NumVertices ; ++x )
	{
		bboxWorld += InPoly->Vertex[x].TransformPointBy( InSurfIdx->Surf->Actor->LocalToWorld().Coords() );

		FVector vtx = InPoly->Vertex[x].TransformPointBy( GMath.UnitCoords * DiffRot );
		bboxPlane += vtx;

		NewVerts.AddItem( vtx );
	}

	FVector UDir = InPoly->Vertex[InPoly->NumVertices-1] - InPoly->Vertex[0];
	//UDir.Normalize();
	FVector VDir = InPoly->Vertex[1] - InPoly->Vertex[0];
	//VDir.Normalize();

	FVector v0, v1, v2;
	v0 = bboxWorld.Min + (VDir * 1);
	v1 = bboxWorld.Min;
	v2 = bboxWorld.Min + (UDir * 1);

	FVector Base, U, V;
	FTexCoordsToVectors(
		v0,	FVector( 0,				0,				0),
		v1,	FVector( 0,				MaterialVSize,	0),
		v2,	FVector( MaterialUSize,	MaterialVSize,	0),
		&Base, &U, &V );

	InSurfIdx->Surf->pBase = GEditor->bspAddPoint(InModel,&Base,0);
	InSurfIdx->Surf->vTextureU = GEditor->bspAddVector( InModel, &U, 0 );
	InSurfIdx->Surf->vTextureV = GEditor->bspAddVector( InModel, &V, 0 );
*/

/*------------------------------------------------------------------------------
	FTexAlignTools.

	A helper class to store the state of the various texture alignment tools.
------------------------------------------------------------------------------*/

FTexAlignTools::FTexAlignTools()
{
}

FTexAlignTools::~FTexAlignTools()
{
}

void FTexAlignTools::Init()
{
	// Create the list of aligners.
	Aligners.Empty();
	Aligners.AddItem( CastChecked<UTexAligner>(GEditor->StaticConstructObject(UTexAlignerDefault::StaticClass(),GEditor->Level->GetOuter(),NAME_None,RF_Public|RF_Standalone) ) );
	Aligners.AddItem( CastChecked<UTexAligner>(GEditor->StaticConstructObject(UTexAlignerPlanar::StaticClass(),GEditor->Level->GetOuter(),NAME_None,RF_Public|RF_Standalone) ) );
	Aligners.AddItem( CastChecked<UTexAligner>(GEditor->StaticConstructObject(UTexAlignerBox::StaticClass(),GEditor->Level->GetOuter(),NAME_None,RF_Public|RF_Standalone) ) );
	Aligners.AddItem( CastChecked<UTexAligner>(GEditor->StaticConstructObject(UTexAlignerFace::StaticClass(),GEditor->Level->GetOuter(),NAME_None,RF_Public|RF_Standalone) ) );
	
	for( INT x = 0 ; x < Aligners.Num() ; ++x )
	{
		UTexAligner* Aligner = Aligners(x);
		check(Aligner);
		Aligner->InitFields();
	}

}

// Returns the most appropriate texture aligner based on the type passed in.
UTexAligner* FTexAlignTools::GetAligner( ETexAlign InTexAlign )
{
	switch( InTexAlign )
	{
		case TEXALIGN_Planar:
		case TEXALIGN_PlanarAuto:
		case TEXALIGN_PlanarWall:
		case TEXALIGN_PlanarFloor:
			return Aligners(1);
			break;

		case TEXALIGN_Default:
			return Aligners(0);
			break;

		case TEXALIGN_Box:
			return Aligners(2);
			break;

		case TEXALIGN_Face:
			return Aligners(3);
			break;
	}

	check(0);	// Unknown type!
	return NULL;

}

FTexAlignTools GTexAlignTools;

/*------------------------------------------------------------------------------
	The End.
------------------------------------------------------------------------------*/


