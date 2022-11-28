//=============================================================================
// TeamInfo.
//=============================================================================
class TeamInfo extends ReplicationInfo
	native
	nativereplication;

var localized string TeamName;
var int Size; //number of players on this team in the level
var float Score;
var int TeamIndex;

cpptext
{
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
}

replication
{
	// Variables the server should send to the client.
	reliable if( bNetDirty && (Role==ROLE_Authority) )
		Score;
	reliable if ( bNetInitial && (Role==ROLE_Authority) )
		TeamName, TeamIndex;
}

function bool AddToTeam( Controller Other )
{
	local Controller P;
	local bool bSuccess;

	// make sure loadout works for this team
	if ( Other == None )
	{
		log("Added none to team!!!");
		return false;
	}

	if (MessagingSpectator(Other) != None)
		return false;

	Size++;
	Other.PlayerReplicationInfo.Team = self;
	Other.PlayerReplicationInfo.NetUpdateTime = Level.TimeSeconds - 1;

	bSuccess = false;
	if ( Other.IsA('PlayerController') )
		Other.PlayerReplicationInfo.TeamID = 0;
	else
		Other.PlayerReplicationInfo.TeamID = 1;

	while ( !bSuccess )
	{
		bSuccess = true;
		for ( P=Level.ControllerList; P!=None; P=P.nextController )
            if ( P.bIsPlayer && (P != Other) 
				&& (P.PlayerReplicationInfo.Team == Other.PlayerReplicationInfo.Team) 
				&& (P.PlayerReplicationInfo.TeamId == Other.PlayerReplicationInfo.TeamId) )
				bSuccess = false;
		if ( !bSuccess )
			Other.PlayerReplicationInfo.TeamID = Other.PlayerReplicationInfo.TeamID + 1;
	}
	return true;
}

function RemoveFromTeam(Controller Other)
{
	Size--;
}

simulated function string GetHumanReadableName()
{
	if ( TeamName == Default.TeamName )
	{
		if ( TeamIndex < 4 )
			return class'Objective'.Default.ColorNames[TeamIndex];
		return TeamName@TeamIndex;
	}
	return TeamName;
}

defaultproperties
{
	NetUpdateFrequency=2
	TeamName="Team"
}
