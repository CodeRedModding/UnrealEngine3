class WarAI_GearFollow extends WarAI_Gear;


/** Current squad leader, potentially None */
var transient Pawn SquadLeader;

/**
 * Overridden to check to see if we've acquired a new squad leader.
 */
event SeePlayer(Pawn Seen)
{
	local int idx;
	Super.SeePlayer(Seen);
	// temp - check to see if it is a potential squad leader
	if (SquadLeader == None &&
		IsInPlayerList(Seen.Controller,idx) &&
		PlayerList[idx].bFriendly &&
		PlayerController(Seen.Controller) != None)
	{
		AILog("Acquired new squad leader:"@Seen,'Sensory');
		SquadLeader = Seen;
		OnAcquireSquadLeader();
	}
}

/**
 * Called once a new squad leader is acquired.
 */
function OnAcquireSquadLeader()
{
}

function CoverNode FindCover(optional float dist)
{
	local NavigationPoint nav;
	local CoverNode node, bestNode;
	local float rating, bestRating;
	local vector anchorLoc;
	if (AssignedCover != None &&
		AssignedCover.bEnabled)
	{
		bestNode = AssignedCover;
	}
	else
	{
		if (dist <= 0.f)
		{
			dist = 1024.f;
		}
		if (SquadLeader != None)
		{
			anchorLoc = SquadLeader.Location;
		}
		else
		{
			anchorLoc = Pawn.Location;
		}
		AILog("Searching for new cover, dist:"@dist$", current cover:"@PrimaryCover,'Cover');
		// set the current choice as best if available
		if (PrimaryCover != None &&
			IsAtCover())
		{
			bestNode = PrimaryCover;
			bestRating = RateCover(PrimaryCover,dist);
			// if we can't see our enemy from this point, reduce the rating
			if (!IsEnemyVisible())
			{
				bestRating *= 0.75f;
			}
		}
		// iterate through nav list, success on first unclaimed cover node in range
		for (nav = Level.NavigationPointList; nav != None; nav = nav.nextNavigationPoint)
		{
			if (VSize(nav.Location - anchorLoc) < dist)
			{
				node = CoverNode(nav);
				if (node != None &&
					!node.IsClaimed() &&
					!IsInvalidatedMoveGoal(node) &&
					node.bEnabled)
				{
					rating = RateCover(node,dist);
					if (bestNode == None ||
						rating > bestRating)
					{
						bestNode = node;
						bestRating = rating;
					}
				}
			}
		}
	}
	return bestNode;
}

state Idle
{
	function OnAcquireSquadLeader()
	{
		GotoState('Idle_Follow');
	}

	function BeginState()
	{
		Super.BeginState();
		if (SquadLeader != None)
		{
			GotoState('Idle_Follow');
		}
	}
}

state Idle_Follow extends Idle
{
	function OnAcquireSquadLeader()
	{
		// reset to beginning of state
		GotoState(GetStateName(),'Begin');
	}

	/**
	 * Picks a place to move to that is within range of the squad leader.
	 */
	function Actor PickFollowMoveGoal()
	{
		local Actor movePath;
		movePath = FindPathToward(SquadLeader);
		return movePath;
		//return WarTeamInfo(PlayerReplicationInfo.Team).GetAssignedPoint(self);
	}

Begin:
	if (SquadLeader == None)
	{
		GotoState('Idle');
	}
	else
	{
		Pawn.ShouldCrouch(false);
		if (VSize(SquadLeader.Location-Pawn.Location) > 384.f)
		{
			AILog("Moving to squad leader:"@SquadLeader$", dist:"@VSize(SquadLeader.Location-Pawn.Location),'Idle');
			SetMoveGoal(PickFollowMoveGoal(),true);
		}
		Sleep(RandRange(1.f,2.f));
		Goto('Begin');
	}
}

defaultproperties
{
}
