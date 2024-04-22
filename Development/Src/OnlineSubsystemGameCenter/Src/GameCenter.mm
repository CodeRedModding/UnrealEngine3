/*=============================================================================
	EAGLView.mm: IPhone window wrapper for a GL view
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/


#include "OnlineSubsystemGameCenter.h"
#include "IPhoneAppDelegate.h"
#include "IPhoneInput.h"
#import "GameCenter.h"
#import "IPhoneObjCWrapper.h"
#import <GameKit/GKScore.h>
#import <GameKit/GKMatchmaker.h>
#import <GameKit/GKVoiceChat.h>

#if WITH_UE3_NETWORKING

/** Game Center delegate object */
FGameCenter* GGameCenter = NULL;

/**
 * Check to see if GameCenter is available (so we can run on pre-4.0 devices)
 */
bool IPhoneDynamicCheckForGameCenter()
{
	// check if user wants to disable GC
	UBOOL bDisableGameCenter = FALSE;
	GConfig->GetBool(TEXT("OnlineSubsystemGameCenter.OnlineSubsystemGameCenter"), TEXT("bDisableGameCenter"), bDisableGameCenter, GEngineIni);

	return !bDisableGameCenter && ([IPhoneAppDelegate GetDelegate].OSVersion >= 4.1f);
}

/**
 * Starts GameCenter (can be disabled in a .ini for UDK users)
 */
void IPhoneStartGameCenter()
{
	if (IPhoneDynamicCheckForGameCenter())
	{
		// create the game center object
		GGameCenter = [FGameCenter alloc];

		// initialize on main iOS thread, it may show UI to log in
		[GGameCenter performSelectorOnMainThread:@selector(init) withObject:nil waitUntilDone:NO];
	}
}

/**
 * Check to see if OS supports achievement banners
 */
bool IPhoneCheckAchievementBannerSupported()
{
	return ([IPhoneAppDelegate GetDelegate].OSVersion >= 5.0f);
}

/**
 * @return GameCenter PlayerId (autoreleased) representation of the given FUniqueNetId
 */
NSString* NetIdToPlayerId(const FUniqueNetId& NetId)
{
	NSString* PlayerId = [NSString stringWithFormat:@"G:%lld", NetId.Uid];
//	debugf(TEXT("Converted NetId %lld to PlayerId %s"), NetId.Uid, *FString(PlayerId));
	return PlayerId;
}

/**
 * @return FUniqueNetID representation of the given GameCenter PlayerId
 */
FUniqueNetId PlayerIdToNetId(NSString* PlayerId)
{
	FUniqueNetId NetId;
	NetId.Uid = [[PlayerId substringFromIndex:2] longLongValue];
//	debugf(TEXT("Converted PlayerId %s to NetId %lld"), *FString(PlayerId), NetId.Uid);
	return NetId;
}


// local, private function declarations
@interface FGameCenter()

/**
 * Try to resubmit any scores that failed previously (and were saved via BackupScore)
 */
- (void)ResubmitScores;

/**
 * Uploads an array of GKScore objects, saving out any error cases
 *
 * @param Scores NSArray of GKScore's to upload to Game Center
 */
- (void)UploadScoreArray:(NSArray*)Scores;

/**
 * Loads the cached achievements from disk
 */
- (void)LoadCachedAchievements;

/**
 * Write the current state of achievements to disk
 */
- (void)CacheAchievements;

/**
 * Kick off the read achievement operation, and figure out what to do with any 
 * differences against the locally cached achievements
 */
- (void)DownloadAchievementsAndResolveWithLocal;

@end

@implementation FGameCenter

/** Values that we need to save from a received invite so game code can process the information and come back */
@synthesize ReceivedInvite;
@synthesize ReceivedPlayersToInvite;
@synthesize CurrentMatch;
@synthesize CurrentMatchmaker;
@synthesize CurrentVoiceChat;
@synthesize PendingMessages;
@synthesize CachedAchievements;
@synthesize CachedAchievementDescriptions;
@synthesize OnlineSubsystem;
@synthesize ReadAchievementsTask;
@synthesize PlayersTalking;

/** 
 * Cache the OSS in the GameCenter object
 */
- (id)init
{
	self = [super init];

	if (self)
	{
		// allocate the pending messages array
		self.PendingMessages = [NSMutableArray arrayWithCapacity:32];

		// allocate a dictionary for tracking who's talking
		self.PlayersTalking = [NSMutableDictionary dictionaryWithCapacity:4];
		YesNumber = [[NSNumber alloc] initWithBool:YES];
		NoNumber = [[NSNumber alloc] initWithBool:NO];

		bDirtyAchievements = FALSE;
		ReadAchievementsStatus = OERS_NotStarted;

		// cache the app delegate object
		AppDelegate = [IPhoneAppDelegate GetDelegate];

		// register for going offline notification
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(OnAuthenticationChanged) name:GKPlayerAuthenticationDidChangeNotificationName object:nil];

		// log the user in, and create an async task to callback to the game thread
		[self AuthenticateLocalUser:[[IPhoneAsyncTask alloc] init]];
	}

	return self;
}


- (void)dealloc
{
	[super dealloc];

	self.ReceivedInvite = nil;
	self.ReceivedPlayersToInvite = nil;
	self.CurrentMatch = nil;
	self.CurrentMatchmaker = nil;
	self.CurrentVoiceChat = nil;
	[self.PendingMessages removeAllObjects];
	self.PendingMessages = nil;
	self.ReadAchievementsTask = nil;
	[self.CachedAchievements removeAllObjects];
	self.CachedAchievements = nil;
	self.CachedAchievementDescriptions = nil;
}

/**
 * Queues up a function to be called on the main thread. This will create an 
 * AsyncTask object for handling replies back to game thread on completion/failure
 *
 * @param Selector Function to run on main thread
 */
- (void)PerformTaskOnMainThread:(SEL)Selector
{
	// just call the full version with nil user data
	[self PerformTaskOnMainThread:Selector WithUserData:nil];
}

/**
 * Queues up a function to be called on the main thread. This will create an 
 * AsyncTask object for handling replies back to game thread on completion/failure
 *
 * @param Selector Function to run on main thread
 * @param UserData Extra user data to send along to the main thread (will be retained via IPhoneAsyncTask property)
 */
- (void)PerformTaskOnMainThread:(SEL)Selector WithUserData:(id)UserData
{
	// create the async task object, which will register it with the array of tasks to check for completion
	IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];

	// assign any user data
	AsyncTask.UserData = UserData;

	// queue up the selector for main thread with builtin functionality
	[self performSelectorOnMainThread:Selector withObject:AsyncTask waitUntilDone:NO];
}



/**
 * Shows the given Game Center supplied controller on the screen
 *
 * @param Controller The Controller object to animate onto the screen
 */
- (void)ShowController:(UIViewController*)Controller
{
	// slide it onto the screen
	[AppDelegate.Controller presentModalViewController:Controller animated:YES];

	// stop drawing the 3D world for faster UI speed
	FViewport::SetGameRenderingEnabled(FALSE);
}


/**
 * Hides the given Game Center supplied controller from the screen, optionally controller
 * animation (sliding off)
 *
 * @param Controller The Controller object to animate off the screen
 * @param bShouldAnimate YES to slide down, NO to hide immediately
 */
- (void)HideController:(UIViewController*)Controller Animated:(BOOL)bShouldAnimate
{
	// slide it off
	[Controller dismissModalViewControllerAnimated:bShouldAnimate];

	// stop drawing the 3D world for faster UI speed
	FViewport::SetGameRenderingEnabled(TRUE);
}

/**
 * Hides the given Game Center supplied controller from the screen
 *
 * @param Controller The Controller object to animate off the screen
 */
- (void)HideController:(UIViewController*)Controller
{
	// call the other version with default animation of YES
	[self HideController:Controller Animated:YES];
}

/**
 * Start voice chat on a given channel
 */
- (void)StartVoiceChat:(IPhoneAsyncTask*)Task
{
	// Ask the INI file if voice is wanted
	UBOOL bHasVoiceEnabled = FALSE;
	if (GConfig->GetBool(TEXT("VoIP"),TEXT("bHasVoiceEnabled"),bHasVoiceEnabled,GEngineIni))
	{
		// Check to see if voice is enabled/disabled
		if (bHasVoiceEnabled == FALSE)
		{
			debugf(NAME_DevOnline, TEXT("VOIP is disabled in Engine.ini (bHasVoiceEnabled=false)"));
			[Task FinishedTask];
			return;
		}
	}

	NSString* ChannelName = Task.UserData;
	if (self.CurrentMatch == nil)
	{
		debugf(NAME_DevOnline, TEXT("Tried to start voice chat without a match in progress"));
		[Task FinishedTask];
		return;
	}

	// don't do anything if aleader setup
	if (self.CurrentVoiceChat)
	{
		debugf(NAME_DevOnline, TEXT("Tried to start voice chat while it was already started"));
		[Task FinishedTask];
		return;
	}

	if ([GKVoiceChat isVoIPAllowed] == NO)
	{
		debugf(NAME_DevOnline, TEXT("VOIP is not allowed, skipping voice chat"));
		[Task FinishedTask];
		return;
	}

	// set our audio session category to one useful for voice chat (play and record)
	AVAudioSession* AudioSession = [AVAudioSession sharedInstance];
	if ([AudioSession setCategory:AVAudioSessionCategoryPlayAndRecord error:NULL] == NO)
	{
		debugf(NAME_DevOnline, TEXT("Failed to setCategory on the AudioSession"));
		[Task FinishedTask];
		return;
	}
	
	if ([AudioSession setActive: YES error:NULL] == NO)
	{
		debugf(NAME_DevOnline, TEXT("Failed to setActive on the AudioSession"));
		[Task FinishedTask];
		return;
	}


	// get the voice chat object for the match
	self.CurrentVoiceChat = [self.CurrentMatch voiceChatWithName:ChannelName];

	self.CurrentVoiceChat.playerStateUpdateHandler = ^(NSString* PlayerId, GKVoiceChatPlayerState State)
	{
		if (State == GKVoiceChatPlayerConnected || State == GKVoiceChatPlayerDisconnected)
		{
			// gameplay should handle muting us, but in case the OSS code needs to trigger the 
			// filtering, here is the code
			/**
			if (State == GKVoiceChatPlayerConnected)
			{
				// start with the player muted, then allow script to try to unmute the player
				[self.CurrentVoiceChat setMute:YES forPlayer:PlayerId];

				// when a remote player joins, we need to trigger some script callbacks to 
				// eventually potentially mute this player
				IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
				AsyncTask.GameThreadCallback = ^ UBOOL (void)
				{
					// find the player controller for player 0
					APlayerController* PlayerController = NULL;
					for (FLocalPlayerIterator It(GEngine); It; ++It)
					{
						// there is no out-of-game mute list, so attempt to unmute this player (the script
						// can override and disallow the unmute
						PlayerController->eventServerUnmutePlayer(PlayerIdToNetId(PlayerId));
						return TRUE;
					}

				};
				[AsyncTask FinishedTask];
			}
			*/
		}
		else if (State == GKVoiceChatPlayerSpeaking || State == GKVoiceChatPlayerSilent)
		{
			// track if the user is talking of not
			@synchronized(PlayersTalking)
			{
				[PlayersTalking setValue:(State ==  GKVoiceChatPlayerSpeaking ? YesNumber : NoNumber) forKey:PlayerId];
			}

			// tell game thread about the talking change
			// @todo: Could only do this if tehre are delegates, but can't truly query if
			// there are delegates without some thread safety - this can happen a lot so 
			// it's worth optimizing
			IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
			AsyncTask.GameThreadCallback = ^ UBOOL (void)
			{
				if (OnlineSubsystem->TalkingDelegates.Num() > 0)
				{
					OnlineSubsystemGameCenter_eventOnPlayerTalkingStateChange_Parms Parms(EC_EventParm);
					// Use the cached id for this
					Parms.Player = PlayerIdToNetId(PlayerId);
					Parms.bIsTalking = State == GKVoiceChatPlayerSpeaking;
					TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->TalkingDelegates, &Parms);
				}

				return TRUE;
			};
			[AsyncTask FinishedTask];
		}
	};

	[self.CurrentVoiceChat start];
	self.CurrentVoiceChat.active = YES;

	[Task FinishedTask];
}

/**
 * Shuts down voice chatting
 */
- (void)StopVoiceChat:(IPhoneAsyncTask*)Task
{
	if (self.CurrentVoiceChat)
	{
		AVAudioSession* AudioSession = [AVAudioSession sharedInstance];
		if ([AudioSession setCategory:AVAudioSessionCategorySoloAmbient error:NULL] == NO)
		{
			debugf(NAME_DevOnline, TEXT("Failed to setCategory on the AudioSession"));
		}
		
		if ([AudioSession setActive: YES error:NULL] == NO)
		{
			debugf(NAME_DevOnline, TEXT("Failed to setActive on the AudioSession"));
		}

		[self.CurrentVoiceChat stop];
		self.CurrentVoiceChat = nil;
	}

	// no one can be talking now!
	[self.PlayersTalking removeAllObjects];

	if (Task)
	{
		[Task FinishedTask];
	}
}

/**
 * Mutes the given player
 */
- (void)MutePlayer:(IPhoneAsyncTask*)Task
{
	if (self.CurrentVoiceChat)
	{
		// tell the voice chat to mute the player
		NSString* PlayerID = Task.UserData;
		[self.CurrentVoiceChat setMute:YES forPlayer:PlayerID];
	}
}

/**
 * Unmutes the given player
 */
- (void)UnmutePlayer:(IPhoneAsyncTask*)Task
{
	if (self.CurrentVoiceChat)
	{
		// tell the voice chat to unmute the player
		NSString* PlayerID = Task.UserData;
		[self.CurrentVoiceChat setMute:NO forPlayer:PlayerID];
	}
}


/**
 * @return TRUE if the given Player is currently talking
 */
- (UBOOL)GameThreadIsPlayerTalking:(NSString*)PlayerId
{
	@synchronized(PlayersTalking)
	{
		// nil input means local player
		if (PlayerId == nil)
		{
			PlayerId = [GKLocalPlayer localPlayer].playerID;
		}
		// look up this player in the is talking mapping
		NSNumber* IsTalking = [PlayersTalking valueForKey:PlayerId];
		// if its there and the value is Yes, then the player is talking
		if (IsTalking != nil && IsTalking == YesNumber)
		{
			return TRUE;
		}
	}

	return FALSE;
}


/**
 * @return GameCenter PlayerId (autoreleased) representation of the given FUniqueNetId
 */
int  CalcStringByteSum(NSString * aString)
{
	int sum = 0;
	int strLen = [aString length];
	int charIndex;

	for (charIndex = 0; charIndex < strLen; charIndex++)
	{
		unichar cur = [aString characterAtIndex:charIndex];
		sum += (int) cur;
	}
	return sum;
}



/**
 * When all players have joined a match, this is called to tell game thread to go!
 */
- (void)StartMatch
{
	int StringSum = 0;
	debugf(NAME_DevOnline, TEXT("GameCenter::StartMatch"));

	// if we've already started the match, do nothing
	if (bHasStartedMatch)
	{
		return;
	}

	// figure out who is going to be the server (all devices run this same check, and all 
	// will agree on the outcome, since the playerIDs are all in sync)
	NSString* ServerId = [GKLocalPlayer localPlayer].playerID;

	debugf(NAME_DevOnline, TEXT("My Player Name(%d):%s "), CalcStringByteSum(ServerId), UTF8_TO_TCHAR([ServerId cStringUsingEncoding:NSUTF8StringEncoding]));

	StringSum = CalcStringByteSum(ServerId);
	for (NSString* Id in self.CurrentMatch.playerIDs)
	{
		int Sum = CalcStringByteSum(Id);
		debugf(NAME_DevOnline, TEXT("Other Player Name(%d):%s"), Sum, UTF8_TO_TCHAR([Id cStringUsingEncoding:NSUTF8StringEncoding]));
		StringSum += Sum;
	}
	debugf(NAME_DevOnline, TEXT("StringSum:%d"), StringSum);

	// figure out if we are the server or a client
	for (NSString* Id in self.CurrentMatch.playerIDs)
	{
		// To help randomize it a little, depending on the byte sum either pick largest or smallest id.
		if (StringSum & 1)
		{
			// is the other Id smaller?
			if ([Id caseInsensitiveCompare:ServerId] == NSOrderedAscending)
			{
				ServerId = Id;
			}
		}
		else
		{
			// is the other Id bigger?
			if ([Id caseInsensitiveCompare:ServerId] == NSOrderedDescending)
			{
				ServerId = Id;
			}
		}
	}

	// we're the server if or playerID is biggest
	UBOOL bIsServer = [ServerId isEqualToString:[GKLocalPlayer localPlayer].playerID];

	debugf(NAME_DevOnline, TEXT("GameCenter::Is %s"), bIsServer ? TEXT("Server") : TEXT("Client"));
		
	// if we're the server, we just send a callback that we have Created an online (most platforms
	// would have the user create an online game, and would be the server)
	if (bIsServer)
	{
		// and we're done!
		MatchmakingTask.GameThreadCallback = ^ UBOOL (void)
		{
			debugf(NAME_DevOnline, TEXT("GameCenter::In Server Callback"));

			// trigger the server created match callback
			FAsyncTaskDelegateResultsNamedSession Results(NAME_None, 0);
			TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->CreateOnlineGameDelegates, &Results);

			return TRUE;
		};
	}
	else
	{
		// setup the results to return to client players
		MatchmakingTask.GameThreadCallback = ^ UBOOL (void)
		{
			debugf(NAME_DevOnline, TEXT("GameCenter::In Client Callback"));

			FOnlineGameSearchResult* ServerResult = NULL;

			// if there was already a result in the GameSearch, that means we accepted an invite and the 
			// result was already set up
			if (OnlineSubsystem->GameSearch->Results.Num() > 0)
			{
				ServerResult = &OnlineSubsystem->GameSearch->Results(0);
			}
			// otherwise create a new result
			else
			{
				ServerResult = new (OnlineSubsystem->GameSearch->Results)FOnlineGameSearchResult(EC_EventParm);

				// create a basic settings object to open up a space for the player to join
				ServerResult->GameSettings = ConstructObject<UOnlineGameSettings>(UOnlineGameSettings::StaticClass());
				ServerResult->GameSettings->NumPublicConnections = ServerResult->GameSettings->NumOpenPublicConnections = 1;
				ServerResult->GameSettings->bWasFromInvite = TRUE;
			}

			//Settings->OwningPlayerName = [BiggestId cStringUsingEncoding:NSUTF8StringEncoding];
			// Settings->OwningPlayerId = 

			// return success to the delegate, who can now look in the results
			FAsyncTaskDelegateResultsNamedSession Results(TEXT("Game"), 0);
			TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->JoinOnlineGameDelegates, &Results);

			return TRUE;
		};
	}

	// finish up the task
	[MatchmakingTask FinishedTask];
	MatchmakingTask = nil;

	// close the matchmaking UI
	[self HideController:self.CurrentMatchmaker];
	self.CurrentMatchmaker = nil;

	// start up voice chat
	// @todo: Get Channel from script code somehow
//	[self StartVoiceChat:@"Game"];
}

/**
 * Sends a message to the game thread via an AsyncTask when user goes on or offline
 * 
 * @param Task Async task object to send message through
 * @param bIsOnline TRUE if the user is now online
 */
-(void) SendSigninChangeAndFinishTask:(IPhoneAsyncTask*)Task IsOnline:(UBOOL)bIsOnline
{
	// don't do anything if the state didn't actually change
	if (bIsOnline == bIsAuthenticated)
	{
		[Task FinishedTask];
		return;
	}

	// remember what we changed to
	bIsAuthenticated = bIsOnline;

	// cache the string so the block doesn't call the GC functionality (most likely this
	// would be fine to call on game thread, but better safe then sorry!)
	NSString* PlayerAlias = bIsOnline ? [GKLocalPlayer localPlayer].alias : nil;
	NSString* PlayerId = bIsOnline ? [GKLocalPlayer localPlayer].playerID : nil;

	// set the code to run on main thread
	Task.GameThreadCallback = ^ UBOOL (void)
	{
		UOnlineSubsystemGameCenter* OnlineSub = GGameCenter.OnlineSubsystem;
		if (bIsOnline)
		{
			// set the player's name, to note the user is logged in
			OnlineSub->LoggedInPlayerName = FString(PlayerAlias);
			OnlineSub->LoggedInPlayerId = PlayerIdToNetId(PlayerId);

			debugf(NAME_DevOnline, TEXT("Game Center local user is now authenticated and online"));
		}
		else
		{
			// make sure hte LoggedInPlayerName is empty as that is how we determine if we are logged in
			OnlineSub->LoggedInPlayerName = TEXT("");
			OnlineSub->LoggedInPlayerId.Uid = 0;

			/// @todo: call delegates here
			debugf(NAME_DevOnline, TEXT("Game Center local user has been signed out!"));
		}

		// Notify the game code a login change occured
		OnlineSubsystemGameCenter_eventOnLoginChange_Parms Parms(EC_EventParm);
		Parms.LocalUserNum = 0;
		TriggerOnlineDelegates(OnlineSub, OnlineSub->LoginChangeDelegates, &Parms);

		// Notify the game code a login change occured
		OnlineSubsystemGameCenter_eventOnLoginStatusChange_Parms Parms2(EC_EventParm);
		Parms2.NewId = OnlineSub->LoggedInPlayerId;
		Parms2.NewStatus = bIsOnline ? LS_LoggedIn : LS_NotLoggedIn;
		TriggerOnlineDelegates(OnlineSub, OnlineSub->PlayerLoginStatusDelegates, &Parms2);

		return TRUE;
	};

	[Task FinishedTask];

	// if we have now signed in, then kick off reuploading any scores that
	// failed to upload previously
	[self ResubmitScores];
}

/**
 * Callback for when the local player has changed authentication state
 */
-(void) OnAuthenticationChanged
{
	// cache the auth status for easier use in the block
	UBOOL bIsNowAuthenticated = [GKLocalPlayer localPlayer].authenticated;

	// create a task for communicating to the game thread
	IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];

	// send message to game thread
	[self SendSigninChangeAndFinishTask:AsyncTask IsOnline:bIsNowAuthenticated];
}

/**
 * Authenticate the user with GameCenter
 */
- (void)AuthenticateLocalUser:(IPhoneAsyncTask*)Task
{
	// check if we are currently authenticating
	if (bIsAuthenticating)
	{
		// since another one is in flight, we can just early out here, and not send any 
		// delegate callbacks, just finish the task
		[Task FinishedTask];

		return;
	}

	// if we are already authenticated, nothing to do here!
	if ([GKLocalPlayer localPlayer].authenticated == YES)
	{
		// finish the task
		[Task FinishedTask];

		return;
	}

	// mark that we are authenticating so another one doesn't interfere
	bIsAuthenticating = YES;

	// authenticate the local user
	[[GKLocalPlayer localPlayer] authenticateWithCompletionHandler:
		^(NSError* Error) 
		{
			// we are no longer authenticating - error or not
			bIsAuthenticating = NO;

			if (Error)
			{
				NSLog(@"Error while logging in: %@\n", Error);

				INT ErrorCode = [Error code];

				Task.GameThreadCallback = ^ UBOOL (void)
				{
					// clear the logged in player name
					GGameCenter.OnlineSubsystem->LoggedInPlayerName = TEXT("");

					// user canceled returns error code 2
					if (ErrorCode == 2)
					{
						// It seems that if the user has turned down game center login 3 times, after that point Apple always returns the user cancelled error code until they log in manually through game center. 
						TriggerOnlineDelegates(GGameCenter.OnlineSubsystem, GGameCenter.OnlineSubsystem->LoginCancelledDelegates, NULL);
					}
					else
					{
						// use a UE3 error code (we could look at Error here if we needed)
						BYTE Params[2];
						Params[0] = 0;
						Params[1] = OSCS_NotConnected;
						
						// trigger any LoginFailed delegates
						TriggerOnlineDelegates(GGameCenter.OnlineSubsystem, GGameCenter.OnlineSubsystem->LoginFailedDelegates, Params);
					}
					
					return TRUE;
				};

				// finish the task
				[Task FinishedTask];
			}
			else
			{
				// make a local copy of the string to use in the block
				NSString* PlayerAlias = [GKLocalPlayer localPlayer].alias;

				debugf(NAME_DevOnline, TEXT("Player %s logged in, unique ID is %s"), 
					UTF8_TO_TCHAR([PlayerAlias cStringUsingEncoding:NSUTF8StringEncoding]),
					UTF8_TO_TCHAR([[GKLocalPlayer localPlayer].playerID cStringUsingEncoding:NSUTF8StringEncoding]));

				// reset achievements if desired
				if (ParseParam(appCmdLine(), TEXT("resetachievements")))
				{
					[GKAchievement resetAchievementsWithCompletionHandler:^(NSError* Error) 
						{
							debugf(NAME_DevOnline, TEXT("Achievements have been reset because -resetachievements was specified"));
						}
					];
				}
				else
				{
					// load the local state of achievements from disk (if not resetting state)
					[self LoadCachedAchievements];
				}

				// send the online message to game thread
				[self SendSigninChangeAndFinishTask:Task IsOnline:TRUE];

				// start downloading achievements from server
				[self DownloadAchievementsAndResolveWithLocal];

				// allow for accepting invites
				[GKMatchmaker sharedMatchmaker].inviteHandler = 
					^(GKInvite* Invite, NSArray* PlayersToInvite)
					{
						debugf(NAME_DevOnline, TEXT("Received an invitation"));

						// cache the input for the AcceptInvitation call
						self.ReceivedInvite = Invite;
						self.ReceivedPlayersToInvite = PlayersToInvite;

						// if a matchmaker was up, then close it and send a delegate to the game
						if (self.CurrentMatchmaker)
						{
							[self matchmakerViewControllerWasCancelled:CurrentMatchmaker];
						}

						IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
						AsyncTask.GameThreadCallback = ^ UBOOL (void)
						{
							// cross title invite
							// create an basic result to send back
							OnlineSubsystemGameCenter_eventOnGameInviteAccepted_Parms Parms(EC_EventParm);
							Parms.InviteResult = FOnlineGameSearchResult(EC_EventParm);

							// create a basic game search object to hold the result, for later joining to it
							GGameCenter.OnlineSubsystem->FreeSearchResults();
							GGameCenter.OnlineSubsystem->GameSearch = ConstructObject<UOnlineGameSearch>(UOnlineGameSearch::StaticClass());
							GGameCenter.OnlineSubsystem->GameSearch->Results.AddItem(Parms.InviteResult);

							// create a basic settings object to open up a space for the player to join
							UOnlineGameSettings* Settings = ConstructObject<UOnlineGameSettings>(UOnlineGameSettings::StaticClass());
							Settings->NumPublicConnections = Settings->NumOpenPublicConnections = 1;
							Settings->bWasFromInvite = TRUE;
							Parms.InviteResult.GameSettings = Settings;

							// finally, after setting up the objects, trigger the callbacks
							TriggerOnlineDelegates(GGameCenter.OnlineSubsystem, GGameCenter.OnlineSubsystem->GameInviteAcceptedDelegates, &Parms);
							
							return TRUE;
						};

						// we're ready for the game thread to process the above block
						[AsyncTask FinishedTask];
					};
			}
		}
	];
}

- (void)GetFriendNames:(NSArray*)Friends WithTask:(IPhoneAsyncTask*)Task
{
	// now we need to turn around and ask for player details, since all we get now is unique ids
	[GKPlayer loadPlayersForIdentifiers:Friends withCompletionHandler:
		^(NSArray* Players, NSError* Error2)
		{
			Task.GameThreadCallback = ^ UBOOL (void)
			{
				if (Error2)
				{
					OnlineSubsystem->ReadFriendsStatus = OERS_Failed;

					// fire off the delegates (negative number for error for FAsyncTaskDelegateResults);
					FAsyncTaskDelegateResults Results(-1);
					TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->ReadFriendsDelegates, &Results);
				}
				else
				{
					// preallocate friend memory
					OnlineSubsystem->CachedFriends.Empty([Players count]);

					// fill in the friends list for game thread access
					for (GKPlayer* Friend in Players)
					{
						FOnlineFriend* NewFriend = new(OnlineSubsystem->CachedFriends) FOnlineFriend;
						NewFriend->NickName = [Friend.alias cStringUsingEncoding:NSUTF8StringEncoding];
						NewFriend->UniqueId = PlayerIdToNetId(Friend.playerID);

						debugf(NAME_DevOnline, TEXT("Received friend %s"), *NewFriend->NickName);
					}

					OnlineSubsystem->ReadFriendsStatus = OERS_Done;

					// fire off the delegates (0 is success for FAsyncTaskDelegateResults);
					FAsyncTaskDelegateResults Results(0);
					TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->ReadFriendsDelegates, &Results);
				}

				return TRUE;
			};

			// we're done (finally!)
			[Task FinishedTask];
		}
	];
}


/**
 * Download friends list
 */
- (void)GetFriends:(IPhoneAsyncTask*)Task
{
	// get the friends list for the local player from the server
	[[GKLocalPlayer localPlayer] loadFriendsWithCompletionHandler:
		^(NSArray* Friends, NSError* Error) 
		{
			if (Error || ParseParam(appCmdLine(), TEXT("fakeoffline")))
			{
				// failure callback
				Task.GameThreadCallback = ^ UBOOL (void)
				{
					OnlineSubsystem->ReadFriendsStatus = OERS_Failed;

					// fire off the delegates (negative number for error for FAsyncTaskDelegateResults);
					FAsyncTaskDelegateResults Results(-1);
					TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->ReadFriendsDelegates, &Results);

					return TRUE;
				};

				// finish the task
				[Task FinishedTask];
			}
			else
			{
				if (Friends == nil || [Friends count] == 0)
				{
					// failure callback
					Task.GameThreadCallback = ^ UBOOL (void)
					{
						OnlineSubsystem->ReadFriendsStatus = OERS_Done;
	
						// fire off the delegates (negative number for error for FAsyncTaskDelegateResults);
						FAsyncTaskDelegateResults Results(0);
						TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->ReadFriendsDelegates, &Results);
	
						return TRUE;
					};
	
					// finish the task
					[Task FinishedTask];
				}
				else
				{
					[self GetFriendNames:Friends WithTask:Task];
				}
			}
		}
	];
}

/**
 * Loads an object graph from disk saved via SaveObject:ToFile
 *
 * @param Filename Full pathname of the file to load
 *
 * @return Root object of the object graph, or nil if Filename doesn't exist
 */
- (id)LoadObjectFromFile:(NSString*)Filename
{
	// use the NSCoding interface to load the score from disk very easily
	return [NSKeyedUnarchiver unarchiveObjectWithFile:Filename];
}

/**
 * Saves an object graph to disk that supports the NSCoding interface. Note that arrays
 * must have property list types (strings, NSNumbers, etc)
 * 
 * @param Object The object to save to disk
 * @param Filename The full pathname of the file to save to
 */
- (void)SaveObject:(id)Object ToFile:(NSString*)Filename
{
	// used the NSCoding interface to save the score to disk very easily
	[NSKeyedArchiver archiveRootObject:Object toFile:Filename];
}

/**
 * @return get a path to a directory to write archive files to
 */
- (NSString*)GetArchiveDirectoryName
{
	// get the documents directory we'll be writing to
	NSString* DocumentsDir = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex: 0];

	// use the unique player ID as part of the username
	NSString* Username = [GKLocalPlayer localPlayer].playerID;
	NSString* DirName = [DocumentsDir stringByAppendingPathComponent:Username];

	// make sure directory exists
	[[NSFileManager defaultManager] createDirectoryAtPath:DirName withIntermediateDirectories:YES attributes:nil error:NULL];

	return DirName;
}

/**
 * Saves a score to disk if it failed to upload. Handles both ascending and descending
 * leaderboards by saving both the min and max scores that fail to upload
 *
 * @param NewScore The score object that failed to upload
 */
- (void)BackupScore:(GKScore*)NewScore
{
	NSString* DirName = [self GetArchiveDirectoryName];

	// get the score name
	NSString* ScoreName = NewScore.category;

	// we need to write the smallest and largest scores, because we don't know if the leaderboard is ascending or descending
	NSString* MinScoreFilename = [DirName stringByAppendingPathComponent:[NSString stringWithFormat:@"%@_min.score", ScoreName]];
	NSString* MaxScoreFilename = [DirName stringByAppendingPathComponent:[NSString stringWithFormat:@"%@_max.score", ScoreName]];

	// did we get smaller than an existing one (or does one not exist)?
	GKScore* MinScore = [self LoadObjectFromFile:MinScoreFilename];
	if (MinScore == nil || NewScore.value < MinScore.value)
	{
		// if so, write it out
		[self SaveObject:NewScore ToFile:MinScoreFilename];
	}

	// did we get a larger score than an existing one (or does one not exist)?
	GKScore* MaxScore = [self LoadObjectFromFile:MaxScoreFilename];
	if (MaxScore == nil || NewScore.value > MaxScore.value)
	{
		// if so, write it out
		[self SaveObject:NewScore ToFile:MaxScoreFilename];
	}
}

/**
 * Try to resubmit any scores that failed previously (and were saved via BackupScore)
 */
- (void)ResubmitScores
{
	// look for any scores for this player

	// get the directory we'll be reading from
	NSString* DocumentsDir = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex: 0];
	NSString* Username = [GKLocalPlayer localPlayer].playerID;
	NSString* DirName = [DocumentsDir stringByAppendingPathComponent:Username];

	// now, loop over the files in the directory and resubmit them
	NSDirectoryEnumerator* DirEnumerator = [[NSFileManager defaultManager] enumeratorAtPath:DirName];
	
	// make an array to hold the scores - we will delete all the files before we upload anything (since 
	// we'll be writing back to the directory if it just fails again]
	NSMutableArray* Scores = [NSMutableArray arrayWithCapacity:4];
	NSMutableArray* FilesToDelete = [NSMutableArray arrayWithCapacity:4];

	NSString* File;
	while ((File = [DirEnumerator nextObject]))
	{
		if (FFilename(FString(File)).GetExtension() == TEXT("score"))
		{
			NSString* FullPath = [DirName stringByAppendingPathComponent:File];
			// load the score from disk
			GKScore* SavedScore = [self LoadObjectFromFile:FullPath];
			// add it to the list of scores
			if (SavedScore)
			{
				debugf(NAME_DevOnline, TEXT("Resubmitting score from %s"), UTF8_TO_TCHAR([File cStringUsingEncoding:NSUTF8StringEncoding])); 
				[Scores addObject:SavedScore];
			}
			// remember this file to delete below (to not conflict with the enumerator)
			[FilesToDelete addObject:FullPath];
		}
	}

	// now submit all the scores
	if ([Scores count])
	{
		// delete the files, without using the enumerator 
		for (NSString* FileToDelete in FilesToDelete)
		{
			[[NSFileManager defaultManager] removeItemAtPath:FileToDelete error:NULL];
		}

		[self UploadScoreArray:Scores];	
	}
}

/**
 * Uploads an array of GKScore objects, saving out any error cases
 *
 * @param Scores NSArray of GKScore's to upload to Game Center
 */
- (void)UploadScoreArray:(NSArray*)Scores
{
	// send each score
	for (GKScore* Score in Scores)
	{
		// handle faking being offline to skip uploading the score, and just cache it for later
		if (ParseParam(appCmdLine(), TEXT("fakeoffline")))
		{
			[self BackupScore:Score];
		}
		else
		{
			// report it!
			[Score reportScoreWithCompletionHandler: ^(NSError* Error)
				{
					// if there was an error, save it for later
					if (Error)
					{
						debugf(NAME_DevOnline, TEXT("Uploading score reported an error, so its being saved to be submitted later"));
						[self BackupScore:Score];
					}
					else
					{
						debugf(NAME_DevOnline, TEXT("Score of %d was published to leaderboard"), Score.value);
					}
				}
			];
		}
	}
}

/**
 * Upload some scores to a leaderboard
 */
- (void)UploadScores:(IPhoneAsyncTask*)Task
{
	// score passed as the UserData
	NSMutableArray* Scores = Task.UserData;

	// send em all
	[self UploadScoreArray:Scores];

	// we finish the task, there is nothing we actually need to perform on game thread
	// but we still need to mark the task finished to free the memory
	[Task FinishedTask];
}

/**
 * Fill out the game thread information based on the results from the Columns
 *
 * @param Columns The results of the leaderboards read (one column per category)
 * @param Task Async task object used to communicate back to game thread
 */
- (void)FinalizeColumns:(NSArray*)Columns WithTask:(IPhoneAsyncTask*)Task
{
	check([Columns count] > 0);

	// use the first columns's number of rows to determine the number of rows in the result
	NSArray* FirstColumn = [Columns objectAtIndex:0];
	INT NumRows = [FirstColumn count];

	// now set up an array of player IDs to query for names
	NSMutableArray* PlayersToLookup = [NSMutableArray arrayWithCapacity:NumRows];
	for (GKScore* Score in FirstColumn)
	{
		[PlayersToLookup addObject:Score.playerID];
	}

	[GKPlayer loadPlayersForIdentifiers:PlayersToLookup withCompletionHandler:
		^(NSArray* PlayerNames, NSError* Error)
		{
			if (Error)
			{
				debugf(NAME_DevOnline, TEXT("FinalizeColumns.loadPlayersForIdentifiers failed with an Error"));
				Task.GameThreadCallback = ^ UBOOL (void) 
				{
					FAsyncTaskDelegateResults Results(-1);
					TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->ReadOnlineStatsCompleteDelegates, &Results);
					OnlineSubsystem->CurrentStatsRead = NULL;
					return TRUE;
				};
			}
			else
			{
				Task.GameThreadCallback = ^ UBOOL (void) 
				{
					debugf(NAME_DevOnline, TEXT("Finished downloading all scores [%s]!"), OnlineSubsystem->CurrentStatsRead ? *OnlineSubsystem->CurrentStatsRead->GetFullName() : TEXT("BADNESS"));
					// fill out the results in the stats read object
					UOnlineStatsRead* StatsRead = OnlineSubsystem->CurrentStatsRead;
					StatsRead->Rows.AddZeroed(NumRows);

					// now set up each row
					for (INT RowIndex = 0; RowIndex < NumRows; RowIndex++)
					{
						FOnlineStatsRow& Row = StatsRead->Rows(RowIndex);
						// get a score from first column for this row
						GKScore* FirstScore = [FirstColumn objectAtIndex:RowIndex];

						// turn player Id into a FUniqueNetId
						Row.PlayerID = PlayerIdToNetId(FirstScore.playerID);

						// use the first column's rank as the whole rank for this 'view'
						Row.Rank.SetData(FirstScore.rank);

						// use the looked up player name
						GKPlayer* Player = [PlayerNames objectAtIndex:RowIndex];
						Row.NickName = FString([Player.alias cStringUsingEncoding:NSUTF8StringEncoding]);
						
						// now make space for the columns
						INT NumColumns = [Columns count];
						Row.Columns.AddZeroed(NumColumns);

						// fill them out
						INT ColumnIndex = 0; 
						for (NSArray* Column in Columns)
						{

							// find the score in this column matching the player of the FirstColumn (if it exists)
							GKScore* MatchingScore = nil;
							for (GKScore* ColumnScore in Column)
							{
								if ([ColumnScore.playerID compare:FirstScore.playerID] == NSOrderedSame)
								{
									MatchingScore = ColumnScore;
									break;
								}
							}

							// get the score for this row/column
							if (MatchingScore)
							{
								Row.Columns(ColumnIndex).StatValue.SetData((QWORD)MatchingScore.value);

								// set which column this stat belongs to
								Row.Columns(ColumnIndex).ColumnNo = OnlineSubsystem->GetPropertyIdFromCategoryName(FString(MatchingScore.category));
							}
							else
							{
								// if this player didn't have this score reported/returned, then set it to 0
								QWORD Zero = 0;
								Row.Columns(ColumnIndex).StatValue.SetData(Zero);
							}

							// move to next column
							ColumnIndex++;
						}
					}

					// fire off the delegates 
					FAsyncTaskDelegateResults Results(0);
					TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->ReadOnlineStatsCompleteDelegates, &Results);
					OnlineSubsystem->CurrentStatsRead = NULL;
					return TRUE;
				};
			}

			// clean up
			[Task FinishedTask];

			// we are now done with the columns array, so we release the ref count
			[Columns release];
		}
	];

}


/**
 * Download a single score for multiple players. This will "recursively" call itself when it's done 
 * to read the next column. 
 *
 * @param Column The index into the LeaderboardRequests to download
 * @param LeaderboardRequests An array of GKLeaderboard objects, which are used to read a category
 * @param Columns Output array to store the scores in
 * @param Task Async task that we use to tell game thread when complete
 */
- (void)DownloadColumn:(INT)Column FromArray:(NSMutableArray*)LeaderboardRequests IntoColumns:(NSMutableArray*)Columns WithTask:(IPhoneAsyncTask*)Task
{
	// get the next request to process
	GKLeaderboard* LeaderboardRequest = [LeaderboardRequests objectAtIndex:Column];

	// kick it off
	[LeaderboardRequest loadScoresWithCompletionHandler: ^(NSArray *Scores, NSError *Error)
		{
			if (Error)
			{
				debugf(NAME_DevOnline, TEXT("DownloadColumn.loadScoresWithCompletionHandler failed with an Error"));
				Task.GameThreadCallback = ^ UBOOL (void) 
				{
					FAsyncTaskDelegateResults Results(-1);
					TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->ReadOnlineStatsCompleteDelegates, &Results);
					OnlineSubsystem->CurrentStatsRead = NULL;
					return TRUE;
				};
				// clean up
				[Task FinishedTask];

				// we are now done with the columns array, so we release the ref count
				[Columns release];
			}
			else
			{

				// just keep a reference to the returned Scores array
				[Columns addObject:Scores];

				// move to the next column
				INT NextColumn = Column + 1;

				// do we have any more columns to read?
				if (NextColumn < [LeaderboardRequests count])
				{
					[self DownloadColumn:NextColumn FromArray:LeaderboardRequests IntoColumns:Columns WithTask:Task];
				}
				else
				{
					// if no more columns to read, send everything back to game thread!
					[self FinalizeColumns:Columns WithTask:Task];
				}
			}
		}
	];

}


/**
 * Set up a set of column reads for a set of players, as specified in the Helper object
 * and the ReadType
 *
 * @param Helper Container object for stats read settings
 * @param ReadType Overrides the ReadType in the Helper, since some have 2 stages
 *
 * @return Array of GKLeaderboard objects to trigger
 */
- (NSMutableArray*)SetupReadsForHelper:(FReadOnlineStatsHelper*)Helper
{
	// make leaderboard requests for my friend's scores
	NSMutableArray* LeaderboardRequests = [NSMutableArray arrayWithCapacity:[Helper.Columns count]];
	for (NSString* ColumnName in Helper.Columns)
	{
		GKLeaderboard* LeaderboardRequest;

		// create a request, depending on read type
		if (Helper.ReadType == ROST_Players)
		{
			LeaderboardRequest = [[GKLeaderboard alloc] initWithPlayerIDs:Helper.Players];
		}
		else
		{
			LeaderboardRequest = [[GKLeaderboard alloc] init];
		}

		// fill out the properties
		LeaderboardRequest.playerScope = (Helper.ReadType == ROST_Friends) ? GKLeaderboardPlayerScopeFriendsOnly : GKLeaderboardPlayerScopeGlobal;
		LeaderboardRequest.timeScope = GKLeaderboardTimeScopeAllTime;
		if (Helper.ReadType == ROST_Range)
		{
			LeaderboardRequest.range = Helper.RequestRange;
		}
		
		// get the category name
		LeaderboardRequest.category = ColumnName;

		// add this request to the list
		[LeaderboardRequests addObject:LeaderboardRequest];
		[LeaderboardRequest release];
	}

	return LeaderboardRequests;
}


/**
 * Download some scores from a leaderboard
 */
- (void)DownloadScores:(IPhoneAsyncTask*)Task
{
	// get the helper object from the task
	FReadOnlineStatsHelper* Helper = Task.UserData;

	// if we just want to get the columns for a set of players or friends, just
	// set up basic read, and then trigger. Also, if there is only one output column,
	// then just trigger it in one stage as well
	if (Helper.ReadType == ROST_Players || Helper.ReadType == ROST_Friends || 
		[Helper.Columns count] == 1)
	{
		// initialize an array of columns to store results (this is retained, and we will release it
		// only when we fill out UE3 structures on game thread)
		NSMutableArray* OutputColumns = [[NSMutableArray alloc] initWithCapacity:[Helper.Columns count]];

		// set up requests
		NSMutableArray* LeaderboardRequests = [self SetupReadsForHelper:Helper];

		// do a recursive block callback scheme, where each block will trigger the next load
		[self DownloadColumn:0 FromArray:LeaderboardRequests IntoColumns:OutputColumns WithTask:Task];
	}
	else if (Helper.ReadType == ROST_Range || Helper.ReadType == ROST_AroundPlayer)
	{
		NSArray* AllColumns = Helper.Columns;
		EReadOnlineStatsType OriginalType = Helper.ReadType;

		// create an array with just the first column, this will be the "ranked"
		if (Helper.ReadType == ROST_AroundPlayer)
		{
			Helper.ReadType = ROST_Players;
			Helper.Players = [NSMutableArray arrayWithObjects:[GKLocalPlayer localPlayer].playerID, nil];
		}
		Helper.Columns = [NSMutableArray arrayWithObjects:[AllColumns objectAtIndex:0], nil];

		// fill out the request for a single ranked column
		NSMutableArray* InitialRequest = [self SetupReadsForHelper:Helper];

		// kick it off
		[[InitialRequest objectAtIndex:0] loadScoresWithCompletionHandler: ^(NSArray *Scores, NSError *Error)
			{
				FReadOnlineStatsHelper* BlockHelper = Task.UserData;
				if (Error)
				{
					debugf(NAME_DevOnline, TEXT("DownloadScores.loadScoresWithCompletionHandler failed with an Error"));
					Task.GameThreadCallback = ^ UBOOL (void) 
					{
						FAsyncTaskDelegateResults Results(-1);
						TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->ReadOnlineStatsCompleteDelegates, &Results);
						OnlineSubsystem->CurrentStatsRead = NULL;
						return TRUE;
					};
					// clean up
					[Task FinishedTask];

					// we are now done with the columns array, so we release the ref count
					[Helper.Columns release];
				}
				else
				{
					// handle setting the range for around player
					if (OriginalType == ROST_AroundPlayer)
					{
						// we will only have one score returned, the local player
						GKScore* PlayerScore = [Scores objectAtIndex:0];
						// the range for this type would be like (-5, 11) for the 5 before and 5 after the player 
						// (I would have thought we need +1 for player, but NSRange of {2,3} will read 2,3,4,5, not
						// 2,3,4, so I assume NSRange.length doesn't include location's entry...)
						BlockHelper.RequestRange = NSMakeRange(
							Max<INT>(1, PlayerScore.rank - BlockHelper.RequestRange.length),
							BlockHelper.RequestRange.length * 2);

						// now that we have a range, we can do the range mode now
						BlockHelper.ReadType = ROST_Range;
						BlockHelper.Columns = AllColumns;
						[self performSelectorOnMainThread:@selector(DownloadScores:) withObject:Task waitUntilDone:NO];
					}
					else
					{
						// now that we have the scores for the range on the ranked column, get all columns
						BlockHelper.Players = [NSMutableArray arrayWithCapacity:[Scores count]];
						for (GKScore* Score in Scores)
						{
							[BlockHelper.Players addObject:Score.playerID];
						}

						// now that we have some players, do player mode
						BlockHelper.ReadType = ROST_Players;
						BlockHelper.Columns = AllColumns;
						[self performSelectorOnMainThread:@selector(DownloadScores:) withObject:Task waitUntilDone:NO];
					}
				}
			}
		];
	}
}


/**
 * Show the matchmaker interface
 */
- (void)ShowMatchmaker:(IPhoneAsyncTask*)Task
{
	if (bHasStartedMatch)
	{
		debugf(NAME_DevOnline, TEXT("Started matchmaking, but a match has already started"));

		// send error callbacks
		Task.GameThreadCallback = ^ UBOOL (void)
		{
			debugf(NAME_DevOnline, TEXT("GameCenter::ShowMatchMaker::Failed"));

			// @todo: Useful return codes? the callback only gets success or failure, so doesn't matter
			FAsyncTaskDelegateResultsNamedSession Results(TEXT("Game"), -1);
			TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->JoinOnlineGameDelegates, &Results);
			TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->CreateOnlineGameDelegates, &Results);
						
			return TRUE;
		};

		// finish up the task
		[Task FinishedTask];
		return;
	}

	// cache the task
	check(MatchmakingTask == nil);
	MatchmakingTask = Task;

	// no match in progress
	self.CurrentMatch = nil;

	// make a basic request for what type of game we want
	GKMatchRequest* MatchRequest = Task.UserData;

	// create the matchmaker UI object
	GKMatchmakerViewController* Matchmaker = [[[GKMatchmakerViewController alloc] initWithMatchRequest:MatchRequest] autorelease];
	Matchmaker.matchmakerDelegate = self;
	
	// cache the matchmaker so we can close it later
	self.CurrentMatchmaker = Matchmaker;

	// show it
	[self ShowController:Matchmaker];
}

/**
 * Show the matchmaker interface due to an invite being accepted
 */
- (void)AcceptInvite:(IPhoneAsyncTask*)Task;
{
	// cache the task
	check(MatchmakingTask == nil);
	MatchmakingTask = Task;

	// create the matchmaker UI object for invite acceptance
	GKMatchmakerViewController* Matchmaker = [[[GKMatchmakerViewController alloc] initWithInvite:self.ReceivedInvite] autorelease];
	Matchmaker.matchmakerDelegate = self;

	// clear out our reference to the invite stuff
	self.ReceivedInvite = nil;
	self.ReceivedPlayersToInvite = nil;

	// no match in progress
	self.CurrentMatch = nil;

	// remember the controller so we can close it later
	self.CurrentMatchmaker = Matchmaker;

	// show it
	[self ShowController:Matchmaker];
}

/**
* Handle game being destroyed to play another game.
*/
- (void)DestroyOnlineGame:(IPhoneAsyncTask*)Task
{
		// trigger the EndOnlineGame callbacks
	Task.GameThreadCallback = ^ UBOOL (void)
	{
		debugf(NAME_DevOnline, TEXT("GameCenter::EndOnlineGame:: Callback"));

		FAsyncTaskDelegateResults Results(0);
		TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->DestroyOnlineGameCompleteDelegates, &Results);

		return TRUE;
	};
	[Task FinishedTask];

}

- (void)matchmakerViewController:(GKMatchmakerViewController*)Matchmaker didFailWithError:(NSError *)Error
{
	debugf(NAME_DevOnline, TEXT("Matchmaking failed with an error"));

	MatchmakingTask.GameThreadCallback = ^ UBOOL (void)
	{
		// @todo: Useful return codes? the callback only gets success or failure, so doesn't matter
		FAsyncTaskDelegateResultsNamedSession Results(TEXT("Game"), -1);
		TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->JoinOnlineGameDelegates, &Results);
		TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->CreateOnlineGameDelegates, &Results);

		return TRUE;
	};

	// finish up the task
	[MatchmakingTask FinishedTask];
	MatchmakingTask = nil;

	// close the view
	[self HideController:Matchmaker];
	self.CurrentMatchmaker = nil;
}

- (void)matchmakerViewController:(GKMatchmakerViewController*)Matchmaker didFindMatch:(GKMatch *)Match
{
	debugf(NAME_DevOnline, TEXT("Found a match with %d players (%d expected)\n"), [Match.playerIDs count], Match.expectedPlayerCount);

	// retain the match object
	self.CurrentMatch = Match;

	// receive match callbacks
	Match.delegate = self;

	// if everyone has already joined at this point (can easily happen with an invite), then
	// we are clear to start the match
	if (Match.expectedPlayerCount == 0 && !bHasStartedMatch)
	{
		debugf(NAME_DevOnline, TEXT("GameCenter::matchmakerViewController calling StartMatch"));
		[self StartMatch];
	}
}

- (void)matchmakerViewController:(GKMatchmakerViewController*)Matchmaker didFindPlayers:(NSArray *)PlayerIDs
{
	// @todo: This isn't used ATM because it's only for hosted matches
}

- (void)matchmakerViewControllerWasCancelled:(GKMatchmakerViewController*)Matchmaker
{
	MatchmakingTask.GameThreadCallback = ^ UBOOL (void)
	{
		debugf(NAME_DevOnline, TEXT("GameCenter::matchmakerViewControllerWasCancelled::Failed"));
		// @todo: Useful return codes? the callback only gets success or failure, so doesn't matter
		FAsyncTaskDelegateResultsNamedSession Results(TEXT("Game"), -1);
		TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->JoinOnlineGameDelegates, &Results);
		TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->CreateOnlineGameDelegates, &Results);

		return TRUE;
	};

	// finish up the task
	[MatchmakingTask FinishedTask];
	MatchmakingTask = nil;

	// close the view (immediately if we received an invite, since we need to show the invite 
	// UI right away, and sliding it off will keep the new one from sliding on)
	[self HideController:Matchmaker Animated:(self.ReceivedInvite == nil)];
	self.CurrentMatchmaker = nil;
}



- (void)match:(GKMatch *)match connectionWithPlayerFailed:(NSString *)playerID withError:(NSError *)error
{
	debugf(NAME_DevOnline, TEXT("Connection with another player failed..."));
}

- (void)match:(GKMatch *)match didFailWithError:(NSError *)error
{
	debugf(NAME_DevOnline, TEXT("Match failed..."));
}

/**
 * Received data from other players in the match
 */
- (void)match:(GKMatch*)Match didReceiveData:(NSData*)Data fromPlayer:(NSString *)PlayerID
{
	// pull the messages that came in on another thread
	@synchronized(GGameCenter.PendingMessages)
	{
		// keep the data around in the queue for game thread to pull off
		[GGameCenter.PendingMessages addObject:Data];
	}
}

- (void)match:(GKMatch *)Match player:(NSString *)PlayerID didChangeState:(GKPlayerConnectionState)State
{
	debugf(NAME_DevOnline, TEXT("A player has changed state, expected count is now %d"), Match.expectedPlayerCount);

	// This method is called when we are ready in an automatch game - self.CurrentMatchmaker != nil
	// it is also called when two players are playing and one accepts another invite - self.CurrentMatchmaker == nil
	if (self.CurrentMatchmaker != nil)
	{
		// when everyone joins, we are done with this phase 
		if (Match.expectedPlayerCount == 0 && !bHasStartedMatch)
		{
			debugf(NAME_DevOnline, TEXT("GameCenter::match calling StartMatch"));
			[self StartMatch];
		}
	}
	else
	{
		// This being the case, AcceptInvite will be called by PlayerController.uc::OnDestroyForInviteComplete()
	}
}


/**
 * Show the leaderboard interface
 */
- (void)ShowLeaderboard:(IPhoneAsyncTask*)Task
{
	// cache the task
	check(LeaderboardTask == nil);
	LeaderboardTask = Task;

	// create the leaderboard display object
	GKLeaderboardViewController* LeaderboardDisplay = [[[GKLeaderboardViewController alloc] init] autorelease];
	LeaderboardDisplay.leaderboardDelegate = self;

	// set which leaderboard to get
	LeaderboardDisplay.category = Task.UserData;

	// show it
	[self ShowController:LeaderboardDisplay];
}

- (void)leaderboardViewControllerDidFinish:(GKLeaderboardViewController*)LeaderboardDisplay
{
	// close the view
	[self HideController:LeaderboardDisplay];

	// fire delegates from game thread
	LeaderboardTask.GameThreadCallback = ^ UBOOL (void)
	{
		// get the SuppliedUI interface
		UOnlineSuppliedUIGameCenter* SuppliedUIInterface = Cast<UOnlineSuppliedUIGameCenter>(OnlineSubsystem->eventGetNamedInterface(FName("SuppliedUI")));
		TriggerOnlineDelegates(OnlineSubsystem, SuppliedUIInterface->ShowOnlineStatsUIDelegates, NULL);

		return TRUE;
	};

	// finish the task
	[LeaderboardTask FinishedTask];
	LeaderboardTask = nil;
}

/**
 * Find an achievement in the given array matching the given achievement (by identifier)
 *
 * @param SrcAchievement source achievement to find the matching achievement
 * @param Array array to look in
 *
 * @return the matching achievement or nil if one isn't found
 */
- (GKAchievement*)FindAchievementMatching:(GKAchievement*)SrcAchievement InArray:(NSArray*)Array
{
	// look for the matching local achievement
	for (GKAchievement* Achievement in Array)
	{
		// are they the same achievement?
		if ([SrcAchievement.identifier compare:Achievement.identifier] == NSOrderedSame)
		{
			return Achievement;
		}
	}

	return nil;
}

/**
 * Unlock an achievement
 */
- (void)UnlockAchievement:(IPhoneAsyncTask*)Task;
{
	GKAchievement* Achievement = Task.UserData;

	// look for the local achievement 
	GKAchievement* LocalAchievement = [self FindAchievementMatching:Achievement InArray:self.CachedAchievements];

	if (LocalAchievement)
	{
		// if the local one was less complete than it is now, update the cache
		if (LocalAchievement.percentComplete < Achievement.percentComplete)
		{
			LocalAchievement.percentComplete = Achievement.percentComplete;

			if (IPhoneCheckAchievementBannerSupported())
			{
				LocalAchievement.showsCompletionBanner = Achievement.showsCompletionBanner;
			}
		}
		// if local is the same or larger, then there's nothing to do here,
		else
		{
			// complete the task
			Task.GameThreadCallback = ^ UBOOL (void)
			{
				// no need to report an error here
				FAsyncTaskDelegateResults Results(0);
				TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->UnlockAchievementCompleteDelegates, &Results);

				return TRUE;
			};

			// finish the task
			[Task FinishedTask];

			// we're done
			return;
		}
	}
	else
	{
		// if the local version didn't even exist, then add it to the local set
		[self.CachedAchievements addObject:Achievement];
	}

	// if we've gotten here, the local cache was updated, so save it
	[self CacheAchievements];

	// handle skipping achievement upload if we are faking upload fail
	if (ParseParam(appCmdLine(), TEXT("fakeoffline")))
	{
		// complete the task
		Task.GameThreadCallback = ^ UBOOL (void)
		{
			// no need to report an error here, it's already handled by the local cache
			FAsyncTaskDelegateResults Results(0);
			TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->UnlockAchievementCompleteDelegates, &Results);

			return TRUE;
		};

		// finish the task
		[Task FinishedTask];
	}
	else
	{
		// achievement complete!
		[Achievement reportAchievementWithCompletionHandler: 
			^(NSError* Error)
			{
				debugf(NAME_DevOnline, TEXT("Finished unlocking achievement"));

				if(Error != nil)
				{
					// got an error, we'll try again later
					bDirtyAchievements = TRUE;
				}

				// complete the task
				Task.GameThreadCallback = ^ UBOOL (void)
				{
					// no need to report an error here, it's already handled by the local cache
					FAsyncTaskDelegateResults Results(0);
					TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->UnlockAchievementCompleteDelegates, &Results);

					return TRUE;
				};

				// finish the task
				[Task FinishedTask];
			}
		];
	}
}

/**
 * Loads the cached achievements from disk
 */
- (void)LoadCachedAchievements
{
	// make the filename
	NSString* DirName = [self GetArchiveDirectoryName];
	NSString* AchievementsStateFilename = [DirName stringByAppendingPathComponent:@"ach_state"];
	NSString* AchievementsDescFilename = [DirName stringByAppendingPathComponent:@"ach_desc"];

	// load it from disk (can be nil if never saved)
	self.CachedAchievements = [NSMutableArray arrayWithArray:[self LoadObjectFromFile:AchievementsStateFilename]];
	self.CachedAchievementDescriptions = [NSMutableArray arrayWithArray:[self LoadObjectFromFile:AchievementsDescFilename]];
}

/**
 * Write the current state of achievements to disk
 */
- (void)CacheAchievements
{
	// make the filename
	NSString* DirName = [self GetArchiveDirectoryName];
	NSString* AchievementsStateFilename = [DirName stringByAppendingPathComponent:@"ach_state"];
	NSString* AchievementsDescFilename = [DirName stringByAppendingPathComponent:@"ach_desc"];

	// write it to disk
	[self SaveObject:self.CachedAchievements ToFile:AchievementsStateFilename];
	[self SaveObject:self.CachedAchievementDescriptions ToFile:AchievementsDescFilename];
}

/**
 * Trigger ReadAchievementsCompleteDelegates using the given Task object
 */
- (void)FireAchievementCallbackOnTask:(IPhoneAsyncTask*)Task
{
	// fire off the callbacks
	Task.GameThreadCallback = ^ UBOOL (void)
	{
		OnlineSubsystemGameCenter_eventOnReadAchievementsComplete_Parms Parms(EC_EventParm);
		Parms.TitleId = 0;
		TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->ReadAchievementsCompleteDelegates, &Parms);
		return TRUE;
	};
	[Task FinishedTask];

}


/**
 * Kick off the read achievement operation, and figure out what to do with any 
 * differences against the locally cached achievements
 */
- (void)DownloadAchievementsAndResolveWithLocal
{
	// mark that the long process in the process
    ReadAchievementsStatus = OERS_InProgress;

	// assume this fixes the dirty achievements
	bDirtyAchievements = FALSE;

	// kickoff achievement download
	[GKAchievement loadAchievementsWithCompletionHandler:
		^(NSArray* RemoteAchievements, NSError* Error)
		{
			if (Error || ParseParam(appCmdLine(), TEXT("fakeoffline")))
			{
				// if we failed to read achievements, then we will have to rely on what has been 
				// previously cached, so there is nothing to do
			}
			// fill out our local copy of achievement info
			else
			{
				UBOOL bAchievementsAreDirty = FALSE;
				// if none were on the disk, then we the loaded achievements are the
				// only ones we know about, so use them directly
				if (self.CachedAchievements == nil)
				{
					self.CachedAchievements = [NSMutableArray arrayWithArray:RemoteAchievements];
					// mark we need to save them out
					bAchievementsAreDirty = TRUE;
				}
				else
				{
					// find all the matching achievements (local vs remote) and deal with 
					// any mismatches
					NSMutableArray* MergedAchievements = [NSMutableArray arrayWithCapacity:[RemoteAchievements count]];
					for (GKAchievement* RemoteAchievement in RemoteAchievements)
					{
						// look for an already cached local version of this achievement
						GKAchievement* LocalAchievement = [self FindAchievementMatching:RemoteAchievement InArray:self.CachedAchievements];
						if (LocalAchievement)
						{
							// if the local achievement is more unlocked than the remote (most likely just 100.0 vs 0.0),
							// then report it to the server
							if (LocalAchievement.percentComplete > RemoteAchievement.percentComplete)
							{
								debugf(NAME_DevOnline, TEXT("Resubmitting offline achievement %s"), *FString(LocalAchievement.identifier));
								[LocalAchievement reportAchievementWithCompletionHandler:
									^(NSError* Error)
									{
										if(Error != nil)
										{
											// got an error, we'll try again later
											bDirtyAchievements = TRUE;
										}
									
										// even if there's an error, it will be handled next time achievements
										// are downloaded, so there's nothing we need to do in this case
										// @todo: We could handle this a little better if we downloaded achievements
										// everytime the user logs in, but we may need to handle more synchronization 
										// with the game thread (might be trivial)
									}
								];
								
								// use local achievement as merged achievement
								[MergedAchievements addObject:LocalAchievement];
								bAchievementsAreDirty = TRUE;
							}
							// if the local is less unlocked, then replace local with remote
							else if (LocalAchievement.percentComplete < RemoteAchievement.percentComplete)
							{
								[MergedAchievements addObject:RemoteAchievement];
								bAchievementsAreDirty = TRUE;
							}
							// if they match, then use local, but don't mark dirty
							else
							{
								[MergedAchievements addObject:LocalAchievement];
							}
						}
						// if there was a remote achievement that didn't exist in the local list, then merge it in
						else
						{
							[MergedAchievements addObject:RemoteAchievement];
							bAchievementsAreDirty = TRUE;
						}
					}

					// now look for any local achievements that weren't in the remote list (unlocked first 
					// time while offline)
					for (GKAchievement* LocalAchievement in self.CachedAchievements)
					{
						GKAchievement* RemoteAchievement = [self FindAchievementMatching:LocalAchievement InArray:RemoteAchievements];
						// if it wasn't found, we need to merge it in
						if (RemoteAchievement == nil)
						{
							debugf(NAME_DevOnline, TEXT("Resubmitting offline achievement %s"), *FString(LocalAchievement.identifier));
							[LocalAchievement reportAchievementWithCompletionHandler:
								^(NSError* Error)
								{
									if(Error != nil)
									{
										// got an error, we'll try again later
										bDirtyAchievements = TRUE;
									}
								
									// even if there's an error, it will be handled next time achievements
									// are downloaded, so there's nothing we need to do in this case
									// @todo: We could handle this a little better if we downloaded achievements
									// everytime the user logs in, but we may need to handle more synchronization 
									// with the game thread (might be trivial)
								}
							];

							[MergedAchievements addObject:LocalAchievement];
							bAchievementsAreDirty = TRUE;
						}
					}

					// use the merged achievements from now on
					self.CachedAchievements = MergedAchievements;
				}

				// and save to disk if needed
				if (bAchievementsAreDirty)
				{
					[self CacheAchievements];
				}
			}
			
			// next download the descriptions
			[GKAchievementDescription loadAchievementDescriptionsWithCompletionHandler:
				^(NSArray* Descriptions, NSError* Error)
				{
					if (Error || ParseParam(appCmdLine(), TEXT("fakeoffline")))
					{
						// if we failed to read achievements, then we will have to rely on what has been 
						// previously cached, so there is nothing to do
					}
					// if there was no error, replace the cached ones with downloaded ones
					else
					{
						// cache the descriptions
						self.CachedAchievementDescriptions = Descriptions;

						// save them to disk in case next boot is offline
						[self CacheAchievements];
					}

					// mark that we are done downloading achievements
					@synchronized(self)
					{
						ReadAchievementsStatus = OERS_Done;

						if (self.ReadAchievementsTask)
						{
							// fire delegateas back on game thread
							[self FireAchievementCallbackOnTask:self.ReadAchievementsTask];

							// no longer need it
							self.ReadAchievementsTask = nil;
						}
					}
				}
			];
		}
	];
}

- (UBOOL)HasDirtyAchievementsToResolve
{
	return bDirtyAchievements && bIsAuthenticated && (ReadAchievementsStatus != OERS_InProgress);
}

- (EOnlineEnumerationReadState)GetAchievementsReadState
{
	return ReadAchievementsStatus;
}

/**
 * Show achievement UI
 */
- (void)ShowAchievements:(IPhoneAsyncTask*)Task
{
	// create the achievement display
	GKAchievementViewController* AchievementDisplay = [[[GKAchievementViewController alloc] init] autorelease];

	// register for callbacks
	AchievementDisplay.achievementDelegate = self;

	// show it
	[self ShowController:AchievementDisplay];
}

/**
 * Delegate function called when achievement UI closes
 */
- (void)achievementViewControllerDidFinish:(GKAchievementViewController*)AchievementDisplay
{
	// close the view
	[self HideController:AchievementDisplay];
}

/**
 * Make sure the achievements and their descriptions are ready to be used
 */
- (void)GetAchievements:(IPhoneAsyncTask*)Task
{
	// get the integer that shows what to download (text, images, both, or neither)
	NSInteger WhatToDownload = [Task.UserData integerValue];

	UBOOL bDownloadText = WhatToDownload & 1;
	UBOOL bDownloadImages = WhatToDownload & 2;

	checkf(!bDownloadImages, TEXT("Downloading images from GameCenter are currently not supported (quite non-trivial)"));
	
	// need to synchronize on an object, use self for now
	@synchronized(self)
	{
		// if we are currently downloading achievements, then atomically set the async 
		// task to be triggered when it's complete
		if (ReadAchievementsStatus == OERS_InProgress)
		{
			self.ReadAchievementsTask = Task;

			// nothing else to do
			return;
		}
	}

	// if the text isn't downloaded yet, kick it off
	if (bDownloadText && self.CachedAchievementDescriptions == nil)
	{
		[GKAchievementDescription loadAchievementDescriptionsWithCompletionHandler:
			^(NSArray* Descriptions, NSError* Error)
			{
				if (Error || ParseParam(appCmdLine(), TEXT("fakeoffline")))
				{
					// if we failed to read achievements, then we will have to rely on what has been 
					// previously cached, so there is nothing to do
				}
				else
				{
					// cache the descriptions
					self.CachedAchievementDescriptions = Descriptions;

					// save them to disk in case next boot is offline
					[self CacheAchievements];
				}

				// fire off the callbacks
				[self FireAchievementCallbackOnTask:Task];
			}
		];

		// nothing else to do, async task kicked off
		return;
	}
	
	// if we get here, then the achievements are already downloaded, so just fire the callbacks
	[self FireAchievementCallbackOnTask:Task];
}

/**
 * Ends the GKMatch
 */
- (void)EndOnlineGame:(IPhoneAsyncTask*)Task
{
	debugf(NAME_DevOnline, TEXT("GameCenter::EndOnlineGame:: Ended"));
	// we are no longer in the match, so we can start again
	bHasStartedMatch = FALSE;

	// docs say to stop the voice chat before calling disconnect on the match
	// so make sure it's shutdown if it wasn't already
	[self StopVoiceChat:nil];

	// shutdown the GKMatch
	[self.CurrentMatch disconnect];
	self.CurrentMatch = nil;

	// trigger the EndOnlineGame callbacks
	Task.GameThreadCallback = ^ UBOOL (void)
	{
		debugf(NAME_DevOnline, TEXT("GameCenter::EndOnlineGame:: Callback"));
		FAsyncTaskDelegateResults Results(0);
		TriggerOnlineDelegates(OnlineSubsystem, OnlineSubsystem->EndOnlineGameCompleteDelegates, &Results);

		return TRUE;
	};
	[Task FinishedTask];
}

@end



@implementation FReadOnlineStatsHelper
@synthesize ReadType;
@synthesize Players;
@synthesize Columns;
@synthesize RequestRange;

@end

#endif