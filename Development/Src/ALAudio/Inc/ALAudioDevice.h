/*=============================================================================
	ALAudioDevice.h: Unreal OpenAL audio interface object.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_ALAUDIODEVICE
#define _INC_ALAUDIODEVICE

/*------------------------------------------------------------------------------------
	Dependencies, helpers & forward declarations.
------------------------------------------------------------------------------------*/

class UALAudioDevice;

#if IPHONE || PLATFORM_MACOSX

#include <OpenAL/al.h>
#include <OpenAL/alc.h>

#elif _WINDOWS

#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif
#define AL_NO_PROTOTYPES 1
#define ALC_NO_PROTOTYPES 1
#include "al.h"
#include "alc.h"
#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif
#define AL_DLL TEXT("OpenAL32.dll")
#else

#error Your platform defines here

#if PLATFORM_UNIX
#define AL_DLL TEXT("libopenal.so")
#endif

#endif

extern FLOAT GALGlobalVolumeMultiplier;

/**
 * OpenAL implementation of FSoundBuffer, containing the wave data and format information.
 */
class FALSoundBuffer
{
public:
	/** 
	 * Constructor
	 *
	 * @param AudioDevice	audio device this sound buffer is going to be attached to.
	 */
	FALSoundBuffer( UALAudioDevice* AudioDevice );
	
	/**
	 * Destructor 
	 * 
	 * Frees wave data and detaches itself from audio device.
 	 */
	~FALSoundBuffer( void );

	/**
	 * Static function used to create a buffer.
	 *
	 * @param	InWave				USoundNodeWave to use as template and wave source
	 * @param	AudioDevice			Audio device to attach created buffer to
	 * @return	FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FALSoundBuffer* Init( USoundNodeWave* InWave, UALAudioDevice* AudioDevice );

	/**
	 * Returns the size of this buffer in bytes.
	 *
	 * @return Size in bytes
	 */
	INT GetSize( void ) 
	{ 
		return( BufferSize ); 
	}

	/** 
	 * Returns the number of channels for this buffer
	 */
	INT GetNumChannels( void ) 
	{ 
		return( NumChannels ); 
	}
		
	/** Audio device this buffer is attached to */
	UALAudioDevice*			AudioDevice;
	/** Array of buffer ids used to reference the data stored in AL. */
	ALuint					BufferIds[2];
	/** Resource ID of associated USoundNodeWave */
	INT						ResourceID;
	/** Human readable name of resource, most likely name of UObject associated during caching. */
	FString					ResourceName;
	/** Format of the data internal to OpenAL */
	ALuint					InternalFormat;

	/** Number of bytes stored in OpenAL, or the size of the ogg vorbis data */
	INT						BufferSize;
	/** The number of channels in this sound buffer - should be directly related to InternalFormat */
	INT						NumChannels;
	/** Sample rate of the ogg vorbis data - typically 44100 or 22050 */
	INT						SampleRate;
};

/**
 * OpenAL implementation of FSoundSource, the interface used to play, stop and update sources
 */
class FALSoundSource : public FSoundSource
{
public:
	/**
	 * Constructor
	 *
	 * @param	InAudioDevice	audio device this source is attached to
	 */
	FALSoundSource( UAudioDevice* InAudioDevice )
	:	FSoundSource( InAudioDevice ),
		SourceId( 0 ),
		Buffer( NULL )
	{}

	/** 
	 * Destructor
	 */
	~FALSoundSource( void );

	/**
	 * Initializes a source with a given wave instance and prepares it for playback.
	 *
	 * @param	WaveInstance	wave instance being primed for playback
	 * @return	TRUE			if initialization was successful, FALSE otherwise
	 */
	virtual UBOOL Init( FWaveInstance* WaveInstance );

	/**
	 * Updates the source specific parameter like e.g. volume and pitch based on the associated
	 * wave instance.	
	 */
	virtual void Update( void );

	/**
	 * Plays the current wave instance.	
	 */
	virtual void Play( void );

	/**
	 * Stops the current wave instance and detaches it from the source.	
	 */
	virtual void Stop( void );

	/**
	 * Pauses playback of current wave instance.
	 */
	virtual void Pause( void );

	/**
	 * Queries the status of the currently associated wave instance.
	 *
	 * @return	TRUE if the wave instance/ source has finished playback and FALSE if it is 
	 *			currently playing or paused.
	 */
	virtual UBOOL IsFinished( void );

	/** 
	 * Access function for the source id
	 */
	ALuint GetSourceId( void ) const { return( SourceId ); }

	/** 
	 * Returns TRUE if an OpenAL source has finished playing
	 */
	UBOOL IsSourceFinished( void );

	/** 
	 * Handle dequeuing and requeuing of a single buffer
	 */
	void HandleQueuedBuffer( void );

protected:
	/** OpenAL source voice associated with this source/ channel. */
	ALuint				SourceId;
	/** Cached sound buffer associated with currently bound wave instance. */
	FALSoundBuffer*		Buffer;

	friend class UALAudioDevice;
};

/**
 * OpenAL implementation of an Unreal audio device.
 */
class UALAudioDevice : public UAudioDevice
{
	DECLARE_CLASS_INTRINSIC( UALAudioDevice, UAudioDevice, CLASS_Config | 0, ALAudio )

	/**
	 * Static constructor, used to associate .ini options with member variables.	
	 */
	void StaticConstructor( void );

#if IPHONE
	/**
	 * Initialize OpenAL early and in a thread to not delay startup by .66 seconds or so (on iPhone)
	 */
	static void IPhoneThreadedStaticInit();

	static ALCcontext* ALAudioContext;
	static INT SuspendCounter;

	static void ResumeContext();
	static void SuspendContext();
#endif

	/**
	 * Initializes the audio device and creates sources.
	 *
	 * @return TRUE if initialization was successful, FALSE otherwise
	 */
	virtual UBOOL Init( void );

	/**
	 * Update the audio device and calculates the cached inverse transform later
	 * on used for spatialization.
	 *
	 * @param	Realtime	whether we are paused or not
	 */
	virtual void Update( UBOOL bGameTicking );

	/**
	 * Precaches the passed in sound node wave object.
	 *
	 * @param	SoundNodeWave	Resource to be precached.
	 */
	virtual void Precache( USoundNodeWave* SoundNodeWave );

	/** 
	 * Lists all the loaded sounds and their memory footprint
	 */
	virtual void ListSounds( const TCHAR* Cmd, FOutputDevice& Ar );

	/**
	 * Frees the bulk resource data associated with this SoundNodeWave.
	 *
	 * @param	SoundNodeWave	wave object to free associated bulk data
	 */
	virtual void FreeResource( USoundNodeWave* SoundNodeWave );

	// UObject interface.
	virtual void Serialize( FArchive& Ar );

	/**
	 * Shuts down audio device. This will never be called with the memory image codepath.
	 */
	virtual void FinishDestroy( void );
	
	/**
	 * Special variant of Destroy that gets called on fatal exit. 
	 */
	virtual void ShutdownAfterError( void );

	void FindProcs( UBOOL AllowExt );

	// Error checking.
	UBOOL alError( const TCHAR* Text, UBOOL Log = 1 );

protected:
	/** Returns the enum for the internal format for playing a sound with this number of channels. */
	ALuint GetInternalFormat( INT NumChannels );

	// Dynamic binding.
	void FindProc( void*& ProcAddress, char* Name, char* SupportName, UBOOL& Supports, UBOOL AllowExt );
	UBOOL FindExt( const TCHAR* Name );

	/**
	 * Tears down audio device by stopping all sounds, removing all buffers,  destroying all sources, ... Called by both Destroy and ShutdownAfterError
	 * to perform the actual tear down.
	 */
	void Teardown( void );

	// Variables.

	/** The name of the OpenAL Device to open - defaults to "Generic Software" */
	FStringNoInit								DeviceName;
	/** All loaded resident buffers */
	TArray<FALSoundBuffer*>						Buffers;
	/** Map from resource ID to sound buffer */
	TMap<INT, FALSoundBuffer*>					WaveBufferMap;
	/** Next resource ID value used for registering USoundNodeWave objects */
	INT											NextResourceID;

	// AL specific

	/** Device/context used to play back sounds (static so it can be initialized early) */
	static ALCdevice*							HardwareDevice;
	static ALCcontext*							SoundContext;
	void*										DLLHandle;
	/** Formats for multichannel sounds */
	ALenum										Surround40Format;
	ALenum										Surround51Format;
	ALenum										Surround61Format;
	ALenum										Surround71Format;

	friend class FALSoundBuffer;
};

#endif