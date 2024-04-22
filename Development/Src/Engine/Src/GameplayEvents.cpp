/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "EnginePrivate.h"
#include "UnIpDrv.h"
#include "GameplayEventsUtilities.h"
#include "EnginePlatformInterfaceClasses.h"

IMPLEMENT_CLASS(UGameplayEvents);
IMPLEMENT_CLASS(UGameplayEventsWriterBase);
IMPLEMENT_CLASS(UGameplayEventsWriter);
IMPLEMENT_CLASS(UGameplayEventsReader);
IMPLEMENT_CLASS(UGameplayEventsHandler);
IMPLEMENT_CLASS(UGenericParamListStatEntry);
IMPLEMENT_CLASS(UGameplayEventsUploadAnalytics);

static INT GGameStatsWriterMinVersion = GAMESTATS_MIN_VER; 
static INT GGameStatsWriterVersion = GAMESTATS_LATEST_VER;

/** List of all registered game stat stream data types */
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FGameStringEvent, GameStringEvent, GET_GameString);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FGameIntEvent, GameIntEvent, GET_GameInt);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FGameFloatEvent, GameFloatEvent, GET_GameFloat);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FGamePositionEvent, GamePositionEvent, GET_GamePosition);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FTeamStringEvent, TeamStringEvent, GET_TeamString);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FTeamIntEvent, TeamIntEvent, GET_TeamInt);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FTeamFloatEvent, TeamFloatEvent, GET_TeamFloat);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FPlayerIntEvent, PlayerIntEvent, GET_PlayerInt);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FPlayerFloatEvent, PlayerFloatEvent, GET_PlayerFloat);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FPlayerStringEvent, PlayerStringEvent, GET_PlayerString);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FPlayerSpawnEvent, PlayerSpawnEvent, GET_PlayerSpawn);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FPlayerLoginEvent, PlayerLoginEvent, GET_PlayerLogin);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FPlayerLocationsEvent, PlayerLocationsEvent, GET_PlayerLocationPoll);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FPlayerKillDeathEvent, PlayerKillDeathEvent, GET_PlayerKillDeath);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FPlayerPlayerEvent, PlayerPlayerEvent, GET_PlayerPlayer);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FWeaponIntEvent, WeaponIntEvent, GET_WeaponInt);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FDamageIntEvent, DamageIntEvent, GET_DamageInt);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FProjectileIntEvent, ProjectileIntEvent, GET_ProjectileInt);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FGenericParamListEvent, GenericParamListEvent, GET_GenericParamList);

IMPLEMENT_GAMEPLAY_EVENT_TYPE(FGameAggregate, GameAggregate, GET_GameAggregate);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FTeamAggregate, TeamAggregate, GET_TeamAggregate);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FPlayerAggregate, PlayerAggregate, GET_PlayerAggregate);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FWeaponAggregate, WeaponAggregate, GET_WeaponAggregate);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FDamageAggregate, DamageAggregate, GET_DamageAggregate);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FProjectileAggregate, ProjectileAggregate, GET_ProjectileAggregate);
IMPLEMENT_GAMEPLAY_EVENT_TYPE(FPawnAggregate, PawnAggregate, GET_PawnAggregate);


#define INVALID_VALUE  0xffffffff

#ifndef FILE_BEGIN 
#define FILE_BEGIN 0
#endif

/*
 *   Create a safe filename for storing stats data
 *	@param Filename - a name to be cleaned and used
 *  @return Filename that is safe to use for stats writing
 */
FFilename CleanFilename(const FString& Filename)
{
#if XBOX
	return Filename;
#else
	//Force the game stats directory and extension
	FFilename FullPathFilename(appConvertRelativePathToFull(Filename));

	FString OutputDir;
	GetStatsDirectory(OutputDir);
	FFilename FullStatsPathname(appConvertRelativePathToFull(OutputDir));
	
	if (FullPathFilename.StartsWith(FullStatsPathname))
	{
		FFilename CleanFile(Filename);
		return CleanFile.GetPath() + PATH_SEPARATOR + CleanFile.GetBaseFilename() + GAME_STATS_FILE_EXT;
	}
	else
	{
		// Just drop the file in the root stats directory
		FFilename CleanFile(Filename);
		return OutputDir + CleanFile.GetBaseFilename() + GAME_STATS_FILE_EXT;
	}
#endif
}

/** 
 * Helper to return a valid name from a controller 
 * @param Player controller of interest
 * @return Name of the Player
 */
FString GetPlayerName(AController* Player)
{
	if (Player && Player->PlayerReplicationInfo)
	{
		return Player->PlayerReplicationInfo->PlayerName;
	}

	return TEXT("INVALID PLAYER");
}

/** 
 * Helper to return the proper player location/rotation given a controller
 * @param Player controller of interest
 * @param Location output of the pawn location if available, else controller location
 * @param Rotation output of the pawn rotation if available, else rotation location
 */
void GetPlayerLocationAndRotation(const AController* Player, FVector& Location, FRotator& Rotation)
{
	if (Player != NULL)
	{
		// grab the values off the pawn if available
		if (Player->Pawn != NULL)
		{
			Location = Player->Pawn->Location;
			Rotation = Player->Pawn->Rotation;
		}
		else
		{
			Location = Player->Location;
			Rotation = Player->Rotation;
		}

		// we do not want to store over-wound rotations, since we will only store the low 16bits of each component
		Rotation.MakeShortestRoute();
#if !CONSOLE
		check(Rotation.Yaw >= -32768 && Rotation.Yaw < 32768);
		check(Rotation.Pitch >= -32768 && Rotation.Pitch < 32768);
		check(Rotation.Roll >= -32768 && Rotation.Roll < 32768);
#endif
	}
	else
	{
		Location = FVector(0.0f);
		Rotation = FRotator(0,0,0);
	}
}

/**
 *  Given a compressed data representation for player index and rotation, uncompress into original form
 * @param IndexAndYaw - 32-bit value containing Index (HiWord) and Yaw (LoWord)
 * @param PitchAndRoll - 32-bit value containing Pitch (HiWord) and Roll (LoWord)
 * @param PlayerIndex - out value for player index
 * @param Rotation - out value of player rotation
 */
void ConvertToPlayerIndexAndRotation(const INT IndexAndYaw, const INT PitchAndRoll, INT& PlayerIndex, FRotator& Rotation)
{
	INT Yaw, Pitch, Roll;

	UnpackInt(IndexAndYaw, PlayerIndex, Yaw);
	UnpackInt(PitchAndRoll, Pitch, Roll);

	//Error checking
	if (PlayerIndex < 0 || PlayerIndex >= 0xffff)
	{
		PlayerIndex = INDEX_NONE;
	}

	Rotation = FRotator(Pitch, Yaw, Roll);
	Rotation.MakeShortestRoute();
#if !CONSOLE
	check(Rotation.Yaw >= -32768 && Rotation.Yaw < 32768);
	check(Rotation.Pitch >= -32768 && Rotation.Pitch < 32768);
	check(Rotation.Roll >= -32768 && Rotation.Roll < 32768);
#endif
}

/** 
 *   Get the metadata associated with a given event
 * @param EventID - the event to get the metadata for
 */
const FGameplayEventMetaData& UGameplayEvents::GetEventMetaData(INT EventID) const
{
	for (INT i=0; i<SupportedEvents.Num(); i++)
	{
		if (SupportedEvents(i).EventID == EventID)
		{
			return SupportedEvents(i);
		}
	}

    //Invalid record
	check(SupportedEvents.Num() > 0);
	return SupportedEvents(0);
}

/* 
 *   Get the metadata associated with a given team
 * @param TeamIndex - the event to get the metadata for
 */
const FTeamInformation& UGameplayEvents::GetTeamMetaData(INT TeamIndex) const
{
	check(TeamIndex >= 0 && TeamIndex < TeamList.Num());
	return TeamList(TeamIndex);
}

/** 
 *   Get the metadata associated with a given player
 * @param PlayerIndex - the player to get the metadata for
 */
const FPlayerInformation& UGameplayEvents::GetPlayerMetaData(INT PlayerIndex) const
{
	check(PlayerIndex >= 0 && PlayerIndex < PlayerList.Num());
	return PlayerList(PlayerIndex);
}

/** 
 *   Get the metadata associated with a given pawn class
 * @param PawnClassIndex - the pawn class to get the metadata for
 */
const FPawnClassEventData& UGameplayEvents::GetPawnMetaData(INT PawnClassIndex) const
{
	check(PawnClassIndex >= 0 && PawnClassIndex < PawnClassArray.Num());
	return PawnClassArray(PawnClassIndex);
}

/** 
 *   Get the metadata associated with a given weapon class
 * @param WeaponClassIndex - the weapon class to get the metadata for
 */
const FWeaponClassEventData& UGameplayEvents::GetWeaponMetaData(INT WeaponClassIndex) const
{
	check(WeaponClassIndex >= 0 && WeaponClassIndex < WeaponClassArray.Num());
	return WeaponClassArray(WeaponClassIndex);
}

/** 
 *	 Get the metadata associated with a given damage class
 * @param DamageClassIndex - the damage class to get the metadata for
 */
const FDamageClassEventData& UGameplayEvents::GetDamageMetaData(INT DamageClassIndex) const
{
	check(DamageClassIndex >= 0 && DamageClassIndex < DamageClassArray.Num());
	return DamageClassArray(DamageClassIndex);
}

/** 
 *   Get the metadata associated with a given projectile class
 * @param ProjectileClassIndex - the projectile class to get the metadata for
 */
const FProjectileClassEventData& UGameplayEvents::GetProjectileMetaData(INT ProjectileClassIndex) const
{
	check(ProjectileClassIndex >= 0 && ProjectileClassIndex < ProjectileClassArray.Num());
	return ProjectileClassArray(ProjectileClassIndex);
}

/** Serialize the file header */
void SerializeGameplayEventsHeader(FArchive& Archive, FGameplayEventsHeader& FileHeader)
{
	// Engine version and stats writer version are first
	Archive << FileHeader.EngineVersion;
	Archive << FileHeader.StatsWriterVersion;

	if (FileHeader.StatsWriterVersion >= GGameStatsWriterMinVersion &&	// lowest supported version
		FileHeader.StatsWriterVersion <= GGameStatsWriterVersion)		// too high is newer editor or just corrupt
	{
		Archive << FileHeader.StreamOffset;

		if (FileHeader.StatsWriterVersion >= ADDED_SUPPORTEDEVENTS_AGGREGATEOFFSET)
		{
			Archive << FileHeader.AggregateOffset;
		}

		Archive << FileHeader.FooterOffset;
		Archive << FileHeader.TotalStreamSize;
		Archive << FileHeader.FileSize;

		if (FileHeader.StatsWriterVersion >= ADDED_SUPPORTEDEVENTS_STRIPPING)
		{
			Archive << FileHeader.FilterClass;
			Archive << FileHeader.Flags;
		}
	}
}

/** Serialize the FGameSessionInformation struct */
void SerializeGameSessionInfo(FArchive& Archive, FGameSessionInformation& GameSession)
{
	Archive << GameSession.AppTitleID;
	Archive << GameSession.GameplaySessionID;
	Archive << GameSession.GameplaySessionTimestamp;
	Archive << GameSession.GameplaySessionStartTime;
	Archive << GameSession.GameplaySessionEndTime;
	Archive << GameSession.PlatformType;
	Archive << GameSession.Language;

	if (Archive.Ver() >= ADDED_MAPNAME_MAPURL_TO_GAMESESSIONINFO)
	{
		Archive << GameSession.GameClassName;
		Archive << GameSession.MapName;
		Archive << GameSession.MapURL;

		if (Archive.Ver() >= ADDED_SESSION_INSTANCE)
		{   
			Archive << GameSession.SessionInstance;

			if (Archive.Ver() >= ADDED_OWNINGNETID_GAMETYPEID)
			{
				Archive << GameSession.OwningNetId;
				Archive << GameSession.GameTypeId;
				if (Archive.Ver() >= ADDED_PLAYLISTID)
				{
					Archive << GameSession.PlaylistId;
				}
				else
				{
					GameSession.PlaylistId = -1;
				}
			}
			else
			{
				GameSession.OwningNetId = FUniqueNetId((QWORD)0);
				GameSession.GameTypeId = 0;
				GameSession.PlaylistId = -1;
			}
		}
		else
		{
			GameSession.SessionInstance = 0;
			GameSession.OwningNetId = FUniqueNetId((QWORD)0);
			GameSession.GameTypeId = 0;
			GameSession.PlaylistId = -1;
		}
	}
	else if (Archive.IsLoading())
	{
		GameSession.GameClassName = FString(TEXT("UNKNOWN"));
		GameSession.MapName = FString(TEXT("UNKNOWN"));
		GameSession.MapURL = FString(TEXT("UNKNOWN"));
		GameSession.SessionInstance = 0;
		GameSession.OwningNetId = FUniqueNetId((QWORD)0);
		GameSession.GameTypeId = 0;
		GameSession.PlaylistId = -1;
	}
}

/**
 * Fill in the current session info struct with all relevant gameplay session info
 * @param CurrentSessionInfo - empty struct to populate
 * @param GameTypeId - game type Id for the game played
 * @param PlaylistId - playlist for the game played
 * @return TRUE if struct was filled in successfully, FALSE otherwise
 */
UBOOL SetupGameSessionInfo(FGameSessionInformation& CurrentSessionInfo, INT GameTypeId, INT PlaylistId)
{
	extern const FString GetMapNameStatic();

	UBOOL bSuccess = FALSE; 
	if (GWorld)
	{
		AGameInfo* Game = GWorld->GetGameInfo(); 
		if (Game)
		{
			UGameEngine* GameEngine = CastChecked<UGameEngine>(GEngine);

			CurrentSessionInfo.bGameplaySessionInProgress = true;
			CurrentSessionInfo.GameplaySessionTimestamp = appUtcTimeString();
			CurrentSessionInfo.GameplaySessionStartTime	= GWorld->GetRealTimeSeconds();
			CurrentSessionInfo.GameplaySessionEndTime = GWorld->GetRealTimeSeconds();
			CurrentSessionInfo.GameplaySessionID = appCreateGuid().String();
			CurrentSessionInfo.AppTitleID = appGetTitleId();
			CurrentSessionInfo.GameClassName = Game->GetClass()->GetName();
			CurrentSessionInfo.GameTypeId = GameTypeId;
			CurrentSessionInfo.PlaylistId = PlaylistId;
			CurrentSessionInfo.MapName = GetMapNameStatic();
			CurrentSessionInfo.MapURL = *GWorld->URL.String();//*CastChecked<UGameEngine>(GEngine)->LastURL.String();
			CurrentSessionInfo.PlatformType = appGetPlatformType();
			CurrentSessionInfo.Language = appGetLanguageExt();
			CurrentSessionInfo.SessionInstance = 0;

			CurrentSessionInfo.OwningNetId = FUniqueNetId((QWORD)0);
			if (GameEngine != NULL && GameEngine->OnlineSubsystem != NULL)
			{
				FNamedSession* GameSession = GameEngine->OnlineSubsystem->GetNamedSession(FName("Game"));
				if (GameSession && GameSession->GameSettings)
				{
					CurrentSessionInfo.OwningNetId = GameSession->GameSettings->OwningPlayerId;
				}
				else
				{
					FNamedSession* PartySession = GameEngine->OnlineSubsystem->GetNamedSession(FName("Party"));
					if (PartySession && PartySession->GameSettings)
					{
						CurrentSessionInfo.OwningNetId = PartySession->GameSettings->OwningPlayerId;
					}
				}
			}

			bSuccess = TRUE;
		}
	}

	return bSuccess;
}

/** Serialize a single game stat group */
FArchive& operator<<(FArchive& Ar, FGameStatGroup& GameStatGroup)
{
	BYTE Group;
	if (Ar.IsLoading())
	{
		Ar << Group;
		GameStatGroup.Group = (EGameStatGroups)Group;
	}
	else 
	{
		Group = (BYTE)GameStatGroup.Group;
		Ar << Group;
	}
	
	Ar << GameStatGroup.Level;
	return Ar;
}

/** Serialize all the metadata arrays */
void SerializeMetadata(FArchive& Archive, UGameplayEvents* GameplaySession, UBOOL bEventIDsOnly)
{
	//Serialize out the supported events array
	if (bEventIDsOnly)
	{
		SerializeGameplayEventMetaData(Archive, GameplaySession->SupportedEvents);
	}
	else
	{
		Archive << GameplaySession->SupportedEvents;
	}

	//Serialize the player array
	Archive << GameplaySession->PlayerList;

	//Serialize the team array
	Archive << GameplaySession->TeamList;

	//Serialize the weapons array
	Archive << GameplaySession->WeaponClassArray;

	//Serialize the pawn array
	Archive << GameplaySession->PawnClassArray;

	//Serialize the projectiles array
	Archive << GameplaySession->ProjectileClassArray;

	//Serialize the damage class array
	Archive << GameplaySession->DamageClassArray;

	if (Archive.Ver() >= ADDED_ACTOR_ARRAY)
	{
		// ADDED_ACTOR_ARRAY - Serialize any actors that were referenced by events
		Archive << GameplaySession->ActorArray;
	}

	if (Archive.Ver() >= ADDED_SOUNDCUE_ARRAY)
	{
		// Serialize all the sound cue information necessary 
	   Archive << GameplaySession->SoundCueArray;
	}
}

/* 
 * Serializes the gameplay event metadata, saving only the event ID and datatype
 * This type of serialization is not expected to be ever loaded by the engine
 * for uploading minimal data to MCP in production
 */
void SerializeGameplayEventMetaData(FArchive& Ar, TArray<FGameplayEventMetaData>& GameplayEventMetaData)
{
	if (Ar.IsSaving())
	{
		// Serialize as TArray but without the extra data
		INT EventCount = GameplayEventMetaData.Num();
		Ar << EventCount;
		for( INT i=0; i<EventCount; i++ )
		{
			WORD WORDVal;
			WORDVal = GameplayEventMetaData(i).EventID;
			Ar << WORDVal;
			WORDVal = GameplayEventMetaData(i).EventDataType;
			Ar << WORDVal; 
		}
	}
}

/** Serialization of the gameplay events supported meta data */
FArchive& operator<<(FArchive& Ar, FGameplayEventMetaData& GameplayEventMetaData)
{
	FString EventName;
	if (Ar.IsLoading())
	{
		appMemzero(&GameplayEventMetaData, sizeof(FGameplayEventMetaData));

		Ar << GameplayEventMetaData.EventID << EventName;
		if (Ar.Ver() >= ADDED_GROUPID_EVENT_DATATYPE)
		{
			Ar << GameplayEventMetaData.StatGroup;
			Ar << GameplayEventMetaData.EventDataType;
		}
		else
		{
			BYTE MappingType;
			INT MaxValue;

			// Consume obsolete data
			Ar << MappingType;
			Ar << MaxValue;
			// Init new data that doesn't exist
			GameplayEventMetaData.StatGroup.Group = GSG_Game;
			GameplayEventMetaData.StatGroup.Level = 0;
			GameplayEventMetaData.EventDataType = -1;
		}

		GameplayEventMetaData.EventName = FName(*EventName);
	}
	else
	{
		EventName = GameplayEventMetaData.EventName.ToString();
		Ar << GameplayEventMetaData.EventID << EventName;
		Ar << GameplayEventMetaData.StatGroup;
		Ar << GameplayEventMetaData.EventDataType;
	}
	return Ar;
}

/** Serialization of the player information meta data */
FArchive& operator<<(FArchive& Ar, FPlayerInformation& PlayerInfo)
{
	FString ControllerName;
	BYTE IsBot;
	if (Ar.IsLoading())
	{
		appMemzero(&PlayerInfo, sizeof(FPlayerInformation));
		if (Ar.Ver() >= ADDED_UNIQUEID_TO_PLAYERINFO)
		{
			if (Ar.Ver() >= REMOVED_UNIQUEID_HASH)
			{
				Ar << ControllerName << PlayerInfo.PlayerName << PlayerInfo.UniqueId << IsBot;
			}
			else
			{
				FString UnusedUniqueIDHash;
				Ar << ControllerName << PlayerInfo.PlayerName << PlayerInfo.UniqueId << UnusedUniqueIDHash << IsBot;
			}
		}
		else
		{
		   Ar << ControllerName << PlayerInfo.PlayerName << IsBot;
		}
		
		PlayerInfo.ControllerName = FName(*ControllerName);
		PlayerInfo.bIsBot = (IsBot != 0) ? TRUE : FALSE;
	}
	else
	{
		IsBot = (PlayerInfo.bIsBot) ? 1 : 0;
		ControllerName = PlayerInfo.ControllerName.ToString();
		Ar << ControllerName << PlayerInfo.PlayerName << PlayerInfo.UniqueId << IsBot;
	}

	return Ar;
}

/** Serialization of the team information meta data */
FArchive& operator<<(FArchive& Ar, FTeamInformation& TeamInfo)
{
	if (Ar.IsLoading())
	{
		appMemzero(&TeamInfo, sizeof(FTeamInformation));
	}

	Ar << TeamInfo.TeamIndex << TeamInfo.TeamName;
	Ar << TeamInfo.TeamColor << TeamInfo.MaxSize;

	return Ar;
}

/** Serialization of the weapon class meta data */
FArchive& operator<<(FArchive& Ar, FWeaponClassEventData& WeaponClassEventData)
{
	FString StringVal;
	if (Ar.IsLoading())
	{
		appMemzero(&WeaponClassEventData, sizeof(FWeaponClassEventData));
		Ar << StringVal;
		WeaponClassEventData.WeaponClassName = FName(*StringVal);
	}
	else
	{
		StringVal = WeaponClassEventData.WeaponClassName.ToString();
		Ar << StringVal;
	}

	return Ar;
}

/** Serialization of the damage class meta data */
FArchive& operator<<(FArchive& Ar, FDamageClassEventData& DamageClassEventData)
{	   
	FString StringVal;
	if (Ar.IsLoading())
	{
		appMemzero(&DamageClassEventData, sizeof(FDamageClassEventData));
		Ar << StringVal;
		DamageClassEventData.DamageClassName = FName(*StringVal);
	}
	else
	{
		StringVal = DamageClassEventData.DamageClassName.ToString();
		Ar << StringVal;
	}

	return Ar;
}

/** Serialization of the projectile class meta data */
FArchive& operator<<(FArchive& Ar, FProjectileClassEventData& ProjectileClassEventData)
{
	FString StringVal;
	if (Ar.IsLoading())
	{
		appMemzero(&ProjectileClassEventData, sizeof(FProjectileClassEventData));
		Ar << StringVal;
		ProjectileClassEventData.ProjectileClassName = FName(*StringVal);
	}
	else
	{
		StringVal = ProjectileClassEventData.ProjectileClassName.ToString();
		Ar << StringVal;
	}

	return Ar;
}

/** Serialization of the pawn class meta data */
FArchive& operator<<(FArchive& Ar, FPawnClassEventData& PawnClassEventData)
{
	FString StringVal;
	if (Ar.IsLoading())
	{
		appMemzero(&PawnClassEventData, sizeof(FPawnClassEventData));
		Ar << StringVal;
		PawnClassEventData.PawnClassName = FName(*StringVal);
	}
	else
	{
		StringVal = PawnClassEventData.PawnClassName.ToString();
		Ar << StringVal;
	}

	return Ar;
}

/** 
 *  Turns a player controller into an index, possibly adding new information to the array 
 *
 *  @param Player - the controller for the player of interest
 *  @return Index into the metadata array
 */
INT UGameplayEventsWriter::ResolvePlayerIndex(AController* Player)
{
	INT PlayerIdx = INDEX_NONE;
	if (Player != NULL && Player->PlayerReplicationInfo != NULL)
	{
		const FName& ControllerName = Player->GetFName();
		
		// look for an existing entry
		for (INT Idx = 0; Idx < PlayerList.Num(); Idx++)
		{
			if (PlayerList(Idx).ControllerName == ControllerName)
			{
				PlayerIdx = Idx;
				PlayerList(PlayerIdx).PlayerName = Player->PlayerReplicationInfo->PlayerName;
				break;
			}
		}
		// if none found,
		if (PlayerIdx == INDEX_NONE)
		{
			// add an entry
			PlayerIdx = PlayerList.AddZeroed();
			FPlayerInformation& PlayerInfo = PlayerList(PlayerIdx);
			PlayerInfo.ControllerName = ControllerName;
			PlayerInfo.PlayerName = Player->PlayerReplicationInfo->PlayerName;
			PlayerInfo.UniqueId = Player->PlayerReplicationInfo->UniqueId;
			PlayerInfo.bIsBot = Player->PlayerReplicationInfo->bBot;

			debugf(NAME_GameStats, TEXT("### Adding Player [%s] to PlayerList @ index %i"), *Player->PlayerReplicationInfo->PlayerName, PlayerIdx);
		}
	}

	return PlayerIdx;
}

/** 
 *  Turns team information into an index, possibly adding new information to the array 
 *
 *  @param TeamIndex - the controller for the player of interest
 *  @return Index into the metadata array
 */
INT UGameplayEventsWriter::ResolveTeamIndex(ATeamInfo* TeamInfo)
{
	INT TeamIdx = INDEX_NONE;
	if (TeamInfo != NULL)
	{
		const FString& TeamName = TeamInfo->TeamName;

		// look for an existing entry
		for (INT Idx = 0; Idx < TeamList.Num(); Idx++)
		{
			if (TeamList(Idx).TeamIndex == TeamInfo->TeamIndex && TeamList(Idx).TeamName == TeamName)
			{
				TeamIdx = Idx;
				TeamList(TeamIdx).MaxSize = Max(TeamInfo->Size, TeamList(TeamIdx).MaxSize);
				break;
			}
		}

		// if none found,
		if (TeamIdx == INDEX_NONE)
		{
			// add an entry
			TeamIdx = TeamList.AddZeroed();
			TeamList(TeamIdx).TeamName = TeamName;
			TeamList(TeamIdx).TeamIndex = TeamIdx;
			TeamList(TeamIdx).TeamColor = TeamInfo->TeamColor;
			TeamList(TeamIdx).MaxSize = TeamInfo->Size;

			debugf(NAME_GameStats, TEXT("### Adding Team [%s] to TeamList @ index %i"), *TeamName, TeamIdx);
		}
	}

	return TeamIdx;
}

/** 
 *  Turns a weapon class into an index, possibly adding new information to the array 
 *
 *  @param WeaponClass - the UClass describing the weapon
 *  @return Index into the metadata array
 */
INT UGameplayEventsWriter::ResolveWeaponClassIndex(UClass* WeaponClass)
{
	INT WeaponIdx = INDEX_NONE;
	if (WeaponClass != NULL)
	{
		const FName& WeaponClassName = WeaponClass->GetFName();

		// look for an existing entry
		for (INT Idx = 0; Idx < WeaponClassArray.Num(); Idx++)
		{
			if (WeaponClassArray(Idx).WeaponClassName == WeaponClassName)
			{
				WeaponIdx = Idx;
				break;
			}
		}

		// if none found,
		if (WeaponIdx == INDEX_NONE)
		{
			// add an entry
			WeaponIdx = WeaponClassArray.AddZeroed();
			WeaponClassArray(WeaponIdx).WeaponClassName = WeaponClassName;

			debugf(NAME_GameStats, TEXT("### Adding WeaponClass [%s] to WeaponList @ index %i"), *WeaponClassName.ToString(), WeaponIdx);
		}
	}

	return WeaponIdx;
}

/** 
 *  Turns a pawn class into an index, possibly adding new information to the array 
 *
 *  @param PawnClass - the UClass describing the pawn
 *  @return Index into the metadata array
 */
INT UGameplayEventsWriter::ResolvePawnIndex(UClass* PawnClass)
{
	INT PawnClassIdx = INDEX_NONE;
	if (PawnClass != NULL)
	{
		const FName& PawnClassName = PawnClass->GetFName();

		// look for an existing entry
		for (INT Idx = 0; Idx < PawnClassArray.Num(); Idx++)
		{
			if (PawnClassArray(Idx).PawnClassName == PawnClassName)
			{
				PawnClassIdx = Idx;
				break;
			}
		}

		// if none found,
		if (PawnClassIdx == INDEX_NONE)
		{
			// add an entry
			PawnClassIdx = PawnClassArray.AddZeroed();
			PawnClassArray(PawnClassIdx).PawnClassName = PawnClassName;

			debugf(NAME_GameStats, TEXT("### Adding PawnClass [%s] to PawnClassList @ index %i"), *PawnClassName.ToString(), PawnClassIdx);
		}
	}

	return PawnClassIdx;
}

/** 
 *  Turns a damage class into an index, possibly adding new information to the array 
 *
 *  @param DamageClass - the UClass describing the damage class
 *  @return Index into the metadata array
 */
INT UGameplayEventsWriter::ResolveDamageClassIndex(UClass* DamageClass)
{
	INT DamageClassIdx = INDEX_NONE;
	if (DamageClass != NULL)
	{
		const FName& DamageClassName = DamageClass->GetFName();

		// look for an existing entry
		for (INT Idx = 0; Idx < DamageClassArray.Num(); Idx++)
		{
			if (DamageClassArray(Idx).DamageClassName == DamageClassName)
			{
				DamageClassIdx = Idx;
				break;
			}
		}

		// if none found,
		if (DamageClassIdx == INDEX_NONE)
		{
			// add an entry
			DamageClassIdx = DamageClassArray.AddZeroed();
			DamageClassArray(DamageClassIdx).DamageClassName = DamageClassName;

			debugf(NAME_GameStats, TEXT("### Adding DamageClass [%s] to DamageClassList @ index %i"), *DamageClassName.ToString(), DamageClassIdx);
		}
	}

	return DamageClassIdx;
}

/** 
 *  Turns a projectile class into an index, possibly adding new information to the array 
 *
 *  @param ProjectileClass - the UClass describing the projectile class
 *  @return Index into the metadata array
 */
INT UGameplayEventsWriter::ResolveProjectileClassIndex(UClass* ProjectileClass)
{
	INT ProjectileClassIdx = INDEX_NONE;
	if (ProjectileClass != NULL)
	{
		const FName& ProjectileClassName = ProjectileClass->GetFName();

		// look for an existing entry
		for (INT Idx = 0; Idx < ProjectileClassArray.Num(); Idx++)
		{
			if (ProjectileClassArray(Idx).ProjectileClassName == ProjectileClassName)
			{
				ProjectileClassIdx = Idx;
				break;
			}
		}

		// if none found,
		if (ProjectileClassIdx == INDEX_NONE)
		{
			// add an entry
			ProjectileClassIdx = ProjectileClassArray.AddZeroed();
			ProjectileClassArray(ProjectileClassIdx).ProjectileClassName = ProjectileClassName;

			debugf(NAME_GameStats, TEXT("### Adding ProjectileClass [%s] to ProjectileClassList @ index %i"), *ProjectileClassName.ToString(), ProjectileClassIdx);
		}
	}

	return ProjectileClassIdx;
}

/**
 * Turns an actor into an index
 * @param Actor the actor to find in the array
 * @return the index in the array for that actor
 */
INT UGameplayEventsWriter::ResolveActorIndex(AActor* Actor)
{
	INT Index = INDEX_NONE;
	if (Actor != NULL)
	{
		Index = ActorArray.FindItemIndex(Actor->GetName());
		if (Index == INDEX_NONE)
		{
			Index = ActorArray.AddItem(Actor->GetName());
		}
	}
	return Index;
}

/**
 * Turns an sound cue into an index
 *
 * @param Cue the sound cue to find in the array
 *
 * @return the index in the array for that sound cue
 */
INT UGameplayEventsWriter::ResolveSoundCueIndex(USoundCue* Cue)
{
	INT Index = INDEX_NONE;
	if (Cue != NULL)
	{
		Index = SoundCueArray.FindItemIndex(Cue->GetName());
		if (Index == INDEX_NONE)
		{
			Index = SoundCueArray.AddItem(Cue->GetName());
		}
	}
	return Index;
}

/** 
 *  Open the game stats file for reading
 * @param Filename - name of the file that will be open for serialization
 * @return TRUE if successful, else FALSE
 */
UBOOL UGameplayEventsReader::OpenStatsFile(const FString& Filename)
{
	UBOOL bSuccess = FALSE;
	if (Archive == NULL && Filename.Len() > 0)
	{
		FString NewFilename = CleanFilename(Filename);
		debugf(/*NAME_GameStats,*/ TEXT("Reading game stats recording file %s..."), *NewFilename);
		Archive = GFileManager->CreateFileReader(*NewFilename);
		if (Archive != NULL)
		{
			Archive->SetForceUnicode(TRUE); // so string length math works out

			// Read in header and meta data
			if (SerializeHeader())
			{
				StatsFileName = NewFilename;
				bSuccess = !Archive->IsError();
			}
			else
			{
				// Try swapping byte order
				Archive->Seek(FILE_BEGIN);
				Archive->SetByteSwapping(TRUE);
				if (SerializeHeader())
				{
					StatsFileName = NewFilename;
					bSuccess = !Archive->IsError();
				}
			}

			if(bSuccess == FALSE)
			{
				debugf(/*NAME_GameStats,*/ TEXT("Failed to serialize header for %s..."), *NewFilename);
				CloseStatsFile();
			}
		}   
		else
		{
			debugf(/*NAME_GameStats,*/ TEXT("Failed to open file %s for reading..."), *NewFilename);
		}
	}

#if XBOX && (!FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE)
	if (!bSuccess)
	{
		XeLogHDDCacheStatus(TRUE);
	}
#endif

	return bSuccess;
}

/** 
 * Closes and deletes the archive that was being read from 
 * clearing all data stored within
 */
void UGameplayEventsReader::CloseStatsFile(void)
{
	if (Archive)
	{
		debugf(NAME_GameStats, TEXT("Closing game stats recording file %s..."), *StatsFileName);

		//Close	file
		delete Archive;
		Archive = NULL;

		//Empty out the data
		PlayerList.Empty();
		TeamList.Empty();
		WeaponClassArray.Empty();
		DamageClassArray.Empty();
		ProjectileClassArray.Empty();
		PawnClassArray.Empty();

		StatsFileName = TEXT("");
	}
}

/** Read in the header from the game stats file */
UBOOL UGameplayEventsReader::SerializeHeader()
{
	if (Archive)
	{
		check(Archive->Tell() == FILE_BEGIN);

		// File header is first
		appMemzero(&Header, sizeof(FGameplayEventsHeader));
		SerializeGameplayEventsHeader(*Archive, Header);

		if(Archive->IsError() ||
		   Header.StatsWriterVersion < GGameStatsWriterMinVersion ||	// lowest supported version
		   Header.StatsWriterVersion > GGameStatsWriterVersion ||		// too high is newer editor or just corrupt
		   Header.StreamOffset <= 0 ||									// stream starts after header
		   Header.FooterOffset == INVALID_VALUE ||						// never wrote out the footer 
		   Header.FileSize == INVALID_VALUE ||	
		   Header.FileSize != Archive->TotalSize() ||
		   Header.TotalStreamSize <= 0 ||
		   Header.FileSize <= 0)
		{
			debugf(/*NAME_GameStats,*/ TEXT("Invalid stats file header %s"), *StatsFileName);
			debugf(/*NAME_GameStats,*/ TEXT("Version: %d"), Header.StatsWriterVersion);
			debugf(/*NAME_GameStats,*/ TEXT("StreamOffset:%d"), Header.StreamOffset);
			debugf(/*NAME_GameStats,*/ TEXT("FooterOffset:%d"), Header.FooterOffset);
			debugf(/*NAME_GameStats,*/ TEXT("FileSize:%d"),	Header.FileSize);
			debugf(/*NAME_GameStats,*/ TEXT("StreamSize:%d"), Header.TotalStreamSize);
			debugf(/*NAME_GameStats,*/ TEXT("ArchiveSize:%d"), Archive->TotalSize());
			return FALSE;
		}

		// Set the archive version for serialization
		Archive->SetVer(Header.StatsWriterVersion);

		// Session info is next
		appMemzero(&CurrentSessionInfo, sizeof(FGameSessionInformation));
		SerializeGameSessionInfo(*Archive, CurrentSessionInfo);

		/* (title id isn't same between platforms)
		//Do some error checking 
		if (CurrentSessionInfo.AppTitleID != appGetTitleId())
		{
			debugf(NAME_GameStats, TEXT("Stats file belongs to a different game %s"), *StatsFileName);
			return FALSE;
		}
		*/

		//Serialize all the metadata
		if (!Archive->IsError() && Header.FooterOffset > 0 && Header.FooterOffset < Header.FileSize)
		{
			Archive->Seek(Header.FooterOffset);
			UBOOL bEventIDsOnly = (Header.Flags & UCONST_HeaderFlags_NoEventStrings) ? TRUE : FALSE;
			SerializeMetadata(*Archive, this, bEventIDsOnly);
			return TRUE;
		}
		else
		{
			debugf(TEXT("Game Stats footer offset outside of file bounds. [%d/%d]"), Header.FooterOffset, Header.FileSize);
		}
	}

	return FALSE;
}

/** Signal start of stream processing */
void UGameplayEventsReader::ProcessStreamStart()
{
	for (INT HandlerIdx=0; HandlerIdx<RegisteredHandlers.Num(); HandlerIdx++)
	{
		RegisteredHandlers(HandlerIdx)->eventPreProcessStream();
	}
}

/** Read the body of the game stats file */
void UGameplayEventsReader::ProcessStream()
{
	if (Archive)
	{
#if STATS
		FScopedLogTimer ScopeTimer(TEXT("UGameplayEventsReader::ProcessStream"), FALSE);
#endif
		// Notify the start of the stream processing
		ProcessStreamStart();

		// Seek back to the start of the stream
		if (Header.StreamOffset > 0 && Header.StreamOffset < Header.FileSize)
		{
			Archive->Seek(Header.StreamOffset);

			INT SizeRead = 0;

			INT FilePos;
			FGameEventHeader TempEvent;
			while (SizeRead < Header.TotalStreamSize)
			{
				// Read in the header before the payload
				*Archive << TempEvent;
				SizeRead += TempEvent.GetDataSize();

				debugfSlow(NAME_GameStats, TEXT("EventType: %d EventId: %d TimeStamp: %.2f Size: %d"), TempEvent.EventType, TempEvent.EventID, TempEvent.TimeStamp, TempEvent.DataSize);
				FilePos = Archive->Tell();

				// Find the factory capable of serializing this data type
				IGameEvent* NewEvent = FGameEventType::GetFactory(TempEvent.EventType);
				if (NewEvent)
				{
					// Serialize the data
					NewEvent->Serialize(*Archive);
					// Allow the reader a chance to handle this data type
					for (INT HandlerIdx=0; HandlerIdx<RegisteredHandlers.Num(); HandlerIdx++)
					{
						RegisteredHandlers(HandlerIdx)->HandleEvent(TempEvent, NewEvent);
					}
				}
				else
				{
					// Skip any unknown data types
					debugf(NAME_GameStats, TEXT("UGameplayEventsReader unknown data type [%d], skipping %d bytes"), TempEvent.EventType, TempEvent.DataSize);
					Archive->Seek(FilePos + TempEvent.DataSize);
				}

				SizeRead += TempEvent.DataSize;
				if (FilePos + TempEvent.DataSize != Archive->Tell())
				{
					debugf(NAME_GameStats, TEXT("UGameplayEventsReader data size does not match archive position!"));
				}
			}
		}
		else
		{
			debugf(TEXT("Game Stats stream offset outside of file bounds. [%d/%d]"), Header.StreamOffset, Header.FileSize);
		}

		// Notify the end of the stream processing
		ProcessStreamEnd();
	}
}

/** Signal end of stream processing */
void UGameplayEventsReader::ProcessStreamEnd()
{
	for (INT HandlerIdx=0; HandlerIdx<RegisteredHandlers.Num(); HandlerIdx++)
	{
		RegisteredHandlers(HandlerIdx)->eventPostProcessStream();
	}
}

/** Get the session ID information for the current session */
FString UGameplayEventsReader::GetSessionID()
{
	return CurrentSessionInfo.GetSessionID();
}

/** Get the title ID for the current session */
INT UGameplayEventsReader::GetTitleID()
{
	return CurrentSessionInfo.AppTitleID;
}

/** Get the platform the data was recorded on */
INT UGameplayEventsReader::GetPlatform()
{
	return CurrentSessionInfo.PlatformType;
}

/** Get the timestamp when the data was recorded */
FString UGameplayEventsReader::GetSessionTimestamp()
{
	return CurrentSessionInfo.GameplaySessionTimestamp;
}

/** Get the time the session started */
FLOAT UGameplayEventsReader::GetSessionStart()
{
	return CurrentSessionInfo.GameplaySessionStartTime;
}

/** Get the time the session ended */
FLOAT UGameplayEventsReader::GetSessionEnd()
{
	return CurrentSessionInfo.GameplaySessionEndTime;
}

/** Get the length of time the session was played */
FLOAT UGameplayEventsReader::GetSessionDuration()
{
	return CurrentSessionInfo.GameplaySessionEndTime - CurrentSessionInfo.GameplaySessionStartTime;
}

/** Mark a new session, clear existing events, etc */
void UGameplayEventsWriter::StartLogging(FLOAT HeartbeatDelta)
{
	if (GIsGame && !GIsEditor && !CurrentSessionInfo.bGameplaySessionInProgress)
	{
		// Create a unique filename for this session
		const FString& Filename = GetUniqueStatsFilename();

		// Create the stats file
		if (OpenStatsFile(Filename))
		{
			// Fill out session information
			if (SetupGameSessionInfo(CurrentSessionInfo, eventGetGameTypeId(), eventGetPlaylistId()))
			{
				// And write the header information to it
				if (SerializeHeader())
				{
					check(GWorld);
					Game = GWorld->GetGameInfo();

					// Setup the heartbeat
					if (HeartbeatDelta > 0.0f)
					{
						eventStartPolling(HeartbeatDelta);
					}
					else
					{
						eventStopPolling();
					}
				}
			}
		}
	}
}

/** 
 * Resets the session, clearing all event data, but keeps the session ID/Timestamp intact
 */
void UGameplayEventsWriter::ResetLogging(FLOAT HeartbeatDelta)
{
	if (GIsGame && !GIsEditor)
	{	
		// Create a unique filename for this session
		const FString& Filename = GetUniqueStatsFilename();

		// Create the stats file
		if (OpenStatsFile(Filename))
		{
			// Save the previous session information
			FGameSessionInformation OldSessionInfo = CurrentSessionInfo;

			// Fill out session information
			if (SetupGameSessionInfo(CurrentSessionInfo, eventGetGameTypeId(), eventGetPlaylistId()))
			{		
				// Restore the previous Timestamp and SessionID
				CurrentSessionInfo.GameplaySessionTimestamp = OldSessionInfo.GameplaySessionTimestamp;
				CurrentSessionInfo.GameplaySessionID = OldSessionInfo.GameplaySessionID;
				CurrentSessionInfo.SessionInstance = OldSessionInfo.SessionInstance + 1;

				// And write the header information to it
				if (SerializeHeader())
				{
					check(GWorld);
					Game = GWorld->GetGameInfo();

					// Restart polling for stats if heartbeat was specified
					if (HeartbeatDelta > 0.0f)
					{
						eventStartPolling(HeartbeatDelta);
					}
					else
					{
						eventStopPolling();
					}
				}
			}
		}

		// Clear out all metadata, except supported events
		PlayerList.Empty();
		TeamList.Empty();
		WeaponClassArray.Empty();
		DamageClassArray.Empty();
		ProjectileClassArray.Empty();
		PawnClassArray.Empty();
		ActorArray.Empty();
		SoundCueArray.Empty();
	}
}

/** Write out the current session's gameplay events */
void UGameplayEventsWriter::EndLogging()
{
	if (GIsGame && !GIsEditor && CurrentSessionInfo.bGameplaySessionInProgress)
	{
		check(GWorld);
		Game = NULL;

		// End heartbeat polling since this session is finished
		eventStopPolling();			

		// Mark end of session
		CurrentSessionInfo.GameplaySessionEndTime = GWorld->GetRealTimeSeconds();
		CurrentSessionInfo.bGameplaySessionInProgress = false;

		// Close the file
		CloseStatsFile();
	}
}

/** 
 *   Creates the archive that we are going to write to 
 * @param Filename - name of the file that will be open for serialization
 * @return TRUE if successful, else FALSE
 */
UBOOL UGameplayEventsWriter::OpenStatsFile(const FString& Filename)
{
	UBOOL bSuccess = FALSE;
	if (Archive == NULL && Filename.Len() > 0)
	{
		FString NewFilename = CleanFilename(Filename);
		debugf(/*NAME_GameStats,*/ TEXT("Writing game stats recording file %s..."), *NewFilename);
		Archive = GFileManager->CreateFileWriter(*NewFilename, FILEWRITE_Async);
		if (Archive != NULL)
		{
			StatsFileName = NewFilename;
			Archive->SetForceUnicode(TRUE); //so string length math works out
			bSuccess = !Archive->IsError();
		}
		else
		{
#if (!CONSOLE && !PLATFORM_MACOSX) || XBOX
			debugf(/*NAME_GameStats,*/ TEXT("Failed to open file %s for writing... %d"), *NewFilename, GetLastError());
#else
			debugf(/*NAME_GameStats,*/ TEXT("Failed to open file %s for writing..."), *NewFilename);
#endif
		}
	}

#if XBOX && (!FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE)
	if (!bSuccess)
	{
		XeLogHDDCacheStatus(TRUE);
	}
#endif

	return bSuccess;
}

/** 
 * Closes and deletes the archive that was being written to 
 * clearing all data stored within
 */
void UGameplayEventsWriter::CloseStatsFile(void)
{
	if (Archive)
	{	
		{
#if STATS
			FScopedLogTimer ScopeTimer(TEXT("UGameplayEventsWriter::CloseStatsFile"), FALSE);
#endif
			//Serialize out the metadata
			if (SerializeFooter())
			{
				// Calculate total stream data
				Header.TotalStreamSize = Header.FooterOffset - Header.StreamOffset;

				//Write out the file size
				Header.FileSize = Archive->TotalSize();

				// Serialize the header again (now with final values)
				Archive->Seek(FILE_BEGIN);
				SerializeGameplayEventsHeader(*Archive, Header);

				// Serialize the game session info (now with final values)
				SerializeGameSessionInfo(*Archive, CurrentSessionInfo);
			}
			else
			{
				debugf(TEXT("Failed to serialize footer, file will not be readable."));
			}

			//Close
			debugf(/*NAME_GameStats,*/ TEXT("Closing game stats recording file %s..."), *StatsFileName);
			delete Archive;
			Archive = NULL;

			//Empty out the data
			PlayerList.Empty();
			TeamList.Empty();
			WeaponClassArray.Empty();
			DamageClassArray.Empty();
			ProjectileClassArray.Empty();
			PawnClassArray.Empty();
			ActorArray.Empty();
			SoundCueArray.Empty();
		}
	}
}

/** Create the header that begins the game stats file */
UBOOL UGameplayEventsWriter::SerializeHeader(void)
{
	UBOOL bSuccess = FALSE;
	if (Archive)
	{	
		//USEFUL?
		//FString MapName = CastChecked<UGameEngine>(GEngine)->LastURL.Map;

		// Fill out header information
		Header.EngineVersion = GEngineVersion;
		Header.StatsWriterVersion = GGameStatsWriterVersion;
		Header.StreamOffset = INVALID_VALUE;
		Header.AggregateOffset = INVALID_VALUE;
		Header.FooterOffset = INVALID_VALUE;
		Header.TotalStreamSize = INVALID_VALUE;
		Header.FileSize = INVALID_VALUE;

		// Serialize the header (not everything filled out yet)
		SerializeGameplayEventsHeader(*Archive, Header);

		// Serialize the session information
		SerializeGameSessionInfo(*Archive, CurrentSessionInfo);
	
		//Mark where the stream begins
		Header.StreamOffset = Archive->Tell();

		// num, slacknum, Bytes used, bytes not used
		//Ar->Logf(TEXT("Player Events, %i, %i, %i, %i"), PlayerEvents.Num(), PlayerEvents.GetSlack(), PlayerEvents.Num() * PlayerEvents.GetTypeSize(), PlayerEvents.GetSlack() * PlayerEvents.GetTypeSize());
		bSuccess = !Archive->IsError();
	}

	return bSuccess;
}

/** Create the footer that ends the game stats file */
UBOOL UGameplayEventsWriter::SerializeFooter(void)
{
	UBOOL bSuccess = FALSE;
	if (Archive)
	{
		// Mark footer location
		Header.FooterOffset = Archive->Tell();

		// For size savings, serialize footer strings as best fit
		Archive->SetForceUnicode(FALSE); 

		// Serialize all the metadata
		UBOOL bEventIDsOnly = (Header.Flags & UCONST_HeaderFlags_NoEventStrings) ? TRUE : FALSE;
		SerializeMetadata(*Archive, this, bEventIDsOnly);
		
		// Back to UNICODE now that we're done
		Archive->SetForceUnicode(TRUE); 

		bSuccess = !Archive->IsError();
	}

	return bSuccess;
}

/** Logs a game defined string event relating to a player
 * 
 * @param EventID - the identifier defining the event to record
 * @param Player - the player this event relates to
 * @param EventString - the string to record
 */
void UGameplayEventsWriter::LogPlayerStringEvent(INT EventID, AController* Player, const FString& EventString)
{
	if (Archive)
	{
		FPlayerStringEvent PlayerStringEvent;
		FRotator Rotation(0,0,0);

		GetPlayerLocationAndRotation(Player, PlayerStringEvent.Location, Rotation);
		PlayerStringEvent.PlayerIndexAndYaw = PackInts(ResolvePlayerIndex(Player), Rotation.Yaw);
		PlayerStringEvent.PlayerPitchAndRoll = PackInts(Rotation.Pitch, Rotation.Roll);
		PlayerStringEvent.StringEvent = EventString;

		FGameEventHeader GameEvent(GET_PlayerString, EventID, GWorld->GetRealTimeSeconds());
		GameEvent.DataSize = PlayerStringEvent.GetDataSize();

		*Archive << GameEvent;
		PlayerStringEvent.Serialize(*Archive); 
		debugf(NAME_GameStats, TEXT("[%.3f]: PlayerString[%d]: Player: %s String: %s"), GameEvent.TimeStamp, EventID, *GetPlayerName(Player), *EventString);
	}
}

/**
 * Logs when a player leaves/joins a session
 *
 * @param EventId the login/logout event for the player
 * @param Player the player that joined/left
 * @param PlayerName the name of the player in question
 * @param PlayerId the net id of the player in question
 * @param bSplitScreen whether the player is on splitscreen
 */
void UGameplayEventsWriter::LogPlayerLoginChange(INT EventID, AController* Player, const FString& PlayerName, FUniqueNetId PlayerID, UBOOL bSplitScreen)
{
	if (Archive)
	{
		FPlayerLoginEvent PlayerLoginEvent;
		FRotator Rotation(0,0,0);

		GetPlayerLocationAndRotation(Player, PlayerLoginEvent.Location, Rotation);
		PlayerLoginEvent.PlayerIndexAndYaw = PackInts(ResolvePlayerIndex(Player), Rotation.Yaw);
		PlayerLoginEvent.PlayerPitchAndRoll = PackInts(Rotation.Pitch, Rotation.Roll);
		PlayerLoginEvent.bSplitScreen = bSplitScreen;

		FGameEventHeader GameEvent(GET_PlayerLogin, EventID, GWorld->GetRealTimeSeconds());
		GameEvent.DataSize = PlayerLoginEvent.GetDataSize();

		*Archive << GameEvent;
		PlayerLoginEvent.Serialize(*Archive);
		debugf(NAME_GameStats, TEXT("[%.3f]: PlayerLogin[%d]: %s"), GameEvent.TimeStamp, EventID, *GetPlayerName(Player));
	}
}

/**
 * Logs a player killing and a player being killed
 *
 * @param EventId the event that should be written
 * @param KillType the additional information about a kill (another valid event id)
 * @param Killer the player that did the killing
 * @param DmgType the damage type that was done
 * @param Dead the player that was killed
 */
void UGameplayEventsWriter::LogPlayerKillDeath(INT EventID, INT KillType, AController* Killer, UClass* DmgType, AController* Dead)
{
	if (Archive)
	{				  
		FPlayerKillDeathEvent PlayerKillDeathEvent;
		FRotator Rotation(0,0,0);

		GetPlayerLocationAndRotation(Killer, PlayerKillDeathEvent.PlayerLocation, Rotation);
		PlayerKillDeathEvent.PlayerIndexAndYaw = PackInts(ResolvePlayerIndex(Killer), Rotation.Yaw);
		PlayerKillDeathEvent.PlayerPitchAndRoll = PackInts(Rotation.Pitch, Rotation.Roll);

		GetPlayerLocationAndRotation(Dead, PlayerKillDeathEvent.TargetLocation, Rotation);
		PlayerKillDeathEvent.TargetIndexAndYaw = PackInts(ResolvePlayerIndex(Dead), Rotation.Yaw);
		PlayerKillDeathEvent.TargetPitchAndRoll = PackInts(Rotation.Pitch, Rotation.Roll);

		PlayerKillDeathEvent.DamageClassIndex = ResolveDamageClassIndex(DmgType);
		PlayerKillDeathEvent.KillType = KillType;

		FGameEventHeader GameEvent(GET_PlayerKillDeath, EventID, GWorld->GetRealTimeSeconds());
		GameEvent.DataSize = PlayerKillDeathEvent.GetDataSize();

		*Archive << GameEvent;
		PlayerKillDeathEvent.Serialize(*Archive);
		debugf(NAME_GameStats, TEXT("[%.3f]: PlayerKillDeath[%d]: Player: %s Target: %s"), GameEvent.TimeStamp, EventID, *GetPlayerName(Killer), *GetPlayerName(Dead));
	}
}

/**
 * Logs an event with an integer value associated with it
 *
 * @param EventId the event being logged
 * @param Player the player that triggered the event
 * @param Value the value for this event
 */
void UGameplayEventsWriter::LogPlayerIntEvent(INT EventID, AController* Player, INT Value)
{
	if (Archive)
	{
		FPlayerIntEvent PlayerIntEvent;
		FRotator Rotation(0,0,0);

		GetPlayerLocationAndRotation(Player, PlayerIntEvent.Location, Rotation);
		PlayerIntEvent.PlayerIndexAndYaw = PackInts(ResolvePlayerIndex(Player), Rotation.Yaw);
		PlayerIntEvent.PlayerPitchAndRoll = PackInts(Rotation.Pitch, Rotation.Roll);
		
		PlayerIntEvent.Value = Value;

		FGameEventHeader GameEvent(GET_PlayerInt, EventID, GWorld->GetRealTimeSeconds());
		GameEvent.DataSize = PlayerIntEvent.GetDataSize();
		
		*Archive << GameEvent;
		PlayerIntEvent.Serialize(*Archive);
		debugf(NAME_GameStats, TEXT("[%.3f]: PlayerInt[%d]: Player: %s Value: %d"), GameEvent.TimeStamp, EventID, *GetPlayerName(Player), Value);
	}
}

/**
 * Logs an event with an float value associated with it
 *
 * @param EventId the event being logged
 * @param Player the player that triggered the event
 * @param Value the value for this event
 */
void UGameplayEventsWriter::LogPlayerFloatEvent(INT EventID, AController* Player, FLOAT Value)
{
	if (Archive)
	{
		FPlayerFloatEvent PlayerFloatEvent;
		FRotator Rotation(0,0,0);

		GetPlayerLocationAndRotation(Player, PlayerFloatEvent.Location, Rotation);
		PlayerFloatEvent.PlayerIndexAndYaw = PackInts(ResolvePlayerIndex(Player), Rotation.Yaw);
		PlayerFloatEvent.PlayerPitchAndRoll = PackInts(Rotation.Pitch, Rotation.Roll);

		PlayerFloatEvent.Value = Value;

		FGameEventHeader GameEvent(GET_PlayerFloat, EventID, GWorld->GetRealTimeSeconds());
		GameEvent.DataSize = PlayerFloatEvent.GetDataSize();

		*Archive << GameEvent;
		PlayerFloatEvent.Serialize(*Archive);
		debugf(NAME_GameStats, TEXT("[%.3f]: PlayerFloat[%d]: Player: %s Value: %d"), GameEvent.TimeStamp, EventID, *GetPlayerName(Player), Value);
	}
}

/**
 * Logs a spawn event for a player (team, class, etc)
 *
 * @param EventId the event being logged
 * @param Player the player that triggered the event
 * @param PawnClass the pawn this player spawned with
 * @param Team the team the player is on
 */
void UGameplayEventsWriter::LogPlayerSpawnEvent(INT EventID, AController* Player, UClass* PawnClass, INT TeamID)
{
	if (Archive)
	{
		FPlayerSpawnEvent PlayerSpawnEvent;
		FRotator Rotation(0,0,0);

		GetPlayerLocationAndRotation(Player, PlayerSpawnEvent.Location, Rotation);
		PlayerSpawnEvent.PlayerIndexAndYaw = PackInts(ResolvePlayerIndex(Player), Rotation.Yaw);
		PlayerSpawnEvent.PlayerPitchAndRoll = PackInts(Rotation.Pitch, Rotation.Roll);

		PlayerSpawnEvent.PawnClassIndex = ResolvePawnIndex(PawnClass);
		INT TeamIndex = ResolveTeamIndex((Player != NULL && Player->PlayerReplicationInfo != NULL) ? Player->PlayerReplicationInfo->Team : NULL);
		PlayerSpawnEvent.TeamIndex = TeamIndex != INDEX_NONE ? TeamIndex : TeamID;

		FGameEventHeader GameEvent(GET_PlayerSpawn, EventID, GWorld->GetRealTimeSeconds());
		GameEvent.DataSize = PlayerSpawnEvent.GetDataSize();
		
		*Archive << GameEvent;
		PlayerSpawnEvent.Serialize(*Archive);
		debugf(NAME_GameStats, TEXT("[%.3f]: PlayerSpawn[%d]: Player: %s Team: %d"), GameEvent.TimeStamp, EventID, *GetPlayerName(Player), TeamID);
	}
}

/**
 * Logs a weapon event with an integer value associated with it
 *
 * @param EventId the event being logged
 * @param Player the player that triggered the event
 * @param WeaponClass the weapon class associated with the event
 * @param Value the value for this event
 */
void UGameplayEventsWriter::LogWeaponIntEvent(INT EventID, AController* Player, UClass* WeaponClass, INT Value)
{
	if (Archive)
	{
		FWeaponIntEvent WeaponIntEvent;
		FRotator Rotation(0,0,0);

		GetPlayerLocationAndRotation(Player, WeaponIntEvent.Location, Rotation);
		WeaponIntEvent.PlayerIndexAndYaw = PackInts(ResolvePlayerIndex(Player), Rotation.Yaw);
		WeaponIntEvent.PlayerPitchAndRoll = PackInts(Rotation.Pitch, Rotation.Roll);

		WeaponIntEvent.WeaponClassIndex = ResolveWeaponClassIndex(WeaponClass);
		WeaponIntEvent.Value = Value;

		FGameEventHeader GameEvent(GET_WeaponInt, EventID, GWorld->GetRealTimeSeconds());
		GameEvent.DataSize = WeaponIntEvent.GetDataSize();

		*Archive << GameEvent;
		WeaponIntEvent.Serialize(*Archive);
		debugf(NAME_GameStats, TEXT("[%.3f]: WeaponInt[%d]: Player: %s Value: %d"), GameEvent.TimeStamp, EventID, *GetPlayerName(Player), Value);
	}
}

/**
 * Logs an int based game event
 *
 * @param EventId the event being logged
 * @param Value the value associated with the event
 */
void UGameplayEventsWriter::LogGameIntEvent(INT EventID, INT Value)
{
	if (Archive)
	{
		FGameIntEvent GameIntEvent(Value);

		FGameEventHeader GameEvent(GET_GameInt, EventID, GWorld->GetRealTimeSeconds());
		GameEvent.DataSize = GameIntEvent.GetDataSize();

		*Archive << GameEvent;
		GameIntEvent.Serialize(*Archive);
		debugf(NAME_GameStats, TEXT("[%.3f]: GameInt[%d]: Value: %d"), GameEvent.TimeStamp, EventID, Value);
	}
}

/**
 * Logs an float game event
 *
 * @param EventId the event being logged
 * @param Value the value associated with the event
 */
void UGameplayEventsWriter::LogGameFloatEvent(INT EventID, FLOAT Value)
{
	if (Archive)
	{
		FGameFloatEvent GameFloatEvent(Value);

		FGameEventHeader GameEvent(GET_GameFloat, EventID, GWorld->GetRealTimeSeconds());
		GameEvent.DataSize = GameFloatEvent.GetDataSize();

		*Archive << GameEvent;
		GameFloatEvent.Serialize(*Archive);
		debugf(NAME_GameStats, TEXT("[%.3f]: GameFloat[%d]: Value: %f"), GameEvent.TimeStamp, EventID, Value);
	}
}

/**
 * Logs a position based game event
 *
 * @param EventId the event being logged
 * @param Position the position of the event
 * @param Value the value associated with the event
 */
void UGameplayEventsWriter::LogGamePositionEvent(INT EventID, const FVector& Position, FLOAT Value)
{
	if (Archive)
	{
		FGamePositionEvent GamePositionEvent(Position, Value);

		FGameEventHeader GameEvent(GET_GamePosition, EventID, GWorld->GetRealTimeSeconds());
		GameEvent.DataSize = GamePositionEvent.GetDataSize();

		*Archive << GameEvent;
		GamePositionEvent.Serialize(*Archive);
		debugf(NAME_GameStats, TEXT("[%.3f]: GamePosition[%d]: Value: %f"), GameEvent.TimeStamp, EventID, Value);
	}
}

/**
 * Logs an int based team event
 *
 * @param EventId - the event being logged
 * @param Team - the team associated with this event
 * @param Value - the value associated with the event
 */
void UGameplayEventsWriter::LogTeamIntEvent(INT EventID, ATeamInfo* Team, INT Value)
{
	if (Archive)
	{
		INT TeamIndex = ResolveTeamIndex(Team);
		FTeamIntEvent TeamIntEvent(TeamIndex, Value);

		FGameEventHeader GameEvent(GET_TeamInt, EventID, GWorld->GetRealTimeSeconds());
		GameEvent.DataSize = TeamIntEvent.GetDataSize();

		*Archive << GameEvent;
		TeamIntEvent.Serialize(*Archive);
		debugf(NAME_GameStats, TEXT("[%.3f]: TeamInt[%d]: Team: %d Value: %d"), GameEvent.TimeStamp, EventID, TeamIndex, Value);
	}
}

/**
 * Logs a float based team event
 *
 * @param EventId - the event being logged
 * @param Team - the team associated with this event
 * @param Value - the value associated with the event
 */
void UGameplayEventsWriter::LogTeamFloatEvent(INT EventID, ATeamInfo* Team, FLOAT Value)
{
	if (Archive)
	{
		INT TeamIndex = ResolveTeamIndex(Team);
		FTeamFloatEvent TeamFloatEvent(TeamIndex, Value);

		FGameEventHeader GameEvent(GET_TeamFloat, EventID, GWorld->GetRealTimeSeconds());
		GameEvent.DataSize = TeamFloatEvent.GetDataSize();

		*Archive << GameEvent;
		TeamFloatEvent.Serialize(*Archive);
		debugf(NAME_GameStats, TEXT("[%.3f]: TeamFloat[%d]: Team: %d Value: %f"), GameEvent.TimeStamp, EventID, TeamIndex, Value);
	}
}

/**
 * Logs a string based team event
 *
 * @param EventId - the event being logged
 * @param Team - the team associated with this event
 * @param Value - the value associated with the event
 */
void UGameplayEventsWriter::LogTeamStringEvent(INT EventID, ATeamInfo* Team, const FString& Value)
{
	if (Archive)
	{
		INT TeamIndex = ResolveTeamIndex(Team);
		FTeamStringEvent TeamStringEvent(TeamIndex, Value);

		FGameEventHeader GameEvent(GET_TeamString, EventID, GWorld->GetRealTimeSeconds());
		GameEvent.DataSize = TeamStringEvent.GetDataSize();

		*Archive << GameEvent;
		TeamStringEvent.Serialize(*Archive);
		debugf(NAME_GameStats, TEXT("[%.3f]: TeamString[%d]: Team: %d Value: %s"), GameEvent.TimeStamp, EventID, TeamIndex, *Value);
	}
}

/**
 * Logs a string based game event
 *
 * @param EventId the event being logged
 * @param Value the value associated with the event
 */
void UGameplayEventsWriter::LogGameStringEvent(INT EventID, const FString& Value)
{
	if (Archive)
	{
		FGameStringEvent GameStringEvent(Value);

		FGameEventHeader GameEvent(GET_GameString, EventID, GWorld->GetRealTimeSeconds());
		GameEvent.DataSize = GameStringEvent.GetDataSize();

		*Archive << GameEvent;
		GameStringEvent.Serialize(*Archive);
		debugf(NAME_GameStats, TEXT("[%.3f]: GameString[%d]: Value: %s"), GameEvent.TimeStamp, EventID, *Value);
	}
}

/**
 * Logs damage with the amount that was done and to whom it was done
 *
 * @param EventId the event being logged
 * @param Player the player that triggered the event
 * @param DmgType the damage type that was done
 * @param Target the player being damaged
 * @param Amount the amount of damage done
 */
void UGameplayEventsWriter::LogDamageEvent(INT EventID, AController* Player, UClass* DmgType, AController* Target, INT Amount)
{
	if (Archive)
	{
		FDamageIntEvent DamageIntEvent;
		FRotator Rotation(0,0,0);

		GetPlayerLocationAndRotation(Player, DamageIntEvent.PlayerLocation, Rotation);
		DamageIntEvent.PlayerIndexAndYaw = PackInts(ResolvePlayerIndex(Player), Rotation.Yaw);
		DamageIntEvent.PlayerPitchAndRoll = PackInts(Rotation.Pitch, Rotation.Roll);

		GetPlayerLocationAndRotation(Target, DamageIntEvent.TargetLocation, Rotation);
		DamageIntEvent.TargetPlayerIndexAndYaw = PackInts(ResolvePlayerIndex(Target), Rotation.Yaw);
		DamageIntEvent.TargetPlayerPitchAndRoll = PackInts(Rotation.Pitch, Rotation.Roll);

		DamageIntEvent.DamageClassIndex = ResolveDamageClassIndex(DmgType);
		DamageIntEvent.Value = Amount;

		FGameEventHeader GameEvent(GET_DamageInt, EventID, GWorld->GetRealTimeSeconds());
		GameEvent.DataSize = DamageIntEvent.GetDataSize();
		
		*Archive << GameEvent;
		DamageIntEvent.Serialize(*Archive);
		debugf(NAME_GameStats, TEXT("[%.3f]: DamageInt[%d]: Player: %s Target: %s Value: %d"), GameEvent.TimeStamp, EventID, *GetPlayerName(Player), *GetPlayerName(Target), Amount);
	}
}

/**
 * Logs a player to player event
 *
 * @param EventId the event that should be written
 * @param Player the player that triggered the event
 * @param Target the player that was the recipient
 */
void UGameplayEventsWriter::LogPlayerPlayerEvent(INT EventID, AController* Player, AController* Target)
{
	if (Archive)
	{
		FPlayerPlayerEvent PlayerPlayerEvent;
		FRotator Rotation(0,0,0);

		GetPlayerLocationAndRotation(Player, PlayerPlayerEvent.PlayerLocation, Rotation);
		PlayerPlayerEvent.PlayerIndexAndYaw = PackInts(ResolvePlayerIndex(Player), Rotation.Yaw);
		PlayerPlayerEvent.PlayerPitchAndRoll = PackInts(Rotation.Pitch, Rotation.Roll);

		GetPlayerLocationAndRotation(Target, PlayerPlayerEvent.TargetLocation, Rotation);
		PlayerPlayerEvent.TargetIndexAndYaw = PackInts(ResolvePlayerIndex(Target), Rotation.Yaw);
		PlayerPlayerEvent.TargetPitchAndRoll = PackInts(Rotation.Pitch, Rotation.Roll);

		FGameEventHeader GameEvent(GET_PlayerPlayer, EventID, GWorld->GetRealTimeSeconds());
		GameEvent.DataSize = PlayerPlayerEvent.GetDataSize();
		
		*Archive << GameEvent;
		PlayerPlayerEvent.Serialize(*Archive);
		debugf(NAME_GameStats, TEXT("[%.3f]: PlayerPlayer[%d]: Player: %s Target: %s"), GameEvent.TimeStamp, EventID, *GetPlayerName(Player), *GetPlayerName(Target));
	}
}

/**
 * Logs the location of all players when this event occurred 
 *
 * @param EventId the event being logged
 */
void UGameplayEventsWriter::LogAllPlayerPositionsEvent(INT EventID)
{
	if (Archive)
	{
		FPlayerLocationsEvent PlayerLocationsEvent;
		FPlayerLocation PlayerLocation;
		FRotator Rotation(0,0,0);
		for (AController *Controller = GWorld->GetWorldInfo()->ControllerList; Controller != NULL; Controller = Controller->NextController)
		{
			// Don't log spectators, just wastes space
			if (Controller->PlayerReplicationInfo && Controller->Pawn != NULL)
			{
				GetPlayerLocationAndRotation(Controller, PlayerLocation.Location, Rotation);
				PlayerLocation.PlayerIndexAndYaw = PackInts(ResolvePlayerIndex(Controller), Rotation.Yaw);
				PlayerLocation.PlayerPitchAndRoll = PackInts(Rotation.Pitch, Rotation.Roll);
				PlayerLocationsEvent.PlayerLocations.AddItem(PlayerLocation);
			}
		}

		const INT PlayerLocationCount = PlayerLocationsEvent.PlayerLocations.Num();
		if (PlayerLocationCount > 0)
		{
			FGameEventHeader GameEvent(GET_PlayerLocationPoll, EventID, GWorld->GetRealTimeSeconds());
			GameEvent.DataSize = PlayerLocationsEvent.GetDataSize();

			*Archive << GameEvent;
			PlayerLocationsEvent.Serialize(*Archive);
			debugf(NAME_GameStats, TEXT("[%.3f]: PlayerLocationPoll[%d]: %d players"), GameEvent.TimeStamp, EventID, PlayerLocationCount);
		}
	}
}

/**
 * Logs system related information at a regular interval
 */
void UGameplayEventsWriter::LogSystemPollEvents()
{
	extern FLOAT GAverageFPS, GUnit_RenderThreadTime, GUnit_GPUFrameTime, GUnit_GameThreadTime, GUnit_FrameTime;
	LogGameIntEvent(UCONST_GAMEEVENT_FRAMERATE_POLL, INT(GAverageFPS));

	check(GEngine);
	APlayerController* PC = GEngine->GamePlayers.Num() > 0 && GEngine->GamePlayers(0) != NULL
		? GEngine->GamePlayers(0)->Actor
		: NULL;
	if (PC != NULL && PC->Pawn)
	{
#if !DEDICATED_SERVER
		LogGamePositionEvent(UCONST_GAMEEVENT_RENDERTHREAD_POLL, PC->Pawn->Location, GUnit_RenderThreadTime);
#endif
		LogGamePositionEvent(UCONST_GAMEEVENT_GAMETHREAD_POLL, PC->Pawn->Location, GUnit_GameThreadTime);
#if (!CONSOLE && !DEDICATED_SERVER) || (!FINAL_RELEASE && STATS)
		LogGamePositionEvent(UCONST_GAMEEVENT_GPUFRAMETIME_POLL, PC->Pawn->Location, GUnit_GPUFrameTime);
#endif
		LogGamePositionEvent(UCONST_GAMEEVENT_FRAMETIME_POLL, PC->Pawn->Location, GUnit_FrameTime);
	}
}

/**
 * Logs a projectile event with an integer value associated with it
 *
 * @param EventId the event being logged
 * @param Player the player that triggered the event
 * @param ProjClass the projectile class associated with the event
 * @param Value the value for this event
 */
void UGameplayEventsWriter::LogProjectileIntEvent(INT EventID, AController* Player, UClass* ProjClass, INT Value)
{
	if (Archive)
	{
		FProjectileIntEvent ProjectileIntEvent;
		FRotator Rotation(0,0,0);

		GetPlayerLocationAndRotation(Player, ProjectileIntEvent.Location, Rotation);
		ProjectileIntEvent.PlayerIndexAndYaw = PackInts(ResolvePlayerIndex(Player), Rotation.Yaw);
		ProjectileIntEvent.PlayerPitchAndRoll = PackInts(Rotation.Pitch, Rotation.Roll);

		ProjectileIntEvent.ProjectileClassIndex = ResolveProjectileClassIndex(ProjClass);
		ProjectileIntEvent.Value = Value;

		FGameEventHeader GameEvent(GET_ProjectileInt, EventID, GWorld->GetRealTimeSeconds());
		GameEvent.DataSize = ProjectileIntEvent.GetDataSize();

		*Archive << GameEvent;
		ProjectileIntEvent.Serialize(*Archive);
		debugf(NAME_GameStats, TEXT("[%.3f]: ProjectileInt[%d]: Player: %s Value: %d"), GameEvent.TimeStamp, EventID, *GetPlayerName(Player), Value);
	}
}

UGenericParamListStatEntry* UGameplayEventsWriter::GetGenericParamListEntry()
{
	if(Archive)
	{
		UGenericParamListStatEntry* NewEvt = Cast<UGenericParamListStatEntry>(StaticConstructObject(UGenericParamListStatEntry::StaticClass(),this));
		NewEvt->Writer = this;
		NewEvt->StatEvent = new FGenericParamListEvent();
		return NewEvt;
	}

	return NULL;
}


/************************************************************************/
/* Script accessible version of generic stats entry                     */
/************************************************************************/
// setters for supported data types
void UGenericParamListStatEntry::AddFloat(FName ParamName, FLOAT Value)
{
	if( StatEvent != NULL )
	{
		StatEvent->SetNamedParamData<FLOAT>(ParamName,Value);
	}
}
void UGenericParamListStatEntry::AddInt(FName ParamName, INT Value)
{
	if( StatEvent != NULL )
	{
		StatEvent->SetNamedParamData<INT>(ParamName,Value);
	}
}
void UGenericParamListStatEntry::AddVector(FName ParamName, FVector Value)
{
	if( StatEvent != NULL )
	{
		StatEvent->SetNamedParamData<FVector>(ParamName,Value);
	}
}
void UGenericParamListStatEntry::AddString(FName ParamName, const FString& Value)
{
	if( StatEvent != NULL )
	{
		StatEvent->SetNamedParamData<FString>(ParamName,Value);
	}
}

// getters for supported data types
UBOOL UGenericParamListStatEntry::GetFloat(FName ParamName, FLOAT& out_Float)
{
	if( StatEvent != NULL )
	{
		return StatEvent->GetNamedParamData<FLOAT>(ParamName,out_Float);
	}
	return FALSE;
}
UBOOL UGenericParamListStatEntry::GetInt(FName ParamName, INT& out_int)
{
	if( StatEvent != NULL )
	{
		return StatEvent->GetNamedParamData<INT>(ParamName,out_int);
	}
	return FALSE;
}
UBOOL UGenericParamListStatEntry::GetVector(FName ParamName, FVector& out_vector)
{
	if( StatEvent != NULL )
	{
		return StatEvent->GetNamedParamData<FVector>(ParamName,out_vector);
	}
	return FALSE;
}
UBOOL UGenericParamListStatEntry::GetString(FName ParamName, FString& out_string)
{
	if( StatEvent != NULL )
	{
		return StatEvent->GetNamedParamData<FString>(ParamName,out_string);
	}
	return FALSE;
}

// will write this event to disk
void UGenericParamListStatEntry::CommitToDisk()
{
	if(Writer != NULL && StatEvent != NULL)
	{
		INT EventID = UCONST_GAMEEVENT_GENERIC_PARAM_LIST_START;
		StatEvent->GetNamedParamData<INT>(TEXT("EventID"),EventID);
		FGameEventHeader GameEvent(GET_GenericParamList, EventID, GWorld->GetRealTimeSeconds());
		GameEvent.DataSize = StatEvent->GetDataSize();
		*Writer->Archive << GameEvent;
		StatEvent->Serialize(*Writer->Archive);

		Writer=NULL;
		delete StatEvent;
		StatEvent = NULL;
	}
}

void UGenericParamListStatEntry::BeginDestroy()
{
	if( StatEvent != NULL )
	{
		delete StatEvent;
		StatEvent = NULL;
	}
	Super::BeginDestroy();
}

/*-----------------------------------------------------------------------------
	UGameplayEventsUploadAnalytics implementation
-----------------------------------------------------------------------------*/

/** 
 * Mark a new session, clear existing events, etc 
 * @param HeartbeatDelta - polling frequency (0 turns it off)
 */
void UGameplayEventsUploadAnalytics::StartLogging(FLOAT HeartbeatDelta)
{
	if (GIsGame && !GIsEditor && !CurrentSessionInfo.bGameplaySessionInProgress)
	{
		// Fill out session information
		if (SetupGameSessionInfo(CurrentSessionInfo, eventGetGameTypeId(), eventGetPlaylistId()))
		{
			Game = GWorld->GetGameInfo();

			// Setup the heartbeat
			if (HeartbeatDelta > 0.0f)
			{
				eventStartPolling(HeartbeatDelta);
			}
			else
			{
				eventStopPolling();
			}
		}
	}
}

/** 
 * Resets the session, clearing all event data, but keeps the session ID/Timestamp intact
 * @param HeartbeatDelta - polling frequency (0 turns it off)
 */
void UGameplayEventsUploadAnalytics::ResetLogging(FLOAT HeartbeatDelta)
{
	if (GIsGame && !GIsEditor)
	{	
		// Save the previous session information
		FGameSessionInformation OldSessionInfo = CurrentSessionInfo;

		// Fill out session information
		if (SetupGameSessionInfo(CurrentSessionInfo, eventGetGameTypeId(), eventGetPlaylistId()))
		{		
			// Restore the previous Timestamp and SessionID
			CurrentSessionInfo.GameplaySessionTimestamp = OldSessionInfo.GameplaySessionTimestamp;
			CurrentSessionInfo.GameplaySessionID = OldSessionInfo.GameplaySessionID;
			CurrentSessionInfo.SessionInstance = OldSessionInfo.SessionInstance + 1;

			// Restart polling for stats if heartbeat was specified
			if (HeartbeatDelta > 0.0f)
			{
				eventStartPolling(HeartbeatDelta);
			}
			else
			{
				eventStopPolling();
			}
		}
	}
}

/** 
 * Mark the end of a logging session
 * closes file, stops polling, etc
 */
void UGameplayEventsUploadAnalytics::EndLogging()
{
	if (GIsGame && !GIsEditor && CurrentSessionInfo.bGameplaySessionInProgress)
	{
		Game = NULL;

		// End heartbeat polling since this session is finished
		eventStopPolling();

		// Mark end of session
		CurrentSessionInfo.GameplaySessionEndTime = GWorld->GetRealTimeSeconds();
		CurrentSessionInfo.bGameplaySessionInProgress = false;
	}
}

/**
* Logs a int based game event
*
* @param EventId the event being logged
* @param Value the value associated with the event
*/
void UGameplayEventsUploadAnalytics::LogGameIntEvent(INT EventID,INT Value)
{
	if (CurrentSessionInfo.bGameplaySessionInProgress)
	{
		UAnalyticEventsBase* Analytics = UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton();
		const FGameplayEventMetaData& EventMeta = GetEventMetaData(EventID);
		Analytics->LogStringEventParam(
			EventMeta.EventName.ToString(),
			TEXT("Value"),
			FString::Printf(TEXT("%d"),Value),
			false);
	}
}

/**
* Logs a string based game event
*
* @param EventId the event being logged
* @param Value the value associated with the event
*/
void UGameplayEventsUploadAnalytics::LogGameStringEvent(INT EventID,const FString& Value)
{
	if (CurrentSessionInfo.bGameplaySessionInProgress)
	{
		UAnalyticEventsBase* Analytics = UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton();
		const FGameplayEventMetaData& EventMeta = GetEventMetaData(EventID);
		Analytics->LogStringEventParam(
			EventMeta.EventName.ToString(),
			TEXT("Value"),
			Value,
			false);
	}
}

/**
* Logs a float based game event
*
* @param EventId the event being logged
* @param Value the value associated with the event
*/
void UGameplayEventsUploadAnalytics::LogGameFloatEvent(INT EventID,FLOAT Value)
{
	if (CurrentSessionInfo.bGameplaySessionInProgress)
	{
		UAnalyticEventsBase* Analytics = UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton();
		const FGameplayEventMetaData& EventMeta = GetEventMetaData(EventID);
		Analytics->LogStringEventParam(
			EventMeta.EventName.ToString(),
			TEXT("Value"),
			FString::Printf(TEXT("%.2f"),Value),
			false);
	}
}

/**
* Logs a position based game event
*
* @param EventId the event being logged
* @param Position the position of the event
* @param Value the value associated with the event
*/
void UGameplayEventsUploadAnalytics::LogGamePositionEvent(INT EventID,const FVector& Position,FLOAT Value)
{
	if (CurrentSessionInfo.bGameplaySessionInProgress)
	{
		UAnalyticEventsBase* Analytics = UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton();
		const FGameplayEventMetaData& EventMeta = GetEventMetaData(EventID);
		TArray<FEventStringParam> Params;
		Params.AddItem(FEventStringParam(TEXT("PositionX"),FString::Printf(TEXT("%.2f"),Position.X)));
		Params.AddItem(FEventStringParam(TEXT("PositionY"),FString::Printf(TEXT("%.2f"),Position.Y)));
		Params.AddItem(FEventStringParam(TEXT("PositionZ"),FString::Printf(TEXT("%.2f"),Position.Z)));
		Params.AddItem(FEventStringParam(TEXT("Value"),FString::Printf(TEXT("%.2f"),Value)));
		Analytics->LogStringEventParamArray(EventMeta.EventName.ToString(),Params,false);
	}
}

/**
* Logs a int based team event
*
* @param EventId - the event being logged
* @param Team - the team associated with this event
* @param Value - the value associated with the event
*/
void UGameplayEventsUploadAnalytics::LogTeamIntEvent(INT EventID,class ATeamInfo* Team,INT Value)
{
	if (CurrentSessionInfo.bGameplaySessionInProgress &&
		Team != NULL)
	{
		UAnalyticEventsBase* Analytics = UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton();
		const FGameplayEventMetaData& EventMeta = GetEventMetaData(EventID);		
		TArray<FEventStringParam> Params;
		Params.AddItem(FEventStringParam(TEXT("Team"),FString::Printf(TEXT("%d"),Team->TeamIndex)));
		Params.AddItem(FEventStringParam(TEXT("Value"),FString::Printf(TEXT("%d"),Value)));
		Analytics->LogStringEventParamArray(EventMeta.EventName.ToString(),Params,false);
	}
}

/**
* Logs a float based team event
*
* @param EventId - the event being logged
* @param Team - the team associated with this event
* @param Value - the value associated with the event
*/
void UGameplayEventsUploadAnalytics::LogTeamFloatEvent(INT EventID,class ATeamInfo* Team,FLOAT Value)
{
	if (CurrentSessionInfo.bGameplaySessionInProgress &&
		Team != NULL)
	{
		UAnalyticEventsBase* Analytics = UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton();
		const FGameplayEventMetaData& EventMeta = GetEventMetaData(EventID);		
		TArray<FEventStringParam> Params;
		Params.AddItem(FEventStringParam(TEXT("Team"),FString::Printf(TEXT("%d"),Team->TeamIndex)));
		Params.AddItem(FEventStringParam(TEXT("Value"),FString::Printf(TEXT("%.2f"),Value)));
		Analytics->LogStringEventParamArray(EventMeta.EventName.ToString(),Params,false);
	}
}

/**
* Logs a string based team event
*
* @param EventId - the event being logged
* @param Team - the team associated with this event
* @param Value - the value associated with the event
*/
void UGameplayEventsUploadAnalytics::LogTeamStringEvent(INT EventID,class ATeamInfo* Team,const FString& Value)
{
	if (CurrentSessionInfo.bGameplaySessionInProgress &&
		Team != NULL)
	{
		UAnalyticEventsBase* Analytics = UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton();
		const FGameplayEventMetaData& EventMeta = GetEventMetaData(EventID);		
		TArray<FEventStringParam> Params;
		Params.AddItem(FEventStringParam(TEXT("Team"),FString::Printf(TEXT("%d"),Team->TeamIndex)));
		Params.AddItem(FEventStringParam(TEXT("Value"),Value));
		Analytics->LogStringEventParamArray(EventMeta.EventName.ToString(),Params,false);
	}
}

/**
* Logs an event with an integer value associated with it
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param Value the value for this event
*/
void UGameplayEventsUploadAnalytics::LogPlayerIntEvent(INT EventID,class AController* Player,INT Value)
{
	if (CurrentSessionInfo.bGameplaySessionInProgress &&
		Player != NULL)
	{
		UAnalyticEventsBase* Analytics = UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton();
		const FGameplayEventMetaData& EventMeta = GetEventMetaData(EventID);
		TArray<FEventStringParam> Params;
		Params.AddItem(FEventStringParam(TEXT("Player"),GetPlayerName(Player)));
		Params.AddItem(FEventStringParam(TEXT("Value"),FString::Printf(TEXT("%d"),Value)));
		Analytics->LogStringEventParamArray(EventMeta.EventName.ToString(),Params,false);
	}
}

/**
* Logs an event with an float value associated with it
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param Value the value for this event
*/
void UGameplayEventsUploadAnalytics::LogPlayerFloatEvent(INT EventID,class AController* Player,FLOAT Value)
{
	if (CurrentSessionInfo.bGameplaySessionInProgress &&
		Player != NULL)
	{
		UAnalyticEventsBase* Analytics = UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton();
		const FGameplayEventMetaData& EventMeta = GetEventMetaData(EventID);
		TArray<FEventStringParam> Params;
		Params.AddItem(FEventStringParam(TEXT("Player"),GetPlayerName(Player)));
		Params.AddItem(FEventStringParam(TEXT("Value"),FString::Printf(TEXT("%.2f"),Value)));
		Analytics->LogStringEventParamArray(EventMeta.EventName.ToString(),Params,false);
	}
}

/**
* Logs an event with an string value associated with it
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param EventString the value for this event
*/
void UGameplayEventsUploadAnalytics::LogPlayerStringEvent(INT EventID,class AController* Player,const FString& EventString)
{
	if (CurrentSessionInfo.bGameplaySessionInProgress &&
		Player != NULL)
	{
		UAnalyticEventsBase* Analytics = UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton();
		const FGameplayEventMetaData& EventMeta = GetEventMetaData(EventID);
		TArray<FEventStringParam> Params;
		Params.AddItem(FEventStringParam(TEXT("Player"),GetPlayerName(Player)));
		Params.AddItem(FEventStringParam(TEXT("String"),EventString));
		Analytics->LogStringEventParamArray(EventMeta.EventName.ToString(),Params,false);
	}
}

/**
* Logs a spawn event for a player (team, class, etc)
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param PawnClass the pawn this player spawned with
* @param Team the team the player is on
*/
void UGameplayEventsUploadAnalytics::LogPlayerSpawnEvent(INT EventID,class AController* Player,class UClass* PawnClass,INT TeamID)
{
	if (CurrentSessionInfo.bGameplaySessionInProgress &&
		Player != NULL &&
		PawnClass != NULL)
	{
		UAnalyticEventsBase* Analytics = UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton();
		const FGameplayEventMetaData& EventMeta = GetEventMetaData(EventID);
		TArray<FEventStringParam> Params;
		Params.AddItem(FEventStringParam(TEXT("Player"),GetPlayerName(Player)));
		Params.AddItem(FEventStringParam(TEXT("Class"),PawnClass->GetName()));
		Params.AddItem(FEventStringParam(TEXT("Team"),FString::Printf(TEXT("%d"),TeamID)));
		Analytics->LogStringEventParamArray(EventMeta.EventName.ToString(),Params,false);
	}
}

/**
* Logs when a player leaves/joins a session
*
* @param EventId the login/logout event for the player
* @param Player the player that joined/left
* @param PlayerName the name of the player in question
* @param PlayerId the net id of the player in question
* @param bSplitScreen whether the player is on splitscreen
*/
void UGameplayEventsUploadAnalytics::LogPlayerLoginChange(INT EventID,class AController* Player,const FString& PlayerName,struct FUniqueNetId PlayerID,UBOOL bSplitScreen)
{
	if (CurrentSessionInfo.bGameplaySessionInProgress &&
		Player != NULL)
	{
		UAnalyticEventsBase* Analytics = UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton();
		const FGameplayEventMetaData& EventMeta = GetEventMetaData(EventID);
		TArray<FEventStringParam> Params;
		Params.AddItem(FEventStringParam(TEXT("Player"),GetPlayerName(Player)));
		Params.AddItem(FEventStringParam(TEXT("Splitscreen"),FString::Printf(TEXT("%s"),bSplitScreen ? TEXT("true") : TEXT("false"))));
		Analytics->LogStringEventParamArray(EventMeta.EventName.ToString(),Params,false);
	}
}

/**
* Logs the location of all players when this event occurred 
*
* @param EventId the event being logged
*/
void UGameplayEventsUploadAnalytics::LogAllPlayerPositionsEvent(INT EventID)
{
	//@todo - not implemented
}

/**
* Logs a player killing and a player being killed
*
* @param EventId the event that should be written
* @param KillType the additional information about a kill
* @param Killer the player that did the killing
* @param DmgType the damage type that was done
* @param Dead the player that was killed
*/
void UGameplayEventsUploadAnalytics::LogPlayerKillDeath(INT EventID,INT KillType,class AController* Killer,class UClass* dmgType,class AController* Dead)
{
	if (CurrentSessionInfo.bGameplaySessionInProgress &&
		Killer != NULL &&
		dmgType != NULL &&
		Dead != NULL)
	{
		UAnalyticEventsBase* Analytics = UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton();
		const FGameplayEventMetaData& EventMeta = GetEventMetaData(EventID);
		TArray<FEventStringParam> Params;
		Params.AddItem(FEventStringParam(TEXT("Killer"),GetPlayerName(Killer)));
		Params.AddItem(FEventStringParam(TEXT("Dead"),GetPlayerName(Dead)));
		Params.AddItem(FEventStringParam(TEXT("Damage"),dmgType->GetName()));
		Params.AddItem(FEventStringParam(TEXT("Type"),FString::Printf(TEXT("%d"),KillType)));
		Analytics->LogStringEventParamArray(EventMeta.EventName.ToString(),Params,false);
	}
}

/**
* Logs a player to player event
*
* @param EventId the event that should be written
* @param Player the player that triggered the event
* @param Target the player that was the recipient
*/
void UGameplayEventsUploadAnalytics::LogPlayerPlayerEvent(INT EventID,class AController* Player,class AController* Target)
{
	if (CurrentSessionInfo.bGameplaySessionInProgress &&
		Player != NULL &&
		Target != NULL)
	{
		UAnalyticEventsBase* Analytics = UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton();
		const FGameplayEventMetaData& EventMeta = GetEventMetaData(EventID);
		TArray<FEventStringParam> Params;
		Params.AddItem(FEventStringParam(TEXT("Player"),GetPlayerName(Player)));
		Params.AddItem(FEventStringParam(TEXT("Target"),GetPlayerName(Target)));
		Analytics->LogStringEventParamArray(EventMeta.EventName.ToString(),Params,false);
	}
}

/**
* Logs a weapon event with an integer value associated with it
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param WeaponClass the weapon class associated with the event
* @param Value the value for this event
*/
void UGameplayEventsUploadAnalytics::LogWeaponIntEvent(INT EventID,class AController* Player,class UClass* WeaponClass,INT Value)
{
	if (CurrentSessionInfo.bGameplaySessionInProgress &&
		Player != NULL &&
		WeaponClass != NULL)
	{
		UAnalyticEventsBase* Analytics = UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton();
		const FGameplayEventMetaData& EventMeta = GetEventMetaData(EventID);
		TArray<FEventStringParam> Params;
		Params.AddItem(FEventStringParam(TEXT("Player"),GetPlayerName(Player)));
		Params.AddItem(FEventStringParam(TEXT("Weapon"),WeaponClass->GetName()));
		Params.AddItem(FEventStringParam(TEXT("Value"),FString::Printf(TEXT("%d"),Value)));
		Analytics->LogStringEventParamArray(EventMeta.EventName.ToString(),Params,false);
	}
}

/**
* Logs damage with the amount that was done and to whom it was done
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param DmgType the damage type that was done
* @param Target the player being damaged
* @param Amount the amount of damage done
*/
void UGameplayEventsUploadAnalytics::LogDamageEvent(INT EventID,class AController* Player,class UClass* dmgType,class AController* Target,INT Amount)
{
	if (CurrentSessionInfo.bGameplaySessionInProgress &&
		Player != NULL &&
		dmgType != NULL &&
		Target != NULL)
	{
		UAnalyticEventsBase* Analytics = UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton();
		const FGameplayEventMetaData& EventMeta = GetEventMetaData(EventID);
		TArray<FEventStringParam> Params;
		Params.AddItem(FEventStringParam(TEXT("Player"),GetPlayerName(Player)));
		Params.AddItem(FEventStringParam(TEXT("Target"),GetPlayerName(Target)));
		Params.AddItem(FEventStringParam(TEXT("Damage"),dmgType->GetName()));
		Params.AddItem(FEventStringParam(TEXT("Amount"),FString::Printf(TEXT("%d"),Amount)));
		Analytics->LogStringEventParamArray(EventMeta.EventName.ToString(),Params,false);
	}
}

/**
* Logs a projectile event with an integer value associated with it
*
* @param EventId the event being logged
* @param Player the player that triggered the event
* @param Proj the projectile class associated with the event
* @param Value the value for this event
*/
void UGameplayEventsUploadAnalytics::LogProjectileIntEvent(INT EventID,class AController* Player,class UClass* Proj,INT Value)
{
	if (CurrentSessionInfo.bGameplaySessionInProgress &&
		Player != NULL &&
		Proj != NULL)
	{
		UAnalyticEventsBase* Analytics = UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton();
		const FGameplayEventMetaData& EventMeta = GetEventMetaData(EventID);
		TArray<FEventStringParam> Params;
		Params.AddItem(FEventStringParam(TEXT("Player"),GetPlayerName(Player)));
		Params.AddItem(FEventStringParam(TEXT("Projectile"),Proj->GetName()));
		Params.AddItem(FEventStringParam(TEXT("Value"),FString::Printf(TEXT("%d"),Value)));
		Analytics->LogStringEventParamArray(EventMeta.EventName.ToString(),Params,false);
	}
}

