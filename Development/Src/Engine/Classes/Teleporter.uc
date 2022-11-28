///=============================================================================
// Teleports actors either between different teleporters within a level
// or to matching teleporters on other levels, or to general Internet URLs.
//=============================================================================
class Teleporter extends NavigationPoint
	placeable
	native;

cpptext
{
	void addReachSpecs(AScout *Scout, UBOOL bOnlyChanged=0);
}

//-----------------------------------------------------------------------------
// Teleporter URL can be one of the following forms:
//
// TeleporterName
//		Teleports to a named teleporter in this level.
//		if none, acts only as a teleporter destination
//
// LevelName/TeleporterName
//     Teleports to a different level on this server.
//
// Unreal://Server.domain.com/LevelName/TeleporterName
//     Teleports to a different server on the net.
//
var() string URL;

//-----------------------------------------------------------------------------
// Product the user must have installed in order to enter the teleporter.
var() name ProductRequired;

//-----------------------------------------------------------------------------
// Teleporter destination flags.
var() bool    bChangesVelocity; // Set velocity to TargetVelocity.
var() bool    bChangesYaw;      // Sets yaw to teleporter's Rotation.Yaw
var() bool    bReversesX;       // Reverses X-component of velocity.
var() bool    bReversesY;       // Reverses Y-component of velocity.
var() bool    bReversesZ;       // Reverses Z-component of velocity.

// Teleporter flags
var() bool	  bEnabled;			// Teleporter is turned on;

//-----------------------------------------------------------------------------
// Teleporter destination directions.
var() vector  TargetVelocity;   // If bChangesVelocity, set target's velocity to this.

var float LastFired;

//-----------------------------------------------------------------------------
// Teleporter destination functions.

replication
{
	reliable if( Role==ROLE_Authority )
		bEnabled, URL;
	reliable if ( bNetInitial && (Role == ROLE_Authority) )
		bChangesVelocity, bChangesYaw, bReversesX, bReversesY, bReversesZ, TargetVelocity;
}

function PostBeginPlay()
{
	if (URL ~= "")
		SetCollision(false, false); //destination only
		
	Super.PostBeginPlay();
}

// Accept an actor that has teleported in.
simulated function bool Accept( actor Incoming, Actor Source )
{
	local rotator newRot, oldRot;
	local float mag;
	local vector oldDir;
	local Controller P;

	if ( Incoming == None )
		return false;
		
	// Move the actor here.
	Disable('Touch');
	newRot = Incoming.Rotation;
	if (bChangesYaw)
	{
		oldRot = Incoming.Rotation;
		newRot.Yaw = Rotation.Yaw;
		if ( Source != None )
			newRot.Yaw += (32768 + Incoming.Rotation.Yaw - Source.Rotation.Yaw);
	}

	if ( Pawn(Incoming) != None )
	{
		//tell enemies about teleport
		if ( Role == ROLE_Authority )
			For ( P=Level.ControllerList; P!=None; P=P.NextController )
				if ( P.Enemy == Incoming )
					P.LineOfSightTo(Incoming);

		if ( !Pawn(Incoming).SetLocation(Location) )
		{
			log(self$" Teleport failed for "$Incoming);
			return false;
		}
		if ( (Role == ROLE_Authority)
			|| (Level.TimeSeconds - LastFired > 0.5) )
		{
			newRot.Roll = 0;
			Pawn(Incoming).SetRotation(newRot);
			Pawn(Incoming).SetViewRotation(newRot);
			Pawn(Incoming).ClientSetRotation(newRot);
			LastFired = Level.TimeSeconds;
		}
		if ( Pawn(Incoming).Controller != None )
		{
			Pawn(Incoming).Controller.MoveTimer = -1.0;
			Pawn(Incoming).Anchor = self;
			Pawn(Incoming).SetMoveTarget(self);
		}
		Incoming.PlayTeleportEffect(false, true);
	}
	else
	{
		if ( !Incoming.SetLocation(Location) )
		{
			Enable('Touch');
			return false;
		}
		if ( bChangesYaw )
			Incoming.SetRotation(newRot);
	}
	Enable('Touch');

	if (bChangesVelocity)
		Incoming.Velocity = TargetVelocity;
	else
	{
		if ( bChangesYaw )
		{
			if ( Incoming.Physics == PHYS_Walking )
				OldRot.Pitch = 0;
			oldDir = vector(OldRot);
			mag = Incoming.Velocity Dot oldDir;		
			Incoming.Velocity = Incoming.Velocity - mag * oldDir + mag * vector(Incoming.Rotation);
		}
		if ( bReversesX )
			Incoming.Velocity.X *= -1.0;
		if ( bReversesY )
			Incoming.Velocity.Y *= -1.0;
		if ( bReversesZ )
			Incoming.Velocity.Z *= -1.0;
	}	
	return true;
}

//-----------------------------------------------------------------------------
// Teleporter functions.

event Touch( Actor Other, vector HitLocation, vector HitNormal )
{
	if ( !bEnabled || (Other == None) )
		return;

	if( Other.bCanTeleport && Other.PreTeleport(Self)==false )
	{
		PendingTouch = Other.PendingTouch;
		Other.PendingTouch = self;
	}
}

// Teleporter was touched by an actor.
simulated function PostTouch( actor Other )
{
	local Teleporter D,Dest[16];
	local int i;

		if( (InStr( URL, "/" ) >= 0) || (InStr( URL, "#" ) >= 0) )
		{
			// Teleport to a level on the net.
			if( (Role == ROLE_Authority) && (Pawn(Other) != None)
				&& Pawn(Other).IsHumanControlled() )
				Level.Game.SendPlayer(PlayerController(Pawn(Other).Controller), URL);
		}
		else
		{
			// Teleport to a random teleporter in this local level, if more than one pick random.

			foreach AllActors( class 'Teleporter', D )
				if( string(D.tag)~=URL && D!=Self )
				{
					Dest[i] = D;
					i++;
					if ( i > arraycount(Dest) )
						break;
				}

			i = rand(i);
			if( Dest[i] != None )
			{
				// Teleport the actor into the other teleporter.
				if ( Other.IsA('Pawn') )
					Other.PlayTeleportEffect(false, true);
				Dest[i].Accept( Other, self );
			}
		}
	}

/* SpecialHandling is called by the navigation code when the next path has been found.
It gives that path an opportunity to modify the result based on any special considerations
*/

function Actor SpecialHandling(Pawn Other)
{
	if ( bEnabled && (Teleporter(Other.Controller.RouteCache[1]) != None)
		&& (string(Other.Controller.RouteCache[1].tag)~=URL) )
	{
		if( TouchingActor(Other) ) 
			PostTouch(Other);

		return self;
	}
	// FIXMESTEVE - need to tell bot how to enable me			
	return None;			
}	
	

defaultproperties
{
	Begin Object NAME=CollisionCylinder
		CollisionRadius=+00040.000000
		CollisionHeight=+00080.000000
	End Object

	Begin Object NAME=Sprite
		Sprite=Texture2D'EngineResources.S_Teleport'
	End Object

	Begin Object Class=ArrowComponent Name=Arrow
		ArrowColor=(R=150,G=200,B=255)
	End Object
	Components.Add(Arrow)

	RemoteRole=ROLE_SimulatedProxy
	bChangesYaw=true
	bEnabled=true
	bCollideActors=true
}
