//=============================================================================
// GameInfo.
//
// The GameInfo defines the game being played: the game rules, scoring, what actors
// are allowed to exist in this game type, and who may enter the game.  While the
// GameInfo class is the public interface, much of this functionality is delegated
// to several classes to allow easy modification of specific game components.  These
// classes include GameInfo, AccessControl, Mutator, and BroadcastHandler.
// A GameInfo actor is instantiated when the level is initialized for gameplay (in
// C++ UGameEngine::LoadMap() ).  The class of this GameInfo actor is determined by
// (in order) either the URL ?game=xxx, or the
// DefaultGame entry in the game's .ini file (in the Engine.Engine section), unless
// its a network game in which case the DefaultServerGame entry is used.
// The GameType used can be overridden in the GameInfo script event SetGameType(), called
// on the game class picked by the above process.
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class GameInfo extends Info
	config(Game)
	native
	dependson(OnlineSubsystem);

//-----------------------------------------------------------------------------
// Variables.

var bool				      bRestartLevel;			// Level should be restarted when player dies
var bool				      bPauseable;				// Whether the game is pauseable.
var bool				      bTeamGame;				// This is a team game.
var	bool					  bGameEnded;				// set when game ends
var	bool					  bOverTime;
var bool					  bDelayedStart;
var bool					  bWaitingToStartMatch;
var globalconfig bool		  bChangeLevels;
var		bool				  bAlreadyChanged;
var globalconfig bool			bAdminCanPause;
var bool						bGameRestarted;
var bool						bLevelChange;			// level transition in progress
var globalconfig	bool		bKickLiveIdlers;		// if true, even playercontrollers with pawns can be kicked for idling

/** Whether this match is going to use arbitration or not */
var bool bUsingArbitration;

/**
 * Whether the arbitrated handshaking has occurred or not.
 * NOTE: The code will reject new connections once handshaking has started
 */
var bool bHasArbitratedHandshakeBegun;

/** Whether the arbitrated handshaking has occurred or not. */
var bool bNeedsEndGameHandshake;

/** Whether the arbitrated handshaking has completed or not. */
var bool bIsEndGameHandshakeComplete;

/** Used to indicate when an arbitrated match has started its end sequence */
var bool bHasEndGameHandshakeBegun;

/** Whether the game expects a fixed player start for profiling. */
var bool bFixedPlayerStart;

/** The causeevent= string that the game passed in This is separate from automatedPerfTesting which is going to probably spawn bots / effects **/
var string CauseEventCommand;

/** This is the BugIt String Data. Other info should be stored here  **/
/** Currently stores the location string form **/
var string BugLocString;

/** Currently stores the rotation in string form **/
var string BugRotString;

/**
 * List of player controllers we're awaiting handshakes with
 *
 * NOTE: Any PC in this list that does not complete the handshake within
 * ArbitrationHandshakeTimeout will be kicked from the match
 */
var array<PlayerController> PendingArbitrationPCs;

/**
 * Holds the list of players that passed handshaking and require finalization
 * of arbitration data written to the online subsystem
 */
var array<PlayerController> ArbitrationPCs;

/** Amount of time a client can take for arbitration handshaking before being kicked */
var globalconfig float ArbitrationHandshakeTimeout;

var globalconfig float        GameDifficulty;
var	  globalconfig int		  GoreLevel;				// 0=Normal, increasing values=less gore
var   float					  GameSpeed;				// Scale applied to game rate.

var   class<Pawn>			  DefaultPawnClass;

// user interface
var	  class<HUD>			  HUDType;					// HUD class this game uses.
var	  class<HUD>			  SecondaryHUDType;			// Secondary HUD class this game uses.

var   globalconfig int	      MaxSpectators;			// Maximum number of spectators allowed by this server.
var	  int					  MaxSpectatorsAllowed;		// Maximum number of spectators ever allowed (MaxSpectators is clamped to this in initgame()
var	  int					  NumSpectators;			// Current number of spectators.
var   globalconfig int		  MaxPlayers;				// Maximum number of players allowed by this server.
var	  int					  MaxPlayersAllowed;		// Maximum number of players ever allowed (MaxPlayers is clamped to this in initgame()
var   int					  NumPlayers;				// number of human players
var	  int					  NumBots;					// number of non-human players (AI controlled but participating as a player)

/** number of players that are still travelling from a previous map */
var int NumTravellingPlayers;
var   int					  CurrentID;				// used to assign unique PlayerIDs to each PlayerReplicationInfo
var localized string	      DefaultPlayerName;
var localized string	      GameName;
var float					  FearCostFallOff;			// how fast the FearCost in NavigationPoints falls off
var	bool					  bDoFearCostFallOff;		// If true, FearCost will fall off over time in NavigationPoints.  Reset to false once all reach FearCosts reach 0.

var config int                GoalScore;                // what score is needed to end the match
var config int                MaxLives;	                // max number of lives for match, unless overruled by level's GameDetails
var config int                TimeLimit;                // time limit in minutes

// Message classes.
var class<LocalMessage>		  DeathMessageClass;
var class<GameMessage>		  GameMessageClass;

//-------------------------------------
// GameInfo components
var Mutator BaseMutator;				// linked list of Mutators (for modifying actors as they enter the game)
var class<AccessControl> AccessControlClass;
var AccessControl AccessControl;		// AccessControl controls whether players can enter and/or become admins
var class<BroadcastHandler> BroadcastHandlerClass;
var BroadcastHandler BroadcastHandler;	// handles message (text and localized) broadcasts

/** Class of automated test manager used by this game class */
var class<AutoTestManager> AutoTestManagerClass;

/** Instantiated AutoTestManager - only exists if requested by command-line */
var AutoTestManager	MyAutoTestManager;

var class<PlayerController> PlayerControllerClass;	// type of player controller to spawn for players logging in
var class<PlayerReplicationInfo> 		PlayerReplicationInfoClass;

// ReplicationInfo
var() class<GameReplicationInfo> GameReplicationInfoClass;
var GameReplicationInfo GameReplicationInfo;

var CrowdPopulationManagerBase PopulationManager;
var class<CrowdPopulationManagerBase> PopulationManagerClass;

var globalconfig float MaxIdleTime;		// maximum time players are allowed to idle before being kicked

/** Max interval that client clock is allowed to get ahead of server clock before triggering speed hack detection */
var globalconfig	float					MaxTimeMargin;

/** How fast we allow client clock to drift from server clock over time without ever triggering speed hack detection */
var globalconfig	float					TimeMarginSlack;

/** Clamps how far behind server clock we let time margin get.  Used to prevent speedhacks where client slows their clock down for a while then speeds it up. */
var globalconfig	float					MinTimeMargin;

var		array<PlayerReplicationInfo> InactivePRIArray;	/** PRIs of players who have left game (saved in case they reconnect) */

/** The list of delegates to check before unpausing a game */
var array<delegate<CanUnpause> > Pausers;

/** Cached online subsystem variable */
var OnlineSubsystem OnlineSub;

/** Cached online game interface variable */
var OnlineGameInterface GameInterface;

/** Class sent to clients to use to create and hold their stats */
var class<OnlineStatsWrite> OnlineStatsWriteClass;

/** The leaderboard to write the stats to for skill/scoring */
var int LeaderboardId;

/** The arbitrated leaderboard to write the stats to for skill/scoring */
var int ArbitratedLeaderboardId;

/** perform map travels using SeamlessTravel() which loads in the background and doesn't disconnect clients
 * @see WorldInfo::SeamlessTravel()
 */
var bool bUseSeamlessTravel;

/** Base copy of cover changes that need to be replicated to clients on join */
var protected CoverReplicator CoverReplicatorBase;

/** Tracks whether the server can travel due to a critical network error or not */
var bool bHasNetworkError;

/** Whether this game type requires voice to be push to talk or not */
var const bool bRequiresPushToTalk;

/** The class to use when registering dedicated servers with the online service */
var const class<OnlineGameSettings> OnlineGameSettingsClass;

/** The options to apply for dedicated server when it starts to register */
var string ServerOptions;

/** Current adjusted net speed - Used for dynamically managing netspeed for listen servers*/
var int AdjustedNetSpeed;

/**  Last time netspeed was updated for server (by client entering or leaving) */
var float LastNetSpeedUpdateTime;

/** Total available bandwidth for listen server, split dynamically across net connections */
var globalconfig int TotalNetBandwidth;

/** Minimum bandwidth dynamically set per connection */
var globalconfig int MinDynamicBandwidth;

/** Maximum bandwidth dynamically set per connection */
var globalconfig int MaxDynamicBandwidth;

/** Standby cheat detection vars */
/** Used to determine if checking for standby cheats should occur */
var config bool bIsStandbyCheckingEnabled;
/** Tracks standby checking status */
var bool bIsStandbyCheckingOn;
/** Used to determine whether we've already caught a cheat or not */
var bool bHasStandbyCheatTriggered;
/** The amount of time without packets before triggering the cheat code */
var config float StandbyRxCheatTime;
/** The amount of time without packets before triggering the cheat code */
var config float StandbyTxCheatTime;
/** The point we determine the server is either delaying packets or has bad upstream */
var config int BadPingThreshold;
/** The percentage of clients missing RX data before triggering the standby code */
var config float PercentMissingForRxStandby;
/** The percentage of clients missing TX data before triggering the standby code */
var config float PercentMissingForTxStandby;
/** The percentage of clients with bad ping before triggering the standby code */
var config float PercentForBadPing;
/** The amount of time to wait before checking a connection for standby issues */
var config float JoinInProgressStandbyWaitTime;

/** Material used for drawing a streaming pause icon. */
var Material StreamingPauseIcon;

/** Describes which standby detection event occured so the game can take appropriate action */
enum EStandbyType
{
	STDBY_Rx,
	STDBY_Tx,
	STDBY_BadPing
};
/** End standby cheat vars */

struct native GameClassShortName
{
	var string ShortName;
	var string GameClassName;
};
var() protected config const array<GameClassShortName> GameInfoClassAliases;

/**
 *	GameTypePrefix helper structure.
 *	Used to find valid gametypes for a map via its prefix.
 */
struct native GameTypePrefix
{
	/** map prefix, e.g. "DM" */
	var string Prefix;
	/** if TRUE, generate a common package for the gametype */
	var bool bUsesCommonPackage;
	/** gametype used if none specified on the URL */
	var string GameType;
	/** additional gametypes supported by this map prefix via the URL (used for cooking) */
	var array<string> AdditionalGameTypes;
	/** forced objects (and classes) that should go into the common package to avoid cooking into every map */
	var array<string> ForcedObjects;
};

/** The default game type to use on a map */
var config string					DefaultGameType;
/** Used for loading appropriate game type if non-specified in URL */
var config array<GameTypePrefix>	DefaultMapPrefixes;
/** Used for loading appropriate game type if non-specified in URL */
var config array<GameTypePrefix>	CustomMapPrefixes;


/** Size of the AnimTree pool. System will keep this number of extra AnimTrees around per Template */
var config int AnimTreePoolSize;

/**
 *	Retrieve the FGameTypePrefix struct for the given map filename.
 *
 *	@param	InFilename		The map file name
 *	@param	OutGameType		The gametype prefix struct to fill in
 *	@param	bCheckExt		Optional parameter to check the extension of the InFilename to ensure it is a map
 *
 *	@return	UBOOL			TRUE if successful, FALSE if map prefix not found.
 *							NOTE: FALSE will fill in with the default gametype.
 */
function native bool GetSupportedGameTypes(const out string InFilename, out GameTypePrefix OutGameType, optional bool bCheckExt = false) const;

/**
 *	Retrieve the name of the common package (if any) for the given map filename.
 *
 *	@param	InFilename		The map file name
 *	@param	OutCommonPackageName	The nane of the common package for the given map
 *
 *	@return	UBOOL			TRUE if successful, FALSE if map prefix not found.
 */
function native bool GetMapCommonPackageName(const out string InFilename, out string OutCommonPackageName) const;

cpptext
{
	/** called on the default object of the class specified by DefaultGame in the [Engine.GameInfo] section of Game.ini
	 * whenever worlds are saved.
	 * Gives the game a chance to add supported gametypes to the WorldInfo's GameTypesSupportedOnThisMap array
	 * (used for console cooking)
	 * @param Info: the WorldInfo of the world being saved
	 */
	virtual void AddSupportedGameTypes(AWorldInfo* Info, const TCHAR* WorldFilename, TArray<FString>& AdditionalPackagesToCook) const
	{
	}

	/** Allows for game classname remapping and/or aliasing (e.g. for shorthand names) */
	static FString StaticGetRemappedGameClassName(FString const& GameClassName);
}

//------------------------------------------------------------------------------
// Engine notifications.

event PreBeginPlay()
{
	AdjustedNetSpeed = MaxDynamicBandwidth;
	SetGameSpeed(GameSpeed);
	GameReplicationInfo = Spawn(GameReplicationInfoClass);
	WorldInfo.GRI = GameReplicationInfo;

	InitGameReplicationInfo();
	InitCrowdPopulationManager();
}

function CoverReplicator GetCoverReplicator()
{
	if (CoverReplicatorBase == None && WorldInfo.NetMode != NM_Standalone)
	{
		CoverReplicatorBase = Spawn(class'CoverReplicator');
	}
	return CoverReplicatorBase;
}

event PostBeginPlay()
{
	if ( MaxIdleTime > 0 )
	{
		MaxIdleTime = FMax(MaxIdleTime, 20);
	}

	if (WorldInfo.NetMode == NM_DedicatedServer)
	{
		// Update any online advertised settings
		UpdateGameSettings();
	}
}

/**
  *  Use 'ShowGameDebug' console command to show this debug info
  *  Useful to show general debug info not tied to a particular actor physically in the level.
  */
simulated function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local Canvas	Canvas;

	Canvas = HUD.Canvas;
	Canvas.SetDrawColor(255,255,255);

	Canvas.DrawText("Game:" $GameName );
	out_YPos += out_YL;
	Canvas.SetPos(4,out_YPos);

	if ( WorldInfo.PopulationManager != None )
	{
		WorldInfo.PopulationManager.DisplayDebug(HUD, out_YL, out_YPos);
	}
}


/* Reset() - reset actor to initial state - used when restarting level without reloading.
	@note: GameInfo::Reset() called after all other actors have been reset */
function Reset()
{
	super.Reset();

	bGameEnded = false;
	bOverTime = false;
	InitGameReplicationInfo();
}

/** @return true if ActorToReset should have Reset() called on it while restarting the game, false if the GameInfo will manually reset it
	or if the actor does not need to be reset
*/
function bool ShouldReset(Actor ActorToReset)
{
	return true;
}

/** Resets level by calling Reset() on all actors */
function ResetLevel()
{
	local Controller C;
	local Actor A;
	local Sequence GameSeq;
	local array<SequenceObject> AllSeqEvents;
	local array<int> ActivateIndices;
	local int i;

	`Log("Reset" @ self);
	// Reset ALL controllers first
	foreach WorldInfo.AllControllers(class'Controller', C)
	{
		if ( PlayerController(C) != None )
		{
			PlayerController(C).ClientReset();
		}
		C.Reset();
	}

	// Reset all actors (except controllers, the GameInfo, and any other actors specified by ShouldReset())
	foreach AllActors(class'Actor', A)
	{
		if (A != self && !A.IsA('Controller') && ShouldReset(A))
		{
			A.Reset();
		}
	}

	// reset the GameInfo
	Reset();

	// reset Kismet and activate any Level Reset events
	GameSeq = WorldInfo.GetGameSequence();
	if (GameSeq != None)
	{
		// reset the game sequence
		GameSeq.Reset();

		// find any Level Loaded events that exist
		GameSeq.FindSeqObjectsByClass(class'SeqEvent_LevelLoaded', true, AllSeqEvents);

		// activate them
		ActivateIndices[0] = 2;
		for (i = 0; i < AllSeqEvents.Length; i++)
		{
			SeqEvent_LevelLoaded(AllSeqEvents[i]).CheckActivate(WorldInfo, None, false, ActivateIndices);
		}
	}
}


event Timer()
{
	BroadcastHandler.UpdateSentText();

	// Update navigation point fear cost fall off.
	if ( bDoFearCostFallOff )
	{
		DoNavFearCostFallOff();
	}
}

/**
 *	Check to see if we should start in cinematic mode (e.g. matinee movie capture).
 *
 * 	@param	OutHidePlayer		Whether to hide the player
 *	@param	OutHideHud		Whether to hide the HUD		
 *	@param	OutDisableMovement	Whether to disable movement
 * 	@param	OutDisableTurning	Whether to disable turning
 *	@param	OutDisableInput		Whether to disable input
 *
 *	@return	UBOOL			TRUE if we should turn on cinematic mode, 
 *					FALSE if if we should not.
 */
final native function bool ShouldStartInCinematicMode(out int OutHidePlayer, out int OutHideHud, out int OutDisableMovement, out int OutDisableTurning, out int OutDisableInput);

/** Update navigation point fear cost fall off. */
final native function DoNavFearCostFallOff();

/** notification when a NavigationPoint becomes blocked or unblocked */
function NotifyNavigationChanged(NavigationPoint N);

// Called when game shutsdown.
event GameEnding()
{
	if (AccessControl != none)
	{
		AccessControl.NotifyGameEnding();
	}

	ClearOnlineDelegates();

	EndLogging("serverquit");
}

/* KickIdler() called if
		if ( (Pawn != None) || (PlayerReplicationInfo.bOnlySpectator && (ViewTarget != self))
			|| (WorldInfo.Pauser != None) || WorldInfo.Game.bWaitingToStartMatch || WorldInfo.Game.bGameEnded )
		{
			LastActiveTime = WorldInfo.TimeSeconds;
		}
		else if ( (WorldInfo.Game.MaxIdleTime > 0) && (WorldInfo.TimeSeconds - LastActiveTime > WorldInfo.Game.MaxIdleTime) )
			KickIdler(self);
*/
event KickIdler(PlayerController PC)
{
	`log("Kicking idle player "$PC.PlayerReplicationInfo.PlayerName);
	AccessControl.KickPlayer(PC, AccessControl.IdleKickReason);
}

// This will kick any player, even if they are an admin.
event ForceKickPlayer(PlayerController PC, string KickReason)
{
	`log("Force kicking player "$PC.PlayerReplicationInfo.PlayerName);
	AccessControl.ForceKickPlayer(PC, KickReason);
}

//------------------------------------------------------------------------------
// Replication

function InitGameReplicationInfo()
{
	GameReplicationInfo.GameClass = Class;
	GameReplicationInfo.ReceivedGameClass();
}

native function string GetNetworkNumber();

function int GetNumPlayers()
{
	return NumPlayers + NumTravellingPlayers;
}

//------------------------------------------------------------------------------
// Misc.

/**
 * Default delegate that provides an implementation for those that don't have
 * special needs other than a toggle
 */
delegate bool CanUnpause()
{
	return true;
}

/**
 * Adds the delegate to the list if the player controller has the right to pause
 * the game. The delegate is called to see if it is ok to unpause the game, e.g.
 * the reason the game was paused has been cleared.
 *
 * @param PC the player controller to check for admin privs
 * @param CanUnpauseDelegate the delegate to query when checking for unpause
 */
function bool SetPause(PlayerController PC, optional delegate<CanUnpause> CanUnpauseDelegate=CanUnpause)
{
	local int FoundIndex;

	if ( AllowPausing(PC) )
	{
		// Don't add the delegate twice (no need)
		FoundIndex = Pausers.Find(CanUnpauseDelegate);
		if (FoundIndex == INDEX_NONE)
		{
			// Not in the list so add it for querying
			FoundIndex = Pausers.Length;
			Pausers.Length = FoundIndex + 1;
			Pausers[FoundIndex] = CanUnpauseDelegate;
		}
		// Let the first one in "own" the pause state
		if (WorldInfo.Pauser == None)
		{
			WorldInfo.Pauser = PC.PlayerReplicationInfo;
		}
		return true;
	}
	return false;
}

/**
 * Checks the list of delegates to determine if the pausing can be cleared. If
 * the delegate says it's ok to unpause, that delegate is removed from the list
 * and the rest are checked. The game is considered unpaused when the list is
 * empty.
 */
event ClearPause()
{
	local int Index;
	local delegate<CanUnpause> CanUnpauseCriteriaMet;

	if ( !AllowPausing() && Pausers.Length > 0 )
	{
		`log("Clearing list of UnPause delegates for" @ Name @ "because game type is not pauseable");
		Pausers.Length = 0;
	}

	for (Index = 0; Index < Pausers.Length; Index++)
	{
		CanUnpauseCriteriaMet = Pausers[Index];
		if (CanUnpauseCriteriaMet())
		{
			Pausers.Remove(Index--,1);
		}
	}

	// Clear the pause state if the list is empty
	if ( Pausers.Length == 0 )
	{
		WorldInfo.Pauser = None;
	}
}

/**
 * Forcibly removes an object's CanUnpause delegates from the list of pausers.  If any of the object's CanUnpause delegate
 * handlers were in the list, triggers a call to ClearPause().
 *
 * Called when the player controller is being destroyed to prevent the game from being stuck in a paused state when a PC that
 * paused the game is destroyed before the game is unpaused.
 */
native final function ForceClearUnpauseDelegates( Actor PauseActor );

/**
 * Dumps the pause delegate list to track down who has the game paused
 */
function DebugPause()
{
	local int Index;
	local delegate<CanUnpause> CanUnpauseCriteriaMet;

	for (Index = 0; Index < Pausers.Length; Index++)
	{
		CanUnpauseCriteriaMet = Pausers[Index];
		if (CanUnpauseCriteriaMet())
		{
			`Log("Pauser in index "$Index$" thinks it's ok to unpause:" @ CanUnpauseCriteriaMet);
		}
		else
		{
			`Log("Pauser in index "$Index$" thinks the game should remain paused:" @ CanUnpauseCriteriaMet);
		}
	}
}

//------------------------------------------------------------------------------
// Game parameters.

//
// Set gameplay speed.
//
function SetGameSpeed( Float T )
{
	GameSpeed = FMax(T, 0.00001);
	WorldInfo.TimeDilation = GameSpeed;
	SetTimer(WorldInfo.TimeDilation, true);
}

//------------------------------------------------------------------------------
// Player start functions

//
// Grab the next option from a string.
//
static function bool GrabOption( out string Options, out string Result )
{
	if( Left(Options,1)=="?" )
	{
		// Get result.
		Result = Mid(Options,1);
		if( InStr(Result,"?")>=0 )
			Result = Left( Result, InStr(Result,"?") );

		// Update options.
		Options = Mid(Options,1);
		if( InStr(Options,"?")>=0 )
			Options = Mid( Options, InStr(Options,"?") );
		else
			Options = "";

		return true;
	}
	else return false;
}

//
// Break up a key=value pair into its key and value.
//
static function GetKeyValue( string Pair, out string Key, out string Value )
{
	if( InStr(Pair,"=")>=0 )
	{
		Key   = Left(Pair,InStr(Pair,"="));
		Value = Mid(Pair,InStr(Pair,"=")+1);
	}
	else
	{
		Key   = Pair;
		Value = "";
	}
}

/* ParseOption()
 Find an option in the options string and return it.
*/
static function string ParseOption( string Options, string InKey )
{
	local string Pair, Key, Value;
	while( GrabOption( Options, Pair ) )
	{
		GetKeyValue( Pair, Key, Value );
		if( Key ~= InKey )
			return Value;
	}
	return "";
}

//
// HasOption - return true if the option is specified on the command line.
//
static function bool HasOption( string Options, string InKey )
{
    local string Pair, Key, Value;
    while( GrabOption( Options, Pair ) )
    {
	GetKeyValue( Pair, Key, Value );
	if( Key ~= InKey )
	    return true;
    }
    return false;
}

static function int GetIntOption( string Options, string ParseString, int CurrentValue)
{
	local string InOpt;

	InOpt = ParseOption( Options, ParseString );
	if ( InOpt != "" )
	{
		return int(InOpt);
	}
	return CurrentValue;
}

/** @return the full path to the optimal GameInfo class to use for the specified map and options
 * this is used for preloading cooked packages, etc. and therefore doesn't need to include any fallbacks
 * as SetGameType() will be called later to actually find/load the desired class
 */
static event string GetDefaultGameClassPath(string MapName, string Options, string Portal)
{
	return PathName(Default.Class);
}

/** @return the class of GameInfo to spawn for the game on the specified map and the specified options
 * this function should include any fallbacks in case the desired class can't be found
 */
static event class<GameInfo> SetGameType(string MapName, string Options, string Portal)
{
	return Default.Class;
}

/* Initialize the game.
 The GameInfo's InitGame() function is called before any other scripts (including
 PreBeginPlay() ), and is used by the GameInfo to initialize parameters and spawn
 its helper classes.
 Warning: this is called before actors' PreBeginPlay.
*/
event InitGame( string Options, out string ErrorMessage )
{
	local string InOpt, LeftOpt;
	local int pos;
	local class<AccessControl> ACClass;
	local AccessControl CurAC;
	local OnlineGameSettings GameSettings;

    MaxPlayers = Clamp(GetIntOption( Options, "MaxPlayers", MaxPlayers ),0,MaxPlayersAllowed);
    MaxSpectators = Clamp(GetIntOption( Options, "MaxSpectators", MaxSpectators ),0,MaxSpectatorsAllowed);
    GameDifficulty = FMax(0,GetIntOption(Options, "Difficulty", GameDifficulty));

	InOpt = ParseOption( Options, "GameSpeed");
	if( InOpt != "" )
	{
		`log("GameSpeed"@InOpt);
		SetGameSpeed(float(InOpt));
	}

	TimeLimit = Max(0,GetIntOption( Options, "TimeLimit", TimeLimit ));

	BroadcastHandler = spawn(BroadcastHandlerClass);

	InOpt = ParseOption( Options, "AccessControl");
	if( InOpt != "" )
	{
		ACClass = class<AccessControl>(DynamicLoadObject(InOpt, class'Class'));
	}
    if ( ACClass == None )
	{
		ACClass = AccessControlClass;
	}

	LeftOpt = ParseOption( Options, "AdminName" );
	InOpt = ParseOption( Options, "AdminPassword");
	/* @FIXME: this is a compiler error, "can't set defaults of non-config properties". Fix or remove at some point.
	if( LeftOpt!="" && InOpt!="" )
		ACClass.default.bDontAddDefaultAdmin = true;
	*/

	// Only spawn access control if we are a server
	if (WorldInfo.NetMode == NM_ListenServer || WorldInfo.NetMode == NM_DedicatedServer)
	{
		// If we are coming out of seamless travel, and there is an access control carried over from the previous level,
		//	use that instead of creating a new one
		if (WorldInfo.IsInSeamlessTravel())
		{
			foreach DynamicActors(Class'AccessControl', CurAC)
			{
				AccessControl = CurAC;
				break;
			}
		}

		if (AccessControl == none)
		{
			AccessControl = Spawn(ACClass);
		}

		if (AccessControl != None && InOpt != "")
		{
			AccessControl.SetAdminPassword(InOpt);
		}
	}

	InOpt = ParseOption( Options, "Mutator");
	if ( InOpt != "" )
	{
		`log("Mutators"@InOpt);
		while ( InOpt != "" )
		{
			pos = InStr(InOpt,",");
			if ( pos > 0 )
			{
				LeftOpt = Left(InOpt, pos);
				InOpt = Right(InOpt, Len(InOpt) - pos - 1);
			}
			else
			{
				LeftOpt = InOpt;
				InOpt = "";
			}
	    	AddMutator(LeftOpt, true);
		}
	}

	InOpt = ParseOption( Options, "GamePassword");
    if( InOpt != "" && AccessControl != None)
	{
		AccessControl.SetGamePassWord(InOpt);
		`log( "GamePassword" @ InOpt );
	}

	bFixedPlayerStart = ( ParseOption( Options, "FixedPlayerStart" ) ~= "1" );
	CauseEventCommand = ( ParseOption( Options, "causeevent" ) );

	if ( ParseOption( Options, "AutoTests" ) ~= "1" )
	{
		if ( MyAutoTestManager == None )
		{
			MyAutoTestManager = spawn(AutoTestManagerClass);
		}
		MyAutoTestManager.InitializeOptions(Options);
	}

	BugLocString = ParseOption(Options, "BugLoc");
	BugRotString = ParseOption(Options, "BugRot");

	if (BaseMutator != none)
	{
		BaseMutator.InitMutator(Options, ErrorMessage);
	}

	// Cache a pointer to the online subsystem
	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		// And grab one for the game interface since it will be used often
		GameInterface = OnlineSub.GameInterface;
		if (GameInterface != None)
		{
			// Grab the current game settings object out
			GameSettings = GameInterface.GetGameSettings(PlayerReplicationInfoClass.default.SessionName);
			if (GameSettings != None)
			{
				// Check for an arbitrated match
				bUsingArbitration = GameSettings.bUsesArbitration;
			}
		}
	}

	if ((WorldInfo.IsConsoleBuild(CONSOLE_Any) == false) &&
		(WorldInfo.NetMode != NM_Standalone) &&
		// Don't register the session if the UI already has
		(GameSettings == None))
	{
		// Cache this so it can be used later by async processes
		ServerOptions = Options;
		// If there isn't a login to process, immediately register the server
		// Otherwise the server will be registered when the login completes
		if (!ProcessServerLogin())
		{
			RegisterServer();
		}
	}
}

/** Called when a connection closes before getting to PostLogin() */
event NotifyPendingConnectionLost();

function AddMutator(string mutname, optional bool bUserAdded)
{
	local class<Mutator> mutClass;
	local Mutator mut;
	local int i;

	if ( !Static.AllowMutator(MutName) )
		return;

	mutClass = class<Mutator>(DynamicLoadObject(mutname, class'Class'));
	if (mutClass == None)
		return;

	if (mutClass.Default.GroupNames.length > 0 && BaseMutator != None)
	{
		// make sure no mutators with same groupname
		for (mut = BaseMutator; mut != None; mut = mut.NextMutator)
		{
			for (i = 0; i < mut.GroupNames.length; i++)
			{
				if (mutClass.default.GroupNames.Find(mut.GroupNames[i]) != INDEX_NONE)
				{
					`log("Not adding "$mutClass$" because already have a mutator in the same group - "$mut);
					return;
				}
			}
		}
	}

	// make sure this mutator is not added already
	for ( mut=BaseMutator; mut!=None; mut=mut.NextMutator )
		if ( mut.Class == mutClass )
		{
			`log("Not adding "$mutClass$" because this mutator is already added - "$mut);
			return;
		}

	mut = Spawn(mutClass);
	// mc, beware of mut being none
	if (mut == None)
		return;

	// Meant to verify if this mutator was from Command Line parameters or added from other Actors
	mut.bUserAdded = bUserAdded;

	if (BaseMutator == None)
	{
		BaseMutator = mut;
	}
	else
	{
		BaseMutator.AddMutator(mut);
	}
}

/* RemoveMutator()
Remove a mutator from the mutator list
*/
function RemoveMutator( Mutator MutatorToRemove )
{
	local Mutator M;

	// remove from mutator list
	if ( BaseMutator == MutatorToRemove )
	{
		BaseMutator = MutatorToRemove.NextMutator;
	}
	else if ( BaseMutator != None )
	{
		for ( M=BaseMutator; M!=None; M=M.NextMutator )
		{
			if ( M.NextMutator == MutatorToRemove )
			{
				M.NextMutator = MutatorToRemove.NextMutator;
				break;
			}
		}
	}
}

/* ProcessServerTravel()
 Optional handling of ServerTravel for network games.
*/
function ProcessServerTravel(string URL, optional bool bAbsolute)
{
	local PlayerController LocalPlayer;
	local bool bSeamless;
	local string NextMap;
	local Guid NextMapGuid;
	local int OptionStart;

	bLevelChange = true;
	EndLogging("mapchange");

	// force an old style load screen if the server has been up for a long time so that TimeSeconds doesn't overflow and break everything
	bSeamless = (bUseSeamlessTravel && WorldInfo.TimeSeconds < 172800.0f); // 172800 seconds == 48 hours

	if (InStr(Caps(URL), "?RESTART") != INDEX_NONE)
	{
		NextMap = string(WorldInfo.GetPackageName());
	}
	else
	{
		OptionStart = InStr(URL, "?");
		if (OptionStart == INDEX_NONE)
		{
			NextMap = URL;
		}
		else
		{
			NextMap = Left(URL, OptionStart);
		}
	}
	NextMapGuid = GetPackageGuid(name(NextMap));

	// Notify clients we're switching level and give them time to receive.
	LocalPlayer = ProcessClientTravel(URL, NextMapGuid, bSeamless, bAbsolute);

	`log("ProcessServerTravel:"@URL);
	WorldInfo.NextURL = URL;
	if (WorldInfo.NetMode == NM_ListenServer && LocalPlayer != None)
	{
		WorldInfo.NextURL $= "?Team="$LocalPlayer.GetDefaultURL("Team")
							$"?Name="$LocalPlayer.GetDefaultURL("Name")
							$"?Class="$LocalPlayer.GetDefaultURL("Class")
							$"?Character="$LocalPlayer.GetDefaultURL("Character");
	}


	// Notify access control, to cleanup online subsystem references
	if (AccessControl != none)
	{
		AccessControl.NotifyServerTravel(bSeamless);
	}

	// Trigger cleanup of online delegates
	ClearOnlineDelegates();

	if (bSeamless)
	{
		WorldInfo.SeamlessTravel(WorldInfo.NextURL, bAbsolute);
		WorldInfo.NextURL = "";
	}
	// Switch immediately if not networking.
	else if (WorldInfo.NetMode != NM_DedicatedServer && WorldInfo.NetMode != NM_ListenServer)
	{
		WorldInfo.NextSwitchCountdown = 0.0;
	}
}

/**
 * Notifies all clients to travel to the specified URL.
 *
 * @param	URL				a string containing the mapname (or IP address) to travel to, along with option key/value pairs
 * @param	NextMapGuid		the GUID of the server's version of the next map
 * @param	bSeamless		indicates whether the travel should use seamless travel or not.
 * @param	bAbsolute		indicates which type of travel the server will perform (i.e. TRAVEL_Relative or TRAVEL_Absolute)
 */
function PlayerController ProcessClientTravel( out string URL, Guid NextMapGuid, bool bSeamless, bool bAbsolute )
{
	local PlayerController P, LP;

	// We call PreClientTravel directly on any local PlayerPawns (ie listen server)
	foreach WorldInfo.AllControllers(class'PlayerController', P)
	{
		if ( NetConnection(P.Player) != None )
		{
			// remote player
			P.ClientTravel(URL, TRAVEL_Relative, bSeamless, NextMapGuid);
		}
		else
		{
			// local player
			LP = P;
			P.PreClientTravel(URL, bAbsolute ? TRAVEL_Absolute : TRAVEL_Relative, bSeamless);
		}
	}

	return LP;
}

function bool RequiresPassword()
{
	return ( (AccessControl != None) && AccessControl.RequiresPassword() );
}

//
// Accept or reject a player on the server.
// Fails login if you set the Error to a non-empty string.
// NOTE: UniqueId should not be trusted at this stage, it requires authentication
//
event PreLogin(string Options, string Address, const UniqueNetId UniqueId, bool bSupportsAuth, out string ErrorMessage)
{
	local bool bSpectator;
	local bool bPerfTesting;

	// Check for an arbitrated match in progress and kick if needed
	if (WorldInfo.NetMode != NM_Standalone && bUsingArbitration && bHasArbitratedHandshakeBegun)
	{
		ErrorMessage = PathName(WorldInfo.Game.GameMessageClass) $ ".ArbitrationMessage";
		return;
	}

	// If this player is banned, reject him
	if (AccessControl != none && AccessControl.IsIDBanned(UniqueId))
	{
		`log(Address@"is banned, rejecting...");
		ErrorMessage = "Engine.AccessControl.SessionBanned";
		return;
	}


	bPerfTesting = ( ParseOption( Options, "AutomatedPerfTesting" ) ~= "1" );
	bSpectator = bPerfTesting || ( ParseOption( Options, "SpectatorOnly" ) ~= "1" ) || ( ParseOption( Options, "CauseEvent" ) ~= "FlyThrough" );

	if (AccessControl != None)
	{
		AccessControl.PreLogin(Options, Address, UniqueId, bSupportsAuth, ErrorMessage, bSpectator);
	}
}

/**
 * If called from within PreLogin, pauses the login process for the currently connecting client (usually to delay login for authentication)
 *
 * @return		A reference to the Player/NetConnection representing the connecting client; used to resume the login process
 */
native static final function Player PauseLogin();

/**
 * Resumes the login process for the specified client Player/NetConnection
 *
 * @param InPlayer	The Player/NetConnection to resume logging in
 */
native static final function ResumeLogin(Player InPlayer);

/**
 * Rejects login for the specified client/NetConnection, with the specified error message
 * NOTE: Error is the same error format PreLogin would take in OutError
 * NOTE: Not restricted to clients at login; can be used on any valid NetConnection to disconnect a client
 *
 * @param InPlayer	The Player/NetConnection to reject from the server
 * @param Error		The error message to give the player
 */
native static final function RejectLogin(Player InPlayer, string Error);

/* 
 * Is the server currently at capacity?
 * @param bSpectator - Whether we should check against player or spectator limits
 * @return TRUE if the server is full, FALSE otherwise
 */
function bool AtCapacity(bool bSpectator)
{
	if ( WorldInfo.NetMode == NM_Standalone )
		return false;

	if ( bSpectator )
		return ( (NumSpectators >= MaxSpectators)
			&& ((WorldInfo.NetMode != NM_ListenServer) || (NumPlayers > 0)) );
	else
		return ( (MaxPlayers>0) && (GetNumPlayers() >= MaxPlayers) );
}

native final function int GetNextPlayerID();

/** spawns a PlayerController at the specified location; split out from Login()/HandleSeamlessTravelPlayer() for easier overriding */
function PlayerController SpawnPlayerController(vector SpawnLocation, rotator SpawnRotation)
{
	return Spawn(PlayerControllerClass,,, SpawnLocation, SpawnRotation);
}

//
// Log a player in.
// Fails login if you set the Error string.
// PreLogin is called before Login, but significant game time may pass before
// Login is called, especially if content is downloaded.
// NOTE: If the AccessControl utilizes PauseLogin, UniqueID should be fully authenticated here
//
event PlayerController Login(string Portal, string Options, const UniqueNetID UniqueID, out string ErrorMessage)
{
	local NavigationPoint StartSpot;
	local PlayerController NewPlayer;
	local string InName, InCharacter/*, InAdminName*/, InPassword;
	local byte InTeam;
	local bool bSpectator, bAdmin, bPerfTesting;
	local rotator SpawnRotation;
	local UniqueNetId ZeroId;

	bAdmin = false;

	// Kick the player if they joined during the handshake process
	if (bUsingArbitration && bHasArbitratedHandshakeBegun)
	{
		ErrorMessage = PathName(WorldInfo.Game.GameMessageClass) $ ".ArbitrationMessage";
		return None;
	}

	if ( BaseMutator != None )
		BaseMutator.ModifyLogin(Portal, Options);

	bPerfTesting = ( ParseOption( Options, "AutomatedPerfTesting" ) ~= "1" );
	bSpectator = bPerfTesting || ( ParseOption( Options, "SpectatorOnly" ) ~= "1" );

	// Get URL options.
	InName     = Left(ParseOption ( Options, "Name"), 20);
	InTeam     = GetIntOption( Options, "Team", 255 ); // default to "no team"
	//InAdminName= ParseOption ( Options, "AdminName");
	InPassword = ParseOption ( Options, "Password" );
	//InChecksum = ParseOption ( Options, "Checksum" );

	if ( AccessControl != None )
	{
		bAdmin = AccessControl.ParseAdminOptions(Options);
	}

	// Make sure there is capacity except for admins. (This might have changed since the PreLogin call).
	if ( !bAdmin && AtCapacity(bSpectator) )
	{
		ErrorMessage = PathName(WorldInfo.Game.GameMessageClass) $ ".MaxedOutMessage";
		return None;
	}

	// if this player is banned, kick him
	if( ( WorldInfo.Game.AccessControl != none ) && (WorldInfo.Game.AccessControl.IsIDBanned(UniqueId)) )
	{
		`Log(InName @ "is banned, rejecting...");
		ErrorMessage = "Engine.AccessControl.SessionBanned";
		return None;
	}

	// If admin, force spectate mode if the server already full of reg. players
	if ( bAdmin && AtCapacity(false) )
	{
		bSpectator = true;
	}

	// Pick a team (if need teams)
	InTeam = PickTeam(InTeam,None);

	// Find a start spot.
	StartSpot = FindPlayerStart( None, InTeam, Portal );

	if( StartSpot == None )
	{
		ErrorMessage = PathName(WorldInfo.Game.GameMessageClass) $ ".FailedPlaceMessage";
		return None;
	}

	SpawnRotation.Yaw = StartSpot.Rotation.Yaw;
	NewPlayer = SpawnPlayerController(StartSpot.Location, SpawnRotation);

	// Handle spawn failure.
	if( NewPlayer == None )
	{
		`log("Couldn't spawn player controller of class "$PlayerControllerClass);
		ErrorMessage = PathName(WorldInfo.Game.GameMessageClass) $ ".FailedSpawnMessage";
		return None;
	}

	NewPlayer.StartSpot = StartSpot;

	// Set the player's ID.
	NewPlayer.PlayerReplicationInfo.PlayerID = GetNextPlayerID();

	// If the access control is currently authenticating the players UID, don't store the UID until it is authenticated
	if (AccessControl == none || !AccessControl.IsPendingAuth(UniqueId))
	{
		NewPlayer.PlayerReplicationInfo.SetUniqueId(UniqueId);
	}

	if (OnlineSub != None && 
		OnlineSub.GameInterface != None &&
		UniqueId != ZeroId)
	{
		// Go ahead and register the player as part of the session
		WorldInfo.Game.OnlineSub.GameInterface.RegisterPlayer(PlayerReplicationInfoClass.default.SessionName, UniqueId, HasOption(Options, "bIsFromInvite"));
	}
	// Now that the unique id is replicated, this player can contribute to skill
	RecalculateSkillRating();

	// Init player's name
	if( InName=="" )
	{
		InName=DefaultPlayerName$NewPlayer.PlayerReplicationInfo.PlayerID;
	}

	ChangeName( NewPlayer, InName, false );

	InCharacter = ParseOption(Options, "Character");
	NewPlayer.SetCharacter(InCharacter);

	if ( bSpectator || NewPlayer.PlayerReplicationInfo.bOnlySpectator || !ChangeTeam(newPlayer, InTeam, false) )
	{
		NewPlayer.GotoState('Spectating');
		NewPlayer.PlayerReplicationInfo.bOnlySpectator = true;
		NewPlayer.PlayerReplicationInfo.bIsSpectator = true;
		NewPlayer.PlayerReplicationInfo.bOutOfLives = true;
		return NewPlayer;
	}

	// perform auto-login if admin password/name was passed on the url
	if ( AccessControl != None && AccessControl.AdminLogin(NewPlayer, InPassword) )
	{
		AccessControl.AdminEntered(NewPlayer);
	}


	// if delayed start, don't give a pawn to the player yet
	// Normal for multiplayer games
	if ( bDelayedStart )
	{
		// @todo ib2merge: Chair had this commented out
		NewPlayer.GotoState('PlayerWaiting');
		return NewPlayer;
	}

	return newPlayer;
}

/* StartMatch()
Start the game - inform all actors that the match is starting, and spawn player pawns
*/
function StartMatch()
{
	local Actor A;

	if ( MyAutoTestManager != None )
	{
		MyAutoTestManager.StartMatch();
	}

	// tell all actors the game is starting
	ForEach AllActors(class'Actor', A)
	{
		A.MatchStarting();
	}

	// start human players first
	StartHumans();

	// start AI players
	StartBots();

	bWaitingToStartMatch = false;

	StartOnlineGame();

	// fire off any level startup events
	WorldInfo.NotifyMatchStarted();
}

/**
 * Tells the online system to start the game and waits for the callback. Tells
 * each connected client to mark their session as in progress
 */
function StartOnlineGame()
{
	local PlayerController PC;

	if (GameInterface != None)
	{
		// Tell clients to mark their game as started
		foreach WorldInfo.AllControllers(class'PlayerController',PC)
		{
			// Skip notifying local PCs as they are handled automatically
			if (!PC.IsLocalPlayerController())
			{
				PC.ClientStartOnlineGame();
			}
		}
		// Register the start callback so that the stat guid can be read
		GameInterface.AddStartOnlineGameCompleteDelegate(OnStartOnlineGameComplete);
		// Start the game locally and wait for it to complete
		GameInterface.StartOnlineGame(PlayerReplicationInfoClass.default.SessionName);
	}
	else
	{
		// Notify all clients that the match has begun
		GameReplicationInfo.StartMatch();
	}
}

/**
 * Callback when the start completes
 *
 * @param SessionName the name of the session this is for
 * @param bWasSuccessful true if it worked, false otherwise
 */
function OnStartOnlineGameComplete(name SessionName,bool bWasSuccessful)
{
	local PlayerController PC;
	local string StatGuid;

	GameInterface.ClearStartOnlineGameCompleteDelegate(OnStartOnlineGameComplete);
	if (bWasSuccessful && OnlineSub.StatsInterface != None)
	{
		// Get the stat guid for the server
		StatGuid = OnlineSub.StatsInterface.GetHostStatGuid();
		if (StatGuid != "")
		{
			// Send the stat guid to all clients
			foreach WorldInfo.AllControllers(class'PlayerController',PC)
			{
				if (PC.IsLocalPlayerController() == false)
				{
					PC.ClientRegisterHostStatGuid(StatGuid);
				}
			}
		}
	}

	// Notify all clients that the match has begun
	GameReplicationInfo.StartMatch();
}

function StartHumans()
{
	local PlayerController P;

	foreach WorldInfo.AllControllers(class'PlayerController', P)
	{
		if (P.Pawn == None)
		{
			if ( bGameEnded )
			{
				return; // telefrag ended the game with ridiculous frag limit
			}
			else if (P.CanRestartPlayer())
			{
				RestartPlayer(P);
			}
		}
	}
}

function StartBots()
{
	local Controller P;

	foreach WorldInfo.AllControllers(class'Controller', P)
	{
		if (P.bIsPlayer && !P.IsA('PlayerController'))
		{
			if (WorldInfo.NetMode == NM_Standalone)
			{
				RestartPlayer(P);
			}
			else
			{
				P.GotoState('Dead','MPStart');
			}
		}
	}
}
//
// Restart a player.
//
function RestartPlayer(Controller NewPlayer)
{
	local NavigationPoint startSpot;
	local int TeamNum, Idx;
	local array<SequenceObject> Events;
	local SeqEvent_PlayerSpawned SpawnedEvent;
	local LocalPlayer LP; 
	local PlayerController PC; 

	if( bRestartLevel && WorldInfo.NetMode!=NM_DedicatedServer && WorldInfo.NetMode!=NM_ListenServer )
	{
		`warn("bRestartLevel && !server, abort from RestartPlayer"@WorldInfo.NetMode);
		return;
	}
	// figure out the team number and find the start spot
	TeamNum = ((NewPlayer.PlayerReplicationInfo == None) || (NewPlayer.PlayerReplicationInfo.Team == None)) ? 255 : NewPlayer.PlayerReplicationInfo.Team.TeamIndex;
	StartSpot = FindPlayerStart(NewPlayer, TeamNum);

	// if a start spot wasn't found,
	if (startSpot == None)
	{
		// check for a previously assigned spot
		if (NewPlayer.StartSpot != None)
		{
			StartSpot = NewPlayer.StartSpot;
			`warn("Player start not found, using last start spot");
		}
		else
		{
			// otherwise abort
			`warn("Player start not found, failed to restart player");
			return;
		}
	}
	// try to create a pawn to use of the default class for this player
	if (NewPlayer.Pawn == None)
	{
		NewPlayer.Pawn = SpawnDefaultPawnFor(NewPlayer, StartSpot);
	}
	if (NewPlayer.Pawn == None)
	{
		`log("failed to spawn player at "$StartSpot);
		NewPlayer.GotoState('Dead');
		if ( PlayerController(NewPlayer) != None )
		{
			PlayerController(NewPlayer).ClientGotoState('Dead','Begin');
		}
	}
	else
	{
		// initialize and start it up
		NewPlayer.Pawn.SetAnchor(startSpot);
		if ( PlayerController(NewPlayer) != None )
		{
			PlayerController(NewPlayer).TimeMargin = -0.1;
			startSpot.AnchoredPawn = None; // SetAnchor() will set this since IsHumanControlled() won't return true for the Pawn yet
		}
		NewPlayer.Pawn.LastStartSpot = PlayerStart(startSpot);
		NewPlayer.Pawn.LastStartTime = WorldInfo.TimeSeconds;
		NewPlayer.Possess(NewPlayer.Pawn, false);
		NewPlayer.Pawn.PlayTeleportEffect(true, true);
		NewPlayer.ClientSetRotation(NewPlayer.Pawn.Rotation, TRUE);

		if (!WorldInfo.bNoDefaultInventoryForPlayer)
		{
			AddDefaultInventory(NewPlayer.Pawn);
		}
		SetPlayerDefaults(NewPlayer.Pawn);

		// activate spawned events
		if (WorldInfo.GetGameSequence() != None)
		{
			WorldInfo.GetGameSequence().FindSeqObjectsByClass(class'SeqEvent_PlayerSpawned',TRUE,Events);
			for (Idx = 0; Idx < Events.Length; Idx++)
			{
				SpawnedEvent = SeqEvent_PlayerSpawned(Events[Idx]);
				if (SpawnedEvent != None &&
					SpawnedEvent.CheckActivate(NewPlayer,NewPlayer))
				{
					SpawnedEvent.SpawnPoint = startSpot;
					SpawnedEvent.PopulateLinkedVariableValues();
				}
			}
		}
	}

	// To fix custom post processing chain when not running in editor or PIE.
	PC = PlayerController(NewPlayer);
	if (PC != none)
	{
		LP = LocalPlayer(PC.Player); 
		if(LP != None) 
		{ 
			LP.RemoveAllPostProcessingChains(); 
			LP.InsertPostProcessingChain(LP.Outer.GetWorldPostProcessChain(),INDEX_NONE,true); 
			if(PC.myHUD != None)
			{
				PC.myHUD.NotifyBindPostProcessEffects();
			}
		} 
	}
}

/**
 * Returns a pawn of the default pawn class
 *
 * @param	NewPlayer - Controller for whom this pawn is spawned
 * @param	StartSpot - PlayerStart at which to spawn pawn
 *
 * @return	pawn
 */
function Pawn SpawnDefaultPawnFor(Controller NewPlayer, NavigationPoint StartSpot)
{
	local class<Pawn> DefaultPlayerClass;
	local Rotator StartRotation;
	local Pawn ResultPawn;

	DefaultPlayerClass = GetDefaultPlayerClass(NewPlayer);

	// don't allow pawn to be spawned with any pitch or roll
	StartRotation.Yaw = StartSpot.Rotation.Yaw;

	ResultPawn = Spawn(DefaultPlayerClass,,,StartSpot.Location,StartRotation);
	if ( ResultPawn == None )
	{
		`log("Couldn't spawn player of type "$DefaultPlayerClass$" at "$StartSpot);
	}
	return ResultPawn;
}

/**
 * Returns the default pawn class for the specified controller,
 *
 * @param	C - controller to figure out pawn class for
 *
 * @return	default pawn class
 */
function class<Pawn> GetDefaultPlayerClass(Controller C)
{
	// default to the game specified pawn class
	return DefaultPawnClass;
}

/** replicates the current level streaming status to the given PlayerController */
function ReplicateStreamingStatus(PlayerController PC)
{
	local int LevelIndex;
	local LevelStreaming TheLevel;

	// don't do this for local players or players after the first on a splitscreen client
	if (LocalPlayer(PC.Player) == None && ChildConnection(PC.Player) == None)
	{
		// if we've loaded levels via CommitMapChange() that aren't normally in the StreamingLevels array, tell the client about that
		if (WorldInfo.CommittedPersistentLevelName != 'None')
		{
			PC.ClientPrepareMapChange(WorldInfo.CommittedPersistentLevelName, true, true);
			// tell the client to commit the level immediately
			PC.ClientCommitMapChange();
		}

		if (WorldInfo.StreamingLevels.length > 0)
		{
	 		// Tell the player controller the current streaming level status
	 		for (LevelIndex = 0; LevelIndex < WorldInfo.StreamingLevels.Length; LevelIndex++)
	 		{
				// streamingServer
				TheLevel = WorldInfo.StreamingLevels[LevelIndex];

				if( TheLevel != none )
				{
					`log( "levelStatus: " $ TheLevel.PackageName $ " "
						$ TheLevel.bShouldBeVisible  $ " "
						$ TheLevel.bIsVisible  $ " "
						$ TheLevel.bShouldBeLoaded  $ " "
						$ TheLevel.LoadedLevel  $ " "
						$ TheLevel.bHasLoadRequestPending  $ " "
						) ;

	 				PC.ClientUpdateLevelStreamingStatus(
	 					TheLevel.PackageName,
	 					TheLevel.bShouldBeLoaded,
	 					TheLevel.bShouldBeVisible,
	 					TheLevel.bShouldBlockOnLoad);
	 			}
			}
	 		PC.ClientFlushLevelStreaming();
		}

		// if we're preparing to load different levels using PrepareMapChange() inform the client about that now
		if (WorldInfo.PreparingLevelNames.length > 0)
		{
			for (LevelIndex = 0; LevelIndex < WorldInfo.PreparingLevelNames.length; LevelIndex++)
			{
				PC.ClientPrepareMapChange(WorldInfo.PreparingLevelNames[LevelIndex], LevelIndex == 0, LevelIndex == WorldInfo.PreparingLevelNames.length - 1);
			}
			// DO NOT commit these changes yet - we'll send that when we're done preparing them
		}
	}
}

/** handles all player initialization that is shared between the travel methods
 * (i.e. called from both PostLogin() and HandleSeamlessTravelPlayer())
 */
function GenericPlayerInitialization(Controller C)
{
	local PlayerController PC;

	PC = PlayerController(C);
	if (PC != None)
	{
		// Keep track of the best host to migrate to in case of a disconnect
		UpdateBestNextHosts();

		// Notify the game that we can now be muted and mute others
		UpdateGameplayMuteList(PC);

		// tell client what hud class to use
		PC.ClientSetHUD(HudType);

		// tell client what secondary hud class to use
		PC.ClientSetSecondaryHUD(SecondaryHudType);

		ReplicateStreamingStatus(PC);

		// see if we need to spawn a CoverReplicator for this player
		if (CoverReplicatorBase != None)
		{
			PC.SpawnCoverReplicator();
		}

		// Set the rich presence strings on the client (has to be done there)
		PC.ClientSetOnlineStatus();
	}

	if (BaseMutator != None)
	{
		BaseMutator.NotifyLogin(C);
	}
}

/**
 * Sort the list of best hosts. Clients with the most peer connections come first.
 * Then sort based on time of join so that newest players are preferred.
 *
 * @param A first item to compare
 * @param B second item to compare
 * @return 0 if A==B, < 0 if A < B, > 0 if A > B
 */
function int BestNextHostSort(PlayerController A, PlayerController B)
{
	local int Result;

	if (A.ConnectedPeers.Length == B.ConnectedPeers.Length &&
		A.PlayerReplicationInfo != None && B.PlayerReplicationInfo != None)
	{
		// sort by newest start time
		Result = FCeil(B.PlayerReplicationInfo.StartTime) - FCeil(A.PlayerReplicationInfo.StartTime);
	}
	else
	{
		// sort by largest connected peers
		Result = A.ConnectedPeers.Length - B.ConnectedPeers.Length;
	}

	return Result;
}

/**
 * Updates the list of best next hosts on the current server and also replicates this list to all clients.
 */
function UpdateBestNextHosts()
{
	local PlayerController PC;
	local array<PlayerController> SortedPCList;
	local UniqueNetId SortedPlayerIdList[10];
	local UniqueNetId ZeroId;
	local int Idx,NumEntries;

	// copy list of remote PCs
	foreach WorldInfo.AllControllers(class'PlayerController',PC)
	{
		if (!PC.IsLocalPlayerController() && 
			PC.PlayerReplicationInfo != None &&
			PC.PlayerReplicationInfo.UniqueId != ZeroId &&
			PC.IsPrimaryPlayer())
		{
			SortedPCList.AddItem(PC);
		}
	}
	// sort list of PCs from best to worst host
	SortedPCList.Sort(BestNextHostSort);

	// copy to list of unique net ids
	NumEntries = Min(SortedPCList.Length,10);
	for (Idx=0; Idx < NumEntries; Idx++)
	{
		SortedPlayerIdList[Idx] = SortedPCList[Idx].PlayerReplicationInfo.UniqueId;
	}

	// send list of best hosts to clients
	foreach WorldInfo.AllControllers(class'PlayerController',PC)
	{
		if (!PC.IsLocalPlayerController())
		{
			PC.ClientUpdateBestNextHosts(SortedPlayerIdList,NumEntries);
		}
	}
}

//
// Called after a successful login. This is the first place
// it is safe to call replicated functions on the PlayerController.
//
event PostLogin( PlayerController NewPlayer )
{
	local string Address, StatGuid;
	local int pos, i;
	local Sequence GameSeq;
	local array<SequenceObject> AllInterpActions;

	local int HidePlayer, HideHud, DisableMovement, DisableTurning, DisableInput;

	// update player count
	if (NewPlayer.PlayerReplicationInfo.bOnlySpectator)
	{
		NumSpectators++;
	}
	else if (WorldInfo.IsInSeamlessTravel() || NewPlayer.HasClientLoadedCurrentWorld())
	{
		NumPlayers++;
	}
	else
	{
		NumTravellingPlayers++;
	}

	// Tell the online subsystem the number of players in the game
	UpdateGameSettingsCounts();

	// save network address for re-associating with reconnecting player, after stripping out port number
	Address = NewPlayer.GetPlayerNetworkAddress();
	pos = InStr(Address,":");
	NewPlayer.PlayerReplicationInfo.SavedNetworkAddress = (pos > 0) ? left(Address,pos) : Address;

	// check if this player is reconnecting and already has PRI
	FindInactivePRI(NewPlayer);

	if ( !bDelayedStart )
	{
		// start match, or let player enter, immediately
		bRestartLevel = false;	// let player spawn once in levels that must be restarted after every death
		if ( 	bWaitingToStartMatch )
			StartMatch();
		else
			RestartPlayer(newPlayer);
		bRestartLevel = Default.bRestartLevel;
	}

	if (NewPlayer.Pawn != None)
	{
		NewPlayer.Pawn.ClientSetRotation(NewPlayer.Pawn.Rotation);
	}

	NewPlayer.ClientCapBandwidth(NewPlayer.Player.CurrentNetSpeed);
	UpdateNetSpeeds();

	GenericPlayerInitialization(NewPlayer);

	// Tell the new player the stat guid
	if (GameReplicationInfo.bMatchHasBegun && OnlineSub != None && OnlineSub.StatsInterface != None)
	{
		// Get the stat guid for the server
		StatGuid = OnlineSub.StatsInterface.GetHostStatGuid();
		if (StatGuid != "")
		{
			NewPlayer.ClientRegisterHostStatGuid(StatGuid);
		}
	}

	// Tell the player to disable voice by default and use the push to talk method
	if (bRequiresPushToTalk)
	{
		NewPlayer.ClientStopNetworkedVoice();
	}
	else
	{
		NewPlayer.ClientStartNetworkedVoice();
	}

	if (NewPlayer.PlayerReplicationInfo.bOnlySpectator)
	{
		NewPlayer.ClientGotoState('Spectating');
	}

	// add the player to any matinees running so that it gets in on any cinematics already running, etc
	GameSeq = WorldInfo.GetGameSequence();
	if (GameSeq != None)
	{
		// find any matinee actions that exist
		GameSeq.FindSeqObjectsByClass(class'SeqAct_Interp', true, AllInterpActions);

		// tell them all to add this PC to any running Director tracks
		for (i = 0; i < AllInterpActions.Length; i++)
		{
			SeqAct_Interp(AllInterpActions[i]).AddPlayerToDirectorTracks(NewPlayer);
		}
	}

	//Check to see if we should start in cinematic mode (matinee movie capture)
	if(ShouldStartInCinematicMode(HidePlayer, HideHud, DisableMovement, DisableTurning, DisableInput))
	{
		NewPlayer.SetCinematicMode(true, HidePlayer == 1, HideHud == 1, DisableMovement == 1, DisableTurning == 1, DisableInput == 1);
	}

	// Pass on to access control
	if (AccessControl != none)
	{
		AccessControl.PostLogin(NewPlayer);
	}
}

function UpdateNetSpeeds()
{
	local int NewNetSpeed;
	local PlayerController PC;
	local OnlineGameSettings GameSettings;

	if (GameInterface != None)
	{
		GameSettings = GameInterface.GetGameSettings(PlayerReplicationInfoClass.default.SessionName);
	}

	if ( (WorldInfo.NetMode == NM_DedicatedServer) || (WorldInfo.NetMode == NM_Standalone) || (GameSettings != None && GameSettings.bIsLanMatch) )
	{
		return;
	}

	if ( WorldInfo.TimeSeconds - LastNetSpeedUpdateTime < 1.0 )
	{
		SetTimer( 1.0, false, nameof(UpdateNetSpeeds) );
		return;
	}

	LastNetSpeedUpdateTime = WorldInfo.TimeSeconds;

	NewNetSpeed = CalculatedNetSpeed();
	`log("New Dynamic NetSpeed "$NewNetSpeed$" vs old "$AdjustedNetSpeed,,'DevNet');

	if ( AdjustedNetSpeed != NewNetSpeed )
	{
		AdjustedNetSpeed = NewNetSpeed;
		ForEach WorldInfo.AllControllers(class'PlayerController', PC)
		{
			PC.SetNetSpeed(AdjustedNetSpeed);
		}
	}
}

function int CalculatedNetSpeed()
{
	return Clamp(TotalNetBandwidth/Max(NumPlayers,1), MinDynamicBandwidth, MaxDynamicBandwidth);
}

/**
 * Engine is shutting down.
 */
event PreExit()
{
	if (AccessControl != none)
	{
		AccessControl.NotifyExit();
	}

	ClearOnlineDelegates();
}

//
// Player exits.
//
function Logout( Controller Exiting )
{
	local PlayerController PC;
	local int PCIndex;

	PC = PlayerController(Exiting);
	if ( PC != None )
	{
		if (AccessControl != None &&
			AccessControl.AdminLogout( PlayerController(Exiting) ))
		{
			AccessControl.AdminExited( PlayerController(Exiting) );
		}

		if ( PC.PlayerReplicationInfo.bOnlySpectator )
		{
			NumSpectators--;
		}
		else
		{
			if (WorldInfo.IsInSeamlessTravel() || PC.HasClientLoadedCurrentWorld())
			{
				NumPlayers--;
			}
			else
			{
				NumTravellingPlayers--;
			}
			// Tell the online subsystem the number of players in the game
			UpdateGameSettingsCounts();
		}
		// This person has left during an arbitration period
		if (bUsingArbitration && bHasArbitratedHandshakeBegun && !bHasEndGameHandshakeBegun)
		{
			`Log("Player "$PC.PlayerReplicationInfo.PlayerName$" has dropped");
		}
		// Unregister the player from the online layer
		UnregisterPlayer(PC);
		// Remove from the arbitrated PC list if in an arbitrated match
		if (bUsingArbitration)
		{
			// Find the PC in the list and remove it if found
			PCIndex = ArbitrationPCs.Find(PC);
			if (PCIndex != INDEX_NONE)
			{
				ArbitrationPCs.Remove(PCIndex,1);
			}
		}
	}
	//notify mutators that a player exited
	if (BaseMutator != None)
	{
		BaseMutator.NotifyLogout(Exiting);
	}
	if ( PC != None )
	{
		UpdateNetSpeeds();
	}
}

/**
 * Removes the player from the named session when they leave
 *
 * @param PC the player controller that just left
 */
function UnregisterPlayer(PlayerController PC)
{
	local UniqueNetId ZeroId;

	// If there is a session that matches the name, unregister this remote player
	if (WorldInfo.NetMode != NM_Standalone &&
		GameInterface != None &&
		PC.PlayerReplicationInfo.UniqueId != ZeroId &&
		GameInterface.GetGameSettings(PC.PlayerReplicationInfo.SessionName) != None)
	{
		// Unregister the player from the session
		GameInterface.UnregisterPlayer(PC.PlayerReplicationInfo.SessionName,PC.PlayerReplicationInfo.UniqueId);
	}
}

//
// Examine the passed player's inventory, and accept or discard each item.
// AcceptInventory needs to gracefully handle the case of some inventory
// being accepted but other inventory not being accepted (such as the default
// weapon).  There are several things that can go wrong: A weapon's
// AmmoType not being accepted but the weapon being accepted -- the weapon
// should be killed off. Or the player's selected inventory item, active
// weapon, etc. not being accepted, leaving the player weaponless or leaving
// the HUD inventory rendering messed up (AcceptInventory should pick another
// applicable weapon/item as current).
//
event AcceptInventory(pawn PlayerPawn)
{
	//default accept all inventory except default weapon (spawned explicitly)
}

//
// Spawn any default inventory for the player.
//
event AddDefaultInventory(Pawn P)
{
    // Allow the pawn itself to modify its inventory
    P.AddDefaultInventory();

	if ( P.InvManager == None )
	{
		`warn("GameInfo::AddDefaultInventory - P.InvManager == None");
	}
}

/* Mutate()
Pass an input string to the mutator list.  Used by PlayerController.Mutate(), intended to allow
mutators to have input exec functions (by binding mutate xxx to keys)
*/
function Mutate(string MutateString, PlayerController Sender)
{
	if ( BaseMutator != None )
		BaseMutator.Mutate(MutateString, Sender);
}

/* SetPlayerDefaults()
 first make sure pawn properties are back to default, then give mutators an opportunity
 to modify them
*/
function SetPlayerDefaults(Pawn PlayerPawn)
{
	PlayerPawn.AirControl = PlayerPawn.Default.AirControl;
	PlayerPawn.GroundSpeed = PlayerPawn.Default.GroundSpeed;
	PlayerPawn.WaterSpeed = PlayerPawn.Default.WaterSpeed;
	PlayerPawn.AirSpeed = PlayerPawn.Default.AirSpeed;
	PlayerPawn.Acceleration = PlayerPawn.Default.Acceleration;
	PlayerPawn.AccelRate = PlayerPawn.Default.AccelRate;
	PlayerPawn.JumpZ = PlayerPawn.Default.JumpZ;
	if ( BaseMutator != None )
		BaseMutator.ModifyPlayer(PlayerPawn);
	PlayerPawn.PhysicsVolume.ModifyPlayer(PlayerPawn);
}

function NotifyKilled(Controller Killer, Controller Killed, Pawn KilledPawn, class<DamageType> damageType )
{
	local Controller C;

	foreach WorldInfo.AllControllers(class'Controller', C)
	{
		C.NotifyKilled(Killer, Killed, KilledPawn, damageType);
	}
}

function Killed( Controller Killer, Controller KilledPlayer, Pawn KilledPawn, class<DamageType> damageType )
{
    if( KilledPlayer != None && KilledPlayer.bIsPlayer )
	{
		KilledPlayer.PlayerReplicationInfo.IncrementDeaths();
		KilledPlayer.PlayerReplicationInfo.SetNetUpdateTime(FMin(KilledPlayer.PlayerReplicationInfo.NetUpdateTime, WorldInfo.TimeSeconds + 0.3 * FRand()));
		BroadcastDeathMessage(Killer, KilledPlayer, damageType);
	}

    if( KilledPlayer != None )
	{
		ScoreKill(Killer, KilledPlayer);
	}

	DiscardInventory(KilledPawn, Killer);
    NotifyKilled(Killer, KilledPlayer, KilledPawn, damageType);
}

function bool PreventDeath(Pawn KilledPawn, Controller Killer, class<DamageType> DamageType, vector HitLocation)
{
	if ( BaseMutator == None )
		return false;
	return BaseMutator.PreventDeath(KilledPawn, Killer, DamageType, HitLocation);
}

function BroadcastDeathMessage(Controller Killer, Controller Other, class<DamageType> damageType)
{
	if ( (Killer == Other) || (Killer == None) )
	{
		BroadcastLocalized(self, DeathMessageClass, 1, None, Other.PlayerReplicationInfo, damageType);
	}
	else
	{
		BroadcastLocalized(self, DeathMessageClass, 0, Killer.PlayerReplicationInfo, Other.PlayerReplicationInfo, damageType);
	}
}

function Kick( string S )
{
	if (AccessControl != None)
		AccessControl.Kick(S);
}

function KickBan( string S )
{
	if (AccessControl != None)
		AccessControl.KickBan(S);
}

//-------------------------------------------------------------------------------------
// Level gameplay modification.

//
// Return whether Viewer is allowed to spectate from the
// point of view of ViewTarget.
//
function bool CanSpectate( PlayerController Viewer, PlayerReplicationInfo ViewTarget )
{
	return true;
}

/* ReduceDamage:
	Use reduce damage for teamplay modifications, etc. */
function ReduceDamage(out int Damage, pawn injured, Controller instigatedBy, vector HitLocation, out vector Momentum, class<DamageType> DamageType, Actor DamageCauser)
{
	local int OriginalDamage;

	OriginalDamage = Damage;

	if ( injured.PhysicsVolume.bNeutralZone || injured.InGodMode() )
	{
		Damage = 0;
		return;
	}

	if (BaseMutator != None)
	{
		BaseMutator.NetDamage(OriginalDamage, Damage, Injured, InstigatedBy, HitLocation, Momentum, DamageType, DamageCauser);
	}
}

/* CheckRelevance()
returns true if actor is relevant to this game and should not be destroyed.  Called in Actor.PreBeginPlay(), intended to allow
mutators to remove or replace actors being spawned
*/
function bool CheckRelevance(Actor Other)
{
	if ( BaseMutator == None )
		return true;
	return BaseMutator.CheckRelevance(Other);
}

/**
  * Return whether an item should respawn.  Default implementation allows item respawning in multiplayer games.
  */
function bool ShouldRespawn( PickupFactory Other )
{
	return ( WorldInfo.NetMode != NM_Standalone );
}

/**
 *	Called when pawn has a chance to pick Item up (i.e. when
 *	the pawn touches a weapon pickup). Should return true if
 *	he wants to pick it up, false if he does not want it.
 * @param Other the Pawn that wants the item
 * @param ItemClass the Inventory class the Pawn can pick up
 * @param Pickup the Actor containing that item (this may be a PickupFactory or it may be a DroppedPickup)
 * @return whether or not the Pickup actor should give its item to Other
 */
function bool PickupQuery(Pawn Other, class<Inventory> ItemClass, Actor Pickup)
{
	local byte bAllowPickup;

	if (BaseMutator != None && BaseMutator.OverridePickupQuery(Other, ItemClass, Pickup, bAllowPickup))
	{
		return bool(bAllowPickup);
	}

	if ( Other.InvManager == None )
	{
		return false;
	}
	else
	{
		return Other.InvManager.HandlePickupQuery(ItemClass, Pickup);
	}
}

/**
  *	Discard a player's inventory after he dies.
  */
function DiscardInventory( Pawn Other, optional controller Killer )
{
	if ( Other.InvManager != None )
		Other.InvManager.DiscardInventory();
}

/* Try to change a player's name.
*/
function ChangeName( Controller Other, coerce string S, bool bNameChange )
{
	if( S == "" )
	{
		return;
	}

	Other.PlayerReplicationInfo.SetPlayerName(S);
}

/* Return whether a team change is allowed.
*/
function bool ChangeTeam(Controller Other, int N, bool bNewTeam)
{
	return true;
}

/* Return a picked team number if none was specified
*/
function byte PickTeam(byte Current, Controller C)
{
	return Current;
}

/* Send a player to a URL.
*/
function SendPlayer( PlayerController aPlayer, string URL )
{
	aPlayer.ClientTravel( URL, TRAVEL_Relative );
}

/** @return the map we should travel to for the next game */
function string GetNextMap();

/**
 * Returns true if we want to travel_absolute
 */
function bool GetTravelType()
{
	return false;
}

/* Restart the game.
*/
function RestartGame()
{
	local string NextMap;
	local string TransitionMapCmdLine;
	local string URLString;
	local int URLMapLen;
	local int MapNameLen;

	// If we are using arbitration and haven't done the end game handshaking,
	// do that process first and then come back here afterward
	if (bUsingArbitration)
	{
		if (bIsEndGameHandshakeComplete)
		{
			// All arbitrated matches must exit after one match
			NotifyArbitratedMatchEnd();
		}
		return;
	}

	if (BaseMutator != None && BaseMutator.HandleRestartGame())
	{
		return;
	}

	if (bGameRestarted)
	{
		return;
	}
	bGameRestarted = true;

	// these server travels should all be relative to the current URL
	if ( bChangeLevels && !bAlreadyChanged )
	{
		// get the next map and start the transition
		bAlreadyChanged = true;

		if ( (MyAutoTestManager != None) && MyAutoTestManager.bUsingAutomatedTestingMapList)
		{
			NextMap = MyAutoTestManager.GetNextAutomatedTestingMap();
		}
		else
		{
			NextMap = GetNextMap();
		}

		if (NextMap != "")
		{
			if ( (MyAutoTestManager == None) || !MyAutoTestManager.bUsingAutomatedTestingMapList )
			{
				WorldInfo.ServerTravel(NextMap,GetTravelType());
			}
			else
			{
				if ( !MyAutoTestManager.bAutomatedTestingWithOpen )
				{
					URLString = WorldInfo.GetLocalURL();
					URLMapLen = Len(URLString);

					MapNameLen = InStr(URLString, "?");
					if (MapNameLen != -1)
					{
						URLString = Right(URLString, URLMapLen - MapNameLen);
					}

					// The ENTIRE url needs to be recreated here...
					TransitionMapCmdLine = NextMap$URLString$"?AutomatedTestingMapIndex="$MyAutoTestManager.AutomatedTestingMapIndex;
					`log(">>> Issuing server travel on " $ TransitionMapCmdLine);
					WorldInfo.ServerTravel(TransitionMapCmdLine,GetTravelType());
				}
				else
				{
					TransitionMapCmdLine = "?AutomatedTestingMapIndex="$MyAutoTestManager.AutomatedTestingMapIndex$"?NumberOfMatchesPlayed="$MyAutoTestManager.NumberOfMatchesPlayed$"?NumMapListCyclesDone="$MyAutoTestManager.NumMapListCyclesDone;
					`log(">>> Issuing open command on " $ NextMap $ TransitionMapCmdLine);
					ConsoleCommand( "open " $ NextMap $ TransitionMapCmdLine);
				}
			}
			return;
		}
	}

	WorldInfo.ServerTravel("?Restart",GetTravelType());
}

//==========================================================================
// Message broadcasting functions (handled by the BroadCastHandler)

event Broadcast( Actor Sender, coerce string Msg, optional name Type )
{
	BroadcastHandler.Broadcast(Sender,Msg,Type);
}

function BroadcastTeam( Controller Sender, coerce string Msg, optional name Type )
{
	BroadcastHandler.BroadcastTeam(Sender,Msg,Type);
}

/*
 Broadcast a localized message to all players.
 Most message deal with 0 to 2 related PRIs.
 The LocalMessage class defines how the PRI's and optional actor are used.
*/
event BroadcastLocalized( actor Sender, class<LocalMessage> Message, optional int Switch, optional PlayerReplicationInfo RelatedPRI_1, optional PlayerReplicationInfo RelatedPRI_2, optional Object OptionalObject )
{
	BroadcastHandler.AllowBroadcastLocalized(Sender,Message,Switch,RelatedPRI_1,RelatedPRI_2,OptionalObject);
}

/*
 Broadcast a localized message to all players on a team.
 Most message deal with 0 to 2 related PRIs.
 The LocalMessage class defines how the PRI's and optional actor are used.
*/
event BroadcastLocalizedTeam( int TeamIndex, actor Sender, class<LocalMessage> Message, optional int Switch, optional PlayerReplicationInfo RelatedPRI_1, optional PlayerReplicationInfo RelatedPRI_2, optional Object OptionalObject )
{
	BroadcastHandler.AllowBroadcastLocalizedTeam(TeamIndex, Sender,Message,Switch,RelatedPRI_1,RelatedPRI_2,OptionalObject);
}

//==========================================================================
function bool CheckModifiedEndGame(PlayerReplicationInfo Winner, string Reason)
{
	return (BaseMutator != None && !BaseMutator.CheckEndGame(Winner, Reason));
}

function bool CheckEndGame(PlayerReplicationInfo Winner, string Reason)
{
	local Controller P;

	if ( CheckModifiedEndGame(Winner, Reason) )
		return false;

	// all player cameras focus on winner or final scene (picked by mutator)
	foreach WorldInfo.AllControllers(class'Controller', P)
	{
		P.GameHasEnded();
	}
	return true;
}

/**
 * Tells all clients to write stats and then handles writing local stats
 */
function WriteOnlineStats()
{
	local PlayerController PC;
	local OnlineGameSettings CurrentSettings;

	if (GameInterface != None)
	{
		// Make sure that we are recording stats
		CurrentSettings = GameInterface.GetGameSettings(PlayerReplicationInfoClass.default.SessionName);
		if (CurrentSettings != None && CurrentSettings.bUsesStats)
		{
			// Iterate through the controllers telling them to write stats
			foreach WorldInfo.AllControllers(class'PlayerController',PC)
			{
				if (PC.IsLocalPlayerController() == false)
				{
					PC.ClientWriteLeaderboardStats(OnlineStatsWriteClass);
				}
			}
			// Iterate through local controllers telling them to write stats
			foreach WorldInfo.AllControllers(class'PlayerController',PC)
			{
				if (PC.IsLocalPlayerController())
				{
					PC.ClientWriteLeaderboardStats(OnlineStatsWriteClass);
				}
			}
		}
	}
}

/**
 * If the match is arbitrated, tells all clients to write out their copies
 * of the player scores. If not arbitrated, it only has the first local player
 * write the scores.
 */
function WriteOnlinePlayerScores()
{
	local PlayerController PC;

	if (bUsingArbitration)
	{
		// Iterate through the controllers telling them to write stats
		foreach WorldInfo.AllControllers(class'PlayerController',PC)
		{
			PC.ClientWriteOnlinePlayerScores(ArbitratedLeaderboardId);
		}
	}
	else
	{
		// Find the first local player and have them write the data
		foreach WorldInfo.AllControllers(class'PlayerController',PC)
		{
			if (PC.IsLocalPlayerController())
			{
				PC.ClientWriteOnlinePlayerScores(LeaderboardId);
				break;
			}
		}
	}
}

/* End of game.
*/
function EndGame( PlayerReplicationInfo Winner, string Reason )
{
	// don't end game if not really ready
	if ( !CheckEndGame(Winner, Reason) )
	{
		bOverTime = true;
		return;
	}

	// Allow replication to happen before reporting scores, stats, etc.
	SetTimer( 1.5,false,nameof(PerformEndGameHandling) );

	bGameEnded = true;
	EndLogging(Reason);
}

/** Does end of game handling for the online layer */
function PerformEndGameHandling()
{
	if (GameInterface != None)
	{
		// Write out any online stats
		WriteOnlineStats();
		// Write the player data used in determining skill ratings
		WriteOnlinePlayerScores();
		// Force the stats to flush and change the status of the match
		// to ended making join in progress an option again
		EndOnlineGame();
		// Notify clients that the session has ended for arbitrated
		if (bUsingArbitration)
		{
			PendingArbitrationPCs.Length = 0;
			ArbitrationPCs.Length = 0;
			// Do end of session handling
			NotifyArbitratedMatchEnd();
		}
	}
}

/**
 * Tells the online system to end the game and tells all clients to do the same
 */
function EndOnlineGame()
{
	local PlayerController PC;

	GameReplicationInfo.EndGame();
	if (GameInterface != None)
	{
		// Have clients end their games
		foreach WorldInfo.AllControllers(class'PlayerController',PC)
		{
			// Skip notifying local PCs as they are handled automatically
			if (!PC.IsLocalPlayerController())
			{
				PC.ClientEndOnlineGame();
			}
		}
		// Server is handled here
		GameInterface.EndOnlineGame(PlayerReplicationInfoClass.default.SessionName);
	}
}

/** Entry point of the stats code for custom game logging at a regular interval */
function GameEventsPoll();
function EndLogging(string Reason);	// Stub function

/** returns whether the given Controller StartSpot property should be used as the spawn location for its Pawn */
function bool ShouldSpawnAtStartSpot(Controller Player)
{
	return ( WorldInfo.NetMode == NM_Standalone && Player != None && Player.StartSpot != None &&
	     (bWaitingToStartMatch || (Player.PlayerReplicationInfo != None && Player.PlayerReplicationInfo.bWaitingPlayer)) );
}

/** FindPlayerStart()
* Return the 'best' player start for this player to start from.  PlayerStarts are rated by RatePlayerStart().
* @param Player is the controller for whom we are choosing a playerstart
* @param InTeam specifies the Player's team (if the player hasn't joined a team yet)
* @param IncomingName specifies the tag of a teleporter to use as the Playerstart
* @returns NavigationPoint chosen as player start (usually a PlayerStart)
 */
function NavigationPoint FindPlayerStart( Controller Player, optional byte InTeam, optional string IncomingName )
{
	local NavigationPoint N, BestStart;
	local Teleporter Tel;

	// allow GameRulesModifiers to override playerstart selection
	if (BaseMutator != None)
	{
		N = BaseMutator.FindPlayerStart(Player, InTeam, IncomingName);
		if (N != None)
		{
			return N;
		}
	}

	// if incoming start is specified, then just use it
	if( incomingName!="" )
	{
		ForEach WorldInfo.AllNavigationPoints( class 'Teleporter', Tel )
			if( string(Tel.Tag)~=incomingName )
				return Tel;
	}

	// always pick StartSpot at start of match
	if ( ShouldSpawnAtStartSpot(Player) &&
		(PlayerStart(Player.StartSpot) == None || RatePlayerStart(PlayerStart(Player.StartSpot), InTeam, Player) >= 0.0) )
	{
		return Player.StartSpot;
	}

	BestStart = ChoosePlayerStart(Player, InTeam);

	if ( (BestStart == None) && (Player == None) )
	{
		// no playerstart found, so pick any NavigationPoint to keep player from failing to enter game
		`log("Warning - PATHS NOT DEFINED or NO PLAYERSTART with positive rating");
		ForEach AllActors( class 'NavigationPoint', N )
		{
			BestStart = N;
			break;
		}
	}
	return BestStart;
}

/** ChoosePlayerStart()
* Return the 'best' player start for this player to start from.  PlayerStarts are rated by RatePlayerStart().
* @param Player is the controller for whom we are choosing a playerstart
* @param InTeam specifies the Player's team (if the player hasn't joined a team yet)
* @returns NavigationPoint chosen as player start (usually a PlayerStart)
 */
function PlayerStart ChoosePlayerStart( Controller Player, optional byte InTeam )
{
	local PlayerStart P, BestStart;
	local float BestRating, NewRating;
	local byte Team;

	// use InTeam if player doesn't have a team yet
	Team = ( (Player != None) && (Player.PlayerReplicationInfo != None) && (Player.PlayerReplicationInfo.Team != None) )
			? byte(Player.PlayerReplicationInfo.Team.TeamIndex)
			: InTeam;

	// Find best playerstart
	foreach WorldInfo.AllNavigationPoints(class'PlayerStart', P)
	{
		NewRating = RatePlayerStart(P,Team,Player);
		if ( NewRating > BestRating )
		{
			BestRating = NewRating;
			BestStart = P;
		}
	}
	return BestStart;
}

/** RatePlayerStart()
* Return a score representing how desireable a playerstart is.
* @param P is the playerstart being rated
* @param Team is the team of the player choosing the playerstart
* @param Player is the controller choosing the playerstart
* @returns playerstart score
*/
function float RatePlayerStart(PlayerStart P, byte Team, Controller Player)
{
	local float Rating;
	if ( !P.bEnabled )
	{
		return 5.f;
	}
	else
	{
		Rating = 10.f;
		if (P.bPrimaryStart)
		{
			Rating += 10.f;
		}
		if (P.TeamIndex == Team)
		{
			Rating += 15.f;
		}
		return Rating;
	}
}

function AddObjectiveScore(PlayerReplicationInfo Scorer, Int Score)
{
	if ( Scorer != None )
	{
		Scorer.Score += Score;
	}
	if (BaseMutator != None)
	{
		BaseMutator.ScoreObjective(Scorer, Score);
	}
}

function ScoreObjective(PlayerReplicationInfo Scorer, Int Score)
{
	AddObjectiveScore(Scorer, Score);
	CheckScore(Scorer);
}

/* CheckScore()
see if this score means the game ends
*/
function bool CheckScore(PlayerReplicationInfo Scorer)
{
	return true;
}

function ScoreKill(Controller Killer, Controller Other)
{
	if( (killer == Other) || (killer == None) )
	{
		if ( (Other!=None) && (Other.PlayerReplicationInfo != None) )
		{
			Other.PlayerReplicationInfo.Score -= 1;
			Other.PlayerReplicationInfo.bForceNetUpdate = TRUE;
		}
	}
	else if ( killer.PlayerReplicationInfo != None )
	{
		Killer.PlayerReplicationInfo.Score += 1;
		Killer.PlayerReplicationInfo.bForceNetUpdate = TRUE;
		Killer.PlayerReplicationInfo.Kills++;
	}

	ModifyScoreKill(Killer, Other);

	if (Killer != None || MaxLives > 0)
	{
		CheckScore(Killer.PlayerReplicationInfo);
	}
}

/**
  * For subclasses which don't call GameInfo.ScoreKill()
  */
function ModifyScoreKill(Controller Killer, Controller Other)
{
	if (BaseMutator != None)
	{
		BaseMutator.ScoreKill(Killer, Other);
	}
}

function DriverEnteredVehicle(Vehicle V, Pawn P)
{
	if ( BaseMutator != None )
		BaseMutator.DriverEnteredVehicle(V, P);
}

function bool CanLeaveVehicle(Vehicle V, Pawn P)
{
	if ( BaseMutator == None )
		return true;
	return BaseMutator.CanLeaveVehicle(V, P);
}

function DriverLeftVehicle(Vehicle V, Pawn P)
{
	if ( BaseMutator != None )
		BaseMutator.DriverLeftVehicle(V, P);
}

function bool PlayerCanRestartGame( PlayerController aPlayer )
{
	return true;
}

// Player Can be restarted ?
function bool PlayerCanRestart( PlayerController aPlayer )
{
	return true;
}

// Returns whether a mutator should be allowed with this gametype
static function bool AllowMutator( string MutatorClassName )
{
	return !class'WorldInfo'.static.IsDemoBuild();
}


function bool AllowCheats(PlayerController P)
{
	return ( WorldInfo.NetMode == NM_Standalone );
}

/**
 * @return	TRUE if the player is allowed to pause the game.
 */
function bool AllowPausing( optional PlayerController PC )
{
	return	bPauseable
		||	WorldInfo.NetMode == NM_Standalone
		||	(bAdminCanPause && AccessControl.IsAdmin(PC));
}

/**
 * Called from C++'s CommitMapChange before unloading previous level
 * @param PreviousMapName Name of the previous persistent level
 * @param NextMapName Name of the persistent level being streamed to
 */
event PreCommitMapChange(string PreviousMapName, string NextMapName);

/**
 * Called from C++'s CommitMapChange after unloading previous level and loading new level+sublevels
 */
event PostCommitMapChange();

/** AddInactivePRI()
* Add PRI to the inactive list, remove from the active list
*/
function AddInactivePRI(PlayerReplicationInfo PRI, PlayerController PC)
{
	local int i;
	local PlayerReplicationInfo NewPRI, CurrentPRI;
	local bool bIsConsole;

	// don't store if it's an old PRI from the previous level or if it's a spectator
	if (!PRI.bFromPreviousLevel && !PRI.bOnlySpectator)
	{
		NewPRI = PRI.Duplicate();
		WorldInfo.GRI.RemovePRI(NewPRI);

		// make PRI inactive
		NewPRI.RemoteRole = ROLE_None;

		// delete after 5 minutes
		NewPRI.LifeSpan = 300;

		// On console, we have to check the unique net id as network address isn't valid
		bIsConsole = WorldInfo.IsConsoleBuild();

		// make sure no duplicates
		for (i=0; i<InactivePRIArray.Length; i++)
		{
			CurrentPRI = InactivePRIArray[i];
			if ( (CurrentPRI == None) || CurrentPRI.bDeleteMe ||
				(!bIsConsole && (CurrentPRI.SavedNetworkAddress == NewPRI.SavedNetworkAddress)) ||
				(bIsConsole && CurrentPRI.UniqueId == NewPRI.UniqueId) )
			{
				InactivePRIArray.Remove(i,1);
				i--;
			}
		}
		InactivePRIArray[InactivePRIArray.Length] = NewPRI;

		// cap at 16 saved PRIs
		if ( InactivePRIArray.Length > 16 )
		{
			InactivePRIArray.Remove(0, InactivePRIArray.Length - 16);
		}
	}

	PRI.Destroy();
	// Readjust the skill rating now that this player has left
	RecalculateSkillRating();
}

/** FindInactivePRI()
* returns the PRI associated with this re-entering player
*/
function bool FindInactivePRI(PlayerController PC)
{
	local string NewNetworkAddress, NewName;
	local int i;
	local PlayerReplicationInfo OldPRI, CurrentPRI;
	local bool bIsConsole;

	// don't bother for spectators
	if (PC.PlayerReplicationInfo.bOnlySpectator)
	{
		return false;
	}

	// On console, we have to check the unique net id as network address isn't valid
	bIsConsole = WorldInfo.IsConsoleBuild();

	NewNetworkAddress = PC.PlayerReplicationInfo.SavedNetworkAddress;
	NewName = PC.PlayerReplicationInfo.PlayerName;
	for (i=0; i<InactivePRIArray.Length; i++)
	{
		CurrentPRI = InactivePRIArray[i];
		if ( (CurrentPRI == None) || CurrentPRI.bDeleteMe )
		{
			InactivePRIArray.Remove(i,1);
			i--;
		}
		else if ( (bIsConsole && CurrentPRI.UniqueId == PC.PlayerReplicationInfo.UniqueId) ||
			(!bIsConsole && (CurrentPRI.SavedNetworkAddress ~= NewNetworkAddress) && (CurrentPRI.PlayerName ~= NewName)) )
		{
			// found it!
			OldPRI = PC.PlayerReplicationInfo;
			PC.PlayerReplicationInfo = CurrentPRI;
			PC.PlayerReplicationInfo.SetOwner(PC);
			PC.PlayerReplicationInfo.RemoteRole = ROLE_SimulatedProxy;
			PC.PlayerReplicationInfo.Lifespan = 0;
			OverridePRI(PC, OldPRI);
			WorldInfo.GRI.AddPRI(PC.PlayerReplicationInfo);
			InactivePRIArray.Remove(i,1);
			OldPRI.bIsInactive = TRUE;
			OldPRI.Destroy();
			return true;
		}
	}
	return false;
}

/** OverridePRI()
* override as needed properties of NewPRI with properties from OldPRI which were assigned during the login process
*/
function OverridePRI(PlayerController PC, PlayerReplicationInfo OldPRI)
{
	PC.PlayerReplicationInfo.OverrideWith(OldPRI);
}

/** called on server during seamless level transitions to get the list of Actors that should be moved into the new level
 * PlayerControllers, Role < ROLE_Authority Actors, and any non-Actors that are inside an Actor that is in the list
 * (i.e. Object.Outer == Actor in the list)
 * are all autmoatically moved regardless of whether they're included here
 * only dynamic (!bStatic and !bNoDelete) actors in the PersistentLevel may be moved (this includes all actors spawned during gameplay)
 * this is called for both parts of the transition because actors might change while in the middle (e.g. players might join or leave the game)
 * @see also PlayerController::GetSeamlessTravelActorList() (the function that's called on clients)
 * @param bToEntry true if we are going from old level -> entry, false if we are going from entry -> new level
 * @param ActorList (out) list of actors to maintain
 */
event GetSeamlessTravelActorList(bool bToEntry, out array<Actor> ActorList)
{
	local int i;

	// always keep PlayerReplicationInfos and TeamInfos, so that after we restart we can keep players on the same team, etc
	for (i = 0; i < WorldInfo.GRI.PRIArray.Length; i++)
	{
		WorldInfo.GRI.PRIArray[i].bFromPreviousLevel = true;
		WorldInfo.GRI.PRIArray[i].bForceNetUpdate = true;
		ActorList[ActorList.length] = WorldInfo.GRI.PRIArray[i];
	}

	if (bToEntry)
	{
		// keep general game state until we transition to the final destination
		ActorList[ActorList.length] = WorldInfo.GRI;
		if (BroadcastHandler != None)
		{
			ActorList[ActorList.length] = BroadcastHandler;
		}
	}

	if (BaseMutator != None)
	{
		BaseMutator.GetSeamlessTravelActorList(bToEntry, ActorList);
	}

	if (MyAutoTestManager != None)
	{
		ActorList[ActorList.length] = MyAutoTestManager;
	}

	// Keep the AccessControl persistent, as it needs to >always< be ready for handling auth callbacks
	if (AccessControl != none)
	{
		ActorList[ActorList.Length] = AccessControl;
	}
}

/** used to swap a viewport/connection's PlayerControllers when seamless travelling and the new gametype's
 * controller class is different than the previous
 * includes network handling
 * @param OldPC - the old PC that should be discarded
 * @param NewPC - the new PC that should be used for the player
 */
native final function SwapPlayerControllers(PlayerController OldPC, PlayerController NewPC);

/** called after a seamless level transition has been completed on the *new* GameInfo
 * used to reinitialize players already in the game as they won't have *Login() called on them
 */
event PostSeamlessTravel()
{
	local Controller C;

	// handle players that are already loaded
	foreach WorldInfo.AllControllers(class'Controller', C)
	{
		if (C.bIsPlayer)
		{
			if (PlayerController(C) == None)
			{
				HandleSeamlessTravelPlayer(C);
			}
			else
			{
				if (!C.PlayerReplicationInfo.bOnlySpectator)
				{
					NumTravellingPlayers++;
				}
				if (PlayerController(C).HasClientLoadedCurrentWorld())
				{
					HandleSeamlessTravelPlayer(C);
				}
			}
		}
	}

	if (bWaitingToStartMatch && !bDelayedStart && NumPlayers + NumBots > 0)
	{
		StartMatch();
	}
	if (WorldInfo.NetMode == NM_DedicatedServer)
	{
		// Update any online advertised settings
		UpdateGameSettings();
	}
}

/**
 * Used to update any changes in game settings that need to be published to
 * players that are searching for games
 */
function UpdateGameSettings();

/** handles reinitializing players that remained through a seamless level transition
 * called from C++ for players that finished loading after the server
 * @param C the Controller to handle
 */
event HandleSeamlessTravelPlayer(out Controller C)
{
	local rotator StartRotation;
	local NavigationPoint StartSpot;
	local PlayerController PC, NewPC;
	local PlayerReplicationInfo OldPRI;

	`log(">> GameInfo::HandleSeamlessTravelPlayer:" @ C,,'SeamlessTravel');

	PC = PlayerController(C);
	if (PC != None && PC.Class != PlayerControllerClass)
	{
		if (PC.Player != None)
		{
			// we need to spawn a new PlayerController to replace the old one
			NewPC = SpawnPlayerController(PC.Location, PC.Rotation);
			if (NewPC == None)
			{
				`Warn("Failed to spawn new PlayerController for" @ PC.GetHumanReadableName() @ "(old class" @ PC.Class $ ")");
				PC.Destroy();
				return;
			}
			else
			{
				PC.CleanUpAudioComponents();
				PC.SeamlessTravelTo(NewPC);
				NewPC.SeamlessTravelFrom(PC);
				SwapPlayerControllers(PC, NewPC);
				PC = NewPC;
				C = NewPC;
			}
		}
		else
		{
			PC.Destroy();
		}
	}
	else
	{
		// clear out data that was only for the previous game
		C.PlayerReplicationInfo.Reset();
		// create a new PRI and copy over info; this is necessary because the old gametype may have used a different PRI class
		OldPRI = C.PlayerReplicationInfo;
		C.InitPlayerReplicationInfo();
		OldPRI.SeamlessTravelTo(C.PlayerReplicationInfo);
		// we don't need the old PRI anymore
		//@fixme: need a way to replace PRIs that doesn't cause incorrect "player left the game"/"player entered the game" messages
		OldPRI.Destroy();
	}

	// get rid of team if this is not a team game
	if (!bTeamGame && C.PlayerReplicationInfo.Team != None)
	{
		C.PlayerReplicationInfo.Team.Destroy();
		C.PlayerReplicationInfo.Team = None;
	}

	// Find a start spot.
	StartSpot = FindPlayerStart(C, C.GetTeamNum());

	if (StartSpot == None)
	{
		`warn(GameMessageClass.Default.FailedPlaceMessage);
	}
	else
	{
		StartRotation.Yaw = StartSpot.Rotation.Yaw;
		C.SetLocation(StartSpot.Location);
		C.SetRotation(StartRotation);
	}

	C.StartSpot = StartSpot;

	if (PC != None)
	{
		PC.CleanUpAudioComponents();

		// tell the player controller to register its data stores again
		PC.ClientInitializeDataStores();

		SetSeamlessTravelViewTarget(PC);
		if (PC.PlayerReplicationInfo.bOnlySpectator)
		{
			PC.GotoState('Spectating');
			PC.PlayerReplicationInfo.bIsSpectator = true;
			PC.PlayerReplicationInfo.bOutOfLives = true;
			NumSpectators++;
		}
		else
		{
			NumPlayers++;
			NumTravellingPlayers--;
			PC.GotoState('PlayerWaiting');
		}


	}
	else
	{
		NumBots++;
		C.GotoState('RoundEnded');
	}

	GenericPlayerInitialization(C);

	`log("<< GameInfo::HandleSeamlessTravelPlayer:" @ C,,'SeamlessTravel');
}

function SetSeamlessTravelViewTarget(PlayerController PC)
{
	PC.SetViewTarget(PC);
}

/**
 * Updates the online subsystem's information for player counts so that
 * LAN matches can show the correct player counts
 */
function UpdateGameSettingsCounts()
{
	local OnlineGameSettings GameSettings;

	if (GameInterface != None)
	{
		GameSettings = GameInterface.GetGameSettings(PlayerReplicationInfoClass.default.SessionName);
		if (GameSettings != None && GameSettings.bIsLanMatch)
		{
			// Update the number of open slots available
			GameSettings.NumOpenPublicConnections = GameSettings.NumPublicConnections - GetNumPlayers();
			if (GameSettings.NumOpenPublicConnections < 0)
			{
				GameSettings.NumOpenPublicConnections = 0;
			}
		}
	}
}

/**
 * This is a base (empty) implementation of the completion notification
 *
 * @param PC the player controller to mark as done
 * @param bWasSuccessful whether the PC was able to register for arbitration or not
 */
function ProcessClientRegistrationCompletion(PlayerController PC,bool bWasSuccessful);

/**
 * Empty implementation of the code that kicks off async registration
 */
function StartArbitrationRegistration();

/**
 * Empty implementation of the code that starts an arbitrated match
 */
function StartArbitratedMatch();

/**
 * Empty implementation of the code that registers the server for arbitration
 */
function RegisterServerForArbitration();

/**
 * Empty implementation of the code that handles the callback for completion
 *
 * @param SessionName the name of the session this is for
 * @param bWasSuccessful whether the call worked or not
 */
function ArbitrationRegistrationComplete(name SessionName,bool bWasSuccessful);

function bool MatchIsInProgress()
{
	return true;
}

/**
 * This state is used to change the flow of start/end match to handle arbitration
 *
 * Basic flow of events:
 *		Server prepares to start the match and tells all clients to register arbitration
 *		Clients register with arbitration and tell the server when they are done
 *		Server checks for all clients to be registered and kicks any clients if
 *			they don't register in time.
 *		Server registers with arbitration and the match begins
 *
 *		Match ends and the server tells connected clients to write arbitrated stats
 *		Clients write stats and notifies server of completion
 *		Server writes stats and ends the match
 */
auto State PendingMatch
{
	function bool MatchIsInProgress()
	{
		return false;
	}

	/**
	 * Tells all of the currently connected clients to register with arbitration.
	 * The clients will call back to the server once they have done so, which
	 * will tell this state to see if it is time for the server to register with
	 * arbitration.
	 */
	function StartMatch()
	{
		if (bUsingArbitration)
		{
			StartArbitrationRegistration();
		}
		else
		{
			Global.StartMatch();
		}
	}

	/**
	 * Kicks off the async tasks of having the clients register with
	 * arbitration before the server does. Sets a timeout for when
	 * all slow to respond clients get kicked
	 */
	function StartArbitrationRegistration()
	{
		local PlayerController PC;
		local UniqueNetId HostId;
		local OnlineGameSettings GameSettings;

		if (!bHasArbitratedHandshakeBegun)
		{
			// Tell PreLogin() to reject new connections
			bHasArbitratedHandshakeBegun = true;

			// Get the host id from the game settings in case splitscreen works with arbitration
			GameSettings = GameInterface.GetGameSettings(PlayerReplicationInfoClass.default.SessionName);
			HostId = GameSettings.OwningPlayerId;

			PendingArbitrationPCs.Length = 0;
			// Iterate the controller list and tell them to register with arbitration
			foreach WorldInfo.AllControllers(class'PlayerController', PC)
			{
				// Skip notifying local PCs as they are handled automatically
				if (!PC.IsLocalPlayerController())
				{
					PC.ClientSetHostUniqueId(HostId);
					PC.ClientRegisterForArbitration();
					// Add to the pending list
					PendingArbitrationPCs[PendingArbitrationPCs.Length] = PC;
				}
				else
				{
					// Add them as having completed arbitration
					ArbitrationPCs[ArbitrationPCs.Length] = PC;
				}
			}
			// Start the kick timer
			SetTimer( ArbitrationHandshakeTimeout,false,nameof(ArbitrationTimeout) );
		}
	}

	/**
	 * Does the registration for the server. This must be done last as it
	 * includes all the players info from their registration
	 */
	function RegisterServerForArbitration()
	{
		if (GameInterface != None)
		{
			GameInterface.AddArbitrationRegistrationCompleteDelegate(ArbitrationRegistrationComplete);
			GameInterface.RegisterForArbitration(PlayerReplicationInfoClass.default.SessionName);
		}
		else
		{
			// Fake as working without subsystem
			ArbitrationRegistrationComplete(PlayerReplicationInfoClass.default.SessionName,true);
		}
	}

	/**
	 * Callback from the server that starts the match if the registration was
	 * successful. If not, it goes back to the menu
	 *
	 * @param SessionName the name of the session this is for
	 * @param bWasSuccessful whether the registration worked or not
	 */
	function ArbitrationRegistrationComplete(name SessionName,bool bWasSuccessful)
	{
		// Clear the delegate so we don't leak with GC
		GameInterface.ClearArbitrationRegistrationCompleteDelegate(ArbitrationRegistrationComplete);
		if (bWasSuccessful)
		{
			// Start the match
			StartArbitratedMatch();
		}
		else
		{
			ConsoleCommand("Disconnect");
		}
	}

	/**
	 * Handles kicking any clients that haven't completed handshaking
	 */
	function ArbitrationTimeout()
	{
		local int Index;

		// Kick any pending players
		for (Index = 0; Index < PendingArbitrationPCs.Length; Index++)
		{
			AccessControl.KickPlayer(PendingArbitrationPCs[Index],GameMessageClass.Default.MaxedOutMessage);
		}
		PendingArbitrationPCs.Length = 0;
		// Do the server registration now that any remaining clients are kicked
		RegisterServerForArbitration();
	}

	/**
	 * Called once arbitration has completed and kicks off the real start of the match
	 */
	function StartArbitratedMatch()
	{
		bNeedsEndGameHandshake = true;
		// Start the match
		Global.StartMatch();
	}

	/**
	 * Removes the player controller from the pending list. Kicks that PC if it
	 * failed to register for arbitration. Starts the match if all clients have
	 * completed their registration
	 *
	 * @param PC the player controller to mark as done
	 * @param bWasSuccessful whether the PC was able to register for arbitration or not
	 */
	function ProcessClientRegistrationCompletion(PlayerController PC,bool bWasSuccessful)
	{
		local int FoundIndex;

		// Search for the specified PC and remove if found
		FoundIndex = PendingArbitrationPCs.Find(PC);
		if (FoundIndex != INDEX_NONE)
		{
			PendingArbitrationPCs.Remove(FoundIndex,1);
			if (bWasSuccessful)
			{
				// Add to the completed list
				ArbitrationPCs[ArbitrationPCs.Length] = PC;
			}
			else
			{
				AccessControl.KickPlayer(PC,GameMessageClass.Default.MaxedOutMessage);
			}
		}
		// Start the match if all clients have responded
		if (PendingArbitrationPCs.Length == 0)
		{
			// Clear the kick timer
			SetTimer( 0,false,nameof(ArbitrationTimeout) );
			RegisterServerForArbitration();
		}
	}

	event EndState(name NextStateName)
	{
		// Clear the kick timer
		SetTimer( 0,false,nameof(ArbitrationTimeout) );

		if( GameInterface != None )
		{
			GameInterface.ClearArbitrationRegistrationCompleteDelegate(ArbitrationRegistrationComplete);
		}
	}
}

/**
 * Tells all clients to disconnect and then goes to the menu
 */
function NotifyArbitratedMatchEnd()
{
	local PlayerController PC;

	// Iterate through the controllers telling them to disconnect
	foreach WorldInfo.AllControllers(class'PlayerController',PC)
	{
		if (PC.IsLocalPlayerController() == false)
		{
			PC.ClientArbitratedMatchEnded();
		}
	}
	// Iterate through local controllers telling them to disconnect
	foreach WorldInfo.AllControllers(class'PlayerController',PC)
	{
		if (PC.IsLocalPlayerController())
		{
			PC.ClientArbitratedMatchEnded();
		}
	}
}

/**
 * Used to notify the game type that it is ok to update a player's gameplay
 * specific muting information now. The playercontroller needs to notify
 * the server when it is possible to do so or the unique net id will be
 * incorrect and the muting not work.
 *
 * @param PC the playercontroller that is ready for updates
 */
function UpdateGameplayMuteList(PlayerController PC)
{
	// Let the server start sending voice packets
	PC.bHasVoiceHandshakeCompleted = true;
	// And tell the client it can start sending voice packets
	PC.ClientVoiceHandshakeComplete();
}

/**
 * Used by the game type to update the advertised skill for this game
 */
function RecalculateSkillRating()
{
	local int Index;
	local array<UniqueNetId> Players;
	local UniqueNetId ZeroId;

	if (WorldInfo.NetMode != NM_Standalone &&
		OnlineSub != None &&
		OnlineSub.GameInterface != None)
	{
		// Iterate through the players adding their unique id for skill calculation
		for (Index = 0; Index < GameReplicationInfo.PRIArray.Length; Index++)
		{
			if (ZeroId != GameReplicationInfo.PRIArray[Index].UniqueId)
			{
				Players[Players.Length] = GameReplicationInfo.PRIArray[Index].UniqueId;
			}
		}
		if (Players.Length > 0)
		{
			// Update the skill rating with the list of players
			OnlineSub.GameInterface.RecalculateSkillRating(PlayerReplicationInfoClass.default.SessionName,Players);
		}
	};
}

/** Called when this PC is in cinematic mode, and its matinee is cancelled by the user. */
event MatineeCancelled();


/**
 * Checks for the login parameters being passed on the command line. If
 * present, it does an async login before starting the dedicated server
 * registration process
 *
 * @return true if the login is in progress, false otherwise
 */
function bool ProcessServerLogin()
{
	if (OnlineSub != None)
	{
		if (OnlineSub.PlayerInterface != None)
		{
			OnlineSub.PlayerInterface.AddLoginChangeDelegate(OnLoginChange);
			OnlineSub.PlayerInterface.AddLoginFailedDelegate(0,OnLoginFailed);
			// Check the command line for login information and login async
			if (OnlineSub.PlayerInterface.AutoLogin() == false)
			{
				ClearAutoLoginDelegates();
				return false;
			}
			return true;
		}
	}
	return false;
}

/**
 * Clears the login delegates once the login process has passed or failed
 */
function ClearAutoLoginDelegates()
{
	if (OnlineSub.PlayerInterface != None)
	{
		OnlineSub.PlayerInterface.ClearLoginChangeDelegate(OnLoginChange);
		OnlineSub.PlayerInterface.ClearLoginFailedDelegate(0,OnLoginFailed);
	}
}

/**
 * Called if the autologin fails
 *
 * @param LocalUserNum the controller number of the associated user
 * @param ErrorCode the async error code that occurred
 */
function OnLoginFailed(byte LocalUserNum,EOnlineServerConnectionStatus ErrorCode)
{
	ClearAutoLoginDelegates();
}

/**
 * Used to tell the game when the autologin has completed
 *
 * @param LocalUserNum ignored
 */
function OnLoginChange(byte LocalUserNum)
{
	ClearAutoLoginDelegates();
	// The login has completed so start the dedicated server
	RegisterServer();
}

/**
 * Registers the dedicated server with the online service
 */
function RegisterServer()
{
	local OnlineGameSettings GameSettings;

	if (OnlineGameSettingsClass != None && OnlineSub != None && OnlineSub.GameInterface != None)
	{
		// Create the default settings to get the standard settings to advertise
		GameSettings = new OnlineGameSettingsClass;
		// Serialize any custom settings from the URL
		GameSettings.UpdateFromURL(ServerOptions, self);

		// If 'bIsLanMatch' is set, disable all authentication
		if (AccessControl != None &&
			(WorldInfo.NetMode == NM_DedicatedServer || WorldInfo.NetMode == NM_ListenServer) && GameSettings.bIsLanMatch)
		{
			`log("Disabling all authentication, due to bIsLanMatch being set to true");
			AccessControl.ClearAuthDelegates(false);
		}

		// Register the delegate so we can see when it's done
		OnlineSub.GameInterface.AddCreateOnlineGameCompleteDelegate(OnServerCreateComplete);
		// Now kick off the async publish
		if ( !OnlineSub.GameInterface.CreateOnlineGame(0,PlayerReplicationInfoClass.default.SessionName,GameSettings) )
		{
			OnlineSub.GameInterface.ClearCreateOnlineGameCompleteDelegate(OnServerCreateComplete);
		}
	}
	else
	{
		`Warn("No game settings to register with the online service. Game won't be advertised");
	}
}

/**
 * Notifies us of the game being registered successfully or not
 *
 * @param SessionName the name of the session that was created
 * @param bWasSuccessful flag telling us whether it worked or not
 */
function OnServerCreateComplete(name SessionName,bool bWasSuccessful)
{
	local OnlineGameSettings GameSettings;

	GameInterface.ClearCreateOnlineGameCompleteDelegate(OnServerCreateComplete);
	if (bWasSuccessful == false)
	{
		GameSettings = GameInterface.GetGameSettings(PlayerReplicationInfoClass.default.SessionName);
		if (GameSettings != None && GameSettings.bIsLanMatch == false)
		{
			`Warn("Failed to register game with online service. Registering as a LAN match");
			// Force to be a LAN match
			GameSettings.bIsLanMatch = true;
			// Register the delegate so we can see when it's done
			GameInterface.AddCreateOnlineGameCompleteDelegate(OnServerCreateComplete);
			// Now kick off the async publish
			if (!GameInterface.CreateOnlineGame(0,SessionName,GameSettings))
			{
				GameInterface.ClearCreateOnlineGameCompleteDelegate(OnServerCreateComplete);
			}
		}
		else
		{
			`Warn("Failed to register game with online service. Game won't be advertised");
		}
	}
	else
	{
		// If a game server is started without Steam running, auth code needs late initialization
		//	(as the auth interface is only setup within CreateOnlineGame when Steam was not started)
		if (OnlineSub.Class.Name == 'OnlineSubsystemSteamworks' && AccessControl != none && AccessControl.CachedAuthInt == none)
		{
			AccessControl.InitAuthHooks();

			// This delegate was not set until after the auth interface became ready, so kick it off here
			AccessControl.OnAuthReady();
		}

		UpdateGameSettings();
	}
}

/**
 * Iterates the player controllers and tells them to return to their party
 */
function TellClientsToReturnToPartyHost()
{
	local PlayerController PC;
	local OnlineGameSettings GameSettings;
	local UniqueNetId RequestingPlayerId;

	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		// And grab one for the game interface since it will be used often
		GameInterface = OnlineSub.GameInterface;
		if (GameInterface != None)
		{
			// Use the game session owner as the host requesting travel
			GameSettings = GameInterface.GetGameSettings(PlayerReplicationInfoClass.default.SessionName);
			if (GameSettings != None)
			{
				RequestingPlayerId = GameSettings.OwningPlayerId;
			}
			else
			{
				// If no valid game session then use local player's net id as the host requesting the travel
				foreach LocalPlayerControllers(class'PlayerController',PC)
				{
					if (PC.IsPrimaryPlayer() &&
						PC.PlayerReplicationInfo != None)
					{
						RequestingPlayerId = PC.PlayerReplicationInfo.UniqueId;
						break;
					}
				}
			}
			// Tell all clients to return using the net id of the host
			foreach WorldInfo.AllControllers(class'PlayerController',PC)
			{
				if (!PC.IsLocalPlayerController() && 
					PC.IsPrimaryPlayer())
				{
					PC.ClientReturnToParty(RequestingPlayerId);
				}
			}
			// Host travels last as this can trigger a disconnect on clients
			foreach LocalPlayerControllers(class'PlayerController',PC)
			{
				if (PC.IsPrimaryPlayer())
				{
					PC.ClientReturnToParty(RequestingPlayerId);
					break;
				}
			}
		}
	}
}

/**
 * Send notification to clients that a party host is about to leave the match
 *
 * @param PartyHostPlayerId net id of the party host that is leaving
 */
function TellClientsPartyHostIsLeaving(UniqueNetId PartyHostPlayerId)
{
	local PlayerController PC;

	// Tell all clients to return using the net id of the host
	foreach WorldInfo.AllControllers(class'PlayerController',PC)
	{
		if ( PC.IsPrimaryPlayer() )
		{
			PC.ClientNotifyPartyHostLeaving(PartyHostPlayerId);
		}
	}
}


/**
 * Iterates the player controllers and tells remote players to travel to the specified session
 *
 * @param SessionName the name of the session to register
 * @param SearchClass the search that should be populated with the session
 * @param PlatformSpecificInfo the binary data to place in the platform specific areas
 */
function TellClientsToTravelToSession(name SessionName,class<OnlineGameSearch> SearchClass,byte PlatformSpecificInfo[80])
{
	local PlayerController PC;

	foreach WorldInfo.AllControllers(class'PlayerController',PC)
	{
		if ( !PC.IsLocalPlayerController() && PC.IsPrimaryPlayer() )
		{
			PC.ClientTravelToSession(SessionName,SearchClass,PlatformSpecificInfo);
		}
	}
}

//=================================================================
/**
  * AutoTestManager INTERFACE
  */

/** function to start the world traveling **/
exec function DoTravelTheWorld()
{
	if ( MyAutoTestManager != None )
	{
		GotoState('TravelTheWorld');
		MyAutoTestManager.DoTravelTheWorld();
	}
}

/** Alters the synthetic bandwidth limit for a running game **/
exec native function SetBandwidthLimit( float AsyncIOBandwidthLimit );

/** This our state which allows us to have delayed actions while traveling the world (e.g. waiting for levels to stream in) **/
state TravelTheWorld
{
}

/**
  *  @returns true if Automated Performance testing is enabled
  */
function bool IsAutomatedPerfTesting()
{
	return (MyAutoTestManager != None) && MyAutoTestManager.bAutomatedPerfTesting;
}

/**
  *  @returns true if checking for fragmentation is enabled
  */
function bool IsCheckingForFragmentation()
{
	return (MyAutoTestManager != None) && MyAutoTestManager.bCheckingForFragmentation;
}

/**
  *  @returns true if checking for memory leaks is enabled
  */
function bool IsCheckingForMemLeaks()
{
	return (MyAutoTestManager != None) && MyAutoTestManager.bCheckingForMemLeaks;
}

/**
  *  @returns true if doing a sentinel run
  */
function bool IsDoingASentinelRun()
{
	return (MyAutoTestManager != None) && MyAutoTestManager.bDoingASentinelRun;
}

/**
  *  @returns true if should auto-continue to next round
  */
function bool ShouldAutoContinueToNextRound()
{
	return (MyAutoTestManager != None) && MyAutoTestManager.bAutoContinueToNextRound;
}

/**
  *  Asks AutoTestManager to start a sentinel run if needed
  *  Must be called by gameinfo subclass - not called in base implementation of GameInfo.StartMatch()
  *  @returns true if should skip normal startmatch process
  */
function bool CheckForSentinelRun()
{
	return (MyAutoTestManager != None) && MyAutoTestManager.CheckForSentinelRun();
}

/** This is for the QA team who don't use UFE nor commandline :-( **/
exec simulated function BeginBVT( optional coerce string TagDesc )
{
	if ( MyAutoTestManager == None )
	{
		MyAutoTestManager = spawn(AutoTestManagerClass);
	}

	MyAutoTestManager.BeginSentinelRun( "BVT", "", TagDesc );
	MyAutoTestManager.SetTimer( 3.0f, TRUE, nameof(MyAutoTestManager.DoTimeBasedSentinelStatGathering) );
}



/**
 * Turns standby detection on/off
 *
 * @param bIsEnabled true to turn it on, false to disable it
 */
native function EnableStandbyCheatDetection(bool bIsEnabled);

/**
 * Notifies the game code that a standby cheat was detected
 *
 * @param StandbyType the type of cheat detected
 */
event StandbyCheatDetected(EStandbyType StandbyType);


/**
 * Used to create a new online session after the previous one has been destroyed after travelling
 *
 * @param SessionName		The name of the game session (not used with Steamworks)
 * @param bWasSuccesful		Whether or not the session was succesfully destroyed
 */
function OnDestroyOnlineGameComplete(name SessionName, bool bWasSuccessful)
{
	if (!ProcessServerLogin())
		RegisterServer();

	GameInterface.ClearDestroyOnlineGameCompleteDelegate(OnDestroyOnlineGameComplete);
}

/**
 * Notifies the game code that the engine has finished loading. This
 * function will only be called one time only.
 */
event OnEngineHasLoaded();

function InitCrowdPopulationManager()
{
	if( PopulationManagerClass != None )
	{
		PopulationManager = Spawn(PopulationManagerClass);
	}
}

/**
 * Cleans up any online subsystem delegates that are set
 */
function ClearOnlineDelegates();


defaultproperties
{
	// The game spawns bots/players which can't be done during physics ticking
	TickGroup=TG_PreAsyncWork

	GameSpeed=1.0
	bDelayedStart=true
	HUDType=class'Engine.HUD'
	bWaitingToStartMatch=false
    bRestartLevel=True
    bPauseable=True
	AccessControlClass=class'Engine.AccessControl'
	BroadcastHandlerClass=class'Engine.BroadcastHandler'
	DeathMessageClass=class'LocalMessage'
	PlayerControllerClass=class'Engine.PlayerController'
	GameMessageClass=class'GameMessage'
	GameReplicationInfoClass=class'GameReplicationInfo'
	AutoTestManagerClass=class'Engine.AutoTestManager'
    FearCostFalloff=+0.95
	CurrentID=1
	PlayerReplicationInfoClass=Class'Engine.PlayerReplicationInfo'
	MaxSpectatorsAllowed=32
	MaxPlayersAllowed=32

	Components.Remove(Sprite)

	// Defaults for if your game has only one skill leaderboard
	LeaderboardId=0xFFFE0000
	ArbitratedLeaderboardId=0xFFFF0000

//	PopulationManagerClass=class'CrowdPopulationManagerBase'

	StreamingPauseIcon=Material'EngineResources.M_StreamingPause'
}
