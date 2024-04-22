//=============================================================================
// LocalPlayer
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class LocalPlayer extends Player
	within Engine
	config(Engine)
	inherits(FObserverInterface)
	native
	transient;

/** The controller ID which this player accepts input from. */
var int ControllerId;

/** The master viewport containing this player's view. */
var GameViewportClient ViewportClient;

/** The coordinates for the upper left corner of the master viewport subregion allocated to this player. 0-1 */
var vector2d Origin;

/** The size of the master viewport subregion allocated to this player. 0-1 */
var vector2d Size;

/** Chain of post process effects for this player view */
var transient const PostProcessChain PlayerPostProcess;
var transient const array<PostProcessChain> PlayerPostProcessChains;
/** This gets set when we don't want to use the player chain and use the world/default chain instead */
var transient bool bForceDefaultPostProcessChain;

var private native const pointer ViewState{FSceneViewStateInterface};

// WITH_REALD BEGIN
var private native const pointer ViewState2{FSceneViewStateInterface};
// WITH_REALD END

struct SynchronizedActorVisibilityHistory
{
	var pointer State;
	var pointer CriticalSection;
};

var private native transient const SynchronizedActorVisibilityHistory ActorVisibilityHistory;

/** The location of the player's view the previous frame. */
var transient vector LastViewLocation;

struct native CurrentPostProcessVolumeInfo
{
	/** Last pp settings used when blending to the next set of volume values. */
	var PostProcessSettings	LastSettings;
	/** The last post process volume that was applied to the scene */
	var PostProcessVolume LastVolumeUsed;
	/** Time when a new post process volume was set */
	var float BlendStartTime;
	/** Time when the settings blend was last updated. */
	var float LastBlendTime;
};

/** The Post Process value used  */
var const noimport transient CurrentPostProcessVolumeInfo CurrentPPInfo;

/** Baseline Level Post Process Info */
var const noimport transient CurrentPostProcessVolumeInfo LevelPPInfo;


struct native PostProcessSettingsOverride
{
	var PostProcessSettings Settings;
	var bool bBlendingIn;
    var bool bBlendingOut;
	var float CurrentBlendInTime;       // blend-in progress, in seconds
    var float CurrentBlendOutTime;      // blend-out progress, in seconds

	var float BlendInDuration;          // total time of current blend-in
	var float BlendOutDuration;         // total time of current blend-out

	var float BlendStartTime;           // time this override became active, for blending internal to Settings

	var InterpCurveFloat TimeAlphaCurve;    // Curve to map time with blend alpha. Optional, will override BlendInDuration/BlendOutDuration
};

/** Stack of active overrides.  Restricting this to 1 "active" and have all others be fading out, though
 *  it should be fairly easy to extend to allow gameplay to maintain multiple actives at once. */
var protected transient array<PostProcessSettingsOverride> ActivePPOverrides;

/** How to constrain perspective viewport FOV */
var config EAspectRatioAxisConstraint AspectRatioAxisConstraint;

/** The last map this player remembers being on. Used to determine if the map has changed and the pp needs to be reset to default*/
var string LastMap;
/** Whether or not to use the next map's defaults and ditch the current pp settings */
var bool bWantToResetToMapDefaultPP;

/** set when we've sent a split join request */
var const editconst transient bool bSentSplitJoin;

/** This Local Player's translation context. See GetTranslationContext() below. */
var TranslationContext TagContext;

/** Caches a local reference to the online subsystems auth interface, if it has one set */
var OnlineAuthInterface CachedAuthInt;

/** Whether or not we are awaiting server auth results */
var bool bPendingServerAuth;

/** Timestamp for when server auth started */
var float ServerAuthTimestamp;

/** If this many seconds pass before server auth completes, 'ServerAuthTimedOut' is triggered (from native code) */
var int ServerAuthTimeout;

/** The number of times server authentication has failed */
var int ServerAuthRetryCount;

/** The maximum number of server auth retries */
var int MaxServerAuthRetryCount;

/** Stores the UID of the server currently being authenticated */
var UniqueNetId ServerAuthUID;


cpptext
{
	/** Is object propagation currently overriding our view? */
	static UBOOL bOverrideView;
	static FVector OverrideLocation;
	static FRotator OverrideRotation;

	// Constructor.
	ULocalPlayer();

	/**
	 * Tick tasks required for auth code
	 *
	 * @param DeltaTime	The time passed since the last tick
	 */
	virtual void Tick(FLOAT DeltaTime);

	/**
	 *	Rebuilds the PlayerPostProcessChain.
	 *	This should be called whenever the chain array has items inserted/removed.
	 */
	void RebuildPlayerPostProcessChain();

	/**
	 * Updates the post-process settings for the player's view.
	 * @param ViewLocation - The player's current view location.
	 */
	virtual void UpdatePostProcessSettings(const FVector& ViewLocation);

	/** Update a specific CurrentPostProcessVolumeInfo with the settings and volume specified
	 *
	 *	@param PPInfo - The CurrentPostProcessVolumeInfo struct to update
	 *	@param NewSettings - The PostProcessSettings to apply to PPInfo
	 *	@param NewVolume - The PostProcessVolume to apply to PPInfo
	 */
	virtual void UpdatePPSetting(FCurrentPostProcessVolumeInfo& PPVolume, FPostProcessSettings& NewSettings, const FLOAT CurrentWorldTime);

#if WITH_REALD
	FSceneView* CalcSceneViewOffs( FSceneViewFamily* ViewFamily, FVector& ViewLocation, FRotator& ViewRotation, FViewport* Viewport, FMatrix* Offset=NULL, FViewElementDrawer* ViewDrawer=NULL );
#endif

	/**
	 * Calculate the view settings for drawing from this view actor
	 *
	 * @param	View - output view struct
	 * @param	ViewLocation - output actor location
	 * @param	ViewRotation - output actor rotation
	 * @param	Viewport - current client viewport
	 * @param	ViewDrawer - optional drawing in the view
	 */
	FSceneView* CalcSceneView( FSceneViewFamily* ViewFamily, FVector& ViewLocation, FRotator& ViewRotation, FViewport* Viewport, FViewElementDrawer* ViewDrawer=NULL );

	// UObject interface.
	virtual void FinishDestroy();

	// FExec interface.
	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);

	void ExecMacro( const TCHAR* Filename, FOutputDevice& Ar );

	// FObserverInterface interface
	virtual FVector		GetObserverViewLocation()
	{
		return LastViewLocation;
	}

}

/**
 * Creates an actor for this player.
 * @param URL - The URL the player joined with.
 * @param OutError - If an error occurred, returns the error description.
 * @return False if an error occurred, true if the play actor was successfully spawned.
 */
native final function bool SpawnPlayActor(string URL,out string OutError);

/** sends a splitscreen join command to the server to allow a splitscreen player to connect to the game
 * the client must already be connected to a server for this function to work
 * @note this happens automatically for all viewports that exist during the initial server connect
 * 	so it's only necessary to manually call this for viewports created after that
 * if the join fails (because the server was full, for example) all viewports on this client will be disconnected
 */
native final function SendSplitJoin();

/**
 * Tests the visibility state of an actor in the most recent frame of this player's view to complete rendering.
 * @param TestActor - The actor to check visibility for.
 * @return True if the actor was visible in the frame.
 */
native final function bool GetActorVisibility(Actor TestActor) const;

/**
 * Begins an override of the current post process settings.
 */
simulated native function OverridePostProcessSettings( PostProcessSettings OverrideSettings, optional float BlendInTime );

/**
 * Begins an override of the current post process settings with a time/alpha curve.
 */
simulated native function OverridePostProcessSettingsCurve( PostProcessSettings OverrideSettings, const out InterpCurveFloat Curve);

/**
 * Stop overriding post process settings.
 * Will only affect active overrides -- in-progress blendouts are unaffected.
 * @param BlendOutTime - The amount of time you want to take to recover from the override you are clearing.
 */
simulated native function ClearPostProcessSettingsOverride(optional float BlendOutTime);

/**
 * Changes the ControllerId for this player; if the specified ControllerId is already taken by another player, changes the ControllerId
 * for the other player to the ControllerId currently in use by this player.
 *
 * @param	NewControllerId		the ControllerId to assign to this player.
 */
final function SetControllerId( int NewControllerId )
{
	local LocalPlayer OtherPlayer;
	local int CurrentControllerId;

	if ( ControllerId != NewControllerId )
	{
		`log(Name @ "changing ControllerId from" @ ControllerId @ "to" @ NewControllerId,,'PlayerManagement');

		// first, unregister the player's data stores if we already have a PlayerController.
		if ( Actor != None )
		{
			Actor.PreControllerIdChange();
		}

		CurrentControllerId = ControllerId;

		// set this player's ControllerId to -1 so that if we need to swap controllerIds with another player we don't
		// re-enter the function for this player.
		ControllerId = -1;

		// see if another player is already using this ControllerId; if so, swap controllerIds with them
		OtherPlayer = ViewportClient.FindPlayerByControllerId(NewControllerId);
		if ( OtherPlayer != None )
		{
			OtherPlayer.SetControllerId(CurrentControllerId);
		}

		ControllerId = NewControllerId;
		if ( Actor != None )
		{
			Actor.PostControllerIdChange();
		}
	}
}

/**
 * A TranslationContext is part of the system for managing translation tags in localization text.
 * This system handles text with special tags. E.g.: Press <Controller:GBA_POI/> to look at point of interest.
 * A TranslationContext provides information that cannot be deduced from the text alone.
 * In this case, it is used to differentiate between players 1 and 2.
 * @return Translation Context for this Local Player.
 */
native final function TranslationContext GetTranslationContext();

/**
 * Add the given post process chain to the chain at the given index.
 *
 *	@param	InChain		The post process chain to insert.
 *	@param	InIndex		The position to insert the chain in the complete chain.
 *						If -1, insert it at the end of the chain.
 *	@param	bInClone	If TRUE, create a deep copy of the chains effects before insertion.
 *
 *	@return	boolean		TRUE if the chain was inserted
 *						FALSE if not
 */
native function bool InsertPostProcessingChain(PostProcessChain InChain, int InIndex, bool bInClone);

/**
 * Remove the post process chain at the given index.
 *
 *	@param	InIndex		The position to insert the chain in the complete chain.
 *
 *	@return	boolean		TRUE if the chain was removed
 *						FALSE if not
 */
native function bool RemovePostProcessingChain(int InIndex);

/**
 * Remove all post process chains.
 *
 *	@return	boolean		TRUE if the chain array was cleared
 *						FALSE if not
 */
native function bool RemoveAllPostProcessingChains();

/**
 *	Get the PPChain at the given index.
 *
 *	@param	InIndex				The index of the chain to retrieve.
 *
 *	@return	PostProcessChain	The post process chain if found; NULL if not.
 */
native function PostProcessChain GetPostProcessChain(int InIndex);

/**
 *	Forces the PlayerPostProcess chain to be rebuilt.
 *	This should be called if a PPChain is retrieved using the GetPostProcessChain,
 *	and is modified directly.
 */
native function TouchPlayerPostProcessChain();

/** transforms 2D screen coordinates into a 3D world-space origin and direction
 * @note: use the Canvas version where possible as it already has the necessary information,
 *	whereas this function must gather it and is therefore slower
 * @param ScreenPos - relative screen coordinates (0 to 1, relative to this player's viewport region)
 * @param WorldOrigin (out) - world-space origin vector
 * @param WorldDirection (out) - world-space direction vector
 */
native final function DeProject(vector2D RelativeScreenPos, out vector WorldOrigin, out vector WorldDirection);

/** transforms 3D world coordinates into a 2D screen position (0-1, 0-1)
 * @note: use the Canvas version where possible as it already has the necessary information,
 *	whereas this function must gather it and is therefore slower
 * @param WorldLoc - world location to project
 * @return screen coordinates (0-1, 0-1)
 */
native final function vector2d Project(vector WorldLoc);

/** transforms 2D screen coordinates into a 3D world-space origin and direction
 * @note: use the Canvas version where possible as it already has the necessary information,
 *	whereas this function must gather it and is therefore slower
 * @note: this version does not update visibility lists, physics world camera, or anything else
 *  that the normal DeProject does.  it doesn't call CalcSceneView.
 * @param ScreenPos - relative screen coordinates (0 to 1, relative to this player's viewport region)
 * @param WorldOrigin (out) - world-space origin vector
 * @param WorldDirection (out) - world-space direction vector
 */
native final function FastDeProject(vector2D RelativeScreenPos, out vector WorldOrigin, out vector WorldDirection);

/** transforms 3D world coordinates into a 2D screen position (0-1, 0-1)
 * @note: use the Canvas version where possible as it already has the necessary information,
 *	whereas this function must gather it and is therefore slower
 * @note: this version does not update visibility lists, physics world camera, or anything else
 *  that the normal Project does.  it doesn't call CalcSceneView.
 * @param WorldLoc - world location to project
 * @return screen coordinates (0-1, 0-1)
 */
native final function vector2d FastProject(vector WorldLoc);

/** retrieves this player's unique net ID from the online subsystem */
final event UniqueNetId GetUniqueNetId()
{
	local UniqueNetId Result;
	local GameEngine TheEngine;

	TheEngine = GameEngine(Outer);
	if (TheEngine != None && TheEngine.OnlineSubsystem != None && TheEngine.OnlineSubsystem.PlayerInterface != None)
	{
		TheEngine.OnlineSubsystem.PlayerInterface.GetUniquePlayerId(ControllerId, Result);
	}

	return Result;
}
/** retrieves this player's name/tag from the online subsytem
 * if this function returns a non-empty string, the returned name will replace the "Name" URL parameter
 * passed around in the level loading and connection code, which normally comes from DefaultEngine.ini
 */
event string GetNickname()
{
	local GameEngine TheEngine;

	TheEngine = GameEngine(Outer);
	if (TheEngine != None && TheEngine.OnlineSubsystem != None && TheEngine.OnlineSubsystem.PlayerInterface != None)
	{
		return TheEngine.OnlineSubsystem .PlayerInterface.GetPlayerNickname(ControllerId);
	}
	else
	{
		return "";
	}
}


/**
 * Authentication handling
 */

/**
 * Triggered when the client opens a connection to a server, in order to setup authentication delegates
 */
event NotifyServerConnectionOpen()
{
	local WorldInfo WI;

	// Currently, only the primary local player supports auth code
	if (GamePlayers[0] == self)
	{
		// Setup the online subsystem delegates
		CachedAuthInt = Class'GameEngine'.static.GetOnlineSubsystem().AuthInterface;

		if (CachedAuthInt != none)
		{
			CachedAuthInt.AddClientAuthRequestDelegate(ProcessClientAuthRequest);
			CachedAuthInt.AddServerAuthResponseDelegate(ProcessServerAuthResponse);
			CachedAuthInt.AddServerAuthCompleteDelegate(OnServerAuthComplete);
			CachedAuthInt.AddClientAuthEndSessionRequestDelegate(ProcessClientAuthEndSessionRequest);
			CachedAuthInt.AddServerConnectionCloseDelegate(OnServerConnectionClose);
		}

		// Execute server auth tracking, no matter what the server auth configuration is
		//	(just so the clientside code can implement strict checks, if desired)
		bPendingServerAuth = true;

		WI = Class'WorldInfo'.static.GetWorldInfo();
		if (WI != none)
		{
			ServerAuthTimestamp = WI.RealTimeSeconds;
		}
		else
		{
			// If WorldInfo does not exist, the native code will set ServerAuthTimestamp when it is created (if > RealTimeSeconds)
			ServerAuthTimestamp = 10.0;
		}
	}
}

/**
 * Client authentication handling
 */

/**
 * Called when the client receives a message from the server, requesting a client auth session
 *
 * @param ServerUID		The UID of the game server
 * @param ServerIP		The public (external) IP of the game server
 * @param ServerPort		The port of the game server
 * @param bSecure		whether or not the server has anticheat enabled (relevant to OnlineSubsystemSteamworks and VAC)
 */
function ProcessClientAuthRequest(UniqueNetId ServerUID, int ServerIP, int ServerPort, bool bSecure)
{
	local UniqueNetId NullId;
	local int AuthTicketUID;

	if (ServerUID != NullId)
	{
		if (CachedAuthInt.CreateClientAuthSession(ServerUID, ServerIP, ServerPort, bSecure, AuthTicketUID))
		{
			if (!CachedAuthInt.SendClientAuthResponse(AuthTicketUID))
			{
				`log("LocalPlayer::ProcessClientAuthRequest: WARNING!!! Failed to send auth ticket to server");
			}
		}
		else
		{
			`log("LocalPlayer::ProcessClientAuthRequest: WARNING!!! Failed to create client auth session");
		}
	}
}

/**
 * Called when the client receives a request from the server, to end an active auth session
 *
 * @param ServerConnection	The server NetConnection
 */
function ProcessClientAuthEndSessionRequest(Player ServerConnection)
{
	local LocalAuthSession CurClientSession;
	local AuthSession CurServerSession;

	if (CachedAuthInt.FindLocalClientAuthSession(ServerConnection, CurClientSession))
	{
		CachedAuthInt.EndLocalClientAuthSession(CurClientSession.EndPointUID, CurClientSession.EndPointIP, CurClientSession.EndPointPort);
	}
	else
	{
		`log("LocalPlayer::ProcessClientAuthEndSessionRequest: Couldn't find local client auth session");
	}

	if (CachedAuthInt.FindServerAuthSession(ServerConnection, CurServerSession))
	{
		CachedAuthInt.EndRemoteServerAuthSession(CurServerSession.EndPointUID, CurServerSession.EndPointIP);
	}
	else
	{
		`log("LocalPlayer::ProcessClientAuthEndSessionRequest: Couldn't find server auth session");
	}
}


/**
 * Server authentication handling
 */

/**
 * Called when the client receives auth data from the server, needed for authentication
 *
 * @param ServerUID		The UID of the server
 * @param ServerIP		The IP of the server
 * @param AuthTicketUID		The UID used to reference the auth data
 */
function ProcessServerAuthResponse(UniqueNetId ServerUID, int ServerIP, int AuthTicketUID)
{
	local WorldInfo WI;

	if (CachedAuthInt.VerifyServerAuthSession(ServerUID, ServerIP, AuthTicketUID))
	{
		bPendingServerAuth = True;
		ServerAuthUID = ServerUID;

		WI = Class'WorldInfo'.static.GetWorldInfo();
		if (WI != none)
		{
			ServerAuthTimestamp = WI.RealTimeSeconds;
		}
		else
		{
			// If WorldInfo does not exist, the native code will set ServerAuthTimestamp when it is created (if > RealTimeSeconds)
			ServerAuthTimestamp = 10.0;
		}

		`log("Kicked off game server auth");
	}
	else
	{
		`log("Failed to kickoff game server auth");
	}
}

/**
 * Called on the client, when the authentication result for the server has returned
 *
 * @param bSuccess		whether or not authentication was successful
 * @param ServerUID		The UID of the server
 * @param ServerConnection	The connection associated with the server (for retrieving auth session data)
 * @param ExtraInfo		Extra information about authentication, e.g. failure reasons
 */
function OnServerAuthComplete(bool bSuccess, UniqueNetId ServerUID, Player ServerConnection, string ExtraInfo)
{
	if (bSuccess)
	{
		`log("Server auth success");
		bPendingServerAuth = False;
	}
	else
	{
		// Retry auth
		ServerAuthTimedOut();
	}
}

/**
 * Triggered by native code, if server auth times out
 * NOTE: Determined by: (WorldInfo.RealTimeSeconds - ServerAuthTimestamp) > ServerAuthTimeout
 */
event ServerAuthTimedOut()
{
	local WorldInfo WI;
	local AuthSession CurServerSession;

	if (CachedAuthInt != none)
	{
		if (ServerAuthRetryCount < MaxServerAuthRetryCount)
		{
			// End the current server auth session, before retrying
			foreach CachedAuthInt.AllServerAuthSessions(CurServerSession)
			{
				if (CurServerSession.EndPointUID == ServerAuthUID)
				{
					CachedAuthInt.EndRemoteServerAuthSession(CurServerSession.EndPointUID, CurServerSession.EndPointIP);
					break;
				}
			}

			`log("Sending server auth retry request");

			// Send a retry request
			CachedAuthInt.SendServerAuthRetryRequest();

			ServerAuthRetryCount++;

			// Update the auth timestamp
			WI = Class'WorldInfo'.static.GetWorldInfo();

			if (WI != none)
			{
				ServerAuthTimestamp = WI.RealTimeSeconds;
			}
			else
			{
				ServerAuthTimestamp = 10.0;
			}
		}
		else
		{
			ServerAuthFailure();
		}
	}
}

/**
 * Called when server authentication fails completely, after several retries.
 * If strict handling of server auth is required (e.g. disconnecting the client), implement it here
 */
function ServerAuthFailure()
{
	`log("Server authentication failed after"@MaxServerAuthRetryCount@"tries");

	// Uncomment to enable strict authentication checks
	//Actor.ConsoleCommand("Disconnect");
}


/**
 * Server disconnect cleanup
 */

/**
 * Called on the client when a server net connection is closing (so auth sessions can be ended)
 * NOTE: Triggered >before< NotifyServerConnectionClose
 *
 * @param ServerConnection	The server NetConnection that is closing
 */
function OnServerConnectionClose(Player ServerConnection)
{
	// Pass on to the static function, to keep code in one place
	StaticOnServerConnectionClose(ServerConnection);
}

/**
 * Static version of the above, to be called when there is no valid LocalPlayer instance (e.g. at exit)
 *
 * @param ServerConnection	The server NetConnection that is closing
 */
static final function StaticOnServerConnectionClose(Player ServerConnection)
{
	local OnlineAuthInterface CurAuthInt;
	local LocalAuthSession CurClientSession;
	local AuthSession CurServerSession;

	CurAuthInt = Class'GameEngine'.static.GetOnlineSubsystem().AuthInterface;

	if (CurAuthInt != none)
	{
		if (CurAuthInt.FindLocalClientAuthSession(ServerConnection, CurClientSession))
		{
			CurAuthInt.EndLocalClientAuthSession(CurClientSession.EndPointUID, CurClientSession.EndPointIP,
								CurClientSession.EndPointPort);
		}
		else
		{
			`log("LocalPlayer::StaticOnServerConnectionClose: Couldn't find local client auth session");
		}

		if (CurAuthInt.FindServerAuthSession(ServerConnection, CurServerSession))
		{
			CurAuthInt.EndRemoteServerAuthSession(CurServerSession.EndPointUID, CurServerSession.EndPointIP);
		}
		else
		{
			`log("LocalPlayer::StaticOnServerConnectionClose: Couldn't find server auth session");
		}
	}
}


/**
 * Exit cleanup
 */

/**
 * Triggered when the viewport associated with this LocalPlayer has been closed, and the LocalPlayer is about to become invalid
 * NOTE: This usually happens just before game exit, by clicking the window close button for the game
 */
event ViewportClosed()
{
	Cleanup(True);
}

/**
 * Triggered when the client closes the connection to the server, for cleaning up authentication delegates
 */
event NotifyServerConnectionClose()
{
	Cleanup();
}

/**
 * Trigered by the game engine upon exit, for online subsystem cleanup
 */
event Exit()
{
	// Trigger regular cleanup
	Cleanup(True);
}

/**
 * Cleans up online subsystem delegates upon server disconnect, or game exit
 *
 * @param bExit		whether or not cleanup was triggered due to game exit
 */
function Cleanup(optional bool bExit)
{
	// Currently, only the primary local player supports auth code
	if (GamePlayers[0] == self)
	{
		// Clear online subsystem delegates
		if (CachedAuthInt != none)
		{
			CachedAuthInt.ClearClientAuthRequestDelegate(ProcessClientAuthRequest);
			CachedAuthInt.ClearServerAuthResponseDelegate(ProcessServerAuthResponse);
			CachedAuthInt.ClearServerAuthCompleteDelegate(OnServerAuthComplete);
			CachedAuthInt.ClearClientAuthEndSessionRequestDelegate(ProcessClientAuthEndSessionRequest);
			CachedAuthInt.ClearServerConnectionCloseDelegate(OnServerConnectionClose);

			if (bExit)
			{
				// End local client auth sessions
				CachedAuthInt.EndAllLocalClientAuthSessions();

				// End server auth sessions
				CachedAuthInt.EndAllRemoteServerAuthSessions();
			}
		}

		CachedAuthInt = none;
		bPendingServerAuth = False;
	}
}


defaultproperties
{
//	bOverridePostProcessSettings=false
	// inital postprocess setting should be set, not faded in
	bWantToResetToMapDefaultPP=true
	bForceDefaultPostProcessChain=false

	ServerAuthTimeout=10
	MaxServerAuthRetryCount=3
}
