/*=============================================================================
	UnAnimTreeDraw.cpp: Function for drawing different AnimNode classes for AnimTreeEditor
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "UnLinkedObjDrawUtils.h"

/*-----------------------------------------------------------------------------
  UAnimNode
-----------------------------------------------------------------------------*/

FIntPoint UAnimNode::GetConnectionLocation(INT ConnType, INT ConnIndex)
{
	if(ConnType == LOC_INPUT)
	{
		check(ConnIndex == 0);
		return FIntPoint( NodePosX - LO_CONNECTOR_LENGTH, OutDrawY );
	}

	return FIntPoint(0,0);
}

/*-----------------------------------------------------------------------------
  UAnimNodeBlendBase
-----------------------------------------------------------------------------*/

void UAnimNodeBlendBase::DrawAnimNode( FRenderInterface* RI, UBOOL bSelected )
{
	// Construct the FLinkedObjDrawInfo for use the linked-obj drawing utils.
	FLinkedObjDrawInfo ObjInfo;

	// AnimTree's don't have an output!
	if( !this->IsA(UAnimTree::StaticClass()) )
	{
		ObjInfo.Inputs.AddItem( FLinkedObjConnInfo(TEXT("Out"),FColor(0,0,0)) );
	}

	for(INT i=0; i<Children.Num(); i++)
	{
		ObjInfo.Outputs.AddItem( FLinkedObjConnInfo(*Children(i).Name,FColor(0,0,0)) );
	}

	ObjInfo.ObjObject = this;

	UBOOL bIsTree = this->IsA(UAnimTree::StaticClass());

	// Generate border color
	FColor BorderColor = FColor(0, 0, 0);
	if( bIsTree )
	{
		BorderColor = FColor(0, 0, 200);
	}

	if(bSelected)
	{
		BorderColor = FColor(255, 255, 0);
	}

	// Generate name for node. User-give name if entered - otherwise just class name.
	const TCHAR* NodeTitle;
	FString NodeDesc = GetClass()->GetDescription();
	if( NodeName != NAME_None && !bIsTree )
	{
		NodeTitle = *NodeName;
	}
	else
	{
		NodeTitle = *NodeDesc;
	}

	// Use util to draw box with connectors etc.
	FLinkedObjDrawUtils::DrawLinkedObj( RI, ObjInfo, NodeTitle, NULL, BorderColor, FColor(112,112,112), FIntPoint(NodePosX, NodePosY) );

	// Read back draw locations of connectors, so we can draw lines in the correct places.
	if( !bIsTree )
	{
		OutDrawY = ObjInfo.InputY(0);
	}

	for(INT i=0; i<Children.Num(); i++)
	{
		Children(i).DrawY = ObjInfo.OutputY(i);
	}

	DrawWidth = ObjInfo.DrawWidth;


	// Now draw links to children

	for(INT i=0; i<Children.Num(); i++)
	{
		UAnimNode* ChildNode = Children(i).Anim;
		if(ChildNode)
		{
			FIntPoint StartPos = GetConnectionLocation(LOC_OUTPUT, i);
			FIntPoint EndPos = ChildNode->GetConnectionLocation(LOC_INPUT, 0);

			RI->DrawLine2D( StartPos.X, StartPos.Y, EndPos.X, EndPos.Y, FColor(0,0,0) );
		}
	}
}

FIntPoint UAnimNodeBlendBase::GetConnectionLocation(INT ConnType, INT ConnIndex)
{
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

	return FIntPoint(0,0);
}

/*-----------------------------------------------------------------------------
  UAnimNodeSequence
-----------------------------------------------------------------------------*/

void UAnimNodeSequence::DrawAnimNode( FRenderInterface* RI, UBOOL bSelected )
{
	FLinkedObjDrawInfo ObjInfo;

	ObjInfo.Inputs.AddItem( FLinkedObjConnInfo(TEXT("Out"),FColor(0,0,0)) );

	ObjInfo.ObjObject = this;

	FColor BorderColor = bSelected ? FColor(255, 255, 0) : FColor(200, 0, 0);

	FIntPoint Point(NodePosX, NodePosY);
	FLinkedObjDrawUtils::DrawLinkedObj( RI, ObjInfo, *AnimSeqName, NULL, BorderColor, FColor(112,112,112), Point );

	OutDrawY = ObjInfo.InputY(0);
}