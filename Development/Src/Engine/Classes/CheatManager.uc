//=============================================================================
// CheatManager
// Object within playercontroller that manages "cheat" commands
// only spawned in single player mode
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

class CheatManager extends Object within PlayerController
	dependson(AppNotificationsBase)
	native;

var localized		string					ViewingFrom;
var localized		string					OwnCamera;

/**
 *	Finds the nearest pawn of the given class (excluding the owner's pawn) and
 *	plays the specified FaceFX animation.
 */
exec function FXPlay(class<Pawn> aClass, string FXAnimPath)
{
	local Pawn P, ClosestPawn;
	local float ThisDistance, ClosestPawnDistance;
	local string FxAnimGroup;
	local string FxAnimName;
	local int dotPos;

	if ( WorldInfo.NetMode == NM_Standalone )
	{
		ClosestPawn = None;
		ClosestPawnDistance = 10000000.0;
		ForEach DynamicActors(class'Pawn', P)
		{
			if( ClassIsChildOf(P.class, aClass) && (P != PlayerController(Owner).Pawn) )
			{
				ThisDistance = VSize(P.Location - PlayerController(Owner).Pawn.Location);
				if(ThisDistance < ClosestPawnDistance)
				{
					ClosestPawn = P;
					ClosestPawnDistance = ThisDistance;
				}
			}
		}

		if( ClosestPawn.Mesh != none )
	    {
			dotPos = InStr(FXAnimPath, ".");
			if( dotPos != -1 )
			{
				FXAnimGroup = Left(FXAnimPath, dotPos);
				FXAnimName  = Right(FXAnimPath, Len(FXAnimPath) - dotPos - 1);
				ClosestPawn.Mesh.PlayFaceFXAnim(None, FXAnimName, FXAnimGroup, none);
			}
	    }
	}
}

/**
 *	Finds the nearest pawn of the given class (excluding the owner's pawn) and
 *	stops any currently playing FaceFX animation.
 */
exec function FXStop(class<Pawn> aClass)
{
	local Pawn P, ClosestPawn;
	local float ThisDistance, ClosestPawnDistance;

	if ( WorldInfo.NetMode == NM_StandAlone )
	{
		ClosestPawn = None;
		ClosestPawnDistance = 10000000.0;
		ForEach DynamicActors(class'Pawn', P)
		{
			if( ClassIsChildOf(P.class, aClass) && (P != PlayerController(Owner).Pawn) )
			{
				ThisDistance = VSize(P.Location - PlayerController(Owner).Pawn.Location);
				if(ThisDistance < ClosestPawnDistance)
				{
					ClosestPawn = P;
					ClosestPawnDistance = ThisDistance;
				}
			}
		}

		if( ClosestPawn.Mesh != none )
	    {
			ClosestPawn.Mesh.StopFaceFXAnim();
		}
	}
}

exec function DebugAI(optional coerce name Category);

exec function EditAIByTrace()
{
	local Vector CamLoc;
	local Rotator CamRot;
	local Vector HitLocation, HitNormal;
	local Pawn HitPawn;
	local Controller C;
	
	GetPlayerViewPoint( CamLoc, CamRot );
	HitPawn = Pawn(Trace( HitLocation, HitNormal, CamLoc + Vector(CamRot) * 10000, CamLoc, TRUE, vect(10,10,10) ));
	if( HitPawn != None )
	{
		C = HitPawn.Controller;
		if( C == None && HitPawn.DrivenVehicle != None )
		{
			C = HitPawn.DrivenVehicle.Controller;
		}

		if( C != None )
		{
			ConsoleCommand( "EDITACTOR NAME="$C.Name, TRUE );
		}		
	}
}

/** Dumps the pause state of the game */
exec function DebugPause()
{
	WorldInfo.Game.DebugPause();
}

exec function ListDynamicActors()
{
`if(`notdefined(FINAL_RELEASE))
	local Actor A;
	local int i;

	ForEach DynamicActors(class'Actor',A)
	{
		i++;
		`log(i@A);
	}
	`log("Num dynamic actors: "$i);
`endif
}

exec function FreezeFrame(float delay)
{
	WorldInfo.Game.SetPause(Outer,Outer.CanUnpause);
	WorldInfo.PauseDelay = WorldInfo.TimeSeconds + delay;
}

exec function WriteToLog( string Param )
{
	`log("NOW! "$Param);
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
	Pawn.CylinderComponent.SetCylinderSize( Pawn.Default.CylinderComponent.CollisionRadius * F, Pawn.Default.CylinderComponent.CollisionHeight * F );
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
	if (Pawn != None && Pawn.CheatWalk())
	{
		Restart(false);
	}
}

exec function Ghost()
{
	if ( (Pawn != None) && Pawn.CheatGhost() )
	{
		bCheatFlying = true;
		Outer.GotoState('PlayerFlying');
	}
	else
	{
		bCollideWorld = false;
	}

	ClientMessage("You feel ethereal");
}

/* AllAmmo
	Sets maximum ammo on all weapons
*/
exec function AllAmmo();

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

exec function Slomo( float T )
{
	WorldInfo.Game.SetGameSpeed(T);
}

exec function SetJumpZ( float F )
{
	Pawn.JumpZ = F;
}

exec function SetGravity( float F )
{
	WorldInfo.WorldGravityZ = F;
}

exec function SetSpeed( float F )
{
	Pawn.GroundSpeed = Pawn.Default.GroundSpeed * f;
	Pawn.WaterSpeed = Pawn.Default.WaterSpeed * f;
}

exec function KillAll(class<actor> aClass)
{
	local Actor A;
`if(`notdefined(FINAL_RELEASE))
	local PlayerController PC;

	foreach WorldInfo.AllControllers(class'PlayerController', PC)
	{
		PC.ClientMessage("Killed all "$string(aClass));
	}
`endif

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
	local Pawn			P, TargetPawn, FirstPawn, OldPawn;
	local bool			bPickNextPawn;

	Foreach DynamicActors(class'Pawn', P)
	{
		if( P == Pawn )
		{
			bPickNextPawn = TRUE;
		}
		else if( P.IsA(ClassName) )
		{
			if( FirstPawn == None )
			{
				FirstPawn = P;
			}

			if( bPickNextPawn )
			{
				TargetPawn = P;
				break;
			}
		}
	}

	// if we went through the list without choosing a pawn, pick first available choice (loop)
	if( TargetPawn == None )
	{
		TargetPawn = FirstPawn;
	}

	if( TargetPawn != None )
	{
		// detach TargetPawn from its controller and kill its controller.
		TargetPawn.DetachFromController( TRUE );

		// detach player from current pawn and possess targetpawn
		if( Pawn != None )
		{
			OldPawn = Pawn;
			Pawn.DetachFromController();
		}

		Possess(TargetPawn, FALSE);

		// Spawn default controller for our ex-pawn (AI)
		if( OldPawn != None )
		{
			OldPawn.SpawnDefaultController();
		}
	}
	else
	{
		`log("Avatar: Couldn't find any Pawn to possess of class '" $ ClassName $ "'");
	}
}

exec function Summon( string ClassName )
{
	local class<actor> NewClass;
	local vector SpawnLoc;

	`log( "Fabricate " $ ClassName );
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

/**
 * Give a specified weapon to the Pawn.
 * If weapon is not carried by player, then it is created.
 * Weapon given is returned as the function's return parmater.
 */
exec function Weapon GiveWeapon( String WeaponClassStr )
{
	Local Weapon		Weap;
	local class<Weapon> WeaponClass;

	WeaponClass = class<Weapon>(DynamicLoadObject(WeaponClassStr, class'Class'));
	Weap		= Weapon(Pawn.FindInventoryType(WeaponClass));
	if( Weap != None )
	{
		return Weap;
	}
	return Weapon(Pawn.CreateInventory( WeaponClass ));
}

exec function PlayersOnly()
{
	if (WorldInfo.bPlayersOnly || WorldInfo.bPlayersOnlyPending)
	{
		WorldInfo.bPlayersOnly = false;
		WorldInfo.bPlayersOnlyPending = false;
	}
	else
	{
		WorldInfo.bPlayersOnlyPending = !WorldInfo.bPlayersOnlyPending;
		// WorldInfo.bPlayersOnly is set after next tick of UWorld::Tick
	}	
}

exec function SuspendAI()
{
	WorldInfo.bSuspendAI = !WorldInfo.bSuspendAI;
}

/** Util for fracturing meshes within an area of the player. */
exec function DestroyFractures(optional float Radius)
{
	local FracturedStaticMeshActor FracActor;

	if(Radius == 0.0)
	{
		Radius = 256.0;
	}

	foreach CollidingActors(class'FracturedStaticMeshActor', FracActor, Radius, Pawn.Location, TRUE)
	{
		if(FracActor.Physics == PHYS_None)
		{
			// Make sure the impacted fractured mesh is visually relevant
			FracActor.BreakOffPartsInRadius(Pawn.Location, Radius, 500.0, TRUE);
		}
	}
}

/** Util for ensuring at least one piece is broken of each FSM in level */
exec function FractureAllMeshes()
{
	local FracturedStaticMeshActor FracActor;

	foreach AllActors(class'FracturedStaticMeshActor', FracActor)
	{
		FracActor.HideOneFragment();
	}
}

/** This will break all Fractured meshes in the map in a way to maximize memory usage **/
exec function FractureAllMeshesToMaximizeMemoryUsage()
{
	local FracturedStaticMeshActor FracActor;

	foreach AllActors(class'FracturedStaticMeshActor', FracActor)
	{
		FracActor.HideFragmentsToMaximizeMemoryUsage();
	}
}



// ***********************************************************
// Navigation Aids (for testing)

// remember spot for path testing (display path using ShowDebug)
exec function RememberSpot()
{
	if ( Pawn != None )
		SetDestinationPosition( Pawn.Location );
	else
		SetDestinationPosition( Location );
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

	foreach WorldInfo.AllControllers(class'Controller', P)
	{
		if ( P.bIsPlayer && (P.PlayerReplicationInfo.PlayerName ~= S ) )
		{
			break;
		}
	}

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

exec function ViewBot()
{
	local actor first;
	local bool bFound;
	local AIController C;

	foreach WorldInfo.AllControllers(class'AIController', C)
	{
		if (C.Pawn != None && C.PlayerReplicationInfo != None)
		{
			if (bFound || first == None)
			{
				first = C;
				if (bFound)
				{
					break;
				}
			}
			if (C.PlayerReplicationInfo == RealViewTarget)
			{
				bFound = true;
			}
		}
	}

	if ( first != None )
	{
		`log("view "$first);
		SetViewTarget(first);
		SetCameraMode( 'ThirdPerson' );
		FixFOV();
	}
	else
		ViewSelf(true);
}

exec function ViewClass( class<actor> aClass )
{
	local actor other, first;
	local bool bFound;

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
		if ( Pawn(first) != None )
			ClientMessage(ViewingFrom@First.GetHumanReadableName(), 'Event');
		else
			ClientMessage(ViewingFrom@first, 'Event');
		SetViewTarget(first);
		FixFOV();
	}
	else
		ViewSelf(false);
}

exec function Loaded()
{
	if( WorldInfo.Netmode!=NM_Standalone )
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

/** streaming level debugging */

function SetLevelStreamingStatus(name PackageName, bool bShouldBeLoaded, bool bShouldBeVisible)
{
	local PlayerController PC;
	local int i;

	if (PackageName != 'All')
	{
		foreach WorldInfo.AllControllers(class'PlayerController', PC)
		{
			PC.ClientUpdateLevelStreamingStatus(PackageName, bShouldBeLoaded, bShouldBeVisible, FALSE );
		}
	}
	else
	{
		foreach WorldInfo.AllControllers(class'PlayerController', PC)
		{
			for (i = 0; i < WorldInfo.StreamingLevels.length; i++)
			{
				PC.ClientUpdateLevelStreamingStatus(WorldInfo.StreamingLevels[i].PackageName, bShouldBeLoaded, bShouldBeVisible, FALSE );
			}
		}
	}
}

exec function StreamLevelIn(name PackageName)
{
	SetLevelStreamingStatus(PackageName, true, true);
}

exec function OnlyLoadLevel(name PackageName)
{
	SetLevelStreamingStatus(PackageName, true, false);
}

exec function StreamLevelOut(name PackageName)
{
	SetLevelStreamingStatus(PackageName, false, false);
}

exec function TestLevel()
{
	local Actor A, Found;
	local bool bFoundErrors;

	ForEach AllActors(class'Actor', A)
	{
		bFoundErrors = bFoundErrors || A.CheckForErrors();
		if ( bFoundErrors && (Found == None) )
			Found = A;
	}

	if ( bFoundErrors )
	{
		`log("Found problem with "$Found);
		assert(false);
	}
}

`if(`notdefined(FINAL_RELEASE))
/**
 * Logs the current session state for the game type and online layer
 */
exec function DumpOnlineSessionState()
{
	local int PlayerIndex;

	if (WorldInfo.NetMode != NM_Client)
	{
		`Log("");
		`Log("GameInfo state");
		`Log("-------------------------------------------------------------");
		`Log("");
		// Log game info data
		`Log("Class: "$WorldInfo.Game.Class.Name);
		// Log player count information
		`Log("  MaxPlayersAllowed: "$WorldInfo.Game.MaxPlayersAllowed);
		`Log("  MaxPlayers: "$WorldInfo.Game.MaxPlayers);
		`Log("  NumPlayers: "$WorldInfo.Game.NumPlayers);
		`Log("  MaxSpectatorsAllowed: "$WorldInfo.Game.MaxSpectatorsAllowed);
		`Log("  MaxSpectators: "$WorldInfo.Game.MaxSpectators);
		`Log("  NumSpectators: "$WorldInfo.Game.NumSpectators);
		`Log("  NumBots: "$WorldInfo.Game.NumBots);

		`Log("  bUseSeamlessTravel: "$WorldInfo.Game.bUseSeamlessTravel);
		`Log("  bRequiresPushToTalk: "$WorldInfo.Game.bRequiresPushToTalk);
		`Log("  bHasNetworkError: "$WorldInfo.Game.bHasNetworkError);

		`Log("  OnlineGameSettingsClass: "$WorldInfo.Game.OnlineGameSettingsClass);
		`Log("  OnlineStatsWriteClass: "$WorldInfo.Game.OnlineStatsWriteClass);

		`Log("  bUsingArbitration: "$WorldInfo.Game.bUsingArbitration);
		if (WorldInfo.Game.bUsingArbitration)
		{
			`Log("  bHasArbitratedHandshakeBegun: "$WorldInfo.Game.bHasArbitratedHandshakeBegun);
			`Log("  bNeedsEndGameHandshake: "$WorldInfo.Game.bNeedsEndGameHandshake);
			`Log("  bIsEndGameHandshakeComplete: "$WorldInfo.Game.bIsEndGameHandshakeComplete);
			`Log("  bHasEndGameHandshakeBegun: "$WorldInfo.Game.bHasEndGameHandshakeBegun);
			`Log("  ArbitrationHandshakeTimeout: "$WorldInfo.Game.ArbitrationHandshakeTimeout);
			`Log("  Number of pending arbitration PCs: "$WorldInfo.Game.PendingArbitrationPCs.Length);
			// List who we are waiting of for arbitration
			for (PlayerIndex = 0; PlayerIndex < WorldInfo.Game.PendingArbitrationPCs.Length; PlayerIndex++)
			{
				`Log("    Player: "$WorldInfo.Game.PendingArbitrationPCs[PlayerIndex].PlayerReplicationInfo.PlayerName$" PC ("$WorldInfo.Game.PendingArbitrationPCs[PlayerIndex].Name$")");
			}
			`Log("  Number of arbitration PCs: "$WorldInfo.Game.ArbitrationPCs.Length);
			// List all of the players that have completed arbitration
			for (PlayerIndex = 0; PlayerIndex < WorldInfo.Game.ArbitrationPCs.Length; PlayerIndex++)
			{
				`Log("    Player: "$WorldInfo.Game.ArbitrationPCs[PlayerIndex].PlayerReplicationInfo.PlayerName$" PC ("$WorldInfo.Game.ArbitrationPCs[PlayerIndex].Name$")");
			}
		}
	}
	// Log PRI player info
	DebugLogPRIs();
	// Log the online session state
	if (OnlineSub != None)
	{
		OnlineSub.DumpSessionState();
	}
}
`endif

/**
 * Changes the OS specific logging level
 *
 * @param DebugLevel the new debug level to use
 */
exec function SetOnlineDebugLevel(int DebugLevel)
{
	if (OnlineSub != None)
	{
		OnlineSub.SetDebugSpewLevel(DebugLevel);
	}
}

/**
 * tries to path from the player's current position to the position the player is looking at 
 *
 */
exec function TestNavMeshPath(optional bool bDrawPath=TRUE)
{
	local actor HitActor;
	local vector HitLoc,HitNorm, Start, End;
	local rotator Rot;

	if(NavigationHandle == none)
	{
		NavigationHandle = new(outer) class'NavigationHandle';
	}

	GetPlayerViewPoint(Start,Rot);
	End = Start + vector(rot) * 10000;

	HitActor = Trace(HitLoc,HitNorm,End,Start,false);
	if(HitActor != none)
	{ 
		class'NavmeshPath_Toward'.static.TowardPoint(NavigationHandle,HitLoc);
		class'NavMeshGoal_At'.static.AtLocation(NavigationHandle,HitLoc);
		
		NavigationHandle.bDebugConstraintsAndGoalEvals=true;
		NavigationHandle.bUltraVerbosePathDebugging=TRUE;
		if(NavigationHandle.FindPath())
		{
			DrawDebugLine(HitLoc,Start,0,255,0,TRUE);
			DrawDebugCoordinateSystem(HitLoc,rot(0,0,0),25.f,TRUE);
			if(bDrawPath)
			{
				NavigationHandle.DrawPathCache(,true);
			}
		}
		else
		{
			DrawDebugLine(HitLoc,Start,255,0,0,TRUE);
			DrawDebugCoordinateSystem(HitLoc,rot(0,0,0),25.f,TRUE);
			DrawDebugBox(Pawn.Location,Pawn.GetCollisionExtent(),255,0,0,TRUE);
		}
	}
}

exec function TestPylonConnectivity()
{
	local Pylon Py;
	foreach AllActors(class'Pylon',Py)
	{
		PY.VerifyTopLevelConnections();
	}
}

exec function VerbosePathDebug()
{
	local vector HitLoc,HitNorm, Start, End;
	local rotator Rot;
	local Pawn P;

	GetPlayerViewPoint(Start,Rot);
	End = Start + vector(rot) * 10000;

	foreach TraceActors(class'Pawn',P,HitLoc,HitNorm,End,Start,vect(1,1,1))
	{
		Pawn.MessagePlayer("Verbosepathdebug trace hit"@P);
		if(P != none && P.Controller != none)
		{
			P.Controller.NavigationHandle.bUltraVerbosePathDebugging=!P.Controller.NavigationHandle.bUltraVerbosePathDebugging;
		}
	}
}

/** This is not an actor, so we need a stand in for PostBeginPlay */
function InitCheatManager();


/**
 * This will have all PlaySound function calls emit a warnf so you can see that name of 
 * the soundcue being played.
 **/
exec native function LogPlaySoundCalls( bool bShouldLog );


/**
* This will have all ActivateSystem function calls emit a warnf so you can see that name of 
* the particlesystem being played.
**/
exec native function LogParticleActivateSystemCalls( bool bShouldLog );

/**
 * debug command, verifies that all path objects and path obstacls are valid 
 * (E.G.) that they haven't been deleted, but left registered
 */
exec native function VerifyNavMeshObjects();

/**
 * debug command, will draw all edges that are not supported for the passed pawn class
 */
exec native function DrawUnsupportingEdges(coerce string PawnClassName);

/**
 * enables a timer to do periodic navmesh verification
 */
exec function NavMeshVerification(float interval=0.5)
{
	if(interval < 0)
	{
		ClearTimer(nameof(VerifyNavMeshObjects),outer);
	}
	else
	{
		SetTimer(interval,true,nameof(VerifyNavMeshObjects),outer);
	}
}

/**
 * debug command, prints all navmesh pathobject edges
 */
exec native function PrintAllPathObjectEdges();

/**
 * debug command, prints all active navmesh obstaces
 */
exec native function PrintNavMeshObstacles();

/**
 * debug command, verifies all cover references
 */
exec native function VerifyNavMeshCoverRefs();

/**
 * toggles AI logging
 */
exec function ToggleAILogging()
{
	local Engine Eng;
	Eng = class'Engine'.static.GetEngine();
	if(Pawn != none)
	{
		if( Eng.bDisableAILogging )
		{
			Pawn.MessagePlayer("OK! AI logging is now ON");
		}
		else
		{
			Pawn.MessagePlayer("OK! AI logging is now OFF");
		}
	}

	Eng.bDisableAILogging = !Eng.bDisableAILogging;

}

exec function DebugIniLocPatcher()
{
	if (OnlineSub != None &&
		OnlineSub.Patcher != None)
	{
		OnlineSub.Patcher.DownloadFiles();
	}
}

exec function DebugDownloadTitleFile(string Filename, optional bool bFromCache)
{
	if (OnlineSub != None)
	{
		if (bFromCache)
		{
			if (OnlineSub.TitleFileCacheInterface != None)
			{
				`Log(`location @ "starting file load for"@Filename);

				OnlineSub.TitleFileCacheInterface.AddLoadTitleFileCompleteDelegate(OnLoadComplete);
				OnlineSub.TitleFileCacheInterface.LoadTitleFile(Filename);
			}
			else
			{
				`Log(`location @ "OnlineTitleFileCacheInterface not supported");
			}
		}
		else
		{
			if (OnlineSub.TitleFileInterface != None)
			{
				`Log(`location @ "starting file download request for"@Filename);

				OnlineSub.TitleFileInterface.AddReadTitleFileCompleteDelegate(OnDownloadComplete);
				OnlineSub.TitleFileInterface.ReadTitleFile(Filename);
			}
			else
			{
				`Log(`location @ "OnlineTitleFileInterface not supported");
			}
		}
	}
}
function OnDownloadComplete(bool bWasSuccessful,string Filename)
{
	OnlineSub.TitleFileInterface.ClearReadTitleFileCompleteDelegate(OnDownloadComplete);

	`Log(`location @ "download completed"
		@"bWasSuccessful="$bWasSuccessful
		@"FileName="$Filename);

	if (bWasSuccessful)
	{
		DebugSaveTitleFile(Filename);
	}
}
function OnLoadComplete(bool bWasSuccessful,string FileName)
{
	OnlineSub.TitleFileCacheInterface.ClearLoadTitleFileCompleteDelegate(OnLoadComplete);

	`Log(`location @ "load completed"
		@"bWasSuccessful="$bWasSuccessful
		@"FileName="$Filename);

	DebugDownloadTitleFile(FileName,false);
}

exec function DebugSaveTitleFile(string Filename)
{
	local array<byte> FileContents;

	if (OnlineSub != None)
	{
		if (OnlineSub.TitleFileInterface != None)
		{
			if (OnlineSub.TitleFileInterface.GetTitleFileContents(Filename, FileContents))
			{
				`Log(`location @ "found file in download cache. using file contents from download cache:"@Filename);						
			}
			else
			{
				`Log(`location @ "couldn't find file in download cache:"@Filename);						
			}
		}
		else
		{
			`Log(`location @ "OnlineTitleFileInterface not supported");
		}

		if (OnlineSub.TitleFileCacheInterface != None)
		{
			`Log(`location @ "starting file save for"@Filename);

			OnlineSub.TitleFileCacheInterface.AddSaveTitleFileCompleteDelegate(OnSaveComplete);
			OnlineSub.TitleFileCacheInterface.SaveTitleFile(Filename, "TestName.ini", FileContents);		
		}
		else
		{
			`Log(`location @ "OnlineTitleFileCacheInterface not supported");
		}
	}
}
function OnSaveComplete(bool bWasSuccessful,string FileName)
{
	OnlineSub.TitleFileCacheInterface.ClearSaveTitleFileCompleteDelegate(OnSaveComplete);

	`Log(`location @ "save completed"
		@"bWasSuccessful="$bWasSuccessful
		@"FileName="$Filename);
}

exec function DebugDeleteTitleFiles()
{
	if (OnlineSub != None)
	{
		if (OnlineSub.TitleFileCacheInterface != None)
		{
			`Log(`location @ "deleting all title files in cache dir");
			if (OnlineSub.TitleFileCacheInterface.DeleteTitleFiles(0))
			{
				`Log(`location @ "delete succeeded");
			}
			else
			{
				`Log(`location @ "cant delete. file ops in progress");
			}		
		}
		else
		{
			`Log(`location @ "OnlineTitleFileCacheInterface not supported");
		}
	}
}

exec function DebugEmsDownload()
{
	if (OnlineSub != None &&
		OnlineSub.Patcher != None)
	{
		OnlineSub.Patcher.DownloadFiles();
	}
}

/**
 * debug command which prints out stats about memory usage by covernodes
 */
exec native function DumpCoverStats();

exec function DrawLocation(vector Loc)
{
	DrawDebugCoordinateSystem(Loc,rot(0,0,0),50.f,TRUE);
}

exec function DrawLocationXYZ(float X, Float Y, float Z)
{
	local vector DrawSpot;

	DrawSpot.X = X;
	DrawSpot.y = y;
	DrawSpot.z = z;
	DrawDebugCoordinateSystem(DrawSpot,rot(0,0,0),150.f,TRUE);
}

/**
 * Debug exec for scheduling a push notification
 *
 * @param MessageBody string to display in message box of notification
 * @param SecondsFromNow seconds to elapse before triggering the notification
 */
exec function DebugNotification(string MessageBody, int SecondsFromNow)
{
	local AppNotificationsBase AppNotification;
	local NotificationInfo NotificationInfo;
	local NotificationMessageInfo MessageInfo;

	AppNotification = class'PlatformInterfaceBase'.static.GetAppNotificationsInterface();
	if (AppNotification != None)
	{
		NotificationInfo.BadgeNumber = 1;
		NotificationInfo.MessageBody = MessageBody;

		MessageInfo.Key = "test key 1";
		MessageInfo.Value = "test val 1";
		NotificationInfo.MessageInfo.AddItem(MessageInfo);

		MessageInfo.Key = "test key 2";
		MessageInfo.Value = "test val 2";
		NotificationInfo.MessageInfo.AddItem(MessageInfo);

		`log(`location@""
			@" MessageBody="$MessageBody
			@" SecondsFromNow="$SecondsFromNow);

		AppNotification.OnReceivedLocalNotification = OnReceivedLocalNotificationDebug;
		AppNotification.ScheduleLocalNotification(NotificationInfo,SecondsFromNow);
	}
}
/**
 * Debug params from a local notification that was processed
 */
private function OnReceivedLocalNotificationDebug(const out NotificationInfo Notification, bool bWasAppActive)
{
	`log(`location@"bWasAppActive="$bWasAppActive);
	class'PlatformInterfaceBase'.static.GetAppNotificationsInterface().DebugLogNotification(Notification);
}

exec function DebugQueryUserFiles(string UserId)
{
	if (OnlineSub != None &&
		OnlineSub.UserCloudInterface != None)
	{
		OnlineSub.UserCloudInterface.AddEnumerateUserFileCompleteDelegate(OnEnumerateUserFilesComplete);
		OnlineSub.UserCloudInterface.EnumerateUserFiles(UserId);
	}
}

private function OnEnumerateUserFilesComplete(bool bWasSuccessful,string UserId)
{
	OnlineSub.UserCloudInterface.ClearEnumerateUserFileCompleteDelegate(OnEnumerateUserFilesComplete);

	ConsoleCommand("obj dump" @OnlineSub.UserCloudInterface.Name);

	`log(`location@""
		$" bWasSuccessful="$bWasSuccessful
		$" UserId="$UserId);
}

exec function DebugWriteUserFile(string UserId, string FileName)
{
	local int Idx;
	local array<byte> FileContents;

	if (OnlineSub != None &&
		OnlineSub.UserCloudInterface != None)
	{
		for (Idx=0; Idx < 1000; Idx++)
		{
			FileContents[Idx]=Idx;
		}
		OnlineSub.UserCloudInterface.AddWriteUserFileCompleteDelegate(OnWriteUserFileComplete);
		OnlineSub.UserCloudInterface.WriteUserFile(UserId,FileName,FileContents);
	}
}

private function OnWriteUserFileComplete(bool bWasSuccessful,string UserId,string FileName)
{
	OnlineSub.UserCloudInterface.ClearWriteUserFileCompleteDelegate(OnWriteUserFileComplete);

	ConsoleCommand("obj dump" @OnlineSub.UserCloudInterface.Name);

	`log(`location@""
		$" bWasSuccessful="$bWasSuccessful
		$" UserId="$UserId
		$" FileName="$FileName);
}

exec function DebugReadUserFile(string UserId, string FileName)
{
	if (OnlineSub != None &&
		OnlineSub.UserCloudInterface != None)
	{
		OnlineSub.UserCloudInterface.AddReadUserFileCompleteDelegate(OnReadUserFileComplete);
		OnlineSub.UserCloudInterface.ReadUserFile(UserId,FileName);
	}
}

private function OnReadUserFileComplete(bool bWasSuccessful,string UserId,string FileName)
{
	local array<byte> FileContents;

	OnlineSub.UserCloudInterface.ClearReadUserFileCompleteDelegate(OnReadUserFileComplete);
	OnlineSub.UserCloudInterface.GetFileContents(UserId,FileName,FileContents);

	ConsoleCommand("obj dump" @OnlineSub.UserCloudInterface.Name);

	`log(`location@""
		$" bWasSuccessful="$bWasSuccessful
		$" UserId="$UserId
		$" FileName="$FileName
		$" FileContents="$FileContents.Length);
}

exec function DebugDeleteUserFile(string UserId, string FileName)
{
	if (OnlineSub != None &&
		OnlineSub.UserCloudInterface != None)
	{
		OnlineSub.UserCloudInterface.AddDeleteUserFileCompleteDelegate(OnDeleteUserFileComplete);
		OnlineSub.UserCloudInterface.DeleteUserFile(UserId,FileName,true,true);
	}
}

private function OnDeleteUserFileComplete(bool bWasSuccessful,string UserId,string FileName)
{
	OnlineSub.UserCloudInterface.ClearDeleteUserFileCompleteDelegate(OnDeleteUserFileComplete);

	ConsoleCommand("obj dump" @OnlineSub.UserCloudInterface.Name);

	`log(`location@""
		$" bWasSuccessful="$bWasSuccessful
		$" UserId="$UserId
		$" FileName="$FileName);
}

defaultproperties
{
}

/**
 * Simple function to illustrate the use of the HttpRequest system.
 */
exec function TestHttp(string Verb, string Payload, string URL, optional bool bSendParallelRequest)
{
	local HttpRequestInterface R;

	// create the request instance using the factory (which handles
	// determining the proper type to create based on config).
	R = class'HttpFactory'.static.CreateRequest();
	// always set a delegate instance to handle the response.
	R.OnProcessRequestComplete = OnRequestComplete;
	`log("Created request");
	// you can make many requests from one request object.
	R.SetURL(URL);
	// Default verb is GET
	if (Len(Verb) > 0)
	{
		R.SetVerb(Verb);
	}
	else
	{
		`log("No Verb given, using the defaults.");
	}
	// Default Payload is empty
	if (Len(Payload) > 0)
	{
		R.SetContentAsString(Payload);
	}
	else
	{
		`log("No payload given.");
	}
	`log("Creating request for URL:"@URL);

	// there is currently no way to distinguish keys that are empty from keys that aren't there.
	`log("Key1 ="@R.GetURLParameter("Key1"));
	`log("Key2 ="@R.GetURLParameter("Key2"));
	`log("Key3NoValue ="@R.GetURLParameter("Key3NoValue"));
	`log("NonexistentKey ="@R.GetURLParameter("NonexistentKey"));
	// A header will not necessarily be present if you don't set one. Platform implementations
	// may add things like Content-Length when you send the request, but won't necessarily
	// be available in the Header.
	`log("NonExistentHeader ="@R.GetHeader("NonExistentHeader"));
	`log("CustomHeaderName ="@R.GetHeader("CustomHeaderName"));
	`log("ContentType ="@R.GetContentType());
	`log("ContentLength ="@R.GetContentLength());
	`log("URL ="@R.GetURL());
	`log("Verb ="@R.GetVerb());

	// multiple ProcessRequest calls can be made from the same instance if desired.
	if (!R.ProcessRequest())
	{
		`log("ProcessRequest failed. Unsuppress DevHttpRequest to see more details.");
	}
	else
	{
		`log("Request sent");
	}
	// send off a parallel request for testing.
	if (bSendParallelRequest)
	{
		if (!class'HttpFactory'.static.CreateRequest()
			.SetURL("http://www.epicgames.com")
			.SetVerb("GET")
			.SetHeader("Test", "Value")
			.SetProcessRequestCompleteDelegate(OnRequestComplete)
			.ProcessRequest())
		{
			`log("ProcessRequest for parallel request failed. Unsuppress DevHttpRequest to see more details.");
		}
		else
		{
			`log("Parallel Request sent");
		}
	}
}


/** Delegate to use for HttpResponses. */
function OnRequestComplete(HttpRequestInterface OriginalRequest, HttpResponseInterface Response, bool bDidSucceed)
{
	local array<String> Headers;
	local String Header;
	local String Payload;
	local int PayloadIndex;

	`log("Got response!!!!!!! Succeeded="@bDidSucceed);
	`log("URL="@OriginalRequest.GetURL());
	// if we didn't succeed, we can't really trust the payload, so you should always really check this.
	if (Response != None)
	{
		`log("ResponseURL="@Response.GetURL());
		`log("Response Code="@Response.GetResponseCode());
		Headers = Response.GetHeaders();
		foreach Headers(Header)
		{
			`log("Header:"@Header);
		}
		// GetContentAsString will make a copy of the payload to add the NULL terminator,
		// then copy it again to convert it to TCHAR, so this could be fairly inefficient.
		// This call also assumes the payload is UTF8 right now, as truly determining the encoding
		// is content-type dependent.
		// You also can't trust the content-length as you don't always get one. You should instead
		// always trust the length of the content payload you receive.
		Payload = Response.GetContentAsString();
		if (Len(Payload) > 1024)
		{
			PayloadIndex = 0;
			`log("Payload:");
			while (PayloadIndex < Len(Payload))
			{
				`log("    "@Mid(Payload, PayloadIndex, 1024));
				PayloadIndex = PayloadIndex + 1024;
			}
		}
		else
		{
			`log("Payload:"@Payload);
		}
	}
}

/**
 * Debug function to send an arbitrary analytics event.
 */
exec function SendAnalyticsEvent(string EventName, optional string AttributeName, optional string AttributeValue)
{
	local AnalyticEventsBase Analytics;

	Analytics = class'PlatformInterfaceBase'.static.GetAnalyticEventsInterface();
	if (Len(AttributeName) > 0)
	{
		Analytics.LogStringEventParam(EventName, AttributeName, AttributeValue, false);
	}
	else
	{
		Analytics.LogStringEvent(EventName, false);
	}
}

/**
 * Debug function to test analytic user events
 */
exec function SendAnalyticsUserAttributeEvent(string AttributeName, string AttributeValue)
{
	class'PlatformInterfaceBase'.static.GetAnalyticEventsInterface().LogUserAttributeUpdate(AttributeName, AttributeValue);
}

/**
 * Debug function to test analytic events
 */
exec function SendAnalyticsItemPurchaseEvent(string ItemId, string Currency, int PerItemCost, int ItemQuantity)
{
	class'PlatformInterfaceBase'.static.GetAnalyticEventsInterface().LogItemPurchaseEvent(ItemId, Currency, PerItemCost, ItemQuantity);
}

/**
 * Debug function to test analytic events
 */
exec function SendAnalyticsCurrencyPurchaseEvent(string GameCurrencyType, int GameCurrencyAmount, string RealCurrencyType, float RealMoneyCost, string PaymentProvider)
{
	class'PlatformInterfaceBase'.static.GetAnalyticEventsInterface().LogCurrencyPurchaseEvent(GameCurrencyType, GameCurrencyAmount, RealCurrencyType, RealMoneyCost, PaymentProvider);
}

/**
 * Debug function to test analytic events
 */
exec function SendAnalyticsCurrencyGivenEvent(string GameCurrencyType, int GameCurrencyAmount)
{
	class'PlatformInterfaceBase'.static.GetAnalyticEventsInterface().LogCurrencyGivenEvent(GameCurrencyType, GameCurrencyAmount);
}

/**
 * Debug function to test analytic events
 */
exec function SendAnalyticsCachedEvents()
{
	class'PlatformInterfaceBase'.static.GetAnalyticEventsInterface().SendCachedEvents();
}

/**
 * Debug function to test analytic events
 */
exec function SetAnalyticsUserId(string UserId)
{
	class'PlatformInterfaceBase'.static.GetAnalyticEventsInterface().SetUserId(UserId);
	`log("Analytics UserId set to:"@UserId);
}

/**
 * Debug function to test analytic events
 */
exec native function GetAnalyticsUserId();

/**
 * Debug function to test analytic events
 */
exec function AnalyticsStartSession()
{
	class'PlatformInterfaceBase'.static.GetAnalyticEventsInterface().StartSession();
}

/**
 * Debug function to test analytic events
 */
exec function AnalyticsEndSession()
{
	class'PlatformInterfaceBase'.static.GetAnalyticEventsInterface().EndSession();
}
