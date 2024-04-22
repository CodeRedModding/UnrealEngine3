/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineDecalClasses.h"

IMPLEMENT_CLASS(ADecalActorBase);
IMPLEMENT_CLASS(ADecalActor);
IMPLEMENT_CLASS(ADecalActorMovable);

void ADecalActorBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// AActor::PostEditChange will ForceUpdateComponents()
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (Decal != NULL)
	{
		// Copy over the actor's location and orientation to the decal.
		FComponentReattachContext ReattachContext( Decal );
		Decal->Location = Location;
		Decal->Orientation = Rotation;
	}
}

/**
 * Whenever the decal actor has moved:
 *  - Copy the actor rot/pos info over to the decal component
 *  - Trigger updates on the decal component to recompute its matrices and generate new geometry.
 */
void ADecalActorBase::PostEditMove(UBOOL bFinished)
{
	Super::PostEditMove( bFinished );

	if (Decal != NULL)
	{
		// Copy over the actor's location and orientation to the decal.
		FComponentReattachContext ReattachContext( Decal );
		Decal->Location = Location;
		Decal->Orientation = Rotation;
	}
}

/** 
  * EditorLinkSelectionInterface 
  */
void ADecalActorBase::LinkSelection(USelection* SelectedActors)
{
	if (Decal != NULL)
	{
		// If component was modified, update it
		if ( Decal->LinkSelection(SelectedActors) )
		{
			ForceUpdateComponents( FALSE, FALSE );
		}
	}
}

void ADecalActorBase::UnLinkSelection(USelection* SelectedActors)
{
	if (Decal != NULL)
	{
		// If component was modified, update it
		if ( Decal->UnLinkSelection(SelectedActors) )
		{
			ForceUpdateComponents( FALSE, FALSE );
		}
	}
}

#if WITH_EDITOR
void ADecalActorBase::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	if (Decal != NULL)
	{
		const FVector ModifiedScale = DeltaScale * 500.0f;

		// make sure decal component is serialized to the undo transaction buffer
		Decal->Modify();

		if ( bCtrlDown )
		{
			Decal->NearPlane += ModifiedScale.X;
			Decal->NearPlane = Max( 0.f, Min( Decal->NearPlane, Decal->FarPlane ) );

			Decal->TileX += ModifiedScale.Y * 0.001f;
			Decal->TileY += ModifiedScale.Z * 0.001f;
		}
		else if ( bAltDown )
		{
			Decal->FarPlane += ModifiedScale.X;
			Decal->NearPlane += ModifiedScale.X;
			Decal->NearPlane = Max( 0.f, Min( Decal->NearPlane, Decal->FarPlane ) );
			Decal->FarPlane = Max( 0.f, Decal->FarPlane );
			Decal->OffsetX += -ModifiedScale.Y * 0.0005f;
			Decal->OffsetY += -ModifiedScale.Z * 0.0005f;
		}
		else
		{
			Decal->FarPlane += ModifiedScale.X;
			Decal->FarPlane = Max( Decal->NearPlane, Decal->FarPlane );

			Decal->Width += ModifiedScale.Y;
			Decal->Width = Max( 0.0f, Decal->Width );

			Decal->Height += ModifiedScale.Z;
			Decal->Height = Max( 0.0f, Decal->Height );
		}
	}
}

void ADecalActorBase::CheckForErrors()
{
	Super::CheckForErrors();
	if( Decal == NULL )
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_DecalComponentNull" ), *GetName() ) ), TEXT( "DecalComponentNull" ) );
	}
	else
	{
		// only do this test for non movable Decals.  Because the moving ones we can't tell where they will move ^^
		if( bMovable == FALSE )
		{
			// if the Decal is valid but it isn't touching anything show a map check warning
			if ( Decal->DecalReceivers.Num() <= 0 )
			{
				GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_DecalComponentNoReceivers" ), *GetName() ) ), TEXT( "DecalComponentNoReceivers" ) );
			}
		}

	}
}
#endif
