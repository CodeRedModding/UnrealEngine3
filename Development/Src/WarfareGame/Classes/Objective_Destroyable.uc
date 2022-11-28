class Objective_Destroyable extends Objective_Game
	placeable;

var()	int		DamageCapacity;		// amount of damage that can be taken before destroyed
var		int		Health;
//var()   vector  AIShootOffset;	//adjust where AI should try to shoot this objective
//var ShootTarget ShootTarget;

var() const editconst StaticMeshComponent	StaticMeshComponent, DestroyedStaticMeshComponent;

simulated function PostBeginPlay()
{
	super.PostBeginPlay();

	if ( Role == Role_Authority )
		Reset();
	/* !!LD merge
	if ( AIShootOffset != vect(0,0,0) )
	{
		ShootTarget = Spawn(class'ShootTarget', Self,, Location + AIShootOffset);
		ShootTarget.SetBase( Self );
	}
	*/
	
}

/*
function Actor GetShootTarget()
{
	if ( ShootTarget != None )
		return ShootTarget;

	return Self;
}
*/

/* TellBotHowToDisable()
tell bot what to do to disable me.
return true if valid/useable instructions were given
*/
/* !!LD merge
function bool TellBotHowToDisable(Bot B)
{
	local int i;
	local float Best, Next;
	local vector Dir;
	local NavigationPoint BestPath;
	local bool bResult;

	// Only a specific Pawn can deal damage to objective ?
	if ( ConstraintInstigator == CI_PawnClass && !ClassIsChildOf(B.Pawn.Class, ConstraintPawnClass) )
		return false;

	if ( (B.Pawn.Physics == PHYS_Flying) && (B.Pawn.MinFlySpeed > 0) )
	{
		if ( (VehiclePath != None) && B.Pawn.ReachedDestination(VehiclePath) )
		{
			B.Pawn.AirSpeed = FMin(B.Pawn.AirSpeed, 1.05 * B.Pawn.MinFlySpeed);
			B.Pawn.bThumped = true;
			Dir = Normal(B.Pawn.Velocity);
			// go on to next pathnode past VehiclePath
			for ( i=0; i<VehiclePath.PathList.Length; i++ )
			{
				if ( BestPath == None )
				{
					BestPath = VehiclePath.PathList[i].End;
					Best = Dir Dot Normal(BestPath.Location - VehiclePath.Location);
				}
				else
				{
					Next = Dir Dot Normal(VehiclePath.PathList[i].End.Location - VehiclePath.Location);
					if ( Next > Best )
					{
						Best = Next;
						BestPath = VehiclePath.PathList[i].End;
					}
				}
			}
			if ( BestPath != None )
			{
				B.MoveTarget = BestPath;
				B.SetAttractionState();
				return true;
			}
		}
		if ( B.CanAttack(GetShootTarget()) )
		{
			B.Pawn.AirSpeed = FMin(B.Pawn.AirSpeed, 1.05 * B.Pawn.MinFlySpeed);
			B.Focus = self;
			B.FireWeaponAt(self);
			B.GoalString = "Attack Objective";
			if ( !B.Squad.FindPathToObjective(B,self) )
			{
				B.DoRangedAttackOn(GetShootTarget());
				B.Pawn.Acceleration = B.Pawn.AccelRate * Normal(Location - B.Pawn.Location);
			}
			else
				return true;
		}
		bResult = Super.TellBotHowToDisable(B);
		if ( bResult && (FlyingPathNode(B.MoveTarget) != None) && (B.MoveTarget.CollisionRadius < 1000) )
			B.Pawn.AirSpeed = FMin(B.Pawn.AirSpeed, 1.05 * B.Pawn.MinFlySpeed);
		else
			B.Pawn.AirSpeed = B.Pawn.Default.AirSpeed;
		return bResult;
	}
	else if ( !B.Pawn.bStationary && B.Pawn.TooCloseToAttack(GetShootTarget()) )
	{
		B.GoalString = "Back off from objective";
		B.RouteGoal = B.FindRandomDest();
		B.MoveTarget = B.RouteCache[0];
		B.SetAttractionState();
		return true;
	}
	else if ( B.CanAttack(GetShootTarget()) )
	{
		if ( KillEnemyFirst(B) )
			return false;

		B.GoalString = "Attack Objective";
		B.DoRangedAttackOn(GetShootTarget());
		return true;
	}

	return super.TellBotHowToDisable(B);
}

function bool KillEnemyFirst(Bot B)
{
	return false;
}

function bool NearObjective(Pawn P)
{
	if ( P.CanAttack(GetShootTarget()) )
		return true;
	return Super.NearObjective(P);
}
*/

/* Reset() - reset actor to initial state - used when restarting level without reloading. */
function Reset()
{
	Health				= DamageCapacity;
	bProjTarget			= true;
	//CollisionComponent	= StaticMeshComponent0;
	//Components[0]		= StaticMeshComponent0;
	super.Reset();
}

function TakeDamage( int Damage, Pawn instigatedBy, Vector hitlocation, 
						Vector momentum, class<DamageType> damageType, optional TraceHitInfo HitInfo)
{
	if ( bDisabled || !IsInstigatorRelevant(instigatedBy) )
		return;

	Health -= Damage;
	if ( Health < 1 )
		CompleteObjective( instigatedBy );
}

simulated function ObjectiveCompleted()
{
	super.ObjectiveCompleted();
	//CollisionComponent = DestroyedStaticMeshComponent0;
	//Components[0] = DestroyedStaticMeshComponent0;
}

/* returns objective's progress status 1->0 (=disabled) */
simulated function float GetObjectiveProgress()
{
	if ( bDisabled )
		return 0;
	return (float(Health) / float(DamageCapacity));
}

simulated function DrawObjectiveDebug( Canvas C )
{
	local string ObjectiveInfo;

	ObjectiveInfo = ObjectiveName @ "bDisabled:" @ bDisabled @ "Health:" @ Health @ "ObjectiveProgress:" @ GetObjectiveProgress();
	C.DrawText( ObjectiveInfo );
}

defaultproperties
{
	Begin Object Class=StaticMeshComponent Name=StaticMeshComponent0
		StaticMesh=StaticMesh'EditorMeshes.TexPropCylinder';
	End Object
	Components.Add(StaticMeshComponent0)
	Components.Remove(CollisionCylinder)
	StaticMeshComponent=StaticMeshComponent0
	CollisionComponent=StaticMeshComponent0
	
	Begin Object Class=StaticMeshComponent Name=DestroyedStaticMeshComponent0
	End Object
	DestroyedStaticMeshComponent=DestroyedStaticMeshComponent0

	ObjectiveName="DestroyableObjective"
	bDestinationOnly=true
	bNotBased=true
	DamageCapacity=100
	bCollideActors=true
	bBlockActors=true
	bProjTarget=true
	bSpecialForced=false	
	bStatic=false
	bNoDelete=true
	bHidden=false
	bEdShouldSnap=true
}