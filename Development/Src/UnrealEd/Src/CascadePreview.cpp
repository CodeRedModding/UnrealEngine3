/*=============================================================================
	CascadePreview.cpp: 'Cascade' particle editor preview pane
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "EngineParticleClasses.h"

static const DOUBLE ResetInterval = 0.5f;

/*-----------------------------------------------------------------------------
FCascadePreviewViewportClient
-----------------------------------------------------------------------------*/

FCascadePreviewViewportClient::FCascadePreviewViewportClient(WxCascade* InCascade)
{
	Cascade = InCascade;
	Level = Cascade->PreviewLevel;
	TimeScale = 1.f;

	SkyLightComponent = ConstructObject<USkyLightComponent>(USkyLightComponent::StaticClass());
	SkyLightComponent->Scene = Level;
	SkyLightComponent->SetParentToWorld( FMatrix::Identity );
	SkyLightComponent->Created();

	DirectionalLightComponent = ConstructObject<UDirectionalLightComponent>(UDirectionalLightComponent::StaticClass());
	DirectionalLightComponent->Scene = Level;
	DirectionalLightComponent->Created();

	// Preview lighting parameters
	SkyBrightness = 0.25f;
	SkyColor = FColor(255, 255, 255);

	Brightness = 1.f;
	Color = FColor(255, 255, 255);

	LightDirection = 45.f;
	LightElevation = 45.f;

	// Update light components from parameters now
	UpdateLighting();

	// Default view position
	// TODO: Store in ParticleSystem
	ViewLocation = FVector(-200.f, 0.f, 0.f);
	ViewRotation = FRotator(0, 0, 0);	

	PreviewAngle = FRotator(0, 0, 0);
	PreviewDistance = 0.f;

	// Use game defaults to hide emitter sprite etc.
	ShowFlags = SHOW_DefaultGame;

	bDrawOriginAxes = false;
	bDrawParticleCounts = true;

	Realtime = 1;
}

FCascadePreviewViewportClient::~FCascadePreviewViewportClient()
{
	if(SkyLightComponent->Initialized)
		SkyLightComponent->Destroyed();

	delete SkyLightComponent;
	SkyLightComponent = NULL;

	if(DirectionalLightComponent->Initialized)
		DirectionalLightComponent->Destroyed();

	delete DirectionalLightComponent;
	DirectionalLightComponent = NULL;
}

FColor FCascadePreviewViewportClient::GetBackgroundColor()
{
	return FColor(0,0,0);
}

void FCascadePreviewViewportClient::UpdateLighting()
{
	SkyLightComponent->PreEditChange();
	DirectionalLightComponent->PreEditChange();

	SkyLightComponent->Brightness = SkyBrightness;
	SkyLightComponent->Color = SkyColor;

	DirectionalLightComponent->Brightness = Brightness;
	DirectionalLightComponent->Color = Color;
	DirectionalLightComponent->SetParentToWorld( FRotationMatrix( FRotator(-LightElevation*(65535.f/360.f), LightDirection*(65535.f/360.f), 0.f) ) );

	SkyLightComponent->PostEditChange(NULL);
	DirectionalLightComponent->PostEditChange(NULL);
}

void FCascadePreviewViewportClient::Draw(FChildViewport* Viewport, FRenderInterface* RI)
{
	if(bDrawOriginAxes)
	{
		FVector XAxis(1,0,0); 
		FVector YAxis(0,1,0); 
		FVector ZAxis(0,0,1);

		FMatrix ArrowMatrix = FMatrix(XAxis, YAxis, ZAxis, FVector(0.f));
		Level->LineBatcher->DrawDirectionalArrow(ArrowMatrix, FColor(255,0,0), 10.f);

		ArrowMatrix = FMatrix(YAxis, ZAxis, XAxis, FVector(0.f));
		Level->LineBatcher->DrawDirectionalArrow(ArrowMatrix, FColor(0,255,0), 10.f);

		ArrowMatrix = FMatrix(ZAxis, XAxis, YAxis, FVector(0.f));
		Level->LineBatcher->DrawDirectionalArrow(ArrowMatrix, FColor(0,0,255), 10.f);
	}

	FEditorLevelViewportClient::Draw(Viewport, RI);

	if (bDrawParticleCounts)
	{
		// 'Up' from the lower left...
		FString strOutput;
		INT XPosition = Viewport->GetSizeX() - 70;
		INT YPosition = Viewport->GetSizeY() - 15;

		UParticleSystemComponent* PartComp = Cascade->PreviewEmitter->PartSysComp;

		if (PartComp->EmitterInstances.Num())
		{
			for (INT i = 0; i < PartComp->EmitterInstances.Num(); i++)
			{
				FParticleEmitterInstance* Instance = PartComp->EmitterInstances(i);
			
				if (Instance->Template->EmitterRenderMode != ERM_None)
				{
					strOutput = FString::Printf(TEXT("%4d/%4d"), Instance->ActiveParticles, 
						Instance->Template->PeakActiveParticles);

					RI->DrawShadowedString(XPosition, YPosition, *strOutput, GEngine->TinyFont, 
						Instance->Template->EmitterEditorColor);
					YPosition -= 12;
				}
			}
		}
		else
		{
			for (INT i = 0; i < PartComp->Template->Emitters.Num(); i++)
			{
				UParticleEmitter* Emitter = PartComp->Template->Emitters(i);
				
				if (Emitter->EmitterRenderMode != ERM_None)
				{
					strOutput = FString::Printf(TEXT("%4d/%4d"), 0, Emitter->PeakActiveParticles);

					RI->DrawShadowedString(XPosition, YPosition, *strOutput, GEngine->SmallFont, 
						Emitter->EmitterEditorColor);
					YPosition -= 12;
				}
			}
		}
	}
}

void FCascadePreviewViewportClient::InputKey(FChildViewport* Viewport, FName Key, EInputEvent Event,FLOAT)
{
	Viewport->CaptureMouseInput( Viewport->KeyState(KEY_LeftMouseButton) || Viewport->KeyState(KEY_MiddleMouseButton) || Viewport->KeyState(KEY_RightMouseButton) );


}

void FCascadePreviewViewportClient::MouseMove(FChildViewport* Viewport, INT X, INT Y)
{

}

void FCascadePreviewViewportClient::InputAxis(FChildViewport* Viewport, FName Key, FLOAT Delta, FLOAT DeltaTime)
{
/***
	if((Key == KEY_MouseX || Key == KEY_MouseY) && Delta != 0.0f)
	{
		UBOOL LeftMouseButton = Viewport->KeyState(KEY_LeftMouseButton);
		UBOOL MiddleMouseButton = Viewport->KeyState(KEY_MiddleMouseButton);
		UBOOL RightMouseButton = Viewport->KeyState(KEY_RightMouseButton);

		FLOAT DX = (Key == KEY_MouseX) ? Delta : 0.0f;
		FLOAT DY = (Key == KEY_MouseY) ? Delta : 0.0f;

		FLOAT YawSpeed = 20.0f;
		FLOAT PitchSpeed = 20.0f;
		FLOAT ZoomSpeed = 1.0f;

		FRotator NewPreviewAngle = PreviewAngle;
		FLOAT NewPreviewDistance = PreviewDistance;

		if(LeftMouseButton != RightMouseButton)
		{
			NewPreviewAngle.Yaw += DX * YawSpeed;
			NewPreviewAngle.Pitch += DY * PitchSpeed;
			NewPreviewAngle.Clamp();
		}
		else if(LeftMouseButton && RightMouseButton)
		{
			NewPreviewDistance += DY * ZoomSpeed;
			NewPreviewDistance = Clamp( NewPreviewDistance, 0.f, 100000.f);
		}
		
		SetPreviewCamera(NewPreviewAngle, NewPreviewDistance);
	}
***/

	if(Input->InputAxis(Key,Delta,DeltaTime))
		return;

	if((Key == KEY_MouseX || Key == KEY_MouseY) && Delta != 0.0f)
	{
		UBOOL	LeftMouseButton = Viewport->KeyState(KEY_LeftMouseButton),
				MiddleMouseButton = Viewport->KeyState(KEY_MiddleMouseButton),
				RightMouseButton = Viewport->KeyState(KEY_RightMouseButton);

		MouseDeltaTracker.AddDelta( this, Key, Delta, 0 );
		FVector DragDelta = MouseDeltaTracker.GetDelta();

		GEditor->MouseMovement += DragDelta;

		if( !DragDelta.IsZero() )
		{
			// Convert the movement delta into drag/rotation deltas

			FVector Drag;
			FRotator Rot;
			FVector Scale;
			MouseDeltaTracker.ConvertMovementDeltaToDragRot( this, DragDelta, Drag, Rot, Scale );

			if(Cascade->bOrbitMode)
			{
				FLOAT DX = (Key == KEY_MouseX) ? Delta : 0.0f;
				FLOAT DY = (Key == KEY_MouseY) ? Delta : 0.0f;

				FLOAT YawSpeed = 20.0f;
				FLOAT PitchSpeed = 20.0f;
				FLOAT ZoomSpeed = 1.0f;

				FRotator NewPreviewAngle = PreviewAngle;
				FLOAT NewPreviewDistance = PreviewDistance;

				if(LeftMouseButton != RightMouseButton)
				{
					NewPreviewAngle.Yaw += DX * YawSpeed;
					NewPreviewAngle.Pitch += DY * PitchSpeed;
					NewPreviewAngle.Clamp();
				}
				else if(LeftMouseButton && RightMouseButton)
				{
					NewPreviewDistance += DY * ZoomSpeed;
					NewPreviewDistance = Clamp( NewPreviewDistance, 0.f, 100000.f);
				}
				
				SetPreviewCamera(NewPreviewAngle, NewPreviewDistance);
			}
			else
			{
				MoveViewportCamera(Drag, Rot);
			}

			MouseDeltaTracker.ReduceBy( DragDelta );
		}
	}

	Viewport->Invalidate();
}

void FCascadePreviewViewportClient::Tick(FLOAT DeltaSeconds)
{
	FEditorLevelViewportClient::Tick(DeltaSeconds);

	// Don't bother ticking at all if paused.
	if(TimeScale > KINDA_SMALL_NUMBER)
	{
		FLOAT fSaveUpdateDelta = Cascade->PartSys->UpdateTime_Delta;
		if (TimeScale < 1.0f)
			Cascade->PartSys->UpdateTime_Delta *= TimeScale;
		Level->Tick(LEVELTICK_All, TimeScale * DeltaSeconds);
		Cascade->PartSys->UpdateTime_Delta = fSaveUpdateDelta;
	}

	// If we are doing auto-reset
	if(Cascade->bResetOnFinish)
	{
		UParticleSystemComponent* PartComp = Cascade->PreviewEmitter->PartSysComp;

		// If system has finish, pause for a bit before resetting.
		if(Cascade->bPendingReset)
		{
			if(Level->TimeSeconds > Cascade->ResetTime)
			{
				PartComp->ResetParticles();
				PartComp->InitParticles();

				Cascade->bPendingReset = false;
			}
		}
		else
		{
			if( PartComp->HasCompleted() )
			{
				Cascade->bPendingReset = true;
				Cascade->ResetTime = Level->TimeSeconds + ResetInterval;
			}
		}
	}
}

void FCascadePreviewViewportClient::SetPreviewCamera(const FRotator& NewPreviewAngle, FLOAT NewPreviewDistance)
{
	PreviewAngle = NewPreviewAngle;
	PreviewDistance = NewPreviewDistance;

	ViewLocation = PreviewAngle.Vector() * -PreviewDistance;
	ViewRotation = PreviewAngle;

	Viewport->Invalidate();
}


/*-----------------------------------------------------------------------------
WxCascadePreview
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxCascadePreview, wxWindow )
	EVT_SIZE( WxCascadePreview::OnSize )
END_EVENT_TABLE()

WxCascadePreview::WxCascadePreview( wxWindow* InParent, wxWindowID InID, class WxCascade* InCascade  )
: wxWindow( InParent, InID )
{
	CascadePreviewVC = new FCascadePreviewViewportClient(InCascade);
	CascadePreviewVC->Viewport = GEngine->Client->CreateWindowChildViewport(CascadePreviewVC, (HWND)GetHandle());
	CascadePreviewVC->Viewport->CaptureJoystickInput(false);
}

WxCascadePreview::~WxCascadePreview()
{

}

void WxCascadePreview::OnSize( wxSizeEvent& In )
{
	wxRect rc = GetClientRect();

	::MoveWindow( (HWND)CascadePreviewVC->Viewport->GetWindow(), 0, 0, rc.GetWidth(), rc.GetHeight(), 1 );
}

//
//	CascadePreviewComponent
//
void UCascadePreviewComponent::Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	// Draw any module 'scene widgets'
	ACascadePreviewActor* PrevActor = CastChecked<ACascadePreviewActor>(Owner);

	UParticleSystem* PartSys = PrevActor->PartSysComp->Template;

	// Can now iterate over the modules on this system...
    for (INT i = 0; i < PartSys->Emitters.Num(); i++)
	{
		UParticleEmitter* Emitter = PartSys->Emitters(i);
		check(Emitter);

		// Emitters may have a set number of loops.
		// After which, the system will kill them off
		if (i < PrevActor->PartSysComp->EmitterInstances.Num())
		{
			FParticleEmitterInstancePointer EmitterInst = PrevActor->PartSysComp->EmitterInstances(i);
			check(EmitterInst);
			check(EmitterInst->Template == Emitter);

			for( INT j=0; j< Emitter->Modules.Num(); j++)
			{
				UParticleModule* Module = Emitter->Modules(j);

				if (Module->bSupported3DDrawMode && Module->b3DDrawMode)
				{
					Module->Render3DPreview( EmitterInst, Context, PRI);
				}
			}
		}
	}
}
