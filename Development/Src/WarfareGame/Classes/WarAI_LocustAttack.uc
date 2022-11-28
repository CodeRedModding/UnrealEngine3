class WarAI_LocustAttack extends WarAI_Locust;

/** Is this locust considered the suppressor of his group? */
var bool bSuppressor;

/** Is this locust considered the leaper of his group? */
var bool bLeaper;

/**
 * Figures out the higher level role of this AI, basically
 * to figure out which state to jump into.
 */
function DetermineRole()
{
	local int idx;
	local WarAI_LocustAttack friendly;
	local bool bFoundSuppressor;
	local array<Controller> members;
	bSuppressor = false;
	bLeaper = false;
	GetTeam().GetSquadMembers(GetPRI().SquadName,members);
	// can only be suppressor if long range
	if (IsLongRange())
	{
		// find all the nearby squad mates, looking for someone else already suppressing
		for (idx = 0; idx < members.Length && !bFoundSuppressor; idx++)
		{
			friendly = WarAI_LocustAttack(members[idx]);
			// check to see if they are a suppressor
			if (friendly != None &&
				friendly.bSuppressor)
			{
				bFoundSuppressor = friendly.IsLongRange();
			}
		}
		if (!bFoundSuppressor)
		{
			AILog("Becoming suppressor",'Combat');
			// we are now a suppressor
			bSuppressor = true;
		}
	}
}

/**
 * First determine suppressor/runner role, then jump into
 * cover.
 */
function EvaluateSituation()
{
	if (HasValidEnemy() ||
		CheckForEnemy())
	{
		DetermineRole();
		if (bSuppressor)
		{
			GotoState('Combat_Suppressor');
		}
		else
		{
			GotoState('Combat_Runner');
		}
	}
	else
	{
		GotoState('Idle');
	}
}

/**
 * While squad not within medium range of enemy, take
 * cover, fire at enemies.  Once squad in range, move
 * to medium range.
 */
state Combat_Suppressor extends Combat_Cover
{
	function bool CheckWeaponReload(optional float reloadPct)
	{
		if (reloadPct == 0.f)
		{
			reloadPct = 1.f;
		}
		return Super.CheckWeaponReload(reloadPct);
	}

	function bool IsUnderFire()
	{
		return (UnderFireMeter > 2.f);
	}

	/**
	 * Returns true if the squad is within medium range to enemies,
	 * so that the suppressor can move up.
	 */
	function bool IsSquadInMediumRange()
	{
		local array<Controller> members;
		local WarAI_Locust locust;
		local int idx;
		GetTeam().GetSquadMembers(GetPRI().SquadName,members);
		for (idx = 0; idx < members.Length; idx++)
		{
			locust = WarAI_Locust(members[idx]);
			if (locust != None)
			{
				if (locust.IsLongRange())
				{
					return false;
				}
			}
		}
		return true;
	}

Begin:
	if (IsSquadInMediumRange())
	{
		AILog("Squad in range, giving up suppressor role",'Cover');
		bSuppressor = false;
		GotoState('Combat_Runner');
	}
	else
	{
		if (HasValidCover() ||
			ClaimCover(FindCover()))
		{
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
				if (CheckWeaponReload(0.9f))
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
					// if enemy is behind cover, try to find one that isn't
					if (IsEnemyBehindCover())
					{
						CheckForUnCoveredEnemy();
					}
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
						Pawn.SetLocation(PrimaryCover.Location);
					}
				}
				else
				{
					AILog("+ Unable to fire at enemy, delaying",'Combat');
					// delay for a bit
					Sleep(0.35f);
				}
			}
		}
	}
	Goto('Begin');
}

state Combat_Runner extends Combat_Cover
{
	function float RateCover(CoverNode inNode, float chkDist)
	{
		local float weight;
		local Pawn nearEnemy;
		local float enemyDist;
		nearEnemy = GetNearestEnemy();
		if (nearEnemy != None)
		{
			enemyDist = VSize(nearEnemy.Location-inNode.Location);
			if (enemyDist <= CombatRange_Long)
			{
				if (enemyDist >= CombatRange_Medium)
				{
					weight = 0.25f;
				}
			}
			else
			{
				// give it a slight weight so that it is at least considered
				weight = 0.01f;
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
}

state Combat_Charger extends Combat
{
}

state Combat_Lone extends COmbat_Cover
{
}

function GetDebugInfo(out array<string> debugInfo)
{
	local float enemyDist;
	Super.GetDebugInfo(debugInfo);
	if (bSuppressor)
	{
		debugInfo[debugInfo.Length] = "*Suppressor*";
	}
	if (bLeaper)
	{
		debugInfo[debugInfo.Length] = "*Leaper*";
	}
	if (GetNearestEnemy(enemyDist) != None)
	{
		debugInfo[debugInfo.Length] = "E.Dist:"@enemyDist;
	}
}

defaultproperties
{
	CombatRange_Long=1536.f
	CombatRange_Medium=512.f
}
