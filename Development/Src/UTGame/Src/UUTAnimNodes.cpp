//=============================================================================
// Copyright 2004 Epic Games - All Rights Reserved.
// Confidential.
//=============================================================================

#include "UTGame.h"

IMPLEMENT_CLASS(UUTAnimBlendBase);
IMPLEMENT_CLASS(UUTAnimBlendByIdle);
IMPLEMENT_CLASS(UUTAnimNodeSequence);
IMPLEMENT_CLASS(UUTAnimBlendByPhysics);
IMPLEMENT_CLASS(UUTAnimBlendByFall);
IMPLEMENT_CLASS(UUTAnimBlendByPosture);
IMPLEMENT_CLASS(UUTAnimBlendByWeapon);
IMPLEMENT_CLASS(UUTAnimBlendByDirection);
IMPLEMENT_CLASS(UUTAnimBlendByDodge);


/**
 *  BlendByPhysics - This AnimNode type is used to determine which branch to player
 *  by looking at the current physics of the pawn.  It uses that value to choose a node
 */
void UUTAnimBlendByPhysics::TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight )
{
	// Get the Pawn Owner
	if (SkelComponent != NULL &&
		SkelComponent->Owner != NULL &&
		SkelComponent->Owner->IsA(APawn::StaticClass()))
	{
		APawn* POwner = (APawn*)SkelComponent->Owner;

		// Get the current physics from the pawn

		INT CurrentPhysics = INT(POwner->Physics);

		// If the physics has changed, and there is a valid blend for it, blend to that value

		if ( LastPhysics != CurrentPhysics )
		{
			INT PhysicsIndex = PhysicsMap[CurrentPhysics];
			SetActiveChild( PhysicsIndex,eventGetBlendTime(PhysicsIndex,false) );
			POwner->eventChangeAnimation( Children(CurrentPhysics).Anim );		// Notify the pawn
		}

		LastPhysics = CurrentPhysics;			
		Super::TickAnim(DeltaSeconds, TotalWeight);
	}
}

/**
 * BlendByFall - Will use the pawn's Z Velocity to determine what type of blend to perform.  
 * -- FIXME: Add code to trace to the ground and blend to landing
 */

void UUTAnimBlendByFall::TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight )
{

	if ( TotalWeight > 0 )
	{
		// If we are not being rendered, reset the state to FBT_None and exit.

		if (SkelComponent != NULL &&
			SkelComponent->Owner != NULL &&
			SkelComponent->Owner->IsA(APawn::StaticClass()))
		{
			APawn* POwner = (APawn*)SkelComponent->Owner;

			if (POwner->Physics == PHYS_Falling)
			{
				FLOAT FallingVelocity = POwner->Velocity.Z;
				switch (FallState)
				{
					case FBT_Land:
					case FBT_None:		//------------- We were inactive, determine the initial state

						if ( FallingVelocity < 0 )			// Falling
							ChangeFallState(FBT_Down);
						else								// Jumping
		
							ChangeFallState(FBT_Up);
															
						for (INT i=0;i<Children.Num();i++)
							if (i != FallState)
							{
								Children(i).Weight=0.0f;
								Children(i).bActive= false;
							}
							else
							{
								Children(i).Weight=1.0f;
								Children(i).bActive= true;
							}

						break;

					case FBT_Up:		//------------- We are jumping
						if ( LastFallingVelocity < FallingVelocity )	// Double Jump
						{
							ChangeFallState(FBT_Up);
						}

						else if (FallingVelocity <= 0)					// Begun to fall
							ChangeFallState(FBT_Down);

						break;
						
					case FBT_Down:		//------------- We are falling

						if ( !bDodgeFall && FallingVelocity > 0 && FallingVelocity > LastFallingVelocity )		// Double Jump
							ChangeFallState(FBT_Up);
						else
						{
							if (!bDodgeFall)
							{

								DWORD TraceFlags = TRACE_World;
								FCheckResult Hit(1.f);

								FVector HowFar = POwner->Velocity * eventGetBlendTime(FBT_PreLand,false) * 1.5;
								POwner->GetLevel()->SingleLineCheck(Hit, POwner, POwner->Location + HowFar, POwner->Location,TraceFlags);

								if ( Hit.Actor ) 
								{
									ChangeFallState(FBT_PreLand);
								}
							}
							else if ( FallingVelocity < 0 )
							{
									ChangeFallState(FBT_PreLand);
									BlendTimeToGo = 1.0f; //FallTime;
							}

						}

						break;
				}
				LastFallingVelocity = FallingVelocity;
			}
			else if ( FallState != FBT_Land )
			{
				ChangeFallState(FBT_Land);
			}

		}
	}
	Super::TickAnim(DeltaSeconds,TotalWeight);	// pass along for now until falls anims are separated			

}

void UUTAnimBlendByFall::SetActiveChild( INT ChildIndex, FLOAT BlendTime )
{
	Super::SetActiveChild(ChildIndex,BlendTime);

	if ( Cast<UAnimNodeSequence>( Children(ChildIndex).Anim ) )
	{
		UAnimNodeSequence* P = Cast<UAnimNodeSequence>( Children(ChildIndex).Anim );	
		P->PlayAnim(P->bLooping);
	}
}


void UUTAnimBlendByFall::OnChildAnimEnd(UAnimNodeSequence* Child)
{
	if ( bDodgeFall && FallState == FBT_Up && Child == Children(FBT_Up).Anim )
	{
		ChangeFallState(FBT_Down);
	}
}
		

// Changes the falling state

void UUTAnimBlendByFall::ChangeFallState(EBlendFallTypes NewState)
{
	if (FallState != NewState)
	{
		FallState = NewState;
		if (FallState!=FBT_None)
		{
			SetActiveChild( NewState, eventGetBlendTime(NewState,false) );
		}
	}
}

/**
 *  BlendByPosture is used to determine if we should be playing the Crouch/walk animations, or the 
 *  running animations.
 */

void UUTAnimBlendByPosture::TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight )
{
	// Get the Pawn Owner
	if (SkelComponent != NULL &&
		SkelComponent->Owner != NULL &&
		SkelComponent->Owner->IsA(APawn::StaticClass()))
	{
		APawn* POwner = (APawn*)SkelComponent->Owner;

		if ( POwner->bIsCrouched && ActiveChildIndex!=1 )
		{
			SetActiveChild(1,0);
		}
		else if ( ActiveChildIndex != 0 )
		{
			SetActiveChild(0,0); // FIXME: Add a blend rate
		}

	}
	Super::TickAnim(DeltaSeconds, TotalWeight);
}

/**
 * BlendByWeapon - This node is NOT automanaged.  Instead it's designed to have it's Fire/StopFire functions
 * called.  If it's playing a firing animation that's not looping (ie: not auto-fire) it will blend back out after the
 * animation completes.
 */

void UUTAnimBlendByWeapon::OnChildAnimEnd(UAnimNodeSequence* Child)
{
	Super::OnChildAnimEnd(Child);

	// Call the script event if we are not looping.

	if (!bLooping)
		eventAnimStopFire( BlendTime );
}


/** 
 * BlendByDirection nodes look at the direction their owner is moving and use it to
 * blend between the different children.  We have extended the Base (BlendDirectional) in
 * order to add the ability to adjust the animation speed of one of this node's children
 * depending on the velocity of the pawn.  
 */

void UUTAnimBlendByDirection::TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight )
{

	// We only work if we are visible

	if (TotalWeight>0)
	{
		// bAdjustRateByVelocity is used to make the animation slow down the animation

		if (bAdjustRateByVelocity)
		{
			if (SkelComponent != NULL &&
				SkelComponent->Owner != NULL &&
				SkelComponent->Owner->IsA(APawn::StaticClass()))
			{
				APawn* POwner = (APawn*)SkelComponent->Owner;

				FLOAT NewRate = POwner->Velocity.Size() / POwner->GroundSpeed;
				for (INT i=0;i<Children.Num();i++)
				{
					if ( Cast<UUTAnimNodeSequence>(Children(i).Anim) )
						Cast<UUTAnimNodeSequence>(Children(i).Anim)->Rate = NewRate;
				}
			}
		}

		EBlendDirTypes	CurrentDirection = Get4WayDir();

		if (CurrentDirection != LastDirection)		// Direction changed
		{
			SetActiveChild( CurrentDirection, eventGetBlendTime(CurrentDirection,false) );
		}

		LastDirection = CurrentDirection;

	}
	else
		LastDirection = FBDir_None;

	Super::TickAnim(DeltaSeconds, TotalWeight);

}

void UUTAnimBlendByDirection::SetActiveChild( INT ChildIndex, FLOAT BlendTime )
{
	Super::SetActiveChild(ChildIndex,BlendTime);

	if ( Cast<UAnimNodeSequence>( Children(ChildIndex).Anim ) )
	{
		UAnimNodeSequence* P = Cast<UAnimNodeSequence>( Children(ChildIndex).Anim );
		P->PlayAnim(P->bLooping);
	}
}


EBlendDirTypes UUTAnimBlendByDirection::Get4WayDir()
{

    FLOAT forward, right;
    FVector V;

	if (SkelComponent != NULL &&
		SkelComponent->Owner != NULL &&
		SkelComponent->Owner->IsA(APawn::StaticClass()))
	{
		APawn* POwner = (APawn*)SkelComponent->Owner;


		V = POwner->Velocity;
		V.Z = 0.0f;

		if ( V.IsNearlyZero() )
			return FBDir_Forward;

		FRotationMatrix RotMatrix(POwner->Rotation);

		V.Normalize();
		forward = RotMatrix.GetAxis(0) | V;
		if (forward > 0.82f) // 55 degrees
			return FBDir_Forward;
		else if (forward < -0.82f)
			return FBDir_Back;
		else
		{
			right = RotMatrix.GetAxis(1) | V;
			
			if (right > 0.0f)
				return FBDir_Right;
			else
				return FBDir_Left;
		}
	}
	
	return FBDir_Forward;

}


/**
 * This blend looks at the velocity of the player and blends depending on if they are moving or not
 */

void UUTAnimBlendByIdle::TickAnim(float DeltaSeconds, FLOAT TotalWeight)
{
	// Get the Pawn Owner
	if (SkelComponent != NULL &&
		SkelComponent->Owner != NULL &&
		SkelComponent->Owner->IsA(APawn::StaticClass()))
	{
		APawn* POwner = (APawn*)SkelComponent->Owner;

		if ( POwner->Velocity.Size() == 0)
			SetActiveChild(0,BlendTime);
		else
			SetActiveChild(1,BlendTime);
	}
	Super::TickAnim(DeltaSeconds, TotalWeight);
}

/**
 * When the sequece becomes active, check to see if we should restart the animation
 */

void UUTAnimNodeSequence::OnBecomeActive()
{
	Super::OnBecomeActive();

	if (bResetOnActivate)
	{
//		PlayAnim(bLooping,Rate);
	}
}

/**
 * BlendByDodge checkes the velocity to determine if a dodge has occured.
 */

void UUTAnimBlendByDodge::TickAnim(float DeltaSeconds, FLOAT TotalWeight)
{
	// Find the type of dodge to perform

	if ( TotalWeight > 0 ) 
	{

		// Get the Pawn Owner
		if (SkelComponent != NULL &&
			SkelComponent->Owner != NULL &&
			SkelComponent->Owner->IsA(APawn::StaticClass()))
		{
			APawn* POwner = (APawn*)SkelComponent->Owner;

			if (POwner->Physics == PHYS_Falling)		// If we aren't falling ,we aren't dodging
			{

				// Check to see if we have any weight and if so, are we not dodging yet?

				if (TotalWeight>0.0f && CurrentDodge == DODGEBLEND_None)
				{
					float DodgeSpeedThresh = ((POwner->GroundSpeed * 1.5) + POwner->GroundSpeed) * 0.5f ;
					float XYVelocitySquared = (POwner->Velocity.X*POwner->Velocity.X)+(POwner->Velocity.Y*POwner->Velocity.Y);

					// Check to see if we are actually Dodging

					if ( XYVelocitySquared > DodgeSpeedThresh*DodgeSpeedThresh ) 
					{
						INT	CurrentDirection = Get4WayDir();

						UUTAnimBlendByFall* BFall = Cast<UUTAnimBlendByFall>( Children(1).Anim );
						if (BFall)
						{
							for (INT i=0;i<4;i++)
							{	
								BFall->Children(i).Anim->SetAnim(DodgeAnims[ (CurrentDirection*4) + i]);
							}
						}

						CurrentDodge = CurrentDirection + 1;
						SetActiveChild(1, eventGetBlendTime(1,false) );
					}
				}

				// If we are no longer being displayed, turn off dodging

				else if ( TotalWeight <= 0.0f && CurrentDodge != DODGEBLEND_None )
				{

					SetActiveChild(0,eventGetBlendTime(0,false) );
					CurrentDodge = DODGEBLEND_None;
				}
			}
			else if (CurrentDodge != DODGEBLEND_None)
			{
				SetActiveChild(0,eventGetBlendTime(0,false) );
				CurrentDodge = DODGEBLEND_None;
			}
		}
	}

	UAnimNodeBlendList::TickAnim(DeltaSeconds, TotalWeight);

}
