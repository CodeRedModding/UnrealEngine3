/*=============================================================================
	UnAsyncLoadingIPhone.cpp: CoreAudio sound playback implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "IPhoneDrv.h"
#include "AudioDeviceIPhone.h"
#include "EngineSoundClasses.h"
#import "IPhoneAppDelegate.h"
#include <OpenAL/oalStaticBufferExtension.h>

IMPLEMENT_CLASS(UAudioDeviceIPhone);

// TRUE when running in iOS5 or later
UBOOL GHasAudioBug = FALSE;
INT UAudioDeviceIPhone::SuspendCounter = 0;
#define SOUND_SOURCE_FREE 0
#define SOUND_SOURCE_LOCKED 1

inline void CheckStatus(OSStatus Status)
{
	if (Status != noErr)
	{
		NSLog(@"IPhone Audio Error %lu", Status);
	}
}
// Quick template to sign-extend < 8 bits
template <typename T, UINT B>
inline T SignExtend(const T ValueToExtend)
{
	struct {
		T ExtendedValue:B;
	} ExtenderStruct;
	return ExtenderStruct.ExtendedValue = ValueToExtend;
}

inline SWORD ReadSWORDFromByteStream(BYTE* ByteStream, INT& HeaderReadIndex)
{
	SWORD ValueRaw = 0;
	ValueRaw  = ByteStream[HeaderReadIndex++] << 0;
	ValueRaw |= ByteStream[HeaderReadIndex++] << 8;
	return ValueRaw;
}

static INT AdaptationTable[] = { 
	230, 230, 230, 230,
	307, 409, 512, 614,
	768, 614, 512, 409,
	307, 230, 230, 230
} ;
static INT AdaptationCoefficient1[] = {
	256,  512, 0, 192, 240,  460,  392
};
static INT AdaptationCoefficient2[] = {
	  0, -256, 0,  64,   0, -208, -232
};

struct ADPCMBlockPreamble {
	INT		Predictor;
	INT		Delta;
	SWORD	Sample1;
	SWORD	Sample2;
	INT		Coefficient1;
	INT		Coefficient2;
};

inline SWORD DecodeADPCMNibble(ADPCMBlockPreamble& BlockPreamble, BYTE EncodedNibble)
{
	BlockPreamble.Predictor = (
			(BlockPreamble.Sample1 * BlockPreamble.Coefficient1) +
			(BlockPreamble.Sample2 * BlockPreamble.Coefficient2)
		) / 256;
	BlockPreamble.Predictor += SignExtend<SBYTE, 4>(EncodedNibble) * BlockPreamble.Delta;
	BlockPreamble.Predictor = Clamp<INT>(BlockPreamble.Predictor, -32768, 32767);

	BlockPreamble.Sample2 = BlockPreamble.Sample1;
	BlockPreamble.Sample1 = BlockPreamble.Predictor;
	BlockPreamble.Delta = (AdaptationTable[EncodedNibble] * BlockPreamble.Delta) / 256;
	BlockPreamble.Delta = Max<INT>(BlockPreamble.Delta, 16);

	return BlockPreamble.Predictor;
}

void DecodeADPCMBlock(BYTE* EncodedADPCMBlock, INT BlockSize, SWORD* DecodedPCMData)
{
	INT HeaderReadIndex = 0;
	INT DecodedOutputIndex = 0;
	ADPCMBlockPreamble BlockPreamble;

	BlockPreamble.Predictor = EncodedADPCMBlock[HeaderReadIndex++];
	BlockPreamble.Delta = ReadSWORDFromByteStream(EncodedADPCMBlock, HeaderReadIndex);
	BlockPreamble.Sample1 = ReadSWORDFromByteStream(EncodedADPCMBlock, HeaderReadIndex);
	BlockPreamble.Sample2 = ReadSWORDFromByteStream(EncodedADPCMBlock, HeaderReadIndex);
	BlockPreamble.Coefficient1 = AdaptationCoefficient1[BlockPreamble.Predictor];
	BlockPreamble.Coefficient2 = AdaptationCoefficient2[BlockPreamble.Predictor];
	DecodedPCMData[DecodedOutputIndex++] = BlockPreamble.Sample2;
	DecodedPCMData[DecodedOutputIndex++] = BlockPreamble.Sample1;

	BYTE EncodedNibblePair;
	BYTE EncodedNibble;
	for( INT NibblePairIndex = HeaderReadIndex; NibblePairIndex < BlockSize; NibblePairIndex++ )
	{
		EncodedNibblePair = EncodedADPCMBlock[NibblePairIndex];

		EncodedNibble = (EncodedNibblePair >> 4) & 0x0f;
		DecodedPCMData[DecodedOutputIndex++] = DecodeADPCMNibble(BlockPreamble, EncodedNibble);

		EncodedNibble = (EncodedNibblePair >> 0) & 0x0f;
		DecodedPCMData[DecodedOutputIndex++] = DecodeADPCMNibble(BlockPreamble, EncodedNibble);
	}
}

inline bool LockSoundSource(FSoundSourceIPhone* Source, INT Channel)
{
	return OSAtomicCompareAndSwap32(SOUND_SOURCE_FREE, SOUND_SOURCE_LOCKED, &Source->SourceLock[Channel]);
}

inline void UnlockSoundSource(FSoundSourceIPhone* Source, INT Channel)
{
	Boolean SwapResult = OSAtomicCompareAndSwap32(SOUND_SOURCE_LOCKED, SOUND_SOURCE_FREE, &Source->SourceLock[Channel]);
	check(SwapResult == true);
}

static OSStatus InputRenderCallbackADPCM(FSoundSourceIPhone* Source, AudioSampleType* OutData, UInt32 NumberOfFrames, UINT Channel)
{
	while (NumberOfFrames > 0)
	{
		// decompress a block if we have no data
		if (Source->StreamingBufferOffset[Channel] == 0)
		{
			check(Source->StreamingBuffer[Channel] != NULL);
			DecodeADPCMBlock((BYTE*)Source->Buffer->Samples + Source->SampleOffset[Channel], Source->Buffer->CompressedBlockSize, 
							 Source->StreamingBuffer[Channel]);
		}
		// get number of samples in the current buffer to use
		// note: StreamingBufferOffset is in SWORDs, not BYTEs!
		UINT NumSamples = Min(NumberOfFrames, Source->Buffer->UncompressedBlockSize / sizeof(SWORD) - Source->StreamingBufferOffset[Channel]);
		//@todo can we do a bulk copy
		for (INT SampleScan = 0; SampleScan < NumSamples; SampleScan++)
		{
			*OutData++ = Source->StreamingBuffer[Channel][Source->StreamingBufferOffset[Channel]++];
		}
		NumberOfFrames -= NumSamples;

		check(Source->StreamingBufferOffset[Channel] * sizeof(SWORD) <= Source->Buffer->UncompressedBlockSize);
		if (Source->StreamingBufferOffset[Channel] * sizeof(SWORD) == Source->Buffer->UncompressedBlockSize)
		{
			// finished up a block, move on
			Source->StreamingBufferOffset[Channel] = 0;
			Source->SampleOffset[Channel] += Source->Buffer->CompressedBlockSize;
			
			check(Source->SampleOffset[Channel] <= Source->Buffer->BufferSize);

			UINT ChannelZeroSamplesEnd;
			if (Source->Buffer->NumChannels == 2)
			{
				ChannelZeroSamplesEnd = Source->Buffer->BufferSize >> 1;
			}
			else
			{
				ChannelZeroSamplesEnd = Source->Buffer->BufferSize;
			}

			UBOOL ReachedEndOfChannelBuffer = FALSE;
			if (Channel == 0 && (Source->SampleOffset[Channel] == ChannelZeroSamplesEnd))
			{
				Source->SampleOffset[Channel] = 0;
				ReachedEndOfChannelBuffer = TRUE;
			}
			else if (Channel == 1 && Source->SampleOffset[Channel] == Source->Buffer->BufferSize)
			{
				Source->SampleOffset[Channel] = (Source->Buffer->BufferSize >> 1);
				ReachedEndOfChannelBuffer = TRUE;

			}
			if (ReachedEndOfChannelBuffer)
			{
				// if we are not a looping sound, then we are done with playback
				if (!Source->bIsLooping)
				{
					Source->bIsFinished[Channel] = TRUE;
					// fill in the rest of the output audio with silence and return
					appMemzero(OutData, NumberOfFrames);
					return noErr;
				}
			}
		}
	}
	return noErr;
}

static OSStatus InputRenderCallbackLPCM(FSoundSourceIPhone* Source, AudioSampleType* OutData, UInt32 NumberOfFrames, UINT Channel)
{
	UINT ChannelStride = Source->Buffer->NumChannels;
	//Each sample is 2 bytes, channel data is interlaced if stereo
	//div 2 for mono(one sample every two bytes), div 4 if stereo (one sample every two bytes but channels are interlaced)
	//Using num channels to reduce branching
	UINT NumSamplesForChannel = (Source->Buffer->BufferSize >> Source->Buffer->NumChannels);
	while (NumberOfFrames > 0)
	{	
		//Determine the number of fRAMES we can render
		UInt32 NumFramesRemaining =  NumSamplesForChannel - Source->SampleOffset[Channel];
		UInt32 NumFramesToRender = Min(NumberOfFrames, NumFramesRemaining);

		//Render frames to ouput buffer
		for (INT SampleScan = 0; SampleScan < NumFramesToRender; ++SampleScan)
		{
			UInt32 CurOffset = Source->SampleOffset[Channel]++;
			*OutData++ = Source->Buffer->Samples[CurOffset*ChannelStride+Channel];
		}
		NumberOfFrames -= NumFramesToRender;

		//Do we have more frames to render
		if (NumberOfFrames > 0)
		{
			if(Source->bIsLooping)
			{
				Source->SampleOffset[Channel] = 0;
			}
			else
			{
				Source->bIsFinished[Channel] = TRUE;
				appMemzero(OutData, NumberOfFrames);
				NumberOfFrames = 0;
			}
		}
	}

	return noErr;
}

static OSStatus InputRenderCallback(void* RefCon, AudioUnitRenderActionFlags* ActionFlags, const AudioTimeStamp* TimeStamp,
									UInt32 BusNumber, UInt32 NumberOfFrames, AudioBufferList* IOData)
{
	FSoundSourceIPhone* Source = (FSoundSourceIPhone*)RefCon;
	UINT Channel = BusNumber & 1;
	AudioSampleType* OutData = (AudioSampleType*)IOData->mBuffers[0].mData;
	//if we are unable to aquire a lock return
	if (!LockSoundSource(Source, Channel))
	{
		appMemzero(OutData, NumberOfFrames);
		return -1;
	}

    //NSLog(@"Rendering %d", BusNumber);
	if (Source->Buffer == NULL || Source->IsPlaying() == FALSE || Source->IsPaused() || Source->bIsFinished[Channel])
	{
		appMemzero(OutData, NumberOfFrames);
		UnlockSoundSource(Source, Channel);
		return -1;
	}

	UINT NumChannels = Source->Buffer->NumChannels;
	
	if (NumChannels == 1 && Channel == 1)
	{
		appMemzero(OutData, NumberOfFrames);
		UnlockSoundSource(Source, Channel);
		return -1;
	}

	
	OSStatus ReturnStatus;
	if (Source->Buffer->FormatTag == FORMAT_LPCM)
	{
		ReturnStatus = InputRenderCallbackLPCM(Source, OutData, NumberOfFrames, Channel);
	}
	else
	{
		ReturnStatus = InputRenderCallbackADPCM(Source, OutData, NumberOfFrames, Channel);
	}

	UnlockSoundSource(Source, Channel);

	return ReturnStatus;
}


//==============================================================================
//UAudioDeviceIPhone
//==============================================================================

/**
 * Initializes the audio device and creates sources.
 *
 * @warning: 
 *
 * @return TRUE if initialization was successful, FALSE otherwise
 */
UBOOL UAudioDeviceIPhone::Init( void )
{
	// cache if we have iOS5 support
	GHasAudioBug = [IPhoneAppDelegate GetDelegate].OSVersion == 5.0f;

	// 32bit linear PCM
	size_t SampleSize = sizeof(AudioSampleType);
	
	DOUBLE GraphSampleRate = 44100.0;
	[[AVAudioSession sharedInstance] setPreferredHardwareSampleRate:GraphSampleRate error:nil];
	[[AVAudioSession sharedInstance] setActive:YES error:nil];
	GraphSampleRate = [[AVAudioSession sharedInstance] currentHardwareSampleRate];

	// linear pcm stream format
	StreamDesc.mFormatID          = kAudioFormatLinearPCM;
	StreamDesc.mFormatFlags       = kAudioFormatFlagsCanonical;
	StreamDesc.mBytesPerPacket    = SampleSize;
	StreamDesc.mFramesPerPacket   = 1;
	StreamDesc.mBytesPerFrame     = SampleSize;
	StreamDesc.mChannelsPerFrame  = 1;
	StreamDesc.mBitsPerChannel    = 8 * SampleSize;
	StreamDesc.mSampleRate        = GraphSampleRate;
		
	NSLog(@"sampleRate = %f", StreamDesc.mSampleRate);

	// new graph
	OSStatus Result = NewAUGraph(&ProcessingGraph);
	CheckStatus(Result);

	
	// set up nodes in the graph

	// I/O unit
	AudioComponentDescription IOUnitDescription;
	IOUnitDescription.componentType          = kAudioUnitType_Output;
	IOUnitDescription.componentSubType       = kAudioUnitSubType_RemoteIO;
	IOUnitDescription.componentManufacturer  = kAudioUnitManufacturer_Apple;
	IOUnitDescription.componentFlags         = 0;
	IOUnitDescription.componentFlagsMask     = 0;

	// Add the nodes to the audio processing graph
	AUNode IONode;
	Result = AUGraphAddNode(ProcessingGraph, &IOUnitDescription, &IONode);
	CheckStatus(Result);
	
	// Multichannel mixer unit
	AudioComponentDescription MixerUnitDescription;
	MixerUnitDescription.componentType          = kAudioUnitType_Mixer;
	MixerUnitDescription.componentSubType       = kAudioUnitSubType_AU3DMixerEmbedded;
	MixerUnitDescription.componentManufacturer  = kAudioUnitManufacturer_Apple;
	MixerUnitDescription.componentFlags         = 0;
	MixerUnitDescription.componentFlagsMask     = 0;

	Result = AUGraphAddNode(ProcessingGraph, &MixerUnitDescription, &MixerNode);
	CheckStatus(Result);
	
	// open the graph
	Result = AUGraphOpen(ProcessingGraph);
	CheckStatus(Result);
	
	// grab the Mixer unit from the Mixer node
	Result = AUGraphNodeInfo(ProcessingGraph, MixerNode, NULL, &MixerUnit);
	CheckStatus(Result);
	
	// setup the mixer (*2 for stereo)
	UINT BusCount = MaxChannels * 2;
	Result = AudioUnitSetProperty(MixerUnit, kAudioUnitProperty_ElementCount, 
								  kAudioUnitScope_Input, 0, &BusCount, sizeof(BusCount));
	CheckStatus(Result);


	// Initialize channels (value comes from .ini)
	for( INT i = 0; i < MaxChannels; i++ )
	{
		FSoundSourceIPhone* Source = new FSoundSourceIPhone(this, i);
		Sources.AddItem( Source );
		FreeSources.AddItem( Source );
	}
	debugf(NAME_Init, TEXT( "CoreAudioAudioDevice: Allocated %i sources" ), MaxChannels);

	// Set up a default (nop) effects manager 
	Effects = new FAudioEffectsManager( this );

	// Initialized.
	NextResourceID = 1;

	// set the mixer unit's output sample rate - this must be set
	Result = AudioUnitSetProperty(MixerUnit, kAudioUnitProperty_SampleRate, kAudioUnitScope_Output,
								   0, &GraphSampleRate, sizeof(GraphSampleRate));
	CheckStatus(Result);

	// connect mixer output to IO input
	Result = AUGraphConnectNodeInput(ProcessingGraph, MixerNode, 0, IONode, 0);
	CheckStatus(Result);
	
	NSLog(@"Graph:");
	CAShow(ProcessingGraph);
	
	// initialize the graph
	Result = AUGraphInitialize(ProcessingGraph);
	CheckStatus(Result);
	
	// start it up, each source will play as appropriate
	Result = AUGraphStart(ProcessingGraph);
	CheckStatus(Result);


	// Initialize base class last as it's going to precache already loaded audio.
	return Super::Init();
}

/**
 * Tears down audio device by stopping all sounds, removing all buffers, 
 * destroying all sources, ... Called by both Destroy and ShutdownAfterError
 * to perform the actual tear down.
 */
void UAudioDeviceIPhone::Teardown( void )
{
	// Flush stops all sources and deletes all buffers so sources can be safely deleted below.
	Flush( NULL );	

	// Destroy all sound sources
	for( INT i = 0; i < Sources.Num(); i++ )
	{
		delete Sources( i );
	}
}

void UAudioDeviceIPhone::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	if( Ar.IsCountingMemory() )
	{
		Ar.CountBytes( Buffers.Num() * sizeof( FSoundBufferIPhone ), Buffers.Num() * sizeof( FSoundBufferIPhone ) );
		Buffers.CountBytes( Ar );
		WaveBufferMap.CountBytes( Ar );
	}
}

/**
 * Shuts down audio device. This will never be called with the memory image codepath.
 */
void UAudioDeviceIPhone::FinishDestroy( void )
{
	if( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		Teardown();
		debugf( NAME_Exit, TEXT( "Core Audio Device shut down." ) );
	}

	Super::FinishDestroy();
}

/**
 * Special variant of Destroy that gets called on fatal exit. Doesn't really
 * matter on the console so for now is just the same as Destroy so we can
 * verify that the code correctly cleans up everything.
 */
void UAudioDeviceIPhone::ShutdownAfterError( void )
{
	if( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		Teardown();
		debugf( NAME_Exit, TEXT( "UAudioDeviceIPhone::ShutdownAfterError" ) );
	}

	Super::ShutdownAfterError();
}

void UAudioDeviceIPhone::Update( UBOOL Realtime )
{
	Super::Update( Realtime );

	PlayerLocation = Listeners(0).Location;
	PlayerFacing = Listeners(0).Front;
	PlayerUp = Listeners(0).Up;
	PlayerRight = Listeners(0).Right;
}

void UAudioDeviceIPhone::Precache( USoundNodeWave* Wave )
{
	FSoundBufferIPhone::Init( Wave, this );

#if STATS
	// Stat housekeeping.
	FSoundBufferIPhone* Buffer = WaveBufferMap.FindRef( Wave->ResourceID );
	if ( Buffer )
	{
		//debugf( TEXT( "UAudioDeviceIPhone::Precache. %s %d"), *Wave->GetName(), Buffer->BufferSize );
		INC_DWORD_STAT_BY( STAT_AudioMemorySize, Buffer->BufferSize);
		INC_DWORD_STAT_BY( STAT_AudioMemory, Buffer->BufferSize);
	}
#endif
}

void UAudioDeviceIPhone::FreeResource( USoundNodeWave* SoundNodeWave )
{
	// Find buffer for resident wavs
	if( SoundNodeWave->ResourceID )
	{
		// Find buffer associated with resource id.
		FSoundBufferIPhone* Buffer = WaveBufferMap.FindRef( SoundNodeWave->ResourceID );
		if( Buffer )
		{
			// Remove from buffers array.
			Buffers.RemoveItem( Buffer );
			WaveBufferMap.Remove( SoundNodeWave->ResourceID );

			// See if it is being used by a sound source...
			for( INT SrcIndex = 0; SrcIndex < Sources.Num(); SrcIndex++ )
			{
				FSoundSourceIPhone* Src = ( FSoundSourceIPhone* )( Sources( SrcIndex ) );
				if( Src && Src->Buffer && ( Src->Buffer == Buffer ) )
				{
					Src->Stop();
					break;
				}
			}

#if STATS			
			//debugf( TEXT( "UAudioDeviceIPhone::FreeResource. %s %d"), *SoundNodeWave->GetName(), (INT)Buffer->BufferSize );
			DEC_DWORD_STAT_BY(STAT_AudioMemorySize, Buffer->BufferSize);
			DEC_DWORD_STAT_BY(STAT_AudioMemory, Buffer->BufferSize);			
#endif

			delete Buffer;
		}

		SoundNodeWave->ResourceID = 0;
	}

	// .. or reference to compressed data
	SoundNodeWave->RemoveAudioResource();
}

// sort memory usage from large to small unless bAlphaSort
static UBOOL bAlphaSort = FALSE;

IMPLEMENT_COMPARE_POINTER( FSoundBufferIPhone, AudioDeviceIPhone, { 
	if( bAlphaSort == TRUE ) \
	{ \
		return( appStricmp( *A->ResourceName, *B->ResourceName ) ); \
	} \
	\
	return B->BufferSize - A->BufferSize; \
}
);

/** 
 * Displays debug information about the loaded sounds
 */
void UAudioDeviceIPhone::ListSounds( const TCHAR* Cmd, FOutputDevice& Ar )
{
	bAlphaSort = ParseParam( Cmd, TEXT( "ALPHASORT" ) );

	INT	TotalSoundSize = 0;

	Ar.Logf( TEXT( "Sound resources:" ) );

	TArray<FSoundBufferIPhone*> AllSounds = Buffers;

	Sort<USE_COMPARE_POINTER( FSoundBufferIPhone, AudioDeviceIPhone )>( &AllSounds( 0 ), AllSounds.Num() );

	for( INT i = 0; i < AllSounds.Num(); ++i )
	{
		FSoundBufferIPhone* Buffer = AllSounds(i);
		INT BufferSize = Buffer->BufferSize;
		Ar.Logf( TEXT( "RawData: %8.2f Kb in %s sound %s" ), BufferSize / 1024.0f, TEXT("ADPCM"), *Buffer->ResourceName );
		TotalSoundSize += BufferSize;
	}

	Ar.Logf( TEXT( "%8.2f Kb for %d sounds" ), TotalSoundSize / 1024.0f, AllSounds.Num() );
}

/**
 * Initialize OpenAL early and in a thread to not delay startup by .66 seconds or so (on iPhone)
 */
void UAudioDeviceIPhone::ThreadedStaticInit()
{

}

void UAudioDeviceIPhone::ResumeContext()
{
	appInterlockedDecrement(&SuspendCounter);
}

void UAudioDeviceIPhone::SuspendContext()
{
	appInterlockedIncrement(&SuspendCounter);
}

//==============================================================================
// FSoundSourceIPhone
//==============================================================================
FSoundSourceIPhone::FSoundSourceIPhone(UAudioDeviceIPhone* InAudioDevice, UINT InBusNumber)
	: FSoundSource(InAudioDevice)
	, Buffer(NULL)
	, BusNumber(InBusNumber)
	, StreamingBufferSize(0)
{

	AURenderCallbackStruct RenderCallbackStruct;	
	RenderCallbackStruct.inputProcRefCon  = this;
	RenderCallbackStruct.inputProc = &InputRenderCallback;

	// Set a callback for the specified node's specified input
	OSStatus Result;
	for (INT Channel = 0; Channel < 2; Channel++)
	{
		StreamingBuffer[Channel] = NULL;
		SampleOffset[Channel] = 0;
		StreamingBufferOffset[Channel] = 0;
		bIsFinished[Channel] = FALSE;

		SourceLock[Channel] = SOUND_SOURCE_FREE;
	
		Result = AudioUnitSetParameter(InAudioDevice->MixerUnit, k3DMixerParam_Enable, 
												kAudioUnitScope_Input, BusNumber * 2 + Channel, 0, 0);	
		CheckStatus(Result);

		Result = AudioUnitSetParameter(InAudioDevice->MixerUnit, k3DMixerParam_Gain,
												kAudioUnitScope_Input, BusNumber * 2 + Channel, -120.0, 0);
		CheckStatus(Result);

		Result = AUGraphSetNodeInputCallback(InAudioDevice->ProcessingGraph, InAudioDevice->MixerNode, 
														BusNumber * 2 + Channel, &RenderCallbackStruct);
		CheckStatus(Result);
	}
}

/**
 * Clean up any hardware referenced by the sound source
 */
FSoundSourceIPhone::~FSoundSourceIPhone( void )
{
	for (INT Channel = 0; Channel < 2; ++Channel)
	{
		appFree(StreamingBuffer[Channel]);
	}
}

UBOOL FSoundSourceIPhone::Init(FWaveInstance* InWaveInstance)
{
	UAudioDeviceIPhone* IPhoneAudioDevice = (UAudioDeviceIPhone*)AudioDevice;

	// Find matching buffer.
	Buffer = FSoundBufferIPhone::Init(InWaveInstance->WaveData, IPhoneAudioDevice);

	if (Buffer == NULL || Buffer->FormatTag != FORMAT_LPCM && Buffer->FormatTag != FORMAT_ADPCM)
	{
		// can happen legitimately if sound was intentionally stripped for memory
		//debugf(TEXT("    - No buffer %s"), InWaveInstance->WaveData ? *InWaveInstance->WaveData->GetPathName() : TEXT("NULL") );
		return FALSE;
	}
	
	SCOPE_CYCLE_COUNTER( STAT_AudioSourceInitTime );		

	WaveInstance = InWaveInstance;
	bIsLooping = InWaveInstance->LoopingMode != LOOP_Never;
	Pan = 0.0f;
	Volume = 1.0f;
	Pitch = 1.0f;


	if (Buffer->FormatTag == FORMAT_LPCM)
	{
		SampleOffset[0] = 0;
		SampleOffset[1] = 0;
	}
	//ADPCM
	else
	{
		SampleOffset[0] = 0;
		//Stereo is split into two encoded discrete sections in the buffer they are NOT interlaced
		SampleOffset[1] = Buffer->BufferSize / 2;

		//Instance could have been intialized for mono 
		//and now may need to be updated for stereo
		//so the logic is a bit more complicated then it used to be
		//before we had stereo support
		UINT BufferSize = StreamingBufferSize;
		UBOOL BufferSizeChanged = FALSE;
		if (StreamingBufferSize < Buffer->UncompressedBlockSize)
		{
			BufferSizeChanged = TRUE;
			BufferSize = Buffer->UncompressedBlockSize;
		}

		for (INT Channel = 0; Channel < Buffer->NumChannels; ++Channel)
		{
			if (StreamingBuffer[Channel] == NULL || BufferSizeChanged)
			{
				appFree(StreamingBuffer[Channel]);
				StreamingBuffer[Channel] = (SWORD*)appMalloc(BufferSize);
			}
		}
		//don't keep around the second channel streaming buffer for stereo if this 
		//instance is being reused for mono
		if (Buffer->NumChannels == 1)
		{
			bIsFinished[1] = TRUE;
			appFree(StreamingBuffer[1]);
			StreamingBuffer[1] = NULL;
		}

		StreamingBufferSize = BufferSize;
	}

	
	// set the sample rate parameter of the shared stream descriptor (all other fields are proper)
	IPhoneAudioDevice->StreamDesc.mSampleRate = Buffer->SampleRate;

	// hook up one or two bus inputs for this sound, depending on stereo or not
	for (INT Channel = 0; Channel < Buffer->NumChannels; Channel++)
	{
		StreamingBufferOffset[Channel] = 0;
		bIsFinished[Channel] = FALSE;

		//todo shawn.harris - May be able to intialize these once during construction?
		OSStatus Result = AudioUnitSetProperty(IPhoneAudioDevice->MixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, BusNumber * 2 + Channel,
											   &IPhoneAudioDevice->StreamDesc, sizeof(AudioStreamBasicDescription));
		CheckStatus(Result);
		 
		// need to put it at some distnace (1 meter) to get any panning
		Result = AudioUnitSetParameter(IPhoneAudioDevice->MixerUnit, k3DMixerParam_Distance, kAudioUnitScope_Input, BusNumber * 2 + Channel, 1.0, 0);
		CheckStatus(Result);

		//Set panning for stereo which is always dead split and never has spatialization
		if(Buffer->NumChannels == 2)
		{
			const FLOAT AzimuthRangeScale = 90.f;
			//Left is channel 0, right is Channel 1
			//-90 - Extreme Left, 90 - Extreme Right
			Pan =(-1.f + (Channel*2.f))*AzimuthRangeScale;
		}
		else if (!WaveInstance->bUseSpatialization)
		{
			//Else center mono sound
			Pan = 0;
		}
		Result = AudioUnitSetParameter(IPhoneAudioDevice->MixerUnit, k3DMixerParam_Azimuth, kAudioUnitScope_Input, BusNumber * 2 + Channel, Pan, 0);
	}

	// make sure it's not playing (Pause has some good logic to disble the bus)
	Pause();
	Paused = FALSE;

	// set pitch/volume
	Update();

	return TRUE;
}

/**
 * Updates the source specific parameter like e.g. volume and pitch based on the associated
 * wave instance.	
 */
void FSoundSourceIPhone::Update( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioUpdateSources );
	if (!WaveInstance || Paused)
	{
		return;
	}

	FLOAT Volume = WaveInstance->Volume * WaveInstance->VolumeMultiplier;
	if (SetStereoBleed())
	{
		// Emulate the bleed to rear speakers followed by stereo fold down
		Volume *= 1.25f;
	}
	Volume		= Clamp<FLOAT>(Volume, 0.0f, 1.0f);
	FLOAT Pitch = Clamp<FLOAT>(WaveInstance->Pitch, 0.5f, 2.0f);

	UAudioDeviceIPhone* Device = (UAudioDeviceIPhone*)AudioDevice;

	//We only adjust panning on playback for mono sounds that want spatialization
	if (Buffer->NumChannels == 1 && WaveInstance->bUseSpatialization)
	{
		// compute the directional offset
		FVector Offset = WaveInstance->Location - Device->PlayerLocation;
			
		const FLOAT AzimuthRangeScale = 90.f;
		// we use the Right direction because we want an object directly to our right to
		// be 1.0, directly to our left to be -1.0, and straight ahead or behind to be 0.0f
		Pan = (Offset.Dot2d(Device->PlayerRight)) * AzimuthRangeScale;
		OSStatus Result = AudioUnitSetParameter(Device->MixerUnit, k3DMixerParam_Azimuth, kAudioUnitScope_Input, BusNumber * 2, Pan, 0);	
		CheckStatus(Result);
	}

	// convert Volume to logarithmic Gain
	AudioUnitParameterValue Gain = Clamp(20.0f * log10f(Volume), -120.0f, 20.0f);
		
	// update playback/mixer parameters
	for (INT Channel = 0; Channel < Buffer->NumChannels; Channel++)
	{
		OSStatus Result;

        //NSLog(@"Setting bus %d gain to %f / %f", BusNumber * 2 + Channel, Gain, Volume);
		Result = AudioUnitSetParameter(Device->MixerUnit, k3DMixerParam_Gain,
										kAudioUnitScope_Input, BusNumber * 2 + Channel, Gain, 0);
		CheckStatus(Result);

		Result = AudioUnitSetParameter(Device->MixerUnit, k3DMixerParam_PlaybackRate,
										kAudioUnitScope_Input, BusNumber * 2 + Channel, Pitch, 0);
		CheckStatus(Result);
	}
}

/**
 * Plays the current wave instance.	
 */
void FSoundSourceIPhone::Play( void )
{
	UAudioDeviceIPhone* IPhoneAudioDevice = (UAudioDeviceIPhone*)AudioDevice;

	if( WaveInstance )
	{	
		// enable the mixer input for this source
		for (INT Channel = 0; Channel < Buffer->NumChannels; Channel++)
		{
			OSStatus Result = AudioUnitSetParameter(IPhoneAudioDevice->MixerUnit, k3DMixerParam_Enable, 
													kAudioUnitScope_Input, BusNumber * 2 + Channel, 1, 0);
			CheckStatus(Result);
		}

		Paused = FALSE;
		Playing = TRUE;
	}
}

/**
 * Stops the current wave instance and detaches it from the source.	
 */
void FSoundSourceIPhone::Stop( void )
{
	if( WaveInstance)
	{
		// Stop future callbacks on this sound source
		Pause();

		//if we are unable to aquire a lock return
		while(!LockSoundSource(this, 0))
		{
			debugf(TEXT("SOUND:Waiting for source to unlock %s"), *WaveInstance->WaveData->GetPathName());
			appSleep(0.f);
		}

		while(!LockSoundSource(this, 1))
		{
			debugf(TEXT("SOUND:Waiting for source to unlock %s"), *WaveInstance->WaveData->GetPathName());
			appSleep(0.f);
		}

		// Cleanup after all the threads are finished
		Buffer = NULL;
		Paused = FALSE;
		Playing = FALSE;

		FSoundSource::Stop();

		UnlockSoundSource(this, 0);
		UnlockSoundSource(this, 1);
	}
}

/**
 * Pauses playback of current wave instance.
 */
void FSoundSourceIPhone::Pause( void )
{
	UAudioDeviceIPhone* IPhoneAudioDevice = (UAudioDeviceIPhone*)AudioDevice;

	if( WaveInstance )
	{
		// disable the mixer input for this source, and set volume to 0, but update will fix it when unpaused
		for (INT Channel = 0; Channel < Buffer->NumChannels; Channel++)
		{
			OSStatus Result = AudioUnitSetParameter(IPhoneAudioDevice->MixerUnit, k3DMixerParam_Enable, 
													kAudioUnitScope_Input, BusNumber * 2 + Channel, 0, 0);	
			CheckStatus(Result);

			Result = AudioUnitSetParameter(IPhoneAudioDevice->MixerUnit, k3DMixerParam_Gain,
													kAudioUnitScope_Input, BusNumber * 2 + Channel, -120.0, 0);
			CheckStatus(Result);
		}

		Paused = TRUE;
	}
}

/** 
 * Queries the status of the currently associated wave instance.
 *
 * @return	TRUE if the wave instance/ source has finished playback and FALSE if it is 
 *			currently playing or paused.
 */
UBOOL FSoundSourceIPhone::IsFinished( void )
{
	if (WaveInstance)
	{
		if (bIsFinished[0] && bIsFinished[1])
		{
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}

	// no wave means we are done
	return TRUE;
}

//==============================================================================
//FSoundBufferIPhone
//==============================================================================

/** 
 * @param AudioDevice	audio device this sound buffer is going to be attached to.
 */
FSoundBufferIPhone::FSoundBufferIPhone( UAudioDeviceIPhone* InAudioDevice )
{
	Samples = NULL;
}

/**
 * Frees wave data and detaches itself from audio device.
 */
FSoundBufferIPhone::~FSoundBufferIPhone( void )
{
	appFree(Samples);
}

/**
 * Static function used to create a buffer.
 *
 * @param InWave		USoundNodeWave to use as template and wave source
 * @param AudioDevice	audio device to attach created buffer to
 */
FSoundBufferIPhone* FSoundBufferIPhone::Init( USoundNodeWave* Wave, UAudioDeviceIPhone* AudioDevice )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioResourceCreationTime );

	// Can't create a buffer without any source data
	if (Wave == NULL || Wave->NumChannels == 0 || Wave->CompressedIPhoneData.GetElementCount() == 0)
	{
		return NULL;
	}

	FSoundBufferIPhone* Buffer = NULL;

	// Find the existing buffer if any
	if( Wave->ResourceID )
	{
		Buffer = AudioDevice->WaveBufferMap.FindRef( Wave->ResourceID );
	}

	if( Buffer == NULL )
	{
		// Create new buffer.
		Buffer = new FSoundBufferIPhone( AudioDevice );

		// Allocate new resource ID and assign to USoundNodeWave. A value of 0 (default) means not yet registered.
		INT ResourceID = AudioDevice->NextResourceID++;
		Buffer->ResourceID = ResourceID;
		Wave->ResourceID = ResourceID;

		AudioDevice->Buffers.AddItem( Buffer );
		AudioDevice->WaveBufferMap.Set( ResourceID, Buffer );

		// Keep track of associated resource name.
		Buffer->ResourceName = Wave->GetPathName();		
		Buffer->NumChannels = Wave->NumChannels;
		Buffer->SampleRate = Wave->SampleRate;

		// get the raw data
		BYTE* FullSoundData = ( BYTE* )Wave->CompressedIPhoneData.Lock( LOCK_READ_ONLY );
		// it's (possibly) a pointer to a wave file, so skip over the header
		INT FullSoundDataSize = Wave->CompressedIPhoneData.GetBulkDataSize();

		checkf(Buffer->NumChannels <= 2, TEXT("Only mono and stereo sounds are supported in iOS"));
		UBOOL bIsStereo	= Buffer->NumChannels > 1 ? TRUE : FALSE;

		// prase the ADPCM wave header
		FWaveModInfo WaveInfo;
		WaveInfo.ReadWaveInfo(FullSoundData, FullSoundDataSize);

		Buffer->FormatTag = *WaveInfo.pFormatTag;

		// adjust the data start location and size after processing the header
		BYTE* SoundData     = WaveInfo.SampleDataStart;
		INT   SoundDataSize = WaveInfo.SampleDataSize;

		// just copy over ADPCM data in place
		Buffer->BufferSize = SoundDataSize;
		Buffer->Samples = (SWORD*)appMalloc(Buffer->BufferSize);
		appMemcpy(Buffer->Samples, SoundData, SoundDataSize);

		if (Buffer->FormatTag == FORMAT_ADPCM)
		{
			// the preamble contains 2 samples and each remaining byte contains 2 samples
			INT BlockSize = (*WaveInfo.pBlockAlign);
			INT PreambleSize = 7;
			INT SamplesPerBlock = 2/*preamble samples*/  + ((BlockSize - PreambleSize) * 2/*a sample is 4 bits*/);

			// remember how big one PCM block is
			Buffer->UncompressedBlockSize = SamplesPerBlock * sizeof(SWORD);
			Buffer->CompressedBlockSize	= BlockSize;

			check((Buffer->BufferSize % Buffer->CompressedBlockSize) == 0);
		}

		// unload it
		Wave->CompressedIPhoneData.Unlock();
	}

	return Buffer;
}

