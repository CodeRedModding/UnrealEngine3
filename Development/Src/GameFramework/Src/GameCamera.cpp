/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "GameFramework.h"

#include "GameFrameworkCameraClasses.h"


IMPLEMENT_CLASS(UGameCameraBase)
IMPLEMENT_CLASS(AGamePlayerCamera)
IMPLEMENT_CLASS(UGameThirdPersonCamera)
IMPLEMENT_CLASS(UGameThirdPersonCameraMode)
IMPLEMENT_CLASS(UGameThirdPersonCameraMode_Default)
IMPLEMENT_CLASS(AGameCameraBlockingVolume)

/*-----------------------------------------------------------------------------
	 Helpers
-----------------------------------------------------------------------------*/

/**
 *	Returns world space heading angle (in radians) of given vector
 *
 *	@param	Dir		Vector to be converted into heading angle
 */
static FLOAT GetHeadingAngle( FVector const& Dir )
{
	FLOAT Angle = appAcos( Clamp( Dir.X, -1.f, 1.f ) );
	if( Dir.Y < 0.f )
	{
		Angle *= -1.f;
	}

	return Angle;
}

/**
 * Internal.  Just like the regular RInterpTo(), but with per-axis interpolation speeds specification
 */
static FRotator RInterpToWithPerAxisSpeeds( FRotator const& Current, FRotator const& Target, FLOAT DeltaTime, FLOAT PitchInterpSpeed, FLOAT YawInterpSpeed, FLOAT RollInterpSpeed )
{
	// if DeltaTime is 0, do not perform any interpolation (Location was already calculated for that frame)
	if( DeltaTime == 0.f || Current == Target )
	{
		return Current;
	}

	// Delta Move, Clamp so we do not over shoot.
	FRotator DeltaMove = (Target - Current).GetNormalized();

	DeltaMove.Pitch = appTrunc(DeltaMove.Pitch * Clamp(DeltaTime * PitchInterpSpeed, 0.f, 1.f));
	DeltaMove.Yaw = appTrunc(DeltaMove.Yaw * Clamp(DeltaTime * YawInterpSpeed, 0.f, 1.f));
	DeltaMove.Roll = appTrunc(DeltaMove.Roll * Clamp(DeltaTime * RollInterpSpeed, 0.f, 1.f));

	return (Current + DeltaMove).GetNormalized();
}

/** Shorthand vector transforms. */
static inline FVector TransformWorldToLocal(FVector const& WorldVect, FRotator const& SystemRot)
{
	//return FRotationMatrix(SystemRot).Transpose().TransformNormal( WorldVect );
	return FRotationMatrix(SystemRot).InverseTransformNormal( WorldVect );
}
static inline FVector TransformLocalToWorld(FVector const& LocalVect, FRotator const& SystemRot)
{
	return FRotationMatrix(SystemRot).TransformNormal( LocalVect );
}

static inline FLOAT FPctByRange( FLOAT Value, FLOAT InMin, FLOAT InMax )
{
	return (Value - InMin) / (InMax - InMin);
}

static inline FLOAT DegToUnrRotatorUnits(FLOAT InDeg)
{
	return InDeg * 182.0444f;
}
static inline FLOAT RadToUnrRotatorUnits(FLOAT InDeg)
{
	return InDeg * 180.f / PI * 182.0444f;
}

/*-----------------------------------------------------------------------------
	AGamePlayerCamera
-----------------------------------------------------------------------------*/

// in native code so we can use LocalToWorld()
void AGamePlayerCamera::CacheLastTargetBaseInfo(AActor* Base)
{
	LastTargetBase = Base;
	if (Base)
	{
		LastTargetBaseTM = Base->LocalToWorld();
	}
}


/** 
* Given a horizontal FOV that assumes a 16:9 viewport, return an appropriately
* adjusted FOV for the viewport of the target Pawn->  Maintains constant vertical FOV.
* Used to correct for splitscreen.
*/
FLOAT AGamePlayerCamera::AdjustFOVForViewport(FLOAT inHorizFOV, APawn* CameraTargetPawn) const
{
	FLOAT OutFOV = inHorizFOV;

	if (CameraTargetPawn)
	{
		APlayerController* const PlayerOwner = Cast<APlayerController>(CameraTargetPawn->Controller);
		ULocalPlayer* const LP = (PlayerOwner != NULL) ? Cast<ULocalPlayer>(PlayerOwner->Player) : NULL;
		UGameViewportClient* const VPClient = (LP != NULL) ? LP->ViewportClient : NULL;
		if ( VPClient && (VPClient->GetCurrentSplitscreenType() == eSST_2P_VERTICAL) )
		{
			FVector2D FullViewportSize(0,0);
			VPClient->GetViewportSize(FullViewportSize);

			FLOAT const BaseAspect =  FullViewportSize.X / FullViewportSize.Y;
			UBOOL bWideScreen = FALSE;
			if ( (BaseAspect > (16.f/9.f-0.01f)) && (BaseAspect < (16.f/9.f+0.01f)) )
			{
				bWideScreen = TRUE;
			}

			// find actual size of player's viewport
			FVector2D PlayerViewportSize;
			PlayerViewportSize.X = FullViewportSize.X * LP->Size.X;
			PlayerViewportSize.Y = FullViewportSize.Y * LP->Size.Y;

			// calculate new horizontal fov
			FLOAT NewAspectRatio = PlayerViewportSize.X / PlayerViewportSize.Y;
			OutFOV = (NewAspectRatio / BaseAspect /*AspectRatio16to9*/) * appTan(inHorizFOV * 0.5f * PI / 180.f);
			OutFOV = 2.f * appAtan(OutFOV) * 180.f / PI;
		}
	}

	return OutFOV;
}



/** Add a pawn from the hidden actor's array */
void AGamePlayerCamera::AddPawnToHiddenActorsArray( APawn *PawnToHide )
{
	if ( PawnToHide && PCOwner )
	{
		PCOwner->HiddenActors.AddUniqueItem(PawnToHide);
	}
}

/** Remove a pawn from the hidden actor's array */
void AGamePlayerCamera::RemovePawnFromHiddenActorsArray( APawn *PawnToShow )
{
	if ( PawnToShow && PCOwner )
	{
		PCOwner->HiddenActors.RemoveItem(PawnToShow);
	}
}


void AGamePlayerCamera::ModifyPostProcessSettings(FPostProcessSettings& PPSettings) const
{
	Super::ModifyPostProcessSettings(PPSettings);

	// allow the current camera to do what it wants
	if (CurrentCamera)
	{
		CurrentCamera->eventModifyPostProcessSettings(PPSettings);
	}
}

/*-----------------------------------------------------------------------------
	UGameThirdPersonCamera
-----------------------------------------------------------------------------*/

/** Interpolate a normal vector Current to Target, by interpolating the angle between those vectors with constant step. */
// FVector VInterpRotationTo(const FVector& Current, const FVector& Target, FLOAT DeltaTime, FLOAT InterpSpeed)
// {
// 	// Find delta rotation between both normals.
// 	FVector CurrentNorm = Current.UnsafeNormal();
// 	FVector TargetNorm = Target.UnsafeNormal();
// 
// 	FQuat DeltaQuat = FQuatFindBetween(CurrentNorm, TargetNorm);
// 
// 	// Decompose into an axis and angle for rotation
// 	FVector DeltaAxis(0.f);
// 	FLOAT TotalDeltaAngle = 0.f;
// 	DeltaQuat.ToAxisAndAngle(DeltaAxis, TotalDeltaAngle);
// 
// 	FLOAT DeltaAngle = TotalDeltaAngle * Clamp<FLOAT>(DeltaTime * InterpSpeed, 0.f, 1.f);
// 	DeltaQuat = FQuat(DeltaAxis, DeltaAngle);
// 	DeltaQuat.Normalize();
// 
// //	ResultDir = DeltaQuat.RotateVector(Current);
// 	FVector ResultDir = FQuatRotationTranslationMatrix( DeltaQuat, FVector::ZeroVector ).TransformNormal(CurrentNorm);
// 	ResultDir.Normalize();
// 
// 	FLOAT ResultMag = FInterpTo(Current.Size(), Target.Size(), DeltaTime, InterpSpeed);
// 	return ResultDir * ResultMag;
// }

#define DEBUGCAMERA_PENETRATION 0
void UGameThirdPersonCamera::PlayerUpdateCamera(APawn* P, AGamePlayerCamera* CameraActor, FLOAT DeltaTime, FTViewTarget& OutVT)
{
	// this can legitimately be NULL if the target isn't a GamePawn (such as a vehicle or turret)
	AGamePawn* const GP = Cast<AGamePawn>(P);

#if !FINAL_RELEASE
	if( bDebugChangedCameraMode )
	{
		bDebugChangedCameraMode = FALSE;
	}
#endif


	// Get pos/rot for camera origin (base location where offsets, etc are applied from)
	// handle a moving base, if necessary
	UpdateForMovingBase(P->Base);

	FRotator	IdealCameraOriginRot;
	FVector		IdealCameraOrigin;
	CurrentCamMode->GetCameraOrigin(P, IdealCameraOrigin, IdealCameraOriginRot);

	FVector ActualCameraOrigin;
	FRotator ActualCameraOriginRot;
	InterpolateCameraOrigin( P, DeltaTime, ActualCameraOrigin, IdealCameraOrigin, ActualCameraOriginRot, IdealCameraOriginRot );
	
	LastIdealCameraOrigin = IdealCameraOrigin;
	LastIdealCameraOriginRot = IdealCameraOriginRot;
	LastActualCameraOrigin = ActualCameraOrigin;
	LastActualCameraOriginRot = ActualCameraOriginRot;
	
	// special case: add any extra offset that needs to be applied after the interp
	// note: this should happen after saving the last origin, since it's a modifier applied independent of the interpolation
	ActualCameraOrigin += GetPostInterpCameraOriginLocationOffset(P);
	ActualCameraOriginRot += GetPostInterpCameraOriginRotationOffset(P);

	//debugfSuppressed(NAME_DevCamera, TEXT("%s.PlayerUpdateCamera: CamMode %s, IdealOrigin %s PawnLoc %s"), *GetName(), *CurrentCamMode->GetName(), *IdealCameraOrigin.ToString(), *P->Location.ToString());

	// do any pre-viewoffset focus point adjustment
	eventUpdateFocusPoint(P);

	// doing adjustment before view offset application in order to rotate around target
	// also doing before origin offset application to avoid pops in cover
	// using last viewoffset here, since we have a circular dependency with the data.  focus point adjustment
	// needs viewoffset, but viewoffset is dependent on results of adjustment.  this introduces a bit of error,
	// but I believe it won't be noticeable.
	AdjustToFocusPointKeepingTargetInView(P, DeltaTime, ActualCameraOrigin, ActualCameraOriginRot, LastViewOffset);

	// Get the camera-space offset from the camera origin
	FVector IdealViewOffset = CurrentCamMode->GetViewOffset(P, DeltaTime, ActualCameraOrigin, ActualCameraOriginRot);
//	debugf(TEXT("Ideal Offset %s"), *IdealViewOffset.ToString() );

	// get the desired FOV
	OutVT.POV.FOV = eventGetDesiredFOV(P);

	OutVT.POV.Rotation = ActualCameraOriginRot;

	// handle camera turns
	if ( bDoingACameraTurn )
	{
		TurnCurTime += DeltaTime;

		FLOAT TurnInterpPct = (TurnCurTime - TurnDelay) / TurnTotalTime;
		TurnInterpPct = Clamp(TurnInterpPct, 0.f, 1.f);
		if (TurnInterpPct == 1.f)
		{
			// turn is finished!
			EndTurn();
		}

		// swing as a square, feels better for 180s
		FLOAT TurnAngle = FInterpEaseInOut(TurnStartAngle, TurnEndAngle, TurnInterpPct, 2.f);

		// rotate view orient
		OutVT.POV.Rotation.Yaw += appTrunc(TurnAngle);
		LastPostCamTurnYaw = OutVT.POV.Rotation.Yaw;
	}

	//
	// View relative offset
	//

	// Interpolate FOV
	FLOAT FOVBlendTime = CurrentCamMode->GetFOVBlendTime(P);
	if( !bResetCameraInterpolation && FOVBlendTime > 0.f )
	{
		FLOAT InterpSpeed = 1.f / FOVBlendTime;
		OutVT.POV.FOV = FInterpTo(LastCamFOV, OutVT.POV.FOV, DeltaTime, InterpSpeed);
	}
	LastCamFOV = OutVT.POV.FOV;

	// View relative offset.
	FVector ActualViewOffset, DeltaViewOffset;
	{
		if (bDoSeamlessPivotTransition)
		{
			// when pivot makes a big jump and we want to keep the camera in-place, we need to
			// re-basis the LastViewOffset from the old pivot to new pivot
			FRotationMatrix const PivotRotMat(ActualCameraOriginRot);

			FVector const ViewOffsetWorld = PlayerCamera->Location - ActualCameraOrigin;
			LastViewOffset = PivotRotMat.InverseTransformNormal(ViewOffsetWorld);

			bDoSeamlessPivotTransition = FALSE;
		}

		FLOAT InterpSpeed = CurrentCamMode->GetViewOffsetInterpSpeed(P, DeltaTime);
		if( !bResetCameraInterpolation && InterpSpeed > 0.f )
		{
			// VInterpRotationTo might feel better for big swings
			ActualViewOffset = VInterpTo(LastViewOffset, IdealViewOffset, DeltaTime, InterpSpeed);
		}
		else
		{
			ActualViewOffset = IdealViewOffset;
		}
		DeltaViewOffset = (ActualViewOffset - LastViewOffset);
		LastViewOffset = ActualViewOffset;
		//`CamDLog("ActualViewOffset post interp is "@ActualViewOffset);
	}

	// dealing with special optional behaviors
	if (!bDoingACameraTurn)
	{
		// are we in direct look mode?
		UBOOL bDirectLook = CurrentCamMode->UseDirectLookMode(P);	
		if ( bDirectLook )
		{
			// the 50 is arbitrary, but any real motion is way above this
			UBOOL const bMoving = (P->Velocity.SizeSquared() > 50.f) ? TRUE : FALSE;
			FRotator BaseRot = (bMoving) ? P->Velocity.Rotation() : P->Rotation;

			if ( (DirectLookYaw != 0.f) || bDoingDirectLook )
			{
				// new goal rot
				BaseRot.Yaw = FRotator::NormalizeAxis(BaseRot.Yaw + DirectLookYaw);
				OutVT.POV.Rotation = RInterpTo(OutVT.POV.Rotation, BaseRot, DeltaTime, DirectLookInterpSpeed);

				if (DirectLookYaw == 0.f)
				{
					INT const StopDirectLookThresh = bMoving ? 1000 : 50;

					// interpolating out of direct look
					if ( Abs(OutVT.POV.Rotation.Yaw - BaseRot.Yaw) < StopDirectLookThresh )
					{
						// and we're done!
						bDoingDirectLook = FALSE;
					}
				}
				else
				{
					bDoingDirectLook = TRUE;
				}
			}
		}

		UBOOL bLockedToViewTarget = CurrentCamMode->LockedToViewTarget(P);
		if ( !bLockedToViewTarget )
		{
			FLOAT PitchInterpSpeed, YawInterpSpeed, RollInterpSpeed;
			// handle following if necessary
			if ( (P->Velocity.SizeSquared() > 50.f) &&
				CurrentCamMode->ShouldFollowTarget(P, PitchInterpSpeed, YawInterpSpeed, RollInterpSpeed) )
			{
				FLOAT Scale;
				if (CurrentCamMode->FollowingCameraVelThreshold > 0.f)
				{
					Scale = Min(1.f, (P->Velocity.Size() / CurrentCamMode->FollowingCameraVelThreshold));
				}
				else
				{
					Scale = 1.f;
				}

				PitchInterpSpeed *= Scale;
				YawInterpSpeed *= Scale;
				RollInterpSpeed *= Scale;

				FRotator const BaseRot = P->Velocity.Rotation();

				// doing this per-axis allows more aggressive pitch tracking, but looser yaw tracking
				OutVT.POV.Rotation = RInterpToWithPerAxisSpeeds(OutVT.POV.Rotation, BaseRot, DeltaTime, PitchInterpSpeed, YawInterpSpeed, RollInterpSpeed);
			}
		}
	}

	// apply viewoffset (in camera space)
	FVector	DesiredCamLoc = CurrentCamMode->ApplyViewOffset( P, ActualCameraOrigin, ActualViewOffset, DeltaViewOffset, OutVT );

	// try to have a focus point in view
	AdjustToFocusPoint(P, DeltaTime, DesiredCamLoc, OutVT.POV.Rotation);

	// Set new camera position
	OutVT.POV.Location = DesiredCamLoc;

	// cache this up, for potential later use
	LastPreModifierCameraLoc = OutVT.POV.Location;
	LastPreModifierCameraRot = OutVT.POV.Rotation;

	HandleCameraSafeZone( OutVT.POV.Location, OutVT.POV.Rotation, DeltaTime );

	// apply post processing modifiers
	if (PlayerCamera)
	{
		FLOAT FOVBeforePostProcess = OutVT.POV.FOV;

		PlayerCamera->ApplyCameraModifiers(DeltaTime, OutVT.POV);

		if (CurrentCamMode->bNoFOVPostProcess)
		{
			OutVT.POV.FOV = FOVBeforePostProcess;
		}
	}

	//
	// find "worst" location, or location we will shoot the penetration tests from
	//
	FVector IdealWorstLocationLocal = CurrentCamMode->eventGetCameraWorstCaseLoc(P, OutVT);
	FMatrix CamSpaceToWorld = GetWorstCaseLocTransform(P);
	IdealWorstLocationLocal = CamSpaceToWorld.InverseTransformFVectorNoScale(IdealWorstLocationLocal);

	FVector WorstLocation = !bResetCameraInterpolation ? VInterpTo(LastWorstLocationLocal, IdealWorstLocationLocal, DeltaTime, WorstLocInterpSpeed)
														: IdealWorstLocationLocal;
	LastWorstLocationLocal = WorstLocation;

	// rotate back to world space
	WorstLocation = CamSpaceToWorld.TransformFVector(WorstLocation);

	// When viewing remote pawns, the player position is likely to be noisy, smooth out the worst location
	if (P->Role == ROLE_SimulatedProxy)
	{
		WorstLocation = !bResetCameraInterpolation ? VInterpTo(LastWorstLocation, WorstLocation, DeltaTime, WorstLocInterpSpeed)
													: WorstLocation;
	}
	LastWorstLocation = WorstLocation;

	//CLOCK_CYCLES(Time);

#if !FINAL_RELEASE && DEBUGCAMERA_PENETRATION
	P->FlushPersistentDebugLines();
	GWorld->LineBatcher->DrawPoint(WorstLocation, FColor(225,255,0), 16, SDPG_World);
#endif

	// adjust worst location origin to prevent any penetration
	if (CurrentCamMode->bValidateWorstLoc)
	{
		PreventCameraPenetration(P, CameraActor, P->Location, WorstLocation, DeltaTime, WorstLocBlockedPct, WorstLocPenetrationExtentScale, TRUE);
	}
	else
	{
		WorstLocBlockedPct = 1.f;
	}

#if !FINAL_RELEASE && DEBUGCAMERA_PENETRATION
	P->DrawDebugSphere(WorstLocation, 16, 10, 255, 255, 0, FALSE);
#endif

	// adjust final desired camera location, to again, prevent any penetration
	if(!CurrentCamMode->bSkipCameraCollision)
	{
		UBOOL bSingleRayPenetrationCheck = !ShouldDoPredictavePenetrationAvoidance(P);
		PreventCameraPenetration(P, CameraActor, WorstLocation, OutVT.POV.Location, DeltaTime, PenetrationBlockedPct, PenetrationExtentScale, bSingleRayPenetrationCheck);
	}

	HandlePawnPenetration(OutVT);

#if !FINAL_RELEASE
	APlayerController * PC = Cast<APlayerController>(P->Controller);

	if( P && P->bDebugShowCameraLocation ) //&& (P->Base == NULL || Cast<AKActor>(P->Base) != NULL) )
	{
		P->DrawDebugBox( OutVT.POV.Location, FVector(5.f), 255, 255, 255, TRUE );
		P->DrawDebugLine( OutVT.POV.Location, OutVT.POV.Location + OutVT.POV.Rotation.Vector() * 32.f, 255, 255, 255, TRUE );
	}
	// if we're debugging camera anims
	else if ( PC && PC->bDebugCameraAnims )
	{
		GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)(PTRINT)this+1, 1.0f, FColor(255,255,255), FString::Printf(TEXT("WhiteBox: Final Camera Location, Red Line : Final Camera Rotation")));
		GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)(PTRINT)this+2, 1.0f, FColor(255,255,255), FString::Printf(TEXT("GreenLine: Matinee Camera Path, Yellow Line: Matinee Camera Rotation")));
		GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)(PTRINT)this+3, 1.0f, FColor(255,255,255), FString::Printf(TEXT("Yellow Box: Player's initial Location")));
		GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)(PTRINT)this+4, 1.0f, FColor(255,255,255), FString::Printf(TEXT("Coordinates: Changes of players location with rotation")));
		GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)(PTRINT)this+5, 1.0f, FColor(255,255,255), FString::Printf(TEXT("Try \"ToggleDebugCamera\" to examine closer")));
		GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)(PTRINT)this+6, 1.0f, FColor(255,255,255), FString::Printf(TEXT("To remove lines, toggle \"DebugCameraAnims\"")));

		// if current active one is available
		if ( PlayerCamera->ActiveAnims.Num() > 0 )
		{
			P->DrawDebugBox( OutVT.POV.Location, FVector(5.f), 200, 200, 200, TRUE );
			P->DrawDebugLine( OutVT.POV.Location, OutVT.POV.Location + OutVT.POV.Rotation.Vector() * 10.f, 255, 0, 0, TRUE );
		}
	}

	if (bDrawDebug)
	{
//		GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)((PTRINT)this) + 1, 5.0f, FColor(255,255,255), FString::Printf(TEXT("OutVT.POV.Location: %f %f %f"), OutVT.POV.Location.X, OutVT.POV.Location.Y, OutVT.POV.Location.Z));		
//		GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)((PTRINT)this) + 2, 5.0f, FColor(255,255,255), FString::Printf(TEXT("DesiredCamLoc: %f %f %f"), DesiredCamLoc.X, DesiredCamLoc.Y, DesiredCamLoc.Z));
		GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)((PTRINT)this) + 3, 5.0f, FColor(255,255,255), FString::Printf(TEXT("FOV: %.2f"), OutVT.POV.FOV));

		if (P != NULL && P->Mesh != NULL)
		{
			FVector RootBoneOffset = OutVT.POV.Location - P->Mesh->GetBoneMatrix(0).GetOrigin();
			GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)((PTRINT)this) + 4, 5.0f, FColor(255,255,255), FString::Printf(TEXT("Offset from root bone: %.2f %.2f %.2f"), RootBoneOffset.X, RootBoneOffset.Y, RootBoneOffset.Z));
		}

		GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)((PTRINT)this) + 5, 5.0f, FColor(255,255,255), FString::Printf(TEXT("IdealPivot: %.2f %.2f %.2f"), IdealCameraOrigin.X, IdealCameraOrigin.Y, IdealCameraOrigin.Z));
		GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)((PTRINT)this) + 6, 5.0f, FColor(255,255,255), FString::Printf(TEXT("ActualPivot: %.2f %.2f %.2f"), ActualCameraOrigin.X, ActualCameraOrigin.Y, ActualCameraOrigin.Z));

		PlayerCamera->DrawDebugBox(LastIdealCameraOrigin, FVector(8.f,8.f,8.f), 0, 200, 0, FALSE);			// dk green
		PlayerCamera->DrawDebugBox(LastActualCameraOrigin, FVector(7.f,7.f,7.f), 128, 255, 128, FALSE);		// lt green
		PlayerCamera->DrawDebugBox(DesiredCamLoc, FVector(15.f,15.f,15.f), 0, 0, 200, FALSE);			// dk blue
		PlayerCamera->DrawDebugBox(OutVT.POV.Location, FVector(14.f,14.f,14.f), 128, 128, 255, FALSE);	// lt blue
		PlayerCamera->DrawDebugBox(WorstLocation, FVector(8.f,8.f,8.f), 255, 0, 0, FALSE);				// red

		for( INT ModifierIdx = 0; ModifierIdx < PlayerCamera->ModifierList.Num(); ++ModifierIdx )
		{
			// Apply camera modification and output into DesiredCameraOffset/DesiredCameraRotation
			if( PlayerCamera->ModifierList(ModifierIdx) != NULL &&
				!PlayerCamera->ModifierList(ModifierIdx)->IsDisabled() )
			{
				TArray<FString> DebugLines;
				PlayerCamera->ModifierList(ModifierIdx)->GetDebugText(DebugLines);

				for (INT LineIdx=0; LineIdx<DebugLines.Num(); ++LineIdx)
				{
					GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)((PTRINT)this) + 7 + LineIdx, 5.0f, FColor(200,200,200), DebugLines(LineIdx));
				}
			}
		}
	}
#endif
}

void UGameThirdPersonCamera::InterpolateCameraOrigin( class APawn* TargetPawn, FLOAT DeltaTime, FVector& out_ActualCameraOrigin, FVector const& IdealCameraOrigin, FRotator& out_ActualCameraOriginRot, FRotator const& IdealCameraOriginRot )
{
	CurrentCamMode->InterpolateCameraOrigin( TargetPawn, DeltaTime, out_ActualCameraOrigin, IdealCameraOrigin, out_ActualCameraOriginRot, IdealCameraOriginRot );
}

FMatrix UGameThirdPersonCamera::GetWorstCaseLocTransform(APawn* P) const
{
	return FRotationTranslationMatrix(P->Rotation, P->Location);
}

UBOOL UGameThirdPersonCamera::ShouldDoPredictavePenetrationAvoidance(APawn* TargetPawn) const
{
	return CurrentCamMode->bDoPredictiveAvoidance;
}

void UGameThirdPersonCamera::HandlePawnPenetration(FTViewTarget& OutVT)
{
	// left for games to handle individually (e.g. hide pawns when penetrating, etc)
}

// motion introduced by the motion of the base
void UGameThirdPersonCamera::UpdateForMovingBase(AActor* BaseActor)
{
	// left for games to handle
}


FVector UGameThirdPersonCamera::GetEffectiveFocusLoc(const FVector& CamLoc, const FVector& FocusLoc, const FVector& ViewOffset)
{
	FLOAT YawDelta=0, PitchDelta=0, PitchDist=0, YawDist=0;
	const FVector Fwd(1.f,0.f,0.f);

	FVector const CamToFocus = FocusLoc - CamLoc;
	FLOAT const CamToFocusSize = CamToFocus.Size();

	// quick summary of what's going on here
	// what we want is for the effective focus loc to mirror the camera offset if the 
	// real camera was looking at the focus point.  This is basically  a parallelogram with the base 
	// camera position and the focusloc as one diagonal, and the ideal camera loc and the effective
	// loc forming the other diagonal.  
	// given the data we have, we need solve an SSA triangle to find the adjustment angle.
	// we do this twice, once for the yaw axis (XY plane) and once for pitch (XZ plane).

	// YAW
	{
		FVector ViewOffset3DNorm = ViewOffset;
		ViewOffset3DNorm.Z = 0.f;
		FLOAT ViewOffset3DSize = ViewOffset3DNorm.Size();
		ViewOffset3DNorm /= ViewOffset3DSize;	

		FLOAT DotProd = ViewOffset3DNorm | Fwd;
		if ( (DotProd < 0.999f) && (DotProd > -0.999f) )
		{
			FLOAT Alpha = PI - appAcos(DotProd);

			FLOAT SinTheta = ViewOffset3DSize * appSin(Alpha) / CamToFocusSize;
			FLOAT Theta = appAsin(SinTheta);

			YawDelta = RadToUnrRotatorUnits(Theta);
			if (ViewOffset.Y > 0.f)
			{
				YawDelta = -YawDelta;
			}

			FLOAT Phi = PI - Theta - Alpha;
			YawDist = ViewOffset3DSize * appSin(Phi) / SinTheta - CamToFocusSize;
		}
	}

	// PITCH
	{
		FVector ViewOffset3DNorm = ViewOffset;
		ViewOffset3DNorm.Y = 0.f;
		FLOAT ViewOffset3DSize = ViewOffset3DNorm.Size();
		ViewOffset3DNorm /= ViewOffset3DSize;	

		FLOAT DotProd = ViewOffset3DNorm | Fwd;
		if ( (DotProd < 0.999f) && (DotProd > -0.999f) )
		{
			FLOAT Alpha = PI - appAcos(DotProd);
			FLOAT SinTheta = ViewOffset3DSize * appSin(Alpha) / CamToFocusSize;
			FLOAT Theta = appAsin(SinTheta);

			PitchDelta = RadToUnrRotatorUnits(Theta);
			if (ViewOffset.Z > 0.f)
			{
				PitchDelta = -PitchDelta;
			}

			FLOAT Phi = PI - Theta - Alpha;
			PitchDist = ViewOffset3DSize * appSin(Phi) / appSin(Theta) - CamToFocusSize;
		}
	}

	FLOAT Dist = CamToFocusSize + PitchDist + YawDist;
	FRotator const CamToFocusRot = CamToFocus.Rotation();
	FRotationMatrix const M(CamToFocusRot);
	FVector X, Y, Z;
	M.GetAxes(X, Y, Z);

	FVector AdjustedCamVec = CamToFocus.RotateAngleAxis(appTrunc(YawDelta), Z);
	AdjustedCamVec = AdjustedCamVec.RotateAngleAxis(appTrunc(-PitchDelta), Y);
	AdjustedCamVec.Normalize();

	FVector EffectiveFocusLoc = CamLoc + AdjustedCamVec * Dist;

	//debugf(TEXT("effectivefocusloc (%f, %f, %f)"), EffectiveFocusLoc.X, EffectiveFocusLoc.Y, EffectiveFocusLoc.Z);

	return EffectiveFocusLoc;
}


/**
* Adjust Camera location and rotation, to try to have a FocusPoint (point in world space) in view.
* Also supports blending in and out from that adjusted location/rotation.
*
* @param	P			Pawn currently being viewed.
* @param	DeltaTime	seconds since last rendered frame.
* @param	CamLoc		(out) cam location
* @param	CamRot		(out) cam rotation
*/
void UGameThirdPersonCamera::AdjustToFocusPoint(APawn* P, FLOAT DeltaTime, FVector& CamLoc, FRotator& CamRot)
{
	UBOOL bProcessedFocusPoint = FALSE;

	AWorldInfo* const WorldInfo = GWorld->GetWorldInfo();

	if (FocusPoint.bAdjustCamera)
	{
		if (LeftoverPitchAdjustment > 0.f)
		{
			CamRot.Pitch += appTrunc(LeftoverPitchAdjustment);
		}
		LeftoverPitchAdjustment = 0.f;

		// this function is only for use if this is false
		return;
	}

	// Adjust Blend interpolation speed
	FLOAT InterpSpeed;
	if( bFocusPointSet )
	{
		// Keep track if focus point changes, to slow down interpolation speed
		if( (ActualFocusPointWorldLoc - LastFocusPointLoc).SizeSquared() > 1.f )
		{
			LastFocusChangeTime = WorldInfo->TimeSeconds;
			LastFocusPointLoc	= ActualFocusPointWorldLoc;
		}

		// Blend from min to max speed, since focus point last moved, over Focus_FastAdjustKickInTime seconds.
		if( (WorldInfo->TimeSeconds - LastFocusChangeTime) > Focus_FastAdjustKickInTime )
		{
			InterpSpeed = FocusPoint.InterpSpeedRange.Y;
		}
		else
		{
			InterpSpeed = Lerp(FocusPoint.InterpSpeedRange.X, FocusPoint.InterpSpeedRange.Y, FPctByRange(WorldInfo->TimeSeconds-LastFocusChangeTime, 0, Focus_FastAdjustKickInTime));
		}
	}
	else
	{
		// Blend out at max speed
		InterpSpeed = FocusPoint.InterpSpeedRange.Y;
	}

	// If we have a valid focus point, try to have in view.
	if( bFocusPointSet &&
		(FocusPoint.bAlwaysFocus || ( (CamRot.Vector() | (ActualFocusPointWorldLoc - CamLoc)) > 0.f )) )
	{
		INT nbTries = 0;
		FLOAT HeightOffset = 0.f;

		if( !FocusPoint.bIgnoreTrace )
		{
			// Move camera up and back to try to have focus point in view
			for( nbTries = 0; nbTries < Focus_MaxTries; nbTries++ )
			{
				// step up and back, step by step, and trace to see if any geometry is blocking our view
				HeightOffset	= Focus_StepHeightAdjustment * nbTries;
				FVector tmpLoc	= CamLoc;
				tmpLoc.Z		+= HeightOffset;

				FVector LocalOffset(0.f);
				LocalOffset.X = -HeightOffset * Focus_BackOffStrength;
				tmpLoc += ::TransformLocalToWorld(LocalOffset, CamRot);				// note: was worldtolocal, bug?

				// basically a fasttrace
				FCheckResult Hit(1.f);
				GWorld->SingleLineCheck( Hit, P, ActualFocusPointWorldLoc, tmpLoc, TRACE_World|TRACE_StopAtAnyHit, FVector(0.f) );

				if (Hit.Actor == NULL)
				{
					break;
				}
			}
		}

		// if we can successfully view focus point, then adjust Camera so focus point is in FOV
		if( nbTries < Focus_MaxTries )
		{
			bProcessedFocusPoint = TRUE;

			// Camera Location
			LastHeightAdjustment = FInterpTo( LastHeightAdjustment, HeightOffset, DeltaTime, InterpSpeed );
			CamLoc.Z += LastHeightAdjustment;
			FVector LocalOffset(0.f);
			LocalOffset.X = -LastHeightAdjustment * Focus_BackOffStrength;
			CamLoc += ::TransformLocalToWorld(LocalOffset, CamRot);

			// ADJUST YAW
			// Get the look direction, ignoring Z
			FVector LookDir	= CamRot.Vector();
			LookDir.Z = 0.f;
			LookDir.Normalize();

			// Save final camera location as current location
			FVector FinalCamLoc = CamLoc;

			FVector DesiredLookDir = ActualFocusPointWorldLoc - FinalCamLoc;
			DesiredLookDir.Z = 0.f;
			DesiredLookDir.Normalize();

			// Get world space angles for both look direction and desired direction
			FLOAT const CurrentHeading = GetHeadingAngle( LookDir );
			FLOAT const DesiredHeading = GetHeadingAngle( DesiredLookDir );

			// Get the change in angles between current and target and convert the radians angle to Unreal Rotator Units
			FLOAT DeltaTarget = int(RadToUnrRotatorUnits(FindDeltaAngle( CurrentHeading, DesiredHeading )));

			// If not within the FOV
			if( Abs(DeltaTarget) > DegToUnrRotatorUnits(FocusPoint.InFocusFOV.X) )
			{
				// Snap Delta to FOV limit
				DeltaTarget = DeltaTarget - DeltaTarget * Abs(DegToUnrRotatorUnits(FocusPoint.InFocusFOV.X) / DeltaTarget);
				// Interpolate from last update position to delta target - saving the new adjustment for next time
				if (!bResetCameraInterpolation)
				{
					LastYawAdjustment = FInterpTo( LastYawAdjustment, DeltaTarget, DeltaTime, InterpSpeed );
				}
				else
				{
					LastYawAdjustment = DeltaTarget;
				}
			}
			// Adjust camera rotation
			CamRot.Yaw += appTrunc(LastYawAdjustment);

			// ADJUST PITCH
			// (Different because we don't have to worry about camera wrapping around)
			// Find change in pitch
			//FLOAT DesiredPitch = (FLOAT)(FRotator::NormalizeAxis((ActualFocusPointWorldLoc - CamLoc).Rotation().Pitch));
			INT DesiredPitch = (ActualFocusPointWorldLoc - CamLoc).Rotation().Pitch + appTrunc(DegToUnrRotatorUnits(FocusPoint.FocusPitchOffsetDeg));
			DeltaTarget = (FLOAT) FRotator::NormalizeAxis(DesiredPitch - CamRot.Pitch);
			if( Abs(DeltaTarget) > DegToUnrRotatorUnits(FocusPoint.InFocusFOV.Y) )
			{
				// Snap Delta to FOV limit
				DeltaTarget = DeltaTarget - DeltaTarget * Abs(DegToUnrRotatorUnits(FocusPoint.InFocusFOV.Y) / DeltaTarget);
				// Interpolate from last pitch adjustment to desired delta - saving the new adjustment for next time
				if (!bResetCameraInterpolation)
				{
					LastPitchAdjustment	= FInterpTo( LastPitchAdjustment, DeltaTarget, DeltaTime, InterpSpeed );
				}
				else
				{
					LastPitchAdjustment = DeltaTarget;
				}
			}

			// Adjust camera rotation
			CamRot.Pitch += appTrunc(LastPitchAdjustment);
		}
	}

	// if we're not viewing a focus point, blend out smoothly at Max interp speed.
	if( !bProcessedFocusPoint )
	{
		FLOAT Zero = 0.f;

		// blend out of vertical offset adjustment
		if( LastHeightAdjustment != 0 )
		{
			if (!bResetCameraInterpolation)
			{
				LastHeightAdjustment	 = FInterpTo( LastHeightAdjustment, Zero, DeltaTime, InterpSpeed );

				CamLoc.Z += LastHeightAdjustment;

				FVector LocalOffset(0.f);
				LocalOffset.X = -LastHeightAdjustment * Focus_BackOffStrength;
				CamLoc += ::TransformLocalToWorld(LocalOffset, CamRot);
			}
			else
			{
				LastHeightAdjustment = 0;
			}
		}

		// blend out of pitch adjustment
		if( LastPitchAdjustment != 0 )
		{
			if (!bResetCameraInterpolation)
			{
				LastPitchAdjustment  = FInterpTo( LastPitchAdjustment, Zero, DeltaTime, InterpSpeed );
				CamRot.Pitch		+= appTrunc(LastPitchAdjustment);
			}
			else
			{
				LastPitchAdjustment = 0;
			}
		}

		// blend out of yaw adjustment
		if( LastYawAdjustment != 0 )
		{
			if (!bResetCameraInterpolation)
			{
				LastYawAdjustment  = FInterpTo( LastYawAdjustment, Zero, DeltaTime, InterpSpeed );
				CamRot.Yaw		+= appTrunc(LastYawAdjustment);
			}
			else
			{
				LastYawAdjustment = 0;
			}
		}
	}

	bFocusPointSuccessful = bProcessedFocusPoint;
}



/**
* This is a simplified version of AdjustToFocusPoint that keeps the camera target in frame.
* CamLoc passed in should be the camera origin, before the view offset is applied.
*/
void UGameThirdPersonCamera::AdjustToFocusPointKeepingTargetInView(APawn* P, FLOAT DeltaTime, FVector& CamLoc, FRotator& CamRot, const FVector& ViewOffset)
{
	// this function is only for use if this is true
	if (P && FocusPoint.bAdjustCamera)
	{
		FVector EffectiveFocusLoc;
		UBOOL bProcessedFocusPoint = FALSE;

		AWorldInfo* const WorldInfo = GWorld->GetWorldInfo();

		APlayerController* const PC = Cast<APlayerController>(P->Controller);

		CamRot = CamRot.GetNormalized();
		FRotator const OriginalCamRot = CamRot;

		// Adjust Blend interpolation speed
		FLOAT InterpSpeed;
		if( bFocusPointSet )
		{
			// this fudges the focus point to counteract the camera view offset
			// so we end up with the camera pointed at the target, and not the pawn
			// (which doesn't look right, since the camera is laterally offset)
			EffectiveFocusLoc = GetEffectiveFocusLoc(CamLoc, ActualFocusPointWorldLoc, ViewOffset);

			// Keep track if focus point changes, to slow down interpolation speed
			if( (EffectiveFocusLoc - LastFocusPointLoc).SizeSquared() > 1.f )
			{
				LastFocusChangeTime = WorldInfo->TimeSeconds;
				LastFocusPointLoc	= EffectiveFocusLoc;
			}

			// Blend from min to max speed, since focus point last moved, over Focus_FastAdjustKickInTime seconds.
			if( WorldInfo->TimeSeconds - LastFocusChangeTime > Focus_FastAdjustKickInTime )
			{
				InterpSpeed = FocusPoint.InterpSpeedRange.Y;
			}
			else
			{
				InterpSpeed = Lerp(FocusPoint.InterpSpeedRange.X, FocusPoint.InterpSpeedRange.Y, FPctByRange(WorldInfo->TimeSeconds-LastFocusChangeTime, 0, Focus_FastAdjustKickInTime));
			}
		}
		else
		{
			// Blend out at max speed
			InterpSpeed = FocusPoint.InterpSpeedRange.Y;
		}

		// If we have a valid focus point, try to have in view.
		if( bFocusPointSet &&
			(FocusPoint.bAlwaysFocus || ((CamRot.Vector() | (EffectiveFocusLoc - CamLoc)) > 0.f)) )
		{
			bProcessedFocusPoint = TRUE;

			// ADJUST YAW
			// Get the look direction, ignoring Z
			FVector LookDir	= CamRot.Vector();
			LookDir.Z	= 0.f;
			LookDir.Normalize();

			FVector DesiredLookDir = EffectiveFocusLoc - CamLoc;
			DesiredLookDir.Z	= 0;
			DesiredLookDir.Normalize();

			// Get world space angles for both look direction and desired direction
			FLOAT const CurrentHeading = GetHeadingAngle( LookDir );
			FLOAT const DesiredHeading = GetHeadingAngle( DesiredLookDir );

			//`log("calc heading; focus"@ActualFocusPointWorldLoc@"effocus"@EffectiveFocusLoc@"camloc"@CamLoc@"headings"@CurrentHeading@DesiredHeading@"camrot"@CamRot@"LastYP"@LastYawAdjustment@LastPitchAdjustment@LastPostAdjustmentCamRot@"XYZ"@X@Y@Z);
			//DrawDebugSphere(EffectiveFocusLoc, 32, 16, 255, 255, 255);
			//DrawDebugSphere(ActualFocusPointWorldLoc, 32, 16, 255, 255, 128);

			// Get the change in angles between current and target and convert the radians angle to Unreal Rotator Units
			FLOAT DeltaTarget = int(RadToUnrRotatorUnits(FindDeltaAngle(CurrentHeading, DesiredHeading)));

			//`log("DeltaTarget"@DeltaTarget@"headings cur/des"@CurrentHeading@DesiredHeading@"LastYawAdj"@LastYawAdjustment);

			// If not within the FOV
			if( Abs(DeltaTarget) > DegToUnrRotatorUnits(FocusPoint.InFocusFOV.X) )
			{
				// Snap Delta to FOV limit
				DeltaTarget = DeltaTarget - DeltaTarget * Abs(DegToUnrRotatorUnits(FocusPoint.InFocusFOV.X) / DeltaTarget);

				// make sure we're interpolating the shortest way around the circle.
				FLOAT Diff = LastYawAdjustment - DeltaTarget;
				if (Diff > 32768.f)
				{
					LastYawAdjustment -= 65536.f;
				}
				else if (Diff < -32768.f)
				{
					LastYawAdjustment += 65536.f;
				}

				// Interpolate from last update position to delta target - saving the new adjustment for next time
				if (!bResetCameraInterpolation)
				{
					LastYawAdjustment = FInterpTo( LastYawAdjustment, DeltaTarget, DeltaTime, InterpSpeed );
				}
				else
				{
					LastYawAdjustment = DeltaTarget;
				}
				//`log("DeltaTarget adjusted to"@DeltaTarget@"LastYawAdj"@LastYawAdjustment@FocusPoint.InFocusFOV.X);
			}
			// Adjust camera rotation
			CamRot.Yaw += appTrunc(LastYawAdjustment);

			// ADJUST PITCH
			// (Different because we don't have to worry about camera wrapping around)
			// Find change in pitch
			DeltaTarget = FLOAT(FRotator::NormalizeAxis((EffectiveFocusLoc - CamLoc).Rotation().Pitch - CamRot.Pitch));
			if( Abs(DeltaTarget) > DegToUnrRotatorUnits(FocusPoint.InFocusFOV.Y) )
			{
				// Snap Delta to FOV limit
				DeltaTarget = DeltaTarget - DeltaTarget * Abs(DegToUnrRotatorUnits(FocusPoint.InFocusFOV.Y) / DeltaTarget);
				// Interpolate from last pitch adjustment to desired delta - saving the new adjustment for next time
				if (!bResetCameraInterpolation)
				{
					LastPitchAdjustment	= FInterpTo( LastPitchAdjustment, DeltaTarget, DeltaTime, InterpSpeed );
				}
				else
				{
					LastPitchAdjustment = DeltaTarget;
				}
			}

			// Adjust camera rotation
			CamRot.Pitch += appTrunc(LastPitchAdjustment);

			// done adjusting, now clamp rotation appropriately
			if (PC)
			{
				CamRot = PC->eventLimitViewRotation( CamRot, P->ViewPitchMin, /*P->ViewPitchMax*/Max((FLOAT)OriginalCamRot.Pitch, 5000.f) );
			}

			// recalc Last adjustment vars in case LimitViewRotation changed something.
			LeftoverPitchAdjustment = LastPitchAdjustment - (CamRot.Pitch - OriginalCamRot.Pitch);
			LastYawAdjustment = CamRot.Yaw - OriginalCamRot.Yaw;
		}

		// if we're not viewing a focus point, blend out smoothly at Max interp speed.
		if( !bProcessedFocusPoint )
		{
			FLOAT Zero = 0.f;

			// blend out of pitch adjustment
			if( LastPitchAdjustment != 0 )
			{
				if (!bResetCameraInterpolation)
				{
					LastPitchAdjustment  = FInterpTo( LastPitchAdjustment, Zero, DeltaTime, InterpSpeed );
					CamRot.Pitch		+= appTrunc(LastPitchAdjustment);
				}
				else
				{
					LastPitchAdjustment = 0.f;
				}

				if (PC)
				{
					CamRot = PC->eventLimitViewRotation( CamRot, P->ViewPitchMin, /*P->ViewPitchMax*/Max((FLOAT)OriginalCamRot.Pitch, 5000.f) );
				}
				LeftoverPitchAdjustment = LastPitchAdjustment - (CamRot.Pitch - OriginalCamRot.Pitch);
			}

			// blend out of yaw adjustment
			if( LastYawAdjustment != 0.f )
			{
				if (!bResetCameraInterpolation)
				{
					LastYawAdjustment  = FInterpTo( LastYawAdjustment, Zero, DeltaTime, InterpSpeed );
					CamRot.Yaw		+= appTrunc(LastYawAdjustment);
				}
				else
				{
					LastYawAdjustment = 0.f;
				}
			}
		}

		bFocusPointSuccessful = bProcessedFocusPoint;
	}
}

/**
* Stops a camera rotation.
*/
void UGameThirdPersonCamera::EndTurn()
{
	bDoingACameraTurn = FALSE;

	// maybe align
	if (bTurnAlignTargetWhenFinished)
	{
		if (PlayerCamera && PlayerCamera->PCOwner)
		{
			FRotator TmpRot = PlayerCamera->PCOwner->Rotation;
			TmpRot.Yaw = LastPostCamTurnYaw;
			PlayerCamera->PCOwner->SetRotation(TmpRot);
		}
	}
}

UBOOL UGameThirdPersonCamera::ShouldIgnorePenetrationHit(FCheckResult const* Hit, APawn* TargetPawn) const
{
	// experimenting with not colliding with pawns
	APawn* HitPawn = Hit->Actor->GetAPawn();
	if( HitPawn )
	{
		if ( TargetPawn && (HitPawn == TargetPawn || (TargetPawn->DrivenVehicle == HitPawn) || (TargetPawn == HitPawn->Base)) )
		{
			// ignore hits on the vehicle we're driving
			return TRUE;
		}
	}
	else
	{
		// ignore KActorSpawnables, since they're only used for small, inconsequential things (for now anyway)
		if (Cast<AKActorSpawnable>(Hit->Actor))
		{
			return TRUE;
		}

		// ignore BlockingVolumes with bBlockCamera=false
		ABlockingVolume* const BV = Cast<ABlockingVolume>(Hit->Actor);
		if (BV && !BV->bBlockCamera)
		{
			return TRUE;
		}

		// if we're a zero extent trace and we hit something that doesn't take NZE hits
		// reject it.  this prevents situations where the predictive feeler traces hit something
		// that the main trace doesn't.
		// @fixme, too late in Gears 2 to try this, consider this for Gears 3.
		// LDs sometimes mark stuff "blockweapons" to let the player walk through, and use BVs
		// to contain the player, which can also block the camera.
		//if (Feeler.Extent.IsZero() && Hit->Component && !Hit->Component->BlockNonZeroExtent && !(TraceFlags & TRACE_ComplexCollision) )
		//{
		//	return TRUE;
		//}

		if (Hit->Component && !Hit->Component->CanBlockCamera)
		{
			return TRUE;
		}

	}

	return FALSE;
}


/**
* Handles traces to make sure camera does not penetrate geometry and tries to find
* the best location for the camera.
* Also handles interpolating back smoothly to ideal/desired position.
*
* @param	WorstLocation		Worst location (Start Trace)
* @param	DesiredLocation		Desired / Ideal position for camera (End Trace)
* @param	DeltaTime			Time passed since last frame.
* @param	DistBlockedPct		percentage of distance blocked last frame, between WorstLocation and DesiredLocation. To interpolate out smoothly
* @param	CameraExtentScale	Scale camera extent. (box used for collision)
* @param	bSingleRayOnly		Only fire a single ray.  Do not send out extra predictive feelers.
*/
void UGameThirdPersonCamera::PreventCameraPenetration(APawn* P, AGamePlayerCamera* CameraActor, const FVector& WorstLocation, FVector& DesiredLocation, FLOAT DeltaTime, FLOAT& DistBlockedPct, FLOAT CameraExtentScale, UBOOL bSingleRayOnly)
{	
	FLOAT HardBlockedPct = DistBlockedPct;
	FLOAT SoftBlockedPct = DistBlockedPct;

	FVector BaseRay = DesiredLocation - WorstLocation;
	FRotationMatrix BaseRayMatrix(BaseRay.Rotation());
	FVector BaseRayLocalUp, BaseRayLocalFwd, BaseRayLocalRight;
	BaseRayMatrix.GetAxes(BaseRayLocalFwd, BaseRayLocalRight, BaseRayLocalUp);

	FLOAT CheckDist = BaseRay.Size();

	FLOAT DistBlockedPctThisFrame = 1.f;

	//FlushPersistentDebugLines();
	INT NumRaysToShoot = (bSingleRayOnly) ? Min(1, PenetrationAvoidanceFeelers.Num()) : PenetrationAvoidanceFeelers.Num();
	DWORD const BaseTraceFlags = ShouldDoPerPolyPenetrationTests(P) ? (TRACE_World | TRACE_ComplexCollision) : TRACE_World;

	for (INT RayIdx=0; RayIdx<NumRaysToShoot; ++RayIdx)
	{
		FMemMark Mark(GMainThreadMemStack);

		FPenetrationAvoidanceFeeler& Feeler = PenetrationAvoidanceFeelers(RayIdx);
		if( Feeler.FramesUntilNextTrace <= 0 )
		{
			// calc ray target
			FVector RayTarget;
			{
				FVector RotatedRay = BaseRay.RotateAngleAxis(Feeler.AdjustmentRot.Yaw, BaseRayLocalUp);
				RotatedRay = RotatedRay.RotateAngleAxis(Feeler.AdjustmentRot.Pitch, BaseRayLocalRight);
				RayTarget = WorstLocation + RotatedRay;
			}

			// cast for world and pawn hits separately.  this is so we can safely ignore the 
			// camera's target pawn
			DWORD const TraceFlags = (Feeler.PawnWeight > 0.f) ? BaseTraceFlags | TRACE_Pawns : BaseTraceFlags;
			FVector const CheckExtent = Feeler.Extent * CameraExtentScale;

			// do multi-line check to make sure we hits we throw out aren't
			// masking real hits behind (these are important rays).

#if !FINAL_RELEASE && DEBUGCAMERA_PENETRATION
			GWorld->bShowExtentLineChecks = TRUE;
			GWorld->bShowLineChecks = TRUE;
#endif

			// MT-> passing camera as actor so that camerablockingvolumes know when its' the camera doing traces
			FCheckResult const* const pHitList = GWorld->MultiLineCheck(GMainThreadMemStack, RayTarget, WorstLocation, CheckExtent, TraceFlags, CameraActor);

#if !FINAL_RELEASE && DEBUGCAMERA_PENETRATION
			GWorld->bShowExtentLineChecks = FALSE;
			GWorld->bShowLineChecks = FALSE;
#endif

			Feeler.FramesUntilNextTrace = Feeler.TraceInterval;
			for( FCheckResult const* Hit = pHitList; Hit != NULL; Hit = Hit->GetNext() )
			{
				if ( (Hit->Actor != NULL) && !ShouldIgnorePenetrationHit(Hit, P) )
				{
					FLOAT const Weight = Hit->Actor->GetAPawn() ? Feeler.PawnWeight : Feeler.WorldWeight;
					FLOAT NewBlockPct = Hit->Time;
					NewBlockPct += (1.f - NewBlockPct) * (1.f - Weight);
					DistBlockedPctThisFrame = Min(NewBlockPct, DistBlockedPctThisFrame);

					// This feeler got a hit, so do another trace next frame
					Feeler.FramesUntilNextTrace = 0;
				}
			}

			if (RayIdx == 0)
			{
				// don't interpolate toward this one, snap to it
				// assumes ray 0 is the center/main ray 
				HardBlockedPct = DistBlockedPctThisFrame;
			}
			else
			{
				SoftBlockedPct = DistBlockedPctThisFrame;
			}
		}
		else
		{
			--Feeler.FramesUntilNextTrace;
		}
		Mark.Pop();
	}

	if (DistBlockedPct < DistBlockedPctThisFrame)
	{
		// interpolate smoothly out
		if (PenetrationBlendOutTime > DeltaTime)
		{
			DistBlockedPct = DistBlockedPct + DeltaTime / PenetrationBlendOutTime * (DistBlockedPctThisFrame - DistBlockedPct);
		}
		else
		{
			DistBlockedPct = DistBlockedPctThisFrame;
		}
	}
	else
	{
		if (DistBlockedPct > HardBlockedPct)
		{
			DistBlockedPct = HardBlockedPct;
		}
		else if (DistBlockedPct > SoftBlockedPct)
		{
			// interpolate smoothly in
			if (PenetrationBlendInTime > DeltaTime)
			{
				DistBlockedPct = DistBlockedPct - DeltaTime / PenetrationBlendInTime * (DistBlockedPct - SoftBlockedPct);
			}
			else
			{
				DistBlockedPct = SoftBlockedPct;
			}
		}
	}

	DistBlockedPct = Clamp<FLOAT>(DistBlockedPct, 0.f, 1.f);
	if( DistBlockedPct < KINDA_SMALL_NUMBER )
	{
		DistBlockedPct = 0.f;
	}

	if( DistBlockedPct < 1.f ) 
	{
		DesiredLocation	= WorstLocation + (DesiredLocation - WorstLocation) * DistBlockedPct;
	}
}



/*-----------------------------------------------------------------------------
	UGameThirdPersonCameraMode
-----------------------------------------------------------------------------*/

void UGameThirdPersonCameraMode::InterpolateCameraOrigin( APawn* TargetPawn, FLOAT DeltaTime, FVector& out_ActualCameraOrigin, FVector const& IdealCameraOrigin, FRotator& out_ActualCameraOriginRot, FRotator const& IdealCameraOriginRot )
{
	// First, update the camera origin.
	// This is the point in world space where camera offsets are applied.
	// We apply lazy cam on this location, so we can have a smooth / slow interpolation speed there,
	// And a different speed for offsets.
	if( ThirdPersonCam->bResetCameraInterpolation )
	{
		// no interpolation this time, snap to ideal
		out_ActualCameraOrigin = IdealCameraOrigin;
	}
	else
	{
		out_ActualCameraOrigin = InterpolateCameraOriginLoc(TargetPawn, TargetPawn->Rotation, ThirdPersonCam->LastActualCameraOrigin, IdealCameraOrigin, DeltaTime);
	}

	// smooth out CameraOriginRot if necessary
	if( ThirdPersonCam->bResetCameraInterpolation )
	{
		// no interpolation this time, snap to ideal
		out_ActualCameraOriginRot = IdealCameraOriginRot;
	}
	else
	{
		out_ActualCameraOriginRot = InterpolateCameraOriginRot(TargetPawn, ThirdPersonCam->LastActualCameraOriginRot, IdealCameraOriginRot, DeltaTime);
	}
}

/*
 *   Interpolates a camera's origin from the last location to a new ideal location
 *   @CameraTargetRot - Rotation of the camera target, used to create a reference frame for interpolation
 *   @LastLoc - Last location of the camera
 *   @IdealLoc - Ideal location for the camera this frame
 *   @DeltaTime - time step
 *   @return if bInterpLocation is false, returns IdealLoc, otherwise if bUsePerAxisOriginLocInterp is TRUE it interpolates relative to the target axis via PerAxisOriginLocInterpSpeed, else via constant OriginLocInterpSpeed
 */
FVector UGameThirdPersonCameraMode::InterpolateCameraOriginLoc(APawn* TargetPawn, FRotator const& CameraTargetRot, FVector const& LastLoc, FVector const& IdealLoc, FLOAT DeltaTime)
{
	if (!bInterpLocation)
	{
		// choosing not to do any interpolation, return ideal
		return IdealLoc;
	}
	else
	{
		if (bUsePerAxisOriginLocInterp)
		{
			FVector OriginLoc(0.f);

			//Create a reference frame for the points to interpolate in
			const FRotationMatrix RotMatrix(CameraTargetRot);
			const FMatrix RotMatrixInverse = RotMatrix.Inverse();
			
			const FVector LastLocInCameraLocal = RotMatrixInverse.TransformFVector(LastLoc);
			const FVector IdealLocInCameraLocal = RotMatrixInverse.TransformFVector(IdealLoc);

			// interp per-axis with per-axis speeds
			FVector OriginLocInCameraLocal;
			OriginLocInCameraLocal.X = FInterpTo(LastLocInCameraLocal.X, IdealLocInCameraLocal.X, DeltaTime, PerAxisOriginLocInterpSpeed.X);
			OriginLocInCameraLocal.Y = FInterpTo(LastLocInCameraLocal.Y, IdealLocInCameraLocal.Y, DeltaTime, PerAxisOriginLocInterpSpeed.Y);
			OriginLocInCameraLocal.Z = FInterpTo(LastLocInCameraLocal.Z, IdealLocInCameraLocal.Z, DeltaTime, PerAxisOriginLocInterpSpeed.Z);

			//Transform back to world space
			OriginLoc = RotMatrix.TransformFVector(OriginLocInCameraLocal);
			return OriginLoc;
		}
		else
		{
			// Apply lazy cam effect to the camera origin point
			return VInterpTo(LastLoc, IdealLoc, DeltaTime, OriginLocInterpSpeed);
		}
	}
}

FRotator UGameThirdPersonCameraMode::InterpolateCameraOriginRot(APawn* TargetPawn, FRotator const& LastRot, FRotator const& IdealRot, FLOAT DeltaTime)
{
	if (!bInterpRotation)
	{
		// choosing not to do any interpolation, return ideal
		return IdealRot;
	}
	else
	{
		return RInterpTo(LastRot, IdealRot, DeltaTime, OriginRotInterpSpeed, bRotInterpSpeedConstant);
	}
}

FVector	UGameThirdPersonCameraMode::ApplyViewOffset( class APawn* ViewedPawn, const FVector& CameraOrigin, const FVector& ActualViewOffset, const FVector& DeltaViewOffset, const FTViewTarget& OutVT )
{
	if( bApplyDeltaViewOffset )
	{
		return CameraOrigin + ::TransformLocalToWorld(DeltaViewOffset, GetViewOffsetRotBase( ViewedPawn, OutVT ));
	}
	else
	{
		return CameraOrigin + ::TransformLocalToWorld(ActualViewOffset, GetViewOffsetRotBase( ViewedPawn, OutVT ));
	}
}

FRotator UGameThirdPersonCameraMode::GetViewOffsetRotBase( class APawn* ViewedPawn, const FTViewTarget& VT )
{
	return VT.POV.Rotation;
}

FLOAT UGameThirdPersonCameraMode::GetViewOffsetInterpSpeed(class APawn* ViewedPawn, FLOAT DeltaTime)
{
	FLOAT Result = 0.f;

	if( ViewedPawn )
	{
		FLOAT BlendTime = GetBlendTime(ViewedPawn);
		if( BlendTime > 0.f )
		{
			Result = 1.f / BlendTime;
		}
	}

	// If we interpolate ViewOffset only for the camera transition
	// ramp up the interpolation factor over time, so it eventually doesn't interpolate anymore.
	if( bInterpViewOffsetOnlyForCamTransition && Result > 0.f )
	{
		ViewOffsetInterp += Result * DeltaTime;
		ViewOffsetInterp = Min<FLOAT>(ViewOffsetInterp, 10000.f);
		return ViewOffsetInterp;
	}

	// No Interpolation
	return Result;
}

FVector UGameThirdPersonCameraMode::GetViewOffset(class APawn* ViewedPawn, FLOAT DeltaTime, const FVector& ViewOrigin, const FRotator& ViewRotation)
{
	FVector out_Offset(0.f);

	// figure out which viewport config we're in.  16:9 full is default to fall back on
	CurrentViewportType = CVT_16to9_Full;
	{
		// get viewport client
		UGameViewportClient* VPClient;
		{
			ULocalPlayer* const LP = (ThirdPersonCam->PlayerCamera->PCOwner != NULL) ? Cast<ULocalPlayer>(ThirdPersonCam->PlayerCamera->PCOwner->Player) : NULL;
			VPClient = (LP != NULL) ? LP->ViewportClient : NULL;
		}

		if ( VPClient )
		{
			// figure out if we are 16:9 or 4:3
			UBOOL bWideScreen = FALSE;
			{
				FVector2D ViewportSize;
				VPClient->GetViewportSize(ViewportSize);

				FLOAT Aspect =  ViewportSize.X / ViewportSize.Y;

				if ( (Aspect > (16.f/9.f-0.01f)) && (Aspect < (16.f/9.f+0.01f)) )
				{
					bWideScreen = TRUE;
				}
				//				else if ( (Aspect < (4.f/3.f-0.01f)) || (Aspect > (4.f/3.f+0.01f)) )
				//				{
				//					debugf(TEXT("GetViewOffset: Unexpected aspect ratio %f, camera offsets may be incorrect."), Aspect);
				//				}
			}

			// decide viewport configuration
			BYTE CurrentSplitType = VPClient->GetCurrentSplitscreenType();
			if (bWideScreen)
			{
				if (CurrentSplitType == eSST_2P_VERTICAL)
				{
					CurrentViewportType = CVT_16to9_VertSplit;
				}
				else if (CurrentSplitType == eSST_2P_HORIZONTAL)
				{
					CurrentViewportType = CVT_16to9_HorizSplit;
				}
				else
				{
					CurrentViewportType = CVT_16to9_Full;
				}
			}
			else
			{
				if (CurrentSplitType == eSST_2P_VERTICAL)
				{
					CurrentViewportType = CVT_4to3_VertSplit;
				}
				else if (CurrentSplitType == eSST_2P_HORIZONTAL)
				{
					CurrentViewportType = CVT_4to3_HorizSplit;
				}
				else
				{
					CurrentViewportType = CVT_4to3_Full;
				}
			}
		}
	}

	// find our 3 offsets
	FVector MidOffset(0.f), LowOffset(0.f), HighOffset(0.f);
	{
		GetBaseViewOffsets( ViewedPawn, CurrentViewportType, DeltaTime, LowOffset, MidOffset, HighOffset );

		// apply viewport-config adjustments
		LowOffset += ViewOffset_ViewportAdjustments[CurrentViewportType].OffsetLow;
		MidOffset += ViewOffset_ViewportAdjustments[CurrentViewportType].OffsetMid;
		HighOffset += ViewOffset_ViewportAdjustments[CurrentViewportType].OffsetHigh;
	}

	// calculate final offset based on camera pitch
	FLOAT const Pitch = GetViewPitch(ViewedPawn, ViewRotation);

	if (bSmoothViewOffsetPitchChanges)
	{
		// build a spline that we can eval
		FInterpCurveInitVector Curve;
		Curve.AddPoint(ViewedPawn->ViewPitchMin, HighOffset);
		Curve.AddPoint(0.f, MidOffset);
		Curve.AddPoint(ViewedPawn->ViewPitchMax, LowOffset);
		Curve.InterpMethod = IMT_UseFixedTangentEvalAndNewAutoTangents;
		Curve.Points(0).InterpMode = CIM_CurveAuto;
		Curve.Points(1).InterpMode = CIM_CurveAuto;
		Curve.Points(2).InterpMode = CIM_CurveAuto;
		Curve.AutoSetTangents();

		out_Offset = Curve.Eval(Pitch, MidOffset);

#if !FINAL_RELEASE
		if (ThirdPersonCam->bDrawDebug)
		{
			GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)((PTRINT)this), 5.0f, FColor(255,255,255), FString::Printf(TEXT("%s out_Offset: %.2f %.2f %.2f, Pitch: %f, MidOffset: %.2f %.2f %.2f, LowOffset: %.2f %.2f %.2f,"), *GetName(), out_Offset.X, out_Offset.Y, out_Offset.Z, Pitch, MidOffset.X, MidOffset.Y, MidOffset.Z, LowOffset.X, LowOffset.Y, LowOffset.Z));
		}
#endif
	}
	else
	{
		// Linear blends between high/mid or mid/low
		// Has a discontinuity at pitch=0 that can feel odd sometimes
		if( Pitch >= 0.f )
		{
			FLOAT Pct = Pitch / ViewedPawn->ViewPitchMax;
			out_Offset	= Lerp<FVector,FLOAT>( MidOffset, LowOffset, Pct );

#if !FINAL_RELEASE
			if (ThirdPersonCam->bDrawDebug)
			{
				GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)((PTRINT)this), 5.0f, FColor(255,255,255), FString::Printf(TEXT("%s out_Offset: %.2f %f %f, Pct: %.3f, MidOffset: %.2f %.2f %.2f, LowOffset: %.2f %.2f %.2f,"), *GetName(), out_Offset.X, out_Offset.Y, out_Offset.Z, Pct, MidOffset.X, MidOffset.Y, MidOffset.Z, LowOffset.X, LowOffset.Y, LowOffset.Z));
			}
#endif
		}
		else
		{
			FLOAT Pct = Pitch / ViewedPawn->ViewPitchMin;
			out_Offset	= Lerp<FVector,FLOAT>( MidOffset, HighOffset, Pct );

#if !FINAL_RELEASE
			if (ThirdPersonCam->bDrawDebug)
			{
				GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)((PTRINT)this), 5.0f, FColor(255,255,255), FString::Printf(TEXT("%s out_Offset: %f %f %f, Pct: %f, MidOffset: %f %f %f, HighOffset: %f %f %f,"), *GetName(), out_Offset.X, out_Offset.Y, out_Offset.Z, Pct, MidOffset.X, MidOffset.Y, MidOffset.Z, HighOffset.X, HighOffset.Y, HighOffset.Z));
			}
#endif
		}

	}

	// give mode a crack at making any situational offset adjustments
	FVector const OffsetPreAdjustment = out_Offset;
	out_Offset = eventAdjustViewOffset(ViewedPawn, out_Offset);
	FVector const AdjustmentDelta = out_Offset - OffsetPreAdjustment;

	// interpolate
	FVector NewDelta = (ThirdPersonCam && !ThirdPersonCam->bResetCameraInterpolation && !ThirdPersonCam->bDoSeamlessPivotTransition) 
						? VInterpTo(ThirdPersonCam->LastOffsetAdjustment, AdjustmentDelta, DeltaTime, OffsetAdjustmentInterpSpeed)
						: AdjustmentDelta;
	
	if (ThirdPersonCam)
	{
		ThirdPersonCam->LastOffsetAdjustment = NewDelta;
	}

	// final offset and return
	out_Offset = OffsetPreAdjustment + NewDelta;
	return out_Offset;
}

FLOAT UGameThirdPersonCameraMode::GetViewPitch(APawn* TargetPawn, FRotator const& ViewRotation) const
{
	return (FLOAT)FRotator::NormalizeAxis(ViewRotation.Pitch);
}

/** Returns true if mode should lock camera to view target, false otherwise */
UBOOL UGameThirdPersonCameraMode::LockedToViewTarget(APawn* CameraTarget)
{
	return bLockedToViewTarget;
}


/** Returns true if mode should be using direct-look mode, false otherwise */
UBOOL UGameThirdPersonCameraMode::UseDirectLookMode(APawn* CameraTarget)
{
	return bDirectLook;
}



/**
* Returns true if this mode should do target following.  If true is returned, interp speeds are filled in.
* If false is returned, interp speeds are not altered.
*/
UBOOL UGameThirdPersonCameraMode::ShouldFollowTarget(APawn* CameraTarget, FLOAT& PitchInterpSpeed, FLOAT& YawInterpSpeed, FLOAT& RollInterpSpeed)
{
	return (bLockedToViewTarget) ? FALSE : bFollowTarget;
}


void UGameThirdPersonCameraMode::GetBaseViewOffsets(APawn* ViewedPawn, BYTE ViewportConfig, FLOAT DeltaTime, FVector& out_Low, FVector& out_Mid, FVector& out_High)
{
	FVector StrafeOffset(0.f), RunOffset(0.f);

	// calculate strafe and running offsets
	FLOAT VelMag = ViewedPawn->Velocity.Size();

	if (VelMag > 0.f)
	{
		FVector X, Y, Z;
		FRotationMatrix(ViewedPawn->Rotation).GetAxes(X, Y, Z);
		FVector NormalVel = ViewedPawn->Velocity / VelMag;

		if (StrafeOffsetScalingThreshold > 0.f)
		{
			FLOAT YDot = Y | NormalVel;
			if (YDot < 0.f)
			{
				StrafeOffset = StrafeLeftAdjustment * -YDot;
			}
			else
			{
				StrafeOffset = StrafeRightAdjustment * YDot;
			}
			StrafeOffset *= Clamp(VelMag / StrafeOffsetScalingThreshold, 0.f, 1.f);
		}

		if (RunOffsetScalingThreshold > 0.f)
		{
			FLOAT XDot = X | NormalVel;
			if (XDot < 0.f)
			{
				RunOffset = RunBackAdjustment * -XDot;
			}
			else
			{
				RunOffset = RunFwdAdjustment * XDot;
			}
			RunOffset *= Clamp(VelMag / RunOffsetScalingThreshold, 0.f, 1.f);
		}
	}

	// interpolate StrafeOffset and RunOffset to avoid little pops
	FLOAT Speed = StrafeOffset.IsZero() ? StrafeOffsetInterpSpeedOut : StrafeOffsetInterpSpeedIn;
	StrafeOffset = VInterpTo(LastStrafeOffset, StrafeOffset, DeltaTime, Speed);
	LastStrafeOffset = StrafeOffset;

	Speed = RunOffset.IsZero() ? RunOffsetInterpSpeedOut : RunOffsetInterpSpeedIn;
	RunOffset = VInterpTo(LastRunOffset, RunOffset, DeltaTime, Speed);
	LastRunOffset = RunOffset;

	//GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)(PTRINT)this, 1.0f, FColor(255,255,255), FString::Printf(TEXT("VelMag: %f"), VelMag));
	//GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)(PTRINT)this + 1, 1.0f, FColor(255,255,255), FString::Printf(TEXT("StrafeOffset: %f,%f,%f"), StrafeOffset.X, StrafeOffset.Y, StrafeOffset.Z));	
	//GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)(PTRINT)this + 2, 1.0f, FColor(255,255,255), FString::Printf(TEXT("RunOffset: %f,%f,%f"), RunOffset.X, RunOffset.Y, RunOffset.Z));

	// Controllers are not valid for other players in MP mode
	FRotator CamRot;
	CamRot.Yaw = 0;
	CamRot.Pitch = 0;
	CamRot.Roll = 0;
	if( ViewedPawn->Controller )
	{
		FVector UnusedVec(0.0f);
		ViewedPawn->Controller->eventGetPlayerViewPoint(UnusedVec, CamRot);
	}
	// so just use the Pawn's data to determine where to place the camera's starting loc / rot
	else
	{
		CamRot = ViewedPawn->Rotation;
	}

	FVector TotalOffset = StrafeOffset + RunOffset;
	TotalOffset = ::TransformWorldToLocal(TotalOffset, ViewedPawn->Rotation);
	TotalOffset = ::TransformLocalToWorld(TotalOffset, CamRot);

	out_Low		= ViewOffset.OffsetLow + TotalOffset;
	out_Mid 	= ViewOffset.OffsetMid + TotalOffset;
	out_High	= ViewOffset.OffsetHigh + TotalOffset;
}

FVector UGameThirdPersonCameraMode::GetTargetRelativeOriginOffset(class APawn* TargetPawn)
{
	return TargetRelativeCameraOriginOffset;
}

void UGameThirdPersonCameraMode::GetCameraOrigin(class APawn* TargetPawn, FVector& OriginLoc, FRotator& OriginRot)
{
	// Rotation
	if ( TargetPawn && (ThirdPersonCam->bResetCameraInterpolation || LockedToViewTarget(TargetPawn)) )
	{
		OriginRot = TargetPawn->eventGetViewRotation();
	}
	else
	{
		// use the camera's rotation
		OriginRot = ThirdPersonCam->PlayerCamera->Rotation;
	}


	// for non-GearPawns, let the Pawn decide what it's viewlocation is.
	OriginLoc = TargetPawn->eventGetPawnViewLocation();

	// apply any location offset
	OriginLoc += TransformLocalToWorld(GetTargetRelativeOriginOffset(TargetPawn), TargetPawn->Rotation);
}

/** Returns time to interpolate FOV changes. */
FLOAT UGameThirdPersonCameraMode::GetFOVBlendTime(class APawn* Pawn)
{
	return BlendTime;
}

/** Returns time to interpolate location/rotation changes. */
FLOAT UGameThirdPersonCameraMode::GetBlendTime(class APawn* Pawn)
{
	return BlendTime;
}

void UGameThirdPersonCameraMode::SetViewOffset(const FViewOffsetData &NewViewOffset)
{
	ViewOffset = NewViewOffset;
}



/*-----------------------------------------------------------------------------
	UGameThirdPersonCameraMode_Default
-----------------------------------------------------------------------------*/


void UGameThirdPersonCameraMode_Default::GetCameraOrigin(class APawn* TargetPawn, FVector& OriginLoc, FRotator& OriginRot)
{
	Super::GetCameraOrigin(TargetPawn, OriginLoc, OriginRot);

	// a kludgy way of interpolating out of grenade cameras
	// @fixme, need a better way to manage transitions between modes
	if (bTemporaryOriginRotInterp)
	{
		FRotator BaseOriginRot = OriginRot;
		OriginRot = RInterpTo(ThirdPersonCam->LastActualCameraOriginRot, BaseOriginRot, TemporaryOriginRotInterpSpeed, GWorld->GetWorldInfo()->DeltaSeconds);
		if (OriginRot == BaseOriginRot)
		{
			// arrived, stop interp
			bTemporaryOriginRotInterp = FALSE;
		}
	}
}

/*-----------------------------------------------------------------------------
	AGameCameraBlockingVolume
-----------------------------------------------------------------------------*/

UBOOL AGameCameraBlockingVolume::IgnoreBlockingBy( const AActor* Other ) const
{
	return !(Other->IsA(AGamePlayerCamera::StaticClass()));
}

UBOOL AGameCameraBlockingVolume::ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags)
{
	return TraceFlags & TRACE_LevelGeometry;
}

#if WITH_EDITOR
void AGameCameraBlockingVolume::SetCollisionForPathBuilding(UBOOL bNowPathBuilding)
{
	if (bNowPathBuilding)
	{
		// turn off collision - for non-static actors with bPathColliding false and blocking volumes with no player collision
		if (!bDeleteMe)
		{
				bPathTemp = TRUE;
				SetCollision(FALSE, bBlockActors, bIgnoreEncroachers);
		}
	}
	else if( bPathTemp )
	{
		bPathTemp = FALSE;
		SetCollision(TRUE, bBlockActors, bIgnoreEncroachers);
	}
}
#endif
