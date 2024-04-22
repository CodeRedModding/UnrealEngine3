/*=============================================================================
 	CoreAudioSource.cpp: Unreal CoreAudio source interface object.
 	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

/*------------------------------------------------------------------------------------
 Audio includes.
 ------------------------------------------------------------------------------------*/

#include "Engine.h"
#include "EngineSoundClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "UnAudioDecompress.h"
#include "UnAudioEffect.h"
#include "CoreAudioDevice.h"
#include "CoreAudioEffects.h"

#define AUDIO_DISTANCE_FACTOR ( 0.0127f )

// CoreAudio functions may return error -10863 (output device not available) if called while OS X is switching to a different output device.
// This happens, for example, when headphones are plugged in or unplugged.
#define SAFE_CA_CALL(Expression)\
{\
	OSStatus Status = noErr;\
	do {\
		Status = Expression;\
	} while (Status == -10863);\
	check(Status == noErr);\
}
#define SAFE_CA_CALL_LOCAL(Expression)\
{\
	Status = noErr;\
	do {\
		Status = Expression;\
	} while (Status == -10863);\
	check(Status == noErr);\
}


FCoreAudioSoundSource *GAudioChannels[MAX_AUDIOCHANNELS + 1];

static INT FindFreeAudioChannel()
{
	for( INT Index = 1; Index < MAX_AUDIOCHANNELS + 1; Index++ )
	{
		if( GAudioChannels[Index] == NULL )
		{
			return Index;
		}
	}

	return 0;
}

/*------------------------------------------------------------------------------------
 FCoreAudioSoundSource.
 ------------------------------------------------------------------------------------*/

/**
 * Simple constructor
 */
FCoreAudioSoundSource::FCoreAudioSoundSource( UAudioDevice* InAudioDevice, FAudioEffectsManager* InEffects )
:	FSoundSource( InAudioDevice ),
	Buffer( NULL ),
	CoreAudioConverter( NULL ),
	CurrentBuffer( 0 ),
	bBuffersToFlush( FALSE ),
	bLoopCallback( FALSE ),
	SourceNode( 0 ),
	SourceUnit( NULL ),
	EQNode( 0 ),
	EQUnit( NULL ),
	RadioNode( 0 ),
	RadioUnit( NULL ),
	ReverbNode( 0 ),
	ReverbUnit( NULL ),
	AudioChannel( 0 ),
	BufferInUse( 0 ),
	NumActiveBuffers( 0 )
{
	AudioDevice = ( UCoreAudioDevice *)InAudioDevice;
	check( AudioDevice );
	Effects = ( FCoreAudioEffectsManager* )InEffects;
	check( Effects );

	appMemzero( CoreAudioBuffers, sizeof( CoreAudioBuffers ) );
}

/**
 * Destructor, cleaning up voice
 */
FCoreAudioSoundSource::~FCoreAudioSoundSource()
{
	FreeResources();
}

/**
 * Free up any allocated resources
 */
void FCoreAudioSoundSource::FreeResources( void )
{
	// If we're a streaming buffer...
	if( CurrentBuffer )
	{
		// ... free the buffers
		appFree( ( void* )CoreAudioBuffers[0].AudioData );
		appFree( ( void* )CoreAudioBuffers[1].AudioData );
		
		// Buffers without a valid resource ID are transient and need to be deleted.
		if( Buffer )
		{
			check( Buffer->ResourceID == 0 );
			delete Buffer;
			Buffer = NULL;
		}
		
		CurrentBuffer = 0;
	}
}

/** 
 * Submit the relevant audio buffers to the system
 */
void FCoreAudioSoundSource::SubmitPCMBuffers( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioSubmitBuffersTime );

	appMemzero( CoreAudioBuffers, sizeof( CoreAudioBuffer ) );
	
	CurrentBuffer = 0;
	
	CoreAudioBuffers[0].AudioData = Buffer->PCMData;
	CoreAudioBuffers[0].AudioDataSize = Buffer->PCMDataSize;
}

UBOOL FCoreAudioSoundSource::ReadProceduralData( const INT BufferIndex )
{
	TArray<BYTE> PCMBuffer;
	const INT MaxSamples = ( MONO_PCM_BUFFER_SIZE * Buffer->NumChannels ) / sizeof( SWORD );
	// !!! FIXME: maybe we should just pass a buffer pointer through UnrealScript (yuck!) and avoid all the array tapdancing.
	WaveInstance->WaveData->eventGeneratePCMData( PCMBuffer, MaxSamples );
	const INT Generated = PCMBuffer.Num() / sizeof( SWORD );
	appMemcpy( ( void* )CoreAudioBuffers[BufferIndex].AudioData, &PCMBuffer( 0 ), Min<INT>( Generated, MaxSamples ) * sizeof( SWORD ) );
	if( Generated < MaxSamples )
	{
		// silence the rest of the buffer if we didn't get enough data from the procedural audio generator.
		appMemzero( ( ( SWORD* )CoreAudioBuffers[BufferIndex].AudioData ) + Generated, ( MaxSamples - Generated ) * sizeof( SWORD ) );
	}
	
	// convenience return value: we're never actually "looping" here.
	return FALSE;  
}

UBOOL FCoreAudioSoundSource::ReadMorePCMData( const INT BufferIndex )
{
	CoreAudioBuffers[BufferIndex].ReadCursor = 0;
	NumActiveBuffers++;

	USoundNodeWave *WaveData = WaveInstance->WaveData;
	if( WaveData && WaveData->bProcedural )
	{
		return ReadProceduralData( BufferIndex );
	}
	else
	{
		return Buffer->ReadCompressedData( CoreAudioBuffers[BufferIndex].AudioData, WaveInstance->LoopingMode != LOOP_Never );
	}
}

/** 
 * Submit the relevant audio buffers to the system
 */
void FCoreAudioSoundSource::SubmitPCMRTBuffers( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioSubmitBuffersTime );

	appMemzero( CoreAudioBuffers, sizeof( CoreAudioBuffer ) * 2 );

	// Set the buffer to be in real time mode
	CurrentBuffer = 1;

	// Set up double buffer area to decompress to
	CoreAudioBuffers[0].AudioData = ( BYTE* )appMalloc( MONO_PCM_BUFFER_SIZE * Buffer->NumChannels );
	CoreAudioBuffers[0].AudioDataSize = MONO_PCM_BUFFER_SIZE * Buffer->NumChannels;

	CoreAudioBuffers[1].AudioData = ( BYTE* )appMalloc( MONO_PCM_BUFFER_SIZE * Buffer->NumChannels );
	CoreAudioBuffers[1].AudioDataSize = MONO_PCM_BUFFER_SIZE * Buffer->NumChannels;

	NumActiveBuffers = 0;
	BufferInUse = 0;
	
	ReadMorePCMData(0);
	ReadMorePCMData(1);
}

/**
 * Initializes a source with a given wave instance and prepares it for playback.
 *
 * @param	WaveInstance	wave instance being primed for playback
 * @return	TRUE			if initialization was successful, FALSE otherwise
 */
UBOOL FCoreAudioSoundSource::Init( FWaveInstance* InWaveInstance )
{
	// Find matching buffer.
	Buffer = FCoreAudioSoundBuffer::Init( AudioDevice, InWaveInstance->WaveData );
	
	// Buffer failed to be created, or there was an error with the compressed data
	if( Buffer && Buffer->NumChannels > 0 )
	{
		SCOPE_CYCLE_COUNTER( STAT_AudioSourceInitTime );
		
		WaveInstance = InWaveInstance;

		// Set whether to apply reverb
		SetReverbApplied( TRUE );

		// Submit audio buffers
		switch( Buffer->SoundFormat )
		{
			case SoundFormat_PCM:
			case SoundFormat_PCMPreview:
				SubmitPCMBuffers();
				break;
				
			case SoundFormat_PCMRT:
				SubmitPCMRTBuffers();
				break;
		}

		// Initialization succeeded.
		return( TRUE );
	}
	
	// Initialization failed.
	return FALSE;
}

/**
 * Updates the source specific parameter like e.g. volume and pitch based on the associated
 * wave instance.	
 */
void FCoreAudioSoundSource::Update( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioUpdateSources );

	if( !WaveInstance || Paused || !AudioChannel )
	{	
		return;
	}

	FLOAT Volume = WaveInstance->Volume * WaveInstance->VolumeMultiplier;
	if( SetStereoBleed() )
	{
		// Emulate the bleed to rear speakers followed by stereo fold down
		Volume *= 1.25f;
	}

	Volume = Clamp<FLOAT>( Volume, 0.0f, 1.0f );

	// apply global multiplier (ie to disable sound when not the foreground app)
	Volume *= GVolumeMultiplier;

	// Convert to dB
	Volume = 20.0 * log10(Volume);
	Volume = Clamp<FLOAT>( Volume, -120.0f, 20.0f );

	const FLOAT Pitch = Clamp<FLOAT>( WaveInstance->Pitch, 0.5f, 2.0f );

	// Set the HighFrequencyGain value
	SetHighFrequencyGain();

	FLOAT Azimuth = 0.0f;
	FLOAT Elevation = 0.0f;

	if( WaveInstance->bApplyRadioFilter )
	{
		Volume = WaveInstance->RadioFilterVolume;
	}
	else if( WaveInstance->bUseSpatialization )
	{
		FVector Direction = AudioDevice->InverseTransform.TransformFVector( WaveInstance->Location ).SafeNormal();
		FLOAT OldY = Direction.Y; 
		Direction.Y = Direction.Z;
		Direction.Z = OldY;

		FRotator Rotation = Direction.Rotation();
		Azimuth = ( Rotation.Yaw / 32767.0f ) * 180.0f;
		Elevation = ( Rotation.Pitch / 32767.0f ) * 90.0f;
	}

	SAFE_CA_CALL(AudioUnitSetParameter( AudioDevice->GetMixerUnit(), k3DMixerParam_Gain, kAudioUnitScope_Input, AudioChannel, Volume, 0 ));
	SAFE_CA_CALL(AudioUnitSetParameter( AudioDevice->GetMixerUnit(), k3DMixerParam_PlaybackRate, kAudioUnitScope_Input, AudioChannel, Pitch, 0 ));
	SAFE_CA_CALL(AudioUnitSetParameter( AudioDevice->GetMixerUnit(), k3DMixerParam_Azimuth, kAudioUnitScope_Input, AudioChannel, Azimuth, 0 ));
	SAFE_CA_CALL(AudioUnitSetParameter( AudioDevice->GetMixerUnit(), k3DMixerParam_Elevation, kAudioUnitScope_Input, AudioChannel, Elevation, 0 ));

}

/**
 * Plays the current wave instance.	
 */
void FCoreAudioSoundSource::Play( void )
{
	if( WaveInstance )
	{
		if( AttachToAUGraph() )
		{
			Paused = FALSE;
			Playing = TRUE;
			bLoopCallback = FALSE;

			// Updates the source which e.g. sets the pitch and volume.
			Update();
		}
	}
}

/**
 * Stops the current wave instance and detaches it from the source.	
 */
void FCoreAudioSoundSource::Stop( void )
{
	if( WaveInstance )
	{	
		if( Playing && AudioChannel )
		{
			DetachFromAUGraph();

			// Free resources
			FreeResources();
		}

		Paused = FALSE;
		Playing = FALSE;
		Buffer = NULL;
		bBuffersToFlush = FALSE;
		bLoopCallback = FALSE;
	}

	FSoundSource::Stop();
}

/**
 * Pauses playback of current wave instance.
 */
void FCoreAudioSoundSource::Pause( void )
{
	if( WaveInstance )
	{	
		if( Playing && AudioChannel )
		{
			DetachFromAUGraph();
		}

		Paused = TRUE;
	}
}

/**
 * Handles feeding new data to a real time decompressed sound
 */
void FCoreAudioSoundSource::HandleRealTimeSource( void )
{
//	// Update the double buffer toggle
//	CurrentBuffer++;
	
	// Get the next bit of streaming data
	const UBOOL bLooped = ReadMorePCMData(1 - BufferInUse);

	// Have we reached the end of the compressed sound?
	if( bLooped )
	{
		switch( WaveInstance->LoopingMode )
		{
			case LOOP_Never:
				// Play out any queued buffers - once there are no buffers left, the state check at the beginning of IsFinished will fire
				bBuffersToFlush = TRUE;
				break;
				
			case LOOP_WithNotification:
				// If we have just looped, and we are programmatically looping, send notification
				WaveInstance->NotifyFinished();
				break;
				
			case LOOP_Forever:
				// Let the sound loop indefinitely
				break;
		}
	}
}

/**
 * Queries the status of the currently associated wave instance.
 *
 * @return	TRUE if the wave instance/ source has finished playback and FALSE if it is 
 *			currently playing or paused.
 */
UBOOL FCoreAudioSoundSource::IsFinished( void )
{
	// A paused source is not finished.
	if( Paused )
	{
		return( FALSE );
	}
	
	if( WaveInstance )
	{
		// If not rendering, we're either at the end of a sound, or starved
		if( NumActiveBuffers == 0 )
		{
			// ... are we expecting the sound to be finishing?
			if( bBuffersToFlush || !CurrentBuffer )
			{
				// ... notify the wave instance that it has finished playing.
				WaveInstance->NotifyFinished();
				return( TRUE );
			}
		}

		// Service any real time sounds
		if( CurrentBuffer )
		{
			if( NumActiveBuffers < 1 )
			{
				// Continue feeding new sound data (unless we are waiting for the sound to finish)
				if( !bBuffersToFlush )
				{
					HandleRealTimeSource();
				}

				return( FALSE );
			}
		}

		// Notify the wave instance that the looping callback was hit
		if( bLoopCallback && WaveInstance->LoopingMode == LOOP_WithNotification )
		{
			WaveInstance->NotifyFinished();
			bLoopCallback = FALSE;
		}

		return( FALSE );
	}

	return( TRUE );
}

OSStatus FCoreAudioSoundSource::CreateAudioUnit( OSType Type, OSType SubType, OSType Manufacturer, AUNode DestNode, DWORD DestInputNumber, AUNode* OutNode, AudioUnit* OutUnit )
{
	AudioComponentDescription Desc;
	Desc.componentFlags = 0;
	Desc.componentFlagsMask = 0;
	Desc.componentType = Type;
	Desc.componentSubType = SubType;
	Desc.componentManufacturer = Manufacturer;

	OSStatus Status;

	SAFE_CA_CALL_LOCAL(AUGraphAddNode( AudioDevice->GetAudioUnitGraph(), &Desc, OutNode ));
	if( Status == noErr )
	{
		SAFE_CA_CALL_LOCAL(AUGraphNodeInfo( AudioDevice->GetAudioUnitGraph(), *OutNode, NULL, OutUnit ));
	}

	if( Status == noErr )
	{
		SAFE_CA_CALL_LOCAL(AudioUnitSetProperty( *OutUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &AudioDevice->MixerFormat, sizeof( AudioStreamBasicDescription )));
		if( Status == noErr )
		{
			SAFE_CA_CALL_LOCAL(AudioUnitSetProperty( *OutUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &AudioDevice->MixerFormat, sizeof( AudioStreamBasicDescription )));
		}
	}

	if( Status == noErr )
	{
		AudioUnitInitialize( *OutUnit );
		SAFE_CA_CALL_LOCAL(AUGraphConnectNodeInput( AudioDevice->GetAudioUnitGraph(), *OutNode, 0, DestNode, DestInputNumber ));
	}
	
	return Status;
}

UBOOL FCoreAudioSoundSource::AttachToAUGraph()
{
	AudioChannel = FindFreeAudioChannel();
	OSStatus Status;
	SAFE_CA_CALL_LOCAL(AudioConverterNew( &Buffer->PCMFormat, &AudioDevice->MixerFormat, &CoreAudioConverter ));
	if( Status != noErr )
	{
		warnf(TEXT("CoreAudioConverter creation failed, error code %d"), Status);
	}

	DWORD SpatialSetting = ( Buffer->NumChannels == 1 ) ? kSpatializationAlgorithm_SoundField : kSpatializationAlgorithm_StereoPassThrough;
	AudioUnitSetProperty( AudioDevice->GetMixerUnit(), kAudioUnitProperty_SpatializationAlgorithm, kAudioUnitScope_Input, AudioChannel, &SpatialSetting, sizeof( SpatialSetting ) );
	
	AUNode DestNode = AudioDevice->GetMixerNode();
	DWORD DestInputNumber = AudioChannel;
#if EQ_ENABLED
	if( IsEQFilterApplied() )
	{
		SAFE_CA_CALL_LOCAL(CreateAudioUnit( kAudioUnitType_Effect, kAudioUnitSubType_AUFilter, kAudioUnitManufacturer_Apple, DestNode, DestInputNumber, &EQNode, &EQUnit ));
		if( Status == noErr )
		{
			DestNode = EQNode;
			DestInputNumber = 0;
		}
	}
#endif
#if RADIO_ENABLED
	if( Effects->bRadioAvailable && WaveInstance->bApplyRadioFilter )
	{
		SAFE_CA_CALL_LOCAL(CreateAudioUnit( kAudioUnitType_Effect, 'Rdio', 'Epic', DestNode, DestInputNumber, &RadioNode, &RadioUnit ));
		if( Status == noErr )
		{
			DestNode = RadioNode;
			DestInputNumber = 0;
		}
	}
#endif
#if REVERB_ENABLED
	if( bReverbApplied )
	{
		SAFE_CA_CALL_LOCAL(CreateAudioUnit( kAudioUnitType_Effect, kAudioUnitSubType_MatrixReverb, kAudioUnitManufacturer_Apple, DestNode, DestInputNumber, &ReverbNode, &ReverbUnit ));
		if( Status == noErr )
		{
			DestNode = ReverbNode;
			DestInputNumber = 0;
		}
	}
#endif
	SAFE_CA_CALL_LOCAL(CreateAudioUnit( kAudioUnitType_FormatConverter, kAudioUnitSubType_AUConverter, kAudioUnitManufacturer_Apple, DestNode, DestInputNumber, &SourceNode, &SourceUnit ));
	if( Status == noErr )
	{
		AURenderCallbackStruct Input;
		Input.inputProc = &CoreAudioRenderCallback;
		Input.inputProcRefCon = this;
		SAFE_CA_CALL_LOCAL(AudioUnitSetProperty( SourceUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &Input, sizeof( Input ) ));
		checkf(Status == noErr);

		SAFE_CA_CALL_LOCAL(AUGraphUpdate( AudioDevice->GetAudioUnitGraph(), NULL ))
		checkf(Status == noErr);

		GAudioChannels[AudioChannel] = this;
	}
	return Status == noErr;
}

UBOOL FCoreAudioSoundSource::DetachFromAUGraph()
{
	AURenderCallbackStruct Input;
	Input.inputProc = NULL;
	Input.inputProcRefCon = NULL;
	SAFE_CA_CALL(AudioUnitSetProperty( SourceUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &Input, sizeof( Input ) ));

	if( EQNode )
	{
		SAFE_CA_CALL(AUGraphDisconnectNodeInput( AudioDevice->GetAudioUnitGraph(), EQNode, 0 ));
	}
	if( RadioNode )
	{
		SAFE_CA_CALL(AUGraphDisconnectNodeInput( AudioDevice->GetAudioUnitGraph(), RadioNode, 0 ));
	}
	if( ReverbNode )
	{
		SAFE_CA_CALL(AUGraphDisconnectNodeInput( AudioDevice->GetAudioUnitGraph(), ReverbNode, 0 ));
	}
	if( AudioChannel )
	{
		SAFE_CA_CALL(AUGraphDisconnectNodeInput( AudioDevice->GetAudioUnitGraph(), AudioDevice->GetMixerNode(), AudioChannel ));
	}
	
	SAFE_CA_CALL(AUGraphUpdate( AudioDevice->GetAudioUnitGraph(), NULL ));

	AudioConverterDispose( CoreAudioConverter );
	CoreAudioConverter = NULL;

	EQNode = 0;
	EQUnit = NULL;
	RadioNode = 0;
	RadioUnit = NULL;
	ReverbNode = 0;
	ReverbUnit = NULL;
	SourceNode = 0;
	SourceUnit = NULL;

	GAudioChannels[AudioChannel] = NULL;
	AudioChannel = 0;

	return TRUE;
}


OSStatus FCoreAudioSoundSource::CoreAudioRenderCallback( void *InRefCon, AudioUnitRenderActionFlags *IOActionFlags,
														const AudioTimeStamp *InTimeStamp, UInt32 InBusNumber,
														UInt32 InNumberFrames, AudioBufferList *IOData )
{
	OSStatus Status = noErr;
	FCoreAudioSoundSource *Source = ( FCoreAudioSoundSource *)InRefCon;

	DWORD DataByteSize = IOData->mBuffers[0].mDataByteSize;
	DWORD PacketsRequested = DataByteSize / sizeof( Float32 );
	DWORD PacketsObtained = 0;

	// AudioBufferList itself holds only one buffer, while AudioConverterFillComplexBuffer expects a couple of them
	struct
	{
		AudioBufferList BufferList;		
		AudioBuffer		SecondBuffer;
	} LocalBuffers;

	AudioBufferList *LocalBufferList = &LocalBuffers.BufferList;
	LocalBufferList->mNumberBuffers = IOData->mNumberBuffers;

	if( Source->Buffer && Source->Playing )
	{
		while( PacketsObtained < PacketsRequested )
		{
			INT BufferFilledBytes = PacketsObtained * sizeof( Float32 );
			for( DWORD Index = 0; Index < LocalBufferList->mNumberBuffers; Index++ )
			{
				LocalBufferList->mBuffers[Index].mDataByteSize = DataByteSize - BufferFilledBytes;
				LocalBufferList->mBuffers[Index].mData = ( BYTE *)IOData->mBuffers[Index].mData + BufferFilledBytes;
			}

			DWORD PacketCount = PacketsRequested - PacketsObtained;
			
			Status = AudioConverterFillComplexBuffer( Source->CoreAudioConverter, &CoreAudioConvertCallback, InRefCon, &PacketCount, LocalBufferList, NULL );

			PacketsObtained += PacketCount;

			if( ( PacketsObtained < PacketsRequested ) && ( Status == noErr ) && ( Source->NumActiveBuffers > 0 ) )
			{
				Source->NumActiveBuffers--;
				Source->BufferInUse = 1 - Source->BufferInUse;

				AudioConverterReset( Source->CoreAudioConverter );

				continue;
			}
			
			if( PacketCount == 0 )
			{
				break;
			}
		}

		if( PacketsObtained == 0 )
		{
			*IOActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
		}
	}
	else
	{
		*IOActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
	}

	if( PacketsObtained < PacketsRequested )
	{
		// Fill the rest of buffers provided with zeroes
		INT BufferFilledBytes = PacketsObtained * sizeof( Float32 );
		for( DWORD Index = 0; Index < IOData->mNumberBuffers; ++Index )
		{
			appMemzero( ( BYTE *)IOData->mBuffers[Index].mData + BufferFilledBytes, DataByteSize - BufferFilledBytes );
		}
	}
	
	return Status;
}

OSStatus FCoreAudioSoundSource::CoreAudioConvertCallback( AudioConverterRef Converter, UInt32 *IONumberDataPackets, AudioBufferList *IOData,
														 AudioStreamPacketDescription **OutPacketDescription, void *InUserData )
{
	FCoreAudioSoundSource *Source = ( FCoreAudioSoundSource *)InUserData;

	BYTE *Buffer = Source->CoreAudioBuffers[Source->BufferInUse].AudioData;
	INT BufferSize = Source->CoreAudioBuffers[Source->BufferInUse].AudioDataSize;
	INT ReadCursor = Source->CoreAudioBuffers[Source->BufferInUse].ReadCursor;

	INT PacketsAvailable = Source->Buffer ? ( BufferSize - ReadCursor ) / Source->Buffer->PCMFormat.mBytesPerPacket : 0;
	if( PacketsAvailable < *IONumberDataPackets )
	{
		*IONumberDataPackets = PacketsAvailable;
	}
	
	IOData->mBuffers[0].mData = *IONumberDataPackets ? Buffer + ReadCursor : NULL;
	IOData->mBuffers[0].mDataByteSize = Source->Buffer ? *IONumberDataPackets * Source->Buffer->PCMFormat.mBytesPerPacket : 0;
	ReadCursor += IOData->mBuffers[0].mDataByteSize;

	if( *IONumberDataPackets && ReadCursor == BufferSize )
	{
		if( Source->WaveInstance->LoopingMode == LOOP_Never )
		{
			Source->bLoopCallback = TRUE;
		}
	}

	Source->CoreAudioBuffers[Source->BufferInUse].ReadCursor = ReadCursor;
	
	return noErr;
}
