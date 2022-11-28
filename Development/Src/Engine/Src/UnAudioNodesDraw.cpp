/*=============================================================================
	UnAudioNodesDraw.cpp: Unreal audio node drawing support.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "EnginePrivate.h" 
#include "EngineSoundClasses.h"
#include "UnLinkedObjDrawUtils.h"

#define SCE_CAPTION_HEIGHT		22
#define SCE_CAPTION_XPAD		4
#define SCE_CONNECTOR_WIDTH		8
#define SCE_CONNECTOR_LENGTH	10
#define SCE_CONNECTOR_SPACING	10

#define SCE_TEXT_BORDER			3
#define SCE_MIN_NODE_DIM		64

// Though this function could just traverse tree and draw all nodes that way, passing in the array means we can use it in the SoundCueEditor where not 
// all nodes are actually connected to the tree.
void USoundCue::DrawCue(FRenderInterface* RI, TArray<USoundNode*>& SelectedNodes)
{
	// First draw speaker 
	UTexture2D* SpeakerTexture = LoadObject<UTexture2D>(NULL, TEXT("EngineMaterials.SoundCue_SpeakerIcon"), NULL, LOAD_NoFail, NULL);
	if(!SpeakerTexture)
		return;

	RI->DrawTile(-128 - SCE_CONNECTOR_LENGTH, -64, 128, 128, 0.f, 0.f, 1.f, 1.f, FLinearColor::White, SpeakerTexture);
	
	UBOOL bIsHitTesting = RI->IsHitTesting();

	// Draw special 'root' input connector.
	if(bIsHitTesting) RI->SetHitProxy( new HLinkedObjConnectorProxy(this, LOC_INPUT, 0) );
	RI->DrawTile( -SCE_CONNECTOR_LENGTH, -0.5*SCE_CONNECTOR_WIDTH, SCE_CONNECTOR_LENGTH, SCE_CONNECTOR_WIDTH, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black );
	if(bIsHitTesting) RI->SetHitProxy( NULL);

	// Draw link to first node
	if(FirstNode)
	{
		FSoundNodeEditorData* EndEdData = EditorData.Find( FirstNode );
		if(EndEdData)
		{
			FIntPoint EndPos = FirstNode->GetConnectionLocation(RI, LOC_OUTPUT, 0, *EndEdData);
			RI->DrawLine2D( 0, 0, EndPos.X, EndPos.Y, FColor(0,0,0) );
		}
	}

	// Draw each SoundNode (will draw links as well)
	for( TMap<USoundNode*, FSoundNodeEditorData>::TIterator It(EditorData);It;++It )
	{
		USoundNode* Node = It.Key();
		UBOOL bNodeIsSelected = SelectedNodes.ContainsItem( Node );		

		FSoundNodeEditorData EdData = It.Value();

		Node->DrawSoundNode(RI, EdData, bNodeIsSelected);

		// Draw lines to children
		for(INT i=0; i<Node->ChildNodes.Num(); i++)
		{
			if( Node->ChildNodes(i) )
			{
				FIntPoint StartPos = Node->GetConnectionLocation(RI, LOC_INPUT, i, EdData);

				FSoundNodeEditorData* EndEdData = EditorData.Find( Node->ChildNodes(i) );
				if(EndEdData)
				{
					FIntPoint EndPos = Node->ChildNodes(i)->GetConnectionLocation(RI, LOC_OUTPUT, 0, *EndEdData);
					RI->DrawLine2D( StartPos.X, StartPos.Y, EndPos.X, EndPos.Y, FColor(0,0,0) );
				}
			}
		}
	}
}

void USoundNode::DrawSoundNode(FRenderInterface* RI, const FSoundNodeEditorData& EdData, UBOOL bSelected)
{
	FString Description;
	if( IsA(USoundNodeWave::StaticClass()) )
		Description = *GetFName();
	else
		Description = GetClass()->GetDescription();

	UBOOL bIsHitTesting = RI->IsHitTesting();

	if(bIsHitTesting) RI->SetHitProxy( new HLinkedObjProxy(this) );
	
	INT NodePosX = EdData.NodePosX;
	INT NodePosY = EdData.NodePosY;
	INT Width = SCE_MIN_NODE_DIM;
	INT Height = SCE_MIN_NODE_DIM;

	INT XL, YL;
	RI->StringSize( GEngine->SmallFont, XL, YL, *Description );
	Width = Max( Width, XL + 2*SCE_TEXT_BORDER + 2*SCE_CAPTION_XPAD );

	INT NumConnectors = Max( 1, ChildNodes.Num() );

	Height = Max( Height, SCE_CAPTION_HEIGHT + NumConnectors*SCE_CONNECTOR_WIDTH + (NumConnectors+1)*SCE_CONNECTOR_SPACING );

	FColor BorderColor = bSelected ? FColor(255,255,0) : FColor(0,0,0);	

	// Draw basic box
	RI->DrawTile(NodePosX,		NodePosY,						Width,		Height,					0.f, 0.f, 0.f, 0.f, BorderColor );
	RI->DrawTile(NodePosX+1,	NodePosY+1,						Width-2,	Height-2,				0.f, 0.f, 0.f, 0.f, FColor(112,112,112) );
	RI->DrawTile(NodePosX+1,	NodePosY+SCE_CAPTION_HEIGHT,	Width-2,	1,						0.f, 0.f, 0.f, 0.f, FLinearColor::Black );

	// Draw title bar
	RI->DrawTile(NodePosX+1,	NodePosY+1,						Width-2,	SCE_CAPTION_HEIGHT-1,	0.f, 0.f, 0.f, 0.f, FColor(140,140,140) );
	RI->DrawShadowedString( NodePosX + 0.5*Width - 0.5*XL, NodePosY + 0.5*SCE_CAPTION_HEIGHT - 0.5*YL, *Description, GEngine->SmallFont, FColor(255,255,128) );

	if(bIsHitTesting) RI->SetHitProxy( NULL );

	// Draw Output connector

	INT ConnectorRangeY = Height - SCE_CAPTION_HEIGHT;
	INT CenterY = NodePosY + SCE_CAPTION_HEIGHT + 0.5*ConnectorRangeY;

	if(bIsHitTesting) RI->SetHitProxy( new HLinkedObjConnectorProxy(this, LOC_OUTPUT, 0) );
	RI->DrawTile( NodePosX - SCE_CONNECTOR_LENGTH, CenterY - 0.5*SCE_CONNECTOR_WIDTH, SCE_CONNECTOR_LENGTH, SCE_CONNECTOR_WIDTH, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black );
	if(bIsHitTesting) RI->SetHitProxy( NULL );

	// Draw input connectors
	if( ChildNodes.Num() > 0 )
	{

		INT SpacingY = ConnectorRangeY/ChildNodes.Num();
		INT StartY = CenterY -  0.5*(ChildNodes.Num()-1)*SpacingY;

		for(INT i=0; i<ChildNodes.Num(); i++)
		{
			INT LinkY = StartY + i * SpacingY;

			if(bIsHitTesting) RI->SetHitProxy( new HLinkedObjConnectorProxy(this, LOC_INPUT, i) );
			RI->DrawTile( NodePosX + Width, LinkY - 0.5*SCE_CONNECTOR_WIDTH, SCE_CONNECTOR_LENGTH, SCE_CONNECTOR_WIDTH, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black );
			if(bIsHitTesting) RI->SetHitProxy( NULL );
		}
	}
}

FIntPoint USoundNode::GetConnectionLocation(FRenderInterface* RI, INT ConnType, INT ConnIndex, const FSoundNodeEditorData& EdData)
{
	FString Description;
	if( IsA(USoundNodeWave::StaticClass()) )
		Description = *GetFName();
	else
		Description = GetClass()->GetDescription();

	INT NodePosX = EdData.NodePosX;
	INT NodePosY = EdData.NodePosY;

	INT NumConnectors = Max( 1, ChildNodes.Num() );
	INT Height = SCE_MIN_NODE_DIM;
	Height = Max( Height, SCE_CAPTION_HEIGHT + NumConnectors*SCE_CONNECTOR_WIDTH + (NumConnectors+1)*SCE_CONNECTOR_SPACING );

	INT ConnectorRangeY = Height - SCE_CAPTION_HEIGHT;

	if(ConnType == LOC_OUTPUT)
	{
		check(ConnIndex == 0);

		INT NodeOutputYOffset = SCE_CAPTION_HEIGHT + 0.5*ConnectorRangeY;

		return FIntPoint(NodePosX - SCE_CONNECTOR_LENGTH, NodePosY + NodeOutputYOffset);
	}
	else
	{
		check( ConnIndex >= 0 && ConnIndex < ChildNodes.Num() );

		INT XL, YL;
		RI->StringSize( GEngine->SmallFont, XL, YL, *Description );

		INT NodeDrawWidth = SCE_MIN_NODE_DIM;
		NodeDrawWidth = Max( NodeDrawWidth, XL + 2*SCE_TEXT_BORDER );

		INT SpacingY = ConnectorRangeY/ChildNodes.Num();
		INT CenterYOffset = SCE_CAPTION_HEIGHT + 0.5*ConnectorRangeY;
		INT StartYOffset = CenterYOffset - 0.5*(ChildNodes.Num()-1)*SpacingY;
		INT NodeYOffset = StartYOffset + ConnIndex * SpacingY;

		return FIntPoint(NodePosX + NodeDrawWidth + SCE_CONNECTOR_LENGTH, NodePosY + NodeYOffset);
	}
}