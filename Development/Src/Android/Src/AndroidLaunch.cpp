/*=============================================================================
	AndroidLaunch.cpp: Android main platform glue definitions
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Android
#include <jni.h>
#include <pthread.h>
#include <semaphore.h>
#include <android/log.h>
#include <dlfcn.h>

// UE3
#include "EnginePrivate.h"
#include "Android.h"
#include "AndroidInput.h"
#include "LaunchPrivate.h"
#include "ES2RHIDebug.h"
#include "FFileManagerAndroid.h"
#include "AndroidJNI.h"
#include "EngineAudioDeviceClasses.h"

#if WITH_ES2_RHI
#include "ES2RHIPrivate.h"
#endif

#define MODULE "UE3"
#if NO_LOGGING
	#define ANDROID_LOG(a)
#else
	#define ANDROID_LOG(a) __android_log_print(ANDROID_LOG_DEBUG, MODULE, a)
#endif

// loaded libraries
void *GOPENSL_HANDLE = NULL;

extern UBOOL GMainThreadIsLoaded;

///////////////////////////////////////////////////////////////////////////////


void AddKeyMapping(JNIEnv* env, jobject thiz, const ANSICHAR* name, FName value)
{
	static jclass KeyCode_class = env->FindClass("android/view/KeyEvent");
	// Add a new mapping...
	jfieldID id = env->GetStaticFieldID(KeyCode_class, name, "I");
	int keyID = env->GetStaticIntField(KeyCode_class, id);
	KeyMapVirtualToName.Set(keyID, value);
}

#define AddKeymappingMacro(name, value) \
	AddKeyMapping(env, thiz, name, value)

void initMap(JNIEnv* env, jobject thiz)
{
	if ( KeyMapVirtualToName.Num() != 0 )
	{
		return;
	}

	AddKeymappingMacro("KEYCODE_TAB",KEY_Tab);
	AddKeymappingMacro("KEYCODE_ENTER",KEY_Enter);

	AddKeymappingMacro("KEYCODE_SPACE",KEY_SpaceBar);
	AddKeymappingMacro("KEYCODE_BACK",KEY_End);
	AddKeymappingMacro("KEYCODE_MENU",KEY_Insert);
	AddKeymappingMacro("KEYCODE_HOME",KEY_Home);

	AddKeymappingMacro("KEYCODE_DPAD_LEFT",KEY_Left);
	AddKeymappingMacro("KEYCODE_DPAD_UP",KEY_Up);
	AddKeymappingMacro("KEYCODE_DPAD_RIGHT",KEY_Right);
	AddKeymappingMacro("KEYCODE_DPAD_DOWN",KEY_Down);

	AddKeymappingMacro("KEYCODE_DEL",KEY_Delete);

	AddKeymappingMacro("KEYCODE_0",KEY_Zero);
	AddKeymappingMacro("KEYCODE_1",KEY_One);
	AddKeymappingMacro("KEYCODE_2",KEY_Two);
	AddKeymappingMacro("KEYCODE_3",KEY_Three);
	AddKeymappingMacro("KEYCODE_4",KEY_Four);
	AddKeymappingMacro("KEYCODE_5",KEY_Five);
	AddKeymappingMacro("KEYCODE_6",KEY_Six);
	AddKeymappingMacro("KEYCODE_7",KEY_Seven);
	AddKeymappingMacro("KEYCODE_8",KEY_Eight);
	AddKeymappingMacro("KEYCODE_9",KEY_Nine);

	AddKeymappingMacro("KEYCODE_A",KEY_A);
	AddKeymappingMacro("KEYCODE_B",KEY_B);
	AddKeymappingMacro("KEYCODE_C",KEY_C);
	AddKeymappingMacro("KEYCODE_D",KEY_D);
	AddKeymappingMacro("KEYCODE_E",KEY_E);
	AddKeymappingMacro("KEYCODE_F",KEY_F);
	AddKeymappingMacro("KEYCODE_G",KEY_G);
	AddKeymappingMacro("KEYCODE_H",KEY_H);
	AddKeymappingMacro("KEYCODE_I",KEY_I);
	AddKeymappingMacro("KEYCODE_J",KEY_J);
	AddKeymappingMacro("KEYCODE_K",KEY_K);
	AddKeymappingMacro("KEYCODE_L",KEY_L);
	AddKeymappingMacro("KEYCODE_M",KEY_M);
	AddKeymappingMacro("KEYCODE_N",KEY_N);
	AddKeymappingMacro("KEYCODE_O",KEY_O);
	AddKeymappingMacro("KEYCODE_P",KEY_P);
	AddKeymappingMacro("KEYCODE_Q",KEY_Q);
	AddKeymappingMacro("KEYCODE_R",KEY_R);
	AddKeymappingMacro("KEYCODE_S",KEY_S);
	AddKeymappingMacro("KEYCODE_T",KEY_T);
	AddKeymappingMacro("KEYCODE_U",KEY_U);
	AddKeymappingMacro("KEYCODE_V",KEY_V);
	AddKeymappingMacro("KEYCODE_W",KEY_W);
	AddKeymappingMacro("KEYCODE_X",KEY_X);
	AddKeymappingMacro("KEYCODE_Y",KEY_Y);
	AddKeymappingMacro("KEYCODE_Z",KEY_Z);

	AddKeymappingMacro("KEYCODE_STAR",KEY_Multiply);
	AddKeymappingMacro("KEYCODE_PLUS",KEY_Add);
	AddKeymappingMacro("KEYCODE_MINUS",KEY_Subtract);

	AddKeymappingMacro("KEYCODE_NUM",KEY_NumLock);

	AddKeymappingMacro("KEYCODE_ALT_LEFT",KEY_LeftAlt);
	AddKeymappingMacro("KEYCODE_ALT_RIGHT",KEY_RightAlt);

	AddKeymappingMacro("KEYCODE_SHIFT_LEFT",KEY_LeftShift);
	AddKeymappingMacro("KEYCODE_SHIFT_RIGHT",KEY_RightShift);

	AddKeymappingMacro("KEYCODE_APOSTROPHE",KEY_Quote);
	AddKeymappingMacro("KEYCODE_SEMICOLON",KEY_Semicolon);
	AddKeymappingMacro("KEYCODE_EQUALS",KEY_Equals);
	AddKeymappingMacro("KEYCODE_COMMA",KEY_Comma);
	AddKeymappingMacro("KEYCODE_PERIOD",KEY_Period);
	AddKeymappingMacro("KEYCODE_SLASH",KEY_Slash);
	AddKeymappingMacro("KEYCODE_GRAVE",KEY_Tilde);
	AddKeymappingMacro("KEYCODE_LEFT_BRACKET",KEY_LeftBracket);
	AddKeymappingMacro("KEYCODE_BACKSLASH",KEY_Backslash);
	AddKeymappingMacro("KEYCODE_RIGHT_BRACKET",KEY_RightBracket);

}


//-----------------------------------------------------------------------------
// SHA-1 functions
//-----------------------------------------------------------------------------

/**
 * Get the hash values out of the executable hash section made with ppu-lv2-objcopy (see makefile)
 */
void InitSHAHashes()
{
	// Include the hash filenames and values directly - note that the
	// cooker has conditioned the file so that it can be included
	// this way
	const BYTE HashesSHAData[] = {
		#include "hashes.sha"
	};

	const INT HashesSHASize = ARRAY_COUNT(HashesSHAData);
	// Note: there may be a dummy byte so the file is not empty
	if( HashesSHASize > 1 )
	{
		debugf(TEXT("Hashes are active, initializing the SHA Verification system"));
		FSHA1::InitializeFileHashesFromBuffer((BYTE*)HashesSHAData, HashesSHASize);
	}
	else
	{
		debugf(TEXT("No hashes are active, not initializing the SHA Verification system"));
	}
}

// *** NEVER CHECK IN THE BELOW SET TO 1!!! ***
#define DISABLE_AUTHENTICATION_FOR_DEV 0
// *** NEVER CHECK IN THE ABOVE SET TO 1!!! ***

/**
 * Callback that is called if the asynchronous SHA verification fails
 * This will be called from a pooled thread.
 *
 * @param FailedPathname Pathname of file that failed to verify
 * @param bFailedDueToMissingHash TRUE if the reason for the failure was that the hash was missing, and that was set as being an error condition
 */
void appOnFailSHAVerification(const TCHAR* FailedPathname, UBOOL bFailedDueToMissingHash)
{
	FString FailureMessage =
		FString::Printf(TEXT("SHA Verification failed for '%s'. Reason: %s"), 
			FailedPathname ? FailedPathname : TEXT("Unknown file"),
			bFailedDueToMissingHash ? TEXT("Missing hash") : TEXT("Bad hash"));

#if !DISABLE_AUTHENTICATION_FOR_DEV
	appErrorf(*FailureMessage);
#else
	debugfSuppressed(NAME_DevSHA, *FailureMessage);
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/**
 * Called when we are resuming, or pausing the android game
 *
 * @param bIsCleaningUp we cleaning up resources?
 */
void GPUStateChanged(UBOOL bIsCleaningUp)
{
	debugf(TEXT("GPUStateChanged %d"), bIsCleaningUp);

	// if no resets don't cleanup anything
	if( !GAllowFullRHIReset )
	{
		return;		
	}

	// list of resources to restore
	static TArray<FRenderResource*> StoredResourceList;


	// we cleaning, or initializing?
	if( bIsCleaningUp )
	{
		// clean up the ES2 Resources held onto outside of resourcelist
#if WITH_ES2_RHI
		ClearES2PendingResources();
#endif

		// push global first
		for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			if( ResourceIt->IsGlobal() == TRUE )
			{
				StoredResourceList.AddItem( *ResourceIt );
			}
		}
		for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			if( ResourceIt->IsGlobal() == FALSE )
			{
				StoredResourceList.AddItem( *ResourceIt );
			}
		}
		for( INT ResourceIter = 0; ResourceIter < StoredResourceList.Num(); ResourceIter++ )
		{
			StoredResourceList( ResourceIter )->ReleaseResource();
		}
		debugf(TEXT("Freed %d GPU Resources"), StoredResourceList.Num());
	}
	else
	{
		for( INT ResourceIter = 0; ResourceIter < StoredResourceList.Num(); ResourceIter++ )
		{
			StoredResourceList( ResourceIter )->InitResource();
		}
		debugf(TEXT("Restored %d GPU Resources"), StoredResourceList.Num());
		StoredResourceList.Empty();
	}	
}

/**
 * Called each tick to syncronize game interruptions
 */
void UpdateGameInterruptions()
{
	static FLOAT GLastWindowScaleFactor = 1.0f;

	if( GLastWindowScaleFactor != GWindowScaleFactor )
	{
		GHasInterruptionRequest = TRUE;
		GForceStopRendering		= TRUE;
	}

	if( GHasInterruptionRequest )
	{
		// check for the going-to-background flag having been set
		if( GForceStopRendering )
		{
			// Force audio device to pause as well when the app goes to the background
			if( GEngine->Client && GEngine->Client->GetAudioDevice() )
			{
				GEngine->Client->GetAudioDevice()->Update( FALSE );
			}

			// stop the rendering thread 
			if( GUseThreadedRendering )
			{
				// in which case we must make sure rendering has completed
				FlushRenderingCommands();
				StopRenderingThread();
			}
			else
			{
				RHIReleaseThreadOwnership();
			}

			glFinish();

			////////////////////////////////////////
			//RELEASE ALL GPU RESOURCSE
			////////////////////////////////////////
			RHIAcquireThreadOwnership();
			GPUStateChanged( TRUE );	
			RHIReleaseThreadOwnership();

			// alert the other thread that it is now safe to return from the going-to-background function			
			GHasInterruptionRequest = FALSE;			

			// update the window scale
			if( GLastWindowScaleFactor != GWindowScaleFactor )
			{
				CallJava_UpdateFixedSizeScale( GWindowScaleFactor );
				GLastWindowScaleFactor = GWindowScaleFactor;
			}

			// now, do nothing until the app has finished coming back to the foreground
			while (GForceStopRendering)
			{
				appSleep(0.1f);
			}	
		}

		if( !GForceStopRendering )
		{
			////////////////////////////////////////
			//RELOAD ALL GPU RESOURCSE
			////////////////////////////////////////
			RHIAcquireThreadOwnership();
			GPUStateChanged( FALSE );
			RHIReleaseThreadOwnership();

			// restart the rendering thread 
			if( GUseThreadedRendering )
			{
				StartRenderingThread();
			}
			else
			{
				RHIAcquireThreadOwnership();
			}

			// Recompile shaders if eglSurface was lost while app was suspended
			// but skip if they'll be recompiled anyways in a Feature Level update
			extern UBOOL GEGLSurfaceRecreated;
			extern UBOOL GFeatureLevelChangeNeeded;
			if (GEGLSurfaceRecreated && !GFeatureLevelChangeNeeded)
			{
				appRecompilePreprocessedShaders();

				// Hide reloader if its visible
				CallJava_HideReloader();
			}
			GEGLSurfaceRecreated = FALSE;

			// Main thread has loaded, set flag so UI thread knows it can suspend if needed
			GMainThreadIsLoaded = TRUE;

			GHasInterruptionRequest = FALSE;
		}
	}
}

/**
 * Load libraries we need that we may not want to directly reference
 */
void CheckAvaliableAndroidLibraries()
{
	GOPENSL_HANDLE = dlopen("/system/lib/libOpenSLES.so", RTLD_NOW);
}

/**
 * Main Thread Entry for UE3
 */
void* UE3GameThread(void*)
{
	GPrimaryUE3StartupPhase = TRUE;

	// all android needs this	
	GAllowFullRHIReset = TRUE;

	CheckAvaliableAndroidLibraries();

	extern bool	RegisterSecondaryThreadForEGL();
	RegisterSecondaryThreadForEGL();
	JNIEnv* LocalJNIEnv = (JNIEnv*)pthread_getspecific(GJavaJNIEnvKey);

#if WITH_ES2_RHI
	// clear screen to a solid color
	debugf(TEXT("Making current UE3GameThread"));
	extern void PlatformMakeCurrent(FES2Viewport* Viewport);
	PlatformMakeCurrent(NULL);
	// call this early for multiple cooked directories
	CheckOpenGLExtensions();
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	extern void PlatformSwapBuffers(FES2Viewport* Viewport);
	PlatformSwapBuffers(NULL);
	glClear(GL_COLOR_BUFFER_BIT);
	PlatformSwapBuffers(NULL);
#endif

	// Must be called _after_ the setup code for the storage root...
	appAndroidInit(0, NULL);

	// Initialize hashes
	InitSHAHashes();
	appInitTiming();

	GIsStarted = TRUE;
	GIsGuarded = FALSE;

	GStartTime = appSeconds();

	extern TCHAR GCmdLine[16384];
	DOUBLE PrintCounter;
	INT FrameCount = 0;
	DOUBLE LastTime;
	FEngineLoop* EngineLoop = new FEngineLoop;

	EngineLoop->PreInit(GCmdLine);
	GLog->SetCurrentThreadAsMasterThread();

	EngineLoop->Init();

	initMap(LocalJNIEnv, GJavaGlobalThiz);

	// Track average frame time
	PrintCounter = appSeconds();
	FrameCount = 0;
	LastTime = appSeconds();

	GPrimaryUE3StartupPhase = FALSE;
	GUE3HasStartedUp = TRUE;

	// run those movies
	if( GFullScreenMovie != NULL )
	{
		GFullScreenMovie->GameThreadInitiateStartupSequence();

		// Have Citadel popup start prompt
		if (GWorld->GetOutermost()->GetName() == TEXT("EpicCitadel"))
		{
			CallJava_AddMovieTextOverlay(TEXT("TAP TO START"));
		}

		GFullScreenMovie->GameThreadWaitForMovie();
	}

	// Ensure splash screen is hidden
	CallJava_HideSplash();

	while (!GIsRequestingExit)
	{
		extern UBOOL GMainThreadExit;
		// look for java shutting down
		if (GMainThreadExit)
		{
			break;
		}
	
		// check for the going-to-background flag having been set
		UpdateGameInterruptions();

		// ready for engine tick
		EngineLoop->Tick();

		// manage android song player
		AndroidUpdateSongPlayer();
	}

	GIsStarted = FALSE;
	__android_log_print(ANDROID_LOG_DEBUG, MODULE, "Dropped out of main loop!!!");

	CallJava_ShutdownApp();

	return NULL;
}





