/*=============================================================================
 	CoreAudioDevice.h: Unreal CoreAudio audio interface object.
 	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#ifndef _INC_COREAUDIODEVICE
#define _INC_COREAUDIODEVICE

/*------------------------------------------------------------------------------------
 CoreAudio system headers
 ------------------------------------------------------------------------------------*/
#include <CoreAudio/AudioHardware.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>

class UCoreAudioDevice;
class FCoreAudioEffectsManager;

enum ESoundFormat
{
	SoundFormat_Invalid,
	SoundFormat_PCM,
	SoundFormat_PCMPreview,
	SoundFormat_PCMRT
};

struct CoreAudioBuffer
{
	BYTE *AudioData;
	INT AudioDataSize;
	INT ReadCursor;
};

/**
 * CoreAudio implementation of FSoundBuffer, containing the wave data and format information.
 */
class FCoreAudioSoundBuffer
{
public:
	/** 
	 * Constructor
	 *
	 * @param AudioDevice	audio device this sound buffer is going to be attached to.
	 */
	FCoreAudioSoundBuffer( UAudioDevice* AudioDevice, ESoundFormat SoundFormat );
	
	/**
	 * Destructor 
	 * 
	 * Frees wave data and detaches itself from audio device.
	 */
	~FCoreAudioSoundBuffer( void );
	
	/** 
	 * Setup an AudioStreamBasicDescription structure
	 */
	void InitAudioStreamBasicDescription( UInt32 FormatID, USoundNodeWave* Wave, UBOOL bCheckPCMData );
	
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
	 * Static function used to create a CoreAudio buffer and dynamically upload decompressed ogg vorbis data to.
	 *
	 * @param InWave		USoundNodeWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FCoreAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FCoreAudioSoundBuffer* CreateQueuedBuffer( UCoreAudioDevice* XAudio2Device, USoundNodeWave* Wave );
	
	/**
	 * Static function used to create a CoreAudio buffer and dynamically upload procedural data to.
	 *
	 * @param InWave		USoundNodeWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FCoreAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FCoreAudioSoundBuffer* CreateProceduralBuffer( UCoreAudioDevice* XAudio2Device, USoundNodeWave* Wave );
	
	/**
	 * Static function used to create a CoreAudio buffer and upload raw PCM data to.
	 *
	 * @param InWave		USoundNodeWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FCoreAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FCoreAudioSoundBuffer* CreatePreviewBuffer( UCoreAudioDevice* XAudio2Device, USoundNodeWave* Wave, FCoreAudioSoundBuffer* Buffer );
	
	/**
	 * Static function used to create a CoreAudio buffer and upload decompressed ogg vorbis data to.
	 *
	 * @param Wave			USoundNodeWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FCoreAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FCoreAudioSoundBuffer* CreateNativeBuffer( UCoreAudioDevice* CoreAudioDevice, USoundNodeWave* Wave );
	
	/**
	 * Static function used to create a buffer.
	 *
	 * @param InWave USoundNodeWave to use as template and wave source
	 * @param AudioDevice audio device to attach created buffer to
	 * @return FCoreAudioSoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FCoreAudioSoundBuffer* Init( UAudioDevice* AudioDevice, USoundNodeWave* InWave );
	
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
	
	/** Format of the source PCM data */
	AudioStreamBasicDescription	PCMFormat;
	/** Address of PCM data in physical memory */
	BYTE*						PCMData;
	/** Size of PCM data in physical memory */
	INT							PCMDataSize;
	
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
 * CoreAudio implementation of FSoundSource, the interface used to play, stop and update sources
 */
class FCoreAudioSoundSource : public FSoundSource
{
public:
	/**
	 * Constructor
	 *
	 * @param	InAudioDevice	audio device this source is attached to
	 */
	FCoreAudioSoundSource( UAudioDevice* InAudioDevice, FAudioEffectsManager* InEffects );
	
	/** 
	 * Destructor
	 */
	~FCoreAudioSoundSource( void );
	
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
	 * Submit the relevant audio buffers to the system
	 */
	void SubmitPCMBuffers( void );
	
	/** 
	 * Submit the relevant audio buffers to the system
	 */
	void SubmitPCMRTBuffers( void );

	static OSStatus CoreAudioRenderCallback( void *InRefCon, AudioUnitRenderActionFlags *IOActionFlags,
											const AudioTimeStamp *InTimeStamp, UInt32 InBusNumber,
											UInt32 InNumberFrames, AudioBufferList *IOData );
	static OSStatus CoreAudioConvertCallback( AudioConverterRef converter, UInt32 *ioNumberDataPackets, AudioBufferList *ioData,
											 AudioStreamPacketDescription **outPacketDescription, void *inUserData );

protected:
	/** Decompress USoundNodeWave procedure to generate more PCM data. Returns true/false: did audio loop? */
	UBOOL ReadMorePCMData( const INT BufferIndex );

	/** Handle obtaining more data for procedural USoundNodeWaves. Always returns FALSE for convenience. */
	UBOOL ReadProceduralData( const INT BufferIndex );

	OSStatus CreateAudioUnit( OSType Type, OSType SubType, OSType Manufacturer, AUNode DestNode, DWORD DestInputNumber, AUNode* OutNode, AudioUnit* OutUnit );
	UBOOL AttachToAUGraph();
	UBOOL DetachFromAUGraph();

	/** Owning classes */
	UCoreAudioDevice*			AudioDevice;
	FCoreAudioEffectsManager*	Effects;

	/** Cached sound buffer associated with currently bound wave instance. */
	FCoreAudioSoundBuffer*		Buffer;

	AudioConverterRef			CoreAudioConverter;

	/** Which sound buffer should be written to next - used for double buffering. */
	INT							CurrentBuffer;
	/** A pair of sound buffers to allow notification when a sound loops. */
	CoreAudioBuffer				CoreAudioBuffers[2];
	/** Set when we wish to let the buffers play themselves out */
	UBOOL						bBuffersToFlush;
	/** Set to TRUE when the loop end callback is hit */
	UBOOL						bLoopCallback;

	AUNode						SourceNode;
	AudioUnit					SourceUnit;
	AUNode						EQNode;
	AudioUnit					EQUnit;
	AUNode						RadioNode;
	AudioUnit					RadioUnit;
	AUNode						ReverbNode;
	AudioUnit					ReverbUnit;

	INT							AudioChannel;
	INT							BufferInUse;
	INT							NumActiveBuffers;

private:

	void FreeResources();

	friend class UCoreAudioDevice;
	friend class FCoreAudioEffectsManager;
};

/**
 * CoreAudio implementation of an Unreal audio device.
 */
class UCoreAudioDevice : public UAudioDevice
{
	DECLARE_CLASS_INTRINSIC( UCoreAudioDevice, UAudioDevice, CLASS_Config | 0, CoreAudio )

	/**
	 * Static constructor, used to associate .ini options with member variables.	
	 */
	void StaticConstructor( void );

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
	
	/**
	 * Shuts down audio device. This will never be called with the memory image codepath.
	 */
	virtual void FinishDestroy( void );
	
	/**
	 * Special variant of Destroy that gets called on fatal exit. 
	 */
	virtual void ShutdownAfterError( void );

	AUGraph GetAudioUnitGraph() const { return AudioUnitGraph; }
	AUNode GetMixerNode() const { return MixerNode; }
	AudioUnit GetMixerUnit() const { return MixerUnit; }
	
protected:

	/**
	 * Frees the resources associated with this buffer
	 *
	 * @param	FCoreAudioSoundBuffer	Buffer to clean up
	 */
	void FreeBufferResource( FCoreAudioSoundBuffer* Buffer );

	/** 
	 * Links up the resource data indices for looking up and cleaning up
	 */
	void TrackResource( USoundNodeWave* Wave, FCoreAudioSoundBuffer* Buffer );

	/**
	 * Tears down audio device by stopping all sounds, removing all buffers, 
	 * destroying all sources, ... Called by both Destroy and ShutdownAfterError
	 * to perform the actual tear down.
	 */
	void Teardown( void );

	/** Next resource ID value used for registering USoundNodeWave objects */
	INT											NextResourceID;

	/** Array of all created buffers associated with this audio device */
	TArray<FCoreAudioSoundBuffer*>		Buffers;
	/** Look up associating a USoundNodeWave's resource ID with low level sound buffers	*/
	TMap<INT, FCoreAudioSoundBuffer*>	WaveBufferMap;
	/** Inverse listener transformation, used for spatialization */
	FMatrix								InverseTransform;
	
private:

	AUGraph						AudioUnitGraph;
	AUNode						OutputNode;
	AudioUnit					OutputUnit;
	AUNode						MixerNode;
	AudioUnit					MixerUnit;
	AudioStreamBasicDescription	MixerFormat;

	friend class FCoreAudioSoundBuffer;
	friend class FCoreAudioSoundSource;
};

#endif
