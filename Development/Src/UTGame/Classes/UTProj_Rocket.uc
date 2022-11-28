class UTProj_Rocket extends UTProjectile;

/** Used for the curling rocket effect */

var byte FlockIndex;
var UTProj_Rocket Flock[2];

var() float	FlockRadius;
var() float	FlockStiffness;
var() float FlockMaxForce;
var() float	FlockCurlForce;
var bool bCurl;
var vector Dir;

var PointLightComponent RocketGlow;

replication
{
    reliable if ( bNetInitial && (Role == ROLE_Authority) )
        FlockIndex, bCurl;
}

function Init(vector Direction)
{
	super.Init(Direction);
	Dir = Direction;
}


simulated function Destroyed()
{
	ClearTimer('FlockTimer');
	super.Destroyed();
}

simulated function PostNetBeginPlay()
{
	local UTProj_Rocket R;
	local int i;

	Super.PostNetBeginPlay();

	if ( FlockIndex != 0 )
	{
	    SetTimer(0.1, true, 'FlockTimer');

	    // look for other rockets
	    if ( Flock[1] == None )
	    {

			ForEach DynamicActors(class'UTProj_Rocket',R)
				if ( R.FlockIndex == FlockIndex )
				{
					Flock[i] = R;
					if ( R.Flock[0] == None )
						R.Flock[0] = self;
					else if ( R.Flock[0] != self )
						R.Flock[1] = self;
					i++;
					if ( i == 2 )
						break;
				}
		}
	}
}

simulated function FlockTimer()
{
    local vector ForceDir, CurlDir;
    local float ForceMag;
    local int i;

	Velocity =  Default.Speed * Normal(Dir * 0.5 * Default.Speed + Velocity);

	// Work out force between flock to add madness
	for(i=0; i<2; i++)
	{
		if(Flock[i] == None)
			continue;

		// Attract if distance between rockets is over 2*FlockRadius, repulse if below.
		ForceDir = Flock[i].Location - Location;
		ForceMag = FlockStiffness * ( (2 * FlockRadius) - VSize(ForceDir) );
		Acceleration = Normal(ForceDir) * Min(ForceMag, FlockMaxForce);

		// Vector 'curl'
		CurlDir = Flock[i].Velocity Cross ForceDir;
		if ( bCurl == Flock[i].bCurl )
			Acceleration += Normal(CurlDir) * FlockCurlForce;
		else
			Acceleration -= Normal(CurlDir) * FlockCurlForce;
	}
}


defaultproperties
{

	ProjFlightTemplate=ParticleSystem'RocketLauncher.FX.P_Weapons_RocketLauncher_FX_RocketTrail'
    ProjExplosionTemplate=ParticleSystem'RocketLauncher.FX.P_Weapons_RocketLauncher_FX_RocketExplosion'
	speed=1350.0
    MaxSpeed=1350.0
    Damage=90.0
    DamageRadius=220.0
    MomentumTransfer=50000
    MyDamageType=class'UTDmgType_Rocket'
    LifeSpan=8.0
    AmbientSound=SoundCue'RocketLauncher.Sounds.SC_RocketProj_Ambient'
    RotationRate=(Roll=50000)
    DesiredRotation=(Roll=30000)
    bCollideWorld=true

	// Add the Mesh

	Begin Object Class=StaticMeshComponent Name=WRocketMesh
		StaticMesh=StaticMesh'RocketLauncher.3rdPerson.S_RocketProj'
	End Object

	Begin Object class=PointLightComponent name=RocketLight
		Brightness=2.0
		Color=(R=255,G=150,B=40)
		Radius=180
		CastShadows=True
		bEnabled=true
	End Object

	Begin Object Class=TransformComponent Name=RocketTransform
    	TransformedComponent=RocketLight
    	Translation=(X=-200,Z=5)
    End Object

	Components.Add(WRocketMesh)
//	Components.Add(RocketTransform);
	Components.Remove(Sprite)

	// Flocking

    FlockRadius=12
    FlockStiffness=-40
    FlockMaxForce=600
    FlockCurlForce=450

    DrawScale=0.25


}
