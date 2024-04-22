//=============================================================================
// AccessControl.
//
// AccessControl is a helper class for GameInfo.
// The AccessControl class determines whether or not the player is allowed to
// login in the PreLogin() function, controls whether or not a player can enter
// as a spectator or a game administrator, and handles authentication of
// clients with the online subsystem (including the listen server host).
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class AccessControl extends Info
	dependson(OnlineAuthInterface)
	config(Game);

/** Contains policies for allowing/denying IP addresses */
var globalconfig array<string>		IPPolicies;

/** Contains the list of banned UIDs */
var globalconfig array<UniqueNetID>	BannedIDs;


/** Various localized strings */
var localized string			IPBanned;
var localized string			WrongPassword;
var localized string			NeedPassword;
var localized string			SessionBanned;
var localized string			KickedMsg;
var localized string			DefaultKickReason;
var localized string			IdleKickReason;


var class<Admin>			AdminClass;

/** Password required for admin privileges */
var private globalconfig string		AdminPassword;

/** Password required to enter the game */
var private globalconfig string		GamePassword;

var localized string			ACDisplayText[3];
var localized string			ACDescText[3];

var bool				bDontAddDefaultAdmin;

/** Whether or not to authenticate clients (specifically their UID's) when they join; client UID's can't be trusted until they are authenticated */
var globalconfig bool			bAuthenticateClients;

/** Whether or not to authenticate the game server with clients; a client must be fully authenticated before it can authenticate the server */
var globalconfig bool			bAuthenticateServer;

/** Whether or not to authenticate the listen host, on lists servers */
var globalconfig bool			bAuthenticateListenHost;

/** The maximum number of times to retry authentication */
var globalconfig int			MaxAuthRetryCount;

/** The delay between authentication attempts */
var globalconfig int			AuthRetryDelay;

/** Caches a local reference to the online subsystem */
var OnlineSubsystem				OnlineSub;

/** Caches a local reference to the online subsystems auth interface, if it has one set */
var OnlineAuthInterface			CachedAuthInt;

/** Whether or not this classes auth delegates have been registered with the auth interface */
var bool						bAuthDelegatesRegistered;

/** Struct used for tracking clients pending authentication */
struct PendingClientAuth
{
	var Player	ClientConnection;	// The NetConnection of the client pending auth
	var UniqueNetId	ClientUID;		// The UID of the client

	var float	AuthTimestamp;		// The timestamp for when authentication was started
	var int		AuthRetryCount;		// The number of times authentication has been retried for this client
};

/** Tracks clients who are currently pending authentication */
var array<PendingClientAuth>		ClientsPendingAuth;

/** Struct used for tracking server auth retry counts */
struct ServerAuthRetry
{
	var UniqueNetId	ClientUID;		// The UID of the client requesting retries
	var int		AuthRetryCount;		// The number of times server authentication has been retried for this client
};

/** Tracks server auth retry requests for clients */
var array<ServerAuthRetry>		ServerAuthRetries;

/** Whether or not the listen host is pending authentication */
var bool bPendingListenAuth;

/** Stores the UID of the listen server auth ticket */
var int ListenAuthTicketUID;

/** The number of times listen host auth has been retried */
var int ListenAuthRetryCount;


function PostBeginPlay()
{
	OnlineSub = Class'GameEngine'.static.GetOnlineSubsystem();
	InitAuthHooks();
}

function Destroyed()
{
	Cleanup();
}

/**
 * Checks whether or not the specified PlayerController is an admin
 *
 * @param P	The PlayerController to check
 * @return	TRUE if the specified player has admin privileges.
 */
function bool IsAdmin(PlayerController P)
{
	if ( P != None )
	{
		if ( Admin(P) != None )
		{
			return true;
		}

		if ( P.PlayerReplicationInfo != None && P.PlayerReplicationInfo.bAdmin )
		{
			return true;
		}
	}

	return false;
}

function bool SetAdminPassword(string P)
{
	AdminPassword = P;
	return true;
}

function SetGamePassword(string P)
{
	GamePassword = P;
	WorldInfo.Game.UpdateGameSettings();
}

function bool RequiresPassword()
{
	return GamePassword != "";
}

/**
 * Takes a string and tries to find the matching controller associated with it.  First it searches as if the string is the
 * player's name.  If it doesn't find a match, it attempts to resolve itself using the target as the player id.
 *
 * @Params	Target		The search key
 *
 * @returns the controller associated with the key.  NONE is a valid return and means not found.
 */
function Controller GetControllerFromString(string Target)
{
	local Controller C,FinalC;
	local int i;

	FinalC = none;
	foreach WorldInfo.AllControllers(class'Controller', C)
	{
		if (C.PlayerReplicationInfo != None && (C.PlayerReplicationInfo.PlayerName ~= Target || C.PlayerReplicationInfo.PlayerName ~= Target))
		{
			FinalC = C;
			break;
		}
	}

	// if we didn't find it by name, attempt to convert the target to a player index and look him up if possible.
	if ( C == none && WorldInfo != none && WorldInfo.GRI != none )
	{
		for (i=0;i<WorldInfo.GRI.PRIArray.Length;i++)
		{
			if ( String(WorldInfo.GRI.PRIArray[i].PlayerID) == Target )
			{
				FinalC = Controller(WorldInfo.GRI.PRIArray[i].Owner);
				break;
			}
		}
	}

	return FinalC;
}

function Kick( string Target )
{
	local Controller C;

	C = GetControllerFromString(Target);
	if ( C != none && C.PlayerReplicationInfo != None )
	{
		if (PlayerController(C) != None)
		{
			KickPlayer(PlayerController(C), DefaultKickReason);
		}
		else if (C.PlayerReplicationInfo != None)
		{
			if (C.Pawn != None)
			{
				C.Pawn.Destroy();
			}
			if (C != None)
			{
				C.Destroy();
			}
		}
	}
}

function KickBan( string Target )
{
	local PlayerController P;
	local string IP;

	P =  PlayerController( GetControllerFromString(Target) );
	if ( NetConnection(P.Player) != None )
	{
		if (!WorldInfo.IsConsoleBuild())
		{
			IP = P.GetPlayerNetworkAddress();
			if( CheckIPPolicy(IP) )
			{
				IP = Left(IP, InStr(IP, ":"));
				`Log("Adding IP Ban for: "$IP);
				IPPolicies[IPPolicies.length] = "DENY," $ IP;
				SaveConfig();
			}
		}

		if ( P.PlayerReplicationInfo.UniqueId != P.PlayerReplicationInfo.default.UniqueId &&
			!IsIDBanned(P.PlayerReplicationInfo.UniqueID) )
		{
			BannedIDs.AddItem(P.PlayerReplicationInfo.UniqueId);
			SaveConfig();
		}
		KickPlayer(P, DefaultKickReason);
		return;
	}
}

function bool ForceKickPlayer(PlayerController C, string KickReason)
{
	if (C != None && NetConnection(C.Player)!=None )
	{
		if (C.Pawn != None)
		{
			C.Pawn.Suicide();
		}
		C.ClientWasKicked();
		if (C != None)
		{
			C.Destroy();
		}
		return true;
	}
	return false;
}

function bool KickPlayer(PlayerController C, string KickReason)
{
	// Do not kick logged admins
	if (C != None && !IsAdmin(C) && NetConnection(C.Player)!=None )
	{
		return ForceKickPlayer(C, KickReason);
	}
	return false;
}

function bool AdminLogin( PlayerController P, string Password )
{
	if (AdminPassword == "")
	{
		return false;
	}

	if (Password == AdminPassword)
	{
		P.PlayerReplicationInfo.bAdmin = true;
		return true;
	}

	return false;
}

function bool AdminLogout(PlayerController P)
{
	if (P.PlayerReplicationInfo.bAdmin)
	{
		P.PlayerReplicationInfo.bAdmin = false;
		P.bGodMode = false;
		P.Suicide();

		return true;
	}

	return false;
}

function AdminEntered( PlayerController P )
{
	local string LoginString;

	LoginString = P.PlayerReplicationInfo.PlayerName@"logged in as a server administrator.";

	`log(LoginString);
	WorldInfo.Game.Broadcast( P, LoginString );
}
function AdminExited( PlayerController P )
{
	local string LogoutString;

	LogoutString = P.PlayerReplicationInfo.PlayerName$"is no longer logged in as a server administrator.";

	`log(LogoutString);
	WorldInfo.Game.Broadcast( P, LogoutString );
}

/**
 * Parses the specified string for admin auto-login options
 *
 * @param	Options		a string containing key/pair options from the URL (?key=value,?key=value)
 *
 * @return	TRUE if the options contained name and password which were valid for admin login.
 */
function bool ParseAdminOptions( string Options )
{
	local string InAdminName, InPassword;

	InPassword = class'GameInfo'.static.ParseOption( Options, "Password" );
	InAdminName= class'GameInfo'.static.ParseOption( Options, "AdminName" );

	return ValidLogin(InAdminName, InPassword);
}

/**
 * @return	TRUE if the specified username + password match the admin username/password
 */
function bool ValidLogin(string UserName, string Password)
{
	return (AdminPassword != "" && Password==AdminPassword);
}

function bool CheckIPPolicy(string Address)
{
	local int i, j;
`if(`notdefined(FINAL_RELEASE))
	local int LastMatchingPolicy;
`endif
	local string Policy, Mask;
	local bool bAcceptAddress, bAcceptPolicy;

	// strip port number
	j = InStr(Address, ":");
	if(j != -1)
		Address = Left(Address, j);

	bAcceptAddress = True;
	for(i=0; i<IPPolicies.Length; i++)
	{
		j = InStr(IPPolicies[i], ",");
		if(j==-1)
			continue;
		Policy = Left(IPPolicies[i], j);
		Mask = Mid(IPPolicies[i], j+1);
		if(Policy ~= "ACCEPT")
			bAcceptPolicy = True;
			else if(Policy ~= "DENY")
			bAcceptPolicy = False;
		else
			continue;

		j = InStr(Mask, "*");
		if(j != -1)
		{
			if(Left(Mask, j) == Left(Address, j))
			{
				bAcceptAddress = bAcceptPolicy;
				`if(`notdefined(FINAL_RELEASE))
				LastMatchingPolicy = i;
				`endif
			}
		}
		else
		{
			if(Mask == Address)
			{
				bAcceptAddress = bAcceptPolicy;
				`if(`notdefined(FINAL_RELEASE))
				LastMatchingPolicy = i;
				`endif
			}
		}
	}

	if(!bAcceptAddress)
	{
		`Log("Denied connection for "$Address$" with IP policy "$IPPolicies[LastMatchingPolicy]);
	}

	return bAcceptAddress;
}

function bool IsIDBanned(const out UniqueNetID NetID)
{
	local int i;

	for (i = 0; i < BannedIDs.length; i++)
	{
		if (BannedIDs[i] == NetID)
		{
			return true;
		}
	}
	return false;
}


/**
 * Client authentication (and PreLogin handling)
 */

/**
 * Initialized auth interface hooks
 */
function InitAuthHooks()
{
	local OnlineGameSettings GameSettings;
	local bool bIsLanMatch;

	if (OnlineSub != None)
	{
		CachedAuthInt = OnlineSub.AuthInterface;
		if (OnlineSub.GameInterface != none)
		{
			GameSettings = OnlineSub.GameInterface.GetGameSettings(WorldInfo.Game.PlayerReplicationInfoClass.default.SessionName);
		}

		// If 'bIsLanMatch' is set, do not enable any authentication
		if ((WorldInfo.NetMode == NM_DedicatedServer || WorldInfo.NetMode == NM_ListenServer) && GameSettings != none &&
			GameSettings.bIsLanMatch)
		{
			if (bAuthenticateClients)
			{
				`log("Disabling all authentication, due to bIsLanMatch being set to true");
			}

			bIsLanMatch = true;
		}
	}
	else
	{
		bIsLanMatch = true;
	}

	if (!bIsLanMatch && bAuthenticateClients)
	{
		RegisterAuthDelegates();
	}
}

function RegisterAuthDelegates()
{
	if (CachedAuthInt != None)
	{
		CachedAuthInt.AddAuthReadyDelegate(OnAuthReady);
		CachedAuthInt.AddServerAuthRequestDelegate(ProcessServerAuthRequest);
		CachedAuthInt.AddClientAuthResponseDelegate(ProcessClientAuthResponse);
		CachedAuthInt.AddClientAuthCompleteDelegate(OnClientAuthComplete);
		CachedAuthInt.AddClientConnectionCloseDelegate(OnClientConnectionClose);
		CachedAuthInt.AddServerAuthRetryRequestDelegate(ProcessServerAuthRetryRequest);

		CachedAuthInt.ClearClientConnectionCloseDelegate(Class'AccessControl'.static.StaticOnClientConnectionClose);

		if (OnlineSub.GameInterface != None)
		{
			OnlineSub.GameInterface.AddDestroyOnlineGameCompleteDelegate(OnDestroyOnlineGameComplete);
		}

		bAuthDelegatesRegistered = true;
	}
	else
	{
		// Don't display this message for OnlineSubsystemSteamworks
		if (OnlineSub.Class.Name != 'OnlineSubsystemSteamworks')
		{
			`log("AccessControl: Trying to register authentication delegates with an online subsystem that does not support authentication");
		}

		bAuthDelegatesRegistered = false;
	}
}

function ClearAuthDelegates(bool bExiting)
{
	if (CachedAuthInt != None)
	{
		CachedAuthInt.ClearAuthReadyDelegate(OnAuthReady);
		CachedAuthInt.ClearServerAuthRequestDelegate(ProcessServerAuthRequest);
		CachedAuthInt.ClearClientAuthResponseDelegate(ProcessClientAuthResponse);
		CachedAuthInt.ClearClientAuthCompleteDelegate(OnClientAuthComplete);
		CachedAuthInt.ClearClientConnectionCloseDelegate(OnClientConnectionClose);
		CachedAuthInt.ClearServerAuthRetryRequestDelegate(ProcessServerAuthRetryRequest);

		// OnClientConnectionClose must still be handled, even if the AccessControl does not exist during non-seamless travel
		if (!bExiting)
		{
			CachedAuthInt.AddClientConnectionCloseDelegate(class'AccessControl'.static.StaticOnClientConnectionClose);
		}

		if (OnlineSub != None && OnlineSub.GameInterface != None)
		{
			OnlineSub.GameInterface.ClearDestroyOnlineGameCompleteDelegate(OnDestroyOnlineGameComplete);
		}
	}
	else
	{
		`log("AccessControl: Trying to clear authentication delegates with an online subsystem that does not support authentication");
	}

	bAuthDelegatesRegistered = false;
}

/**
 * Accept or reject a joining player on the server; fails login if OutError is set to a non-empty string
 * NOTE: UniqueId requires authentication before it can be trusted
 *
 * @param Options	URL options the player used when connecting
 * @param Address	The IP address of the player
 * @param UniqueId	The UID of the player (requires authentication before it can be trusted)
 * @param bSupportsAuth	whether or not the client supports authentication (i.e. has an AuthInterface set)
 * @param OutError	If the player fails any checks in this function, set this to a non-empty value to reject the player
 * @param bSpectator	whether or not the player is trying to join as a spectator
 */
event PreLogin(string Options, string Address, const UniqueNetId UniqueId, bool bSupportsAuth, out string OutError, bool bSpectator)
{
	local string InPassword;
	local int i, CurIP, CurPort, ClientIP, LingeringPort;
	local bool bFound, bSuccess;
	local UniqueNetId NullId, HostUID;
	local Player ClientConn, CurConn;
	local AuthSession CurClientSession;
	local OnlineGameSettings GameSettings;

	OutError="";
	InPassword = WorldInfo.Game.ParseOption(Options, "Password");

	// Check server capacity and passwords
	if (WorldInfo.NetMode != NM_Standalone && WorldInfo.Game.AtCapacity(bSpectator))
	{
		OutError = PathName(WorldInfo.Game.GameMessageClass)$".MaxedOutMessage";
	}
	else if (GamePassword != "" && !(InPassword == GamePassword) && (AdminPassword == "" || !(InPassword == AdminPassword)))
	{
		OutError = (InPassword == "") ? "Engine.AccessControl.NeedPassword" : "Engine.AccessControl.WrongPassword";
	}

	// Check server IP bans (UID bans are checked in GameInfo::PreLogin)
	if (!CheckIPPolicy(Address))
	{
		OutError = "Engine.AccessControl.IPBanned";
	}

	// If the client was not already rejected, handle authentication of the clients UID
	if (bAuthenticateClients && OutError == "" && CachedAuthInt != None && bAuthDelegatesRegistered)
	{
		if (OnlineSub != None && OnlineSub.GameInterface != None)
		{
			GameSettings = OnlineSub.GameInterface.GetGameSettings(WorldInfo.Game.PlayerReplicationInfoClass.default.SessionName);

			// If 'bIsLanMatch' is set, do not enable any authentication
			if ((WorldInfo.NetMode == NM_DedicatedServer || WorldInfo.NetMode == NM_ListenServer) && GameSettings != None && !GameSettings.bIsLanMatch)
			{
				// If the client does not support authentication, reject him immediately
				if (!bSupportsAuth)
				{
					if (OnlineSub.Class.Name == 'OnlineSubsystemSteamworks')
					{
						OutError = "Engine.Errors.SteamClientRequired";
					}
					else
					{
						OutError = "Server requires authentication";
					}
				}

				// Pause the login process for the client
				if (OutError == "")
				{
					ClientConn = WorldInfo.Game.PauseLogin();
				}

				if (ClientConn != none)
				{
					// If there are any other client connections from the same UID and IP, kick them (fixes an auth issue,
					//	preventing players from rejoining if they were disconnected, and the old connection still lingers)

					// First find the joining clients IP
					foreach WorldInfo.AllClientConnections(CurConn, CurIP, CurPort)
					{
						if (CurConn == ClientConn)
						{
							ClientIP = CurIP;
							break;
						}
					}

					// See if there is an active auth session matching the same IP and UID
					LingeringPort = 0;

					foreach CachedAuthInt.AllClientAuthSessions(CurClientSession)
					{
						if (CurClientSession.EndPointIP == ClientIP && CurClientSession.EndPointUID == UniqueId)
						{
							LingeringPort = CurClientSession.EndPointPort;
							break;
						}
					}


					// If there was an existing active auth session, match it up to the lingering connection and disconnect it
					if (LingeringPort != 0)
					{
						foreach WorldInfo.AllClientConnections(CurConn, CurIP, CurPort)
						{
							if (CurConn != ClientConn && CurIP == ClientIP && CurPort == LingeringPort)
							{
								`log("Closing old connection with duplicate IP ("$Address$") and SteamId ("$
									Class'OnlineSubsystem'.static.UniqueNetIdToString(UniqueId)$")",, 'DevNet');

								WorldInfo.Game.RejectLogin(CurConn, "");

								break;
							}
						}
					}


					// If there are other client connections from the same UID, but not the same IP, reject the new player
					// NOTE: The above code shouldn't affect this, as OnClientConnectionClose (which cleans up lists)
					//		is called during RejectLogin
					for (i=0; i<ClientsPendingAuth.Length; i++)
					{
						if (ClientsPendingAuth[i].ClientUID == UniqueId)
						{
							bFound = True;
							break;
						}
					}

					if (!bFound)
					{
						foreach CachedAuthInt.AllClientAuthSessions(CurClientSession)
						{
							if (CurClientSession.EndPointUID == UniqueId && CurClientSession.EndPointIP != ClientIP)
							{
								bFound = True;
								break;
							}
						}
					}


					// Make sure the player is not trying to join with a listen hosts UID
					if (WorldInfo.NetMode == NM_ListenServer && OnlineSub.PlayerInterface != none &&
						OnlineSub.PlayerInterface.GetUniquePlayerId(0, HostUID) && UniqueId == HostUID)
					{
						bFound = True;
					}


					// If the UID is not already present on server, and is not otherwise invalid, begin authentication
					if (!bFound && UniqueId != NullId)
					{
						// Begin authentication, and if it kicks off successfully, start tracking the auth progress
						if (CachedAuthInt.IsReady())
						{
							bSuccess = CachedAuthInt.SendClientAuthRequest(ClientConn, UniqueId);

							if (bSuccess && !IsTimerActive('PendingAuthTimer'))
							{
								SetTimer(3.0, True, nameof(PendingAuthTimer));
							}
						}
						// If the auth interface is not ready, add an entry anyway, and kick off auth later when it is ready
						else
						{
							bSuccess = True;
						}

						if (bSuccess)
						{
							i = ClientsPendingAuth.Length;
							ClientsPendingAuth.Length = i+1;

							ClientsPendingAuth[i].ClientConnection = ClientConn;
							ClientsPendingAuth[i].ClientUID = UniqueId;
							ClientsPendingAuth[i].AuthTimestamp = WorldInfo.RealTimeSeconds;
						}
						else
						{
							OutError = "Failed to kickoff authentication";
						}
					}
					// Reject the client if the current UID is already being authenticated
					else if (bFound)
					{
						OutError = "Duplicate UID";
					}
					// Reject the client straight away if their UID is null
					else
					{
						OutError = "Invalid UID";
					}
				}
				else if (OutError == "")
				{
					OutError = "Failed to kickoff authentication";
				}
			}
		}
	}
}

/**
 * Triggered after a player has successfully joined the game (post-auth for remote clients, pre-auth for listen host);
 * used to kickoff authentication of listen hosts
 *
 * @param NewPlayer	The newly logged in player
 */
function PostLogin(PlayerController NewPlayer)
{
	if (LocalPlayer(NewPlayer.Player) != none && bAuthDelegatesRegistered && bAuthenticateListenHost &&
		WorldInfo.NetMode == NM_ListenServer && CachedAuthInt != none)
	{
		if (CachedAuthInt.IsReady())
		{
			BeginListenHostAuth();
		}
		else
		{
			bPendingListenAuth = true;
		}
	}
}

/**
 * Called once every 3 seconds, to see if any auth attempts have timed out
 */
function PendingAuthTimer()
{
	local int i, OldLength;
	local bool bFailed;
	local AuthSession CurClientSession;

	for (i=0; i<ClientsPendingAuth.Length; ++i)
	{
		// Remove any connections that have become invalid
		if (ClientsPendingAuth[i].ClientConnection == none)
		{
			ClientsPendingAuth.Remove(i, 1);
			i--;
		}
		// Need to detect level change messing up timestamps (and reset it)
		else if (WorldInfo.RealTimeSeconds < ClientsPendingAuth[i].AuthTimestamp)
		{
			ClientsPendingAuth[i].AuthTimestamp = WorldInfo.RealTimeSeconds;
		}
		// Handle timeouts and retries
		else if (WorldInfo.RealTimeSeconds - ClientsPendingAuth[i].AuthTimestamp >= AuthRetryDelay)
		{
			if (CachedAuthInt.FindClientAuthSession(ClientsPendingAuth[i].ClientConnection, CurClientSession))
			{
				if (ClientsPendingAuth[i].AuthRetryCount < MaxAuthRetryCount)
				{
					// End the auth session first before retrying
					CachedAuthInt.EndRemoteClientAuthSession(CurClientSession.EndPointUID, CurClientSession.EndPointIP);

					// Get the client to end it on his end too (this should execute on client before the new auth request below)
					CachedAuthInt.SendClientAuthEndSessionRequest(ClientsPendingAuth[i].ClientConnection);

					// Start the new auth session
					if (CachedAuthInt.SendClientAuthRequest(ClientsPendingAuth[i].ClientConnection, CurClientSession.EndPointUID))
					{
						ClientsPendingAuth[i].AuthTimestamp = WorldInfo.RealTimeSeconds;
						ClientsPendingAuth[i].AuthRetryCount++;
					}
					else
					{
						bFailed = True;
					}
				}
				else
				{
					bFailed = True;
				}

				if (bFailed)
				{
					`log("Client authentication timed out after"@MaxAuthRetryCount@"tries",, 'DevOnline');

					OldLength = ClientsPendingAuth.Length;

					WorldInfo.Game.RejectLogin(ClientsPendingAuth[i].ClientConnection, "Authentication failed");

					// If OnClientConnectionClose did not alter ClientsPendingAuth, remove the entry now
					if (OldLength == ClientsPendingAuth.Length)
					{
						ClientsPendingAuth.Remove(i, 1);
					}

					i--;
				}
			}
		}
	}

	if (ClientsPendingAuth.Length == 0)
	{
		ClearTimer('PendingAuthTimer');
	}
}

/**
 * Called when the auth interface is ready to perform authentication (may not be called, if the auth interface was already ready)
 * NOTE: Listen host authentication may be kicked off here
 */
function OnAuthReady()
{
	local int i, OldLength;

	if (bAuthDelegatesRegistered)
	{
		// If there are any pending client auth's queued, kickoff authentication
		for (i=0; i<ClientsPendingAuth.Length; ++i)
		{
			// Remove invalid entries
			if (ClientsPendingAuth[i].ClientConnection == none)
			{
				ClientsPendingAuth.Remove(i, 1);
				i--;

				continue;
			}

			if (CachedAuthInt.SendClientAuthRequest(ClientsPendingAuth[i].ClientConnection, ClientsPendingAuth[i].ClientUID))
			{
				ClientsPendingAuth[i].AuthTimestamp = WorldInfo.RealTimeSeconds;
			}
			else
			{
				OldLength = ClientsPendingAuth.Length;

				// Kick the client
				WorldInfo.Game.RejectLogin(ClientsPendingAuth[i].ClientConnection, "Authentication failed");

				// If OnClientConnectionClose did not alter ClientsPendingAuth, remove the entry now
				if (OldLength == ClientsPendingAuth.Length)
				{
					ClientsPendingAuth.Remove(i, 1);
				}

				i--;

				continue;
			}
		}

		// If any clients are now pending auth, activate the pending auth timer
		if (ClientsPendingAuth.Length > 0)
		{
			`log("OnAuthReady: Kicking off delayed auth for clients");

			SetTimer(3.0, True, nameof(PendingAuthTimer));
		}

		if (bAuthenticateListenHost && WorldInfo.NetMode == NM_ListenServer && bPendingListenAuth)
		{
			BeginListenHostAuth();
		}
	}
}

/**
 * Called when the server receives auth data from a client, needed for authentication
 *
 * @param ClientUID		The UID of the client
 * @param ClientIP		The IP of the client
 * @param AuthTicketUID		The UID used to reference the auth data
 */
function ProcessClientAuthResponse(UniqueNetId ClientUID, int ClientIP, int AuthTicketUID)
{
	local bool bSuccess;
	local int i, PendingIdx, OldLength;

	// Check that we are expecting auth data from this client
	PendingIdx = INDEX_None;

	for (i=0; i<ClientsPendingAuth.Length; i++)
	{
		if (ClientsPendingAuth[i].ClientUID == ClientUID)
		{
			PendingIdx = i;
			break;
		}
	}

	if (PendingIdx != INDEX_None)
	{
		// Now that the client has sent auth data required for verification, finish verifying the client
		bSuccess = CachedAuthInt.VerifyClientAuthSession(ClientUID, ClientIP, 0, AuthTicketUID);

		// If auth verification failed to kickoff successfully, kick the client
		if (!bSuccess)
		{
			OldLength = ClientsPendingAuth.Length;

			// Kick the client
			WorldInfo.Game.RejectLogin(ClientsPendingAuth[i].ClientConnection, "Authentication failed");

			// If OnClientConnectionClose did not alter ClientsPendingAuth, remove the tracking entry here
			if (OldLength == ClientsPendingAuth.Length)
			{
				ClientsPendingAuth.Remove(PendingIdx, 1);
			}
		}
	}
	else
	{
		`log("AccessControl::ProcessClientAuthResponse: Received unexpected auth ticket from client",, 'DevOnline');
	}
}

/**
 * Called on the server, when the authentication result for a client auth session has returned
 * NOTE: This is the first place where a clients UID is verified as valid
 *
 * @param bSuccess		whether or not authentication was successful
 * @param ClientUID		The UID of the client
 * @param ClientConnection	The connection associated with the client (for retrieving auth session data)
 * @param ExtraInfo		Extra information about authentication, e.g. failure reasons
 */
function OnClientAuthComplete(bool bSuccess, UniqueNetId ClientUID, Player ClientConnection, string ExtraInfo)
{
	local UniqueNetId HostUID;
	local int i, PendingLen, PendingIdx;
	local PlayerController P;
	local PlayerReplicationInfo PRI;
	local bool bResumeLogin;
	local AuthSession CurClientSession;

	// Check if the auth result was for the listen host
	if (WorldInfo.NetMode == NM_ListenServer && OnlineSub.PlayerInterface != none &&
		OnlineSub.PlayerInterface.GetUniquePlayerId(0, HostUID) && HostUID == ClientUID)
	{
		if (bSuccess)
		{
			`log("Listen host successfully authenticated");

			ClearTimer('ListenHostAuthTimeout');
			ClearTimer('ContinueListenHostAuth');
		}

		return;
	}

	// Check that we are expecting an auth result for this client
	PendingLen = ClientsPendingAuth.Length;
	PendingIdx = INDEX_None;

	for (i=0; i<PendingLen; i++)
	{
		if ((ClientConnection != none && ClientsPendingAuth[i].ClientConnection == ClientConnection) ||
			(ClientConnection == none && ClientsPendingAuth[i].ClientUID == ClientUID))
		{
			PendingIdx = i;
			break;
		}
	}

	if (PendingIdx != INDEX_None)
	{
		if (ClientConnection != none)
		{
			if (bSuccess)
			{
				foreach WorldInfo.AllControllers(Class'PlayerController', P)
				{
					if (P.Player == ClientConnection)
					{
						PRI = P.PlayerReplicationInfo;
						break;
					}
				}

				if (PRI != none)
				{
					// If the code is setup to >not< pause at login, the UID needs to be stored in the PRI from here
					P.PlayerReplicationInfo.SetUniqueId(ClientUID);

					`log("Client '"$PRI.PlayerName$"'passed authentication, UID:"@
						Class'OnlineSubsystem'.static.UniqueNetIdToString(ClientUID));
				}
				else
				{
					`log("Client passed authentication, UID:"@Class'OnlineSubsystem'.static.UniqueNetIdToString(ClientUID));
				}

				bResumeLogin = True;

				// Kick off server auth
				if (bAuthenticateServer)
				{
					if (CachedAuthInt.FindClientAuthSession(ClientConnection, CurClientSession))
					{
						ProcessServerAuthRequest(ClientConnection, ClientUID, CurClientSession.EndPointIP,
										CurClientSession.EndPointPort);
					}
					else
					{
						`log("Failed to kickoff server auth; could not find matching client session");
					}
				}
			}
			else
			{
				`log("Client failed authentication (unauthenticated UID:"@
					Class'OnlineSubsystem'.static.UniqueNetIdToString(ClientUID)$"), kicking");

				// Kick the client
				WorldInfo.Game.RejectLogin(ClientConnection, "Authentication failed");
			}
		}

		// Remove the tracking entry, if it was not removed above
		if (ClientsPendingAuth.Length == PendingLen)
		{
			ClientsPendingAuth.Remove(PendingIdx, 1);
		}
	}
	else
	{
		`log("AccessControl::OnClientAuthComplete: Received unexpected auth result for client",, 'DevOnline');
	}

	if (bResumeLogin)
	{
		WorldInfo.Game.ResumeLogin(ClientConnection);
	}
}


/**
 * Server authentication
 */

/**
 * Called when the server receives a message from a client, requesting a server auth session
 *
 * @param ClientConnection	The NetConnection of the client the request came from
 * @param ClientUID		The UID of the client making the request
 * @param ClientIP		The IP of the client making the request
 * @param ClientPort		The port the client is on
 */
function ProcessServerAuthRequest(Player ClientConnection, UniqueNetId ClientUID, int ClientIP, int ClientPort)
{
	local int AuthTicketUID;
	local LocalAuthSession CurServerSession;
	local bool bFound;

	// NOTE: Native code handles checking of whether or not client is authenticated
	if (bAuthenticateServer)
	{
		// Make sure there is not already a server auth session for this client
		foreach CachedAuthInt.AllLocalServerAuthSessions(CurServerSession)
		{
			if (CurServerSession.EndPointUID == ClientUID && CurServerSession.EndPointIP == ClientIP)
			{
				bFound = true;
			}
		}

		if (!bFound)
		{
			// Kickoff server auth
			if (CachedAuthInt.CreateServerAuthSession(ClientUID, ClientIP, ClientPort, AuthTicketUID))
			{
				if (!CachedAuthInt.SendServerAuthResponse(ClientConnection, AuthTicketUID))
				{
					`log("WARNING!!! Failed to send auth ticket to client");
				}
			}
			else
			{
				`log("Failed to kickoff server auth",, 'DevOnline');
			}
		}
	}
}

/**
 * Called when the server receives a server auth retry request from a client
 *
 * @param ClientConnection	The client NetConnection
 */
function ProcessServerAuthRetryRequest(Player ClientConnection)
{
	local bool bFoundAndAuthenticated;
	local int ClientIP, ClientPort, i, CurRetryIdx;
	local UniqueNetId ClientUID;
	local AuthSession CurClientSession;
	local LocalAuthSession CurServerSession;

	if (bAuthenticateServer && ClientConnection != none)
	{
		bFoundAndAuthenticated =	CachedAuthInt.FindClientAuthSession(ClientConnection, CurClientSession) &&
						CurClientSession.AuthStatus == AUS_Authenticated;

		// Only execute a server auth retry, if the client is fully authenticated
		if (bFoundAndAuthenticated)
		{
			ClientUID = CurClientSession.EndPointUID;
			ClientIP = CurClientSession.EndPointIP;
			ClientPort = CurClientSession.EndPointPort;

			CurRetryIdx = INDEX_None;

			for (i=0; i<ServerAuthRetries.Length; ++i)
			{
				if (ServerAuthRetries[i].ClientUID == ClientUID)
				{
					CurRetryIdx = i;
					break;
				}
			}

			if (CurRetryIdx == INDEX_None)
			{
				CurRetryIdx = ServerAuthRetries.Length;
				ServerAuthRetries.Length = CurRetryIdx + 1;

				ServerAuthRetries[CurRetryIdx].ClientUId = ClientUID;
			}


			// Only attempt server auth retry, if the retry count has not been exceeded
			if (ServerAuthRetries[CurRetryIdx].AuthRetryCount < MaxAuthRetryCount)
			{
				// End the current server auth session
				foreach CachedAuthInt.AllLocalServerAuthSessions(CurServerSession)
				{
					if (CurServerSession.EndPointUID == ClientUID)
					{
						CachedAuthInt.EndLocalServerAuthSession(ClientUID, ClientIP);
						break;
					}
				}

				// Kick off a new server auth session
				ProcessServerAuthRequest(ClientConnection, ClientUID, ClientIP, ClientPort);

				// Update the retry count
				ServerAuthRetries[CurRetryIdx].AuthRetryCount++;
			}
			// Kick the client, if they spam retry requests
			else if (ServerAuthRetries[CurRetryIdx].AuthRetryCount > Max(30, MaxAuthRetryCount + 20))
			{
				WorldInfo.Game.RejectLogin(ClientConnection, "Spamming server auth");
			}
			else
			{
				// Update the retry count
				ServerAuthRetries[CurRetryIdx].AuthRetryCount++;
			}
		}
	}
}


/**
 * Listen host authentication
 */

/**
 * Kicks off authentication of the listen host
 */
function BeginListenHostAuth(optional bool bRetry)
{
	local UniqueNetId ServerUID, HostUID;
	local int ServerIP, ServerPort;
	local OnlineGameSettings GameSettings;
	local bool bGotHostInfo, bFound, bSecure;
	local AuthSession CurClientSession, ListenSession;

	bPendingListenAuth = false;

	if (CachedAuthInt.IsReady())
	{
		bGotHostInfo =	CachedAuthInt.GetServerUniqueId(ServerUID) &&
				CachedAuthInt.GetServerAddr(ServerIP, ServerPort) &&
				OnlineSub.PlayerInterface.GetUniquePlayerId(0, HostUID);
	}

	if (bGotHostInfo)
	{
		// Search for an existing listen host auth session first
		foreach CachedAuthInt.AllClientAuthSessions(CurClientSession)
		{
			if (CurClientSession.EndPointUID == HostUID && CurClientSession.EndPointIP == ServerIP)
			{
				ListenSession = CurClientSession;
				bFound = true;

				break;
			}
		}

		// If there is not an existing session, kick one off
		if (!bFound || bRetry)
		{
			`log("Kicking off listen auth session");

			if (OnlineSub.GameInterface != none)
			{
				GameSettings = OnlineSub.GameInterface.GetGameSettings(WorldInfo.Game.PlayerReplicationInfoClass.default.SessionName);
			}

			if (GameSettings != none)
			{
				bSecure = GameSettings.bAntiCheatProtected;
			}

			// Kickoff authentication
			if (CachedAuthInt.CreateClientAuthSession(ServerUID, ServerIP, ServerPort, bSecure, ListenAuthTicketUID))
			{
				// Give the auth interface a moment to setup the auth session, before verifying
				SetTimer(1.0, false, nameof(ContinueListenHostAuth));
			}

			SetTimer(AuthRetryDelay, false, nameof(ListenHostAuthTimeout));
		}
		// If there is an existing session, do nothing if already authenticated, or enable timeout if not
		else if (ListenSession.AuthStatus != AUS_Authenticated && !IsTimerActive('ListenHostAuthTimeout'))
		{
			`log("BeginListenHostAuth was called when there is already a listen auth session, but the timeout is not active");

			SetTimer(AuthRetryDelay, false, nameof(ListenHostAuthTimeout));
		}
	}
	else
	{
		`log("Failed to kickoff listen host authentication");
		// Go straight to failed auth
		OnlineSub.PlayerInterface.GetUniquePlayerId(0, HostUID);
		OnClientAuthComplete(false, HostUID, None, "Failed to kickoff listen host authentication");
	}
}

/**
 * After listen host authentication kicks off, this is called after a short delay, to continue authentication
 */
function ContinueListenHostAuth()
{
	local bool bGotHostInfo;
	local UniqueNetId HostUID;
	local int ServerIP, ServerPort;

	if (OnlineSub.PlayerInterface != none)
	{
		bGotHostInfo =	OnlineSub.PlayerInterface.GetUniquePlayerId(0, HostUID) &&
				CachedAuthInt.GetServerAddr(ServerIP, ServerPort);
	}

	if (!bGotHostInfo || !CachedAuthInt.VerifyClientAuthSession(HostUID, ServerIP, ServerPort, ListenAuthTicketUID))
	{
		`log("VerifyClientAuthSession failed for listen host");
		OnlineSub.PlayerInterface.GetUniquePlayerId(0, HostUID);
		OnClientAuthComplete(false, HostUID, None, "VerifyClientAuthSession failed for listen host");
	}
}

/**
 * Ends any active listen host auth sessions
 */
function EndListenHostAuth()
{
	local bool bGotHostInfo;
	local UniqueNetId ServerUID, HostUID;
	local int ServerIP, ServerPort;

	if (OnlineSub.PlayerInterface != none)
	{
		bGotHostInfo =	CachedAuthInt.GetServerUniqueId(ServerUID) &&
				CachedAuthInt.GetServerAddr(ServerIP, ServerPort) &&
				OnlineSub.PlayerInterface.GetUniquePlayerId(0, HostUID);
	}

	if (bGotHostInfo)
	{
		CachedAuthInt.EndLocalClientAuthSession(ServerUID, ServerIP, ServerPort);
		CachedAuthInt.EndRemoteClientAuthSession(HostUID, ServerIP);
	}
	else
	{
		`log("Failed to end listen host auth session");
	}
}

/**
 * Triggered upon listen host authentication failure, or timeout
 */
function ListenHostAuthTimeout()
{
	local UniqueNetId HostUID;

	ClearTimer('ListenHostAuthTimeout');
	ClearTimer('ContinueListenHostAuth');

	if (ListenAuthRetryCount < MaxAuthRetryCount)
	{
		ListenAuthRetryCount++;

		// Retry auth again
		BeginListenHostAuth(true);
	}
	else
	{
		`log("Listen host authentication failed after"@MaxAuthRetryCount@"attempts");
		OnlineSub.PlayerInterface.GetUniquePlayerId(0, HostUID);
		OnClientAuthComplete(false, HostUID, None, "VerifyClientAuthSession failed for listen host");
		EndListenHostAuth();
	}
}


/**
 * Client disconnect cleanup
 */

/**
 * Called on the server when a clients net connection is closing (so auth sessions can be ended)
 *
 * @param ClientConnection	The client NetConnection that is closing
 */
function OnClientConnectionClose(Player ClientConnection)
{
	local int i;

	if (ClientConnection != none)
	{
		// End the auth session for the exiting client (done in the static function to keep it in one place)
		StaticOnClientConnectionClose(ClientConnection);

		// Remove from tracking
		for (i=0; i<ClientsPendingAuth.Length; ++i)
		{
			if (ClientConnection != none && ClientsPendingAuth[i].ClientConnection == ClientConnection)
			{
				ClientsPendingAuth.Remove(i, 1);
				break;
			}
		}
	}
}

/**
 * It is extremely important that client disconnects are detected, even when an AccessControl does not exist;
 * otherwise, clients may be kept in an active auth session, even though they should not be (Steam in particular, is picky about this).
 *
 * When the AccessControl is cleaning up before server travel, it adds this static function as a delegate, until after server travel;
 * ensuring disconnects are always handled
 *
 * @param ClientConnection	The client NetConnection that is closing
 */
static final function StaticOnClientConnectionClose(Player ClientConnection)
{
	local OnlineSubsystem CurOnlineSub;
	local OnlineAuthInterface CurAuthInt;
	local int i;
	local WorldInfo WI;
	local AuthSession CurClientSession;
	local LocalAuthSession CurServerSession;

	CurOnlineSub = Class'GameEngine'.static.GetOnlineSubsystem();

	if (CurOnlineSub != none)
	{
		CurAuthInt = CurOnlineSub.AuthInterface;
	}

	if (CurAuthInt != none && ClientConnection != none)
	{
		// If the client is authenticated, end the client auth session
		if (CurAuthInt.FindClientAuthSession(ClientConnection, CurClientSession) && CurClientSession.AuthStatus == AUS_Authenticated)
		{
			CurAuthInt.EndRemoteClientAuthSession(CurClientSession.EndPointUID, CurClientSession.EndPointIP);
		}

		// End any local server session
		if (CurAuthInt.FindLocalServerAuthSession(ClientConnection, CurServerSession))
		{
			CurAuthInt.EndLocalServerAuthSession(CurServerSession.EndPointUID, CurServerSession.EndPointIP);

			// Remove any 'ServerAuthRetries' entry
			WI = Class'WorldInfo'.static.GetWorldInfo();

			if (WI != none && WI.Game != none && WI.Game.AccessControl != none)
			{
				for (i=0; i<WI.Game.AccessControl.ServerAuthRetries.Length; ++i)
				{
					if (WI.Game.AccessControl.ServerAuthRetries[i].ClientUID == CurServerSession.EndPointUID)
					{
						WI.Game.AccessControl.ServerAuthRetries.Remove(i, 1);
						break;
					}
				}
			}
		}
	}
}


/**
 * Exit/mapchange cleanup
 */

/**
 * Triggered when the current online game has ended; used to end auth sessions
 * NOTE: Delegate cleanup does not happen here
 */
function OnDestroyOnlineGameComplete(name SessionName, bool bWasSuccessful)
{
	local int CurIP, CurPort;
	local Player ClientConn, CurConn;
	local AuthSession CurClientSession;

	// End listen host auth
	if (WorldInfo.NetMode == NM_ListenServer)
	{
		EndListenHostAuth();
	}


	// End auth for all connected clients
	foreach CachedAuthInt.AllClientAuthSessions(CurClientSession)
	{
		if (CurClientSession.AuthStatus == AUS_Authenticated)
		{
			// End the client auth session
			CachedAuthInt.EndRemoteClientAuthSession(CurClientSession.EndPointUID, CurClientSession.EndPointIP);

			// Tell the client to end the auth session their end
			foreach WorldInfo.AllClientConnections(CurConn, CurIP, CurPort)
			{
				if (CurIP == CurClientSession.EndPointIP && CurPort == CurClientSession.EndPointPort)
				{
					ClientConn = CurConn;
					break;
				}
			}

			if (ClientConn != none)
			{
				if (!CachedAuthInt.SendClientAuthEndSessionRequest(ClientConn))
				{
					`log("Failed to send client kill auth request");
				}
			}
			else
			{
				`log("WARNING!!! Came across client auth session with no matching NetConnection");
			}
		}
	}

	// End all local server auth sessions
	CachedAuthInt.EndAllLocalServerAuthSessions();

	// Clear the 'ServerAuthRetries' lists
	ServerAuthRetries.Length = 0;
}

/**
 * Called by GameInfo when servertravel begins, to allow for online subsystem cleanup
 * NOTE: Worth keeping, in addition to NotifyGameEnding, as it is triggered earlier and can check for seamless travel
 *
 * @param bSeamless	whether or not travel is seamless
 */
function NotifyServerTravel(bool bSeamless)
{
	if (!bSeamless)
	{
		Cleanup();
	}
}

/**
 * Called by GameInfo when the game is ending (either through exit or non-seamless travel), to allow for online subsystem cleanup
 */
function NotifyGameEnding()
{
	local GameEngine Engine;

	Engine = GameEngine(Class'Engine'.static.GetEngine());

	// If the server is just switching level, do a normal cleanup
	// @todo JohnB: This way of distinguishing travel from exit is quite hacky (but works and is necessary); implement a better solution
	if (WorldInfo.NextURL != "" || Engine.TravelURL != "")
	{
		Cleanup();
	}
	// Otherwise, the game is exiting and NotifyExit may need to do special handling
	else
	{
		NotifyExit();
	}
}

/**
 * Called by GameInfo when the game is exiting (PreExit), to allow for online subsystem cleanup
 */
function NotifyExit()
{
	Cleanup(true);
}

/**
 * Cleanup any online subsystem references
 */
function Cleanup(optional bool bExit)
{
	if (CachedAuthInt != none)
	{
		ClearAuthDelegates(bExit);

		// If the game is exiting, end all auth sessions
		if (bExit)
		{
			// End all remote client auth sessions
			CachedAuthInt.EndAllRemoteClientAuthSessions();

			// End all local server auth sessions
			CachedAuthInt.EndAllLocalServerAuthSessions();

			// Clear the 'ServerAuthRetries' list
			ServerAuthRetries.Length = 0;
		}
	}

	CachedAuthInt = None;
	OnlineSub = None;
}


/**
 * Helper functions
 */

/**
 * Whether or not the specified player UID is awaiting authentication
 *
 * @param PlayerUID	The UID of the player
 * @return		Returns True if the UID is awaiting authentication, False otherwise
 */
function bool IsPendingAuth(UniqueNetId PlayerUID)
{
	local int i;

	for (i=0; i<ClientsPendingAuth.Length; ++i)
	{
		if (ClientsPendingAuth[i].ClientUID == PlayerUID)
		{
			return True;
		}
	}

	return False;
}


defaultproperties
{
	AdminClass=class'Engine.Admin'
	bAlwaysTick=True
}
