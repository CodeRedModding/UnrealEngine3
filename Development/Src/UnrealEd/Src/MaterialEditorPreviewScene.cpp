/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "MouseDeltaTracker.h"
#include "NewMaterialEditor.h"
#include "MaterialEditorPreviewScene.h"

namespace {
	static const FLOAT	MaterialEditor_LightRotSpeed = 40.0f;
}

FMaterialEditorPreviewVC::FMaterialEditorPreviewVC(WxMaterialEditorBase* InMaterialEditor, UStaticMesh* InStaticMesh, USkeletalMesh* InSkeletalMesh)
	:	MaterialEditor( InMaterialEditor )
{
	PreviewScene = new FPreviewScene();

	// Setup DrawHelper for drawing grid, axes etc.
	DrawHelper.bDrawPivot = FALSE;
	DrawHelper.bDrawWorldBox = FALSE;
	DrawHelper.bDrawKillZ = FALSE;
	DrawHelper.GridColorHi = FColor(80,80,80);
	DrawHelper.GridColorLo = FColor(72,72,72);
	DrawHelper.PerspectiveGridSize = 32767;

	// Create the preview mesh component.
	MaterialEditor->PreviewMeshComponent = ConstructObject<UMaterialEditorMeshComponent>(
		UMaterialEditorMeshComponent::StaticClass(), UObject::GetTransientPackage(), NAME_None, RF_Transient );

	MaterialEditor->PreviewMeshComponent->MaterialEditor = MaterialEditor;
	MaterialEditor->PreviewMeshComponent->StaticMesh = InStaticMesh;

	// Create the preview skeletal mesh component.
	MaterialEditor->PreviewSkeletalMeshComponent = ConstructObject<UMaterialEditorSkeletalMeshComponent>(
		UMaterialEditorSkeletalMeshComponent::StaticClass(), UObject::GetTransientPackage(), NAME_None, RF_Transient );

	MaterialEditor->PreviewSkeletalMeshComponent->MaterialEditor = MaterialEditor;
	MaterialEditor->PreviewSkeletalMeshComponent->SkeletalMesh = InSkeletalMesh;

	ShowFlags = (SHOW_DefaultEditor&~SHOW_ViewMode_Mask)|SHOW_ViewMode_Lit;

	// no effects that can distract from material tweaking
	ShowFlags &= ~SHOW_DepthOfField;
	ShowFlags &= ~SHOW_MotionBlur;
	ShowFlags &= ~SHOW_ImageGrain;

	NearPlane = 1.0f;
	bAllowMayaCam = TRUE;
}

FMaterialEditorPreviewVC::~FMaterialEditorPreviewVC()
{
	delete PreviewScene;
}

/**
 * Sets whether or not the grid and world box should be drawn.
 */
void FMaterialEditorPreviewVC::SetShowGrid(UBOOL bShowGrid)
{
	DrawHelper.bDrawGrid = bShowGrid;
}

void FMaterialEditorPreviewVC::Serialize(FArchive& Ar)
{ 
	Ar << Input; 
	Ar << *PreviewScene;
}

FSceneInterface* FMaterialEditorPreviewVC::GetScene()
{
	return PreviewScene->GetScene();
}

FLinearColor FMaterialEditorPreviewVC::GetBackgroundColor()
{
	const EBlendMode PreviewBlendMode = (EBlendMode)MaterialEditor->MaterialInterface->GetMaterial()->BlendMode;
	FLinearColor BackgroundColor = FLinearColor::Black;
	if ((PreviewBlendMode == BLEND_Modulate) || (PreviewBlendMode == BLEND_ModulateAndAdd))
	{
		BackgroundColor = FLinearColor::White;
	}
	else if (PreviewBlendMode == BLEND_Translucent || PreviewBlendMode == BLEND_AlphaComposite || PreviewBlendMode == BLEND_DitheredTranslucent)
	{
		BackgroundColor = FColor(64, 64, 64);
	}
	return BackgroundColor;
}

UBOOL FMaterialEditorPreviewVC::InputKey(FViewport* Viewport, INT ControllerId, FName Key, EInputEvent Event,FLOAT AmountDepressed,UBOOL Gamepad)
{
	// Hide and lock mouse cursor if we're capturing mouse input
	Viewport->ShowCursor( !Viewport->HasMouseCapture() );
	Viewport->LockMouseToWindow( Viewport->HasMouseCapture() );

	const INT HitX = Viewport->GetMouseX();
	const INT HitY = Viewport->GetMouseY();

	// Do stuff for FEditorLevelViewportClient camera movement.
	if( Event == IE_Pressed) 
	{
		if( Key == KEY_LeftMouseButton || Key == KEY_RightMouseButton || Viewport->KeyState(KEY_MiddleMouseButton) )
		{
			MouseDeltaTracker->StartTracking( this, HitX, HitY );
		}
	}
	else if( Event == IE_Released )
	{
		if( Key == KEY_LeftMouseButton || Key == KEY_RightMouseButton || Viewport->KeyState(KEY_MiddleMouseButton) )
		{
			MouseDeltaTracker->EndTracking( this );
		}
	}

	// Handle viewport screenshot.
	InputTakeScreenshot( Viewport, Key, Event );

	return TRUE;
}

/** Handles mouse being moved while input is captured - in this case, while a mouse button is down. */
UBOOL FMaterialEditorPreviewVC::InputAxis(FViewport* Viewport, INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad)
{
	// Get some useful info about buttons being held down
	const UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	const UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);
	const UBOOL bLightMoveDown = Viewport->KeyState(KEY_L);

	const UBOOL bMouseButtonDown = Viewport->KeyState( KEY_LeftMouseButton ) || Viewport->KeyState( KEY_MiddleMouseButton ) || Viewport->KeyState( KEY_RightMouseButton );


	// Look at which axis is being dragged and by how much
	const FLOAT DragX = (Key == KEY_MouseX) ? Delta : 0.f;
	const FLOAT DragY = (Key == KEY_MouseY) ? Delta : 0.f;

	if( bLightMoveDown )
	{
		FRotator LightDir = PreviewScene->GetLightDirection();

		LightDir.Yaw += -DragX * MaterialEditor_LightRotSpeed;
		LightDir.Pitch += -DragY * MaterialEditor_LightRotSpeed;

		PreviewScene->SetLightDirection( LightDir );

		Viewport->Invalidate();
	}
	// Use the MouseDeltaTracker to update camera.
	else if( bMouseButtonDown )
	{
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
				MoveViewportCamera( Drag, Rot );
			}

			MouseDeltaTracker->ReduceBy( DragDelta );
		}

		Viewport->Invalidate();
	}


	return TRUE;
}

/** Scene drawing callback. */
void FMaterialEditorPreviewVC::Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	DrawHelper.Draw(View, PDI);
}

void FMaterialEditorPreviewVC::Draw(FViewport* Viewport,FCanvas* Canvas)
{
	FEditorLevelViewportClient::Draw(Viewport, Canvas);
	MaterialEditor->DrawMessages(Viewport, Canvas);
}

void FMaterialEditorPreviewVC::MouseMove(FViewport* Viewport, INT X, INT Y)
{
	// Override to prevent FEditorLevelViewportClient propagating mouse movement to EditorModeTools as we don't want them updating at the same time.
}

void FMaterialEditorPreviewVC::CapturedMouseMove(FViewport* InViewport, INT InMouseX, INT InMouseY)
{
	// Override to prevent FEditorLevelViewportClient propagating mouse movement to EditorModeTools as we don't want them updating at the same time.
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// UMaterialEditorMeshComponent
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_CLASS( UMaterialEditorMeshComponent );

void UMaterialEditorMeshComponent::Attach()
{
	if (Materials.Num() > 0 && Materials(0) != NULL)
	{
		UMaterial* PreviewMaterial = Materials(0)->GetMaterial();
		if (PreviewMaterial && PreviewMaterial->bUsedWithFogVolumes)
		{
			Scene->AddFogVolume(this);
		}
	}
	Super::Attach();
}

void UMaterialEditorMeshComponent::Detach( UBOOL bWillReattach )
{
	if (Materials.Num() > 0 && Materials(0) != NULL)
	{
		UMaterial* PreviewMaterial = Materials(0)->GetMaterial();
		if (PreviewMaterial && PreviewMaterial->bUsedWithFogVolumes)
		{
			Scene->RemoveFogVolume(this);
		}
	}
	Super::Detach( bWillReattach );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// UMaterialEditorSkeletalMeshComponent
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_CLASS( UMaterialEditorSkeletalMeshComponent );
