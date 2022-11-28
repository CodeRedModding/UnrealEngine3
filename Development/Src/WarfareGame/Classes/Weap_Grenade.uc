/** 
 * Weap_Grenade
 * Warfare grenade weapon implementation
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

class Weap_Grenade extends WarWeapon
	abstract;

/** Class of grenade projectile to use */
var()	class<Proj_Grenade>		GrenadeProjectileClass;

/** Cooking sound */
var()	SoundCue				CookingSound;

/** defines if player is 'cooking' grenade. Means pin is pulled, but player keeps grenade in hand to reduce lifespan */
var		bool					bCooking;

/**	Time grenade has been cooking... in seconds */
var		float					CookedTime;

// debugging
var bool					bFreezeArc;
var vector					StartLoc, AimDir;

/** Focus Point to adjust camera */
var	transient	vector	FocusPoint;

/** @see Weapon::GetPhysicalFireStartLoc() */
simulated function Vector GetPhysicalFireStartLoc()
{
	local vector				StartLoc, CamLoc, Offset;
	local WarPC.ECoverDirection	CoverDir;
	local rotator				CamRot;
	local float					Dist;

	// adjust based on cover direction
	StartLoc	= Instigator.GetPawnViewLocation();
	CoverDir	= WarPC(Instigator.Controller).GetCoverDirection();
	Dist		= 48;

	switch( CoverDir )
	{
		case CD_Up		: Offset = vect(0,0,1);		break;
		case CD_Left	: Offset = vect(0,-1,0);	break;
		case CD_Right	: Offset = vect(0,1,0);		break;
	}

	Instigator.Controller.GetPlayerViewPoint( CamLoc, CamRot );

	return (StartLoc + ((Offset*Dist) >> CamRot));
}

/**
 * returns grenade class to throw/spawn. Used also by simulation to get proper default values/effects.
 *
 * @return	projectile class of grenade to throw.
 */
simulated function class<Proj_Grenade> GetGrenadeClass()
{
	return GrenadeProjectileClass;
}

/**
 * Player cooked grenade, but failed to throw it in time...
 * Grenade explodes in player's hand
 */
function EndOfCooking()
{
	GetGrenadeClass().static.StaticExplosion( Instigator, Instigator.Location, vect(0,0,1) );
}

/**
 * freeze grenade arc and grenade projectiles. For debugging simulated and real trajectories.
 */
exec function nadefreeze()
{
	local Proj_Grenade	GrenadeProj;

	bFreezeArc			= !bFreezeArc;

	foreach DynamicActors(class'Proj_Grenade', GrenadeProj)
	{
		if ( GrenadeProj != None )
		{
			GrenadeProj.bFreeze = bFreezeArc;
			if ( bFreezeArc )
				GrenadeProj.Lifespan = 9999999;
			else
				GrenadeProj.Lifespan = GrenadeProj.default.Lifespan;
		}
	}
}

/**
 * Render Grenade Arc
 * performs a simulation of the grenade trajectory
 * @note	WIP, not optimized
 *
 * @param	deltatime, provides a base time step for rendering
 * @param	StartLoc, location when the grenade is spawned
 * @param	AimDir, direction of grenade when spawned
 */
simulated function RenderThrowingArc( vector StartLoc, vector AimDir )
{
	local Vector				StartLocation;
	local Vector				DestLocation;
	local Vector				InterStartLoc;
	local Vector				SimVelocity;
	local Vector				HitLocation, HitNormal;
	local float					fTimeStep;
	local float					fInterpTime;
	local float					fTimeCount;
	local class<Proj_Grenade>	GrenadeClass;
	local Actor					HitActor;
	local int					BounceCount, InterpBounceCount;

	// Debug
	local int					NumIterations;

	GrenadeClass	= GetGrenadeClass();
	StartLocation	= StartLoc;
	fTimeStep		= GrenadeClass.default.LifeSpan / 100.f;
	SimVelocity		= GrenadeClass.default.Speed * AimDir;

	while ( fTimeCount < (GrenadeClass.default.LifeSpan - CookedTime) && 
			VSize(SimVelocity) > 10.f && 
			BounceCount < GrenadeClass.default.MaxBounceCount )
	{
		// FIXEME: PhysicsVolume is where the player is, not where the simulated grenade would be
		SimVelocity += GrenadeClass.default.fGravityScale * Instigator.PhysicsVolume.Gravity * fTimeStep;
		
		InterStartLoc		= StartLocation;
		fInterpTime			= fTimeStep;
		InterpBounceCount	= 0;
		while ( fInterpTime > 0.f && InterpBounceCount < 2 )
		{
			DestLocation	= InterStartLoc + SimVelocity * fTimeStep;

			// Check if we hit something
			HitActor = Trace(HitLocation, HitNormal, DestLocation, StartLocation, true, vect(0,0,0));
			if ( GrenadeClass.static.ShoundBounce( HitActor, Instigator, BounceCount ) )
			{
				//log("bounce! HitActor:"  $ HitActor );
				GrenadeClass.static.UpdateBounceVelocity( SimVelocity, HitLocation, HitNormal );
				fInterpTime *= 1.f - VSize(HitLocation - StartLocation) / VSize(DestLocation - StartLocation);
				InterStartLoc = HitLocation;
				BounceCount++;
				InterpBounceCount++;
				if ( InterpBounceCount == 2 )
				{
					DestLocation = HitLocation;
				}
			}
			else
			{
				fInterpTime = 0.f;
			}
		}

		DrawDebugLine(StartLocation, DestLocation, 255, 0, 0 );
		
		StartLocation = DestLocation;
		fTimeCount += fTimeStep;
		NumIterations++;
	}

	FocusPoint = DestLocation;
}

/** returns focus point to adjust camera */
simulated function vector GetFocusPoint()
{
	return vect(0,0,0);
}

/**
 * Returns Aim rotation to toss the grenade. (for spawning and simulation).
 * FIXME, should work the same on both client and server.
 *
 * @param	StartFireLoc	world location when grenade would be spawned.
 * @return					Aim direction to use when spawning the grenade.
 */
simulated function vector GetGrenadeTossDirection( Vector StartFireLoc )
{
	local Rotator	AimRot;
	local float		fPitch;

	// use controller rotation, and not camera, as camera rotation is adjusted to show grenade's destination.
	AimRot = Instigator.Controller.Rotation;
	/*
	if ( Role == Role_Authority )
	{
		AimRot = GetAdjustedAim( StartFireLoc );
	}
	else
	{
		AimRot = Instigator.GetBaseAimRotation();
	}
	*/

	// because player input was divided by half to limit camera angle, we compensate here, so player has same throwing pitch range
	fPitch			= FNormalizedRotAxis( AimRot.Pitch );
	fPitch			*= 2.f;
	AimRot.Pitch	= fPitch;

	return vector(AimRot);
}
/**
 * State Ready
 * State when the player is in 'ready position', holding fire and ready to throw a grenade
 */
state Ready
{
	/**
	 * returns grenade projectile to spawn
	 */
	function class<Projectile> GetProjectileClass()
	{
		return GrenadeProjectileClass;
	}

	/**
	 * Fires a projectile 
	 * State scoped function. Override in proper state
	 * Network: Server
	 */
	function ProjectileFire()
	{
		local Proj_Grenade	NadeProj;
		local Vector		SpawnLoc;

		SpawnLoc = GetPhysicalFireStartLoc();
		NadeProj = Proj_Grenade(Spawn(GetProjectileClass(),,,SpawnLoc));

		if ( NadeProj != None )
		{
			NadeProj.Init( Normal(GetGrenadeTossDirection(SpawnLoc)) );
			if ( bCooking )
			{
				NadeProj.SetCookedTime( CookedTime );
			}
		}
	}

	/**
	* Called from PlayerController::UpdateRotation() -> PlayerController::ProcessViewRotation() -> 
	* Pawn::ProcessViewRotation() -> WarInventoryManager::::ProcessViewRotation() to (pre)process player ViewRotation.
	* adds delta rot (player input), applies any limits and post-processing
	* returns the final ViewRotation set on PlayerController
	*
	* extended to reduce player view rotation when simulating grenade trajectory, so the arc remains visible when aiming up or down.
	* essentially, it halves the viewrotation's Pitch, with a pretty smooth interpolation.
	*
	* @param	DeltaTime, time since last frame
	* @param	ViewRotation, actual PlayerController view rotation
	* @input	out_DeltaRot, delta rotation to be applied on ViewRotation. Represents player's input.
	* @return	processed ViewRotation to be set on PlayerController.
	*/
	simulated function ProcessViewRotation( float DeltaTime, out rotator out_ViewRotation, out Rotator out_DeltaRot )
	{
		// limit how high camera can look and we divide player input by half
		out_DeltaRot.Pitch = FNormalizedRotAxis( out_DeltaRot.Pitch ) * 0.5;

		SClampRotAxis( DeltaTime, out_ViewRotation.Pitch, out_DeltaRot.Pitch, 5500, -5500, 10.f );

		super.ProcessViewRotation( DeltaTime, out_ViewRotation, out_DeltaRot );
	}

	/** Don't render crosshair during Ready mode */
	simulated function DrawWeaponCrosshair( Hud H );

	simulated function StopFire( byte FireModeNum )
	{
		ServerStopFire( FireModeNum );
		
		if ( Role < Role_Authority )
		{
			GotoState('ThrowingGrenade');
		}
	}

	/**
	 * release fire to throw grenade
	 */
	simulated function ServerStopFire( byte FireModeNum )
	{
		// Throw grenade
		ProjectileFire();
		GotoState('ThrowingGrenade');
	}

	/**
	 * force reload button to abort ready mode
	 */
	simulated function ForceReload()
	{
		if ( Role < Role_Authority )
		{
			ServerForceReload();
		}
		GotoState('Active');
	}

	/**
	 * force reload button to abort throwing mode
	 */
	function ServerForceReload()
	{
		GotoState('Active');
	}

	/**
	 * SetNextFire mode aborts ready mode
	 */
	simulated function SetNextFireMode()
	{
		if ( Role < Role_Authority )
		{
			ServerForceReload();
		}
		GotoState('Active');
	}

	/**
	 * Force Pawn to walk when in ready position
	 */
	simulated function bool ForceSetWalking( bool bNewIsWalking )
	{
		return true;
	}

	/**
	 * ignore targeting mode, and start grenade cooking
	 */
	simulated function bool OverrideTargetingMode()
	{
		if( !bCooking )
		{
			bCooking = true;
			Owner.PlaySound( CookingSound );
		}
		return true;
	}

	/**
	 * render grenade cooking 
	 */
	simulated function ActiveRenderOverlays( HUD H )
	{
		local float		fCountDown, XL, YL;
		local String	CountText;

		Global.ActiveRenderOverlays( H );
		
		if( bCooking )
		{
			fCountDown	= GetGrenadeClass().default.LifeSpan - CookedTime + 1.f;
			CountText	= "COOKING..." @ int(fCountDown);
			H.Canvas.Font = class'Engine'.Default.SmallFont;
			H.Canvas.StrLen( CountText , XL, YL);
			H.Canvas.SetPos( H.Canvas.SizeX*0.5 - XL*0.5, H.Canvas.SizeY*0.67 );
			XL = FRand() * 255.f;
			H.Canvas.DrawColor = MakeColor( XL, XL, XL, 255 );
			H.Canvas.DrawText( CountText );
		}
	}

	simulated function tick( float deltatime )
	{
		if( bCooking )
		{
			CookedTime += DeltaTime;
			if ( CookedTime >= GetGrenadeClass().default.LifeSpan )
			{	
				EndOfCooking();	
				GotoState('Active');
			}
		}

		if( Instigator.IsLocallyControlled() )
		{
			if( !bFreezeArc )
			{
				StartLoc	= GetPhysicalFireStartLoc();
				AimDir		= GetGrenadeTossDirection( StartLoc );
			}

			RenderThrowingArc( StartLoc, AimDir );
		}
	}

	/** returns focus point to adjust camera */
	simulated function vector GetFocusPoint()
	{
		return FocusPoint;
	}

	simulated function EndState()
	{
		bCooking	= false;
		CookedTime	= 0.f;
	}
}

/** @see AWarWeapon::PlayFireEffects */
simulated function PlayWeaponFireEffects( byte FireModeNum )
{
	local AnimNodeCrossfader	ACrossfader;
	local AnimNodeSequence		ASequence;
	local float					fDuration, fNewRate, fPitch;
	local AnimSequence			AnimSeq;
	local Name					ThrowingAnim;
	local Rotator				AimRot;

	// play throwing animation based on instigator's pitch
	AimRot = Instigator.GetBaseAimRotation();
	fPitch	= FNormalizedRotAxis( AimRot.Pitch );
	if ( fPitch > 4096 )
	{
		ThrowingAnim = 'GRDThLo01'; // lob
	}
	else if ( fPitch < -4096 )
	{
		ThrowingAnim = 'GRDThRo01'; // lob
	}
	else
	{
		ThrowingAnim = 'GRDThPt01'; // normal
	}

	fDuration = 60.f / FireModeInfoArray[FireModeNum].FireRate;

	// update Standing Fire Sequence
	ACrossfader = AnimNodeCrossfader( WarPawn(Instigator).Mesh.Animations.FindAnimNode('anFire') );
	if ( ACrossfader != None )
	{
		// play idle
		ASequence = ACrossFader.GetActiveChild();
		ASequence.SetAnim( PawnIdleAnim );
		ASequence.PlayAnim( true, 1.f, 0.f );
		
		// blend to firing animation
		AnimSeq = Instigator.Mesh.FindAnimSequence( ThrowingAnim );
		fNewRate = AnimSeq.SequenceLength / fDuration;
		ACrossfader.PlayOneShotAnim( ThrowingAnim, 0.15, 0.3, false, fNewRate );
	}

	// update standing idle animation
	// that's a hack to counter upper body per bone blend
	ACrossfader = AnimNodeCrossfader( WarPawn(Instigator).Mesh.Animations.FindAnimNode('anStandIdle') );
	if ( ACrossfader != None && ACrossFader.GetActiveAnimSeq() != ThrowingAnim )
	{
		AnimSeq = Instigator.Mesh.FindAnimSequence( ThrowingAnim );
		fNewRate = AnimSeq.SequenceLength / fDuration;
		ACrossfader.PlayOneShotAnim( ThrowingAnim, 0.15, 0.3, false, fNewRate );
	}
}

/**
 * Sets the timing for the ThrowingGrenade state on server and client.
 * For example plays the throwing animation locally (must call SynchEvent() to properly time firing)
 * Network: LocalPlayer and Server
 */
simulated function TimeThrowGrenade()
{
	local float	fDuration;

	fDuration = 60.f / FireModeInfoArray[CurrentFireMode].FireRate;
	SetTimer(fDuration, false);
}

state ThrowingGrenade
{
	/** @see	Weapon::StartFire */
	simulated function StartFire( byte FireModeNum );

	simulated function BeginState()
	{
		// increase shotcount to play firing anim to other clients
		ShotCount++;

		// play throwing anim locally and on server
		PlayFireEffects( 0 );

		// setup state timing
		TimeThrowGrenade();
		SetWeaponFiringState( true );	// hack - to blend weapon animation while running. fix when designing new player anim tree
	}

	simulated function SynchEvent()
	{
		GotoState('Active');
	}

	simulated function EndState()
	{
		SetWeaponFiringState( false );	// hack - to blend weapon animation while running. fix when designing new player anim tree
	}
}

simulated function DrawWeaponInfo( HUD H )
{
	local WarHUD wfHUD;
	local int idx;
	//@fixme - clean this up post greenlight
	wfHUD = WarHUD(H);
	// draw the elements first
	for (idx = 0; idx < HUDElements.Length; idx++)
	{
		wfHUD.DrawHUDElement(HUDElements[idx]);
	}
}

/*
 GrenadeReadyAnim=GRDId01
 GrenadeThrowLob=GRDThLo01
 GrenadeThrowRoll=GRDThRo01
 */
defaultproperties
{
	CookingSound=SoundCue'COG_Grunt_FragGrenade.FragGrenade_CookingSound'
	PawnFiringAnim=GRDThPt01
	PawnIdleAnim=GRDIdRx01
	PawnMeleeAnim=GRDAtPu01
	FiringStatesArray(0)=Ready
	FireModeInfoArray(0)=(FireRate=90,Name="Throw Grenade")

	HUDElements.Empty
	HUDElements(0)=(bEnabled=True,DrawX=0.040000,DrawY=0.022000,bCenterX=False,bCenterY=False,DrawColor=(B=255,G=255,R=255,A=128),DrawW=256,DrawH=128,DrawIcon=Texture2D'WarfareHudGFX.HUD.HUD_Weapon_T_FragGrenadeIcon',DrawU=0,DrawV=0,DrawUSize=256,DrawVSize=128,DrawScale=0.700000,DrawFont=None,DrawLabel="")
}
