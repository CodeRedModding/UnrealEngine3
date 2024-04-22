/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineFluidClasses.h"

IMPLEMENT_CLASS(AFluidSurfaceActorMovable);
IMPLEMENT_CLASS(AFluidSurfaceActor);

void AFluidSurfaceActor::PostEditImport()
{
	Super::PostEditImport();
}

void AFluidSurfaceActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// AActor::PostEditChange will ForceUpdateComponents()
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void AFluidSurfaceActor::PostEditMove(UBOOL bFinished)
{
	Super::PostEditMove( bFinished );

	if ( bFinished )
	{
		// Propagate scale changes to the fluid vertices instead.
		FluidComponent->OnScaleChange();
		DrawScale = 1.0f;
		DrawScale3D = FVector(1.0f, 1.0f, 1.0f);
		GCallbackEvent->Send( CALLBACK_UpdateUI );
	}
}

#if WITH_EDITOR
void AFluidSurfaceActor::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	Super::EditorApplyScale( DeltaScale, ScaleMatrix, PivotLocation, bAltDown, bShiftDown, bCtrlDown );
}
#endif

void AFluidSurfaceActor::TickSpecial( FLOAT DeltaSeconds )
{
	for ( INT ActorIndex=0; ActorIndex < Touching.Num(); ++ActorIndex )
	{
		AActor* Actor = Touching( ActorIndex );
		if (Actor && Actor->bAllowFluidSurfaceInteraction)
		{
			FLOAT ActorVelocity = Actor->Velocity.Size();
			if ( ActorVelocity > KINDA_SMALL_NUMBER )
			{
				if(Actor->CollisionComponent)
				{
					FLOAT Radius = Actor->CollisionComponent->Bounds.SphereRadius;
					FluidComponent->ApplyForce( Actor->Location, FluidComponent->ForceContinuous, Radius*0.3f, FALSE );
				}
			}
		}
	}
}
