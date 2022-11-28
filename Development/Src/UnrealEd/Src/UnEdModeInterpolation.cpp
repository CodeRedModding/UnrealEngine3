/*=============================================================================
	UnEdModeInterpolation : Editor mode for setting up interpolation sequences.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "InterpEditor.h"

static const FLOAT	CurveHandleScale = 0.5f;

//////////////////////////////////////////////////////////////////////////
// FEdModeInterpEdit
//////////////////////////////////////////////////////////////////////////

FEdModeInterpEdit::FEdModeInterpEdit()
{
	ID = EM_InterpEdit;
	Desc = TEXT("InterpEdit");

	Interp = NULL;
	InterpEd = NULL;

	Tools.AddItem( new FModeTool_InterpEdit() );
	SetCurrentTool( MT_InterpEdit );

	Settings = NULL;

	bLeavingMode = false;
}

FEdModeInterpEdit::~FEdModeInterpEdit()
{
}

void FEdModeInterpEdit::Enter()
{
	FEdMode::Enter();
}

void FEdModeInterpEdit::Exit()
{
	Interp = NULL;

	// If there is one, close the Interp Editor and clear pointers.
	if(InterpEd != NULL)
	{
		bLeavingMode = true; // This is so the editor being closed doesn't try and change the mode again!

		InterpEd->Close(true);

		bLeavingMode = false;
	}

	InterpEd = NULL;

	FEdMode::Exit();
}

// We see if we have 
void FEdModeInterpEdit::ActorMoveNotify()
{
	if(!InterpEd)
		return;

	InterpEd->ActorModified();
}


void FEdModeInterpEdit::CamMoveNotify(FEditorLevelViewportClient* ViewportClient)
{
	if(!InterpEd)
		return;

	InterpEd->CamMoved(ViewportClient->ViewLocation, ViewportClient->ViewRotation);
}

void FEdModeInterpEdit::ActorSelectionChangeNotify()
{
	if(!InterpEd)
		return;

	InterpEd->ActorSelectionChange();
}

void FEdModeInterpEdit::ActorPropChangeNotify()
{
	if(!InterpEd)
		return;

	InterpEd->ActorModified();
}

// Set the currently edited SeqAct_Interp and open timeline window. Should always be called after we change to EM_InterpEdit.
void FEdModeInterpEdit::InitInterpMode(USeqAct_Interp* EditInterp)
{
	check(EditInterp);
	check(!InterpEd);

	Interp = EditInterp;

	InterpEd = new WxInterpEd( GApp->EditorFrame, -1, Interp );
	InterpEd->Show(1);
}

void FEdModeInterpEdit::RenderForeground(const FSceneContext& Context, FPrimitiveRenderInterface* PRI)
{
	FEdMode::RenderForeground(Context, PRI);

	check( GEditorModeTools.GetCurrentModeID() == EM_InterpEdit );

	if(InterpEd)
		InterpEd->DrawTracks3D(Context, PRI);
}

void FEdModeInterpEdit::DrawHUD(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,const FSceneContext& Context,FRenderInterface* RI)
{
	FEdMode::DrawHUD(ViewportClient,Viewport,Context,RI);

	if(InterpEd)
		InterpEd->DrawModeHUD(ViewportClient,Viewport,Context,RI);
}

UBOOL FEdModeInterpEdit::AllowWidgetMove()
{
	FModeTool_InterpEdit* InterpTool = (FModeTool_InterpEdit*)FindTool(MT_InterpEdit);

	if(InterpTool->bMovingHandle)
	{
		return false;
	}
	else
	{
		return true;
	}
}

//////////////////////////////////////////////////////////////////////////
// FModeTool_InterpEdit
//////////////////////////////////////////////////////////////////////////

FModeTool_InterpEdit::FModeTool_InterpEdit()
{
	ID = MT_InterpEdit;

	bMovingHandle = false;
	DragGroup = false;
	DragTrackIndex = false;
	DragKeyIndex = false;
	bDragArriving = false;
}

FModeTool_InterpEdit::~FModeTool_InterpEdit()
{
}

UBOOL FModeTool_InterpEdit::MouseMove(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,INT x, INT y)
{
	check( GEditorModeTools.GetCurrentModeID() == EM_InterpEdit );

	FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools.GetCurrentMode();

	return 1;
}

UBOOL FModeTool_InterpEdit::InputKey(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,FName Key,EInputEvent Event)
{
	check( GEditorModeTools.GetCurrentModeID() == EM_InterpEdit );

	FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools.GetCurrentMode();
	check(mode->InterpEd);

	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);

	if( Key == KEY_LeftMouseButton )
	{

		if( Event == IE_Pressed)
		{
			INT HitX = ViewportClient->Viewport->GetMouseX();
			INT HitY = ViewportClient->Viewport->GetMouseY();
			HHitProxy*	HitResult = ViewportClient->Viewport->GetHitProxy(HitX, HitY);

			if(HitResult)
			{
				if( HitResult->IsA(TEXT("HInterpTrackKeypointProxy")) )
				{
					HInterpTrackKeypointProxy* KeyProxy = (HInterpTrackKeypointProxy*)HitResult;
					UInterpGroup* Group = KeyProxy->Group;
					INT TrackIndex = KeyProxy->TrackIndex;
					INT KeyIndex = KeyProxy->KeyIndex;

					// This will invalidate the display - so we must not access the KeyProxy after this!
					mode->InterpEd->SetActiveTrack(Group, TrackIndex);

					if(!bCtrlDown)
						mode->InterpEd->ClearKeySelection();

					mode->InterpEd->AddKeyToSelection(Group, TrackIndex, KeyIndex, true);
				}
				else if( HitResult->IsA(TEXT("HInterpTrackKeyHandleProxy")) )
				{
					// If we clicked on a 3D track handle, remember which key.
					HInterpTrackKeyHandleProxy* KeyProxy = (HInterpTrackKeyHandleProxy*)HitResult;
					DragGroup = KeyProxy->Group;
					DragTrackIndex = KeyProxy->TrackIndex;
					DragKeyIndex = KeyProxy->KeyIndex;
					bDragArriving = KeyProxy->bArriving;

					bMovingHandle = true;

					mode->InterpEd->BeginDrag3DHandle(DragGroup, DragTrackIndex);
				}
			}
		}
		else if( Event == IE_Released)
		{
			if(bMovingHandle)
			{
				mode->InterpEd->EndDrag3DHandle();
				bMovingHandle = false;
			}
		}
	}

	// Handle keys
	if( Event == IE_Pressed )
	{
		if( Key == KEY_Delete )
		{
			// Swallow 'Delete' key to avoid deleting stuff when trying to interpolate it!
			return true;
		}
		else if( mode->InterpEd->ProcessKeyPress( Key, bCtrlDown ) )
		{
			return true;
		}
	}

	return FModeTool::InputKey(ViewportClient, Viewport, Key, Event);
}

UBOOL FModeTool_InterpEdit::InputDelta(FEditorLevelViewportClient* InViewportClient,FChildViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale)
{
	check( GEditorModeTools.GetCurrentModeID() == EM_InterpEdit );

	FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools.GetCurrentMode();
	check(mode->InterpEd);

	if(bMovingHandle)
	{
		mode->InterpEd->Move3DHandle( DragGroup, DragTrackIndex, DragKeyIndex, bDragArriving, InDrag * (1.f/CurveHandleScale) );

		return 1;
	}


	InViewportClient->Viewport->Invalidate();

	return 0;
}

void FModeTool_InterpEdit::SelectNone()
{
	check( GEditorModeTools.GetCurrentModeID() == EM_InterpEdit );

	FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GEditorModeTools.GetCurrentMode();

}

//////////////////////////////////////////////////////////////////////////
// WxModeBarInterpEdit
//////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE( WxModeBarInterpEdit, WxModeBar )
END_EVENT_TABLE()

WxModeBarInterpEdit::WxModeBarInterpEdit( wxWindow* InParent, wxWindowID InID )
	: WxModeBar( (wxWindow*)InParent, InID )
{
	SetSize( wxRect( -1, -1, 400, 1) );
	LoadData();
	Show(0);
}

WxModeBarInterpEdit::~WxModeBarInterpEdit()
{
}