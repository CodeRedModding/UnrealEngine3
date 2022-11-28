/*=============================================================================
	StaticMeshEditor.cpp: StaticMesh editor implementation.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "UnrealEd.h"
#include "..\..\Launch\Resources\resource.h"


IMPLEMENT_CLASS(UStaticMeshEditorComponent);

//
//	UStaticMeshEditorComponent::Render
//

void UStaticMeshEditorComponent::Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	Super::Render(Context,PRI);

	if(StaticMeshEditor && StaticMeshEditor->DrawOpenEdges)
	{
		UStaticMesh*	StaticMesh = StaticMeshEditor->StaticMesh;
		for(UINT EdgeIndex = 0;EdgeIndex < (UINT)StaticMesh->Edges.Num();EdgeIndex++)
		{
			FMeshEdge&	Edge = StaticMesh->Edges(EdgeIndex);
			if(Edge.Faces[1] != INDEX_NONE && StaticMesh->ShadowTriangleDoubleSided(Edge.Faces[0]))
				PRI->DrawLine(
					LocalToWorld.TransformFVector( StaticMesh->Vertices(Edge.Vertices[0]).Position ),
					LocalToWorld.TransformFVector( StaticMesh->Vertices(Edge.Vertices[1]).Position ),
					FColor(128,0,0)
					);
		}
	}
}

//
//	UStaticMeshEditorComponent::GetLayerMask
//

DWORD UStaticMeshEditorComponent::GetLayerMask(const FSceneContext& Context) const
{
	if(StaticMeshEditor && StaticMeshEditor->DrawOpenEdges)
		return PLM_Foreground | PLM_Opaque | Super::GetLayerMask(Context);
	else
		return Super::GetLayerMask(Context);
}

//
//	UStaticMeshEditorComponent::RenderForeground
//

void UStaticMeshEditorComponent::RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	Super::RenderForeground(Context,PRI);

	if(StaticMeshEditor && StaticMeshEditor->DrawOpenEdges)
	{
		UStaticMesh*	StaticMesh = StaticMeshEditor->StaticMesh;
		for(UINT EdgeIndex = 0;EdgeIndex < (UINT)StaticMesh->Edges.Num();EdgeIndex++)
		{
			FMeshEdge&	Edge = StaticMesh->Edges(EdgeIndex);
			if(Edge.Faces[1] == INDEX_NONE)
				PRI->DrawLine(
					LocalToWorld.TransformFVector( StaticMesh->Vertices(Edge.Vertices[0]).Position ),
					LocalToWorld.TransformFVector( StaticMesh->Vertices(Edge.Vertices[1]).Position ),
					FColor(255,255,0)
					);
		}
	}
}

//
//	FStaticMeshEditorViewportClient::FStaticMeshEditorViewportClient
//

FStaticMeshEditorViewportClient::FStaticMeshEditorViewportClient( WxStaticMeshEditor* InStaticMeshEditor ):
	StaticMeshEditor(InStaticMeshEditor)
{
	StaticMeshComponent = ConstructObject<UStaticMeshEditorComponent>(UStaticMeshEditorComponent::StaticClass());
	StaticMeshComponent->StaticMeshEditor = InStaticMeshEditor;
	StaticMeshComponent->StaticMesh = InStaticMeshEditor->StaticMesh;
	StaticMeshComponent->Scene = this;
	StaticMeshComponent->SetParentToWorld(FMatrix::Identity);
	if( StaticMeshComponent->IsValidComponent() )
	{
		StaticMeshComponent->Created();
		Components.AddItem(StaticMeshComponent);
	}

	ViewLocation = -FVector(0,StaticMeshEditor->StaticMesh->Bounds.SphereRadius / (75.0f * (FLOAT)PI / 360.0f),0);
	ViewRotation = FRotator(0,16384,0);

	LockLocation = FVector(0,0,0);
	LockRot = StaticMeshEditor->StaticMesh->ThumbnailAngle;

	FMatrix LockMatrix = FRotationMatrix( FRotator(0,LockRot.Yaw,0) ) * FRotationMatrix( FRotator(0,0,LockRot.Pitch) );

	StaticMeshComponent->SetParentToWorld( FTranslationMatrix(-StaticMeshEditor->StaticMesh->Bounds.Origin) * LockMatrix );
	if( StaticMeshComponent->IsValidComponent() )
	{
		StaticMeshComponent->Update();
	}

	bLock = 1;
	bDrawAxes = 0;
}

//
//	FStaticMeshEditorViewportClient::GetBackgroundColor
//

FColor FStaticMeshEditorViewportClient::GetBackgroundColor()
{
	return FColor(64,64,64);
}

//
//	FStaticMeshEditorViewportClient::Draw
//

void FStaticMeshEditorViewportClient::Draw(FChildViewport* Viewport,FRenderInterface* RI)
{
	FEditorLevelViewportClient::Draw( Viewport, RI );

	RI->DrawShadowedString(
		6,
		6,
		*FString::Printf(TEXT("Triangles:  %u"),StaticMeshEditor->StaticMesh->IndexBuffer.Indices.Num() / 3),
		GEngine->SmallFont,
		FLinearColor::White
		);

	RI->DrawShadowedString(
		6,
		24,
		*FString::Printf(TEXT("Open edges: %u"),StaticMeshEditor->NumOpenEdges),
		GEngine->SmallFont,
		FLinearColor::White
		);

	RI->DrawShadowedString(
		6,
		42,
		*FString::Printf(TEXT("Double-sided shadow triangles:  %u"),StaticMeshEditor->NumDoubleSidedShadowTriangles),
		GEngine->SmallFont,
		FLinearColor::White
		);

	RI->DrawShadowedString(
		6,
		60,
		*FString::Printf(TEXT("UV channels:  %u"),StaticMeshEditor->StaticMesh->UVBuffers.Num()),
		GEngine->SmallFont,
		FLinearColor::White
		);

	FStaticMeshEditorViewportClient * vp    = (FStaticMeshEditorViewportClient*)Viewport->ViewportClient;
    FRotator                          rot   = FRotator( vp->LockRot.Pitch, vp->LockRot.Yaw * -1, vp->LockRot.Roll );
	if( vp->bLock )
	{
		vp->DrawAxes( Viewport, RI, &rot );
	}
}

void FStaticMeshEditorViewportClient::InputAxis(FChildViewport* Viewport,FName Key,FLOAT Delta,FLOAT DeltaTime)
{
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

			if( bLock )
			{
				LockRot += FRotator( Rot.Pitch, -Rot.Yaw, Rot.Roll );
				LockLocation.Y -= Drag.Y;

				FMatrix LockMatrix = FRotationMatrix( FRotator(0,LockRot.Yaw,0) ) * FRotationMatrix( FRotator(0,0,LockRot.Pitch) ) * FTranslationMatrix( LockLocation );

				StaticMeshComponent->SetParentToWorld( FTranslationMatrix(-StaticMeshEditor->StaticMesh->Bounds.Origin) * LockMatrix );
				if( StaticMeshComponent->IsValidComponent() )
				{
					StaticMeshComponent->Update();
				}
			}
			else
			{
				MoveViewportCamera( Drag, Rot );
			}

			MouseDeltaTracker.ReduceBy( DragDelta );
		}
	}

	Viewport->Invalidate();
}

/*-----------------------------------------------------------------------------
	WxStaticMeshEditor
-----------------------------------------------------------------------------*/

IMPLEMENT_DYNAMIC_CLASS(WxStaticMeshEditor,wxFrame)

BEGIN_EVENT_TABLE( WxStaticMeshEditor, wxFrame )
	EVT_SIZE( WxStaticMeshEditor::OnSize )
	EVT_PAINT( WxStaticMeshEditor::OnPaint )
	EVT_UPDATE_UI( ID_SHOW_OPEN_EDGES, WxStaticMeshEditor::UI_ShowEdges )
	EVT_UPDATE_UI( ID_SHOW_WIREFRAME, WxStaticMeshEditor::UI_ShowWireframe )
	EVT_UPDATE_UI( ID_SHOW_BOUNDS, WxStaticMeshEditor::UI_ShowBounds )
	EVT_UPDATE_UI( IDMN_COLLISION, WxStaticMeshEditor::UI_ShowCollision )
	EVT_UPDATE_UI( ID_LOCK_CAMERA, WxStaticMeshEditor::UI_LockCamera )
	EVT_MENU( ID_SHOW_OPEN_EDGES, WxStaticMeshEditor::OnShowEdges )
	EVT_MENU( ID_SHOW_WIREFRAME, WxStaticMeshEditor::OnShowWireframe )
	EVT_MENU( ID_SHOW_BOUNDS, WxStaticMeshEditor::OnShowBounds )
	EVT_MENU( IDMN_COLLISION, WxStaticMeshEditor::OnShowCollision )
	EVT_MENU( ID_LOCK_CAMERA, WxStaticMeshEditor::OnLockCamera )
	EVT_MENU( ID_SAVE_THUMBNAIL, WxStaticMeshEditor::OnSaveThumbnailAngle )
	EVT_MENU( IDMN_SME_COLLISION_6DOP, WxStaticMeshEditor::OnCollision6DOP )
	EVT_MENU( IDMN_SME_COLLISION_10DOPX, WxStaticMeshEditor::OnCollision10DOPX )
	EVT_MENU( IDMN_SME_COLLISION_10DOPY, WxStaticMeshEditor::OnCollision10DOPY )
	EVT_MENU( IDMN_SME_COLLISION_10DOPZ, WxStaticMeshEditor::OnCollision10DOPZ )
	EVT_MENU( IDMN_SME_COLLISION_18DOP, WxStaticMeshEditor::OnCollision18DOP )
	EVT_MENU( IDMN_SME_COLLISION_26DOP, WxStaticMeshEditor::OnCollision26DOP )
	EVT_MENU( IDMN_SME_COLLISION_SPHERE, WxStaticMeshEditor::OnCollisionSphere )
END_EVENT_TABLE()

WxStaticMeshEditor::WxStaticMeshEditor( wxWindow* Parent, wxWindowID id, UStaticMesh* InStaticMesh )
	: wxFrame( Parent, id, TEXT("Static Mesh Editor"), wxDefaultPosition, wxDefaultSize, wxFRAME_FLOAT_ON_PARENT | wxDEFAULT_FRAME_STYLE | wxFRAME_NO_TASKBAR )
	,	DrawOpenEdges(0)
	,	NumOpenEdges(0)
	,	NumDoubleSidedShadowTriangles(0)
{
	StaticMesh = InStaticMesh;

	// Count the number of open edges and double sided shadow triangles in the static mesh.

	for( INT EdgeIndex = 0 ; EdgeIndex < StaticMesh->Edges.Num() ; EdgeIndex++ )
	{
		if( StaticMesh->Edges(EdgeIndex).Faces[1] == INDEX_NONE )
		{
			NumOpenEdges++;
		}
	}

	for( INT TriangleIndex = 0 ; TriangleIndex < StaticMesh->ShadowTriangleDoubleSided.Num() ; TriangleIndex++ )
	{
		if( StaticMesh->ShadowTriangleDoubleSided(TriangleIndex) )
		{
			NumDoubleSidedShadowTriangles++;
		}
	}

	SplitterWnd = new wxSplitterWindow( this, ID_SPLITTERWINDOW, wxDefaultPosition, wxSize(100, 100), wxSP_3D|wxSP_3DBORDER|wxSP_FULLSASH );

	// Create property window

	PropertyWindow = new WxPropertyWindow( SplitterWnd, NULL );
	PropertyWindow->SetObject( StaticMesh, 1,1,0 );

	// Create viewport.

	ViewportHolder = new EViewportHolder( SplitterWnd, -1, 0 );
	ViewportClient = new FStaticMeshEditorViewportClient( this );
	ViewportClient->Viewport = GEngine->Client->CreateWindowChildViewport( ViewportClient, (HWND)ViewportHolder->GetHandle() );
	ViewportClient->Viewport->CaptureJoystickInput(false);
	ViewportHolder->SetViewport( ViewportClient->Viewport );
	ViewportHolder->Show();

	FWindowUtil::LoadPosSize( TEXT("StaticMeshEditor"), this, 64,64,800,450 );

	wxRect rc = GetClientRect();

	SplitterWnd->SplitVertically( ViewportHolder, PropertyWindow, rc.GetWidth() - 350 );

	ToolBar = new WxStaticMeshEditorBar( this, -1 );
	SetToolBar( ToolBar );

	MenuBar = new WxStaticMeshEditMenu();
	SetMenuBar( MenuBar );
}

WxStaticMeshEditor::~WxStaticMeshEditor()
{
	FWindowUtil::SavePosSize( TEXT("StaticMeshEditor"), this );

	GEngine->Client->CloseViewport( ViewportClient->Viewport );
	ViewportClient->Viewport = NULL;
	delete ViewportClient;

	PropertyWindow->SetObject( NULL, 0,0,0 );
	delete PropertyWindow;
}

void WxStaticMeshEditor::OnSize( wxSizeEvent& In )
{
	wxPoint origin = GetClientAreaOrigin();
	wxRect rc = GetClientRect();
	rc.y -= origin.y;

	SplitterWnd->SetSize( rc );
}

void WxStaticMeshEditor::Serialize(FArchive& Ar)
{
	Ar << StaticMesh;
	check(ViewportClient);
	ViewportClient->Serialize(Ar);
}

void WxStaticMeshEditor::OnPaint( wxPaintEvent& In )
{
	wxPaintDC dc( this );

	ViewportClient->Viewport->Invalidate();
}

void WxStaticMeshEditor::UI_ShowEdges( wxUpdateUIEvent& In )
{
    In.Check( DrawOpenEdges ? true : false );
}

void WxStaticMeshEditor::UI_ShowWireframe( wxUpdateUIEvent& In )
{
	In.Check( ViewportClient->ViewMode == SVM_Wireframe );
}

void WxStaticMeshEditor::UI_ShowBounds( wxUpdateUIEvent& In )
{
    In.Check( ( ViewportClient->ShowFlags & SHOW_Bounds ) ? true : false );
}

void WxStaticMeshEditor::UI_ShowCollision( wxUpdateUIEvent& In )
{
    In.Check( ( ViewportClient->ShowFlags & SHOW_Collision ) ? true : false );
}

void WxStaticMeshEditor::UI_LockCamera( wxUpdateUIEvent& In )
{
    In.Check( ViewportClient->bLock ? true : false );
}

void WxStaticMeshEditor::OnShowEdges( wxCommandEvent& In )
{
	DrawOpenEdges = !DrawOpenEdges;
	ViewportClient->Viewport->Invalidate();
}

void WxStaticMeshEditor::OnShowWireframe( wxCommandEvent& In )
{
	if(ViewportClient->ViewMode == SVM_Wireframe)
		ViewportClient->ViewMode = SVM_Lit;
	else
		ViewportClient->ViewMode = SVM_Wireframe;
	ViewportClient->Viewport->Invalidate();
}

void WxStaticMeshEditor::OnShowBounds( wxCommandEvent& In )
{
	ViewportClient->ShowFlags ^= SHOW_Bounds;
	ViewportClient->Viewport->Invalidate();
}

void WxStaticMeshEditor::OnShowCollision( wxCommandEvent& In )
{
	ViewportClient->ShowFlags ^= SHOW_Collision;
	ViewportClient->Viewport->Invalidate();
}

void WxStaticMeshEditor::OnLockCamera( wxCommandEvent& In )
{
	ViewportClient->bLock = !ViewportClient->bLock;
	ViewportClient->Viewport->Invalidate();

	if( ViewportClient->bLock )
	{
		ViewportClient->ViewLocation = -FVector(0,StaticMesh->Bounds.SphereRadius / (75.0f * (FLOAT)PI / 360.0f),0);
		ViewportClient->ViewRotation = FRotator(0,16384,0);

		ViewportClient->LockLocation = FVector(0,0,0);
		ViewportClient->LockRot = StaticMesh->ThumbnailAngle;

		FMatrix LockMatrix = FRotationMatrix( FRotator(0,ViewportClient->LockRot.Yaw,0) ) * FRotationMatrix( FRotator(0,0,ViewportClient->LockRot.Pitch) );

		ViewportClient->StaticMeshComponent->SetParentToWorld( FTranslationMatrix(-StaticMesh->Bounds.Origin) * LockMatrix );
		if( ViewportClient->StaticMeshComponent->IsValidComponent() )
		{
			ViewportClient->StaticMeshComponent->Update();
		}
	}
}

void WxStaticMeshEditor::OnSaveThumbnailAngle( wxCommandEvent& In )
{
	ViewportClient->StaticMeshComponent->StaticMesh->ThumbnailAngle = ViewportClient->LockRot;
	ViewportClient->StaticMeshComponent->StaticMesh->MarkPackageDirty();
	GCallback->Send( CALLBACK_RefreshEditor, ERefreshEditor_GenericBrowser );
}

void WxStaticMeshEditor::OnCollision6DOP( wxCommandEvent& In )
{
	GenerateKDop(KDopDir6,6);
}

void WxStaticMeshEditor::OnCollision10DOPX( wxCommandEvent& In )
{
	GenerateKDop(KDopDir10X,10);
}

void WxStaticMeshEditor::OnCollision10DOPY( wxCommandEvent& In )
{
	GenerateKDop(KDopDir10Y,10);
}

void WxStaticMeshEditor::OnCollision10DOPZ( wxCommandEvent& In )
{
	GenerateKDop(KDopDir10Z,10);
}

void WxStaticMeshEditor::OnCollision18DOP( wxCommandEvent& In )
{
	GenerateKDop(KDopDir18,18);
}

void WxStaticMeshEditor::OnCollision26DOP( wxCommandEvent& In )
{
	GenerateKDop(KDopDir26,26);
}

void WxStaticMeshEditor::OnCollisionSphere( wxCommandEvent& In )
{
	GenerateSphere();
}

void WxStaticMeshEditor::GenerateKDop(const FVector* Directions,UINT NumDirections)
{
	TArray<FVector>	DirArray;

	for(UINT DirectionIndex = 0;DirectionIndex < NumDirections;DirectionIndex++)
		DirArray.AddItem(Directions[DirectionIndex]);

	GenerateKDopAsCollisionModel( StaticMesh, DirArray );

	ViewportClient->Viewport->Invalidate();
	PropertyWindow->SetObject( StaticMesh, 1,1,0 );
}

void WxStaticMeshEditor::GenerateSphere()
{
	GenerateSphereAsKarmaCollision( StaticMesh );

	ViewportClient->Viewport->Invalidate();
	PropertyWindow->SetObject( StaticMesh, 1,1,0 );
}
