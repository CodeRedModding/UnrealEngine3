//=============================================================================
// Copyright 2003 Epic Games - All Rights Reserved.
// Confidential.
//=============================================================================

#include "WarfareGame.h"

IMPLEMENT_CLASS(UAnimNodeBlendByPhysics);

IMPLEMENT_CLASS(UWarAnim_BaseBlendNode);
IMPLEMENT_CLASS(UWarAnim_CoverBlendNode);
IMPLEMENT_CLASS(UWarAnim_CoverMoveBlendNode);
IMPLEMENT_CLASS(UWarAnim_CoverSequenceNode);
IMPLEMENT_CLASS(UWarAnim_CoverFireBlendNode);
IMPLEMENT_CLASS(UWarAnim_EvadeBlendNode);

void UWarAnim_CoverSequenceNode::TickAnim(FLOAT DeltaSeconds, FLOAT TotalWeight)
{
	// check to see if we should apply the intro transition
	if (bIntroTransition)
	{
		if (AnimSeqName != IntroAnimSeqName)
		{
			bZeroRootTranslationX = false;
			bZeroRootTranslationY = false;
			SkelComponent->RootBoneOption[0] = RBA_Translate;
			SkelComponent->RootBoneOption[1] = RBA_Translate;
			SetAnim(IntroAnimSeqName);
			PlayAnim(0, Rate, 0.f);
		}
		else
		{
			// otherwise check to see if anim has finished
			if (AnimSeq != NULL &&
				CurrentTime == AnimSeq->SequenceLength)
			{
				SkelComponent->RootBoneOption[0] = RBA_Default;
				SkelComponent->RootBoneOption[1] = RBA_Default;
				SetAnim(IdleAnimSeqName);
				PlayAnim(1, Rate, 0.f);
				bIntroTransition = 0;
			}
		}
	}
	else
	if (bOutroTransition)
	{
		if (AnimSeqName != OutroAnimSeqName)
		{
			SkelComponent->RootBoneOption[0] = RBA_Translate;
			SkelComponent->RootBoneOption[1] = RBA_Translate;
			SetAnim(OutroAnimSeqName);
			PlayAnim(0, Rate, 0.f);
		}
		else
		{
			if (AnimSeq != NULL &&
				CurrentTime == AnimSeq->SequenceLength)
			{
				bZeroRootTranslationX = true;
				bZeroRootTranslationY = true;
				SkelComponent->RootBoneOption[0] = RBA_Default;
				SkelComponent->RootBoneOption[1] = RBA_Default;
				bOutroTransition = 0;
			}
		}
	}
	Super::TickAnim(DeltaSeconds, TotalWeight);
}

/**
 * Overridden to check the state of the weapon, and to activate
 * the second child if currently firing.
 * 
 * @param		DeltaSeconds - time since last update
 */
void UWarAnim_CoverFireBlendNode::TickAnim(FLOAT DeltaSeconds, FLOAT TotalWeight)
{
	AWarPawn *wfPawn = Cast<AWarPawn>(SkelComponent->Owner);
	if ( wfPawn != NULL )
	{
		// try to get the weapon
		AWarWeapon	*weap = Cast<AWarWeapon>(wfPawn->Weapon);
		if ( weap != NULL )
		{
			// check what the current weapon state is
			if ( weap->bIsFiring )
			{
				LastFireTime = wfPawn->Level->TimeSeconds;

				// make sure the 2nd child is active
				if ( Child2WeightTarget != 1.f )
				{
					SetBlendTarget(1.f,0.15f);
				}
			}
			else
			{
				// not firing, make sure the 1st child is active
				if ( Child2WeightTarget != 0.f && (wfPawn->Level->TimeSeconds - LastFireTime) >= FireBlendOutDelay )
				{
					SetBlendTarget(0.f,0.15f);
				}
			}
			
		}
		else
		{
			// no weapon, so make sure that the 1st child is active
			if ( Child2WeightTarget != 0.f )
			{
				SetBlendTarget(0.f,0.15f);
			}
		}
	}
	else
	{
		// no pawn, so make sure that the 1st child is active
		if ( Child2WeightTarget != 0.f )
		{
			SetBlendTarget(0.f,0.15f);
		}
	}

	// normal update
	Super::TickAnim( DeltaSeconds, TotalWeight );
}

/**
 * Overridden to look at the current CoverAction value and activate
 * the appropriate animation child.
 * 
 * @param		DeltaSeconds - time since last update
 */
void UWarAnim_CoverMoveBlendNode::TickAnim(FLOAT DeltaSeconds, FLOAT TotalWeight)
{
	AWarPawn *wfPawn = Cast<AWarPawn>(SkelComponent->Owner);

	if( wfPawn != NULL )
	{
		INT newChildIdx = -1;

		// first check to see if reloading
		AWarWeapon *warWeap = Cast<AWarWeapon>(wfPawn->Weapon);
		if( warWeap != NULL &&
			warWeap->eventIsReloading() )
		{
			newChildIdx = 10;
		}
		// otherwise, if the pawn is moving, then activate the movement nodes
		else
		if( !wfPawn->Velocity.IsNearlyZero() )
		{
			// figure out which direction we're moving by dotting the velocity against the y axis of the pawn orientation
			FLOAT velDot = wfPawn->Velocity | (wfPawn->Rotation.Vector() ^ FVector(0.f,0.f,1.f));
			newChildIdx = velDot < 0.f ? 1 : 2;
		}
		else
		{
			// activate the new animation
			switch( wfPawn->CoverAction )
			{
				case CA_Default:
					newChildIdx = 0;
					break;
				case CA_LeanRight:
					newChildIdx = 3;
					break;
				case CA_LeanLeft:
					newChildIdx = 4;
					break;
				case CA_StepRight:
					newChildIdx = 5;
					break;
				case CA_StepLeft:
					newChildIdx = 6;
					break;
				case CA_BlindRight:
					newChildIdx = 7;
					break;
				case CA_BlindLeft:
					newChildIdx = 8;
					break;
				case CA_PopUp:
					newChildIdx = 9;
					break;
				default:
					break;
			}
		}

		newChildIdx = Clamp<INT>(newChildIdx, 0, Children.Num()-1 );
		if( newChildIdx != ActiveChildIndex )
		{
			// if activating reload, then fire up the animation
			if( newChildIdx == 10 )
			{
				UAnimNodeSequence *reloadNode = Cast<UAnimNodeSequence>(Children(newChildIdx).Anim);
				if( reloadNode != NULL )
				{
					reloadNode->PlayAnim(0,reloadNode->Rate, 0.f);
					SetActiveChild(newChildIdx, 0.25f);
				}
			}
			// otherwise activate the new child
			else
			{
				// check for an intro transition
				UWarAnim_CoverSequenceNode *seqNode = Cast<UWarAnim_CoverSequenceNode>(Children(newChildIdx).Anim);
				if( seqNode != NULL )
				{
					seqNode->bIntroTransition = 1;
					SetActiveChild(newChildIdx, 0.25f);
				}
				else
				{
					seqNode = Cast<UWarAnim_CoverSequenceNode>(Children(ActiveChildIndex).Anim);
					// check for an outro anim
					if( seqNode != NULL )
					{
						seqNode->bOutroTransition = 1;
						if( seqNode->CurrentTime == seqNode->AnimSeq->SequenceLength )
						{
							// transition finished, allow new node to be activated
							SetActiveChild(newChildIdx, 0.25f);
						}
					}
					// not an outro node, 
					else
					{
						// so activate the new child
						SetActiveChild(newChildIdx, 0.25f);
					}
				}
			}
		}

	}
	else
	{
		if( ActiveChildIndex != 0 )
		{
			SetActiveChild(0, 0.f);
		}
	}
	// normal blend update
	Super::TickAnim(DeltaSeconds, TotalWeight);
}

/**
 * Overridden to look at the WarPawn owner and activate the 
 * third child if evading, second child if using cover, and finally
 * the base child if neither.
 * 
 * @param		DeltaSeconds - time since last update
 */
void UWarAnim_BaseBlendNode::TickAnim(FLOAT DeltaSeconds, FLOAT TotalWeight)
{
	AWarPawn *wfPawn = Cast<AWarPawn>(SkelComponent->Owner);
	if ( wfPawn != NULL )
	{
		// first check if we are evading
		if( wfPawn->EvadeDirection != ED_None )
		{
			if (wfPawn->EvadeDirection != LastEvadeDirection)
			{
				LastEvadeDirection = wfPawn->EvadeDirection;
				// make sure the 3rd child is active
				SetActiveChild(2,0.25f);
				UAnimNodeBlendList *anim = Cast<UAnimNodeBlendList>(Children(2).Anim);
				if (anim != NULL)
				{
					// activate the proper animation
					INT evadeIdx = (INT)LastEvadeDirection - 1;
					UAnimNodeSequence *ANodeSeq = Cast<UAnimNodeSequence>(anim->Children(evadeIdx).Anim);
					// and start the appropriate child
					if (ANodeSeq != NULL)
					{
						ANodeSeq->PlayAnim(0, ANodeSeq->Rate);
					}
					anim->SetActiveChild(evadeIdx, 0.f);
				}
			}
		}
		else
		{

			// next check if we are using cover
			if ( wfPawn->CoverType != CT_None )
			{
				// make sure the 2nd child is active
				if (ActiveChildIndex != 1)
				{
					SetActiveChild(1,0.25f);
				}
			}
			else
			{
				// otherwise make sure the 1st child is active
				if (ActiveChildIndex != 0)
				{
					SetActiveChild(0,0.25f);
				}
			}
		}
	}
	// and call the parent version to perform the actual interpolation
	Super::TickAnim(DeltaSeconds, TotalWeight);
}

/** @see AnimNode::OnChildAnimEnd() */
void UWarAnim_BaseBlendNode::OnChildAnimEnd(UAnimNodeSequence* Child) 
{
	if( Child->ParentNode == Children(2).Anim )
	{
		// Reset LastEvadeDirection when animation is finished
		if( LastEvadeDirection != ED_None )
		{
			LastEvadeDirection = ED_None;
		}
	}

	Super::OnChildAnimEnd( Child );
}

void UWarAnim_CoverBlendNode::TickAnim(FLOAT DeltaSeconds, FLOAT TotalWeight)
{
	AWarPawn *wfPawn = Cast<AWarPawn>(SkelComponent->Owner);
	// query the current cover state to determine which child should be active
	if (	wfPawn != NULL && 
			wfPawn->CoverType != CT_None && 
			LastCoverType != wfPawn->CoverType )
	{
		SetActiveChild(wfPawn->CoverType-1,0.25f);
		LastCoverType = wfPawn->CoverType;
	}
	Super::TickAnim(DeltaSeconds, TotalWeight);
}

// BlendByPhysics - Look at the pawns current physics and set the blends accordingly if they are wrong.

void UAnimNodeBlendByPhysics::TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight )
{
	// Get the Pawn Owner
	if (SkelComponent != NULL &&
		SkelComponent->Owner != NULL &&
		SkelComponent->Owner->IsA(APawn::StaticClass()))
	{
		APawn* POwner = (APawn*)SkelComponent->Owner;
		// Remap the physics to a node in the tree
		INT MappedPhysics = PhysicsMap(INT(POwner->Physics));
		if (MappedPhysics >= 0)
		{
			// If the node for this physics isn't blended in, or being blended in, blend it
			if (LastMap != MappedPhysics)
			{
				SetActiveChild(MappedPhysics,0.1f);
				POwner->eventChangeAnimation( Children(MappedPhysics).Anim );	// Notify the pawn
			}
			LastMap = MappedPhysics;			
			Super::TickAnim(DeltaSeconds, TotalWeight);
		}
	}
}

