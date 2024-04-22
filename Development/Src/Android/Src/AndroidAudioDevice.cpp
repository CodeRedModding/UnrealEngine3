/////////////////////////////////////////////////////////////////////////////////
// Android Audio Device
/////////////////////////////////////////////////////////////////////////////////

#include "Core.h"
#include "Engine.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineSoundClasses.h"
#include "UnAudioEffect.h"

#include "AndroidAudioDevice.h"

#include <jni.h>
#include <pthread.h>
#include <semaphore.h>
#include <android/log.h>

// some global java native interface objects
extern jobject			GJavaGlobalThiz;
extern pthread_key_t	GJavaJNIEnvKey;

// java callback functions used only for sound
static jmethodID		GLoadSoundFile;
static jmethodID		GUnloadSoundFile;
static jmethodID		GPlaySound;
static jmethodID		GStopSound;
static jmethodID		GSetVolume;



////////////////////////////////////
/// Class: 
///    class FAndroidAudioTrackWrapper
///    
/// Description: 
///    
///    
////////////////////////////////////
class FAndroidActiveSoundPoolElementWrapper
{
public:
	FAndroidActiveSoundPoolElementWrapper( INT InSoundID, FLOAT InDuration, UBOOL bInIsLooping )
	{ 
		SoundID		= InSoundID;			
		Duration	= InDuration;
		bIsLooping	= bInIsLooping;
		bIsPlaying	= FALSE;
	}	

	~FAndroidActiveSoundPoolElementWrapper()
	{
	}
		
	////////////////////////////////////
	/// Function: 
	///    Play
	///    
	/// Specifiers: 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	void Play()
	{		
		if( bIsPlaying )
		{
			return;
		}

		bIsPlaying = TRUE;
		StartTime = appSeconds();	

		
		//
		JNIEnv* jniEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
		StreamID = jniEnv->CallIntMethod(GJavaGlobalThiz, GPlaySound, SoundID, bIsLooping );		
	}
	
	////////////////////////////////////
	/// Function: 
	///    Stop
	///    
	/// Specifiers: 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	void Stop()
	{
		bIsPlaying = FALSE;
		
		JNIEnv* jniEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
		jniEnv->CallVoidMethod(GJavaGlobalThiz, GStopSound, StreamID);
	}
	
	
	////////////////////////////////////
	/// Function: 
	///    SetVolume
	///    
	/// Specifiers: 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	void SetVolume( FLOAT volume )
	{
		JNIEnv* jniEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
		jniEnv->CallVoidMethod(GJavaGlobalThiz, GSetVolume, StreamID, volume);		
	}

	////////////////////////////////////
	/// Function: 
	///    IsFinished
	///    
	/// Specifiers: 
	///    [UBOOL] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	UBOOL IsFinished()
	{
		if( bIsPlaying && !bIsLooping )
		{
			return ( appSeconds() - StartTime > Duration );
		}
		else if( bIsPlaying && bIsLooping )
		{
			return FALSE;
		}
		else
		{
			return TRUE;
		}
	}

	//
	DOUBLE	StartTime;
	UBOOL	bIsLooping;
	UBOOL	bIsPlaying;

	//
	FLOAT	Duration;
	// the loaded id
	INT		SoundID;
	// the playing ID
	INT		StreamID;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FAndroidSoundBuffer
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FAndroidSoundBuffer
{
public:
	FAndroidSoundBuffer( UAndroidAudioDevice* InAudioDevice )
	{
		AudioDevice				= InAudioDevice;
		AndroidResourceID		= -1;
		UE3ResourceID			= -1;
		NumChannels				= 0;
		SampleRate				= 0;	
	}

	~FAndroidSoundBuffer( void )
	{
		// never cleaned up
#if !NEVER_DESTROY_ANDROID_SOUND_BUFFER
		if( UE3ResourceID >= 0 )
		{
			AudioDevice->WaveBufferMap.Remove( UE3ResourceID );			
		}		

		if( AndroidResourceID >= 0 )
		{
			//REMOVE FROM SOUND POOL
			JNIEnv* jniEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
			jniEnv->CallVoidMethod( GJavaGlobalThiz, GUnLoadSoundFile, AndroidResourceID );		
		}
#endif
	}


	static FAndroidSoundBuffer* Init( USoundNodeWave* InWave, UAndroidAudioDevice* InAudioDevice )
	{
		SCOPE_CYCLE_COUNTER( STAT_AudioResourceCreationTime );

		// Can't create a buffer without any source data
		if( InWave == NULL || InWave->NumChannels == 0 )
		{
			return( NULL );
		}

		FAndroidSoundBuffer* Buffer = NULL;

		// Find the existing buffer if any
		if( InWave->ResourceID )
		{
#if NEVER_DESTROY_ANDROID_SOUND_BUFFER
			Buffer = InAudioDevice->WaveBufferMap.FindRef( InWave->GetPathName() );
#else
			Buffer = InAudioDevice->WaveBufferMap.FindRef( InWave->ResourceID );
#endif
		}

		if( Buffer == NULL )
		{
			// Create new buffer.
			Buffer = new FAndroidSoundBuffer( InAudioDevice );

			// Allocate new resource ID and assign to USoundNodeWave. A value of 0 (default) means not yet registered.
			INT ResourceID			= InAudioDevice->NextResourceID++;
			Buffer->UE3ResourceID	= ResourceID;
			InWave->ResourceID		= ResourceID;

			InAudioDevice->Buffers.AddItem( Buffer );

#if NEVER_DESTROY_ANDROID_SOUND_BUFFER
			InAudioDevice->WaveBufferMap.Set( InWave->GetPathName(), Buffer );
#else
			InAudioDevice->WaveBufferMap.Set( ResourceID, Buffer );
#endif

			// Keep track of associated resource name.
			Buffer->ResourceName	= InWave->GetPathName();
			Buffer->NumChannels		= InWave->NumChannels;
			Buffer->SampleRate		= InWave->SampleRate;

			//LOAD INTO SOUND POOL
			JNIEnv* jniEnv				= (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
			jstring PathNameJAVA		= jniEnv->NewStringUTF( *InWave->GetPathName() );
			Buffer->AndroidResourceID	= jniEnv->CallIntMethod( GJavaGlobalThiz, GLoadSoundFile, PathNameJAVA );		
			jniEnv->DeleteLocalRef( PathNameJAVA );
		}

		return Buffer;
	}

	INT GetNumChannels() const { return( NumChannels ); }	
	INT GetAndroidResourceID() const { return( AndroidResourceID ); }	
		
	/** Audio device this buffer is attached to */
	UAndroidAudioDevice*	AudioDevice;
	// ids
	INT						AndroidResourceID;
	INT						UE3ResourceID;
	// resource name back up
	FString					ResourceName;
	// info
	INT						NumChannels;
	INT						SampleRate;	
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FAndroidSoundSource
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FAndroidSoundSource::FAndroidSoundSource( UAudioDevice* InAudioDevice )
	:	FSoundSource( InAudioDevice ),		
		Buffer( NULL ),
		bUsingMonoSource( TRUE ),
		ReadPosition(0),
		bLooping(FALSE),
		ActiveAudioTrack(NULL)
{	
}

FAndroidSoundSource::~FAndroidSoundSource( void )
{	
}


////////////////////////////////////
/// Function: 
///    FAndroidSoundSource::Init
///    
/// Specifiers: 
///    [UBOOL] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
UBOOL FAndroidSoundSource::Init( FWaveInstance* InWaveInstance )
{
	// don't do anything if no volume! THIS APPEARS TO HAVE THE VOLUME IN TIME, CHECK HERE THOUGH IF ISSUES
	if( InWaveInstance && ( InWaveInstance->Volume * InWaveInstance->VolumeMultiplier ) <= 0 )
	{
		return FALSE;
	}

	// Find matching buffer.
	Buffer = FAndroidSoundBuffer::Init( InWaveInstance->WaveData, ( UAndroidAudioDevice * )AudioDevice );
	
	// make sure its a usuable buffer for us
	if( Buffer && Buffer->GetAndroidResourceID() >= 0 && Buffer->NumChannels <= 2 )
	{	
		// set it to the instance
		WaveInstance = InWaveInstance;	
		// make sure we know if we are looping
		bLooping = ( InWaveInstance->LoopingMode == LOOP_Forever );
		// reset streaming position
		ReadPosition = 0;
		// we using mono?
		bUsingMonoSource = ( Buffer->NumChannels == 1 );		
		// Initialization was successful.

		// make sure stop was ALWAYS CALLED first
		check( !ActiveAudioTrack );
		// we aren't streaming it in, use per buffer tracks
		ActiveAudioTrack = new FAndroidActiveSoundPoolElementWrapper( Buffer->GetAndroidResourceID(), InWaveInstance->WaveData->Duration, bLooping );		
		return TRUE;
	}

	// Failed to initialize source.
	return FALSE;
}


////////////////////////////////////
/// Function: 
///    FAndroidSoundSource::Update
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void FAndroidSoundSource::Update( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioUpdateSources );

	if( !WaveInstance || Paused )
	{
		return;
	}

	FLOAT Volume = GetVolume();
	
	if( Buffer )
	{
		ActiveAudioTrack->SetVolume( Volume );
	}	
}


////////////////////////////////////
/// Function: 
///    FAndroidSoundSource::GetVolume
///    
/// Specifiers: 
///    [FLOAT] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
FLOAT FAndroidSoundSource::GetVolume()
{
	if( WaveInstance && Buffer )
	{
		FLOAT Volume = WaveInstance->Volume * WaveInstance->VolumeMultiplier;
		if( SetStereoBleed() )
		{
			// Emulate the bleed to rear speakers followed by stereo fold down
			Volume *= 1.25f;
		}

		// clamp it to a resonable value
		Volume = Clamp<FLOAT>( Volume, 0.0f, 1.0f );

		return Volume;
	}

	return 1.0f;
}

////////////////////////////////////
/// Function: 
///    FAndroidSoundSource::Play
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void FAndroidSoundSource::Play( void )
{
	if( WaveInstance && Buffer )
	{
		// don't play if volume is 0
		// VOLUMEHACK
		FLOAT Volume = GetVolume();

		if( Volume > 0 )
		//ENDVOLUMEHACK
		{
			// set it to be playing
			ActiveAudioTrack->Play();
			// go ahead and push some data
			Update();
			// defaults 
			Paused = FALSE;
			Playing = TRUE;
		}
	}
}


////////////////////////////////////
/// Function: 
///    FAndroidSoundSource::Stop
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void FAndroidSoundSource::Stop( void )
{	
	if( WaveInstance && Buffer )
	{		
		ActiveAudioTrack->Stop();

		Paused				= FALSE;
		Playing				= FALSE;
		Buffer				= NULL;

		delete ActiveAudioTrack;
		ActiveAudioTrack	= NULL;
	}

	FSoundSource::Stop();
}


////////////////////////////////////
/// Function: 
///    FAndroidSoundSource::Pause
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void FAndroidSoundSource::Pause( void )
{
	// nothing for now
}


////////////////////////////////////
/// Function: 
///    FAndroidSoundSource::IsFinished
///    
/// Specifiers: 
///    [UBOOL] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
UBOOL FAndroidSoundSource::IsFinished( void )
{
	if( WaveInstance && Buffer )
	{
		// Check for a non starved, stopped source
		if( ActiveAudioTrack->IsFinished() )
		{
			// Notify the wave instance that it has finished playing.
			WaveInstance->NotifyFinished();
			return TRUE;
		}		

		return FALSE;
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UAndroidAudioDevice
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_CLASS( UAndroidAudioDevice );


////////////////////////////////////
/// Function: 
///    AudioDeviceJavaInit
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [JNIEnv *InJNIEnv] - 
///    [jclass &InAppClass] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void AudioDeviceJavaInit( JNIEnv *InJNIEnv, jclass &InAppClass )
{
	debugf(TEXT("Setting up Java audio methods..."));

	// get the sound track creation and destruction methods
	GLoadSoundFile		= InJNIEnv->GetMethodID( InAppClass, "JavaCallback_LoadSoundFile",	"(Ljava/lang/String;)I");	
    GUnloadSoundFile	= InJNIEnv->GetMethodID( InAppClass, "JavaCallback_UnloadSoundID",	"(I)V");

	// sound track use
	GPlaySound			= InJNIEnv->GetMethodID( InAppClass, "JavaCallback_PlaySound",		"(IZ)I");
	GStopSound			= InJNIEnv->GetMethodID( InAppClass, "JavaCallback_StopSound",		"(I)V");			
	GSetVolume			= InJNIEnv->GetMethodID( InAppClass, "JavaCallback_SetVolume",		"(IF)V");

	check(GLoadSoundFile && GUnloadSoundFile && GPlaySound && GStopSound && GSetVolume);

	debugf(TEXT("... Completed setting up Java audio methods"));
}


////////////////////////////////////
/// Function: 
///    UAndroidAudioDevice::Init
///    
/// Specifiers: 
///    [UBOOL] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
UBOOL UAndroidAudioDevice::Init( void )
{
	// 10 hardcoded tracks for now
	for( INT i = 0; i < 6; i++ )
	{		
		FAndroidSoundSource* Source = new FAndroidSoundSource( this );
		Sources.AddItem( Source );
		FreeSources.AddItem( Source );
	}

	// Update MaxChannels in case we couldn't create enough sources.
	MaxChannels = Sources.Num();

	// Set up a default (nop) effects manager 
	Effects = new FAudioEffectsManager( this );

	// Initialize base class last as it's going to precache already loaded audio.
	Super::Init();

	return TRUE;
}



////////////////////////////////////
/// Function: 
///    UAndroidAudioDevice::Precache
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void UAndroidAudioDevice::Precache( USoundNodeWave* Wave )
{
#if !NO_ANDROIDAUDIO_PRECACHING
	FAndroidSoundBuffer::Init( Wave, this );
#endif
}

////////////////////////////////////
/// Function: 
///    UAndroidAudioDevice::FreeResource
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void UAndroidAudioDevice::FreeResource( USoundNodeWave* SoundNodeWave )
{
	// Just in case the data was created but never uploaded
	if( SoundNodeWave->RawPCMData )
	{
		appFree( SoundNodeWave->RawPCMData );
		SoundNodeWave->RawPCMData = NULL;
	}

	// Find buffer for resident wavs
	if( SoundNodeWave->ResourceID )
	{
		// Find buffer associated with resource id.
#if NEVER_DESTROY_ANDROID_SOUND_BUFFER
		FAndroidSoundBuffer* Buffer = WaveBufferMap.FindRef( SoundNodeWave->GetPathName() );
#else
		FAndroidSoundBuffer* Buffer = WaveBufferMap.FindRef( SoundNodeWave->ResourceID );
#endif

		if( Buffer )
		{
			// See if it is being used by a sound source...
			for( INT SrcIndex = 0; SrcIndex < Sources.Num(); SrcIndex++ )
			{
				FAndroidSoundSource* Src = ( FAndroidSoundSource* )( Sources( SrcIndex ) );
				if( Src && Src->Buffer && ( Src->Buffer == Buffer ) )
				{
					Src->Stop();
					break;
				}
			}

#if !NEVER_DESTROY_ANDROID_SOUND_BUFFER
			// Remove from buffers array.
			Buffers.RemoveItem( Buffer );
			delete Buffer;
#endif
		}

		SoundNodeWave->ResourceID = 0;
	}

	// .. or reference to compressed data
	SoundNodeWave->RemoveAudioResource();
}