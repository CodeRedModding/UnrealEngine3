/*=============================================================================
	UnAnimTreeDraw.cpp: Function for drawing different AnimNode classes for AnimTreeEditor
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "UnLinkedObjDrawUtils.h"

static const FColor SkelControlTitleColorDisabled(50,100,50);

static const FColor FullWeightColor(255, 130, 30);
static const FColor ZeroWeightColor(0,0,0);
/** BackGround color when a node has been deprecated */
static const FColor AnimTree_DeprecatedBGColor(200,0,0);

/** Color to draw a selected object */
static const FColor AnimTree_SelectedColor( 255, 255, 0 );

static inline FColor GetNodeColorFromWeight(FLOAT Weight)
{
	return FColor( ZeroWeightColor.R + (BYTE)(Weight * (FLOAT)(FullWeightColor.R - ZeroWeightColor.R)),
					ZeroWeightColor.G + (BYTE)(Weight * (FLOAT)(FullWeightColor.G - ZeroWeightColor.G)),
					ZeroWeightColor.B + (BYTE)(Weight * (FLOAT)(FullWeightColor.B - ZeroWeightColor.B)) );
}

/*-----------------------------------------------------------------------------
	UAnimTree
-----------------------------------------------------------------------------*/

FIntPoint UAnimTree::GetConnectionLocation(INT ConnType, INT ConnIndex)
{
#if WITH_EDITORONLY_DATA
	if(ConnType == LOC_INPUT)
	{
		check(ConnIndex == 0);
		return FIntPoint( NodePosX - LO_CONNECTOR_LENGTH, OutDrawY );
	}
	else if(ConnType == LOC_OUTPUT)
	{
		if(ConnIndex == 0)
		{
			return FIntPoint( NodePosX + DrawWidth + LO_CONNECTOR_LENGTH, Children(0).DrawY );
		}
		else if(ConnIndex == 1)
		{
			return FIntPoint( NodePosX + DrawWidth + LO_CONNECTOR_LENGTH, MorphConnDrawY );
		}
		else
		{
			return FIntPoint( NodePosX + DrawWidth + LO_CONNECTOR_LENGTH, SkelControlLists(ConnIndex-2).DrawY );
		}
	}
#endif // WITH_EDITORONLY_DATA

	return FIntPoint( 0, 0 );
}

/**
 * Draws this node in the AnimTreeEditor.
 *
 * @param	Canvas			The canvas to use.
 * @param	SelectedNodes	Reference to array of all currently selected nodes, potentially including this node
 * @param	bShowWeight		If TRUE, show the global percentage weight of this node, if applicable.
 */
void UAnimTree::DrawAnimNode( FCanvas* Canvas, const TArray<UAnimObject*>& SelectedNodes, UBOOL bShowWeight)
{
#if WITH_EDITORONLY_DATA
	check(Children.Num() == 1);

	// Construct the FLinkedObjDrawInfo for use the linked-obj drawing utils.
	FLinkedObjDrawInfo ObjInfo;

	const FColor WeightOneColor( GetNodeColorFromWeight(1.f) );

	// Add one connector for animation tree.
	ObjInfo.Outputs.AddItem( FLinkedObjConnInfo(TEXT("Animation"),WeightOneColor) );

	// Add one connector for morph nodes.
	ObjInfo.Outputs.AddItem( FLinkedObjConnInfo(TEXT("Morph"), FColor(50,50,100)) );

	// Add connector for attaching controller chains.
	for(INT i=0; i<SkelControlLists.Num(); i++)
	{
		ObjInfo.Outputs.AddItem( FLinkedObjConnInfo(*SkelControlLists(i).BoneName.ToString(),SkelControlTitleColorDisabled) );
	}

	ObjInfo.ObjObject = this;

	// Generate border color
	const UBOOL bSelected = SelectedNodes.ContainsItem( this );
	const FColor& BorderColor = bSelected ? AnimTree_SelectedColor : FColor(0, 0, 200);

	// Font color
	const FColor FontColor = FColor( 255, 255, 128 );

	// Use util to draw box with connectors etc.
	FString ControlDesc = GetClass()->GetDescription();
	FLinkedObjDrawUtils::DrawLinkedObj( Canvas, ObjInfo, *ControlDesc, NULL, FontColor, BorderColor, FColor(112,112,112), FIntPoint(NodePosX, NodePosY) );

	// Read back draw locations of connectors, so we can draw lines in the correct places.
	Children(0).DrawY = ObjInfo.OutputY(0);
	MorphConnDrawY = ObjInfo.OutputY(1);

	for(INT i=0; i<SkelControlLists.Num(); i++)
	{
		SkelControlLists(i).DrawY = ObjInfo.OutputY(i+2);
	}

	DrawWidth = ObjInfo.DrawWidth;

	// Now draw links to animation children
	UAnimNode* ChildNode = Children(0).Anim;
	if(ChildNode)
	{
		FIntPoint Start = GetConnectionLocation(LOC_OUTPUT, 0);
		FIntPoint End = ChildNode->GetConnectionLocation(LOC_INPUT, 0);

		FLOAT Tension = Abs<INT>(Start.X - End.X);
		const FColor& ChildNodeDrawColor = ( bSelected || SelectedNodes.ContainsItem( ChildNode ) ) ? AnimTree_SelectedColor : GetNodeColorFromWeight( 1.0f );
		FLinkedObjDrawUtils::DrawSpline(Canvas, End, -Tension * FVector2D(1,0), Start, -Tension * FVector2D(1,0), ChildNodeDrawColor, TRUE);
	}

	// Draw links to child morph nodes.
	for(INT i=0; i<RootMorphNodes.Num(); i++)
	{
		UMorphNodeBase* MorphNode = RootMorphNodes(i);
		if(MorphNode)
		{
			FIntPoint Start = GetConnectionLocation(LOC_OUTPUT, 1);
			FIntPoint End = MorphNode->GetConnectionLocation(LOC_INPUT, 0);

			FLOAT Tension = Abs<INT>(Start.X - End.X);
			const FColor& MorphNodeDrawColor = ( bSelected || SelectedNodes.ContainsItem( MorphNode ) ) ? AnimTree_SelectedColor : FColor( 50, 50, 100 );
			FLinkedObjDrawUtils::DrawSpline(Canvas, End, -Tension * FVector2D(1,0), Start, -Tension * FVector2D(1,0), MorphNodeDrawColor, TRUE);
		}
	}

	// If an AnimTree, draw links to start of head SkelControl list.
	for(INT i=0; i<SkelControlLists.Num(); i++)
	{
		USkelControlBase* Control = SkelControlLists(i).ControlHead;
		if(Control)
		{
			FIntPoint Start = GetConnectionLocation(LOC_OUTPUT, i+2);
			FIntPoint End = Control->GetConnectionLocation(LOC_INPUT);

			FLOAT Tension = Abs<INT>(Start.X - End.X);
			const FColor& LineColor = ( bSelected || SelectedNodes.ContainsItem( Control ) ) ? AnimTree_SelectedColor : SkelControlTitleColorDisabled;
			FLinkedObjDrawUtils::DrawSpline( Canvas, Start, Tension * FVector2D(1,0), End, Tension * FVector2D(1,0), LineColor, TRUE );
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/*-----------------------------------------------------------------------------
  UAnimNode
-----------------------------------------------------------------------------*/

FIntPoint UAnimNode::GetConnectionLocation(INT ConnType, INT ConnIndex)
{
#if WITH_EDITORONLY_DATA
	if(ConnType == LOC_INPUT)
	{
		check(ConnIndex == 0);
		return FIntPoint( NodePosX - LO_CONNECTOR_LENGTH, OutDrawY );
	}
#endif // WITH_EDITORONLY_DATA

	return FIntPoint( 0, 0 );
}

/*-----------------------------------------------------------------------------
  UAnimNodeBlendBase
-----------------------------------------------------------------------------*/

FString UAnimNodeBlendBase::GetNodeTitle()
{
	// Generate name for node. User-give name if entered - otherwise just class name.
	FString NodeTitle;

#if WITH_EDITORONLY_DATA
	const FString NodeDesc = GetClass()->GetDescription(); // Need to assign it here, so pointer isn't invalid by the time we draw it.
	if( NodeName != NAME_None )
	{
		NodeTitle = NodeName.ToString();
	}
	else
	{
		NodeTitle = NodeDesc;
	}
#endif // WITH_EDITORONLY_DATA

	return NodeTitle;
}

/**
 * Draws this node in the AnimTreeEditor.
 *
 * @param	Canvas			The canvas to use.
 * @param	SelectedNodes	Reference to array of all currently selected nodes, potentially including this node
 * @param	bShowWeight		If TRUE, show the global percentage weight of this node, if applicable.
 */
void UAnimNodeBlendBase::DrawAnimNode( FCanvas* Canvas, const TArray<UAnimObject*>& SelectedNodes, UBOOL bShowWeight)
{
#if WITH_EDITORONLY_DATA
	// Construct the FLinkedObjDrawInfo for use the linked-obj drawing utils.
	FLinkedObjDrawInfo ObjInfo;

	// AnimTree's don't have an output connector on left
	ObjInfo.Inputs.AddItem( FLinkedObjConnInfo(TEXT("Out"), GetNodeColorFromWeight(NodeTotalWeight) ) );

	// Add output for each child.
	for(INT i=0; i<Children.Num(); i++)
	{
		UAnimNode* ChildNode = Children(i).Anim;
		if( ChildNode )
		{
			ObjInfo.Outputs.AddItem( FLinkedObjConnInfo(*Children(i).Name.ToString(), GetNodeColorFromWeight(NodeTotalWeight * Children(i).Weight)) );
		}
		else
		{
			ObjInfo.Outputs.AddItem( FLinkedObjConnInfo(*Children(i).Name.ToString(), FColor(0,0,0)) );
		}
	}

	ObjInfo.ObjObject = this;

	// Generate border color
	const UBOOL bSelected = SelectedNodes.ContainsItem( this );
	const FColor& BorderColor =  bSelected ? AnimTree_SelectedColor : GetNodeColorFromWeight( NodeTotalWeight );

	FString NodeTitle = GetNodeTitle();

	// Draw deprecated nodes with a red color background
	const FColor& BackGroundColor = (GetClass()->ClassFlags & CLASS_Deprecated) ? AnimTree_DeprecatedBGColor : FColor(112,112,112);

	// Font color
	const FColor FontColor = FColor( 255, 255, 128 );

	// Use util to draw box with connectors etc.
	FLinkedObjDrawUtils::DrawLinkedObj( Canvas, ObjInfo, *NodeTitle, NULL, FontColor, BorderColor, BackGroundColor, FIntPoint(NodePosX, NodePosY) );

	// Read back draw locations of connectors, so we can draw lines in the correct places.
	OutDrawY = ObjInfo.InputY(0);

	for(INT i=0; i<Children.Num(); i++)
	{
		Children(i).DrawY = ObjInfo.OutputY(i);
	}

	DrawWidth = ObjInfo.DrawWidth;
	DrawHeight = ObjInfo.DrawHeight;

	// Show the global percentage weight of this node if we wish.
	if( bShowWeight )
	{
		const FString WeightString = FString::Printf( TEXT("%d - %2.1f pct"), ParentNodes.Num(), NodeTotalWeight * 100.f );
		//const FString WeightString = FString::Printf( TEXT("%2.1f pct"), NodeTotalWeight * 100.f );

		INT XL, YL;
		StringSize(GEngine->SmallFont, XL, YL, *WeightString);

		DrawShadowedString(Canvas, NodePosX + DrawWidth - XL, NodePosY - YL, *WeightString, GEngine->SmallFont, GetNodeColorFromWeight(NodeTotalWeight) );
	}

	INT SliderDrawY = NodePosY + ObjInfo.DrawHeight;

	// Draw sliders underneath this node if desired.
	const UBOOL bDrawTextOnSide = GetNumSliders() > 1;

	// Draw deprecated nodes with a red color background
	const FColor& SliderBackGroundColor = (GetClass()->ClassFlags & CLASS_Deprecated) ? AnimTree_DeprecatedBGColor : FColor(140,140,140);

	for(INT i=0; i<GetNumSliders(); ++i)
	{
		if(GetSliderType(i) == ST_1D)
		{
			SliderDrawY += FLinkedObjDrawUtils::DrawSlider(Canvas, FIntPoint(NodePosX, SliderDrawY), DrawWidth, BorderColor, SliderBackGroundColor, GetSliderPosition(i, 0), GetSliderDrawValue(i), this, i, bDrawTextOnSide);
		}
		else if(GetSliderType(i) == ST_2D)
		{
			FIntPoint SliderPos(NodePosX, SliderDrawY);
			INT BoxHeight = FLinkedObjDrawUtils::Draw2DSlider(Canvas, SliderPos, DrawWidth, BorderColor, SliderBackGroundColor, GetSliderPosition(i, 0), GetSliderPosition(i, 1), GetSliderDrawValue(i), this, i, bDrawTextOnSide);
			
			UBOOL bAABBLiesWithinViewport = FLinkedObjDrawUtils::AABBLiesWithinViewport(Canvas, SliderPos.X, SliderPos.Y, DrawWidth, DrawWidth);
			BoxHeight += Extend2DSlider(Canvas, SliderPos, DrawWidth, bAABBLiesWithinViewport, LO_SLIDER_HANDLE_HEIGHT);
			SliderDrawY += BoxHeight;
		}
		else
		{
			check(FALSE);
		}
	}

	// Now draw links to animation children
	for(INT i=0; i<Children.Num(); i++)
	{
		UAnimNode* ChildNode = Children(i).Anim;
		if(ChildNode)
		{
			FIntPoint Start = GetConnectionLocation(LOC_OUTPUT, i);
			FIntPoint End = ChildNode->GetConnectionLocation(LOC_INPUT, 0);

			FLOAT Tension = Abs<INT>(Start.X - End.X);
			const FColor& ChildNodeDrawColor = ( SelectedNodes.ContainsItem( ChildNode ) ) ? AnimTree_SelectedColor : GetNodeColorFromWeight( NodeTotalWeight * Children(i).Weight );
			FLinkedObjDrawUtils::DrawSpline(Canvas, End, -Tension * FVector2D(1,0), Start, -Tension * FVector2D(1,0), ChildNodeDrawColor, TRUE);
		}
	}
#endif // WITH_EDITORONLY_DATA
}

FIntPoint UAnimNodeBlendBase::GetConnectionLocation(INT ConnType, INT ConnIndex)
{
#if WITH_EDITORONLY_DATA
	if(ConnType == LOC_INPUT)
	{
		check(ConnIndex == 0);
		return FIntPoint( NodePosX - LO_CONNECTOR_LENGTH, OutDrawY );
	}
	else if(ConnType == LOC_OUTPUT)
	{
		check( ConnIndex >= 0 && ConnIndex < Children.Num() );
		return FIntPoint( NodePosX + DrawWidth + LO_CONNECTOR_LENGTH, Children(ConnIndex).DrawY );
	}
#endif // WITH_EDITORONLY_DATA

	return FIntPoint( 0, 0 );
}

/*-----------------------------------------------------------------------------
  UAnimNodeSequence
-----------------------------------------------------------------------------*/

FString UAnimNodeSequence::GetNodeTitle()
{
	FString NodeTitle;
#if WITH_EDITORONLY_DATA
	if ( NodeName != NAME_None )
	{
		NodeTitle += FString::Printf(TEXT("[%s]"), *NodeName.ToString());
	}

	NodeTitle += FString::Printf(TEXT("%s"), *AnimSeqName.ToString());
#endif // WITH_EDITORONLY_DATA
	return NodeTitle;
}

/**
 * Draws this node in the AnimTreeEditor.
 *
 * @param	Canvas			The canvas to use.
 * @param	SelectedNodes	Reference to array of all currently selected nodes, potentially including this node
 * @param	bShowWeight		If TRUE, show the global percentage weight of this node, if applicable.
 */
void UAnimNodeSequence::DrawAnimNode(FCanvas* Canvas, const TArray<UAnimObject*>& SelectedNodes, UBOOL bShowWeight)
{
#if WITH_EDITORONLY_DATA
	FLinkedObjDrawInfo ObjInfo;

	ObjInfo.Inputs.AddItem(FLinkedObjConnInfo(TEXT("Out"), GetNodeColorFromWeight(NodeTotalWeight)));
	ObjInfo.ObjObject = this;

	UBOOL bSelected = SelectedNodes.ContainsItem( this );
	const FColor& BorderColor = bSelected ? AnimTree_SelectedColor : GetNodeColorFromWeight(NodeTotalWeight);

	// Generate name for node. User-give name if entered - otherwise just class name.
	FString NodeTitle = GetNodeTitle();

	// Draw deprecated nodes with a red color background
	const FColor& BackGroundColor = (GetClass()->ClassFlags & CLASS_Deprecated) ? AnimTree_DeprecatedBGColor : FColor(100,50,50);

	// Font color
	const FColor FontColor = FColor( 255, 255, 128 );

	FLinkedObjDrawUtils::DrawLinkedObj(Canvas, ObjInfo, *NodeTitle, NULL, FontColor, BorderColor, BackGroundColor, FIntPoint(NodePosX, NodePosY));

	DrawWidth	= ObjInfo.DrawWidth;
	DrawHeight	= ObjInfo.DrawHeight;
	OutDrawY	= ObjInfo.InputY(0);

	// Show the global percentage weight of this node if we wish.
	if( bShowWeight )
	{
		//const FString WeightString = FString::Printf( TEXT("%d - %2.1f pct"), ParentNodes.Num(), NodeTotalWeight * 100.f );
		const FString WeightString = FString::Printf( TEXT("%2.1f pct"), NodeTotalWeight * 100.f );
		INT XL, YL;
		StringSize(GEngine->SmallFont, XL, YL, *WeightString);

		DrawShadowedString(Canvas, NodePosX + DrawWidth - XL, NodePosY - YL, *WeightString, GEngine->SmallFont, GetNodeColorFromWeight(NodeTotalWeight) );
	}

	INT SliderDrawY = NodePosY + ObjInfo.DrawHeight;

	// Draw tick indicating animation position.
	FLOAT AnimPerc = GetNormalizedPosition();
	INT DrawPosX = NodePosX + appRound(AnimPerc * (FLOAT)DrawWidth);
	DrawTile(Canvas, DrawPosX, SliderDrawY, 2, 5, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black);
	SliderDrawY += 5;
	
	// Draw sliders underneath this node if desired.
	const UBOOL bDrawTextOnSide = GetNumSliders() > 1;

	// Draw deprecated nodes with a red color background
	const FColor SliderBackGroundColor = (GetClass()->ClassFlags & CLASS_Deprecated) ? AnimTree_DeprecatedBGColor : FColor(140,140,140);

	for(INT i=0; i<GetNumSliders(); ++i)
	{
		if( GetSliderType(i) == ST_1D )
		{
			SliderDrawY += FLinkedObjDrawUtils::DrawSlider(Canvas, FIntPoint(NodePosX, SliderDrawY), DrawWidth, BorderColor, SliderBackGroundColor, GetSliderPosition(i, 0), GetSliderDrawValue(i), this, i, bDrawTextOnSide);
		}
		else if( GetSliderType(i) == ST_2D )
		{
			SliderDrawY += FLinkedObjDrawUtils::Draw2DSlider(Canvas, FIntPoint(NodePosX, SliderDrawY), DrawWidth, BorderColor, SliderBackGroundColor, GetSliderPosition(i, 0), GetSliderPosition(i, 1), GetSliderDrawValue(i), this, i, bDrawTextOnSide);
		}
		else
		{
			check(FALSE);
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/*-----------------------------------------------------------------------------
	UAnimNodeBlend
-----------------------------------------------------------------------------*/

FLOAT UAnimNodeBlend::GetSliderPosition(INT SliderIndex, INT ValueIndex)
{
	check(0 == SliderIndex && 0 == ValueIndex);
	return Child2Weight;
}

void UAnimNodeBlend::HandleSliderMove(INT SliderIndex, INT ValueIndex, FLOAT NewSliderValue)
{
	check(0 == SliderIndex && 0 == ValueIndex);
	check( Children.Num() == 2 );

	Child2WeightTarget = NewSliderValue;
	Child2Weight = NewSliderValue;
}

FString UAnimNodeBlend::GetSliderDrawValue(INT SliderIndex)
{
	check(0 == SliderIndex);
	return FString::Printf( TEXT("%3.2f"), Child2Weight );
}
/*-----------------------------------------------------------------------------
	AnimNodeBlendBySpeed
-----------------------------------------------------------------------------*/

FLOAT UAnimNodeBlendBySpeed::GetSliderPosition(INT SliderIndex, INT ValueIndex)
{
	check(0 == SliderIndex && 0 == ValueIndex);
	FLOAT MaxSpeed = Constraints( Constraints.Num() - 1 );
	return Speed / (MaxSpeed * 1.1f);
}

void UAnimNodeBlendBySpeed::HandleSliderMove(INT SliderIndex, INT ValueIndex, FLOAT NewSliderValue)
{
	check(0 == SliderIndex && 0 == ValueIndex);
	FLOAT MaxSpeed = Constraints( Constraints.Num() - 1 );
	Speed = NewSliderValue * MaxSpeed * 1.1f;
}

FString UAnimNodeBlendBySpeed::GetSliderDrawValue(INT SliderIndex)
{
	check(0 == SliderIndex);
	return FString::Printf( TEXT("%3.2f"), Speed );
}

/*-----------------------------------------------------------------------------
	UAnimNodeBlendDirectional
-----------------------------------------------------------------------------*/

FLOAT UAnimNodeBlendDirectional::GetSliderPosition(INT SliderIndex, INT ValueIndex)
{
	check(0 == SliderIndex && 0 == ValueIndex);
	// DirAngle is between -PI and PI. Return value between 0.0 and 1.0 - so 0.5 is straight ahead.
	return 0.5f + (0.5f * (DirAngle / (FLOAT)PI));
}

void UAnimNodeBlendDirectional::HandleSliderMove(INT SliderIndex, INT ValueIndex, FLOAT NewSliderValue)
{
	check(0 == SliderIndex && 0 == ValueIndex);
	// Convert from 0.0 -> 1.0 to -PI to PI.
	DirAngle = (FLOAT)PI * 2.f * (NewSliderValue - 0.5f);
}

FString UAnimNodeBlendDirectional::GetSliderDrawValue(INT SliderIndex)
{
	check(0 == SliderIndex);
	return FString::Printf( TEXT("%3.2f%c"), DirAngle * (180.f/(FLOAT)PI), 176 );
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// SKELCONTROL
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/*-----------------------------------------------------------------------------
	USkelControlBase
-----------------------------------------------------------------------------*/

FIntPoint USkelControlBase::GetConnectionLocation(INT ConnType)
{
#if WITH_EDITORONLY_DATA
	static const INT ConnectorYOffset = 35;

	if(ConnType == LOC_INPUT)
	{
		return FIntPoint( NodePosX - LO_CONNECTOR_LENGTH, NodePosY + ConnectorYOffset );
	}
	else
	{
		return FIntPoint( NodePosX + DrawWidth + LO_CONNECTOR_LENGTH, NodePosY + ConnectorYOffset );
	}
#else
	return FIntPoint( 0, 0 );
#endif // WITH_EDITORONLY_DATA
}

/** 
 * Draw this SkelControl in the AnimTreeEditor.
 *
 * @param	Canvas			The canvas to use.
 * @param	SelectedNodes	Reference to array of all currently selected nodes, potentially including this node
 */
void USkelControlBase::DrawSkelControl(FCanvas* Canvas, const TArray<UAnimObject*>& SelectedNodes)
{
#if WITH_EDITORONLY_DATA
	FLinkedObjDrawInfo ObjInfo;
	ObjInfo.Inputs.AddItem( FLinkedObjConnInfo(TEXT("In"),SkelControlTitleColorDisabled) );
	ObjInfo.Outputs.AddItem( FLinkedObjConnInfo(TEXT("Out"),SkelControlTitleColorDisabled) );
	ObjInfo.ObjObject = this;

	const FString ControlTitle = FString::Printf( TEXT("%s : %s"), *GetClass()->GetDescription(), *ControlName.ToString());
	const UBOOL bSelected = SelectedNodes.ContainsItem( this );
	const FColor& BorderColor = bSelected ? AnimTree_SelectedColor : FColor(0, 0, 0);
	const FIntPoint ControlPos(NodePosX, NodePosY);

	if ( Canvas->IsHitTesting() )
	{
		Canvas->SetHitProxy( new HLinkedObjProxy(this) );
	}
	const FColor& BackGroundColor = ( GetClass()->ClassFlags & CLASS_Deprecated ) ? AnimTree_DeprecatedBGColor : SkelControlTitleColorDisabled;
	const FColor FontColor = FColor( 255, 255, 128 );

	FLinkedObjDrawUtils::DrawLinkedObj( Canvas, ObjInfo, *ControlTitle, NULL, FontColor, BorderColor, BackGroundColor, ControlPos );
	
	if ( Canvas->IsHitTesting() )
	{
		Canvas->SetHitProxy( NULL );
	}

	DrawWidth = ObjInfo.DrawWidth;

	const FString StrengthString = FString::Printf( TEXT("%3.2f"), ControlStrength );

	const FColor& SliderBackGroundColor = (GetClass()->ClassFlags & CLASS_Deprecated) ? AnimTree_DeprecatedBGColor : FColor(140,140,140);

	FLinkedObjDrawUtils::DrawSlider(Canvas, FIntPoint(NodePosX, NodePosY + ObjInfo.DrawHeight), DrawWidth - 15, BorderColor, SliderBackGroundColor, ControlStrength, StrengthString, this );

	// Draw little button to toggle auto-blend.
	if ( FLinkedObjDrawUtils::AABBLiesWithinViewport( Canvas, NodePosX + DrawWidth - 15 + 1, NodePosY + ObjInfo.DrawHeight - 1, 14, LO_SLIDER_HANDLE_HEIGHT + 4 ) )
	{
		if( Canvas->IsHitTesting() )
		{
			Canvas->SetHitProxy( new HLinkedObjProxySpecial(this, 1) );
		}
		DrawTile( Canvas, NodePosX + DrawWidth - 15 + 1, NodePosY + ObjInfo.DrawHeight - 1, 14, LO_SLIDER_HANDLE_HEIGHT + 4, 0.f, 0.f, 1.f, 1.f, BorderColor );
		DrawTile( Canvas, NodePosX + DrawWidth - 15 + 2, NodePosY + ObjInfo.DrawHeight + 0, 14 - 2, LO_SLIDER_HANDLE_HEIGHT + 4 - 2, 0.f, 0.f, 1.f, 1.f, FColor(255,128,0) );
		if ( Canvas->IsHitTesting() )
		{
			Canvas->SetHitProxy( NULL );
		}
	}

	// Now draw to next node, if there is one.
	if( NextControl )
	{
		const FIntPoint Start	= GetConnectionLocation(LOC_OUTPUT);
		const FIntPoint End		= NextControl->GetConnectionLocation(LOC_INPUT);
		const FColor& LineColor	= ( bSelected || SelectedNodes.ContainsItem( NextControl ) ) ? AnimTree_SelectedColor : SkelControlTitleColorDisabled;
		
		// Curves
		{
			const FLOAT Tension		= Abs<INT>(Start.X - End.X);
			FLinkedObjDrawUtils::DrawSpline( Canvas, Start, Tension * FVector2D(1,0), End, Tension * FVector2D(1,0), LineColor, TRUE );
		}
	}
#endif // WITH_EDITORONLY_DATA
}
