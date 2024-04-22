/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "GameFramework.h"

#include "GameFrameworkSpecialMovesClasses.h"

IMPLEMENT_CLASS(UGameSpecialMove);

void UGameSpecialMove::PrePerformPhysics(FLOAT DeltaTime)
{
	// Below, we require a locally controlled Pawn, unless bForcePrecisePosition.
	if( !PawnOwner || (!bForcePrecisePosition && !PawnOwner->IsLocallyControlled()) )
	{
		return;
	}

	// 	debugf(TEXT("UGearSpecialMove::PrePerformPhysics"));

	// Reach for a precise destination
	// Controller.bPreciseDestination is not precise enough, so we have this alternate method to get as close as possible to destination.
	if( bReachPreciseDestination && !bReachedPreciseDestination )
	{
		// Update Precise Destination if we're based on a mover
		if( PreciseDestBase )
		{
			PreciseDestination = PreciseDestBase->Location + RelativeToWorldOffset(PreciseDestBase->Rotation, PreciseDestRelOffset);
		}

		// Distance to Destination
		const FLOAT Distance = (PreciseDestination - PawnOwner->Location).Size2D();

		if( Abs(Distance) > 1.f )
		{
			// Push Pawn at full speed
			const FLOAT		MaxSpeedModifier	= PawnOwner->MaxSpeedModifier();
			const FVector	Direction			= (PreciseDestination - PawnOwner->Location).SafeNormal2D();
			const FLOAT PushMagnitude = Min( (Distance / DeltaTime), PawnOwner->GroundSpeed * MaxSpeedModifier);
			PawnOwner->Velocity = Direction * PushMagnitude;
			PawnOwner->Acceleration	= (PawnOwner->Velocity / DeltaTime).SafeNormal();

			//  			debugf(TEXT("Distance: %f, Vect %s, Vel: %s, Accel: %s, DeltaTime: %f"), Distance, *(PreciseDestination - PawnOwner->Location).ToString(), *PawnOwner->Velocity.ToString(), *PawnOwner->Acceleration.ToString(), DeltaTime );
		}
		else
		{
			// PawnOwner is close enough, consider position reached
			PawnOwner->Velocity			= FVector(0.f);
			PawnOwner->Acceleration		= FVector(0.f);
			bReachedPreciseDestination	= TRUE;
		}
	}

	// Precise Rotation interpolation
	if( bReachPreciseRotation && !bReachedPreciseRotation )
	{
		FRotator NewRotation = PawnOwner->Rotation;

		if( PreciseRotationInterpolationTime > DeltaTime )
		{
			// Delta rotation
			const FRotator RotDelta	= (PreciseRotation.GetNormalized() - PawnOwner->Rotation.GetNormalized()).GetNormalized();
			NewRotation.Yaw = (PawnOwner->Rotation + RotDelta * (DeltaTime / PreciseRotationInterpolationTime)).GetNormalized().Yaw;
			PreciseRotationInterpolationTime -= DeltaTime;
		}
		else
		{
			NewRotation.Yaw			= PreciseRotation.Yaw;
			bReachedPreciseRotation	= TRUE;
		}

		ForcePawnRotation(PawnOwner, NewRotation);
	}

	// Send event once Pawn has reached precise position
	if( bReachedPreciseRotation || bReachedPreciseDestination )
	{
		UBOOL bDelay = FALSE;

		if( bReachPreciseDestination && !bReachedPreciseDestination )
		{
			bDelay = TRUE;
		}

		if( bReachPreciseRotation && !bReachedPreciseRotation )
		{
			bDelay = TRUE;
		}

		if( !bDelay )
		{
			bReachPreciseRotation		= FALSE;
			bReachedPreciseRotation		= FALSE;
			bReachPreciseDestination	= FALSE;
			bReachedPreciseDestination	= FALSE;
			eventReachedPrecisePosition();
		}
	}
}

void UGameSpecialMove::PostProcessPhysics(FLOAT DeltaTime)
{
}

void UGameSpecialMove::TickSpecialMove(FLOAT DeltaTime)
{
}

void UGameSpecialMove::SetReachPreciseDestination(FVector DestinationToReach, UBOOL bCancel)
{
	// Cancel Precise Destination move
	if( bCancel )
	{
		bReachPreciseDestination	= FALSE;
		bReachedPreciseDestination	= FALSE;
		PreciseDestBase				= NULL;
	}
	// Start a new Precise Destination move
	else
	{
		PreciseDestination			= DestinationToReach;
		bReachPreciseDestination	= TRUE;
		bReachedPreciseDestination	= FALSE;

		// If PawnOwner is based on something that is not world geometry, then make destination relative to that.
		// For cases when PawnOwner is on a moving vehicle
		if( PawnOwner->Base && !PawnOwner->Base->bWorldGeometry )
		{
			PreciseDestBase = PawnOwner->Base;
			PreciseDestRelOffset = WorldToRelativeOffset(PreciseDestBase->Rotation, PreciseDestination - PreciseDestBase->Location);
		}
	}
}

void UGameSpecialMove::SetFacePreciseRotation(FRotator RotationToFace, FLOAT InterpolationTime)
{
	if ( PawnOwner )
	{
		if ( PawnOwner->IsHumanControlled() )
		{
			PreciseRotation						= RotationToFace;
			PreciseRotationInterpolationTime	= InterpolationTime;
			bReachPreciseRotation				= TRUE;
			bReachedPreciseRotation				= FALSE;
		}
		else
		{
			// Have AI to use DesiredRotation instead of PreciseRotation at the end.
			PawnOwner->SetDesiredRotation(RotationToFace, TRUE, TRUE, InterpolationTime);
		}
	}
}

/* ResetFacePreciseRotation
* Clear all the vars related to PreciseRotation
* Called when this specialmove ends, otherwise it would carry over to next SM
*/
void UGameSpecialMove::ResetFacePreciseRotation()
{
	if ( PawnOwner->IsHumanControlled() )
	{
		PreciseRotationInterpolationTime	= 0;
		bReachPreciseRotation				= FALSE;
		bReachedPreciseRotation				= FALSE;
	}
	else
	{	
		// need to unlock
		PawnOwner->LockDesiredRotation(FALSE);
	}
}


/** Forces Pawn's rotation to a given Rotator */
void UGameSpecialMove::ForcePawnRotation(APawn* P, FRotator NewRotation)
{
	if( !P || P->Rotation == NewRotation )
	{
		return;
	}

	P->SetRotation(NewRotation);
	P->SetDesiredRotation (NewRotation);

	// Update AI Controller as well.
	// Don't change controller rotation if human player is in free cam
	if( P->Controller && (!P->IsHumanControlled() || !P->eventInFreeCam()) )
	{
		FRotator ControllerRot = P->Controller->Rotation;
		ControllerRot.Yaw = P->Rotation.Yaw;
		P->SetDesiredRotation(ControllerRot);
		P->Controller->SetFocalPoint( P->Location + NewRotation.Vector() * 1024.f );	// only needed when bLockPawnRotation is not set.
	}
}

/**
* Turn a World Space Offset into an Rotation relative Offset.
*/
FVector UGameSpecialMove::WorldToRelativeOffset( FRotator InRotation, FVector WorldSpaceOffset ) const
{
	FRotationMatrix	RotM(InRotation);
	return FVector(WorldSpaceOffset | RotM.GetAxis(0), WorldSpaceOffset | RotM.GetAxis(1), WorldSpaceOffset | RotM.GetAxis(2));
}

/** Do the opposite as above. Get a Rotation relative offset, and turn it into a world space offset */
FVector UGameSpecialMove::RelativeToWorldOffset( FRotator InRotation, FVector RelativeSpaceOffset ) const
{
	FRotationMatrix	RotM(InRotation);
	return RelativeSpaceOffset.X * RotM.GetAxis(0) + RelativeSpaceOffset.Y * RotM.GetAxis(1) + RelativeSpaceOffset.Z * RotM.GetAxis(2);
}
