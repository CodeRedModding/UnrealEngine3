/*=============================================================================
	UnSequenceDraw.cpp: Utils for drawing sequence objects.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnLinkedObjDrawUtils.h"

#define LO_CIRCLE_SIDES			16
#define DEBUG_ACTIVE_BORDER_WIDTH 2

static const FColor SelectedColor(255,255,0);
static const FColor MouseOverLogicColor(225,225,0);
static const FColor	TitleBkgColor(112,112,112);
static const FColor	SequenceTitleBkgColor(112,112,200);
static const FColor	MatineeTitleBkgColor(200,112,0);
static const FColor	DebuggerActiveColor(255,255,255);
static const FLOAT	MouseOverColorScale(0.8f);
static const FLOAT	DebugColorFadeEndTime(10.0f);

/** Background color when a node has been deprecated */
static const FColor KismetDeprecatedBGColor(200,0,0);

extern UBOOL GKismetRealtimeDebugging;

//-----------------------------------------------------------------------------
//	USequence
//-----------------------------------------------------------------------------

// Draw an entire gameplay sequence
void USequence::DrawSequence(FCanvas* Canvas, TArray<USequenceObject*>& SelectedSeqObjs, USequenceObject* MouseOverSeqObj, INT MouseOverConnType, INT MouseOverConnIndex, FLOAT MouseOverTime)
{
	// Cache sequence object selection status.
	TArray<UBOOL> SelectionStatus;
	for(INT i=0; i<SequenceObjects.Num(); i++)
	{
		const UBOOL bSelected = SelectedSeqObjs.ContainsItem( SequenceObjects(i) );
		SelectionStatus.AddItem( bSelected );
	}

	// draw first 
	for(INT i=0; i<SequenceObjects.Num(); i++)
	{
		if( SequenceObjects(i)->bDrawFirst )
		{
			const UBOOL bSelected = SelectionStatus(i);
			const UBOOL bMouseOver = (SequenceObjects(i) == MouseOverSeqObj);
			SequenceObjects(i)->DrawSeqObj(Canvas, bSelected, bMouseOver, MouseOverConnType, MouseOverConnIndex, MouseOverTime);
		}
	}

	// first pass draw most sequence ops
	for(INT i=0; i<SequenceObjects.Num(); i++)
	{
		if( !SequenceObjects(i)->bDrawFirst && !SequenceObjects(i)->bDrawLast )
		{
			const UBOOL bSelected = SelectionStatus(i);
			const UBOOL bMouseOver = (SequenceObjects(i) == MouseOverSeqObj);
			SequenceObjects(i)->DrawSeqObj(Canvas, bSelected, bMouseOver, MouseOverConnType, MouseOverConnIndex, MouseOverTime);
		}
	}

	// draw logic and variable Links
	for (INT i = 0; i < SequenceObjects.Num(); i++)
	{
		SequenceObjects(i)->DrawLogicLinks(Canvas, SelectedSeqObjs, MouseOverSeqObj, MouseOverConnType, MouseOverConnIndex);
		SequenceObjects(i)->DrawVariableLinks(Canvas, SelectedSeqObjs, MouseOverSeqObj, MouseOverConnType, MouseOverConnIndex);
	}

	// draw final layer, for variables etc
	if (!GKismetRealtimeDebugging)
	{
		for(INT i=0; i<SequenceObjects.Num(); i++)
		{
			if( SequenceObjects(i)->bDrawLast )
			{
				const UBOOL bSelected = SelectionStatus(i);
				const UBOOL bMouseOver = (SequenceObjects(i) == MouseOverSeqObj);
				SequenceObjects(i)->DrawSeqObj(Canvas, bSelected, bMouseOver, MouseOverConnType, MouseOverConnIndex, MouseOverTime);
			}
		}
	}
	else
	{
		// draw non-sequence op objects first
		for(INT i=0; i<SequenceObjects.Num(); i++)
		{
			if( SequenceObjects(i)->bDrawLast )
			{
				USequenceOp* SequenceOp = Cast<USequenceOp>(SequenceObjects(i));
				if( !SequenceOp )
				{
					const UBOOL bSelected = SelectionStatus(i);
					const UBOOL bMouseOver = (SequenceObjects(i) == MouseOverSeqObj);
					SequenceObjects(i)->DrawSeqObj(Canvas, bSelected, bMouseOver, MouseOverConnType, MouseOverConnIndex, MouseOverTime);
				}
			}
		}

		// then draw sequence op objects last
		for(INT i=0; i<SequenceObjects.Num(); i++)
		{
			if( SequenceObjects(i)->bDrawLast )
			{
				USequenceOp* SequenceOp = Cast<USequenceOp>(SequenceObjects(i));
				if( SequenceOp )
				{
					const UBOOL bSelected = SelectionStatus(i);
					const UBOOL bMouseOver = (SequenceObjects(i) == MouseOverSeqObj);
					SequenceObjects(i)->DrawSeqObj(Canvas, bSelected, bMouseOver, MouseOverConnType, MouseOverConnIndex, MouseOverTime);
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	USequenceObject
//-----------------------------------------------------------------------------

FColor USequenceObject::GetBorderColor(UBOOL bSelected, UBOOL bMouseOver)
{
	FColor ObjDrawColor;

	if( bSelected )
	{
		ObjDrawColor = FColor(255,255,0);
	}
	else
	{
		if(bMouseOver)
		{
			ObjDrawColor = ObjColor;
		}
		else
		{
			ObjDrawColor = FColor( FLinearColor(ObjColor) * MouseOverColorScale );
		}
	}

#if _WINDOWS
	// If Kismet realtime debugging is active
	if( GKismetRealtimeDebugging && GEditor->PlayWorld && PIESequenceObject)
	{
		USequenceOp* SequenceOp = Cast<USequenceOp>(PIESequenceObject);
		if(SequenceOp)
		{
			// There are two separate border color behaviors, one when running and one when paused/stepping
			if (GEditor->PlayWorld->GetWorldInfo()->bDebugPauseExecution)
			{
				if (SequenceOp->bActive || SequenceOp->bIsActivated)
				{
					ObjDrawColor = DebuggerActiveColor;

					// The game is paused but it is the next game tick, reset the debugger state of this link
					if (SequenceOp->bIsActivated && ((GEditor->PlayWorld->GetTimeSeconds() - SequenceOp->PIEActivationTime) >= GWorld->GetDeltaSeconds()))
					{
						SequenceOp->bIsActivated = FALSE;
					}
				}
				else
				{
					SequenceOp->bIsActivated = FALSE;
				}
			}
			else
			{
				if (SequenceOp->bActive && SequenceOp->bIsActivated)
				{
					ObjDrawColor = DebuggerActiveColor;
				}
				else if (!SequenceOp->bActive && SequenceOp->bIsActivated)
				{
					// When the game is running, we want to fade the node's color from the "activated" color to it's normal color
					FLOAT TimeDiff = GEditor->PlayWorld->GetTimeSeconds() - SequenceOp->PIEActivationTime;
					if (TimeDiff < DebugColorFadeEndTime)
					{
						FLOAT LerpVal = TimeDiff / DebugColorFadeEndTime;
						BYTE LerpR = (BYTE)Lerp((FLOAT)DebuggerActiveColor.R, (FLOAT)ObjDrawColor.R, LerpVal);
						BYTE LerpG = (BYTE)Lerp((FLOAT)DebuggerActiveColor.G, (FLOAT)ObjDrawColor.G, LerpVal);
						BYTE LerpB = (BYTE)Lerp((FLOAT)DebuggerActiveColor.B, (FLOAT)ObjDrawColor.B, LerpVal);
						ObjDrawColor = FColor(LerpR,LerpG,LerpB);
					}
					else
					{
						// Color has faded to normal, reset the debugger state
						SequenceOp->bIsActivated = FALSE;
					}
				}
				else
				{
					SequenceOp->bIsActivated = FALSE;
				}
			}
		}
	}
#endif

	return ObjDrawColor;
}

FIntPoint USequenceObject::GetTitleBarSize(FCanvas* Canvas)
{
	// remove the category from the title
	FString Title = GetDisplayTitle();
	return FLinkedObjDrawUtils::GetTitleBarSize(Canvas, *Title);
}

FString USequenceObject::GetDisplayTitle() const
{
	return ObjName;
}

FString USequenceObject::GetAutoComment() const
{
	FString AutoComment;

	for( TFieldIterator<UProperty> It( GetClass() ); It; ++It )
	{
		UProperty* Property = *It;
		if(Property && (Property->PropertyFlags & CPF_Edit))
		{
			FString MetaString = Property->GetMetaData(TEXT("autocomment"));
			if(MetaString.Len() > 0)
			{
				UNameProperty* NameProp = Cast<UNameProperty>(Property);
				if(NameProp)
				{
					FName* NamePtr = (FName*)(((BYTE*)this) + NameProp->Offset);
					AutoComment += FString::Printf(TEXT("%s=%s "), *(NameProp->GetName()), *(NamePtr->ToString()));
				}

				UStrProperty* StrProp = Cast<UStrProperty>(Property);
				if(StrProp)
				{
					FString* StringPtr = (FString*)(((BYTE*)this) + StrProp->Offset);
					AutoComment += FString::Printf(TEXT("%s=%s "), *(StrProp->GetName()), *(*StringPtr));
				}

				UIntProperty* IntProp = Cast<UIntProperty>(Property);
				if(IntProp)
				{
					INT* IntPtr = (INT*)(((BYTE*)this) + IntProp->Offset);
					AutoComment += FString::Printf(TEXT("%s=%d "), *(IntProp->GetName()), *IntPtr);
				}

				UFloatProperty* FloatProp = Cast<UFloatProperty>(Property);
				if(FloatProp)
				{
					FLOAT* FloatPtr = (FLOAT*)(((BYTE*)this) + FloatProp->Offset);
					AutoComment += FString::Printf(TEXT("%s=%f "), *(FloatProp->GetName()), *FloatPtr);
				}

				UBoolProperty* BoolProp = Cast<UBoolProperty>(Property);
				if(BoolProp)
				{
					UBOOL bValue = *(BITFIELD*)((BYTE*)this + BoolProp->Offset) & BoolProp->BitMask;
					AutoComment += FString::Printf(TEXT("%s=%s "), *(BoolProp->GetName()), bValue ? TEXT("TRUE") : TEXT("FALSE"));
				}

				UByteProperty* ByteProp = Cast<UByteProperty>(Property);
				if (ByteProp)
				{
					BYTE* BytePtr = (BYTE*)(((BYTE*)this) + ByteProp->Offset);
					INT ByteVal = (INT)(*BytePtr);

					if (ByteProp->Enum && ByteVal < ByteProp->Enum->NumEnums())
					{
						FString EnumName;
						ByteProp->Enum->GetEnum(ByteVal).ToString( EnumName );
						AutoComment += FString::Printf(TEXT("%s=%s "), *(ByteProp->GetName()), *EnumName);
					}
					else
					{
						AutoComment += FString::Printf(TEXT("%s=%d "), *(ByteProp->GetName()), *BytePtr);
					}
				}
			}
		}
	}

	return AutoComment;
}

void USequenceObject::DrawTitleBar(FCanvas* Canvas, UBOOL bSelected, UBOOL bMouseOver, const FIntPoint& Pos, const FIntPoint& Size)
{
	// determine color
	FColor BkgColor = TitleBkgColor;
	if( this->IsA(USequence::StaticClass()) )
	{
		BkgColor = SequenceTitleBkgColor;
	}
	else if( this->IsA(USeqAct_Interp::StaticClass()) )
	{
		BkgColor = MatineeTitleBkgColor;
	}

	// Make a list of comments
	TArray<FString> Comments;
	TArray<FString> DebugComments;
	
	// determine title
	FString Title = GetDisplayTitle();

	// check for out of date objects
	if ((eventGetObjClassVersion() != ObjInstanceVersion) && !IsA(USequence::StaticClass()))
	{
		Title += TEXT(" (Outdated)");
		BkgColor = FColor(128,0,0);
	}
	else if (GetClass()->ClassFlags & CLASS_Deprecated)
	{
		Title += TEXT(" (Deprecated)");
		BkgColor = KismetDeprecatedBGColor;
	}


	// Make a string of all the 'extra' information that is desired by the action.
	FString AutoComment;
	if (!bSuppressAutoComment)
	{
		AutoComment = GetAutoComment();
	}

	if (ObjComment.Len() > 0)
	{
		Comments.AddItem(ObjComment);
	}
	if (AutoComment.Len() > 0)
	{
		Comments.AddItem(AutoComment);
	}

	// draw the title bar
	FColor BorderColor = GetBorderColor(bSelected, bMouseOver);
	FColor FontColor = FColor( 255, 255, 128 );
	UBOOL bIsDebuggingActiveBorder = GKismetRealtimeDebugging && (BorderColor == DebuggerActiveColor);
	INT BorderWidth = bIsDebuggingActiveBorder? DEBUG_ACTIVE_BORDER_WIDTH: 0;
	INT NewPosY = FLinkedObjDrawUtils::DrawTitleBar( Canvas, Pos, Size, FontColor, BorderColor, BkgColor, *Title, Comments, BorderWidth );
	
	// should draw debug info?
	UBOOL bIsPaused = GEditor->PlayWorld && GEditor->PlayWorld->GetWorldInfo()->bDebugPauseExecution;
	UBOOL bIsActivated_Paused = bIsPaused && bIsDebuggingActiveBorder;

	if ( GKismetRealtimeDebugging && PIESequenceObject && (bSelected || bIsActivated_Paused))
	{
		bDrawLast = TRUE;
		USequence* ParentSequencePIE = ParentSequence ? Cast<USequence>(ParentSequence->PIESequenceObject) : NULL;
		if ( ParentSequencePIE )
		{
			for( INT Idx = 0; Idx < ParentSequencePIE->DelayedActivatedOps.Num(); ++Idx )
			{
				FActivateOp& DelayedOp = ParentSequencePIE->DelayedActivatedOps(Idx);
				if( DelayedOp.Op == PIESequenceObject )
				{
					DebugComments.AddItem(FString::Printf(TEXT("Input %d delay (%.2f)"), DelayedOp.InputIdx, DelayedOp.RemainingDelay) );
				}
			}
		}
		PIESequenceObject->GetRealtimeComments(DebugComments);
		if(DebugComments.Num() > 0)
		{
			FIntPoint CommentSize = FLinkedObjDrawUtils::GetCommentBarSize(Canvas, *(DebugComments(0)));
			for(INT Idx = 1; Idx < DebugComments.Num(); Idx++)
			{
				FIntPoint TempSize = FLinkedObjDrawUtils::GetCommentBarSize(Canvas, *(DebugComments(Idx)));
				CommentSize.Y += TempSize.Y;
				CommentSize.X = (TempSize.X > CommentSize.X) ? TempSize.X : CommentSize.X;
			}
			if ( FLinkedObjDrawUtils::AABBLiesWithinViewport( Canvas, Pos.X, NewPosY, Size.X, Size.Y ) )
			{
				DrawTile( Canvas, Pos.X,	NewPosY-CommentSize.Y,		CommentSize.X,		CommentSize.Y,		0.0f,0.0f,0.0f,0.0f, BorderColor );
				DrawTile( Canvas, Pos.X+1,	NewPosY+1-CommentSize.Y,	CommentSize.X-2,	CommentSize.Y-2,	0.0f,0.0f,0.0f,0.0f, FColor(200,200,200) );
			}
			FLinkedObjDrawUtils::DrawComments(Canvas, FIntPoint(Pos.X, NewPosY), Size, DebugComments, GEngine->TinyFont );

		}
	
	}
	else if ( GKismetRealtimeDebugging && PIESequenceObject && !(bSelected || bIsActivated_Paused) )
	{
		// Give selected kismet actions draw priority when debugging, do not trample existing instances of flag
		bDrawLast = FALSE;
	}
}

/** Calculate the bounding box of this sequence object. For use by Kismet. */
FIntRect USequenceObject::GetSeqObjBoundingBox()
{
	return FIntRect(ObjPosX, ObjPosY, ObjPosX + DrawWidth, ObjPosY + DrawHeight);
}

/** Snap to ObjPosX/Y to an even multiple of Gridsize. */
void USequenceObject::SnapPosition(INT Gridsize, INT MaxSequenceSize)
{
	ObjPosX = appRound(ObjPosX/Gridsize) * Gridsize;
	ObjPosY = appRound(ObjPosY/Gridsize) * Gridsize;

	FIntPoint BoundsSize = GetSeqObjBoundingBox().Size();

	ObjPosX = ::Clamp<INT>(ObjPosX, -MaxSequenceSize, MaxSequenceSize - BoundsSize.X);
	ObjPosY = ::Clamp<INT>(ObjPosY, -MaxSequenceSize, MaxSequenceSize - BoundsSize.Y);
}

//-----------------------------------------------------------------------------
//	USequenceOp
//-----------------------------------------------------------------------------

FColor USequenceOp::GetConnectionColor( INT ConnType, INT ConnIndex, INT MouseOverConnType, INT MouseOverConnIndex )
{
	if( ConnType == LOC_INPUT )
	{
#if _WINDOWS
		if( GKismetRealtimeDebugging && GEditor->PlayWorld )
		{
			return FColor(0,0,0);
		}
		else
#endif
		if( MouseOverConnType == LOC_INPUT && MouseOverConnIndex == ConnIndex ) 
		{
			return MouseOverLogicColor;
		}
		else if( InputLinks(ConnIndex).bDisabled )
		{
			return FColor(255,0,0);
		}
		else if( InputLinks(ConnIndex).bDisabledPIE )
		{
			return FColor(255,128,0);
		}
	}
	else if( ConnType == LOC_OUTPUT )
	{
#if _WINDOWS
		if( GKismetRealtimeDebugging && GEditor->PlayWorld )
		{
			return FColor(0,0,0);
		}
		else
#endif
		if( MouseOverConnType == LOC_OUTPUT && MouseOverConnIndex == ConnIndex ) 
		{
			return MouseOverLogicColor;
		}
		else if( OutputLinks(ConnIndex).bDisabled )
		{
			return FColor(255,0,0);
		}
		else if( OutputLinks(ConnIndex).bDisabledPIE )
		{
			return FColor(255,128,0);
		}
	}
	else if( ConnType == LOC_VARIABLE )
	{
		FColor VarColor = GetVarConnectorColor(ConnIndex);
		if( MouseOverConnType == LOC_VARIABLE && MouseOverConnIndex == ConnIndex )
		{
			return VarColor;
		}
		else
		{
			return FColor( FLinearColor(VarColor) * MouseOverColorScale );
		}
	}
	else if( ConnType == LOC_EVENT )
	{
		FColor EventColor = FColor(255,0,0);
		if( MouseOverConnType == LOC_EVENT && MouseOverConnIndex == ConnIndex ) 
		{
			return EventColor;
		}
		else
		{ 
			return FColor( FLinearColor(EventColor) * MouseOverColorScale );
		}
	}

	return FColor(0,0,0);
}


/** 
 * Sets a pending connector position recalculation on this sequence object.
 */
void USequenceOp::SetPendingConnectorRecalc()
{
	bPendingVarConnectorRecalc = TRUE;
	bPendingInputConnectorRecalc = TRUE;
	bPendingOutputConnectorRecalc = TRUE;
}

void USequenceOp::MakeLinkedObjDrawInfo(FLinkedObjDrawInfo& ObjInfo, INT MouseOverConnType, INT MouseOverConnIndex)
{
	// add all input Links
	for(INT i=0; i<InputLinks.Num(); i++)
	{
		const FSeqOpInputLink& InputLink = InputLinks(i);
		// only add if visible
		if (!InputLinks(i).bHidden)
		{
			const FColor ConnColor = GetConnectionColor( LOC_INPUT, i, MouseOverConnType, MouseOverConnIndex );
			ObjInfo.Inputs.AddItem( FLinkedObjConnInfo(*InputLink.LinkDesc, ConnColor, InputLink.bMoving, InputLink.OverrideDelta==0, InputLink.OverrideDelta) );
		}
	}
	// add all output Links
	for(INT i=0; i<OutputLinks.Num(); i++)
	{
		const FSeqOpOutputLink& OutputLink = OutputLinks(i);
		// only add if visible
		if ( !OutputLink.bHidden )
		{
			const FColor ConnColor = GetConnectionColor( LOC_OUTPUT, i, MouseOverConnType, MouseOverConnIndex );
			// Setup connection info
			// Note: if the connector has a delta offset of zero then it is considered a new connection and its postion is recalculated.
			// No connector should have an offset of zero since the offset is relative to the sequence object position, and 0 would be a the edge of the object
			ObjInfo.Outputs.AddItem( FLinkedObjConnInfo(*OutputLink.LinkDesc, ConnColor, OutputLink.bMoving, OutputLink.OverrideDelta==0, OutputLink.OverrideDelta ) );
		}
	}
	// add all variable Links
	for(INT i=0; i<VariableLinks.Num(); i++)
	{
		const FSeqVarLink& VarLink = VariableLinks(i);
		// only add if visible
		if (!VarLink.bHidden)
		{
			const FColor ConnColor = GetConnectionColor( LOC_VARIABLE, i, MouseOverConnType, MouseOverConnIndex );
			// Setup connection info
			// Note: if the connector has a delta offset of zero then it is considered a new connection and its postion is recalculated.
			// No connector should have an offset of zero since the offset is relative to the sequence object position, and 0 would be a the edge of the object
			ObjInfo.Variables.AddItem( FLinkedObjConnInfo(*VarLink.LinkDesc, ConnColor, VarLink.bMoving, VarLink.OverrideDelta==0, VarLink.OverrideDelta, VarLink.bWriteable) );
		}
	}
	// add all event Links
	for(INT i=0; i<EventLinks.Num(); i++)
	{
		const FSeqEventLink& EventLink = EventLinks(i);
		// only add if visible
		if (!EventLink.bHidden)
		{
			const FColor ConnColor = GetConnectionColor( LOC_EVENT, i, MouseOverConnType, MouseOverConnIndex );
			// Setup connection info
			// Note: if the connector has a delta offset of zero then it is considered a new connection and its postion is recalculated.
			// No connector should have an offset of zero since the offset is relative to the sequence object position, and 0 would be a the edge of the object
			ObjInfo.Events.AddItem( FLinkedObjConnInfo(*EventLink.LinkDesc, ConnColor, EventLink.bMoving, EventLink.OverrideDelta==0, EventLink.OverrideDelta ) );
		}
	}
	// set the object reference to this object, for later use
	ObjInfo.ObjObject = this;

	
	ObjInfo.bPendingVarConnectorRecalc = bPendingVarConnectorRecalc;
	ObjInfo.bPendingInputConnectorRecalc = bPendingInputConnectorRecalc;
	ObjInfo.bPendingOutputConnectorRecalc = bPendingOutputConnectorRecalc;
}

/** Utility for converting the 'visible' index of a connector to its actual connector index. */
INT USequenceOp::VisibleIndexToActualIndex(INT ConnType, INT VisibleIndex)
{
	INT CurrentVisIndex = -1;
	if(ConnType == LOC_INPUT)
	{
		for(INT i=0; i<InputLinks.Num(); i++)
		{
			if(!InputLinks(i).bHidden)
			{
				CurrentVisIndex++;
			}

			if(CurrentVisIndex == VisibleIndex)
			{
				return i;
			}
		}	
	}
	else if(ConnType == LOC_OUTPUT)
	{
		for(INT i=0; i<OutputLinks.Num(); i++)
		{
			if(!OutputLinks(i).bHidden)
			{
				CurrentVisIndex++;
			}

			if(CurrentVisIndex == VisibleIndex)
			{
				return i;
			}
		}
	}
	else if(ConnType == LOC_VARIABLE)
	{
		for(INT i=0; i<VariableLinks.Num(); i++)
		{
			if(!VariableLinks(i).bHidden)
			{
				CurrentVisIndex++;
			}

			if(CurrentVisIndex == VisibleIndex)
			{
				return i;
			}
		}
	}
	else if(ConnType == LOC_EVENT)
	{
		for(INT i=0; i<EventLinks.Num(); i++)
		{
			if(!EventLinks(i).bHidden)
			{
				CurrentVisIndex++;
			}

			if(CurrentVisIndex == VisibleIndex)
			{
				return i;
			}
		}
	}

	// Shouldn't get here!
	return 0;
}

void USequenceOp::GetRealtimeComments(TArray<FString> &OutComments)
{
	OutComments.AddItem(FString::Printf(TEXT("Activation count : %d"), ActivateCount));
	OutComments.AddItem(FString::Printf(TEXT("Last activation time : %.2f"), PIEActivationTime));

	Super::GetRealtimeComments(OutComments);
}

FIntPoint USequenceOp::GetLogicConnectorsSize(FCanvas* Canvas, INT* InputY, INT* OutputY)
{
	FLinkedObjDrawInfo ObjInfo;
	MakeLinkedObjDrawInfo(ObjInfo);

	return FLinkedObjDrawUtils::GetLogicConnectorsSize(ObjInfo, InputY, OutputY);
}

FIntPoint USequenceOp::GetVariableConnectorsSize(FCanvas* Canvas )
{
	FLinkedObjDrawInfo ObjInfo;
	MakeLinkedObjDrawInfo(ObjInfo);

	return FLinkedObjDrawUtils::GetVariableConnectorsSize(Canvas, ObjInfo);
}

void USequenceOp::DrawLogicConnectors(FCanvas* Canvas, const FIntPoint& Pos, const FIntPoint& Size, INT MouseOverConnType, INT MouseOverConnIndex)
{
	DrawWidth = Size.X;

	FLinkedObjDrawInfo ObjInfo;
	MakeLinkedObjDrawInfo(ObjInfo, MouseOverConnType, MouseOverConnIndex);

	const UBOOL bShouldGhostNonMoving = bHaveMovingVarConnector || bHaveMovingInputConnector || bHaveMovingOutputConnector;
	FLinkedObjDrawUtils::DrawLogicConnectorsWithMoving( Canvas, ObjInfo, Pos, Size, NULL, bHaveMovingInputConnector, bHaveMovingOutputConnector, bShouldGhostNonMoving );

	// Save off the state of pending connector recalculations
	bPendingInputConnectorRecalc = ObjInfo.bPendingInputConnectorRecalc;
	bPendingOutputConnectorRecalc = ObjInfo.bPendingOutputConnectorRecalc;

	INT LinkIdx = 0;
	for(INT i=0; i<InputLinks.Num(); i++)
	{
		if (!InputLinks(i).bHidden)
		{
			InputLinks(i).DrawY = ObjInfo.InputY(LinkIdx);

			// Store back this information on the actual link.
			InputLinks(i).OverrideDelta = ObjInfo.Inputs(LinkIdx).OverrideDelta;
			InputLinks(i).bClampedMax = ObjInfo.Inputs(LinkIdx).bClampedMax;
			InputLinks(i).bClampedMin = ObjInfo.Inputs(LinkIdx).bClampedMin;

			LinkIdx++;
		}
	}

	LinkIdx = 0;
	for(INT i=0; i<OutputLinks.Num(); i++)
	{
		if (!OutputLinks(i).bHidden)
		{
			OutputLinks(i).DrawY = ObjInfo.OutputY(LinkIdx);

			// Store back this information on the actual link.
			OutputLinks(i).OverrideDelta = ObjInfo.Outputs(LinkIdx).OverrideDelta;
			OutputLinks(i).bClampedMax = ObjInfo.Outputs(LinkIdx).bClampedMax;
			OutputLinks(i).bClampedMin = ObjInfo.Outputs(LinkIdx).bClampedMin;

			LinkIdx++;
		}
	}
}

FColor USequenceOp::GetVarConnectorColor(INT ConnIndex)
{
	if( ConnIndex < 0 || ConnIndex >= VariableLinks.Num() )
	{
		return FColor(0,0,0);
	}

	FSeqVarLink* VarLink = &VariableLinks(ConnIndex);

	if(VarLink->ExpectedType == NULL)
	{
		return FColor(0,0,0);
	}

	USequenceVariable* DefVar = (USequenceVariable*)VarLink->ExpectedType->GetDefaultObject();
	return DefVar->ObjColor;
}


void USequenceOp::DrawVariableConnectors(FCanvas* Canvas, const FIntPoint& Pos, const FIntPoint& Size, INT MouseOverConnType, INT MouseOverConnIndex, INT VarWidth)
{
	DrawHeight = (Pos.Y + Size.Y) - ObjPosY;

	FLinkedObjDrawInfo ObjInfo;
	MakeLinkedObjDrawInfo(ObjInfo, MouseOverConnType, MouseOverConnIndex);

	const UBOOL bShouldGhostNonMoving = bHaveMovingVarConnector || bHaveMovingInputConnector || bHaveMovingOutputConnector;
	FLinkedObjDrawUtils::DrawVariableConnectorsWithMoving( Canvas, ObjInfo, Pos, Size, VarWidth, bHaveMovingVarConnector, bShouldGhostNonMoving );

	// Save off the state of pending connector recalculations
	bPendingVarConnectorRecalc = ObjInfo.bPendingVarConnectorRecalc;

	INT idx, LinkIdx = 0;
	for (idx = 0; idx < VariableLinks.Num(); idx++)
	{
		if (!VariableLinks(idx).bHidden)
		{
			VariableLinks(idx).DrawX = ObjInfo.VariableX(LinkIdx);
	
			// Store back this information on the actual link.
			VariableLinks(idx).OverrideDelta = ObjInfo.Variables(LinkIdx).OverrideDelta;
			VariableLinks(idx).bClampedMax = ObjInfo.Variables(LinkIdx).bClampedMax;
			VariableLinks(idx).bClampedMin = ObjInfo.Variables(LinkIdx).bClampedMin;

			LinkIdx++;
		}


	}

	LinkIdx = 0;
	for (idx = 0; idx < EventLinks.Num(); idx++)
	{
		if (!EventLinks(idx).bHidden)
		{
			EventLinks(idx).DrawX = ObjInfo.EventX(LinkIdx);	

			// Store back this information on the actual link.
			EventLinks(idx).OverrideDelta = ObjInfo.Events(LinkIdx).OverrideDelta;
			EventLinks(idx).bClampedMax = ObjInfo.Events(LinkIdx).bClampedMax;
			EventLinks(idx).bClampedMin = ObjInfo.Events(LinkIdx).bClampedMin;

			LinkIdx++;
		}
	}
}

/** Utility for determining if the mouse is currently over a connector on one end of this link. */
static UBOOL MouseOverLink(const USequenceOp* Me, const FSeqOpOutputLink& Output, INT OutputIndex, INT LinkIndex, const USequenceObject* MouseOverSeqObj, INT MouseOverConnType, INT MouseOverConnIndex)
{
	if( MouseOverConnType == LOC_OUTPUT && 
		Me == MouseOverSeqObj && 
		OutputIndex == MouseOverConnIndex)
	{
		return TRUE;
	}

	if( MouseOverConnType == LOC_INPUT && 
		Output.Links(LinkIndex).LinkedOp == MouseOverSeqObj && 
		Output.Links(LinkIndex).InputLinkIdx == MouseOverConnIndex )
	{
		return TRUE;
	}

	return FALSE;
}

void USequenceOp::DrawLogicLinks(FCanvas* Canvas, TArray<USequenceObject*> &SelectedSeqObjs, USequenceObject* MouseOverSeqObj, INT MouseOverConnType, INT MouseOverConnIndex)
{
	const UBOOL bSelected = SelectedSeqObjs.ContainsItem(this);
	UFont* FontToUse = FLinkedObjDrawUtils::GetFont();
	for (INT i=0; i<InputLinks.Num(); i++)
	{
		const FSeqOpInputLink &Link = InputLinks(i);
		if (Link.ActivateDelay > 0.f)
		{
			FString DelayStr = FString::Printf(TEXT("Delay %2.2f"),Link.ActivateDelay);
			FIntPoint Start = this->GetConnectionLocation(LOC_INPUT, i);
			INT SizeX, SizeY;
			UCanvas::ClippedStrLen(FontToUse,1.f,1.f,SizeX,SizeY,*DelayStr);
			Start.X -= SizeX;
			Start.Y -= SizeY;
			DrawString( Canvas, Start.X, Start.Y, *DelayStr, FontToUse, FLinearColor::White );
		}
	}
	// for each valid output Link,
	for(INT i=0; i<OutputLinks.Num(); i++)
	{
		const FSeqOpOutputLink &Link = OutputLinks(i);
		// grab the start point for this line
		const FIntPoint Start = this->GetConnectionLocation(LOC_OUTPUT, i);
		// iterate through all Linked inputs,
		for (INT LinkIdx = 0; LinkIdx < Link.Links.Num(); LinkIdx++)
		{
			const FSeqOpOutputInputLink &InLink = Link.Links(LinkIdx);
			if (InLink.LinkedOp != NULL &&
				InLink.InputLinkIdx >= 0 &&
				InLink.InputLinkIdx < InLink.LinkedOp->InputLinks.Num())
			{
				// grab the end point
				const FIntPoint End = InLink.LinkedOp->GetConnectionLocation(LOC_INPUT, InLink.InputLinkIdx);
				FColor LineColor = FColor(0,0,0);
				if( bSelected || SelectedSeqObjs.ContainsItem(InLink.LinkedOp) )
				{
					LineColor = FColor(255,255,0);
				}
				else if( MouseOverLink(this, Link, i, LinkIdx, MouseOverSeqObj, MouseOverConnType, MouseOverConnIndex) )
				{
					LineColor = FColor(255,200,0);
				}
				else if( Link.bDisabled || InLink.LinkedOp->InputLinks(InLink.InputLinkIdx).bDisabled )
				{
					LineColor = FColor(255,0,0);
				} 
				else if( Link.bDisabledPIE || InLink.LinkedOp->InputLinks(InLink.InputLinkIdx).bDisabledPIE )
				{
					LineColor = FColor(255,128,0);
				} 
				else if( Link.bHasImpulse )
				{
					LineColor = FColor(0,255,0);
				}

				if( GKismetRealtimeDebugging && GEditor->PlayWorld )
				{
					USequenceOp* SequenceOp = Cast<USequenceOp>(PIESequenceObject);
					if( SequenceOp )
					{
						// There are two separate border color behaviors, one when running and one when paused/stepping
						if (GEditor->PlayWorld->GetWorldInfo()->bDebugPauseExecution)
						{
							if (SequenceOp->OutputLinks(i).bIsActivated)
							{
								LineColor = DebuggerActiveColor;

								// The game is paused but it is the next game tick, reset the debugger state of this link
								if ((GEditor->PlayWorld->GetTimeSeconds() - SequenceOp->OutputLinks(i).PIEActivationTime) >= GWorld->GetDeltaSeconds())
								{
									SequenceOp->OutputLinks(i).bIsActivated = FALSE;
								}
							}
							else
							{
								SequenceOp->OutputLinks(i).bIsActivated = FALSE;
							}
						}
						else
						{
							// When the game is running, we want to fade the link's color from the "activated" color to it's normal color
							if (SequenceOp->OutputLinks(i).bIsActivated)
							{
								FLOAT TimeDiff = GEditor->PlayWorld->GetTimeSeconds() - SequenceOp->OutputLinks(i).PIEActivationTime;
								if (TimeDiff < (DebugColorFadeEndTime * 0.5f))
								{
									FLOAT LerpVal = TimeDiff / (DebugColorFadeEndTime * 0.5f);
									BYTE LerpR = (BYTE)Lerp((FLOAT)DebuggerActiveColor.R, (FLOAT)LineColor.R, LerpVal);
									BYTE LerpG = (BYTE)Lerp((FLOAT)DebuggerActiveColor.G, (FLOAT)LineColor.G, LerpVal);
									BYTE LerpB = (BYTE)Lerp((FLOAT)DebuggerActiveColor.B, (FLOAT)LineColor.B, LerpVal);
									LineColor = FColor(LerpR,LerpG,LerpB);
								}
								else
								{
									// Color has faded to normal, reset the debugger state
									SequenceOp->OutputLinks(i).bIsActivated = FALSE;
								}
							}
							else
							{
								SequenceOp->OutputLinks(i).bIsActivated = FALSE;
							}
						}
					}
				}

				if (Canvas->IsHitTesting())
				{
					Canvas->SetHitProxy(new HLinkedObjLineProxy(this,i,InLink.LinkedOp,InLink.InputLinkIdx));
				}

				// Curves
				{
					const FLOAT Tension = Abs<INT>(Start.X - End.X);
					//const FLOAT Tension = 100.f;
					FLinkedObjDrawUtils::DrawSpline(Canvas, Start, Tension * FVector2D(1,0), End, Tension * FVector2D(1,0), LineColor, true);
				}

				if (Canvas->IsHitTesting())
				{
					Canvas->SetHitProxy(NULL);
				}
			}
		}
		// draw the activate delay if set
		if (Link.ActivateDelay > 0.f)
		{
			DrawString( Canvas, Start.X, Start.Y, *FString::Printf(TEXT("Delay %2.2f"),Link.ActivateDelay), FontToUse, FLinearColor::White );
		}
	}
}

/**
 * Draws lines to all Linked variables and events.
 */
void USequenceOp::DrawVariableLinks(FCanvas* Canvas, TArray<USequenceObject*> &SelectedSeqObjs, USequenceObject* MouseOverSeqObj, INT MouseOverConnType, INT MouseOverConnIndex)
{
	const UBOOL bSelected = SelectedSeqObjs.ContainsItem(this);
	for (INT VarIdx = 0; VarIdx < VariableLinks.Num(); VarIdx++)
	{
		FSeqVarLink &VarLink = VariableLinks(VarIdx);
		const FIntPoint Start = this->GetConnectionLocation(LOC_VARIABLE, VarIdx);
		// draw Links for each variable connected to this connection
		for (INT Idx = 0; Idx < VarLink.LinkedVariables.Num(); Idx++)
		{
			USequenceVariable *Var = VarLink.LinkedVariables(Idx);
			// remove any null entries
			if (Var == NULL)
			{
				VarLink.LinkedVariables.Remove(Idx--,1);
				continue;
			}
			const FIntPoint End = Var->GetVarConnectionLocation();
			FColor LinkColor;
			if (bSelected || SelectedSeqObjs.ContainsItem(Var))
			{
				LinkColor = FColor(255,255,0);
			}
			else
			{
				LinkColor = (Var->ObjColor.ReinterpretAsLinear() * MouseOverColorScale).Quantize();
			}

			// Curves
			{
				const FLOAT Tension = Abs<INT>(Start.Y - End.Y);

				if(!VarLink.bWriteable)
				{
					FLinkedObjDrawUtils::DrawSpline(Canvas, End, FVector2D(0,0), Start, Tension*FVector2D(0,-1), LinkColor, true);
				}
				else
				{
					FLinkedObjDrawUtils::DrawSpline(Canvas, Start, Tension*FVector2D(0,1), End, FVector2D(0,0), LinkColor, true);
				}
			}
		}
	}
	for (INT EvtIdx = 0; EvtIdx < EventLinks.Num(); EvtIdx++)
	{
		FSeqEventLink &EvtLink = EventLinks(EvtIdx);
		const FIntPoint Start = this->GetConnectionLocation(LOC_EVENT, EvtIdx);
		// draw Links for each variable connected to this connection
		for (INT Idx = 0; Idx < EvtLink.LinkedEvents.Num(); Idx++)
		{
			USequenceEvent *Evt = EvtLink.LinkedEvents(Idx);
			// remove any null entries
			if (Evt == NULL)
			{
				EvtLink.LinkedEvents.Remove(Idx--,1);
				continue;
			}
			FColor LinkColor;
			if (bSelected || SelectedSeqObjs.ContainsItem(Evt))
			{
				LinkColor = FColor(255,255,0);
			}
			else
			{
				LinkColor = Evt->ObjColor;
			}
			const FIntPoint End = Evt->GetCenterPoint(Canvas);

			// Curves
			{
				const FLOAT Tension = Abs<INT>(Start.Y - End.Y);
				FLinkedObjDrawUtils::DrawSpline(Canvas, Start, Tension*FVector2D(0,1), End, Tension*FVector2D(0,1), LinkColor, true);
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

/**
 * Adjusts the postions of a connector based on the Delta position passed in.
 * Currently only variable, event, and output connectors can be moved. 
 * 
 * @param ConnType	The connector type to be moved
 * @param ConnIndex	The index in the connector array where the connector is located
 * @param DeltaX	The amount to move the connector in X
 * @param DeltaY	The amount to move the connector in Y	
 */
void USequenceOp::MoveConnectionLocation(INT ConnType, INT ConnIndex, INT DeltaX, INT DeltaY)
{
#if _WINDOWS
	// Do not move in Kismet Debugging mode
	if( GKismetRealtimeDebugging && GEditor->PlayWorld )
	{
		return;
	}
#endif

	UBOOL bMovedConnector = FALSE;
	if (ConnType == LOC_VARIABLE)
	{	
		// A variable connector was moved check to make sure it is not clamped.
		if( ConnIndex >= 0 && ConnIndex < VariableLinks.Num() )
		{	
			if( DeltaX > 0 && !VariableLinks(ConnIndex).bClampedMax  )
			{
				VariableLinks(ConnIndex).OverrideDelta += DeltaX;
				bMovedConnector = TRUE;
			}
			else if( DeltaX < 0 && !VariableLinks(ConnIndex).bClampedMin )
			{
				VariableLinks(ConnIndex).OverrideDelta += DeltaX;
				bMovedConnector = TRUE;
			}
		}
	}
	else if (ConnType == LOC_EVENT)
	{
		// An event connector was moved check to make sure it is not clamped.
		if( ConnIndex >= 0 && ConnIndex < EventLinks.Num() )
		{
			if( DeltaX > 0 && !EventLinks(ConnIndex).bClampedMax  )
			{
				EventLinks(ConnIndex).OverrideDelta += DeltaX;
				bMovedConnector = TRUE;
			}
			else if( DeltaX < 0 && !EventLinks(ConnIndex).bClampedMin )
			{
				EventLinks(ConnIndex).OverrideDelta += DeltaX;
				bMovedConnector = TRUE;
			}
		}
	}
	else if ( ConnType == LOC_OUTPUT )
	{
		// An output connector was moved check to make sure it is not clamped.
		if( ConnIndex >= 0 && ConnIndex < OutputLinks.Num() )
		{
			if( DeltaY > 0 && !OutputLinks(ConnIndex).bClampedMax  )
			{
				OutputLinks(ConnIndex).OverrideDelta += DeltaY;
				bMovedConnector = TRUE;
			}
			else if( DeltaY < 0 && !OutputLinks(ConnIndex).bClampedMin )
			{
				OutputLinks(ConnIndex).OverrideDelta += DeltaY;
				bMovedConnector = TRUE;
			}
		}
	}
	else if ( ConnType == LOC_INPUT )
	{
		// An output connector was moved check to make sure it is not clamped.
		if( ConnIndex >= 0 && ConnIndex < InputLinks.Num() )
		{
			if( DeltaY > 0 && !InputLinks(ConnIndex).bClampedMax  )
			{
				InputLinks(ConnIndex).OverrideDelta += DeltaY;
				bMovedConnector = TRUE;
			}
			else if( DeltaY < 0 && !InputLinks(ConnIndex).bClampedMin )
			{
				InputLinks(ConnIndex).OverrideDelta += DeltaY;
				bMovedConnector = TRUE;
			}
		}
	}

	if( bMovedConnector )
	{
		// Package should be dirty if a connector moved so we can save off new positions of connectors
		MarkPackageDirty();
	}
}

/**
 * Sets the member variable on the connector struct to bMoving so we can perform different calculations in the draw code
 * 
 * @param ConnType	The connector type to be moved
 * @param ConnIndex	The index in the connector array where the connector is located
 * @param bMoving	True if the connector is moving
 */
void USequenceOp::SetConnectorMoving( INT ConnType, INT ConnIndex, UBOOL bMoving )
{
	if ( ConnType == LOC_VARIABLE )
	{
		if( ConnIndex >= 0 && ConnIndex < VariableLinks.Num() )
		{	
			VariableLinks(ConnIndex).bMoving = bMoving;
			bHaveMovingVarConnector = bMoving;
		}
	}
	else if ( ConnType == LOC_EVENT )
	{
		if( ConnIndex >= 0 && ConnIndex < EventLinks.Num() )
		{
			EventLinks(ConnIndex).bMoving = bMoving;
			bHaveMovingVarConnector = bMoving;
		}
	}
	else if ( ConnType == LOC_OUTPUT )
	{
		if( ConnIndex >= 0 && ConnIndex < OutputLinks.Num() )
		{
			OutputLinks(ConnIndex).bMoving = bMoving;
			bHaveMovingOutputConnector = bMoving;
		}
	}
	else if( ConnType == LOC_INPUT )
	{
		if( ConnIndex >= 0 && ConnIndex < InputLinks.Num() )
		{
			InputLinks(ConnIndex).bMoving = bMoving;
			bHaveMovingInputConnector = bMoving;
		}
	}	
}

void USequenceOp::DrawSeqObj(FCanvas* Canvas, UBOOL bSelected, UBOOL bMouseOver, INT MouseOverConnType, INT MouseOverConnIndex, FLOAT MouseOverTime)
{
	const UBOOL bHitTesting = Canvas->IsHitTesting();

	const FIntPoint TitleSize = GetTitleBarSize(Canvas);
	const FIntPoint LogicSize = GetLogicConnectorsSize(Canvas);
	const FIntPoint VarSize = GetVariableConnectorsSize(Canvas);

	const INT Width = Max3(TitleSize.X, LogicSize.X, VarSize.X);
	const INT Height = TitleSize.Y + LogicSize.Y + VarSize.Y + 3;

	FColor BorderColor = GetBorderColor(bSelected, bMouseOver);
	UBOOL bIsDebuggingActiveBorder = GKismetRealtimeDebugging && (BorderColor == DebuggerActiveColor);
	INT BorderWidth = bIsDebuggingActiveBorder? DEBUG_ACTIVE_BORDER_WIDTH: 0;
	INT DBorderWidth = BorderWidth * 2;

	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HLinkedObjProxy(this) );

	DrawTitleBar(Canvas, bSelected, bMouseOver, FIntPoint(ObjPosX, ObjPosY), FIntPoint(Width, TitleSize.Y));

	DrawTile(Canvas,ObjPosX - BorderWidth,	ObjPosY + TitleSize.Y + 1 - BorderWidth,	Width + DBorderWidth,	LogicSize.Y + VarSize.Y + DBorderWidth,	0.0f,0.0f,0.0f,0.0f, BorderColor );
	DrawTile(Canvas,ObjPosX + 1,			ObjPosY + TitleSize.Y + 2,					Width - 2,				LogicSize.Y + VarSize.Y - 2,			0.0f,0.0f,0.0f,0.0f, FColor(140,140,140) );

	DrawExtraInfo(Canvas,FVector(ObjPosX + (Width/2),ObjPosY + (LogicSize.Y + VarSize.Y + TitleSize.Y)/2,0));

	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );

	if(bMouseOver)
	{
		DrawLogicConnectors(	Canvas, FIntPoint(ObjPosX, ObjPosY + TitleSize.Y + 1),					FIntPoint(Width, LogicSize.Y), MouseOverConnType, MouseOverConnIndex);
		DrawVariableConnectors(	Canvas, FIntPoint(ObjPosX, ObjPosY + TitleSize.Y + 1 + LogicSize.Y),	FIntPoint(Width, VarSize.Y), MouseOverConnType, MouseOverConnIndex, VarSize.X);
	}
	else
	{
		DrawLogicConnectors(	Canvas, FIntPoint(ObjPosX, ObjPosY + TitleSize.Y + 1),					FIntPoint(Width, LogicSize.Y), -1, INDEX_NONE);
		DrawVariableConnectors(	Canvas, FIntPoint(ObjPosX, ObjPosY + TitleSize.Y + 1 + LogicSize.Y),	FIntPoint(Width, VarSize.Y), -1, INDEX_NONE, VarSize.X);
	}

	if( bIsBreakpointSet )
	{
		FLOAT BreakpointImagePosX = ObjPosX - 20.f;
		FLOAT BreakpointImagePosY = ObjPosY;
		// Draw circle
		const FIntPoint CircleCenter(BreakpointImagePosX + 8.f, BreakpointImagePosY + 8.f);

		FLinkedObjDrawUtils::DrawNGon( Canvas, CircleCenter, FColor(255,0,0), LO_CIRCLE_SIDES, 8 );
	}

	UBOOL bIsPaused = GEditor->PlayWorld && GEditor->PlayWorld->GetWorldInfo()->bDebugPauseExecution;
	USequenceOp* PIESequenceOp = Cast<USequenceOp>(PIESequenceObject);
	if (PIESequenceOp && PIESequenceOp->bIsCurrentDebuggerOp && bIsPaused && bIsDebuggingActiveBorder)
	{
		// Draw arrow
		DrawTriangle2D(Canvas,
			FIntPoint(ObjPosX - 11, ObjPosY + 3), FVector2D(0.f, 0.f),
			FIntPoint(ObjPosX - 5, ObjPosY + 8), FVector2D(0.f, 0.f),
			FIntPoint(ObjPosX - 11, ObjPosY + 13), FVector2D(0.f, 0.f),
			FColor(255,192,0)
			);
		DrawTile(Canvas, ObjPosX - 19.0f, ObjPosY + 6.0f, 8.0f, 4.0f, 0.f, 0.f, 0.f, 0.f, FColor(255,192,0));
	}
}

//-----------------------------------------------------------------------------
//	USequenceEvent
//-----------------------------------------------------------------------------

FIntPoint USequenceEvent::GetCenterPoint(FCanvas* Canvas)
{
	return FIntPoint(ObjPosX + MaxWidth / 2, ObjPosY);
}

FIntRect USequenceEvent::GetSeqObjBoundingBox()
{
	return FIntRect(ObjPosX, ObjPosY, ObjPosX + MaxWidth, ObjPosY + DrawHeight);
}	

void USequenceEvent::DrawSeqObj(FCanvas* Canvas, UBOOL bSelected, UBOOL bMouseOver, INT MouseOverConnType, INT MouseOverConnIndex, FLOAT MouseOverTime)
{
	const UBOOL bHitTesting = Canvas->IsHitTesting();
	const FColor BorderColor = GetBorderColor(bSelected, bMouseOver);

	const FIntPoint TitleSize = GetTitleBarSize(Canvas);
	const FIntPoint LogicSize = GetLogicConnectorsSize(Canvas);
	const FIntPoint VarSize = GetVariableConnectorsSize(Canvas);

	UBOOL bIsDebuggingActiveBorder = GKismetRealtimeDebugging && (BorderColor == DebuggerActiveColor);
	INT BorderWidth = bIsDebuggingActiveBorder? DEBUG_ACTIVE_BORDER_WIDTH: 0;
	INT DBorderWidth = BorderWidth * 2;

	MaxWidth = Max3(TitleSize.X, LogicSize.X, VarSize.X);

	// Draw diamond
	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HLinkedObjProxy(this) );

	DrawTitleBar(Canvas, bSelected, bMouseOver, FIntPoint(ObjPosX, ObjPosY), FIntPoint(MaxWidth, TitleSize.Y));

#define TRIANGLE_SIZE 32

	const INT CenterX = ObjPosX + MaxWidth / 2;
	const INT TriangleTop = ObjPosY + TitleSize.Y + 1;
	const INT LogicLeft = CenterX - LogicSize.X / 2;
	const INT LogicRight = CenterX + LogicSize.X / 2;
	// draw top triangle
	DrawTriangle2D(Canvas,
		FIntPoint(CenterX, TriangleTop - BorderWidth), FVector2D(0.f, 0.f),
		FIntPoint(LogicLeft - 1 - BorderWidth, TriangleTop + TRIANGLE_SIZE), FVector2D(0.f, 0.f),
        FIntPoint(LogicRight + 1 + BorderWidth, TriangleTop + TRIANGLE_SIZE), FVector2D(0.f, 0.f),
		BorderColor
		);
	DrawTriangle2D(Canvas,
		FIntPoint(CenterX, TriangleTop + 1), FVector2D(0.f, 0.f),
		FIntPoint(LogicLeft, TriangleTop + TRIANGLE_SIZE), FVector2D(0.f, 0.f),
		FIntPoint(LogicRight, TriangleTop + TRIANGLE_SIZE), FVector2D(0.f, 0.f),
		FColor(140,140,140)
		);
	// draw bottom triangle
	DrawTriangle2D(Canvas,
		FIntPoint(LogicLeft - 1 - BorderWidth, TriangleTop + TRIANGLE_SIZE + LogicSize.Y), FVector2D(0.f, 0.f),
		FIntPoint(LogicRight + 1 + BorderWidth, TriangleTop + TRIANGLE_SIZE + LogicSize.Y), FVector2D(0.f, 0.f),
		FIntPoint(CenterX, TriangleTop + TRIANGLE_SIZE * 2 + LogicSize.Y + BorderWidth), FVector2D(0.f, 0.f),
		BorderColor
		);
	DrawTriangle2D(Canvas,
		FIntPoint(LogicLeft, TriangleTop + TRIANGLE_SIZE + LogicSize.Y - 1), FVector2D(0.f, 0.f),
		FIntPoint(LogicRight, TriangleTop + TRIANGLE_SIZE + LogicSize.Y - 1), FVector2D(0.f, 0.f),
		FIntPoint(CenterX, TriangleTop + TRIANGLE_SIZE * 2 + LogicSize.Y - 1), FVector2D(0.f, 0.f),
		FColor(140,140,140)
		);
	// draw center rectangle
	DrawTile(Canvas,LogicLeft - 1 - BorderWidth, TriangleTop + TRIANGLE_SIZE, LogicSize.X + 2 + DBorderWidth, LogicSize.Y, 0.f, 0.f, 0.f, 0.f, BorderColor);
	DrawTile(Canvas,LogicLeft, TriangleTop + TRIANGLE_SIZE, LogicSize.X, LogicSize.Y, 0.f, 0.f, 0.f, 0.f, FColor(140,140,140));

	// try to draw the originator's sprite icon
	if (Originator != NULL)
	{
		USpriteComponent *SpriteComp = NULL;
		ULightComponent *LightComp = NULL;
		for (INT Idx = 0; Idx < Originator->Components.Num(); Idx++)
		{
			UComponent *Comp = Originator->Components(Idx);
			if (Comp != NULL)
			{
				if (Comp->IsA(USpriteComponent::StaticClass()))
				{
					if (!((USpriteComponent*)Comp)->HiddenEditor)
					{
						SpriteComp = (USpriteComponent*)Comp;
					}
				}
				else if (Comp->IsA(ULightComponent::StaticClass()))
				{
					LightComp = (ULightComponent*)Comp;
				}
				// if we found everything,
				if (SpriteComp != NULL && LightComp != NULL)
				{
					// no need to keep looking
					break;
				}
			}
		}
		if (SpriteComp != NULL)
		{
			FColor DrawColor(128,128,128,255);
			// use the light color if available
			if (LightComp != NULL)
			{
				DrawColor = LightComp->LightColor;
				DrawColor.A = 255;
			}
			UTexture2D *Sprite = SpriteComp->Sprite;
			INT SizeX = Min<INT>(Sprite->SizeX,LO_MIN_SHAPE_SIZE);
			INT SizeY = Min<INT>(Sprite->SizeY,LO_MIN_SHAPE_SIZE);
			DrawTile(Canvas, appTrunc(CenterX - SizeX/2),TriangleTop + TRIANGLE_SIZE + LogicSize.Y/2 - SizeY/2,
						 SizeX,SizeY,
						 0.f,0.f,
						 1.f,1.f,
						 DrawColor,
						 Sprite->Resource);
		}
	}

	DrawExtraInfo(Canvas,FVector(CenterX,TriangleTop + TRIANGLE_SIZE,0));

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
		DrawTile(Canvas,ObjPosX - BorderWidth, TriangleTop + TRIANGLE_SIZE * 2 + LogicSize.Y + 3 - BorderWidth, MaxWidth + DBorderWidth, VarSize.Y + DBorderWidth, 0.f, 0.f, 0.f, 0.f, BorderColor);
		DrawTile(Canvas,ObjPosX + 1, TriangleTop + TRIANGLE_SIZE * 2 + LogicSize.Y + 3 + 1, MaxWidth - 2, VarSize.Y - 2, 0.f, 0.f, 0.f, 0.f, FColor(140,140,140));

		if(bMouseOver)
		{
			DrawVariableConnectors(Canvas, FIntPoint(ObjPosX, TriangleTop + TRIANGLE_SIZE * 2 + LogicSize.Y + 3), FIntPoint(MaxWidth, VarSize.Y), MouseOverConnType, MouseOverConnIndex, VarSize.X);
		}
		else
		{
			DrawVariableConnectors(Canvas, FIntPoint(ObjPosX, TriangleTop + TRIANGLE_SIZE * 2 + LogicSize.Y + 3), FIntPoint(MaxWidth, VarSize.Y), -1, INDEX_NONE, VarSize.X);
		}
	}

	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );

	// Draw output connectors
	if (bMouseOver)
	{
		DrawLogicConnectors(Canvas, FIntPoint(LogicLeft, TriangleTop + TRIANGLE_SIZE), FIntPoint(LogicSize.X + 1, LogicSize.Y), MouseOverConnType, MouseOverConnIndex);
	}
	else
	{
		DrawLogicConnectors(Canvas, FIntPoint(LogicLeft, TriangleTop + TRIANGLE_SIZE), FIntPoint(LogicSize.X + 1, LogicSize.Y), -1, INDEX_NONE);
	}
	DrawWidth = (MaxWidth + LogicSize.X) / 2 + 1;

	if( bIsBreakpointSet )
	{
		FLOAT BreakpointImagePosX = ObjPosX - 20.f;
		FLOAT BreakpointImagePosY = ObjPosY;
		// Draw circle
		const FIntPoint CircleCenter(BreakpointImagePosX + 8.f, BreakpointImagePosY + 8.f);

		FLinkedObjDrawUtils::DrawNGon( Canvas, CircleCenter, FColor(255,0,0), LO_CIRCLE_SIDES, 8 );
	}

	UBOOL bIsPaused = GEditor->PlayWorld && GEditor->PlayWorld->GetWorldInfo()->bDebugPauseExecution;
	USequenceOp* PIESequenceOp = Cast<USequenceOp>(PIESequenceObject);
	if (PIESequenceOp && PIESequenceOp->bIsCurrentDebuggerOp && bIsPaused && bIsDebuggingActiveBorder)
	{
		// Draw arrow
		DrawTriangle2D(Canvas,
			FIntPoint(ObjPosX - 11, ObjPosY + 3), FVector2D(0.f, 0.f),
			FIntPoint(ObjPosX - 5, ObjPosY + 8), FVector2D(0.f, 0.f),
			FIntPoint(ObjPosX - 11, ObjPosY + 13), FVector2D(0.f, 0.f),
			FColor(255,192,0)
			);
		DrawTile(Canvas, ObjPosX - 19.0f, ObjPosY + 6.0f, 8.0f, 4.0f, 0.f, 0.f, 0.f, 0.f, FColor(255,192,0));
	}
}

//-----------------------------------------------------------------------------
//	USequenceVariable
//-----------------------------------------------------------------------------

void USequenceVariable::DrawSeqObj(FCanvas* Canvas, UBOOL bSelected, UBOOL bMouseOver, INT MouseOverConnType, INT MouseOverConnIndex, FLOAT MouseOverTime)
{
	const UBOOL bHitTesting = Canvas->IsHitTesting();
	const FColor BorderColor = GetBorderColor(bSelected, bMouseOver);
	UFont* FontToUse = FLinkedObjDrawUtils::GetFont();

	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HLinkedObjProxy(this) );

	// Draw comment for variable
	if(ObjComment.Len() > 0)
	{
		INT XL, YL;
		StringSize( FontToUse, XL, YL, *ObjComment );

		const INT CommentX = ObjPosX + 2;
		const INT CommentY = ObjPosY - YL - 2;
		if ( FLinkedObjDrawUtils::AABBLiesWithinViewport( Canvas, CommentX, CommentY, XL, YL ) )
		{
			FLinkedObjDrawUtils::DrawString( Canvas, CommentX, CommentY, *ObjComment, FontToUse, FColor(64,64,192) );
		}
	}

	// Draw circle
	const FIntPoint CircleCenter(ObjPosX + LO_MIN_SHAPE_SIZE / 2, ObjPosY + LO_MIN_SHAPE_SIZE / 2);

	DrawWidth = CircleCenter.X - ObjPosX;
	DrawHeight = CircleCenter.Y - ObjPosY;

	FLinkedObjDrawUtils::DrawNGon( Canvas, CircleCenter, BorderColor, LO_CIRCLE_SIDES, LO_MIN_SHAPE_SIZE / 2 );
	FLinkedObjDrawUtils::DrawNGon( Canvas, CircleCenter, FColor(140,140,140), LO_CIRCLE_SIDES, LO_MIN_SHAPE_SIZE / 2 -1 );

	// Give subclasses a chance to draw extra stuff
	DrawExtraInfo(Canvas, CircleCenter);

	// Draw the actual value of the variable in the middle of the circle
	FString VarString = GetValueStr();

	// Display realtime value when debugging is active
	if ( GKismetRealtimeDebugging )
	{
		USequenceVariable* PIEVar = Cast<USequenceVariable>(PIESequenceObject);
		if (PIEVar)
		{
			VarString = PIEVar->GetValueStr();
		}
	}

	INT XL, YL;
	StringSize(FontToUse, XL, YL, *VarString);

	// check if we need to shorten the string and add ellipses due to width constraints
	if (!bSelected &&
		VarString.Len() > 8 &&
		XL > LO_MIN_SHAPE_SIZE &&
		!this->IsA(USeqVar_Named::StaticClass()))
	{
		//TODO: calculate the optimal number of chars to fit instead of hard-coded
		VarString = VarString.Left(4) + TEXT("..") + VarString.Right(4);
		StringSize(FontToUse, XL, YL, *VarString);
	}

	{
		const FIntPoint StringPos( CircleCenter.X - XL / 2, CircleCenter.Y - YL / 2 );
		if ( FLinkedObjDrawUtils::AABBLiesWithinViewport( Canvas, StringPos.X, StringPos.Y, XL, YL ) )
		{
			FLinkedObjDrawUtils::DrawShadowedString( Canvas, StringPos.X, StringPos.Y, *VarString, FontToUse, FLinearColor::White );
		}
	}

	// If this variable has a name, write it underneath the variable
	if(VarName != NAME_None)
	{
		StringSize(FontToUse, XL, YL, *VarName.ToString());
		const FIntPoint StringPos( CircleCenter.X - XL / 2, ObjPosY + LO_MIN_SHAPE_SIZE + 2 );
		if ( FLinkedObjDrawUtils::AABBLiesWithinViewport( Canvas, StringPos.X, StringPos.Y, XL, YL ) )
		{
			FLinkedObjDrawUtils::DrawShadowedString( Canvas, StringPos.X, StringPos.Y, *VarName.ToString(), FontToUse, FColor(255,64,64) );
		}
	}

	if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
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

void USeqVar_Object::DrawExtraInfo(FCanvas* Canvas, const FVector& CircleCenter)
{
	// try to draw the object's sprite icon
	AActor *Originator = Cast<AActor>(ObjValue);
	if (Originator != NULL)
	{
		USpriteComponent *SpriteComp = NULL;
		ULightComponent *LightComp = NULL;
		for (INT Idx = 0; Idx < Originator->Components.Num(); Idx++)
		{
			UComponent *Comp = Originator->Components(Idx);
			if (Comp != NULL)
			{
				if (Comp->IsA(USpriteComponent::StaticClass()))
				{
					if (!((USpriteComponent*)Comp)->HiddenEditor)
					{
						SpriteComp = (USpriteComponent*)Comp;
					}
				}
				else if (Comp->IsA(ULightComponent::StaticClass()))
				{
					LightComp = (ULightComponent*)Comp;
				}
				// if we found everything,
				if (SpriteComp != NULL && LightComp != NULL)
				{
					// no need to keep looking
					break;
				}
			}
		}
		if (SpriteComp != NULL)
		{
			FColor DrawColor(128,128,128,255);
			// use the light color if available
			if (LightComp != NULL)
			{
				DrawColor = LightComp->LightColor;
				DrawColor.A = 255;
			}
			UTexture2D *Sprite = SpriteComp->Sprite;
			INT SizeX = Min<INT>(Sprite->SizeX,LO_MIN_SHAPE_SIZE);
			INT SizeY = Min<INT>(Sprite->SizeY,LO_MIN_SHAPE_SIZE);
			DrawTile(Canvas, appTrunc(CircleCenter.X - SizeX/2),appTrunc(CircleCenter.Y - SizeY/2),
						 SizeX,SizeY,
						 0.f,0.f,
						 1.f,1.f,
						 DrawColor,
						 Sprite->Resource);
		}
	}
}

void USeqVar_ObjectVolume::DrawExtraInfo(FCanvas* Canvas, const FVector& CircleCenter)
{
	Super::DrawExtraInfo(Canvas,CircleCenter);
	// draw a little mark to differentiate this object var from all other types
	DrawTile(Canvas,appTrunc(CircleCenter.X - LO_MIN_SHAPE_SIZE/2), appTrunc(CircleCenter.Y - LO_MIN_SHAPE_SIZE/2),
				 8, 8,
				 0.f, 0.f,
				 1.f, 1.f,
				 ObjColor,
				 NULL);
	DrawTile(Canvas,appTrunc(CircleCenter.X + LO_MIN_SHAPE_SIZE/2), appTrunc(CircleCenter.Y - LO_MIN_SHAPE_SIZE/2),
				 8, 8,
				 0.f, 0.f,
				 1.f, 1.f,
				 ObjColor,
				 NULL);
}

//-----------------------------------------------------------------------------
//	USeqVar_ObjectList
//-----------------------------------------------------------------------------

void USeqVar_ObjectList::DrawExtraInfo(FCanvas* Canvas, const FVector& CircleCenter)
{

	// if the list is empty then return as we can't draw anything
	if( 0 == ObjList.Num() )	
	{
		return;
	}
	// try to draw the object's sprite icon of the FIRST entry in the list
	AActor* Originator = Cast<AActor>(ObjList(0));
	if (Originator != NULL)
	{
		USpriteComponent* spriteComponent = NULL;
		for (INT idx = 0; idx < Originator->Components.Num() && spriteComponent == NULL; idx++)
		{
			// check if this is a sprite component
			spriteComponent = Cast<USpriteComponent>(Originator->Components(idx));
		}
		if (spriteComponent != NULL)
		{
			UTexture2D *sprite = spriteComponent->Sprite;
			DrawTile(Canvas, appTrunc(CircleCenter.X - sprite->SizeX/2)
				,appTrunc(CircleCenter.Y - sprite->SizeY / 2)
				,sprite->SizeX,sprite->SizeY
				,0.f,0.f
				,1.f,1.f
				,FColor(102,0,102,255)
				,sprite->Resource
				);
		}
	}
}

//-----------------------------------------------------------------------------
//	USeqVar_Named
//-----------------------------------------------------------------------------

/** Draw cross or tick to indicate status of this USeqVar_Named. */
void USeqVar_Named::DrawExtraInfo(FCanvas* Canvas, const FVector& CircleCenter)
{
	// Ensure materials are there.
	if(!GEngine->TickMaterial || !GEngine->CrossMaterial)
	{
		return;
	}

	const UMaterial* UseMaterial = bStatusIsOk ? GEngine->TickMaterial : GEngine->CrossMaterial;
	DrawTile(Canvas, appTrunc(CircleCenter.X-16), appTrunc(CircleCenter.Y-16), 32, 32, 0.f, 0.f, 1.f, 1.f, UseMaterial->GetRenderProxy(0) );
}

//-----------------------------------------------------------------------------
//	USeqAct_ActivateRemoteEvent
//-----------------------------------------------------------------------------

/** Draw cross or tick to indicate status of this remote event (has at least one matching event). */
void USeqAct_ActivateRemoteEvent::DrawExtraInfo(FCanvas* Canvas, const FVector& BoxCenter)
{
	// Ensure materials are there.
	if(!GEngine->TickMaterial || !GEngine->CrossMaterial)
	{
		return;
	}

	const UMaterial* UseMaterial = bStatusIsOk ? GEngine->TickMaterial : GEngine->CrossMaterial;
	DrawTile(Canvas, appTrunc(BoxCenter.X-16), appTrunc(BoxCenter.Y-8), 32, 32, 0.f, 0.f, 1.f, 1.f, UseMaterial->GetRenderProxy(0) );
}

FString USeqAct_ActivateRemoteEvent::GetDisplayTitle() const
{
	FString Title = Super::GetDisplayTitle();

	if (EventName != NAME_None)
	{
		Title += FString::Printf( TEXT(" \"%s\""), *EventName.ToString() );
	}

	return Title;
}

/** Same for the remote events (has at least one matching action). */
void USeqEvent_RemoteEvent::DrawExtraInfo(FCanvas* Canvas, const FVector& BoxCenter)
{
	// Ensure materials are there.
	if(!GEngine->TickMaterial || !GEngine->CrossMaterial)
	{
		return;
	}

	const UMaterial* UseMaterial = bStatusIsOk ? GEngine->TickMaterial : GEngine->CrossMaterial;
	DrawTile(Canvas, appTrunc(BoxCenter.X-16), appTrunc(BoxCenter.Y-8), 32, 32, 0.f, 0.f, 1.f, 1.f, UseMaterial->GetRenderProxy(0) );
}

FString USeqEvent_RemoteEvent::GetDisplayTitle() const
{
	FString Title = Super::GetDisplayTitle();

	if (EventName != NAME_None)
	{
		Title += FString::Printf( TEXT(" \"%s\""), *EventName.ToString() );
	}

	return Title;
}

/* Level streaming actions */

void USeqAct_LevelVisibility::DrawExtraInfo(FCanvas* Canvas, const FVector& BoxCenter)
{
	// Ensure materials are there.
	if(!GEngine->TickMaterial || !GEngine->CrossMaterial)
	{
		return;
	}

	const UMaterial* UseMaterial = bStatusIsOk ? GEngine->TickMaterial : GEngine->CrossMaterial;
	DrawTile(Canvas, appTrunc(BoxCenter.X-16), appTrunc(BoxCenter.Y-8), 32, 32, 0.f, 0.f, 1.f, 1.f, UseMaterial->GetRenderProxy(0) );
}


void USeqAct_LevelStreaming::DrawExtraInfo(FCanvas* Canvas, const FVector& BoxCenter)
{
	// Ensure materials are there.
	if(!GEngine->TickMaterial || !GEngine->CrossMaterial)
	{
		return;
	}

	const UMaterial* UseMaterial = bStatusIsOk ? GEngine->TickMaterial : GEngine->CrossMaterial;
	DrawTile(Canvas, appTrunc(BoxCenter.X-16), appTrunc(BoxCenter.Y-8), 32, 32, 0.f, 0.f, 1.f, 1.f, UseMaterial->GetRenderProxy(0) );
}

void USeqAct_MultiLevelStreaming::DrawExtraInfo(FCanvas* Canvas, const FVector& BoxCenter)
{
	// Ensure materials are there.
	if(!GEngine->TickMaterial || !GEngine->CrossMaterial)
	{
		return;
	}

	const UMaterial* UseMaterial = bStatusIsOk ? GEngine->TickMaterial : GEngine->CrossMaterial;
	DrawTile(Canvas, appTrunc(BoxCenter.X-16), appTrunc(BoxCenter.Y-8), 32, 32, 0.f, 0.f, 1.f, 1.f, UseMaterial->GetRenderProxy(0) );
}

void USeqAct_PrepareMapChange::DrawExtraInfo(FCanvas* Canvas, const FVector& BoxCenter)
{
	// Ensure materials are there.
	if(!GEngine->TickMaterial || !GEngine->CrossMaterial)
	{
		return;
	}

	const UMaterial* UseMaterial = bStatusIsOk ? GEngine->TickMaterial : GEngine->CrossMaterial;
	DrawTile(Canvas, appTrunc(BoxCenter.X-16), appTrunc(BoxCenter.Y-8), 32, 32, 0.f, 0.f, 1.f, 1.f, UseMaterial->GetRenderProxy(0) );
}


//-----------------------------------------------------------------------------
//	USequenceFrame
//-----------------------------------------------------------------------------

void USequenceFrame::DrawFrameBox(FCanvas* Canvas, UBOOL bSelected)
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
				DrawTile(Canvas, ObjPosX, ObjPosY, SizeX, SizeY, 0.f, 0.f, (FLOAT)SizeX/64.f, (FLOAT)SizeY/64.f, FillMaterial->GetRenderProxy(0) );
			}
			else
			{
				DrawTile(Canvas, ObjPosX, ObjPosY, SizeX, SizeY, 0.f, 0.f, 1.f, 1.f, FillMaterial->GetRenderProxy(0) );
			}
		}
		else if(FillTexture)
		{
			if(bTileFill)
			{
				DrawTile(Canvas, ObjPosX, ObjPosY, SizeX, SizeY, 0.f, 0.f, (FLOAT)SizeX/64.f, (FLOAT)SizeY/64.f, FillColor, FillTexture->Resource );
			}
			else
			{
				DrawTile(Canvas, ObjPosX, ObjPosY, SizeX, SizeY, 0.f, 0.f, 1.f, 1.f, FillColor, FillTexture->Resource );
			}
		}
		// .. otherwise just use a solid color.
		else
		{
			DrawTile(Canvas, ObjPosX, ObjPosY, SizeX, SizeY, 0.f, 0.f, 1.f, 1.f, FillColor );
		}
	}

	// Draw frame
	const FColor FrameColor = bSelected ? FColor(255,255,0) : BorderColor;

	const INT MinDim = Min(SizeX, SizeY);
	const INT UseBorderWidth = Clamp( BorderWidth, 0, (MinDim/2)-3 );

	for(INT i=0; i<UseBorderWidth; i++)
	{
		DrawLine2D(Canvas, FVector2D(ObjPosX,				ObjPosY + i),			FVector2D(ObjPosX + SizeX,		ObjPosY + i),			FrameColor );
		DrawLine2D(Canvas, FVector2D(ObjPosX + SizeX - i,	ObjPosY),				FVector2D(ObjPosX + SizeX - i,	ObjPosY + SizeY),		FrameColor );
		DrawLine2D(Canvas, FVector2D(ObjPosX + SizeX,		ObjPosY + SizeY - i),	FVector2D(ObjPosX,				ObjPosY + SizeY - i),	FrameColor );
		DrawLine2D(Canvas, FVector2D(ObjPosX + i,			ObjPosY + SizeY),		FVector2D(ObjPosX + i,			ObjPosY - 1),			FrameColor );
	}

	// Draw little sizing triangle in bottom left.
	const INT HandleSize = 16;
	const FIntPoint A(ObjPosX + SizeX,				ObjPosY + SizeY);
	const FIntPoint B(ObjPosX + SizeX,				ObjPosY + SizeY - HandleSize);
	const FIntPoint C(ObjPosX + SizeX - HandleSize,	ObjPosY + SizeY);
	const BYTE TriangleAlpha = (bSelected) ? 255 : 32; // Make it more transparent if comment is not selected.

	const UBOOL bHitTesting = Canvas->IsHitTesting();

	if(bHitTesting)  Canvas->SetHitProxy( new HLinkedObjProxySpecial(this, 1) );
	DrawTriangle2D(Canvas, A, FVector2D(0,0), B, FVector2D(0,0), C, FVector2D(0,0), FColor(0,0,0,TriangleAlpha) );
	if(bHitTesting)  Canvas->SetHitProxy( NULL );
}

void USequenceFrame::DrawSeqObj(FCanvas* Canvas, UBOOL bSelected, UBOOL bMouseOver, INT MouseOverConnType, INT MouseOverConnIndex, FLOAT MouseOverTime)
{
	// Draw box
	if(bDrawBox)
	{
		DrawFrameBox(Canvas, bSelected);
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

	const FLOAT OldZoom2D = FLinkedObjDrawUtils::GetUniformScaleFromMatrix(Canvas->GetTransform());
	UFont* FontToUse = FLinkedObjDrawUtils::GetFont();

	FTextSizingParameters Parameters(FontToUse, 1.f, 1.f);
	FLOAT& XL = Parameters.DrawXL, &YL = Parameters.DrawYL;

	UCanvas::CanvasStringSize( Parameters, *ObjComment );

	// We always draw comment-box text at normal size (don't scale it as we zoom in and out.)
	const INT x = appTrunc(ObjPosX*OldZoom2D + 2);
	const INT y = appTrunc(ObjPosY*OldZoom2D - YL - 2);


	// Viewport cull at a zoom of 1.0, because that's what we'll be drawing with.
	if ( FLinkedObjDrawUtils::AABBLiesWithinViewport( Canvas, ObjPosX, ObjPosY-YL-2, SizeX * OldZoom2D, YL ) )
	{
		Canvas->PushRelativeTransform(FScaleMatrix(1.0f / OldZoom2D));
		{
			const UBOOL bHitTesting = Canvas->IsHitTesting();

			DrawString(Canvas, x, y, *ObjComment, FontToUse, FColor(64,64,192) );

			// We only set the hit proxy for the area above the comment box with the height of the comment text
			if(bHitTesting)
			{
				Canvas->SetHitProxy(new HLinkedObjProxy(this));

				// account for the +2 when x was assigned
				DrawTile(Canvas, x - 2, y, SizeX * OldZoom2D, YL, 0.f, 0.f, 1.f, 1.f, FLinearColor(1.0f, 0.0f, 0.0f));

				Canvas->SetHitProxy(NULL);
			}
		}
		Canvas->PopTransform();
	}

	// Fill in base SequenceObject rendering info (used by bounding box calculation).
	DrawWidth = SizeX;
	DrawHeight = SizeY;
}

void USequenceFrameWrapped::DrawSeqObj(FCanvas* Canvas, UBOOL bSelected, UBOOL bMouseOver, INT MouseOverConnType, INT MouseOverConnIndex, FLOAT MouseOverTime)
{
	// Draw box
	if(bDrawBox)
	{
		DrawFrameBox(Canvas, bSelected);
	}

	FString CommentText = ObjComment;

	// Ensure there is at least one word!
	if(CommentText.Len() == 0)
	{
		CommentText = TEXT("Comment");
	}

	const UBOOL bHitTesting = Canvas->IsHitTesting();
	if(bHitTesting)  
	{
		Canvas->SetHitProxy( new HLinkedObjProxy(this) );
	}

	FLOAT W = SizeX - 10, H = SizeY - 10;

	FLinkedObjDrawUtils::DisplayComment( Canvas, TRUE, ObjPosX+10, ObjPosY+10, 1.0f, W, H, FLinkedObjDrawUtils::GetFont(), *CommentText, FColor(255,255,255) );

	if(bHitTesting) 
	{
		Canvas->SetHitProxy( NULL );
	}
}

/**
 * Returns the color that should be used for an input, variable, or output link connector in the kismet editor.
 *
 * @param	ConnType	the type of connection this represents.  Valid values are:
 *							LOC_INPUT		(input link)
 *							LOC_OUTPUT		(output link)
 *							LOC_VARIABLE	(variable link)
 *							LOC_EVENT		(event link)
 * @param	ConnIndex	the index [into the corresponding array (i.e. InputLinks, OutputLinks, etc.)] for the link
 *						being queried.
 * @param	MouseOverConnType
 *						INDEX_NONE if the user is not currently mousing over the specified link connector.  One of the values
 *						listed for ConnType otherwise.
 * @param	MouseOverConnIndex
 *						INDEX_NONE if the user is not currently mousing over the specified link connector.  The index for the
 *						link being moused over otherwise.
 */
FColor USeqCond_SwitchBase::GetConnectionColor( INT ConnType, INT ConnIndex, INT MouseOverConnType, INT MouseOverConnIndex )
{
	if( ConnType == LOC_OUTPUT )
	{
		if( (MouseOverConnType != LOC_OUTPUT || MouseOverConnIndex != ConnIndex) && !eventIsFallThruEnabled(ConnIndex) ) 
		{
			return FColor(255,0,0);
		}
	}

	return Super::GetConnectionColor( ConnType, ConnIndex, MouseOverConnType, MouseOverConnIndex );
}

/** Overridden to append ExtraDelay information to comment area. */
void USeqAct_PlaySound::DrawTitleBar(FCanvas* Canvas, UBOOL bSelected, UBOOL bMouseOver, const FIntPoint& Pos, const FIntPoint& Size)
{
	if (ExtraDelay > 0.f)
	{
		FString OrigObjComment = ObjComment;
		ObjComment = FString::Printf(TEXT("%s (ExtraDelay=%.2f)"), *ObjComment, ExtraDelay);
		Super::DrawTitleBar(Canvas, bSelected, bMouseOver, Pos, Size);
		ObjComment = OrigObjComment;
	}
	else
	{
		Super::DrawTitleBar(Canvas, bSelected, bMouseOver, Pos, Size);
	}
}


FString USeqAct_Delay::GetDisplayTitle() const
{
	FString Title = Super::GetDisplayTitle();

	// if duration is a constant and not set via variable, display delay in title
	if ( (VariableLinks.Num() > 0) && (VariableLinks(0).LinkedVariables.Num() == 0) )
	{
		Title += FString::Printf(TEXT(" (%.2f)"), Duration);
	}

	return Title;
}

void USeqAct_Delay::GetRealtimeComments(TArray<FString> &OutComments)
{
	if (bActive && RemainingTime >= 0.0f)
	{
		OutComments.AddItem(FString::Printf(TEXT("Remaining : %.2f"), RemainingTime));
	}

	Super::GetRealtimeComments(OutComments);
}

FString USequenceEvent::GetDisplayTitle() const
{
	FString Title = Super::GetDisplayTitle();

	if (bClientSideOnly)
	{
		Title += TEXT(" (ClientSideOnly)");
	}
	if (!bEnabled)
	{
		Title += TEXT(" (Disabled)");
	}

	return Title;
}

FString USeqEvent_Console::GetDisplayTitle() const
{
	FString Title = Super::GetDisplayTitle();

	if (ConsoleEventName != NAME_None)
	{
		Title += FString::Printf( TEXT(" \"%s\""), *ConsoleEventName.ToString() );
	}

	return Title;
}


FString USeqAct_FinishSequence::GetDisplayTitle() const
{
	FString Title = Super::GetDisplayTitle();

	if (OutputLabel.Len() > 0)
	{
		Title += FString::Printf( TEXT(" \"%s\""), *OutputLabel );
	}

	return Title;
}

FString USeqEvent_SequenceActivated::GetDisplayTitle() const
{
	FString Title = Super::GetDisplayTitle();

	if (InputLabel.Len() > 0)
	{
		Title += FString::Printf( TEXT(" \"%s\""), *InputLabel );
	}

	return Title;
}

// Display the current damage accumulated by TakeDamage events.
void USeqEvent_TakeDamage::GetRealtimeComments(TArray<FString> &OutComments)
{
	if (bEnabled)
	{
		OutComments.AddItem(FString::Printf(TEXT("CurrentDamage : %d / %d"), (INT)CurrentDamage, (INT)DamageThreshold));
	}

	Super::GetRealtimeComments(OutComments);
}

#if WITH_EDITOR
FString USeqAct_Gate::GetAutoComment() const
{
	FString AutoComment;

	for( TFieldIterator<UProperty> It( GetClass() ); It; ++It )
	{
		UProperty* Property = *It;
		
		// AutoCloseCount is a special case that should only display when > 0
		if(Property && (Property->PropertyFlags & CPF_Edit) && (Property->GetName() == FString("AutoCloseCount")))
		{
			UIntProperty* IntProp = Cast<UIntProperty>(Property);
			if(IntProp)
			{
				INT* IntPtr = (INT*)(((BYTE*)this) + IntProp->Offset);
				if(*IntPtr > 0)
				{
					AutoComment += FString::Printf(TEXT("%s=%d "), *(IntProp->GetName()), *IntPtr);
				}
			}
		}
	}

	return (AutoComment + Super::GetAutoComment());
}
#endif

// Display the current value of bOpen.
void USeqAct_Gate::GetRealtimeComments(TArray<FString> &OutComments)
{
	OutComments.AddItem(FString::Printf(TEXT("bOpen:%s"), bOpen?GTrue:GFalse));

	Super::GetRealtimeComments(OutComments);
}
