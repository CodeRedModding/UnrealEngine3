/*=============================================================================
	UnSceneCapture.cpp: render scenes to texture
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineMaterialClasses.h"
#include "EngineAnimClasses.h"
#include "ScenePrivate.h"

IMPLEMENT_CLASS(USceneCaptureComponent);
IMPLEMENT_CLASS(USceneCapture2DComponent);
IMPLEMENT_CLASS(USceneCapture2DHitMaskComponent);
IMPLEMENT_CLASS(USceneCaptureCubeMapComponent);
IMPLEMENT_CLASS(USceneCaptureReflectComponent);
IMPLEMENT_CLASS(USceneCapturePortalComponent);
IMPLEMENT_CLASS(ASceneCaptureActor);
IMPLEMENT_CLASS(ASceneCapture2DActor);
IMPLEMENT_CLASS(ASceneCaptureCubeMapActor);
IMPLEMENT_CLASS(ASceneCaptureReflectActor);
IMPLEMENT_CLASS(ASceneCapturePortalActor);
IMPLEMENT_CLASS(APortalTeleporter);
 
// arbitrary min/max
static const FLOAT MAX_FAR_PLANE = FLT_MAX;
static const FLOAT MIN_NEAR_PLANE = 1.f;

// WRH - 2007/07/20 - Disable occlusion queries on capture probes on PS3.
// They are only rendered once and just create stale queries that hang around forever.
#if PS3
static UBOOL GUseOcclusionQueriesForSceneCaptures = FALSE;
#else
static UBOOL GUseOcclusionQueriesForSceneCaptures = TRUE;
#endif


/*-----------------------------------------------------------------------------
USceneCaptureComponent
-----------------------------------------------------------------------------*/

/** 
* Constructor 
*/
USceneCaptureComponent::USceneCaptureComponent()
{
}

/**
 * Adds a capture proxy for this component to the scene
 */
void USceneCaptureComponent::Attach()
{
    Super::Attach();

	PostProcessProxies.Empty();

	// Create the post-process scene proxies here because the engine will crash if a 
	// material effect proxy referencing a MIC is created on the rendering thread.
	if( bEnablePostProcess && PostProcess )
	{
		for( INT EffectIndex = 0; EffectIndex < PostProcess->Effects.Num(); ++EffectIndex )
		{
			UPostProcessEffect* Effect = PostProcess->Effects(EffectIndex);
			FPostProcessSettings* WorldSettings = Effect->bUseWorldSettings ? &GWorld->GetWorldInfo()->DefaultPostProcessSettings : NULL;
			FPostProcessSceneProxy* Proxy = Effect->CreateSceneProxy(WorldSettings);

			if( Proxy )
			{
				PostProcessProxies.AddItem(Proxy);
			}
		}
	}

	if (Scene && bEnabled)
	{
		Scene->AddSceneCapture(this);
	}
}

/**
* Removes a capture proxy for thsi component from the scene
*/
void USceneCaptureComponent::Detach( UBOOL bWillReattach )
{
	if( Scene )
	{
		// remove this capture component from the scene
		Scene->RemoveSceneCapture(this);
	}

	Super::Detach( bWillReattach );
}

void USceneCaptureComponent::UpdateTransform()
{
	Super::UpdateTransform();

	if( Scene )
	{
		Scene->RemoveSceneCapture(this);

		if (bEnabled)
		{
			Scene->AddSceneCapture(this);
		}
	}
}

/**
 * Tick the component to handle updates
 */
void USceneCaptureComponent::Tick( FLOAT DeltaTime )
{
	Super::Tick( DeltaTime );
}

/**
* Map the various capture view settings to show flags.
*/
EShowFlags USceneCaptureComponent::GetSceneShowFlags()
{
	// start with default settings
	EShowFlags Result = SHOW_DefaultGame & (~SHOW_SceneCaptureUpdates);

	// lighting modes
	switch( ViewMode )
	{
	case SceneCapView_Unlit:
		Result = SHOW_ViewMode_Unlit|(Result&~SHOW_ViewMode_Mask);
		break;
	case SceneCapView_Wire:
		Result = SHOW_ViewMode_Wireframe|(Result&~SHOW_ViewMode_Mask);
		break;
	case SceneCapView_Lit:
		Result = SHOW_ViewMode_Lit|(Result&~SHOW_ViewMode_Mask);
		break;
	case SceneCapView_LitNoShadows:
		Result &= ~SHOW_DynamicShadows;
		Result = SHOW_ViewMode_Lit|(Result&~SHOW_ViewMode_Mask);
		break;
	default:		
		break;

	}
	// toggle fog
	if( !bEnableFog ) 
	{
		Result &= ~SHOW_Fog;
	}
	// toggle post-processing
	if( !bEnablePostProcess ) 
	{
		Result &= ~SHOW_PostProcess;
	}
	return Result;
}

void USceneCaptureComponent::FinishDestroy()
{
	Super::FinishDestroy();
}

void USceneCaptureComponent::SetFrameRate(FLOAT NewFrameRate)
{
	if (NewFrameRate != FrameRate)
	{
		FrameRate = NewFrameRate;
		BeginDeferredReattach();
	}
}

void USceneCaptureComponent::SetEnabled(UBOOL bEnable)
{
	if (bEnabled != bEnable)
	{
		bEnabled = bEnable;
		BeginDeferredReattach();
	}
}

/*-----------------------------------------------------------------------------
FSceneCaptureProbe
-----------------------------------------------------------------------------*/

FSceneCaptureProbe::~FSceneCaptureProbe()
{
	for (INT ViewStateIndex = 0; ViewStateIndex < ViewStates.Num(); ViewStateIndex++)
	{
		if (ViewStates(ViewStateIndex) != NULL)
		{ 
			ViewStates(ViewStateIndex)->Destroy(); 
			ViewStates(ViewStateIndex) = NULL;
		}
	}
	ViewStates.Empty();
}

/** 
* Determine if a capture is needed based on TimeBetweenCaptures and LastCaptureTime
* @param CurrentTime - seconds since start of play in the world
* @return TRUE if the capture needs to be updated
*/
UBOOL FSceneCaptureProbe::UpdateRequired(const FSceneViewFamily& ViewFamily)
{
	// Skip rendering if the render target texture hasn't been bound for at least a second
	FTextureRenderTargetResource* RTResource = TextureTarget ? TextureTarget->GetRenderTargetResource() : NULL;
	if (bSkipUpdateIfTextureUsersOccluded && (RTResource != NULL) && (!GIsEditor || ViewFamily.bRealtimeUpdate))
	{
		if (GCurrentTime - RTResource->LastRenderTime > 1.0f)
		{
			return FALSE;
		}
	}

	LastCaptureTime = Min<FLOAT>(ViewFamily.CurrentWorldTime,LastCaptureTime);
	if (ViewActor != NULL)
	{
		if (bSkipUpdateIfOwnerOccluded && ViewFamily.CurrentWorldTime - ViewActor->LastRenderTime > 1.0f)
		{
			return FALSE;
		}
		else if (MaxUpdateDistSq > 0.f)
		{
			UBOOL bInRange = FALSE;
			for (INT i = 0; i < ViewFamily.Views.Num(); i++)
			{
				if ((ViewActor->Location - ViewFamily.Views(i)->ViewOrigin).SizeSquared() <= MaxUpdateDistSq)
				{
					bInRange = TRUE;
					break;
				}
			}
			if (!bInRange)
			{
				return FALSE;
			}
		}
	}

    return (	(TimeBetweenCaptures == 0 && LastCaptureTime == 0) ||
				(TimeBetweenCaptures > 0 && (ViewFamily.CurrentWorldTime - LastCaptureTime) >= TimeBetweenCaptures)		);
}

/**
 *	Return the location of the probe (the actual portal).
 *
 *	@return	FVector		The location of the probes ViewActor
 */
FVector FSceneCaptureProbe::GetProbeLocation() const
{
	if (ViewActor)
	{
		return ViewActor->Location;
	}

	return FVector(0.0f);
}

/**
*	Return the location of the ViewActor of the probe.
*
*	@param  FVector		The location of the probes ViewActor
*	@return	TRUE if view actor exist and location is valid
*/
UBOOL FSceneCaptureProbe::GetViewActorLocation(FVector & ViewLocation) const
{
	if (ViewActor)
	{
		ViewLocation = ViewActor->Location;
		return TRUE;
	}

	return FALSE;
}

/*-----------------------------------------------------------------------------
USceneCapture2DComponent
-----------------------------------------------------------------------------*/

/**
 * Attach a new 2d capture component
 */
void USceneCapture2DComponent::Attach()
{
	// clamp near/far clip distances
	NearPlane = Max( NearPlane, MIN_NEAR_PLANE );
	if (FarPlane > 0.f)
	{
		FarPlane = Clamp<FLOAT>( FarPlane, NearPlane, MAX_FAR_PLANE );
	}
	// clamp fov
	FieldOfView = Clamp<FLOAT>( FieldOfView, 1.f, 179.f );	

    Super::Attach();
}

/**
* Sets the ParentToWorld transform the component is attached to.
* @param ParentToWorld - The ParentToWorld transform the component is attached to.
*/
void USceneCapture2DComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	// only affects the view matrix
	if( bUpdateMatrices )
	{
		// set view to location/orientation of parent
		ViewMatrix = ParentToWorld.Inverse();
		// swap axis st. x=z,y=x,z=y (unreal coord space) so that z is up
		ViewMatrix = ViewMatrix * FMatrix(
				FPlane(0,	0,	1,	0),
				FPlane(1,	0,	0,	0),
				FPlane(0,	1,	0,	0),
				FPlane(0,	0,	0,	1)); 
	}

	Super::SetParentToWorld( ParentToWorld );
}

/** 
 * Update the projection matrix using the fov,near,far,aspect
 */
void USceneCapture2DComponent::UpdateProjMatrix()
{
	// matrix updates can be skipped  
	if( bUpdateMatrices	)
	{
		if (FarPlane > 0.f)
		{
			// projection matrix based on the fov,near/far clip settings
			ProjMatrix = FPerspectiveMatrix(
				FieldOfView * (FLOAT)PI / 360.0f,
				TextureTarget ? (FLOAT)TextureTarget->GetSurfaceWidth() : (FLOAT)GSceneRenderTargets.GetBufferSizeX(),
				TextureTarget ? (FLOAT)TextureTarget->GetSurfaceHeight() : (FLOAT)GSceneRenderTargets.GetBufferSizeY(),
				NearPlane,
				FarPlane
				);
		}
		else
		{
			ProjMatrix = FPerspectiveMatrix(
				FieldOfView * (FLOAT)PI / 360.0f,
				TextureTarget ? (FLOAT)TextureTarget->GetSurfaceWidth() : (FLOAT)GSceneRenderTargets.GetBufferSizeX(),
				TextureTarget ? (FLOAT)TextureTarget->GetSurfaceHeight() : (FLOAT)GSceneRenderTargets.GetBufferSizeY(),
				NearPlane
				);
		}
	}
}

/**
* Create a new probe with info needed to render the scene
*/
FSceneCaptureProbe* USceneCapture2DComponent::CreateSceneCaptureProbe() 
{	
	// make sure projection matrix is updated before adding to scene
	UpdateProjMatrix();

	// any info from the component needed for capturing the scene should be copied to the probe
	return new FSceneCaptureProbe2D(
		Owner,
		TextureTarget,
		GetSceneShowFlags(),
		FLinearColor(ClearColor),
		bEnabled ? FrameRate : 0,
		PostProcess,
		bUseMainScenePostProcessSettings,
		bSkipUpdateIfTextureUsersOccluded,
		bSkipUpdateIfOwnerOccluded,
		bSkipRenderingDepthPrepass,
		MaxUpdateDist,
		MaxStreamingUpdateDist,
		MaxViewDistanceOverride,
		ViewMatrix,
		ProjMatrix); 
}

/** interface for changing TextureTarget, FOV, and clip planes */
void USceneCapture2DComponent::execSetCaptureParameters(FFrame& Stack, RESULT_DECL)
{
	P_GET_OBJECT_OPTX(UTextureRenderTarget2D, NewTextureTarget, TextureTarget);
	P_GET_FLOAT_OPTX(NewFOV, FieldOfView);
	P_GET_FLOAT_OPTX(NewNearPlane, NearPlane);
	P_GET_FLOAT_OPTX(NewFarPlane, FarPlane);
	P_FINISH;

	// set the parameters
	TextureTarget = NewTextureTarget;
	FieldOfView = NewFOV;
	NearPlane = NewNearPlane;
	FarPlane = NewFarPlane;

	// clamp near/far clip distances
	NearPlane = Max(NearPlane, MIN_NEAR_PLANE);
	if (FarPlane > 0.f)
	{
		FarPlane = Clamp<FLOAT>(FarPlane, NearPlane, MAX_FAR_PLANE);
	}
	// clamp fov
	FieldOfView = Clamp<FLOAT>(FieldOfView, 1.f, 179.f);

	// force update projection matrix
	UBOOL bOldUpdateMatrices = bUpdateMatrices;
	bUpdateMatrices = TRUE;
	UpdateProjMatrix();
	bUpdateMatrices = bOldUpdateMatrices;

	// update the sync components
	ASceneCaptureActor* CaptureActor = Cast<ASceneCaptureActor>(GetOwner());
	if (CaptureActor != NULL)
	{
		CaptureActor->SyncComponents();
	}

	BeginDeferredReattach();
}

/** changes the view location and rotation
 * @note: unless bUpdateMatrices is false, this will get overwritten as soon as the component or its owner moves
 */
void USceneCapture2DComponent::SetView(FVector NewLocation, FRotator NewRotation)
{
	ViewMatrix = FRotationTranslationMatrix(NewRotation, NewLocation).Inverse();
	// swap axis st. x=z,y=x,z=y (unreal coord space) so that z is up
	ViewMatrix = ViewMatrix * FMatrix(
			FPlane(0,	0,	1,	0),
			FPlane(1,	0,	0,	0),
			FPlane(0,	1,	0,	0),
			FPlane(0,	0,	0,	1)); 
	BeginDeferredReattach();
}

/*-----------------------------------------------------------------------------
FSceneCaptureProbe2D
-----------------------------------------------------------------------------*/

/**
* Called by the rendering thread to render the scene for the capture
* @param	MainSceneRenderer - parent scene renderer with info needed 
*			by some of the capture types.
*/
void FSceneCaptureProbe2D::CaptureScene( FSceneRenderer* MainSceneRenderer )
{
	check(MainSceneRenderer);

	// render target resource to render with
	FTextureRenderTargetResource* RTResource = TextureTarget ? TextureTarget->GetRenderTargetResource() : NULL;
	if( RTResource &&
		MainSceneRenderer->ViewFamily.Views.Num() &&
		UpdateRequired(MainSceneRenderer->ViewFamily) )
	{
		LastCaptureTime = MainSceneRenderer->ViewFamily.CurrentWorldTime;

		// should have a 2d render target resource
		check(RTResource->GetTextureRenderTarget2DResource());
		// create a temp view family for rendering the scene. use the same scene as parent
		FSceneViewFamilyContext ViewFamily(
 			RTResource,
 			(FSceneInterface*)MainSceneRenderer->Scene,
 			ShowFlags,
 			MainSceneRenderer->ViewFamily.CurrentWorldTime,
			MainSceneRenderer->ViewFamily.DeltaWorldTime,
			MainSceneRenderer->ViewFamily.CurrentRealTime, 
			FALSE, 
			FALSE, 
			FALSE, 
			TRUE, 
			TRUE, 
			1.0f, 
			FALSE, 
			TRUE	// Force the renderer to write its results to the RenderTarget
 			);

		const FPostProcessSettings *PPSettings = (bUseMainScenePostProcessSettings ? MainSceneRenderer->ViewFamily.Views(0)->PostProcessSettings : NULL);

		// allocate view state, if necessary
		if (ViewStates.Num() == 0)
		{
			ViewStates.AddItem(GUseOcclusionQueriesForSceneCaptures ? AllocateViewState() : NULL);
		}

		// Get a list of primitives to hide from the capture
		TSet<UPrimitiveComponent*> HiddenPrimitives;
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		if (WorldInfo != NULL && WorldInfo->GRI != NULL)
		{
			WorldInfo->GRI->UpdateHiddenComponentsForSceneCapture(HiddenPrimitives);
		}

		// create a view for the capture
		FSceneView* View = new FSceneView(
			&ViewFamily,
			ViewStates(0),
			-1,
			(const FSceneViewFamily*)&(MainSceneRenderer->ViewFamily),
			NULL,
			ViewActor,
			PostProcess,
			PPSettings,
			NULL,
			0,
			0,
			RTResource->GetSizeX(),
			RTResource->GetSizeY(),
			ViewMatrix,
			ProjMatrix,
			BackgroundColor,
			FLinearColor(0.f,0.f,0.f,0.f),
			FLinearColor::White,
			HiddenPrimitives
			);

		// add the view to the family
		ViewFamily.Views.AddItem(View);

		// create a new scene renderer for rendering the capture
		FSceneRenderer* CaptureSceneRenderer = CreateSceneCaptureRenderer(
			View,
			&ViewFamily,
			PostProcessProxies,
			NULL,
			MainSceneRenderer->CanvasTransform,
			TRUE
			);
		CaptureSceneRenderer->MaxViewDistanceSquaredOverride = (MaxViewDistanceOverrideSq > 0.0f) ? MaxViewDistanceOverrideSq : MAX_FLT;
		CaptureSceneRenderer->bUseDepthOnlyPass = !bSkipRenderingDepthPrepass;

		DOUBLE SavedTextureUpdateTime = RTResource->LastRenderTime;

		// render the scene to the target
		CaptureSceneRenderer->Render();

		// restore the texture update time if we're selectively updating based on it
		// so that any bits with the texture mapped in the capture don't count towards needing to render
		if (bSkipUpdateIfTextureUsersOccluded)
		{
			RTResource->LastRenderTime = SavedTextureUpdateTime;
		}
		
		// copy the results of the scene rendering from the target surface to its texture
		RHICopyToResolveTarget(RTResource->GetRenderTargetSurface(), FALSE, FResolveParams());

		DeleteSceneCaptureRenderer(CaptureSceneRenderer);
	}
}

/*-----------------------------------------------------------------------------
USceneCapture2DHitMaskComponent
-----------------------------------------------------------------------------*/

/**
* Attach a new 2d capture component
*/
void USceneCapture2DHitMaskComponent::Attach()
{
	Super::Attach();

	// set mesh that this needs to render  
	if ( Owner )
	{
		// find proper attach skeletalmesh component
		USkeletalMeshComponent * AttachSkeletalMeshComponent=NULL;

		if ( Owner->GetAPawn() )
		{
			AttachSkeletalMeshComponent = Owner->GetAPawn()->Mesh;
		}
		else if ( Owner->IsA(ASkeletalMeshActor::StaticClass()))
		{
			ASkeletalMeshActor * SMA = CastChecked<ASkeletalMeshActor>(Owner);
			AttachSkeletalMeshComponent = SMA->SkeletalMeshComponent;
		}
		else 
		{
			Owner->Components.FindItemByClass(&AttachSkeletalMeshComponent);
		}

		if ( SkeletalMeshComp != AttachSkeletalMeshComponent)
		{
			SkeletalMeshComp = AttachSkeletalMeshComponent;
			BeginDeferredReattach();
		}
	}
}

/**
* Sets the ParentToWorld transform the component is attached to.
* @param ParentToWorld - The ParentToWorld transform the component is attached to.
*/
void USceneCapture2DHitMaskComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	// To avoid readdition to scene, calling ActorComponent::SetParentToWorld
	// Check USceneCaptureComponent::SetParentToWorld
	UActorComponent::SetParentToWorld( ParentToWorld );
}

/** When parent transforms, to prevent super behavior of re-attaching **/
void USceneCapture2DHitMaskComponent::UpdateTransform()
{
	// To avoid readdition to scene, calling ActorComponent::SetParentToWorld
	// Check USceneCaptureComponent::UpdateTransform
	UActorComponent::UpdateTransform();
}
/**
* Create a new probe with info needed to render the scene
*/
FSceneCaptureProbe* USceneCapture2DHitMaskComponent::CreateSceneCaptureProbe() 
{	
	if (SkeletalMeshComp)
	{
		// any info from the component needed for capturing the scene should be copied to the probe
		return new FSceneCaptureProbe2DHitMask(
			SkeletalMeshComp,
			TextureTarget, 
			MaterialIndex,
			ForceLOD, 
			FadingStartTimeSinceHit, 
			FadingPercentage, 
			FadingDurationTime,
			FadingIntervalTime, 
			HitMaskCullDistance); 
	}

	return NULL;
}

/** interface for changing TextureTarget */
void USceneCapture2DHitMaskComponent::execSetCaptureTargetTexture(FFrame& Stack, RESULT_DECL)
{
 	P_GET_OBJECT(UTextureRenderTarget2D, NewTextureTarget);
 	P_FINISH;

	TextureTarget = NewTextureTarget;
	BeginDeferredReattach();
}

/** Set Capture Parameters 
	 *  This sent message to render thread with parameter information
	 *  Render thread will modify the texture
	 */
void USceneCapture2DHitMaskComponent::SetCaptureParameters( const FVector & InMaskPosition, const FLOAT InMaskRadius, const FVector& InStartPosition, const UBOOL bOnlyWhenFacing )
{
	// make sure skeletalmeshcomponent is attached
	if ( SkeletalMeshComp && SkeletalMeshComp->IsAttached() && SkeletalMeshComp->SceneInfo )
	{
		FHitMaskMaterialInfo MaskInfo(InMaskPosition, InMaskRadius, InStartPosition, bOnlyWhenFacing, SkeletalMeshComp->SceneInfo);

		// set flag, so that when detached, it should send delete message
		// even if this mask has been deleted long time ago
		SkeletalMeshComp->bNeedsToDeleteHitMask = TRUE;

		// if capture info exists
		if ( CaptureInfo )
		{
			// Send a command to the rendering thread to update parameter
			ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
				FHitMaskUpdateCommand,
				FCaptureSceneInfo*,CaptureInfo,CaptureInfo,
				FHitMaskMaterialInfo, MaskInfo, MaskInfo,
				FLOAT, CurrentTime, GWorld->GetTimeSeconds(),
			{
				FSceneCaptureProbe2DHitMask * SceneProbe = (FSceneCaptureProbe2DHitMask*)CaptureInfo->SceneCaptureProbe;
				// add the newly created capture info to the list of captures in the scene
				SceneProbe->AddMask(MaskInfo, CurrentTime);
			});
		}
	}
}

/** Mask information - Hit World Position, Start Position, Radius **/
void USceneCapture2DHitMaskComponent::execSetCaptureParameters(FFrame& Stack, RESULT_DECL)
{
	P_GET_VECTOR(InMaskPosition);
	P_GET_FLOAT(InMaskRadius);
	P_GET_VECTOR(InStartPosition);
	P_GET_UBOOL(InOnlyWhenFacing);
 	P_FINISH;
	
	SetCaptureParameters(InMaskPosition, InMaskRadius, InStartPosition, InOnlyWhenFacing);
}

/** Set Fading Start Time Since Hit
 *  Will update the value in render thread - SceneProbe
 */
void USceneCapture2DHitMaskComponent::SetFadingStartTimeSinceHit( const FLOAT InFadingStartTimeSinceHit )
{
	if ( SkeletalMeshComp && SkeletalMeshComp->SceneInfo )
	{
		// if capture info exists
		if ( CaptureInfo )
		{
			// Send a command to the rendering thread to update parameter
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
				FHitMaskUpdateFadingParameterCommand,
				FCaptureSceneInfo*,CaptureInfo,CaptureInfo,
				FLOAT, InFadingStartTimeSinceHit, InFadingStartTimeSinceHit,
			{
				FSceneCaptureProbe2DHitMask * SceneProbe = (FSceneCaptureProbe2DHitMask*)CaptureInfo->SceneCaptureProbe;
				// update Fading Start Time
				SceneProbe->SetFadingStartTimeSinceHit(InFadingStartTimeSinceHit);
			});
		}
	}
}

/** Mask information - Hit World Position, Start Position, Radius **/
void USceneCapture2DHitMaskComponent::execSetFadingStartTimeSinceHit(FFrame& Stack, RESULT_DECL)
{
	P_GET_FLOAT(InFadingStartTimeSinceHit);
	P_FINISH;

	SetFadingStartTimeSinceHit(InFadingStartTimeSinceHit);
}

/*-----------------------------------------------------------------------------
FSceneCaptureProbe2D
-----------------------------------------------------------------------------*/
/** Constructor **/
FSceneCaptureProbe2DHitMask::FSceneCaptureProbe2DHitMask(
							USkeletalMeshComponent * InMeshComponent,
							UTextureRenderTarget* InTextureTarget, 
							INT	InMaterialIndex,
							INT InForceLOD, 
							FLOAT InFadingStartTimeSinceHit, 
							FLOAT InFadingPercentage, 
							FLOAT InFadingDurationTime,
							FLOAT InFadingIntervalTime,
							FLOAT InMaskCullDistance
							)
:	FSceneCaptureProbe(NULL,InTextureTarget,0,FLinearColor::Black,30,NULL,FALSE,FALSE,FALSE,FALSE,0.f,0.f,0.f)
,	MeshComponent(InMeshComponent)
, 	LastAddedTime(0.f)
,	MaterialIndex(InMaterialIndex)
, 	ForceLOD(InForceLOD)
, 	FadingStartTimeSinceHit(InFadingStartTimeSinceHit)
,	FadingPercentage(InFadingPercentage)
,	FadingDurationTime(InFadingDurationTime)
,	FadingIntervalTime(InFadingIntervalTime)
,	MaskCullDistance(InMaskCullDistance)
{
	check (MeshComponent);
}


/**
* Added by the game thread via render thread to add mask
* @param	HitMask			HitMask information it needs
*/
void FSceneCaptureProbe2DHitMask::AddMask(const FHitMaskMaterialInfo& HitMask, FLOAT CurrentTime)
{
	// should be done by render thread
	check(IsInRenderingThread());

	// create new gore mesh material info
	new(MaskList) FHitMaskMaterialInfo(HitMask);
	LastAddedTime = CurrentTime;
}

/**
*	Callback function to clear anything if dependent 
*	when PrimitiveComponent is detached
*/
void FSceneCaptureProbe2DHitMask::Clear(const UPrimitiveComponent * ComponentToBeDetached)
{
	check (IsInRenderingThread());

	// if this is same component as check component, clear the list
	if ( MeshComponent == ComponentToBeDetached )
	{
		// empty all list
		MaskList.Empty();
	}
}

/**
*	Callback function to clear anything if dependent 
*	when PrimitiveComponent is detached
*/
void FSceneCaptureProbe2DHitMask::Update(const UPrimitiveComponent * ComponentToBeDetached)
{
	check (IsInRenderingThread());

	// if this is same component as check component, clear the list
	if ( MeshComponent == ComponentToBeDetached && MeshComponent->SceneInfo && MeshComponent->SceneInfo->Proxy )
	{
		// iterate list and render to the target
		for( INT MaskID = 0; MaskID < MaskList.Num(); ++MaskID )
		{
			FHitMaskMaterialInfo& MaskInfo = MaskList(MaskID);
			MaskInfo.SceneInfo = MeshComponent->SceneInfo;
		}
	}
}


/** 
 * Update FadingStarttimesinceHit. It's used when disabling - i.e. when pawn is dead
 */
void FSceneCaptureProbe2DHitMask::SetFadingStartTimeSinceHit(const FLOAT InFadingStartTimeSinceHit)
{
	FadingStartTimeSinceHit = InFadingStartTimeSinceHit;
}
	 
/**
* Called by the rendering thread to render the scene for the capture
* @param	MainSceneRenderer - parent scene renderer with info needed 
*			by some of the capture types.
*/
void FSceneCaptureProbe2DHitMask::CaptureScene( FSceneRenderer* MainSceneRenderer )
{
	check( MainSceneRenderer );
	check( MeshComponent );

	if ( GWorld == NULL || MeshComponent->HiddenGame) 
	{
		// if GWorld is NULL, clear it and return
		MaskList.Empty();
		return;
	}

	// render target resource to render with
	FTextureRenderTarget2DResource* RTResource2D = TextureTarget && TextureTarget->GetRenderTargetResource() ? TextureTarget->GetRenderTargetResource()->GetTextureRenderTarget2DResource() : NULL;
	if( RTResource2D && LastAddedTime>0.f )
	{
		FLOAT TimeSeconds = MainSceneRenderer->ViewFamily.CurrentWorldTime;

		// Have items on the list?
		if ( MaskList.Num() > 0 )
		{
			LastCaptureTime = MainSceneRenderer->ViewFamily.CurrentWorldTime;

			// iterate list and render to the target
			for( INT MaskID = 0; MaskID < MaskList.Num(); ++MaskID )
			{
				const FHitMaskMaterialInfo& MaskInfo = MaskList(MaskID);
				// Go iterate through to find which PrimitiveSceneInfo belongs to this mesh. 
				// I can't access MeshComponent::SceneInfo data since it can be removed anytime by game thread. 
				const FPrimitiveSceneInfo* PrimitiveSceneInfo = MaskInfo.SceneInfo;
				if (PrimitiveSceneInfo)
				{
					FSkeletalMeshSceneProxy* Proxy = (FSkeletalMeshSceneProxy*)PrimitiveSceneInfo->Proxy;

					// if proxy is still there - when primitive gets re-attached, proxy can be null
					if ( Proxy )
					{
						// create view family/view
						FSceneViewFamilyContext ViewFamily(
							RTResource2D,
							(FSceneInterface*)MainSceneRenderer->Scene,
							SHOW_SkeletalMeshes|SHOW_Materials,
							MainSceneRenderer->ViewFamily.CurrentWorldTime,
							MainSceneRenderer->ViewFamily.DeltaWorldTime,
							MainSceneRenderer->ViewFamily.CurrentRealTime, 
							FALSE, 
							FALSE, 
							FALSE, 
							TRUE, 
							TRUE, 
							1.0f, 
							FALSE, 
							TRUE	// Force the renderer to write its results to the RenderTarget
							);

						// create a view for the capture
						FViewInfo* View = new FViewInfo(
							&ViewFamily,
							NULL,
							-1,
							(const FSceneViewFamily*)&(MainSceneRenderer->ViewFamily),
							NULL,
							NULL,
							NULL,
							NULL,
							NULL,
							0,
							0,
							0,
							0,
							RTResource2D->GetSizeX(), 
							RTResource2D->GetSizeY(), 
							FMatrix::Identity, 
							FMatrix::Identity,
							FLinearColor::Black, 
							FLinearColor::White, 
							FLinearColor::White, 
							TSet<UPrimitiveComponent*>()
							);

						// add the view to the family
						ViewFamily.Views.AddItem(View);

						// Setup the rendertarget.
						RHISetRenderTarget(RTResource2D->GetRenderTargetSurface(), NULL);
						RHISetViewParameters(*View);
						RHISetMobileHeightFogParams(View->HeightFogParams);

						// Additive doesn't work for Xbox render target since it's reused
						//RHISetBlendState(TStaticBlendState<BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());

						RHISetBlendState(TStaticBlendState<>::GetRHI());

						// only delete mask list if rendered
						// Need to handle, SDPG_World and SDPG_Foreground. 
						for ( INT DPGIndex=SDPG_World; DPGIndex<=SDPG_Foreground; ++DPGIndex )
						{
							// Draw the mesh
							TDynamicPrimitiveDrawer<FHitMaskDrawingPolicyFactory> Drawer(
								View,DPGIndex,FHitMaskDrawingPolicyFactory::ContextType(MaskInfo.MaskLocation, MaskInfo.MaskStartPosition, MaskInfo.MaskRadius, MaskCullDistance, MaskInfo.MaskOnlyWhenFacing, RTResource2D),TRUE);

							Drawer.SetPrimitive(PrimitiveSceneInfo);

							// Only the section they want to generate mask
							Proxy->DrawDynamicElementsByMaterial(
								&Drawer,
								View,
								DPGIndex,
								0, 
								ForceLOD, 
								MaterialIndex);
						}
					}

					// Now I need to resolve everytime to reserve multiple hits
					RHICopyToResolveTarget(RTResource2D->GetRenderTargetSurface(), FALSE, FResolveParams());
				}

				MaskList.Remove(MaskID--);
			}
		}
		// Fading part
		else if ( FadingStartTimeSinceHit > 0.f && TimeSeconds - LastAddedTime > FadingStartTimeSinceHit && TimeSeconds - LastAddedTime < FadingStartTimeSinceHit+FadingDurationTime && TimeSeconds - LastCaptureTime > FadingIntervalTime )
		{
			// during this time, fade out
			LastCaptureTime = TimeSeconds;

			// Slowly fading out by setting color 0.99 resulting 0.99 * texture color
			// Although color seems a lot fast fading
			FLinearColor Color = FLinearColor(FadingPercentage, FadingPercentage, FadingPercentage, 1.f);
			FBatchedElements BatchedElements;
			FLOAT MinX = -1.0f - GPixelCenterOffset / ((FLOAT)RTResource2D->GetSizeX() * 0.5f);
			FLOAT MaxX = +1.0f - GPixelCenterOffset / ((FLOAT)RTResource2D->GetSizeX() * 0.5f);
			FLOAT MinY = +1.0f + GPixelCenterOffset / ((FLOAT)RTResource2D->GetSizeY() * 0.5f);
			FLOAT MaxY = -1.0f + GPixelCenterOffset / ((FLOAT)RTResource2D->GetSizeY() * 0.5f);

			INT V00 = BatchedElements.AddVertex(FVector4(MinX,MinY,0,1),FVector2D(0,0),Color,FHitProxyId());
			INT V10 = BatchedElements.AddVertex(FVector4(MaxX,MinY,0,1),FVector2D(1,0),Color,FHitProxyId());
			INT V01 = BatchedElements.AddVertex(FVector4(MinX,MaxY,0,1),FVector2D(0,1),Color,FHitProxyId());
			INT V11 = BatchedElements.AddVertex(FVector4(MaxX,MaxY,0,1),FVector2D(1,1),Color,FHitProxyId());
			RHISetRenderTarget(RTResource2D->GetRenderTargetSurface(),NULL);	
			// Draw a quad using the generated vertices.
			BatchedElements.AddTriangle(V00,V10,V11,RTResource2D,BLEND_Opaque);
			BatchedElements.AddTriangle(V00,V11,V01,RTResource2D,BLEND_Opaque);
			BatchedElements.Draw(FMatrix::Identity,RTResource2D->GetSizeX(),RTResource2D->GetSizeY(),FALSE);
			// Copy (resolve) the rendered image from the frame buffer to its render target texture
			RHICopyToResolveTarget(	RTResource2D->GetRenderTargetSurface(),	FALSE,	FResolveParams() );		
		}
	}
}

/*-----------------------------------------------------------------------------
USceneCaptureCubeMapComponent
-----------------------------------------------------------------------------*/

/**
* Attach a new cube capture component
*/
void USceneCaptureCubeMapComponent::Attach()
{
    // clamp near/far cliip distances
    NearPlane = Max( NearPlane, MIN_NEAR_PLANE );
	FarPlane = Clamp<FLOAT>( FarPlane, NearPlane, MAX_FAR_PLANE );

	Super::Attach();
}

/**
* Sets the ParentToWorld transform the component is attached to.
* @param ParentToWorld - The ParentToWorld transform the component is attached to.
*/
void USceneCaptureCubeMapComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	// world location of component 
	WorldLocation = ParentToWorld.GetOrigin();

	Super::SetParentToWorld( ParentToWorld );
}

/**
* Create a new probe with info needed to render the scene
*/
FSceneCaptureProbe* USceneCaptureCubeMapComponent::CreateSceneCaptureProbe() 
{
	// any info from the component needed for capturing the scene should be copied to the probe
	return new FSceneCaptureProbeCube(
											Owner,
											TextureTarget,
											GetSceneShowFlags(),
											FLinearColor(ClearColor),
											bEnabled ? FrameRate : 0,
											PostProcess,
											bUseMainScenePostProcessSettings,
											bSkipUpdateIfTextureUsersOccluded,
											bSkipUpdateIfOwnerOccluded,
											bSkipRenderingDepthPrepass,
											MaxUpdateDist,
											MaxStreamingUpdateDist,
											MaxViewDistanceOverride,
											WorldLocation,
											NearPlane,
											FarPlane
											);
}

/*-----------------------------------------------------------------------------
FSceneCaptureProbeCube
-----------------------------------------------------------------------------*/

/**
* Called by the rendering thread to render the scene for the capture
* @param	MainSceneRenderer - parent scene renderer with info needed 
*			by some of the capture types.
*/
void FSceneCaptureProbeCube::CaptureScene( FSceneRenderer* MainSceneRenderer )
{
	check(MainSceneRenderer);

	// render target resource to render with
	FTextureRenderTargetResource* RTResource = TextureTarget ? TextureTarget->GetRenderTargetResource() : NULL;
	if( RTResource &&
		MainSceneRenderer->ViewFamily.Views.Num() &&
		UpdateRequired(MainSceneRenderer->ViewFamily) )
	{
		LastCaptureTime = MainSceneRenderer->ViewFamily.CurrentWorldTime;

		// projection matrix based on the fov,near/far clip settings
		// each face always uses a 90 degree field of view
		FPerspectiveMatrix ProjMatrix(
			90.f * (FLOAT)PI / 360.0f,
			(FLOAT)RTResource->GetSizeX(),
			(FLOAT)RTResource->GetSizeY(),
			NearPlane,
			FarPlane
			);

		// allocate view state, if necessary
		if (ViewStates.Num() == 0)
		{
			ViewStates.AddItem(GUseOcclusionQueriesForSceneCaptures ? AllocateViewState() : NULL);
		}

		// Get a list of primitives to hide from the cube map capture
		TSet<UPrimitiveComponent*> HiddenPrimitives;
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		if (WorldInfo != NULL && WorldInfo->GRI != NULL)
		{
			WorldInfo->GRI->UpdateHiddenComponentsForSceneCapture(HiddenPrimitives);
		}

		// should have a cube render target resource
		FTextureRenderTargetCubeResource* RTCubeResource = RTResource->GetTextureRenderTargetCubeResource();		
		check(RTCubeResource);
        // render scene once for each cube face
		for( INT FaceIdx=CubeFace_PosX; FaceIdx < CubeFace_MAX; FaceIdx++ )
		{
			// set current target face on cube render target
			RTCubeResource->SetCurrentTargetFace((ECubeFace)FaceIdx);
			// create a temp view family for rendering the scene. use the same scene as parent
			FSceneViewFamilyContext ViewFamily(
				RTCubeResource,
				(FSceneInterface*)MainSceneRenderer->Scene,
				ShowFlags,
				MainSceneRenderer->ViewFamily.CurrentWorldTime,
				MainSceneRenderer->ViewFamily.DeltaWorldTime,
				MainSceneRenderer->ViewFamily.CurrentRealTime, 
				FALSE, 
				FALSE, 
				FALSE, 
				TRUE, 
				TRUE, 
				1.0f, 
				FALSE, 
				TRUE	// Force the renderer to write its results to the RenderTarget
				);			
			// create a view for the capture
			FSceneView* View = new FSceneView(
				&ViewFamily,
				ViewStates(0),
				-1,
				(const FSceneViewFamily*)&(MainSceneRenderer->ViewFamily),
				NULL,
				ViewActor,
				PostProcess,
				NULL,
				NULL,
				0,
				0,
				RTResource->GetSizeX(),
				RTResource->GetSizeY(),
				CalcCubeFaceViewMatrix((ECubeFace)FaceIdx),
				ProjMatrix,
				BackgroundColor,
				FLinearColor(0.f,0.f,0.f,0.f),
				FLinearColor::White,
				HiddenPrimitives
				);

			// add the view to the family
			ViewFamily.Views.AddItem(View);
			// create a new scene renderer for rendering the capture
			FSceneRenderer* CaptureSceneRenderer = CreateSceneCaptureRenderer(
				View, 
				&ViewFamily,
				PostProcessProxies,
				NULL,
				MainSceneRenderer->CanvasTransform,
				TRUE
				);
			CaptureSceneRenderer->MaxViewDistanceSquaredOverride = (MaxViewDistanceOverrideSq > 0.0f) ? MaxViewDistanceOverrideSq : MAX_FLT;
			CaptureSceneRenderer->bUseDepthOnlyPass = !bSkipRenderingDepthPrepass;

			DOUBLE SavedTextureUpdateTime = RTResource->LastRenderTime;

			// render the scene to the target
			CaptureSceneRenderer->Render();

			// restore the texture update time if we're selectively updating based on it
			// so that any bits with the texture mapped in the capture don't count towards needing to render
			if (bSkipUpdateIfTextureUsersOccluded)
			{
				RTResource->LastRenderTime = SavedTextureUpdateTime;
			}

			// copy the results of the scene rendering from the target surface to its texture
			FResolveParams ResolveParams;
			ResolveParams.CubeFace = (ECubeFace)FaceIdx;
			RHICopyToResolveTarget(RTResource->GetRenderTargetSurface(), FALSE, ResolveParams);

			DeleteSceneCaptureRenderer(CaptureSceneRenderer);
		}
	}
}

/**
* Generates a view matrix for a cube face direction 
* @param	Face - enum for the cube face to use as the facing direction
* @return	view matrix for the cube face direction
*/
FMatrix FSceneCaptureProbeCube::CalcCubeFaceViewMatrix( ECubeFace Face )
{
	FMatrix Result(FMatrix::Identity);

	static const FVector XAxis(1.f,0.f,0.f);
	static const FVector YAxis(0.f,1.f,0.f);
	static const FVector ZAxis(0.f,0.f,1.f);

	// vectors we will need for our basis
	FVector vUp(YAxis);
	FVector vDir;

	switch( Face )
	{
	case CubeFace_PosX:
		//vUp = YAxis;
		vDir = XAxis;
		break;
	case CubeFace_NegX:
		//vUp = YAxis;
		vDir = -XAxis;
		break;
	case CubeFace_PosY:
		vUp = -ZAxis;
		vDir = YAxis;
		break;
	case CubeFace_NegY:
		vUp = ZAxis;
		vDir = -YAxis;
		break;
	case CubeFace_PosZ:
		//vUp = YAxis;
		vDir = ZAxis;
		break;
	case CubeFace_NegZ:
		//vUp = YAxis;
		vDir = -ZAxis;
		break;
	}

	// derive right vector
	FVector vRight( vUp ^ vDir );
	// create matrix from the 3 axes
	Result = FBasisVectorMatrix( vRight, vUp, vDir, -WorldLocation );	

	return Result;
}

/*-----------------------------------------------------------------------------
USceneCaptureReflectComponent
-----------------------------------------------------------------------------*/

/**
* Attach a new reflect capture component
*/
void USceneCaptureReflectComponent::Attach()
{
	Super::Attach();
}

/**
* Create a new probe with info needed to render the scene
*/
FSceneCaptureProbe* USceneCaptureReflectComponent::CreateSceneCaptureProbe() 
{ 
	// use the actor's view direction as the reflecting normal
	FVector ViewActorDir( Owner ? Owner->Rotation.Vector() : FVector(0,0,1) );
	ViewActorDir.Normalize();
	FVector ViewActorPos( Owner ? Owner->Location : FVector(0,0,0) );

	// create the mirror plane	
	FPlane MirrorPlane = FPlane( ViewActorPos, ViewActorDir );

	// any info from the component needed for capturing the scene should be copied to the probe
	return new FSceneCaptureProbeReflect(
								Owner,
								TextureTarget,
								GetSceneShowFlags(),
								FLinearColor(ClearColor),
								bEnabled ? FrameRate : 0,
								PostProcess,
								bUseMainScenePostProcessSettings,
								bSkipUpdateIfTextureUsersOccluded,
								bSkipUpdateIfOwnerOccluded,
								bSkipRenderingDepthPrepass,
								MaxUpdateDist,
								MaxStreamingUpdateDist,
								MaxViewDistanceOverride,
								MirrorPlane
								);
}

/*-----------------------------------------------------------------------------
FSceneCaptureProbeReflect
-----------------------------------------------------------------------------*/

/**
 * Wrapper for performing occlusion queries on a UPrimitiveComponent object
 *
 */
class FPrimitiveComponentOcclusionWrapper
{
protected:
	UPrimitiveComponent* Component;
	FBoxSphereBounds Bounds;
	UBOOL bIgnoreNearPlaneIntersection;
	UBOOL bAllowApproximateOcclusion;
public:
	inline FPrimitiveComponentOcclusionWrapper(UPrimitiveComponent* InComponent, UBOOL bInIgnoreNearPlaneIntersection, UBOOL bInAllowApproximateOcclusion)
		:	Component(InComponent)
		,	Bounds(Component->Bounds)
		,	bIgnoreNearPlaneIntersection(bInIgnoreNearPlaneIntersection)
		,	bAllowApproximateOcclusion(bInAllowApproximateOcclusion)
	{
		Bounds.BoxExtent = Bounds.BoxExtent * OCCLUSION_SLOP + OCCLUSION_SLOP;
		Bounds.SphereRadius = Bounds.SphereRadius * OCCLUSION_SLOP + OCCLUSION_SLOP;
	}

	inline INT GetVisibilityId() const
	{
		//@todo - okay to use precomputed visibility on the game thread?
		return INDEX_NONE;
	}

	inline UPrimitiveComponent* GetComponent() const
	{
		return Component;
	}

	inline UBOOL IgnoresNearPlaneIntersection() const
	{
		return bIgnoreNearPlaneIntersection;
	}

	inline UBOOL AllowsApproximateOcclusion() const
	{
		return bAllowApproximateOcclusion;
	}

	inline UBOOL IsOccludable(FViewInfo&) const
	{
		return TRUE;
	}

	inline FLOAT PixelPercentageOnFirstFrame() const
	{
		return GEngine->MaxOcclusionPixelsFraction;
	}

	inline const FBoxSphereBounds& GetOccluderBounds() const
	{
		return Bounds;
	}

	inline UBOOL RequiresOcclusionForCorrectness() const
	{
		return FALSE;
	}
};

/** 
* Determine if a capture is needed based on the given ViewFamily
* @param ViewFamily - the main renderer's ViewFamily
* @return TRUE if the capture needs to be updated
*/
UBOOL FSceneCaptureProbeReflect::UpdateRequired( const FSceneViewFamily& ViewFamily )
{
	if (bSkipUpdateIfOwnerOccluded)
	{
		UStaticMeshComponent* PrimComponent;
		INT PrimIndex = INDEX_NONE;
		if (const_cast<AActor*>(ViewActor)->Components.FindItemByClass<UStaticMeshComponent>(&PrimComponent, &PrimIndex) == FALSE)
		{
			// If the mesh doesn't exist, we can't do any better job of determining if an update is needed.
			return ((TimeBetweenCaptures == 0 && LastCaptureTime == 0) ||
				(TimeBetweenCaptures > 0 && (ViewFamily.CurrentWorldTime - LastCaptureTime) >= TimeBetweenCaptures));
		}
		else if (PrimComponent->HiddenGame)
		{
			// We've got a mesh but it's not visible, so we need to explicitly do occlusion queries for it
			PrimComponent->UpdateBounds();

			// Check to see if the reflection plane actor static mesh is in the frustum for at least one view.
			UBOOL bIsVisible = FALSE;
			for (INT ViewIndex = 0; (ViewIndex < ViewFamily.Views.Num()) && !bIsVisible; ViewIndex++)
			{
				const FSceneView* const View = ViewFamily.Views(ViewIndex);

				if (View->ViewFrustum.IntersectBox(PrimComponent->Bounds.Origin, PrimComponent->Bounds.BoxExtent))
				{
					bIsVisible = TRUE;
				}
			}

			if (!bIsVisible)
			{
				return FALSE;
			}

			// Might be visible, do an occlusion query in every view to be sure
			FPrimitiveComponentOcclusionWrapper Primitive(PrimComponent, TRUE, FALSE);
			for (INT ViewIndex = 0; ViewIndex < ViewFamily.Views.Num(); ViewIndex++)
			{
				// NOTE: Assuming this cast is always safe!
				FViewInfo* View = (FViewInfo*)ViewFamily.Views(ViewIndex);
				FSceneViewState* SceneViewState = static_cast<FSceneViewState*>(View->State);
				checkSlow(SceneViewState != NULL);

				UBOOL bIsDefinitelyUnoccluded;
				UBOOL bIsOccluded = SceneViewState->UpdatePrimitiveOcclusion(Primitive, *View, ViewFamily.CurrentRealTime, TRUE, /*out*/bIsDefinitelyUnoccluded);

				if (!bIsOccluded)
				{
					// Update the last rendered time so the parent FSceneCaptureProbe call will determine that it's visible and needs an update
					const_cast<AActor*>(ViewActor)->LastRenderTime = ViewFamily.CurrentRealTime;
					break;
				}
			}
		}
	}

	return FSceneCaptureProbe::UpdateRequired(ViewFamily);
}

/**
* Called by the rendering thread to render the scene for the capture
* @param	MainSceneRenderer - parent scene renderer with info needed 
*			by some of the capture types.
*/
void FSceneCaptureProbeReflect::CaptureScene( FSceneRenderer* MainSceneRenderer )
{
	check(MainSceneRenderer);

	// render target resource to render with
	FTextureRenderTargetResource* RTResource = TextureTarget ? TextureTarget->GetRenderTargetResource() : NULL;
	if (RTResource != NULL && MainSceneRenderer->ViewFamily.Views.Num() && UpdateRequired(MainSceneRenderer->ViewFamily) &&
		GSceneRenderTargets.GetBufferSizeX() > 0 &&
		GSceneRenderTargets.GetBufferSizeY() > 0)
	{ 
		// make sure the render targets are clamped to the existing scene buffer sizes since we never render outside of the view region
		RTResource->ClampSize(GSceneRenderTargets.GetBufferSizeX(),GSceneRenderTargets.GetBufferSizeY());
		
		LastCaptureTime = MainSceneRenderer->ViewFamily.CurrentWorldTime;
		// should have a 2d render target resource
		check(RTResource->GetTextureRenderTarget2DResource());
		// create a temp view family for rendering the scene. use the same scene as parent
		FSceneViewFamilyContext ViewFamily(
			RTResource,
			(FSceneInterface*)MainSceneRenderer->Scene,
			ShowFlags,
			MainSceneRenderer->ViewFamily.CurrentWorldTime,
			MainSceneRenderer->ViewFamily.DeltaWorldTime,
			MainSceneRenderer->ViewFamily.CurrentRealTime, 
			FALSE, 
			FALSE, 
			FALSE, 
			TRUE, 
			TRUE, 
			1.0f, 
			FALSE, 
			TRUE	// Force the renderer to write its results to the RenderTarget
			);

		INT MaxViewIdx = GIsEditor ? 1 : MainSceneRenderer->ViewFamily.Views.Num(); 

		// allocate view state, if necessary
		if (ViewStates.Num() != MaxViewIdx)
		{
			ViewStates.Empty(MaxViewIdx);
			for (INT ViewIndex = 0; ViewIndex < MaxViewIdx; ViewIndex++)
			{
				ViewStates.AddItem(GUseOcclusionQueriesForSceneCaptures ? AllocateViewState() : NULL);
			}
		}

		// Get a list of primitives to hide from the capture
		TSet<UPrimitiveComponent*> HiddenPrimitives;
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		if (WorldInfo != NULL && WorldInfo->GRI != NULL)
		{
			WorldInfo->GRI->UpdateHiddenComponentsForSceneCapture(HiddenPrimitives);
		}

		for( INT ViewIdx=0; ViewIdx < MaxViewIdx; ViewIdx++ )
		{
			// use the first view as the parent
			FSceneView* ParentView = (FSceneView*)MainSceneRenderer->ViewFamily.Views(ViewIdx);

			// create a mirror matrix and premultiply the view transform by it
			FMirrorMatrix MirrorMatrix( MirrorPlane );
			FMatrix ViewMatrix(MirrorMatrix * ParentView->ViewMatrix);

			// transform the clip plane so that it is in view space
			FPlane MirrorPlaneViewSpace = MirrorPlane.TransformBy(ViewMatrix);

			// create a skewed projection matrix that aligns the near plane with the mirror plane
			// so that items behind the plane are properly clipped
			FClipProjectionMatrix ClipProjectionMatrix( ParentView->ProjectionMatrix, MirrorPlaneViewSpace );
 
			FLOAT X = (FLOAT)RTResource->GetSizeX() * ((FLOAT)ParentView->RenderTargetX / GSceneRenderTargets.GetBufferSizeX());
			FLOAT Y = (FLOAT)RTResource->GetSizeY() * ((FLOAT)ParentView->RenderTargetY / GSceneRenderTargets.GetBufferSizeY());
			FLOAT SizeX = (FLOAT)RTResource->GetSizeX() * ((FLOAT)ParentView->RenderTargetSizeX / GSceneRenderTargets.GetBufferSizeX());
			FLOAT SizeY = (FLOAT)RTResource->GetSizeY() * ((FLOAT)ParentView->RenderTargetSizeY / GSceneRenderTargets.GetBufferSizeY());

			// create a view for the capture
			FSceneView* View = new FSceneView(
				&ViewFamily,
				ViewStates(ViewIdx),
				ViewIdx,
				(const FSceneViewFamily*)&(MainSceneRenderer->ViewFamily),
				NULL,
				ViewActor,
				PostProcess,
				NULL,
				NULL,
				X,
				Y,
				SizeX,
				SizeY,
				ViewMatrix,
				ClipProjectionMatrix,
				BackgroundColor,
				FLinearColor(0.f,0.f,0.f,0.f),
				FLinearColor::White,
				HiddenPrimitives
				);

			
			// add the view to the family
			ViewFamily.Views.AddItem(View);
		}
		
		// create a new scene renderer for rendering the capture
		FSceneRenderer* CaptureSceneRenderer = ::new FSceneRenderer(
			&ViewFamily,
			NULL,
			MainSceneRenderer->CanvasTransform,
			TRUE
			);
		CaptureSceneRenderer->MaxViewDistanceSquaredOverride = (MaxViewDistanceOverrideSq > 0.0f) ? MaxViewDistanceOverrideSq : MAX_FLT;
		CaptureSceneRenderer->bUseDepthOnlyPass = !bSkipRenderingDepthPrepass;

		DOUBLE SavedTextureUpdateTime = RTResource->LastRenderTime;

		// render the scene to the target
		CaptureSceneRenderer->Render();

		// restore the texture update time if we're selectively updating based on it
		// so that any bits with the texture mapped in the capture don't count towards needing to render
		if (bSkipUpdateIfTextureUsersOccluded)
		{
			RTResource->LastRenderTime = SavedTextureUpdateTime;
		}

		// copy the results of the scene rendering from the target surface to its texture
		RHICopyToResolveTarget(RTResource->GetRenderTargetSurface(), FALSE, FResolveParams());

		delete CaptureSceneRenderer;
	}
}

/*-----------------------------------------------------------------------------
USceneCapturePortalComponent
-----------------------------------------------------------------------------*/

/**
* Attach a new portal capture component
*/
void USceneCapturePortalComponent::Attach()
{
	Super::Attach();
}

void USceneCapturePortalComponent::execSetCaptureParameters(FFrame& Stack, RESULT_DECL)
{
	P_GET_OBJECT_OPTX(UTextureRenderTarget2D, NewTextureTarget, TextureTarget);
	P_GET_FLOAT_OPTX(NewScaleFOV, ScaleFOV);
	P_GET_ACTOR_OPTX(NewViewDest, ViewDestination);
	P_FINISH;

	// set the parameters
	TextureTarget = NewTextureTarget;
	ScaleFOV = NewScaleFOV;
	ViewDestination = NewViewDest;

	// update the sync components
	ASceneCaptureActor* CaptureActor = Cast<ASceneCaptureActor>(GetOwner());
	if (CaptureActor != NULL)
	{
		CaptureActor->SyncComponents();
	}

	BeginDeferredReattach();
}

/**
* Create a new probe with info needed to render the scene
*/
FSceneCaptureProbe* USceneCapturePortalComponent::CreateSceneCaptureProbe() 
{
	// source actor is always owner
	AActor* SrcActor = Owner;
	// each portal has a destination actor (default to our Owner if one is not avaialble)
	AActor* DestActor = ViewDestination ? ViewDestination : Owner;
	check(SrcActor && DestActor);

	FRotator SrcRotation = SrcActor->Rotation;

	// form a transform that takes a point relative to the dest actor and places it 
	// in that same relative location with respect to the source actor.
	// Notice that all roll is take off the rotation in this calculation. This is intentional
	// because we don't want the actor's roll to cause the rendered scene to be rolled.
	FMatrix DestWorldToLocalM( FTranslationMatrix( -DestActor->Location ) * 
		FInverseRotationMatrix( FVector(DestActor->Rotation.Vector()).Rotation() )
		);

	// invert the source rotation because when viewing the portal we will be looking through the actor
	FMatrix SrcLocalToWorldM( FRotationMatrix( FVector(-SrcRotation.Vector()).Rotation() ) *
		FTranslationMatrix( SrcActor->Location )
		);

	// Transform for source to destination view
	FMatrix SrcToDestChangeBasisM = DestWorldToLocalM * SrcLocalToWorldM;

	// create the Clip plane (negated because we want to clip things BEHIND the source actor's plane
	FPlane ClipPlane = FPlane( SrcActor->Location, -SrcRotation.Vector() );

	// any info from the component needed for capturing the scene should be copied to the probe
	return new FSceneCaptureProbePortal(
		Owner,
		TextureTarget,
		GetSceneShowFlags(),
		FLinearColor(ClearColor),
		bEnabled ? FrameRate : 0,
		PostProcess,
		bUseMainScenePostProcessSettings,
		bSkipUpdateIfTextureUsersOccluded,
		bSkipUpdateIfOwnerOccluded,
		bSkipRenderingDepthPrepass,
		MaxUpdateDist,
		MaxStreamingUpdateDist,
		MaxViewDistanceOverride,
		SrcToDestChangeBasisM,
		DestActor,
		ClipPlane
		);
}

/*-----------------------------------------------------------------------------
FSceneCaptureProbePortal
-----------------------------------------------------------------------------*/

/**
* Called by the rendering thread to render the scene for the capture
* @param	MainSceneRenderer - parent scene renderer with info needed 
*			by some of the capture types.
*/
void FSceneCaptureProbePortal::CaptureScene( FSceneRenderer* MainSceneRenderer )
{
	// render target resource to render with
	FTextureRenderTargetResource* RTResource = TextureTarget ? TextureTarget->GetRenderTargetResource() : NULL;
	if (RTResource != NULL && MainSceneRenderer->ViewFamily.Views.Num() && UpdateRequired(MainSceneRenderer->ViewFamily) &&
		GSceneRenderTargets.GetBufferSizeX() > 0 && GSceneRenderTargets.GetBufferSizeY() > 0)
	{
		// make sure the render targets are clamped to the existing scene buffer sizes since we never render outside of the view region
		RTResource->ClampSize(GSceneRenderTargets.GetBufferSizeX(),GSceneRenderTargets.GetBufferSizeY());

		LastCaptureTime = MainSceneRenderer->ViewFamily.CurrentWorldTime;
		// should have a 2d render target resource
		check(RTResource->GetTextureRenderTarget2DResource());
		// create a temp view family for rendering the scene. use the same scene as parent
		FSceneViewFamilyContext ViewFamily(
			RTResource,
			(FSceneInterface*)MainSceneRenderer->Scene,
			ShowFlags,
			MainSceneRenderer->ViewFamily.CurrentWorldTime,
			MainSceneRenderer->ViewFamily.DeltaWorldTime,
			MainSceneRenderer->ViewFamily.CurrentRealTime, 
			FALSE, 
			FALSE, 
			FALSE, 
			TRUE, 
			TRUE, 
			1.0f, 
			FALSE, 
			TRUE	// Force the renderer to write its results to the RenderTarget
			);

		// all prim components from the destination portal should be hidden
		TSet<UPrimitiveComponent*> HiddenPrimitives;
		for( INT CompIdx=0; CompIdx < DestViewActor->Components.Num(); CompIdx++ )
		{
			UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(DestViewActor->Components(CompIdx));
			if( PrimitiveComponent && !PrimitiveComponent->bIgnoreHiddenActorsMembership )
			{
				HiddenPrimitives.Add(PrimitiveComponent);
			}
		}

		INT MaxViewIdx = GIsEditor ? 1 : MainSceneRenderer->ViewFamily.Views.Num(); 

		// allocate view state, if necessary
		if (ViewStates.Num() != MaxViewIdx)
		{
			ViewStates.Empty(MaxViewIdx);
			for (INT ViewIndex = 0; ViewIndex < MaxViewIdx; ViewIndex++)
			{
				ViewStates.AddItem(GUseOcclusionQueriesForSceneCaptures ? AllocateViewState() : NULL);
			}
		}

		for( INT ViewIdx=0; ViewIdx < MaxViewIdx; ViewIdx++ )
		{
			// use the first view as the parent
			FSceneView* ParentView = (FSceneView*)MainSceneRenderer->ViewFamily.Views(ViewIdx);

			// transform view matrix so that it is relative to Destination 
			FMatrix ViewMatrix = SrcToDestChangeBasisM * ParentView->ViewMatrix;			
			
			// transform the clip plane so that it is in view space
			FPlane MirrorPlaneViewSpace = ClipPlane.TransformBy(ParentView->ViewMatrix);

			// create a skewed projection matrix that aligns the near plane with the mirror plane
			// so that items behind the plane are properly clipped
			FClipProjectionMatrix ClipProjectionMatrix( ParentView->ProjectionMatrix, MirrorPlaneViewSpace );

			FLOAT X = (FLOAT)RTResource->GetSizeX() * ((FLOAT)ParentView->RenderTargetX / GSceneRenderTargets.GetBufferSizeX());
			FLOAT Y = (FLOAT)RTResource->GetSizeY() * ((FLOAT)ParentView->RenderTargetY / GSceneRenderTargets.GetBufferSizeY());
			FLOAT SizeX = (FLOAT)RTResource->GetSizeX() * ((FLOAT)ParentView->RenderTargetSizeX / GSceneRenderTargets.GetBufferSizeX());
			FLOAT SizeY = (FLOAT)RTResource->GetSizeY() * ((FLOAT)ParentView->RenderTargetSizeY / GSceneRenderTargets.GetBufferSizeY());

			// create a view for the capture
			FSceneView* View = new FSceneView(
				&ViewFamily,
				ViewStates(ViewIdx),
				ViewIdx,
				(const FSceneViewFamily*)&(MainSceneRenderer->ViewFamily),
				NULL,
				ViewActor,
				PostProcess,
				NULL,
				NULL,
				X,
				Y,
				SizeX,
				SizeY,
				ViewMatrix,
				ClipProjectionMatrix,
				BackgroundColor,
				FLinearColor(0.f,0.f,0.f,0.f),
				FLinearColor::White,
				HiddenPrimitives
				);
			// add the view to the family
			ViewFamily.Views.AddItem(View);
		}

		// create a new scene renderer for rendering the capture
		FSceneRenderer* CaptureSceneRenderer = ::new FSceneRenderer(
			&ViewFamily,
			NULL,
			MainSceneRenderer->CanvasTransform,
			TRUE
			);
		CaptureSceneRenderer->MaxViewDistanceSquaredOverride = (MaxViewDistanceOverrideSq > 0.0f) ? MaxViewDistanceOverrideSq : MAX_FLT;
		CaptureSceneRenderer->bUseDepthOnlyPass = !bSkipRenderingDepthPrepass;

		DOUBLE SavedTextureUpdateTime = RTResource->LastRenderTime;

		// render the scene to the target
		CaptureSceneRenderer->Render();

		// restore the texture update time if we're selectively updating based on it
		// so that any bits with the texture mapped in the capture don't count towards needing to render
		if (bSkipUpdateIfTextureUsersOccluded)
		{
			RTResource->LastRenderTime = SavedTextureUpdateTime;
		}
 
		// copy the results of the scene rendering from the target surface to its texture
		RHICopyToResolveTarget(RTResource->GetRenderTargetSurface(), FALSE, FResolveParams());

		delete CaptureSceneRenderer;
	}

}

/**
*	Return the location of the ViewActor of the probe.
*
*	@param  FVector		The location of the probes ViewActor
*	@return	TRUE if view actor exist and location is valid
*/
UBOOL FSceneCaptureProbePortal::GetViewActorLocation(FVector & ViewLocation) const
{
	if (DestViewActor)
	{
		ViewLocation = DestViewActor->Location;
		return TRUE;
	}

	return FSceneCaptureProbe::GetViewActorLocation(ViewLocation);
}

/*-----------------------------------------------------------------------------
ASceneCaptureActor
-----------------------------------------------------------------------------*/

/** synchronize components when a property changes */
void ASceneCaptureActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	SyncComponents();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/** synchronize components after load */
void ASceneCaptureActor::PostLoad()
{
	Super::PostLoad();
	SyncComponents();
}

#if WITH_EDITOR
void ASceneCaptureActor::CheckForErrors()
{
	Super::CheckForErrors();
	if( SceneCapture == NULL )
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_SceneCaptureNull" ), *GetName() ) ), TEXT( "SceneCaptureNull" ) );
	}
}
#endif

/*-----------------------------------------------------------------------------
ASceneCapture2DActor
-----------------------------------------------------------------------------*/

/**
 * Sync updated properties
 */
void ASceneCapture2DActor::SyncComponents()
{
	USceneCapture2DComponent* SceneCapture2D = Cast<USceneCapture2DComponent>(SceneCapture);

	// update the draw frustum component to match the scene capture settings
	if( DrawFrustum &&
		SceneCapture2D )
	{
		//@r2t todo - texture on near plane doesn't display ATM
		// set texture to display on near plane
        DrawFrustum->Texture = SceneCapture2D->TextureTarget;
		// FOV
		DrawFrustum->FrustumAngle = SceneCapture2D->FieldOfView;
        // near/far plane
		const FLOAT MIN_NEAR_DIST = 50.f;
		const FLOAT MIN_FAR_DIST = 200.f;
		DrawFrustum->FrustumStartDist = Max( SceneCapture2D->NearPlane, MIN_NEAR_DIST );
		DrawFrustum->FrustumEndDist = Max( SceneCapture2D->FarPlane, MIN_FAR_DIST );
		// update aspect ratio
		if( SceneCapture2D->TextureTarget )
		{
			FLOAT fAspect = (FLOAT)SceneCapture2D->TextureTarget->SizeX / (FLOAT)SceneCapture2D->TextureTarget->SizeY;
			DrawFrustum->FrustumAspectRatio = fAspect;
		}
	}
}

/*-----------------------------------------------------------------------------
ASceneCaptureCubeMapActor
-----------------------------------------------------------------------------*/

/**
* Init the helper components 
*/
void ASceneCaptureCubeMapActor::Init()
{
	if( GEngine->SceneCaptureCubeActorMaterial && !CubeMaterialInst )
	{
		// create new instance
		// note that it has to be RF_Transient or the map won't save
		CubeMaterialInst = ConstructObject<UMaterialInstanceConstant>( 
			UMaterialInstanceConstant::StaticClass(),
			INVALID_OBJECT, 
			NAME_None, 
			RF_Transient, 
			NULL );

		// init with the parent material
		CubeMaterialInst->SetParent( GEngine->SceneCaptureCubeActorMaterial );
	}

	if( StaticMesh && CubeMaterialInst )
	{
		// assign the material instance to the static mesh plane
		if( StaticMesh->Materials.Num() == 0 )
		{
			// add a slot if needed
			StaticMesh->Materials.Add();
		}
		// only one material entry
		StaticMesh->Materials(0) = CubeMaterialInst;
	}
}

/**
* Called when the actor is loaded
*/
void ASceneCaptureCubeMapActor::PostLoad()
{
	Super::PostLoad();

	// initialize components
	Init();	

	// update
	SyncComponents();
}

/**
* Called when the actor is spawned
*/
void ASceneCaptureCubeMapActor::Spawned()
{
	Super::Spawned();

	// initialize components
	Init();	

	// update
	SyncComponents();
}

/**
* Called when the actor is destroyed
*/
void ASceneCaptureCubeMapActor::FinishDestroy()
{
	// clear the references to the cube material instance
	if( StaticMesh )
	{
		for( INT i=0; i < StaticMesh->Materials.Num(); i++ )
		{
			if( StaticMesh->Materials(i) == CubeMaterialInst ) {
				StaticMesh->Materials(i) = NULL;
			}
		}
	}
	CubeMaterialInst = NULL;

	Super::FinishDestroy();
}

/**
* Sync updated properties
*/
void ASceneCaptureCubeMapActor::SyncComponents()
{
	USceneCaptureCubeMapComponent* SceneCaptureCube = Cast<USceneCaptureCubeMapComponent>(SceneCapture);

	if( !SceneCaptureCube )
		return;

	if( CubeMaterialInst )
	{
		// update the material instance texture parameter
		CubeMaterialInst->SetTextureParameterValue( FName(TEXT("TexCube")), SceneCaptureCube->TextureTarget );        
	}
}

/*-----------------------------------------------------------------------------
ASceneCaptureReflectActor
-----------------------------------------------------------------------------*/

/**
 * Init the helper components 
 */
void ASceneCaptureReflectActor::Init()
{
	if( GEngine->SceneCaptureReflectActorMaterial && !ReflectMaterialInst )
	{
		// create new instance
		// note that it has to be RF_Transient or the map won't save
		ReflectMaterialInst = ConstructObject<UMaterialInstanceConstant>( 
			UMaterialInstanceConstant::StaticClass(),
			INVALID_OBJECT, 
			NAME_None, 
			RF_Transient, 
			NULL );

		// init with the parent material
		ReflectMaterialInst->SetParent( GEngine->SceneCaptureReflectActorMaterial );		
	}

	if( StaticMesh && ReflectMaterialInst )
	{
		// assign the material instance to the static mesh plane
		if( StaticMesh->Materials.Num() == 0 )
		{
			// add a slot if needed
			StaticMesh->Materials.Add();
		}
		// only one material entry
		StaticMesh->Materials(0) = ReflectMaterialInst;
	}
}

/**
 * Called when the actor is loaded
 */
void ASceneCaptureReflectActor::PostLoad()
{
	Super::PostLoad();

	// initialize components
    Init();	

	// update
	SyncComponents();
}

/**
 * Called when the actor is spawned
 */
void ASceneCaptureReflectActor::Spawned()
{
	Super::Spawned();

	// initialize components
    Init();	

	// update
	SyncComponents();
}

/**
 * Called when the actor is destroyed
 */
void ASceneCaptureReflectActor::FinishDestroy()
{
	// clear the references to the reflect material instance
	if( StaticMesh )
	{
        for( INT i=0; i < StaticMesh->Materials.Num(); i++ )
		{
			if( StaticMesh->Materials(i) == ReflectMaterialInst ) {
				StaticMesh->Materials(i) = NULL;
			}
		}
	}
	ReflectMaterialInst = NULL;

	Super::FinishDestroy();
}

/**
 * Sync updated properties
 */
void ASceneCaptureReflectActor::SyncComponents()
{
	USceneCaptureReflectComponent* SceneCaptureReflect = Cast<USceneCaptureReflectComponent>(SceneCapture);	

	if( !SceneCaptureReflect )
		return;

	if( ReflectMaterialInst )
	{
		// update the material instance texture parameter
        ReflectMaterialInst->SetTextureParameterValue( FName(TEXT("ScreenTex")), SceneCaptureReflect->TextureTarget );        
	}
}

/*-----------------------------------------------------------------------------
ASceneCapturePortalActor
-----------------------------------------------------------------------------*/

/**
* Sync updated properties
*/
void ASceneCapturePortalActor::SyncComponents()
{
	USceneCapturePortalComponent* SceneCapturePortal = Cast<USceneCapturePortalComponent>(SceneCapture);

	if( !SceneCapturePortal )
		return;

	if( ReflectMaterialInst )
	{
		// update the material instance texture parameter
		ReflectMaterialInst->SetTextureParameterValue( FName(TEXT("ScreenTex")), SceneCapturePortal->TextureTarget );        
	}
}


/*-----------------------------------------------------------------------------
APortalTeleporter
-----------------------------------------------------------------------------*/

void APortalTeleporter::Spawned()
{
	// create a render to texture target for the portal
	USceneCapturePortalComponent* PortalComponent = Cast<USceneCapturePortalComponent>(SceneCapture);
	if (PortalComponent != NULL)
	{
		PortalComponent->TextureTarget = CreatePortalTexture();
	}
	Super::Spawned();
}

void APortalTeleporter::PostLoad()
{
	// create a render to texture target for the portal
	USceneCapturePortalComponent* PortalComponent = Cast<USceneCapturePortalComponent>(SceneCapture);
	if (PortalComponent != NULL)
	{
		PortalComponent->TextureTarget = CreatePortalTexture();
		// also make sure destination is correct
		PortalComponent->ViewDestination = SisterPortal;
	}
	Super::PostLoad();
}

void APortalTeleporter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	USceneCapturePortalComponent* PortalComponent = Cast<USceneCapturePortalComponent>(SceneCapture);
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged != NULL)
	{
		if (PropertyThatChanged->GetFName() == FName(TEXT("TextureResolutionX")) || PropertyThatChanged->GetFName() == FName(TEXT("TextureResolutionY")))
		{
			// enforce valid positive power of 2 values for resolution
			if (TextureResolutionX <= 2)
			{
				TextureResolutionX = 2;
			}
			else
			{
				TextureResolutionX = appRoundUpToPowerOfTwo(TextureResolutionX);
			}
			if (TextureResolutionY <= 2)
			{
				TextureResolutionY = 2;
			}
			else
			{
				TextureResolutionY = appRoundUpToPowerOfTwo(TextureResolutionY);
			}
			if (PortalComponent != NULL)
			{
				if (PortalComponent->TextureTarget == NULL)
				{
					PortalComponent->TextureTarget = CreatePortalTexture();
				}
				else
				{
					// update the size of the texture
					PortalComponent->TextureTarget->Init(
						TextureResolutionX,TextureResolutionY,(EPixelFormat)PortalComponent->TextureTarget->Format);
				}
			}
		}
	}

	// propagate bMovable
	if (bMovablePortal != bMovable)
	{
		bMovable = bMovablePortal;
		// bMovable affects static shadows, so lightning must be rebuilt if this flag is changed
		GWorld->GetWorldInfo()->SetMapNeedsLightingFullyRebuilt( TRUE );
	}

	if (PortalComponent != NULL)
	{
		if(PropertyThatChanged)
		{
			// try to update the sister portal if the view destination was set
			if (PropertyThatChanged->GetFName() == FName(TEXT("ViewDestination")))
			{
				SisterPortal = Cast<APortalTeleporter>(PortalComponent->ViewDestination);
			}
			// make sure the component has the proper view destination
			if (PropertyThatChanged->GetFName() == FName(TEXT("SisterPortal")))
			{
				PortalComponent->ViewDestination = SisterPortal;
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

UTextureRenderTarget2D* APortalTeleporter::CreatePortalTexture()
{
	if (TextureResolutionX <= 2 || TextureResolutionY <= 2)
	{
		debugf(NAME_Warning, TEXT("%s cannot create portal texture because invalid resolution specified"), *GetName());
		return NULL;
	}
	else
	{
		// make sure resolution is a power of 2
		TextureResolutionX = appRoundUpToPowerOfTwo(TextureResolutionX);
		TextureResolutionY = appRoundUpToPowerOfTwo(TextureResolutionY);
		// create and initialize the texture
		UTextureRenderTarget2D* PortalTexture = CastChecked<UTextureRenderTarget2D>(StaticConstructObject(UTextureRenderTarget2D::StaticClass(), GetOuter(), NAME_None, RF_Transient));
		PortalTexture->Init(TextureResolutionX, TextureResolutionY, PF_A8R8G8B8);
		return PortalTexture;
	}
}

UBOOL APortalTeleporter::TransformActor(AActor* A)
{
	// if there's no destination or we're not allowed to teleport the actor, abort
	USceneCapturePortalComponent* PortalComponent = Cast<USceneCapturePortalComponent>(SceneCapture);
	if (SisterPortal == NULL || PortalComponent == NULL || !CanTeleport(A))
	{
		return FALSE;
	}

	FMatrix WorldToLocalM = WorldToLocal();
	FMatrix SisterLocalToWorld = SisterPortal->LocalToWorld();

	FVector LocalLocation = WorldToLocalM.TransformFVector(A->Location);
	LocalLocation.X *= -1.f;
	LocalLocation = SisterLocalToWorld.TransformFVector(LocalLocation);

	if (!GWorld->FarMoveActor(A, LocalLocation, 0, 0, 0))
	{
		return FALSE;
	}

	FRotationMatrix SourceAxes(Rotation);
	FVector SourceXAxis = SourceAxes.GetAxis(0);
	FVector SourceYAxis = SourceAxes.GetAxis(1);
	FVector SourceZAxis = SourceAxes.GetAxis(2);

	FRotationMatrix DestAxes(SisterPortal->Rotation);
	FVector DestXAxis = DestAxes.GetAxis(0);
	FVector DestYAxis = DestAxes.GetAxis(1);
	FVector DestZAxis = DestAxes.GetAxis(2);

	// transform velocity
	FVector LocalVelocity = A->Velocity;
	FVector LocalVector;
	LocalVector.X = SourceXAxis | LocalVelocity;
	LocalVector.Y = SourceYAxis | LocalVelocity;
	LocalVector.Z = SourceZAxis | LocalVelocity;
	A->Velocity = LocalVector.X * DestXAxis + LocalVector.Y * DestYAxis + LocalVector.Z * DestZAxis;

	// transform acceleration
	FVector LocalAcceleration = A->Acceleration;
	LocalVector.X = SourceXAxis | LocalAcceleration;
	LocalVector.Y = SourceYAxis | LocalAcceleration;
	LocalVector.Z = SourceZAxis | LocalAcceleration;
	A->Acceleration = LocalVector.X * DestXAxis + LocalVector.Y * DestYAxis + LocalVector.Z * DestZAxis;

	// transform rotation
	INT SavedRoll = A->Rotation.Roll;
	FVector RotDir = A->Rotation.Vector();
	LocalVector.X = SourceXAxis | RotDir;
	LocalVector.Y = SourceYAxis | RotDir;
	LocalVector.Z = SourceZAxis | RotDir;
	RotDir = LocalVector.X * DestXAxis + LocalVector.Y * DestYAxis + LocalVector.Z * DestZAxis;
	FRotator NewRotation = RotDir.Rotation();
	NewRotation.Roll = SavedRoll;
	FCheckResult Hit(1.f);
	GWorld->MoveActor(A, FVector(0,0,0), NewRotation, 0, Hit);

	APawn* P = A->GetAPawn();
	if (P != NULL && P->Controller != NULL)
	{
		// transform Controller rotation
		SavedRoll = P->Controller->Rotation.Roll;
		RotDir = P->Controller->Rotation.Vector();
		LocalVector.X = SourceXAxis | RotDir;
		LocalVector.Y = SourceYAxis | RotDir;
		LocalVector.Z = SourceZAxis | RotDir;
		RotDir = LocalVector.X * DestXAxis + LocalVector.Y * DestYAxis + LocalVector.Z * DestZAxis;
		NewRotation = RotDir.Rotation();
		NewRotation.Roll = SavedRoll;
		GWorld->MoveActor(P->Controller, FVector(0,0,0), NewRotation, 0, Hit);

		P->Anchor = MyMarker;
		P->Controller->MoveTimer = -1.0f;
	}

	return TRUE;
}

FVector APortalTeleporter::TransformVectorDir(FVector V)
{
	USceneCapturePortalComponent* PortalComponent = Cast<USceneCapturePortalComponent>(SceneCapture);
	if (!SisterPortal || PortalComponent == NULL)
		return V;

	FRotationMatrix SourceAxes(Rotation);
	FVector SourceXAxis = SourceAxes.GetAxis(0);
	FVector SourceYAxis = SourceAxes.GetAxis(1);
	FVector SourceZAxis = SourceAxes.GetAxis(2);

	FRotationMatrix DestAxes(SisterPortal->Rotation);
	FVector DestXAxis = DestAxes.GetAxis(0);
	FVector DestYAxis = DestAxes.GetAxis(1);
	FVector DestZAxis = DestAxes.GetAxis(2);
	FVector LocalVector;

	// transform velocity
	LocalVector.X = SourceXAxis | V;
	LocalVector.Y = SourceYAxis | V;
	LocalVector.Z = SourceZAxis | V;
	return LocalVector.X * DestXAxis + LocalVector.Y * DestYAxis + LocalVector.Z * DestZAxis;
}

FVector APortalTeleporter::TransformHitLocation(FVector HitLocation)
{
	USceneCapturePortalComponent* PortalComponent = Cast<USceneCapturePortalComponent>(SceneCapture);
	if (!SisterPortal || PortalComponent == NULL)
		return HitLocation;

	FMatrix WorldToLocalM = WorldToLocal();
	FMatrix SisterLocalToWorld = SisterPortal->LocalToWorld();

	FVector LocalLocation = WorldToLocalM.TransformFVector(HitLocation);
	LocalLocation.X *= -1.f;
	return SisterLocalToWorld.TransformFVector(LocalLocation);
}

void APortalTeleporter::TickSpecial(FLOAT DeltaTime)
{
	Super::TickSpecial(DeltaTime);

	if (SisterPortal != NULL)
	{
		// update ourselves in Controllers' VisiblePortals array
		FVisiblePortalInfo PortalInfo(this, SisterPortal);
		for (AController* C = WorldInfo->ControllerList; C != NULL; C = C->NextController)
		{
			if (C->SightCounter < 0.0f)
			{
				FCheckResult Hit(1.0f);
				if (GWorld->SingleLineCheck(Hit, this, Location, C->GetViewTarget()->Location, TRACE_World | TRACE_StopAtAnyHit | TRACE_ComplexCollision))
				{
					// we are visible to C
					C->VisiblePortals.AddUniqueItem(PortalInfo);
				}
				else
				{
					C->VisiblePortals.RemoveItem(PortalInfo);
				}
			}
		}
	}
}

UBOOL APortalTeleporter::CanTeleport(AActor* A)
{
	if (A == NULL)
	{
		return FALSE;
	}
	else if (bAlwaysTeleportNonPawns && A->GetAPawn() == NULL)
	{
		return TRUE;
	}
	else
	{
		return (A->bCanTeleport && (bCanTeleportVehicles || Cast<AVehicle>(A) == NULL));
	}
}

/*-----------------------------------------------------------------------------
FSceneCaptureProxy
-----------------------------------------------------------------------------*/

/**
* Constructor
*
* @param InViewport - parent viewport to use for rendering
* @param InParentViewFamily - view family of parent needed for rendering a scene capture
*/
FSceneCaptureProxy::FSceneCaptureProxy(FViewport* InViewport, const FSceneViewFamily* InParentViewFamily)
: Viewport(InViewport)
, ParentViewFamily(InParentViewFamily)
{	
}

/**
* Render the scene capture probe without relying on regular viewport rendering of the scene
* Note that this call will Begin/End frame without swapping
*
* @param CaptureProbe - the probe to render, will be deleted after rendering is completed on the render thread
* @param bFlushRendering - TRUE if render commands should be flushed and block for it to finish
*/
void FSceneCaptureProxy::Render(FSceneCaptureProbe* CaptureProbe,UBOOL bFlushRendering)
{
	// create a temp parent scene renderer which will get deleted after we render the scene
	FSceneRenderer* SceneRenderer = new FSceneRenderer(ParentViewFamily,NULL,FCanvas::CalcBaseTransform2D(Viewport->GetSizeX(),Viewport->GetSizeY()),TRUE);
	
	// parameters to use on ther render thread
	struct FCaptureParams
	{
		FSceneCaptureProbe* CaptureProbe;
		FViewport* Viewport;
		FSceneRenderer*	SceneRenderer;	
	};
	
	FCaptureParams CaptureParams =
	{
		CaptureProbe,
		Viewport,
		SceneRenderer,
	};

	// queue scene capture on render thread
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		SceneCaptureProxyRenderCommand,
		FCaptureParams,Params,CaptureParams,
	{
		// render the scene capture probe 
		Params.Viewport->BeginRenderFrame();
		FMemMark MemStackMark(GRenderingThreadMemStack);
		Params.CaptureProbe->CaptureScene(Params.SceneRenderer);
		Params.Viewport->EndRenderFrame(FALSE,FALSE);

		// delete the probe and the scene renderer on the render thread when finished
		delete Params.CaptureProbe;
		delete Params.SceneRenderer;			
	});
	// force flush if needed
	if( bFlushRendering )
	{
		FlushRenderingCommands();
	}
}