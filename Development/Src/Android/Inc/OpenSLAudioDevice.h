
#ifndef _INC_OPENSLAUDIODEVICE
#define _INC_OPENSLAUDIODEVICE

// necessary files
#include <SLES/OpenSLES.h>
#include "SLES/OpenSLES_Android.h"


class UOpenSLAudioDevice;

////////////////////////////////////
/// Class: 
///    class FOpenSLSoundBuffer
///    
/// Description: 
///    
///    
////////////////////////////////////
class FOpenSLSoundBuffer
{
public:
	FOpenSLSoundBuffer( UOpenSLAudioDevice* AudioDevice );
	~FOpenSLSoundBuffer( void );
	
	static FOpenSLSoundBuffer* Init( USoundNodeWave* InWave, UOpenSLAudioDevice* AudioDevice );

	INT GetSize( void ) 
	{ 
		return( BufferSize ); 
	}
	INT GetNumChannels( void ) 
	{ 
		return( NumChannels ); 
	}
	BYTE* GetSoundData() { return AudioData; };
		
	/** Audio device this buffer is attached to */
	UOpenSLAudioDevice*		AudioDevice;
	
	BYTE*					AudioData;

	INT						ResourceID;
	FString					ResourceName;
	INT						BufferSize;
	INT						NumChannels;
	INT						SampleRate;
};



////////////////////////////////////
/// Class: 
///    class FOpenSLSoundSource : public FSoundSource
///    
/// Description: 
///    
///    
////////////////////////////////////
class FOpenSLSoundSource : public FSoundSource
{
public:
	FOpenSLSoundSource( UAudioDevice* InAudioDevice ) : FSoundSource( InAudioDevice ), Device( (UOpenSLAudioDevice*) InAudioDevice), Buffer( NULL ) {}
	~FOpenSLSoundSource( void );

	virtual UBOOL	Init( FWaveInstance* WaveInstance );	
	virtual void	Update( void );
	virtual void	Play( void );
	virtual void	Stop( void );
	virtual void	Pause( void );
	virtual UBOOL	IsFinished( void );
			UBOOL	IsSourceFinished( void );

protected:
	FOpenSLSoundBuffer*		Buffer;
	UOpenSLAudioDevice*		Device;
	friend class UOpenSLAudioDevice;

	//
	SLObjectItf						SL_PlayerObject;
	SLPlayItf						SL_PlayerPlayInterface;
	SLAndroidSimpleBufferQueueItf	SL_PlayerBufferQueue;
	SLVolumeItf						SL_VolumeInterface;
};



////////////////////////////////////
/// Class: 
///    class UOpenSLAudioDevice : public UAudioDevice
///    
/// Description: 
///    
///    
////////////////////////////////////
class UOpenSLAudioDevice : public UAudioDevice
{
	DECLARE_CLASS_INTRINSIC( UOpenSLAudioDevice, UAudioDevice, CLASS_Config | 0, ALAudio )

	//
	virtual UBOOL	Init( void );
	virtual void	Update( UBOOL bGameTicking );
	virtual void	Precache( USoundNodeWave* SoundNodeWave );	
	virtual void	FreeResource( USoundNodeWave* SoundNodeWave );

	// UObject interface.
			void	StaticConstructor( void );
	virtual void	Serialize( FArchive& Ar );
	virtual void	FinishDestroy( void );	
	virtual void	ShutdownAfterError( void );

protected:
			void	Teardown( void );

	// engine interfaces
	SLObjectItf									SL_EngineObject;
	SLEngineItf									SL_EngineEngine;
	SLObjectItf									SL_OutputMixObject;

	INT											SL_VolumeMax;
	INT											SL_VolumeMin;

	// Variables.
	/** The name of the OpenSL Device to open - defaults to "Generic Software" */
	FStringNoInit								DeviceName;
	/** All loaded resident buffers */
	TArray<FOpenSLSoundBuffer*>					Buffers;
	/** Map from resource ID to sound buffer */
	TMap<INT, FOpenSLSoundBuffer*>				WaveBufferMap;
	/** Next resource ID value used for registering USoundNodeWave objects */
	INT											NextResourceID;

	// 
	friend class FOpenSLSoundBuffer;
	friend class FOpenSLSoundSource;
};

#endif