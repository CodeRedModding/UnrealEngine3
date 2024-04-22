/*=============================================================================
	SpeedTreeEditor.cpp: SpeedTree editor implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "MouseDeltaTracker.h"
#include "PropertyWindow.h"
#include "PropertyUtils.h"
#include "..\..\Launch\Resources\resource.h"
#include "SpeedTree.h"
#include "UnLinkedObjDrawUtils.h"

#if WITH_SPEEDTREE

static const FLOAT	LightRotationSpeed = 40.0f;


////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FSpeedTreeEditorViewportClient::FSpeedTreeEditorViewportClient

FSpeedTreeEditorViewportClient::FSpeedTreeEditorViewportClient(class WxSpeedTreeEditor* Editor) :
	SpeedTreeEditor(Editor)
{
	FBoxSphereBounds SpeedTreeBounds(FVector(0,0,0), FVector(1,1,1), 1);
	if (SpeedTreeEditor->SpeedTree->SRH)
	{
		SpeedTreeBounds = SpeedTreeEditor->SpeedTree->SRH->Bounds;
		ViewLocation = -FVector(0,SpeedTreeBounds.SphereRadius / (75.0f * (FLOAT)PI / 360.0f),0);
		//ViewLocation = SpeedTreeBounds.Origin;
		ViewRotation = FRotator(0,16384,0);
	}

	ShowFlags = (SHOW_DefaultEditor&~SHOW_ViewMode_Mask)|SHOW_ViewMode_Lit;
	bAllowMayaCam = TRUE;
	bDrawAxes = 0;

	SpeedTreeComponent = ConstructObject<USpeedTreeComponent>(USpeedTreeComponent::StaticClass( ), UObject::GetTransientPackage(), NAME_None, RF_Transient);
	SpeedTreeComponent->SpeedTree = Editor->SpeedTree;
	FPreviewScene::AddComponent(SpeedTreeComponent, FTranslationMatrix(-SpeedTreeBounds.Origin));
}

UBOOL FSpeedTreeEditorViewportClient::InputKey(FViewport* Viewport, INT ControllerId, FName Key, EInputEvent Event,FLOAT AmountDepressed,UBOOL Gamepad)
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

namespace 
{
	static const FLOAT SpeedTreeEditor_LightRotSpeed = 40.0f;
}

/** Handles mouse being moved while input is captured - in this case, while a mouse button is down. */
UBOOL FSpeedTreeEditorViewportClient::InputAxis(FViewport* Viewport, INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad)
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
		FRotator LightDir = GetLightDirection();

		LightDir.Yaw += -DragX * SpeedTreeEditor_LightRotSpeed;
		LightDir.Pitch += -DragY * SpeedTreeEditor_LightRotSpeed;

		SetLightDirection( LightDir );

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

	SpeedTreeComponent->ConditionalTick(DeltaTime);
	return TRUE;
}

void FSpeedTreeEditorViewportClient::Draw(FViewport* Viewport,FCanvas* Canvas)
{
	FEditorLevelViewportClient::Draw(Viewport, Canvas);
	if (SpeedTreeEditor->SpeedTree->IsLegacySpeedTree())
	{
		Canvas->PushAbsoluteTransform(FMatrix::Identity);
		UFont* FontToUse = GEngine->MediumFont;

		FLinkedObjDrawUtils::DrawShadowedString(
			Canvas,
			5,
			50,
			*FString::Printf(TEXT("This is a legacy speedtree that could be not loaded due to Speedtree 5.0 not being backwards compatible with other versions.")),
			FontToUse,
			FLinearColor(1,0,0)
			);

		FLinkedObjDrawUtils::DrawShadowedString(
			Canvas,
			5,
			70,
			*FString::Printf(TEXT("Import a new Speedtree 5.0 asset with the same name as this legacy USpeedTree to replace it.")),
			FontToUse,
			FLinearColor(1,0,0)
			);
	}
	
	Canvas->PopTransform();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Message Map

IMPLEMENT_DYNAMIC_CLASS(WxSpeedTreeEditor,wxFrame)
BEGIN_EVENT_TABLE(WxSpeedTreeEditor, wxFrame)
	EVT_SIZE(WxSpeedTreeEditor::OnSize)
	EVT_PAINT(WxSpeedTreeEditor::OnPaint)
END_EVENT_TABLE( )


////////////////////////////////////////////////////////////////////////////////////////////////////////////
// WxSpeedTreeEditor::WxSpeedTreeEditor

WxSpeedTreeEditor::WxSpeedTreeEditor(wxWindow* Parent, wxWindowID wxID, USpeedTree* SpeedTree) : 
	wxFrame(Parent, wxID, *LocalizeUnrealEd("SpeedTreeEditor"), wxDefaultPosition, wxDefaultSize, wxFRAME_FLOAT_ON_PARENT | wxDEFAULT_FRAME_STYLE | wxFRAME_NO_TASKBAR),
	SpeedTree(SpeedTree)
{
	SplitterWnd = new wxSplitterWindow(this, ID_SPLITTERWINDOW, wxDefaultPosition, wxSize(100, 100), wxSP_3D | wxSP_3DBORDER | wxSP_FULLSASH);

	// Create property window
	PropertyWindow = new WxPropertyWindowHost;
	PropertyWindow->Create(SplitterWnd, NULL);
	PropertyWindow->SetObject(SpeedTree, EPropertyWindowFlags::ShouldShowCategories);

	// Create viewport
	ViewportHolder = new WxViewportHolder(SplitterWnd, -1, 0);
	ViewportClient = new FSpeedTreeEditorViewportClient(this);
	ViewportClient->Viewport = GEngine->Client->CreateWindowChildViewport(ViewportClient, (HWND)ViewportHolder->GetHandle( ));
	ViewportClient->Viewport->CaptureJoystickInput(false);
	ViewportHolder->SetViewport(ViewportClient->Viewport);
	ViewportHolder->Show( );

	FWindowUtil::LoadPosSize(TEXT("SpeedTreeEditor"), this, 64, 64, 800, 450);

	// Load the preview scene
	ViewportClient->LoadSettings(TEXT("SpeedTreeEditor"));

	wxRect rc = GetClientRect( );

	SplitterWnd->SplitVertically(ViewportHolder, PropertyWindow, rc.GetWidth( ) - 350);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////
// WxSpeedTreeEditor::~WxSpeedTreeEditor

WxSpeedTreeEditor::~WxSpeedTreeEditor( )
{
	// Save the preview scene
	check(ViewportClient);
	ViewportClient->SaveSettings(TEXT("SpeedTreeEditor"));

	FWindowUtil::SavePosSize(TEXT("SpeedTreeEditor"), this);

	GEngine->Client->CloseViewport(ViewportClient->Viewport);
	ViewportClient->Viewport = NULL;
	delete ViewportClient;
	delete PropertyWindow;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////
// WxSpeedTreeEditor::OnSize

void WxSpeedTreeEditor::OnSize(wxSizeEvent& wxIn)
{
	wxPoint wxOrigin = GetClientAreaOrigin( );
	wxRect wxRect = GetClientRect( );
	wxRect.y -= wxOrigin.y;
	SplitterWnd->SetSize(wxRect);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////
// WxSpeedTreeEditor::Serialize

void WxSpeedTreeEditor::Serialize(FArchive& Ar)
{
	Ar << SpeedTree;
	check(ViewportClient);
	ViewportClient->Serialize(Ar);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////
// WxSpeedTreeEditor::OnPaint

void WxSpeedTreeEditor::OnPaint(wxPaintEvent& wxIn)
{
	wxPaintDC dc(this);
	ViewportClient->Viewport->Invalidate( );
}

#endif

