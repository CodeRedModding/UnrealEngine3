/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __GAMEPLAYEVENTSUTILITIES_H__
#define __GAMEPLAYEVENTSUTILITIES_H__

#define GAME_STATS_FILE_EXT TEXT(".gamestats")
#define GAME_STATS_REPORT_FILE_EXT TEXT(".xml")
#define GAME_STATS_REPORT_IMAGE_FILE_EXT TEXT(".bmp")

/** Game Stats Versioning (similar to UnObjVer.h) */
#define INITIAL_VERSION 1
#define ADDED_MAPNAME_MAPURL_TO_GAMESESSIONINFO 2
#define ADDED_KILLTYPE_TO_KILLSTAT 3
#define ADDED_ACTOR_ARRAY 4
#define ADDED_SPLITSCREEN_TO_LOGIN 5
#define ADDED_UNIQUEID_TO_PLAYERINFO 6
#define ADDED_GROUPID_EVENT_DATATYPE 7
#define ADDED_SOUNDCUE_ARRAY 8
#define ADDED_SESSION_INSTANCE 9
#define ADDED_SUPPORTEDEVENTS_AGGREGATEOFFSET 10
#define ADDED_SUPPORTEDEVENTS_STRIPPING 11
#define REMOVED_UNIQUEID_HASH 12
#define ADDED_OWNINGNETID_GAMETYPEID 13
#define ADDED_PLAYLISTID 14

#define GAMESTATS_MIN_VER		ADDED_MAPNAME_MAPURL_TO_GAMESESSIONINFO
#define GAMESTATS_LATEST_VER	ADDED_PLAYLISTID

/**
* Return the directory where stats files are typically stored 
* @param OutStatsDir - out string where the directory is stored
*/
inline void GetStatsDirectory(FString& OutStatsDir)
{
	OutStatsDir = appGameDir() + TEXT("Stats") + PATH_SEPARATOR;

	//Test on GearGame
	//OutStatsDir = FString::Printf( TEXT("..") PATH_SEPARATOR TEXT("..") PATH_SEPARATOR TEXT("%sGame") PATH_SEPARATOR, TEXT("Gear") ) + TEXT("Stats") + PATH_SEPARATOR;
}

/** 
*  Get a unique filename for use in recording gameplay stats
* @return Filename to use this session
*/
inline FString GetUniqueStatsFilename()
{
	FString OutputDir;
	GetStatsDirectory(OutputDir);

	extern FString CreateProfileFilename( const FString& InFileExtension, UBOOL bIncludeDateForDirectoryName );
	const FString StatFilename = CreateProfileFilename( GAME_STATS_FILE_EXT, FALSE );
	return OutputDir + StatFilename;
}

/** Pack two 16-bit values into one 32-bit INT */
inline INT PackInts(INT HiWord, INT LoWord)
{
	return ((HiWord & 0xffff) << 16) | (LoWord & 0xffff);
}

/** Unpack two 16-bit values from one 32-bit INT */
inline void UnpackInt(INT PackedVal, INT& HiWord, INT& LoWord)
{
   HiWord = (PackedVal >> 16) & 0xffff;
   LoWord = (PackedVal & 0xffff);
}

/**
 *  Given a compressed data representation for player index and rotation, uncompress into original form
 * @param IndexAndYaw - 32-bit value containing Index (HiWord) and Yaw (LoWord)
 * @param PitchAndRoll - 32-bit value containing Pitch (HiWord) and Roll (LoWord)
 * @param PlayerIndex - out value for player index
 * @param Rotation - out value of player rotation
 */
void ConvertToPlayerIndexAndRotation(const INT IndexAndYaw, const INT PitchAndRoll, INT& PlayerIndex, FRotator& Rotation);

/** 
 * Helper to return a valid name from a controller 
 * @param Player controller of interest
 * @return String for the player name
 */
FString GetPlayerName(AController* Player);

/** 
 * Helper to return the proper player location/rotation given a controller
 * @param Player controller of interest
 * @param Location output of the pawn location if available, else controller location
 * @param Rotation output of the pawn rotation if available, else rotation location
 */
void GetPlayerLocationAndRotation(const AController* Player, FVector& Location, FRotator& Rotation);

/** Serialize the file header */
void SerializeGameplayEventsHeader(FArchive& Archive, struct FGameplayEventsHeader& FileHeader);

/** Serialize the FGameSessionInformation struct */
void SerializeGameSessionInfo(FArchive& Archive, struct FGameSessionInformation& GameSession);

/* 
 * Serializes the gameplay event metadata, saving only the event ID and datatype
 * This type of serialization is not expected to be ever loaded by the engine
 * for uploading minimal data to MCP in production
 */
void SerializeGameplayEventMetaData(FArchive& Ar, TArray<FGameplayEventMetaData>& GameplayEventMetaData);

/** Serialize all the metadata arrays */
void SerializeMetadata(FArchive& Archive, class UGameplayEvents* GameplaySession, UBOOL bEventIDsOnly=FALSE);

/** Serialization of the gameplay events supported meta data */
FArchive& operator<<(FArchive& Ar, FGameplayEventMetaData& GameplayEventMetaData);

/**
 * Helper for returning the actor name from the file reader
 *
 * @param ActorIndex the actor index to look up
 *
 * @return The name of the string if found
 */
FString GetActorNameFromIndex(INT ActorIndex);

/** Enumeration defining the header type in the file stream (changing order breaks backwards compatibility) */
enum EGameplayEventType
{
	GET_GameString = 0,
	GET_GameInt,
	GET_TeamInt,
	GET_PlayerInt,
	GET_PlayerFloat,
	GET_PlayerString,
	GET_PlayerSpawn,
	GET_PlayerLogin,
	GET_PlayerLocationPoll,
	GET_PlayerKillDeath,
	GET_PlayerPlayer,
	GET_WeaponInt,
	GET_DamageInt,
	GET_ProjectileInt,
	GET_GenericParamList,
	GET_GameFloat, 
	GET_TeamString,
	GET_TeamFloat,
	GET_GamePosition,
	GET_GameAggregate,
	GET_TeamAggregate,
	GET_PlayerAggregate,
	GET_WeaponAggregate,
	GET_DamageAggregate,
	GET_ProjectileAggregate,
	GET_PawnAggregate,
	GET_GameType = 1000 // Game Specific Values start after this
};

/**
* An object used to represent the type of a game event factory.
*/
class FGameEventType
{
public:

	/* Function definition to return proper class for serializer */
	typedef class IGameEvent* (*Factory)();

	/**
	 * @return The global event type list.
	 */
	static TLinkedList<FGameEventType*>*& GetTypeList()
	{
		/* @TODO - make this a TMap */
		static TLinkedList<FGameEventType*>* TypeList = NULL;
		return TypeList;
	}

	FGameEventType(const TCHAR* InTypeName, INT InGameType, Factory InFactoryFn) : TypeName(InTypeName), EventType(InGameType), FactoryFn(InFactoryFn)
	{
		// Add this game event type to the global list.
		(new TLinkedList<FGameEventType*>(this))->Link(GetTypeList());
	}

	/**
	 * Finds a FGameEventType by name.
	 */
	static IGameEvent* GetFactory(INT InGameType)
	{
		for(TLinkedList<FGameEventType*>::TIterator It(GetTypeList()); It; It.Next())
		{
			if (InGameType == It->EventType)
			{
				return It->FactoryFn();
			}
		}

		return NULL;
	}

	/** 
	 *   @return The event identifier
	 */
	INT GetTypeID() const
	{
		return EventType;
	}

private:

	/* User specified "friendly name" for this type of game event */
	FName TypeName;
	/* Unique identifier (EGameplayEventType) of the type of data this is */
	INT EventType;
	/* Function that will output the IGameEvent class */
	Factory  FactoryFn;
};

/**
 * A macro for declaring a new game event type, for use in the game event type definition body.
 */
#define DECLARE_GAMEPLAY_EVENT_TYPE(GameEventType) \
	public: \
	static FGameEventType StaticType; \
	virtual FGameEventType* GetType() const { return &StaticType; } \
	virtual INT GetTypeID() const { return StaticType.GetTypeID(); } \
	static IGameEvent* Factory() { static GameEventType Event;	return &Event;}	\


/**
 * A macro for implementing the static game event factory type object, and specifying parameters used by the type.
 */
#define IMPLEMENT_GAMEPLAY_EVENT_TYPE(GameEventStruct, GameEventName, GameEventType) \
	FGameEventType GameEventStruct::StaticType( TEXT(#GameEventName), \
												GameEventType, \
												GameEventStruct::Factory \
												);

/*  
 *  Interface for creating custom game event types 
 *  must define a function for serializing data and returning its size on disk
 *  and then use the two macros above DECLARE_GAMEPLAY_EVENT_TYPE/IMPLEMENT_GAMEPLAY_EVENT_TYPE
 */
class IGameEvent
{
public:

	/** Constructor / Destructor */
	IGameEvent() {}
	virtual ~IGameEvent() {}

	/** Serialize data into/out of the given archive for this data type */
	virtual void Serialize(FArchive& Ar) = 0;

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() = 0;
};

/** Base struct of a gameplay event, all event types must fill it out and serialize it into the stream before themselves **/
struct FGameEventHeader
{
	/** Type of game event struct following (unique per type) */
	INT EventType;
	/** The unique id of the event (16 bits clamped) */
	INT EventID;
	/** Time the event occurred relative to game start */
	FLOAT TimeStamp;
	/** Size of the data contained in this event */
	INT DataSize;

	/** Constructors */
	FGameEventHeader()
	{
		appMemzero(this, sizeof(FGameEventHeader));
	}

	/** Return the size in bytes of the serialized data */
	INT GetDataSize() { return sizeof(WORD) +
							   sizeof(WORD) +
							   sizeof(FLOAT) +
							   sizeof(WORD);  }

	/** Convenience constructor */
	FGameEventHeader(WORD InEventType, WORD InEventID, FLOAT InTimeStamp) 
	: EventType(InEventType), EventID(InEventID), TimeStamp(InTimeStamp), DataSize(0)
	{}

	/** Serialization of struct */
	friend FArchive& operator<<(FArchive& Ar, FGameEventHeader& GameEvent)
	{
		WORD TempWord;

		//Handles both loading from/saving to the right WORD type
		TempWord = GameEvent.EventType;
		Ar << TempWord;
		GameEvent.EventType = TempWord;

		TempWord = GameEvent.EventID;
		Ar << TempWord;
		GameEvent.EventID = TempWord;

		//FLOAT
		Ar << GameEvent.TimeStamp;

		TempWord = GameEvent.DataSize;
		Ar << TempWord;
		GameEvent.DataSize = TempWord;

		return Ar;
	}
};

/** A generic container for a string based game event, data is context dependent */
struct FGameStringEvent	: public IGameEvent
{
	/** String data associated with this event */
	FString StringEvent;

	/** Constructors */
	FGameStringEvent() {}
	FGameStringEvent(const FString& InString) : StringEvent(InString) {}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { INT StringLen = StringEvent.Len(); 
								return sizeof(INT) + (StringLen > 0 ? StringLen + 1 : 0) * sizeof(TCHAR); }

	/** Serialization of struct  */
	void Serialize(FArchive& Ar)
	{
		Ar << StringEvent; 
	}

	DECLARE_GAMEPLAY_EVENT_TYPE(FGameStringEvent);
};

/** A generic container for an int based game event, data is context dependent */
struct FGameIntEvent : public IGameEvent
{
	/** Value that has meaning in the context of a given EventID */
	INT Value;

	/** Constructors */
	FGameIntEvent() {}
	FGameIntEvent(INT InValue) : Value(InValue) {}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return sizeof(INT); }

	/** Serialization of struct  */
	void Serialize(FArchive& Ar)
	{
		Ar << Value; 
	}

	DECLARE_GAMEPLAY_EVENT_TYPE(FGameIntEvent);
};

/** A generic container for an float based game event, data is context dependent */
struct FGameFloatEvent : public IGameEvent
{
	/** Value that has meaning in the context of a given EventID */
	FLOAT Value;

	/** Constructors */
	FGameFloatEvent() {}
	FGameFloatEvent(FLOAT InValue) : Value(InValue) {}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return sizeof(FLOAT); }

	/** Serialization of struct  */
	void Serialize(FArchive& Ar)
	{
		Ar << Value; 
	}

	DECLARE_GAMEPLAY_EVENT_TYPE(FGameFloatEvent);
};

/** A generic container for an position based game event, data is context dependent */
struct FGamePositionEvent : public IGameEvent
{
	/** Location of the event **/
	FVector Location;
	/** Value that has meaning in the context of a given EventID */
	FLOAT Value;

	/** Constructors */
	FGamePositionEvent() {}
	FGamePositionEvent(const FVector& InLocation, FLOAT InValue) : Location(InLocation), Value(InValue) {}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return 4 * sizeof(FLOAT); }

	/** Serialization of struct  */
	void Serialize(FArchive& Ar)
	{
		Ar << Location << Value; 
	}

	DECLARE_GAMEPLAY_EVENT_TYPE(FGamePositionEvent);
};

/** A generic container for a string based team event, data is context dependent */
struct FTeamStringEvent	: public IGameEvent
{
	/** Index of the team relevant to this event (hi-word) */
	INT TeamIndex;
	/** String data associated with this event */
	FString StringEvent;

	/** Constructors */
	FTeamStringEvent() {}
	FTeamStringEvent(INT InTeamIndex, const FString& InString) : TeamIndex(InTeamIndex), StringEvent(InString) {}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { INT StringLen = StringEvent.Len(); 
								return sizeof(INT) +
									   sizeof(INT) + (StringLen > 0 ? StringLen + 1 : 0) * sizeof(TCHAR); }

	/** Serialization of struct  */
	void Serialize(FArchive& Ar)
	{
		Ar << TeamIndex << StringEvent; 
	}

	DECLARE_GAMEPLAY_EVENT_TYPE(FTeamStringEvent);
};

/** A generic container for an int based team event, data is context dependent */
struct FTeamIntEvent : public IGameEvent
{
	/** Index of the team relevant to this event (hi-word) */
	INT TeamIndex;
	/** Value that has meaning in the context of a given EventID */
	INT Value;

	/** Constructors */
	FTeamIntEvent() {}
	FTeamIntEvent(INT InTeamIndex, INT InValue) : TeamIndex(InTeamIndex), Value(InValue) {}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return sizeof(INT) +
									   sizeof(INT); }

	/** Serialization of struct  */
	void Serialize(FArchive& Ar)
	{
		Ar << TeamIndex << Value; 
	}

	DECLARE_GAMEPLAY_EVENT_TYPE(FTeamIntEvent);
};

/** A generic container for an float based team event, data is context dependent */
struct FTeamFloatEvent : public IGameEvent
{
	/** Index of the team relevant to this event (hi-word) */
	INT TeamIndex;
	/** Value that has meaning in the context of a given EventID */
	FLOAT Value;

	/** Constructors */
	FTeamFloatEvent() {}
	FTeamFloatEvent(INT InTeamIndex, FLOAT InValue) : TeamIndex(InTeamIndex), Value(InValue) {}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return sizeof(INT) + 
									   sizeof(FLOAT); }

	/** Serialization of struct  */
	void Serialize(FArchive& Ar)
	{
		Ar << Value; 
	}

	DECLARE_GAMEPLAY_EVENT_TYPE(FTeamFloatEvent);
};

/** A generic container for a string based player event, data is context dependent */
struct FPlayerStringEvent : public IGameEvent
{
	/** Index of the player relevant to this event (hi-word) and orientation (lo-word) */
	INT PlayerIndexAndYaw;
	/** Pitch (hi-word) and Roll (lo-word) of player */
	INT PlayerPitchAndRoll;
	/** String data associated with this event */
	FString StringEvent;
	/** Location of the event if relevant **/
	FVector Location;

	/** Constructors */
	FPlayerStringEvent() {}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() {	INT StringLen = StringEvent.Len(); 
								return sizeof(INT) +
									   sizeof(INT) + 
									   sizeof(INT) + (StringLen > 0 ? StringLen + 1 : 0) * sizeof(TCHAR) +
									   3 * sizeof(FLOAT); }
	
	/** Serialization of struct  */
	void Serialize(FArchive& Ar)
	{
		Ar << PlayerIndexAndYaw << PlayerPitchAndRoll << StringEvent << Location;
	}

	DECLARE_GAMEPLAY_EVENT_TYPE(FPlayerStringEvent);
};

/** A generic container for an int based player event, data is context dependent */
struct FPlayerIntEvent : public IGameEvent
{
	/** Index of the player relevant to this event (hi-word) and orientation (lo-word) */
	INT PlayerIndexAndYaw;
	/** Pitch (hi-word) and Roll (lo-word) of player */
	INT PlayerPitchAndRoll;
	/** Value that has meaning in the context of a given EventID */
	INT Value;
	/** Location of the event if relevant **/
	FVector Location;

	/** Constructors */
	FPlayerIntEvent() {}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return sizeof(INT) +
									   sizeof(INT) +
									   sizeof(INT) +
									   3 * sizeof(FLOAT); }

	/** Serialization of struct  */
	void Serialize(FArchive& Ar)
	{
		Ar << PlayerIndexAndYaw << PlayerPitchAndRoll << Value << Location;
	}

	DECLARE_GAMEPLAY_EVENT_TYPE(FPlayerIntEvent);
};

/** A generic container for a float based player event, data is context dependent */
struct FPlayerFloatEvent : public IGameEvent
{
	/** Index of the player relevant to this event (hi-word) and orientation (lo-word) */
	INT PlayerIndexAndYaw;
	/** Pitch (hi-word) and Roll (lo-word) of player */
	INT PlayerPitchAndRoll;
	/** Value that has meaning in the context of a given EventID */
	FLOAT Value;
	/** Location of the event if relevant **/
	FVector Location;

	/** Constructors */
	FPlayerFloatEvent()	{}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return sizeof(INT) +
									   sizeof(INT) +
									   sizeof(FLOAT) +
									   3 * sizeof(FLOAT); }

	/** Serialization of struct  */
	void Serialize(FArchive& Ar)
	{
		Ar << PlayerIndexAndYaw << PlayerPitchAndRoll << Value << Location;
	}

	DECLARE_GAMEPLAY_EVENT_TYPE(FPlayerFloatEvent);
};

/** A container for a player spawn event */
struct FPlayerSpawnEvent : public IGameEvent
{
	/** Index of the player relevant to this event (hi-word) and orientation (lo-word) */
	INT PlayerIndexAndYaw;
	/** Pitch (hi-word) and Roll (lo-word) of player */
	INT PlayerPitchAndRoll;
	/** Index to class of pawn spawned **/
	INT PawnClassIndex;
	/** Team identifier **/
	INT TeamIndex;
	/** Location of the event if relevant **/
	FVector Location;

	/** Constructors */
	FPlayerSpawnEvent() {}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return sizeof(INT) +
									   sizeof(INT) +
									   sizeof(INT) + 
									   sizeof(INT) +
									   3 * sizeof(FLOAT); }

	/** Serialization of struct  */
	void Serialize(FArchive& Ar)
	{
		Ar << PlayerIndexAndYaw << PlayerPitchAndRoll << PawnClassIndex << TeamIndex << Location;
	}

	DECLARE_GAMEPLAY_EVENT_TYPE(FPlayerSpawnEvent);
};

/** A container for a player login event */
struct FPlayerLoginEvent : public IGameEvent
{		 
	/** Index of the player relevant to this event (hi-word) and orientation (lo-word) */
	INT PlayerIndexAndYaw;
	/** Pitch (hi-word) and Roll (lo-word) of player */
	INT PlayerPitchAndRoll;
	/** Location of the event if relevant **/
	FVector Location;
	/** Whether the player is on SplitScreen */
	BYTE bSplitScreen;

	/** Constructors */
	FPlayerLoginEvent()	{}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return sizeof(INT) +
									   sizeof(INT) +
									   3 * sizeof(FLOAT) +
									   sizeof(BYTE); }

	/** Serialization of struct  */
	void Serialize(FArchive& Ar)
	{
		if (Ar.Ver() >= ADDED_SPLITSCREEN_TO_LOGIN)
		{
			Ar << PlayerIndexAndYaw << PlayerPitchAndRoll << Location << bSplitScreen;
		}
		else
		{
			Ar << PlayerIndexAndYaw << PlayerPitchAndRoll << Location;
			bSplitScreen = 255;
		}
	}

	DECLARE_GAMEPLAY_EVENT_TYPE(FPlayerLoginEvent);
};

/** Tuple of player index to player location */
struct FPlayerLocation
{
	INT PlayerIndexAndYaw;
	INT PlayerPitchAndRoll;
	FVector Location;
};

/** Serialization of struct */
inline FArchive& operator<<(FArchive& Ar, FPlayerLocation& PlayerLoc)
{
	return Ar << PlayerLoc.PlayerIndexAndYaw << PlayerLoc.PlayerPitchAndRoll << PlayerLoc.Location;
}

/** A container for a snapshot of all players in a moment in time */
struct FPlayerLocationsEvent : public IGameEvent
{
	/** Location of all players */
	TArray<FPlayerLocation> PlayerLocations;

	/** Constructors */
	FPlayerLocationsEvent() {}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return sizeof(INT) + PlayerLocations.Num() * sizeof(FPlayerLocation); }

	/** Serialization of struct  */
	void Serialize(FArchive& Ar)
	{
		Ar << PlayerLocations;
	}

	DECLARE_GAMEPLAY_EVENT_TYPE(FPlayerLocationsEvent);
};

/** A container for a player killing another player event */
struct FPlayerKillDeathEvent : public IGameEvent
{
	/** Index of the player relevant to this event (hi-word) and orientation (lo-word) */
	INT PlayerIndexAndYaw;
	/** Pitch (hi-word) and Roll (lo-word) of player */
	INT PlayerPitchAndRoll;
	/** Index of the killed player (hi-word) and orientation (lo-word) */
	INT TargetIndexAndYaw;
	/** Pitch (hi-word) and Roll (lo-word) of player */
	INT TargetPitchAndRoll;
	/** Index of the damage class used */
	INT DamageClassIndex;
	/** Location of the killer **/
	FVector PlayerLocation;
	/** Location of the killed **/
	FVector TargetLocation;
	/** Type of kill it was */
	INT KillType;

	/** Constructors */
	FPlayerKillDeathEvent() {}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return sizeof(INT) + 
									   sizeof(INT) +
									   sizeof(INT) + 
									   sizeof(INT) +
									   sizeof(INT) +
									   3 * sizeof(FLOAT) +
									   3 * sizeof(FLOAT) + 
									   sizeof(INT); }

	/** Serialization of struct  */
	void Serialize(FArchive& Ar)
	{
		Ar << PlayerIndexAndYaw << PlayerPitchAndRoll 
			<< TargetIndexAndYaw << TargetPitchAndRoll 
			<< DamageClassIndex 
			<< PlayerLocation << TargetLocation;

		if (Ar.Ver() >= ADDED_KILLTYPE_TO_KILLSTAT)
		{
			Ar << KillType;
		}
		else if (Ar.IsLoading())
		{
			KillType = UCONST_GAMEEVENT_PLAYER_KILL_NORMAL;
		}
	}

	DECLARE_GAMEPLAY_EVENT_TYPE(FPlayerKillDeathEvent);
};

/** A container for a player interacting with another player event */
struct FPlayerPlayerEvent : public IGameEvent
{
	/** Index of the player relevant to this event (hi-word) and orientation (lo-word) */
	INT PlayerIndexAndYaw;
	/** Pitch (hi-word) and Roll (lo-word) of player */
	INT PlayerPitchAndRoll;
	/** Index of the targeted player relevant to this event (hi-word) and orientation (lo-word) */
	INT TargetIndexAndYaw;
	/** Pitch (hi-word) and Roll (lo-word) of player */
	INT TargetPitchAndRoll;
	/** Location of the player if relevant **/
	FVector PlayerLocation;
	/** Location of the target if relevant **/
	FVector TargetLocation;

	/** Constructors */
	FPlayerPlayerEvent() {}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return sizeof(INT) +
		                               sizeof(INT) + 
									   sizeof(INT) +
									   sizeof(INT) +
									   3 * sizeof(FLOAT) +
									   3 * sizeof(FLOAT); }

	/** Serialization of struct  */
	void Serialize(FArchive& Ar)
	{
		Ar << PlayerIndexAndYaw << PlayerPitchAndRoll 
			<< TargetIndexAndYaw << TargetPitchAndRoll 
			<< PlayerLocation;
		
		if (Ar.Ver() >= ADDED_KILLTYPE_TO_KILLSTAT)
		{
			Ar << TargetLocation;
		}
		else if (Ar.IsLoading())
		{
			TargetLocation = FVector::ZeroVector;
		}
	}

	DECLARE_GAMEPLAY_EVENT_TYPE(FPlayerPlayerEvent);
};

/** A generic container for an int based weapon event, data is context dependent */
struct FWeaponIntEvent : public IGameEvent
{					
	/** Index of the player relevant to this event (hi-word) and orientation (lo-word) */
	INT PlayerIndexAndYaw;
	/** Pitch (hi-word) and Roll (lo-word) of player */
	INT PlayerPitchAndRoll;
	/** Index of relevant weapon */
	INT WeaponClassIndex;
	/** Data payload */
	INT Value;
	/** Location of the event if relevant **/
	FVector Location;

	/** Constructors */
	FWeaponIntEvent() {}
	
	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return sizeof(INT) +
									   sizeof(INT) +
								       sizeof(INT) +
									   sizeof(INT) +
									   3 * sizeof(FLOAT); }

	/** Serialization of struct  */
	void Serialize(FArchive& Ar)
	{
		Ar << PlayerIndexAndYaw << PlayerPitchAndRoll
			<< WeaponClassIndex << Value << Location;
	}

	DECLARE_GAMEPLAY_EVENT_TYPE(FWeaponIntEvent);
};

/** A generic container for an int based damage related event, data is context dependent */
struct FDamageIntEvent : public IGameEvent
{					 	
	/** Index of the player relevant to this event (hi-word) and orientation (lo-word) */
	INT PlayerIndexAndYaw;
	/** Pitch (hi-word) and Roll (lo-word) of player */
	INT PlayerPitchAndRoll;
	/** Index of the targeted player relevant to this event (hi-word) and orientation (lo-word) */
	INT TargetPlayerIndexAndYaw;
	/** Pitch (hi-word) and Roll (lo-word) of player */
	INT TargetPlayerPitchAndRoll;
	/** Index of damage type */
	INT DamageClassIndex;
	/** Data Payload */
	INT Value;
	/** Location of the event if relevant **/
	FVector PlayerLocation;
	/** Location of the event if relevant **/
	FVector TargetLocation;

	/** Constructors */
	FDamageIntEvent() {}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return sizeof(INT) +
								  	   sizeof(INT) + 
							           sizeof(INT) +
								       sizeof(INT) +
									   sizeof(INT) +
									   sizeof(INT) +
									   3 * sizeof(FLOAT) +
									   3 * sizeof(FLOAT); }

	/** Serialization of struct  */
	void Serialize(FArchive& Ar)
	{
		Ar << PlayerIndexAndYaw << PlayerPitchAndRoll 
			<< TargetPlayerIndexAndYaw << TargetPlayerPitchAndRoll 
			<< DamageClassIndex << Value
			<< PlayerLocation << TargetLocation;
	}

	DECLARE_GAMEPLAY_EVENT_TYPE(FDamageIntEvent);
};

/** A generic container for an int based projectile event, data is context dependent */
struct FProjectileIntEvent : public IGameEvent
{
	/** Index of the player relevant to this event (hi-word) and orientation (lo-word) */
	INT PlayerIndexAndYaw;
	/** Pitch (hi-word) and Roll (lo-word) of player */
	INT PlayerPitchAndRoll;
	/** Index of projectile type */
	INT ProjectileClassIndex;
	/** Data Payload */
	INT Value;
	/** Location of the event if relevant **/
	FVector Location;

	/** Constructors */
	FProjectileIntEvent() {}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return sizeof(INT) +
		                               sizeof(INT) +
									   sizeof(INT) +
									   sizeof(INT) +
									   3 * sizeof(FLOAT); }

	/** Serialization of struct  */
	void Serialize(FArchive& Ar)
	{
		Ar << PlayerIndexAndYaw << PlayerPitchAndRoll << ProjectileClassIndex << Value << Location;
	}

	DECLARE_GAMEPLAY_EVENT_TYPE(FProjectileIntEvent);
};

/** A generic container for an event containing a named parameter list */
enum ESupportedParamTypes
{
	 ESPT_FLOAT
	,ESPT_INT
	,ESPT_FVector
	,ESPT_FString
	,ESPT_NIL
};

#define DECL_PARAM_TYPE(PARAM_TYPE) \
	ESupportedParamTypes GetDataType(const PARAM_TYPE& In)\
	{\
		return ESPT_##PARAM_TYPE;\
	}



/** container for a single property stored within the genericparamlistevent */
struct NamedParameter
{
	/**
	 * get the size of a string as serialized on disk DERP
	 * @param Str - the string to get the size of
	 * @return bytes the string will occupy on disk
	 */
	INT GetStringDiskSizeDerp(FString& Str)
	{
		return Str.GetCharArray().Num()*sizeof(TCHAR) + sizeof(INT);
	}

	/**
	 * gets the serialized size of an FName on disk DERP
	 * @param Name - the name to get teh size of (derp)
	 * @return bytes the fname will occupy on disk
	 */
	INT GetNameDiskSizeDerp(FName& Name)
	{
		FString Str = Name.GetNameString();
		return GetStringDiskSizeDerp(Str) + sizeof(INT);
	}

	/**
	 * serialize an FName on disk
	 */
	inline void SerializeName(FArchive& Ar, FName& N)
	{
		// we cannot safely use the name index as that is not guaranteed to persist across game sessions
		FString NameText;
		if (Ar.IsSaving())
		{
			NameText = N.GetNameString();
		}
		INT Number = N.GetNumber();

		Ar << NameText;
		Ar << Number;

		if (Ar.IsLoading())
		{
			// copy over the name with a name made from the name string and number
			N = FName(*NameText, Number);
		}
	}

	/** name of this param */
	FName ParamName;
	
	/** actual data for this param */
	TArray<BYTE> Payload;

	/** type of this param (for type checking) */
	ESupportedParamTypes ParamType;

	NamedParameter(FName InName) :
		ParamName(InName)
		,ParamType(ESPT_NIL)
	{
	}

	DECL_PARAM_TYPE(FLOAT)
	DECL_PARAM_TYPE(FString)
	DECL_PARAM_TYPE(FVector)
	DECL_PARAM_TYPE(INT)

	/**
	 * templated function to set the data associated with this parameter 
	 */
	template<class T> void SetData(T InData)
	{
		ESupportedParamTypes ThisParamType = GetDataType(InData);

		Payload.Empty(sizeof(T));
		Payload.AddZeroed(sizeof(T));
		ParamType=ThisParamType;
		T* Tmp = (T*)&Payload(0);
		*Tmp = InData;
	}

	/**
	 * return the typed data assoicated with this parameter 
	 */
	template<class T> T GetData()
	{
		return *(T*)(&Payload(0));
	}

	/**
	 * return the size on disk of this parameter
	 */
	INT GetDataSize()
	{
		FString* StringPayload;

		INT PayloadSize = 0;
		switch(ParamType)
		{
			case ESPT_FLOAT:
			case ESPT_INT:
			case ESPT_FVector:
				PayloadSize = Payload.Num();
				break;
			case ESPT_FString:
				StringPayload = (FString*)&Payload(0);
				PayloadSize = GetStringDiskSizeDerp(*StringPayload); 
				break;
			default:
				check(0 && "Invalid data type set!");
				PayloadSize = 0;
		}



		return PayloadSize + GetNameDiskSizeDerp(ParamName) + sizeof(WORD);
	}
#define LOAD_PARAM(PARAM_TYPE) \
	case ESPT_##PARAM_TYPE:\
	{\
		PARAM_TYPE Temp##PARAM_TYPE;\
		Ar << Temp##PARAM_TYPE;\
		SetData<PARAM_TYPE>(Temp##PARAM_TYPE);\
	}\
		break;

#define SAVE_PARAM(PARAM_TYPE) \
	case ESPT_##PARAM_TYPE:\
	{\
		PARAM_TYPE Temp##PARAM_TYPE = GetData<PARAM_TYPE>();\
		Ar << Temp##PARAM_TYPE;\
	}\
		break;

	/**
	 * serialize this parameter to disk 
	 */
	void Serialize(FArchive& Ar)
	{
		WORD intParamType = (WORD)ParamType;
		Ar << intParamType;
		ParamType =  (ESupportedParamTypes)intParamType;

		SerializeName(Ar,ParamName);

		if(Ar.IsLoading())
		{
			switch(ParamType)
			{
				LOAD_PARAM(FLOAT)
				LOAD_PARAM(INT)
				LOAD_PARAM(FVector)
				LOAD_PARAM(FString)
			default:
				check(0 && "Invalid data type set!");
				return;
			}
		}
		else if(Ar.IsSaving())
		{
			switch(ParamType)
			{
				SAVE_PARAM(FLOAT)
				SAVE_PARAM(INT)
				SAVE_PARAM(FVector)
				SAVE_PARAM(FString)
			default:
				check(0 && "Invalid data type set!");
				return;
			}
		}
	}

};

/**
 * stat event which can store an arbitrary number of params stuffed from script
 */
struct FGenericParamListEvent : public IGameEvent
{					
	TArray< NamedParameter > Params;

	/** Constructors */
	FGenericParamListEvent() {}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() 
	{ 
		INT Total=0;
		for(INT Idx=0;Idx<Params.Num();++Idx)
		{
			Total+=Params(Idx).GetDataSize();
		}
		return Total+sizeof(WORD);
	}

	/** Serialization of struct  */
	void Serialize(FArchive& Ar)
	{
		WORD NumParams=Params.Num();
		Ar << NumParams;
		if( Ar.IsLoading() )
		{
			Params.Empty(NumParams);
			Params.AddZeroed(NumParams);
		}
		for(INT Idx=0;Idx<Params.Num();++Idx)
		{
			NamedParameter& Param = Params(Idx);
			Param.Serialize(Ar);
		}
	}

	/**
	 * get the parameter assoicated with the name (if any)
	 * @param Name - name of the parameter w'er elooking for
	 * @param out_paramData - out param stuffed with teh data found if anything
	 * @return TRUE if a param is found
	 */
	template<class T> UBOOL GetNamedParamData(FName Name, T& out_ParamData)
	{
		for(INT Idx=0;Idx<Params.Num();++Idx)
		{
			NamedParameter& Param = Params(Idx);
			if(Param.ParamName == Name)
			{
				out_ParamData = Param.GetData<T>();
				return TRUE;
			}
		}

		// not found!
		return FALSE;
	}

	/**
	 * set the parameter of the passed name to the passed data (or create a new one if none exists)
	 * @param name - name of param to set 
	 * @param NewParamData - data to set
	 */
	template<class T> void SetNamedParamData(FName Name, T NewParamData)
	{
		for(INT Idx=0;Idx<Params.Num();++Idx)
		{
			NamedParameter& Param = Params(Idx);
			if(Param.ParamName == Name)
			{
				Param.SetData<T>(NewParamData);
				return;
			}
		}

		if(Params.Num() >= MAXWORD)
		{
			warnf(TEXT("Could not add parameter %s to FGenericParamListEvent.. reached max params of 255!"),*Name.ToString());
			return;
		}
		// not found, add a new entry
		NamedParameter NewParam(Name);
		NewParam.SetData<T>(NewParamData);
		Params.AddItem(NewParam);
	}

	DECLARE_GAMEPLAY_EVENT_TYPE(FGenericParamListEvent);
};

/** Aggregate container for storing a game event total */
struct FGameAggregate : public IGameEvent
{	
	/** Time period the aggregate applies to */
	INT TimePeriod;
	/** Total value of the event */
	INT Value;

	/** Constructor / Destructor */
	FGameAggregate() {}
	virtual ~FGameAggregate() {}

	/** Serialize data into/out of the given archive for this data type */
	virtual void Serialize(FArchive& Ar)
	{
		Ar << TimePeriod << Value;
	}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return 2 * sizeof(INT);}

	DECLARE_GAMEPLAY_EVENT_TYPE(FGameAggregate);
};

/** Aggregate container for storing a team event total */
struct FTeamAggregate : public IGameEvent
{	
	/** Time period the aggregate applies to */
	INT TimePeriod;
	/** Team the aggregate applies to */
	INT TeamIndex;
	/** Total value of the event */
	INT Value;

	/** Constructor / Destructor */
	FTeamAggregate() {}
	virtual ~FTeamAggregate() {}

	/*
	 *   Interface for setting aggregate properties
	 * @param InPlayerIndex - the player index for this aggregate
	 * @param InClassIndex - the class index for whatever metadata this aggregate is referencing
	 * @param InTimePeriod - the time period this aggregate covers
	 * @param InValue - the aggregated value
	 */
	virtual void SetData(INT InPlayerIndex, INT InClassIndex, INT InTimePeriod, INT InValue)
	{
		TimePeriod = InTimePeriod;
		Value = InValue;
	}

	/** Serialize data into/out of the given archive for this data type */
	virtual void Serialize(FArchive& Ar)
	{
		Ar << TimePeriod << TeamIndex << Value;
	}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return 3 * sizeof(INT);}

	DECLARE_GAMEPLAY_EVENT_TYPE(FTeamAggregate);
};

/** Aggregate container for storing a player event total */
struct FPlayerAggregate : public IGameEvent
{	
	/** Time period the aggregate applies to */
	INT TimePeriod;
	/** Player the aggregate applies to */
	INT PlayerIndex;
	/** Total value of the event */
	INT Value;

	/** Constructor / Destructor */
	FPlayerAggregate() {}
	virtual ~FPlayerAggregate() {}

	/*
	 *   Interface for setting aggregate properties
	 * @param InPlayerIndex - the player index for this aggregate
	 * @param InClassIndex - the class index for whatever metadata this aggregate is referencing
	 * @param InTimePeriod - the time period this aggregate covers
	 * @param InValue - the aggregated value
	 */
	virtual void SetData(INT InPlayerIndex, INT InClassIndex, INT InTimePeriod, INT InValue)
	{
		PlayerIndex = InPlayerIndex;
		TimePeriod = InTimePeriod;
		Value = InValue;
	}

	/** Serialize data into/out of the given archive for this data type */
	virtual void Serialize(FArchive& Ar)
	{
		Ar << TimePeriod << PlayerIndex << Value;
	}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return 3 * sizeof(INT);}

	DECLARE_GAMEPLAY_EVENT_TYPE(FPlayerAggregate);
};

/** Aggregate container for storing a weapon event total */
struct FWeaponAggregate  : public FPlayerAggregate
{	
	/** Weapon index the aggregate applies to */
	INT WeaponIndex;

	/** Constructor / Destructor */
	FWeaponAggregate() {}
	virtual ~FWeaponAggregate() {}

	/*
	 *   Interface for setting aggregate properties
	 * @param InPlayerIndex - the player index for this aggregate
	 * @param InClassIndex - the class index for whatever metadata this aggregate is referencing
	 * @param InTimePeriod - the time period this aggregate covers
	 * @param InValue - the aggregated value
	 */
	virtual void SetData(INT InPlayerIndex, INT InClassIndex, INT InTimePeriod, INT InValue)
	{
		PlayerIndex = InPlayerIndex;
		WeaponIndex = InClassIndex;
		TimePeriod = InTimePeriod;
		Value = InValue;
	}

	/** Serialize data into/out of the given archive for this data type */
	virtual void Serialize(FArchive& Ar)
	{
		FPlayerAggregate::Serialize(Ar);
		Ar << WeaponIndex;
	}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return sizeof(INT) + FPlayerAggregate::GetDataSize();}

	DECLARE_GAMEPLAY_EVENT_TYPE(FWeaponAggregate);
};

/** Aggregate container for storing a damage event total */
struct FDamageAggregate  : public FPlayerAggregate
{	
	/** Damage index the aggregate applies to */
	INT DamageIndex;

	/** Constructor / Destructor */
	FDamageAggregate() {}
	virtual ~FDamageAggregate() {}

	/*
	 *   Interface for setting aggregate properties
	 * @param InPlayerIndex - the player index for this aggregate
	 * @param InClassIndex - the class index for whatever metadata this aggregate is referencing
	 * @param InTimePeriod - the time period this aggregate covers
	 * @param InValue - the aggregated value
	 */
	virtual void SetData(INT InPlayerIndex, INT InClassIndex, INT InTimePeriod, INT InValue)
	{
		PlayerIndex = InPlayerIndex;
		DamageIndex = InClassIndex;
		TimePeriod = InTimePeriod;
		Value = InValue;
	}

	/** Serialize data into/out of the given archive for this data type */
	virtual void Serialize(FArchive& Ar)
	{
		FPlayerAggregate::Serialize(Ar);
		Ar << DamageIndex;
	}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return sizeof(INT) + FPlayerAggregate::GetDataSize();}

	DECLARE_GAMEPLAY_EVENT_TYPE(FDamageAggregate);
};

/** Aggregate container for storing a projectile event total */
struct FProjectileAggregate  : public FPlayerAggregate
{	
	/** Projectile index the aggregate applies to */
	INT ProjectileIndex;

	/** Constructor / Destructor */
	FProjectileAggregate() {}
	virtual ~FProjectileAggregate() {}

	/*
	 *   Interface for setting aggregate properties
	 * @param InPlayerIndex - the player index for this aggregate
	 * @param InClassIndex - the class index for whatever metadata this aggregate is referencing
	 * @param InTimePeriod - the time period this aggregate covers
	 * @param InValue - the aggregated value
	 */
	virtual void SetData(INT InPlayerIndex, INT InClassIndex, INT InTimePeriod, INT InValue)
	{
		PlayerIndex = InPlayerIndex;
		ProjectileIndex = InClassIndex;
		TimePeriod = InTimePeriod;
		Value = InValue;
	}

	/** Serialize data into/out of the given archive for this data type */
	virtual void Serialize(FArchive& Ar)
	{
		FPlayerAggregate::Serialize(Ar);
		Ar << ProjectileIndex;
	}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return sizeof(INT) + FPlayerAggregate::GetDataSize();}

	DECLARE_GAMEPLAY_EVENT_TYPE(FProjectileAggregate);
};

/** Aggregate container for storing a projectile event total */
struct FPawnAggregate  : public FPlayerAggregate
{	
	/** Pawn index the aggregate applies to */
	INT PawnIndex;

	/** Constructor / Destructor */
	FPawnAggregate() {}
	virtual ~FPawnAggregate() {}

	/*
	 *   Interface for setting aggregate properties
	 * @param InPlayerIndex - the player index for this aggregate
	 * @param InClassIndex - the class index for whatever metadata this aggregate is referencing
	 * @param InTimePeriod - the time period this aggregate covers
	 * @param InValue - the aggregated value
	 */
	virtual void SetData(INT InPlayerIndex, INT InClassIndex, INT InTimePeriod, INT InValue)
	{
		PlayerIndex = InPlayerIndex;
		PawnIndex = InClassIndex;
		TimePeriod = InTimePeriod;
		Value = InValue;
	}

	/** Serialize data into/out of the given archive for this data type */
	virtual void Serialize(FArchive& Ar)
	{
		FPlayerAggregate::Serialize(Ar);
		Ar << PawnIndex;
	}

	/** Return the size in bytes of the serialized data */
	virtual INT GetDataSize() { return sizeof(INT) + FPlayerAggregate::GetDataSize();}

	DECLARE_GAMEPLAY_EVENT_TYPE(FPawnAggregate);
};

#endif //__GAMEPLAYEVENTSUTILITIES_H__
