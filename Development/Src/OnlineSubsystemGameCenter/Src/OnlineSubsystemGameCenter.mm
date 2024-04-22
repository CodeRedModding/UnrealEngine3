/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "OnlineSubsystemGameCenter.h"

#if WITH_UE3_NETWORKING

#include "GameCenter.h"
#include "IPhoneObjCWrapper.h"

IMPLEMENT_CLASS(UOnlineSubsystemGameCenter);
IMPLEMENT_CLASS(UOnlineSuppliedUIGameCenter);

/**
 * GameCenter specific implementation. Sets the supported interface pointers
 *
 * @return always returns TRUE
 */

UBOOL UOnlineSubsystemGameCenter::Init(void)
{
	Super::Init();

	GGameCenter.OnlineSubsystem = this;

	// Set the various interfaces to this
	eventSetPlayerInterface(this);
	eventSetStatsInterface(this);
	eventSetGameInterface(this);
	eventSetSystemInterface(this);
	eventSetPlayerInterfaceEx(this);
	eventSetVoiceInterface(this);
	// Use web requests for downloading title files
	UOnlineTitleFileDownloadWeb* TitleFileObject = ConstructObject<UOnlineTitleFileDownloadWeb>(UOnlineTitleFileDownloadWeb::StaticClass(),this);
	TitleFileObject->eventInit();
	eventSetTitleFileInterface(TitleFileObject);
	// Add interface for caching downloaded files to disk
	UTitleFileDownloadCache* TitleFileCacheObject = ConstructObject<UTitleFileDownloadCache>(UTitleFileDownloadCache::StaticClass(),this);
	eventSetTitleFileCacheInterface(TitleFileCacheObject);
	// Use web requests for downloading user files
	UMcpUserCloudFileDownload* UserCloudFileDownload = ConstructObject<UMcpUserCloudFileDownload>(UMcpUserCloudFileDownload::StaticClass(),this);
	UserCloudFileDownload->eventInit();
	eventSetUserCloudInterface(UserCloudFileDownload);

	// Handle a missing profile data directory specification
	if (ProfileDataDirectory.Len() == 0)
	{
		ProfileDataDirectory = TEXT(".\\");
	}

	return TRUE;
}

/**
 * Displays the UI that prompts the user for their login credentials. Each
 * platform handles the authentication of the user's data.
 *
 * @param bShowOnlineOnly whether to only display online enabled profiles or not
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemGameCenter::ShowLoginUI(UBOOL bShowOnlineOnly)
{
	// route to the function iOS thread side function
	[GGameCenter PerformTaskOnMainThread:@selector(AuthenticateLocalUser:)];

	return TRUE;
}

/**
 * Logs the player into the online service. If this fails, it generates a
 * OnLoginFailed notification
 *
 * @param LocalUserNum the controller number of the associated user
 * @param LoginName the unique identifier for the player
 * @param Password the password for this account
 * @param bWantsLocalOnly whether the player wants to sign in locally only or not
 *
 * @return true if the async call started ok, false otherwise
 */
UBOOL UOnlineSubsystemGameCenter::Login(BYTE LocalUserNum,const FString& LoginName,const FString& Password,UBOOL bWantsLocalOnly)
{
	// we can't take a name or password, so just show the login UI with global params
	ShowLoginUI(FALSE);

	return TRUE;
}

/**
 * Starts an async task that retrieves the list of friends for the player from the
 * online service. The list can be retrieved in whole or in part.
 *
 * @param LocalUserNum the user to read the friends list of
 * @param Count the number of friends to read or zero for all
 * @param StartingAt the index of the friends list to start at (for pulling partial lists)
 *
 * @return true if the read request was issued successfully, false otherwise
 */
UBOOL UOnlineSubsystemGameCenter::ReadFriendsList(BYTE LocalUserNum, INT Count, INT StartingAt)
{
	ReadFriendsStatus = OERS_InProgress;

	// currently no support for reading partial list
	check(Count == 0 && StartingAt == 0);

	// route the function to iOS thread
	[GGameCenter PerformTaskOnMainThread:@selector(GetFriends:)];

	return TRUE;
}

/**
 * Copies the list of friends for the player previously retrieved from the online
 * service. The list can be retrieved in whole or in part.
 *
 * @param LocalUserNum the user to read the friends list of
 * @param Friends the out array that receives the copied data
 * @param Count the number of friends to read or zero for all
 * @param StartingAt the index of the friends list to start at (for pulling partial lists)
 *
 * @return OERS_Done if the read has completed, otherwise one of the other states
 */
BYTE UOnlineSubsystemGameCenter::GetFriendsList(BYTE LocalUserNum, TArray<FOnlineFriend>& Friends, INT Count, INT StartingAt)
{
	// @todo : Check for ReadFriendsList still happening
	if (ReadFriendsStatus == OERS_Done)
	{
		// Count of 0 means all
		if (Count == 0)
		{	
			Count = CachedFriends.Num();
		}

		// copy the friends list out into Friends
		for (INT FriendIndex = StartingAt; FriendIndex < Count && FriendIndex < CachedFriends.Num(); FriendIndex++)
		{
			Friends.AddItem(CachedFriends(FriendIndex));
		}
	}

	return ReadFriendsStatus;
}


/**
 * Writes out the stats contained within the stats write object to the online
 * subsystem's cache of stats data. Note the new data replaces the old. It does
 * not write the data to the permanent storage until a FlushOnlineStats() call
 * or a session ends. Stats cannot be written without a session or the write
 * request is ignored. No more than 5 stats views can be written to at a time
 * or the write request is ignored.
 *
 * @param SessionName the name of the session the stats are for
 * @param Player the player to write stats for
 * @param StatsWrite the object containing the information to write
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameCenter::WriteOnlineStats(FName SessionName, FUniqueNetId Player, UOnlineStatsWrite* StatsWrite)
{
	// must be logged in
	if (LoggedInPlayerName == TEXT(""))
	{
		return FALSE;
	}

	// can only send leaderboard scores for myself
	check(Player == LoggedInPlayerId);

	// create an array of scores to report
	NSMutableArray* ScoreArray = [NSMutableArray arrayWithCapacity:StatsWrite->ViewIds.Num()];

	// make a GKScore object for each score to report
	for (INT ScoreIndex = 0; ScoreIndex < StatsWrite->Properties.Num(); ScoreIndex++)
	{
		// get the Property for this score
		const FSettingsProperty& Property = StatsWrite->Properties(ScoreIndex);

		// get the GC-usable category name
		FString CategoryName = GetCategoryNameFromPropertyId(Property.PropertyId);

		// create a score object to pass to iOS thread to publish
		GKScore* Score = [[GKScore alloc] initWithCategory:
			[NSString stringWithUTF8String:TCHAR_TO_ANSI(*CategoryName)]];

		// put the score into the array
		[ScoreArray addObject:Score];

		// array is now the owner of the score
		[Score release];

		// only support integers and floats, which we'll assume are in hundredths of seconds (is this right?)
		check(Property.Data.Type == SDT_Int32 || Property.Data.Type == SDT_Int64 || Property.Data.Type == SDT_Float);

		// get the value from the StatsWrite object
		if (Property.Data.Type == SDT_Int32)
		{
			INT Value;
			Property.Data.GetData(Value);
			Score.value = (int64_t)Value;
		}
		else if (Property.Data.Type == SDT_Int64)
		{
			QWORD Value;
			Property.Data.GetData(Value);
			Score.value = Value;
		}
		else if (Property.Data.Type == SDT_Float)
		{
			FLOAT Value;
			Property.Data.GetData(Value);

			// value will be in hundredths of a second (any way to query this?)
			Score.value = (int64_t)(Value * 100.0f);
		}

		debugf(NAME_DevOnline, TEXT("Writing score %lld to category %s"), Score.value, *CategoryName);
	}

	// route the function to iOS thread 
	[GGameCenter PerformTaskOnMainThread:@selector(UploadScores:) WithUserData:ScoreArray];

	return TRUE;
}

/**
 * Reads a set of stats for the specified list of players
 *
 * @param Players the array of unique ids to read stats for
 * @param StatsRead holds the definitions of the tables to read the data from and
 *		  results are copied into the specified object
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameCenter::ReadOnlineStats(const TArray<FUniqueNetId>& Players, UOnlineStatsRead* StatsRead)
{
	if (CurrentStatsRead != NULL)
	{
		debugf(NAME_DevOnline, TEXT("Can't perform multiple ReadOnlineStats at once!"));
		return FALSE;
	}

	// cache the stats read object (we will return results to it)
	CurrentStatsRead = StatsRead;

	checkf(Players.Num() == 1 && Players(0) == LoggedInPlayerId, TEXT("Game Center currently only supports reading stats for the logged in player"));

	// make an array of column names to request
	NSMutableArray* ColumnsArray = [NSMutableArray arrayWithCapacity:StatsRead->ColumnIds.Num()];
	for (INT Column = 0; Column < StatsRead->ColumnIds.Num(); Column++)
	{
		FString ColumnName = GetCategoryNameFromPropertyId(StatsRead->ColumnIds(Column));
		[ColumnsArray addObject:[NSString stringWithUTF8String:TCHAR_TO_ANSI(*ColumnName)]];
	}

	// make an array of playerIds
	NSMutableArray* PlayersArray = [NSMutableArray arrayWithCapacity:Players.Num()];
	for (INT PlayerIndex = 0; PlayerIndex < Players.Num(); PlayerIndex++)
	{
		// @todo: Turn a FUniqueNetId into an NSString player Id
		[PlayersArray addObject:[GKLocalPlayer localPlayer].playerID];
	}

	// fill out the helper struct to send as the one param to the main thread
	FReadOnlineStatsHelper* Helper = [[FReadOnlineStatsHelper alloc] init];
	Helper.Columns = ColumnsArray;
	Helper.Players = PlayersArray;
	Helper.ReadType = ROST_Players;

	// have main thread perform all the action
	[GGameCenter PerformTaskOnMainThread:@selector(DownloadScores:) WithUserData:Helper];

	// task is now the owner
	[Helper release];

	return TRUE;
}

/**
 * Reads a player's stats and all of that player's friends stats for the
 * specified set of stat views. This allows you to easily compare a player's
 * stats to their friends.
 *
 * @param LocalUserNum the local player having their stats and friend's stats read for
 * @param StatsRead holds the definitions of the tables to read the data from and
 *		  results are copied into the specified object
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameCenter::ReadOnlineStatsForFriends(BYTE LocalUserNum, UOnlineStatsRead* StatsRead)
{
	if (CurrentStatsRead != NULL)
	{
		debugf(NAME_DevOnline, TEXT("Can't perform multiple ReadOnlineStats at once!"));
		return FALSE;
	}

	// cache the stats read object (we will return results to it)
	CurrentStatsRead = StatsRead;

	// make an array of column names to request
	NSMutableArray* ColumnsArray = [NSMutableArray arrayWithCapacity:StatsRead->ColumnIds.Num()];
	for (INT Column = 0; Column < StatsRead->ColumnIds.Num(); Column++)
	{
		FString ColumnName = GetCategoryNameFromPropertyId(StatsRead->ColumnIds(Column));
		[ColumnsArray addObject:[NSString stringWithUTF8String:TCHAR_TO_ANSI(*ColumnName)]];
	}

	// fill out the helper struct to send as the one param to the main thread
	FReadOnlineStatsHelper* Helper = [[FReadOnlineStatsHelper alloc] init];
	Helper.Columns = ColumnsArray;
	Helper.ReadType = ROST_Friends;

	// have main thread perform all the action
	[GGameCenter PerformTaskOnMainThread:@selector(DownloadScores:) WithUserData:Helper];

	// task is now the owner
	[Helper release];

	return TRUE;
}

/**
 * Reads stats by ranking. This grabs the rows starting at StartIndex through
 * NumToRead and places them in the StatsRead object.
 *
 * @param StatsRead holds the definitions of the tables to read the data from and
 *		  results are copied into the specified object
 * @param StartIndex the starting rank to begin reads at (1 for top)
 * @param NumToRead the number of rows to read (clamped at 100 underneath)
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameCenter::ReadOnlineStatsByRank(UOnlineStatsRead* StatsRead, INT StartIndex, INT NumToRead)
{
	if (CurrentStatsRead != NULL)
	{
		debugf(NAME_DevOnline, TEXT("Can't perform multiple ReadOnlineStats at once!"));
		return FALSE;
	}

	if (StartIndex == 0)
	{
		debugf(NAME_DevOnline, TEXT("Can't start reading leaderboard at Rank 0, changing to 1"));
		StartIndex = 1;
	}

	// cache the stats read object (we will return results to it)
	CurrentStatsRead = StatsRead;

	// make an array of column names to request
	NSMutableArray* ColumnsArray = [NSMutableArray arrayWithCapacity:StatsRead->ColumnIds.Num()];
	for (INT Column = 0; Column < StatsRead->ColumnIds.Num(); Column++)
	{
		FString ColumnName = GetCategoryNameFromPropertyId(StatsRead->ColumnIds(Column));
		[ColumnsArray addObject:[NSString stringWithUTF8String:TCHAR_TO_ANSI(*ColumnName)]];
	}

	// fill out the helper struct to send as the one param to the main thread
	FReadOnlineStatsHelper* Helper = [[FReadOnlineStatsHelper alloc] init];
	Helper.Columns = ColumnsArray;
	Helper.ReadType = ROST_Range;
	Helper.RequestRange = NSMakeRange(StartIndex, NumToRead);

	// have main thread perform all the action
	[GGameCenter PerformTaskOnMainThread:@selector(DownloadScores:) WithUserData:Helper];

	// task is now the owner
	[Helper release];

	return TRUE;
}

/**
 * Reads stats by ranking centered around a player. This grabs a set of rows
 * above and below the player's current rank
 *
 * @param LocalUserNum the local player having their stats being centered upon
 * @param StatsRead holds the definitions of the tables to read the data from and
 *		  results are copied into the specified object
 * @param NumRows the number of rows to read above and below the player's rank
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameCenter::ReadOnlineStatsByRankAroundPlayer(BYTE LocalUserNum, UOnlineStatsRead* StatsRead, INT NumRows)
{
	if (CurrentStatsRead != NULL)
	{
		debugf(NAME_DevOnline, TEXT("Can't perform multiple ReadOnlineStats at once!"));
		return FALSE;
	}

	// cache the stats read object (we will return results to it)
	CurrentStatsRead = StatsRead;

	// make an array of column names to request
	NSMutableArray* ColumnsArray = [NSMutableArray arrayWithCapacity:StatsRead->ColumnIds.Num()];
	for (INT Column = 0; Column < StatsRead->ColumnIds.Num(); Column++)
	{
		FString ColumnName = GetCategoryNameFromPropertyId(StatsRead->ColumnIds(Column));
		[ColumnsArray addObject:[NSString stringWithUTF8String:TCHAR_TO_ANSI(*ColumnName)]];
	}

	// fill out the helper struct to send as the one param to the main thread
	FReadOnlineStatsHelper* Helper = [[FReadOnlineStatsHelper alloc] init];
	Helper.Columns = ColumnsArray;
	Helper.ReadType = ROST_AroundPlayer;
	// the actual range will be fixed up later, just storing the amount requested in the range
	Helper.RequestRange = NSMakeRange(1, NumRows);

	// have main thread perform all the action
	[GGameCenter PerformTaskOnMainThread:@selector(DownloadScores:) WithUserData:Helper];

	// task is now the owner
	[Helper release];

	return TRUE;
}

/**
 * Cleans up the Live specific session data contained in the search results
 *
 * @param Search the object to free the previous results from
 *
 * @return TRUE if it could, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameCenter::FreeSearchResults(UOnlineGameSearch* Search)
{
	UBOOL bDidFree = FALSE;
	// If they didn't pass on object in, they meant for us to use the current one
	if (Search == NULL)
	{
		Search = GameSearch;
	}
	if (Search != NULL)
	{
		if (Search->bIsSearchInProgress == FALSE)
		{
			// free the pointer platform data associated with the result
			for (INT Index = 0; Index < Search->Results.Num(); Index++)
			{
				FOnlineGameSearchResult& Result = Search->Results(Index);

				// Free the data and clear the leak detection flag
				delete (FSessionInfo*)Result.PlatformData;
			}
			Search->Results.Empty();
			bDidFree = TRUE;
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Can't free search results while the search is in progress"));
		}
	}
	return bDidFree;
}


/**
 * Tells the online subsystem to accept the game invite that is currently pending
 *
 * @param LocalUserNum the local user accepting the invite
 * @param SessionName the name of the session this invite is to be known as
 *
 * @return true if the game invite was able to be accepted, false otherwise
 */
UBOOL UOnlineSubsystemGameCenter::AcceptGameInvite(BYTE LocalUserNum, FName SessionName)
{
	// route the function to iOS thread
	[GGameCenter PerformTaskOnMainThread:@selector(AcceptInvite:)];

	return TRUE;
}

/**
 * Destroys the current online game
 * NOTE: online game de-registration is an async process and does not complete
 * until the OnDestroyOnlineGameComplete delegate is called.
 *
 * @param SessionName the name of the session to delete
 *
 * @return true if successful destroying the session, false otherwsie
 */
UBOOL UOnlineSubsystemGameCenter::DestroyOnlineGame(FName SessionName)
{
	// route the function to iOS thread
	[GGameCenter PerformTaskOnMainThread:@selector(DestroyOnlineGame:)];

	return TRUE;
}

/**
 * Returns the platform specific connection information for joining the match.
 * Call this function from the delegate of join completion
 *
 * @param SessionName the name of the session to fetch the connection information for
 * @param ConnectInfo the out var containing the platform specific connection information
 *
 * @return true if the call was successful, false otherwise
 */
UBOOL UOnlineSubsystemGameCenter::GetResolvedConnectString(FName SessionName, FString& ConnectInfo)
{
	// a None session name was a server, so we don't want the default PLayerController implementation
	// to try to join a server, so we return false
	if (SessionName == NAME_None)
	{
		return FALSE;
	}

	if (GameSearch == NULL)
	{
		return FALSE;
	}

	// There have been cases where this is valid to happen
	TEXT("GetResolvedConnectString: The GameSearch object has returned 0 results.");

	// dummy connect string, it doesn't matter what it is, as long as it resolves to a pending level connection
	ConnectInfo = TEXT("127.0.0.1");

	return TRUE;
}

/**
 * Unlocks the specified achievement for the specified user
 *
 * @param LocalUserNum the controller number of the associated user
 * @param AchievementId the id of the achievement to unlock
 *
 * @return TRUE if the call worked, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameCenter::UnlockAchievement(BYTE LocalUserNum,INT AchievementId,FLOAT PercentComplete)
{
	// figure out the GC name for the achievement
	FString AchievementName = GetAchievementNameFromId(AchievementId);

	debugf(NAME_DevOnline, TEXT("Unlocking achievement %s"), *AchievementName);

	// make an achievement object
	GKAchievement* Achievement = [[GKAchievement alloc] initWithIdentifier:
		[NSString stringWithUTF8String:TCHAR_TO_ANSI(*AchievementName)]];

	// we only support fully unlocking at this time
	Achievement.percentComplete = PercentComplete;

	if( IPhoneCheckAchievementBannerSupported() )
	{
		UBOOL bShowAchievementBanner = FALSE;
		GConfig->GetBool(TEXT("SystemSettingsIPhone"), TEXT("bShowAchievementBannerOnComplete"), bShowAchievementBanner, GSystemSettingsIni);

		if( PercentComplete >= 100.0f && bShowAchievementBanner == TRUE )
		{
			Achievement.showsCompletionBanner = YES;
		}
	}

	// route the function to iOS thread (releasing out retain on achievement, since we're no longer "owner")
	[GGameCenter PerformTaskOnMainThread:@selector(UnlockAchievement:) WithUserData:Achievement];
	[Achievement release];

	return TRUE;
}

/**
 * Starts an async read for the achievement list
 *
 * @param LocalUserNum the controller number of the associated user
 * @param TitleId the title id of the game the achievements are to be read for
 * @param bShouldReadText whether to fetch the text strings or not
 * @param bShouldReadImages whether to fetch the image data or not
 *
 * @return TRUE if the task starts, FALSE if it failed
 */
UBOOL UOnlineSubsystemGameCenter::ReadAchievements(BYTE LocalUserNum, INT TitleId, UBOOL bShouldReadText, UBOOL bShouldReadImages)
{
	if (TitleId != 0)
	{
		debugf(NAME_DevOnline, TEXT("UOnlineSubsystemGameCenter only supports reading this game's achievements (use 0 for TitleId)"));
		return FALSE;
	}

	debugf(NAME_DevOnline, TEXT("Reading achievements"));

	// set up an object with a number that will tell the other side what to do (bit 1 = text, bit 2 = images)
	NSNumber* ControlNumber = [NSNumber numberWithInt:(bShouldReadText ? 1 : 0) | (bShouldReadImages ? 2 : 0)];

	// allow GGameCenter to download achievements if needed
	[GGameCenter PerformTaskOnMainThread:@selector(GetAchievements:) WithUserData:ControlNumber];

	return TRUE;
}

/**
 * Copies the list of achievements for the specified player and title id
 *
 * @param LocalUserNum the user to read the friends list of
 * @param Achievements the out array that receives the copied data
 * @param TitleId the title id of the game that these were read for
 *
 * @return OERS_Done if the read has completed, otherwise one of the other states
 */
BYTE UOnlineSubsystemGameCenter::GetAchievements(BYTE LocalUserNum, TArray<FAchievementDetails>& Achievements, INT TitleId)
{
	@synchronized(GGameCenter)
	{
		Achievements.AddZeroed([GGameCenter.CachedAchievementDescriptions count]);

		// extract all info we can
		INT AchievementIndex = 0;
		for (GKAchievementDescription* GCAchievement in GGameCenter.CachedAchievementDescriptions)
		{
			FAchievementDetails& Achievement = Achievements(AchievementIndex++);
			// convert string name to UE3 identifier
			Achievement.Id = GetAchievementIdFromName(FString(GCAchievement.identifier));
			Achievement.AchievementName = FString(GCAchievement.title);
			Achievement.HowTo = FString(GCAchievement.unachievedDescription);
			Achievement.Description = FString(GCAchievement.achievedDescription);
			Achievement.GamerPoints = GCAchievement.maximumPoints;
			Achievement.bIsSecret = GCAchievement.hidden ? TRUE : FALSE;

			// look for the state in the cached achievements to see if unlocked
			for (GKAchievement* AchievementState in GGameCenter.CachedAchievements)
			{
				if ([AchievementState.identifier compare:GCAchievement.identifier] == NSOrderedSame)
				{
					// if the achievement is fully complete, we have achieved it!
					Achievement.bWasAchievedOnline = AchievementState.percentComplete == 100.0;
					break;
				}
			}
		}
	}
	return [GGameCenter GetAchievementsReadState];
}

/**
 * Displays the achievements UI for a player
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemGameCenter::ShowAchievementsUI(BYTE LocalUserNum)
{
	// route the function to iOS thread
	[GGameCenter PerformTaskOnMainThread:@selector(ShowAchievements:)];

	return TRUE;
}


/**
 * Tells GameCenter that the session has ended
 *
 * @param SessionName the name of the session to end
 *
 * @return TRUE if the call succeeds, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameCenter::EndOnlineGame(FName SessionName)
{
	// route the function to iOS thread
	[GGameCenter PerformTaskOnMainThread:@selector(EndOnlineGame:)];

	return TRUE;
}

/**
 * Tells the voice layer that networked processing of the voice data is allowed
 * for the specified player. This allows for push-to-talk style voice communication
 *
 * @param LocalUserNum the local user to allow network transimission for
 */
void UOnlineSubsystemGameCenter::StartNetworkedVoice(BYTE LocalUserNum)
{
	// tell main thread to startup voice chat
	[GGameCenter PerformTaskOnMainThread:@selector(StartVoiceChat:) WithUserData:@"Game"];
}

/**
 * Tells the voice layer to stop processing networked voice support for the
 * specified player. This allows for push-to-talk style voice communication
 *
 * @param LocalUserNum the local user to disallow network transimission for
 */
void UOnlineSubsystemGameCenter::StopNetworkedVoice(BYTE LocalUserNum)
{
	// tell main to stop voice chat
	[GGameCenter PerformTaskOnMainThread:@selector(StopVoiceChat:)];
}

/**
 * Mutes a remote talker for the specified local player. NOTE: This only mutes them in the
 * game unless the bIsSystemWide flag is true, which attempts to mute them globally
 *
 * @param LocalUserNum the user that is muting the remote talker
 * @param PlayerId the remote talker that is being muted
 * @param bIsSystemWide whether to try to mute them globally or not
 *
 * @return TRUE if the function succeeds, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameCenter::MuteRemoteTalker(BYTE LocalUserNum, FUniqueNetId PlayerId, UBOOL bIsSystemWide)
{
	NSString* GCPlayerId = NetIdToPlayerId(PlayerId);
	[GGameCenter PerformTaskOnMainThread:@selector(MutePlayer:) WithUserData:GCPlayerId];

	return TRUE;
}

/**
 * Allows a remote talker to talk to the specified local player. NOTE: This only unmutes them in the
 * game unless the bIsSystemWide flag is true, which attempts to unmute them globally
 *
 * @param LocalUserNum the user that is allowing the remote talker to talk
 * @param PlayerId the remote talker that is being restored to talking
 * @param bIsSystemWide whether to try to unmute them globally or not
 *
 * @return TRUE if the function succeeds, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameCenter::UnmuteRemoteTalker(BYTE LocalUserNum, FUniqueNetId PlayerId, UBOOL bIsSystemWide)
{
	NSString* GCPlayerId = NetIdToPlayerId(PlayerId);
	[GGameCenter PerformTaskOnMainThread:@selector(UnmutePlayer:) WithUserData:GCPlayerId];

	return TRUE;
}

/**
 * Determines if the specified player is actively talking into the mic
 *
 * @param LocalUserNum the local player index being queried
 *
 * @return TRUE if the player is talking, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameCenter::IsLocalPlayerTalking(BYTE LocalUserNum)
{
	// ask game center if the local player (nil for ID) is currently talking 
	return [GGameCenter GameThreadIsPlayerTalking:nil];
}

/**
 * Determines if the specified remote player is actively talking into the mic
 * NOTE: Network latencies will make this not 100% accurate
 *
 * @param PlayerId the unique id of the remote player being queried
 *
 * @return TRUE if the player is talking, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameCenter::IsRemotePlayerTalking(FUniqueNetId PlayerId)
{
	NSString* GCPlayerId = NetIdToPlayerId(PlayerId);

	// ask game center if the remote player is talking
	return [GGameCenter GameThreadIsPlayerTalking:GCPlayerId];
}







/**
 * Handles updating of any async tasks that need to be performed
 *
 * @param DeltaTime the amount of time that has passed since the last tick
 */
void UOnlineSubsystemGameCenter::Tick(FLOAT DeltaTime)
{
	// check every 30s to see if our connection has changed and if we need to push dirty achievements
	CheckForConnectionTime += DeltaTime;
	if(CheckForConnectionTime > 30.0f)
	{
		if([GGameCenter HasDirtyAchievementsToResolve])
		{
			UBOOL bOnline = IPhoneIsNetConnected();
			if(bOnline && !bWasOnline)
			{
				// yeah, time to try to push out the dirty achievements
				[GGameCenter DownloadAchievementsAndResolveWithLocal];
			}

			// setup for next time
			bWasOnline = bOnline;
		}
		
		CheckForConnectionTime = 0.0f;
	}
}


/** 
 * Convert a property id (aka category) into the category name that should
 * match the name specified in iTunes Connect (see UniqueCategoryPrefix)
 *
 * @param PropertyId Identifier for the property to write
 *
 * @return String name of a category
 */
FString UOnlineSubsystemGameCenter::GetCategoryNameFromPropertyId(INT PropertyId)
{
	return FString::Printf(TEXT("%s%02d"), *UniqueCategoryPrefix, PropertyId);
}

/** 
 * Convert an iTunes Connect category name into the UE3 identifier (see
 * UniqueCategoryPrefix)
 *
 * @param CategoryName iTunes Connect catgegory name
 *
 * @return UE3 property id, or -1 if the name is bad
 */
INT UOnlineSubsystemGameCenter::GetPropertyIdFromCategoryName(const FString& CategoryName)
{
	const FString& Prefix = UniqueCategoryPrefix;

	// the name should be prefix followed by a 2 character integer (see GetCategoryNameFromPropertyId)
	if (CategoryName.StartsWith(Prefix) && CategoryName.Len() == Prefix.Len() + 2)
	{
		// convert the bit after the prefix to an integer
		return appAtoi(*CategoryName.Mid(Prefix.Len()));
	}

	// -1 is error code
	return -1;
}

/** 
 * Convert an achievemt identifier into the achievement name that should
 * match the name specified in iTunes Connect (see UniqueAchievementPrefix)
 *
 * @param AchievementId UE3 achievement id
 *
 * @return String name of a achievement 
 */
FString UOnlineSubsystemGameCenter::GetAchievementNameFromId(INT AchievementId)
{
	return FString::Printf(TEXT("%s%02d"), *UniqueAchievementPrefix, AchievementId);
}

/** 
 * Convert an iTunes Connect achievement name into the UE3 identifier (see
 * UniqueAchievementPrefix)
 *
 * @param AchievementName iTunes Connect achievement name
 *
 * @return UE3 achievement id
 */
INT UOnlineSubsystemGameCenter::GetAchievementIdFromName(const FString& AchievementName)
{
	const FString& Prefix = UniqueAchievementPrefix;

	// the name should be prefix followed by a 2 character integer (see GetAchievementNameFromId)
	if (AchievementName.StartsWith(Prefix) && AchievementName.Len() == Prefix.Len() + 2)
	{
		// convert the bit after the prefix to an integer
		return appAtoi(*AchievementName.Mid(Prefix.Len()));
	}

	// -1 is error code
	return -1;
}











/**
 * Determines whether the user's profile file exists or not
 */
UBOOL UOnlineSubsystemGameCenter::DoesProfileExist(void)
{
	// attempt to open the file
	FArchive* ProfileAr = GFileManager->CreateFileReader(*CreateProfileName(), FILEREAD_SaveGame);

	// make sure the file exists
	UBOOL bExists = ProfileAr && ProfileAr->TotalSize() != INDEX_NONE;

	// close and return
	delete ProfileAr;
	return bExists;
}

/**
 * Reads the online profile settings for a given user from disk. If the file
 * exists, an async task is used to verify the file wasn't hacked and to
 * decompress the contents into a buffer. Once the task
 *
 * @param LocalUserNum the user that we are reading the data for
 * @param ProfileSettings the object to copy the results to and contains the list of items to read
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineSubsystemGameCenter::ReadProfileSettings(BYTE LocalUserNum,
	UOnlineProfileSettings* ProfileSettings)
{
	DWORD Return = E_FAIL;
	// Only read if we don't have a profile for this player
	if (CachedProfile == NULL)
	{
		if (ProfileSettings != NULL)
		{
			CachedProfile = ProfileSettings;
			// Don't bother reading if they haven't saved it before
			if (DoesProfileExist())
			{
				CachedProfile->AsyncState = OPAS_Read;
				// Clear the previous set of results
				CachedProfile->ProfileSettings.Empty();
				TArray<BYTE> Buffer;
				// Load the profile into a byte array
				if (appLoadFileToArray(Buffer,*CreateProfileName(), GFileManager, FILEREAD_SaveGame))
				{
					FProfileSettingsReader Reader(64 * 1024,TRUE,Buffer.GetTypedData(),Buffer.Num());
					// Serialize the profile from that array
					if (Reader.SerializeFromBuffer(CachedProfile->ProfileSettings))
					{
						INT ReadVersion = CachedProfile->GetVersionNumber();
						// Check the version number and reset to defaults if they don't match
						if (CachedProfile->VersionNumber != ReadVersion)
						{
							debugf(NAME_DevOnline,
								TEXT("Detected profile version mismatch (%d != %d), setting to defaults"),
								CachedProfile->VersionNumber,
								ReadVersion);
							CachedProfile->SetToDefaults();
						}
						CachedProfile->AsyncState = OPAS_Finished;
						Return = S_OK;
					}
					else
					{
						debugf(NAME_DevOnline,
							TEXT("Profile data for %s was corrupt, using defaults"),
							*LoggedInPlayerName);
						CachedProfile->SetToDefaults();
						Return = S_OK;
					}
				}
			}
			else
			{
				debugf(NAME_DevOnline,
					TEXT("Profile for %s doesn't exist, using defaults"),
					*LoggedInPlayerName);
				CachedProfile->SetToDefaults();
				Return = S_OK;
			}
			if (Return != S_OK && Return != ERROR_IO_PENDING)
			{
				debugf(NAME_DevOnline,
					TEXT("Unable to read the profile for %s, using defaults"),
					*LoggedInPlayerName);
				CachedProfile->SetToDefaults();
				CachedProfile->AsyncState = OPAS_Finished;
			}
		}
		else
		{
			debugf(NAME_Error,TEXT("Can't specify a null profile settings object"));
		}
	}
	// Make sure the profile isn't already being read, since this is going to
	// complete immediately
	else if (CachedProfile->AsyncState != OPAS_Read)
	{
		debugf(NAME_DevOnline,TEXT("Using cached profile data instead of reading"));
		// If the specified read isn't the same as the cached object, copy the
		// data from the cache
		if (CachedProfile != ProfileSettings)
		{
			ProfileSettings->ProfileSettings = CachedProfile->ProfileSettings;
		}
		Return = S_OK;
	}
	else
	{
		debugf(NAME_Error,TEXT("Profile read for player (%d) is already in progress"),LocalUserNum);
	}
	// Trigger the delegates if there are any registered
	if (Return != ERROR_IO_PENDING)
	{
		FAsyncTaskDelegateResults Params(Return);
		TriggerOnlineDelegates(this,ReadProfileSettingsDelegates,&Params);
	}
	return Return == S_OK || Return == ERROR_IO_PENDING;
}

/**
 * Writes the online profile settings for a given user Live using an async task
 *
 * @param LocalUserNum the user that we are writing the data for
 * @param ProfileSettings the list of settings to write out
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineSubsystemGameCenter::WriteProfileSettings(BYTE LocalUserNum,
	UOnlineProfileSettings* ProfileSettings)
{
	DWORD Return = E_FAIL;
	// Don't allow a write if there is a task already in progress
	if (CachedProfile == NULL ||
		(CachedProfile->AsyncState != OPAS_Read && CachedProfile->AsyncState != OPAS_Write))
	{
		if (ProfileSettings != NULL)
		{
			// Cache to make sure GC doesn't collect this while we are waiting
			// for the task to complete
			CachedProfile = ProfileSettings;
			// Mark this as a write in progress
			CachedProfile->AsyncState = OPAS_Write;
			// Make sure the profile settings have a version number
			CachedProfile->AppendVersionToSettings();
			// Used to write the profile settings into a blob
			FProfileSettingsWriter Writer(64 * 1024,TRUE);
			if (Writer.SerializeToBuffer(CachedProfile->ProfileSettings))
			{
				// Write the file to disk
				FArchive* FileWriter = GFileManager->CreateFileWriter(*CreateProfileName(), FILEWRITE_SaveGame);
				if (FileWriter)
				{
					FileWriter->Serialize((void*)Writer.GetFinalBuffer(),Writer.GetFinalBufferLength());
					delete FileWriter;
				}
				// Remove the write state so that subsequent writes work
				CachedProfile->AsyncState = OPAS_Finished;
				Return = S_OK;
			}
			if (Return != S_OK && Return != ERROR_IO_PENDING)
			{
				debugf(NAME_DevOnline,TEXT("Failed to write profile data for %s"),*LoggedInPlayerName);
				// Remove the write state so that subsequent writes work
				CachedProfile->AsyncState = OPAS_Finished;
			}
		}
		else
		{
			debugf(NAME_Error,TEXT("Can't write a null profile settings object"));
		}
	}
	else
	{
		debugf(NAME_Error,
			TEXT("Can't write profile as an async profile task is already in progress for player (%d)"),
			LocalUserNum);
	}
	if (Return != ERROR_IO_PENDING)
	{
		// Remove the write state so that subsequent writes work
		CachedProfile->AsyncState = OPAS_Finished;
		// Send the notification of completion
		FAsyncTaskDelegateResults Params(Return);
		TriggerOnlineDelegates(this,WriteProfileSettingsDelegates,&Params);
	}
	return Return == S_OK || Return == ERROR_IO_PENDING;
}





/**
 * Shows the platform supplid leaderboard UI
 *
 * @param Players the array of unique ids to show stats for
 * @param StatsRead holds the definitions of the tables to show the data for
 *		  (note that no results will be filled out)
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSuppliedUIGameCenter::ShowOnlineStatsUI(const TArray<FUniqueNetId>& Players, UOnlineStatsRead* StatsRead)
{
	UOnlineSubsystemGameCenter* OnlineSub = CastChecked<UOnlineSubsystemGameCenter>(UGameEngine::GetOnlineSubsystem());

	// get the category 
	check(StatsRead->ColumnIds.Num() > 0);
	FString CategoryName = OnlineSub->GetCategoryNameFromPropertyId(StatsRead->ColumnIds(0));

	// the userdata is just a string
	NSString* CategoryToShow = [NSString stringWithUTF8String:TCHAR_TO_UTF8(*CategoryName)];

	// route the function to iOS thread, passing the category string as user data
	[GGameCenter PerformTaskOnMainThread:@selector(ShowLeaderboard:) WithUserData:CategoryToShow];
	
	return TRUE;
}

/**
 * Lookup one of the special Game Center properties in the given settings object. The
 * property may not have been specified in the settings, in which case use the default
 *
 * @param Settings Settings object (usually a UOnlineGameSearch*)
 * @param PropertyName Name of the property to lookup
 * @param DefaultValue Value to return of the property was not found in Settings
 *
 * @return Value of the property, or DefaultValue if it was not specified
 */
INT GetGameCenterProperty(USettings* Settings, FName PropertyName, INT DefaultValue)
{
	INT PropertyId, PropertyValue;

	// lookup the property in the settings object
	if (Settings->GetPropertyId(PropertyName, PropertyId) &&
		Settings->GetIntProperty(PropertyId, PropertyValue))
	{
		return PropertyValue;
	}

	// use default if it failed to be found
	return DefaultValue;
}

/**
 * Shows the platform supplied matchmaking UI. This will eventually either the JoinOnlineGameComplete 
 * or CreateOnlineGameComplete delegates, depending on if it's server or client
 *
 * @param SearchingPlayerNum the index of the player searching for a match
 * @param SearchSettings settings used to search for
 * @param GameSettings the game settings to use if this player becomes the server
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSuppliedUIGameCenter::ShowMatchmakingUI(BYTE SearchingPlayerNum, UOnlineGameSearch* SearchSettings, UOnlineGameSettings* GameSettings)
{
	// create the match request object
	GKMatchRequest* MatchRequest = [[GKMatchRequest alloc] init];

	// cache the search settings in the OSSGC (this variable would be used when doing non-supplied-UI searches)
	UOnlineSubsystemGameCenter* OnlineSub = CastChecked<UOnlineSubsystemGameCenter>(UGameEngine::GetOnlineSubsystem());
	// free any old search results
	OnlineSub->FreeSearchResults();
	OnlineSub->GameSearch = SearchSettings;

#define DEFAULT_MIN_PLAYERS			2
#define DEFAULT_MAX_PLAYERS			4
#define DEFAULT_PLAYER_GROUP		0
#define DEFAULT_PLAYER_ATTRIBUTES	0

	// get the match settings from the SearchSettings, or use the default if not specified
	MatchRequest.minPlayers = GetGameCenterProperty(SearchSettings, FName(TEXT("GameCenterMinPlayers")), DEFAULT_MIN_PLAYERS);
	MatchRequest.maxPlayers = GetGameCenterProperty(SearchSettings, FName(TEXT("GameCenterMaxPlayers")), DEFAULT_MAX_PLAYERS);
	MatchRequest.playerGroup = GetGameCenterProperty(SearchSettings, FName(TEXT("GameCenterPlayerGroup")), DEFAULT_PLAYER_GROUP);
	MatchRequest.playerAttributes = GetGameCenterProperty(SearchSettings, FName(TEXT("GameCenterPlayerAttributes")), DEFAULT_PLAYER_ATTRIBUTES);

	debugf(NAME_DevOnline, TEXT("ShowMatchmakingUI %d %d %d %08x"), MatchRequest.minPlayers, MatchRequest.maxPlayers, MatchRequest.playerGroup, MatchRequest.playerAttributes);
	
	// validate settings
	check(MatchRequest.minPlayers >= 2);
	// note: If using hosted matches (which we do not support), then max players could actually go up to 16
	check(MatchRequest.maxPlayers <= 4);
	check(MatchRequest.minPlayers <= MatchRequest.maxPlayers);

	// don't invite anyone automatically
	MatchRequest.playersToInvite = nil;

	// route the function to iOS thread (releasing out retain on the match request, since we're no longer "owner")
	[GGameCenter PerformTaskOnMainThread:@selector(ShowMatchmaker:) WithUserData:MatchRequest];
	[MatchRequest release];

	return TRUE;
}


#endif	//#if WITH_UE3_NETWORKING
