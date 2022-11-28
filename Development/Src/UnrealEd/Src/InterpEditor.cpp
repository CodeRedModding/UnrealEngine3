/*=============================================================================
	InterpEditor.cpp: Interpolation editing
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "InterpEditor.h"
#include "UnLinkedObjDrawUtils.h"

IMPLEMENT_CLASS(UInterpEdOptions);

static const INT LabelWidth = 140;
static const FLOAT ZoomIncrement = 1.2f;

static const FColor PositionMarkerLineColor(255, 222, 206);
static const FColor LoopRegionFillColor(80,255,80,4);
static const FColor Track3DSelectedColor(255,255,0);


/*-----------------------------------------------------------------------------
	UInterpEdTransBuffer / FInterpEdTransaction
-----------------------------------------------------------------------------*/

void UInterpEdTransBuffer::BeginSpecial(const TCHAR* SessionName)
{
	CheckState();
	if( ActiveCount++==0 )
	{
		// Cancel redo buffer.
		//debugf(TEXT("BeginTrans %s"), SessionName);
		if( UndoCount )
		{
			UndoBuffer.Remove( UndoBuffer.Num()-UndoCount, UndoCount );
		}

		UndoCount = 0;

		// Purge previous transactions if too much data occupied.
		while( UndoDataSize() > MaxMemory )
		{
			UndoBuffer.Remove( 0 );
		}

		// Begin a new transaction.
		GUndo = new(UndoBuffer)FInterpEdTransaction( SessionName, 1 );
		Overflow = 0;
	}
	CheckState();
}

void UInterpEdTransBuffer::EndSpecial()
{
	CheckState();
	check(ActiveCount>=1);
	if( --ActiveCount==0 )
	{
		GUndo = NULL;
	}
	CheckState();
}

void FInterpEdTransaction::SaveObject( UObject* Object )
{
	check(Object);

	if( Object->IsA( USeqAct_Interp::StaticClass() ) ||
		Object->IsA( UInterpData::StaticClass() ) ||
		Object->IsA( UInterpGroup::StaticClass() ) ||
		Object->IsA( UInterpTrack::StaticClass() ) ||
		Object->IsA( UInterpGroupInst::StaticClass() ) ||
		Object->IsA( UInterpTrackInst::StaticClass() ) ||
		Object->IsA( UInterpEdOptions::StaticClass() ) )
	{
		// Save the object.
		new( Records )FObjectRecord( this, Object, NULL, 0, 0, 0, 0, NULL, NULL );
	}
}

void FInterpEdTransaction::SaveArray( UObject* Object, FArray* Array, INT Index, INT Count, INT Oper, INT ElementSize, STRUCT_AR Serializer, STRUCT_DTOR Destructor )
{
	// Never want this.
}

IMPLEMENT_CLASS(UInterpEdTransBuffer);

/*-----------------------------------------------------------------------------
 FInterpEdViewportClient
-----------------------------------------------------------------------------*/

FInterpEdViewportClient::FInterpEdViewportClient( class WxInterpEd* InInterpEd )
{
	InterpEd = InInterpEd;

	// Set initial zoom range
	ViewStartTime = 0.f;
	ViewEndTime = InterpEd->IData->InterpLength;

	OldMouseX = 0;
	OldMouseY = 0;

	DistanceDragged = 0;

	BoxStartX = 0;
	BoxStartY = 0;
	BoxEndX = 0;
	BoxEndY = 0;

	PixelsPerSec = 1.f;
	TrackViewSizeX = 0;

	bPanning = false;
	bMouseDown = false;
	bGrabbingHandle = false;
	bBoxSelecting = false;
	bTransactionBegun = false;
	bNavigating = false;
	bGrabbingMarker	= false;

	Realtime = false;
}

FInterpEdViewportClient::~FInterpEdViewportClient()
{

}

void FInterpEdViewportClient::InputKey(FChildViewport* Viewport, FName Key, EInputEvent Event,FLOAT)
{
	//Viewport->CaptureMouseInput( Viewport->KeyState(KEY_LeftMouseButton) || Viewport->KeyState(KEY_RightMouseButton) );

	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);
	UBOOL bAltDown = Viewport->KeyState(KEY_LeftAlt) || Viewport->KeyState(KEY_RightAlt);

	INT HitX = Viewport->GetMouseX();
	INT HitY = Viewport->GetMouseY();

	if( Key == KEY_LeftMouseButton )
	{
		switch( Event )
		{
		case IE_Pressed:
			{
				HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);

				if(HitResult)
				{
					if(HitResult->IsA(TEXT("HInterpEdGroupTitle")))
					{
						UInterpGroup* Group = ((HInterpTrackKeypointProxy*)HitResult)->Group;

						InterpEd->SetActiveTrack(Group, INDEX_NONE);

						InterpEd->ClearKeySelection();
					}
					else if(HitResult->IsA(TEXT("HInterpEdGroupCollapseBtn")))
					{
						UInterpGroup* Group = ((HInterpTrackKeypointProxy*)HitResult)->Group;

						InterpEd->SetActiveTrack(Group, INDEX_NONE);
						Group->bCollapsed = !Group->bCollapsed;

						InterpEd->ClearKeySelection();
					}
					else if(HitResult->IsA(TEXT("HInterpEdGroupLockCamBtn")))
					{
						UInterpGroup* Group = ((HInterpEdGroupLockCamBtn*)HitResult)->Group;

						if(Group == InterpEd->CamViewGroup)
						{
							InterpEd->LockCamToGroup(NULL);
						}
						else
						{
							InterpEd->LockCamToGroup(Group);
						}
					}
					else if(HitResult->IsA(TEXT("HInterpEdTrackTitle")))
					{
						UInterpGroup* Group = ((HInterpTrackKeypointProxy*)HitResult)->Group;
						INT TrackIndex = ((HInterpEdTrackTitle*)HitResult)->TrackIndex;

						InterpEd->SetActiveTrack(Group, TrackIndex);

						InterpEd->ClearKeySelection();
					}
					else if(HitResult->IsA(TEXT("HInterpEdTrackGraphPropBtn")))
					{
						UInterpGroup* Group = ((HInterpEdTrackGraphPropBtn*)HitResult)->Group;
						INT TrackIndex = ((HInterpEdTrackGraphPropBtn*)HitResult)->TrackIndex;

						InterpEd->AddTrackToCurveEd(Group, TrackIndex);
					}
					else if(HitResult->IsA(TEXT("HInterpEdEventDirBtn")))
					{
						UInterpGroup* Group = ((HInterpEdEventDirBtn*)HitResult)->Group;
						INT TrackIndex = ((HInterpEdEventDirBtn*)HitResult)->TrackIndex;
						EInterpEdEventDirection Dir = ((HInterpEdEventDirBtn*)HitResult)->Dir;

						UInterpTrackEvent* EventTrack = CastChecked<UInterpTrackEvent>( Group->InterpTracks(TrackIndex) );

						if(Dir == IED_Forward)
						{
							EventTrack->bFireEventsWhenForwards = !EventTrack->bFireEventsWhenForwards;
						}
						else
						{
							EventTrack->bFireEventsWhenBackwards = !EventTrack->bFireEventsWhenBackwards;
						}
					}
					else if(HitResult->IsA(TEXT("HInterpTrackKeypointProxy")))
					{
						UInterpGroup* Group = ((HInterpTrackKeypointProxy*)HitResult)->Group;
						INT TrackIndex = ((HInterpTrackKeypointProxy*)HitResult)->TrackIndex;
						INT KeyIndex = ((HInterpTrackKeypointProxy*)HitResult)->KeyIndex;

						if(!bCtrlDown)
						{
							InterpEd->ClearKeySelection();
							InterpEd->AddKeyToSelection(Group, TrackIndex, KeyIndex, true);
						}
					}
					else if(HitResult->IsA(TEXT("HInterpEdTrackBkg")))
					{
						InterpEd->SetActiveTrack(NULL, INDEX_NONE);
					}
					else if(HitResult->IsA(TEXT("HInterpEdTimelineBkg")))
					{
						FLOAT NewTime = ViewStartTime + ((HitX - LabelWidth) / PixelsPerSec);

						// When jumping to location by clicking, stop playback.
						InterpEd->Interp->Stop();
						Realtime = false;

						// Move to clicked on location
						InterpEd->SetInterpPosition(NewTime);

						// Act as if we grabbed the handle as well.
						bGrabbingHandle = true;
					}
					else if(HitResult->IsA(TEXT("HInterpEdNavigator")))
					{
						// Center view on position we clicked on in the navigator.
						FLOAT JumpToTime = ((HitX - LabelWidth)/NavPixelsPerSecond);
						FLOAT ViewWindow = (ViewEndTime - ViewStartTime);

						ViewStartTime = JumpToTime - (0.5f * ViewWindow);
						ViewEndTime = JumpToTime + (0.5f * ViewWindow);
						InterpEd->SyncCurveEdView();

						bNavigating = true;
					}
					else if(HitResult->IsA(TEXT("HInterpEdMarker")))
					{
						GrabbedMarkerType = ((HInterpEdMarker*)HitResult)->Type;

						InterpEd->BeginMoveMarker();
						bGrabbingMarker = true;
					}
				}
				else
				{
					if(bCtrlDown && bAltDown)
					{
						BoxStartX = BoxEndX = HitX;
						BoxStartY = BoxEndY = HitY;

						bBoxSelecting = true;
					}
					else
					{
						bPanning = true;
					}
				}

				Viewport->LockMouseToWindow(true);

				bMouseDown = true;
				OldMouseX = HitX;
				OldMouseY = HitY;
				DistanceDragged = 0;
			}
			break;

		case IE_Released:
			{
				if(bBoxSelecting)
				{
					INT MinX = Min(BoxStartX, BoxEndX);
					INT MinY = Min(BoxStartY, BoxEndY);
					INT MaxX = Max(BoxStartX, BoxEndX) + 1;
					INT MaxY = Max(BoxStartY, BoxEndY) + 1;
					INT TestSizeX = MaxX - MinX;
					INT TestSizeY = MaxY - MinY;

					// Find how much (in time) 1.5 pixels represents on the screen.
					FLOAT PixelTime = 1.5f/PixelsPerSec;

					// We read back the hit proxy map for the required region.
					TArray<HHitProxy*> ProxyMap;
					Viewport->GetHitProxyMap((UINT)MinX, (UINT)MinY, (UINT)MaxX, (UINT)MaxY, ProxyMap);

					TArray<FInterpEdSelKey>	NewSelection;

					// Find any keypoint hit proxies in the region - add the keypoint to selection.
					for(INT Y=0; Y < TestSizeY; Y++)
					{
						for(INT X=0; X < TestSizeX; X++)
						{
							HHitProxy* HitProxy = ProxyMap(Y * TestSizeX + X);

							if(HitProxy && HitProxy->IsA(TEXT("HInterpTrackKeypointProxy")))
							{
								UInterpGroup* Group = ((HInterpTrackKeypointProxy*)HitProxy)->Group;
								INT TrackIndex = ((HInterpTrackKeypointProxy*)HitProxy)->TrackIndex;
								INT KeyIndex = ((HInterpTrackKeypointProxy*)HitProxy)->KeyIndex;

								// Because AddKeyToSelection might invalidate the display, we just remember all the keys here and process them together afterwards.
								NewSelection.AddUniqueItem( FInterpEdSelKey(Group, TrackIndex, KeyIndex) );

								// Slight hack here. We select any other keys on the same track which are within 1.5 pixels of this one.
								UInterpTrack* Track = Group->InterpTracks(TrackIndex);
								FLOAT SelKeyTime = Track->GetKeyframeTime(KeyIndex);

								for(INT i=0; i<Track->GetNumKeyframes(); i++)
								{
									FLOAT KeyTime = Track->GetKeyframeTime(i);
									if( Abs(KeyTime - SelKeyTime) < PixelTime )
									{
										NewSelection.AddUniqueItem( FInterpEdSelKey(Group, TrackIndex, i) );
									}
								}
							}
						}
					}

					if(!bShiftDown)
					{
						InterpEd->ClearKeySelection();
					}

					for(INT i=0; i<NewSelection.Num(); i++)
					{
						InterpEd->AddKeyToSelection( NewSelection(i).Group, NewSelection(i).TrackIndex, NewSelection(i).KeyIndex, false );
					}
				}
				else if(DistanceDragged < 4)
				{
					HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);

					// If mouse didn't really move since last time, and we released over empty space, deselect everything.
					if(!HitResult)
					{
						InterpEd->ClearKeySelection();
					}
					else if(bCtrlDown && HitResult->IsA(TEXT("HInterpTrackKeypointProxy")))
					{
						UInterpGroup* Group = ((HInterpTrackKeypointProxy*)HitResult)->Group;
						INT TrackIndex = ((HInterpTrackKeypointProxy*)HitResult)->TrackIndex;
						INT KeyIndex = ((HInterpTrackKeypointProxy*)HitResult)->KeyIndex;

						UBOOL bAlreadySelected = InterpEd->KeyIsInSelection(Group, TrackIndex, KeyIndex);
						if(bAlreadySelected)
						{
							InterpEd->RemoveKeyFromSelection(Group, TrackIndex, KeyIndex);
						}
						else
						{
							InterpEd->AddKeyToSelection(Group, TrackIndex, KeyIndex, true);
						}
					}
				}

				if(bTransactionBegun)
				{
					InterpEd->EndMoveSelectedKeys();
					bTransactionBegun = false;
				}

				if(bGrabbingMarker)
				{
					InterpEd->EndMoveMarker();
					bGrabbingMarker = false;
				}

				Viewport->LockMouseToWindow(false);

				DistanceDragged = 0;

				bPanning = false;
				bMouseDown = false;
				bGrabbingHandle = false;
				bNavigating = false;
				bBoxSelecting = false;
			}
			break;
		}
	}
	else if( Key == KEY_RightMouseButton )
	{
		switch( Event )
		{
		case IE_Pressed:
			{
				INT HitX = Viewport->GetMouseX();
				INT HitY = Viewport->GetMouseY();
				HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);

				if(HitResult)
				{
					wxMenu* Menu = NULL;

					if(HitResult->IsA(TEXT("HInterpEdTrackBkg")))
					{
						InterpEd->ClearKeySelection();
						InterpEd->SetActiveTrack(NULL, INDEX_NONE);				
						Viewport->Invalidate();

						Menu = new WxMBInterpEdBkgMenu(InterpEd);
					}
					else if(HitResult->IsA(TEXT("HInterpEdGroupTitle")))
					{
						UInterpGroup* Group = ((HInterpEdGroupTitle*)HitResult)->Group;

						InterpEd->ClearKeySelection();
						InterpEd->SetActiveTrack(Group, INDEX_NONE);
						Viewport->Invalidate();

						Menu = new WxMBInterpEdGroupMenu(InterpEd);		
					}
					else if(HitResult->IsA(TEXT("HInterpEdTrackTitle")))
					{
						UInterpGroup* Group = ((HInterpEdTrackTitle*)HitResult)->Group;
						INT TrackIndex = ((HInterpEdTrackTitle*)HitResult)->TrackIndex;

						InterpEd->ClearKeySelection();
						InterpEd->SetActiveTrack(Group, TrackIndex);
						Viewport->Invalidate();

						Menu = new WxMBInterpEdTrackMenu(InterpEd);
					}
					else if(HitResult->IsA(TEXT("HInterpTrackKeypointProxy")))
					{
						UInterpGroup* Group = ((HInterpTrackKeypointProxy*)HitResult)->Group;
						INT TrackIndex = ((HInterpTrackKeypointProxy*)HitResult)->TrackIndex;
						INT KeyIndex = ((HInterpTrackKeypointProxy*)HitResult)->KeyIndex;

						UBOOL bAlreadySelected = InterpEd->KeyIsInSelection(Group, TrackIndex, KeyIndex);
						if(bAlreadySelected)
						{
							Menu = new WxMBInterpEdKeyMenu(InterpEd);
						}
					}

					if(Menu)
					{
						FTrackPopupMenu tpm( InterpEd, Menu );
						tpm.Show();
						delete Menu;
					}
				}
			}
			break;

		case IE_Released:
			{
				
			}
			break;
		}
	}
	
	if(Event == IE_Pressed)
	{
		if( Key == KEY_Delete )
			InterpEd->DeleteSelectedKeys();
		else if(Key == KEY_MouseScrollDown)
			InterpEd->ZoomView(1.f/ZoomIncrement);
		else if(Key == KEY_MouseScrollUp)
			InterpEd->ZoomView(ZoomIncrement);
		else
			InterpEd->ProcessKeyPress(Key, bCtrlDown);
	}
}

// X and Y here are the new screen position of the cursor.
void FInterpEdViewportClient::MouseMove(FChildViewport* Viewport, INT X, INT Y)
{
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);

	INT DeltaX = OldMouseX - X;
	INT DeltaY = OldMouseY - Y;

	if(bMouseDown)
	{
		DistanceDragged += ( Abs<INT>(DeltaX) + Abs<INT>(DeltaY) );
	}

	OldMouseX = X;
	OldMouseY = Y;


	if(bMouseDown)
	{
		if(bGrabbingHandle)
		{
			FLOAT NewTime = ViewStartTime + ((X - LabelWidth) / PixelsPerSec);

			InterpEd->SetInterpPosition(NewTime);
		}
		else if(bBoxSelecting)
		{
			BoxEndX = X;
			BoxEndY = Y;
		}
		else if( bCtrlDown && InterpEd->Opt->SelectedKeys.Num() > 0 )
		{
			if(DistanceDragged > 4)
			{
				if(!bTransactionBegun)
				{
					InterpEd->BeginMoveSelectedKeys();
					bTransactionBegun = true;
				}

				FLOAT DeltaTime = -DeltaX / PixelsPerSec;
				InterpEd->MoveSelectedKeys(DeltaTime);
			}
		}
		else if(bNavigating)
		{
			FLOAT DeltaTime = -DeltaX / NavPixelsPerSecond;
			ViewStartTime += DeltaTime;
			ViewEndTime += DeltaTime;
			InterpEd->SyncCurveEdView();
		}
		else if(bGrabbingMarker)
		{
			FLOAT DeltaTime = -DeltaX / PixelsPerSec;
			UnsnappedMarkerPos += DeltaTime;

			if(GrabbedMarkerType == ISM_SeqEnd)
			{
				InterpEd->SetInterpEnd( InterpEd->SnapTime(UnsnappedMarkerPos) );
			}
			else if(GrabbedMarkerType == ISM_LoopStart || GrabbedMarkerType == ISM_LoopEnd)
			{
				InterpEd->MoveLoopMarker( InterpEd->SnapTime(UnsnappedMarkerPos), GrabbedMarkerType == ISM_LoopStart );
			}
		}
		else if(bPanning)
		{
			FLOAT DeltaTime = -DeltaX / PixelsPerSec;
			ViewStartTime -= DeltaTime;
			ViewEndTime -= DeltaTime;
			InterpEd->SyncCurveEdView();
		}
	}
}

void FInterpEdViewportClient::InputAxis(FChildViewport* Viewport, FName Key, FLOAT Delta, FLOAT DeltaTime)
{
}

EMouseCursor FInterpEdViewportClient::GetCursor(FChildViewport* Viewport,INT X,INT Y)
{
	return MC_Cross;
}

void FInterpEdViewportClient::Tick(FLOAT DeltaSeconds)
{
	InterpEd->TickInterp(DeltaSeconds);

	// If curve editor is shown - sync us with it.
	if(InterpEd->CurveEd->IsShown())
	{
		ViewStartTime = InterpEd->CurveEd->StartIn;
		ViewEndTime = InterpEd->CurveEd->EndIn;
	}

	Viewport->Invalidate();
}

/*-----------------------------------------------------------------------------
 WxInterpEdVCHolder
 -----------------------------------------------------------------------------*/


BEGIN_EVENT_TABLE( WxInterpEdVCHolder, wxWindow )
	EVT_SIZE( WxInterpEdVCHolder::OnSize )
END_EVENT_TABLE()

WxInterpEdVCHolder::WxInterpEdVCHolder( wxWindow* InParent, wxWindowID InID, WxInterpEd* InInterpEd )
: wxWindow( InParent, InID )
{
	InterpEdVC = new FInterpEdViewportClient( InInterpEd );
	InterpEdVC->Viewport = GEngine->Client->CreateWindowChildViewport(InterpEdVC, (HWND)GetHandle());
	InterpEdVC->Viewport->CaptureJoystickInput(false);
}

WxInterpEdVCHolder::~WxInterpEdVCHolder()
{
	GEngine->Client->CloseViewport(InterpEdVC->Viewport);
	InterpEdVC->Viewport = NULL;
	delete InterpEdVC;
}

void WxInterpEdVCHolder::OnSize( wxSizeEvent& In )
{
	wxRect rc = GetClientRect();

	::MoveWindow( (HWND)InterpEdVC->Viewport->GetWindow(), 0, 0, rc.GetWidth(), rc.GetHeight(), 1 );
}


/*-----------------------------------------------------------------------------
 WxInterpEd
 -----------------------------------------------------------------------------*/

UBOOL				WxInterpEd::bInterpTrackClassesInitialized = false;
TArray<UClass*>		WxInterpEd::InterpTrackClasses;

// On init, find all track classes. Will use later on to generate menus.
void WxInterpEd::InitInterpTrackClasses()
{
	if(bInterpTrackClassesInitialized)
		return;

	// Construct list of non-abstract gameplay sequence object classes.
	for(TObjectIterator<UClass> It; It; ++It)
	{
		if( It->IsChildOf(UInterpTrack::StaticClass()) && !(It->ClassFlags & CLASS_Abstract) )
		{
			InterpTrackClasses.AddItem(*It);
		}
	}

	bInterpTrackClassesInitialized = true;
}

BEGIN_EVENT_TABLE( WxInterpEd, wxFrame )
	EVT_SIZE( WxInterpEd::OnSize )
	EVT_CLOSE( WxInterpEd::OnClose )
	EVT_MENU_RANGE( IDM_INTERP_NEW_TRACK_START, IDM_INTERP_NEW_TRACK_END, WxInterpEd::OnContextNewTrack )
	EVT_MENU_RANGE( IDM_INTERP_KEYMODE_LINEAR, IDM_INTERP_KEYMODE_CONSTANT, WxInterpEd::OnContextKeyInterpMode )
	EVT_MENU( IDM_INTERP_EVENTKEY_RENAME, WxInterpEd::OnContextRenameEventKey )
	EVT_MENU( IDM_INTERP_GROUP_RENAME, WxInterpEd::OnContextGroupRename )
	EVT_MENU( IDM_INTERP_GROUP_DELETE, WxInterpEd::OnContextGroupDelete )
	EVT_MENU( IDM_INTERP_TRACK_RENAME, WxInterpEd::OnContextTrackRename )
	EVT_MENU( IDM_INTERP_TRACK_DELETE, WxInterpEd::OnContextTrackDelete )
	EVT_MENU( IDM_INTERP_TOGGLE_CURVEEDITOR, WxInterpEd::OnToggleCurveEd )
	EVT_MENU_RANGE( IDM_INTERP_TRACK_FRAME_WORLD, IDM_INTERP_TRACK_FRAME_RELINITIAL, WxInterpEd::OnContextTrackChangeFrame )
	EVT_MENU( IDM_INTERP_NEW_GROUP, WxInterpEd::OnContextNewGroup )
	EVT_MENU( IDM_INTERP_NEW_DIRECTOR_GROUP, WxInterpEd::OnContextNewGroup )
	EVT_MENU( IDM_INTERP_ADDKEY, WxInterpEd::OnMenuAddKey )
	EVT_MENU( IDM_INTERP_PLAY, WxInterpEd::OnMenuPlay )
	EVT_MENU( IDM_INTERP_PLAYLOOPSECTION, WxInterpEd::OnMenuPlay )
	EVT_MENU( IDM_INTERP_STOP, WxInterpEd::OnMenuStop )
	EVT_MENU_RANGE( IDM_INTERP_SPEED_1, IDM_INTERP_SPEED_100, WxInterpEd::OnChangePlaySpeed )
	EVT_MENU( IDM_INTERP_EDIT_UNDO, WxInterpEd::OnMenuUndo )
	EVT_MENU( IDM_INTERP_EDIT_REDO, WxInterpEd::OnMenuRedo )
	EVT_MENU( IDM_INTERP_TOGGLE_SNAP, WxInterpEd::OnToggleSnap )
	EVT_MENU( IDM_INTERP_VIEW_FITSEQUENCE, WxInterpEd::OnViewFitSequence )
	EVT_MENU( IDM_INTERP_VIEW_FITLOOP, WxInterpEd::OnViewFitLoop )
	EVT_COMBOBOX( IDM_INTERP_SNAPCOMBO, WxInterpEd::OnChangeSnapSize )
	EVT_MENU( IDM_INTERP_EDIT_INSERTSPACE, WxInterpEd::OnMenuInsertSpace )
	EVT_MENU( IDM_INTERP_EDIT_STRETCHSECTION, WxInterpEd::OnMenuStretchSection )
	EVT_MENU( IDM_INTERP_EDIT_DELETESECTION, WxInterpEd::OnMenuDeleteSection )
	EVT_MENU( IDM_INTERP_EDIT_SELECTINSECTION, WxInterpEd::OnMenuSelectInSection )
	EVT_MENU( IDM_INTERP_EDIT_DUPLICATEKEYS, WxInterpEd::OnMenuDuplicateSelectedKeys )
	EVT_MENU( IDM_INTERP_EDIT_SAVEPATHTIME, WxInterpEd::OnSavePathTime )
	EVT_MENU( IDM_INTERP_EDIT_JUMPTOPATHTIME, WxInterpEd::OnJumpToPathTime )
	EVT_SPLITTER_SASH_POS_CHANGED( IDM_INTERP_GRAPH_SPLITTER, WxInterpEd::OnGraphSplitChangePos )
END_EVENT_TABLE()

/** Should NOT open an InterpEd unless InInterp has a valid InterpData attached! */
WxInterpEd::WxInterpEd( wxWindow* InParent, wxWindowID InID, USeqAct_Interp* InInterp )
: wxFrame( InParent, InID, TEXT("Matinee"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR )
{
	// Make sure we have a list of available track classes
	WxInterpEd::InitInterpTrackClasses();

	// Create options object.
	Opt = ConstructObject<UInterpEdOptions>( UInterpEdOptions::StaticClass(), (UObject*)-1, NAME_None, RF_Transactional );
	check(Opt);

	// Swap out regular UTransactor for our special one
	GEditor->Trans->Reset( TEXT("Open Matinee") );

	NormalTransactor = GEditor->Trans;
	InterpEdTrans = new UInterpEdTransBuffer( 8*1024*1024 );
	GEditor->Trans = InterpEdTrans;

	// Load the desired window position from .ini file
	FString Wk;
	GConfig->GetString( TEXT("WindowPosManager"), TEXT("Matinee"), Wk, GEditorIni );

	TArray<FString> Args;
	wxRect rc(800,256,800,800);
	INT PropSashPos = 600;
	GraphSplitPos = 200;
	INT NumArgs = Wk.ParseIntoArray( TEXT(","), &Args );
	if( NumArgs == 5 || NumArgs == 6 )
	{
		INT X = appAtoi( *Args(0) );
		INT Y = appAtoi( *Args(1) );
		INT W = appAtoi( *Args(2) );
		INT H = appAtoi( *Args(3) );

		PropSashPos = appAtoi( *Args(4) );
		if(NumArgs == 6)
		{
			GraphSplitPos = appAtoi( *Args(5) );
		}

		INT vleft = ::GetSystemMetrics( SM_XVIRTUALSCREEN );
		INT vtop = ::GetSystemMetrics( SM_YVIRTUALSCREEN );
		INT vright = ::GetSystemMetrics( SM_CXVIRTUALSCREEN );
		INT vbottom = ::GetSystemMetrics( SM_CYVIRTUALSCREEN );

		if( X < vleft || X+W >= vright )		X = vleft;
		if( Y < vtop || Y+H >= vbottom )		Y = vtop;

		rc.SetX( X );
		rc.SetY( Y );
		rc.SetWidth( W );
		rc.SetHeight( H );
	}

	// Set up pointers to interp objects
	Interp = InInterp;

	// Do all group/track instancing and variable hook-up.
	Interp->InitInterp();

	// Should always find some data.
	check(Interp->InterpData);
	IData = Interp->InterpData;

	// Slight hack to ensure interpolation data is transactional.
	Interp->SetFlags( RF_Transactional );
	IData->SetFlags( RF_Transactional );
	for(INT i=0; i<IData->InterpGroups.Num(); i++)
	{
		UInterpGroup* Group = IData->InterpGroups(i);
		Group->SetFlags( RF_Transactional );

		for(INT j=0; j<Group->InterpTracks.Num(); j++)
		{
			Group->InterpTracks(j)->SetFlags( RF_Transactional );
		}
	}

	// For each track let it save the state of the object its going to work on before being changed at all by Matinee
	for(INT i=0; i<Interp->GroupInst.Num(); i++)
	{
		Interp->GroupInst(i)->SaveGroupActorState();
	}

	// Set position to the start of the interpolation.
	// Will position objects as the first frame of the sequence.
	Interp->UpdateInterp(0.f, true);

	ActiveGroup = NULL;
	ActiveTrackIndex = INDEX_NONE;

	CamViewGroup = NULL;

	bAdjustingKeyframe = false;
	bLoopingSection = false;
	bDragging3DHandle = false;

	PlaybackSpeed = 1.0f;

	// Update cam frustum colours.
	UpdateCamColours();

	// Create other windows and stuff

	wxBoxSizer* item2 = new wxBoxSizer(wxVERTICAL);
	SetSizer(item2);
	SetAutoLayout(TRUE);

	TopSplitterWnd = new wxSplitterWindow(this, -1, wxDefaultPosition, wxSize(100, 100), wxSP_3DBORDER|wxSP_FULLSASH );
	GraphSplitterWnd = new wxSplitterWindow(TopSplitterWnd, IDM_INTERP_GRAPH_SPLITTER, wxDefaultPosition, wxSize(100, 100), wxSP_3DBORDER|wxSP_FULLSASH );
	GraphSplitterWnd->SetMinimumPaneSize(40);

	PropertyWindow = new WxPropertyWindow( TopSplitterWnd, this );

	TopSplitterWnd->SplitHorizontally( GraphSplitterWnd, PropertyWindow, PropSashPos );

	TrackWindow = new WxInterpEdVCHolder( GraphSplitterWnd, -1, this );
	InterpEdVC = TrackWindow->InterpEdVC;

	// Create new curve editor setup if not already done
	if(!IData->CurveEdSetup)
	{
		IData->CurveEdSetup = ConstructObject<UInterpCurveEdSetup>( UInterpCurveEdSetup::StaticClass(), IData, NAME_None, RF_NotForClient | RF_NotForServer );
	}

	// Create graph editor to work on InterpData's CurveEd setup.
	CurveEd = new WxCurveEditor( GraphSplitterWnd, -1, IData->CurveEdSetup );

	// Register this window with the Curve editor so we will be notified of various things.
	CurveEd->SetNotifyObject(this);

	// Set graph view to match track view.
	SyncCurveEdView();

	PosMarkerColor = PositionMarkerLineColor;
	RegionFillColor = LoopRegionFillColor;

	CurveEd->SetEndMarker(true, IData->InterpLength);
	CurveEd->SetPositionMarker(true, 0.f, PosMarkerColor);
	CurveEd->SetRegionMarker(true, IData->EdSectionStart, IData->EdSectionEnd, RegionFillColor);

    // Initially the curve editor is not visible.
	GraphSplitterWnd->Initialize(TrackWindow);

	item2->Add(TopSplitterWnd, 1, wxGROW|wxALL, 0);

	GetSizer()->Fit(this);
	GetSizer()->SetSizeHints(this);
	Centre();

	SetSize(rc);

	// Create toolbar
	ToolBar = new WxInterpEdToolBar( this, -1 );
	SetToolBar( ToolBar );

	// Initialise snap settings.
	bSnapEnabled = false;
	SnapAmount = InterpEdSnapSizes[ ToolBar->SnapCombo->GetSelection() ];
	
	// Create menu bar
	MenuBar = new WxInterpEdMenuBar();
	SetMenuBar( MenuBar );

	// Will look at current selection to set active track
	ActorSelectionChange();

	// Load gradient texture for bars
	BarGradText = LoadObject<UTexture2D>(NULL, TEXT("EditorMaterials.MatineeGreyGrad"), NULL, LOAD_NoFail, NULL);

	// If there is a Director group in this data, default to locking the camera to it.
	UInterpGroupDirector* DirGroup = IData->FindDirectorGroup();
	if(DirGroup)
	{
		LockCamToGroup(DirGroup);
	}

	for(INT i=0; i<4; i++)
	{
		WxLevelViewportWindow* LevelVC = GApp->EditorFrame->ViewportConfigData->Viewports[i].ViewportWindow;
		if(LevelVC)
		{
			// If there is a director group, set the perspective viewports to realtime automatically.
			if(DirGroup && LevelVC->ViewportType == LVT_Perspective)
			{
				LevelVC->Realtime = true;
			}

			// Turn on 'show camera frustums' flag
			LevelVC->ShowFlags |= SHOW_CamFrustums;
		}
	}

	// Update UI to reflect any change in realtime status
	GCallback->Send( CALLBACK_UpdateUI );
}

WxInterpEd::~WxInterpEd()
{
	wxRect rc = GetRect();

	FString Wk = *FString::Printf( TEXT("%d,%d,%d,%d,%d,%d"), 
		rc.GetX(), 
		rc.GetY(), 
		rc.GetWidth(), 
		rc.GetHeight(), 
		TopSplitterWnd->GetSashPosition(), 
		GraphSplitPos );

	GConfig->SetString( TEXT("WindowPosManager"), TEXT("Matinee"), *Wk, GEditorIni );
}

void WxInterpEd::Serialize(FArchive& Ar)
{
	Ar << Interp;
	Ar << IData;
	Ar << NormalTransactor;
	Ar << Opt;

	CurveEd->CurveEdVC->Serialize(Ar);
	InterpEdVC->Serialize(Ar);
}

void WxInterpEd::OnSize( wxSizeEvent& In )
{
	if( TopSplitterWnd )
	{
		wxRect rc = GetClientRect();
		TopSplitterWnd->SetSize( rc );
	}

	In.Skip();
}

void WxInterpEd::OnClose( wxCloseEvent& In )
{
	// Re-instate regular transactor
	check( GEditor->Trans == InterpEdTrans );
	check( NormalTransactor->IsA( UTransBuffer::StaticClass() ) );

	GEditor->Trans->Reset( TEXT("Exit Matinee") );
	GEditor->Trans = NormalTransactor;

	// Detach editor camera from any group and clear any previewing stuff.
	LockCamToGroup(NULL);

	// Restore the saved state of any actors we were previewing interpolation on.
	for(INT i=0; i<Interp->GroupInst.Num(); i++)
	{
		Interp->GroupInst(i)->RestoreGroupActorState();

		// Set any manipulated cameras back to default frustum colours.
		ACameraActor* Cam = Cast<ACameraActor>(Interp->GroupInst(i)->GroupActor);
		if(Cam && Cam->DrawFrustum)
		{
			ACameraActor* DefCam = (ACameraActor*)(Cam->GetClass()->GetDefaultActor());
			Cam->DrawFrustum->FrustumColor = DefCam->DrawFrustum->FrustumColor;
		}
	}

	// Destroy all group/track instances
	//Interp->TermInterp();
	// We actuall don't want to call TermInterp in the editor - just leave them for garbage collection.
	// Because we don't delete groups/tracks so undo works, we could end up deleting the Outer of a valid object.
	Interp->GroupInst.Empty();
	Interp->InterpData = NULL;

	// Reset interpolation to the beginning when quitting.
	Interp->Position = 0.f;

	// When they close the window - change the mode away from InterpEdit.
	if( GEditorModeTools.GetCurrentModeID() == EM_InterpEdit )
	{
		FEdModeInterpEdit* InterpEditMode = (FEdModeInterpEdit*)GEditorModeTools.GetCurrentMode();

		// Only change mode if this window closing wasn't instigated by someone changing mode!
		if( !InterpEditMode->bLeavingMode )
		{
			InterpEditMode->InterpEd = NULL;
			GApp->SetCurrentMode( EM_Default );
		}
	}

	// Undo any weird settings to editor level viewports.
	for(INT i=0; i<4; i++)
	{
		WxLevelViewportWindow* LevelVC = GApp->EditorFrame->ViewportConfigData->Viewports[i].ViewportWindow;
		if(LevelVC)
		{
			// Turn off 'show camera frustums' flag.
			LevelVC->ShowFlags &= ~SHOW_CamFrustums;
		}
	}

	this->Destroy();
}

void WxInterpEd::DrawTracks3D(const FSceneContext& Context, FPrimitiveRenderInterface* PRI)
{
	for(INT i=0; i<Interp->GroupInst.Num(); i++)
	{
		UInterpGroupInst* GrInst = Interp->GroupInst(i);
		check( GrInst->Group );
		check( GrInst->TrackInst.Num() == GrInst->Group->InterpTracks.Num() );

		// In 3D viewports, Don't draw path if we are locking the camera to this group.
		//if( !(Context.View->ShowFlags & SHOW_Orthographic) && (Group == CamViewGroup) )
		//	continue;

		for(INT j=0; j<GrInst->TrackInst.Num(); j++)
		{
			UInterpTrackInst* TrInst = GrInst->TrackInst(j);
			UInterpTrack* Track = GrInst->Group->InterpTracks(j);

			//UBOOL bTrackSelected = ((GrInst->Group == ActiveGroup) && (j == ActiveTrackIndex));
			UBOOL bTrackSelected = (GrInst->Group == ActiveGroup);
			FColor TrackColor = bTrackSelected ? Track3DSelectedColor : GrInst->Group->GroupColor;

			Track->Render3DTrack( TrInst, Context, PRI, j, TrackColor, Opt->SelectedKeys);
		}
	}
}

void WxInterpEd::DrawModeHUD(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,const FSceneContext& Context,FRenderInterface* RI)
{
	// Show a notification if we are adjusting a particular keyframe.
	if(bAdjustingKeyframe)
	{
		check(Opt->SelectedKeys.Num() == 1);

		FInterpEdSelKey& SelKey = Opt->SelectedKeys(0);
		FString AdjustNotify = FString::Printf( TEXT("ADJUST KEY %d"), SelKey.KeyIndex );

		INT XL, YL;
		RI->StringSize(GEngine->SmallFont, XL, YL, *AdjustNotify);
		RI->DrawString( 3, Viewport->GetSizeY() - (3 + YL) , *AdjustNotify, GEngine->SmallFont, FLinearColor::White );
	}
}

///////////////////////////////////////////////////////////////////////////////////////
// Properties window NotifyHook stuff

void WxInterpEd::NotifyDestroy( void* Src )
{

}

void WxInterpEd::NotifyPreChange( void* Src, UProperty* PropertyAboutToChange )
{

}

void WxInterpEd::NotifyPostChange( void* Src, UProperty* PropertyThatChanged )
{
	CurveEd->CurveChanged();
	InterpEdVC->Viewport->Invalidate();
}

void WxInterpEd::NotifyExec( void* Src, const TCHAR* Cmd )
{

}

///////////////////////////////////////////////////////////////////////////////////////
// Curve editor notify stuff

/** Implement Curve Editor notify interface, so we can back up state before changes and support Undo. */
void WxInterpEd::PreEditCurve(TArray<UObject*> CurvesAboutToChange)
{
	InterpEdTrans->BeginSpecial(TEXT("Curve Edit"));

	// Call Modify on all tracks with keys selected
	for(INT i=0; i<CurvesAboutToChange.Num(); i++)
	{
		// If this keypoint is from an InterpTrack, call Modify on it to back up its state.
		UInterpTrack* Track = Cast<UInterpTrack>( CurvesAboutToChange(i) );
		if(Track)
		{
			Track->Modify();
		}
	}
}

void WxInterpEd::PostEditCurve()
{
	InterpEdTrans->EndSpecial();
}

void WxInterpEd::MovedKey()
{
	// Update interpolation to the current position - but thing may have changed due to fiddling on the curve display.
	SetInterpPosition(Interp->Position);
}

void WxInterpEd::DesireUndo()
{
	InterpEdUndo();
}

void WxInterpEd::DesireRedo()
{
	InterpEdRedo();
}
