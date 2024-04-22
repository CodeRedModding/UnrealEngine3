/*=============================================================================
	TwitterIntegrationIPhone.cpp: IPhone specific Twitter support
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "EnginePlatformInterfaceClasses.h"
#import "IPhoneAppDelegate.h"
#import "IPhoneAsyncTask.h"
#import <MessageUI/MessageUI.h>


/**
 * StoreKit subclass of generic MicroTrans API
 */
class UInAppMessageIPhone : public UInAppMessageBase
{
	DECLARE_CLASS_INTRINSIC(UInAppMessageIPhone, UInAppMessageBase, 0, IPhoneDrv)

	// UInAppMessageBase interface
	/**
	 * Perform any needed initialization
	 */
	void Init()
	{
		NSLog(@"UInAppMessageIPhone INIT!");
	}
	
	UBOOL ShowInAppSMSUI(const FString& InitialMessage)	
	{
		//should always work as we require iOS 4 for UE3
		if([MFMessageComposeViewController canSendText]) 
		{
			NSString* InitialMessageObj = nil;
			// does the caller want to specify a message?
			if (InitialMessage != TEXT(""))
			{
				InitialMessageObj = [NSString stringWithCString:TCHAR_TO_UTF8(*InitialMessage) encoding:NSUTF8StringEncoding];
			}
	
			//do this on the main thread				
			[[IPhoneAppDelegate GetDelegate] performSelectorOnMainThread:@selector(ShowMessageController:) withObject:InitialMessageObj waitUntilDone:NO];
			return TRUE;
		}	

		return FALSE;		
	}

	UBOOL ShowInAppEmailUI(const FString& InitialSubject, const FString& InitialMessage)
	{
		//should always work as we require iOS 4 for UE3
		if([MFMailComposeViewController canSendMail]) 
		{
			NSString* InitialSubjectObj = nil;
			NSString* InitialMessageObj = nil;

			// does the caller want to specify a subject?
			if (InitialSubject != TEXT(""))
			{
				InitialSubjectObj = [NSString stringWithCString:TCHAR_TO_UTF8(*InitialSubject) encoding:NSUTF8StringEncoding];
			}
			
			// does the caller want to specify a message?
			if (InitialMessage != TEXT(""))
			{
				InitialMessageObj = [NSString stringWithCString:TCHAR_TO_UTF8(*InitialMessage) encoding:NSUTF8StringEncoding];
			}
			
			//note this isn't the safest way to do things as if subject is nil we won't get the message either...  should be fine for our use though
			NSArray* Params = [NSArray arrayWithObjects:InitialSubjectObj, InitialMessageObj, nil];	
			//do this on the main thread				
			[[IPhoneAppDelegate GetDelegate] performSelectorOnMainThread:@selector(ShowMailController:) withObject:Params waitUntilDone:NO];
			return TRUE;
		}	
			
		return FALSE;
	}
};

IMPLEMENT_CLASS(UInAppMessageIPhone);

void AutoInitializeRegistrantsInAppMessageIPhone( INT& Lookup )
{
	UInAppMessageIPhone::StaticClass();
}

