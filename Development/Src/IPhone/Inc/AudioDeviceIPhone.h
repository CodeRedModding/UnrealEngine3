/*=============================================================================
	UnAsyncLoadingIPhone.h: Interface for CodeAudio sound playback
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_COREAUDIODEVICE
#define _INC_COREAUDIODEVICE

#include "EngineAudioDeviceClasses.h"
#include "UnAudioEffect.h"
#import <AudioToolbox/AudioToolbox.h>
#import <AVFoundation/AVFoundation.h>

class UAudioDeviceIPhone;

class FSoundBufferIPhone
{
public:
	FSoundBufferIPhone( UAudioDeviceIPhone* AudioDevice );
	~FSoundBufferIPhone( void );
	
	static FSoundBufferIPhone* Init( USoundNodeWave* InWave, UAudioDeviceIPhone* AudioDevice );
	
	/** Unique identifier for this buffer */
	INT						ResourceID;
	
	/** UE3 object resource name */
	FString					ResourceName;
	
	/** The number of channels in this sound buffer */
	INT						NumChannels;

	/** Sample rate of the ogg vorbis data - typically 44100 or 22050 */
	INT						SampleRate;

	/** Size of the Samples array (in bytes!) */
	UINT					BufferSize;

	/** Audio data */
	SWORD*					Samples;

	/** How big is one block? */
	UINT					CompressedBlockSize;
	UINT					UncompressedBlockSize;

	/**The format of this buffer*/
	WORD					FormatTag;
};

class FSoundSourceIPhone : public FSoundSource
{
public:
	/**
	 * Constructor
	 */
	FSoundSourceIPhone(UAudioDeviceIPhone* InAudioDevice, UINT InBusNumber);
	~FSoundSourceIPhone(void);

	virtual UBOOL	Init( FWaveInstance* WaveInstance );	
	virtual void	Update( void );
	virtual void	Play( void );
	virtual void	Stop( void );
	virtual void	Pause( void );
	virtual UBOOL	IsFinished( void );

	/** Is CoreAudio done playing the sound? */
	WORD					bIsFinished[2];

	/**Int for compare and swap atomic operation to synchronize  
	*the decompression thread and the release of the resource on the 
	*main thread
	*/
	int32_t 				SourceLock[2];

	/** The buffer currently using this sound source */
	FSoundBufferIPhone*		Buffer;

	/** TRUE if the wave instance wants to loop */
	UBOOL					bIsLooping;

	/** Which bus this sound is currently playing on */
	UINT					BusNumber;

	/** Offset into the buffer's samples */
	UINT					SampleOffset[2];

	/** Current playback setting values */
	AudioUnitParameterValue	Pan;
	AudioUnitParameterValue	Volume;
	AudioUnitParameterValue	Pitch;

	/** For streaming decompression of ADPCM, this holds a blocks worth of decoded PCM data */
	SWORD*					StreamingBuffer[2];

	/** The size of StreamingBuffer, in case it needs to grow */
	UINT					StreamingBufferSize;

	/** The offset into the buffer */
	UINT					StreamingBufferOffset[2];
};

class UAudioDeviceIPhone : public UAudioDevice
{
	DECLARE_CLASS_INTRINSIC(UAudioDeviceIPhone, UAudioDevice, CLASS_Config | 0, IPhoneDrv)

	/**
	 * Initialize OpenAL early and in a thread to not delay startup by .66 seconds or so (on iPhone)
	 */
	static void ThreadedStaticInit();

	static void ResumeContext();
	static void SuspendContext();

	// UAudioDevice interface.

	/**
	 * Initializes the audio device and creates sources.
	 *
	 * @return TRUE if initialization was successful, FALSE otherwise
	 */
	virtual UBOOL	Init( void );

	/**
	 * Update the audio device
	 *
	 * @param	Realtime	whether we are paused or not
	 */
	virtual void	Update( UBOOL bGameTicking );

	/**
	 * Precaches the passed in sound node wave object.
	 *
	 * @param	SoundNodeWave	Resource to be precached.
	 */
	virtual void	Precache( USoundNodeWave* SoundNodeWave );	

	/**
	 * Frees the bulk resource data associated with this SoundNodeWave.
	 *
	 * @param	SoundNodeWave	wave object to free associated bulk data
	 */
	virtual void	FreeResource( USoundNodeWave* SoundNodeWave );

	// UObject interface.
	virtual void	Serialize( FArchive& Ar );

	/**
	 * Shuts down audio device. This will never be called with the memory image codepath.
	 */
	virtual void	FinishDestroy( void );	

	/**
	 * Special variant of Destroy that gets called on fatal exit. Doesn't really
	 * matter on the console so for now is just the same as Destroy so we can
	 * verify that the code correctly cleans up everything.
	 */
	virtual void	ShutdownAfterError( void );

	/** 
	* Lists all the loaded sounds and their memory footprint
	*/
	virtual void ListSounds( const TCHAR* Cmd, FOutputDevice& Ar );

protected:
	/** Cleanup. */
	void	Teardown( void );

	// Variables.
	/** All loaded resident buffers */
	TArray<FSoundBufferIPhone*>				Buffers;
	/** Map from resource ID to sound buffer */
	TMap<INT, FSoundBufferIPhone*>			WaveBufferMap;
	/** Next resource ID value used for registering USoundNodeWave objects */
	INT										NextResourceID;

	/** Cached stream description structure */
	AudioStreamBasicDescription StreamDesc;

	/** The Core Audio graph of units */
	AUGraph ProcessingGraph;

	/** Pointer to the multi channel mixer unit through which we will render all audio*/ 
	AudioUnit MixerUnit;

	/** The node in the mixer unit */
	AUNode MixerNode;

	/** Counter for device suspend/resume */
	static INT SuspendCounter;

	/** Player location information */
	FVector PlayerLocation;
	FVector PlayerFacing;
	// this MUST follow PlayerFacing, they are both specified in one call 
	FVector PlayerUp;
	FVector PlayerRight;

	// allow access into this class
	friend class FSoundBufferIPhone;
	friend class FSoundSourceIPhone;
};

#endif