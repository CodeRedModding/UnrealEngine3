//=============================================================================
// AIController, the base class of AI.
//
// Controllers are non-physical actors that can be attached to a pawn to control
// its actions.  AIControllers implement the artificial intelligence for the pawns they control.
//
//Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class AIController extends Controller
	native(AI);

/** auto-adjust around corners, with no hitwall notification for controller or pawn
	if wall is hit during a MoveTo() or MoveToward() latent execution. */
var		bool		bAdjustFromWalls;	

/** skill, scaled by game difficulty (add difficulty to this value) */
var     float		Skill;

/** Move target from last scripted action */
var Actor ScriptedMoveTarget;

/** Route from last scripted action; if valid, sets ScriptedMoveTarget with the points along the route */
var Route ScriptedRoute;

/** if true, we're following the scripted route in reverse */
var bool bReverseScriptedRoute;

/** if ScriptedRoute is valid, the index of the current point we're moving to */
var int ScriptedRouteIndex;

/** view focus from last scripted action */
var Actor ScriptedFocus;

cpptext
{
	INT AcceptNearbyPath(AActor *goal);
	void AdjustFromWall(FVector HitNormal, AActor* HitActor);
	virtual void SetAdjustLocation(FVector NewLoc,UBOOL bAdjust,UBOOL bOffsetFromBase=FALSE);
	virtual FVector DesiredDirection();
	
	/** Called when the AIController is destroyed via script */
	virtual void PostScriptDestroyed();
}

event PreBeginPlay()
{
	Super.PreBeginPlay();
	if ( bDeleteMe )
		return;

	if ( WorldInfo.Game != None )
		Skill += WorldInfo.Game.GameDifficulty;
	Skill = FClamp(Skill, 0, 3);
}

/* Reset()
reset actor to initial state - used when restarting level without reloading.
*/
function Reset()
{
	Super.Reset();
}

/**
 * list important AIController variables on canvas.  HUD will call DisplayDebug() on the current ViewTarget when
 * the ShowDebug exec is used
 *
 * @param	HUD		- HUD with canvas to draw on
 * @input	out_YL		- Height of the current font
 * @input	out_YPos	- Y position on Canvas. out_YPos += out_YL, gives position to draw text for next debug line.
 */
simulated function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local int i;
	local string T;
	local Canvas Canvas;

	Canvas = HUD.Canvas;

	super.DisplayDebug(HUD, out_YL, out_YPos);

	if (HUD.ShouldDisplayDebug('AI'))
	{
		Canvas.DrawColor.B = 255;
		if ( (Pawn != None) && (MoveTarget != None) && Pawn.ReachedDestination(MoveTarget) )
		Canvas.DrawText("     Skill "$Skill$" NAVIGATION MoveTarget "$GetItemName(String(MoveTarget))$"(REACHED) MoveTimer "$MoveTimer, false);
		else
		Canvas.DrawText("     Skill "$Skill$" NAVIGATION MoveTarget "$GetItemName(String(MoveTarget))$" MoveTimer "$MoveTimer, false);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);

		Canvas.DrawText("      Destination "$GetDestinationPosition()$" Focus "$GetItemName(string(Focus))$" Preparing Move "$bPreparingMove, false);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);

		Canvas.DrawText("     RouteGoal "$GetItemName(string(RouteGoal))$" RouteDist "$RouteDist, false);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);

		for ( i=0; i<RouteCache.Length; i++ )
		{
			if ( RouteCache[i] == None )
			{
				if ( i > 5 )
					T = T$"--"$GetItemName(string(RouteCache[i-1]));
				break;
			}
			else if ( i < 5 )
				T = T$GetItemName(string(RouteCache[i]))$"-";
		}

		Canvas.DrawText("     RouteCache: "$T, false);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);
	}
}

event SetTeam(int inTeamIdx)
{
	WorldInfo.Game.ChangeTeam(self,inTeamIdx,true);
}

simulated event GetPlayerViewPoint(out vector out_Location, out Rotator out_Rotation)
{
	// AI does things from the Pawn
	if (Pawn != None)
	{
		out_Location = Pawn.Location;
		out_Rotation = Pawn.Rotation;
	}
	else
	{
		Super.GetPlayerViewPoint(out_Location, out_Rotation);
	}
}

/**
 * Scripting hook to move this AI to a specific actor.
 */
function OnAIMoveToActor(SeqAct_AIMoveToActor Action)
{
	local Actor DestActor;
	local SeqVar_Object ObjVar;

	// abort any previous latent moves
	ClearLatentAction(class'SeqAct_AIMoveToActor',true,Action);
	// pick a destination
	DestActor = Action.PickDestination(Pawn);
	// if we found a valid destination
	if (DestActor != None)
	{
		// set the target and push our movement state
		ScriptedRoute = Route(DestActor);
		if (ScriptedRoute != None)
		{
			if (ScriptedRoute.RouteList.length == 0)
			{
				`warn("Invalid route with empty MoveList for scripted move");
			}
			else
			{
				ScriptedRouteIndex = 0;
				if (!IsInState('ScriptedRouteMove'))
				{
					PushState('ScriptedRouteMove');
				}
			}
		}
		else
		{
			ScriptedMoveTarget = DestActor;
			if (!IsInState('ScriptedMove'))
			{
				PushState('ScriptedMove');
			}
		}
		// set AI focus, if one was specified
		ScriptedFocus = None;
		foreach Action.LinkedVariables(class'SeqVar_Object', ObjVar, "Look At")
		{
			ScriptedFocus = Actor(ObjVar.GetObjectValue());
			if (ScriptedFocus != None)
			{
				break;
			}
		}
	}
	else
	{
		`warn("Invalid destination for scripted move");
	}
}

/**
 * Simple scripted movement state, attempts to pathfind to ScriptedMoveTarget and
 * returns execution to previous state upon either success/failure.
 */
state ScriptedMove
{
	event PoppedState()
	{
		if (ScriptedRoute == None)
		{
			// if we still have the move target, then finish the latent move
			// otherwise consider it aborted
			ClearLatentAction(class'SeqAct_AIMoveToActor', (ScriptedMoveTarget == None));
		}
		// and clear the scripted move target
		ScriptedMoveTarget = None;
	}

	event PushedState()
	{
		if (Pawn != None)
		{
			// make sure the pawn physics are initialized
			Pawn.SetMovementPhysics();
		}
	}

Begin:
	// while we have a valid pawn and move target, and
	// we haven't reached the target yet
	while (Pawn != None &&
		   ScriptedMoveTarget != None &&
		   !Pawn.ReachedDestination(ScriptedMoveTarget))
	{
		// check to see if it is directly reachable
		if (ActorReachable(ScriptedMoveTarget))
		{
			// then move directly to the actor
			MoveToward(ScriptedMoveTarget, ScriptedFocus);
		}
		else
		{
			// attempt to find a path to the target
			MoveTarget = FindPathToward(ScriptedMoveTarget);
			if (MoveTarget != None)
			{
				// move to the first node on the path
				MoveToward(MoveTarget, ScriptedFocus);
			}
			else
			{
				// abort the move
				`warn("Failed to find path to"@ScriptedMoveTarget);
				ScriptedMoveTarget = None;
			}
		}
	}
	// return to the previous state
	PopState();
}

/** scripted route movement state, pushes ScriptedMove for each point along the route */
state ScriptedRouteMove
{
	event PoppedState()
	{
		// if we still have the move target, then finish the latent move
		// otherwise consider it aborted
		ClearLatentAction(class'SeqAct_AIMoveToActor', (ScriptedRoute == None));
		ScriptedRoute = None;
	}

Begin:
	while (Pawn != None && ScriptedRoute != None && ScriptedRouteIndex < ScriptedRoute.RouteList.length && ScriptedRouteIndex >= 0)
	{
		ScriptedMoveTarget = ScriptedRoute.RouteList[ScriptedRouteIndex].Actor;
		if (ScriptedMoveTarget != None)
		{
			PushState('ScriptedMove');
		}
		if (Pawn != None && Pawn.ReachedDestination(ScriptedRoute.RouteList[ScriptedRouteIndex].Actor))
		{
			if (bReverseScriptedRoute)
			{
				ScriptedRouteIndex--;
			}
			else
			{
				ScriptedRouteIndex++;
			}
		}
		else
		{
			`warn("Aborting scripted route");
			ScriptedRoute = None;
			PopState();
		}
	}

	if (Pawn != None && ScriptedRoute != None && ScriptedRoute.RouteList.length > 0)
	{
		switch (ScriptedRoute.RouteType)
		{
			case ERT_Linear:
				PopState();
				break;
			case ERT_Loop:
				bReverseScriptedRoute = !bReverseScriptedRoute;
				// advance index by one to get back into valid range
				if (bReverseScriptedRoute)
				{
					ScriptedRouteIndex--;
				}
				else
				{
					ScriptedRouteIndex++;
				}
				Goto('Begin');
				break;
			case ERT_Circle:
				ScriptedRouteIndex = 0;
				Goto('Begin');
				break;
			default:
				`warn("Unknown route type");
				ScriptedRoute = None;
				PopState();
				break;
		}
	}
	else
	{
		ScriptedRoute = None;
		PopState();
	}

	// should never get here
	`warn("Reached end of state execution");
	ScriptedRoute = None;
	PopState();
}

function NotifyWeaponFired(Weapon W, byte FireMode);
function NotifyWeaponFinishedFiring(Weapon W, byte FireMode);

function bool CanFireWeapon( Weapon Wpn, byte FireModeNum ) { return TRUE; }


defaultproperties
{
	 bAdjustFromWalls=true
	 bCanDoSpecial=true
	 MinHitWall=-0.5f
}
