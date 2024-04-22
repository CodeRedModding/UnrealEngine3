/*=============================================================================
	IPhoneAppDelegate.h: IPhone application class / main loop
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#import <Foundation/Foundation.h>
#import <UIKit/UIImage.h>
#import <GameKit/GKLocalPlayer.h>
#import <GameKit/GKMatchmakerViewController.h>
#import <GameKit/GKLeaderboardViewController.h>
#import <GameKit/GKMatch.h>
#import <GameKit/GKMatchmaker.h>
#import <GameKit/GKScore.h>
#import <GameKit/GKAchievement.h>
#import <GameKit/GKAchievementDescription.h>
#import <GameKit/GKAchievementViewController.h>
#import <GameKit/GKLeaderboard.h>

#import "IPhoneAsyncTask.h"

/**
 * @return GameCenter PlayerId (autoreleased) representation of the given FUniqueNetId
 */
NSString* NetIdToPlayerId(const FUniqueNetId& NetId);

/**
 * @return FUniqueNetID representation of the given GameCenter PlayerId
 */
FUniqueNetId PlayerIdToNetId(NSString* PlayerId);



@class IPhoneAppDelegate;

@interface FGameCenter : NSObject <GKMatchmakerViewControllerDelegate, 
	GKLeaderboardViewControllerDelegate, GKMatchDelegate, GKAchievementViewControllerDelegate>
{
@private
	/** Cache the application delegate object */
	IPhoneAppDelegate* AppDelegate;

	/** This is needed outside of the initial call to ShowMatchmaker, so cache it with this */
	IPhoneAsyncTask* MatchmakingTask;

	/** This is needed outside of the initial call to ShowLeaderboard, so cache it with this */
	IPhoneAsyncTask* LeaderboardTask;

	/** TRUE if the user has been authenticated (used to track changes to LocalPlayer authenticated) */
	UBOOL bIsAuthenticated; 

	/** TRUE if the user is currently in the authentication process */
	UBOOL bIsAuthenticating;

	/** TRUE after the GKMatch* has had all players join, and up until EndOnlineGame is called */
	UBOOL bHasStartedMatch;

	/** The current read state for achievements */
	EOnlineEnumerationReadState ReadAchievementsStatus;

	/** TRUE if we have dirty achievements that we need to try and resolve */
	UBOOL bDirtyAchievements;

	/** YES / NO objects used as values in the mutable array for currently talking players */
	NSNumber* YesNumber;
	NSNumber* NoNumber;
}


/** Values that we need to save from a received invite so game code can process the information and come back */
@property (retain) GKInvite* ReceivedInvite;
@property (retain) NSArray* ReceivedPlayersToInvite;

/** The current match (should be nil when no match has begun */
@property (retain) GKMatch* CurrentMatch;
@property (retain) GKMatchmakerViewController* CurrentMatchmaker;

/** The current voice chat object for the match (can be nil when VoiceChat fails, etc) */
@property (retain) GKVoiceChat* CurrentVoiceChat;

/** The queue of NSData objects received from the network */
@property (retain) NSMutableArray* PendingMessages;

/** The known, local, set of achievements (read from disk and network, and merged) */
@property (retain) NSMutableArray* CachedAchievements;

/** The list of achievements from the server */
@property (retain) NSArray* CachedAchievementDescriptions;

/** Cached copy of the OnlineSubsystem */
@property (assign) class UOnlineSubsystemGameCenter* OnlineSubsystem;

/** A task to call achievement delegates if a ReadAchievements request occurred while downloading */
@property (retain) IPhoneAsyncTask* ReadAchievementsTask;

/** A mapping of player ID to whether or not they are currently talking */
@property (retain) NSMutableArray* PlayersTalking;

/** 
 * Constructor
 */
- (id)init;

/**
 * Queues up a function to be called on the main thread. This will create an 
 * AsyncTask object for handling replies back to game thread on completion/failure
 *
 * @param Selector Function to run on main thread
 */
- (void)PerformTaskOnMainThread:(SEL)Selector;

/**
 * Queues up a function to be called on the main thread. This will create an 
 * AsyncTask object for handling replies back to game thread on completion/failure
 *
 * @param Selector Function to run on main thread
 * @param UserData Extra user data to send along to the main thread (will be retained via IPhoneAsyncTask property)
 */
- (void)PerformTaskOnMainThread:(SEL)Selector WithUserData:(id)UserData;

/**
 * Authenticate the user with GameCenter
 */
- (void)AuthenticateLocalUser:(IPhoneAsyncTask*)Task;

/**
 * Download friends list
 */
- (void)GetFriends:(IPhoneAsyncTask*)Task;

/**
 * Show the matchmaker interface
 */
- (void)ShowMatchmaker:(IPhoneAsyncTask*)Task;

/**
 * Show the matchmaker interface due to an invite being accepted
 */
- (void)AcceptInvite:(IPhoneAsyncTask*)Task;

/**
* Handle game being destroyed to play another game.
*/
- (void)DestroyOnlineGame:(IPhoneAsyncTask*)Task;

/**
 * Show the leaderboard interface
 */
- (void)ShowLeaderboard:(IPhoneAsyncTask*)Task;

/**
 * Upload some scores to a leaderboard
 */
- (void)UploadScores:(IPhoneAsyncTask*)Task;

/**
 * Download some scores from a leaderboard
 */
- (void)DownloadScores:(IPhoneAsyncTask*)Task;

/**
 * Unlock an achievement
 */
- (void)UnlockAchievement:(IPhoneAsyncTask*)Task;

/**
 * Show achievement UI
 */
- (void)ShowAchievements:(IPhoneAsyncTask*)Task;

/**
 * Make sure the achievements and their descriptions are ready to be used
 */
- (void)GetAchievements:(IPhoneAsyncTask*)Task;

/**
 * Ends the GKMatch
 */
- (void)EndOnlineGame:(IPhoneAsyncTask*)Task;

/**
 * Start voice chat on a given channel
 */
- (void)StartVoiceChat:(IPhoneAsyncTask*)Task;

/**
 * Stops voice chat
 */
- (void)StopVoiceChat:(IPhoneAsyncTask*)Task;

/**
 * Mutes the given player
 */
- (void)MutePlayer:(IPhoneAsyncTask*)Task;

/**
 * Unmutes the given player
 */
- (void)UnmutePlayer:(IPhoneAsyncTask*)Task;

/**
 * @return TRUE if the given Player is currently talking
 */
- (UBOOL)GameThreadIsPlayerTalking:(NSString*)PlayerId;

/**
 * Kick off the read achievement operation, and figure out what to do with any 
 * differences against the locally cached achievements
 */
- (void)DownloadAchievementsAndResolveWithLocal;

- (UBOOL)HasDirtyAchievementsToResolve;

/**
*Returns the current read state for the get achievements attribute
*/
- (EOnlineEnumerationReadState)GetAchievementsReadState;

@end


/**
 * An enum to describe how to download scores
 */
enum EReadOnlineStatsType
{
	// standard read of some columns for a set of players
	ROST_Players,
	// read of some columns for local user's friends
	ROST_Friends,
	// read of some columns for a range of players, where the range is based on the first column's ranking (2 stage operation)
	ROST_Range,
	// read of some columns for the players around the local player, based on first column's ranking (2 stage operation)
	ROST_AroundPlayer,
};


/**
 * A helper struct for holding information about ReadOnlineStats request(s)
 */
@interface FReadOnlineStatsHelper: NSObject 
{
}

/** What type of read operation will this be? */
@property (assign) EReadOnlineStatsType ReadType;

/** [Array of NSString*'s] Set of players to read stats for (can be nil if an initial read happens to get the player list) */
@property (retain) NSMutableArray* Players;

/** [Array of NSNumber's] Set of columns to read. Must be at least one entry */
@property (retain) NSArray* Columns;

/** The range of rows to read */
@property (assign) NSRange RequestRange;

@end

/** Global GameCenter singleton */
extern FGameCenter* GGameCenter;
