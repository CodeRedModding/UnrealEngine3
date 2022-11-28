class UTBot extends AIController
	native;

cpptext
{
	DECLARE_FUNCTION(execPollWaitToSeeEnemy)
	UBOOL Tick( FLOAT DeltaSeconds, ELevelTick TickType );
	virtual void UpdateEnemyInfo(APawn* AcquiredEnemy);
}

var		bool		bHuntPlayer;		// hunting player
var		bool		bEnemyInfoValid;	// false when change enemy, true when LastSeenPos etc updated

var float WarningDelay;		// delay before act on firing warning
var Projectile WarningProjectile;

// for monitoring the position of a pawn
var		vector		MonitorStartLoc;	// used by latent function MonitorPawn()
var		Pawn		MonitoredPawn;		// used by latent function MonitorPawn()
var		float		MonitorMaxDistSq;

// enemy position information
var		vector		LastSeenPos; 	// enemy position when I last saw enemy (auto updated if EnemyNotVisible() enabled)
var		vector		LastSeeingPos;	// position where I last saw enemy (auto updated if EnemyNotVisible enabled)
var		float		LastSeenTime;


native final latent function WaitToSeeEnemy(); // return when looking directly at visible enemy
native final function bool CanMakePathTo(Actor A); // assumes valid CurrentPath, tries to see if CurrentPath can be combine with path to N

/* epic ===============================================
* ::FindBestInventoryPath
*
* Searches the navigation network for a path that leads
* to nearby inventory pickups.
*
* =====================================================
*/
// native final function actor FindBestInventoryPath(out float MinWeight);

/* Reset()
reset actor to initial state
*/
function Reset()
{
	super.Reset();

	LastSeenTime = 0;
	MonitoredPawn = None;
	WarningProjectile = None;
	bHuntPlayer = false;
}

/* ReceiveWarning()
 AI controlled creatures may duck
 if not falling, and projectile time is long enough
 often pick opposite to current direction (relative to shooter axis)
*/
event ReceiveWarning(Pawn shooter, float projSpeed, vector FireDir);

function NotifyTakeHit(pawn InstigatedBy, vector HitLocation, int Damage, class<DamageType> damageType, vector Momentum)
{
	if ( (instigatedBy != None) && (instigatedBy != pawn) )
		damageAttitudeTo(instigatedBy, Damage);
}

function damageAttitudeTo(pawn Other, float Damage);


function WasKilledBy(Controller Other)
{
/* FIXMESTEVE	local Controller C;

	if ( Pawn.bUpdateEyeHeight )
	{
		for ( C=Level.ControllerList; C!=None; C=C.NextController )
			if ( C.IsA('UTPlayerController') && (PlayerController(C).ViewTarget == Pawn) && (PlayerController(C).RealViewTarget == None) )
				UTPlayerController(C).ViewNextBot();
	}
	if ( (Other != None) && (Other.Pawn != None) )
		LastKillerPosition = Other.Pawn.Location;
*/
}


function ReceiveProjectileWarning(Projectile proj)
{
	if ( WarningProjectile == None )
		ReceiveWarning(Proj.Instigator, Proj.speed, Normal(Proj.Velocity));
}

/* If ReceiveWarning caused WarningDelay to be set, this will be called when it times out
*/
event DelayedWarning();

/* DisplayDebug()
list important controller attributes on canvas
*/
function DisplayDebug(HUD HUD, out float YL, out float YPos)
{
	local string DebugString;
	local Canvas Canvas;

	Canvas = HUD.Canvas;
	if ( Pawn == None )
	{
		Super.DisplayDebug(HUD,YL,YPos);
		return;
	}

	Canvas.SetDrawColor(255,0,0);
	Canvas.DrawText("CONTROLLER "$GetItemName(string(self))$" Pawn "$GetItemName(string(Pawn))$" viewpitch "$Rotation.Pitch);
	YPos += YL;
	Canvas.SetPos(4,YPos);

	if ( HUD.bShowAIDebug )
	{
		if ( MonitoredPawn != None )
			DebugString = DebugString$"     MonitoredPawn: "@MonitoredPawn.GetHumanReadableName();
		else
			DebugString = DebugString$"     MonitoredPawn: None";
		Canvas.DrawText(DebugString);
		YPos += YL;
		Canvas.SetPos(4,YPos);
	}
}

event MonitoredPawnAlert();

function StartMonitoring(Pawn P, float MaxDist)
{
	MonitoredPawn = P;
	MonitorStartLoc = P.Location;
	MonitorMaxDistSq = MaxDist * MaxDist;
}

function PawnDied(Pawn P)
{
	if ( Pawn != P )
		return;
	if ( UTPawn(Pawn) != None )
	{
		if ( UTPawn(Pawn).InCurrentCombo() )
			UTPlayerReplicationInfo(PlayerReplicationInfo).Adrenaline = 0;
	}
	Super.PawnDied(P);
}

defaultproperties
{
	bIsPlayer=true
}
