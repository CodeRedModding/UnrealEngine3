/*=============================================================================
	UnSequenceDraw.cpp: Utils for drawing sequence objects.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineSequenceClasses.h"
#include "UnLinkedObjDrawUtils.h"

#define LO_CIRCLE_SIDES			16

static const FColor SelectedColor(255,255,0);
static const FColor MouseOverLogicColor(225,225,0);
static const FLOAT	MouseOverColorScale(0.8f);
static const FColor	TitleBkgColor(112,112,112);
static const FColor	SequenceTitleBkgColor(112,112,200);


//-----------------------------------------------------------------------------
//	USequence
//-----------------------------------------------------------------------------

// Draw an entire gameplay sequence
void USequence::DrawSequence(FRenderInterface* RI, TArray<USequenceObject*>& SelectedSeqObjs, USequenceObject* MouseOverSeqObj, INT MouseOverConnType, INT MouseOverConnIndex, FLOAT MouseOverTime, UBOOL bCurves)
{
	// draw first 
	for(INT i=0; i<SequenceObjects.Num(); i++)
	{
		if( SequenceObjects(i)->bDrawFirst )
		{
			UBOOL bSelected = SelectedSeqObjs.ContainsItem( SequenceObjects(i) );
			UBOOL bMouseOver = (SequenceObjects(i) == MouseOverSeqObj);
			SequenceObjects(i)->DrawSeqObj(RI, bSelected, bMouseOver, MouseOverConnType, MouseOverConnIndex, MouseOverTime);
		}
	}

	// first pass draw most sequence ops
	for(INT i=0; i<SequenceObjects.Num(); i++)
	{
		if( !SequenceObjects(i)->bDrawFirst && !SequenceObjects(i)->bDrawLast )
		{
			UBOOL bSelected = SelectedSeqObjs.ContainsItem( SequenceObjects(i) );
			UBOOL bMouseOver = (SequenceObjects(i) == MouseOverSeqObj);
			SequenceObjects(i)->DrawSeqObj(RI, bSelected, bMouseOver, MouseOverConnType, MouseOverConnIndex, MouseOverTime);
		}
	}

	// draw logic and variable links
	for (INT i = 0; i < SequenceObjects.Num(); i++)
	{
		SequenceObjects(i)->DrawLogicLinks(RI, bCurves);
		SequenceObjects(i)->DrawVariableLinks(RI, bCurves);
	}

	// draw final layer, for variables etc
	for(INT i=0; i<SequenceObjects.Num(); i++)
	{
		if( SequenceObjects(i)->bDrawLast )
		{
			UBOOL bSelected = SelectedSeqObjs.ContainsItem( SequenceObjects(i) );
			UBOOL bMouseOver = (SequenceObjects(i) == MouseOverSeqObj);
			SequenceObjects(i)->DrawSeqObj(RI, bSelected, bMouseOver, MouseOverConnType, MouseOverConnIndex, MouseOverTime);
		}
	}
}

//-----------------------------------------------------------------------------
//	USequenceObject
//-----------------------------------------------------------------------------

FColor USequenceObject::GetBorderColor(UBOOL bSelected, UBOOL bMouseOver)
{
	if( bSelected )
	{
		return FColor(255,255,0);
	}
	else
	{
		if(bMouseOver)
		{
			return ObjColor;
		}
		else
		{
			return FColor( ObjColor.Plane() * MouseOverColorScale );
		}
	}
}

FIntPoint USequenceObject::GetTitleBarSize(FRenderInterface* RI)
{
	return FLinkedObjDrawUtils::GetTitleBarSize(RI, *ObjName);
}

void USequenceObject::DrawTitleBar(FRenderInterface* RI, UBOOL bSelected, UBOOL bMouseOver, const FIntPoint& Pos, const FIntPoint& Size)
{
	FColor BgkColor = TitleBkgColor;
	if( this->IsA(USequence::StaticClass()) )
	{
		BgkColor = SequenceTitleBkgColor;
	}

	if(ObjComment.Len() > 0)
	{
		FLinkedObjDrawUtils::DrawTitleBar( RI, Pos, Size, GetBorderColor(bSelected, bMouseOver), BgkColor, *ObjName, *ObjComment );
	}
	else
	{
		FLinkedObjDrawUtils::DrawTitleBar( RI, Pos, Size, GetBorderColor(bSelected, bMouseOver), BgkColor, *ObjName, NULL );
	}
}

/** Calculate the bounding box of this sequence object. For use by Kismet. */
FIntRect USequenceObject::GetSeqObjBoundingBox()
{
	return FIntRect(ObjPosX, ObjPosY, ObjPosX + DrawWidth, ObjPosY + DrawHeight);
}

//-----------------------------------------------------------------------------
//	USequenceOp
//-----------------------------------------------------------------------------

void USequenceOp::MakeLinkedObjDrawInfo(FLinkedObjDrawInfo& ObjInfo, INT MouseOverConnType, INT MouseOverConnIndex)
{
	// add all input links
	for(INT i=0; i<InputLinks.Num(); i++)
	{
		// only add if visible
		if (!InputLinks(i).bHidden)
		{
			FColor ConnColor = (MouseOverConnType == LOC_INPUT && MouseOverConnIndex == i) ? MouseOverLogicColor : FColor(0,0,0);
			ObjInfo.Inputs.AddItem( FLinkedObjConnInfo(*InputLinks(i).LinkDesc, ConnColor) );
		}
	}
	// add all output links
	for(INT i=0; i<OutputLinks.Num(); i++)
	{
		// only add if visible
		if (!OutputLinks(i).bHidden)
		{
			FColor ConnColor = (MouseOverConnType == LOC_OUTPUT && MouseOverConnIndex == i) ? MouseOverLogicColor : FColor(0,0,0);
			ObjInfo.Outputs.AddItem( FLinkedObjConnInfo(*OutputLinks(i).LinkDesc, ConnColor) );
		}
	}
	// add all variable links
	for(INT i=0; i<VariableLinks.Num(); i++)
	{
		// only add if visible
		if (!VariableLinks(i).bHidden)
		{
			FColor VarColor = GetVarConnectorColor(i);
			FColor ConnColor = (MouseOverConnType == LOC_VARIABLE && MouseOverConnIndex == i) ? VarColor : FColor( VarColor.Plane() * MouseOverColorScale );
			ObjInfo.Variables.AddItem( FLinkedObjConnInfo(*VariableLinks(i).LinkDesc, ConnColor) );
		}
	}
	// add all event links
	for(INT i=0; i<EventLinks.Num(); i++)
	{
		// only add if visible
		if (!EventLinks(i).bHidden)
		{
			FColor EventColor = FColor(255,0,0);
			FColor ConnColor = (MouseOverConnType == LOC_EVENT && MouseOverConnIndex == i) ? EventColor : FColor( EventColor.Plane() * MouseOverColorScale );
			ObjInfo.Events.AddItem( FLinkedObjConnInfo(*EventLinks(i).LinkDesc, ConnColor) );
		}
	}
	// set the object reference to this object, for later use
	ObjInfo.ObjObject = this;
}


FIntPoint USequenceOp::GetLogicConnectorsSize(FRenderInterface* RI, INT* InputY, INT* OutputY)
{
	FLinkedObjDrawInfo ObjInfo;
	MakeLinkedObjDrawInfo(ObjInfo);

	return FLinkedObjDrawUtils::GetLogicConnectorsSize(RI, ObjInfo, InputY, OutputY);
}

FIntPoint USequenceOp::GetVariableConnectorsSize(FRenderInterface* RI)
{
	FLinkedObjDrawInfo ObjInfo;
	MakeLinkedObjDrawInfo(ObjInfo);

	return FLinkedObjDrawUtils::GetVariableConnectorsSize(RI, ObjInfo);
}

void USequenceOp::DrawLogicConnectors(FRenderInterface* RI, const FIntPoint& Pos, const FIntPoint& Size, INT MouseOverConnType, INT MouseOverConnIndex)
{
	DrawWidth = Size.X;

	FLinkedObjDrawInfo ObjInfo;
	MakeLinkedObjDrawInfo(ObjInfo, MouseOverConnType, MouseOverConnIndex);

	FLinkedObjDrawUtils::DrawLogicConnectors( RI, ObjInfo, Pos, Size );

	INT linkIdx = 0;
	for(INT i=0; i<InputLinks.Num(); i++)
	{
		if (!InputLinks(i).bHidden)
		{
			InputLinks(i).DrawY = ObjInfo.InputY(linkIdx);
			linkIdx++;
		}
	}

	linkIdx = 0;
	for(INT i=0; i<OutputLinks.Num(); i++)
	{
		if (!OutputLinks(i).bHidden)
		{
			OutputLinks(i).DrawY = ObjInfo.OutputY(linkIdx);
			linkIdx++;
		}
	}
}

FColor USequenceOp::GetVarConnectorColor(INT ConnIndex)
{
	if( ConnIndex < 0 || ConnIndex >= VariableLinks.Num() )
		return FColor(0,0,0);

	FSeqVarLink* VarLink = &VariableLinks(ConnIndex);

	if(VarLink->ExpectedType == NULL)
		return FColor(0,0,0);

	USequenceVariable* DefVar = (USequenceVariable*)VarLink->ExpectedType->GetDefaultObject();
	return DefVar->ObjColor;
}


void USequenceOp::DrawVariableConnectors(FRenderInterface* RI, const FIntPoint& Pos, const FIntPoint& Size, INT MouseOverConnType, INT MouseOverConnIndex)
{
	DrawHeight = (Pos.Y + Size.Y) - ObjPosY;

	FLinkedObjDrawInfo ObjInfo;
	MakeLinkedObjDrawInfo(ObjInfo, MouseOverConnType, MouseOverConnIndex);

	FLinkedObjDrawUtils::DrawVariableConnectors( RI, ObjInfo, Pos, Size );

	INT idx, linkIdx = 0;
	for (idx = 0; idx < VariableLinks.Num(); idx++)
	{
		if (!VariableLinks(idx).bHidden)
		{
			VariableLinks(idx).DrawX = ObjInfo.VariableX(linkIdx);
			linkIdx++;
		}
	}

	linkIdx = 0;
	for (idx = 0; idx < EventLinks.Num(); idx++)
	{
		if (!EventLinks(idx).bHidden)
		{
			EventLinks(idx).DrawX = ObjInfo.EventX(linkIdx);
			linkIdx++;
		}
	}
}

void USequenceOp::DrawLogicLinks(FRenderInterface* RI, UBOOL bCurves)
{
	// for each valid input link,
	for(INT i=0; i<OutputLinks.Num(); i++)
	{
		FSeqOpOutputLink &link = OutputLinks(i);
		// grab the start point for this line
		FIntPoint Start = this->GetConnectionLocation(LOC_OUTPUT, i);
		// iterate through all linked inputs,
		for (INT linkIdx = 0; linkIdx < link.Links.Num(); linkIdx++)
		{
			FSeqOpOutputInputLink &inLink = link.Links(linkIdx);
			if (inLink.LinkedOp != NULL &&
				inLink.InputLinkIdx >= 0 &&
				inLink.InputLinkIdx < inLink.LinkedOp->InputLinks.Num())
			{
				// grab the end point
				FIntPoint End = inLink.LinkedOp->GetConnectionLocation(LOC_INPUT, inLink.InputLinkIdx);
				FColor LineColor = link.bHasImpulse ? FColor(255,0,0) : FColor(0,0,0);

				if(bCurves)
				{
					FLOAT Tension = Abs<INT>(Start.X - End.X);
					//FLOAT Tension = 100.f;
					FLinkedObjDrawUtils::DrawSpline(RI, Start, Tension * FVector2D(1,0), End, Tension * FVector2D(1,0), LineColor, true);
				}
				else
				{
					// Draw the line, highlighted if it has an impulse
					RI->DrawLine2D( Start.X, Start.Y, End.X, End.Y, LineColor);

					// Make a pretty little arrow!
					FVector2D Dir = (FVector2D(End) - FVector2D(Start)).Normalize();
					FLinkedObjDrawUtils::DrawArrowhead(RI, End, Dir, LineColor);
				}
			}
		}
	}
}

/* epic ===============================================
* ::DrawVariableLinks
*
* Draws the link between all the variable connectors for
* this op and any attached variables.
*
* =====================================================
*/
void USequenceOp::DrawVariableLinks(FRenderInterface* RI, UBOOL bCurves)
{
	for(INT i=0; i<VariableLinks.Num(); i++)
	{
		FIntPoint Start = this->GetConnectionLocation(LOC_VARIABLE, i);
		FSeqVarLink &VarLink = VariableLinks(i);

		// draw links for each variable connected to this connection
		for (INT idx = 0; idx < VarLink.LinkedVariables.Num(); idx++)
		{
			FIntPoint End = VarLink.LinkedVariables(idx)->GetVarConnectionLocation();
			FColor LinkColor = FColor(VarLink.LinkedVariables(idx)->ObjColor.Plane() * MouseOverColorScale);

			if(bCurves)
			{
				FLOAT Tension = Abs<INT>(Start.Y - End.Y);

				if(!VarLink.bWriteable)
				{
					FLinkedObjDrawUtils::DrawSpline(RI, End, FVector2D(0,0), Start, Tension*FVector2D(0,-1), LinkColor, true);
				}
				else
				{
					FLinkedObjDrawUtils::DrawSpline(RI, Start, Tension*FVector2D(0,1), End, FVector2D(0,0), LinkColor, true);
				}
			}
			else
			{
				RI->DrawLine2D( Start.X, Start.Y, End.X, End.Y, LinkColor);

				// make a pretty little arrow!
				if (!VarLink.bWriteable)
				{
					FVector2D Dir = (FVector2D(Start) - FVector2D(End)).Normalize();
					FLinkedObjDrawUtils::DrawArrowhead(RI, Start, Dir, LinkColor );
				}
				else
				{
					FVector2D Dir = (FVector2D(End) - FVector2D(Start)).Normalize();
					FLinkedObjDrawUtils::DrawArrowhead(RI, End, Dir, LinkColor );
				}
			}
		}
	}
	for (INT i = 0; i < EventLinks.Num(); i++)
	{
		FIntPoint Start = this->GetConnectionLocation(LOC_EVENT, i);

		// draw links for each variable connected to this connection
		for (INT idx = 0; idx < EventLinks(i).LinkedEvents.Num(); idx++)
		{
			FIntPoint End = EventLinks(i).LinkedEvents(idx)->GetCenterPoint(RI);

			if(bCurves)
			{
				FLOAT Tension = Abs<INT>(Start.Y - End.Y);
				FLinkedObjDrawUtils::DrawSpline(RI, Start, Tension*FVector2D(0,1), End, Tension*FVector2D(0,1), FColor(255,0,0), true);
			}
			else
			{
				RI->DrawLine2D( Start.X, Start.Y, End.X, End.Y, FColor(255,0,0));

				FVector2D Dir = (FVector2D(End) - FVector2D(Start)).Normalize();
				FLinkedObjDrawUtils::DrawArrowhead(RI, End, Dir, FColor(255,0,0) );
			}
		}
	}
}

FIntPoint USequenceOp::GetConnectionLocation(INT Type, INT ConnIndex)
{
	if(Type == LOC_INPUT)
	{
		if( ConnIndex < 0 || ConnIndex >= InputLinks.Num() )
			return FIntPoint(0,0);

		return FIntPoint( ObjPosX - LO_CONNECTOR_LENGTH, InputLinks(ConnIndex).DrawY );
	}
	else if(Type == LOC_OUTPUT)
	{
		if( ConnIndex < 0 || ConnIndex >= OutputLinks.Num() )
			return FIntPoint(0,0);

		return FIntPoint( ObjPosX + DrawWidth + LO_CONNECTOR_LENGTH, OutputLinks(ConnIndex).DrawY );
	}
	else
	if (Type == LOC_VARIABLE)
	{
		if( ConnIndex < 0 || ConnIndex >= VariableLinks.Num() )
			return FIntPoint(0,0);

		return FIntPoint( VariableLinks(ConnIndex).DrawX, ObjPosY + DrawHeight + LO_CONNECTOR_LENGTH);
	}
	else
	if (Type == LOC_EVENT)
	{
		if (ConnIndex >= 0 &&
			ConnIndex < EventLinks.Num())
		{
			return FIntPoint(EventLinks(ConnIndex).DrawX, ObjPosY + DrawHeight + LO_CONNECTOR_LENGTH);
		}
	}
	return FIntPoint(0,0);
}

void USequenceOp::DrawSeqObj(FRenderInterface* RI, UBOOL bSelected, UBOOL bMouseOver, INT MouseOverConnType, INT MouseOverConnIndex, FLOAT MouseOverTime)
{
	UBOOL bHitTesting = RI->IsHitTesting();

	FIntPoint TitleSize = GetTitleBarSize(RI);
	FIntPoint LogicSize = GetLogicConnectorsSize(RI);
	FIntPoint VarSize = GetVariableConnectorsSize(RI);

	INT Width = Max3(TitleSize.X, LogicSize.X, VarSize.X);
	INT Height = TitleSize.Y + LogicSize.Y + VarSize.Y + 3;

	if(bHitTesting) RI->SetHitProxy( new HLinkedObjProxy(this) );

	DrawTitleBar(RI, bSelected, bMouseOver, FIntPoint(ObjPosX, ObjPosY), FIntPoint(Width, TitleSize.Y));

	RI->DrawTile(ObjPosX,		ObjPosY + TitleSize.Y + 1,	Width,		LogicSize.Y + VarSize.Y,		0.0f,0.0f,0.0f,0.0f, GetBorderColor(bSelected, bMouseOver) );
	RI->DrawTile(ObjPosX + 1,	ObjPosY + TitleSize.Y + 2,	Width - 2,	LogicSize.Y + VarSize.Y - 2,	0.0f,0.0f,0.0f,0.0f, FColor(140,140,140) );

	if(bHitTesting) RI->SetHitProxy( NULL );

	if(bMouseOver)
	{
		DrawLogicConnectors(	RI, FIntPoint(ObjPosX, ObjPosY + TitleSize.Y + 1),					FIntPoint(Width, LogicSize.Y), MouseOverConnType, MouseOverConnIndex);
		DrawVariableConnectors(	RI, FIntPoint(ObjPosX, ObjPosY + TitleSize.Y + 1 + LogicSize.Y),	FIntPoint(Width, VarSize.Y), MouseOverConnType, MouseOverConnIndex);
	}
	else
	{
		DrawLogicConnectors(	RI, FIntPoint(ObjPosX, ObjPosY + TitleSize.Y + 1),					FIntPoint(Width, LogicSize.Y), -1, INDEX_NONE);
		DrawVariableConnectors(	RI, FIntPoint(ObjPosX, ObjPosY + TitleSize.Y + 1 + LogicSize.Y),	FIntPoint(Width, VarSize.Y), -1, INDEX_NONE);
	}
}

//-----------------------------------------------------------------------------
//	USequenceEvent
//-----------------------------------------------------------------------------

FIntPoint USequenceEvent::GetCenterPoint(FRenderInterface* RI)
{
	return FIntPoint(ObjPosX + 0.5*MaxWidth, ObjPosY);
}

FIntRect USequenceEvent::GetSeqObjBoundingBox()
{
	return FIntRect(ObjPosX, ObjPosY, ObjPosX + MaxWidth, ObjPosY + DrawHeight);
}	

void USequenceEvent::DrawSeqObj(FRenderInterface* RI, UBOOL bSelected, UBOOL bMouseOver, INT MouseOverConnType, INT MouseOverConnIndex, FLOAT MouseOverTime)
{
	UBOOL bHitTesting = RI->IsHitTesting();
	FColor BorderColor = GetBorderColor(bSelected, bMouseOver);

	FIntPoint TitleSize = GetTitleBarSize(RI);
	FIntPoint VarSize = GetVariableConnectorsSize(RI);

	MaxWidth = Max3(TitleSize.X, LO_MIN_SHAPE_SIZE, VarSize.X);
	INT Height = TitleSize.Y + LO_MIN_SHAPE_SIZE + 3 + VarSize.Y;

	FIntPoint DiamondCenter(ObjPosX + 0.5*MaxWidth, ObjPosY + TitleSize.Y + 1 + 0.5*LO_MIN_SHAPE_SIZE);

	// Draw a single output connector
	check(OutputLinks.Num() == 1);

	FLinearColor ConnectorColor = (bMouseOver && MouseOverConnType == LOC_OUTPUT) ? MouseOverLogicColor : FLinearColor::Black;

	if(bHitTesting) RI->SetHitProxy( new HLinkedObjConnectorProxy(this, LOC_OUTPUT, 0) );
	RI->DrawTile(DiamondCenter.X + 0.5*LO_MIN_SHAPE_SIZE - LO_CONNECTOR_LENGTH,
		DiamondCenter.Y - 0.5*LO_CONNECTOR_WIDTH,
		2 * LO_CONNECTOR_LENGTH, LO_CONNECTOR_WIDTH,
		0.f, 0.f, 0.f, 0.f, ConnectorColor );
	if(bHitTesting) RI->SetHitProxy( NULL );

	OutputLinks(0).DrawY = DiamondCenter.Y;

	// Draw diamond
	if(bHitTesting) RI->SetHitProxy( new HLinkedObjProxy(this) );

	DrawTitleBar(RI, bSelected, bMouseOver, FIntPoint(ObjPosX, ObjPosY), FIntPoint(MaxWidth, TitleSize.Y));

	DrawWidth = 0.5*MaxWidth + 0.5*LO_MIN_SHAPE_SIZE;

	FLinkedObjDrawUtils::DrawNGon( RI, DiamondCenter, BorderColor, 4, 0.5*LO_MIN_SHAPE_SIZE );
	FLinkedObjDrawUtils::DrawNGon( RI, DiamondCenter, FColor(140,140,140), 4, (0.5*LO_MIN_SHAPE_SIZE)-1 );

	// try to draw the originator's sprite icon
	if (Originator != NULL)
	{
		USpriteComponent *spriteComponent = NULL;
		for (INT idx = 0; idx < Originator->Components.Num() && spriteComponent == NULL; idx++)
		{
			// check if this is a sprite component
			spriteComponent = Cast<USpriteComponent>(Originator->Components(idx));
		}
		if (spriteComponent != NULL)
		{
			UTexture2D *sprite = spriteComponent->Sprite;
			RI->DrawTile(DiamondCenter.X - (0.5f*sprite->SizeX),DiamondCenter.Y - (0.5f*sprite->SizeY),
						 sprite->SizeX,sprite->SizeY,
						 0.f,0.f,
						 1.f,1.f,
						 FColor(128,128,128,255),
						 sprite);
		}
	}

	// If there are any variable connectors visible, draw the the box under the event that contains them.
	INT NumVisibleVarLinks = 0;
	for(INT i=0; i<VariableLinks.Num(); i++)
	{
		if(!VariableLinks(i).bHidden)
		{
			NumVisibleVarLinks++;
		}
	}

	if(NumVisibleVarLinks > 0)
	{
		// Draw the variable connector bar at the bottom
		RI->DrawTile( ObjPosX, DiamondCenter.Y+(0.5*LO_MIN_SHAPE_SIZE)+2, MaxWidth, VarSize.Y, 0.f, 0.f, 0.f, 0.f, BorderColor );
		RI->DrawTile( ObjPosX+1, DiamondCenter.Y+(0.5*LO_MIN_SHAPE_SIZE)+2+1, MaxWidth-2, VarSize.Y-2, 0.f, 0.f, 0.f, 0.f, FColor(140,140,140) );

		if(bMouseOver)
		{
			DrawVariableConnectors( RI, FIntPoint(ObjPosX, DiamondCenter.Y+(0.5*LO_MIN_SHAPE_SIZE)+2), FIntPoint(MaxWidth, VarSize.Y), MouseOverConnType, MouseOverConnIndex );
		}
		else
		{
			DrawVariableConnectors( RI, FIntPoint(ObjPosX, DiamondCenter.Y+(0.5*LO_MIN_SHAPE_SIZE)+2), FIntPoint(MaxWidth, VarSize.Y), -1, INDEX_NONE );
		}
	}

	if(bHitTesting) RI->SetHitProxy( NULL );

	INT XL, YL;
	RI->StringSize(GEngine->SmallFont, XL, YL, *OutputLinks(0).LinkDesc);

	if(RI->Zoom2D < 1.f)
		RI->DrawString(DiamondCenter.X + 0.5*LO_MIN_SHAPE_SIZE - XL - 2*LO_TEXT_BORDER, DiamondCenter.Y - 0.5*YL,
			*OutputLinks(0).LinkDesc, GEngine->SmallFont, FLinearColor::White );
	else
		RI->DrawShadowedString(DiamondCenter.X + 0.5*LO_MIN_SHAPE_SIZE - XL - 2*LO_TEXT_BORDER, DiamondCenter.Y - 0.5*YL,
			*OutputLinks(0).LinkDesc, GEngine->SmallFont, FLinearColor::White );
}

//-----------------------------------------------------------------------------
//	USequenceVariable
//-----------------------------------------------------------------------------

void USequenceVariable::DrawSeqObj(FRenderInterface* RI, UBOOL bSelected, UBOOL bMouseOver, INT MouseOverConnType, INT MouseOverConnIndex, FLOAT MouseOverTime)
{
	UBOOL bHitTesting = RI->IsHitTesting();
	FColor BorderColor = GetBorderColor(bSelected, bMouseOver);

	if(bHitTesting) RI->SetHitProxy( new HLinkedObjProxy(this) );

	// Draw comment for variable
	if(ObjComment.Len() > 0)
	{
		INT XL, YL;
		RI->StringSize( GEngine->SmallFont, XL, YL, *ObjComment );

		INT CommentX = ObjPosX + 2;
		INT CommentY = ObjPosY - YL - 2;

		if(RI->Zoom2D < 1.f)
			RI->DrawString( CommentX, CommentY, *ObjComment, GEngine->SmallFont, FColor(64,64,192) );
		else
			RI->DrawShadowedString( CommentX, CommentY, *ObjComment, GEngine->SmallFont, FColor(64,64,192) );
	}

	// Draw circle
	FIntPoint CircleCenter(ObjPosX + 0.5*LO_MIN_SHAPE_SIZE, ObjPosY + 0.5*LO_MIN_SHAPE_SIZE);

	DrawWidth = CircleCenter.X - ObjPosX;
	DrawHeight = CircleCenter.Y - ObjPosY;

	FLinkedObjDrawUtils::DrawNGon( RI, CircleCenter, BorderColor, LO_CIRCLE_SIDES, 0.5*LO_MIN_SHAPE_SIZE );
	FLinkedObjDrawUtils::DrawNGon( RI, CircleCenter, FColor(140,140,140), LO_CIRCLE_SIDES, (0.5*LO_MIN_SHAPE_SIZE)-1 );

	// Give subclasses a chance to draw extra stuff
	DrawExtraInfo(RI, CircleCenter);

	// Draw the actual value of the variable in the middle of the circle
	FString VarString = GetValueStr();

	INT XL, YL;
	RI->StringSize(GEngine->SmallFont, XL, YL, *VarString);

	// check if we need to elipse the string due to width constraint
	if (!bSelected &&
		VarString.Len() > 8 &&
		XL > LO_MIN_SHAPE_SIZE &&
		!this->IsA(USeqVar_Named::StaticClass()))
	{
		//TODO: calculate the optimal number of chars to fit instead of hard-coded
		VarString = VarString.Left(4) + TEXT("..") + VarString.Right(4);
		RI->StringSize(GEngine->SmallFont, XL, YL, *VarString);
	}

	if(RI->Zoom2D < 1.f)
		RI->DrawString(CircleCenter.X - 0.5*XL, CircleCenter.Y - 0.5*YL, *VarString, GEngine->SmallFont, FLinearColor::White );
	else
		RI->DrawShadowedString(CircleCenter.X - 0.5*XL, CircleCenter.Y - 0.5*YL, *VarString, GEngine->SmallFont, FLinearColor::White );

	// If this variable has a name, write it underneath the variable
	if(VarName != NAME_None)
	{
		RI->StringSize(GEngine->SmallFont, XL, YL, *VarName);

		if(RI->Zoom2D < 1.f)
			RI->DrawString(CircleCenter.X - 0.5*XL, ObjPosY + LO_MIN_SHAPE_SIZE + 2, *VarName, GEngine->SmallFont, FColor(255,64,64) );
		else
			RI->DrawShadowedString(CircleCenter.X - 0.5*XL, ObjPosY + LO_MIN_SHAPE_SIZE + 2, *VarName, GEngine->SmallFont, FColor(255,64,64) );
	}

	if(bHitTesting) RI->SetHitProxy( NULL );
}

FIntPoint USequenceVariable::GetVarConnectionLocation()
{
	return FIntPoint(DrawWidth + ObjPosX, DrawHeight + ObjPosY);
}

FIntRect USequenceVariable::GetSeqObjBoundingBox()
{
	return FIntRect(ObjPosX, ObjPosY, ObjPosX + LO_MIN_SHAPE_SIZE, ObjPosY + LO_MIN_SHAPE_SIZE);
}

//-----------------------------------------------------------------------------
//	USeqVar_Object
//-----------------------------------------------------------------------------

void USeqVar_Object::DrawExtraInfo(FRenderInterface* RI, const FVector& CircleCenter)
{
	// try to draw the object's sprite icon
	AActor *Originator = Cast<AActor>(ObjValue);
	if (Originator != NULL)
	{
		USpriteComponent *spriteComponent = NULL;
		for (INT idx = 0; idx < Originator->Components.Num() && spriteComponent == NULL; idx++)
		{
			// check if this is a sprite component
			spriteComponent = Cast<USpriteComponent>(Originator->Components(idx));
		}
		if (spriteComponent != NULL)
		{
			UTexture2D *sprite = spriteComponent->Sprite;
			RI->DrawTile(CircleCenter.X - (0.5f*sprite->SizeX),CircleCenter.Y - (0.5f*sprite->SizeY),
						 sprite->SizeX,sprite->SizeY,
						 0.f,0.f,
						 1.f,1.f,
						 FColor(128,128,128,255),
						 sprite);
		}
	}
}

//-----------------------------------------------------------------------------
//	USeqVar_Named
//-----------------------------------------------------------------------------

/** Draw cross or tick to indicate status of this USeqVar_Named. */
void USeqVar_Named::DrawExtraInfo(FRenderInterface* RI, const FVector& CircleCenter)
{
	// Ensure materials are there.
	if(!GEngine->TickMaterial || !GEngine->CrossMaterial)
	{
		return;
	}

	UMaterial* UseMaterial = bStatusIsOk ? GEngine->TickMaterial : GEngine->CrossMaterial;
	RI->DrawTile( CircleCenter.X-16, CircleCenter.Y-16, 32, 32, 0.f, 0.f, 1.f, 1.f, UseMaterial->GetMaterialInterface(false), UseMaterial->GetInstanceInterface() );
}


//-----------------------------------------------------------------------------
//	USequenceFrame
//-----------------------------------------------------------------------------

void USequenceFrame::DrawSeqObj(FRenderInterface* RI, UBOOL bSelected, UBOOL bMouseOver, INT MouseOverConnType, INT MouseOverConnIndex, FLOAT MouseOverTime)
{
	UBOOL bHitTesting = RI->IsHitTesting();
	
	// Draw box
	if(bDrawBox)
	{
		// Draw filled center if desired.
		if(bFilled)
		{
			// If texture, use it...
			if(FillMaterial)
			{
				// Tiling is every 64 pixels.
				if(bTileFill)
				{
					RI->DrawTile( ObjPosX, ObjPosY, SizeX, SizeY, 0.f, 0.f, (FLOAT)SizeX/64.f, (FLOAT)SizeY/64.f, FillMaterial->GetMaterialInterface(false), FillMaterial->GetInstanceInterface() );
				}
				else
				{
					RI->DrawTile( ObjPosX, ObjPosY, SizeX, SizeY, 0.f, 0.f, 1.f, 1.f, FillMaterial->GetMaterialInterface(false), FillMaterial->GetInstanceInterface() );
				}
			}
			else if(FillTexture)
			{
				if(bTileFill)
				{
					RI->DrawTile( ObjPosX, ObjPosY, SizeX, SizeY, 0.f, 0.f, (FLOAT)SizeX/64.f, (FLOAT)SizeY/64.f, FillColor, FillTexture, false );				
				}
				else
				{
					RI->DrawTile( ObjPosX, ObjPosY, SizeX, SizeY, 0.f, 0.f, 1.f, 1.f, FillColor, FillTexture, false );
				}
			}
			// .. otherwise just use a solid color.
			else
			{
				RI->DrawTile( ObjPosX, ObjPosY, SizeX, SizeY, 0.f, 0.f, 1.f, 1.f, FillColor );
			}
		}

		// Draw frame
		FColor FrameColor = bSelected ? FColor(255,255,0) : BorderColor;

		INT MinDim = Min(SizeX, SizeY);
		INT UseBorderWidth = Clamp( BorderWidth, 0, (MinDim/2)-3 );

		for(INT i=0; i<UseBorderWidth; i++)
		{
			RI->DrawLine2D( ObjPosX,				ObjPosY + i,			ObjPosX + SizeX,		ObjPosY + i,			FrameColor );
			RI->DrawLine2D( ObjPosX + SizeX - i,	ObjPosY,				ObjPosX + SizeX - i,	ObjPosY + SizeY,		FrameColor );
			RI->DrawLine2D( ObjPosX + SizeX,		ObjPosY + SizeY - i,	ObjPosX,				ObjPosY + SizeY - i,	FrameColor );
			RI->DrawLine2D( ObjPosX + i,			ObjPosY + SizeY,		ObjPosX + i,			ObjPosY - 1,			FrameColor );
		}

		// If selected, draw little sizing triangle in bottom left.
		if(bSelected)
		{
			const INT HandleSize = 20;
			FIntPoint A(ObjPosX + SizeX,				ObjPosY + SizeY);
			FIntPoint B(ObjPosX + SizeX,				ObjPosY + SizeY - HandleSize);
			FIntPoint C(ObjPosX + SizeX - HandleSize,	ObjPosY + SizeY);

			if(bHitTesting)  RI->SetHitProxy( new HLinkedObjProxySpecial(this, 1) );
			RI->DrawTriangle2D( A, 0.f, 0.f, B, 0.f, 0.f, C, 0.f, 0.f, FColor(0,0,0) );
			if(bHitTesting)  RI->SetHitProxy( NULL );
		}
	}

	// Draw comment text

	// Check there are some valid chars in string. If not - we can't select it! So we force it back to default.
	INT NumProperChars = 0;
	for(INT i=0; i<ObjComment.Len(); i++)
	{
		if(ObjComment[i] != ' ')
		{
			NumProperChars++;
		}
	}

	if(NumProperChars == 0)
	{
		ObjComment = TEXT("Comment");
	}

	// We only set the hit proxy for the comment text.
	if(bHitTesting)  RI->SetHitProxy( new HLinkedObjProxy(this) );

	FLOAT OldZoom2D = RI->Zoom2D;

	INT XL, YL;
	RI->StringSize( GEngine->SmallFont, XL, YL, *ObjComment );

	// We always draw comment-box text at normal size (don't scale it as we zoom in and out.)
	INT x = ObjPosX*OldZoom2D + 2;
	INT y = ObjPosY*OldZoom2D - YL - 2;

	RI->SetZoom2D(1.f);
	RI->DrawShadowedString( x, y, *ObjComment, GEngine->SmallFont, FColor(64,64,192) );
	RI->SetZoom2D(OldZoom2D);

	if(bHitTesting) RI->SetHitProxy( NULL );

	// Fill in base SequenceObject rendering info (used by bounding box calculation).
	DrawWidth = SizeX;
	DrawHeight = SizeY;
}
