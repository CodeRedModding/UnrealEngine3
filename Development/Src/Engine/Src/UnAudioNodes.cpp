/*=============================================================================
	UnAudioNodes.cpp: Unreal audio node implementations.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Daniel Vogel
=============================================================================*/

#include "EnginePrivate.h" 
#include "EngineSoundClasses.h"

/*-----------------------------------------------------------------------------
	USoundCue implementation.
-----------------------------------------------------------------------------*/

FArchive& operator<<( FArchive& Ar, FSoundNodeEditorData& EditorData )
{
	Ar << EditorData.NodePosX << EditorData.NodePosY;

	return Ar;
}

void USoundCue::Serialize(FArchive& Ar)
{
	Super::Serialize( Ar );

	Ar << EditorData;
}

/*-----------------------------------------------------------------------------
	USoundNode implementation.
-----------------------------------------------------------------------------*/

void USoundNode::ParseNodes( UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	for( INT i=0; i<ChildNodes.Num() && i<GetMaxChildNodes(); i++ )
		if( ChildNodes(i) )
			ChildNodes(i)->ParseNodes( AudioComponent, WaveInstances );
}

void USoundNode::GetNodes( UAudioComponent* AudioComponent, TArray<USoundNode*>& SoundNodes )
{
	SoundNodes.AddItem( this );
	INT MaxChildNodes = GetMaxChildNodes();
	for( INT i=0; i<ChildNodes.Num() && (i<MaxChildNodes || MaxChildNodes==-1); i++ )
		if( ChildNodes(i) )
			ChildNodes(i)->GetNodes( AudioComponent, SoundNodes );
}

void USoundNode::GetAllNodes( TArray<USoundNode*>& SoundNodes )
{
	SoundNodes.AddItem( this );
	INT MaxChildNodes = GetMaxChildNodes();
	for( INT i=0; i<ChildNodes.Num() && (i<MaxChildNodes || MaxChildNodes==-1); i++ )
		if( ChildNodes(i) )
			ChildNodes(i)->GetAllNodes( SoundNodes );
}
	
void USoundNode::InsertChildNode( INT Index )
{
	check( Index >= 0 && Index <= ChildNodes.Num() );
	ChildNodes.InsertZeroed( Index );
}

void USoundNode::RemoveChildNode( INT Index )
{
	check( Index >= 0 && Index < ChildNodes.Num() );
	ChildNodes.Remove( Index );
}

IMPLEMENT_CLASS(USoundNode);


/*-----------------------------------------------------------------------------
	USoundNodeMixer implementation.
-----------------------------------------------------------------------------*/

void USoundNodeMixer::ParseNodes( UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	FAudioComponentSavedState SavedState;

	for( INT i=0; i<ChildNodes.Num(); i++ )
	{
		if( ChildNodes(i) )
		{
			SavedState.Set( AudioComponent );
			ChildNodes(i)->ParseNodes( AudioComponent, WaveInstances );
			SavedState.Restore( AudioComponent );
		}
	}
}

IMPLEMENT_CLASS(USoundNodeMixer);

/*-----------------------------------------------------------------------------
	USoundNodeWave implementation.
-----------------------------------------------------------------------------*/

void USoundNodeWave::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	Ar	<<	FileType
		<<	RawData;
}

void USoundNodeWave::ParseNodes( UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	AudioComponent->CurrentVolume	*= Volume;
	AudioComponent->CurrentPitch	*= Pitch;

	// Start, update, terminate SoundNodeWaves.
	if( AudioComponent->PlaybackTime < AudioComponent->CurrentDelay )
		AudioComponent->bFinished = 0;
	else
	{
		// See whether this SoundNodeWave already has a WaveInstance associated with it. Note that we also have to handle
		// the same SoundNodeWave being used multiple times inside the SoundCue which is why we're using TMultiMap and a 
		// node (or well, rather SoundNodeWave) index.
		TArray<FWaveInstance*> ComponentWaveInstances;
		AudioComponent->SoundNodeWaveMap.MultiFind( this, ComponentWaveInstances );
		
		FWaveInstance* WaveInstance = NULL;
		for( INT i=0; i<ComponentWaveInstances.Num(); i++ )
		{
			if( ComponentWaveInstances(i)->NodeIndex == AudioComponent->CurrentNodeIndex )
			{
				WaveInstance = ComponentWaveInstances(i);
				break;
			}
		}

		// Create a new WaveInstance if this SoundNodeWave doesn't already have one associated with it.
		if( WaveInstance == NULL )
		{
			WaveInstance			= new FWaveInstance( AudioComponent );
			WaveInstance->NodeIndex = AudioComponent->CurrentNodeIndex;
			AudioComponent->WaveInstances.AddItem( WaveInstance );
			AudioComponent->SoundNodeWaveMap.Add( this, WaveInstance );
		}

		// Check for finished paths.
		if( WaveInstance->IsFinished )
		{
			// This wave instance is finished.
		}
		else
		{
			// Propagate properties and add WaveIntance to outgoing array of FWaveInstances.
			WaveInstance->Volume				= AudioComponent->CurrentVolume;
			WaveInstance->Priority				= AudioComponent->CurrentVolume;
			WaveInstance->Pitch					= AudioComponent->CurrentPitch;
			WaveInstance->Location				= AudioComponent->CurrentLocation;
			WaveInstance->UseSpatialization		= AudioComponent->CurrentUseSpatialization;
			WaveInstance->WaveData				= this;
			WaveInstance->NotifyFinished		= AudioComponent->CurrentNotifyFinished;
			WaveInstance->PotentiallyLooping	= AudioComponent->CurrentNotifyFinished ? AudioComponent->CurrentNotifyFinished->IsPotentiallyLooping() : FALSE;

			WaveInstances.AddItem( WaveInstance );

			// We're still alive.
			AudioComponent->bFinished			= 0;
		}
	}

	// Increment node index used for SoundNodeWave association.
	AudioComponent->CurrentNodeIndex++;
}

void USoundNodeWave::DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz )
{
	UTexture2D* Icon = CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.UnrealEdIcon_Sound") ));
	InRI->DrawTile( InX, InY, Icon->SizeX*InZoom, Icon->SizeY*InZoom, 0.0f,	0.0f, 1.0f, 1.0f, FLinearColor::White, Icon );
}

FThumbnailDesc USoundNodeWave::GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz )
{
	UTexture2D*		Icon	= CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.UnrealEdIcon_Sound") ));
	FThumbnailDesc	ThumbnailDesc;
	ThumbnailDesc.Width		= InFixedSz ? InFixedSz : Icon->SizeX*InZoom;
	ThumbnailDesc.Height	= InFixedSz ? InFixedSz : Icon->SizeY*InZoom;
	return ThumbnailDesc;
}

INT USoundNodeWave::GetThumbnailLabels( TArray<FString>* InLabels )
{
	InLabels->Empty();
	new( *InLabels )FString( GetName() );
	new( *InLabels )FString( FString::Printf( TEXT("%f Seconds"), Duration ) );
	return InLabels->Num();
}


IMPLEMENT_CLASS(USoundNodeWave);

/*-----------------------------------------------------------------------------
	USoundNodeAttenuation implementation.
-----------------------------------------------------------------------------*/

void USoundNodeAttenuation::ParseNodes( UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	if( AudioComponent->bAllowSpatialization )
	{
		if( bAttenuate )
		{
			RETRIEVE_SOUNDNODE_PAYLOAD( sizeof(FLOAT) + sizeof(FLOAT) );
			DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedMinRadius );
			DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedMaxRadius );

			if( *RequiresInitialization )
			{
				UsedMinRadius = MinRadius->GetValue();
				UsedMaxRadius = MaxRadius->GetValue();
				*RequiresInitialization = 0;
			}

			FLOAT Distance = FDist( AudioComponent->CurrentLocation, AudioComponent->Listener->Location );

			if( Distance >= UsedMaxRadius )
				AudioComponent->CurrentVolume = 0.f;
			else
			if( Distance > UsedMinRadius )
				AudioComponent->CurrentVolume *= 1.f - (Distance - UsedMinRadius) / (UsedMaxRadius - UsedMinRadius);
		}

		AudioComponent->CurrentUseSpatialization |= bSpatialize;
	}
	else
	{
		AudioComponent->CurrentUseSpatialization = 0;
	}

	USoundNode::ParseNodes( AudioComponent, WaveInstances );
}

IMPLEMENT_CLASS(USoundNodeAttenuation);

/*-----------------------------------------------------------------------------
	USoundNodeLooping implementation.
-----------------------------------------------------------------------------*/
		
void USoundNodeLooping::ParseNodes( UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof(INT) + sizeof(INT) );
	DECLARE_SOUNDNODE_ELEMENT( INT, UsedLoopCount );
	DECLARE_SOUNDNODE_ELEMENT( INT, FinishedCount );
		
	if( *RequiresInitialization )
	{
		UsedLoopCount = (INT) LoopCount->GetValue();
		FinishedCount = 0;

		*RequiresInitialization = 0;
	}

	if( UsedLoopCount > 0 )
		AudioComponent->CurrentNotifyFinished = this;
	
	USoundNode::ParseNodes( AudioComponent, WaveInstances );
}

UBOOL USoundNodeLooping::Finished( UAudioComponent* AudioComponent )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof(INT) + sizeof(INT) );
	DECLARE_SOUNDNODE_ELEMENT( INT, UsedLoopCount );
	DECLARE_SOUNDNODE_ELEMENT( INT, FinishedCount );
	check(*RequiresInitialization == 0 );
	
	if( UsedLoopCount >= 0 )
	{
		// Figure out how many child wave nodes are in subtree - this could be precomputed.
		INT NumChildNodes = 0;
		TArray<USoundNode*> SoundNodes;
		GetNodes( AudioComponent, SoundNodes );
		for( INT i=1; i<SoundNodes.Num(); i++ )
			if( Cast<USoundNodeWave>(SoundNodes(i)) )
				NumChildNodes++;

		// Wait till all leaves are finished.
		if( ++FinishedCount == NumChildNodes )
			FinishedCount = 0;
		else
			return 1;

		// Only decrease loopcount if all leaves are finished.
		UsedLoopCount--;

		// Reset all child nodes (GetNodes includes current node so we have to start at Index 1).
		for( INT i=1; i<SoundNodes.Num(); i++ )
		{
			UINT* Offset = AudioComponent->SoundNodeOffsetMap.Find( SoundNodes(i) );
			if( Offset )
			{
				UBOOL* RequiresInitialization = (UBOOL*) &AudioComponent->SoundNodeData( *Offset );
				*RequiresInitialization = 1;
			}

			// Reset all wave instances in subtree.
			TArray<FWaveInstance*> ComponentWaveInstances;
			AudioComponent->SoundNodeResetWaveMap.MultiFind( this, ComponentWaveInstances );

			for( INT i=0; i<ComponentWaveInstances.Num(); i++ )
				ComponentWaveInstances(i)->IsFinished = 0;

			AudioComponent->SoundNodeResetWaveMap.Remove( this );
		}

		return 0;
	}
	else
		return 1;
}

IMPLEMENT_CLASS(USoundNodeLooping);

/*-----------------------------------------------------------------------------
	USoundNodeRandom implementation.
-----------------------------------------------------------------------------*/

void USoundNodeRandom::ParseNodes( UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof(INT) );
	DECLARE_SOUNDNODE_ELEMENT( INT, NodeIndex );
		
	// Pick a random child node and save the index.
	if( *RequiresInitialization )
	{
		NodeIndex		= 0;	
		FLOAT WeightSum = 0;

		for( INT i=0; i<Weights.Num(); i++ )
			WeightSum += Weights(i);

		FLOAT Weight = appFrand() * WeightSum;
		for( INT i=0; i<ChildNodes.Num() && i<Weights.Num(); i++ )
		{
			if( Weights(i) >= Weight )
			{
				NodeIndex = i;
				break;
			}
			else
				Weight -= Weights(i);
		}

		*RequiresInitialization = 0;
	}

	if( NodeIndex < ChildNodes.Num() && ChildNodes(NodeIndex) )
		ChildNodes(NodeIndex)->ParseNodes( AudioComponent, WaveInstances );	
}

void USoundNodeRandom::GetNodes( UAudioComponent* AudioComponent, TArray<USoundNode*>& SoundNodes )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof(INT) );
	DECLARE_SOUNDNODE_ELEMENT( INT, NodeIndex );

	if( !*RequiresInitialization )
	{
		SoundNodes.AddItem( this );
		if( NodeIndex < ChildNodes.Num() && ChildNodes(NodeIndex) )
			ChildNodes(NodeIndex)->GetNodes( AudioComponent, SoundNodes );	
	}
}
	
void USoundNodeRandom::InsertChildNode( INT Index )
{
	check( Index >= 0 && Index <= Weights.Num() );
	check( ChildNodes.Num() == Weights.Num() );
	Weights.InsertZeroed( Index );

	Super::InsertChildNode( Index );
}

void USoundNodeRandom::RemoveChildNode( INT Index )
{
	check( Index >= 0 && Index < Weights.Num() );
	check( ChildNodes.Num() == Weights.Num() );
	Weights.Remove( Index );

	Super::RemoveChildNode( Index );
}

IMPLEMENT_CLASS(USoundNodeRandom);


/*-----------------------------------------------------------------------------
	USoundNodeOscillator implementation.
-----------------------------------------------------------------------------*/

void USoundNodeOscillator::ParseNodes( UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof(FLOAT) + sizeof(FLOAT) + sizeof(FLOAT) + sizeof(FLOAT) );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedAmplitude );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedFrequency );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedOffset );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedCenter );

	if( *RequiresInitialization )
	{
		UsedAmplitude	= Amplitude->GetValue();
		UsedFrequency	= Frequency->GetValue();
		UsedOffset		= Offset->GetValue();
		UsedCenter		= Center->GetValue();

		*RequiresInitialization = 0;
	}

	FLOAT ModulationFactor = UsedCenter + UsedAmplitude * appSin( UsedOffset + UsedFrequency * AudioComponent->PlaybackTime * PI );
	if( bModulateVolume )
		AudioComponent->CurrentVolume *= ModulationFactor;
	if( bModulatePitch )
		AudioComponent->CurrentPitch *= ModulationFactor;

	USoundNode::ParseNodes( AudioComponent, WaveInstances );
}

IMPLEMENT_CLASS(USoundNodeOscillator);


/*-----------------------------------------------------------------------------
	USoundNodeModulator implementation.
-----------------------------------------------------------------------------*/

void USoundNodeModulator::ParseNodes( UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances )
{
	RETRIEVE_SOUNDNODE_PAYLOAD( sizeof(FLOAT) + sizeof(FLOAT) );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedVolumeModulation );
	DECLARE_SOUNDNODE_ELEMENT( FLOAT, UsedPitchModulation );

	if( *RequiresInitialization )
	{
		UsedVolumeModulation		= VolumeModulation->GetValue();
		UsedPitchModulation			= PitchModulation->GetValue();
		*RequiresInitialization		= 0;
	}

	
	AudioComponent->CurrentVolume	*= UsedVolumeModulation;
	AudioComponent->CurrentPitch	*= UsedPitchModulation;

	USoundNode::ParseNodes( AudioComponent, WaveInstances );
}

IMPLEMENT_CLASS(USoundNodeModulator);

/*-----------------------------------------------------------------------------
	USoundCue implementation.
-----------------------------------------------------------------------------*/

void USoundCue::DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz )
{
	UTexture2D* Icon = CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.UnrealEdIcon_SoundCue") ));
	InRI->DrawTile( InX, InY, Icon->SizeX*InZoom, Icon->SizeY*InZoom, 0.0f,	0.0f, 1.0f, 1.0f, FLinearColor::White, Icon );
}

FThumbnailDesc USoundCue::GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz )
{
	UTexture2D*		Icon	= CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.UnrealEdIcon_SoundCue") ));
	FThumbnailDesc	ThumbnailDesc;
	ThumbnailDesc.Width		= InFixedSz ? InFixedSz : Icon->SizeX*InZoom;
	ThumbnailDesc.Height	= InFixedSz ? InFixedSz : Icon->SizeY*InZoom;
	return ThumbnailDesc;
}

INT USoundCue::GetThumbnailLabels( TArray<FString>* InLabels )
{
	InLabels->Empty();
	new( *InLabels )FString( TEXT("SoundCue") );
	new( *InLabels )FString( GetName() );
	return InLabels->Num();
}

IMPLEMENT_CLASS(USoundCue);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

