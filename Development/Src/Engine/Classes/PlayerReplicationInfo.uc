//=============================================================================
// PlayerReplicationInfo.
//=============================================================================
class PlayerReplicationInfo extends ReplicationInfo
	native nativereplication;

var float				Score;			// Player's current score.
var float				Deaths;			// Number of player's deaths.
var private CarriedObject		HasFlag;
var byte				Ping;
var Actor				PlayerLocationHint;
var int					NumLives;

var repnotify string	PlayerName;		// Player name, or blank if none.
var string				OldName, PreviousName;
var int					PlayerID;		// Unique id number.
var RepNotify TeamInfo	Team;			// Player Team
var int					TeamID;			// Player position in team.

var bool				bAdmin;				// Player logged in as Administrator
var bool				bIsFemale;
var bool				bIsSpectator;
var bool				bOnlySpectator;
var bool				bWaitingPlayer;
var bool				bReadyToPlay;
var bool				bOutOfLives;
var bool				bBot;
var bool				bReceivedPing;			
var bool				bHasFlag;			
var	bool				bHasBeenWelcomed;	// client side flag - whether this player has been welcomed or not

var byte				PacketLoss;

// Time elapsed.
var int					StartTime;

var localized String	StringDead;
var localized String    StringSpectating;
var localized String	StringUnknown;

var int					GoalsScored;		// not replicated - used on server side only
var int					Kills;				// not replicated

var class<GameMessage>	GameMessageClass;

cpptext
{
	// Constructors.
	APlayerReplicationInfo() {}

	// AActor interface.
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
}

replication
{
	// Things the server should send to the client.
	reliable if ( bNetDirty && (Role == Role_Authority) )
		Score, Deaths, bHasFlag, PlayerLocationHint,
		PlayerName, Team, TeamID, bIsFemale, bAdmin, 
		bIsSpectator, bOnlySpectator, bWaitingPlayer, bReadyToPlay,
		StartTime, bOutOfLives;
	reliable if ( bNetDirty && (Role == Role_Authority) && !bNetOwner )
		PacketLoss, Ping;
	reliable if ( bNetInitial && (Role == Role_Authority) )
		PlayerID, bBot;
}

event PostBeginPlay()
{
	if ( Role < ROLE_Authority )
		return;
    if (AIController(Owner) != None)
        bBot = true;
	StartTime = Level.GRI.ElapsedTime;
	Timer();
	SetTimer(1.5 + FRand(), true);
}

simulated event PostNetBeginPlay()
{
	if ( Level.GRI != None )
		Level.GRI.AddPRI(self);
}

simulated event ReplicatedEvent(string VarName)
{
	local Pawn P;
	local PlayerController PC;
	local int WelcomeMessageNum;

	if ( VarName ~= "Team" )
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
	}
	else if ( VarName ~= "PlayerName" )
	{
		if ( Level.TimeSeconds < 2 )
			return;

		// new player or name change
		if ( bHasBeenWelcomed )
		{
			ForEach LocalPlayerControllers(PC)
				PC.ReceiveLocalizedMessage( GameMessageClass, 2, self );          
		}
		else
		{
			if ( bOnlySpectator )
				WelcomeMessageNum = 16;
			else
				WelcomeMessageNum = 1;

			bHasBeenWelcomed = true;
			PreviousName = PlayerName;
			ForEach LocalPlayerControllers(PC)
				PC.ReceiveLocalizedMessage( GameMessageClass, WelcomeMessageNum, self );          
		}
		OldName = PreviousName;
		PreviousName = PlayerName;
	}
}

simulated function Destroyed()
{
	local PlayerController PC;

	if ( Level.GRI != None )
		Level.GRI.RemovePRI(self);
 
	ForEach LocalPlayerControllers(PC)
		PC.ReceiveLocalizedMessage( GameMessageClass, 4, self);

    Super.Destroyed();
}

/* Reset() 
reset actor to initial state - used when restarting level without reloading.
*/
function Reset()
{
	Super.Reset();
	Score = 0;
	Deaths = 0;
	SetFlag(None);
	bReadyToPlay = false;
	NumLives = 0;
	bOutOfLives = false;
}

function SetFlag(CarriedObject NewFlag)
{
	HasFlag = NewFlag;
	bHasFlag = (HasFlag != None);
}

function CarriedObject GetFlag()
{
	return HasFlag;
}

simulated function string GetHumanReadableName()
{
	return PlayerName;
}

simulated function string GetLocationName()
{
	local String LocationString;

    if( PlayerLocationHint == None )
        return StringSpectating;
    
	LocationString = PlayerLocationHint.GetLocationStringFor(self);

	if ( LocationString == "" )
        return StringUnknown;

	return LocationString;
}

function UpdatePlayerLocation()
{
    local Volume V, Best;
    local Pawn P;

    if( Controller(Owner) != None )
        P = Controller(Owner).Pawn;
    
    if( P == None )
	{
        PlayerLocationHint = None;
        return;
    }

    foreach P.TouchingActors( class'Volume', V )
    {
        if( V.LocationName == "" ) 
            continue;
        
        if( (Best != None) && (V.LocationPriority <= Best.LocationPriority) )
            continue;
            
        if( V.Encompasses(P) )
            Best = V;
	}
	if ( Best != None )
		PlayerLocationHint = Best;
	else
		PlayerLocationHint = P.Region.Zone;
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
    HUD.Canvas.Font	= class'Engine'.Default.SmallFont;
	HUD.Canvas.StrLen(PlayerName, XS, YS);
	HUD.Canvas.DrawText(PlayerName);
	HUD.Canvas.SetPos(4 + XS, YPos);
	HUD.Canvas.Font	= class'Engine'.Default.TinyFont;
	HUD.Canvas.SetDrawColor(255,255,0);
	if ( HasFlag != None )
		HUD.Canvas.DrawText("   has flag "$HasFlag);

	YPos += YS;
	HUD.Canvas.SetPos(4, YPos);

	if ( !bBot )
	{
		HUD.Canvas.SetDrawColor(128,128,255);
		HUD.Canvas.DrawText("      bIsSpec:"@bIsSpectator@"OnlySpec:"$bOnlySpectator@"Waiting:"$bWaitingPlayer@"Ready:"$bReadyToPlay@"OutOfLives:"$bOutOfLives);
		YPos += YL;
		HUD.Canvas.SetPos(4, YPos);
	}
}

function Timer()
{
    local Controller C;

	UpdatePlayerLocation();
	SetTimer(1.5 + FRand(), true);
	if( FRand() < 0.65 )
		return;

	if( !bBot )
	{
	    C = Controller(Owner);
		if ( !bReceivedPing )
			Ping = Min(int(0.25 * float(C.ConsoleCommand("GETPING"))),255);
	}
}

function SetPlayerName(string S)
{
	PlayerName = S;

    // ReplicatedEvent() won't get called by net code if standalone game
    if ( Level.NetMode == NM_Standalone )
	{
		ReplicatedEvent("PlayerName");
	}
	OldName = PlayerName;
}

function SetWaitingPlayer(bool B)
{
	bIsSpectator = B;	
	bWaitingPlayer = B;
}

defaultproperties
{
	RemoteRole=ROLE_SimulatedProxy
	bAlwaysRelevant=True
    StringSpectating="Spectating"
    StringUnknown="Unknown"
    StringDead="Dead"
    NetUpdateFrequency=1
	GameMessageClass=class'GameMessage'
}
