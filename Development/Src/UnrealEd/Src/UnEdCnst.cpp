/*=============================================================================
	UnEdCnst.cpp: Editor movement contraints.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"

/*------------------------------------------------------------------------------
	Functions.
------------------------------------------------------------------------------*/

FLOAT FEditorConstraints::GetGridSize()
{
	return GridSizes[ CurrentGridSz ];
}

void FEditorConstraints::SetGridSz( INT InIndex )
{
	GEditor->FinishAllSnaps();

	CurrentGridSz = InIndex;
	CurrentGridSz = Clamp<INT>( CurrentGridSz, 0, MAX_GRID_SIZES-1 );

	GEditor->RedrawLevelEditingViewports();
	GCallbackEvent->Send( CALLBACK_UpdateUI );
}

void FEditorConstraints::RotGridSizeIncrement( )
{
	SetRotGridSize( RotGridSize.Pitch * 2 );
}

void FEditorConstraints::RotGridSizeDecrement( )
{
	SetRotGridSize( RotGridSize.Pitch / 2 );
}

void FEditorConstraints::SetRotGridSize( INT InAngle )
{
	GEditor->FinishAllSnaps();

	InAngle = Clamp<INT>( InAngle, MIN_ROT_ANGLE, MAX_ROT_ANGLE );
	GUnrealEd->Exec( *FString::Printf( TEXT("MAP ROTGRID PITCH=%d YAW=%d ROLL=%d"), InAngle, InAngle, InAngle ) );

	GEditor->RedrawLevelEditingViewports();
	GCallbackEvent->Send( CALLBACK_UpdateUI );
}

void FEditorConstraints::Snap( FVector& Point, const FVector& GridBase )
{
	if( GridEnabled )
	{
		Point = (Point - GridBase).GridSnap( GetGridSize() ) + GridBase;
	}
}

void FEditorConstraints::SnapScale( FVector& Point, const FVector& GridBase )
{
	if( SnapScaleEnabled )
	{
		Point = (Point - GridBase).GridSnap( GetGridSize() ) + GridBase;
	}
}

void FEditorConstraints::Snap( FRotator& Rotation )
{
	if( RotGridEnabled )
	{
		Rotation = Rotation.GridSnap( RotGridSize );
	}
}
UBOOL FEditorConstraints::Snap( FVector& Location, FVector GridBase, FRotator& Rotation )
{
	UBOOL Snapped = 0;
	Snap( Rotation );
	if( SnapVertices )
	{
		FVector	DestPoint;
		INT Temp;
		if( GWorld->GetModel()->FindNearestVertex( Location, DestPoint, SnapDistance, Temp ) >= 0.0)
		{
			Location = DestPoint;
			Snapped = 1;
		}
	}
	if( !Snapped )
		Snap( Location, GridBase );
	return Snapped;
}


