//=============================================================================
// GameInfo.
//
// The GameInfo defines the game being played: the game rules, scoring, what actors
// are allowed to exist in this game type, and who may enter the game.  While the
// GameInfo class is the public interface, much of this functionality is delegated
// to several classes to allow easy modification of specific game components.  These
// classes include GameInfo, AccessControl, Mutator, BroadcastHandler, and GameRules.
// A GameInfo actor is instantiated when the level is initialized for gameplay (in
// C++ UGameEngine::LoadMap() ).  The class of this GameInfo actor is determined by
// (in order) either the DefaultGameType if specified in the LevelInfo, or the
// DefaultGame entry in the game's .ini file (in the Engine.Engine section), unless
// its a network game in which case the DefaultServerGame entry is used.
//
//=============================================================================
class GameInfo extends Info
	config(Game)
	native;

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
var bool						bLoggingGame;           // Does this gametype log?
var globalconfig bool			bEnableStatLogging;		// If True, games will log
var globalconfig bool			bAdminCanPause;
var bool						bGameRestarted;
var bool					  bAllowVehicles;			// whether vehicles are allowed in this game type (hint for networking)
var bool bIsSaveGame;			// stays true during entire game (unlike LevelInfo property)

var globalconfig float        GameDifficulty;
var	  globalconfig int		  GoreLevel;				// 0=Normal, increasing values=less gore
var   globalconfig float	  GameSpeed;				// Scale applied to game rate.

var   config string				  DefaultPawnClassName;

// user interface
var   string                  ScoreBoardType;           // Type of class<Menu> to use for scoreboards. (gam)
var	  string				  HUDType;					// HUD class this game uses.
var   string				  MapListType;				// Maplist this game uses.
var   string			      MapPrefix;				// Prefix characters for names of maps for this game type.

var   globalconfig int	      MaxSpectators;			// Maximum number of spectators.
var	  int					  NumSpectators;			// Current number of spectators.
var   globalconfig int		  MaxPlayers;
var   int					  NumPlayers;				// number of human players
var	  int					  NumBots;					// number of non-human players (AI controlled but participating as a player)
var   int					  CurrentID;				// used to assign unique PlayerIDs to each PlayerReplicationInfo
var localized string	      DefaultPlayerName;
var localized string	      GameName;
var float					  FearCostFallOff;			// how fast the FearCost in NavigationPoints falls off

var config int                GoalScore;                // what score is needed to end the match
var config int                MaxLives;	                // max number of lives for match, unless overruled by level's GameDetails
var config int                TimeLimit;                // time limit in minutes

// Message classes.
var class<LocalMessage>		  DeathMessageClass;
var class<GameMessage>		  GameMessageClass;

//-------------------------------------
// GameInfo components
var string MutatorClass;
var private Mutator BaseMutator;				// linked list of Mutators (for modifying actors as they enter the game)
var globalconfig string AccessControlClass;
var private AccessControl AccessControl;		// AccessControl controls whether players can enter and/or become admins
var private GameRules GameRulesModifiers;		// linked list of modifier classes which affect game rules
var string BroadcastHandlerClass;
var private BroadcastHandler BroadcastHandler;	// handles message (text and localized) broadcasts

var class<PlayerController> PlayerControllerClass;	// type of player controller to spawn for players logging in
var config string			PlayerControllerClassName;
var class<PlayerReplicationInfo> 		PlayerReplicationInfoClass;

// ReplicationInfo
var() class<GameReplicationInfo> GameReplicationInfoClass;
var GameReplicationInfo GameReplicationInfo;

// Stats
var GameStats                   GameStats;				// Holds the GameStats actor
var globalconfig string			GameStatsClass;			// Type of GameStats actor to spawn

var globalconfig float MaxIdleTime;		// maximum time players are allowed to idle before being kicked

//------------------------------------------------------------------------------
// Engine notifications.

function PreBeginPlay()
{
	SetGameSpeed(GameSpeed);
	GameReplicationInfo = Spawn(GameReplicationInfoClass);
	Level.GRI = GameReplicationInfo;

	InitGameReplicationInfo();
	// Create stat logging actor.
    InitLogging();
}

function string FindPlayerByID( int PlayerID )
{
    local PlayerReplicationInfo PRI;

	PRI = GameReplicationInfo.FindPlayerByID(PlayerID);
	if ( PRI != None )
		return PRI.PlayerName;
    return "";
}

static function bool UseLowGore()
{
	return ( Default.GoreLevel > 0 );
}

function PostBeginPlay()
{
	if ( MaxIdleTime > 0 )
		MaxIdleTime = FMax(MaxIdleTime, 20);

	if (GameStats!=None)
	{
		GameStats.NewGame();
		GameStats.ServerInfo();
	}
}

/* Reset() - reset actor to initial state - used when restarting level without reloading. */
function Reset()
{
	super.Reset();
	bGameEnded = false;
	bOverTime = false;
	bWaitingToStartMatch = true;
	InitGameReplicationInfo();
}

/* Reset level */
exec function ResetLevel()
{
	local Controller		C, NextC;
	local Actor				A;

	// Reset ALL controllers first
	C = Level.ControllerList;
	while ( C != None )
	{
		NextC = C.NextController;
		if ( C.PlayerReplicationInfo == None || !C.PlayerReplicationInfo.bOnlySpectator )
		{
			if ( PlayerController(C) != None )
				PlayerController(C).ClientReset();
			C.Reset();
		}

		C = NextC;
	}

	// Reset ALL actors (except controllers)
	foreach AllActors(class'Actor', A)
		if ( !A.IsA('Controller') && !A.IsA('GameInfo') )
			A.Reset();
}

/* InitLogging()
Set up statistics logging
*/
function InitLogging()
{
	local class <GameStats> MyGameStatsClass;

    if ( !bEnableStatLogging || !bLoggingGame || (Level.NetMode == NM_Standalone) || (Level.NetMode == NM_ListenServer) )
		return;

	MyGameStatsClass=class<GameStats>(DynamicLoadObject(GameStatsClass,class'class'));
    if (MyGameStatsClass!=None)
    {
		GameStats = spawn(MyGameStatsClass);
        if (GameStats==None)
        	log("Could to create Stats Object");
    }
    else
    	log("Error loading GameStats ["$GameStatsClass$"]");
}

function Timer()
{
	local NavigationPoint N;

	BroadcastHandler.UpdateSentText();
    for ( N=Level.NavigationPointList; N!=None; N=N.NextNavigationPoint )
		N.FearCost *= FearCostFallOff;
}

// Called when game shutsdown.
event GameEnding()
{
	EndLogging("serverquit");
}

/* KickIdler() called if
		if ( (Pawn != None) || (PlayerReplicationInfo.bOnlySpectator && (ViewTarget != self))
			|| (Level.Pauser != None) || Level.Game.bWaitingToStartMatch || Level.Game.bGameEnded )
		{
			LastActiveTime = Level.TimeSeconds;
		}
		else if ( (Level.Game.MaxIdleTime > 0) && (Level.TimeSeconds - LastActiveTime > Level.Game.MaxIdleTime) )
			KickIdler(self);
*/
event KickIdler(PlayerController PC)
{
	log("Kicking idle player "$PC.PlayerReplicationInfo.PlayerName);
	AccessControl.KickPlayer(PC, AccessControl.IdleKickReason);
}

//------------------------------------------------------------------------------
// Replication

function InitGameReplicationInfo()
{
	GameReplicationInfo.bTeamGame = bTeamGame;
	GameReplicationInfo.GameName = GameName;
	GameReplicationInfo.GameClass = string(Class);
    GameReplicationInfo.MaxLives = MaxLives;
}

native function string GetNetworkNumber();

//------------------------------------------------------------------------------
// Server/Game Querying.

function GetServerInfo( out ServerResponseLine ServerState )
{
	ServerState.ServerName		= StripColor(GameReplicationInfo.ServerName);
	ServerState.MapName			= Left(string(Level), InStr(string(Level), "."));
	ServerState.GameType		= Mid( string(Class), InStr(string(Class), ".")+1);
	ServerState.CurrentPlayers	= GetNumPlayers();
	ServerState.MaxPlayers		= MaxPlayers;
	ServerState.IP				= ""; // filled in at the other end.
	ServerState.Port			= GetServerPort();

	ServerState.ServerInfo.Length = 0;
	ServerState.PlayerInfo.Length = 0;
}

function int GetNumPlayers()
{
	return NumPlayers;
}

function GetServerDetails( out ServerResponseLine ServerState )
{
	local Mutator M;

	if ( Level.NetMode == NM_ListenServer )
        AddServerDetail( ServerState, "ServerMode", "non-dedicated" );
	else
        AddServerDetail( ServerState, "ServerMode", "dedicated" );

	AddServerDetail( ServerState, "AdminName", GameReplicationInfo.AdminName );
	AddServerDetail( ServerState, "AdminEmail", GameReplicationInfo.AdminEmail );
	AddServerDetail( ServerState, "ServerVersion", Level.EngineVersion );

	if ( AccessControl != None && AccessControl.RequiresPassword() )
		AddServerDetail( ServerState, "GamePassword", "True" );

	AddServerDetail( ServerState, "GameStats", GameStats != None );

	AddServerDetail( ServerState, "MaxSpectators", MaxSpectators );

	// Ask the mutators if they have anything to add.
	if ( BaseMutator != None )
	{
		for (M = BaseMutator.NextMutator; M != None; M = M.NextMutator)
			M.GetServerDetails(ServerState);
	}
// FIXMESTEVE - standard server needs to also be checked server-side, in case serverdetails are messed with
}

static function AddServerDetail( out ServerResponseLine ServerState, string RuleName, coerce string RuleValue )
{
	local int i;

	i = ServerState.ServerInfo.Length;
	ServerState.ServerInfo.Length = i + 1;

	ServerState.ServerInfo[i].Key = RuleName;
	ServerState.ServerInfo[i].Value = RuleValue;
}

function GetServerPlayers( out ServerResponseLine ServerState )
{
    local Mutator M;
	local Controller C;
	local PlayerReplicationInfo PRI;
	local int i;

	i = ServerState.PlayerInfo.Length;

	for( C=Level.ControllerList;C!=None;C=C.NextController )
        {
			PRI = C.PlayerReplicationInfo;
			if( (PRI != None) && !PRI.bBot && MessagingSpectator(C) == None )
            {
			ServerState.PlayerInfo.Length = i+1;
			ServerState.PlayerInfo[i].PlayerNum  = C.PlayerNum;
			ServerState.PlayerInfo[i].PlayerName = PRI.PlayerName;
			ServerState.PlayerInfo[i].Score		 = PRI.Score;
			ServerState.PlayerInfo[i].Ping		 = 4 * PRI.Ping;
			i++;
		}
	}

	// Ask the mutators if they have anything to add.
	if ( BaseMutator != None )
	{
		for (M = BaseMutator.NextMutator; M != None; M = M.NextMutator)
			M.GetServerPlayers(ServerState);
	}
}

//------------------------------------------------------------------------------
// Misc.

// Return the server's port number.
function int GetServerPort()
{
	local string S;
	local int i;

	// Figure out the server's port.
	S = Level.GetAddressURL();
	i = InStr( S, ":" );
	assert(i>=0);
	return int(Mid(S,i+1));
}

function bool SetPause( BOOL bPause, PlayerController P )
{
    if( bPauseable || (bAdminCanPause && (P.IsA('Admin') || P.PlayerReplicationInfo.bAdmin)) || Level.Netmode==NM_Standalone )
	{
		if( bPause )
			Level.Pauser=P.PlayerReplicationInfo;
		else
			Level.Pauser=None;
		return True;
	}
	else return False;
}

//------------------------------------------------------------------------------
// Game parameters.

//
// Set gameplay speed.
//
function SetGameSpeed( Float T )
{
	local float OldSpeed;

	OldSpeed = GameSpeed;
	GameSpeed = FMax(T, 0.1);
	Level.TimeDilation = GameSpeed;
	if ( GameSpeed != OldSpeed )
	{
		Default.GameSpeed = GameSpeed;
		class'GameInfo'.static.StaticSaveConfig();
	}
	SetTimer(Level.TimeDilation, true);
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

/* Initialize the game.
 The GameInfo's InitGame() function is called before any other scripts (including
 PreBeginPlay() ), and is used by the GameInfo to initialize parameters and spawn
 its helper classes.
 Warning: this is called before actors' PreBeginPlay.
*/
event InitGame( string Options, out string Error )
{
	local string InOpt, LeftOpt;
	local int pos;
	local class<AccessControl> ACClass;
	local class<GameRules> GRClass;
	local class<BroadcastHandler> BHClass;

    MaxPlayers = Clamp(GetIntOption( Options, "MaxPlayers", MaxPlayers ),0,32);
    MaxSpectators = Clamp(GetIntOption( Options, "MaxSpectators", MaxSpectators ),0,32);
    GameDifficulty = FMax(0,GetIntOption(Options, "Difficulty", GameDifficulty));

	InOpt = ParseOption( Options, "GameSpeed");
	if( InOpt != "" )
	{
		log("GameSpeed"@InOpt);
		SetGameSpeed(float(InOpt));
	}

    AddMutator(MutatorClass);

	BHClass = class<BroadcastHandler>(DynamicLoadObject(BroadcastHandlerClass,Class'Class'));
	BroadcastHandler = spawn(BHClass);

	InOpt = ParseOption( Options, "AccessControl");
	if( InOpt != "" )
		ACClass = class<AccessControl>(DynamicLoadObject(InOpt, class'Class'));
    if ( ACClass == None )
	{
		ACClass = class<AccessControl>(DynamicLoadObject(AccessControlClass, class'Class'));
		if (ACClass == None)
			ACClass = class'Engine.AccessControl';
	}

	LeftOpt = ParseOption( Options, "AdminName" );
	InOpt = ParseOption( Options, "AdminPassword");
	if( LeftOpt!="" && InOpt!="" )
		ACClass.default.bDontAddDefaultAdmin = true;

	// Only spawn access control if we are a server
	if (Level.NetMode == NM_ListenServer || Level.NetMode == NM_DedicatedServer )
	{
		AccessControl = Spawn(ACClass);
	}

	InOpt = ParseOption( Options, "GameRules");
	if ( InOpt != "" )
	{
		log("Game Rules"@InOpt);
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
			log("Add game rules "$LeftOpt);
			GRClass = class<GameRules>(DynamicLoadObject(LeftOpt, class'Class'));
			if ( GRClass != None )
			{
				if ( GameRulesModifiers == None )
					GameRulesModifiers = Spawn(GRClass);
				else
					GameRulesModifiers.AddGameRules(Spawn(GRClass));
			}
		}
	}

	InOpt = ParseOption( Options, "Mutator");
	if ( InOpt != "" )
	{
		log("Mutators"@InOpt);
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
		log( "GamePassword" @ InOpt );
	}

	InOpt = ParseOption(Options, "GameStats");
	if ( InOpt != "")
		bEnableStatLogging = bool(InOpt);

	log("GameInfo::InitGame : bEnableStatLogging"@bEnableStatLogging);
}

function AddMutator(string mutname, optional bool bUserAdded)
{
    local class<Mutator> mutClass;
    local Mutator mut;

	if ( !Static.AllowMutator(MutName) )
		return;

    mutClass = class<Mutator>(DynamicLoadObject(mutname, class'Class'));
    if (mutClass == None)
        return;

	if ( (mutClass.Default.GroupName != "") && (BaseMutator != None) )
	{
		// make sure no mutators with same groupname
		for ( mut=BaseMutator; mut!=None; mut=mut.NextMutator )
			if ( mut.GroupName == mutClass.Default.GroupName )
			{
				log("Not adding "$mutClass$" because already have a mutator in the same group - "$mut);
				return;
			}
	}

	// make sure this mutator is not added already
	for ( mut=BaseMutator; mut!=None; mut=mut.NextMutator )
		if ( mut.Class == mutClass )
		{
			log("Not adding "$mutClass$" because this mutator is already added - "$mut);
				return;
	}

    mut = Spawn(mutClass);
	// mc, beware of mut being none
	if (mut == None)
		return;

	// Meant to verify if this mutator was from Command Line parameters or added from other Actors
	mut.bUserAdded = bUserAdded;

    if (BaseMutator == None)
        BaseMutator = mut;
    else
        BaseMutator.AddMutator(mut);
}

/* RemoveMutator()
Remove a mutator from the mutator list
*/
function RemoveMutator( Mutator MutatorToRemove )
{
	local Mutator M;
	
	// remove from mutator list
	if ( BaseMutator == MutatorToRemove )
		BaseMutator = MutatorToRemove.NextMutator;
	else if ( BaseMutator != None )
	{
		for ( M=BaseMutator; M!=None; M=M.NextMutator )
			if ( M.NextMutator == MutatorToRemove )
			{	
				M.NextMutator = MutatorToRemove.NextMutator;
				break;
			}
	}
}

//
// Return beacon text for serverbeacon.
//
event string GetBeaconText()
{
	return
		Level.ComputerName
    $   " "
    $   Left(Level.Title,24)
    $   "\\t"
    $   MapPrefix
    $   "\\t"
    $   GetNumPlayers()
	$	"/"
	$	MaxPlayers;
}

/* ProcessServerTravel()
 Optional handling of ServerTravel for network games.
*/
function ProcessServerTravel( string URL, bool bItems )
{
    local playercontroller P, LocalPlayer;

	EndLogging("mapchange");

	// Notify clients we're switching level and give them time to receive.
	// We call PreClientTravel directly on any local PlayerPawns (ie listen server)
	log("ProcessServerTravel:"@URL);
	foreach DynamicActors( class'PlayerController', P )
		if( NetConnection( P.Player)!=None )
			P.ClientTravel( URL, TRAVEL_Relative, bItems );
		else
		{
			LocalPlayer = P;
			P.PreClientTravel();
		}

	if ( (Level.NetMode == NM_ListenServer) && (LocalPlayer != None) )
        Level.NextURL = Level.NextURL
					 $"?Team="$LocalPlayer.GetDefaultURL("Team")
					 $"?Name="$LocalPlayer.GetDefaultURL("Name")
                     $"?Class="$LocalPlayer.GetDefaultURL("Class")
                     $"?Character="$LocalPlayer.GetDefaultURL("Character");

	// Switch immediately if not networking.
	if( Level.NetMode!=NM_DedicatedServer && Level.NetMode!=NM_ListenServer )
		Level.NextSwitchCountdown = 0.0;
}

function bool RequiresPassword()
{
	return ( (AccessControl != None) && AccessControl.RequiresPassword() );
}

//
// Accept or reject a player on the server.
// Fails login if you set the Error to a non-empty string.
//
event PreLogin
(
	string Options,
	string Address,
	out string Error,
	out string FailCode
)
{
	local bool bSpectator;

    bSpectator = ( ParseOption( Options, "SpectatorOnly" ) ~= "1" );
    if (AccessControl != None)
	AccessControl.PreLogin(Options, Address, Error, FailCode, bSpectator);
}

function int GetIntOption( string Options, string ParseString, int CurrentValue)
{
	local string InOpt;

	InOpt = ParseOption( Options, ParseString );
	if ( InOpt != "" )
	{
		return int(InOpt);
	}
	return CurrentValue;
}

function bool AtCapacity(bool bSpectator)
{
	if ( Level.NetMode == NM_Standalone )
		return false;

	if ( bSpectator )
		return ( (NumSpectators >= MaxSpectators)
			&& ((Level.NetMode != NM_ListenServer) || (NumPlayers > 0)) );
	else
		return ( (MaxPlayers>0) && (NumPlayers>=MaxPlayers) );
}

//
// Log a player in.
// Fails login if you set the Error string.
// PreLogin is called before Login, but significant game time may pass before
// Login is called, especially if content is downloaded.
//
event PlayerController Login
(
	string Portal,
	string Options,
	out string Error
)
{
	local NavigationPoint StartSpot;
	local PlayerController NewPlayer;
    local string          InName, InAdminName, InPassword, InChecksum, InCharacter;
	local byte            InTeam;
    local bool bSpectator, bAdmin;

	Options = StripColor(Options);	// Strip out color codes
		
    bSpectator = ( ParseOption( Options, "SpectatorOnly" ) ~= "1" );

    // Make sure there is capacity except for admins. (This might have changed since the PreLogin call).
    if ( !bAdmin && AtCapacity(bSpectator) )
	{
		Error=GameMessageClass.Default.MaxedOutMessage;
		return None;
	}

	// If admin, force spectate mode if the server already full of reg. players
	if ( bAdmin && AtCapacity(false))
		bSpectator = true;

	if ( BaseMutator != None )
		BaseMutator.ModifyLogin(Portal, Options);

	// Get URL options.
	InName     = Left(ParseOption ( Options, "Name"), 20);
	InTeam     = GetIntOption( Options, "Team", 255 ); // default to "no team"
    InAdminName= ParseOption ( Options, "AdminName");
	InPassword = ParseOption ( Options, "Password" );
	InChecksum = ParseOption ( Options, "Checksum" );

	// Pick a team (if need teams)
    InTeam = PickTeam(InTeam,None);

	// Find a start spot.
	StartSpot = FindPlayerStart( None, InTeam, Portal );

	if( StartSpot == None )
	{
		Error = GameMessageClass.Default.FailedPlaceMessage;
		return None;
	}

	if ( PlayerControllerClass == None )
		PlayerControllerClass = class<PlayerController>(DynamicLoadObject(PlayerControllerClassName, class'Class'));

	NewPlayer = spawn(PlayerControllerClass,,,StartSpot.Location,StartSpot.Rotation);

	// Handle spawn failure.
	if( NewPlayer == None )
	{
		log("Couldn't spawn player controller of class "$PlayerControllerClass);
		Error = GameMessageClass.Default.FailedSpawnMessage;
		return None;
	}

	NewPlayer.StartSpot = StartSpot;

    // Set the player's ID.
    NewPlayer.PlayerReplicationInfo.PlayerID = CurrentID++;

	// Init player's name
	if( InName=="" )
		InName=DefaultPlayerName$NewPlayer.PlayerReplicationInfo.PlayerID;
	ChangeName( NewPlayer, InName, false );

    InCharacter = ParseOption(Options, "Character");
    NewPlayer.SetCharacter(InCharacter);


    if ( bSpectator || NewPlayer.PlayerReplicationInfo.bOnlySpectator || !ChangeTeam(newPlayer, InTeam, false) )
	{
		NewPlayer.GotoState('Spectating');
        NewPlayer.PlayerReplicationInfo.bOnlySpectator = true;
		NewPlayer.PlayerReplicationInfo.bIsSpectator = true;
        NewPlayer.PlayerReplicationInfo.bOutOfLives = true;
		NumSpectators++;
		return NewPlayer;
	}

	newPlayer.StartSpot = StartSpot;

	NumPlayers++;

	// if delayed start, don't give a pawn to the player yet
	// Normal for multiplayer games
	if ( bDelayedStart )
	{
		NewPlayer.GotoState('PlayerWaiting');
		return NewPlayer;
	}

	return newPlayer;
}

exec function TestLevel()
{
	local Actor A, Found;
	local bool bFoundErrors;

	ForEach AllActors(class'Actor', A)
	{
		bFoundErrors = bFoundErrors || A.CheckForErrors();
		if ( bFoundErrors && (Found == None) )
			Found = A;
	}

	if ( bFoundErrors )
	{
		log("Found problem with "$Found);
		assert(false);
	}
}

/* StartMatch()
Start the game - inform all actors that the match is starting, and spawn player pawns
*/
function StartMatch()
{
	local Controller P;
	local Actor A;

	if (GameStats!=None)
		GameStats.StartGame();

	// tell all actors the game is starting
	ForEach AllActors(class'Actor', A)
		A.MatchStarting();

	// start human players first
	for ( P = Level.ControllerList; P!=None; P=P.nextController )
		if ( P.IsA('PlayerController') && (P.Pawn == None) )
		{
            if ( bGameEnded )
                return; // telefrag ended the game with ridiculous frag limit
            else if ( PlayerController(P).CanRestartPlayer()  )
				RestartPlayer(P);
		}

	// start AI players
	for ( P = Level.ControllerList; P!=None; P=P.nextController )
		if ( P.bIsPlayer && !P.IsA('PlayerController') )
        {
			if ( Level.NetMode == NM_Standalone )
			RestartPlayer(P);
        	else
				P.GotoState('Dead','MPStart');
		}

	bWaitingToStartMatch = false;
	GameReplicationInfo.bMatchHasBegun = true;
}

//
// Restart a player.
//
function RestartPlayer( Controller aPlayer )
{
	local NavigationPoint startSpot;
	local int TeamNum;
	local class<Pawn> DefaultPlayerClass;

	if( bRestartLevel && Level.NetMode!=NM_DedicatedServer && Level.NetMode!=NM_ListenServer )
	{
		Warn("bRestartLevel && !server, abort from RestartPlayer"@Level.NetMode);
		return;
	}

	if ( (aPlayer.PlayerReplicationInfo == None) || (aPlayer.PlayerReplicationInfo.Team == None) )
		TeamNum = 255;
	else
		TeamNum = aPlayer.PlayerReplicationInfo.Team.TeamIndex;

	StartSpot = FindPlayerStart(aPlayer, TeamNum);
	if( startSpot == None )
	{
		if ( aPlayer.StartSpot != None )
		{
			StartSpot = aPlayer.StartSpot;
			log(" Player start not found!!! using last start spot");
		}
		else
		{
			log(" Player start not found!!!");
			return;
		}
	}

	if( aPlayer.Pawn==None )
	{
        DefaultPlayerClass = GetDefaultPlayerClass(aPlayer);
		aPlayer.Pawn = Spawn(DefaultPlayerClass,,,StartSpot.Location,StartSpot.Rotation);
	}
	if ( aPlayer.Pawn == None )
	{
		log("Couldn't spawn player of type "$DefaultPlayerClass$" at "$StartSpot);
		aPlayer.GotoState('Dead');
        if ( PlayerController(aPlayer) != None )
			PlayerController(aPlayer).ClientGotoState('Dead','Begin');
		return;
	}
    if ( PlayerController(aPlayer) != None )
		PlayerController(aPlayer).TimeMargin = -0.1;
    aPlayer.Pawn.Anchor = startSpot;
	aPlayer.Pawn.LastStartSpot = PlayerStart(startSpot);
	aPlayer.Pawn.LastStartTime = Level.TimeSeconds;
	aPlayer.Possess(aPlayer.Pawn);

    aPlayer.Pawn.PlayTeleportEffect(true, true);
	aPlayer.ClientSetRotation(aPlayer.Pawn.Rotation);
	AddDefaultInventory(aPlayer.Pawn);
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
    return( class<Pawn>( DynamicLoadObject( DefaultPawnClassName, class'Class' ) ) );
}

//
// Called after a successful login. This is the first place
// it is safe to call replicated functions on the PlayerController.
//
event PostLogin( PlayerController NewPlayer )
{
    local class<HUD> HudClass;
    local class<Scoreboard> ScoreboardClass;

	if ( !bIsSaveGame )
	{		
    // Log player's login.
	if (GameStats!=None)
	{
		GameStats.ConnectEvent(NewPlayer.PlayerReplicationInfo);
		GameStats.GameEvent("NameChange",NewPlayer.PlayerReplicationInfo.playername,NewPlayer.PlayerReplicationInfo);
	}

	if ( !bDelayedStart )
	{
		// start match, or let player enter, immediately
		bRestartLevel = false;	// let player spawn once in levels that must be restarted after every death
		if ( bWaitingToStartMatch )
			StartMatch();
		else
			RestartPlayer(newPlayer);
		bRestartLevel = Default.bRestartLevel;
	}
	}

	// tell client what hud and scoreboard to use

    if( HUDType != "" )
        HudClass = class<HUD>(DynamicLoadObject(HUDType, class'Class'));


    if( ScoreBoardType != "" )
        ScoreboardClass = class<Scoreboard>(DynamicLoadObject(ScoreBoardType, class'Class'));
    NewPlayer.ClientSetHUD( HudClass, ScoreboardClass );

    if ( bIsSaveGame )
		return;

	if ( NewPlayer.Pawn != None )
		NewPlayer.Pawn.ClientSetRotation(NewPlayer.Pawn.Rotation);

	NewPlayer.ClientCapBandwidth(NewPlayer.Player.CurrentNetSpeed);
	if ( BaseMutator != None )
		BaseMutator.NotifyLogin(NewPlayer);
}

//
// Player exits.
//
function Logout( Controller Exiting )
{
	local bool bMessage;

	bMessage = true;
	if ( PlayerController(Exiting) != None )
	{
//FIXMESTEVE		if ( AccessControl.AdminLogout( PlayerController(Exiting) ) )
//			AccessControl.AdminExited( PlayerController(Exiting) );

        if ( PlayerController(Exiting).PlayerReplicationInfo.bOnlySpectator )
		{
			bMessage = false;
				NumSpectators--;
		}
		else
        {
			NumPlayers--;
        }
		//notify mutators that a player exited
		if ( BaseMutator != None )
			BaseMutator.NotifyLogout(PlayerController(Exiting));
	}

	if ( GameStats!=None)
		GameStats.DisconnectEvent(Exiting.PlayerReplicationInfo);
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
function AddDefaultInventory( Pawn P )
{
    // Allow the pawn itself to modify its inventory
    P.AddDefaultInventory();

	if ( P.InvManager == None )
	{
		Warn("GameInfo::AddDefaultInventory - P.InvManager == None");
		return;
	}

	SetPlayerDefaults( P );
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
    PlayerPawn.JumpZ = PlayerPawn.Default.JumpZ;
	if ( BaseMutator != None )
		BaseMutator.ModifyPlayer(PlayerPawn);
}

function NotifyKilled(Controller Killer, Controller Killed, Pawn KilledPawn )
{
	local Controller C;

	for ( C=Level.ControllerList; C!=None; C=C.nextController )
		C.NotifyKilled(Killer, Killed, KilledPawn);
}

function KillEvent(string Killtype, PlayerReplicationInfo Killer, PlayerReplicationInfo Victim, class<DamageType> Damage)
{
	if ( GameStats != None )
		GameStats.KillEvent(KillType, Killer, Victim, Damage);
}

function Killed( Controller Killer, Controller Killed, Pawn KilledPawn, class<DamageType> damageType )
{
    if ( (Killed != None) && Killed.bIsPlayer )
	{
		Killed.PlayerReplicationInfo.Deaths += 1;
        Killed.PlayerReplicationInfo.NetUpdateTime = FMin(Killed.PlayerReplicationInfo.NetUpdateTime, Level.TimeSeconds + 0.3 * FRand());
		BroadcastDeathMessage(Killer, Killed, damageType);

		if ( (Killer == Killed) || (Killer == None) )
		{
			if ( Killer == None )
				KillEvent("K", None, Killed.PlayerReplicationInfo, DamageType);	//"Kill"
			else
				KillEvent("K", Killer.PlayerReplicationInfo, Killed.PlayerReplicationInfo, DamageType);	//"Kill"
		}
		else if ( bTeamGame && (Killer.PlayerReplicationInfo != None)
			&& (Killer.PlayerReplicationInfo.Team == Killed.PlayerReplicationInfo.Team) )
		{
			KillEvent("TK", Killer.PlayerReplicationInfo, Killed.PlayerReplicationInfo, DamageType);	//"Teamkill"
		}
			else
			{
			KillEvent("K", Killer.PlayerReplicationInfo, Killed.PlayerReplicationInfo, DamageType);	//"Kill"
		}
	}
    if ( Killed != None )
		ScoreKill(Killer, Killed);
	DiscardInventory(KilledPawn);
    NotifyKilled(Killer,Killed,KilledPawn);
}

function bool PreventDeath(Pawn Killed, Controller Killer, class<DamageType> damageType, vector HitLocation)
{
	if ( GameRulesModifiers == None )
		return false;
	return GameRulesModifiers.PreventDeath(Killed,Killer, damageType,HitLocation);
}

function BroadcastDeathMessage(Controller Killer, Controller Other, class<DamageType> damageType)
{
	if ( (Killer == Other) || (Killer == None) )
        BroadcastLocalized(self,DeathMessageClass, 1, None, Other.PlayerReplicationInfo, damageType);
	else
        BroadcastLocalized(self,DeathMessageClass, 0, Killer.PlayerReplicationInfo, Other.PlayerReplicationInfo, damageType);
}


// %k = Owner's PlayerName (Killer)
// %o = Other's PlayerName (Victim)
// %w = Owner's Weapon ItemName
static native function string ParseKillMessage( string KillerName, string VictimName, string DeathMessage );

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
function bool CanSpectate( PlayerController Viewer, bool bOnlySpectator, actor ViewTarget )
{
	return true;
}

/* ReduceDamage: 
	Use reduce damage for teamplay modifications, etc. */
function ReduceDamage( out int Damage, pawn injured, pawn instigatedBy, vector HitLocation, out vector Momentum, class<DamageType> DamageType )
{
	local int OriginalDamage;

	OriginalDamage = Damage;

	if ( injured.PhysicsVolume.bNeutralZone || injured.InGodMode() )
	{
		Damage = 0;
		return;
	}
	else if ( (damage > 0) && (injured.InvManager != None) ) // then check if carrying items that can reduce damage
		injured.InvManager.ModifyDamage( Damage, instigatedBy, HitLocation, Momentum, DamageType );

	if ( GameRulesModifiers != None )
        GameRulesModifiers.NetDamage( OriginalDamage, Damage,injured,instigatedBy,HitLocation,Momentum,DamageType );
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

//
// Return whether an item should respawn.
//
function bool ShouldRespawn( PickupFactory Other )
{
	return ( Level.NetMode != NM_Standalone );
}

/* PickupQuery:
	Called when pawn has a chance to pick Item up (i.e. when
	the pawn touches a weapon pickup). Should return true if
	he wants to pick it up, false if he does not want it. */
function bool PickupQuery( Pawn Other, class<Inventory> ItemClass )
{
	local byte bAllowPickup;

	if ( (GameRulesModifiers != None) && GameRulesModifiers.OverridePickupQuery(Other, ItemClass, bAllowPickup) )
		return (bAllowPickup == 1);

	if ( Other.InvManager == None )
		return false;
	else
		return Other.InvManager.HandlePickupQuery( ItemClass );
}

/* DiscardInventory:
	Discard a player's inventory after he dies. */
function DiscardInventory( Pawn Other )
{
	if ( Other.InvManager != None )
		Other.InvManager.DiscardInventory();
}

/* Try to change a player's name.
*/
function ChangeName( Controller Other, coerce string S, bool bNameChange )
{
	if( S == "" )
		return;

	S = StripColor(s);	// Strip out color codes

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
	aPlayer.ClientTravel( URL, TRAVEL_Relative, true );
}

/* Restart the game.
*/
function RestartGame()
{
	local string NextMap;
    local MapList MyList;

	if ( (GameRulesModifiers != None) && GameRulesModifiers.HandleRestartGame() )
		return;

	if ( bGameRestarted )
		return;
    bGameRestarted = true;

	// these server travels should all be relative to the current URL
	if ( bChangeLevels && !bAlreadyChanged && (MapListType != "") )
	{
		// open a the nextmap actor for this game type and get the next map
		bAlreadyChanged = true;
        MyList = GetMapList(MapListType);
		if (MyList != None)
		{
			NextMap = MyList.GetNextMap();
			MyList.Destroy();
		}

		if ( NextMap != "" )
		{
			Level.ServerTravel(NextMap, false);
			return;
		}
	}

	Level.ServerTravel( "?Restart", false );
}

function MapList GetMapList(string MapListClassType)
{
local class<MapList> MapListClass;

	if (MapListClassType != "")
	{
        MapListClass = class<MapList>(DynamicLoadObject(MapListClassType, class'Class'));
		if (MapListClass != None)
			return Spawn(MapListClass);
	}
	return None;
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

//==========================================================================

function bool CheckEndGame(PlayerReplicationInfo Winner, string Reason)
{
	local Controller P;

	if ( (GameRulesModifiers != None) && !GameRulesModifiers.CheckEndGame(Winner, Reason) )
		return false;

	// all player cameras focus on winner or final scene (picked by gamerules)
	for ( P=Level.ControllerList; P!=None; P=P.NextController )
	{
		P.ClientGameEnded();
        P.GameHasEnded();
	}
	return true;
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

	bGameEnded = true;
	EndLogging(Reason);
}

function EndLogging(string Reason)
{

	if (GameStats == None)
		return;

	GameStats.EndGame(Reason);
	GameStats.Destroy();
	GameStats = None;
}

/* Return the 'best' player start for this player to start from.
 */
function NavigationPoint FindPlayerStart( Controller Player, optional byte InTeam, optional string incomingName )
{
	local NavigationPoint N, BestStart;
	local Teleporter Tel;
	local float BestRating, NewRating;
	local byte Team;

	// always pick StartSpot at start of match
    if ( (Player != None) && (Player.StartSpot != None) && (Level.NetMode == NM_Standalone)
		&& (bWaitingToStartMatch || ((Player.PlayerReplicationInfo != None) && Player.PlayerReplicationInfo.bWaitingPlayer))  )
	{
		return Player.StartSpot;
	}

	if ( GameRulesModifiers != None )
	{
		N = GameRulesModifiers.FindPlayerStart(Player,InTeam,incomingName);
		if ( N != None )
		    return N;
	}

	// if incoming start is specified, then just use it
	if( incomingName!="" )
		foreach AllActors( class 'Teleporter', Tel )
			if( string(Tel.Tag)~=incomingName )
				return Tel;

	// use InTeam if player doesn't have a team yet
	if ( (Player != None) && (Player.PlayerReplicationInfo != None) )
	{
		if ( Player.PlayerReplicationInfo.Team != None )
			Team = Player.PlayerReplicationInfo.Team.TeamIndex;
		else
            Team = InTeam;
	}
	else
		Team = InTeam;

	for ( N=Level.NavigationPointList; N!=None; N=N.NextNavigationPoint )
	{
        NewRating = RatePlayerStart(N,Team,Player);
		if ( NewRating > BestRating )
		{
			BestRating = NewRating;
			BestStart = N;
		}
	}

    if ( PlayerStart(BestStart) == None )
	{
        log("Warning - PATHS NOT DEFINED or NO PLAYERSTART with positive rating");
		BestRating = -100000000;
        ForEach AllActors( class 'NavigationPoint', N )
		{
			NewRating = RatePlayerStart(N,0,Player);
            if ( PickupFactory(N) != None )
				NewRating -= 50;
			NewRating += 20 * FRand();
			if ( NewRating > BestRating )
			{
				BestRating = NewRating;
				BestStart = N;
			}
		}
	}

	return BestStart;
}

/* Rate whether player should choose this NavigationPoint as its start
default implementation is for single player game
*/
function float RatePlayerStart(NavigationPoint N, byte Team, Controller Player)
{
	local PlayerStart P;

	P = PlayerStart(N);
	if ( P != None )
	{
		if ( P.bSinglePlayerStart )
		{
			if ( P.bEnabled )
				return 1000;
			return 20;
		}
		return 10;
	}
	return 0;
}

function bool CanCompleteObjective( Objective O )
{
	return true;
}

function ObjectiveCompleted( Objective O );

function ScoreObjective(PlayerReplicationInfo Scorer, Int Score)
{
	if ( Scorer != None )
	{
		Scorer.Score += Score;
	}
	if ( GameRulesModifiers != None )
		GameRulesModifiers.ScoreObjective(Scorer,Score);

	CheckScore(Scorer);
}

/* CheckScore()
see if this score means the game ends
*/
function CheckScore(PlayerReplicationInfo Scorer)
{
	if ( (GameRulesModifiers != None) && GameRulesModifiers.CheckScore(Scorer) )
		return;
}

function ScoreEvent(PlayerReplicationInfo Who, float Points, string Desc)
{
	if (GameStats!=None)
		GameStats.ScoreEvent(Who,Points,Desc);
}

function TeamScoreEvent(int Team, float Points, string Desc)
{
	if ( GameStats != None )
		GameStats.TeamScoreEvent(Team, Points, Desc);
}

function ScoreKill(Controller Killer, Controller Other)
{
	if( (killer == Other) || (killer == None) )
	{
    	if ( (Other!=None) && (Other.PlayerReplicationInfo != None) )
        {
		Other.PlayerReplicationInfo.Score -= 1;
			Other.PlayerReplicationInfo.NetUpdateTime = Level.TimeSeconds - 1;
			ScoreEvent(Other.PlayerReplicationInfo,-1,"self_frag");
        }
	}
	else if ( killer.PlayerReplicationInfo != None )
	{
		Killer.PlayerReplicationInfo.Score += 1;
		Killer.PlayerReplicationInfo.NetUpdateTime = Level.TimeSeconds - 1;
		Killer.PlayerReplicationInfo.Kills++;
		ScoreEvent(Killer.PlayerReplicationInfo,1,"frag");
	}

	if ( GameRulesModifiers != None )
		GameRulesModifiers.ScoreKill(Killer, Other);

    if ( (Killer != None) || (MaxLives > 0) )
	CheckScore(Killer.PlayerReplicationInfo);
}

static function string FindTeamDesignation(GameReplicationInfo GRI, actor A)	// Should be subclassed in various team games
{
	return "";
}

// - Parse out % vars for various messages

static function string ParseMessageString(Controller Who, String Message)
{
	return Message;
}

function ReviewJumpSpots(name TestLabel);

function bool CanEnterVehicle(Vehicle V, Pawn P)
{
	if ( BaseMutator == None )
		return true;
	return BaseMutator.CanEnterVehicle(V, P);
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

function TeamInfo OtherTeam(TeamInfo Requester)
{
	return None;
}

exec function KillBots(int num);

exec function AdminSay(string Msg)
{
	local controller C;

	for( C=Level.ControllerList; C!=None; C=C.nextController )
		if( C.IsA('PlayerController') )
		{
			PlayerController(C).ClearProgressMessages();
			PlayerController(C).SetProgressTime(6);
			PlayerController(C).SetProgressMessage(0, Msg, MakeColor(255,255,255,255));
		}
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

function RegisterVehicle(Vehicle V);

// Returns whether a mutator should be allowed with this gametype
static function bool AllowMutator( string MutatorClassName )
{
	if ( MutatorClassName == Default.MutatorClass )
		return true;

	return !class'LevelInfo'.static.IsDemoBuild();
}

/* StripColor()
strip out color codes from string
*/
function string StripColor(string s)
{
	local int p;

    p = InStr(s,chr(27));
	while ( p>=0 )
	{
		s = left(s,p)$mid(S,p+4);
		p = InStr(s,Chr(27));
	}

	return s;
}

defaultproperties
{
	bAdminCanPause=false
    bDelayedStart=true
	HUDType="Engine.HUD"
	bWaitingToStartMatch=false
	bLoggingGame=False
	MaxPlayers=16
    GameDifficulty=+1.0
    bRestartLevel=True
    bPauseable=True
    bChangeLevels=True
    GameSpeed=1.000000
    MaxSpectators=2
    DefaultPlayerName="Player"
	GameName="Game"
	MutatorClass="Engine.Mutator"
	BroadcastHandlerClass="Engine.BroadcastHandler"
	DeathMessageClass=class'LocalMessage'
	GameStatsClass="GameStats"
	bEnableStatLogging=false
	AccessControlClass="Engine.AccessControl"
	PlayerControllerClassName="Engine.PlayerController"
	GameMessageClass=class'GameMessage'
	GameReplicationInfoClass=class'GameReplicationInfo'
    FearCostFalloff=+0.95
	CurrentID=1
	MaxIdleTime=+0.0
	PlayerReplicationInfoClass=Class'Engine.PlayerReplicationInfo'
}