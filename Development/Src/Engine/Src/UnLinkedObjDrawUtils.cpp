/*=============================================================================
	UnLinkedObjDrawUtils.cpp: Utils for drawing linked objects.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "EnginePrivate.h"
#include "UnLinkedObjDrawUtils.h"

static const FLOAT	MaxPixelsPerStep(10.f);
static const INT	ArrowheadLength(14);
static const INT	ArrowheadWidth(4);

void FLinkedObjDrawUtils::DrawNGon(FRenderInterface* RI, FIntPoint Center, FColor Color, INT NumSides, INT Radius)
{
	FIntPoint Verts[255];

	NumSides = Clamp(NumSides, 3, 255);

	for(INT i=0; i<NumSides+1; i++)
	{
		FLOAT Angle = (2 * (FLOAT)PI) * (FLOAT)i/(FLOAT)NumSides;
		Verts[i] = Center + FIntPoint( Radius*appCos(Angle), Radius*appSin(Angle) );
	}

	for(INT i=0; i<NumSides; i++)
	{
		RI->DrawTriangle2D( Center, 0.f, 0.f, Verts[i+0], 0.f, 0.f, Verts[i+1], 0.f, 0.f, Color );
	}
}

/**
 *	@param EndDir End tangent. Note that this points in the same direction as StartDir ie 'along' the curve. So if you are dealing with 'handles' you will need to invert when you pass it in.
 *
 */
void FLinkedObjDrawUtils::DrawSpline(FRenderInterface* RI, const FIntPoint& Start, const FVector2D& StartDir, const FIntPoint& End, const FVector2D& EndDir, const FColor& LineColor, UBOOL bArrowhead)
{
	FVector2D StartVec( Start );
	FVector2D EndVec( End );

	// Rough estimate of length of curve. Use direct length and distance between 'handles'. Sure we can do better.
	FLOAT DirectLength = (EndVec - StartVec).Size();
	FLOAT HandleLength = ((EndVec - EndDir) - (StartVec + StartDir)).Size();
	
	INT NumSteps = appCeil(Max(DirectLength,HandleLength)/MaxPixelsPerStep);

	FVector2D OldPos = StartVec;

	for(INT i=0; i<NumSteps; i++)
	{
		FLOAT Alpha = ((FLOAT)i+1.f)/(FLOAT)NumSteps;
		FVector2D NewPos = CubicInterp(StartVec, StartDir, EndVec, EndDir, Alpha);

		FIntPoint OldIntPos = OldPos.IntPoint();
		FIntPoint NewIntPos = NewPos.IntPoint();

		RI->DrawLine2D( OldIntPos.X, OldIntPos.Y, NewIntPos.X, NewIntPos.Y, LineColor );

		// If this is the last section, use its direction to draw the arrowhead.
		if(bArrowhead && (i == NumSteps-1) && (i >= 2))
		{
			// Go back 3 steps to give us decent direction for arrowhead
			FLOAT ArrowStartAlpha = ((FLOAT)i-2.f)/(FLOAT)NumSteps;
			FVector2D ArrowStartPos = CubicInterp(StartVec, StartDir, EndVec, EndDir, ArrowStartAlpha);

			FVector2D StepDir = (NewPos - ArrowStartPos).Normalize();
			DrawArrowhead( RI, NewIntPos, StepDir, LineColor );
		}

		OldPos = NewPos;
	}
}

void FLinkedObjDrawUtils::DrawArrowhead(FRenderInterface* RI, const FIntPoint& Pos, const FVector2D& Dir, const FColor& Color)
{
	FVector2D Orth(Dir.Y, -Dir.X);
	FVector2D PosVec(Pos);
	FVector2D pt2 = PosVec - (Dir * ArrowheadLength) - (Orth * ArrowheadWidth);
	FVector2D pt1 = PosVec;
	FVector2D pt3 = PosVec - (Dir * ArrowheadLength) + (Orth * ArrowheadWidth);
	RI->DrawTriangle2D(
		pt1.IntPoint(),0,0,
		pt2.IntPoint(),0,0,
		pt3.IntPoint(),0,0,
		Color,NULL,0);
}


FIntPoint FLinkedObjDrawUtils::GetTitleBarSize(FRenderInterface* RI, const TCHAR* Name)
{
	INT XL, YL;
	RI->StringSize( GEngine->SmallFont, XL, YL, Name );

	INT LabelWidth = XL + (LO_TEXT_BORDER*2) + 4;

	return FIntPoint( Max(LabelWidth, LO_MIN_SHAPE_SIZE), LO_CAPTION_HEIGHT );
}


void FLinkedObjDrawUtils::DrawTitleBar(FRenderInterface* RI, const FIntPoint& Pos, const FIntPoint& Size, const FColor& BorderColor, const FColor& BkgColor, const TCHAR* Name, const TCHAR* Comment )
{
	// Draw label at top
	RI->DrawTile(Pos.X,		Pos.Y,		Size.X,		Size.Y,		0.0f,0.0f,0.0f,0.0f, BorderColor );
	RI->DrawTile(Pos.X+1,	Pos.Y+1,	Size.X-2,	Size.Y-2,	0.0f,0.0f,0.0f,0.0f, BkgColor );

	INT XL, YL;
	RI->StringSize( GEngine->SmallFont, XL, YL, Name );

	if(RI->Zoom2D < 1.f)
		RI->DrawString( Pos.X+((Size.X-XL)/2), Pos.Y+((Size.Y-YL)/2)+1, Name, GEngine->SmallFont, FColor(255,255,128) );
	else
		RI->DrawShadowedString( Pos.X+((Size.X-XL)/2), Pos.Y+((Size.Y-YL)/2)+1, Name, GEngine->SmallFont, FColor(255,255,128) );

	// Level designer description below the window
	if( Comment )
	{
		RI->StringSize( GEngine->SmallFont, XL, YL, Comment );
		INT x = Pos.X + 2;
		INT y = Pos.Y - YL - 2;
		RI->DrawString( x,y, Comment, GEngine->SmallFont, FColor(64,64,192) );
		RI->DrawString( x+1,y, Comment, GEngine->SmallFont, FColor(64,64,192) );
	}
}



// The InputY and OuputY are optional extra outputs, giving the size of the input and output connectors

FIntPoint FLinkedObjDrawUtils::GetLogicConnectorsSize(FRenderInterface* RI, const FLinkedObjDrawInfo& ObjInfo, INT* InputY, INT* OutputY)
{
	INT MaxInputDescX = 0;
	INT MaxInputDescY = 0;
	for(INT i=0; i<ObjInfo.Inputs.Num(); i++)
	{
		INT XL, YL;
		RI->StringSize( GEngine->SmallFont, XL, YL, ObjInfo.Inputs(i).Name );

		MaxInputDescX = Max(XL, MaxInputDescX);

		if(i>0) MaxInputDescY += LO_DESC_Y_PADDING;
		MaxInputDescY += Max(YL, LO_CONNECTOR_WIDTH);
	}

	INT MaxOutputDescX = 0;
	INT MaxOutputDescY = 0;
	for(INT i=0; i<ObjInfo.Outputs.Num(); i++)
	{
		INT XL, YL;
		RI->StringSize( GEngine->SmallFont, XL, YL, ObjInfo.Outputs(i).Name );

		MaxOutputDescX = Max(XL, MaxOutputDescX);

		if(i>0) MaxOutputDescY += LO_DESC_Y_PADDING;
		MaxOutputDescY += Max(YL, LO_CONNECTOR_WIDTH);
	}

	INT NeededX = MaxInputDescX + MaxOutputDescX + LO_DESC_X_PADDING + (2*LO_TEXT_BORDER);
	INT NeededY = Max( MaxInputDescY, MaxOutputDescY ) + (2*LO_TEXT_BORDER);

	if(InputY)
		*InputY = MaxInputDescY + (2*LO_TEXT_BORDER);

	if(OutputY)
		*OutputY = MaxOutputDescY + (2*LO_TEXT_BORDER);

	return FIntPoint(NeededX, NeededY);
}

void FLinkedObjDrawUtils::DrawLogicConnectors(FRenderInterface* RI, FLinkedObjDrawInfo& ObjInfo, const FIntPoint& Pos, const FIntPoint& Size)
{
	UBOOL bHitTesting = RI->IsHitTesting();

	INT XL, YL;
	RI->StringSize(GEngine->SmallFont, XL, YL, TEXT("GgIhy"));

	INT ConnectorWidth = Max(YL, LO_CONNECTOR_WIDTH);
	INT ConnectorRangeY = Size.Y - 2*LO_TEXT_BORDER;
	INT CenterY = Pos.Y + LO_TEXT_BORDER + 0.5*ConnectorRangeY;

	INT SpacingY, StartY;

	// Do nothing if no Input connectors
	if( ObjInfo.Inputs.Num() > 0 )
	{
		SpacingY = ConnectorRangeY/ObjInfo.Inputs.Num();
		StartY = CenterY - 0.5*(ObjInfo.Inputs.Num()-1)*SpacingY;

		ObjInfo.InputY.Add( ObjInfo.Inputs.Num() );
		for(INT i=0; i<ObjInfo.Inputs.Num(); i++)
		{
			INT LinkY = StartY + i * SpacingY;

			ObjInfo.InputY(i) = LinkY;

			if(bHitTesting) RI->SetHitProxy( new HLinkedObjConnectorProxy(ObjInfo.ObjObject, LOC_INPUT, i) );
			RI->DrawTile(Pos.X - LO_CONNECTOR_LENGTH, LinkY - 0.5*LO_CONNECTOR_WIDTH, LO_CONNECTOR_LENGTH, LO_CONNECTOR_WIDTH, 0.f, 0.f, 0.f, 0.f, ObjInfo.Inputs(i).Color );
			if(bHitTesting) RI->SetHitProxy( NULL );

			RI->StringSize(GEngine->SmallFont, XL, YL, ObjInfo.Inputs(i).Name);
			RI->DrawShadowedString(Pos.X + LO_TEXT_BORDER, LinkY - 0.5*YL, ObjInfo.Inputs(i).Name, GEngine->SmallFont, FLinearColor::White );
		}
	}

	// Do nothing if no Output connectors
	if( ObjInfo.Outputs.Num() > 0 )
	{
		SpacingY = ConnectorRangeY/ObjInfo.Outputs.Num();
		StartY = CenterY - 0.5*(ObjInfo.Outputs.Num()-1)*SpacingY;

		ObjInfo.OutputY.Add( ObjInfo.Outputs.Num() );
		for(INT i=0; i<ObjInfo.Outputs.Num(); i++)
		{
			INT LinkY = StartY + i * SpacingY;

			ObjInfo.OutputY(i) = LinkY;

			if(bHitTesting) RI->SetHitProxy( new HLinkedObjConnectorProxy(ObjInfo.ObjObject, LOC_OUTPUT, i) );
			RI->DrawTile(Pos.X + Size.X, LinkY - 0.5*LO_CONNECTOR_WIDTH, LO_CONNECTOR_LENGTH, LO_CONNECTOR_WIDTH, 0.f, 0.f, 0.f, 0.f, ObjInfo.Outputs(i).Color );
			if(bHitTesting) RI->SetHitProxy( NULL );

			RI->StringSize(GEngine->SmallFont, XL, YL, ObjInfo.Outputs(i).Name);
			RI->DrawShadowedString(Pos.X + Size.X - XL - LO_TEXT_BORDER, LinkY - 0.5*YL, ObjInfo.Outputs(i).Name, GEngine->SmallFont, FLinearColor::White );
		}
	}
}



FIntPoint FLinkedObjDrawUtils::GetVariableConnectorsSize(FRenderInterface* RI, const FLinkedObjDrawInfo& ObjInfo)
{
	INT MaxVarX = 0;
	INT MaxVarY = 0;
	for(INT i=0; i<ObjInfo.Variables.Num(); i++)
	{
		INT XL, YL;
		RI->StringSize( GEngine->SmallFont, XL, YL, ObjInfo.Variables(i).Name );

		MaxVarX = Max(XL, MaxVarX);
		MaxVarY = Max(YL, MaxVarY);
	}
	// also check the list of event links
	for (INT idx = 0; idx < ObjInfo.Events.Num(); idx++)
	{
		INT XL, YL;
		RI->StringSize( GEngine->SmallFont, XL, YL, ObjInfo.Events(idx).Name );

		MaxVarX = Max(XL, MaxVarX);
		MaxVarY = Max(YL, MaxVarY);
	}

	INT NeededX = ((MaxVarX + LO_DESC_X_PADDING) * (ObjInfo.Variables.Num() + ObjInfo.Events.Num())) + (2*LO_TEXT_BORDER);
	INT NeededY = MaxVarY + LO_TEXT_BORDER;

	return FIntPoint(NeededX, NeededY);
}


void FLinkedObjDrawUtils::DrawVariableConnectors(FRenderInterface* RI, FLinkedObjDrawInfo& ObjInfo, const FIntPoint& Pos, const FIntPoint& Size)
{
	// Do nothing here if no variables or event connectors.
	if( ObjInfo.Variables.Num() == 0 && ObjInfo.Events.Num() == 0 )
		return;

	UBOOL bHitTesting = RI->IsHitTesting();

	INT MaxVarX = 0;
	INT MaxVarY = 0;
	for(INT i=0; i<ObjInfo.Variables.Num(); i++)
	{
		INT XL, YL;
		RI->StringSize( GEngine->SmallFont, XL, YL, ObjInfo.Variables(i).Name );
		MaxVarX = Max(XL, MaxVarX);
		MaxVarY = Max(YL, MaxVarY);
	}
	for (INT idx = 0; idx < ObjInfo.Events.Num(); idx++)
	{
		INT XL, YL;
		RI->StringSize( GEngine->SmallFont, XL, YL, ObjInfo.Events(idx).Name );
		MaxVarX = Max(XL, MaxVarX);
		MaxVarY = Max(YL, MaxVarY);
	}
	MaxVarX = Max(MaxVarX, LO_CONNECTOR_WIDTH);

	INT VarWidth = MaxVarX + LO_DESC_X_PADDING;
	INT VarRangeX = Size.X - (2*LO_TEXT_BORDER);
	INT CenterX = Pos.X + LO_TEXT_BORDER + 0.5*VarRangeX;

	INT SpacingX = VarRangeX/(ObjInfo.Variables.Num()+ObjInfo.Events.Num());
	INT StartX = CenterX - 0.5*(ObjInfo.Variables.Num()+ObjInfo.Events.Num()-1)*SpacingX;
	INT LabelY = Pos.Y + Size.Y - LO_TEXT_BORDER - MaxVarY;

	ObjInfo.VariableX.Add( ObjInfo.Variables.Num() );
	for(INT i=0; i<ObjInfo.Variables.Num(); i++)
	{
		INT VarX = StartX + i * SpacingX;

		ObjInfo.VariableX(i) = VarX;

		if(bHitTesting) RI->SetHitProxy( new HLinkedObjConnectorProxy(ObjInfo.ObjObject, LOC_VARIABLE, i) );
		RI->DrawTile( VarX - 0.5*LO_CONNECTOR_WIDTH, Pos.Y + Size.Y, LO_CONNECTOR_WIDTH, LO_CONNECTOR_LENGTH, 0.f, 0.f, 0.f, 0.f, ObjInfo.Variables(i).Color );
		if(bHitTesting) RI->SetHitProxy( NULL );

		INT XL, YL;
		RI->StringSize( GEngine->SmallFont, XL, YL, ObjInfo.Variables(i).Name );
		RI->DrawShadowedString( VarX - 0.5*XL, LabelY, ObjInfo.Variables(i).Name, GEngine->SmallFont, FLinearColor::White );
	}

	// draw event connectors
	ObjInfo.EventX.Add( ObjInfo.Events.Num() );
	for (INT idx = 0; idx < ObjInfo.Events.Num(); idx++)
	{
		INT VarX = StartX + ((ObjInfo.Variables.Num()+idx) * SpacingX);

		ObjInfo.EventX(idx) = VarX;

		if(bHitTesting) RI->SetHitProxy( new HLinkedObjConnectorProxy(ObjInfo.ObjObject, LOC_EVENT, idx) );
		RI->DrawTile( VarX - 0.5*LO_CONNECTOR_WIDTH, Pos.Y + Size.Y, LO_CONNECTOR_WIDTH, LO_CONNECTOR_LENGTH, 0.f, 0.f, 0.f, 0.f, ObjInfo.Events(idx).Color );

		// draw an extra bar to indicate non-optional links
		RI->DrawTile( VarX - 0.5*LO_CONNECTOR_WIDTH - 1, Pos.Y + Size.Y, LO_CONNECTOR_WIDTH + 2, 2, 0.f, 0.f, 0.f, 0.f, FColor(0,0,0) );
		if(bHitTesting) RI->SetHitProxy( NULL );

		INT XL, YL;
		RI->StringSize( GEngine->SmallFont, XL, YL, ObjInfo.Events(idx).Name );
		RI->DrawShadowedString( VarX - 0.5*XL, LabelY, ObjInfo.Events(idx).Name, GEngine->SmallFont, FLinearColor::White );
	}
}

void FLinkedObjDrawUtils::DrawLinkedObj(FRenderInterface* RI, FLinkedObjDrawInfo& ObjInfo, const TCHAR* Name, const TCHAR* Comment, const FColor& BorderColor, const FColor& TitleBkgColor, const FIntPoint& Pos)
{
	UBOOL bHitTesting = RI->IsHitTesting();

	FIntPoint TitleSize = GetTitleBarSize(RI, Name);
	FIntPoint LogicSize = GetLogicConnectorsSize(RI, ObjInfo);
	FIntPoint VarSize = GetVariableConnectorsSize(RI, ObjInfo);

	ObjInfo.DrawWidth = Max3(TitleSize.X, LogicSize.X, VarSize.X);
	ObjInfo.DrawHeight = TitleSize.Y + LogicSize.Y + VarSize.Y + 3;

	if(bHitTesting) RI->SetHitProxy( new HLinkedObjProxy(ObjInfo.ObjObject) );

	DrawTitleBar(RI, Pos, FIntPoint(ObjInfo.DrawWidth, TitleSize.Y), BorderColor, TitleBkgColor, Name, Comment);

	RI->DrawTile(Pos.X,		Pos.Y + TitleSize.Y + 1,	ObjInfo.DrawWidth,		LogicSize.Y + VarSize.Y,		0.0f,0.0f,0.0f,0.0f, BorderColor );
	RI->DrawTile(Pos.X + 1,	Pos.Y + TitleSize.Y + 2,	ObjInfo.DrawWidth - 2,	LogicSize.Y + VarSize.Y - 2,	0.0f,0.0f,0.0f,0.0f, FColor(140,140,140) );

	if(bHitTesting) RI->SetHitProxy( NULL );

	DrawLogicConnectors(RI, ObjInfo, Pos + FIntPoint(0, TitleSize.Y + 1), FIntPoint(ObjInfo.DrawWidth, LogicSize.Y));
	DrawVariableConnectors(RI, ObjInfo, Pos + FIntPoint(0, TitleSize.Y + 1 + LogicSize.Y), FIntPoint(ObjInfo.DrawWidth, VarSize.Y));
}
