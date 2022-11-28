
#include "EditorPrivate.h"

/*------------------------------------------------------------------------------
    FWidget.
------------------------------------------------------------------------------*/

FWidget::FWidget()
{
	// Compute and store sample vertices for drawing the axis arrow heads

    for( INT s = 0 ; s <= AXIS_ARROW_SEGMENTS ; s++ )
	{
		FLOAT theta =  PI * 2.f * s / AXIS_ARROW_SEGMENTS;

		FVector vtx(80,0,0);
		vtx.Y += AXIS_ARROW_RADIUS * appSin( theta );
		vtx.Z += AXIS_ARROW_RADIUS * appCos( theta );

		verts[s] = FRawTriangleVertex( FVector(vtx.X,vtx.Y,vtx.Z) * 0.5 );
	}

	rootvtx = FRawTriangleVertex( FVector(64,0,0) );

	AxisColorX = FColor(255,0,0);
	AxisColorY = FColor(0,255,0);
	AxisColorZ = FColor(0,0,255);
	CurrentColor = FColor(255,255,0);

	AxisMaterialX = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(),NULL,TEXT("EditorMaterials.WidgetMaterial_X"),NULL,LOAD_NoFail,NULL );
	AxisMaterialY = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(),NULL,TEXT("EditorMaterials.WidgetMaterial_Y"),NULL,LOAD_NoFail,NULL );
	AxisMaterialZ = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(),NULL,TEXT("EditorMaterials.WidgetMaterial_Z"),NULL,LOAD_NoFail,NULL );
	CurrentMaterial = (UMaterial*)UObject::StaticLoadObject( UMaterial::StaticClass(),NULL,TEXT("EditorMaterials.WidgetMaterial_Current"),NULL,LOAD_NoFail,NULL );

	CurrentAxis = AXIS_None;

	CustomCoordSystem = FMatrix::Identity;
}

FWidget::~FWidget()
{
}

void FWidget::RenderForeground( const FSceneContext& Context,FPrimitiveRenderInterface* PRI )
{
	// If hiding the widget, leave.

	if( !GEditorModeTools.GetShowWidget() )
	{
		return;
	}

	// Let the mode determine if we should draw the widget.

	if( !GEditorModeTools.GetCurrentMode()->ShouldDrawWidget() )
	{
		return;
	}

	// See if there is a custom coordinate system we should be using

	CustomCoordSystem = GEditorModeTools.GetCustomDrawingCoordinateSystem();

	// If the current mode doesn't want to use the widget, don't draw it.

	if( !GEditorModeTools.GetCurrentMode()->UsesWidget() )
	{
		CurrentAxis = AXIS_None;
		return;
	}

	INT X, Y;

	FVector Loc = GEditorModeTools.GetWidgetLocation();

	Context.Project( Loc, X, Y );
	Origin.X = X;
	Origin.Y = Y;

	switch( GEditorModeTools.GetWidgetMode() )
	{
		case WM_Translate:
			Render_Translate( Context, PRI, Loc );
			break;

		case WM_Rotate:
			Render_Rotate( Context, PRI, Loc );
			break;

		case WM_Scale:
			Render_Scale( Context, PRI, Loc );
			break;

		case WM_ScaleNonUniform:
			Render_ScaleNonUniform( Context, PRI, Loc );
			break;

		default:
			break;
	}
};

/**
 * Draws an arrow head line for a specific axis.
 */

void FWidget::Render_Axis( const FSceneContext& Context, FPrimitiveRenderInterface* PRI, FTriangleRenderInterface* TRI, EAxis InAxis, FMatrix& InMatrix, UMaterialInstance* InMaterial, FColor* InColor, FVector& InAxisEnd, float InScale )
{
	PRI->SetHitProxy( new HWidgetAxis(InAxis) );

		PRI->DrawLine( InMatrix.TransformFVector( FVector(8,0,0) * InScale ), InMatrix.TransformFVector( FVector(48,0,0) * InScale ), *InColor );

		FScaleMatrix sm( FVector(InScale,InScale,InScale) );
		TRI = PRI->GetTRI( InMatrix, InMaterial->GetMaterialInterface(0), InMaterial->GetInstanceInterface() );
		for( INT s = 0 ; s < AXIS_ARROW_SEGMENTS ; s++ )
			TRI->DrawTriangle(
				FRawTriangleVertex( sm.TransformFVector( rootvtx.Position ) ),
				FRawTriangleVertex( sm.TransformFVector( verts[s].Position ) ),
				FRawTriangleVertex( sm.TransformFVector( verts[s+1].Position ) )
			);
		TRI->Finish();

		INT X, Y;
		//PRI->DrawWireStar(InMatrix.TransformFVector( sm.TransformFVector( FVector(96,0,0) ) ), 10.f, *InColor );
		Context.Project  ( InMatrix.TransformFVector( sm.TransformFVector( FVector(96,0,0) ) ), X, Y );
		InAxisEnd.X = X;
		InAxisEnd.Y = Y;

	PRI->SetHitProxy( NULL );
}

// Draws the translation widget

void FWidget::Render_Translate( const FSceneContext& Context,FPrimitiveRenderInterface* PRI, const FVector& InLocation )
{
	FLOAT Scale = Context.View->Project( InLocation ).W * ( 4 / (FLOAT)Context.SizeX / Context.View->ProjectionMatrix.M[0][0] );
	FTriangleRenderInterface* TRI = NULL;

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

	UBOOL bIsPerspective = ( Context.View->ProjectionMatrix.M[3][3] < 1.0f );

	INT DrawAxis = GEditorModeTools.GetCurrentMode()->GetWidgetAxisToDraw( GEditorModeTools.GetWidgetMode() );

	// Draw the axis lines with arrow heads
	if( DrawAxis&AXIS_X && (bIsPerspective || Context.View->ViewMatrix.M[0][0] != 0.f) )
	{
		Render_Axis( Context, PRI, TRI, AXIS_X, XMatrix, XMaterial, XColor, XAxisEnd, Scale );
	}

	if( DrawAxis&AXIS_Y && (bIsPerspective || Context.View->ViewMatrix.M[0][0] != 1.f) )
	{
		Render_Axis( Context, PRI, TRI, AXIS_Y, YMatrix, YMaterial, YColor, YAxisEnd, Scale );
	}

	if( DrawAxis&AXIS_Z && (bIsPerspective || Context.View->ViewMatrix.M[0][0] != -1.f) )
	{
		Render_Axis( Context, PRI, TRI, AXIS_Z, ZMatrix, ZMaterial, ZColor, ZAxisEnd, Scale );
	}

	// Draw the grabbers
	if(bIsPerspective || Context.View->ViewMatrix.M[0][0] == -1.f)
	{
		if( (DrawAxis&(AXIS_X|AXIS_Y)) == (AXIS_X|AXIS_Y) ) 
		{
			PRI->SetHitProxy( new HWidgetAxis(AXIS_XY) );
				PRI->DrawLine( XMatrix.TransformFVector(FVector(16,0,0) * Scale), XMatrix.TransformFVector(FVector(16,16,0) * Scale), *XColor );
				PRI->DrawLine( XMatrix.TransformFVector(FVector(16,16,0) * Scale), XMatrix.TransformFVector(FVector(0,16,0) * Scale), *YColor );
			PRI->SetHitProxy( NULL );
		}
	}

	if(bIsPerspective || Context.View->ViewMatrix.M[0][0] == 1.f)
	{
		if( (DrawAxis&(AXIS_X|AXIS_Z)) == (AXIS_X|AXIS_Z) ) 
		{
			PRI->SetHitProxy( new HWidgetAxis(AXIS_XZ) );
				PRI->DrawLine( XMatrix.TransformFVector(FVector(16,0,0) * Scale), XMatrix.TransformFVector(FVector(16,0,16) * Scale), *XColor );
				PRI->DrawLine( XMatrix.TransformFVector(FVector(16,0,16) * Scale), XMatrix.TransformFVector(FVector(0,0,16) * Scale), *ZColor );
			PRI->SetHitProxy( NULL );
		}
	}

	if(bIsPerspective || Context.View->ViewMatrix.M[0][0] == 0.f)
	{
		if( (DrawAxis&(AXIS_Y|AXIS_Z)) == (AXIS_Y|AXIS_Z) ) 
		{
			PRI->SetHitProxy( new HWidgetAxis(AXIS_YZ) );
				PRI->DrawLine( XMatrix.TransformFVector(FVector(0,16,0) * Scale), XMatrix.TransformFVector(FVector(0,16,16) * Scale), *YColor );
				PRI->DrawLine( XMatrix.TransformFVector(FVector(0,16,16) * Scale), XMatrix.TransformFVector(FVector(0,0,16) * Scale), *ZColor );
			PRI->SetHitProxy( NULL );
		}
	}
}

// Draws the rotation widget

void FWidget::Render_Rotate( const FSceneContext& Context,FPrimitiveRenderInterface* PRI, const FVector& InLocation )
{
	FLOAT Scale = Context.View->Project( InLocation ).W * ( 4 / (FLOAT)Context.SizeX / Context.View->ProjectionMatrix.M[0][0] );
	FTriangleRenderInterface* TRI = NULL;
	INT X, Y;

	// Figure out axis colors

	FColor *XColor = &( CurrentAxis&AXIS_X ? CurrentColor : AxisColorX );
	FColor *YColor = &( CurrentAxis&AXIS_Y ? CurrentColor : AxisColorY );
	FColor *ZColor = &( CurrentAxis&AXIS_Z ? CurrentColor : AxisColorZ );

	// Figure out axis matrices

	FMatrix XMatrix = FRotationMatrix( FRotator(0,16384,0) ) * FTranslationMatrix( InLocation );
	FMatrix YMatrix = FRotationMatrix( FRotator(0,16384,0) ) * FTranslationMatrix( InLocation );
	FMatrix ZMatrix = FTranslationMatrix( InLocation );

	INT DrawAxis = GEditorModeTools.GetCurrentMode()->GetWidgetAxisToDraw( GEditorModeTools.GetWidgetMode() );

	// Draw a circle for each axis

	if( DrawAxis&AXIS_X )
	{
		PRI->SetHitProxy( new HWidgetAxis(AXIS_X) );
			PRI->DrawCircle( InLocation, CustomCoordSystem.TransformFVector( FVector(0,1,0) ), CustomCoordSystem.TransformFVector( FVector(0,0,1) ), *XColor, AXIS_CIRCLE_RADIUS*Scale, AXIS_CIRCLE_SIDES );

			//PRI->DrawWireStar( XMatrix.TransformFVector( FVector(96,0,0) ), 10.f, *XColor );
			Context.Project( XMatrix.TransformFVector( FVector(96,0,0) ), X, Y );
			XAxisEnd.X = X;
			XAxisEnd.Y = Y;
		PRI->SetHitProxy( NULL );
	}

	if( DrawAxis&AXIS_Y )
	{
		PRI->SetHitProxy( new HWidgetAxis(AXIS_Y) );
			PRI->DrawCircle( InLocation, CustomCoordSystem.TransformFVector( FVector(1,0,0) ), CustomCoordSystem.TransformFVector( FVector(0,0,1) ), *YColor, AXIS_CIRCLE_RADIUS*Scale, AXIS_CIRCLE_SIDES );

			//PRI->DrawWireStar( YMatrix.TransformFVector( FVector(0,96,0) ), 10.f, *YColor );
			Context.Project( YMatrix.TransformFVector( FVector(0,96,0) ), X, Y );
			YAxisEnd.X = X;
			YAxisEnd.Y = Y;
		PRI->SetHitProxy( NULL );
	}

	if( DrawAxis&AXIS_Z )
	{
		PRI->SetHitProxy( new HWidgetAxis(AXIS_Z) );
			PRI->DrawCircle( InLocation, CustomCoordSystem.TransformFVector( FVector(1,0,0) ), CustomCoordSystem.TransformFVector( FVector(0,1,0) ), *ZColor, AXIS_CIRCLE_RADIUS*Scale, AXIS_CIRCLE_SIDES );

			//PRI->DrawWireStar( ZMatrix.TransformFVector( FVector(96,0,0) ), 10.f, *ZColor );
			Context.Project( ZMatrix.TransformFVector( FVector(96,0,0) ), X, Y );
			ZAxisEnd.X = X;
			ZAxisEnd.Y = Y;
		PRI->SetHitProxy( NULL );
	}
}

// Draws the uniform scaling widget

void FWidget::Render_Scale( const FSceneContext& Context,FPrimitiveRenderInterface* PRI, const FVector& InLocation )
{
	FLOAT Scale = Context.View->Project( InLocation ).W * ( 4 / (FLOAT)Context.SizeX / Context.View->ProjectionMatrix.M[0][0] );
	FTriangleRenderInterface* TRI = NULL;

	// Figure out axis colors

	FColor *Color = &( CurrentAxis != AXIS_None ? CurrentColor : AxisColorX );

	// Figure out axis materials

	UMaterial* Material = ( CurrentAxis&AXIS_X ? CurrentMaterial : AxisMaterialX );

	// Figure out axis matrices

	FMatrix XMatrix = CustomCoordSystem * FTranslationMatrix( InLocation );
	FMatrix YMatrix = FRotationMatrix( FRotator(0,16384,0) ) * CustomCoordSystem * FTranslationMatrix( InLocation );
	FMatrix ZMatrix = FRotationMatrix( FRotator(16384,0,0) ) * CustomCoordSystem * FTranslationMatrix( InLocation );

	// Draw the axis lines with arrow heads

	Render_Axis( Context, PRI, TRI, AXIS_X, XMatrix, Material, Color, XAxisEnd, Scale );
	Render_Axis( Context, PRI, TRI, AXIS_Y, YMatrix, Material, Color, YAxisEnd, Scale );
	Render_Axis( Context, PRI, TRI, AXIS_Z, ZMatrix, Material, Color, ZAxisEnd, Scale );

	PRI->SetHitProxy( NULL );
}

// Draws the non-uniform scaling widget

void FWidget::Render_ScaleNonUniform( const FSceneContext& Context,FPrimitiveRenderInterface* PRI, const FVector& InLocation )
{
	FLOAT Scale = Context.View->Project( InLocation ).W * ( 4 / (FLOAT)Context.SizeX / Context.View->ProjectionMatrix.M[0][0] );
	FTriangleRenderInterface* TRI = NULL;

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

	INT DrawAxis = GEditorModeTools.GetCurrentMode()->GetWidgetAxisToDraw( GEditorModeTools.GetWidgetMode() );

	// Draw the axis lines with arrow heads

	if( DrawAxis&AXIS_X )	Render_Axis( Context, PRI, TRI, AXIS_X, XMatrix, XMaterial, XColor, XAxisEnd, Scale );
	if( DrawAxis&AXIS_Y )	Render_Axis( Context, PRI, TRI, AXIS_Y, YMatrix, YMaterial, YColor, YAxisEnd, Scale );
	if( DrawAxis&AXIS_Z )	Render_Axis( Context, PRI, TRI, AXIS_Z, ZMatrix, ZMaterial, ZColor, ZAxisEnd, Scale );

	// Draw grabber handles

	if( (DrawAxis&(AXIS_X|AXIS_Y)) == (AXIS_X|AXIS_Y) )
	{
		PRI->SetHitProxy( new HWidgetAxis(AXIS_XY) );
			PRI->DrawLine( XMatrix.TransformFVector(FVector(16,0,0) * Scale), XMatrix.TransformFVector(FVector(8,8,0) * Scale), *XColor );
			PRI->DrawLine( XMatrix.TransformFVector(FVector(8,8,0) * Scale), XMatrix.TransformFVector(FVector(0,16,0) * Scale), *YColor );
		PRI->SetHitProxy( NULL );
	}

	if( (DrawAxis&(AXIS_X|AXIS_Z)) == (AXIS_X|AXIS_Z) )
	{
		PRI->SetHitProxy( new HWidgetAxis(AXIS_XZ) );
			PRI->DrawLine( XMatrix.TransformFVector(FVector(16,0,0) * Scale), XMatrix.TransformFVector(FVector(8,0,8) * Scale), *XColor );
			PRI->DrawLine( XMatrix.TransformFVector(FVector(8,0,8) * Scale), XMatrix.TransformFVector(FVector(0,0,16) * Scale), *ZColor );
		PRI->SetHitProxy( NULL );
	}

	if( (DrawAxis&(AXIS_Y|AXIS_Z)) == (AXIS_Y|AXIS_Z) )
	{
		PRI->SetHitProxy( new HWidgetAxis(AXIS_YZ) );
			PRI->DrawLine( XMatrix.TransformFVector(FVector(0,16,0) * Scale), XMatrix.TransformFVector(FVector(0,8,8) * Scale), *YColor );
			PRI->DrawLine( XMatrix.TransformFVector(FVector(0,8,8) * Scale), XMatrix.TransformFVector(FVector(0,0,16) * Scale), *ZColor );
		PRI->SetHitProxy( NULL );
	}
}

// Converts mouse movement on the screen to widget axis movement/rotation

void FWidget::ConvertMouseMovementToAxisMovement( FEditorLevelViewportClient* InViewportClient, const FVector& InLocation, const FVector& InDiff, FVector& InDrag, FRotator& InRotation, FVector& InScale )
{
	FSceneView	View;
	InViewportClient->CalcSceneView(&View);
	FPlane Wk;
	FPlane AxisEnd;
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

	FVector AxisDir = AxisEnd - Origin;
	AxisDir.Normalize();

	FVector DragDir = FVector( Diff.X, Diff.Y, Diff.Z );
	DragDir.Normalize();

	// The dot product of those two vectors tells us if the user is dragging with or against the axis direction

	FLOAT Dot = AxisDir | DragDir;
	Dot = fabs(Dot);

	// Use the most dominant axis the mouse is being dragged along

	INT idx = 0;
	if( fabs(Diff.X) < fabs(Diff.Y) )
		idx = 1;

	FLOAT Val = Diff[idx];

	// If the axis dir is negative, it is pointing in the negative screen direction.  In this situation, the mouse
	// drag must be inverted so that you are still dragging in the right logical direction.
	//
	// For example, if the X axis is pointing left and you drag left, this will ensure that the widget moves left.

	if( AxisDir[idx] < 0 )
		Val *= -1;

	// Honor INI option to invert Z axis movement on the widget

	if( idx == 1 && (CurrentAxis&AXIS_Z) && GEditor->InvertwidgetZAxis )
	{
		Val *= -1;
	}

	FMatrix InputCoordSystem = GEditorModeTools.GetCustomInputCoordinateSystem();

	switch( GEditorModeTools.GetWidgetMode() )
	{
		case WM_Translate:
			switch( CurrentAxis )
			{
				case AXIS_X:	InDrag = FVector( Val, 0, 0 );	break;
				case AXIS_Y:	InDrag = FVector( 0, Val, 0 );	break;
				case AXIS_Z:	InDrag = FVector( 0, 0, -Val );	break;
				case AXIS_XY:	InDrag = ( InDiff.X != 0 ? FVector( Val, 0, 0 ) : FVector( 0, Val, 0 ) );	break;
				case AXIS_XZ:	InDrag = ( InDiff.X != 0 ? FVector( Val, 0, 0 ) : FVector( 0, 0, -Val ) );	break;
				case AXIS_YZ:	InDrag = ( InDiff.X != 0 ? FVector( 0, Val, 0 ) : FVector( 0, 0, -Val ) );	break;
			}

			InDrag = InputCoordSystem.TransformFVector( InDrag );
			break;

		case WM_Rotate:
			{
				FVector Axis;
				switch( CurrentAxis )
				{
					case AXIS_X:	Axis = FVector( 1, 0, 0 );	break;
					case AXIS_Y:	Axis = FVector( 0, 1, 0 );	break;
					case AXIS_Z:	Axis = FVector( 0, 0, 1 );	break;
				}

				Axis = CustomCoordSystem.TransformNormal( Axis );

				FLOAT RotationSpeed = (2.f*(FLOAT)PI)/65535.f;
				FQuat DeltaQ = FQuat(Axis, Val*RotationSpeed);
				
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

		default:
			break;
	}

}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
