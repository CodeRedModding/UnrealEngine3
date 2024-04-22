//=============================================================================
// TeamInfo.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class TeamInfo extends ReplicationInfo
	native(ReplicationInfo)
	nativereplication;

var localized string TeamName;
var int Size; //number of players on this team in the level
var float Score;
var repnotify int TeamIndex;
var color TeamColor;

cpptext
{
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
}

replication
{
	// Variables the server should send to the client.
	if( bNetDirty && (Role==ROLE_Authority) )
		Score;
	if ( bNetInitial && (Role==ROLE_Authority) )
		TeamName, TeamIndex;
}

simulated event ReplicatedEvent(name VarName)
{
	//`log(GetFuncName()@`showvar(VarName));
	if (VarName == 'TeamIndex')
	{
		if (WorldInfo.GRI != None)
		{
			// register this TeamInfo instance now
			WorldInfo.GRI.SetTeam(TeamIndex, self);
		}
	}
	else
	{

		Super.ReplicatedEvent(VarName);
	}
}

simulated event Destroyed()
{
	local TeamInfo OtherTeam;

	Super.Destroyed();

	// see if there's another TeamInfo that should take our spot in the GRI
	// (this could happen after seamless travel as there may be a time during which both the old and new TeamInfos are around)
	if (WorldInfo.GRI != None)
	{
		foreach DynamicActors(class'TeamInfo', OtherTeam)
		{
			if (OtherTeam != self && OtherTeam.TeamIndex == TeamIndex)
			{
				WorldInfo.GRI.SetTeam(TeamIndex, OtherTeam);
				break;
			}
		}
	}
}

function bool AddToTeam( Controller Other )
{
	// make sure loadout works for this team
	if ( Other == None )
	{
		`log("Added none to team!!!");
		return false;
	}

	if (Other.PlayerReplicationInfo == None)
	{
		`Warn(Other @ "is missing PlayerReplicationInfo");
		ScriptTrace();
		return false;
	}

	Size++;
	Other.PlayerReplicationInfo.SetPlayerTeam(Self);
	return true;
}

function RemoveFromTeam(Controller Other)
{
	Size--;
	if ( Other != None && Other.PlayerReplicationInfo != None )
	{
		Other.PlayerReplicationInfo.SetPlayerTeam(None);
	}
}

simulated function string GetHumanReadableName()
{
	return TeamName;
}

simulated native function byte GetTeamNum();

defaultproperties
{
	TickGroup=TG_DuringAsyncWork

	TeamIndex=-1					// can't be zero, otherwise the property will not be replicated and the notify will not fire
	NetUpdateFrequency=2
	TeamColor=(r=255,g=64,b=64,a=255)
}
