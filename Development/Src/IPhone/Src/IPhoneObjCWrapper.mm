/*=============================================================================
 IPhoneObjCWrapper.mm: iPhone wrapper for making ObjC calls from C++ code
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#include "Engine.h"

#include "IPhoneObjCWrapper.h"
#include "IPhoneAppDelegate.h"
#import "NSStringExtensions.h"
#include <unistd.h>

#import <Foundation/NSArray.h>
#import <Foundation/NSPathUtilities.h>
#import <mach/mach_host.h>
#import <mach/task.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if_dl.h>

#ifdef PF_MAX
#undef PF_MAX
#endif

#include "Engine.h"

#import <SystemConfiguration/SystemConfiguration.h>
#import <netinet/in.h>


#if USE_DETAILED_IPHONE_MEM_TRACKING
extern UINT IPhoneBackBufferMemSize;
#endif

/**
 * Get the path to the .app where file loading occurs
 * 
 * @param AppDir [out] Return path for the application directory that is the root of file loading
 * @param MaxLen Size of AppDir buffer
 */
void IPhoneGetApplicationDirectory( char *AppDir, int MaxLen ) 
{
	// use the API to retrieve where the application is stored
	NSString *dir = [NSSearchPathForDirectoriesInDomains(NSAllApplicationsDirectory, NSUserDomainMask, YES) objectAtIndex: 0];
	[dir getCString: AppDir maxLength: MaxLen encoding: NSASCIIStringEncoding];
}

/**
 * Get the path to the user document directory where file saving occurs
 * 
 * @param DocDir [out] Return path for the application directory that is the root of file saving
 * @param MaxLen Size of DocDir buffer
 */
void IPhoneGetDocumentDirectory( char *DocDir, int MaxLen ) 
{
	// use the API to retrieve where the application is stored
	NSString *dir = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex: 0];
	[dir getCString: DocDir maxLength: MaxLen encoding: NSASCIIStringEncoding];
}

/**
 * Creates a directory (must be in the Documents directory
 *
 * @param Directory Path to create
 * @param bMakeTree If true, it will create intermediate directory
 *
 * @return true if successful)
 */
bool IPhoneCreateDirectory(char* Directory, bool bMakeTree)
{
	NSAutoreleasePool *autoreleasepool = [[NSAutoreleasePool alloc] init];
	
	// convert to iPhone string
	NSFileManager* FileManager = [NSFileManager defaultManager];
	NSString* NSPath = [FileManager stringWithFileSystemRepresentation:Directory length:strlen(Directory)];

	// create the directory (with optional tree)
	BOOL Result = [FileManager createDirectoryAtPath:NSPath withIntermediateDirectories:bMakeTree attributes:nil error:nil];
	
	[autoreleasepool release];
	
	return Result;
}

/* These are defined in mach/shared_memory_server.h, which does not exist when
   compiling for device */
#ifndef SHARED_TEXT_REGION_SIZE
#define SHARED_TEXT_REGION_SIZE  0x08000000
#endif

#ifndef SHARED_DATA_REGION_SIZE
#define SHARED_DATA_REGION_SIZE 0x08000000
#endif

/**
 * Retrieve current memory information (for just this task)
 *
 * @param FreeMemory Amount of free memory in bytes
 * @param UsedMemory Amount of used memory in bytes
 */
void IPhoneGetTaskMemoryInfo(uint64_t& ResidentSize, uint64_t& VirtualSize)
{
	// Get stats about the current task
	task_basic_info Stats;
	mach_msg_type_number_t Size = TASK_BASIC_INFO_COUNT;

	task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&Stats, &Size);

	// Return the most interesting stuff
	ResidentSize = Stats.resident_size;
	VirtualSize = Stats.virtual_size - SHARED_TEXT_REGION_SIZE - SHARED_DATA_REGION_SIZE;
}


 
/**
 * Retrieve current memory information (for the entire device, not limited to our process)
 *
 * @param FreeMemory Amount of free memory in bytes
 * @param UsedMemory Amount of used memory in bytes
 */
void IPhoneGetPhysicalMemoryInfo( uint64_t & FreeMemory, uint64_t & UsedMemory )
{
	// get stats about the host (mem is in pages)
	vm_statistics Stats;
	mach_msg_type_number_t Size = sizeof(Stats);
	host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&Stats, &Size);
	
	// get the page size
	vm_size_t PageSize;
	host_page_size(mach_host_self(), &PageSize);
	
	// combine to get free memory!
	FreeMemory = Stats.free_count * PageSize;
	// and used memory
	UsedMemory = ( Stats.active_count + Stats.inactive_count + Stats.wire_count ) * PageSize;
}


/**
 * Enables or disables the view autorotation when the user rotates the view
 */
void IPhoneSetRotationEnabled(int bEnabled)
{
	[[IPhoneAppDelegate GetDelegate] SetRotationEnabled:(bEnabled ? YES : NO)];
}

/**
 * Launch a URL for the given Tag
 *
 * @param Tag String describing what URL to launch
 */
void IPhoneLaunchURL(const char* LaunchURL, UBOOL bProcessRedirectsLocally)
{
	Class NSURLClass = NSClassFromString(@"NSURL");

	// create the URL
	NSString* URLString = [NSString stringWithFormat:@"%s", LaunchURL];
	NSURL* URL = [NSURLClass URLWithString:URLString];

	// launch app store directly, or if we don't want to process redirects
	if ([URLString hasPrefix:@"itms"] || !bProcessRedirectsLocally)
	{
		// Launch the ITMS URL directly (these are Apple URLs that open directly into iTunes)
		[[UIApplication sharedApplication] openURL:URL];
	}
	else 
	{
		// Pass this URL to the referral function that will open the final page without showing intermediate redirects
		IPhoneAppDelegate* AppDelegate = (IPhoneAppDelegate*)[UIApplication sharedApplication].delegate;
		[AppDelegate performSelectorOnMainThread:@selector(openReferralURL:) withObject:URL waitUntilDone:NO];
	}
}

/**
 * Save a key/value string pair to the user's settings
 *
 * @param Key Name of the setting
 * @param Value Value to save to the setting
 */
void IPhoneSaveUserSetting(const char* Key, const char* Value)
{
	// get the user settings object
    NSUserDefaults *Defaults = [NSUserDefaults standardUserDefaults];

	// convert input strings to NSStrings
	NSString* KeyString = [NSString stringWithUTF8String:Key];
	NSString* ValueString = [NSString stringWithUTF8String:Value];

	// set the setting
	[Defaults setObject:ValueString forKey:KeyString];
	[Defaults synchronize];
}


/**
 * Load a value from the user's settings for the given Key
 *
 * @param Key Name of the setting
 * @param Value [out] String to put the loaded value into
 * @param MaxValueLen Size of the OutValue string
 */
void IPhoneLoadUserSetting(const char* Key, char* OutValue, int MaxValueLen)
{
	// get the user settings object
    NSUserDefaults *Defaults = [NSUserDefaults standardUserDefaults];

	// convert input string to NSStrings
	NSString* KeyString = [NSString stringWithUTF8String:Key];

	// load the value
	NSString* ValueString = [Defaults stringForKey:KeyString];

	// make sure we have a non-null string
	if (ValueString == nil)
	{
		ValueString = @"";
	}

	// convert it to a C string to pass back
	[ValueString getCString: OutValue maxLength: MaxValueLen encoding: NSASCIIStringEncoding];
}


/**
 * Convenience wrapper around IPhoneLoadUserSetting for integers
 * NOTE: strtoull returns 0 if it can't parse the int (this will be the default when we first load)
 * 
 * @param Name Name of the setting
 * @return Setting value as uint64_t
 */
uint64_t IPhoneLoadUserSettingU64(const char* Name)
{
	char Buf[32];
	IPhoneLoadUserSetting(Name, Buf, sizeof(Buf)-1); 
	return strtoull(Buf, NULL, 0);
}

/**
 * Convenience wrapper around IPhoneLoadUserSetting for integers
 * NOTE: strtoull returns 0 if it can't parse the int (this will be the default when we first load)
 * 
 * @param Name Name of the setting
 * @param Min Lower clamp value
 * @param Max Upper clamp value
 * @return Setting value as uint64_t
 */
uint64_t IPhoneLoadUserSettingU64Clamped(const char* Name, uint64_t Min, uint64_t Max)
{
	char Buf[32];
	IPhoneLoadUserSetting(Name, Buf, sizeof(Buf)-1); 
	return Clamp(strtoull(Buf, NULL, 0), Min, Max);
}

/**
 * Convenience wrapper around IPhoneSaveUserSetting for integers
 * 
 * @param Name The name of the setting
 * @param Value The new setting value
 */
void IPhoneSaveUserSettingU64(const char* Name, uint64_t Value)
{
	char Buf[32];
	snprintf(Buf, sizeof(Buf)-1, "%llu", Value);
	IPhoneSaveUserSetting(Name, Buf); 
}


/**
 * Convenience wrapper around IPhoneLoadUserSettingU64 and IPhoneSaveUserSettingU64
 *
 * @param Name The name of the setting
 */
uint64_t IPhoneIncrementUserSettingU64(const char* Name, uint64_t By)
{
	uint64_t Value = IPhoneLoadUserSettingU64(Name);
	IPhoneSaveUserSettingU64(Name, Value + By);
	return Value + By;
}

/**
 * Scales the volume for mp3s
 *
 * @param VolumeMultiplier Amount to scale the volume (0.0 - 1.0)
 */
void IPhoneScaleMusicVolume(const char* VolumeMultiplier)
{
	// route the update to the main iphone thread (may not be necessary, 
	// but it won't hitch the game thread at least)
	[[IPhoneAppDelegate GetDelegate] performSelectorOnMainThread:@selector(ScaleMusicVolume:) 
		withObject:[NSString stringWithUTF8String:VolumeMultiplier] waitUntilDone:NO];
}

/**
 * Plays an mp3 in hardware
 *
 * @param SongName Name of the mp3 to play, WITHOUT path or extension info
 */
void IPhonePlaySong(const char* SongName)
{
	// route the playback to the main iphone thread (may not be necessary, 
	// but it won't hitch the game thread at least)
	[[IPhoneAppDelegate GetDelegate] performSelectorOnMainThread:@selector(PlaySong:) 
		withObject:[NSString stringWithUTF8String:SongName] waitUntilDone:NO];
}

/**
 * Stops any current mp3 playing
 */
void IPhoneStopSong()
{
	// route the playback to the main iphone thread (may not be necessary, 
	// but it won't hitch the game thread at least)
	[[IPhoneAppDelegate GetDelegate] performSelectorOnMainThread:@selector(StopSong) 
		withObject:nil waitUntilDone:NO];
}

/**
 * Pauses the hardware mp3 stream
 */
void IPhonePauseSong()
{
	// route the playback to the main iphone thread (may not be necessary, 
	// but it won't hitch the game thread at least)
	[(IPhoneAppDelegate*)[UIApplication sharedApplication].delegate performSelectorOnMainThread:@selector(PauseSong) 
		withObject:nil waitUntilDone:NO];
}

/**
 * Returns the OS version of the device
 */
float IPhoneGetOSVersion()
{
	if ( GOverrideIOSVersion < 1.0f )
	{
		return [IPhoneAppDelegate GetDelegate].OSVersion;
	}
	else
	{
		return GOverrideIOSVersion;
	}
}

/**
 * Returns the SHA1 hashed UDID
 */
FString IPhoneGetSHA1HashedUDID()
{
	return FString(DataToString([[[UIDevice currentDevice] uniqueIdentifier] dataUsingEncoding:NSUTF8StringEncoding], YES));
}

/**
 * Returns the MD5 hashed UDID
 */
FString IPhoneGetMD5HashedUDID()
{
	extern FString MD5HashAnsiString( const TCHAR* String );

	return MD5HashAnsiString(*FString([[UIDevice currentDevice] uniqueIdentifier]));
}

/**
 * Converts an FString to an NSString
 */
NSString* ToNSString(const FString& InString)
{
	return [NSString stringWithUTF8String:TCHAR_TO_UTF8(*InString)];
}

/**
 * Returns the push notification token for the device + application
 */
void IPhoneGetDevicePushNotificationToken( char *DevicePushNotificationToken, int MaxLen )
{
	IPhoneLoadUserSetting("DevicePushNotificationToken", DevicePushNotificationToken, MaxLen);
}

/**
 * Resumes a paused song on the hardware mp3 stream
 */
void IPhoneResumeSong()
{
	// route the playback to the main iphone thread (may not be necessary, 
	// but it won't hitch the game thread at least)
	[(IPhoneAppDelegate*)[UIApplication sharedApplication].delegate performSelectorOnMainThread:@selector(ResumeSong) 
		withObject:nil waitUntilDone:NO];
}

/**
 * Resumes the previous song from the point in playback where it was paused before the current song started playing
 */
void IPhoneResumePreviousSong()
{
	// route the playback to the main iphone thread (may not be necessary, 
	// but it won't hitch the game thread at least)
	[(IPhoneAppDelegate*)[UIApplication sharedApplication].delegate performSelectorOnMainThread:@selector(ResumePreviousSong) 
		withObject:nil waitUntilDone:NO];
}

void IPhoneDisableSongLooping()
{
	// route the playback to the main iphone thread (may not be necessary, 
	// but it won't hitch the game thread at least)
	[(IPhoneAppDelegate*)[UIApplication sharedApplication].delegate performSelectorOnMainThread:@selector(DisableSongLooping) 
		withObject:nil waitUntilDone:NO];	
}

/**
 * @return How much to scale globally scale UI elements in the X direction 
 * (useful for hi res screens, iPhone4, etc)
 */
float IPhoneGetGlobalUIScaleX()
{
	return [IPhoneAppDelegate GetDelegate].GlobalViewScale;
}

/**
 * @return How much to scale globally scale UI elements in the Y direction 
 * (useful for hi res screens, iPhone4, etc)
 */
float IPhoneGetGlobalUIScaleY()
{
	return [IPhoneAppDelegate GetDelegate].GlobalViewScale;
}

/**
 * @return the type of device we are currently running on
 */
EIOSDevice IPhoneGetDeviceType()
{
	// default to unknown
	static EIOSDevice DeviceType = IOS_Unknown;

	if (GOverrideIOSDevice != IOS_Unknown)
	{
		return GOverrideIOSDevice;
	}

	// if we've already figured it out, return it
	if (DeviceType != IOS_Unknown)
	{
		return DeviceType;
	}

	// get the device hardware type string length
	size_t DeviceIDLen;
	sysctlbyname("hw.machine", NULL, &DeviceIDLen, NULL, 0);

	// get the device hardware type
	char* DeviceID = (char*)malloc(DeviceIDLen);
	sysctlbyname("hw.machine", DeviceID, &DeviceIDLen, NULL, 0);

	// convert to NSStringt
	NSString *DeviceIDString= [NSString stringWithCString:DeviceID encoding:NSUTF8StringEncoding];
	free(DeviceID);

	// iPods
	if ([DeviceIDString hasPrefix:@"iPod"])
	{
		// get major revision number
		int Major = [DeviceIDString characterAtIndex:4] - '0';
		
		// iPod3,1 is either the real 3rd gen, which has ES2.0, or an 8GB 2nd gen, but the .plist won't
		// allow for the 8GB 2nd gen, so we can ignore it
		if (Major == 3)
		{
			// iPod 3rd gen is the same innards as IPhone 3GS, so we reuse the identifier
			DeviceType = IOS_IPhone3GS;
		}
		// iPod4,1 is iPod Touch 4th gen
		else if (Major == 4)
		{
			DeviceType = IOS_IPodTouch4;
		}
		// iPod5 is iPod Touch 5th gen, anything higher will use 5th gen settings until released
		else if (Major >= 5)
		{
			DeviceType = IOS_IPodTouch5;
		}
	}
	// iPads
	else if ([DeviceIDString hasPrefix:@"iPad"])
	{
		// get major revision number
		int Major = [DeviceIDString characterAtIndex:4] - '0';
		int Minor = [DeviceIDString characterAtIndex:6] - '0';

		// iPad1,1 is first iPad
		if (Major == 1)
		{
			DeviceType = IOS_IPad;
		}
		// iPad2,[1|2|3] is iPad 2 (1 - wifi, 2 - gsm, 3 - cdma)
		else if (Major == 2)
		{
			// iPad2,5+ is the new iPadMini, anything higher will use these settings until released
			if (Minor >= 5)
			{
				DeviceType = IOS_IPadMini;
			}
			else
			{
				DeviceType = IOS_IPad2;
			}
		}
		// iPad3,[1|2|3] is iPad 3 and iPad3,4+ is iPad (4th generation)
		else if (Major == 3)
		{
			// iPad3,4+ is the new iPad, anything higher will use these settings until released
			if (Minor >= 4)
			{
				DeviceType = IOS_IPad4;
			}
			else
			{
				DeviceType = IOS_IPad3;
			}
		}
		// Default to highest settings currently available for any future device
		else if (Major >= 4)
		{
			DeviceType = IOS_IPad4;
		}
	}
	// iPhones
	else if ([DeviceIDString hasPrefix:@"iPhone"])
	{
		int Major = [DeviceIDString characterAtIndex:6] - '0';

		// iPhone2,1 is iPhone 3GS
		if (Major == 2)
		{
			DeviceType = IOS_IPhone3GS;
		}
		else if (Major == 3)
		{
			DeviceType = IOS_IPhone4;
		}
		else if (Major == 4)
		{
			DeviceType = IOS_IPhone4S;
		}
		else if (Major >= 5)
		{
			DeviceType = IOS_IPhone5;
		}
	}

	// if this is unknown at this point, we have a problem
	if (DeviceType == IOS_Unknown)
	{
		NSLog(@"THIS IS AN UNSUPPORTED DEVICE [%@]\n", DeviceIDString);
	}
	else
	{
		NSLog(@"This device is [%@], enum value %d\n", DeviceIDString, DeviceType);
	}

	return DeviceType;
}

/**
 * @return the device type as a string
 */
FString IPhoneGetDeviceTypeString( EIOSDevice DeviceType )
{
	switch(DeviceType)
	{
		case IOS_IPhone3GS:		return TEXT("iPhone3GS");
		case IOS_IPhone4:		return TEXT("iPhone4");
		case IOS_IPhone4S:		return TEXT("iPhone4S");
		case IOS_IPhone5:		return TEXT("iPhone5");
		case IOS_IPodTouch4:	return TEXT("iPodTouch4");
		case IOS_IPodTouch5:    return TEXT("iPodTouch5");
		case IOS_IPad:			return TEXT("iPad");
		case IOS_IPad2:			return TEXT("iPad2");
		case IOS_IPad3:			return TEXT("iPad3");
		case IOS_IPad4:			return TEXT("iPad4");
		case IOS_IPadMini:		return TEXT("iPadMini");
	}
	return TEXT("Unknown");
}

/**
 * @return the number of cores on this device
 */
int IPhoneGetNumCores()
{
	return [[NSProcessInfo processInfo] processorCount];
}

/**
 * @return the orientation of the UI on the device
 */
int IPhoneGetUIOrientation()
{
	return [[IPhoneAppDelegate GetDelegate].Controller interfaceOrientation];
}

/**
 * Displays a console interface on device
 */
void IPhoneShowConsole()
{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	// Route the command to the main iPhone thread (all UI must go to the main thread)
	[[IPhoneAppDelegate GetDelegate] performSelectorOnMainThread:@selector(ShowConsole) withObject:nil waitUntilDone:NO];
#endif
}

/**
 * Shows a keyboard and allows user to input text in device
 */
void IPhoneGetUserInput(const char* Title, const char* InitialString, const char* ExecResponseFunction, const char* CancelResponseFunction, const char* CharacterLimit)
{
	NSString* TitleStr = [NSString stringWithUTF8String:Title];
	NSString* InitValueStr = [NSString stringWithUTF8String:InitialString];
	NSString* ExecFuncStr = [NSString stringWithUTF8String:ExecResponseFunction];
	NSString* CancelFuncStr = [NSString stringWithUTF8String:CancelResponseFunction];
	NSString* CharLimitStr = [NSString stringWithUTF8String:CharacterLimit];

	NSMutableDictionary *info = [NSMutableDictionary dictionary];
	[info setObject:TitleStr forKey:@"Title"];
	[info setObject:InitValueStr forKey:@"InitValue"];
	[info setObject:ExecFuncStr forKey:@"ExecFunc"];
	[info setObject:CancelFuncStr forKey:@"CancelFunc"];
	[info setObject:CharLimitStr forKey:@"CharLimit"];

	// Route the command to the main iPhone thread (all UI must go to the main thread)
	[[IPhoneAppDelegate GetDelegate] performSelectorOnMainThread:@selector(GetUserInput:) withObject:info waitUntilDone:NO];
}

/**
 * Shows a keyboard and allows user to input text in device
 */
void IPhoneGetUserInputMulti(const char* Title, const char* InitValue, const char* ExecFunc)
{
	debugf(TEXT("No in IPhoneGetUserInputMulti"));

	NSString* TitleStr = [NSString stringWithUTF8String:Title];
	NSString* InitValueStr = [NSString stringWithUTF8String:InitValue];
	NSString* ExecFuncStr = [NSString stringWithUTF8String:ExecFunc];

	//NSDictionary *info = [NSDictionary dictionaryWithObjectsAndKeys: TitleStr, @"Title", InitValueStr, @"InitValue", ExecFuncStr, @"ExecFunc", nil];
	NSMutableDictionary *info = [NSMutableDictionary dictionary];
	[info setObject:TitleStr forKey:@"Title"];
	[info setObject:InitValueStr forKey:@"InitValue"];
	[info setObject:ExecFuncStr forKey:@"ExecFunc"];

	// Route the command to the main iPhone thread (all UI must go to the main thread)
	[[IPhoneAppDelegate GetDelegate] performSelectorOnMainThread:@selector(GetUserInputMulti:) withObject:info waitUntilDone:NO];
}


/**
 * Shows an alert, blocks until user selects a choice, and then returns the index of the 
 * choice the user has chosen
 *
 * @param Message The text to display
 * @param Button0 Label for Button0
 * @param Button1 Label for Button1 (or NULL for no Button1)
 * @param Button2 Label for Button2 (or NULL for no Button2)
 *
 * @return 0, 1 or 2 depending on which button was clicked
 */
int IPhoneShowBlockingAlert(const char* Title, const char* Message, const char* Button0, const char* Button1, const char* Button2)
{
	NSMutableArray* StringArray = [NSMutableArray arrayWithCapacity:5];
	// always add message and first button
	[StringArray addObject:[NSString stringWithUTF8String:Title]];
	[StringArray addObject:[NSString stringWithUTF8String:Message]];
	[StringArray addObject:[NSString stringWithUTF8String:Button0]];

	// add optional buttons
	if (Button1)
	{
		[StringArray addObject:[NSString stringWithUTF8String:Button1]];
	}
	if (Button2)
	{
		[StringArray addObject:[NSString stringWithUTF8String:Button2]];
	}

	IPhoneAppDelegate* AppDelegate = [IPhoneAppDelegate GetDelegate];
	AppDelegate.AlertResponse = -1;

	// route the playback to the main iphone thread (all UI we route to main thread)
	[AppDelegate performSelectorOnMainThread:@selector(ShowAlert:) withObject:StringArray waitUntilDone:NO];

	// block until a user has clicked a button
	while (AppDelegate.AlertResponse == -1)
	{
		// sleep 0.1 seconds
		usleep(100000.0f);
	}

	return AppDelegate.AlertResponse;
}

/**
 * Gets the language the user has selected
 *
 * @param Language String to receive the Language into
 * @param MaxLen Size of the Language string
 */
void IPhoneGetUserLanguage(char* Language, int MaxLen)
{
	// get the set of languages
	NSArray* Languages = [[NSUserDefaults standardUserDefaults] objectForKey:@"AppleLanguages"];

	// get the language the user would like (first element is selected)
	NSString* PreferredLanguage = [Languages objectAtIndex:0];

	// convert to C string to pass back
	[PreferredLanguage getCString:Language maxLength:MaxLen encoding:NSASCIIStringEncoding];
}

/**
 * Retrieves the string value for the given key in the application's bundle (ie Info.plist)
 *
 * @param Key The key to look up
 * @param Value A buffer to fill in with the value
 * @param MaxLen Size of the Value string
 */
bool IPhoneGetBundleStringValue(const char* Key, char* Value, int MaxLen)
{
	// look up the key in the main bundle
	id BundleValue = [[[NSBundle mainBundle] infoDictionary] objectForKey:[NSString stringWithUTF8String:Key]];

	// make sure it's a string object
	if (BundleValue && [BundleValue isKindOfClass:[NSString class]])
	{
		// if it is, convert it to a c string
		[BundleValue getCString:Value maxLength:MaxLen encoding:NSASCIIStringEncoding];
		
		return true;
	}

	return false;
}

/**
 * Will show an iAd on the top or bottom of screen, on top of the GL view (doesn't resize
 * the view)
 * 
 * @param bShowOnBottomOfScreen If true, the iAd will be shown at the bottom of the screen, top otherwise
 */
void IPhoneShowAdBanner(bool bShowOnBottomOfScreen)
{
	[[IPhoneAppDelegate GetDelegate] performSelectorOnMainThread:@selector(ShowAdBanner:) withObject:[NSNumber numberWithBool:bShowOnBottomOfScreen] waitUntilDone:NO];
}

/**
 * Hides the iAd banner shows with IPhoneShowAdBanner. Will force close the ad if it's open
 */
void IPhoneHideAdBanner()
{
	[[IPhoneAppDelegate GetDelegate] performSelectorOnMainThread:@selector(HideAdBanner) withObject:nil waitUntilDone:NO];
}

/**
 * Forces closed any displayed ad. Can lead to loss of revenue
 */
void IPhoneCloseAd()
{
	[[IPhoneAppDelegate GetDelegate] performSelectorOnMainThread:@selector(CloseAd) withObject:nil waitUntilDone:NO];
}

/**
 * @return true if the phone currently has a net connection
 */
bool IPhoneIsNetConnected()
{
	// create a 0'd address
	sockaddr_in NullAddr;
	appMemzero(&NullAddr, sizeof(NullAddr));
	NullAddr.sin_len = sizeof(NullAddr); 
	NullAddr.sin_family = AF_INET; 

	// create the reachability target for the null address
	SCNetworkReachabilityRef NullTarget = SCNetworkReachabilityCreateWithAddress(NULL, (sockaddr*)&NullAddr);

	SCNetworkReachabilityFlags Flags;
	bool bIsConnected = false;

	// check for reachability
	if (SCNetworkReachabilityGetFlags(NullTarget, &Flags))
	{
		// if reachability could be determined, check the flags for reachability
		bIsConnected = (Flags & kSCNetworkReachabilityFlagsReachable) != 0;
	}

	// release the target
	CFRelease(NullTarget);

	// return connection status
	return bIsConnected;
}

/**
 * Enables or disables the sleep functionality of the device
 */
void IPhoneSetSleepEnabled(bool bIsSleepEnabled)
{
#if APPLE_BATTERY_TEST_BUILD
	bIsSleepEnabled = FALSE;	// Never sleep when testing batteries
#endif

	debugf(TEXT("Setting IdleTimerDisabled to %d"), (bIsSleepEnabled ? 0 : 1));
	[[UIApplication sharedApplication] setIdleTimerDisabled:bIsSleepEnabled ? NO : YES];
}

/**
 * Reads the mac address for the device
 *
 * @return the MAC address as a string (or empty if MAC address could not be read).
 * Note: on iOS, this will return en0:{MAC}, but since it's simply a string representation
 * we're after, this isn't considered an issue. If the actual bytes of the MAC address are 
 * required, the sockaddr_dl structure should be parsed directly, something like this:
 *     BYTE* MacAddress = (BYTE*)LLADDR(LinkLevelSockaddr);
 *     INT MacAddressLength = LinkLevelSockaddr->sdl_alen; // should be 6 bytes
 *     for (int Ndx=0;Ndx<MacAddressLength;++Ndx) // <-- use MacAddress[Ndx] in the loop
 */
FString IPhoneGetMacAddress()
{
	FString Result;

	ifaddrs* Addrs = NULL;
	// get the list of network interfaces on the system.
	if (getifaddrs(&Addrs) == 0)
	{
		ifaddrs* CurInterface = Addrs;
		do 
		{
			if (appStrcmpANSI(CurInterface->ifa_name, "en0") == 0)
			{
				if (CurInterface->ifa_addr->sa_family == AF_LINK)
				{
					Result = FString(link_ntoa((sockaddr_dl*)CurInterface->ifa_addr));
					break;
				}
			}
		} while ((CurInterface = CurInterface->ifa_next));
		// done iterating. Free the memory.
		freeifaddrs(Addrs);
	}
	else
	{
		debugf(TEXT("Failed to call getifaddrs. errno = %d"), errno);
	}
	if (Result.Len() == 0)
	{
		debugf(TEXT("Failed to find en0 AF_LINK interface with getifaddrs"));
	}
	return Result;
}

/**
 * Returns the MD5 hashed Mac address (Mac address is in no particular notation. Don't rely on this matching
 * any one else's hashed MAC address).
 */
FString IPhoneGetMD5HashedMacAddress()
{
	extern FString MD5HashAnsiString(const TCHAR*);

	return MD5HashAnsiString(*IPhoneGetMacAddress());
}



/**
 * Reads the IP address for the device
 *
 * @return the IP address as a string xxx.xxx.xxx.xxx (or empty if MAC address could not be read).
 */
FString IPhoneGetIPAddress()
{
	FString Result;

	ifaddrs* Addrs = NULL;
	// get the list of network interfaces on the system.
	if (getifaddrs(&Addrs) == 0)
	{
		ifaddrs* CurInterface = Addrs;
		do 
		{
			if (appStrcmpANSI(CurInterface->ifa_name, "en0") == 0)
			{
				if (CurInterface->ifa_addr->sa_family == AF_INET)
				{
					Result = FString(inet_ntoa(((sockaddr_in*)CurInterface->ifa_addr)->sin_addr));
				}
			}
		} while ((CurInterface = CurInterface->ifa_next));
		// done iterating. Free the memory.
		freeifaddrs(Addrs);
	}
	else
	{
		debugf(TEXT("Failed to call getifaddrs. errno = %d"), errno);
	}
	if (Result.Len() == 0)
	{
		debugf(TEXT("Failed to find en0 AF_INET interface with getifaddrs"));
	}
	return Result;
}

/**
 * Returns whether the build is packaged as Distribution (ie, shipping build)
 */
bool IPhoneIsPackagedForDistribution()
{
	NSString* PackagingMode = [[[NSBundle mainBundle] infoDictionary] objectForKey: @"EpicPackagingMode"];
	return PackagingMode != nil && [PackagingMode isEqualToString:@"Distribution"];
}

/**
*Returns the size of the back buffer alloced through OpenGL
*/
#if USE_DETAILED_IPHONE_MEM_TRACKING
UINT GetIPhoneOpenGLBackBufferSize()
{
	return IPhoneBackBufferMemSize;
}
#endif

