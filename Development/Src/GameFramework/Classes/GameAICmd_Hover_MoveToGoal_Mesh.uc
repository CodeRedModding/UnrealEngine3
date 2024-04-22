/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

class GameAICmd_Hover_MoveToGoal_Mesh extends GameAICommand;

var transient Actor Find;
var float	Radius;
var transient bool    bWasFiring;

var float	DesiredHoverHeight;
var transient float	CurrentHoverHeight;

var float	SubGoalReachDist;

/** how close to get to the enemy (only valid of bCompleteMove is TRUE) */
var transient float GoalDistance;

/** current vector destination */
var transient vector IntermediatePoint;
var transient vector LastMovePoint;
var transient int NumMovePointFails;
var int MaxMovePointFails;

var transient Vector FallbackDest;

var transient Actor MoveToActor;
/** location of MoveToActor last time we did pathfinding */
var BasedPosition LastMoveTargetPathLocation;


/** storage of initial desired move location */
var transient vector InitialFinalDestination;


/** is this AI on 'final approach' ( i.e. moving directly to it's end-goal destination )*/
var bool bFinalApproach;

/** TRUE when we are trying to get back on the mesh */
var bool bFallbackMoveToMesh;


/** Simple constructor that pushes a new instance of the command for the AI */
static function bool HoverToGoal( GameAIController AI, Actor InGoal, float InGoalDistance, float InHoverHeight )
{
	local GameAICmd_Hover_MoveToGoal_Mesh Cmd;

	if( AI != None && AI.Pawn != None && AI.Pawn.bCanFly)
	{
		Cmd = new(AI) class'GameAICmd_Hover_MoveToGoal_Mesh';
		if( Cmd != None )
		{
			Cmd.GoalDistance = InGoalDistance;
			Cmd.MoveToActor = InGoal;
			Cmd.InitialFinalDestination = InGoal.GetDestination(AI);
			Cmd.DesiredHoverHeight = InHoverHeight;
			Cmd.CurrentHoverHeight = InHoverHeight;
			AI.PushCommand( Cmd );
			return TRUE;
		}
	}

	return FALSE;
}

static function bool HoverToPoint( GameAIController AI, vector InPoint, float InGoalDistance, float InHoverHeight )
{
	local GameAICmd_Hover_MoveToGoal_Mesh Cmd;

	if( AI != None && AI.Pawn != None && AI.Pawn.bCanFly)
	{
		Cmd = new(AI) class'GameAICmd_Hover_MoveToGoal_Mesh';
		if( Cmd != None )
		{
			Cmd.GoalDistance = InGoalDistance;
			Cmd.MoveToActor = none;
			Cmd.InitialFinalDestination = InPoint;
			Cmd.DesiredHoverHeight = InHoverHeight;
			Cmd.CurrentHoverHeight = InHoverHeight;
			AI.PushCommand( Cmd );
			return TRUE;
		}
	}

	return FALSE;
}

static function bool HoverBackToMesh( GameAIController AI )
{
	local GameAICmd_Hover_MoveToGoal_Mesh Cmd;

	if( AI != None && AI.Pawn != None && AI.Pawn.bCanFly)
	{
		Cmd = new(AI) class'GameAICmd_Hover_MoveToGoal_Mesh';
		if( Cmd != None )
		{
			Cmd.bFallbackMoveToMesh=true;
			AI.PushCommand( Cmd );
			return TRUE;
		}
	}

	return FALSE;
}


function Pushed()
{
	Super.Pushed();

	if( bFallbackMoveToMesh )
	{
		`AILog("Going into breadcrumb fallback state to get back onto navmesh CurLoc:"@Pawn.Location);
		GotoState('Fallback_Breadcrumbs');
		return;
	}
	if( !NavigationHandle.ComputeValidFinalDestination(InitialFinalDestination) )
		
	{
		`AILog("ABORTING! Final destination"@InitialFinalDestination@"is not reachable! (ComputeValidFinalDestination returned FALSE)");
		GotoState('DelayFailure');
	}
	else if( !NavigationHandle.SetFinalDestination(InitialFinalDestination) )
	{
		`AILog("ABORTING! Final destination"@InitialFinalDestination@"is not reachable! (SetFinalDestination returned FALSE)");
		GotoState('DelayFailure');
	}
	else
	{
		GotoState('Moving');
	}
}

function Popped()
{
	Super.Popped();

	// Check for any latent move actions
	ClearLatentAction( class'SeqAct_AIMoveToActor', Status != 'Success' );

	// clear the route cache to make sure any intermediate claims are destroyed
	NavigationHandle.PathCache_Empty();

	// explicitly clear velocity/accel as they aren't necessarily
	// cleared by the physics code
	if( Pawn != None )
	{
		Pawn.ZeroMovementVariables();
		Pawn.DestinationOffset = 0.f;
	}

	ReachedMoveGoal();
}

function Tick( float DeltaTime )
{
	Super.Tick(DeltaTime);

	if( ShouldUpdateBreadCrumbs() )
	{
		NavigationHandle.UpdateBreadCrumbs(Pawn.Location);
	}


	NavigationHandle.DrawBreadCrumbs();
}


function bool HandlePathObstruction(Actor BlockedBy)
{

	// ! intentionally does not pass on to children

	MoveTimer = -1.f; // kills latent moveto's
	GotoState('Fallback_Breadcrumbs');
	
	return false;
}

state DelayFailure
{
	function bool HandlePathObstruction(Actor BlockedBy);
	
Begin:
	Sleep( 0.5f );

	Status = 'Failure';
	PopCommand( self );
}


state MoveDown `DEBUGSTATE
{
	
	// find a safe altitude to fly to 
	function vector GetMoveDest()
	{
		local vector HitLocation;
		local vector HitNormal;
		local vector Dest;
		local actor HitActor;
		
		// find the poly we're above and try to fly to that
		if(NavigationHandle.LineCheck(Pawn.Location, Pawn.Location + vect(0.f,0.f,-4096.f),vect(5,5,5),HitLocation,HitNormal))
		{
			// if we didn't hit the mesh for some reason, trace against geo
			HitActor = Trace(HitLocation,HitNormal,Pawn.Location + vect(0,0,-4096.f),Pawn.Location);
			if(HitActor == none)
			{
				`AILog(GetFuncName()@"Could not find surface to adjust height to!");
				return Pawn.Location;
			}		
		}

		Dest = HitLocation;
		Dest.Z += Pawn.GetCollisionHeight() * 1.5f;
		
		return Dest;
	}
Begin:
	`AILog("Moving down!");
	MoveTo(GetMoveDest());
	Sleep(1.0f);
	GotoState('Moving');

};


function ReEvaluatePath()
{
/*	`AILog(GetFuncName()@bReevaluatePath);

	if( HasReachedGoal() )
	{
		Status = 'Success';
		PopCommand(self);
	}
	else if( bReevaluatePath &&
		Pawn != None &&
		MovePointIsValid() )
	{
		//debug
		`AILog( "Move continued... Goal"@MoveGoal@"Anchor"@Pawn.Anchor);

		NavigationHandle.PathCache_Empty();
		`AILog(GetFuncName()@"disabling bReevaluatePath");
		bReevaluatePath = FALSE;

		// Restart the movement state
		GotoState( 'Moving', 'Begin' );
	}*/
}

/** Check if AI has successfully reached the given goal */
function bool HasReachedGoal()
{
	if( Pawn == None )
	{
		return TRUE;
	}

	`AILog(GetFuncName()@bFinalApproach@MoveToActor);
	if(bFinalApproach && MoveToActor != None)
	{
		return Pawn.ReachedDestination(MoveToActor);
	}

	if( BP2Vect( NavigationHandle.FinalDestination ) != vect(0,0,0) )
	{
		if( VSize(BP2Vect(NavigationHandle.FinalDestination)-Pawn.Location) < GoalDistance )
		{
			return TRUE;
		}
		return Pawn.ReachedPoint( BP2Vect(NavigationHandle.FinalDestination), None );
	}

    return FALSE;
}

function bool ShouldUpdateBreadCrumbs()
{
	return true;
}

state Moving `DEBUGSTATE
{
	final function float GetMoveDestinationOffset()
	{
		// return a negative destination offset so we get closer to our points (yay)
		if( bFinalApproach )
		{
			return GoalDistance;
		}
		else
		{
			return (SubGoalReachDist - Pawn.GetCollisionRadius());
		}
	}
CheckMove:
	//debug
	`AILog("CHECKMOVE TAG");

	if( HasReachedGoal() )
	{
		Goto( 'ReachedGoal' );
	}

Begin:
	`AILog("BEGIN TAG"@GetStateName());

	if( Enemy != none )
	{
		Radius	= Pawn.GetCollisionRadius() + Enemy.GetCollisionRadius();
	}
	Radius = FMax(Radius, GoalDistance);

	NavigationHandle.SetFinalDestination(InitialFinalDestination);

	if( NavigationHandle.PointReachable(BP2Vect(NavigationHandle.FinalDestination)) )
	{
		IntermediatePoint = BP2Vect(NavigationHandle.FinalDestination);
	}
	else
	{
		if( MoveToActor != None )
		{
			// update final dest in case target moved
			if( !NavigationHandle.SetFinalDestination(MoveToActor.GetDestination(Outer)) )
			{
				`AILog("ABORTING! Final destination"@InitialFinalDestination@"is not reachable! (SetFinalDestination returned FALSE)");
				Goto('FailedMove');
			}
		}

		if( !GeneratePathToLocation( BP2Vect(NavigationHandle.FinalDestination),GoalDistance, TRUE ) )
		{
			//debug
			`AILog("Couldn't generate path to location"@BP2Vect(NavigationHandle.FinalDestination)@"from"@Pawn.Location);

			//`AILog("Retrying with mega debug on");
			//NavigationHandle.bDebugConstraintsAndGoalEvals = TRUE;
			//NavigationHandle.bUltraVerbosePathDebugging = TRUE;
			//GeneratePathToLocation( BP2Vect(NavigationHandle.FinalDestination),GoalDistance, TRUE );

			GotoState( 'Fallback_Breadcrumbs' );
		}

		//debug
		`AILog( "Generated path..." );
		`AILog( "Found path!" @ `showvar(BP2Vect(NavigationHandle.FinalDestination)), 'Move' );

		if( !NavigationHandle.GetNextMoveLocation( IntermediatePoint, SubGoalReachDist ) )
		{
			//debug
			`AILog("Generated path, but couldn't retrieve next move location?");

			Goto( 'FailedMove' );
		}
	}

	if( MoveToActor != None )
	{
		Vect2BP(LastMoveTargetPathLocation, MoveToActor.GetDestination(Outer));
	}

	while( TRUE )
	{
		//debug
		`AILog( "Still moving to"@IntermediatePoint, 'Loop' );

		bFinalApproach = VSizeSq(IntermediatePoint - BP2Vect(NavigationHandle.FinalDestination)) < 1.0;
		
		`AILog("Calling MoveTo -- "@IntermediatePoint);
		// if on our final move to an Actor, send it in directly so we account for any movement it does
		if( bFinalApproach && MoveToActor != None )
		{
			`AILog(" - Final approach to" @ MoveToActor $ ", using MoveToward()");
			Vect2BP(LastMoveTargetPathLocation, MoveToActor.GetDestination(outer));
			NavigationHandle.SetFinalDestination(MoveToActor.GetDestination(outer));
			MoveToward(MoveToActor, Enemy, GetMoveDestinationOffset(), FALSE);
		}
		else
		{
			// if we weren't given a focus, default to looking where we're going
			if( Enemy == None )
			{
				SetFocalPoint(IntermediatePoint);
			}

			// use a negative offset so we get closer to our points!
			MoveTo(IntermediatePoint, Enemy, GetMoveDestinationOffset() );
		}
		`AILog("MoveTo Finished -- "@IntermediatePoint);

// 				if( bReevaluatePath )
// 				{
// 					ReEvaluatePath();
// 				}

		if( HasReachedGoal() )
		{
			Goto( 'CheckMove' );
		}
		// if we are moving towards a moving target, repath every time we successfully reach the next node
		// as that Pawn's movement may cause the best path to change
		else if( MoveToActor != None &&
			VSize(MoveToActor.GetDestination(Outer) - BP2Vect(LastMoveTargetPathLocation)) > 512.0 )
		{
			Vect2BP(LastMoveTargetPathLocation, MoveToActor.GetDestination(outer));
			`AILog("Repathing because target moved:" @ MoveToActor);
			Goto('CheckMove');
		}
		else if( !NavigationHandle.GetNextMoveLocation( IntermediatePoint, SubGoalReachDist ) )
		{
			`AILog( "Couldn't get next move location" );
			if (!bFinalApproach && ((MoveToActor != None) ? /*NavigationHandle.*/ActorReachable(MoveToActor) : /*NavigationHandle.*/PointReachable(BP2Vect(NavigationHandle.FinalDestination))))
			{
				`AILog("Target is directly reachable; try direct move");
				IntermediatePoint =((MoveToActor != None) ? MoveToActor.GetDestination(outer) : BP2Vect(NavigationHandle.FinalDestination));
				Sleep(RandRange(0.1,0.175));
			}
			else
			{
				Sleep(0.1f);
				`AILog("GetNextMoveLocation returned false, and finaldest is not directly reachable");

				Goto('FailedMove');
			}
		}
		else
		{
			if(VSize(IntermediatePoint-LastMovePoint) < (Pawn.GetCollisionRadius() * 0.1f) )
			{
				NumMovePointFails++;

//				DrawDebugBox(Pawn.Location, vect(2,2,2) * NumMovePointFails, 255, 255, 255, true );
				`AILog("WARNING: Got same move location... something's wrong?!"@`showvar(LastMovePoint)@`showvar(IntermediatePoint)@"Delta"@VSize(LastMovePoint-IntermediatePoint)@"ChkDist"@(Pawn.GetCollisionRadius() * 0.1f));
			}
			else
			{
				NumMovePointFails=0;
			}
			LastMovePoint = IntermediatePoint;

			if(NumMovePointFails >= MaxMovePointFails && MaxMovePointFails >= 0)
			{
				`AILog("ERROR: Got same move location 5x in a row.. something's wrong! bailing from this move");
				Goto('FailedMove');
			}
			else
			{
				//debug
				`AILog( "NextMove"@IntermediatePoint@`showvar(NumMovePointFails) );
			}
		}
	}

	//debug
	`AILog( "Reached end of move loop??" );

	Goto( 'CheckMove' );

FailedMove:

	`AILog( "Failed move.  Now ZeroMovementVariables" );

	MoveTo(Pawn.Location);
	Pawn.ZeroMovementVariables();
	GotoState('DelayFailure');

ReachedGoal:
	//debug
	`AILog("Reached move point:"@BP2Vect(NavigationHandle.FinalDestination)@VSize(Pawn.Location-BP2Vect(NavigationHandle.FinalDestination)));

	Status = 'Success';
	PopCommand( self );
}
/**
 * this state will follow our breadcrumbs backward until we are back in the mesh, and then transition back to moving, or go to other fallback states
 * if we run out of breadcrumbs and are not yet back in the mesh
 */
state Fallback_Breadcrumbs `DEBUGSTATE
{

	function bool ShouldUpdateBreadCrumbs()
	{
		return false;
	}

	function bool HandlePathObstruction(Actor BlockedBy)
	{
		Pawn.SetLocation(IntermediatePoint);
		MoveTimer=-1;
		GotoState('Fallback_Breadcrumbs','Begin');
		return true;
	}

Begin:
`AILog("trying to move back along breadcrumb path");
	if( NavigationHandle.GetNextBreadCrumb(IntermediatePoint) )
	{
		`AILog("Moving to breadcrumb pos:"$IntermediatePoint);
// 		GameAIOwner.DrawDebugLine(Pawn.Location,IntermediatePoint,255,0,0,TRUE);
// 		GameAIOwner.DrawDebugLine(IntermediatePoint+vect(0,0,100),IntermediatePoint,255,0,0,TRUE);

		MoveToDirectNonPathPos(IntermediatePoint);

		if( !NavigationHandle.IsAnchorInescapable() )
		{
			GotoState('Moving');
		}
		Sleep(0.1);
		Goto('Begin');
	}
	else if( !NavigationHandle.IsAnchorInescapable())
	{
		GotoState('Moving','Begin');
	}
	else
	{
		GotoState('Fallback_FindNearbyMeshPoint');
	}

}


state Fallback_FindNearbyMeshPoint `DEBUGSTATE
{
	function bool FindAPointWhereICanHoverTo( out Vector out_FallbackDest, float Inradius, optional float minRadius=0, optional float entityRadius = 32, optional bool bDirectOnly=true, optional int MaxPoints=-1,optional float ValidHitBoxSize)
	{
		local Vector Retval;
		local array<vector> poses;
//		local int i;
		local vector extent;
		local vector validhitbox;

		Extent.X = entityRadius;
		Extent.Y = entityRadius;
		Extent.Z = entityRadius;

		validhitbox = vect(1,1,1) * ValidHitBoxSize;
		NavigationHandle.GetValidPositionsForBox(Pawn.Location,Inradius,Extent,bDirectOnly,poses,MaxPoints,minRadius,validhitbox);
// 		for(i=0;i<Poses.length;++i)
// 		{
// 			DrawDebugStar(poses[i],55.f,255,255,0,TRUE);
// 			if(i < poses.length-1 )
// 			{
// 				DrawDebugLine(poses[i],poses[i+1],255,255,0,TRUE);
// 			}
// 		}

		if( poses.length > 0)
		{
			Retval = Poses[Rand(Poses.Length)];

			// if for some reason we have a 0,0,0 vect that is never going to be correct
			if( VSize(Retval) == 0.0f )
			{
				out_FallbackDest = vect(0,0,0);
				return FALSE;
			}

			`AILog( `showvar(Retval) );
			out_FallbackDest = Retval;
			return TRUE;
		}

		out_FallbackDest = vect(0,0,0);
		return FALSE;
	}


	function bool ShouldUpdateBreadCrumbs()
	{
		return false;
	}

Begin:

	`AILog( "Fallback! We now try MoveTo directly to a point that is avail to us" );

			
	if( !FindAPointWhereICanHoverTo( FallbackDest, 2048 ) )
	{
		GotoState('MoveDown');
	}
	else
	{
		MoveToDirectNonPathPos( FallbackDest,, SubGoalReachDist );
		Sleep( 0.5f );

		if( bFallbackMoveToMesh )
		{
			GotoState('DelaySuccess');
		}
		else
		{
			GotoState('Moving','Begin');
		}
	}
}




/** Allows subclasses to determine if our enemy is based on an interp actor or not **/
function bool IsEnemyBasedOnInterpActor( Pawn InEnemy )
{
	return FALSE;
}

event DrawDebug( HUD H, Name Category )
{
	Super.DrawDebug( H, Category );

	if(Category != 'Pathing')
	{
		return;
	}

	// BLUE
	DrawDebugLine( Pawn.Location, GetDestinationPosition(), 0, 0, 255 );
	// GREEN
	DrawDebugLine( Pawn.Location, BP2Vect(NavigationHandle.FinalDestination), 0, 255, 0 );

	NavigationHandle.DrawPathCache( vect(0,0,15) );
}




defaultproperties
{
	SubGoalReachDist=128.0
	MaxMovePointFails=5
}
