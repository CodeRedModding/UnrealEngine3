//=============================================================================
// PlayerReplicationInfo.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//
// A PlayerReplicationInfo is created for every player on a server (or in a standalone game).
// Players are PlayerControllers, or other Controllers with bIsPlayer=true
// PlayerReplicationInfos are replicated to all clients, and contain network game relevant information about the player,
// such as playername, score, etc.
//=============================================================================
class PlayerReplicationInfo extends ReplicationInfo
	native(ReplicationInfo)
	nativereplication
	dependson(SoundNodeWave,OnlineSubsystem);

/** Player's current score. */
var repnotify float			Score;

/** Number of player's deaths. */
var int				Deaths;

/** Replicated compressed ping for this player (holds ping in msec divided by 4) */
var byte				Ping;

/** Number of lives used by this player */
var 			int					NumLives;

/** Player name, or blank if none. */
var repnotify string	PlayerName;

/** Voice to use for TTS */
var transient	ETTSSpeaker			TTSSpeaker;

/** Previous playername.  Saved on client-side to detect playername changes. */
var string				OldName;

/** Unique id number. */
var int					PlayerID;

/** Player team */
var editinline RepNotify TeamInfo	Team;

/** Player logged in as Administrator */
var bool				bAdmin;

/** Whether this player is currently a spectator */
var bool				bIsSpectator;

/** Whether this player can only ever be a spectator */
var bool				bOnlySpectator;

/** Whether this player is waiting to enter match */
var bool				bWaitingPlayer;

/** Whether this player has confirmed ready to play */
var bool				bReadyToPlay;

/** Can't respawn once out of lives */
var bool				bOutOfLives;

/** True if this PRI is associated with an AIController */
var bool				bBot;

/** client side flag - whether this player has been welcomed or not (player entered message) */
var	bool				bHasBeenWelcomed;

/** Means this PRI came from the GameInfo's InactivePRIArray */
var repnotify bool bIsInactive;

/** indicates this is a PRI from the previous level of a seamless travel,
 * waiting for the player to finish the transition before creating a new one
 * this is used to avoid preserving the PRI in the InactivePRIArray if the player leaves
 */
var bool bFromPreviousLevel;

/** Elapsed time on server when this PRI was first created.  */
var int					StartTime;

/** Used for reporting player location */
var localized String    StringSpectating;
var localized String	StringUnknown;

/** Kills by this player.  Not replicated. */
var int		Kills;

/** Message class to use for PRI originated localized messages */
var class<GameMessage>	GameMessageClass;

/** Exact ping as float (rounded and compressed in Ping) */
var float				ExactPing;

/** Used to match up InactivePRI with rejoining playercontroller. */
var string				SavedNetworkAddress;

/** The id used by the network to uniquely identify a player.
 * NOTE: this property should *never* be exposed to the player as it's transient
 * and opaque in meaning (ie it might mean date/time followed by something else) */
var repnotify UniqueNetId UniqueId;

/** The session that the player needs to join/remove from as it is created/leaves */
var const name SessionName;

struct native AutomatedTestingDatum
{
	/** Number of matches played (maybe remove this before shipping)  This is really useful for doing soak testing and such to see how long you lasted! NOTE:  This is not replicated out to clients atm. **/
	var int NumberOfMatchesPlayed;

	/** Keeps track of the current run so when we have repeats and such we know how far along we are **/
	var int NumMapListCyclesDone;
};

var AutomatedTestingDatum AutomatedTestingData;

var int StatConnectionCounts;	// Used for Averages;
var int StatPingTotals;
var int StatPingMin;
var int StatPingMax;

var int StatPKLTotal;
var int StatPKLMin;
var int StatPKLMax;

var int StatMaxInBPS, StatAvgInBPS;
var int StatMaxOutBPS, StatAvgOutBPS;

/** The online avatar for this player. May be None if we haven't downloaded it yet, or player doesn't have one. */
var transient Texture2D Avatar;   // not replicated.

cpptext
{
	// AActor interface.
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
}


replication
{
	// Things the server should send to the client.
	if (bNetDirty)
		Score, Deaths,
		PlayerName, Team, bAdmin,
		bIsSpectator, bOnlySpectator, bWaitingPlayer, bReadyToPlay,
		StartTime, bOutOfLives, bFromPreviousLevel,
		// NOTE: This needs to be replicated to the owning client so don't move it from here
		UniqueId;

	// sent to everyone except the player that belongs to this pri
	if (bNetDirty && !bNetOwner)
		Ping;

	if (bNetInitial)
		PlayerID, bBot, bIsInactive;
}

simulated event PostBeginPlay()
{
	// register this PRI with the game's ReplicationInfo
	if ( WorldInfo.GRI != None )
		WorldInfo.GRI.AddPRI(self);

	if ( Role < ROLE_Authority )
		return;

    if (AIController(Owner) != None)
	{
		bBot = true;
	}

	StartTime = WorldInfo.GRI.ElapsedTime;
}

/* epic ===============================================
* ::ClientInitialize
*
* Called by Controller when its PlayerReplicationInfo is initially replicated.
* Now that
*
* =====================================================
*/
simulated function ClientInitialize(Controller C)
{
	local Actor A;
	local PlayerController PlayerOwner;

	SetOwner(C);

	PlayerOwner = PlayerController(C);
	if (PlayerOwner != None)
	{
		// any replicated playercontroller  must be this client's playercontroller
		if (Team != default.Team)
		{
			// wasnt' able to call this in ReplicatedEvent() when Team was replicated, because PlayerController did not have me as its PRI
			foreach AllActors(class'Actor', A)
			{
				A.NotifyLocalPlayerTeamReceived();
			}
		}
	}
}

function SetPlayerTeam( TeamInfo NewTeam )
{
	bForceNetUpdate = Team != NewTeam;

	Team = NewTeam;
}

/* epic ===============================================
* ::ReplicatedEvent
*
* Called when a variable with the property flag "RepNotify" is replicated
*
* =====================================================
*/
simulated event ReplicatedEvent(name VarName)
{
	local Pawn P;
	local PlayerController PC;
	local int WelcomeMessageNum;
	local Actor A;
	local UniqueNetId ZeroId;

	if ( VarName == 'Team' )
	{
		ForEach DynamicActors(class'Pawn', P)
		{
			// find my pawn and tell it
			if ( P.PlayerReplicationInfo == self )
			{
				P.NotifyTeamChanged();
				break;
			}
		}
		ForEach LocalPlayerControllers(class'PlayerController', PC)
		{
			if ( PC.PlayerReplicationInfo == self )
			{
				ForEach AllActors(class'Actor', A)
				{
					A.NotifyLocalPlayerTeamReceived();
				}

				break;
			}
		}
	}
	else if ( VarName == 'PlayerName' )
	{
		// If the new name doesn't match what the local player thinks it should be,
		// then reupdate the name forcing it to be the unique profile name
		if (IsInvalidName())
		{
			return;
		}

		if ( WorldInfo.TimeSeconds < 2 )
		{
			bHasBeenWelcomed = true;
			OldName = PlayerName;
			return;
		}

		// new player or name change
		if ( bHasBeenWelcomed )
		{
			if( ShouldBroadCastWelcomeMessage() )
			{
				ForEach LocalPlayerControllers(class'PlayerController', PC)
				{
					PC.ReceiveLocalizedMessage( GameMessageClass, 2, self );
				}
			}
		}
		else
		{
			if ( bOnlySpectator )
				WelcomeMessageNum = 16;
			else
				WelcomeMessageNum = 1;

			bHasBeenWelcomed = true;

			if( ShouldBroadCastWelcomeMessage() )
			{
				ForEach LocalPlayerControllers(class'PlayerController', PC)
				{
					PC.ReceiveLocalizedMessage( GameMessageClass, WelcomeMessageNum, self );
				}
			}
		}
		OldName = PlayerName;
	}
	else if (VarName == 'UniqueId')
	{
		if (UniqueId != ZeroId)
		{
			// Register the player as part of the session
			RegisterPlayerWithSession();
		}
	}
	else if (VarName == 'bIsInactive')
	{
		// remove and re-add from the GRI so it's in the right list
		WorldInfo.GRI.RemovePRI(self);
		WorldInfo.GRI.AddPRI(self);
	}
}

/**
 * update average ping based on newly received round trip timestamp.
 */
final native function UpdatePing(float TimeStamp);

/**
 * Returns true if should broadcast player welcome/left messages.
 * Current conditions: must be a human player a network game */
simulated function bool ShouldBroadCastWelcomeMessage(optional bool bExiting)
{
	return (!bIsInactive && WorldInfo.NetMode != NM_StandAlone);
}

simulated event Destroyed()
{
	local PlayerController PC;
	local UniqueNetId ZeroId;

	if ( WorldInfo.GRI != None )
	{
		WorldInfo.GRI.RemovePRI(self);
	}

	if( ShouldBroadCastWelcomeMessage(TRUE) )
	{
		ForEach LocalPlayerControllers(class'PlayerController', PC)
		{
			PC.ReceiveLocalizedMessage( GameMessageClass, 4, self);
		}
	}

	if (UniqueId != ZeroId)
	{
		// Remove the player from the online session
		UnregisterPlayerFromSession();
	}

    Super.Destroyed();
}

/* Reset()
reset actor to initial state - used when restarting level without reloading.
*/
function Reset()
{
	Super.Reset();
	Score = 0;
	Kills = 0;
	Deaths = 0;
	bReadyToPlay = false;
	NumLives = 0;
	bOutOfLives = false;
	bForceNetUpdate = TRUE;
}

simulated function string GetHumanReadableName()
{
	return PlayerName;
}

/* DisplayDebug()
list important controller attributes on canvas
*/
simulated function DisplayDebug(HUD HUD, out float YL, out float YPos)
{
	local float XS, YS;

	if ( Team == None )
		HUD.Canvas.SetDrawColor(255,255,0);
	else if ( Team.TeamIndex == 0 )
		HUD.Canvas.SetDrawColor(255,0,0);
	else
		HUD.Canvas.SetDrawColor(64,64,255);
	HUD.Canvas.SetPos(4, YPos);
    HUD.Canvas.Font	= class'Engine'.Static.GetSmallFont();
	HUD.Canvas.StrLen(PlayerName, XS, YS);
	HUD.Canvas.DrawText(PlayerName);
	HUD.Canvas.SetPos(4 + XS, YPos);
	HUD.Canvas.Font	= class'Engine'.Static.GetTinyFont();
	HUD.Canvas.SetDrawColor(255,255,0);

	YPos += YS;
	HUD.Canvas.SetPos(4, YPos);

	if ( (PlayerController(Owner) != None) && (PlayerController(HUD.Owner).ViewTarget != PlayerController(HUD.Owner).Pawn) )
	{
		HUD.Canvas.SetDrawColor(128,128,255);
		HUD.Canvas.DrawText("      bIsSpec:"@bIsSpectator@"OnlySpec:"$bOnlySpectator@"Waiting:"$bWaitingPlayer@"Ready:"$bReadyToPlay@"OutOfLives:"$bOutOfLives);
		YPos += YL;
		HUD.Canvas.SetPos(4, YPos);
	}
}

event SetPlayerName(string S)
{
	PlayerName = S;

	// ReplicatedEvent() won't get called by net code if we are the server
	if (WorldInfo.NetMode == NM_Standalone || WorldInfo.NetMode == NM_ListenServer)
	{
		ReplicatedEvent('PlayerName');
	}
	OldName = PlayerName;
	bForceNetUpdate = TRUE;
}

function SetWaitingPlayer(bool B)
{
	bIsSpectator = B;
	bWaitingPlayer = B;
	bForceNetUpdate = TRUE;
}

/* epic ===============================================
* ::Duplicate
Create duplicate PRI (for saving Inactive PRI)
*/
function PlayerReplicationInfo Duplicate()
{
	local PlayerReplicationInfo NewPRI;

	NewPRI = Spawn(class);
	CopyProperties(NewPRI);
	return NewPRI;
}

/* epic ===============================================
* ::OverrideWith
Get overridden properties from old PRI
*/
function OverrideWith(PlayerReplicationInfo PRI)
{
	bIsSpectator = PRI.bIsSpectator;
	bOnlySpectator = PRI.bOnlySpectator;
	bWaitingPlayer = PRI.bWaitingPlayer;
	bReadyToPlay = PRI.bReadyToPlay;
	bOutOfLives = PRI.bOutOfLives || bOutOfLives;

	Team = PRI.Team;
}

/* epic ===============================================
* ::CopyProperties
Copy properties which need to be saved in inactive PRI
*/
function CopyProperties(PlayerReplicationInfo PRI)
{
	PRI.Score = Score;
	PRI.Deaths = Deaths;
	PRI.Ping = Ping;
	PRI.NumLives = NumLives;
	PRI.PlayerName = PlayerName;
	PRI.PlayerID = PlayerID;
	PRI.StartTime = StartTime;
	PRI.Kills = Kills;
	PRI.bOutOfLives = bOutOfLives;
	PRI.SavedNetworkAddress = SavedNetworkAddress;
	PRI.Team = Team;
	PRI.UniqueId = UniqueId;
	PRI.AutomatedTestingData = AutomatedTestingData;
}

function IncrementDeaths(optional int Amt = 1)
{
	Deaths += Amt;
}

/** called by seamless travel when initializing a player on the other side - copy properties to the new PRI that should persist */
function SeamlessTravelTo(PlayerReplicationInfo NewPRI)
{
	CopyProperties(NewPRI);
	NewPRI.bOnlySpectator = bOnlySpectator;
}


/**
 * Sets the player's unique net id on the server.
 */
simulated function SetUniqueId( UniqueNetId PlayerUniqueId )
{
	// Store the unique id, so it will be replicated to all clients
	UniqueId = PlayerUniqueId;
}

simulated native function byte GetTeamNum();

/**
 * Validates that the new name matches the profile if the player is logged in
 *
 * @return TRUE if the name doesn't match, FALSE otherwise
 */
simulated function bool IsInvalidName()
{
	local LocalPlayer LocPlayer;
	local PlayerController PC;
	local string ProfileName;
	local OnlineSubsystem OnlineSub;

	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		PC = PlayerController(Owner);
		if (PC != None)
		{
			LocPlayer = LocalPlayer(PC.Player);
			if (LocPlayer != None &&
				OnlineSub.GameInterface != None &&
				OnlineSub.PlayerInterface != None)
			{
				// Check to see if they are logged in locally or not
				if (OnlineSub.PlayerInterface.GetLoginStatus(LocPlayer.ControllerId) == LS_LoggedIn)
				{
					// Ignore what ever was specified and use the profile's nick
					ProfileName = OnlineSub.PlayerInterface.GetPlayerNickname(LocPlayer.ControllerId);
					if (ProfileName != PlayerName)
					{
						// Force an update to the proper name
						PC.SetName(ProfileName);
						return true;
					}
				}
			}
		}
	}
	return false;
}

/**
 * The base implementation registers the player with the online session so that
 * recent players list and session counts are updated.
 */
simulated function RegisterPlayerWithSession()
{
	local OnlineSubsystem Online;
	local OnlineRecentPlayersList PlayersList;
	local UniqueNetId ZeroId;

	Online = class'GameEngine'.static.GetOnlineSubsystem();
	if (UniqueId != ZeroId &&
		Online != None &&
		Online.GameInterface != None &&
		SessionName != 'None' &&
		Online.GameInterface.GetGameSettings(SessionName) != None)
	{
		// Register the player as part of the session
		Online.GameInterface.RegisterPlayer(SessionName,UniqueId,false);
		// If this is not us, then add the player to the recent players list
		if (!bNetOwner)
		{
			PlayersList = OnlineRecentPlayersList(Online.GetNamedInterface('RecentPlayersList'));
			if (PlayersList != None)
			{
				PlayersList.AddPlayerToRecentPlayers(UniqueId);
			}
		}
	}
}

/**
 * The base implementation unregisters the player with the online session so that
 * session counts are updated.
 */
simulated function UnregisterPlayerFromSession()
{
	local OnlineSubsystem OnlineSub;
	local UniqueNetId ZeroId;

	// If there is a game and we are a client, unregister this remote player
	if (UniqueId != ZeroId &&
		WorldInfo.NetMode == NM_Client &&
		SessionName != 'None')
	{
		OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
		if (OnlineSub != None &&
			OnlineSub.GameInterface != None &&
			OnlineSub.GameInterface.GetGameSettings(SessionName) != None &&
			// if host migration is currently in-progress then don't unregister players as this is handled manually (see RemoveMissingPeersFromSession)
			!(WorldInfo.PeerHostMigration.bHostMigrationEnabled && WorldInfo.PeerHostMigration.HostMigrationProgress != HostMigration_None))
		{
			// Remove the player from the session
			OnlineSub.GameInterface.UnregisterPlayer(SessionName,UniqueId);
		}
	}
}

/** return TRUE if PRI is primary (ie. non-splitscreen) player */
simulated function bool IsPrimaryPlayer()
{
	return true;
}

defaultproperties
{
	TickGroup=TG_DuringAsyncWork

	RemoteRole=ROLE_SimulatedProxy
	bAlwaysRelevant=True
	NetUpdateFrequency=1
	GameMessageClass=class'GameMessage'

	// The default online session is the game one
	SessionName="Game"
}
