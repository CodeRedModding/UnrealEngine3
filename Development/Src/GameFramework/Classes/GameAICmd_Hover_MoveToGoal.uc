/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

class GameAICmd_Hover_MoveToGoal extends GameAICommand;

var transient Actor Path, Find;
var transient Actor Goal;
var float	Radius;
var transient bool    bWasFiring;

var float	DesiredHoverHeight;
var transient float	CurrentHoverHeight;

var float	SubGoalReachDist;

/** how close to get to the enemy (only valid of bCompleteMove is TRUE) */
var float GoalDistance;

/** current vector destination */
var transient vector MoveVectDest;

var transient ReachSpec CurrentSpec;
/** GoW global macros */

/** Simple constructor that pushes a new instance of the command for the AI */
static function bool MoveToGoal( GameAIController AI, Actor InGoal, float InGoalDistance, float InHoverHeight )
{
	local GameAICmd_Hover_MoveToGoal Cmd;

	if( AI != None && AI.Pawn != None && AI.Pawn.bCanFly)
	{
		Cmd = new(AI) class'GameAICmd_Hover_MoveToGoal';
		if( Cmd != None )
		{
			Cmd.GoalDistance = InGoalDistance;
			Cmd.Goal = InGoal;
			Cmd.DesiredHoverHeight = InHoverHeight;
			Cmd.CurrentHoverHeight = InHoverHeight;
			AI.PushCommand( Cmd );
			return TRUE;
		}
	}

	return FALSE;
}


function Pushed()
{
	Super.Pushed();

	GotoState('Moving');
}

function bool HandlePathObstruction(Actor BlockedBy)
{

	// ! intentionally does not pass on to children

	MoveTimer = -1.f; // kills latent moveto's
	GotoState('MoveDown');
	
	return false;
}

state MoveDown `DEBUGSTATE
{
	function vector GetMoveDest()
	{
		local float Height,RadRad;
		local navigationPoint PtForHeight;
		local vector Dest,HitLocation,HitNormal;
		local actor HitActor;
		
		if(Pawn.Anchor != none)
		{
			PtForHeight = Pawn.Anchor;
		}
		else if(RouteCache.Length > 0 && RouteCache[0] != none)
		{
			PtForHeight = RouteCache[0];
		}

		if(PtForHeight != none)
		{
			PtForHeight.GetBoundingCylinder(RadRad,Height);
			CurrentHoverHeight = Max(0.f, Height - (Pawn.GetCollisionHeight()*0.5f));
			Dest = PtForHeight.Location;
			Dest.z = PtForHeight.Location.Z + CurrentHoverHeight;
		}
		else
		{
			// do a linecheck down to find the ground
			HitActor = Trace(HitLocation,HitNormal,Pawn.Location + vect(0,0,-4096.f),Pawn.Location);
			if(HitActor != none)
			{
				Dest = HitLocation;
				Dest.Z += Pawn.GetCollisionHeight() * 1.5f;
			}
			else
			{
				`AILog(GetFuncName()@"Could not find good hover height!");
				Dest = Pawn.Location;
			}
		}

		return Dest;
	}
Begin:
	MoveTo(GetMoveDest());
	Sleep(1.0f);
	GotoState('Moving');

};
state Moving `DEBUGSTATE
{

	final function bool ReachedDest(Actor Dest)
	{
		local float latDistSq;
		local float VertDist;
		latDistSq = VSizeSq2D(Pawn.Location - Dest.Location);
		//@TONKS_TEMP
		`AILog("LatDist:"@sqrt(latDistSq));
		if(latDistSq < SubGoalReachDist * SubGoalReachDist)
		{
			VertDist = abs(Pawn.Location.Z - Dest.location.Z);
			//@TONKS_TEMP
			`AILog("VertDist:"@VertDist);
			if(VertDist < max(SubGoalReachDist,CurrentHoverHeight+(Pawn.GetCollisionHeight()*2)) )
			{
				return true;
			}
		}

		return false;
	}

	final protected function bool PopNextNode( out vector Dest )
	{
		while( 	RouteCache.Length > 0 &&
			RouteCache[0] != None )
		{
			if( ReachedDest( RouteCache[0] ) )
			{

				//debug
				`AILog( "Reached route cache 0:"@RouteCache[0] );


				// MAKE SURE ANCHOR IS UPDATED -- this is cause of NO CURRENT PATH bug
				Pawn.SetAnchor( RouteCache[0] );

				//debug
				`AILog( "Remove from route:"@RouteCache[0], 'Move' );

				RouteCache_RemoveIndex( 0 );

				// reset hoverheight since we just arrived at a subgoal
				CurrentHoverHeight = DesiredHoverHeight;
			}
			else
			{
				//debug
				`AILog( "Did NOT reach route cache 0:"@RouteCache[0] );

				break;
			}
		}

		if( RouteCache.Length < 1 )
		{
			return false;
		}

		CurrentSpec = Pawn.Anchor.GetReachSpecTo(RouteCache[0]);
		Dest = RouteCache[0].Location;
		return true;
	}
Begin:
	`AILog("BEGIN TAG"@GetSTatename());

	Find	= Goal;
	Radius	= Pawn.GetCollisionRadius() + Enemy.GetCollisionRadius();
	if( IsEnemyBasedOnInterpActor( Enemy ) == TRUE )
	{
		Find	= Enemy.Base;
		Radius	= 0.f;
	}
	Radius = FMax(Radius, GoalDistance);

	if( ActorReachable(Find) )
	{
		MoveVectDest = Find.Location;
		MoveVectDest.Z += CurrentHoverHeight;
		`AILog("Moving directly to "$Find);
		MoveTo(MoveVectDest,Enemy);
	}
	else
	{
		//FIXME: Navmesh
/*		// Try to find path to enemy
		Path = GeneratePathTo( Find,GoalDistance, TRUE );

		// If no path available
		if( Path == None )
		{
			`AILog("Could not find path to enemy!!!");
			GotoState( 'DelayFailure' );
		}
		else
		{
			//debug
			`AILog( "Found path toward enemy..."@Find@Path, 'Move' );

			
			while(PopNextNode(MoveVectDest))
			{
				MoveVectDest.Z += CurrentHoverHeight;
				if(CurrentHoverHeight > CurrentSpec.CollisionHeight && !FastTrace(MoveVectDest,Pawn.Location,pawn.GetCollisionExtent()))
				{
					`AILog("Could not trace to next position, trying to move down...");
					GotoState('MoveDown');
				}
				else
				{
					`AILog("Moving to "$MoveVectDest);
					MoveTo(MoveVectDest,Enemy);
				}
				
			}			
		}
		*/
	}

	GotoState('DelaySuccess');
}


/** Allows subclasses to determine if our enemy is based on an interp actor or not **/
function bool IsEnemyBasedOnInterpActor( Pawn InEnemy )
{
	return FALSE;
}



defaultproperties
{
	SubGoalReachDist=768.f
}
