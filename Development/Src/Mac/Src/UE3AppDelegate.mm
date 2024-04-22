/*=============================================================================
	UE3AppDelegate.mm: Mac application class and main loop.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#import "UE3AppDelegate.h"
#include "MacObjCWrapper.h"

/**
 * Takes the path of the .app resources, .app itself, and the path of the binary within the .app, from the Obj-C launcher
 *
 * @param ResourcePath		The absolute path of the .app resources
 * @param AppPathAndName	The absolute path of the .app
 * @param BinaryPathAndName	The absolute path of the binary within the .app
 * @param bResourcesBasePath	Whether or not the game content resides within ResourcePath
 */
extern void appMacSaveAppPaths(const char* ResourcesPath, const char* AppPathAndName, const char* BinaryPathAndName, bool bResourcesBasePath);

extern int appMacCallGuardedMain();
extern uint32_t GIsStarted;
extern int GToggleFullscreen;
extern int GAppActiveChanged;
extern void appExit();

@implementation UE3AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	// Disabled unnecessary chdir code (messing with the current working directory here this early on, interferes with internal UE3 code)
#if 0
	NSFileManager* FM = [NSFileManager defaultManager];
	NSString* GameName = [[[NSBundle mainBundle] executablePath] lastPathComponent];
	NSString* GameDirName = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:GameName];
	NSString* GameCookedDirName = [GameDirName stringByAppendingPathComponent:@"CookedMac"];
	
	// Useful for debugging/verifying the game name and cooked directory are set up correctly
	//NSRunInformationalAlertPanel(@"GameName", GameName, @"OK", nil, nil);
	//NSRunInformationalAlertPanel(@"GameDirName", GameDirName, @"OK", nil, nil);
	//NSRunInformationalAlertPanel(@"GameCookedDirName", GameCookedDirName, @"OK", nil, nil);

	if ([FM fileExistsAtPath:GameCookedDirName])
	{
		// Only check if game is installed if it's self-contained app
		[self CheckInstallationFolder];
		chdir([FM fileSystemRepresentationWithPath:[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"Engine/Config"]]);
	}
	else
	{
		NSString *path = [[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent];
		if ([[path lastPathComponent] isEqual:@"Mac"])
		{
			chdir([FM fileSystemRepresentationWithPath:[path stringByAppendingPathComponent:@"../../Engine/Config"]]);
		}
		else
		{
			chdir([FM fileSystemRepresentationWithPath:[path stringByAppendingPathComponent:@"Engine/Config"]]);
		}
	}
#endif

	NSString* ResourcesPath = [[NSBundle mainBundle] resourcePath];
	NSString* AppPathAndName = [[NSBundle mainBundle] bundlePath];
	NSString* BinaryPathAndName = [[NSBundle mainBundle] executablePath];
	const char* ANSIResourcesPath = [ResourcesPath cStringUsingEncoding:NSASCIIStringEncoding];
	const char* ANSIAppPathAndName = [AppPathAndName cStringUsingEncoding:NSASCIIStringEncoding];
	const char* ANSIBinaryPathAndName = [BinaryPathAndName cStringUsingEncoding:NSASCIIStringEncoding];
	bool bResourcesBasePath = 0;

	// Determine whether or not the game content is within ResourcesPath
	NSFileManager* FM = [NSFileManager defaultManager];
	NSString* GameName = [[[NSBundle mainBundle] executablePath] lastPathComponent];
	NSString* GameDirName = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:GameName];
	NSString* GameCookedDirName = [GameDirName stringByAppendingPathComponent:@"CookedMac"];

	if ([FM fileExistsAtPath:GameCookedDirName])
	{
		// Only check if game is installed if it's self-contained app
		[self CheckInstallationFolder];

		bResourcesBasePath = 1;
	}

	appMacSaveAppPaths(ANSIResourcesPath, ANSIAppPathAndName, ANSIBinaryPathAndName, bResourcesBasePath);

	// We will be using pthreads, nevertheless Cocoa needs to know it must run in
	// multithreaded mode. To do this we simply create Cocoa thread that will exit immediately.
	[NSThread detachNewThreadSelector:@selector(DummyThreadRoutine:) toTarget:[UE3AppDelegate class] withObject:nil];

	GIsStarted = 1;

	appMacCallGuardedMain();

	appExit();

	GIsStarted = 0;
	
	exit(0);
}

+ (void)DummyThreadRoutine:(id)AnObject
{
}

- (void)applicationDidBecomeActive:(NSNotification *)Notification
{
	GAppActiveChanged = 1;
}

- (void)applicationDidResignActive:(NSNotification *)Notification
{
	GAppActiveChanged = 2;
}

- (IBAction)toggleFullScreen:(id)Sender
{
	GToggleFullscreen = 1;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)Sender
{
	MacAppRequestExit( 0 );
	return NSTerminateCancel;
}

- (BOOL)ShowEULA:(const char *)RTFPath
{
	[EULAView readRTFDFromFile:[NSString stringWithUTF8String:RTFPath]];
	NSInteger ReturnValue = [NSApp runModalForWindow:EULAWindow];
	[EULAWindow close];
	return (BOOL)ReturnValue;
}

- (IBAction)OnEULAAccepted:(id)Sender
{
	[NSApp stopModalWithCode:1];
}

- (IBAction)OnEULADeclined:(id)Sender
{
	_Exit(0);
}

- (void)CheckInstallationFolder
{
	if (![[[NSBundle mainBundle] bundlePath] hasPrefix:@"/Applications"])
	{
		NSFileManager *FileManager = [NSFileManager defaultManager];
		NSString *BundlePath = [[NSBundle mainBundle] bundlePath];

		// Get the timestamp for the bundle
		double BundleTimestamp = -1.0;
		NSDictionary *FileAttributes = [FileManager attributesOfItemAtPath:BundlePath error:nil];
		if (FileAttributes != nil)
		{
			NSDate *FileModificationDate = [FileAttributes fileModificationDate];
			BundleTimestamp = [FileModificationDate timeIntervalSinceReferenceDate];
		}

		// Check whether we've asked already for this bundle by looking at the timestamp
		NSUserDefaults *UserDefaults = [NSUserDefaults standardUserDefaults];
		if (BundleTimestamp > 0.0 &&
			[UserDefaults doubleForKey:@"BundleTimestampWhenAsked"] == BundleTimestamp)
		{
			// Already warned user about the game being not installed, but user ignored it. We don't ask again.
			return;
		}

		[UserDefaults setDouble:BundleTimestamp forKey:@"BundleTimestampWhenAsked"];
		[UserDefaults synchronize];

		NSString *BundleName = [BundlePath lastPathComponent];
		NSString *DestPath = [NSString stringWithFormat:@"/Applications/%@", BundleName];

		BOOL bShouldMoveToApplications = (NSRunAlertPanel([NSString stringWithFormat:@"Would you like to move \"%@\" into the Applications folder before launching?", [BundleName stringByDeletingPathExtension]], @"", @"Yes", @"No", nil) == NSAlertDefaultReturn);
		if (bShouldMoveToApplications)
		{
			Authorization = NULL;

			if ([self MoveAppBundle:BundlePath ToAppsFolder:DestPath] == TRUE)
			{
				// Relaunch the app from new location
				LSLaunchURLSpec LaunchSpec;
				LaunchSpec.appURL = (CFURLRef)[NSURL fileURLWithPath:DestPath];
				LaunchSpec.itemURLs = NULL;
				LaunchSpec.passThruParams = NULL;
				LaunchSpec.launchFlags = kLSLaunchDefaults | kLSLaunchNewInstance;
				LaunchSpec.asyncRefCon = NULL;
				LSOpenFromURLSpec(&LaunchSpec, NULL);
				exit(0);
			}
		}
	}
}

- (BOOL)MoveAppBundle:(NSString *)BundlePath ToAppsFolder:(NSString *)DestPath
{
	NSFileManager *FM = [NSFileManager defaultManager];

	if ([FM fileExistsAtPath:DestPath])
	{
		BOOL bShouldReplace = (NSRunAlertPanel(@"Warning",
				[NSString stringWithFormat:@"\"%@\" already exists in Applications folder. Replace it?", [[BundlePath lastPathComponent] stringByDeletingPathExtension]],
				@"Yes", @"No", nil) == NSAlertDefaultReturn);
		if (bShouldReplace)
		{
			NSError *Error;
			if ([FM removeItemAtPath:DestPath error:&Error] == FALSE)
			{
				if ([Error code] == NSFileWriteNoPermissionError && [self AuthorizeExtendedPrivileges])
				{
					char Arg0[] = "-c";
					char Arg1[2048];
					sprintf(Arg1, "rm -r \"%s\"", [DestPath fileSystemRepresentation]);
					char *Arguments[3] = {Arg0, Arg1, NULL};
					FILE *Pipe;
					OSStatus Status = AuthorizationExecuteWithPrivileges(Authorization, "/bin/sh", kAuthorizationFlagDefaults, Arguments, &Pipe);
					if (Status == errAuthorizationSuccess)
					{
						size_t LineLength = 0;
						fgetln(Pipe, &LineLength); // this makes us wait till the operation is finished
					}
					else
					{
						return FALSE;
					}
				}
			}
		}
		else
		{
			return FALSE;
		}
	}

	NSError *Error;
	BOOL bResult;
	BOOL bRemoveOriginal = [FM isDeletableFileAtPath:BundlePath];
	if (bRemoveOriginal)
	{
		bResult = [FM moveItemAtPath:BundlePath toPath:DestPath error:&Error];
	}
	else
	{
		bResult = [FM copyItemAtPath:BundlePath toPath:DestPath error:&Error];
	}

	if (bResult == FALSE)
	{
		NSInteger ErrorCode = [Error code];
		if (ErrorCode == NSFileWriteNoPermissionError)
		{
			if ([self AuthorizeExtendedPrivileges])
			{
				char Arg0[] = "-c";
				char Arg1[2048];
				sprintf(Arg1, "%s \"%s\" \"%s\"; echo Done", bRemoveOriginal ? "mv" : "cp -R", [BundlePath fileSystemRepresentation], [DestPath fileSystemRepresentation]);
				char *Arguments[] = {Arg0, Arg1, NULL};
				FILE *Pipe;
				OSStatus Status = AuthorizationExecuteWithPrivileges(Authorization, "/bin/sh", kAuthorizationFlagDefaults, Arguments, &Pipe);
				if (Status == errAuthorizationSuccess)
				{
					size_t LineLength = 0;
					fgetln(Pipe, &LineLength); // this makes us wait till the operation is finished
				}
				else
				{
					return FALSE;
				}
			}
		}
		else
		{
			return FALSE;
		}
	}

	return TRUE;
}

- (BOOL)AuthorizeExtendedPrivileges
{
	if (!Authorization)
	{
		OSStatus Status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &Authorization);
		if (Status == errAuthorizationSuccess)
		{
			AuthorizationFlags Flags = kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed
				| kAuthorizationFlagPreAuthorize | kAuthorizationFlagExtendRights;
			AuthorizationItem Items[] = {{kAuthorizationRightExecute, strlen("/bin/sh"), (void *)"/bin/sh", 0}};
			AuthorizationRights Rights = {sizeof(Items) / sizeof(AuthorizationItem), Items};
			Status = AuthorizationCopyRights(Authorization, &Rights, NULL, Flags, NULL);
			return (Status == errAuthorizationSuccess);
		}
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

@end
