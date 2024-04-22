/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This class holds a set of online interfaces that game code uses to talk
 * with the platform layer's services. The set of services are implemented
 * as interface pointers so that we can mix & match services at run time.
 * This also allows licensees the ability to use part of our base services
 * and provide custom implmentations of others.
 */
class OnlineSubsystem extends Object
	native
	abstract
	inherits(FTickableObject)
	dependson(Settings)
	config(Engine);

/** The interface to use for creating and/or enumerating account information */
var OnlineAccountInterface AccountInterface;

/** The interface for accessing online player methods */
var OnlinePlayerInterface PlayerInterface;

/** The interface for accessing online player extension methods */
var OnlinePlayerInterfaceEx PlayerInterfaceEx;

/** The interface for accessing system wide network functions */
var OnlineSystemInterface SystemInterface;

/** The interface to use for creating, searching for, or destroying online games */
var OnlineGameInterface GameInterface;

/** The interface to use for online content */
var OnlineContentInterface ContentInterface;

/** The interface to use for voice communication */
var OnlineVoiceInterface VoiceInterface;

/** The interface to use for stats read/write operations */
var OnlineStatsInterface StatsInterface;

/** The interface to use for reading game specific news announcements */
var OnlineNewsInterface NewsInterface;

/** The interface to use for accessing online party chat methods */
var OnlinePartyChatInterface PartyChatInterface;

/** The interface to use for downloading files from the network platform */
var OnlineTitleFileInterface TitleFileInterface;

/** The interface to use for checking the availability of cached files */
var OnlineTitleFileCacheInterface TitleFileCacheInterface;

/** The interface to use for reading and writing files to the cloud */
var UserCloudFileInterface UserCloudInterface;

/** The interface to use for sharing files on the cloud */
var SharedCloudFileInterface SharedCloudInterface;

/** The interface to use for posting to social networks */
var OnlineSocialInterface SocialInterface;

/** The interface to use for multiplayer authentication */
var OnlineAuthInterface AuthInterface;

/** Struct that holds a transient, unique identifier for a player */
struct native UniqueNetId
{
	/** The id used by the network to uniquely identify a player */
	var QWORD Uid;

	structcpptext
	{
		/**
		 * The noinit version of the ctor
		 */
		FORCEINLINE FUniqueNetId(void)
		{
		}

		/**
		 * Zeros the data in the struct
		 *
		 * @param ignored. only present to allow zeroing of data
		 */
		FORCEINLINE FUniqueNetId(EEventParm)
		{
			Uid = (QWORD)0;
		}

		/**
		 * Zeros the data in the struct
		 *
		 * @param ignored. only present to allow zeroing of data
		 */
		FORCEINLINE FUniqueNetId(DWORD)
		{
			Uid = (QWORD)0;
		}

		/**
		 * Initialize from a QWORD
		 *
		 * @param InQWORD	The QWORD the UID should be initialized to
		 */
		FORCEINLINE FUniqueNetId(QWORD InQWORD)
		{
			Uid = InQWORD;
		}

		/** Copy constructor */
		FORCEINLINE FUniqueNetId(const FUniqueNetId& Other)
		{
			Uid = Other.Uid;
		}

		/**
		 * Compares the IDs for equality
		 *
		 * @param Id the value to compare
		 *
		 * @returns TRUE if they are equal, FALSE otherwise
		 */
		FORCEINLINE UBOOL operator==(const FUniqueNetId& Id) const
		{
			return Uid == Id.Uid;
		}

		/**
		 * Compares the IDs for inequality
		 *
		 * @param Id the value to compare
		 *
		 * @returns TRUE if they are not equal, FALSE otherwise
		 */
		FORCEINLINE UBOOL operator!=(const FUniqueNetId& Id) const
		{
			return Uid != Id.Uid;
		}

		/**
		 * Converts a DWORD into our type in network byte order
		 *
		 * @param Id the value to assign
		 */
		FORCEINLINE FUniqueNetId& operator=(const DWORD Id)
		{
#if WITH_PANORAMA || XBOX || WITH_STEAMWORKS
			check(0 && "Bad conversion loses data");
#else
			Uid = Id;
#endif
			return *this;
		}

		/**
		 * Converts a QWORD into our type in network byte order
		 *
		 * @param Id the value to assign
		 */
		FORCEINLINE FUniqueNetId& operator=(const QWORD Id)
		{
			Uid = Id;
			return *this;
		}

		/**
		 * Converts the unique id to a DWORD
		 *
		 * @return unique id as a DWORD
		 */
		FORCEINLINE DWORD ToDWORD(void) const
		{
#if WITH_PANORAMA || XBOX || WITH_STEAMWORKS
			check(0 && "Bad conversion loses data");
			return 0;
#else
			return Uid & 0xFFFFFFFF;
#endif
		}

		/**
		 * Accessor for determining whether this net id has been assigned a value.
		 */
		FORCEINLINE UBOOL HasValue() const
		{
			return (QWORD)0 != Uid;
		}

		/** serialization interface */
		friend FArchive& operator<<(FArchive& Ar, FUniqueNetId& Id)
		{
			Ar << Id.Uid;
			return Ar;
		}
	}
};

/** The different login statuses for a player */
enum ELoginStatus
{
	/** Player has not logged in or chosen a local profile */
	LS_NotLoggedIn,
	/** Player is using a local profile but is not logged in */
	LS_UsingLocalProfile,
	/** Player has been validated by the platform specific authentication service */
	LS_LoggedIn
};

/** This enum indicates access to major features in the game (parent controls */
enum EFeaturePrivilegeLevel
{
	/** Parental controls have disabled this feature */
	FPL_Disabled,
	/** Parental controls allow this feature only with people on their friends list */
	FPL_EnabledFriendsOnly,
	/** Parental controls allow this feature everywhere */
	FPL_Enabled
};

/** Used to bulk query the friends list */
struct native FriendsQuery
{
	/** The unique player id to check friends status for */
	var UniqueNetId UniqueId;
	/** Out param indicating whether the player is a friend or not */
	var bool bIsFriend;
};

/** Indicates where network notifications should appear on the screen */
enum ENetworkNotificationPosition
{
	NNP_TopLeft,
	NNP_TopCenter,
	NNP_TopRight,
	NNP_CenterLeft,
	NNP_Center,
	NNP_CenterRight,
	NNP_BottomLeft,
	NNP_BottomCenter,
	NNP_BottomRight
};

/** Enum indicating the current state of the online game (in progress, ended, etc.) */
enum EOnlineGameState
{
	/** An online game has not been created yet */
	OGS_NoSession,
	/** Session has been created and the match hasn't started (pre match lobby) */
	OGS_Pending,
	/** Session has been asked to start (may take time due to communication with backend) */
	OGS_Starting,
	/** The current session has started. Matches with join in progress disabled are no longer joinable */
	OGS_InProgress,
	/** The session is still valid, but the match is no longer being played (post match lobby) */
	OGS_Ending,
	/** The session is closed and any stats committed */
	OGS_Ended
};

/** The state of an async enumeration (friends, content, etc) read request */
enum EOnlineEnumerationReadState
{
	OERS_NotStarted,
	OERS_InProgress,
	OERS_Done,
	OERS_Failed
};

/** The possible friend states for a friend entry */
enum EOnlineFriendState
{
	/** Not currently online */
	OFS_Offline,
	/** Signed in and online */
	OFS_Online,
	/** Signed in, online, and idle */
	OFS_Away,
	/** Signed in, online, and asks to be left alone */
	OFS_Busy
};

/** Holds information about a player in a friends list */
struct native OnlineFriend
{
	/** Unique identifier of the friend */
	var const UniqueNetId UniqueId;
	/** The session id that the player is in */
	var const qword SessionId;
	/** Player's nick as published to the online service */
	var const string NickName;
	/** String holding information about the player's game state (cap-ed flag, etc.) */
	var const string PresenceInfo;
	/** The current state for this friend */
	var const EOnlineFriendState FriendState;
	/** Whether the friend is online or not */
	var const bool bIsOnline;
	/** Whether the friend is playing a game or not */
	var const bool bIsPlaying;
	/** Whether the friend is playing the same game or not */
	var const bool bIsPlayingThisGame;
	/** Whether the game the friend is in is joinable or not */
	var const bool bIsJoinable;
	/** Whether the friend can chat via voice or not */
	var const bool bHasVoiceSupport;
	/** Whether the friends has an invitation outstanding from you or not*/
	var bool bHaveInvited;
	/** Whether the friends has sent an invite to you or not */
	var const bool bHasInvitedYou;

	structcpptext
	{
		/**
		 * Zeroes members
		 */
		FOnlineFriend(void)
		{
			appMemzero(this,sizeof(FOnlineFriend));
		}

		/**
		 * Copy constructor. Copies the other into this object
		 *
		 * @param Other the other structure to copy
		 */
		FOnlineFriend(const FOnlineFriend& Other)
		{
			appMemzero(this,sizeof(FOnlineFriend));
			appMemcpy(&UniqueId,&Other.UniqueId,sizeof(FUniqueNetId));
			SessionId = Other.SessionId;
			NickName = Other.NickName;
			PresenceInfo = Other.PresenceInfo;
			FriendState = Other.FriendState;
			bIsOnline = Other.bIsOnline;
			bIsPlaying = Other.bIsPlaying;
			bIsPlayingThisGame = Other.bIsPlayingThisGame;
			bIsJoinable = Other.bIsJoinable;
			bHasVoiceSupport = Other.bHasVoiceSupport;
			bHaveInvited = Other.bHaveInvited;
			bHasInvitedYou = Other.bHasInvitedYou;
		}

		/**
		 * Assignment operator. Copies the other into this object
		 *
		 * @param Other the other structure to copy
		 */
		FOnlineFriend& operator=(const FOnlineFriend& Other)
		{
			if (this != &Other)
			{
				appMemcpy(&UniqueId,&Other.UniqueId,sizeof(FUniqueNetId));
				SessionId = Other.SessionId;
				NickName = Other.NickName;
				PresenceInfo = Other.PresenceInfo;
				FriendState = Other.FriendState;
				bIsOnline = Other.bIsOnline;
				bIsPlaying = Other.bIsPlaying;
				bIsPlayingThisGame = Other.bIsPlayingThisGame;
				bIsJoinable = Other.bIsJoinable;
				bHasVoiceSupport = Other.bHasVoiceSupport;
				bHaveInvited = Other.bHaveInvited;
				bHasInvitedYou = Other.bHasInvitedYou;
			}
			return *this;
		}
	}
};

/** The type of player owned content */
enum EOnlineContentType
{
	/** Content downloaded digitally from the online market */
	OCT_Downloaded,
	/** Player's save game content */
	OCT_SaveGame
};

/** Holds information about a single piece of downloaded content */
struct native OnlineContent
{
	/** The type of content this represents */
	var EOnlineContentType ContentType;
	/** Optional user index that content is downloaded for (-1 means it's not associated with any user) */
	var byte UserIndex;
	/** Whether the package is corrupt or not */
	var bool bIsCorrupt;
	/** The device ID the DLC is on */
	var int DeviceId;
	/** The game specific license mask for this item */
	var int LicenseMask;
	/** Displayable name of the content */
	var string FriendlyName;
	/** Non-displayable file name of the content */
	var string Filename;
	/** File system usable reference to the content */
	var string ContentPath;
	/** List of packages in the content */
	var array<string> ContentPackages;
	/** List of all non-package files in the content */
	var array<string> ContentFiles;
};

/** Holds information about a single piece of cross title downloaded content */
struct native OnlineCrossTitleContent extends OnlineContent
{
	/** The title ID the content is from */
	var int TitleId;
};

/**
 * Indicates the connection status with the remote online servers
 */
enum EOnlineServerConnectionStatus
{
	/** Gracefully disconnected from the online servers */
	OSCS_NotConnected,
	/** Connected to the online servers just fine */
	OSCS_Connected,
	/** Connection was lost for some reason */
	OSCS_ConnectionDropped,
	/** Can't connect because of missing network connection */
	OSCS_NoNetworkConnection,
	/** Service is temporarily unavailable */
	OSCS_ServiceUnavailable,
	/** An update is required before connecting is possible */
	OSCS_UpdateRequired,
	/** Servers are too busy to handle the request right now */
	OSCS_ServersTooBusy,
	/** Disconnected due to duplicate login */
	OSCS_DuplicateLoginDetected,
	/** Can't connect because of an invalid/unknown user */
	OSCS_InvalidUser
};

/**
 * The various NAT types the player may have
 */
enum ENATType
{
	/** Unable to determine the NAT type */
	NAT_Unknown,
	/** Anyone can join without connectivity problems */
	NAT_Open,
	/** Most can join but might have problems with strict */
	NAT_Moderate,
	/** Will most likely have connectivity problems with strict/moderate */
	NAT_Strict
};

/** A player in a session */
struct native OnlineRegistrant
{
	/** Unique id of the player involved in the arbitrated session */
	var const UniqueNetId PlayerNetId;

	structcpptext
	{
		/**
		 * The noinit version of the ctor
		 */
		FORCEINLINE FOnlineRegistrant(void)
		{
		}

		/**
		 * Inits it to the player id specified
		 *
		 * @param InId sets the player id
		 */
		FORCEINLINE FOnlineRegistrant(const FUniqueNetId InId) :
			PlayerNetId(InId)
		{
		}

		/**
		 * Compares the player IDs for equality
		 *
		 * @param Id the value to compare
		 *
		 * @returns TRUE if they are equal, FALSE otherwise
		 */
		FORCEINLINE UBOOL operator==(const FOnlineRegistrant& Other) const
		{
			return PlayerNetId == Other.PlayerNetId;
		}
	}
};

/** Struct holding the information about a single arbitration registrant */
struct native OnlineArbitrationRegistrant extends OnlineRegistrant
{
	/** Unique id of the machine involved in the arbitrated session */
	var const qword MachineId;
	/** Trust level of the machine/player for the arbitrated session */
	var const int Trustworthiness;
};

/**
 * Holds a word/phrase that was recognized by the speech analyzer
 *
 * @note See VoiceInterface.h to change the native layout of this struct
 */
struct SpeechRecognizedWord
{
	/** The id of the word in the vocabulary */
	var int WordId;
	/** the actual word */
	var string WordText;
	/** How confident the analyzer was in the recognition */
	var float Confidence;
};

/** Indicates the state the LAN beacon is in */
enum ELanBeaconState
{
	/** The lan beacon is disabled */
	LANB_NotUsingLanBeacon,
	/** The lan beacon is responding to client requests for information */
	LANB_Hosting,
	/** The lan beacon is querying servers for information */
	LANB_Searching
};

/**
 * Struct holding information used when writing scoring information that is used
 * to determine a player's skill rating
 */
struct native OnlinePlayerScore
{
	/** The player that this score is for */
	var UniqueNetId PlayerId;
	/** The team that the player is on */
	var int TeamId;
	/** The score for this player */
	var int Score;
};

/** The series of status codes that the account creation method can return */
enum EOnlineAccountCreateStatus
{
	/** Created the account successfully */
	OACS_CreateSuccessful,
	/** Failed but no indication why */
	OACS_UnknownError,
	/** The user name is invalid */
	OACS_InvalidUserName,
	/** The password is invalid */
	OACS_InvalidPassword,
	/** The unique user name is invalid */
	OACS_InvalidUniqueUserName,
	/** The user name is invalid */
	OACS_UniqueUserNameInUse,
	/** The backend service is not reachable */
	OACS_ServiceUnavailable
};

/** Information about a local talker */
struct native LocalTalker
{
	/** Whether this talker is currently registered */
	var bool bHasVoice;
	/** Whether the talker should send network data */
	var bool bHasNetworkedVoice;
	/** Whether the player is trying to speak voice commands */
	var bool bIsRecognizingSpeech;
	/** Whether the local talker was speaking last frame */
	var bool bWasTalking;
	/** Whether the remote talker is speaking this frame */
	var bool bIsTalking;
	/** Whether this player was already registered with the voice interface or not */
	var bool bIsRegistered;
};

/** Information about a remote talker */
struct native RemoteTalker
{
	/** The unique id for this talker */
	var UniqueNetId TalkerId;
	/** Used to trigger talking delegates only after a certain period of time has passed */
	var float LastNotificationTime;
	/** Whether the remote talker was speaking last frame */
	var bool bWasTalking;
	/** Whether the remote talker is speaking this frame */
	var bool bIsTalking;
	/** Whether this player was already registered with the voice interface or not */
	var bool bIsRegistered;
};

/** Holds the data used in a friend message */
struct native OnlineFriendMessage
{
	/** The player that is sending the message */
	var UniqueNetId SendingPlayerId;
	/** The nick name of the player that sent the message */
	var string SendingPlayerNick;
	/** Whether this is a friend invite or just a generic message */
	var bool bIsFriendInvite;
	/** Whether this message is a game invite or not */
	var bool bIsGameInvite;
	/** Whether the invite has been accepted or not */
	var bool bWasAccepted;
	/** Whether the invite has been denied or not */
	var bool bWasDenied;
	/** The corresponding message that was sent */
	var string Message;
};

/** Holds a named object interface for dynamically bound interfaces */
struct native NamedInterface
{
	/** The name to bind this object to */
	var name InterfaceName;
	/** The object to store at this location */
	var Object InterfaceObject;
};

/** Holds the set of registered named interfaces */
var private array<NamedInterface> NamedInterfaces;

/** Holds a name to class name mapping for adding the named interfaces automatically */
struct native NamedInterfaceDef
{
	/** The name to bind this object to */
	var name InterfaceName;
	/** The class to load and create for the named interface */
	var string InterfaceClassName;
};

/** The list of named interfaces to automatically create and store */
var config array<NamedInterfaceDef> NamedInterfaceDefs;

/** Holds metadata about a given downloadable file */
struct native EmsFile
{
	/** Hash value, if applicable, of the given file contents */
	var string Hash;
	/** Filename as downloaded */
	var string DLName;
	/** Logical filename, maps to the downloaded filename */
	var string Filename;
	/** File size */
	var int FileSize;
};

/** Holds the data used in downloading a file asynchronously from the online service */
struct native TitleFile
{
	/** The name of the file as requested */
	var string Filename;
	/** The async state the file download is in */
	var EOnlineEnumerationReadState AsyncState;
	/** The buffer of data for the file */
	var array<byte> Data;
};

/** Community file info */
struct native CommunityContentFile
{
	/** Unique identifier for this content file */
	var int ContentId;
	/** Additional identifier for providers that need it */
	var int FileId;
	/** Game specific content type field */
	var int ContentType;
	/** Size in bytes of the payload in the file */
	var int FileSize;
	/** The original owner of the file */
	var UniqueNetId Owner;
	/** The number of times this file has been downloaded */
	var int DownloadCount;
	/** The average rating for this file from the community */
	var float AverageRating;
	/** The number of times this file has been rated */
	var int RatingCount;
	/** The last rating the user gave this content */
	var int LastRatingGiven;
	/** The path to the file if stored locally */
	var string LocalFilePath;
};

/** Community file metadata */
struct native CommunityContentMetadata
{
	/** Unique identifier for this content type */
	var int ContentType;
	/** Metadata to associate with the file */
	var array<SettingsProperty> MetadataItems;
};

/** Holds the per session information for named sessions */
struct native NamedSession
{
	/** The name of the session */
	var name SessionName;
	/** The platform specific session information */
	var const native transient pointer SessionInfo{void};
	/** The game settings associated with this session */
	var OnlineGameSettings GameSettings;
	/** List of players registered in the session */
	var array<OnlineRegistrant> Registrants;
	/** The set of registered players for arbitration */
	var array<OnlineArbitrationRegistrant> ArbitrationRegistrants;
};

/** The list of sessions currently managed by the online subsystem */
var const protected array<NamedSession> Sessions;

/** Holds the information contained in Live's achievements for this title */
struct native AchievementDetails
{
	/** The ID of the achievement */
	var const int Id;
	/** The name of the achievement */
	var const string AchievementName;
	/** The description of the achievement */
    var const string Description;
	/** The description of how to meet the requirements of the achievement */
    var const string HowTo;
    /** The image associated with the achievement */
	var Surface Image;
	/** The month of when it was earned */
	var const byte MonthEarned;
	/** The day of the month of when it was earned */
	var const byte DayEarned;
	/** The year of when it was earned (byte is fine since the none of the consoles will last that long :) ) */
	var const byte YearEarned;
	/** The day of week since we have a byte available for padding */
	var const byte DayOfWeekEarned;
	/** How much this achievement is worth */
	var const int GamerPoints;
	/** Whether the achievement is secret (hidden if not achieved) or not */
	var const bool bIsSecret;
	/** Whether the achievement awarded online or not */
	var const bool bWasAchievedOnline;
	/** Whether the achievement awarded offline or not */
	var const bool bWasAchievedOffline;
};

/** The state of an async enumeration (friends, content, etc) read request */
enum EOnlineNewsType
{
	ONT_Unknown,
	ONT_GameNews,
	ONT_ContentAnnouncements,
	ONT_Misc
};

/** Whether to use the override or not */
var config bool bUseBuildIdOverride;

/** Used to specify a build override value */
var config int BuildIdOverride;

/** The class to create our patcher instance from */
var config string IniLocPatcherClassName;

/** The object responsible for patching downloaded INI/loc files */
var transient IniLocPatcher Patcher;

/** Used to introduce latency when communicating with the online layer for testing purposes */
var config float AsyncMinCompletionTime;

/** Holds information about a player in a friends list */
struct native OnlinePartyMember
{
	/** Unique identifier of the friend */
	var const UniqueNetId UniqueId;
	/** The player's nickname */
    var const string Nickname;
	/** The local controller id if local */
	var const byte LocalUserNum;
	/** The NAT type for this player */
    var const ENATType NatType;
	/** The title id of the title the player is in */
	var const int TitleId;
	/** Whether the player is local to this machine or remote */
	var const bool bIsLocal;
	/** Whether the player is in party voice or game voice */
	var const bool bIsInPartyVoice;
	/** Whether the player is talking or not */
	var const bool bIsTalking;
	/** Whether the player is in a game session */
	var const bool bIsInGameSession;
	/** Whether the party member is playing the same game or not */
	var const bool bIsPlayingThisGame;
	/** The session id that the player is in */
	var const qword SessionId;
	/** The first 4 bytes of custom application data */
	var const int Data1;
	/** The second 4 bytes of custom application data */
	var const int Data2;
	/** The third 4 bytes of custom application data */
	var const int Data3;
	/** The fourth 4 bytes of custom application data */
	var const int Data4;
};

/** Flags indicating the privileges allowed for posting to social networking sites */
struct native SocialPostPrivileges
{
	/** The title can post images on social networks */
	var const bool bCanPostImage;
	/** The title can post links on social networks */
	var const bool bCanPostLink;
};

/** Flags describing the posted content */
struct native SocialPostImageFlags
{
	/** Image is user generated content */
	var bool bIsUserGeneratedImage;
	/** Post is the result of miscellaneous game activity */
	var bool bIsGameGeneratedImage;
	/** Post is the result of an earned achievement */
	var bool bIsAchievementImage;
	/** Post is for media */
	var bool bIsMediaImage;
};

/** Info needed to post an image */
struct native SocialPostImageInfo
{
	var SocialPostImageFlags Flags;
	/** On Facebook, this is the text that the user provides to describe the image, which appears directly below the user's name */
	var string MessageText;
	/** On Facebook, clicking this text displays the image at full size */
    var string TitleText;
	/** The caption of the preview image that accompanies this post */
    var string PictureCaption;
	/** On Facebook, this text is displayed below the caption */
    var string PictureDescription;
};

/** Info needed to post an image link */
struct native SocialPostLinkInfo extends SocialPostImageInfo
{
	/** Clicking on the message title text on the social networking site navigates to this URL */
	var string TitleURL;
	/** Clicking on the preview image on the social networking site navigates to this URL */
    var string PictureURL;
};

cpptext
{
// FTickableObject interface

	/**
	 * Returns whether it is okay to tick this object. E.g. objects being loaded in the background shouldn't be ticked
	 * till they are finalized and unreachable objects cannot be ticked either.
	 *
	 * @return	TRUE if tickable, FALSE otherwise
	 */
	virtual UBOOL IsTickable() const
	{
		// We cannot tick objects that are unreachable or are in the process of being loaded in the background.
		return !HasAnyFlags( RF_Unreachable | RF_AsyncLoading );
	}

	/**
	 * Used to determine if an object should be ticked when the game is paused.
	 *
	 * @return always TRUE as networking needs to be ticked even when paused
	 */
	virtual UBOOL IsTickableWhenPaused() const
	{
		return TRUE;
	}

	/**
	 * Here to complete the interface but needs to be overriden
	 *
	 * @param ignored
	 */
	virtual void Tick(FLOAT)
	{
		check(0 && "Must be overridden");
	}

	/**
	 * Generates a string representation of a UniqueNetId struct.
	 *
	 * @param	IdToConvert		the unique net id that should be converted to a string.
	 *
	 * @return	the specified UniqueNetId represented as a string.
	 */
	static FString UniqueNetIdToString( const FUniqueNetId& IdToConvert );

	/**
	 * Converts a string representing a UniqueNetId into a UniqueNetId struct.
	 *
	 * @param	UniqueNetIdString	the string containing the text representation of the unique id.
	 * @param	out_UniqueId		will receive the UniqueNetId generated from the string.
	 *
	 * @return	TRUE if the string was successfully converted into a UniqueNetId; FALSE if the string was not a valid UniqueNetId.
	 */
	static UBOOL StringToUniqueNetId( const FString& UniqueNetIdString, FUniqueNetId& out_UniqueId );

	/**
	 * Searches the named session array for the specified session
	 *
	 * @param SessionName the name to search for
	 *
	 * @return pointer to the struct if found, NULL otherwise
	 */
	inline FNamedSession* GetNamedSession(FName SessionName)
	{
		for (INT SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
		{
			if (Sessions(SearchIndex).SessionName == SessionName)
			{
				return &Sessions(SearchIndex);
			}
		}
		return NULL;
	}

	/**
	 * Searches the named session array for the specified session
	 *
	 * @param SessionName the name to search for
	 *
	 * @return TRUE if found, FALSE otherwise
	 */
	inline UBOOL HasNamedSession(FName SessionName)
	{
		return GetNamedSession(SessionName) != NULL;
	}

	/**
	 * Adds a new named session to the list
	 *
	 * @param SessionName the name to search for
	 * @param GameSettings the game settings to add
	 *
	 * @return a pointer to the struct that was added
	 */
	inline FNamedSession* AddNamedSession(FName SessionName,UOnlineGameSettings* GameSettings)
	{
		INT AddIndex = Sessions.AddZeroed();
		Sessions(AddIndex).SessionName = SessionName;
		Sessions(AddIndex).GameSettings = GameSettings;
		return &Sessions(AddIndex);
	}

	/**
	 * Searches the named session array for the specified session and removes it
	 *
	 * @param SessionName the name to search for
	 */
	virtual void RemoveNamedSession(FName SessionName)
	{
		for (INT SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
		{
			if (Sessions(SearchIndex).SessionName == SessionName)
			{
				Sessions.Remove(SearchIndex);
				return;
			}
		}
	}

	/**
	 * @return platform specific id for a session
	 */
	virtual QWORD GetSessionId(FName SessionName)
	{
		return 0;
	}

	/**
	 * @return platform specific id for a session from a search result
	 */
	virtual QWORD GetSearchResultSessionId(const struct FOnlineGameSearchResult* SearchResult)
	{
		return 0;
	}

	/**
	 * Notification sent to OnlineSubsystem that pre-travel cleanup is occuring
	 *
	 * @param bSessionEnded		whether or not the game session has ended
	 */
	virtual void NotifyCleanupWorld(UBOOL bSessionEnded)
	{
	}
}

/**
 * Called from engine start up code to allow the subsystem to initialize
 *
 * @return TRUE if the initialization was successful, FALSE otherwise
 */
native event bool Init();

/**
 * Used to trigger any automated tasks that need to happen just after startup
 */
event bool PostInit()
{
	local class<IniLocPatcher> IniLocPatcherClass;

	// Now that the subsystem is setup, try to download any config/loc updates
	if (IniLocPatcherClassName != "")
	{
		`Log("Loading "$IniLocPatcherClassName$" for INI/Loc patching",,'DevMCP');
		IniLocPatcherClass = class<IniLocPatcher>(DynamicLoadObject(IniLocPatcherClassName,class'Class'));
		Patcher = new IniLocPatcherClass;
		if (Patcher != None)
		{
			Patcher.Init();
		}
		else
		{
			return false;
		}
	}
	return true;
}

/**
 * Called from the engine shutdown code to allow the subsystem to release any
 * resources that may have been allocated
 */
event Exit();

/**
 * Called from native code to assign the account interface
 *
 * @param NewInterface the object to assign as providing the account interface
 *
 * @return TRUE if the interface is valid, FALSE otherwise
 */
event bool SetAccountInterface(Object NewInterface)
{
	AccountInterface = OnlineAccountInterface(NewInterface);
	// This will return false, if the interface wasn't supported
	return AccountInterface != None;
}

/**
 * Called from native code to assign the player interface
 *
 * @param NewInterface the object to assign as providing the player interface
 *
 * @return TRUE if the interface is valid, FALSE otherwise
 */
event bool SetPlayerInterface(Object NewInterface)
{
	PlayerInterface = OnlinePlayerInterface(NewInterface);
	// This will return false, if the interface wasn't supported
	return PlayerInterface != None;
}

/**
 * Called from native code to assign the extended player interface
 *
 * @param NewInterface the object to assign as providing the player interface
 *
 * @return TRUE if the interface is valid, FALSE otherwise
 */
event bool SetPlayerInterfaceEx(Object NewInterface)
{
	PlayerInterfaceEx = OnlinePlayerInterfaceEx(NewInterface);
	// This will return false, if the interface wasn't supported
	return PlayerInterfaceEx != None;
}

/**
 * Called from native code to assign the system interface
 *
 * @param NewInterface the object to assign as providing the system interface
 *
 * @return TRUE if the interface is valid, FALSE otherwise
 */
event bool SetSystemInterface(Object NewInterface)
{
	SystemInterface = OnlineSystemInterface(NewInterface);
	// This will return false, if the interface wasn't supported
	return SystemInterface != None;
}

/**
 * Called from native code to assign the game interface
 *
 * @param NewInterface the object to assign as providing the game interface
 *
 * @return TRUE if the interface is valid, FALSE otherwise
 */
event bool SetGameInterface(Object NewInterface)
{
	GameInterface = OnlineGameInterface(NewInterface);
	// This will return false, if the interface wasn't supported
	return GameInterface != None;
}

/**
 * Called from native code to assign the content interface
 *
 * @param NewInterface the object to assign as providing the content interface
 *
 * @return TRUE if the interface is valid, FALSE otherwise
 */
event bool SetContentInterface(Object NewInterface)
{
	ContentInterface = OnlineContentInterface(NewInterface);
	// This will return false, if the interface wasn't supported
	return ContentInterface != None;
}

/**
 * Called from native code to assign the voice interface
 *
 * @param NewInterface the object to assign as providing the voice interface
 *
 * @return TRUE if the interface is valid, FALSE otherwise
 */
event bool SetVoiceInterface(Object NewInterface)
{
	VoiceInterface = OnlineVoiceInterface(NewInterface);
	// This will return false, if the interface wasn't supported
	return VoiceInterface != None;
}

/**
 * Called from native code to assign the stats interface
 *
 * @param NewInterface the object to assign as providing the stats interface
 *
 * @return TRUE if the interface is valid, FALSE otherwise
 */
event bool SetStatsInterface(Object NewInterface)
{
	StatsInterface = OnlineStatsInterface(NewInterface);
	// This will return false, if the interface wasn't supported
	return StatsInterface != None;
}

/**
 * Called from native code to assign the news interface
 *
 * @param NewInterface the object to assign as providing the news interface
 *
 * @return TRUE if the interface is valid, FALSE otherwise
 */
event bool SetNewsInterface(Object NewInterface)
{
	NewsInterface = OnlineNewsInterface(NewInterface);
	// This will return false, if the interface wasn't supported
	return NewsInterface != None;
}

/**
 * Called from native code to assign the party chat interface
 *
 * @param NewInterface the object to assign as providing the party chat interface
 *
 * @return TRUE if the interface is valid, FALSE otherwise
 */
event bool SetPartyChatInterface(Object NewInterface)
{
	PartyChatInterface = OnlinePartyChatInterface(NewInterface);
	// This will return false, if the interface wasn't supported
	return PartyChatInterface != None;
}

/**
 * Called from native code to assign the title file interface
 *
 * @param NewInterface the object to assign as providing the file downloading interface
 *
 * @return TRUE if the interface is valid, FALSE otherwise
 */
event bool SetTitleFileInterface(Object NewInterface)
{
	TitleFileInterface = OnlineTitleFileInterface(NewInterface);
	// This will return false, if the interface wasn't supported
	return TitleFileInterface != None;
}

/**
 * Called from native code to assign the title file caching interface
 *
 * @param NewInterface the object to assign as providing the file caching interface
 *
 * @return TRUE if the interface is valid, FALSE otherwise
 */
event bool SetTitleFileCacheInterface(Object NewInterface)
{
	TitleFileCacheInterface = OnlineTitleFileCacheInterface(NewInterface);
	// This will return false, if the interface wasn't supported
	return TitleFileCacheInterface != None;
}

/**
 * Called from native code to assign the social interface
 *
 * @param SocialInterface the object to assign as providing the interface for posting to social networks
 *
 * @return TRUE if the interface is valid, FALSE otherwise
 */
event bool SetSocialInterface(Object InSocialInterface)
{
	SocialInterface = OnlineSocialInterface(InSocialInterface);
	// This will return false, if the interface wasn't supported
	return SocialInterface != None;
}

/**
 * Called from native code to assign the auth interface
 *
 * @param AuthInterface the object to assign as providing the interface for multiplayer authentication
 *
 * @return TRUE if the interface is valid, FALSE otherwise
 */
event bool SetAuthInterface(Object InAuthInterface)
{
	AuthInterface = OnlineAuthInterface(InAuthInterface);
	// This will return false, if the interface wasn't supported
	return AuthInterface != None;
}

/**
 * Called from native code to assign the user cloud interface
 *
 * @param InCloudInterface the object to assign as providing the interface for user cloud access
 *
 * @return TRUE if the interface is valid, FALSE otherwise
 */
event bool SetUserCloudInterface(Object InCloudInterface)
{
	UserCloudInterface = UserCloudFileInterface(InCloudInterface);
	// This will return false, if the interface wasn't supported
	return UserCloudInterface != None;
}

/**
 * Called from native code to assign the shared cloud interface
 *
 * @param InCloudInterface the object to assign as providing the interface for shared cloud access
 *
 * @return TRUE if the interface is valid, FALSE otherwise
 */
event bool SetSharedCloudInterface(Object InCloudInterface)
{
	SharedCloudInterface = SharedCloudFileInterface(InCloudInterface);
	// This will return false, if the interface wasn't supported
	return SharedCloudInterface != None;
}

/**
 * Adds the interface object to the named interface list with the specified name
 *
 * @param InterfaceName the name of the interface to add it as
 * @param NewInterface the object/interface that is being cached
 */
event SetNamedInterface(name InterfaceName,Object NewInterface)
{
	local int InterfaceIndex;

	// Determine whether to add or replace
	InterfaceIndex = NamedInterfaces.Find('InterfaceName',InterfaceName);
	if (InterfaceIndex == INDEX_NONE)
	{
		// We are adding to the list
		InterfaceIndex = NamedInterfaces.Length;
		NamedInterfaces.Length = NamedInterfaces.Length + 1;
		NamedInterfaces[InterfaceIndex].InterfaceName = InterfaceName;
	}
	// Replace whatever pointer was previously there
	NamedInterfaces[InterfaceIndex].InterfaceObject = NewInterface;
}

/**
 * Returns the interface object associated with the specified name
 *
 * @param InterfaceName the name of the interface to retrieve
 *
 * @return The object that matches the interface name if found
 */
event Object GetNamedInterface(name InterfaceName)
{
	local int InterfaceIndex;

	// Find the index in the array for this interface
	InterfaceIndex = NamedInterfaces.Find('InterfaceName',InterfaceName);
	if (InterfaceIndex != INDEX_NONE)
	{
		return NamedInterfaces[InterfaceIndex].InterfaceObject;
	}
	return None;
}

/**
 * Generates a string representation of a UniqueNetId struct.
 *
 * @param	IdToConvert		the unique net id that should be converted to a string.
 *
 * @return	the specified UniqueNetId represented as a string.
 */
static final native noexportheader function string UniqueNetIdToString(const out UniqueNetId IdToConvert);

/**
 * Converts a string representing a UniqueNetId into a UniqueNetId struct.
 *
 * @param	UniqueNetIdString	the string containing the text representation of the unique id.
 * @param	out_UniqueId		will receive the UniqueNetId generated from the string.
 *
 * @return	TRUE if the string was successfully converted into a UniqueNetId; FALSE if the string was not a valid UniqueNetId.
 */
static final native noexportheader function bool StringToUniqueNetId(string UniqueNetIdString,out UniqueNetId out_UniqueId);

/**
 * Returns the unique id of the player for the specified index
 *
 * @param UserIndex the user to return the id of
 *
 * @return the unique id of the player at the specified index
 */
event UniqueNetId GetPlayerUniqueNetIdFromIndex(int UserIndex)
{
	local UniqueNetId ReturnVal, NullId;

	if (PlayerInterface == none || !PlayerInterface.GetUniquePlayerId(UserIndex, ReturnVal))
	{
		ReturnVal = NullId;
	}

	return ReturnVal;
}

/**
 * Generates a unique number based off of the current script compilation
 *
 * @return the unique number from the current script compilation
 */
native function int GetBuildUniqueId();

/**
 * Returns the number of players that can be signed in on this platform
 */
static final native function int GetNumSupportedLogins();

/**
 * Logs the game settings object
 *
 * @param GameSettings settings to dump
 */
static function DumpGameSettings(const OnlineGameSettings GameSettings)
{
	`Log("    OnlineGameSettings: "$GameSettings);
	`Log("      OwningPlayerName: "$GameSettings.OwningPlayerName);
	`Log("      OwningPlayerId: "$UniqueNetIdToString(GameSettings.OwningPlayerId));
	`Log("      PingInMs: "$GameSettings.PingInMs);
	`Log("      NumPublicConnections: "$GameSettings.NumPublicConnections);
	`Log("      NumOpenPublicConnections: "$GameSettings.NumOpenPublicConnections);
	`Log("      NumPrivateConnections: "$GameSettings.NumPrivateConnections);
	`Log("      NumOpenPrivateConnections: "$GameSettings.NumOpenPrivateConnections);
	`Log("      bIsLanMatch: "$GameSettings.bIsLanMatch);
	`Log("      bIsDedicated: "$GameSettings.bIsDedicated);
	`Log("      bUsesStats: "$GameSettings.bUsesStats);
	`Log("      bUsesArbitration: "$GameSettings.bUsesArbitration);
	`Log("      bAntiCheatProtected: "$GameSettings.bAntiCheatProtected);
	`Log("      bShouldAdvertise: "$GameSettings.bShouldAdvertise);
	`Log("      bAllowJoinInProgress: "$GameSettings.bAllowJoinInProgress);
	`Log("      bAllowInvites: "$GameSettings.bAllowInvites);
	`Log("      bUsesPresence: "$GameSettings.bUsesPresence);
	`Log("      bWasFromInvite: "$GameSettings.bWasFromInvite);
	`Log("      bAllowJoinViaPresence: "$GameSettings.bAllowJoinViaPresence);
	`Log("      bAllowJoinViaPresenceFriendsOnly: "$GameSettings.bAllowJoinViaPresenceFriendsOnly);
	`Log("      GameState: "$GameSettings.GameState);
}

/**
 * Logs the session state for all sessions present
 */
function DumpSessionState()
{
	local int Index;
	local int PlayerIndex;
	local UniqueNetId NetId;
	local UniqueNetId ZeroId;

	// To make the dumb warning go away even though it is used
	NetId = ZeroId;
	ZeroId = NetId;

	`Log("Unreal online session state");
	`Log("-------------------------------------------------------------");
	`Log("");
	`Log("Number of sessions: "$Sessions.Length);

	// Iterate through the sessions listing the session plus the registrants
	for (Index = 0; Index < Sessions.Length; Index++)
	{
		`Log("  Session: "$Sessions[Index].SessionName);
		DumpGameSettings(Sessions[Index].GameSettings);
		`Log("");

		`Log("    Number of players: "$Sessions[Index].Registrants.Length);
		// List each player in the session
		for (PlayerIndex = 0; PlayerIndex < Sessions[Index].Registrants.Length; PlayerIndex++)
		{
			NetId = Sessions[Index].Registrants[PlayerIndex].PlayerNetId;
			`Log("      Player: "$UniqueNetIdToString(NetId));
		}
		`Log("    Number of arbitrated players: "$Sessions[Index].ArbitrationRegistrants.Length);
		// List each arbitrated player in the session
		for (PlayerIndex = 0; PlayerIndex < Sessions[Index].ArbitrationRegistrants.Length; PlayerIndex++)
		{
			NetId = Sessions[Index].ArbitrationRegistrants[PlayerIndex].PlayerNetId;
			`Log("      Player: "$UniqueNetIdToString(NetId));
		}
	}
}

`if(`notdefined(FINAL_RELEASE))
	/**
	 * Log list of players' netids for debugging
	 */
	static function DumpNetIds(const out array<UniqueNetId> Players, string DebugLabel)
	{
		local int PlayerIdx;
		local UniqueNetId NetId;
		for (PlayerIdx=0; PlayerIdx<Players.Length; PlayerIdx++)
		{
			NetId = Players[PlayerIdx];
			`Log(DebugLabel$": "
				$" PlayerIdx="$PlayerIdx
				$" UniqueId="$UniqueNetIdToString(NetId));
		}
	}
`endif

/**
 * Logs the list of players that are registered for voice
 */
function DumpVoiceRegistration();

/**
 * Sets the debug output level for the platform specific API (if applicable)
 *
 * @param DebugSpewLevel the level to set
 */
function SetDebugSpewLevel(int DebugSpewLevel);
