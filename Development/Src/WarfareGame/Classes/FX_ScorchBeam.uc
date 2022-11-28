
class FX_ScorchBeam extends Actor;

/** start location of effect (replicated) */
var vector	Start;

/** End location of effect (replicated) */
var vector	End;

/** color of temp effect */
var Color	LineColor;

/** looping ambient sound */
var()	AudioComponent	BeamSound;

replication
{
	reliable if ( Role == ROLE_Authority && !bNetOwner )
		Start, End;
}

simulated function Tick( float DeltaTime )
{
	local PlayerController	PC;
	local WarWeapon			Weap;
	local vector			HL, HN, AimDir;
	local TraceHitInfo		HitInfo;

	// Update Beam
	if ( Role == Role_Authority || Instigator.IsLocallyControlled() )
	{
		Weap	= WarWeapon(Instigator.Weapon);
		Weap.CalcWeaponFire( HL, HN , AimDir, HitInfo );

		Start	= Weap.GetPhysicalFireStartLoc();
		End		= HL;
	}

	if ( Level.NetMode != NM_DedicatedServer )
	{
		ForEach LocalPlayerControllers(PC)
		{
			if ( PC.MyHUD != None )
			{
				PC.MyHUD.Draw3DLine(Start, End, LineColor);
			}
		}
	}
}

defaultproperties
{
	LineColor=(R=255)
	bReplicateInstigator=true
	bAlwaysRelevant=true
	RemoteRole=ROLE_SimulatedProxy
	
	Begin Object Class=AudioComponent Name=AudioComponent0
		SoundCue=SoundCue'LD_TempUTSounds.BLinkGunLink'
		bAutoPlay=true
	End Object
	BeamSound=AudioComponent0
	Components.Add(AudioComponent0)
}