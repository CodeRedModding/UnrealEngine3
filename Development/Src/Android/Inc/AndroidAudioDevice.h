/*=============================================================================

=============================================================================*/

#ifndef __ANDROIDAUDIODEVICE_H__
#define __ANDROIDAUDIODEVICE_H__

class UAndroidAudioDevice;
class FAndroidSoundBuffer;
class FAndroidActiveSoundPoolElementWrapper;


#define NO_ANDROIDAUDIO_PRECACHING				1
#define NEVER_DESTROY_ANDROID_SOUND_BUFFER		1

class FAndroidSoundSource : public FSoundSource
{
public:

	FAndroidSoundSource( UAudioDevice* InAudioDevice );
	~FAndroidSoundSource( void );

	virtual UBOOL Init( FWaveInstance* WaveInstance );
	virtual void Update( void );
	virtual void Play( void );
	virtual void Stop( void );
	virtual void Pause( void );
	virtual UBOOL IsFinished( void );

	FLOAT GetVolume();

protected:
	/** Cached sound buffer associated with currently bound wave instance. */
	FAndroidSoundBuffer*					Buffer;	
	FAndroidActiveSoundPoolElementWrapper*	ActiveAudioTrack;

	// is mono active? else stereo
	UBOOL						bUsingMonoSource;
	UBOOL						bLooping;
	INT							ReadPosition;	

	friend class UAndroidAudioDevice;
};


class UAndroidAudioDevice : public UAudioDevice
{
	DECLARE_CLASS_INTRINSIC( UAndroidAudioDevice, UAudioDevice, CLASS_Config | 0, ALAudio )

public:
	virtual UBOOL Init( void );
	//virtual void Update( UBOOL bGameTicking );
	virtual void Precache( USoundNodeWave* SoundNodeWave );
	//virtual void ListSounds( const TCHAR* Cmd, FOutputDevice& Ar );
	virtual void FreeResource( USoundNodeWave* SoundNodeWave );
	//virtual void Serialize( FArchive& Ar );
	//virtual void FinishDestroy( void );
	//virtual void ShutdownAfterError( void );
	//void FindProcs( UBOOL AllowExt );
	
protected:
	/** The name of the OpenAL Device to open - defaults to "Generic Software" */
	FStringNoInit									DeviceName;
	/** All loaded resident buffers */
	TArray<FAndroidSoundBuffer*>					Buffers;
	/** Map from resource ID to sound buffer */
#if NEVER_DESTROY_ANDROID_SOUND_BUFFER
	TMap<FString, FAndroidSoundBuffer*>				WaveBufferMap;
#else
	TMap<INT, FAndroidSoundBuffer*>					WaveBufferMap;
#endif
	/** Next resource ID value used for registering USoundNodeWave objects */
	INT												NextResourceID;	

	friend class FAndroidSoundBuffer;
};

#endif