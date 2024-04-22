/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "UnrealEd.h"

static const INT AXIS_ARROW_RADIUS		= 5;
static const FLOAT AXIS_CIRCLE_RADIUS	= 48.0f;
static const FLOAT TRANSLATE_ROTATE_AXIS_CIRCLE_RADIUS	= 20.0f;
static const FLOAT INNER_AXIS_CIRCLE_RADIUS = 48.0f;
static const FLOAT OUTER_AXIS_CIRCLE_RADIUS = 56.0f;
static const FLOAT ROTATION_TEXT_RADIUS = 75.0f;
static const INT AXIS_CIRCLE_SIDES		= 24;


FWidget::FWidget()
{
	// Compute and store sample vertices for drawing the axis arrow heads

	AxisColorX = FColor(255,0,0);
	AxisColorY = FColor(0,255,0);
	AxisColorZ = FColor(0,0,255);
	PlaneColorXY = FColor(255,255,0);
	CurrentColor = FColor(255,255,0);

	AxisMaterialX = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(),NULL,TEXT("EditorMaterials.WidgetMaterial_X"),NULL,LOAD_None,NULL );
	AxisMaterialY = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(),NULL,TEXT("EditorMaterials.WidgetMaterial_Y"),NULL,LOAD_None,NULL );
	AxisMaterialZ = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(),NULL,TEXT("EditorMaterials.WidgetMaterial_Z"),NULL,LOAD_None,NULL );
	PlaneMaterialXY = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(),NULL,TEXT("EditorMaterials.WidgetVertexColorMaterial"),NULL,LOAD_None,NULL );
	GridMaterial = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(),NULL,TEXT("EditorMaterials.WidgetGridVertexColorMaterial_Mat"),NULL,LOAD_None,NULL );
	if (!GridMaterial)
	{
		GridMaterial = PlaneMaterialXY;
	}
	CurrentMaterial = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(),NULL,TEXT("EditorMaterials.WidgetMaterial_Current"),NULL,LOAD_None,NULL );

	CurrentAxis = AXIS_None;

	CustomCoordSystem = FMatrix::Identity;

	bAbsoluteTranslationInitialOffsetCached = FALSE;
	InitialTranslationOffset = FVector(0,0,0);
	InitialTranslationPosition = FVector(0, 0, 0);

	bDragging = FALSE;
	bSnapEnabled = FALSE;
}

extern INT DrawStringCenteredZ(FCanvas* Canvas,FLOAT StartX,FLOAT StartY,FLOAT Z,const TCHAR* Text,class UFont* Font,const FLinearColor& Color);
extern void StringSize(UFont* Font,INT& XL,INT& YL,const TCHAR* Text);

/**
 * Renders any widget specific HUD text
 * @param Canvas - Canvas to use for 2d rendering
 */
void FWidget::DrawHUD (FCanvas* Canvas)
{
	if (HUDString.Len())
	{
		INT StringPosX = appFloor(HUDInfoPos.X);
		INT StringPosY = appFloor(HUDInfoPos.Y);

		//measure string size
		INT StringSizeX, StringSizeY;
		StringSize(GEngine->SmallFont, StringSizeX, StringSizeY, *HUDString);
		
		//add some padding to the outside
		const INT Border = 5;
		INT FillMinX = StringPosX - Border - (StringSizeX>>1);
		INT FillMinY = StringPosY - Border;// - (StringSizeY>>1);
		StringSizeX += 2*Border;
		StringSizeY += 2*Border;

		//mostly alpha'ed black
		FLinearColor BackgroundColor(0.0f, 0.0f, 0.0f, .25f);
		DrawTile(Canvas, FillMinX, FillMinY, StringSizeX, StringSizeY, 0.0f, 0.0f, 0.0f, 0.0f, BackgroundColor);

		DrawStringCenteredZ(Canvas, StringPosX, StringPosY, 1.0f, *HUDString, GEngine->SmallFont, FColor(255,255,255) );
	}
}


void FWidget::Render( const FSceneView* View,FPrimitiveDrawInterface* PDI )
{
	TArray<FEdMode*> ActiveModes;
	GEditorModeTools().GetActiveModes( ActiveModes );

	//reset HUD text
	HUDString.Empty();

	UBOOL bDrawModeSupportsWidgetDrawing = FALSE;

	// Check to see of any active modes support widget drawing
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bDrawModeSupportsWidgetDrawing |= ActiveModes(ModeIndex)->ShouldDrawWidget();
	}

	const UBOOL bShowFlagsSupportsWidgetDrawing = (View->Family->ShowFlags & SHOW_ModeWidgets) ? TRUE : FALSE;
	const UBOOL bEditorModeToolsSupportsWidgetDrawing = GEditorModeTools().GetShowWidget();
	UBOOL bDrawWidget;

	// Because the movement routines use the widget axis to determine how to transform mouse movement into
	// editor object movement, we need to still run through the Render routine even though widget drawing may be
	// disabled.  So we keep a flag that is used to determine whether or not to actually render anything.  This way
	// we can still update the widget axis' based on the Context's transform matrices, even though drawing is disabled.
	if(bDrawModeSupportsWidgetDrawing && bShowFlagsSupportsWidgetDrawing && bEditorModeToolsSupportsWidgetDrawing)
	{
		bDrawWidget = TRUE;

		// See if there is a custom coordinate system we should be using, only change it if we are drawing widgets.
		CustomCoordSystem = GEditorModeTools().GetCustomDrawingCoordinateSystem();
	}
	else
	{
		bDrawWidget = FALSE;
	}

	// If the current modes don't want to use the widget, don't draw it.
	if( !GEditorModeTools().UsesTransformWidget() )
	{
		CurrentAxis = AXIS_None;
		return;
	}

	FVector Loc = GEditorModeTools().GetWidgetLocation();
	if(!View->ScreenToPixel(View->WorldToScreen(Loc),Origin))
	{
		// GEMINI_TODO: This case wasn't handled before.  Was that intentional?
		Origin.X = Origin.Y = 0;
	}

	switch( GEditorModeTools().GetWidgetMode() )
	{
		case WM_Translate:
			Render_Translate( View, PDI, Loc, bDrawWidget );
			break;

		case WM_Rotate:
			Render_Rotate( View, PDI, Loc, bDrawWidget );
			break;

		case WM_Scale:
			Render_Scale( View, PDI, Loc, bDrawWidget );
			break;

		case WM_ScaleNonUniform:
			Render_ScaleNonUniform( View, PDI, Loc, bDrawWidget );
			break;

		case WM_TranslateRotateZ:
			Render_TranslateRotateZ( View, PDI, Loc, bDrawWidget );
			break;

		default:
			break;
	}
};

static const FLOAT CUBE_SCALE = 4.0f;
#define CUBE_VERT(x,y,z) MeshBuilder.AddVertex( FVector((x)*CUBE_SCALE+52,(y)*CUBE_SCALE,(z)*CUBE_SCALE), FVector2D(0,0), FVector(0,0,0), FVector(0,0,0), FVector(0,0,0), FColor(255,255,255) )
#define CUBE_FACE(i,j,k) MeshBuilder.AddTriangle( CubeVerts[(i)],CubeVerts[(j)],CubeVerts[(k)] )

/**
 * Draws an arrow head line for a specific axis.
 */
void FWidget::Render_Axis( const FSceneView* View, FPrimitiveDrawInterface* PDI, EAxis InAxis, FMatrix& InMatrix, UMaterialInterface* InMaterial, FColor* InColor, FVector2D& OutAxisEnd, FLOAT InScale, UBOOL bDrawWidget, UBOOL bCubeHead )
{
	FMatrix ArrowToWorld = FScaleMatrix(FVector(InScale,InScale,InScale)) * InMatrix;

	if( bDrawWidget )
	{
		PDI->SetHitProxy( new HWidgetAxis(InAxis) );

		PDI->DrawLine( InMatrix.TransformFVector( FVector(8,0,0) * InScale ), InMatrix.TransformFVector( FVector(48,0,0) * InScale ), *InColor, SDPG_UnrealEdForeground );

		FDynamicMeshBuilder MeshBuilder;

		if ( bCubeHead )
		{
			// Compute cube vertices.
			INT CubeVerts[8];
			CubeVerts[0] = CUBE_VERT( 1, 1, 1 );
			CubeVerts[1] = CUBE_VERT( 1, -1, 1 );
			CubeVerts[2] = CUBE_VERT( 1, -1, -1 );
			CubeVerts[3] = CUBE_VERT( 1, 1, -1 );
			CubeVerts[4] = CUBE_VERT( -1, 1, 1 );
			CubeVerts[5] = CUBE_VERT( -1, -1, 1 );
			CubeVerts[6] = CUBE_VERT( -1, -1, -1 );
			CubeVerts[7] = CUBE_VERT( -1, 1, -1 );
			CUBE_FACE(0,1,2);
			CUBE_FACE(0,2,3);
			CUBE_FACE(4,5,6);
			CUBE_FACE(4,6,7);

			CUBE_FACE(4,0,7);
			CUBE_FACE(0,7,3);
			CUBE_FACE(1,6,5);
			CUBE_FACE(1,6,2);

			CUBE_FACE(5,4,0);
			CUBE_FACE(5,0,1);
			CUBE_FACE(3,7,6);
			CUBE_FACE(3,6,2);
		}
		else
		{
			// Compute the arrow cone vertices.
			INT ArrowVertices[AXIS_ARROW_SEGMENTS];
			for(INT VertexIndex = 0;VertexIndex < AXIS_ARROW_SEGMENTS;VertexIndex++)
			{
				FLOAT theta =  PI * 2.f * VertexIndex / AXIS_ARROW_SEGMENTS;
				ArrowVertices[VertexIndex] = MeshBuilder.AddVertex(
					FVector(40,AXIS_ARROW_RADIUS * appSin( theta ) * 0.5f,AXIS_ARROW_RADIUS * appCos( theta ) * 0.5f),
					FVector2D(0,0),
					FVector(0,0,0),
					FVector(0,0,0),
					FVector(0,0,0),
					*InColor
					);
			}
			INT RootArrowVertex = MeshBuilder.AddVertex(FVector(54,0,0),FVector2D(0,0),FVector(0,0,0),FVector(0,0,0),FVector(0,0,0), *InColor);

			// Build the arrow mesh.
			for( INT s = 0 ; s < AXIS_ARROW_SEGMENTS ; s++ )
			{
				MeshBuilder.AddTriangle(RootArrowVertex,ArrowVertices[s],ArrowVertices[(s+1)%AXIS_ARROW_SEGMENTS]);
			}
		}

		// Draw the arrow mesh.
		MeshBuilder.Draw(PDI,ArrowToWorld,InMaterial->GetRenderProxy(FALSE),SDPG_UnrealEdForeground,0.f);

		PDI->SetHitProxy( NULL );
	}

	if(!View->ScreenToPixel(View->WorldToScreen(ArrowToWorld.TransformFVector(FVector(64,0,0))),OutAxisEnd))
	{
		// GEMINI_TODO: This case previously left the contents of OutAxisEnd unmodified.  Is that desired?
		OutAxisEnd.X = OutAxisEnd.Y = 0;
	}
}

/**
 * Draws the translation widget.
 */
void FWidget::Render_Translate( const FSceneView* View, FPrimitiveDrawInterface* PDI, const FVector& InLocation, UBOOL bDrawWidget )
{
	FLOAT Scale = View->WorldToScreen( InLocation ).W * ( 4 / View->SizeX / View->ProjectionMatrix.M[0][0] );

	// Figure out axis colors

	FColor *XColor = &( CurrentAxis&AXIS_X ? CurrentColor : AxisColorX );
	FColor *YColor = &( CurrentAxis&AXIS_Y ? CurrentColor : AxisColorY );
	FColor *ZColor = &( CurrentAxis&AXIS_Z ? CurrentColor : AxisColorZ );

	// Figure out axis materials

	UMaterial* XMaterial = ( CurrentAxis&AXIS_X ? CurrentMaterial : AxisMaterialX );
	UMaterial* YMaterial = ( CurrentAxis&AXIS_Y ? CurrentMaterial : AxisMaterialY );
	UMaterial* ZMaterial = ( CurrentAxis&AXIS_Z ? CurrentMaterial : AxisMaterialZ );

	// Figure out axis matrices

	FMatrix XMatrix = CustomCoordSystem * FTranslationMatrix( InLocation );
	FMatrix YMatrix = FRotationMatrix( FRotator(0,16384,0) ) * CustomCoordSystem * FTranslationMatrix( InLocation );
	FMatrix ZMatrix = FRotationMatrix( FRotator(16384,0,0) ) * CustomCoordSystem * FTranslationMatrix( InLocation );

	UBOOL bIsPerspective = ( View->ProjectionMatrix.M[3][3] < 1.0f );

	// For local space widgets, we always want to draw all three axis, since they may not be aligned with
	// the orthographic projection anyway.
	UBOOL bIsLocalSpace = ( GEditorModeTools().CoordSystem == COORD_Local );

	INT DrawAxis = GEditorModeTools().GetWidgetAxisToDraw( GEditorModeTools().GetWidgetMode() );

	// Draw the axis lines with arrow heads
	if( DrawAxis&AXIS_X && (bIsPerspective || bIsLocalSpace || View->ViewMatrix.M[0][2] != 1.f) )
	{
		Render_Axis( View, PDI, AXIS_X, XMatrix, XMaterial, XColor, XAxisEnd, Scale, bDrawWidget );
	}

	if( DrawAxis&AXIS_Y && (bIsPerspective || bIsLocalSpace || View->ViewMatrix.M[0][0] != 1.f) )
	{
		Render_Axis( View, PDI, AXIS_Y, YMatrix, YMaterial, YColor, YAxisEnd, Scale, bDrawWidget );
	}

	if( DrawAxis&AXIS_Z && (bIsPerspective || bIsLocalSpace || View->ViewMatrix.M[0][1] != 1.f) )
	{
		Render_Axis( View, PDI, AXIS_Z, ZMatrix, ZMaterial, ZColor, ZAxisEnd, Scale, bDrawWidget );
	}

	// Draw the grabbers
	if( bDrawWidget )
	{
		if( bIsPerspective || bIsLocalSpace || View->ViewMatrix.M[0][1] == 1.f )
		{
			if( (DrawAxis&(AXIS_X|AXIS_Y)) == (AXIS_X|AXIS_Y) ) 
			{
				PDI->SetHitProxy( new HWidgetAxis(AXIS_XY) );
				{
					PDI->DrawLine( XMatrix.TransformFVector(FVector(16,0,0) * Scale), XMatrix.TransformFVector(FVector(16,16,0) * Scale), *XColor, SDPG_UnrealEdForeground );
					PDI->DrawLine( XMatrix.TransformFVector(FVector(16,16,0) * Scale), XMatrix.TransformFVector(FVector(0,16,0) * Scale), *YColor, SDPG_UnrealEdForeground );
				}
				PDI->SetHitProxy( NULL );
			}
		}

		if( bIsPerspective || bIsLocalSpace || View->ViewMatrix.M[0][0] == 1.f )
		{
			if( (DrawAxis&(AXIS_X|AXIS_Z)) == (AXIS_X|AXIS_Z) ) 
			{
				PDI->SetHitProxy( new HWidgetAxis(AXIS_XZ) );
				{
					PDI->DrawLine( XMatrix.TransformFVector(FVector(16,0,0) * Scale), XMatrix.TransformFVector(FVector(16,0,16) * Scale), *XColor, SDPG_UnrealEdForeground );
					PDI->DrawLine( XMatrix.TransformFVector(FVector(16,0,16) * Scale), XMatrix.TransformFVector(FVector(0,0,16) * Scale), *ZColor, SDPG_UnrealEdForeground );
				}
				PDI->SetHitProxy( NULL );
			}
		}

		if( bIsPerspective || bIsLocalSpace || View->ViewMatrix.M[0][0] == 0.f )
		{
			if( (DrawAxis&(AXIS_Y|AXIS_Z)) == (AXIS_Y|AXIS_Z) ) 
			{
				PDI->SetHitProxy( new HWidgetAxis(AXIS_YZ) );
				{
					PDI->DrawLine( XMatrix.TransformFVector(FVector(0,16,0) * Scale), XMatrix.TransformFVector(FVector(0,16,16) * Scale), *YColor, SDPG_UnrealEdForeground );
					PDI->DrawLine( XMatrix.TransformFVector(FVector(0,16,16) * Scale), XMatrix.TransformFVector(FVector(0,0,16) * Scale), *ZColor, SDPG_UnrealEdForeground );
				}
				PDI->SetHitProxy( NULL );
			}
		}
	}
}

/**
 * Draws the rotation widget.
 */
void FWidget::Render_Rotate( const FSceneView* View,FPrimitiveDrawInterface* PDI, const FVector& InLocation, UBOOL bDrawWidget )
{
	FLOAT Scale = View->WorldToScreen( InLocation ).W * ( 4 / View->SizeX / View->ProjectionMatrix.M[0][0] );

	//get the axes 
	FVector XAxis = CustomCoordSystem.TransformNormal(FVector(-1, 0, 0));
	FVector YAxis = CustomCoordSystem.TransformNormal(FVector(0, -1, 0));
	FVector ZAxis = CustomCoordSystem.TransformNormal(FVector(0, 0, 1));

	INT DrawAxis = GEditorModeTools().GetWidgetAxisToDraw( GEditorModeTools().GetWidgetMode() );

	FMatrix XMatrix = FRotationMatrix( FRotator(0,16384,0) ) * FTranslationMatrix( InLocation );

	FVector DirectionToWidget = View->IsPerspectiveProjection() ? (InLocation - View->ViewOrigin) : -View->GetViewDirection();
	DirectionToWidget.Normalize();

	// Draw a circle for each axis
	if (bDrawWidget || bDragging)
	{
		//no draw the arc segments
		if( DrawAxis&AXIS_X )
		{
			DrawRotationArc(View, PDI, AXIS_X, InLocation, YAxis, ZAxis, DirectionToWidget, AxisColorX, Scale);
		}

		if( DrawAxis&AXIS_Y )
		{
			DrawRotationArc(View, PDI, AXIS_Y, InLocation, ZAxis, XAxis, DirectionToWidget, AxisColorY, Scale);
		}

		if( DrawAxis&AXIS_Z )
		{
			DrawRotationArc(View, PDI, AXIS_Z, InLocation, XAxis, YAxis, DirectionToWidget, AxisColorZ, Scale);
		}
	}
	// Update Axis by projecting the axis vector to screenspace.
	View->ScreenToPixel(View->WorldToScreen( XMatrix.TransformFVector( FVector(96,0,0) ) ), XAxisEnd);
	// Update Axis by projecting the axis vector to screenspace.
	View->ScreenToPixel(View->WorldToScreen( XMatrix.TransformFVector( FVector(0,96,0) ) ), YAxisEnd);
	// Update Axis by projecting the axis vector to screenspace.
	View->ScreenToPixel(View->WorldToScreen( XMatrix.TransformFVector( FVector(0,0,96) ) ), ZAxisEnd);
}

/**
 * Draws the scaling widget.
 */
void FWidget::Render_Scale( const FSceneView* View,FPrimitiveDrawInterface* PDI, const FVector& InLocation, UBOOL bDrawWidget )
{
	FLOAT Scale = View->WorldToScreen( InLocation ).W * ( 4 / View->SizeX / View->ProjectionMatrix.M[0][0] );

	// Figure out axis colors

	FColor *Color = &( CurrentAxis != AXIS_None ? CurrentColor : AxisColorX );

	// Figure out axis materials

	UMaterial* Material = ( CurrentAxis&AXIS_X ? CurrentMaterial : AxisMaterialX );

	// Figure out axis matrices

	FMatrix XMatrix = CustomCoordSystem * FTranslationMatrix( InLocation );
	FMatrix YMatrix = FRotationMatrix( FRotator(0,16384,0) ) * CustomCoordSystem * FTranslationMatrix( InLocation );
	FMatrix ZMatrix = FRotationMatrix( FRotator(16384,0,0) ) * CustomCoordSystem * FTranslationMatrix( InLocation );

	// Draw the axis lines with cube heads

	Render_Axis( View, PDI, AXIS_X, XMatrix, Material, Color, XAxisEnd, Scale, bDrawWidget, TRUE );
	Render_Axis( View, PDI, AXIS_Y, YMatrix, Material, Color, YAxisEnd, Scale, bDrawWidget, TRUE );
	Render_Axis( View, PDI, AXIS_Z, ZMatrix, Material, Color, ZAxisEnd, Scale, bDrawWidget, TRUE );

	PDI->SetHitProxy( NULL );
}

/**
 * Draws the non-uniform scaling widget.
 */
void FWidget::Render_ScaleNonUniform( const FSceneView* View,FPrimitiveDrawInterface* PDI, const FVector& InLocation, UBOOL bDrawWidget )
{
	FLOAT Scale = View->WorldToScreen( InLocation ).W * ( 4 / View->SizeX / View->ProjectionMatrix.M[0][0] );

	// Figure out axis colors

	FColor *XColor = &( CurrentAxis&AXIS_X ? CurrentColor : AxisColorX );
	FColor *YColor = &( CurrentAxis&AXIS_Y ? CurrentColor : AxisColorY );
	FColor *ZColor = &( CurrentAxis&AXIS_Z ? CurrentColor : AxisColorZ );

	// Figure out axis materials

	UMaterial* XMaterial = ( CurrentAxis&AXIS_X ? CurrentMaterial : AxisMaterialX );
	UMaterial* YMaterial = ( CurrentAxis&AXIS_Y ? CurrentMaterial : AxisMaterialY );
	UMaterial* ZMaterial = ( CurrentAxis&AXIS_Z ? CurrentMaterial : AxisMaterialZ );

	// Figure out axis matrices

	FMatrix XMatrix = CustomCoordSystem * FTranslationMatrix( InLocation );
	FMatrix YMatrix = FRotationMatrix( FRotator(0,16384,0) ) * CustomCoordSystem * FTranslationMatrix( InLocation );
	FMatrix ZMatrix = FRotationMatrix( FRotator(16384,0,0) ) * CustomCoordSystem * FTranslationMatrix( InLocation );

	INT DrawAxis = GEditorModeTools().GetWidgetAxisToDraw( GEditorModeTools().GetWidgetMode() );

	// Draw the axis lines with cube heads

	if( DrawAxis&AXIS_X )	Render_Axis( View, PDI, AXIS_X, XMatrix, XMaterial, XColor, XAxisEnd, Scale, bDrawWidget, TRUE );
	if( DrawAxis&AXIS_Y )	Render_Axis( View, PDI, AXIS_Y, YMatrix, YMaterial, YColor, YAxisEnd, Scale, bDrawWidget, TRUE );
	if( DrawAxis&AXIS_Z )	Render_Axis( View, PDI, AXIS_Z, ZMatrix, ZMaterial, ZColor, ZAxisEnd, Scale, bDrawWidget, TRUE );

	// Draw grabber handles
	if ( bDrawWidget )
	{
		if( ((DrawAxis&(AXIS_X|AXIS_Y)) == (AXIS_X|AXIS_Y)) )
		{
			PDI->SetHitProxy( new HWidgetAxis(AXIS_XY) );
			{
				PDI->DrawLine( XMatrix.TransformFVector(FVector(16,0,0) * Scale), XMatrix.TransformFVector(FVector(8,8,0) * Scale), *XColor, SDPG_UnrealEdForeground );
				PDI->DrawLine( XMatrix.TransformFVector(FVector(8,8,0) * Scale), XMatrix.TransformFVector(FVector(0,16,0) * Scale), *YColor, SDPG_UnrealEdForeground );
			}
			PDI->SetHitProxy( NULL );
		}

		if( ((DrawAxis&(AXIS_X|AXIS_Z)) == (AXIS_X|AXIS_Z)) )
		{
			PDI->SetHitProxy( new HWidgetAxis(AXIS_XZ) );
			{
				PDI->DrawLine( XMatrix.TransformFVector(FVector(16,0,0) * Scale), XMatrix.TransformFVector(FVector(8,0,8) * Scale), *XColor, SDPG_UnrealEdForeground );
				PDI->DrawLine( XMatrix.TransformFVector(FVector(8,0,8) * Scale), XMatrix.TransformFVector(FVector(0,0,16) * Scale), *ZColor, SDPG_UnrealEdForeground );
			}
			PDI->SetHitProxy( NULL );
		}

		if( ((DrawAxis&(AXIS_Y|AXIS_Z)) == (AXIS_Y|AXIS_Z)) )
		{
			PDI->SetHitProxy( new HWidgetAxis(AXIS_YZ) );
			{
				PDI->DrawLine( XMatrix.TransformFVector(FVector(0,16,0) * Scale), XMatrix.TransformFVector(FVector(0,8,8) * Scale), *YColor, SDPG_UnrealEdForeground );
				PDI->DrawLine( XMatrix.TransformFVector(FVector(0,8,8) * Scale), XMatrix.TransformFVector(FVector(0,0,16) * Scale), *ZColor, SDPG_UnrealEdForeground );
			}
			PDI->SetHitProxy( NULL );
		}
	}
}

/**
* Draws the Translate & Rotate Z widget.
*/

void FWidget::Render_TranslateRotateZ( const FSceneView* View, FPrimitiveDrawInterface* PDI, const FVector& InLocation, UBOOL bDrawWidget )
{
	FLOAT Scale = View->WorldToScreen( InLocation ).W * ( 4 / View->SizeX / View->ProjectionMatrix.M[0][0] );

	// Figure out axis colors

	FColor XYPlaneColor  = ((CurrentAxis&AXIS_XY)==AXIS_XY) ? CurrentColor : PlaneColorXY;
	FColor ZRotateColor  = ((CurrentAxis&AXIS_ZROTATION)==AXIS_ZROTATION) ? CurrentColor : AxisColorZ ;
	FColor XColor        = ((CurrentAxis==AXIS_X) ? CurrentColor : AxisColorX );
	FColor YColor        = ((CurrentAxis==AXIS_Y) ? CurrentColor : AxisColorY );
	FColor ZColor        = ((CurrentAxis==AXIS_Z) ? CurrentColor : AxisColorZ );

	// Figure out axis matrices
	FMatrix XMatrix = CustomCoordSystem * FTranslationMatrix( InLocation );
	FMatrix YMatrix = FRotationMatrix( FRotator(0,16384,0) ) * CustomCoordSystem * FTranslationMatrix( InLocation );
	FMatrix ZMatrix = FRotationMatrix( FRotator(16384,0,0) ) * CustomCoordSystem * FTranslationMatrix( InLocation );

	UBOOL bIsPerspective = ( View->ProjectionMatrix.M[3][3] < 1.0f );

	// For local space widgets, we always want to draw all three axis, since they may not be aligned with
	// the orthographic projection anyway.
	UBOOL bIsLocalSpace = ( GEditorModeTools().CoordSystem == COORD_Local );

	INT DrawAxis = GEditorModeTools().GetWidgetAxisToDraw( GEditorModeTools().GetWidgetMode() );

	// Draw the grabbers
	if( bDrawWidget )
	{

		// Draw the axis lines with arrow heads
		if( DrawAxis&AXIS_X && (bIsPerspective || bIsLocalSpace || View->ViewMatrix.M[0][2] != 1.f) )
		{
			Render_Axis( View, PDI, AXIS_X, XMatrix, PlaneMaterialXY, &XColor, XAxisEnd, Scale, bDrawWidget );
		}

		if( DrawAxis&AXIS_Y && (bIsPerspective || bIsLocalSpace || View->ViewMatrix.M[0][0] != 1.f) )
		{
			Render_Axis( View, PDI, AXIS_Y, YMatrix, PlaneMaterialXY, &YColor, YAxisEnd, Scale, bDrawWidget );
		}

		if( DrawAxis&AXIS_Z && (bIsPerspective || bIsLocalSpace || View->ViewMatrix.M[0][1] != 1.f) )
		{
			Render_Axis( View, PDI, AXIS_Z, ZMatrix, PlaneMaterialXY, &ZColor, ZAxisEnd, Scale, bDrawWidget );
		}

		//X Axis
		if( DrawAxis&AXIS_ZROTATION && (bIsPerspective || bIsLocalSpace || View->ViewMatrix.M[0][2] != 1.f) )
		{
			PDI->SetHitProxy( new HWidgetAxis(AXIS_ZROTATION) );
			{
				FLOAT ScaledRadius = TRANSLATE_ROTATE_AXIS_CIRCLE_RADIUS*Scale;
				FVector XAxis = CustomCoordSystem.TransformFVector( FVector(1,0,0).RotateAngleAxis(GEditorModeTools().TranslateRotateXAxisAngle, FVector(0,0,1)) );
				FVector YAxis = CustomCoordSystem.TransformFVector( FVector(0,1,0).RotateAngleAxis(GEditorModeTools().TranslateRotateXAxisAngle, FVector(0,0,1)) );
				FVector BaseArrowPoint = InLocation + XAxis*ScaledRadius;
				ZRotateColor.A = (CurrentAxis==AXIS_ZROTATION) ? 0x7f : 0x3f;	//make the disc transparent
				DrawFlatArrow(PDI, BaseArrowPoint, XAxis, YAxis, ZRotateColor, ScaledRadius, ScaledRadius*.5f, PlaneMaterialXY->GetRenderProxy(FALSE), SDPG_UnrealEdForeground);
			}
			PDI->SetHitProxy( NULL );
		}

		//XY Plane
		if( bIsPerspective || bIsLocalSpace || View->ViewMatrix.M[0][1] == 1.f )
		{
			if( (DrawAxis&(AXIS_XY)) == (AXIS_XY) ) 
			{
				PDI->SetHitProxy( new HWidgetAxis(AXIS_XY) );
				{
					DrawCircle( PDI, InLocation, CustomCoordSystem.TransformFVector( FVector(1,0,0) ), CustomCoordSystem.TransformFVector( FVector(0,1,0) ), XYPlaneColor, TRANSLATE_ROTATE_AXIS_CIRCLE_RADIUS*Scale, AXIS_CIRCLE_SIDES, SDPG_UnrealEdForeground );
					XYPlaneColor.A = ((CurrentAxis&AXIS_XY)==AXIS_XY) ? 0x3f : 0x0f;	//make the disc transparent
					DrawDisc  ( PDI, InLocation, CustomCoordSystem.TransformFVector( FVector(1,0,0) ), CustomCoordSystem.TransformFVector( FVector(0,1,0) ), XYPlaneColor, TRANSLATE_ROTATE_AXIS_CIRCLE_RADIUS*Scale, AXIS_CIRCLE_SIDES, PlaneMaterialXY->GetRenderProxy(FALSE), SDPG_UnrealEdForeground );
					//PDI->DrawLine( XMatrix.TransformFVector(FVector(16,0,0) * Scale), XMatrix.TransformFVector(FVector(16,16,0) * Scale), *XColor, SDPG_UnrealEdForeground );
					//PDI->DrawLine( XMatrix.TransformFVector(FVector(16,16,0) * Scale), XMatrix.TransformFVector(FVector(0,16,0) * Scale), *YColor, SDPG_UnrealEdForeground );
				}
				PDI->SetHitProxy( NULL );
			}
		}
	}
}

/**
 * Converts mouse movement on the screen to widget axis movement/rotation.
 */
void FWidget::ConvertMouseMovementToAxisMovement( FEditorLevelViewportClient* InViewportClient, const FVector& InLocation, const FVector& InDiff, FVector& InDrag, FRotator& InRotation, FVector& InScale )
{
	FSceneViewFamilyContext ViewFamily(InViewportClient->Viewport,InViewportClient->GetScene(),InViewportClient->ShowFlags,GWorld->GetTimeSeconds(),GWorld->GetDeltaSeconds(),GWorld->GetRealTimeSeconds());
	InViewportClient->CalcSceneView(&ViewFamily);
	FPlane Wk;
	FVector2D AxisEnd;
	FVector Diff = InDiff;

	InDrag = FVector(0,0,0);
	InRotation = FRotator(0,0,0);
	InScale = FVector(0,0,0);

	// Get the end of the axis (in screen space) based on which axis is being pulled

	switch( CurrentAxis )
	{
		case AXIS_X:	AxisEnd = XAxisEnd;		break;
		case AXIS_Y:	AxisEnd = YAxisEnd;		break;
		case AXIS_Z:	AxisEnd = ZAxisEnd;		break;
		case AXIS_XY:	AxisEnd = Diff.X != 0 ? XAxisEnd : YAxisEnd;		break;
		case AXIS_XZ:	AxisEnd = Diff.X != 0 ? XAxisEnd : ZAxisEnd;		break;
		case AXIS_YZ:	AxisEnd = Diff.X != 0 ? YAxisEnd : ZAxisEnd;		break;
		case AXIS_XYZ:	AxisEnd = Diff.X != 0 ? YAxisEnd : ZAxisEnd;		break;
		default:
			break;
	}

	// Screen space Y axis is inverted

	Diff.Y *= -1;

	// Get the directions of the axis (on the screen) and the mouse drag direction (in screen space also).

	FVector2D AxisDir = AxisEnd - Origin;
	AxisDir.Normalize();

	FVector2D DragDir( Diff.X, Diff.Y );
	DragDir.Normalize();

	// Use the most dominant axis the mouse is being dragged along

	INT idx = 0;
	if( Abs(Diff.X) < Abs(Diff.Y) )
	{
		idx = 1;
	}

	FLOAT Val = Diff[idx];

	// If the axis dir is negative, it is pointing in the negative screen direction.  In this situation, the mouse
	// drag must be inverted so that you are still dragging in the right logical direction.
	//
	// For example, if the X axis is pointing left and you drag left, this will ensure that the widget moves left.
	//Only valid for single axis movement.  For planar movement, this widget gets caught up at the origin and oscillates
	if(( AxisDir[idx] < 0 ) && ((CurrentAxis == AXIS_X) || (CurrentAxis == AXIS_Y) || (CurrentAxis == AXIS_Z)))
	{
		Val *= -1;
	}

	const INT WidgetMode = GEditorModeTools().GetWidgetMode();

	// Honor INI option to invert Z axis movement on the widget
	if( idx == 1 && (CurrentAxis&AXIS_Z) && GEditor->InvertwidgetZAxis && ((WidgetMode==WM_Translate) || (WidgetMode==WM_Rotate) || (WidgetMode==WM_TranslateRotateZ)) &&
		// Don't apply this if the origin and the AxisEnd are the same
		AxisDir.IsNearlyZero() == FALSE)
	{
		Val *= -1;
	}

	FMatrix InputCoordSystem = GEditorModeTools().GetCustomInputCoordinateSystem();

	switch( WidgetMode )
	{
		case WM_Translate:
			switch( CurrentAxis )
			{
				case AXIS_X:	InDrag = FVector( Val, 0, 0 );	break;
				case AXIS_Y:	InDrag = FVector( 0, Val, 0 );	break;
				case AXIS_Z:	InDrag = FVector( 0, 0, -Val );	break;
				case AXIS_XY:	InDrag = ( InDiff.X != 0 ? FVector( 0, Val, 0 ) : FVector( -Val, 0, 0 ) );	break;
				case AXIS_XZ:	InDrag = ( InDiff.X != 0 ? FVector( Val, 0, 0 ) : FVector( 0, 0, Val ) );	break;
				case AXIS_YZ:	InDrag = ( InDiff.X != 0 ? FVector( 0, Val, 0 ) : FVector( 0, 0, Val ) );	break;
			}

			InDrag = InputCoordSystem.TransformFVector( InDrag );
			break;

		case WM_Rotate:
			{
				FVector Axis;
				switch( CurrentAxis )
				{
					case AXIS_X:	Axis = FVector( -1, 0, 0 );	break;
					case AXIS_Y:	Axis = FVector( 0, -1, 0 );	break;
					case AXIS_Z:	Axis = FVector( 0, 0, 1 );	break;
					default:		checkf( 0, TEXT("Axis not correctly set while rotating!") ); break;
				}

				Axis = CustomCoordSystem.TransformNormal( Axis );

				const FLOAT RotationSpeed = (2.f*(FLOAT)PI)/65536.f;
				const FQuat DeltaQ( Axis, Val*RotationSpeed );
				GEditorModeTools().CurrentDeltaRotation = Val;
				
				InRotation = FRotator(DeltaQ);
			}
			break;

		case WM_Scale:
			InScale = FVector( Val, Val, Val );
			break;

		case WM_ScaleNonUniform:
			{
				FVector Axis;
				switch( CurrentAxis )
				{
					case AXIS_X:	Axis = FVector( 1, 0, 0 );	break;
					case AXIS_Y:	Axis = FVector( 0, 1, 0 );	break;
					case AXIS_Z:	Axis = FVector( 0, 0, 1 );	break;
					case AXIS_XY:	Axis = ( InDiff.X != 0 ? FVector( 1, 0, 0 ) : FVector( 0, 1, 0 ) );	break;
					case AXIS_XZ:	Axis = ( InDiff.X != 0 ? FVector( 1, 0, 0 ) : FVector( 0, 0, 1 ) );	break;
					case AXIS_YZ:	Axis = ( InDiff.X != 0 ? FVector( 0, 1, 0 ) : FVector( 0, 0, 1 ) );	break;
					case AXIS_XYZ:	Axis = FVector( 1, 1, 1 ); break;
				}

				InScale = Axis * Val;
			}
			break;
		case WM_TranslateRotateZ:
			switch( CurrentAxis )
			{
				case AXIS_X:	InDrag = FVector( Val, 0, 0 );	break;
				case AXIS_Y:	InDrag = FVector( 0, Val, 0 );	break;
				case AXIS_Z:	InDrag = FVector( 0, 0, -Val );	break;
			}

			InDrag = InputCoordSystem.TransformFVector( InDrag );
			break;
		default:
			break;
	}
}

/**
 * For axis movement, get the "best" planar normal and axis mask
 * @param InAxis - Axis of movement
 * @param InDirToPixel - 
 * @param OutPlaneNormal - Normal of the plane to project the mouse onto
 * @param OutMask - Used to mask out the component of the planar movement we want
 */
void GetAxisPlaneNormalAndMask(const FMatrix& InCoordSystem, const FVector& InAxis, const FVector& InDirToPixel, FVector& OutPlaneNormal, FVector& NormalToRemove)
{
	FVector XAxis = InCoordSystem.TransformNormal(FVector(1, 0, 0));
	FVector YAxis = InCoordSystem.TransformNormal(FVector(0, 1, 0));
	FVector ZAxis = InCoordSystem.TransformNormal(FVector(0, 0, 1));

	FLOAT XDot = Abs(InDirToPixel | XAxis);
	FLOAT YDot = Abs(InDirToPixel | YAxis);
	FLOAT ZDot = Abs(InDirToPixel | ZAxis);

	if ((InAxis|XAxis) > .1f)
	{
		OutPlaneNormal = (YDot > ZDot) ? YAxis : ZAxis;
		NormalToRemove = (YDot > ZDot) ? ZAxis : YAxis;
	}
	else if ((InAxis|YAxis) > .1f)
	{
		OutPlaneNormal = (XDot > ZDot) ? XAxis : ZAxis;
		NormalToRemove = (XDot > ZDot) ? ZAxis : XAxis;
	}
	else
	{
		OutPlaneNormal = (XDot > YDot) ? XAxis : YAxis;
		NormalToRemove = (XDot > YDot) ? YAxis : XAxis;
	}
}

/**
 * For planar movement, get the "best" planar normal and axis mask
 * @param InAxis - Axis of movement
 * @param OutPlaneNormal - Normal of the plane to project the mouse onto
 * @param OutMask - Used to mask out the component of the planar movement we want
 */
void GetPlaneNormalAndMask(const FVector& InAxis, FVector& OutPlaneNormal, FVector& NormalToRemove)
{
	OutPlaneNormal = InAxis;
	NormalToRemove = InAxis;
}

/**
 * Absolute Translation conversion from mouse movement on the screen to widget axis movement/rotation.
 */
void FWidget::AbsoluteTranslationConvertMouseMovementToAxisMovement(FSceneView* InView, FEditorLevelViewportClient* InViewportClient, const FVector& InLocation, const FVector2D& InMousePosition, FVector& OutDrag, FRotator& OutRotation, FVector& OutScale )
{
	//reset all output variables
	//OutDrag = FVector::ZeroVector;
	//OutScale = FVector::ZeroVector;
	//OutRotation = FRotator(0,0,0);

	// Compute a world space ray from the screen space mouse coordinates
	FViewportCursorLocation MouseViewportRay( InView, InViewportClient, InMousePosition.X, InMousePosition.Y );

	FAbsoluteMovementParams Params;
	Params.EyePos = MouseViewportRay.GetOrigin();
	Params.PixelDir = MouseViewportRay.GetDirection();
	Params.CameraDir = InView->GetViewDirection();
	Params.Position = InLocation;
	//dampen by 
	Params.bMovementLockedToCamera = InViewportClient->Input->IsShiftPressed();
	Params.bPositionSnapping = TRUE;

	Params.XAxis = CustomCoordSystem.TransformNormal(FVector(1, 0, 0));
	Params.YAxis = CustomCoordSystem.TransformNormal(FVector(0, 1, 0));
	Params.ZAxis = CustomCoordSystem.TransformNormal(FVector(0, 0, 1));


	//FVector RayToPixel = InViewportClient->
	switch( GEditorModeTools().GetWidgetMode() )
	{
		case WM_Translate:
		{
			switch( CurrentAxis )
			{
				case AXIS_X:  GetAxisPlaneNormalAndMask(CustomCoordSystem, Params.XAxis, Params.CameraDir, Params.PlaneNormal, Params.NormalToRemove); break;
				case AXIS_Y:  GetAxisPlaneNormalAndMask(CustomCoordSystem, Params.YAxis, Params.CameraDir, Params.PlaneNormal, Params.NormalToRemove); break;
				case AXIS_Z:  GetAxisPlaneNormalAndMask(CustomCoordSystem, Params.ZAxis, Params.CameraDir, Params.PlaneNormal, Params.NormalToRemove); break;
				case AXIS_XY: GetPlaneNormalAndMask(Params.ZAxis, Params.PlaneNormal, Params.NormalToRemove); break;
				case AXIS_XZ: GetPlaneNormalAndMask(Params.YAxis, Params.PlaneNormal, Params.NormalToRemove); break;
				case AXIS_YZ: GetPlaneNormalAndMask(Params.XAxis, Params.PlaneNormal, Params.NormalToRemove); break;
			}

			OutDrag = GetAbsoluteTranslationDelta (Params);

			break;
		}

		case WM_TranslateRotateZ:
		{
			FVector LineToUse;
			switch( CurrentAxis )
			{
				case AXIS_X:	
					{
						GetAxisPlaneNormalAndMask(CustomCoordSystem, Params.XAxis, Params.CameraDir, Params.PlaneNormal, Params.NormalToRemove);
						OutDrag = GetAbsoluteTranslationDelta (Params);
						break;
					}
				case AXIS_Y:	
					{
						GetAxisPlaneNormalAndMask(CustomCoordSystem, Params.YAxis, Params.CameraDir, Params.PlaneNormal, Params.NormalToRemove);
						OutDrag = GetAbsoluteTranslationDelta (Params);
						break;
					}
				case AXIS_Z:	
				{
					GetAxisPlaneNormalAndMask(CustomCoordSystem, Params.ZAxis, Params.CameraDir, Params.PlaneNormal, Params.NormalToRemove);
					OutDrag = GetAbsoluteTranslationDelta (Params);
					break;
				}
				case AXIS_XY:
				{
					GetPlaneNormalAndMask(Params.ZAxis, Params.PlaneNormal, Params.NormalToRemove);
					OutDrag = GetAbsoluteTranslationDelta (Params);
					break;
				}
				//Rotate about the z-axis
				case AXIS_ZROTATION:
				{
					//no position snapping, we'll handle the rotation snapping elsewhere
					Params.bPositionSnapping = FALSE;

					//find new point on the 
					GetPlaneNormalAndMask(Params.ZAxis, Params.PlaneNormal, Params.NormalToRemove);
					//No DAMPING
					Params.bMovementLockedToCamera = FALSE;
					//this is the one movement type where we want to always use the widget origin and 
					//NOT the "first click" origin
					FVector XYPlaneProjectedPosition = GetAbsoluteTranslationDelta (Params) + InitialTranslationOffset;

					//remove the component along the normal we want to mute
					FLOAT MovementAlongMutedAxis = XYPlaneProjectedPosition|Params.NormalToRemove;
					XYPlaneProjectedPosition = XYPlaneProjectedPosition - (Params.NormalToRemove*MovementAlongMutedAxis);

					if (!XYPlaneProjectedPosition.Normalize())
					{
						XYPlaneProjectedPosition = Params.XAxis;
					}

					//NOW, find the rotation around the PlaneNormal to make the xaxis point at InDrag
					OutRotation = FRotator::ZeroRotator;
					OutRotation.Yaw = XYPlaneProjectedPosition.Rotation().Yaw - GEditorModeTools().TranslateRotateXAxisAngle;

					if (bSnapEnabled)
					{
						GEditor->Constraints.Snap( OutRotation );
					}

					break;
				}
				default:
					break;
			}
		}
	}
}

/** Only some modes support Absolute Translation Movement */
UBOOL FWidget::AllowsAbsoluteTranslationMovement(void)
{
	EWidgetMode CurrentMode = GEditorModeTools().GetWidgetMode();
	if ((CurrentMode == WM_Translate) || (CurrentMode == WM_TranslateRotateZ))
	{
		return TRUE;
	}
	return FALSE;
}

/** 
 * Serializes the widget references so they don't get garbage collected.
 *
 * @param Ar	FArchive to serialize with
 */
void FWidget::Serialize(FArchive& Ar)
{
	if (!Ar.IsLoading() && !Ar.IsSaving())
	{
		Ar << AxisMaterialX << AxisMaterialY << AxisMaterialZ << PlaneMaterialXY << GridMaterial << CurrentMaterial;
	}
}

#define CAMERA_LOCK_DAMPING_FACTOR .1f
#define MAX_CAMERA_MOVEMENT_SPEED 512.0f
/**
 * Returns the Delta from the current position that the absolute movement system wants the object to be at
 * @param InParams - Structure containing all the information needed for absolute movement
 * @return - The requested delta from the current position
 */
FVector FWidget::GetAbsoluteTranslationDelta (const FAbsoluteMovementParams& InParams)
{
	FPlane MovementPlane(InParams.Position, InParams.PlaneNormal);
	FVector ProposedEndofEyeVector = InParams.EyePos + InParams.PixelDir;

	//default to not moving
	FVector RequestedPosition = InParams.Position;

	FLOAT DotProductWithPlaneNormal = InParams.PixelDir|InParams.PlaneNormal;
	//check to make sure we're not co-planar
	if (Abs(DotProductWithPlaneNormal) > DELTA)
	{
		//Get closest point on plane
		RequestedPosition = FLinePlaneIntersection(InParams.EyePos, ProposedEndofEyeVector, MovementPlane);
	}

	//drag is a delta position, so just update the different between the previous position and the new position
	FVector DeltaPosition = RequestedPosition - InParams.Position;

	//Retrieve the initial offset, passing in the current requested position and the current position
	FVector InitialOffset = GetAbsoluteTranslationInitialOffset(RequestedPosition, InParams.Position);

	//subtract off the initial offset (where the widget was clicked) to prevent popping
	DeltaPosition -= InitialOffset;

	//remove the component along the normal we want to mute
	FLOAT MovementAlongMutedAxis = DeltaPosition|InParams.NormalToRemove;
	FVector OutDrag = DeltaPosition - (InParams.NormalToRemove*MovementAlongMutedAxis);

	if (InParams.bMovementLockedToCamera)
	{
		//DAMPEN ABSOLUTE MOVEMENT when the camera is locked to the object
		OutDrag *= CAMERA_LOCK_DAMPING_FACTOR;
		OutDrag.X = Clamp(OutDrag.X, -MAX_CAMERA_MOVEMENT_SPEED, MAX_CAMERA_MOVEMENT_SPEED);
		OutDrag.Y = Clamp(OutDrag.Y, -MAX_CAMERA_MOVEMENT_SPEED, MAX_CAMERA_MOVEMENT_SPEED);
		OutDrag.Z = Clamp(OutDrag.Z, -MAX_CAMERA_MOVEMENT_SPEED, MAX_CAMERA_MOVEMENT_SPEED);
	}

	//the they requested position snapping and we're not moving with the camera
	if (InParams.bPositionSnapping && !InParams.bMovementLockedToCamera && bSnapEnabled)
	{
		FVector MovementAlongAxis = FVector(OutDrag|InParams.XAxis, OutDrag|InParams.YAxis, OutDrag|InParams.ZAxis);
		//translation (either xy plane or z)
		GEditor->Constraints.Snap( MovementAlongAxis, FVector(GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize(),GEditor->Constraints.GetGridSize()) );
		OutDrag = MovementAlongAxis.X*InParams.XAxis + MovementAlongAxis.Y*InParams.YAxis + MovementAlongAxis.Z*InParams.ZAxis;
	}

	//get the distance from the original position to the new proposed position 
	FVector DeltaFromStart = InParams.Position + OutDrag - InitialTranslationPosition;

	//Get the vector from the eye to the proposed new position (to make sure it's not behind the camera
	FVector EyeToNewPosition = RequestedPosition - InParams.EyePos;
	FLOAT BehindTheCameraDotProduct = EyeToNewPosition|InParams.CameraDir;

	//clamp so we don't lose objects off the edge of the world, or the requested position is behind the camera
	if ((DeltaFromStart.Size() > HALF_WORLD_MAX*.5) || ( BehindTheCameraDotProduct <= 0))
	{
		OutDrag = OutDrag.ZeroVector;
	}
	return OutDrag;
}

/**
 * Returns the offset from the initial selection point
 */
FVector FWidget::GetAbsoluteTranslationInitialOffset(const FVector& InNewPosition, const FVector& InCurrentPosition)
{
	if (!bAbsoluteTranslationInitialOffsetCached)
	{
		bAbsoluteTranslationInitialOffsetCached = TRUE;
		InitialTranslationOffset = InNewPosition - InCurrentPosition;
		InitialTranslationPosition = InCurrentPosition;
	}
	return InitialTranslationOffset;
}



/**
 * Returns TRUE if we're in Local Space editing mode or editing BSP (which uses the World axes anyway
 */
UBOOL FWidget::IsRotationLocalSpace (void) const
{
	UBOOL bIsLocalSpace = ( GEditorModeTools().CoordSystem == COORD_Local );
	//for bsp and things that don't have a "true" local space, they will always use world.  So do NOT invert.
	if (bIsLocalSpace && CustomCoordSystem.Equals(FMatrix::Identity))
	{
		bIsLocalSpace = FALSE;
	}
	return bIsLocalSpace;
}

/**
 * Returns the "word" representation of how far we have just rotated
 */
INT FWidget::GetDeltaRotation (void) const
{
	UBOOL bIsLocalSpace = IsRotationLocalSpace();
	return (bIsLocalSpace ? -1 : 1)*GEditorModeTools().TotalDeltaRotation;
}


BYTE LargeInnerAlpha = 0x1f;
BYTE SmallInnerAlpha = 0x0f;
BYTE LargeOuterAlpha = 0x3f;
BYTE SmallOuterAlpha = 0x0f;

/**
 * If actively dragging, draws a ring representing the potential rotation of the selected objects, snap ticks, and "delta" markers
 * If not actively dragging, draws a quarter ring representing the closest quadrant to the camera
 * @param View - Information about the scene/camera/etc
 * @param PDI - Drawing interface
 * @param InAxis - Enumeration of axis to rotate about
 * @param InLocation - The Origin of the widget
 * @param Axis0 - The Axis that describes a 0 degree rotation
 * @param Axis1 - The Axis that describes a 90 degree rotation
 * @param InDirectionToWidget - Direction from camera to the widget
 * @param InColor - The color associated with the axis of rotation
 * @param InScale - Multiplier to maintain a constant screen size for rendering the widget
 */
void FWidget::DrawRotationArc(const FSceneView* View, FPrimitiveDrawInterface* PDI, EAxis InAxis, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const FVector& InDirectionToWidget, const FColor& InColor, const FLOAT InScale)
{
	UBOOL bIsPerspective = ( View->ProjectionMatrix.M[3][3] < 1.0f );
	UBOOL bIsOrtho = !bIsPerspective;
	//if we're in an ortho viewport and the ring is perpendicular to the camera (both Axis0 & Axis1 are perpendicular)
	UBOOL bIsOrthoDrawingFullRing = bIsOrtho && (Abs(Axis0|InDirectionToWidget) < KINDA_SMALL_NUMBER) && (Abs(Axis1|InDirectionToWidget) < KINDA_SMALL_NUMBER);

	FColor ArcColor = InColor;
	ArcColor.A = LargeOuterAlpha;

	if (bDragging || (bIsOrthoDrawingFullRing))
	{
		if ((CurrentAxis&InAxis) || (bIsOrthoDrawingFullRing))
		{
			const INT WORD90Degrees = (MAXWORD >> 2)+1;

			INT DeltaRotation = GetDeltaRotation();
			INT WORDRotation = DeltaRotation&MAXWORD;
			INT AbsWORDRotation = Abs(DeltaRotation)&MAXWORD;
			FLOAT AngleOfChange (2*PI*AbsWORDRotation / MAXWORD);

			//always draw clockwise, so if we're negative we need to flip the angle
			FLOAT StartAngle = DeltaRotation < 0.0f ? -AngleOfChange : 0.0f;
			FLOAT FilledAngle = AngleOfChange;

			//the axis of rotation
			FVector ZAxis = Axis0 ^ Axis1;

			ArcColor.A = LargeOuterAlpha;
			DrawPartialRotationArc(View, PDI, InAxis, InLocation,  Axis0, Axis1, StartAngle, StartAngle + FilledAngle, ArcColor, InScale);
			ArcColor.A = SmallOuterAlpha;
			DrawPartialRotationArc(View, PDI, InAxis, InLocation,  Axis0, Axis1, StartAngle + FilledAngle, StartAngle + 2*PI, ArcColor, InScale);

			ArcColor = (CurrentAxis&InAxis) ? CurrentColor : ArcColor;
			//Hallow Arrow
			ArcColor.A = 0;
			DrawStartStopMarker(PDI, InLocation, Axis0, Axis1, 0, ArcColor, InScale);
			//Filled Arrow
			ArcColor.A = LargeOuterAlpha;
			DrawStartStopMarker(PDI, InLocation, Axis0, Axis1, DeltaRotation, ArcColor, InScale);

			if (GEditor->Constraints.RotGridEnabled)
			{
				INT DeltaAngle = GEditor->Constraints.RotGridSize.Yaw;
				//every 22.5 degrees
				INT BigTickMask = (MAXWORD >> 4);
				for (INT Angle = 0; Angle < MAXWORD; Angle+=DeltaAngle)
				{
					FVector GridAxis = Axis0.RotateAngleAxis(Angle, ZAxis);
					FLOAT PercentSize = ((Angle & BigTickMask)==0) ? .75f : .25f;
					if ((Angle & (WORD90Degrees-1)) != 0)
					{
						DrawSnapMarker(PDI, InLocation,  GridAxis,  FVector::ZeroVector, ArcColor, InScale, 0.0f, PercentSize);
					}
				}
			}

			//draw axis tick marks
			FColor AxisColor = InColor;
			//Rotate Colors to match Axis 0
			Swap(AxisColor.R, AxisColor.G);
			Swap(AxisColor.B, AxisColor.R);
			AxisColor.A = (WORDRotation == 0) ? MAXBYTE : LargeOuterAlpha;
			DrawSnapMarker(PDI, InLocation,  Axis0,  Axis1, AxisColor, InScale, .25f);
			AxisColor.A = (WORDRotation == 2*WORD90Degrees) ? MAXBYTE : LargeOuterAlpha;
			DrawSnapMarker(PDI, InLocation, -Axis0, -Axis1, AxisColor, InScale, .25f);

			//Rotate Colors to match Axis 1
			Swap(AxisColor.R, AxisColor.G);
			Swap(AxisColor.B, AxisColor.R);
			AxisColor.A = (WORDRotation == 1*WORD90Degrees) ? MAXBYTE : LargeOuterAlpha;
			DrawSnapMarker(PDI, InLocation,  Axis1, -Axis0, AxisColor, InScale, .25f);
			AxisColor.A = (WORDRotation == 3*WORD90Degrees) ? MAXBYTE : LargeOuterAlpha;
			DrawSnapMarker(PDI, InLocation, -Axis1,  Axis0, AxisColor, InScale, .25f);

			if (bDragging)
			{
				INT OffsetAngle = IsRotationLocalSpace() ? 0 : WORDRotation;

				FLOAT DisplayAngle = (2*PI*DeltaRotation / MAXWORD);
				CacheRotationHUDText(View, PDI, InLocation, Axis0.RotateAngleAxis(OffsetAngle, ZAxis), Axis1.RotateAngleAxis(OffsetAngle, ZAxis), DisplayAngle, InScale);
			}
		}
	}
	else
	{
		//Reverse the axes based on camera view
		FVector RenderAxis0 = ((Axis0|InDirectionToWidget) <= 0.0f) ? Axis0 : -Axis0;
		FVector RenderAxis1 = ((Axis1|InDirectionToWidget) <= 0.0f) ? Axis1 : -Axis1;

		DrawPartialRotationArc(View, PDI, InAxis, InLocation, RenderAxis0, RenderAxis1, 0, PI/2, ArcColor, InScale);
	}
}

/**
 * If actively dragging, draws a ring representing the potential rotation of the selected objects, snap ticks, and "delta" markers
 * If not actively dragging, draws a quarter ring representing the closest quadrant to the camera
 * @param View - Information about the scene/camera/etc
 * @param PDI - Drawing interface
 * @param InAxis - Enumeration of axis to rotate about
 * @param InLocation - The Origin of the widget
 * @param Axis0 - The Axis that describes a 0 degree rotation
 * @param Axis1 - The Axis that describes a 90 degree rotation
 * @param InStartAngle - The starting angle about (Axis0^Axis1) to render the arc
 * @param InEndAngle - The ending angle about (Axis0^Axis1) to render the arc
 * @param InColor - The color associated with the axis of rotation
 * @param InScale - Multiplier to maintain a constant screen size for rendering the widget
 */
void FWidget::DrawPartialRotationArc(const FSceneView* View, FPrimitiveDrawInterface* PDI, EAxis InAxis, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const FLOAT InStartAngle, const FLOAT InEndAngle, const FColor& InColor, const FLOAT InScale)
{
	PDI->SetHitProxy( new HWidgetAxis(InAxis) );
	{
		FThickArcParams OuterArcParams(PDI, InLocation, PlaneMaterialXY, INNER_AXIS_CIRCLE_RADIUS*InScale, OUTER_AXIS_CIRCLE_RADIUS*InScale);
		FColor OuterColor = ( CurrentAxis&InAxis ? CurrentColor : InColor );
		//Pass through alpha
		OuterColor.A = InColor.A;
		DrawThickArc(OuterArcParams, Axis0, Axis1, InStartAngle, InEndAngle, OuterColor);
	}
	PDI->SetHitProxy( NULL );

	UBOOL bIsPerspective = ( View->ProjectionMatrix.M[3][3] < 1.0f );
	if (bIsPerspective)
	{
		FThickArcParams InnerArcParams(PDI, InLocation, GridMaterial, 0.0f, INNER_AXIS_CIRCLE_RADIUS*InScale);
		FColor InnerColor = InColor;
		//if something is selected and it's not this
		InnerColor.A = ((CurrentAxis & InAxis) && !bDragging) ? LargeInnerAlpha : SmallInnerAlpha;
		DrawThickArc(InnerArcParams, Axis0, Axis1, InStartAngle, InEndAngle, InnerColor);
	}
}

/**
 * Renders a portion of an arc for the rotation widget
 * @param InParams - Material, Radii, etc
 * @param InStartAxis - Start of the arc
 * @param InEndAxis - End of the arc
 * @param InColor - Color to use for the arc
 */
void FWidget::DrawThickArc (const FThickArcParams& InParams, const FVector& Axis0, const FVector& Axis1, const FLOAT InStartAngle, const FLOAT InEndAngle, const FColor& InColor)
{
	if (InColor.A == 0)
	{
		return;
	}
	const INT NumPoints = appTrunc(AXIS_CIRCLE_SIDES * (InEndAngle-InStartAngle)/(PI/2)) + 1;

	FColor TriangleColor = InColor;
	FColor RingColor = InColor;
	RingColor.A = MAXBYTE;

	FVector ZAxis = Axis0 ^ Axis1;
	FVector LastVertex;

	FDynamicMeshBuilder MeshBuilder;

	for (INT RadiusIndex = 0; RadiusIndex < 2; ++RadiusIndex)
	{
		FLOAT Radius = (RadiusIndex == 0) ? InParams.OuterRadius : InParams.InnerRadius;
		FLOAT TCRadius = Radius / (float) InParams.OuterRadius;
		//Compute vertices for base circle.
		for(INT VertexIndex = 0;VertexIndex <= NumPoints;VertexIndex++)
		{
			FLOAT Percent = VertexIndex/(float)NumPoints;
			FLOAT Angle = Lerp(InStartAngle, InEndAngle, Percent);
			INT WORDAngle = appTrunc(MAXWORD*Angle/(2*PI)) & MAXWORD;

			FVector VertexDir = Axis0.RotateAngleAxis(WORDAngle, ZAxis);
			VertexDir.Normalize();

			FLOAT TCAngle = Percent*(PI/2);
			FVector2D TC(TCRadius*appCos(Angle), TCRadius*appSin(Angle));

			const FVector VertexPosition = InParams.Position + VertexDir*Radius;
			FVector Normal = VertexPosition - InParams.Position;
			Normal.Normalize();

			FDynamicMeshVertex MeshVertex;
			MeshVertex.Position = VertexPosition;
			MeshVertex.Color = TriangleColor;
			MeshVertex.TextureCoordinate = TC;

			MeshVertex.SetTangents(
				-ZAxis,
				(-ZAxis) ^ Normal,
				Normal
				);

			MeshBuilder.AddVertex(MeshVertex); //Add bottom vertex

			if (VertexIndex != 0)
			{
				InParams.PDI->DrawLine(LastVertex,VertexPosition,RingColor,SDPG_UnrealEdForeground);
			}
			LastVertex = VertexPosition;
		}
	}

	//Add top/bottom triangles, in the style of a fan.
	INT InnerVertexStartIndex = NumPoints + 1;
	for(INT VertexIndex = 0; VertexIndex < NumPoints; VertexIndex++)
	{
		MeshBuilder.AddTriangle(VertexIndex, VertexIndex+1, InnerVertexStartIndex+VertexIndex);
		MeshBuilder.AddTriangle(VertexIndex+1, InnerVertexStartIndex+VertexIndex+1, InnerVertexStartIndex+VertexIndex);
	}

	MeshBuilder.Draw(InParams.PDI, FMatrix::Identity, InParams.Material->GetRenderProxy(FALSE),SDPG_UnrealEdForeground,0.f);
}

/**
 * Draws protractor like ticks where the rotation widget would snap too.
 * Also, used to draw the wider axis tick marks
 * @param PDI - Drawing interface
 * @param InLocation - The Origin of the widget
 * @param Axis0 - The Axis that describes a 0 degree rotation
 * @param Axis1 - The Axis that describes a 90 degree rotation
 * @param InAngle - The Angle to rotate about the axis of rotation, the vector (Axis0 ^ Axis1)
 * @param InColor - The color to use for line/poly drawing
 * @param InScale - Multiplier to maintain a constant screen size for rendering the widget
 * @param InWidthPercent - The percent of the distance between the outer ring and inner ring to use for tangential thickness
 * @param InPercentSize - The percent of the distance between the outer ring and inner ring to use for radial distance
 */
void FWidget::DrawSnapMarker(FPrimitiveDrawInterface* PDI, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const FColor& InColor, const FLOAT InScale, const FLOAT InWidthPercent, const FLOAT InPercentSize)
{
	const FLOAT InnerDistance = (INNER_AXIS_CIRCLE_RADIUS*InScale);
	const FLOAT OuterDistance = (OUTER_AXIS_CIRCLE_RADIUS*InScale);
	const FLOAT MaxMarkerHeight = OuterDistance - InnerDistance;
	const FLOAT MarkerWidth = MaxMarkerHeight*InWidthPercent;
	const FLOAT MarkerHeight = MaxMarkerHeight*InPercentSize;

	FVector Vertices[4];
	Vertices[0] = InLocation + (OuterDistance)*Axis0 - (MarkerWidth*.5)*Axis1;
	Vertices[1] = Vertices[0] + (MarkerWidth)*Axis1;
	Vertices[2] = InLocation + (OuterDistance-MarkerHeight)*Axis0 - (MarkerWidth*.5)*Axis1;
	Vertices[3] = Vertices[2] + (MarkerWidth)*Axis1;

	//draw at least one line
	PDI->DrawLine(Vertices[0], Vertices[2], InColor, SDPG_UnrealEdForeground);

	//if there should be thickness, draw the other lines
	if (InWidthPercent > 0.0f)
	{
		PDI->DrawLine(Vertices[0], Vertices[1], InColor, SDPG_UnrealEdForeground);
		PDI->DrawLine(Vertices[1], Vertices[3], InColor, SDPG_UnrealEdForeground);
		PDI->DrawLine(Vertices[2], Vertices[3], InColor, SDPG_UnrealEdForeground);

		//fill in the box
		FDynamicMeshBuilder MeshBuilder;

		for(INT VertexIndex = 0;VertexIndex < 4; VertexIndex++)
		{
			FDynamicMeshVertex MeshVertex;
			MeshVertex.Position = Vertices[VertexIndex];
			MeshVertex.Color = InColor;
			MeshVertex.TextureCoordinate = FVector2D(0.0f, 0.0f);
			MeshVertex.SetTangents(
				Axis0,
				Axis1,
				(Axis0) ^ Axis1
				);
			MeshBuilder.AddVertex(MeshVertex); //Add bottom vertex
		}

		MeshBuilder.AddTriangle(0, 1, 2);
		MeshBuilder.AddTriangle(1, 3, 2);
		MeshBuilder.Draw(PDI, FMatrix::Identity, PlaneMaterialXY->GetRenderProxy(FALSE),SDPG_UnrealEdForeground,0.f);
	}
}

/**
 * Draw Start/Stop Marker to show delta rotations along the arc of rotation
 * @param PDI - Drawing interface
 * @param InLocation - The Origin of the widget
 * @param Axis0 - The Axis that describes a 0 degree rotation
 * @param Axis1 - The Axis that describes a 90 degree rotation
 * @param InAngle - The Angle to rotate about the axis of rotation, the vector (Axis0 ^ Axis1)
 * @param InColor - The color to use for line/poly drawing
 * @param InScale - Multiplier to maintain a constant screen size for rendering the widget
 */
void FWidget::DrawStartStopMarker(FPrimitiveDrawInterface* PDI, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const INT InAngle, const FColor& InColor, const FLOAT InScale)
{
	const FLOAT ArrowHeightPercent = .8f;
	const FLOAT InnerDistance = (INNER_AXIS_CIRCLE_RADIUS*InScale);
	const FLOAT OuterDistance = (OUTER_AXIS_CIRCLE_RADIUS*InScale);
	const FLOAT RingHeight = OuterDistance - InnerDistance;
	const FLOAT ArrowHeight = RingHeight*ArrowHeightPercent;
	const FLOAT ThirtyDegrees = PI / 6.0f;
	const FLOAT HalfArrowidth = ArrowHeight*appTan(ThirtyDegrees);

	FVector ZAxis = Axis0 ^ Axis1;
	FVector RotatedAxis0 = Axis0.RotateAngleAxis(InAngle, ZAxis);
	FVector RotatedAxis1 = Axis1.RotateAngleAxis(InAngle, ZAxis);

	FVector Vertices[3];
	Vertices[0] = InLocation + (OuterDistance)*RotatedAxis0;
	Vertices[1] = Vertices[0] + (ArrowHeight)*RotatedAxis0 - HalfArrowidth*RotatedAxis1;
	Vertices[2] = Vertices[1] + (2*HalfArrowidth)*RotatedAxis1;

	PDI->DrawLine(Vertices[0], Vertices[1], InColor, SDPG_UnrealEdForeground);
	PDI->DrawLine(Vertices[1], Vertices[2], InColor, SDPG_UnrealEdForeground);
	PDI->DrawLine(Vertices[0], Vertices[2], InColor, SDPG_UnrealEdForeground);

	if (InColor.A > 0)
	{
		//fill in the box
		FDynamicMeshBuilder MeshBuilder;

		for(INT VertexIndex = 0;VertexIndex < 3; VertexIndex++)
		{
			FDynamicMeshVertex MeshVertex;
			MeshVertex.Position = Vertices[VertexIndex];
			MeshVertex.Color = InColor;
			MeshVertex.TextureCoordinate = FVector2D(0.0f, 0.0f);
			MeshVertex.SetTangents(
				RotatedAxis0,
				RotatedAxis1,
				(RotatedAxis0) ^ RotatedAxis1
				);
			MeshBuilder.AddVertex(MeshVertex); //Add bottom vertex
		}

		MeshBuilder.AddTriangle(0, 1, 2);
		MeshBuilder.Draw(PDI, FMatrix::Identity, PlaneMaterialXY->GetRenderProxy(FALSE),SDPG_UnrealEdForeground,0.f);
	}
}

/**
 * Caches off HUD text to display after 3d rendering is complete
 * @param View - Information about the scene/camera/etc
 * @param PDI - Drawing interface
 * @param InLocation - The Origin of the widget
 * @param Axis0 - The Axis that describes a 0 degree rotation
 * @param Axis1 - The Axis that describes a 90 degree rotation
 * @param AngleOfAngle - angle we've rotated so far (in degrees)
 */
void FWidget::CacheRotationHUDText(const FSceneView* View, FPrimitiveDrawInterface* PDI, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const FLOAT AngleOfChange, const FLOAT InScale)
{
	const FLOAT TextDistance = (ROTATION_TEXT_RADIUS*InScale);

	FVector AxisVectors[4] = { Axis0, Axis1, -Axis0, -Axis1};

	for (int i = 0 ; i < 4; ++i)
	{
		FVector PotentialTextPosition = InLocation + (TextDistance)*AxisVectors[i];
		if(View->ScreenToPixel(View->WorldToScreen(PotentialTextPosition), HUDInfoPos))
		{
			if (IsWithin<FLOAT>(HUDInfoPos.X, 0, View->SizeX) && IsWithin<FLOAT>(HUDInfoPos.Y, 0, View->SizeY))
			{
				FLOAT AngleInDegrees = AngleOfChange*360.0f/(2*PI);
				//only valid screen locations get a valid string
				HUDString = FString::Printf(TEXT("%3.2f"), AngleInDegrees);
				break;
			}
		}
	}
}
