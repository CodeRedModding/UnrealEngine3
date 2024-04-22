/*=============================================================================
	MacClient.cpp: Unreal Mac platform interface
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "MacDrv.h"

/*-----------------------------------------------------------------------------
	Class implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UMacClient);

/*-----------------------------------------------------------------------------
	UMacClient implementation.
-----------------------------------------------------------------------------*/

//
// UMacClient constructor.
//
UMacClient::UMacClient()
: Engine(NULL)
, AudioDeviceClass(NULL)
, AudioDevice(NULL)
{
	KeyMapAppleCodeToName.Set( 0, KEY_A );
	KeyMapAppleCodeToName.Set( 1, KEY_S );
	KeyMapAppleCodeToName.Set( 2, KEY_D );
	KeyMapAppleCodeToName.Set( 3, KEY_F );
	KeyMapAppleCodeToName.Set( 4, KEY_H );
	KeyMapAppleCodeToName.Set( 5, KEY_G );
	KeyMapAppleCodeToName.Set( 6, KEY_Z );
	KeyMapAppleCodeToName.Set( 7, KEY_X );
	KeyMapAppleCodeToName.Set( 8, KEY_C );
	KeyMapAppleCodeToName.Set( 9, KEY_V );
	// 10 is an unsupported key code
	KeyMapAppleCodeToName.Set( 11, KEY_B );
	KeyMapAppleCodeToName.Set( 12, KEY_Q );
	KeyMapAppleCodeToName.Set( 13, KEY_W );
	KeyMapAppleCodeToName.Set( 14, KEY_E );
	KeyMapAppleCodeToName.Set( 15, KEY_R );
	KeyMapAppleCodeToName.Set( 16, KEY_Y );
	KeyMapAppleCodeToName.Set( 17, KEY_T );
	KeyMapAppleCodeToName.Set( 18, KEY_One );
	KeyMapAppleCodeToName.Set( 19, KEY_Two );
	KeyMapAppleCodeToName.Set( 20, KEY_Three );
	KeyMapAppleCodeToName.Set( 21, KEY_Four );
	KeyMapAppleCodeToName.Set( 22, KEY_Six );
	KeyMapAppleCodeToName.Set( 23, KEY_Five );
	KeyMapAppleCodeToName.Set( 24, KEY_Equals );
	KeyMapAppleCodeToName.Set( 25, KEY_Nine );
	KeyMapAppleCodeToName.Set( 26, KEY_Seven );
	KeyMapAppleCodeToName.Set( 27, KEY_Underscore );
	KeyMapAppleCodeToName.Set( 28, KEY_Eight );
	KeyMapAppleCodeToName.Set( 29, KEY_Zero );
	KeyMapAppleCodeToName.Set( 30, KEY_RightBracket );
	KeyMapAppleCodeToName.Set( 31, KEY_O );
	KeyMapAppleCodeToName.Set( 32, KEY_U );
	KeyMapAppleCodeToName.Set( 33, KEY_LeftBracket );
	KeyMapAppleCodeToName.Set( 34, KEY_I );
	KeyMapAppleCodeToName.Set( 35, KEY_P );
	KeyMapAppleCodeToName.Set( 36, KEY_Enter );
	KeyMapAppleCodeToName.Set( 37, KEY_L );
	KeyMapAppleCodeToName.Set( 38, KEY_J );
	KeyMapAppleCodeToName.Set( 39, KEY_Quote );
	KeyMapAppleCodeToName.Set( 40, KEY_K );
	KeyMapAppleCodeToName.Set( 41, KEY_Semicolon );
	KeyMapAppleCodeToName.Set( 42, KEY_Backslash );
	KeyMapAppleCodeToName.Set( 43, KEY_Comma );
	KeyMapAppleCodeToName.Set( 44, KEY_Slash );
	KeyMapAppleCodeToName.Set( 45, KEY_N );
	KeyMapAppleCodeToName.Set( 46, KEY_M );
	KeyMapAppleCodeToName.Set( 47, KEY_Period );
	KeyMapAppleCodeToName.Set( 48, KEY_Tab );
	KeyMapAppleCodeToName.Set( 49, KEY_SpaceBar );
	KeyMapAppleCodeToName.Set( 50, KEY_Tilde );
	KeyMapAppleCodeToName.Set( 51, KEY_BackSpace );
	// unused key code
	KeyMapAppleCodeToName.Set( 53, KEY_Escape );
	// 54 is right Cmd
	// 55 is left Cmd
	KeyMapAppleCodeToName.Set( 56, KEY_LeftShift );
	KeyMapAppleCodeToName.Set( 57, KEY_CapsLock );
	KeyMapAppleCodeToName.Set( 58, KEY_LeftAlt );
	KeyMapAppleCodeToName.Set( 59, KEY_LeftControl );
	KeyMapAppleCodeToName.Set( 60, KEY_RightShift );
	KeyMapAppleCodeToName.Set( 61, KEY_RightAlt );
	KeyMapAppleCodeToName.Set( 62, KEY_RightControl );
	// unused key code
	// unused key code
	KeyMapAppleCodeToName.Set( 65, KEY_Decimal );
	// unused key code
	KeyMapAppleCodeToName.Set( 67, KEY_Multiply );
	// unused key code
	KeyMapAppleCodeToName.Set( 69, KEY_Add );
	// unused key code
	// unused key code
	// unused key code
	// unused key code
	// unused key code
	KeyMapAppleCodeToName.Set( 75, KEY_Divide );
	KeyMapAppleCodeToName.Set( 76, KEY_Enter );	// on numpad
	// unused key code
	KeyMapAppleCodeToName.Set( 78, KEY_Subtract );
	// unused key code
	// unused key code
	KeyMapAppleCodeToName.Set( 81, KEY_NumLock );
	KeyMapAppleCodeToName.Set( 82, KEY_NumPadZero );
	KeyMapAppleCodeToName.Set( 83, KEY_NumPadOne );
	KeyMapAppleCodeToName.Set( 84, KEY_NumPadTwo );
	KeyMapAppleCodeToName.Set( 85, KEY_NumPadThree );
	KeyMapAppleCodeToName.Set( 86, KEY_NumPadFour );
	KeyMapAppleCodeToName.Set( 87, KEY_NumPadFive );
	KeyMapAppleCodeToName.Set( 88, KEY_NumPadSix );
	KeyMapAppleCodeToName.Set( 89, KEY_NumPadSeven );
	// unused key code
	KeyMapAppleCodeToName.Set( 91, KEY_NumPadEight );
	KeyMapAppleCodeToName.Set( 92, KEY_NumPadNine );
	// unused key code
	// unused key code
	// unused key code
	KeyMapAppleCodeToName.Set( 96, KEY_F5 );
	KeyMapAppleCodeToName.Set( 97, KEY_F6 );
	KeyMapAppleCodeToName.Set( 98, KEY_F7 );
	KeyMapAppleCodeToName.Set( 99, KEY_F3 );
	KeyMapAppleCodeToName.Set( 100, KEY_F8 );
	KeyMapAppleCodeToName.Set( 101, KEY_F9 );
	// unused key code
	KeyMapAppleCodeToName.Set( 103, KEY_F11 );
	// unused key code
	// 105 is F13
	// unused key code
	// unused key code
	// unused key code
	KeyMapAppleCodeToName.Set( 109, KEY_F10 );
	// unused key code
	KeyMapAppleCodeToName.Set( 111, KEY_F12 );
	// unused key code
	// unused key code
	// unused key code
	KeyMapAppleCodeToName.Set( 115, KEY_Home );
	KeyMapAppleCodeToName.Set( 116, KEY_PageUp );
	KeyMapAppleCodeToName.Set( 117, KEY_Delete );
	KeyMapAppleCodeToName.Set( 118, KEY_F4 );
	KeyMapAppleCodeToName.Set( 119, KEY_End );
	KeyMapAppleCodeToName.Set( 120, KEY_F2 );
	KeyMapAppleCodeToName.Set( 121, KEY_PageDown );
	KeyMapAppleCodeToName.Set( 122, KEY_F1 );
	KeyMapAppleCodeToName.Set( 123, KEY_Left );
	KeyMapAppleCodeToName.Set( 124, KEY_Right );
	KeyMapAppleCodeToName.Set( 125, KEY_Down );
	KeyMapAppleCodeToName.Set( 126, KEY_Up );
	// unused key code

	// Keys not present on Mac keyboards: Pause, ScrollLock
	// Keys present on Mac keyboard, but not present on PC: ***not a UTF-8 symbol***, right Cmd, left Cmd
}

//
// Static init.
//
void UMacClient::StaticConstructor()
{
	new(GetClass(),TEXT("AudioDeviceClass")	,RF_Public)UClassProperty(CPP_PROPERTY(AudioDeviceClass)	,TEXT("Audio")	,CPF_Config,UAudioDevice::StaticClass());

	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UMacClient, AudioDeviceClass ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( UMacClient, Engine ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( UMacClient, AudioDevice ) );
}

void UMacClient::Init( UEngine* InEngine )
{
	Engine = InEngine;

	// Initialize the audio device.
	if( GEngine->bUseSound )
	{
		AudioDevice = ConstructObject<UAudioDevice>(UCoreAudioDevice::StaticClass());
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
//	UMacClient::Serialize - Make sure objects the client reference aren't garbage collected.
//
void UMacClient::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << Engine << AudioDevice;
}


//
//	UMacClient::Destroy - Shut down the platform-specific viewport manager subsystem.
//
void UMacClient::FinishDestroy()
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

		debugf( NAME_Exit, TEXT("Mac client shut down") );
	}

	Super::FinishDestroy();
}

//
//	UMacClient::Tick - Perform timer-tick processing on all visible viewports.
//
void UMacClient::Tick( FLOAT DeltaTime )
{
	// Process input.
	SCOPE_CYCLE_COUNTER(STAT_InputTime);

	GInputLatencyTimer.GameThreadTick();
	Viewport->ProcessInput(DeltaTime);
}

//
//	UMacClient::Exec
//
UBOOL UMacClient::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	return Super::Exec(Cmd, Ar);
}

FViewportFrame* UMacClient::CreateViewportFrame(FViewportClient* ViewportClient,const TCHAR* InName,UINT SizeX,UINT SizeY,UBOOL Fullscreen)
{
	check(Viewport == NULL);
	Viewport = new FMacViewport(this, ViewportClient, InName, SizeX, SizeY, Fullscreen);
	return Viewport;
}

//
//	UMacClient::CreateWindowChildViewport
//
FViewport* UMacClient::CreateWindowChildViewport(FViewportClient* ViewportClient,void* ParentWindow,UINT SizeX,UINT SizeY,INT InPosX, INT InPosY)
{
	check(0);
	return NULL;
}

//
//	UMacClient::CloseViewport
//
void UMacClient::CloseViewport(FViewport* Viewport)
{
	FMacViewport* MacViewport = (FMacViewport*)Viewport;
	MacViewport->Destroy();
}

/**
 * Retrieves the name of the key mapped to the specified character code.
 *
 * @param	KeyCode	the key code to get the name for; should be the key's ANSI value
 */
FName UMacClient::GetVirtualKeyName( INT KeyCode ) const
{
	return NAME_None;
}

/**
 * Initialize registrants, basically calling StaticClass() to create the class and also 
 * populating the lookup table.
 *
 * @param	Lookup	current index into lookup table
 */
void AutoInitializeRegistrantsMacDrv( INT& Lookup )
{
	UMacClient::StaticClass();
}

/**
 * Auto generates names.
 */
void AutoRegisterNamesMacDrv()
{
}
