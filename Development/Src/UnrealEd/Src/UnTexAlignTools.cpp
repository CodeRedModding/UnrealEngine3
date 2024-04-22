/*=============================================================================
	UnTexAlignTools.cpp: Tools for aligning textures on surfaces
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnTexAlignTools.h"
#include "BSPOps.h"

FTexAlignTools GTexAlignTools;

static INT GetMajorAxis( FVector InNormal, INT InForceAxis )
{
	// Figure out the major axis information.
	INT Axis = TAXIS_X;
	if( Abs(InNormal.Y) >= 0.5f ) Axis = TAXIS_Y;
	else 
	{
		// Only check Z if we aren't aligned to walls
		if( InForceAxis != TAXIS_WALLS )
			if( Abs(InNormal.Z) >= 0.5f ) Axis = TAXIS_Z;
	}

	return Axis;

}

// Checks the normal of the major axis ... if it's negative, returns 1.
static UBOOL ShouldFlipVectors( FVector InNormal, INT InAxis )
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
	TAxis = TAXIS_AUTO;
	UTile = VTile = 1.f;
	DefTexAlign = TEXALIGN_Default;
}

/**
 * Perform any static initialization, such as adding UProperties to the class (for intrinsic classes), or initializing static variable values.
 * It is not recommended to use this function to initialize member property values, since those values will be overwritten when the class's default
 * object is re-initialized against its archetype.  Use InitializeInstrinsicPropertyValues instead.
 */
void UTexAligner::StaticConstructor()
{
	TArray<FName> EnumNames;
	EnumNames.AddItem( FName( TEXT("TAXIS_X") ) );
	EnumNames.AddItem( FName( TEXT("TAXIS_Y") ) );
	EnumNames.AddItem( FName( TEXT("TAXIS_Z") ) );
	EnumNames.AddItem( FName( TEXT("TAXIS_WALLS") ) );
	EnumNames.AddItem( FName( TEXT("TAXIS_AUTO") ) );

	TAxisEnum = new( GetClass(), TEXT("TAxis") ) UEnum();
	TAxisEnum->SetEnums( EnumNames );

	new(GetClass(),TEXT("VTile"),	RF_Public)UFloatProperty	(CPP_PROPERTY(VTile),	TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("UTile"),	RF_Public)UFloatProperty	(CPP_PROPERTY(UTile),	TEXT(""), CPF_Edit );

}

void UTexAligner::Align( ETexAlign InTexAlignType )
{
	for( INT LevelIndex = 0; LevelIndex < GWorld->Levels.Num(); ++LevelIndex )
	{
		ULevel* Level = GWorld->Levels(LevelIndex);
		Align( InTexAlignType, Level->Model );
	}
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

	GEditor->RedrawLevelEditingViewports();

	GWorld->MarkPackageDirty();
	GCallbackEvent->Send( CALLBACK_LevelDirtied );
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
	Desc = *LocalizeUnrealEd("Planar");
	DefTexAlign = TEXALIGN_Planar;
}

/**
 * Perform any static initialization, such as adding UProperties to the class (for intrinsic classes), or initializing static variable values.
 * It is not recommended to use this function to initialize member property values, since those values will be overwritten when the class's default
 * object is re-initialized against its archetype.  Use InitializeInstrinsicPropertyValues instead.
 */
void UTexAlignerPlanar::StaticConstructor()
{
	new(GetClass()->HideCategories) FName(NAME_Object);
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

	InSurfIdx->Surf->pBase = FBSPOps::bspAddPoint(InModel,&Base,0);
	InSurfIdx->Surf->vTextureU = FBSPOps::bspAddVector( InModel, &U, 0);
	InSurfIdx->Surf->vTextureV = FBSPOps::bspAddVector( InModel, &V, 0);

}

/*------------------------------------------------------------------------------
	UTexAlignerDefault

	Aligns to a default setting.
------------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UTexAlignerDefault);

UTexAlignerDefault::UTexAlignerDefault()
{
	Desc = *LocalizeUnrealEd("Default");
	DefTexAlign = TEXALIGN_Default;
}

/**
 * Perform any static initialization, such as adding UProperties to the class (for intrinsic classes), or initializing static variable values.
 * It is not recommended to use this function to initialize member property values, since those values will be overwritten when the class's default
 * object is re-initialized against its archetype.  Use InitializeInstrinsicPropertyValues instead.
 */
void UTexAlignerDefault::StaticConstructor()
{
	new(GetClass()->HideCategories) FName(NAME_Object);
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

	InSurfIdx->Surf->vTextureU = FBSPOps::bspAddVector( InModel, &InPoly->TextureU, 0);
	InSurfIdx->Surf->vTextureV = FBSPOps::bspAddVector( InModel, &InPoly->TextureV, 0);

}

/*------------------------------------------------------------------------------
	UTexAlignerBox

	Aligns to the best U and V axis according to the polys normal.
------------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UTexAlignerBox);

UTexAlignerBox::UTexAlignerBox()
{
	Desc = *LocalizeUnrealEd("Box");
	DefTexAlign = TEXALIGN_Box;
}

/**
 * Perform any static initialization, such as adding UProperties to the class (for intrinsic classes), or initializing static variable values.
 * It is not recommended to use this function to initialize member property values, since those values will be overwritten when the class's default
 * object is re-initialized against its archetype.  Use InitializeInstrinsicPropertyValues instead.
 */
void UTexAlignerBox::StaticConstructor()
{
	new(GetClass()->HideCategories) FName(NAME_Object);
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

	InSurfIdx->Surf->pBase = FBSPOps::bspAddPoint(InModel,&Base,0);
	InSurfIdx->Surf->vTextureU = FBSPOps::bspAddVector( InModel, &U, 0 );
	InSurfIdx->Surf->vTextureV = FBSPOps::bspAddVector( InModel, &V, 0 );

}


/*------------------------------------------------------------------------------
	UTexAlignerFit
------------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UTexAlignerFit);

UTexAlignerFit::UTexAlignerFit()
{
	Desc = *LocalizeUnrealEd("Fit");
	DefTexAlign = TEXALIGN_Fit;
}

/**
 * Perform any static initialization, such as adding UProperties to the class (for intrinsic classes), or initializing static variable values.
 * It is not recommended to use this function to initialize member property values, since those values will be overwritten when the class's default
 * object is re-initialized against its archetype.  Use InitializeInstrinsicPropertyValues instead.
 */
void UTexAlignerFit::StaticConstructor()
{
	new(GetClass()->HideCategories) FName(NAME_Object);
}

void UTexAlignerFit::AlignSurf( ETexAlign InTexAlignType, UModel* InModel, FBspSurfIdx* InSurfIdx, FPoly* InPoly, FVector* InNormal )
{
	// @todo: Support cycling between texture corners by FIT'ing again?  Each Ctrl+Shift+F would rotate texture.
	// @todo: Consider making initial FIT match the texture's current orientation as close as possible?
	// @todo: Handle subtractive brush polys differently?  (flip U texture direction)
	// @todo: Option to ignore pixel aspect for quads (e.g. stretch full texture non-uniformly over quad)


	// Compute world space vertex positions
	TArray< FVector > WorldSpacePolyVertices;
	for( INT VertexIndex = 0; VertexIndex < InPoly->Vertices.Num(); ++VertexIndex )
	{
		WorldSpacePolyVertices.AddItem( InSurfIdx->Surf->Actor->LocalToWorld().TransformFVector( InPoly->Vertices( VertexIndex ) ) );
	}

			
	// Create an orthonormal basis for the polygon
	FMatrix WorldToPolyRotationMatrix;
	const FVector& FirstPolyVertex = WorldSpacePolyVertices( 0 );
	{
		const FVector& VertexA = FirstPolyVertex;
		const FVector& VertexB = WorldSpacePolyVertices( 1 );
		FVector UpVec = ( VertexB - VertexA ).SafeNormal();
		FVector RightVec = InPoly->Normal ^ UpVec;
		WorldToPolyRotationMatrix.SetIdentity();
		WorldToPolyRotationMatrix.SetAxes( &RightVec, &UpVec, &InPoly->Normal );
	}


	// Find a corner of the polygon that's closest to a 90 degree angle.  When there are multiple corners with
	// similar angles, we'll use the one closest to the local space bottom-left along the polygon's plane
	const FLOAT DesiredAbsDotProduct = 0.0f;
	INT BestVertexIndex = INDEX_NONE;
	FLOAT BestDotProductDiff = 10000.0f;
	FLOAT BestPositivity = 10000.0f;
	for( INT VertexIndex = 0; VertexIndex < WorldSpacePolyVertices.Num(); ++VertexIndex )
	{
		// Compute the previous and next vertex in the winding
		const INT PrevWindingVertexIndex = ( VertexIndex > 0 ) ? ( VertexIndex - 1 ) : ( WorldSpacePolyVertices.Num() - 1 );
		const INT NextWindingVertexIndex = ( VertexIndex < WorldSpacePolyVertices.Num() - 1 ) ? ( VertexIndex + 1 ) : 0;

		const FVector& PrevVertex = WorldSpacePolyVertices( PrevWindingVertexIndex );
		const FVector& CurVertex = WorldSpacePolyVertices( VertexIndex );
		const FVector& NextVertex = WorldSpacePolyVertices( NextWindingVertexIndex );

		// Compute the corner angle
		FLOAT AbsDotProduct = Abs( ( PrevVertex - CurVertex ).SafeNormal() | ( NextVertex - CurVertex ).SafeNormal() );

		// Compute how 'positive' this vertex is relative to the bottom left position in the polygon's plane
		FVector PolySpaceVertex = WorldToPolyRotationMatrix.InverseTransformNormal( CurVertex - FirstPolyVertex );
		const FLOAT Positivity = PolySpaceVertex.X + PolySpaceVertex.Y;

		// Is the corner angle closer to 90 degrees than our current best?
		const FLOAT DotProductDiff = Abs( AbsDotProduct - DesiredAbsDotProduct );
		if( appIsNearlyEqual( DotProductDiff, BestDotProductDiff, 0.1f ) )
		{
			// This angle is just as good as the current best, so check to see which is closer to the local space
			// bottom-left along the polygon's plane
			if( Positivity < BestPositivity )
			{
				// This vertex is in a more suitable location for the bottom-left of the texture
				BestVertexIndex = VertexIndex;
				if( DotProductDiff < BestDotProductDiff )
				{
					// Only store the new dot product if it's actually better than the existing one
					BestDotProductDiff = DotProductDiff;
				}
				BestPositivity = Positivity;
			}
		}
		else if( DotProductDiff <= BestDotProductDiff )
		{
			// This angle is definitely better!
			BestVertexIndex = VertexIndex;
			BestDotProductDiff = DotProductDiff;
			BestPositivity = Positivity;
		}
	}


	// Compute orthonormal basis for the 'best corner' of the polygon.  The texture will be positioned at the corner
	// of the bounds of the poly in this coordinate system
	const FVector& BestVertex = WorldSpacePolyVertices( BestVertexIndex );
	const INT NextWindingVertexIndex = ( BestVertexIndex < WorldSpacePolyVertices.Num() - 1 ) ? ( BestVertexIndex + 1 ) : 0;
	const FVector& NextVertex = WorldSpacePolyVertices( NextWindingVertexIndex );

	FVector TextureUpVec = ( NextVertex - BestVertex ).SafeNormal();
	FVector TextureRightVec = InPoly->Normal ^ TextureUpVec;

	FMatrix WorldToTextureRotationMatrix;
	WorldToTextureRotationMatrix.SetIdentity();
	WorldToTextureRotationMatrix.SetAxes( &TextureRightVec, &TextureUpVec, &InPoly->Normal );


	// Compute bounds of polygon along plane
	FLOAT MinX = FLT_MAX;
	FLOAT MaxX = -FLT_MAX;
	FLOAT MinY = FLT_MAX;
	FLOAT MaxY = -FLT_MAX;
	for( INT VertexIndex = 0; VertexIndex < WorldSpacePolyVertices.Num(); ++VertexIndex )
	{
		const FVector& CurVertex = WorldSpacePolyVertices( VertexIndex );

		// Transform vertex into the coordinate system of our texture
		FVector TextureSpaceVertex = WorldToTextureRotationMatrix.InverseTransformNormal( CurVertex - BestVertex );

		if( TextureSpaceVertex.X < MinX )
		{
			MinX = TextureSpaceVertex.X;
		}
		if( TextureSpaceVertex.X > MaxX )
		{
			MaxX = TextureSpaceVertex.X;
		}

		if( TextureSpaceVertex.Y < MinY )
		{
			MinY = TextureSpaceVertex.Y;
		}
		if( TextureSpaceVertex.Y > MaxY )
		{
			MaxY = TextureSpaceVertex.Y;
		}
	}


	// We'll use the texture space corner of the bounds as the origin of the texture.  This ensures that
	// the texture fits over the entire polygon without revealing any tiling
	const FVector TextureSpaceBasePos( MinX, MinY, 0.0f );
	FVector WorldSpaceBasePos = WorldToTextureRotationMatrix.TransformNormal( TextureSpaceBasePos ) + BestVertex;


	// Apply scale to UV vectors.  We incorporate the parameterized tiling rations and scale by our texture size
	const FLOAT WorldTexelScale = 128.0f;
	const FLOAT TextureSizeU = Abs( MaxX - MinX );
	const FLOAT TextureSizeV = Abs( MaxY - MinY );
	FVector TextureUVector = UTile * TextureRightVec * WorldTexelScale / TextureSizeU;
	FVector TextureVVector = VTile * TextureUpVec * WorldTexelScale / TextureSizeV;

	// Flip the texture vertically if we want that
	const UBOOL bFlipVertically = TRUE;
	if( bFlipVertically )
	{
		WorldSpaceBasePos += TextureUpVec * TextureSizeV;
		TextureVVector *= -1.0f;
	}


	// Apply texture base position
	{
		const UBOOL bExactMatch = FALSE;
		InSurfIdx->Surf->pBase = FBSPOps::bspAddPoint( InModel, const_cast< FVector* >( &WorldSpaceBasePos ), bExactMatch );
	}

	// Apply texture UV vectors
	{
		const UBOOL bExactMatch = FALSE;
		InSurfIdx->Surf->vTextureU = FBSPOps::bspAddVector( InModel, const_cast< FVector* >( &TextureUVector ), bExactMatch );
		InSurfIdx->Surf->vTextureV = FBSPOps::bspAddVector( InModel, const_cast< FVector* >( &TextureVVector ), bExactMatch );
	}
}



/*------------------------------------------------------------------------------
	FTexAlignTools.

	A helper class to store the state of the various texture alignment tools.
------------------------------------------------------------------------------*/

void FTexAlignTools::Init()
{
	// Create the list of aligners.
	Aligners.Empty();
	Aligners.AddItem( CastChecked<UTexAligner>(UObject::StaticConstructObject(UTexAlignerDefault::StaticClass(),UObject::GetTransientPackage(),NAME_None,RF_Public|RF_Standalone) ) );
	Aligners.AddItem( CastChecked<UTexAligner>(UObject::StaticConstructObject(UTexAlignerPlanar::StaticClass(),UObject::GetTransientPackage(),NAME_None,RF_Public|RF_Standalone) ) );
	Aligners.AddItem( CastChecked<UTexAligner>(UObject::StaticConstructObject(UTexAlignerBox::StaticClass(),UObject::GetTransientPackage(),NAME_None,RF_Public|RF_Standalone) ) );
	Aligners.AddItem( CastChecked<UTexAligner>(UObject::StaticConstructObject(UTexAlignerFit::StaticClass(),UObject::GetTransientPackage(),NAME_None,RF_Public|RF_Standalone) ) );
	
	GCallbackEvent->Register( CALLBACK_FitTextureToSurface, this );
}


FTexAlignTools::FTexAlignTools()
{
}


FTexAlignTools::~FTexAlignTools()
{
	if(GCallbackEvent)
	{
		GCallbackEvent->UnregisterAll( this );
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

		case TEXALIGN_Fit:
			return Aligners(3);
			break;
	}

	check(0);	// Unknown type!
	return NULL;

}


/**
 * Routes the event to the appropriate handlers
 *
 * @param InType the event that was fired
 */
void FTexAlignTools::Send( ECallbackEventType InType )
{
	switch( InType )
	{
		case CALLBACK_FitTextureToSurface:
			{
				UTexAligner* FitAligner = GTexAlignTools.Aligners( 3 );
				for ( INT LevelIndex = 0; LevelIndex < GWorld->Levels.Num() ; ++LevelIndex )
				{
					ULevel* Level = GWorld->Levels(LevelIndex);
					FitAligner->Align( TEXALIGN_None, Level->Model );
				}
			}
			break;
	}
}

