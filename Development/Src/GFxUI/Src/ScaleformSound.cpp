/**********************************************************************

Filename    :   ScaleformSound.cpp
Content     :   Sound support for GFx

Copyright   :   (c) 2001-2007 Scaleform Corp. All Rights Reserved.

Notes       :

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/

#include "GFxUI.h"
#include "ScaleformEngine.h"

#if WITH_GFx_AUDIO || WITH_GFx_VIDEO

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#if WITH_GFx_AUDIO

#include <fmod_errors.h>
#include <fmod.hpp>
#include "Sound/Sound_SoundRendererFMOD.h"
#include "GFx/GFx_Audio.h"

#if defined(SF_OS_PS3)

#include <cell/audio.h>
#include <fmodps3.h>

static FMOD_RESULT InitGFxSoundPS3 ( FMOD::System* pFMOD )
{
	UInt32 port;
	CellAudioPortParam portParam;

	portParam.nChannel = CELL_AUDIO_PORT_8CH;
	portParam.nBlock = CELL_AUDIO_BLOCK_8;
	portParam.attr = CELL_AUDIO_PORTATTR_INITLEVEL;
	portParam.level = 1.0f;
	if ( CELL_OK != cellAudioPortOpen ( &portParam, &port ) )
	{
		debugf ( NAME_Error, TEXT ( "InitGFxSoundPS3: cellAudioPortOpen failed\n" ) );
		return FMOD_ERR_OUTPUT_DRIVERCALL;
	}

	FMOD_PS3_EXTRADRIVERDATA extradriverdata;
	memset ( &extradriverdata, 0, sizeof ( FMOD_PS3_EXTRADRIVERDATA ) );
	extradriverdata.spurs = GSPURS;
	extradriverdata.cell_audio_initialized = 1;
	extradriverdata.cell_audio_port = port;
	pFMOD->setSpeakerMode ( FMOD_SPEAKERMODE_7POINT1 );
	return pFMOD->init ( 64, FMOD_INIT_NORMAL, ( void * ) &extradriverdata );
}
#endif

#endif // WITH_GFx_AUDIO


#if WITH_GFx_VIDEO

// Video system sound interfaces
#if defined(SF_OS_WIN32) || defined(SF_OS_XBOX360)
#include "EngineSoundClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "UnAudioEffect.h"
#include "XAudio2Device.h"
#include "Video/Video_VideoSoundSystemXA2.h"
#elif defined(SF_OS_PS3)
#include "UnPS3.h"
#include "Video/Video_VideoSoundSystemPS3.h"
#endif

#endif // WITH_GFx_VIDEO

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif


#if WITH_GFx_AUDIO

UBOOL FGFxEngine::InitSound()
{
	FMOD::System*    pFMOD;
	FMOD_RESULT      result;
	unsigned int     version;

	result = FMOD::System_Create ( &pFMOD );
	if ( result != FMOD_OK )
	{
		appMsgf ( AMT_OK, FUTF8ToTCHAR ( FMOD_ErrorString ( result ) ) );
		pFMOD = NULL;
		return FALSE;
	}
	result = pFMOD->getVersion ( &version );
	if ( result != FMOD_OK )
	{
		appMsgf ( AMT_OK, FUTF8ToTCHAR ( FMOD_ErrorString ( result ) ) );
		pFMOD->release();
		pFMOD = NULL;
		return FALSE;
	}
	if ( version < FMOD_VERSION )
	{
		appMsgf ( AMT_OK, TEXT ( "fmod version mismatch: was %x expecting %x" ), version, FMOD_VERSION );
		pFMOD->release();
		pFMOD = NULL;
		return FALSE;
	}

#if defined(SF_OS_WIN32)
	FMOD_SPEAKERMODE speakermode;
	FMOD_CAPS        caps;

	result = pFMOD->getDriverCaps ( 0, &caps, 0, &speakermode );
	if ( result != FMOD_OK )
	{
		appMsgf ( AMT_OK, FUTF8ToTCHAR ( FMOD_ErrorString ( result ) ) );
		pFMOD->release();
		pFMOD = NULL;
		return FALSE;
	}

	result = pFMOD->setSpeakerMode ( speakermode );
	if ( result != FMOD_OK )
	{
		appMsgf ( AMT_OK, FUTF8ToTCHAR ( FMOD_ErrorString ( result ) ) );
		pFMOD->release();
		pFMOD = NULL;
		return FALSE;
	}

	if ( caps & FMOD_CAPS_HARDWARE_EMULATED )
	{
		result = pFMOD->setDSPBufferSize ( 1024, 10 );
		if ( result != FMOD_OK )
		{
			appMsgf ( AMT_OK, FUTF8ToTCHAR ( FMOD_ErrorString ( result ) ) );
			pFMOD->release();
			pFMOD = NULL;
			return FALSE;
		}
	}
#endif

#if defined(SF_OS_PS3)
	result = InitGFxSoundPS3 ( pFMOD );
#else
	result = pFMOD->init ( 64, FMOD_INIT_NORMAL, 0 );
#endif

	if ( result == FMOD_ERR_OUTPUT_CREATEBUFFER )
	{
		// Switch it back to stereo...
		result = pFMOD->setSpeakerMode ( FMOD_SPEAKERMODE_STEREO );
		if ( result != FMOD_OK )
		{
			appMsgf ( AMT_OK, FUTF8ToTCHAR ( FMOD_ErrorString ( result ) ) );
			pFMOD->release();
			pFMOD = NULL;
			return FALSE;
		}

		result = pFMOD->init ( 100, FMOD_INIT_NORMAL, 0 );
		if ( result != FMOD_OK )
		{
			appMsgf ( AMT_OK, FUTF8ToTCHAR ( FMOD_ErrorString ( result ) ) );
			pFMOD->release();
			pFMOD = NULL;
			return FALSE;
		}
	}
	else if ( result != FMOD_OK )
	{
		appMsgf ( AMT_OK, FUTF8ToTCHAR ( FMOD_ErrorString ( result ) ) );
		pFMOD->release();
		pFMOD = NULL;
		return FALSE;
	}

    // Create sound renderer
    Sound::SoundRendererFMOD* pSoundFMOD = Sound::SoundRendererFMOD::CreateSoundRenderer();
	pSoundRenderer = *pSoundFMOD;
	if ( !pSoundFMOD->Initialize ( pFMOD ) )
	{
		appMsgf ( AMT_OK, TEXT ( "GFx: Cannot initialize sound system." ) );
		pFMOD->release();
		pFMOD = NULL;
		pSoundRenderer = NULL;
		return FALSE;
	}
	pSoundDevice = pFMOD;

    // Set audio state
	Ptr<Audio> audioState = *SF_NEW Audio ( pSoundRenderer );
	mLoader.SetAudio ( audioState );

	return TRUE;
}

void FGFxEngine::ShutdownSound()
{
	if ( pSoundDevice )
	{
		( ( FMOD::System* ) pSoundDevice )->release();
	}
	pSoundDevice = NULL;
}

#endif // WITH_GFx_AUDIO


#if WITH_GFx_VIDEO

UBOOL FGFxEngine::InitVideoSound ( Video::Video* pvideo )
{
    // Video system sound interfaces
#if defined(SF_OS_WIN32) || defined(SF_OS_XBOX360)
    pvideo->SetSoundSystem ( Ptr<Video::VideoSoundSystem> ( *SF_NEW Video::VideoSoundSystemXA2 (
                             UXAudio2Device::XAudio2, UXAudio2Device::MasteringVoice, Memory::pGlobalHeap ) ) );
#elif defined(SF_OS_PS3)
    pvideo->SetSoundSystem ( Ptr<Video::VideoSoundSystem> ( *SF_NEW Video::VideoSoundSystemPS3 (
                             false, GSPURS, Memory::pGlobalHeap ) ) );
#else
	// Sound renderer interface
#if WITH_GFx_AUDIO
	if ( pSoundRenderer )
	{
		pvideo->SetSoundSystem ( pSoundRenderer );
	}
#endif
#endif

	return TRUE;
}

#endif // WITH_GFx_VIDEO

#endif
