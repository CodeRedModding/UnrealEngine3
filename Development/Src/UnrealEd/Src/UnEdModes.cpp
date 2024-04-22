/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEd.h"
#include "UnTerrain.h"
#include "MeshPaintEdMode.h"
#include "MouseDeltaTracker.h"
#include "ScopedTransaction.h"
#include "SurfaceIterators.h"
#include "EngineSplineClasses.h"
#include "EngineSoundClasses.h"
#include "EngineProcBuildingClasses.h"
#include "SplineEdit.h"
#include "LandscapeEdMode.h"
#include "FoliageEdMode.h"
#include "LevelBrowser.h"

/*------------------------------------------------------------------------------
    Base class.
------------------------------------------------------------------------------*/

FEdMode::FEdMode():
	Desc( TEXT("N/A") ),
	BitmapOn( NULL ),
	BitmapOff( NULL ),
	CurrentTool( NULL ),
	Settings( NULL ),
	ID( EM_None )
{
	Component = ConstructObject<UEdModeComponent>(UEdModeComponent::StaticClass());
}

FEdMode::~FEdMode()
{
	delete Settings;
}

UBOOL FEdMode::MouseMove(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,INT x, INT y)
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->MouseMove( ViewportClient, Viewport, x, y );
	}

	return 0;
}


/**
 * Called when the mouse is moved while a window input capture is in effect
 *
 * @param	InViewportClient	Level editor viewport client that captured the mouse input
 * @param	InViewport			Viewport that captured the mouse input
 * @param	InMouseX			New mouse cursor X coordinate
 * @param	InMouseY			New mouse cursor Y coordinate
 *
 * @return	TRUE if input was handled
 */
UBOOL FEdMode::CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY )
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->CapturedMouseMove( InViewportClient, InViewport, InMouseX, InMouseY );
	}

	return FALSE;
}


UBOOL FEdMode::InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event)
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->InputKey( ViewportClient, Viewport, Key, Event );
	}
	else
	{
		// HACK:  Pass input up to selected actors if not in a tool mode
		TArray<AActor*> SelectedActors;
		GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);

		for( TArray<AActor*>::TIterator it(SelectedActors); it; ++it )
		{
			// Tell the object we've had a key press
			(*it)->OnEditorKeyPressed(Key, Event);
		}
	}

	return 0;
}

UBOOL FEdMode::InputAxis(FEditorLevelViewportClient* InViewportClient,FViewport* Viewport,INT ControllerId,FName Key,FLOAT Delta,FLOAT DeltaTime)
{
	FModeTool* Tool = GetCurrentTool();
	if (Tool)
	{
		return Tool->InputAxis(InViewportClient, Viewport, ControllerId, Key, Delta, DeltaTime);
	}

	return FALSE;
}

UBOOL FEdMode::InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale)
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->InputDelta(InViewportClient,InViewport,InDrag,InRot,InScale);
	}

	return 0;
}

/**
 * Lets each tool determine if it wants to use the editor widget or not.  If the tool doesn't want to use it, it will be
 * fed raw mouse delta information (not snapped or altered in any way).
 */

UBOOL FEdMode::UsesTransformWidget() const
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->UseWidget();
	}

	return 1;
}

/**
 * Allows each mode/tool to determine a good location for the widget to be drawn at.
 */

FVector FEdMode::GetWidgetLocation() const
{
	//debugf(TEXT("In FEdMode::GetWidgetLocation"));
	return GEditorModeTools().PivotLocation;
}

/**
 * Lets the mode determine if it wants to draw the widget or not.
 */

UBOOL FEdMode::ShouldDrawWidget() const
{
	return (GEditor->GetSelectedActors()->GetTop<AActor>() != NULL);
}

/**
 * Allows each mode to customize the axis pieces of the widget they want drawn.
 *
 * @param	InwidgetMode	The current widget mode
 *
 * @return	A bitfield comprised of AXIS_ values
 */

INT FEdMode::GetWidgetAxisToDraw( FWidget::EWidgetMode InWidgetMode ) const
{
	return AXIS_XYZ;
}

/**
 * Lets each mode/tool handle box selection in its own way.
 *
 * @param	InBox	The selection box to use, in worldspace coordinates.
 * @return		TRUE if something was selected/deselected, FALSE otherwise.
 */
UBOOL FEdMode::BoxSelect( FBox& InBox, UBOOL InSelect )
{
	UBOOL bResult = FALSE;
	if( GetCurrentTool() )
	{
		bResult = GetCurrentTool()->BoxSelect( InBox, InSelect );
	}
	return bResult;
}

/**
 * Lets each mode/tool handle frustum selection in its own way.
 *
 * @param	InFrustum	The selection frustum to use, in worldspace coordinates.
 * @return	TRUE if something was selected/deselected, FALSE otherwise.
 */
UBOOL FEdMode::FrustumSelect( const FConvexVolume& InFrustum, UBOOL InSelect )
{
	UBOOL bResult = FALSE;
	if( GetCurrentTool() )
	{
		bResult = GetCurrentTool()->FrustumSelect( InFrustum, InSelect );
	}
	return bResult;
}

void FEdMode::SelectNone()
{
	if( GetCurrentTool() )
	{
		GetCurrentTool()->SelectNone();
	}
}

void FEdMode::Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime)
{
	if( GetCurrentTool() )
	{
		GetCurrentTool()->Tick(ViewportClient,DeltaTime);
	}
}

void FEdMode::ClearComponent()
{
	check(Component);
	Component->ConditionalDetach();
}

void FEdMode::UpdateComponent()
{
	check(Component);
	Component->ConditionalAttach(GWorld->Scene,NULL,FMatrix::Identity);
}

void FEdMode::Enter()
{
	UpdateComponent();

	// Update components for selected actors, in case the mode we just exited
	// was hijacking selection events selection and not updating components.
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* SelectedActor = static_cast<AActor*>( *It );
		checkSlow( SelectedActor->IsA(AActor::StaticClass()) );
		SelectedActor->ForceUpdateComponents( FALSE, FALSE );
	}

	// Update the mode bar if needed
	GCallbackEvent->Send(CALLBACK_EditorModeEnter,this);
}

void FEdMode::Exit()
{
	ClearComponent();
	// Save any mode bar data if needed
	GCallbackEvent->Send(CALLBACK_EditorModeExit,this);
}

void FEdMode::SetCurrentTool( EModeTools InID )
{
	CurrentTool = FindTool( InID );
	check( CurrentTool );	// Tool not found!  This can't happen.

	CurrentToolChanged();
}

void FEdMode::SetCurrentTool( FModeTool* InModeTool )
{
	CurrentTool = InModeTool;
	check(CurrentTool);

	CurrentToolChanged();
}

FModeTool* FEdMode::FindTool( EModeTools InID )
{
	for( INT x = 0 ; x < Tools.Num() ; ++x )
	{
		if( Tools(x)->GetID() == InID )
		{
			return Tools(x);
		}
	}

	appErrorf(LocalizeSecure(LocalizeUnrealEd("Error_FailedToFindTool"),(INT)InID));
	return NULL;
}

const FToolSettings* FEdMode::GetSettings() const
{
	return Settings;
}

/** Material proxy wrapper that can be created on the game thread and passed on to the render thread. */
class FDynamicColoredMaterialRenderProxy : public FDynamicPrimitiveResource, public FColoredMaterialRenderProxy
{
public:
	/** Initialization constructor. */
	FDynamicColoredMaterialRenderProxy(const FMaterialRenderProxy* InParent,const FLinearColor& InColor)
	:	FColoredMaterialRenderProxy(InParent,InColor)
	{
	}
	virtual ~FDynamicColoredMaterialRenderProxy()
	{
	}

	// FDynamicPrimitiveResource interface.
	virtual void InitPrimitiveResource()
	{
	}
	virtual void ReleasePrimitiveResource()
	{
		delete this;
	}
};

void FEdMode::Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI)
{
	if( GEditor->bShowBrushMarkerPolys )
	{
		// Draw translucent polygons on brushes and volumes

		for( FActorIterator It; It; ++ It )
		{
			ABrush* Brush = Cast<ABrush>( *It );

			// Brush->Brush is checked to safe from brushes that were created without having their brush members attached.
			if( Brush && Brush->Brush && (Brush->IsABuilderBrush() || Brush->IsVolumeBrush()) && GEditor->GetSelectedActors()->IsSelected(Brush) )
			{
				// Build a mesh by basically drawing the triangles of each 
				FDynamicMeshBuilder MeshBuilder;
				INT VertexOffset = 0;

				for( INT PolyIdx = 0 ; PolyIdx < Brush->Brush->Polys->Element.Num() ; ++PolyIdx )
				{
					const FPoly* Poly = &Brush->Brush->Polys->Element(PolyIdx);

					if( Poly->Vertices.Num() > 2 )
					{
						const FVector Vertex0 = Poly->Vertices(0);
						FVector Vertex1 = Poly->Vertices(1);

						MeshBuilder.AddVertex(Vertex0, FVector2D(0,0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
						MeshBuilder.AddVertex(Vertex1, FVector2D(0,0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));

						for( INT VertexIdx = 2 ; VertexIdx < Poly->Vertices.Num() ; ++VertexIdx )
						{
							const FVector Vertex2 = Poly->Vertices(VertexIdx);
							MeshBuilder.AddVertex(Vertex2, FVector2D(0,0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
							MeshBuilder.AddTriangle(VertexOffset,VertexOffset + VertexIdx,VertexOffset+VertexIdx-1);
							Vertex1 = Vertex2;
						}

						// Increment the vertex offset so the next polygon uses the correct vertex indices.
						VertexOffset += Poly->Vertices.Num();
					}
				}

				// Allocate the material proxy and register it so it can be deleted properly once the rendering is done with it.
				FDynamicColoredMaterialRenderProxy* MaterialProxy = new FDynamicColoredMaterialRenderProxy(GEngine->EditorBrushMaterial->GetRenderProxy(FALSE),Brush->GetWireColor());
				PDI->RegisterDynamicResource( MaterialProxy );

				// Flush the mesh triangles.
				MeshBuilder.Draw(PDI, Brush->LocalToWorld(), MaterialProxy, SDPG_World, 0.f);
			}
		}
	}

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		// Draw building debug info
		// TODO: Would be nice to make this generic as part of Actor

		AProcBuilding* Building = Cast<AProcBuilding>(*It);
		if(Building)
		{
			if(Building->bDebugDrawEdgeInfo)
			{
				Building->DrawDebugEdgeInfo(View, Viewport, PDI);
			}
				
			if(Building->bDebugDrawScopes)
			{
				Building->DrawDebugScopes(View, Viewport, PDI);
			}
		}
	}

	// Let the current mode tool render if it wants to
	FModeTool* tool = GetCurrentTool();
	if( tool )
	{
		tool->Render( View, Viewport, PDI );
	}

	AGroupActor::DrawBracketsForGroups(PDI, Viewport);
}

void FEdMode::DrawHUD(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas)
{
	// Render the drag tool.
	ViewportClient->MouseDeltaTracker->RenderDragTool( View, Canvas );

	// Let the current mode tool draw a HUD if it wants to
	FModeTool* tool = GetCurrentTool();
	if( tool )
	{
		tool->DrawHUD( ViewportClient, Viewport, View, Canvas );
	}

	if (ViewportClient->IsPerspective() && GEditorModeTools().GetHighlightWithBrackets() )
	{
		DrawBrackets( ViewportClient, Viewport, View, Canvas );
	}

	// If this viewport doesn't show mode widgets or the mode itself doesn't want them, leave.
	if( !(ViewportClient->ShowFlags&SHOW_ModeWidgets) || !ShowModeWidgets() )
	{
		return;
	}

	// Clear Hit proxies
	const UBOOL bIsHitTesting = Canvas->IsHitTesting();
	if ( !bIsHitTesting )
	{
		Canvas->SetHitProxy(NULL);
	}

	// Draw vertices for selected BSP brushes and static meshes if the large vertices show flag is set.
	if ( !ViewportClient->bDrawVertices )
	{
		return;
	}

	const UBOOL bLargeVertices		= View->Family->ShowFlags & SHOW_LargeVertices ? TRUE : FALSE;
	const UBOOL bShowBrushes		= (View->Family->ShowFlags & SHOW_Brushes) ? TRUE : FALSE;
	const UBOOL bShowBSP			= (View->Family->ShowFlags & SHOW_BSP) ? TRUE : FALSE;
	const UBOOL bShowBuilderBrush	= (View->Family->ShowFlags & SHOW_BuilderBrush) ? TRUE : FALSE;

	UTexture2D* VertexTexture		= GetVertexTexture();
	const FLOAT TextureSizeX		= VertexTexture->SizeX * ( bLargeVertices ? 1.0f : 0.5f );
	const FLOAT TextureSizeY		= VertexTexture->SizeY * ( bLargeVertices ? 1.0f : 0.5f );

	// Temporaries.
	TArray<FVector> Vertices;

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* SelectedActor = static_cast<AActor*>( *It );
		checkSlow( SelectedActor->IsA(AActor::StaticClass()) );

		if ( SelectedActor->IsABrush() && (bShowBrushes || bShowBSP) )
		{
			// Check to see if the current mode wants brush vertices drawn

			if( GEditorModeTools().ShouldDrawBrushVertices() )
			{
				ABrush* Brush = static_cast<ABrush*>( SelectedActor );
				if ( Brush->Brush )
				{
					// Don't render builder brush vertices if the builder brush show flag is disabled.
					if( !bShowBuilderBrush && Brush->IsABuilderBrush() )
					{
						continue;
					}

					for( INT p = 0 ; p < Brush->Brush->Polys->Element.Num() ; ++p )
					{
						FPoly* poly = &Brush->Brush->Polys->Element(p);
						for( INT VertexIndex = 0 ; VertexIndex < poly->Vertices.Num() ; ++VertexIndex )
						{
							const FVector& PolyVertex	= poly->Vertices(VertexIndex);
							const FVector vtx			= Brush->LocalToWorld().TransformFVector( PolyVertex );
							FVector2D PixelLocation;
							if(View->ScreenToPixel(View->WorldToScreen(vtx),PixelLocation))
							{
								const UBOOL bOutside =
									PixelLocation.X < 0.0f || PixelLocation.X > View->SizeX ||
									PixelLocation.Y < 0.0f || PixelLocation.Y > View->SizeY;
								if ( !bOutside )
								{
									const FLOAT X = PixelLocation.X - (TextureSizeX/2);
									const FLOAT Y = PixelLocation.Y - (TextureSizeY/2);
									const FColor Color( Brush->GetWireColor() );
									if ( bIsHitTesting ) Canvas->SetHitProxy( new HBSPBrushVert(Brush,&poly->Vertices(VertexIndex)) );
									DrawTile( Canvas, X, Y, TextureSizeX, TextureSizeY, 0.f, 0.f, 1.f, 1.f, Color, VertexTexture->Resource );
									if ( bIsHitTesting ) Canvas->SetHitProxy( NULL );
								}
							}
						}
					}
				}
			}
		}
#if !SHIPPING_PC_GAME || UDK
		// Not supported on shipped builds because PC cooking strips raw mesh data.
		else if( bLargeVertices )
		{
			// Static mesh vertices
			AStaticMeshActor* Actor = Cast<AStaticMeshActor>( SelectedActor );
			if( Actor && Actor->StaticMeshComponent && Actor->StaticMeshComponent->StaticMesh )
			{
				Vertices.Empty();
				const FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*) Actor->StaticMeshComponent->StaticMesh->LODModels(0).RawTriangles.Lock(LOCK_READ_ONLY);
				for( INT tri = 0 ; tri < Actor->StaticMeshComponent->StaticMesh->LODModels(0).RawTriangles.GetElementCount() ; tri++ )
				{
					const FStaticMeshTriangle* smt = &RawTriangleData[tri];
					Vertices.AddUniqueItem( Actor->LocalToWorld().TransformFVector( smt->Vertices[0] ) );
					Vertices.AddUniqueItem( Actor->LocalToWorld().TransformFVector( smt->Vertices[1] ) );
					Vertices.AddUniqueItem( Actor->LocalToWorld().TransformFVector( smt->Vertices[2] ) );
				}
				Actor->StaticMeshComponent->StaticMesh->LODModels(0).RawTriangles.Unlock();

				for( INT VertexIndex = 0 ; VertexIndex < Vertices.Num() ; ++VertexIndex )
				{				
					const FVector& Vertex = Vertices(VertexIndex);
					FVector2D PixelLocation;
					if(View->ScreenToPixel(View->WorldToScreen(Vertex),PixelLocation))
					{
						const UBOOL bOutside =
							PixelLocation.X < 0.0f || PixelLocation.X > View->SizeX ||
							PixelLocation.Y < 0.0f || PixelLocation.Y > View->SizeY;
						if ( !bOutside )
						{
							const FLOAT X = PixelLocation.X - (TextureSizeX/2);
							const FLOAT Y = PixelLocation.Y - (TextureSizeY/2);
							if ( bIsHitTesting ) Canvas->SetHitProxy( new HStaticMeshVert(Actor,Vertex) );
							DrawTile( Canvas, X, Y, TextureSizeX, TextureSizeY, 0.f, 0.f, 1.f, 1.f, FLinearColor::White, VertexTexture->Resource );
							if ( bIsHitTesting ) Canvas->SetHitProxy( NULL );
						}
					}
				}
			}
		}
#endif // SHIPPING_PC_GAME
	}
}

// Draw brackets around all selected objects
void FEdMode::DrawBrackets( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas )
{
	USelection& SelectedActors = *GEditor->GetSelectedActors();
	for( INT CurSelectedActorIndex = 0; CurSelectedActorIndex < SelectedActors.Num(); ++CurSelectedActorIndex )
	{
		AActor* SelectedActor = Cast<AActor>( SelectedActors( CurSelectedActorIndex ) );
		if( SelectedActor != NULL )
		{
			// Draw a bracket for selected "paintable" static mesh actors
			const UBOOL bIsValidActor =
				( Cast< AStaticMeshActor >( SelectedActor ) != NULL || 
				Cast< ADynamicSMActor >( SelectedActor ) != NULL );

			const FLinearColor SelectedActorBoxColor( 0.6f, 0.6f, 1.0f );
			const UBOOL bDrawBracket = bIsValidActor;
			ViewportClient->DrawActorScreenSpaceBoundingBox( Canvas, View, Viewport, SelectedActor, SelectedActorBoxColor, bDrawBracket );
		}
	}
}

UBOOL FEdMode::StartTracking()
{
	UBOOL bResult = FALSE;
	if( GetCurrentTool() )
	{
		bResult = GetCurrentTool()->StartModify();
	}
	return bResult;
}

UBOOL FEdMode::EndTracking()
{
	UBOOL bResult = FALSE;
	if( GetCurrentTool() )
	{
		bResult = GetCurrentTool()->EndModify();
	}
	return bResult;
}

FVector FEdMode::GetWidgetNormalFromCurrentAxis( void* InData )
{
	// Figure out the proper coordinate system.

	FMatrix matrix = FMatrix::Identity;
	if( GEditorModeTools().CoordSystem == COORD_Local )
	{
		GetCustomDrawingCoordinateSystem( matrix, InData );
	}

	// Get a base normal from the current axis.

	FVector BaseNormal(1,0,0);		// Default to X axis
	switch( CurrentWidgetAxis )
	{
		case AXIS_Y:	BaseNormal = FVector(0,1,0);	break;
		case AXIS_Z:	BaseNormal = FVector(0,0,1);	break;
		case AXIS_XY:	BaseNormal = FVector(1,1,0);	break;
		case AXIS_XZ:	BaseNormal = FVector(1,0,1);	break;
		case AXIS_YZ:	BaseNormal = FVector(0,1,1);	break;
		case AXIS_XYZ:	BaseNormal = FVector(1,1,1);	break;
	}

	// Transform the base normal into the proper coordinate space.
	return matrix.TransformFVector( BaseNormal );
}

/*------------------------------------------------------------------------------
    Default.
------------------------------------------------------------------------------*/

FEdModeDefault::FEdModeDefault()
{
	ID = EM_Default;
	Desc = TEXT("Default");
}

/*------------------------------------------------------------------------------
    Geometry Editing.
------------------------------------------------------------------------------*/

FEdModeGeometry::FEdModeGeometry()
{
	ID = EM_Geometry;
	Desc = TEXT("Geometry Editing");

	Tools.AddItem( new FModeTool_GeometryModify() );
	SetCurrentTool( MT_GeometryModify );

	Settings = new FGeometryToolSettings;
}

FEdModeGeometry::~FEdModeGeometry()
{
	for( INT i=0; i<GeomObjects.Num(); i++ )
	{
		FGeomObject* GeomObject	= GeomObjects(i);
		delete GeomObject;
	}
	GeomObjects.Empty();
}

void FEdModeGeometry::Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View,Viewport,PDI);

	RenderVertex( View, PDI );
	RenderEdge( View, PDI );
	RenderPoly( View, Viewport, PDI );
}

UBOOL FEdModeGeometry::ShowModeWidgets() const
{
	return 1;
}

UBOOL FEdModeGeometry::ShouldDrawBrushWireframe( AActor* InActor ) const
{
	checkSlow( InActor );

	// If the actor isn't selected, we don't want to interfere with it's rendering.
	if( !GEditor->GetSelectedActors()->IsSelected( InActor ) )
	{
		return TRUE;
	}

	return TRUE;//FALSE;
}

UBOOL FEdModeGeometry::GetCustomDrawingCoordinateSystem( FMatrix& InMatrix, void* InData )
{
	if( GetSelectionState() == GSS_None )
	{
		return 0;
	}

	if( InData )
	{
		InMatrix = FRotationMatrix( ((FGeomBase*)InData)->GetNormal().Rotation() );
	}
	else
	{
		// If we don't have a specific geometry object to get the normal from
		// use the one that was last selected.

		for( INT o = 0 ; o < GeomObjects.Num() ; ++o )
		{
			FGeomObject* go = GeomObjects(o);
			go->CompileSelectionOrder();

			if( go->SelectionOrder.Num() )
			{
				InMatrix = FRotationMatrix( go->SelectionOrder( go->SelectionOrder.Num()-1 )->GetNormal().Rotation() );
				return 1;
			}
		}
	}

	return 0;
}

UBOOL FEdModeGeometry::GetCustomInputCoordinateSystem( FMatrix& InMatrix, void* InData )
{
	return GetCustomDrawingCoordinateSystem( InMatrix, InData );
}

void FEdModeGeometry::Enter()
{
	FEdMode::Enter();

	GetFromSource();
}

void FEdModeGeometry::Exit()
{
	FEdMode::Exit();

	for( INT i=0; i<GeomObjects.Num(); i++ )
	{
		FGeomObject* GeomObject	= GeomObjects(i);
		delete GeomObject;
	}
	GeomObjects.Empty();
}

void FEdModeGeometry::ActorSelectionChangeNotify()
{
	GetFromSource();
}

void FEdModeGeometry::MapChangeNotify()
{
	// If the map changes in some major way, just refresh all the geometry data.
	GetFromSource();
}

void FEdModeGeometry::Serialize( FArchive &Ar )
{
	// Call parent implementation
	FEdMode::Serialize( Ar );

	FModeTool_GeometryModify* mtgm = (FModeTool_GeometryModify*)FindTool( MT_GeometryModify );
	for( FModeTool_GeometryModify::TModifierIterator Itor( mtgm->ModifierIterator() ) ; Itor ; ++Itor )
	{
		Ar << *Itor;
	}
}

/**
* Returns the number of objects that are selected.
*/

INT FEdModeGeometry::CountObjectsSelected()
{
	return GeomObjects.Num();
}

/**
* Returns the number of polygons that are selected.
*/

INT FEdModeGeometry::CountSelectedPolygons()
{
	INT Count = 0;

	for( INT ObjectIdx = 0 ; ObjectIdx < GeomObjects.Num() ; ++ObjectIdx )
	{
		FGeomObject* GeomObject = GeomObjects(ObjectIdx);

		for( INT P = 0 ; P < GeomObject->PolyPool.Num() ; ++P )
		{
			if( GeomObject->PolyPool(P).IsSelected() )
			{
				Count++;
			}
		}
	}

	return Count;
}

/**
* Returns the polygons that are selected.
*
* @param	InPolygons	An array to fill with the selected polygons.
*/

void FEdModeGeometry::GetSelectedPolygons( TArray<FGeomPoly*>& InPolygons )
{
	for( INT ObjectIdx = 0 ; ObjectIdx < GeomObjects.Num() ; ++ObjectIdx )
	{
		FGeomObject* GeomObject = GeomObjects(ObjectIdx);

		for( INT P = 0 ; P < GeomObject->PolyPool.Num() ; ++P )
		{
			if( GeomObject->PolyPool(P).IsSelected() )
			{
				InPolygons.AddItem( &GeomObject->PolyPool(P) );
			}
		}
	}
}

/**
* Returns TRUE if the user has polygons selected.
*/

UBOOL FEdModeGeometry::HavePolygonsSelected()
{
	for( INT ObjectIdx = 0 ; ObjectIdx < GeomObjects.Num() ; ++ObjectIdx )
	{
		FGeomObject* GeomObject = GeomObjects(ObjectIdx);

		for( INT P = 0 ; P < GeomObject->PolyPool.Num() ; ++P )
		{
			if( GeomObject->PolyPool(P).IsSelected() )
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/**
* Returns the number of edges that are selected.
*/

INT FEdModeGeometry::CountSelectedEdges()
{
	INT Count = 0;

	for( INT ObjectIdx = 0 ; ObjectIdx < GeomObjects.Num() ; ++ObjectIdx )
	{
		FGeomObject* GeomObject = GeomObjects(ObjectIdx);

		for( INT E = 0 ; E < GeomObject->EdgePool.Num() ; ++E )
		{
			if( GeomObject->EdgePool(E).IsSelected() )
			{
				Count++;
			}
		}
	}

	return Count;
}

/**
* Returns TRUE if the user has edges selected.
*/

UBOOL FEdModeGeometry::HaveEdgesSelected()
{
	for( INT ObjectIdx = 0 ; ObjectIdx < GeomObjects.Num() ; ++ObjectIdx )
	{
		FGeomObject* GeomObject = GeomObjects(ObjectIdx);

		for( INT E = 0 ; E < GeomObject->EdgePool.Num() ; ++E )
		{
			if( GeomObject->EdgePool(E).IsSelected() )
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/**
* Returns the edges that are selected.
*
* @param	InEdges	An array to fill with the selected edges.
*/
void FEdModeGeometry::GetSelectedEdges( TArray<FGeomEdge*>& InEdges )
{
	for( INT ObjectIdx = 0 ; ObjectIdx < GeomObjects.Num() ; ++ObjectIdx )
	{
		FGeomObject* GeomObject = GeomObjects(ObjectIdx);

		for( INT E = 0 ; E < GeomObject->EdgePool.Num() ; ++E )
		{
			if( GeomObject->EdgePool(E).IsSelected() )
			{
				InEdges.AddItem( &GeomObject->EdgePool(E) );
			}
		}
	}
}

/**
* Returns the number of vertices that are selected.
*/

INT FEdModeGeometry::CountSelectedVertices()
{
	INT Count = 0;

	for( INT ObjectIdx = 0 ; ObjectIdx < GeomObjects.Num() ; ++ObjectIdx )
	{
		FGeomObject* GeomObject = GeomObjects(ObjectIdx);

		for( INT V = 0 ; V < GeomObject->VertexPool.Num() ; ++V )
		{
			if( GeomObject->VertexPool(V).IsSelected() )
			{
				Count++;
			}
		}
	}

	return Count;
}

/**
* Returns TRUE if the user has vertices selected.
*/

UBOOL FEdModeGeometry::HaveVerticesSelected()
{
	for( INT ObjectIdx = 0 ; ObjectIdx < GeomObjects.Num() ; ++ObjectIdx )
	{
		FGeomObject* GeomObject = GeomObjects(ObjectIdx);

		for( INT V = 0 ; V < GeomObject->VertexPool.Num() ; ++V )
		{
			if( GeomObject->VertexPool(V).IsSelected() )
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/**
* Fills an array with all selected vertices.
*
* @param	InVerts		An array to fill with the unique list of selected vertices.
*/
void FEdModeGeometry::GetSelectedVertices( TArray<FGeomVertex*>& InVerts )
{
	InVerts.Empty();

	for( INT ObjectIdx = 0 ; ObjectIdx < GeomObjects.Num() ; ++ObjectIdx )
	{
		FGeomObject* GeomObject = GeomObjects(ObjectIdx);

		for( INT V = 0 ; V < GeomObject->VertexPool.Num() ; ++V )
		{
			if( GeomObject->VertexPool(V).IsSelected() )
			{
				InVerts.AddItem( &GeomObject->VertexPool(V) );
			}
		}
	}
}


/**
* Utility function that allow you to poll and see if certain sub elements are currently selected.
*
* Returns a combination of the flags in ESelectionStatus.
*/

INT FEdModeGeometry::GetSelectionState()
{
	INT Status = 0;

	if( HavePolygonsSelected() )
	{
		Status |= GSS_Polygon;
	}

	if( HaveEdgesSelected() )
	{
		Status |= GSS_Edge;
	}

	if( HaveVerticesSelected() )
	{
		Status |= GSS_Vertex;
	}

	return Status;
}

FVector FEdModeGeometry::GetWidgetLocation() const
{
	return FEdMode::GetWidgetLocation();
}

// ------------------------------------------------------------------------------

/**
 * Deselects all edges, polygons, and vertices for all selected objects.
 */
void FEdModeGeometry::SelectNone()
{
	for( INT ObjectIdx = 0 ; ObjectIdx < GeomObjects.Num() ; ++ObjectIdx )
	{
		FGeomObject* GeomObject = GeomObjects(ObjectIdx);
		GeomObject->Select( 0 );

		for( int VertexIdx = 0 ; VertexIdx < GeomObject->EdgePool.Num() ; ++VertexIdx )
		{
			GeomObject->EdgePool(VertexIdx).Select( 0 );
		}
		for( int VertexIdx = 0 ; VertexIdx < GeomObject->PolyPool.Num() ; ++VertexIdx )
		{
			GeomObject->PolyPool(VertexIdx).Select( 0 );
		}
		for( int VertexIdx = 0 ; VertexIdx < GeomObject->VertexPool.Num() ; ++VertexIdx )
		{
			GeomObject->VertexPool(VertexIdx).Select( 0 );
		}

		GeomObject->SelectionOrder.Empty();
	}
}

// ------------------------------------------------------------------------------

void FEdModeGeometry::RenderPoly( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI )
{
	for( INT ObjectIdx = 0 ; ObjectIdx < GeomObjects.Num() ; ++ObjectIdx )
	{
		const FGeomObject* GeomObject = GeomObjects(ObjectIdx);
		
		// Allocate the material proxy and register it so it can be deleted properly once the rendering is done with it.
		FDynamicColoredMaterialRenderProxy* SelectedColorInstance = new FDynamicColoredMaterialRenderProxy(GEngine->GeomMaterial->GetRenderProxy(FALSE),FLinearColor(GeomObject->GetActualBrush()->GetWireColor()) * 2.0f);
		PDI->RegisterDynamicResource( SelectedColorInstance );

		FDynamicColoredMaterialRenderProxy* UnselectedColorInstance = new FDynamicColoredMaterialRenderProxy(GEngine->GeomMaterial->GetRenderProxy(FALSE),FLinearColor(GeomObject->GetActualBrush()->GetWireColor()) * 0.25f);
		PDI->RegisterDynamicResource( UnselectedColorInstance );		

		// Render selected filled polygons.
		for( INT PolyIdx = 0 ; PolyIdx < GeomObject->PolyPool.Num() ; ++PolyIdx )
		{
			const FGeomPoly* GeomPoly = &GeomObject->PolyPool(PolyIdx);
			PDI->SetHitProxy( new HGeomPolyProxy(const_cast<FGeomObject*>(GeomPoly->GetParentObject()),PolyIdx) );
			{
				FDynamicMeshBuilder MeshBuilder;

				TArray<FVector> Verts;

				// Look at the edge list and create a list of vertices to render from.

				FVector LastPos(0);

				for( INT EdgeIdx = 0 ; EdgeIdx < GeomPoly->EdgeIndices.Num() ; ++EdgeIdx )
				{
					const FGeomEdge* GeomEdge = &GeomPoly->GetParentObject()->EdgePool( GeomPoly->EdgeIndices(EdgeIdx) );

					if( EdgeIdx == 0 )
					{
						Verts.AddItem( GeomPoly->GetParentObject()->VertexPool( GeomEdge->VertexIndices[0] ) );
						LastPos = GeomPoly->GetParentObject()->VertexPool( GeomEdge->VertexIndices[0] );
					}
					else if( GeomPoly->GetParentObject()->VertexPool( GeomEdge->VertexIndices[0] ).Equals( LastPos ) )
					{
						Verts.AddItem( GeomPoly->GetParentObject()->VertexPool( GeomEdge->VertexIndices[1] ) );
						LastPos = GeomPoly->GetParentObject()->VertexPool( GeomEdge->VertexIndices[1] );
					}
					else
					{
						Verts.AddItem( GeomPoly->GetParentObject()->VertexPool( GeomEdge->VertexIndices[0] ) );
						LastPos = GeomPoly->GetParentObject()->VertexPool( GeomEdge->VertexIndices[0] );
					}
				}

				// Draw Polygon Triangles
				const INT VertexOffset = MeshBuilder.AddVertex(Verts(0), FVector2D(0,0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				MeshBuilder.AddVertex(Verts(1), FVector2D(0,0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));

				for( INT VertIdx = 2 ; VertIdx < Verts.Num() ; ++VertIdx )
				{
					MeshBuilder.AddVertex(Verts(VertIdx), FVector2D(0,0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
					MeshBuilder.AddTriangle( VertexOffset + VertIdx - 1, VertexOffset, VertexOffset + VertIdx);
				}

				if( GeomPoly->IsSelected() )
				{
					// Selected polygons are drawn on top of the world so the user can easily see their extents.
					MeshBuilder.Draw(PDI, GeomObject->GetActualBrush()->LocalToWorld(), SelectedColorInstance, SDPG_Foreground, 0.f );
				}
				else
				{
					// We only draw unselected polygons in the perspective viewport
					if( !((FEditorLevelViewportClient*)(Viewport->GetClient()))->IsOrtho() )
					{
						// Unselected polygons are drawn at the world level but are bumped slightly forward to avoid z-fighting
						MeshBuilder.Draw(PDI, GeomObject->GetActualBrush()->LocalToWorld(), UnselectedColorInstance, SDPG_World, -0.000002f );
					}
				}
			}
			PDI->SetHitProxy( NULL );
		}
	}
}

// ------------------------------------------------------------------------------

void FEdModeGeometry::RenderEdge( const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	for( INT ObjectIdx = 0 ; ObjectIdx < GeomObjects.Num() ; ++ObjectIdx )
	{
		const FGeomObject* GeometryObject = GeomObjects(ObjectIdx);
		const FColor WireColor = GeometryObject->GetActualBrush()->GetWireColor();

		// Edges
		for( INT EdgeIdx = 0 ; EdgeIdx < GeometryObject->EdgePool.Num() ; ++EdgeIdx )
		{
			const FGeomEdge* GeometryEdge = &GeometryObject->EdgePool(EdgeIdx);
			const FColor Color = GeometryEdge->IsSelected() ? FColor(255,128,64) : WireColor;

			PDI->SetHitProxy( new HGeomEdgeProxy(const_cast<FGeomObject*>(GeometryObject),EdgeIdx) );
			{
				FVector V0 = GeometryObject->VertexPool( GeometryEdge->VertexIndices[0] );
				FVector V1 = GeometryObject->VertexPool( GeometryEdge->VertexIndices[1] );
				const FMatrix LocalToWorldMatrix = GeometryObject->GetActualBrush()->LocalToWorld();

				V0 = LocalToWorldMatrix.TransformFVector( V0 );
				V1 = LocalToWorldMatrix.TransformFVector( V1 );

				PDI->DrawLine( V0, V1, Color, SDPG_Foreground );
			}
			PDI->SetHitProxy( NULL );
		}
	}
}

// ------------------------------------------------------------------------------

void FEdModeGeometry::RenderVertex( const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	for( INT ObjectIdx = 0 ; ObjectIdx < GeomObjects.Num() ; ++ObjectIdx )
	{
		const FGeomObject* GeomObject = GeomObjects(ObjectIdx);
		check(GeomObject);

		// Vertices

		FColor Color;
		FLOAT Scale;
		FVector Location;

		for( INT VertIdx = 0 ; VertIdx < GeomObject->VertexPool.Num() ; ++VertIdx )
		{
			const FGeomVertex* GeomVertex = &GeomObject->VertexPool(VertIdx);
			check(GeomVertex);
			check(GeomObject->GetActualBrush());

			Location = GeomObject->GetActualBrush()->LocalToWorld().TransformFVector( *GeomVertex );
			Scale = View->WorldToScreen( Location ).W * ( 4 / View->SizeX / View->ProjectionMatrix.M[0][0] );
			Color = GeomVertex->IsSelected() ? FColor(255,128,64) : GeomObject->GetActualBrush()->GetWireColor();

			PDI->SetHitProxy( new HGeomVertexProxy( const_cast<FGeomObject*>(GeomObject), VertIdx) );
			PDI->DrawSprite( Location, 4.f * Scale, 4.f * Scale, GWorld->GetWorldInfo()->BSPVertex->Resource, Color, SDPG_UnrealEdForeground, 0.0, 0.0, 0.0, 0.0 );
			PDI->SetHitProxy( NULL );
		}
	}
}

/**
 * Cache all the selected geometry on the object, and add to the array if any is found
 *
 * Return TRUE if new object has been added to the array.
 */

UBOOL FEdModeGeometry::CacheSelectedData( TArray<HGeomMidPoints>& raGeomData, const FGeomObject& rGeomObject ) const
{
	// Early out if this object doesn't have a brush
	if ( !rGeomObject.ActualBrush )
	{
		return FALSE;
	}

	HGeomMidPoints GeomData;

	// Loop through all the verts caching their midpoint if they're selected
	for ( INT i=0; i<rGeomObject.VertexPool.Num(); i++ )
	{
		const FGeomVertex& rGeomVertex = rGeomObject.VertexPool(i);
		if(rGeomVertex.IsSelected())
		{
			GeomData.VertexPool.AddItem( rGeomVertex.GetMidPoint() );
		}
	}

	// Loop through all the edges caching their midpoint if they're selected
	for ( INT i=0; i<rGeomObject.EdgePool.Num(); i++ )
	{
		const FGeomEdge& rGeomEdge = rGeomObject.EdgePool(i);
		if(rGeomEdge.IsSelected())
		{
			GeomData.EdgePool.AddItem( rGeomEdge.GetMidPoint() );
		}
	}

	// Loop through all the polys caching their midpoint if they're selected
	for ( INT i=0; i<rGeomObject.PolyPool.Num(); i++ )
	{
		const FGeomPoly& rGeomPoly = rGeomObject.PolyPool(i);
		if(rGeomPoly.IsSelected())
		{
			GeomData.PolyPool.AddItem( rGeomPoly.GetMidPoint() );
		}
	}
	
	// Only add the data to the array if there was anything that was selected
	UBOOL bRet = ( ( GeomData.VertexPool.Num() + GeomData.EdgePool.Num() + GeomData.PolyPool.Num() ) > 0 ? TRUE : FALSE );
	if ( bRet )
	{
		// Make note of the brush this belongs to, then add
		GeomData.ActualBrush = rGeomObject.ActualBrush;
		raGeomData.AddItem( GeomData );
	}
	return bRet;
}

/**
 * Attempt to find all the new geometry using the cached data, and cache those new ones out
 *
 * Return TRUE everything was found (or there was nothing to find)
 */
UBOOL FEdModeGeometry::FindFromCache( TArray<HGeomMidPoints>& raGeomData, FGeomObject& rGeomObject, TArray<FGeomBase*>& raSelectedGeom ) const
{
	// Early out if this object doesn't have a brush
	if ( !rGeomObject.ActualBrush )
	{
		return TRUE;
	}

	// Early out if we don't have anything cached
	if ( raGeomData.Num() == 0 )
	{
		return TRUE;
	}

	// Loop through all the cached data, seeing if there's a match for the brush
	// Note: if GetMidPoint wasn't pure virtual this could be much nicer
	UBOOL bRet = FALSE;		// True if the brush that was parsed was found and all verts/edges/polys were located
	UBOOL bFound = FALSE;	// True if the brush that was parsed was found
	for( INT i=0; i<raGeomData.Num(); i++ )
	{
		// Does this brush match the cached actor?
		HGeomMidPoints& rGeomData = raGeomData(i);
		if ( rGeomData.ActualBrush == rGeomObject.ActualBrush )
		{
			// Compare location of new midpoints with cached versions
			bFound = TRUE;
			UBOOL bSucess = TRUE;		// True if all verts/edges/polys were located
			for ( INT j=0; j<rGeomData.VertexPool.Num(); j++ )
			{
				const FVector& rGeomVector = rGeomData.VertexPool(j);
				for ( INT k=0; k<rGeomObject.VertexPool.Num(); k++ )
				{
					// If we have a match select it and move on to the next one
					FGeomVertex& rGeomVertex = rGeomObject.VertexPool(k);
					if ( rGeomVector.Equals( rGeomVertex.GetMidPoint() ) )
					{
						// Add the new geometry to the to-be-selected pool, and remove from the data pool
						raSelectedGeom.AddItem(&rGeomVertex);
						rGeomData.VertexPool.Remove(j--);
						break;
					}
				}
			}
			// If we didn't locate them all inform the user
			if ( rGeomData.VertexPool.Num() != 0 )
			{
				debugf( NAME_Warning, TEXT( "Unable to find %d Vertex(s) in new BSP" ), rGeomData.VertexPool.Num() );
				bSucess = FALSE;
			}

			// Compare location of new midpoints with cached versions
			for ( INT j=0; j<rGeomData.EdgePool.Num(); j++ )
			{
				const FVector& rGeomVector = rGeomData.EdgePool(j);
				for ( INT k=0; k<rGeomObject.EdgePool.Num(); k++ )
				{
					// If we have a match select it and move on to the next one
					FGeomEdge& rGeomEdge = rGeomObject.EdgePool(k);
					if ( rGeomVector.Equals( rGeomEdge.GetMidPoint() ) )
					{
						// Add the new geometry to the to-be-selected pool, and remove from the data pool
						raSelectedGeom.AddItem(&rGeomEdge);
						rGeomData.EdgePool.Remove(j--);
						break;
					}
				}
			}
			// If we didn't locate them all inform the user
			if ( rGeomData.EdgePool.Num() != 0 )
			{
				debugf( NAME_Warning, TEXT( "Unable to find %d Edge(s) in new BSP" ), rGeomData.EdgePool.Num() );
				bSucess = FALSE;
			}

			// Compare location of new midpoints with cached versions
			for ( INT j=0; j<rGeomData.PolyPool.Num(); j++ )
			{
				const FVector& rGeomVector = rGeomData.PolyPool(j);
				for ( INT k=0; k<rGeomObject.PolyPool.Num(); k++ )
				{
					// If we have a match select it and move on to the next one
					FGeomPoly& rGeomPoly = rGeomObject.PolyPool(k);
					if ( rGeomVector.Equals( rGeomPoly.GetMidPoint() ) )
					{
						// Add the new geometry to the to-be-selected pool, and remove from the data pool
						raSelectedGeom.AddItem(&rGeomPoly);
						rGeomData.PolyPool.Remove(j--);
						break;
					}
				}
			}
			// If we didn't locate them all inform the user
			if ( rGeomData.PolyPool.Num() != 0 )
			{
				debugf( NAME_Warning, TEXT( "Unable to find %d Poly(s) in new BSP" ), rGeomData.PolyPool.Num() );
				bSucess = FALSE;
			}

			// If we didn't locate them all inform the user, then remove from the data pool
			if ( !bSucess )
			{
				debugf( NAME_Warning, TEXT( "Unable to resolve %s Brush in new BSP, see above" ), *rGeomData.ActualBrush->GetName() );
			}
			bRet = bSucess;
			raGeomData.Remove(i--);
			break;
		}
	}
	// If we didn't locate the brush inform the user
	if ( !bFound )
	{
		debugf( NAME_Warning, TEXT( "Unable to find %s Brush(s) in new BSP" ), *rGeomObject.ActualBrush->GetName() );
	}
	return bRet;
}

/**
 * Select all the verts/edges/polys that were found
 *
 * Return TRUE if successful
 */
UBOOL FEdModeGeometry::SelectCachedData( TArray<FGeomBase*>& raSelectedGeom ) const
{
	// Early out if we don't have anything cached
	if ( raSelectedGeom.Num() == 0 )
	{
		return FALSE;
	}

	// Grab the editor tools so we can reposition the widget correctly
	FEditorModeTools& Tools = GEditorModeTools();
	check( Tools.IsModeActive(EM_Geometry) );

	// Backup widget position, we want it to be in the same position as it was previously too
	FVector PivLoc = Tools.PivotLocation;
	FVector SnapLoc = Tools.SnappedLocation;

	// Loop through all the geometry that should be selected
	for( INT i=0; i<raSelectedGeom.Num(); i++ )
	{
		// Does this brush match the cached actor?
		FGeomBase* pGeom = raSelectedGeom(i);
		if ( pGeom )
		{
			pGeom->Select();
		}
	}

	// Restore the widget position
	Tools.PivotLocation = PivLoc;
	Tools.SnappedLocation = SnapLoc;

	return TRUE;
}

#define BSP_RESELECT	// Attempt to reselect any geometry that was selected prior to the BSP being rebuilt
#define BSP_RESELECT__ALL_OR_NOTHING	// If any geometry can't be located, then don't select anything

/**
 * Compiles geometry mode information from the selected brushes.
 */

void FEdModeGeometry::GetFromSource()
{
	GWarn->BeginSlowTask( TEXT("Rebuilding BSP"), FALSE);

	TArray<HGeomMidPoints> GeomData;

	// Go through each brush and update its components before updating below
	for( INT i=0; i<GeomObjects.Num(); i++ )
	{
		FGeomObject* GeomObject = GeomObjects(i);
		if(GeomObject && GeomObject->ActualBrush)
		{
#ifdef BSP_RESELECT
			// Cache any information that'll help us reselect the object after it's reconstructed
			CacheSelectedData( GeomData, *GeomObject );
#endif // BSP_RESELECT
			GeomObject->ActualBrush->ClearComponents();
			GeomObject->ActualBrush->ConditionalUpdateComponents();
			delete GeomObject;
		}		
	}
	GeomObjects.Empty();

	TArray<FGeomBase*> SelectedGeom;

	// Notify the selected actors that they have been moved.
	UBOOL bFound = TRUE;
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if ( Actor->IsABrush() )
		{
			ABrush* BrushActor = static_cast<ABrush*>( Actor );
			if( BrushActor->Brush != NULL )
			{
				FGeomObject* GeomObject			= new FGeomObject();
				GeomObject->SetParentObjectIndex( GeomObjects.AddItem( GeomObject ) );
				GeomObject->ActualBrush			= BrushActor;
				GeomObject->GetFromSource();
#ifdef BSP_RESELECT
				// Attempt to find all the previously selected geometry on this object if everything has gone OK so far
				if ( bFound && !FindFromCache( GeomData, *GeomObject, SelectedGeom ) )
				{
#ifdef BSP_RESELECT__ALL_OR_NOTHING
					// If it didn't succeed, don't attempt to reselect anything as the user will only end up moving part of their previous selection
					debugf( NAME_Warning, TEXT( "Unable to find all previously selected geometry data, resetting selection" ) );
					SelectedGeom.Empty();
					GeomData.Empty();
					bFound = FALSE;
#endif // BSP_RESELECT__ALL_OR_NOTHING
				}
#endif // BSP_RESELECT
			}
		}
	}

#ifdef BSP_RESELECT
	// Reselect anything that came close to the cached midpoints
	SelectCachedData( SelectedGeom );
#endif // BSP_RESELECT

	GWarn->EndSlowTask();
}

/**
 * Changes the source brushes to match the current geometry data.
 */

void FEdModeGeometry::SendToSource()
{
	for( INT o = 0 ; o < GeomObjects.Num() ; ++o )
	{
		FGeomObject* go = GeomObjects(o);

		go->SendToSource();
	}
}

UBOOL FEdModeGeometry::FinalizeSourceData()
{
	UBOOL Result = 0;

	for( INT o = 0 ; o < GeomObjects.Num() ; ++o )
	{
		FGeomObject* go = GeomObjects(o);

		if( go->FinalizeSourceData() )
		{
			Result = 1;
		}
	}

	return Result;
}

void FEdModeGeometry::PostUndo()
{
	// Rebuild the geometry data from the current brush state

	GetFromSource();
	
	// Restore selection information.

	INT HighestSelectionIndex = INDEX_NONE;

	for( INT o = 0 ; o < GeomObjects.Num() ; ++o )
	{
		FGeomObject* go = GeomObjects(o);
		ABrush* Actor = go->GetActualBrush();

		for( INT s = 0 ; s < Actor->SavedSelections.Num() ; ++s )
		{
			FGeomSelection* gs = &Actor->SavedSelections(s);

			if( gs->SelectionIndex > HighestSelectionIndex )
			{
				HighestSelectionIndex = gs->SelectionIndex;
			}

			switch( gs->Type )
			{
				case GS_Poly:
					go->PolyPool( gs->Index ).ForceSelectionIndex( gs->SelectionIndex );
					GEditorModeTools().PivotLocation = GEditorModeTools().SnappedLocation = go->PolyPool( gs->Index ).GetWidgetLocation();
					break;
				
				case GS_Edge:
					go->EdgePool( gs->Index ).ForceSelectionIndex( gs->SelectionIndex );
					GEditorModeTools().PivotLocation = GEditorModeTools().SnappedLocation = go->EdgePool( gs->Index ).GetWidgetLocation();
					break;

				case GS_Vertex:
					go->VertexPool( gs->Index ).ForceSelectionIndex( gs->SelectionIndex );
					GEditorModeTools().PivotLocation = GEditorModeTools().SnappedLocation = go->VertexPool( gs->Index ).GetWidgetLocation();
					break;
			}
		}

		go->ForceLastSelectionIndex(HighestSelectionIndex );
	}
}

UBOOL FEdModeGeometry::ExecDelete()
{
	check( GEditorModeTools().IsModeActive( EM_Geometry ) );

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode( EM_Geometry );

	// Find the delete modifier and execute it.

	FModeTool_GeometryModify* mtgm = (FModeTool_GeometryModify*)FindTool( MT_GeometryModify );

	for( FModeTool_GeometryModify::TModifierIterator Itor( mtgm->ModifierIterator() ) ; Itor ; ++Itor )
	{
		UGeomModifier* gm = *Itor;

		if( gm->IsA( UGeomModifier_Delete::StaticClass()) )
		{
			if( gm->Apply() )
			{
				return 1;
			}
		}
	}

	return 0;
}

void FEdModeGeometry::UpdateInternalData()
{
	GetFromSource();
}

/*------------------------------------------------------------------------------
	Static Mesh.
------------------------------------------------------------------------------*/

FEdModeStaticMesh::FEdModeStaticMesh()
{
	ID = EM_StaticMesh;
	Desc = TEXT("Static Mesh");

	Tools.AddItem( new FModeTool_StaticMesh() );
	SetCurrentTool( MT_StaticMesh );

	Settings = new FStaticMeshToolSettings;
}

FEdModeStaticMesh::~FEdModeStaticMesh()
{
}

/*------------------------------------------------------------------------------
	Terrain Tools
------------------------------------------------------------------------------*/

FEdModeTerrainEditing::FEdModeTerrainEditing()
{
	ID = EM_TerrainEdit;
	Desc = TEXT("Terrain Editing");
	bPerToolSettings = FALSE;
	bShowBallAndSticks = TRUE;
	ModeColor = FColor(255,255,255);
	CurrentTerrain = NULL;

	ToolColor = FLinearColor(1.0f, 0.0f, 0.0f);
	MirrorColor = FLinearColor(1.0f, 0.0f, 1.0f);

	BallTexture = (UTexture2D*)UObject::StaticLoadObject(UTexture2D::StaticClass(), NULL, 
		TEXT("EditorMaterials.TerrainLayerBrowser.TerrainBallImage"),NULL,LOAD_None,NULL);
	
	Tools.AddItem( new FModeTool_TerrainPaint() );
	Tools.AddItem( new FModeTool_TerrainSmooth() );
	Tools.AddItem( new FModeTool_TerrainAverage() );
	Tools.AddItem( new FModeTool_TerrainFlatten() );
	Tools.AddItem( new FModeTool_TerrainNoise() );
	Tools.AddItem( new FModeTool_TerrainVisibility() );
	Tools.AddItem( new FModeTool_TerrainTexturePan() );
	Tools.AddItem( new FModeTool_TerrainTextureRotate() );
	Tools.AddItem( new FModeTool_TerrainTextureScale() );
	Tools.AddItem( new FModeTool_TerrainSplitX() );
	Tools.AddItem( new FModeTool_TerrainSplitY() );
	Tools.AddItem( new FModeTool_TerrainMerge() );
	Tools.AddItem( new FModeTool_TerrainAddRemoveSectors() );
	Tools.AddItem( new FModeTool_TerrainOrientationFlip() );
	Tools.AddItem( new FModeTool_TerrainVertexEdit() );

	SetCurrentTool( MT_TerrainPaint );

	Settings = new FTerrainToolSettings;
}

const FToolSettings* FEdModeTerrainEditing::GetSettings() const
{
	if( bPerToolSettings )
	{
		return ((FModeTool_Terrain*)GetCurrentTool())->GetSettings();
	}
	else
	{
		return Settings;
	}
}

void FEdModeTerrainEditing::DrawTool( const FSceneView* View,FViewport* Viewport, FPrimitiveDrawInterface* PDI, 
	class ATerrain* Terrain, FVector& Location, FLOAT InnerRadius, FLOAT OuterRadius, TArray<ATerrain*>& Terrains )
{
	FTerrainToolSettings* ToolSettings = (FTerrainToolSettings*)(GetSettings());

	DrawToolCircle(View, Viewport, PDI, Terrain, Location, InnerRadius, Terrains);
	if (OuterRadius != InnerRadius)
	{
		DrawToolCircle(View, Viewport, PDI, Terrain, Location, OuterRadius, Terrains);
	}
	// Draw the 'ball' at the exact vertex...
	DrawToolCircleBallAndSticks(View, Viewport, PDI, Terrain, Location, InnerRadius, OuterRadius, Terrains);

	FModeTool_Terrain* TerrainTool = (FModeTool_Terrain*)CurrentTool;
	if ((TerrainTool->SupportsMirroring() == TRUE) && (ToolSettings->MirrorSetting != FTerrainToolSettings::TTMirror_NONE))
	{
		DetermineMirrorLocation( PDI, Terrain, Location );
		FVector MirroredPosition = MirrorLocation;

		FVector MirrorWorld = Terrain->LocalToWorld().TransformFVector(MirroredPosition);
        DrawToolCircle(View, Viewport, PDI, Terrain, MirrorWorld, InnerRadius, Terrains, TRUE);
		if (OuterRadius != InnerRadius)
		{
			DrawToolCircle(View, Viewport, PDI, Terrain, MirrorWorld, OuterRadius, Terrains, TRUE);
		}
		// Draw the 'ball' at the exact vertex...
		DrawToolCircleBallAndSticks(View, Viewport, PDI, Terrain, MirrorWorld, InnerRadius, OuterRadius, Terrains);
	}
}

#define CIRCLESEGMENTS 16
void FEdModeTerrainEditing::DrawToolCircle( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI, 
	ATerrain* Terrain, FVector& Location, FLOAT Radius, TArray<ATerrain*>& Terrains, UBOOL bMirror )
{
	FTerrainToolSettings* ToolSettings = (FTerrainToolSettings*)(GetSettings());

	FModeTool_Terrain* TerrainTool = (FModeTool_Terrain*)CurrentTool;
	UBOOL bAllowMultipleTerrains = (TerrainTool->GetID() == MT_TerrainMerge);

	ToolTerrains.Empty();

	FVector	Extent(0,0,0);
	FVector	LastVertex(0,0,0);
	UBOOL	LastVertexValid = 0;
    for( INT s=0;s<=CIRCLESEGMENTS;s++ )
	{
		FLOAT theta =  PI * 2.f * s / CIRCLESEGMENTS;

		FVector TraceStart = Location;
		TraceStart.X += Radius * appSin(theta);
		TraceStart.Y += Radius * appCos(theta);
		FVector TraceEnd = TraceStart;
        TraceStart.Z = HALF_WORLD_MAX;
		TraceEnd.Z = -HALF_WORLD_MAX;

		FCheckResult Result;
		Result.Actor = NULL;

		// Get list of hit actors.
		FMemMark Mark(GMainThreadMemStack);

		INT TraceFlags = TRACE_Terrain|TRACE_TerrainIgnoreHoles|TRACE_Visible;
		
		FCheckResult* FirstHit = GWorld->MultiLineCheck(
			GMainThreadMemStack,
			TraceEnd,
			TraceStart,
			FVector(0.0f),
			TRACE_Terrain|TRACE_TerrainIgnoreHoles|TRACE_Visible,
			NULL,
			NULL
		);

		Mark.Pop();
		
		Result.Component = NULL;
		FCheckResult* CheckHit = FirstHit;
		while (CheckHit && (Result.Component == NULL))
		{
			if (CheckHit->Component->IsA(UTerrainComponent::StaticClass()) == FALSE)
			{
				CheckHit = CheckHit->GetNext();
			}
			else
			{
				if ((CheckHit->Component->GetOwner() == Terrain) || bAllowMultipleTerrains)
				{
					Result.Component = CheckHit->Component;
					Result.Actor = CheckHit->Component->GetOwner();
					Result.Location = CheckHit->Location;
				}
				else
				{
					CheckHit = CheckHit->GetNext();
				}
			}
		}

		if (Result.Component != NULL)
		{
			ATerrain* HitTerrain = CastChecked<ATerrain>(Result.Actor);
			if( (Terrains.FindItemIndex(HitTerrain) != INDEX_NONE) ||
				(((FModeTool_Terrain*)GEditorModeTools().GetActiveTool(EM_TerrainEdit))->TerrainIsValid(HitTerrain, ToolSettings)) )
			{
				if(LastVertexValid)
				{
					if (bMirror == FALSE)
					{
						PDI->DrawLine(LastVertex,Result.Location,ToolColor,SDPG_Foreground);
					}
					else
					{
						PDI->DrawLine(LastVertex,Result.Location,MirrorColor,SDPG_Foreground);
					}
				}
				LastVertex = Result.Location;
				LastVertexValid = 1;
				Terrains.AddUniqueItem(HitTerrain);
				ToolTerrains.AddUniqueItem(HitTerrain);
			}
			else
			{
				LastVertexValid = 0;
			}
		}
		else
		{
			LastVertexValid = 0;
		}
	}
}

void FEdModeTerrainEditing::DrawToolCircleBallAndSticks( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI, 
	class ATerrain* Terrain, FVector& Location, FLOAT InnerRadius, FLOAT OuterRadius, TArray<ATerrain*>& Terrains, UBOOL bMirror )
{
	FVector OutVertex;

	for (INT TerrainIndex = 0; TerrainIndex < Terrains.Num(); TerrainIndex++)
	{
		ATerrain* CheckTerrain = Terrains(TerrainIndex);
		UBOOL bGetConstained = FALSE;
		if (bConstrained && (CheckTerrain->EditorTessellationLevel > 0))
		{
			bGetConstained = TRUE;
		}
		if (CheckTerrain->GetClosestVertex(Location, OutVertex, bGetConstained) == TRUE)
		{
			// Draw the ball and stick
			DrawBallAndStick( View, Viewport, PDI, CheckTerrain, OutVertex, -1.0f );
		}
	}
}

void FEdModeTerrainEditing::DrawBallAndStick( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI, 
	class ATerrain* Terrain, FVector& Location, FLOAT Strength )
{
	// Draw the line
	FVector Start = Location;
	FVector End = Location + FVector(0.0f, 0.0f, 20.0f);
	FLinearColor Color(1.0f, 0.0f, 0.0f, 1.0f);
	PDI->DrawLine(Start, End, Color, SDPG_Foreground);

	// Determine the size and color for the ball
	// Calculate the view-dependent scaling factor.
	FLOAT ViewedSizeX = 10.0f;
	FLOAT ViewedSizeY = 10.0f;
	if ((View->ProjectionMatrix.M[3][3] != 1.0f))
	{
		FVector EndView = View->ViewMatrix.TransformFVector(End);
		static FLOAT BallDivisor = 40.0f;
		ViewedSizeX = EndView.Z / BallDivisor;
		ViewedSizeY = EndView.Z / BallDivisor;
	}

	if (Strength < 0.0f)
	{
		// This indicates the ball should only be rendered in a Red shade
		Color = FLinearColor(abs(Strength), 0.0f, 0.0f);
	}
	else
	{
		Color = FLinearColor(Strength, Strength, Strength);
	}
	PDI->DrawSprite(End, ViewedSizeX, ViewedSizeY, BallTexture->Resource, Color, SDPG_Foreground, 0.0, 0.0, 0.0, 0.0);
}

void FEdModeTerrainEditing::Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View,Viewport,PDI);

	if(!PDI->IsHitTesting())
	{
		// check the mouse cursor is inside the viewport
		FIntPoint	MousePosition;
		Viewport->GetMousePos(MousePosition);
		INT		X = MousePosition.X,
				Y = MousePosition.Y;
		if ((X >= 0) && (Y >=0) && (X < (INT)Viewport->GetSizeX()) && (Y < (INT)Viewport->GetSizeY()))
		{
			FTerrainToolSettings* CurrentSettings = (FTerrainToolSettings*)GetSettings();
			FVector HitNormal;
			ATerrain* Terrain = ((FModeTool_Terrain*)GEditorModeTools().GetActiveTool(EM_TerrainEdit))->TerrainTrace(Viewport,View,ToolHitLocation,HitNormal,CurrentSettings, TRUE);

			if (Terrain)
			{
				DrawTool(View,Viewport,PDI,Terrain,ToolHitLocation,CurrentSettings->RadiusMin,CurrentSettings->RadiusMax,ToolTerrains);
			}
			else
			{
				ToolTerrains.Empty();
			}

			// Allow the tool to render anything custom.
			for (INT ToolTerrainIndex = 0; ToolTerrainIndex < ToolTerrains.Num(); ToolTerrainIndex++)
			{
				ATerrain* ToolTerrain = ToolTerrains(ToolTerrainIndex);
				((FModeTool_Terrain*)GEditorModeTools().GetActiveTool(EM_TerrainEdit))->RenderTerrain( ToolTerrain, ToolHitLocation, View, Viewport, PDI );
			}
		}
	}
}

void FEdModeTerrainEditing::DetermineMirrorLocation( FPrimitiveDrawInterface* PDI, class ATerrain* Terrain, FVector& Location )
{
	FTerrainToolSettings* ToolSettings = (FTerrainToolSettings*)(GetSettings());
	FVector LocalPosition = Terrain->WorldToLocal().TransformFVector(Location);

	FLOAT HalfX = (FLOAT)(Terrain->NumPatchesX) / 2.0f;
	FLOAT HalfY = (FLOAT)(Terrain->NumPatchesY) / 2.0f;

	FVector MirroredPosition = LocalPosition;
	FLOAT Diff;

	switch (ToolSettings->MirrorSetting)
	{
	case FTerrainToolSettings::TTMirror_X:
		// Mirror about the X-axis
		Diff = HalfY - MirroredPosition.Y;
		MirroredPosition.Y = HalfY + Diff;
		break;
	case FTerrainToolSettings::TTMirror_Y:
		// Mirror about the Y-axis
		Diff = HalfX - MirroredPosition.X;
		MirroredPosition.X = HalfX + Diff;
		break;
	case FTerrainToolSettings::TTMirror_XY:
		// Mirror about the X and Y-axes
		Diff = HalfX - MirroredPosition.X;
		MirroredPosition.X = HalfX + Diff;
		Diff = HalfY - MirroredPosition.Y;
		MirroredPosition.Y = HalfY + Diff;
		break;
	}

	MirrorLocation = MirroredPosition;
}

INT FEdModeTerrainEditing::GetMirroredValue_Y(ATerrain* Terrain, INT InY, UBOOL bPatchOperation)
{
	INT HalfY = Terrain->NumPatchesY / 2;

	// Mirror about the X-axis
	INT Diff = HalfY - InY;
	INT ReturnY = HalfY + Diff;
	if (bPatchOperation == TRUE)
	{
		ReturnY--;
	}

	return ReturnY;
}

INT FEdModeTerrainEditing::GetMirroredValue_X(ATerrain* Terrain, INT InX, UBOOL bPatchOperation)
{
	INT HalfX = Terrain->NumPatchesX / 2;

	// Mirror about the X-axis
	INT Diff = HalfX - InX;
	INT ReturnX = HalfX + Diff;
	if (bPatchOperation == TRUE)
	{
		ReturnX--;
	}

	return ReturnX;
}

UBOOL FEdModeTerrainEditing::InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event)
{
	if( (Key == KEY_MouseScrollUp || Key == KEY_MouseScrollDown) && (ViewportClient->Input->IsShiftPressed() || ViewportClient->Input->IsAltPressed() ) )
	{
		FTerrainToolSettings* ToolSettings = (FTerrainToolSettings*)(GetSettings());
		int Delta = (Key == KEY_MouseScrollUp) ? 32 : -32;

		UBOOL bUpdate = FALSE;
		if( ViewportClient->Input->IsShiftPressed() )
		{
			ToolSettings->RadiusMax += Delta;
			// Decrease min radius too, if the max is smaller than the min.
			if( ToolSettings->RadiusMax < 0 )
			{
				ToolSettings->RadiusMax = 0;
			}
			else if( ToolSettings->RadiusMin > ToolSettings->RadiusMax )
			{
				ToolSettings->RadiusMin = ToolSettings->RadiusMax;
			}
			bUpdate = TRUE;
		}

		if( ViewportClient->Input->IsAltPressed() )
		{
			ToolSettings->RadiusMin += Delta;
			// Increase max radius too, if the min is bigger than the max.
			if( ToolSettings->RadiusMin < 0 )
			{
				ToolSettings->RadiusMin = 0;
			}
			else if( ToolSettings->RadiusMin > ToolSettings->RadiusMax )
			{
				ToolSettings->RadiusMax = ToolSettings->RadiusMin;
			}
			bUpdate = TRUE;
		}

		if (bUpdate == TRUE)
		{
			ViewportClient->Invalidate( TRUE, TRUE );
			GCallbackEvent->Send(CALLBACK_RefreshEditor_TerrainBrowser);
		}

		return TRUE;
	}

	return FEdMode::InputKey(ViewportClient, Viewport, Key, Event);
}

UBOOL FEdModeTerrainEditing::HandleClick(HHitProxy* HitProxy, const FViewportClick& Click)
{
	UBOOL bProcessedLocally = FALSE;

	if (HitProxy == NULL)
	{
		int dummy = 0;
	}
	else if (HitProxy->IsA(HActor::StaticGetType()))
	{
		HActor* ActorHitProxy = (HActor*)HitProxy;
		if (ActorHitProxy->Actor)
		{
			if (ActorHitProxy->Actor->IsA(ATerrain::StaticClass()) == FALSE)
			{
				// Don't process this!
				bProcessedLocally = TRUE;
			}
		}
	}
	else if (HitProxy->IsA(HBSPBrushVert::StaticGetType()))
	{
		// Don't process this!
		int dummy = 0;
		bProcessedLocally = TRUE;
	}
	else if (HitProxy->IsA(HStaticMeshVert::StaticGetType()))
	{
		// Don't process this!
		int dummy = 0;
		bProcessedLocally = TRUE;
	}
	else if (HitProxy->IsA(HGeomPolyProxy::StaticGetType()))
	{
		// Don't process this!
		int dummy = 0;
		bProcessedLocally = TRUE;
	}
	else if (HitProxy->IsA(HGeomEdgeProxy::StaticGetType()))
	{
		// Don't process this!
		int dummy = 0;
		bProcessedLocally = TRUE;
	}
	else if (HitProxy->IsA(HGeomVertexProxy::StaticGetType()))
	{
		// Don't process this!
		int dummy = 0;
		bProcessedLocally = TRUE;
	}
	else
	{
		debugf(NAME_Warning,TEXT("Unknown hit proxy type '%s' in FEdModeTerrainEditing::HandleClick()"), HitProxy->GetType()->GetName());
	}

	if (!bProcessedLocally)
	{
		return FEdMode::HandleClick(HitProxy,Click);
	}
	return TRUE;
}

/*------------------------------------------------------------------------------
	Texture
------------------------------------------------------------------------------*/

FEdModeTexture::FEdModeTexture()
	:	ScopedTransaction( NULL )
{
	ID = EM_Texture;
	Desc = TEXT("Texture Alignment");

	Tools.AddItem( new FModeTool_Texture() );
	SetCurrentTool( MT_Texture );

	Settings = new FTextureToolSettings;
}

FEdModeTexture::~FEdModeTexture()
{
	// Ensure no transaction is outstanding.
	check( !ScopedTransaction );
}

void FEdModeTexture::Enter()
{
	FEdMode::Enter();

	SaveCoordSystem = GEditorModeTools().CoordSystem;
	GEditorModeTools().CoordSystem = COORD_Local;
}

void FEdModeTexture::Exit()
{
	FEdMode::Exit();

	GEditorModeTools().CoordSystem = SaveCoordSystem;
}

FVector FEdModeTexture::GetWidgetLocation() const
{
	for ( TSelectedSurfaceIterator<> It ; It ; ++It )
	{
		FBspSurf* Surf = *It;
		ABrush* BrushActor = ( ABrush* )Surf->Actor;
		if( BrushActor )
		{
			FPoly* poly = &BrushActor->Brush->Polys->Element( Surf->iBrushPoly );
			return BrushActor->LocalToWorld().TransformFVector( poly->GetMidPoint() );
		}
	}

	return FEdMode::GetWidgetLocation();
}

UBOOL FEdModeTexture::ShouldDrawWidget() const
{
	return TRUE;
}

UBOOL FEdModeTexture::GetCustomDrawingCoordinateSystem( FMatrix& InMatrix, void* InData )
{
	FPoly* poly = NULL;

	for ( TSelectedSurfaceIterator<> It ; It ; ++It )
	{
		FBspSurf* Surf = *It;
		ABrush* BrushActor = ( ABrush* )Surf->Actor;
		if( BrushActor )
		{
			poly = &BrushActor->Brush->Polys->Element( Surf->iBrushPoly );
			break;
		}
	}

	if( !poly )
	{
		return FALSE;
	}

	InMatrix = FMatrix::Identity;

	InMatrix.SetAxis( 2, poly->Normal );
	InMatrix.SetAxis( 0, poly->TextureU );
	InMatrix.SetAxis( 1, poly->TextureV );

	InMatrix.RemoveScaling();

	return TRUE;
}

UBOOL FEdModeTexture::GetCustomInputCoordinateSystem( FMatrix& InMatrix, void* InData )
{
	return FALSE;
}

INT FEdModeTexture::GetWidgetAxisToDraw( FWidget::EWidgetMode InWidgetMode ) const
{
	switch( InWidgetMode )
	{
		case FWidget::WM_Translate:
		case FWidget::WM_ScaleNonUniform:
			return AXIS_X | AXIS_Y;
			break;

		case FWidget::WM_Rotate:
			return AXIS_Z;
			break;
	}

	return AXIS_XYZ;
}

UBOOL FEdModeTexture::StartTracking()
{
	// call base version because it calls the StartModify() virtual method needed to track drag events
	UBOOL BaseRtn = FEdMode::StartTracking();

	// Complete the previous transaction if one exists
	if( ScopedTransaction )
	{
		EndTracking();
	}

	// Start a new transaction
	ScopedTransaction = new FScopedTransaction( *LocalizeUnrealEd(TEXT("TextureManipulation")) );

	FOR_EACH_UMODEL;
		Model->ModifySelectedSurfs( TRUE );
	END_FOR_EACH_UMODEL;

	return BaseRtn;
}

UBOOL FEdModeTexture::EndTracking()
{
	// Clean up the scoped transaction if one is still pending
	if( ScopedTransaction != NULL )
	{
		delete ScopedTransaction;
		ScopedTransaction = NULL;
	}

	GWorld->MarkPackageDirty();
	GCallbackEvent->Send( CALLBACK_LevelDirtied );

	// call base version because it calls the EndModify() virtual method needed to track drag events 
	return FEdMode::EndTracking();
}


/*------------------------------------------------------------------------------
	FEdModeCoverEdit.
------------------------------------------------------------------------------*/

FEdModeCoverEdit::FEdModeCoverEdit() : FEdMode()
{
	ID = EM_CoverEdit;
	Desc = TEXT("Cover Editing");
	bTabDown = FALSE;
	LastSelectedCoverLink = NULL;
	LastSelectedCoverSlot = NULL;
}

FEdModeCoverEdit::~FEdModeCoverEdit()
{
}

void FEdModeCoverEdit::Enter()
{
	bCanAltDrag = TRUE;

	// set cover rendering flag
	if (GEditor != NULL)
	{
		for (INT Idx = 0; Idx < GEditor->ViewportClients.Num(); Idx++)
		{
			if (GEditor->ViewportClients(Idx) != NULL)
			{
				GEditor->ViewportClients(Idx)->ShowFlags |= SHOW_Cover;
			}
		}
	}
	FEdMode::Enter();
}

void FEdModeCoverEdit::Exit()
{
	FEdMode::Exit();
	// clear cover rendering flag
	if (GEditor != NULL)
	{
		for (INT Idx = 0; Idx < GEditor->ViewportClients.Num(); Idx++)
		{
			if (GEditor->ViewportClients(Idx) != NULL)
			{
				GEditor->ViewportClients(Idx)->ShowFlags &= ~SHOW_Cover;
			}
		}
	}
	bTabDown = FALSE;
	LastSelectedCoverLink = NULL;
	LastSelectedCoverSlot = NULL;
}

UBOOL FEdModeCoverEdit::UsesTransformWidget(FWidget::EWidgetMode CheckMode) const
{
	return (CheckMode == FWidget::WM_Translate || CheckMode == FWidget::WM_Rotate || CheckMode == FWidget::WM_Scale);
}

void FEdModeCoverEdit::ActorSelectionChangeNotify()
{
	// Find the last found Coverlink in the selection (if any)
	LastSelectedCoverLink = NULL;
	TArray<ACoverLink*> SelectedLinks;
	GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks);
	if(SelectedLinks.Num())
	{
		LastSelectedCoverLink = SelectedLinks(SelectedLinks.Num()-1);
	}
}

/**
 * Overridden to handle selecting individual cover link slots.
 */
UBOOL FEdModeCoverEdit::InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event)
{
	// Get some useful info about buttons being held down
	const UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	const UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);
	const UBOOL bAltDown = Viewport->KeyState(KEY_LeftAlt) || Viewport->KeyState(KEY_RightAlt);
	const UBOOL bTabDownKeyState = Viewport->KeyState(KEY_Tab);
	const INT HitX = Viewport->GetMouseX(), HitY = Viewport->GetMouseY();
	// grab a list of currently selected links
	TArray<ACoverLink*> SelectedLinks;
	TArray<ACoverGroup*> SelectedGroups;
	GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks);
	GEditor->GetSelectedActors()->GetSelectedObjects<ACoverGroup>(SelectedGroups);

	// Refresh viewport if tab state changes to potentially redraw widget for CoverLinks
	if( bTabDownKeyState != bTabDown )
	{
		bTabDown = bTabDownKeyState;
		GEditor->RedrawAllViewports();
	}

	if (Key == KEY_Escape)
	{
		// If the user hits ESC, deselect any existing selected slots first.  If no slots are selected, then go
		// ahead with the regular editor de-selection logic.

		UBOOL bSelectedSlots = FALSE;

		for (INT LinkIdx = 0; LinkIdx < SelectedLinks.Num(); LinkIdx++)
		{
			ACoverLink* Link = SelectedLinks(LinkIdx);

			for (INT Idx = 0 ; Idx < Link->Slots.Num() ; ++Idx )
			{
				FCoverSlot& Slot = Link->Slots(Idx);

				if( Slot.bSelected )
				{
					bSelectedSlots = TRUE;
					Slot.bSelected = FALSE;
				}

				Link->ForceUpdateComponents(FALSE,FALSE);
			}
		}

		if( bSelectedSlots )
		{
			GEditor->RedrawAllViewports();
			return 1;
		}
	}
	else if (Key == KEY_LeftMouseButton)
	{
		switch (Event)
		{
			case IE_Released:
			{
				// sort all selected links, in case a drag changed the slot positions dramatically
				for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
				{
					SelectedLinks(Idx)->SortSlots();
				}
				bCanAltDrag = TRUE;
				break;
			}
			default:
			{
				break;
			}
		}
	}
	else if (Key == KEY_Delete)
	{
		if (Event == IE_Pressed)
		{
			// delete the selected slots
			const FScopedTransaction Transaction( *LocalizeUnrealEd("DeleteCoverSlots") );
			UBOOL bSelectedSlots = FALSE;
			for (INT LinkIdx = 0; LinkIdx < SelectedLinks.Num(); LinkIdx++)
			{
				ACoverLink *Link = SelectedLinks(LinkIdx);
				Link->Modify();
				for( INT Idx = 0; Idx < Link->Slots.Num(); Idx++ )
				{
					FCoverSlot& Slot = Link->Slots(Idx);

					if( Slot.bSelected )
					{
						// force a min of 2 slots for circular cover or 
						// a min of 1 slots for regular cover
						if( ( Link->bCircular && Link->Slots.Num() > 2) || 
							(!Link->bCircular && Link->Slots.Num() > 1)	)
						{
							// Clean up swat turn targets
							FCoverInfo LeftDestInfo;
							if( Link->GetCachedCoverInfo( Slot.GetLeftTurnTargetCoverRefIdx(), LeftDestInfo ) )
							{
								FCoverSlot& LeftDestSlot = LeftDestInfo.Link->Slots(LeftDestInfo.SlotIdx);
								LeftDestSlot.SetRightTurnTargetCoverRefIdx( MAXWORD );
							}

							FCoverInfo RightDestInfo;
							if( Link->GetCachedCoverInfo( Slot.GetRightTurnTargetCoverRefIdx(), RightDestInfo ) )
							{
								FCoverSlot& RightDestSlot = RightDestInfo.Link->Slots(RightDestInfo.SlotIdx);
								RightDestSlot.SetLeftTurnTargetCoverRefIdx( MAXWORD );
							}

							Link->Slots.Remove(Idx--,1);
						}

						bSelectedSlots = TRUE;
					}
				}

				Link->ForceUpdateComponents(FALSE,FALSE);
			}

			// If there were slots selected - just refresh viewport
			// Otherwise, fall through to delete actors
			if( bSelectedSlots )
			{
				GEditor->RedrawAllViewports();
				return 1;
			}
		}
	}
	else if (Key == KEY_Insert)
	{
		if (Event == IE_Pressed)
		{
			// insert a new slot
			for (INT LinkIdx = 0; LinkIdx < SelectedLinks.Num(); LinkIdx++)
			{
				ACoverLink *Link = SelectedLinks(LinkIdx);
				if (Link->bCircular &&
					Link->Slots.Num() >= 2)
				{
					// don't allow more than 2 slots for circular cover
					continue;
				}
				// look for a selected slot
				UBOOL bLinkSelected = 0;
				INT NumSlots = Link->Slots.Num();
				for (INT Idx = 0; Idx < NumSlots; Idx++)
				{
					FCoverSlot &Slot = Link->Slots(Idx);
					if( Slot.bSelected )
					{
						bLinkSelected = TRUE;
						// Create one with an offset to this slot
						// and match necessary information
						FCoverSlot NewSlot = Slot;
						NewSlot.LocationOffset += FVector(0.f,64.f,0.f);
						// Select new slot and deselect old one
						NewSlot.bSelected	= TRUE;
						Slot.bSelected		= FALSE;
						// add tot he list
						Link->Slots.AddItem(NewSlot);
					}
				}

				// if no links were selected create one at the end
				if (!bLinkSelected)
				{
					const INT SlotIdx = Link->Slots.AddZeroed();
					Link->Slots(SlotIdx).LocationOffset = Link->Slots(SlotIdx-1).LocationOffset + FVector(0,64.f,0);
					Link->Slots(SlotIdx).RotationOffset = Link->Slots(SlotIdx-1).RotationOffset;
					Link->Slots(SlotIdx).bEnabled = TRUE;
				}

				Link->ForceUpdateComponents(FALSE,FALSE);
			}

			// Auto adjust groups if no links selected
			if( !SelectedLinks.Num() )
			{
				for( INT GroupIdx = 0; GroupIdx < SelectedGroups.Num(); GroupIdx++ )
				{
					SelectedGroups(GroupIdx)->ToggleGroup();
				}
			}

			GEditor->RedrawAllViewports();
		}
		return 1;
	}
	else if (Key == KEY_MiddleMouseButton)
	{
		UBOOL bMouseMoved = ViewportClient->MouseDeltaTracker->GetDelta().Size() >= MOUSE_CLICK_DRAG_DELTA;
		// NOTE: we don't want to trap ALT+MIDDLE_CLICK here as that is a special case click used to move the pivot around
		if (Event == IE_Released && !bAltDown && !bMouseMoved)
		{
#if !FINAL_RELEASE
			// Clear nasty debug cylinder
			// REMOVE AFTER LDs SOLVE ISSUES
			AWorldInfo *Info = GWorld->GetWorldInfo();
			Info->FlushPersistentDebugLines();
#endif

			// auto-adjust selected slots
			for (INT LinkIdx = 0; LinkIdx < SelectedLinks.Num(); LinkIdx++)
			{
				ACoverLink *Link = SelectedLinks(LinkIdx);
				Link->SortSlots(&LastSelectedCoverSlot);
				Link->FindBase();
				UBOOL bLinkSelected = 0;

				for( INT Idx = 0; Idx < Link->Slots.Num(); Idx++ )
				{
					if( Link->Slots(Idx).bSelected )
					{
						bLinkSelected = 1;
						Link->AutoAdjustSlot( Idx, FALSE );
						Link->AutoAdjustSlot( Idx, TRUE );
						Link->BuildSlotInfo( Idx );
					}
				}

				// if no links were selected, then adjust all of them
				if( !bLinkSelected )
				{
					// first pass to auto position everything
					for( INT Idx = 0; Idx < Link->Slots.Num(); Idx++ )
					{
						Link->AutoAdjustSlot( Idx, FALSE );
					}
					// second pass to build the slot info
					for( INT Idx = 0; Idx < Link->Slots.Num(); Idx++ )
					{
						Link->AutoAdjustSlot( Idx, TRUE );
						Link->BuildSlotInfo( Idx );
					}
				}

				if (Link->Base != NULL && (Link->Base->GetOutermost() != Link->GetOutermost()))
				{
					Link->SetBase(NULL);
				}

				FPathBuilder::DestroyScout();
				Link->ForceUpdateComponents(FALSE,FALSE);
			}

			// Auto adjust groups if no links selected
			if( !SelectedLinks.Num() )
			{
				TArray<ACoverLink*> Junk;
				for( INT GroupIdx = 0; GroupIdx < SelectedGroups.Num(); GroupIdx++ )
				{
					ACoverGroup* Group = SelectedGroups(GroupIdx);
					Group->AutoFillGroup( CGFA_Cylinder, Junk );
				}
			}

			UBOOL bShowCursor = ViewportClient->ShouldCursorBeVisible();
			Viewport->ShowCursor( bShowCursor );
			Viewport->LockMouseToWindow( !bShowCursor );

			return 1;
		}
	}

	return FEdMode::InputKey(ViewportClient,Viewport,Key,Event);
}

UBOOL FEdModeCoverEdit::HandleClick(HHitProxy *HitProxy, const FViewportClick &Click)
{
	if (HitProxy != NULL)
	{
		if (HitProxy->IsA(HActorComplex::StaticGetType()))
		{
			TArray<ACoverLink*> SelectedLinks;
			GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks);

			HActorComplex *ComplexProxy = (HActorComplex*)HitProxy;
			ACoverLink *Link = Cast<ACoverLink>(ComplexProxy->Actor);
			if( Link != NULL )
			{
				FCoverSlot &Slot = Link->Slots(ComplexProxy->Index);
		
				if( Click.GetKey() == KEY_RightMouseButton )
				{
					if( !Click.IsControlDown() )
					{
						Slot.bSelected = TRUE;
						LastSelectedCoverSlot = &Slot;

						// Reattach the cover link's components, to show the selection update.
						Link->ForceUpdateComponents(FALSE,FALSE);
						// Redraw the viewport so the user can see which object was right clicked on
						GCurrentLevelEditingViewportClient->Viewport->Draw();
						// Wait for the viewport to finish drawing.
						FlushRenderingCommands();
						
						GEditor->ShowUnrealEdContextCoverSlotMenu(Link,Slot);
					}
				}
				else if( Click.GetKey() == KEY_LeftMouseButton )
				{
					if( !Click.IsControlDown() )
					{
						// deselect all other slots
						for( INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++ )
						{
							Link->Slots(SlotIdx).bSelected = FALSE;
						}

						Slot.bSelected = TRUE;
						LastSelectedCoverSlot = &Slot;
					}
					else
					{
						// Control clicking a slot will toggle it
						Slot.bSelected = !Slot.bSelected;
						if( Slot.bSelected )
						{
							LastSelectedCoverSlot = &Slot;
						}
						else if( LastSelectedCoverSlot == &Slot )
						{
							LastSelectedCoverSlot = NULL;
						}
					}

					// sort all selected links, in case a drag changed the slot positions dramatically
					for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
					{
						SelectedLinks(Idx)->SortSlots(&LastSelectedCoverSlot);
					}

				}

				// Reattach the cover link's components, to show the selection update.
				for( INT SelectIdx = 0; SelectIdx < SelectedLinks.Num(); SelectIdx++ )
				{
					Link = SelectedLinks(SelectIdx);
					if( Link != NULL )
					{
						Link->ForceUpdateComponents(FALSE,FALSE);
					}
				}

				// note that we handled the click
				return 1;
			}
		}
	}
	// if clicked on something other than a slot, deselect all slots
	if (HitProxy == NULL ||
		!HitProxy->IsA(HActor::StaticGetType()) ||
		((HActor*)HitProxy)->Actor == NULL ||
		!((HActor*)HitProxy)->Actor->IsA(ACoverLink::StaticClass()))
	{
		LastSelectedCoverSlot = NULL;

		// check to see if any slots are currently selected
		TArray<ACoverLink*> SelectedLinks;
		if (GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks) > 0)
		{
			UBOOL bHadSelectedSlots = 0;
			for (INT Idx = 0; Idx < SelectedLinks.Num(); Idx++)
			{
				ACoverLink *Link = SelectedLinks(Idx);
				for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
				{
					if (Link->Slots(SlotIdx).bSelected)
					{
						bHadSelectedSlots = 1;
						// deselect the slot
						Link->Slots(SlotIdx).bSelected = 0;
					}
				}
				if (bHadSelectedSlots)
				{
					Link->ForceUpdateComponents(FALSE,FALSE);
				}
			}
			// if we deselected slots,
			if (bHadSelectedSlots)
			{
				// then claim the click and keep the links selected
				return 1;
			}
		}
	}

	return FEdMode::HandleClick(HitProxy,Click);
}

/**
 * Overridden to duplicate selected slots instead of the actor.
 */
UBOOL FEdModeCoverEdit::HandleDuplicate()
{
	UBOOL bSlotDuplicated = FALSE;
	// grab a list of currently selected links
	TArray<ACoverLink*> SelectedLinks;
	GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks);
	// make a copy of all currently selected slots
	for (INT LinkIdx = 0; LinkIdx < SelectedLinks.Num(); LinkIdx++)
	{
		ACoverLink *Link = SelectedLinks(LinkIdx);
		const INT NumSlots = Link->Slots.Num();
		UBOOL bFoundLast = FALSE;
		for (INT Idx = 0; Idx < NumSlots; Idx++)
		{
			FCoverSlot& Slot = Link->Slots(Idx);
			if( Slot.bSelected )
			{
				// create a copy of the slot
				FCoverSlot NewSlot = Slot;
				// offset slightly
				NewSlot.LocationOffset += FVector(0.f,64.f,0.f);
				// select new one, deselect old one
				NewSlot.bSelected = TRUE;
				Slot.bSelected	   = FALSE;
				// note that a slot was duplicated
				bSlotDuplicated = TRUE;
				// and add this to the end of the list of slots for the link
				UINT index = Link->Slots.AddItem(NewSlot);
				if(!bFoundLast &&
					LastSelectedCoverSlot == &Slot)
				{
					LastSelectedCoverSlot = &Link->Slots(index);
					bFoundLast = TRUE;
				}
			}
		}
		Link->SortSlots(&LastSelectedCoverSlot);
		Link->ForceUpdateComponents(FALSE,FALSE);
	}
	return bSlotDuplicated;
}

/**
 * Overridden to drag duplicate a selected slot if possible, instead of the actor.
 */
UBOOL FEdModeCoverEdit::HandleDragDuplicate()
{
	UBOOL bSlotDuplicated = FALSE;
	// grab a list of currently selected links
	TArray<ACoverLink*> SelectedLinks;
	GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks);

	// make a copy of all currently selected slots
	for( INT LinkIdx = 0; LinkIdx < SelectedLinks.Num(); LinkIdx++ )
	{
		ACoverLink *Link = SelectedLinks(LinkIdx);

		// Store original slot count so we don't duplicate duplicates 
		// when we select them
		const INT SlotNum = Link->Slots.Num();
		UBOOL bFoundLast = FALSE;
		for( INT Idx = 0; Idx < SlotNum; Idx++ )
		{
			FCoverSlot &Slot = Link->Slots(Idx);
			// Only affect selected slots
			if( Slot.bSelected )
			{
				FCoverSlot NewSlot = Slot;
				// Select new slot - unselect old
				NewSlot.bSelected	= TRUE;
				Slot.bSelected		= FALSE;
				// Set return flag
				bSlotDuplicated		= TRUE;
				UINT index = Link->Slots.AddItem(NewSlot);

				if(!bFoundLast &&
					LastSelectedCoverSlot == &Slot)
				{
					LastSelectedCoverSlot = &Link->Slots(index);
					bFoundLast = TRUE;
				}
			}
		}
		Link->ForceUpdateComponents(FALSE,FALSE);
	}
	return bSlotDuplicated;
}

/**
 * Overridden to handle dragging/rotating cover link slots.
 */
UBOOL FEdModeCoverEdit::InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale)
{
	// Get some useful info about buttons being held down
	UBOOL bCtrlDown		= InViewport->KeyState(KEY_LeftControl) || InViewport->KeyState(KEY_RightControl);
	UBOOL bShiftDown	= InViewport->KeyState(KEY_LeftShift)	|| InViewport->KeyState(KEY_RightShift);
	UBOOL bAltDown		= InViewport->KeyState(KEY_LeftAlt)		|| InViewport->KeyState(KEY_RightAlt);
	UBOOL bTabDownKeyState = InViewport->KeyState(KEY_Tab);
	UBOOL bMovedObjects = 0;

	// If we have a valid axis, TAB is being held down and we have a cached CoverLink selected, move it while ignoring other selections
	if( InViewportClient->Widget->GetCurrentAxis() != AXIS_None && bTabDownKeyState && LastSelectedCoverLink )
	{
		for( INT SlotIdx = 0; SlotIdx < LastSelectedCoverLink->Slots.Num(); SlotIdx++ )
		{
			FCoverSlot &Slot = LastSelectedCoverLink->Slots(SlotIdx);
			// Apply the inverse location to keep the slots stationary in relation to the CoverLink
			Slot.LocationOffset -= FRotationMatrix(LastSelectedCoverLink->Rotation).InverseTransformFVectorNoScale(InDrag);
			Slot.RotationOffset -= InRot;

			bMovedObjects = 1;
		}
		LastSelectedCoverLink->ForceUpdateComponents(FALSE,FALSE);
		InViewportClient->ApplyDeltaToActor( LastSelectedCoverLink, InDrag, InRot, InScale );
	}
	else
	{
		TArray<ACoverLink*> SelectedLinks;
		GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks);

		// Look for a selected slot
		UBOOL bSlotsSelected = 0;
		for( INT LinkIdx = 0; LinkIdx < SelectedLinks.Num(); LinkIdx++ )
		{
			ACoverLink* Link = SelectedLinks(LinkIdx);
			for( INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++ )
			{
				FCoverSlot &Slot = Link->Slots(SlotIdx);
				if( Slot.bSelected )
				{
					bSlotsSelected = 1;
				}
			}		
		}

		if ( bSlotsSelected )
		{
			if( bAltDown && bCanAltDrag )
			{
				bCanAltDrag = FALSE;
				HandleDragDuplicate();
			}
			for( INT LinkIdx = 0; LinkIdx < SelectedLinks.Num(); LinkIdx++ )
			{
				ACoverLink* Link = SelectedLinks(LinkIdx);
				for( INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++ )
				{
					FCoverSlot &Slot = Link->Slots(SlotIdx);
					// Move only selected slots if we grabbed widget or holding ctrl
					if( Slot.bSelected && 
						(InViewportClient->Widget->GetCurrentAxis() != AXIS_None ||
						bCtrlDown) )
					{
						// Update slot offsets
						Slot.LocationOffset += FRotationMatrix(Link->Rotation).InverseTransformFVectorNoScale( InDrag );
						Slot.RotationOffset += InRot;

						// Limit the pitch/roll
						Slot.RotationOffset.Pitch	= 0;
						Slot.RotationOffset.Roll	= 0;

						// Set flag saying we handled all movement
						bMovedObjects = 1;
					}
				}
				Link->ForceUpdateComponents(FALSE,FALSE);
			}
		}
	}

	if( bMovedObjects )
	{
		// If holding shift - adjust camera with slots
		if( bShiftDown )
		{
			FVector CameraDelta( InDrag );
			if( InViewportClient->ViewportType == LVT_OrthoXY )
			{
				CameraDelta.X = -InDrag.Y;
				CameraDelta.Y =  InDrag.X;
			}
			InViewportClient->MoveViewportCamera( CameraDelta, FRotator(0,0,0) );
		}

		return 1;
	}
	else
	{
		return FEdMode::InputDelta(InViewportClient,InViewport,InDrag,InRot,InScale);
	}
}

/**
 * Returns the current selected slot location.
 */
FVector FEdModeCoverEdit::GetWidgetLocation() const
{
	// Short circuit widget location if we have a cached CoverLink and tab is down
	if( bTabDown && LastSelectedCoverLink )
	{
		//Set the pivot and snapped location to the slot's position.
		GEditorModeTools().PivotLocation = GEditorModeTools().SnappedLocation = LastSelectedCoverLink->Location;
		return LastSelectedCoverLink->Location;
	}

	// grab a list of currently selected links
	TArray<ACoverLink*> SelectedLinks;
	GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks);

	// Find an appropriate widget location for slots (if any are selected)
	FVector WidgetLocation = FVector(0.f,0.f,0.f);
	UBOOL bFoundSlot = FALSE;
	for( INT LinkIdx=0; LinkIdx<SelectedLinks.Num(); ++LinkIdx )
	{
		ACoverLink *Link = SelectedLinks(LinkIdx);
		// check to see if a slot is selected
		for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
		{
			FCoverSlot& Slot = Link->Slots(SlotIdx);
			if( LastSelectedCoverSlot == &Slot )
			{
				//Set the pivot and snapped location to the slot's position.
				GEditorModeTools().PivotLocation = GEditorModeTools().SnappedLocation = Link->GetSlotLocation(SlotIdx);
				return (Link->Location + FRotationMatrix(Link->Rotation).TransformFVector(Slot.LocationOffset));
			}
			else if (Slot.bSelected)
			{
				bFoundSlot = TRUE;

				//Set the pivot and snapped location to the slot's position.
				GEditorModeTools().PivotLocation = GEditorModeTools().SnappedLocation = Link->GetSlotLocation(SlotIdx);
				WidgetLocation = (Link->Location + FRotationMatrix(Link->Rotation).TransformFVector(Slot.LocationOffset));
			}
		}
	}
	
	// If a slot was not found, try to use a link or other selected actor
	if(!bFoundSlot)
	{
		if( SelectedLinks.Num() )
		{
			WidgetLocation = LastSelectedCoverLink != NULL ? LastSelectedCoverLink->Location : SelectedLinks(0)->Location;
		}
		else
		{
			TArray<AActor*> SelectedActors;
			GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);
			if( SelectedActors.Num() )
			{
				WidgetLocation = SelectedActors(0)->Location;
			}
		}
	}

	// If nothing else, just use whatever widget location was calculated.
	GEditorModeTools().PivotLocation = GEditorModeTools().SnappedLocation = WidgetLocation;
	return WidgetLocation;
}

/**
 * Draw some simple information about the currently selected links.
 */
void FEdModeCoverEdit::DrawHUD(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas)
{
	FEdMode::DrawHUD(ViewportClient,Viewport,View,Canvas);

	if (!ViewportClient->IsOrtho())
	{
		// grab a list of currently selected links
		TArray<ACoverLink*> SelectedLinks;
		GEditor->GetSelectedActors()->GetSelectedObjects<ACoverLink>(SelectedLinks);
		for (INT LinkIdx = 0; LinkIdx < SelectedLinks.Num(); LinkIdx++)
		{
			ACoverLink *Link = SelectedLinks(LinkIdx);
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				// and draw the slot index overtop the actual slot
				FVector SlotLocation = Link->GetSlotLocation(SlotIdx);
				FVector2D DrawLocation(0,0);
				if (View->ScreenToPixel(View->WorldToScreen(SlotLocation),DrawLocation))
				{
					DrawStringCenteredZ(Canvas,DrawLocation.X,DrawLocation.Y,1.0f,*FString::Printf(TEXT("%d"),SlotIdx),GEngine->GetMediumFont(),FColor(200,200,200));
				}
			}
		}
	}
}




/*------------------------------------------------------------------------------
	FEditorModeTools.

	The master class that handles tracking of the current mode.
------------------------------------------------------------------------------*/

FEditorModeTools::FEditorModeTools()
	:	PivotShown( 0 )
	,	Snapping( 0 )
	,	TranslateRotateXAxisAngle(0)
	,	TotalDeltaRotation(0)
	,	CoordSystem( COORD_World )
	,	WidgetMode( FWidget::WM_Translate )
	,	OverrideWidgetMode( FWidget::WM_None )
	,	bAllowTranslateRotateZWidget(0)
	,	bUseAbsoluteTranslation(0)
	,	bShowWidget( 1 )
	,	bMouseLock( 0 )
	,	bPanMovesCanvas(TRUE)
	,	bCenterZoomAroundCursor(TRUE)
	,	bReplaceRespectsScale(TRUE)
	,	bHighlightWithBrackets(FALSE)
	,	bClickBSPSelectsBrush(FALSE)
	,	bBSPAutoUpdate(FALSE)
{}

FEditorModeTools::~FEditorModeTools()
{
	// Unregister all of our events
	GCallbackEvent->UnregisterAll(this);
}

// A simple macro to help set editor mode compatibility mappings
#define SET_MODE_COMPATABILITY( InMode1, InMode2  ) \
	ModeCompatabilityMap.Add( InMode1, InMode2 ); \
	ModeCompatabilityMap.Add( InMode2, InMode1 ); \

void FEditorModeTools::Init()
{
	// Load the last used settings
	LoadConfig();

	// Editor modes
	Modes.Empty();
	Modes.AddItem( new FEdModeDefault() );
	Modes.AddItem( new FEdModeGeometry() );
	Modes.AddItem( new FEdModeTerrainEditing() );
	Modes.AddItem( new FEdModeTexture() );
	Modes.AddItem( new FEdModeCoverEdit() );
	Modes.AddItem( new FEdModeMeshPaint() );
	Modes.AddItem( new FEdModeSpline() );
	Modes.AddItem( new FEdModeStaticMesh() );
	Modes.AddItem( new FEdModeInterpEdit() );
	Modes.AddItem( new FEdModeLandscape() );
	Modes.AddItem( new FEdModeFoliage() );
	Modes.AddItem( new FEdModeAmbientSoundSpline() );

	// Set which modes can run together
	
	// Interp edit and mesh paint can run together
	SET_MODE_COMPATABILITY( EM_InterpEdit, EM_MeshPaint );
	// Interp edit and geometry mode can run together
	SET_MODE_COMPATABILITY( EM_InterpEdit, EM_Geometry );

	// Register our callback for actor selection changes
	GCallbackEvent->Register(CALLBACK_SelectObject,this);
	GCallbackEvent->Register(CALLBACK_SelChange,this);
	GCallbackEvent->Register(CALLBACK_SelectNone,this);
}

/**
 * Called from UUnrealEdEngine::PreExit
 */
void FEditorModeTools::Shutdown()
{
	// Clean up modes
	ActiveModes.Empty();
	ModeCompatabilityMap.Empty();
	for( INT ModeIndex = 0; ModeIndex < Modes.Num(); ++ModeIndex )
	{
		delete Modes(ModeIndex);
	}
	Modes.Empty();
}

/**
 * Loads the state that was saved in the INI file
 */
void FEditorModeTools::LoadConfig(void)
{
	GConfig->GetBool(TEXT("FEditorModeTools"),TEXT("ShowWidget"),bShowWidget,
		GEditorUserSettingsIni);
	GConfig->GetBool(TEXT("FEditorModeTools"),TEXT("MouseLock"),bMouseLock,
		GEditorUserSettingsIni);

	// Ensure no widget mode is active if the mouse is locked
	if ( bMouseLock )
	{
		SetWidgetMode( FWidget::WM_None );
	}

	INT Bogus = (INT)CoordSystem;
	GConfig->GetInt(TEXT("FEditorModeTools"),TEXT("CoordSystem"),Bogus,
		GEditorUserSettingsIni);
	CoordSystem = (ECoordSystem)Bogus;

	// Load user settings from the editor user settings .ini.
	GConfig->GetBool( TEXT("UnEdViewport"), TEXT("MoveCanvas"), bPanMovesCanvas, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("UnEdViewport"), TEXT("CenterZoomAroundCursor"), bCenterZoomAroundCursor, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("UnEdViewport"), TEXT("InterpEdPanInvert"), bInterpPanInverted, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("UnEdViewport"), TEXT("ReplaceRespectsScale"), bReplaceRespectsScale, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("UnEdViewport"), TEXT("HighlightWithBrackets"), bHighlightWithBrackets, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("UnEdViewport"), TEXT("ClickingBSPSelectsBrush"), bClickBSPSelectsBrush, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("UnEdViewport"), TEXT("BSPAutoUpdate"), bBSPAutoUpdate, GEditorUserSettingsIni );

	GEngine->SelectedMaterialColor = bHighlightWithBrackets ? FLinearColor::Black : GEngine->DefaultSelectedMaterialColor;

	LoadWidgetSettings();
}

/**
 * Saves the current state to the INI file
 */
void FEditorModeTools::SaveConfig(void)
{
	GConfig->SetBool(TEXT("FEditorModeTools"),TEXT("ShowWidget"),bShowWidget,
		GEditorUserSettingsIni);
	GConfig->SetBool(TEXT("FEditorModeTools"),TEXT("MouseLock"),bMouseLock,
		GEditorUserSettingsIni);
	GConfig->SetInt(TEXT("FEditorModeTools"),TEXT("CoordSystem"),(INT)CoordSystem,
		GEditorUserSettingsIni);

	// Load user settings from the editor user settings .ini.
	GConfig->SetBool( TEXT("UnEdViewport"), TEXT("MoveCanvas"), bPanMovesCanvas, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("UnEdViewport"), TEXT("CenterZoomAroundCursor"), bCenterZoomAroundCursor, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("UnEdViewport"), TEXT("InterpEdPanInvert"), bInterpPanInverted, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("UnEdViewport"), TEXT("ReplaceRespectsScale"), bReplaceRespectsScale, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("UnEdViewport"), TEXT("HighlightWithBrackets"), bHighlightWithBrackets, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("UnEdViewport"), TEXT("ClickingBSPSelectsBrush"), bClickBSPSelectsBrush, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("UnEdViewport"), TEXT("BSPAutoUpdate"), bBSPAutoUpdate, GEditorUserSettingsIni );

	SaveWidgetSettings();
}

/**
 * Handles notification of an object selection change. Updates the
 * Pivot and Snapped location values based upon the selected actor
 *
 * @param InType the event that was fired
 * @param InObject the object associated with this event
 */
void FEditorModeTools::Send(ECallbackEventType InType,UObject* InObject)
{
	if (InType == CALLBACK_SelectObject || InType == CALLBACK_SelChange)
	{
		// If selecting an actor, move the pivot location.
		AActor* Actor = Cast<AActor>(InObject);
		if ( Actor != NULL)
		{
			//@fixme - why isn't this using UObject::IsSelected()?
			if ( GEditor->GetSelectedActors()->IsSelected( Actor ) )
			{
				PivotLocation = SnappedLocation = Actor->Location;
			}
		}

		// Don't do auto-switch-to-cover-editing when in Matinee, as it will exit Matinee in a bad way.
		if( !IsModeActive(EM_InterpEdit) )
		{
			// if the new selection is in cover mode, reset into cover mode to update the cover links properly
			if ( Actor != NULL && 
				Actor->IsSelected() && 
				(Actor->IsA(ACoverLink::StaticClass()) ||
				Actor->IsA(ACoverGroup::StaticClass())) )
			{
				//switch to default as a temp way of making sure the cover edit mode takes
				ActivateMode(EM_Default);
				// switch to cover edit
				ActivateMode(EM_CoverEdit);
			}

			// if already editing cover
			if ( IsModeActive(EM_CoverEdit) )
			{
				// determine if we have cover links selected
				UBOOL bHasCoverLinkSelected = FALSE;
				for ( FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It )
				{
					ACoverLink *Link = Cast<ACoverLink>(*It);
					if (Link != NULL && Link->IsSelected())
					{
						bHasCoverLinkSelected = TRUE;
						break;
					}
					ACoverGroup *Group = Cast<ACoverGroup>(*It);
					if( Group != NULL && Group->IsSelected() )
					{
						bHasCoverLinkSelected = TRUE;
						break;
					}
				}
				// if no links selected
				if ( !bHasCoverLinkSelected )
				{
					// clear out cover edit mode
					ActivateMode(EM_Default);
				}
			}
				
			// Auto-switch to/from spline editing mode
			if( IsModeActive(EM_Spline) )
			{
				UBOOL bHasSplineSelected = FALSE;
				for ( FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It )
				{
					ASplineActor *Spline = Cast<ASplineActor>(*It);
					if (Spline != NULL && Spline->IsSelected())
					{
						bHasSplineSelected = TRUE;
						break;
					}
				}
				// if no links selected, clear out splineedit mode
				if (!bHasSplineSelected)
				{
					ActivateMode(EM_Default);
				}
			}			
			// spline selected, change to spline mode
			else if(Actor != NULL && Actor->IsSelected() && Actor->IsA(ASplineActor::StaticClass()))
			{
				ActivateMode(EM_Spline);			
			}

				// mode for editing actors inherited from AAmbientSoundSpline
			if(IsModeActive(EM_AmbientSoundSpline))
			{
				UBOOL bHasSplineSelected = FALSE;
				// if any AmbientSoundSpline actor is selected stay in this mode
				for ( FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It )
				{
					AAmbientSoundSpline *Spline = Cast<AAmbientSoundSpline>(*It);
					if (Spline != NULL && Spline->IsSelected())
					{
						bHasSplineSelected = TRUE;
						break;
					}
				}
				// if no links selected, clear out splineedit mode
				if (!bHasSplineSelected)
				{
					ActivateMode(EM_Default);
				}
			}
			else if(Actor != NULL && Actor->IsSelected() && Actor->IsA(AAmbientSoundSpline::StaticClass()))
			{
				ActivateMode(EM_AmbientSoundSpline);		
			}
		}
	}
}

void FEditorModeTools::Send(ECallbackEventType InType)
{
	if (InType == CALLBACK_SelectNone)
	{
		GEditor->SelectNone( FALSE, TRUE );
	}
}

/** 
 * Returns a list of active modes that are incompatible with the passed in mode.
 * 
 * @param InID 				The mode to check for incompatibilites.
 * @param IncompatibleModes	The list of incompatible modes.
 */
void FEditorModeTools::GetIncompatibleActiveModes( EEditorMode InID, TArray<FEdMode*>& IncompatibleModes )
{
	// Find all modes which are compatible with the one passed in.
	TArray<EEditorMode> CompatibleModes;
	ModeCompatabilityMap.MultiFind( InID, CompatibleModes );

	// Search through all active modes for incompatible ones
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		FEdMode* ActiveMode = ActiveModes(ModeIndex);

		if( !CompatibleModes.ContainsItem( ActiveMode->GetID() ) )
		{
			// This active mode is not compatible with the one that was passed in.
			IncompatibleModes.AddItem ( ActiveMode );
		}
	}
}


/**
 * Deactivates an editor mode. 
 * 
 * @param InID		The ID of the editor mode to deactivate.
 */
void FEditorModeTools::DeactivateMode( EEditorMode InID )
{
	// If the mode is already inactive do nothing
	if( !IsModeActive(InID) )
	{
		return;
	}

	// Find the active mode from the ID and exit it.
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		FEdMode* Mode = ActiveModes(ModeIndex);
		if( Mode->GetID() == InID )
	{
			Mode->Exit();
			ActiveModes.RemoveItem( Mode );
			break;
		}
	}

	if( ActiveModes.Num() == 0 )
	{
		// Ensure the default mode is active if there are no active modes.
		ActivateMode( EM_Default );
	}
}

/**
 * Activates an editor mode. Shuts down all other active modes which cannot run with the passed in mode.
 * 
 * @param InID		The ID of the editor mode to activate.
 * @param bToggle	TRUE if the passed in editor mode should be toggled off if it is already active.
 */
void FEditorModeTools::ActivateMode( EEditorMode InID, UBOOL bToggle )
{
	// Check to see if the mode is already active
	if( IsModeActive(InID) )
	{
		// The mode is already active toggle it off if we should toggle off already active modes.
		if( bToggle )
		{
			DeactivateMode( InID );
		}
		// Nothing more to do
		return;
	}

	// Get a list of modes which are incompatible with the one we are trying to activate
	TArray<FEdMode*> IncompatibleModes;
	GetIncompatibleActiveModes( InID, IncompatibleModes );

	// Exit all incompatible modes
	for( INT ModeIndex = 0; ModeIndex < IncompatibleModes.Num(); ++ModeIndex )
	{
		FEdMode* Mode = IncompatibleModes(ModeIndex);
		Mode->Exit();
		ActiveModes.RemoveItem( Mode );
	}

	// Find the new mode from its ID
	FEdMode* NewMode = FindMode( InID );

	if( !NewMode )
	{
		// Couldn't find a valid mode. Activate the default mode
		debugf( TEXT("FEditorModeTools::SetCurrentMode : Couldn't find mode %d.  Using default."), (INT)InID );
		// We must call this function again to remove any other active modes that aren't compatible with the default
		ActivateMode( EM_Default );
	}
	else
	{
		// Add the new mode to the list of active ones
		ActiveModes.AddItem( NewMode );

		// Enter the new mode
		NewMode->Enter();
	
		// Update the editor UI
	GCallbackEvent->Send( CALLBACK_UpdateUI );
}
}

/**
 * Returns TRUE if the current mode is not the specified ModeID.  Also optionally warns the user.
 *
 * @param	ModeID			The editor mode to query.
 * @param	bNotifyUser		If TRUE, inform the user that the requested operation cannot be performed in the specified mode.
 * @return					TRUE if the current mode is not the specified mode.
 */
UBOOL FEditorModeTools::EnsureNotInMode(EEditorMode ModeID, UBOOL bNotifyUser) const
{
	// We're in a 'safe' mode if we're not in the specified mode.
	const UBOOL bInASafeMode = !IsModeActive(ModeID);
	if( !bInASafeMode && bNotifyUser )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("EnsureNotInMode") );
	}
	return bInASafeMode;
}

FEdMode* FEditorModeTools::FindMode( EEditorMode InID )
{
	for( INT x = 0 ; x < Modes.Num() ; x++ )
	{
		if( Modes(x)->ID == InID )
		{
			return Modes(x);
		}
	}

	return NULL;
}

/**
 * Returns a coordinate system that should be applied on top of the worldspace system.
 */

FMatrix FEditorModeTools::GetCustomDrawingCoordinateSystem()
{
	FMatrix matrix = FMatrix::Identity;

	switch( CoordSystem )
	{
		case COORD_Local:
		{
			// Let the current mode have a shot at setting the local coordinate system.
			// If it doesn't want to, create it by looking at the currently selected actors list.

			// @todo Modes with custom drawing systems should probably not be active at once
			if( !ActiveModes(0)->GetCustomDrawingCoordinateSystem( matrix, NULL ) )
			{
				if( IsModeActive( EM_Texture ) )
				{
					// Texture mode is ALWAYS in local space, so do not switch the coordinate system in that case.
					CoordSystem = COORD_Local;
				}
				else
				{
					const INT Num = GEditor->GetSelectedActors()->CountSelections<AActor>();

					// Coordinate system needs to come from the last actor selected
					if( Num > 0 )
					{
						matrix = FRotationMatrix( GEditor->GetSelectedActors()->GetBottom<AActor>()->Rotation );
					}
				}
			}
		}
		break;

		case COORD_World:
			break;

		default:
			break;
	}

	matrix.RemoveScaling();

	return matrix;
}

FMatrix FEditorModeTools::GetCustomInputCoordinateSystem()
{
	FMatrix matrix = FMatrix::Identity;

	switch( CoordSystem )
	{
		case COORD_Local:
		{
			// Let the current mode have a shot at setting the local coordinate system.
			// If it doesn't want to, create it by looking at the currently selected actors list.

			// @todo Modes with custom drawing systems should probably not be active at once
			if( !ActiveModes(0)->GetCustomDrawingCoordinateSystem( matrix, NULL ) )
			{
				const INT Num = GEditor->GetSelectedActors()->CountSelections<AActor>();

				if( Num > 0 )
				{
					// Coordinate system needs to come from the last actor selected
					matrix = FRotationMatrix( GEditor->GetSelectedActors()->GetBottom<AActor>()->Rotation );
				}
			}
		}
		break;

		case COORD_World:
			break;

		default:
			break;
	}

	matrix.RemoveScaling();

	return matrix;
}

/** Gets the widget axis to be drawn */
INT FEditorModeTools::GetWidgetAxisToDraw( FWidget::EWidgetMode InWidgetMode ) const
{
	INT OutAxis = AXIS_XYZ;
	if( ActiveModes.Num() > 0 )
	{
		// first active mode is the top priority so get the axis from it.
		OutAxis = ActiveModes(0)->GetWidgetAxisToDraw( InWidgetMode );
	}

	return OutAxis;
}

/** Mouse tracking interface.  Passes tracking messages to all active modes */
UBOOL FEditorModeTools::StartTracking()
{
	UBOOL bTransactionHandled = FALSE;
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bTransactionHandled |= ActiveModes(ModeIndex)->StartTracking();
	}

	return bTransactionHandled;
}

/** Mouse tracking interface.  Passes tracking messages to all active modes */
UBOOL FEditorModeTools::EndTracking()
{
	UBOOL bTransactionHandled = FALSE;
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bTransactionHandled |= ActiveModes(ModeIndex)->EndTracking();
	}

	return bTransactionHandled;
}

/** Notifies all active modes that a map change has occured */
void FEditorModeTools::MapChangeNotify()
{
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		ActiveModes(ModeIndex)->MapChangeNotify();
	}
}


/** Notifies all active modes to empty their selections */
void FEditorModeTools::SelectNone()
{
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		ActiveModes(ModeIndex)->SelectNone();
	}
}

/** Notifies all active modes of box selection attempts */
UBOOL FEditorModeTools::BoxSelect( FBox& InBox, UBOOL InSelect )
{
	UBOOL bHandled = FALSE;
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bHandled |= ActiveModes(ModeIndex)->BoxSelect( InBox, InSelect );
	}
	return bHandled;
}

/** Notifies all active modes of frustum selection attempts */
UBOOL FEditorModeTools::FrustumSelect( const FConvexVolume& InFrustum, UBOOL InSelect )
{
	UBOOL bHandled = FALSE;
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bHandled |= ActiveModes(ModeIndex)->FrustumSelect( InFrustum, InSelect );
	}
	return bHandled;
}


/** TRUE if any active mode uses a transform widget */
UBOOL FEditorModeTools::UsesTransformWidget() const
{
	UBOOL bUsesTransformWidget = FALSE;
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bUsesTransformWidget |= ActiveModes(ModeIndex)->UsesTransformWidget();
	}

	return bUsesTransformWidget;
}

/** TRUE if any active mode uses the passed in transform widget */
UBOOL FEditorModeTools::UsesTransformWidget( FWidget::EWidgetMode CheckMode ) const
{
	UBOOL bUsesTransformWidget = FALSE;
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bUsesTransformWidget |= ActiveModes(ModeIndex)->UsesTransformWidget(CheckMode);
	}

	return bUsesTransformWidget;
}

/** Sets the current widget axis */
void FEditorModeTools::SetCurrentWidgetAxis( EAxis NewAxis )
{
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		ActiveModes(ModeIndex)->SetCurrentWidgetAxis( NewAxis );
	}
}

/** Notifies all active modes of mouse click messages. */
UBOOL FEditorModeTools::HandleClick( HHitProxy *HitProxy, const FViewportClick& Click )
{
	UBOOL bHandled = FALSE;
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bHandled |= ActiveModes(ModeIndex)->HandleClick(HitProxy, Click);
	}

	return bHandled;
}

/** TRUE if the passed in brush actor should be drawn in wireframe */	
UBOOL FEditorModeTools::ShouldDrawBrushWireframe( AActor* InActor ) const
{
	UBOOL bShouldDraw = FALSE;
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bShouldDraw |= ActiveModes(ModeIndex)->ShouldDrawBrushWireframe( InActor );
	}

	if( ActiveModes.Num() == 0 )
	{
		// We can get into a state where there are no active modes at editor startup if the builder brush is created before the default mode is activated.
		// Ensure we can see the builder brush when no modes are active.
		bShouldDraw = TRUE;
	}
	return bShouldDraw;
}

/** TRUE if brush vertices should be drawn */
UBOOL FEditorModeTools::ShouldDrawBrushVertices() const
{
	// Currently only geometry mode being active prevents vertices from being drawn.
	return !IsModeActive( EM_Geometry );
}

/** Ticks all active modes */
void FEditorModeTools::Tick( FEditorLevelViewportClient* ViewportClient, FLOAT DeltaTime )
{
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		ActiveModes(ModeIndex)->Tick( ViewportClient, DeltaTime );
	}
}

/** Notifies all active modes of any change in mouse movement */
UBOOL FEditorModeTools::InputDelta( FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale )
{
	UBOOL bHandled = FALSE;
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bHandled |= ActiveModes(ModeIndex)->InputDelta( InViewportClient, InViewport, InDrag, InRot, InScale );
	}
	return bHandled;
}

/** Notifies all active modes of captured mouse movement */	
UBOOL FEditorModeTools::CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY )
{
	UBOOL bHandled = FALSE;
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bHandled |= ActiveModes(ModeIndex)->CapturedMouseMove( InViewportClient, InViewport, InMouseX, InMouseY );
	}
	return bHandled;
}

/** Notifies all active modes of keyboard input */
UBOOL FEditorModeTools::InputKey(FEditorLevelViewportClient* InViewportClient,FViewport* Viewport,FName Key,EInputEvent Event)
{
	UBOOL bHandled = FALSE;
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bHandled |= ActiveModes(ModeIndex)->InputKey( InViewportClient, Viewport, Key, Event );
	}
	return bHandled;
}

/** Notifies all active modes of axis movement */
UBOOL FEditorModeTools::InputAxis(FEditorLevelViewportClient* InViewportClient,FViewport* Viewport,INT ControllerId,FName Key,FLOAT Delta,FLOAT DeltaTime)
{
	UBOOL bHandled = FALSE;
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bHandled |= ActiveModes(ModeIndex)->InputAxis( InViewportClient, Viewport, ControllerId, Key, Delta, DeltaTime );
	}
	return bHandled;
}

/** Notifies all active modes that the mouse has moved */
UBOOL FEditorModeTools::MouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* Viewport, INT X, INT Y )
{
	UBOOL bHandled = FALSE;
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bHandled |= ActiveModes(ModeIndex)->MouseMove( InViewportClient, Viewport, X, Y );
	}
	return bHandled;
}

/** Draws all active mode components */	
void FEditorModeTools::DrawComponents( const FSceneView* InView, FPrimitiveDrawInterface* PDI )
{
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		ActiveModes(ModeIndex)->GetComponent()->Draw( InView, PDI );
	}
}

/** Renders all active modes */
void FEditorModeTools::Render( const FSceneView* InView, FViewport* Viewport, FPrimitiveDrawInterface* PDI )
{
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		ActiveModes(ModeIndex)->Render( InView, Viewport, PDI );
	}
}

/** Draws the HUD for all active modes */
void FEditorModeTools::DrawHUD( FEditorLevelViewportClient* InViewportClient,FViewport* Viewport, const FSceneView* View, FCanvas* Canvas )
{
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		ActiveModes(ModeIndex)->DrawHUD( InViewportClient, Viewport, View, Canvas );
	}
}

/** Updates all active mode components */
void FEditorModeTools::UpdateComponents()
{
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		ActiveModes(ModeIndex)->UpdateComponent();
	}
}

/** Clears all active mode components */
void FEditorModeTools::ClearComponents()
{
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		ActiveModes(ModeIndex)->ClearComponent();
	}
}

/** Calls PostUndo on all active components */
void FEditorModeTools::PostUndo()
{
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		ActiveModes(ModeIndex)->PostUndo();
	}
}

/** Called when an object is duplicated.  Sends the message to all active components */
UBOOL FEditorModeTools::HandleDuplicate()
{
	UBOOL bHandled = FALSE;
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bHandled |= ActiveModes(ModeIndex)->HandleDuplicate();
	}
	return bHandled;
}

/** TRUE if we should allow widget move */
UBOOL FEditorModeTools::AllowWidgetMove() const
{
	UBOOL bAllow = FALSE;
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bAllow |= ActiveModes(ModeIndex)->AllowWidgetMove();
	}
	return bAllow;
}

/**
 * Used to cycle widget modes
 */
void FEditorModeTools::CycleWidgetMode (void)
{
	//make sure we're not currently tracking mouse movement.  If we are, changing modes could cause a crash due to referencing an axis/plane that is incompatible with the widget
	for(INT ViewportIndex = 0;ViewportIndex < GEditor->ViewportClients.Num();ViewportIndex++)
	{
		FEditorLevelViewportClient* ViewportClient = GEditor->ViewportClients( ViewportIndex );
		if (ViewportClient->bIsTracking)
		{
			return;
		}
	}

	//if the widget is currently hidden, flash it up for a while to show that we're changing mode...
	if( !GetShowWidget() )
	{
		FlashWidget();
	}

		const INT CurrentWk = GetWidgetMode();
		INT Wk = CurrentWk;
		// don't allow cycling through non uniform scaling
		const INT MaxWidget = (INT)FWidget::WM_ScaleNonUniform;
		do
		{
			Wk++;
			if (Wk == FWidget::WM_ScaleNonUniform)
			{
				Wk++;
			}
			if ((Wk == FWidget::WM_TranslateRotateZ) && (!bAllowTranslateRotateZWidget))
			{
				Wk++;
			}
			// Roll back to the start if we go past FWidget::WM_Scale
			if( Wk >= FWidget::WM_Max)
			{
				Wk -= FWidget::WM_Max;
			}
		}
		while (!GEditorModeTools().UsesTransformWidget((FWidget::EWidgetMode)Wk) && Wk != CurrentWk);
		SetWidgetMode( (FWidget::EWidgetMode)Wk );
		GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
		// Turn off selection mode when the widget changes;
		SetMouseLock(FALSE);
}

/**Save Widget Settings to Ini file*/
void FEditorModeTools::SaveWidgetSettings(void)
{
	GConfig->SetBool(TEXT("FEditorModeTools"),TEXT("UseAbsoluteTranslation"), bUseAbsoluteTranslation, GEditorUserSettingsIni);
	GConfig->SetBool(TEXT("FEditorModeTools"),TEXT("AllowTranslateRotateZWidget"), bAllowTranslateRotateZWidget, GEditorUserSettingsIni);
}

/**Load Widget Settings from Ini file*/
void FEditorModeTools::LoadWidgetSettings(void)
{
	GConfig->GetBool(TEXT("FEditorModeTools"),TEXT("UseAbsoluteTranslation"), bUseAbsoluteTranslation, GEditorUserSettingsIni);
	GConfig->GetBool(TEXT("FEditorModeTools"),TEXT("AllowTranslateRotateZWidget"), bAllowTranslateRotateZWidget, GEditorUserSettingsIni);
}

/**
 * Returns a good location to draw the widget at.
 */

FVector FEditorModeTools::GetWidgetLocation() const
{
	//debugf(TEXT("In FEditorModeTools::GetWidgetLocation"));
	//@todo multiple widgets for multiple active modes?
	return ActiveModes(0)->GetWidgetLocation();
}

/**
 * Changes the current widget mode.
 */

void FEditorModeTools::SetWidgetMode( FWidget::EWidgetMode InWidgetMode )
{
	WidgetMode = InWidgetMode;
}

/**
 * Allows you to temporarily override the widget mode.  Call this function again
 * with FWidget::WM_None to turn off the override.
 */

void FEditorModeTools::SetWidgetModeOverride( FWidget::EWidgetMode InWidgetMode )
{
	OverrideWidgetMode = InWidgetMode;
}

/**
 * Retrieves the current widget mode, taking overrides into account.
 */

FWidget::EWidgetMode FEditorModeTools::GetWidgetMode() const
{
	if( OverrideWidgetMode != FWidget::WM_None )
	{
		return OverrideWidgetMode;
	}

	return WidgetMode;
}

/**
 * Sets a bookmark in the levelinfo file, allocating it if necessary.
 */

void FEditorModeTools::SetBookmark( UINT InIndex, FEditorLevelViewportClient* InViewportClient )
{
	if ( GWorld )
	{
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();

		// Verify the index is valid for the bookmark
		if ( WorldInfo && InIndex < UCONST_MAX_BOOKMARK_NUMBER )
		{
			// If the index doesn't already have a bookmark in place, create a new one
			if ( !WorldInfo->BookMarks[ InIndex ] )
			{
				WorldInfo->BookMarks[ InIndex ] = ConstructObject<UBookMark>( UBookMark::StaticClass(), WorldInfo );
			}

			UBookMark* CurBookMark = WorldInfo->BookMarks[ InIndex ];
			check(CurBookMark);
			check(InViewportClient);

			// Use the rotation from the first perspective viewport can find.
			FRotator Rotation(0,0,0);
			if( !InViewportClient->IsOrtho() )
			{
				Rotation = InViewportClient->ViewRotation;
			}

			CurBookMark->Location = InViewportClient->ViewLocation;
			CurBookMark->Rotation = Rotation;

			// Keep a record of which levels were hidden so that we can restore these with the bookmark
			CurBookMark->HiddenLevels.Empty();
			for ( INT LevelIndex = 0 ; LevelIndex < WorldInfo->StreamingLevels.Num() ; ++LevelIndex )
			{
				ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
				if ( StreamingLevel )
				{
					if( !StreamingLevel->bShouldBeVisibleInEditor )
					{
						CurBookMark->HiddenLevels.AddItem( StreamingLevel->GetFullName() );
					}
				}
			}
		}
	}
}

/**
 * Checks to see if a bookmark exists at a given index
 */

UBOOL FEditorModeTools::CheckBookmark( UINT InIndex )
{
	if ( GWorld )
	{
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		if ( WorldInfo && InIndex < UCONST_MAX_BOOKMARK_NUMBER && WorldInfo->BookMarks[ InIndex ] )
		{
			return ( WorldInfo->BookMarks[ InIndex ] ? TRUE : FALSE );
		}
	}

	return FALSE;
}

/**
 * Retrieves a bookmark from the list.
 */

void FEditorModeTools::JumpToBookmark( UINT InIndex, UBOOL bShouldRestoreLevelVisibility )
{
	if ( GWorld )
	{
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();

		// Can only jump to a pre-existing bookmark
		if ( WorldInfo && InIndex < UCONST_MAX_BOOKMARK_NUMBER && WorldInfo->BookMarks[ InIndex ] )
		{
			const UBookMark* CurBookMark = WorldInfo->BookMarks[ InIndex ];
			check(CurBookMark);

			// Set all level editing cameras to this bookmark
			for( INT v = 0 ; v < GEditor->ViewportClients.Num() ; v++ )
			{
				if ( !GEditor->ViewportClients(v)->bViewportLocked )
				{
					GEditor->ViewportClients(v)->ViewLocation = CurBookMark->Location;
					if( !GEditor->ViewportClients(v)->IsOrtho() )
					{
						GEditor->ViewportClients(v)->ViewRotation = CurBookMark->Rotation;
					}
					GEditor->ViewportClients(v)->Invalidate();
				}
			}

			// Restore level visibility
			if( bShouldRestoreLevelVisibility )
			{
				for ( INT LevelIndex = 0 ; LevelIndex < WorldInfo->StreamingLevels.Num() ; ++LevelIndex )
				{
					ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
					if ( StreamingLevel )
					{
						StreamingLevel->bShouldBeVisibleInEditor = !CurBookMark->HiddenLevels.ContainsItem( StreamingLevel->GetFullName() );
						LevelBrowser::SetLevelVisibility( StreamingLevel->LoadedLevel, StreamingLevel->bShouldBeVisibleInEditor );
					}
				}
			}
		}
	}
}

/**
 * Serializes the components for all modes.
 */

void FEditorModeTools::Serialize( FArchive &Ar )
{
	for( INT x = 0 ; x < Modes.Num() ; ++x )
	{
		Modes(x)->Serialize( Ar );
		Ar << Modes(x)->Component;
	}
}

/**
 * Returns a pointer to an active mode specified by the passed in ID
 * If the editor mode is not active, NULL is returned
 */
FEdMode* FEditorModeTools::GetActiveMode( EEditorMode InID )
{
	FEdMode* ActiveMode = NULL;
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		if( ActiveModes(ModeIndex)->GetID() == InID )
		{
			ActiveMode = ActiveModes(ModeIndex);
			break;
		}
	}
	return ActiveMode;
}

/**
 * Returns a pointer to an active mode specified by the passed in ID
 * If the editor mode is not active, NULL is returned
 */
const FEdMode* FEditorModeTools::GetActiveMode( EEditorMode InID ) const
{
	const FEdMode* ActiveMode = NULL;
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		if( ActiveModes(ModeIndex)->GetID() == InID )
		{
			ActiveMode = ActiveModes(ModeIndex);
			break;
		}
	}
	return ActiveMode;
}

/**
 * Returns the active tool of the passed in editor mode.
 * If the passed in editor mode is not active or the mode has no active tool, NULL is returned
 */
const FModeTool* FEditorModeTools::GetActiveTool( EEditorMode InID ) const
{
	const FEdMode* ActiveMode = GetActiveMode( InID );
	const FModeTool* Tool = NULL;
	if( ActiveMode )
	{
		Tool = ActiveMode->GetCurrentTool();
	}
	return Tool;
}

/** 
 * Returns TRUE if the passed in editor mode is active 
 */
UBOOL FEditorModeTools::IsModeActive( EEditorMode InID ) const
{
	return GetActiveMode( InID ) != NULL;
}

/** 
 * Returns an array of all active modes
 */
void FEditorModeTools::GetActiveModes( TArray<FEdMode*>& OutActiveModes )
{
	OutActiveModes.Empty();
	// Copy into an array.  Do not let users modify the active list directly.
	OutActiveModes = ActiveModes;
}

