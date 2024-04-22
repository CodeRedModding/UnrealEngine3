/*=============================================================================
	UnLinkedObjDrawUtils.cpp: Utils for drawing linked objects.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnLinkedObjDrawUtils.h"
#include "Localization.h"

/** Minimum viewport zoom at which text will be rendered. */
static const FLOAT	TextMinZoom(0.3f);

/** Minimum viewport zoom at which arrowheads will be rendered. */
static const FLOAT	ArrowheadMinZoom(0.3f);

/** Minimum viewport zoom at which connectors will be rendered. */
static const FLOAT	ConnectorMinZoom(0.2f);

/** Minimum viewport zoom at which connectors will be rendered. */
static const FLOAT	SliderMinZoom(0.2f);

static const FLOAT	MaxPixelsPerStep(15.f);

static const INT	ArrowheadLength(14);
static const INT	ArrowheadWidth(4);

static const FColor SliderHandleColor(0, 0, 0);

static const FLinearColor DisabledColor(.127f,.127f,.127f);
static const FLinearColor DisabledTextColor(FLinearColor::Gray);

/** Font used by Canvas-based editors */
UFont* FLinkedObjDrawUtils::NormalFont = NULL;

/** Set the font used by Canvas-based editors */
void FLinkedObjDrawUtils::InitFonts( UFont* InNormalFont )
{
	NormalFont = InNormalFont;
}

/** Get the font used by Canvas-based editor */
UFont* FLinkedObjDrawUtils::GetFont()
{
	return NormalFont;
}

void FLinkedObjDrawUtils::DrawNGon(FCanvas* Canvas, const FVector2D& Center, const FColor& Color, INT NumSides, FLOAT Radius)
{
	if ( AABBLiesWithinViewport( Canvas, Center.X-Radius, Center.Y-Radius, Radius*2, Radius*2) )
	{
		FVector2D Verts[256];
		NumSides = Clamp(NumSides, 3, 255);

		for(INT i=0; i<NumSides+1; i++)
		{
			const FLOAT Angle = (2 * (FLOAT)PI) * (FLOAT)i/(FLOAT)NumSides;
			Verts[i] = Center + FVector2D( Radius*appCos(Angle), Radius*appSin(Angle) );
		}

	    for(INT i=0; i<NumSides; i++)
	    {
		    DrawTriangle2D(
			    Canvas,
			    FVector2D(Center), FVector2D(0,0),
			    FVector2D(Verts[i+0]), FVector2D(0,0),
			    FVector2D(Verts[i+1]), FVector2D(0,0),
			    Color
			    );
	    }
	}
}

/**
 *	@param EndDir End tangent. Note that this points in the same direction as StartDir ie 'along' the curve. So if you are dealing with 'handles' you will need to invert when you pass it in.
 *
 */
void FLinkedObjDrawUtils::DrawSpline(FCanvas* Canvas, const FIntPoint& Start, const FVector2D& StartDir, const FIntPoint& End, const FVector2D& EndDir, const FColor& LineColor, UBOOL bArrowhead, UBOOL bInterpolateArrowDirection/*=FALSE*/)
{
	const INT MinX = Min( Start.X, End.X );
	const INT MaxX = Max( Start.X, End.X );
	const INT MinY = Min( Start.Y, End.Y );
	const INT MaxY = Max( Start.Y, End.Y );

	if ( AABBLiesWithinViewport( Canvas, MinX, MinY, MaxX - MinX, MaxY - MinY ) )
	{
		// Don't draw the arrowhead if the editor is zoomed out most of the way.
		const FLOAT Zoom2D = GetUniformScaleFromMatrix(Canvas->GetTransform());
		if ( Zoom2D < ArrowheadMinZoom )
		{
			bArrowhead = FALSE;
		}

		const FVector2D StartVec( Start );
		const FVector2D EndVec( End );

		// Rough estimate of length of curve. Use direct length and distance between 'handles'. Sure we can do better.
		const FLOAT DirectLength = (EndVec - StartVec).Size();
		const FLOAT HandleLength = ((EndVec - EndDir) - (StartVec + StartDir)).Size();
		
		const INT NumSteps = appCeil(Max(DirectLength,HandleLength)/MaxPixelsPerStep);

		FVector2D OldPos = StartVec;

		for(INT i=0; i<NumSteps; i++)
		{
			const FLOAT Alpha = ((FLOAT)i+1.f)/(FLOAT)NumSteps;
			const FVector2D NewPos = CubicInterp(StartVec, StartDir, EndVec, EndDir, Alpha);

			const FIntPoint OldIntPos = OldPos.IntPoint();
			const FIntPoint NewIntPos = NewPos.IntPoint();

			DrawLine2D( Canvas, OldIntPos, NewIntPos, LineColor );

			// If this is the last section, use its direction to draw the arrowhead.
			if( (i == NumSteps-1) && (i >= 2) && bArrowhead )
			{
				// Go back 3 steps to give us decent direction for arrowhead
				FVector2D ArrowStartPos;

				if(bInterpolateArrowDirection)
				{
					const FLOAT ArrowStartAlpha = ((FLOAT)i-2.f)/(FLOAT)NumSteps;
					ArrowStartPos = CubicInterp(StartVec, StartDir, EndVec, EndDir, ArrowStartAlpha);
				}
				else
				{
					ArrowStartPos = OldPos;
				}

				const FVector2D StepDir = (NewPos - ArrowStartPos).SafeNormal();
				DrawArrowhead( Canvas, NewIntPos, StepDir, LineColor );
			}

			OldPos = NewPos;
		}
	}
}

void FLinkedObjDrawUtils::DrawArrowhead(FCanvas* Canvas, const FIntPoint& Pos, const FVector2D& Dir, const FColor& Color)
{
	// Don't draw the arrowhead if the editor is zoomed out most of the way.
	const FLOAT Zoom2D = GetUniformScaleFromMatrix(Canvas->GetTransform());
	if ( Zoom2D > ArrowheadMinZoom )
	{
		const FVector2D Orth(Dir.Y, -Dir.X);
		const FVector2D PosVec(Pos);
		const FVector2D pt2 = PosVec - (Dir * ArrowheadLength) - (Orth * ArrowheadWidth);
		const FVector2D pt1 = PosVec;
		const FVector2D pt3 = PosVec - (Dir * ArrowheadLength) + (Orth * ArrowheadWidth);
		DrawTriangle2D(Canvas,
		    pt1,FVector2D(0,0),
		    pt2,FVector2D(0,0),
		    pt3,FVector2D(0,0),
		    Color,NULL,0);
	}
}


FIntPoint FLinkedObjDrawUtils::GetTitleBarSize(FCanvas* Canvas, const TCHAR* Name)
{
	INT XL, YL;
	StringSize( NormalFont, XL, YL, Name );

	const INT LabelWidth = XL + (LO_TEXT_BORDER*2) + 4;

	return FIntPoint( Max(LabelWidth, LO_MIN_SHAPE_SIZE), LO_CAPTION_HEIGHT );
}

FIntPoint FLinkedObjDrawUtils::GetCommentBarSize(FCanvas* Canvas, const TCHAR* Comment)
{
	INT XL, YL;
	StringSize( GEngine->TinyFont, XL, YL, Comment );

	const INT LabelWidth = XL + (LO_TEXT_BORDER*2) + 4;

	return FIntPoint( Max(LabelWidth, LO_MIN_SHAPE_SIZE), YL + 4 );
}

INT FLinkedObjDrawUtils::DrawTitleBar(FCanvas* Canvas, const FIntPoint& Pos, const FIntPoint& Size, const FColor& FontColor, const FColor& BorderColor, const FColor& BkgColor, const TCHAR* Name, const TArray<FString>& Comments, INT BorderWidth /*= 0*/)
{
	// Draw label at top
	if ( AABBLiesWithinViewport( Canvas, Pos.X, Pos.Y, Size.X, Size.Y ) )
	{
		INT DBorderWidth = BorderWidth * 2;
		DrawTile( Canvas, Pos.X-BorderWidth,	Pos.Y-BorderWidth,	Size.X+DBorderWidth,	Size.Y+DBorderWidth,	0.0f,0.0f,0.0f,0.0f, BorderColor );
		DrawTile( Canvas, Pos.X+1,				Pos.Y+1,			Size.X-2,				Size.Y-2,				0.0f,0.0f,0.0f,0.0f, BkgColor );
	}

	if ( Name )
	{
		INT XL, YL;
		StringSize( NormalFont, XL, YL, Name );

		const FIntPoint StringPos( Pos.X+((Size.X-XL)/2), Pos.Y+((Size.Y-YL)/2)+1 );
		if ( AABBLiesWithinViewport( Canvas, StringPos.X, StringPos.Y, XL, YL ) )
		{
			DrawShadowedString( Canvas, StringPos.X, StringPos.Y, Name, NormalFont, FontColor );
		}
	}

	return DrawComments(Canvas, Pos, Size, Comments, GEngine->SmallFont);
}

INT FLinkedObjDrawUtils::DrawComments(FCanvas* Canvas, const FIntPoint& Pos, const FIntPoint& Size, const TArray<FString>& Comments, UFont* Font)
{
	const FLOAT Zoom2D = GetUniformScaleFromMatrix(Canvas->GetTransform());
	INT CommentY = Pos.Y - 2;

	// Handle multiline comments
	if( !Canvas->IsHitTesting() && Comments.Num() > 0 )
	{
		for (INT CommentIdx = Comments.Num() - 1; CommentIdx >= 0; --CommentIdx)
		{
			INT XL, YL;
			StringSize( Font, XL, YL, *Comments(CommentIdx) );
			CommentY -= YL;

			const FIntPoint StringPos( Pos.X + 2, CommentY );
			if ( AABBLiesWithinViewport( Canvas, StringPos.X, StringPos.Y, XL, YL ) )
			{
				DrawString( Canvas, StringPos.X, StringPos.Y, *Comments(CommentIdx), Font, FColor(0,0,0) );
				if( Zoom2D > 1.f - DELTA )
				{
					DrawString( Canvas, StringPos.X+1, StringPos.Y, *Comments(CommentIdx), Font, FColor(120,120,255) );
				}
			}
			CommentY -= 2;
		}
	}
	return CommentY;
}


// The InputY and OuputY are optional extra outputs, giving the size of the input and output connectors

FIntPoint FLinkedObjDrawUtils::GetLogicConnectorsSize(const FLinkedObjDrawInfo& ObjInfo, INT* InputY, INT* OutputY)
{
	INT MaxInputDescX = 0;
	INT MaxInputDescY = 0;
	for(INT i=0; i<ObjInfo.Inputs.Num(); i++)
	{
		INT XL, YL;
		StringSize( NormalFont, XL, YL, *ObjInfo.Inputs(i).Name );

		MaxInputDescX = Max(XL, MaxInputDescX);

		if(i>0) MaxInputDescY += LO_DESC_Y_PADDING;
		MaxInputDescY += Max(YL, LO_CONNECTOR_WIDTH);
	}

	INT MaxOutputDescX = 0;
	INT MaxOutputDescY = 0;
	for(INT i=0; i<ObjInfo.Outputs.Num(); i++)
	{
		INT XL, YL;
		StringSize( NormalFont, XL, YL, *ObjInfo.Outputs(i).Name );

		MaxOutputDescX = Max(XL, MaxOutputDescX);

		if(i>0) MaxOutputDescY += LO_DESC_Y_PADDING;
		MaxOutputDescY += Max(YL, LO_CONNECTOR_WIDTH);
	}

	const INT NeededX = MaxInputDescX + MaxOutputDescX + LO_DESC_X_PADDING + (2*LO_TEXT_BORDER);
	const INT NeededY = Max( MaxInputDescY, MaxOutputDescY ) + (2*LO_TEXT_BORDER);

	if(InputY)
	{
		*InputY = MaxInputDescY + (2*LO_TEXT_BORDER);
	}

	if(OutputY)
	{
		*OutputY = MaxOutputDescY + (2*LO_TEXT_BORDER);
	}

	return FIntPoint(NeededX, NeededY);
}

// Helper strut for drawing moving connectors
struct FConnectorPlacementData
{
	/** The index into the original array where the connector resides. We need this because data can be sorted out of its original order */
	INT Index;

	/** Only used by variable event connectors since their connectors reside in a different array but still 
		Need to be sorted with the rest of the variable connectors */
	INT EventOffset;

	/** The position of the connector (X or Y axis depending on connector type) */
	INT Pos;
	
	/** The delta that is added on to the above position based on user movement */
	INT OverrideDelta;

	/** Spacing in both the X and Y direction for the connector string */
	FIntPoint Spacing;

	/** Whether or not the connector can be moved to the left */
	UBOOL bClampedMin;

	/** Whether or not the connector can be moved to the right */
	UBOOL bClampedMax;

	/** The type of connector we have */
	EConnectorHitProxyType ConnType;

	
	/** Constructor */
	FConnectorPlacementData()
		: Index(-1),
		  EventOffset(0),
		  Pos(0),
		  OverrideDelta(0),
		  Spacing(0,0),
		  bClampedMin(FALSE),
		  bClampedMax(FALSE),
		  ConnType(LOC_VARIABLE)
	{
	}
};

/**
 * Compare callback function for appQsort. Compares Connector X positions
 */
static INT ConnectorDataCompareX( const FConnectorPlacementData* A, const FConnectorPlacementData *B )
{
	// The midpoint positions of the connectors is what is compared.
	// Calculated as follows:
	//	 Pos.X		         Pos.X+Spacing.X/2	      Pos.X+Spacing.X
	//	   |-----------------------|-----------------------|
				
	const INT ConnPosA =  A->Pos + A->Spacing.X/2;
	const INT ConnPosB = B->Pos + B->Spacing.X/2;
	if( ConnPosA < ConnPosB )
	{
		return -1;
	}
	else if( ConnPosA > ConnPosB )
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/**
 * Compare callback function for appQsort. Compares Connector Y positions
 */
static INT ConnectorDataCompareY( const FConnectorPlacementData* A, const FConnectorPlacementData *B )
{
	// The midpoint positions of the connectors is what is compared.
	// Calculated as follows:
	//	 Pos.Y		         Pos.Y+Spacing.Y/2	      Pos.Y+Spacing.Y
	//	   |-----------------------|-----------------------|
				
	const INT ConnPosA =  A->Pos + A->Spacing.Y/2;
	const INT ConnPosB = B->Pos + B->Spacing.Y/2;
	if( ConnPosA < ConnPosB )
	{
		return -1;
	}
	else if( ConnPosA > ConnPosB )
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/** 
 * Adjusts connectors after they have been moved so that they do not overlap.
 * This is done by sorting the connectors based on their (possibly) overlapping positions.
 * Once they are sorted, we recompute their positions based on their sorted order so that they are spaced evenly and do not overlap but still maintain relative position based on their sorted order.
 *
 * @param PlacementData		An array of connector placement data to be adjusted
 * @param Pos			The position of the sequence object where these connectors reside
 * @param Size			The size of the sequence object where these connectors reside.
 * @param VarSpace		The amount of space we have to place connectors
 * @param AdjustY		Whether or not we are adjusting in the Y direction (since spacing is computed differently).
 */
static void AdjustForOverlap( TArray<FConnectorPlacementData>& PlacementData, const FIntPoint& Pos, const FIntPoint& Size, INT VarSpace, UBOOL bAdjustY )
{	
	// Copy the placement data into a new array so we maintain the original data's order
	TArray<FConnectorPlacementData> SortedData = PlacementData;
	// The position of the last connector (initialized to the center of the sequence object
	INT LastPos;
	// The spacing of the last connector
	INT LastSpacing = 0;
	// The size of the sequence object to check when centering connectors.
	INT CheckSize;	

	if( bAdjustY )
	{	
		// We are adjusting in the Y direction, compute the amount of space we have to place connectors
		const INT ConnectorRangeY	= Size.Y - 2*LO_TEXT_BORDER;
		// Calculate the center spot
		const INT CenterY		= Pos.Y + LO_TEXT_BORDER + ConnectorRangeY / 2;
		
		CheckSize = Size.Y;

		// Initialize the last connector spacing and position.
		LastSpacing = ConnectorRangeY/PlacementData.Num();
		LastPos = CenterY - (PlacementData.Num()-1) * LastSpacing / 2;

		// Sort the connectors based on their positions
		appQsort( SortedData.GetTypedData(), SortedData.Num(), sizeof(FConnectorPlacementData), (QSORT_COMPARE)ConnectorDataCompareY);
	}
	else
	{
		// Initialize last position
		LastPos = Pos.X;
		CheckSize = Size.X;

		// if the title is wider than needed for the variables
		if (VarSpace < CheckSize)
		{
			// then center the variables
			LastPos += (CheckSize - VarSpace)/2;
		}

		// Sort the connectors based on their positions
		appQsort( SortedData.GetTypedData(), SortedData.Num(), sizeof(FConnectorPlacementData), (QSORT_COMPARE)ConnectorDataCompareX);
	}

	// Now iterate through each connector in sorted position and place them in that order
	for( INT SortedIdx = 0; SortedIdx < SortedData.Num(); ++SortedIdx )
	{
		FConnectorPlacementData CurData = SortedData( SortedIdx );
		if( bAdjustY )
		{
			// Calculation of position done by using the position of the starting connector, 
			// offset by the current index of the connector and adding padding.
			// We only worry about the Y position of variable connectors since the X position is fixed
			const INT ConnectorPos = LastPos + SortedIdx * LastSpacing;
			CurData.OverrideDelta = ConnectorPos - Pos.Y;
			CurData.Pos = Pos.Y + CurData.OverrideDelta;

			// Lookup into the original data and change its positon and delta override
			// This is why we store the index on the PlacmentData struct
			// Delta override is relative to the sequence objects position. 
			FConnectorPlacementData& OriginalData = PlacementData( CurData.Index );
			OriginalData.Pos = CurData.Pos;
			OriginalData.OverrideDelta = SortedData(SortedIdx).OverrideDelta = ConnectorPos - Pos.Y;
		}
		else
		{
			// Calculation of position done by using the position of the previous connector and adding spacing for
			// the current connector's name plus some padding
			// We only worry about the X position of variable connectors since the Y position is fixed
			const INT ConnectorPos = LastPos + LastSpacing + (LO_DESC_X_PADDING * 2);
			
			CurData.OverrideDelta = ConnectorPos - Pos.X;
			CurData.Pos = Pos.X + CurData.OverrideDelta;

			// Lookup into the original data and change its positon and delta override
			// This is why we store the index on the PlacmentData struct
			// Delta override is relative to the sequence objects position. 
			FConnectorPlacementData& OriginalData = PlacementData( CurData.Index + CurData.EventOffset );
			OriginalData.Pos = CurData.Pos;
			OriginalData.OverrideDelta = CurData.OverrideDelta = ConnectorPos - Pos.X;
	
			// The new last pos and spacing is this connectors position and spacing
			LastPos = ConnectorPos;
			LastSpacing = SortedData(SortedIdx).Spacing.X;
		}
	
	}
}

/** 
 * Clamps a connectors position so it never crosses over the boundary of a sequence object.
 * @param PlacementData		The connector data to clamp.  
 * @param Min				The min edge of the sequence object
 * @param Max				The max edge of the sequence object
 */
static void ClampConnector( FConnectorPlacementData& PlacementData, INT MinEdge, INT MaxEdge )
{
	if( PlacementData.Pos <= MinEdge )
	{
		// The connector is too far past the min edge of the sequence object
		PlacementData.Pos = MinEdge+1;
		// The delta override is always 0 at the min edge of the sequence object, the 1 is so we arent actually on the edge of the object
		PlacementData.OverrideDelta = PlacementData.Pos - MinEdge;
		PlacementData.bClampedMin = TRUE;
		PlacementData.bClampedMax = FALSE;
	}
	else if( PlacementData.Pos >= MaxEdge )
	{
		// The connector is too far past the max edge of the sequence object
		PlacementData.Pos = MaxEdge-1;
		// the delta override is always the length of the sequence object
		PlacementData.OverrideDelta = PlacementData.Pos - MinEdge;
		PlacementData.bClampedMin = FALSE;
		PlacementData.bClampedMax = TRUE;
	}
	else
	{
		// Nothing needs to be clamped
		PlacementData.bClampedMin = FALSE;
		PlacementData.bClampedMax = FALSE;
	}
}

/**
 * Pre-Calculates input or output connector positions based on their current delta offsets.
 * We need all the positions of connectors upfront so we can sort them based on position later.
 *
 * @param OutPlacementData	An array of connector placement data that is poptulated
 * @param ObjInfo		Information about the sequence object were the output connectors reside
 * @param Pos			The position of the sequence object where these connectors reside
 * @param Size			The size of the sequence object where these connectors reside.
 */
static void PreCalculateLogicConnectorPositions( TArray< FConnectorPlacementData >& OutPlacementData, FLinkedObjDrawInfo& ObjInfo, UBOOL bCalculateInputs, const FIntPoint& Pos, const FIntPoint& Size )
{
	// Set up a reference to the connector data we are actually calculating
	TArray<FLinkedObjConnInfo>& Connectors = bCalculateInputs ? ObjInfo.Inputs : ObjInfo.Outputs;

	// The amount of space there is to draw output connectors
	const INT ConnectorRangeY	= Size.Y - 2*LO_TEXT_BORDER;
	// The center position of the draw area
	const INT CenterY			= Pos.Y + LO_TEXT_BORDER + ConnectorRangeY / 2;
	// The spacing that should exist between each connector
	const INT SpacingY	= ConnectorRangeY/Connectors.Num();
	// The location of the first connector
	const INT StartY	= CenterY - (Connectors.Num()-1) * SpacingY / 2;

	// Pre-calculate all connections.  We need to know the positions of each connector 
	// before they are drawn so we can position them correctly if one moved
	// We will adjust for overlapping connectors if need be.
	for( INT ConnectorIdx=0; ConnectorIdx< Connectors.Num(); ++ConnectorIdx )
	{
		FLinkedObjConnInfo& ConnectorInfo = Connectors(ConnectorIdx);

		if( ConnectorInfo.bNewConnection == TRUE && ConnectorInfo.bMoving == FALSE )
		{	
			// This is a new connection.  It should be positioned at the end of all the connectors
			const INT NewLoc = StartY + ConnectorIdx * SpacingY;

			// Find out the change in position from the sequence objects top edge. The top edge is used as zero for the Delta override )
			ConnectorInfo.OverrideDelta = NewLoc - Pos.Y;

			// Recalc all connector positions if one was added.
			// Store it on the ObjInfo struct so it can be passed back to the sequence object.
			// This way we can defer this recalculation.
			bCalculateInputs ? ObjInfo.bPendingInputConnectorRecalc = TRUE : ObjInfo.bPendingOutputConnectorRecalc = TRUE;
		}
		else if( ConnectorInfo.bMoving == TRUE )
		{
			// Recalc all connector positions if one was moved.
			// Store it on the ObjInfo struct so it can be passed back to the sequence object.
			// This way we can defer this recalculation until when we don't have a mover
			bCalculateInputs ? ObjInfo.bPendingInputConnectorRecalc = TRUE : ObjInfo.bPendingOutputConnectorRecalc = TRUE;
		}

		const INT DrawY = Pos.Y + ConnectorInfo.OverrideDelta;

		// Set up some placement data for the connector 
		// Not all the placement data is needed
		FConnectorPlacementData ConnData;
		// Index of the connector. This is used to lookup the correct index later because the call to AdjustForOverlap sorts the connectors by position
		ConnData.Index = ConnectorIdx;
		ConnData.ConnType = bCalculateInputs ? LOC_INPUT : LOC_OUTPUT;
		ConnData.Pos = DrawY;
		ConnData.OverrideDelta = ConnectorInfo.OverrideDelta;
		ConnData.Spacing = FIntPoint(0, SpacingY);

		if( ConnectorInfo.bMoving )
		{
			// Make sure moving connectors can not go past the bounds of the sequence object
 			ClampConnector( ConnData, Pos.Y, Pos.Y+Size.Y );
		}

		OutPlacementData.AddItem( ConnData );
	}

}

void FLinkedObjDrawUtils::DrawLogicConnectors(FCanvas* Canvas, FLinkedObjDrawInfo& ObjInfo, const FIntPoint& Pos, const FIntPoint& Size, const FLinearColor* ConnectorTileBackgroundColor)
{
	const UBOOL bHitTesting				= Canvas->IsHitTesting();
	const FLOAT Zoom2D = GetUniformScaleFromMatrix(Canvas->GetTransform());
	const UBOOL bSufficientlyZoomedIn	= Zoom2D > ConnectorMinZoom;

	INT XL, YL;
	StringSize(NormalFont, XL, YL, TEXT("GgIhy"));

	const INT ConnectorWidth	= Max(YL, LO_CONNECTOR_WIDTH);
	const INT ConnectorRangeY	= Size.Y - 2*LO_TEXT_BORDER;
	const INT CenterY			= Pos.Y + LO_TEXT_BORDER + ConnectorRangeY / 2;

	// Do nothing if no Input connectors
	if( ObjInfo.Inputs.Num() > 0 )
	{
		const INT SpacingY	= ConnectorRangeY/ObjInfo.Inputs.Num();
		const INT StartY	= CenterY - (ObjInfo.Inputs.Num()-1) * SpacingY / 2;
		ObjInfo.InputY.Add( ObjInfo.Inputs.Num() );

		for(INT i=0; i<ObjInfo.Inputs.Num(); i++)
		{
			const INT LinkY		= StartY + i * SpacingY;
			ObjInfo.InputY(i)	= LinkY;

			if ( bSufficientlyZoomedIn )
			{
				if(bHitTesting) Canvas->SetHitProxy( new HLinkedObjConnectorProxy(ObjInfo.ObjObject, LOC_INPUT, i) );
				FColor ConnectorColor = ObjInfo.Inputs(i).bEnabled ? ObjInfo.Inputs(i).Color : FColor(DisabledColor);
				DrawTile( Canvas, Pos.X - LO_CONNECTOR_LENGTH, LinkY - LO_CONNECTOR_WIDTH / 2, LO_CONNECTOR_LENGTH, LO_CONNECTOR_WIDTH, 0.f, 0.f, 0.f, 0.f, ConnectorColor );
				if(bHitTesting) Canvas->SetHitProxy( NULL );

				StringSize( NormalFont, XL, YL, *ObjInfo.Inputs(i).Name );
				const FIntPoint StringPos( Pos.X + LO_TEXT_BORDER, LinkY - YL / 2 );
				if ( AABBLiesWithinViewport( Canvas, StringPos.X, StringPos.Y, XL, YL ) ) 
				{
					if ( ConnectorTileBackgroundColor )
					{
						DrawTile( Canvas, StringPos.X, StringPos.Y, XL, YL, 0.f, 0.f, 0.f, 0.f, *ConnectorTileBackgroundColor, NULL, TRUE );
					}
					const FLinearColor& TextColor = ObjInfo.Inputs(i).bEnabled ? FLinearColor::White : DisabledTextColor;
					DrawShadowedString( Canvas, StringPos.X, StringPos.Y, *ObjInfo.Inputs(i).Name, NormalFont, TextColor );
				}
			}
		}
	}

	// Do nothing if no Output connectors
	if( ObjInfo.Outputs.Num() > 0 )
	{
		const INT SpacingY	= ConnectorRangeY/ObjInfo.Outputs.Num();
		const INT StartY	= CenterY - (ObjInfo.Outputs.Num()-1) * SpacingY / 2;
		ObjInfo.OutputY.Add( ObjInfo.Outputs.Num() );

		for(INT i=0; i<ObjInfo.Outputs.Num(); i++)
		{
			const INT LinkY		= StartY + i * SpacingY;
			ObjInfo.OutputY(i)	= LinkY;

			if ( bSufficientlyZoomedIn )
			{
				if(bHitTesting) Canvas->SetHitProxy( new HLinkedObjConnectorProxy(ObjInfo.ObjObject, LOC_OUTPUT, i) );
				FColor ConnectorColor = ObjInfo.Outputs(i).bEnabled ? ObjInfo.Outputs(i).Color : FColor(DisabledColor);
				DrawTile( Canvas, Pos.X + Size.X, LinkY - LO_CONNECTOR_WIDTH / 2, LO_CONNECTOR_LENGTH, LO_CONNECTOR_WIDTH, 0.f, 0.f, 0.f, 0.f, ConnectorColor );
				if(bHitTesting) Canvas->SetHitProxy( NULL );

				StringSize( NormalFont, XL, YL, *ObjInfo.Outputs(i).Name );
				const FIntPoint StringPos( Pos.X + Size.X - XL - LO_TEXT_BORDER, LinkY - YL / 2 );
				if ( AABBLiesWithinViewport( Canvas, StringPos.X, StringPos.Y, XL, YL ) )
				{
					if ( ConnectorTileBackgroundColor )
					{
						DrawTile( Canvas, StringPos.X, StringPos.Y, XL, YL, 0.f, 0.f, 0.f, 0.f, *ConnectorTileBackgroundColor, NULL, TRUE );
					}
					const FLinearColor& TextColor = ObjInfo.Outputs(i).bEnabled ? FLinearColor::White : DisabledTextColor;
					DrawShadowedString( Canvas, StringPos.X, StringPos.Y, *ObjInfo.Outputs(i).Name, NormalFont, TextColor );
				}
			}
		}
	}
}

/** 
 * Draws logic connectors on the sequence object with adjustments for moving connectors.
 * 
 * @param Canvas	The canvas to draw on
 * @param ObjInfo	Information about the sequence object so we know how to draw the connectors
 * @param Pos		The positon of the sequence object
 * @param Size		The size of the sequence object
 * @param ConnectorTileBackgroundColor		(Optional)Color to apply to a connector tiles bacground
 * @param bHaveMovingInput			(Optional)Whether or not we have a moving input connector
 * @param bHaveMovingOutput			(Optional)Whether or not we have a moving output connector
 * @param bGhostNonMoving			(Optional)Whether or not we should ghost all non moving connectors while one is moving
 */
void FLinkedObjDrawUtils::DrawLogicConnectorsWithMoving(FCanvas* Canvas, FLinkedObjDrawInfo& ObjInfo, const FIntPoint& Pos, const FIntPoint& Size, const FLinearColor* ConnectorTileBackgroundColor, const UBOOL bHaveMovingInput, const UBOOL bHaveMovingOutput, const UBOOL bGhostNonMoving )
{
	const UBOOL bHitTesting				= Canvas->IsHitTesting();
	const FLOAT Zoom2D = GetUniformScaleFromMatrix(Canvas->GetTransform());
	const UBOOL bSufficientlyZoomedIn	= Zoom2D > ConnectorMinZoom;

	INT XL, YL;
	StringSize(NormalFont, XL, YL, TEXT("GgIhy"));

	const INT ConnectorWidth	= Max(YL, LO_CONNECTOR_WIDTH);
	const INT ConnectorRangeY	= Size.Y - 2*LO_TEXT_BORDER;
	const INT CenterY			= Pos.Y + LO_TEXT_BORDER + ConnectorRangeY / 2;

	const FLinearColor GhostedConnectorColor(.2f, .2f, .2f);
	const FLinearColor GhostedTextColor(.6f, .6f, .6f);

	// Do nothing if no Input connectors
	if( ObjInfo.Inputs.Num() > 0 )
	{
		// Return as many Y positions as we have connectors
		ObjInfo.InputY.Add( ObjInfo.Inputs.Num() );

		// Pre-calculate all positions.
		// Every connector needs to have a known position so we can sort their locations and properly place movers.
		TArray< FConnectorPlacementData > PlacementData;
		PreCalculateLogicConnectorPositions( PlacementData, ObjInfo, TRUE, Pos, Size );

		if( !bHaveMovingInput && ObjInfo.bPendingInputConnectorRecalc == TRUE )
		{
			// If we don't have a moving connector and we need to recalc connector positions do it now
			AdjustForOverlap( PlacementData, Pos, Size, 0, TRUE);
			ObjInfo.bPendingInputConnectorRecalc = FALSE;
		}

		// Now actually do the drawing
		for(INT InputIdx=0; InputIdx < ObjInfo.Inputs.Num(); ++InputIdx )
		{
			FLinkedObjConnInfo& InputInfo = ObjInfo.Inputs(InputIdx);
			// Get the placement data from the same index as the variable
			const FConnectorPlacementData& CurData = PlacementData(InputIdx);

			const INT DrawY = CurData.Pos;
			ObjInfo.InputY(InputIdx) = DrawY;
			// pass back the important data to the connector so it can be saved per draw call
			InputInfo.OverrideDelta = CurData.OverrideDelta;
			InputInfo.bClampedMax = CurData.bClampedMax;
			InputInfo.bClampedMin = CurData.bClampedMin;

			if ( bSufficientlyZoomedIn )
			{
				FLinearColor ConnColor = InputInfo.Color;
				FLinearColor TextColor = FLinearColor::White;

				if( bGhostNonMoving && InputInfo.bMoving == FALSE )
				{
					// The color of non-moving connectors and their names should be ghosted if we are moving connectors
					ConnColor = GhostedConnectorColor;
					TextColor = GhostedTextColor;
				}


				INT ConnectorWidth(LO_CONNECTOR_WIDTH);
				INT ConnectorLength(LO_CONNECTOR_LENGTH);
				INT NewX(Pos.X);
				INT NewY(DrawY);
				if(bHitTesting)
				{
					Canvas->SetHitProxy( new HLinkedObjConnectorProxy(ObjInfo.ObjObject, LOC_INPUT, InputIdx) );
					ConnectorWidth += LO_EXTRA_HIT_WIDTH;
					ConnectorLength += LO_EXTRA_HIT_LENGTH;
					NewX -= LO_EXTRA_HIT_X;
					NewY -= LO_EXTRA_HIT_Y;
				}
		
				DrawTile( Canvas, NewX - ConnectorLength, NewY - ConnectorWidth / 2, ConnectorLength, ConnectorWidth, 0.f, 0.f, 0.f, 0.f, ConnColor);

				if(bHitTesting) 
				{
					Canvas->SetHitProxy( NULL );
				}

				StringSize( NormalFont, XL, YL, *InputInfo.Name );
				const FIntPoint StringPos( Pos.X + LO_TEXT_BORDER, DrawY - YL / 2 );
				if ( AABBLiesWithinViewport( Canvas, StringPos.X, StringPos.Y, XL, YL ) ) 
				{
					if ( ConnectorTileBackgroundColor )
					{
						FLinearColor BgColor = *ConnectorTileBackgroundColor;
						if( bGhostNonMoving )
						{
							// All non moving connectors should be ghosted
							BgColor = GhostedConnectorColor;
						}

						DrawTile( Canvas, StringPos.X, StringPos.Y, XL, YL, 0.f, 0.f, 0.f, 0.f, BgColor, NULL, TRUE );
					}

					if( bGhostNonMoving && !InputInfo.bMoving )
					{
						DrawString( Canvas, StringPos.X, StringPos.Y, *InputInfo.Name, NormalFont, TextColor );
					}
					else
					{
						DrawShadowedString( Canvas, StringPos.X, StringPos.Y, *InputInfo.Name, NormalFont, TextColor );
					}
				}
			}
		}
	}

	// Do nothing if no Output connectors
	if( ObjInfo.Outputs.Num() > 0 )
	{
		ObjInfo.OutputY.Add( ObjInfo.Outputs.Num() );
		// Pre-calculate all positions.
		// Every connector needs to have a known position so we can sort their locations and properly place movers.
		TArray< FConnectorPlacementData > PlacementData;
		PreCalculateLogicConnectorPositions( PlacementData, ObjInfo, FALSE, Pos, Size );

		if( !bHaveMovingOutput && ObjInfo.bPendingOutputConnectorRecalc == TRUE )
		{
			// If we don't have a moving connector and we need to recalc connector positions do it now
			AdjustForOverlap( PlacementData, Pos, Size, 0, TRUE);
			ObjInfo.bPendingOutputConnectorRecalc = FALSE;
		}

		// Now actually do the drawing
		for( INT OutputIdx=0; OutputIdx<ObjInfo.Outputs.Num(); ++OutputIdx )
		{
			FLinkedObjConnInfo& OutputInfo = ObjInfo.Outputs(OutputIdx);
			// Get the placement data from the same index as the variable
			const FConnectorPlacementData& CurData = PlacementData(OutputIdx);

			const INT DrawY = CurData.Pos;
			ObjInfo.OutputY(OutputIdx) = DrawY;
			// pass back the important data to the connector so it can be saved per draw call
			OutputInfo.OverrideDelta = CurData.OverrideDelta;
			OutputInfo.bClampedMax = CurData.bClampedMax;
			OutputInfo.bClampedMin = CurData.bClampedMin;
		
			if ( bSufficientlyZoomedIn )
			{
				FLinearColor ConnColor = OutputInfo.Color;
				FLinearColor TextColor = FLinearColor::White;
				
				if( bGhostNonMoving && OutputInfo.bMoving == FALSE )
				{
					// The color of non-moving connectors and their names should be ghosted if we are moving connectors
					ConnColor = GhostedConnectorColor;
					TextColor = GhostedTextColor;
				}

				INT ConnectorWidth(LO_CONNECTOR_WIDTH);
				INT ConnectorLength(LO_CONNECTOR_LENGTH);
				INT NewX(Pos.X);
				INT NewY(DrawY);
				if(bHitTesting)
				{
					Canvas->SetHitProxy( new HLinkedObjConnectorProxy(ObjInfo.ObjObject, LOC_OUTPUT, OutputIdx) );
					ConnectorWidth += LO_EXTRA_HIT_WIDTH;
					ConnectorLength += LO_EXTRA_HIT_LENGTH;
					NewX -= LO_EXTRA_HIT_X;
					NewY -= LO_EXTRA_HIT_Y;
				}

				// Draw the connector tile
				DrawTile( Canvas, NewX + Size.X, NewY - ConnectorWidth / 2, ConnectorLength, ConnectorWidth, 0.f, 0.f, 0.f, 0.f, ConnColor);
				if( bHitTesting )
				{
					Canvas->SetHitProxy( NULL );
				}

				StringSize( NormalFont, XL, YL, *OutputInfo.Name );
				const FIntPoint StringPos( Pos.X + Size.X - XL - LO_TEXT_BORDER, DrawY - YL / 2 );
				if ( AABBLiesWithinViewport( Canvas, StringPos.X, StringPos.Y, XL, YL ) )
				{
					if ( ConnectorTileBackgroundColor )
					{
						FLinearColor BgColor = *ConnectorTileBackgroundColor;
						if( bGhostNonMoving && OutputInfo.bMoving == FALSE )
						{
							BgColor = GhostedConnectorColor;
						}
						DrawTile( Canvas, StringPos.X, StringPos.Y, XL, YL, 0.f, 0.f, 0.f, 0.f, BgColor, NULL, TRUE );
					}

					// Draw the connector name
					if( bGhostNonMoving && !OutputInfo.bMoving )
					{
						DrawString( Canvas, StringPos.X, StringPos.Y, *OutputInfo.Name, NormalFont, TextColor );
					}
					else
					{
						DrawShadowedString( Canvas, StringPos.X, StringPos.Y, *OutputInfo.Name, NormalFont, TextColor );
					}
				}
			}
		}
	}
}

/**
 * Special version of string size that handles unique wrapping of variable names.
 */
static UBOOL VarStringSize(UFont *Font, INT &XL, INT &YL, FString Text, FString *LeftSplit = NULL, INT *LeftXL = NULL, FString *RightSplit = NULL, INT *RightXL = NULL)
{
	if (Text.Len() >= 4)
	{
		// walk through the string and find the wrap point (skip the first few chars since wrapping early would be pointless)
		for (INT Idx = 4; Idx < Text.Len(); Idx++)
		{
			TCHAR TextChar = Text[Idx];
			if (TextChar == ' ' || appIsUpper(TextChar))
			{
				// found wrap point, find the size of the first string
				FString FirstPart = Text.Left(Idx);
				FString SecondPart = Text.Right(Text.Len() - Idx);
				INT FirstXL, FirstYL, SecondXL, SecondYL;
				StringSize(Font, FirstXL, FirstYL, *FirstPart);
				StringSize(Font, SecondXL, SecondYL, *SecondPart);
				XL = Max(FirstXL, SecondXL);
				YL = FirstYL + SecondYL;
				if (LeftSplit != NULL)
				{
					*LeftSplit = FirstPart;
				}
				if (LeftXL != NULL)
				{
					*LeftXL = FirstXL;
				}
				if (RightSplit != NULL)
				{
					*RightSplit = SecondPart;
				}
				if (RightXL != NULL)
				{
					*RightXL = SecondXL;
				}
				return TRUE;
			}
		}
	}
	// no wrap, normal size
	StringSize(Font, XL, YL,*Text);
	return FALSE;
}

FIntPoint FLinkedObjDrawUtils::GetVariableConnectorsSize(FCanvas* Canvas, const FLinkedObjDrawInfo& ObjInfo)
{
	// sum the length of all the var/event names and add extra padding
	INT TotalXL = 0, MaxYL = 0;
	for (INT Idx = 0; Idx < ObjInfo.Variables.Num(); Idx++)
	{
		INT XL, YL;
		VarStringSize( NormalFont, XL, YL, ObjInfo.Variables(Idx).Name );
		TotalXL += XL;
		MaxYL = Max(MaxYL,YL);
	}
	for (INT Idx = 0; Idx < ObjInfo.Events.Num(); Idx++)
	{
		INT XL, YL;
		VarStringSize( NormalFont, XL, YL, ObjInfo.Events(Idx).Name );
		TotalXL += XL;
		MaxYL = Max(MaxYL,YL);
	}
	// add the final padding based on number of connectors
	TotalXL += (2 * LO_DESC_X_PADDING) * (ObjInfo.Variables.Num() + ObjInfo.Events.Num()) + (2 * LO_DESC_X_PADDING);
	return FIntPoint(TotalXL,MaxYL);
}

/**
 * Pre-Calculates variable connector positions based on their current delta offsets.
 * We need all the positions of connectors upfront so we can sort them based on position later.
 *
 * @param OutPlacementData	An array of connector placement data that is poptulated
 * @param ObjInfo		Information about the sequence object were the output connectors reside
 * @param Pos			The position of the sequence object where these connectors reside
 * @param Size			The size of the sequence object where these connectors reside.
 * @param InFont        Font to use
 */
static void PreCalculateVariableConnectorPositions( TArray< FConnectorPlacementData >& OutPlacementData, FLinkedObjDrawInfo& ObjInfo, const FIntPoint& Pos, const FIntPoint& Size, const INT VarWidth, UFont* InFont )
{
	// The position of the last connector
	INT LastPos = Pos.X;
	// The spacing of the last connector
	INT LastSpacing = 0;

	// if the title is wider than needed for the variables
	if (VarWidth < Size.X)
	{
		// then center the variables
		LastPos += (Size.X - VarWidth)/2;
	}

	// String split and spacing info
	FString LeftSplit, RightSplit;
	INT LeftXSpacing, RightXSpacing;

	// Pre-calculate all connections.  We need to know the positions of each connector 
	// before they are drawn so we can position them correctly if one moved
	// We will adjust for overlapping connectors if need be.
	// 
	// Calculation is done by using the position of the previous connector and adding spacing for
	// the current connector's name plus some padding
	// We only worry about the X position of variable connectors since the Y position is fixed
	for (INT Idx = 0; Idx < ObjInfo.Variables.Num(); Idx++)
	{
		// The current variable connector info
		FLinkedObjConnInfo& VarInfo = ObjInfo.Variables(Idx);

		// Figure out how much space the name of the connector takes up takes up.
		INT XL, YL;
		UBOOL bSplit = VarStringSize( InFont, XL, YL, VarInfo.Name, &LeftSplit, &LeftXSpacing, &RightSplit, &RightXSpacing );

		// Set up some placement data for the connector 
		FConnectorPlacementData ConnData; 

		if( VarInfo.bNewConnection == TRUE && VarInfo.bMoving == FALSE )
		{
			// This is a new connection.  It should be positioned at the end of all the connectors
			const INT NewLoc = LastPos + LastSpacing + (LO_DESC_X_PADDING * 2);
			
			// Find out the change in position from the sequence objects left edge. The left edge is used as zero for the Delta override )
			VarInfo.OverrideDelta = NewLoc - Pos.X;

			// Recalc all connector positions if one was added.
			// Store it on the ObjInfo struct so it can be passed back to the sequence object.
			// This way we can defer this recalculation.
			ObjInfo.bPendingVarConnectorRecalc = TRUE;
		}
		else if( VarInfo.bMoving == TRUE )
		{
			// Recalc all connector positions if one was moved.
			// Store it on the ObjInfo struct so it can be passed back to the sequence object.
			// This way we can defer this recalculation until when we don't have a mover
			ObjInfo.bPendingVarConnectorRecalc = TRUE;
		}

		// The new location to draw
		INT DrawX = Pos.X + VarInfo.OverrideDelta;
		
		// Setup placement data for this connector.
		// Index of the connector. This is used to lookup the correct index later because the call to AdjustForOverlap sorts the connectors by position
		ConnData.Index = Idx; 
		ConnData.ConnType = LOC_VARIABLE;
		ConnData.Pos = DrawX;
		ConnData.OverrideDelta = VarInfo.OverrideDelta;
		ConnData.Spacing = FIntPoint(XL,YL);

		if( VarInfo.bMoving )
		{
			// Make sure the connector doesn't go over the edge of the sequence object
			ClampConnector( ConnData, Pos.X, Pos.X + Size.X - XL );
		}

		OutPlacementData.AddItem(ConnData);
		// Update the position and spacing for the next connector
		LastPos = DrawX;
		LastSpacing = XL;
	}

	// calculate event connectors.
	for (INT Idx = 0; Idx < ObjInfo.Events.Num(); Idx++)
	{
		// The current variable connector info
		FLinkedObjConnInfo& EventInfo = ObjInfo.Events(Idx);

		// Figure out how much space the name of the connector takes up takes up.
		INT XL, YL;
		VarStringSize( InFont, XL, YL, EventInfo.Name );

		// Set up some placement data for the connector 
		FConnectorPlacementData ConnData; 

		if( EventInfo.bNewConnection == TRUE && EventInfo.bMoving == FALSE )
		{
			// This is a new connection.  It should be positioned at the end of all the connectors
			const INT NewLoc = LastPos + LastSpacing + (LO_DESC_X_PADDING * 2);
			
			// Find out the change in position from the sequence objects left edge. The left edge is used as zero for the Delta override )
			EventInfo.OverrideDelta = NewLoc - Pos.X;

			// Recalc all connector positions if one was added.
			// Store it on the ObjInfo struct so it can be passed back to the sequence object.
			// This way we can defer this recalculation.
			ObjInfo.bPendingVarConnectorRecalc = TRUE;
		}
		else if( EventInfo.bMoving == TRUE )
		{
			// Recalc all connector positions if one was moved.
			// Store it on the ObjInfo struct so it can be passed back to the sequence object.
			// This way we can defer this recalculation until when we don't have a mover
			ObjInfo.bPendingVarConnectorRecalc = TRUE;
		}

		// The new location to draw
		INT DrawX = Pos.X + EventInfo.OverrideDelta;
		
	
		// Index of the connector. This is used to lookup the correct index later because the call to AdjustForOverlap sorts the connectors by position
		ConnData.Index = Idx; 
		ConnData.ConnType = LOC_EVENT;
		// This is index offset we need to add when looking up the original index of this connector
		// This is needed since event variable connectors reside in a different array but must be sorted together with variable connectors.
		// A variable and event connector can have the same index since they are in different arrays, which is why we need the offset.
		ConnData.EventOffset = ObjInfo.Variables.Num();
		ConnData.Pos = DrawX;
		ConnData.OverrideDelta = EventInfo.OverrideDelta;
		ConnData.Spacing = FIntPoint(XL,YL);

		if( EventInfo.bMoving )
		{
			// Make sure the connector doesn't go over the edge of the sequence object
			ClampConnector( ConnData, Pos.X, Pos.X + Size.X - XL );
		}

		OutPlacementData.AddItem(ConnData);
		// Update the position and spacing for the next connector
		LastPos = DrawX;
		LastSpacing = XL;
	}
}

/** 
 * Draws variable connectors on the sequence object with adjustments for moving connectors
 * 
 * @param Canvas	The canvas to draw on
 * @param ObjInfo	Information about the sequence object so we know how to draw the connectors
 * @param Pos		The positon of the sequence object
 * @param Size		The size of the sequence object
 * @param VarWidth	The width of space we have to draw connectors
 * @param bHaveMovingVariable			(Optional)Whether or not we have a moving variable connector
 * @param bGhostNonMoving			(Optional)Whether or not we should ghost all non moving connectors while one is moving
 */
void FLinkedObjDrawUtils::DrawVariableConnectorsWithMoving(FCanvas* Canvas, FLinkedObjDrawInfo& ObjInfo, const FIntPoint& Pos, const FIntPoint& Size, const INT VarWidth, const UBOOL bHaveMovingVariable, const UBOOL bGhostNonMoving )
{
	// Do nothing here if no variables or event connectors.
	if( ObjInfo.Variables.Num() == 0 && ObjInfo.Events.Num() == 0 )
	{
		return;
	}

	const FLinearColor GhostedConnectorColor(.2f, .2f, .2f);
	const FLinearColor GhostedTextColor(.6f, .6f, .6f);

	const FLOAT Zoom2D = GetUniformScaleFromMatrix(Canvas->GetTransform());
	const UBOOL bHitTesting = Canvas->IsHitTesting();
	const UBOOL bSufficientlyZoomedIn = Zoom2D > ConnectorMinZoom;
	const INT LabelY = Pos.Y - LO_TEXT_BORDER;
	
	// Return as many x locations as there are variables and events
	ObjInfo.VariableX.Add( ObjInfo.Variables.Num() );
	ObjInfo.EventX.Add( ObjInfo.Events.Num() );

	// Initialize placement data
	TArray<FConnectorPlacementData> PlacementData;

	PreCalculateVariableConnectorPositions( PlacementData, ObjInfo, Pos, Size, VarWidth, NormalFont );

	if( !bHaveMovingVariable && ObjInfo.bPendingVarConnectorRecalc == TRUE )
	{
		// If we don't have a moving connector and we need to recalc connector positions do it now
		AdjustForOverlap( PlacementData, Pos, Size, VarWidth, FALSE);
		ObjInfo.bPendingVarConnectorRecalc = FALSE;
	}

	FString LeftSplit, RightSplit;
	INT LeftXSpacing=0,RightXSpacing=0;
	// Now actually do the drawing
	for (INT DataIdx = 0; DataIdx < PlacementData.Num(); ++DataIdx)
	{
		const FConnectorPlacementData& CurData = PlacementData( DataIdx );
		EConnectorHitProxyType ConnType = CurData.ConnType;
		// Get the variable data from the same index as the placement data
		FLinkedObjConnInfo& VarInfo = ( ConnType == LOC_VARIABLE ? ObjInfo.Variables( CurData.Index ) : ObjInfo.Events( CurData.Index ) );

		UBOOL bSplit = FALSE;
		if( ConnType == LOC_VARIABLE)
		{
			// Calculate the string size to get split string and spacing info
			INT XL, YL;
			bSplit = VarStringSize( NormalFont, XL, YL, VarInfo.Name, &LeftSplit, &LeftXSpacing, &RightSplit, &RightXSpacing );
		}

		// The x location of where to draw
		const INT DrawX = CurData.Pos;

		// Calculate this only once
		const INT HalfXL = CurData.Spacing.X/2;
		
		// Pass back this info to the sequence object so it can be stored and saved
		// We use the midpoint of the connector as the X location to pass back so the arrow of anything linked to the connector will show up in the middle
		if( ConnType == LOC_VARIABLE )
		{
			ObjInfo.VariableX( CurData.Index ) = DrawX + HalfXL; 
		}
		else 
		{
			ObjInfo.EventX( CurData.Index ) = DrawX + HalfXL;
		}

		VarInfo.OverrideDelta = CurData.OverrideDelta;
		VarInfo.bClampedMax = CurData.bClampedMax;
		VarInfo.bClampedMin = CurData.bClampedMin;

		// Only draw if we are zoomed in close enough
		if ( bSufficientlyZoomedIn )
		{
			// The connector color
			FLinearColor ConnColor = VarInfo.Color;
			// The color of the connector text
			FLinearColor TextColor = FLinearColor::White;
			if( bGhostNonMoving && VarInfo.bMoving == FALSE )
			{
				// If we are moving, gray everything out (unless we are the mover!)
				ConnColor = GhostedConnectorColor;
				TextColor = GhostedTextColor;
			}

			INT ConnectorWidth(LO_CONNECTOR_WIDTH);
			INT ConnectorLength(LO_CONNECTOR_LENGTH);
			INT NewX(DrawX);
			INT NewY(Pos.Y);
			// Set up hit proxy info
			if( bHitTesting )
			{
				Canvas->SetHitProxy( new HLinkedObjConnectorProxy(ObjInfo.ObjObject, ConnType, CurData.Index ) );
				ConnectorWidth += LO_EXTRA_HIT_WIDTH;
				ConnectorLength += LO_EXTRA_HIT_LENGTH;
				NewX -= LO_EXTRA_HIT_X;
				NewY -= LO_EXTRA_HIT_Y;
			}

			if (VarInfo.bOutput)
			{
				// Draw a triangle if this is an output variable
				FIntPoint Vert0, Vert1, Vert2;
				Vert0.X = -2 + NewX  + HalfXL - ConnectorWidth/2;
				Vert0.Y = NewY + Size.Y;
				Vert1.X = Vert0.X + ConnectorWidth + 2;
				Vert1.Y = Vert0.Y;
				Vert2.X = (Vert1.X + Vert0.X) / 2;
				Vert2.Y = Vert0.Y + ConnectorLength + 2;
				DrawTriangle2D(Canvas,Vert0,FVector2D(0,0),Vert1,FVector2D(0,0),Vert2,FVector2D(0,0),ConnColor);
			}
			else
			{
				// Draw the connector square
				DrawTile( Canvas, NewX + HalfXL - ConnectorWidth / 2, NewY+Size.Y, ConnectorWidth, ConnectorLength, 0.f, 0.f, 0.f, 0.f, ConnColor );
			}

			if(bHitTesting)
			{
				Canvas->SetHitProxy( NULL );
			}

			if ( AABBLiesWithinViewport( Canvas, DrawX, LabelY, CurData.Spacing.X, CurData.Spacing.Y ) )
			{
				//Draw strings for the connector name
				if ( bSplit )
				{
					if( bGhostNonMoving && !VarInfo.bMoving )
					{
						// Don't draw a shadowed string if this one is grayed out
						DrawString( Canvas, DrawX + HalfXL - RightXSpacing/2, LabelY + CurData.Spacing.Y/2, *RightSplit, NormalFont, TextColor );
						DrawString( Canvas, DrawX + HalfXL - LeftXSpacing/2, LabelY, *LeftSplit, NormalFont, TextColor );
					}
					else
					{
						DrawShadowedString( Canvas, DrawX + HalfXL - RightXSpacing/2, LabelY + CurData.Spacing.Y/2, *RightSplit, NormalFont, TextColor );
						DrawShadowedString( Canvas, DrawX + HalfXL - LeftXSpacing/2, LabelY, *LeftSplit, NormalFont, TextColor );
					}
				}
				else
				{
					if( bGhostNonMoving && !VarInfo.bMoving )
					{
						// Don't draw a shadowed string if this one is grayed out
						DrawString( Canvas, DrawX, LabelY, *VarInfo.Name, NormalFont, TextColor);
					}
					else
					{
						DrawShadowedString( Canvas, DrawX, LabelY, *VarInfo.Name, NormalFont, TextColor);
					}
				}
			}
		}
	}

}

void FLinkedObjDrawUtils::DrawVariableConnectors(FCanvas* Canvas, FLinkedObjDrawInfo& ObjInfo, const FIntPoint& Pos, const FIntPoint& Size, const INT VarWidth)
{
	// Do nothing here if no variables or event connectors.
	if( ObjInfo.Variables.Num() == 0 && ObjInfo.Events.Num() == 0 )
	{
		return;
	}
	const FLOAT Zoom2D = GetUniformScaleFromMatrix(Canvas->GetTransform());
	const UBOOL bHitTesting = Canvas->IsHitTesting();
	const UBOOL bSufficientlyZoomedIn = Zoom2D > ConnectorMinZoom;
	const INT LabelY = Pos.Y - LO_TEXT_BORDER;

	INT LastX = Pos.X, LastXL = 0;
	// if the title is wider than needed for the variables
	if (VarWidth < Size.X)
	{
		// then center the variables
		LastX += (Size.X - VarWidth)/2;
	}
	ObjInfo.VariableX.Add(ObjInfo.Variables.Num());
	FString LeftSplit, RightSplit;
	INT LeftXL, RightXL;
	for (INT Idx = 0; Idx < ObjInfo.Variables.Num(); Idx++)
	{
		INT VarX = LastX + LastXL + (LO_DESC_X_PADDING * 2);
		INT XL, YL;
		UBOOL bSplit = VarStringSize( NormalFont, XL, YL, ObjInfo.Variables(Idx).Name, &LeftSplit, &LeftXL, &RightSplit, &RightXL );
		ObjInfo.VariableX(Idx) = VarX + XL/2;
		if ( bSufficientlyZoomedIn )
		{

			INT ConnectorWidth(LO_CONNECTOR_WIDTH);
			INT ConnectorLength(LO_CONNECTOR_LENGTH);
			INT NewX(VarX);
			INT NewY(Pos.Y);
			if(bHitTesting)
			{
				Canvas->SetHitProxy( new HLinkedObjConnectorProxy(ObjInfo.ObjObject, LOC_VARIABLE, Idx) );
				ConnectorWidth += LO_EXTRA_HIT_WIDTH;
				ConnectorLength += LO_EXTRA_HIT_LENGTH;
				NewX -= LO_EXTRA_HIT_X;
				NewY -= LO_EXTRA_HIT_Y;
			}

			if (ObjInfo.Variables(Idx).bOutput)
			{
				FIntPoint Vert0, Vert1, Vert2;
				Vert0.X = -2 + NewX + XL/2 - ConnectorWidth/2;
				Vert0.Y = NewY + Size.Y;
				Vert1.X = Vert0.X + ConnectorWidth + 2;
				Vert1.Y = Vert0.Y;
				Vert2.X = (Vert1.X + Vert0.X) / 2;
				Vert2.Y = Vert0.Y + ConnectorLength + 2;
				DrawTriangle2D(Canvas,Vert0,FVector2D(0,0),Vert1,FVector2D(0,0),Vert2,FVector2D(0,0),ObjInfo.Variables(Idx).Color);
			}
			else
			{
				DrawTile( Canvas, NewX + XL/2 - ConnectorWidth / 2, NewY + Size.Y, ConnectorWidth, ConnectorLength, 0.f, 0.f, 0.f, 0.f, ObjInfo.Variables(Idx).Color );
			}
			if(bHitTesting) Canvas->SetHitProxy( NULL );

			if ( AABBLiesWithinViewport( Canvas, VarX, LabelY, XL, YL ) )
			{
				if (bSplit)
				{
					DrawShadowedString( Canvas, VarX + XL/2 - RightXL/2, LabelY + YL/2, *RightSplit, NormalFont, FLinearColor::White );
					DrawShadowedString( Canvas, VarX + XL/2 - LeftXL/2, LabelY, *LeftSplit, NormalFont, FLinearColor::White );
				}
				else
				{
					DrawShadowedString( Canvas, VarX, LabelY, *ObjInfo.Variables(Idx).Name, NormalFont, FLinearColor::White );
				}
			}
		}
		LastX = VarX;
		LastXL = XL;
	}
	// Draw event connectors.
	ObjInfo.EventX.Add( ObjInfo.Events.Num() );
	for (INT Idx = 0; Idx < ObjInfo.Events.Num(); Idx++)
	{
		INT VarX = LastX + LastXL + (LO_DESC_X_PADDING * 2);
		INT XL, YL;
		VarStringSize( NormalFont, XL, YL, ObjInfo.Events(Idx).Name );
		ObjInfo.EventX(Idx)	= VarX + XL/2;

		if ( bSufficientlyZoomedIn )
		{
			INT ConnectorWidth(LO_CONNECTOR_WIDTH);
			INT ConnectorLength(LO_CONNECTOR_LENGTH);
			INT NewX(VarX);
			INT NewY(Pos.Y);
			if(bHitTesting)
			{
				Canvas->SetHitProxy( new HLinkedObjConnectorProxy(ObjInfo.ObjObject, LOC_EVENT, Idx) );
				ConnectorWidth += LO_EXTRA_HIT_WIDTH;
				ConnectorLength += LO_EXTRA_HIT_LENGTH;
				NewX -= LO_EXTRA_HIT_X;
				NewY -= LO_EXTRA_HIT_Y;
			}

			DrawTile( Canvas, NewX + XL/2 - ConnectorWidth / 2, NewY + Size.Y, ConnectorWidth, ConnectorLength, 0.f, 0.f, 0.f, 0.f, ObjInfo.Events(Idx).Color );
			if(bHitTesting) Canvas->SetHitProxy( NULL );

			if ( AABBLiesWithinViewport( Canvas, VarX, LabelY, XL, YL ) )
			{
				DrawShadowedString( Canvas, VarX, LabelY, *ObjInfo.Events(Idx).Name, NormalFont, FLinearColor::White );
			}
		}
		LastX = VarX;
		LastXL = XL;
	}
}

/** Draws connection and node tooltips. */
void FLinkedObjDrawUtils::DrawToolTips(FCanvas* Canvas, const FLinkedObjDrawInfo& ObjInfo, const FIntPoint& Pos, const FIntPoint& Size)
{
	const FLOAT Zoom2D = GetUniformScaleFromMatrix(Canvas->GetTransform());
	const UBOOL bSufficientlyZoomedIn	= Zoom2D > ConnectorMinZoom;

	const INT ConnectorRangeY	= Size.Y - 2*LO_TEXT_BORDER;
	const INT CenterY			= Pos.Y + LO_TEXT_BORDER + ConnectorRangeY / 2;

	// These can be exposed via function parameters if needed
	const FColor ToolTipTextColor(255, 255, 255);
	const FColor ToolTipBackgroundColor(140,140,140);
	const INT ToolTipBackgroundBorder = 3;

	// Draw tooltips for the node
	if (bSufficientlyZoomedIn && ObjInfo.ToolTips.Num() > 0)
	{
		TArray<FIntPoint> ToolTipPositions;
		ToolTipPositions.Empty(ObjInfo.ToolTips.Num());
		FIntRect ToolTipBounds(INT_MAX, INT_MAX, INT_MIN, INT_MIN);

		// Calculate the tooltip string's bounds
		for (INT ToolTipIndex = 0; ToolTipIndex < ObjInfo.ToolTips.Num(); ToolTipIndex++)
		{
			FIntPoint StringDim;
			const FString& ToolTip = ObjInfo.ToolTips(ToolTipIndex);
			StringSize(NormalFont, StringDim.X, StringDim.Y, *ToolTip);

			FIntPoint StringPos(Pos.X - 30, Pos.Y + Size.Y - 5 + ToolTipIndex * 17);
			ToolTipPositions.AddItem(StringPos);
			ToolTipBounds.Min.X = Min(ToolTipBounds.Min.X, StringPos.X);
			ToolTipBounds.Min.Y = Min(ToolTipBounds.Min.Y, StringPos.Y);
			ToolTipBounds.Max.X = Max(ToolTipBounds.Max.X, StringPos.X + StringDim.X);
			ToolTipBounds.Max.Y = Max(ToolTipBounds.Max.Y, StringPos.Y + StringDim.Y);
		}

		const INT BackgroundX = ToolTipBounds.Min.X - ToolTipBackgroundBorder;
		const INT BackgroundY = ToolTipBounds.Min.Y - ToolTipBackgroundBorder;
		const INT BackgroundSizeX = ToolTipBounds.Max.X - ToolTipBounds.Min.X + ToolTipBackgroundBorder * 2;
		const INT BackgroundSizeY = ToolTipBounds.Max.Y - ToolTipBounds.Min.Y + ToolTipBackgroundBorder * 2;

		// Draw the black outline
		DrawTile(
			Canvas, 
			BackgroundX, BackgroundY, 
			BackgroundSizeX, BackgroundSizeY,	
			0.0f,0.0f,0.0f,0.0f, 
			FColor(0, 0, 0));

		// Draw the background
		DrawTile(
			Canvas, 
			BackgroundX + 1, BackgroundY + 1, 
			BackgroundSizeX - 2, BackgroundSizeY - 2,	
			0.0f,0.0f,0.0f,0.0f, 
			ToolTipBackgroundColor);

		// Draw the tooltip strings
		for (INT ToolTipIndex = 0; ToolTipIndex < ObjInfo.ToolTips.Num(); ToolTipIndex++)
		{
			const FString& ToolTip = ObjInfo.ToolTips(ToolTipIndex);
			DrawShadowedString(Canvas, ToolTipPositions(ToolTipIndex).X, ToolTipPositions(ToolTipIndex).Y, *ToolTip, NormalFont, ToolTipTextColor);
		}
	}
	
	// Draw tooltips for connections on the left side of the node
	if( ObjInfo.Inputs.Num() > 0 )
	{
		const INT SpacingY = ConnectorRangeY/ObjInfo.Inputs.Num();
		const INT StartY = CenterY - (ObjInfo.Inputs.Num()-1) * SpacingY / 2;

		for(INT i=0; i<ObjInfo.Inputs.Num(); i++)
		{
			const INT LinkY	= StartY + i * SpacingY;

			if ( bSufficientlyZoomedIn && ObjInfo.Inputs(i).ToolTips.Num() > 0 )
			{
				TArray<FIntPoint> ToolTipPositions;
				ToolTipPositions.Empty(ObjInfo.Inputs(i).ToolTips.Num());
				FIntRect ToolTipBounds(INT_MAX, INT_MAX, INT_MIN, INT_MIN);

				// Calculate the tooltip string's bounds
				for (INT ToolTipIndex = 0; ToolTipIndex < ObjInfo.Inputs(i).ToolTips.Num(); ToolTipIndex++)
				{
					const INT LinkY	= StartY + i * SpacingY;

					FIntPoint StringDim;
					const FString& ToolTip = ObjInfo.Inputs(i).ToolTips(ToolTipIndex);
					StringSize(NormalFont, StringDim.X, StringDim.Y, *ToolTip);

					FIntPoint StringPos(Pos.X - LO_CONNECTOR_LENGTH * 2 - 5, LinkY - LO_CONNECTOR_WIDTH / 2 + ToolTipIndex * 17);
					ToolTipPositions.AddItem(StringPos);
					ToolTipBounds.Min.X = Min(ToolTipBounds.Min.X, StringPos.X);
					ToolTipBounds.Min.Y = Min(ToolTipBounds.Min.Y, StringPos.Y);
					ToolTipBounds.Max.X = Max(ToolTipBounds.Max.X, StringPos.X + StringDim.X);
					ToolTipBounds.Max.Y = Max(ToolTipBounds.Max.Y, StringPos.Y + StringDim.Y);
				}

				// Move the bounds and string positions left, by how long the longest tooltip string is
				const INT XOffset = ToolTipBounds.Max.X - ToolTipBounds.Min.X;
				for (INT ToolTipIndex = 0; ToolTipIndex < ObjInfo.Inputs(i).ToolTips.Num(); ToolTipIndex++)
				{
					ToolTipPositions(ToolTipIndex).X -= XOffset;
				}
				ToolTipBounds.Min.X -= XOffset;
				ToolTipBounds.Max.X -= XOffset;

				const INT BackgroundX = ToolTipBounds.Min.X - ToolTipBackgroundBorder;
				const INT BackgroundY = ToolTipBounds.Min.Y - ToolTipBackgroundBorder;
				const INT BackgroundSizeX = ToolTipBounds.Max.X - ToolTipBounds.Min.X + ToolTipBackgroundBorder * 2;
				const INT BackgroundSizeY = ToolTipBounds.Max.Y - ToolTipBounds.Min.Y + ToolTipBackgroundBorder * 2;

				// Draw the black outline
				DrawTile(
					Canvas, 
					BackgroundX, BackgroundY, 
					BackgroundSizeX, BackgroundSizeY,	
					0.0f,0.0f,0.0f,0.0f, 
					FColor(0, 0, 0));

				// Draw the background
				DrawTile(
					Canvas, 
					BackgroundX + 1, BackgroundY + 1, 
					BackgroundSizeX - 2, BackgroundSizeY - 2,	
					0.0f,0.0f,0.0f,0.0f, 
					ToolTipBackgroundColor);

				for (INT ToolTipIndex = 0; ToolTipIndex < ObjInfo.Inputs(i).ToolTips.Num(); ToolTipIndex++)
				{
					// Draw the tooltip strings
					const FString& ToolTip = ObjInfo.Inputs(i).ToolTips(ToolTipIndex);
					DrawShadowedString(Canvas, ToolTipPositions(ToolTipIndex).X, ToolTipPositions(ToolTipIndex).Y, *ToolTip, NormalFont, ToolTipTextColor);
				}
			}
		}
	}

	// Draw tooltips for connections on the right side of the node
	if( ObjInfo.Outputs.Num() > 0 )
	{
		const INT SpacingY	= ConnectorRangeY/ObjInfo.Outputs.Num();
		const INT StartY = CenterY - (ObjInfo.Outputs.Num()-1) * SpacingY / 2;

		for(INT i=0; i<ObjInfo.Outputs.Num(); i++)
		{
			const INT LinkY	= StartY + i * SpacingY;

			if ( bSufficientlyZoomedIn && ObjInfo.Outputs(i).ToolTips.Num() > 0 )
			{
				TArray<FIntPoint> ToolTipPositions;
				ToolTipPositions.Empty(ObjInfo.Outputs(i).ToolTips.Num());
				FIntRect ToolTipBounds(INT_MAX, INT_MAX, INT_MIN, INT_MIN);

				// Calculate the tooltip string's bounds
				for (INT ToolTipIndex = 0; ToolTipIndex < ObjInfo.Outputs(i).ToolTips.Num(); ToolTipIndex++)
				{
					const INT LinkY	= StartY + i * SpacingY;

					FIntPoint StringDim;
					const FString& ToolTip = ObjInfo.Outputs(i).ToolTips(ToolTipIndex);
					StringSize(NormalFont, StringDim.X, StringDim.Y, *ToolTip);

					FIntPoint StringPos(Pos.X + Size.X + LO_CONNECTOR_LENGTH + 16, LinkY - LO_CONNECTOR_WIDTH / 2 + ToolTipIndex * 17);
					ToolTipPositions.AddItem(StringPos);
					ToolTipBounds.Min.X = Min(ToolTipBounds.Min.X, StringPos.X);
					ToolTipBounds.Min.Y = Min(ToolTipBounds.Min.Y, StringPos.Y);
					ToolTipBounds.Max.X = Max(ToolTipBounds.Max.X, StringPos.X + StringDim.X);
					ToolTipBounds.Max.Y = Max(ToolTipBounds.Max.Y, StringPos.Y + StringDim.Y);
				}

				const INT BackgroundX = ToolTipBounds.Min.X - ToolTipBackgroundBorder;
				const INT BackgroundY = ToolTipBounds.Min.Y - ToolTipBackgroundBorder;
				const INT BackgroundSizeX = ToolTipBounds.Max.X - ToolTipBounds.Min.X + ToolTipBackgroundBorder * 2;
				const INT BackgroundSizeY = ToolTipBounds.Max.Y - ToolTipBounds.Min.Y + ToolTipBackgroundBorder * 2;

				// Draw the black outline
				DrawTile(
					Canvas, 
					BackgroundX, BackgroundY, 
					BackgroundSizeX, BackgroundSizeY,	
					0.0f,0.0f,0.0f,0.0f, 
					FColor(0, 0, 0));

				// Draw the background
				DrawTile(
					Canvas, 
					BackgroundX + 1, BackgroundY + 1, 
					BackgroundSizeX - 2, BackgroundSizeY - 2,	
					0.0f,0.0f,0.0f,0.0f, 
					ToolTipBackgroundColor);

				for (INT ToolTipIndex = 0; ToolTipIndex < ObjInfo.Outputs(i).ToolTips.Num(); ToolTipIndex++)
				{
					// Draw the tooltip strings
					const FString& ToolTip = ObjInfo.Outputs(i).ToolTips(ToolTipIndex);
					DrawShadowedString(Canvas, ToolTipPositions(ToolTipIndex).X, ToolTipPositions(ToolTipIndex).Y, *ToolTip, NormalFont, ToolTipTextColor);
				}
			}
		}
	}
}

void FLinkedObjDrawUtils::DrawLinkedObj(FCanvas* Canvas, FLinkedObjDrawInfo& ObjInfo, const TCHAR* Name, const TCHAR* Comment, const FColor& FontColor, const FColor& BorderColor, const FColor& TitleBkgColor, const FIntPoint& Pos)
{
	const UBOOL bHitTesting = Canvas->IsHitTesting();

	const FIntPoint TitleSize			= GetTitleBarSize(Canvas, Name);
	const FIntPoint LogicSize			= GetLogicConnectorsSize(ObjInfo);
	const FIntPoint VarSize				= GetVariableConnectorsSize(Canvas, ObjInfo);
	const FIntPoint VisualizationSize	= ObjInfo.VisualizationSize;

	ObjInfo.DrawWidth	= Max(Max3(TitleSize.X, LogicSize.X, VarSize.X), VisualizationSize.X);
	ObjInfo.DrawHeight	= TitleSize.Y + LogicSize.Y + VarSize.Y + VisualizationSize.Y + 3;

	ObjInfo.VisualizationPosition = Pos + FIntPoint(0, TitleSize.Y + LogicSize.Y + 1);

	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HLinkedObjProxy(ObjInfo.ObjObject) );

	// Comment list
	TArray<FString> Comments;
	Comments.AddItem(FString(Comment));
	DrawTitleBar(Canvas, Pos, FIntPoint(ObjInfo.DrawWidth, TitleSize.Y), FontColor, BorderColor, TitleBkgColor, Name, Comments);

	//border
	DrawTile( Canvas, Pos.X,		Pos.Y + TitleSize.Y + 1,	ObjInfo.DrawWidth,		LogicSize.Y + VarSize.Y + VisualizationSize.Y,		0.0f,0.0f,0.0f,0.0f, BorderColor );
	//background
	DrawTile( Canvas, Pos.X + 1,	Pos.Y + TitleSize.Y + 2,	ObjInfo.DrawWidth - 2,	LogicSize.Y + VarSize.Y + VisualizationSize.Y - 2,	0.0f,0.0f,0.0f,0.0f, FColor(140,140,140) );

	//drop shadow
	DrawTile( Canvas, Pos.X,Pos.Y + TitleSize.Y + LogicSize.Y + VisualizationSize.Y,ObjInfo.DrawWidth - 2,2,0.f,0.f,0.f,0.f,BorderColor);

	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );

	DrawLogicConnectors(Canvas, ObjInfo, Pos + FIntPoint(0, TitleSize.Y + 1), FIntPoint(ObjInfo.DrawWidth, LogicSize.Y));
	DrawVariableConnectors(Canvas, ObjInfo, Pos + FIntPoint(0, TitleSize.Y + 1 + LogicSize.Y + 1 + VisualizationSize.Y), FIntPoint(ObjInfo.DrawWidth, VarSize.Y), VarSize.X);
}

// if the rendering changes, these need to change
INT FLinkedObjDrawUtils::ComputeSliderHeight(INT SliderWidth)
{
	return LO_SLIDER_HANDLE_HEIGHT+4;
}

INT FLinkedObjDrawUtils::Compute2DSliderHeight(INT SliderWidth)
{
	return SliderWidth;
}

INT FLinkedObjDrawUtils::DrawSlider( FCanvas* Canvas, const FIntPoint& SliderPos, INT SliderWidth, const FColor& BorderColor, const FColor& BackGroundColor, FLOAT SliderPosition, const FString& ValText, UObject* Obj, int SliderIndex , UBOOL bDrawTextOnSide)
{
	const UBOOL bHitTesting = Canvas->IsHitTesting();
	const INT SliderBoxHeight = LO_SLIDER_HANDLE_HEIGHT + 4;

	if ( AABBLiesWithinViewport( Canvas, SliderPos.X, SliderPos.Y, SliderWidth, SliderBoxHeight ) )
	{
		const FLOAT Zoom2D = GetUniformScaleFromMatrix(Canvas->GetTransform());
		const INT SliderRange = (SliderWidth - 4 - LO_SLIDER_HANDLE_WIDTH);
		const INT SliderHandlePosX = appTrunc(SliderPos.X + 2 + (SliderPosition * SliderRange));

		if(bHitTesting) Canvas->SetHitProxy( new HLinkedObjProxySpecial(Obj, SliderIndex) );
		DrawTile( Canvas, SliderPos.X,		SliderPos.Y - 1,	SliderWidth,		SliderBoxHeight,		0.0f,0.0f,0.0f,0.0f, BorderColor );
		DrawTile( Canvas, SliderPos.X + 1,	SliderPos.Y,		SliderWidth - 2,	SliderBoxHeight - 2,	0.0f,0.0f,0.0f,0.0f, BackGroundColor );

		if ( Zoom2D > SliderMinZoom )
		{
			DrawTile( Canvas, SliderHandlePosX, SliderPos.Y + 1, LO_SLIDER_HANDLE_WIDTH, LO_SLIDER_HANDLE_HEIGHT, 0.f, 0.f, 1.f, 1.f, SliderHandleColor );
		}
		if(bHitTesting) Canvas->SetHitProxy( NULL );
	}

	if(bDrawTextOnSide)
	{
		INT SizeX, SizeY;
		StringSize(NormalFont, SizeX, SizeY, *ValText);

		const INT PosX = SliderPos.X - 2 - SizeX; 
		const INT PosY = SliderPos.Y + (SliderBoxHeight + 1 - SizeY)/2;
		if ( AABBLiesWithinViewport( Canvas, PosX, PosY, SizeX, SizeY ) )
		{
			DrawString( Canvas, PosX, PosY, *ValText, NormalFont, FColor(0,0,0) );
		}
	}
	else
	{
		DrawString( Canvas, SliderPos.X + 2, SliderPos.Y + SliderBoxHeight + 1, *ValText, NormalFont, FColor(0,0,0) );
	}
	return SliderBoxHeight;
}

INT FLinkedObjDrawUtils::Draw2DSlider(FCanvas* Canvas, const FIntPoint &SliderPos, INT SliderWidth, const FColor& BorderColor, const FColor& BackGroundColor, FLOAT SliderPositionX, FLOAT SliderPositionY, const FString &ValText, UObject *Obj, INT SliderIndex, UBOOL bDrawTextOnSide)
{
	const UBOOL bHitTesting = Canvas->IsHitTesting();

	const INT SliderBoxHeight = SliderWidth;

	if ( AABBLiesWithinViewport( Canvas, SliderPos.X, SliderPos.Y, SliderWidth, SliderBoxHeight ) )
	{
		const FLOAT Zoom2D = GetUniformScaleFromMatrix(Canvas->GetTransform());
		const INT SliderRangeX = (SliderWidth - 4 - LO_SLIDER_HANDLE_HEIGHT);
		const INT SliderRangeY = (SliderBoxHeight - 4 - LO_SLIDER_HANDLE_HEIGHT);
		const INT SliderHandlePosX = SliderPos.X + 2 + appTrunc(SliderPositionX * SliderRangeX);
		const INT SliderHandlePosY = SliderPos.Y + 2 + appTrunc(SliderPositionY * SliderRangeY);

		if(bHitTesting) Canvas->SetHitProxy( new HLinkedObjProxySpecial(Obj, SliderIndex) );
		DrawTile( Canvas, SliderPos.X,		SliderPos.Y - 1,	SliderWidth,		SliderBoxHeight,		0.0f,0.0f,0.0f,0.0f, BorderColor );
		DrawTile( Canvas, SliderPos.X + 1,	SliderPos.Y,		SliderWidth - 2,	SliderBoxHeight - 2,	0.0f,0.0f,0.0f,0.0f, BackGroundColor );

		if ( Zoom2D > SliderMinZoom )
		{
			DrawTile( Canvas, SliderHandlePosX, SliderHandlePosY, LO_SLIDER_HANDLE_HEIGHT, LO_SLIDER_HANDLE_HEIGHT, 0.f, 0.f, 1.f, 1.f, SliderHandleColor );
		}
		if(bHitTesting) Canvas->SetHitProxy( NULL );
	}

	if(bDrawTextOnSide)
	{
		INT SizeX, SizeY;
		StringSize(NormalFont, SizeX, SizeY, *ValText);
		const INT PosX = SliderPos.X - 2 - SizeX;
		const INT PosY = SliderPos.Y + (SliderBoxHeight + 1 - SizeY)/2;
		if ( AABBLiesWithinViewport( Canvas, PosX, PosY, SizeX, SizeY ) )
		{
			DrawString( Canvas, PosX, PosY, *ValText, NormalFont, FColor(0,0,0));
		}
	}
	else
	{
		DrawString( Canvas, SliderPos.X + 2, SliderPos.Y + SliderBoxHeight + 1, *ValText, NormalFont, FColor(0,0,0) );
	}

	return SliderBoxHeight;
}


/**
 * @return		TRUE if the current viewport contains some portion of the specified AABB.
 */
UBOOL FLinkedObjDrawUtils::AABBLiesWithinViewport(FCanvas* Canvas, FLOAT X, FLOAT Y, FLOAT SizeX, FLOAT SizeY)
{
	const FMatrix TransformMatrix = Canvas->GetTransform();
	const FLOAT Zoom2D = GetUniformScaleFromMatrix(Canvas->GetTransform());
	FRenderTarget* RenderTarget = Canvas->GetRenderTarget();
	if ( !RenderTarget )
	{
		return FALSE;
	}

	// Transform the 2D point by the current transform matrix.
	FVector Point(X,Y, 0.0f);
	Point = TransformMatrix.TransformFVector(Point);
	X = Point.X;
	Y = Point.Y;

	// Check right side.
	if ( X > RenderTarget->GetSizeX() )
	{
		return FALSE;
	}

	// Check left side.
	if ( X+SizeX*Zoom2D < 0.f )
	{
		return FALSE;
	}

	// Check bottom side.
	if ( Y > RenderTarget->GetSizeY() )
	{
		return FALSE;
	}

	// Check top side.
	if ( Y+SizeY*Zoom2D < 0.f )
	{
		return FALSE;
	}

	return TRUE;
}

/**
 * Convenience function for filtering calls to FRenderInterface::DrawTile via AABBLiesWithinViewport.
 */
void FLinkedObjDrawUtils::DrawTile(FCanvas* Canvas,FLOAT X,FLOAT Y,FLOAT SizeX,FLOAT SizeY,FLOAT U,FLOAT V,FLOAT SizeU,FLOAT SizeV,const FLinearColor& Color,FTexture* Texture,UBOOL AlphaBlend)
{
	if ( AABBLiesWithinViewport( Canvas, X, Y, SizeX, SizeY ) )
	{
		::DrawTile(Canvas,X,Y,SizeX,SizeY,U,V,SizeU,SizeV,Color,Texture,AlphaBlend);
	}
}

/**
 * Convenience function for filtering calls to FRenderInterface::DrawTile via AABBLiesWithinViewport. Additional flag to control the freezing of time.
 */
void FLinkedObjDrawUtils::DrawTile(FCanvas* Canvas,FLOAT X,FLOAT Y,FLOAT SizeX,FLOAT SizeY,FLOAT U,FLOAT V,FLOAT SizeU,FLOAT SizeV,FMaterialRenderProxy* MaterialRenderProxy,UBOOL bFreezeTime)
{
	if ( AABBLiesWithinViewport( Canvas, X, Y, SizeX, SizeY ) )
	{
		::DrawTile(Canvas,X,Y,SizeX,SizeY,U,V,SizeU,SizeV,MaterialRenderProxy,bFreezeTime);
	}
}

/**
 * Convenience function for filtering calls to FRenderInterface::DrawString through culling heuristics.
 */
INT FLinkedObjDrawUtils::DrawString(FCanvas* Canvas,FLOAT StartX,FLOAT StartY,const TCHAR* Text,UFont* Font,const FLinearColor& Color)
{
	const FLOAT Zoom2D = GetUniformScaleFromMatrix(Canvas->GetTransform());

	if ( Zoom2D > TextMinZoom )
	{
		return ::DrawString(Canvas,StartX,StartY,Text,Font,Color);
	}
	else
	{
		return 0;
	}
}

/**
 * Convenience function for filtering calls to FRenderInterface::DrawShadowedString through culling heuristics.
 */
INT FLinkedObjDrawUtils::DrawShadowedString(FCanvas* Canvas,FLOAT StartX,FLOAT StartY,const TCHAR* Text,UFont* Font,const FLinearColor& Color)
{
	const FLOAT Zoom2D = GetUniformScaleFromMatrix(Canvas->GetTransform());

	if ( Zoom2D > TextMinZoom )
	{
		if ( Zoom2D < 1.f - DELTA )
		{
			return ::DrawString(Canvas,StartX,StartY,Text,Font,Color);
		}
		else
		{
			return ::DrawShadowedString(Canvas,StartX,StartY,Text,Font,Color);
		}
	}
	else
	{
		return 0;
	}
}

/**
 * Convenience function for filtering calls to FRenderInterface::DrawStringZ through culling heuristics.
 */
INT FLinkedObjDrawUtils::DisplayComment( FCanvas* Canvas, UBOOL Draw, FLOAT CurX, FLOAT CurY, FLOAT Z, FLOAT& XL, FLOAT& YL, UFont* Font, const TCHAR* Text, FLinearColor DrawColor )
{
	const FLOAT Zoom2D = GetUniformScaleFromMatrix(Canvas->GetTransform());

	if ( Zoom2D > TextMinZoom )
	{
		TArray<FWrappedStringElement> Lines;
		FTextSizingParameters RenderParms( 0.0f, 0.0f, XL, 0.0f, Font, 0.0f );
		UCanvas::WrapString( RenderParms, 0, Text, Lines );

		if( Lines.Num() > 0 )
		{
			FLOAT StrHeight = Font->GetMaxCharHeight();
			FLOAT HeightTest = Canvas->GetRenderTarget()->GetSizeY();
			StrHeight *= Font->GetScalingFactor( HeightTest );

			for( INT Idx = 0; Idx < Lines.Num(); Idx++ )
			{
				const TCHAR* TextLine = *Lines( Idx ).Value;

				if ( Zoom2D > 1.0f - DELTA )
				{
					DrawStringZ( Canvas, CurX + 1.0f, CurY + 1.0f, Z, TextLine, Font, FLinearColor::Black );
				}

				DrawStringZ( Canvas, CurX, CurY, Z, TextLine, Font, DrawColor );

				CurY += StrHeight;
			}
		}

		return 1;
	}

	return 0;
}

/**
 * Takes a transformation matrix and returns a uniform scaling factor based on the 
 * length of the rotation vectors.
 */
FLOAT FLinkedObjDrawUtils::GetUniformScaleFromMatrix(const FMatrix &Matrix)
{
	const FVector XAxis = Matrix.GetAxis(0);
	const FVector YAxis = Matrix.GetAxis(1);
	const FVector ZAxis = Matrix.GetAxis(2);

	FLOAT Scale = Max(XAxis.Size(), YAxis.Size());
	Scale = Max(Scale, ZAxis.Size());

	return Scale;
}

