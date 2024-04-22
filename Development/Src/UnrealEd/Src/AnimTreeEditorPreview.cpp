/*=============================================================================
	AnimTreeEditorPreview.cpp: AnimTree Editor Preview Window
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnLinkedObjEditor.h"
#include "AnimTreeEditor.h"
#include "EngineAnimClasses.h"
#include "MouseDeltaTracker.h"

static const FLOAT	AnimTreeEditor_TranslateSpeed = 0.25f;
static const FLOAT	AnimTreeEditor_RotateSpeed = 0.02f;
static const FLOAT	AnimTreeEditor_FloorDragSpeed = 0.25f;
static const FLOAT	AnimTreeEditor_FloorTurnSpeed = 0.005f;
static const FLOAT	AnimTreeEditor_LightRotSpeed = 40.0f;

/*-----------------------------------------------------------------------------
	FAnimTreeEdPreviewVC
-----------------------------------------------------------------------------*/

FAnimTreeEdPreviewVC::FAnimTreeEdPreviewVC( WxAnimTreeEditor* InAnimTreeEd )
{
	AnimTreeEd = InAnimTreeEd;

	// Setup defaults for the common draw helper.
	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.bDrawKillZ = false;
	DrawHelper.GridColorHi = FColor(80,80,80);
	DrawHelper.GridColorLo = FColor(72,72,72);
	DrawHelper.PerspectiveGridSize = 32767;

	// Create SkeletalMeshComponent for rendering skeletal mesh
	InAnimTreeEd->PreviewSkelComp = ConstructObject<UAnimTreeEdSkelComponent>(UAnimTreeEdSkelComponent::StaticClass());
// 	InAnimTreeEd->PreviewSkelComp = ConstructObject<UAnimTreeEdSkelComponent>(UAnimTreeEdSkelComponent::StaticClass(),UObject::GetTransientPackage(),NAME_None,RF_Transient);
	InAnimTreeEd->PreviewSkelComp->AnimTreeEdPtr = InAnimTreeEd;
	PreviewScene.AddComponent(InAnimTreeEd->PreviewSkelComp,FMatrix::Identity);

	// Get initial position for preview floor from AnimTree.
	FRotationTranslationMatrix FloorTM( FRotator(0, AnimTreeEd->AnimTree->PreviewFloorYaw, 0), AnimTreeEd->AnimTree->PreviewFloorPos );

	InAnimTreeEd->FloorComp = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass());
	InAnimTreeEd->FloorComp->StaticMesh = LoadObject<UStaticMesh>(NULL, TEXT("EditorMeshes.AnimTreeEd_PreviewFloor"), NULL, LOAD_None, NULL);
	PreviewScene.AddComponent(InAnimTreeEd->FloorComp,FMatrix::Identity);

	ShowFlags = SHOW_DefaultEditor;

	// Set the viewport to be fully lit.
	ShowFlags &= ~SHOW_ViewMode_Mask;
	ShowFlags |= SHOW_ViewMode_Lit;

	bManipulating = FALSE;
	bDrawingInfoWidget = FALSE;
	ManipulateAxis = AXIS_None;
	ManipulateWidgetIndex = INDEX_NONE;

	DragDirX = 0.f;
	DragDirY = 0.f;
	WorldManDir = FVector(0.f);
	LocalManDir = FVector(0.f);

	NearPlane = 1.0f;

	SetRealtime( TRUE );
	bAllowMayaCam = TRUE;
}

void FAnimTreeEdPreviewVC::Serialize(FArchive& Ar)
{ 
	Ar << Input; 
	Ar << PreviewScene;
}

FLinearColor FAnimTreeEdPreviewVC::GetBackgroundColor()
{
	return FColor(64,64,64);
}

UBOOL FAnimTreeEdPreviewVC::InputKey(FViewport* Viewport, INT ControllerId, FName Key, EInputEvent Event,FLOAT AmountDepressed,UBOOL Gamepad)
{
	// Hide and lock mouse cursor if we're capturing mouse input
	Viewport->ShowCursor( !Viewport->HasMouseCapture() );
	Viewport->LockMouseToWindow( Viewport->HasMouseCapture() );
	
	const INT HitX = Viewport->GetMouseX();
	const INT HitY = Viewport->GetMouseY();

	if(Key == KEY_LeftMouseButton)
	{
		if(Event == IE_Pressed)
		{
			HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);
			
			if(HitResult)
			{
				if(HitResult->IsA(HWidgetUtilProxy::StaticGetType()))
				{
					HWidgetUtilProxy* WidgetProxy = (HWidgetUtilProxy*)HitResult;

					ManipulateAxis = WidgetProxy->Axis;
					ManipulateWidgetIndex = WidgetProxy->Info1;

					// Calculate the scree-space directions for this drag.
					FSceneViewFamilyContext ViewFamily(Viewport,GetScene(),ShowFlags,GWorld->GetTimeSeconds(),GWorld->GetDeltaSeconds(),GWorld->GetRealTimeSeconds());
					FSceneView* View = CalcSceneView(&ViewFamily);
					WidgetProxy->CalcVectors(View, FViewportClick(View, this, Key, Event, HitX, HitY), LocalManDir, WorldManDir, DragDirX, DragDirY);

					bManipulating = true;
				}
			}
		}
		else if(Event == IE_Released)
		{
			if( bManipulating )
			{
				ManipulateAxis = AXIS_None;
				bManipulating = false;
			}
		}
	}

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
UBOOL FAnimTreeEdPreviewVC::InputAxis(FViewport* Viewport, INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad)
{
	// Get some useful info about buttons being held down
	const UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	const UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);
	const UBOOL bLightMoveDown = Viewport->KeyState(KEY_L);

	// Look at which axis is being dragged and by how much
	const FLOAT DragX = (Key == KEY_MouseX) ? Delta : 0.f;
	const FLOAT DragY = (Key == KEY_MouseY) ? Delta : 0.f;

	// If Ctrl is held, we are in 'floor moving' mode.
	if( bCtrlDown )
	{
		// Don't update the floor if its not being drawn. Very confusing.
		if(AnimTreeEd->bShowFloor)
		{
			FMatrix NewLocalToWorld = AnimTreeEd->FloorComp->LocalToWorld;

			// LMB and LMB+RMB are translation
			if(Viewport->KeyState(KEY_LeftMouseButton))
			{
				FVector NewOrigin = NewLocalToWorld.GetOrigin();
				if(Viewport->KeyState(KEY_RightMouseButton))
				{
					// Translate up/down
					NewOrigin += AnimTreeEditor_FloorDragSpeed * FVector(0.f, -0.f, DragY);
				}
				else
				{
					const FRotator CamYaw( 0, ViewRotation.Yaw, 0 );
					const FRotationMatrix CamToWorld( CamYaw );

					// Translate around in X/Y plane. We drag relative to the camera look direction.
					const FVector WorldSpaceDrag = CamToWorld.TransformNormal( FVector(DragY, DragX, 0.f) );

					NewOrigin += AnimTreeEditor_FloorDragSpeed * WorldSpaceDrag;
				}

				NewLocalToWorld.SetOrigin( NewOrigin );
			}
			// Just RMB is rotate.
			else if(Viewport->KeyState(KEY_RightMouseButton))
			{
				const FQuat RotQuat( FVector(0,0,1), AnimTreeEditor_FloorTurnSpeed * DragX );
				const FQuatRotationTranslationMatrix RotTM( RotQuat, FVector(0.f) );
				NewLocalToWorld = NewLocalToWorld * RotTM;
			}

			// Update component to its new LocalToWorld
			AnimTreeEd->FloorComp->ConditionalUpdateTransform(NewLocalToWorld);
		}
	}
	else if(bLightMoveDown)
	{
		FRotator LightDir = PreviewScene.GetLightDirection();

		LightDir.Yaw += -DragX * AnimTreeEditor_LightRotSpeed;
		LightDir.Pitch += -DragY * AnimTreeEditor_LightRotSpeed;

		PreviewScene.SetLightDirection(LightDir);
	}
	else if(bManipulating)
	{
		const FLOAT DragMag = (DragX * DragDirX) + (DragY * DragDirY);

		if(bDrawingInfoWidget)
		{
			FQuat DeltaWorldQuat( WorldManDir, -DragMag * AnimTreeEditor_RotateSpeed );
			FVector DeltaWorldTranslate = WorldManDir * DragMag * AnimTreeEditor_TranslateSpeed;

			UBOOL bCalledHandler = FALSE;
			for(INT i=0; i<AnimTreeEd->AnimNodeEditInfos.Num() && !bCalledHandler; i++)
			{
				UAnimNodeEditInfo* EditInfo = AnimTreeEd->AnimNodeEditInfos(i);
				if( EditInfo->ShouldDrawWidget() )
				{
					EditInfo->HandleWidgetDrag(DeltaWorldQuat, DeltaWorldTranslate);
					bCalledHandler = TRUE;
				}
			}			
		}
		else
		{
			FVector DeltaLocalTranslate = LocalManDir * DragMag * AnimTreeEditor_TranslateSpeed;

#if !FINAL_RELEASE
			TArray<USkelControlBase *> SkelControls;
			AnimTreeEd->GetSelectedNodeByClass<USkelControlBase>(SkelControls);

			check(SkelControls.Num() == 1); // Shouldn't draw axes unless this is the case
#endif
			USkelControlBase* Control = AnimTreeEd->GetFirstSelectedNodeByClass<USkelControlBase>();
			Control->HandleWidgetDrag( ManipulateWidgetIndex, DeltaLocalTranslate );
			AnimTreeEd->AnimTree->MarkPackageDirty();
		}
	}
	// If we are not manipulating an axis, use the MouseDeltaTracker to update camera.
	else
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
	}

	Viewport->Invalidate();

	return TRUE;
}

void FAnimTreeEdPreviewVC::MouseMove(FViewport* Viewport,INT x, INT y)
{
	// If we are not currently moving the widget - update the ManipulateAxis to the one we are mousing over.
	if(!bManipulating)
	{
		const INT	HitX = Viewport->GetMouseX();
		const INT HitY = Viewport->GetMouseY();

		HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);
		if(HitResult && HitResult->IsA(HWidgetUtilProxy::StaticGetType()))
		{
			HWidgetUtilProxy* WidgetProxy = (HWidgetUtilProxy*)HitResult;
			ManipulateAxis = WidgetProxy->Axis;
			ManipulateWidgetIndex = WidgetProxy->Info1;
		}
		else 
		{
			ManipulateAxis = AXIS_None;
		}
	}
}

void FAnimTreeEdPreviewVC::Draw(FViewport* Viewport, FCanvas* Canvas)
{
	// Remove temporary debug lines.
	PreviewScene.ClearLineBatcher();

	// Hide/show the floor StaticMeshComponent as desired before rendering.
	AnimTreeEd->FloorComp->SetHiddenEditor(!AnimTreeEd->bShowFloor);

	// Do main viewport drawing stuff.
	FEditorLevelViewportClient::Draw(Viewport, Canvas);

	if( AnimTreeEd->bShowBoneNames )
	{
		FSceneViewFamilyContext ViewFamily(Viewport,GetScene(),ShowFlags,GWorld->GetTimeSeconds(),GWorld->GetDeltaSeconds(),GWorld->GetRealTimeSeconds());
		FSceneView* View = CalcSceneView(&ViewFamily);

		USkeletalMesh* SkelMesh = AnimTreeEd->PreviewSkelComp->SkeletalMesh;

		for(INT i=0; i<AnimTreeEd->PreviewSkelComp->SpaceBases.Num(); i++)
		{
			const INT	BoneIndex = i;

			FColor& BoneColor = SkelMesh->RefSkeleton(BoneIndex).BoneColor;
			if( BoneColor.A != 0 )
			{
				FVector BonePos = AnimTreeEd->PreviewSkelComp->LocalToWorld.TransformFVector(AnimTreeEd->PreviewSkelComp->SpaceBases(i).GetOrigin());

				FPlane proj = View->Project( BonePos );
				if(proj.W > 0.f) // This avoids drawing bone names that are behind us.
				{
					const INT HalfX = Viewport->GetSizeX()/2;
					const INT HalfY = Viewport->GetSizeY()/2;
					const INT XPos = HalfX + ( HalfX * proj.X );
					const INT YPos = HalfY + ( HalfY * (proj.Y * -1) );

					const FName BoneName = SkelMesh->RefSkeleton(i).Name;

					//if(bHitTesting) Canvas->SetHitProxy( new HPhATBoneNameProxy(i) );
					DrawString(Canvas,XPos, YPos, *BoneName.ToString(), GEngine->SmallFont, BoneColor);
					//if(bHitTesting) Canvas->SetHitProxy( NULL );
				}
			}
		}
	}

	// Morph node canvas drawing
	TArray<UMorphNodeBase *> MorphNodes;
	AnimTreeEd->GetSelectedNodeByClass<UMorphNodeBase>(MorphNodes);
	if( MorphNodes.Num() == 1 )
	{
		UMorphNodeBase* SelectedMorphNode = MorphNodes(0);
		if( SelectedMorphNode )
		{
			FSceneViewFamilyContext ViewFamily(Viewport, GetScene(), ShowFlags, GWorld->GetTimeSeconds(), GWorld->GetDeltaSeconds(), GWorld->GetRealTimeSeconds());
			FSceneView* View = CalcSceneView(&ViewFamily);

			SelectedMorphNode->Draw(Viewport, Canvas, View);
		}
	}
}

void FAnimTreeEdPreviewVC::Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	// Set wireframe mode for the skeletal mesh component.
	AnimTreeEd->PreviewSkelComp->SetForceWireframe(AnimTreeEd->bShowWireframe);

	// Render common scene elements before the preview.
	DrawHelper.Draw(View, PDI);

	// Render the preview component.
	AnimTreeEd->PreviewSkelComp->Render(View,PDI);
}

void FAnimTreeEdPreviewVC::Tick(FLOAT DeltaSeconds)
{
	FEditorLevelViewportClient::Tick(DeltaSeconds);

	AnimTreeEd->TickPreview(DeltaSeconds);
}

/*-----------------------------------------------------------------------------
	WxAnimTreePreview
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxAnimTreePreview, wxWindow )
	EVT_SIZE( WxAnimTreePreview::OnSize )
END_EVENT_TABLE()

WxAnimTreePreview::WxAnimTreePreview( wxWindow* InParent, wxWindowID InID, class WxAnimTreeEditor* InAnimTreeEd, FVector& InViewLocation, FRotator& InViewRotation  )
: wxWindow( InParent, InID )
{
	AnimTreePreviewVC = new FAnimTreeEdPreviewVC(InAnimTreeEd);
	AnimTreePreviewVC->Viewport = GEngine->Client->CreateWindowChildViewport(AnimTreePreviewVC, (HWND)GetHandle());
	AnimTreePreviewVC->Viewport->CaptureJoystickInput(false);

	AnimTreePreviewVC->ViewLocation = InViewLocation;
	AnimTreePreviewVC->ViewRotation = InViewRotation;
	AnimTreePreviewVC->SetViewLocationForOrbiting( FVector(0.f,0.f,0.f) );
}

WxAnimTreePreview::~WxAnimTreePreview()
{
	GEngine->Client->CloseViewport(AnimTreePreviewVC->Viewport);
	AnimTreePreviewVC->Viewport = NULL;
	delete AnimTreePreviewVC;
}

void WxAnimTreePreview::OnSize( wxSizeEvent& In )
{
	const wxRect rc = GetClientRect();
	::MoveWindow( (HWND)AnimTreePreviewVC->Viewport->GetWindow(), 0, 0, rc.GetWidth(), rc.GetHeight(), 1 );
}


/*-----------------------------------------------------------------------------
	UAnimTreeEdSkelComponent
-----------------------------------------------------------------------------*/

void UAnimTreeEdSkelComponent::Render(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	// Get the AnimTreeEditor itself.
	WxAnimTreeEditor* AnimTreeEd = (WxAnimTreeEditor*)AnimTreeEdPtr;
	check(AnimTreeEd);

	// If we are drawing the skeleton, draw lines between each bone and its parent.
	if( AnimTreeEd->bShowSkeleton && SkeletalMesh != NULL )
	{
		FStaticLODModel& LODModel = SkeletalMesh->LODModels(PredictedLODLevel);

		for(INT i=0; i<LODModel.RequiredBones.Num(); i++)
		{
			const INT BoneIndex	= LODModel.RequiredBones(i);

			const FVector	LocalBonePos	= SpaceBases(BoneIndex).GetOrigin();
			const FVector	BonePos			= LocalToWorld.TransformFVector(LocalBonePos);

			FColor& BoneColor = SkeletalMesh->RefSkeleton(BoneIndex).BoneColor;
			if( BoneColor.A != 0 )
			{
				if( BoneIndex == 0 )
				{
					PDI->DrawLine(BonePos, LocalToWorld.GetOrigin(), BoneColor, SDPG_Foreground);
				}
				else
				{
					// Link Parent to Bone
					const INT		ParentIndex		= SkeletalMesh->RefSkeleton(BoneIndex).ParentIndex;
					const FVector	ParentPos		= LocalToWorld.TransformFVector( SpaceBases(ParentIndex).GetOrigin() );
					PDI->DrawLine(ParentPos, BonePos, BoneColor, SDPG_Foreground);
				}

				// Draw coord system at each bone
				PDI->DrawLine( BonePos, LocalToWorld.TransformFVector( LocalBonePos + 3.75f * SpaceBases(BoneIndex).GetAxis(0) ), FColor(255,0,0), SDPG_Foreground );
				PDI->DrawLine( BonePos, LocalToWorld.TransformFVector( LocalBonePos + 3.75f * SpaceBases(BoneIndex).GetAxis(1) ), FColor(0,255,0), SDPG_Foreground );
				PDI->DrawLine( BonePos, LocalToWorld.TransformFVector( LocalBonePos + 3.75f * SpaceBases(BoneIndex).GetAxis(2) ), FColor(0,0,255), SDPG_Foreground );
			}
		}
	}

	AnimTreeEd->PreviewVC->bDrawingInfoWidget = FALSE;

	for(INT i=0; i<AnimTreeEd->AnimNodeEditInfos.Num() && !AnimTreeEd->PreviewVC->bDrawingInfoWidget; i++)
	{
		UAnimNodeEditInfo* EditInfo = AnimTreeEd->AnimNodeEditInfos(i);
		EditInfo->Draw3DInfo(View, PDI);

		if( EditInfo->ShouldDrawWidget() )
		{
			const FMatrix WidgetTM = EditInfo->GetWidgetTM();
			UBOOL bRotWidget = EditInfo->IsRotationWidget();

			EAxis HighlightAxis = AXIS_None;
			if(AnimTreeEd->PreviewVC->ManipulateWidgetIndex == 0)
			{
				HighlightAxis = AnimTreeEd->PreviewVC->ManipulateAxis;
			}

			FUnrealEdUtils::DrawWidget(View, PDI, WidgetTM, 0, 0, HighlightAxis, bRotWidget ? WMM_Rotate : WMM_Translate);

			AnimTreeEd->PreviewVC->bDrawingInfoWidget = TRUE;
		}
	}

	// Morph node viewport rendering
	TArray<UMorphNodeBase *> MorphNodes;
	AnimTreeEd->GetSelectedNodeByClass<UMorphNodeBase>(MorphNodes);

	if( !AnimTreeEd->PreviewVC->bDrawingInfoWidget && 
		MorphNodes.Num() == 1 )
	{
		UMorphNodeBase* SelectedMorphNode = MorphNodes(0);
		if( SelectedMorphNode )
		{
			SelectedMorphNode->Render(View, PDI);
		}
	}

	TArray<USkelControlBase *> SkelControls;
	AnimTreeEd->GetSelectedNodeByClass<USkelControlBase>(SkelControls);

	// If a SkelControl is only thing selected, see if it wants a Widget drawn.
	if(	!AnimTreeEd->PreviewVC->bDrawingInfoWidget && 
		SkelControls.Num() == 1 && 
		AnimTreeEd->GetNumSelectedByClass<UAnimNode>() == 0 )
	{
		// Because a SkelControl could be used by multiple chains, we have to draw the widgets for each use in a chain.

		USkelControlBase* SelectedControl = SkelControls(0);

		for(INT i=0; i<AnimTreeEd->AnimTree->SkelControlLists.Num(); i++)
		{
			const FName BoneName = AnimTreeEd->AnimTree->SkelControlLists(i).BoneName;
			const INT BoneIndex = AnimTreeEd->PreviewSkelComp->MatchRefBone(BoneName);
			if(BoneIndex != INDEX_NONE)
			{
				USkelControlBase* Control = AnimTreeEd->AnimTree->SkelControlLists(i).ControlHead;
				while(Control)
				{
					if(Control == SelectedControl)
					{
						// First, let the SkelControl draw anything it wants.
						Control->DrawSkelControl3D(View, PDI, this, BoneIndex);

						// Then draw the widgets for moving things.
						const INT NumWidgets = Control->GetWidgetCount();
						for(INT WidgetNum = 0; WidgetNum < NumWidgets; WidgetNum++)
						{
							const FBoneAtom WidgetTM = Control->GetWidgetTM(WidgetNum, AnimTreeEd->PreviewSkelComp, BoneIndex);
							
							EAxis HighlightAxis = AXIS_None;
							if(WidgetNum == AnimTreeEd->PreviewVC->ManipulateWidgetIndex)
							{
								HighlightAxis = AnimTreeEd->PreviewVC->ManipulateAxis;
							}

							// Info1 is widget index
							// Info2 is bone index
							FUnrealEdUtils::DrawWidget(View, PDI, WidgetTM.ToMatrix(), WidgetNum, BoneIndex, HighlightAxis, WMM_Translate);
						}
					}

					// Move on to next control.
					Control = Control->NextControl;
				}
			}
		}
	}
}

/** Special version of line-check function used by SkelControlFootPlacement, so we can test just against the 'floor' mesh in the AnimTreeEditor. */
UBOOL UAnimTreeEdSkelComponent::LegLineCheck(const FVector& Start, const FVector& End, FVector& HitLocation, FVector& HitNormal, const FVector& Extent)
{
	// Get the AnimTreeEditor itself.
	WxAnimTreeEditor* AnimTreeEd = (WxAnimTreeEditor*)AnimTreeEdPtr;
	check(AnimTreeEd);

	if(AnimTreeEd->bShowFloor)
	{
		FCheckResult Hit(1.f);
		const UBOOL bHit = !AnimTreeEd->FloorComp->LineCheck(Hit, End, Start, Extent, TRACE_AllBlocking);
		if(bHit)
		{
			HitLocation = Hit.Location;
			HitNormal = Hit.Normal;
			return true;
		}
	}

	return false;
}
