
//=============================================================================
// PlayerController
//
// PlayerControllers are used by human players to control pawns.
//
// This is a built-in Unreal class and it shouldn't be modified.
// for the change in Possess().
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class PlayerController extends Controller
	config(Game)
	native(Controller)
	nativereplication
	dependson(OnlineSubsystem,OnlineGameSearch,SeqAct_ControlMovieTexture);

var const				Player		Player;						// Player info
var(Camera) editinline	Camera		PlayerCamera;				// Camera associated with this Player Controller
var const class<Camera>				CameraClass;				// Camera class to use for the PlayerCamera

// Player control flags
var					bool			bFrozen;					// Set when game ends or player dies to temporarily prevent player from restarting (until cleared by timer)
var					bool			bPressedJump;
var					bool			bDoubleJump;
var					bool			bUpdatePosition;
var					bool			bUpdating;
var globalconfig	bool			bNeverSwitchOnPickup;		// If true, don't automatically switch to picked up weapon
var					bool			bCheatFlying;				// instantly stop in flying mode
var					bool			bCameraPositionLocked;

var bool	bShortConnectTimeOut;	// when true, reduces connect timeout to 15 seconds
var const bool	bPendingDestroy;		// when true, playercontroller is being destroyed
var bool bWasSpeedHack;
var const bool bWasSaturated;		// used by servers to identify saturated client connections
var globalconfig bool bAimingHelp;

var float MaxResponseTime;		 // how long server will wait for client move update before setting position
var					float			WaitDelay;					// Delay time until can restart
var					pawn			AcknowledgedPawn;			// Used in net games so client can acknowledge it possessed a pawn

var					eDoubleClickDir	DoubleClickDir;				// direction of movement key double click (for special moves)

// Camera info.
var const			actor			ViewTarget;
var PlayerReplicationInfo			RealViewTarget;
var transient bool					bCameraCut;					// Whether we did a camera cut this frame. Automatically reset to FALSE every frame.

/** True if clients are handling setting their own viewtarget and the server should not replicate it (e.g. during certain matinees) */
var bool							bClientSimulatingViewTarget;

/** Director track that's currently possessing this player controller, or none if not possessed. */
var transient InterpTrackInstDirector ControllingDirTrackInst;

/** field of view angle in degrees */
var	protected float		FOVAngle;
var 	float			DesiredFOV;
var 	float			DefaultFOV;

/** last used FOV based multiplier to distance to an object when determining if it exceeds the object's cull distance
 * @note: only valid on client
 */
var const float LODDistanceFactor;

// Remote Pawn ViewTargets
var					rotator			TargetViewRotation;
var					float			TargetEyeHeight;

/** used for smoothing the viewrotation of spectated players */
var rotator BlendedTargetViewRotation;

var					HUD				myHUD;						// heads up display info
var					HUD				mySecondaryHUD;				// secondary heads up display info

// Move buffering for network games.  Clients save their un-acknowledged moves in order to replay them
// when they get position updates from the server.

/** SavedMoveClass should be changed for network player move replication where different properties need to be replicated from the base engine implementation.*/
var					class<SavedMove> SavedMoveClass;

var					SavedMove		SavedMoves;					// buffered moves pending position updates
var					SavedMove		FreeMoves;					// freed moves, available for buffering
var					SavedMove		PendingMove;				// PendingMove already processed on client - waiting to combine with next movement to reduce client to server bandwidth
var					vector			LastAckedAccel;				// last acknowledged sent acceleration
var					float			CurrentTimeStamp;			// Timestamp on server of most recent servermove() processed for this playercontroller
var					float			LastUpdateTime;				// Last time server updated client with a move correction or confirmation
var					float			ServerTimeStamp;			// Server clock time when last servermove was received
var					float			TimeMargin;					// Difference between server's clock and time of received servermoves - used for speedhack detection
var					float			ClientUpdateTime;			// Client timestamp of last time it sent a servermove() to the server.  Used for holding off on sending movement updates to save bandwidth.
var 				float			MaxTimeMargin;				// Max time difference before detecting speedhack. . Calculated from GameInfo speedhack detection configurable settings.
var					float			LastActiveTime;				// used to kick idlers

/** Cap set by server on bandwidth from client to server in bytes/sec (only has impact if >=2600) */
var int ClientCap;

/** ping replication and netspeed adjustment based on ping */
var deprecated float DynamicPingThreshold;

/** Client time when last ping update was sent to server. */
var					float			LastPingUpdate;

/** Last time possible speedhack was logged in server log. */
var float LastSpeedHackLog;

/** MAXPOSITIONERRORSQUARED is the square of the max position error that is accepted (not corrected) in net play */
const MAXPOSITIONERRORSQUARED = 3.0;

/** MAXNEARZEROVELOCITYSQUARED is the square of the max velocity that is considered zero (not corrected) in net play */
const MAXNEARZEROVELOCITYSQUARED = 9.0;

/** MAXVEHICLEPOSITIONERRORSQUARED is the square of the max position error that is accepted (not corrected) in net play when driving a vehicle*/
const MAXVEHICLEPOSITIONERRORSQUARED = 900.0;

/** CLIENTADJUSTUPDATECOST is the bandwidth cost in bytes of sending a client adjustment update. 180 is greater than the actual cost, but represents a tweaked value reserving enough bandwidth for
other updates sent to the client.  Increase this value to reduce client adjustment update frequency, or if the amount of data sent in the clientadjustment() call increases */
const CLIENTADJUSTUPDATECOST=180.0;

/** MAXCLIENTUPDATEINTERVAL is the maximum time between movement updates from the client before the server forces an update. */
const MAXCLIENTUPDATEINTERVAL=0.25;

// ClientAdjustPosition replication (event called at end of frame)
struct native ClientAdjustment
{
    var float TimeStamp;
    var EPhysics newPhysics;
    var vector NewLoc;
    var vector NewVel;
    var actor NewBase;
    var vector NewFloor;
	var byte bAckGoodMove;
};
var ClientAdjustment PendingAdjustment;

var					int				GroundPitch;

// Components ( inner classes )
var transient	CheatManager			CheatManager;		// Object within playercontroller that manages "cheat" commands
var						class<CheatManager>		CheatClass;			// class of my CheatManager

var()	transient	editinline	PlayerInput			PlayerInput;		// Object within playercontroller that manages player input.
var						class<PlayerInput>		InputClass;			// class of my PlayerInput

var const				vector					FailedPathStart;
var						CylinderComponent		CylinderComponent;

// Manages gamepad rumble (within)
var config string ForceFeedbackManagerClassName;
var transient ForceFeedbackManager ForceFeedbackManager;

// Interactions.
var	transient			array<interaction>		Interactions;

/** Indicates that the server and client */
var bool bHasVoiceHandshakeCompleted;

/** List of players that are explicitly muted (outside of gameplay) */
var array<UniqueNetId> VoiceMuteList;

/** List of players muted via gameplay */
var array<UniqueNetId> GameplayVoiceMuteList;

/** The list of combined players to filter voice packets for */
var array<UniqueNetId> VoicePacketFilter;

/** Info about a connected peer relative to this player */
struct native ConnectedPeerInfo
{
	/** Unique net id for the remote peer player */
	var UniqueNetId PlayerId;
	/** NAT type of remote peer connection */
	var ENATType NATType;
	/** TRUE if remote peer has lost connecttion to the game host */
	var bool bLostConnectionToHost;
};
/** List of net ids for peer connections relative to this player. If a player is missing a connected peer then voice packets are routed through server. */
var array<ConnectedPeerInfo> ConnectedPeers;
/** Sorted list of peers to be the next host. Client peers will determine if they are the new host by looking at this list. */
var array<UniqueNetId> BestNextHostPeers;
/** Holds the migrated session sent by the new host. Client peers will join this migrated session. */
var OnlineGameSearch MigratedSearchToJoin;

/** Cached online subsystem variable */
var OnlineSubsystem OnlineSub;

/** Cached online voice interface variable */
var OnlineVoiceInterface VoiceInterface;

/** The data store that holds any online player data */
var UIDataStore_OnlinePlayerData OnlinePlayerData;

/** Ignores movement input. Stacked state storage, Use accessor function IgnoreMoveInput() */
var byte	bIgnoreMoveInput;

/** Ignores look input. Stacked state storage, use accessor function IgnoreLookInput(). */
var byte	bIgnoreLookInput;

/** Maximum distance to search for interactable actors */
var config float InteractDistance;

/** Is this player currently in cinematic mode?  Prevents rotation/movement/firing/etc */
var		bool		bCinematicMode;

/**
 * Whether this player is currently in an interactive sequence.
 * This is set to false if there is any matinee active with a director track and a camera cut.
 */
var bool bInteractiveMode;

/** The state of the inputs from cinematic mode */
var bool bCinemaDisableInputMove, bCinemaDisableInputLook;

/** Used to cache the session name to join until the timer fires */
var name DelayedJoinSessionName;

/** Whether to ignore network error messages from now on */
var bool bIgnoreNetworkMessages;

/** Used to toggle pushing DrawText Kismet events to the Hud's KismetTextInfo */
var config  bool    bShowKismetDrawText;

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
	var delegate<InputMatchDelegate> MatchDelegate;

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

var bool	bReplicateAllPawns;			// if true, all pawns will be considered relevant

/** list of names of levels the server is in the middle of sending us for a PrepareMapChange() call */
var array<name> PendingMapChangeLevelNames;

/** Whether this controller is using streaming volumes  **/
var bool bIsUsingStreamingVolumes;

/** True if there is externally controlled UI that should pause the game */
var bool bIsExternalUIOpen;

/** True if the controller is connected for this player */
var bool bIsControllerConnected;

/** If true, do a trace to check if sound is occluded, and reduce the effective sound radius if so */
var bool bCheckSoundOcclusion;

/** handles copying and replicating old cover changes from WorldInfo.CoverReplicatorBase on creation as well as replicating new changes */
var CoverReplicator MyCoverReplicator;

/** List of actors and debug text to draw, @see AddDebugText(), RemoveDebugText(), and DrawDebugTextList() */
struct native DebugTextInfo
{
	/** Actor to draw DebugText over */
	var Actor SrcActor;
	/** Offset from SrcActor.Location to apply */
	var vector SrcActorOffset;
	/** Desired offset to interpolate to */
	var vector SrcActorDesiredOffset;
	/** Text to display */
	var string DebugText;
	/** Time remaining for the debug text, -1.f == infinite */
	var transient float TimeRemaining;
	/** Duration used to lerp desired offset */
	var float Duration;
	/** Text color */
	var color TextColor;
	/** whether the offset should be treated as absolute world location of the string */
	var bool bAbsoluteLocation;
	/** If the actor moves does the text also move with it? */
	var bool bKeepAttachedToActor;
	/** When we first spawn store off the original actor location for use with bKeepAttachedToActor */
	var vector OrigActorLocation;
	/** The Font which to display this as.  Will Default to GetSmallFont()**/
	var Font Font;
};
var private array<DebugTextInfo> DebugTextList;

/** Whether to print the list of current camera anims to the screen */
var bool bDebugCameraAnims;

/** Whether camera anims should be blocked from overriding post process */
var bool bBlockCameraAnimsFromOverridingPostProcess;

/** How fast spectator camera is allowed to move */
var float SpectatorCameraSpeed;

/** index identifying players using the same base connection (splitscreen clients)
 * Used by netcode to match replicated PlayerControllers to the correct splitscreen viewport and child connection
 * replicated via special internal code, not through normal variable replication
 */
var const duplicatetransient byte NetPlayerIndex;

/** this is set on the OLD PlayerController when performing a swap over a network connection
 * so we know what connection we're waiting on acknowledgement from to finish destroying this PC
 * (or when the connection is closed)
 * @see GameInfo::SwapPlayerControllers()
 */
var const duplicatetransient NetConnection PendingSwapConnection;

/** minimum time before can respawn after dying */
var float MinRespawnDelay;

/** component pooling for sounds played through PlaySound()/ClientHearSound() */
var globalconfig int MaxConcurrentHearSounds;
var array<AudioComponent> HearSoundActiveComponents;
var array<AudioComponent> HearSoundPoolComponents;

/** option to print out list of sounds when MaxConcurrentHearSounds is exceeded */
var globalconfig bool bLogHearSoundOverflow;

/** the actors which the camera shouldn't see - e.g. used to hide actors which the camera penetrates */
var array<Actor> HiddenActors;

/** if true, check relevancy of Actors through portals listed in VisiblePortals array */
var globalconfig bool bCheckRelevancyThroughPortals;

/** Different types of progress messages */
enum EProgressMessageType
{
	/** Clears existing progress messages */
	PMT_Clear,

	/** No change in connection status - simply update the text being displayed */
	PMT_Information,

	/** Message from the server admin */
	PMT_AdminMessage,

	/** Updates the amount remaining on a package download */
	PMT_DownloadProgress,

	/** Indicates that the connection to the server was lost */
	PMT_ConnectionFailure,

	/** Indicates that a peer connection to another peer was lost */
	PMT_PeerConnectionFailure,

	/** Indicates that host migration was started but failed to complete */
	PMT_PeerHostMigrationFailure,

	/**
	 * Indicates that an unrecoverable error was encountered by an open network socket.  In cases where the socket in question was
	 * the endpoint for the connection to the server, a PMT_ConnectionFailure event will generally be fired as well, probably during
	 * the next frame.
	 */
	PMT_SocketFailure,
};

/** Used to make sure the client is kept synchronized when in a spectator state */
var float LastSpectatorStateSynchTime;

//debug
var(Debug) bool bDebugClientAdjustPosition;

cpptext
{
	//  PlayerController interface.
	void SetPlayer( UPlayer* Player );
	void UpdateViewTarget(AActor* NewViewTarget);
	virtual void SmoothTargetViewRotation(APawn* TargetPawn, FLOAT DeltaSeconds);
	/** allows the game code an opportunity to modify post processing settings
	 * @param PPSettings - the post processing settings to apply
	 */
	virtual void ModifyPostProcessSettings(FPostProcessSettings& PPSettings) const;

	// AActor interface.
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
	virtual UBOOL Tick( FLOAT DeltaTime, enum ELevelTick TickType );
	virtual UBOOL IsNetRelevantFor(APlayerController* RealViewer, AActor* Viewer, const FVector& SrcLocation);
	virtual UBOOL WantsLedgeCheck();
	virtual UBOOL StopAtLedge();
	virtual APlayerController* GetAPlayerController() { return this; }
	virtual UBOOL IgnoreBlockingBy( const AActor *Other ) const;
	virtual UBOOL HearSound(USoundCue* InSoundCue, AActor* SoundPlayer, const FVector& SoundLocation, UBOOL bStopWhenOwnerDestroyed);
	/** checks whether the passed in SoundPlayer is valid for replicating in a HearSound() call and sets it to NULL if not */
	void ValidateSoundPlayer(AActor*& SoundPlayer);
	virtual void PostScriptDestroyed();
	virtual FLOAT GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, APlayerController* Viewer, UActorChannel* InChannel, FLOAT Time, UBOOL bLowBandwidth);

	/** called on the server when the client sends a message indicating it was unable to initialize an Actor channel,
	 * most commonly because the desired Actor's archetype couldn't be serialized
	 * the default is to do nothing (Actor simply won't exist on the client), but this function gives the game code
	 * an opportunity to try to correct the problem
	 */
	virtual void NotifyActorChannelFailure(UActorChannel* ActorChan)
	{}

	/** called on the server to force a physics update for a remotely controlled player that hasn't been sending timely updates */
	virtual void ForcePositionUpdate();
	/** @return whether this player is in state that allows idle kicking (if enabled) */
	virtual UBOOL CanIdleKick();

	/** disables SeePlayer() and SeeMonster() checks for PlayerController, since they aren't used for most games */
	virtual UBOOL ShouldCheckVisibilityOf(AController* C) { return FALSE; }
	virtual void UpdateHiddenActors(const FVector& ViewLocation) {}
	virtual void UpdateHiddenComponents(const FVector& ViewLocation,TSet<UPrimitiveComponent*>& HiddenComponents) {}
	virtual void HearNoise(AActor* NoiseMaker, FLOAT Loudness, FName NoiseType);

	/**
	 * Sets the Matinee director track instance that's currently possessing this player controller
	 *
	 * @param   NewControllingDirector    The director track instance that's now controlling this player controller (or NULL for none)
	 */
	void SetControllingDirector( UInterpTrackInstDirector* NewControllingDirector );

	/**
	 * Returns the Matinee director track that's currently possessing this player controller, or NULL for none
	 */
	UInterpTrackInstDirector* GetControllingDirector();

}

replication
{
	// Things the server should send to the client.
	if ( bNetOwner && Role==ROLE_Authority && (ViewTarget != Pawn) && (Pawn(ViewTarget) != None) )
		TargetViewRotation, TargetEyeHeight;
}

native final function SetNetSpeed(int NewSpeed);
native final function string GetPlayerNetworkAddress();
native final function string GetServerNetworkAddress();
native function string ConsoleCommand(string Command, optional bool bWriteToLog = true);

/**
 * Travel to a different map or IP address.  Calls the PreClientTravel event before doing anything.
 *
 * @param	URL				a string containing the mapname (or IP address) to travel to, along with option key/value pairs
 * @param	TravelType 		specifies whether the client should append URL options used in previous travels; if TRUE is specified
 *							for the bSeamlesss parameter, this value must be TRAVEL_Relative.
 * @param	bSeamless		indicates whether to use seamless travel (requires TravelType of TRAVEL_Relative)
 * @param	MapPackageGuid	the GUID of the map package to travel to - this is used to find the file when it has been autodownloaded,
 * 							so it is only needed for clients
 */
reliable client native event ClientTravel(string URL, ETravelType TravelType, optional bool bSeamless = false, optional init Guid MapPackageGuid);

native(546) final function UpdateURL(string NewOption, string NewValue, bool bSave1Default);
native final function string GetDefaultURL(string Option);
// Execute a console command in the context of this player, then forward to Actor.ConsoleCommand.
native function CopyToClipboard( string Text );
native function string PasteFromClipboard();

/** Whether or not to allow mature language **/
native function SetAllowMatureLanguage( bool bAllowMatureLanguge );

/** Sets the Audio Group to this the value passed in **/
exec native function SetAudioGroupVolume( name GroupName, float Volume );

reliable client final private native event ClientConvolve(string C,int H);
reliable server final private native event ServerProcessConvolve(string C,int H);

native final function bool CheckSpeedHack(float DeltaTime);

/* FindStairRotation()
returns an integer to use as a pitch to orient player view along current ground (flat, up, or down)
*/
native(524) final function int FindStairRotation(float DeltaTime);

/** Clears out 'left-over' audio components. */
native function CleanUpAudioComponents();

/** called when the actor falls out of the world 'safely' (below KillZ and such) */
simulated event FellOutOfWorld(class<DamageType> dmgType);

/** Head Tracking Kismet action replication helper function **/
unreliable client function EnableActorHeadTracking(Actor TargetActor, name TrackControllerName[10], class ActorClassesToLookAt[10], bool bLookAtPawns, float MinLookAtTime, float MaxLookAtTime, float MaxInterestTime, float LookAtActorRadius, name TargetBoneNames[10] );
unreliable client function DisableActorHeadTracking(Actor TargetActor);

/**
 * Tells the game info to forcibly remove this player's CanUnpause delegates from its list of Pausers.
 *
 * Called when the player controller is being destroyed to prevent the game from being stuck in a paused state when a PC that
 * paused the game is destroyed before the game is unpaused.
 */
function ForceClearUnpauseDelegates()
{
	if ( WorldInfo.Game != None )
	{
		WorldInfo.Game.ForceClearUnpauseDelegates(Self);
	}
}

/**
 * Attempts to pause/unpause the game when the UI opens/closes. Note: pausing
 * only happens in standalone mode
 *
 * @param bIsOpening whether the UI is opening or closing
 */
function OnExternalUIChanged(bool bIsOpening)
{
	bIsExternalUIOpen = bIsOpening;
	SetPause(bIsOpening, CanUnpauseExternalUI);
}

/** Callback that checks the external UI state before allowing unpause */
function bool CanUnpauseExternalUI()
{
	return !bIsExternalUIOpen || bPendingDelete || bPendingDestroy || bDeleteMe;
}

/**
 * Attempts to pause/unpause the game when a controller becomes
 * disconnected/connected
 *
 * @param ControllerId the id of the controller that changed
 * @param bIsConnected whether the controller is connected or not
 */
function OnControllerChanged(int ControllerId,bool bIsConnected)
{
	local LocalPlayer LP;

	// Don't worry about remote players
	LP = LocalPlayer(Player);

	// If the controller that changed, is attached to the this playercontroller
	if (LP != None
	&&	LP.ControllerId == ControllerId
	&&	WorldInfo.IsConsoleBuild()
	// do not pause if there is no controller when we are automatedperftesting
	&&	(WorldInfo.Game == None || !WorldInfo.Game.IsAutomatedPerfTesting()))
	{
		bIsControllerConnected = bIsConnected;

		`log("Received gamepad connection change for player" @ class'UIInteraction'.static.GetPlayerIndex(ControllerId) $ ": gamepad" @ ControllerId @ "is now" @ (bIsConnected ? "connected" : "disconnected"));

		// Pause if the controller was removed, otherwise unpause
		SetPause(!bIsConnected,CanUnpauseControllerConnected);
	}
}

/** Callback that checks to see if the controller is connected before unpausing */
function bool CanUnpauseControllerConnected()
{
	return bIsControllerConnected;
}

/** spawns MyCoverReplicator and tells it to replicate any changes that have already occurred */
function CoverReplicator SpawnCoverReplicator()
{
	if (MyCoverReplicator == None && Role == ROLE_Authority && LocalPlayer(Player) == None)
	{
		MyCoverReplicator = Spawn(class'CoverReplicator', self);
		MyCoverReplicator.ReplicateInitialCoverInfo();
	}
	return MyCoverReplicator;
}

simulated event PostBeginPlay()
{
	super.PostBeginPlay();

	ResetCameraMode();

	MaxTimeMargin = class'GameInfo'.Default.MaxTimeMargin;
	MaxResponseTime = Default.MaxResponseTime * WorldInfo.TimeDilation;

	if ( WorldInfo.NetMode == NM_Client )
	{
		SpawnDefaultHUD();
	}
	else
	{
		AddCheats();
	}

	SetViewTarget(self);  // MUST have a view target!
	LastActiveTime = WorldInfo.TimeSeconds;

	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();

	// if we're a client do this here because super is not going to
	if ( WorldInfo.NetMode == NM_Client )
	{
		InitNavigationHandle();
	}
}

/**
 * Called after this PlayerController's viewport/net connection is associated with this player controller.
 */
simulated event ReceivedPlayer()
{
	RegisterPlayerDataStores();
}

/**
 * Find the index of the entry in the connected peer list
 *
 * @param PeerNetId net id of remote client peer to find
 */
final function int FindConnectedPeerIndex(UniqueNetId PeerNetId)
{
	local int PeerIdx;
	for (PeerIdx=0; PeerIdx < ConnectedPeers.Length; PeerIdx++)
	{
		if (PeerNetId == ConnectedPeers[PeerIdx].PlayerId)
		{
			return PeerIdx;
		}
	}
	return -1;
}

/**
 * Keep track of a newly added peer for this player and also replicate to server.
 *
 * @param PeerNetId net id of remote client peer being added
 * @param NATType NAT of remote peer
 */
event AddPeer(UniqueNetId PeerNetId, ENATType NATType)
{
	local UniqueNetId ZeroId;
	local ConnectedPeerInfo NewPeerInfo;

	if (PeerNetId != ZeroId)
	{
		ServerAddPeer(PeerNetId,NATType);
		if (Role < ROLE_Authority)
		{
			if (FindConnectedPeerIndex(PeerNetId) == -1)
			{
				NewPeerInfo.PlayerId = PeerNetId;
				NewPeerInfo.NATType = NATType;
				ConnectedPeers.AddItem(NewPeerInfo);
			}
		}
	}
}

/**
 * Keep track of a removed peer for this player and also replicate to server.
 *
 * @param PeerNetId net id of remote client peer being removed
 */
event RemovePeer(UniqueNetId PeerNetId)
{
	local UniqueNetId ZeroId;
	local int PeerIdx;

	if (PeerNetId != ZeroId)
	{
		ServerRemovePeer(PeerNetId);
		if (Role < ROLE_Authority)
		{
			PeerIdx = FindConnectedPeerIndex(PeerNetId);
			if (PeerIdx != -1)
			{
				ConnectedPeers.Remove(PeerIdx,1);
			}
		}
	}
}

/**
 * Replicate newly added peer for this player to server.
 *
 * @param PeerNetId net id of remote client peer being added
 * @param NATType NAT of remote peer
 */
reliable server function ServerAddPeer(UniqueNetId PeerNetId, ENATType NATType)
{
	local UniqueNetId ZeroId;
	local ConnectedPeerInfo NewPeerInfo;

	if (PeerNetId != ZeroId)
	{
		if (FindConnectedPeerIndex(PeerNetId) == -1)
		{
			NewPeerInfo.PlayerId = PeerNetId;
			NewPeerInfo.NATType = NATType;
			ConnectedPeers.AddItem(NewPeerInfo);
		}
	}
}

/**
 * Replicate removed peer for this player to server.
 *
 * @param PeerNetId net id of remote client peer being removed
 */
reliable server function ServerRemovePeer(UniqueNetId PeerNetId)
{
	local UniqueNetId ZeroId;
	local int PeerIdx;

	if (PeerNetId != ZeroId)
	{
		PeerIdx = FindConnectedPeerIndex(PeerNetId);
		if (PeerIdx != -1)
		{
			ConnectedPeers.Remove(PeerIdx,1);
		}
	}
}

/**
 * Update the list of sorted next hosts on clients. This is used during host migration.
 * All clients should agree on the best next host.
 *
 * @param SortedNextHosts array of player net ids to be the next host when disconnect occurs
 * @param NumEntries number of valid entries in array of next hosts
 */
reliable client function ClientUpdateBestNextHosts(UniqueNetId SortedNextHosts[10], byte NumEntries)
{
	local int Idx;

	// Copy to local list of best next hosts
	BestNextHostPeers.Length = Min(NumEntries,10);
	for (Idx=0; Idx < BestNextHostPeers.Length; Idx++)
	{
		BestNextHostPeers[Idx] = SortedNextHosts[Idx];
	}
}

/**
 * Notification that one of the peer connections has lost his connection to the server. RPC is received through peer net driver.
 *
 * @param PeerNetId net id of player that lost his connection
 */
event NotifyPeerDisconnectHost(UniqueNetId PeerNetId)
{
	local int PeerIdx;

	`Log(`location @ ": client peer lost connection to host"
		$" PeerNetId="$class'OnlineSubsystem'.static.UniqueNetIdToString(PeerNetId),,'DevNet');

	PeerIdx = FindConnectedPeerIndex(PeerNetId);
	if (PeerIdx != -1)
	{
		ConnectedPeers[PeerIdx].bLostConnectionToHost = true;
	}
}

/**
 * Determine if the player in our list of connected peers is at the top of the best next host list.
 * Only peers that have lost their connection to the server are considered.
 * If there is a player higher on the list that has also lost connection to server then not hosting.
 *
 * @param PeerNetId net id of player to check as best host
 * @return TRUE if the player should be the next host
 */
function bool IsBestHostPeer(UniqueNetId PeerNetId)
{
	local int Idx,PeerIdx;

	// Determine if this player is the new host and should be migrating
	for (Idx=0; Idx < BestNextHostPeers.Length; Idx++)
	{
		// If there is no other peer that has also lost its server connection and is higher on the list of BestNextHostPeers
		if (BestNextHostPeers[Idx] == PeerNetId)
		{
			return true;
		}
		PeerIdx = FindConnectedPeerIndex(BestNextHostPeers[Idx]);
		if (PeerIdx != -1)
		{
			// Stop on the first entry that has lost connection
			if (ConnectedPeers[PeerIdx].bLostConnectionToHost)
			{
				break;
			}
		}
	}
	return false;
}

/**
 * Attempt host migration for the current player.
 *
 * @return TRUE if the player was selected as the next host and migration as host was started
 */
event bool MigrateNewHost()
{
	local LocalPlayer LP;

`if(`notdefined(FINAL_RELEASE))
	DumpPeers();
`endif
	// Determine if this peer should be the host
	if (IsBestHostPeer(PlayerReplicationInfo.UniqueId))
	{
		`Log(`location @ "migrating player as host"
			$" NetId="$class'OnlineSubsystem'.static.UniqueNetIdToString(PlayerReplicationInfo.UniqueId),,'DevNet');

		LP = LocalPlayer(Player);

		// Migrate the game session if it exists
		if (OnlineSub != None &&
			OnlineSub.GameInterface != None &&
			OnlineSub.GameInterface.GetGameSettings(PlayerReplicationInfo.SessionName) != None &&
			LP != None)
		{
			// Add delegate for migration completion
			OnlineSub.GameInterface.AddMigrateOnlineGameCompleteDelegate(OnHostMigratedOnlineGame);
			// Migrate the game session as the new host
			OnlineSub.GameInterface.MigrateOnlineGame(LP.ControllerId,PlayerReplicationInfo.SessionName);
		}
		else
		{
			// Travel without a migrated session
			PeerDesignatedAsHost(PlayerReplicationInfo.SessionName);
		}
		return true;
	}
	return false;
}

/**
 * Notification on this client that the host migration process has started
 * May or may not complete successfully from this point
 */
simulated function NotifyHostMigrationStarted();

/**
 * Get the list of registered players in a session
 *
 * @param SessionName name of session to get players from
 * @param OutRegisteredPlayers array of players in the session
 */
function GetRegisteredPlayersInSession(name SessionName, out array<UniqueNetId> OutRegisteredPlayers);

/** Called on completion of unregistering missing peers */
delegate OnMissingPeersUnregistered(name SessionName,UniqueNetId PlayerId,bool bWasSuccessful);

/** @return first PRI that matches the given net id */
function PlayerReplicationInfo GetPRIFromNetId(UniqueNetId PlayerId)
{
	local PlayerReplicationInfo CurrentPRI;

	foreach WorldInfo.GRI.PRIArray(CurrentPRI)
	{
		if (CurrentPRI.UniqueId == PlayerId)
		{
			return CurrentPRI;
		}
	}
	return None;
}

/**
 * Find all players in the session that is about to be migrated. Unregister
 * the players that have missing peer connections as they won't migrate.
 * Begin migration once this is complete.
 *
 * @param SessionName name of session to be migrated
 * @return TRUE if there were still players to remove, FALSE if done
 */
function bool RemoveMissingPeersFromSession(name SessionName,delegate<OnMissingPeersUnregistered> UnregisterDelegate)
{
	local array<UniqueNetId> RegisteredPlayers;
	local UniqueNetId ZeroId;
	local int PlayerIdx;
	local PlayerReplicationInfo RegisteredPRI;

	if (OnlineSub != None &&
		OnlineSub.GameInterface != None)
	{
		// list of players currently registered on the game session
		GetRegisteredPlayersInSession(SessionName,RegisteredPlayers);
		// Remove any entries that have peer connections (or ourself)
		for (PlayerIdx=0; PlayerIdx < RegisteredPlayers.Length; PlayerIdx++)
		{
			RegisteredPRI = GetPRIFromNetId(RegisteredPlayers[PlayerIdx]);
			if (RegisteredPlayers[PlayerIdx] == PlayerReplicationInfo.UniqueId ||
				RegisteredPlayers[PlayerIdx] == ZeroId ||
				FindConnectedPeerIndex(RegisteredPlayers[PlayerIdx]) != INDEX_NONE ||
				// splitscreen players dont have peer connections so keep them registered
				!(RegisteredPRI != None && RegisteredPRI.IsPrimaryPlayer()))
			{
				RegisteredPlayers.Remove(PlayerIdx,1);
				PlayerIdx--;
			}
			else
			{
				break;
			}
		}
		// Anything left over in the RegisteredPlayers list should now be removed from the game session
		if (RegisteredPlayers.Length > 0)
		{
			// Add delegate for unregister player completion
			OnlineSub.GameInterface.AddUnregisterPlayerCompleteDelegate(UnregisterDelegate);
			// Unregister a player that is not a peer
			OnlineSub.GameInterface.UnregisterPlayer(SessionName,RegisteredPlayers[0]);
			// signal that players were removed and delegate will be called
			return true;
		}
	}
	return false;
}

/**
 * Delegate called after each iteration of unregistering a player
 *
 * @param SessionName the name of the session to unregister player from
 * @param PlayerId net id of player that was unregistered
 * @param bWasSuccessful whether the unregister completed ok or not
 */
function OnUnregisterPlayerCompleteForHostMigrate(name SessionName,UniqueNetId PlayerId,bool bWasSuccessful)
{
	// Clear the delegate so another unregister can occur
	OnlineSub.GameInterface.ClearUnregisterPlayerCompleteDelegate(OnUnregisterPlayerCompleteForHostMigrate);

	// Continue unregistering missing peers until none are left
	if (!RemoveMissingPeersFromSession(SessionName,OnUnregisterPlayerCompleteForHostMigrate))
	{
		// Travel host with the migrated session
		PeerDesignatedAsHost(SessionName);
	}
}

/**
 * Delegate called once the session migration on the host completed.
 *
 * @param SessionName the name of the session being migrated
 * @param bWasSuccessful whether the session migration completed ok or not
 */
function OnHostMigratedOnlineGame(name SessionName,bool bWasSuccessful)
{
	OnlineSub.GameInterface.ClearMigrateOnlineGameCompleteDelegate(OnHostMigratedOnlineGame);

	if (bWasSuccessful)
	{
		// Remove all non-peers from session and migrate as host once that completes
		if (!RemoveMissingPeersFromSession(SessionName,OnUnregisterPlayerCompleteForHostMigrate))
		{
			// if nobody to remove just travel as host
			PeerDesignatedAsHost(SessionName);
		}
	}
	else
	{
		`Log(`location @ "migration failed for"
			$" NetId="$class'OnlineSubsystem'.static.UniqueNetIdToString(PlayerReplicationInfo.UniqueId),,'DevNet');

		//@todo peer - Notify other peers of failed host migration so another peer can be selected.
		// Currently they will just fail after they wait for the HostMigrationTimeout period

		ClientSetProgressMessage(PMT_PeerHostMigrationFailure,
			"<Strings:Engine.Errors.ConnectionFailed>",
			"<Strings:Engine.Errors.ConnectionFailed_Title>",
			true);
	}
}

/**
 * @return Current game search class in use
 */
function class<OnlineGameSearch> GetCurrentSearchClass()
{
	return class'OnlineGameSearch';
}

/**
 * This peer player has been selected as the new host.
 * Notify all other clients that have also lost their server connection to begin traveling to the newly migrated session.
 * Begin traveling as the new host.
 *
 * @param SessionName Name of the session that was migrated. Can be 'None' if migrating without a session
 */
function PeerDesignatedAsHost(name SessionName)
{
	local int PeerIdx;
	local byte PlatformInfo[80];

	// Notify peers (that have also lost connection) to travel to us as the new host
	if (OnlineSub != None &&
		OnlineSub.GameInterface != None &&
		OnlineSub.GameInterface.GetGameSettings(SessionName) != None &&
		OnlineSub.GameInterface.ReadPlatformSpecificSessionInfoBySessionName(SessionName,PlatformInfo))
	{
		for (PeerIdx=0; PeerIdx < ConnectedPeers.Length; PeerIdx++)
		{
			if (ConnectedPeers[PeerIdx].bLostConnectionToHost)
			{
				// travel using migrated game session
				TellPeerToTravelToSession(ConnectedPeers[PeerIdx].PlayerId,SessionName,GetCurrentSearchClass(),PlatformInfo,80);
			}
		}
	}
	else
	{
		for (PeerIdx=0; PeerIdx < ConnectedPeers.Length; PeerIdx++)
		{
			if (ConnectedPeers[PeerIdx].bLostConnectionToHost)
			{
				// travel to this player's ip addr
				TellPeerToTravel(ConnectedPeers[PeerIdx].PlayerId);
			}
		}
	}

	// New host starts travel to the same map
	PeerTravelAsHost(0.5,GetNewPeerHostURL());
}

/**
 * @return URL to use when traveling a peer as the new host after host migration
 */
function string GetNewPeerHostURL()
{
	return WorldInfo.GetMapName(true) $ "?game=" $ PathName(WorldInfo.GetGameClass()) $ "?listen";
}

/**
 * Delay and then travel as the new host to the given URL
 *
 * @param TravelCountdownTimer Seconds to delay before initiating the travel
 * @param URL browse path for the map/game to load as host
 */
native function PeerTravelAsHost(float TravelCountdownTimer,string URL);

/**
 * Notify client peer to travel to the new host. RPC is sent through peer net driver.
 *
 * @param ToPeerNetId peer player to find connection for
 */
native function TellPeerToTravel(UniqueNetId ToPeerNetId);

/**
 * Notify client peer to travel to the new host via its migrated session. RPC is sent through peer net driver.
 *
 * @param ToPeerNetId peer player to find connection for
 * @param SessionName Name of session that was migrated to travel to
 * @param SearchClass Search class being used by the current game session
 * @param PlatformSpecificInfo Byte array with secure session info
 * @param PlatformSpecificInfoSize Size in bytes of PlatformSpecificInfo
 */
native function TellPeerToTravelToSession(UniqueNetId ToPeerNetId, name SessionName, class<OnlineGameSearch> SearchClass, byte PlatformSpecificInfo[80],int PlatformSpecificInfoSize);

/**
 * Notification when client peer received a migrated session. The session is joined via migration and the client travels to the new host once the join succeeds.
 *
 * @param FromPeerNetId peer player that that sent us the migrated session. This is the new host
 * @param SearchClass Search class being used by the game session on the host
 * @param PlatformSpecificInfo Byte array with secure session info
 */
event PeerReceivedMigratedSession(UniqueNetId FromPeerNetId, name SessionName, class<OnlineGameSearch> SearchClass, byte PlatformSpecificInfo[80])
{
	local OnlineGameSearchResult SessionToJoin;
	local LocalPlayer LP;

	LP = LocalPlayer(Player);
	if (LP != None &&
		OnlineSub != None &&
		OnlineSub.GameInterface != None)
	{
		`Log(`location @ "received migrated session to join"
			$" SessionName="$SessionName
			$" SearchClass="$SearchClass
			$" UniqueId="$OnlineSub.static.UniqueNetIdToString(PlayerReplicationInfo.UniqueId)
			$" FromPeerNetId="$OnlineSub.static.UniqueNetIdToString(FromPeerNetId)
			,,'DevNet');

		// This search object is kept around until the current game is ended so the join/migration can be initiated
		MigratedSearchToJoin = new SearchClass;
		// Bind to the migrated session sent to us by the new host
		if (OnlineSub.GameInterface.BindPlatformSpecificSessionToSearch(LP.ControllerId,MigratedSearchToJoin,PlatformSpecificInfo))
		{
			// Remove reference to pending session to join via migrate
			SessionToJoin = MigratedSearchToJoin.Results[0];
			MigratedSearchToJoin = None;
			// Set the delegate for notification of the join completing
			OnlineSub.GameInterface.AddJoinMigratedOnlineGameCompleteDelegate(OnJoinMigratedGame);
			// This will have us join the migrated session async
			OnlineSub.GameInterface.JoinMigratedOnlineGame(LP.ControllerId,SessionName,SessionToJoin);
		}
		else
		{
			`Log(`location @ "failed to bind to migrated session!",,'DevNet');
			MigratedSearchToJoin = None;

			ClientSetProgressMessage(PMT_PeerHostMigrationFailure,
				"<Strings:Engine.Errors.ConnectionFailed>",
				"<Strings:Engine.Errors.ConnectionFailed_Title>",
				true);
		}
	}
}

/**
 * Delegate called after each iteration of unregistering a player
 *
 * @param SessionName the name of the session to unregister player from
 * @param PlayerId net id of player that was unregistered
 * @param bWasSuccessful whether the unregister completed ok or not
 */
function OnUnregisterPlayerCompleteForJoinMigrate(name SessionName,UniqueNetId PlayerId,bool bWasSuccessful)
{
	// Clear the delegate so another unregister can occur
	OnlineSub.GameInterface.ClearUnregisterPlayerCompleteDelegate(OnUnregisterPlayerCompleteForJoinMigrate);

	// Continue unregistering missing peers until none are left
	if (!RemoveMissingPeersFromSession(SessionName,OnUnregisterPlayerCompleteForJoinMigrate))
	{
		PeerDesignatedAsClient(SessionName);
	}
}

/**
 * This peer player has been to told to travel with a migrated session to new host.
 *
 * @param SessionName Name of the session that was migrated. Can be 'None' if migrating without a session
 */
function PeerDesignatedAsClient(name SessionName)
{
	local string URL;

	// We are joining so grab the connect string to use
	if (OnlineSub.GameInterface.GetResolvedConnectString(SessionName,URL))
	{
		`Log(`location @ "traveling to joined,migrated session "
			$" SessionName="$SessionName
			$" URL="$URL
			,,'DevNet');

		// travel to the specified URL
		ClientTravel(URL, TRAVEL_Absolute);
	}
	else
	{
		`Log(`location @ "failed joining migrated session "
				$" SessionName="$SessionName
				,,'DevNet');

		ClientSetProgressMessage(PMT_PeerHostMigrationFailure,
			"<Strings:Engine.Errors.ConnectionFailed>",
			"<Strings:Engine.Errors.ConnectionFailed_Title>",
			true);
	}

}

/**
 * Delegate called once the join/migration of the session has completed.
 *
 * @param SessionName the name of the session being joined/migrated
 * @param bWasSuccessful whether the join/migrate completed ok or not
 */
function OnJoinMigratedGame(name SessionName,bool bWasSuccessful)
{
	OnlineSub.GameInterface.ClearJoinMigratedOnlineGameCompleteDelegate(OnJoinMigratedGame);

	if (bWasSuccessful)
	{
		// Remove all non-peers from session and join once that completes
		if (!RemoveMissingPeersFromSession(SessionName,OnUnregisterPlayerCompleteForJoinMigrate))
		{
			PeerDesignatedAsClient(SessionName);
		}
	}
	if (!bWasSuccessful)
	{
		`Log(`location @ "failed joining migrated session "
				$" SessionName="$SessionName
				,,'DevNet');

		ClientSetProgressMessage(PMT_PeerHostMigrationFailure,
			"<Strings:Engine.Errors.ConnectionFailed>",
			"<Strings:Engine.Errors.ConnectionFailed_Title>",
			true);
	}
}

event PreRender(Canvas Canvas);

function ResetTimeMargin()
{
	TimeMargin = -0.1;
	MaxTimeMargin = class'GameInfo'.Default.MaxTimeMargin;
}

reliable server function ServerShortTimeout()
{
	local Actor A;

	if (!bShortConnectTimeout)
	{
		bShortConnectTimeOut = true;
		ResetTimeMargin();

		// quick update of pickups and gameobjectives since this player is now relevant
		if (WorldInfo.Pauser != None)
		{
			// update everything immediately, as TimeSeconds won't get advanced while paused
			// so otherwise it won't happen at all until the game is unpaused
			// this floods the network, but we're paused, so no gameplay is going on that would care much
			foreach AllActors(class'Actor', A)
			{
				if (!A.bOnlyRelevantToOwner)
				{
					A.bForceNetUpdate = TRUE;
				}
			}
		}
		else if ( WorldInfo.Game.NumPlayers < 8 )
		{
			foreach AllActors(class'Actor', A)
			{
				if ( (A.NetUpdateFrequency < 1) && !A.bOnlyRelevantToOwner )
				{
					A.SetNetUpdateTime(FMin(A.NetUpdateTime, WorldInfo.TimeSeconds + 0.2 * FRand()));
				}
			}
		}
		else
		{
			foreach AllActors(class'Actor', A)
			{
				if ( (A.NetUpdateFrequency < 1) && !A.bOnlyRelevantToOwner )
				{
					A.SetNetUpdateTime(FMin(A.NetUpdateTime, WorldInfo.TimeSeconds + 0.5 * FRand()));
				}
			}
		}
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

function AddCheats(optional bool bForce)
{
	// Assuming that this never gets called for NM_Client without bForce=true
	if ( (CheatManager == None) && (WorldInfo.Game != None) && WorldInfo.Game.AllowCheats(self) || bForce)
	{
		CheatManager = new(Self) CheatClass;
		CheatManager.InitCheatManager();
	}
}

exec function EnableCheats()
{
`if(`notdefined(FINAL_RELEASE))
	AddCheats(true);
`else
	AddCheats();
`endif
}

/* SpawnDefaultHUD()
Spawn a HUD (make sure that PlayerController always has valid HUD, even if \
ClientSetHUD() hasn't been called\
*/
function SpawnDefaultHUD()
{
	if ( LocalPlayer(Player) == None )
		return;
	`log(GetFuncName());
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
	WaitDelay = WorldInfo.TimeSeconds + 2;
    FixFOV();
    if ( PlayerReplicationInfo.bOnlySpectator )
    	GotoState('Spectating');
    else
		GotoState('PlayerWaiting');
}

reliable client function ClientReset()
{
	ResetCameraMode();
	SetViewTarget(self);
	GotoState(PlayerReplicationInfo.bOnlySpectator ? 'Spectating' : 'PlayerWaiting');
}

function CleanOutSavedMoves()
{
	SavedMoves = None;
	PendingMove = None;
}

/**
 * Notification that the ControllerId for this PC LocalPlayer is about to change.  Provides the PC a chance to cleanup anything that was
 * associated with the old ControllerId.  When this method is called, LocalPlayer.ControllerId is still the old value.
 */
function PreControllerIdChange()
{
	local LocalPlayer LP;

	LP = LocalPlayer(Player);
	if ( LP != None )
	{
		ClientStopNetworkedVoice();
		ClearOnlineDelegates();

		UnregisterPlayerDataStores();
	}
}

/**
 * Notification that the ControllerId for this PC's LocalPlayer has been changed.  Re-register all player data stores and any delegates that
 * require a ControllerId.
 */
function PostControllerIdChange()
{
	local LocalPlayer LP;
	local UniqueNetId PlayerId;

	LP = LocalPlayer(Player);
	if ( LP != None )
	{
		// update unique ID
		// you get kicked for this in network games so no need to update
		if (WorldInfo.NetMode != NM_Client && OnlineSub != None && OnlineSub.PlayerInterface != None)
		{
			// Get our local id from the online subsystem
			OnlineSub.PlayerInterface.GetUniquePlayerId(LP.ControllerId, PlayerId);
			PlayerReplicationInfo.SetUniqueId(PlayerId);
		}
		RegisterPlayerDataStores();

		RegisterOnlineDelegates();
		ClientSetOnlineStatus();

		// we don't currently allow players to migrate gamepads after the initial interactive screen.  If we are a client or not in the menu
		// level then that constraint has been lifted - technically this shouldn't be an assertion, but this would be a really difficult bug
		// to track down otherwise, if switching gamepads is ever allowed after initial interactive scene.
		`assert(WorldInfo.Game != None);
		if ( !WorldInfo.Game.bRequiresPushToTalk )
		{
			ClientStartNetworkedVoice();
		}
	}
}

/**
 * Wrapper for getting reference to the OnlineSubsystem
 */
simulated final function OnlineSubsystem GetOnlineSubsystem()
{
	if ( OnlineSub == None )
	{
		OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	}

	return OnlineSub;
}

/* InitInputSystem()
Spawn the appropriate class of PlayerInput
Only called for playercontrollers that belong to local players
*/
event InitInputSystem()
{
	local Class<ForceFeedbackManager> FFManagerClass;
	local int i;
	local Sequence GameSeq;
	local array<SequenceObject> AllInterpActions;

	if (PlayerInput == None)
	{
		Assert(InputClass != None);
		PlayerInput = new(Self) InputClass;

		if (PlayerInput != none)
		{
			PlayerInput.InitInputSystem();
		}
	}


	if ( Interactions.Find(PlayerInput) == -1 )
	{
		Interactions[Interactions.Length] = PlayerInput;
	}

	// Spawn the waveform manager here
	if (ForceFeedbackManagerClassName != "")
	{
		FFManagerClass = class<ForceFeedbackManager>(DynamicLoadObject(ForceFeedbackManagerClassName,class'Class'));
		if (FFManagerClass != None)
		{
			ForceFeedbackManager = new(Self) FFManagerClass;
		}
	}

	RegisterOnlineDelegates();

	// add the player to any matinees running so that it gets in on any cinematics already running, etc
	// (already done on server in PostLogin())
	if (Role < ROLE_Authority)
	{
		GameSeq = WorldInfo.GetGameSequence();
		if (GameSeq != None)
		{
			// find any matinee actions that exist
			GameSeq.FindSeqObjectsByClass(class'SeqAct_Interp', true, AllInterpActions);

			// tell them all to add this PC to any running Director tracks
			for (i = 0; i < AllInterpActions.Length; i++)
			{
				SeqAct_Interp(AllInterpActions[i]).AddPlayerToDirectorTracks(self);
			}
		}
	}

	// this is a good stop gap measure for any cases that we miss / other code getting turned on / called
	// there is never a case where we want the tilt to be on at the point where the player controller is created
	SetOnlyUseControllerTiltInput( FALSE );
	SetUseTiltForwardAndBack( TRUE );
	SetControllerTiltActive( FALSE );
}

/**
 * Initializes this client's Player data stores after seamless map travel
 */
reliable client function ClientInitializeDataStores()
{
	`log(">> PlayerController::ClientInitializeDataStores for player" @ Self,,'DevDataStore');

	// register the player's data stores and bind the PRI to the PlayerOwner data store.
	RegisterPlayerDataStores();

	`log("<< PlayerController::ClientInitializeDataStores for player" @ Self,,'DevDataStore');
}

/**
 * Register all player data stores.
 */
simulated final function RegisterPlayerDataStores()
{
	RegisterCustomPlayerDataStores();
	RegisterStandardPlayerDataStores();
}

/**
 * Creates and initializes the "PlayerOwner" and "PlayerSettings" data stores.  This function assumes that the PlayerReplicationInfo
 * for this player has not yet been created.
 */
simulated protected function RegisterCustomPlayerDataStores()
{
	local LocalPlayer LP;
	local DataStoreClient DataStoreManager;
	local class<UIDataStore_OnlinePlayerData> PlayerDataStoreClass;

`if(`notdefined(FINAL_RELEASE))
	local string PlayerName;

	PlayerName = PlayerReplicationInfo != None ? PlayerReplicationInfo.PlayerName : "None";
`endif

	// only create player data store for local players
	LP = LocalPlayer(Player);

	`log(">>" @ `location @ "(" $ PlayerName $ ")" @ `showobj(LP),,'DevDataStore');
	if ( LP != None )
	{
		// get a reference to the main data store client
		DataStoreManager = class'UIInteraction'.static.GetDataStoreClient();
		if ( DataStoreManager != None )
		{
			// create the OnlinePlayerData data store
			OnlinePlayerData = UIDataStore_OnlinePlayerData(DataStoreManager.FindDataStore('OnlinePlayerData',LP));
			if ( OnlinePlayerData == None )
			{
				PlayerDataStoreClass = class<UIDataStore_OnlinePlayerData>(DataStoreManager.FindDataStoreClass(class'UIDataStore_OnlinePlayerData'));
				if ( PlayerDataStoreClass != None )
				{
					// find the appropriate class to use for the PlayerSettings data store
					// create the PlayerSettings data store
					OnlinePlayerData = DataStoreManager.CreateDataStore(PlayerDataStoreClass);
					if ( OnlinePlayerData != None )
					{
						if ( !DataStoreManager.RegisterDataStore(OnlinePlayerData, LP) )
						{
							`log("Failed to register 'OnlinePlayerData' data store for player:"@ Self @ "(" $ PlayerName $ ")" @ `showobj(OnlinePlayerData),,'DevDataStore');
						}
					}
					else
					{
						`log("Failed to create 'OnlinePlayerData' data store for player:"@ Self @ "(" $ PlayerName $ ") using class" @ PlayerDataStoreClass,,'DevDataStore');
					}
				}
				else
				{
					`log("Failed to find valid data store class while attempting to register the 'OnlinePlayerData' data store for player:"@ Self @ "(" $ PlayerName $ ")",,'DevDataStore');
				}
			}
			else
			{
				`log("'OnlinePlayerData' data store already registered for player:"@ Self @ "(" $ PlayerName $ ")",,'DevDataStore');
			}
		}

	}

	`log("<<" @ `location @ "(" $ PlayerName $ ")",,'DevDataStore');
}

/**
 * Register any player data stores which do not require special initialization.
 */
simulated protected function RegisterStandardPlayerDataStores()
{
	local LocalPlayer LP;
	local DataStoreClient DataStoreManager;
	local array<class<UIDataStore> > PlayerDataStoreClasses;
	local class<UIDataStore> PlayerDataStoreClass;
	local UIDataStore PlayerDataStore;
	local int ClassIndex;

`if(`notdefined(FINAL_RELEASE))
	local string PlayerName;

	PlayerName = PlayerReplicationInfo != None ? PlayerReplicationInfo.PlayerName : "None";
`endif

	// only create player data store for local players
	LP = LocalPlayer(Player);
	if ( LP != None )
	{
		`log(">>" @ `location @ "(" $ PlayerName $ ")",,'DevDataStore');

		// get a reference to the main data store client
		DataStoreManager = class'UIInteraction'.static.GetDataStoreClient();
		if ( DataStoreManager != None )
		{
			DataStoreManager.GetPlayerDataStoreClasses(PlayerDataStoreClasses);

			// go through the data store client's list of player data store classes
			for ( ClassIndex = 0; ClassIndex < PlayerDataStoreClasses.Length; ClassIndex++ )
			{
				PlayerDataStoreClass = PlayerDataStoreClasses[ClassIndex];
				if ( PlayerDataStoreClass != None )
				{
					// if the data store isn't already registered, do it now
					PlayerDataStore = DataStoreManager.FindDataStore(PlayerDataStoreClass.default.Tag, LP);
					if ( PlayerDataStore == None )
					{
						`log("    Registering standard player data store '" $ PlayerDataStoreClass.Name $ "' for player" @ Self @ "(" $ PlayerName $ ")" @ `showobj(LP),,'DevDataStore');
						PlayerDataStore = DataStoreManager.CreateDataStore(PlayerDataStoreClass);
						if ( PlayerDataStore != None )
						{
							if ( !DataStoreManager.RegisterDataStore(PlayerDataStore, LP) )
							{
								`log("Failed to register '" $ PlayerDataStoreClass.default.Tag $ "' data store for player:" @ Self @ "(" $ PlayerName $ ")" @ `showobj(PlayerDataStore),,'DevDataStore');
							}
						}
					}
					else
					{
						`log("'" $ PlayerDataStoreClass.default.Tag $ "' data store already registered for player:" @ Self @ "(" $ PlayerName $ ")",,'DevDataStore');
					}
				}
			}
		}
	}
}

/**
 * Unregisters the "PlayerOwner" data store for this player.  Called when this PlayerController is destroyed.
 */
simulated function UnregisterPlayerDataStores()
{
	local LocalPlayer LP;
	local DataStoreClient DataStoreManager;
	local UIDataStore_OnlinePlayerData OnlinePlayerDataStore;

`if(`notdefined(FINAL_RELEASE))
	local string PlayerName;

	PlayerName = PlayerReplicationInfo != None ? PlayerReplicationInfo.PlayerName : "None";
`endif

	// only execute for local players
	LP = LocalPlayer(Player);
	if ( LP != None )
	{
		`log(">>" @ `location @ "(" $ PlayerName $ ")",,'DevDataStore');

		// because the PlayerOwner data store is created and registered each match, all we should need to do is
		// unregister it from the data store client and clear our reference
		// get a reference to the main data store client
		DataStoreManager = class'UIInteraction'.static.GetDataStoreClient();
		if ( DataStoreManager != None )
		{
			// unregister the current player data store
			// Don't hold onto a ref
			OnlinePlayerData = None;
			// Unregister the online player data
			OnlinePlayerDataStore = UIDataStore_OnlinePlayerData(DataStoreManager.FindDataStore('OnlinePlayerData',LP));
			if ( OnlinePlayerDataStore != None )
			{
				if ( !DataStoreManager.UnregisterDataStore(OnlinePlayerDataStore) )
				{
					`log("Failed to unregister 'OnlinePlayerData' data store for player:"@ Self @ "(" $ PlayerName $ ")" @ `showobj(OnlinePlayerDataStore),,'DevDataStore');
				}
			}
			else
			{
				`log("'OnlinePlayerData' data store not registered for player:" @ Self @ "(" $ PlayerName $ ")",,'DevDataStore');
			}

			UnregisterStandardPlayerDataStores();
		}
		else
		{
			`log("Data store client not found!",,'DevDataStore');
		}

		`log("<<" @ `location @ "(" $ PlayerName $ ")",,'DevDataStore');
	}
}

/**
 * Unregisters all player data stores that remain after unregistering all player data stores that require custom unregister logic.
 */
simulated function UnregisterStandardPlayerDataStores()
{
	local LocalPlayer LP;
	local DataStoreClient DataStoreManager;
	local array<class<UIDataStore> > PlayerDataStoreClasses;
	local class<UIDataStore> PlayerDataStoreClass;
	local UIDataStore PlayerDataStore;
	local int ClassIndex;

`if(`notdefined(FINAL_RELEASE))
	local string PlayerName;

	PlayerName = PlayerReplicationInfo != None ? PlayerReplicationInfo.PlayerName : "None";
`endif

	// only execute for local players
	LP = LocalPlayer(Player);
	if ( LP != None )
	{
		`log(">>" @ `location @ "(" $ PlayerName $ ")",,'DevDataStore');

		// because the PlayerOwner data store is created and registered each match, all we should need to do is
		// unregister it from the data store client and clear our reference
		// get a reference to the main data store client
		DataStoreManager = class'UIInteraction'.static.GetDataStoreClient();
		if ( DataStoreManager != None )
		{
			DataStoreManager.GetPlayerDataStoreClasses(PlayerDataStoreClasses);

			// now go through the rest of the registered player data stores and unregister them
			for ( ClassIndex = 0; ClassIndex < PlayerDataStoreClasses.Length; ClassIndex++ )
			{
				PlayerDataStoreClass = PlayerDataStoreClasses[ClassIndex];
				if ( PlayerDataStoreClass != None )
				{
					// if the data store isn't already registered, do it now
					PlayerDataStore = DataStoreManager.FindDataStore(PlayerDataStoreClass.default.Tag, LP);
					if ( PlayerDataStore != None )
					{
						if ( !DataStoreManager.UnregisterDataStore(PlayerDataStore) )
						{
							`log("Failed to unregister '" $ PlayerDataStore.Tag $ "' data store for player:"@ Self @ "(" $ PlayerName $ ")" @ `showobj(PlayerDataStore),,'DevDataStore');
						}
					}
				}
			}
		}

		`log("<<" @ `location @ "(" $ PlayerName $ ")",,'DevDataStore');
	}
}

/**
 * Refetches this player's profile settings.
 */
simulated function ReloadProfileSettings()
{
	if ( OnlinePlayerData != None && OnlinePlayerData.ProfileProvider != None )
	{
		OnlinePlayerData.ProfileProvider.RefreshStorageData();
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
 * @return whether or not this Controller has Tilt Turned on
 **/
native simulated function bool IsControllerTiltActive() const;

/**
 * sets whether or not the Tilt functionality is turned on
 **/
native simulated function SetControllerTiltActive( bool bActive );

/**
 * sets whether or not to ONLY use the tilt input controls
 **/
native simulated function SetOnlyUseControllerTiltInput( bool bActive );

/**
 * sets whether or not to use the tilt forward and back input controls
 **/
native simulated function SetUseTiltForwardAndBack( bool bActive );

/**
 * @return whether or not this Controller has a keyboard available to be used
 **/
native simulated function bool IsKeyboardAvailable() const;

/**
 * @return whether or not this Controller has a mouse available to be used
 **/
native simulated function bool IsMouseAvailable() const;

/**
 * Exec command to control tilt so that kismet can control it
 */
exec function SetTiltActive(bool bActive)
{
	SetControllerTiltActive(bActive);
}

/* ClientGotoState()
server uses this to force client into NewState
*/
reliable client function ClientGotoState(name NewState, optional name NewLabel)
{
	if ((NewLabel == 'Begin' || NewLabel == '') && !IsInState(NewState))
	{
		GotoState(NewState);
	}
	else
	{
		GotoState(NewState,NewLabel);
	}
}

reliable server function AskForPawn()
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

reliable client function GivePawn(Pawn NewPawn)
{
	if (NewPawn == None)
	{
		return;
	}

	// Avoid calling ClientRestart if we have already accepted this pawn
	if (Pawn != NewPawn || NewPawn.Controller != Self)
	{
		Pawn = NewPawn;
		NewPawn.Controller = self;
		ClientRestart(Pawn);
	}
}

// Possess a pawn
event Possess(Pawn aPawn, bool bVehicleTransition)
{
	local Actor A;
	local int i;
	local SeqEvent_Touch TouchEvent;

	if (!PlayerReplicationInfo.bOnlySpectator)
	{
		if (aPawn.Controller != None)
		{
			aPawn.Controller.UnPossess();
		}

		aPawn.PossessedBy(self, bVehicleTransition);
		Pawn = aPawn;
		Pawn.SetTickIsDisabled(false);
		ResetTimeMargin();
		Restart(bVehicleTransition);

		// check if touching any actors with playeronly kismet touch events
		ForEach Pawn.TouchingActors(class'Actor', A)
		{
			for ( i=0; i < A.GeneratedEvents.length; i++)
			{
				TouchEvent = SeqEvent_Touch(A.GeneratedEvents[i]);
				if ( (TouchEvent != None) && TouchEvent.bPlayerOnly )
				{
					TouchEvent.CheckTouchActivate(A,Pawn);
				}
			}
		}
	}
}

function AcknowledgePossession(Pawn P)
{
	if ( LocalPlayer(Player) != None )
	{
		AcknowledgedPawn = P;
		if ( P != None )
		{
			P.SetBaseEyeHeight();
			P.Eyeheight = P.BaseEyeHeight;
		}
		ServerAcknowledgePossession(P);
	}
}

reliable server function ServerAcknowledgePossession(Pawn P)
{
	if ( (P != None) && (P == Pawn) && (P != AcknowledgedPawn) )
	{
	ResetTimeMargin();
	}
    AcknowledgedPawn = P;
}

// unpossessed a pawn (not because pawn was killed)
event UnPossess()
{
	if (Pawn != None)
	{
		SetLocation(Pawn.Location);
		if (Role == ROLE_Authority)
		{
			Pawn.RemoteRole = ROLE_SimulatedProxy;
		}
		Pawn.UnPossessed();
		CleanOutSavedMoves();  // don't replay moves previous to unpossession
		if (GetViewTarget() == Pawn)
		{
			SetViewTarget(self);
		}
	}
	Pawn = None;
}

// unpossessed a pawn (because pawn was killed)
function PawnDied(Pawn P)
{
	if (P == Pawn)
	{
		if (Role == ROLE_Authority && Pawn != None)
		{
			Pawn.RemoteRole = ROLE_SimulatedProxy;
		}

		Super.PawnDied(P);
	}
}

reliable client function ClientSetHUD(class<HUD> newHUDType)
{
	if ( myHUD != None )
	{
		myHUD.Destroy();
	}

	myHUD = (newHUDType != None) ? Spawn(newHUDType, self) : None;
}

reliable client function ClientSetSecondaryHUD(class<HUD> newHUDType)
{
	if ( mySecondaryHUD != None )
	{
		mySecondaryHUD.Destroy();
	}

	mySecondaryHUD = (newHUDType != None) ? Spawn(newHUDType, self) : None;
}

function HandlePickup(Inventory Inv)
{
	ReceiveLocalizedMessage(Inv.MessageClass,,,,Inv.class);
}

/* epic ===============================================
* ::CleanupPRI
*
* Called from Destroyed().  Cleans up PlayerReplicationInfo.
* PlayerControllers add their PRI to the gameinfo's InactivePRI list, so disconnected players can rejoin without losing their score.
*
* =====================================================
*/
function CleanupPRI()
{
	WorldInfo.Game.AddInactivePRI(PlayerReplicationInfo, self);
	PlayerReplicationInfo = None;
}

reliable client event ReceiveLocalizedMessage( class<LocalMessage> Message, optional int Switch, optional PlayerReplicationInfo RelatedPRI_1, optional PlayerReplicationInfo RelatedPRI_2, optional Object OptionalObject )
{
	// Wait for player to be up to date with replication when joining a server, before stacking up messages
	if ( WorldInfo.NetMode == NM_DedicatedServer || WorldInfo.GRI == None )
		return;

	Message.Static.ClientReceive( Self, Switch, RelatedPRI_1, RelatedPRI_2, OptionalObject );
}

//Play a sound client side (so only client will hear it)
unreliable client event ClientPlaySound(SoundCue ASound)
{
	ClientHearSound(ASound, self, Location, false, false);
}

/** hooked up to the OnAudioFinished delegate of AudioComponents created through PlaySound() to return them to the pool */
simulated function HearSoundFinished(AudioComponent AC)
{
	HearSoundActiveComponents.RemoveItem(AC);
	// if the component is already pending kill (playing actor was destroyed), then we can't put it back in the pool
	if (!AC.IsPendingKill())
	{
		AC.ResetToDefaults();
		HearSoundPoolComponents[HearSoundPoolComponents.length] = AC;
	}
}

/** get an audio component from the HearSound pool
 * creates a new component if the pool is empty and MaxConcurrentHearSounds has not been exceeded
 * the component is initialized with the values passed in, ready to call Play() on
 * its OnAudioFinished delegate is set to this PC's HearSoundFinished() function
 * @param ASound - the sound to play
 * @param SourceActor - the Actor to attach the sound to (if None, attached to self)
 * @param bStopWhenOwnerDestroyed - whether the sound stops if SourceActor is destroyed
 * @param bUseLocation (optional) - whether to use the SourceLocation parameter for the sound's location (otherwise, SourceActor's location)
 * @param SourceLocation (optional) - if bUseLocation, the location for the sound
 * @return the AudioComponent that was found/created
 */
native function AudioComponent GetPooledAudioComponent(SoundCue ASound, Actor SourceActor, bool bStopWhenOwnerDestroyed, optional bool bUseLocation, optional vector SourceLocation);

/* ClientHearSound()
Replicated function from server for replicating audible sounds played on server
*/
unreliable client event ClientHearSound(SoundCue ASound, Actor SourceActor, vector SourceLocation, bool bStopWhenOwnerDestroyed, optional bool bIsOccluded )
{
	local AudioComponent AC;

//    `log("### ClientHearSound:"@ASound@SourceActor@SourceLocation@bStopWhenOwnerDestroyed@VSize(SourceLocation - Pawn.Location));

	if ( SourceActor == None )
	{
		AC = GetPooledAudioComponent(ASound, SourceActor, bStopWhenOwnerDestroyed, true, SourceLocation);
		if (AC == None)
		{
			return;
		}
		AC.bUseOwnerLocation = false;
		AC.Location = SourceLocation;
	}
	else if ( (SourceActor == GetViewTarget()) || (SourceActor == self) )
	{
		AC = GetPooledAudioComponent(ASound, None, bStopWhenOwnerDestroyed);
		if (AC == None)
		{
			return;
		}
		AC.bAllowSpatialization = false;
	}
	else
	{
		AC = GetPooledAudioComponent(ASound, SourceActor, bStopWhenOwnerDestroyed);
		if (AC == None)
		{
			return;
		}
		if (!IsZero(SourceLocation) && SourceLocation != SourceActor.Location)
		{
			AC.bUseOwnerLocation = false;
			AC.Location = SourceLocation;
		}
	}
	if ( bIsOccluded )
	{
		// if occluded reduce volume: @FIXME do something better
		AC.VolumeMultiplier *= 0.5;
	}
	AC.Play();
}
simulated function bool IsClosestLocalPlayerToActor(Actor TheActor)
{
	local PlayerController PC;
	local float MyDist;

	if(ViewTarget == none)
	{
		return false;
	}
	MyDist = VSize(ViewTarget.Location - TheActor.Location);
	ForEach LocalPlayerControllers( class'PlayerController', PC )
	{
		if( PC != self && (PC.ViewTarget != None) && (VSize(PC.ViewTarget.Location - TheActor.Location) < MyDist) )
		{
			return false;
		}
	}
	return true;
}
reliable client event Kismet_ClientPlaySound(SoundCue ASound, Actor SourceActor, float VolumeMultiplier, float PitchMultiplier, float FadeInTime, bool bSuppressSubtitles, bool bSuppressSpatialization)
{
	local AudioComponent AC;

	if ( SourceActor != None && IsClosestLocalPlayerToActor(SourceActor))
	{
		// If we have a FaceFX animation hooked up, play that instead
		if( ASound.FaceFXAnimName != "" &&
			SourceActor.PlayActorFaceFXAnim(ASound.FaceFXAnimSetRef, ASound.FaceFXGroupName, ASound.FaceFXAnimName, ASound) )
		{
			// Success - In case of failure, fall back to playing the sound with no Face FX animation, but there will be a warning in the log instead.
		}
		else
		{
			AC = SourceActor.CreateAudioComponent(ASound, false, true);
			if ( AC != None )
			{
				AC.VolumeMultiplier = VolumeMultiplier;
				AC.PitchMultiplier = PitchMultiplier;
				AC.bAutoDestroy = true;
				AC.SubtitlePriority = 10000;
				AC.bSuppressSubtitles = bSuppressSubtitles;
				AC.FadeIn(FadeInTime, 1.f);
				if( bSuppressSpatialization )
				{
					AC.bAllowSpatialization = false;
				}
			}
		}
	}
}

reliable client event Kismet_ClientStopSound(SoundCue ASound, Actor SourceActor, float FadeOutTime)
{
	local AudioComponent AC, CheckAC;

	if (SourceActor == None)
	{
		SourceActor = WorldInfo;
	}
	foreach SourceActor.AllOwnedComponents(class'AudioComponent',CheckAC)
	{
		if (CheckAC.SoundCue == ASound)
		{
			AC = CheckAC;
			break;
		}
	}
	if (AC != None)
	{
		AC.FadeOut(FadeOutTime,0.f);
	}
}

/** plays a FaceFX anim on the specified actor for the client */
reliable client function ClientPlayActorFaceFXAnim(Actor SourceActor, FaceFXAnimSet AnimSet, string GroupName, string SeqName, SoundCue SoundCueToPlay)
{
	if (SourceActor != None)
	{
		SourceActor.PlayActorFaceFXAnim(AnimSet, GroupName, SeqName, SoundCueToPlay);
	}
}

reliable client event ClientMessage( coerce string S, optional Name Type, optional float MsgLifeTime )
{
	if ( WorldInfo.NetMode == NM_DedicatedServer || WorldInfo.GRI == None )
	{
		return;
	}

	if (Type == '')
	{
		Type = 'Event';
	}

	TeamMessage(PlayerReplicationInfo, S, Type, MsgLifeTime);
}

/** Overridden by specific games */
simulated private function bool CanCommunicate()
{
	return TRUE;
}

simulated private function bool AllowTTSMessageFrom( PlayerReplicationInfo PRI )
{
	return TRUE;
}

/**
 * Text to speech handling
 */

/** Constructs a SoundCue, performs text-to-wavedata conversion. */
simulated private native function SoundCue CreateTTSSoundCue( string StrToSpeak, PlayerReplicationInfo PRI );

exec function Talk()
{
	local Console PlayerConsole;
	local LocalPlayer LP;

	LP = LocalPlayer( Player );
	if( ( LP != None ) && CanCommunicate() && ( LP.ViewportClient.ViewportConsole != None ) )
	{
		PlayerConsole = LocalPlayer( Player ).ViewportClient.ViewportConsole;
		PlayerConsole.StartTyping( "Say " );
	}
}

exec function TeamTalk()
{
	local Console PlayerConsole;
	local LocalPlayer LP;

	LP = LocalPlayer( Player );
	if( ( LP != None ) && CanCommunicate() && ( LP.ViewportClient.ViewportConsole != None ) )
	{
		PlayerConsole = LocalPlayer( Player ).ViewportClient.ViewportConsole;
		PlayerConsole.StartTyping( "TeamSay " );
	}
}

simulated function SpeakTTS( coerce string S, optional PlayerReplicationInfo PRI )
{
	local SoundCue Cue;
	local AudioComponent AC;

	Cue = CreateTTSSoundCue( S, PRI );
	if( Cue != None )
	{
		AC = CreateAudioComponent( Cue, FALSE, TRUE, , , TRUE );
		AC.bAllowSpatialization = FALSE;
		AC.bAutoDestroy = TRUE;
		AC.Play();
	}
}

reliable client event TeamMessage( PlayerReplicationInfo PRI, coerce string S, name Type, optional float MsgLifeTime  )
{
	local bool bIsUserCreated;

	if( CanCommunicate() )
	{
		if( ( ( Type == 'Say' ) || (Type == 'TeamSay' ) ) && ( PRI != None ) && AllowTTSMessageFrom( PRI ) )
		{
			if( !bIsUserCreated || ( bIsUserCreated && CanViewUserCreatedContent() ) )
			{
				SpeakTTS( S, PRI );
			}
		}

		if( myHUD != None )
		{
			myHUD.Message( PRI, S, Type, MsgLifeTime );
		}

		if( ( ( Type == 'Say' ) || ( Type == 'TeamSay' ) ) && ( PRI != None ) )
		{
			S = PRI.PlayerName$": "$S;
			// This came from a user so flag as user created
			bIsUserCreated = true;
		}

		// since this is on the client, we can assume that if Player exists, it is a LocalPlayer
		if( Player != None )
		{
			// Don't allow this if the parental controls block it
			if( !bIsUserCreated || ( bIsUserCreated && CanViewUserCreatedContent() ) )
			{
				LocalPlayer( Player ).ViewportClient.ViewportConsole.OutputText( S );
			}
		}
	}
}

function PlayBeepSound();

/**
 * Registers any handlers for delegates in the OnlineSubsystem.  Called when a player is being created and/or ControllerId is changing.
 */
function RegisterOnlineDelegates()
{
	local LocalPlayer LP;

	LP = LocalPlayer(Player);
	// If there is an online subsystem, add our callback for UI changes
	if (OnlineSub != None && LP != None)
	{
		VoiceInterface = OnlineSub.VoiceInterface;
		if (OnlineSub.SystemInterface != None)
		{
			// Register the callback for when external UI is shown/hidden
			// This will pause/unpause a single player game based upon the UI state
			OnlineSub.SystemInterface.AddExternalUIChangeDelegate(OnExternalUIChanged);
			// This will pause/unpause a single player game based upon the controller state
			OnlineSub.SystemInterface.AddControllerChangeDelegate(OnControllerChanged);
		}
		// Register for accepting game invites if possible
		if (OnlineSub.GameInterface != None)
		{
			OnlineSub.GameInterface.AddGameInviteAcceptedDelegate(LP.ControllerId,OnGameInviteAccepted);
		}
		if (OnlineSub.PartyChatInterface != None)
		{
			OnlineSub.PartyChatInterface.AddPartyMemberListChangedDelegate(LP.ControllerId,OnPartyMemberListChanged);
			OnlineSub.PartyChatInterface.AddPartyMembersInfoChangedDelegate(LP.ControllerId,OnPartyMembersInfoChanged);
		}
	}
}

/**
 * Is called when someone joins/leaves the player's party chat session
 *
 * @param bJoinedOrLeft true if the player joined, false if they left
 * @param PlayerName the name of the player that was affected
 * @param PlayerId the net id of the player that left
 */
function OnPartyMemberListChanged(bool bJoinedOrLeft,string PlayerName,UniqueNetId PlayerId);

/**
 * Is called when someone in your party chat has their custom data change (game controlled)
 *
 * @param PlayerName the name of the player that was affected
 * @param PlayerId the net id of the player that had data change
 * @param CustomData1 the first 4 bytes of the custom data
 * @param CustomData2 the second 4 bytes of the custom data
 * @param CustomData3 the third 4 bytes of the custom data
 * @param CustomData4 the fourth 4 bytes of the custom data
 */
function OnPartyMembersInfoChanged(string PlayerName,UniqueNetId PlayerId,int CustomData1,int CustomData2,int CustomData3,int CustomData4);

/**
 * Unregisters all delegates previously registered with the online subsystem.  Called when the player controller is being
 * destroyed and/or replaced.
 *
 * @note: in certain cases (i.e. when the channel is closed from the server's end), the player controller will no longer have
 * a reference its ULocalPlayer object.  these delegates won't be cleared, but GC should clear the references for us.
 */
event ClearOnlineDelegates()
{
	local LocalPlayer LP;

	`log("Clearing online delegates for" @ Self @ "(" $ `showobj(Player) $ ")",,'DevOnline');

	LP = LocalPlayer(Player);
	if (Role < ROLE_Authority || LP != None)
	{
		// If there is an online subsystem, clear our callback for UI/controller changes
		if (OnlineSub != None)
		{
			if (OnlineSub.SystemInterface != None)
			{
				OnlineSub.SystemInterface.ClearExternalUIChangeDelegate(OnExternalUIChanged);
				OnlineSub.SystemInterface.ClearControllerChangeDelegate(OnControllerChanged);
			}

			if (LP != None)
			{
				// Cleanup game invite delegate
				if (OnlineSub.GameInterface != None)
				{
					OnlineSub.GameInterface.ClearGameInviteAcceptedDelegate(LP.ControllerId, OnGameInviteAccepted);
				}
				if (OnlineSub.PartyChatInterface != None)
				{
					OnlineSub.PartyChatInterface.ClearPartyMemberListChangedDelegate(LP.ControllerId, OnPartyMemberListChanged);
					OnlineSub.PartyChatInterface.ClearPartyMembersInfoChangedDelegate(LP.ControllerId, OnPartyMembersInfoChanged);
				}
			}
		}
	}
}

/** When the Controller is destroyed, the controlled pawn will need to be killed, but not the vehicle */
function CleanupPawn()
{
	local Vehicle	DrivenVehicle;
	local Pawn		Driver;

	// If its a vehicle, just destroy the driver, otherwise do the normal.
	DrivenVehicle = Vehicle(Pawn);
	if (DrivenVehicle != None)
	{
		Driver = DrivenVehicle.Driver;
		DrivenVehicle.DriverLeave(TRUE); // Force the driver out of the car
		if ( Driver != None )
		{
			Driver.Health = 0;
			Driver.Died(Self, class'DmgType_Suicided', Driver.Location);
		}
	}
	else if (Pawn != None)
	{
		Pawn.Health = 0;
		Pawn.Died(Self, class'DmgType_Suicided', Pawn.Location);
	}
}

event Destroyed()
{
	local int EffectIdx;
	local LocalPlayer LPlayer;
	local MaterialEffect Effect;
	local MaterialInstanceConstant MIC;

	// Disable any currently playing rumble
	ClientPlayForceFeedbackWaveform(None,None);

	// if this is a local player, clear all online delegates
	if ( Role < ROLE_Authority || LocalPlayer(Player) != None )
	{
		ClearOnlineDelegates();
	}

	// cheatmanager and playerinput cleaned up in C++ PostScriptDestroyed()
	if ( Pawn != None )
	{
		CleanupPawn();
	}
	if ( myHUD != None )
	{
		myHud.Destroy();
		myHUD = None;
	}

	if( PlayerCamera != None )
	{
		PlayerCamera.Destroy();
		PlayerCamera = None;
	}

	ForceClearUnpauseDelegates();

	// remove this player's data store from the registered data stores..
	UnregisterPlayerDataStores();

	// remove instanced MICs from the PP chain
	LPlayer = LocalPlayer(Player);
	if ((LPlayer != None) && (LPlayer.PlayerPostProcess != None))
	{
		for (EffectIdx = 0; EffectIdx < LPlayer.PlayerPostProcess.Effects.Length; EffectIdx++)
		{
			Effect = MaterialEffect(LPlayer.PlayerPostProcess.Effects[EffectIdx]);
			if ((Effect != None) && (Effect.Material != None))
			{
				MIC = MaterialInstanceConstant(Effect.Material);
				if ((MIC != None) && (MIC.Parent != None))
				{
					Effect.Material = MIC.Parent;
				}
			}
		}
	}

	Super.Destroyed();
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

	if( (F >= 80.0) || (WorldInfo.NetMode==NM_Standalone) || PlayerReplicationInfo.bOnlySpectator )
	{
		DefaultFOV = FClamp(F, 80, 100);
		DesiredFOV = DefaultFOV;
	}
}

exec function Mutate(string MutateString)
{
	ServerMutate(MutateString);
}

reliable server function ServerMutate(string MutateString)
{
	if( WorldInfo.NetMode == NM_Client )
		return;
	WorldInfo.Game.Mutate(MutateString, Self);
}

// ------------------------------------------------------------------------
// Messaging functions

function bool AllowTextMessage(string Msg)
{
	local int i;

	if ( (WorldInfo.NetMode == NM_Standalone) || PlayerReplicationInfo.bAdmin )
		return true;
	if ( ( WorldInfo.Pauser == none) && (WorldInfo.TimeSeconds - LastBroadcastTime < 2 ) )
		return false;

	// lower frequency if same text
	if ( WorldInfo.TimeSeconds - LastBroadcastTime < 5 )
	{
		Msg = Left(Msg,Clamp(len(Msg) - 4, 8, 64));
		for ( i=0; i<4; i++ )
			if ( LastBroadcastString[i] ~= Msg )
				return false;
	}
	for ( i=3; i>0; i-- )
		LastBroadcastString[i] = LastBroadcastString[i-1];

	LastBroadcastTime = WorldInfo.TimeSeconds;
	return true;
}

// Send a message to all players.
exec function Say( string Msg )
{
	Msg = Left(Msg,128);

	if ( AllowTextMessage(Msg) )
		ServerSay(Msg);
}

unreliable server function ServerSay( string Msg )
{
	local PlayerController PC;

	// center print admin messages which start with #
	if (PlayerReplicationInfo.bAdmin && left(Msg,1) == "#" )
	{
		Msg = right(Msg,len(Msg)-1);
		foreach WorldInfo.AllControllers(class'PlayerController', PC)
		{
			PC.ClientAdminMessage(Msg);
		}
		return;
	}

	WorldInfo.Game.Broadcast(self, Msg, 'Say');
}

reliable client function ClientAdminMessage(string Msg)
{
	local LocalPlayer LP;

	LP = LocalPlayer(Player);
	if (LP != None)
	{
		LP.ViewportClient.ClearProgressMessages();
		LP.ViewportClient.SetProgressTime(6);
		LP.ViewportClient.SetProgressMessage(PMT_AdminMessage, Msg);
	}
}

exec function TeamSay( string Msg )
{
	Msg = Left(Msg,128);
	if ( AllowTextMessage(Msg) )
		ServerTeamSay(Msg);
}

unreliable server function ServerTeamSay( string Msg )
{
	LastActiveTime = WorldInfo.TimeSeconds;

	if( !WorldInfo.GRI.GameClass.Default.bTeamGame )
	{
		Say( Msg );
		return;
	}

    WorldInfo.Game.BroadcastTeam( self, Msg, 'TeamSay');
}

// ------------------------------------------------------------------------
/**
 * Called when the local player is about to travel to a new map or IP address.  Provides subclass with an opportunity
 * to perform cleanup or other tasks prior to the travel.
 */
event PreClientTravel( string PendingURL, ETravelType TravelType, bool bIsSeamlessTravel )
{
	local UIInteraction UIController;
	local GameUISceneClient GameSceneClient;

	// notify the UI system that we're about to perform a travel.
	UIController = GetUIController();
	if ( UIController != None && IsPrimaryPlayer() )
	{
		GameSceneClient = UIController.SceneClient;
		if ( GameSceneClient != None )
		{
			GameSceneClient.NotifyClientTravel(Self, PendingURL, TravelType, bIsSeamlessTravel);
		}
		if (PlayerInput != none)
		{
			PlayerInput.PreClientTravel(PendingURL, TravelType, bIsSeamlessTravel);
		}
	}
}

/**
 * Change Camera mode
 *
 * @param	New camera mode to set
 */
exec function Camera( name NewMode )
{
	ServerCamera(NewMode);
}

reliable server function ServerCamera( name NewMode )
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

`if(`notdefined(FINAL_RELEASE))
	if ( PlayerCamera != None )
		`log("#### " $ PlayerCamera.CameraStyle);
`endif
}

/**
 * Replicated function to set camera style on client
 *
 * @param	NewCamMode, name defining the new camera mode
 */
reliable client function ClientSetCameraMode( name NewCamMode )
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
		if ( WorldInfo.NetMode == NM_DedicatedServer )
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

reliable client event ClientSetCameraFade(bool bEnableFading, optional color FadeColor, optional vector2d FadeAlpha, optional float FadeTime, optional bool bFadeAudio)
{
	if (PlayerCamera != None)
	{
		PlayerCamera.bEnableFading = bEnableFading;
		if (PlayerCamera.bEnableFading)
		{
			PlayerCamera.FadeColor = FadeColor;
			PlayerCamera.FadeAlpha = FadeAlpha;
			PlayerCamera.FadeTime = FadeTime;
			PlayerCamera.FadeTimeRemaining = FadeTime;
			PlayerCamera.bFadeAudio = bFadeAudio;
		}
 		else
 		{
 			// Make sure FadeAmount finishes at the correct value
 			PlayerCamera.FadeAmount = PlayerCamera.FadeAlpha.Y;

			// Remove any previous fade color.
			PlayerCamera.FadeColor = FadeColor;

 			if (PlayerCamera.bFadeAudio)
 			{
 				PlayerCamera.ApplyAudioFade();
 			}
 		}
	}
}

/**
* return whether viewing in first person mode
*/
function bool UsingFirstPersonCamera()
{
	return ((PlayerCamera == None) || (PlayerCamera.CameraStyle == 'FirstPerson')) && LocalPlayer(Player) != None;
}

/* ForceDeathUpdate()
Make sure ClientAdjustPosition immediately informs client of pawn's death
*/
function ForceDeathUpdate()
{
	LastUpdateTime = WorldInfo.TimeSeconds - 10;
}

/* DualServerMove()
- replicated function sent by client to server - contains client movement and firing info for two moves
*/
unreliable server function DualServerMove
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
    ServerMove(TimeStamp0,InAccel0,vect(1,2,3),PendingFlags,ClientRoll,View0);
    ServerMove(TimeStamp,InAccel,ClientLoc,NewFlags,ClientRoll,View);
}

/* OldServerMove()
- resending an (important) old move.  Process it if not already processed.
*/
unreliable server function OldServerMove
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

		//`log("Recovered move from "$OldTimeStamp$" acceleration "$Accel$" from "$OldAccel);
		OldTimeStamp = FMin(OldTimeStamp, CurrentTimeStamp + MaxResponseTime);
		MoveAutonomous(OldTimeStamp - CurrentTimeStamp, OldMoveFlags, Accel, rot(0,0,0));
		CurrentTimeStamp = OldTimeStamp;
	}
}

/** @return time delta to use for the current ServerMove() */
function float GetServerMoveDeltaTime(float TimeStamp)
{
	local float DeltaTime;

	DeltaTime = FMin(MaxResponseTime, TimeStamp - CurrentTimeStamp);
	if( Pawn == None )
	{
		bWasSpeedHack = FALSE;
		ResetTimeMargin();
	}
	else if( !CheckSpeedHack(DeltaTime) )
	{
		if( !bWasSpeedHack )
		{
			if( WorldInfo.TimeSeconds - LastSpeedHackLog > 20 )
			{
				`log("Possible speed hack by "$PlayerReplicationInfo.PlayerName);
				LastSpeedHackLog = WorldInfo.TimeSeconds;
			}
			ClientMessage( "Speed Hack Detected!",'CriticalEvent' );
		}
		else
		{
			bWasSpeedHack = TRUE;
		}
		DeltaTime = 0;
		Pawn.Velocity = vect(0,0,0);
	}
	else
	{
		DeltaTime *= Pawn.CustomTimeDilation;
		bWasSpeedHack = FALSE;
	}

	return DeltaTime;
}

/** called after movement in ServerMove() to check for and handle any potential error between server and client position
 * by setting PendingAdjustment appropriately
 */
function ServerMoveHandleClientError(float TimeStamp, vector Accel, vector ClientLoc)
{
	local float ClientErr;
	local vector LocDiff;

	// Accumulate movement error.
	if (ClientLoc == vect(1,2,3))
	{
		return;		// first part of double servermove
	}
	else if (WorldInfo.TimeSeconds - LastUpdateTime < CLIENTADJUSTUPDATECOST / Player.CurrentNetSpeed)
	{
		// limit frequency of corrections if connection speed is limited
		return;
	}

	if (Pawn == None)
	{
		LocDiff = Location - ClientLoc;
	}
	else if (Pawn.bForceRMVelocity && Pawn.default.Mesh.RootMotionMode == RMM_Ignore)
	{
		// don't do corrections during root motion
		LocDiff = vect(0,0,0);
	}
	// force a correction periodically if the Pawn is not moving
	// this handles cases where the client incorrectly thinks movement is impossible
	// since without movement on one side or the other error can never be accumulated
	else if (Pawn.Physics != PHYS_None && WorldInfo.TimeSeconds - LastUpdateTime > 1.0 && IsZero(Accel))
	{
		LocDiff = vect(1000,1000,1000);
	}
	else
	{
		LocDiff = Pawn.Location - ClientLoc;

	}
	ClientErr = LocDiff Dot LocDiff;

	// If client has accumulated a noticeable positional error, correct him.
	if (ClientErr > MAXPOSITIONERRORSQUARED)
	{
		if (Pawn == None)
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
			if( (InterpActor(Pawn.Base) != None) || (Vehicle(Pawn.Base) != None) || DynamicBlockingVolume(Pawn.Base) != None )
			{
				PendingAdjustment.NewLoc = Pawn.Location - Pawn.Base.Location;
			}
			else
			{
				PendingAdjustment.NewLoc = Pawn.Location;
			}
			PendingAdjustment.NewFloor = Pawn.Floor;
		}

		//`log("Client Error at" @ TimeStamp @ "is" @ ClientErr @ "with acceleration" @ Accel @ "LocDiff" @ LocDiff @ "Physics" @ Pawn.Physics);

		LastUpdateTime = WorldInfo.TimeSeconds;
		PendingAdjustment.TimeStamp = TimeStamp;
		PendingAdjustment.bAckGoodMove = 0;
	}
	else
	{
		// acknowledge receipt of this successful servermove()
		PendingAdjustment.TimeStamp = TimeStamp;
		PendingAdjustment.bAckGoodMove = 1;
	}
}

/* ServerMove()
- replicated function sent by client to server - contains client movement and firing info.
*/
unreliable server function ServerMove(float TimeStamp, vector InAccel, vector ClientLoc, byte MoveFlags, byte ClientRoll, int View)
{
	local float DeltaTime;
	local rotator DeltaRot, Rot, ViewRot;
	local vector Accel;
	local int maxPitch, ViewPitch, ViewYaw;

	// If this move is outdated, discard it.
	if( CurrentTimeStamp >= TimeStamp )
	{
		return;
	}

	if( AcknowledgedPawn != Pawn )
	{
		InAccel = vect(0,0,0);
		GivePawn(Pawn);
	}

	// View components
	ViewPitch	= (View & 65535);
	ViewYaw		= (View >> 16);

	// Acceleration was scaled by 10x for replication, to keep more precision since vectors are rounded for replication
	Accel = InAccel * 0.1;
	// Save move parameters.
	DeltaTime = GetServerMoveDeltaTime(TimeStamp);

	CurrentTimeStamp = TimeStamp;
	ServerTimeStamp = WorldInfo.TimeSeconds;
	ViewRot.Pitch = ViewPitch;
	ViewRot.Yaw = ViewYaw;
	ViewRot.Roll = 0;

	if( InAccel != vect(0,0,0) )
	{
		LastActiveTime = WorldInfo.TimeSeconds;
	}

	SetRotation(ViewRot);

	if( AcknowledgedPawn != Pawn )
	{
		return;
	}

	if( Pawn != None )
	{
		Rot.Roll	= 256 * ClientRoll;
		Rot.Yaw		= ViewYaw;
		if( (Pawn.Physics == PHYS_Swimming) || (Pawn.Physics == PHYS_Flying) )
		{
			maxPitch = 2;
		}
		else
		{
			maxPitch = 0;
		}

		if( (ViewPitch > maxPitch * Pawn.MaxPitchLimit) && (ViewPitch < 65536 - maxPitch * Pawn.MaxPitchLimit) )
		{
			if( ViewPitch < 32768 )
			{
				Rot.Pitch = maxPitch * Pawn.MaxPitchLimit;
			}
			else
			{
				Rot.Pitch = 65536 - maxPitch * Pawn.MaxPitchLimit;
			}
		}
		else
		{
			Rot.Pitch = ViewPitch;
		}
		DeltaRot = (Rotation - Rot);
		Pawn.FaceRotation(Rot, DeltaTime);
	}

	// Perform actual movement
	if( (WorldInfo.Pauser == None) && (DeltaTime > 0) )
	{
		MoveAutonomous(DeltaTime, MoveFlags, Accel, DeltaRot);
	}

	ServerMoveHandleClientError(TimeStamp, Accel, ClientLoc);
	//`log("Server moved stamp "$TimeStamp$" location "$Pawn.Location$" Acceleration "$Pawn.Acceleration$" Velocity "$Pawn.Velocity);
}


/* Called on server at end of tick when PendingAdjustment has been set.
Done this way to avoid ever sending more than one ClientAdjustment per server tick.
*/
event SendClientAdjustment()
{
	if( AcknowledgedPawn != Pawn )
	{
		PendingAdjustment.TimeStamp = 0;
		return;
	}

	if( PendingAdjustment.bAckGoodMove == 1 )
	{
		// just notify client this move was received
		ClientAckGoodMove(PendingAdjustment.TimeStamp);
	}
	else if( (Pawn == None) || (Pawn.Physics != PHYS_Spider) )
	{
		if( PendingAdjustment.NewVel == vect(0,0,0) )
		{
			if (GetStateName() == 'PlayerWalking' && Pawn != None && Pawn.Physics == PHYS_Walking)
			{
				VeryShortClientAdjustPosition
				(
					PendingAdjustment.TimeStamp,
					PendingAdjustment.NewLoc.X,
					PendingAdjustment.NewLoc.Y,
					PendingAdjustment.NewLoc.Z,
					PendingAdjustment.NewBase
				);
			}
			else
			{
				ShortClientAdjustPosition
				(
					PendingAdjustment.TimeStamp,
					GetStateName(),
					PendingAdjustment.newPhysics,
					PendingAdjustment.NewLoc.X,
					PendingAdjustment.NewLoc.Y,
					PendingAdjustment.NewLoc.Z,
					PendingAdjustment.NewBase
				);
			}
		}
		else
		{
			ClientAdjustPosition
			(
				PendingAdjustment.TimeStamp,
				GetStateName(),
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
			GetStateName(),
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
unreliable server function ServerDrive(float InForward, float InStrafe, float aUp, bool InJump, int View)
{
	local rotator ViewRotation;

	ViewRotation.Pitch = (View & 65535);
	ViewRotation.Yaw = (View >> 16);
	ViewRotation.Roll = 0;
	SetRotation(ViewRotation);

	ProcessDrive(InForward, InStrafe, aUp, InJump);
}

function ProcessDrive(float InForward, float InStrafe, float InUp, bool InJump)
{
	ClientGotoState(GetStateName(), 'Begin');
}

function ProcessMove( float DeltaTime, vector newAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
{
    if( (Pawn != None) && (Pawn.Acceleration != newAccel) )
	{
		Pawn.Acceleration = newAccel;
	}
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
	{
		Pawn.AutonomousPhysics(DeltaTime);
	}
	else
	{
		AutonomousPhysics(DeltaTime);
	}
    bDoubleJump = false;
	//`log("Role "$Role$" moveauto time "$100 * DeltaTime$" ("$WorldInfo.TimeDilation$")");
}

/* VeryShortClientAdjustPosition
bandwidth saving version, when velocity is zeroed, and pawn is walking
*/
unreliable client function VeryShortClientAdjustPosition
(
	float TimeStamp,
	float NewLocX,
	float NewLocY,
	float NewLocZ,
	Actor NewBase
)
{
	local vector Floor;

	if( Pawn != None )
	{
		Floor = Pawn.Floor;
	}
	LongClientAdjustPosition(TimeStamp, 'PlayerWalking', PHYS_Walking, NewLocX, NewLocY, NewLocZ, 0, 0, 0, NewBase, Floor.X, Floor.Y, Floor.Z);
}

/* ShortClientAdjustPosition
bandwidth saving version, when velocity is zeroed
*/
unreliable client function ShortClientAdjustPosition
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

	if( Pawn != None )
	{
		Floor = Pawn.Floor;
	}

	LongClientAdjustPosition(TimeStamp, newState, newPhysics, NewLocX, NewLocY, NewLocZ, 0, 0, 0, NewBase, Floor.X, Floor.Y, Floor.Z);
}

reliable client function ClientCapBandwidth(int Cap)
{
	ClientCap = Cap;
	if( (Player != None) && (Player.CurrentNetSpeed > Cap) )
	{
		SetNetSpeed(Cap);
	}
}

unreliable client function ClientAckGoodMove(float TimeStamp)
{
	UpdatePing(TimeStamp);
	CurrentTimeStamp = TimeStamp;
	ClearAckedMoves();
}

/* ClientAdjustPosition
- pass newloc and newvel in components so they don't get rounded
*/
unreliable client function ClientAdjustPosition
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
	{
		Floor = Pawn.Floor;
	}

	LongClientAdjustPosition(TimeStamp,newState,newPhysics,NewLocX,NewLocY,NewLocZ,NewVelX,NewVelY,NewVelZ,NewBase,Floor.X,Floor.Y,Floor.Z);
}


/* epic ===============================================
* ::UpdatePing
update average ping based on newly received round trip timestamp.
Occasionally send ping updates to the server, and also adjust netspeed if connection appears to be saturated
*/
final function UpdatePing(float TimeStamp)
{
	if ( PlayerReplicationInfo != None )
	{
		PlayerReplicationInfo.UpdatePing(TimeStamp);
		if ( WorldInfo.TimeSeconds - LastPingUpdate > 4 )
		{
			LastPingUpdate = WorldInfo.TimeSeconds;
			ServerUpdatePing(1000 * PlayerReplicationInfo.ExactPing);
		}
	}
}

/** @return whether we should skip the position updating in LongClientAdjustPosition() due to current or upcoming root motion
 * (does not affect move acknowledgement or state transitions)
 */
function bool SkipPositionUpdateForRM()
{
	local SavedMove CurrentMove;

	// avoiding correcting for temporary root motion moves; wait until the animation completes
	if (Pawn != None && Pawn.default.Mesh.RootMotionMode == RMM_Ignore)
	{
		if ( Pawn.Physics != PHYS_Falling && Pawn.Mesh != None &&
			Pawn.Mesh.RootMotionMode != RMM_Ignore && !Pawn.bRootMotionFromInterpCurve )
		{
			`log("- skipping position update for root motion",,'PlayerMove');
			return true;
		}

		// don't correct if have upcoming root motion
		CurrentMove = SavedMoves;
		while( CurrentMove != None )
		{
			if ( CurrentMove.bForceRMVelocity )
			{
				`log("- skipping position update for upcoming root motion",,'PlayerMove');
				return true;
			}
			CurrentMove = CurrentMove.NextMove;
		}
	}

	return false;
}

/* LongClientAdjustPosition
long version, when care about pawn's floor normal
*/
unreliable client function LongClientAdjustPosition
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
	local Actor TheViewTarget;
`if(`notdefined(FINAL_RELEASE))
	local Vector OldLoc;

	OldLoc = (Pawn != None) ? Pawn.Location : Location;
`endif
	UpdatePing(TimeStamp);
	if( Pawn != None )
	{
		if( Pawn.bTearOff )
		{
			Pawn = None;
			if( !GamePlayEndedState() && !IsInState('Dead') )
			{
				GotoState('Dead');
			}
			return;
		}

		MoveActor = Pawn;
		TheViewTarget = GetViewTarget();

		if( (TheViewTarget != Pawn)
			&& ((TheViewTarget == self) || ((Pawn(TheViewTarget) != None) && (Pawn(TheViewTarget).Health <= 0))) )
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
			`log("- state change:"@GetStateName()@"->"@newstate,,'PlayerMove');
		    if( NewState == 'RoundEnded' )
			{
			    GotoState(NewState);
			}
			else if( IsInState('Dead') )
			{
		    	if( (NewState != 'PlayerWalking') && (NewState != 'PlayerSwimming') )
		        {
				    GotoState(NewState);
		        }
		        return;
			}
			else if( NewState == 'Dead' )
			{
				GotoState(NewState);
			}
		}
	}

	if( CurrentTimeStamp >= TimeStamp )
	{
		return;
	}
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
	while( CurrentMove != None )
	{
		if( CurrentMove.TimeStamp <= CurrentTimeStamp )
		{
			SavedMoves = CurrentMove.NextMove;
			CurrentMove.NextMove = FreeMoves;
			FreeMoves = CurrentMove;
			if( CurrentMove.TimeStamp == CurrentTimeStamp )
			{
				LastAckedAccel = CurrentMove.Acceleration;
				FreeMoves.Clear();
				if( ((InterpActor(NewBase) != None) || (Vehicle(NewBase) != None) || DynamicBlockingVolume(NewBase) != None )
					&& (NewBase == CurrentMove.EndBase) )
				{
					if ( (GetStateName() == NewState)
						&& IsInState('PlayerWalking')
						&& ((MoveActor.Physics == PHYS_Walking) || (MoveActor.Physics == PHYS_Falling)) )
					{
						if ( VSizeSq(CurrentMove.SavedRelativeLocation - NewLocation) < MAXPOSITIONERRORSQUARED )
						{
							CurrentMove = None;
							return;
						}
						else if ( (Vehicle(NewBase) != None)
								&& (VSizeSq(Velocity) < MAXNEARZEROVELOCITYSQUARED) && (VSizeSq(NewVelocity) < MAXNEARZEROVELOCITYSQUARED)
								&& (VSizeSq(CurrentMove.SavedRelativeLocation - NewLocation) < MAXVEHICLEPOSITIONERRORSQUARED) )
						{
							CurrentMove = None;
							return;
						}
					}
				}
				else if ( (VSizeSq(CurrentMove.SavedLocation - NewLocation) < MAXPOSITIONERRORSQUARED)
					&& (VSizeSq(CurrentMove.SavedVelocity - NewVelocity) < MAXNEARZEROVELOCITYSQUARED)
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
		{
			CurrentMove = None;
		}
	}

	if (MoveActor.bHardAttach)
	{
		if (MoveActor.Base == None || MoveActor.Base.bWorldGeometry)
		{
			if (NewBase != None)
			{
				MoveActor.SetLocation(NewLocation);
				MoveActor.SetPhysics(NewPhysics);
				MoveActor.SetBase(NewBase);
			}
			if( MoveActor.Base == None )
			{
				MoveActor.SetHardAttach(false);
			}
			else
			{
				return;
			}
		}
		else
		{
			return;
		}
	}

	NewFloor.X = NewFloorX;
	NewFloor.Y = NewFloorY;
	NewFloor.Z = NewFloorZ;

	//@debug - track down the errors
	`log("- base mismatch:"@MoveActor.Base@NewBase,MoveActor.Base != NewBase,'PlayerMove');
	`log("- location mismatch, delta:"@VSize(MoveActor.Location - NewLocation),MoveActor.Location != NewLocation,'PlayerMove');
	`log("- velocity mismatch, delta:"@VSize(NewVelocity - MoveActor.Velocity)@"client:"@VSize(MoveActor.Velocity)@"server:"@VSize(NewVelocity),MoveActor.Velocity != NewVelocity,'PlayerMove');

	if (SkipPositionUpdateForRM())
	{
		return;
	}

	if( (InterpActor(NewBase) != None) || (Vehicle(NewBase) != None) || DynamicBlockingVolume(NewBase) != None )
	{
		NewLocation += NewBase.Location;
	}

// 	`log("Client "$Role$" adjust "$self$" stamp "$TimeStamp$" location "$MoveActor.Location);

	MoveActor.bCanTeleport = FALSE;
	if ( !MoveActor.SetLocation(NewLocation) && (Pawn(MoveActor) != None)
		&& (Pawn(MoveActor).CylinderComponent.CollisionHeight > Pawn(MoveActor).CrouchHeight)
		&& !Pawn(MoveActor).bIsCrouched
		&& (newPhysics == PHYS_Walking)
		&& (MoveActor.Physics != PHYS_RigidBody) )
	{
		MoveActor.SetPhysics(newPhysics);

		if( !MoveActor.SetLocation(NewLocation + vect(0,0,1)*Pawn(MoveActor).MaxStepHeight) )
		{
			Pawn(MoveActor).ForceCrouch();
			MoveActor.SetLocation(NewLocation);
		}
		else
		{
			MoveActor.MoveSmooth(vect(0,0,-1)*Pawn(MoveActor).MaxStepHeight);
		}
	}
	MoveActor.bCanTeleport = TRUE;

	// Hack. Don't let network change physics mode of rigid body stuff on the client.
	if( MoveActor.Physics != PHYS_RigidBody &&
		newPhysics != PHYS_RigidBody )
	{
		MoveActor.SetPhysics(newPhysics);
	}

	if( MoveActor != self )
	{
		MoveActor.SetBase(NewBase, NewFloor);
	}

	MoveActor.Velocity = NewVelocity;
	UpdateStateFromAdjustment(NewState);
	bUpdatePosition = TRUE;

`if(`notdefined(FINAL_RELEASE))
	if( bDebugClientAdjustPosition )
	{
		DrawDebugBox( OldLoc, vect(2,2,2), 0, 120, 0, TRUE );
		DrawDebugBox( Pawn.Location, vect(3,3,3), 255, 255, 255, TRUE );
		DrawDebugLine( Pawn.Location, OldLoc, 255, 255, 255, TRUE );
		`log( `location@"!!!!!!!!!!!!!!"@SavedMoves@`showvar(Pawn.Rotation)@`showvar(WorldInfo.TimeSeconds));
	}
`endif
}


/**
Called by LongClientAdjustPosition()
@param NewState is the state recommended by the server
*/
function UpdateStateFromAdjustment(name NewState)
{
	if( GetStateName() != newstate )
	{
		GotoState(newstate);
	}
}

unreliable server function ServerUpdatePing(int NewPing)
{
	PlayerReplicationInfo.Ping = Min(0.25 * NewPing, 250);
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

/**
 * Called from PlayerTick after receiving ClientAdjustPosition call from server (and setting the bUpdating flag)
 * Client has already had position information corrected
 *
 * This function plays through previously saved moves that haven't been acknowledged by the server, predicting where the client
 * should be after the server correction
 */
function ClientUpdatePosition()
{
	local SavedMove CurrentMove;
	local int		realbRun, realbDuck;
	local bool		bRealJump;
	local bool		bRealPreciseDestination;
	local bool		bRealForceMaxAccel;
	local bool		bRealRootMotionFromInterpCurve;
	local ERootMotionMode RealRootMotionMode;
`if(`notdefined(FINAL_RELEASE))
	local Vector	OldLoc;
`endif

	bUpdatePosition = FALSE;

	// Dont do any network position updates on things running PHYS_RigidBody
	if( Pawn != None && Pawn.Physics == PHYS_RigidBody )
	{
		return;
	}

`if(`notdefined(FINAL_RELEASE))
	if( bDebugClientAdjustPosition )
	{
		`log( `location@"!!!!!!!!!!!!!!"@SavedMoves@`showvar(Pawn.Rotation)@`showvar(WorldInfo.TimeSeconds));
	}
`endif

	realbRun  = bRun;
	realbDuck = bDuck;
	bRealJump = bPressedJump;
	bUpdating = TRUE;
	bRealPreciseDestination = bPreciseDestination;
	if( Pawn != None )
	{
		bRealForceMaxAccel = Pawn.bForceMaxAccel;
		bRealRootMotionFromInterpCurve = Pawn.bRootMotionFromInterpCurve;
		RealRootMotionMode = Pawn.Mesh.RootMotionMode;
	}

	ClearAckedMoves();
	CurrentMove = SavedMoves;
	while( CurrentMove != None )
	{
		if( (PendingMove == CurrentMove) && (Pawn != None) )
		{
			PendingMove.SetInitialPosition(Pawn);
		}

`if(`notdefined(FINAL_RELEASE))
	if( bDebugClientAdjustPosition )
	{
		`log( CurrentMove.GetDebugString() );
		`log( "Old"@Pawn.Location@Pawn.bRootMotionFromInterpCurve@Pawn.RootMotionInterpCurrentTime );
		OldLoc = Pawn.Location;
	}
`endif

		CurrentMove.PrepMoveFor( Pawn );
		MoveAutonomous(CurrentMove.Delta, CurrentMove.CompressedFlags(), CurrentMove.Acceleration, rot(0,0,0));
		CurrentMove.ResetMoveFor( Pawn );

`if(`notdefined(FINAL_RELEASE))
	if( bDebugClientAdjustPosition )
	{
		`log( "New"@Pawn.Location@Pawn.bRootMotionFromInterpCurve@Pawn.RootMotionInterpCurrentTime );
		DrawDebugBox( OldLoc, vect(4,4,4), 120, 0, 0, TRUE );
		DrawDebugBox( Pawn.Location, vect(5,5,5), 0, 0, 120, TRUE );
		DrawDebugLine( OldLoc + vect(0,0,2), Pawn.Location + vect(0,0,2), 0, 120, 0, TRUE );
	}
`endif

		CurrentMove = CurrentMove.NextMove;
	}

	bUpdating = FALSE;
	bDuck = realbDuck;
	bRun = realbRun;
	bPressedJump = bRealJump;
	bPreciseDestination = bRealPreciseDestination;
	if( Pawn != None )
	{
		Pawn.bForceMaxAccel = bRealForceMaxAccel;
		Pawn.bRootMotionFromInterpCurve = bRealRootMotionFromInterpCurve;
		Pawn.Mesh.RootMotionMode = RealRootMotionMode;
	}
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
				PendingMove = None;
				return first;
			}
		}
		return new(self) SavedMoveClass;
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

	// do nothing if we are no longer connected
	if (Player == None)
	{
		return;
	}

	MaxResponseTime = Default.MaxResponseTime * WorldInfo.TimeDilation;
	DeltaTime = ((Pawn != None) ? Pawn.CustomTimeDilation : CustomTimeDilation) * FMin(DeltaTime, MaxResponseTime);

	// find the most recent move (LastMove), and the oldest (unacknowledged) important move (OldMove)
	// a SavedMove is interesting if it differs significantly from the last acknowledged move
	if ( SavedMoves != None )
	{
		LastMove = SavedMoves;
		AlmostLastMove = LastMove;
		OldMove = None;
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

	// Get a SavedMove object to store the movement in.
	NewMove = GetFreeMove();
	if ( NewMove == None )
	{
		return;
	}
	NewMove.SetMoveFor(self, DeltaTime, NewAccel, DoubleClickMove);

	// Simulate the movement locally.
	bDoubleJump = false;
	ProcessMove(NewMove.Delta, NewMove.Acceleration, NewMove.DoubleClickMove, DeltaRot);

	// see if the two moves could be combined
	if ( (PendingMove != None) && PendingMove.CanCombineWith(NewMove, Pawn, MaxResponseTime) )
	{
		// to combine move, first revert pawn position to PendingMove start position, before playing combined move on client
		Pawn.SetLocation(PendingMove.GetStartLocation());
		Pawn.Velocity = PendingMove.StartVelocity;
		if( PendingMove.StartBase != Pawn.Base )
		{
			Pawn.SetBase(PendingMove.StartBase);
		}
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

	if( Pawn != None )
	{
		Pawn.AutonomousPhysics(NewMove.Delta);
	}
	else
	{
		AutonomousPhysics(DeltaTime);
	}
	NewMove.PostUpdate(self);

	if( SavedMoves == None )
	{
		SavedMoves = NewMove;
	}
	else
	{
		LastMove.NextMove = NewMove;
	}

	if ( PendingMove == None )
	{
		// Decide whether to hold off on move
		// send moves more frequently in small games where server isn't likely to be saturated
		if( (Player.CurrentNetSpeed > 10000) && (WorldInfo.GRI != None) && (WorldInfo.GRI.PRIArray.Length <= 10) )
		{
			NetMoveDelta = 0.011;
		}
		else
		{
			NetMoveDelta = FMax(0.0222,2 * WorldInfo.MoveRepSize/Player.CurrentNetSpeed);
		}

		if( (WorldInfo.TimeSeconds - ClientUpdateTime) * WorldInfo.TimeDilation < NetMoveDelta )
		{
			PendingMove = NewMove;
			return;
		}
	}

	ClientUpdateTime = WorldInfo.TimeSeconds;

	// Send to the server
	ClientRoll = (Rotation.Roll >> 8) & 255;

	CallServerMove( NewMove,
			((Pawn == None) ? Location : Pawn.Location),
			ClientRoll,
			((Rotation.Yaw & 65535) << 16) + (Rotation.Pitch & 65535),
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
    byte ClientRoll,
    int View,
    SavedMove OldMove
)
{
	local vector BuildAccel;
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

	if ( PendingMove != None )
	{
		// send two moves simultaneously
		DualServerMove
		(
			PendingMove.TimeStamp,
			PendingMove.Acceleration * 10,
			PendingMove.CompressedFlags(),
			((PendingMove.Rotation.Yaw & 65535) << 16) + (PendingMove.Rotation.Pitch & 65535),
			NewMove.TimeStamp,
			NewMove.Acceleration * 10,
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
	    NewMove.Acceleration * 10,
	    ClientLoc,
		NewMove.CompressedFlags(),
		ClientRoll,
	    View
		);
	}

	if (PlayerCamera != None && PlayerCamera.bUseClientSideCameraUpdates)
	{
		PlayerCamera.bShouldSendClientSideCameraUpdate = TRUE;
	}
}

/** If PlayerCamera.bUseClientSideCameraUpdates is set, client will replicate camera positions to the server. */
// @TODO - combine pitch/yaw into one int, maybe also send location compressed
unreliable server function ServerUpdateCamera(vector CamLoc, int CamPitchAndYaw)
{
	local TPOV		NewPOV;

	NewPOV.Location = CamLoc;

	NewPOV.Rotation.Yaw = (CamPitchAndYaw >> 16) & 65535;
	NewPOV.Rotation.Pitch = CamPitchAndYaw & 65535;

	if ( PlayerCamera.bDebugClientSideCamera )
	{
		// show differences (on server) between local and replicated camera
	  DrawDebugSphere( PlayerCamera.CameraCache.POV.Location, 10, 10, 0, 255, 0 );
	  DrawDebugSphere(NewPOV.Location, 10, 10, 255, 255, 0 );
	  DrawDebugLine(PlayerCamera.CameraCache.POV.Location, PlayerCamera.CameraCache.POV.Location + 100*vector(PlayerCamera.CameraCache.POV.Rotation), 0, 255, 0);
	  DrawDebugLine(NewPOV.Location, NewPOV.Location + 100*vector(NewPOV.Rotation), 255, 255, 0);
	}
	else
	{
		PlayerCamera.FillCameraCache(NewPOV);
	}
}

/* HandleWalking:
	Called by PlayerController and PlayerInput to set bIsWalking flag, affecting Pawn's velocity */
function HandleWalking()
{
	if ( Pawn != None )
		Pawn.SetWalking( bRun != 0 );
}

reliable server function ServerRestartGame()
{
}

// Send a voice message of a certain type to a certain player.
exec function Speech( name Type, int Index, string Callsign )
{
	ServerSpeech(Type,Index,Callsign);
}

reliable server function ServerSpeech( name Type, int Index, string Callsign );

exec function RestartLevel()
{
	if( WorldInfo.NetMode==NM_Standalone )
	{
		ClientTravel( "?restart", TRAVEL_Relative );
	}
}

exec function LocalTravel( string URL )
{
	if( WorldInfo.NetMode==NM_Standalone )
		ClientTravel( URL, TRAVEL_Relative );
}

/**
 * Pause force-feedback for all players.
 *
 * @param	bShouldPauseRumble	indicates whether force-feedback should be paused or unpaused.
 */
function PauseRumbleForAllPlayers( optional bool bShouldPauseRumble=true )
{
	local PlayerController PC;

	foreach WorldInfo.AllControllers(class'PlayerController', PC)
	{
		PC.ClientPauseRumble(bShouldPauseRumble);
	}
}

/**
 * Pause force-feedback for this player.
 *
 * @param bShouldPauseRumble indicates whether force-feedback should be paused or unpaused.
 */
reliable client function ClientPauseRumble(bool bShouldPauseRumble)
{
	if (ForceFeedbackManager != None)
	{
		ForceFeedbackManager.PauseWaveform(bShouldPauseRumble);
	}
}

/** Callback the server uses to determine if the unpause can happen */
delegate bool CanUnpause()
{
	return WorldInfo.Pauser == PlayerReplicationInfo;
}

/**  Try to pause game; returns success indicator. */
function bool SetPause( bool bPause, optional delegate<CanUnpause> CanUnpauseDelegate=CanUnpause)
{
	local bool bResult;

	if (WorldInfo.NetMode != NM_Client)
	{
		if (bPause)
		{
			bFire = 0;
			// Pause gamepad rumbling too if needed
			bResult = WorldInfo.Game.SetPause(self,CanUnpauseDelegate);
			if (bResult)
			{
				PauseRumbleForAllPlayers();
			}
		}
		else
		{
			WorldInfo.Game.ClearPause();
			// If the unpause is complete, let rumble occur
			if ( WorldInfo.Pauser == None )
			{
				// If we did a gameplay frame pause clear it out now
				WorldInfo.bGameplayFramePause = false;

				PauseRumbleForAllPlayers(false);
			}
		}
	}
	return bResult;
}


/**
 * Returns whether the game is currently paused.
 */
final simulated function bool IsPaused()
{
	return WorldInfo.Pauser != None;
}

/* Pause()
Command to try to pause the game.
*/
exec function Pause()
{
	ServerPause();
}

reliable server function ServerPause()
{
	// Pause if not already
	if( !IsPaused() )
		SetPause(true);
	else
		SetPause(false);
}

/**
 * Toggles the game's paused state if it does not match the desired pause state.
 *
 * @param	bDesiredPauseState	TRUE indicates that the game should be paused.
 */
event ConditionalPause( bool bDesiredPauseState )
{
	if ( bDesiredPauseState != IsPaused() )
	{
		SetPause(bDesiredPauseState);
	}
}

reliable server function ServerUTrace()
{
	if (WorldInfo.NetMode != NM_Standalone
	&& (PlayerReplicationInfo == None || !PlayerReplicationInfo.bAdmin) )
	{
		return;
	}

	UTrace();
}


exec function UTrace()
{
	// disable the log, or we'll grind the game to a halt
	ConsoleCommand("hidelog");
	if ( Role != ROLE_Authority )
	{
		ServerUTrace();
	}

	SetUTracing( !IsUTracing() );
	`log("UTracing changed to "$ IsUTracing() $ " at "$ WorldInfo.TimeSeconds,,'UTrace');
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

reliable server function ServerThrowWeapon()
{
    if ( Pawn.CanThrowWeapon() )
    {
		Pawn.ThrowActiveWeapon();
    }
}

/* PrevWeapon()
- switch to previous inventory group weapon
*/
exec function PrevWeapon()
{
	if ( WorldInfo.Pauser!=None )
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
	if ( WorldInfo.Pauser!=None )
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
	if ( WorldInfo.Pauser == PlayerReplicationInfo )
	{
		SetPause( false );
		return;
	}

	if ( Pawn != None && !bCinematicMode && !WorldInfo.bPlayersOnly )
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
 * @param		interactDistanceToCheck - distance to search for nearby triggers
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
function GetTriggerUseList(float interactDistanceToCheck, float crosshairDist, float minDot, bool bUsuableOnly, out array<Trigger> out_useList)
{
	local int Idx;
	local vector cameraLoc;
	local rotator cameraRot;
	local Trigger checkTrigger;
	local SeqEvent_Used	UseSeq;

	if (Pawn != None)
	{
		// grab camera location/rotation for checking crosshairDist
		GetPlayerViewPoint(cameraLoc, cameraRot);

		// This doesn't work how it should.  It really needs to query ALL of the triggers and get their
		// InteractDistance and then compare those against the pawn's location and then do the various checks

		// search of nearby actors that have use events
		foreach Pawn.CollidingActors(class'Trigger',checkTrigger,interactDistanceToCheck)
		{
			for (Idx = 0; Idx < checkTrigger.GeneratedEvents.Length; Idx++)
			{
				UseSeq = SeqEvent_Used(checkTrigger.GeneratedEvents[Idx]);

				if( ( UseSeq != None )
					// if bUsuableOnly is true then we must get true back from CheckActivate (which tests various validity checks on the player and on the trigger's trigger count and retrigger conditions etc)
					&& ( !bUsuableOnly || ( checkTrigger.GeneratedEvents[Idx].CheckActivate(checkTrigger,Pawn,true)) )
					// check to see if we are looking at the object
					&& ( Normal(checkTrigger.Location-cameraLoc) dot vector(cameraRot) >= minDot )

					// if this is an aimToInteract then check to see if we are aiming at the object and we are inside the InteractDistance (NOTE: we need to do use a number close to 1.0 as the dot will give a number that is very close to 1.0 for aiming at the target)
					&& ( ( ( UseSeq.bAimToInteract && IsAimingAt( checkTrigger, 0.98f ) && ( VSize(Pawn.Location - checkTrigger.Location) <= UseSeq.InteractDistance ) ) )
					      // if we should NOT aim to interact then we need to be close to the trigger
			  || ( !UseSeq.bAimToInteract && ( VSize(Pawn.Location - checkTrigger.Location) <= UseSeq.InteractDistance ) )  // this should be UseSeq.InteractDistance
						  )
				   )
				{
					out_useList[out_useList.Length] = checkTrigger;

					// don't bother searching for more events
					Idx = checkTrigger.GeneratedEvents.Length;
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
unreliable server function ServerUse()
{
	PerformedUseAction();
}

/**
 * return true if player the Use action was handled
 */
function bool PerformedUseAction()
{
	// if the level is paused,
	if( WorldInfo.Pauser == PlayerReplicationInfo )
	{
		if( Role == Role_Authority )
		{
			// unpause and move on
			SetPause( false );
		}
		return true;
	}

    if ( Pawn == None )
	{
		return true;
	}

	// below is only on server
	if( Role < Role_Authority )
	{
		return false;
	}

	// leave vehicle if currently in one
	if( Vehicle(Pawn) != None )
	{
		return Vehicle(Pawn).DriverLeave(false);
	}

	// try to find a vehicle to drive
	if( FindVehicleToDrive() )
	{
		return true;
	}

	// try to interact with triggers
	return TriggerInteracted();
}

/** Tries to find a vehicle to drive within a limited radius. Returns true if successful */
function bool FindVehicleToDrive()
{
	local Vehicle V, Best;
	local vector ViewDir, PawnLoc2D, VLoc2D;
	local float NewDot, BestDot;

	if (Vehicle(Pawn.Base) != None  && Vehicle(Pawn.Base).TryToDrive(Pawn))
	{
		return true;
	}

	// Pick best nearby vehicle
	PawnLoc2D = Pawn.Location;
	PawnLoc2D.Z = 0;
	ViewDir = vector(Pawn.Rotation);

	ForEach Pawn.OverlappingActors(class'Vehicle', V, Pawn.VehicleCheckRadius)
	{
		// Prefer vehicles that Pawn is facing
		VLoc2D = V.Location;
		Vloc2D.Z = 0;
		NewDot = Normal(VLoc2D-PawnLoc2D) Dot ViewDir;
		if ( (Best == None) || (NewDot > BestDot) )
		{
			// check that vehicle is visible
			if ( FastTrace(V.Location,Pawn.Location) )
			{
				Best = V;
				BestDot = NewDot;
			}
		}
	}
	return (Best != None && Best.TryToDrive(Pawn));
}

/**
 * Examines the nearby enviroment and generates a priority sorted
 * list of interactable actors, and then attempts to activate each
 * of them until either one was successfully activated, or no more
 * actors are available.
 */
function bool TriggerInteracted()
{
	local Actor A;
	local int Idx;
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
				for (Idx = 0; Idx < sortedList.Length && !bInserted; Idx++)
				{
					if (weightList[Idx] < weight)
					{
						// insert the new entry
						sortedList.Insert(Idx,1);
						weightList.Insert(Idx,1);
						sortedList[Idx] = A;
						weightList[Idx] = weight;
						bInserted = true;
					}
				}
				// if no insertion was made
				if (!bInserted)
				{
					// then tack on the end of the list
					Idx = sortedList.Length;
					sortedList[Idx] = A;
					weightList[Idx] = weight;
				}
			}
			// finally iterate through each actor in the sorted list and
			// attempt to activate it until one succeeds or none are left
			for (Idx = 0; Idx < sortedList.Length; Idx++)
			{
				if (sortedList[Idx].UsedBy(Pawn))
				{
					// skip the rest
					// Idx = sortedList.Length;
					return true;
				}
			}
		}
	}

	return false;
}


exec function Suicide()
{
	ServerSuicide();
}

reliable server function ServerSuicide()
{
	if ( (Pawn != None) && ((WorldInfo.TimeSeconds - Pawn.LastStartTime > 10) || (WorldInfo.NetMode == NM_Standalone)) )
	{
		Pawn.Suicide();
	}
}

exec function SetName(coerce string S)
{
	local string NewName;
	local LocalPlayer LocPlayer;

	if (S != "")
	{
		LocPlayer = LocalPlayer(Player);
		if (LocPlayer != None &&
			OnlineSub.GameInterface != None &&
			OnlineSub.PlayerInterface != None)
		{
			// Check to see if they are logged in locally or not
			if (OnlineSub.PlayerInterface.GetLoginStatus(LocPlayer.ControllerId) == LS_LoggedIn &&
				OnlineSub.GameInterface.GetGameSettings('Game') != None)
			{
				// Ignore what ever was specified and use the profile's nick
				S = OnlineSub.PlayerInterface.GetPlayerNickname(LocPlayer.ControllerId);
			}
		}

		NewName = S;

		ServerChangeName(NewName);
		UpdateURL("Name", NewName, true);
		SaveConfig();
	}
}

reliable server function ServerChangeName( coerce string S )
{
	if (S != "")
	{
		WorldInfo.Game.ChangeName( self, S, true );
	}
}

exec function SwitchTeam()
{
	if ( (PlayerReplicationInfo.Team == None) || (PlayerReplicationInfo.Team.TeamIndex == 1) )
	{
		ServerChangeTeam(0);
	}
	else
	{
		ServerChangeTeam(1);
	}
}

exec function ChangeTeam( optional string TeamName )
{
	local int N;

	if ( TeamName ~= "blue" )
		N = 1;
	else if ( (TeamName ~= "red") || (PlayerReplicationInfo == None) || (PlayerReplicationInfo.Team == None) || (PlayerReplicationInfo.Team.TeamIndex > 1) )
		N = 0;
	else
		N = 1 - PlayerReplicationInfo.Team.TeamIndex;

	ServerChangeTeam(N);
}

reliable server function ServerChangeTeam(int N)
{
	local TeamInfo OldTeam;

	OldTeam = PlayerReplicationInfo.Team;
	WorldInfo.Game.ChangeTeam(self, N, true);
	if (WorldInfo.Game.bTeamGame && PlayerReplicationInfo.Team != OldTeam)
	{
		if (Pawn != None)
		{
			Pawn.PlayerChangedTeam();
		}
	}
}


exec function SwitchLevel(string URL)
{
	if (WorldInfo.NetMode == NM_Standalone || WorldInfo.NetMode == NM_ListenServer)
	{
		WorldInfo.ServerTravel(URL);
	}
}

/** server to client RPC for server-generated network messages that aren't part of the connection process (e.g. being kicked) */
reliable client event ClientSetProgressMessage(EProgressMessageType MessageType, string Message, optional string Title, optional bool bIgnoreFutureNetworkMessages)
{
	if (LocalPlayer(Player) != None)
	{
		LocalPlayer(Player).ViewportClient.SetProgressMessage(MessageType, Message, Title, bIgnoreFutureNetworkMessages);
	}
	else
	{
		`Warn("Discarded progress message due to no viewport:" @ MessageType @ Message @ Title);
	}
}

function Restart(bool bVehicleTransition)
{
	Super.Restart(bVehicleTransition);
	ServerTimeStamp = 0;
	ResetTimeMargin();
	EnterStartState();
	ClientRestart(Pawn);
	SetViewTarget(Pawn);
	ResetCameraMode();
}

/** called to notify the server when the client has loaded a new world via seamless travelling
 * @param WorldPackageName the name of the world package that was loaded
 */
reliable server native final event ServerNotifyLoadedWorld(name WorldPackageName);

/** called clientside when it is loaded a new world via seamless travelling
 * @param WorldPackageName the name of the world package that was loaded
 * @param bFinalDest whether this world is the destination map for the travel (i.e. not the transition level)
 */
event NotifyLoadedWorld(name WorldPackageName, bool bFinalDest)
{
	local PlayerStart P;
	local rotator SpawnRotation;


	// place the camera at the first playerstart we can find
	SetViewTarget(self);
	foreach WorldInfo.AllNavigationPoints(class'PlayerStart', P)
	{
		SetLocation(P.Location);
		SpawnRotation.Yaw = P.Rotation.Yaw;
		SetRotation(SpawnRotation);
		break;
	}
}

/** returns whether the client has completely loaded the server's current world (valid on server only) */
native final function bool HasClientLoadedCurrentWorld();

/** forces a full replication check of the specified Actor on only the client that owns this PlayerController
 * (equivalent to setting bForceNetUpdate and bNetDirty to true, but only for that client)
 * this function has no effect if this PC is not a remote client or if the Actor is not relevant to that client
 */
native final function ForceSingleNetUpdateFor(Actor Target);

function EnterStartState()
{
	local name NewState;

	if ( Pawn.PhysicsVolume.bWaterVolume )
	{
		if ( Pawn.HeadVolume.bWaterVolume )
		{
			Pawn.BreathTime = Pawn.UnderWaterTime;
		}
		NewState = Pawn.WaterMovementState;
	}
	else
	{
		NewState = Pawn.LandMovementState;
	}

	if (GetStateName() == NewState)
	{
		BeginState(NewState);
	}
	else
	{
		GotoState(NewState);
	}
}

reliable client function ClientRestart(Pawn NewPawn)
{
	ResetPlayerMovementInput();
    CleanOutSavedMoves();  // don't replay moves previous to possession

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
	if (Role < ROLE_Authority)
	{
		SetViewTarget(Pawn);
		ResetCameraMode();
		EnterStartState();
	}
	CleanOutSavedMoves();
}

/* epic ===============================================
* ::GameHasEnded
*
* Called from game info upon end of the game, used to
* transition to proper state.
*
* =====================================================
*/
function GameHasEnded(optional Actor EndGameFocus, optional bool bIsWinner)
{
	// and transition to the game ended state
	SetViewTarget(EndGameFocus);
	GotoState('RoundEnded');
	ClientGameEnded(EndGameFocus, bIsWinner);
}

/* epic ===============================================
* ::ClientGameEnded
*
* Replicated function called by GameHasEnded().
*
 * @param	EndGameFocus - actor to view with camera
 * @param	bIsWinner - true if this controller is on winning team
* =====================================================
*/
reliable client function ClientGameEnded(Actor EndGameFocus, bool bIsWinner)
{
	SetViewTarget(EndGameFocus);
	GotoState('RoundEnded');
}

// Just changed to pendingWeapon
function NotifyChangedWeapon( Weapon PreviousWeapon, Weapon NewWeapon );

/**
 * PlayerTick is only called if the PlayerController has a PlayerInput object.  Therefore, it will not be called on servers for non-locally controlled playercontrollers
 */
event PlayerTick( float DeltaTime )
{
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
	{
		ClientUpdatePosition();
	}
	PlayerMove(DeltaTime);

	AdjustFOV(DeltaTime);
}

function PlayerMove(float DeltaTime);

function bool AimingHelp(bool bInstantHit)
{
 return (WorldInfo.NetMode == NM_Standalone) && bAimingHelp;
}

/** The function called when a CameraLookAt action is deactivated from kismet */
event CameraLookAtFinished(SeqAct_CameraLookAt Action);

/**
 * Adjusts weapon aiming direction.
 * Gives controller a chance to modify the aiming of the pawn. For example aim error, auto aiming, adhesion, AI help...
 * Requested by weapon prior to firing.
 *
 * @param	W, weapon about to fire
 * @param	StartFireLoc, world location of weapon fire start trace, or projectile spawn loc.
 * @param	BaseAimRot, original aiming rotation without any modifications.
 */
function Rotator GetAdjustedAimFor( Weapon W, vector StartFireLoc )
{
	local vector	FireDir, AimSpot, HitLocation, HitNormal, OldAim, AimOffset;
	local actor		BestTarget, HitActor;
	local float		bestAim, bestDist;
	local bool		bNoZAdjust, bInstantHit;
	local rotator	BaseAimRot, AimRot;

	bInstantHit = ( W == None || W.bInstantHit );
	BaseAimRot = (Pawn != None) ? Pawn.GetBaseAimRotation() : Rotation;
	FireDir	= vector(BaseAimRot);
	HitActor = Trace(HitLocation, HitNormal, StartFireLoc + W.GetTraceRange() * FireDir, StartFireLoc, true);

	if ( (HitActor != None) && HitActor.bProjTarget )
	{
		BestTarget = HitActor;
		bNoZAdjust = true;
		OldAim = HitLocation;
		BestDist = VSize(BestTarget.Location - Pawn.Location);
	}
	else
	{
		// adjust aim based on FOV
		bestAim = 0.90;
		if ( AimingHelp(bInstantHit) )
		{
			bestAim = AimHelpDot(bInstantHit);
		}
		else if ( bInstantHit )
			bestAim = 1.0;

		BestTarget = PickTarget(class'Pawn', bestAim, bestDist, FireDir, StartFireLoc, W.WeaponRange);
		if ( BestTarget == None )
		{
			return BaseAimRot;
		}
		OldAim = StartFireLoc + FireDir * bestDist;
	}

	ShotTarget = Pawn(BestTarget);
    if ( !AimingHelp(bInstantHit) )
	{
    	return BaseAimRot;
	}

	// aim at target - help with leading also
	FireDir = BestTarget.Location - StartFireLoc;
	AimSpot = StartFireLoc + bestDist * Normal(FireDir);
	AimOffset = AimSpot - OldAim;

    if ( ShotTarget != None )
    {
	    // adjust Z of shooter if necessary
	    if ( bNoZAdjust )
	        AimSpot.Z = OldAim.Z;
	    else if ( AimOffset.Z < 0 )
	        AimSpot.Z = ShotTarget.Location.Z + 0.4 * ShotTarget.CylinderComponent.CollisionHeight;
	    else
	        AimSpot.Z = ShotTarget.Location.Z - 0.7 * ShotTarget.CylinderComponent.CollisionHeight;
    }
    else
	    AimSpot.Z = OldAim.Z;

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
	return rotator(AimSpot - StartFireLoc);
}

/** AimHelpDot()
* @returns the dot product corresponding to the maximum deflection of target for which aiming help should be applied
*/
function float AimHelpDot(bool bInstantHit)
{
	if ( FOVAngle < DefaultFOV - 8 )
		return 0.99;
	if ( bInstantHit )
		return 0.97;
	return 0.93;
}

event bool NotifyLanded(vector HitNormal, Actor FloorActor)
{
	return bUpdating;
}

//=============================================================================
// Player Control

// Player view.
// Compute the rendering viewpoint for the player.
//

/** AdjustFOV()
FOVAngle smoothly interpolates to DesiredFOV
*/
function AdjustFOV(float DeltaTime )
{
	if ( FOVAngle != DesiredFOV )
	{
		if ( FOVAngle > DesiredFOV )
			FOVAngle = FOVAngle - FMax(7, 0.9 * DeltaTime * (FOVAngle - DesiredFOV));
		else
			FOVAngle = FOVAngle - FMin(-7, 0.9 * DeltaTime * (FOVAngle - DesiredFOV));
		if ( Abs(FOVAngle - DesiredFOV) <= 10 )
			FOVAngle = DesiredFOV;
	}
}

/** returns player's FOV angle */
event float GetFOVAngle()
{
	return (PlayerCamera != None) ? PlayerCamera.GetFOVAngle() : FOVAngle;
}

/** returns whether this Controller is a locally controlled PlayerController
 * @note not valid until the Controller is completely spawned (i.e, unusable in Pre/PostBeginPlay())
 */
native function bool IsLocalPlayerController();

/** returns whether this controller is a local controller.
  * @RETURN true if NM_Standalone, or is local playercontroller
 */
native function bool IsLocalController();

native function SetViewTarget(Actor NewViewTarget, optional ViewTargetTransitionParams TransitionParams);

/** Wrapper to SetViewTarget with useful defaults */
final function SetViewTargetWithBlend(Actor NewViewTarget, optional float BlendTime = 0.35, optional EViewTargetBlendFunction BlendFunc = VTBlend_Cubic, optional float BlendExp = 2.f, optional bool bLockOutgoing = FALSE)
{
	local ViewTargetTransitionParams TransitionParams;
	TransitionParams.BlendTime = BlendTime;
	TransitionParams.BlendFunction = BlendFunc;
	TransitionParams.BlendExp = BlendExp;
	TransitionParams.bLockOutgoing = bLockOutgoing;
	SetViewTarget(NewViewTarget,TransitionParams);
}

reliable client event ClientSetViewTarget( Actor A, optional ViewTargetTransitionParams TransitionParams )
{
	if (!bClientSimulatingViewTarget)
	{
		if( A == None )
		{
			ServerVerifyViewTarget();
		}
		SetViewTarget(A, TransitionParams);
	}
}

native function Actor GetViewTarget();

reliable server function ServerVerifyViewTarget()
{
	local Actor TheViewTarget;

	TheViewTarget = GetViewTarget();

	if( TheViewTarget == Self )
	{
		return;
	}
	ClientSetViewTarget( TheViewTarget );
}

event SpawnPlayerCamera()
{
	if (CameraClass != None)
	{
		// Associate Camera with PlayerController
		PlayerCamera = Spawn( CameraClass, self );
		if( PlayerCamera != None )
		{
			PlayerCamera.InitializeFor( self );
		}
		else
		{
			`Log( "Couldn't Spawn Camera Actor for Player!!" );
		}
	}
	else
	{
		// not having a CameraClass is fine.  Another class will handing the "camera" type duties
	// usually PlayerController
	}
}

/**
 * Returns Player's Point of View
 * For the AI this means the Pawn's 'Eyes' ViewPoint
 * For a Human player, this means the Camera's ViewPoint
 *
 * @output	out_Location, view location of player
 * @output	out_rotation, view rotation of player
 */
simulated event GetPlayerViewPoint( out vector out_Location, out Rotator out_Rotation )
{
	local Actor TheViewTarget;

	// sometimes the PlayerCamera can be none and we probably do not want this
	// so we will check to see if we have a CameraClass.  Having a CameraClass is
	// saying:  we want a camera so make certain one exists by spawning one
	if( PlayerCamera == None )
	{
		if( CameraClass != None )
		{
			// Associate Camera with PlayerController
			PlayerCamera = Spawn(CameraClass, Self);
			if( PlayerCamera != None )
			{
				PlayerCamera.InitializeFor( Self );
			}
			else
			{
				`log("Couldn't Spawn Camera Actor for Player!!");
			}
		}
	}

	if( PlayerCamera != None )
	{
		PlayerCamera.GetCameraViewPoint(out_Location, out_Rotation);
	}
	else
	{
		TheViewTarget = GetViewTarget();

		if( TheViewTarget != None )
		{
			out_Location = TheViewTarget.Location;
			out_Rotation = TheViewTarget.Rotation;
		}
		else
		{
			super.GetPlayerViewPoint(out_Location, out_Rotation);
		}
	}
}

/** Updates any camera view shaking that is going on */
function ViewShake(float DeltaTime);

function UpdateRotation( float DeltaTime )
{
	local Rotator	DeltaRot, newRotation, ViewRotation;

	ViewRotation = Rotation;
	if (Pawn!=none)
	{
		Pawn.SetDesiredRotation(ViewRotation);
	}

	// Calculate Delta to be applied on ViewRotation
	DeltaRot.Yaw	= PlayerInput.aTurn;
	DeltaRot.Pitch	= PlayerInput.aLookUp;

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

		// Add Delta Rotation
		out_ViewRotation	+= DeltaRot;
		out_ViewRotation	 = LimitViewRotation(out_ViewRotation, -16384, 16383 );
	}
}


/**
 * Limit the player's view rotation. (Pitch component).
 */
event Rotator LimitViewRotation( Rotator ViewRotation, float ViewPitchMin, float ViewPitchMax )
{
	ViewRotation.Pitch = ViewRotation.Pitch & 65535;

    if( ViewRotation.Pitch > ViewPitchMax &&
		ViewRotation.Pitch < (65535+ViewPitchMin) )
	{
		if( ViewRotation.Pitch < 32768 )
		{
			ViewRotation.Pitch = ViewPitchMax;
		}
		else
		{
			ViewRotation.Pitch = 65535 + ViewPitchMin;
		}
	}

	return ViewRotation;
}

/* CheckJumpOrDuck()
Called by ProcessMove()
handle jump and duck buttons which are pressed
*/
function CheckJumpOrDuck()
{
	if ( bPressedJump && (Pawn != None) )
	{
		Pawn.DoJump( bUpdating );
	}
}

/**
 * Allow player controllers to adjust the acceleration in PlayerWalking
 *
 * @param NewAccel - the acceleration used by PlayerWalking::PlayerMove
 */
function AdjustPlayerWalkingMoveAccel(out vector NewAccel);

// Player movement.
// Player Standing, walking, running, falling.
state PlayerWalking
{
ignores SeePlayer, HearNoise, Bump;

	event NotifyPhysicsVolumeChange( PhysicsVolume NewVolume )
	{
		if ( NewVolume.bWaterVolume && Pawn.bCollideWorld )
		{
			GotoState(Pawn.WaterMovementState);
		}
	}

	function ProcessMove(float DeltaTime, vector NewAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
	{
		if( Pawn == None )
		{
			return;
		}

		if (Role == ROLE_Authority)
		{
			// Update ViewPitch for remote clients
			Pawn.SetRemoteViewPitch( Rotation.Pitch );
		}

		Pawn.Acceleration = NewAccel;

		CheckJumpOrDuck();
	}

	function PlayerMove( float DeltaTime )
	{
		local vector			X,Y,Z, NewAccel;
		local eDoubleClickDir	DoubleClickMove;
		local rotator			OldRotation;
		local bool				bSaveJump;

		if( Pawn == None )
		{
			GotoState('Dead');
		}
		else
		{
			GetAxes(Pawn.Rotation,X,Y,Z);

			// Update acceleration.
			NewAccel = PlayerInput.aForward*X + PlayerInput.aStrafe*Y;
			NewAccel.Z	= 0;
			NewAccel = Pawn.AccelRate * Normal(NewAccel);

			if (IsLocalPlayerController())
			{
				AdjustPlayerWalkingMoveAccel(NewAccel);
			}

			DoubleClickMove = PlayerInput.CheckForDoubleClickMove( DeltaTime/WorldInfo.TimeDilation );

			// Update rotation.
			OldRotation = Rotation;
			UpdateRotation( DeltaTime );
			bDoubleJump = false;

			if( bPressedJump && Pawn.CannotJumpNow() )
			{
				bSaveJump = true;
				bPressedJump = false;
			}
			else
			{
				bSaveJump = false;
			}

			if( Role < ROLE_Authority ) // then save this move and replicate it
			{
				ReplicateMove(DeltaTime, NewAccel, DoubleClickMove, OldRotation - Rotation);
			}
			else
			{
				ProcessMove(DeltaTime, NewAccel, DoubleClickMove, OldRotation - Rotation);
			}
			bPressedJump = bSaveJump;
		}
	}

	event BeginState(Name PreviousStateName)
	{
		DoubleClickDir = DCLICK_None;
		bPressedJump = false;
		GroundPitch = 0;
		if ( Pawn != None )
		{
			Pawn.ShouldCrouch(false);
			if (Pawn.Physics != PHYS_Falling && Pawn.Physics != PHYS_RigidBody) // FIXME HACK!!!
				Pawn.SetPhysics(Pawn.WalkingPhysics);
		}
	}

	event EndState(Name NextStateName)
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

Begin:
}

// player is climbing ladder
state PlayerClimbing
{
ignores SeePlayer, HearNoise, Bump;

	event NotifyPhysicsVolumeChange( PhysicsVolume NewVolume )
	{
		if( NewVolume.bWaterVolume )
		{
			GotoState( Pawn.WaterMovementState );
		}
		else
		{
			GotoState( Pawn.LandMovementState );
		}
	}

	function ProcessMove(float DeltaTime, vector NewAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
	{
		if( Pawn == None )
		{
			return;
		}

		if (Role == ROLE_Authority)
		{
			// Update ViewPitch for remote clients
			Pawn.SetRemoteViewPitch( Rotation.Pitch );
		}

		Pawn.Acceleration	= NewAccel;

		if( bPressedJump )
		{
			Pawn.DoJump( bUpdating );
			if( Pawn.Physics == PHYS_Falling )
			{
				GotoState(Pawn.LandMovementState);
			}
		}
	}

	function PlayerMove( float DeltaTime )
	{
		local vector X,Y,Z, NewAccel;
		local rotator OldRotation, ViewRotation;

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
		NewAccel = Pawn.AccelRate * Normal(NewAccel);

		ViewRotation = Rotation;

		// Update rotation.
		SetRotation(ViewRotation);
		OldRotation = Rotation;
		UpdateRotation( DeltaTime );

		if ( Role < ROLE_Authority ) // then save this move and replicate it
			ReplicateMove(DeltaTime, NewAccel, DCLICK_None, OldRotation - Rotation);
		else
			ProcessMove(DeltaTime, NewAccel, DCLICK_None, OldRotation - Rotation);
		bPressedJump = false;
	}

	event BeginState(Name PreviousStateName)
	{
		Pawn.ShouldCrouch(false);
		bPressedJump = false;
	}

	event EndState(Name NextStateName)
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

	function ProcessMove(float DeltaTime, vector NewAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot);

	// Set the throttle, steering etc. for the vehicle based on the input provided
	function ProcessDrive(float InForward, float InStrafe, float InUp, bool InJump)
	{
		local Vehicle CurrentVehicle;

		CurrentVehicle = Vehicle(Pawn);
		if (CurrentVehicle != None)
		{
			//`log("Forward:"@InForward@" Strafe:"@InStrafe@" Up:"@InUp);
			bPressedJump = InJump;
			CurrentVehicle.SetInputs(InForward, -InStrafe, InUp);
			CheckJumpOrDuck();
		}
	}

	function PlayerMove( float DeltaTime )
	{
		// update 'looking' rotation
		UpdateRotation(DeltaTime);

		// TODO: Don't send things like aForward and aStrafe for gunners who don't need it
		// Only servers can actually do the driving logic.
		ProcessDrive(PlayerInput.RawJoyUp, PlayerInput.RawJoyRight, PlayerInput.aUp, bPressedJump);
		if (Role < ROLE_Authority)
		{
			ServerDrive(PlayerInput.RawJoyUp, PlayerInput.RawJoyRight, PlayerInput.aUp, bPressedJump, ((Rotation.Yaw & 65535) << 16) + (Rotation.Pitch & 65535));
		}

		bPressedJump = false;
	}

	unreliable server function ServerUse()
	{
		local Vehicle CurrentVehicle;

		CurrentVehicle = Vehicle(Pawn);
		CurrentVehicle.DriverLeave(false);
	}

	event BeginState(Name PreviousStateName)
	{
		CleanOutSavedMoves();
	}

	event EndState(Name NextStateName)
	{
		CleanOutSavedMoves();
	}
}

// Player movement.
// Player Swimming
state PlayerSwimming
{
ignores SeePlayer, HearNoise, Bump;

	event bool NotifyLanded(vector HitNormal, Actor FloorActor)
	{
		if ( Pawn.PhysicsVolume.bWaterVolume )
			Pawn.SetPhysics(PHYS_Swimming);
		else
			GotoState(Pawn.LandMovementState);
		return bUpdating;
	}

	event NotifyPhysicsVolumeChange( PhysicsVolume NewVolume )
	{
		local actor HitActor;
		local vector HitLocation, HitNormal, Checkpoint;
		local vector X,Y,Z;

		if ( !Pawn.bCollideActors )
		{
			GotoState(Pawn.LandMovementState);
		}
		if (Pawn.Physics != PHYS_RigidBody)
		{
			if ( !NewVolume.bWaterVolume )
			{
				Pawn.SetPhysics(PHYS_Falling);
				if ( Pawn.Velocity.Z > 0 )
				{
					GetAxes(Rotation,X,Y,Z);
	 				Pawn.bUpAndOut = ((X Dot Pawn.Acceleration) > 0) && ((Pawn.Acceleration.Z > 0) || (Rotation.Pitch > 2048));
				    if (Pawn.bUpAndOut && Pawn.CheckWaterJump(HitNormal)) //check for waterjump
				    {
					    Pawn.velocity.Z = Pawn.OutOfWaterZ; //set here so physics uses this for remainder of tick
					    GotoState(Pawn.LandMovementState);
				    }
				    else if ( (Pawn.Velocity.Z > 160) || !Pawn.TouchingWaterVolume() )
					    GotoState(Pawn.LandMovementState);
				    else //check if in deep water
				    {
					    Checkpoint = Pawn.Location;
					    Checkpoint.Z -= (Pawn.CylinderComponent.CollisionHeight + 6.0);
					    HitActor = Trace(HitLocation, HitNormal, Checkpoint, Pawn.Location, false);
					    if (HitActor != None)
						    GotoState(Pawn.LandMovementState);
					    else
					    {
						    SetTimer(0.7, false);
					    }
				    }
			    }
			}
			else
			{
				ClearTimer();
				Pawn.SetPhysics(PHYS_Swimming);
			}
		}
		else if (!NewVolume.bWaterVolume)
		{
			// if in rigid body, go to appropriate state, but don't modify pawn physics
			GotoState(Pawn.LandMovementState);
		}
	}

	function ProcessMove(float DeltaTime, vector NewAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
	{
		Pawn.Acceleration = NewAccel;
	}

	function PlayerMove(float DeltaTime)
	{
		local rotator oldRotation;
		local vector X,Y,Z, NewAccel;

		if (Pawn == None)
		{
			GotoState('Dead');
		}
		else
		{
			GetAxes(Rotation,X,Y,Z);

			NewAccel = PlayerInput.aForward*X + PlayerInput.aStrafe*Y + PlayerInput.aUp*vect(0,0,1);
			NewAccel = Pawn.AccelRate * Normal(NewAccel);

			// Update rotation.
			oldRotation = Rotation;
			UpdateRotation( DeltaTime );

			if ( Role < ROLE_Authority ) // then save this move and replicate it
			{
				ReplicateMove(DeltaTime, NewAccel, DCLICK_None, OldRotation - Rotation);
			}
			else
			{
				ProcessMove(DeltaTime, NewAccel, DCLICK_None, OldRotation - Rotation);
			}
			bPressedJump = false;
		}
	}

	event Timer()
	{
		if (!Pawn.PhysicsVolume.bWaterVolume && Role == ROLE_Authority)
		{
			GotoState(Pawn.LandMovementState);
		}

		ClearTimer();
	}

	event BeginState(Name PreviousStateName)
	{
		ClearTimer();
		if (Pawn.Physics != PHYS_RigidBody)
		{
			Pawn.SetPhysics(PHYS_Swimming);
		}
	}

Begin:
}

state PlayerFlying
{
ignores SeePlayer, HearNoise, Bump;

	function PlayerMove(float DeltaTime)
	{
		local vector X,Y,Z;

		GetAxes(Rotation,X,Y,Z);

		Pawn.Acceleration = PlayerInput.aForward*X + PlayerInput.aStrafe*Y + PlayerInput.aUp*vect(0,0,1);;
		Pawn.Acceleration = Pawn.AccelRate * Normal(Pawn.Acceleration);

		if ( bCheatFlying && (Pawn.Acceleration == vect(0,0,0)) )
			Pawn.Velocity = vect(0,0,0);
		// Update rotation.
		UpdateRotation( DeltaTime );

		if ( Role < ROLE_Authority ) // then save this move and replicate it
			ReplicateMove(DeltaTime, Pawn.Acceleration, DCLICK_None, rot(0,0,0));
		else
			ProcessMove(DeltaTime, Pawn.Acceleration, DCLICK_None, rot(0,0,0));
	}

	event BeginState(Name PreviousStateName)
	{
		Pawn.SetPhysics(PHYS_Flying);
	}
}

function bool IsSpectating()
{
	return false;
}

/** when spectating, tells server where the client is (client is authoritative on location when spectating) */
unreliable server function ServerSetSpectatorLocation(vector NewLoc)
{
	// if we receive this here, the client is in the wrong state; tell it what state it should be in
	if (WorldInfo.TimeSeconds != LastSpectatorStateSynchTime)
	{
		ClientGotoState(GetStateName());
		ClientSetViewTarget(GetViewTarget());
		LastSpectatorStateSynchTime = WorldInfo.TimeSeconds;
	}
}

state BaseSpectating
{
	function bool IsSpectating()
	{
		return true;
	}

	/**
	  * Adjust spectator velocity if "out of bounds"
	  * (above stallz or below killz)
	  */
	function bool LimitSpectatorVelocity()
	{
		if ( Location.Z > WorldInfo.StallZ )
		{
			Velocity.Z = FMin(SpectatorCameraSpeed, WorldInfo.StallZ - Location.Z - 2.0);
			return true;
		}
		else if ( Location.Z < WorldInfo.KillZ )
		{
			Velocity.Z = FMin(SpectatorCameraSpeed, WorldInfo.KillZ - Location.Z + 2.0);
			return true;
		}
		return false;
	}

	function ProcessMove(float DeltaTime, vector NewAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
	{
		local float		VelSize;

		/* smoothly accelerate and decelerate */
		Acceleration = Normal(NewAccel) * SpectatorCameraSpeed;

		VelSize = VSize(Velocity);
		if( VelSize > 0 )
		{
			Velocity = Velocity - (Velocity - Normal(Acceleration) * VelSize) * FMin(DeltaTime * 8, 1);
		}

		Velocity = Velocity + Acceleration * DeltaTime;
		if( VSize(Velocity) > SpectatorCameraSpeed )
		{
			Velocity = Normal(Velocity) * SpectatorCameraSpeed;
		}

		LimitSpectatorVelocity();
		if( VSize(Velocity) > 0 )
		{
			MoveSmooth( (1+bRun) * Velocity * DeltaTime );
			// correct if out of bounds after move
			if ( LimitSpectatorVelocity() )
			{
				MoveSmooth( Velocity.Z * vect(0,0,1) * DeltaTime );
			}
		}
	}

	function PlayerMove(float DeltaTime)
	{
		local vector X,Y,Z;

		GetAxes(Rotation,X,Y,Z);
		Acceleration = PlayerInput.aForward*X + PlayerInput.aStrafe*Y + PlayerInput.aUp*vect(0,0,1);
		UpdateRotation(DeltaTime);

		if (Role < ROLE_Authority) // then save this move and replicate it
		{
			ReplicateMove(DeltaTime, Acceleration, DCLICK_None, rot(0,0,0));
		}
		else
		{
			ProcessMove(DeltaTime, Acceleration, DCLICK_None, rot(0,0,0));
		}
	}

	/** when spectating, tells server where the client is (client is authoritative on location when spectating) */
	unreliable server function ServerSetSpectatorLocation(vector NewLoc)
	{
		SetLocation(NewLoc);
		if ( WorldInfo.TimeSeconds - LastSpectatorStateSynchTime > 2.0 )
		{
			ClientGotoState(GetStateName());
			LastSpectatorStateSynchTime = WorldInfo.TimeSeconds;
		}
	}

	function ReplicateMove(float DeltaTime, vector NewAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
	{
		ProcessMove(DeltaTime, NewAccel, DoubleClickMove, DeltaRot);
		// when spectating, client position is authoritative
		ServerSetSpectatorLocation(Location);

		if (PlayerCamera != None && PlayerCamera.bUseClientSideCameraUpdates)
		{
			PlayerCamera.bShouldSendClientSideCameraUpdate = TRUE;
		}
	}


	event BeginState(Name PreviousStateName)
	{
		bCollideWorld = true;
	}

	event EndState(Name NextStateName)
	{
		bCollideWorld = false;
	}
}

unreliable server function ServerViewNextPlayer()
{
	if (IsSpectating())
	{
		ViewAPlayer(+1);
	}
}

unreliable server function ServerViewPrevPlayer()
{
	if (IsSpectating())
	{
		ViewAPlayer(-1);
	}
}

/**
 * Get next active viewable player in PRIArray.
 * @param dir is the direction to go in the array
 */
function PlayerReplicationInfo GetNextViewablePlayer(int dir)
{
	local int i, CurrentIndex, NewIndex;
	local PlayerReplicationInfo PRI;

	CurrentIndex = -1;
	if ( RealViewTarget != None )
	{
		// Find index of current viewtarget's PRI
		For ( i=0; i<WorldInfo.GRI.PRIArray.Length; i++ )
		{
			if ( RealViewTarget == WorldInfo.GRI.PRIArray[i] )
			{
				CurrentIndex = i;
				break;
			}
		}
	}

	// Find next valid viewtarget in appropriate direction
	for ( NewIndex=CurrentIndex+dir; (NewIndex>=0)&&(NewIndex<WorldInfo.GRI.PRIArray.Length); NewIndex=NewIndex+dir )
	{
		PRI = WorldInfo.GRI.PRIArray[NewIndex];
		if ( (PRI != None) && (Controller(PRI.Owner) != None) && (Controller(PRI.Owner).Pawn != None)
			&& WorldInfo.Game.CanSpectate(self, PRI) )
		{
			return PRI;
		}
	}

	// wrap around
	CurrentIndex = (NewIndex < 0) ? WorldInfo.GRI.PRIArray.Length : -1;
	for ( NewIndex=CurrentIndex+dir; (NewIndex>=0)&&(NewIndex<WorldInfo.GRI.PRIArray.Length); NewIndex=NewIndex+dir )
	{
		PRI = WorldInfo.GRI.PRIArray[NewIndex];
		if ( (PRI != None) && (Controller(PRI.Owner) != None) && (Controller(PRI.Owner).Pawn != None) &&
			WorldInfo.Game.CanSpectate(self, PRI) )
		{
			return PRI;
		}
	}

	return None;
}

/**
 * View next active player in PRIArray.
 * @param dir is the direction to go in the array
 */
function ViewAPlayer(int dir)
{
	local PlayerReplicationInfo PRI;

	PRI = GetNextViewablePlayer(dir);

	if ( PRI != None )
	{
		SetViewTarget(PRI);
	}
}

unreliable server function ServerViewSelf(optional ViewTargetTransitionParams TransitionParams)
{
	if (IsSpectating())
	{
		ResetCameraMode();
		SetViewTarget( Self, TransitionParams );
		ClientSetViewTarget( Self, TransitionParams );
	}
}


state Spectating extends BaseSpectating
{
	ignores RestartLevel, Suicide, ThrowWeapon, NotifyPhysicsVolumeChange, NotifyHeadVolumeChange;

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

	event BeginState(Name PreviousStateName)
	{
		if ( Pawn != None )
		{
			SetLocation(Pawn.Location);
			UnPossess();
		}
		bCollideWorld = true;
	}

	event EndState(Name NextStateName)
	{
		if ( PlayerReplicationInfo != None )
		{
			if ( PlayerReplicationInfo.bOnlySpectator )
			{
				`log("WARNING - Spectator only player leaving spectating state to go to "$NextStateName);
			}
			PlayerReplicationInfo.bIsSpectator = false;
		}

		bCollideWorld = false;
	}
}

auto state PlayerWaiting extends BaseSpectating
{
ignores SeePlayer, HearNoise, NotifyBump, TakeDamage, PhysicsVolumeChange, NextWeapon, PrevWeapon, SwitchToBestWeapon;

	exec function Jump();
	exec function Suicide();

	reliable server function ServerSuicide();

	reliable server function ServerChangeTeam( int N )
	{
		WorldInfo.Game.ChangeTeam(self, N, true);
	}

	reliable server function ServerRestartPlayer()
	{

		if ( WorldInfo.TimeSeconds < WaitDelay )
			return;
		if ( WorldInfo.NetMode == NM_Client )
			return;
		if ( WorldInfo.Game.bWaitingToStartMatch )
			PlayerReplicationInfo.bReadyToPlay = true;
		else
			WorldInfo.Game.RestartPlayer(self);
	}

	exec function StartFire( optional byte FireModeNum )
	{
		ServerReStartPlayer();
	}

	event EndState(Name NextStateName)
	{
		if ( PlayerReplicationInfo != None )
		{
			PlayerReplicationInfo.SetWaitingPlayer(false);
		}
		bCollideWorld = false;
	}

	// @note: this must be simulated to execute on the client because at the time the initial state is entered, RemoteRole has not been
	// set yet and so only simulated functions will be executed
	simulated event BeginState(Name PreviousStateName)
	{
		if ( PlayerReplicationInfo != None )
		{
			PlayerReplicationInfo.SetWaitingPlayer(true);
		}
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

	reliable client function ClientGotoState(name NewState, optional name NewLabel)
	{
		if (NewState == 'RoundEnded')
		{
			Global.ClientGotoState(NewState, NewLabel);
		}
	}

	unreliable client function LongClientAdjustPosition
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
		if ( newState == 'RoundEnded' )
			GotoState(newState);
	}

	event PlayerTick(float DeltaTime)
	{
		Global.PlayerTick(DeltaTime);

		if ( Pawn != None )
		{
			Pawn.Controller = self;
			Pawn.BecomeViewTarget(self);
			ClientRestart(Pawn);
		}
		else if ( !IsTimerActive() || GetTimerCount() > 1.f )
		{
			SetTimer(0.2,true);
			AskForPawn();
		}
	}

	function ReplicateMove(float DeltaTime, vector NewAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
	{
		ProcessMove(DeltaTime, NewAccel, DoubleClickMove, DeltaRot);
		// do not actually call ServerSetSpectatorLocation() as the server is not in this state and so won't accept it anyway
		// something is wrong if we're in this state for an extended period of time, so the fact that
		// the server side spectator location is not being updated should not be relevant
	}

	event Timer()
	{
		AskForPawn();
	}

	event BeginState(Name PreviousStateName)
	{
		SetTimer(0.2, true);
		AskForPawn();
	}

	event EndState(Name NextStateName)
	{
		ResetCameraMode();
		SetTimer(0.0, false);
	}
}

state RoundEnded
{
ignores SeePlayer, HearNoise, KilledBy, NotifyBump, HitWall, NotifyHeadVolumeChange, NotifyPhysicsVolumeChange, Falling, TakeDamage, Suicide;

	reliable server function ServerReStartPlayer()
	{
	}

	function bool IsSpectating()
	{
		return true;
	}

	exec function ThrowWeapon() {}
	exec function Use() {}

	event Possess(Pawn aPawn, bool bVehicleTransition)
	{
		Global.Possess(aPawn, bVehicleTransition);

		if (Pawn != None)
			Pawn.TurnOff();
	}

	reliable server function ServerReStartGame()
	{
		if (WorldInfo.Game.PlayerCanRestartGame(self))
		{
			WorldInfo.Game.ResetLevel();
		}
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
		ViewRotation = Rotation;
		// Calculate Delta to be applied on ViewRotation
		DeltaRot.Yaw	= PlayerInput.aTurn;
		DeltaRot.Pitch	= PlayerInput.aLookUp;
		ProcessViewRotation( DeltaTime, ViewRotation, DeltaRot );
		SetRotation(ViewRotation);

		ViewShake(DeltaTime);

		if ( Role < ROLE_Authority ) // then save this move and replicate it
			ReplicateMove(DeltaTime, vect(0,0,0), DCLICK_None, rot(0,0,0));
		else
			ProcessMove(DeltaTime, vect(0,0,0), DCLICK_None, rot(0,0,0));
		bPressedJump = false;
	}

	unreliable server function ServerMove
	(
		float TimeStamp,
		vector InAccel,
		vector ClientLoc,
		byte NewFlags,
		byte ClientRoll,
		int View
	)
	{
	Global.ServerMove(	TimeStamp,
							InAccel,
							ClientLoc,
							NewFlags,
							ClientRoll,
							//epic superville: Cleaner compression with no roundoff error
							((Rotation.Yaw & 65535) << 16) + (Rotation.Pitch & 65535)
						);

	}

	function FindGoodView()
	{
		local rotator GoodRotation;

		GoodRotation = Rotation;
		GetViewTarget().FindGoodEndView(self, GoodRotation);
		SetRotation(GoodRotation);
	}

	event Timer()
	{
		bFrozen = false;
	}

	unreliable client function LongClientAdjustPosition
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

	event BeginState(Name PreviousStateName)
	{
		local Pawn P;

		FOVAngle = DesiredFOV;
		bFire = 0;

		if( Pawn != None )
		{
			Pawn.TurnOff();
			StopFiring();
		}

		if( myHUD != None )
		{
			myHUD.SetShowScores(TRUE);
		}

		bFrozen = TRUE;
		FindGoodView();
		SetTimer(5, FALSE);

		ForEach DynamicActors(class'Pawn', P)
		{
			P.TurnOff();
		}
	}

	event EndState(name NextStateName)
	{
		if (myHUD != None)
		{
			myHUD.SetShowScores(false);
		}
	}

Begin:
}

state Dead
{
	ignores SeePlayer, HearNoise, KilledBy, NextWeapon, PrevWeapon;

	simulated event ReplicatedEvent(name VarName)
	{
		// if we got a Pawn, get into the correct control state
		// probably should be in global ReplicatedEvent() but minimizing risk here
		if (VarName == nameof(Pawn) && Pawn != None && Pawn != AcknowledgedPawn)
		{
			ClientRestart(Pawn);
		}
		Global.ReplicatedEvent(VarName);
	}

	exec function ThrowWeapon()
	{
		//clientmessage("Throwweapon while dead, pawn "$Pawn$" health "$Pawn.health);
	}

	function bool IsDead()
	{
		return true;
	}

	reliable server function ServerReStartPlayer()
	{
		if ( !WorldInfo.Game.PlayerCanRestart( Self ) )
			return;

		super.ServerRestartPlayer();
	}

	exec function StartFire( optional byte FireModeNum )
	{
		if ( bFrozen )
		{
			if ( !IsTimerActive() || GetTimerCount() > MinRespawnDelay )
				bFrozen = false;
			return;
		}

		ServerReStartPlayer();
	}

	exec function Use()
	{
		StartFire(0);
	}

	exec function Jump()
	{
		StartFire(0);
	}

	unreliable server function ServerMove
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
		else if ( !IsTimerActive() || GetTimerCount() > MinRespawnDelay )
		{
			bFrozen = false;
		}

		ViewShake(DeltaTime);
	}

	function FindGoodView()
	{
		local vector cameraLoc;
		local rotator cameraRot, ViewRotation;
		local int tries, besttry;
		local float bestdist, newdist;
		local int startYaw;
		local Actor TheViewTarget;

		ViewRotation = Rotation;
		ViewRotation.Pitch = 56000;
		tries = 0;
		besttry = 0;
		bestdist = 0.0;
		startYaw = ViewRotation.Yaw;
		TheViewTarget = GetViewTarget();

		for (tries=0; tries<16; tries++)
		{
			cameraLoc = TheViewTarget.Location;
			SetRotation(ViewRotation);
			GetPlayerViewPoint( cameraLoc, cameraRot );
			newdist = VSize(cameraLoc - TheViewTarget.Location);
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

	event Timer()
	{
		if (!bFrozen)
			return;

		bFrozen = false;
		bPressedJump = false;
	}

	event BeginState(Name PreviousStateName)
	{
		if ( (Pawn != None) && (Pawn.Controller == self) )
			Pawn.Controller = None;
		Pawn = None;
		FOVAngle = DesiredFOV;
		Enemy = None;
		bFrozen = true;
		bPressedJump = false;
		FindGoodView();
	    SetTimer(MinRespawnDelay, false);
		CleanOutSavedMoves();
	}

	event EndState(Name NextStateName)
	{
		CleanOutSavedMoves();
		Velocity = vect(0,0,0);
		Acceleration = vect(0,0,0);
	    if ( !PlayerReplicationInfo.bOutOfLives )
			ResetCameraMode();
		bPressedJump = false;
	    if ( myHUD != None )
		{
			myHUD.SetShowScores(false);
		}
	}

Begin:
	if ( LocalPlayer(Player) != None )
	{
		if (myHUD != None)
		{
			myHUD.PlayerOwnerDied();
		}
	}
}

function bool CanRestartPlayer()
{
	return PlayerReplicationInfo != None && !PlayerReplicationInfo.bOnlySpectator && HasClientLoadedCurrentWorld() && PendingSwapConnection == None;
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

	if ( PlayerInput != None )
	{
		PlayerInput.DrawHUD( H );
	}
}

/**
 * Get a chance to adjust the viewport size before the HUD gets rendered.
 * e.g. This allows you to render HUD elements outside the cinematic black bars
 *
 * X,Y,SizeX,SizeY - these are already set to the viewport size. Adjust as desired.
 */
event AdjustHUDRenderSize(out int X, out int Y, out int SizeX, out int SizeY, const int FullScreenSizeX, const int FullScreenSizeY)
{
	local LocalPlayer LP;

	if ( MyHUD.bRenderFullScreen )
	{
		// Use the full render target (ignores splitscreen quadrants and black bars in cinematic mode)
		X = 0;
		Y = 0;
		SizeX = FullScreenSizeX;
		SizeY = FullScreenSizeY;
	}
	else if ( !MyHUD.bScaleCanvasForCinematicMode )
	{
		// Set the canvas size back to the full extents defined by the splitscreen mode, ignoring the scaling due to the cinematic black bars
		LP = LocalPlayer(Player);
		if ( LP != None && LP.ViewportClient != None )
		{
			X = LP.Origin.X * FullScreenSizeX;
			Y = LP.Origin.Y * FullScreenSizeY;
			SizeX = LP.Size.X * FullScreenSizeX;
			SizeY = LP.Size.Y * FullScreenSizeY;
		}
	}
}

/* epic ===============================================
 * ::OnToggleInput
 *
 * Looks at the activated input from the SeqAct_ToggleInput
 * op and sets bPlayerInputEnabled accordingly.
 *
 * =====================================================
 */

function OnToggleInput(SeqAct_ToggleInput inAction)
{
	local bool bNewValue;

	if (Role < ROLE_Authority)
	{
		`Warn("Not supported on client");
		return;
	}

	if( inAction.InputLinks[0].bHasImpulse )
	{
		if( inAction.bToggleMovement )
		{
			IgnoreMoveInput( FALSE );
			ClientIgnoreMoveInput(false);
		}
		if( inAction.bToggleTurning )
		{
			IgnoreLookInput( FALSE );
			ClientIgnoreLookInput(false);
		}
	}
	else
	if( inAction.InputLinks[1].bHasImpulse )
	{
		if( inAction.bToggleMovement )
		{
			IgnoreMoveInput( TRUE );
			ClientIgnoreMoveInput(true);
		}
		if( inAction.bToggleTurning )
		{
			IgnoreLookInput( TRUE );
			ClientIgnoreLookInput(true);
		}
	}
	else
	if( inAction.InputLinks[2].bHasImpulse )
	{
		if( inAction.bToggleMovement )
		{
			bNewValue = !IsMoveInputIgnored();
			IgnoreMoveInput(bNewValue);
			ClientIgnoreMoveInput(bNewValue);
		}
		if( inAction.bToggleTurning )
		{
			bNewValue = !IsLookInputIgnored();
			IgnoreLookInput(bNewValue);
			ClientIgnoreLookInput(bNewValue);
		}
	}
}

/** calls IgnoreMoveInput on client */
client reliable function ClientIgnoreMoveInput(bool bIgnore)
{
	IgnoreMoveInput(bIgnore);
}
/** calls IgnoreLookInput on client */
client reliable function ClientIgnoreLookInput(bool bIgnore)
{
	IgnoreLookInput(bIgnore);
}

/**
 * list important PlayerController variables on canvas.  HUD will call DisplayDebug() on the current ViewTarget when
 * the ShowDebug exec is used
 *
 * @param	HUD		- HUD with canvas to draw on
 * @input	out_YL		- Height of the current font
 * @input	out_YPos	- Y position on Canvas. out_YPos += out_YL, gives position to draw text for next debug line.
 */
simulated function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	super.DisplayDebug(HUD, out_YL, out_YPos);

	if (HUD.ShouldDisplayDebug('camera'))
	{
		if( PlayerCamera != None )
		{
			PlayerCamera.DisplayDebug( HUD, out_YL, out_YPos );
		}
		else
		{
			HUD.Canvas.SetDrawColor(255,0,0);
			HUD.Canvas.DrawText("NO CAMERA");
			out_YPos += out_YL;
			HUD.Canvas.SetPos(4, out_YPos);
		}
	}
	if ( HUD.ShouldDisplayDebug('input') )
	{
		HUD.Canvas.SetDrawColor(255,0,0);
		HUD.Canvas.DrawText("Input ignoremove "$bIgnoreMoveInput$" ignore look "$bIgnoreLookInput$" aForward "$PlayerInput.aForward);
		out_YPos += out_YL;
		HUD.Canvas.SetPos(4, out_YPos);
	}
}

/* epic ===============================================
* ::OnDrawText
*
* Displays text according to parameters set in inAction
*
* =====================================================
*/
function OnDrawText(SeqAct_DrawText inAction)
{
	if( inAction.InputLinks[0].bHasImpulse )
	{
		ClientDrawKismetText(inAction.DrawTextInfo, inAction.DisplayTimeSeconds);
	}
	else
	{
		ClientClearKismetText(inAction.DrawTextInfo.MessageOffset);
	}
}


/** Start drawing a kismet message on the HUD */
reliable client final function ClientDrawKismetText(KismetDrawTextInfo DrawTextInfo, float DisplayTime)
{
    if ( !bShowKismetDrawText )
    {
	return;
    }

	if ( DisplayTime > 0 )
	{
		DrawTextInfo.MessageEndTime = WorldInfo.TimeSeconds + DisplayTime;
	}
	else
	{
		DrawTextInfo.MessageEndTime = -1;
	}

	myHUD.KismetTextInfo.AddItem(DrawTextInfo);
}

/** Stop drawing a kismet message on the HUD */
reliable client final function ClientClearKismetText(Vector2D MessageOffset)
{
	local int RemoveIdx;

	RemoveIdx = myHUD.KismetTextInfo.Find('MessageOffset', MessageOffset);

	if( RemoveIdx != INDEX_NONE )
	{
		myHUD.KismetTextInfo.Remove(RemoveIdx, 1);
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
	local Actor	RealCameraTarget;

	RealCameraTarget = inAction.CameraTarget;
	if (RealCameraTarget == None)
	{
		RealCameraTarget = (Pawn != None) ? Pawn : self;
	}
	// If we're asked to view a Controller, set its Pawn as out view target instead.
	else if (RealCameraTarget.IsA('Controller'))
	{
		RealCameraTarget = Controller(RealCameraTarget).Pawn;
	}

	SetViewTarget( RealCameraTarget, inAction.TransitionParams );
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
unreliable server function ServerCauseEvent(Name EventName)
{
	local array<SequenceObject> AllConsoleEvents;
	local SeqEvent_Console ConsoleEvt;
	local Sequence GameSeq;
	local int Idx;
	local bool bFoundEvt;
	// Get the gameplay sequence.
	GameSeq = WorldInfo.GetGameSequence();
	if ( (GameSeq != None) && (EventName != '') )
	{
		// Find all SeqEvent_Console objects anywhere.
		GameSeq.FindSeqObjectsByClass(class'SeqEvent_Console', TRUE, AllConsoleEvents);

		// Iterate over them, seeing if the name is the one we typed in.
		for( Idx=0; Idx < AllConsoleEvents.Length; Idx++ )
		{
			ConsoleEvt = SeqEvent_Console(AllConsoleEvents[Idx]);
			if (ConsoleEvt != None &&
				EventName == ConsoleEvt.ConsoleEventName)
			{
				bFoundEvt = TRUE;
				// activate the vent
				ConsoleEvt.CheckActivate(self, Pawn);
			}
		}
	}
	if (!bFoundEvt)
	{
		ListConsoleEvents();
	}
}

exec function CauseEvent(optional Name EventName)
{
	ServerCauseEvent(EventName);
}

/**
 * Shortcut version for LDs who get tired of typing 'CauseEvent' all day. :-)
 */
exec function CE(optional Name EventName)
{
	ServerCauseEvent(EventName);
}

/**
 * Lists all console events to the HUD.
 */
exec function ListConsoleEvents()
{
	local array<SequenceObject> ConsoleEvents;
	local SeqEvent_Console ConsoleEvt;
	local Sequence GameSeq;
	local int Idx;
	GameSeq = WorldInfo.GetGameSequence();
	if (GameSeq != None)
	{
		`log("Console events:");
		ClientMessage("Console events:",,15.f);
		GameSeq.FindSeqObjectsByClass(class'SeqEvent_Console',TRUE,ConsoleEvents);
		for (Idx = 0; Idx < ConsoleEvents.Length; Idx++)
		{
			ConsoleEvt = SeqEvent_Console(ConsoleEvents[Idx]);
			if (ConsoleEvt != None &&
				ConsoleEvt.bEnabled)
			{
				`log("-"@ConsoleEvt.ConsoleEventName@ConsoleEvt.EventDesc);
				ClientMessage("-"@ConsoleEvt.ConsoleEventName@ConsoleEvt.EventDesc,,15.f);
			}
		}
	}
}

exec function ListCE()
{
	ListConsoleEvents();
}

/** triggers a SeqEvent_RemoteEvent instead of a console event; LDs specifically requested separate commands */
exec function RemoteEvent(optional name EventName)
{
	ServerRemoteEvent(EventName);
}
exec function RE(optional name EventName)
{
	ServerRemoteEvent(EventName);
}
unreliable server function ServerRemoteEvent(name EventName)
{
	local array<SequenceObject> AllRemoteEvents;
	local SeqEvent_RemoteEvent RemoteEvt;
	local Sequence GameSeq;
	local int Idx;
	local bool bFoundEvt;

	// Get the gameplay sequence.
	GameSeq = WorldInfo.GetGameSequence();
	if (GameSeq != None)
	{
		// Find all SeqEvent_Console objects anywhere.
		GameSeq.FindSeqObjectsByClass(class'SeqEvent_RemoteEvent', true, AllRemoteEvents);

		if (EventName != '')
		{
			// Iterate over them, seeing if the name is the one we typed in.
			for (Idx = 0; Idx < AllRemoteEvents.length; Idx++)
			{
				RemoteEvt = SeqEvent_RemoteEvent(AllRemoteEvents[Idx]);
				if (RemoteEvt != None && EventName == RemoteEvt.EventName)
				{
					bFoundEvt = true;
					// activate the vent
					RemoteEvt.CheckActivate(self, Pawn);
				}
			}
		}
	}
	if (!bFoundEvt)
	{
		`log("Remote events:");
		ClientMessage("Remote events:",, 15.0);
		for (Idx = 0; Idx < AllRemoteEvents.Length; Idx++)
		{
			RemoteEvt = SeqEvent_RemoteEvent(AllRemoteEvents[Idx]);
			if (RemoteEvt != None && RemoteEvt.bEnabled)
			{
				`log("-" @ RemoteEvt.EventName);
				ClientMessage("-" @ RemoteEvt.EventName,, 15.0);
			}
		}
	}
}

exec function ShowPlayerState()
{
	`log("Dumping state stack for" @ Self);
	DumpStateStack();
}


exec function ShowGameState()
{
	if ( WorldInfo.Game != None )
	{
		`log(`location$": Dumping state stack for" @ WorldInfo.Game);
		WorldInfo.Game.DumpStateStack();
	}
	else
	{
		`log(`location$": No GameInfo found!");
	}
}

/**
 * Notification from pawn that it has received damage
 * via TakeDamage().
 */
function NotifyTakeHit(Controller InstigatedBy, vector HitLocation, int Damage,
	class<DamageType> damageType, vector Momentum)
{
	Super.NotifyTakeHit(InstigatedBy,HitLocation,Damage,damageType,Momentum);

	// Play waveform
	ClientPlayForceFeedbackWaveform(damageType.default.DamagedFFWaveform);
}

/**
 * Kismet interface for playing/stopping force feedback.
 */
function OnForceFeedback(SeqAct_ForceFeedback Action)
{
	if (Action.InputLinks[0].bHasImpulse)
	{
		ClientPlayForceFeedbackWaveform(Action.FFWaveform);
	}
	else if (Action.InputLinks[1].bHasImpulse)
	{
		ClientStopForceFeedbackWaveform(Action.FFWaveform);
	}
}

/** This will take an AnimNotify_Rumble and then grab out the correct waveform to be played **/
event PlayRumble( const AnimNotify_Rumble TheAnimNotify )
{
	if( TheAnimNotify.PredefinedWaveForm != none )
	{
		ClientPlayForceFeedbackWaveform( TheAnimNotify.PredefinedWaveForm.default.TheWaveForm );
	}
	else
	{
		ClientPlayForceFeedbackWaveform( TheAnimNotify.WaveForm );
	}
}


/**
 * Tells the client to play a waveform for the specified damage type
 *
 * @param FFWaveform The forcefeedback waveform to play
 * @param FFWaveformInstigator the actor causing the waveform to play
 */
reliable client event ClientPlayForceFeedbackWaveform(ForceFeedbackWaveform FFWaveform,optional Actor FFWaveformInstigator)
{
	if (PlayerInput != None && !PlayerInput.bUsingGamepad && !WorldInfo.IsConsoleBuild(CONSOLE_Any))
	{
		return; // don't play forcefeedback if gamepad isn't being used
	}

	if( ForceFeedbackManager != None && PlayerReplicationInfo != None && IsForceFeedbackAllowed() )
	{
		ForceFeedbackManager.PlayForceFeedbackWaveform(FFWaveform,FFWaveformInstigator);
	}
}

/**
 * Tells the client to stop any waveform that is playing. Note if the optional
 * parameter is passed in, then the waveform is only stopped if it matches
 *
 * @param FFWaveform The forcefeedback waveform to stop
 */
reliable client final event ClientStopForceFeedbackWaveform(optional ForceFeedbackWaveform FFWaveform)
{
	if( ForceFeedbackManager != None )
	{
		ForceFeedbackManager.StopForceFeedbackWaveform(FFWaveform);
	}
}

/**
 * @return	TRUE if starting a force feedback waveform is allowed;  child classes should override this method to e.g. temporarily disable
 * 			force feedback
 */
simulated function bool IsForceFeedbackAllowed()
{
	return ForceFeedbackManager != None && ForceFeedbackManager.bAllowsForceFeedback;
}

/**
 * Handles switching the player in/out of cinematic mode.
 */
function OnToggleCinematicMode(SeqAct_ToggleCinematicMode Action)
{
	local bool bNewCinematicMode;

	if (Role < ROLE_Authority)
	{
		`Warn("Not supported on client");
		return;
	}

	if (Action.InputLinks[0].bHasImpulse)
	{
		bNewCinematicMode = TRUE;
	}
	else if (Action.InputLinks[1].bHasImpulse)
	{
		bNewCinematicMode = FALSE;
	}
	else if (Action.InputLinks[2].bHasImpulse)
	{
		bNewCinematicMode = !bCinematicMode;
	}

	SetCinematicMode(bNewCinematicMode, Action.bHidePlayer, Action.bHideHUD, Action.bDisableMovement, Action.bDisableTurning, Action.bDisableInput);
}

/**
 * Server/SP only function for changing whether the player is in cinematic mode.  Updates values of various state variables, then replicates the call to the client
 * to sync the current cinematic mode.
 *
 * @param	bInCinematicMode	specify TRUE if the player is entering cinematic mode; FALSE if the player is leaving cinematic mode.
 * @param	bHidePlayer			specify TRUE to hide the player's pawn (only relevant if bInCinematicMode is TRUE)
 * @param	bAffectsHUD			specify TRUE if we should show/hide the HUD to match the value of bCinematicMode
 * @param	bAffectsMovement	specify TRUE to disable movement in cinematic mode, enable it when leaving
 * @param	bAffectsTurning		specify TRUE to disable turning in cinematic mode or enable it when leaving
 * @param	bAffectsButtons		specify TRUE to disable button input in cinematic mode or enable it when leaving.
 */
function SetCinematicMode( bool bInCinematicMode, bool bHidePlayer, bool bAffectsHUD, bool bAffectsMovement, bool bAffectsTurning, bool bAffectsButtons )
{
	local bool bAdjustMoveInput, bAdjustLookInput;

	bCinematicMode = bInCinematicMode;

	// if now in cinematic mode
	if( bCinematicMode )
	{
		// hide the player
		if (Pawn != None && bHidePlayer)
		{
			Pawn.SetHidden(True);
		}
	}
	else
	{
		if( Pawn != None )
		{
			Pawn.SetHidden(False);
		}
	}

	bAdjustMoveInput = bAffectsMovement && (bCinematicMode != bCinemaDisableInputMove);
	bAdjustLookInput = bAffectsTurning && (bCinematicMode != bCinemaDisableInputLook);
	if ( bAdjustMoveInput )
	{
		IgnoreMoveInput(bCinematicMode);
		bCinemaDisableInputMove = bCinematicMode;
	}
	if ( bAdjustLookInput )
	{
		IgnoreLookInput(bCinematicMode);
		bCinemaDisableInputLook = bCinematicMode;
	}

	ClientSetCinematicMode(bCinematicMode, bAdjustMoveInput, bAdjustLookInput, bAffectsHUD);
}

/** called by the server to synchronize cinematic transitions with the client */
reliable client function ClientSetCinematicMode(bool bInCinematicMode, bool bAffectsMovement, bool bAffectsTurning, bool bAffectsHUD)
{
	bCinematicMode = bInCinematicMode;

	// if there's a hud, set whether it should be shown or not
	if ( (myHUD != None) && bAffectsHUD )
	{
		myHUD.bShowHUD = !bCinematicMode;
	}

	if (bAffectsMovement)
	{
		IgnoreMoveInput(bCinematicMode);
	}
	if (bAffectsTurning)
	{
		IgnoreLookInput(bCinematicMode);
	}
}

/** Toggles move input. FALSE means movement input is cleared. */
function IgnoreMoveInput( bool bNewMoveInput )
{
	bIgnoreMoveInput = Max( bIgnoreMoveInput + (bNewMoveInput ? +1 : -1), 0 );
	//`Log("IgnoreMove: " $ bIgnoreMoveInput);
}


/** return TRUE if movement input is ignored. */
event bool IsMoveInputIgnored()
{
	return (bIgnoreMoveInput > 0);
}


/** Toggles look input. FALSE means look input is cleared. */
function IgnoreLookInput( bool bNewLookInput )
{
	bIgnoreLookInput = Max( bIgnoreLookInput + (bNewLookInput ? +1 : -1), 0 );
	//`Log("IgnoreLook: " $ bIgnoreLookInput);
}


/** return TRUE if look input is ignored. */
event bool IsLookInputIgnored()
{
	return (bIgnoreLookInput > 0);
}


/** reset input to defaults */
function ResetPlayerMovementInput()
{
	bIgnoreMoveInput = default.bIgnoreMoveInput;
	bIgnoreLookInput = default.bIgnoreLookInput;
}


/** Kismet hook to trigger console events */
function OnConsoleCommand( SeqAct_ConsoleCommand inAction )
{
	local string Command;

	foreach inAction.Commands(Command)
	{
		// prevent "set" commands from ever working in Kismet as they are e.g. disabled in netplay
		if (!(Left(Command, 4) ~= "set ") && !(Left(Command, 9) ~= "setnopec "))
		{
			ConsoleCommand(Command);
		}
	}
}

/** forces GC at the end of the tick on the client */
reliable client event ClientForceGarbageCollection()
{
	WorldInfo.ForceGarbageCollection();
}

final event LevelStreamingStatusChanged(LevelStreaming LevelObject, bool bNewShouldBeLoaded, bool bNewShouldBeVisible, bool bNewShouldBlockOnLoad )
{
	//`log( "LevelStreamingStatusChanged: " @ LevelObject @ bNewShouldBeLoaded @ bNewShouldBeVisible @ bNewShouldBeVisible );
	ClientUpdateLevelStreamingStatus(LevelObject.PackageName,bNewShouldBeLoaded,bNewShouldBeVisible,bNewShouldBlockOnLoad);
}

native reliable client function ClientUpdateLevelStreamingStatus(Name PackageName, bool bNewShouldBeLoaded, bool bNewShouldBeVisible, bool bNewShouldBlockOnLoad);

/** called when the client adds/removes a streamed level
 * the server will only replicate references to Actors in visible levels so that it's impossible to send references to
 * Actors the client has not initialized
 * @param PackageName the name of the package for the level whose status changed
 */
native reliable server final event ServerUpdateLevelVisibility(name PackageName, bool bIsVisible);

/** asynchronously loads the given level in preparation for a streaming map transition.
 * the server sends one function per level name since dynamic arrays can't be replicated
 * @param LevelNames the names of the level packages to load. LevelNames[0] will be the new persistent (primary) level
 * @param bFirst whether this is the first item in the list (so clear the list first)
 * @param bLast whether this is the last item in the list (so start preparing the change after receiving it)
 */
reliable client event ClientPrepareMapChange(name LevelName, bool bFirst, bool bLast)
{
	// Only call on the first local player controller to handle it being called on multiple PCs for splitscreen.
	local PlayerController PC;

	foreach LocalPlayerControllers(class'PlayerController', PC)
	{
		if( PC != self )
		{
			return;
		}
		else
		{
			break;
		}
	}

	if (bFirst)
	{
		PendingMapChangeLevelNames.length = 0;
		ClearTimer('DelayedPrepareMapChange');
	}
	PendingMapChangeLevelNames[PendingMapChangeLevelNames.length] = LevelName;
	if (bLast)
	{
		DelayedPrepareMapChange();
	}
}

/** used to wait until a map change can be prepared when one was already in progress */
function DelayedPrepareMapChange()
{
	if (WorldInfo.IsPreparingMapChange())
	{
		// we must wait for the previous one to complete
		SetTimer( 0.01, false, nameof(DelayedPrepareMapChange) );
	}
	else
	{
		WorldInfo.PrepareMapChange(PendingMapChangeLevelNames);
	}
}

/** actually performs the level transition prepared by PrepareMapChange() */
reliable client event ClientCommitMapChange()
{
	if (IsTimerActive(nameof(DelayedPrepareMapChange)))
	{
		SetTimer(0.01, false, nameof(ClientCommitMapChange));
	}
	else
	{
		if (Pawn != None)
		{
			SetViewTarget(Pawn);
		}
		else
		{
			SetViewTarget(self);
		}
		WorldInfo.CommitMapChange();
	}
}

/** tells client to cancel any pending map change */
reliable client event ClientCancelPendingMapChange()
{
	WorldInfo.CancelPendingMapChange();
}

/** tells the client to block until all pending level streaming actions are complete
 * happens at the end of the tick
 * primarily used to force update the client ASAP at join time
 */
reliable client native final event ClientFlushLevelStreaming();

/** sets bRequestedBlockOnAsyncLoading which will later bring up a loading screen and then finish any async loading in progress
 * called automatically on all clients whenever something triggers it on the server
 */
reliable client event ClientSetBlockOnAsyncLoading()
{
	WorldInfo.bRequestedBlockOnAsyncLoading = true;
}

/** used to allow clients to process a SeqAct_WaitForLevelsVisible by blocking, if required */
reliable client function ClientWaitForLevelsVisible(SeqAct_WaitForLevelsVisible InAction)
{
	InAction.CheckLevelsVisible();
}

/**
 * Force a save config on the specified class.
 */
exec function SaveClassConfig(coerce string className)
{
	local class<Object> saveClass;

	`log("SaveClassConfig:"@className);
	saveClass = class<Object>(DynamicLoadObject(className,class'class'));
	if (saveClass != None)
	{
		`log("- Saving config on:"@saveClass);
		saveClass.static.StaticSaveConfig();
	}
`if(`notdefined(FINAL_RELEASE))
	else
	{
		`log("- Failed to find class:"@className);
	}
`endif
}

/**
 * Force a save config on the specified actor.
 */
exec function SaveActorConfig(coerce Name actorName)
{
	local Actor chkActor;
	`log("SaveActorConfig:"@actorName);
	foreach AllActors(class'Actor',chkActor)
	{
		if (chkActor != None &&
			chkActor.Name == actorName)
		{
			`log("- Saving config on:"@chkActor);
			chkActor.SaveConfig();
		}
	}
}

/**
 * Returns the interaction that manages the UI system.
 */
final function UIInteraction GetUIController()
{
	local LocalPlayer LP;
	local UIInteraction Result;

	LP = LocalPlayer(Player);
	if ( LP != None && LP.ViewportClient != None )
	{
		Result = LP.ViewportClient.UIController;
	}

	return Result;
}

/**
 * Native function to determine if voice data should be received from this player.
 * Only called on the server to determine whether voice packet replication
 * should happen for the given sender.
 *
 * NOTE: This function is final because it can be called n^2 number of times
 * in a given frame, where n is the number of players. Change/overload this
 * function with caution as this can affect your network performance.
 *
 * @param Sender the player to check for mute status
 *
 * @return TRUE if this player is muted, FALSE otherwise
 */
native final function bool IsPlayerMuted(const out UniqueNetId Sender);


/** called on client during seamless level transitions to get the list of Actors that should be moved into the new level
 * PlayerControllers, Role < ROLE_Authority Actors, and any non-Actors that are inside an Actor that is in the list
 * (i.e. Object.Outer == Actor in the list)
 * are all autmoatically moved regardless of whether they're included here
 * only dynamic (!bStatic and !bNoDelete) actors in the PersistentLevel may be moved (this includes all actors spawned during gameplay)
 * this is called for both parts of the transition because actors might change while in the middle (e.g. players might join or leave the game)
 * @see also GameInfo::GetSeamlessTravelActorList() (the function that's called on servers)
 * @param bToEntry true if we are going from old level -> entry, false if we are going from entry -> new level
 * @param ActorList (out) list of actors to maintain
 */
event GetSeamlessTravelActorList(bool bToEntry, out array<Actor> ActorList)
{
	// clear out audio pool
	HearSoundActiveComponents.length = 0;
	HearSoundPoolComponents.length = 0;

	if (myHUD != None)
	{
		ActorList[ActorList.length] = myHUD;
	}
}

/** called when seamless travelling and we are being replaced by the specified PC
 * clean up any persistent state (post process chains on LocalPlayers, for example)
 * (not called if PlayerControllerClass is the same for the from and to gametypes)
 */
function SeamlessTravelTo(PlayerController NewPC);

/** called when seamless travelling and the specified PC is being replaced by this one
 * copy over data that should persist
 * (not called if PlayerControllerClass is the same for the from and to gametypes)
 */
function SeamlessTravelFrom(PlayerController OldPC)
{
	// copy PRI data
	OldPC.PlayerReplicationInfo.Reset();
	OldPC.PlayerReplicationInfo.SeamlessTravelTo(PlayerReplicationInfo);

	// mark the old PC as not a player, so it doesn't get Logout() etc when they player represented didn't really leave
	OldPC.bIsPlayer = false;
	//@fixme: need a way to replace PRIs that doesn't cause incorrect "player left the game"/"player entered the game" messages
	OldPC.PlayerReplicationInfo.Destroy();
	OldPC.PlayerReplicationInfo = None;
}

/**
 * Looks at the current game state and uses that to set the
 * rich presence strings
 *
 * Licensees should override this in their player controller derived class
 */
reliable client function ClientSetOnlineStatus();

/**
 * Returns the player controller associated with this net id
 *
 * @param PlayerNetId the id to search for
 *
 * @return the player controller if found, otherwise none
 */
native static function PlayerController GetPlayerControllerFromNetId(UniqueNetId PlayerNetId);

/**
 * Tells the client that the server has all the information it needs and that it
 * is ok to start sending voice packets. The server will already send voice packets
 * when this function is called, since it is set server side and then forwarded
 *
 * NOTE: This is done as an RPC instead of variable replication because ordering matters
 */
reliable client function ClientVoiceHandshakeComplete()
{
	local int PeerIdx;

	bHasVoiceHandshakeCompleted = true;

	// Replicate list of connected peers for each player to the server
	for (PeerIdx=0; PeerIdx < ConnectedPeers.Length; PeerIdx++)
	{
		ServerAddPeer(ConnectedPeers[PeerIdx].PlayerId,ConnectedPeers[PeerIdx].NATType);
	}
}

/**
 * Locally mutes a remote player
 *
 * @param PlayerNetId the remote player to mute
 */
reliable client event ClientMutePlayer(UniqueNetId PlayerNetId)
{
	local LocalPlayer LocPlayer;

	// Add to the filter list on clients (used for peer to peer voice)
	if (VoicePacketFilter.Find('Uid',PlayerNetId.Uid) == INDEX_NONE)
	{
		VoicePacketFilter.AddItem(PlayerNetId);
	}

	if (VoiceInterface != None)
	{
		// Use the local player to determine the controller id
		LocPlayer = LocalPlayer(Player);
		if (LocPlayer != None)
		{
			// Have the voice subsystem mute this player
			VoiceInterface.MuteRemoteTalker(LocPlayer.ControllerId,PlayerNetId);
		}
	}
}

/**
 * Locally unmutes a remote player
 *
 * @param PlayerNetId the remote player to unmute
 */
reliable client event ClientUnmutePlayer(UniqueNetId PlayerNetId)
{
	local LocalPlayer LocPlayer;
	local int RemoveIndex;

	// It's safe to remove them from the filter list on clients (used for peer to peer voice)
	RemoveIndex = VoicePacketFilter.Find('Uid',PlayerNetId.Uid);
	if (RemoveIndex != INDEX_NONE)
	{
		VoicePacketFilter.Remove(RemoveIndex,1);
	}

	if (VoiceInterface != None)
	{
		// Use the local player to determine the controller id
		LocPlayer = LocalPlayer(Player);
		if (LocPlayer != None)
		{
			// Have the voice subsystem restore voice for this player
			VoiceInterface.UnmuteRemoteTalker(LocPlayer.ControllerId,PlayerNetId);
		}
	}
}

/**
 * Mutes a remote player on the server and then tells the client to mute
 *
 * @param PlayerNetId the remote player to mute
 */
function GameplayMutePlayer(UniqueNetId PlayerNetId)
{
	// Don't add if already muted
	if (GameplayVoiceMuteList.Find('Uid',PlayerNetId.Uid) == INDEX_NONE)
	{
		GameplayVoiceMuteList.AddItem(PlayerNetId);
	}
	// Add to the filter list too, if missing
	if (VoicePacketFilter.Find('Uid',PlayerNetId.Uid) == INDEX_NONE)
	{
		VoicePacketFilter.AddItem(PlayerNetId);
	}

	// Now process on the client needed for splitscreen net play
	ClientMutePlayer(PlayerNetId);
}

/**
 * Unmutes a remote player on the server and then tells the client to unmute
 *
 * @param PlayerNetId the remote player to unmute
 */
function GameplayUnmutePlayer(UniqueNetId PlayerNetId)
{
	local int RemoveIndex;
	local PlayerController Other;

	// Remove from the gameplay mute list
	RemoveIndex = GameplayVoiceMuteList.Find('Uid',PlayerNetId.Uid);
	if (RemoveIndex != INDEX_NONE)
	{
		GameplayVoiceMuteList.Remove(RemoveIndex,1);
	}
	// Find the muted player's player controller so it can be notified
	Other = GetPlayerControllerFromNetId(PlayerNetId);
	if (Other != None)
	{
		// If they were not explicitly muted
		if (VoiceMuteList.Find('Uid',PlayerNetId.Uid) == INDEX_NONE &&
			// and they did not explicitly mute us
			Other.VoiceMuteList.Find('Uid',PlayerReplicationInfo.UniqueId.Uid) == INDEX_NONE)
		{
			// It's safe to remove them from the filter list
			RemoveIndex = VoicePacketFilter.Find('Uid',PlayerNetId.Uid);
			if (RemoveIndex != INDEX_NONE)
			{
				VoicePacketFilter.Remove(RemoveIndex,1);
			}

			// Now process on the client
			ClientUnmutePlayer(PlayerNetId);
		}
	}
}

/**
 * Updates the server side information by adding to the mute list. Tells the
 * player controller that owns the specified net id to also mute this PC.
 *
 * @param PlayerNetId the remote player to mute
 */
reliable server event ServerMutePlayer(UniqueNetId PlayerNetId)
{
	local PlayerController Other;

	// Don't reprocess if they are already muted
	if (VoiceMuteList.Find('Uid',PlayerNetId.Uid) == INDEX_NONE)
	{
		VoiceMuteList.AddItem(PlayerNetId);
	}
	// Add them to the packet filter list if not already on it
	if (VoicePacketFilter.Find('Uid',PlayerNetId.Uid) == INDEX_NONE)
	{
		VoicePacketFilter.AddItem(PlayerNetId);
	}
	ClientMutePlayer(PlayerNetId);
	// Find the muted player's player controller so it can be notified
	Other = GetPlayerControllerFromNetId(PlayerNetId);
	if (Other != None)
	{
		// Update their packet filter list too
		if (Other.VoicePacketFilter.Find('Uid',PlayerReplicationInfo.UniqueId.Uid) == INDEX_NONE)
		{
			Other.VoicePacketFilter.AddItem(PlayerReplicationInfo.UniqueId);
		}

		// Tell the other PC to mute this one
		Other.ClientMutePlayer(PlayerReplicationInfo.UniqueId);
	}
}

/**
 * Updates the server side information by removing from the mute list. Tells the
 * player controller that owns the specified net id to also unmute this PC.
 *
 * @param PlayerNetId the remote player to unmute
 */
reliable server event ServerUnmutePlayer(UniqueNetId PlayerNetId)
{
	local PlayerController Other;
	local int RemoveIndex;

	RemoveIndex = VoiceMuteList.Find('Uid',PlayerNetId.Uid);
	// If the player was found, remove them from our explicit list
	if (RemoveIndex != INDEX_NONE)
	{
		VoiceMuteList.Remove(RemoveIndex,1);
	}
	// Find the muted player's player controller so it can be notified
	Other = GetPlayerControllerFromNetId(PlayerNetId);
	if (Other != None)
	{
		// Make sure this player isn't muted for gameplay reasons
		if (GameplayVoiceMuteList.Find('Uid',PlayerNetId.Uid) == INDEX_NONE &&
			// And make sure they didn't mute us
			Other.VoiceMuteList.Find('Uid',PlayerReplicationInfo.UniqueId.Uid) == INDEX_NONE)
		{
			ClientUnmutePlayer(PlayerNetId);
		}
		// If the other player doesn't have this player muted
		if (Other.VoiceMuteList.Find('Uid',PlayerReplicationInfo.UniqueId.Uid) == INDEX_NONE &&
			Other.GameplayVoiceMuteList.Find('Uid',PlayerReplicationInfo.UniqueId.Uid) == INDEX_NONE)
		{
			// Remove them from the packet filter list
			RemoveIndex = VoicePacketFilter.Find('Uid',PlayerNetId.Uid);
			if (RemoveIndex != INDEX_NONE)
			{
				VoicePacketFilter.Remove(RemoveIndex,1);
			}
			// If found, remove so packets flow to that client too
			RemoveIndex = Other.VoicePacketFilter.Find('Uid',PlayerReplicationInfo.UniqueId.Uid);
			if (RemoveIndex != INDEX_NONE)
			{
				Other.VoicePacketFilter.Remove(RemoveIndex,1);
			}

			// Tell the other PC to unmute this one
			Other.ClientUnmutePlayer(PlayerReplicationInfo.UniqueId);
		}
	}
}

/** notification when a matinee director track starts or stops controlling the ViewTarget of this PlayerController */
event NotifyDirectorControl(bool bNowControlling, SeqAct_Interp CurrentMatinee)
{
	// matinee is done, make sure client syncs up viewtargets, since we were ignoring
	// ClientSetViewTarget during the matinee.
	if ( !bNowControlling && (WorldInfo.NetMode == NM_Client) && bClientSimulatingViewTarget )
	{
		ServerVerifyViewTarget();
	}
}

/**
 * This will turn the subtitles on or off depending on the value of bValue
 *
 * @param bValue  to show or not to show
 **/
native simulated exec function SetShowSubtitles( bool bValue );

/**
 * This will turn return whether the subtitles are on or off
 *
 **/
native simulated function bool IsShowingSubtitles();

reliable client event ClientWasKicked();

/**
 * Tells the client to register with arbitration. The client must notify the
 * server once that is complete or it will be kicked from the match
 */
reliable client function ClientRegisterForArbitration()
{
	if (OnlineSub != None && OnlineSub.GameInterface != None)
	{
		OnlineSub.GameInterface.AddArbitrationRegistrationCompleteDelegate(OnArbitrationRegisterComplete);
		// Kick off async arbitration registration
		OnlineSub.GameInterface.RegisterForArbitration('Game');
	}
	else
	{
		// Fake completion with the server for flow testing
		ServerRegisteredForArbitration(true);
	}
}

/**
 * Delegate that is notified when registration is complete. Forwards the call
 * to the server so that it can finalize processing
 *
 * @param SessionName the name of the session this is for
 * @param bWasSuccessful whether registration worked or not
 */
function OnArbitrationRegisterComplete(name SessionName,bool bWasSuccessful)
{
	// Clear the delegate since it isn't needed
	OnlineSub.GameInterface.ClearArbitrationRegistrationCompleteDelegate(OnArbitrationRegisterComplete);
	// Tell the server that we completed registration
	ServerRegisteredForArbitration(bWasSuccessful);
}

/**
 * Notifies the server that the arbitration registration is complete
 *
 * @param bWasSuccessful whether the registration with arbitration worked
 */
reliable server function ServerRegisteredForArbitration(bool bWasSuccessful)
{
	// Tell the game info this PC is done and whether it worked or not
	WorldInfo.Game.ProcessClientRegistrationCompletion(self,bWasSuccessful);
}

/**
 * Delegate called when the user accepts a game invite externally. This allows
 * the game code a chance to clean up before joining the game via
 * AcceptGameInvite() call.
 *
 * NOTE: There must be space for all signed in players to join the game. All
 * players must also have permission to play online too.
 *
 * @param InviteResult the search/settings for the game we're to join
 */
function OnGameInviteAccepted(const out OnlineGameSearchResult InviteResult)
{
	local OnlineGameSettings GameInviteSettings;

	if (OnlineSub != None && OnlineSub.GameInterface != None)
	{
		GameInviteSettings = InviteResult.GameSettings;
		if (GameInviteSettings != None)
		{
			// Make sure the new game has space
			if (InviteHasEnoughSpace(GameInviteSettings))
			{
				// Make sure everyone logged in can play online
				if (CanAllPlayersPlayOnline())
				{
					if (WorldInfo.NetMode != NM_Standalone)
					{
						// Write arbitration data, if required
						if (OnlineSub.GameInterface.GetGameSettings('Game').bUsesArbitration)
						{
							// Write out our version of the scoring before leaving
							ClientWriteOnlinePlayerScores(WorldInfo.GRI.GameClass != None ? WorldInfo.GRI.GameClass.default.ArbitratedLeaderBoardId : 0);
						}
						// Set the end delegate, where we'll destroy the game and then join
						OnlineSub.GameInterface.AddEndOnlineGameCompleteDelegate(OnEndForInviteComplete);
						// Force the flush of the stats
						OnlineSub.GameInterface.EndOnlineGame('Game');
					}
					else
					{
						// Set the delegate for notification of the join completing
						OnlineSub.GameInterface.AddJoinOnlineGameCompleteDelegate(OnInviteJoinComplete);

						// We can immediately accept since there is no online game
						if (!OnlineSub.GameInterface.AcceptGameInvite(LocalPlayer(Player).ControllerId,'Game'))
						{
							OnlineSub.GameInterface.ClearJoinOnlineGameCompleteDelegate(OnInviteJoinComplete);
							// Do some error handling
							NotifyInviteFailed();
						}
					}
				}
				else
				{
					// Display an error message
					NotifyNotAllPlayersCanJoinInvite();
				}
			}
			else
			{
				// Display an error message
				NotifyNotEnoughSpaceInInvite();
			}
		}
		else
		{
			// Display an error message
			NotifyInviteFailed();
		}
	}
}

/**
 * Counts the number of local players to verify there is enough space
 *
 * @return true if there is sufficient space, false if not
 */
function bool InviteHasEnoughSpace(OnlineGameSettings InviteSettings)
{
	local int NumLocalPlayers;
	local PlayerController PC;

	foreach LocalPlayerControllers(class'PlayerController', PC)
	{
		NumLocalPlayers++;
	}
	// Invites consume private connections
	return (InviteSettings.NumOpenPrivateConnections + InviteSettings.NumOpenPublicConnections) >= NumLocalPlayers;
}

/**
 * Validates that each local player can play online
 *
 * @return true if there is sufficient space, false if not
 */
function bool CanAllPlayersPlayOnline()
{
	local PlayerController PC;
	local LocalPlayer LocPlayer;

	foreach LocalPlayerControllers(class'PlayerController', PC)
	{
		LocPlayer = LocalPlayer(PC.Player);
		if (LocPlayer != None)
		{
			// Check their login status and permissions
			if (OnlineSub.PlayerInterface.GetLoginStatus(LocPlayer.ControllerId) != LS_LoggedIn ||
				OnlineSub.PlayerInterface.CanPlayOnline(LocPlayer.ControllerId) == FPL_Disabled)
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}
	return true;
}

/**
 * Clears all of the invite delegates
 */
function ClearInviteDelegates()
{
	// Clear the end delegate
	OnlineSub.GameInterface.ClearEndOnlineGameCompleteDelegate(OnEndForInviteComplete);
	// Clear the destroy delegate
	OnlineSub.GameInterface.ClearDestroyOnlineGameCompleteDelegate(OnDestroyForInviteComplete);
	// Set the delegate for notification of the join completing
	OnlineSub.GameInterface.ClearJoinOnlineGameCompleteDelegate(OnInviteJoinComplete);
}

/**
 * Delegate called once the destroy of an online game before accepting an invite
 * is complete. From here, the game invite can be accepted
 *
 * @param SessionName the name of the session being ended
 * @param bWasSuccessful whether the end completed ok or not
 */
function OnEndForInviteComplete(name SessionName,bool bWasSuccessful)
{
	// Set the destroy delegate so we can know when that is complete
	OnlineSub.GameInterface.AddDestroyOnlineGameCompleteDelegate(OnDestroyForInviteComplete);
	// Now we can destroy the game (completion delegate guaranteed to be called)
	OnlineSub.GameInterface.DestroyOnlineGame(SessionName);
}

/**
 * Delegate called once the destroy of an online game before accepting an invite
 * is complete. From here, the game invite can be accepted
 *
 * @param SessionName the name of the session being ended
 * @param bWasSuccessful whether the end completed ok or not
 */
function OnDestroyForInviteComplete(name SessionName,bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		// Set the delegate for notification of the join completing
		OnlineSub.GameInterface.AddJoinOnlineGameCompleteDelegate(OnInviteJoinComplete);
		// This will have us join async
		if (!OnlineSub.GameInterface.AcceptGameInvite(LocalPlayer(Player).ControllerId,SessionName))
		{
			OnlineSub.GameInterface.ClearJoinOnlineGameCompleteDelegate(OnInviteJoinComplete);
			// Do some error handling
			NotifyInviteFailed();
		}
	}
	else
	{
		// Do some error handling
		NotifyInviteFailed();
	}
}

/**
 * Allows the game to modify the URL that clients will use to connect to a server (should be called
 * by pretty much anyone who calls GetResolvedConnectString())
 */
function string ModifyClientURL(string URL)
{
	return URL;
}

/**
 * Once the join completes, use the platform specific connection information
 * to connect to it
 *
 * @param SessionName the name of the session that was joined
 * @param bWasSuccessful whether the join worked or not
 */
function OnInviteJoinComplete(name SessionName,bool bWasSuccessful)
{
	local string URL;

	if (bWasSuccessful)
	{
		if (OnlineSub != None && OnlineSub.GameInterface != None)
		{
			// Get the platform specific information
			if (OnlineSub.GameInterface.GetResolvedConnectString(SessionName,URL))
			{
				URL $= "?bIsFromInvite";

				// allow game to override
				URL = ModifyClientURL(URL);

				`Log("Resulting url is ("$URL$")");
				// Open a network connection to it
				ClientTravel(URL, TRAVEL_Absolute);
			}
		}
	}
	else
	{
		// Do some error handling
		NotifyInviteFailed();
	}
	ClearInviteDelegates();
}

/** Override to display a message to the user */
function NotifyInviteFailed()
{
	`Log("Invite handling failed");
	ClearInviteDelegates();
}

/** Override to display a message to the user */
function NotifyNotAllPlayersCanJoinInvite()
{
	`Log("Not all local players have permission to join the game invite");
}

/** Override to display a message to the user */
function NotifyNotEnoughSpaceInInvite()
{
	`Log("Not enough space for all local players in the game invite");
}

/**
 * Called when an arbitrated match has ended and we need to disconnect
 */
reliable client function ClientArbitratedMatchEnded()
{
	ConsoleCommand("Disconnect");
}

/**
 * Writes the scores for all active players. Override this in your
 * playercontroller class to provide custom scoring
 *
 * @param LeaderboardId the leaderboard the scores are being written to
 */
reliable client function ClientWriteOnlinePlayerScores(int LeaderboardId)
{
	local GameReplicationInfo GRI;
	local int Index;
	local array<OnlinePlayerScore> PlayerScores;
	local UniqueNetId ZeroUniqueId;
	local bool bIsTeamGame;
	local int ScoreIndex;

	GRI = WorldInfo.GRI;
	if (GRI != None && OnlineSub != None && OnlineSub.StatsInterface != None)
	{
		// Look at the default object of the gameinfo to determine if this is a
		// team game or not
		bIsTeamGame = GRI.GameClass != None ? GRI.GameClass.default.bTeamGame : false;
		// Iterate through the players building their score data
		for (Index = 0; Index < GRI.PRIArray.Length; Index++)
		{
			if (GRI.PRIArray[Index].UniqueId != ZeroUniqueId)
			{
				if (bIsTeamGame)
				{
					// Players without teams in team games are excluded from submitted scores
					if (GRI.PRIArray[Index].Team != None)
					{
						ScoreIndex = PlayerScores.Length;
						PlayerScores.Length = ScoreIndex + 1;
						// Build the skill data for this player
						PlayerScores[ScoreIndex].PlayerId = GRI.PRIArray[Index].UniqueId;
						PlayerScores[ScoreIndex].TeamId = GRI.PRIArray[Index].Team.TeamIndex;
						PlayerScores[ScoreIndex].Score = GRI.PRIArray[Index].Team.Score;
					}
				}
				else
				{
					ScoreIndex = PlayerScores.Length;
					PlayerScores.Length = ScoreIndex + 1;
					// Build the skill data for this player
					PlayerScores[ScoreIndex].PlayerId = GRI.PRIArray[Index].UniqueId;
					PlayerScores[ScoreIndex].TeamId = Index;
					PlayerScores[ScoreIndex].Score = GRI.PRIArray[Index].Score;
				}
			}
		}
		// Now write out the scores
		OnlineSub.StatsInterface.WriteOnlinePlayerScores(PlayerReplicationInfo.SessionName,LeaderboardId,PlayerScores);
	}
}

/**
 * Tells the clients to write the stats using the specified stats object
 *
 * @param OnlineStatsWriteClass the stats class to write with
 * @param bIsIncomplete TRUE if the match wasn't finished at the time of the write
 */
reliable client function ClientWriteLeaderboardStats(class<OnlineStatsWrite> OnlineStatsWriteClass, optional bool bIsIncomplete=false);

/**
 * Sets the host's net id for handling dropped arbitrated matches
 *
 * @param InHostId the host's unique net id to report the drop for
 */
reliable client function ClientSetHostUniqueId(UniqueNetId InHostId);

/** Tells this client that it should not send voice data over the network */
reliable client function ClientStopNetworkedVoice()
{
	local LocalPlayer LocPlayer;

	LocPlayer = LocalPlayer(Player);
	if (LocPlayer != None && OnlineSub != None && OnlineSub.VoiceInterface != None)
	{
		OnlineSub.VoiceInterface.StopNetworkedVoice(LocPlayer.ControllerId);
	}
}

/** Tells this client that it should send voice data over the network */
reliable client function ClientStartNetworkedVoice()
{
	local LocalPlayer LocPlayer;

	LocPlayer = LocalPlayer(Player);
	if (LocPlayer != None && OnlineSub != None && OnlineSub.VoiceInterface != None)
	{
		OnlineSub.VoiceInterface.StartNetworkedVoice(LocPlayer.ControllerId);
	}
}

simulated function OnDestroy(SeqAct_Destroy Action)
{
	// Kismet is not allowed to disconnect players from the game
	Action.ScriptLog("Cannot use Destroy action on players");
}

`if(`notdefined(ShippingPC))
/** console control commands, useful when remote debugging so you can't touch the console the normal way */
exec function ConsoleKey(name Key)
{
	if (LocalPlayer(Player) != None)
	{
		LocalPlayer(Player).ViewportClient.ViewportConsole.InputKey(0, Key, IE_Pressed);
	}
}
exec function SendToConsole(string Command)
{
	if (LocalPlayer(Player) != None)
	{
		LocalPlayer(Player).ViewportClient.ViewportConsole.ConsoleCommand(Command);
	}
}
`endif

/**
 * Iterate through list of debug text and draw it over the associated actors in world space.
 * Also handles culling null entries, and reducing the duration for timed debug text.
 */
final simulated function DrawDebugTextList(Canvas Canvas, float RenderDelta)
{
	local vector CameraLoc, ScreenLoc, Offset, WorldTextLoc;
	local rotator CameraRot;
	local int Idx;
	if (DebugTextList.Length > 0)
	{
		GetPlayerViewPoint(CameraLoc,CameraRot);
		Canvas.SetDrawColor(255,255,255);
		for (Idx = 0; Idx < DebugTextList.Length; Idx++)
		{
			if (DebugTextList[Idx].SrcActor == None)
			{
				DebugTextList.Remove(Idx--,1);
				continue;
			}
			if (DebugTextList[Idx].TimeRemaining != -1.f)
			{
				DebugTextList[Idx].TimeRemaining -= RenderDelta;
				if (DebugTextList[Idx].TimeRemaining <= 0.f)
				{
					DebugTextList.Remove(Idx--,1);
					continue;
				}
			}

			if( DebugTextList[Idx].Font != None )
			{
				Canvas.Font = DebugTextList[Idx].Font;
			}
			else
			{
				Canvas.Font = class'Engine'.static.GetSmallFont();
			}


			if(DebugTextList[Idx].bAbsoluteLocation)
			{
				WorldTextLoc = VLerp(DebugTextList[Idx].SrcActorOffset,DebugTextList[Idx].SrcActorDesiredOffset,1.f - (DebugTextList[Idx].TimeRemaining/DebugTextList[Idx].Duration));
			}
			else
			{
				Offset = VLerp(DebugTextList[Idx].SrcActorOffset,DebugTextList[Idx].SrcActorDesiredOffset,1.f - (DebugTextList[Idx].TimeRemaining/DebugTextList[Idx].Duration));

				if( DebugTextList[Idx].bKeepAttachedToActor )
				{
					WorldTextLoc = DebugTextList[Idx].SrcActor.Location + (Offset >> CameraRot);
				}
				else
				{
					WorldTextLoc = DebugTextList[Idx].OrigActorLocation + (Offset >> CameraRot);
				}

			}

			// don't draw text behind the camera
			if ( ((WorldTextLoc - CameraLoc) dot vector(CameraRot)) > 0.f )
			{
				ScreenLoc = Canvas.Project(WorldTextLoc);
				Canvas.SetPos(ScreenLoc.X,ScreenLoc.Y);
				Canvas.DrawColor = DebugTextList[Idx].TextColor;
				Canvas.DrawText(DebugTextList[Idx].DebugText);
			}
		}
	}
}

/**
 * Add debug text for a specific actor to be displayed via DrawDebugTextList().  If the debug text is invalid then it will
 * attempt to remove any previous entries via RemoveDebugText().
 */
reliable client event AddDebugText(string DebugText,
										 optional Actor SrcActor,
										 optional float Duration = -1.f,
										 optional vector Offset,
										 optional vector DesiredOffset,
										 optional color TextColor,
										 optional bool bSkipOverwriteCheck,
										 optional bool bAbsoluteLocation,
										 optional bool bKeepAttachedToActor = TRUE,
										 optional Font InFont
										 )
{
	local int Idx;
	// set a default color
	if (TextColor.R == 0 && TextColor.G == 0 && TextColor.B == 0 && TextColor.A == 0)
	{
		TextColor.R = 255;
		TextColor.G = 255;
		TextColor.B = 255;
		TextColor.A = 255;
	}
	// and a default source actor of our pawn
	if (SrcActor != None)
	{
		if (Len(DebugText) == 0)
		{
			RemoveDebugText(SrcActor);
		}
		else
		{
			//`log("Adding debug text:"@DebugText@"for actor:"@SrcActor);
			// search for an existing entry
			if (!bSkipOverwriteCheck)
			{
				Idx = DebugTextList.Find('SrcActor',SrcActor);
				if (Idx == INDEX_NONE)
				{
					// manually grow the array one struct element
					Idx = DebugTextList.Length;
					DebugTextList.Length = Idx + 1;
				}
			}
			else
			{
				Idx = DebugTextList.Length;
				DebugTextList.Length = Idx + 1;
			}
			// assign the new text and actor
			DebugTextList[Idx].SrcActor = SrcActor;
			DebugTextList[Idx].SrcActorOffset = Offset;
			DebugTextList[Idx].SrcActorDesiredOffset = DesiredOffset;
			DebugTextList[Idx].DebugText = DebugText;
			DebugTextList[Idx].TimeRemaining = Duration;
			DebugTextList[Idx].Duration = Duration;
			DebugTextList[Idx].TextColor = TextColor;
			DebugTextList[Idx].bAbsoluteLocation=bAbsoluteLocation;
			DebugTextList[Idx].bKeepAttachedToActor=bKeepAttachedToActor;
			DebugTextList[Idx].OrigActorLocation = SrcActor.Location;
			DebugTextList[Idx].Font=InFont;
		}
	}
}

/**
 * Remove debug text for the specific actor.
 */
final reliable client event RemoveDebugText(Actor SrcActor)
{
	local int Idx;
	Idx = DebugTextList.Find('SrcActor',SrcActor);
	if (Idx != INDEX_NONE)
	{
		DebugTextList.Remove(Idx,1);
	}
}

/**
 * Remove all debug text
 */
final reliable client event RemoveAllDebugStrings()
{
	DebugTextList.Length=0;
}

/**
 * Registers the host's stat guid with the online subsystem
 *
 * @param StatGuid the stat guid to register
 */
reliable client function ClientRegisterHostStatGuid(string StatGuid)
{
	if (OnlineSub != None && OnlineSub.StatsInterface != None)
	{
		OnlineSub.StatsInterface.AddRegisterHostStatGuidCompleteDelegate(OnRegisterHostStatGuidComplete);
		if(OnlineSub.StatsInterface.RegisterHostStatGuid(StatGuid)==false)
		{
			OnRegisterHostStatGuidComplete(false);
		}
	}
}

/**
 * Called once the host registration has completed. Sends the host this clients stat guid
 *
 * @param bWasSuccessful true if the registration worked, false otherwise
 */
function OnRegisterHostStatGuidComplete(bool bWasSuccessful)
{
	local string StatGuid;

	OnlineSub.StatsInterface.ClearRegisterHostStatGuidCompleteDelegateDelegate(OnRegisterHostStatGuidComplete);
	if (bWasSuccessful)
	{
		StatGuid = OnlineSub.StatsInterface.GetClientStatGuid();
		// Report the client stat guid back to the server
		ServerRegisterClientStatGuid(StatGuid);
	}
}

/**
 * Registers the client's stat guid with the online subsystem
 *
 * @param StatGuid the stat guid to register
 */
reliable server function ServerRegisterClientStatGuid(string StatGuid)
{
	if (OnlineSub != None && OnlineSub.StatsInterface != None)
	{
		OnlineSub.StatsInterface.RegisterStatGuid(PlayerReplicationInfo.UniqueId,StatGuid);
	}
}

/**
 * Starts the online game using the session name in the PRI
 */
reliable client function ClientStartOnlineGame()
{
	local OnlineGameSettings GameSettings;

	if (OnlineSub != None &&
		OnlineSub.GameInterface != None &&
		IsPrimaryPlayer())
	{
		GameSettings = OnlineSub.GameInterface.GetGameSettings(PlayerReplicationInfo.SessionName);
		// Start the game if not already in progress
		if (GameSettings != None &&
			(GameSettings.GameState == OGS_Pending || GameSettings.GameState == OGS_Ended))
		{
			OnlineSub.GameInterface.StartOnlineGame(PlayerReplicationInfo.SessionName);
		}
	}
}

/**
 * Ends the online game using the session name in the PRI
 */
reliable client function ClientEndOnlineGame()
{
	local OnlineGameSettings GameSettings;

	if (OnlineSub != None &&
		OnlineSub.GameInterface != None&&
		IsPrimaryPlayer())
	{
		GameSettings = OnlineSub.GameInterface.GetGameSettings(PlayerReplicationInfo.SessionName);
		// End the game if in progress
		if (GameSettings != None &&
			GameSettings.GameState == OGS_InProgress)
		{
			OnlineSub.GameInterface.EndOnlineGame(PlayerReplicationInfo.SessionName);
		}
	}
}

/** Checks for parental controls blocking user created content */
function bool CanViewUserCreatedContent()
{
	local LocalPlayer LocPlayer;

	LocPlayer = LocalPlayer(Player);
	if (LocPlayer != None && OnlineSub != None && OnlineSub.PlayerInterface != None)
	{
		return OnlineSub.PlayerInterface.CanDownloadUserContent(LocPlayer.ControllerId) == FPL_Enabled;
	}
	return true;
}


function IncrementNumberOfMatchesPlayed()
{
	`log( "  Num Matches Played: " $ PlayerReplicationInfo.AutomatedTestingData.NumberOfMatchesPlayed );
	PlayerReplicationInfo.AutomatedTestingData.NumberOfMatchesPlayed++;
}

/** For AI debugging */
event SoakPause(Pawn P)
{
	`log("Soak pause by "$P);
	SetViewTarget(P);
	SetPause(true);
	myHud.bShowDebugInfo = true;
}

// AI PATHING DEBUG
exec function PathStep( optional int Cnt)
{
	Pawn.IncrementPathStep( Max(1, Cnt), myHud.Canvas );
}
exec function PathChild( optional int Cnt )
{
	Pawn.IncrementPathChild( Max(1, Cnt), myHud.Canvas );
}
exec function PathClear()
{
	Pawn.ClearPathStep();
}

/**
 * Used when a host is telling a client to go to a specific Internet session
 *
 * @param SessionName the name of the session to register
 * @param SearchClass the search that should be populated with the session
 * @param PlatformSpecificInfo the binary data to place in the platform specific areas
 */
reliable client function ClientTravelToSession(name SessionName,class<OnlineGameSearch> SearchClass,byte PlatformSpecificInfo[80])
{
	local OnlineGameSearch Search;
	local LocalPlayer LP;
	local OnlineGameSearchResult SessionToJoin;

	LP = LocalPlayer(Player);
	if (LP != None)
	{
		Search = new SearchClass;
		// Get the information needed to travel to this destination
		if (OnlineSub.GameInterface.BindPlatformSpecificSessionToSearch(LP.ControllerId,Search,PlatformSpecificInfo))
		{
			// There will be only one search result so use it
			SessionToJoin = Search.Results[0];

			`Log("(PlayerController.ClientTravelToSession): "
				$" SessionName="$SessionName
				$" SearchClass="$SearchClass
				$" UniqueId="$OnlineSub.static.UniqueNetIdToString(PlayerReplicationInfo.UniqueId)
				$" Session.OwnerId="$OnlineSub.static.UniqueNetIdToString(SessionToJoin.GameSettings.OwningPlayerId));

			// Fixup game settings object pre-join
			PreJoinUpdateGameSettings(SessionName,SessionToJoin.GameSettings);

			OnlineSub.GameInterface.AddJoinOnlineGameCompleteDelegate(OnJoinTravelToSessionComplete);
			// Now join this Session
			OnlineSub.GameInterface.JoinOnlineGame(LP.ControllerId,SessionName,SessionToJoin);
		}
	}
}

/**
 * Last chance to fixup game settings values before joining a game with those settings
 *
 * @param SessionName name of the session that is about to be joined
 * @param GameSettings settings object to be modified
 */
simulated function PreJoinUpdateGameSettings(name SessionName,OnlineGameSettings GameSettings);

/**
 * Called when the join for the travel destination has completed
 *
 * @param SessionName the name of the session the event is for
 * @param bWasSuccessful whether it worked or not
 */
function OnJoinTravelToSessionComplete(name SessionName,bool bWasSuccessful)
{
	local string URL;

	if (bWasSuccessful)
	{
		// We are joining so grab the connect string to use
		if (OnlineSub.GameInterface.GetResolvedConnectString(SessionName,URL))
		{
			`Log("Resulting url for 'Game' is ("$URL$")");
			// travel to the specified URL
			ClientTravel(URL, TRAVEL_Absolute);
		}
	}
}


/**
 * Used when a host is telling a client to return to their party host from the
 * current session. It looks for the session named 'Party' and does a travel to
 * it. If it's not available, it just does a "disconnect"
 *
 * @param RequestingPlayerId net id of the player that is requesting the travel
 */
reliable client function ClientReturnToParty(UniqueNetId RequestingPlayerId)
{
	local string URL;

	// Disable when returning ourself to menus
	WorldInfo.ToggleHostMigration(false);

	// only do this for the first player in split-screen sessions.
	if (IsPrimaryPlayer())
	{
		if (OnlineSub != None &&
			OnlineSub.GameInterface != None &&
			OnlineSub.PlayerInterface != None)
		{
			// Find the party settings to verify that a party is registered
			if (OnlineSub.GameInterface.GetGameSettings('Party') != None)
			{
				// Now see if we are the party host or not
				if (IsPartyLeader())
				{
					// We are the party host so create the session and listen for returning clients
					URL = GetPartyMapName() $ "?game=" $ GetPartyGameTypeName() $ "?listen";
					// Transition to being the party host without notifying clients and traveling absolute
					WorldInfo.ServerTravel(URL,true,true);
				}
				else
				{
					// We are joining so grab the connect string to use
					if (OnlineSub.GameInterface.GetResolvedConnectString('Party',URL))
					{
						ClientTravel(URL, TRAVEL_Absolute);
					}
				}
			}
			else
			{
				ConsoleCommand("disconnect");
			}
		}
		else
		{
			ConsoleCommand("disconnect");
		}
	}
}

/**
 * RPC notification to client that a party host is about to leave the match
 *
 * @param PartyHostPlayerId net id of the party host that is leaving
 */
reliable client function ClientNotifyPartyHostLeaving(UniqueNetId PartyHostPlayerId)
{
	if (PlayerReplicationInfo != None && PlayerReplicationInfo.UniqueId != PartyHostPlayerId)
	{
		//@todo sz - implement
		`Log(`location @ "Party host is leaving: "
			$" PartyHostPlayerId="$class'OnlineSubsystem'.static.UniqueNetIdToString(PartyHostPlayerId));
	}
}

/**
 * RPC notification to server that a party host is about to leave the match.
 *
 * @param PartyHostPlayerId net id of the party host that is leaving
 */
reliable server function ServerNotifyPartyHostLeaving(UniqueNetId PartyHostPlayerId)
{
	if (WorldInfo.Game != None)
	{
		// Forwarded notification to all clients
		WorldInfo.Game.TellClientsPartyHostIsLeaving(PartyHostPlayerId);
	}
}

/**
 * Wrapper for determining whether this player is the first player on their console.
 *
 * @return	TRUE if this player is not using splitscreen, or is the first player in the split-screen layout.
 */
simulated final function bool IsPrimaryPlayer()
{
	local int SSIndex;

	return !IsSplitscreenPlayer(SSIndex) || SSIndex == 0;
}

/**
 * Determines whether this player is playing split-screen.
 *
 * @param	out_SplitscreenPlayerIndex	receives the index [into the player's local GamePlayers array] for this player, if playing splitscreen.
 *									.
 * @return	TRUE if this player is playing splitscreen.
 */
simulated final function bool IsSplitscreenPlayer(optional out int out_SplitscreenPlayerIndex)
{
	local bool bResult;
	local LocalPlayer LP;
	local NetConnection RemoteConnection;
	local ChildConnection ChildRemoteConnection;

	out_SplitscreenPlayerIndex = NetPlayerIndex;
	if (Player != None)
	{
		LP = LocalPlayer(Player);
		if (LP != None)
		{
			if (LP.Outer.GamePlayers.Length > 1)
			{
				out_SplitscreenPlayerIndex = LP.Outer.GamePlayers.Find(LP);
				bResult = true;
			}
		}
		else
		{
			RemoteConnection = NetConnection(Player);
			if (RemoteConnection.Children.Length > 0)
			{
				out_SplitscreenPlayerIndex = 0;
				bResult = true;
			}
			else
			{
				ChildRemoteConnection = ChildConnection(RemoteConnection);
				if (ChildRemoteConnection != None)
				{
					if (ChildRemoteConnection.Parent != None)
					{
						out_SplitscreenPlayerIndex = ChildRemoteConnection.Parent.Children.Find(ChildRemoteConnection) + 1;
					}
					bResult = true;
				}
			}
		}
	}

	return bResult;
}

/**
 * Returns the PRI associated with the player at the specified index.
 *
 * @param	PlayerIndex		the index [into the local player's GamePlayers array] for the player PRI to find
 *
 * @return	the PRI associated with the player at the specified index, or None if the player is not a split-screen player or
 *			the index was out of range.
 */
simulated function PlayerReplicationInfo GetSplitscreenPlayerByIndex( optional int PlayerIndex=1 )
{
	local PlayerReplicationInfo Result;
	local LocalPlayer LP, SplitPlayer;
	local NetConnection MasterConnection, RemoteConnection;
	local ChildConnection ChildRemoteConnection;

	if ( Player != None )
	{
		if ( IsSplitscreenPlayer() )
		{
			LP = LocalPlayer(Player);
			RemoteConnection = NetConnection(Player);
			if ( LP != None )
			{
				// this PC is a local player
				if ( PlayerIndex >= 0 && PlayerIndex < LP.ViewportClient.Outer.GamePlayers.Length )
				{
					SplitPlayer = LP.ViewportClient.Outer.GamePlayers[PlayerIndex];
					Result = SplitPlayer.Actor.PlayerReplicationInfo;
				}
				else
				{
					`warn(`location $ ":" @ "requested player at invalid index!" @ `showvar(PlayerIndex) @ `showvar(LP.ViewportClient.Outer.GamePlayers.Length,NumLocalPlayers));
				}
			}
			else if ( RemoteConnection != None )
			{
				if ( WorldInfo.NetMode == NM_Client )
				{
					//THIS SHOULD NEVER HAPPEN - IF HAVE A REMOTECONNECTION, WE SHOULDN'T BE A CLIENT
					// this player is a client
					`warn(`location $ ":" @ "CALLED ON CLIENT WITH VALID REMOTE NETCONNECTION!");
				}
				else
				{
					ChildRemoteConnection = ChildConnection(RemoteConnection);
					if ( ChildRemoteConnection != None )
					{
						// this player controller is not the primary player in the splitscreen layout
						MasterConnection = ChildRemoteConnection.Parent;
						if ( PlayerIndex == 0 )
						{
							Result = MasterConnection.Actor.PlayerReplicationInfo;
						}
						else
						{
							PlayerIndex--;
							if ( PlayerIndex >= 0 && PlayerIndex < MasterConnection.Children.Length )
							{
								ChildRemoteConnection = MasterConnection.Children[PlayerIndex];
								Result = ChildRemoteConnection.Actor.PlayerReplicationInfo;
							}
						}
					}
					else if ( RemoteConnection.Children.Length > 0 )
					{
						// this PC is the primary splitscreen player
						if ( PlayerIndex == 0 )
						{
							// they want this player controller's PRI
							Result = PlayerReplicationInfo;
						}
						else
						{
							// our split-screen's PRI is being requested.
							PlayerIndex--;
							if ( PlayerIndex >= 0 && PlayerIndex < RemoteConnection.Children.Length )
							{
								ChildRemoteConnection = RemoteConnection.Children[PlayerIndex];
								Result = ChildRemoteConnection.Actor.PlayerReplicationInfo;
							}
						}
					}
					else
					{
						`log(`location $ ":" @ Player @ "IS NOT THE PRIMARY CONNECTION AND HAS NO CHILD CONNECTIONS!");
					}
				}
			}
			else
			{
				`log(`location $ ":" @ Player @ "IS NOT A LOCALPLAYER AND NOT A REMOTECONNECTION! (No valid Player reference)");
			}
		}
	}
	else
	{
		`log(`location $ ":" @ "NULL value for Player!");
	}

	return Result;
}

/**
 * Returns the number of split-screen players playing on this player's machine.
 *
 * @return	the total number of players on the player's local machine, or 0 if this player isn't playing split-screen.
 */
simulated function int GetSplitscreenPlayerCount()
{
	local LocalPlayer LP;
	local NetConnection RemoteConnection;
	local int Result;

	if ( IsSplitscreenPlayer() )
	{
		if ( Player != None )
		{
			LP = LocalPlayer(Player);
			RemoteConnection = NetConnection(Player);
			if ( LP != None )
			{
				Result = LP.ViewportClient.Outer.GamePlayers.Length;
			}
			else if ( RemoteConnection != None )
			{
				if ( ChildConnection(RemoteConnection) != None )
				{
					// we're the secondary (or otherwise) player in the split - we need to move up to the primary connection
					RemoteConnection = ChildConnection(RemoteConnection).Parent;
				}

				// add one for the primary player
				Result = RemoteConnection.Children.Length + 1;
			}
			else
			{
				`log(`location @ "NOT A LOCALPLAYER AND NOT A REMOTECONNECTION!");
			}
		}
		else
		{
			`log(`location @ "called without a valid Player value!");
		}
	}

	return Result;
}

reliable client function ClientControlMovieTexture(TextureMovie MovieTexture, SeqAct_ControlMovieTexture.EMovieControlType Mode)
{
	if (MovieTexture != None)
	{
		switch (Mode)
		{
			case MCT_Play:
				MovieTexture.Play();
				break;
			case MCT_Stop:
				MovieTexture.Stop();
				break;
			case MCT_Pause:
				MovieTexture.Pause();
				break;
			default:
				break;
		}
	}
}

/**
 * Forces the streaming system to disregard the normal logic for the specified duration and
 * instead always load all mip-levels for all textures used by the specified material.
 *
 * @param Material		- The material whose textures should be forced into memory.
 * @param ForceDuration	- Number of seconds to keep all mip-levels in memory, disregarding the normal priority logic.
 * @param CinematicTextureGroups	- Bitfield indicating which texture groups that use extra high-resolution mips
 */
reliable client event ClientSetForceMipLevelsToBeResident( MaterialInterface Material, float ForceDuration, optional int CinematicTextureGroups )
{
	if ( Material != None && IsPrimaryPlayer() )
	{
		Material.SetForceMipLevelsToBeResident( false, false, ForceDuration, CinematicTextureGroups );
	}
}

/**
 * Forces the streaming system to disregard the normal logic for the specified duration and
 * instead always load all mip-levels for all textures used by the specified actor.
 *
 * @param ForcedActor		- The actor whose textures should be forced into memory.
 * @param ForceDuration		- Number of seconds to keep all mip-levels in memory, disregarding the normal priority logic.
 * @param bEnableStreaming	- Whether to start (TRUE) or stop (FALSE) streaming
 * @param CinematicTextureGroups	- Bitfield indicating which texture groups that use extra high-resolution mips
 */
reliable client event ClientPrestreamTextures( Actor ForcedActor, float ForceDuration, bool bEnableStreaming, optional int CinematicTextureGroups = 0 )
{
	if ( ForcedActor != None && IsPrimaryPlayer() )
	{
		ForcedActor.PrestreamTextures( ForceDuration, bEnableStreaming, CinematicTextureGroups );
	}
}

/** adds a location to the texture streaming system for the specified duration */
reliable client native final event ClientAddTextureStreamingLoc(vector InLoc, float Duration, bool bOverrideLocation);

/**
 * Wrapper for determining whether a player is the party leader.
 *
 * @return	TRUE if the player is the leader of the party; FALSE if not in a party or not the party leader.
 */
simulated function bool IsPartyLeader()
{
	local OnlineGameSettings PartySettings;

	if ( OnlineSub != None && OnlineSub.GameInterface != None )
	{
		// Find the party settings to verify that a party is registered
		PartySettings = OnlineSub.GameInterface.GetGameSettings('Party');
		if ( PartySettings != None )
		{
			if ( PlayerReplicationInfo != None )
			{
				return PartySettings.OwningPlayerId == PlayerReplicationInfo.UniqueId;
			}
		}
	}

	return WorldInfo.NetMode != NM_Client && IsPrimaryPlayer();
}

/**
 * Returns the party map name for this game
 */
static function string GetPartyMapName();

/**
 * Returns the party game info name for this game
 */
static function string GetPartyGameTypeName();

/**
 * Get the completion amount for a game achievement.
 *
 * @param	AchievementId	the id for the achievement to get the completion percetage for
 * @param	CurrentValue	the current number of times the event required to unlock the achievement has occurred.
 * @param	MaxValue		the value that represents 100% completion.
 *
 * @return	TRUE if the AchievementId specified represents an progressive achievement.  FALSE if the achievement is a one-time event
 *			or if the AchievementId specified is invalid.
 */
event bool GetAchievementProgression( int AchievementId, out float CurrentValue, out float MaxValue );



//// SENTINEL FUNCTIONS START

/** This is used to notify the PlayerController that a fly through has ended and then quit if we are doing a Sentinel run **/
simulated function OnFlyThroughHasEnded( SeqAct_FlyThroughHasEnded InAction )
{
	local PlayerController PC;

	if( WorldInfo.Game.IsDoingASentinelRun() )
	{
		foreach WorldInfo.AllControllers(class'PlayerController', PC)
		{
			PC.ConsoleCommand( "quit" );
		}
	}
}


/**
 * This is a function which is called when sentinel is able to start TravelTheWorld.  This allows the specific game
 * to do things such as turning off UI/HUD and to not enter some default starting the game state.
 **/
function Sentinel_SetupForGamebasedTravelTheWorld()
{
}

/**
 * This function is called before we acquire the travel points.
 **/
function Sentinel_PreAcquireTravelTheWorldPoints()
{
}

/**
 * This function is called after we acquire the travel points right before we start traveling.
 **/
function Sentinel_PostAcquireTravelTheWorldPoints()
{
}

//// SENTINEL FUNCTIONS END

delegate InputMatchDelegate()
{
}

/** Attempts to play force-feedback that matches the camera shake.  To be implemented at a higher level (see GameFramework). */
simulated protected function DoForceFeedbackForScreenShake( CameraShake ShakeData, float ShakeScale );

/** Play Camera Shake */
unreliable client function ClientPlayCameraShake( CameraShake Shake, optional float Scale=1.f, optional bool bTryForceFeedback, optional ECameraAnimPlaySpace PlaySpace=CAPS_CameraLocal, optional rotator UserPlaySpaceRot )
{
	if (PlayerCamera != None)
	{
		PlayerCamera.PlayCameraShake(Shake, Scale, PlaySpace, UserPlaySpaceRot);

		if (bTryForceFeedback)
		{
			DoForceFeedbackForScreenShake(Shake, Scale);
		}
	}
}

unreliable client function ClientStopCameraShake( CameraShake Shake )
{
	if (PlayerCamera != None)
	{
		PlayerCamera.StopCameraShake(Shake);
	}
}

/**
 * Scripting hook for camera shakes.
 */
function OnCameraShake(SeqAct_CameraShake inAction)
{
	if (inAction.InputLinks[0].bHasImpulse)
	{
		// start!
		if (InAction.bRadialShake)
		{
			// play an in-world camera shake
			if (InAction.LocationActor != None)
			{
				class'Camera'.static.PlayWorldCameraShake(InAction.Shake, inAction.LocationActor, inAction.LocationActor.Location, inAction.RadialShake_InnerRadius, inAction.RadialShake_OuterRadius, InAction.RadialShake_Falloff, InAction.bDoControllerVibration, inAction.bOrientTowardRadialEpicenter);
			}
			else
			{
				`Warn(self@"Location actor needed for bRadialFalloff camera shake.");
				return;
			}
		}
		else
		{
			// just shake Target(s)
			ClientPlayCameraShake(inAction.Shake, inAction.ShakeScale,
								  inAction.bDoControllerVibration, inAction.PlaySpace,
								  (inAction.LocationActor == None) ? rot(0,0,0) : inAction.LocationActor.Rotation );
		}
	}
	else
	{
		// stop!
		ClientStopCameraShake(inAction.Shake);
	}
}

unreliable client event ClientPlayCameraAnim( CameraAnim AnimToPlay, optional float Scale=1.f, optional float Rate=1.f,
						optional float BlendInTime, optional float BlendOutTime, optional bool bLoop,
						optional bool bRandomStartTime, optional ECameraAnimPlaySpace Space=CAPS_CameraLocal, optional rotator CustomPlaySpace )
{
	local CameraAnimInst AnimInst;
	if (PlayerCamera != None)
	{
		AnimInst = PlayerCamera.PlayCameraAnim(AnimToPlay, Rate, Scale, BlendInTime, BlendOutTime, bLoop, bRandomStartTime);
		if (AnimInst != None && Space != CAPS_CameraLocal)
		{
			AnimInst.SetPlaySpace(Space, CustomPlaySpace);
		}
	}
}

reliable client event ClientStopCameraAnim(CameraAnim AnimToStop, optional bool bImmediate)
{
	if (PlayerCamera != None)
	{
		PlayerCamera.StopAllCameraAnimsByType(AnimToStop, bImmediate);
	}
}

/** Toggle camera animation debug output */
exec function DebugCameraAnims()
{
	bDebugCameraAnims = !bDebugCameraAnims;
	if (bDebugCameraAnims == FALSE)
	{
		// clear debug lines
		WorldInfo.FlushPersistentDebugLines();
	}
}

/** Spawn a camera lens effect (e.g. blood).*/
unreliable client event ClientSpawnCameraLensEffect( class<EmitterCameraLensEffectBase> LensEffectEmitterClass )
{
	if (PlayerCamera != None)
	{
		PlayerCamera.AddCameraLensEffect(LensEffectEmitterClass);
	}
}

/** A trivial way to handle the SetSoundMode kismet action */
function OnSetSoundMode(SeqAct_SetSoundMode Action)
{
	local AudioDevice Audio;

	Audio = class'Engine'.static.GetAudioDevice();
	if (Audio != None)
	{
		if ( Action.InputLinks[0].bHasImpulse && (Action.SoundMode != None) )
		{
			Audio.SetSoundMode(Action.SoundMode.Name);
		}
		else
		{
			Audio.SetSoundMode('Default');
		}
	}
}

/**
 * Determine if this player has a peer connection for the given net id
 *
 * @param PeerNetId net id of remote client peer
 * @return TRUE if the player has the peer connection
 */
native function bool HasPeerConnection(const out UniqueNetId PeerNetId) const;

/**
 * This will move the player and set their rotation to the passed in values.
 * We have this version of the BugIt family as it is easier to type in just raw numbers in the console.
 **/
exec function BugItGo( coerce float X, coerce float Y, coerce float Z, coerce int Pitch, coerce int Yaw, coerce int Roll )
{
	local vector TheLocation;
	local rotator TheRotation;

	TheLocation.X = X;
	TheLocation.Y = Y;
	TheLocation.Z = Z;

	TheRotation.Pitch = Pitch;
	TheRotation.Yaw = Yaw;
	TheRotation.Roll = Roll;

	BugItWorker( TheLocation, TheRotation );
}

/**
* This will move the player and set their rotation to the passed in values.
* We have this version of the BugIt family strings can be passed in from the game ?options easily
**/
function BugItGoString(string TheLocation, string TheRotation)
{
	BugItWorker(vector(TheLocation), rotator(TheRotation));
}

/**
* This will move the player and set their rotation to the passed in values.
* This actually does the location / rotation setting.  Additionally it will set you as ghost as the level may have
* changed since the last time you were here.  And the bug may actually be inside of something.
**/
function BugItWorker( vector TheLocation, rotator TheRotation )
{
	`log( "BugItGo to:" @ TheLocation @ TheRotation );

	if( CheatManager != none )
	{
		CheatManager.Ghost();
	}

	ViewTarget.SetLocation( TheLocation );

	Pawn.FaceRotation( TheRotation, 0.0f );
	SetRotation( TheRotation );
}

/**
* This function is used to print out the BugIt location.  It prints out copy and paste versions for both IMing someone to type in
* and also a gameinfo ?options version so that you can append it to your launching url and be taken to the correct place.
* Additionally, it will take a screen shot so reporting bugs is a one command action!
*
**/
exec event BugIt( optional string ScreenShotDescription )
{
	local vector	ViewLocation;
	local rotator	ViewRotation;

	local String GoString;
	local String LocString;

	ConsoleCommand( "bugscreenshot " $ ScreenShotDescription );

	GetPlayerViewPoint( ViewLocation, ViewRotation );

	if( Pawn != None )
	{
		ViewLocation = Pawn.Location;
	}


	BugItStringCreator( ViewLocation, ViewRotation, GoString, LocString );

	LogOutBugItGoToLogFile( ScreenShotDescription, GoString, LocString );
}

/**
 * Logs the current location in bugit format without taking screenshot and further routing.
 */
exec function LogLoc()
{
	local vector	ViewLocation;
	local rotator	ViewRotation;
	local String GoString;
	local String LocString;

	GetPlayerViewPoint( ViewLocation, ViewRotation );
	if( Pawn != None )
	{
		ViewLocation = Pawn.Location;
	}
	BugItStringCreator( ViewLocation, ViewRotation, GoString, LocString );
}

exec event BugItAI( optional string ScreenShotDescription )
{
	local vector	ViewLocation;
	local rotator	ViewRotation;

	local String GoString;
	local String LocString;

	GetPlayerViewPoint( ViewLocation, ViewRotation );

	if( Pawn != None )
	{
		ViewLocation = Pawn.Location;
	}


	BugItStringCreator( ViewLocation, ViewRotation, GoString, LocString );

	ConsoleCommand("debugai");
	SetTimer(0.1,FALSE,nameof(DisableDebugAI));
	LogOutBugItAIGoToLogFile( ScreenShotDescription, GoString, LocString );
}


/** This will create a BugItGo string for us.  Nice for calling form c++ where you just want the string and no Screenshots **/
exec event BugItStringCreator( const out Vector ViewLocation, const out Rotator ViewRotation, out String GoString, out String LocString )
{
	GoString = "BugItGo " $ ViewLocation.X $ " " $ ViewLocation.Y $ " " $ ViewLocation.Z $ " " $ ViewRotation.Pitch $ " " $ ViewRotation.Yaw $ " " $ ViewRotation.Roll;
	`log( GoString );

	LocString = "?BugLoc=" $ string(ViewLocation) $ "?BugRot=" $ string(ViewRotation);
	`log( LocString );
}

/** An event that is called after the first tick of GEngine */
event OnEngineInitialTick()
{
	// by default, we need to hide any loading movies at this point (a subclass
	// can override this to keep the movie up longer)
	if (WorldInfo.IsConsoleBuild(CONSOLE_Mobile))
	{
		ConsoleCommand("mobile stoploading");
	}
}


`if(`notdefined(FINAL_RELEASE))
/**
 * Logs the list of active PRIs in the game
 */
function DebugLogPRIs()
{
	local int PlayerIndex;
	local UniqueNetId NetId;

	// Log the PRI information for comparison with the online session state
	if (WorldInfo != None &&
		WorldInfo.GRI != None)
	{
		`Log("  Number of PRI players: "$WorldInfo.GRI.PRIArray.Length);
		// List the PRIs that are here to find mismatches between the online state
		for (PlayerIndex = 0; PlayerIndex < WorldInfo.GRI.PRIArray.Length; PlayerIndex++)
		{
			NetId = WorldInfo.GRI.PRIArray[PlayerIndex].UniqueId;
			`Log("    Player: "$WorldInfo.GRI.PRIArray[PlayerIndex].PlayerName$" UID ("$class'OnlineSubsystem'.static.UniqueNetIdToString(NetId)$") PC ("$WorldInfo.GRI.PRIArray[PlayerIndex].Owner$")");
		}
		`Log("");
	}
}
/**
 * Logs the current session state for the online layer
 */
exec function DumpOnlineSessionState()
{
	if (CheatManager != None)
	{
		CheatManager.DumpOnlineSessionState();
	}
	else
	{
		// Log PRI player info
		DebugLogPRIs();
		// Log the online session state
		if (OnlineSub != None)
		{
			OnlineSub.DumpSessionState();
		}
	}
}

/**
 * Logs the current muting state of the server
 */
exec function DumpVoiceMutingState()
{
	local UniqueNetId NetId;
	local PlayerController PC;
	local int MuteIndex;

	`Log("");
	`Log("Voice state");
	`Log("-------------------------------------------------------------");
	`Log("");
	// Log the online view of the voice state
	if (OnlineSub != None)
	{
		OnlineSub.DumpVoiceRegistration();
	}

	`Log("Muting state");
	// For each player list their gameplay mutes and system wide mutes
	foreach WorldInfo.AllControllers(class'PlayerController', PC)
	{
		`Log("  Player: "$PC.PlayerReplicationInfo.PlayerName);
		`Log("    Gameplay mutes: ");
		for (MuteIndex = 0; MuteIndex < PC.GameplayVoiceMuteList.Length; MuteIndex++)
		{
			NetId = PC.GameplayVoiceMuteList[MuteIndex];
			`Log("      "$class'OnlineSubsystem'.static.UniqueNetIdToString(NetId));
		}
		`Log("    System mutes: ");
		for (MuteIndex = 0; MuteIndex < PC.VoiceMuteList.Length; MuteIndex++)
		{
			NetId = PC.VoiceMuteList[MuteIndex];
			`Log("      "$class'OnlineSubsystem'.static.UniqueNetIdToString(NetId));
		}
		`Log("    Voice packet filter: ");
		for (MuteIndex = 0; MuteIndex < PC.VoicePacketFilter.Length; MuteIndex++)
		{
			NetId = PC.VoicePacketFilter[MuteIndex];
			`Log("      "$class'OnlineSubsystem'.static.UniqueNetIdToString(NetId));
		}
		`Log("");
	}
}

/**
 * Logs the list of active peer connections for this player
 */
exec function DumpPeers()
{
	local UniqueNetId NetId;
	local PlayerController PC;
	local int PeerIdx;
	local ConnectedPeerInfo PeerInfo;

	`Log("");
	`Log("Peer List");

	foreach WorldInfo.AllControllers(class'PlayerController', PC)
	{
		if (WorldInfo.NetMode == NM_Client || !PC.IsLocalPlayerController())
		{
			`Log("  Player: "$PC.PlayerReplicationInfo.PlayerName$"("$class'OnlineSubsystem'.static.UniqueNetIdToString(PC.PlayerReplicationInfo.UniqueId)$")");
			`Log("    Peer connections: ");
			for (PeerIdx = 0; PeerIdx < PC.ConnectedPeers.Length; PeerIdx++)
			{
				PeerInfo = PC.ConnectedPeers[PeerIdx];
				`Log("      "$class'OnlineSubsystem'.static.UniqueNetIdToString(PeerInfo.PlayerId)
//					$" NAT="$PeerInfo.NATType
					$" HostLost="$PeerInfo.bLostConnectionToHost);
			}
			`Log("    Best Hosts:");
			for (PeerIdx=0; PeerIdx < PC.BestNextHostPeers.Length; PeerIdx++)
			{
				NetId = PC.BestNextHostPeers[PeerIdx];
				`Log("      "$class'OnlineSubsystem'.static.UniqueNetIdToString(NetId));
			}
			`Log("");
		}
	}
}

`endif

function DisableDebugAI()
{
	ConsoleCommand("debugai");
}

private native function LogOutBugItGoToLogFile( const String InScreenShotDesc, const string InGoString, const string InLocString );
private native function LogOutBugItAIGoToLogFile( const String InScreenShotDesc, const string InGoString, const string InLocString );

/**
 * This function will be called to notify the player controller that the world has received it's game class. In the case of a client
 * we need to initialize the Input System here.
 *
 * @Param GameClass - The Class of the game that was replicated
 */

simulated function ReceivedGameClass(class<GameInfo> GameClass)
{
	if (PlayerInput != none)
	{
		PlayerInput.ClientInitInputSystem();
	}
}

/**
 * Triggered when the 'disconnect' console command is called, to allow cleanup before disconnecting (e.g. for the online subsystem)
 * NOTE: If you block disconnect, store the 'Command' parameter, and trigger ConsoleCommand(Command) when done; be careful to avoid recursion
 *
 * @param Command	The command which triggered disconnection, e.g. "disconnect" or "disconnect local" (can parse additional parameters here)
 * @return		Return True to block the disconnect command from going through, if cleanup can't be completed immediately
 */
event bool NotifyDisconnect(string Command)
{
	return false;
}

/** Function called from matinee to turn on/off a constant camera anim for noise */
event SetMatineeConstantCameraAnim(bool bOn, byte Type, float Rate);


defaultproperties
{
	// The PlayerController is currently required to have collision as there is code that uses the collision
	// for moving the spectator camera around.  Without collision certain assumptions will not hold (e.g. the
	// spectator does not have a pawn so the various game code looks at ALL pawns.  Having a new pawnType will
	// require changes to that code.
	// Removing the collision from the controller and using the controller - pawn mechanic will eventually be coded
	// for spectator-esque functionality
	Begin Object Class=CylinderComponent Name=CollisionCylinder
		CollisionRadius=+22.000000
		CollisionHeight=+22.000000
	End Object
	CollisionComponent=CollisionCylinder
	CylinderComponent=CollisionCylinder
	Components.Add(CollisionCylinder)

	bInteractiveMode=true
	FOVAngle=85.000
	NetPriority=3
	bIsPlayer=true
	bCanDoSpecial=true
	Physics=PHYS_None
	CheatClass=class'Engine.CheatManager'
	InputClass=class'Engine.PlayerInput'

	CameraClass=class'Camera'

	MaxResponseTime=0.125
	ClientCap=0
	LastSpeedHackLog=-100.0
	SavedMoveClass=class'SavedMove'

	DesiredFOV=85.000000
	DefaultFOV=85.000000
	LODDistanceFactor=1.0

	bCinemaDisableInputMove=false
	bCinemaDisableInputLook=false

	bIsUsingStreamingVolumes=TRUE
	SpectatorCameraSpeed=600.0
	MinRespawnDelay=1.0
}
