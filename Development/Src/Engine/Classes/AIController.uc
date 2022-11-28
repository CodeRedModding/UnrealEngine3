//=============================================================================
// AIController, the base class of AI.
//
// Controllers are non-physical actors that can be attached to a pawn to control
// its actions.  AIControllers implement the artificial intelligence for the pawns they control.
//
// This is a built-in Unreal class and it shouldn't be modified.
//=============================================================================
class AIController extends Controller
	native(AI);

var		bool		bHunting;			// tells navigation code that pawn is hunting another pawn,
										//	so fall back to finding a path to a visible pathnode if none
										//	are reachable
var		bool		bAdjustFromWalls;	// auto-adjust around corners, with no hitwall notification for controller or pawn
										// if wall is hit during a MoveTo() or MoveToward() latent execution.

var     float		Skill;				// skill, scaled by game difficulty (add difficulty to this value)	

cpptext
{
	INT AcceptNearbyPath(AActor *goal);
	void AdjustFromWall(FVector HitNormal, AActor* HitActor);
	void SetAdjustLocation(FVector NewLoc);
}

event PreBeginPlay()
{
	Super.PreBeginPlay();
	if ( bDeleteMe )
		return;

	if ( Level.Game != None )
		Skill += Level.Game.GameDifficulty;
	Skill = FClamp(Skill, 0, 3);
}

/* Reset()
reset actor to initial state - used when restarting level without reloading.
*/
function Reset()
{
	bHunting = false;
	Super.Reset();
}

/* WeaponFireAgain()
Notification from weapon when it is ready to fire (either just finished firing,
or just finished coming up/reloading).
Returns true if weapon should fire.
If it returns false, can optionally set up a weapon change
*/
function bool WeaponFireAgain(float RefireRate, bool bFinishedFire)
{
	/*
	if ( Pawn.IsPressingFire() && (FRand() < RefireRate) )
	{
		BotFire( bFinishedFire );
		return true;
	}

	*/
	StopFiring();
	return false;
}
	
/* BotFire 
	Bot decides to fire
*/
function BotFire( bool bFinishedFire, optional byte FireModeNum )
{
	bFire = 1;
	Pawn.StartFire( FireModeNum );
}

/** 
 * list important AIController variables on canvas.  HUD will call DisplayDebug() on the current ViewTarget when
 * the ShowDebug exec is used
 *
 * @param	HUD		- HUD with canvas to draw on
 * @input	out_YL		- Height of the current font
 * @input	out_YPos	- Y position on Canvas. out_YPos += out_YL, gives position to draw text for next debug line.
 */
function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local int i;
	local string T;
	local Canvas Canvas;

	Canvas = HUD.Canvas;

	super.DisplayDebug(HUD, out_YL, out_YPos);

	if ( HUD.bShowAIDebug )
	{
		Canvas.DrawColor.B = 255;
		if ( (Pawn != None) && (MoveTarget != None) && Pawn.ReachedDestination(MoveTarget) )
		Canvas.DrawText("     Skill "$Skill$" NAVIGATION MoveTarget "$GetItemName(String(MoveTarget))$"(REACHED) MoveTimer "$MoveTimer, false);
		else
		Canvas.DrawText("     Skill "$Skill$" NAVIGATION MoveTarget "$GetItemName(String(MoveTarget))$" MoveTimer "$MoveTimer, false);
		out_YPos += out_YL;
		Canvas.SetPos(4,out_YPos);

		Canvas.DrawText("      Destination "$Destination$" Focus "$GetItemName(string(Focus))$" Preparing Move "$bPreparingMove, false);
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

function float AdjustDesireFor(PickupFactory P)
{
	return 0;
}

// AdjustView() called if Controller's pawn is viewtarget of a player
function AdjustView(float DeltaTime)
{
	local float TargetYaw, TargetPitch;
	local rotator OldViewRotation,ViewRotation;

	Super.AdjustView(DeltaTime);
	if( !Pawn.bUpdateEyeHeight )
		return;

	// update viewrotation
	ViewRotation = Rotation;
	OldViewRotation = Rotation;			

	if ( Enemy == None )
	{
		ViewRotation.Roll = 0;
		if ( DeltaTime < 0.2 )
		{
			OldViewRotation.Yaw = OldViewRotation.Yaw & 65535;
			OldViewRotation.Pitch = OldViewRotation.Pitch & 65535;
			TargetYaw = float(Rotation.Yaw & 65535);
			if ( Abs(TargetYaw - OldViewRotation.Yaw) > 32768 )
			{
				if ( TargetYaw < OldViewRotation.Yaw )
					TargetYaw += 65536;
				else
					TargetYaw -= 65536;
			}
			TargetYaw = float(OldViewRotation.Yaw) * (1 - 5 * DeltaTime) + TargetYaw * 5 * DeltaTime;
			ViewRotation.Yaw = int(TargetYaw);

			TargetPitch = float(Rotation.Pitch & 65535);
			if ( Abs(TargetPitch - OldViewRotation.Pitch) > 32768 )
			{
				if ( TargetPitch < OldViewRotation.Pitch )
					TargetPitch += 65536;
				else
					TargetPitch -= 65536;
			}
			TargetPitch = float(OldViewRotation.Pitch) * (1 - 5 * DeltaTime) + TargetPitch * 5 * DeltaTime;
			ViewRotation.Pitch = int(TargetPitch);
			SetRotation(ViewRotation);
		}
	}
}

function SetOrders(name NewOrders, Controller OrderGiver);

function actor GetOrderObject()
{
	return None;
}

function name GetOrders()
{
	return 'None';
}

/* PrepareForMove()
Give controller a chance to prepare for a move along the navigation network, from
Anchor (current node) to Goal, given the reachspec for that movement.

Called if the reachspec doesn't support the pawn's current configuration.
By default, the pawn will crouch when it hits an actual obstruction. However,
Pawns with complex behaviors for setting up their smaller collision may want
to call that behavior from here
*/
event PrepareForMove(NavigationPoint Goal, ReachSpec Path);

function bool PriorityObjective()
{
	return false;
}

function Startle(Actor A);

event SetTeam(int inTeamIdx)
{
	Level.Game.ChangeTeam(self,inTeamIdx,true);
}

defaultproperties
{
	 bAdjustFromWalls=true
     bCanOpenDoors=true
	 bCanDoSpecial=true
	 MinHitWall=-0.5f
}