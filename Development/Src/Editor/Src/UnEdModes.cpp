
#include "EditorPrivate.h"
#include "UnTerrain.h"

/*------------------------------------------------------------------------------
    Base class.
------------------------------------------------------------------------------*/

FEdMode::FEdMode()
{
	ID = EM_None;
	Desc = TEXT("N/A");
	BitmapOn = NULL;
	BitmapOff = NULL;
	CurrentTool = NULL;
	Settings = NULL;
	ModeBar = NULL;

	Component = ConstructObject<UEdModeComponent>(UEdModeComponent::StaticClass());
	Component->SetParentToWorld(FMatrix::Identity);
}

FEdMode::~FEdMode()
{
	delete Settings;
}

UBOOL FEdMode::MouseMove(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,INT x, INT y)
{
	if( GetCurrentTool() )
		return GetCurrentTool()->MouseMove( ViewportClient, Viewport, x, y );

	return 0;
}

UBOOL FEdMode::InputKey(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,FName Key,EInputEvent Event)
{
	if( GetCurrentTool() )
		return GetCurrentTool()->InputKey( ViewportClient, Viewport, Key, Event );

	return 0;
}

UBOOL FEdMode::InputDelta(FEditorLevelViewportClient* InViewportClient,FChildViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale)
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

UBOOL FEdMode::UsesWidget()
{
	if( GetCurrentTool() )
		return GetCurrentTool()->bUseWidget;

	return 1;
}

/**
 * Allows each mode/tool to determine a good location for the widget to be drawn at.
 */

FVector FEdMode::GetWidgetLocation()
{
	return GEditorModeTools.PivotLocation;
}

/**
 * Lets the mode determine if it wants to draw the widget or not.
 */

UBOOL FEdMode::ShouldDrawWidget()
{
	return (GSelectionTools.GetTop<AActor>() != NULL);
}

/**
 * Allows each mode to customize the axis pieces of the widget they want drawn.
 *
 * @param	InwidgetMode	The current widget mode
 *
 * @return	A bitfield comprised of AXIS_ values
 */

INT FEdMode::GetWidgetAxisToDraw( EWidgetMode InWidgetMode )
{
	return AXIS_XYZ;
}

/**
 * Lets each mode/tool handle box selection in its own way.
 *
 * @param	InBox	The selection box to use, in worldspace coordinates.
 */

void FEdMode::BoxSelect( FBox& InBox, UBOOL InSelect )
{
	if( GetCurrentTool() )
	{
		GetCurrentTool()->BoxSelect( InBox, InSelect );
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
	if(Component->Initialized)
	{
		Component->Destroyed();
		Component->Scene = NULL;
	}
}

void FEdMode::UpdateComponent()
{
	check(Component);
	check(Component->IsValidComponent());
	Component->Scene = GEditor->GetLevel();
	Component->Created();
}

void FEdMode::Enter()
{
	UpdateComponent();
	GCallback->Send( CALLBACK_EditorModeEnter, this );
}

void FEdMode::Exit()
{
	ClearComponent();
	GCallback->Send( CALLBACK_EditorModeExit, this );
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

	CurrentToolChanged();
}

FModeTool* FEdMode::FindTool( EModeTools InID )
{
	for( INT x = 0 ; x < Tools.Num() ; ++x )
		if( Tools(x)->GetID() == InID )
			return Tools(x);

	return NULL;

}

FToolSettings* FEdMode::GetSettings()
{
	return Settings;
}

void FEdMode::RenderBackground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
}

void FEdMode::Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	if( GEditor->bShowBrushMarkerPolys )
	{
		// Draw translucent polygons on brushes and volumes

		for( INT i = 0 ; i < GEngine->GetLevel()->Actors.Num() ; i++ )
		{
			ABrush* Brush = Cast<ABrush>( GEngine->GetLevel()->Actors(i) );

			if( Brush && Brush->IsSelected() && (Brush == GEngine->GetLevel()->Brush() || Brush->IsVolumeBrush() ) )
			{
				static FMaterialInstance ColorInstance;
				ColorInstance.VectorValueMap.Set(NAME_Color,Brush->GetWireColor());

				FTriangleRenderInterface* TRI = PRI->GetTRI( Brush->LocalToWorld(), GEngine->EditorBrushMaterial->GetMaterialInterface(0), &ColorInstance );

				for( INT p = 0 ; p < Brush->Brush->Polys->Element.Num() ; ++p )
				{
					FPoly* Poly = &Brush->Brush->Polys->Element(p);

					if( Poly->NumVertices > 2 )
					{
						FVector v0, v1, v2;

						v0 = Poly->Vertex[0];
						v1 = Poly->Vertex[1];

						for( INT v = 2 ; v < Poly->NumVertices ; ++v )
						{
							v2 = Poly->Vertex[v];

							TRI->DrawTriangle( FRawTriangleVertex(v0), FRawTriangleVertex(v2), FRawTriangleVertex(v1) );

							v1 = v2;
						}
					}
				}

				TRI->Finish();
			}
		}
	}
}

void FEdMode::RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
}

void FEdMode::DrawHUD(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,const FSceneContext& Context,FRenderInterface* RI)
{
	// If there is an active drag tool, let it render

	FDragTool* dt = ViewportClient->MouseDeltaTracker.DragTool;
	if( dt )
	{
		dt->Render(Context,RI);
	}

	// If this viewport doesn't show mode widgets or the mode itself doesn't want them, leave.

	if( !(ViewportClient->ShowFlags&SHOW_ModeWidgets) || !ShowModeWidgets() )
	{
		return;
	}

	// Draw vertices for selected BSP brushes.

	UTexture2D* texture = GetVertexTexture();
	UBOOL bLargeVertices = ((FEditorLevelViewportClient*)Viewport->ViewportClient)->ShowFlags & SHOW_LargeVertices;
	INT X, Y;

	for( INT i = 0 ; i < GEngine->GetLevel()->Actors.Num() ; i++ )
	{
		// Brushes

		ABrush* Brush = Cast<ABrush>( GEngine->GetLevel()->Actors(i) );

		if( Brush && GSelectionTools.IsSelected( Brush ) && Brush->Brush )
		{
			for( INT p = 0 ; p < Brush->Brush->Polys->Element.Num() ; ++p )
			{
				FPoly* poly = &Brush->Brush->Polys->Element(p);
				for( INT v = 0 ; v < poly->NumVertices ; ++v )
				{
					FColor Color = Brush->GetWireColor();

					FVector vtx = Brush->LocalToWorld().TransformFVector( poly->Vertex[v] );

					UBOOL bSelected = IsVertexSelected( Brush, &poly->Vertex[v] );
					if( !bSelected )
					{
						Color = Color.Plane() * .5f;
					}
					Color.A = 255;

					if( Context.Project( vtx, X, Y ) )
					{
						FLOAT SzX = texture->SizeX;
						FLOAT SzY = texture->SizeY;

						if( !bLargeVertices )
						{
							SzX *= .5f;
							SzY *= .5f;
						}

						RI->SetHitProxy(new HBSPBrushVert(Brush,&poly->Vertex[v]));
						RI->DrawTile( X - (SzX/2), Y - (SzY/2), SzX, SzY, 0.f, 0.f, 1.f, 1.f, Color, texture );
						RI->SetHitProxy(NULL);
					}
				}
			}
		}

		if( bLargeVertices )
		{
			// Static mesh vertices

			AStaticMeshActor* Actor = Cast<AStaticMeshActor>(GEngine->GetLevel()->Actors(i));

			if( Actor && GSelectionTools.IsSelected( Actor ) && Actor->StaticMeshComponent && Actor->StaticMeshComponent->StaticMesh )
			{
				TArray<FVector> Vertices;

				if(!Actor->StaticMeshComponent->StaticMesh->RawTriangles.Num())
				{
					Actor->StaticMeshComponent->StaticMesh->RawTriangles.Load();
				}

				for( INT tri = 0 ; tri < Actor->StaticMeshComponent->StaticMesh->RawTriangles.Num() ; tri++ )
				{
					FStaticMeshTriangle* smt = &Actor->StaticMeshComponent->StaticMesh->RawTriangles(tri);

					for( INT v = 0 ; v < 2 ; ++v )
					{
						FVector vtx = Actor->LocalToWorld().TransformFVector( smt->Vertices[v] );
						Vertices.AddUniqueItem( vtx );
					}
				}

				if( Vertices.Num() > 0 )
				{
					for( INT x = 0 ; x < Vertices.Num() ; ++x )
					{
						if( Context.Project( Vertices(x), X, Y ) )
						{
							FLOAT SzX = texture->SizeX;
							FLOAT SzY = texture->SizeY;

							RI->SetHitProxy(new HStaticMeshVert(Actor,Vertices(x)));
							RI->DrawTile( X - (SzX/2), Y - (SzY/2), SzX, SzY, 0.f, 0.f, 1.f, 1.f, FLinearColor::White, texture );
							RI->SetHitProxy(NULL);
						}
					}
				}
			}
		}
	}
}

UBOOL FEdMode::StartTracking()
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->StartModify();
	}

	return 0;
}

UBOOL FEdMode::EndTracking()
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->EndModify();
	}

	return 0;
}

FVector FEdMode::GetWidgetNormalFromCurrentAxis( void* InData )
{
	// Figure out the proper coordinate system.

	FMatrix matrix = FMatrix::Identity;
	if( GEditorModeTools.CoordSystem == COORD_Local )
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

	FVector Normal = matrix.TransformFVector( BaseNormal );

	return Normal;
}

UBOOL FEdMode::ExecDelete()
{
	return 0;
}

/*------------------------------------------------------------------------------
    Default.
------------------------------------------------------------------------------*/

FEdModeDefault::FEdModeDefault()
{
	ID = EM_Default;
	Desc = TEXT("Default");
}

FEdModeDefault::~FEdModeDefault()
{
}

/*------------------------------------------------------------------------------
    Geometry Editing.
------------------------------------------------------------------------------*/

FEdModeGeometry::FEdModeGeometry()
{
	ID = EM_Geometry;
	Desc = TEXT("Geometry Editing");
	ModifierWindow = NULL;

	Tools.AddItem( new FModeTool_GeometryModify() );
	SetCurrentTool( MT_GeometryModify );

	Settings = new FGeometryToolSettings;
}

FEdModeGeometry::~FEdModeGeometry()
{
}

UBOOL FEdModeGeometry::CanSelect( AActor* InActor )
{
	// Only allow brushes to be selected in geometry mode.

	if( InActor->IsBrush() )
	{
		return 1;
	}

	return 0;
}

void FEdModeGeometry::RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	EGeometrySelectionType seltype = ((FGeometryToolSettings*)GetSettings())->SelectionType;

	switch( seltype )
	{
		case GST_Object:
			RenderObject( Context, PRI );
			break;
		case GST_Poly:
			RenderPoly( Context, PRI );
			break;
		case GST_Edge:
			RenderEdge( Context, PRI );
			break;
		case GST_Vertex:
			RenderVertex( Context, PRI );
			break;
	}
}

UBOOL FEdModeGeometry::ShowModeWidgets()
{
	EGeometrySelectionType seltype = ((FGeometryToolSettings*)GetSettings())->SelectionType;

	switch( seltype )
	{
		case GST_Object:
		case GST_Poly:
		case GST_Edge:
			return 1;
	}

	return 0;
}

UBOOL FEdModeGeometry::ShouldDrawBrushWireframe( AActor* InActor )
{
	// If the actor isn't selected, we don't want to interfere with it's rendering.

	if( !GSelectionTools.IsSelected( InActor ) )
	{
		return 1;
	}

	EGeometrySelectionType seltype = ((FGeometryToolSettings*)GetSettings())->SelectionType;

	switch( seltype )
	{
		case GST_Object:
		case GST_Poly:
		case GST_Edge:
		case GST_Vertex:
			return 0;
	}

	check(0);	// Shouldn't be here
	return 0;
}

UBOOL FEdModeGeometry::GetCustomDrawingCoordinateSystem( FMatrix& InMatrix, void* InData )
{
	EGeometrySelectionType seltype = ((FGeometryToolSettings*)GetSettings())->SelectionType;

	if( seltype == GST_Object )
	{
		return 0;
	}

	if( InData )
	{
		InMatrix = FRotationMatrix( ((FGeomBase*)InData)->Normal.Rotation() );
	}
	else
	{
		// If we don't have a specific geometry object to get the normal from
		// use the one that was last selected.

		for( INT o = 0 ; o < GeomObjects.Num() ; ++o )
		{
			FGeomObject* go = &GeomObjects(o);
			go->CompileSelectionOrder();

			if( go->SelectionOrder.Num() )
			{
				InMatrix = FRotationMatrix( go->SelectionOrder( go->SelectionOrder.Num()-1 )->Normal.Rotation() );
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

	// Start up the tool window

	ModifierWindow = new WxGeomModifiers( (wxWindow*)GWarn->winEditorFrame, -1 );
	ModifierWindow->Show( ((FGeometryToolSettings*)GetSettings())->bShowModifierWindow == 1 );
	ModifierWindow->InitFromTool();
}

void FEdModeGeometry::Exit()
{
	FEdMode::Exit();

	GeomObjects.Empty();

	ModifierWindow->Destroy();
}

void FEdModeGeometry::ActorSelectionChangeNotify()
{
	// Check for newly selected brushes.

	for( TSelectedActorIterator It( GEditor->GetLevel() ) ; It ; ++It )
	{
		ABrush* Actor = Cast<ABrush>( *It );

		if( Actor )
		{
			// See if we have this brush already selected.  If not, create a geometry object for it.

			UBOOL bAlreadySelected = 0;

			for( INT o = 0 ; o < GeomObjects.Num() ; ++o )
			{
				FGeomObject* go = &GeomObjects(o);
				if( go->GetActualBrush() == Actor )
				{
					bAlreadySelected = 1;
					break;
				}
			}

			if( !bAlreadySelected )
			{
				new( GeomObjects )FGeomObject();
				FGeomObject* go = &GeomObjects( GeomObjects.Num()-1 );
				go->ParentObjectIndex = GeomObjects.Num()-1;
				go->ActualBrushIndex = It.GetIndex();
				go->GetFromSource();
			}
		}
	}
	
	// Check for brushes that were selected, but now aren't.

	for( INT o = 0 ; o < GeomObjects.Num() ; ++o )
	{
		ABrush* Actor = GeomObjects(o).GetActualBrush();

		if( !GSelectionTools.IsSelected( Actor ) )
		{
			GeomObjects.Remove( o );
			o = -1;
		}
	}
}

void FEdModeGeometry::MapChangeNotify()
{
	// If the map changes in some major way, just refresh all the geometry data.

	GetFromSource();
}

void FEdModeGeometry::CurrentToolChanged()
{
	UpdateModifierWindow();
}

void FEdModeGeometry::UpdateModifierWindow()
{
	ModifierWindow->InitFromTool();
}

void FEdModeGeometry::Serialize( FArchive &Ar )
{
	FModeTool_GeometryModify* mtgm = (FModeTool_GeometryModify*)FindTool( MT_GeometryModify );

	for( INT x = 0 ; x < mtgm->Modifiers.Num() ; ++x )
	{
		Ar << mtgm->Modifiers(x);
	}
}

// ------------------------------------------------------------------------------

void FEdModeGeometry::SelectNone()
{
	for( INT x = 0 ; x < GeomObjects.Num() ; ++x )
	{
		FGeomObject* go = &GeomObjects(x);
		go->Select( 0 );

		for( int i = 0 ; i < go->EdgePool.Num() ; ++i )
		{
			go->EdgePool(i).Select( 0 );
		}
		for( int i = 0 ; i < go->PolyPool.Num() ; ++i )
		{
			go->PolyPool(i).Select( 0 );
		}
		for( int i = 0 ; i < go->VertexPool.Num() ; ++i )
		{
			go->VertexPool(i).Select( 0 );
		}

		go->SelectionOrder.Empty();
	}
}

// ------------------------------------------------------------------------------

void FEdModeGeometry::RenderSinglePoly( FGeomPoly* InPoly, const FSceneContext &InContext, FPrimitiveRenderInterface* InPRI, FTriangleRenderInterface* InTRI )
{
	// Look at the edge list and create a list of vertices to render from.

	TArray<FVector> Verts;

	FVector LastPos;

	for( INT e = 0 ; e < InPoly->EdgeIndices.Num() ; ++e )
	{
		FGeomEdge* ge = &InPoly->GetParentObject()->EdgePool( InPoly->EdgeIndices(e) );

		FVector v0 = InPoly->GetParentObject()->VertexPool( ge->VertexIndices[0] ),
			v1 = InPoly->GetParentObject()->VertexPool( ge->VertexIndices[1] );

		if( e == 0 )
		{
			Verts.AddItem( InPoly->GetParentObject()->VertexPool( ge->VertexIndices[0] ) );
			LastPos = InPoly->GetParentObject()->VertexPool( ge->VertexIndices[0] );
		}
		else if( InPoly->GetParentObject()->VertexPool( ge->VertexIndices[0] ) == LastPos )
		{
			Verts.AddItem( InPoly->GetParentObject()->VertexPool( ge->VertexIndices[1] ) );
			LastPos = InPoly->GetParentObject()->VertexPool( ge->VertexIndices[1] );
		}
		else
		{
			Verts.AddItem( InPoly->GetParentObject()->VertexPool( ge->VertexIndices[0] ) );
			LastPos = InPoly->GetParentObject()->VertexPool( ge->VertexIndices[0] );
		}
	}

	FRawTriangleVertex v1( Verts(0) ), v2( Verts(1) ), v3( FVector(0,0,0) );

	for( INT v = 2 ; v < Verts.Num() ; ++v )
	{
		v3 = Verts(v);
		InTRI->DrawTriangle( v2, v1, v3 );
		v2 = v3;
	}

	if( ((FGeometryToolSettings*)GetSettings())->bShowNormals )
	{
		FVector Mid = InPoly->GetParentObject()->GetActualBrush()->LocalToWorld().TransformFVector( InPoly->Mid );
		FLOAT Scale = InContext.View->Project( Mid ).W * ( 4 / (FLOAT)InContext.SizeX / InContext.View->ProjectionMatrix.M[0][0] );
		InPRI->DrawLine( Mid, Mid+(InPoly->Normal * (32*Scale)), FColor(255,0,0) );
	}
}

// ------------------------------------------------------------------------------

void FEdModeGeometry::RenderSingleEdge( FGeomEdge* InEdge, FColor InColor, const FSceneContext &InContext, FPrimitiveRenderInterface* InPRI, UBOOL InShowEdgeMarkers )
{
	FVector v0 = InEdge->GetParentObject()->VertexPool( InEdge->VertexIndices[0] ),
		v1 = InEdge->GetParentObject()->VertexPool( InEdge->VertexIndices[1] );

	v0 = InEdge->GetParentObject()->GetActualBrush()->LocalToWorld().TransformFVector( v0 );
	v1 = InEdge->GetParentObject()->GetActualBrush()->LocalToWorld().TransformFVector( v1 );

	if( InShowEdgeMarkers )
	{
		FVector Mid = InEdge->GetParentObject()->GetActualBrush()->LocalToWorld().TransformFVector( InEdge->Mid );
		FLOAT Scale = InContext.View->Project( Mid ).W * ( 4 / (FLOAT)InContext.SizeX / InContext.View->ProjectionMatrix.M[0][0] );
		InPRI->DrawSprite( Mid, 3.f * Scale, 3.f * Scale, GEngine->GetLevel()->GetLevelInfo()->BSPVertex, InColor );

		if( ((FGeometryToolSettings*)GetSettings())->bShowNormals )
		{
			InPRI->DrawLine( Mid, Mid+(InEdge->Normal * (32*Scale)), FColor(255,0,0) );
		}
	}

	InPRI->DrawLine( v0, v1, InColor );
}

// ------------------------------------------------------------------------------

void FEdModeGeometry::RenderSinglePolyWireframe( FGeomPoly* InPoly, FColor InColor, const FSceneContext &InContext, FPrimitiveRenderInterface* InPRI )
{
	for( INT e = 0 ; e < InPoly->EdgeIndices.Num() ; ++e )
	{
		FGeomEdge* ge = &InPoly->GetParentObject()->EdgePool( InPoly->EdgeIndices(e) );
		RenderSingleEdge( ge, InColor, InContext, InPRI, 0 );
	}
}

// ------------------------------------------------------------------------------

void FEdModeGeometry::RenderObject( const FSceneContext& Context, FPrimitiveRenderInterface* PRI )
{
	for( INT o = 0 ; o < GeomObjects.Num() ; ++o )
	{
		FGeomObject* go = &GeomObjects(o);

		ColorInstance.VectorValueMap.Set(NAME_Color,FLinearColor(FColor(255,128,64)));
		FTriangleRenderInterface* TRI = PRI->GetTRI( go->GetActualBrush()->LocalToWorld(), GEngine->GeomMaterial->GetMaterialInterface(0), &ColorInstance );

		for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
		{
			FGeomPoly* gp = &go->PolyPool(p);
			RenderSinglePolyWireframe( gp, FColor(255,255,255), Context, PRI );
		}

		for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
		{
			FGeomPoly* gp = &go->PolyPool(p);
			RenderSinglePoly( gp, Context, PRI, TRI );
		}

		TRI->Finish();
	}
}

// ------------------------------------------------------------------------------

void FEdModeGeometry::RenderPoly( const FSceneContext& Context, FPrimitiveRenderInterface* PRI )
{
	// Selected

	ColorInstance.VectorValueMap.Set(NAME_Color,FLinearColor(FColor(255,128,64)));

	for( INT o = 0 ; o < GeomObjects.Num() ; ++o )
	{
		FGeomObject* go = &GeomObjects(o);

		for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
		{
			FGeomPoly* gp = &go->PolyPool(p);
			if( gp->IsSelected() )
			{
				RenderSinglePolyWireframe( gp, FColor(255,255,255), Context, PRI );
			}
		}

		for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
		{
			FGeomPoly* gp = &go->PolyPool(p);
			if( gp->IsSelected() )
			{
				PRI->SetHitProxy( new HGeomPolyProxy(go,p) );
				FTriangleRenderInterface* TRI = PRI->GetTRI( go->GetActualBrush()->LocalToWorld(), GEngine->GeomMaterial->GetMaterialInterface(0), &ColorInstance );
				RenderSinglePoly( gp, Context, PRI, TRI );
				TRI->Finish();
				PRI->SetHitProxy( NULL );
			}
		}
	}

	// Unselected

	for( INT o = 0 ; o < GeomObjects.Num() ; ++o )
	{
		FGeomObject* go = &GeomObjects(o);

		ColorInstance.VectorValueMap.Set(NAME_Color,FLinearColor(go->GetActualBrush()->GetWireColor()) * .5f);

		for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
		{
			FGeomPoly* gp = &go->PolyPool(p);
			if( !gp->IsSelected() )
			{
				RenderSinglePolyWireframe( gp, go->GetActualBrush()->GetWireColor(), Context, PRI );
			}
		}

		for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
		{
			FGeomPoly* gp = &go->PolyPool(p);
			if( !gp->IsSelected() )
			{
				PRI->SetHitProxy( new HGeomPolyProxy(go,p) );
				FTriangleRenderInterface* TRI = PRI->GetTRI( go->GetActualBrush()->LocalToWorld(), GEngine->GeomMaterial->GetMaterialInterface(0), &ColorInstance );
				RenderSinglePoly( gp, Context, PRI, TRI );
				TRI->Finish();
				PRI->SetHitProxy( NULL );
			}
		}
	}
}

// ------------------------------------------------------------------------------

void FEdModeGeometry::RenderEdge( const FSceneContext& Context, FPrimitiveRenderInterface* PRI )
{
	for( INT o = 0 ; o < GeomObjects.Num() ; ++o )
	{
		FGeomObject* go = &GeomObjects(o);

		// Edges

		for( INT e = 0 ; e < go->EdgePool.Num() ; ++e )
		{
			FGeomEdge* ge = &go->EdgePool(e);
			FColor Color = ge->IsSelected() ? FColor(255,128,64) : go->GetActualBrush()->GetWireColor();

			PRI->SetHitProxy( new HGeomEdgeProxy(go,e) );
			RenderSingleEdge( ge, Color, Context, PRI, 1 );
			PRI->SetHitProxy( NULL );
		}
	}
}

// ------------------------------------------------------------------------------

void FEdModeGeometry::RenderVertex( const FSceneContext& Context, FPrimitiveRenderInterface* PRI )
{
	for( INT o = 0 ; o < GeomObjects.Num() ; ++o )
	{
		FGeomObject* go = &GeomObjects(o);

		// Shape and outline

		for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
		{
			FGeomPoly* gp = &go->PolyPool(p);
			RenderSinglePolyWireframe( gp, go->GetActualBrush()->GetWireColor(), Context, PRI );
		}

		// Vertices

		FColor Color;
		FLOAT Scale;
		FVector loc;

		for( INT v = 0 ; v < go->VertexPool.Num() ; ++v )
		{
			FGeomVertex* gv = &go->VertexPool(v);

			loc = go->GetActualBrush()->LocalToWorld().TransformFVector( *gv );
			Scale = Context.View->Project( loc ).W * ( 4 / (FLOAT)Context.SizeX / Context.View->ProjectionMatrix.M[0][0] );
			Color = gv->IsSelected() ? FColor(255*gv->SelStrength,128*gv->SelStrength,64*gv->SelStrength) : go->GetActualBrush()->GetWireColor();

			PRI->SetHitProxy( new HGeomVertexProxy(go,v) );
			PRI->DrawSprite( loc, 4.f * Scale, 4.f * Scale, GEngine->GetLevel()->GetLevelInfo()->BSPVertex, Color );
			PRI->SetHitProxy( NULL );

			if( ((FGeometryToolSettings*)GetSettings())->bShowNormals )
			{
				FVector Mid = go->GetActualBrush()->LocalToWorld().TransformFVector( gv->Mid );
				PRI->DrawLine( Mid, Mid+(gv->Normal * (32*Scale)), FColor(255,0,0) );
			}
		}
	}
}

// Applies delta information to a single vertex.

void FEdModeGeometry::ApplyToVertex( FGeomVertex* InVtx, FVector& InDrag, FRotator& InRot, FVector& InScale )
{
	// If we are only allowed to move the widget, leave.

	if( ((FGeometryToolSettings*)GetSettings())->bAffectWidgetOnly )
	{
		return;
	}

	UBOOL bUseSoftSelection = ((FGeometryToolSettings*)GetSettings())->bUseSoftSelection;

	// Drag

	if( !InDrag.IsZero() )
	{
		*InVtx += InDrag * (bUseSoftSelection ? InVtx->SelStrength : 1.f);
	}

	// Rotate
	
	if( !InRot.IsZero() )
	{
		FRotationMatrix matrix( InRot * (bUseSoftSelection ? InVtx->SelStrength : 1.f) );

		FVector Wk( InVtx->X, InVtx->Y, InVtx->Z );
		Wk = InVtx->GetParentObject()->GetActualBrush()->LocalToWorld().TransformFVector( Wk );
		Wk -= GEditorModeTools.PivotLocation;
		Wk = matrix.TransformFVector( Wk );
		Wk += GEditorModeTools.PivotLocation;
		*InVtx = InVtx->GetParentObject()->GetActualBrush()->WorldToLocal().TransformFVector( Wk );
	}

	// Scale

	if( !InScale.IsZero() )
	{
		FScaleMatrix matrix(
			FVector(
				(InScale.X * (bUseSoftSelection ? InVtx->SelStrength : 1.f)) / (GEditor->Constraints.GetGridSize() * 100.f),
				(InScale.Y * (bUseSoftSelection ? InVtx->SelStrength : 1.f)) / (GEditor->Constraints.GetGridSize() * 100.f), 
				(InScale.Z * (bUseSoftSelection ? InVtx->SelStrength : 1.f)) / (GEditor->Constraints.GetGridSize() * 100.f)
			)
		);

		FVector Wk( InVtx->X, InVtx->Y, InVtx->Z );
		Wk = InVtx->GetParentObject()->GetActualBrush()->LocalToWorld().TransformFVector( Wk );
		Wk -= GEditorModeTools.PivotLocation;
		Wk += matrix.TransformFVector( Wk );
		Wk += GEditorModeTools.PivotLocation;
		*InVtx = InVtx->GetParentObject()->GetActualBrush()->WorldToLocal().TransformFVector( Wk );
	}
}

/**
 * Compiles geometry mode information from the selected brushes.
 */

void FEdModeGeometry::GetFromSource()
{
	GeomObjects.Empty();

	for( TSelectedActorIterator It( GEditor->GetLevel() ) ; It ; ++It )
	{
		ABrush* Brush = Cast<ABrush>( *It );
		if( Brush )
		{
			new( GeomObjects )FGeomObject();
			FGeomObject* go = &GeomObjects( GeomObjects.Num()-1 );
			go->ParentObjectIndex = GeomObjects.Num()-1;
			go->ActualBrushIndex = It.GetIndex();
			go->GetFromSource();
		}
	}
}

/**
 * Changes the source brushes to match the current geometry data.
 */

void FEdModeGeometry::SendToSource()
{
	for( INT o = 0 ; o < GeomObjects.Num() ; ++o )
	{
		FGeomObject* go = &GeomObjects(o);

		go->SendToSource();
	}
}

UBOOL FEdModeGeometry::FinalizeSourceData()
{
	UBOOL Result = 0;

	for( INT o = 0 ; o < GeomObjects.Num() ; ++o )
	{
		FGeomObject* go = &GeomObjects(o);

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
		FGeomObject* go = &GeomObjects(o);
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
				case GST_Poly:
				{
					go->PolyPool( gs->Index ).ForceSelectionIndex( gs->SelectionIndex );
					GEditorModeTools.PivotLocation = GEditorModeTools.SnappedLocation = go->PolyPool( gs->Index ).GetWidgetLocation();
				}
				break;
				case GST_Edge:
				{
					go->EdgePool( gs->Index ).ForceSelectionIndex( gs->SelectionIndex );
					GEditorModeTools.PivotLocation = GEditorModeTools.SnappedLocation = go->EdgePool( gs->Index ).GetWidgetLocation();
				}
				break;
				case GST_Vertex:
				{
					go->VertexPool( gs->Index ).ForceSelectionIndex( gs->SelectionIndex );
					GEditorModeTools.PivotLocation = GEditorModeTools.SnappedLocation = go->VertexPool( gs->Index ).GetWidgetLocation();
				}
				break;
			}
		}

		go->ForceLastSelectionIndex(HighestSelectionIndex );
	}
}

UBOOL FEdModeGeometry::ExecDelete()
{
	// Find the delete modifier and execute it.

	FModeTool_GeometryModify* mtgm = (FModeTool_GeometryModify*)FindTool( MT_GeometryModify );

	for( INT x = 0 ; x < mtgm->Modifiers.Num() ; ++x )
	{
		UGeomModifier* gm = mtgm->Modifiers(x);

		if( gm->IsA( UGeomModifier_Delete::StaticClass()) )
		{
			gm->Apply();
			break;
		}
	}

	return 1;
}

/**
 * Applies soft selection to the currently selected vertices.
 */

void FEdModeGeometry::SoftSelect()
{
	FGeometryToolSettings* Settings = (FGeometryToolSettings*)GetSettings();

	if( !Settings->bUseSoftSelection )
	{
		return;
	}

	for( INT o = 0 ; o < GeomObjects.Num() ; ++o )
	{
		FGeomObject* go = &GeomObjects(o);
		go->SoftSelect();
	}
}

/**
 * Called when soft selection is turned off.  This clears out any extra data
 * that the geometry data might contain related to soft selection.
 */

void FEdModeGeometry::SoftSelectClear()
{
	FGeometryToolSettings* Settings = (FGeometryToolSettings*)GetSettings();

	for( INT o = 0 ; o < GeomObjects.Num() ; ++o )
	{
		FGeomObject* go = &GeomObjects(o);

		for( INT v = 0 ; v < go->VertexPool.Num() ; ++v )
		{
			FGeomVertex* gv = &go->VertexPool(v);
			
			if( !gv->IsFullySelected() )
			{
				gv->Select( 0 );
			}
		}
	}
}

void FEdModeGeometry::UpdateInternalData()
{
	GetFromSource();
}

/*------------------------------------------------------------------------------
	Terrain Tools
------------------------------------------------------------------------------*/

FEdModeTerrainEditing::FEdModeTerrainEditing()
{
	ID = EM_TerrainEdit;
	Desc = TEXT("Terrain Editing");
	bPerToolSettings = 0;
	CurrentTerrain = NULL;

	Tools.AddItem( new FModeTool_TerrainPaint() );
	Tools.AddItem( new FModeTool_TerrainSmooth() );
	Tools.AddItem( new FModeTool_TerrainAverage() );
	Tools.AddItem( new FModeTool_TerrainFlatten() );
	Tools.AddItem( new FModeTool_TerrainNoise() );
	SetCurrentTool( MT_TerrainPaint );

	Settings = new FTerrainToolSettings;
}

FEdModeTerrainEditing::~FEdModeTerrainEditing()
{
}

FToolSettings* FEdModeTerrainEditing::GetSettings()
{
	if( bPerToolSettings )
		return ((FModeTool_Terrain*)GetCurrentTool())->Settings;
	else
		return Settings;
}

#define CIRCLESEGMENTS 16
void FEdModeTerrainEditing::DrawToolCircle( FPrimitiveRenderInterface* PRI, ATerrain* Terrain, FVector Location, FLOAT Radius )
{
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

		if( !Terrain->XLevel->SingleLineCheck(Result,NULL,TraceEnd,TraceStart,TRACE_Terrain) )
		{
			if(LastVertexValid)
				PRI->DrawLine(LastVertex,Result.Location,FColor(255,0,0));

			LastVertex = Result.Location;
			LastVertexValid = 1;
		}
		else
			LastVertexValid = 0;
	}
}

void FEdModeTerrainEditing::RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	FEdMode::RenderForeground(Context,PRI);

	if(!PRI->IsHitTesting())
	{
		FTerrainToolSettings*	Settings = (FTerrainToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
		FChildViewport*			Viewport = PRI->GetViewport();
		FVector					HitLocation;
		ATerrain*				Terrain = ((FModeTool_Terrain*)GEditorModeTools.GetCurrentTool())->TerrainTrace(Viewport,Context.View,HitLocation);

		if(Terrain)
		{
			DrawToolCircle(PRI,Terrain,HitLocation,Settings->RadiusMin);
			DrawToolCircle(PRI,Terrain,HitLocation,Settings->RadiusMax);
		}
	}
}

/*------------------------------------------------------------------------------
	Texture
------------------------------------------------------------------------------*/

FEdModeTexture::FEdModeTexture()
{
	ID = EM_Texture;
	Desc = TEXT("Texture Alignment");

	Tools.AddItem( new FModeTool_Texture() );
	SetCurrentTool( MT_Texture );

	Settings = new FTextureToolSettings;
}

FEdModeTexture::~FEdModeTexture()
{
}

void FEdModeTexture::Enter()
{
	FEdMode::Enter();

	SaveCoordSystem = GEditorModeTools.CoordSystem;
	GEditorModeTools.CoordSystem = COORD_Local;
}

void FEdModeTexture::Exit()
{
	FEdMode::Exit();

	GEditorModeTools.CoordSystem = SaveCoordSystem;
}

FVector FEdModeTexture::GetWidgetLocation()
{
	for( INT s = 0 ; s < GEditor->GetLevel()->Model->Surfs.Num() ; ++s )
	{
		FBspSurf*	Surf = &GEditor->GetLevel()->Model->Surfs(s);

		if( Surf->PolyFlags&PF_Selected )
		{
			FPoly* poly = &((ABrush*)Surf->Actor)->Brush->Polys->Element( Surf->iBrushPoly );
			return Surf->Actor->LocalToWorld().TransformFVector( poly->GetMidPoint() );
		}
	}

	return FEdMode::GetWidgetLocation();
}

UBOOL FEdModeTexture::ShouldDrawWidget()
{
	return 1;
}

UBOOL FEdModeTexture::GetCustomDrawingCoordinateSystem( FMatrix& InMatrix, void* InData )
{
	FPoly* poly = NULL;
	FMatrix LocalToWorld;

	for( INT s = 0 ; s < GEditor->GetLevel()->Model->Surfs.Num() ; ++s )
	{
		FBspSurf*	Surf = &GEditor->GetLevel()->Model->Surfs(s);

		if( Surf->PolyFlags&PF_Selected )
		{
			poly = &((ABrush*)Surf->Actor)->Brush->Polys->Element( Surf->iBrushPoly );
			LocalToWorld = Surf->Actor->LocalToWorld();
			break;
		}
	}

	if( !poly )
	{
		return 0;
	}

	InMatrix = FMatrix::Identity;

	InMatrix.SetAxis( 2, poly->Normal );
	InMatrix.SetAxis( 0, poly->TextureU );
	InMatrix.SetAxis( 1, poly->TextureV );

	InMatrix.RemoveScaling();

	return 1;
}

UBOOL FEdModeTexture::GetCustomInputCoordinateSystem( FMatrix& InMatrix, void* InData )
{
	return 0;
}

INT FEdModeTexture::GetWidgetAxisToDraw( EWidgetMode InWidgetMode )
{
	switch( InWidgetMode )
	{
		case WM_Translate:
		case WM_ScaleNonUniform:
			return AXIS_X | AXIS_Y;
			break;

		case WM_Rotate:
			return AXIS_Z;
			break;
	}

	return AXIS_XYZ;
}

UBOOL FEdModeTexture::StartTracking()
{
	GEditor->Trans->Begin( TEXT("Texture Manipulation") );
	GEditor->Level->Model->ModifySelectedSurfs( 1 );

	return 1;
}

UBOOL FEdModeTexture::EndTracking()
{
	GEditor->Trans->End();

	return 1;
}

/*------------------------------------------------------------------------------
	FEditorModeTools.

	The master class that handles tracking of the current mode.
------------------------------------------------------------------------------*/

FEditorModeTools::FEditorModeTools()
{
	WidgetMode = WM_Translate;
	OverrideWidgetMode = WM_None;
	CoordSystem = COORD_World;
	PivotShown = 0;
	Snapping = 0;
	bShowWidget = 1;
}

FEditorModeTools::~FEditorModeTools()
{
}

void FEditorModeTools::Init()
{
	// Editor modes

	Modes.Empty();
	Modes.AddItem( new FEdModeDefault() );
	Modes.AddItem( new FEdModeGeometry() );
	Modes.AddItem( new FEdModeTerrainEditing() );
	Modes.AddItem( new FEdModeTexture() );

	SetCurrentMode( EM_Default );
}

void FEditorModeTools::SetCurrentMode( EEditorMode InID )
{
	// Don't set the mode if it isn't really changing.

	if( GetCurrentMode() && GetCurrentModeID() == InID )
	{
		return;
	}

	if( GetCurrentMode() )
		GetCurrentMode()->Exit();

	CurrentMode = FindMode( InID );

	if( !CurrentMode )
	{
		debugf( TEXT("FEditorModeTools::SetCurrentMode : Couldn't find mode %d.  Using default."), (INT)InID );
		CurrentMode = Modes(0);
	}

	GetCurrentMode()->Enter();

	GCallback->Send( CALLBACK_UpdateUI );
}

FEdMode* FEditorModeTools::FindMode( EEditorMode InID )
{
	for( INT x = 0 ; x < Modes.Num() ; x++ )
		if( Modes(x)->ID == InID )
			return Modes(x);

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

			if( !GetCurrentMode()->GetCustomDrawingCoordinateSystem( matrix, NULL ) )
			{
				INT Num = GSelectionTools.CountSelections<AActor>();

				if( Num == 1 )
				{
					matrix = FRotationMatrix( GSelectionTools.GetTop<AActor>()->Rotation );
				}
				else
				{
					// If the user selects more than 1 actor, we can't work in the local coordinate system anymore.  Switch to world automatically.

					CoordSystem = COORD_World;
					GCallback->Send( CALLBACK_UpdateUI );
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

			if( !GetCurrentMode()->GetCustomInputCoordinateSystem( matrix, NULL ) )
			{
				INT Num = GSelectionTools.CountSelections<AActor>();

				if( Num == 1 )
				{
					matrix = FRotationMatrix( GSelectionTools.GetTop<AActor>()->Rotation );
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

/**
 * Gets a good location to draw the widget at.
 */

FVector FEditorModeTools::GetWidgetLocation()
{
	return GetCurrentMode()->GetWidgetLocation();
}

/**
 * Changes the current widget mode.
 */

void FEditorModeTools::SetWidgetMode( EWidgetMode InWidgetMode )
{
	WidgetMode = InWidgetMode;
}

/**
 * Allows you to temporarily override the widget mode.  Call this function again
 * with WM_None to turn off the override.
 */

void FEditorModeTools::SetWidgetModeOverride( EWidgetMode InWidgetMode )
{
	OverrideWidgetMode = InWidgetMode;
}

/**
 * Retrieves the current widget mode, taking overrides into account.
 */

EWidgetMode FEditorModeTools::GetWidgetMode()
{
	if( OverrideWidgetMode != WM_None )
	{
		return OverrideWidgetMode;
	}

	return WidgetMode;
}

/**
 * Sets a bookmark in the levelinfo file, allocating it if necessary.
 */

void FEditorModeTools::SetBookMark( INT InIndex, FEditorLevelViewportClient* InViewportClient )
{
	ALevelInfo* li = GEditor->GetLevel()->GetLevelInfo();

	if( li->BookMarks[ InIndex ] == NULL )
	{
		li->BookMarks[ InIndex ] = ConstructObject<UBookMark>( UBookMark::StaticClass(), GEditor->GetLevel() );
	}

	// Use the rotation from the first perspective viewport can find.

	FRotator Rotation(0,0,0);
	if( !InViewportClient->IsOrtho() )
	{
		Rotation = InViewportClient->ViewRotation;
	}

	li->BookMarks[ InIndex ]->Location = InViewportClient->ViewLocation;
	li->BookMarks[ InIndex ]->Rotation = Rotation;
}

/**
 * Retrieves a bookmark from the list.
 */

void FEditorModeTools::JumpToBookMark( INT InIndex )
{
	ALevelInfo* li = GEditor->GetLevel()->GetLevelInfo();

	if( li->BookMarks[ InIndex ] == NULL )
	{
		return;
	}

	// Set all level editing cameras to this bookmark

	for( INT v = 0 ; v < GEditor->ViewportClients.Num() ; v++ )
	{
		if( GEditor->ViewportClients(v)->GetLevel() == GEditor->GetLevel() )
		{
			GEditor->ViewportClients(v)->ViewLocation = li->BookMarks[ InIndex ]->Location;
			if( !GEditor->ViewportClients(v)->IsOrtho() )
			{
				GEditor->ViewportClients(v)->ViewRotation = li->BookMarks[ InIndex ]->Rotation;
			}

			GEditor->ViewportClients(v)->Viewport->Invalidate();
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

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
