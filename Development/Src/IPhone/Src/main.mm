/*=============================================================================
	main.mm: IPhone main function
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/


#import <UIKit/UIKit.h>
#include "Core.h"
#include "IPhoneObjCWrapper.h"
#include <stdio.h>

// save these for phone-home
uint64_t GStartupFreeMem;
uint64_t GStartupUsedMem;

/**
 * Attempt to add a key (and maybe value) to the commandline
 *
 * @param Key Settings key to check
 * @param UserDefaults The user settings, Key can be looked up in it
 * @param Commandline The commandline to modify
 * @param bInsertSpace If TRUE, a space will be inserted before the option is added to the commandline
 */
void UseKeyValue(NSString* Key, NSUserDefaults* UserDefaults, NSString*& Commandline, UBOOL bInsertSpace)
{
	// get the value of the key
	NSString* Value = [UserDefaults stringForKey:Key];

	// handle -key= case
	UBOOL bUseKey = FALSE;
	if ([Key characterAtIndex:[Key length] - 1] == '=')
	{
		// use -key=value if the value is specified
		if (Value && [Value length] > 0)
		{
			bUseKey = TRUE;
			// append the value to the key
			Key = [Key stringByAppendingString:Value];
		}
	}
	// handle -key case
	else
	{
		// use -key if value is valid and non-zero
		if (Value && [Value length] > 0 && [Value compare:@"0"] != NSOrderedSame)
		{
			bUseKey = TRUE;
		}
	}

	if (bUseKey)
	{
		// modify the commandline
		if (bInsertSpace)
		{
			Commandline = [Commandline stringByAppendingFormat:@" %@", Key];
		}
		else
		{
			Commandline = [Commandline stringByAppendingString:Key];
		}
	}
}


int main(int argc, char *argv[]) 
{
	// get the physical memeory into asap and save it
	IPhoneGetPhysicalMemoryInfo( GStartupFreeMem, GStartupUsedMem );
	
	uint64_t StartupResident;
	uint64_t StartupVirtual;
	IPhoneGetTaskMemoryInfo(StartupResident, StartupVirtual);
	appOutputDebugStringf(TEXT("Physical memory at the beginning of main() = %d MB free, %d MB used") LINE_TERMINATOR, (int)(GStartupFreeMem >> 20), (int)(GStartupUsedMem >> 20) );
	appOutputDebugStringf(TEXT("Task memory at the beginning of main() = %d MB resident, %d MB virtual") LINE_TERMINATOR, (int)(StartupResident >> 20), (int)(StartupVirtual >> 20) );

	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];


	// get the app identifier
	NSBundle* MainBundle = [NSBundle mainBundle];
	NSString* AppIdentifier = [MainBundle bundleIdentifier];

	// get the user settings for this app
	NSUserDefaults* UserDefaults = [NSUserDefaults standardUserDefaults];
	NSDictionary* Settings = [UserDefaults persistentDomainForName:AppIdentifier];

	// initialze the URL (?) options and launch (-) options to the general use
	// option fields with special names
	NSString* URLExtras = [UserDefaults stringForKey:@"URLOptions"];
	if (URLExtras == nil)
	{
		URLExtras = @"";
	}

	NSString* LaunchExtras = [UserDefaults stringForKey:@"LaunchOptions"];
	if (LaunchExtras == nil)
	{
		LaunchExtras = @"";
	}
	else
	{
		LaunchExtras = [@" " stringByAppendingString:LaunchExtras];
	}

	// programmatically parse the user settings for command line modification,
	// looking for - or ? options
	NSEnumerator* Keys = [Settings keyEnumerator];
	NSString* Key;
	while ((Key = [Keys nextObject]))
	{
		// there are a few Key cases to handle here:
		//   * -somekey					[put this on the commandline if Value != "0" && Value != ""]
		//   * -somekey=				[put -somekey=Value on the commandline if Value != ""]

		// is this key a launch (-fps) option?
		if ([Key characterAtIndex:0] == '-')
		{
			// modify the command line
			UseKeyValue(Key, UserDefaults, LaunchExtras, TRUE);
		}
		// is this key a URL (?fast=1) option?
		else if ([Key characterAtIndex:0] == '?')
		{
			// modify the command line
			UseKeyValue(Key, UserDefaults, URLExtras, FALSE);
		}
	}

	// put the ? and - options together
	NSString* FullSettingsCommandline = [URLExtras stringByAppendingString:LaunchExtras];

	// initialize the UE3 iphone stuff while we still have argc and argv
	appIPhoneInit(argc, argv, [FullSettingsCommandline cStringUsingEncoding:NSUTF8StringEncoding]);

	int retVal = UIApplicationMain(argc, argv, nil, @"IPhoneAppDelegate");
    [pool release];
    return retVal;
}
