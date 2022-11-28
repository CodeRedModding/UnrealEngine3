//=============================================================================
// Volume:  a bounding volume
// touch() and untouch() notifications to the volume as actors enter or leave it
// enteredvolume() and leftvolume() notifications when center of actor enters the volume
// pawns with bIsPlayer==true  cause playerenteredvolume notifications instead of actorenteredvolume()
// This is a built-in Unreal class and it shouldn't be modified.
//=============================================================================
class Volume extends Brush
	native;

var Actor AssociatedActor;			// this actor gets touch() and untouch notifications as the volume is entered or left
var() name AssociatedActorTag;		// Used by L.D. to specify tag of associated actor
var() int LocationPriority;
var() localized string LocationName;

cpptext
{
	INT Encompasses(FVector point);
	void SetVolumes();
	virtual void SetVolumes(const TArray<class AVolume*>& Volumes);
	UBOOL ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags);
	virtual UBOOL IsAVolume() {return true;}
}

native function bool Encompasses(Actor Other); // returns true if center of actor is within volume

function PostBeginPlay()
{
	Super.PostBeginPlay();

	if ( AssociatedActorTag != '' )
		ForEach AllActors(class'Actor',AssociatedActor, AssociatedActorTag)
			break;
	if ( AssociatedActor != None )
	{
		GotoState('AssociatedTouch');
		InitialState = GetStateName();
	}
}

simulated function string GetLocationStringFor(PlayerReplicationInfo PRI)
{
	return LocationName;
}

/** 
 * list important Volume variables on canvas.  HUD will call DisplayDebug() on the current ViewTarget when
 * the ShowDebug exec is used
 *
 * @param	HUD		- HUD with canvas to draw on
 * @input	out_YL		- Height of the current font
 * @input	out_YPos	- Y position on Canvas. out_YPos += out_YL, gives position to draw text for next debug line.
 */
function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	super.DisplayDebug(HUD, out_YL, out_YPos);

	HUD.Canvas.DrawText("AssociatedActor "$AssociatedActor, false);
	out_YPos += out_YL;
	HUD.Canvas.SetPos(4, out_YPos);
}	

State AssociatedTouch
{
	event Touch( Actor Other, vector HitLocation, vector HitNormal )
	{
		AssociatedActor.Touch(Other, HitLocation, HitNormal);
	}

	event untouch( Actor Other )
	{
		AssociatedActor.untouch(Other);
	}

	function BeginState()
	{
		local Actor A;

		ForEach TouchingActors(class'Actor', A)
			Touch(A, A.Location, Vect(0,0,1) );
	}
}

/**	Handling Toggle event from Kismet. */
simulated function OnToggle(SeqAct_Toggle Action)
{
	// Turn ON
	if (Action.InputLinks[0].bHasImpulse)
	{
		if(!bCollideActors)
		{
			SetCollision(true, bBlockActors);
		}
	}
	// Turn OFF
	else if (Action.InputLinks[1].bHasImpulse)
	{
		if(bCollideActors)
		{
			SetCollision(false, bBlockActors);
		}
	}
	// Toggle
	else if (Action.InputLinks[2].bHasImpulse)
	{
		SetCollision(!bCollideActors, bBlockActors);
	}
}

defaultproperties
{
	Begin Object Name=BrushComponent0
		CollideActors=true
		BlockActors=false
		BlockZeroExtent=false
		BlockNonZeroExtent=true
		BlockRigidBody=false
		AlwaysLoadOnClient=True
		AlwaysLoadOnServer=True
	End Object
	bCollideActors=True
	bSkipActorPropertyReplication=true
}