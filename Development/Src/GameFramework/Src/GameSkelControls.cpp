/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "GameFramework.h"
#include "EngineAnimClasses.h"
#include "GameFrameworkAnimClasses.h"

IMPLEMENT_CLASS(UGameSkelCtrl_Recoil);

/************************************************************************************
 * UGameSkelCtrl_Recoil
 ***********************************************************************************/

void UGameSkelCtrl_Recoil::TickSkelControl(FLOAT DeltaSeconds, USkeletalMeshComponent* SkelComp)
{
	bApplyControl = FALSE;

	if( ControlStrength > ZERO_ANIMWEIGHT_THRESH )
	{
		// if willing to play recoil, reset its state
		if( bPlayRecoil != bOldPlayRecoil )
		{
			bPlayRecoil = bOldPlayRecoil;

			Recoil.TimeToGo			= Recoil.TimeDuration;

			// ERS_Random == Start at random position along sine wave, 
			// ERS_Zero == Start at 0
			const FLOAT TWO_PI		= 2.f * (FLOAT)PI;
			Recoil.RotSinOffset.X	= Recoil.RotParams.X == ERS_Random ? appFrand() * TWO_PI : 0.f;
			Recoil.RotSinOffset.Y	= Recoil.RotParams.Y == ERS_Random ? appFrand() * TWO_PI : 0.f;
			Recoil.RotSinOffset.Z	= Recoil.RotParams.Z == ERS_Random ? appFrand() * TWO_PI : 0.f;

			Recoil.LocSinOffset.X	= Recoil.LocParams.X == ERS_Random ? appFrand() * TWO_PI : 0.f;
			Recoil.LocSinOffset.Y	= Recoil.LocParams.Y == ERS_Random ? appFrand() * TWO_PI : 0.f;
			Recoil.LocSinOffset.Z	= Recoil.LocParams.Z == ERS_Random ? appFrand() * TWO_PI : 0.f;

			Recoil.RotOffset		= FRotator(0,0,0);
			Recoil.LocOffset		= FVector(0.f);
		}

		if( Recoil.TimeToGo > DeltaSeconds )
		{
			Recoil.TimeToGo -= DeltaSeconds;

			if( Recoil.TimeToGo > 0.f )
			{
				bApplyControl = TRUE;

				// Smooth fade out
				const FLOAT TimePct			= Clamp<FLOAT>(Recoil.TimeToGo / Recoil.TimeDuration, 0.f, 1.f);
				const FLOAT Alpha			= TimePct*TimePct*(3.f - 2.f*TimePct);
				const FLOAT	AlphaTimesDelta	= Alpha * DeltaSeconds;

				// Recoil Bone Rotation, compute sin wave value for each component
				if( !Recoil.RotAmplitude.IsZero() )
				{
					if( Recoil.RotAmplitude.X != 0.f ) 
					{
						Recoil.RotSinOffset.X	+= AlphaTimesDelta * Recoil.RotFrequency.X;
						Recoil.RotOffset.Pitch	= appTrunc(Alpha * Recoil.RotAmplitude.X * appSin(Recoil.RotSinOffset.X));
					}
					if( Recoil.RotAmplitude.Y != 0.f ) 
					{
						Recoil.RotSinOffset.Y	+= AlphaTimesDelta * Recoil.RotFrequency.Y;
						Recoil.RotOffset.Yaw	= appTrunc(Alpha * Recoil.RotAmplitude.Y * appSin(Recoil.RotSinOffset.Y));
					}
					if( Recoil.RotAmplitude.Z != 0.f ) 
					{
						Recoil.RotSinOffset.Z	+= AlphaTimesDelta * Recoil.RotFrequency.Z;
						Recoil.RotOffset.Roll	= appTrunc(Alpha * Recoil.RotAmplitude.Z * appSin(Recoil.RotSinOffset.Z));
					}
				}

				// Recoil Bone Location, compute sin wave value for each component
				if( !Recoil.LocAmplitude.IsZero() )
				{
					if( Recoil.LocAmplitude.X != 0.f ) 
					{
						Recoil.LocSinOffset.X	+= AlphaTimesDelta * Recoil.LocFrequency.X;
						Recoil.LocOffset.X		= Alpha * Recoil.LocAmplitude.X * appSin(Recoil.LocSinOffset.X);
					}
					if( Recoil.LocAmplitude.Y != 0.f ) 
					{
						Recoil.LocSinOffset.Y	+= AlphaTimesDelta * Recoil.LocFrequency.Y;
						Recoil.LocOffset.Y		= Alpha * Recoil.LocAmplitude.Y * appSin(Recoil.LocSinOffset.Y);
					}
					if( Recoil.LocAmplitude.Z != 0.f ) 
					{
						Recoil.LocSinOffset.Z	+= AlphaTimesDelta * Recoil.LocFrequency.Z;
						Recoil.LocOffset.Z		= Alpha * Recoil.LocAmplitude.Z * appSin(Recoil.LocSinOffset.Z);
					}
				}
			}
		}
	}

	Super::TickSkelControl(DeltaSeconds, SkelComp);
}


/** Pull aim information from Pawn */
FVector2D UGameSkelCtrl_Recoil::GetAim(USkeletalMeshComponent* InSkelComponent) 
{ 
	return Aim;
}

/** Is skeleton currently mirrored */
UBOOL UGameSkelCtrl_Recoil::IsMirrored(USkeletalMeshComponent* InSkelComponent) 
{
	return FALSE;
}

void UGameSkelCtrl_Recoil::GetAffectedBones(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<INT>& OutBoneIndices)
{
	check(OutBoneIndices.Num() == 0);

	// Only process bone if there is something to do
	if( bApplyControl )
	{
		OutBoneIndices.AddItem(BoneIndex);
	}
}

void UGameSkelCtrl_Recoil::CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	// Current bone transform matrix in component space
	FBoneAtom NewBoneTM = SkelComp->SpaceBases(BoneIndex);

	if(bBoneSpaceRecoil)
	{
		if( !Recoil.LocOffset.IsZero() || !Recoil.RotOffset.IsZero() )
		{
			FBoneAtom RecoilTM(Recoil.RotOffset, Recoil.LocOffset);
			NewBoneTM = RecoilTM * NewBoneTM;
			OutBoneTransforms.AddItem(NewBoneTM);
		}

		return;
	}
	

	// Extract Aim
	Aim = GetAim(SkelComp);

	// Actor to Aim transform matrix
	const FRotator	AimRotOffset( appTrunc(Aim.Y*16384), appTrunc(Aim.X*16384), 0);
	FBoneAtom ActorToAim(AimRotOffset, FVector::ZeroVector);
	ActorToAim.RemoveScaling();
	ActorToAim.SetOrigin(FVector(0.f));
	const FBoneAtom AimToActor = ActorToAim.Inverse();

	// Component to Actor transform matrix
	FBoneAtom ComponentToActor = SkelComp->CalcComponentToFrameMatrix(BoneIndex, BCS_ActorSpace, NAME_None);
	ComponentToActor.RemoveScaling();
	ComponentToActor.SetOrigin(FVector(0.f));
	const FBoneAtom ActorToComponent = ComponentToActor.InverseSafe();

	// Add rotation offset in component space
	if( !Recoil.RotOffset.IsZero() )
	{
		FRotator RotOffset = Recoil.RotOffset;

		// Handle mirroring
		if( IsMirrored(SkelComp) )
		{
			RotOffset.Yaw = -RotOffset.Yaw;
		}

		FBoneAtom NewRotTM = NewBoneTM * (ComponentToActor * (AimToActor * FBoneAtom(RotOffset, FVector::ZeroVector) * ActorToAim) * ActorToComponent);	
		NewRotTM.SetOrigin(NewBoneTM.GetOrigin());
		NewBoneTM = NewRotTM;
	}

	// Add location offset in component space
	if( !Recoil.LocOffset.IsZero() )
	{
		FVector LocOffset = Recoil.LocOffset;

		// Handle mirroring
		if( IsMirrored(SkelComp) )
		{
			LocOffset.Y = -LocOffset.Y;
		}

		const FVector	TransInWorld	= ActorToAim.TransformNormal(LocOffset);
		const FVector	TransInComp		= ActorToComponent.TransformNormal(TransInWorld);
		const FVector	NewOrigin		= NewBoneTM.GetOrigin() + TransInComp;
		NewBoneTM.SetOrigin(NewOrigin);
	}

	OutBoneTransforms.AddItem(NewBoneTM);
}


