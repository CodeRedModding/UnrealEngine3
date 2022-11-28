class WarTeamInfo extends TeamInfo
	native;

/** List of all players on this team */
var array<Controller> TeamMembers;

/**
 * Contains all information related to a specific squad
 * on this team.
 */
struct native SquadInfo
{
	/** Name of the squad */
	var Name SquadName;

	/** List of players in the squad */
	var array<Controller> Members;
};

/** List of all current squads */
var array<SquadInfo> Squads;

/** Debug file log */
var FileLog TeamLogFile;

function TeamLog(string logTxt)
{
	if (TeamLogFile == None)
	{
		TeamLogFile = Spawn(class'FileLog');
		TeamLogFile.OpenLog(Name,".ailog");
	}
	TeamLogFile.Logf(logTxt);
}

/**
 * Adds the player to the specified squad.
 */
function AddToSquad(Controller newMember, Name squadName)
{
	local int squadIdx, idx;
	local WarAI_Infantry infController;
	squadIdx = GetSquadIndex(squadName);
	TeamLog("Adding"@newMember@"to squad"@squadName@squadIdx);
	Squads[squadIdx].Members[Squads[squadIdx].Members.Length] = newMember;
	// notify other squad members of the new member
	infController = WarAI_Infantry(newMember);
	for (idx = 0; idx < Squads[squadIdx].Members.Length; idx++)
	{
		if (Squads[squadIdx].Members[idx] != newMember)
		{
			if (infController != None)
			{
				// notify the new member of existing squad members
				infController.NotifyNewSquadMember(Squads[squadIdx].Members[idx]);
			}
			if (Squads[squadIdx].Members[idx].IsA('WarAI_Infantry'))
			{
				WarAI_Infantry(Squads[squadIdx].Members[idx]).NotifyNewSquadMember(newMember);
			}
		}
	}
}

/**
 * Removes the player from their squad (if any).
 */
function RemoveFromSquad(Controller oldMember)
{
	local Name squadName;
	local int squadIdx, idx;
	local bool bFoundMember;
	// search for the owning squad
	squadName = GetMemberSquad(oldMember);
	if (squadName != 'None')
	{
		squadIdx = GetSquadIndex(squadName);
		for (idx = 0; idx < Squads[squadIdx].Members.Length && !bFoundMember; idx++)
		{
			if (Squads[squadIdx].Members[idx] == oldMember)
			{
				bFoundMember = true;
				// remove entry
				Squads[squadIdx].Members.Remove(idx,1);
			}
		}
	}
}

/**
 * Returns index to Squads[] based on squad name, creating
 * new entries as necessary.
 */
function int GetSquadIndex(Name squadName)
{
	local int idx, squadIdx;
	local bool bFoundSquad;
	if (squadName == 'None')
	{
		squadName = 'Default';
	}
	for (idx = 0; idx < Squads.Length && !bFoundSquad; idx++)
	{
		if (Squads[idx].SquadName == squadName)
		{
			bFoundSquad = true;
			squadIdx = idx;
		}
	}
	if (!bFoundSquad)
	{
		// create new entry
		squadIdx = Squads.Length;
		Squads.Length = squadIdx + 1;
		Squads[squadIdx].SquadName = squadName;
		TeamLog("Creating new squad"@squadName@squadIdx);
	}
	return squadIdx;
}

/**
 * Returns name of squad the player is currently in.
 */
function Name GetMemberSquad(Controller chkMember)
{
	local int idx, memIdx;
	local Name squadName;
	// search all squads for the member
	for (idx = 0; idx < Squads.Length && squadName == 'None'; idx++)
	{
		for (memIdx = 0; memIdx < Squads[idx].Members.Length && squadName == 'None'; memIdx++)
		{
			if (Squads[idx].Members[memIdx] == chkMember)
			{
				squadName = Squads[idx].SquadName;
			}
		}
	}
	return squadName;
}

/**
 * Copies out a list of squad members.
 */
function GetSquadMembers(Name squadName,out array<Controller> squadMembers)
{
	local int squadIdx, idx;
	squadMembers.Length = 0;
	if (squadName != 'None')
	{
		squadIdx = GetSquadIndex(squadName);
		squadMembers.Length = Squads[squadIdx].Members.Length;
		for (idx = 0; idx < Squads[squadIdx].Members.Length; idx++)
		{
			squadMembers[idx] = Squads[squadIdx].Members[idx];
		}
	}
}

function bool AddToTeam( Controller Other )
{
	local Controller P;
	local bool bSuccess;
	local WarPRI pri;
	if (Other != None &&
		MessagingSpectator(Other) == None)
	{
		TeamLog("Adding"@Other@"to team");
		Size++;
		Other.PlayerReplicationInfo.Team = self;
		Other.PlayerReplicationInfo.NetUpdateTime = Level.TimeSeconds - 1;
		bSuccess = false;
		if ( Other.IsA('PlayerController') )
		{
			Other.PlayerReplicationInfo.TeamID = 0;
		}
		else
		{
			Other.PlayerReplicationInfo.TeamID = 1;
		}
		// get a unique id for this player
		while ( !bSuccess )
		{
			bSuccess = true;
			for ( P=Level.ControllerList; P!=None; P=P.nextController )
			{
				if ( P.bIsPlayer && (P != Other) 
					&& (P.PlayerReplicationInfo.Team == Other.PlayerReplicationInfo.Team) 
					&& (P.PlayerReplicationInfo.TeamId == Other.PlayerReplicationInfo.TeamId) )
				{
					bSuccess = false;
				}
			}
			if ( !bSuccess )
			{
				Other.PlayerReplicationInfo.TeamID = Other.PlayerReplicationInfo.TeamID + 1;
			}
		}
		// add to members list
		TeamMembers[TeamMembers.Length] = Other;
		// figure out squad assignement
		pri = WarPRI(Other.PlayerReplicationInfo);
		if (pri != None &&
			pri.SquadName != 'None')
		{
			// add to specified squad
			AddToSquad(Other,pri.SquadName);
		}
		else
		{
			// add to default squad
			AddToSquad(Other,'Default');
		}
	}
	return bSuccess;
}

function RemoveFromTeam(Controller Other)
{
	local int idx;
	Size--;
	// remove from master member list
	for (idx = 0; idx < TeamMembers.Length; idx++)
	{
		if (TeamMembers[idx] == Other)
		{
			RemoveFromSquad(Other);
			TeamMembers.Remove(idx,1);
			break;
		}
	}
}

/**
 * Called by infantry AI upon noticing new potential enemy.
 */
function NotifyEnemy(WarAI_Infantry notifier, Controller newEnemy)
{
	local int squadIdx, idx;
	local WarAI_Infantry infController;
	local Name squadName;
	squadName = GetMemberSquad(notifier);
	if (squadName != 'None')
	{
		squadIdx = GetSquadIndex(squadName);
		for (idx = 0; idx < Squads[squadIdx].Members.Length; idx++)
		{
			infController = WarAI_Infantry(Squads[squadIdx].Members[idx]);
			if (infController != None)
			{
				infController.NotifyEnemy(notifier,newEnemy);
			}
		}
	}
}

function AdjustMorale(float adjustment)
{
	//@todo - support per squad morale
}

function NotifyKilled(Controller Killer, Controller Killed, Pawn KilledPawn)
{
	//@todo - morale adjustments, etc
}

defaultproperties
{
}
