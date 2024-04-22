/*=============================================================================
	IPhoneFacebook.cpp: IPhone specific Facebook integration.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "EnginePlatformInterfaceClasses.h"
#include "IPhoneObjCWrapper.h"
#import "FBConnect.h"
#import "IPhoneAppDelegate.h"
#import "IPhoneAsyncTask.h"




@interface FacebookDelegate : NSObject <FBSessionDelegate, FBRequestDelegate, FBDialogDelegate>
{
	/** Facebook API object */
	Facebook* FB;

	/** Cached username */
	NSString* Username;
	
	/** Cached user id */
	NSString* UserId;
}


@property(retain) Facebook* FB;
@property(retain) NSString* Username;
@property(retain) NSString* UserId;



/** Initializer */
-(void) SetAppID:(NSString*)AppID;

/** Destructor */
-(void) dealloc;

/** Have FB verify with user they want to use FB */
-(BOOL) Authorize:(NSArray*)Permissions;

/** Perform actions when user is authorized (after loading auth token, or performing online authorization */
-(void) OnAuthorized;

/** Perform a generic FB graph API request ("me/friends") */
-(void) GenericRequest:(NSString*)Request;

/** Perform a generic FB dialog ("feed") */
-(void) ShowDialog:(NSArray*)Params;

/**
 * Tell the game thread authorization is completed, and we have a username
 */
-(void) TellGameThreadAuthorizationResult:(BOOL)bSucceeded;

@end

@implementation FacebookDelegate

@synthesize FB;
@synthesize Username;
@synthesize UserId;

/** Initializer */
-(void) SetAppID:(NSString*)AppID
{
	// startup the FB API
	self.FB = [[Facebook alloc] initWithAppId:AppID];

	// load the last auth token/timeout and put them right into facebook
	FB.accessToken = [[NSUserDefaults standardUserDefaults] stringForKey:@"FBToken"];
	FB.expirationDate = (NSDate*)[[NSUserDefaults standardUserDefaults] objectForKey:@"FBExpiration"];

	NSLog(@"SetAppID to %@ / %@ / %@!\n", AppID, FB.accessToken, FB.expirationDate);

	// if the access token is still valid, then go and grab the username
	if (FB.isSessionValid)
	{
		[self OnAuthorized];
	}
}

-(void) dealloc
{
	[super dealloc];

	self.FB = nil;
	self.Username = nil;
	self.UserId = nil;
}

/**
 * Have FB verify with user they want to use FB 
 */
-(BOOL) Authorize:(NSArray*)Permissions
{
	// are we already authorized with a valid auth token?
	if (FB.isSessionValid)
	{
		// if we haven't gotten a username yet, then get it now
		if (Username == nil)
		{
			[self OnAuthorized];
		}
		// if we have a username, then we are done; tell the game thread!
		else
		{
			[self TellGameThreadAuthorizationResult:YES];
		}
		return YES;
	}

	NSLog(@"Authorizing!\n");

	// authorize this app (a callback will be called later)
	[FB authorize:Permissions delegate:self];

	return YES;
}

/**
 * "Log out" from Facebook, user will see the auth web page again on next authorization
 */
-(void) Logout
{
	[[NSUserDefaults standardUserDefaults] setObject:nil forKey:@"FBToken"];

	// tell Facebook to "log out", this will call fbDidLogout
	[FB logout:self];
}

/** Perform actions when user is authorized (after loading auth token, or performing online authorization */
-(void) OnAuthorized;
{
	NSLog(@"OnAuthorized");
	// get my info
	[FB requestWithGraphPath:@"me" andDelegate:self];
}

/** Perform a generic FB graph API request ("me/friends") */
-(void) GenericRequest:(NSString*)Request
{
	[FB requestWithGraphPath:Request andDelegate:self];
}

/** Perform a generic FB dialog ("feed") */
-(void) ShowDialog:(NSArray*)Params
{
	// get the info from the game thread
	NSString* Action = [Params objectAtIndex:0];
	NSMutableDictionary* DialogParams = [Params objectAtIndex:1];

	// show the dialog
	[FB dialog:Action andParams:DialogParams andDelegate:self];
}

/**
 * Tell the game thread authorization is completed, and we have a username
 */
-(void) TellGameThreadAuthorizationResult:(BOOL)bSucceeded
{
	// make an async task to talk to game thread
	IPhoneAsyncTask* Task = [[IPhoneAsyncTask alloc] init];
	Task.GameThreadCallback = ^ UBOOL (void)
	{
		if (bSucceeded)
		{
			// set the username in the facebook if it's there
			UPlatformInterfaceBase::GetFacebookIntegrationSingleton()->UserName = FString(Username);
			UPlatformInterfaceBase::GetFacebookIntegrationSingleton()->UserId = FString(UserId);
			UPlatformInterfaceBase::GetFacebookIntegrationSingleton()->AccessToken = FString(FB.accessToken);
		}
		else
		{
			// reset on failure
			UPlatformInterfaceBase::GetFacebookIntegrationSingleton()->UserName = FString(TEXT(""));
			UPlatformInterfaceBase::GetFacebookIntegrationSingleton()->UserId = FString(TEXT(""));
			UPlatformInterfaceBase::GetFacebookIntegrationSingleton()->AccessToken = FString(TEXT(""));
		}

		// tell the game we finished, success is determined by Results being nil or not
		FPlatformInterfaceDelegateResult Result;
		Result.bSuccessful = bSucceeded ? TRUE : FALSE;
		UPlatformInterfaceBase::GetFacebookIntegrationSingleton()->CallDelegates(FID_AuthorizationComplete, Result);

		// now, kick off a friends list request
		if (bSucceeded)
		{
			UPlatformInterfaceBase::GetFacebookIntegrationSingleton()->FacebookRequest(TEXT("me/friends"));
		}

		return TRUE;
	};

	[Task FinishedTask];
}

/********************************
 * FBSessionDelegate functions
 *******************************/

/**
 * Called when the user successfully logged in.
 */
- (void)fbDidLogin
{
	NSLog(@"fbDidLogin!\n");

	// save off our now good auth token/expiration
	[[NSUserDefaults standardUserDefaults] setObject:FB.accessToken forKey:@"FBToken"];
	[[NSUserDefaults standardUserDefaults] setObject:FB.expirationDate forKey:@"FBExpiration"];

	// and now handle logged in action
	[self OnAuthorized];
}

/**
 * Called when the user dismissed the dialog without logging in.
 */
- (void)fbDidNotLogin:(BOOL)cancelled
{
	// tell game thread we failed
	[self TellGameThreadAuthorizationResult:NO];

	// clear any saved info
	[[NSUserDefaults standardUserDefaults] setObject:nil forKey:@"FBToken"];
}

/**
 * Called when the user logged out.
 */
- (void)fbDidLogout
{
	// clear any saved info
	[[NSUserDefaults standardUserDefaults] setObject:nil forKey:@"FBToken"];
}


/********************************
 * FBDialogDelegate functions
 *******************************/

/**
 * Called whenever a dialog completion callback is received
 */
- (void)OnDialogComplete:(BOOL)bSucceeded ResultString:(NSString*)ResultString
{
	IPhoneAsyncTask* Task = [[IPhoneAsyncTask alloc] init];
	Task.GameThreadCallback = ^ UBOOL (void)
	{
		// tell the game we finished, success is determined by Results being nil or not
		FPlatformInterfaceDelegateResult Result;
		Result.bSuccessful = bSucceeded;
		Result.Data.Type = PIDT_String;
		Result.Data.StringValue = ResultString ? FString(ResultString) : TEXT("");
		UPlatformInterfaceBase::GetFacebookIntegrationSingleton()->CallDelegates(FID_DialogComplete, Result);

		return TRUE;
	};

	[Task FinishedTask];
}

/**
 * Called when the dialog succeeds and is about to be dismissed.
 */
- (void)dialogDidComplete:(FBDialog *)dialog
{
	[self OnDialogComplete:YES ResultString:nil];
}

/**
 * Called when the dialog succeeds with a returning url.
 */
- (void)dialogCompleteWithUrl:(NSURL *)url
{
	[self OnDialogComplete:YES ResultString:url.path];
}

/**
 * Called when the dialog get canceled by the user.
 */
- (void)dialogDidNotCompleteWithUrl:(NSURL *)url
{
	[self OnDialogComplete:NO ResultString:url.path];
}

/**
 * Called when the dialog is cancelled and is about to be dismissed.
 */
- (void)dialogDidNotComplete:(FBDialog *)dialog
{
	[self OnDialogComplete:NO ResultString:nil];
}

/**
 * Called when dialog failed to load due to an error.
 */
- (void)dialog:(FBDialog*)dialog didFailWithError:(NSError *)error
{
	[self OnDialogComplete:NO ResultString:error.localizedDescription];
}

/**
 * Asks if a link touched by a user should be opened in an external browser.
 *
 * If a user touches a link, the default behavior is to open the link in the Safari browser,
 * which will cause your app to quit.  You may want to prevent this from happening, open the link
 * in your own internal browser, or perhaps warn the user that they are about to leave your app.
 * If so, implement this method on your delegate and return NO.  If you warn the user, you
 * should hold onto the URL and once you have received their acknowledgement open the URL yourself
 * using [[UIApplication sharedApplication] openURL:].
 */
- (BOOL)dialog:(FBDialog*)dialog shouldOpenURLInExternalBrowser:(NSURL *)url
{
	return YES;
}






/********************************
 * FBRequestDelegate functions
 *******************************/


/**
 * Called when an error prevents the request from completing successfully.
 */
- (void)request:(FBRequest *)request didFailWithError:(NSError *)error
{
	debugf(TEXT("FBRequest failed with error %s %s"), *FString([error description]), *FString(request.url));

	// tell game thread we failed if the me request failed
	if ([[request.url substringFromIndex:[request.url length] - 2] isEqualToString:@"me"])
	{
		// make sure to logout if the /me request fails. This could mean we lost FB App permissions
		[self Logout];

		[self TellGameThreadAuthorizationResult:NO];
	}

	if ([[request.url substringFromIndex:[request.url length] - 10] isEqualToString:@"me/friends"])
	{
		IPhoneAsyncTask* Task = [[IPhoneAsyncTask alloc] init];
		Task.GameThreadCallback = ^ UBOOL (void)
		{
			FPlatformInterfaceDelegateResult Result;
			Result.bSuccessful = FALSE;
			UPlatformInterfaceBase::GetFacebookIntegrationSingleton()->CallDelegates(FID_FriendsListComplete, Result);
			
			return TRUE;
		};
		[Task FinishedTask];
	}
}

/**
 * Called when a request returns a response.
 *
 * The result object is the raw response from the server of type NSData
 */
- (void)request:(FBRequest *)request didLoadRawResponse:(NSData*)data
{
	// is this a special request?
	BOOL bIsMeRequest = [[request.url substringFromIndex:[request.url length] - 2] isEqualToString:@"me"];
	BOOL bIsFriendListRequest = [[request.url substringFromIndex:[request.url length] - 10] isEqualToString:@"me/friends"];
	if (bIsMeRequest || bIsFriendListRequest)
	{
		// do nothing for this case, handle it in didLoad, below
		return;
	}

	// get the Json string representation as a string
	NSString* JsonString = [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease];

	// make an async task to talk to game thread
	IPhoneAsyncTask* Task = [[IPhoneAsyncTask alloc] init];
	Task.GameThreadCallback = ^ UBOOL (void)
	{
		// tell the game we finished
		FPlatformInterfaceDelegateResult Result;
		Result.bSuccessful = TRUE;
		Result.Data.Type = PIDT_String;
		Result.Data.StringValue = FString(JsonString);
		UPlatformInterfaceBase::GetFacebookIntegrationSingleton()->CallDelegates(FID_FacebookRequestComplete, Result);

		return TRUE;
	};
	[Task FinishedTask];
}

/**
 * Called when a request returns and its response has been parsed into
 * an object.
 *
 * The resulting object may be a dictionary, an array, a string, or a number,
 * depending on thee format of the API response.
 */
- (void)request:(FBRequest *)request didLoad:(id)result
{
	// handle the me request specially
	if ([[request.url substringFromIndex:[request.url length] - 2] isEqualToString:@"me"])
	{
		// cache the username,id
		self.Username = [result objectForKey:@"name"];
		self.UserId = [result objectForKey:@"id"];

		// tell game thread we failed
		[self TellGameThreadAuthorizationResult:YES];
	}

	if ([[request.url substringFromIndex:[request.url length] - 10] isEqualToString:@"me/friends"])
	{
		// cache the friends list
		NSArray* Friends = [result objectForKey:@"data"];

		IPhoneAsyncTask* Task = [[IPhoneAsyncTask alloc] init];
		Task.GameThreadCallback = ^ UBOOL (void)
		{
			// loop over each friend
			UPlatformInterfaceBase::GetFacebookIntegrationSingleton()->FriendsList.Empty();
			UPlatformInterfaceBase::GetFacebookIntegrationSingleton()->FriendsList.AddZeroed([Friends count]);
			
			INT Index = 0;
			for (NSDictionary* F in Friends)
			{
				FFacebookFriend& Friend = UPlatformInterfaceBase::GetFacebookIntegrationSingleton()->FriendsList(Index++);
				NSString* Value = [F objectForKey:@"name"];
				Friend.Name = FString(Value);
				Value = [F objectForKey:@"id"];
				Friend.Id = FString(Value);
			}

			// tell the game we finished
			FPlatformInterfaceDelegateResult Result;
			Result.bSuccessful = TRUE;
			UPlatformInterfaceBase::GetFacebookIntegrationSingleton()->CallDelegates(FID_FriendsListComplete, Result);

			return TRUE;
		};
		[Task FinishedTask];
	}
}


@end




class UFacebookIPhone: public UFacebookIntegration
{
	DECLARE_CLASS_INTRINSIC(UFacebookIPhone, UFacebookIntegration, 0, IPhoneDrv)

	virtual UBOOL Init()
	{
		if (FBDelegate)
		{
			[FBDelegate dealloc];
		}

		NSLog(@"Initialize!\n");

		// crank up facebook!
		FBDelegate = [[FacebookDelegate alloc] init];

		NSLog(@"FBDelegate: %@!\n", FBDelegate);

		// start it up on main thread (for the run loop, and any UI it may show)
		// @todo: This blocks to make sure the FB object has been created properly before we let game thread use it, this is fine as long
		// as we don't create this UFacebookIPhone object during gameplay
		[FBDelegate performSelectorOnMainThread:@selector(SetAppID:) 
			withObject:[NSString stringWithCString:TCHAR_TO_UTF8(*AppID) encoding:NSUTF8StringEncoding]
			waitUntilDone:YES];

		NSLog(@"FBDelegate.FB: %@!\n", FBDelegate.FB);

		return TRUE;
	}
	
	virtual UBOOL Authorize()
	{
		// startup facebook if we haven't already been started
		if (!FBDelegate)
		{
			debugf(TEXT("No FBDelegate object. Check your AppID in [FacebookIntegration] in your Engine.ini"));
			return FALSE;
		}

		NSLog(@"Authorize, already authorized? %d\n", IsAuthorized());

		// get the desired permissions from the .ini
		TArray<FString> IniPermissions;
		GConfig->GetArray(TEXT("Engine.FacebookIntegration"), TEXT("Permissions"), IniPermissions, GEngineIni);
		debugf(TEXT("Getting Permissions for FacebookIntegration:%d"), IniPermissions.Num());

		// build up an NSArray of strings from the .ini file
		NSMutableArray* Permissions = nil;
		if (IniPermissions.Num() > 0)
		{
			Permissions = [NSMutableArray arrayWithCapacity:IniPermissions.Num()];
			for (INT PermIndex = 0; PermIndex < IniPermissions.Num(); PermIndex++)
			{
				debugf(TEXT("%s"), *IniPermissions(PermIndex));
				NSString* PermString = [NSString stringWithCString:TCHAR_TO_UTF8(*IniPermissions(PermIndex)) encoding:NSUTF8StringEncoding];
				[Permissions addObject:PermString];
			}
		}

		// authorize on main thread, with requested permissions
		[FBDelegate performSelectorOnMainThread:@selector(Authorize:) withObject:Permissions waitUntilDone:NO];

		return TRUE;
	}

	virtual UBOOL IsAuthorized()
	{
		return FBDelegate && FBDelegate.FB && FBDelegate.FB.isSessionValid && FBDelegate.Username != nil;
	}

	virtual void Disconnect()
	{
		// toss our authorization info
		[FBDelegate Logout];
	}

	void FacebookRequest(const FString& GraphRequest)
	{
		// send the request off on main thread
		[FBDelegate performSelectorOnMainThread:@selector(GenericRequest:) 
			withObject:[NSString stringWithCString:TCHAR_TO_UTF8(*GraphRequest) encoding:NSUTF8StringEncoding]
			waitUntilDone:NO];
	}

	void FacebookDialog(const FString& Action, const TArray<FString>& ParamKeysAndValues)
	{
		// make sure it's a set of pairs, chop off an odd number of them
		INT NumKeysValues = ParamKeysAndValues.Num();
		if (NumKeysValues & 1)
		{
			NumKeysValues -= 1;
		}

		// create a dictionary to hold the parameters
		NSMutableDictionary* Params = [NSMutableDictionary dictionaryWithCapacity:NumKeysValues / 2];
		for (INT ParamIndex = 0; ParamIndex < NumKeysValues; ParamIndex += 2)
		{
			[Params setObject:[NSString stringWithCString:TCHAR_TO_UTF8(*ParamKeysAndValues(ParamIndex + 1)) encoding:NSUTF8StringEncoding]
					   forKey:[NSString stringWithCString:TCHAR_TO_UTF8(*ParamKeysAndValues(ParamIndex + 0)) encoding:NSUTF8StringEncoding]];
		}
		
		// we can only pass one param to the function, so make a temp array with 2 objects
		NSArray* FuncParams = [NSArray arrayWithObjects:[NSString stringWithCString:TCHAR_TO_UTF8(*Action) encoding:NSUTF8StringEncoding], Params, nil];
		// send the request off on main thread
		[FBDelegate performSelectorOnMainThread:@selector(ShowDialog:) withObject:FuncParams waitUntilDone:NO];
	}



	BOOL HandleOpenURL(NSURL* URL)
	{
		if (!FBDelegate)
		{
			debugf(TEXT("No FBDelegate object. Check your AppID in [FacebookIntegration] in your Engine.ini"));
		}

		// pass along the handleOpenURL call to the FB object
		return [FBDelegate.FB handleOpenURL:URL];
	}


protected:

	/** The ObjC object that can use the FB API */
	FacebookDelegate* FBDelegate;
};




/************************************************
 * IPhoneAppDelegate extension
 ***********************************************/

// by extending IPhoneAppDelegate, we can add the handleOpenURL callback function
// without needing to dirty up the AppDelegate code that would just call in to
// this file anyway
@interface IPhoneAppDelegate (FacebookExt)

- (BOOL)application:(UIApplication *)application handleOpenURL:(NSURL *)url;
- (BOOL)application:(UIApplication *)application openURL:(NSURL *)url
    sourceApplication:(NSString *)sourceApplication annotation:(id)annotation;

@end

@implementation IPhoneAppDelegate (FacebookExt)

/**
 * Facebook delegate function that will be called to show the app site (Pre 4.2 support)
 */
- (BOOL)application:(UIApplication *)application handleOpenURL:(NSURL *)url 
{
	NSLog(@"handleOpenURL %@!\n", url);
	// we only respond to facebook 
	return CastChecked<UFacebookIPhone>(UPlatformInterfaceBase::GetFacebookIntegrationSingleton())->HandleOpenURL(url); 
}

/**
 * Facebook delegate function that will be called to show the app site (4.2+ support)
 */
- (BOOL)application:(UIApplication *)application openURL:(NSURL *)url
    sourceApplication:(NSString *)sourceApplication annotation:(id)annotation
{
	NSLog(@"openURL %@!\n", url);
	return CastChecked<UFacebookIPhone>(UPlatformInterfaceBase::GetFacebookIntegrationSingleton())->HandleOpenURL(url); 
}

@end




IMPLEMENT_CLASS(UFacebookIPhone);

void AutoInitializeRegistrantsIPhoneFacebook( INT& Lookup )
{
	UFacebookIPhone::StaticClass();
}

