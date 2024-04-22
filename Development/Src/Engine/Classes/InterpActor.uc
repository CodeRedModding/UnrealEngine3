/** 
 *  Dynamic static mesh actor intended to be used with Matinee replaces movers
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpActor extends DynamicSMActor
	native
	ClassGroup(Common)
	placeable;

cpptext
{
	UBOOL ShouldTrace(UPrimitiveComponent* Primitive, AActor *SourceActor, DWORD TraceFlags);
	virtual void TickSpecial(FLOAT DeltaSeconds);
	virtual FLOAT GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, APlayerController* Viewer, UActorChannel* InChannel, FLOAT Time, UBOOL bLowBandwidth);

	/**
	 * Function that gets called from within Map_Check to allow this actor to check itself
	 * for any potential errors and register them with map check dialog.
	 */
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif
}

/** Data relevant to checkpoint save/load, see CreateCheckpointRecord/ApplyCheckpointRecord below */
struct CheckpointRecord
{
    var vector Location;
    var rotator Rotation;
    var ECollisionType CollisionType;
    var bool bHidden;
    var bool bIsShutdown;
    var bool bNeedsPositionReplication;
};
/** whether this should be saved in checkpoints */
var bool bShouldSaveForCheckpoint;

/** NavigationPoint associated with this actor for sending AI related notifications (could be a LiftCenter or DoorMarker) */
var NavigationPoint MyMarker;
/** true when AI is waiting for us to finish moving */
var bool bMonitorMover;
/** if true, call MoverFinished() event on all Controllers with us as their PendingMover when we reach peak Z velocity */
var bool bMonitorZVelocity;
/** set while monitoring lift movement */
var float MaxZVelocity;
/** delay after mover finishes interpolating before it notifies any mover events */
var float StayOpenTime;
/** sound played when the mover is interpolated forward */
var() SoundCue OpenSound;
/** looping sound while opening */
var() SoundCue OpeningAmbientSound;
/** sound played when mover finished moving forward */
var() SoundCue OpenedSound;
/** sound played when the mover is interpolated in reverse */
var() SoundCue CloseSound;
/** looping sound while closing */
var() SoundCue ClosingAmbientSound;
/** sound played when mover finished moving backward */
var() SoundCue ClosedSound;
/** component for looping sounds */
var AudioComponent AmbientSoundComponent;

/** if set this mover blows up projectiles when it encroaches them */
var() bool bDestroyProjectilesOnEncroach;
/** if set, this mover keeps going if it encroaches an Actor in PHYS_RigidBody.  */
var() bool bContinueOnEncroachPhysicsObject;
/** true by default, prevents mover from completing the movement that would leave it encroaching another actor */
var() bool bStopOnEncroach;

/**
 * This is used for having the Actor ShadowParent all of the components that are "SetBased" onto it.  This allows LDs to
 * take InterpActors in the level and then SetBase a ton of other meshes to them and not incur multiple shadow casters.
 **/
var() bool bShouldShadowParentAllAttachedActors;

/** If true, have a liftcenter associated with this interpactor, so it is being used as a lift */
var bool bIsLift;

simulated event PostBeginPlay()
{
	Super.PostBeginPlay();

	if (bShouldShadowParentAllAttachedActors)
	{
		SetShadowParentOnAllAttachedComponents(StaticMeshComponent, LightEnvironment);
	}

	// create ambient sound component if needed
	if (OpeningAmbientSound != None || ClosingAmbientSound != None)
	{
		AmbientSoundComponent = new(self) class'AudioComponent';
		AttachComponent(AmbientSoundComponent);
	}

	// by default don't save InterpActors that are based on a skeletal mesh bone
	if (Base != None && (bHardAttach || (BaseSkelComponent != None && BaseBoneName != 'None')))
	{
		bShouldSaveForCheckpoint = false;
	}
}


event bool EncroachingOn(Actor Other)
{
	local int i;
	local SeqEvent_Mover MoverEvent;
	local Pawn P;
	local vector Height, HitLocation, HitNormal;
	local bool bLandingPawn;

	// Allow move into rigid bodies - should just push them out of the way.
	if(bContinueOnEncroachPhysicsObject && (Other.Physics == PHYS_RigidBody))
	{
		return FALSE;
	}

	// Check if this is something that should be destroyed when mover runs into it
	if(Other.bDestroyedByInterpActor)
	{
		Other.Destroy();
		return FALSE;
	}

	// if we're moving towards the actor
	if ( (Other.Base == self) || (Normal(Velocity) Dot Normal(Other.Location - Location) >= 0.f) )
	{
		// if we're moving up into a pawn, ignore it so it can land on us instead
		P = Pawn(Other);
		if (P != None)
		{
			if (P.Physics == PHYS_Falling && Velocity.Z > 0.f)
			{
				Height = P.GetCollisionHeight() * vect(0,0,1);
				// @note: only checking against our StaticMeshComponent, assumes we have no other colliding components
				if (TraceComponent(HitLocation, HitNormal, StaticMeshComponent, P.Location - Height, P.Location + Height, P.GetCollisionExtent()))
				{
					// make sure the pawn doesn't fall through us
					if (P.Location.Z < Location.Z)
					{
						P.SetLocation(HitLocation + Height);
					}
					bLandingPawn = true;
				}
			}
			else if (P.Base != self && P.Controller != None && P.Controller.PendingMover != None && P.Controller.PendingMover == self)
			{
				P.Controller.UnderLift(LiftCenter(MyMarker));
			}
		}
		else if (bDestroyProjectilesOnEncroach && Other.IsA('Projectile'))
		{
			Projectile(Other).Explode(Other.Location, -Normal(Velocity));
			return false;
		}

		if ( !bLandingPawn )
		{
			// search for any mover events
			for (i = 0; i < GeneratedEvents.Length; i++)
			{
				MoverEvent = SeqEvent_Mover(GeneratedEvents[i]);
				if (MoverEvent != None)
				{
					// notify the event that we encroached something
					MoverEvent.NotifyEncroachingOn(Other);
				}
			}
			return bStopOnEncroach;
		}
	}

	return false;
}

/*
 * called for encroaching actors which successfully moved the other actor out of the way
 */
event RanInto( Actor Other )
{
	local int i;
	local SeqEvent_Mover MoverEvent;

	if (bDestroyProjectilesOnEncroach && Other.IsA('Projectile'))
	{
		Projectile(Other).Explode(Other.Location, -Normal(Velocity));
	}
	// Check if this is something that should be destroyed when mover runs into it
	else if(Other.bDestroyedByInterpActor)
	{
		Other.Destroy();
	}
	else if ( bIsLift )
	{
		// no encroach event if have liftcenter based on me
		// keeps lifts from returning when object/player based on them bounces/jumps and then runs into lift
		return;
	}
	else
	{
		// search for any mover events
		for (i = 0; i < GeneratedEvents.Length; i++)
		{
			MoverEvent = SeqEvent_Mover(GeneratedEvents[i]);
			if (MoverEvent != None)
			{
				// notify the event that we encroached something
				MoverEvent.NotifyEncroachingOn(Other);
			}
		}
	}
}


event Attach(Actor Other)
{
	local int i;
	local SeqEvent_Mover MoverEvent;

	if (!IsTimerActive('FinishedOpen'))
	{
		// search for any mover events
		for (i = 0; i < GeneratedEvents.Length; i++)
		{
			MoverEvent = SeqEvent_Mover(GeneratedEvents[i]);
			if (MoverEvent != None)
			{
				// notify the event that an Actor has been attached
				MoverEvent.NotifyAttached(Other);
			}
		}
	}
}

event Detach(Actor Other)
{
	local int i;
	local SeqEvent_Mover MoverEvent;

	// search for any mover events
	for (i = 0; i < GeneratedEvents.Length; i++)
	{
		MoverEvent = SeqEvent_Mover(GeneratedEvents[i]);
		if (MoverEvent != None)
		{
			// notify the event that an Actor has been detached
			MoverEvent.NotifyDetached(Other);
		}
	}
}

/** checks if anything is still attached to the mover, and if so notifies Kismet so that it may restart it if desired */
function Restart()
{
	local Actor A;

	foreach BasedActors(class'Actor', A)
	{
		Attach(A);
	}
}

/** called on a timer StayOpenTime seconds after the mover has finished opening (forward matinee playback) */
function FinishedOpen()
{
	local int i;
	local SeqEvent_Mover MoverEvent;

	// search for any mover events
	for (i = 0; i < GeneratedEvents.Length; i++)
	{
		MoverEvent = SeqEvent_Mover(GeneratedEvents[i]);
		if (MoverEvent != None)
		{
			// notify the event that all opening and associated delays are finished and it may now reverse our direction
			// (or do any other actions as set up in Kismet)
			MoverEvent.NotifyFinishedOpen();
		}
	}
}

simulated function PlayMovingSound(bool bClosing)
{
	local SoundCue SoundToPlay;
	local SoundCue AmbientToPlay;

	if (bClosing)
	{
		SoundToPlay = CloseSound;
		AmbientToPlay = OpeningAmbientSound;
	}
	else
	{
		SoundToPlay = OpenSound;
		AmbientToPlay = ClosingAmbientSound;
	}
	if (SoundToPlay != None)
	{
		PlaySound(SoundToPlay, true);
	}
	if (AmbientToPlay != None)
	{
		AmbientSoundComponent.Stop();
		AmbientSoundComponent.SoundCue = AmbientToPlay;
		AmbientSoundComponent.Play();
	}
}

simulated event InterpolationStarted(SeqAct_Interp InterpAction, InterpGroupInst GroupInst)
{
	ClearTimer('Restart');
	ClearTimer('FinishedOpen');

	PlayMovingSound(InterpAction.bReversePlayback);

	// we need to save it if it's affected by a matinee
	bShouldSaveForCheckpoint = true;
}

simulated event InterpolationFinished(SeqAct_Interp InterpAction)
{
	local DoorMarker DoorNav;
	local Controller C;
	local SoundCue StoppedSound;

	if (AmbientSoundComponent != None)
	{
		AmbientSoundComponent.Stop();
	}

	StoppedSound = InterpAction.bReversePlayback ? ClosedSound : OpenedSound;
	if (StoppedSound != None)
	{
		PlaySound(StoppedSound, true);
	}

	DoorNav = DoorMarker(MyMarker);
	if (InterpAction.bReversePlayback)
	{
		// we are done; if something is still attached, set timer to try restart
		if (Attached.length > 0)
		{
			SetTimer( StayOpenTime, false, nameof(Restart) );
		}
		if (DoorNav != None)
		{
			DoorNav.MoverClosed();
		}
	}
	else
	{
		// set timer to notify any mover events
		SetTimer( StayOpenTime, false, nameof(FinishedOpen) );

		if (DoorNav != None)
		{
			DoorNav.MoverOpened();
		}
	}

	if (bMonitorMover)
	{
		// notify any Controllers with us as PendingMover that we have finished moving
		foreach WorldInfo.AllControllers(class'Controller', C)
		{
			if (C.PendingMover == self)
			{
				C.MoverFinished();
			}
		}
	}

	//@hack: force location update on clients if future matinee actions rely on it
	if (InterpAction.bNoResetOnRewind && InterpAction.bRewindOnPlay)
	{
		ForceNetRelevant();
		bUpdateSimulatedPosition = true;
		bReplicateMovement = true;
	}
}

simulated event InterpolationChanged(SeqAct_Interp InterpAction)
{
	PlayMovingSound(InterpAction.bReversePlayback);
}

simulated function ShutDown()
{
	Super.ShutDown();

	// safe to save regardless of other factors because it's going to be invisible/uncollidable on load
	bShouldSaveForCheckpoint = true;
}

function bool ShouldSaveForCheckpoint()
{
	return bShouldSaveForCheckpoint || RemoteRole == ROLE_SimulatedProxy;
}

/** Called when this actor is being saved in a checkpoint, records pertinent information for restoration via ApplyCheckpointRecord. */
function CreateCheckpointRecord(out CheckpointRecord Record)
{
	Record.Location = Location;
	Record.Rotation = Rotation;
	Record.bHidden = bHidden;
	Record.CollisionType = ReplicatedCollisionType;
	Record.bNeedsPositionReplication = (RemoteRole == ROLE_SimulatedProxy && bUpdateSimulatedPosition);
	//@fixme - is there a more reliable way to detect this?  maybe add a bIsShutDown flag to actor?
	Record.bIsShutdown = (Physics == PHYS_None && bHidden);
}

function ApplyCheckpointRecord(const out CheckpointRecord Record)
{
	local Actor OldBase;
	local SkeletalMeshComponent OldBaseComp;
	local name OldBaseBoneName;
	local array<Actor> OldAttached;
	local array<vector> OldLocations;
	local int i;

	if (Record.bIsShutdown)
	{
		ShutDown();
	}
	else
	{
		// store and recover the location of other checkpoint saved actors
		// as they may have already been processed
		// otherwise their post-checkpoint location will be based on our pre-checkpoint location
		// which will put them out of position
		OldAttached = Attached;
		while (i < OldAttached.length)
		{
			// checkpoint code clears bJustTeleported, so checking it only gets actors teleported by checkpoint loading
			if (OldAttached[i] != None && OldAttached[i].bJustTeleported)
			{
				OldLocations[i] = OldAttached[i].Location;
				i++;
			}
			else
			{
				OldAttached.Remove(i, 1);
			}
		}
		// SetLocation() will clear our base, so we need to restore it
		OldBase = Base;
		OldBaseComp = BaseSkelComponent;
		OldBaseBoneName = BaseBoneName;
		SetLocation(Record.Location);
		SetRotation(Record.Rotation);
		SetBase(OldBase,, OldBaseComp, OldBaseBoneName);
		// restore attached actors
		for (i = 0; i < OldAttached.length; i++)
		{
			if (OldAttached[i] != None)
			{
				OldAttached[i].SetLocation(OldLocations[i]);
				OldAttached[i].SetBase(self);
			}
		}

		if (Record.CollisionType != ReplicatedCollisionType)
		{
			SetCollisionType(Record.CollisionType);
			ForceNetRelevant();
		}
		if (Record.bHidden != bHidden)
		{
			SetHidden(Record.bHidden);
			SetForcedInitialReplicatedProperty(Property'Engine.Actor.bHidden', (bHidden == default.bHidden));
			ForceNetRelevant();
		}
		if (Record.bNeedsPositionReplication)
		{
			bUpdateSimulatedPosition = true;
			bReplicateMovement = true;
			ForceNetRelevant();
		}
	}

	bShouldSaveForCheckpoint = true;
}

defaultproperties
{
	bShouldShadowParentAllAttachedActors=TRUE

	Begin Object Name=StaticMeshComponent0
		WireframeColor=(R=255,G=0,B=255,A=255)
		AlwaysLoadOnClient=true
		AlwaysLoadOnServer=true
		RBCollideWithChannels=(Default=TRUE,BlockingVolume=TRUE)
	End Object

	bStatic=false
	bWorldGeometry=false
	Physics=PHYS_Interpolating

	bNoDelete=true
	bAlwaysRelevant=true
	bSkipActorPropertyReplication=false
	bUpdateSimulatedPosition=false
	bOnlyDirtyReplication=true
	RemoteRole=ROLE_None
	NetPriority=2.7
	NetUpdateFrequency=1.0
	bDestroyProjectilesOnEncroach=true
	bStopOnEncroach=true
	bContinueOnEncroachPhysicsObject=TRUE
	bCollideWhenPlacing=FALSE
	bBlocksTeleport=true
	bShouldSaveForCheckpoint=true

	SupportedEvents.Add(class'SeqEvent_Mover')
}

