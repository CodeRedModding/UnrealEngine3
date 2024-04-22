/*=============================================================================
	IPhoneAppDelegate.mm: IPhone application class / main loop
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#import <pthread.h>
#import <CoreFoundation/CFRunLoop.h>
#include <AudioToolbox/AudioToolbox.h>
#include <execinfo.h>
#include <exception>

#include "Engine.h"

#include "IPhoneObjCWrapper.h"
#include "LaunchPrivate.h"
#include "IPhoneInput.h"
#include "HTTPDownload.h"
#include "AudioDeviceIPhone.h"

#import "IPhoneAppDelegate.h"
#import "EAGLView.h"
#import "IPhoneHome.h"
#import "IPhoneAsyncTask.h"

#include "EnginePlatformInterfaceClasses.h"
#include "AppNotificationsIPhone.h"

#if APPLE_BATTERY_TEST_BUILD
	#import <CoreFoundation/CoreFoundation.h>
	#import <stdio.h>
	#import <mach/port.h>
	#import <notify.h>
#endif

#if WITH_GFx
#include "GFx/AMP/Amp_Server.h"
#endif

@implementation IPhoneAppDelegate

@synthesize GlobalViewScale;
@synthesize VolumeMultiplier;
@synthesize bCanAutoRotate; 
@synthesize OSVersion;
@synthesize PreviousSongURL;
@synthesize PreviousSongPauseTime;
@synthesize NextSongToPlay;
@synthesize Controller;
@synthesize GLView;
@synthesize bMainGLViewReadyToInitialize;
@synthesize bMainGLViewInitialized;
@synthesize SecondaryGLView;
@synthesize Window;
@synthesize SecondaryWindow;
@synthesize MusicPlayer;
@synthesize FadeTimer;
@synthesize UE3InitCheckTimer;
@synthesize AlertResponse;
@synthesize RootView;
@synthesize BannerView;
@synthesize bUsingBackgroundMusic;
@synthesize iTunesURL;

@synthesize UserInputAlert;
@synthesize UserInputField;
@synthesize UserInputExec;
@synthesize UserInputCancel;
@synthesize UserInputCharacterLimit;
@synthesize UserInputView;
@synthesize UserView;

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	@synthesize ConsoleAlert;
	@synthesize ConsoleTextField;
	@synthesize ConsoleHistoryValues;
	@synthesize ConsoleHistoryValuesIndex;
#endif

#define SHOW_FPS (!FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE)

/** This flag is used to communicate from iPhone thread to the game thread to stop rendering so it can go to the background */
UBOOL GForceStopRendering = FALSE;

/** This flag tells the iPhone thread that there is no rendering happening (starts as TRUE because the startup sequence doesn't do any rendering to wait to finish */
UBOOL GHasStoppedRendering = TRUE;

UBOOL bIPhonePortraitMode = FALSE;

/** Global options app was launched with */
NSDictionary* GLaunchOptionsDict = nil;

/** If set to something other than IOS_Unknown, allows overriding the device type for testing purposes. */
EIOSDevice GOverrideIOSDevice = IOS_Unknown;

/** If set to something other than 0.0f, allows overriding the device iOS version for testing purposes. */
float GOverrideIOSVersion = 0.0f;


/**
 * Encapsulates stopping and starting the game and render threads so that the main iOS thread can work freely
 */
class FScopedSuspendGameAndRenderThreads
{
public:
	/**
	 *	Constructor that flushes and suspends the renderthread
	 *	@param bRecreateThread	- Whether the rendering thread should be completely destroyed and recreated, or just suspended.
	 */
	FScopedSuspendGameAndRenderThreads()
	{
		// The game thread will control the render thread with this global
		// and will idle itself as long as it's set
		GForceStopRendering = TRUE;

		// Wait until rendering has stopped and the game thread is idle
		while( GHasStoppedRendering == FALSE )
		{
			appSleep(0.01f);
		}
	}

	/** Destructor that starts the renderthread again */
	~FScopedSuspendGameAndRenderThreads()
	{
		// Restore the game and rendering threads
		GForceStopRendering = FALSE;
	}
};

/**
 * @return the single app delegate object
 */
+ (IPhoneAppDelegate*)GetDelegate
{
	return (IPhoneAppDelegate*)[UIApplication sharedApplication].delegate;
}



void appSigHandler(int sig, siginfo_t *info, void *context)
{
	// report the error
	NSLog(@"SigHandler fired: %d", sig);
	appCaptureCrashCallStack(TEXT("appSigHandler"), 1.5f, -2);
}

void appSigHandlerSimple(int sig)
{
	// report the error
	NSLog(@"Simple SigHandler fired: %d", sig);
	appCaptureCrashCallStack(TEXT("appSigHandlerSimple"), 1.5f, -2);
}

void SetupSigaction(int inSig)
{
	// This is the more advanced handler setup...
//  	struct sigaction sigActionData;
//  	sigActionData.sa_sigaction = appSigHandler;
//  	sigActionData.sa_flags = SA_SIGINFO;
//  	sigemptyset(&sigActionData.sa_mask);
//	if (sigaction(inSig,	&sigActionData, NULL) != 0)
//	{
//		NSLog(@"Failed    sigaction for %d!", inSig);
//	}

	// This is the simple handler setup...
	signal(inSig, appSigHandlerSimple);
}

void SetupSignalHandling()
{
	SetupSigaction(SIGQUIT);		//create core image       quit program
	SetupSigaction(SIGILL);			//create core image       illegal instruction
	SetupSigaction(SIGTRAP);		//create core image       trace trap
	SetupSigaction(SIGABRT);		//create core image       abort(3) call (formerly SIGIOT)
	SetupSigaction(SIGEMT);			//create core image       emulate instruction executed
	SetupSigaction(SIGFPE);			//create core image       floating-point exception
	SetupSigaction(SIGBUS);			//create core image       bus error
	SetupSigaction(SIGSEGV);		//create core image       segmentation violation
	SetupSigaction(SIGSYS);			//create core image       non-existent system call invoked
	SetupSigaction(SIGPIPE);		//terminate process       write on a pipe with no reader
	SetupSigaction(SIGALRM);		//terminate process       real-time timer expired
	SetupSigaction(SIGXCPU);		//terminate process       cpu time limit exceeded (see setrlimit(2))
	SetupSigaction(SIGXFSZ);		//terminate process       file size limit exceeded (see setrlimit(2))

	// Signals we are NOT handling (listed for reference)
// 	SIGHUP		//terminate process       terminal line hangup
// 	SIGINT		//terminate process       interrupt program
//	SIGKILL		//terminate process       kill program
// 	SIGTERM		//terminate process       software termination signal
// 	SIGURG		//discard signal          urgent condition present on socket
//	SIGSTOP		//stop process            stop (cannot be caught or ignored)
// 	SIGTSTP		//stop process            stop signal generated from keyboard
// 	SIGCONT		//discard signal          continue after stop
// 	SIGCHLD		//discard signal          child status has changed
// 	SIGTTIN		//stop process            background read attempted from control terminal
// 	SIGTTOU		//stop process            background write attempted to control terminal
//	SIGIO		//discard signal          I/O is possible on a descriptor (see fcntl(2))
// 	SIGVTALRM	//terminate process       virtual time alarm (see setitimer(2))
// 	SIGPROF		//terminate process       profiling timer alarm (see setitimer(2))
// 	SIGWINCH	//discard signal          Window size change
// 	SIGINFO		//discard signal          status request from keyboard
// 	SIGUSR1		//terminate process       User defined signal 1
// 	SIGUSR2		//terminate process       User defined signal 2
}

void ClearSignalHandling()
{
	signal(SIGQUIT, NULL);		//create core image       quit program
	signal(SIGILL,  NULL);		//create core image       illegal instruction
	signal(SIGTRAP, NULL);		//create core image       trace trap
	signal(SIGABRT, NULL);		//create core image       abort(3) call (formerly SIGIOT)
	signal(SIGEMT,  NULL);		//create core image       emulate instruction executed
	signal(SIGFPE,  NULL);		//create core image       floating-point exception
	signal(SIGBUS,  NULL);		//create core image       bus error
	signal(SIGSEGV, NULL);		//create core image       segmentation violation
	signal(SIGSYS,  NULL);		//create core image       non-existent system call invoked
	signal(SIGPIPE, NULL);		//terminate process       write on a pipe with no reader
	signal(SIGALRM, NULL);		//terminate process       real-time timer expired
	signal(SIGXCPU, NULL);		//terminate process       cpu time limit exceeded (see setrlimit(2))
	signal(SIGXFSZ, NULL);		//terminate process       file size limit exceeded (see setrlimit(2))
}

#if WITH_ES2_RHI

/**
 * Fill the ES2 viewport with information about the EAGL View it represents
 */
void PlatformInitializeViewport(FES2Viewport* Viewport, void* WindowHandle)
{
	// the WindowHandle is the EAGLView pointer
	Viewport->PlatformData = WindowHandle;

	// set the pre-existing render buffer names
	EAGLView* GLView = (EAGLView*)Viewport->PlatformData;
NSLog(@"GLView: %x", Viewport->PlatformData);
NSLog(@"GLView: %@", GLView);
	Viewport->BackBufferName = GLView.OnScreenColorRenderBuffer;
	Viewport->MSAABackBufferName = GLView.OnScreenColorRenderBufferMSAA;
}

/**
 * Tear down the viewport
 */
void PlatformDestroyViewport(FES2Viewport* Viewport)
{
	// @todo iphone: Remove the EAGLview from the screen?
}

/**
 * ES2-called function to swap the buffers
 */
void PlatformSwapBuffers(FES2Viewport* Viewport) 
{
	// track time blocked on GPU
	DWORD BeforeSwap = appCycles();
	
	EAGLView* GLView = (EAGLView*)Viewport->PlatformData;
	[GLView SwapBuffers];
	
	GRenderThreadIdle += appCycles() - BeforeSwap;
}

/**
 * ES2-called function to make the GL context be the current context
 */
void PlatformMakeCurrent(FES2Viewport* Viewport)
{
	EAGLView* GLView = (EAGLView*)Viewport->PlatformData;
	[GLView MakeCurrent];
}

/**
 * ES2-called function to unbind the GL context
 */
void PlatformUnmakeCurrent(FES2Viewport* Viewport)
{
	EAGLView* GLView = (EAGLView*)Viewport->PlatformData;
	[GLView UnmakeCurrent];
}

#endif

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

/**
 *
 */
-(void) ParseCommandLineOverrides
{
	// Check for device type override
	FString IOSDeviceName;
	if ( Parse( appCmdLine(), TEXT("-IOSDevice="), IOSDeviceName) )
	{
		for ( INT DeviceTypeIndex = 0; DeviceTypeIndex < IOS_Unknown; ++DeviceTypeIndex )
		{
			if ( IOSDeviceName == IPhoneGetDeviceTypeString( (EIOSDevice) DeviceTypeIndex) )
			{
				GOverrideIOSDevice = (EIOSDevice) DeviceTypeIndex;
			}
		}
	}

	// Check for iOS version override
	FLOAT IOSVersion = 0.0f;
	if ( Parse( appCmdLine(), TEXT("-IOSVersion="), IOSVersion) )
	{
		GOverrideIOSVersion = IOSVersion;
	}
}

#if APPLE_BATTERY_TEST_BUILD
#define kAppleAutomationNotificationKey  "GameAutomationNotificationKey"
static void ChangeGameAutomationState(int token, const char *key, int value)
{
	if (token != -1)
	{
		notify_set_state(token, value);
		notify_post(key);
	}
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/**
 * This is the main game thread that has been created by the iPhone app. This allows the game
 * to be as slow as it wants without interfering with iPhone application behavior. Input
 * from the device is pushed to this thread from the iPhone thread, this does not poll for input
 *
 * @param launchOptions non-nil if the app was launched via accepting a push notification
 */
-(void) MainUE3GameThread:(NSDictionary*)launchOptions 
{
	// game thread has begun!
	GIsStarted = TRUE;
	GIsGuarded = FALSE;
	GStartTime = appSeconds();
	VolumeMultiplier = 1.0f;
	
	// make sure this thread has an auto release pool setup 
	NSAutoreleasePool* AutoreleasePool = [[NSAutoreleasePool alloc] init];

	// Initialize hashes
	InitSHAHashes();

	// create the engine loop object
	FEngineLoop EngineLoop;

	// Look for overrides specified on the command-line
	[self ParseCommandLineOverrides];
	
	// do the shared early engine startup stuff
	extern TCHAR GCmdLine[16384];
	EngineLoop.PreInit(GCmdLine);

	// Initialize the GLView object and all associated EGL/OpenGL systems
	{
		// Create the framebuffer and renderbuffers we'll use for rendering
		// HACK: We need to do this on the main thread, so use a couple BOOLs to synchronize the easy way
		bMainGLViewInitialized = NO;
		bMainGLViewReadyToInitialize = YES;
		while( bMainGLViewInitialized == NO )
		{
			appSleep(0.0001f);
		}

#if WITH_ES2_RHI
		// Get the default, device-specific value from the ini and
		// store the view scale so we can apply this to UI as well
		self.GlobalViewScale = GSystemSettings.MobileContentScaleFactor;
#endif
	}

	// Take control of the context for this thread
	[GLView MakeCurrent];

#if EXPERIMENTAL_FAST_BOOT_IPHONE
	// by making the viewport now ahead of time, we can start compiling the ES2 shaders sooner
	// which will make for faster startup times
	extern void MakeExperimentalIPhoneViewport(EAGLView* View, FLOAT ScaleFactor);
	MakeExperimentalIPhoneViewport(GLView, self.GlobalViewScale);
#endif

	// this thread owns debugf	
	GLog->SetCurrentThreadAsMasterThread();
	ProfNodeSetCurrentThreadAsMasterThread();

	// Keep a global reference to the launch options
	GLaunchOptionsDict = launchOptions;
	
	// start up the engine
	EngineLoop.Init();

	// Enable occlusion query support if it's allowed in the system settings
	// and the OS and HW combination support it
	EIOSDevice DeviceType = IPhoneGetDeviceType();
#if WITH_ES2_RHI
	extern UBOOL GIgnoreAllOcclusionQueries;
	if( GSystemSettings.bAllowMobileOcclusionQueries &&
		( OSVersion >= 5.0 &&
			( DeviceType == IOS_IPad2 ) ) )
	{
		GIgnoreAllOcclusionQueries = FALSE;
	}
#endif


	// track average frame time
	DOUBLE PrintCounter = appSeconds();
	INT FrameCount = 0;

	// vsync is required on iPhone	
	GSystemSettings.bUseVSync = TRUE;
	
#if WITH_UE3_NETWORKING
	// @todo hack: Remove when network file loading is optimized as possible
extern DOUBLE DEBUG_NetworkFileTimeCopyOverhead;
extern DOUBLE DEBUG_NetworkFileTimeFindOverhead;
extern DOUBLE DEBUG_NetworkFileTimeSizeOverhead;
	debugf(TEXT("-----------------"));
	debugf(TEXT("Total network file manager time: Copy: %.2fs, FileTime: %.2fs, FindFiles: %.2fs"), DEBUG_NetworkFileTimeCopyOverhead, DEBUG_NetworkFileTimeSizeOverhead, DEBUG_NetworkFileTimeFindOverhead);
	debugf(TEXT("-----------------"));
#endif
	
	// loop until we're done!
	DOUBLE LastTime = appSeconds();

	appOutputDebugStringf(TEXT("IPhone thread boot time = %.2f") LINE_TERMINATOR, LastTime - GStartTime);

	// Put the game thread into real-time mode so that thread scheduling becomes much more precise.
	IPhoneThreadSetRealTimeMode( pthread_self(), 20, 60);

	// we have now started rendering, so reset this to FALSE
	GHasStoppedRendering = FALSE;

#if APPLE_BATTERY_TEST_BUILD
	// Setup a notify register to send messages to an external Apple app
	static int AppleNotifyToken = -1;
	if (AppleNotifyToken == -1)
	{
		notify_register_check(kAppleAutomationNotificationKey, &AppleNotifyToken);
	}
#endif

	while( !GIsRequestingExit ) 
	{
		// run the run loop in default mode, zero time slice, as we don't want
		// a preponderance of input sources to hitch any given frame.
		// Right now the engine doesn't really use run loops on the game thread
		// but this is done to help 3rd party libs who expect a run loop to be present.
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);

#if APPLE_BATTERY_TEST_BUILD
		// Begin Apple automation check
		ChangeGameAutomationState(AppleNotifyToken, kAppleAutomationNotificationKey, 1);
#endif

		// check for the going-to-background flag having been set
		if (GForceStopRendering)
		{
			// in which case we must make sure rendering has completed, and temporarily release the GL context
			SCOPED_SUSPEND_RENDERING_THREAD(FSuspendRenderingThread::ST_ReleaseThreadOwnership);

			// alert the other thread that it is now safe to return from the going-to-background function
			GHasStoppedRendering = TRUE;

			// now, do nothing until the app has finished coming back to the foreground
			while (GForceStopRendering)
			{
				appSleep(0.1f);
			}
		}
		else
		{
			if (GHasStoppedRendering == TRUE)
			{
				// We have transitioned out of the background?
				GHasStoppedRendering = FALSE;
			}
		}

		// Tick any async tasks that may need to notify their delegates
		[IPhoneAsyncTask TickAsyncTasks];
	
		// tick the engine
		EngineLoop.Tick();

		// free any autoreleased objects every once in awhile to keep memory use down (strings, splash screens, etc)
		if (((FrameCount++) & 31) == 0)
		{
			[AutoreleasePool release];
			AutoreleasePool = [[NSAutoreleasePool alloc] init];
		}

#if APPLE_BATTERY_TEST_BUILD
		// End Apple automation check
		ChangeGameAutomationState(AppleNotifyToken, kAppleAutomationNotificationKey, 0);
#endif
	}
	
	// and... we are done
	GIsStarted = FALSE;
}
	
/**
 * Sets whether or not the view can autorotate
 */
- (void)SetRotationEnabled:(BOOL)bEnabled
{
	bCanAutoRotate = bEnabled;
}

/**
 * Sets the VolumeMultiplier and adjusts the MusicPlayer's current volume
 */
- (void)ScaleMusicVolume:(NSString*)VolumeScale
{
	VolumeMultiplier = [VolumeScale floatValue];

	if (FadeAlpha < 0.0f)
	{
		self.MusicPlayer.volume = -FadeAlpha * VolumeMultiplier;
	}
	else
	{
		self.MusicPlayer.volume = (1.0f - FadeAlpha) * VolumeMultiplier;
	}
}

- (void)UpdatePlayerVolume
{
	// calculate DeltaTime
	double Now = appSeconds();
	float DeltaTime = Now - LastFadeTime;
	LastFadeTime = Now;

	if(self.bUsingBackgroundMusic)
		return;

	if (FadeAlpha == 0.0f)
	{
		return;
	}

	// fade out if negative
	if (FadeAlpha < 0.0f)
	{
		// update fade amount
		FadeAlpha += DeltaTime;

		if (FadeAlpha < 0.0f)
		{
			self.MusicPlayer.volume = -FadeAlpha * VolumeMultiplier;
		}
		else
		{
			self.MusicPlayer.volume = 0.0f;
			
			// kill the previous one
			if (self.MusicPlayer != nil)
			{
				[self.MusicPlayer stop];
				self.MusicPlayer = nil;
			}

			// Stop the timer
			FadeAlpha = 0.0f;
			
			// start the next one
			[self PlaySong:self.NextSongToPlay];
			self.NextSongToPlay = nil;
		}
	}
	// else fade in
	else
	{
		// update fade amount
		FadeAlpha -= DeltaTime;

		if (FadeAlpha > 0.0f)
		{
			self.MusicPlayer.volume = (1.0f - FadeAlpha) * VolumeMultiplier;
		}
		else
		{
			// if done fading, set volume
			self.MusicPlayer.volume = VolumeMultiplier;

			// stop the timer
			FadeAlpha = 0.0f;
		}
	}
}

-(void) audioPlayerBeginInterruption: (AVAudioPlayer*)player
{
}

-(void) audioPlayerEndInterruption: (AVAudioPlayer*)player
{
}

/**
 * Plays a hardware mp3 stream
 */
- (void)PlaySong:(NSString*)SongName
{
	if(self.bUsingBackgroundMusic)
	{
		//save this so we can start if we want game music again
		self.NextSongToPlay = SongName;
		return;
	}

	if (SongName == nil || SongName.length == 0)
	{
		return;
	}

	// get the path to the sound file
	NSString* SongPath = [[NSBundle mainBundle] pathForResource:SongName ofType:@"mp3"];

	if (SongPath == nil)
	{
		debugf(TEXT("Failed to find song %s.mp3 in bundle!"), UTF8_TO_TCHAR([SongName cStringUsingEncoding:NSUTF8StringEncoding]));
		return;
	}

	// convert it to a URL (with OS4 weak reference workaround to the class)
	Class NSURLClass = NSClassFromString(@"NSURL");
	NSURL* URL = [NSURLClass fileURLWithPath:SongPath];

	if (self.MusicPlayer != nil)
	{
		if (FadeAlpha < 0.0f)
		{
			// if we are already fading out, then do nothing, but set the next song
		}
		else
		{
			// otherwise, fade out from our current fade level (so, FadeAlpha of 0, which is
			// full volume, will go to -1, which is what we want use to fade from full volume), 
			// and if FadeAlpha is 1.0, which silent, then start fading out from silent
			// clamp to 0.01f to make sure we hit the proper checks in UpdatePlayerVolume
			FadeAlpha = -(1.0f - Min<float>(FadeAlpha, 0.99f));
		}

		// loop forever, set this again as a call to disable looping may have happened
		self.MusicPlayer.numberOfLoops = -1;

		// keep a pointer to the string
		self.NextSongToPlay = SongName;
	}
	else
	{
		// create a music player object 
		self.MusicPlayer = [[AVAudioPlayer alloc] initWithContentsOfURL:URL error:nil];
		
		// property is now the owner
		[self.MusicPlayer release];

		// loop forever
		self.MusicPlayer.numberOfLoops = -1;

		// fade in (FadeAlpha is 1.0 - volume)
		FadeAlpha = 1.0f;
		self.MusicPlayer.volume = 0.0f;

		// Set us as the delegate
		self.MusicPlayer.delegate = self;

		if (self.PreviousSongURL != nil)
		{
			if ([URL.lastPathComponent isEqualToString:self.PreviousSongURL.lastPathComponent])
			{
				self.MusicPlayer.currentTime = self.PreviousSongPauseTime;
				self.PreviousSongURL = nil;
			}
		}

		// play the sound!
		[self.MusicPlayer play];
	}

	// start a timer
	LastFadeTime = appSeconds();
	if (self.FadeTimer == nil)
	{
		self.FadeTimer = [NSTimer scheduledTimerWithTimeInterval:0.05f target:self selector:@selector(UpdatePlayerVolume) userInfo:nil repeats:YES];
	}
}

/**
 * Stops the hardware mp3 stream
 */
- (void)StopSong
{
	if(self.bUsingBackgroundMusic)
	{
		//save this so we can start if we want game music again
		self.NextSongToPlay = nil;
		return;
	}

	if (self.MusicPlayer != nil)
	{
		// start the fade out
		FadeAlpha = -1.0f;

		// no next song
		self.NextSongToPlay = nil;

		// start a timer
		LastFadeTime = appSeconds();
		if (self.FadeTimer == nil)
		{
			self.FadeTimer = [NSTimer scheduledTimerWithTimeInterval:0.05f target:self selector:@selector(UpdatePlayerVolume) userInfo:nil repeats:YES];
		}
	}
}

/**
 * Pauses the hardware mp3 stream
 */
- (void)PauseSong
{
	if (self.MusicPlayer != nil)
	{
		self.PreviousSongURL = [self.MusicPlayer.url copy];
		self.PreviousSongPauseTime = self.MusicPlayer.currentTime;
		[self.MusicPlayer pause];
	}
}

/**
 * Resumes a paused song on the hardware mp3 stream
 */
- (void)ResumeSong
{
	if (self.MusicPlayer != nil)
	{
		[self.MusicPlayer play];
	}
}

/**
 * Resumes the previous song from the point in playback where it was paused before the current song started playing
 */
- (void)ResumePreviousSong
{
	if (self.PreviousSongURL != nil)
	{
		[self StopSong];

		NSString* extension = self.PreviousSongURL.pathExtension;
		NSUInteger index = [self.PreviousSongURL.lastPathComponent rangeOfString:extension].location - 1;
		NSString* songName = [self.PreviousSongURL.lastPathComponent substringToIndex:index];
		[self PlaySong:songName];
	}
}

- (void)DisableSongLooping
{
	if (self.MusicPlayer != nil)
	{
		// disable the looping, next PlaySong call will restore default to loop forever
		self.MusicPlayer.numberOfLoops = 0;
	}	
}

/**
 * Callback for when device orientation changes
 */
- (void) didRotate:(NSNotification *)notification
{	

}

/** Audio session callback for interruption */
-(void) beginInterruption
{
/** @todo ib2merge: Deal with audio interruption changes - Don't used these, we'll use our handler interruptionListener */
/*
	// Shutdown ALAudio sounds 
	UAudioDeviceIPhone::SuspendContext();

	// Disable our audio session
	NSError* setActiveCategoryError = nil;
	[[AVAudioSession sharedInstance] setActive: NO error: &setActiveCategoryError];
	if (setActiveCategoryError != nil)
	{
		NSLog(@"AudioSession::beginInterruption> FAILED to set audio session inactive!");
	}
*/
}

/** Audio session callback for resuming interruption */
-(void) endInterruption
{
/*** CHAIR - Don't used these, we'll use our handler interruptionListener
	// Activate the audio session
	NSError* setActiveCategoryError = nil;
	[[AVAudioSession sharedInstance] setActive: YES error: &setActiveCategoryError];
	if (setActiveCategoryError != nil)
	{
		NSLog(@"AudioSession::endInterruption>  FAILED to set audio session active!");
	}
	else
	{
		// Start our music player again
		if (self.MusicPlayer != nil)
		{
			// if done fading, set volume
			self.MusicPlayer.volume = 1.0f * VolumeMultiplier;
			// stop the timer
			FadeAlpha = 0.0f;
			[self.MusicPlayer play];
		}
	}

	// resume ALAudio sounds 
	UAudioDeviceIPhone::ResumeContext();
*/
}

void appUncaughtExceptionHandler(NSException* inException)
{
	FString ExceptionName(inException.name);
	FString ExceptionReason(inException.reason);
	FString OutputMessage = FString::Printf(TEXT("appUncaughtExceptionHandler: %s - %s"), *ExceptionName, *ExceptionReason);
	appCaptureCrashCallStack(*OutputMessage, 1.5f, -3);
}

void appTerminateHandler()
{
	NSLog(@"Termination handler!");
	appCaptureCrashCallStack(TEXT("appTerminateHandler"), 1.5f, -4);
}

- (void)ProcessScreenAttached:(UIScreen*)ExternalScreen
{
#if WITH_ES2_RHI
	// Are secondary screens even allowed?
	if (!GSystemSettings.bAllowSecondaryDisplays)
	{
		return;
	}

	// Process the newly attached screen after suspending the game and render threads
	FScopedSuspendGameAndRenderThreads();

	UIScreenMode* BestMode = nil;
	for (UIScreenMode* Mode in ExternalScreen.availableModes)
	{
		NSLog(@" Found Mode %f x %f", Mode.size.width, Mode.size.height);
		if (BestMode == nil ||
			(Mode.size.width >= BestMode.size.width && Mode.size.width <= GSystemSettings.SecondaryDisplayMaximumWidth) &&
			(Mode.size.height >= BestMode.size.height && Mode.size.height <= GSystemSettings.SecondaryDisplayMaximumHeight))
		{
			BestMode = Mode;
		}
	}
	NSLog(@" Best Mode is %f x %f", BestMode.size.width, BestMode.size.height);
	ExternalScreen.currentMode = BestMode;

	// create a new window for the external screen (keeping the current window on the device)
	self.SecondaryWindow = [[UIWindow alloc] init];
	self.SecondaryWindow.screen = ExternalScreen;
	self.SecondaryWindow.frame.size = ExternalScreen.currentMode.size;
	self.SecondaryWindow.clipsToBounds = YES;
	// property is the owner
	[self.SecondaryWindow release];

	// remember parent, for adding the new view
	UIView* ParentView = self.GLView.superview;

	// move the standard view hierarchy over to the external display
	[self.GLView removeFromSuperview];
	[self.SecondaryWindow addSubview:self.GLView];

	// show this window
	[self.SecondaryWindow makeKeyAndVisible];

	// create a new GL view for the device, old one went to screen
	self.SecondaryGLView = [[EAGLView alloc] initWithFrame:ParentView.bounds];
	self.SecondaryGLView.clearsContextBeforeDrawing = NO;
	self.SecondaryGLView.multipleTouchEnabled = YES;
	// If the main GL view wasn't created yet, don't create this one here (will happen later)
	if (self.GLView->bInitialized)
	{
		// Create the renderbuffer for the new view
		[self.SecondaryGLView CreateFramebuffer:YES];
	}
	// property is the owner
	[self.SecondaryGLView release];

	// add this to the device window
	[ParentView addSubview:self.SecondaryGLView];

	// Cache the sizes for the windows
	CGFloat PrimaryCSF = self.GLView.contentScaleFactor;
	CGFloat SecondaryCSF = self.SecondaryGLView.contentScaleFactor;
	const INT PrimaryWidth = PrimaryCSF * self.GLView.superview.bounds.size.width;
	const INT PrimaryHeight = PrimaryCSF * self.GLView.superview.bounds.size.height;
	const INT SecondaryWidth = SecondaryCSF * self.SecondaryGLView.superview.bounds.size.width;
	const INT SecondaryHeight = SecondaryCSF * self.SecondaryGLView.superview.bounds.size.height;

	// Add the viewport in the game thread, after game has started up
	IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
	AsyncTask.GameThreadCallback = ^ UBOOL (void)
	{
		// Resize the viewport of the primary
		if (GEngine->GameViewport &&
			GEngine->GameViewport->Viewport &&
			GEngine->GameViewport->ViewportFrame)
		{
			GEngine->GameViewport->ViewportFrame->Resize(PrimaryWidth, PrimaryHeight, TRUE);
		}

		// Call the function to make a new secondary viewport
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		if (GameEngine)
		{
			GameEngine->CreateSecondaryViewport(SecondaryWidth, SecondaryHeight);
		}
		return TRUE;
	};
	[AsyncTask FinishedTask];
#endif
}

- (void)ProcessScreenDetached:(UIScreen*)ExternalScreen
{
#if WITH_ES2_RHI
	// Are secondary screens even allowed?
	if (!GSystemSettings.bAllowSecondaryDisplays)
	{
		return;
	}

	// Process the newly detached screen after suspending the game and render threads
	FScopedSuspendGameAndRenderThreads();

	// Clean up the secondary GL view
	UIView* ParentView = self.SecondaryGLView.superview;
	[self.SecondaryGLView removeFromSuperview];
	 self.SecondaryGLView = nil;

	// Move the primary GL view back to the device
	[self.GLView removeFromSuperview];
	[ParentView addSubview:self.GLView];

	// All done with the secondary window now
	[self.Window makeKeyWindow];
	 self.SecondaryWindow = nil;

	CGFloat PrimaryCSF = self.GLView.contentScaleFactor;
	const INT PrimaryWidth = PrimaryCSF * self.GLView.superview.bounds.size.width;
	const INT PrimaryHeight = PrimaryCSF * self.GLView.superview.bounds.size.height;

	// Tear down the viewport in the game thread
	IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
	AsyncTask.GameThreadCallback = ^ UBOOL (void)
	{
		// Resize the viewport of the primary
		if (GEngine->GameViewport &&
			GEngine->GameViewport->Viewport &&
			GEngine->GameViewport->ViewportFrame)
		{
			GEngine->GameViewport->ViewportFrame->Resize(PrimaryWidth, PrimaryHeight, TRUE);
		}

		// Call the function to close the secondary viewport
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		if (GameEngine)
		{
			GameEngine->CloseSecondaryViewports();
		}
		return TRUE;
	};
	[AsyncTask FinishedTask];
#endif
}

/** Handle the booted up event
 *
 * @param application The application objbect
 */
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
	// disable the exception handling if attached to a debugger. There isn't
	// a good way to determine if we're attached to a debugger, so you can
	// fake it with an environment variable set in the executable properties in Xcode
	NSDictionary* Env = [[NSProcessInfo processInfo] environment];
	if (![[Env valueForKey:@"debugger"] isEqual:@"true"])
	{
		NSLog(@"Installing signal handler");
	 	SetupSignalHandling();
 		NSSetUncaughtExceptionHandler(appUncaughtExceptionHandler);

		// 	std::set_terminate(appTerminateHandler);
	}
	else
	{
		NSLog(@"NOT Installing signal handler");
	}
	
	//Determine PortraitMode()
	NSString* OrientationString = [[[NSBundle mainBundle] infoDictionary] objectForKey: @"UIInterfaceOrientation"];
	if (([OrientationString compare :@"UIInterfaceOrientationPortrait"] == NSOrderedSame) || ([OrientationString compare :@"UIInterfaceOrientationPortraitUpsideDown"] == NSOrderedSame))
	{
		bIPhonePortraitMode = TRUE;
	}
	
	// make sure any controllers added here can rotate	
	self.bCanAutoRotate = YES;

	// check OS version to make sure we have the API
	OSVersion = [[[UIDevice currentDevice] systemVersion] floatValue];
    
	// create the main landscape window object
	CGRect MainFrame = [[UIScreen mainScreen] bounds];
	if (!bIPhonePortraitMode)
	{
		Swap<float>(MainFrame.size.width, MainFrame.size.height);
	}
	self.Window = [[UIWindow alloc] initWithFrame:MainFrame];
	self.Window.screen = [UIScreen mainScreen];
	
	// show it
	[self.Window makeKeyAndVisible];

	// make a controller object
	self.Controller = [[IPhoneViewController alloc] init];		
	
	// property owns it now
	[self.Controller release];

	// point to the GL view we want to use
	self.RootView = [Controller view];
	
#if defined(__IPHONE_6_0)
	[self.Window setRootViewController:self.Controller];
#else
	[self.Window addSubview:self.RootView];
#endif

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	// Initialize the console history array
	self.ConsoleHistoryValues = [[NSUserDefaults standardUserDefaults] objectForKey:@"ConsoleHistory"];
	if (self.ConsoleHistoryValues == nil)
	{
		self.ConsoleHistoryValues = [[NSMutableArray alloc] init];
	}
	ConsoleHistoryValuesIndex = -1;
#endif

	// Disable any "shake to undo" functionality in the application (generally for the console)
	application.applicationSupportsShakeToEdit = NO;

	// reset badge count on launch
	application.applicationIconBadgeNumber = 0;

/*** CHAIR - Fix for ALAudio interruption START */
 	//Make this configurable to support users playing their own music.	 	
	OSStatus result = AudioSessionInitialize(NULL, NULL, interruptionListener, NULL); 
	if (result) 
		NSLog(@"AudioSession: Error initializing audio session! %d", result); 
/*** CHAIR - Fix for ALAudio interruption END */

	//Find out if other audio is playing
	UInt32 isOtherAudioPlaying = 0; 
	UInt32 size = sizeof(isOtherAudioPlaying); 
	AudioSessionGetProperty(kAudioSessionProperty_OtherAudioIsPlaying, &size, &isOtherAudioPlaying); 
    
	//If it is then keep it going, otherwise we'll use our own music
	self.bUsingBackgroundMusic = isOtherAudioPlaying > 0;
 		 	
	NSError* setAudioCategoryError = nil;
	if(!self.bUsingBackgroundMusic)
	{
		// Setup our audio session
		NSError* setActiveError = nil;
 		[[AVAudioSession sharedInstance] setActive: YES error: &setActiveError];
 		if (setActiveError != nil)
 		{
 			NSLog(@"AudioSession: Failed to set audio active!");
 		}
	
		// Steal it all for us
		[[AVAudioSession sharedInstance] setCategory: AVAudioSessionCategorySoloAmbient error: &setAudioCategoryError];
	}
	else
	{
		// We want to support background ipod music
		[[AVAudioSession sharedInstance] setCategory: AVAudioSessionCategoryAmbient error: &setAudioCategoryError];	
	}
	if (setAudioCategoryError != nil)
 	{
		NSLog(@"AudioSession: Failed to set audio category to SoloAmbient!");
 	}

	// Set ourselves as the delegate 	
	[[AVAudioSession sharedInstance] setDelegate: self];

	// Initialize
	self.MusicPlayer = nil;


	// Get the movie going soon!
	// @todo: handle disabling sound
	if( ParseParam(appCmdLine(),TEXT("nomovie")) )
	{
		GFullScreenMovie = FFullScreenMovieFallback::StaticInitialize(TRUE);
	}
	else
	{
		GFullScreenMovie = FFullScreenMovieIPhone::StaticInitialize(TRUE);
	}

	// create a new thread 
	[NSThread detachNewThreadSelector:@selector(MainUE3GameThread:) toTarget:self withObject:launchOptions]; 

	self.UE3InitCheckTimer = [NSTimer scheduledTimerWithTimeInterval:0.01f target:self selector:@selector(UE3InitializationComplete) userInfo:nil repeats:YES];

	// listen for external screen attach/detach notifications
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(ScreenAttached:)
												 name:UIScreenDidConnectNotification
											   object:nil];
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(ScreenDetached:)
												 name:UIScreenDidDisconnectNotification
											   object:nil];
	return YES;
}


#if defined(__IPHONE_6_0)
/**
 * Called to figure out the supported orientations
 */
- (NSUInteger)application:(UIApplication *)application supportedInterfaceOrientationsForWindow:(UIWindow *)Window
{
    if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad)
        return UIInterfaceOrientationMaskAll;
    else  /* iphone */
        return UIInterfaceOrientationMaskAllButUpsideDown;

	// The above is a workaround for gamecenter crashes replacing the following code:
	//
	//     return bIPhonePortraitMode ? UIInterfaceOrientationMaskPortrait | UIInterfaceOrientationMaskPortraitUpsideDown : UIInterfaceOrientationMaskLandscape;
}
#endif

/**
 * Called when an external display has been connected
 */
- (void)ScreenAttached:(NSNotification*)Notification
{
	[self ProcessScreenAttached:Notification.object];
}

/**
 * Called when an external display has been connected
 */
- (void)ScreenDetached:(NSNotification*)Notification
{
	[self ProcessScreenDetached:Notification.object];
}

- (void)UE3InitializationComplete
{
	// Wait for the UE3 thread to finish PreInit so that we'll have access to ini settings
	// which we'll use to make decisions about MSAA and contentScaleFactor
	if (bMainGLViewReadyToInitialize == NO)
	{
		return;
	}

	// kill the timer
	[self.UE3InitCheckTimer invalidate];
	self.UE3InitCheckTimer = nil;

#if WITH_ES2_RHI
	// Size the view appropriately for any potentially dynamically attached displays,
	// prior to creating any framebuffers
	CGRect MainFrame = [[UIScreen mainScreen] bounds];
	if (!bIPhonePortraitMode)
	{
		Swap<float>(MainFrame.size.width, MainFrame.size.height);
	}
	CGRect FullResolutionRect =
		CGRectMake(
			0.0f,
			0.0f,
			GSystemSettings.bAllowSecondaryDisplays ?
				Max<float>(MainFrame.size.width, GSystemSettings.SecondaryDisplayMaximumWidth)	:
				MainFrame.size.width,
			GSystemSettings.bAllowSecondaryDisplays ?
				Max<float>(MainFrame.size.height, GSystemSettings.SecondaryDisplayMaximumHeight) :
				MainFrame.size.height
		);

	// Make the root view of the controller an EAGL view
	self.GLView = [[EAGLView alloc] initWithFrame:FullResolutionRect];
	self.GLView.clearsContextBeforeDrawing = NO;
	self.GLView.multipleTouchEnabled = YES;

	// Put the GL view into the window
	[self.RootView addSubview:self.GLView];

	[self.GLView CreateFramebuffer:YES];
	if (self.SecondaryGLView != nil)
	{
		[self.SecondaryGLView CreateFramebuffer:YES];
	}

	// Do we have a second screen attached?
	if ([[UIScreen screens] count] > 1)
	{
		// If so, create the primary rendering surface, then process the new screen
		[self.GLView CreateFramebuffer:NO];
		[self ProcessScreenAttached:[[UIScreen screens] objectAtIndex:1]];
	}
	else
	{
		// If not, simply create the primary rendering surface
		[self.GLView CreateFramebuffer:YES];

		// Final adjustment to the viewport (this is deferred and won't run until the first engine tick)
		IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
		AsyncTask.GameThreadCallback = ^ UBOOL (void)
		{
			if (GEngine->GameViewport &&
				GEngine->GameViewport->Viewport &&
				GEngine->GameViewport->ViewportFrame)
			{
				CGFloat CSF = self.GLView.contentScaleFactor;
				GEngine->GameViewport->ViewportFrame->Resize(
					(INT)(CSF * self.GLView.superview.bounds.size.width),
					(INT)(CSF * self.GLView.superview.bounds.size.height),
					TRUE);
			}
			return TRUE;
		};
		[AsyncTask FinishedTask];
	}
#endif

	// Signal that we're done with the main GL View initialization
	bMainGLViewInitialized = YES;

	// perform early AL Audio initialization, because it takes between .3 and .7 seconds, so let's not block startup
	UAudioDeviceIPhone::ThreadedStaticInit();

	UIDevice* Device = [UIDevice currentDevice];
	[Device beginGeneratingDeviceOrientationNotifications];

	[[NSNotificationCenter defaultCenter] addObserver:self
		selector:@selector(didRotate:)
		name:UIDeviceOrientationDidChangeNotification object:nil];
	
	// send the PhoneHome data (self-throttling)
	debugf(TEXT("Kicking off PhoneHome request in UE3InitializationComplete"));
	[IPhoneHome queueRequest];

	// allow receiving remote notifications if requested
	UBOOL bSupportsPushNotifications = FALSE;
	GConfig->GetBool(TEXT("PushNotifications"), TEXT("bSupportsPushNotifications"), bSupportsPushNotifications, GEngineIni);

	if (bSupportsPushNotifications)
	{
		UBOOL bSupportsAlerts = FALSE, bSupportsSounds = FALSE, bSupportsBadges = FALSE;
		// figure out which type to register for
		GConfig->GetBool(TEXT("PushNotifications"), TEXT("bSupportsAlerts"), bSupportsAlerts, GEngineIni);
		GConfig->GetBool(TEXT("PushNotifications"), TEXT("bSupportsSounds"), bSupportsSounds, GEngineIni);
		GConfig->GetBool(TEXT("PushNotifications"), TEXT("bSupportsBadges"), bSupportsBadges, GEngineIni);

		// register for them, this will eventually call a callback function with the device identifier
		[[UIApplication sharedApplication] registerForRemoteNotificationTypes:(UIRemoteNotificationType)(
			(bSupportsAlerts ? UIRemoteNotificationTypeAlert : 0) |
			(bSupportsSounds ? UIRemoteNotificationTypeSound : 0) |
			(bSupportsBadges ? UIRemoteNotificationTypeBadge : 0)
			)];
	}
}

extern time_t GAppInvokeTime;

- (void)applicationWillResignActive:(UIApplication *)application 
{

#if WITH_GFx && defined(SF_AMP_SERVER)
	// close Scaleform GFx AMP connection state
	Scaleform::AmpServer::GetInstance().CloseConnection();
#endif

	if(!self.bUsingBackgroundMusic)
	{
		NSError* setActiveCategoryError = nil;
		[[AVAudioSession sharedInstance] setActive: NO error: &setActiveCategoryError];
		if (setActiveCategoryError != nil)
		{
			NSLog(@"AudioSession::beginInterruption> FAILED to set audio session inactive! %@", setActiveCategoryError);
		}
				
		NSError* setAudioCategoryError = nil;
		// We want to support background ipod music, seems that we need to set this mode or we'll kill audio when coming back
		[[AVAudioSession sharedInstance] setCategory: AVAudioSessionCategoryAmbient error: &setAudioCategoryError];	
	}

	// tell the main thread to stop rendering
	GForceStopRendering = TRUE;

	// log the time the user spent in the app
	time_t Now = time(NULL);
	IPhoneIncrementUserSettingU64("IPhoneHome::AppPlaytimeSecs", (uint64_t)(Now - GAppInvokeTime));

	// if we are unloaded while in the BG, it's okay
	IPhoneSaveUserSettingU64("IPhoneHome::LastRunCrashed", 0);

	// wait for the stop to happen
	while (!GHasStoppedRendering)
	{
		appSleep(0.05f);
	}

	// suspend OpenAL
	UAudioDeviceIPhone::SuspendContext();
}

- (void)applicationDidBecomeActive:(UIApplication *)application 
{
	BOOL bWasUsingBackgroundMusic = self.bUsingBackgroundMusic;

    //Find out if other audio is playing now
    UInt32 isOtherAudioPlaying = 0; 
    UInt32 size = sizeof(isOtherAudioPlaying); 
    AudioSessionGetProperty(kAudioSessionProperty_OtherAudioIsPlaying, &size, &isOtherAudioPlaying); 
    
    //If it is then keep it going, otherwise we'll use our own music
	self.bUsingBackgroundMusic = isOtherAudioPlaying > 0;
 	
 	if(bWasUsingBackgroundMusic != self.bUsingBackgroundMusic)
 	{	 	
 		//things have changed, time to update state
		NSError* setAudioCategoryError = nil;
		if(!self.bUsingBackgroundMusic)
		{
			// Setup our audio session
			NSError* setActiveError = nil;
 			[[AVAudioSession sharedInstance] setActive: YES error: &setActiveError];
 			if (setActiveError != nil)
 			{
 				NSLog(@"AudioSession applicationDidBecomeActive: Failed to set audio active!");
 			}
		
			NSError* setAudioCategoryError = nil;
			// Steal it all for us
			[[AVAudioSession sharedInstance] setCategory: AVAudioSessionCategorySoloAmbient error: &setAudioCategoryError];
			
			// start our music up now
			[self PlaySong:self.NextSongToPlay];
		}
		else
		{
			// kill the current player
			if (self.MusicPlayer != nil)
			{
				[self.MusicPlayer stop];
				self.MusicPlayer = nil;
			}		

			NSError* setAudioCategoryError = nil;
			// We want to support background ipod music
			[[AVAudioSession sharedInstance] setCategory: AVAudioSessionCategoryAmbient error: &setAudioCategoryError];	
		}
		
		if (setAudioCategoryError != nil)
		{
			NSLog(@"AudioSession applicationDidBecomeActive: Failed to set audio category to SoloAmbient!");
		}	
	}	
	else
	{
		if(!self.bUsingBackgroundMusic)
		{			
			NSError* setActiveCategoryError = nil;
			[[AVAudioSession sharedInstance] setActive: YES error: &setActiveCategoryError];
			if (setActiveCategoryError != nil)
			{
				NSLog(@"AudioSession::applicationDidBecomeActive> FAILED to set audio session active!");
			}
			
			NSError* setAudioCategoryError = nil;
			// Steal it all back for us
			[[AVAudioSession sharedInstance] setCategory: AVAudioSessionCategorySoloAmbient error: &setAudioCategoryError];				
		}
	}

	// resume OpenAL
	UAudioDeviceIPhone::ResumeContext();

	// we are no longer forcing the game thread to stop rendering
	GForceStopRendering = FALSE;

	// reset our invocation time
	GAppInvokeTime = time(NULL);

	// increment num invocations stat
	IPhoneIncrementUserSettingU64("IPhoneHome::NumInvocations");

	// reset crash flag
	IPhoneSaveUserSettingU64("IPhoneHome::LastRunCrashed", 1);

	// send the PhoneHome data (self-throttling)
 	[IPhoneHome queueRequest];

	// reset badge count when app is re-activated
	application.applicationIconBadgeNumber = 0;

#if WITH_GFx && defined(SF_AMP_SERVER)
	// (re)open Scaleform GFx AMP connection state
	Scaleform::AmpServer::GetInstance().OpenConnection();
#endif

}

static DOUBLE LastPauseTime;

/**
 * Notification that the application is heading in to the background, and
 * rendering must cease
 */
- (void)applicationDidEnterBackground:(UIApplication*)application
{
#if WITH_SWRVE
	extern void FlushSwrveEventsToDiskIfNecessary();
	FlushSwrveEventsToDiskIfNecessary();
#endif
	LastPauseTime = appSeconds();
}

/**
 * Notification that we have returned to the foreground, so we may resume
 * rendering with OpenGL
 */
- (void)applicationWillEnterForeground:(UIApplication*)application
{
	DOUBLE PauseTimeSec = appSeconds() - LastPauseTime;
	UAnalyticEventsBase* Analytics = UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton();
	if (Analytics->bSessionInProgress && Analytics->SessionPauseThresholdSec > 0 && PauseTimeSec >= Analytics->SessionPauseThresholdSec)
	{
		debugf(NAME_DevStats, TEXT("Application entering foreground after %d seconds. Analytics session will be restarted."), (INT)PauseTimeSec);
		Analytics->EndSession();
		Analytics->StartSession();
	}
}

/**
 * Handle shutdown event
 */
- (void)applicationWillTerminate:(UIApplication *)application 
{
	// note that we are shutting down
	GIsRequestingExit = TRUE;

	// this is a clean exit
	IPhoneSaveUserSettingU64("IPhoneHome::LastRunCrashed", 0);
	
	// ensure the analytics session is ended immediately.
	UAnalyticEventsBase* Analytics = UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton();
	if (Analytics->bSessionInProgress)
	{
		debugf(NAME_DevStats, TEXT("Application terminating. Analytics session will be ended."));
		Analytics->EndSession();
	}

	// wait until the game thread has noticed the shutdown
	while ( GIsStarted == TRUE )
	{
		usleep( 3 );
	}
}

/**
 * Handle low memory warning
 */
- (void)applicationDidReceiveMemoryWarning:(UIApplication *)application 
{
	IPhoneIncrementUserSettingU64("IPhoneHome::NumMemoryWarnings");
}


/**
 * Callback when we have registered for remote notifications
 */
- (void)application:(UIApplication *)Application didRegisterForRemoteNotificationsWithDeviceToken:(NSData*)DeviceToken
{ 
	// send the device ID to web server
	FString RegistrationURL;
	if (!GConfig->GetString(TEXT("PushNotifications"), TEXT("RegistrationServerURL"), RegistrationURL, GEngineIni))
	{
		appErrorf(TEXT("It is not allowed to have push notifications enabled without having RegistrationServerURL specified"));
	}
	
	// convert UDID and token to strings, to send to server
	NSData* UDIDData = [[[UIDevice currentDevice] uniqueIdentifier] dataUsingEncoding:NSUTF8StringEncoding];
	NSString* DeviceIDString = DataToString(UDIDData, YES);
	FString DeviceID(DeviceIDString);
	NSString* DeviceTokenString = DataToString(DeviceToken, NO);
	FString DeviceTokenFS(DeviceTokenString);

	debugf(TEXT("Push Notification registration for Device UDID [%s] with Device Token [%s]"), *DeviceID, *DeviceTokenFS);
	
	// save the token off, since this is our only opportunity to do so on the device
	IPhoneSaveUserSetting("DevicePushNotificationToken", TCHAR_TO_ANSI(*DeviceTokenFS));

	// replace the two `~'s in the URL ini string
	FString FullURL = FString::Printf( LocalizeSecure(RegistrationURL, *DeviceID, *DeviceTokenFS) );

	ANSICHAR ValString[512];
	// Load the last URL that we sent this to, so we don't update when it wasn't needed (saves money)
	// NOTE: Do this on the URL in case we move where to send them
	IPhoneLoadUserSetting("DevicePushNotificationURL", ValString, ARRAY_COUNT(ValString) - 1);
	FString LastPushNotificationURL(ANSI_TO_TCHAR(ValString));

	// Only send an update if the current URL with current Token don't match our last one
	if (LastPushNotificationURL != FullURL)
	{
		// Save the URL that we sent this to
		IPhoneSaveUserSetting("DevicePushNotificationURL", TCHAR_TO_ANSI(*FullURL));

		// @todo ib2merge: Chair appended ib2 secret here (deleted to keep it secret!)

		// get the URL from the .ini
		Class NSURLClass = NSClassFromString(@"NSURL");
		NSURL* URL = [NSURLClass URLWithString:[NSString stringWithUTF8String:TCHAR_TO_UTF8(*FullURL)]];


		// create a GET HTTP request to send the device token
		NSMutableURLRequest* Request = [NSMutableURLRequest requestWithURL:URL];
		[Request setHTTPMethod:@"POST"];

		// append a UE3-specific user agent string, for custom servers
		FString UserAgent = FString::Printf(TEXT("UE3-%s,UE3Ver(%d)"),appGetGameName(),GEngineVersion);
		NSString* UserAgentStr = [NSString stringWithCString:TCHAR_TO_UTF8(*UserAgent) encoding:NSUTF8StringEncoding];
		[Request addValue:UserAgentStr forHTTPHeaderField:@"User-Agent"];

		// send it asynchronously
		[NSURLConnection connectionWithRequest:Request delegate:self]; 
	}
}

- (void)application:(UIApplication *)Application didFailToRegisterForRemoteNotificationsWithError:(NSError*)Error
{ 
	NSLog(@"FAILED to register for remote notifications, error is %@", Error);
}

/**
 * Remote push notification received
 */
- (void)application:(UIApplication *)application didReceiveRemoteNotification:(NSDictionary*)userInfo
{
	if (userInfo != nil)
	{
		// Determine if the app was active when processing this notification
		UBOOL bWasAppActive = application.applicationState == UIApplicationStateActive;
		// Make sure to keep this object around until the async task uses it
		[userInfo retain];
		// Game thread lambda
		IPhoneAsyncTask* Task = [[IPhoneAsyncTask alloc] init];
 		Task.GameThreadCallback = ^ UBOOL (void)
 		{
			// copy user info to game thread
			UAppNotificationsIPhone* NotificationHandler = Cast<UAppNotificationsIPhone>(UPlatformInterfaceBase::GetAppNotificationsInterfaceSingleton());
			if (NotificationHandler != NULL)
			{
				NotificationHandler->ProcessRemoteNotification(userInfo,bWasAppActive);
			}
 			return TRUE;
 		};
 		[Task FinishedTask];

		// get the Message from the notification
		NSString* Message = nil;
		// pull out the standard aps value, which is antother dictionary
		NSDictionary* AlertDictionary = [userInfo objectForKey:@"aps"];
		// the message could be the 'alert' value, or it could be the 'body' of a sub-dictionary
		id Alert = [AlertDictionary objectForKey:@"alert"];
		if ([Alert isKindOfClass:[NSDictionary class]])
		{
			Message = [Alert objectForKey:@"body"];
		}
		else
		{
			Message = Alert;
		}

		// if the app is inactive, then the app was in the background, and the OS already showed
		// an alert, so no need to show it again
		if (application.applicationState == UIApplicationStateActive)
		{
			// the OS won't show an alert if we are already running, so show a message
			NSString* AppName = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleDisplayName"];
			UIAlertView *AlertView = [[UIAlertView alloc] initWithTitle:AppName
																message:Message
															   delegate:self
													  cancelButtonTitle:@"OK"
													  otherButtonTitles:nil];
			[AlertView show];
			[AlertView release];
		}

		// set the current requested badge # for the app icon
//		NSString* badge = [AlertDictionary objectForKey:@"badge"];
//		if (badge != nil)
//		{
//			application.applicationIconBadgeNumber = [badge integerValue];
//		}
	}
}

/**
 * Local push notification received
 */
- (void)application:(UIApplication *)application didReceiveLocalNotification:(UILocalNotification *)notification
{
	if (notification != nil)
	{
		// Determine if the app was active when processing this notification
		UBOOL bWasAppActive = application.applicationState == UIApplicationStateActive;
		// Add ref until the game thread finishes with it
		[notification retain];
		// Game thread lambda
		IPhoneAsyncTask* Task = [[IPhoneAsyncTask alloc] init];
 		Task.GameThreadCallback = ^ UBOOL (void)
 		{
			// copy user info to game thread
			UAppNotificationsIPhone* NotificationHandler = Cast<UAppNotificationsIPhone>(UPlatformInterfaceBase::GetAppNotificationsInterfaceSingleton());
			if (NotificationHandler != NULL)
			{
				NotificationHandler->ProcessLocalNotification(notification,bWasAppActive);
			}
 			return TRUE;
 		};
 		[Task FinishedTask];

		// if the app is inactive, then the app was in the background, and the OS already showed
		// an alert, so no need to show it again
		if (application.applicationState == UIApplicationStateActive)
		{
			// @todo ib2merge: Chair doesn't want this, have an internal system, is that internal system for all games?
			// the OS won't show an alert if we are already running, so show a message
			NSString* AppName = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleDisplayName"];
			UIAlertView *AlertView = [[UIAlertView alloc] initWithTitle:AppName
																message:notification.alertBody
															   delegate:self
													  cancelButtonTitle:@"OK"
													  otherButtonTitles:nil];
			[AlertView show];
			[AlertView release];
			
			//let's clean up the badge though
			application.applicationIconBadgeNumber = 0;	
		}

		// set the current requested badge # for the app icon
//		application.applicationIconBadgeNumber = notification.applicationIconBadgeNumber;
	}
}

/** 
 * Shows an alert with up to 3 buttons. A delegate callback will later set AlertResponse property
 */
- (void)ShowAlert:(NSMutableArray*)StringArray
{
	// reset our response to unset
	self.AlertResponse = -1;

	// set up the alert message and buttons
	UIAlertView *Alert = [[[UIAlertView alloc] initWithTitle:[StringArray objectAtIndex:0]
													 message:[StringArray objectAtIndex:1]
													delegate:self // use ourself to handle the button clicks 
										   cancelButtonTitle:[StringArray objectAtIndex:2]
										   otherButtonTitles:nil] autorelease];

	// add any extra buttons
	for (INT OptionalButtonIndex = 3; OptionalButtonIndex < [StringArray count]; OptionalButtonIndex++)
	{
		[Alert addButtonWithTitle:[StringArray objectAtIndex:OptionalButtonIndex]];
	}

	// show it!
	[Alert show];
}

/**
 * An alert button was pressed
 */
- (void)alertView:(UIAlertView*)AlertView clickedButtonAtIndex:(NSInteger)ButtonIndex
{
	// just set our AlertResponse property, all we need to do
	self.AlertResponse = ButtonIndex;
}

/** 
 * Brings up an on-screen keyboard for input.
 */
- (void)GetUserInput:(NSDictionary *)info
{
	NSString *Title = [info objectForKey:@"Title"];
	NSString *InitValue = [info objectForKey:@"InitValue"];
	self.UserInputExec = [info objectForKey:@"ExecFunc"];
	self.UserInputCancel = [info objectForKey:@"CancelFunc"];

	NSString *CharacterLimit = [info objectForKey:@"CharLimit"];
	self.UserInputCharacterLimit = [CharacterLimit intValue];

	// Set up a containing alert message and buttons
	self.UserInputAlert = [[UIAlertView alloc] initWithTitle:Title
												   message:@"\n"
												  delegate:self
										 cancelButtonTitle:NSLocalizedString(@"Cancel", nil)
										 otherButtonTitles:nil];//NSLocalizedString(@"OK", nil), nil];
	[self.UserInputAlert addButtonWithTitle:NSLocalizedString(@"OK", nil)];
	// The property is now the owner
	[self.UserInputAlert release];

	// Set up the text field used for putting in the console command
	if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad)
	{
		// iPad has a slightly different offset for the text box
		self.UserInputField = [[UITextField alloc] initWithFrame:CGRectMake(12, 46, 261, 24)];
	}
	else
	{
		self.UserInputField = [[UITextField alloc] initWithFrame:CGRectMake(12, 32, 261, 24)];
	}
	// The property is now the owner
	[self.UserInputField release];

	self.UserInputField.clearsOnBeginEditing = NO;
	self.UserInputField.font = [UIFont systemFontOfSize:16.0];
	self.UserInputField.keyboardAppearance = UIKeyboardAppearanceAlert;
	self.UserInputField.autocorrectionType = UITextAutocorrectionTypeNo;
	self.UserInputField.autocapitalizationType = UITextAutocapitalizationTypeSentences;
	self.UserInputField.borderStyle = UITextBorderStyleRoundedRect;
	self.UserInputField.placeholder = InitValue;
	self.UserInputField.clearButtonMode = UITextFieldViewModeWhileEditing;
	self.UserInputField.delegate = self;

	// Add the children to the alert view
	[self.UserInputAlert addSubview:self.UserInputField];
	[self.UserInputAlert show];

	// Bring up the keyboard for the text field
	[self.UserInputField becomeFirstResponder];
}

/** 
 * Determine whether the specified text should be changed or not upon input
 */
- (BOOL)textField:(UITextField *)textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string;
{
	NSString *CurrentText = [textField.text stringByReplacingCharactersInRange:range withString:string];
	return (self.UserInputCharacterLimit == 0 || ([CurrentText length] <= self.UserInputCharacterLimit));
}

/** 
 * Brings up an on-screen keyboard for input.
 */
- (void)GetUserInputMulti:(NSDictionary *)info
{
	NSString *Title = [info objectForKey:@"Title"];
	NSString *InitValue = [info objectForKey:@"InitValue"];
	self.UserInputExec = [info objectForKey:@"ExecFunc"];

	// Anything more that 4 lines does not make the info box bigger, just gives a scrollable text box.
	int NumLines = 3;
	int Top = 34;
	int Height = 60;

	switch (IPhoneGetDeviceType())
	{
		case IOS_IPhone3GS:
		case IOS_IPhone4:
		case IOS_IPhone4S:
		case IOS_IPhone5:
		case IOS_IPodTouch4:
		case IOS_IPodTouch5:		
			Top = 34;
			Height = 60;
			NumLines = 3;
			break;
		case IOS_IPad:
			Top = 34;
			Height = 80;
			NumLines = 4;
			break;
		case IOS_IPad2:
		case IOS_IPad3:
		case IOS_IPad4:
		case IOS_IPadMini:
			Top = 46;
			Height = 92;
			NumLines = 4;
			break;
		case IOS_Unknown:
			break;
	}

	NSString *StretchYArea = @" \n";
	for (int Idx = 1; Idx < NumLines; Idx++)
	{
		StretchYArea = [StretchYArea stringByAppendingString:@" \n"];
	}

	// Set up a containing alert message and buttons
	self.UserInputAlert = [[UIAlertView alloc] initWithTitle:Title
												   message:StretchYArea
												  delegate:self
										 cancelButtonTitle:NSLocalizedString(@"Cancel", nil)
										 otherButtonTitles:nil];
	[self.UserInputAlert addButtonWithTitle:NSLocalizedString(@"OK", nil)];
	// The property is now the owner
	[self.UserInputAlert release];

	self.UserInputView = [[UITextView alloc] initWithFrame:CGRectMake(12, Top, 261, Height)];

	// The property is now the owner: 
	[self.UserInputView release];

	self.UserInputView.font = [UIFont systemFontOfSize:16.0];
	self.UserInputView.keyboardAppearance = UIKeyboardAppearanceAlert;
	self.UserInputView.autocorrectionType = UITextAutocorrectionTypeYes;
	self.UserInputView.autocapitalizationType = UITextAutocapitalizationTypeSentences;

	// There is no placeholder in UITextview, here is a way to implement it: 
	// http://stackoverflow.com/questions/7038876/iphonehow-to-insert-placeholder-in-uitextview
	//self.UserInputField.placeholder = InitValue;
	self.UserInputView.text = InitValue;

	self.UserInputView.delegate = self;

	// Add the children to the alert view
	[self.UserInputAlert addSubview:self.UserInputView];
	[self.UserInputAlert show];

	// Bring up the keyboard for the text field
	[self.UserInputField becomeFirstResponder];
}

- (void)alertView:(UIAlertView*)AlertView didDismissWithButtonIndex:(NSInteger)ButtonIndex
{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	// If the AlertView is the console, handle it specially
	if( AlertView == self.ConsoleAlert )
	{
		// Let go of the keyboard before dismissing to avoid
		// "wait_fences: failed to receive reply: 10004003"
		[self.ConsoleTextField resignFirstResponder];

		// If the button was the cancel button, clear the text to ignore it
		if( ButtonIndex != self.ConsoleAlert.cancelButtonIndex &&
			self.ConsoleTextField.text.length > 0 )
		{
			// Make a copy of the string for the command so the block will have
			// valid data by the time it executes on the main thread
			NSString* CopyOfConsoleText = self.ConsoleTextField.text;

			IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
			AsyncTask.GameThreadCallback = ^ UBOOL (void)
			{
				FString ConsoleCommand;
				ConsoleCommand = FString(CopyOfConsoleText);

				new(GEngine->DeferredCommands) FString(ConsoleCommand);
				return TRUE;
			};
			[AsyncTask FinishedTask];

			// And add the command to the command history, if different than last
			BOOL bShouldAddCommand = YES;
			if( self.ConsoleHistoryValues.count > 0 &&
				[self.ConsoleTextField.text caseInsensitiveCompare:[self.ConsoleHistoryValues objectAtIndex:0]] == NSOrderedSame )
			{
				bShouldAddCommand = NO;
			}
			if( bShouldAddCommand )
			{
				[self.ConsoleHistoryValues insertObject:self.ConsoleTextField.text atIndex:0];
				if( self.ConsoleHistoryValues.count > 42 )
				{
					[self.ConsoleHistoryValues removeLastObject];
				}

				// save the history
				[[NSUserDefaults standardUserDefaults] setObject:self.ConsoleHistoryValues forKey:@"ConsoleHistory"];
			}
		}
		// Reset the history index any time the window closes
		self.ConsoleHistoryValuesIndex = -1;

		// We're done with these objects now
		self.ConsoleTextField = nil;
		self.ConsoleAlert = nil;
	}
#endif // !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE

	// If the AlertView is for user input, handle it here.
	if( AlertView == self.UserInputAlert )
	{
		NSString * SendString = nil;
		if (self.UserInputField != nil)
		{
			// Let go of the keyboard before dismissing to avoid
			// "wait_fences: failed to receive reply: 10004003"
			[self.UserInputField resignFirstResponder];

			// If the button was the cancel button, clear the text to ignore it
			if( ButtonIndex != self.UserInputAlert.cancelButtonIndex &&	self.UserInputField.text.length > 0 )
			{
				SendString = self.UserInputField.text;
			}
		}
		else if (self.UserInputView != nil)
		{
			// Let go of the keyboard before dismissing to avoid
			// "wait_fences: failed to receive reply: 10004003"
			[self.UserInputView resignFirstResponder];
			
			if( ButtonIndex != self.UserInputAlert.cancelButtonIndex &&	self.UserInputView.text.length > 0 )
			{
				// @todo ib2merge: We need to add the option now, to escape or not!
				// NOTE: We are encoding all strings at this point because this is currently 
				// only used for facebook posts.  I would add an option, but we supposed to be at ZBR, so 
				// a quick fix.
//				SendString = (NSString *)CFURLCreateStringByAddingPercentEscapes(
//					NULL,
//					(CFStringRef)self.UserInputView.text,
//					NULL,
//					(CFStringRef)@"!*'();:@&=+$,/?%#[]",
//					kCFStringEncodingUTF8
//				);
			}
		}
		
		// Make a copy of the string for the command so the block will have
		// valid data by the time it executes on the main thread
		NSString* ExecString = self.UserInputExec;
		if (SendString != nil)
		{
			ExecString = [ExecString stringByAppendingString:@" "];
			ExecString = [ExecString stringByAppendingString:SendString];
		}
		//debugf(TEXT("Will Call:%s"), UTF8_TO_TCHAR([ExecString cStringUsingEncoding:NSUTF8StringEncoding]));

		IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
		AsyncTask.GameThreadCallback = ^ UBOOL (void)
		{
			FString ExecStr = FString(ExecString);
			debugf(TEXT("Calling:%s"), *ExecStr);

			// Issue the console command to the engine
			new(GEngine->DeferredCommands) FString( ExecStr );
			return TRUE;
		};
		[AsyncTask FinishedTask];

		// We're done with these objects now
		self.UserInputField = nil;
		self.UserInputView = nil;
		self.UserInputAlert = nil;
		self.UserInputExec = nil;
	}
}


#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
/** 
 * Shows the console and brings up an on-screen keyboard for input
 */
- (void)ShowConsole
{
	self.UserInputCharacterLimit = 0;
	// Set up a containing alert message and buttons
	self.ConsoleAlert = [[UIAlertView alloc] initWithTitle:@"Type a console command"
												   message:@"\n"
												  delegate:self
										 cancelButtonTitle:NSLocalizedString(@"Cancel", nil)
										 otherButtonTitles:nil];//NSLocalizedString(@"OK", nil), nil];
	[self.ConsoleAlert addButtonWithTitle:NSLocalizedString(@"OK", nil)];
	// The property is now the owner
	[self.ConsoleAlert release];

	// Set up the text field used for putting in the console command
	if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad)
	{
		// iPad has a slightly different offset for the text box
		self.ConsoleTextField = [[UITextField alloc] initWithFrame:CGRectMake(12, 46, 261, 24)];
	}
	else
	{
		self.ConsoleTextField = [[UITextField alloc] initWithFrame:CGRectMake(12, 32, 261, 24)];
	}
	// The property is now the owner
	[self.ConsoleTextField release];

	self.ConsoleTextField.clearsOnBeginEditing = NO;
	self.ConsoleTextField.font = [UIFont systemFontOfSize:16.0];
	self.ConsoleTextField.keyboardAppearance = UIKeyboardAppearanceAlert;
	self.ConsoleTextField.autocorrectionType = UITextAutocorrectionTypeNo;
	self.ConsoleTextField.autocapitalizationType = UITextAutocapitalizationTypeNone;
	self.ConsoleTextField.borderStyle = UITextBorderStyleRoundedRect;
	self.ConsoleTextField.placeholder = @"or swipe for history";
	self.ConsoleTextField.clearButtonMode = UITextFieldViewModeWhileEditing;
	self.ConsoleTextField.delegate = self;

	// Add gesture recognizers
	UISwipeGestureRecognizer* SwipeUpGesture = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(SwipeUpAction:)];
	SwipeUpGesture.direction = UISwipeGestureRecognizerDirectionUp;
	SwipeUpGesture.delegate = self;
	[self.ConsoleAlert addGestureRecognizer:SwipeUpGesture];

	UISwipeGestureRecognizer* SwipeDownGesture = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(SwipeDownAction:)];
	SwipeDownGesture.direction = UISwipeGestureRecognizerDirectionDown;
	SwipeDownGesture.delegate = self;
	[self.ConsoleAlert addGestureRecognizer:SwipeDownGesture];

	UISwipeGestureRecognizer* SwipeLeftGesture = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(SwipeLeftAction:)];
	SwipeLeftGesture.direction = UISwipeGestureRecognizerDirectionLeft;
	SwipeLeftGesture.delegate = self;
	[self.ConsoleAlert addGestureRecognizer:SwipeLeftGesture];

	UISwipeGestureRecognizer* SwipeRightGesture = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(SwipeRightAction:)];
	SwipeRightGesture.direction = UISwipeGestureRecognizerDirectionRight;
	SwipeRightGesture.delegate = self;
	[self.ConsoleAlert addGestureRecognizer:SwipeRightGesture];

	// Add the children to the alert view
	[self.ConsoleAlert addSubview:self.ConsoleTextField];
	[self.ConsoleAlert show];

	// Bring up the keyboard for the text field
	[self.ConsoleTextField becomeFirstResponder];
}

- (BOOL)textFieldShouldClear:(UITextField*)TextField
{
	// Reset the history index any time the text is cleared
	self.ConsoleHistoryValuesIndex = -1;
	return YES;
}

- (BOOL)textFieldShouldReturn:(UITextField*)TextField
{
	// If return is pressed in the console, simply dismiss the console alert with "OK"
	[self.ConsoleAlert dismissWithClickedButtonIndex:1 animated:YES];
	return NO;
}

- (void)SwipeUpAction:(id)Ignored
{
}

- (void)SwipeDownAction:(id)Ignored
{
}

- (void)SwipeLeftAction:(id)Ignored
{
	// Populate the text field with the previous entry in the history array
	if( self.ConsoleHistoryValues.count > 0 &&
		self.ConsoleHistoryValuesIndex + 1 < self.ConsoleHistoryValues.count )
	{
		self.ConsoleHistoryValuesIndex++;
		self.ConsoleTextField.text = [self.ConsoleHistoryValues objectAtIndex:self.ConsoleHistoryValuesIndex];
	}
}

- (void)SwipeRightAction:(id)Ignored
{
	// Populate the text field with the next entry in the history array
	if( self.ConsoleHistoryValues.count > 0 &&
		self.ConsoleHistoryValuesIndex > 0 )
	{
		self.ConsoleHistoryValuesIndex--;
		self.ConsoleTextField.text = [self.ConsoleHistoryValues objectAtIndex:self.ConsoleHistoryValuesIndex];
	}
}

#endif // !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE

/** TRUE if the iAd banner should be on the bottom of the screen */
BOOL bDrawOnBottom;

/** TRUE when the banner is onscreen */
BOOL bIsBannerVisible = NO;

/**
 * Will show an iAd on the top or bottom of screen, on top of the GL view (doesn't resize
 * the view)
 * 
 * @param bShowOnBottomOfScreen If true, the iAd will be shown at the bottom of the screen, top otherwise
 */
- (void)ShowAdBanner:(NSNumber*)bShowOnBottomOfScreen
{
	// close any existing ad
	[self HideAdBanner];

	NSString* SizeOfAdStr;

	// choose landscape or portrait size
#if __IPHONE_OS_VERSION_MIN_REQUIRED < 40200
	if (OSVersion < 4.2f)
	{
		SizeOfAdStr = bIPhonePortraitMode ? ADBannerContentSizeIdentifier320x50 : ADBannerContentSizeIdentifier480x32;
	}
	else
#endif
	{
		SizeOfAdStr = bIPhonePortraitMode ? ADBannerContentSizeIdentifierPortrait : ADBannerContentSizeIdentifierLandscape;
	}

	// convert size identifier into a usable size
	CGSize SizeOfAd = [ADBannerView sizeFromBannerContentSizeIdentifier:SizeOfAdStr];
	bDrawOnBottom = [bShowOnBottomOfScreen boolValue];
	// open it off screen, we will slide it on when it actually loads with something valid
	CGRect Frame = CGRectMake(0, bDrawOnBottom ? self.RootView.bounds.size.height : -SizeOfAd.height , SizeOfAd.width, SizeOfAd.height);

	self.BannerView = [[ADBannerView alloc] initWithFrame:Frame];
	self.BannerView.delegate = self;	
	NSMutableSet* IdentifierSet = [NSMutableSet set];
	[IdentifierSet addObject:SizeOfAdStr];
	self.BannerView.requiredContentSizeIdentifiers = IdentifierSet;
	[self.RootView addSubview:self.BannerView];
}

- (void)bannerViewDidLoadAd:(ADBannerView*)Banner
{
    if (!bIsBannerVisible)
    {
		// init a slide on animation
        [UIView beginAnimations:@"animateAdBannerOn" context:NULL];
		if (bDrawOnBottom)
		{
			Banner.frame = CGRectOffset(Banner.frame, 0, -Banner.frame.size.height);
		}
		else
		{
			Banner.frame = CGRectOffset(Banner.frame, 0, Banner.frame.size.height);
		}

		// slide on!
        [UIView commitAnimations];
		bIsBannerVisible = YES;
    }
}

- (void)bannerView:(ADBannerView *)Banner didFailToReceiveAdWithError:(NSError *)Error
{
	// if we get an error, hide the banner 
	if (bIsBannerVisible)
	{
		// init a slide off animation
		[UIView beginAnimations:@"animateAdBannerOff" context:NULL];
		if (bDrawOnBottom)
		{
			Banner.frame = CGRectOffset(Banner.frame, 0, Banner.frame.size.height);
		}
		else
		{
			Banner.frame = CGRectOffset(Banner.frame, 0, -Banner.frame.size.height);
		}
        [UIView commitAnimations];
        bIsBannerVisible = NO;
    }
}

/**
 * Callback when the user clicks on an ad
 */
- (BOOL)bannerViewActionShouldBegin:(ADBannerView*)Banner willLeaveApplication:(BOOL)bWillLeave
{
	// if we aren't about to swap out the app, tell the game to pause (or whatever)
	if (!bWillLeave)
	{
		IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
		AsyncTask.GameThreadCallback = ^ UBOOL (void)
		{
			// tell the ad manager the user clicked on the banner
			UPlatformInterfaceBase::GetInGameAdManagerSingleton()->OnUserClickedBanner();

			return TRUE;
		};
		[AsyncTask FinishedTask];
	}

	return YES;
}

/**
 * Callback when an ad is closed
 */
- (void)bannerViewActionDidFinish:(ADBannerView*)Banner
{
	IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
	AsyncTask.GameThreadCallback = ^ UBOOL (void)
	{
		// tell ad singleton we closed the ad
		UPlatformInterfaceBase::GetInGameAdManagerSingleton()->OnUserClosedAd();

		return TRUE;
	};
	[AsyncTask FinishedTask];
}

/**
 * Hides the iAd banner shows with ShowAdBanner. Will force close the ad if it's open
 */
- (void)HideAdBanner
{
	// make sure it's not open
	[self CloseAd];

	// must set delegate to nil before releasing the view
	self.BannerView.delegate = nil;
	[self.BannerView removeFromSuperview];
	self.BannerView = nil;

	bIsBannerVisible = NO;
}

/**
 * Forces closed any displayed ad. Can lead to loss of revenue
 */
- (void)CloseAd
{
	// boot user out of the ad
	[self.BannerView cancelBannerViewAction];
}

/** 
 * Application destructor
 */
- (void)dealloc 
{
	self.Window = nil;
	self.SecondaryWindow = nil;
	self.Controller = nil;
	self.GLView = nil;
	self.SecondaryGLView = nil;
	self.MusicPlayer = nil;
	self.FadeTimer = nil;
	self.UE3InitCheckTimer = nil;
	self.NextSongToPlay = nil;
	self.RootView = nil;

	[self HideAdBanner];

	[super dealloc];

}

/*** @todo ib2merge - Fix for ALAudio interruption START */
/** 
 * Handle ALAudio suspend and resume
 * http://developer.apple.com/library/ios/#documentation/Audio/Conceptual/AudioSessionProgrammingGuide/Cookbook/Cookbook.html#//apple_ref/doc/uid/TP40007875-CH6-SW7
 * Listing 7-17
 */
void interruptionListener(void *inClientData, UInt32 inInterruption) 
{
	//get the app delegate
	IPhoneAppDelegate* AppDelegate = (IPhoneAppDelegate*)[UIApplication sharedApplication].delegate;

	 if (inInterruption == kAudioSessionBeginInterruption) 
	 {
		// Activate the audio session
		NSError* setActiveCategoryError = nil;
		[[AVAudioSession sharedInstance] setActive: NO error: &setActiveCategoryError];
		if (setActiveCategoryError != nil)
		{
			NSLog(@"AudioSession::interruptionListener>  FAILED to set audio session disabled! %@", setActiveCategoryError);
			// kill the current player
			if (AppDelegate.MusicPlayer != nil)
			{
				[AppDelegate.MusicPlayer stop];
				AppDelegate.MusicPlayer = nil;
			}					
		}

// 		NSLog(@"Suspending OpenAL");
// 		UAudioDeviceIPhone::SuspendContext();
	 } 
	 else if (inInterruption == kAudioSessionEndInterruption) 
	 {
		if (!AppDelegate.bUsingBackgroundMusic)	  
		{
			// Activate the audio session
			NSError* setActiveCategoryError = nil;
			[[AVAudioSession sharedInstance] setActive: YES error: &setActiveCategoryError];
			if (setActiveCategoryError != nil)
			{
				NSLog(@"AudioSession::interruptionListener>  FAILED to set audio session active! %@", setActiveCategoryError);
				// kill the current player
				if (AppDelegate.MusicPlayer != nil)
				{
					[AppDelegate.MusicPlayer stop];
					AppDelegate.MusicPlayer = nil;
				}					
			}
			else
			{
				// Start our music player again
				if (AppDelegate.MusicPlayer != nil)
				{
					// set volume
					AppDelegate.MusicPlayer.volume = 1.0f * AppDelegate.VolumeMultiplier;
					[AppDelegate.MusicPlayer play];
				}
			}	 
		}
//		NSLog(@"Resuming OpenAL");
//		UAudioDeviceIPhone::ResumeContext();
	 } 
} 


// Better support for rate this app and other itunes link functionality 
// Apple sample code http://developer.apple.com/library/ios/#qa/qa2008/qa1629.html so we can avoid opening safari when we only want itunes
// Process a LinkShare/TradeDoubler/DGM URL to something iPhone can handle
- (void)openReferralURL:(NSURL *)referralURL 
{
	NSString* URLStr = [referralURL absoluteString];
	debugf(TEXT("openReferralURL called with %s"), UTF8_TO_TCHAR([URLStr cStringUsingEncoding:NSUTF8StringEncoding]));
    NSURLConnection *conn = [[NSURLConnection alloc] initWithRequest:[NSURLRequest requestWithURL:referralURL] delegate:self startImmediately:YES];
    [conn release];
}

// @todo ib2merge: What is all this for?
// Save the most recent URL in case multiple redirects occur
// "iTunesURL" is an NSURL property in your class declaration
- (NSURLRequest *)connection:(NSURLConnection *)connection willSendRequest:(NSURLRequest *)request redirectResponse:(NSURLResponse *)response 
{
	//check for redirect, our link for 
	if(response != nil && [response isKindOfClass:[NSHTTPURLResponse class]])
	{
		NSHTTPURLResponse* HTTPResponse = (NSHTTPURLResponse*)response;

		//Only set for HTTP
		//This is to make sure the game start ping doesn't actually open the webpage... 
		//(See //- (void)connection:(NSURLConnection*)Connection didReceiveResponse:(NSHTTPURLResponse*)Response above)

		NSInteger responseCode = [HTTPResponse statusCode];
		NSString* ResponseStr = [NSHTTPURLResponse localizedStringForStatusCode:responseCode];
		debugf(TEXT("HTTPResponse connection called with response (%s) %d"), UTF8_TO_TCHAR([ResponseStr cStringUsingEncoding:NSUTF8StringEncoding]), responseCode);
			
		//if this is a permanet move request, then follow it (prevents opening of Safari for itunes links)
		if([HTTPResponse statusCode] == 301)
		{
			self.iTunesURL = [request URL];
		}
		else
		{
			self.iTunesURL = [response URL];
		}
		
		NSString* URLStr = [self.iTunesURL absoluteString];
		debugf(TEXT("connection, new URL bound to iTunesURL %s"), UTF8_TO_TCHAR([URLStr cStringUsingEncoding:NSUTF8StringEncoding]));
	}
	else 
	{
		debugf(TEXT("Not a redirect, opening"));
		self.iTunesURL = [request URL];
	}
	
	if(request != nil)
	{
		NSString* RequestStr = [[request URL] absoluteString];
		debugf(TEXT("connection, request URL %s"), UTF8_TO_TCHAR([RequestStr cStringUsingEncoding:NSUTF8StringEncoding]));
	}
	else
	{
		debugf(TEXT("connection, request is NULL"));
	}
	
	return request;
}

/**
 * Called if the connection attempt fails for any reason (bad url, no internet connection, etc)
 */
- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
	FString ErrorMsg = LocalizeError(TEXT("IPhoneLaunchURLError"), TEXT("Engine"));
	UIAlertView *alert = [[UIAlertView alloc] initWithTitle:@"Connection Error" message:[NSString stringWithUTF8String:TCHAR_TO_UTF8(*ErrorMsg)] delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil];
	[alert show];
	[alert release];
}

// No more redirects; use the last URL saved
- (void)connectionDidFinishLoading:(NSURLConnection *)connection 
{
	if(self.iTunesURL != nil)
	{
		NSString* URLStr = [self.iTunesURL absoluteString];
		debugf(TEXT("connectionDidFinishLoading called, using URL %s"), UTF8_TO_TCHAR([URLStr cStringUsingEncoding:NSUTF8StringEncoding]));
		[[UIApplication sharedApplication] openURL:self.iTunesURL];
    }
}

- (void)ShowMessageController:(NSString*)InitialMessage
{
	MFMessageComposeViewController *SMScontroller = [[[MFMessageComposeViewController alloc] init] autorelease];
	if([MFMessageComposeViewController canSendText])
	{
		SMScontroller.body = InitialMessage;
		SMScontroller.recipients = [NSArray arrayWithObjects:nil];
		//messageComposeViewController delegate called when finished
		SMScontroller.messageComposeDelegate = Controller;
		[Controller presentModalViewController:SMScontroller animated:YES];
		//sometimes the system info bar that shows up when this controller is up looks bad, so just keep it hidden
		[[UIApplication sharedApplication] setStatusBarHidden:YES withAnimation:UIStatusBarAnimationNone];
	}
}

- (void)ShowMailController:(NSArray*)Params
{
	MFMailComposeViewController  *Mailcontroller = [[[MFMailComposeViewController  alloc] init] autorelease];
	if([MFMailComposeViewController canSendMail])
	{
		NSString* InitialSubjectObj = nil;
		NSString* InitialMessageObj = nil;

		// get the parameters
		if ([Params count] > 0)
			InitialSubjectObj = [Params objectAtIndex:0];
		if ([Params count] > 1)
			InitialMessageObj = [Params objectAtIndex:1];
	
		[Mailcontroller setSubject:InitialSubjectObj];
		[Mailcontroller setMessageBody:InitialMessageObj isHTML:NO];
		//mailComposeController delegate called when finished
		Mailcontroller.mailComposeDelegate = Controller;
		[Controller presentModalViewController:Mailcontroller animated:YES];
	}	
}


@end
