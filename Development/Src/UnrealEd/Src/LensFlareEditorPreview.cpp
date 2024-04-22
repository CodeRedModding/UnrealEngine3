/*=============================================================================
	LensFlareEditorPreview.cpp: 'LensFlareEditor' particle editor preview pane
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "LensFlare.h"
#include "LensFlareEditor.h"
#include "MouseDeltaTracker.h"
#include "EngineMaterialClasses.h"
#include "ImageUtils.h"

static FColor LFED_GridColorHi = FColor(0,0,80);
static FColor LFED_GridColorLo = FColor(0,0,72);

/*-----------------------------------------------------------------------------
FLensFlarePreviewViewportClient
-----------------------------------------------------------------------------*/

FLensFlarePreviewViewportClient::FLensFlarePreviewViewportClient(WxLensFlareEditor* InLensFlareEditor):
	PreviewScene( FRotator(-45.f*(65535.f/360.f), 180.f*(65535.f/360.f), 0.f), 0.25f, 1.f)
{
	LensFlareEditor = InLensFlareEditor;
	if (LensFlareEditor && LensFlareEditor->EditorOptions)
	{
		LFED_GridColorHi = LensFlareEditor->EditorOptions->GridColor_Hi;
		LFED_GridColorLo = LensFlareEditor->EditorOptions->GridColor_Low;
	}

	//@todo. Put these in the options class for LensFlareEditor...
	DrawHelper.bDrawGrid = LensFlareEditor->EditorOptions->bShowGrid;
	DrawHelper.GridColorHi = LFED_GridColorHi;
	DrawHelper.GridColorLo = LFED_GridColorLo;
	DrawHelper.bDrawKillZ = FALSE;
	DrawHelper.bDrawWorldBox = FALSE;
	DrawHelper.bDrawPivot = FALSE;
	if (LensFlareEditor && LensFlareEditor->EditorOptions)
	{
		DrawHelper.PerspectiveGridSize = LensFlareEditor->EditorOptions->GridPerspectiveSize;
	}
	else
	{
		DrawHelper.PerspectiveGridSize = 32767;
	}
	DrawHelper.DepthPriorityGroup = SDPG_World;
	//@todo. END - Put these in the options class for LensFlareEditor...

	// Create the post process chain
	SetupPostProcessChain();

	// Create ParticleSystemComponent to use for preview.
	LensFlareEditor->LensFlareComp = ConstructObject<ULensFlareComponent>(ULensFlareComponent::StaticClass());
	PreviewScene.AddComponent(LensFlareEditor->LensFlareComp,FMatrix::Identity);
	LensFlareEditor->LensFlareComp->SetFlags(RF_Transactional);

	// Create LensFlareEditorPreviewComponent for drawing extra useful info
	LensFlareEditor->LensFlarePrevComp = ConstructObject<ULensFlarePreviewComponent>(ULensFlarePreviewComponent::StaticClass());
	LensFlareEditor->LensFlarePrevComp->LensFlareEditorPtr = InLensFlareEditor;
	PreviewScene.AddComponent(LensFlareEditor->LensFlarePrevComp,FMatrix::Identity);

	TimeScale = 1.f;

	BackgroundColor = FColor(0, 0, 0);

	// Update light components from parameters now
	UpdateLighting();

	// Default view position
	// TODO: Store in ParticleSystem
	ViewLocation = FVector(-200.f, 0.f, 0.f);
	ViewRotation = FRotator(0, 0, 0);	

	PreviewAngle = FRotator(0, 0, 0);
	PreviewDistance = 0.f;

	TotalTime = 0.f;

	// Use game defaults to hide emitter sprite etc.
	ShowFlags = SHOW_DefaultGame | SHOW_Grid;

	//@todo. Temp hack - unlit mode forced
	ShowFlags &= ~SHOW_Lighting;

	bDrawOriginAxes = FALSE;

	SetRealtime( 1 );

	bWireframe	= FALSE;
	bBounds = FALSE;
	bAllowMayaCam = TRUE;

	bCaptureScreenShot = FALSE;
}

FLinearColor FLensFlarePreviewViewportClient::GetBackgroundColor()
{
	return BackgroundColor;
}

void FLensFlarePreviewViewportClient::UpdateLighting()
{
	// TODO
}

void FLensFlarePreviewViewportClient::SetupPostProcessChain()
{
    LensFlareEditor->DefaultPostProcessName = LensFlareEditor->EditorOptions->PostProcessChainName;
	ShowPPFlags = LensFlareEditor->EditorOptions->ShowPPFlags;
	if (LensFlareEditor->DefaultPostProcessName != FString(TEXT("")))
	{
		LensFlareEditor->DefaultPostProcess = LoadObject<UPostProcessChain>(
			NULL,*(LensFlareEditor->DefaultPostProcessName),NULL,LOAD_None,NULL);

		if (LensFlareEditor->DefaultPostProcess)
		{
			LensFlareEditor->UpdatePostProcessChain();
		}
		else
		{
			warnf(TEXT("LENSFLAREEDITOR: Failed to load default post process chain."));
		}
	}
}

void FLensFlarePreviewViewportClient::Draw(FViewport* Viewport, FCanvas* Canvas)
{
	// We make sure the background is black because some of it will show through (black bars).
	Clear(Canvas,BackgroundColor);

	// clear any lines rendered the previous frame   
	PreviewScene.ClearLineBatcher();
	ULineBatchComponent* LineBatcher = PreviewScene.GetLineBatcher();

	const FVector XAxis(1,0,0); 
	const FVector YAxis(0,1,0); 
	const FVector ZAxis(0,0,1);
	if (bDrawOriginAxes)
	{
		FMatrix ArrowMatrix = FMatrix(XAxis, YAxis, ZAxis, FVector(0.f));
		DrawDirectionalArrow(LineBatcher, ArrowMatrix, FColor(255,0,0), 10.f, 1.0f, SDPG_World);

		ArrowMatrix = FMatrix(YAxis, ZAxis, XAxis, FVector(0.f));
		DrawDirectionalArrow(LineBatcher, ArrowMatrix, FColor(0,255,0), 10.f, 1.0f, SDPG_World);

		ArrowMatrix = FMatrix(ZAxis, XAxis, YAxis, FVector(0.f));
		DrawDirectionalArrow(LineBatcher, ArrowMatrix, FColor(0,0,255), 10.f, 1.0f, SDPG_World);
	}

	PreviewScene.AddComponent(LineBatcher,FMatrix::Identity);

	EShowFlags SavedShowFlags = ShowFlags;
	if (bWireframe)
	{
		ShowFlags &= ~SHOW_ViewMode_Mask;
		ShowFlags |= SHOW_ViewMode_Wireframe;
	}
	if (bBounds)
	{
		ShowFlags |= SHOW_Bounds;
	}
	else
	{
		ShowFlags &= ~SHOW_Bounds;
	}
	if (ShowPPFlags != 0)
	{
		ShowFlags |= SHOW_PostProcess;
	}
	else
	{
		ShowFlags &= ~SHOW_PostProcess;
	}

	FEditorLevelViewportClient::Draw(Viewport, Canvas);
	ShowFlags = SavedShowFlags;

	if (LensFlareEditor->ToolBar)
	{
		if (IsRealtime() && !LensFlareEditor->ToolBar->bRealtime)
		{
			LensFlareEditor->ToolBar->ToggleTool(IDM_LENSFLAREEDITOR_REALTIME, TRUE);
			LensFlareEditor->ToolBar->bRealtime	= TRUE;
		}
		else
		if (!IsRealtime() && LensFlareEditor->ToolBar->bRealtime)
		{
			LensFlareEditor->ToolBar->ToggleTool(IDM_LENSFLAREEDITOR_REALTIME, FALSE);
			LensFlareEditor->ToolBar->bRealtime	= FALSE;
		}
	}

	if (Viewport && bCaptureScreenShot)
	{
		INT SrcWidth = Viewport->GetSizeX();
		INT SrcHeight = Viewport->GetSizeY();
		// Read the contents of the viewport into an array.
		TArray<FColor> OrigBitmap;
		if (Viewport->ReadPixels(OrigBitmap))
		{
			check(OrigBitmap.Num() == SrcWidth * SrcHeight);

			// Resize image to enforce max size.
			TArray<FColor> ScaledBitmap;
			INT ScaledWidth	 = 512;
			INT ScaledHeight = 512;
			FImageUtils::ImageResize( SrcWidth, SrcHeight, OrigBitmap, ScaledWidth, ScaledHeight, ScaledBitmap, TRUE );

			// Compress.
			EObjectFlags ObjectFlags = RF_NotForClient|RF_NotForServer;
			FCreateTexture2DParameters Params;
			LensFlareEditor->LensFlare->ThumbnailImage = FImageUtils::CreateTexture2D( ScaledWidth, ScaledHeight, ScaledBitmap, LensFlareEditor->LensFlare, TEXT("ThumbnailTexture"), ObjectFlags, Params );

			LensFlareEditor->LensFlare->ThumbnailImageOutOfDate = FALSE;
			LensFlareEditor->LensFlare->MarkPackageDirty();
		}
		bCaptureScreenShot = FALSE;
	}
}

void FLensFlarePreviewViewportClient::Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	DrawHelper.Draw(View, PDI);
	FEditorLevelViewportClient::Draw(View, PDI);
}

/**
 * Configures the specified FSceneView object with the view and projection matrices for this viewport.
 * @param	View		The view to be configured.  Must be valid.
 * @return	A pointer to the view within the view family which represents the viewport's primary view.
 */
FSceneView* FLensFlarePreviewViewportClient::CalcSceneView(FSceneViewFamily* ViewFamily)
{
	FSceneView* View = FEditorLevelViewportClient::CalcSceneView(ViewFamily);
	if (View)
	{
		View->PostProcessChain = LensFlareEditor->DefaultPostProcess;
	}

	return View;
}

UBOOL FLensFlarePreviewViewportClient::InputKey(FViewport* Viewport, INT ControllerId, FName Key, EInputEvent Event,FLOAT /*AmountDepressed*/,UBOOL /*Gamepad*/)
{
	// Hide and lock mouse cursor if we're capturing mouse input
	Viewport->ShowCursor( !Viewport->HasMouseCapture() );
	Viewport->LockMouseToWindow( Viewport->HasMouseCapture() );

	if (Event == IE_Pressed)
	{
	}

	// Handle viewport screenshot.
	InputTakeScreenshot( Viewport, Key, Event );

	return TRUE;
}

void FLensFlarePreviewViewportClient::MouseMove(FViewport* Viewport, INT X, INT Y)
{

}

UBOOL FLensFlarePreviewViewportClient::InputAxis(FViewport* Viewport, INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad)
{
	if(Input->InputAxis(ControllerId,Key,Delta,DeltaTime))
	{
		return TRUE;
	}

	if((Key == KEY_MouseX || Key == KEY_MouseY) && Delta != 0.0f)
	{
		const UBOOL LeftMouseButton = Viewport->KeyState(KEY_LeftMouseButton);
		const UBOOL MiddleMouseButton = Viewport->KeyState(KEY_MiddleMouseButton);
		const UBOOL RightMouseButton = Viewport->KeyState(KEY_RightMouseButton);

		MouseDeltaTracker->AddDelta( this, Key, Delta, 0 );
		const FVector DragDelta = MouseDeltaTracker->GetDelta();

		GEditor->MouseMovement += DragDelta;

		if( !DragDelta.IsZero() )
		{
			// Convert the movement delta into drag/rotation deltas
			if ( bAllowMayaCam && GEditor->bUseMayaCameraControls )
			{
				FVector TempDrag;
				FRotator TempRot;
				InputAxisMayaCam( Viewport, DragDelta, TempDrag, TempRot );
			}
			else
			{
				FVector Drag;
				FRotator Rot;
				FVector Scale;
				MouseDeltaTracker->ConvertMovementDeltaToDragRot( this, DragDelta, Drag, Rot, Scale );

				if(LensFlareEditor->bOrbitMode)
				{
					const FLOAT DX = (Key == KEY_MouseX) ? Delta : 0.0f;
					const FLOAT DY = (Key == KEY_MouseY) ? Delta : 0.0f;

					const FLOAT YawSpeed = 20.0f;
					const FLOAT PitchSpeed = 20.0f;
					const FLOAT ZoomSpeed = 1.0f;

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
			}

			MouseDeltaTracker->ReduceBy( DragDelta );
		}
	}

	Viewport->Invalidate();

	return TRUE;
}

void FLensFlarePreviewViewportClient::SetPreviewCamera(const FRotator& NewPreviewAngle, FLOAT NewPreviewDistance)
{
	PreviewAngle = NewPreviewAngle;
	PreviewDistance = NewPreviewDistance;

	ViewLocation = PreviewAngle.Vector() * -PreviewDistance;
	ViewRotation = PreviewAngle;

	Viewport->Invalidate();
}


/*-----------------------------------------------------------------------------
WxLensFlareEditorPreview
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxLensFlareEditorPreview, wxWindow )
	EVT_SIZE( WxLensFlareEditorPreview::OnSize )
END_EVENT_TABLE()

WxLensFlareEditorPreview::WxLensFlareEditorPreview( wxWindow* InParent, wxWindowID InID, class WxLensFlareEditor* InLensFlareEditor  ) :
	wxWindow( InParent, InID )
{
	LensFlareEditorPreviewVC = new FLensFlarePreviewViewportClient(InLensFlareEditor);
	LensFlareEditorPreviewVC->Viewport = GEngine->Client->CreateWindowChildViewport(LensFlareEditorPreviewVC, (HWND)GetHandle());
	LensFlareEditorPreviewVC->Viewport->CaptureJoystickInput(FALSE);
}

WxLensFlareEditorPreview::~WxLensFlareEditorPreview()
{

}

void WxLensFlareEditorPreview::OnSize( wxSizeEvent& In )
{
	wxRect rc = GetClientRect();
	::MoveWindow( (HWND)LensFlareEditorPreviewVC->Viewport->GetWindow(), 0, 0, rc.GetWidth(), rc.GetHeight(), 1 );
}

//
//	ULensFlarePreviewComponent
//
void ULensFlarePreviewComponent::Render(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
}
