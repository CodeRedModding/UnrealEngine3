//=============================================================================
// PlayerController
//
// PlayerControllers are used by human players to control pawns.
//
// This is a built-in Unreal class and it shouldn't be modified.
// for the change in Possess().
//=============================================================================
class PlayerController extends Controller
	config(Game)
	native
    nativereplication;


var const			Player			Player;						// Player info
var 				Camera			PlayerCamera;				// Camera associated with this Player Controller
var const class<Camera>				CameraClass;

// Player control flags

var					bool			bFrozen;					// Set when game ends or player dies to temporarily prevent player from restarting (until cleared by timer)
var					bool			bPressedJump;
var					bool			bDoubleJump;
var					bool			bUpdatePosition;
var					bool			bIsTyping;
var					bool			bFixedCamera;				// Used to fix camera in position (to view animations)
var					bool			bJumpStatus;				// Used in net games
var					bool			bUpdating;
var globalconfig	bool			bNeverSwitchOnPickup;		// If true, don't automatically switch to picked up weapon
var					bool			bZooming;
var					bool			bSetTurnRot;
var					bool			bCheatFlying;				// instantly stop in flying mode
var					bool			bZeroRoll;
var					bool			bCameraPositionLocked;
var bool	bShortConnectTimeOut;	// when true, reduces connect timeout to 15 seconds
var const bool	bPendingDestroy;		// when true, playercontroller is being destroyed
var bool bWasSpeedHack;
var const bool bWasSaturated;		// used by servers to identify saturated client connections
var globalconfig bool bDynamicNetSpeed;
var globalconfig bool bAimingHelp;
var globalconfig	bool	bLandingShake;

var float MaxResponseTime;		 // how long server will wait for client move update before setting position
var					float			WaitDelay;					// Delay time until can restart
var					pawn			AcknowledgedPawn;			// Used in net games so client can acknowledge it possessed a pawn

var					eDoubleClickDir	DoubleClickDir;				// direction of movement key double click (for special moves)

// Camera info.
var const			actor			ViewTarget;
var const			Controller		RealViewTarget;

var globalconfig	float			DesiredFOV;
var globalconfig	float			DefaultFOV;
var					float			ZoomLevel;

// Remote Pawn ViewTargets
var					rotator			TargetViewRotation;
var					float			TargetEyeHeight;
var					vector			TargetWeaponViewOffset;

var					HUD				myHUD;						// heads up display info

// Move buffering for network games.  Clients save their un-acknowledged moves in order to replay them
// when they get position updates from the server.

/** SavedMoveClass should be changed for network player move replication where different properties need to be replicated from the base engine implementation.
*/
var					class<SavedMove> SavedMoveClass;
var					SavedMove		SavedMoves;					// buffered moves pending position updates
var					SavedMove		FreeMoves;					// freed moves, available for buffering
var					SavedMove		PendingMove;
var					vector			LastAckedAccel;				// last acknowledged sent acceleration
var					float			CurrentTimeStamp;
var					float			LastUpdateTime;
var					float			ServerTimeStamp;
var					float			TimeMargin;
var					float			ClientUpdateTime;
var 	float			MaxTimeMargin;
var float LastActiveTime;		// used to kick idlers
var globalconfig float DynamicPingThreshold;

var int ClientCap;

var					float			LastPingUpdate;
var float ExactPing;
var float OldPing;
var float LastSpeedHackLog;

// ClientAdjustPosition replication (event called at end of frame)
struct native ClientAdjustment
{
    var float TimeStamp;
    var name newState;
    var EPhysics newPhysics;
    var vector NewLoc;
    var vector NewVel;
    var actor NewBase;
    var vector NewFloor;
	var byte bAckGoodMove;
};
var ClientAdjustment PendingAdjustment;

// Progess Indicator - used by the engine to provide status messages (HUD is responsible for displaying these).
var					string			ProgressMessage[4];
var					color			ProgressColor[4];
var					float			ProgressTimeOut;

// Localized strings
var localized		string			QuickSaveString;
var localized		string			NoPauseMessage;
var localized		string			ViewingFrom;
var localized		string			OwnCamera;

// Stats Logging
var globalconfig	string			StatsUsername;
var globalconfig	string			StatsPassword;

var					class<LocalMessage> LocalMessageClass;

// view shaking (affects roll, and offsets camera position)
var					float			MaxShakeRoll;			// max magnitude to roll camera
var					vector			MaxShakeOffset;			// max magnitude to offset camera position
var					float			ShakeRollRate;			// rate to change roll
var					vector			ShakeOffsetRate;
var					vector			ShakeOffset;			//current magnitude to offset camera from shake
var					float			ShakeRollTime;			// how long to roll.  if value is < 1.0, then MaxShakeOffset gets damped by this, else if > 1 then its the number of times to repeat undamped
var					vector			ShakeOffsetTime;

var					Pawn			TurnTarget;
var	config			int				EnemyTurnSpeed;
var					int				GroundPitch;
var					rotator			TurnRot180;

var					vector			OldFloor;				// used by PlayerSpider mode - floor for which old rotation was based;

// Components ( inner classes )
var transient	CheatManager			CheatManager;		// Object within playercontroller that manages "cheat" commands
var						class<CheatManager>		CheatClass;			// class of my CheatManager

var()	transient	editinline	PlayerInput			PlayerInput;		// Object within playercontroller that manages player input.
var config				class<PlayerInput>		InputClass;			// class of my PlayerInput
var config				class<Console>			ConsoleClass;

var const				vector					FailedPathStart;
var						CylinderComponent		CylinderComponent;
// Manages gamepad rumble (within)
var config class<ForceFeedbackManager> ForceFeedbackManagerClass;
var transient ForceFeedbackManager ForceFeedbackManager;

// Interactions.
var						array<interaction>		Interactions;
var						Console					Console;

/** Toggle control for player movement, checked by PlayerInput */
var bool bMovementInputEnabled;

/** Toggles control for player turning, checked by PlayerInput */
var bool bTurningInputEnabled;

/** Maximum distance to search for interactable actors */
var config float InteractDistance;


// PLAYER INPUT MATCHING =============================================================

/** Type of inputs the matching code recognizes */
enum EInputTypes
{
	IT_XAxis,
	IT_YAxis,
};

/** How to match an input action */
enum EInputMatchAction
{
	IMA_GreaterThan,
	IMA_LessThan
};

/** Individual entry to input matching sequences */
struct native InputEntry
{
	/** Type of input to match */
	var EInputTypes Type;

	/** Min value required to consider as a valid match */
	var	float Value;

	/** Max amount of time since last match before sequence resets */
	var	float TimeDelta;

	/** What type of match is this? */
	var EInputMatchAction Action;
};

/**
 * Contains information to match a series of a inputs and call the given
 * function upon a match.  Processed by PlayerInput, defined in the
 * PlayerController.
 */
struct native InputMatchRequest
{
	/** Number of inputs to match, in sequence */
	var array<InputEntry> Inputs;

	/** Actor to call below functions on */
	var Actor MatchActor;

	/** Name of function to call upon successful match */
	var Name MatchFuncName;

	/** Name of function to call upon a failed partial match */
	var Name FailedFuncName;

	/** Name of this input request, mainly for debugging */
	var Name RequestName;

	/** Current index into Inputs that is being matched */
	var transient int MatchIdx;

	/** Last time an input entry in Inputs was matched */
	var transient float LastMatchTime;
};
var array<InputMatchRequest> InputRequests;


// MISC VARIABLES ====================================================================

var input byte bRun, bDuck;

var float LastBroadcastTime;
var string LastBroadcastString[4];

cpptext
{
	//  Player Pawn interface.
	void SetPlayer( UPlayer* Player );

	// AActor interface.
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
	UBOOL Tick( FLOAT DeltaTime, enum ELevelTick TickType );
	UBOOL IsNetRelevantFor( APlayerController* RealViewer, AActor* Viewer, FVector SrcLocation );
	virtual UBOOL LocalPlayerController();
	virtual UBOOL WantsLedgeCheck();
	virtual UBOOL StopAtLedge();
	virtual AActor* GetViewTarget();
	virtual APlayerController* GetAPlayerController() { return this; }
	void SetViewTarget(AActor* NewViewTarget);

    virtual UBOOL IsAPlayerController() { return true; }
	virtual void PostScriptDestroyed();
}

replication
{
	// Things the server should send to the client.
	reliable if ( bNetOwner && Role==ROLE_Authority && (ViewTarget != Pawn) && (Pawn(ViewTarget) != None) )
        TargetViewRotation, TargetEyeHeight;
	
	// Functions server can call.
	reliable if( Role==ROLE_Authority )
		ClientSetHUD, FOV, StartZoom,
		ToggleZoom, StopZoom, EndZoom, ClientRestart, ClientReset,
		ClientSetCameraMode, ClearProgressMessages,
        SetProgressMessage, SetProgressTime,
		GivePawn, ClientGotoState,
		ClientValidate,
        ClientSetViewTarget,
		ClientMessage, TeamMessage, ReceiveLocalizedMessage,
        ClientTravel, ClientCapBandwidth;

	unreliable if( Role==ROLE_Authority )
        SetFOVAngle,
		ClientAckGoodMove, ClientAdjustPosition, ShortClientAdjustPosition, VeryShortClientAdjustPosition, LongClientAdjustPosition,
		ClientPlayForceFeedbackWaveform, ClientStopForceFeedbackWaveform, ClientPlaySound;

	// Functions client can call.
	unreliable if( Role<ROLE_Authority )
        ServerUpdatePing, ServerMove,ServerSay, ServerTeamSay, 
		ServerViewNextPlayer, ServerViewSelf, ServerUse, ServerDrive,
		DualServerMove, OldServerMove;

	reliable if( Role<ROLE_Authority )
		ServerShortTimeout, Camera,
        ServerSpeech, ServerPause, SetPause, ServerMutate, ServerAcknowledgePossession,
        ServerReStartGame, AskForPawn,
        ChangeName, ServerChangeTeam, Suicide,
        ServerThrowWeapon, Typing,
		ServerValidationResponse, ServerVerifyViewTarget;

}

native final function SetNetSpeed(int NewSpeed);
native final function string GetPlayerNetworkAddress();
native final function string GetServerNetworkAddress();
native function string ConsoleCommand( string Command, optional bool bWriteToLog );
native final function LevelInfo GetEntryLevel();
native event ClientTravel( string URL, ETravelType TravelType, bool bItems );
native(546) final function UpdateURL(string NewOption, string NewValue, bool bSaveDefault);
native final function string GetDefaultURL(string Option);
// Execute a console command in the context of this player, then forward to Actor.ConsoleCommand.
native function CopyToClipboard( string Text );
native function string PasteFromClipboard();
native final function SetViewTarget(Actor NewViewTarget);

// Validation.
private native event ClientValidate(string C);
private native event ServerValidationResponse(string R);

native final function bool CheckSpeedHack(float DeltaTime);

/* FindStairRotation()
returns an integer to use as a pitch to orient player view along current ground (flat, up, or down)
*/
native(524) final function int FindStairRotation(float DeltaTime);

simulated event PostBeginPlay()
{
	super.PostBeginPlay();

	ResetCameraMode();
	if ( CameraClass != None )
	{
		// Associate Camera with PlayerController
		PlayerCamera = Spawn(CameraClass, Self);
		if ( PlayerCamera != None )
			PlayerCamera.InitializeFor( Self );
		else
			log("Couldn't Spawn Camera Actor for Player!!");
	}

	MaxTimeMargin = Level.MaxTimeMargin;
	MaxResponseTime = Default.MaxResponseTime * Level.TimeDilation;
	
	if ( Level.NetMode == NM_Client )
	{
		SpawnDefaultHUD();
	}
	DesiredFOV = DefaultFOV;
	SetViewTarget(self);  // MUST have a view target!
	LastActiveTime = Level.TimeSeconds;

	if ( Level.NetMode == NM_Standalone ) //FIXMESTEVE- and not in SP match
		AddCheats();
}

event PreRender(Canvas Canvas);

function ResetTimeMargin()
{
    TimeMargin = -0.1;
	MaxTimeMargin = Level.MaxTimeMargin;
}

function ServerShortTimeout()
{
	local Actor A;

	bShortConnectTimeOut = true;
	ResetTimeMargin();

	// quick update of pickups and gameobjectives since this player is now relevant
	if ( Level.Game.NumPlayers < 8 )
	{
		ForEach AllActors(class'Actor', A)
			if ( (A.NetUpdateFrequency < 1) && !A.bOnlyRelevantToOwner )
				A.NetUpdateTime = FMin(A.NetUpdateTime, Level.TimeSeconds + 0.2 * FRand());
}
	else
	{
		ForEach AllActors(class'Actor', A)
			if ( (A.NetUpdateFrequency < 1) && !A.bOnlyRelevantToOwner )
				A.NetUpdateTime = FMin(A.NetUpdateTime, Level.TimeSeconds + 0.5 * FRand());
	}
}

function ServerGivePawn()
{
	GivePawn(Pawn);
}

event KickWarning()
{
	ReceiveLocalizedMessage( class'GameMessage', 15 );
}

function AddCheats()
{
	// Assuming that this never gets called for NM_Client
	if ( CheatManager == None && (Level.NetMode == NM_Standalone) ) 
		CheatManager = new(Self) CheatClass;
}

exec function EnableCheats()
{
	AddCheats();
}

/* SpawnDefaultHUD()
Spawn a HUD (make sure that PlayerController always has valid HUD, even if \
ClientSetHUD() hasn't been called\
*/
function SpawnDefaultHUD()
{
	if ( LocalPlayer(Player) == None )
		return;
	myHUD = spawn(class'HUD',self);
}

/* Reset()
reset actor to initial state - used when restarting level without reloading.
*/
function Reset()
{
	local vehicle DrivenVehicle;

    DrivenVehicle = Vehicle(Pawn);
	if( DrivenVehicle != None )
		DrivenVehicle.DriverLeave(true); // Force the driver out of the car

	if ( Pawn != None )
	{
		PawnDied( Pawn );
		UnPossess();
	}

	super.Reset();

	SetViewTarget( Self );
    ResetCameraMode();
	WaitDelay = Level.TimeSeconds + 2;
    FixFOV();
    if ( PlayerReplicationInfo.bOnlySpectator )
    	GotoState('Spectating');
    else
		GotoState('PlayerWaiting');
}

event ClientReset()
{
	ResetCameraMode();
	SetViewTarget( Self );

    if ( PlayerReplicationInfo.bOnlySpectator )
    	GotoState('Spectating');
    else
		GotoState('PlayerWaiting');
}

function CleanOutSavedMoves()
{
    local SavedMove Next;

	// clean out saved moves
	while ( SavedMoves != None )
	{
		Next = SavedMoves.NextMove;
		SavedMoves.Destroy();
		SavedMoves = Next;
	}
	if ( PendingMove != None )
	{
		PendingMove.Destroy();
		PendingMove = None;
	}
}

/* InitInputSystem()
Spawn the appropriate class of PlayerInput
Only called for playercontrollers that belong to local players
*/
event InitInputSystem()
{
	PlayerInput = new(Self) InputClass;
	Console = new(none) ConsoleClass;

	PlayerInput.Player = Self;
	Console.Player = Self;

	Interactions[Interactions.Length] = Console;
	Interactions[Interactions.Length] = PlayerInput;

	// Spawn the waveform manager here
	if (ForceFeedbackManagerClass != None)
	{
		ForceFeedbackManager = new(Self) ForceFeedbackManagerClass;
	}
}

/**
 * Scales the amount the rumble will play on the gamepad
 *
 * @param ScaleBy The amount to scale the waveforms by
 */
final function SetRumbleScale(float ScaleBy)
{
	if (ForceFeedbackManager != None)
	{
		ForceFeedbackManager.ScaleAllWaveformsBy = ScaleBy;
	}
}

/**
 * Returns the rumble scale (or 1 if none is specified)
 */
final function float GetRumbleScale()
{
	local float retval;
	retval = 1.0;
	if (ForceFeedbackManager != None)
	{
		retval = ForceFeedbackManager.ScaleAllWaveformsBy;
	}
	return retval;
}

/* ClientGotoState()
server uses this to force client into NewState
*/
function ClientGotoState(name NewState, name NewLabel)
{
	GotoState(NewState,NewLabel);
}

function AskForPawn()
{
	if ( GamePlayEndedState() )
		ClientGotoState(GetStateName(), 'Begin');
	else if ( Pawn != None )
		GivePawn(Pawn);
	else
	{
		bFrozen = false;
		ServerRestartPlayer();
	}
}

function GivePawn(Pawn NewPawn)
{
	if ( NewPawn == None )
		return;
	Pawn = NewPawn;
	NewPawn.Controller = self;
	ClientRestart(Pawn);
}

// Possess a pawn
function Possess(Pawn aPawn)
{
    if ( PlayerReplicationInfo.bOnlySpectator )
		return;

	SetRotation(aPawn.Rotation);
	aPawn.PossessedBy(self);
	Pawn = aPawn;
	Pawn.bStasis = false;
	ResetTimeMargin();
    CleanOutSavedMoves();  // don't replay moves previous to possession
	if ( Vehicle(Pawn) != None && Vehicle(Pawn).Driver != None )
		PlayerReplicationInfo.bIsFemale = Vehicle(Pawn).Driver.bIsFemale;
	else
		PlayerReplicationInfo.bIsFemale = Pawn.bIsFemale;
	Restart();
}

function AcknowledgePossession(Pawn P)
{
	if ( LocalPlayer(Player) != None )
	{
		AcknowledgedPawn = P;
/* FIXMESTEVE
		if ( P != None )
			P.SetBaseEyeHeight();
*/
		ServerAcknowledgePossession(P);
	}
}

function ServerAcknowledgePossession(Pawn P)
{
	ResetTimeMargin();
    AcknowledgedPawn = P;
}

// unpossessed a pawn (not because pawn was killed)
function UnPossess()
{
	if ( Pawn != None )
	{
		SetLocation(Pawn.Location);
		Pawn.RemoteRole = ROLE_SimulatedProxy;
		Pawn.UnPossessed();
		CleanOutSavedMoves();  // don't replay moves previous to unpossession
		if ( Viewtarget == Pawn )
			SetViewTarget(self);
	}
	Pawn = None;
}

// unpossessed a pawn (because pawn was killed)
function PawnDied(Pawn P)
{
	if ( P != Pawn )
		return;

	EndZoom();

	if ( Pawn != None )
		Pawn.RemoteRole = ROLE_SimulatedProxy;

    super.PawnDied( P );
}

function ClientSetHUD(class<HUD> newHUDType, class<Scoreboard> newScoringType)
{
    if ( myHUD != None )
        myHUD.Destroy();

    if (newHUDType == None)
        myHUD = None;
    else
    {
        myHUD = spawn (newHUDType, self);
		if ( (myHUD != None) && (newScoringType != None) )
			MyHUD.SpawnScoreBoard(newScoringType);
	}
}

function HandlePickup(Inventory Inv)
{
	ReceiveLocalizedMessage(Inv.MessageClass,,,,Inv.class);
}

event ReceiveLocalizedMessage( class<LocalMessage> Message, optional int Switch, optional PlayerReplicationInfo RelatedPRI_1, optional PlayerReplicationInfo RelatedPRI_2, optional Object OptionalObject )
{
	// Wait for player to be up to date with replication when joining a server, before stacking up messages
	if ( Level.NetMode == NM_DedicatedServer || Level.GRI == None )
		return;

	Message.Static.ClientReceive( Self, Switch, RelatedPRI_1, RelatedPRI_2, OptionalObject );
}

//Play a sound client side (so only client will hear it
function ClientPlaySound(SoundCue ASound)
{
	PlaySound(ASound);
}

event ClientMessage( coerce string S, optional Name Type )
{
	if ( Level.NetMode == NM_DedicatedServer || Level.GRI == None )
		return;

	if (Type == '')
		Type = 'Event';

	TeamMessage(PlayerReplicationInfo, S, Type);
}

event TeamMessage( PlayerReplicationInfo PRI, coerce string S, name Type  )
{
	if ( myHUD != None )
	myHUD.Message( PRI, S, Type );

    if ( ((Type == 'Say') || (Type == 'TeamSay')) && (PRI != None) )
		S = PRI.PlayerName$": "$S;

    Console.Message( S, 6.0 );
}

simulated function PlayBeepSound();

simulated event Destroyed()
{
	local SavedMove Next;
    local Vehicle	DrivenVehicle;
	local Pawn		Driver;

	// cheatmanager and playerinput cleaned up in C++ PostScriptDestroyed()

	if ( Pawn != None )
	{
		// If its a vehicle, just destroy the driver, otherwise do the normal.
		DrivenVehicle = Vehicle(Pawn);
		if ( DrivenVehicle != None )
		{
			Driver = DrivenVehicle.Driver;
			DrivenVehicle.DriverLeave( true ); // Force the driver out of the car
			if ( Driver != None )
			{
				Driver.Health = 0;
				Driver.Died( self, class'DmgType_Suicided', Driver.Location );
			}
		}
		else
		{
			Pawn.Health = 0;
			Pawn.Died( self, class'DmgType_Suicided', Pawn.Location );
		}
	}
    if ( myHUD != None )
	myHud.Destroy();

	while ( FreeMoves != None )
	{
		Next = FreeMoves.NextMove;
		FreeMoves.Destroy();
		FreeMoves = Next;
	}
	while ( SavedMoves != None )
	{
		Next = SavedMoves.NextMove;
		SavedMoves.Destroy();
		SavedMoves = Next;
	}

	if( PlayerCamera != None )
	{
		PlayerCamera.Destroy();
		PlayerCamera = None;
	}

    super.Destroyed();
}

// ------------------------------------------------------------------------
// Zooming/FOV change functions

function ToggleZoom()
{
	if ( DefaultFOV != DesiredFOV )
		EndZoom();
	else
		StartZoom();
}

function StartZoom()
{
	ZoomLevel = 0.0;
	bZooming = true;
}

function StopZoom()
{
	bZooming = false;
}

function EndZoom()
{
	bZooming = false;
	DesiredFOV = DefaultFOV;
}

function FixFOV()
{
	FOVAngle = Default.DefaultFOV;
	DesiredFOV = Default.DefaultFOV;
	DefaultFOV = Default.DefaultFOV;
}

function SetFOV(float NewFOV)
{
	DesiredFOV = NewFOV;
	FOVAngle = NewFOV;
}

function ResetFOV()
{
	DesiredFOV = DefaultFOV;
	FOVAngle = DefaultFOV;
}

exec function FOV(float F)
{
	if( PlayerCamera != None )
	{
		PlayerCamera.SetFOV( F );
		return;
	}

    if( (F >= 80.0) || (Level.Netmode==NM_Standalone) || PlayerReplicationInfo.bOnlySpectator )
	{
		DefaultFOV = FClamp(F, 1, 170);
		DesiredFOV = DefaultFOV;
		SaveConfig();
	}
}

exec function Mutate(string MutateString)
{
	ServerMutate(MutateString);
}

function ServerMutate(string MutateString)
{
	if( Level.NetMode == NM_Client )
		return;
	Level.Game.Mutate(MutateString, Self);
}

// ------------------------------------------------------------------------
// Messaging functions

function bool AllowTextMessage(string Msg)
{
	local int i;

	if ( (Level.NetMode == NM_Standalone) || PlayerReplicationInfo.bAdmin )
		return true;
	if ( ( Level.Pauser == none) && (Level.TimeSeconds - LastBroadcastTime < 2 ) )
		return false;

	// lower frequency if same text
	if ( Level.TimeSeconds - LastBroadcastTime < 5 )
	{
		Msg = Left(Msg,Clamp(len(Msg) - 4, 8, 64));
		for ( i=0; i<4; i++ )
			if ( LastBroadcastString[i] ~= Msg )
				return false;
	}
	for ( i=3; i>0; i-- )
		LastBroadcastString[i] = LastBroadcastString[i-1];

	LastBroadcastTime = Level.TimeSeconds;
	return true;
}

// Send a message to all players.
exec function Say( string Msg )
{
	Msg = Left(Msg,128);

	if ( AllowTextMessage(Msg) )
		ServerSay(Msg);
}

function ServerSay( string Msg )
{
	local controller C;

//FIXMESTEVE	Msg = Level.Game.StripColor(Msg);

	// center print admin messages which start with #
	if (PlayerReplicationInfo.bAdmin && left(Msg,1) == "#" )
	{
		Msg = right(Msg,len(Msg)-1);
		for( C=Level.ControllerList; C!=None; C=C.nextController )
			if( C.IsA('PlayerController') )
			{
				PlayerController(C).ClearProgressMessages();
				PlayerController(C).SetProgressTime(6);
				PlayerController(C).SetProgressMessage(0, Msg, MakeColor(255,255,255,255));
			}
		return;
	}

	Level.Game.Broadcast(self, Msg, 'Say');
}

exec function TeamSay( string Msg )
{
	Msg = Left(Msg,128);
	if ( AllowTextMessage(Msg) )
		ServerTeamSay(Msg);
}

function ServerTeamSay( string Msg )
{
	LastActiveTime = Level.TimeSeconds;

	//FIXMESTEVE Msg = Level.Game.StripColor(Msg);

	if( !Level.GRI.bTeamGame )
	{
		Say( Msg );
		return;
	}

    Level.Game.BroadcastTeam( self, Level.Game.ParseMessageString( self, Msg ) , 'TeamSay');
}

// ------------------------------------------------------------------------

function bool IsDead()
{
	return false;
}

event PreClientTravel();

function ClientSetFixedCamera(bool B)
{
	bFixedCamera = B;
}

/**
 * Change Camera mode
 *
 * @param	New camera mode to set
 */
exec function Camera( name NewMode )
{
	if ( NewMode == '1st' )
	{
    	NewMode = 'FirstPerson';
	}
    else if ( NewMode == '3rd' )
	{
    	NewMode = 'ThirdPerson';
	}

	SetCameraMode( NewMode );
    log("#### " $ PlayerCamera.CameraStyle);
}

/**
 * Replicated function to set camera style on client
 *
 * @param	NewCamMode, name defining the new camera mode
 */
function ClientSetCameraMode( name NewCamMode )
{
	if ( PlayerCamera != None )
		PlayerCamera.CameraStyle = NewCamMode;
}

/**
 * Set new camera mode
 *
 * @param	NewCamMode, new camera mode.
 */
function SetCameraMode( name NewCamMode )
{
	if ( PlayerCamera != None )
	{
		PlayerCamera.CameraStyle = NewCamMode;
		if ( Level.NetMode == NM_DedicatedServer )
		{
			ClientSetCameraMode( NewCamMode );
		}
	}
}

/**
 * Reset Camera Mode to default
 */
event ResetCameraMode()
{
	if ( Pawn != None )
	{	// If we have a pawn, let's ask it which camera mode we should use
		SetCameraMode( Pawn.GetDefaultCameraMode( Self ) );
	}
	else
	{	// otherwise default to first person view.
		SetCameraMode( 'FirstPerson' );
	}
}

function ClientVoiceMessage(PlayerReplicationInfo Sender, PlayerReplicationInfo Recipient, name messagetype, byte messageID);

/* ForceDeathUpdate()
Make sure ClientAdjustPosition immediately informs client of pawn's death
*/
function ForceDeathUpdate()
{
	LastUpdateTime = Level.TimeSeconds - 10;
}

/* DualServerMove()
- replicated function sent by client to server - contains client movement and firing info for two moves
*/
function DualServerMove
(
	float TimeStamp0,
	vector InAccel0,
	byte PendingFlags,
	int View0,
    float TimeStamp,
    vector InAccel,
    vector ClientLoc,
    byte NewFlags,
    byte ClientRoll,
    int View
)
{
    ServerMove(TimeStamp0,InAccel0,vect(0,0,0),PendingFlags,ClientRoll,View0);
	if ( ClientLoc == vect(0,0,0) )
		ClientLoc = vect(0.1,0,0);
    ServerMove(TimeStamp,InAccel,ClientLoc,NewFlags,ClientRoll,View);
}

/* OldServerMove()
- resending an (important) old move.  Process it if not already processed.
FIXMESTEVE - perhaps don't compress so much?
*/
function OldServerMove
(
	float OldTimeStamp,
    byte OldAccelX,
    byte OldAccelY,
    byte OldAccelZ,
    byte OldMoveFlags
)
{
	local vector Accel;

	if ( AcknowledgedPawn != Pawn )
		return;

	if ( CurrentTimeStamp < OldTimeStamp - 0.001 )
	{
		// split out components of lost move (approx)
		Accel.X = OldAccelX;
		if ( Accel.X > 127 )
			Accel.X = -1 * (Accel.X - 128);
		Accel.Y = OldAccelY;
		if ( Accel.Y > 127 )
			Accel.Y = -1 * (Accel.Y - 128);
		Accel.Z = OldAccelZ;
		if ( Accel.Z > 127 )
			Accel.Z = -1 * (Accel.Z - 128);
		Accel *= 20;

		//log("Recovered move from "$OldTimeStamp$" acceleration "$Accel$" from "$OldAccel);
        OldTimeStamp = FMin(OldTimeStamp, CurrentTimeStamp + MaxResponseTime);
        MoveAutonomous(OldTimeStamp - CurrentTimeStamp, OldMoveFlags, Accel, rot(0,0,0));
		CurrentTimeStamp = OldTimeStamp;
	}
}

/* ServerMove()
- replicated function sent by client to server - contains client movement and firing info.
*/
function ServerMove
(
	float TimeStamp,
	vector InAccel,
	vector ClientLoc,
	byte MoveFlags,
	byte ClientRoll,
	int View
)
{
	local float DeltaTime, clientErr;
	local rotator DeltaRot, Rot, ViewRot;
    local vector Accel, LocDiff;
	local int maxPitch, ViewPitch, ViewYaw;
    local bool NewbPressedJump;

	// If this move is outdated, discard it.
	if ( CurrentTimeStamp >= TimeStamp )
		return;

	if ( AcknowledgedPawn != Pawn )
	{
		InAccel = vect(0,0,0);
		GivePawn(Pawn);
	}

	// View components
	ViewPitch = View/32768;
	ViewYaw = 2 * (View - 32768 * ViewPitch);
	ViewPitch *= 2;
	// Make acceleration.
    Accel = InAccel * 0.1;

	// Save move parameters.
    DeltaTime = FMin(MaxResponseTime,TimeStamp - CurrentTimeStamp);

	if ( Pawn == None )
	{
		bWasSpeedHack = false;
		ResetTimeMargin();
	}
	else if ( !CheckSpeedHack(DeltaTime) )
	{
		if ( !bWasSpeedHack )
		{
			if ( Level.TimeSeconds - LastSpeedHackLog > 20 )
		{
				log("Possible speed hack by "$PlayerReplicationInfo.PlayerName);
				LastSpeedHackLog = Level.TimeSeconds;
			}
			ClientMessage( "Speed Hack Detected!",'CriticalEvent' );
		}
			else
			bWasSpeedHack = true;
			DeltaTime = 0;
		Pawn.Velocity = vect(0,0,0);
		}
	else
		bWasSpeedHack = false;

	CurrentTimeStamp = TimeStamp;
	ServerTimeStamp = Level.TimeSeconds;
	ViewRot.Pitch = ViewPitch;
	ViewRot.Yaw = ViewYaw;
	ViewRot.Roll = 0;

    if ( NewbPressedJump || (InAccel != vect(0,0,0)) )
		LastActiveTime = Level.TimeSeconds;

	SetRotation(ViewRot);

	if ( AcknowledgedPawn != Pawn )
		return;

    if ( Pawn != None )
	{
		Rot.Roll = 256 * ClientRoll;
		Rot.Yaw = ViewYaw;
		if ( (Pawn.Physics == PHYS_Swimming) || (Pawn.Physics == PHYS_Flying) )
			maxPitch = 2;
		else
            maxPitch = 0;
		if ( (ViewPitch > maxPitch * RotationRate.Pitch) && (ViewPitch < 65536 - maxPitch * RotationRate.Pitch) )
		{
			if (ViewPitch < 32768)
				Rot.Pitch = maxPitch * RotationRate.Pitch;
			else
				Rot.Pitch = 65536 - maxPitch * RotationRate.Pitch;
		}
		else
			Rot.Pitch = ViewPitch;
		DeltaRot = (Rotation - Rot);
		Pawn.SetRotation(Rot);
	}

    // Perform actual movement
	if ( (Level.Pauser == None) && (DeltaTime > 0) )
        MoveAutonomous(DeltaTime, MoveFlags, Accel, DeltaRot);

	// Accumulate movement error.
    if ( ClientLoc == vect(0,0,0) )
		return;		// first part of double servermove
	else if ( Level.TimeSeconds - LastUpdateTime < 180.0/Player.CurrentNetSpeed )
		return;

	if ( Pawn == None )
		LocDiff = Location - ClientLoc;
	else
		LocDiff = Pawn.Location - ClientLoc;
	ClientErr = LocDiff Dot LocDiff;

	// If client has accumulated a noticeable positional error, correct him.
	if ( ClientErr > 3 )
	{
		if ( Pawn == None )
		{
            PendingAdjustment.newPhysics = Physics;
            PendingAdjustment.NewLoc = Location;
            PendingAdjustment.NewVel = Velocity;
		}
		else
		{
            PendingAdjustment.newPhysics = Pawn.Physics;
            PendingAdjustment.NewVel = Pawn.Velocity;
            PendingAdjustment.NewBase = Pawn.Base;
            if ( /*FIXMESTEVE(Mover(Pawn.Base) != None) ||*/ (Vehicle(Pawn.Base) != None) )
                PendingAdjustment.NewLoc = Pawn.Location - Pawn.Base.Location;
			else
                PendingAdjustment.NewLoc = Pawn.Location;
            PendingAdjustment.NewFloor = Pawn.Floor;
		}
		//log("Client Error at "$TimeStamp$" is "$ClientErr$" with acceleration "$Accel$" LocDiff "$LocDiff$" Physics "$Pawn.Physics);
		LastUpdateTime = Level.TimeSeconds;

		PendingAdjustment.TimeStamp = TimeStamp;
		PendingAdjustment.bAckGoodMove = 0;
		PendingAdjustment.newState = GetStateName();
    }
	else 
	{
		// acknowledge receipt of this successful servermove()
		PendingAdjustment.TimeStamp = TimeStamp;
		PendingAdjustment.bAckGoodMove = 1;
	}
	//log("Server moved stamp "$TimeStamp$" location "$Pawn.Location$" Acceleration "$Pawn.Acceleration$" Velocity "$Pawn.Velocity);
}

/* Called on server at end of tick when PendingAdjustment has been set.
Done this way to avoid ever sending more than one ClientAdjustment per server tick.
*/
event SendClientAdjustment()
{
	if ( AcknowledgedPawn != Pawn )
	{
		PendingAdjustment.TimeStamp = 0;
		return;
	}

	if ( PendingAdjustment.bAckGoodMove == 1 )
	{
		// just notify client this move was received
		ClientAckGoodMove(PendingAdjustment.TimeStamp);
	}
	else if ( (Pawn == None) || (Pawn.Physics != PHYS_Spider) )
	{
		if ( PendingAdjustment.NewVel == vect(0,0,0) )
		{
			ShortClientAdjustPosition
			(
			PendingAdjustment.TimeStamp,
            PendingAdjustment.newState,
            PendingAdjustment.newPhysics,
            PendingAdjustment.NewLoc.X,
            PendingAdjustment.NewLoc.Y,
            PendingAdjustment.NewLoc.Z,
            PendingAdjustment.NewBase
			);
		}
		else
		{
			ClientAdjustPosition
			(
            PendingAdjustment.TimeStamp,
            PendingAdjustment.newState,
            PendingAdjustment.newPhysics,
            PendingAdjustment.NewLoc.X,
            PendingAdjustment.NewLoc.Y,
            PendingAdjustment.NewLoc.Z,
            PendingAdjustment.NewVel.X,
            PendingAdjustment.NewVel.Y,
            PendingAdjustment.NewVel.Z,
            PendingAdjustment.NewBase
			);
	}
    }
	else
	{
		LongClientAdjustPosition
		(
        PendingAdjustment.TimeStamp,
        PendingAdjustment.newState,
        PendingAdjustment.newPhysics,
        PendingAdjustment.NewLoc.X,
        PendingAdjustment.NewLoc.Y,
        PendingAdjustment.NewLoc.Z,
        PendingAdjustment.NewVel.X,
        PendingAdjustment.NewVel.Y,
        PendingAdjustment.NewVel.Z,
        PendingAdjustment.NewBase,
        PendingAdjustment.NewFloor.X,
        PendingAdjustment.NewFloor.Y,
        PendingAdjustment.NewFloor.Z
		);
	}

	PendingAdjustment.TimeStamp = 0;
	PendingAdjustment.bAckGoodMove = 0;
}

// Only executed on server
function ServerDrive(float InForward, float InStrafe, float aUp, bool InJump, int View)
{
    local rotator ViewRotation;

    // Added to handle setting of the correct ViewRotation on the server in network games --Dave@Psyonix
    ViewRotation.Pitch = View/32768;
    ViewRotation.Yaw = 2 * (View - 32768 * ViewRotation.Pitch);
    ViewRotation.Pitch *= 2;
    ViewRotation.Roll = 0;
    SetRotation(ViewRotation);

    ProcessDrive(InForward, InStrafe, aUp, InJump);
}

function ProcessDrive(float InForward, float InStrafe, float InUp, bool InJump)
{
	ClientGotoState(GetStateName(), 'Begin');
}

function ProcessMove ( float DeltaTime, vector newAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
{
    if ( (Pawn != None) && (Pawn.Acceleration != newAccel) )
		Pawn.Acceleration = newAccel;
}

function MoveAutonomous
(
	float DeltaTime,
	byte CompressedFlags,
	vector newAccel,
	rotator DeltaRot
)
{
	local EDoubleClickDir DoubleClickMove;

	if ( (Pawn != None) && Pawn.bHardAttach )
		return;

	DoubleClickMove = SavedMoveClass.static.SetFlags(CompressedFlags, self);
	HandleWalking();
	ProcessMove(DeltaTime, newAccel, DoubleClickMove, DeltaRot);
	if ( Pawn != None )
		Pawn.AutonomousPhysics(DeltaTime);
	else
		AutonomousPhysics(DeltaTime);
    bDoubleJump = false;
	//log("Role "$Role$" moveauto time "$100 * DeltaTime$" ("$Level.TimeDilation$")");
}

/* VeryShortClientAdjustPosition
bandwidth saving version, when velocity is zeroed, and pawn is walking
*/
function VeryShortClientAdjustPosition
(
	float TimeStamp,
	float NewLocX,
	float NewLocY,
	float NewLocZ,
	Actor NewBase
)
{
	local vector Floor;

	if ( Pawn != None )
		Floor = Pawn.Floor;
	LongClientAdjustPosition(TimeStamp,'PlayerWalking',PHYS_Walking,NewLocX,NewLocY,NewLocZ,0,0,0,NewBase,Floor.X,Floor.Y,Floor.Z);
}

/* ShortClientAdjustPosition
bandwidth saving version, when velocity is zeroed
*/
function ShortClientAdjustPosition
(
	float TimeStamp,
	name newState,
	EPhysics newPhysics,
	float NewLocX,
	float NewLocY,
	float NewLocZ,
	Actor NewBase
)
{
	local vector Floor;

	if ( Pawn != None )
		Floor = Pawn.Floor;
	LongClientAdjustPosition(TimeStamp,newState,newPhysics,NewLocX,NewLocY,NewLocZ,0,0,0,NewBase,Floor.X,Floor.Y,Floor.Z);
}

function ClientCapBandwidth(int Cap)
{
	ClientCap = Cap;
	if ( (Player != None) && (Player.CurrentNetSpeed > Cap) )
		SetNetSpeed(Cap);
}

function ClientAckGoodMove(float TimeStamp)
{
	CurrentTimeStamp = TimeStamp;
	ClearAckedMoves();
}

/* ClientAdjustPosition
- pass newloc and newvel in components so they don't get rounded
*/
function ClientAdjustPosition
(
	float TimeStamp,
	name newState,
	EPhysics newPhysics,
	float NewLocX,
	float NewLocY,
	float NewLocZ,
	float NewVelX,
	float NewVelY,
	float NewVelZ,
	Actor NewBase
)
{
	local vector Floor;

	if ( Pawn != None )
		Floor = Pawn.Floor;
	LongClientAdjustPosition(TimeStamp,newState,newPhysics,NewLocX,NewLocY,NewLocZ,NewVelX,NewVelY,NewVelZ,NewBase,Floor.X,Floor.Y,Floor.Z);
}

/* LongClientAdjustPosition
long version, when care about pawn's floor normal
*/
function LongClientAdjustPosition
(
	float TimeStamp,
	name newState,
	EPhysics newPhysics,
	float NewLocX,
	float NewLocY,
	float NewLocZ,
	float NewVelX,
	float NewVelY,
	float NewVelZ,
	Actor NewBase,
	float NewFloorX,
	float NewFloorY,
	float NewFloorZ
)
{
    local vector NewLocation, NewVelocity, NewFloor;
	local Actor MoveActor;
    local SavedMove CurrentMove;
    local float NewPing;

	// update ping
	if ( PlayerReplicationInfo != None )
	{
		NewPing = FMin(1.5, Level.TimeSeconds - TimeStamp);

		if ( ExactPing < 0.004 )
			ExactPing = FMin(0.3,NewPing);
		else
		{
			if ( NewPing > 2 * ExactPing )
				NewPing = FMin(NewPing, 3*ExactPing);
			ExactPing = FMin(0.99, 0.99 * ExactPing + 0.008 * NewPing); // placebo effect
		}
		PlayerReplicationInfo.Ping = Min(250.0 * ExactPing, 255);
		PlayerReplicationInfo.bReceivedPing = true;
		if ( Level.TimeSeconds - LastPingUpdate > 4 )
		{
			if ( bDynamicNetSpeed && (OldPing > DynamicPingThreshold * 0.001) && (ExactPing > DynamicPingThreshold * 0.001) )
			{
				if ( Level.MoveRepSize < 64 )
					Level.MoveRepSize += 8;
				else if ( Player.CurrentNetSpeed >= 6000 )
					SetNetSpeed(Player.CurrentNetSpeed - 1000);
				OldPing = 0;
			}
			else
				OldPing = ExactPing;
			LastPingUpdate = Level.TimeSeconds;
			ServerUpdatePing(1000 * ExactPing);
		}
	}
	if ( Pawn != None )
	{
		if ( Pawn.bTearOff )
		{
			Pawn = None;
			if ( !GamePlayEndedState() && !IsInState('Dead') )
			{
				GotoState('Dead');
            }
			return;
		}
		MoveActor = Pawn;
        if ( (ViewTarget != Pawn)
			&& ((ViewTarget == self) || ((Pawn(ViewTarget) != None) && (Pawn(ViewTarget).Health <= 0))) )
		{
			ResetCameraMode();
			SetViewTarget(Pawn);
		}
	}
	else
    {
		MoveActor = self;
 	   	if( GetStateName() != newstate )
		{
		    if ( NewState == 'GameEnded' || NewState == 'RoundEnded' )
			    GotoState(NewState);
			else if ( IsInState('Dead') )
			{
		    	if ( (NewState != 'PlayerWalking') && (NewState != 'PlayerSwimming') )
		        {
				    GotoState(NewState);
		        }
		        return;
			}
			else if ( NewState == 'Dead' )
				GotoState(NewState);
		}
	}
    if ( CurrentTimeStamp >= TimeStamp )
		return;
	CurrentTimeStamp = TimeStamp;

	NewLocation.X = NewLocX;
	NewLocation.Y = NewLocY;
	NewLocation.Z = NewLocZ;
    NewVelocity.X = NewVelX;
    NewVelocity.Y = NewVelY;
    NewVelocity.Z = NewVelZ;

	// skip update if no error
    CurrentMove = SavedMoves;

	// note that acked moves are cleared here, instead of calling ClearAckedMoves()
    while ( CurrentMove != None )
    {
        if ( CurrentMove.TimeStamp <= CurrentTimeStamp )
        {
            SavedMoves = CurrentMove.NextMove;
            CurrentMove.NextMove = FreeMoves;
            FreeMoves = CurrentMove;
			if ( CurrentMove.TimeStamp == CurrentTimeStamp )
			{
				LastAckedAccel = CurrentMove.Acceleration;
				FreeMoves.Clear();
				if ( (/*FIXMESTEVE(Mover(NewBase) != None) || */(Vehicle(NewBase) != None))
					&& (NewBase == CurrentMove.EndBase) )
				{
					if ( (GetStateName() == NewState)
						&& IsInState('PlayerWalking')
						&& ((MoveActor.Physics == PHYS_Walking) || (MoveActor.Physics == PHYS_Falling)) )
					{
						if ( VSize(CurrentMove.SavedRelativeLocation - NewLocation) < 3 )
						{
							CurrentMove = None;
							return;
						}
						else if ( (Vehicle(NewBase) != None)
								&& (VSize(Velocity) < 3) && (VSize(NewVelocity) < 3)
								&& (VSize(CurrentMove.SavedRelativeLocation - NewLocation) < 30) )
						{
							CurrentMove = None;
							return;
						}
					}
				}
				else if ( (VSize(CurrentMove.SavedLocation - NewLocation) < 3)
					&& (VSize(CurrentMove.SavedVelocity - NewVelocity) < 3)
					&& (GetStateName() == NewState)
					&& IsInState('PlayerWalking')
					&& ((MoveActor.Physics == PHYS_Walking) || (MoveActor.Physics == PHYS_Falling)) )
				{
					CurrentMove = None;
					return;
				}
				CurrentMove = None;
			}
			else
			{
				FreeMoves.Clear();
				CurrentMove = SavedMoves;
			}
        }
        else
			CurrentMove = None;
    }
	if ( MoveActor.bHardAttach )
	{
		if ( MoveActor.Base == None )
		{
			if ( NewBase != None )
				MoveActor.SetBase(NewBase);
			if ( MoveActor.Base == None )
				MoveActor.bHardAttach = false;
			else
				return;
		}
		else
			return;
	}

	NewFloor.X = NewFloorX;
	NewFloor.Y = NewFloorY;
	NewFloor.Z = NewFloorZ;
	/*FIXMESTEVE: handle interp actors as bases
	if ( (Mover(NewBase) != None) || (Vehicle(NewBase) != None) )
		NewLocation += NewBase.Location;
	*/

	//log("Client "$Role$" adjust "$self$" stamp "$TimeStamp$" location "$MoveActor.Location);
	MoveActor.bCanTeleport = false;
    if ( !MoveActor.SetLocation(NewLocation) && (Pawn(MoveActor) != None)
		&& (Pawn(MoveActor).CylinderComponent.CollisionHeight > Pawn(MoveActor).CrouchHeight)
		&& !Pawn(MoveActor).bIsCrouched
		&& (newPhysics == PHYS_Walking)
		&& (MoveActor.Physics != PHYS_RigidBody) && (MoveActor.Physics != PHYS_Articulated) )
	{
		MoveActor.SetPhysics(newPhysics);
		Pawn(MoveActor).ForceCrouch();
		MoveActor.SetLocation(NewLocation);
	}
	MoveActor.bCanTeleport = true;

	// Hack. Don't let network change physics mode of rigid body stuff on the client.
	if( MoveActor.Physics != PHYS_RigidBody && MoveActor.Physics != PHYS_Articulated &&
		newPhysics != PHYS_RigidBody && newPhysics != PHYS_Articulated )
	{
		MoveActor.SetPhysics(newPhysics);
	}
	if ( MoveActor != self )
		MoveActor.SetBase(NewBase, NewFloor);

    MoveActor.Velocity = NewVelocity;

	if( GetStateName() != newstate )
		GotoState(newstate);

	bUpdatePosition = true;
}

function ServerUpdatePing(int NewPing)
{
	PlayerReplicationInfo.Ping = Min(0.25 * NewPing, 250);
	PlayerReplicationInfo.bReceivedPing = true;
}

/* ClearAckedMoves()
clear out acknowledged/missed sent moves
*/
function ClearAckedMoves()
{
	local SavedMove CurrentMove;

	CurrentMove = SavedMoves;
	while ( CurrentMove != None )
	{
		if ( CurrentMove.TimeStamp <= CurrentTimeStamp )
		{
			if ( CurrentMove.TimeStamp == CurrentTimeStamp )
				LastAckedAccel = CurrentMove.Acceleration;
			SavedMoves = CurrentMove.NextMove;
			CurrentMove.NextMove = FreeMoves;
			FreeMoves = CurrentMove;
			FreeMoves.Clear();
			CurrentMove = SavedMoves;
		}
		else
			break;
	}
}

function ClientUpdatePosition()
{
	local SavedMove CurrentMove;
	local int realbRun, realbDuck;
	local bool bRealJump;

	bUpdatePosition = false;

	// Dont do any network position updates on things running PHYS_RigidBody
	if( Pawn != None && (Pawn.Physics == PHYS_RigidBody || Pawn.Physics == PHYS_Articulated) )
		return;

	realbRun= bRun;
	realbDuck = bDuck;
	bRealJump = bPressedJump;
	bUpdating = true;

	ClearAckedMoves();
	CurrentMove = SavedMoves;
	while ( CurrentMove != None )
	{
		if ( (PendingMove == CurrentMove) && (Pawn != None) )
			PendingMove.SetInitialPosition(Pawn);
		assert( CurrentMove.TimeStamp > CurrentTimeStamp );
        MoveAutonomous(CurrentMove.Delta, CurrentMove.CompressedFlags(), CurrentMove.Acceleration, rot(0,0,0));
		if ( Pawn != None )
		{
			CurrentMove.SavedLocation = Pawn.Location;
			CurrentMove.SavedVelocity = Pawn.Velocity;
			CurrentMove.EndBase = Pawn.Base;
			if ( (CurrentMove.EndBase != None) && !CurrentMove.EndBase.bWorldGeometry )
				CurrentMove.SavedRelativeLocation = Pawn.Location - CurrentMove.EndBase.Location;
		}
		CurrentMove = CurrentMove.NextMove;
	}

	bUpdating = false;
	bDuck = realbDuck;
	bRun = realbRun;
	bPressedJump = bRealJump;
}

final function SavedMove GetFreeMove()
{
	local SavedMove s, first;
	local int i;

	if ( FreeMoves == None )
	{
        // don't allow more than 100 saved moves
		For ( s=SavedMoves; s!=None; s=s.NextMove )
		{
			i++;
            if ( i > 100 )
			{
				first = SavedMoves;
				SavedMoves = SavedMoves.NextMove;
				first.Clear();
				first.NextMove = None;
				// clear out all the moves
				While ( SavedMoves != None )
				{
					s = SavedMoves;
					SavedMoves = SavedMoves.NextMove;
					s.Clear();
					s.NextMove = FreeMoves;
					FreeMoves = s;
				}
				return first;
			}
		}
		return Spawn(SavedMoveClass);
	}
	else
	{
		s = FreeMoves;
		FreeMoves = FreeMoves.NextMove;
		s.NextMove = None;
		return s;
	}
}

function int CompressAccel(int C)
{
	if ( C >= 0 )
		C = Min(C, 127);
	else
		C = Min(abs(C), 127) + 128;
	return C;
}

/*
========================================================================
Here's how player movement prediction, replication and correction works in network games:

Every tick, the PlayerTick() function is called.  It calls the PlayerMove() function (which is implemented
in various states).  PlayerMove() figures out the acceleration and rotation, and then calls ProcessMove()
(for single player or listen servers), or ReplicateMove() (if its a network client).

ReplicateMove() saves the move (in the PendingMove list), calls ProcessMove(), and then replicates the move
to the server by calling the replicated function ServerMove() - passing the movement parameters, the client's
resultant position, and a timestamp.

ServerMove() is executed on the server.  It decodes the movement parameters and causes the appropriate movement
to occur.  It then looks at the resulting position and if enough time has passed since the last response, or the
position error is significant enough, the server calls ClientAdjustPosition(), a replicated function.

ClientAdjustPosition() is executed on the client.  The client sets its position to the servers version of position,
and sets the bUpdatePosition flag to true.

When PlayerTick() is called on the client again, if bUpdatePosition is true, the client will call
ClientUpdatePosition() before calling PlayerMove().  ClientUpdatePosition() replays all the moves in the pending
move list which occured after the timestamp of the move the server was adjusting.
*/

//
// Replicate this client's desired movement to the server.
//
function ReplicateMove
(
	float DeltaTime,
	vector NewAccel,
	eDoubleClickDir DoubleClickMove,
	rotator DeltaRot
)
{
    local SavedMove NewMove, OldMove, AlmostLastMove, LastMove;
	local byte ClientRoll;
	local float NetMoveDelta;
    local vector MoveLoc;
	local bool bPendingJumpStatus;

	MaxResponseTime = Default.MaxResponseTime * Level.TimeDilation;
	DeltaTime = FMin(DeltaTime, MaxResponseTime);

	// find the most recent move (LastMove), and the oldest (unacknowledged) important move (OldMove)
	// a SavedMove is interesting if it differs significantly from the last acknowledged move
	if ( SavedMoves != None )
	{
        LastMove = SavedMoves;
        AlmostLastMove = LastMove;
        while ( LastMove.NextMove != None )
		{
			// find first important unacknowledged move
			if ( (OldMove == None) && (Pawn != None) && LastMove.IsImportantMove(LastAckedAccel) )
			{
				OldMove = LastMove;
			}
            AlmostLastMove = LastMove;
            LastMove = LastMove.NextMove;
		}
    }

    // Get a SavedMove actor to store the movement in.
	NewMove = GetFreeMove();
	if ( NewMove == None )
		return;
	NewMove.SetMoveFor(self, DeltaTime, NewAccel, DoubleClickMove);

	// Simulate the movement locally.
    bDoubleJump = false;
	ProcessMove(NewMove.Delta, NewMove.Acceleration, NewMove.DoubleClickMove, DeltaRot);

	// see if the two moves could be combined
	// FIXMESTEVE- remove timedilation constraint
	if ( (PendingMove != None) && (Level.TimeDilation >= 0.9) && PendingMove.CanCombineWith(NewMove, Pawn, MaxResponseTime) )
	{
		Pawn.SetLocation(PendingMove.GetStartLocation());
		Pawn.Velocity = PendingMove.StartVelocity;
		if ( PendingMove.StartBase != Pawn.Base);
			Pawn.SetBase(PendingMove.StartBase);
		Pawn.Floor = PendingMove.StartFloor;
		NewMove.Delta += PendingMove.Delta;
		NewMove.SetInitialPosition(Pawn);

		// remove pending move from move list
		if ( LastMove == PendingMove )
		{
			if ( SavedMoves == PendingMove )
			{
				SavedMoves.NextMove = FreeMoves;
				FreeMoves = SavedMoves;
				SavedMoves = None;
			}
			else
			{
				PendingMove.NextMove = FreeMoves;
				FreeMoves = PendingMove;
				if ( AlmostLastMove != None )
				{
					AlmostLastMove.NextMove = None;
					LastMove = AlmostLastMove;
				}
			}
			FreeMoves.Clear();
		}
		PendingMove = None;
	}

    if ( Pawn != None )
        Pawn.AutonomousPhysics(NewMove.Delta);
    else
        AutonomousPhysics(DeltaTime);
    NewMove.PostUpdate(self);

	if ( SavedMoves == None )
		SavedMoves = NewMove;
	else
		LastMove.NextMove = NewMove;

	if ( PendingMove == None )
	{
		// Decide whether to hold off on move
		if ( (Player.CurrentNetSpeed > 10000) && (Level.GRI != None) && (Level.GRI.PRIArray.Length <= 10) )
			NetMoveDelta = 0.011;
		else
			NetMoveDelta = FMax(0.0222,2 * Level.MoveRepSize/Player.CurrentNetSpeed);

		if ( (Level.TimeSeconds - ClientUpdateTime) * Level.TimeDilation * 0.91 < NetMoveDelta )
		{
			PendingMove = NewMove;
			return;
		}
	}

    ClientUpdateTime = Level.TimeSeconds;

	// Send to the server
	ClientRoll = (Rotation.Roll >> 8) & 255;
    if ( PendingMove != None )
    {
		if ( PendingMove.bPressedJump )
			bJumpStatus = !bJumpStatus;
		bPendingJumpStatus = bJumpStatus;
	}
	if ( NewMove.bPressedJump )
		bJumpStatus = !bJumpStatus;

	if ( Pawn == None )
		MoveLoc = Location;
	else
		MoveLoc = Pawn.Location;

    CallServerMove(NewMove, MoveLoc, bPendingJumpStatus, bJumpStatus, ClientRoll,
			(32767 & (Rotation.Pitch/2)) * 32768 + (32767 & (Rotation.Yaw/2)),
			OldMove );

	PendingMove = None;
}

/* CallServerMove()
Call the appropriate replicated servermove() function to send a client player move to the server
*/
function CallServerMove
(
	SavedMove NewMove,
    vector ClientLoc,
    bool NewbPendingJumpStatus,
    bool NewbJumpStatus,
    byte ClientRoll,
    int View,
    SavedMove OldMove
)
{
	local bool bCombine;
	local vector InAccel, BuildAccel;
	local byte OldAccelX, OldAccelY, OldAccelZ;

	// compress old move if it exists
	if ( OldMove != None )
	{
		// old move important to replicate redundantly
		BuildAccel = 0.05 * OldMove.Acceleration + vect(0.5, 0.5, 0.5);
		OldAccelX = CompressAccel(BuildAccel.X);
		OldAccelY = CompressAccel(BuildAccel.Y);
		OldAccelZ = CompressAccel(BuildAccel.Z);
		OldServerMove(OldMove.TimeStamp,OldAccelX, OldAccelY, OldAccelZ, OldMove.CompressedFlags());
	}

	InAccel = NewMove.Acceleration * 10;

	if ( PendingMove != None )
	{
		// send two moves simultaneously
		// FIXMESTEVE - REMOVE - when does this happen that isn't covered by earlier move combining code?
		if ( (InAccel == vect(0,0,0))
			&& (PendingMove.StartVelocity == vect(0,0,0))
			&& (NewMove.DoubleClickMove == DCLICK_None)
			&& (PendingMove.Acceleration == vect(0,0,0)) && (PendingMove.DoubleClickMove == DCLICK_None) && !PendingMove.bDoubleJump )
		{
			if ( Pawn == None )
				bCombine = (Velocity == vect(0,0,0));
			else
				bCombine = (Pawn.Velocity == vect(0,0,0));

			if ( bCombine )
			{
				ServerMove
				(
					NewMove.TimeStamp,
					InAccel,
					ClientLoc,
					NewMove.CompressedFlags(),
					ClientRoll,
					View
				);
				return;
			}
		}

		DualServerMove
		(
			PendingMove.TimeStamp,
			PendingMove.Acceleration * 10,
			PendingMove.CompressedFlags(),
			(32767 & (PendingMove.Rotation.Pitch/2)) * 32768 + (32767 & (PendingMove.Rotation.Yaw/2)),
			NewMove.TimeStamp,
			InAccel,
			ClientLoc,
			NewMove.CompressedFlags(),
			ClientRoll,
			View
		);
	}
	else
	{
		ServerMove
		(
            NewMove.TimeStamp,
            InAccel,
            ClientLoc,
			NewMove.CompressedFlags(),
			ClientRoll,
            View
		);
	}
}

/* HandleWalking:
	Called by PlayerController and PlayerInput to set bIsWalking flag, affecting Pawn's velocity */
function HandleWalking()
{
	if ( Pawn != None )
		Pawn.SetWalking( bRun != 0 );
}

function ServerRestartGame()
{
}

function SetFOVAngle(float newFOV)
{
	FOVAngle = newFOV;
}

function Typing( bool bTyping )
{
	bIsTyping = bTyping;
    if ( (Pawn != None) && !Pawn.bTearOff )
        Pawn.bIsTyping = bIsTyping;

}

//*************************************************************************************
// Normal gameplay execs
// Type the name of the exec function at the console to execute it

exec function Jump()
{
	if ( Level.Pauser == PlayerReplicationInfo )
		SetPause( False );
	else
		bPressedJump = true;
}

// Send a voice message of a certain type to a certain player.
exec function Speech( name Type, int Index, string Callsign )
{
	ServerSpeech(Type,Index,Callsign);
}

function ServerSpeech( name Type, int Index, string Callsign );

exec function RestartLevel()
{
	if( Level.Netmode==NM_Standalone )
		ClientTravel( "?restart", TRAVEL_Relative, false );
}

exec function LocalTravel( string URL )
{
	if( Level.Netmode==NM_Standalone )
		ClientTravel( URL, TRAVEL_Relative, true );
}

// ------------------------------------------------------------------------
// Loading and saving

/* QuickSave()
Save game to slot 9
*/
exec function QuickSave()
{
    if ( (Pawn != None) && (Pawn.Health > 0)
		&& (Level.NetMode == NM_Standalone) )
	{
		ClientMessage(QuickSaveString);
		ConsoleCommand("SaveGame 9");
	}
}

/* QuickLoad()
Load game from slot 9
*/
exec function QuickLoad()
{
	if ( Level.NetMode == NM_Standalone )
		ClientTravel( "?load=9", TRAVEL_Absolute, false);
}

/* SetPause()
 Try to pause game; returns success indicator.
 Replicated to server in network games.
 */
function bool SetPause( BOOL bPause )
{
	local bool bResult;
    bFire = 0;
	// Pause gamepad rumbling too if needed
	bResult = Level.Game.SetPause(bPause, self);
	if (bResult)
	{
		// Set the pause of gamepad rumbling state
		if (ForceFeedbackManager != None)
		{
			ForceFeedbackManager.PauseWaveform(bPause);
		}
	}
	return bResult;
}

/* Pause()
Command to try to pause the game.
*/
exec function Pause()
{
	ServerPause();
}

function ServerPause()
{
	// Pause if not already
	if(Level.Pauser == None)
		SetPause(true);
	else
		SetPause(false);
}

exec function ShowMenu()
{
	// Pause if not already
	if(Level.Pauser == None)
		SetPause(true);
}

// ------------------------------------------------------------------------
// Weapon changing functions

/* ThrowWeapon()
Throw out current weapon, and switch to a new weapon
*/
exec function ThrowWeapon()
{
    if ( (Pawn == None) || (Pawn.Weapon == None) )
		return;

    ServerThrowWeapon();
}

function ServerThrowWeapon()
{
    local Vector	TossVel, POVLoc;
	local Rotator	POVRot;

    if ( Pawn.CanThrowWeapon() )
    {
		GetActorEyesViewPoint( POVLoc, POVRot );
        TossVel = Vector(POVRot);
        TossVel = TossVel * ((Pawn.Velocity Dot TossVel) + 500) + Vect(0,0,200);
        Pawn.TossWeapon(TossVel);
        ClientSwitchToBestWeapon();
    }
}

/* PrevWeapon()
- switch to previous inventory group weapon
*/
exec function PrevWeapon()
{
	if ( Level.Pauser!=None )
		return;

	if ( Pawn.Weapon == None )
	{
		SwitchToBestWeapon();
		return;
	}

	if ( Pawn.InvManager != None )
		Pawn.InvManager.PrevWeapon();
}

/* NextWeapon()
- switch to next inventory group weapon
*/
exec function NextWeapon()
{
	if ( Level.Pauser!=None )
		return;

	if ( Pawn.Weapon == None )
	{
		SwitchToBestWeapon();
		return;
	}

	if ( Pawn.InvManager != None )
		Pawn.InvManager.NextWeapon();
}

// The player wants to fire.
exec function StartFire( optional byte FireModeNum )
{
	if ( Level.Pauser == PlayerReplicationInfo )
	{
		SetPause( false );
		return;
	}
	
	if ( Pawn != None )
	{
		Pawn.StartFire( FireModeNum );
	}
}

exec function StopFire( optional byte FireModeNum )
{
	if ( Pawn != None )
	{
		Pawn.StopFire( FireModeNum );
	}
}

// The player wants to alternate-fire.
exec function StartAltFire( optional Byte FireModeNum )
{
	StartFire( 1 );
}

exec function StopAltFire( optional byte FireModeNum )
{
	StopFire( 1 );
}

/**
 * Looks at all nearby triggers, looking for any that can be
 * interacted with.
 * 
 * @param		checkRadius - distance to search for nearby triggers
 * 
 * @param		crosshairDist - distance from the crosshair that 
 * 				triggers must be, else they will be filtered out
 * 
 * @param		minDot - minimum dot product between trigger and the
 * 				camera orientation needed to make the list
 * 
 * @param		bUsuableOnly - if true, event must return true from
 * 				SequenceEvent::CheckActivate()
 * 
 * @param		out_useList - the list of triggers found to be 
 * 				usuable
 */
simulated function GetTriggerUseList(float checkRadius, float crosshairDist, float minDot, bool bUsuableOnly, out array<Trigger> out_useList)
{
	local int idx;
	local vector cameraLoc;
	local rotator cameraRot;
	local Trigger checkTrigger;
	local SeqEvent_Used	UseSeq;


	if (Pawn != None)
	{
		// grab camera location/rotation for checking crosshairDist
		GetPlayerViewPoint(cameraLoc, cameraRot);

		// search of nearby actors that have use events
		foreach Pawn.CollidingActors(class'Trigger',checkTrigger,checkRadius)
		{
			for (idx = 0; idx < checkTrigger.GeneratedEvents.Length; idx++)
			{
				UseSeq = SeqEvent_Used(checkTrigger.GeneratedEvents[idx]);
				if( UseSeq != None &&
					(!bUsuableOnly || checkTrigger.GeneratedEvents[idx].CheckActivate(checkTrigger,Pawn,true)) &&
					Normal(checkTrigger.Location-cameraLoc) dot vector(cameraRot) >= minDot &&
					(!UseSeq.bAimToInteract || PointDistToLine(checkTrigger.Location, vector(cameraRot), cameraLoc) <= crosshairDist) )
				{
					out_useList[out_useList.Length] = checkTrigger;
					// don't bother searching for more events
					idx = checkTrigger.GeneratedEvents.Length;
				}
			}
		}
	}
}

/**
 * Entry point function for player interactions with the world,
 * re-directs to ServerUse.
 */
exec function Use()
{
	if( Role < Role_Authority )
	{
		PerformedUseAction();
	}
	ServerUse();
}

/**
 * Player pressed UseKey
 */
function ServerUse()
{
	PerformedUseAction();
}

/**
 * return true if player was able to perform an action by pressing the Use Key
 */
simulated function bool PerformedUseAction()
{
	// if the level is paused,
	if( Level.Pauser == PlayerReplicationInfo )
	{
		if( Role == Role_Authority )
		{
			// unpause and move on
			SetPause( false );
		}
		return true;
	}

	// try to interact with triggers
	return TriggerInteracted();
}

/**
 * Examines the nearby enviroment and generates a priority sorted 
 * list of interactable actors, and then attempts to activate each
 * of them until either one was successfully activated, or no more
 * actors are available.
 */
simulated function bool TriggerInteracted()
{
	local Actor A;
	local int idx;
	local float Weight;
	local bool bInserted;
	local vector cameraLoc;
	local rotator cameraRot;
	local array<Trigger> useList;
	// the following 2 arrays should always match in length
	local array<Actor> sortedList;
	local array<float> weightList;

	if ( Pawn != None )
	{
		GetTriggerUseList(InteractDistance,60.f,0.f,true,useList);
		// if we have found some interactable actors,
		if (useList.Length > 0)
		{
			// grab the current camera location/rotation for weighting purposes
			GetPlayerViewPoint(cameraLoc, cameraRot);
			// then build the sorted list
			while (useList.Length > 0)
			{
				// pop the actor off this list
				A = useList[useList.Length-1];
				useList.Length = useList.Length - 1;
				// calculate the weight of this actor in terms of optimal interaction
				// first based on the dot product from our view rotation
				weight = Normal(A.Location-cameraLoc) dot vector(cameraRot);
				// and next on the distance
				weight += 1.f - (VSize(A.Location-Pawn.Location)/InteractDistance);
				// find the optimal insertion point
				bInserted = false;
				for (idx = 0; idx < sortedList.Length && !bInserted; idx++)
				{
					if (weightList[idx] < weight)
					{
						// insert the new entry
						sortedList.Insert(idx,1);
						weightList.Insert(idx,1);
						sortedList[idx] = A;
						weightList[idx] = weight;
						bInserted = true;
					}
				}
				// if no insertion was made
				if (!bInserted)
				{
					// then tack on the end of the list
					idx = sortedList.Length;
					sortedList[idx] = A;
					weightList[idx] = weight;
				}
			}
			// finally iterate through each actor in the sorted list and
			// attempt to activate it until one succeeds or none are left
			for (idx = 0; idx < sortedList.Length; idx++)
			{
				if (sortedList[idx].UsedBy(Pawn))
				{
					// skip the rest
					// idx = sortedList.Length;
					return true;
				}
			}
		}
	}

	return false;
}


exec function Suicide()
{
	if ( (Pawn != None) && (Level.TimeSeconds - Pawn.LastStartTime > 10) )
		Pawn.Suicide();
}

exec function SetName( coerce string S)
{
	ChangeName(S);
	UpdateURL("Name", S, true);
	SaveConfig();
}

function ChangeName( coerce string S )
{
    if ( Len(S) > 20 )
        S = left(S,20);
    ReplaceText(S, " ", "_");
    ReplaceText(S, "\"", "");
    Level.Game.ChangeName( self, S, true );
}

exec function SwitchTeam()
{
	if ( (PlayerReplicationInfo.Team == None) || (PlayerReplicationInfo.Team.TeamIndex == 1) )
        ServerChangeTeam(0);
	else
        ServerChangeTeam(1);
}

exec function ChangeTeam( int N )
{
	ServerChangeTeam(N);
}

function ServerChangeTeam( int N )
{
	local TeamInfo OldTeam;

	OldTeam = PlayerReplicationInfo.Team;
    Level.Game.ChangeTeam(self, N, true);
	if ( Level.Game.bTeamGame && (PlayerReplicationInfo.Team != OldTeam) )
    {
		if ( Pawn != None )
			Pawn.PlayerChangedTeam();
}
}


exec function SwitchLevel( string URL )
{
	if( Level.NetMode==NM_Standalone || Level.netMode==NM_ListenServer )
		Level.ServerTravel( URL, false );
}

exec function ClearProgressMessages()
{
	local int i;

	for (i=0; i<ArrayCount(ProgressMessage); i++)
	{
		ProgressMessage[i] = "";
		ProgressColor[i] = MakeColor(255,255,255,255);
	}
}

exec event SetProgressMessage( int Index, string S, color C )
{
	if ( Index < ArrayCount(ProgressMessage) )
	{
		ProgressMessage[Index] = S;
		ProgressColor[Index] = C;
	}
}

exec event SetProgressTime( float T )
{
	ProgressTimeOut = T + Level.TimeSeconds;
}

function Restart()
{
	Super.Restart();
	ServerTimeStamp = 0;
	ResetTimeMargin();
	EnterStartState();
    ClientRestart(Pawn);
	SetViewTarget(Pawn);
    ResetCameraMode();
}

function EnterStartState()
{
	local name NewState;

	if ( Pawn.PhysicsVolume.bWaterVolume )
	{
		if ( Pawn.HeadVolume.bWaterVolume )
			Pawn.BreathTime = Pawn.UnderWaterTime;
		NewState = Pawn.WaterMovementState;
	}
	else
		NewState = Pawn.LandMovementState;

	if ( IsInState(NewState) )
		BeginState();
	else
		GotoState(NewState);
}

function ClientRestart(Pawn NewPawn)
{
	local bool bNewViewTarget;

	bMovementInputEnabled	= default.bMovementInputEnabled;
	bTurningInputEnabled	= default.bTurningInputEnabled;

	Pawn = NewPawn;
	if ( (Pawn != None) && Pawn.bTearOff )
	{
		UnPossess();
		Pawn = None;
	}

	AcknowledgePossession(Pawn);

	if ( Pawn == None )
	{
		GotoState('WaitingForPawn');
		return;
	}
	Pawn.ClientRestart();
    bNewViewTarget = (ViewTarget != Pawn);
	SetViewTarget(Pawn);
    ResetCameraMode();
    CleanOutSavedMoves();
	EnterStartState();
}


//=============================================================================
// functions.

// Just changed to pendingWeapon
function NotifyChangedWeapon( Weapon PrevWeapon, Weapon NewWeapon );

event TravelPostAccept()
{
	if ( Pawn.Health <= 0 )
		Pawn.Health = Pawn.Default.Health;
}

event PlayerTick( float DeltaTime )
{
/* FIXMESTEVE
	if ( bForcePrecache )
	{
		if ( Level.TimeSeconds > ForcePrecacheTime )
		{
			bForcePrecache = false;
			// precache game materials and staticmeshes
		}
	}
	else 
*/
if ( !bShortConnectTimeOut )
	{
		bShortConnectTimeOut = true;
		ServerShortTimeout();
	}

	if ( Pawn != AcknowledgedPawn )
	{
		if ( Role < ROLE_Authority )
		{
			// make sure old pawn controller is right
			if ( (AcknowledgedPawn != None) && (AcknowledgedPawn.Controller == self) )
				AcknowledgedPawn.Controller = None;
		}
		AcknowledgePossession(Pawn);
	}

	PlayerInput.PlayerInput(DeltaTime);
	if ( bUpdatePosition )
		ClientUpdatePosition();
	PlayerMove(DeltaTime);
}

function PlayerMove(float DeltaTime);

function bool AimingHelp()
{
 return (Level.NetMode == NM_Standalone) && bAimingHelp;
}

/**
 * Adjusts weapon aiming direction.
 * Gives controller a chance to modify the aiming of the pawn. For example aim error, auto aiming, adhesion, AI help...
 * Requested by weapon prior to firing.
 *
 * @param	W, weapon about to fire
 * @param	StartFireLoc, world location of weapon fire start trace, or projectile spawn loc.
 * @param	BaseAimRot, original aiming rotation without any modifications.
 */
function Rotator GetAdjustedAimFor( Weapon W, vector StartFireLoc, rotator BaseAimRot )
{
	local vector	FireDir, AimSpot, HitLocation, HitNormal, OldAim, AimOffset;
	local actor		BestTarget;
	local float		bestAim, bestDist, projspeed;
	local actor		HitActor;
	local bool		bNoZAdjust, bLeading, bInstantHit;
	local rotator	AimRot;

	if ( W == None || W.IsInstantHit() )
		bInstantHit = true;

	FireDir		= vector(BaseAimRot);

	if ( bInstantHit )
		HitActor = Trace(HitLocation, HitNormal, StartFireLoc + 10000 * FireDir, StartFireLoc, true);
	else
		HitActor = Trace(HitLocation, HitNormal, StartFireLoc + 4000 * FireDir, StartFireLoc, true);

	if ( (HitActor != None) && HitActor.bProjTarget )
	{
		BestTarget	= HitActor;
		bNoZAdjust	= true;
		OldAim		= HitLocation;
		BestDist	= VSize(BestTarget.Location - Pawn.Location);
	}
	else
	{
		// adjust aim based on FOV
        bestAim = 0.90;
        if ( AimingHelp() )
		{
			bestAim = 0.93;
			if ( bInstantHit )
				bestAim = 0.97;
			if ( FOVAngle < DefaultFOV - 8 )
				bestAim = 0.99;
		}
        else if ( bInstantHit )
                bestAim = 1.0;
        //FIXMESTEVE BestTarget = PickTarget(bestAim, bestDist, FireDir, StartFireLoc, W.WeaponRange);
		if ( BestTarget == None )
		{
        	return BaseAimRot;
		}
		OldAim = StartFireLoc + FireDir * bestDist;
	}
if ( bInstantHit )
	InstantWarnTarget(BestTarget,W,FireDir);

	ShotTarget = Pawn(BestTarget);
    if ( !bAimingHelp || (Level.NetMode != NM_Standalone) )
	{
    	return BaseAimRot;
	}

	// aim at target - help with leading also
	if ( !bInstantHit )
	{
		projspeed = W.GetProjectileClass().default.Speed;
        BestDist = vsize(BestTarget.Location + BestTarget.Velocity * FMin(1, 0.02 + BestDist/projSpeed) - StartFireLoc);
		bLeading = true;
        FireDir = BestTarget.Location + BestTarget.Velocity * FMin(1, 0.02 + BestDist/projSpeed) - StartFireLoc;
		AimSpot = StartFireLoc + bestDist * Normal(FireDir);
        // if splash damage weapon, try aiming at feet - trace down to find floor
/*FIXMESTEVE
        if ( FiredAmmunition.bTrySplash
            && ((BestTarget.Velocity != vect(0,0,0)) || (BestDist > 1500)) )
        {
            HitActor = Trace(HitLocation, HitNormal, AimSpot - BestTarget.CollisionHeight * vect(0,0,2), AimSpot, false);
            if ( (HitActor != None)
                && FastTrace(HitLocation + vect(0,0,4),StartFireLoc) )
                return rotator(HitLocation + vect(0,0,6) - StartFireLoc);
        }
		*/
	}
	else
	{
		FireDir = BestTarget.Location - StartFireLoc;
		AimSpot = StartFireLoc + bestDist * Normal(FireDir);
	}
	AimOffset = AimSpot - OldAim;

    if ( ShotTarget != None )
    {
	    // adjust Z of shooter if necessary
	    if ( bNoZAdjust || (bLeading && (Abs(AimOffset.Z) < ShotTarget.CylinderComponent.CollisionHeight)) )
	        AimSpot.Z = OldAim.Z;
	    else if ( AimOffset.Z < 0 )
	        AimSpot.Z = ShotTarget.Location.Z + 0.4 * ShotTarget.CylinderComponent.CollisionHeight;
	    else
	        AimSpot.Z = ShotTarget.Location.Z - 0.7 * ShotTarget.CylinderComponent.CollisionHeight;
    }
    else
	    AimSpot.Z = OldAim.Z;

	if ( !bLeading )
	{
		// if not leading, add slight random error ( significant at long distances )
		if ( !bNoZAdjust )
		{
			AimRot = rotator(AimSpot - StartFireLoc);
			if ( FOVAngle < DefaultFOV - 8 )
				AimRot.Yaw = AimRot.Yaw + 200 - Rand(400);
			else
				AimRot.Yaw = AimRot.Yaw + 375 - Rand(750);
			return AimRot;
		}
	}
	else if ( !FastTrace(StartFireLoc + 0.9 * bestDist * Normal(FireDir), StartFireLoc) )
	{
		FireDir = BestTarget.Location - StartFireLoc;
		AimSpot = StartFireLoc + bestDist * Normal(FireDir);
	}

	return rotator(AimSpot - StartFireLoc);
}

function bool NotifyLanded(vector HitNormal)
{
	return bUpdating;
}

//=============================================================================
// Player Control

// Player view.
// Compute the rendering viewpoint for the player.
//

function AdjustView(float DeltaTime )
{
	// teleporters affect your FOV, so adjust it back down
	if ( FOVAngle != DesiredFOV )
	{
		if ( FOVAngle > DesiredFOV )
			FOVAngle = FOVAngle - FMax(7, 0.9 * DeltaTime * (FOVAngle - DesiredFOV));
		else
			FOVAngle = FOVAngle - FMin(-7, 0.9 * DeltaTime * (FOVAngle - DesiredFOV));
		if ( Abs(FOVAngle - DesiredFOV) <= 10 )
			FOVAngle = DesiredFOV;
	}

	// adjust FOV for weapon zooming
	if ( bZooming )
	{
		ZoomLevel += DeltaTime * 1.0;
		if (ZoomLevel > 0.9)
			ZoomLevel = 0.9;
		DesiredFOV = FClamp(90.0 - (ZoomLevel * 88.0), 1, 170);
	}
}

/**
 * Returns player's FOV angle 
 */
event float	GetFOVAngle()
{
	if( PlayerCamera != None )
	{
		return PlayerCamera.GetFOVAngle();
	}
	return super.GetFOVAngle();
}

event ClientSetViewTarget( Actor a )
{
	if ( A == None )
		ServerVerifyViewTarget();
    SetViewTarget( a );
}

function ServerVerifyViewTarget()
{
	if ( ViewTarget == Self )
		return;

	ClientSetViewTarget(ViewTarget);
}

/**
 * Returns Player's Point of View
 * For the AI this means the Pawn's 'Eyes' ViewPoint
 * For a Human player, this means the Camera's ViewPoint 
 *
 * @output	out_Location, view location of player
 * @output	out_rotation, view rotation of player
 */
event GetPlayerViewPoint( out vector out_Location, out Rotator out_Rotation )
{
	if ( PlayerCamera != None )
		PlayerCamera.GetCameraViewPoint(out_Location, out_Rotation);
	else
		super.GetPlayerViewPoint(out_Location, out_Rotation);
}

function CheckShake(out float MaxOffset, out float Offset, out float Rate, out float Time, float DeltaTime)
{
	if ( abs(Offset) < abs(MaxOffset) )
		return;

	Offset = MaxOffset;
	if ( Time > 1 )
	{
		if ( Time * abs(MaxOffset/Rate) <= 1 )
			MaxOffset = MaxOffset * (1/Time - 1);
		else
			MaxOffset *= -1;
        Time -= DeltaTime;
		Rate *= -1;
	}
	else
	{
		MaxOffset = 0;
		Offset = 0;
		Rate = 0;
	}
}

function ViewShake(float DeltaTime)
{
	local Rotator ViewRotation;
	local float FRoll;

	if ( ShakeOffsetRate != vect(0,0,0) )
	{
		// modify shake offset
		ShakeOffset.X += DeltaTime * ShakeOffsetRate.X;
		CheckShake(MaxShakeOffset.X, ShakeOffset.X, ShakeOffsetRate.X, ShakeOffsetTime.X, DeltaTime);

		ShakeOffset.Y += DeltaTime * ShakeOffsetRate.Y;
		CheckShake(MaxShakeOffset.Y, ShakeOffset.Y, ShakeOffsetRate.Y, ShakeOffsetTime.Y, DeltaTime);

		ShakeOffset.Z += DeltaTime * ShakeOffsetRate.Z;
		CheckShake(MaxShakeOffset.Z, ShakeOffset.Z, ShakeOffsetRate.Z, ShakeOffsetTime.Z, DeltaTime);
	}

	ViewRotation = Rotation;

	if ( ShakeRollRate != 0 )
	{
		ViewRotation.Roll = ((ViewRotation.Roll & 65535) + ShakeRollRate * DeltaTime) & 65535;
		if ( ViewRotation.Roll > 32768 )
			ViewRotation.Roll -= 65536;
		FRoll = ViewRotation.Roll;
		CheckShake(MaxShakeRoll, FRoll, ShakeRollRate, ShakeRollTime, DeltaTime);
		ViewRotation.Roll = FRoll;
	}
	else if ( bZeroRoll )
	ViewRotation.Roll = 0;

	SetRotation(ViewRotation);
}

function bool TurnTowardNearestEnemy();

function TurnAround()
{
	if ( !bSetTurnRot )
	{
		TurnRot180 = Rotation;
		TurnRot180.Yaw += 32768;
		bSetTurnRot = true;
	}

	DesiredRotation = TurnRot180;
}

function UpdateRotation( float DeltaTime )
{
	local Rotator	DeltaRot, newRotation, ViewRotation;

	ViewRotation = Rotation;
	DesiredRotation = ViewRotation; //save old rotation
	if ( PlayerInput.bTurnToNearest != 0 )
		TurnTowardNearestEnemy();
	else if ( PlayerInput.bTurn180 != 0 )
		TurnAround();
	else
	{
		TurnTarget = None;
		bSetTurnRot = false;
		
		// Calculate Delta to be applied on ViewRotation
		DeltaRot.Yaw	= PlayerInput.aTurn;
		DeltaRot.Pitch	= PlayerInput.aLookUp;
	}

	ProcessViewRotation( DeltaTime, ViewRotation, DeltaRot );
	SetRotation(ViewRotation);

	ViewShake( deltaTime );

	NewRotation = ViewRotation;
	NewRotation.Roll = Rotation.Roll;

	if ( Pawn != None )
		Pawn.FaceRotation(NewRotation, deltatime);
}

/**
 * Processes the player's ViewRotation
 * adds delta rot (player input), applies any limits and post-processing
 * returns the final ViewRotation set on PlayerController
 *
 * @param	DeltaTime, time since last frame
 * @param	ViewRotation, current player ViewRotation
 * @param	DeltaRot, player input added to ViewRotation
 */
function ProcessViewRotation( float DeltaTime, out Rotator out_ViewRotation, Rotator DeltaRot )
{
	if( PlayerCamera != None ) 
	{
		PlayerCamera.ProcessViewRotation( DeltaTime, out_ViewRotation, DeltaRot );
	}

	if ( Pawn != None )	
	{	// Give the Pawn a chance to modify DeltaRot (limit view for ex.)
		Pawn.ProcessViewRotation( DeltaTime, out_ViewRotation, DeltaRot );
	}
	else
	{	
		// If Pawn doesn't exist, limit view
		out_ViewRotation.Pitch = out_ViewRotation.Pitch & 65535;

		if( out_ViewRotation.Pitch > 18000 && out_ViewRotation.Pitch < 32768 )
		{
			out_ViewRotation.Pitch = 18000;
			DeltaRot.Pitch = Min(0, DeltaRot.Pitch);
		}
		else if( out_ViewRotation.Pitch < 49152 && out_ViewRotation.Pitch > 32768  )
		{
			out_ViewRotation.Pitch = 49152;
			DeltaRot.Pitch = Max(0, DeltaRot.Pitch);
		}
	}
	
	// Apply final delta
	out_ViewRotation += DeltaRot;
}

function ClearDoubleClick()
{
	if (PlayerInput != None)
		PlayerInput.DoubleClickTimer = 0.0;
}

/* CheckJumpOrDuck()
Called by ProcessMove()
handle jump and duck buttons which are pressed
*/
function CheckJumpOrDuck()
{
	if ( bPressedJump )
	{
		Pawn.DoJump( bUpdating );
	}
}

// Player movement.
// Player Standing, walking, running, falling.
state PlayerWalking
{
ignores SeePlayer, HearNoise, Bump;

	function NotifyPhysicsVolumeChange( PhysicsVolume NewVolume )
	{
		if ( NewVolume.bWaterVolume )
			GotoState(Pawn.WaterMovementState);
	}

	function ProcessMove(float DeltaTime, vector NewAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
	{
		local vector OldAccel;

		if ( Pawn == None )
		{
			return;
		}

		if ( Role == Role_Authority && Level.NetMode != NM_StandAlone )
		{
			// Update ViewPitch for remote clients
			Pawn.SetRemoteViewPitch( Rotation.Pitch );
		}

		OldAccel			= Pawn.Acceleration;
		Pawn.Acceleration	= NewAccel;

		CheckJumpOrDuck();
	}

	function PlayerMove( float DeltaTime )
	{
		local vector X,Y,Z, NewAccel;
		local eDoubleClickDir DoubleClickMove;
		local rotator OldRotation;
		local bool	bSaveJump;

		GetAxes(Pawn.Rotation,X,Y,Z);

		// Update acceleration.
		NewAccel = PlayerInput.aForward*X + PlayerInput.aStrafe*Y;
		NewAccel.Z = 0;
		if ( VSize(NewAccel) < 1.0 )
			NewAccel = vect(0,0,0);
        DoubleClickMove = PlayerInput.CheckForDoubleClickMove(DeltaTime/Level.TimeDilation);

		Pawn.CheckBob(DeltaTime, Y);

		// Update rotation.
		OldRotation = Rotation;
		UpdateRotation( DeltaTime );
		bDoubleJump = false;

		if ( bPressedJump && Pawn.CannotJumpNow() )
		{
			bSaveJump = true;
			bPressedJump = false;
		}
		else
			bSaveJump = false;

		if ( Role < ROLE_Authority ) // then save this move and replicate it
			ReplicateMove(DeltaTime, NewAccel, DoubleClickMove, OldRotation - Rotation);
		else
			ProcessMove(DeltaTime, NewAccel, DoubleClickMove, OldRotation - Rotation);
		bPressedJump = bSaveJump;
	}

	function BeginState()
	{
       	DoubleClickDir = DCLICK_None;
       	bPressedJump = false;
       	GroundPitch = 0;
		if ( Pawn != None )
		{
			Pawn.ShouldCrouch(false);
			if (Pawn.Physics != PHYS_Falling && Pawn.Physics != PHYS_RigidBody) // FIXME HACK!!!
				Pawn.SetPhysics(PHYS_Walking);
		}
	}

	function EndState()
	{
		GroundPitch = 0;
		if ( Pawn != None )
		{
			Pawn.SetRemoteViewPitch( 0 );
			if ( bDuck == 0 )
			{
				Pawn.ShouldCrouch(false);
			}
		}
	}
}

// player is climbing ladder
state PlayerClimbing
{
ignores SeePlayer, HearNoise, Bump;

	function NotifyPhysicsVolumeChange( PhysicsVolume NewVolume )
	{
		if ( NewVolume.bWaterVolume )
			GotoState(Pawn.WaterMovementState);
		else
			GotoState(Pawn.LandMovementState);
	}

	function ProcessMove(float DeltaTime, vector NewAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
	{
		local vector OldAccel;

		if ( Pawn == None )
		{
			return;
		}

		if ( Role == Role_Authority && Level.NetMode != NM_StandAlone )
		{
			// Update ViewPitch for remote clients
			Pawn.SetRemoteViewPitch( Rotation.Pitch );
		}

		OldAccel			= Pawn.Acceleration;
		Pawn.Acceleration	= NewAccel;

		if ( bPressedJump )
		{
			Pawn.DoJump(bUpdating);
			if ( Pawn.Physics == PHYS_Falling )
			{
				GotoState('PlayerWalking');
			}
		}
	}

	function PlayerMove( float DeltaTime )
	{
		local vector X,Y,Z, NewAccel;
		local eDoubleClickDir DoubleClickMove;
		local rotator OldRotation, ViewRotation;
		local bool	bSaveJump;

		GetAxes(Rotation,X,Y,Z);

		// Update acceleration.
		if ( Pawn.OnLadder != None )
		{
			NewAccel = PlayerInput.aForward*Pawn.OnLadder.ClimbDir;
            if ( Pawn.OnLadder.bAllowLadderStrafing )
				NewAccel += PlayerInput.aStrafe*Y;
		}
		else
			NewAccel = PlayerInput.aForward*X + PlayerInput.aStrafe*Y;
		if ( VSize(NewAccel) < 1.0 )
			NewAccel = vect(0,0,0);

		ViewRotation = Rotation;

		// Update rotation.
		SetRotation(ViewRotation);
		OldRotation = Rotation;
		UpdateRotation( DeltaTime );

		if ( Role < ROLE_Authority ) // then save this move and replicate it
			ReplicateMove(DeltaTime, NewAccel, DoubleClickMove, OldRotation - Rotation);
		else
			ProcessMove(DeltaTime, NewAccel, DoubleClickMove, OldRotation - Rotation);
		bPressedJump = bSaveJump;
	}

	function BeginState()
	{
		Pawn.ShouldCrouch(false);
		bPressedJump = false;
	}

	function EndState()
	{
		if ( Pawn != None )
		{
			Pawn.SetRemoteViewPitch( 0 );
			Pawn.ShouldCrouch(false);
		}
	}
}

// Player Driving a vehicle.
state PlayerDriving
{
ignores SeePlayer, HearNoise, Bump;

    function ProcessMove(float DeltaTime, vector NewAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
    {

    }

	// Set the throttle, steering etc. for the vehicle based on the input provided
	function ProcessDrive(float InForward, float InStrafe, float InUp, bool InJump)
	{
		local Vehicle CurrentVehicle;

	    CurrentVehicle = Vehicle(Pawn);

        if(CurrentVehicle == None)
			return;

		//log("Forward:"@InForward@" Strafe:"@InStrafe@" Up:"@InUp);

		CurrentVehicle.Throttle = FClamp( InForward/5000.0, -1.0, 1.0 );
		CurrentVehicle.Steering = FClamp( -InStrafe/5000.0, -1.0, 1.0 );
		CurrentVehicle.Rise = FClamp( InUp/5000.0, -1.0, 1.0 );
	}

	function PlayerMove( float DeltaTime )
	{
		local Vehicle CurrentVehicle;

		CurrentVehicle = Vehicle(Pawn);

		// update 'looking' rotation
        UpdateRotation(DeltaTime);

        // TODO: Don't send things like aForward and aStrafe for gunners who don't need it
		// Only servers can actually do the driving logic.
		if(Role < ROLE_Authority)
		{
			if (CurrentVehicle != None)
			{
				CurrentVehicle.Throttle = FClamp( PlayerInput.aForward/5000.0, -1.0, 1.0 );
				CurrentVehicle.Steering = FClamp( -PlayerInput.aStrafe/5000.0, -1.0, 1.0 );
				CurrentVehicle.Rise = FClamp( PlayerInput.aUp/5000.0, -1.0, 1.0 );
			}

			ServerDrive(PlayerInput.aForward, PlayerInput.aStrafe, PlayerInput.aUp, bPressedJump, (32767 & (Rotation.Pitch/2)) * 32768 + (32767 & (Rotation.Yaw/2)));
		}
		else
		{
			ProcessDrive(PlayerInput.aForward, PlayerInput.aStrafe, PlayerInput.aUp, bPressedJump);
		}
	}

	function ServerUse()
	{
		local Vehicle CurrentVehicle;

		CurrentVehicle = Vehicle(Pawn);
		CurrentVehicle.DriverLeave(false);
	}

	function BeginState()
	{
		PlayerReplicationInfo.bReceivedPing = false;
		CleanOutSavedMoves();
	}

	function EndState()
	{
		CleanOutSavedMoves();
	}
}

// Player movement.
// Player walking on walls
state PlayerSpidering
{
ignores SeePlayer, HearNoise, Bump;

	event bool NotifyHitWall(vector HitNormal, actor HitActor)
	{
		Pawn.SetPhysics(PHYS_Spider);
		Pawn.SetBase(HitActor, HitNormal);
		return true;
	}

	// if spider mode, update rotation based on floor
	function UpdateRotation( float DeltaTime )
	{
        local rotator ViewRotation;
		local vector MyFloor, CrossDir, FwdDir, OldFwdDir, OldX, RealFloor;

		TurnTarget = None;
		bSetTurnRot = false;

		if ( (Pawn.Base == None) || (Pawn.Floor == vect(0,0,0)) )
			MyFloor = vect(0,0,1);
		else
			MyFloor = Pawn.Floor;

		if ( MyFloor != OldFloor )
		{
			// smoothly change floor
			RealFloor = MyFloor;
			MyFloor = Normal(6*DeltaTime * MyFloor + (1 - 6*DeltaTime) * OldFloor);
			if ( (RealFloor Dot MyFloor) > 0.999 )
				MyFloor = RealFloor;

			// translate view direction
			CrossDir = Normal(RealFloor Cross OldFloor);
			FwdDir = CrossDir Cross MyFloor;
			OldFwdDir = CrossDir Cross OldFloor;
			ViewX = MyFloor * (OldFloor Dot ViewX)
						+ CrossDir * (CrossDir Dot ViewX)
						+ FwdDir * (OldFwdDir Dot ViewX);
			ViewX = Normal(ViewX);

			ViewZ = MyFloor * (OldFloor Dot ViewZ)
						+ CrossDir * (CrossDir Dot ViewZ)
						+ FwdDir * (OldFwdDir Dot ViewZ);
			ViewZ = Normal(ViewZ);
			OldFloor = MyFloor;
			ViewY = Normal(MyFloor Cross ViewX);
		}

		if ( (PlayerInput.aTurn != 0) || (PlayerInput.aLookUp != 0) )
		{
			// adjust Yaw based on aTurn
			if ( PlayerInput.aTurn != 0 )
				ViewX = Normal(ViewX + 2 * ViewY * Sin(0.0005*DeltaTime*PlayerInput.aTurn));

			// adjust Pitch based on aLookUp
			if ( PlayerInput.aLookUp != 0 )
			{
				OldX = ViewX;
				ViewX = Normal(ViewX + 2 * ViewZ * Sin(0.0005*DeltaTime*PlayerInput.aLookUp));
				ViewZ = Normal(ViewX Cross ViewY);

				// bound max pitch
				if ( (ViewZ Dot MyFloor) < 0.707   )
				{
					OldX = Normal(OldX - MyFloor * (MyFloor Dot OldX));
					if ( (ViewX Dot MyFloor) > 0)
						ViewX = Normal(OldX + MyFloor);
					else
						ViewX = Normal(OldX - MyFloor);

					ViewZ = Normal(ViewX Cross ViewY);
				}
			}

			// calculate new Y axis
			ViewY = Normal(MyFloor Cross ViewX);
		}
		ViewRotation =  OrthoRotation(ViewX,ViewY,ViewZ);
		SetRotation(ViewRotation);
		ViewShake(deltaTime);
		Pawn.FaceRotation(ViewRotation, deltaTime );
	}

	function bool NotifyLanded(vector HitNormal)
	{
		Pawn.SetPhysics(PHYS_Spider);
		return bUpdating;
	}

	function NotifyPhysicsVolumeChange( PhysicsVolume NewVolume )
	{
		if ( NewVolume.bWaterVolume )
			GotoState(Pawn.WaterMovementState);
	}

	function ProcessMove(float DeltaTime, vector NewAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
	{
		local vector OldAccel;

		OldAccel = Pawn.Acceleration;
		Pawn.Acceleration = NewAccel;

		if ( bPressedJump )
			Pawn.DoJump(bUpdating);
	}

	function PlayerMove( float DeltaTime )
	{
		local vector NewAccel;
		local eDoubleClickDir DoubleClickMove;
		local rotator OldRotation, ViewRotation;
		local bool	bSaveJump;

		GroundPitch = 0;
		ViewRotation = Rotation;

		Pawn.CheckBob(DeltaTime,vect(0,0,0));

		// Update rotation.
		SetRotation(ViewRotation);
		OldRotation = Rotation;
		UpdateRotation( DeltaTime );

		// Update acceleration.
		NewAccel = PlayerInput.aForward*Normal(ViewX - OldFloor * (OldFloor Dot ViewX)) + PlayerInput.aStrafe*ViewY;
		if ( VSize(NewAccel) < 1.0 )
			NewAccel = vect(0,0,0);

		if ( bPressedJump && Pawn.CannotJumpNow() )
		{
			bSaveJump = true;
			bPressedJump = false;
		}
		else
			bSaveJump = false;

		if ( Role < ROLE_Authority ) // then save this move and replicate it
			ReplicateMove(DeltaTime, NewAccel, DoubleClickMove, OldRotation - Rotation);
		else
			ProcessMove(DeltaTime, NewAccel, DoubleClickMove, OldRotation - Rotation);
		bPressedJump = bSaveJump;
	}

	function BeginState()
	{
		/* AJS_HACK
		if ( Pawn.Mesh == None )
			Pawn.SetMesh();
		*/
		OldFloor = vect(0,0,1);
		GetAxes(Rotation,ViewX,ViewY,ViewZ);
		DoubleClickDir = DCLICK_None;
		Pawn.ShouldCrouch(false);
		bPressedJump = false;
		if (Pawn.Physics != PHYS_Falling)
			Pawn.SetPhysics(PHYS_Spider);
		GroundPitch = 0;
		Pawn.bCrawler = true;
		Pawn.SetCollisionSize(Pawn.Default.CylinderComponent.CollisionHeight, Pawn.Default.CylinderComponent.CollisionHeight);
	}

	function EndState()
	{
		GroundPitch = 0;
		if ( Pawn != None )
		{
			Pawn.SetCollisionSize(Pawn.Default.CylinderComponent.CollisionRadius, Pawn.Default.CylinderComponent.CollisionHeight);
			Pawn.ShouldCrouch(false);
			Pawn.bCrawler = Pawn.Default.bCrawler;
		}
	}
}

// Player movement.
// Player Swimming
state PlayerSwimming
{
ignores SeePlayer, HearNoise, Bump;

    function bool WantsSmoothedView()
    {
        return ( !Pawn.bJustLanded );
    }

	function bool NotifyLanded(vector HitNormal)
	{
		if ( Pawn.PhysicsVolume.bWaterVolume )
			Pawn.SetPhysics(PHYS_Swimming);
		else
			GotoState(Pawn.LandMovementState);
		return bUpdating;
	}

	function NotifyPhysicsVolumeChange( PhysicsVolume NewVolume )
	{
		local actor HitActor;
		local vector HitLocation, HitNormal, checkpoint;

		if ( !NewVolume.bWaterVolume )
		{
			Pawn.SetPhysics(PHYS_Falling);
            if ( Pawn.Velocity.Z > 0 )
            {
			    if (Pawn.bUpAndOut && Pawn.CheckWaterJump(HitNormal)) //check for waterjump
			    {
				    Pawn.velocity.Z = FMax(Pawn.JumpZ,420) + 2 * Pawn.CylinderComponent.CollisionRadius; //set here so physics uses this for remainder of tick
				    GotoState(Pawn.LandMovementState);
			    }
			    else if ( (Pawn.Velocity.Z > 160) || !Pawn.TouchingWaterVolume() )
				    GotoState(Pawn.LandMovementState);
			    else //check if in deep water
			    {
				    checkpoint = Pawn.Location;
				    checkpoint.Z -= (Pawn.CylinderComponent.CollisionHeight + 6.0);
				    HitActor = Trace(HitLocation, HitNormal, checkpoint, Pawn.Location, false);
				    if (HitActor != None)
					    GotoState(Pawn.LandMovementState);
				    else
				    {
					    Enable('Timer');
					    SetTimer(0.7,false);
				    }
			    }
		    }
        }
		else
		{
			Disable('Timer');
			Pawn.SetPhysics(PHYS_Swimming);
		}
	}

	function ProcessMove(float DeltaTime, vector NewAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
	{
		local vector X,Y,Z, OldAccel;
		local bool bUpAndOut;

		GetAxes(Rotation,X,Y,Z);
		OldAccel = Pawn.Acceleration;
		Pawn.Acceleration = NewAccel;
        bUpAndOut = ((X Dot Pawn.Acceleration) > 0) && ((Pawn.Acceleration.Z > 0) || (Rotation.Pitch > 2048));
		if ( Pawn.bUpAndOut != bUpAndOut )
			Pawn.bUpAndOut = bUpAndOut;
		if ( !Pawn.PhysicsVolume.bWaterVolume ) //check for waterjump
			NotifyPhysicsVolumeChange(Pawn.PhysicsVolume);
	}

	function PlayerMove(float DeltaTime)
	{
		local rotator oldRotation;
		local vector X,Y,Z, NewAccel;

		GetAxes(Rotation,X,Y,Z);

		NewAccel = PlayerInput.aForward*X + PlayerInput.aStrafe*Y + PlayerInput.aUp*vect(0,0,1);
		if ( VSize(NewAccel) < 1.0 )
			NewAccel = vect(0,0,0);

		//add bobbing when swimming
		Pawn.CheckBob(DeltaTime, Y);

		// Update rotation.
		oldRotation = Rotation;
		UpdateRotation( DeltaTime );

		if ( Role < ROLE_Authority ) // then save this move and replicate it
			ReplicateMove(DeltaTime, NewAccel, DCLICK_None, OldRotation - Rotation);
		else
			ProcessMove(DeltaTime, NewAccel, DCLICK_None, OldRotation - Rotation);
		bPressedJump = false;
	}

	function Timer()
	{
		if ( !Pawn.PhysicsVolume.bWaterVolume && (Role == ROLE_Authority) )
			GotoState(Pawn.LandMovementState);

		Disable('Timer');
	}

	function BeginState()
	{
		Disable('Timer');
		Pawn.SetPhysics(PHYS_Swimming);
	}
}

state PlayerFlying
{
ignores SeePlayer, HearNoise, Bump;

	function PlayerMove(float DeltaTime)
	{
		local vector X,Y,Z;

		GetAxes(Rotation,X,Y,Z);

		Pawn.Acceleration = PlayerInput.aForward*X + PlayerInput.aStrafe*Y;
		if ( VSize(Pawn.Acceleration) < 1.0 )
			Pawn.Acceleration = vect(0,0,0);
		if ( bCheatFlying && (Pawn.Acceleration == vect(0,0,0)) )
			Pawn.Velocity = vect(0,0,0);
		// Update rotation.
		UpdateRotation( DeltaTime );

		if ( Role < ROLE_Authority ) // then save this move and replicate it
			ReplicateMove(DeltaTime, Pawn.Acceleration, DCLICK_None, rot(0,0,0));
		else
			ProcessMove(DeltaTime, Pawn.Acceleration, DCLICK_None, rot(0,0,0));
	}

	function BeginState()
	{
		Pawn.SetPhysics(PHYS_Flying);
	}
}

function bool IsSpectating()
{
	return false;
}

state BaseSpectating
{
	function bool IsSpectating()
	{
		return true;
	}

	function ProcessMove(float DeltaTime, vector NewAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
	{
		local float		VelSize;

		/* smoothly accelerate and decelerate */
		Acceleration = Normal(NewAccel) * class'Pawn'.Default.AirSpeed;

		VelSize = VSize(Velocity);
		if ( VelSize > 0 )
			Velocity = Velocity - (Velocity - Normal(Acceleration) * VelSize) * FMin(DeltaTime * 8, 1);
		Velocity = Velocity + Acceleration * DeltaTime;
		if ( VSize(Velocity) > class'Pawn'.Default.AirSpeed )
			Velocity = Normal(Velocity) * class'Pawn'.default.AirSpeed;
		if ( VSize(Velocity) > 0 )
			MoveSmooth( (1+bRun) * Velocity * DeltaTime );
	}

	function PlayerMove(float DeltaTime)
	{
        local vector X,Y,Z;

		GetAxes(Rotation,X,Y,Z);
		Acceleration = PlayerInput.aForward*X + PlayerInput.aStrafe*Y + PlayerInput.aUp*vect(0,0,1);
		UpdateRotation( DeltaTime );

		if ( Role < ROLE_Authority ) // then save this move and replicate it
            ReplicateMove(DeltaTime, Acceleration, DCLICK_None, rot(0,0,0));
		else
            ProcessMove(DeltaTime, Acceleration, DCLICK_None, rot(0,0,0));
	}
}

function ServerViewNextPlayer()
{
    local Controller C, Pick;
	local bool bFound, bRealSpec;

    bRealSpec = PlayerReplicationInfo.bOnlySpectator;
    PlayerReplicationInfo.bOnlySpectator = true;

	// view next player
	for ( C=Level.ControllerList; C!=None; C=C.NextController )
	{
        if ( Level.Game.CanSpectate(self,bRealSpec,C) )
		{
			if ( Pick == None )
                Pick = C;
			if ( bFound )
			{
                Pick = C;
				break;
			}
			else
                bFound = ( ViewTarget == C );
		}
	}
	SetViewTarget(Pick);
    ClientSetViewTarget(Pick);
/*FIXME Joe
	if ( ViewTarget == self )
		bBehindView = false;
	else
		bBehindView = true; //bChaseCam;
*/
    PlayerReplicationInfo.bOnlySpectator = bRealSpec;
}

function ServerViewSelf()
{
	ResetCameraMode();
    SetViewTarget( Self );
    ClientSetViewTarget( Self );
	ClientMessage(OwnCamera, 'Event');
}

state Spectating extends BaseSpectating
{
	ignores RestartLevel, ClientRestart, Suicide, ThrowWeapon, NotifyPhysicsVolumeChange, NotifyHeadVolumeChange;

	exec function StartFire( optional byte FireModeNum )
	{
		ServerViewNextPlayer();
	}

	// Return to spectator's own camera.
	exec function StartAltFire( optional byte FireModeNum )
	{
        ResetCameraMode();
		ServerViewSelf();
	}

	function BeginState()
	{
		if ( Pawn != None )
		{
			SetLocation(Pawn.Location);
			UnPossess();
		}
		bCollideWorld = true;
	}

	function EndState()
	{
		PlayerReplicationInfo.bIsSpectator = false;
		bCollideWorld = false;
	}
}

/* dev exec for Shane. Hides Pawn, turns off HUD */
exec function ShaneMode()
{
	local bool bPawnHidden;

	myHUD.ToggleHud();
	bPawnHidden = bool(ConsoleCommand("get Pawn bHidden"));
	ConsoleCommand("Set Pawn bHidden" @ !bPawnHidden);
}

auto state PlayerWaiting extends BaseSpectating
{
ignores SeePlayer, HearNoise, NotifyBump, TakeDamage, PhysicsVolumeChange, NextWeapon, PrevWeapon, SwitchToBestWeapon;

	exec function Jump();

	exec function Suicide();

	function ChangeTeam( int N )
	{
        Level.Game.ChangeTeam(self, N, true);
	}

    function ServerRestartPlayer()
	{
		if ( Level.TimeSeconds < WaitDelay )
			return;
		if ( Level.NetMode == NM_Client )
			return;
		if ( Level.Game.bWaitingToStartMatch )
			PlayerReplicationInfo.bReadyToPlay = true;
		else
			Level.Game.RestartPlayer(self);
	}

	exec function StartFire( optional byte FireModeNum )
	{
/* FIXMESTEVE - load skins that haven't been precached (because players joined since game start)
        LoadPlayers();
        if ( !bForcePrecache && (Level.TimeSeconds > 0.2) )
*/
		ServerReStartPlayer();
	}

	function EndState()
	{
        if ( PlayerReplicationInfo != None )
		PlayerReplicationInfo.SetWaitingPlayer(false);
		bCollideWorld = false;
	}

	function BeginState()
	{
		if ( PlayerReplicationInfo != None )
			PlayerReplicationInfo.SetWaitingPlayer(true);
		bCollideWorld = true;
	}
}

state WaitingForPawn extends BaseSpectating
{
	ignores SeePlayer, HearNoise, KilledBy;

	exec function StartFire( optional byte FireModeNum )
	{
		AskForPawn();
	}

	function LongClientAdjustPosition
	(
		float TimeStamp,
		name newState,
		EPhysics newPhysics,
		float NewLocX,
		float NewLocY,
		float NewLocZ,
		float NewVelX,
		float NewVelY,
		float NewVelZ,
		Actor NewBase,
		float NewFloorX,
		float NewFloorY,
		float NewFloorZ
	)
	{
		if ( newState == 'GameEnded' || newState == 'RoundEnded' )
			GotoState(newState);
	}

	function PlayerTick(float DeltaTime)
	{
		Global.PlayerTick(DeltaTime);

		if ( Pawn != None )
		{
			Pawn.Controller = self;
            Pawn.bUpdateEyeHeight = true;
			ClientRestart(Pawn);
		}
        else if ( !IsTimerActive() || GetTimerCount() > 1.f )
		{
			SetTimer(0.2,true);
			AskForPawn();
		}
	}

	function Timer()
	{
		AskForPawn();
	}

	function BeginState()
	{
		SetTimer(0.2, true);
        AskForPawn();
	}

	function EndState()
	{
ResetCameraMode();
		SetTimer(0.0, false);
	}
}

state GameEnded
{
ignores SeePlayer, HearNoise, KilledBy, NotifyBump, HitWall, NotifyHeadVolumeChange, NotifyPhysicsVolumeChange, Falling, TakeDamage, Suicide;

	function ServerReStartPlayer()
	{
	}

	function bool IsSpectating()
	{
		return true;
	}

	exec function ThrowWeapon() {}
	exec function Use() {}

	function Possess(Pawn aPawn)
	{
		Global.Possess(aPawn);

		if (Pawn != None)
			Pawn.TurnOff();
	}

	function ServerReStartGame()
	{
		if ( Level.Game.PlayerCanRestartGame( Self ) )
			Level.Game.RestartGame();
	}

	exec function StartFire( optional byte FireModeNum )
	{
		if ( Role < ROLE_Authority)
			return;
		if ( !bFrozen )
			ServerReStartGame();
		else if ( !IsTimerActive() )
			SetTimer(1.5, false);
	}

	function PlayerMove(float DeltaTime)
	{
		local vector X,Y,Z;
		local Rotator DeltaRot, ViewRotation;

		GetAxes(Rotation,X,Y,Z);
		// Update view rotation.

		if ( !bFixedCamera )
		{
			ViewRotation = Rotation;
		// Calculate Delta to be applied on ViewRotation
		DeltaRot.Yaw	= PlayerInput.aTurn;
		DeltaRot.Pitch	= PlayerInput.aLookUp;
		ProcessViewRotation( DeltaTime, ViewRotation, DeltaRot );
		SetRotation(ViewRotation);
		}
		else if ( ViewTarget != None )
			SetRotation(ViewTarget.Rotation);

		ViewShake(DeltaTime);

		if ( Role < ROLE_Authority ) // then save this move and replicate it
			ReplicateMove(DeltaTime, vect(0,0,0), DCLICK_None, rot(0,0,0));
		else
			ProcessMove(DeltaTime, vect(0,0,0), DCLICK_None, rot(0,0,0));
		bPressedJump = false;
	}

	function ServerMove
	(
		float TimeStamp,
		vector InAccel,
		vector ClientLoc,
		byte NewFlags,
		byte ClientRoll,
		int View
	)
	{
        Global.ServerMove(TimeStamp, InAccel, ClientLoc, NewFlags, ClientRoll, (32767 & (Rotation.Pitch/2)) * 32768 + (32767 & (Rotation.Yaw/2)) );

	}

	function FindGoodView()
	{
		local vector cameraLoc;
		local rotator cameraRot, ViewRotation;
		local int tries, besttry;
		local float bestdist, newdist;
		local int startYaw;

		ViewRotation = Rotation;
		ViewRotation.Pitch = 56000;
		tries = 0;
		besttry = 0;
		bestdist = 0.0;
		startYaw = ViewRotation.Yaw;

		for (tries=0; tries<16; tries++)
		{
			cameraLoc = ViewTarget.Location;
			SetRotation(ViewRotation);
			GetPlayerViewPoint( cameraLoc, cameraRot );
			newdist = VSize(cameraLoc - ViewTarget.Location);
			if (newdist > bestdist)
			{
				bestdist = newdist;
				besttry = tries;
			}
			ViewRotation.Yaw += 4096;
		}

		ViewRotation.Yaw = startYaw + besttry * 4096;
		SetRotation(ViewRotation);
	}

	function Timer()
	{
		bFrozen = false;
	}

	function LongClientAdjustPosition
	(
		float TimeStamp,
		name newState,
		EPhysics newPhysics,
		float NewLocX,
		float NewLocY,
		float NewLocZ,
		float NewVelX,
		float NewVelY,
		float NewVelZ,
		Actor NewBase,
		float NewFloorX,
		float NewFloorY,
		float NewFloorZ
	)
	{
	}

	function BeginState()
	{
		local Pawn P;

		EndZoom();
        FOVAngle = DesiredFOV;
		bFire = 0;
		if ( Pawn != None )
		{
			Pawn.TurnOff(); // FIXMESTEVE- pawn should also be responsible for stopping weapons, animation
			Pawn.bSpecialHUD = false;
            StopFiring();
		}
		myHUD.bShowScores = true;
		bFrozen = true;
		if ( !bFixedCamera )
		{
			FindGoodView();
            //FIXMESTEVE bBehindView = true;
		}
        SetTimer(5, false);
		ForEach DynamicActors(class'Pawn', P)
		{
			if ( P.Role == ROLE_Authority )
				P.RemoteRole = ROLE_SimulatedProxy;
			P.TurnOff();
		}
	}

Begin:
}

state Dead
{
	ignores SeePlayer, HearNoise, KilledBy, NextWeapon, PrevWeapon;

	exec function ThrowWeapon()
	{
		//clientmessage("Throwweapon while dead, pawn "$Pawn$" health "$Pawn.health);
	}

	function bool IsDead()
	{
		return true;
	}

	function ServerReStartPlayer()
	{
		if ( !Level.Game.PlayerCanRestart( Self ) )
			return;

		super.ServerRestartPlayer();
	}

	exec function StartFire( optional byte FireModeNum )
	{
		if ( bFrozen )
		{
			if ( !IsTimerActive() || GetTimerCount() > 1.f )
				bFrozen = false;
			return;
		}

        // FIXMESTEVE LoadPlayers();
		ServerReStartPlayer();
	}

	function ServerMove
	(
		float TimeStamp,
		vector Accel,
		vector ClientLoc,
		byte NewFlags,
		byte ClientRoll,
		int View
	)
	{
		Global.ServerMove(
					TimeStamp,
					Accel,
					ClientLoc,
					0,
					ClientRoll,
					View);
	}

	function PlayerMove(float DeltaTime)
	{
		local vector X,Y,Z;
        local rotator DeltaRot, ViewRotation;

		if ( !bFrozen )
		{
			if ( bPressedJump )
			{
				StartFire( 0 );
				bPressedJump = false;
			}
			GetAxes(Rotation,X,Y,Z);
			// Update view rotation.
			ViewRotation = Rotation;
			// Calculate Delta to be applied on ViewRotation
			DeltaRot.Yaw	= PlayerInput.aTurn;
			DeltaRot.Pitch	= PlayerInput.aLookUp;
			ProcessViewRotation( DeltaTime, ViewRotation, DeltaRot );
			SetRotation(ViewRotation);
			if ( Role < ROLE_Authority ) // then save this move and replicate it
					ReplicateMove(DeltaTime, vect(0,0,0), DCLICK_None, rot(0,0,0));
		}
        else if ( !IsTimerActive() || GetTimerCount() > 1.f )
			bFrozen = false;

		ViewShake(DeltaTime);
	}

	function FindGoodView()
	{
		/*FIXMESTEVE
		local vector cameraLoc;
		local rotator cameraRot, ViewRotation;
		local int tries, besttry;
		local float bestdist, newdist;
		local int startYaw;

		////log("Find good death scene view");
		ViewRotation = Rotation;
		ViewRotation.Pitch = 56000;
		tries = 0;
		besttry = 0;
		bestdist = 0.0;
		startYaw = ViewRotation.Yaw;

		for (tries=0; tries<16; tries++)
		{
			cameraLoc = ViewTarget.Location;
			SetRotation(ViewRotation);
			GetPlayerViewPoint( cameraLoc, cameraRot );
			newdist = VSize(cameraLoc - ViewTarget.Location);
			if (newdist > bestdist)
			{
				bestdist = newdist;
				besttry = tries;
			}
			ViewRotation.Yaw += 4096;
		}

		ViewRotation.Yaw = startYaw + besttry * 4096;
		SetRotation(ViewRotation);
		*/
	}

	function Timer()
	{
		if (!bFrozen)
			return;

		bFrozen = false;
		bPressedJump = false;
	}

	function BeginState()
	{
		if ( (Pawn != None) && (Pawn.Controller == self) )
			Pawn.Controller = None;
		Pawn = None;
		EndZoom();
		FOVAngle = DesiredFOV;
		Enemy = None;
		bFrozen = true;
		bJumpStatus = false;
		bPressedJump = false;
		FindGoodView();
        SetTimer(1.0, false);
		CleanOutSavedMoves();
	}

	function EndState()
	{
		CleanOutSavedMoves();
		Velocity = vect(0,0,0);
		Acceleration = vect(0,0,0);
        if ( !PlayerReplicationInfo.bOutOfLives )
			ResetCameraMode();
		bPressedJump = false;
        if ( myHUD != None )
		{
			myHUD.bShowScores = false;
		}
		// FIXMESTEVE		StopViewShaking();
	}

Begin:
	if ( LocalPlayer(Player) != None )
	{
		Sleep(3.0);
		if ( myHUD != None )
		{
			myHUD.bShowScores = true;
		}
	}
}

function bool CanRestartPlayer()
{
    return !PlayerReplicationInfo.bOnlySpectator;
}

exec function NoGun()
{
	if ( Pawn == None || Pawn.Weapon == None )
    	return;

    Pawn.Weapon.Destroy();
}

exec function SetX(int newx)
{
	local vector v;
    v.X = NewX;
	Pawn.SetMeshTransformOffset(v);
}

exec function Sety(int newy)
{
	local vector v;
    v.Y = NewY;
	Pawn.SetMeshTransformOffset(v);
}

exec function Setz(int newz)
{
	local vector v;
    v.z = NewZ;
	Pawn.SetMeshTransformOffset(v);
}

exec function showpp()
{
    log("### Pawn.PrePivot"$Pawn.PrePivot@Pawn.CylinderComponent.CollisionHeight);
}

/**
 * Hook called from HUD actor. Gives access to HUD and Canvas
 */
function DrawHUD( HUD H )
{
	if ( Pawn != None )
	{
    	Pawn.DrawHUD( H );
	}

	PlayerInput.DrawHUD( H );
}

/* epic ===============================================
* ::OnToggleInput
*
* Looks at the activated input from the SeqAct_ToggleInput
* op and sets bPlayerInputEnabled accordingly.
*
* =====================================================
*/
simulated function OnToggleInput(SeqAct_ToggleInput inAction)
{
	if (inAction.InputLinks[0].bHasImpulse)
	{
		bMovementInputEnabled = true;
	}
	else
	if (inAction.InputLinks[1].bHasImpulse)
	{
		bMovementInputEnabled = false;
	}
	else
	if (inAction.InputLinks[2].bHasImpulse)
	{
		bMovementInputEnabled = !bMovementInputEnabled;
	}
}

/** 
 * list important PlayerController variables on canvas.  HUD will call DisplayDebug() on the current ViewTarget when
 * the ShowDebug exec is used
 *
 * @param	HUD		- HUD with canvas to draw on
 * @input	out_YL		- Height of the current font
 * @input	out_YPos	- Y position on Canvas. out_YPos += out_YL, gives position to draw text for next debug line.
 */
function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	super.DisplayDebug(HUD, out_YL, out_YPos);

	if ( PlayerCamera != None )
		PlayerCamera.DisplayDebug( HUD, out_YL, out_YPos );
	else
	{
		HUD.Canvas.SetDrawColor(255,0,0);
		HUD.Canvas.DrawText("NO CAMERA");
		out_YPos += out_YL;
		HUD.Canvas.SetPos(4, out_YPos);
	}
}

/* epic ===============================================
* ::OnSetCameraTarget
*
* Sets the specified view target.
*
* =====================================================
*/
simulated function OnSetCameraTarget(SeqAct_SetCameraTarget inAction)
{
	SetViewTarget( inAction.CameraTarget );
}

simulated function OnToggleHUD(SeqAct_ToggleHUD inAction)
{
	if (myHUD != None)
	{
		if (inAction.InputLinks[0].bHasImpulse)
		{
			myHUD.bShowHUD = true;
		}
		else
		if (inAction.InputLinks[1].bHasImpulse)
		{
			myHUD.bShowHUD = false;
		}
		else
		if (inAction.InputLinks[2].bHasImpulse)
		{
			myHUD.bShowHUD = !myHUD.bShowHUD;
		}
	}
}

/**
 * Attempts to match the name passed in to a SeqEvent_Console
 * object and then activate it.
 * 
 * @param		eventName - name of the event to cause
 */
exec function CauseEvent(Name inEventName)
{
	local array<SequenceObject> AllConsoleEvents;
	local SeqEvent_Console consoleEvt;
	local Sequence gameSeq;
	local int idx;

	// Get the gameplay sequence for the level.
	gameSeq = Level.GetLevelSequence();
	if(gameSeq != None)
	{
		// Find all SeqEvent_Console objects anywhere.
		gameSeq.FindSeqObjectsByClass( class'Engine.SeqEvent_Console', AllConsoleEvents);

		// Iterate over them, seeing if the name is the one we typed in.
		for( idx=0; idx < AllConsoleEvents.Length; idx++ )
		{
			consoleEvt = SeqEvent_Console(AllConsoleEvents[idx]);
			if (consoleEvt != None &&
				inEventName == consoleEvt.ConsoleEventName)
			{
				// activate the vent
				ActivateEvent(consoleEvt,Pawn);
			}
		}
	}
}

/**
 * Notification from pawn that it has received damage
 * via TakeDamage().
 */
function NotifyTakeHit(pawn InstigatedBy, vector HitLocation, int Damage,
	class<DamageType> damageType, vector Momentum)
{
	Super.NotifyTakeHit(InstigatedBy,HitLocation,Damage,damageType,Momentum);

	// Play waveform
	ClientPlayForceFeedbackWaveform(damageType.default.DamagedFFWaveform);
}

/**
 * Tells the client to play a waveform for the specified damage type
 *
 * @param FFWaveform The forcefeedback waveform to play
 */
simulated final function ClientPlayForceFeedbackWaveform(ForceFeedbackWaveform FFWaveform)
{
	if (ForceFeedbackManager != None)
	{
		ForceFeedbackManager.PlayForceFeedbackWaveform(FFWaveform);
	}
}

/**
 * Tells the client to stop any waveform that is playing. Note if the optional
 * parameter is passed in, then the waveform is only stopped if it matches
 *
 * @param FFWaveform The forcefeedback waveform to stop
 */
simulated final function ClientStopForceFeedbackWaveform(optional ForceFeedbackWaveform FFWaveform)
{
	if (ForceFeedbackManager != None)
	{
		ForceFeedbackManager.StopForceFeedbackWaveform(FFWaveform);
	}
}

/**
 * Camera Shake
 * Plays camera shake effect
 *
 * @param	Duration			Duration in seconds of shake
 * @param	newRotAmplitude		view rotation amplitude (pitch,yaw,roll)
 * @param	newRotFrequency		frequency of rotation shake
 * @param	newLocAmplitude		relative view offset amplitude (x,y,z)
 * @param	newLocFrequency		frequency of view offset shake
 * @param	newFOVAmplitude		fov shake amplitude
 * @param	newFOVFrequency		fov shake frequency
 */
function CameraShake
( 
	float	Duration, 
	vector	newRotAmplitude, 
	vector	newRotFrequency,
	vector	newLocAmplitude, 
	vector	newLocFrequency, 
	float	newFOVAmplitude,
	float	newFOVFrequency
);

defaultproperties
{
	bAimingHelp=false
	FOVAngle=85.000
	DesiredFOV=85.000000
	DefaultFOV=85.000000
	ViewingFrom="Now viewing from"
	OwnCamera="Now viewing from own camera"
	QuickSaveString="Quick Saving"
	NoPauseMessage="Game is not pauseable"
	bTravel=True
	bStasis=False
	NetPriority=3
	LocalMessageClass=class'LocalMessage'
	bIsPlayer=true
	bCanOpenDoors=true
	bCanDoSpecial=true
	Physics=PHYS_None
	EnemyTurnSpeed=45000
	CheatClass=class'Engine.CheatManager'
	InputClass=class'Engine.PlayerInput'
	ConsoleClass=class'Engine.Console'
	bZeroRoll=true
	bDynamicNetSpeed=true
	ProgressTimeOut=8.0
	InteractDistance=512

	Begin Object Name=CollisionCylinder
		CollisionRadius=+22.000000
		CollisionHeight=+22.000000
	End Object
	CylinderComponent=CollisionCylinder

	bMovementInputEnabled=true
	bTurningInputEnabled=true

	CameraClass=class'Camera'

	MaxResponseTime=0.125
	DynamicPingThreshold=+400.0
	ClientCap=0
	LastSpeedHackLog=-100.0
	SavedMoveClass=class'SavedMove'
}
