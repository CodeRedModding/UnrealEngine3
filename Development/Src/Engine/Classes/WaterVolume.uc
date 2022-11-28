class WaterVolume extends PhysicsVolume;

var() SoundCue  EntrySound;	//only if waterzone
var() SoundCue  ExitSound;		// only if waterzone
var() class<actor> EntryActor;	// e.g. a splash (only if water zone)
var() class<actor> ExitActor;	// e.g. a splash (only if water zone)
var() class<actor> PawnEntryActor; // when pawn center enters volume

var string EntrySoundName, ExitSoundName, EntryActorName, PawnEntryActorName;

function PostBeginPlay()
{
	Super.PostBeginPlay();

	if ( (EntrySound == None) && (EntrySoundName != "") )
		EntrySound = SoundCue(DynamicLoadObject(EntrySoundName,class'SoundCue'));
	if ( (ExitSound == None) && (ExitSoundName != "") )
		ExitSound = SoundCue(DynamicLoadObject(ExitSoundName,class'SoundCue'));
	if ( (EntryActor == None) && (EntryActorName != "") )
		EntryActor = class<Actor>(DynamicLoadObject(EntryActorName,class'Class'));	
	if ( (PawnEntryActor == None) && (PawnEntryActorName != "") )
		PawnEntryActor = class<Actor>(DynamicLoadObject(PawnEntryActorName,class'Class'));
}

simulated event Touch( Actor Other, vector HitLocation, vector HitNormal )
{
	Super.Touch(Other, HitLocation, HitNormal);

	if ( bWaterVolume && Other.CanSplash() )
		PlayEntrySplash(Other);
}

function PlayEntrySplash(Actor Other)
{
	local float SplashSize;
	local actor splash;

	splashSize = FClamp(0.00003 * Other.Mass * (250 - 0.5 * FMax(-600,Other.Velocity.Z)), 0.1, 1.0 );
	if( EntrySound != None )
	{
		//OldPlaySound(EntrySound, SLOT_Interact, splashSize);
		if ( Other.Instigator != None )
			MakeNoise(SplashSize);
	}
	if( EntryActor != None )
	{
		splash = Spawn(EntryActor);
		if ( splash != None )
			splash.SetDrawScale(splashSize);
	}
}

event untouch(Actor Other)
{
	if ( Other.CanSplash() )
		PlayExitSplash(Other);
}

function PlayExitSplash(Actor Other)
{
	local float SplashSize;
	local actor splash;

	splashSize = FClamp(0.003 * Other.Mass, 0.1, 1.0 );
	//if( ExitSound != None )
	//	OldPlaySound(ExitSound, SLOT_Interact, splashSize);
	if( ExitActor != None )
	{
		splash = Spawn(ExitActor);
		if ( splash != None )
			splash.SetDrawScale(splashSize);
	}
}

defaultproperties
{
	//EntrySoundName="PlayerSounds.FootstepWater1"
	//ExitSoundName="GeneralImpacts.ImpactSplash2"
	bWaterVolume=True
    FluidFriction=+00002.400000
	LocationName="under water"
	KExtraLinearDamping=0.8
	KExtraAngularDamping=0.1
}