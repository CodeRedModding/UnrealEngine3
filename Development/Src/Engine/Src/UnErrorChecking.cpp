/*
UnErrorChecking.cpp
Actor Error checking functions
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Steven Polge
=============================================================================*/

#include "EnginePrivate.h"

void APhysicsVolume::CheckForErrors()
{
	if ( Gravity.Z == 0.f )
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("Gravity must not be zero!"));

	Super::CheckForErrors();
}

void ALevelInfo::CheckForErrors()
{
	if ( GetLevel()->GetLevelInfo() != this )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("Duplicate level info!") ) );
	else
	{
	if ( !bPathsRebuilt )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("Paths need to be rebuilt!") ) );
	//if ( !Screenshot )
	//	GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("No screenshot for this level!") ) );
	if ( Title.Len() == 0 )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("No title for this level!") ) );
}

}

void AActor::CheckForErrors()
{
	if ( GetClass()->ClassFlags & CLASS_Deprecated )
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("%s is obsolete and must be removed!!!"), GetName() ) );
		return;
	}
	if ( GetClass()->GetDefaultActor()->bStatic && !bStatic )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("%s bStatic false, but is bStatic by default - map will fail in netplay"), GetName() ) );
	if ( GetClass()->GetDefaultActor()->bNoDelete && !bNoDelete )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("%s bNoDelete false, but is bNoDelete by default - map will fail in netplay"), GetName() ) );

	if ( AttachTag != NAME_None )
	{
		INT bSuccess = 0;

		// make sure actor with matching tag for all events
		for ( INT i=0; i<GetLevel()->Actors.Num(); i++)
		{
			AActor *A = GetLevel()->Actors(i);
			if ( A && !A->bDeleteMe && ((A->Tag == AttachTag) || (A->GetFName() == AttachTag)) )
			{
				bSuccess = 1;
				break;
			}
		}
		if ( !bSuccess )
			GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("No Actor with Tag or Name corresponding to this Actor's AttachTag"));
	}

	// check if placed in same location as another actor of same class
	if ( GetClass()->ClassFlags & CLASS_Placeable )
		for ( INT i=0; i<GetLevel()->Actors.Num(); i++)
		{
#if VIEWPORT_ACTOR_DISABLED
			AActor *A = GetLevel()->Actors(i);
			if ( A && (A != this) && ((A->Location - Location).SizeSquared() < 1.f) && (A->GetClass() == GetClass()) && (A->Rotation == Rotation)
				&& (A->DrawType == DrawType) && (A->DrawScale3D == DrawScale3D) && ((DrawType != DT_StaticMesh) || (StaticMesh == A->StaticMesh)) )
				GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("%s in same location as %s"), GetName(), A->GetName() ) );
#endif
		}

	if ( !bStatic && (Mass == 0.f) )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("%s mass must be greater than zero!"), GetName() ) );

}

void ANote::CheckForErrors()
{
	GWarn->MapCheck_Add( MCTYPE_NOTE, this, *Text );

	AActor::CheckForErrors();

}

void ABrush::CheckForErrors()
{
	check(BrushComponent);

	// NOTE : don't report NULL texture references on the builder brueh - it doesn't matter there
	// NOTE : don't check volume brushes since those are forced to have the NULL texture on all polys
	if( Brush
			&& this != GetLevel()->Brush()
			&& !IsVolumeBrush() )
	{
		// Check for missing textures
		for( INT x = 0 ; x < Brush->Polys->Element.Num() ; ++x )
		{
			FPoly* Poly = &(Brush->Polys->Element(x));

			if( !Poly->Material )
			{
				GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("%s : Brush has NULL material reference(s)"), GetName() ) );
				break;
			}
		}

		// Check for non-coplanar polygons.
		for( INT x = 0 ; x < Brush->Polys->Element.Num() ; ++x )
		{
			FPoly* Poly = &(Brush->Polys->Element(x));
			UBOOL	Coplanar = 1;

			for(INT VertexIndex = 0;VertexIndex < Poly->NumVertices;VertexIndex++)
			{
				if(Abs(FPlane(Poly->Vertex[0],Poly->Normal).PlaneDot(Poly->Vertex[VertexIndex])) > THRESH_POINT_ON_PLANE)
				{
					GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("%s : Brush has non-coplanar polygons"), GetName() ) );
					Coplanar = 0;
					break;
				}
			}

			if(!Coplanar)
				break;
		}
	}

	AActor::CheckForErrors();

}

void APawn::CheckForErrors()
{
	AActor::CheckForErrors();
}

void ANavigationPoint::CheckForErrors()
{
	AActor::CheckForErrors();

	if( !GetLevel()->GetLevelInfo()->bHasPathNodes )
			return;

	if ( !bDestinationOnly && (PathList.Num() == 0) )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("No paths from %s"), GetName() ) );

	if ( ExtraCost < 0 )
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("Extra Cost cannot be less than zero!") );

	if ( Location.Z > GetLevel()->GetLevelInfo()->StallZ )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, TEXT("NavigationPoint above level's StallZ!") );
}
