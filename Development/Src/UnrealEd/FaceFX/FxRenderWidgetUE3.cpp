//------------------------------------------------------------------------------
// The render widget for Unreal Engine 3.
//
// Owner: Jamie Redmond
//
// Copyright (c) 2002-2005 OC3 Entertainment, Inc.
//------------------------------------------------------------------------------

#include "UnrealEd.h"
#include "stdwx.h"

#include "FxRenderWidgetUE3.h"

#ifdef __UNREAL__

#include "MouseDeltaTracker.h"
#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "FxStudioApp.h"
#include "FxSessionProxy.h"

namespace OC3Ent
{

namespace Face
{

//------------------------------------------------------------------------------
// FFaceFXStudioViewportClient.
//------------------------------------------------------------------------------

static const FLOAT	TranslateSpeed = 0.25f;
static const FLOAT	RotateSpeed = 0.02f;
static const FLOAT	LightRotSpeed = 40.0f;

FFaceFXStudioViewportClient::FFaceFXStudioViewportClient( FxRenderWidgetUE3* InRWUE3 )
{
	RenderWidgetUE3 = InRWUE3;

	// Create EditorComponent for drawing grid, axes etc.
	DrawHelper.bDrawPivot = FALSE;
	DrawHelper.bDrawWorldBox = FALSE;
	DrawHelper.bDrawKillZ = FALSE;
	DrawHelper.GridColorHi = FColor(80,80,80);
	DrawHelper.GridColorLo = FColor(72,72,72);
	DrawHelper.PerspectiveGridSize = 32767;

	RenderWidgetUE3->PreviewAnimNodeSeq = ConstructObject<UAnimNodeSequence>(UAnimNodeSequence::StaticClass());

	InRWUE3->PreviewSkelComp = ConstructObject<UFaceFXStudioSkelComponent>(UFaceFXStudioSkelComponent::StaticClass());
	InRWUE3->PreviewSkelComp->Animations = RenderWidgetUE3->PreviewAnimNodeSeq;
	InRWUE3->PreviewSkelComp->RenderWidgetUE3Ptr = InRWUE3;

	ShowFlags = SHOW_DefaultEditor;

	// Set the viewport to be fully lit.
	ShowFlags &= ~SHOW_ViewMode_Mask;
	ShowFlags |= SHOW_ViewMode_Lit;

	bDrawingInfoWidget = FALSE;
	
	NearPlane = 1.0f;

	SetRealtime(TRUE);
	bAllowMayaCam = TRUE;
	SetViewLocationForOrbiting(FVector(0.f,0.f,0.f));
}

FFaceFXStudioViewportClient::~FFaceFXStudioViewportClient()
{
}

void FFaceFXStudioViewportClient::Serialize(FArchive& Ar)
{ 
	Ar << Input;
	Ar << PreviewScene;
}

FLinearColor FFaceFXStudioViewportClient::GetBackgroundColor()
{
	return FColor(64,64,64);
}

void FFaceFXStudioViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	DrawHelper.Draw(View, PDI);
}

void FFaceFXStudioViewportClient::Draw( FViewport* Viewport, FCanvas* Canvas )
{
	// Do main viewport drawing stuff.
	FEditorLevelViewportClient::Draw(Viewport,Canvas);
}

void FFaceFXStudioViewportClient::Tick(FLOAT DeltaSeconds)
{
	FEditorLevelViewportClient::Tick(DeltaSeconds);
	RenderWidgetUE3->Tick(DeltaSeconds);
}

UBOOL FFaceFXStudioViewportClient::InputKey(FViewport* Viewport, INT ControllerId, FName Key, EInputEvent Event,FLOAT AmountDepressed,UBOOL bGamepad)
{
	const INT HitX = Viewport->GetMouseX();
	const INT HitY = Viewport->GetMouseY();

	// Do stuff for FEditorLevelViewportClient camera movement.
	if( Event == IE_Pressed) 
	{
		if( Key == KEY_LeftMouseButton  || 
			Key == KEY_RightMouseButton || 
			Viewport->KeyState(KEY_MiddleMouseButton) )
		{
			MouseDeltaTracker->StartTracking(this, HitX, HitY);
		}
	}
	else if( Event == IE_Released )
	{
		if( Key == KEY_LeftMouseButton  || 
			Key == KEY_RightMouseButton || 
			Viewport->KeyState(KEY_MiddleMouseButton) )
		{
			MouseDeltaTracker->EndTracking(this);
		}
	}

	// Handle viewport screenshot.
	InputTakeScreenshot( Viewport, Key, Event );

	return TRUE;
}

/** Handles mouse being moved while input is captured - in this case, while a mouse button is down. */
UBOOL FFaceFXStudioViewportClient::InputAxis(FViewport* Viewport, INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad)
{
	// Get some useful info about buttons being held down.
	const UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	const UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);
	const UBOOL bLightMoveDown = Viewport->KeyState(KEY_L);

	// Look at which axis is being dragged and by how much.
	const FLOAT DragX = (Key == KEY_MouseX) ? Delta : 0.f;
	const FLOAT DragY = (Key == KEY_MouseY) ? Delta : 0.f;

	if( bLightMoveDown )
	{
		FRotator LightDir = PreviewScene.GetLightDirection();
		LightDir.Yaw += -DragX * LightRotSpeed;
		LightDir.Pitch += -DragY * LightRotSpeed;
		PreviewScene.SetLightDirection(LightDir);
	}
	// If we are not manipulating the light, use the MouseDeltaTracker to update the camera.
	else
	{
		MouseDeltaTracker->AddDelta(this, Key, Delta, 0);
		const FVector DragDelta = MouseDeltaTracker->GetDelta();
		GEditor->MouseMovement += DragDelta;

		if( !DragDelta.IsZero() )
		{
			// Convert the movement delta into drag/rotation deltas.	
			if( bAllowMayaCam && GEditor->bUseMayaCameraControls )
			{
				FVector TempDrag;
				FRotator TempRot;
				InputAxisMayaCam(Viewport, DragDelta, TempDrag, TempRot);
			}
			else
			{
				FVector Drag;
				FRotator Rot;
				FVector Scale;
				MouseDeltaTracker->ConvertMovementDeltaToDragRot(this, DragDelta, Drag, Rot, Scale);
				MoveViewportCamera(Drag, Rot);
			}

			MouseDeltaTracker->ReduceBy(DragDelta);
		}
	}

	Viewport->Invalidate();
	return TRUE;
}

//------------------------------------------------------------------------------
// WxFaceFXStudioPreview.
//------------------------------------------------------------------------------

BEGIN_EVENT_TABLE(WxFaceFXStudioPreview, wxWindow)
	EVT_SIZE(WxFaceFXStudioPreview::OnSize)
END_EVENT_TABLE()

WxFaceFXStudioPreview::WxFaceFXStudioPreview( wxWindow* InParent, wxWindowID InID, FxRenderWidgetUE3* InRWUE3 )
	: wxWindow(InParent, InID)
	, PreviewVC(NULL)
{
	CreateViewport(InRWUE3);
}

WxFaceFXStudioPreview::~WxFaceFXStudioPreview()
{
	DestroyViewport();
}

void WxFaceFXStudioPreview::CreateViewport( FxRenderWidgetUE3* InRWUE3 )
{
	check(InRWUE3);

	DestroyViewport();

	PreviewVC = new FFaceFXStudioViewportClient(InRWUE3);
	PreviewVC->Viewport = GEngine->Client->CreateWindowChildViewport(PreviewVC, (HWND)GetHandle());
	check(PreviewVC->Viewport);
	PreviewVC->Viewport->CaptureJoystickInput(FALSE);
}

void WxFaceFXStudioPreview::DestroyViewport( void )
{
	if( PreviewVC )
	{
		GEngine->Client->CloseViewport(PreviewVC->Viewport);
		PreviewVC->Viewport = NULL;

		delete PreviewVC;
		PreviewVC = NULL;
	}
}

void WxFaceFXStudioPreview::OnSize( wxSizeEvent& In )
{
	if( PreviewVC )
	{
		checkSlow(PreviewVC->Viewport);
		wxRect rc = GetClientRect();
		::MoveWindow((HWND)PreviewVC->Viewport->GetWindow(), 0, 0, rc.GetWidth(), rc.GetHeight(), 1);

		PreviewVC->Viewport->Invalidate();
	}
}

//------------------------------------------------------------------------------
// FxRenderWidgetUE3.
//------------------------------------------------------------------------------

WX_IMPLEMENT_DYNAMIC_CLASS(FxRenderWidgetUE3, wxWindow)

BEGIN_EVENT_TABLE(FxRenderWidgetUE3, wxWindow)
	EVT_SIZE(FxRenderWidgetUE3::OnSize)
	EVT_HELP_RANGE( wxID_ANY, wxID_HIGHEST, FxRenderWidgetUE3::OnHelp )
END_EVENT_TABLE()


FxRenderWidgetUE3::FxRenderWidgetUE3( wxWindow* parent, FxWidgetMediator* mediator )
	: Super(parent, mediator)
	, PreviewWindow(NULL)
	, PreviewSkelMesh(NULL)
	, PreviewSkelComp(NULL)
	, PreviewAnimNodeSeq(NULL)
{
	bReadyToDraw = 0;

	PreviewWindow = new WxFaceFXStudioPreview(this, -1, this);
	FFaceFXStudioViewportClient* PreviewVC = GetPreviewVC();
	check(PreviewVC);

	PreviewVC->ViewLocation = FVector(0,-256,0);
	PreviewVC->ViewRotation = FRotator(0,16384,0);	
}

FxRenderWidgetUE3::~FxRenderWidgetUE3()
{
	FlushRenderingCommands();
	if( PreviewWindow )
	{
		PreviewWindow->DestroyViewport();
	}
}

FFaceFXStudioViewportClient* FxRenderWidgetUE3::GetPreviewVC( void )
{
	checkSlow(PreviewWindow);
	return PreviewWindow->PreviewVC;
}

void FxRenderWidgetUE3::OnAppStartup( FxWidget* FxUnused(sender) )
{
}

void FxRenderWidgetUE3::OnAppShutdown( FxWidget* FxUnused(sender) )
{
}

void FxRenderWidgetUE3::OnCreateActor( FxWidget* FxUnused(sender) )
{
}

void FxRenderWidgetUE3::OnLoadActor( FxWidget* FxUnused(sender), const FxString& FxUnused(actorPath) )
{
}

void FxRenderWidgetUE3::OnCloseActor( FxWidget* FxUnused(sender) )
{
}

void FxRenderWidgetUE3::OnSaveActor( FxWidget* FxUnused(sender), const FxString& FxUnused(actorPath) )
{
}

void FxRenderWidgetUE3::OnTimeChanged( FxWidget* FxUnused(sender), FxReal FxUnused(newTime) )
{
	// When this happens, we need to force the viewport to redraw immediately.
	if( bReadyToDraw )
	{
		//@todo This is an arbitrary hardcoded DeltaSeconds value.  Some way to actually
		//      pass in a valid DeltaSeconds value so that eventually when actual Unreal
		//      animation playback happens in here the animations don't look terribly out
		//      of synch.
		Tick(0.016667f);
		
		FFaceFXStudioViewportClient* PreviewVC = GetPreviewVC();
		check(PreviewVC);
		PreviewVC->Viewport->Invalidate();
		PreviewVC->Viewport->Draw();
	}
}

void FxRenderWidgetUE3::OnRefresh( FxWidget* FxUnused(sender) )
{
	// When this happens, we need to force the viewport to redraw immediately.
	if( bReadyToDraw )
	{
		//@todo This is an arbitrary hardcoded DeltaSeconds value.  Some way to actually
		//      pass in a valid DeltaSeconds value so that eventually when actual Unreal
		//      animation playback happens in here the animations don't look terribly out
		//      of synch.
		Tick(0.016667f);
	
		FFaceFXStudioViewportClient* PreviewVC = GetPreviewVC();
		check(PreviewVC);
		PreviewVC->Viewport->Invalidate();
		PreviewVC->Viewport->Draw();
	}
}

void FxRenderWidgetUE3::OnQueryRenderWidgetCaps( FxWidget* FxUnused(sender), FxRenderWidgetCaps& renderWidgetCaps )
{
	renderWidgetCaps.renderWidgetName = FxString("Unreal Engine 3 Render Widget");
	renderWidgetCaps.renderWidgetVersion = 1710;
	renderWidgetCaps.supportsOffscreenRenderTargets = FxFalse;
	renderWidgetCaps.supportsMultipleCameras = FxFalse;
	renderWidgetCaps.supportsFixedAspectRatioCameras = FxFalse;
	renderWidgetCaps.supportsBoneLockedCameras = FxFalse;
}

void FxRenderWidgetUE3::Serialize( FArchive& Ar )
{
	// Serialize the preview scene so that it isn't garbage collected.
	FFaceFXStudioViewportClient* PreviewVC = GetPreviewVC();
	check(PreviewVC);
	PreviewVC->Serialize(Ar);
}

void FxRenderWidgetUE3::NotifyDestroy( void* Src )
{
}

void FxRenderWidgetUE3::NotifyPreChange( void* Src, UProperty* PropertyAboutToChange )
{
}

void FxRenderWidgetUE3::NotifyPostChange( void* Src, UProperty* PropertyThatChanged )
{
	if( PropertyThatChanged )
	{
		// If it was the PreviewMorphSets array, update the preview SkelMeshComponent.
		if( PropertyThatChanged->GetFName() == FName(TEXT("PreviewMorphSets")) )
		{
			PreviewSkelComp->MorphSets = PreviewSkelMesh->FaceFXAsset->PreviewMorphSets;
		}
	}

	FFaceFXStudioViewportClient* PreviewVC = GetPreviewVC();
	check(PreviewVC);
	check(PreviewVC->Viewport);

	FlushRenderingCommands();
	// Set the actor instance as open in Studio so that the code in 
	// USkeletalMeshComponent::UpdateFaceFX() will execute and set up the 
	// material instance constants when calling UpdateSkeletalComponent().
	PreviewSkelComp->FaceFXActorInstance->SetIsOpenInStudio(FxTrue);
	UpdateSkeletalComponent();
	// The component was potentially just re-attached, so wait for the 
	// commands to process before continuing.
	FlushRenderingCommands();

	PreviewVC->Viewport->Invalidate();

	// Tell the FaceFX Studio session about the actor instance that was just 
	// created on the PreviewSkelComp.
	void* pVoidSession = NULL;
	FxSessionProxy::GetSession(&pVoidSession);
	if( pVoidSession )
	{
		FxStudioSession* pSession = reinterpret_cast<FxStudioSession*>(pVoidSession);
		check(PreviewSkelComp->FaceFXActorInstance);
		pSession->SetActorInstance(PreviewSkelComp->FaceFXActorInstance);
	}
}

void FxRenderWidgetUE3::NotifyExec( void* Src, const TCHAR* Cmd )
{
	GUnrealEd->NotifyExec(Src, Cmd);
}

void FxRenderWidgetUE3::SetSkeletalMesh( USkeletalMesh* pSkeletalMesh )
{
	FlushRenderingCommands();
	PreviewSkelMesh = pSkeletalMesh;
	check(PreviewWindow);
	if( !PreviewWindow->PreviewVC )
	{
		PreviewWindow->CreateViewport(this);
	}
    
	// Send a dummy size event to the preview window so that it gets 
	// reinitialized with the correct size and location.
	wxSizeEvent dummySizeEvent;
	PreviewWindow->OnSize(dummySizeEvent);

	FFaceFXStudioViewportClient* PreviewVC = GetPreviewVC();
	check(PreviewVC);

	if( PreviewSkelComp )
	{
		PreviewSkelComp->SetSkeletalMesh(pSkeletalMesh);
		PreviewSkelComp->MorphSets = pSkeletalMesh->FaceFXAsset->PreviewMorphSets;
		PreviewSkelComp->ForcedLodModel = 0;

		PreviewVC->PreviewScene.RemoveComponent(PreviewSkelComp);
		PreviewVC->PreviewScene.AddComponent(PreviewSkelComp, FMatrix::Identity);
		
		FlushRenderingCommands();
		// Set the actor instance as open in Studio so that the code in 
		// USkeletalMeshComponent::UpdateFaceFX() will execute and set up the 
		// material instance constants when calling UpdateSkeletalComponent().
		PreviewSkelComp->FaceFXActorInstance->SetIsOpenInStudio(FxTrue);
		UpdateSkeletalComponent();
		// The component was potentially just re-attached, so wait for the 
		// commands to process before continuing.
		FlushRenderingCommands();

		PreviewVC->Viewport->Invalidate();

		// Tell the FaceFX Studio session about the actor instance that was just 
		// created on the PreviewSkelComp.
		void* pVoidSession = NULL;
		FxSessionProxy::GetSession(&pVoidSession);
		if( pVoidSession )
		{
			FxStudioSession* pSession = reinterpret_cast<FxStudioSession*>(pVoidSession);
			check(PreviewSkelComp->FaceFXActorInstance);
			pSession->SetActorInstance(PreviewSkelComp->FaceFXActorInstance);
		}
	}
	bReadyToDraw = 1;
}

void FxRenderWidgetUE3::Tick( FLOAT DeltaSeconds )
{
	if( PreviewSkelComp )
	{
		// Tick the PreviewSkelComp to move animation forwards, then Update to update bone locations.
		PreviewSkelComp->TickTag++;
		if( PreviewSkelComp->Animations )
		{
			PreviewSkelComp->Animations->TickAnim(DeltaSeconds);
		}
		
		UpdateSkeletalComponent();
	}
}

void FxRenderWidgetUE3::UpdateSkeletalComponent( void )
{
	// Force any re-attach commands to be executed.
	FlushRenderingCommands();
	if( PreviewSkelComp && PreviewSkelComp->IsAttached() )
	{
		PreviewSkelComp->UpdateSkelPose();
		PreviewSkelComp->ConditionalUpdateTransform();

		// Force any re-attach commands to be executed.
		FlushRenderingCommands();

		// Tell the FaceFX Studio session about the actor instance that was 
		// potentially just re-created on the PreviewSkelComp.
		void* pVoidSession = NULL;
		FxSessionProxy::GetSession(&pVoidSession);
		if( pVoidSession )
		{
			FxStudioSession* pSession = reinterpret_cast<FxStudioSession*>(pVoidSession);
			check(PreviewSkelComp->FaceFXActorInstance);
			pSession->SetActorInstance(PreviewSkelComp->FaceFXActorInstance);
		}
	}
}

void FxRenderWidgetUE3::OnSize( wxSizeEvent& event )
{
	if( PreviewWindow )
	{
		wxRect rc = GetClientRect();
		::MoveWindow((HWND)PreviewWindow->GetHandle(), 0, 0, rc.GetWidth(), rc.GetHeight(), 1);
	}
	event.Skip();
}

void FxRenderWidgetUE3::OnHelp(wxHelpEvent& FxUnused(event))
{
	static_cast<FxStudioMainWin*>(FxStudioApp::GetMainWindow())->GetHelpController().LoadFile();
	static_cast<FxStudioMainWin*>(FxStudioApp::GetMainWindow())->GetHelpController().DisplaySection(wxT("Unreal Engine 3 Integration"));
}

} // namespace Face

} // namespace OC3Ent

#endif
