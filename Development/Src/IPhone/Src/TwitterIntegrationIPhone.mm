/*=============================================================================
	TwitterIntegrationIPhone.cpp: IPhone specific Twitter support
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "EnginePlatformInterfaceClasses.h"
#import "IPhoneAppDelegate.h"
#import "IPhoneAsyncTask.h"

#if WITH_IOS_5

#import <Twitter/Twitter.h>
#import <Accounts/Accounts.h>

@interface FTwitterSupport : NSObject
{
	/** Account storage singleton */
	ACAccountStore* _AccountStore;

	/** List of accounts that can be used */
	NSArray* _Accounts;
}

@property(retain) ACAccountStore* AccountStore;
@property(retain) NSArray* Accounts;

/**
 * Twitter account status changed
 */
- (void)AccountStatusChanged:(NSNotification*)Notification;


/**
 * Show the twitter view on the iOS thread
 */
- (void)ShowTweetController:(NSArray*)Params;

/**
 * Check for authorized accounts
 */
- (void)GetAccounts;

@end


@implementation FTwitterSupport

@synthesize AccountStore=_AccountStore;
@synthesize Accounts=_Accounts;

/**
 * Notification called when a cloud key/value has changed
 */
- (void)AccountStatusChanged:(NSNotification*)Notification
{
//	GCanTweet = [TWTweetComposeViewController canSendTweet] ? TRUE : FALSE;
//	NSLog(@"Can tweet in callback? %d\n", [TWTweetComposeViewController canSendTweet]);
}

/**
 * Show the twitter view on the iOS thread
 */
- (void)ShowTweetController:(NSArray*)Params
{
	NSString* InitialMessageObj = nil;
	NSURL* URLObj = nil;
	UIImage* ImageObj = nil;

	// get the parameters
	if ([Params count] > 0)
	{
		InitialMessageObj = [Params objectAtIndex:0];
	}
	if ([Params count] > 1)
	{
		URLObj = [Params objectAtIndex:1];
	}
	if ([Params count] > 2)
	{
		ImageObj = [Params objectAtIndex:2];
	}

	TWTweetComposeViewController* TweetController = [[TWTweetComposeViewController alloc] init];

	// set up controller
	if (ImageObj)
	{
		[TweetController addImage:ImageObj];
	}
	if (URLObj)
	{
		[TweetController addURL:URLObj];
	}	
	if (InitialMessageObj)
	{
		[TweetController setInitialText:InitialMessageObj];
	}

	// set block for handling user completion
	[TweetController setCompletionHandler:^(TWTweetComposeViewControllerResult TweetResult) 
	{
		IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
		AsyncTask.GameThreadCallback = ^ UBOOL (void)
		{
			if (!UPlatformInterfaceBase::GetCloudStorageInterfaceSingleton()->bSuppressDelegateCalls)
			{
				FPlatformInterfaceDelegateResult Result(EC_EventParm);
				Result.bSuccessful = TweetResult == TWTweetComposeViewControllerResultDone;
				UPlatformInterfaceBase::GetTwitterIntegrationSingleton()->CallDelegates(TID_TweetUIComplete, Result);
			}

			return TRUE;
		};
		[AsyncTask FinishedTask];

		// hide the view
		[[IPhoneAppDelegate GetDelegate].Controller dismissModalViewControllerAnimated:YES];
	}];
    
	// show the controller
	[[IPhoneAppDelegate GetDelegate].Controller presentModalViewController:TweetController animated:YES];
}

/**
 * Check for authorized accounts
 */
- (void)GetAccounts
{
	// create the account storage if needed
	if (self.AccountStore == nil)
	{
		self.AccountStore = [[ACAccountStore alloc] init];
	}
    
	// get Twitter account type
	ACAccountType *AccountType = [self.AccountStore accountTypeWithAccountTypeIdentifier:ACAccountTypeIdentifierTwitter];
    
	// request access to the twitter accounts (this may show a dialog asking for permission)
	[self.AccountStore requestAccessToAccountsWithType:AccountType withCompletionHandler:^(BOOL bGranted, NSError *Error) 
	{
		@synchronized(self)
		{
			if (bGranted)
			{
				// get list of twitter accounts
				self.Accounts = [self.AccountStore accountsWithAccountType:AccountType];
			}
			else
			{
				// make an empty array so game thread knows we have tried
				self.Accounts = [NSArray arrayWithObjects:nil];
			}
		}

		IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
		AsyncTask.GameThreadCallback = ^ UBOOL (void)
		{
			FPlatformInterfaceDelegateResult Result(EC_EventParm);
			Result.bSuccessful = bGranted;
			UPlatformInterfaceBase::GetTwitterIntegrationSingleton()->CallDelegates(TID_AuthorizeComplete, Result);

			return TRUE;
		};
		[AsyncTask FinishedTask];
	}];


}


@end

/**
 * StoreKit subclass of generic MicroTrans API
 */
class UTwitterIntegrationIPhone : public UTwitterIntegrationBase
{
	DECLARE_CLASS_INTRINSIC(UTwitterIntegrationIPhone, UTwitterIntegrationBase, 0, IPhoneDrv)

	/**
	 * Perform any needed initialization
	 */
	void Init()
	{
		// cloud support object (this will leak, that's okay)
		TwitterSupport = [[FTwitterSupport alloc] init];

		// listen for changes
		[[NSNotificationCenter defaultCenter] addObserver:TwitterSupport
												 selector:@selector(AccountStatusChanged:)
												 	name:ACAccountStoreDidChangeNotification
												  object:nil];
		
//		TwitterAccount = nil;
	}

	/**
	 * Starts the process of authorizing the local user
	 */
	UBOOL AuthorizeAccounts()
	{
		// authorize on main thread
		[TwitterSupport performSelectorOnMainThread:@selector(GetAccounts) withObject:nil waitUntilDone:NO];

		return TRUE;
	}

	/**
	 * @return true if the user is allowed to use the Tweet UI
	 */
	UBOOL CanShowTweetUI()
	{
		// if this returns YES, then the user is okay to make posts and do other 
		return [TWTweetComposeViewController canSendTweet] ? TRUE : FALSE;
	}

	/**
	 * @return The number of accounts that were authorized
	 */
	INT GetNumAccounts()
	{
		@synchronized(TwitterSupport)
		{
			return [TwitterSupport.Accounts count];
		}
	}

	/**
	 * @return the display name of the given Twitter account
	 */
	FString GetAccountName(INT AccountIndex)
	{
		if (AccountIndex >= GetNumAccounts())
		{
			return TEXT("");
		}
		
		@synchronized(TwitterSupport)
		{
			ACAccount* Account = [TwitterSupport.Accounts objectAtIndex:AccountIndex];
			return FString(Account.username);
		}
	}

	/**
	 * @return the display name of the given Twitter account
	 */
	FString GetAccountId(INT AccountIndex)
	{
		if (AccountIndex >= GetNumAccounts())
		{
			return TEXT("");
		}

		@synchronized(TwitterSupport)
		{
			ACAccount* Account = [TwitterSupport.Accounts objectAtIndex:AccountIndex];
			NSDictionary* PropertiesDict = [Account dictionaryWithValuesForKeys:[NSArray arrayWithObject:@"properties"]];
			NSString* UserID = [[PropertiesDict objectForKey:@"properties"] objectForKey:@"user_id"];
			return UserID != nil ? FString(UserID) : TEXT("");
		}
	}


	/**
	 * Kicks off a tweet, using the platform to show the UI. If this returns FALSE, or you are on a platform that doesn't support the UI,
	 * you can use the TwitterRequest method to perform a manual tweet using the Twitter API
	 *
	 * @param InitialMessage [optional] Initial message to show
	 * @param URL [optional] URL to attach to the tweet
	 * @param Picture [optional] Name of a picture (stored locally, platform subclass will do the searching for it) to add to the tweet
	 */
	UBOOL ShowTweetUI(const FString& InitialMessage, const FString& URL, const FString& Picture)
	{
		if (!CanShowTweetUI())
		{
			return FALSE;
		}

	
		NSString* InitialMessageObj = nil;
		NSURL* URLObj = nil;
		UIImage* ImageObj = nil;
		
		// does the caller want to specify a message?
		if (InitialMessage != TEXT(""))
		{
			InitialMessageObj = [NSString stringWithCString:TCHAR_TO_UTF8(*InitialMessage) encoding:NSUTF8StringEncoding];
		}

		// does the caller want to attach a URL?
		if (URL != TEXT(""))
		{
			URLObj = [NSURL URLWithString:[NSString stringWithCString:TCHAR_TO_UTF8(*URL) encoding:NSUTF8StringEncoding]];
		}

		// does the caller want to attach a picture?
		if (Picture != TEXT(""))
		{
			// look for the picture in the app directory (from the MyGame/Build/IPhone/Resources/Graphics directory)
			ImageObj = [UIImage imageNamed:[NSString stringWithCString:TCHAR_TO_UTF8(*Picture) encoding:NSUTF8StringEncoding]];
		}

		NSArray* Params = [NSArray arrayWithObjects:InitialMessageObj, URLObj, ImageObj, nil];
		[TwitterSupport performSelectorOnMainThread:@selector(ShowTweetController:) withObject:Params waitUntilDone:NO];

		return TRUE;
	}

	/**
	 * Kicks off a generic twitter request
	 *
	 * @param URL The URL for the twitter request
	 * @param KeysAndValues The extra parameters to pass to the request (request specific). Separate keys and values: < "key1", "value1", "key2", "value2" >
	 * @param Method The method for this request (get, post, delete)
	 * @param AccountIndex A user index if an account is needed, or -1 if an account isn't needed for the request
	 *
	 * @return TRUE the request was sent off, and a TID_RequestComplete
	 */
	UBOOL TwitterRequest(const FString& URL, const TArray<FString>& ParamKeysAndValues, BYTE RequestMethod, INT AccountIndex)
	{
/*
		NSString* URLObj = [NSString stringWithCString:TCHAR_TO_UTF8(*InitialMessage) encoding:NSUTF8StringEncoding];
		NSMutableDictionary* ParamsObj = nil;
		NSNumber* MethodObj = [NSNumber numberWithInt:RequestMethod];
		NSNumber* AccountIndexObj = [NSNumber numberWithInt:AccountIndex];
		
		// make sure even number of keys/values
		INT NumKeysValues = ParamKeysAndValues.Num();
		if (NumKeysValues & 1)
		{
			NumKeysValues -= 1;
		}

		// fill out a dictionary if we have any key/value pairs
		if (NumKeysValues)
		{
			// create a dictionary to hold the parameters
			NSMutableDictionary* ParamsObj = [NSMutableDictionary dictionaryWithCapacity:NumKeysValues / 2];
			for (INT ParamIndex = 0; ParamIndex < NumKeysValues; ParamIndex += 2)
			{
				[ParamsObj setObject:[NSString stringWithCString:TCHAR_TO_UTF8(*ParamKeysAndValues(ParamIndex + 1)) encoding:NSUTF8StringEncoding]
							  forKey:[NSString stringWithCString:TCHAR_TO_UTF8(*ParamKeysAndValues(ParamIndex + 0)) encoding:NSUTF8StringEncoding]];
			}
		}

		// call the function on the main thread (it might show UI)
		NSArray* FuncParams = [NSArray arrayWithObjects:URLObj, ParamsObj, MethodObj, AccountIndexObj, nil];
		[TwitterSupport performSelectorOnMainThread:@selector(TwitterRequest:) withObject:Params waitUntilDone:NO];
*/

		// make sure even number of keys/values
		NSMutableDictionary* ParamsObj = nil;
		INT NumKeysValues = ParamKeysAndValues.Num();
		if (NumKeysValues & 1)
		{
			NumKeysValues -= 1;
		}

		// fill out a dictionary if we have any key/value pairs
		if (NumKeysValues)
		{
			// create a dictionary to hold the parameters
			ParamsObj = [NSMutableDictionary dictionaryWithCapacity:NumKeysValues / 2];
			for (INT ParamIndex = 0; ParamIndex < NumKeysValues; ParamIndex += 2)
			{
				[ParamsObj setObject:[NSString stringWithCString:TCHAR_TO_UTF8(*ParamKeysAndValues(ParamIndex + 1)) encoding:NSUTF8StringEncoding]
							  forKey:[NSString stringWithCString:TCHAR_TO_UTF8(*ParamKeysAndValues(ParamIndex + 0)) encoding:NSUTF8StringEncoding]];
			}
		}

		// get the account if needed
		ACAccount* Account = nil;
		if (AccountIndex != -1)
		{
			// validate
			if (AccountIndex >= GetNumAccounts())
			{
				return FALSE;
			}

			// get the account
			@synchronized(TwitterSupport)
			{
				Account = [TwitterSupport.Accounts objectAtIndex:AccountIndex];
			}
		}

		// make URL object
		NSURL* URLObj = [NSURL URLWithString:[NSString stringWithCString:TCHAR_TO_UTF8(*URL) encoding:NSUTF8StringEncoding]];

		// get method

#if defined(__IPHONE_6_0)
		TWRequestMethod Method = RequestMethod == TRM_Get ? SLRequestMethodGET : RequestMethod == TRM_Post ? SLRequestMethodPOST : SLRequestMethodDELETE;
#else
		TWRequestMethod Method = RequestMethod == TRM_Get ? TWRequestMethodGET : RequestMethod == TRM_Post ? TWRequestMethodPOST : TWRequestMethodDELETE;
#endif

		// finally make the request
		TWRequest *Request = [[TWRequest alloc] initWithURL:URLObj parameters:ParamsObj requestMethod:Method];
		[Request setAccount:Account];
		[Request performRequestWithHandler:^(NSData *ResponseData, NSHTTPURLResponse *URLResponse, NSError *Error) 
		{
			IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
			AsyncTask.GameThreadCallback = ^ UBOOL (void)
			{
				FPlatformInterfaceDelegateResult Result(EC_EventParm);
				Result.bSuccessful = Error == nil;
				Result.Data.Type = PIDT_Custom;
				Result.Data.IntValue = [URLResponse statusCode];

				// get the Json string representation as a string
				NSString* ResultString = [[NSString alloc] initWithData:ResponseData encoding:NSUTF8StringEncoding];
				Result.Data.StringValue = FString(ResultString);
				[ResultString release];

				UPlatformInterfaceBase::GetTwitterIntegrationSingleton()->CallDelegates(TID_RequestComplete, Result);

				return TRUE;
			};
			[AsyncTask FinishedTask];
		}];
		
		return TRUE;
	}

private:

	/** [retained] Twitter helper object */
	FTwitterSupport* TwitterSupport;
};



IMPLEMENT_CLASS(UTwitterIntegrationIPhone);

#endif

void HACK_CanTweet()
{
// #if WITH_IOS_5
// //	GCanTweet = [TWTweetComposeViewController canSendTweet];
// 	NSLog(@"Can tweet? %d %d\n", GCanTweet, [TWTweetComposeViewController canSendTweet]);
// #endif
}

void AutoInitializeRegistrantsIPhoneTwitterIntegration( INT& Lookup )
{
#if WITH_IOS_5

	if (NSClassFromString(@"TWRequest"))
	{
		UTwitterIntegrationIPhone::StaticClass();
	}
#endif
}

