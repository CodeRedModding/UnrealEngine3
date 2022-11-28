/**
 * WarGameSP
 * Warfare Game single player
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class WarGameSP extends WarGame
	config(Game);

/** Total number of teams to create */
var config byte NumTeams;

/** List of currently active teams */
var transient array<WarTeamInfo> Teams;

function Reset()
{
	super.Reset();
	GotoState('PendingMatch');
}

auto State PendingMatch
{
Begin:
	StartMatch();
}

/**
 * Handles creating the default teams.
 */
final function InitializeTeams()
{
	local int idx;
	// initialize all the teams
	for (idx = 0; idx < NumTeams; idx++)
	{
		Teams[idx] = Spawn(class'WarTeamInfo',self);
		Teams[idx].TeamIndex = idx;
	}
}

/**
 * Handles moving players from one team to another, or for the initial
 * team assignment.
 * 
 * @return	true if the team change was successful
 */
function bool ChangeTeam(Controller Other, int N, bool bNewTeam)
{
	// check to see if we need to initialize teams
	if (Teams.Length < NumTeams)
	{
		InitializeTeams();
	}
	if (Other != None)
	{
		// make sure a valid team is specified, defaulting to enemy team
		if (Other.IsA('PlayerController'))
		{
			N = 0;
		}
		else
		if (N < 0 ||
			N >= NumTeams)
		{
			N = 1;
		}
		//@temp - force friendly AI into god mode
		else
		if (N == 0)
		{
			Other.bGodMode = true;
		}
		// if not already on that team
		if (Other.PlayerReplicationInfo.Team == None ||
			Other.PlayerReplicationInfo.Team != Teams[N])
		{
			// remove from their current team
			if (Other.PlayerReplicationInfo.Team != None)
			{
				Other.PlayerReplicationInfo.Team.RemoveFromTeam(Other);
			}
			// and attempt to add them to their new team
			return Teams[N].AddToTeam(Other);
		}
		else
		{
			return true;
		}
	}
	return false;
}

/**
 * Overridden to prevent friendly fire damage.
 */
function ReduceDamage( out int Damage, pawn injured, pawn instigatedBy, vector HitLocation, out vector Momentum, class<DamageType> DamageType )
{
	if (injured != None &&
		instigatedBy != None &&
		Level.GRI.OnSameTeam(injured.Controller,instigatedBy.Controller))
	{
		Damage = 0;
	}
	else
	{
		Super.ReduceDamage(Damage,injured,instigatedBy,HitLocation,Momentum,damageType);
	}
}

/**
 * Overridden to notify teams of the death for the purposes of morale tracking.
 */
function NotifyKilled(Controller Killer, Controller Killed, Pawn KilledPawn )
{
	local int idx;
	for (idx = 0; idx < Teams.Length; idx++)
	{
		if (Teams[idx] != None)
		{
			Teams[idx].NotifyKilled(Killer,Killed,KilledPawn);
		}
	}
	Super.NotifyKilled(Killer,Killed,KilledPawn);
}

defaultproperties
{
	bTeamGame=true
	NumTeams=2
	GameName="Singe Player"
}
