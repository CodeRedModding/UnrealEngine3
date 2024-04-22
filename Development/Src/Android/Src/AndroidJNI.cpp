/*=============================================================================
	AndroidJNI.cpp: Android main platform glue definitions
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include <jni.h>
#include <pthread.h>
#include <semaphore.h>
#include <android/log.h>
#include <cpu-features.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "EnginePrivate.h"
#include "Android.h"
#include "AndroidInput.h"
#include "LaunchPrivate.h"
#include "ES2RHIDebug.h"
#include "FFileManagerAndroid.h"
#include "ES2RHIPrivate.h"
#include "EnginePlatformInterfaceClasses.h"

#define MODULE "UE3"
#define MEMORY_MOVIE_CUTOFF 90

//
static pthread_t GNativeMainThread;

//
FString GAndroidRootPath;

// state of gpu and startup
UBOOL GForceStopRendering		= FALSE;
UBOOL GPUStateActive			= TRUE;
UBOOL GPrimaryUE3StartupPhase	= FALSE;
UBOOL GUE3HasStartedUp			= FALSE;
UBOOL GHasInterruptionRequest	= FALSE;
UBOOL GMainThreadIsLoaded		= TRUE;
//
UBOOL GHasConnection			= FALSE;
UBOOL GIsWifi					= TRUE;
FLOAT GWindowScaleFactor		= 1.0f;
UBOOL GMainThreadExit			= FALSE;

//
TMap<INT,FName>	KeyMapVirtualToName;

//
JavaVM*			GJavaVM				= NULL;
pthread_key_t	GJavaJNIEnvKey		= NULL;
JNIEnv*			GJavaJNIEnv			= NULL;
jobject			GJavaGlobalThiz		= NULL;
jobject			GJavaThiz			= NULL;
jobject			GJavaAssetManager	= NULL;

AAssetManager*	GAssetManagerRef	= NULL;

/** Global vars for setting pixel formats at runtime */
INT GAndroidPF_Red				= 8;
INT GAndroidPF_Green			= 8;
INT GAndroidPF_Blue				= 8;
INT GAndroidPF_Alpha			= 8;
INT GAndroidPF_Depth			= 24;
INT GAndroidPF_Stencil			= 0;
INT GAndroidPF_SampleBuffers	= 0;
INT GAndroidPF_SampleSamples	= 0;

static jmethodID		GMethod_Swap;
static jmethodID		GMethod_MakeCurrent;
static jmethodID		GMethod_UnmakeCurrent;
static jmethodID		GMethod_InitEGL;
static jmethodID		GMethod_HasAppLocalValue;
static jmethodID		GMethod_GetAppLocalValue;
static jmethodID		GMethod_SetAppLocalValue;
static jmethodID		GMethod_AndroidPlaySong;
static jmethodID		GMethod_AndroidStopSong;
static jmethodID		GMethod_AndroidUpdateSongPlayer;
static jmethodID		GMethod_StartMovie;
static jmethodID		GMethod_StopMovie;
static jmethodID		GMethod_IsMoviePlaying;
static jmethodID		GMethod_VideoAddTextOverlay;
static jmethodID		GMethod_ShowKeyboard;
static jmethodID		GMethod_HideKeyboard;
static jmethodID		GMethod_HideSplash;
static jmethodID		GMethod_HideReloader;
static jmethodID		GMethod_ShowWebPage;
static jmethodID		GMethod_SetFixedSizeScale;
static jmethodID		GMethod_ShowExitDialog;
static jmethodID		GMethod_ShutdownApp;
static jmethodID		GMethod_GetPerformanceLevel;
static jmethodID		GMethod_GetResolutionScale;
static jmethodID		GMethod_GetMainAPKExpansionName;
static jmethodID		GMethod_GetPatchAPKExpansionName;
static jmethodID		GMethod_OpenSettingsMenu;
static jmethodID		GMethod_GetDepthSize;
static jmethodID		GMethod_IsExpansionInAPK;
static jmethodID		GMethod_GetFileFromAPKAssets;
static jmethodID		GMethod_GetAssetManager;
static jmethodID		GMethod_GetAppCommandLine;
static jmethodID		GMethod_GetDeviceModel;
//Apsalar
static jmethodID		GMethod_ApsalarInit;
static jmethodID		GMethod_ApsalarStartSession;
static jmethodID		GMethod_ApsalarEndSession;
static jmethodID		GMethod_ApsalarLogStringEvent;
static jmethodID		GMethod_ApsalarLogStringEventParam;
static jmethodID		GMethod_ApsalarLogStringEventParamArray;
static jmethodID		GMethod_ApsalarSetReportLocation;
static jmethodID		GMethod_ApsalarLogEngineData;

struct JNIJavaMethods
{
	jmethodID *JavaMethod;
	const ANSICHAR *FunctionName;
	const ANSICHAR *FunctionParams;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// JNI ENVIRONMENTS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UBOOL RegisterThreadContextForEGL(JNIEnv* InJNIEnv)
{
	if (pthread_setspecific(GJavaJNIEnvKey, InJNIEnv))
	{
		appOutputDebugStringf("Could not set TLS for JNI Env"); 
		return false;
	}
	return true;
}

UBOOL DestroyingRegisteredObjectInstance()
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: DestroyingRegisteredObjectInstance no TLS data!");
	}

	LocalJNIEnv->DeleteGlobalRef(GJavaGlobalThiz);
	GJavaGlobalThiz = NULL;

	//appOutputDebugStringf("Released global thiz!");
	return true;
}

// Called at the outset of any function that has been called directly through the
// JNI and thus is already connected to the JVM prior to invocation
UBOOL RegisterJNIThreadForEGL(JNIEnv* InJNIEnv, jobject InJavaThiz)
{
	GJavaJNIEnv = InJNIEnv;
	GJavaThiz = InJavaThiz;

	if (!GJavaGlobalThiz)
	{
		GJavaGlobalThiz = InJNIEnv->NewGlobalRef(InJavaThiz);
		if (!GJavaGlobalThiz)
		{
			appOutputDebugStringf("Error: Thiz NewGlobalRef failed!");
			return false;
		}
		//appOutputDebugStringf("Thiz NewGlobalRef: 0x%p", GJavaGlobalThiz);
	}

	if (!GJavaJNIEnvKey && pthread_key_create(&GJavaJNIEnvKey, NULL))
	{
		appOutputDebugStringf("Could not create TLS for JNI Env");        
		return false;
	}
	else if (!RegisterThreadContextForEGL(InJNIEnv))
	{
		appOutputDebugStringf("Could not set main-thread TLS for JNI Env"); 
		return false;
	}

	//appOutputDebugStringf("RegisterJNIThreadForEGL: success");
	return true;
}

// Called at the outset of any thread function for threads created within the native
// and thus are not automatically connected to the JVM prior to invocation
UBOOL RegisterSecondaryThreadForEGL()
{
	JNIEnv* LocalJNIEnv = NULL;
	if (!GJavaVM)
	{
		appOutputDebugStringf("Error: RegisterSecondaryThreadForEGL no global JVM ptr available");
		return false;
	}

	INT Error = GJavaVM->AttachCurrentThread(&LocalJNIEnv, NULL);
	//appOutputDebugStringf("AttachCurrentThread: %d, 0x%p", Error, LocalJNIEnv);
	if (Error || !LocalJNIEnv)
	{
		appOutputDebugStringf("AttachCurrentThread: %d, 0x%p", Error, LocalJNIEnv);
		appOutputDebugStringf("Error - could not attach thread to JVM!");
		return false;
	}

	return RegisterThreadContextForEGL(LocalJNIEnv);
}

UBOOL UnRegisterSecondaryThreadFromEGL()
{
	// We MUST MUST detach the thread before exiting, otherwise the whole app goes down in flames. Yay.
	return GJavaVM->DetachCurrentThread() ? false : true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// NATIVE -> JAVA CALLBACKS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FString getLocalAppValue(const char* LocalAppKey)
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in getLocalAppValue");
	}
	else
	{
		jstring LocalAppKeyJava = LocalJNIEnv->NewStringUTF(LocalAppKey);
		//appOutputDebugStringf("In call to getLocalAppValue");
		jstring LocalAppValueJava = (jstring)LocalJNIEnv->CallObjectMethod(GJavaGlobalThiz, GMethod_GetAppLocalValue, LocalAppKeyJava);

		jboolean bIsCopy;
		const char* ReturnString = LocalJNIEnv->GetStringUTFChars(LocalAppValueJava, &bIsCopy);
		//appOutputDebugStringf("getLocalAppValue: (%s, %s)", LocalAppKey, string);
		FString ReturnValue(ReturnString);
		LocalJNIEnv->ReleaseStringUTFChars(LocalAppValueJava, ReturnString);
		
		// clean up references
		LocalJNIEnv->DeleteLocalRef(LocalAppValueJava);
		LocalJNIEnv->DeleteLocalRef(LocalAppKeyJava);
		
		return ReturnValue;
	}

	return FString();
}

bool hasLocalAppValue(const char* LocalAppKey)
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in getLocalAppValue");
		return false;
	}
	else
	{
		jstring LocalAppKeyJava = LocalJNIEnv->NewStringUTF(LocalAppKey);
		jboolean bHasIt = LocalJNIEnv->CallBooleanMethod(GJavaGlobalThiz, GMethod_HasAppLocalValue, LocalAppKeyJava);

		// clean up references
		LocalJNIEnv->DeleteLocalRef(LocalAppKeyJava);

		return (bHasIt == JNI_TRUE) ? true : false;
	}
}

void CallJava_StartMovie( const TCHAR* MovieName )
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);

	FString MoviePath = GFileManager->ConvertToAbsolutePath( *FString::Printf(TEXT("%sMovies%s%s.mp4"), 
		*appGameDir(), PATH_SEPARATOR, MovieName) );

	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in GMethod_StartMovie");
	}
	else
	{
		jclass FileDescriptorClass = LocalJNIEnv->FindClass("java/io/FileDescriptor");

		// Get the file descriptor from the file manager (will retrieve from expansion if found)
		FFileManagerAndroid* AndroidFileManager = static_cast<FFileManagerAndroid*>(GFileManager);

		SQWORD FileOffset = 0;
		SQWORD FileLength = 0;
		int FileDescriptor =  AndroidFileManager->GetFileHandle(*MoviePath, FileOffset, FileLength);

		// Build the java descriptor
		jmethodID FDConstructorMethod = LocalJNIEnv->GetMethodID(FileDescriptorClass, "<init>", "()V");
		jobject FileDescriptorObject = LocalJNIEnv->NewObject(FileDescriptorClass, FDConstructorMethod);

		// assign native descriptor into object
		jfieldID FileDescriptorField = LocalJNIEnv->GetFieldID(FileDescriptorClass, "descriptor", "I");
		LocalJNIEnv->SetIntField(FileDescriptorObject, FileDescriptorField, FileDescriptor);

		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_StartMovie, FileDescriptorObject, FileOffset, FileLength);

		// clean up references
		LocalJNIEnv->DeleteLocalRef(FileDescriptorObject);
		LocalJNIEnv->DeleteLocalRef(FileDescriptorClass);
	}
}

void CallJava_StopMovie( )
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in GMethod_StopMovie");
	}
	else
	{
		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_StopMovie);
	}
}

void CallJava_AddMovieTextOverlay( const TCHAR* OverlayText )
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in GMethod_StopMovie");
	}
	else
	{
		jstring OverlayTextJava = LocalJNIEnv->NewStringUTF( TCHAR_TO_ANSI(OverlayText) );

		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_VideoAddTextOverlay, OverlayTextJava);

		// clean up references
		LocalJNIEnv->DeleteLocalRef(OverlayTextJava);
	}
}

void CallJava_ApsalarInit()
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in GMethod_ApsalarInit");
	}
	else
	{
		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_ApsalarInit);
	}
}

void CallJava_ApsalarStartSession( const TCHAR* ApiKey, const TCHAR* ApiSecret )
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in GMethod_ApsalarStartSession");
	}
	else
	{
		jstring ApiKeyJava = LocalJNIEnv->NewStringUTF( TCHAR_TO_ANSI(ApiKey) );
		jstring ApiSecretJava = LocalJNIEnv->NewStringUTF( TCHAR_TO_ANSI(ApiSecret) );

		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_ApsalarStartSession, ApiKeyJava, ApiSecretJava);

		// clean up references
		LocalJNIEnv->DeleteLocalRef(ApiKeyJava);
		LocalJNIEnv->DeleteLocalRef(ApiSecretJava);
	}
}

void CallJava_ApsalarEndSession()
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in GMethod_ApsalarEndSession");
	}
	else
	{
		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_ApsalarEndSession);
	}
}

void CallJava_ApsalarLogStringEvent( const TCHAR* EventName )
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in GMethod_ApsalarLogStringEvent");
	}
	else
	{
		jstring EventNameJava = LocalJNIEnv->NewStringUTF( TCHAR_TO_ANSI(EventName) );

		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_ApsalarLogStringEvent, EventNameJava);

		// clean up references
		LocalJNIEnv->DeleteLocalRef(EventNameJava);
	}
}

void CallJava_ApsalarLogStringEventParam( const TCHAR* EventName, const TCHAR* ParamName, const TCHAR* ParamValue )
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in GMethod_ApsalarLogStringEventParam");
	}
	else
	{
		jstring EventNameJava = LocalJNIEnv->NewStringUTF( TCHAR_TO_ANSI(EventName) );
		jstring ParamNameJava = LocalJNIEnv->NewStringUTF( TCHAR_TO_ANSI(ParamName) );
		jstring ParamValueJava = LocalJNIEnv->NewStringUTF( TCHAR_TO_ANSI(ParamValue) );

		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_ApsalarLogStringEventParam, EventNameJava, ParamNameJava, ParamValueJava);

		// clean up references
		LocalJNIEnv->DeleteLocalRef(EventNameJava);
		LocalJNIEnv->DeleteLocalRef(ParamNameJava);
		LocalJNIEnv->DeleteLocalRef(ParamValueJava);
	}
}

void CallJava_ApsalarLogStringEventParamArray(const TCHAR* EventName,const TArray<struct FEventStringParam>& ParamArray )
{
#if WITH_APSALAR
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);

	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in GMethod_ApsalarLogStringEventParamArray");
	}
	else
	{
		jobjectArray ParamArrayJava = (jobjectArray) LocalJNIEnv->NewObjectArray(ParamArray.Num() * 2, LocalJNIEnv->FindClass("java/lang/String"), LocalJNIEnv->NewStringUTF(""));
		for (UINT Param = 0; Param < ParamArray.Num(); Param += 2)
		{
			LocalJNIEnv->SetObjectArrayElement(ParamArrayJava, Param, LocalJNIEnv->NewStringUTF(TCHAR_TO_ANSI(*ParamArray(Param).ParamName)));
			LocalJNIEnv->SetObjectArrayElement(ParamArrayJava, Param + 1, LocalJNIEnv->NewStringUTF(TCHAR_TO_ANSI(*ParamArray(Param).ParamValue)));
		}

		jstring EventNameJava = LocalJNIEnv->NewStringUTF( TCHAR_TO_ANSI(EventName) );
		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_ApsalarLogStringEventParamArray, EventNameJava, ParamArrayJava);

		// clean up references
		for (UINT Param = 0; Param < ParamArray.Num(); ++Param)
		{
			jobject TargetObject = LocalJNIEnv->GetObjectArrayElement(ParamArrayJava, Param);
			LocalJNIEnv->DeleteLocalRef(TargetObject);
		}
		LocalJNIEnv->DeleteLocalRef(ParamArrayJava);
		LocalJNIEnv->DeleteLocalRef(EventNameJava);
	}
#endif
}

void CallJava_ApsalarLogEngineData(const TCHAR* EventName)
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);

	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in GMethod_ApsalarLogEngineData");
	}
	else
	{
		jstring EventNameJava = LocalJNIEnv->NewStringUTF( TCHAR_TO_ANSI(EventName) );
		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_ApsalarLogEngineData, EventNameJava, GEngineVersion);

		// clean up references
		LocalJNIEnv->DeleteLocalRef(EventNameJava);
	}
}

////////////////////////////////////
/// Function: 
///    CallJava_HideSplash
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void CallJava_HideSplash()
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in CallJava_HideKeyboard");
	}
	else
	{
		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_HideSplash);
	}
}

////////////////////////////////////
/// Function: 
///    CallJava_HideReloader
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void CallJava_HideReloader()
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in CallJava_HideKeyboard");
	}
	else
	{
		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_HideReloader);
	}
}

////////////////////////////////////
/// Function: 
///    CallJava_ShutdownApp
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    Notify Java that we need to close the application
///    
void CallJava_ShutdownApp()
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific( GJavaJNIEnvKey );
	if ( !LocalJNIEnv || !GJavaGlobalThiz )
	{
		appOutputDebugStringf("Error: No valid JNI env in CallJava_ShutdownApp" );
	}
	else
	{
		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_ShutdownApp);
	}
}

////////////////////////////////////
/// Function: 
///    CallJava_UpdateFixedSizeScale
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void CallJava_UpdateFixedSizeScale( float InScale )
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);

	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in CallJava_UpdateFixedSizeScale");
	}
	else
	{
		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_SetFixedSizeScale, InScale);
	}
}


////////////////////////////////////
/// Function: 
///    CallJava_OpenAboutPage
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    Callback to open the About web page in java
///    
////////////////////////////////////
void CallJava_LaunchURL(const TCHAR* FullURL)
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);

	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in CallJava_OpenAboutPage");
	}
	else
	{
		jstring FullURLJava = LocalJNIEnv->NewStringUTF(FullURL);
		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_ShowWebPage, FullURLJava);

		// clean up references
		LocalJNIEnv->DeleteLocalRef(FullURLJava);
	}
}

////////////////////////////////////
/// Function: 
///    CallJava_GetPerformanceLevel
///    
/// Specifiers: 
///    [INT] - The performance level
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    Get the performance level from shared preferences in java
///    
////////////////////////////////////
INT CallJava_GetPerformanceLevel()
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	jint PerfLevel = 0;

	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in CallJava_GetPerformanceLevel");
	}
	else
	{
		PerfLevel = LocalJNIEnv->CallIntMethod(GJavaGlobalThiz, GMethod_GetPerformanceLevel);
	}

	return (INT)PerfLevel;
}


////////////////////////////////////
/// Function: 
///    CallJava_GetResolutionScale
///    
/// Specifiers: 
///    [FLOAT] - The resolution scale
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    Get the resolution scale from shared preferences in java
///    
////////////////////////////////////
FLOAT CallJava_GetResolutionScale()
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	jfloat ResolutionScale = 0;

	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in CallJava_GetResolutionScale");
	}
	else
	{
		ResolutionScale = LocalJNIEnv->CallFloatMethod(GJavaGlobalThiz, GMethod_GetResolutionScale);
	}

	return (FLOAT)ResolutionScale;
}

void CallJava_OpenSettingsMenu()
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);

	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in CallJava_OpenSettingsMenu");
	}
	else
	{
		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_OpenSettingsMenu);
	}
}

INT CallJava_GetDepthSize()
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);

	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in CallJava_OpenSettingsMenu");
	}
	else
	{
		return LocalJNIEnv->CallIntMethod(GJavaGlobalThiz, GMethod_GetDepthSize);
	}
}

UBOOL CallJava_IsExpansionInAPK ()
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);

	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in CallJava_IsExpansionInAPK");
	}
	else
	{
		return LocalJNIEnv->CallBooleanMethod(GJavaGlobalThiz, GMethod_IsExpansionInAPK);
	}
}

AAssetManager* CallJava_GetAssetManager ()
{
	if (GAssetManagerRef)
	{
		return GAssetManagerRef;
	}
	
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);

	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in CallJava_GetAssetManager");
	}
	else
	{
		GJavaAssetManager = LocalJNIEnv->CallObjectMethod(GJavaGlobalThiz, GMethod_GetAssetManager);
		LocalJNIEnv->NewGlobalRef(GJavaAssetManager);
		GAssetManagerRef = AAssetManager_fromJava(LocalJNIEnv, GJavaAssetManager);
		return GAssetManagerRef;
	}
}

////////////////////////////////////
/// Function: 
///    AndroidPlaySong
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void AndroidPlaySong( const TCHAR* SongName )
{	
	FString SongPath = GFileManager->ConvertToAbsolutePath( *FString::Printf(TEXT("%sMusic%s%s.mp3"), 
		*appGameDir(), PATH_SEPARATOR, SongName) );

	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in AndroidPlaySong");
	}
	else
	{
		jclass FileDescriptorClass = LocalJNIEnv->FindClass("java/io/FileDescriptor");

		// Get the file descriptor from the file manager (will retrieve from expansion if found)
		FFileManagerAndroid* AndroidFileManager = static_cast<FFileManagerAndroid*>(GFileManager);

		SQWORD FileOffset = 0;
		SQWORD FileLength = 0;
		int FileDescriptor =  AndroidFileManager->GetFileHandle(*SongPath, FileOffset, FileLength);

		// Build the java descriptor
		jmethodID FDConstructorMethod = LocalJNIEnv->GetMethodID(FileDescriptorClass, "<init>", "()V");
		jobject FileDescriptorObject = LocalJNIEnv->NewObject(FileDescriptorClass, FDConstructorMethod);

		// assign native descriptor into object
		jfieldID FileDescriptorField = LocalJNIEnv->GetFieldID(FileDescriptorClass, "descriptor", "I");
		LocalJNIEnv->SetIntField(FileDescriptorObject, FileDescriptorField, FileDescriptor);

		// convert the songName so that the player can track it
		jstring SongNameJava = LocalJNIEnv->NewStringUTF(TCHAR_TO_ANSI(SongName));

		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_AndroidPlaySong, FileDescriptorObject, FileOffset, FileLength, SongNameJava);

		// clean up references
		LocalJNIEnv->DeleteLocalRef(SongNameJava);
		LocalJNIEnv->DeleteLocalRef(FileDescriptorObject);
		LocalJNIEnv->DeleteLocalRef(FileDescriptorClass);
	}
}

////////////////////////////////////
/// Function: 
///    AndroidStopSong
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [void] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void AndroidStopSong()
{
	appOutputDebugStringf("Called AndroidStopSong()");
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in AndroidStopSong");
	}
	else
	{
		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_AndroidStopSong);
	}
}

////////////////////////////////////
/// Function: 
///    AndroidUpdateSongPlayer
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [float] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void AndroidUpdateSongPlayer()
{
	static DOUBLE previousUpdateTime = appSeconds();
	DOUBLE currentUpdateTime = appSeconds();

	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in AndroidStopSong");
	}
	else
	{
		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_AndroidUpdateSongPlayer, (FLOAT) (currentUpdateTime - previousUpdateTime));
	}

	previousUpdateTime = currentUpdateTime;
}

void setLocalAppValue(const char* LocalAppKey, const char* LocalAppValue)
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in getLocalAppValue");
	}
	else
	{
		jstring LocalAppKeyJava = LocalJNIEnv->NewStringUTF(LocalAppKey);
		jstring LocalAppValueJava = LocalJNIEnv->NewStringUTF(LocalAppValue);
		//appOutputDebugStringf("In call to setLocalAppValue");
		LocalJNIEnv->CallVoidMethod(GJavaGlobalThiz, GMethod_SetAppLocalValue, LocalAppKeyJava, LocalAppValueJava);

		// clean up references
		LocalJNIEnv->DeleteLocalRef(LocalAppValueJava);
		LocalJNIEnv->DeleteLocalRef(LocalAppKeyJava);
	}
}

#if WITH_ES2_RHI

/**
 * Fill the ES2 viewport with information about the EAGL View it represents
 */
void PlatformInitializeViewport(FES2Viewport* Viewport, void* WindowHandle)
{

}

/**
 * Tear down the viewport
 */
void PlatformDestroyViewport(FES2Viewport* Viewport)
{

}

void PlatformMakeCurrent(FES2Viewport* Viewport)
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in MakeCurrent");
	}
	else if (!LocalJNIEnv->CallBooleanMethod(GJavaGlobalThiz, GMethod_MakeCurrent))
	{
		appOutputDebugStringf("Error: MakeCurrent failed");
	}
}

void PlatformUnmakeCurrent(FES2Viewport* Viewport)
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in UnMakeCurrent");
	}
	else if (!LocalJNIEnv->CallBooleanMethod(GJavaGlobalThiz, GMethod_UnmakeCurrent))
	{
		appOutputDebugStringf("Error: UnMakeCurrent failed");
	}
}

void PlatformSwapBuffers(FES2Viewport* Viewport)
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in SwapBuffers");
	}
	else if (!LocalJNIEnv->CallBooleanMethod(GJavaGlobalThiz, GMethod_Swap))
	{
		appOutputDebugStringf("Error: SwapBuffers failed");
	}
}

#endif

FString CallJava_GetMainAPKExpansionName()
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in getLocalAppValue");
	}
	else
	{
		jstring MainAPKExpansionName = (jstring)LocalJNIEnv->CallObjectMethod(GJavaGlobalThiz, GMethod_GetMainAPKExpansionName);
		
		jboolean bIsCopy;
		const char* ReturnString = LocalJNIEnv->GetStringUTFChars(MainAPKExpansionName, &bIsCopy);

		FString ReturnValue(ReturnString);
		LocalJNIEnv->ReleaseStringUTFChars(MainAPKExpansionName, ReturnString);

		// clean up references
		LocalJNIEnv->DeleteLocalRef(MainAPKExpansionName);

		return ReturnValue;
	}

	return FString();
}

FString CallJava_GetPatchAPKExpansionName()
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in getLocalAppValue");
	}
	else
	{
		jstring PatchAPKExpansionName = (jstring)LocalJNIEnv->CallObjectMethod(GJavaGlobalThiz, GMethod_GetPatchAPKExpansionName);

		jboolean bIsCopy;
		const char* ReturnString = LocalJNIEnv->GetStringUTFChars(PatchAPKExpansionName, &bIsCopy);

		FString ReturnValue(ANSI_TO_TCHAR(ReturnString));
		LocalJNIEnv->ReleaseStringUTFChars(PatchAPKExpansionName, ReturnString);

		// clean up references
		LocalJNIEnv->DeleteLocalRef(PatchAPKExpansionName);

		return ReturnValue;
	}

	return FString();
}

FString CallJava_GetAppCommandLine()
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in getLocalAppValue");
	}
	else
	{
		jstring CommandLine = (jstring)LocalJNIEnv->CallObjectMethod(GJavaGlobalThiz, GMethod_GetAppCommandLine);

		jboolean bIsCopy;
		const char* ReturnString = LocalJNIEnv->GetStringUTFChars(CommandLine, &bIsCopy);

		FString ReturnValue(ANSI_TO_TCHAR(ReturnString));
		LocalJNIEnv->ReleaseStringUTFChars(CommandLine, ReturnString);

		// clean up references
		LocalJNIEnv->DeleteLocalRef(CommandLine);

		return ReturnValue;
	}

	return TEXT("");
}

FString CallJava_GetDeviceModel()
{
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);
	if (!LocalJNIEnv || !GJavaGlobalThiz)
	{
		appOutputDebugStringf("Error: No valid JNI env in getLocalAppValue");
	}
	else
	{
		jstring ModelName = (jstring)LocalJNIEnv->CallObjectMethod(GJavaGlobalThiz, GMethod_GetDeviceModel);

		jboolean bIsCopy;
		const char* ReturnString = LocalJNIEnv->GetStringUTFChars(ModelName, &bIsCopy);

		FString ReturnValue(ANSI_TO_TCHAR(ReturnString));
		LocalJNIEnv->ReleaseStringUTFChars(ModelName, ReturnString);

		// clean up references
		LocalJNIEnv->DeleteLocalRef(ModelName);

		return ReturnValue;
	}

	return TEXT("");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// JAVA -> NATIVE CALLBACKS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////
/// Function: 
///    NativeCallback_InitEGLCallback
///    
/// Specifiers: 
///    [jboolean] - 
///    
/// Parameters: 
///    [JNIEnv* LocalJNIEnv] - 
///    [jobject LocalThiz] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
jboolean NativeCallback_InitEGLCallback(JNIEnv* env, jobject thiz)
{
#if WITH_ES2_RHI
	appOutputDebugStringf("In initEGLCallback!");

    if (!RegisterJNIThreadForEGL(env, thiz))
    {
        appOutputDebugStringf("Error: init could not RegisterJNIThreadForEGL!");
        return JNI_FALSE;
    }

    appOutputDebugStringf("Querying EGLConfigParms class");
    jclass EGLConfigParms_class = env->FindClass("com/epicgames/EpicCitadel/UE3JavaApp$EGLConfigParms");

	appOutputDebugStringf("Querying EGLConfigParms fields");
    jfieldID redId    = env->GetFieldID(EGLConfigParms_class, "redSize", "I");
    jfieldID greenId    = env->GetFieldID(EGLConfigParms_class, "greenSize", "I");
    jfieldID blueId    = env->GetFieldID(EGLConfigParms_class, "blueSize", "I");
    jfieldID alphaId    = env->GetFieldID(EGLConfigParms_class, "alphaSize", "I");
    jfieldID stencilId    = env->GetFieldID(EGLConfigParms_class, "stencilSize", "I");
    jfieldID depthId    = env->GetFieldID(EGLConfigParms_class, "depthSize", "I");

    appOutputDebugStringf("Allocating EGLConfigParms");
    jobject parms = env->AllocObject(EGLConfigParms_class);
    env->SetIntField(parms, redId, GAndroidPF_Red);
    env->SetIntField(parms, greenId, GAndroidPF_Green);
    env->SetIntField(parms, blueId, GAndroidPF_Blue);
    env->SetIntField(parms, alphaId, GAndroidPF_Alpha);
    env->SetIntField(parms, stencilId, GAndroidPF_Stencil);
    env->SetIntField(parms, depthId, GAndroidPF_Depth);

	// storage root
	if (hasLocalAppValue("STORAGE_ROOT"))
	{
		appOutputDebugStringf("Calling getLocalAppValue");
		GAndroidRootPath = getLocalAppValue("STORAGE_ROOT");
		appOutputDebugStringf("Called getLocalAppValue %s", *GAndroidRootPath);
	}
	else
	{
		appOutputDebugStringf("STORAGE_ROOT not set");
	}

	// SDCARD Path
	//if (hasLocalAppValue("EXTERNAL_ROOT"))
	//{
	//	appOutputDebugStringf("Calling getLocalAppValu EXTERNAL_ROOTe");
	//	GAndroidExternalRootPath = getLocalAppValue("EXTERNAL_ROOT");
	//	appOutputDebugStringf("Called getLocalAppValue %s", *GAndroidExternalRootPath);
	//}
	//else
	//{
	//	appOutputDebugStringf("EXTERNAL_ROOT not set");
	//}

	//appOutputDebugStringf("appAndroidInit");
	//appOutputDebugStringf("appAndroidInit Done");	
    appOutputDebugStringf("initEGLCallback calling up to initEGL");

	UBOOL ResultValue = env->CallBooleanMethod(GJavaGlobalThiz, GMethod_InitEGL, parms) ? true : false;

	// clean up references
	env->DeleteLocalRef(parms);
	env->DeleteLocalRef(EGLConfigParms_class);

	return ResultValue;
#else
	return false;
#endif
}


////////////////////////////////////
/// Function: 
///    NativeCallback_Init
///    
/// Specifiers: 
///    [jboolean] - 
///    
/// Parameters: 
///    [JNIEnv* LocalJNIEnv] - 
///    [jobject LocalThiz] - 
///    [jint InScreenWidth] - 
///    [jint InScreenHeight] - 
///    [jfloat ScreenDiagonal] - 
///    [jboolean bFullMultiTouch] - 
///    [jobject AssetManager] - 
///    [jboolean bCrashDetected] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
jboolean NativeCallback_Initialize(JNIEnv* LocalJNIEnv, jobject LocalThiz, jint InScreenWidth, jint InScreenHeight, jfloat ScreenDiagonal, jboolean bFullMultiTouch, jobject AssetManager, jboolean bCrashDetected)
{
#if WITH_ES2_RHI	
	GScreenWidth = InScreenWidth;
	GScreenHeight = InScreenHeight;

	appOutputDebugStringf("Screen size: %d, %d", GScreenWidth, GScreenHeight);

	if (!RegisterJNIThreadForEGL(LocalJNIEnv, LocalThiz))
	{
		return JNI_FALSE;
	}
#endif
	
	extern void* UE3GameThread(void*);
	const int ThreadErrno = pthread_create(&GNativeMainThread, NULL, UE3GameThread, NULL);

	return JNI_TRUE;
}


////////////////////////////////////
/// Function: 
///    NativeCallback_Cleanup
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [JNIEnv* LocalJNIEnv] - 
///    [jobject LocalThiz] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void NativeCallback_Cleanup(JNIEnv* LocalJNIEnv, jobject LocalThiz)
{
	appOutputDebugStringf("NativeCallback_Cleanup!!!");

	RegisterJNIThreadForEGL(LocalJNIEnv, LocalThiz);

	GMainThreadExit = TRUE;
	pthread_join(GNativeMainThread, NULL);
	appOutputDebugStringf("Main loop exited");

	GGameThreadId = appGetCurrentThreadId();

	// Android TODO TBD - we should call EngineLoop->Exit here, but that causes
	// the thread to hang up.  Not sure why we're hanging.
	// Instead, with this WAR, we crash during shutdown
//	EngineLoop->Exit();
//	delete EngineLoop;
//	EngineLoop = NULL;
//	appOutputDebugStringf("EngineLoop deleted");

	DestroyingRegisteredObjectInstance();
}

////////////////////////////////////
/// Function: 
///    NativeCallback_InputEvent
///    
/// Specifiers: 
///    [jboolean] - 
///    
/// Parameters: 
///    [JNIEnv* LocalJNIEnv] - 
///    [jobject LocalThiz] - 
///    [jint InAction] - 
///    [jint InX] - 
///    [jint InY] - 
///    [jint PointerId] - 
///    [jlong TimeStamp] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
jboolean NativeCallback_InputEvent(JNIEnv* LocalJNIEnv, jobject LocalThiz, jint InAction, jint InX, jint InY, jint PointerId, jlong TimeStamp )
{
	static jclass KeyCode_class = LocalJNIEnv->FindClass("android/view/MotionEvent");
	static jfieldID ACTION_DOWN_id = LocalJNIEnv->GetStaticFieldID(KeyCode_class, "ACTION_DOWN", "I");
	static jfieldID ACTION_UP_id = LocalJNIEnv->GetStaticFieldID(KeyCode_class, "ACTION_UP", "I");
	static jfieldID ACTION_POINTER_DOWN_id = LocalJNIEnv->GetStaticFieldID(KeyCode_class, "ACTION_POINTER_DOWN", "I");
	static jfieldID ACTION_POINTER_UP_id = LocalJNIEnv->GetStaticFieldID(KeyCode_class, "ACTION_POINTER_UP", "I");

	static int ACTION_DOWN = LocalJNIEnv->GetStaticIntField(KeyCode_class, ACTION_DOWN_id);
	static int ACTION_UP = LocalJNIEnv->GetStaticIntField(KeyCode_class, ACTION_UP_id);
	static int ACTION_POINTER_DOWN = LocalJNIEnv->GetStaticIntField(KeyCode_class, ACTION_POINTER_DOWN_id);
	static int ACTION_POINTER_UP = LocalJNIEnv->GetStaticIntField(KeyCode_class, ACTION_POINTER_UP_id);

	//DEBUGCALLBACKS( "Action is %x\n", InAction );
	ETouchType EventType = Touch_Stationary;
	if( InAction == ACTION_DOWN ||
		InAction == ACTION_POINTER_DOWN )
	{
		EventType = Touch_Began;
	}
	else if( InAction == ACTION_UP ||
			 InAction == ACTION_POINTER_UP )
	{
		EventType = Touch_Ended;
	}
	else
	{
		EventType = Touch_Moved;
	}

	DOUBLE TimeStampSeconds = TimeStamp / 1000.0;
	GAndroidInputManager.AddTouchEvent(FAndroidTouchEvent(PointerId, FIntPoint(InX, InY), EventType, TimeStampSeconds));
	
	return JNI_TRUE;
}


////////////////////////////////////
/// Function: 
///    NativeCallback_KeyEvent
///    
/// Specifiers: 
///    [jboolean] - 
///    
/// Parameters: 
///    [JNIEnv* LocalJNIEnv] - 
///    [jobject LocalThiz] - 
///    [jint InAction] - 
///    [jint InKeycode] - 
///    [jint InUnichar] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
jboolean NativeCallback_KeyEvent(JNIEnv* LocalJNIEnv, jobject LocalThiz, jint InAction, jint InKeycode, jint InUnichar)
{
	static jclass KeyCode_class = LocalJNIEnv->FindClass("android/view/KeyEvent");
	static jfieldID ACTION_UP_id = LocalJNIEnv->GetStaticFieldID(KeyCode_class, "ACTION_UP", "I");
	static int ACTION_UP = LocalJNIEnv->GetStaticIntField(KeyCode_class, ACTION_UP_id);
  
	const FName* const Key = KeyMapVirtualToName.Find( InKeycode );
	if (Key)
	{
		debugf(TEXT("Got key event %s"), *Key->ToString());
		GAndroidInputManager.AddKeyEvent(FAndroidKeyEvent((InAction != ACTION_UP) ? TRUE : FALSE, *Key, InUnichar));
	}
	else 
	{
		debugf(TEXT("Got key event, but failed to find key"));
	}

	return JNI_TRUE;
}


////////////////////////////////////
/// Function: 
///    NativeCallback_PostInitUpdate
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [JNIEnv* LocalJNIEnv] - 
///    [jobject LocalThiz] - 
///    [jint DrawWidth] - 
///    [jint DrawHeight] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void NativeCallback_PostInitUpdate(JNIEnv* LocalJNIEnv, jobject LocalThiz, jint DrawWidth, jint DrawHeight)
{	
	GScreenWidth = DrawWidth;
	GScreenHeight = DrawHeight;
	
	appOutputDebugStringf("Screen size = %d, %d", GScreenWidth, GScreenHeight);	
}

////////////////////////////////////
/// Function: 
///    NativeCallback_KeyboardFinished
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [JNIEnv* LocalJNIEnv] - 
///    [jobject LocalThiz] - 
///    [jstring NewKeyText] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void NativeCallback_KeyboardFinished(JNIEnv* LocalJNIEnv, jobject LocalThiz, jstring NewKeyText )
{
	const ANSICHAR* FromJavaString = LocalJNIEnv->GetStringUTFChars(NewKeyText, 0);	
	FString KeyboardInput = ANSI_TO_TCHAR( FromJavaString );
	// @todo 
	LocalJNIEnv->ReleaseStringUTFChars(NewKeyText, FromJavaString);
	debugf(TEXT("KEYBOARD FINSISHED %s"), *KeyboardInput);
}


////////////////////////////////////
/// Function: 
///    NativeCallback_MovieFinished
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [JNIEnv* LocalJNIEnv] - 
///    [jobject LocalThiz] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void NativeCallback_MovieFinished(JNIEnv* LocalJNIEnv, jobject LocalThiz)
{
	check(GFullScreenMovie);
	((FAndroidFullScreenMovie*)GFullScreenMovie)->CALLBACK_MovieFinished();
}

INT GAndroidSystemMemory = 0;
UBOOL GAndroidUseMovies = TRUE;

////////////////////////////////////
/// Function: 
///    NativeCallback_SystemStats
///    
/// Specifiers: 
///    [jboolean] - 
///    
/// Parameters: 
///    [JNIEnv* LocalJNIEnv] - 
///    [jobject LocalThiz] - 
///    [jlong SystemMemory] - 
///    [jint ProcessorCount] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
jboolean NativeCallback_SystemStats(JNIEnv* LocalJNIEnv, jobject LocalThiz, jlong SystemMemory )
{
	GAndroidSystemMemory = (INT)((SystemMemory / 1024) / 1024);
	GNumHardwareThreads = android_getCpuCount();

	debugf(TEXT("*****************************************************"));
	debugf(TEXT("********** ANDROID SYSTEM MEMORY %d(MB) *************"), GAndroidSystemMemory);

	// if too little memory is available, disable movie playback
	if (GAndroidSystemMemory < MEMORY_MOVIE_CUTOFF)
	{
		GAndroidUseMovies = FALSE;
		debugf(TEXT("Bad Memory Conditions - no movies!!!"));
	}	

	debugf(TEXT("*****************************************************"));

	return JNI_TRUE;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         
}


////////////////////////////////////
/// Function: 
///    NativeCallback_InterruptionChanged
///    
/// Specifiers: 
///    [jboolean] - 
///    
/// Parameters: 
///    [JNIEnv* LocalJNIEnv] - 
///    [jobject LocalThiz] - 
///    [jboolean bIsInterruptionActive] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
jboolean NativeCallback_InterruptionChanged(JNIEnv* LocalJNIEnv, jobject LocalThiz, jboolean InterruptionActive)
{	
	appOutputDebugStringf("=====================================");
	appOutputDebugStringf("InteruptionChanged %s START", InterruptionActive ? "INACTIVE" : "ACTIVE" );
	appOutputDebugStringf("=====================================");

	if( InterruptionActive == GForceStopRendering )
	{
		appOutputDebugStringf("=====================================");
		appOutputDebugStringf("Currently in progress %d %d", InterruptionActive, GForceStopRendering );
		appOutputDebugStringf("=====================================");

		return JNI_TRUE;
	}
	
	if( InterruptionActive )
	{
		// returning false kills the game
		if( GPrimaryUE3StartupPhase ) return FALSE;

		// if Started up but app is still reloading from a suspend, wait for reload to finish
		while (!GMainThreadIsLoaded)
		{
			appSleep(0.1f);
		}

		// Set Main thread flag to false until app returns from suspend
		GMainThreadIsLoaded = FALSE;

		GForceStopRendering = TRUE;
	}
	else
	{
		GForceStopRendering = FALSE;
	}

	// if we haven't started up don't wait
	if( !GUE3HasStartedUp )
	{
		return JNI_TRUE;	
	}

	GHasInterruptionRequest = TRUE;

	//wait for it to be done
	if (GForceStopRendering)
	{
		while( GHasInterruptionRequest )
		{
			appSleep(0.1f);
		}
	}

	appOutputDebugStringf("=====================================");
	appOutputDebugStringf("InteruptionChanged %s DONE", InterruptionActive ? "INACTIVE" : "ACTIVE" );
	appOutputDebugStringf("=====================================");


	return JNI_TRUE;
}

////////////////////////////////////
/// Function: 
///    NativeCallback_NetworkUpdate
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [JNIEnv* LocalJNIEnv] - 
///    [jobject LocalThiz] - 
///    [jboolean bIsConnected] - 
///    [jboolean bIsWifiConnected] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void NativeCallback_NetworkUpdate(JNIEnv* LocalJNIEnv, jobject LocalThiz, jboolean bIsConnected, jboolean bIsWifiConnected )
{
	GHasConnection	= bIsConnected == JNI_TRUE ? TRUE : FALSE;
	GIsWifi			= GHasConnection && ( bIsWifiConnected == JNI_TRUE ? TRUE : FALSE );
}


////////////////////////////////////
/// Function: 
///    NativeCallback_LanguageSet
///    
/// Specifiers: 
///    [void] - 
///    
/// Parameters: 
///    [JNIEnv* LocalJNIEnv] - 
///    [jobject LocalThiz] - 
///    [jstring NewLanguageText] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
void NativeCallback_LanguageSet(JNIEnv* LocalJNIEnv, jobject LocalThiz, jstring NewLanguageText)
{	
	const ANSICHAR* FromJavaString = LocalJNIEnv->GetStringUTFChars(NewLanguageText, 0);
	// Remember the string
	extern FString GAndroidLocale;	
	GAndroidLocale = FromJavaString;
	LocalJNIEnv->ReleaseStringUTFChars(NewLanguageText, FromJavaString);
	// save the locale text
	debugf(TEXT("GOT LOCALE %s"), *GAndroidLocale);
}

bool NativeCallback_IsShippingBuild(JNIEnv* LocalJNIEnv, jobject LocalThiz)
{
#if FINAL_RELEASE
	return JNI_TRUE;
#else
	return JNI_FALSE;
#endif
}


JNIEXPORT void JNICALL NativeCallback_KeyPadChange( JNIEnv*  env, jobject thiz, jboolean IsVisible )
{
}

jstring NativeCallback_PhoneHomeGetURL(JNIEnv* LocalJNIEnv, jobject LocalThiz)
{
	return LocalJNIEnv->NewStringUTF( appGetAndroidPhoneHomeURL() );
}

jint NativeCallback_GetEngineVersion(JNIEnv* LocalJNIEnv, jobject LocalThiz)
{
	return GEngineVersion;
}

void NativeCallback_UpdatePerformanceSettings(JNIEnv* LocalJNIEnv, jobject LocalThiz, jint PerformanceLevel, jfloat ResolutionScale)
{
	// Handle runtime settings change
	appHandleFeatureLevelChange(PerformanceLevel, ResolutionScale);
}

// Called if OS has destroyed the eglSurface and the app has now recreated it
void NativeCallback_EGLSurfaceRecreated(JNIEnv* LocalJNIEnv, jobject LocalThiz)
{
	extern UBOOL GEGLSurfaceRecreated;
	GEGLSurfaceRecreated = TRUE;
}

jint NativeCallback_GetPerformanceLevel(JNIEnv* LocalJNIEnv, jobject LocalThiz)
{
	return GAndroidPerformanceLevel;
}

jfloat NativeCallback_GetResolutionScale(JNIEnv* LocalJNIEnv, jobject LocalThiz)
{
	return GAndroidResolutionScale;
}

jstring NativeCallback_GetGameName(JNIEnv* LocalJNIEnv, jobject LocalThiz)
{
	extern void appSetGameName();
	appSetGameName();
	
	return LocalJNIEnv->NewStringUTF( GGameName );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// JNI
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////
/// Function: 
///    JNI_OnLoad
///    
/// Specifiers: 
///    [jint] - 
///    
/// Parameters: 
///    [JavaVM* InJavaVM] - 
///    [void* InReserved] - 
///    
/// Description: 
///    
///    
////////////////////////////////////
jint JNI_OnLoad(JavaVM* InJavaVM, void* InReserved)
{
	JNIEnv *LocalJNIEnv;
	GJavaVM = InJavaVM;

	if (InJavaVM->GetEnv((void**) &LocalJNIEnv, JNI_VERSION_1_4) != JNI_OK)
	{		
		return -1;
	}		

	//public native void	NativeCallback_KeyPadChange( boolean IsVisible );
	//public native boolean	NativeCallback_InitEGLCallback();   
	//public native boolean	NativeCallback_Initialize( int drawWidth, int drawHeight, float ScreenDiagonal, boolean bFullMultiTouch );
	//public native void	NativeCallback_PostInitUpdate(int drawWidth, int drawHeight);	
	//public native void	NativeCallback_Cleanup();    
	//public native void	NativeCallback_KeyboardFinished(String TextOut);
	//public native void	NativeCallback_MovieFinished();
	//public native boolean	NativeCallback_SystemStats( long SystemMemory );
	//public native boolean	NativeCallback_InterruptionChanged( boolean InteruptionActive );
	//public native void	NativeCallback_NetworkUpdate( boolean IsConnected, boolean IsWifiConnected );
	//public native void	NativeCallback_LanguageSet(String LocaleSet);
	//public native boolean	NativeCallback_InputEvent(int action, int x, int y, int PointerIndex);
	//public native boolean NativeCallback_IsShippingBuild();
	//public native String	NativeCallback_PhoneHomeGetURL();
	//public native int		NativeCallback_GetEngineVersion();
	//public native int		NativeCallback_GetPerformanceLevel();
	//public native float	NativeCallback_GetReolutionScale();
	//public native void	NativeCallback_UpdatePerformanceSettings(int PerformanceLevel, float ResolutionScale);

	// hook up native functions to string names and params
	JNINativeMethod NativeCallbackMethods[] =
	{
		{ "NativeCallback_KeyPadChange",		"(Z)V",								(void*)NativeCallback_KeyPadChange },
		{ "NativeCallback_InitEGLCallback",		"()Z",								(void*)NativeCallback_InitEGLCallback },
		{ "NativeCallback_Initialize",			"(IIFZ)Z",							(void*)NativeCallback_Initialize },
		{ "NativeCallback_PostInitUpdate",		"(II)V",							(void*)NativeCallback_PostInitUpdate },		
		{ "NativeCallback_Cleanup",				"()V",								(void*)NativeCallback_Cleanup	},		
		{ "NativeCallback_KeyboardFinished",	"(Ljava/lang/String;)V",			(void*)NativeCallback_KeyboardFinished },
		{ "NativeCallback_MovieFinished",		"()V",								(void*)NativeCallback_MovieFinished	},
		{ "NativeCallback_SystemStats",			"(J)Z",								(void*)NativeCallback_SystemStats },
		{ "NativeCallback_InterruptionChanged",	"(Z)Z",								(void*)NativeCallback_InterruptionChanged },
		{ "NativeCallback_NetworkUpdate",		"(ZZ)V",							(void*)NativeCallback_NetworkUpdate },
		{ "NativeCallback_LanguageSet",			"(Ljava/lang/String;)V",			(void*)NativeCallback_LanguageSet },
		{ "NativeCallback_InputEvent",			"(IIIIJ)Z",							(void*)NativeCallback_InputEvent	},
		{ "NativeCallback_IsShippingBuild",		"()Z",								(void*)NativeCallback_IsShippingBuild },
		{ "NativeCallback_PhoneHomeGetURL",		"()Ljava/lang/String;",				(void*)NativeCallback_PhoneHomeGetURL	},
		{ "NativeCallback_GetEngineVersion",	"()I",								(void*)NativeCallback_GetEngineVersion },
		{ "NativeCallback_GetPerformanceLevel",	"()I",								(void*)NativeCallback_GetPerformanceLevel },
		{ "NativeCallback_GetResolutionScale",	"()F",								(void*)NativeCallback_GetResolutionScale },
		{ "NativeCallback_UpdatePerformanceSettings",	"(IF)V",					(void*)NativeCallback_UpdatePerformanceSettings },
		{ "NativeCallback_EGLSurfaceRecreated",	"()V",								(void*)NativeCallback_EGLSurfaceRecreated },
		{ "NativeCallback_GetGameName",			"()Ljava/lang/String;",				(void*)NativeCallback_GetGameName },
	};

	// get the java class from JNI
	jclass UE3AppClass;
	UE3AppClass = LocalJNIEnv->FindClass ("com/epicgames/EpicCitadel/UE3JavaApp");
	LocalJNIEnv->RegisterNatives(UE3AppClass, NativeCallbackMethods, ARRAY_COUNT(NativeCallbackMethods));
		
	// hook up java functions to string names and params
	JNIJavaMethods JavaCallBackMethods[] = 
	{
		//public boolean JavaCallback_swapBuffers()
		//public boolean JavaCallback_makeCurrent()
		//public boolean JavaCallback_unMakeCurrent()
		//public boolean JavaCallback_initEGL(EGLConfigParms parms)
		//public void JavaCallback_destroyEGLSurface() 
		//public void JavaCallback_SetFixedSizeScale( float InScale )
		{ &GMethod_Swap,				"JavaCallback_swapBuffers",			"()Z" },
		{ &GMethod_MakeCurrent,			"JavaCallback_makeCurrent",			"()Z" },
		{ &GMethod_UnmakeCurrent,		"JavaCallback_unMakeCurrent",		"()Z" },
		{ &GMethod_InitEGL,				"JavaCallback_initEGL",				"(Lcom/epicgames/EpicCitadel/UE3JavaApp$EGLConfigParms;)Z" },
		{ &GMethod_SetFixedSizeScale,	"JavaCallback_SetFixedSizeScale",	"(F)V" },

		//public void JavaCallback_PlaySong( final FileDescriptor SongFD, final long SongOffset, final long SongLength, String SongName )
		//public void JavaCallback_StopSong() 
		//public void JavaCallback_UpdateSongPlayer( float DeltaTime )
		{ &GMethod_AndroidPlaySong,			"JavaCallback_PlaySong",		"(Ljava/io/FileDescriptor;JJLjava/lang/String;)V" },
		{ &GMethod_AndroidStopSong,			"JavaCallback_StopSong",		"()V" },
		{ &GMethod_AndroidUpdateSongPlayer,	"JavaCallback_UpdateSongPlayer","(F)V" },

		//public void JavaCallback_StartVideo( FileDescriptor MovieFD, long MovieOffset, long MovieLength ) 
		//public void JavaCallback_StopVideo()
		//public boolean JavaCallback_IsVideoPlaying()
		//public void JavaCallback_VideoAddTextOverlay(String text)
		{ &GMethod_StartMovie,		"JavaCallback_StartVideo",			"(Ljava/io/FileDescriptor;JJ)V" },
		{ &GMethod_StopMovie,		"JavaCallback_StopVideo",			"()V" },
		{ &GMethod_IsMoviePlaying,	"JavaCallback_IsVideoPlaying",		"()Z" },
		{ &GMethod_VideoAddTextOverlay, "JavaCallback_VideoAddTextOverlay",	"(Ljava/lang/String;)V" },

		//public void JavaCallback_ShowKeyBoard( String InputText, float posX, float posY, float sizeX, float sizeY, boolean IsPassword )
		//public void JavaCallback_HideKeyBoard( boolean wasCancelled )
		//public void JavaCallback_ShowWebPage( String theURL )
		{ &GMethod_ShowKeyboard,	"JavaCallback_ShowKeyBoard",	"(Ljava/lang/String;FFFFZ)V" },
		{ &GMethod_HideKeyboard,	"JavaCallback_HideKeyBoard",	"(Z)V" },
		{ &GMethod_ShowWebPage,		"JavaCallback_ShowWebPage",		"(Ljava/lang/String;)V" },

		//public int JavaCallback_GetPerformanceLevel()
		//public float JavaCallback_GetResolutionScale()
		//public void JavaCallback_OpenSettingsMenu()
		//public int JavaCallback_GetDepthSize()
		//public boolean JavaCallback_IsExpansionInAPK()
		//public AssetManager JavaCallback_GetAssetManager()
		//public String JavaCallback_GetAppCommandLine()
		//public String JavaCallback_GetDeviceModel()
		{ &GMethod_GetPerformanceLevel, "JavaCallback_GetPerformanceLevel", "()I" },
		{ &GMethod_GetResolutionScale,	"JavaCallback_GetResolutionScale",	"()F" },
		{ &GMethod_OpenSettingsMenu,	"JavaCallback_OpenSettingsMenu",	"()V" },
		{ &GMethod_GetDepthSize,		"JavaCallback_GetDepthSize",		"()I" },
		{ &GMethod_IsExpansionInAPK,	"JavaCallback_IsExpansionInAPK",	"()Z" },
		{ &GMethod_GetAssetManager,		"JavaCallback_GetAssetManager",		"()Landroid/content/res/AssetManager;" },
		{ &GMethod_GetAppCommandLine,	"JavaCallback_GetAppCommandLine",	"()Ljava/lang/String;" },
		{ &GMethod_GetDeviceModel,		"JavaCallback_GetDeviceModel",		"()Ljava/lang/String;" },

		// public void JavaCallback_ApsalarInit() { Apsalar.Init(); }
		// public void JavaCallback_ApsalarStartSession(String ApiKey, String ApiSecret) { Apsalar.StartSession(ApiKey, ApiSecret); }
		// public void JavaCallback_ApsalarEndSession() { Apsalar.EndSession(); }
		// public void JavaCallback_ApsalarEvent(String EventName) { Apsalar.Event(EventName); }
		// public void JavaCallback_ApsalarEventParam(String EventName, String ParamName, String ParamValue) { Apsalar.EventParam(EventName, ParamName, ParamValue); }
		// public void JavaCallback_ApsalarEventParamArray(String EventName, String[] Params) { Apsalar.EventParamArray(EventName, Params); }
		// public void JavaCallback_ApsalarLogEngineData(String EventName, int EngineVersion) { Apsalar.LogEventEngineData(EventName, EngineVersion); }
		{ &GMethod_ApsalarInit,							"JavaCallback_ApsalarInit",							"()V" },
		{ &GMethod_ApsalarStartSession,					"JavaCallback_ApsalarStartSession",					"(Ljava/lang/String;Ljava/lang/String;)V" },
		{ &GMethod_ApsalarEndSession,					"JavaCallback_ApsalarEndSession",					"()V" },
		{ &GMethod_ApsalarLogStringEvent,				"JavaCallback_ApsalarEvent",						"(Ljava/lang/String;)V" },
		{ &GMethod_ApsalarLogStringEventParam,			"JavaCallback_ApsalarEventParam",					"(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V" },
		{ &GMethod_ApsalarLogStringEventParamArray,		"JavaCallback_ApsalarEventParamArray",				"(Ljava/lang/String;[Ljava/lang/String;)V" },
		{ &GMethod_ApsalarLogEngineData,				"JavaCallback_ApsalarLogEngineData",				"(Ljava/lang/String;I)V" },

		//public void JavaCallback_ShutDownApp() 
		//public boolean JavaCallback_hasAppLocalValue(String key) 
		//public String JavaCallback_getAppLocalValue(String key) 
		//public void JavaCallback_setAppLocalValue(String key, String value)  
		//public void JavaCallback_HideSplash()
		//public void JavaCallback_HideReloader()
		{ &GMethod_ShutdownApp,			"JavaCallback_ShutDownApp",			"()V" },
		{ &GMethod_HasAppLocalValue,	"JavaCallback_hasAppLocalValue",	"(Ljava/lang/String;)Z" },
		{ &GMethod_GetAppLocalValue,	"JavaCallback_getAppLocalValue",	"(Ljava/lang/String;)Ljava/lang/String;" },
		{ &GMethod_SetAppLocalValue,	"JavaCallback_setAppLocalValue",	"(Ljava/lang/String;Ljava/lang/String;)V" },
		{ &GMethod_HideSplash,			"JavaCallback_HideSplash",			"()V" },
		{ &GMethod_HideReloader,		"JavaCallback_HideReloader",		"()V" },

		// public String JavaCallback_GetMainAPKExpansionName()
		// public String JavaCallback_GetPatchAPKExpansionName()
		{ &GMethod_GetMainAPKExpansionName,		"JavaCallback_GetMainAPKExpansionName",		"()Ljava/lang/String;" },
		{ &GMethod_GetPatchAPKExpansionName,	"JavaCallback_GetPatchAPKExpansionName",	"()Ljava/lang/String;" },
	};

	UBOOL bAnyFailures = FALSE;
	for( INT MethodIter = 0; MethodIter < ARRAY_COUNT(JavaCallBackMethods); MethodIter++ )
	{
		*JavaCallBackMethods[MethodIter].JavaMethod = LocalJNIEnv->GetMethodID(UE3AppClass, JavaCallBackMethods[MethodIter].FunctionName, JavaCallBackMethods[MethodIter].FunctionParams);
		bAnyFailures |= (*JavaCallBackMethods[MethodIter].JavaMethod == NULL);
		if( *JavaCallBackMethods[MethodIter].JavaMethod == NULL )
		{
			appOutputDebugStringf("Method Failed to be found!! %s(%s)", JavaCallBackMethods[MethodIter].FunctionName, JavaCallBackMethods[MethodIter].FunctionParams); 
		}
	}

	check( bAnyFailures == FALSE );
	
	// setup audio functions
	void AudioDeviceJavaInit( JNIEnv *InJNIEnv, jclass &InAppClass );
	AudioDeviceJavaInit( LocalJNIEnv, UE3AppClass );

	// clean up references
	LocalJNIEnv->DeleteLocalRef(UE3AppClass);

	return JNI_VERSION_1_4;
}

void JNI_OnUnload(JavaVM* InJavaVM, void* InReserved)
{
	//ANDROID_LOG("JNI_OnUnload called");


	GJavaVM = NULL;
}
