/*=============================================================================
	UnCamera.cpp: Unreal Engine Camera Actor implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineAnimClasses.h"
#include "EngineCameraClasses.h"
#include "EngineAudioDeviceClasses.h"

IMPLEMENT_CLASS(ACamera);
IMPLEMENT_CLASS(ACameraActor);
IMPLEMENT_CLASS(ADynamicCameraActor);
IMPLEMENT_CLASS(UCameraAnim);
IMPLEMENT_CLASS(UCameraAnimInst);

IMPLEMENT_CLASS(UCameraModifier);
IMPLEMENT_CLASS(UCameraModifier_CameraShake);
IMPLEMENT_CLASS(UCameraShake);



/*------------------------------------------------------------------------------
	ACamera
------------------------------------------------------------------------------*/


/**
 * Set a new ViewTarget with optional transition time
 */
void ACamera::SetViewTarget(class AActor* NewTarget, struct FViewTargetTransitionParams TransitionParams)
{
	// Make sure view target is valid
	if( NewTarget == NULL )
	{
		NewTarget = PCOwner;
	}

	// Update current ViewTargets
	CheckViewTarget(ViewTarget);
	if( PendingViewTarget.Target )
	{
		CheckViewTarget(PendingViewTarget);
	}

	// If we're already transitioning to this new target, don't interrupt.
	if( PendingViewTarget.Target != NULL && NewTarget == PendingViewTarget.Target )
	{
		return;
	}

	// if different then new one, then assign it
	if( NewTarget != ViewTarget.Target )
	{
		// if a transition time is specified, then set pending view target accordingly
		if( TransitionParams.BlendTime > 0 )
		{
			// band-aid fix so that eventEndViewTarget() gets called properly in this case
			if (PendingViewTarget.Target == NULL)
			{
				PendingViewTarget.Target = ViewTarget.Target;
			}

			// use last frame's POV
			ViewTarget.POV = LastFrameCameraCache.POV;
			BlendParams		= TransitionParams;
			BlendTimeToGo	= TransitionParams.BlendTime;
			
			AssignViewTarget(NewTarget, PendingViewTarget, TransitionParams);
			CheckViewTarget(PendingViewTarget);
		}
		else
		{
			// otherwise, assign new viewtarget instantly
			AssignViewTarget(NewTarget, ViewTarget);
			CheckViewTarget(ViewTarget);
			// remove old pending ViewTarget so we don't still try to switch to it
			PendingViewTarget.Target = NULL;
		}
	}
	else
	{
		// we're setting the viewtarget to the viewtarget we were transitioning away from,
		// just abort the transition.
		// @fixme, investigate if we want this case to go through the above code, so AssignViewTarget et al
		// get called
		if (PendingViewTarget.Target != NULL)
		{
			if (!PCOwner->bPendingDelete && !PCOwner->IsLocalPlayerController() && WorldInfo->NetMode != NM_Client)
			{
				PCOwner->eventClientSetViewTarget(NewTarget, TransitionParams);
			}
		}
		PendingViewTarget.Target = NULL;
	}
}


void ACamera::AssignViewTarget(AActor* NewTarget, FTViewTarget& VT, struct FViewTargetTransitionParams TransitionParams)
{
	if( !NewTarget || (NewTarget == VT.Target) )
	{
		return;
	}

// 	debugf(TEXT("%f AssignViewTarget OldTarget: %s, NewTarget: %s, BlendTime: %f"), WorldInfo->TimeSeconds, VT.Target ? *VT.Target->GetFName().ToString() : TEXT("NULL"),
// 		NewTarget ? *NewTarget->GetFName().ToString() : TEXT("NULL"),
// 		TransitionParams.BlendTime);

	AActor* OldViewTarget	= VT.Target;
	VT.Target				= NewTarget;
	// Set aspect ratio with default.
	VT.AspectRatio			= DefaultAspectRatio;

	// Set FOV with default.
	VT.POV.FOV				= DefaultFOV;

	VT.Target->eventBecomeViewTarget(PCOwner);
	
	if( OldViewTarget )
	{
		OldViewTarget->eventEndViewTarget(PCOwner);
	}

	if (!PCOwner->IsLocalPlayerController() && WorldInfo->NetMode != NM_Client)
	{
		PCOwner->eventClientSetViewTarget(VT.Target, TransitionParams);
	}
}


/** 
 * Make sure ViewTarget is valid 
 */
void ACamera::CheckViewTarget(FTViewTarget& VT)	
{
	if( !VT.Target )
	{
		VT.Target = PCOwner;
	}

	// Update ViewTarget PlayerReplicationInfo (used to follow same player through pawn transitions, etc., when spectating)
	if( VT.Target == PCOwner || (VT.Target->GetAPawn() && (VT.Target == PCOwner->Pawn)) ) 
	{	
		VT.PRI = NULL;
	}
	else if( VT.Target->GetAController() )
	{
		VT.PRI = VT.Target->GetAController()->PlayerReplicationInfo;
	}
	else if( VT.Target->GetAPawn() )
	{
		VT.PRI = VT.Target->GetAPawn()->PlayerReplicationInfo;
	}
	else if( Cast<APlayerReplicationInfo>(VT.Target) )
	{
		VT.PRI = Cast<APlayerReplicationInfo>(VT.Target);
	}
	else
	{
		VT.PRI = NULL;
	}

	if( VT.PRI && !VT.PRI->bDeleteMe )
	{
		if( !VT.Target || VT.Target->bDeleteMe || !VT.Target->GetAPawn() || (VT.Target->GetAPawn()->PlayerReplicationInfo != VT.PRI) )
		{
			VT.Target = NULL;

			// not viewing pawn associated with RealViewTarget, so look for one
			// Assuming on server, so PRI Owner is valid
			if( !VT.PRI->Owner )
			{
				VT.PRI = NULL;
			}
			else
			{
				AController* PRIOwner = VT.PRI->Owner->GetAController();
				if( PRIOwner )
				{
					AActor* PRIViewTarget = PRIOwner->Pawn;
					if( PRIViewTarget && !PRIViewTarget->bDeleteMe )
					{
						AssignViewTarget(PRIViewTarget, VT);
					}
					else
					{
						VT.PRI = NULL;
					}
				}
				else
				{
					VT.PRI = NULL;
				}
			}
		}
	}

	if( !VT.Target || VT.Target->bDeleteMe )
	{
		check(PCOwner);
		if( PCOwner->Pawn && !PCOwner->Pawn->bDeleteMe && !PCOwner->Pawn->bPendingDelete )
		{
			AssignViewTarget(PCOwner->Pawn, VT);
		}
		else
		{
			AssignViewTarget(PCOwner, VT);
		}
	}

	// Keep PlayerController in synch
	PCOwner->ViewTarget		= VT.Target;
	PCOwner->RealViewTarget	= VT.PRI;
}


/** 
 * Returns current ViewTarget 
 */
AActor* ACamera::GetViewTarget()
{
	// if blending to another view target, return this one first
	if( PendingViewTarget.Target )
	{
		CheckViewTarget(PendingViewTarget);
		if( PendingViewTarget.Target )
		{
			return PendingViewTarget.Target;
		}
	}

	CheckViewTarget(ViewTarget);
	return ViewTarget.Target;
}


UBOOL ACamera::PlayerControlled()
{
	return (PCOwner != NULL);
}


void ACamera::ApplyCameraModifiers(FLOAT DeltaTime, FTPOV& OutPOV)
{
	// Loop through each camera modifier
	for( INT ModifierIdx = 0; ModifierIdx < ModifierList.Num(); ++ModifierIdx )
	{
		// Apply camera modification and output into DesiredCameraOffset/DesiredCameraRotation
		if( ModifierList(ModifierIdx) != NULL &&
			!ModifierList(ModifierIdx)->IsDisabled() )
		{
			// If ModifyCamera returns true, exit loop
			// Allows high priority things to dictate if they are
			// the last modifier to be applied
			if( ModifierList(ModifierIdx)->ModifyCamera(this, DeltaTime, OutPOV) )
			{
				break;
			}
		}
	}

	// Now apply CameraAnims
	// these essentially behave as the highest-pri modifier.

	// apply each camera anim
	for (INT Idx=0; Idx<ActiveAnims.Num(); ++Idx)
	{
		UCameraAnimInst* const AnimInst = ActiveAnims(Idx);

		if (!AnimInst->bFinished)
		{
			// clear out animated camera actor
			InitTempCameraActor(AnimCameraActor, AnimInst->CamAnim);

			// evaluate the animation at the new time
			AnimInst->AdvanceAnim(DeltaTime, FALSE);

			if (!PCOwner->bBlockCameraAnimsFromOverridingPostProcess)
			{
				// store PP settings in the inst for later application
				AnimInst->LastPPSettings = AnimCameraActor->CamOverridePostProcess;
				AnimInst->LastPPSettingsAlpha = AnimCameraActor->CamOverridePostProcessAlpha;
			}

			// Add weighted properties to the accumulator actor
			if (AnimInst->CurrentBlendWeight > 0.f)
			{
				ApplyAnimToCamera(AnimCameraActor, AnimInst, OutPOV);
			}

#if !FINAL_RELEASE
			if (PCOwner->bDebugCameraAnims)
			{
				WorldInfo->AddOnScreenDebugMessage((QWORD)AnimInst, 1.0f, FColor(255,255,255), FString::Printf(TEXT("%s: CurrentBlendWeight: %f CurTime: %f"), *AnimInst->CamAnim->GetName(), AnimInst->CurrentBlendWeight, AnimInst->CurTime));

				// debug information
				if (AnimInst->LastCameraLoc.IsZero()==FALSE)
				{
					// draw persistent line
					WorldInfo->DrawDebugLine(AnimInst->LastCameraLoc, OutPOV.Location, 0, 150, 0, TRUE);
					WorldInfo->DrawDebugLine(OutPOV.Location, OutPOV.Location+OutPOV.Rotation.Vector()*10.f, 150, 150, 0, TRUE);

					FVector BoxExtent(30, 30, 100);
					if (PCOwner->Pawn)
					{
						if (PCOwner->Pawn->CylinderComponent)
						{
							BoxExtent = FVector(PCOwner->Pawn->CylinderComponent->CollisionRadius, PCOwner->Pawn->CylinderComponent->CollisionRadius, PCOwner->Pawn->CylinderComponent->CollisionHeight );
						}

						if (PCOwner->Pawn->Mesh)
						{
							// if not first time, draw small coordinate of the roation on the root location
							FBoneAtom RootBA = PCOwner->Pawn->Mesh->GetBoneAtom(0);
							FVector RootLocation = RootBA.GetOrigin()+FVector(0, 0, BoxExtent.Z);
							FRotator RootRotation = RootBA.GetRotation().Rotator();
							//WorldInfo->DrawDebugSphere(RootLocation, 10, 10, 150, 150, 150, TRUE);
							WorldInfo->DrawDebugCoordinateSystem(RootLocation, RootRotation, 10,  TRUE);
						}
					}
				}
				else if (PCOwner->Pawn)
				{
					// initial location
					FVector BoxExtent(30, 30, 100);
					if (PCOwner->Pawn->CylinderComponent)
					{
						BoxExtent = FVector(PCOwner->Pawn->CylinderComponent->CollisionRadius, PCOwner->Pawn->CylinderComponent->CollisionRadius, PCOwner->Pawn->CylinderComponent->CollisionHeight );
					}

					WorldInfo->DrawDebugBox(PCOwner->Pawn->Location, BoxExtent, 255, 255, 0, TRUE);
					WorldInfo->DrawDebugCoordinateSystem(PCOwner->Pawn->Location, PCOwner->Pawn->Rotation, 50,  TRUE);
				}

				AnimInst->LastCameraLoc = OutPOV.Location;
			}
#endif
		}

		// handle animations that have finished
		if (AnimInst->bFinished && AnimInst->bAutoReleaseWhenFinished)
		{
			ReleaseCameraAnimInst(AnimInst);
			Idx--;		// we removed this from the ActiveAnims array
		}

		// changes to this are good for a single update, so reset this to 1.f after processing
		AnimInst->TransientScaleModifier = 1.f;
	}

	// need to zero this when we are done with it.  playing another animation
	// will calc a new InitialTM for the move track instance based on these values.
	AnimCameraActor->Location = FVector::ZeroVector;
	AnimCameraActor->Rotation = FRotator::ZeroRotator;
}

void ACamera::ApplyAnimToCamera(class ACameraActor const* AnimatedCamActor, class UCameraAnimInst const* AnimInst, FTPOV& OutPOV)
{
	FLOAT const Scale = AnimInst->CurrentBlendWeight;

	FRotationMatrix const CameraToWorld(OutPOV.Rotation);

	if (AnimInst->PlaySpace == CAPS_CameraLocal)
	{
		// the code in the else block will handle this just fine, but this path provides efficiency and simplicity for the most common case

		// loc
		FVector const LocalOffset = CameraToWorld.TransformNormal( AnimatedCamActor->Location*Scale );
		OutPOV.Location += LocalOffset;

		// rot
		FRotationMatrix const AnimRotMat( AnimatedCamActor->Rotation*Scale );
		OutPOV.Rotation = (AnimRotMat * CameraToWorld).Rotator();
	}
	else
	{
		// handle playing the anim in an arbitrary space relative to the camera

		// find desired space
		FMatrix const PlaySpaceToWorld = (AnimInst->PlaySpace == CAPS_UserDefined) ? AnimInst->UserPlaySpaceMatrix : FMatrix::Identity; 

		// loc
		FVector const LocalOffset = PlaySpaceToWorld.TransformNormal( AnimatedCamActor->Location*Scale );
		OutPOV.Location += LocalOffset;

		// rot
		// find transform from camera to the "play space"
		FMatrix const CameraToPlaySpace = CameraToWorld * PlaySpaceToWorld.InverseSafe();	// CameraToWorld * WorldToPlaySpace

		// find transform from anim (applied in playspace) back to camera
		FRotationMatrix const AnimToPlaySpace(AnimatedCamActor->Rotation*Scale);
		FMatrix const AnimToCamera = AnimToPlaySpace * CameraToPlaySpace.InverseSafe();			// AnimToPlaySpace * PlaySpaceToCamera

		// RCS = rotated camera space, meaning camera space after it's been animated
		// this is what we're looking for, the diff between rotated cam space and regular cam space.
		// apply the transform back to camera space from the post-animated transform to get the RCS
		FMatrix const RCSToCamera = CameraToPlaySpace * AnimToCamera;

		// now apply to real camera
		FRotationMatrix const RealCamToWorld(OutPOV.Rotation);
		OutPOV.Rotation = (RCSToCamera * RealCamToWorld).Rotator();
	}

	// fov
	ACameraActor const* const DefaultCamActor = ACameraActor::StaticClass()->GetDefaultObject<ACameraActor>();
	if (DefaultCamActor)
	{
		OutPOV.FOV += (AnimatedCamActor->FOVAngle - DefaultCamActor->FOVAngle) * Scale;
	}
}

/** Returns an available CameraAnimInst, or NULL if no more are available. */
UCameraAnimInst* ACamera::AllocCameraAnimInst()
{
	UCameraAnimInst* FreeAnim = (FreeAnims.Num() > 0) ? FreeAnims.Pop() : NULL;
	if (FreeAnim)
	{
		UCameraAnimInst const* const DefaultInst = UCameraAnimInst::StaticClass()->GetDefaultObject<UCameraAnimInst>();

		ActiveAnims.Push(FreeAnim);

		// reset some defaults
		if (DefaultInst)
		{
			FreeAnim->TransientScaleModifier = DefaultInst->TransientScaleModifier;
			FreeAnim->PlaySpace = DefaultInst->PlaySpace;
		}

		FreeAnim->SourceAnimNode = NULL;

		// make sure any previous anim has been terminated correctly
		check( (FreeAnim->MoveTrack == NULL) && (FreeAnim->MoveInst == NULL) && (FreeAnim->SourceAnimNode == NULL) );
	}

	return FreeAnim;
}

/** Returns an available CameraAnimInst, or NULL if no more are available. */
void ACamera::ReleaseCameraAnimInst(UCameraAnimInst* Inst)
{	
	ActiveAnims.RemoveItem(Inst);
	FreeAnims.Push(Inst);
}

/** Returns first existing instance of the specified camera anim, or NULL if none exists. */
UCameraAnimInst* ACamera::FindExistingCameraAnimInst(UCameraAnim const* Anim)
{
	INT const NumActiveAnims = ActiveAnims.Num();
	for (INT Idx=0; Idx<NumActiveAnims; Idx++)
	{
		if (ActiveAnims(Idx)->CamAnim == Anim)
		{
			return ActiveAnims(Idx);
		}
	}

	return NULL;
}


/** Play the indicated CameraAnim on this camera.  Returns the CameraAnim instance, which can be stored to manipulate/stop the anim after the fact. */
UCameraAnimInst* ACamera::PlayCameraAnim(class UCameraAnim* Anim, FLOAT Rate, FLOAT Scale, FLOAT BlendInTime, FLOAT BlendOutTime, UBOOL bLoop, UBOOL bRandomStartTime, FLOAT Duration, UBOOL bSingleInstance)
{
	if (bSingleInstance)
	{
		// try to update the existing instance with the new data
		UCameraAnimInst* ExistingInst = FindExistingCameraAnimInst(Anim);
		if (ExistingInst)
		{
			ExistingInst->Update(Rate, Scale, BlendInTime, BlendOutTime, Duration);
			return ExistingInst;
		}
	}

	// get a new instance and play it
	UCameraAnimInst* const Inst = AllocCameraAnimInst();
	if (Inst)
	{
		// clear LastCameraLoc
		Inst->LastCameraLoc = FVector::ZeroVector;
		Inst->Play(Anim, AnimCameraActor, Rate, Scale, BlendInTime, BlendOutTime, bLoop, bRandomStartTime, Duration);
		return Inst;
	}

	return NULL;
}

/** Stops all instances of the given CameraAnim from playing. */
void ACamera::StopAllCameraAnimsByType(class UCameraAnim* Anim, UBOOL bImmediate)
{
	// find cameraaniminst for this.
	for (INT Idx=0; Idx<ActiveAnims.Num(); ++Idx)
	{
		if (ActiveAnims(Idx)->CamAnim == Anim)
		{
			ActiveAnims(Idx)->Stop(bImmediate);
		}
	}
}

/** Stops all instances of the given CameraAnim from playing. */
void ACamera::StopAllCameraAnims(UBOOL bImmediate)
{
	for (INT Idx=0; Idx<ActiveAnims.Num(); ++Idx)
	{
		ActiveAnims(Idx)->Stop(bImmediate);
	}
}

/** Stops the given CameraAnim instances from playing.  The given pointer should be considered invalid after this. */
void ACamera::StopCameraAnim(class UCameraAnimInst* AnimInst, UBOOL bImmediate)
{
	if (AnimInst != NULL)
	{
		AnimInst->Stop(bImmediate);
	}
}


/** Gets specified temporary CameraActor ready to update the specified Anim. */
void ACamera::InitTempCameraActor(class ACameraActor* CamActor, class UCameraAnim* AnimToInitFor) const
{
	if (CamActor)
	{
		CamActor->Location = FVector::ZeroVector;
		CamActor->Rotation = FRotator::ZeroRotator;

		if (AnimToInitFor)
		{
			ACameraActor const* const DefaultCamActor = ACameraActor::StaticClass()->GetDefaultObject<ACameraActor>();
			if (DefaultCamActor)
			{
				CamActor->AspectRatio = DefaultCamActor->AspectRatio;
				CamActor->FOVAngle = AnimToInitFor->BaseFOV;
				CamActor->DrawScale = DefaultCamActor->DrawScale;
				CamActor->DrawScale3D = DefaultCamActor->DrawScale3D;

				CamActor->CamOverridePostProcess = AnimToInitFor->BasePPSettings;
				CamActor->CamOverridePostProcessAlpha = AnimToInitFor->BasePPSettingsAlpha;
			}
		}
	}
}

void ACamera::ModifyPostProcessSettings(FPostProcessSettings& PPSettings) const
{
	// common for this to be empty, should be worth the check
	if (ActiveAnims.Num() > 0)
	{
		// apply in order from oldest to youngest anim.  assumes ActiveAnims is sorted that way
		for (TArray<UCameraAnimInst*>::TConstIterator It(ActiveAnims); It; ++It)
		{
			UCameraAnimInst* const AnimInst = *It;
			if (AnimInst)
			{
				// apply
				AnimInst->LastPPSettings.OverrideSettingsFor(PPSettings, AnimInst->LastPPSettingsAlpha * AnimInst->CurrentBlendWeight);
			}
		}
	}
}

/** Apply audio fading. */
void ACamera::ApplyAudioFade()
{
	if (GEngine && GEngine->GetAudioDevice() )
	{
		GEngine->GetAudioDevice()->TransientMasterVolume = 1.0 - FadeAmount;
	}
}

/*------------------------------------------------------------------------------
	UCameraModifier
------------------------------------------------------------------------------*/

UBOOL UCameraModifier::ModifyCamera(class ACamera* Camera,FLOAT DeltaTime,FTPOV& OutPOV)
{
	// If pending disable and fully alpha'd out, truly disable this modifier
	if( bPendingDisable && Alpha <= 0.0 )
	{
		eventDisableModifier(TRUE);
	}

	return FALSE;
}

FLOAT UCameraModifier::GetTargetAlpha(class ACamera* Camera )
{
	if( bPendingDisable )
	{
		return 0.0f;
	}
	return 1.0f;		
}

void UCameraModifier::UpdateAlpha(class ACamera* Camera,FLOAT DeltaTime)
{
	FLOAT Time;

	TargetAlpha = GetTargetAlpha( Camera );

	// Alpha out
	if( TargetAlpha == 0.0 )
	{
		Time = AlphaOutTime;
	}
	else
	{
		// Otherwise, alpha in
		Time = AlphaInTime;
	}

	// interpolate!
	if( Time <= 0.0 )
	{
		Alpha = TargetAlpha;
	}
	else if( Alpha > TargetAlpha )
	{
		Alpha = Max<FLOAT>( Alpha - (DeltaTime * (1.0 / Time)), TargetAlpha );
	}
	else
	{
		Alpha = Min<FLOAT>( Alpha + (DeltaTime * (1.0 / Time)), TargetAlpha );
	}
}


UBOOL UCameraModifier::IsDisabled() const
{
	return bDisabled;
}


/*------------------------------------------------------------------------------
	UCameraModifier_CameraShake
------------------------------------------------------------------------------*/

/** For situational scaling of individual shakes. */
FLOAT UCameraModifier_CameraShake::GetShakeScale(FCameraShakeInstance const& ShakeInst) const
{
	return ShakeInst.Scale * Alpha;
}

static inline FLOAT UpdateFOscillator(FFOscillator const& Osc, FLOAT& CurrentOffset, FLOAT DeltaTime)
{
	if (Osc.Amplitude != 0.f)
	{
		CurrentOffset += DeltaTime * Osc.Frequency;
		return Osc.Amplitude * appSin(CurrentOffset);
	}
	return 0.f;
}

void UCameraModifier_CameraShake::UpdateCameraShake(FLOAT DeltaTime, FCameraShakeInstance& Shake, FTPOV& OutPOV)
{
	if (!Shake.SourceShake)
	{
		return;
	}

	// this is the base scale for the whole shake, anim and oscillation alike
	FLOAT const BaseShakeScale = GetShakeScale(Shake);
	// do not update if percentage is null
	if (BaseShakeScale <= 0.f)
	{
		return;
	}

	// update anims with any desired scaling
	if (Shake.AnimInst)
	{
		Shake.AnimInst->TransientScaleModifier *= BaseShakeScale;
	}


	// update oscillation times
 	if (Shake.OscillatorTimeRemaining > 0.f)
 	{
 		Shake.OscillatorTimeRemaining -= DeltaTime;
 		Shake.OscillatorTimeRemaining = Max(0.f, Shake.OscillatorTimeRemaining);
 	}
	if (Shake.bBlendingIn)
	{
		Shake.CurrentBlendInTime += DeltaTime;
	}
	if (Shake.bBlendingOut)
	{
		Shake.CurrentBlendOutTime += DeltaTime;
	}

	// see if we've crossed any important time thresholds and deal appropriately
	UBOOL bOscillationFinished = FALSE;
	FLOAT const Duration = Shake.SourceShake->OscillationDuration;

	if (Shake.OscillatorTimeRemaining == 0.f)
	{
		// finished!
		bOscillationFinished = TRUE;
	}
	else if (Shake.OscillatorTimeRemaining < Shake.SourceShake->OscillationBlendOutTime)
	{
		// start blending out
		Shake.bBlendingOut = TRUE;
		Shake.CurrentBlendOutTime = Shake.SourceShake->OscillationBlendOutTime - Shake.OscillatorTimeRemaining;
	}

	if (Shake.bBlendingIn)
	{
		if (Shake.CurrentBlendInTime > Shake.SourceShake->OscillationBlendInTime)
		{
			// done blending in!
			Shake.bBlendingIn = FALSE;
		}
	}
	if (Shake.bBlendingOut)
	{
		if (Shake.CurrentBlendOutTime > Shake.SourceShake->OscillationBlendOutTime)
		{
			// done!!
			Shake.CurrentBlendOutTime = Shake.SourceShake->OscillationBlendOutTime;
			bOscillationFinished = TRUE;
		}
	}

	// calculate blend weight. calculating separately and taking the minimum handles overlapping blends nicely.
	FLOAT const BlendInWeight = (Shake.bBlendingIn) ? (Shake.CurrentBlendInTime / Shake.SourceShake->OscillationBlendInTime) : 1.f;
	FLOAT const BlendOutWeight = (Shake.bBlendingOut) ? (1.f - Shake.CurrentBlendOutTime / Shake.SourceShake->OscillationBlendOutTime) : 1.f;
	FLOAT const CurrentBlendWeight = ::Min(BlendInWeight, BlendOutWeight);


	// Do not update oscillation further if finished
	if (bOscillationFinished)
	{
		return;
	}

	// this is the oscillation scale, which includes oscillation fading
	FLOAT const OscillationScale = GetShakeScale(Shake) * CurrentBlendWeight;

	if (OscillationScale > 0.f)
	{
		// View location offset, compute sin wave value for each component
		FVector	LocOffset = FVector(0);
		LocOffset.X = UpdateFOscillator(Shake.SourceShake->LocOscillation.X, Shake.LocSinOffset.X, DeltaTime);
		LocOffset.Y = UpdateFOscillator(Shake.SourceShake->LocOscillation.Y, Shake.LocSinOffset.Y, DeltaTime);
		LocOffset.Z = UpdateFOscillator(Shake.SourceShake->LocOscillation.Z, Shake.LocSinOffset.Z, DeltaTime);
		LocOffset *= OscillationScale;

		// View rotation offset, compute sin wave value for each component
		FRotator RotOffset;
		RotOffset.Pitch = appTrunc( UpdateFOscillator(Shake.SourceShake->RotOscillation.Pitch, Shake.RotSinOffset.X, DeltaTime) * OscillationScale );
		RotOffset.Yaw = appTrunc( UpdateFOscillator(Shake.SourceShake->RotOscillation.Yaw, Shake.RotSinOffset.Y, DeltaTime) * OscillationScale );
		RotOffset.Roll = appTrunc( UpdateFOscillator(Shake.SourceShake->RotOscillation.Roll, Shake.RotSinOffset.Z, DeltaTime) * OscillationScale );

		if (Shake.PlaySpace == CAPS_CameraLocal)
		{
			// the else case will handle this as well, but this is the faster, cleaner, most common code path

			// apply loc offset relative to camera orientation
			FRotationMatrix CamRotMatrix(OutPOV.Rotation);
			OutPOV.Location += CamRotMatrix.TransformNormal(LocOffset);

			// apply rot offset relative to camera orientation
			FRotationMatrix const AnimRotMat( RotOffset );
			OutPOV.Rotation = (AnimRotMat * FRotationMatrix(OutPOV.Rotation)).Rotator();
		}
		else
		{
			// find desired space
			FMatrix const PlaySpaceToWorld = (Shake.PlaySpace == CAPS_UserDefined) ? Shake.UserPlaySpaceMatrix : FMatrix::Identity; 

			// apply loc offset relative to desired space
			OutPOV.Location += PlaySpaceToWorld.TransformNormal( LocOffset );

			// apply rot offset relative to desired space

			// find transform from camera to the "play space"
			FRotationMatrix const CamToWorld(OutPOV.Rotation);
			FMatrix const CameraToPlaySpace = CamToWorld * PlaySpaceToWorld.InverseSafe();	// CameraToWorld * WorldToPlaySpace

			// find transform from anim (applied in playspace) back to camera
			FRotationMatrix const AnimToPlaySpace(RotOffset);
			FMatrix const AnimToCamera = AnimToPlaySpace * CameraToPlaySpace.InverseSafe();			// AnimToPlaySpace * PlaySpaceToCamera

			// RCS = rotated camera space, meaning camera space after it's been animated
			// this is what we're looking for, the diff between rotated cam space and regular cam space.
			// apply the transform back to camera space from the post-animated transform to get the RCS
			FMatrix const RCSToCamera = CameraToPlaySpace * AnimToCamera;

			// now apply to real camera
			OutPOV.Rotation = (RCSToCamera * CamToWorld).Rotator();
		}

		// Compute FOV change
		OutPOV.FOV += OscillationScale * UpdateFOscillator(Shake.SourceShake->FOVOscillation, Shake.FOVSinOffset, DeltaTime);
	}
}

UBOOL UCameraModifier_CameraShake::ModifyCamera(class ACamera* Camera,FLOAT DeltaTime,FTPOV& OutPOV)
{
	// Update the alpha
	UpdateAlpha(Camera, DeltaTime);

	// Call super where modifier may be disabled
	Super::ModifyCamera(Camera, DeltaTime, OutPOV);

	// If no alpha, exit early
	if( Alpha <= 0.f )
	{
		return FALSE;
	}

	// Update Screen Shakes array
	if( ActiveShakes.Num() > 0 )
	{
		for(INT i=0; i<ActiveShakes.Num(); i++)
		{
			UpdateCameraShake(DeltaTime, ActiveShakes(i), OutPOV);
		}

		// Delete any obsolete shakes
		for(INT i=ActiveShakes.Num()-1; i>=0; i--)
		{
			FCameraShakeInstance& ShakeInst = ActiveShakes(i);
			if( !ShakeInst.SourceShake
				|| ( (ShakeInst.OscillatorTimeRemaining == 0.f) && 
					 ((ShakeInst.AnimInst == NULL) || (ShakeInst.AnimInst->bFinished == TRUE)) ) )
			{
				ActiveShakes.Remove(i,1);
			}
		}
	}

	// If ModifyCamera returns true, exit loop
	// Allows high priority things to dictate if they are
	// the last modifier to be applied
	// Returning TRUE causes to stop adding another modifier! 
	// Returning FALSE is the right behavior since this is not high priority modifier.
	return FALSE;
}


/*------------------------------------------------------------------------------
	ACameraActor
------------------------------------------------------------------------------*/

/** 
 *	Use to assign the camera static mesh to the CameraActor used in the editor.
 *	Because HiddenGame is true and CollideActors is false, the component should be NULL in-game.
 */
void ACameraActor::Spawned()
{
	Super::Spawned();

	// Since camera actors contain only override post-process settings, disable all of 
	// them by default. The user can opt in only the settings they want to override. 
	CamOverridePostProcess.DisableAllOverrides();

	if(MeshComp)
	{
		if( !MeshComp->StaticMesh)
		{
			UStaticMesh* CamMesh = LoadObject<UStaticMesh>(NULL, TEXT("EditorMeshes.MatineeCam_SM"), NULL, LOAD_None, NULL);
			FComponentReattachContext ReattachContext(MeshComp);
			MeshComp->StaticMesh = CamMesh;
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
		DrawFrustum->FrustumAspectRatio = AspectRatio;
	}
}

/** Ensure DrawFrustumComponent is up to date. */
void ACameraActor::UpdateComponentsInternal(UBOOL bCollisionUpdate)
{
	Super::UpdateComponentsInternal(bCollisionUpdate);
	UpdateDrawFrustum();
}

/** Used to push new frustum settings down into preview component when modifying camera through property window. */
void ACameraActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UpdateDrawFrustum();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void ACameraActor::PostLoad()
{
	// update to new variable setup
	if (bCamOverridePostProcess_DEPRECATED)
	{
		CamOverridePostProcessAlpha = 1.f;
		bCamOverridePostProcess_DEPRECATED = FALSE;
		MarkPackageDirty();
	}

	Super::PostLoad();
}

#if WITH_EDITOR
void ACameraActor::CheckForErrors()
{
	Super::CheckForErrors();

	if( AspectRatio == 0 )
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_CameraAspectRatioIsZero" ), *GetName() ) ), TEXT( "CameraAspectRatioIsZero" ) );
	}
}
#endif


/*------------------------------------------------------------------------------
	UCameraAnimInst
------------------------------------------------------------------------------*/

/** Advances animation by DeltaTime.  Changes are applied to the group's actor. */
void UCameraAnimInst::AdvanceAnim(FLOAT DeltaTime, UBOOL bJump)
{
	// check to see if our animnodeseq has been deleted.  not a fan of 
	// polling for this, but we want to stop this immediately, not when
	// GC gets around to cleaning up.
	if (SourceAnimNode)
	{
		if ( (SourceAnimNode->SkelComponent == NULL) || SourceAnimNode->SkelComponent->IsPendingKill() )
		{
			SourceAnimNode = NULL;		// clear this ref so GC can release the node
			Stop(TRUE);
		}
	}


	if ( (CamAnim != NULL) && !bFinished )
	{
		// will set to true if anim finishes this frame
		UBOOL bAnimJustFinished = FALSE;

		FLOAT const ScaledDeltaTime = DeltaTime * PlayRate;

		// find new times
		CurTime += ScaledDeltaTime;
		if (bBlendingIn)
		{
			CurBlendInTime += DeltaTime;
		}
		if (bBlendingOut)
		{
			CurBlendOutTime += DeltaTime;
		}

		// see if we've crossed any important time thresholds and deal appropriately
		if (bLooping)
		{
			if (CurTime > CamAnim->AnimLength)
			{
				// loop back to the beginning
				CurTime -= CamAnim->AnimLength;
			}
		}
		else
		{
			if (CurTime > CamAnim->AnimLength)
			{
				// done!!
				bAnimJustFinished = TRUE;
			}
			else if (CurTime > (CamAnim->AnimLength - BlendOutTime))
			{
				// start blending out
				bBlendingOut = TRUE;
				CurBlendOutTime = CurTime - (CamAnim->AnimLength - BlendOutTime);
			}
		}

		if (bBlendingIn)
		{
			if (CurBlendInTime > BlendInTime)
			{
				// done blending in!
				bBlendingIn = FALSE;
			}
		}
		if (bBlendingOut)
		{
			if (CurBlendOutTime > BlendOutTime)
			{
				// done!!
				CurBlendOutTime = BlendOutTime;
				bAnimJustFinished = TRUE;
			}
		}
		
		// calculate blend weight. calculating separately and taking the minimum handles overlapping blends nicely.
		{
			FLOAT BlendInWeight = (bBlendingIn) ? (CurBlendInTime / BlendInTime) : 1.f;
			FLOAT BlendOutWeight = (bBlendingOut) ? (1.f - CurBlendOutTime / BlendOutTime) : 1.f;
			CurrentBlendWeight = ::Min(BlendInWeight, BlendOutWeight) * BasePlayScale * TransientScaleModifier;
		}

		// this will update tracks and apply the effects to the group actor (except move tracks)
		InterpGroupInst->Group->UpdateGroup(CurTime, InterpGroupInst, FALSE, bJump);

		// UpdateGroup won't handle the movement track, need to deal with it separately.
		AActor* const GroupActor = InterpGroupInst->GetGroupActor();
		if (GroupActor != NULL && MoveTrack != NULL && MoveInst != NULL)
		{
			GroupActor->MoveWithInterpMoveTrack(MoveTrack, MoveInst, CurTime, DeltaTime);
		}

		if (bAnimJustFinished)
		{
			// completely finished
			Stop(TRUE);
		}
		else if (RemainingTime > 0.f)
		{
			// handle any specified duration
			RemainingTime -= DeltaTime;
			if (RemainingTime <= 0.f)
			{
				// stop with blend out
				Stop();
			}
		}
	}
}

/** Updates this active instance with new parameters. */
void UCameraAnimInst::Update(FLOAT NewRate, FLOAT NewScale, FLOAT NewBlendInTime, FLOAT NewBlendOutTime, FLOAT NewDuration)
{
	if (bBlendingOut)
	{
		bBlendingOut = FALSE;
		CurBlendOutTime = 0.f;

		// stop any blendout and reverse it to a blendin
		bBlendingIn = TRUE;
		CurBlendInTime = NewBlendInTime * (1.f - CurBlendOutTime / BlendOutTime);
	}

	PlayRate = NewRate;
	BasePlayScale = NewScale;
	BlendInTime = NewBlendInTime;
	BlendOutTime = NewBlendOutTime;
	RemainingTime = (NewDuration > 0.f) ? (NewDuration - BlendOutTime) : 0.f;
	bFinished = FALSE;
}


/** Starts this instance playing the specified CameraAnim. */
void UCameraAnimInst::Play(class UCameraAnim* Anim, class AActor* CamActor, FLOAT InRate, FLOAT InScale, FLOAT InBlendInTime, FLOAT InBlendOutTime, UBOOL bInLooping, UBOOL bRandomStartTime, FLOAT Duration)
{
	if (Anim && Anim->CameraInterpGroup)
	{
		// make sure any previous anim has been terminated correctly
		Stop(TRUE);

		CurTime = bRandomStartTime ? (appFrand() * Anim->AnimLength) : 0.f;
		CurBlendInTime = 0.f;
		CurBlendOutTime = 0.f;
		bBlendingIn = TRUE;
		bBlendingOut = FALSE;
		bFinished = FALSE;

		// copy properties
		CamAnim = Anim;
		PlayRate = InRate;
		BasePlayScale = InScale;
		BlendInTime = InBlendInTime;
		BlendOutTime = InBlendOutTime;
		bLooping = bInLooping;
		RemainingTime = (Duration > 0.f) ? (Duration - BlendOutTime) : 0.f;

		// init the interpgroup

		if ( CamActor->IsA(ACameraActor::StaticClass()) )
		{
			// ensure CameraActor is zeroed, so RelativeToInitial anims get proper InitialTM
			CamActor->Location = FVector::ZeroVector;
			CamActor->Rotation = FRotator::ZeroRotator;
		}
		InterpGroupInst->InitGroupInst(CamAnim->CameraInterpGroup, CamActor);

		// cache move track refs
		for (INT Idx = 0; Idx < InterpGroupInst->TrackInst.Num(); ++Idx)
		{
			MoveTrack = Cast<UInterpTrackMove>(CamAnim->CameraInterpGroup->InterpTracks(Idx));
			if (MoveTrack != NULL)
			{
				MoveInst = CastChecked<UInterpTrackInstMove>(InterpGroupInst->TrackInst(Idx));
				// only 1 move track per group, so we can bail here
				break;					
			}
		}	
	}
}

/** Stops this instance playing whatever animation it is playing. */
void UCameraAnimInst::Stop(UBOOL bImmediate)
{
	if ( bImmediate || (BlendOutTime <= 0.f) )
	{
		if (InterpGroupInst->Group != NULL)
		{
			InterpGroupInst->TermGroupInst(TRUE);
		}
		MoveTrack = NULL;
		MoveInst = NULL;
		bFinished = TRUE;
		SourceAnimNode = NULL;
 	}
	else
	{
		// start blend out
		bBlendingOut = TRUE;
		CurBlendOutTime = 0.f;
	}
}

void UCameraAnimInst::ApplyTransientScaling(FLOAT Scalar)
{
	TransientScaleModifier *= Scalar;
}

void UCameraAnimInst::RegisterAnimNode(class UAnimNodeSequence* AnimNode)
{
	SourceAnimNode = AnimNode;
}

void UCameraAnimInst::SetPlaySpace(BYTE NewSpace, FRotator UserPlaySpace)
{
	PlaySpace = NewSpace;
	UserPlaySpaceMatrix = (PlaySpace == CAPS_UserDefined) ? FRotationMatrix(UserPlaySpace) : FMatrix::Identity;
}



/*------------------------------------------------------------------------------
	UCameraAnim
------------------------------------------------------------------------------*/

/** 
 * Construct a camera animation from an InterpGroup.  The InterpGroup must control a CameraActor.  
 * Used by the editor to "export" a camera animation from a normal Matinee scene.
 */
UBOOL UCameraAnim::CreateFromInterpGroup(class UInterpGroup* SrcGroup, class USeqAct_Interp* Interp)
{
	// assert we're controlling a camera actor
#if !FINAL_RELEASE
	{
		UInterpGroupInstCamera* GroupInst = Cast<UInterpGroupInstCamera>(Interp->FindFirstGroupInst(SrcGroup));
		if (GroupInst)
		{
			check( GroupInst->GetGroupActor()->IsA(ACameraActor::StaticClass()) );
		}
	}
#endif
	
	// copy length information
	AnimLength = (Interp && Interp->InterpData) ? Interp->InterpData->InterpLength : 0.f;

	UInterpGroupCamera* OldGroup = CameraInterpGroup;

	if (CameraInterpGroup != SrcGroup)
	{
		// copy the source interp group for use in the CameraAnim
		CameraInterpGroup = (UInterpGroupCamera*)UObject::StaticDuplicateObject(SrcGroup, SrcGroup, this, TEXT("None"));

		if (CameraInterpGroup)
		{
			// delete the old one, if it exists
			if (OldGroup)
			{
				OldGroup->MarkPendingKill();
			}

			// success!
			return TRUE;
		}
		else
		{
			// creation of new one failed somehow, restore the old one
			CameraInterpGroup = OldGroup;
		}
	}
	else
	{
		// no need to perform work above, but still a "success" case
		return TRUE;
	}

	// failed creation
	return FALSE;
}


FBox UCameraAnim::GetAABB(FVector const& BaseLoc, FRotator const& BaseRot, FLOAT Scale) const
{
	FRotationTranslationMatrix const BaseTM(BaseRot, BaseLoc);

	FBox ScaledLocalBox = BoundingBox;
	ScaledLocalBox.Min *= Scale;
	ScaledLocalBox.Max *= Scale;

	return ScaledLocalBox.TransformBy(BaseTM);
}


void UCameraAnim::PreSave()
{
#if WITH_EDITORONLY_DATA
	CalcLocalAABB();
#endif // WITH_EDITORONLY_DATA
	Super::PreSave();
}

void UCameraAnim::PostLoad()
{
	if (GIsEditor)
	{
		// update existing CameraAnims' bboxes on load, so editor knows they 
		// they need to be resaved
		if (!BoundingBox.IsValid)
		{
			CalcLocalAABB();
			if (BoundingBox.IsValid)
			{
				MarkPackageDirty();
			}
		}
	}

	// During cooking, reduce keys
	if (GIsCooking)
	{
		if (CameraInterpGroup)
		{
			// find move track
			UInterpTrack *Track = NULL;
			for (INT TrackIdx = 0; TrackIdx < CameraInterpGroup->InterpTracks.Num(); ++TrackIdx)
			{
				Track = CameraInterpGroup->InterpTracks(TrackIdx);

				if (Track)
				{
					FLOAT IntervalStart, IntervalEnd;
					Track->GetTimeRange(IntervalStart, IntervalEnd);
					FLOAT CompressTolerance = CameraInterpGroup->CompressTolerance;
					Track->ReduceKeys(IntervalStart, IntervalEnd, CompressTolerance);
				}
			}
		}
	}

	Super::PostLoad();
}	


void UCameraAnim::CalcLocalAABB()
{
	BoundingBox.Init();

	if (CameraInterpGroup)
	{
		// find move track
		UInterpTrackMove *MoveTrack = NULL;
		for (INT TrackIdx = 0; TrackIdx < CameraInterpGroup->InterpTracks.Num(); ++TrackIdx)
		{
			MoveTrack = Cast<UInterpTrackMove>(CameraInterpGroup->InterpTracks(TrackIdx));
			if (MoveTrack != NULL)
			{
				break;
			}
		}

		if (MoveTrack != NULL)
		{
			FVector Zero(0.f), MinBounds, MaxBounds;
			MoveTrack->PosTrack.CalcBounds(MinBounds, MaxBounds, Zero);
			BoundingBox = FBox(MinBounds, MaxBounds);
		}
	}
}

INT UCameraAnim::GetResourceSize()
{
	FArchiveCountMem CountBytesSize(this);
	INT ResourceSize = CountBytesSize.GetNum();

	if (CameraInterpGroup)
	{
		// find move track
		UInterpTrackMove *MoveTrack = NULL;
		for (INT TrackIdx = 0; TrackIdx < CameraInterpGroup->InterpTracks.Num(); ++TrackIdx)
		{
			// somehow movement track's not calculated when you just used serialize, so I'm adding it here. 
			MoveTrack = Cast<UInterpTrackMove>(CameraInterpGroup->InterpTracks(TrackIdx));
			if (MoveTrack)
			{
				FArchiveCountMem CountBytesSize(MoveTrack);
				ResourceSize += CountBytesSize.GetNum();
			}
		}
	}

	return ResourceSize;
}
