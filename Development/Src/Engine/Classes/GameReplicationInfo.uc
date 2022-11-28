//=============================================================================
// GameReplicationInfo.
//=============================================================================
class GameReplicationInfo extends ReplicationInfo
	config(Game)
	native nativereplication;

var string GameName;						// Assigned by GameInfo.
var string GameClass;						// Assigned by GameInfo.
var bool bTeamGame;							// Assigned by GameInfo.
var bool bStopCountDown;
var bool bMatchHasBegun;
var int  RemainingTime, ElapsedTime, RemainingMinute;
var float SecondCount;
var int GoalScore;
var int TimeLimit;
var int MaxLives;

var TeamInfo Teams[2];

var() globalconfig string ServerName;		// Name of the server, i.e.: Bob's Server.
var() globalconfig string ShortName;		// Abbreviated name of server, i.e.: B's Serv (stupid example)
var() globalconfig string AdminName;		// Name of the server admin.
var() globalconfig string AdminEmail;		// Email address of the server admin.
var() globalconfig int	  ServerRegion;		// Region of the game server.

var() globalconfig string MessageOfTheDay;

var Actor Winner;			// set by gameinfo when game ends

var() array<PlayerReplicationInfo> PRIArray;

enum ECarriedObjectState
{
    COS_Home,
    COS_HeldFriendly,
    COS_HeldEnemy,
    COS_Down,
};
var ECarriedObjectState CarriedObjectState[2];

// stats
var int MatchID;

cpptext
{
	// Constructors.
	AGameReplicationInfo() {}

	// AActor interface.
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
}

replication
{
	reliable if ( bNetDirty && (Role == ROLE_Authority) )
		bStopCountDown, Winner, Teams, CarriedObjectState, bMatchHasBegun, MatchID; 

	reliable if ( !bNetInitial && bNetDirty && (Role == ROLE_Authority) )
		RemainingMinute;

	reliable if ( bNetInitial && (Role==ROLE_Authority) )
		GameName, GameClass, bTeamGame, 
		RemainingTime, ElapsedTime,MessageOfTheDay, 
		ServerName, ShortName, AdminName,
		AdminEmail, ServerRegion, GoalScore, MaxLives, TimeLimit; 
}

simulated function PostNetBeginPlay()
{
	local PlayerReplicationInfo PRI;
	
	Level.GRI = self;

	ForEach DynamicActors(class'PlayerReplicationInfo',PRI)
		AddPRI(PRI);
}

simulated function PostBeginPlay()
{
	if( Level.NetMode == NM_Client )
	{
		// clear variables so we don't display our own values if the server has them left blank 
		ServerName = "";
		AdminName = "";
		AdminEmail = "";
		MessageOfTheDay = "";
	}

	SecondCount = Level.TimeSeconds;
	SetTimer(Level.TimeDilation, true);
}

/* Reset() 
reset actor to initial state - used when restarting level without reloading.
*/
function Reset()
{
	Super.Reset();
	Winner = None;
}

simulated function Timer()
{
	if ( Level.NetMode == NM_Client )
	{
		ElapsedTime++;
		if ( RemainingMinute != 0 )
		{
			RemainingTime = RemainingMinute;
			RemainingMinute = 0;
		}
		if ( (RemainingTime > 0) && !bStopCountDown )
			RemainingTime--;
		SetTimer(Level.TimeDilation, true);
	}
}

/**
 * Checks to see if two actors are on the same team.
 * 
 * @return	true if they are, false if they aren't
 */
simulated function bool OnSameTeam(Actor A, Actor B)
{
	local byte ATeamIndex, BTeamIndex;

	if ( !bTeamGame || (A == None) || (B == None) )
		return false;

	ATeamIndex = A.GetTeamNum();
	if ( ATeamIndex == 255 )
		return false;

	BTeamIndex = B.GetTeamNum();
	if ( BTeamIndex == 255 )
		return false;

	return ( ATeamIndex == BTeamIndex );
}

simulated function PlayerReplicationInfo FindPlayerByID( int PlayerID )
{
    local int i;

    for( i=0; i<PRIArray.Length; i++ )
    {
        if( PRIArray[i].PlayerID == PlayerID )
            return PRIArray[i];
    }
    return None;
}


simulated function AddPRI(PlayerReplicationInfo PRI)
{
    PRIArray[PRIArray.Length] = PRI;
}

simulated function RemovePRI(PlayerReplicationInfo PRI)
{
    local int i;

    for (i=0; i<PRIArray.Length; i++)
    {
        if (PRIArray[i] == PRI)
            break;
    }

    if (i == PRIArray.Length)
    {
        log("GameReplicationInfo::RemovePRI() pri="$PRI$" not found.", 'Error');
        return;
    }

    PRIArray.Remove(i,1);
	}

simulated function GetPRIArray(out array<PlayerReplicationInfo> pris)
{
    local int i;
    local int num;

    pris.Remove(0, pris.Length);
    for (i=0; i<PRIArray.Length; i++)
    {
        if (PRIArray[i] != None)
            pris[num++] = PRIArray[i];
    }
}

defaultproperties
{
	CarriedObjectState[0]=COS_Home
	CarriedObjectState[1]=COS_Home
	bStopCountDown=true
	RemoteRole=ROLE_SimulatedProxy
	bAlwaysRelevant=True
	ServerName="Another Server"
	ShortName="Server"
	MessageOfTheDay=""
}
