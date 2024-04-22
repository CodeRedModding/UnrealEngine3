/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemGameSpy.h"
#include "VoiceInterfaceCommon.h"

#if WITH_UE3_NETWORKING && WITH_GAMESPY

// Include the windows client code, so we can get the game viewport's window.
#if _WINDOWS
#include "WinDrv.h"
#endif

#define VOICE_USE_LOOPBACK 0

#define MAX_CAPTURE_DEVICES 8

#if SUPPORTS_PRAGMA_PACK
#pragma pack( push, 8 )
#endif

#include "voice2/gv.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack( pop )
#endif

#ifdef _WINDOWS_
#pragma comment( lib, "dsound.lib" )
#endif

#define INVALID_OWNING_INDEX (DWORD)-1

/**
 * Logs each device that was found in the array specified
 *
 * @param Devices the array of devices to log
 * @param NumDevices the number of devices in the array
 * @param DeviceType a string indicating their type (capture, playback, both)
 */
void DumpDevices(GVDeviceInfo* Devices,INT NumDevices,const TCHAR* DeviceType)
{
	debugf(NAME_DevOnline,TEXT("Found %d %s devices:"),NumDevices,DeviceType);
	for (INT Count = 0; Count < NumDevices; Count++)
	{
#if PS3
		debugf(NAME_DevOnline,
			TEXT("Found device (%s) with id (%d) of type %s"),
			Devices[Count].m_name,
			Devices[Count].m_id,
			Devices[Count].m_deviceType == GV_CAPTURE_AND_PLAYBACK ? TEXT("capture & playback") :
				Devices[Count].m_deviceType == GV_CAPTURE ? TEXT("capture") : TEXT("playback"));
#else
		debugf(NAME_DevOnline,
			TEXT("Found device (%s) of type %s"),
			Devices[Count].m_name,
			Devices[Count].m_deviceType == GV_CAPTURE_AND_PLAYBACK ? TEXT("capture & playback") :
				Devices[Count].m_deviceType == GV_CAPTURE ? TEXT("capture") : TEXT("playback"));
#endif
	}
}

/**
 * This interface is an abstract mechanism for getting voice data. Each platform
 * implements a specific version of this interface. The 
 */
class FVoiceInterfaceGameSpy :
	public FVoiceInterfaceCommon
{
	/** Singleton instance pointer */
	static FVoiceInterfaceGameSpy* GVoiceInterface;

	/** The audio playback device */
	GVDevice PlayDevice;
	/** The audio capture device */
	GVDevice CaptureDevice;
	/** The user that owns the voice devices */
	DWORD OwningIndex;
	/** Whether gvStartup() was successful or not */
	UBOOL bWasStartedOk;
	/** INI setting controlling whether voice is wanted or not */
	UBOOL bHasVoiceEnabled;
	/** The configured volume threshold to use for capturing voice packets */
	FLOAT VolumeThreshold;

	/** 
	 * Codec info
	 */
	INT	SamplesPerFrame;
	INT EncodedFrameSize;
	INT BitsPerSecond;

	/** Simple constructor that zeros members. Hidden due to factory method */
	FVoiceInterfaceGameSpy( void ) :
		PlayDevice(NULL),
		CaptureDevice(NULL),
		OwningIndex(INVALID_OWNING_INDEX),
		bWasStartedOk(FALSE),
		bHasVoiceEnabled(FALSE)
	{
	}

	/**
	 * Static wrapper that re-routes the call to the singleton
	 */
	static void FilterCallback( GVDevice InDevice, GVSample* Data, GVFrameStamp FrameStamp )
	{
#if PS3
		GVSample* Source = Data;
		SWORD* ShortSamples = ( SWORD* )appAlloca( GVoiceInterface->SamplesPerFrame * sizeof( SWORD ) );
		SWORD* Dest = ShortSamples;

		for( INT i = 0; i < GVoiceInterface->SamplesPerFrame; i++ )
		{
			SWORD NewSample = *Source++;
			*Dest++ = ( ( NewSample >> 8 ) & 0xff ) | ( ( NewSample << 8 ) & 0xff00 ); 
		}

		GVoiceInterface->RawVoiceDataCallback( 0, ShortSamples, GVoiceInterface->SamplesPerFrame );
#else
		GVoiceInterface->RawVoiceDataCallback( 0, Data, GVoiceInterface->SamplesPerFrame );
#endif
	}

	/**
	 * Creates the GameSpy voice engine
	 *
	 * @param MaxLocalTalkers the maximum number of local talkers to support
	 * @param MaxRemoteTalkers the maximum number of remote talkers to support
	 * @param bIsSpeechRecognitionDesired whether speech recognition should be enabled
	 *
	 * @return TRUE if everything initialized correctly, FALSE otherwise
	 */
	virtual UBOOL Init( INT MaxLocalTalkers, INT MaxRemoteTalkers, UBOOL bIsSpeechRecognitionDesired );

	/**
	 * Determines if the specified index is the owner or not
	 *
	 * @param InIndex the index being tested
	 *
	 * @return TRUE if this is the owner, FALSE otherwise
	 */
	FORCEINLINE UBOOL IsOwningIndex(DWORD InIndex)
	{
		return InIndex >= 0 && InIndex < MAX_SPLITSCREEN_TALKERS && OwningIndex == InIndex;
	}

	/** Creates the capture and playback devices */
	void CreateDevices(void);

public:

	/** Destructor that releases the engine if allocated */
	virtual ~FVoiceInterfaceGameSpy( void )
	{
		if (bWasStartedOk)
		{
			if (CaptureDevice)
			{
				gvStopDevice(CaptureDevice,GV_CAPTURE);
				gvFreeDevice(CaptureDevice);
			}
			gvCleanup();
		}
	}

	/**
	 * Returns an instance of the singleton object
	 *
	 * @param MaxLocalTalkers the maximum number of local talkers to support
	 * @param MaxRemoteTalkers the maximum number of remote talkers to support
	 * @param bIsSpeechRecognitionDesired whether speech recognition should be enabled
	 *
	 * @return A pointer to the singleton object or NULL if failed to init
	 */
	static FVoiceInterfaceGameSpy* CreateInstance( INT MaxLocalTalkers, INT MaxRemoteTalkers, UBOOL bIsSpeechRecognitionDesired )
	{
		if( GVoiceInterface == NULL )
		{
			GVoiceInterface = new FVoiceInterfaceGameSpy();
			// Init the GameSpy engine with those defaults
			if( GVoiceInterface->Init( MaxLocalTalkers, MaxRemoteTalkers, bIsSpeechRecognitionDesired ) == FALSE )
			{
				delete GVoiceInterface;
				GVoiceInterface = NULL;
			}
		}
		return GVoiceInterface;
	}

// FVoiceInterface

	/**
	 * Starts local voice processing for the specified user index
	 *
	 * @param UserIndex the user to start processing for
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD StartLocalVoiceProcessing(DWORD UserIndex);

	/**
	 * Stops local voice processing for the specified user index
	 *
	 * @param UserIndex the user to stop processing of
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD StopLocalVoiceProcessing(DWORD UserIndex);

	/**
	 * Starts remote voice processing for the specified user
	 *
	 * @param UniqueId the unique id of the user that will be talking
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD StartRemoteVoiceProcessing(FUniqueNetId UniqueId);

	/**
	 * Stops remote voice processing for the specified user
	 *
	 * @param UniqueId the unique id of the user that will no longer be talking
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD StopRemoteVoiceProcessing(FUniqueNetId UniqueId);

	/**
	 * Tells the voice system to start tracking voice data for speech recognition
	 *
	 * @param UserIndex the local user to recognize voice data for
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD StartSpeechRecognition(DWORD UserIndex);

	/**
	 * Tells the voice system to stop tracking voice data for speech recognition
	 *
	 * @param UserIndex the local user to recognize voice data for
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD StopSpeechRecognition(DWORD UserIndex);

	/**
	 * Registers the user index as a local talker (interested in voice data)
	 *
	 * @param UserIndex the user index that is going to be a talker
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD RegisterLocalTalker(DWORD UserIndex);

	/**
	 * Unregisters the user index as a local talker (not interested in voice data)
	 *
	 * @param UserIndex the user index that is no longer a talker
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD UnregisterLocalTalker(DWORD UserIndex);

	/**
	 * Registers the unique player id as a remote talker (submitted voice data only)
	 *
	 * @param UniqueId the id of the remote talker
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD RegisterRemoteTalker(FUniqueNetId UniqueId);

	/**
	 * Unregisters the unique player id as a remote talker
	 *
	 * @param UniqueId the id of the remote talker
	 *
	 * @return 0 upon success, an error code otherwise
	 */
    virtual DWORD UnregisterRemoteTalker(FUniqueNetId UniqueId);

	/**
	 * Checks whether a local user index has a headset present or not
	 *
	 * @param UserIndex the user to check status for
	 *
	 * @return TRUE if there is a headset, FALSE otherwise
	 */
    virtual UBOOL IsHeadsetPresent(DWORD UserIndex);

	/**
	 * Determines whether a local user index is currently talking or not
	 *
	 * @param UserIndex the user to check status for
	 *
	 * @return TRUE if the user is talking, FALSE otherwise
	 */
    virtual UBOOL IsLocalPlayerTalking(DWORD UserIndex);

	/**
	 * Determines whether a remote talker is currently talking or not
	 *
	 * @param UniqueId the unique id of the talker to check status on
	 *
	 * @return TRUE if the user is talking, FALSE otherwise
	 */
	virtual UBOOL IsRemotePlayerTalking(FUniqueNetId UniqueId);

	/**
	 * Returns which local talkers have data ready to be read from the voice system
	 *
	 * @return Bit mask of talkers that have data to be read (1 << UserIndex)
	 */
	virtual DWORD GetVoiceDataReadyFlags(void);

	/**
	 * Sets the playback priority of a remote talker for the given user. A
	 * priority of 0xFFFFFFFF indicates that the player is muted. All other
	 * priorities sorted from zero being most important to higher numbers
	 * being less important.
	 *
	 * @param UserIndex the local talker that is setting the priority
	 * @param UniqueId the id of the remote talker that is having priority changed
	 * @param Priority the new priority to apply to the talker
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD SetPlaybackPriority(DWORD UserIndex,FUniqueNetId RemoteTalkerId,DWORD Priority);

	/**
	 * Reads local voice data for the specified local talker. The size field
	 * contains the buffer size on the way in and contains the amount read on
	 * the way out
	 *
	 * @param UserIndex the local talker that is having their data read
	 * @param Data the buffer to copy the voice data into
	 * @param Size in: the size of the buffer, out: the amount of data copied
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD ReadLocalVoiceData(DWORD UserIndex,BYTE* Data,DWORD* Size);

	/**
	 * Submits remote voice data for playback by the voice system. No playback
	 * occurs if the priority for this remote talker is 0xFFFFFFFF. Size
	 * indicates how much data to submit for processing. It's also an out
	 * value in case the system could only process a smaller portion of the data
	 *
	 * @param RemoteTalkerId the remote talker that sent this data
	 * @param Data the buffer to copy the voice data into
	 * @param Size in: the size of the buffer, out: the amount of data copied
	 *
	 * @return 0 upon success, an error code otherwise
	 */
	virtual DWORD SubmitRemoteVoiceData(FUniqueNetId RemoteTalkerId,BYTE* Data,DWORD* Size);

	/**
	 * Allows for platform specific servicing of devices, etc.
	 *
	 * @param DeltaTime the amount of time that has elapsed since the last update
	 */
	virtual void Tick(FLOAT DeltaTime);
};

/** Singleton instance pointer initialization */
FVoiceInterfaceGameSpy* FVoiceInterfaceGameSpy::GVoiceInterface = NULL;

/**
 * Platform specific method for creating the voice interface to use for all
 * voice data/communication
 *
 * @param MaxLocalTalkers the number of local talkers to handle voice for
 * @param MaxRemoteTalkers the number of remote talkers to handle voice for
 * @param bIsSpeechRecognitionDesired whether speech recognition should be enabled or not
 *
 * @return The voice interface to use
 */
FVoiceInterface* appCreateVoiceInterface(INT MaxLocalTalkers,INT MaxRemoteTalkers,UBOOL bIsSpeechRecognitionDesired)
{
	return FVoiceInterfaceGameSpy::CreateInstance(MaxLocalTalkers,MaxRemoteTalkers,bIsSpeechRecognitionDesired);
}

/**
 * Creates the capture and playback devices. It first tries to create the
 * capture and playback device as one unit. If that doesn't work it creates
 * the capture device followed by the playback device.
 */
void FVoiceInterfaceGameSpy::CreateDevices(void)
{
	GVDeviceInfo Devices[MAX_CAPTURE_DEVICES];
	// PS3 allows dual devices, whereas the PC does not
#if PS3
	// See if there are any combination devices present
	INT NumDevices = gvListDevices(Devices,MAX_CAPTURE_DEVICES,GV_CAPTURE_AND_PLAYBACK);
	if (NumDevices)
	{
		DumpDevices(Devices,NumDevices,TEXT("capture & playback"));
		// Create one device for both playback and capture
		PlayDevice = CaptureDevice = gvNewDevice(Devices[0].m_id,GV_CAPTURE_AND_PLAYBACK);
		debugf(NAME_DevOnline,TEXT("Using capture & playback device (%s)"),Devices[0].m_name);
	}
	else
#endif
	{
		// Find the default capture device
		INT NumCaptureDevices = gvListDevices(Devices,MAX_CAPTURE_DEVICES,GV_CAPTURE);
		if (NumCaptureDevices)
		{
			DumpDevices(Devices,NumCaptureDevices,TEXT("capture"));
			// The default device appears first
			CaptureDevice = gvNewDevice(Devices[0].m_id,GV_CAPTURE);
			if (CaptureDevice)
			{
				debugf(NAME_DevOnline,TEXT("Using voice capture device (%s)"),Devices[0].m_name);
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Failed to init capture device!"));
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("No capture devices found!"));
		}
		// Find the default playback device
		INT NumPlaybackDevices = gvListDevices(Devices,MAX_CAPTURE_DEVICES,GV_PLAYBACK);
		if (NumPlaybackDevices)
		{
			DumpDevices(Devices,NumPlaybackDevices,TEXT("playback"));
			// The default device appears first
			PlayDevice = gvNewDevice(Devices[0].m_id,GV_PLAYBACK);
			if (PlayDevice)
			{
				debugf(NAME_DevOnline,TEXT("Using voice playback device (%s)"),Devices[0].m_name);
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Failed to init playback device!"));
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("No playback devices found!"));
		}
	}
	// Kick off the playback device
	if (PlayDevice)
	{
		if( !gvStartDevice(PlayDevice,GV_PLAYBACK) )
		{
			debugf(NAME_DevOnline,TEXT("Unable to start playback device!"));
		}
	}
}

/**
 * Creates the XHV engine and performs any other initialization
 *
 * @param MaxLocalTalkers the maximum number of local talkers to support
 * @param MaxRemoteTalkers the maximum number of remote talkers to support
 * @param bIsSpeechRecognitionDesired whether speech recognition should be enabled
 *
 * @return TRUE if everything initialized correctly, FALSE otherwise
 */
UBOOL FVoiceInterfaceGameSpy::Init( INT MaxLocalTalkers, INT MaxRemoteTalkers, UBOOL bIsSpeechRecognitionDesired )
{
	// Ask the INI file if voice is wanted
	if (GConfig->GetBool(TEXT("VoIP"),TEXT("bHasVoiceEnabled"),bHasVoiceEnabled,GEngineIni))
	{
		// Check to see if voice is enabled/disabled
		if (bHasVoiceEnabled == FALSE)
		{
			return FALSE;
		}
	}
	// Skip if a dedicated server
	if (GIsServer)
	{
		debugf(NAME_DevOnline,TEXT("Skipping voice initialization for dedicated server"));
		return FALSE;
	}

	GVBool Result;
#if !_WINDOWS
	bWasStartedOk = Result = gvStartup();
#else
	// Look up the window handle for the game viewport.
	HWND WindowHandle = NULL;
	UWindowsClient* WindowsClient = Cast<UWindowsClient>(GEngine->Client);
	if(WindowsClient)
	{
		if(WindowsClient->Viewports.Num())
		{
			WindowHandle = (HWND)WindowsClient->Viewports(0)->GetWindow();
		}
	}
	bWasStartedOk = Result = gvStartup( WindowHandle );
#endif
	if (!Result)
	{
		debugf( NAME_DevOnline, TEXT( "gvStartup() failed!" ) );
		return FALSE;
	}

	// Default to average codec
	GVCodec CodecChoice = GVCodecAverage;

	// Set the sample rate to 16Khz since speech recognition doesn't work well with 8Khz
	gvSetSampleRate(GVRate_16KHz);

	Result = gvSetCodec(CodecChoice);
	if (Result == GVFalse)
	{
		debugf(NAME_DevOnline,TEXT("gvSetCodec() failed!"));
		return FALSE;
	}

	// Get info about the codec
	gvGetCodecInfo(&SamplesPerFrame,&EncodedFrameSize,&BitsPerSecond);
	debugf(NAME_DevOnline,
		TEXT("Codec Info: SamplesPerFrame (%d), EncodedFrameSize (%d), BitsPerSecond(%d)"),
		SamplesPerFrame,
		EncodedFrameSize,
		BitsPerSecond);

	// Create the capture and playback devices
	CreateDevices();

	if (CaptureDevice)
	{
		gvSetFilter(CaptureDevice,GV_CAPTURE,FilterCallback);
		// Read the threshold from the ini file
		VolumeThreshold = 0.1f;
		GConfig->GetFloat(TEXT("VoIP"),TEXT("VolumeThreshold"),VolumeThreshold,GEngineIni);
		// Set the volume threshold for capturing
		gvSetCaptureThreshold(CaptureDevice,VolumeThreshold);
		debugf(NAME_DevOnline,TEXT("VolumeThreshold is set to %f"),VolumeThreshold);
	}

	return CaptureDevice &&
		FVoiceInterfaceCommon::Init( MaxLocalTalkers, MaxRemoteTalkers, bIsSpeechRecognitionDesired );
}

/**
 * Starts local voice processing for the specified user index
 *
 * @param UserIndex the user to start processing for
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceGameSpy::StartLocalVoiceProcessing(DWORD UserIndex)
{
	if (IsOwningIndex(UserIndex))
	{
		// Start the capture device
		if (CaptureDevice && !gvIsDeviceStarted(CaptureDevice, GV_CAPTURE))
		{
		DWORD StartStatus = E_FAIL;
			if (gvStartDevice( CaptureDevice, GV_CAPTURE ))
		{
				StartStatus = S_OK;
		}
		else
		{
				debugf(NAME_Error,TEXT("StartLocalVoiceProcessing(): Failed to start voice capture device"));
		}
		return StartStatus;
	}
		return S_OK;
	}
	else
	{
		debugf(NAME_Error,TEXT("StartLocalVoiceProcessing(): Device is currently owned by another user"));
	}
	return E_FAIL;
}

/**
 * Stops local voice processing for the specified user index
 *
 * @param UserIndex the user to stop processing of
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceGameSpy::StopLocalVoiceProcessing(DWORD UserIndex)
{
	if (IsOwningIndex(UserIndex))
	{
		// Stop the capture and playback devices
		if (CaptureDevice && gvIsDeviceStarted(CaptureDevice, GV_CAPTURE))
		{
			gvStopDevice( CaptureDevice, GV_CAPTURE );
		}
		return S_OK;
	}
	else
	{
		debugf(NAME_Error,TEXT("StopLocalVoiceProcessing: Ignoring stop request for non-owning user"));
	}
	return E_FAIL;
}

/**
 * Starts remote voice processing for the specified user
 *
 * @param UniqueId the unique id of the user that will be talking
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceGameSpy::StartRemoteVoiceProcessing(FUniqueNetId UniqueId)
{
	return S_OK;
}

/**
 * Stops remote voice processing for the specified user
 *
 * @param UniqueId the unique id of the user that will no longer be talking
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceGameSpy::StopRemoteVoiceProcessing(FUniqueNetId UniqueId)
{
	return S_OK;
}

/**
 * Tells the voice system to start tracking voice data for speech recognition
 *
 * @param UserIndex the local user to recognize voice data for
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceGameSpy::StartSpeechRecognition(DWORD UserIndex)
{
	if (IsOwningIndex(UserIndex))
	{
		if (CaptureDevice)
		{
			gvSetCaptureThreshold(CaptureDevice,0.f);
			StartLocalVoiceProcessing(UserIndex);
		}
		return FVoiceInterfaceCommon::StartSpeechRecognition(UserIndex);
	}
	return E_FAIL;
}

/**
 * Tells the voice system to stop tracking voice data for speech recognition
 *
 * @param UserIndex the local user to recognize voice data for
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceGameSpy::StopSpeechRecognition(DWORD UserIndex)
{
	if (IsOwningIndex(UserIndex))
	{
		if (CaptureDevice)
		{
			gvSetCaptureThreshold(CaptureDevice,VolumeThreshold);
			StopLocalVoiceProcessing(UserIndex);
		}
		return FVoiceInterfaceCommon::StopSpeechRecognition(UserIndex);
	}
	return E_FAIL;
}

/**
 * Registers the user index as a local talker (interested in voice data)
 *
 * @param UserIndex the user index that is going to be a talker
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceGameSpy::RegisterLocalTalker(DWORD UserIndex)
{
	if (OwningIndex == INVALID_OWNING_INDEX)
	{
		OwningIndex = UserIndex;
		return S_OK;
	}
	return E_FAIL;
}

/**
 * Unregisters the user index as a local talker (not interested in voice data)
 *
 * @param UserIndex the user index that is no longer a talker
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceGameSpy::UnregisterLocalTalker(DWORD UserIndex)
{
	if (IsOwningIndex(UserIndex))
	{
		OwningIndex = INVALID_OWNING_INDEX;
		return S_OK;
	}
	return E_FAIL;
}

/**
 * Registers the unique player id as a remote talker (submitted voice data only)
 *
 * @param UniqueId the id of the remote talker
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceGameSpy::RegisterRemoteTalker(FUniqueNetId UniqueId)
{
	return S_OK;
}

/**
 * Unregisters the unique player id as a remote talker
 *
 * @param UniqueId the id of the remote talker
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceGameSpy::UnregisterRemoteTalker(FUniqueNetId UniqueId)
{
	return S_OK;
}

/**
 * Checks whether a local user index has a headset present or not
 *
 * @param UserIndex the user to check status for
 *
 * @return TRUE if there is a headset, FALSE otherwise
 */
UBOOL FVoiceInterfaceGameSpy::IsHeadsetPresent(DWORD UserIndex)
{
	if (IsOwningIndex(UserIndex))
	{
		return CaptureDevice && PlayDevice ? S_OK : E_FAIL;
	}
	return E_FAIL;
}

/**
 * Determines whether a local user index is currently talking or not
 *
 * @param UserIndex the user to check status for
 *
 * @return TRUE if the user is talking, FALSE otherwise
 */
UBOOL FVoiceInterfaceGameSpy::IsLocalPlayerTalking(DWORD UserIndex)
{
	if (IsOwningIndex(UserIndex))
	{
		return CaptureDevice && gvGetAvailableCaptureBytes(CaptureDevice) > 0;
	}
	return FALSE;
}

/**
 * Determines whether a remote talker is currently talking or not
 *
 * @param UniqueId the unique id of the talker to check status on
 *
 * @return TRUE if the user is talking, FALSE otherwise
 */
UBOOL FVoiceInterfaceGameSpy::IsRemotePlayerTalking(FUniqueNetId UniqueId)
{
	return PlayDevice != NULL && gvIsSourceTalking(PlayDevice,UniqueId.ToDWORD());
}

/**
 * Returns which local talkers have data ready to be read from the voice system
 *
 * @return Bit mask of talkers that have data to be read (1 << UserIndex)
 */
DWORD FVoiceInterfaceGameSpy::GetVoiceDataReadyFlags(void)
{
	return OwningIndex != INVALID_OWNING_INDEX &&
		CaptureDevice &&
		gvGetAvailableCaptureBytes(CaptureDevice) > 0 ?
		1 << OwningIndex : 0;
}

/**
 * Sets the playback priority of a remote talker for the given user. A
 * priority of 0xFFFFFFFF indicates that the player is muted. All other
 * priorities sorted from zero being most important to higher numbers
 * being less important.
 *
 * @param UserIndex the local talker that is setting the priority
 * @param UniqueId the id of the remote talker that is having priority changed
 * @param Priority the new priority to apply to the talker
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceGameSpy::SetPlaybackPriority(DWORD UserIndex,FUniqueNetId RemoteTalkerId,DWORD Priority)
{
	return S_OK;
}

/**
 * Reads local voice data for the specified local talker. The size field
 * contains the buffer size on the way in and contains the amount read on
 * the way out
 *
 * @param UserIndex the local talker that is having their data read
 * @param Data the buffer to copy the voice data into
 * @param Size in: the size of the buffer, out: the amount of data copied
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceGameSpy::ReadLocalVoiceData(DWORD UserIndex,BYTE* Data,DWORD* Size)
{
	if (CaptureDevice)
	{
		// Verify that the caller has specified the minimum buffer size
		if (*Size >= EncodedFrameSize + sizeof(GVFrameStamp))
		{
			// Point directly into our buffer to avoid a second copy
			GVByte* Packet = &Data[2];
			GVFrameStamp FrameStamp;
			GVScalar Ignored = 0;
			INT BufferSize = *Size - sizeof(GVFrameStamp);

			// Read the data into temp structures
			if (gvCapturePacket(CaptureDevice,Packet,&BufferSize,&FrameStamp,&Ignored) == GVTrue)
			{
#if VOICE_USE_LOOPBACK
				debugf(TEXT("******* size %d, stamp %d"), BufferSize, FrameStamp);
				gvPlayPacket(PlayDevice,Packet,BufferSize,0,FrameStamp,FALSE);
#endif
				// Now that we have the data in our buffer, encode the frame stamp
				Data[0] = (FrameStamp & 0xFF00) >> 8;
				Data[1] = FrameStamp & 0xFF;
				// Make sure to include our frame stamp encoding
				*Size = BufferSize + sizeof(GVFrameStamp);
				return S_OK;
			}
			return E_FAIL;
		}
		else
		{
			static UBOOL bWasLogged = FALSE;
			if (bWasLogged == FALSE)
			{
				bWasLogged = TRUE;
				debugf(NAME_Error,TEXT("ReadLocalVoiceData: Encoded frame size > than read buffer!"));
			}
		}
	}
	return E_FAIL;
}

/**
 * Submits remote voice data for playback by the voice system. No playback
 * occurs if the priority for this remote talker is 0xFFFFFFFF. Size
 * indicates how much data to submit for processing. It's also an out
 * value in case the system could only process a smaller portion of the data
 *
 * @param RemoteTalkerId the remote talker that sent this data
 * @param Data the buffer to copy the voice data into
 * @param Size in: the size of the buffer, out: the amount of data copied
 *
 * @return 0 upon success, an error code otherwise
 */
DWORD FVoiceInterfaceGameSpy::SubmitRemoteVoiceData(FUniqueNetId RemoteTalkerId,BYTE* Data,DWORD* Size)
{
	if (PlayDevice)
	{
		// Make sure there is enough data (frame stamp plus at least one byte)
		if (*Size > sizeof(GVFrameStamp))
		{
			// Reconstruct the frame stamp from the data buffer
			GVFrameStamp FrameStamp = Data[0] << 8 | Data[1];
			// Queue the data for processing
			gvPlayPacket(PlayDevice,
				&Data[2],
				*Size - sizeof(GVFrameStamp),
				RemoteTalkerId.ToDWORD(),
				FrameStamp,
				FALSE);
			return S_OK;
		}
		else
		{
			// Say we copied no data
			*Size = 0;
		}
	}
	return E_FAIL;
}

/**
 * Voice processing tick function
 */
void FVoiceInterfaceGameSpy::Tick(FLOAT DeltaTime)
{
	gvThink();
}

#endif	//#if WITH_UE3_NETWORKING
