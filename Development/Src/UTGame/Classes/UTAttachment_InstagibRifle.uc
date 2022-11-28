class UTAttachment_InstagibRifle extends UTWeaponAttachment;

var float EffectTimer;
var color LineColor;
var vector Start,End;

simulated event ImpactEffects()
{
	local Emitter	EmitActor;
	local vector Dir;
	local PlayerController PC;

	super.ImpactEffects();

	if ( UTPawn(Owner) != none )
	{

		Start = Owner.Location;
		End   = UTPawn(Owner).FlashLocation;
		Dir = Normal(End - Start);
		EmitActor = Spawn( class'Emitter',,, End - 10*Dir, rotator(-Dir) );
		EmitActor.SetTemplate( ParticleSystem'WeaponEffects.InstagibHitEffectReal', true );// OLD ParticleSystem'WeaponEffects.SparkOneShot' );

		foreach LocalPlayerControllers(PC)
		{
			if ( PC.MyHUD != None )
			{
				PC.MyHUD.Draw3DLine(Start, End, LineColor);
			}
		}
		EffectTimer=0.75;
		Enable('Tick');
	}

}

simulated function Tick(float DeltaTime)
{
	local PlayerController PC;

	if ( Level.Netmode == NM_DedicatedServer )
	{
		Disable('Tick');
		return;
	}

	ForEach LocalPlayerControllers(PC)
	{
		if ( PC.MyHUD != None )
		{
			LineColor.A = 255 * EffectTimer/0.75;
			PC.MyHUD.Draw3DLine(Start, End, LineColor);
		}
	}

	EffectTimer -= DeltaTime;

	if (EffectTimer <= 0.0)
		Disable('Tick');

}



defaultproperties
{
	// Weapon SkeletalMesh
	Begin Object Class=StaticMeshComponent Name=StaticMeshComponent0
		StaticMesh=StaticMesh'ShockRifle.Model.S_Weapons_ShockRifle_Model_3P'
		bOwnerNoSee=true
		bOnlyOwnerSee=false
		CollideActors=false
		AlwaysLoadOnClient=true
		AlwaysLoadOnServer=true
	End Object
    Mesh=StaticMeshComponent0

	// Weapon mesh Transform
	Begin Object Class=TransformComponent Name=TransformComponentMesh0
    	TransformedComponent=StaticMeshComponent0
		Translation=(X=-7.0,Y=1.0,Z=1.00)
		Rotation=(Pitch=1024,Roll=49152)
		Scale3D=(X=0.3,Y=0.3,Z=0.3)
		scale=1.2
    End Object
	MeshTransform=TransformComponentMesh0

	Begin Object class=PointLightComponent name=MuzzleFlashLightC
		Brightness=1.0
		Color=(R=255,G=255,B=128)
		Radius=255
		CastShadows=True
		bEnabled=false
	End Object
	MuzzleFlashLight=MuzzleFlashLightC

	// Muzzle Flashlight Positioning
	Begin Object Class=TransformComponent Name=TransformComponentMesh1
    	TransformedComponent=MuzzleFlashLightC
    	Translation=(X=-60,Z=5)
    End Object
	MuzzleFlashLightTransform=TransformComponentMesh1


	MuzzleFlashLightDuration=0.3
	MuzzleFlashLightBrightness=2.0
	LineColor=(R=255,G=0,B=255,A=255)

}
