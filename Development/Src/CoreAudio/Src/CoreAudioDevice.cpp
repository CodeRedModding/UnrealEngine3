/*=============================================================================
 	CoreAudioDevice.cpp: Unreal CoreAudio audio interface object.
 	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

/*------------------------------------------------------------------------------------
 Audio includes.
 ------------------------------------------------------------------------------------*/

#include "Engine.h"

#include "EngineSoundClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "UnAudioEffect.h"
#include "UnConsoleTools.h"
#include "UnAudioDecompress.h"
#include "CoreAudioDevice.h"
#include "CoreAudioEffects.h"

/*------------------------------------------------------------------------------------
 UCoreAudioDevice constructor and UObject interface.
 ------------------------------------------------------------------------------------*/

IMPLEMENT_CLASS( UCoreAudioDevice );

/**
 * Static constructor, used to associate .ini options with member variables.	
 */
void UCoreAudioDevice::StaticConstructor( void )
{
}

/**
 * Tears down audio device by stopping all sounds, removing all buffers, 
 * destroying all sources, ... Called by both Destroy and ShutdownAfterError
 * to perform the actual tear down.
 */
void UCoreAudioDevice::Teardown( void )
{
	// Flush stops all sources so sources can be safely deleted below.
	Flush( NULL );
	
	// Release any loaded buffers - this calls stop on any sources that need it
	for( INT i = Buffers.Num() - 1; i >= 0; i-- )
	{
		FCoreAudioSoundBuffer* Buffer = Buffers( i );
		FreeBufferResource( Buffer );
	}
	
	// Must be after FreeBufferResource as that potentially stops sources
	for( INT i = 0; i < Sources.Num(); i++ )
	{
		delete Sources( i );
	}
	
	// Clear out the EQ/Reverb/LPF effects
	delete Effects;
	
	Sources.Empty();
	FreeSources.Empty();
	
	if( AudioUnitGraph )
	{
		AUGraphStop( AudioUnitGraph );
		DisposeAUGraph( AudioUnitGraph );
		AudioUnitGraph = NULL;
		OutputNode = NULL;
		OutputUnit = NULL;
		MixerNode = NULL;
		MixerUnit = NULL;
	}
}

/**
 * Shuts down audio device. This will never be called with the memory image codepath.
 */
void UCoreAudioDevice::FinishDestroy( void )
{
	if( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		Teardown();
		debugf( NAME_Exit, TEXT( "CoreAudio Audio Device shut down." ) );
	}
	
	Super::FinishDestroy();
}

/**
 * Special variant of Destroy that gets called on fatal exit. 
 */
void UCoreAudioDevice::ShutdownAfterError( void )
{
	if( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		Teardown();
		debugf( NAME_Exit, TEXT( "UCoreAudioDevice::ShutdownAfterError" ) );
	}
	
	Super::ShutdownAfterError();
}

/*------------------------------------------------------------------------------------
 UAudioDevice Interface.
 ------------------------------------------------------------------------------------*/

/**
 * Initializes the audio device and creates sources.
 *
 * @return TRUE if initialization was successful, FALSE otherwise
 */
UBOOL UCoreAudioDevice::Init( void )
{
	// Make sure no interface classes contain any garbage
	Effects = NULL;
	
	// Default to sensible channel count.
	if( MaxChannels < 1 )
	{
		MaxChannels = 32;
	}
	
	// Make sure the output audio device exists
	AudioDeviceID HALDevice;
	UInt32 Size = sizeof( AudioDeviceID );
	AudioObjectPropertyAddress PropertyAddress;
	PropertyAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
	PropertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
	PropertyAddress.mElement = kAudioObjectPropertyElementMaster;

	OSStatus Status = AudioObjectGetPropertyData( kAudioObjectSystemObject, &PropertyAddress, 0, NULL, &Size, &HALDevice );
	if( Status != noErr )
	{
		debugf( NAME_Init, TEXT( "No audio devices found!" ) );
		return FALSE;
	}

	Status = NewAUGraph( &AudioUnitGraph );
	if( Status != noErr )
	{
		debugf( NAME_Init, TEXT( "Failed to create audio unit graph!" ) );
		return FALSE;
	}

	AudioComponentDescription Desc;
	Desc.componentFlags = 0;
	Desc.componentFlagsMask = 0;
	Desc.componentType = kAudioUnitType_Output;
	Desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	Desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	Status = AUGraphAddNode( AudioUnitGraph, &Desc, &OutputNode );
	if( Status == noErr )
	{
		Status = AUGraphOpen( AudioUnitGraph );
		if( Status == noErr )
		{
			Status = AUGraphNodeInfo( AudioUnitGraph, OutputNode, NULL, &OutputUnit );
			if( Status == noErr )
			{
				Status = AudioUnitInitialize( OutputUnit );
			}
		}
	}

	if( Status != noErr )
	{
		debugf( NAME_Init, TEXT( "Failed to initialize audio output unit!" ) );
		Teardown();
		return FALSE;
	}

	Desc.componentFlags = 0;
	Desc.componentFlagsMask = 0;
	Desc.componentType = kAudioUnitType_Mixer;
	Desc.componentSubType = kAudioUnitSubType_3DMixer;
	Desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	Status = AUGraphAddNode( AudioUnitGraph, &Desc, &MixerNode );
	if( Status == noErr )
	{
		Status = AUGraphNodeInfo( AudioUnitGraph, MixerNode, NULL, &MixerUnit );
		if( Status == noErr )
		{
			Status = AudioUnitInitialize( MixerUnit );
		}
	}
	
	if( Status != noErr )
	{
		debugf( NAME_Init, TEXT( "Failed to initialize audio 3D mixer unit!" ) );
		Teardown();
		return FALSE;
	}

	Size = sizeof( AudioStreamBasicDescription );
	Status = AudioUnitGetProperty( MixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &MixerFormat, &Size );
	if( Status == noErr )
	{
		Status = AUGraphConnectNodeInput( AudioUnitGraph, MixerNode, 0, OutputNode, 0 );
	}

	if( Status != noErr )
	{
		debugf( NAME_Init, TEXT( "Failed to start audio graph!" ) );
		Teardown();
		return FALSE;
	}

	// Set up the effects manager
	Effects = new FCoreAudioEffectsManager( this );

	Status = AUGraphInitialize( AudioUnitGraph );
	if( Status == noErr )
	{
		Status = AUGraphStart( AudioUnitGraph );
	}

	if( Status != noErr )
	{
		debugf( NAME_Init, TEXT( "Failed to start audio graph!" ) );
		Teardown();
		return FALSE;
	}

	// Initialize channels.
	for( INT i = 0; i < Min( MaxChannels, MAX_AUDIOCHANNELS ); i++ )
	{
		FCoreAudioSoundSource* Source = new FCoreAudioSoundSource( this, Effects );
		Sources.AddItem( Source );
		FreeSources.AddItem( Source );
	}

	if( !Sources.Num() )
	{
		debugf( NAME_Error, TEXT( "CoreAudioDevice: couldn't allocate sources" ) );
		return FALSE;
	}
	
	// Update MaxChannels in case we couldn't create enough sources.
	MaxChannels = Sources.Num();
	debugf( NAME_DevAudio, TEXT( "Allocated %i sources" ), MaxChannels );
	
	// Initialized.
	NextResourceID = 1;
	
	// Initialize base class last as it's going to precache already loaded audio.
	Super::Init();
	
	return TRUE;
}

/**
 * Update the audio device and calculates the cached inverse transform later
 * on used for spatialization.
 *
 * @param	Realtime	whether we are paused or not
 */
void UCoreAudioDevice::Update( UBOOL bGameTicking )
{
	Super::Update( bGameTicking );
	
	// Caches the matrix used to transform a sounds position into local space so we can just look
	// at the Y component after normalization to determine spatialization.
	InverseTransform = FMatrix( Listeners( 0 ).Up, Listeners( 0 ).Right, Listeners( 0 ).Up ^ Listeners( 0 ).Right, Listeners( 0 ).Location ).Inverse();
	
	// Print statistics for first non initial load allocation.
	static UBOOL bFirstTime = TRUE;
	if( bFirstTime && CommonAudioPoolSize != 0 )
	{
		bFirstTime = FALSE;
		if( CommonAudioPoolFreeBytes != 0 )
		{
			debugf( TEXT( "CoreAudio: Audio pool size mismatch by %d bytes. Please update CommonAudioPoolSize ini setting to %d to avoid waste!" ),
				   CommonAudioPoolFreeBytes, CommonAudioPoolSize - CommonAudioPoolFreeBytes );
		}
	}
}

/**
 * Precaches the passed in sound node wave object.
 *
 * @param	SoundNodeWave	Resource to be precached.
 */
void UCoreAudioDevice::Precache( USoundNodeWave* SoundNodeWave )
{
	FCoreAudioSoundBuffer::GetSoundFormat( SoundNodeWave, MinCompressedDurationGame );

	if( SoundNodeWave->VorbisDecompressor == NULL && SoundNodeWave->DecompressionType == DTYPE_Native )
	{
		// Grab the compressed vorbis data
		SoundNodeWave->InitAudioResource( SoundNodeWave->CompressedPCData );
		
		check(!SoundNodeWave->VorbisDecompressor); // should not have had a valid pointer at this point
		// Create a worker to decompress the vorbis data
		SoundNodeWave->VorbisDecompressor = new FAsyncVorbisDecompress( SoundNodeWave );
		SoundNodeWave->VorbisDecompressor->StartBackgroundTask();
	}
	else
	{
		// If it's not native, then it will remain compressed
		INC_DWORD_STAT_BY( STAT_AudioMemorySize, SoundNodeWave->GetResourceSize() );
		INC_DWORD_STAT_BY( STAT_AudioMemory, SoundNodeWave->GetResourceSize() );
	}
}

// sort memory usage from large to small unless bAlphaSort
static UBOOL bAlphaSort = FALSE;
IMPLEMENT_COMPARE_POINTER( FCoreAudioSoundBuffer, CoreAudioDevice, { return bAlphaSort ? appStricmp( *A->ResourceName,*B->ResourceName ) : ( A->GetSize() > B->GetSize() ) ? -1 : 1; } );

/**
 * This will return the name of the SoundClass of the SoundCue that this buffer(soundnodewave) belongs to.
 * NOTE: This will find the first cue in the ObjectIterator list.  So if we are using SoundNodeWaves in multiple
 * SoundCues we will pick up the first first one only.
 **/
static FName GetSoundClassNameFromBuffer( const FCoreAudioSoundBuffer* const Buffer )
{
	// for each buffer
	// look at all of the SoundCue's SoundNodeWaves to see if the ResourceID matched
	// if it does then grab the SoundClass of the SoundCue (that the waves werre gotten from)
	
	for( TObjectIterator<USoundCue> It; It; ++It )
	{
		USoundCue* Cue = *It;
		TArray<USoundNodeWave*> OutWaves;
		Cue->RecursiveFindNode<USoundNodeWave>( Cue->FirstNode, OutWaves );
		
		for( INT WaveIndex = 0; WaveIndex < OutWaves.Num(); WaveIndex++ )
		{
			USoundNodeWave* WaveNode = OutWaves(WaveIndex);
			if( WaveNode != NULL )
			{
				if( WaveNode->ResourceID == Buffer->ResourceID )
				{
					return Cue->SoundClass;
				}
			}
		}
	}
	
	return NAME_None;
}

/** 
 * Lists all the loaded sounds and their memory footprint
 */
void UCoreAudioDevice::ListSounds( const TCHAR* Cmd, FOutputDevice& Ar )
{
	bAlphaSort = ParseParam( Cmd, TEXT( "ALPHASORT" ) );
	
	INT	TotalResident = 0;
	INT	ResidentCount = 0;
	
	Ar.Logf( TEXT("Listing all sounds.") );
	Ar.Logf( TEXT( ", Size Kb, NumChannels, SoundName, bAllocationInPermanentPool, SoundClass" ) );
	
	TArray<FCoreAudioSoundBuffer*> AllSounds;
	for( INT BufferIndex = 0; BufferIndex < Buffers.Num(); BufferIndex++ )
	{
		AllSounds.AddItem( Buffers( BufferIndex ) );
	}
	
	Sort<USE_COMPARE_POINTER( FCoreAudioSoundBuffer, CoreAudioDevice )>( &AllSounds( 0 ), AllSounds.Num() );
	
	for( INT i = 0; i < AllSounds.Num(); ++i )
	{
		FCoreAudioSoundBuffer* Buffer = AllSounds( i );
		
		const FName SoundClassName = GetSoundClassNameFromBuffer( Buffer );
		
		Ar.Logf( TEXT( ", %8.2f, %d channel(s), %s, %d, %s" ), Buffer->GetSize() / 1024.0f, Buffer->NumChannels, *Buffer->ResourceName, Buffer->bAllocationInPermanentPool, *SoundClassName.ToString() );
		
		
		
		TotalResident += Buffer->GetSize();
		ResidentCount++;
	}
	
	Ar.Logf( TEXT( "%8.2f Kb for %d resident sounds" ), TotalResident / 1024.0f, ResidentCount );
}

/**
 * Frees the resources associated with this buffer
 *
 * @param	FCoreAudioSoundBuffer	Buffer to clean up
 */
void UCoreAudioDevice::FreeBufferResource( FCoreAudioSoundBuffer* Buffer )
{
	if( Buffer )
	{
		// Remove from buffers array.
		Buffers.RemoveItem( Buffer );
		
		// See if it is being used by a sound source...
		UBOOL bWasReferencedBySource = FALSE;
		for( INT SrcIndex = 0; SrcIndex < Sources.Num(); SrcIndex++ )
		{
			FCoreAudioSoundSource* Src = ( FCoreAudioSoundSource* )( Sources( SrcIndex ) );
			if( Src && Src->Buffer && ( Src->Buffer == Buffer ) )
			{
				// Make sure the buffer is no longer referenced by anything
				Src->Stop();
				break;
			}
		}
		
		// Delete it. This will automatically remove itself from the WaveBufferMap.
		delete Buffer;
	}
}

/**
 * Frees the bulk resource data associated with this SoundNodeWave.
 *
 * @param	SoundNodeWave	wave object to free associated bulk data
 */
void UCoreAudioDevice::FreeResource( USoundNodeWave* SoundNodeWave )
{
	// Find buffer for resident wavs
	if( SoundNodeWave->ResourceID )
	{
		// Find buffer associated with resource id.
		FCoreAudioSoundBuffer* Buffer = WaveBufferMap.FindRef( SoundNodeWave->ResourceID );
		FreeBufferResource( Buffer );
		
		SoundNodeWave->ResourceID = 0;
	}
	
	// Just in case the data was created but never uploaded
	if( SoundNodeWave->RawPCMData )
	{
		appFree( SoundNodeWave->RawPCMData );
		SoundNodeWave->RawPCMData = NULL;
	}
	
	// Remove the compressed copy of the data
	SoundNodeWave->RemoveAudioResource();
	
	// Stat housekeeping
	DEC_DWORD_STAT_BY( STAT_AudioMemorySize, SoundNodeWave->GetResourceSize() );
	DEC_DWORD_STAT_BY( STAT_AudioMemory, SoundNodeWave->GetResourceSize() );
}

/** 
 * Links up the resource data indices for looking up and cleaning up
 */
void UCoreAudioDevice::TrackResource( USoundNodeWave* Wave, FCoreAudioSoundBuffer* Buffer )
{
	// Allocate new resource ID and assign to USoundNodeWave. A value of 0 (default) means not yet registered.
	INT ResourceID = NextResourceID++;
	Buffer->ResourceID = ResourceID;
	Wave->ResourceID = ResourceID;
	
	Buffers.AddItem( Buffer );
	WaveBufferMap.Set( ResourceID, Buffer );
}

/*------------------------------------------------------------------------------------
 Static linking helpers.
 ------------------------------------------------------------------------------------*/

/**
 * Initialize registrants, basically calling StaticClass() to create the class and also 
 * populating the lookup table.
 *
 * @param	Lookup	current index into lookup table
 */
void AutoInitializeRegistrantsCoreAudio( INT& Lookup )
{
	UCoreAudioDevice::StaticClass();
}

/**
 * Auto generates names.
 */
void AutoRegisterNamesCoreAudio( void )
{
}

// end

