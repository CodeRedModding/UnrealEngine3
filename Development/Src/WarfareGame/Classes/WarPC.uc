//=============================================================================
// WarfarePController.
//=============================================================================
class WarPC extends PlayerController
	dependson(WarPawn)
	config(Game)
	native;

cpptext
{
	virtual UBOOL WantsLedgeCheck()
	{
		return 1;
	}
}

// the interval at which to check for cover when holding the button down
var() config float UseCoverInterval;

var bool	bLateComer;
var bool 	bDontUpdate;

var bool	bTargetingMode;		// when viewing in TargetingMode

//
// Cover
//

/** FOV to snap into cover mode. Desired cover position must be in this FOV (around player's crosshair) to go into cover mode */
var()	config	Vector2D CoverAdhesionFOV;

/** distance pct to stick to cover spot. (% of col cylinder radius) */
var(Cover)	config	float	CoverAcquireDistPct;

/** debug cover flag */
var(Cover)	config	bool	bDebugCover;

var config bool bDebugAI;

/** 
 * set to true when any of these properties gets changed to update pawn server side 
 * CoverAction, CoverType, CoverDirection 
 */
var		transient	bool	bReplicatePawnCover;

/** Save best cover spot, for debugging */
var	vector	LastDesiredCoverLocation;
var rotator	LastDesiredCoverRotation;

/** Main cover our pawn is associated with at the moment */
var transient CoverNode PrimaryCover;

/** Second cover used when in-between multiple cover points */
var transient CoverNode SecondaryCover;

/** Toggle for the assess mode */
var transient bool bAssessMode;

/** Local copy of bWasLeft/bWasRight from playerinput, for Cover_Stationary => Cover_Moving transitions */
var transient bool bWasLeft, bWasRight;

/** Current cover lean direction for the camera, independent of the cover action */
enum ECoverDirection
{
	CD_Default,
	CD_Left,
	CD_Right,
	CD_Up,
};
var transient ECoverDirection CoverDirection;

var()	config		float	CoverTransitionTime;
var		transient	float	CoverTransitionCountHold;
var		transient	float	CoverTransitionCountDown;

/** List of all interactable triggers, for rendering purposes */
struct native InteractableInfo
{
	var Trigger InteractTrigger;
	var bool bUsuable;
	var SeqEvent_Used Event;
};
var transient array<InteractableInfo> InteractableList;

/** Amount of cash player is carrying */
var()	int		Cash;

/** Last time seconds player killed another player. For multi kills tracking */
var		transient	float	LastKillTime;
/** multi kill count, for stats. */
var		transient	int		MultiKillCount;
/** cash reward for 2 kills at once */
var()	config		int		Reward_2kills;
/** cash reward for 3 kills at once */
var()	config		int		Reward_3kills;
/** cash reward for more than 3 kills at once */
var()	config		int		Reward_multikills;
/** cash reward for a melee kill */
var()	config		int		Reward_MeleeKillBonus;

var() config class<WarWeapon> PrimaryWeaponClass;
var() config class<WarWeapon> SecondaryWeaponClass;

/** Radius that is checked for nearby vehicles when pressing use */
var() config float	VehicleCheckRadius;

replication
{
	// variables the server should send to the client.
	reliable if( Role == ROLE_Authority && bNetOwner )
		Cash;

	// Things the server should send to the client.
	unreliable if( Role == ROLE_Authority )
		ClientPlayTakeHit, PlayStartupMessage, ClientForceMeleeAttack;

	// functions called from local player to server
	reliable if ( Role < ROLE_Authority )
		ServerTransitionToCover, ServerUpdatePawnCover, ServerEvade, ServerGiveItem, ServerSetTargetingMode;

	unreliable if ( Role < ROLE_Authority )
        TurretServerMove, DualTurretServerMove;
}

/** exec function to test camera shake */
exec function TestShake()
{
	CameraShake(	1.f, 
					vect(100,100,200), 
					vect(10,10,25),  
					vect(0,3,5), 
					vect(1,10,20),
					2,
					5
				);
}

/**
 * Scripting hook for camera shakes.
 */
simulated function OnCameraShake(SeqAct_CameraShake inAction)
{
	CameraShake(inAction.Duration,
				inAction.RotAmplitude,
				inAction.RotFrequency,
				inAction.LocAmplitude,
				inAction.LocFrequency,
				inAction.FOVAmplitude,
				inAction.FOVFrequency);
}

/**
 * Camera Shake
 * Plays camera shake effect
 *
 * @param	Duration			Duration in seconds of shake
 * @param	newRotAmplitude		view rotation amplitude (pitch,yaw,roll)
 * @param	newRotFrequency		frequency of rotation shake
 * @param	newLocAmplitude		relative view offset amplitude (x,y,z)
 * @param	newLocFrequency		frequency of view offset shake
 * @param	newFOVAmplitude		fov shake amplitude
 * @param	newFOVFrequency		fov shake frequency
 */
function CameraShake
( 
	float	Duration, 
	vector	newRotAmplitude, 
	vector	newRotFrequency,
	vector	newLocAmplitude, 
	vector	newLocFrequency, 
	float	newFOVAmplitude,
	float	newFOVFrequency
)
{
	if( WarPlayerCamera(PlayerCamera) != None )
	{
		WarPlayerCamera(PlayerCamera).CameraShake( Duration, newRotAmplitude, newRotFrequency, newLocAmplitude, newLocFrequency, newFOVAmplitude, newFOVFrequency );
	}
}

/** Lookip function because input class is object and doesn't have access to timers */
function GetInputTargets()
{
	WarPlayerInput(PlayerInput).GetInputTargets();
}

/** 
 * Reward player with Cash points 
 *
 * @param	CashAmount	amount of cash added or deducted
 */
function AddCash( int CashAmount )
{
	Cash += CashAmount;
}

/**
 * Notification called when this player gave damage to another player.
 *
 * @param	Victim		Victim who received the damage.
 * @param	DamageType	Type of damage given to victim
 * @param	LocDmgInfo	Locational Damage info
 */
function NotifyGiveDamage( WarPawn Victim, class<DamageType> DamageType, WarPawn.LocDmgStruct LocDmgInfo )
{
	if( Victim.Health <= 0 )
	{
		CheckMultiKillBonus();
		LastKillTime = Level.TimeSeconds;
		MultiKillCount++;

		// Victim was killed, award kill
		AddCash( Victim.KillReward );

		if( LocDmgInfo.BodyPartName == "Head" )
		{
			// if killed with headshot, award fast kill bonus
			ReceiveLocalizedMessage( class'Msg_CashBonuses', 0 );
			AddCash( Victim.FastKillBonus );
		}

		if( Victim.bAwardEliteKillBonus )
		{
			// if killed a higher rank enemy, award elite kill bonus
			ReceiveLocalizedMessage( class'Msg_CashBonuses', 1 );
			AddCash( Victim.EliteKillBonus );
		}

		if( class<WarDamageType>(DamageType) != None &&  class<WarDamageType>(DamageType).default.bMeleeDamage )
		{
			// if killed with a melee attack, award melee kill bonus.
			ReceiveLocalizedMessage( class'Msg_CashBonuses', 5 );
			AddCash( Reward_MeleeKillBonus );
		}
	}
}

/** Check kill related bonuses and rewards */
function CheckMultiKillBonus()
{
	// Only update for past kills. If we're checking the same frame as the last kill, we may miss more on the way.
	if( LastKillTime < Level.TimeSeconds && MultiKillCount > 0)
	{
		if( MultiKillCount == 2 )
		{
			// Two-In-One kill bonus
			AddCash( Reward_2kills );
			ReceiveLocalizedMessage( class'Msg_CashBonuses', 2 );
		}
		else if( MultiKillCount == 3 )
		{
			// Trio kill bonus
			AddCash( Reward_3kills );
			ReceiveLocalizedMessage( class'Msg_CashBonuses', 3 );
		}
		else if( MultiKillCount > 3 )
		{
			// multi kill bonus
			AddCash( Reward_multikills );
			ReceiveLocalizedMessage( class'Msg_CashBonuses', 4 );
		}
		MultiKillCount = 0;
	}

	SetTimer(0.25, false, 'CheckMultiKillBonus');
}

function PlayStartupMessage(byte StartupStage)
{
//	ReceiveLocalizedMessage( class'StartupMessage', StartupStage, PlayerReplicationInfo );
}

function NotifyTakeHit(pawn InstigatedBy, vector HitLocation, int Damage, class<DamageType> damageType, vector Momentum)
{
	local int iDam;

	Super.NotifyTakeHit(InstigatedBy,HitLocation,Damage,DamageType,Momentum);

	iDam = Clamp(Damage,0,250);
	if ( (Level.NetMode == NM_DedicatedServer) || (Level.NetMode == NM_ListenServer) )
		ClientPlayTakeHit(hitLocation - Pawn.Location, iDam, damageType);
}

function ClientPlayTakeHit(vector HitLoc, byte Damage, class<DamageType> damageType)
{
	HitLoc += Pawn.Location;
	Pawn.PlayTakeHit(HitLoc, Damage, damageType);
}

/**
 * returns true if player is targeting an enemy. And Enemy is in range of fire.
 *
 * @param	out_Enemy		Enemy we're aiming at.
 * @param	out_HitLocation	Projected aim point.
 * @param	TraceExtent		Extent to use for trace.
 *
 * @return	true if player is aiming at an enemy in range of fire.
 */
function bool IsAimingAtEnemyInRange( optional out WarPawn out_Enemy, optional out vector out_HitLocation, optional vector TraceExtent )
{
	local vector	out_HitNormal;
	local Actor		HitActor;

	if ( WarPawn(Pawn) != None && Pawn.Weapon != None )
	{
		HitActor	= WarPawn(Pawn).TraceWithPawnOffset( Pawn.Weapon.GetTraceRange(), out_HitLocation, out_HitNormal, TraceExtent );
		out_Enemy	= WarPawn(HitActor);
		return ( out_Enemy != None && !out_Enemy.bDeleteMe && out_Enemy.Health > 0 && !Level.GRI.OnSameTeam(Pawn,out_Enemy) );
	}

	return false;
}

// Player movement.
// Player Standing, walking, running, falling.
state PlayerWalking
{
ignores SeePlayer, HearNoise;

	function bool NotifyLanded(vector HitNormal)
	{
		if (DoubleClickDir == DCLICK_Active)
		{
			DoubleClickDir = DCLICK_Done;
			ClearDoubleClick();
			Pawn.Velocity *= Vect(0.1,0.1,1.0);
		}
		else
			DoubleClickDir = DCLICK_None;

		if ( Global.NotifyLanded(HitNormal) )
			return true;

		return false;
	}

    function PlayerMove( float DeltaTime )
    {
        // Turn off sprinting if we are done sprinting;
        if ( WarPawn(Pawn).bIsSprinting )
        {
        	if (PlayerInput.aForward <=0)
	        	WarPawn(Pawn).bIsSprinting = false;
            else
            	PlayerInput.aStrafe *= 0.25;
        }

		Super.PlayerMove(DeltaTime);
	}

	function ProcessMove(float DeltaTime, vector NewAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
	{
		if ( (DoubleClickMove == DCLICK_Active) && (Pawn.Physics == PHYS_Falling) )
			DoubleClickDir = DCLICK_Active;
		else if ( (DoubleClickMove != DCLICK_None) && (DoubleClickMove < DCLICK_Active) )
		{
			if ( WarPawn(Pawn).Dodge(DoubleClickMove) )
				DoubleClickDir = DCLICK_Active;
		}

		Super.ProcessMove(DeltaTime,NewAccel,DoubleClickMove,DeltaRot);
	}

	function EndState()
	{
		Super.EndState();
		if ( WarPawn(Pawn) != None && bDuck==0 )
		{
	    	WarPawn(Pawn).bIsSprinting = false;
		}
	}
}

function ServerSpectate()
{
	GotoState('Spectating');
}

state Dead
{
	ignores SeePlayer, HearNoise, KilledBy;

	exec function StartFire( optional Byte FireModeNum )
	{
		if ( bFrozen )
		{
			if ( !IsTimerActive() || GetTimerCount() > 1.f )
			{
				bFrozen = false;
			}
			return;
		}

		if ( PlayerReplicationInfo.bOutOfLives )
		{
			if ( Pawn != None )
			{
				UnPossess();
			}
			ServerSpectate();
		}
		else
		{
			super.StartFire( FireModeNum );
		}
	}

Begin:
	// for local players, display scores with a slight delay
	if ( LocalPlayer(Player) != None )
	{
    Sleep(3.0);
	if ( (ViewTarget == None) || (ViewTarget == self) || (VSize(ViewTarget.Velocity) < 1.0) )
	{
		Sleep(1.0);
		myHUD.bShowScores = true;
	}
	else
		Goto('Begin');
}
}

/** 
 * list important WarPController variables on canvas. HUD will call DisplayDebug() on the current ViewTarget when
 * the ShowDebug exec is used
 *
 * @param	HUD		- HUD with canvas to draw on
 * @input	out_YL		- Height of the current font
 * @input	out_YPos	- Y position on Canvas. out_YPos += out_YL, gives position to draw text for next debug line.
 */
function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local Canvas Canvas;

	Canvas = HUD.Canvas;
	Canvas.SetPos(0,0);
	Canvas.SetDrawColor(255,255,255,255);
	Canvas.DrawText("CONTROLLER "$GetItemName(string(self))$" Pawn "$GetItemName(string(Pawn)));
	out_YPos += out_YL;
	Canvas.SetPos(4, out_YPos);

	if ( Pawn == None )
	{
		super.DisplayDebug(HUD, out_YL, out_YPos);
		return;
	}

	if ( Enemy != None )
		Canvas.DrawText("     STATE: "$GetStateName()$" Timer: "$GetTimerCount()$" Enemy "$Enemy.GetHumanReadableName(), false);
	else
		Canvas.DrawText("     STATE: "$GetStateName()$" Timer: "$GetTimerCount()$" NO Enemy ", false);
	out_YPos += out_YL;
	Canvas.SetPos(4, out_YPos);

	if ( PlayerReplicationInfo == None )
		Canvas.DrawText("     NO PLAYERREPLICATIONINFO", false);
	else
		PlayerReplicationInfo.DisplayDebug(HUD, out_YL, out_YPos);

	out_YPos += out_YL;
	Canvas.SetPos(4, out_YPos);

	if ( PlayerCamera != None )
		PlayerCamera.DisplayDebug( HUD, out_YL, out_YPos );
	else
	{
		Canvas.SetDrawColor(255,0,0);
		Canvas.DrawText("NO CAMERA");
		out_YPos += out_YL;
		Canvas.SetPos(4, out_YPos);
	}
}

exec function SetRadius(float newradius)
{
	Pawn.SetCollisionSize(NewRadius,Pawn.CollisionHeight());
}

exec function GiveItem(string InvClassName)
{
	ServerGiveItem(InvClassName);
}

function ServerGiveItem(string InvClassName)
{
 	local class <Inventory> IClass;
	local inventory Item;

    if ( InStr(InvClassName,".") == -1 )
    	InvClassName = "WarfareGame." $ InvClassName;

    if (Pawn==None)
    {
    	log("ServerGiveItem: Needs a pawn");
        return;
    }

   	IClass = class<Inventory>(DynamicLoadObject(InvClassName,class'class'));
	if (IClass != None)
	{
		Item = Spawn(IClass,,,Pawn.Location);
        if (Item!=None)
        	Item.GiveTo(Pawn);
    	else
        	log("ServerGiveItem: Could Not Spawn");
    }
}

/**
 * Jump is disabled
 */
exec function Jump();

/* FIXME
exec function COG()
{
	ChangeSpawnClass("warfaregame.Pawn_COGGear");
}

exec function Geist()
{
	ChangeSpawnClass("warfaregame.Pawn_GeistLocust");
}

exec function Wretch()
{
	ChangeSpawnClass("warfaregame.Pawn_GeistWretch");
}
*/

/**
 * Called by PlayerController and PlayerInput to set bIsWalking flag, affecting Pawn's velocity 
 */
function HandleWalking()
{
	local bool					bCoverWalk;
	local CoverNode.ECoverType	PawnCT;

	if ( Pawn != None )
	{
		PawnCT		= GetPawnCoverType();
		bCoverWalk	= PawnCT != CT_None && PawnCT != CT_Standing;
		Pawn.SetWalking( (bCoverWalk || (bRun!=0) || bTargetingMode) && !Region.Zone.IsA('WarpZoneInfo') );
	}
}

/**
 * Switch between normal and targeting mode (= walking + cam above shoulder)
 */
exec function ToggleTargetingMode()
{
	if( bTargetingMode )
	{
		SetTargetingOff();
	}
	else
	{
		SetTargetingOn();
	}
}

function ServerSetTargetingMode( bool bNewTargeting )
{
	if( bNewTargeting )
	{
		SetTargetingOn();
	}
	else
	{
		SetTargetingOff();
	}
}

exec function SetTargetingOn()
{
	if( WarPawn(Pawn) == None )
	{
		SetTargetingOff();
		return;
	}

	if( bTargetingMode )
	{
		return;
	}

	if( Role < Role_Authority )
	{
		ServerSetTargetingMode( true );
	}

	DoToggleTargetingMode();
}

exec function SetTargetingOff()
{
	if( !bTargetingMode )
	{
		return;
	}

	if( Role < Role_Authority )
	{
		ServerSetTargetingMode( false );
	}

	DoToggleTargetingMode();
}

/**
 * Toggles Targeting mode.
 * Called on both client and server.
 */
simulated function DoToggleTargetingMode()
{
	// Inventory manager can override targeting mode
	if ( WarInventoryManager(Pawn.InvManager) != None && WarInventoryManager(Pawn.InvManager).OverrideTargetingMode() )
		return;

	// Set targetting mode
	bTargetingMode = !bTargetingMode;
	
	UpdateTargetingMode();
}

simulated function UpdateTargetingMode();

/** cover key pressed */
exec function CoverKeyPressed();
/** cover key released */
exec function CoverKeyReleased();

/** Special cover debug log formatting */
function CoverLog( string msg, string function )
{
	if( bDebugCover )
	{
		log( "[" $ Level.TimeSeconds $"]" @ "(" $ GetStateName() $ "::" $ function $ ")" @ msg);
	}
}

/** Tries to find a valid cover spot */
function bool FindCoverSpot()
{
	local CoverNode	BestCoverNode;
	local bool		bFoundCover;
	local vector	DesiredLocation, Origin, X, Y, Z, ClosestPt;
	local float		Radius, NodeDot, Dist;
	local rotator	CoverRotation;

	// Find best (closest) cover node
	BestCoverNode = FindBestCoverNode();
	if( BestCoverNode == None )
	{
		CoverLog("BestCoverNode == None", "FindCoverSpot");
		return false;
	}

	PrimaryCover	= BestCoverNode;
	SecondaryCover	= None;
	DesiredLocation = PrimaryCover.Location;
	CoverRotation	= PrimaryCover.Rotation;

	// Check for direct CoverNode proximity
	if( VSize2D(PrimaryCover.Location-Pawn.Location) < GetCollisionRadius() * CoverAcquireDistPct )
	{
		CoverLog("Acquired range cover", "FindCoverSpot");
		// then start in the stationary state
		bFoundCover = true;
	}
	else // Check for circular cover
	if( PrimaryCover.bCircular )
	{
		SecondaryCover = PrimaryCover.LeftNode;
		if( SecondaryCover != None )
		{
			Origin = (SecondaryCover.Location + PrimaryCover.Location)/2.f;
			Radius = VSize(SecondaryCover.Location-PrimaryCover.Location)/2.f;
			
			// check to see if we're within the extended radius
			if( (VSize2D(Origin-Pawn.Location) - VSize2D(Normal(Pawn.Location-Origin)*Radius) ) < GetCollisionRadius() * CoverAcquireDistPct )
			{
				// success!
				bFoundCover = true;

				// set the desired location as the nearest point on the circle
				DesiredLocation = Origin + (Normal(Pawn.Location - Origin) * Radius);
				CoverRotation	= Rotator(Origin - Pawn.Location);

				CoverLog("Acquired circular cover", "FindCoverSpot");
			}
		}
	}
	else // Check if in between 2 linked cover nodes
	{
		// if there is a secondary node, check for intermediate,
		GetAxes(PrimaryCover.Rotation, X, Y, Z);
		NodeDot = Normal(PrimaryCover.Location - Pawn.Location) dot -Y;
		
		if( NodeDot < 0.f )
		{
			SecondaryCover = PrimaryCover.LeftNode;
		}
		else
		{
			SecondaryCover = PrimaryCover.RightNode;
		}

		if( SecondaryCover != None )
		{
			Dist = PointDistToPlane(Pawn.Location, Rotator(SecondaryCover.Location-PrimaryCover.Location), PrimaryCover.Location, ClosestPt);
			if( Dist < GetCollisionRadius() * CoverAcquireDistPct )
			{
				bFoundCover = true;

				// set the new desired cover position
				DesiredLocation = ClosestPt;
				if( NodeDot < 0.f )
				{
					CoverRotation = Rotator( vect(0,0,1) Cross (SecondaryCover.Location-PrimaryCover.Location) );
				}
				else
				{
					CoverRotation = Rotator( (SecondaryCover.Location-PrimaryCover.Location) Cross vect(0,0,1) );
				}
				
				CoverLog("Acquired line interp cover", "FindCoverSpot");
			}
		}
	}

	if( !bFoundCover )
	{
		CoverLog("cover not found", "FindCoverSpot");
		return false;
	}

	// Now that we have a desired cover location, make sure we have it in our FOV to go into cover mode
	if( bFoundCover && !ValidateDesiredCover(DesiredLocation, CoverRotation) )
	{
		CoverLog("cover not validated", "FindCoverSpot");
		return false;
	}

	return true;
}

/** find the nearest cover node */
function CoverNode FindBestCoverNode()
{
	local int				idx;
	local CoverNode			BestNode, Node;
	local float				BestDist, Dist;
	local NavigationPoint	Nav;

	// first searching through touching nodes
	for (idx = 0; idx<Pawn.Touching.Length; idx++)
	{
		Node = CoverNode(Pawn.Touching[idx]);
		if( Node != None && Node.bEnabled )
		{
			Dist = VSize(Node.Location - Pawn.Location);
			if( Dist < BestDist || BestNode == None )
			{
				if( IsCoverNodeRelevant(Node) )
				{
					BestNode = Node;
					BestDist = Dist;
				}
			}
		}
	}
	
	// if none found, look for nearby cover nodes
	if( BestNode == None )
	{
		// and next through all nearby cover nodes in the nav list
		for (Nav = Level.NavigationPointList; Nav != None; Nav = Nav.NextNavigationPoint)
		{
			Node = CoverNode(Nav);
			if( Node != None && Node.bEnabled )
			{
				Dist = VSize(Node.Location - Pawn.Location);
				if( Dist < BestDist || bestNode == None )
				{
					if( IsCoverNodeRelevant(Node) )
					{
						BestNode = Node;
						BestDist = Dist;
					}
				}
			}
		}
	}

	return BestNode;
}

/** Make sure node is visible */
simulated function bool IsCoverNodeRelevant( CoverNode N )
{
	// returns true if did not hit world geometry
	if( FastTrace(N.Location, Pawn.Location) )
	{
		return true;
	}

	return false;
}

/**
 * Validates best cover spot.
 * Checks if player if facing the right direction.
 *
 * @param	CoverLocation	Desired cover location.
 * @param	CoverRotation	Desired cover rotation.
 *
 * @return	true if cover spot is validated.
 */
function bool ValidateDesiredCover( Vector CoverLocation, Rotator CoverRotation )
{
	local	Vector2D	out_DotDist, DotCoverAdhesionFOV;
	local	Rotator		CamRot;
	local	Vector		CamLoc, AxisX, AxisY, AxisZ;
	local	bool		bInFront;

	// Save best cover spot, for debugging
	LastDesiredCoverRotation = CoverRotation;
	LastDesiredCoverLocation = CoverLocation;

	// get dot distance between cam rot and cover rotation.
	GetPlayerViewPoint(CamLoc, CamRot);
	GetAxes(CamRot, AxisX, AxisY, AxisZ);
	bInFront = GetDotDistance(out_DotDist, Vector(CoverRotation), AxisX, AxisY, AxisZ);

	// convert to metric valuation
	out_DotDist.X = Abs(out_DotDist.X);
	out_DotDist.Y = 1.f - Abs(out_DotDist.Y);

	// get FOV range
	DotCoverAdhesionFOV.X = Cos(CoverAdhesionFOV.X*Pi/180.f);
	DotCoverAdhesionFOV.Y = Cos(CoverAdhesionFOV.Y*Pi/180.f);

	// if not within FOV, then reject this cover spot
	if( !bInFront||
		out_DotDist.X < DotCoverAdhesionFOV.X || 
		out_DotDist.Y < DotCoverAdhesionFOV.Y )
	{
		return false;
	}

	return true;
}

/** 
 * Sets new cover direction. 
 * Which represents where the camera is focusing. 
 * (CD_Right for a right lean or right blind firing).
 *
 * @param	NewCD	new cover direction
 */
final function SetCoverDirection( ECoverDirection NewCD )
{
	if( CoverDirection != NewCD )
	{
		CoverLog("NewCD:" @ NewCD, "SetCoverDirection" );
		CoverDirection = NewCD;
		bReplicatePawnCover = true;
	}
}

/** 
 * Return cover direction
 * Which represents where the camera is focusing. 
 * (CD_Right for a right lean or right blind firing).
 *
 * @return cover direction
 */
final function ECoverDirection GetCoverDirection()
{
	return CoverDirection;
}

/**
 * Assign new cover action to Pawn. (Defined in CoverNode.uc)
 *
 * @param	NewCA	New CoverAction.
 */
final function SetPawnCoverAction( CoverNode.ECoverAction NewCA )
{
	local WarPawn	WP;

	WP = WarPawn(Pawn);
	if( WP != None &&
		WP.CoverAction != NewCA )
	{
		bReplicatePawnCover = true;
		WP.SetCoverAction( NewCA );
	}
}

/**
 * Return current pawn's CoverAction. (Defined in CoverNode.uc)
 *
 * @return Pawn's CoverAction.
 */
final function CoverNode.ECoverAction GetPawnCoverAction()
{
	local WarPawn	WP;

	WP = WarPawn(Pawn);
	if( WP != None )
	{
		return WP.CoverAction;
	}

	return CA_Default;
}

/**
 * Assign new cover type to Pawn. (Defined in CoverNode.uc)
 *
 * @param	NewCT	New CoverType.
 */
final function SetPawnCoverType( CoverNode.ECoverType NewCT )
{
	local WarPawn	WP;

	WP = WarPawn(Pawn);
	if( WP != None &&
		WP.CoverType != NewCT )
	{
		bReplicatePawnCover = true;
		WP.SetCoverType( NewCT );
	}
}

/**
 * Return current pawn's CoverType. (Defined in CoverNode.uc)
 *
 * @return Pawn's CoverType.
 */
final function CoverNode.ECoverType GetPawnCoverType()
{
	local WarPawn	WP;

	WP = WarPawn(Pawn);
	if( WP != None )
	{
		return WP.CoverType;
	}

	return CT_None;
}

/** 
 * Replicate to server Pawn's cover attributes.
 * FIXEME LAURENT -- optimize when more solid
 */
function ServerUpdatePawnCover
(
	CoverNode.ECoverType	PawnCT,
	CoverNode.ECoverAction	PawnCA,
	ECoverDirection			PawnCD
)
{
	CoverLog("", "ServerUpdatePawnCover");

	SetPawnCoverType( PawnCT );
	SetPawnCoverAction( PawnCA );
	SetCoverDirection( PawnCD );
}

/** toggle cover code debugging */
exec function DebugCover()
{
	bDebugCover = !bDebugCover;
}

/** toggle ai debugging */
exec function DebugAI()
{
	bDebugAI = !bDebugAI;
}

/** overriden to provide cover code debugging */
event PlayerTick( float DeltaTime )
{
	local	vector		CamLoc;
	local	rotator		CamRot;
	local	float		PlayerCylRadius;

	super.PlayerTick( DeltaTime );

	if( bDebugCover )
	{
		PlayerCylRadius = GetCollisionRadius() * CoverAcquireDistPct;

		GetPlayerViewPoint(CamLoc, CamRot);
		WarPlayerInput(PlayerInput).DebugDrawFOV( CoverAdhesionFOV, CamLoc, CamRot, MakeColor(0,0,255,255), PlayerCylRadius );

		DrawDebugLine(LastDesiredCoverLocation, LastDesiredCoverLocation + Vector(LastDesiredCoverRotation) * PlayerCylRadius, 255, 0, 255 );
		DrawDebugLine(	LastDesiredCoverLocation + Vector(LastDesiredCoverRotation) * PlayerCylRadius + Vect(0,0,1) * 32, 
						LastDesiredCoverLocation + Vector(LastDesiredCoverRotation) * PlayerCylRadius - Vect(0,0,1) * 32, 0, 255, 255 );
	}

	// if anything changed, update our pawn server side...
	if( bReplicatePawnCover && Role < Role_Authority )
	{
		bReplicatePawnCover = false;
		ServerUpdatePawnCover( GetPawnCoverType(), GetPawnCoverAction(), GetCoverDirection() );
	}
}


/**
 * Tries to find an available cover spot for the player
 * And sends the player to the proper cover state.
 *
 * @param	bOneShot	Should only check for cover once.
 */
function StartCover( optional bool bOneShot )
{
	local CoverNode		NewPrimary, NewSecondary;
	local name			newState;
	local bool			bSuccess;

	CoverLog("", "StartCover");

	// if we have no pawn, we shouldn't be here
	if( Pawn == None )
	{
		CoverLog("Pawn == None", "StartCover");
		FailedStartCover( bOneShot );
		return;
	}

	if( IsEvading() )
	{
		// if we're evading, we cannot check for cover yet
		CoverLog("doing evade, skip", "UseCover");
	}
	else if( FindCoverSpot() )
	{
		// Look for available cover spot
		CoverLog("cover validated, send to state", "StartCover");
		ClearTimer('StartCover');
		bTargetingMode = false;
		bSuccess = true;

		NewPrimary		= PrimaryCover;
		PrimaryCover	= None;

		// transition to the cover state
		if( NewPrimary.bCircular )
		{
			// Make sure we have a valide secondary cover...
			NewSecondary	= NewPrimary.RightNode;
			NewState		= 'Cover_Circular';
		}
		else if( SecondaryCover != None )
		{
			NewSecondary	= SecondaryCover;
			SecondaryCover	= None;
			NewState		= 'Cover_Moving';
		}
		else
		{
			// Make sure we have a valide secondary cover for transitioning
			if( NewPrimary.RightNode != None )
			{
				NewSecondary = NewPrimary.RightNode;
			}
			else if ( NewPrimary.LeftNode != None )
			{
				NewSecondary = NewPrimary.LeftNode;
			}
			NewState = 'Cover_Stationary';
		}

		SuccessfulStartCover();
		if( newState != '' ||
			PrimaryCover != NewPrimary ||
			SecondaryCover != NewSecondary )
		{
			TransitionToCover( NewPrimary, NewSecondary, newState, true );
		}
	}
	else
	{
		CoverLog("no valid cover found", "StartCover");
	}

	if( !bSuccess )
	{
		CoverLog("no valid cover found", "StartCover");

		FailedStartCover( bOneShot );

		// FIXME -- LAURENT
		// make sure that no cover claims remain
		PrimaryCover	= None;
		SecondaryCover	= None;
	}
}

/** Succeeded to enter cover */
function SuccessfulStartCover();

/** Failed to enter cover */
function FailedStartCover( bool bOneShot )
{
	if( !bOneShot || IsTimerActive('StartCover') )
	{
		// if no valid cover found, then crouch and keep checking for valid cover
		if( Pawn != None )
		{
			Pawn.ShouldCrouch( true );
		}
		SetTimer(UseCoverInterval, false, 'StartCover');
	}
}

/** Breaks the player out of cover, and sends it back to normal physics */
function StopCover()
{
	CoverLog("", "StopCover");
	ClearTimer('StartCover');

	SetCoverDirection( CD_Default );
	SetPawnCoverAction( CA_Default );
	SetPawnCoverType( CT_None );

	// Uncrouch the pawn
	if( Pawn != None )
	{
		Pawn.UnCrouch();
	}
}

/** Transition to new cover state */
function TransitionToCover
(
				CoverNode	newPrimary,
				CoverNode	newSecondary,
	optional	name		newCoverState,
	optional	bool		bReplicate
)
{
	if( PrimaryCover != newPrimary )
	{
		// FIXME LAURENT - Should claims be made only when player touches the primary covernode?
		if( PrimaryCover != None )
		{
			PrimaryCover.UnClaim( Self );
		}
		PrimaryCover = newPrimary;
		if( NewPrimary != None )
		{
			newPrimary.Claim( Self );
		}
	}

	SecondaryCover = newSecondary;

	CoverLog(	"PrimaryCover:"$PrimaryCover @ 
				"SecondaryCover:"$SecondaryCover @ 
				"newCoverState:"$newCoverState, 
				"TransitionToCover" );

	// send to proper cover state if mentioned
	if( newCoverState != '' )
	{
		GotoState( newCoverState );
	}

	// That means we break from cover...
	if( newPrimary == None &&
		newSecondary == None &&
		newCoverState == '' )
	{
		CoverLog(	"EnterStartState()", "TransitionToCover" );
		EnterStartState();
	}

	// Synchronize server
	if( bReplicate && Role < Role_Authority )
	{
		ServerTransitionToCover(newPrimary, newSecondary, newCoverState);
	}
}

/** replicated function to synchronize server */
function ServerTransitionToCover
(
	CoverNode	newPrimary,
	CoverNode	newSecondary,
	name		newCoverState
)
{
	TransitionToCover(newPrimary, newSecondary, newCoverState, false);
}

/** 
	* Get cover axes (Z up and Y pointing right) 
	* 
	* @param	out_X - normalized vector along x axis
	* @param	out_Y - normalized vector along y axis
	* @param	out_Z - normalized vector along z axis
	*/
function GetCoverAxes(out vector out_X, out vector out_Y, out vector out_Z)
{
	if( IsInCoverState() && PrimaryCover != None )
	{
		GetAxes(PrimaryCover.Rotation, out_X, out_Y, out_Z);
	}
	else if( Pawn != None )
	{
		GetAxes(Pawn.Rotation, out_X, out_Y, out_Z);
	}
	else
	{
		GetAxes(Rotation, out_X, out_Y, out_Z);
	}
}

/** return true if player is in cover */
function bool IsInCoverState()
{
	return false;
}


/**
 * This is an abstract state derived by various cover states to provide
 * a generic interface.  
 * Should not be considered as a valid state for transitioning to.
 */
simulated state Cover
{
	/** return true if player is in cover */
	function bool IsInCoverState()
	{
		return true;
	}

	event PlayerTick( float DeltaTime )
	{
		if( CoverTransitionCountDown > 0 )
		{
			CoverTransitionCountDown -= DeltaTime;
			if( CoverTransitionCountDown < 0 )
			{
				CoverTransitionCountDown = 0.f;
			}
		}

		Global.PlayerTick(DeltaTime);
	}

	/** Overridden to prevent evades in cover until properly implemented. */
	exec function PlayerEvade();

	function StartCover( optional bool bOneShot );

	/**
	 * Transitions out of the cover state as well as cleans up any info
	 * related to cover.
	 */
	function StopCover()
	{
		Global.StopCover();

		bMovementInputEnabled	= true;
		/*
		PrimaryCover			= None;
		SecondaryCover			= None;
		EnterStartState();
		*/
		TransitionToCover(None, None, '', true);
	}

	/** Return current covernode's cover type */
	function CoverNode.ECoverType GetNodeCoverType()
	{
		return PrimaryCover.CoverType;
	}

	/** Get Cover Tangent (this means the Y component of GetCoverAxes) */
	final function vector	GetCoverTangent()
	{
		local vector X, Y, Z;

		GetCoverAxes(X, Y, Z);
		return Y;
	}

	/** Update Player posture. Return true if allowed to move */
	function bool UpdatePlayerPosture
	(
			float					DeltaTime,
		out	CoverNode.ECoverType	out_PawnCT,
		out CoverNode.ECoverAction	out_PawnCA,
		out	ECoverDirection			out_PawnCD
	);

	function PlayerMove( float DeltaTime )
	{
		local vector					X, Y, Z, NewAccel;
		local rotator					OldRotation, ViewRotation;
		local bool						bCanMove;
		local CoverNode.ECoverType		PawnCT;
		local CoverNode.ECoverAction	PawnCA;
		local ECoverDirection			PawnCD;

		PawnCT	= GetPawnCoverType();
		PawnCA	= GetPawnCoverAction();
		PawnCD	= GetCoverDirection();

		bCanMove = UpdatePlayerPosture( DeltaTime, PawnCT, PawnCA, PawnCD);

		SetPawnCoverType( PawnCT );
		SetPawnCoverAction( PawnCA );
		SetCoverDirection( PawnCD );

		GetCoverAxes(X, Y, Z);

		if( bCanMove )
		{
			NewAccel = PlayerInput.aStrafe * Y;
			NewAccel.Z = 0;
		}

		GroundPitch = 0;
		ViewRotation = Rotation;
		Pawn.CheckBob(DeltaTime, Y);

		// Update rotation.
		SetRotation(ViewRotation);
		OldRotation = Rotation;
		UpdateRotation( DeltaTime );

		if( Role < ROLE_Authority ) // then save this move and replicate it
		{
			ReplicateMove(DeltaTime, NewAccel, DCLICK_None, OldRotation - Rotation);
		}
		else
		{
			ProcessMove(DeltaTime, NewAccel, DCLICK_None, OldRotation - Rotation);
		}
	}

	function UpdateRotation( float DeltaTime )
	{
		local vector	X,Y,Z;
		local rotator	DeltaRot, newRotation, ViewRotation;

		ViewRotation	= Rotation;
		DesiredRotation = ViewRotation; //save old rotation
		TurnTarget		= None;
		bSetTurnRot		= false;

		// Calculate Delta to be applied on ViewRotation
		DeltaRot.Yaw	= PlayerInput.aTurn;
		DeltaRot.Pitch	= PlayerInput.aLookUp;
		ProcessViewRotation( DeltaTime, ViewRotation, DeltaRot );
		SetRotation( ViewRotation );

		GetCoverAxes(X, Y, Z);
		// force the pawn to face the cover direction
		Pawn.FaceRotation(Rotator(X), DeltaTime);
	
		ViewShake(deltaTime);
	
		NewRotation			= ViewRotation;
		NewRotation.Roll	= Rotation.Roll;
	}

	function BeginState()
	{
		CoverLog("", "BeginState");

		CoverTransitionCountHold = 0.f;
		CoverTransitionCountDown = 0.f;

		SetPawnCoverAction( CA_Default );
		// set correct cover type (crouch, midlevel, standing)
		SetPawnCoverType( GetNodeCoverType() );
	}

	function EndState()
	{
		CoverLog("", "EndState");
	}
}


/**
 * Examines all triggers in the level and returns the largest
 * distance a SeqEvent_Used is set to.
 */
final function float GetMaxInteractDistance()
{
	local float maxDist;
	local Trigger chkTrigger;
	local array<SequenceEvent> evtList;
	local SeqEvent_Used useEvt;
	local int idx;
	maxDist = InteractDistance;
	foreach AllActors(class'Trigger',chkTrigger)
	{
		evtList.Length = 0;
		chkTrigger.FindEventsOfClass(class'SeqEvent_Used',evtList);
		for (idx = 0; idx < evtList.Length; idx++)
		{
			useEvt = SeqEvent_Used(evtList[idx]);
			if (useEvt != None)
			{
				maxDist = FMax(maxDist,useEvt.InteractDistance);
			}
		}
	}
	return FMax(2048.f,maxDist);
}

/**
 * Enables the assess mode, so as to notify the HUD that it should
 * start rendering all possible interactions.
 */
exec function EnableAssessMode()
{
	InteractDistance = GetMaxInteractDistance();
	bAssessMode = true;
	if (Level.NetMode == NM_Standalone)
	{
		Level.TimeDilation = 0.5f;
	}
	// set the timer for updating the interactable list
	SetTimer(0.15f,true,'AssessModeTimer');
	// and do the initial update
	AssessModeTimer();
}

/**
 * Handles updating the InteractableList for the HUD.
 */
simulated function AssessModeTimer()
{
	local vector cameraLoc;
	local rotator cameraRot;
	local array<Trigger> useList;
	local int idx, evtIdx;
	local array<SequenceEvent> evtList;
	GetPlayerViewPoint(cameraLoc, cameraRot);
	// empty the current list
	InteractableList.Length = 0;
	// get the list of usuable triggers
	GetTriggerUseList(InteractDistance,1024.f,0.f,false,useList);
	// now build the list of interactable objects
	InteractableList.Length = useList.Length;
	for (idx = 0; idx < useList.Length; idx++)
	{
		InteractableList[idx].InteractTrigger = useList[idx];
		// check for actual event activation
		evtList.Length = 0;
		useList[idx].FindEventsOfClass(class'SeqEvent_Used',evtList);
		if (evtList.Length > 0)
		{
			InteractableList[idx].bUsuable = false;
			// test each event for activation
			for (evtIdx = 0; evtIdx < evtList.Length && !InteractableList[idx].bUsuable; evtIdx++)
			{
				if (evtList[evtIdx].CheckActivate(useList[idx],Pawn,true))
				{
					InteractableList[idx].bUsuable = true;
					InteractableList[idx].Event = SeqEvent_Used(evtList[evtIdx]);
				}
				else
				if (evtList[evtIdx].bEnabled)
				{
					// save the last enabled event as the one to draw unusuable
					InteractableList[idx].Event = SeqEvent_Used(evtList[evtIdx]);
				}
			}
			// only highlight if directly looking at the trigger
			if (InteractableList[idx].bUsuable)
			{
				InteractableList[idx].bUsuable = PointDistToLine(useList[idx].Location,vector(cameraRot),cameraLoc) <= 60.f;
			}
		}
		// remove if no events associated with it
		else
		{
			useList.Remove(idx,1);
			InteractableList.Remove(idx,1);
			idx--;
		}
	}
}

/**
 * Disables the assess mode.
 */
exec function DisableAssessMode()
{
	bAssessMode = false;
	if (Level.NetMode == NM_Standalone)
	{
		Level.TimeDilation = 1.f;
	}
	// clear any left-over interactables
	InteractableList.Length = 0;
	// Disable timer
	ClearTimer('AssessModeTimer');
}

/** returns true if player can fire his weapon */
function bool CanFireWeapon()
{
	return true;
}

exec function StartFire( optional byte FireModeNum )
{
	if( bAssessMode )
	{
		DisableAssessMode();
	}
	Super.StartFire(FireModeNum);
}

/**
 * Toggle Assess mode
 */
exec function ToggleAssessMode()
{
	if ( bAssessMode )
	{
		DisableAssessMode();
	}
	else
	{
		EnableAssessMode();
	}
}

/**
 * Figures out the direction the player is attempting to move and
 * passes the evade direction to the Pawn to instigate the movement.
 */
exec function PlayerEvade()
{
	local EEvadeDirection EvadeDir;

	// If cannot start evade, abort
	if( WarPawn(Pawn) == None ||
		!bMovementInputEnabled || 
		WarPawn(Pawn).EvadeDirection != ED_None )
	{
		return;
	}

	if( bAssessMode )
	{
		DisableAssessMode();
	}

	if( abs(WarPlayerInput(PlayerInput).RawJoyRight) > abs(WarPlayerInput(PlayerInput).RawJoyUp))
	{
		if( WarPlayerInput(PlayerInput).RawJoyRight > 0 )
		{
			EvadeDir = ED_Right;
		}
		else
		{
			EvadeDir = ED_Left;
		}
	}
	else
	{
		if( WarPlayerInput(PlayerInput).RawJoyUp < 0 )
		{ 
			EvadeDir = ED_Backward;
		}
		else
		{
			EvadeDir = ED_Forward;
		}
	}
	Evade( EvadeDir );
}

/** Trigger an Evade */
function Evade( EEvadeDirection evadeDir )
{
	WarPawn(Pawn).Evade( evadeDir );
	if( Role < Role_Authority )
	{
		ServerEvade( evadeDir );
	}
}

/** 
 * Server replicated function to trigger an evade. 
 * called from client to stay in synch 
 */
function ServerEvade( EEvadeDirection evadeDir )
{
	if( Role == Role_Authority )
	{
		Evade( evadeDir );
	}
}

/** return true if player is doing an evade */
function bool IsEvading()
{
	return WarPawn(Pawn).EvadeDirection != ED_None;
}

/** Called once our last evade has finished. */
simulated function NotifyEvadeFinished()
{
	EnableInput();
}

/** Enables movement and turning input. */
function EnableInput()
{
	bMovementInputEnabled	= true;
	bTurningInputEnabled	= true;
}

/**
 * return true if player was able to perform an action by pressing the Use Key
 */
simulated function bool PerformedUseAction()
{
	local Vehicle	DrivenVehicle;

	// try to interact...
	if( super.PerformedUseAction() )
	{
		return true;
	}

	if( WarPawn(Pawn) != None )
	{
		// first check for a jump down ledge
		if( WarPawn(Pawn).JumpDownLedge() )
		{
			return true;
		}
		if( WarPawn(Pawn).JumpUpLedge() )
		{
			return true;
		}
	}

	// below is only on server
	if( Role < Role_Authority )
	{
		return false;
	}

	DrivenVehicle = Vehicle(Pawn);
	if( DrivenVehicle != None )
	{
		return DrivenVehicle.DriverLeave(false);
	}

	// try to find a vehicle to drive
	if( FindVehicleToDrive() )
	{ 
		return true;
	}

	// If we're in a good position to do a melee attack, then do it!
	if( WarPawn(Pawn) != None && WarPawn(Pawn).SuggestMeleeAttack() )
	{
		ClientForceMeleeAttack();
		return true;
	}

	if( WarPawn(Pawn) != None )
	{
		// if we have nothing to interact with, just perform a melee attack
		ClientForceMeleeAttack();
	}

	return true;
}

/** Force a melee attack from server to client */
function ClientForceMeleeAttack()
{
	if( LocalPlayer(Player) != None && WarPawn(Pawn) != None )
	{
		WarPawn(Pawn).DoMeleeAttack();
	}
}

/** Tries to find a vehicle to drive within a limited radius. Returns true if successful */
function bool FindVehicleToDrive()
{
	local Vehicle EntryVehicle, V;

	if( Pawn == None )
	{
		return false;
	}

    // Check for nearby vehicles
    ForEach Pawn.VisibleCollidingActors(class'Vehicle', V, VehicleCheckRadius)
    {
        // Found a vehicle within radius
        EntryVehicle = V;
		//EntryVehicle = V.FindEntryVehicle(Pawn);
        if( EntryVehicle != None && EntryVehicle.TryToDrive(Pawn) )
		{
            return true;
		}
    }

	return false;
}

event MayFall()
{
	if (WarPawn(Pawn) != None &&
		WarPawn(Pawn).bJumpingDownLedge)
	{
		Pawn.bCanJump = true;
	}
}


/* TurretServerMove()
compressed version of server move for PlayerTurreting state
*/
function TurretServerMove
(
	float	TimeStamp,
	vector	ClientLoc,
	bool	NewbDuck,
	byte	ClientRoll,
	int		View
)
{
	ServerMove(TimeStamp,Vect(0,0,0),ClientLoc,0,ClientRoll,View);
}

/* DualTurretServerMove()
compressed version of server move for PlayerTurreting state
*/
function DualTurretServerMove
(
	float	TimeStamp0,
	bool	NewbDuck0,
	byte	ClientRoll0,
	int		View0,
	float	TimeStamp,
	vector	ClientLoc,
	bool	NewbDuck,
	byte	ClientRoll,
	int		View
)
{
	ServerMove(TimeStamp0,Vect(0,0,0),vect(0,0,0),0,ClientRoll0,View0);
	ServerMove(TimeStamp,Vect(0,0,0),ClientLoc,0,ClientRoll,View);
}

state PlayerTurreting
{
ignores SeePlayer, HearNoise, Bump;
/* FIXMESTEVE
	function CallServerMove
	(
		float				TimeStamp,
		vector				InAccel,
		vector				ClientLoc,
		byte				NewFlags,
		byte				ClientRoll,
		int					View,
		optional byte		OldTimeDelta,
		optional int		OldAccel
	)
	{

		if ( PendingMove != None )
		{
			DualTurretServerMove
			(
				PendingMove.TimeStamp,
				PendingMove.bDuck,
		        ((PendingMove.Rotation.Roll >> 8) & 255),
				(32767 & (PendingMove.Rotation.Pitch/2)) * 32768 + (32767 & (PendingMove.Rotation.Yaw/2)),
				TimeStamp,
				ClientLoc,
				NewbDuck,
				ClientRoll,
				View
			);
		}
		else
			TurretServerMove
			(
				TimeStamp,
				ClientLoc,
				NewbDuck,
				ClientRoll,
				View
			);
	}
*/
	/* ServerMove()
	- replicated function sent by client to server - contains client movement and firing info
	Passes acceleration in components so it doesn't get rounded.
	IGNORE VANILLA SERVER MOVES
	*/
/*
	function ServerMove
	(
		float				TimeStamp,
		vector				InAccel,
		vector				ClientLoc,
		byte				NewFlags,
		byte				ClientRoll,
		int					View
	)
	{
		// If this move is outdated, discard it.
		if ( CurrentTimeStamp >= TimeStamp )
			return;

		if ( AcknowledgedPawn != Pawn )
		{
			OldTimeDelta = 0;
			InAccel = vect(0,0,0);
		}

		if ( AcknowledgedPawn == Pawn && CurrentTimeStamp < TimeStamp )
	       Pawn.AutonomousPhysics(TimeStamp - CurrentTimeStamp);
		CurrentTimeStamp = TimeStamp;
		ServerTimeStamp = Level.TimeSeconds;
	}
*/
	function TurretServerMove
	(
		float	TimeStamp,
		vector	ClientLoc,
		bool	NewbDuck,
		byte	ClientRoll,
		int		View
	)
	{
		Global.ServerMove(TimeStamp,Vect(0,0,0),ClientLoc,0,ClientRoll,View);
	}

	/* DualTurretServerMove()
	compressed version of server move for PlayerTurreting state
	*/
	/*
	function DualTurretServerMove
	(
		float	TimeStamp0,
		bool	NewbDuck0,
		byte	ClientRoll0,
		int		View0,
		float	TimeStamp,
		vector	ClientLoc,
		bool	NewbDuck,
		byte	ClientRoll,
		int		View
	)
	{
		Global.ServerMove(TimeStamp0,Vect(0,0,0),vect(0,0,0),false,NewbDuck0,false,false, DCLICK_NONE,ClientRoll0,View0);
		Global.ServerMove(TimeStamp,Vect(0,0,0),ClientLoc,false,NewbDuck,false,false, DCLICK_NONE,ClientRoll,View);
	}
	*/
    function PlayerMove(float DeltaTime)
    {
		if ( Pawn == None )
		{
			GotoState('dead');
			return;
		}

		Pawn.UpdateRocketAcceleration(DeltaTime, PlayerInput.aTurn, PlayerInput.aLookUp);
		// FIXME SetRotation( Pawn.GetRotation() );

		ViewShake( deltaTime );

        if ( Role < ROLE_Authority ) // then save this move and replicate it
            ReplicateMove(DeltaTime, Pawn.Acceleration, DCLICK_None, rot(0,0,0));
        else
            ProcessMove(DeltaTime, Pawn.Acceleration, DCLICK_None, rot(0,0,0));
    }

	function ProcessMove(float DeltaTime, vector NewAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
    {
		if ( Pawn == None )
			return;

		//Pawn.Acceleration = newAccel;
	}

    function BeginState()
    {
		if ( Pawn != None )
			Pawn.SetPhysics( PHYS_Flying );

		RotationRate.Pitch	= 16384; // extending pitch limit (limits network weapon aiming)
    }

    function EndState()
    {
		RotationRate.Pitch = default.RotationRate.Pitch; // restoring pitch limit
	}

Begin:
}

exec function SaveClassConfig(coerce string className)
{
	local class<Object> saveClass;
	log("SaveClassConfig:"@className);
	saveClass = class<Object>(DynamicLoadObject(className,class'class'));
	if (saveClass != None)
	{
		log("- Saving config on:"@saveClass);
		saveClass.static.StaticSaveConfig();
	}
	else
	{
		log("- Failed to find class:"@className);
	}
}

exec function SaveActorConfig(coerce Name actorName)
{
	local Actor chkActor;
	log("SaveActorConfig:"@actorName);
	foreach AllActors(class'Actor',chkActor)
	{
		if (chkActor != None &&
			chkActor.Name == actorName)
		{
			log("- Saving config on:"@chkActor);
			chkActor.SaveConfig();
		}
	}
}

/**
 * Scripting hook for morale adjusments, just redirects to our
 * current team.
 */
simulated function OnAdjustMorale(SeqAct_AdjustMorale inAction)
{
	local WarTeamInfo team;
	team = WarTeamInfo(PlayerReplicationInfo.Team);
	if (team != None &&
		inAction.MoraleAdjustment != 0.f)
	{
		team.AdjustMorale(inAction.MoraleAdjustment);
	}
}

exec function Suicide()
{
	Pawn.Suicide();
}

/**
 * Scripting hook for temp HUD fade, nuke post greenlight.
 */
simulated function OnTempHUDFade(SeqAct_TempHUDFade inAction)
{
	local WarHUD wfHUD;
	wfHUD = WarHUD(myHUD);
	// set the new fade color/alpha
	wfHUD.PreviousFadeAlpha = wfHUD.FadeAlpha;
	wfHUD.DesiredFadeAlpha = inAction.FadeColor.A;
	wfHUD.FadeColor = inAction.FadeColor;
	// set the new fade time
	wfHUD.DesiredFadeAlphaTime = inAction.FadeTime;
	wfHUD.FadeAlphaTime = 0.f;
}

simulated function OnModifyCash(SeqAct_ModifyCash inAction)
{
	AddCash(inAction.CashAmount);
}

exec function Loadout()
{
	WarConsole(Console).ActivateUIContainer(new class'UIContainer_Loadout');
}

exec function MainMenu()
{
	WarConsole(Console).ActivateUIContainer(new class'UIContainer_MainMenu');
}

function OnConsoleCommand(SeqAct_ConsoleCommand inAction)
{
	ConsoleCommand(inAction.Command);
}

defaultproperties
{
	bDebugCover=false

	CoverAcquireDistPct=1.0
	CoverAdhesionFOV=(X=35,Y=80)

    FovAngle=+00085.000000
	CameraClass=class'WarPlayerCamera'
	CheatClass=class'WarCheatManager'
	InputClass=class'WarPlayerInput'
	ConsoleClass=class'WarConsole'
	UseCoverInterval=0.25f

	Reward_2kills=50
	Reward_3kills=100
	Reward_multikills=200
	Reward_MeleeKillBonus=25

	PrimaryWeaponClass=class'Weap_AssaultRifle'
	SecondaryWeaponClass=class'Weap_MX8SnubPistol'

	VehicleCheckRadius=350
}
