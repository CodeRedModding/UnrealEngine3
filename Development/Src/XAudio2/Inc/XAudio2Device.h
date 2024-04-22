/*=============================================================================
	XAudio2Device.h: Unreal XAudio2 audio interface object.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_XAUDIO2DEVICE
#define _INC_XAUDIO2DEVICE

/*------------------------------------------------------------------------------------
	XAudio2 system headers
------------------------------------------------------------------------------------*/
#include <xaudio2.h>
#include <X3Daudio.h>

/*------------------------------------------------------------------------------------
	Dependencies, helpers & forward declarations.
------------------------------------------------------------------------------------*/

/** Min clamp value for audio source volume */
#define XA2_SOURCE_VOL_MIN 0.0f

/** Max clamp value for audio source volume (NOTE: Was originally 1.0, but Steamworks VOIP requires a boost) */
#define XA2_SOURCE_VOL_MAX 1.0f


#if _WINDOWS
#define AUDIO_HWTHREAD			XAUDIO2_DEFAULT_PROCESSOR
#endif

enum ProcessingStages
{
	STAGE_SOURCE = 1,
	STAGE_RADIO,
	STAGE_REVERB,
	STAGE_EQPREMASTER,
	STAGE_OUTPUT
};

enum SourceDestinations
{
	DEST_DRY,
	DEST_REVERB,
	DEST_RADIO,
	DEST_COUNT
};

enum ChannelOutputs
{
	CHANNELOUT_FRONTLEFT = 0,
	CHANNELOUT_FRONTRIGHT,
	CHANNELOUT_FRONTCENTER,
	CHANNELOUT_LOWFREQUENCY,
	CHANNELOUT_LEFTSURROUND,
	CHANNELOUT_RIGHTSURROUND,

	CHANNELOUT_REVERB,
	CHANNELOUT_RADIO,
	CHANNELOUT_COUNT
};

#define SPEAKER_5POINT0          ( SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT )
#define SPEAKER_6POINT1          ( SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT | SPEAKER_BACK_CENTER )

class UXAudio2Device;

enum ESoundFormat
{
	SoundFormat_Invalid,
	SoundFormat_PCM,
	SoundFormat_PCMPreview,
	SoundFormat_PCMRT,
	SoundFormat_XMA2,
	SoundFormat_XWMA
};

struct FPCMBufferInfo
{
	/** Format of the source PCM data */
	WAVEFORMATEX				PCMFormat;
	/** Address of PCM data in physical memory */
	BYTE*						PCMData;
	/** Size of PCM data in physical memory */
	UINT32						PCMDataSize;
};

struct FXMA2BufferInfo
{
	/** Format of the source XMA2 data */
	XMA2WAVEFORMATEX			XMA2Format;
	/** Address of XMA2 data in physical memory */
	BYTE*						XMA2Data;
	/** Size of XMA2 data in physical memory */
	UINT32						XMA2DataSize;
};

struct FXWMABufferInfo
{
	/** Format of the source XWMA data */
	WAVEFORMATEXTENSIBLE		XWMAFormat;
	/** Additional info required for xwma */
	XAUDIO2_BUFFER_WMA			XWMABufferData;
	/** Address of XWMA data in physical memory */
	BYTE*						XWMAData;
	/** Size of XWMA data in physical memory */
	UINT32						XWMADataSize;
	/** Address of XWMA seek data in physical memory */
	UINT32*						XWMASeekData;
	/** Size of XWMA seek data */
	UINT32						XWMASeekDataSize;
};

/**
 * XAudio2 implementation of FSoundBuffer, containing the wave data and format information.
 */
class FXAudio2SoundBuffer
{
public:
	/** 
	 * Constructor
	 *
	 * @param AudioDevice	audio device this sound buffer is going to be attached to.
	 */
	FXAudio2SoundBuffer( UAudioDevice* AudioDevice, ESoundFormat SoundFormat );
	
	/**
	 * Destructor 
	 * 
	 * Frees wave data and detaches itself from audio device.
	 */
	~FXAudio2SoundBuffer( void );

	/** 
	 * Set up this buffer to contain and play XMA2 data
	 */
	void InitXMA2( UXAudio2Device* XAudio2Device, USoundNodeWave* Wave, struct FXMAInfo* XMAInfo );

	/** 
	 * Set up this buffer to contain and play XWMA data
	 */
	void InitXWMA( USoundNodeWave* Wave, struct FXMAInfo* XMAInfo );

	/** 
	 * Setup a WAVEFORMATEX structure
	 */
	void InitWaveFormatEx( WORD Format, USoundNodeWave* Wave, UBOOL bCheckPCMData );

	/** 
	 * Gets the type of buffer that will be created for this wave and stores it.
	 */
	static void GetSoundFormat( USoundNodeWave* Wave, FLOAT MinOggVorbisDuration );

	/**
	 * Decompresses a chunk of compressed audio to the destination memory
	 *
	 * @param Destination		Memory to decompress to
	 * @param bLooping			Whether to loop the sound seamlessly, or pad with zeroes
	 * @return					Whether the sound looped or not
	 */
	UBOOL ReadCompressedData( BYTE* Destination, UBOOL bLooping );

	/** 
	 * Handle Xenon specific bulk data
	 */
	static FXAudio2SoundBuffer* InitXenon( UXAudio2Device* XAudio2Device, USoundNodeWave* Wave );

	/**
	 * Static function used to create an OpenAL buffer and dynamically upload decompressed ogg vorbis data to.
	 *
	 * @param InWave		USoundNodeWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FXAudio2SoundBuffer* CreateQueuedBuffer( UXAudio2Device* XAudio2Device, USoundNodeWave* Wave );

	/**
	 * Static function used to create an OpenAL buffer and dynamically upload procedural data to.
	 *
	 * @param InWave		USoundNodeWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FXAudio2SoundBuffer* CreateProceduralBuffer( UXAudio2Device* XAudio2Device, USoundNodeWave* Wave );

	/**
	 * Static function used to create an OpenAL buffer and upload raw PCM data to.
	 *
	 * @param InWave		USoundNodeWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FXAudio2SoundBuffer* CreatePreviewBuffer( UXAudio2Device* XAudio2Device, USoundNodeWave* Wave, FXAudio2SoundBuffer* Buffer );

	/**
	 * Static function used to create an OpenAL buffer and upload decompressed ogg vorbis data to.
	 *
	 * @param InWave		USoundNodeWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FALSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FXAudio2SoundBuffer* CreateNativeBuffer( UXAudio2Device* XAudio2Device, USoundNodeWave* Wave );

	/**
	 * Static function used to create a buffer.
	 *
	 * @param InWave USoundNodeWave to use as template and wave source
	 * @param AudioDevice audio device to attach created buffer to
	 * @return FXAudio2SoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FXAudio2SoundBuffer* Init( UAudioDevice* AudioDevice, USoundNodeWave* InWave );

	/**
	 * Returns the size of this buffer in bytes.
	 *
	 * @return Size in bytes
	 */
	INT GetSize( void );

	/** Audio device this buffer is attached to	*/
	UAudioDevice*				AudioDevice;

	/** Format of the sound referenced by this buffer */
	INT							SoundFormat;

	union
	{
		FPCMBufferInfo			PCM;		
		FXMA2BufferInfo			XMA2;			// Xenon only
		FXWMABufferInfo			XWMA;			// Xenon only
	};

	/** Wrapper to handle the decompression of vorbis code */
	class FVorbisAudioInfo*		DecompressionState;
	/** Cumulative channels from all streams */
	INT							NumChannels;
	/** Resource ID of associated USoundNodeWave */
	INT							ResourceID;
	/** Human readable name of resource, most likely name of UObject associated during caching.	*/
	FString						ResourceName;
	/** Whether memory for this buffer has been allocated from permanent pool. */
	UBOOL						bAllocationInPermanentPool;
	/** Set to true when the PCM data should be freed when the buffer is destroyed */
	UBOOL						bDynamicResource;
};

/**
 * Source callback class for handling loops
 */
class FXAudio2SoundSourceCallback : public IXAudio2VoiceCallback
{
public:
	FXAudio2SoundSourceCallback( void )
	{
	}

	virtual ~FXAudio2SoundSourceCallback( void )
	{ 
	}

	virtual void STDCALL OnStreamEnd( void ) 
	{ 
	}

	virtual void STDCALL OnVoiceProcessingPassEnd( void ) 
	{
	}

	virtual void STDCALL OnVoiceProcessingPassStart( UINT32 SamplesRequired )
	{
	}

	virtual void STDCALL OnBufferEnd( void* BufferContext )
	{
	}

	virtual void STDCALL OnBufferStart( void* BufferContext )
	{
	}

	virtual void STDCALL OnLoopEnd( void* BufferContext );

	virtual void STDCALL OnVoiceError( void* BufferContext, HRESULT Error )
	{
	}

	friend class FXAudio2SoundSource;
};

/**
 * XAudio2 implementation of FSoundSource, the interface used to play, stop and update sources
 */
class FXAudio2SoundSource : public FSoundSource
{
public:
	/**
	 * Constructor
	 *
	 * @param	InAudioDevice	audio device this source is attached to
	 */
	FXAudio2SoundSource( UAudioDevice* InAudioDevice, FAudioEffectsManager* InEffects );

	/**
	 * Destructor, cleaning up voice
	 */
	virtual ~FXAudio2SoundSource( void );

	/**
	 * Frees existing resources. Called from destructor and therefore not virtual.
	 */
	void FreeResources( void );

	/**
	 * Initializes a source with a given wave instance and prepares it for playback.
	 *
	 * @param	WaveInstance	wave instance being primed for playback
	 * @return	TRUE if initialization was successful, FALSE otherwise
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
	 * Handles feeding new data to a real time decompressed sound
	 */
	void HandleRealTimeSource( void );

	/**
	 * Queries the status of the currently associated wave instance.
	 *
	 * @return	TRUE if the wave instance/ source has finished playback and FALSE if it is 
	 *			currently playing or paused.
	 */
	virtual UBOOL IsFinished( void );

	/**
	 * Create a new source voice
	 */
	UBOOL CreateSource( void );

	/** 
	 * Submit the relevant audio buffers to the system
	 */
	void SubmitPCMBuffers( void );

	/** 
	 * Submit the relevant audio buffers to the system
	 */
	void SubmitPCMRTBuffers( void );

	/** 
	 * Submit the relevant audio buffers to the system, accounting for looping modes
	 */
	void SubmitXMA2Buffers( void );

	/** 
	 * Submit the relevant audio buffers to the system
	 */
	void SubmitXWMABuffers( void );

	/**
	 * Calculates the volume for each channel
	 */
	void GetChannelVolumes( FLOAT ChannelVolumes[CHANNELOUT_COUNT], FLOAT AttenuatedVolume );

	/** 
	 * Maps a sound with a given number of channels to to expected speakers
	 */
	void RouteDryToSpeakers( FLOAT ChannelVolumes[CHANNELOUT_COUNT] );

	/** 
	 * Maps the sound to the relevant reverb effect
	 */
	void RouteToReverb( FLOAT ChannelVolumes[CHANNELOUT_COUNT] );

	/** 
	 * Maps the sound to the relevant radio effect.
	 *
	 * @param	ChannelVolumes	The volumes associated to each channel. 
	 *							Note: Not all channels are mapped directly to a speaker.
	 */
	void RouteToRadio( FLOAT ChannelVolumes[CHANNELOUT_COUNT] );

protected:
	/** Decompress through XAudio2Buffer, or call USoundNodeWave procedure to generate more PCM data. Returns true/false: did audio loop? */
	UBOOL ReadMorePCMData(const INT BufferIndex);

	/** Handle obtaining more data for procedural USoundNodeWaves. Always returns FALSE for convenience. */
	UBOOL ReadProceduralData(const INT BufferIndex);

	/**
	 * Utility function for determining the proper index of an effect. Certain effects (such as: reverb and radio distortion) 
	 * are optional. Thus, they may be NULL, yet XAudio2 cannot have a NULL output voice in the send list for this source voice.
	 *
	 * @return	The index of the destination XAudio2 submix voice for the given effect; -1 if effect not in destination array. 
	 *
	 * @param	Effect	The effect type's (Reverb, Radio Distoriton, etc) index to find. 
	 */
	INT GetDestinationVoiceIndexForEffect( SourceDestinations Effect );

	/** Owning classes */
	UXAudio2Device*				AudioDevice;
	FXAudio2EffectsManager*		Effects;

	/** XAudio2 source voice associated with this source. */
	IXAudio2SourceVoice*		Source;
	/** Structure to handle looping sound callbacks */
	FXAudio2SoundSourceCallback	SourceCallback;
	/** Destination voices */
	XAUDIO2_SEND_DESCRIPTOR		Destinations[DEST_COUNT];
	/** Cached sound buffer associated with currently bound wave instance. */
	FXAudio2SoundBuffer*		Buffer;
	/** Which sound buffer should be written to next - used for double buffering. */
	INT							CurrentBuffer;
	/** A pair of sound buffers to allow notification when a sound loops. */
	XAUDIO2_BUFFER				XAudio2Buffers[2];
	/** Additional buffer info for XWMA sounds */
	XAUDIO2_BUFFER_WMA			XAudio2BufferXWMA[1];
	/** Set when we wish to let the buffers play themselves out */
	UBOOL						bBuffersToFlush;
	/** Set to TRUE when the loop end callback is hit */
	UBOOL						bLoopCallback;

	friend class UXAudio2Device;
	friend class FXAudio2SoundSourceCallback;
};

/**
 * XAudio2 implementation of an Unreal audio device. Neither use XACT nor X3DAudio.
 */
class UXAudio2Device : public UAudioDevice
{
	DECLARE_CLASS_INTRINSIC( UXAudio2Device, UAudioDevice, CLASS_Config | 0, XAudio2 )

	/**
	 * Static constructor, used to associate .ini options with member variables.	
	 */
	void StaticConstructor( void );

	/**
	 * Initializes the audio device and creates sources.
	 *
	 * @warning: Relies on XAudioInitialize already being called
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
	virtual void Update( UBOOL Realtime );

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

	/**
	 * Shuts down audio device. This will never be called with the memory image
	 * codepath.
	 */
	virtual void FinishDestroy( void );
	
	/**
	 * Special variant of Destroy that gets called on fatal exit. Doesn't really
	 * matter on the console so for now is just the same as Destroy so we can
	 * verify that the code correctly cleans up everything.
	 */
	virtual void ShutdownAfterError( void );

	/** 
	 * Check for errors and output a human readable string 
	 */
	virtual UBOOL ValidateAPICall( const TCHAR* Function, INT ErrorCode );

	/**
	 * Exec handler used to parse console commands.
	 *
	 * @param	Cmd		Command to parse
	 * @param	Ar		Output device to use in case the handler prints anything
	 * @return	TRUE if command was handled, FALSE otherwise
	 */
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );

	/**
	 * Init the XAudio2 device
	 */
	static UBOOL InitHardware( void );

	/** 
     * Derives the output matrix to use based on the channel mask and the number of channels
	 */
	static UBOOL GetOutputMatrix( DWORD ChannelMask, DWORD NumChannels );

	/**
	 * Get information about the memory allocated for a sound.
	 * @param	ResourceID	The resource to get information about.
	 * @param	BytesUsed	The number of bytes used for the resource.
	 * @param	IsPartOfPool	Whether the bytes came from the common audio pool or an individual allocation.
	 * @return	True if successful, false if ResourceID wasn't found or some other error occured.
	 */
	UBOOL GetResourceAllocationInfo(INT ResourceID, /*OUT*/ INT& BytesUsed, /*OUT*/ UBOOL& IsPartOfPool);
protected:
	/**
	 * Frees the resources associated with this buffer
	 *
	 * @param	FXAudio2SoundBuffer	Buffer to clean up
	 */
	void FreeBufferResource( FXAudio2SoundBuffer* Buffer );

	/** 
	 * Links up the resource data indices for looking up and cleaning up
	 */
	void TrackResource( USoundNodeWave* Wave, FXAudio2SoundBuffer* Buffer );

	/**
     * Allocates memory from permanent pool. This memory will NEVER be freed.
	 *
	 * @param	Size	Size of allocation.
	 * @param	AllocatedInPool	(OUT) True if the allocation occurred from the pool; false if it was a regular physical allocation.
	 *
	 * @return pointer to a chunk of memory with size Size
	 */
	void* AllocatePermanentMemory( INT Size, /*OUT*/ UBOOL& AllocatedInPool );
	
	/**
	 * Tears down audio device by stopping all sounds, removing all buffers, 
	 * destroying all sources, ... Called by both Destroy and ShutdownAfterError
	 * to perform the actual tear down.
	 */
	void Teardown( void );

	/** Test decompress a vorbis file */
	void TestDecompressOggVorbis( USoundNodeWave* Wave );
	/** Decompress a wav a number of times for profiling purposes */
	void TimeTest( FOutputDevice& Ar, const TCHAR* WaveAssetName );

	/** Array of all created buffers associated with this audio device */
	TArray<FXAudio2SoundBuffer*>		Buffers;
	/** Look up associating a USoundNodeWave's resource ID with low level sound buffers	*/
	TMap<INT, FXAudio2SoundBuffer*>		WaveBufferMap;
	/** Inverse listener transformation, used for spatialization */
	FMatrix								InverseTransform;
	/** Next resource ID value used for registering USoundNodeWave objects */
	INT									NextResourceID;

	/** Variables required for the early init */
	static INT							NumSpeakers;
	static IXAudio2*					XAudio2;
	static IXAudio2MasteringVoice*		MasteringVoice;
	static const FLOAT*					OutputMixMatrix;
	static XAUDIO2_DEVICE_DETAILS		DeviceDetails;

	friend class FXAudio2SoundBuffer;
	friend class FXAudio2SoundSource;
	friend class FXAudio2EffectsManager;
	friend class FVoiceInterfaceXe;
	friend class FBinkMovieAudio;
    friend class FGFxEngine;
};

/**
 * Helper class for 5.1 spatialization.
 */
class FSpatializationHelper
{
	/** Instance of X3D used to calculate volume multipliers.	*/
	X3DAUDIO_HANDLE		          X3DInstance;
	
    X3DAUDIO_DSP_SETTINGS         DSPSettings;
    X3DAUDIO_LISTENER             Listener;
    X3DAUDIO_EMITTER              Emitter;
    X3DAUDIO_CONE                 Cone;
	
	X3DAUDIO_DISTANCE_CURVE_POINT VolumeCurvePoint[2];
	X3DAUDIO_DISTANCE_CURVE       VolumeCurve;
	
	X3DAUDIO_DISTANCE_CURVE_POINT ReverbVolumeCurvePoint[2];
	X3DAUDIO_DISTANCE_CURVE       ReverbVolumeCurve;

	FLOAT                         EmitterAzimuths;
	FLOAT					      MatrixCoefficients[SPEAKER_COUNT];
	
public:
	/**
	 * Constructor, initializing all member variables.
	 */
	FSpatializationHelper( void );

	/**
	 * Calculates the spatialized volumes for each channel.
	 *
	 * @param	OrientFront				The listener's facing direction.
	 * @param	ListenerPosition		The position of the listener.
	 * @param	EmitterPosition			The position of the emitter.
	 * @param	OutVolumes				An array of floats with one volume for each output channel.
	 * @param	OutReverbLevel			The reverb volume
	 */
	void CalculateDolbySurroundRate( const FVector& OrientFront, const FVector& ListenerPosition, const FVector& EmitterPosition, FLOAT OmniRadius, FLOAT* OutVolumes );
};

class FXMPHelper
{
private:
	/** Count of current cinematic audio clips playing (used to turn on/off XMP background music, allowing for overlap) */
	INT							CinematicAudioCount;
	/** Count of current movies playing (used to turn on/off XMP background music, NOT allowing for overlap) */
	BOOL						MoviePlaying;
	/** Flag indicating whether or not XMP playback is enabled (defaults to TRUE) */
	BOOL						XMPEnabled;
	/** Flag indicating whether or not XMP playback is blocked (defaults to FALSE)
	 *	Updated when player enters single-play:
	 *		XMP is blocked if the player hasn't finished the game before
	 */
	BOOL						XMPBlocked;

public:
	/**
	 * Constructor, initializing all member variables.
	 */
	FXMPHelper( void );
	/**
	 * Destructor, performing final cleanup.
	 */
	~FXMPHelper( void );

	/**
	 * Accessor for getting the XMPHelper class
	 */
	static FXMPHelper* GetXMPHelper( void );

	/**
	 * Records that a cinematic audio track has started playing.
	 */
	void CinematicAudioStarted( void );
	/**
	 * Records that a cinematic audio track has stopped playing.
	 */
	void CinematicAudioStopped( void );
	/**
	 * Records that a movie has started playing.
	 */
	void MovieStarted( void );
	/**
	 * Records that a movie has stopped playing.
	 */
	void MovieStopped( void );
	/**
	 * Called with every movie/cinematic change to update XMP status if necessary
	 */
	void CountsUpdated( void );
	/**
	 * Called to block XMP playback (when the gamer hasn't yet finished the game and enters single-play)
	 */
	void BlockXMP( void );
	/**
	 * Called to unblock XMP playback (when the gamer has finished the game or exits single-play)
	 */
	void UnblockXMP( void );
};

#endif
