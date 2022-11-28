//=============================================================================
// Scout used for path generation.
//=============================================================================
class Scout extends Pawn
	native
	config(Game)
	notplaceable;

cpptext
{
	NO_DEFAULT_CONSTRUCTOR(AScout)
	INT findStart(FVector start);

	UBOOL HurtByVolume(AActor *A)
	{
		// scouts are never hurt by volumes
		return 0;
	}

	virtual void InitForPathing()
	{
		Physics = PHYS_Walking;
		JumpZ = TestJumpZ; 
		bCanWalk = 1;
		bJumpCapable = 1;
		bCanJump = 1;
		bCanSwim = 1;
		bCanClimbLadders = 1;
		bCanFly = 0;
		GroundSpeed = TestGroundSpeed;
		MaxFallSpeed = TestMaxFallSpeed;
	}

	FVector GetSize(FName desc)
	{
		for (INT idx = 0; idx < PathSizes.Num(); idx++)
		{
			if (PathSizes(idx).Desc == desc)
			{
				return FVector(PathSizes(idx).Radius,PathSizes(idx).Height,0.f);
			}
		}
		return FVector(PathSizes(0).Radius,PathSizes(0).Height,0.f);
	}
};

struct native PathSizeInfo
{
	var Name		Desc;
	var	float		Radius,
					Height,
					CrouchHeight;
};
var array<PathSizeInfo>			PathSizes;		// dimensions of reach specs to test for
var float						TestJumpZ,
								TestGroundSpeed,
								TestMaxFallSpeed,
								TestFallSpeed;

var const float MaxLandingVelocity;

simulated function PreBeginPlay()
{
	Destroy(); //scouts shouldn't exist during play
}

defaultproperties
{
	Begin Object NAME=CollisionCylinder
		CollisionRadius=+0034.000000
	End Object

	 RemoteRole=ROLE_None
	AccelRate=+00001.000000
	bCollideActors=false
	bCollideWorld=false
	bBlockActors=false
	bProjTarget=false
	bPathColliding=true

	PathSizes(0)=(Desc=Human,Radius=48,Height=80)
	PathSizes(1)=(Desc=Common,Radius=72,Height=100)
	PathSizes(2)=(Desc=Max,Radius=120,Height=120)

	TestJumpZ=420
	TestGroundSpeed=600
	TestMaxFallSpeed=2500
	TestFallSpeed=1200
}
