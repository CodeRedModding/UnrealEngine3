//=============================================================================
// CheatManager
// Object within playercontroller that manages "cheat" commands
// only spawned in single player mode
//=============================================================================

class CheatManager extends Object within PlayerController
	native;

/* Used for correlating game situation with log file
*/

exec function ReviewJumpSpots(name TestLabel)
{
	if ( TestLabel == 'Transloc' )
		TestLabel = 'Begin';
	else if ( TestLabel == 'Jump' )
		TestLabel = 'Finished';
	else if ( TestLabel == 'Combo' )
		TestLabel = 'FinishedJumping';
	else if ( TestLabel == 'LowGrav' )
		TestLabel = 'FinishedComboJumping';
	log("TestLabel is "$TestLabel);
	Level.Game.ReviewJumpSpots(TestLabel);
}

exec function ListDynamicActors()
{
	local Actor A;
	local int i;

	ForEach DynamicActors(class'Actor',A)
	{
		i++;
		log(i@A);
	}
	log("Num dynamic actors: "$i);
}

exec function FreezeFrame(float delay)
{
	Level.Game.SetPause(true,outer);
	Level.PauseDelay = Level.TimeSeconds + delay;
}

exec function WriteToLog( string Param )
{
	log("NOW! "$Param);
}

exec function KillViewedActor()
{
	if ( ViewTarget != None )
	{
		if ( (Pawn(ViewTarget) != None) && (Pawn(ViewTarget).Controller != None) )
			Pawn(ViewTarget).Controller.Destroy();
		ViewTarget.Destroy();
		SetViewTarget(None);
	}
}

/* Teleport()
Teleport to surface player is looking at
*/
exec function Teleport()
{
	local Actor		HitActor;
	local vector	HitNormal, HitLocation;
	local vector	ViewLocation;
	local rotator	ViewRotation;

	GetPlayerViewPoint( ViewLocation, ViewRotation );

	HitActor = Trace(HitLocation, HitNormal, ViewLocation + 1000000 * vector(ViewRotation), ViewLocation, true);
	if ( HitActor != None)
		HitLocation += HitNormal * 4.0;

	ViewTarget.SetLocation( HitLocation );
}

/*
Scale the player's size to be F * default size
*/
exec function ChangeSize( float F )
{
	Pawn.CylinderComponent.SetSize( Pawn.Default.CylinderComponent.CollisionHeight * F, Pawn.Default.CylinderComponent.CollisionRadius * F );
	Pawn.SetDrawScale(F);
	Pawn.SetLocation(Pawn.Location);
}

/* Stop interpolation
*/
exec function EndPath()
{
}

exec function Amphibious()
{
	Pawn.UnderwaterTime = +999999.0;
}

exec function Fly()
{
	if ( (Pawn != None) && Pawn.CheatFly() )
	{
	ClientMessage("You feel much lighter");
	bCheatFlying = true;
	Outer.GotoState('PlayerFlying');
}
}

exec function Walk()
{
		bCheatFlying = false;
	if ( (Pawn != None) && Pawn.CheatWalk() )
		ClientReStart(Pawn);
}

exec function Ghost()
{
	if ( (Pawn != None) && Pawn.CheatGhost() )
	{
		ClientMessage("You feel ethereal");
		bCheatFlying = true;
		Outer.GotoState('PlayerFlying');
	}
}

/* AllAmmo
	Sets maximum ammo on all weapons
*/
exec function AllAmmo();

exec function Invisible(bool B)
{
	Pawn.bHidden = B;

	if (B)
		Pawn.Visibility = 0;
	else
		Pawn.Visibility = Pawn.Default.Visibility;
}

exec function God()
{
	if ( bGodMode )
	{
		bGodMode = false;
		ClientMessage("God mode off");
		return;
	}

	bGodMode = true;
	ClientMessage("God Mode on");
}

exec function SloMo( float T )
{
	Level.Game.SetGameSpeed(T);
	Level.Game.SaveConfig();
	Level.Game.GameReplicationInfo.SaveConfig();
}

exec function SetJumpZ( float F )
{
	Pawn.JumpZ = F;
}

exec function SetGravity( float F )
{
	PhysicsVolume.Gravity.Z = F;
}

exec function SetSpeed( float F )
{
	Pawn.GroundSpeed = Pawn.Default.GroundSpeed * f;
	Pawn.WaterSpeed = Pawn.Default.WaterSpeed * f;
}

exec function KillAll(class<actor> aClass)
{
	local Actor A;
	local Controller C;

	for (C = Level.ControllerList; C != None; C = C.NextController)
		if (PlayerController(C) != None)
			PlayerController(C).ClientMessage("Killed all "$string(aClass));

	if ( ClassIsChildOf(aClass, class'AIController') )
	{
		Level.Game.KillBots(Level.Game.NumBots);
		return;
	}
	if ( ClassIsChildOf(aClass, class'Pawn') )
	{
		KillAllPawns(class<Pawn>(aClass));
		return;
	}
	ForEach DynamicActors(class 'Actor', A)
		if ( ClassIsChildOf(A.class, aClass) )
			A.Destroy();
}

// Kill non-player pawns and their controllers
function KillAllPawns(class<Pawn> aClass)
{
	local Pawn P;

	Level.Game.KillBots(Level.Game.NumBots);
	ForEach DynamicActors(class'Pawn', P)
		if ( ClassIsChildOf(P.Class, aClass)
			&& !P.IsPlayerPawn() )
		{
			if ( P.Controller != None )
				P.Controller.Destroy();
			P.Destroy();
		}
}

exec function KillPawns()
{
	KillAllPawns(class'Pawn');
}

/**
 * Possess a pawn of the requested class
 */
exec function Avatar( name ClassName )
{
	local Pawn			TargetPawn, FirstPawn, OldPawn;
	local bool			bPickNextPawn;

	Foreach DynamicActors(class'Pawn',TargetPawn)
	{
		if ( TargetPawn == Pawn )
		{
			bPickNextPawn = true;
		}
		else if ( TargetPawn.IsA(ClassName) )
		{
			if ( FirstPawn == None )
			{
				FirstPawn = TargetPawn;
			}

			if ( bPickNextPawn )
			{
				break;
			}
		}
	}

	// if we went through the list without choosing a pawn, pick first available choice (loop)
	if ( TargetPawn == None )
	{
		TargetPawn = FirstPawn;
	}

	if ( TargetPawn != None )
	{
		// detach TargetPawn from its controller and kill its controller.
		TargetPawn.DetachFromController( true );

		// detach player from current pawn and possess targetpawn
		OldPawn = Pawn;
		Pawn.DetachFromController();
		Possess( TargetPawn );

		// Spawn default controller for our ex-pawn (AI)
		OldPawn.SpawnDefaultController();
	}
	else
	{
		log("Avatar: Couldn't find any Pawn to possess of class '" $ ClassName $ "'");
	}
}

exec function Summon( string ClassName )
{
	local class<actor> NewClass;
	local vector SpawnLoc;

	log( "Fabricate " $ ClassName );
	NewClass = class<actor>( DynamicLoadObject( ClassName, class'Class' ) );
	if( NewClass!=None )
	{
		if ( Pawn != None )
			SpawnLoc = Pawn.Location;
		else
			SpawnLoc = Location;
		Spawn( NewClass,,,SpawnLoc + 72 * Vector(Rotation) + vect(0,0,1) * 15 );
	}
}

exec function PlayersOnly()
{
	Level.bPlayersOnly = !Level.bPlayersOnly;
}

exec function CheatView( class<actor> aClass, optional bool bQuiet )
{
	ViewClass(aClass,bQuiet,true);
}

// ***********************************************************
// Navigation Aids (for testing)

// remember spot for path testing (display path using ShowDebug)
exec function RememberSpot()
{
	if ( Pawn != None )
		Destination = Pawn.Location;
	else
		Destination = Location;
}

// ***********************************************************
// Changing viewtarget

exec function ViewSelf(optional bool bQuiet)
{
	Outer.ResetCameraMode();
	if ( Pawn != None )
		SetViewTarget(Pawn);
	else
		SetViewtarget(outer);
	if (!bQuiet )
		ClientMessage(OwnCamera, 'Event');

	FixFOV();
}

exec function ViewPlayer( string S )
{
	local Controller P;

	for ( P=Level.ControllerList; P!=None; P= P.NextController )
		if ( P.bIsPlayer && (P.PlayerReplicationInfo.PlayerName ~= S) )
			break;

	if ( P.Pawn != None )
	{
		ClientMessage(ViewingFrom@P.PlayerReplicationInfo.PlayerName, 'Event');
		SetViewTarget(P.Pawn);
	}
}

exec function ViewActor( name ActorName)
{
	local Actor A;

	ForEach AllActors(class'Actor', A)
		if ( A.Name == ActorName )
		{
			SetViewTarget(A);
            SetCameraMode('ThirdPerson');
			return;
		}
}

exec function ViewFlag()
{
	local Controller C;

	For ( C=Level.ControllerList; C!=None; C=C.NextController )
		if ( C.IsA('AIController') && (C.PlayerReplicationInfo != None) && C.PlayerReplicationInfo.bHasFlag )
		{
			SetViewTarget(C.Pawn);
			return;
		}
}

exec function ViewBot()
{
	local actor first;
	local bool bFound;
	local Controller C;

	For ( C=Level.ControllerList; C!=None; C=C.NextController )
		if ( C.IsA('AIController') && (C.Pawn != None) )
	{
		if ( bFound || (first == None) )
		{
			first = C;
			if ( bFound )
				break;
		}
		if ( C == RealViewTarget )
			bFound = true;
	}

	if ( first != None )
	{
		SetViewTarget(first);
        SetCameraMode( 'ThirdPerson' );
		FixFOV();
	}
	else
		ViewSelf(true);
}

exec function ViewClass( class<actor> aClass, optional bool bQuiet, optional bool bCheat )
{
	local actor other, first;
	local bool bFound;

	if ( !bCheat && (Level.Game != None) )
		return;

	first = None;

	ForEach AllActors( aClass, other )
	{
		if ( bFound || (first == None) )
		{
			first = other;
			if ( bFound )
				break;
		}
		if ( other == ViewTarget )
			bFound = true;
	}

	if ( first != None )
	{
		if ( !bQuiet )
		{
			if ( Pawn(first) != None )
				ClientMessage(ViewingFrom@First.GetHumanReadableName(), 'Event');
			else
				ClientMessage(ViewingFrom@first, 'Event');
		}
		SetViewTarget(first);
		FixFOV();
	}
	else
		ViewSelf(bQuiet);
}

exec function Loaded()
{
	if( Level.Netmode!=NM_Standalone )
		return;

    AllWeapons();
    AllAmmo();
}

/* AllWeapons
	Give player all available weapons
*/
exec function AllWeapons()
{
	// subclass me
}

function GiveWeapon( String WeaponClassStr )
{
	local class<Weapon> WeaponClass;

	WeaponClass = class<Weapon>(DynamicLoadObject(WeaponClassStr, class'Class'));
	if ( Pawn.FindInventoryType(WeaponClass) != None )
		return;

	Pawn.CreateInventory( WeaponClass );
}

defaultproperties
{
}