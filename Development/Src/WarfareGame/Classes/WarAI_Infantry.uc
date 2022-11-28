class WarAI_Infantry extends WarAIController;

/** === COVER VARIABLES === */

/** Current cover node claimed by this AI */
var transient CoverNode PrimaryCover;

/** Last cover node claimed by this AI, useful for moving between links */
var transient CoverNode PreviousCover;

/** Current script assigned cover node, may or may not be the same as PrimaryCover */
var transient CoverNode AssignedCover;

/**
 * Called by team when a new member is added to our squad.
 */
function NotifyNewSquadMember(Controller newMember)
{
	local int playerIdx;
	// add them to the player list
	if (!IsInPlayerList(newMember,playerIdx))
	{
		AILog("Notify of squad member:"@newMember,'Squad');
		playerIdx = PlayerList.Length;
		PlayerList.Length = playerIdx + 1;
		PlayerList[playerIdx].Player = newMember;
		PlayerList[playerIdx].bFriendly = true;
	}
}

/**
 * Called by team when a squad member notices an enemy.
 */
function NotifyEnemy(WarAI_Infantry notifier,Controller newEnemy)
{
	local int playerIdx;
	local bool bNewEnemy;
	if (!IsInPlayerList(newEnemy,playerIdx))
	{
		AILog("Notified of enemy from squad member:"@notifier@newEnemy,'Squad');
		// add them to the list
		playerIdx = PlayerList.Length;
		PlayerList.Length = playerIdx + 1;
		PlayerList[playerIdx].Player = newEnemy;
		PlayerList[playerIdx].bFriendly = false;
		bNewEnemy = true;
	}
	OnSeeEnemy(playerIdx,bNewEnemy);
}

function EvaluateSituation()
{
	if (HasValidEnemy() ||
		CheckForEnemy())
	{
		GotoState('Combat');
	}
	else
	{
		GotoState('Idle');
	}
}

/**
 * Overridden to handle cover claims when directed to move to cover.
 */
function SetMoveGoal(Actor inGoalActor, optional bool inbInterruptable, optional bool inbReachable, optional Actor inMoveTarget)
{
	if (inGoalActor != None)
	{
		// check to see if moving to a cover node
		if (inGoalActor.IsA('CoverNode'))
		{
			// claim the new node
			ClaimCover(CoverNode(inGoalActor));
		}
		Super.SetMoveGoal(inGoalActor,inbInterruptable,inbReachable,inMoveTarget);
	}
}

/**
 * Returns the averaged location of this AI's squad.
 */
function vector GetSquadLocation()
{
	local WarPRI pri;
	local int idx,cnt;
	local array<Controller> members;
	local vector resultLoc;
	pri = GetPRI();
	if (pri.SquadName != 'None')
	{
		GetTeam().GetSquadMembers(pri.SquadName,members);
		for (idx = 0; idx < members.Length; idx++)
		{
			if (members[idx] != None &&
				members[idx].Pawn != None)
			{
				resultLoc += members[idx].Pawn.Location;
				cnt++;
			}
		}
		resultLoc = resultLoc/cnt;
	}
	else
	{
		if (Pawn != None)
		{
			resultLoc = Pawn.Location;
		}
		else
		{
			resultLoc = Location;
		}
	}
	return resultLoc;
}

/** === COVER FUNCTIONS === */

/**
 * Returns the best cover node within the specified distance that isn't already claimed.
 */
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
		anchorLoc = Pawn.Location;
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

/**
 * Figures out the best location for checking visiblity from the given cover node.
 * 
 * @param	chkNode - cover node to use as a base
 * 
 * @return	vector indicating new viewpoint to use for visiblity
 */
final function vector GetCoverViewpoint(CoverNode chkNode)
{
	local vector viewPt, x, y, z;
	local CylinderComponent cyl;
	if (chkNode != None)
	{
		// first determine the proper location + height
		cyl = CylinderComponent(chkNode.CollisionComponent);
		viewPt = chkNode.Location;
		viewPt.Z -= cyl.CollisionHeight;
		if (chkNode.CoverType == CT_Standing)
		{
			viewPt.Z += chkNode.StandHeight;
		}
		else
		if (chkNode.CoverType == CT_MidLevel)
		{
			viewPt.Z += chkNode.MidHeight;
		}
		else
		{
			viewPt.Z += chkNode.CrouchHeight;
		}
		GetAxes(chkNode.Rotation,x,y,z);
		if (chkNode.bLeanLeft)
		{
			viewPt += Y * -64.f;
		}
		else
		if (chkNode.bLeanRight)
		{
			viewPt += Y * 64.f;
		}
	}
	return viewPt;
}

/**
 * Returns true if we can fire at the specified player,
 * either directly or at their current cover position.
 */
final function bool CanFireAt(Controller chkPlayer,vector chkLoc)
{
	local WarPawn wfPawn;
	local Actor hitActor;
	local vector hitL, hitN;
	if (chkPlayer != None &&
		WarPawn(chkPlayer.Pawn) != None)
	{
		wfPawn = WarPawn(chkPlayer.Pawn);
		// first check direct visiblity
		if (LineOfSightTo(wfPawn,chkLoc))
		{
			return true;
		}
		// if that failed, check for visibility to the cover node
		else
		if (wfPawn.CoverType != CT_None)
		{
			// fire a trace to the pawn's location
			hitActor = Pawn.Trace(hitL,hitN,wfPawn.Location,chkLoc);
			// if we hit the target pawn, or
			// we hit the level, and it's nearby, or
			// we hit a cover node claimed by the target
			if (hitActor != None &&
				(hitActor == wfPawn ||
				 (hitActor == Level &&
				  VSize(hitL-Pawn.Location) < 256.f) ||
				 (hitActor.IsA('CoverNode') &&
				  CoverNode(hitActor).CoverClaim == chkPlayer)))
			{
				// then we can shoot at them
				return true;
			}
		}
	}
	// no luck
	return false;
}

/**
 * Returns a weight indicating the desirability of the given
 * cover node, or -1.f if this node should be ignored.
 */
function float RateCover(CoverNode inNode, float chkDist)
{
	local float weight, enemyDot;
	// rate against distance from player
	if (Enemy != None)
	{
		// rate against to fov of this cover towards the enemy
		enemyDot = vector(inNode.Rotation) dot Normal(Enemy.Location - inNode.Location);
		// if less than the min threshold then ignore entirely
		if (enemyDot < 0.1f)
		{
			weight = -1.f;
		}
		else
		{
			weight += enemyDot;
			// if within a min distance
			if (VSize(inNode.Location - Enemy.Location) <= GetCollisionRadius() * 4.f)
			{
				// then penalize
				weight -= 0.5f;
			}
			else
			{
				// then weight in partially favor the closer we are
				weight += (1.f - FClamp(VSize(inNode.Location - Enemy.Location)/1024.f,0.f,1.f)) * 0.65f;
			}
			// now check to see if we would have to pass the enemy to get to the point
			if (PointDistToLine(Enemy.Location, inNode.Location-Pawn.Location, Pawn.Location) < VSize(inNode.Location-Pawn.Location))
			{
				weight -= 0.2f;
			}
			// weight nodes favorably that we can fire at our enemy from
			if (CanFireAt(Enemy.Controller,GetCoverViewpoint(inNode)))
			{
				weight += 0.35f;
			}
		}
	}
	if (weight > 0.f)
	{
		// account for any manual weighting
		weight += inNode.AIRating/255.f;
		// initial rating based on node distance
		weight += (1.f - FClamp(VSize(inNode.Location - Pawn.Location)/chkDist,0.f,1.f)) * 1.3f;
		// if linked to current cover then favor it slightly
		if (PrimaryCover != None &&
			PrimaryCover.IsLinkedTo(inNode))
		{
			weight += 0.25f;
		}
		if (inNode.CoverType == CT_Standing)
		{
			weight += 0.2f;
		}
		else
		if (inNode.CoverType == CT_MidLevel)
		{
			weight += 0.1f;
		}
		if (inNode.CoverType != CT_Crouching)
		{
			if (inNode.bLeanLeft ||
				inNode.bLeanRight)
			{
				weight += 0.2f;
			}
			else
			{
				weight -= 0.5f;
			}
		}
		// prefer cover that has links to other cover, as that generally indicates more actual coverage
		if (inNode.RightNode != None ||
			inNode.LeftNode != None)
		{
			weight += 0.25f;
		}
		// favor cover on the same general z as our current location
		if (abs(inNode.Location.Z - Pawn.Location.Z) < GetCollisionHeight() * 3.f)
		{
			weight += 0.2f;
		}
		// avoid our previous cover if applicable
		// to discourage ping-pong behavior
		if (inNode == PreviousCover)
		{
			weight -= 0.25f;
		}
	}
	AILog("- node:"@inNode@"rated:"@weight,'Cover');
	return weight;
}

event SeePlayer(Pawn Seen)
{
	local int playerIdx;
	Super.SeePlayer(Seen);
	if (IsInPlayerList(Seen.Controller,playerIdx) &&
		!PlayerList[playerIdx].bFriendly)
	{
		GetTeam().NotifyEnemy(self,PlayerList[playerIdx].Player);
	}
}

/**
 * Determines whether or not our current cover claim is a
 * valid claim, and useful against our current enemy.
 * 
 * @return	true if the cover is considered valid for use
 */
function bool HasValidCover()
{
	return (PrimaryCover != None &&
			PrimaryCover.bEnabled &&
			PrimaryCover.CoverClaim == self &&
			!IsInvalidatedMoveGoal(PrimaryCover) &&
			(!HasValidEnemy() ||
			 (Normal(Enemy.Location-PrimaryCover.Location) dot vector(PrimaryCover.Rotation) > 0.1f)));
}

/**
 * Performs distance check to see if our pawn is currently at the
 * specified cover point.
 * 
 * @param	chkCover - defaults to PrimaryCover if none specified
 * 
 * @return	true if we are within min distance to chkCover
 */
final function bool IsAtCover(optional CoverNode chkCover)
{
	local WarPawn wfPawn;
	local float dist;
	dist = GetCollisionRadius() * 2.f;
	if (chkCover == None)
	{
		chkCover = PrimaryCover;
		// check to see if we're leaning out
		wfPawn = WarPawn(Pawn);
		if (wfPawn != None &&
			(wfPawn.CoverAction == CA_StepLeft ||
			 wfPawn.CoverAction == CA_StepRight))
		{
			dist += 64.f;
		}
	}
	return (chkCover != None &&
			chkCover.bEnabled &&
			VSize(chkCover.Location-Pawn.Location) < dist);
}

/**
 * Same as ClaimCover, except that the return value indicates
 * whether or not a new cover was claimed.
 */
final function bool ClaimNewCover(CoverNode newCover)
{
	local bool bClaimed;
	if (PrimaryCover != newCover)
	{
		bClaimed = ClaimCover(newCover);
	}
	return bClaimed;
}

/**
 * Asserts a claim on the specified cover node.  Also will
 * remove previous cover claim if it does not match the new
 * claim.
 * 
 * @param	newCover - new cover node to lay claim
 */
final function bool ClaimCover(CoverNode newCover)
{
	if (newCover != None &&
		newCover != PrimaryCover)
	{
		// revert previous claim if any
		if (PrimaryCover != None)
		{
			UnClaimCover();
		}
		AILog("Claimed new cover node:"@newCover,'Cover');
		PrimaryCover = newCover;
		PrimaryCover.Claim(self);
		WarPawn(Pawn).SetCoverAction(CA_Default);
		WarPawn(Pawn).SetCoverType(CT_None);
	}
	return HasValidCover();
}

/**
 * Revokes current cover claim.
 */
final function UnClaimCover()
{
	if (PrimaryCover != None)
	{
		AILog("Unclaimed cover node:"@PrimaryCover,'Cover');
		PreviousCover = PrimaryCover;
		PrimaryCover.UnClaim(self);
		PrimaryCover = None;
	}
	WarPawn(Pawn).SetCoverAction(CA_Default);
	WarPawn(Pawn).SetCoverType(CT_None);
}

/** === SCRIPTING FUNCTIONS === */

/**
 * Scripting hook for assigning cover for this AI.
 */
function OnAIAssignCover(SeqAct_AIAssignCover inAction)
{
	local array<Object> objVars;
	local int idx;
	local CoverNode node;
	for (idx = 0; idx < objVars.Length; idx++)
	{
		node = CoverNode(objVars[idx]);
		if (node != None &&
			node.bEnabled)
		{
			AssignedCover = node;
		}
	}
	if (AssignedCover != PrimaryCover &&
		AssignedCover != None)
	{
		ClaimCover(AssignedCover);
	}
}

state Idle
{
Begin:
	EvaluateWeapons();

LookAround:
	AILog("Looking around...",'Idle');
	if (CheckForEnemy())
	{
		EvaluateSituation();
	}
	else
	{
		DesiredRotation = Pawn.Rotation;
		DesiredRotation.Yaw += RandRange(-2048,2048);
		Sleep(1.f);
		Goto('LookAround');
	}

InvestigateNoise:
	Focus = RecentNoise[RecentNoise.Length-1].NoiseMaker;
	Goto('Begin');
}

/** === COMBAT STATES === */

/**
 * Base for all combat states.
 */
state Combat
{
	function OnSeeEnemy(int playerIdx, bool bNewEnemy)
	{
		// evaluate against our current one if new
		if (bNewEnemy)
		{
			AcquireEnemy(playerIdx);
		}
	}

Begin:
	if (!HasValidEnemy() &&
		!CheckForEnemy())
	{
		AILog("- invalid enemy, no enemies available",'Combat');
		EvaluateSituation();
	}
	else
	if (HasValidCover() ||
		ClaimCover(FindCover()))
	{
		AILog("- valid cover, transitioning to Combat_Cover",'Combat');
		GotoState('Combat_Cover');
	}
	else
	{
		AILog("- defaulting to Combat_Stationary",'Combat');
		GotoState('Combat_Stationary');
	}
}

/**
 * Combat state used when no cover is available, attempts to fire at an
 * enemy, and find new cover periodically.
 */
state Combat_Stationary extends Combat
{
Begin:
	if (!HasValidEnemy())
	{
		EvaluateSituation();
	}
	else
	{
		if (VSize(Enemy.Location-Pawn.Location) > 384.f &&
			ClaimNewCover(FindCover()))
		{
			AILog("- found new cover, aborting stationary attack",'Combat');
			Sleep(0.5f);
			GotoState('Combat_Cover');
		}
		else
		{
			Pawn.ShouldCrouch(true);
			// aim and shoot
			StartFiring();
			Sleep(RandRange(0.35f,2.f));
		}
	}
	Goto('Begin');
}

/**
 * Combat state used when attacking an enemy from cover.  Handles moving
 * pawn into position, setting various cover animation states and firing
 * at the enemy.
 */
state Combat_Cover extends Combat
{
	function bool StepAsideFor(Pawn chkPawn)
	{
		// don't step aside for players when in cover
		return false;
	}

	function NotifyCoverDisabled(CoverNode disabledNode)
	{
		if (disabledNode == PrimaryCover)
		{
			AILog("Cover claim disabled, looking for new cover",'Cover');
			PrimaryCover = FindCover();
			if (HasValidCover())
			{
				GotoState(GetStateName(),'Begin');
			}
		}
	}

	function NotifyCoverClaimRevoked(CoverNode revokedNode)
	{
		if (revokedNode == PrimaryCover)
		{
			AILog("Cover claim revoked, looking for new cover",'Cover');
			PrimaryCover = FindCover();
			if (HasValidCover())
			{
				// move to the new position
				GotoState(GetStateName(),'Begin');
			}
		}
	}

	/**
	 * Updates our pawn's covertype to match currently selected
	 * cover.
	 */
	function SetCoverType()
	{
		local WarPawn wfPawn;
		AILog("New cover type:"@PrimaryCover.CoverType,'Cover');
		wfPawn = WarPawn(Pawn);
		if (wfPawn != None)
		{
			wfPawn.SetCoverType(PrimaryCover.CoverType);
		}
	}

	function ResetCoverType()
	{
		local WarPawn wfPawn;
		bPreciseDestination = false;
		wfPawn = WarPawn(Pawn);
		if (wfPawn != None)
		{
			wfPawn.SetCoverType(CT_None);
			wfPawn.SetCoverAction(CA_Default);
		}
	}

	/**
	 * Looks at PrimaryCover and figures out the best available
	 * action to take, calling SetCoverAction() on our pawn.
	 */
	function bool SetCoverAction()
	{
		local vector viewPt, x, y, z;
		local CylinderComponent cyl;
		local WarPawn wfPawn;
		local bool bSetAction;
		if (PrimaryCover != None)
		{
			wfPawn = WarPawn(Pawn);
			cyl = CylinderComponent(PrimaryCover.CollisionComponent);
			// first determine the proper location + height
			viewPt = PrimaryCover.Location;
			viewPt.Z -= cyl.CollisionHeight;
			if (PrimaryCover.CoverType == CT_Standing)
			{
				viewPt.Z += PrimaryCover.StandHeight;
			}
			else
			if (PrimaryCover.CoverType == CT_MidLevel)
			{
				viewPt.Z += PrimaryCover.MidHeight;
			}
			else
			{
				viewPt.Z += PrimaryCover.CrouchHeight;
			}
			GetAxes(PrimaryCover.Rotation,x,y,z);
			// now determine what to do based on the cover type
			switch (PrimaryCover.CoverType)
			{
			case CT_Crouching:
				// no action, but pretend we did for firing purposes
				bSetAction = true;
				break;
			case CT_MidLevel:
				// check for a standing finre
				if (CanFireAt(Enemy.Controller,viewPt + Z * 32.f))
				{
					wfPawn.SetCoverAction(CA_PopUp);
					bSetAction = true;
					break;
				}
			case CT_Standing:
				// try from the left
				if (PrimaryCover.bLeanLeft &&
					CanFireAt(Enemy.Controller,viewPt + Y * -64.f))
				{
					wfPawn.SetCoverAction(CA_StepLeft);
					bSetAction = true;
				}
				else
				// and try from the right
				if (PrimaryCover.bLeanRight &&
					CanFireAt(Enemy.Controller,viewPt + Y * 64.f))
				{
					wfPawn.SetCoverAction(CA_StepRight);
					bSetAction = true;
				}
				break;
			default:
				AILog("Unknown cover type:"@PrimaryCover.CoverType,'Warning');
				break;
			}
		}
		else
		{
			ResetCoverAction();
		}
		if (bSetAction)
		{
			// disable cover snapping since it will interfere with root motion
			bPreciseDestination = false;
		}
		return bSetAction;
	}

	/**
	 * Resets the current cover action to default.
	 */
	function ResetCoverAction()
	{
		WarPawn(Pawn).SetCoverAction(CA_Default);
	}

	/**
	 * Returns true if we are currently under fire.
	 */
	function bool IsUnderFire()
	{
		return (UnderFireMeter > 0.5f);
	}

	function BeginState()
	{
		if (HasValidCover() &&
			IsAtCover())
		{
			// match cover stance
			SetCoverType();
		}
	}

	function EndState()
	{
		StopFiring();
		ResetCoverType();
		UnClaimCover();
		bForceDesiredRotation = false;
		bPreciseDestination = false;
	}

	function PausedState()
	{
		local WarPawn wfPawn;
		AILog("Paused state"@PrimaryCover@IsAtCover(),'Cover');
		bForceDesiredRotation = false;
		bPreciseDestination = false;
		// unset the cover action
		wfPawn = WarPawn(Pawn);
		if (wfPawn != None)
		{
			wfPawn.SetCoverType(CT_None);
			wfPawn.SetCoverAction(CA_Default);
		}
	}

	function ContinuedState()
	{
		AILog("Continued state"@PrimaryCover@IsAtCover(),'Cover');
		if (HasValidCover() &&
			IsAtCover())
		{
			// match cover stance
			SetCoverType();
		}
	}

	/**
	 * Overridden to use our current cover's viewpoint.
	 */
	function bool CheckForVisibleEnemy()
	{
		local int idx;
		local bool bAcquired;
		local vector viewPt;
		if (HasValidCover())
		{
			viewPt = GetCoverViewpoint(PrimaryCover);
			if (IsEnemyVisible() ||
				IsEnemyVisibleFrom(viewPt))
			{
				bAcquired = true;
			}
			else
			{
				for (idx = 0; idx < PlayerList.Length && !bAcquired; idx++)
				{
					if (PlayerList[idx].Player != None &&
						!PlayerList[idx].bFriendly &&
						(PlayerList[idx].bVisible ||
						 IsEnemyVisibleFrom(viewPt)))
					{
						bAcquired = AcquireEnemy(idx,true);
					}
				}
			}
		}
		else
		{
			bAcquired = Global.CheckForVisibleEnemy();
		}
		return bAcquired;
	}

Begin:
	if (!HasValidEnemy() &&
		!CheckForEnemy())
	{
		AILog("- invalid enemy, no enemies available",'Combat');
		EvaluateSituation();
	}
	if (!HasValidCover() &&
		!ClaimNewCover(FindCover()))
	{
		AILog("- invalid cover, no cover available",'Combat');
		EvaluateSituation();
	}
	while (HasValidCover() &&
		   HasValidEnemy())
	{
		// check to see if the enemy is in our face
		if (VSize(Enemy.Location-Pawn.Location) < 384.f &&
			(Normal(PrimaryCover.Location-Enemy.Location) dot vector(PrimaryCover.Rotation) < 0.f) &&
			IsEnemyVisibleFrom(Pawn.Location))
		{
			AILog("Enemy directly visible and within close range",'Combat');
			ResetCoverType();
			bForceDesiredRotation = false;
			Focus = Enemy;
			StartFiring();
			Sleep(RandRange(0.5f,1.5f));
		}
		else
		if (!IsAtCover())
		{
			// check for a linked cover
			if (PreviousCover != None &&
				IsAtCover(PreviousCover) &&
				(PreviousCover.LeftNode == PrimaryCover ||
				 PreviousCover.RightNode == PrimaryCover))
			{
				AILog("Moving between cover links",'Cover');
				Sleep(0.2f);
				Goto('MoveBetweenCover');
			}
			else
			{
				AILog("Moving to cover",'Cover');
				bForceDesiredRotation = false;
				ResetCoverType();
				SetMoveGoal(PrimaryCover,false);
				if (IsAtCover())
				{
					bPreciseDestination = true;
					Destination = PrimaryCover.Location;
				}
			}
		}
		else
		{
			// force rotation to match cover rotation
			bForceDesiredRotation = true;
			DesiredRotation = PrimaryCover.Rotation;
			// check to see if we should reload now
			if (CheckWeaponReload())
			{
				// wait until weapon is finished reloading
				StartReload();
				do
				{
					Sleep(0.25f);
				}
				until (!IsReloading());
			}
			// if we are currently under fire
			if (IsUnderFire())
			{
				AILog("+ Under fire, waiting behind cover",'Combat');
				// wait until no longer under fire
				do
				{
					Sleep(0.25f);
				}
				until (!IsUnderFire());
			}
			// step out from behind cover
			if (HasValidEnemy() &&
				SetCoverAction())
			{
				// initial delay
				Sleep(RandRange(0.5f,0.9f));
				AILog("+ Firing at enemy from cover",'Combat');
				// if they are currently behind cover (BTW only)
				if (IsEnemyBehindCover())
				{
					AILog("+- Enemy is behind cover, laying sporadic fire",'Combat');
					// then lay sporadic fire on their cover
					do
					{
						StartFiring();
						Sleep(RandRange(0.4f,0.8f));
						StopFiring();
						Sleep(RandRange(0.7f,1.1f));
					}
					until (IsUnderFire() || !IsEnemyBehindCover() || CheckWeaponReload());
				}
				else
				{
					AILog("+- Enemy isn't behind cover, unleashing",'Combat');
					// unload on them to push them back behind cover
					StartFiring();
					do
					{
						Sleep(0.4f);
					}
					until (IsUnderFire() || IsEnemyBehindCover() || CheckWeaponReload());
					StopFiring();
				}
				// if we're under fire, then jump back behind cover
				if (IsUnderFire() ||
					CheckWeaponReload())
				{
					AILog("+ Returning to cover",'Combat');
					ResetCoverAction();
					Sleep(0.6f);
					// re-enable cover snap, after animation has blended out
					bPreciseDestination = true;
					Destination = PrimaryCover.Location;
				}
			}
			else
			{
				AILog("+ Unable to fire at enemy, delaying",'Combat');
				// delay for a bit
				Sleep(0.35f);
				// and try again?
				if (FRand() > 0.65f)
				{
					// attempt to find new better cover
					ClaimNewCover(FindCover());
				}
			}
		}
	}
	StopFiring();
	Goto('Begin');

MoveBetweenCover:
	if (HasValidCover() &&
		PreviousCover != None)
	{
		SetCoverType();
		MoveToward(PrimaryCover);
		bPreciseDestination = true;
		Destination = PrimaryCover.Location;
	}
	Sleep(0.2f);
	Goto('Begin');
}

/** === DEAD STATE === */

state Dead
{
	/**
	 * Overridden to nuke any cover claims, etc.
	 */
	function BeginState()
	{
		UnClaimCover();
		Super.BeginState();
	}
}
