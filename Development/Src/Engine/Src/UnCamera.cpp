/*=============================================================================
	UnCamera.cpp: Unreal Engine Camera Actor implementation
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Laurent Delayen
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(ACamera);
IMPLEMENT_CLASS(ACameraActor);


/*------------------------------------------------------------------------------
	ACamera
------------------------------------------------------------------------------*/

/* SetViewTarget
	Set a new ViewTarget with optional BlendTime */
void ACamera::execSetViewTarget( FFrame& Stack, RESULT_DECL )
{
	P_GET_ACTOR(NewViewTarget);
	P_FINISH;

	SetViewTarget(NewViewTarget);
}

void ACamera::SetViewTarget( AActor* NewViewTarget )
{
	// Make sure we don't set a stupid view target
	if( NewViewTarget == NULL )
	{
		NewViewTarget = PCOwner;
	}

	// Update current ViewTarget
	CheckViewTarget( &primaryVT );

	// if different then new one, then assign it
	if( primaryVT.Target != NewViewTarget )
	{
		// Force a camera update
		CameraCache.LastTimeStamp = 0.f;

		// Set TargetController if viewing a Controller other than the LocalPlayer
		// This allows us spectating a Player's many Pawns (through Possess() or deaths)
		if( primaryVT.Target == PCOwner )
		{
			primaryVT.TargetController = NULL;
		}
		else
		{
			primaryVT.TargetController = Cast<AController>(primaryVT.Target);
		}

		primaryVT.Target = NewViewTarget;
		CheckViewTarget( &primaryVT );
		primaryVT.Target->eventBecomeViewTarget( PCOwner );

		if ( primaryVT.Target == PCOwner )
		{
			primaryVT.TargetController = NULL;
		}
	}
}

/* CheckViewTarget: 
	Make sure ViewTarget is valid */
void ACamera::CheckViewTarget( FsViewTarget* VT )
{
	// If viewing a controller other than the Local Player, update it
	if( VT->TargetController && !VT->TargetController->bDeleteMe )
	{
		AActor *OldViewTarget = VT->Target;
		if( VT->TargetController->Pawn )
		{
			if( VT->TargetController->Pawn->bDeleteMe )
			{
				VT->Target = VT->TargetController;
			}
			else
			{
				VT->Target = VT->TargetController->Pawn;
			}
		}
		else if( VT->TargetController->GetAPlayerController() && VT->TargetController->GetAPlayerController()->ViewTarget )
		{
			VT->Target = VT->TargetController->GetAPlayerController()->ViewTarget;
		}
		else if( !VT->Target )
		{
			VT->Target = VT->TargetController;
		}

		if( (OldViewTarget != VT->Target) && !PCOwner->LocalPlayerController() )
		{
			PCOwner->eventClientSetViewTarget( VT->Target );
		}
	}

	if( !VT->Target || VT->Target->bDeleteMe )
	{
		if( PCOwner->Pawn && !PCOwner->Pawn->bDeleteMe && !PCOwner->Pawn->bPendingDelete )
		{
			VT->Target = PCOwner->Pawn;
		}
		else
		{
			VT->Target = PCOwner;
		}
	}

	// Update PlayerController
	PCOwner->ViewTarget		= VT->Target;
	PCOwner->RealViewTarget	= VT->TargetController;
}

/* GetViewTarget: 
	Returns current ViewTarget */
AActor* ACamera::GetViewTarget()
{
	CheckViewTarget( &primaryVT );

	return primaryVT.Target;
}

/* execFillCameraCache: 
	Fill Camera Cache, for optimization */
void ACamera::execFillCameraCache( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(NewCamLoc);
	P_GET_ROTATOR(NewCamRot);
	P_FINISH;

	// Cache Camera
	CameraCache.LastTimeStamp	= Level->TimeSeconds;
	CameraCache.LastLocation	= NewCamLoc;
	CameraCache.LastRotation	= NewCamRot;

	// Cache View Target
	FVector		POVLoc;
	FRotator	POVRot;
	if ( primaryVT.Target )
	{
		PCOwner->ViewTarget = primaryVT.Target;
		primaryVT.Target->eventGetActorEyesViewPoint( POVLoc, POVRot);
		primaryVT.LastLocation = POVLoc;
		primaryVT.LastRotation = POVRot;
	}

}

/*------------------------------------------------------------------------------
	ACameraActor
------------------------------------------------------------------------------*/

/** 
 *	Use to assign the camera static mesh to the CameraActor used in the editor.
 *	Beacuse HiddenGame is true and CollideActors is false, the component should be NULL in-game.
 */
void ACameraActor::Spawned()
{
	Super::Spawned();

	if(MeshComp)
	{
		if( !MeshComp->StaticMesh)
		{
			UStaticMesh* CamMesh = LoadObject<UStaticMesh>(NULL, TEXT("EditorMeshes.MatineeCam_SM"), NULL, LOAD_NoFail, NULL);

			if(MeshComp->Initialized)
			{
				MeshComp->Destroyed();
			}

			MeshComp->StaticMesh = CamMesh;

			if(MeshComp->IsValidComponent())
			{
				MeshComp->Created();
			}
		}
	}

	// Sync component with CameraActor frustum settings.
	UpdateDrawFrustum();
}

/** Used to synchronise the DrawFrustumComponent with the CameraActor settings. */
void ACameraActor::UpdateDrawFrustum()
{
	if(DrawFrustum)
	{
		DrawFrustum->FrustumAngle = FOVAngle;
		DrawFrustum->FrustumStartDist = 10.f;
		DrawFrustum->FrustumEndDist = 1000.f;
	}
}


void ACameraActor::PostEditChange(UProperty* PropertyThatChanged)
{
	// Update the 
	UpdateDrawFrustum();

	Super::PostEditChange(PropertyThatChanged);
}
