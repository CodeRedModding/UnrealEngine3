/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameExplosionActor extends Actor
	abstract
	config(Weapon)
	native;

/** True if explosion has occurred. */
var protected transient bool bHasExploded;

/** True if this actor can explode more than once and doesn't die after an explosion.
 *	Used by placeable actors whose explosions are triggered via matinee
 */
var() protected bool bExplodeMoreThanOnce;

/** The actual light used for the explosion. */
var protected transient PointLightComponent ExplosionLight;

/** Radial blur for explosion */
var protected transient RadialBlurComponent ExplosionRadialBlur;

/** Temp data for light fading. */
var protected transient float LightFadeTime;
var protected transient float LightFadeTimeRemaining;
var protected transient float LightInitialBrightness;

/** Temp data for radial blur fading. */
var protected transient float RadialBlurFadeTime;
var protected transient float RadialBlurFadeTimeRemaining;
var protected transient float RadialBlurMaxBlurAmount;

/** Temp reference to the explosion template, used for delayed damage */
var GameExplosion ExplosionTemplate;

/**
 *	If TRUE, take the Explosion ParticleSystem lifespan into account when determining
 *	the lifespan of the GameExplosionActor. This is useful in cases where the GEA
 *	needs to do further processing while the particle system is active.
 *	For example, in the case of a smoke grenade, you would want to ensure that the
 *	explosion actor stayed around long enough to properly trigger coughing, etc. when
 *	a pawn enters/exits the smoke area.
 */
var bool bTrackExplosionParticleSystemLifespan;

/** Used to push physics when explosion goes off. */
var protected RB_RadialImpulseComponent	RadialImpulseComponent;

/** player responsible for damage */
var Controller InstigatorController;

/** This the saved off hit actor and location from the GetPhysicalMaterial trace so we can see if it is a FluidSurfaceActor and then apply some forces to it **/
var Actor HitActorFromPhysMaterialTrace;
var vector HitLocationFromPhysMaterialTrace;

/** Are we attached to something?  Used to attach FX for stuff like the smoke grenade. */
var Actor Attachee;
var Controller AttacheeController;

/** Minimum dot product for explosion to be able to affect a point.  Used as an optimization for directional explosions. */
var transient float DirectionalExplosionMinDot;
/** Forward dir for directional explosions. */
var transient vector ExplosionDirection;

/** Toggles debug explosion rendering. */
var bool bDrawDebug;

replication
{
	if (bNetInitial)
		ExplosionDirection;
}

cpptext
{
	virtual void TickSpecial(FLOAT DeltaSeconds);
}

event PreBeginPlay()
{
	Super.PreBeginPlay();

	if (Instigator != None && InstigatorController == None)
	{
		InstigatorController = Instigator.Controller;
	}
}

/**
 * Internal. Tries to find a physical material for the surface the explosion occurred upon.
 * @note: It sucks doing an extra trace here.  We could conceivably pass the physical material info around
 * by changing the lower level physics code (e.g. processHitWall), but that's a big engine-level change.
 */
simulated protected function PhysicalMaterial GetPhysicalMaterial()
{
	local PhysicalMaterial Retval;
	local vector TraceStart, TraceDest, OutHitNorm, ExploNormal;
	local TraceHitInfo OutHitInfo;

	// here we have to do an additional trace shooting straight down to see if we are under water.
	TraceStart = Location + (vect(0,0,1) * 256.f);
	TraceDest = Location - (vect(0,0,1) * 16.f);

	HitActorFromPhysMaterialTrace = Trace(HitLocationFromPhysMaterialTrace, OutHitNorm, TraceDest, TraceStart, TRUE, vect(0,0,0), OutHitInfo, TRACEFLAG_Bullet|TRACEFLAG_PhysicsVolumes);
	//DrawDebugLine( TraceStart, TraceDest, 0, 255, 0, TRUE);
	//`log("EXPLOSION SURFACE:"@HitActorFromPhysMaterialTrace);
	//DrawDebugCoordinateSystem( TraceStart, Rotation, 10.0f, TRUE );

	if( FluidSurfaceActor(HitActorFromPhysMaterialTrace) != None )
	{
		Retval = OutHitInfo.PhysMaterial;
		return Retval;
	}

	ExploNormal = vector(Rotation);
	TraceStart = Location + (ExploNormal * 8.f);
	TraceDest = TraceStart - (ExploNormal * 64.f);

	HitActorFromPhysMaterialTrace = Trace(HitLocationFromPhysMaterialTrace, OutHitNorm, TraceDest, TraceStart, TRUE, vect(0,0,0), OutHitInfo, TRACEFLAG_Bullet);
	//DrawDebugLine( TraceStart, TraceDest, 0, 255, 0, TRUE);
	//DrawDebugCoordinateSystem( TraceStart, Rotation, 10.0f, TRUE );

	if( HitActorFromPhysMaterialTrace != None )
	{
		Retval = OutHitInfo.PhysMaterial;
	}

	return Retval;
}

simulated function bool DoFullDamageToActor(Actor Victim)
{
	return (Victim.bStatic || Victim.IsA('KActor') || Victim.IsA('InterpActor') || Victim.IsA('FracturedStaticMeshPart'));
}

simulated protected function bool IsBehindExplosion(Actor A)
{
	if (ExplosionTemplate.bDirectionalExplosion && !IsZero(ExplosionDirection))
	{
		// @todo, for certain types of actors (e.g. large actors), we may want to test a location other than the
		// actor's location.  Like a cone/bbox check or something.
		// @todo, maybe use Actor's bbox center, like damage does below?
		return (ExplosionDirection dot Normal(A.Location - Location)) < DirectionalExplosionMinDot;
	}

	return FALSE;
}

/**
  * Returns distance from bounding box to point
  */
final static native function float BoxDistanceToPoint(vector Start, Box BBox);

/**
  * Does damage modeling and application for explosions
  * @PARAM bCauseDamage if true cause damage to actors within damage radius
  * @PARAM bCauseEffects if true apply other affects to actors within appropriate radii
  * @RETURN TRUE if at least one Pawn victim got hurt. (This is only valid if bCauseDamage == TRUE)
  */
protected simulated function bool DoExplosionDamage(bool bCauseDamage, bool bCauseEffects)
{
	local Actor		Victim, HitActor;
	local vector	HitL, HitN, Dir, BBoxCenter;//, BBoxExtent;
	local bool		bDamageBlocked, bDoFullDamage, bCauseFractureEffects, bCausePawnEffects, bCauseDamageEffects, bHurtSomeone;
	local float		ColRadius, ColHeight, CheckRadius, VictimDist;
	local array<Actor> VictimsList;
	local Box BBox;
	local Controller ModInstigator;
	local GamePawn VictimPawn;
	local FracturedStaticMeshActor FracActor;
	local byte WantPhysChunksAndParticles;
	local TraceHitInfo HitInfo;
	local KActorFromStatic NewKActor;
	local StaticMeshComponent HitStaticMesh;

	// can pre-calculate this condition now
	bCauseFractureEffects = bCauseEffects && WorldInfo.NetMode != NM_DedicatedServer && ExplosionTemplate.bCausesFracture;
	bCauseEffects = bCauseEffects && WorldInfo.NetMode != NM_Client;
	bHurtSomeone = FALSE;

	// determine radius to check
	CheckRadius = GetEffectCheckRadius(bCauseDamage, bCauseFractureEffects, bCauseEffects);
	if ( CheckRadius > 0.0 )
	{
		foreach CollidingActors(class'Actor', Victim, CheckRadius, Location, ExplosionTemplate.bUseOverlapCheck,,HitInfo )
		{
			// check for static mesh that can become dynamic
			if ( Victim.bWorldGeometry )
			{
				HitStaticMesh = StaticMeshComponent(HitInfo.HitComponent);
				if ( (HitStaticMesh != None) && HitStaticMesh.CanBecomeDynamic() )
				{
					NewKActor = class'KActorFromStatic'.Static.MakeDynamic(HitStaticMesh);
					if ( NewKActor != None )
					{
						Victim = NewKActor;
					}
				}
			}

			// Look for things that are not yourself and not world geom
			if ( Victim != Self
				&& (!Victim.bWorldGeometry || Victim.bCanBeDamaged)
				&& (NavigationPoint(Victim) == None)
				&& Victim != ExplosionTemplate.ActorToIgnoreForDamage
				&& (!ExplosionTemplate.bIgnoreInstigator || Victim != Instigator)
				&& !ClassIsChildOf(Victim.Class, ExplosionTemplate.ActorClassToIgnoreForDamage)
				&& !IsBehindExplosion(Victim) )
			{
				// If attached to a pawn and victim is a pawn on other team
				VictimPawn = GamePawn(Victim);

				// check if visible, unless physics object
				// note: using bbox center instead of location, because that's what visiblecollidingactors does
				Victim.GetComponentsBoundingBox(BBox);

				// adjust distance if using overlap check
				if ( ExplosionTemplate.bUseOverlapCheck )
				{
					VictimDist = BoxDistanceToPoint(Location, BBox);
				}
				else
				{
					VictimDist = VSize(Location - Victim.Location);
				}

				// Do fracturing
				if( bCauseFractureEffects && (VictimPawn == None) )
				{
					FracActor = FracturedStaticMeshActor(Victim);
					if ( (FracActor != None)
						&& (VictimDist < ExplosionTemplate.FractureMeshRadius)
						&& (FracActor.Physics == PHYS_None)
						&& FracActor.IsFracturedByDamageType(ExplosionTemplate.MyDamageType)
						&& FracActor.FractureEffectIsRelevant( false, Instigator, WantPhysChunksAndParticles) )
					{
						// Let kismet know that we were hit by an explosion
						FracActor.NotifyHitByExplosion(InstigatorController, ExplosionTemplate.Damage, ExplosionTemplate.MyDamageType);

						FracActor.BreakOffPartsInRadius(Location, ExplosionTemplate.FractureMeshRadius, ExplosionTemplate.FracturePartVel, WantPhysChunksAndParticles == 1 ? true : false);
					}
				}

				bCausePawnEffects = bCauseEffects && (VictimPawn != None) && !VictimPawn.InGodMode();
				bCauseDamageEffects = bCauseDamage && (VictimDist < ExplosionTemplate.DamageRadius);

				// skip line check for some objects
				if ( DoFullDamageToActor(Victim) )
				{
					bDamageBlocked = FALSE;
					bDoFullDamage = TRUE;			// force full damage for these objects
				}
				else if ( bCausePawnEffects || bCauseDamageEffects )
				{
					BBoxCenter = (BBox.Min + BBox.Max) * 0.5f;

					HitActor = Trace(HitL, HitN, BBoxCenter, Location + vect(0, 0, 20), FALSE,,,TRACEFLAG_Bullet);
					bDamageBlocked = (HitActor != None && HitActor != Victim);
					bDoFullDamage = FALSE;
				}

				if ( !bDamageBlocked )
				{
					if ( bCauseDamageEffects )
					{
						// apply damage
						ModInstigator = InstigatorController;

						// Same team check always returns FALSE if PRI is None
						if (AttacheeController != None && AttacheeController.PlayerReplicationInfo != None && VictimPawn != None && !WorldInfo.GRI.OnSameTeam(AttacheeController, VictimPawn.Controller))
						{
							ModInstigator = AttacheeController;	// Make the instigator the base pawn's controller
						}

						Victim.TakeRadiusDamage(ModInstigator, ExplosionTemplate.Damage, ExplosionTemplate.DamageRadius, ExplosionTemplate.MyDamageType, ExplosionTemplate.MomentumTransferScale, Location, bDoFullDamage, (Owner != None) ? Owner : self, ExplosionTemplate.DamageFalloffExponent);
						VictimsList[VictimsList.Length] = Victim;

						if( Victim.IsA('Pawn') )
						{
							bHurtSomeone = TRUE;
						}
					}

					if ( bCausePawnEffects )
					{
						SpecialPawnEffectsFor(VictimPawn, VictimDist);
					}
					else if (bCauseEffects)
					{
						SpecialCringeEffectsFor(Victim, VictimDist);
					}
				}
			}
		}

		if (ExplosionTemplate.bFullDamageToAttachee && VictimsList.Find(Attachee) == INDEX_NONE)
		{
			Victim = Attachee;

			Victim.GetBoundingCylinder(ColRadius, ColHeight);
			Dir = Normal(Victim.Location - Location);

			Victim.TakeDamage( ExplosionTemplate.Damage, InstigatorController, Victim.Location - 0.5 * (ColHeight + ColRadius) * dir,
						(ExplosionTemplate.MomentumTransferScale * Dir), ExplosionTemplate.MyDamageType,, (Owner != None) ? Owner : self );
		}
	}
	return bHurtSomeone;
}

/**  Return the desired radius to check for actors which get effects from explosion  */
function float GetEffectCheckRadius(bool bCauseDamage, bool bCauseFractureEffects, bool bCauseEffects)
{
	local float CheckRadius;

	if ( bCauseFractureEffects )
	{
		CheckRadius = ExplosionTemplate.FractureMeshRadius;
	}
	if ( bCauseDamage )
	{
		CheckRadius = FMax(CheckRadius, ExplosionTemplate.DamageRadius);
	}
	if ( bCauseEffects )
	{
		CheckRadius = FMax(CheckRadius, ExplosionTemplate.KnockDownRadius);
		CheckRadius = FMax(CheckRadius, ExplosionTemplate.CringeRadius);
	}
	return CheckRadius;
}

/**
  * Handle making pawns cringe or fall down from nearby explosions.  Server only.
  */
protected function SpecialPawnEffectsFor(GamePawn VictimPawn, float VictimDist);

/**
 * Handle applying cringe to non-pawn actors
 * 
 * @param Victim - the actor hit
 * 
 * @param VictimDist - the distance the victim was from the blast
 */
protected function SpecialCringeEffectsFor(Actor Victim, float VictimDist);

/**
 * Internal.  Extract what data we can from the physical material-based effects system
 * and stuff it into the ExplosionTemplate.
 * Data in the physical material will take precedence.
 *
 * We are also going to be checking for relevance here as when any of these params are "none" / invalid we do not
 * play those effects in Explode().  So this way we avoid any work on looking things up in the physmaterial
 *
 */
simulated protected function UpdateExplosionTemplateWithPerMaterialFX(PhysicalMaterial PhysMaterial);

simulated function SpawnExplosionParticleSystem(ParticleSystem Template);

simulated function SpawnExplosionDecal();

simulated function SpawnExplosionFogVolume();

/**
 * @todo break this up into the same methods that <Game>Weapon uses (SpawnImpactEffects, SpawnImpactSounds, SpawnImpactDecal) as they are all
 * orthogonal and so indiv subclasses can choose to have base functionality or override
 *
 * @param Direction     For bDirectionalExplosion=true explosions, this is the forward direction of the blast.
 **/
simulated function Explode(GameExplosion NewExplosionTemplate, optional vector Direction)
{
	local float HowLongToLive;
	local PhysicalMaterial PhysMat;
	local bool bHurtSomeone;

	// copy our significant data
	ExplosionTemplate = NewExplosionTemplate;
	if (ExplosionTemplate.bDirectionalExplosion)
	{
		ExplosionDirection = Normal(Direction);
		DirectionalExplosionMinDot = Cos(ExplosionTemplate.DirectionalExplosionAngleDeg * DegToRad);
	}

	// by default, live just long enough to go boom
	HowLongToLive = LifeSpan + ExplosionTemplate.DamageDelay + 0.01f;

	if (!bHasExploded || bExplodeMoreThanOnce )
	{
		// maybe find the physical material and extract the properties we need
		if (ExplosionTemplate.bAllowPerMaterialFX)
		{
			PhysMat = GetPhysicalMaterial();
			if (PhysMat != None)
			{
				UpdateExplosionTemplateWithPerMaterialFX(PhysMat);
			}
		}

		// spawn explosion effects
		if( WorldInfo.NetMode != NM_DedicatedServer )
		{
			if( ExplosionTemplate.ParticleEmitterTemplate != none )
			{
				SpawnExplosionParticleSystem(ExplosionTemplate.ParticleEmitterTemplate);
				if (bTrackExplosionParticleSystemLifespan == TRUE)
				{
					// Let the particle system contribute to life span determination...
					HowLongToLive = FMax(ExplosionTemplate.ParticleEmitterTemplate.GetMaxLifespan(0.0f) + 0.1f, HowLongToLive);
				}
			}

			// spawn a decal
			SpawnExplosionDecal();

			// turn on the light
			if (ExplosionTemplate.ExploLight != None)
			{
				if( ExplosionLight != None )
				{
					// If there is already an explosion light, detach it
					DetachComponent(ExplosionLight);
				}

				// construct a copy of the PLC, turn it on
				ExplosionLight = new(self) class'PointLightComponent' (ExplosionTemplate.ExploLight);
				if (ExplosionLight != None)
				{
					AttachComponent(ExplosionLight);
					ExplosionLight.SetEnabled(TRUE);
					SetTimer(ExplosionTemplate.ExploLightFadeOutTime);
					LightFadeTime = ExplosionTemplate.ExploLightFadeOutTime;
					LightFadeTimeRemaining = LightFadeTime;
					HowLongToLive = FMax( LightFadeTime + 0.2f, HowLongToLive );
					LightInitialBrightness = ExplosionTemplate.ExploLight.Brightness;
				}
			}

			// radial blur
			if (ExplosionTemplate.ExploRadialBlur != None)
			{
				if ((ExplosionTemplate.bPerformRadialBlurRelevanceCheck == false) || ImpactEffectIsRelevant(Instigator, Location+vect(0,0,1), false, 4000.0f, 350.0f, true))
				{
					if( ExplosionRadialBlur != None )
					{
						// If there is already a radial blur, detach it
						DetachComponent(ExplosionRadialBlur);
					}

					ExplosionRadialBlur = new(self) class'RadialBlurComponent' (ExplosionTemplate.ExploRadialBlur);
					if (ExplosionRadialBlur != None)
					{
						AttachComponent(ExplosionRadialBlur);
						RadialBlurFadeTime = ExplosionTemplate.ExploRadialBlurFadeOutTime;
						RadialBlurFadeTimeRemaining = RadialBlurFadeTime;
						RadialBlurMaxBlurAmount = ExplosionTemplate.ExploRadialBlurMaxBlur;
						SetTimer(FMax(RadialBlurFadeTime,LightFadeTime));
						HowLongToLive = FMax( RadialBlurFadeTime + 0.2f, HowLongToLive );
					}
				}
			}

			// cam shakes
			DoExplosionCameraEffects();

			// Apply impulse to physics stuff (before we do fracture)
			if (ExplosionTemplate.MyDamageType != None && ExplosionTemplate.MyDamageType.default.RadialDamageImpulse > 0.0)
			{
				RadialImpulseComponent.ImpulseRadius = FMax(ExplosionTemplate.DamageRadius, ExplosionTemplate.KnockDownRadius);
				RadialImpulseComponent.ImpulseStrength = ExplosionTemplate.MyDamageType.default.RadialDamageImpulse;
				RadialImpulseComponent.bVelChange = ExplosionTemplate.MyDamageType.default.bRadialDamageVelChange;
				RadialImpulseComponent.ImpulseFalloff = RIF_Constant;
				//`log("AA"@ExplosionTemplate.MyDamageType@RadialImpulseComponent.ImpulseStrength@RadialImpulseComponent.ImpulseRadius);
				RadialImpulseComponent.FireImpulse(Location);
			}

			SpawnExplosionFogVolume();

			if( FluidSurfaceActor(HitActorFromPhysMaterialTrace) != none )
			{
				FluidSurfaceActor(HitActorFromPhysMaterialTrace).FluidComponent.ApplyForce( HitLocationFromPhysMaterialTrace, 1024.0f, 20.0f, FALSE );
			}
		}

		// do damage
		// delay the damage if necessary,
		bHurtSomeone = FALSE;
		if ( ExplosionTemplate.Damage > 0.0 )
		{
			if (ExplosionTemplate.DamageDelay > 0.0)
			{
				// cause effects now, damage later
				DoExplosionDamage(false, true);
				SetTimer( ExplosionTemplate.DamageDelay, FALSE, nameof(DelayedExplosionDamage) );
			}
			else
			{
				// otherwise apply immediately
				bHurtSomeone = DoExplosionDamage(true, true);
			}
		}
		else
		{
			DoExplosionDamage(false, true);
		}

	// play the sound
		if( WorldInfo.NetMode != NM_DedicatedServer )
		{
			if( bHurtSomeone && ExplosionTemplate.ExplosionSoundHurtSomeone != None)
			{
				//`log( "Playing Explosion Sound (debug left in to test distance)" @ ExplosionTemplate.ExplosionSound );
				PlaySound( ExplosionTemplate.ExplosionSoundHurtSomeone, TRUE, TRUE, FALSE, Location, TRUE );
			}
			else if( ExplosionTemplate.ExplosionSound != None )
			{
				//`log( "Playing Explosion Sound (debug left in to test distance)" @ ExplosionTemplate.ExplosionSound );
				PlaySound( ExplosionTemplate.ExplosionSound, TRUE, TRUE, FALSE, Location, TRUE );
			}
		}

		if( Role == Role_Authority )
		{
			MakeNoise(1.0);
		}

`if(`notdefined(FINAL_RELEASE))
		if (bDrawDebug)
		{
			DrawDebug();
		}
`endif

		bHasExploded = TRUE;

		// done with it
		if (!bPendingDelete && !bDeleteMe)
		{
			// Live forever if this actor can explode more than once
			LifeSpan = bExplodeMoreThanOnce ? 0.0 : HowLongToLive;
		}
	}
}

simulated function DelayedExplosionDamage()
{
	DoExplosionDamage(true, false);
}

simulated function DrawDebug()
{
	local Color C;
	local float Angle;

	// debug spheres
	if (ExplosionTemplate.bDirectionalExplosion)
	{
		C.R = 255;
		C.G = 128;
		C.B = 16;
		C.A = 255;
		Angle = ExplosionTemplate.DirectionalExplosionAngleDeg * DegToRad;

		DrawDebugCone(Location, ExplosionDirection, ExplosionTemplate.DamageRadius, Angle, Angle, 8, C, TRUE);
	}
	else
	{
		DrawDebugSphere(Location, ExplosionTemplate.DamageRadius, 10, 255, 128, 16, TRUE);
		//DrawDebugLine(Location, Location + HitNormal*16, 255, 255, 255, TRUE);
	}
}

simulated function DoExplosionCameraEffects()
{
	local CameraShake Shake;
	local float ShakeScale;
	local PlayerController PC;

	// do camera shake(s)
	// note: intentionally letting directional explosions still shake everything
	foreach WorldInfo.LocalPlayerControllers(class'PlayerController', PC)
	{
		if (PC.PlayerCamera != None)
		{
			Shake = ChooseCameraShake(Location, PC);
			if (Shake != None)
			{
				ShakeScale = PC.PlayerCamera.CalcRadialShakeScale(PC.PlayerCamera, Location, ExplosionTemplate.CamShakeInnerRadius, ExplosionTemplate.CamShakeOuterRadius, ExplosionTemplate.CamShakeFalloff);

				if (ExplosionTemplate.bOrientCameraShakeTowardsEpicenter)
				{
					PC.ClientPlayCameraShake(Shake, ShakeScale, ExplosionTemplate.bAutoControllerVibration, CAPS_UserDefined, rotator(Location - PC.ViewTarget.Location));
				}
				else
				{
					PC.ClientPlayCameraShake(Shake, ShakeScale, ExplosionTemplate.bAutoControllerVibration);
				}
			}
		}
	}

	// do lens effects
	SpawnCameraLensEffects();
}

/**
 * Spawns the camera lens effect(s) if needed by this explosion
 */
simulated function SpawnCameraLensEffects()
{
	local PlayerController PC;

	if (ExplosionTemplate.CameraLensEffect != None)
	{
		foreach WorldInfo.LocalPlayerControllers(class'PlayerController', PC)
		{
			// splatter some blood on their camera if they are a human and decently close
			if ( PC.Pawn != None &&
				VSize(PC.Pawn.Location - Location) < ExplosionTemplate.CameraLensEffectRadius &&
				PC.IsAimingAt(self, 0.1) &&    // if we are semi looking in the direction of the explosion
				!IsBehindExplosion(PC.Pawn) )
			{
				PC.ClientSpawnCameraLensEffect(ExplosionTemplate.CameraLensEffect);
			}
		}
	}
}

 /**
  * Internal.  When using directional camera shakes, used to determine which anim to use.
  * @todo: nativise for speed?
  */
protected simulated function CameraShake ChooseCameraShake(vector Epicenter, PlayerController PC)
{
	local vector CamX, CamY, CamZ, ToEpicenter;
	local float FwdDot, RtDot;
	local CameraShake ChosenShake;
	local Rotator NoPitchRot;

	if (ExplosionTemplate.bOrientCameraShakeTowardsEpicenter)
	{
		return ExplosionTemplate.CamShake;
	}
	// expected to be false much of the time, so maybe bypass the math
	else if ( (ExplosionTemplate.CamShake_Left != None) || (ExplosionTemplate.CamShake_Right != None) || (ExplosionTemplate.CamShake_Rear != None) )
	{
 		ToEpicenter = Epicenter - PC.PlayerCamera.Location;
 		ToEpicenter.Z = 0.f;
 		ToEpicenter = Normal(ToEpicenter);
 		NoPitchRot = PC.PlayerCamera.Rotation;
 		NoPitchRot.Pitch = 0.f;
 		GetAxes(NoPitchRot, CamX, CamY, CamZ);

 		FwdDot = CamX dot ToEpicenter;
 		if (FwdDot > 0.707f)
 		{
 			// use forward
 			ChosenShake = ExplosionTemplate.CamShake;
 		}
 		else if (FwdDot > -0.707f)
 		{
 			// need to determine r or l
 			RtDot = CamY dot ToEpicenter;
 			ChosenShake = (RtDot > 0.f) ? ExplosionTemplate.CamShake_Right : ExplosionTemplate.CamShake_Left;
 		}
 		else
 		{
 			// use back
 			ChosenShake = ExplosionTemplate.CamShake_Rear;
 		}
	}

 	if (ChosenShake == None)
 	{
 		// fall back to forward
 		ChosenShake = ExplosionTemplate.CamShake;
	}

	return ChosenShake;
}

defaultproperties
{
	RemoteRole=ROLE_None

	bExplodeMoreThanOnce = False;

	Begin Object Class=RB_RadialImpulseComponent Name=ImpulseComponent0
	End Object
	RadialImpulseComponent=ImpulseComponent0
	Components.Add(ImpulseComponent0)

	//bDebug=TRUE
}
