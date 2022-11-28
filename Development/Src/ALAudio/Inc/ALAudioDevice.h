/*=============================================================================
	ALAudioDevice.h: Unreal OpenAL Audio interface object.
	Copyright 1999-2004 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Daniel Vogel.
	* Ported to Linux by Ryan C. Gordon.
=============================================================================*/

#ifndef _INC_ALAUDIODEVICE
#define _INC_ALAUDIODEVICE

/*------------------------------------------------------------------------------------
	Dependencies, helpers & forward declarations.
------------------------------------------------------------------------------------*/

class UALAudioDevice;

#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif
#include "altypes.h"
#include "alctypes.h"
typedef struct ALCdevice_struct ALCdevice;
typedef struct ALCcontext_struct ALCcontext;
#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

#define MAX_AUDIOCHANNELS 64

#if __WIN32__
#define AL_DLL TEXT("OpenAL32.dll")
#define AL_DEFAULT_DLL TEXT("DefOpenAL32.dll")
#else
#define AL_DLL TEXT("libopenal.so")
#define AL_DEFAULT_DLL TEXT("openal.so")
#endif

#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }


/*------------------------------------------------------------------------------------
	FALSoundBuffer
------------------------------------------------------------------------------------*/

class FALSoundBuffer
{
public:
	// Constructor/ Destructor.
	FALSoundBuffer( UALAudioDevice* AudioDevice );
	~FALSoundBuffer();

	// Buffer creation (static).
	static FALSoundBuffer* Init( USoundNodeWave* InWave, UALAudioDevice* AudioDevice );

	// Variables.
	UALAudioDevice*		AudioDevice;
	TArray<ALuint>		BufferIds;
	/** Resource ID of associated USoundNodeWave */
	INT					ResourceID;
};


/*------------------------------------------------------------------------------------
	FALSoundSource
------------------------------------------------------------------------------------*/

class FALSoundSource : public FSoundSource
{
public:
	// Constructor/ Destructor.
	FALSoundSource( UAudioDevice* InAudioDevice )
	:	FSoundSource( InAudioDevice ),
		Playing( 0 ),
		Paused( 0 ),
		Buffer( NULL )
	{}

	// Initialization & update.
	UBOOL Init( FWaveInstance* WaveInstance );
	void Update();

	// Playback.
	void Play();
	void Stop();
	void Pause();

	// Query.
	UBOOL IsFinished();

protected:
	// Variables.
	UBOOL				Playing,
						Paused;
	ALuint				SourceId;
	FALSoundBuffer*		Buffer;

	friend class UALAudioDevice;
};


/*------------------------------------------------------------------------------------
	UALAudioDevice
------------------------------------------------------------------------------------*/

class UALAudioDevice : public UAudioDevice
{
	DECLARE_CLASS(UALAudioDevice,UAudioDevice,CLASS_Config,ALAudio)

	// Constructor.
	void StaticConstructor();

	// UAudioDevice interface.
	UBOOL Init();
	void Flush();
	void Update( UBOOL Realtime );

	// UObject interface.
	void Destroy();
	void ShutdownAfterError();
	void SerializeHack(FArchive& Ar);

protected:
	// Dynamic binding.
	void FindProc( void*& ProcAddress, char* Name, char* SupportName, UBOOL& Supports, UBOOL AllowExt );
	void FindProcs( UBOOL AllowExt );
	UBOOL FindExt( const TCHAR* Name );

	// Error checking.
	UBOOL alError( TCHAR* Text, UBOOL Log = 1 );

	// Cleanup.
	void Teardown();


	// Variables.
	TArray<FALSoundBuffer*>						Buffers;
	/** Map from resource ID to sound buffer */
	TDynamicMap<INT,FALSoundBuffer*>			WaveBufferMap;
	DOUBLE										LastHWUpdate;
	/** Next resource ID value used for registering USoundNodeWave objects */
	INT											NextResourceID;

	// AL specific.
	ALCdevice*									SoundDevice;
	ALCcontext*									SoundContext;
	void*										DLLHandle;

	// Configuration.
	FLOAT										TimeBetweenHWUpdates;
	UBOOL										ReverseStereo;

	friend class FALSoundBuffer;
};

#define AUTO_INITIALIZE_REGISTRANTS_ALAUDIO	\
	UALAudioDevice::StaticClass();

#endif

