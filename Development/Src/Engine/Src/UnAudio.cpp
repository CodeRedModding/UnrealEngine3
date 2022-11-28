/*=============================================================================
	UnAudio.cpp: Unreal base audio.
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney
	* Wave modification code by Erik de Neve
	* Complete rewrite by Daniel Vogel
=============================================================================*/

#include "EnginePrivate.h" 
#include "EngineSoundClasses.h"

/*-----------------------------------------------------------------------------
	UAudioDevice implementation.
-----------------------------------------------------------------------------*/

// Helper function for "Sort" (higher priority sorts last).
IMPLEMENT_COMPARE_POINTER( FWaveInstance, UnAudio, { return (B->Priority - A->Priority >= 0) ? -1 : 1; } )

void UAudioDevice::StaticConstructor()
{
	new(GetClass(),TEXT("SoundVolume"			), RF_Public) UFloatProperty(CPP_PROPERTY(SoundVolume			), TEXT("Audio"), CPF_Config );
}

UBOOL UAudioDevice::Init()
{
	LastRealtime	= 1;
	CurrentTick		= 0;
	return 1;
}

void UAudioDevice::SetListener( const FVector& Location, const FVector& Up, const FVector& Right, const FVector& Front )
{
	Listener.Location	= Location;
	Listener.Up			= Up;
	Listener.Right		= Right;
	Listener.Front		= Front;
}

void UAudioDevice::Update( UBOOL Realtime )
{
	FCycleCounterSection CycleCounter(GAudioStats.UpdateTime);
	CurrentTick++;

	// Stop all sounds if transitioning out of realtime.
	if( !Realtime && LastRealtime )
	{
		for( INT i=0; i<Sources.Num(); i++ )
			Sources(i)->Stop();
	}
	LastRealtime = Realtime;

	// Stop finished sources.
	for( INT i=0; i<Sources.Num(); i++ )
	{
		if( Sources(i)->IsFinished() )
			Sources(i)->Stop();
	}

	//@todo audio: Ideally we'd use the mixer's time here though OpenAL currently has no concept of that.
	static DOUBLE LastTime;
	DOUBLE	CurrentTime		= appSeconds();
	FLOAT	DeltaTime		= CurrentTime - LastTime;
	LastTime				= CurrentTime;

	// Poll audio components for active wave instances (== paths in node tree that end in a USoundNodeWave)
	TArray<FWaveInstance*> WaveInstances;
	for( INT i=AudioComponents.Num()-1; i>=0; i-- )
	{
		UAudioComponent* AudioComponent = AudioComponents(i);
		// UpdateWaveInstances might cause AudioComponent to remove itself from AudioComponents TArray which is why why iterate in reverse order!
		AudioComponent->UpdateWaveInstances( WaveInstances, &Listener, (Realtime || AudioComponent->bNonRealtime) ? DeltaTime : 0.f );
	}

	// Sort by priority (lowest priority first).
	Sort<USE_COMPARE_POINTER(FWaveInstance,UnAudio)>( &WaveInstances(0), WaveInstances.Num() );

	INT FirstActiveIndex = Max( WaveInstances.Num() - MaxChannels, 0 );

	// Mark sources that have to be kept alive.
	for( INT i=FirstActiveIndex; i<WaveInstances.Num(); i++ )
	{
		FSoundSource* Source = WaveInstanceSourceMap.FindRef( WaveInstances(i) );
		if( Source && i >= FirstActiveIndex )
			Source->LastUpdate = CurrentTick;
	}
	
	// Stop inactive sources, sources that no longer have a WaveInstance associated or sources that need to be reset because Stop & Play were called in the same frame.
	for( INT i=0; i<Sources.Num(); i++ )
		if( Sources(i)->LastUpdate != CurrentTick || (Sources(i)->WaveInstance && Sources(i)->WaveInstance->RequestRestart) )
			Sources(i)->Stop();

	// Start sources as needed.
	for( INT i=FirstActiveIndex; i<WaveInstances.Num(); i++ )
	{
		// Editor uses bNonRealtime for sounds played in the browser.
		if( Realtime || WaveInstances(i)->AudioComponent->bNonRealtime )
		{
			FSoundSource* Source = WaveInstanceSourceMap.FindRef( WaveInstances(i) );
			if( !Source )
			{
				check(FreeSources.Num());
				Source = FreeSources.Pop();
				check(Source);

				// Try to initialize source.
				if( Source->Init( WaveInstances(i) ) )
				{
					// Associate wave instance with it which is used earlier in this function.
					WaveInstanceSourceMap.Set( WaveInstances(i), Source );
					// Playback might be deferred till the end of the update function on certain implementations.
					Source->Play();
				}
				else
				{
					// This can happen if e.g. the USoundNodeWave pointed to by the WaveInstance is not a valid sound file.
					FreeSources.AddItem( Source );
				}
			}
			else
			{
				Source->Update();
			}
		}
	}

	GAudioStats.WaveInstances.Value += WaveInstances.Num();
	GAudioStats.AudioComponents.Value += AudioComponents.Num();
}

void UAudioDevice::AddComponent( UAudioComponent* AudioComponent )
{
	check(AudioComponent);
	AudioComponents.AddUniqueItem( AudioComponent );
}

void UAudioDevice::RemoveComponent( UAudioComponent* AudioComponent )
{
	check(AudioComponent);
	for( INT i=0; i<AudioComponent->WaveInstances.Num(); i++ )
	{
		FSoundSource* Source = WaveInstanceSourceMap.FindRef( AudioComponent->WaveInstances(i) );
		if( Source )
			Source->Stop();
	}
	AudioComponents.RemoveItem( AudioComponent );
}

UAudioComponent* UAudioDevice::CreateComponent( USoundCue* SoundCue, FScene* Scene, AActor* Actor, UBOOL Play )
{
	if( SoundCue )
	{
		UAudioComponent* AudioComponent		= Actor ? ConstructObject<UAudioComponent>(UAudioComponent::StaticClass(),Actor) : ConstructObject<UAudioComponent>(UAudioComponent::StaticClass());
		
		AudioComponent->Scene				= Scene;
		AudioComponent->SoundCue			= SoundCue;
		AudioComponent->bUseOwnerLocation	= Actor ? 1 : 0;
		AudioComponent->bAutoPlay			= 0;
		AudioComponent->bAutoDestroy		= Play;
		AudioComponent->Owner				= Actor;

		AudioComponent->Created();

		if( Actor )
		{
			// Add this audio component to the actor's components array so it gets destroyed when the actor gets destroyed.
			Actor->Components.AddItem( AudioComponent );

			// AActor::UpdateComponents calls this as well though we need an initial location as we manually create the component.
			AudioComponent->SetParentToWorld( Actor->LocalToWorld() );
		}

		if( Play )
		{
			AudioComponent->Play();
			// An AudioComponent with bAutoDestroy set to true will "delete this" once finished so we have to make sure there are no reference to it which is why we return NULL in this case.
			return NULL;
		}
		else
			return AudioComponent;
	}
	else
		return NULL;
}

void UAudioDevice::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);

	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		for( INT i=0; i<AudioComponents.Num(); i++ )
			Ar << AudioComponents(i);
	}

	// For some reason VS.NET 2003 puts UAudioDevice::Serialize into UALAudioDevice's vftble instead of UALAudioDevice::Serialize
	// in release builds though does the right thing for debug builds. I suspect this to be a compiler bug so the need for this
	// hack schould be re-evaluated with VS.NET 2005.
#if _MSC_VER > 1500 && !defined XBOX
#error re-evaluate need for this COMPILER HACK
#endif
	SerializeHack( Ar );
}

void UAudioDevice::Flush()
{
	for( INT i=0; i<Sources.Num(); i++ )
		Sources(i)->Stop();

	WaveInstanceSourceMap.Empty();
	AudioComponents.Empty();
}


IMPLEMENT_CLASS(UAudioDevice);


/*-----------------------------------------------------------------------------
	FSoundSource implementation.
-----------------------------------------------------------------------------*/

void FSoundSource::Stop()
{
	if( WaveInstance )
	{
		check(AudioDevice);
		AudioDevice->FreeSources.AddUniqueItem(this);
		AudioDevice->WaveInstanceSourceMap.Remove( WaveInstance );
		WaveInstance->Finished();
		WaveInstance->RequestRestart = 0;
		WaveInstance = NULL;
	}
	else
	{
		check( AudioDevice->FreeSources.FindItemIndex( this ) != INDEX_NONE );
	}
}

/*-----------------------------------------------------------------------------
	FWaveInstance implementation.
-----------------------------------------------------------------------------*/

DWORD FWaveInstance::TypeHashCounter = 0;

FWaveInstance::FWaveInstance( UAudioComponent* InAudioComponent )
:	WaveData(NULL),
	NotifyFinished(NULL),
	AudioComponent(InAudioComponent),
	Volume(0),
	Pitch(0),
	Priority(0),
	Velocity(FVector(0,0,0)),
	Location(FVector(0,0,0)),
	IsFinished(0),
	UseSpatialization(0),
	RequestRestart(0),
	PotentiallyLooping(0)
{
	TypeHash = ++TypeHashCounter;
}

UBOOL FWaveInstance::Finished()
{
	IsFinished = 1;
	if( NotifyFinished )
	{
		// Check NotifyFinished hook to see whether we are really finished (e.g. looping)...
		IsFinished &= NotifyFinished->Finished( AudioComponent );

		// ... or whether we will need to be reset later.
		if( IsFinished )
		{
			AudioComponent->SoundNodeResetWaveMap.Add( NotifyFinished, this );
			// Avoid double notifications.
			NotifyFinished = NULL;
		}
	}
	return IsFinished;
}

FArchive& operator<<( FArchive& Ar, FWaveInstance* WaveInstance )
{
	if( !Ar.IsLoading() && !Ar.IsSaving() )
		Ar << WaveInstance->WaveData << WaveInstance->NotifyFinished << WaveInstance->AudioComponent;
	return Ar;
}

/*-----------------------------------------------------------------------------
	FAudioComponentSavedState implementation.
-----------------------------------------------------------------------------*/

void FAudioComponentSavedState::Set( UAudioComponent* AudioComponent )
{
	CurrentNotifyFinished						= AudioComponent->CurrentNotifyFinished;
	CurrentLocation								= AudioComponent->CurrentLocation;
	CurrentDelay								= AudioComponent->CurrentDelay;
	CurrentVolume								= AudioComponent->CurrentVolume;
	CurrentPitch								= AudioComponent->CurrentPitch;
	CurrentUseSpatialization					= AudioComponent->CurrentUseSpatialization;
}

void FAudioComponentSavedState::Restore( UAudioComponent* AudioComponent )
{
	AudioComponent->CurrentNotifyFinished		= CurrentNotifyFinished;
	AudioComponent->CurrentLocation				= CurrentLocation;
	AudioComponent->CurrentDelay				= CurrentDelay;
	AudioComponent->CurrentVolume				= CurrentVolume;
	AudioComponent->CurrentPitch				= CurrentPitch;
	AudioComponent->CurrentUseSpatialization	= CurrentUseSpatialization;
}

void FAudioComponentSavedState::Reset( UAudioComponent* AudioComponent )
{
	AudioComponent->CurrentNotifyFinished		= NULL;
	AudioComponent->CurrentDelay				= 0.f,
	AudioComponent->CurrentVolume				= 1.0,
	AudioComponent->CurrentPitch				= 1.f;
	AudioComponent->CurrentUseSpatialization	= 0;
	AudioComponent->CurrentLocation				= AudioComponent->bUseOwnerLocation ? AudioComponent->ComponentLocation : AudioComponent->Location;
}


/*-----------------------------------------------------------------------------
	UAudioComponent implementation.
-----------------------------------------------------------------------------*/

void UAudioComponent::Created()
{
	Super::Created();
	if( bAutoPlay )
		Play();
}

void UAudioComponent::Destroy()
{
	// Make sure audio component gets correctly cleaned up.
	if( Initialized )
		Destroyed();

	Super::Destroy();
}

void UAudioComponent::Destroyed()
{
	Super::Destroyed();

	// Removes component from the audio device's component list.
	UAudioDevice* AudioDevice = GEngine && GEngine->Client ? GEngine->Client->GetAudioDevice() : NULL;
	if( AudioDevice )
		AudioDevice->RemoveComponent( this );

	// Delete wave instances.
	for( TMultiMap<USoundNodeWave*,FWaveInstance*>::TIterator It(SoundNodeWaveMap); It; ++It )
		delete It.Value();
	SoundNodeWaveMap.Empty();
}

void UAudioComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	Super::SetParentToWorld( ParentToWorld );
	ComponentLocation = ParentToWorld.GetOrigin();
}

void UAudioComponent::Play()
{
	// Cache root node of sound container to avoid subsequent dereferencing.
	if(SoundCue)
		CueFirstNode = SoundCue->FirstNode;

	// Reset variables.
	if( bWasPlaying )
	{
		for( TMultiMap<USoundNodeWave*,FWaveInstance*>::TIterator It(SoundNodeWaveMap); It; ++It )
		{		
			if( It.Value() )
			{
				It.Value()->IsFinished		= 0;	
				It.Value()->RequestRestart	= 1;
			}
		}
	}
	PlaybackTime	= 0.f;
	bFinished		= 0;
	bWasPlaying		= 1;

	// Adds component from the audio device's component list.
	UAudioDevice* AudioDevice = GEngine && GEngine->Client ? GEngine->Client->GetAudioDevice() : NULL;
	if( AudioDevice )
		AudioDevice->AddComponent( this );
}

void UAudioComponent::Stop()
{
	// Removes component from the audio device's component list unless bAutoDestroy is set in which case this is done in Destroyed.
	if( !bAutoDestroy )
	{
		UAudioDevice* AudioDevice = GEngine && GEngine->Client ? GEngine->Client->GetAudioDevice() : NULL;
		if( AudioDevice )
			AudioDevice->RemoveComponent( this );
	}

	// For safety, clear out the cached root sound node pointer.
	CueFirstNode	= NULL;

	// We're done.
	bFinished		= 1;

	// Automatically clean up component.
	if( bAutoDestroy )
	{
		// Remove this audio component from the actor's components array as we're going to manually delete it below.
		if( Owner )
			Owner->Components.RemoveItem( this );

		// Destroy component and disassociate with audio device.
		Destroyed();

		// Do NOT acccess any member variables in UAudioComponent after this line.
		delete this;
	}
	else
	{
		// Force re-initialization on next play.
		SoundNodeData.Empty();
		SoundNodeOffsetMap.Empty();
		SoundNodeResetWaveMap.Empty();
		SoundNodeWaveMap.Empty();
	}
}

void UAudioComponent::PostEditChange(UProperty* PropertyThatChanged)
{
	Super::PostEditChange(PropertyThatChanged);

	// Reset variables.
	if( bWasPlaying )
	{
		for( TMultiMap<USoundNodeWave*,FWaveInstance*>::TIterator It(SoundNodeWaveMap); It; ++It )
		{		
			if( It.Value() )
			{
				It.Value()->IsFinished		= 0;	
				It.Value()->RequestRestart	= 1;
			}
		}
	}
	PlaybackTime	= 0.f;
	bFinished		= 0;

	// Clear node offset associations and data so dynamic data gets re-initialized.
	SoundNodeData.Empty();
	SoundNodeOffsetMap.Empty();
}

void UAudioComponent::UpdateWaveInstances( TArray<FWaveInstance*> &InWaveInstances, FListener* InListener, FLOAT DeltaTime )
{
	// Early outs.
	if( !CueFirstNode || !InListener )
		return;

	// I'll have to rethink this approach once we have multi- viewport support and I have an idea how to handle multiple listeners.
	Listener = InListener;

	//@todo audio: Need to handle pausing and not getting out of sync by using the mixer's time.
	PlaybackTime += DeltaTime;

	// Reset temporary variables used for node traversal.
	FAudioComponentSavedState::Reset( this );
	CurrentNodeIndex = 0;

	// Recurse nodes, have SoundNodeWave's create new wave instances and update bFinished.
	bFinished = 1;
	CueFirstNode->ParseNodes( this, InWaveInstances );

	// Stop playback, handles bAutoDestroy in Stop.
	if( bFinished )
		Stop();
}

void UAudioComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if( !Ar.IsLoading() && !Ar.IsSaving() )
		Ar << SoundCue << CueFirstNode << SoundNodeOffsetMap << SoundNodeWaveMap;
}

//
// Script interface
//

void UAudioComponent::execPlay( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	Play();	
}
IMPLEMENT_FUNCTION(UAudioComponent,INDEX_NONE,execPlay);

void UAudioComponent::execPause( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
}
IMPLEMENT_FUNCTION(UAudioComponent,INDEX_NONE,execPause);

void UAudioComponent::execStop( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	Stop();
}
IMPLEMENT_FUNCTION(UAudioComponent,INDEX_NONE,execStop);


/*-----------------------------------------------------------------------------
	WaveModInfo implementation - downsampling of wave files.
-----------------------------------------------------------------------------*/

//  Macros to convert 4 bytes to a Riff-style ID DWORD.
//  Todo: make these endian independent !!!

#undef MAKEFOURCC

#define MAKEFOURCC(ch0, ch1, ch2, ch3)\
    ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |\
    ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))

#define mmioFOURCC(ch0, ch1, ch2, ch3)\
    MAKEFOURCC(ch0, ch1, ch2, ch3)

// Main Riff-Wave header.
struct FRiffWaveHeader
{ 
	DWORD	rID;			// Contains 'RIFF'
	DWORD	ChunkLen;		// Remaining length of the entire riff chunk (= file).
	DWORD	wID;			// Form type. Contains 'WAVE' for .wav files.
};

// General chunk header format.
struct FRiffChunkOld
{
	DWORD	ChunkID;		  // General data chunk ID like 'data', or 'fmt ' 
	DWORD	ChunkLen;		  // Length of the rest of this chunk in bytes.
};

// ChunkID: 'fmt ' ("WaveFormatEx" structure ) 
struct FFormatChunk
{
    _WORD   wFormatTag;        // Format type: 1 = PCM
    _WORD   nChannels;         // Number of channels (i.e. mono, stereo...).
    DWORD   nSamplesPerSec;    // Sample rate. 44100 or 22050 or 11025  Hz.
    DWORD   nAvgBytesPerSec;   // For buffer estimation  = sample rate * BlockAlign.
    _WORD   nBlockAlign;       // Block size of data = Channels times BYTES per sample.
    _WORD   wBitsPerSample;    // Number of bits per sample of mono data.
    _WORD   cbSize;            // The count in bytes of the size of extra information (after cbSize).
};

// ChunkID: 'smpl'
struct FSampleChunk
{
	DWORD   dwManufacturer;
	DWORD   dwProduct;
	DWORD   dwSamplePeriod;
	DWORD   dwMIDIUnityNote;
	DWORD   dwMIDIPitchFraction;
	DWORD	dwSMPTEFormat;		
	DWORD   dwSMPTEOffset;		//
	DWORD   cSampleLoops;		// Number of tSampleLoop structures following this chunk
	DWORD   cbSamplerData;		// 
};
 
struct FSampleLoop				// Immediately following cbSamplerData in the SMPL chunk.
{
	DWORD	dwIdentifier;		//
	DWORD	dwType;				//
	DWORD	dwStart;			// Startpoint of the loop in samples
	DWORD	dwEnd;				// Endpoint of the loop in samples
	DWORD	dwFraction;			// Fractional sample adjustment
	DWORD	dwPlayCount;		// Play count
};

//
//	Figure out the WAVE file layout.
//
UBOOL FWaveModInfo::ReadWaveInfo( TArray<BYTE>& WavData )
{
	FFormatChunk*		FmtChunk;
	FRiffWaveHeader*	RiffHdr = (FRiffWaveHeader*)&WavData(0);
	WaveDataEnd					= &WavData(0) + WavData.Num();	

	// Verify we've got a real 'WAVE' header.
#if __INTEL_BYTE_ORDER__
	if ( RiffHdr->wID != mmioFOURCC('W','A','V','E') )
	{
		return 0;
	}
#else
	if ( (RiffHdr->wID != (mmioFOURCC('W','A','V','E'))) &&
	     (RiffHdr->wID != (mmioFOURCC('E','V','A','W'))) )
	{
		return 0;
	}

	UBOOL AlreadySwapped = (RiffHdr->wID == (mmioFOURCC('W','A','V','E')));
	if( !AlreadySwapped )
	{
		RiffHdr->rID		= INTEL_ORDER32(RiffHdr->rID);
		RiffHdr->ChunkLen	= INTEL_ORDER32(RiffHdr->ChunkLen);
		RiffHdr->wID		= INTEL_ORDER32(RiffHdr->wID);
	}
#endif

	FRiffChunkOld* RiffChunk	= (FRiffChunkOld*)&WavData(3*4);
	pMasterSize = &RiffHdr->ChunkLen;
	
	// Look for the 'fmt ' chunk.
	while( ( ((BYTE*)RiffChunk + 8) < WaveDataEnd)  && ( INTEL_ORDER32(RiffChunk->ChunkID) != mmioFOURCC('f','m','t',' ') ) )
		RiffChunk = (FRiffChunkOld*) ( (BYTE*)RiffChunk + Pad16Bit(INTEL_ORDER32(RiffChunk->ChunkLen)) + 8);

	if( INTEL_ORDER32(RiffChunk->ChunkID) != mmioFOURCC('f','m','t',' ') )
	{
		#if !__INTEL_BYTE_ORDER__  // swap them back just in case.
			if(! AlreadySwapped )
			{
				RiffHdr->rID = INTEL_ORDER32(RiffHdr->rID);
				RiffHdr->ChunkLen = INTEL_ORDER32(RiffHdr->ChunkLen);
				RiffHdr->wID = INTEL_ORDER32(RiffHdr->wID);
            }
		#endif
		return 0;
	}

	FmtChunk 		= (FFormatChunk*)((BYTE*)RiffChunk + 8);
#if !__INTEL_BYTE_ORDER__
	if( !AlreadySwapped )
	{
		FmtChunk->wFormatTag		= INTEL_ORDER16(FmtChunk->wFormatTag);
		FmtChunk->nChannels			= INTEL_ORDER16(FmtChunk->nChannels);
		FmtChunk->nSamplesPerSec	= INTEL_ORDER32(FmtChunk->nSamplesPerSec);
		FmtChunk->nAvgBytesPerSec	= INTEL_ORDER32(FmtChunk->nAvgBytesPerSec);
		FmtChunk->nBlockAlign		= INTEL_ORDER16(FmtChunk->nBlockAlign);
		FmtChunk->wBitsPerSample	= INTEL_ORDER16(FmtChunk->wBitsPerSample);
	}
#endif
	pBitsPerSample  = &FmtChunk->wBitsPerSample;
	pSamplesPerSec  = &FmtChunk->nSamplesPerSec;
	pAvgBytesPerSec = &FmtChunk->nAvgBytesPerSec;
	pBlockAlign		= &FmtChunk->nBlockAlign;
	pChannels       = &FmtChunk->nChannels;

	// re-initalize the RiffChunk pointer
	RiffChunk = (FRiffChunkOld*)&WavData(3*4);
	
	// Look for the 'data' chunk.
	while( ( ((BYTE*)RiffChunk + 8) < WaveDataEnd) && ( INTEL_ORDER32(RiffChunk->ChunkID) != mmioFOURCC('d','a','t','a') ) )
		RiffChunk = (FRiffChunkOld*) ( (BYTE*)RiffChunk + Pad16Bit(INTEL_ORDER32(RiffChunk->ChunkLen)) + 8);

	if( INTEL_ORDER32(RiffChunk->ChunkID) != mmioFOURCC('d','a','t','a') )
	{
		#if !__INTEL_BYTE_ORDER__  // swap them back just in case.
			if( !AlreadySwapped )
			{
				RiffHdr->rID				= INTEL_ORDER32(RiffHdr->rID);
				RiffHdr->ChunkLen			= INTEL_ORDER32(RiffHdr->ChunkLen);
				RiffHdr->wID				= INTEL_ORDER32(RiffHdr->wID);
				FmtChunk->wFormatTag		= INTEL_ORDER16(FmtChunk->wFormatTag);
				FmtChunk->nChannels			= INTEL_ORDER16(FmtChunk->nChannels);
				FmtChunk->nSamplesPerSec	= INTEL_ORDER32(FmtChunk->nSamplesPerSec);
				FmtChunk->nAvgBytesPerSec	= INTEL_ORDER32(FmtChunk->nAvgBytesPerSec);
				FmtChunk->nBlockAlign		= INTEL_ORDER16(FmtChunk->nBlockAlign);
				FmtChunk->wBitsPerSample	= INTEL_ORDER16(FmtChunk->wBitsPerSample);
			}
		#endif
		return 0;
	}

	#if !__INTEL_BYTE_ORDER__  // swap them back just in case.
		if( AlreadySwapped ) // swap back into Intel order for chunk search...
			RiffChunk->ChunkLen = INTEL_ORDER32(RiffChunk->ChunkLen);
	#endif

	SampleDataStart 	= (BYTE*)RiffChunk + 8;
	pWaveDataSize   	= &RiffChunk->ChunkLen;
	SampleDataSize  	=  INTEL_ORDER32(RiffChunk->ChunkLen);
	OldBitsPerSample 	= FmtChunk->wBitsPerSample;
	SampleDataEnd   	=  SampleDataStart+SampleDataSize;

	if ((BYTE *) SampleDataEnd > (BYTE *) WaveDataEnd)
	{
		debugf(NAME_Warning, TEXT("Wave data chunk is too big!"));

		// Fix it up by clamping data chunk.
		SampleDataEnd = (BYTE *) WaveDataEnd;
		SampleDataSize = SampleDataEnd - SampleDataStart;
		RiffChunk->ChunkLen = INTEL_ORDER32(SampleDataSize);
	}

	NewDataSize	= SampleDataSize;

	if( FmtChunk->wFormatTag != 1 )
		return 0;

#if !__INTEL_BYTE_ORDER__
	if( !AlreadySwapped )
	{
		if (FmtChunk->wBitsPerSample == 16)
		{
			for (_WORD *i = (_WORD *) SampleDataStart; i < (_WORD *) SampleDataEnd; i++)
			{
				*i = INTEL_ORDER16(*i);
			}
		}
		else if (FmtChunk->wBitsPerSample == 32)
		{
			for (DWORD *i = (DWORD *) SampleDataStart; i < (DWORD *) SampleDataEnd; i++)
			{
				*i = INTEL_ORDER32(*i);
			}
		}
	}
#endif

	// Re-initalize the RiffChunk pointer
	RiffChunk = (FRiffChunkOld*)&WavData(3*4);
	// Look for a 'smpl' chunk.
	while( ( (((BYTE*)RiffChunk) + 8) < WaveDataEnd) && ( INTEL_ORDER32(RiffChunk->ChunkID) != mmioFOURCC('s','m','p','l') ) )
		RiffChunk = (FRiffChunkOld*) ( (BYTE*)RiffChunk + Pad16Bit(INTEL_ORDER32(RiffChunk->ChunkLen)) + 8);
	
	// Chunk found ? smpl chunk is optional.
	// Find the first sample-loop structure, and the total number of them.
	if( (PTRINT)RiffChunk+4<(PTRINT)WaveDataEnd && INTEL_ORDER32(RiffChunk->ChunkID) == mmioFOURCC('s','m','p','l') )
	{
		FSampleChunk *pSampleChunk = (FSampleChunk *) ((BYTE*)RiffChunk + 8);
		pSampleLoop = (FSampleLoop*) ((BYTE*)RiffChunk + 8 + sizeof(FSampleChunk));

		if (((BYTE *)pSampleChunk + sizeof (FSampleChunk)) > (BYTE *)WaveDataEnd)
			pSampleChunk = NULL;

		if (((BYTE *)pSampleLoop + sizeof (FSampleLoop)) > (BYTE *)WaveDataEnd)
			pSampleLoop = NULL;

#if !__INTEL_BYTE_ORDER__
		if( !AlreadySwapped )
		{
			if( pSampleChunk != NULL )
			{
				pSampleChunk->dwManufacturer		= INTEL_ORDER32(pSampleChunk->dwManufacturer);
				pSampleChunk->dwProduct				= INTEL_ORDER32(pSampleChunk->dwProduct);
				pSampleChunk->dwSamplePeriod		= INTEL_ORDER32(pSampleChunk->dwSamplePeriod);
				pSampleChunk->dwMIDIUnityNote		= INTEL_ORDER32(pSampleChunk->dwMIDIUnityNote);
				pSampleChunk->dwMIDIPitchFraction	= INTEL_ORDER32(pSampleChunk->dwMIDIPitchFraction);
				pSampleChunk->dwSMPTEFormat			= INTEL_ORDER32(pSampleChunk->dwSMPTEFormat);
				pSampleChunk->dwSMPTEOffset			= INTEL_ORDER32(pSampleChunk->dwSMPTEOffset);
				pSampleChunk->cSampleLoops			= INTEL_ORDER32(pSampleChunk->cSampleLoops);
				pSampleChunk->cbSamplerData			= INTEL_ORDER32(pSampleChunk->cbSamplerData);
			}

			if (pSampleLoop != NULL)
			{
				pSampleLoop->dwIdentifier			= INTEL_ORDER32(pSampleLoop->dwIdentifier);
				pSampleLoop->dwType					= INTEL_ORDER32(pSampleLoop->dwType);
				pSampleLoop->dwStart				= INTEL_ORDER32(pSampleLoop->dwStart);
				pSampleLoop->dwEnd					= INTEL_ORDER32(pSampleLoop->dwEnd);
				pSampleLoop->dwFraction				= INTEL_ORDER32(pSampleLoop->dwFraction);
				pSampleLoop->dwPlayCount			= INTEL_ORDER32(pSampleLoop->dwPlayCount);
            }
		}
#endif
		SampleLoopsNum = pSampleChunk->cSampleLoops;
	}

	// Couldn't byte swap this before, since it'd throw off the chunk search.
#if !__INTEL_BYTE_ORDER__
		*pWaveDataSize = INTEL_ORDER32(*pWaveDataSize);
#endif
	
	return 1;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

