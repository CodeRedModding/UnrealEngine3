/**
 * KActor used for spawning "effect" type of physics objects.  (e.g. gun magazines)
 * This will nice "fade to nothing when it is dying.
 *
 *
 * So in PhysX2 when we use smallish collision on objects we will get tunneling and that will cause the
 * object to either fall directly through the ground or bounce a little bit and then fall through the ground.
 *
 * CCD currently is slow, has some bugs, and is a global setting (as opposed to compartment)
 *
 * So for physx2 you need to make a larger than correct box/shape and that should stop these RigidBodyies from falling through
 * the world.
 *
 * One way to do that is with the "Set Collision from Builder Brush" functionality.  Make a builder brush around the object and use that for
 * the collision!
 *
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class GameKActorSpawnableEffect extends KActor
	notplaceable;

// don't do normal KActor init
simulated event PostBeginPlay()
{
	SetTimer(FMax(0.01,LifeSpan-1.1), false, 'StartScalingDown');
}

simulated event FellOutOfWorld( class<DamageType> dmgType )
{
	Destroy();
}

simulated function StartScalingDown()
{
 	GotoState('ScalingDown');
}

simulated state ScalingDown
{
	simulated event Tick( float DeltaTime )
	{
		Super.Tick( DeltaTime );

		// if we are close to the end of our life start scaling to zero
		if( LifeSpan < 1.0f )
		{
			SetDrawScale( LifeSpan );
		}
	}
}

defaultproperties
{
	Begin Object Name=MyLightEnvironment
		bCastShadows=FALSE
		bEnabled=TRUE
		bDynamic=TRUE
	End Object

	Begin Object Name=StaticMeshComponent0
		CastShadow=FALSE
		BlockActors=FALSE
		bAcceptsStaticDecals=FALSE
		bAcceptsDynamicDecals=FALSE
	End Object

	bNoDelete=FALSE
	RemoteRole=ROLE_None
	bBlocksNavigation=FALSE
	bCollideWorld=FALSE
	bCollideActors=FALSE
	bBlockActors=FALSE
	bNoEncroachCheck=TRUE
	LifeSpan=30.0f
}
