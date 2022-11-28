//=============================================================================
// UTTeamInfo.
// includes list of bots on team for multiplayer games
//
//=============================================================================

class UTTeamInfo extends TeamInfo;

var() UTRosterEntry DefaultRosterEntry;
var() export editinline array<UTRosterEntry> Roster;
var() class<UTPawn> AllowedTeamMembers[32];
var() byte TeamAlliance;
var int DesiredTeamSize;
var UTTeamAI AI;
var Color HudTeamColor;
var string TeamSymbolName;
var RepNotify Material TeamIcon; 
var Objective HomeBase;			// key objective associated with this team

var array<string> RosterNames;  // promoted from Team/DM rosters

replication
{
	// Variables the server should send to the client.
	reliable if ( bNetInitial && (Role==ROLE_Authority) )
		TeamIcon;
}

/* FIXMESTEVE
event ReplicatedEvent(string VarName)
{
	if ( VarName ~= "TeamIcon" )
	{
		TeamSymbolNotify();
	}
	else
	{
		Super.ReplicatedEvent(VarName);
	}
}

simulated function TeamSymbolNotify()
{
	local Actor A;
	
	ForEach AllActors(class'Actor', A)
		A.SetTeamSymbol(self);
}
*/

function int OverrideInitialBots(int N, UTTeamInfo T)
{
	return N;
}

function bool AllBotsSpawned()
{
	return false;
}

function Initialize(int TeamBots);

simulated function class<Pawn> NextLoadOut(class<Pawn> CurrentLoadout)
{
	local int i;
	local class<Pawn> Result;

	Result = AllowedTeamMembers[0];

	for ( i=0; i<ArrayCount(AllowedTeamMembers) - 1; i++ )
	{
		if ( AllowedTeamMembers[i] == CurrentLoadout )
		{
			if ( AllowedTeamMembers[i+1] != None )
				Result = AllowedTeamMembers[i+1];
			break;
		}
		else if ( AllowedTeamMembers[i] == None )
			break;
	}

	return Result;
}

function bool NeedsBotMoreThan(UTTeamInfo T)
{
	return ( (DesiredTeamSize - Size) > (T.DesiredTeamSize - T.Size) );
}

function UTRosterEntry ChooseBotClass(optional string botName)
{
    if (botName == "")
        return GetNextBot();

    return GetNamedBot(botName);
}

function UTRosterEntry GetRandomPlayer();

function bool AlreadyExistsEntry(string CharacterName, bool bNoRecursion)
{
	return false;
}

function AddRandomPlayer()
{
	local int j;

	j = Roster.Length;
	Roster.Length = Roster.Length + 1;
	Roster[j] = GetRandomPlayer();
	Roster[j].PrecacheRosterFor(self);
}

function UTRosterEntry GetNextBot()
{
	local int i;

	for ( i=0; i<Roster.Length; i++ )
		if ( !Roster[i].bTaken )
		{
			Roster[i].bTaken = true;
			return Roster[i];
		}
	i = Roster.Length;
	Roster.Length = Roster.Length + 1;
	Roster[i] = GetRandomPlayer();
	Roster[i].bTaken = true;
	return Roster[i];
}

function UTRosterEntry GetNamedBot(string botName)
{
    return GetNextBot();
}

/*FIXME - Merge back in Gameplay.U
function SetBotOrders(Bot NewBot, RosterEntry R)
{
    if( AI != None )
	    AI.SetBotOrders( NewBot, R );
}

function RemoveFromTeam(Controller Other)
{
	Super.RemoveFromTeam(Other);
	if ( AI != None )
		AI.RemoveFromTeam(Other);

//	for ( i=0; i<Roster.Length; i++ )
//	FIXME- clear bTaken for the roster entry

}
*/
defaultproperties
{
	HudTeamColor=(R=255,G=255,B=255,A=255)
	DesiredTeamSize=8
}

