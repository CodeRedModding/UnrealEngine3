/*=============================================================================
	AndroidJni.h: Unreal definitions for Android.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*----------------------------------------------------------------------------
	Platform compiler definitions.
----------------------------------------------------------------------------*/

#ifndef __UE3_ANDROID_JNI_H__
#define __UE3_ANDROID_JNI_H__

extern JavaVM*			GJavaVM;
extern pthread_key_t	GJavaJNIEnvKey;
extern JNIEnv*			GJavaJNIEnv;
extern jobject			GJavaGlobalThiz;
extern jobject			GJavaThiz;

extern UBOOL			GForceStopRendering;
extern UBOOL			GHasStoppedRendering;
extern UBOOL			GUE3HasStartedUp;
extern UBOOL			GHasInterruptionRequest;
extern UBOOL			GPrimaryUE3StartupPhase;
extern UBOOL			GHasConnection;
extern UBOOL			GIsWifi;
extern FLOAT			GWindowScaleFactor;

extern TMap<INT,FName>	KeyMapVirtualToName;

class AAssetManager;

// java calls
void CallJava_HideSplash();
void CallJava_HideReloader();
void CallJava_UpdateFixedSizeScale( float InScale );
void CallJava_StartMovie( const TCHAR* ScreenName );
void CallJava_StopMovie();
void CallJava_AddMovieTextOverlay( const TCHAR* OverlayText );
void CallJava_ShutdownApp();
void CallJava_LaunchURL(const TCHAR* FullURL);
INT  CallJava_GetPerformanceLevel();
FLOAT CallJava_GetResolutionScale();
FString CallJava_GetMainAPKExpansionName();
FString CallJava_GetPatchAPKExpansionName();
void CallJava_OpenSettingsMenu();
INT CallJava_GetDepthSize();
void AndroidUpdateSongPlayer();
UBOOL CallJava_IsExpansionInAPK();
AAssetManager* CallJava_GetAssetManager();
FString CallJava_GetAppCommandLine();
FString CallJava_GetDeviceModel();

// Apsalar
void CallJava_ApsalarInit();
void CallJava_ApsalarStartSession( const TCHAR* ApiKey, const TCHAR* ApiSecret );
void CallJava_ApsalarEndSession();
void CallJava_ApsalarLogStringEvent( const TCHAR* EventName );
void CallJava_ApsalarLogStringEventParam( const TCHAR* EventName, const TCHAR* Name, const TCHAR* Value );
void CallJava_ApsalarLogStringEventParamArray(const TCHAR* EventName,const TArray<struct FEventStringParam>& ParamArray );
void CallJava_ApsalarLogEngineData( const TCHAR* EventName );

#endif // __UE3_ANDROID_H__
