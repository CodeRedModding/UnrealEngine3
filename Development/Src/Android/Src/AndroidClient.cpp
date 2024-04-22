/*=============================================================================
	AndroidClient.cpp: UAndroidClient code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "EngineAudioDeviceClasses.h"
#include "AndroidAudioDevice.h"
#include "AndroidClient.h"

// New andriod native audio device layer
#include "OpenSLAudioDevice.h"

#if WITH_ES2_RHI
INT GScreenWidth;
INT GScreenHeight;
#endif

// Global for the holding the Engine's MaxDeltaTime when it's switched during benchmark mode
FLOAT GSavedMaxDeltaTime =  0;

/*-----------------------------------------------------------------------------
	Class implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UKdClient);

/*-----------------------------------------------------------------------------
	UAndroidClient implementation.
-----------------------------------------------------------------------------*/
extern void CallJava_StartLoadingScreen( const char* ScreenName );
extern void CallJava_StopLoadingScreen();
extern void CallJava_LaunchURL(const TCHAR* FullURL);
extern void CallJava_OpenSettingsMenu();

//
// UAndroidClient constructor.
//
UKdClient::UKdClient()
: Viewport(NULL)
, Engine(NULL)
, AudioDevice(NULL)
{
}

//
// Static init.
//
void UKdClient::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UKdClient, Engine ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( UKdClient, AudioDevice ) );
}

void UKdClient::Init( UEngine* InEngine )
{
	Engine = InEngine;

	// Initialize the audio device.
	if( GEngine->bUseSound )
	{
		extern void *GOPENSL_HANDLE;
		if( GOPENSL_HANDLE )
		{
			AudioDevice = ConstructObject<UOpenSLAudioDevice>( UOpenSLAudioDevice::StaticClass() );
		}
		else
		{
			AudioDevice = ConstructObject<UAndroidAudioDevice>( UAndroidAudioDevice::StaticClass() );
		}

		if( !AudioDevice->Init() )
		{
			AudioDevice = NULL;
		}
	}

	// remove bulk data if no sounds were initialized
	if( AudioDevice == NULL )
	{
		appSoundNodeRemoveBulkData();
	}

	// Success.
	debugf( NAME_Init, TEXT("Client initialized"));
}

//
//	UAndroidClient::Serialize - Make sure objects the client reference aren't garbage collected.
//
void UKdClient::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << Engine << AudioDevice;
}


//
//	UAndroidClient::Destroy - Shut down the platform-specific viewport manager subsystem.
//
void UKdClient::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		if( GFullScreenMovie )
		{
			// force movie playback to stop
			GFullScreenMovie->GameThreadStopMovie(0,FALSE,TRUE);
		}

		// Close all viewports.
		Viewport->Destroy();
		delete Viewport;

		debugf( NAME_Exit, TEXT("Android client shut down") );
	}

	Super::FinishDestroy();
}

//
//	UAndroidClient::Tick - Perform timer-tick processing on all visible viewports.
//
void UKdClient::Tick( FLOAT DeltaTime )
{
	// Process input.
	SCOPE_CYCLE_COUNTER(STAT_InputTime);

	// Check if the user has changed performance settings
	appUpdateFeatureLevelChangeFromMainThread();

	GInputLatencyTimer.GameThreadTick();
	Viewport->ProcessInput(DeltaTime);
}

//
//	UAndroidClient::Exec
//

UBOOL UKdClient::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	if( ParseCommand(&Cmd,TEXT("CALIBRATETILT")) )
	{
		Viewport->CalibrateTilt();
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("TOGGLEFULLSCREEN")) )
	{
		// TODO LMB - Not used on Android yet
		// mark a fullscreen switch as being needed
//		extern UBOOL GAndroidPendingFullscreenToggle;
//		GAndroidPendingFullscreenToggle = TRUE;

		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("MOBILE")) )
	{
		if (ParseCommand(&Cmd, TEXT("PlaySong")))
		{
			// play the music in Java
			AndroidPlaySong(TCHAR_TO_ANSI(Cmd));
			return TRUE;
		}
		else if (ParseCommand(&Cmd, TEXT("StopSong")))
		{
			// stop the music in Java
			AndroidStopSong();
			return TRUE;
		}
		else if ( ParseCommand(&Cmd, TEXT("ABOUT")) )
		{
			FString IniURL;
			// get the URL from the .ini
			if (GConfig->GetString(TEXT("Mobile"), TEXT("AboutURL"), IniURL, GEngineIni))
			{
				// replace the `~ with the param to About
				FString FullURL = FString::Printf( LocalizeSecure( IniURL, Cmd ) );

				// launch the URL in JNI
				CallJava_LaunchURL(*FullURL);
			}

			return TRUE;
		}
		else if ( ParseCommand(&Cmd, TEXT("SettingsMenu")) )
		{
			CallJava_OpenSettingsMenu();
			return TRUE;
		}
		else if ( ParseCommand(&Cmd, TEXT("benchmark")) )
		{
			if ( ParseCommand(&Cmd, TEXT("begin")) )
			{
				GSavedMaxDeltaTime = (CastChecked<UGameEngine>(GEngine))->MaxDeltaTime;
				(CastChecked<UGameEngine>(GEngine))->MaxDeltaTime = 0;
				return TRUE;
			}
			else if ( ParseCommand(&Cmd, TEXT("end")) )
			{
				(CastChecked<UGameEngine>(GEngine))->MaxDeltaTime = GSavedMaxDeltaTime;
				return TRUE;
			}
		}
	}
#if !FINAL_RELEASE
	else if( ParseCommand(&Cmd,TEXT("ANDROID")) )
	{
		if (ParseCommand(&Cmd, TEXT("PF_RED")))
		{
			extern INT GAndroidPF_Red;
			GAndroidPF_Red = appAtoi(Cmd);
		}
		else if (ParseCommand(&Cmd, TEXT("PF_GREEN")))
		{
			extern INT GAndroidPF_Green;
			GAndroidPF_Green = appAtoi(Cmd);
		}
		else if (ParseCommand(&Cmd, TEXT("PF_BLUE")))
		{
			extern INT GAndroidPF_Blue;
			GAndroidPF_Blue = appAtoi(Cmd);
		}
		else if (ParseCommand(&Cmd, TEXT("PF_ALPHA")))
		{
			extern INT GAndroidPF_Alpha;
			GAndroidPF_Alpha = appAtoi(Cmd);
		}
		else if (ParseCommand(&Cmd, TEXT("PF_DEPTH")))
		{
			extern INT GAndroidPF_Depth;
			GAndroidPF_Depth = appAtoi(Cmd);
		}
		else if (ParseCommand(&Cmd, TEXT("PF_STENCIL")))
		{
			extern INT GAndroidPF_Stencil;
			GAndroidPF_Stencil = appAtoi(Cmd);
		}
		else if (ParseCommand(&Cmd, TEXT("PF_COVBUFFERS")))
		{
			extern INT GAndroidPF_SampleBuffers;
			GAndroidPF_SampleBuffers = appAtoi(Cmd);
		}
		else if (ParseCommand(&Cmd, TEXT("PF_COVSAMPLES")))
		{
			extern INT GAndroidPF_SampleSamples;
			GAndroidPF_SampleSamples = appAtoi(Cmd);
		}

		return TRUE;
	}
#endif
	else if( Super::Exec(Cmd,Ar) )
	{
		return TRUE;
	}

	return FALSE;
}

FViewportFrame* UKdClient::CreateViewportFrame(FViewportClient* ViewportClient,const TCHAR* InName,UINT SizeX,UINT SizeY,UBOOL Fullscreen)
{
	check(Viewport == NULL);
	Viewport = new FKdViewport(this, ViewportClient, GScreenWidth, GScreenHeight);
	return Viewport;
}

//
//	UAndroidClient::CreateWindowChildViewport
//
FViewport* UKdClient::CreateWindowChildViewport(FViewportClient* ViewportClient,void* ParentWindow,UINT SizeX,UINT SizeY,INT InPosX, INT InPosY)
{
	check(0);
	return NULL;
}

//
//	UAndroidClient::CloseViewport
//
void UKdClient::CloseViewport(FViewport* Viewport)
{
	FKdViewport* AndroidViewport = (FKdViewport*)Viewport;
	AndroidViewport->Destroy();
}

/**
* Retrieves the name of the key mapped to the specified character code.
*
* @param	KeyCode	the key code to get the name for; should be the key's ANSI value
*/
FName UKdClient::GetVirtualKeyName( INT KeyCode ) const
{
	// @todo android keyboard: can we just copy Windows keys codes?
	return NAME_None;
}


/**
* Initialize registrants, basically calling StaticClass() to create the class and also 
* populating the lookup table.
*
* @param	Lookup	current index into lookup table
*/
void AutoInitializeRegistrantsAndroidDrv( INT& Lookup )
{
	UAndroidAudioDevice::StaticClass();
	UKdClient::StaticClass();

#if WITH_APSALAR
	extern void AutoInitializeRegistrantsApsalarAnalyticsAndroid( INT& Lookup );
	AutoInitializeRegistrantsApsalarAnalyticsAndroid( Lookup );
#endif //WITH_APSALAR
}

/**
* Auto generates names.
*/
void AutoRegisterNamesAndroidDrv()
{
}
