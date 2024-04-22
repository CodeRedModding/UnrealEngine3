/*=============================================================================
	PS3Client.cpp: UIPhoneClient code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "EnginePlatformInterfaceClasses.h"
#include "IPhoneDrv.h"
#include "IPhoneObjCWrapper.h"
#include "AudioDeviceIPhone.h"
#import "IPhoneAppDelegate.h"
#import "EAGLView.h"

#if WITH_ES2_RHI
INT GScreenWidth = 0;
INT GScreenHeight = 0;
#endif


/*-----------------------------------------------------------------------------
	UIPhoneClient implementation.
-----------------------------------------------------------------------------*/

//
// UIPhoneClient constructor.
//
UIPhoneClient::UIPhoneClient()
: Engine(NULL)
, AudioDevice(NULL)
{
}

//
// Static init.
//
void UIPhoneClient::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UIPhoneClient, Engine ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( UIPhoneClient, AudioDevice ) );
}

void UIPhoneClient::Init( UEngine* InEngine )
{
	Engine = InEngine;

	// Initialize the audio device.
	if( GEngine->bUseSound )
	{
		AudioDevice = ConstructObject<UAudioDevice>(UAudioDeviceIPhone::StaticClass());
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
//	UIPhoneClient::Serialize - Make sure objects the client reference aren't garbage collected.
//
void UIPhoneClient::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << Engine << AudioDevice;
}


//
//	UIPhoneClient::Destroy - Shut down the platform-specific viewport manager subsystem.
//
void UIPhoneClient::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		if( GFullScreenMovie )
		{
			// force movie playback to stop
			GFullScreenMovie->GameThreadStopMovie(0,FALSE,TRUE);
		}

		// Close all viewports.
		for (INT ViewportIndex = 0; ViewportIndex < Viewports.Num(); ViewportIndex++)
		{
			Viewports(ViewportIndex)->Destroy();
			delete Viewports(ViewportIndex);
		}

		debugf( NAME_Exit, TEXT("PS3 client shut down") );
	}

	Super::FinishDestroy();
}

//
//	UIPhoneClient::Tick - Perform timer-tick processing on all visible viewports.
//
void UIPhoneClient::Tick( FLOAT DeltaTime )
{
	// Process input.
	SCOPE_CYCLE_COUNTER(STAT_InputTime);

	GInputLatencyTimer.GameThreadTick();
	for (INT ViewportIndex = 0; ViewportIndex < Viewports.Num(); ViewportIndex++)
	{
		Viewports(ViewportIndex)->ProcessInput(DeltaTime);
	}
}

//
//	UIPhoneClient::Exec
//
UBOOL UIPhoneClient::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	if( ParseCommand(&Cmd,TEXT("CALIBRATETILT")) )
	{
		for (INT ViewportIndex = 0; ViewportIndex < Viewports.Num(); ViewportIndex++)
		{
			Viewports(ViewportIndex)->CalibrateTilt();
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("IPHONE")) || ParseCommand(&Cmd,TEXT("MOBILE")) )
	{
		if( ParseCommand(&Cmd,TEXT("EnableRotation")) )
		{
			IPhoneSetRotationEnabled(TRUE);
		}
		else if( ParseCommand(&Cmd,TEXT("DisableRotation")) )
		{
			IPhoneSetRotationEnabled(FALSE);
		}
		else if (ParseCommand(&Cmd, TEXT("About")))
		{
			FString IniURL;
			// get the URL from the .ini
			if (GConfig->GetString(TEXT("Mobile"), TEXT("AboutURL"), IniURL, GEngineIni))
			{
				// replace the `~ with the param to About
				FString FullURL = FString::Printf( LocalizeSecure( IniURL, Cmd ) );

				// launch the URL in ObjC
				IPhoneLaunchURL(TCHAR_TO_ANSI(*FullURL), FALSE);
			}
		}
		else if (ParseCommand(&Cmd, TEXT("LaunchURL")))
		{
			// launch any URL through Safari
			FString FullURL = FString::Printf( TEXT("%s"), Cmd );
			IPhoneLaunchURL(TCHAR_TO_ANSI(*FullURL), FALSE);
		}
		else if (ParseCommand(&Cmd, TEXT("LaunchURLWithRedirects")))
		{
			// launch any URL through Safari
			FString FullURL = FString::Printf( TEXT("%s"), Cmd );
			IPhoneLaunchURL(TCHAR_TO_ANSI(*FullURL), TRUE);
		}
		else if (ParseCommand(&Cmd, TEXT("SaveSetting")))
		{
			FString SettingName = ParseToken(Cmd, TRUE);
			FString Value = Cmd;

			// save the setting if we have proper input
			if (SettingName != TEXT("") && Value != TEXT(""))
			{
				IPhoneSaveUserSetting(TCHAR_TO_ANSI(*SettingName), TCHAR_TO_ANSI(*Value));
			}
			else
			{
				debugf(TEXT("Mobile SaveSetting did not receive a setting name and value"));
			}
		}
		else if (ParseCommand(&Cmd, TEXT("LoadSetting")))
		{
			FString SettingName = ParseToken(Cmd, TRUE);
			FString Default = Cmd;

			// load the setting if we got the a setting name
			if (SettingName != TEXT(""))
			{
				ANSICHAR ValString[256];
				IPhoneLoadUserSetting(TCHAR_TO_ANSI(*SettingName), ValString, ARRAY_COUNT(ValString) - 1);

				// if we loaded no string, use the default
				if (ValString[0] == 0)
				{
					Ar.Log(*Default);
				}
				else
				{
					// return the value via the Ar
					Ar.Log(ANSI_TO_TCHAR(ValString));
				}
			}
			else
			{
				debugf(TEXT("Mobile LoadSetting did not receive a setting name"));
			}
		}
		else if (ParseCommand(&Cmd, TEXT("SetMusicVolume")))
		{
			// set the volume in ObjC
			IPhoneScaleMusicVolume(TCHAR_TO_UTF8(Cmd));
		}
		else if (ParseCommand(&Cmd, TEXT("PlaySong")))
		{
			// play the music in ObjC
			IPhonePlaySong(TCHAR_TO_ANSI(Cmd));
		}
		else if (ParseCommand(&Cmd, TEXT("StopSong")))
		{
			// stop the music in ObjC
			IPhoneStopSong();
		}
		else if (ParseCommand(&Cmd, TEXT("PauseSong")))
		{
			// pause the music in ObjC
			IPhonePauseSong();
		}
		else if (ParseCommand(&Cmd, TEXT("ResumeSong")))
		{
			// resume the music in ObjC
			IPhoneResumeSong();
		}
		else if (ParseCommand(&Cmd, TEXT("ResumePreviousSong")))
		{
			// resume the previous music in ObjC
			IPhoneResumePreviousSong();
		}
		else if (ParseCommand(&Cmd, TEXT("DisableSongLooping")))
		{
			// resume the music in ObjC
			IPhoneDisableSongLooping();		
		}
		else if (ParseCommand(&Cmd, TEXT("AppExit")))
		{
			exit(0);
		}
		else if (ParseCommand(&Cmd, TEXT("GetUserInput")))
		{
			FString Title = ParseToken(Cmd, TRUE);
			FString InitValue = ParseToken(Cmd, TRUE);
			FString ExecFunc = ParseToken(Cmd, TRUE);
			FString CancelFunc = ParseToken(Cmd, TRUE);
			FString CharLimit = ParseToken(Cmd, TRUE);

			IPhoneGetUserInput(TCHAR_TO_UTF8(*Title), TCHAR_TO_UTF8(*InitValue), TCHAR_TO_ANSI(*ExecFunc), TCHAR_TO_ANSI(*CancelFunc), TCHAR_TO_UTF8(*CharLimit));
		}
		else if (ParseCommand(&Cmd, TEXT("GetUserInputMulti")))
		{
			FString Title = ParseToken(Cmd, TRUE);
			FString InitValue = ParseToken(Cmd, TRUE);
			FString ExecFunc = ParseToken(Cmd, TRUE);

			IPhoneGetUserInputMulti(TCHAR_TO_UTF8(*Title), TCHAR_TO_UTF8(*InitValue), TCHAR_TO_ANSI(*ExecFunc));
		}		
		else if (ParseCommand(&Cmd, TEXT("DisableSleep")))
		{
			IPhoneSetSleepEnabled(FALSE);
		}
		else if (ParseCommand(&Cmd, TEXT("EnableSleep")))
		{
			IPhoneSetSleepEnabled(TRUE);
		}
		return TRUE;
	}
	else if( Super::Exec(Cmd,Ar) )
	{
		return TRUE;
	}

	return FALSE;
}

#if EXPERIMENTAL_FAST_BOOT_IPHONE
// global pre-made viewport that we hook up to be;low
FIPhoneViewport* GIPhoneViewport=NULL;

/**
 * Make an IPhoneViewport as early as possible (after EAGLView exists and is usable in game thread)
 */
void MakeExperimentalIPhoneViewport(EAGLView* View, FLOAT ScaleFactor)
{
	check(GIPhoneViewport == NULL);

	// mimic the CreateViewportFrame code below
	[View MakeCurrent];

	// make a viewport, which will trigger shader precompiling
	UINT Width = appTrunc(View.bounds.size.width * ScaleFactor);
	UINT Height = appTrunc(View.bounds.size.height * ScaleFactor);
NSLog(@"Premaking a %d x %d viewport", Width, Height);
	GIPhoneViewport = new FIPhoneViewport(NULL, NULL,  Width, Height, View);
}
#endif


FViewportFrame* UIPhoneClient::CreateViewportFrame(FViewportClient* ViewportClient,const TCHAR* InName,UINT SizeX,UINT SizeY,UBOOL Fullscreen)
{
	EAGLView* EAGLViewForViewport;

	// for the first viewport, just subsume the already made GLView from the app delegate
	if (Viewports.Num() == 0)
	{
		EAGLViewForViewport = [IPhoneAppDelegate GetDelegate].GLView;
		// grab the global screen width and height, which could be different than the EAGLView size, depending on content scale factor
		SizeX = GScreenWidth;
		SizeY = GScreenHeight;
	}
	else if (Viewports.Num() == 1)
	{
		EAGLViewForViewport = [IPhoneAppDelegate GetDelegate].SecondaryGLView;
		// grab the size from the view, not what is passed in
		SizeX = EAGLViewForViewport.bounds.size.width;
		SizeY = EAGLViewForViewport.bounds.size.height;
	}
	// @todo iphone: We only handle one external screen here, could there every be more than one external screen? yikes!
	// we could handle it, if we could get the new UIScreen in this function (property in AppDelegate for currently adding screen?)
	else
	{
		return NULL;
	}

	// switch back to this thread
	[EAGLViewForViewport MakeCurrent];
	FIPhoneViewport* Viewport;
#if EXPERIMENTAL_FAST_BOOT_IPHONE
	if (Viewports.Num() == 0)
		{
		check(GIPhoneViewport);

		// hook up to the already made viewport, after setting it's Client and ViewportClient that were NULL
		// when we constructed it
		GIPhoneViewport->SetViewportClient(ViewportClient);
		GIPhoneViewport->Client = this;
		Viewport = GIPhoneViewport;
	}
#else
	Viewport = new FIPhoneViewport(this, ViewportClient, SizeX, SizeY, EAGLViewForViewport);
#endif
	Viewports.AddItem(Viewport);
	return Viewport;
}

//
//	UIPhoneClient::CreateWindowChildViewport
//
FViewport* UIPhoneClient::CreateWindowChildViewport(FViewportClient* ViewportClient,void* ParentWindow,UINT SizeX,UINT SizeY,INT InPosX, INT InPosY)
{
	check(0);
	return NULL;
}

//
//	UIPhoneClient::CloseViewport
//
void UIPhoneClient::CloseViewport(FViewport* Viewport)
{
	FIPhoneViewport* IPhoneViewport = (FIPhoneViewport*)Viewport;
	IPhoneViewport->Destroy();

	// remove the viewport from the list of viewports
	Viewports.RemoveItem(IPhoneViewport);
}

/**
 * Retrieves the name of the key mapped to the specified character code.
 *
 * @param	KeyCode	the key code to get the name for; should be the key's ANSI value
 */
FName UIPhoneClient::GetVirtualKeyName( INT KeyCode ) const
{
	// iphone won't have a keyboard usable at runtime
	return NAME_None;
}


/**
 * IPhone Ad subclass to hook up to iAd functionality in the main thread
 */
class UIPhoneAdManager : public UInGameAdManager
{
	DECLARE_CLASS_INTRINSIC(UIPhoneAdManager, UInGameAdManager, 0, IPhoneDrv)

	/**
	 * Route calls to ObjC land
	 */
	void ShowBanner(UBOOL bShowOnBottomOfScreen)
	{
		IPhoneShowAdBanner(bShowOnBottomOfScreen);
	}
	void HideBanner()
	{
		IPhoneHideAdBanner();
	}
	void ForceCloseAd()
	{
		IPhoneCloseAd();
	}
};




IMPLEMENT_CLASS(UIPhoneClient);
IMPLEMENT_CLASS(UIPhoneAdManager);


/**
 * Initialize registrants, basically calling StaticClass() to create the class and also 
 * populating the lookup table.
 *
 * @param	Lookup	current index into lookup table
 */
void AutoInitializeRegistrantsIPhoneDrv( INT& Lookup )
{
	UIPhoneClient::StaticClass();
	UIPhoneAdManager::StaticClass();
	UAudioDeviceIPhone::StaticClass();


	// facebook class is solely in a .mm file
	extern void AutoInitializeRegistrantsIPhoneFacebook( INT& Lookup );
	AutoInitializeRegistrantsIPhoneFacebook( Lookup );

	// cloud storage class is solely in a .mm file
	extern void AutoInitializeRegistrantsIPhoneCloudStorage( INT& Lookup );
	AutoInitializeRegistrantsIPhoneCloudStorage( Lookup );

	// microtrans storage class is solely in a .mm file
	extern void AutoInitializeRegistrantsIPhoneMicroTransaction( INT& Lookup );
	AutoInitializeRegistrantsIPhoneMicroTransaction( Lookup );

	// microtrans storage class is solely in a .mm file
	extern void AutoInitializeRegistrantsIPhoneTwitterIntegration( INT& Lookup );
	AutoInitializeRegistrantsIPhoneTwitterIntegration( Lookup );

	// push notification handler class is solely in a .mm file
	extern void AutoInitializeRegistrantsAppNotificationsIPhone( INT& Lookup );
	AutoInitializeRegistrantsAppNotificationsIPhone( Lookup );

	extern void AutoInitializeRegistrantsInAppMessageIPhone( INT& Lookup );
	AutoInitializeRegistrantsInAppMessageIPhone( Lookup );

	// Http class is solely in a .mm file
	extern void AutoInitializeRegistrantsHttpIPhone( INT& Lookup );
	AutoInitializeRegistrantsHttpIPhone( Lookup );

#if WITH_APSALAR
	// apsalar analytics class is solely in a .mm file
	extern void AutoInitializeRegistrantsApsalarAnalyticsIPhone( INT& Lookup );
	AutoInitializeRegistrantsApsalarAnalyticsIPhone( Lookup );
#endif //WITH_APSALAR

#if WITH_SWRVE
	// swrve analytics class is solely in a .mm file
	extern void AutoInitializeRegistrantsSwrveAnalyticsIPhone( INT& Lookup );
	AutoInitializeRegistrantsSwrveAnalyticsIPhone( Lookup );
#endif //WITH_SWRVE
}

/**
 * Auto generates names.
 */
void AutoRegisterNamesIPhoneDrv()
{
}











/*

void TestClassSize()
{
	VERIFY_CLASS_SIZE(UObject);
	VERIFY_CLASS_SIZE(UComponent);

//	VERIFY_CLASS_OFFSET(UActorComponent, ActorComponent, Scene);
	VERIFY_CLASS_OFFSET(UActorComponent, ActorComponent, TickGroup);

	debugf(TEXT("ActorCOmponent C++: %d, Script: %d, AlignedScript: %d, MinAlign: %d"),
		sizeof(UActorComponent), UActorComponent::StaticClass()->GetPropertiesSize(),
		Align(UActorComponent::StaticClass()->GetPropertiesSize(), UActorComponent::StaticClass()->GetMinAlignment()),
		UActorComponent::StaticClass()->GetMinAlignment());
		VERIFY_CLASS_SIZE(UActorComponent);

	VERIFY_CLASS_SIZE(UPrimitiveComponent);
	VERIFY_CLASS_SIZE(UDrawBoxComponent);
	VERIFY_CLASS_OFFSET(UDrawBoxComponent, DrawBoxComponent, BoxExtent);
	VERIFY_CLASS_SIZE(UDrawCapsuleComponent);
	VERIFY_CLASS_OFFSET(UDrawCapsuleComponent, DrawCapsuleComponent, CapsuleRadius);
}
*/
