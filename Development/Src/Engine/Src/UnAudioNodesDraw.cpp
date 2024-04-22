/*=============================================================================
	UnAudioNodesDraw.cpp: Unreal audio node drawing support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h" 
#include "EngineAudioDeviceClasses.h"
#include "EngineSoundClasses.h"
#include "UnLinkedObjDrawUtils.h"

#define SCE_CAPTION_HEIGHT		22
#define SCE_CAPTION_XPAD		4
#define SCE_CONNECTOR_WIDTH		8
#define SCE_CONNECTOR_LENGTH	10
#define SCE_CONNECTOR_SPACING	10

#define SCE_TEXT_BORDER			3
#define SCE_MIN_NODE_DIM		64

static FIntPoint GetConnectorLocation( FString& Description, INT ConnType, INT ConnIndex, INT NodePosX, INT NodePosY, INT NumConnectors )
{
#if WITH_EDITORONLY_DATA
	INT Height = SCE_MIN_NODE_DIM;
	Height = Max( Height, SCE_CAPTION_HEIGHT + NumConnectors * SCE_CONNECTOR_WIDTH + ( NumConnectors + 1 ) * SCE_CONNECTOR_SPACING );

	INT ConnectorRangeY = Height - SCE_CAPTION_HEIGHT;

	if( ConnType == LOC_OUTPUT )
	{
		check( ConnIndex == 0 );

		INT NodeOutputYOffset = SCE_CAPTION_HEIGHT + ConnectorRangeY / 2;

		return( FIntPoint( NodePosX - SCE_CONNECTOR_LENGTH, NodePosY + NodeOutputYOffset ) );
	}
	else
	{
		check( ConnIndex >= 0 && ConnIndex < NumConnectors );

		INT XL, YL;
		StringSize( GEngine->SmallFont, XL, YL, *Description );

		INT NodeDrawWidth = SCE_MIN_NODE_DIM;
		NodeDrawWidth = Max( NodeDrawWidth, XL + 2 * SCE_TEXT_BORDER );

		INT SpacingY = ConnectorRangeY / NumConnectors;
		INT CenterYOffset = SCE_CAPTION_HEIGHT + ConnectorRangeY / 2;
		INT StartYOffset = CenterYOffset - ( NumConnectors - 1 ) * SpacingY / 2;
		INT NodeYOffset = StartYOffset + ConnIndex * SpacingY;

		return( FIntPoint( NodePosX + NodeDrawWidth + SCE_CONNECTOR_LENGTH, NodePosY + NodeYOffset ) );
	}
#else
	return FIntPoint( 0, 0 );
#endif // WITH_EDITORONLY_DATA
}

static void DrawBoxWithTitle( FCanvas* Canvas, FString& Description, INT NodePosX, INT NodePosY, INT Width, INT Height, INT XL, INT YL, FColor& BorderColor )
{
#if WITH_EDITORONLY_DATA
	// Draw basic box
	DrawTile( Canvas, NodePosX, NodePosY, Width, Height, 0.0f, 0.0f, 0.0f, 0.0f, BorderColor );
	DrawTile( Canvas, NodePosX + 1,	NodePosY + 1, Width - 2, Height - 2, 0.0f, 0.0f, 0.0f, 0.0f, FColor( 112, 112, 112 ) );
	DrawTile( Canvas, NodePosX + 1,	NodePosY + SCE_CAPTION_HEIGHT, Width - 2, 1, 0.0f, 0.0f, 0.0f, 0.0f, FLinearColor::Black );

	// Draw title bar
	DrawTile( Canvas, NodePosX + 1, NodePosY + 1, Width  -2, SCE_CAPTION_HEIGHT - 1, 0.0f, 0.0f, 0.0f, 0.0f, FColor( 140, 140, 140 ) );
	DrawShadowedString( Canvas, NodePosX + ( Width - XL ) / 2, NodePosY + ( SCE_CAPTION_HEIGHT - YL ) / 2, *Description, GEngine->SmallFont, FColor( 255, 255, 128 ) );
#endif // WITH_EDITORONLY_DATA
}

static void DrawInputConnectors( UObject* Object, FCanvas* Canvas, INT NumInputs, INT ConnectorRangeY, INT CenterY, INT PosX )
{
#if WITH_EDITORONLY_DATA
	if( NumInputs > 0 )
	{
		INT SpacingY = ConnectorRangeY / NumInputs;
		INT StartY = CenterY - ( NumInputs - 1 ) * SpacingY / 2;

		for( INT i = 0; i < NumInputs; i++ )
		{
			INT LinkY = StartY + i * SpacingY;

			if( Canvas->IsHitTesting() ) 
			{
				Canvas->SetHitProxy( new HLinkedObjConnectorProxy( Object, LOC_INPUT, i ) );
			}

			DrawTile( Canvas, PosX, LinkY - SCE_CONNECTOR_WIDTH / 2, SCE_CONNECTOR_LENGTH, SCE_CONNECTOR_WIDTH, 0.0f, 0.0f, 0.0f, 0.0f, FLinearColor::Black );

			if( Canvas->IsHitTesting() )
			{
				Canvas->SetHitProxy( NULL );
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

static void DrawOutputConnector( UObject* Object, FCanvas* Canvas, INT ConnectorRangeY, INT CenterY, INT PosX )
{
#if WITH_EDITORONLY_DATA
	if( Canvas->IsHitTesting() ) 
	{
		Canvas->SetHitProxy( new HLinkedObjConnectorProxy( Object, LOC_OUTPUT, 0 ) );
	}

	DrawTile( Canvas, PosX - SCE_CONNECTOR_LENGTH, CenterY - SCE_CONNECTOR_WIDTH / 2, SCE_CONNECTOR_LENGTH, SCE_CONNECTOR_WIDTH, 0.0f, 0.0f, 0.0f, 0.0f, FLinearColor::Black );

	if( Canvas->IsHitTesting() ) 
	{
		Canvas->SetHitProxy( NULL );
	}
#endif // WITH_EDITORONLY_DATA
}

// Though this function could just traverse tree and draw all nodes that way, passing in the array means we can use it in the SoundCueEditor where not 
// all nodes are actually connected to the tree.
void USoundCue::DrawCue( FCanvas* Canvas, TArray<USoundNode*>& SelectedNodes )
{
#if WITH_EDITORONLY_DATA
	// First draw speaker 
	UTexture2D* SpeakerTexture = LoadObject<UTexture2D>( NULL, TEXT( "EngineMaterials.SoundCue_SpeakerIcon" ), NULL, LOAD_None, NULL );
	if( !SpeakerTexture )
	{
		return;
	}

	DrawTile( Canvas, -128 - SCE_CONNECTOR_LENGTH, -64, 128, 128, 0.0f, 0.0f, 1.0f, 1.0f, FLinearColor::White, SpeakerTexture->Resource );

	// Draw special 'root' input connector.
	if( Canvas->IsHitTesting() )
	{
		Canvas->SetHitProxy( new HLinkedObjConnectorProxy( this, LOC_INPUT, 0 ) );
	}

	DrawTile( Canvas, -SCE_CONNECTOR_LENGTH, -SCE_CONNECTOR_WIDTH / 2, SCE_CONNECTOR_LENGTH, SCE_CONNECTOR_WIDTH, 0.0f, 0.0f, 0.0f, 0.0f, FLinearColor::Black );

	if( Canvas->IsHitTesting() ) 
	{
		Canvas->SetHitProxy( NULL );
	}

	// Draw link to first node
	if( FirstNode )
	{
		FSoundNodeEditorData* EndEdData = EditorData.Find( FirstNode );
		if( EndEdData )
		{
			FIntPoint EndPos = FirstNode->GetConnectionLocation( Canvas, LOC_OUTPUT, 0, *EndEdData );
			DrawLine2D( Canvas, FVector2D( 0, 0 ), FVector2D( EndPos.X, EndPos.Y ), FColor( 0, 0, 0 ) );
		}
	}

	// Draw each SoundNode (will draw links as well)
	for( TMap<USoundNode*, FSoundNodeEditorData>::TIterator It( EditorData ); It; ++It )
	{
		USoundNode* Node = It.Key();

		// the node was null so just continue to the next one
		//@todo audio:  should this ever be null?
		if( Node == NULL )
		{
			continue;
		}

		UBOOL bNodeIsSelected = SelectedNodes.ContainsItem( Node );		

		FSoundNodeEditorData EdData = It.Value();

		Node->DrawSoundNode( Canvas, EdData, bNodeIsSelected );

		// Draw lines to children
		for( INT i = 0; i < Node->ChildNodes.Num(); i++ )
		{
			if( Node->ChildNodes( i ) )
			{
				FIntPoint StartPos = Node->GetConnectionLocation( Canvas, LOC_INPUT, i, EdData );

				FSoundNodeEditorData* EndEdData = EditorData.Find( Node->ChildNodes( i ) );
				if( EndEdData )
				{
					FIntPoint EndPos = Node->ChildNodes( i )->GetConnectionLocation( Canvas, LOC_OUTPUT, 0, *EndEdData );
					DrawLine2D( Canvas, StartPos, EndPos, FColor( 0, 0, 0 ) );
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void USoundNode::DrawSoundNode( FCanvas* Canvas, const FSoundNodeEditorData& EdData, UBOOL bSelected )
{
#if WITH_EDITORONLY_DATA
	FString Description;
	if( IsA( USoundNodeWave::StaticClass() ) )
	{
		Description = GetFName().ToString();
	}
	else
	{
		Description = GetClass()->GetDescription();
	}

	if( Canvas->IsHitTesting() )
	{
		Canvas->SetHitProxy( new HLinkedObjProxy( this ) );
	}
	
	INT NodePosX = EdData.NodePosX;
	INT NodePosY = EdData.NodePosY;
	INT Width = SCE_MIN_NODE_DIM;
	INT Height = SCE_MIN_NODE_DIM;

	INT XL, YL;
	StringSize( GEngine->SmallFont, XL, YL, *Description );
	Width = Max( Width, XL + 2 * SCE_TEXT_BORDER + 2 * SCE_CAPTION_XPAD );

	INT NumConnectors = Max( 1, ChildNodes.Num() );

	Height = Max( Height, SCE_CAPTION_HEIGHT + NumConnectors * SCE_CONNECTOR_WIDTH + ( NumConnectors + 1 ) * SCE_CONNECTOR_SPACING );

	FColor BorderColor = bSelected ? FColor( 255, 255, 0 ) : FColor( 0, 0, 0 );	
	DrawBoxWithTitle( Canvas, Description, NodePosX, NodePosY, Width, Height, XL, YL, BorderColor );

	if( Canvas->IsHitTesting() )
	{
		Canvas->SetHitProxy( NULL );
	}

	// Draw Output connector
	INT ConnectorRangeY = Height - SCE_CAPTION_HEIGHT;
	INT CenterY = NodePosY + SCE_CAPTION_HEIGHT + ConnectorRangeY / 2;

	DrawOutputConnector( this, Canvas, ConnectorRangeY, CenterY, NodePosX );

	// Draw input connectors
	DrawInputConnectors( this, Canvas, ChildNodes.Num(), ConnectorRangeY, CenterY, NodePosX + Width );
#endif // WITH_EDITORONLY_DATA
}

FIntPoint USoundNode::GetConnectionLocation( FCanvas* Canvas, INT ConnType, INT ConnIndex, const FSoundNodeEditorData& EdData )
{
#if WITH_EDITORONLY_DATA
	FString Description;
	if( IsA( USoundNodeWave::StaticClass() ) )
	{
		Description = GetFName().ToString();
	}
	else
	{
		Description = GetClass()->GetDescription();
	}

	INT NodePosX = EdData.NodePosX;
	INT NodePosY = EdData.NodePosY;

	return( GetConnectorLocation( Description, ConnType, ConnIndex, NodePosX, NodePosY, Max( 1, ChildNodes.Num() ) ) );
#else
	return FIntPoint( 0, 0 );
#endif // WITH_EDITORONLY_DATA
}

/**
 * Sets up stub editor data for any sound class that doesn't have any
 */
void USoundClass::InitSoundClassEditor( UAudioDevice* AudioDevice )
{
#if WITH_EDITORONLY_DATA
	// Create editor data for any sound class that doesn't have any
	for( TMap<FName, USoundClass*>::TIterator It( AudioDevice->SoundClasses ); It; ++It )
	{
		USoundClass* SoundClass = It.Value();
		if( !EditorData.Find( SoundClass ) )
		{
			FSoundClassEditorData EdData;
			appMemset( &EdData, 0, sizeof( FSoundClassEditorData ) );
			EditorData.Set( SoundClass, EdData );
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/** 
 * Draws the sound class hierarchy in the editor
 */
void USoundClass::DrawSoundClasses( UAudioDevice* AudioDevice, FCanvas* Canvas, TArray<USoundClass*>& SelectedClasses )
{
#if WITH_EDITORONLY_DATA
	// Draw each child sound class (will draw links as well)
	for( TMap<USoundClass*, FSoundClassEditorData>::TIterator It( EditorData ); It; ++It )
	{
		USoundClass* SoundClass = It.Key();
		if( SoundClass == NULL )
		{
			continue;
		}

		UBOOL bClassIsSelected = SelectedClasses.ContainsItem( SoundClass );		
		FSoundClassEditorData EdData = It.Value();

		SoundClass->DrawSoundClass( Canvas, EdData, bClassIsSelected );

		// Draw lines to children
		for( INT i = 0; i < SoundClass->ChildClassNames.Num(); i++ )
		{
			FName ChildSoundClassName = SoundClass->ChildClassNames( i );
			USoundClass* ChildSoundClass = AudioDevice->SoundClasses.FindRef( ChildSoundClassName );
			if( ChildSoundClass )
			{
				FIntPoint StartPos = SoundClass->GetConnectionLocation( Canvas, LOC_INPUT, i, EdData );

				FSoundClassEditorData* EndEdData = EditorData.Find( ChildSoundClass );
				if( EndEdData )
				{
					FIntPoint EndPos = ChildSoundClass->GetConnectionLocation( Canvas, LOC_OUTPUT, 0, *EndEdData );
					DrawLine2D( Canvas, StartPos, EndPos, FColor( 0, 0, 0 ) );
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void USoundClass::DrawSoundClass( FCanvas* Canvas, const FSoundClassEditorData& EdData, UBOOL bSelected )
{
#if WITH_EDITORONLY_DATA
	FString Description = GetFName().ToString();

	if( Canvas->IsHitTesting() )
	{
		Canvas->SetHitProxy( new HLinkedObjProxy( this ) );
	}

	INT NodePosX = EdData.NodePosX;
	INT NodePosY = EdData.NodePosY;
	INT Width = SCE_MIN_NODE_DIM;
	INT Height = SCE_MIN_NODE_DIM;

	INT XL, YL;
	StringSize( GEngine->SmallFont, XL, YL, *Description );
	Width = Max( Width, XL + 2 * SCE_TEXT_BORDER + 2 * SCE_CAPTION_XPAD );

	INT NumConnectors = Max( 1, ChildClassNames.Num() );

	Height = Max( Height, SCE_CAPTION_HEIGHT + NumConnectors * SCE_CONNECTOR_WIDTH + ( NumConnectors + 1 ) * SCE_CONNECTOR_SPACING );

	FColor BorderColor = bSelected ? FColor( 255, 255, 0 ) : FColor( 0, 0, 0 );	
	DrawBoxWithTitle( Canvas, Description, NodePosX, NodePosY, Width, Height, XL, YL, BorderColor );

	if( Canvas->IsHitTesting() )
	{
		Canvas->SetHitProxy( NULL );
	}

	// Draw Output connector
	INT ConnectorRangeY = Height - SCE_CAPTION_HEIGHT;
	INT CenterY = NodePosY + SCE_CAPTION_HEIGHT + ConnectorRangeY / 2;

	if( GetFName() != NAME_Master )
	{
		DrawOutputConnector( this, Canvas, ConnectorRangeY, CenterY, NodePosX );
	}

	// Draw input connectors
	DrawInputConnectors( this, Canvas, ChildClassNames.Num(), ConnectorRangeY, CenterY, NodePosX + Width );
#endif // WITH_EDITORONLY_DATA
}

/** 
 * Calculates where to draw the connectors for this class in the sound class editor window
 */
FIntPoint USoundClass::GetConnectionLocation( FCanvas* Canvas, INT ConnType, INT ConnIndex, const FSoundClassEditorData& EdData )
{
#if WITH_EDITORONLY_DATA
	FString Description = GetFName().ToString();

	INT NodePosX = EdData.NodePosX;
	INT NodePosY = EdData.NodePosY;

	return( GetConnectorLocation( Description, ConnType, ConnIndex, NodePosX, NodePosY, Max( 1, ChildClassNames.Num() ) ) );
#else
	return FIntPoint( 0, 0 );
#endif // WITH_EDITORONLY_DATA
}

/** 
 * Returns TRUE if the child sound class exists in the tree 
 */
UBOOL USoundClass::RecurseCheckChild( UAudioDevice* AudioDevice, FName ChildSoundClassName )
{
#if WITH_EDITORONLY_DATA
	for( INT Index = 0; Index < ChildClassNames.Num(); Index++ )
	{
		if( ChildClassNames( Index ) == ChildSoundClassName )
		{
			return( TRUE );
		}

		USoundClass* ChildSoundClass = AudioDevice->GetSoundClass( ChildClassNames( Index ) );
		if( ChildSoundClass->RecurseCheckChild( AudioDevice, ChildSoundClassName ) )
		{
			return( TRUE );
		}
	}
#endif // WITH_EDITORONLY_DATA

	return( FALSE );
}

/** 
 * Validates a connection, returns TRUE if it is allowed
 */
UBOOL USoundClass::CheckValidConnection( UAudioDevice* AudioDevice, USoundClass* ParentSoundClass, USoundClass* ChildSoundClass )
{
#if WITH_EDITORONLY_DATA
	FName ParentSoundClassName = ParentSoundClass->GetFName();
	FName ChildSoundClassName = ChildSoundClass->GetFName();

	// If the new sound class is already a child elsewhere, delete the old reference
	UBOOL bDeleting = TRUE;
	while( bDeleting )
	{
		bDeleting = FALSE;

		for( TMap<FName, USoundClass*>::TIterator It( AudioDevice->SoundClasses ); It; ++It )
		{
			USoundClass* SoundClass = It.Value();
			if( SoundClass != ParentSoundClass )
			{
				for( INT Index = 0; Index < SoundClass->ChildClassNames.Num(); Index++ )
				{
					if( SoundClass->ChildClassNames( Index ) == ChildSoundClassName )
					{
						SoundClass->ChildClassNames.Remove( Index );
						bDeleting = TRUE;
						break;
					}
				}
			}
		}
	}

	// Check for recursion
	if( ChildSoundClass->RecurseCheckChild( AudioDevice, ParentSoundClassName ) )
	{
		return( FALSE );
	}
#endif // WITH_EDITORONLY_DATA

	return( TRUE );
}

