/*=============================================================================
	UnEdViewport.cpp: Unreal editor viewport implementation.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EditorPrivate.h"
#include "EngineSequenceClasses.h"

//
//	UEditorEngine::DisableRealtimeViewports
//

void UEditorEngine::DisableRealtimeViewports()
{
	// Loop through all viewports and disable any realtime viewports viewing the edited level before running the game.
	// This won't disable things like preview viewports in Cascade etc.
	for( INT x = 0 ; x < ViewportClients.Num() ; ++x)
	{
		FEditorLevelViewportClient* VC = ViewportClients(x);
		if(VC && VC->GetLevel() == Level)
		{
			VC->Realtime = 0;
		}
	}

	RedrawAllViewports( 1 );

	GCallback->Send( CALLBACK_UpdateUI );
}

//
//	UEditorEngine::AddActor
//

AActor* UEditorEngine::AddActor( ULevel* Level, UClass* Class, FVector V, UBOOL bSilent )
{
	check(Class);

	if( !bSilent )
	{
		debugf( NAME_Log, TEXT("Attempting to add actor of class '%s' to level at %0.2f,%0.2f,%0.2f"),
			Class->GetName(),
			V.X, V.Y, V.Z );
	}

	// Validate everything.
	if( Class->ClassFlags & CLASS_Abstract )
	{
		warnf( TEXT("Class %s is abstract.  You can't add actors of this class to the world."), Class->GetName() );
		return NULL;
	}
	if( !(Class->ClassFlags & CLASS_Placeable) )
	{
		warnf( TEXT("Class %s isn't placeable.  You can't add actors of this class to the world."), Class->GetName() );
		return NULL;
	}
	else if( Class->ClassFlags & CLASS_Transient )
	{
		warnf( TEXT("Class %s is transient.  You can't add actors of this class in UnrealEd."), Class->GetName() );
		return NULL;
	}

	// Transactionally add the actor.
	Trans->Begin( TEXT("Add Actor") );
	SelectNone( Level, 0 );
	Level->Modify();

	AActor* Default = Class->GetDefaultActor();
	AActor* Actor = Level->SpawnActor( Class, NAME_None, V, Default->Rotation );
	if( Actor )
	{
		// set bDeleteMe to true so that when the initial undo record is made,
		// the actor will be treated as destroyed, in that undo an add will
		// actually work
		Actor->bDeleteMe = 1;
		Actor->Modify();
		Actor->bDeleteMe = 0;

		SelectActor( Level, Actor, 1, 0 );
		if( Actor->IsA(ABrush::StaticClass()) && !Actor->IsA(AShape::StaticClass()) )
			csgCopyBrush( (ABrush*)Actor, (ABrush*)Class->GetDefaultActor(), 0, 0, 1 );
		Actor->PostEditMove();
	}
	else warnf( TEXT("Actor doesn't fit there (couldn't spawn it) -- make sure snap to grid isn't on") );

	Trans->End();

	NoteSelectionChange( Level );

	return Actor;
}

/**
 * Looks for an appropriate actor factory for a UClass.
 *
 * @param	InClass		The class to find the factory for.
 *
 * @return	A pointer to the factory to use.  NULL if no factories support this class.
 */

UActorFactory* UEditorEngine::FindActorFactory( UClass* InClass )
{
	for( INT x = 0 ; x < ActorFactories.Num() ; ++x )
	{
		UActorFactory* Factory = ActorFactories(x);

		if( InClass == Factory->NewActorClass )
		{
			return Factory;
		}
	}

	return NULL;
}

// Use the supplied factory to create an actor at the clicked location and add to level.
AActor* UEditorEngine::UseActorFactory(UActorFactory* Factory)
{
	Factory->AutoFillFields();

	if( !Factory->CanCreateActor() )
		return NULL;

	Constraints.Snap( ClickLocation, FVector(0, 0, 0) );

	AActor* NewActorTemplate = Factory->NewActorClass->GetDefaultActor();
	FVector Collision = NewActorTemplate->GetCylinderExtent();
	FVector Location = ClickLocation + ClickPlane * (FBoxPushOut(ClickPlane, Collision) + 0.1);

	Constraints.Snap( Location, FVector(0, 0, 0) );

	Trans->Begin( TEXT("Create Actor") );

	Level->Modify();

	AActor* Actor = Factory->CreateActor( Level, &Location, NULL ); 
	if(Actor)
	{
		SelectNone( Level, 0 );
		// set bDeleteMe to true so that when the initial undo record is made,
		// the actor will be treated as destroyed, in that undo an add will actually work
		Actor->bDeleteMe = 1;
		Actor->Modify();
		Actor->bDeleteMe = 0;

		SelectActor( GEditor->Level, Actor, 1, 0 );

		Actor->PostEditMove();
	}

	Trans->End();

	RedrawLevel(Level);

	return Actor;
}

//
//	FViewportClick::FViewportClick - Calculates useful information about a click for the below ClickXXX functions to use.
//

FViewportClick::FViewportClick(FSceneView* View,FEditorLevelViewportClient* ViewportClient,FName InKey,EInputEvent InEvent,INT X,INT Y):
	Key(InKey),
	Event(InEvent)
{
	FMatrix	InvViewMatrix = View->ViewMatrix.Inverse(),
			InvProjMatrix = View->ProjectionMatrix.Inverse();

	FLOAT	ScreenX = (FLOAT)X / (FLOAT)ViewportClient->Viewport->GetSizeX() * 2.0f - 1.0f,
			ScreenY = 1.0f - (FLOAT)Y / (FLOAT)ViewportClient->Viewport->GetSizeY() * 2.0f;

	if(ViewportClient->ViewportType == LVT_Perspective)
	{
		Origin = InvViewMatrix.GetOrigin();
		Direction = InvViewMatrix.TransformNormal(FVector(InvProjMatrix.TransformFPlane(FPlane(ScreenX * NEAR_CLIPPING_PLANE,ScreenY * NEAR_CLIPPING_PLANE,0.0f,NEAR_CLIPPING_PLANE)))).SafeNormal();
	}
	else
	{
		Origin = InvViewMatrix.TransformFPlane(InvProjMatrix.TransformFPlane(FPlane(ScreenX,ScreenY,0.0f,1.0f)));
		Direction = InvViewMatrix.TransformNormal(FVector(0,0,1)).SafeNormal();
	}

	ControlDown = ViewportClient->Viewport->KeyState(KEY_LeftControl) || ViewportClient->Viewport->KeyState(KEY_RightControl);
	ShiftDown = ViewportClient->Viewport->KeyState(KEY_LeftShift) || ViewportClient->Viewport->KeyState(KEY_RightShift);
	AltDown = ViewportClient->Viewport->KeyState(KEY_LeftAlt) || ViewportClient->Viewport->KeyState(KEY_RightAlt);
}

//
//	ClickActor
//

void ClickActor(FEditorLevelViewportClient* ViewportClient,AActor* Actor,const FViewportClick& Click)
{
	// Find the point on the actor component which was clicked on.
	FCheckResult Hit(1);
	if(!Actor->ActorLineCheck(Hit,Click.Origin + Click.Direction * HALF_WORLD_MAX,Click.Origin,FVector(0,0,0),0))
	{	
		GEditor->ClickLocation = Hit.Location;
		GEditor->ClickPlane = FPlane(Hit.Location,Hit.Normal);
	}

	GEditor->Trans->Begin( TEXT("clicking on actors") );

	// Handle selection.
	if( Click.Key == KEY_RightMouseButton && !Click.ControlDown )
	{
		GEditor->SelectActor( ViewportClient->GetLevel(), Actor );

		GEditor->ShowUnrealEdContextMenu();
	}
	else if(Click.Event == IE_DoubleClick)
	{
		if(!Click.ControlDown)
			GEditor->SelectNone(ViewportClient->GetLevel(),0);
		GEditor->SelectActor(ViewportClient->GetLevel(),Actor);
		GEditor->Exec(TEXT("EDCALLBACK ACTORPROPS"));
	}
	else
	{
		if( Click.Key != KEY_RightMouseButton )
		{
			Actor->Modify();
			if( Click.ControlDown )
				GEditor->SelectActor( ViewportClient->GetLevel(), Actor, !GSelectionTools.IsSelected( Actor ) );
			else
			{
				GEditor->SelectNone( ViewportClient->GetLevel(), 0 );
				GEditor->SelectActor( ViewportClient->GetLevel(), Actor );
			}
		}
	}

	GEditor->Trans->End();
}

//
//	ClickBrushVertex
//

void ClickBrushVertex(FEditorLevelViewportClient* ViewportClient,ABrush* InBrush,FVector* InVertex,const FViewportClick& Click)
{
	GEditor->Trans->Begin( TEXT("clicking on brush vertex") );
	GEditor->SetPivot( InBrush->LocalToWorld().TransformFVector(*InVertex), 0, 0, 0 );

	if( Click.Key == KEY_RightMouseButton )
	{
		FEdMode* mode = GEditorModeTools.GetCurrentMode();

		FVector World = InBrush->LocalToWorld().TransformFVector(*InVertex);
		FVector Snapped = World;
		GEditor->Constraints.Snap( Snapped, GEditor->Constraints.GetGridSize() );
		FVector Delta = Snapped - World;
		GEditor->SetPivot( Snapped, 0, 0, 0 );

		switch( mode->GetID() )
		{
			case EM_Default:
			{
				// All selected actors need to move by the delta

				for( INT i = 0 ; i < GEditor->GetLevel()->Actors.Num() ; i++ )
				{
					AActor* Actor = GEditor->GetLevel()->Actors(i);

					if( Actor && GSelectionTools.IsSelected( Actor ) )
					{
						Actor->Modify();
						Actor->Location += Delta;
						Actor->UpdateComponents();
					}
				}
			}
			break;
		}
	}

	GEditor->Trans->End();

	ViewportClient->Viewport->Invalidate();
}

//
//	ClickStaticMeshVertex
//

void ClickStaticMeshVertex(FEditorLevelViewportClient* ViewportClient,AActor* InActor,FVector& InVertex,const FViewportClick& Click)
{
	GEditor->Trans->Begin( TEXT("clicking on static mesh vertex") );

	if( Click.Key == KEY_RightMouseButton )
	{
		FEdMode* mode = GEditorModeTools.GetCurrentMode();

		FVector World = InVertex;
		FVector Snapped = World;
		GEditor->Constraints.Snap( Snapped, GEditor->Constraints.GetGridSize() );
		FVector Delta = Snapped - World;
		GEditor->SetPivot( Snapped, 0, 0, 0 );

		// All selected actors need to move by the delta

		for( TSelectedActorIterator It(GEditor->GetLevel()) ; It ; ++It )
		{
			It->Modify();
			It->Location += Delta;
			It->UpdateComponents();
		}
	}

	GEditor->Trans->End();

	ViewportClient->Viewport->Invalidate();
}

//
//	ClickGeomPoly
//

void ClickGeomPoly(FEditorLevelViewportClient* ViewportClient,HGeomPolyProxy* InHitProxy,const FViewportClick& Click)
{
	FEdMode* mode = GEditorModeTools.GetCurrentMode();

	if( Click.Key == KEY_LeftMouseButton )
	{
		mode->GetCurrentTool()->StartTrans();

		if( !Click.ControlDown )
		{
			mode->GetCurrentTool()->SelectNone();
		}

		FGeomPoly* gp = &InHitProxy->GeomObject->PolyPool( InHitProxy->PolyIndex );
		gp->Select( Click.ControlDown ? !gp->IsSelected() : 1 );

		mode->GetCurrentTool()->EndTrans();
	}

	ViewportClient->Viewport->Invalidate();
}

//
//	ClickGeomEdge
//

void ClickGeomEdge(FEditorLevelViewportClient* ViewportClient,HGeomEdgeProxy* InHitProxy,const FViewportClick& Click)
{
	FEdMode* mode = GEditorModeTools.GetCurrentMode();

	if( Click.Key == KEY_LeftMouseButton )
	{
		mode->GetCurrentTool()->StartTrans();

		if( !Click.ControlDown )
		{
			mode->GetCurrentTool()->SelectNone();
		}

		FGeomEdge* HitEdge = &InHitProxy->GeomObject->EdgePool( InHitProxy->EdgeIndex );
		HitEdge->Select( Click.ControlDown ? !HitEdge->IsSelected() : 1 );

		if( ViewportClient->IsOrtho() )
		{
			// If this is an ortho viewport, select any vertices which are in the same 2D location as this one.

			FVector cmp = HitEdge->Mid;

			switch( ViewportClient->ViewportType )
			{
				case LVT_OrthoXY:
					cmp.Z = 0.f;
					break;

				case LVT_OrthoXZ:
					cmp.Y = 0.f;
					break;

				case LVT_OrthoYZ:
					cmp.X = 0.f;
					break;
			}

			// Select all edges in the brush that match the mid point of the original edge

			for( int e = 0 ; e < InHitProxy->GeomObject->EdgePool.Num() ; e++ )
			{
				FGeomEdge* ge = &InHitProxy->GeomObject->EdgePool(e);
				FVector Loc = ge->Mid;

				switch( ViewportClient->ViewportType )
				{
					case LVT_OrthoXY:	Loc.Z = 0.f;	break;
					case LVT_OrthoXZ:	Loc.Y = 0.f;	break;
					case LVT_OrthoYZ:	Loc.X = 0.f;	break;
				}

				if( Loc == cmp && ge != HitEdge )
				{
					InHitProxy->GeomObject->EdgePool(e).Select( Click.ControlDown ? !InHitProxy->GeomObject->EdgePool(e).IsSelected() : 1 );
				}
			}
		}

		mode->GetCurrentTool()->EndTrans();
	}

	ViewportClient->Viewport->Invalidate();
}

//
//	ClickGeomVertex
//

void ClickGeomVertex(FEditorLevelViewportClient* ViewportClient,HGeomVertexProxy* InHitProxy,const FViewportClick& Click)
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools.GetCurrentMode();

	if( Click.Key == KEY_RightMouseButton )
	{
		FModeTool_GeometryModify* tool = (FModeTool_GeometryModify*)mode->GetCurrentTool();

		tool->CurrentModifier->StartTrans();

		// Figure out how much to move to get back on the grid

		FVector World = InHitProxy->GeomObject->GetActualBrush()->LocalToWorld().TransformFVector( InHitProxy->GeomObject->VertexPool( InHitProxy->VertexIndex ) );
		FVector Snapped = World;
		GEditor->Constraints.Snap( Snapped, GEditor->Constraints.GetGridSize() );
		FVector Delta = Snapped - World;
		GEditor->SetPivot( Snapped, 0, 0, 0 );

		for( INT v = 0 ; v < InHitProxy->GeomObject->VertexPool.Num() ; ++v )
		{
			FGeomVertex* gv = &InHitProxy->GeomObject->VertexPool(v);

			if( gv->IsSelected() )
			{
				*gv += Delta;
			}
		}

		tool->CurrentModifier->EndTrans();
		InHitProxy->GeomObject->SendToSource();
	}
	else if( Click.Key == KEY_LeftMouseButton )
	{
		mode->GetCurrentTool()->StartTrans();

		if( !Click.ControlDown )
		{
			mode->GetCurrentTool()->SelectNone();
		}

		FGeomVertex* HitVertex = &InHitProxy->GeomObject->VertexPool( InHitProxy->VertexIndex );
		HitVertex->Select( Click.ControlDown ? !HitVertex->IsFullySelected() : 1 );

		if( ViewportClient->IsOrtho() )
		{
			// If this is an ortho viewport, select any vertices which are in the same 2D location as this one.

			FGeomVertex cmp = *HitVertex;

			switch( ViewportClient->ViewportType )
			{
				case LVT_OrthoXY:
					cmp.Z = 0.f;
					break;

				case LVT_OrthoXZ:
					cmp.Y = 0.f;
					break;

				case LVT_OrthoYZ:
					cmp.X = 0.f;
					break;
			}

			// Select all vertices in the brush that match the location of the original vertex

			for( int v = 0 ; v < InHitProxy->GeomObject->VertexPool.Num() ; v++ )
			{
				FGeomVertex gv = InHitProxy->GeomObject->VertexPool(v);

				switch( ViewportClient->ViewportType )
				{
					case LVT_OrthoXY:	gv.Z = 0.f;	break;
					case LVT_OrthoXZ:	gv.Y = 0.f;	break;
					case LVT_OrthoYZ:	gv.X = 0.f;	break;
				}

				if( gv == cmp && gv != *HitVertex )
				{
					InHitProxy->GeomObject->VertexPool(v).Select( Click.ControlDown ? !InHitProxy->GeomObject->VertexPool(v).IsSelected() : 1 );
				}
			}
		}

		mode->GetCurrentTool()->EndTrans();
	}

	ViewportClient->Viewport->Invalidate();
}

//
//	ClickSurface
//

static FBspSurf GSaveSurf;
void ClickSurface(FEditorLevelViewportClient* ViewportClient,UModel* Model,INT iSurf,INT iNode,const FViewportClick& Click)
{
	FBspSurf& Surf = Model->Surfs(iSurf);

	// Gizmos can cause BSP surfs to become selected without this check
	if(Click.Key == KEY_RightMouseButton && Click.ControlDown)
		return;

	// Adding actor.
	FPlane	Plane = Surf.Plane;

	// Remember hit location for actor-adding.
	GEditor->ClickLocation = FLinePlaneIntersection( Click.Origin, Click.Origin + Click.Direction, Plane );
	GEditor->ClickPlane    = Plane;

	if( Click.Key == KEY_LeftMouseButton && Click.ShiftDown && Click.ControlDown )
	{
		// Select the brush actor that belongs to this BSP surface.
		check( Surf.Actor );
		GEditor->SelectActor( GEditor->Level, Surf.Actor );
	}
	else if( Click.Key == KEY_LeftMouseButton && Click.ShiftDown )
	{
		// Apply texture to all selected.
		GEditor->Trans->Begin( TEXT("apply texture to selected surfaces") );
		for( INT i=0; i<Model->Surfs.Num(); i++ )
		{
			if( Model->Surfs(i).PolyFlags & PF_Selected )
			{
				Model->ModifySurf( i, 1 );
				Model->Surfs(i).Material = GSelectionTools.GetTop<UMaterialInstance>();
				GEditor->polyUpdateMaster( Model, i, 0 );
			}
		}
		GEditor->Trans->End();
	}
	else if( Click.Key == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_A) )
	{
		if( GSelectionTools.GetTop<UClass>() )
		{
			TCHAR Cmd[256];
			appSprintf( Cmd, TEXT("ACTOR ADD CLASS=%s"), GSelectionTools.GetTop<UClass>()->GetName() );
			GEditor->Exec( Cmd );
		}
	}
	else if( Click.Key == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_L) )
	{
		GEditor->Exec( TEXT("ACTOR ADD CLASS=POINTLIGHT") );
	}
	else if( Click.AltDown && Click.Key == KEY_RightMouseButton )
	{
		// Grab the texture.
		GSelectionTools.SelectNone(UMaterialInstance::StaticClass());
		if(Surf.Material)
			GSelectionTools.Select(Surf.Material,1);
		GSaveSurf = Surf;
	}
	else if( Click.AltDown && Click.Key == KEY_LeftMouseButton)
	{
		// Apply texture to the one polygon clicked on.
		GEditor->Trans->Begin( TEXT("apply texture to surface") );
		Model->ModifySurf( iSurf, 1 );
		Surf.Material = GSelectionTools.GetTop<UMaterialInstance>();
		if( Click.ControlDown )
		{
			Surf.vTextureU	= GSaveSurf.vTextureU;
			Surf.vTextureV	= GSaveSurf.vTextureV;
			if( Surf.vNormal == GSaveSurf.vNormal )
			{
				GLog->Logf( TEXT("WARNING: the texture coordinates were not parallel to the surface.") );
			}
			Surf.PolyFlags	= GSaveSurf.PolyFlags;
			GEditor->polyUpdateMaster( Model, iSurf, 1 );
		}
		else
		{
			GEditor->polyUpdateMaster( Model, iSurf, 0 );
		}
		GEditor->Trans->End();
	}
	else if( Click.Key == KEY_RightMouseButton && !Click.ControlDown )
	{
		// Edit surface properties.
		GEditor->Trans->Begin( TEXT("select surface for editing") );
		Model->ModifySurf( iSurf, 0 );
		Surf.PolyFlags |= PF_Selected;
		GEditor->NoteSelectionChange( GEditor->Level );

		GEditor->ShowUnrealEdContextSurfaceMenu();

		GEditor->Trans->End();
	}
	else
	{
		// Select or deselect surfaces.
		GEditor->Trans->Begin( TEXT("select surfaces") );
		DWORD SelectMask = Surf.PolyFlags & PF_Selected;
		if( !Click.ControlDown )
			GEditor->SelectNone( GEditor->Level, 0 );
		Model->ModifySurf( iSurf, 0 );
		Surf.PolyFlags = (Surf.PolyFlags & ~PF_Selected) | (SelectMask ^ PF_Selected);
		GEditor->NoteSelectionChange( GEditor->Level );
		GEditor->Trans->End();
	}

}

//
//	ClickBackdrop
//

void ClickBackdrop(FEditorLevelViewportClient* ViewportClient,const FViewportClick& Click)
{
	GEditor->ClickLocation = Click.Origin + Click.Direction * HALF_WORLD_MAX;
	GEditor->ClickPlane    = FPlane(0,0,0,0);

	if( Click.Key == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_A) )
	{
		if( GSelectionTools.GetTop<UClass>() )
		{
			TCHAR Cmd[256];
			appSprintf( Cmd, TEXT("ACTOR ADD CLASS=%s"), GSelectionTools.GetTop<UClass>()->GetName() );
			GEditor->Exec( Cmd );
		}
	}
	else if( Click.Key == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_L) )
	{
		GEditor->Exec( TEXT("ACTOR ADD CLASS=POINTLIGHT") );
	}
	else if( Click.Key == KEY_RightMouseButton && !Click.ControlDown )
	{
		GEditor->ShowUnrealEdContextMenu();
	}
	else if( Click.Key == KEY_LeftMouseButton )
	{
		if( !Click.ControlDown )
		{
			GEditor->Trans->Begin( TEXT("Select None") );
			GEditor->SelectNone( ViewportClient->GetLevel(), 1 );
			GEditor->Trans->End();
		}
	}

}

//
//	FEditorLevelViewportClient::FEditorLevelViewportClient
//

FEditorLevelViewportClient::FEditorLevelViewportClient():
	ViewportType(LVT_Perspective),
	ViewLocation(0,0,0),
	ViewRotation(0,0,0),
	ViewFOV(GEditor->FOVAngle),
	OrthoZoom(DEFAULT_ORTHOZOOM),
	Viewport(NULL),
	ViewMode(SVM_Lit),
	ShowFlags(SHOW_DefaultEditor),
	Realtime(false),
	RealtimeAudio(false),
	bDrawAxes(true),
	bConstrainAspectRatio(false),
	bEnableFading(false),
	FadeAmount(0.f)
{
	Input = ConstructObject<UEditorViewportInput>(UEditorViewportInput::StaticClass());
	Input->Editor = GEditor;
	Widget = new FWidget;
	GEditor->ViewportClients.AddItem(this);

	FadeColor = FColor(0,0,0);
}

//
//	FEditorLevelViewportClient::~FEditorLevelViewportClient
//

FEditorLevelViewportClient::~FEditorLevelViewportClient()
{
	delete Widget;

	if(Viewport)
		appErrorf(TEXT("Viewport != NULL in FEditorLevelViewportClient destructor."));

	GEditor->ViewportClients.RemoveItem(this);
}

//
//	FEditorLevelViewportClient::CalcSceneView
//

void FEditorLevelViewportClient::CalcSceneView(FSceneView* View)
{
	View->ViewMatrix = FTranslationMatrix(-ViewLocation);

	if(ViewportType == LVT_OrthoXY)
	{
		View->ViewMatrix = View->ViewMatrix * FMatrix(
			FPlane(-1,	0,	0,					0),
			FPlane(0,	1,	0,					0),
			FPlane(0,	0,	-1,					0),
			FPlane(0,	0,	-ViewLocation.Z,	1));
	}
	else if(ViewportType == LVT_OrthoXZ)
	{
		View->ViewMatrix = View->ViewMatrix * FMatrix(
			FPlane(1,	0,	0,					0),
			FPlane(0,	0,	-1,					0),
			FPlane(0,	1,	0,					0),
			FPlane(0,	0,	-ViewLocation.Y,	1));
	}
	else if(ViewportType == LVT_OrthoYZ)
	{
		View->ViewMatrix = View->ViewMatrix * FMatrix(
			FPlane(0,	0,	1,					0),
			FPlane(1,	0,	0,					0),
			FPlane(0,	1,	0,					0),
			FPlane(0,	0,	ViewLocation.X,		1));
	}

	if(ViewportType == LVT_Perspective)
	{
		View->ViewMatrix = View->ViewMatrix * FInverseRotationMatrix(ViewRotation);
		View->ViewMatrix = View->ViewMatrix * FMatrix(
			FPlane(0,	0,	1,	0),
			FPlane(1,	0,	0,	0),
			FPlane(0,	1,	0,	0),
			FPlane(0,	0,	0,	1));

		if( bConstrainAspectRatio)
		{
			View->ProjectionMatrix = FPerspectiveMatrix(
				ViewFOV * (FLOAT)PI / 360.0f,
				AspectRatio,
				1.f,
				NEAR_CLIPPING_PLANE
				);
		}
		else
		{
			View->ProjectionMatrix = FPerspectiveMatrix(
				ViewFOV * (FLOAT)PI / 360.0f,
				Viewport->GetSizeX(),
				Viewport->GetSizeY(),
				NEAR_CLIPPING_PLANE
				);
		}
	}
	else
	{
		FLOAT	Zoom = OrthoZoom / (Viewport->GetSizeX() * 15.0f);
		View->ProjectionMatrix = FOrthoMatrix(
			Zoom * Viewport->GetSizeX() / 2,
			Zoom * Viewport->GetSizeY() / 2,
			0.5f / HALF_WORLD_MAX,
			HALF_WORLD_MAX
			);
	}

	View->ViewMode = ViewMode;
	View->ShowFlags = ShowFlags | (ViewportType != LVT_Perspective ? SHOW_Orthographic : 0);
	View->BackgroundColor = GetBackgroundColor();
}

//
//	FEditorLevelViewportClient::ProcessClick
//

void FEditorLevelViewportClient::ProcessClick(FSceneView* View,HHitProxy* HitProxy,FName Key,EInputEvent Event,UINT HitX,UINT HitY)
{
	FViewportClick	Click(View,this,Key,Event,HitX,HitY);

	if(HitProxy && HitProxy->IsA(TEXT("HActor")))
		ClickActor(this,((HActor*)HitProxy)->Actor,Click);
	else if(HitProxy && HitProxy->IsA(TEXT("HBSPBrushVert")))
		ClickBrushVertex(this,((HBSPBrushVert*)HitProxy)->Brush,((HBSPBrushVert*)HitProxy)->Vertex,Click);
	else if(HitProxy && HitProxy->IsA(TEXT("HStaticMeshVert")))
		ClickStaticMeshVertex(this,((HStaticMeshVert*)HitProxy)->Actor,((HStaticMeshVert*)HitProxy)->Vertex,Click);
	else if(HitProxy && HitProxy->IsA(TEXT("HGeomPolyProxy")))
		ClickGeomPoly(this,(HGeomPolyProxy*)HitProxy,Click);
	else if(HitProxy && HitProxy->IsA(TEXT("HGeomEdgeProxy")))
		ClickGeomEdge(this,(HGeomEdgeProxy*)HitProxy,Click);
	else if(HitProxy && HitProxy->IsA(TEXT("HGeomVertexProxy")))
		ClickGeomVertex(this,(HGeomVertexProxy*)HitProxy,Click);
	else if(HitProxy && HitProxy->IsA(TEXT("HBspSurf")))
	{	
		HBspSurf*	SurfaceHit = ((HBspSurf*)HitProxy);
		ClickSurface(this,SurfaceHit->Model,SurfaceHit->iSurf,SurfaceHit->iNode,Click);
	}
	else if(!HitProxy)
		ClickBackdrop(this,Click);

}

//
//	FEditorLevelViewportClient::Tick
//

void FEditorLevelViewportClient::Tick(FLOAT DeltaTime)
{
	UpdateMouseDelta();

	GEditorModeTools.GetCurrentMode()->Tick(this,DeltaTime);
}

void FEditorLevelViewportClient::UpdateMouseDelta()
{
	if( !MouseDeltaTracker.DragTool )
	{
		FVector DragDelta;
		
		if( Widget->CurrentAxis != AXIS_None && GSelectionTools.ShouldSnapMovement() )
		{
			DragDelta = MouseDeltaTracker.GetDeltaSnapped();
		}
		else
		{
			DragDelta = MouseDeltaTracker.GetDelta();
		}

		GEditor->MouseMovement += FVector( abs(DragDelta.X), abs(DragDelta.Y), abs(DragDelta.Z) );

		if( Viewport )
		{
			UBOOL AltDown = Input->IsAltPressed(),
				ShiftDown = Input->IsShiftPressed(),
				ControlDown = Input->IsCtrlPressed(),
				LeftMouseButtonDown = Viewport->KeyState(KEY_LeftMouseButton),
				RightMouseButtonDown = Viewport->KeyState(KEY_RightMouseButton);

			if( !DragDelta.IsZero() )
			{
				for( INT x = 0 ; x < 3 ; ++x )
				{
					FVector WkDelta = FVector( 0,0,0 );
					WkDelta[x] = DragDelta[x];

					if( !WkDelta.IsZero() )
					{
						// Convert the movement delta into drag/rotation deltas

						FVector Drag;
						FRotator Rot;
						FVector Scale;
						MouseDeltaTracker.ConvertMovementDeltaToDragRot( this, WkDelta, Drag, Rot, Scale );

						// Give the current editor mode a chance to use the input first.  If it does, don't apply it to anything else.

						if( GEditorModeTools.GetCurrentMode()->InputDelta( this, Viewport, Drag, Rot, Scale ) )
						{
							if( GEditorModeTools.GetCurrentMode()->AllowWidgetMove() )
							{
								GEditorModeTools.PivotLocation += Drag;
								GEditorModeTools.SnappedLocation += Drag;
							}
						}
						else
						{
							// Apply deltas to selected actors or viewport cameras

							if( Widget->CurrentAxis != AXIS_None )
							{
								ApplyDeltaToActors( Drag, Rot, Scale );
								GEditorModeTools.PivotLocation += Drag;
								GEditorModeTools.SnappedLocation += Drag;

								if( ShiftDown )
								{
									MoveViewportCamera( Drag, FRotator(0,0,0) );
								}

								GEditorModeTools.GetCurrentMode()->UpdateInternalData();
							}
							else
							{
								// Only apply camera speed modifiers to the drag if we aren't zooming in an ortho viewport.

								if( !IsOrtho() || !(LeftMouseButtonDown && RightMouseButtonDown) )
								{
									Drag *= GEditor->MovementSpeed / 4.f;
								}
								MoveViewportCamera( Drag, Rot );
							}
						}

						// Clean up

						MouseDeltaTracker.ReduceBy( WkDelta );
					}
				}

				Viewport->Invalidate();
			}
		}
	}
}

//
//	FEditorLevelViewportClient::InputKey
//

void FEditorLevelViewportClient::InputKey(FChildViewport* Viewport,FName Key,EInputEvent Event,FLOAT)
{
	INT			HitX = Viewport->GetMouseX(),
				HitY = Viewport->GetMouseY();
	FSceneView	View;
	CalcSceneView(&View);

	// Compute the click location.

	GEditor->ClickLocation = FVector((View.ViewMatrix * View.ProjectionMatrix).Inverse().TransformFPlane(FPlane((HitX - Viewport->GetSizeX() / 2.0f) / (Viewport->GetSizeX() / 2.0f),(HitY - Viewport->GetSizeY() / 2.0f) / -(Viewport->GetSizeY() / 2.0f),0.5f,1.0f)));

	// Let the current mode have a look at the input before reacting to it

	if( GEditorModeTools.GetCurrentMode()->InputKey(this, Viewport, Key, Event) )
		return;

	// Tell the viewports input handler about the keypress.

	if(Input->InputKey(Key,Event))
		return;

	// Tell the other editor viewports about the keypress so they will be aware if the user tries
	// to do something like box select inside of them without first clicking them for focus.

	for( UINT v = 0 ; v < (UINT)GEditor->ViewportClients.Num() ; ++v )
	{
		FEditorLevelViewportClient* vc = GEditor->ViewportClients(v);

		if( vc->GetLevel() == GEditor->GetLevel() && vc != this )
		{
			vc->Input->InputKey(Key,Event);
		}
	}

	// Remember which keys and buttons are pressed down

	UBOOL AltDown = Input->IsAltPressed(),
		ShiftDown = Input->IsShiftPressed(),
		ControlDown = Input->IsCtrlPressed(),
		LeftMouseButtonDown = Viewport->KeyState(KEY_LeftMouseButton),
		MiddleMouseButtonDown = Viewport->KeyState(KEY_MiddleMouseButton),
		RightMouseButtonDown = Viewport->KeyState(KEY_RightMouseButton);

	if( Key == KEY_LeftMouseButton || Key == KEY_MiddleMouseButton || Key == KEY_RightMouseButton )
	{
		Viewport->CaptureMouseInput( LeftMouseButtonDown || MiddleMouseButtonDown || RightMouseButtonDown );

		switch( Event )
		{
			case IE_DoubleClick:
			{
				MouseDeltaTracker.StartTracking( this, HitX, HitY );
				GEditor->MouseMovement = FVector(0,0,0);
				HHitProxy*	HitProxy = Viewport->GetHitProxy(HitX,HitY);
				ProcessClick(&View,HitProxy,Key,Event,HitX,HitY);
			}
			break;

			case IE_Pressed:
			{
				if( (ControlDown && RightMouseButtonDown) && IsOrtho() )
				{
					GEditorModeTools.SetWidgetModeOverride( WM_Rotate );
				}

				MouseDeltaTracker.StartTracking( this, HitX, HitY );

				// If the user is holding down the ALT key, and they are using the widget, duplicate the actors and move them instead.

				EAxis Whatever = Widget->CurrentAxis;
				if( AltDown && Widget->CurrentAxis != AXIS_None )
				{
					GEditor->edactCopySelected( GEditor->GetLevel() );
					//GEditor->SelectNone( GEditor->GetLevel(), 1, 0 );
					GEditor->edactPasteSelected( GEditor->GetLevel(), 1, 0 );
					//GEditor->RedrawLevel( GEditor->GetLevel() );
				}

				GEditor->MouseMovement = FVector(0,0,0);
			}
			break;

			case IE_Released:
			{
				if( MouseDeltaTracker.EndTracking( this ) )
				{
					// If the mouse haven't moved too far, treat the button release as a click
					
					if( GEditor->MouseMovement.Size() < MOUSE_CLICK_DRAG_DELTA && !MouseDeltaTracker.DragTool )
					{
						HHitProxy*	HitProxy = Viewport->GetHitProxy(HitX,HitY);

						ProcessClick(&View,HitProxy,Key,Event,HitX,HitY);
					}

					Widget->CurrentAxis = AXIS_None;
					Viewport->Invalidate();

					GEditorModeTools.SetWidgetModeOverride( WM_None );
				}
			}
			break;
		}
	}
	else if( !IsOrtho() && (!ControlDown && !ShiftDown && !AltDown) && (Key == KEY_Left || Key == KEY_Right || Key == KEY_Up || Key == KEY_Down) )
	{
		FVector Drag;
		if( Key == KEY_Up )
		{
			Drag.X = GEditor->Constraints.GetGridSize() * GMath.CosTab( ViewRotation.Yaw );
			Drag.Y = GEditor->Constraints.GetGridSize() * GMath.SinTab( ViewRotation.Yaw );
		}
		else if( Key == KEY_Down )
		{
			Drag.X = GEditor->Constraints.GetGridSize() * GMath.CosTab( ViewRotation.Yaw - 32768 );
			Drag.Y = GEditor->Constraints.GetGridSize() * GMath.SinTab( ViewRotation.Yaw - 32768 );
		}
		else if( Key == KEY_Right )
		{
			Drag.X = GEditor->Constraints.GetGridSize() * -GMath.SinTab( ViewRotation.Yaw );
			Drag.Y = GEditor->Constraints.GetGridSize() *  GMath.CosTab( ViewRotation.Yaw );
		}

		else if( Key == KEY_Left )
		{
			Drag.X = GEditor->Constraints.GetGridSize() * -GMath.SinTab( ViewRotation.Yaw - 32768 );
			Drag.Y = GEditor->Constraints.GetGridSize() *  GMath.CosTab( ViewRotation.Yaw - 32768 );
		}

		MoveViewportCamera( Drag, FRotator(0,0,0) );
		Viewport->Invalidate();
	}
	else if( ControlDown && MiddleMouseButtonDown )
	{
		FVector vec;

		if( IsOrtho() )
		{
			vec = GEditor->ClickLocation;
		}
		else
		{
			vec = ViewLocation;
		}

		INT idx = -1;

		switch( ViewportType )
		{
			case LVT_OrthoXY:
				idx = 2;
				break;
			case LVT_OrthoXZ:
				idx = 1;
				break;
			case LVT_OrthoYZ:
				idx = 0;
				break;
		}

		for( INT v = 0 ; v < GEditor->ViewportClients.Num() ; ++v )
		{
			FEditorLevelViewportClient* vc = GEditor->ViewportClients(v);

			if( vc->GetLevel() == GEditor->GetLevel() && vc != this )
			{
				if( IsOrtho() )
				{
					if( idx != 0 )		vc->ViewLocation.X = vec.X;
					if( idx != 1 )		vc->ViewLocation.Y = vec.Y;
					if( idx != 2 )		vc->ViewLocation.Z = vec.Z;

					vc->Viewport->Invalidate();
				}
				else
				{
					vc->ViewLocation = vec;
					vc->Viewport->Invalidate();
				}
			}
		}
	}
	else if( Event == IE_Pressed )
	{
		if( AltDown && (Key == KEY_Left || Key == KEY_Right || Key == KEY_Up || Key == KEY_Down) )
		{
			// Keyboard nudging of the widget

			MouseDeltaTracker.StartTracking( this, HitX, HitY );
			GEditorModeTools.GetCurrentMode()->StartTracking();

			if( Key == KEY_Left )
			{
				Widget->CurrentAxis = GetHorizAxis();
				MouseDeltaTracker.AddDelta( this, KEY_MouseX, -GEditor->Constraints.GetGridSize(), 1 );
				Widget->CurrentAxis = GetHorizAxis();
			}
			else if( Key == KEY_Right )
			{
				Widget->CurrentAxis = GetHorizAxis();
				MouseDeltaTracker.AddDelta( this, KEY_MouseX, GEditor->Constraints.GetGridSize(), 1 );
				Widget->CurrentAxis = GetHorizAxis();
			}
			else if( Key == KEY_Up )
			{
				Widget->CurrentAxis = GetVertAxis();
				MouseDeltaTracker.AddDelta( this, KEY_MouseY, GEditor->Constraints.GetGridSize(), 1 );
				Widget->CurrentAxis = GetVertAxis();
			}
			else if( Key == KEY_Down )
			{
				Widget->CurrentAxis = GetVertAxis();
				MouseDeltaTracker.AddDelta( this, KEY_MouseY, -GEditor->Constraints.GetGridSize(), 1 );
				Widget->CurrentAxis = GetVertAxis();
			}

			UpdateMouseDelta();
			GEditorModeTools.GetCurrentMode()->EndTracking();
			MouseDeltaTracker.EndTracking( this );
			Widget->CurrentAxis = AXIS_None;
		}

		// BOOKMARKS

		TCHAR ch = 0;
		if( Key == KEY_Zero )		ch = '0';
		else if( Key == KEY_One )	ch = '1';
		else if( Key == KEY_Two )	ch = '2';
		else if( Key == KEY_Three )	ch = '3';
		else if( Key == KEY_Four )	ch = '4';
		else if( Key == KEY_Five )	ch = '5';
		else if( Key == KEY_Six )	ch = '6';
		else if( Key == KEY_Seven )	ch = '7';
		else if( Key == KEY_Eight )	ch = '8';
		else if( Key == KEY_Nine )	ch = '9';

		if( ch >= '0' && ch <= '9' )
		{
			// Bookmarks

			INT BookMarkIndex = ch - '0';

			// CTRL+# will set a bookmark, # will jump to it

			if( ControlDown )
			{
				GEditorModeTools.SetBookMark( BookMarkIndex, this );
			}
			else
			{
				GEditorModeTools.JumpToBookMark( BookMarkIndex );
			}
		}

		// Change grid size

		if( Key == KEY_LeftBracket )
		{
			GEditor->Constraints.SetGridSz( GEditor->Constraints.CurrentGridSz - 1 );
		}
		if( Key == KEY_RightBracket )
		{
			GEditor->Constraints.SetGridSz( GEditor->Constraints.CurrentGridSz + 1 );
		}
	}
}

/**
 * Returns the horizontal axis for this viewport.
 */

EAxis FEditorLevelViewportClient::GetHorizAxis()
{
	switch( ViewportType )
	{
		case LVT_OrthoXY:
			return AXIS_X;
		case LVT_OrthoXZ:
			return AXIS_X;
		case LVT_OrthoYZ:
			return AXIS_Y;
	}

	return AXIS_X;
}

/**
 * Returns the vertical axis for this viewport.
 */

EAxis FEditorLevelViewportClient::GetVertAxis()
{
	switch( ViewportType )
	{
		case LVT_OrthoXY:
			return AXIS_Y;
		case LVT_OrthoXZ:
			return AXIS_Z;
		case LVT_OrthoYZ:
			return AXIS_Z;
	}

	return AXIS_Z;
}

//
//	FEditorLevelViewportClient::InputAxis
//

void FEditorLevelViewportClient::InputAxis(FChildViewport* Viewport,FName Key,FLOAT Delta,FLOAT DeltaTime)
{
	if( GCurrentLevelEditingViewportClient != this )
	{
		GCallback->Send( CALLBACK_RedrawAllViewports );
		GCurrentLevelEditingViewportClient = this;
	}

	// Let the engine try to handle the input first

	if( Input->InputAxis(Key,Delta,DeltaTime) )
		return;

	// Accumulate and snap the mouse movement since the last mouse button click

	MouseDeltaTracker.AddDelta( this, Key, Delta, 0 );

	// If we are using a drag tool, paint the viewport so we can see it update

	if( MouseDeltaTracker.DragTool )
	{
		Viewport->Invalidate();
	}
}

// Determines if InComponent is inside of InSelBBox.  This check differs depending on the type of component.

UBOOL FEditorLevelViewportClient::ComponentIsTouchingSelectionBox( AActor* InActor, UPrimitiveComponent* InComponent, FBox InSelBBox )
{
	if( !InComponent )
		return 0;

	if( InComponent->IsA( USkeletalMeshComponent::StaticClass() ) )
	{
		USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>( InComponent );

		if( SkeletalMeshComponent->SkeletalMesh )
		{
			check(SkeletalMeshComponent->SkeletalMesh->LODModels.Num());
			SkeletalMeshComponent->SkeletalMesh->LODModels(0).Points.Load();
			for( INT i=0; i<SkeletalMeshComponent->SkeletalMesh->LODModels(0).Points.Num(); i++)
			{
				FVector Location = SkeletalMeshComponent->LocalToWorld.TransformFVector(SkeletalMeshComponent->SkeletalMesh->LODModels(0).Points(i));

				if(FPointBoxIntersection(Location,InSelBBox))
					return 1;
			}
		}
	}
	else if(InComponent->IsA(UBrushComponent::StaticClass()))
	{
		UBrushComponent*	BrushComponent = Cast<UBrushComponent>(InComponent);

		if(BrushComponent->Brush && BrushComponent->Brush->Polys)
		{
			for(UINT PolyIndex = 0;PolyIndex < (UINT)BrushComponent->Brush->Polys->Element.Num();PolyIndex++)
			{
				FPoly&	Poly = BrushComponent->Brush->Polys->Element(PolyIndex);

				for(UINT VertexIndex = 0;VertexIndex < (UINT)Poly.NumVertices;VertexIndex++)
				{
					FVector Location = InComponent->LocalToWorld.TransformFVector(Poly.Vertex[VertexIndex]);
			
					if(FPointBoxIntersection(Location,InSelBBox))
						return 1;
				}
			}
		}
	}
	else if(InComponent->IsA(UStaticMeshComponent::StaticClass()) )
	{
		UStaticMeshComponent*	StaticMeshComponent = Cast<UStaticMeshComponent>(InComponent);

		if(StaticMeshComponent->StaticMesh && (((FEditorLevelViewportClient*)Viewport->ViewportClient)->ShowFlags&SHOW_StaticMeshes))
		{
			for(UINT VertexIndex = 0;VertexIndex < (UINT)StaticMeshComponent->StaticMesh->Vertices.Num();VertexIndex++)
			{
				FStaticMeshVertex*	Vertex = &StaticMeshComponent->StaticMesh->Vertices(VertexIndex);
				FVector				Location = InComponent->LocalToWorld.TransformFVector(Vertex->Position);

				if(FPointBoxIntersection(Location,InSelBBox))
					return 1;
			}
		}
	}
	else if(InComponent->IsA(USpriteComponent::StaticClass()))
	{
		USpriteComponent*	SpriteComponent = Cast<USpriteComponent>(InComponent);

		if(InSelBBox.Intersect(
			FBox(
				InActor->Location - InActor->DrawScale * Max(SpriteComponent->Sprite->SizeX,SpriteComponent->Sprite->SizeY) * FVector(1,1,1),
				InActor->Location + InActor->DrawScale * Max(SpriteComponent->Sprite->SizeX,SpriteComponent->Sprite->SizeY) * FVector(1,1,1)
				)
			))
			return 1;
	}

	return 0;
}

// Determines which axis InKey and InDelta most refer to and returns
// a corresponding FVector.  This vector represents the mouse movement
// translated into the viewports/widgets axis space.
//
// @param InNudge		If 1, this delta is coming from a keyboard nudge and not the mouse

FVector FEditorLevelViewportClient::TranslateDelta( FName InKey, FLOAT InDelta, UBOOL InNudge )
{
	UBOOL AltDown = Input->IsAltPressed(),
		ShiftDown = Input->IsShiftPressed(),
		ControlDown = Input->IsCtrlPressed(),
		LeftMouseButtonDown = Viewport->KeyState(KEY_LeftMouseButton),
		RightMouseButtonDown = Viewport->KeyState(KEY_RightMouseButton);

	FVector vec;

	FLOAT X = InKey == KEY_MouseX ? InDelta : 0.f;
	FLOAT Y = InKey == KEY_MouseY ? InDelta : 0.f;

	switch( ViewportType )
	{
		case LVT_OrthoXY:
		case LVT_OrthoXZ:
		case LVT_OrthoYZ:

			if( InNudge )
			{
				vec = FVector( X, Y, 0.f );
			}
			else
			{
				vec = FVector( X * (OrthoZoom/CAMERA_ZOOM_DIV), Y * (OrthoZoom/CAMERA_ZOOM_DIV), 0.f );
			}

			if( Widget->CurrentAxis == AXIS_None )
			{
				switch( ViewportType )
				{
					case LVT_OrthoXY:
						vec.X *= -1;
						break;
					case LVT_OrthoXZ:
						vec = FVector( X * (OrthoZoom/CAMERA_ZOOM_DIV), 0.f, Y * (OrthoZoom/CAMERA_ZOOM_DIV) );
						break;
					case LVT_OrthoYZ:
						vec = FVector( 0.f, X * (OrthoZoom/CAMERA_ZOOM_DIV), Y * (OrthoZoom/CAMERA_ZOOM_DIV) );
						break;
				}
			}
			break;

		case LVT_Perspective:
			vec = FVector( X, Y, 0.f );
			break;

		default:
			check(0);		// Unknown viewport type
			break;
	}

	if( IsOrtho() && (LeftMouseButtonDown && RightMouseButtonDown) && Y != 0.f )
		vec = FVector(0,0,Y);

	return vec;
}

// Converts a generic movement delta into drag/rotation deltas based on the viewport and keys held down

void FEditorLevelViewportClient::ConvertMovementToDragRot( const FVector& InDelta, FVector& InDragDelta, FRotator& InRotDelta )
{
	UBOOL AltDown = Input->IsAltPressed(),
		ShiftDown = Input->IsShiftPressed(),
		ControlDown = Input->IsCtrlPressed(),
		LeftMouseButtonDown = Viewport->KeyState(KEY_LeftMouseButton),
		RightMouseButtonDown = Viewport->KeyState(KEY_RightMouseButton);

	InDragDelta = FVector(0,0,0);
	InRotDelta = FRotator(0,0,0);

	switch( ViewportType )
	{
		case LVT_OrthoXY:
		case LVT_OrthoXZ:
		case LVT_OrthoYZ:
		{
			if( LeftMouseButtonDown && RightMouseButtonDown )
			{
				// Change ortho zoom

				InDragDelta = FVector(0,0,InDelta.Z);
			}
			else if( !LeftMouseButtonDown && RightMouseButtonDown )
			{
				// Move camera

				InDragDelta = InDelta;
			}
			else if( LeftMouseButtonDown && !RightMouseButtonDown )
			{
				// Move actors

				InDragDelta = InDelta;
			}
		}
		break;

		case LVT_Perspective:
		{
			if( LeftMouseButtonDown && !RightMouseButtonDown )
			{
				// Move forward and yaw

				InDragDelta.X = InDelta.Y * GMath.CosTab( ViewRotation.Yaw );
				InDragDelta.Y = InDelta.Y * GMath.SinTab( ViewRotation.Yaw );

				InRotDelta.Yaw = InDelta.X * CAMERA_ROTATION_SPEED;
			}
			else if( RightMouseButtonDown && LeftMouseButtonDown )
			{
				// Pan left/right/up/down

				InDragDelta.X = InDelta.X * -GMath.SinTab( ViewRotation.Yaw );
				InDragDelta.Y = InDelta.X *  GMath.CosTab( ViewRotation.Yaw );
				InDragDelta.Z = InDelta.Y;
			}
			else if( RightMouseButtonDown && !LeftMouseButtonDown )
			{
				// Change viewing angle

				InRotDelta.Yaw = InDelta.X * CAMERA_ROTATION_SPEED;
				InRotDelta.Pitch = InDelta.Y * CAMERA_ROTATION_SPEED;
			}
		}
		break;

		default:
			check(0);	// unknown viewport type
			break;
	}
}

void FEditorLevelViewportClient::MoveViewportCamera( const FVector& InDrag, const FRotator& InRot )
{
	UBOOL AltDown = Input->IsAltPressed(),
		ShiftDown = Input->IsShiftPressed(),
		ControlDown = Input->IsCtrlPressed(),
		LeftMouseButtonDown = Viewport->KeyState(KEY_LeftMouseButton),
		RightMouseButtonDown = Viewport->KeyState(KEY_RightMouseButton);

	switch( ViewportType )
	{
		case LVT_OrthoXY:
		case LVT_OrthoXZ:
		case LVT_OrthoYZ:
		{
			if( LeftMouseButtonDown && RightMouseButtonDown )
			{
				OrthoZoom += (OrthoZoom / CAMERA_ZOOM_DAMPEN) * InDrag.Z;
				OrthoZoom = Clamp<FLOAT>( OrthoZoom, MIN_ORTHOZOOM, MAX_ORTHOZOOM );
			}
			else
			{
				ViewLocation.AddBounded( InDrag, HALF_WORLD_MAX1 );
			}
		}
		break;

		case LVT_Perspective:
		{
			// Update camera Rotation
			ViewRotation += FRotator( InRot.Pitch, InRot.Yaw, InRot.Roll );
			
			// If rotation is pitching down by using a big positive number, change it so its using a smaller negative number
			if((ViewRotation.Pitch > 49151) && (ViewRotation.Pitch <= 65535))
			{
				ViewRotation.Pitch = ViewRotation.Pitch - 65535;
			}

			// Make sure its withing  +/- 90 degrees.
			ViewRotation.Pitch = Clamp( ViewRotation.Pitch, -16384, 16384 );

			// Update camera Location
			ViewLocation.AddBounded( InDrag, HALF_WORLD_MAX1 );

			// Tell the editing mode that the camera moved, in case its interested.
			GEditorModeTools.GetCurrentMode()->CamMoveNotify(this);
		}
		break;
	}
}

void FEditorLevelViewportClient::ApplyDeltaToActors( FVector& InDrag, FRotator& InRot, FVector& InScale )
{
	UBOOL AltDown = Input->IsAltPressed(),
		ShiftDown = Input->IsShiftPressed(),
		ControlDown = Input->IsCtrlPressed(),
		LeftMouseButtonDown = Viewport->KeyState(KEY_LeftMouseButton),
		RightMouseButtonDown = Viewport->KeyState(KEY_RightMouseButton);

	if( InDrag.IsZero() && InRot.IsZero() && InScale.IsZero() )
		return;

	// Transact the actors.

	GEditor->NoteActorMovement( GetLevel() );

	// Apply the deltas to any selected actors

	for( INT i = 0 ; i < GetLevel()->Actors.Num() ; i++ )
	{
		AActor* Actor = GetLevel()->Actors(i);
		if( Actor && GSelectionTools.IsSelected( Actor ) && !Actor->bLockLocation )
		{
			ApplyDeltaToActor( Actor, InDrag, InRot, InScale );
		}
	}
}

//
//	FEditorLevelViewportClient::ApplyDeltaToActor
//

void FEditorLevelViewportClient::ApplyDeltaToActor( AActor* InActor, FVector& InDeltaDrag, FRotator& InDeltaRot, FVector& InDeltaScale )
{
	if( InActor->IsBrush() )
	{
		ABrush* Brush = (ABrush*)InActor;
		if( Brush->BrushComponent && Brush->BrushComponent->Brush )
		{
			Brush->BrushComponent->Brush->Polys->Element.ModifyAllItems();
		}
	}

	// **
	// ** Rotation
	// **

	if( InActor->IsBrush() )
	{
		// Brushes need their individual verts rotated

		ABrush* Brush = (ABrush*)InActor;
		if(Brush->BrushComponent->Brush && Brush->BrushComponent->Brush->Polys)
		{
			for( INT poly = 0 ; poly < Brush->BrushComponent->Brush->Polys->Element.Num() ; poly++ )
			{
				FPoly* Poly = &(Brush->BrushComponent->Brush->Polys->Element(poly));

				// Rotate the vertices

				for( INT vertex = 0 ; vertex < Poly->NumVertices ; vertex++ )
					Poly->Vertex[vertex] = Brush->PrePivot + FRotationMatrix( InDeltaRot ).TransformNormal( Poly->Vertex[vertex] - Brush->PrePivot );
				Poly->Base = Brush->PrePivot + FRotationMatrix( InDeltaRot ).TransformNormal( Poly->Base - Brush->PrePivot );

				// Rotate the texture vectors

				Poly->TextureU = FRotationMatrix( InDeltaRot ).TransformNormal( Poly->TextureU );
				Poly->TextureV = FRotationMatrix( InDeltaRot ).TransformNormal( Poly->TextureV );

				// Recalc the normal for the poly

				Poly->Normal = FVector(0,0,0);
				Poly->Finalize(Brush,0);
			}

			Brush->BrushComponent->Brush->BuildBound();

			if( !Brush->IsStaticBrush() )
				GEditor->csgPrepMovingBrush( Brush );

			InActor->ClearComponents();
		}
	}
	else
	{
		FRotator ActorRotWind, ActorRotRem;
		InActor->Rotation.GetWindingAndRemainder(ActorRotWind, ActorRotRem);

		FQuat ActorQ = ActorRotRem.Quaternion();
		FQuat DeltaQ = InDeltaRot.Quaternion();
		FQuat ResultQ = DeltaQ * ActorQ;

		FRotator NewActorRotRem = FRotator( ResultQ );
		FRotator DeltaRot = NewActorRotRem - ActorRotRem;
		DeltaRot.MakeShortestRoute();

		InActor->Rotation += DeltaRot;
	}

	APawn* P = Cast<APawn>( InActor );
	if( P && P->Controller )
		P->Controller->Rotation = InActor->Rotation;

	if( GEditorModeTools.CoordSystem == COORD_World )
	{
		FVector Wk = InActor->Location;

		Wk -= GEditorModeTools.PivotLocation;
		Wk = FRotationMatrix( InDeltaRot ).TransformFVector( Wk );
		Wk += GEditorModeTools.PivotLocation;

		InActor->Location = Wk;
	}

	// **
	// ** Translation
	// **

	InActor->Location += InDeltaDrag;

	// **
	// ** Scale
	// **

	if( !InDeltaScale.IsZero() )
	{
		FScaleMatrix matrix( FVector( InDeltaScale.X / (GEditor->Constraints.GetGridSize() * 100.f), InDeltaScale.Y / (GEditor->Constraints.GetGridSize() * 100.f),InDeltaScale.Z / (GEditor->Constraints.GetGridSize() * 100.f) ) );

		// If the actor is a brush, update the vertices

		if( InActor->IsBrush() )
		{
			ABrush* Brush = (ABrush*)InActor;
			if(Brush->BrushComponent->Brush && Brush->BrushComponent->Brush->Polys)
			{
				for( INT poly = 0 ; poly < Brush->BrushComponent->Brush->Polys->Element.Num() ; poly++ )
				{
					FPoly* Poly = &(Brush->BrushComponent->Brush->Polys->Element(poly));

					FBox bboxBefore(0);
					for( INT vertex = 0 ; vertex < Poly->NumVertices ; vertex++ )
						bboxBefore += Brush->LocalToWorld().TransformFVector( Poly->Vertex[vertex] );

					// Scale the vertices

					for( INT vertex = 0 ; vertex < Poly->NumVertices ; vertex++ )
					{
						FVector Wk = Brush->LocalToWorld().TransformFVector( Poly->Vertex[vertex] );
						Wk -= GEditorModeTools.PivotLocation;
						Wk += matrix.TransformFVector( Wk );
						Wk += GEditorModeTools.PivotLocation;
						Poly->Vertex[vertex] = Brush->WorldToLocal().TransformFVector( Wk );
					}

					FBox bboxAfter(0);
					for( INT vertex = 0 ; vertex < Poly->NumVertices ; vertex++ )
						bboxAfter += Brush->LocalToWorld().TransformFVector( Poly->Vertex[vertex] );

					FVector Wk = Brush->LocalToWorld().TransformFVector( Poly->Base );
					Wk -= GEditorModeTools.PivotLocation;
					Wk += matrix.TransformFVector( Wk );
					Wk += GEditorModeTools.PivotLocation;
					Poly->Base = Brush->WorldToLocal().TransformFVector( Wk );

					// Scale the texture vectors

					for( INT a = 0 ; a < 3 ; ++a )
					{
						FLOAT Before = bboxBefore.GetExtent()[a];
						FLOAT After = bboxAfter.GetExtent()[a];

						if( After != 0.0 )
						{
							FLOAT Pct = Before / After;

							if( Pct != 0.0 )
							{
								Poly->TextureU[a] *= Pct;
								Poly->TextureV[a] *= Pct;
							}
						}
					}

					// Recalc the normal for the poly

					Poly->Normal = FVector(0,0,0);
					Poly->Finalize((ABrush*)InActor,0);
				}

				Brush->BrushComponent->Brush->BuildBound();

				if( !Brush->IsStaticBrush() )
					GEditor->csgPrepMovingBrush( Brush );
			}
		}
		else
		{
			InActor->DrawScale3D += matrix.TransformFVector( InActor->DrawScale3D );

			if( GEditorModeTools.CoordSystem == COORD_World )
			{
				InActor->Location -= GEditorModeTools.PivotLocation;
				InActor->Location += matrix.TransformFVector( InActor->Location );
				InActor->Location += GEditorModeTools.PivotLocation;
			}
		}

		InActor->ClearComponents();
	}

	// Update the actor before leaving

	InActor->InvalidateLightingCache();
	InActor->UpdateComponents();
}

//
//	FEditorLevelViewportClient::MouseMove
//

void FEditorLevelViewportClient::MouseMove(FChildViewport* Viewport,INT x, INT y)
{
	// Editor viewports have to take the focus when they are moused over.  If they don't do this,
	// they won't respond properly to box selection and other drag operations that depend
	// on the state of the keyboard.

	//SetFocus( (HWND)Viewport->GetWindow() );

	// Let the current editor mode know about the mouse movement.

	if( GEditorModeTools.GetCurrentMode()->MouseMove(this, Viewport, x, y) )
		return;
}

//
//	FEditorLevelVIewportClient::GetCursor
//

EMouseCursor FEditorLevelViewportClient::GetCursor(FChildViewport* Viewport,INT X,INT Y)
{
	EMouseCursor MouseCursor = MC_Arrow;
	HHitProxy*	HitProxy = Viewport->GetHitProxy(X,Y);
	EAxis SaveAxis = Widget->CurrentAxis;
	Widget->CurrentAxis = AXIS_None;

	// Change the mouse cursor if the user is hovering over something they can interact with

	if(HitProxy)
	{
		MouseCursor = HitProxy->GetMouseCursor();

		if( HitProxy->IsA(TEXT("HWidgetAxis") ) )
		{
			Widget->CurrentAxis = ((HWidgetAxis*)HitProxy)->Axis;
		}
	}

	GEditorModeTools.GetCurrentMode()->CurrentWidgetAxis = Widget->CurrentAxis;

	// If the current axis on the widget changed, repaint the viewport.

	if( Widget->CurrentAxis != SaveAxis )
	{
		Viewport->InvalidateDisplay();
	}

	return MouseCursor;
}

//
//	FEditorLevelViewportClient::Draw
//

void FEditorLevelViewportClient::Draw(FChildViewport* Viewport,FRenderInterface* RI)
{
	FSceneView	View;
	CalcSceneView(&View);

	// Update the listener.

	UAudioDevice* AudioDevice = GEngine->Client->GetAudioDevice();	
	if( AudioDevice && (ViewportType == LVT_Perspective) && Realtime )
	{
		FMatrix CameraToWorld = View.ViewMatrix.Inverse();

		FVector ProjUp		 = CameraToWorld.TransformNormal(FVector(0,1000,0));
		FVector ProjRight	 = CameraToWorld.TransformNormal(FVector(1000,0,0));
		FVector ProjFront	 = ProjRight ^ ProjUp;

		ProjUp.Z = Abs( ProjUp.Z ); // Don't allow flipping "up".

		ProjUp.Normalize();
		ProjRight.Normalize();
		ProjFront.Normalize();

		AudioDevice->SetListener( ViewLocation, ProjUp, ProjRight, ProjFront );
	}

	FSceneContext	Context(&View,GetScene(),0,0,Viewport->GetSizeX(),Viewport->GetSizeY());

	// If desired, constrain the aspect ratio of the viewport. Will insert black bars etc.
	if((ViewportType == LVT_Perspective) && bConstrainAspectRatio)
	{
		Context.FixAspectRatio( AspectRatio );

		// We make sure the background is black because some of it will show through (black bars).
		RI->Clear(FColor(0,0,0));
	}

	if(bEnableFading)
	{
		View.OverlayColor = FadeColor;
		View.OverlayColor.A = Clamp(appFloor(FadeAmount*256),0,255);
	}

	RI->DrawScene(Context);

	GEditorModeTools.GetCurrentMode()->DrawHUD(this,Viewport,Context,RI);

	// Axes indicators

	if(bDrawAxes && ViewportType == LVT_Perspective)
	{
		DrawAxes(Viewport, RI);
	}

	// Information string

	RI->DrawShadowedString( 4,4, *GEditorModeTools.InfoString, GEngine->SmallFont, FColor(255,255,255) );

	// If we are the current viewport, draw a highlighted border around the outer edge

	FColor BorderColor(0,0,0);
	if( GCurrentLevelEditingViewportClient == this )
	{
		BorderColor = FColor(255,255,0);
	}

	for( INT c = 0 ; c < 3 ; ++c )
	{
		INT X = 1+c,
			Y = 1+c,
			W = Viewport->GetSizeX()-c,
			H = Viewport->GetSizeY()-c;

		RI->DrawLine2D( X,Y, W,Y, BorderColor );
		RI->DrawLine2D( W,Y, W,H, BorderColor );
		RI->DrawLine2D( W,H, X,H, BorderColor );
		RI->DrawLine2D( X,H, X,Y-1, BorderColor );
	}

	// Kismet references

	ULevel* Level = GetLevel();
	if(Level)
	{
		if((View.ShowFlags & SHOW_KismetRefs) && Level->GameSequence)
		{
			DrawKismetRefs(Level->GameSequence, Viewport, &Context, RI);
		}

		// Possibly a hack. Lines may get added without the scene being rendered etc.
		Level->LineBatcher->BatchedLines.Empty();
	}
}

/** Draw screen-space box around each Actor in the level referenced by the Kismet sequence. */
void FEditorLevelViewportClient::DrawKismetRefs(USequence* Seq, FChildViewport* Viewport, FSceneContext* Context, FRenderInterface* RI)
{
	// Get all SeqVar_Objects and SequenceEvents from the Sequence (and all subsequences).
	TArray<USequenceObject*> SeqObjVars;
	Seq->FindSeqObjectsByClass( USeqVar_Object::StaticClass(), SeqObjVars );
	Seq->FindSeqObjectsByClass( USequenceEvent::StaticClass(), SeqObjVars );

	TArray<AActor*>	DrawnActors;

	for(INT i=0; i<SeqObjVars.Num(); i++)
	{
		AActor* RefActor = NULL;

		USeqVar_Object* VarObj = Cast<USeqVar_Object>( SeqObjVars(i) );
		if(VarObj)
		{
			RefActor = Cast<AActor>(VarObj->ObjValue);
		}

		USequenceEvent* EventObj = Cast<USequenceEvent>( SeqObjVars(i) );
		if(EventObj)
		{
			RefActor = EventObj->Originator;
		}

		// If this is a refence to an Actor that we haven't drawn yet, draw it now.
		if(RefActor && !DrawnActors.ContainsItem(RefActor))
		{
			// If this actor 
			USpriteComponent* Sprite = NULL;
			for(INT j=0; j<RefActor->Components.Num() && !Sprite; j++)
			{
				Sprite = Cast<USpriteComponent>( RefActor->Components(j) );
			}

			FBox ActorBox;
			if(Sprite)
			{
				ActorBox = Sprite->Bounds.GetBox();
			}
			else
			{
				ActorBox = RefActor->GetComponentsBoundingBox(true);
			}

			// If we didn't get a valid bounding box, just make a little one around the actor location
			if(!ActorBox.IsValid || ActorBox.GetExtent().GetMin() < KINDA_SMALL_NUMBER )
			{
				ActorBox = FBox(RefActor->Location - FVector(-20), RefActor->Location + FVector(20));
			}

			FVector BoxCenter, BoxExtents;
			ActorBox.GetCenterAndExtents(BoxCenter, BoxExtents);

			// Project center of bounding box onto screen.
			FPlane ProjBoxCenter = Context->View->Project(BoxCenter);

			// Do nothing if behind camera
			if(ProjBoxCenter.W > 0.f)
			{
				// Project verts of world-space bounding box onto screen and take their bounding box
				FVector Verts[8];
				Verts[0] = FVector( 1, 1, 1);
				Verts[1] = FVector( 1, 1,-1);
				Verts[2] = FVector( 1,-1, 1);
				Verts[3] = FVector( 1,-1,-1);
				Verts[4] = FVector(-1, 1, 1);
				Verts[5] = FVector(-1, 1,-1);
				Verts[6] = FVector(-1,-1, 1);
				Verts[7] = FVector(-1,-1,-1);

				INT HalfX = 0.5f * Viewport->GetSizeX();
				INT HalfY = 0.5f * Viewport->GetSizeY();

				FIntPoint ScreenBoxMin(1000000000, 1000000000);
				FIntPoint ScreenBoxMax(-1000000000, -1000000000);

				for(INT j=0; j<8; j++)
				{
					// Project vert into screen space.
					FVector WorldVert = BoxCenter + (Verts[j]*BoxExtents);
					FPlane ProjVert = Context->View->Project(WorldVert);

					FIntPoint VertPos;
					VertPos.X = HalfX + ( HalfX * ProjVert.X );
					VertPos.Y = HalfY + ( HalfY * (ProjVert.Y * -1.f) );

					// Update screen-space bounding box with with transformed vert.
					ScreenBoxMin.X = ::Min<INT>(ScreenBoxMin.X, VertPos.X);
					ScreenBoxMin.Y = ::Min<INT>(ScreenBoxMin.Y, VertPos.Y);

					ScreenBoxMax.X = ::Max<INT>(ScreenBoxMax.X, VertPos.X);
					ScreenBoxMax.Y = ::Max<INT>(ScreenBoxMax.Y, VertPos.Y);
				}

				// Draw box on the screen.
				FColor KismetRefColor(175, 255, 162);

				RI->DrawLine2D( ScreenBoxMin.X, ScreenBoxMin.Y, ScreenBoxMin.X, ScreenBoxMax.Y, KismetRefColor );
				RI->DrawLine2D( ScreenBoxMin.X, ScreenBoxMax.Y, ScreenBoxMax.X, ScreenBoxMax.Y, KismetRefColor );
				RI->DrawLine2D( ScreenBoxMax.X, ScreenBoxMax.Y, ScreenBoxMax.X, ScreenBoxMin.Y, KismetRefColor );
				RI->DrawLine2D( ScreenBoxMax.X, ScreenBoxMin.Y, ScreenBoxMin.X, ScreenBoxMin.Y, KismetRefColor );
			}

			// Not not to draw this actor again.
			DrawnActors.AddItem(RefActor);
		}
	}
}


void FEditorLevelViewportClient::DrawAxes(FChildViewport* Viewport, FRenderInterface* RI, const FRotator* InRotation)
{
	FRotationMatrix ViewTM( this->ViewRotation );

	if( InRotation )
	{
		ViewTM = FRotationMatrix( *InRotation );
	}

	INT SizeX = Viewport->GetSizeX();
	INT SizeY = Viewport->GetSizeY();

	FIntPoint AxisOrigin( 30, SizeY - 30 );
	FLOAT AxisSize = 25.f;

	INT XL, YL;
	RI->StringSize(GEngine->SmallFont, XL, YL, TEXT("Z"));

	FVector AxisVec = AxisSize * ViewTM.InverseTransformNormal( FVector(1,0,0) );
	FIntPoint AxisEnd = AxisOrigin + FIntPoint( AxisVec.Y, -AxisVec.Z );
	RI->DrawLine2D( AxisOrigin.X, AxisOrigin.Y, AxisEnd.X, AxisEnd.Y, FColor(255,0,0) );
	RI->DrawString( AxisEnd.X + 2, AxisEnd.Y - 0.5*YL, TEXT("X"), GEngine->SmallFont, FColor(255,0,0) );

	AxisVec = AxisSize * ViewTM.InverseTransformNormal( FVector(0,1,0) );
	AxisEnd = AxisOrigin + FIntPoint( AxisVec.Y, -AxisVec.Z );
	RI->DrawLine2D( AxisOrigin.X, AxisOrigin.Y, AxisEnd.X, AxisEnd.Y, FColor(0,255,0) );
	RI->DrawString( AxisEnd.X + 2, AxisEnd.Y - 0.5*YL, TEXT("Y"), GEngine->SmallFont, FColor(0,255,0) );

	AxisVec = AxisSize * ViewTM.InverseTransformNormal( FVector(0,0,1) );
	AxisEnd = AxisOrigin + FIntPoint( AxisVec.Y, -AxisVec.Z );
	RI->DrawLine2D( AxisOrigin.X, AxisOrigin.Y, AxisEnd.X, AxisEnd.Y, FColor(0,0,255) );
	RI->DrawString( AxisEnd.X + 2, AxisEnd.Y - 0.5*YL, TEXT("Z"), GEngine->SmallFont, FColor(0,0,255) );
}

//
//	FEditorLevelViewportClient::GetScene
//

FScene* FEditorLevelViewportClient::GetScene()
{
	return GEditor->Level;
}

//
//	FEditorLevelViewportClient::GetLevel
//

ULevel* FEditorLevelViewportClient::GetLevel()
{
	return GEditor->Level;
}

//
//	FEditorLevelViewportClient::GetBackgroundColor
//

FColor FEditorLevelViewportClient::GetBackgroundColor()
{
	return (ViewportType == LVT_Perspective) ? GEditor->C_WireBackground : GEditor->C_OrthoBackground;
}

//
//	UEditorViewportInput::Exec
//

UBOOL UEditorViewportInput::Exec(const TCHAR* Cmd,FOutputDevice& Ar)
{
	if(CurrentEvent == IE_Pressed || (CurrentEvent == IE_Released && ParseCommand(&Cmd,TEXT("OnRelease"))))
	{
		if(Editor->Exec(Cmd,Ar))
			return 1;
	}

	return Super::Exec(Cmd,Ar);

}

IMPLEMENT_CLASS(UEditorViewportInput);
