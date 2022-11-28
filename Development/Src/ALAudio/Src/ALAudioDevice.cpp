/*=============================================================================
	ALAudioDevice.cpp: Unreal OpenAL Audio interface object.
	Copyright 1999-2004 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Daniel Vogel.
	* Ported to Linux by Ryan C. Gordon.

	OpenAL is RHS
	Unreal is RHS with Y and Z swapped (or technically LHS with flipped axis)

=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "ALAudioPrivate.h"
#include "UnNet.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif

// OpenAL functions.
#define AL_EXT(name) static UBOOL SUPPORTS##name;
#define AL_PROC(ext,ret,func,parms) static ret (CDECL *func)parms;
#include "ALFuncs.h"
#define INITGUID 1
#undef DEFINE_GUID
#undef AL_EXT
#undef AL_PROC
#define OPENAL

#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

/*------------------------------------------------------------------------------------
	UALAudioDevice constructor and UObject interface.
------------------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UALAudioDevice);

//
// UALAudioDevice::StaticConstructor
//
void UALAudioDevice::StaticConstructor()
{
	new(GetClass(),TEXT("Channels"				), RF_Public) UIntProperty	(CPP_PROPERTY(MaxChannels			), TEXT("ALAudio"), CPF_Config );
	new(GetClass(),TEXT("TimeBetweenHWUpdates"	), RF_Public) UFloatProperty(CPP_PROPERTY(TimeBetweenHWUpdates	), TEXT("ALAudio"), CPF_Config );
}

//
//	UALAudioDevice::Flush
//
void UALAudioDevice::Flush()
{
	Super::Flush();

	for( INT i=0; i<Buffers.Num(); i++ )
		delete Buffers(i);

	Buffers.Empty();
}

//
//	UALAudioDevice::Teardown
//
void UALAudioDevice::Teardown()
{
	Flush();
	
	if( alcProcessContext )
		alcProcessContext( SoundContext );
	if( alcMakeContextCurrent )
		alcMakeContextCurrent( NULL );
	if( alcDestroyContext )
		alcDestroyContext( SoundContext );
	if( alcCloseDevice )
		alcCloseDevice( SoundDevice );

	appFreeDllHandle( DLLHandle );
}

//
//	UALAudioDevice::Destroy
//
void UALAudioDevice::Destroy()
{
	Teardown();
	debugf(NAME_Exit,TEXT("OpenAL Audio Device shut down."));

	Super::Destroy();
}

//
// UALAudioDevice::ShutdownAfterError
//
void UALAudioDevice::ShutdownAfterError()
{
	Teardown();
	debugf(NAME_Exit,TEXT("UALAudioDevice::ShutdownAfterError"));

	Super::ShutdownAfterError();
}

//
//	UALAudioDevice::SerializeHack
//
void UALAudioDevice::SerializeHack(FArchive& Ar)
{
	//Super::Serialize(Ar);
	
	//@warning: the compiler puts UAudioDevice::Serialize into UALAudioDevices vftable in release modes
	//@warning: so we use this lame hack to work around the issue
	//@todo: re- evaluate need for hack with VS.NET 2005

	// We currently don't have anything to serialize though I'm eager to leave this empty function body
	// as it took quite a while to track down a rare crash to the wrong serialize function being called
	// in release modes.
}

/*------------------------------------------------------------------------------------
	UAudioDevice Interface.
------------------------------------------------------------------------------------*/

//
// UALAudioDevice::Init
//
UBOOL UALAudioDevice::Init()
{
	// Find DLL's.
	DLLHandle = NULL;
	if( !DLLHandle )
	{
		DLLHandle = appGetDllHandle( AL_DEFAULT_DLL );
		if( !DLLHandle )
		{
			debugf( NAME_Init, TEXT("Couldn't locate %s - giving up."), AL_DEFAULT_DLL );
			return 0;
		}
	}
	
	// Find functions.
	SUPPORTS_AL = 1;
	FindProcs( 0 );
	if( !SUPPORTS_AL )
		return 0;

	char *Device = NULL;
	#if __WIN32__
		Device = "DirectSound";// "MMSYSTEM", "DirectSound3D", "DirectSound"
	#endif
	SoundDevice = alcOpenDevice( (unsigned char *)Device );
	if( !SoundDevice )
	{
		debugf(NAME_Init,TEXT("ALAudio: no OpenAL devices found."));
		return 0;
	}

	INT Caps[] = { ALC_FREQUENCY, 44100, 0 };
	SoundContext = alcCreateContext( SoundDevice, Caps );
	if( !SoundContext )
	{
		debugf(NAME_Init, TEXT("ALAudio: context creation failed."));
		return 0;
	}

	alcMakeContextCurrent( SoundContext );
	
	if( alError(TEXT("Init")) )
	{
		debugf(NAME_Init,TEXT("ALAudio: alcMakeContextCurrent failed."));
		return 0;
	}

	// No channels, no sound.
	if ( MaxChannels <= 8 )
		return 0;
	
	// Initialize channels.
	alError(TEXT("Emptying error stack"),0);
	for( INT i=0; i<Min(MaxChannels, MAX_AUDIOCHANNELS); i++ )
	{
		ALuint SourceId;
		alGenSources( 1, &SourceId );
		if ( !alError(TEXT("Init (creating sources)"), 0 ) )
		{
			FALSoundSource* Source = new FALSoundSource(this);
			Source->SourceId = SourceId;
			Sources.AddItem( Source );
			FreeSources.AddItem( Source );
		}
		else
			break;
	}
	if( !Sources.Num() )
	{
		debugf(NAME_Error,TEXT("ALAudio: couldn't allocate sources"));
		return 0;
	}

	// Use our own distance model.
	alDistanceModel( AL_NONE );
	
	// Initialiaze base class.
	Super::Init();

	// Initialized.
	LastRealtime		= 1;
	LastHWUpdate		= 0;
	NextResourceID		= 1;

	debugf(NAME_Init,TEXT("ALAudio: Device initialized."));
	return 1;
}

//
//	UALAudioDevice::Update
//
void UALAudioDevice::Update( UBOOL Realtime )
{
	Super::Update( Realtime );

	// Set Player position and orientation.
	FVector Orientation[2];

	// See file header for coordinate system explanation.
	Orientation[0].X	= Listener.Front.X;
	Orientation[0].Y	= Listener.Front.Z; // Z/Y swapped on purpose, see file header	
	Orientation[0].Z	= Listener.Front.Y; // Z/Y swapped on purpose, see file header
	
	// See file header for coordinate system explanation.
	Orientation[1].X	= Listener.Up.X;
	Orientation[1].Y	= Listener.Up.Z; // Z/Y swapped on purpose, see file header
	Orientation[1].Z	= Listener.Up.Y; // Z/Y swapped on purpose, see file header

	// Make the listener still and the sounds move relatively -- this allows 
	// us to scale the doppler effect on a per-sound basis.
	FVector Velocity	= FVector(0,0,0),
			Location	= Listener.Location;

	// See file header for coordinate system explanation.
	Location.X			= Listener.Location.X;
	Location.Y			= Listener.Location.Z; // Z/Y swapped on purpose, see file header
	Location.Z			= Listener.Location.Y; // Z/Y swapped on purpose, see file header
	Location		   *= AUDIO_DISTANCE_FACTOR;
	
	alListenerfv( AL_POSITION	, (ALfloat *) &Location			);
	alListenerfv( AL_ORIENTATION, (ALfloat *) &Orientation[0]	);
	alListenerfv( AL_VELOCITY	, (ALfloat *) &Velocity			);

	// Deferred commit (enforce min time between updates).
	if( GCurrentTime < LastHWUpdate )
		LastHWUpdate = GCurrentTime;
	if( (GCurrentTime - LastHWUpdate) >= (TimeBetweenHWUpdates / 1000.f) )
	{
		LastHWUpdate = GCurrentTime;
		alcProcessContext( SoundContext );
#ifdef WIN32
		alcSuspendContext( SoundContext );
#endif
	}
}

/*------------------------------------------------------------------------------------
	UALAudioDevice Internals.
------------------------------------------------------------------------------------*/

//
//	UALAudioDevice::FindExt
//
UBOOL UALAudioDevice::FindExt( const TCHAR* Name )
{
	UBOOL Result = appStrfind(ANSI_TO_TCHAR((char*)alGetString(AL_EXTENSIONS)),Name)!=NULL;
	if( Result )
		debugf( NAME_Init, TEXT("Device supports: %s"), Name );
	return Result;
}

//
//	UALAudioDevice::FindProc
//
void UALAudioDevice::FindProc( void*& ProcAddress, char* Name, char* SupportName, UBOOL& Supports, UBOOL AllowExt )
{
	ProcAddress = NULL;
	if( !ProcAddress )
		ProcAddress = appGetDllExport( DLLHandle, ANSI_TO_TCHAR(Name) );
	if( !ProcAddress && Supports && AllowExt )
		ProcAddress = alGetProcAddress( (ALubyte*) Name );
	if( !ProcAddress )
	{
		if( Supports )
			debugf( TEXT("   Missing function '%s' for '%s' support"), ANSI_TO_TCHAR(Name), ANSI_TO_TCHAR(SupportName) );
		Supports = 0;
	}
}

//
//	UALAudioDevice::FindProcs
//
void UALAudioDevice::FindProcs( UBOOL AllowExt )
{
	#define AL_EXT(name) if( AllowExt ) SUPPORTS##name = FindExt( TEXT(#name)+1 );
	#define AL_PROC(ext,ret,func,parms) FindProc( *(void**)&func, #func, #ext, SUPPORTS##ext, AllowExt );
	#include "ALFuncs.h"
	#undef AL_EXT
	#undef AL_PROC
}

//
//	UALAudioDevice::alError
//
UBOOL UALAudioDevice::alError( TCHAR* Text, UBOOL Log )
{
	ALenum Error = alGetError();
	if( Error != AL_NO_ERROR )
	{
		do 
		{		
			if ( Log )
			{
				switch ( Error )
				{
				case AL_INVALID_NAME:
					debugf(TEXT("ALAudio: AL_INVALID_NAME in %s"), Text);
					break;
				case AL_INVALID_VALUE:
					debugf(TEXT("ALAudio: AL_INVALID_VALUE in %s"), Text);
					break;
				case AL_OUT_OF_MEMORY:
					debugf(TEXT("ALAudio: AL_OUT_OF_MEMORY in %s"), Text);
					break;
				default:
					debugf(TEXT("ALAudio: Unknown error in %s"), Text);
#if __WIN32__  // !!! FIXME: This isn't in the Linux reference implemention? --ryan.
				case AL_INVALID_ENUM:
					debugf(TEXT("ALAudio: AL_INVALID_ENUM in %s"), Text);
					break;
				case AL_INVALID_OPERATION:
					debugf(TEXT("ALAudio: AL_INVALID_OPERATION in %s"), Text);
					break;
#endif
				}
			}
		}
		while( (Error = alGetError()) != AL_NO_ERROR );
		
		return true;
	}
	else
		return false;
}

/*------------------------------------------------------------------------------------
	FALSoundSource.
------------------------------------------------------------------------------------*/

//
//	FALSoundSource::Init
//
UBOOL FALSoundSource::Init( FWaveInstance* InWaveInstance )
{
	// Find matching buffer.
	Buffer = FALSoundBuffer::Init( InWaveInstance->WaveData, (UALAudioDevice*) AudioDevice );
	if( Buffer )
	{
		WaveInstance = InWaveInstance;

		alSourcei( SourceId, AL_SOURCE_RELATIVE	, WaveInstance->UseSpatialization	? AL_FALSE	: AL_TRUE	);

		alSourceQueueBuffers( SourceId, Buffer->BufferIds.Num(), &Buffer->BufferIds(0) );	
		if( WaveInstance->PotentiallyLooping )
		{		
			// We queue the sound twice for wave instances that can potentially loop so we can have smooth loop
			// transitions. The downside is that we might play at most one frame worth of audio from the 
			// beginning of the wave after the wave stops looping.
			alSourceQueueBuffers( SourceId, Buffer->BufferIds.Num(), &Buffer->BufferIds(0) );
		}
	
		Update();

		// Initialization was successful.
		return TRUE;
	}
	else
	{
		// Failed to initialize source.
		return FALSE;
	}
}

//
//	FALSoundSource::Update
//
void FALSoundSource::Update()
{
	if( !WaveInstance || Paused )
		return;

	FLOAT	Volume		= Clamp<FLOAT>( WaveInstance->Volume, 0.0f, 1.0f ),
			Pitch		= Clamp<FLOAT>( WaveInstance->Pitch , 0.5f, 2.0f );

	FVector Location,
			Velocity;

	// See file header for coordinate system explanation.
	Location.X			= WaveInstance->Location.X;
	Location.Y			= WaveInstance->Location.Z; // Z/Y swapped on purpose, see file header
	Location.Z			= WaveInstance->Location.Y; // Z/Y swapped on purpose, see file header
	
	Velocity.X			= WaveInstance->Velocity.X;
	Velocity.Y			= WaveInstance->Velocity.Z; // Z/Y swapped on purpose, see file header
	Velocity.Z			= WaveInstance->Velocity.Y; // Z/Y swapped on purpose, see file header

	// Convert to meters.
	Location *= AUDIO_DISTANCE_FACTOR;
	Velocity *= AUDIO_DISTANCE_FACTOR;

	// We're using a relative coordinate system for un- spatialized sounds.
	if( !WaveInstance->UseSpatialization )
		Location = FVector( 0.f, 0.f, 0.f );

	//if( Velocity.Size() > AUDIO_MAX_SOUND_SPEED ) 
	//	Velocity = FVector(0,0,0);

	alSourcef ( SourceId, AL_GAIN		, Volume					);	
	alSourcef ( SourceId, AL_PITCH		, Pitch						);		

	alSourcefv( SourceId, AL_POSITION	, (ALfloat *) &Location		);
	alSourcefv( SourceId, AL_VELOCITY	, (ALfloat *) &Velocity		);
}

//
//	FALSoundSource::Play
//
void FALSoundSource::Play()
{
	if( WaveInstance )
	{
		alSourcePlay( SourceId );
		Paused	= 0;
		Playing = 1;
	}
}

//
//	FALSoundSource::Stop
//
void FALSoundSource::Stop()
{
	if( WaveInstance )
	{	
		alSourceStop( SourceId );
		alSourcei( SourceId, AL_BUFFER, NULL );
		Paused		= 0;
		Playing		= 0;
		Buffer		= NULL;
	}

	FSoundSource::Stop();
}

//
//	UALSoundSource::Pause
//
void FALSoundSource::Pause()
{
	if( WaveInstance )
	{
		alSourcePause( SourceId );
		Paused = 1;
	}
}

//
//	FALSoundSource::IsFinished
//
UBOOL FALSoundSource::IsFinished()
{
	if( WaveInstance )
	{
		ALint State;
		ALint BuffersProcessed;
		alGetSourcei( SourceId, AL_SOURCE_STATE			, &State			);
		alGetSourcei( SourceId, AL_BUFFERS_PROCESSED	, &BuffersProcessed	);

		// Handle the case of the source being stopped or potentially looping sources having looped once.
		if( State == AL_STOPPED || BuffersProcessed >= Buffer->BufferIds.Num() )
		{
			if( WaveInstance->Finished() )
			{
				// Sound is finished and the high level audio code will stop this source.
				return 1;
			}
			else
			{
				// OpenAL requires a non NULL buffer argument so we just allocate a bit of throwaway data on the stack.
				ALuint* Buffers = (ALuint*) appAlloca( sizeof(ALuint) * Buffer->BufferIds.Num() );

				// Unqueue one full set worth of buffers.
				alSourceUnqueueBuffers( SourceId, Buffer->BufferIds.Num(), Buffers );

				// Source was stopped for some reason (breakpoint in the debugger, long frame, ...)
				if( State == AL_STOPPED )
				{
					// This is a looping source as Finished returned false which means we need to
					// buffer up the sound once and start playing it again.
					alSourceQueueBuffers( SourceId, Buffer->BufferIds.Num(), &Buffer->BufferIds(0) );
					alSourcePlay( SourceId );
				}

				// Queue another set of buffers to (potentially) loop it once more.
				alSourceQueueBuffers( SourceId, Buffer->BufferIds.Num(), &Buffer->BufferIds(0) );

				// We're not finished yet.
				return 0;
			}
		}
	}

	// We're not finished yet.
	return 0;
}

/*------------------------------------------------------------------------------------
	FALSoundBuffer.
------------------------------------------------------------------------------------*/

//
//	FALSoundBuffer::FALSoundBuffer
//
FALSoundBuffer::FALSoundBuffer( UALAudioDevice* InAudioDevice )
{
	AudioDevice	= InAudioDevice;
}

//
//	FALSoundBuffer::~FALSoundBuffer
//
FALSoundBuffer::~FALSoundBuffer()
{
	if( ResourceID )
	{
		AudioDevice->WaveBufferMap.Remove( ResourceID );

		// Delete AL buffers.
		for( INT i=0; i<BufferIds.Num(); i++ )	
			alDeleteBuffers( 1, &BufferIds(i) );
		BufferIds.Empty();
	}
}

//
//	FALSoundBuffer::Init
//
FALSoundBuffer* FALSoundBuffer::Init( USoundNodeWave* Wave, UALAudioDevice* AudioDevice )
{
	if( Wave == NULL )
		return NULL;

	FALSoundBuffer* Buffer = NULL;

	if( Wave->ResourceID )
	{
		Buffer = AudioDevice->WaveBufferMap.FindRef( Wave->ResourceID );
	}

	if( Buffer )
	{
		return Buffer;
	}
	else
	{
		FALSoundBuffer* Buffer = new FALSoundBuffer( AudioDevice );

		if( Wave->FileType == FName(TEXT("WAV")) )
		{
			// Load the data.
			Wave->RawData.Load();
			check(Wave->RawData.Num()>0);
	
			FWaveModInfo WaveInfo;
			if( !WaveInfo.ReadWaveInfo(Wave->RawData) )
			{
				debugf( NAME_Warning, TEXT("%s is not a WAVE file?"), *Wave->GetPathName() );
				Wave->RawData.Unload();
				delete Buffer;
				return NULL;
			}

			ALint Format;
			if( *WaveInfo.pBitsPerSample == 8 )
			{
				if (*WaveInfo.pChannels == 1)
					Format = AL_FORMAT_MONO8;
				else
					Format = AL_FORMAT_STEREO8;
			}
			else
			{
				if (*WaveInfo.pChannels == 1)
					Format = AL_FORMAT_MONO16;
				else
					Format = AL_FORMAT_STEREO16;				
			}

			ALuint BufferId;
			alGenBuffers( 1, &BufferId );
			AudioDevice->alError(TEXT("RegisterSound (generating buffer)"));
	
			alBufferData( BufferId, Format, WaveInfo.SampleDataStart, WaveInfo.SampleDataSize, *WaveInfo.pSamplesPerSec );

			// Unload the data.
			Wave->RawData.Unload();

			if( AudioDevice->alError(TEXT("RegisterSound (creating buffer)")) )
			{			
				alDeleteBuffers( 1, &BufferId );
				delete Buffer;
				return NULL;
			}

			// Allocate new resource ID and assign to USoundNodeWave. A value of 0 (default) means not yet registered.
			INT ResourceID		= AudioDevice->NextResourceID++;
			Buffer->ResourceID	= ResourceID;
			Wave->ResourceID	= ResourceID;

			Buffer->BufferIds.AddItem( BufferId );

			AudioDevice->Buffers.AddItem( Buffer );
			AudioDevice->WaveBufferMap.Set( ResourceID, Buffer );

			return Buffer;
		}
		else
		{
			appErrorf( TEXT("Format not currently supported.") );
			return NULL;
		}
	}
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

